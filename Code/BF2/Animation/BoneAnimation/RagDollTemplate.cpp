// Shared ragdoll configuration, instance creation and per-frame updates.
// [CHOSEN] split from RagDoll.cpp. The template owns its reference skeleton and must outlive
// Every registered instance. writeConstraints remains a declaration-only [FRONTIER].
// SystemFieldTwin is temporary access to the CPU frequency; remove it when System exposes it.

#include "RagDollTemplate.h"

#include "../../Core/System.h"
#include "../../Game/Utility/Vars.h"  // dice::hfe::Vars::getFloat - the cvar handle pattern
#include "RagDoll.h"                  // the instance half of the same ELF TU
#include "Skeleton.h"                 // the owned reference skeleton: skeleton(), getBoneDistance
#include "../../IO/File/File.h"       // io::File - writeConstraints

#include <cfloat>       // FLT_EPSILON
#include <cmath>        // sqrtf, fabsf
#include <cstdio>       // fprintf/fflush - the [FRONTIER] stops
#include <cstdlib>      // abort
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <utility>

#if defined(_WIN32)
#include <intrin.h>     // __rdtsc
#endif

namespace dice {
namespace anim {


// Active ragdoll template singleton.

RagDollTemplate* g_ragDollTemplate = 0;

namespace {


// Client-side per-ragdoll update time budget.

float* const g_ragdollClientMaxUpdateTime = hfe::Vars::getFloat("ragdoll_clientMaxUpdateTime", 0.002f);
float* const g_ragdollMaxInheritSpeed     = hfe::Vars::getFloat("ragdoll-max-inherit-speed", 40.0f);
float* const g_ragdollBoneSize            = hfe::Vars::getFloat("ragdoll_boneSize", 0.1f);
float* const g_ragdollKneeBoneSize        = hfe::Vars::getFloat("ragdoll_kneeBoneSize", 0.05f);
float* const g_ragdollBodyBoneSize        = hfe::Vars::getFloat("ragdoll_bodyBoneSize", 0.12f);
// [CHOSEN mechanism] CPU timestamp helper matching System's calibration.
inline unsigned long long rdtsc()
{
#if defined(_WIN32)
    return __rdtsc();
#elif defined(__aarch64__)
    // [CHOSEN, not recovered] arm64 macOS port (untested): the generic timer's virtual count is
    // the constant-rate counter that stands in for the TSC; callers calibrate its frequency.
    unsigned long long count;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(count));
    return count;
#else
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
#endif
}


// [CHOSEN, not recovered] Layout view used to read System's CPU tick calibration.
// [INFERRED] Original access level. Replace this view with a typed accessor when the System interface exposes it.

struct SystemFieldTwin
{
    void*              m_vptr;               // System is polymorphic: vptr at +0x00, m_refCount +0x04
    int                m_refCount;
    int                m_simulationRate;
    double             m_exactTime;
    unsigned long long m_cpuTicksPerSecond;  // ELF +0x14
};

inline unsigned long long cpuTicksPerSecond()
{
    return reinterpret_cast<const SystemFieldTwin*>(hfe::g_system)->m_cpuTicksPerSecond;
}

} // namespace


// Creates the ragdoll template singleton if it is absent.

void RagDollTemplate::init()
{
    if (!g_ragDollTemplate)
        g_ragDollTemplate = new RagDollTemplate;
}


// Deletes the ragdoll template singleton and clears its pointer.

void RagDollTemplate::shutdown()
{
    if (g_ragDollTemplate)
        delete g_ragDollTemplate;
    g_ragDollTemplate = 0;
}


// No automatic initialization is performed by this hook.

void RagDollTemplate::autoInit()
{
}


// Initializes empty ragdoll configuration with gravity -30 and 128 bone-lock/link entries.

RagDollTemplate::RagDollTemplate()
    : m_drawConstraints(false), m_active(false), m_resetRequested(false), m_gravity(-30.0f),
      m_skeleton(0), m_lastInstance(0),
      m_particleCollisionCriteria1(-1.0f), m_particleCollisionCriteria2(0.0f)
{
    m_lockedBones.resize(128, false);
    m_linkModes.resize(128, 0);
    for (int i = 0; i <= 127; ++i)
        m_particleIndexes[i] = -1;
}


// Destroys template resources. Aborts if any ragdoll instances are still registered.

RagDollTemplate::~RagDollTemplate()
{
    if (!myInstances.empty())
        abort();
    delete m_skeleton;                                   // x - the owned reference skeleton
}


// A single _Rb_tree::erase(const key_type&) call on the map at +0x84 and nothing else. Return
// type [CHOSEN] void: eax carries the erase count out, but no caller in the binary reads it.

void RagDollTemplate::removeInstance(RagDoll* ragDoll)
{
    myInstances.erase(ragDoll);
}

// Updates registered ragdolls within the frame budget.
// [INFERRED] parameter names: viewerPos is a position and viewerDir is a direction.
// Without sorting, uses registry order. Viewer sorting favors near/front instances;
// Equal float keys collapse to one entry, so a tied instance may skip this frame.
void RagDollTemplate::updateInstances(bool sortFromViewer, const hfe::Vec3& viewerPos,
                                      const hfe::Vec3& viewerDir, float dt)
{
    // Construct the sort map even when no instances are registered.
    std::map<float, RagDoll*> sorted;

    // fldz -> var_98. The unsorted path's running key.
    float nextKey = 0.0f;

    // Build the update order, re-reading the registry end on each iteration.
    for (std::map<RagDoll*, bool>::iterator it = myInstances.begin(); it != myInstances.end(); ++it)
    {
        // mov ebx, [edx+10h]  - the node's value_type.first, the RagDoll*. The bool
        // payload at +0x14 is never read here (nor anywhere else recovered).
        RagDoll* instance = it->first;

        float key;
        if (sortFromViewer)
        {
            // Viewer-to-instance delta.
            float dx = instance->m_position.x - viewerPos.x;
            float dy = instance->m_position.y - viewerPos.y;
            float dz = instance->m_position.z - viewerPos.z;

            // Preserve the z/y/x summation order.
            key = sqrtf((dz * dz + dy * dy) + dx * dx);

            // Normalize the delta independently of the earlier length calculation.
            const float lengthSq = (dy * dy + dx * dx) + dz * dz;
            if (fabsf(lengthSq - 1.0f) < FLT_EPSILON)
            {
                // already unit length - nothing to do
            }
            else if (fabsf(lengthSq) >= FLT_EPSILON)
            {
                const float inv = 1.0f / sqrtf(lengthSq);
                dx *= inv;
                dy *= inv;
                dz *= inv;
            }
            else
            {
                dx = 0.0f;
                dy = 0.0f;
                dz = 0.0f;
            }

            // Apply a threefold distance penalty behind the viewer; NaN does not take the penalty.
            if (dx * viewerDir.x + dy * viewerDir.y + dz * viewerDir.z < 0.0f)
                key *= 3.0f;
        }
        else
        {
            // fld var_98 / fadd flt_888CA8C (== 1.0f) / fst var_98. First key is 1.0f.
            nextKey += 1.0f;
            key = nextKey;
        }

        // Equal distance keys share a map entry; preserve unique insertion.
        sorted.insert(std::pair<const float, RagDoll*>(key, instance));
    }

    // Start the frame budget even when the registry is empty.
    const unsigned long long startTicks = rdtsc();

    // xor esi, esi.
    int sleepingSeen = 0;

    for (std::map<float, RagDoll*>::iterator it = sorted.begin(); it != sorted.end(); ++it)
    {
        // mov ebx, [edx+14h] - value_type.second.
        RagDoll* instance = it->second;

        // Reset update mode even when the ragdoll is disabled.
        const bool disabled = instance->m_exited;
        instance->m_updateMode = 0;
        if (disabled)
            continue;

        if (sortFromViewer)
        {
            // Demote later instances to mode 1 once more than two awake instances have been visited.
            if (sleepingSeen > 2)
                instance->m_updateMode = 1;

            // Client instances over the frame budget use mode 2; compare elapsed milliseconds as float.
            const double elapsedTicks = static_cast<double>(rdtsc() - startTicks);
            const double ticksPerSecond = static_cast<double>(cpuTicksPerSecond());
            const float elapsedMs = static_cast<float>(elapsedTicks / ticksPerSecond * 1000.0);

            if (elapsedMs > *g_ragdollClientMaxUpdateTime && instance->m_isClient)
                instance->m_updateMode = 2;

            // Count after this instance's demotion checks, so crossing the threshold affects only later instances.
            if (instance->m_awake)
                ++sleepingSeen;
        }

        // Forward frame time unchanged; this is not the fixed simulation step.
        instance->update(dt);

        // Unsorted updates also write the pose back to the skeleton.
        if (!sortFromViewer)
            instance->applyOnSkeleton();
    }

}


// Creates, resets, and registers an instance with clamped inherited velocity.
// Remembers it as the target of the console addForce command.

RagDoll* RagDollTemplate::makeInstance(const hfe::Mat4& transform, const hfe::Vec3& velocity,
                                       Skeleton& skeleton)
{
    hfe::Vec3 v = velocity;
    const float speed = sqrtf(velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z);
    const float maxSpeed = *g_ragdollMaxInheritSpeed;
    if (speed > maxSpeed)
    {
        const float k = maxSpeed / speed;
        v.x *= k;
        v.y *= k;
        v.z *= k;
    }
    RagDoll* const instance = new RagDoll(*this);
    instance->m_skeleton = &skeleton;
    instance->reset(transform, v);
    myInstances.insert(std::make_pair(instance, true));
    m_lastInstance = instance;
    return instance;
}


// Constraint adders insert at the front, so solvers run in reverse script order.


// The console `addForce` lands on the last instance made.
void RagDollTemplate::addForce(int bone, const hfe::Vec3& force)
{
    if (m_lastInstance)
        m_lastInstance->addForce(bone, force);
}

// Distance between two bones' rest positions on the reference skeleton (identity
// for a negative bone id).
float RagDollTemplate::getBoneDistance(int boneA, int boneB)
{
    const hfe::Mat4& a = boneA < 0 ? hfe::Mat4::getIdentity() : m_skeleton->m_transforms[boneA];
    const hfe::Mat4& b = boneB < 0 ? hfe::Mat4::getIdentity() : m_skeleton->m_transforms[boneB];
    const float dy = a.m[13] - b.m[13];
    const float dx = a.m[12] - b.m[12];
    const float dz = a.m[14] - b.m[14];
    return sqrtf(dy * dy + dx * dx + dz * dz);
}

// Marks an inclusive bone range as locked: applyOnSkeleton recomputes those from
// their parents instead of from particles. No bounds check - as shipped.
void RagDollTemplate::lockBone(int fromBone, int toBone)
{
    for (int i = fromBone; i <= toBone; ++i)
        m_lockedBones[i] = true;
}

// A prototype particle for `bone` at the origin (RagDoll::reset places it):
// radius rd_kneeBoneSize for the knees (2, 7), rd_bodyBoneSize for hips and shoulders
// (1, 6, 15, 31), rd_boneSize otherwise. Records the particle index for the bone.
// The bone-id bound check (> 127 aborts) comes after the push_back - as shipped.
void RagDollTemplate::addParticle(int bone, float mass)
{
    float size = *g_ragdollBoneSize;
    if (bone == 2 || bone == 7)
        size = *g_ragdollKneeBoneSize;
    else if (bone == 6 || bone == 31 || bone == 1 || bone == 15)
        size = *g_ragdollBodyBoneSize;

    const int index = static_cast<int>(m_particles.size());
    m_particles.push_back(RagDoll::Particle(hfe::Vec3::getZero(), bone, mass, size));
    if (bone > 127)
        abort();
    m_particleIndexes[bone] = index;
}

// The distance constraint joining two particle indices, in either order; end()
// when there is none.
std::list<RagDollTemplate::Constraint>::iterator RagDollTemplate::findConstraint(int particleA, int particleB)
{
    std::list<Constraint>::iterator it = m_distanceConstraints.begin();
    for (; it != m_distanceConstraints.end(); ++it)
    {
        if (particleA == it->m_a && particleB == it->m_b)
            break;
        if (particleB == it->m_a && particleA == it->m_b)
            break;
    }
    return it;
}

// Only the upper bound is checked - a negative bone id reads before the table,
// as shipped; every recovered caller that can pass one guards it first.
int RagDollTemplate::findParticleIndex(int bone)
{
    if (bone <= 127)
        return m_particleIndexes[bone];
    return -1;
}

int RagDollTemplate::findBoneIndex(int particleIndex)
{
    return m_particles[particleIndex].m_bone;
}

// The a-c distance implied by the angle at b (law of cosines on the two bone
// lengths, |angle| in degrees); a negative angle stores the distance negated, which the solver
// reads as a maximum. An existing (a, b, c) entry is updated in place.
void RagDollTemplate::addAngularConstraint(int a, int b, int c, float angleDeg)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    const int ic = findParticleIndex(c);
    if (ia < 0 || ib < 0 || ic < 0)
        return;

    AngleConstraint k;
    k.m_a = ia;
    k.m_b = ib;
    k.m_c = ic;
    k.m_distAB = getBoneDistance(a, b);
    k.m_distBC = getBoneDistance(b, c);
    const float radians = fabsf(angleDeg) * 0.017453292f;
    float dAC = sqrtf(k.m_distBC * k.m_distBC + k.m_distAB * k.m_distAB
                      - cosf(radians) * (k.m_distAB * k.m_distBC + k.m_distAB * k.m_distBC));
    if (0.0f > angleDeg)
        dAC = -dAC;
    k.m_distAC = dAC;

    for (std::list<AngleConstraint>::iterator it = m_angularConstraints.begin();
         it != m_angularConstraints.end(); ++it)
    {
        if (it->m_a == ia && it->m_b == ib && it->m_c == ic)
        {
            it->m_distAC = dAC;
            return;
        }
    }
    m_angularConstraints.insert(m_angularConstraints.begin(), k);
}

// The (d-c)+(b-a) length implied by the angle (degrees, not absolute here) on the
// a-b and c-d lengths. An existing (a, b, c, d) entry is updated in place.
void RagDollTemplate::addDihedralAngleConstraint(int a, int b, int c, int d, float angleDeg)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    const int ic = findParticleIndex(c);
    const int id = findParticleIndex(d);
    if (ia < 0 || ib < 0 || ic < 0 || id < 0)
        return;

    DihedralAngleConstraint k;
    k.m_a = ia;
    k.m_b = ib;
    k.m_c = ic;
    k.m_d = id;
    k.m_distAB = getBoneDistance(a, b);
    k.m_distBC = getBoneDistance(b, c);
    k.m_distCD = getBoneDistance(c, d);
    const float radians = angleDeg * 0.017453292f;
    k.m_targetAD = sqrtf(k.m_distCD * k.m_distCD + k.m_distAB * k.m_distAB
                         - cosf(radians) * (k.m_distAB * k.m_distCD + k.m_distAB * k.m_distCD));

    for (std::list<DihedralAngleConstraint>::iterator it = m_dihedralAngleConstraints.begin();
         it != m_dihedralAngleConstraints.end(); ++it)
    {
        if (it->m_a == ia && it->m_b == ib && it->m_c == ic && it->m_d == id)
        {
            it->m_targetAD = k.m_targetAD;
            return;
        }
    }
    m_dihedralAngleConstraints.insert(m_dihedralAngleConstraints.begin(), k);
}

void RagDollTemplate::addDistanceLessThanConstraint(int a, int b, float minDistance)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    if (ia < 0 || ib < 0)
        return;
    DistanceLessThanConstraint k;
    k.m_a = ia;
    k.m_b = ib;
    k.m_minDistance = minDistance;
    m_distanceLessThanConstraints.insert(m_distanceLessThanConstraints.begin(), k);
}

// The bone mappings keep script order (push_back).
void RagDollTemplate::toSkeleton(int bone, int partA, int partB)
{
    BoneMapping m;
    m.m_bone = bone;
    m.m_partA = partA;
    m.m_partB = partB;
    m_boneMappings.push_back(m);
}

// The rest length is the skeleton distance; an existing pair is updated.
void RagDollTemplate::addDistanceConstraint(int a, int b)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    if (ia < 0 || ib < 0)
        return;
    const float length = getBoneDistance(a, b);
    std::list<Constraint>::iterator it = findConstraint(ia, ib);
    if (it == m_distanceConstraints.end())
    {
        Constraint k;
        k.m_a = ia;
        k.m_b = ib;
        k.m_length = length;
        m_distanceConstraints.insert(m_distanceConstraints.begin(), k);
    }
    else
    {
        it->m_length = length;
    }
}

void RagDollTemplate::addCapsuleCollision(int a, int b)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    if (ia < 0 || ib < 0)
        return;
    Constraint k;
    k.m_a = ia;
    k.m_b = ib;
    k.m_length = 0.0f;
    m_capsuleConstraints.insert(m_capsuleConstraints.begin(), k);
}

// The joint list is never read by any recovered solver body.
void RagDollTemplate::addJointConstraint(int a, int b)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    if (ia < 0 || ib < 0)
        return;
    Constraint k;
    k.m_a = ia;
    k.m_b = ib;
    k.m_length = 0.0f;
    m_jointConstraints.insert(m_jointConstraints.begin(), k);
}

// No index guard - an unknown bone yields -1, which findConstraint never
// matches, so nothing is removed. As shipped.
void RagDollTemplate::removeConstraint(int a, int b)
{
    const int ia = findParticleIndex(a);
    const int ib = findParticleIndex(b);
    std::list<Constraint>::iterator it = findConstraint(ia, ib);
    if (it != m_distanceConstraints.end())
        m_distanceConstraints.erase(it);
}

// The console `ragDoll.save` tool (ELF 0x8341110): one "ragDoll.addConstraint <boneA> <boneB> <length>"
// line per distance constraint, written through io::File to ragDollConstraints.inc (mode 0,
// access 2, as SettingsManagerBase::save opens its files). The particle indices map back to bone
// ids; the getBoneDistance result is computed and discarded, as shipped.
void RagDollTemplate::writeConstraints()
{
    // Retail writes the relative name, which lands in the game folder; the macOS client does the
    // same (docs/macos-write-paths.md).
    const char* const path = "ragDollConstraints.inc";
    hfe::io::File file;
    file.open(path, static_cast<hfe::io::IFileSystem::FileMode>(0), hfe::io::IFileSystem::FILEACCESS_2, 0);
    hfe::io::IStream* stream = file.getStream();
    std::ostringstream text;
    for (std::list<Constraint>::iterator it = m_distanceConstraints.begin();
         it != m_distanceConstraints.end(); ++it)
    {
        const int boneA = findBoneIndex(it->m_a);
        const int boneB = findBoneIndex(it->m_b);
        (void)getBoneDistance(boneA, boneB);
        text << "ragDoll.addConstraint " << boneA << " " << boneB << " "
             << static_cast<double>(it->m_length) << std::endl;
    }
    const std::string out = text.str();
    stream->write(out.data(), static_cast<unsigned int>(out.length()));
    file.close();
}

// Loads the reference skeleton once; a failed load deletes it again.
void RagDollTemplate::skeleton(const std::string& name)
{
    if (m_skeleton)
        return;
    bool loaded = false;
    Skeleton* const s = new Skeleton(name, &loaded, 0);
    m_skeleton = s;
    if (!loaded)
    {
        delete s;
        m_skeleton = 0;
    }
}

void RagDollTemplate::setParticleCollisionCriteria(float minImpact, float debounce)
{
    m_particleCollisionCriteria1 = minImpact;
    m_particleCollisionCriteria2 = debounce;
}

// Constraint serialization remains declaration-only; the console save binding stops visibly.
// [FRONTIER] [RETIRE-WHEN: ragdoll-write-constraints]

} // namespace anim
} // namespace dice
