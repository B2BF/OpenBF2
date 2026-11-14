// Ragdoll simulation, collision response and network state.

#include "RagDoll.h"
#include "RagDollTemplate.h"
#include "Skeleton.h"

#include <cfloat>       // FLT_EPSILON - the inlined Vec3::normalize guard
#include <cmath>        // sqrtf / fabsf
#include <cstdio>
#include <cstdlib>      // abort
#include <list>
#include <string>
#include <vector>

#include "../../Core/Ids.h"                        // CID_TriggerableTargetTemplate
#include "../../Game/Utility/Vars.h"               // the rd_* cvar handles
#include "../../Game/Objects/IObject.h"
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Game/Objects/ObjectLayoutAccess.h" // the raw Object offsets the ELF reads
#include "../../Scene/Object/ObjectManager.h"      // objectManager->getObjectsFromGrid
#include "../../Scene/Object/ObjectPredicator.h"   // ObjectFlagPredicator - the predicator's base
#include "../../Physics/ICollisionMesh.h"
#include "../../Physics/IPhysicsNode.h"
#include "../../Physics/IResponsePhysics.h"
#include "../../Physics/HeightmapCluster.h"        // IHeightmapCluster + the heightmapCluster global
#include "../../Physics/GeomUtil.h"                // capsuleVsCapsuleCollision
#include "../../Math/LocalBoundingBox.h"
#include "../../IO/BitStream.h"
#include "../../Core/IBase.h"
#include "../../Core/SmartItf.h"                 // classManager - the DebugRenderer singleton
#include "../../Core/ClassManager.h"

namespace dice {

// Terrain collision queries use the shared heightmap cluster.
namespace hfe { namespace world { extern IBase* heightmapCluster; } }


// Requires collision LOD 2, response physics without group 0x10, and all requested object flags.

class RagDollObjectPredicator : public hfe::world::ObjectFlagPredicator
{
public:
    explicit RagDollObjectPredicator(unsigned int flags) : hfe::world::ObjectFlagPredicator(flags) {}
    virtual bool predicate(hfe::world::IObject* object);
};

bool RagDollObjectPredicator::predicate(hfe::world::IObject* object)
{
    typedef hfe::world::object_detail::LayoutAccess L;

    if (!object->hasCollisionLod(2))
        return false;
    hfe::world::IResponsePhysics* const response =
        static_cast<hfe::world::IResponsePhysics*>(L::responsePhysics(object));      // +0x48
    if (!response)
        return false;
    if (response->getCollisionGroups() & 0x10u)
        return false;
    return hfe::world::ObjectFlagPredicator::predicate(object);
}

namespace anim {

namespace {

using hfe::Vars;
using hfe::Vec3;
using hfe::Mat4;


// Cache Vars handles and dereference them at use. The template shares the max-inherit-speed entry.

float* const rd_maxInheritSpeed      = Vars::getFloat("ragdoll-max-inherit-speed", 40.0f);
int*   const rd_netDebug             = Vars::getInt  ("ragdoll_net_debug", 0);
int*   const rd_netBoneEnable        = Vars::getInt  ("ragdoll_net_boneEnable", 15);
float* const rd_netInterpolate       = Vars::getFloat("ragdoll_net_interpolate", 0.1f);
float* const rd_gravityOverTime      = Vars::getFloat("ragdoll_gravityOverTime", 0.5f);
float* const rd_dT                   = Vars::getFloat("ragdoll_dT", 0.011111111f);
// 1/90 second.
float* const rd_slowMotion           = Vars::getFloat("ragdoll_slowMotion", 1.0f);
int*   const rd_enableForce          = Vars::getInt  ("ragdoll_enableForce", 1);
int*   const rd_iterations           = Vars::getInt  ("ragdoll_iterations", 2);
float* const rd_clientMaxUpdateTime  = Vars::getFloat("ragdoll_clientMaxUpdateTime", 0.002f);
float* const rd_sleep                = Vars::getFloat("ragdoll_sleep", 2.5f);
float* const rd_sleepDelta           = Vars::getFloat("ragdoll_sleepDelta", 0.02f);
int*   const rd_useObjectCollision   = Vars::getInt  ("ragdoll_useObjectCollision", 1);
int*   const rd_useCapsuleCollision  = Vars::getInt  ("ragdoll_useCapsuleCollision", 1);
float* const rd_completeCollRadius   = Vars::getFloat("ragdoll_completeCollRadius", 1.5f);
int*   const rd_useLandCollision     = Vars::getInt  ("ragdoll_useLandCollision", 1);
int*   const rd_useConstraints       = Vars::getInt  ("ragdoll_useConstraints", 1);
int*   const rd_useAngularConstraints= Vars::getInt  ("ragdoll_useAngularConstraints", 1);
float* const rd_expRotateForce       = Vars::getFloat("ragdoll_expRotateForce", 13.0f);
float* const rd_boneSize             = Vars::getFloat("ragdoll_boneSize", 0.1f);
float* const rd_kneeBoneSize         = Vars::getFloat("ragdoll_kneeBoneSize", 0.05f);
float* const rd_bodyBoneSize         = Vars::getFloat("ragdoll_bodyBoneSize", 0.12f);
float* const rd_collMid              = Vars::getFloat("ragdoll_collMid", 0.9999f);
float* const rd_legBackAngle         = Vars::getFloat("ragdoll_legBackAngle", -0.1f);
float* const rd_legForwardAngle      = Vars::getFloat("ragdoll_legForwardAngle", 10.0f);
float* const rd_legForwardFactor     = Vars::getFloat("ragdoll_legForwardFactor", 0.0f);
float* const rd_legPointForwardAngle = Vars::getFloat("ragdoll_legPointForwardAngle", 0.5f);
float* const rd_legPointForwardFactor= Vars::getFloat("ragdoll_legPointForwardFactor", 0.01f);
// Leaves unit vectors unchanged, normalizes others, and zeroes vectors with squared length below FLT_EPSILON.
inline void normalizeShipped(Vec3& v)
{
    const float len2 = v.z * v.z + v.y * v.y + v.x * v.x;
    if (fabsf(len2 - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(len2) >= FLT_EPSILON)
        {
            const float inv = 1.0f / sqrtf(len2);
            v.x *= inv;
            v.y *= inv;
            v.z *= inv;
        }
        else
        {
            v.x = 0.0f;
            v.y = 0.0f;
            v.z = 0.0f;
        }
    }
}

inline void setVec(Vec3& v, float x, float y, float z) { v.x = x; v.y = y; v.z = z; }

// The kinematic radius used by the two Particle constructors and RagDollTemplate::addParticle.
inline float defaultBoneSize() { return *rd_boneSize; }

}
std::vector<hfe::world::IObject*> RagDoll::m_objects;


// [CHOSEN, not recovered] Zeroes collision/padding bytes for deterministic copies.
// Collision checking resets m_collided before reading it.

RagDoll::Particle::Particle()
{
    setVec(m_position, 0.0f, 0.0f, 0.0f);
    setVec(m_oldPosition, 0.0f, 0.0f, 0.0f);
    setVec(m_acceleration, 0.0f, 0.0f, 0.0f);
    setVec(m_lastMove, 0.0f, 0.0f, 0.0f);
    setVec(m_netPosition, 0.0f, 0.0f, 0.0f);
    setVec(m_force, 0.0f, 0.0f, 0.0f);
    m_bone = -1;
    m_mass = 1.0f;
    m_size = defaultBoneSize();
    m_netControlled = false;
    m_collided = false;
    m_byte56 = false;
    m_byte57 = false;
}

RagDoll::Particle::Particle(const Vec3& position, int bone, float mass, float size)
{
    m_position    = position;
    m_oldPosition = position;
    setVec(m_acceleration, 0.0f, 0.0f, 0.0f);
    setVec(m_lastMove, 0.0f, 0.0f, 0.0f);
    setVec(m_netPosition, 0.0f, 0.0f, 0.0f);
    setVec(m_force, 0.0f, 0.0f, 0.0f);
    m_bone = bone;
    m_mass = mass;
    m_size = size;
    m_netControlled = false;
    m_collided = false;
    m_byte56 = false;
    m_byte57 = false;
}


// Starts collision debounce at twice its threshold so the first impact can fire immediately, then wakes.
// [CHOSEN, not recovered] Uninitialized skeleton, collision, and padding fields are zeroed.

RagDoll::RagDoll(RagDollTemplate& templ)
{
    m_byte00 = false;
    m_template = &templ;
    m_skeleton = 0;
    m_forceApplied = false;
    m_awake = true;
    m_byte1A = false;
    m_byte1B = false;
    setVec(m_position, 0.0f, 0.0f, 0.0f);
    setVec(m_vec28, 0.0f, 0.0f, 0.0f);
    m_sleepTimer = 0.0f;
    m_particleCollision.m_strength = 0.0f;
    m_particleCollision.m_material = 0;
    m_hasCollision = false;
    m_byte41 = false;
    m_byte42 = false;
    m_byte43 = false;
    m_collisionTimer = 2.0f * templ.m_particleCollisionCriteria2;
    m_stepAccumulator = 0.0f;
    m_gravityOverTime = *rd_gravityOverTime;
    m_gravityTime = 0.0f;
    m_updateMode = 0;
    m_clientTimer = 0.0f;
    m_exited = false;
    m_isClient = false;
    m_firstNetState = true;
    m_settled = false;
    wakeUp();
}

// (D1, 104 B): unregister from the template, then the particle vector frees itself.
RagDoll::~RagDoll()
{
    m_template->removeInstance(this);
}

void RagDoll::wakeUp()
{
    m_sleepTimer = *rd_sleep;
    if (!m_awake)
        m_awake = true;
}


// Applies force once. A negative bone distributes front-hit forces across the body.
// Preserve per-particle reads of rd_expRotateForce.

void RagDoll::addForce(int bone, const Vec3& force)
{
    if (!*rd_enableForce)
        return;
    if (m_forceApplied)
        return;
    m_forceApplied = true;
    wakeUp();

    if (bone < 0)
    {
        Vec3 forward = getForward();
        const float scale = *rd_expRotateForce;
        const float bx = forward.x * scale;
        const float by = forward.y * scale;
        const float bz = forward.z * scale;
        for (size_t i = 0; i < m_particles.size(); ++i)
        {
            Particle& p = m_particles[i];
            const int b = p.m_bone;
            if (b == 4 || b == 9 || b == 2 || b == 7 || b == 1 || b == 6)
            {
                p.m_force.x -= bx;
                p.m_force.y -= by;
                p.m_force.z -= bz;
            }
            else
            {
                const float s = *rd_expRotateForce;
                p.m_force.x += force.x * s;
                p.m_force.y += force.y * s;
                p.m_force.z += force.z * s;
            }
        }
        return;
    }

    const int index = m_template->findParticleIndex(bone);
    if (index >= 0)
    {
        Particle& p = m_particles[index];
        p.m_force.x += force.x;
        p.m_force.y += force.y;
        p.m_force.z += force.z;
    }
}

// Returns a function-local static (guarded in the ELF) - one shared slot.
const Vec3& RagDoll::getParticleSpeed(int bone)
{
    if (bone < 0)
        return Vec3::getZero();
    const int index = m_template->findParticleIndex(bone);
    if (index < 0)
        return Vec3::getZero();

    static Vec3 s_speed;
    const Particle& p = m_particles[index];
    s_speed.x = p.m_position.x - p.m_oldPosition.x;
    s_speed.y = p.m_position.y - p.m_oldPosition.y;
    s_speed.z = p.m_position.z - p.m_oldPosition.z;
    return s_speed;
}

// Hands the pending collision out once (clears the flag and restarts the debounce).
const RagDoll::ParticleCollision* RagDoll::getParticleCollision()
{
    if (m_template->m_particleCollisionCriteria1 < 0.0f)
        return 0;
    if (!m_hasCollision)
        return 0;
    m_hasCollision = false;
    m_collisionTimer = 0.0f;
    return &m_particleCollision;
}


// Integrates particles with the averaged Verlet displacement; client-controlled particles interpolate network positions.
// Clears acceleration and accumulated force afterward.

void RagDoll::verlet(float dt)
{
    for (size_t i = 0; i < m_particles.size(); ++i)
    {
        Particle& p = m_particles[i];
        if (m_isClient && p.m_netControlled)
        {
            const Vec3 cur = p.m_position;
            const float k = *rd_netInterpolate;
            p.m_position.x = (p.m_netPosition.x - cur.x) * k + cur.x;
            p.m_position.y = (p.m_netPosition.y - cur.y) * k + cur.y;
            p.m_position.z = (p.m_netPosition.z - cur.z) * k + cur.z;
            p.m_oldPosition = cur;
        }
        else
        {
            const Vec3 cur = p.m_position;
            Vec3 d;
            d.x = cur.x - p.m_oldPosition.x + p.m_acceleration.x * dt * dt;
            d.y = cur.y - p.m_oldPosition.y + p.m_acceleration.y * dt * dt;
            d.z = cur.z - p.m_oldPosition.z + p.m_acceleration.z * dt * dt;
            p.m_position.x = cur.x + (d.x + p.m_lastMove.x) * 0.5f;
            p.m_position.y = cur.y + (d.y + p.m_lastMove.y) * 0.5f;
            p.m_position.z = cur.z + (d.z + p.m_lastMove.z) * 0.5f;
            p.m_oldPosition = cur;
            p.m_lastMove = d;
        }
        setVec(p.m_acceleration, 0.0f, 0.0f, 0.0f);
        setVec(p.m_force, 0.0f, 0.0f, 0.0f);
    }
}

// acc += force / mass, plus gravity ramped by m_gravityTime / m_gravityOverTime
// (a full 1.0 when the ramp length is not positive).
void RagDoll::accumulateForces()
{
    float gravityFraction = 1.0f;
    if (m_gravityOverTime > 0.0f)
        gravityFraction = m_gravityTime / m_gravityOverTime;

    for (size_t i = 0; i < m_particles.size(); ++i)
    {
        Particle& p = m_particles[i];
        const float invMass = 1.0f / p.m_mass;
        p.m_acceleration.x += p.m_force.x * invMass;
        p.m_acceleration.y += p.m_force.y * invMass;
        p.m_acceleration.z += p.m_force.z * invMass;
        p.m_acceleration.y += m_template->m_gravity * gravityFraction;
    }
}

// The impact strength is the particle's last displacement projected against the
// surface normal. It is recorded only past the template's two criteria (debounce time, minimum
// strength), and only if stronger than what is already pending.
void RagDoll::addHitImpact(Particle& particle, const Vec3& normal, int material)
{
    const RagDollTemplate* const t = m_template;
    const float impact = -(normal.z * particle.m_lastMove.z
                         + normal.y * particle.m_lastMove.y
                         + normal.x * particle.m_lastMove.x);
    if (m_collisionTimer >= t->m_particleCollisionCriteria2 && impact >= t->m_particleCollisionCriteria1)
    {
        if (!m_hasCollision)
        {
            m_hasCollision = true;
            m_particleCollision.m_strength = 0.0f;
        }
        if (impact > m_particleCollision.m_strength)
        {
            m_particleCollision.m_strength = impact;
            m_particleCollision.m_material = material;
        }
    }
}


// Separates intersecting thigh capsules by a quarter of the penetration per leg.

void RagDoll::checkCapsuleConstraints()
{
    RagDollTemplate* const t = m_template;
    Particle& a = m_particles[t->findParticleIndex(2)];
    Particle& b = m_particles[t->findParticleIndex(4)];
    Particle& c = m_particles[t->findParticleIndex(7)];
    Particle& d = m_particles[t->findParticleIndex(9)];

    Vec3 contact;
    setVec(contact, 0.0f, 0.0f, 0.0f);
    Vec3 normal;
    setVec(normal, 0.0f, 0.0f, 0.0f);
    float depth = 0.0f;

    if (hfe::world::capsuleVsCapsuleCollision(a.m_position, b.m_position, c.m_position, d.m_position,
                                              *rd_boneSize, contact, normal, depth))
    {
        const float penetration = -depth;
        const float px = normal.x * 0.5f * penetration * 0.5f;
        const float py = normal.y * 0.5f * penetration * 0.5f;
        const float pz = 0.5f * (penetration * (normal.z * 0.5f));
        a.m_position.x += px; a.m_position.y += py; a.m_position.z += pz;
        b.m_position.x += px; b.m_position.y += py; b.m_position.z += pz;
        c.m_position.x -= px; c.m_position.y -= py; c.m_position.z -= pz;
        d.m_position.x -= px; d.m_position.y -= py; d.m_position.z -= pz;
    }
}


// Collects nearby collision-LOD-2 objects around the hip; rejected bounds leave null entries.
// Requires hip bone 6. Sphere conversion uses the inverse rotation only.

void RagDoll::getCollidingObjects()
{
    typedef hfe::world::object_detail::LayoutAccess L;

    m_objects.clear();

    const int centreIndex = m_template->findParticleIndex(6);
    if (centreIndex == -1)
        std::abort();
    const Vec3& centre = m_particles[centreIndex].m_position;

    RagDollObjectPredicator predicator(0x100u);
    hfe::world::objectManager->getObjectsFromGrid(centre, *rd_completeCollRadius, m_objects, &predicator);

    for (size_t i = 0; i < m_objects.size(); ++i)
    {
        hfe::world::IObject* const object = m_objects[i];
        if (!object->hasCollisionLod(2))
        {
            m_objects[i] = 0;
            continue;
        }
        hfe::world::ICollisionMesh* const mesh =
            static_cast<hfe::world::ICollisionMesh*>(L::collisionMesh(object));       // +0x40
        const hfe::LocalBoundingBox* const box =
            static_cast<const hfe::LocalBoundingBox*>(mesh->getBoundingBox(2));
        const Mat4& abs = object->getAbsoluteTransformation();
        const Mat4& inv = object->getInvAbsoluteTransformation();
        const float dy = centre.y - abs.m[13];
        const float dx = centre.x - abs.m[12];
        const float dz = centre.z - abs.m[14];
        Vec3 local;
        local.x = inv.m[4] * dy + inv.m[0] * dx + inv.m[8] * dz;
        local.y = inv.m[5] * dy + inv.m[1] * dx + inv.m[9] * dz;
        local.z = dy * inv.m[6] + dx * inv.m[2] + dz * inv.m[10];
        if (!box->intersectSphere(local, *rd_completeCollRadius))
            m_objects[i] = 0;
    }
}

// static: empties the shared list.
void RagDoll::restoreCollidingObjects()
{
    m_objects.clear();
}


// Pushes both capsule ends out of intersecting LOD-2 meshes and updates history to avoid adding velocity.
// Reuses three shared scratch vectors; not reentrant.

void RagDoll::checkCollisionCapsulesAgainstObjects()
{
    typedef hfe::world::object_detail::LayoutAccess L;

    if (!*rd_useCapsuleCollision)
        return;

    static std::vector<Vec3>  intersectNormal;
    static std::vector<Vec3>  intersectionPoint;
    static std::vector<float> vecPassedByProjectedOnNormal;
    intersectNormal.clear();
    intersectionPoint.clear();
    vecPassedByProjectedOnNormal.clear();

    for (size_t o = 0; o < m_objects.size(); ++o)
    {
        hfe::world::IObject* const object = m_objects[o];
        if (!object)
            continue;
        if (!object->hasCollisionLod(2))
            continue;
        hfe::world::ICollisionMesh* const mesh =
            static_cast<hfe::world::ICollisionMesh*>(L::collisionMesh(object));

        std::list<RagDollTemplate::Constraint>& capsules = m_template->m_capsuleConstraints;
        for (std::list<RagDollTemplate::Constraint>::iterator it = capsules.begin(); it != capsules.end(); ++it)
        {
            Particle& a = m_particles[it->m_a];
            Particle& b = m_particles[it->m_b];
            Vec3 axis;
            axis.x = b.m_position.x - a.m_position.x;
            axis.y = b.m_position.y - a.m_position.y;
            axis.z = b.m_position.z - a.m_position.z;

            intersectNormal.clear();
            intersectionPoint.clear();
            vecPassedByProjectedOnNormal.clear();

            const Mat4& inv = object->getInvAbsoluteTransformation();
            const Mat4& abs = object->getAbsoluteTransformation();
            mesh->getDistanceToCapsules(2, abs, inv, a.m_position, axis, a.m_size,
                                        intersectNormal, intersectionPoint, vecPassedByProjectedOnNormal);

            for (size_t k = 0; k < intersectNormal.size(); ++k)
            {
                const float depth = vecPassedByProjectedOnNormal[k];
                if (depth < 0.0f)
                {
                    const Vec3& n = intersectNormal[k];
                    a.m_position.x += n.x * -depth;
                    a.m_position.y += n.y * -depth;
                    a.m_position.z += n.z * -depth;
                    a.m_oldPosition = a.m_position;
                    b.m_position.x += n.x * -depth;
                    b.m_position.y += n.y * -depth;
                    b.m_position.z += n.z * -depth;
                    b.m_oldPosition = b.m_position;
                }
            }
        }
    }
}


// Constrains particle motion against world geometry using swept collision queries.

void RagDoll::checkCollisionConstraints()
{
    typedef hfe::world::object_detail::LayoutAccess L;

    if (m_updateMode <= 1)
        checkCapsuleConstraints();

    if (*rd_useObjectCollision)
    {
        for (size_t i = 0; i < m_particles.size(); ++i)
        {
            Particle& p = m_particles[i];
            p.m_collided = false;
            if (m_updateMode > 1)
            {
                const int b = p.m_bone;
                if (b == 2 || b == 7 || b == 16 || b == 32 || b == 47)
                    continue;
            }
            if (m_isClient && p.m_netControlled)
                continue;

            const float radius = p.m_size;
            for (size_t o = 0; o < m_objects.size(); ++o)
            {
                hfe::world::IObject* const object = m_objects[o];
                if (!object)
                    continue;
                if (object->hasCollisionLod(2) != true)
                    continue;
                hfe::world::ICollisionMesh* const mesh =
                    static_cast<hfe::world::ICollisionMesh*>(L::collisionMesh(object));

                Vec3 start = p.m_oldPosition;
                const Vec3 end = p.m_position;
                Vec3 dir;
                dir.x = end.x - start.x;
                dir.y = end.y - start.y;
                dir.z = end.z - start.z;
                if (dir.x * dir.x + dir.y * dir.y + dir.z * dir.z <= FLT_EPSILON)
                {
                    start.x = end.x;
                    start.y = radius * 0.5f + end.y;
                    start.z = end.z;
                    dir.x = end.x - start.x;
                    dir.y = end.y - start.y;
                    dir.z = end.z - start.z;
                }
                Vec3 n = dir;
                normalizeShipped(n);
                Vec3 sweep;                              // the motion, extended by one radius
                sweep.x = dir.x + radius * n.x;
                sweep.y = dir.y + radius * n.y;
                sweep.z = dir.z + radius * n.z;
                Vec3 rayEnd;
                rayEnd.x = end.x + radius * n.x;
                rayEnd.y = radius * n.y + end.y;
                rayEnd.z = end.z + radius * n.z;

                Vec3 normal;
                Vec3 point;
                float planeDist = 0.0f;
                float along = 0.0f;
                int material = 0;
                const Mat4& abs = object->getAbsoluteTransformation();
                const Mat4& inv = object->getInvAbsoluteTransformation();
                const bool hit = mesh->getDistance(2, false, abs, inv, start, sweep, normal, point,
                                                   planeDist, along, &material, 0, true, true);
                if (hit && 0.0f > normal.y * n.y + normal.x * n.x + normal.z * n.z)
                {
                    p.m_collided = true;
                    Vec3 hitPos;
                    hitPos.x = rayEnd.x - normal.x * along;
                    hitPos.y = rayEnd.y - normal.y * along;
                    hitPos.z = rayEnd.z - normal.z * along;
                    p.m_position.x = hitPos.x - radius * n.x;
                    p.m_position.y = hitPos.y - radius * n.y;
                    p.m_position.z = hitPos.z - radius * n.z;
                    p.m_oldPosition = p.m_position;
                    m_settled = true;
                    addHitImpact(p, normal, material);
                    break;                                          // next particle
                }
                if (!m_awake)
                    wakeUp();
            }
        }
    }

    if (*rd_useLandCollision)
    {
        hfe::world::IHeightmapCluster* const cluster =
            reinterpret_cast<hfe::world::IHeightmapCluster*>(hfe::world::heightmapCluster);
        for (size_t i = 0; i < m_particles.size(); ++i)
        {
            Particle& p = m_particles[i];
            if (p.m_collided)
            {
                p.m_collided = false;
                continue;
            }
            if (m_isClient && p.m_netControlled)
                continue;

            const float radius = p.m_size;
            Vec3 start = p.m_oldPosition;
            const Vec3 end = p.m_position;
            Vec3 dir;
            dir.x = end.x - start.x;
            dir.y = end.y - start.y;
            dir.z = end.z - start.z;
            if (dir.x * dir.x + dir.y * dir.y + dir.z * dir.z <= FLT_EPSILON)
            {
                start.x = end.x;
                start.y = radius * 0.5f + end.y;
                start.z = end.z;
                dir.x = end.x - start.x;
                dir.y = end.y - start.y;
                dir.z = end.z - start.z;
            }
            Vec3 n = dir;
            normalizeShipped(n);
            Vec3 rayEnd;
            rayEnd.x = end.x + radius * n.x;
            rayEnd.y = radius * n.y + end.y;
            rayEnd.z = end.z + radius * n.z;

            Vec3 hitPoint;
            float fraction = 0.0f;
            Vec3 normal;
            unsigned char material = 0;
            const bool hit = cluster->intersectRayInWorldCoords(start, rayEnd, hitPoint, fraction,
                                                                normal, material, true);
            if (hit && material != 1)
            {
                if (0.0f > n.y * normal.y + n.x * normal.x + n.z * normal.z)
                {
                    Vec3 hitPos;
                    hitPos.x = rayEnd.x - normal.x * fraction;
                    hitPos.y = rayEnd.y - normal.y * fraction;
                    hitPos.z = rayEnd.z - normal.z * fraction;
                    p.m_position.x = hitPos.x - radius * n.x;
                    p.m_position.y = hitPos.y - radius * n.y;
                    p.m_position.z = hitPos.z - radius * n.z;
                    p.m_oldPosition = p.m_position;
                    m_settled = true;
                    addHitImpact(p, normal, material);
                    continue;
                }
            }

            const float waterLevel = cluster->getWaterLevelInWorldCoords(p.m_position);
            if (waterLevel <= p.m_position.y)
                continue;
            // Under water: drift toward (x, waterLevel, z) by one percent, velocity zeroed.
            p.m_position.y += (waterLevel - p.m_position.y) * 0.01f;
            p.m_oldPosition = p.m_position;
        }
    }

    if (m_updateMode <= 1)
        checkCollisionCapsulesAgainstObjects();
}


// Applies mass-weighted distance constraints; client-controlled particles retain network positions.

void RagDoll::checkDistanceConstraints()
{
    std::list<RagDollTemplate::Constraint>& constraints = m_template->m_distanceConstraints;
    for (std::list<RagDollTemplate::Constraint>::iterator it = constraints.begin(); it != constraints.end(); ++it)
    {
        Particle& a = m_particles[it->m_a];
        Particle& b = m_particles[it->m_b];
        if (m_isClient && a.m_netControlled && b.m_netControlled)
            continue;

        const float dx = b.m_position.x - a.m_position.x;
        const float dy = b.m_position.y - a.m_position.y;
        const float dz = b.m_position.z - a.m_position.z;
        const float len = sqrtf(dy * dy + dx * dx + dz * dz);
        if (len > 0.0001f)
        {
            const float invA = 1.0f / a.m_mass;
            const float invB = 1.0f / b.m_mass;
            const float f = (len - it->m_length) / (len * (invA + invB));
            if (!(m_isClient && a.m_netControlled))
            {
                a.m_position.x += dx * f * invA;
                a.m_position.y += dy * f * invA;
                a.m_position.z += dz * f * invA;
            }
            if (!(m_isClient && b.m_netControlled))
            {
                b.m_position.x -= dx * f * invB;
                b.m_position.y -= dy * f * invB;
                b.m_position.z -= dz * f * invB;
            }
        }
    }
}

// The same projection, applied only while the pair is closer than its limit -
// it keeps two particles apart (ragDollConstraints.inc calls them "no less than" constraints).
void RagDoll::checkDistanceLessThanConstraints()
{
    std::list<RagDollTemplate::DistanceLessThanConstraint>& constraints = m_template->m_distanceLessThanConstraints;
    for (std::list<RagDollTemplate::DistanceLessThanConstraint>::iterator it = constraints.begin();
         it != constraints.end(); ++it)
    {
        Particle& a = m_particles[it->m_a];
        Particle& b = m_particles[it->m_b];
        if (m_isClient && a.m_netControlled && b.m_netControlled)
            continue;

        const float dx = b.m_position.x - a.m_position.x;
        const float dy = b.m_position.y - a.m_position.y;
        const float dz = b.m_position.z - a.m_position.z;
        const float len = sqrtf(dy * dy + dx * dx + dz * dz);
        if (len > 0.0001f && it->m_minDistance > len)
        {
            const float invA = 1.0f / a.m_mass;
            const float invB = 1.0f / b.m_mass;
            const float f = (len - it->m_minDistance) / (len * (invA + invB));
            if (!(m_isClient && a.m_netControlled))
            {
                a.m_position.x += dx * f * invA;
                a.m_position.y += dy * f * invA;
                a.m_position.z += dz * f * invA;
            }
            if (!(m_isClient && b.m_netControlled))
            {
                b.m_position.x -= dx * f * invB;
                b.m_position.y -= dy * f * invB;
                b.m_position.z -= dz * f * invB;
            }
        }
    }
}


// Constraint debug drawing: each segment is the renderer's debug cylinder (CID 81016) stretched
// between its two points, 0.002 thick, coloured through the mesh's IID 80007 interface.
namespace {

// "DebugRenderer" (geom::IDebugRenderer, GeomMisc/DebugRenderer.h): the slots used here, in table
// order. Declared locally, as AI's DebugRenderer.cpp does for its writer.
class IDebugGeometryQueue : public hfe::IBase
{
public:
    virtual void flush() = 0;                                                           // +0x10
    virtual void queueGeometry(unsigned int classId, const hfe::Mat4& transform,
                               const std::string& technique) = 0;                       // +0x14
    virtual hfe::IBase* acquireGeometry(unsigned int classId) = 0;                           // +0x18

protected:
    ~IDebugGeometryQueue() {}
};

// The debug mesh's table up to its colour setter: entry 62 (+0xF8) of the DebugCylinderMesh table
// (RendDX9 PE 0x1020BD00, GeomMisc/DebugCylinderMesh.cpp). The entries before it are not called.
class IDebugMeshColour : public hfe::IBase
{
public:
    virtual void unused10() = 0;
    virtual void unused14() = 0;
    virtual void unused18() = 0;
    virtual void unused1C() = 0;
    virtual void unused20() = 0;
    virtual void unused24() = 0;
    virtual void unused28() = 0;
    virtual void unused2C() = 0;
    virtual void unused30() = 0;
    virtual void unused34() = 0;
    virtual void unused38() = 0;
    virtual void unused3C() = 0;
    virtual void unused40() = 0;
    virtual void unused44() = 0;
    virtual void unused48() = 0;
    virtual void unused4C() = 0;
    virtual void unused50() = 0;
    virtual void unused54() = 0;
    virtual void unused58() = 0;
    virtual void unused5C() = 0;
    virtual void unused60() = 0;
    virtual void unused64() = 0;
    virtual void unused68() = 0;
    virtual void unused6C() = 0;
    virtual void unused70() = 0;
    virtual void unused74() = 0;
    virtual void unused78() = 0;
    virtual void unused7C() = 0;
    virtual void unused80() = 0;
    virtual void unused84() = 0;
    virtual void unused88() = 0;
    virtual void unused8C() = 0;
    virtual void unused90() = 0;
    virtual void unused94() = 0;
    virtual void unused98() = 0;
    virtual void unused9C() = 0;
    virtual void unusedA0() = 0;
    virtual void unusedA4() = 0;
    virtual void unusedA8() = 0;
    virtual void unusedAC() = 0;
    virtual void unusedB0() = 0;
    virtual void unusedB4() = 0;
    virtual void unusedB8() = 0;
    virtual void unusedBC() = 0;
    virtual void unusedC0() = 0;
    virtual void unusedC4() = 0;
    virtual void unusedC8() = 0;
    virtual void unusedCC() = 0;
    virtual void unusedD0() = 0;
    virtual void unusedD4() = 0;
    virtual void unusedD8() = 0;
    virtual void unusedDC() = 0;
    virtual void unusedE0() = 0;
    virtual void unusedE4() = 0;
    virtual void unusedE8() = 0;
    virtual void unusedEC() = 0;
    virtual void unusedF0() = 0;
    virtual void unusedF4() = 0;
    virtual void setColour(const hfe::Vec4& colour) = 0;                                // +0xF8

protected:
    ~IDebugMeshColour() {}
};

const unsigned int kDebugCylinderMesh  = 81016u;   // CID_DebugCylinderMesh (BF2.exe ds:0x94F660)
const unsigned int kIDebugCylinderMesh = 80007u;   // IID_IDebugCylinderMesh (BF2.exe ds:0x94F5D0)

IDebugGeometryQueue* debugRenderer()
{
    return static_cast<IDebugGeometryQueue*>(hfe::classManager->getSingleton(std::string("DebugRenderer")));
}

// The shared segment body (BF2.exe 0x7E71B0..0x7E7450, ELF 0x8343F40): scale(0.002, 0.002, |d|)
// times the orthonormal basis built from d, translated to the midpoint, queued and coloured. The
// renderer and the acquired geometry are used unchecked; only the colour interface is tested.
void queueSegment(IDebugGeometryQueue* renderer, const hfe::Vec3& from, const hfe::Vec3& to,
                  const hfe::Vec4& colour)
{
    hfe::Vec3 d;
    d.x = to.x - from.x;
    d.y = to.y - from.y;
    d.z = to.z - from.z;

    hfe::Mat4 transform = hfe::Mat4::getIdentity();
    hfe::Vec3 thickness;
    thickness.x = 0.002f;
    thickness.y = 0.002f;
    thickness.z = std::sqrt(d.z * d.z + d.y * d.y + d.x * d.x);
    transform.scale(thickness);

    // Rows: x (1,0,0), y (0,1,0), z = d; y.makeOrthonormalBasis(z, x), as the call is made.
    hfe::Vec3 right; right.x = 1.0f; right.y = 0.0f; right.z = 0.0f;
    hfe::Vec3 up;    up.x    = 0.0f; up.y    = 1.0f; up.z    = 0.0f;
    hfe::Vec3 forward = d;
    up.makeOrthonormalBasis(forward, right);
    hfe::Mat4 basis = hfe::Mat4::getIdentity();
    basis.m[0]  = right.x;   basis.m[1]  = right.y;   basis.m[2]  = right.z;
    basis.m[4]  = up.x;      basis.m[5]  = up.y;      basis.m[6]  = up.z;
    basis.m[8]  = forward.x; basis.m[9]  = forward.y; basis.m[10] = forward.z;
    basis.m[12] = d.x * 0.5f + from.x;
    basis.m[13] = d.y * 0.5f + from.y;
    basis.m[14] = d.z * 0.5f + from.z;

    const hfe::Mat4 scaled = transform;
    transform.mult(scaled, basis);

    renderer->queueGeometry(kDebugCylinderMesh, transform, std::string(""));
    hfe::IBase* geometry = renderer->acquireGeometry(kDebugCylinderMesh);
    IDebugMeshColour* mesh = static_cast<IDebugMeshColour*>(geometry->queryInterface(kIDebugCylinderMesh));
    if (mesh)
        mesh->setColour(colour);
}

} // namespace

// ELF 0x8343F40. No caller in the ELF, and BF2.exe has no copy (the linker dropped it); kept as the
// ELF has it.
void RagDoll::drawDebugLine(const Vec3& from, const Vec3& to, const hfe::Vec4& colour)
{
    queueSegment(debugRenderer(), from, to, colour);
}

// BF2.exe 0x7E7160 (ELF 0x83458D0): every distance constraint as a red segment between its two
// particles. The renderer is looked up once, before the loop.
void RagDoll::drawConstraints()
{
    IDebugGeometryQueue* renderer = debugRenderer();
    hfe::Vec4 red;
    red.x = 1.0f; red.y = 0.0f; red.z = 0.0f; red.w = 1.0f;
    for (std::list<RagDollTemplate::Constraint>::iterator it = m_template->m_distanceConstraints.begin();
         it != m_template->m_distanceConstraints.end(); ++it)
        queueSegment(renderer, m_particles[it->m_a].m_position, m_particles[it->m_b].m_position, red);
}


// Enforces angular constraints by projecting endpoint distance, then restoring adjacent segment lengths.
// Positive targets are exact; negative targets are maxima. Original inverse-mass weighting is preserved.

void RagDoll::checkAngularConstraints()
{
    if (!*rd_useAngularConstraints)
        return;

    std::list<RagDollTemplate::AngleConstraint>& constraints = m_template->m_angularConstraints;
    for (std::list<RagDollTemplate::AngleConstraint>::iterator it = constraints.begin(); it != constraints.end(); ++it)
    {
        Particle& a = m_particles[it->m_a];
        Particle& c = m_particles[it->m_c];
        Particle& b = m_particles[it->m_b];

        const float dx = c.m_position.x - a.m_position.x;
        const float dy = c.m_position.y - a.m_position.y;
        const float dz = c.m_position.z - a.m_position.z;
        const float len = sqrtf(dz * dz + dy * dy + dx * dx);
        if (len <= 0.0001f)
            continue;

        float lo, hi, target;
        if (it->m_distAC >= 0.0f)
        {
            lo = it->m_distAC;
            target = lo;
            hi = 9999.0f;
        }
        else
        {
            lo = 0.0f;
            hi = -it->m_distAC;
            target = hi;
        }
        if (!(lo > len || len > hi))
            continue;

        const float invA = 1.0f / a.m_mass;
        const float invC = 1.0f / c.m_mass;
        const float sum = invA + invC;
        const float f = (len - target) / (len * sum);
        if (!(m_isClient && a.m_netControlled))
        {
            a.m_position.x += dx * f * invA;
            a.m_position.y += dy * f * invA;
            a.m_position.z += dz * f * invA;
        }
        if (!(m_isClient && c.m_netControlled))
        {
            c.m_position.x -= dx * f * invC;
            c.m_position.y -= dy * f * invC;
            c.m_position.z -= dz * f * invC;
        }

        // re-satisfy a-b
        const float ex = a.m_position.x - b.m_position.x;
        const float ey = a.m_position.y - b.m_position.y;
        const float ez = a.m_position.z - b.m_position.z;
        const float len2 = sqrtf(ez * ez + ey * ey + ex * ex);
        if (len2 > 0.0001f)
        {
            const float f2 = (len2 - it->m_distAB) / (len2 * sum);
            if (!(m_isClient && b.m_netControlled))
            {
                b.m_position.x += ex * f2 * invC;
                b.m_position.y += ey * f2 * invC;
                b.m_position.z += ez * f2 * invC;
            }
            if (!(m_isClient && a.m_netControlled))
            {
                a.m_position.x -= ex * f2 * invA;
                a.m_position.y -= ey * f2 * invA;
                a.m_position.z -= ez * f2 * invA;
            }

            // re-satisfy c-b
            const float gx = b.m_position.x - c.m_position.x;
            const float gy = b.m_position.y - c.m_position.y;
            const float gz = b.m_position.z - c.m_position.z;
            const float len3 = sqrtf(gz * gz + gy * gy + gx * gx);
            if (0.0001f < len3)
            {
                const float f3 = (len3 - it->m_distBC) / (len3 * (invC + invC));
                if (!(m_isClient && c.m_netControlled))
                {
                    c.m_position.x += gx * f3 * invC;
                    c.m_position.y += gy * f3 * invC;
                    c.m_position.z += gz * f3 * invC;
                }
                if (!(m_isClient && b.m_netControlled))
                {
                    b.m_position.x -= gx * f3 * invC;
                    b.m_position.y -= gy * f3 * invC;
                    b.m_position.z -= gz * f3 * invC;
                }
            }
        }
    }
}


// Clamps thigh swing and forward shin angle for both legs, applying corrections along each leg's side axis.

void RagDoll::checkLegAngularConstraints()
{
    static const int leg[2] = { 1, 6 };

    const Vec3 forward = getForward();
    const Vec3 right = getRight();

    for (int i = 0; i < 2; ++i)
    {
        const int bone = leg[i];
        Particle& a = m_particles[m_template->findParticleIndex(bone)];
        Particle& b = m_particles[m_template->findParticleIndex(bone + 1)];
        Particle& c = m_particles[m_template->findParticleIndex(bone + 3)];

        const float lx = b.m_position.x - a.m_position.x;
        const float ly = b.m_position.y - a.m_position.y;
        const float lz = b.m_position.z - a.m_position.z;

        Vec3 side;                                          // right x leg
        side.x = right.y * lz - right.z * ly;
        side.y = right.z * lx - lz * right.x;
        side.z = right.x * ly - right.y * lx;
        normalizeShipped(side);

        Vec3 legN;
        setVec(legN, lx, ly, lz);
        normalizeShipped(legN);

        const float d = forward.z * legN.z + forward.y * legN.y + forward.x * legN.x;
        const float factor = *rd_legForwardFactor;
        float correction = 0.0f;
        if (factor != 0.0f)
        {
            if (*rd_legBackAngle > d)
                correction = (*rd_legBackAngle - d) * factor;
            else if (d > *rd_legForwardAngle)
                correction = (*rd_legForwardAngle - d) * factor;
        }
        if (correction != 0.0f)
        {
            const float px = side.x * correction;
            const float py = side.y * correction;
            const float pz = side.z * correction;
            a.m_position.x -= px; a.m_position.y -= py; a.m_position.z -= pz;
            b.m_position.x += px; b.m_position.y += py; b.m_position.z += pz;
        }

        const float factor2 = *rd_legPointForwardFactor;
        if (factor2 != 0.0f)
        {
            Vec3 shin;
            shin.x = c.m_position.x - b.m_position.x;
            shin.y = c.m_position.y - b.m_position.y;
            shin.z = c.m_position.z - b.m_position.z;
            normalizeShipped(shin);
            const float d2 = -(shin.y * side.y + shin.x * side.x + shin.z * side.z);
            if (*rd_legPointForwardAngle > d2)
            {
                const float k = (*rd_legPointForwardAngle - d2) * factor2;
                const float px = side.x * k;
                const float py = side.y * k;
                const float pz = side.z * k;
                b.m_position.x += px; b.m_position.y += py; b.m_position.z += pz;
                c.m_position.x -= px; c.m_position.y -= py; c.m_position.z -= pz;
            }
        }
    }
}


// Separates endpoints until the dihedral sum reaches its target, then restores a-b, c-d, and b-c distances.
// Each correction remains nested in the preceding nondegenerate branch.

void RagDoll::checkDihedralAngleConstraints()
{
    if (!*rd_useAngularConstraints)
        return;

    std::list<RagDollTemplate::DihedralAngleConstraint>& constraints = m_template->m_dihedralAngleConstraints;
    for (std::list<RagDollTemplate::DihedralAngleConstraint>::iterator it = constraints.begin();
         it != constraints.end(); ++it)
    {
        Particle& a = m_particles[it->m_a];
        Particle& b = m_particles[it->m_b];
        Particle& c = m_particles[it->m_c];
        Particle& d = m_particles[it->m_d];

        const float wy = d.m_position.y - c.m_position.y + b.m_position.y - a.m_position.y;
        const float wx = d.m_position.x - c.m_position.x + b.m_position.x - a.m_position.x;
        const float wz = d.m_position.z - c.m_position.z + b.m_position.z - a.m_position.z;
        const float len = sqrtf(wz * wz + wy * wy + wx * wx);
        if (len <= 0.0001f)
            continue;
        const float target = it->m_targetAD;
        if (!(target > len))
            continue;

        const float invB = 1.0f / b.m_mass;
        const float invC = 1.0f / c.m_mass;
        const float invD = 1.0f / d.m_mass;
        const float invA = 1.0f / a.m_mass;
        const float f = (len - target) / (len * (invA + invD));
        if (!(m_isClient && a.m_netControlled))
        {
            a.m_position.x += wx * f * invA;
            a.m_position.y += wy * f * invA;
            a.m_position.z += wz * f * invA;
        }
        if (!(m_isClient && d.m_netControlled))
        {
            d.m_position.x -= wx * f * invD;
            d.m_position.y -= wy * f * invD;
            d.m_position.z -= wz * f * invD;
        }

        // re-satisfy a-b
        const float ex = a.m_position.x - b.m_position.x;
        const float ey = a.m_position.y - b.m_position.y;
        const float ez = a.m_position.z - b.m_position.z;
        const float len1 = sqrtf(ez * ez + ey * ey + ex * ex);
        if (len1 <= 0.0001f)
            continue;
        const float f1 = (len1 - it->m_distAB) / (len1 * (invB + invA));
        if (!(m_isClient && b.m_netControlled))
        {
            b.m_position.x += ex * f1 * invB;
            b.m_position.y += ey * f1 * invB;
            b.m_position.z += ez * f1 * invB;
        }
        if (!(m_isClient && a.m_netControlled))
        {
            a.m_position.x -= ex * f1 * invA;
            a.m_position.y -= ey * f1 * invA;
            a.m_position.z -= ez * f1 * invA;
        }

        // re-satisfy c-d
        const float gx = c.m_position.x - d.m_position.x;
        const float gy = c.m_position.y - d.m_position.y;
        const float gz = c.m_position.z - d.m_position.z;
        const float len2 = sqrtf(gz * gz + gy * gy + gx * gx);
        if (!(0.0001f < len2))
            continue;
        const float f2 = (len2 - it->m_distCD) / (len2 * (invC + invD));
        if (!(m_isClient && d.m_netControlled))
        {
            d.m_position.x += gx * f2 * invD;
            d.m_position.y += gy * f2 * invD;
            d.m_position.z += gz * f2 * invD;
        }
        if (!(m_isClient && c.m_netControlled))
        {
            c.m_position.x -= gx * f2 * invC;
            c.m_position.y -= gy * f2 * invC;
            c.m_position.z -= gz * f2 * invC;
        }

        // re-satisfy b-c
        const float hx = b.m_position.x - c.m_position.x;
        const float hy = b.m_position.y - c.m_position.y;
        const float hz = b.m_position.z - c.m_position.z;
        const float len3 = sqrtf(hz * hz + hy * hy + hx * hx);
        if (!(0.0001f < len3))
            continue;
        const float f3 = (len3 - it->m_distBC) / (len3 * (invB + invC));
        if (!(m_isClient && c.m_netControlled))
        {
            c.m_position.x += hx * f3 * invC;
            c.m_position.y += hy * f3 * invC;
            c.m_position.z += hz * f3 * invC;
        }
        if (!(m_isClient && b.m_netControlled))
        {
            b.m_position.x -= hx * f3 * invB;
            b.m_position.y -= hy * f3 * invB;
            b.m_position.z -= hz * f3 * invB;
        }
    }
}


// Runs constraint and collision iterations, restoring client-controlled particle positions after each round.
// Collects colliding objects once and releases them after the solve.

void RagDoll::satisfyConstraints()
{
    std::vector<Vec3> netPositions;
    if (m_isClient)
    {
        for (size_t i = 0; i < m_particles.size(); ++i)
            if (m_particles[i].m_netControlled)
                netPositions.push_back(m_particles[i].m_position);
    }

    if (*rd_useObjectCollision || *rd_useCapsuleCollision)
        getCollidingObjects();

    for (int round = 0; round < *rd_iterations; ++round)
    {
        if (*rd_useConstraints)
        {
            checkAngularConstraints();
            checkDihedralAngleConstraints();
            checkLegAngularConstraints();
            checkDistanceLessThanConstraints();
            checkDistanceConstraints();
        }
        checkCollisionConstraints();

        if (m_isClient)
        {
            size_t k = 0;
            for (size_t i = 0; i < m_particles.size(); ++i)
            {
                Particle& p = m_particles[i];
                if (p.m_netControlled)
                {
                    p.m_position = netPositions[k];
                    p.m_oldPosition = netPositions[k];
                    ++k;
                }
            }
        }
    }

    restoreCollidingObjects();
}


// The three body axes, from the two hips (1, 6) and the right shoulder (31):
// right   = normalise(hip1 - hip6)
// forward = normalise((hip6 - shoulder31) x (shoulder31 - hip1))
// up      = normalise(right x forward)

Vec3 RagDoll::getForward()
{
    const Particle& s = m_particles[m_template->findParticleIndex(31)];
    const Particle& h1 = m_particles[m_template->findParticleIndex(1)];
    const Particle& h6 = m_particles[m_template->findParticleIndex(6)];

    const float wx = s.m_position.x - h1.m_position.x;
    const float wy = s.m_position.y - h1.m_position.y;
    const float wz = s.m_position.z - h1.m_position.z;
    const float uy = h6.m_position.y - s.m_position.y;
    const float uz = h6.m_position.z - s.m_position.z;
    const float ux = h6.m_position.x - s.m_position.x;

    Vec3 forward;
    forward.x = uy * wz - uz * wy;
    forward.y = uz * wx - ux * wz;
    forward.z = ux * wy - uy * wx;
    normalizeShipped(forward);
    return forward;
}

Vec3 RagDoll::getRight()
{
    const Particle& h1 = m_particles[m_template->findParticleIndex(1)];
    const Particle& h6 = m_particles[m_template->findParticleIndex(6)];
    Vec3 right;
    right.x = h1.m_position.x - h6.m_position.x;
    right.y = h1.m_position.y - h6.m_position.y;
    right.z = h1.m_position.z - h6.m_position.z;
    normalizeShipped(right);
    return right;
}

Vec3 RagDoll::getUp()
{
    const Vec3 right = getRight();
    const Vec3 forward = getForward();
    Vec3 up;
    up.x = right.y * forward.z - right.z * forward.y;
    up.y = right.z * forward.x - forward.z * right.x;
    up.z = forward.y * right.x - forward.x * right.y;
    normalizeShipped(up);
    return up;
}


// Rebuilds particles from transformed skeleton bones and seeds history with 0.015 seconds of velocity.
// rd_netBoneEnable selects network control for bones 1, 6, 15, and 31.

void RagDoll::reset(const Mat4& transform, const Vec3& velocity)
{
    m_template->m_active = false;
    m_particles.clear();

    Vec3 inherit;
    inherit.x = velocity.x * 0.015f;
    inherit.y = velocity.y * 0.015f;
    inherit.z = velocity.z * 0.015f;

    const std::vector<Particle>& source = m_template->m_particles;
    const int mask = *rd_netBoneEnable;
    for (size_t k = 0; k < source.size(); ++k)
    {
        const int bone = source[k].m_bone;
        const Mat4& boneMat = bone < 0 ? Mat4::getIdentity() : m_skeleton->m_transforms[bone];
        const float bx = boneMat.m[12];
        const float by = boneMat.m[13];
        const float bz = boneMat.m[14];
        const float* const t = transform.m;
        Vec3 world;
        world.y = t[9] * bz + t[5] * by + t[1] * bx + t[13];
        world.x = t[4] * by + t[0] * bx + t[8] * bz + t[12];
        world.z = bz * t[10] + bx * t[2] + by * t[6] + t[14];

        Particle p = source[k];
        p.m_position = world;
        p.m_oldPosition.x = world.x - inherit.x;
        p.m_oldPosition.y = world.y - inherit.y;
        p.m_oldPosition.z = world.z - inherit.z;
        if ((bone == 1 && (mask & 1)) || (bone == 6 && (mask & 2))
            || (bone == 15 && (mask & 4)) || (bone == 31 && (mask & 8)))
            p.m_netControlled = true;
        m_particles.push_back(p);
    }
}


// Advances the ragdoll simulation and returns its center.
// Inactive or not-yet-synchronized instances update center tracking without a simulation step.

const Vec3& RagDoll::update(float dt)
{
    typedef hfe::world::object_detail::LayoutAccess L;

    float step = *rd_dT;
    if (m_updateMode > 0)
        step = *rd_dT * 1.5f;
    const float scaled = dt * *rd_slowMotion;

    if (m_template->m_distanceConstraints.empty())
        return m_position;

    if (scaled < 0.0f || m_exited)
    {
        m_position = getCenter().m_position;
        return m_position;
    }

    bool awake;
    if (!m_isClient)
    {
        awake = m_awake;
    }
    else
    {
        if (m_firstNetState)
        {
            m_position = getCenter().m_position;
            return m_position;
        }
        awake = m_awake;
        if (awake && m_clientTimer > 0.0f)
        {
            m_clientTimer -= scaled;
            if (m_clientTimer <= 0.0f)
            {
                m_clientTimer = 0.0f;
                setIsClient(false);
                awake = m_awake;
            }
        }
    }

    if (awake)
    {
        m_stepAccumulator += scaled;
        m_collisionTimer += scaled;
        if (m_stepAccumulator >= step)
        {
            int n = static_cast<int>(m_stepAccumulator / step);
            m_stepAccumulator -= static_cast<float>(n) * step;
            for (; n > 0; --n)
            {
                if (m_gravityOverTime > 0.0f)
                {
                    m_gravityTime += step;
                    if (m_gravityTime >= m_gravityOverTime)
                        m_gravityTime = m_gravityOverTime;
                }
                accumulateForces();
                verlet(step);
                satisfyConstraints();
            }
        }

        const Vec3 centre = getCenter().m_position;
        if (*rd_sleep <= 0.0f)
        {
            m_awake = true;
        }
        else
        {
            const float mx = centre.x - m_position.x;
            const float my = centre.y - m_position.y;
            const float mz = centre.z - m_position.z;
            const float delta = *rd_sleepDelta;
            if (mz * mz + my * my + mx * mx > delta * delta)
            {
                wakeUp();
            }
            else
            {
                m_sleepTimer -= scaled;
                if (m_sleepTimer <= 0.0f)
                {
                    m_sleepTimer = 0.0f;
                    if (m_awake)
                        m_awake = false;
                }
            }
        }
        m_position = centre;
    }
    else
    {
        m_stepAccumulator += scaled;
        if (m_stepAccumulator >= 0.033333335f)
        {
            m_stepAccumulator = 0.0f;
            getCollidingObjects();
            for (size_t i = 0; i < m_objects.size(); ++i)
            {
                hfe::world::IObject* const object = m_objects[i];
                if (!object)
                    continue;
                if (object->getTemplate()->getClassID() == hfe::world::CID_TriggerableTargetTemplate)
                    wakeUp();
                hfe::world::IPhysicsNode* const node =
                    static_cast<hfe::world::IPhysicsNode*>(L::physicsNode(object));      // +0x44
                if (!node)
                    continue;
                if (node->getIsSleeping())
                    continue;
                if (!node->getIsMobile())
                    continue;
                const Vec3& speed = node->getPositionalSpeed();
                if (speed.z * speed.z + speed.y * speed.y + speed.x * speed.x < 0.001f)
                    continue;
                wakeUp();
            }
            restoreCollidingObjects();
        }
    }

    if (m_template->m_drawConstraints)
        drawConstraints();
    return m_position;
}


// Leaving client mode after a network update promotes received positions, clears forces, and solves constraints once.

void RagDoll::setIsClient(bool isClient)
{
    if (isClient == m_isClient)
        return;
    if (!isClient)
    {
        if (m_isClient && !m_firstNetState)
        {
            for (size_t i = 0; i < m_particles.size(); ++i)
            {
                Particle& p = m_particles[i];
                if (!p.m_netControlled)
                    continue;
                const Vec3 cur = p.m_position;
                p.m_position = p.m_netPosition;
                p.m_oldPosition = cur;
                setVec(p.m_acceleration, 0.0f, 0.0f, 0.0f);
                setVec(p.m_force, 0.0f, 0.0f, 0.0f);
            }
            satisfyConstraints();
        }
        m_clientTimer = 0.0f;
    }
    m_isClient = isClient;
}


// Ragdoll state uses a nine-bit length prefix. The first controlled particle is uncompressed;
// subsequent positions use the configured relative encoding.

void RagDoll::skipCurrentState(hfe::io::BitStream* stream)
{
    unsigned int payloadBits = 0;
    stream->readBits(&payloadBits, 9);
    stream->skipBits(payloadBits);
}

void RagDoll::writeCurrentState(hfe::io::BitStream* stream)
{
    hfe::io::BSPosition mark;
    mark.m_unknown00 = 0;
    mark.m_unknown08 = 0;
    if (stream->m_readWriteSwitch == 1)
    {
        mark.m_readWriteSwitch = 1;
        mark.m_position = stream->getBitPosition();
    }
    else
    {
        mark.m_readWriteSwitch = stream->m_readWriteSwitch;
        mark.m_position = stream->getBitPosition();
    }
    stream->skipBits(9);
    const unsigned int start = stream->getBitPosition();

    const Vec3* reference = 0;
    for (size_t i = 0; i < m_particles.size(); ++i)
    {
        const Particle& p = m_particles[i];
        if (!p.m_netControlled)
            continue;
        if (i)
        {
            stream->writeCompressedVector2(p.m_position, *reference, 0.01f);
        }
        else
        {
            reference = &p.m_position;
            stream->writeBits(&p.m_position.x, 32);
            stream->writeBits(&p.m_position.y, 32);
            stream->writeBits(&p.m_position.z, 32);
        }
    }

    unsigned int payloadBits = stream->getBitPosition() - start;
    stream->setWritePosition(mark);
    stream->writeBits(&payloadBits, 9);
    stream->resetWritePosition();
}

void RagDoll::readCurrentState(hfe::io::BitStream* stream)
{
    m_clientTimer = 0.3f;
    setIsClient(true);
    wakeUp();

    unsigned int payloadBits = 0;
    stream->readBits(&payloadBits, 9);

    const Vec3* reference = 0;
    for (size_t i = 0; i < m_particles.size(); ++i)
    {
        Particle& p = m_particles[i];
        if (!p.m_netControlled)
            continue;
        if (i)
        {
            p.m_netPosition = stream->readCompressedVector2(*reference, 0.01f);
        }
        else
        {
            Vec3 v;
            setVec(v, 0.0f, 0.0f, 0.0f);
            stream->readBits(&v.x, 32);
            stream->readBits(&v.y, 32);
            stream->readBits(&v.z, 32);
            p.m_netPosition = v;
            reference = &p.m_netPosition;
        }
        if (m_firstNetState)
            p.m_position = p.m_netPosition;
    }
    m_firstNetState = false;
}

// The hip particle. aborts without one - as shipped.
RagDoll::Particle& RagDoll::getCenter()
{
    const int index = m_template->findParticleIndex(6);
    if (index == -1)
        std::abort();
    return m_particles[index];
}


// Writes ragdoll particle poses back to the skeleton's absolute bone transforms.

void RagDoll::applyOnSkeleton()
{
    static std::vector<Vec3> localSpaceCoords;

    RagDollTemplate* const t = m_template;
    const int centreIndex = t->findParticleIndex(6);
    if (centreIndex == -1)
        std::abort();
    const Vec3 centre = m_particles[centreIndex].m_position;

    for (size_t i = 0; i < m_particles.size(); ++i)
    {
        Vec3 l;
        l.x = m_particles[i].m_position.x - centre.x;
        l.y = m_particles[i].m_position.y - centre.y;
        l.z = m_particles[i].m_position.z - centre.z;
        localSpaceCoords.push_back(l);
    }

    const Vec3 forward = getForward();
    const Vec3 right = getRight();
    Vec3 legPlaneNormal;
    setVec(legPlaneNormal, 1.0f, 0.0f, 0.0f);
    Vec3 upHint;
    setVec(upHint, 1.0f, 0.0f, 0.0f);

    Skeleton* const sk = m_skeleton;
    const std::vector<RagDollTemplate::BoneMapping>& mappings = t->m_boneMappings;
    for (size_t k = 0; k < mappings.size(); ++k)
    {
        const int bone = mappings[k].m_bone;
        const int partA = mappings[k].m_partA;
        const int partB = mappings[k].m_partB;

        Mat4 boneMat = bone < 0 ? Mat4::getIdentity() : sk->m_transforms[bone];
        Vec3 translation;
        setVec(translation, boneMat.m[12], boneMat.m[13], boneMat.m[14]);

        const int ia = t->findParticleIndex(bone < 0 ? -bone : bone);
        const int ib = t->findParticleIndex(partA < 0 ? -partA : partA);
        const int ic = t->findParticleIndex(partB < 0 ? -partB : partB);

        Vec3 dir;
        if (ia < 0)
        {
            Vec3 pa, pb;
            if (ib < 0)
            {
                const int j = partA < 0 ? -partA : partA;
                const Mat4& mA = j < 0 ? Mat4::getIdentity() : sk->m_transforms[j];
                setVec(pa, mA.m[12], mA.m[13], mA.m[14]);
            }
            else
                pa = localSpaceCoords[ib];
            if (ic < 0)
            {
                const int j = partB < 0 ? -partB : partB;
                const Mat4& mB = j < 0 ? Mat4::getIdentity() : sk->m_transforms[j];
                setVec(pb, mB.m[12], mB.m[13], mB.m[14]);
            }
            else
                pb = localSpaceCoords[ic];

            dir.x = pb.x - pa.x;
            dir.y = pb.y - pa.y;
            dir.z = pb.z - pa.z;
            normalizeShipped(dir);
            if (!(partA < 0 || partB < 0))
            {
                const float dist = t->getBoneDistance(partA, bone < 0 ? -bone : bone);
                translation.x = pa.x + dir.x * dist;
                translation.y = pa.y + dir.y * dist;
                translation.z = pa.z + dir.z * dist;
            }
        }
        else
        {
            if (partA >= 0 && partB >= 0)
                translation = localSpaceCoords[ia];
            dir.x = localSpaceCoords[ic].x - localSpaceCoords[ib].x;
            dir.y = localSpaceCoords[ic].y - localSpaceCoords[ib].y;
            dir.z = localSpaceCoords[ic].z - localSpaceCoords[ib].z;
            normalizeShipped(dir);

            if (bone == 1 || bone == 6)
            {
                const int i2 = t->findParticleIndex(bone + 1);
                const int i4 = t->findParticleIndex(bone + 3);
                const float ax = localSpaceCoords[i2].x - translation.x;
                const float ay = localSpaceCoords[i2].y - translation.y;
                const float az = localSpaceCoords[i2].z - translation.z;
                const float bx = localSpaceCoords[i4].x - translation.x;
                const float by = localSpaceCoords[i4].y - translation.y;
                const float bz = localSpaceCoords[i4].z - translation.z;
                legPlaneNormal.x = by * az - ay * bz;
                legPlaneNormal.y = bz * ax - az * bx;
                legPlaneNormal.z = bx * ay - ax * by;
                normalizeShipped(legPlaneNormal);
            }
        }

        if ((bone > 5 && bone <= 9) || (bone > 0 && bone <= 4))
            upHint = legPlaneNormal;
        if (bone == 0 || bone == 11 || bone == 12 || bone == 13 || bone == 45 || bone == 46 || bone == 47)
            setVec(upHint, -right.x, -right.y, -right.z);
        if (bone == 30)
            upHint = forward;
        if (bone == 14)
            setVec(upHint, -forward.x, -forward.y, -forward.z);
        if (bone >= 31 && bone <= 34)
        {
            const int i31 = t->findParticleIndex(31);
            const int i32 = t->findParticleIndex(32);
            const int i34 = t->findParticleIndex(34);
            const float ax = localSpaceCoords[i32].x - localSpaceCoords[i31].x;
            const float ay = localSpaceCoords[i32].y - localSpaceCoords[i31].y;
            const float az = localSpaceCoords[i32].z - localSpaceCoords[i31].z;
            const float bx = localSpaceCoords[i34].x - localSpaceCoords[i31].x;
            const float by = localSpaceCoords[i34].y - localSpaceCoords[i31].y;
            const float bz = localSpaceCoords[i34].z - localSpaceCoords[i31].z;
            Vec3 n;
            n.x = az * by - bz * ay;
            n.y = bz * ax - az * bx;
            n.z = ay * bx - ax * by;
            normalizeShipped(n);
            upHint.x = n.y * dir.z - n.z * dir.y;
            upHint.y = n.z * dir.x - dir.z * n.x;
            upHint.z = dir.y * n.x - dir.x * n.y;
        }
        if (bone >= 15 && bone <= 19)
        {
            const int i15 = t->findParticleIndex(15);
            const int i16 = t->findParticleIndex(16);
            const int i19 = t->findParticleIndex(19);
            const float ax = localSpaceCoords[i16].x - localSpaceCoords[i15].x;
            const float ay = localSpaceCoords[i16].y - localSpaceCoords[i15].y;
            const float az = localSpaceCoords[i16].z - localSpaceCoords[i15].z;
            const float bx = localSpaceCoords[i19].x - localSpaceCoords[i15].x;
            const float by = localSpaceCoords[i19].y - localSpaceCoords[i15].y;
            const float bz = localSpaceCoords[i19].z - localSpaceCoords[i15].z;
            Vec3 n;
            n.x = az * by - bz * ay;
            n.y = bz * ax - az * bx;
            n.z = ay * bx - ax * by;
            normalizeShipped(n);
            upHint.x = n.y * dir.z - n.z * dir.y;
            upHint.y = n.z * dir.x - dir.z * n.x;
            upHint.z = dir.y * n.x - dir.x * n.y;
        }

        Vec3 row0 = upHint;                       // makeOrthonormalBasis(forward = hint, up = axis)
        Vec3 row1 = dir;
        Vec3 row2;
        setVec(row2, 0.0f, 0.0f, 0.0f);
        row2.makeOrthonormalBasis(row0, row1);

        if (partA == partB)
        {
            // A leaf: its own local rotation under the parent's absolute transform.
            const Skeleton::Bone& leaf = sk->m_bones[partA];
            const int parent = sk->m_boneTemplates[partA].m_parentIndex;
            const Mat4& parentMat = parent < 0 ? Mat4::getIdentity() : sk->m_transforms[parent];
            Mat4 world;
            world.mult(leaf.m_transform, parentMat);
            for (int c = 0; c < 12; ++c)
                boneMat.m[c] = world.m[c];
        }
        else
        {
            boneMat.m[0] = row0.x; boneMat.m[1] = row0.y; boneMat.m[2]  = row0.z;
            boneMat.m[4] = row1.x; boneMat.m[5] = row1.y; boneMat.m[6]  = row1.z;
            boneMat.m[8] = row2.x; boneMat.m[9] = row2.y; boneMat.m[10] = row2.z;
        }
        boneMat.m[12] = translation.x;
        boneMat.m[13] = translation.y;
        boneMat.m[14] = translation.z;
        sk->m_transforms[bone] = boneMat;
    }

    for (int i = 0; i < sk->m_boneCount; ++i)
    {
        if (!t->m_lockedBones[i])
            continue;
        const Mat4 local = sk->m_bones[i].m_transform;
        const int parent = sk->m_boneTemplates[i].m_parentIndex;
        Mat4 absolute;
        if (parent < 0)
            absolute = (i + 1 < 0) ? Mat4::getIdentity() : sk->m_transforms[i + 1];   // shipped
        else
            absolute.mult(local, sk->m_transforms[parent]);
        sk->m_transforms[i] = absolute;
    }

    localSpaceCoords.clear();
}

// The lowest of foot 9, foot 4 and head 47.
Vec3 RagDoll::getLowestVisiblePosition()
{
    RagDollTemplate* const t = m_template;
    Vec3 lowest = m_particles[t->findParticleIndex(9)].m_position;
    const Vec3& foot4 = m_particles[t->findParticleIndex(4)].m_position;
    if (lowest.y > foot4.y)
        lowest = foot4;
    const Vec3& head = m_particles[t->findParticleIndex(47)].m_position;
    if (!(lowest.y <= head.y))
        lowest = head;
    return lowest;
}

} // namespace anim
} // namespace dice
