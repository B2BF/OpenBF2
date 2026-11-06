// Rigid-body collision impulses, friction, effects, and terrain/object contact handling.
// [INFERRED] Source path.

#include "ResponsePhysics.h"

#include <float.h>
#include <math.h>
#include <stdint.h>

#include "../Core/Ids.h"
#include "../Core/StringUtil.h"                       // compareIgnoreCase - the two submarines
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"          // getName / getClassID
#include "../Game/Objects/ObjectLayoutAccess.h"       // +0x14 template, +0x40 mesh, +0x44 node, +0x28 parent
#include "../Game/Objects/Fx/ParticleTemplate.h"      // dice::hfe::Random - addEffect's timer
#include "../Game/Objects/EngineTemplate.h"           // the differential and the newCar2 block
#include "../Game/Objects/TemplateLeaves.h"           // SpringTemplate::m_rotateUV
#include "../Game/Objects/Game/Engine.h"              // the wheel's engine: feedbackLoop, setTrackHasContact
#include "../Game/Objects/Game/Spring.h"              // getEngine
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "PhysicsSpring.h"                            // the wheel node: m_wheelSpin, m_wheelAngle, getWheelRadius
#include "../Game/IGameLogic.h"                       // gameLogic->isServer - solveImpulse
#include "../Math/LocalBoundingBox.h"
#include "../Math/Mat4.h"
#include "../Math/Ray3.h"
#include "BasicPhysicsSystem.h"                       // gravity - the buoyancy arm
#include "CollisionManager.h"                        // optional checkObjectVsObject ray debug
#include "CollisionStats.h"
#include "HeightmapCluster.h"
#include "ICollisionMesh.h"
#include "IPhysicsNode.h"
#include "MaterialManager.h"
#include "../Sound/NullSound.h"                     // ISound - the sound instance getSound answers
#if !DEDICATED
// playCollisionEffect's client body (BF2.exe 0x6E5EF0).
#include "../Game/Main/Game.h"                      // isDedicated() - BF2.exe 0x440510
#include "../Game/Objects/Fx/EffectBundle.h"        // setCollisionPlane - BF2.exe 0x641BA0
#include "../Math/Plane3.h"
#endif

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;                       // ds:0x8ABAF24 - Core/Globals.cpp
extern IBase* materialManager;                        // ds:0x8AD2820 - Core/Globals.cpp

static inline void zero(Vec3& v) { v.x = v.y = v.z = 0.0f; }

static void initEffect(ResponsePhysics::EffectRecord& e)
{
    e.active = 0;
    e.id0 = -1;
    e.id1 = -1;
    e.sound = 0;
    e.speedSq = 0.0f;
    e.timer = 0.0f;
    zero(e.position);
    e.normal.x = 0.0f;    e.normal.y = 1.0f;     e.normal.z = 0.0f;
    e.direction.x = 0.0f; e.direction.y = -1.0f; e.direction.z = 0.0f;
}

// PointResponsePhysics ctor, then every field.
ResponsePhysics::ResponsePhysics()
{
    m_refCount = 1;
    zero(m_positionalAdjusts);
    zero(m_rootPositionalAdjusts);
    zero(m_speedAdjusts);
    zero(m_averageNormal);
    zero(m_averageSpeed);
    zero(m_averagePosition);
    m_contactCount = 0;
    m_sharedF4 = 0;
    m_averageFriction = 1.0f;
    m_averageElasticity = 0.0f;
    m_averageResistance = 0.0f;
    m_grip = 0;
    m_permanentGrip = 1;
    m_dwordF0 = 0;
    zero(m_intersectingTerrainPoint);
    m_shouldTestDeathMaterial = -1;
    m_isIntersectingTerrain = false;
    m_peakAdjustSq = 0.0f;
    m_pushLevel = -1;
    m_hardPush = 0.0f;
    // m_contacts: empty, by its own ctor
    initEffect(m_effect0);
    initEffect(m_effect1);
}

// the vector's storage (its own dtor here), the +0xF4 intrusive count, the base.
ResponsePhysics::~ResponsePhysics()
{
    if (m_sharedF4)
    {
        if ((*m_sharedF4)-- == 1)
            delete m_sharedF4;                       // [UNRESOLVED] pointee type - `operator delete(p)` in the ELF
    }
}

void ResponsePhysics::addRef()  { ++m_refCount; }
int  ResponsePhysics::getRef()  { return m_refCount; }
int  ResponsePhysics::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

void* ResponsePhysics::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)             return const_cast<ResponsePhysics*>(this);
    if (iid == IID_IResponsePhysics)  return const_cast<ResponsePhysics*>(this);
    return 0;
}


void ResponsePhysics::reset()
{
    zero(m_positionalAdjusts);
    zero(m_rootPositionalAdjusts);
    zero(m_speedAdjusts);
    m_contactCount = 0;
    zero(m_averageNormal);
    zero(m_averageSpeed);
    zero(m_averagePosition);
    m_contacts.resize(0);
}

unsigned int ResponsePhysics::getClassID() const              { return CID_ResponsePhysics; }
void ResponsePhysics::setCollisionChecked(unsigned int v)     { m_collisionChecked = v; }

bool ResponsePhysics::getIsIntersectingTerrain(Vec3& out) const
{
    out = m_intersectingTerrainPoint;
    return m_isIntersectingTerrain;
}

unsigned char ResponsePhysics::getPermanentGrip() const       { return m_permanentGrip; }
void ResponsePhysics::setPermanentGrip(unsigned char v)       { m_permanentGrip = v; }
const Vec3& ResponsePhysics::getPositionalAdjusts() const     { return m_positionalAdjusts; }

// the two countdowns - faster (4x) while there are no contacts.
void ResponsePhysics::updateEffect(float dt)
{
    if (m_effect0.timer > 0.0f)
    {
        const float t = m_effect0.timer - (m_contacts.empty() ? dt * 4.0f : dt);
        m_effect0.timer = t;
        if (t <= 0.0f)
        {
            m_effect0.speedSq = 0.0f;
            m_effect0.active = 0;
        }
    }
    if (m_effect1.timer > 0.0f)
    {
        const float t = m_effect1.timer - (m_contacts.empty() ? dt * 4.0f : dt);
        m_effect1.timer = t;
        if (t <= 0.0f)
        {
            m_effect1.speedSq = 0.0f;
            m_effect1.active = 0;
        }
    }
}

// Spawns a contact effect only on the client, aligning its Y axis to the contact normal when possible.
// EffectBundle receives the collision plane. The object is initialized and messaged, without retaining
// or releasing it here. The dedicated implementation is empty.
#if DEDICATED
void ResponsePhysics::playCollisionEffect(const Vec3&, const IObjectTemplate*, const Vec3&, const Vec3&) {}
#else
namespace {
// BF2.exe 0x4639B0 - the engine's conditional normalise: untouched within FLT_EPSILON (89E4E8) of
// unit length, zeroed within FLT_EPSILON of zero, else scaled by 1/sqrt. lenSq summed (x*x + y*y)
// + z*z and stored to a float before the tests, as the x87 sequence does.
inline void effectNormalise(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (fabsf(lengthSq - 1.0f) < FLT_EPSILON)
        return;
    if (fabsf(lengthSq) < FLT_EPSILON)
    {
        v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / sqrtf(lengthSq);
    v.x = inverse * v.x;
    v.y = inverse * v.y;
    v.z = inverse * v.z;
}

// BF2.exe 0x5CA350, 68 B - out = a x b (a in EDX, b on the stack), each term `b * a - b * a` as the
// x87 sequence orders it (the products commute exactly; the subtraction order is kept).
inline Vec3 effectCross(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = b.z * a.y - b.y * a.z;
    out.y = b.x * a.z - b.z * a.x;
    out.z = b.y * a.x - b.x * a.y;
    return out;
}

// BF2.exe 0x5A6BD0, 66 B - [INFERRED] Plane3(normal, point): the normal copied, d = -(point .
// normal) summed (p.z*n.z + p.y*n.y) + n.x*p.x. The same helper GameLogicCollision.cpp spells for
// the doImpact spawner; the tree's Plane3 is an aggregate without constructors.
inline Plane3 effectPlaneFrom(const Vec3& normal, const Vec3& point)
{
    Plane3 plane;
    plane.normal = normal;
    plane.d = -(point.z * normal.z + point.y * normal.y + normal.x * point.x);
    return plane;
}
} // namespace

void ResponsePhysics::playCollisionEffect(const Vec3& position, const IObjectTemplate* effectTemplate,
                                          const Vec3& normal, const Vec3& direction)
{
    if (isDedicated())
        return;
    if (!effectTemplate)
        return;
    IObject* const effect = effectTemplate->createObject();                   // +0x50
    if (!effect)
        return;

    if (effect->getTemplate()->getClassID() == CID_EffectBundleTemplate)       // 91C1E4 only
        static_cast<EffectBundle*>(effect)->setCollisionPlane(
            effectPlaneFrom(normal, position));

    effect->setAbsolutePosition(position);                                     // +0x6C
    Mat4 transform = effect->getParent() ? effect->getRelativeTransformation()     // +0x28 / +0xE4
                                         : effect->getAbsoluteTransformation();    // +0x80, 0x4531C0 copy

    // Summed (d.z*n.z + d.y*n.y) + n.x*d.x; `test ah, 5; jnp` skips the frame only on a strict
    // |dot| < 0.0001f (8EBDFC) - a NaN dot builds it.
    const float dot = direction.z * normal.z + direction.y * normal.y + normal.x * direction.x;
    if (!(fabsf(dot) < 0.000099999997f))
    {
        Vec3 y = normal;
        effectNormalise(y);
        Vec3 x = effectCross(direction, y);
        effectNormalise(x);
        const Vec3 z = effectCross(x, y);
        transform.m[0] = x.x; transform.m[1] = x.y; transform.m[2]  = x.z;
        transform.m[4] = y.x; transform.m[5] = y.y; transform.m[6]  = y.z;
        transform.m[8] = z.x; transform.m[9] = z.y; transform.m[10] = z.z;
    }

    if (effect->getParent())                                                   // +0x28, read again
        effect->setRelativeTransformation(transform);                          // +0xE8
    else
        effect->setAbsoluteTransformation(transform);                          // +0x88
    effect->init();                                                            // +0x1C
    effect->handleMessage(static_cast<TemplateMessage>(0), 0, false);          // +0x100
}
#endif

// Response tuning values. Flattened configuration pairs and descriptive names are [CHOSEN].
namespace {
float s_maxImpulseSq          = 1000.0f;
float s_frictionScale         = 1.0f;
float s_elasticityScale       = 1.0f;
int   s_contactsForHasContact = 2;
int   s_wheelAdjustAlongNormal = 1;
float s_hardPushAdjust        = 0.5f;
float s_hardPushScale         = 100.0f;
int   s_hardPushLevel         = 3;

// checkObjectVsObject's remaining TU knobs. The ELF stores each value beside a pointer to it;
// flattening that indirection is [CHOSEN], while every value and use below was read from
// 0x83668B0 and the TU initialiser at 0x836E610. PE 0x69B680 has the same gates and constants.
int   s_drawObjectCollisionLines = 0;
float s_collisionWakeMassFactor  = 1.0f;
float s_minImpulseShare          = 0.05f;
float s_equalShareNormalSpeed    = 10.0f;
int   s_rejectSeparatingHits     = 0;
int   s_collectMeshVertexStats   = 0;
int   s_enableEdgePushPass       = 1;
float s_edgePushScale            = 3.0f;
float s_edgeNormalFallback       = 0.1f;
int   s_smallStaticVertexLimit   = 20;
unsigned int s_meshVertexQueries = 0;
unsigned int s_meshQueryCount    = 0;

// This is checkObjectVsObject's distinct spring transform, not checkVsTerrain's 0x8ABAAC0 one.
// Its guard is 0x8ABAA38 and its Mat4 is 0x8ABAA60.
bool s_objectSpringTransformInitialised = false;
Mat4 s_objectSpringTransform;

// checkVsTerrain's function-local static: the transform a spring's hull is tested under - its
// root's orientation with the spring's own position. Guard byte 0x8ABAAB0, Mat4 0x8ABAAC0,
// identity on first use (the ELF's own lazy-init shape, reproduced).
bool s_springTransformInitialised = false;
Mat4 s_springTransform;
} // namespace

// internalFindFirstChildOfCID - / PE 0x698450. Despite its name, the shipped
// walk tests the current object, then its sibling chain, and only then its child. Objects carrying
// flag bit 0 are neither tested nor descended through, although their siblings are still walked.
// The two getClassID calls when neither CID matches are intentional.
IObject* internalFindFirstChildOfCID(IObject* object, unsigned int firstCid,
                                     unsigned int secondCid)
{
    IObject* found = 0;
    if (object)
    {
        const bool descend = (object->getFlags() & 1u) == 0;
        if (descend)
        {
            if (object->getTemplate()->getClassID() == firstCid)
                return object;
            // Retail's second CID is a stop sentinel, not a second positive match: EDX is
            // explicitly zeroed before this comparison at ELF 0x836FA16. getShould passes the
            // same CID twice, so its Engine match is taken by the first comparison.
            if (object->getTemplate()->getClassID() == secondCid)
                return 0;
        }

        found = internalFindFirstChildOfCID(object->getSibling(), firstCid, secondCid);
        if (!found && descend)
            found = internalFindFirstChildOfCID(object->getChild(), firstCid, secondCid);
    }
    return found;
}

// Caches the death-material decision only after an Engine object is available.
bool ResponsePhysics::getShouldTestDeathMaterial()
{
    int answer = m_shouldTestDeathMaterial;
    if (answer == -1)
    {
        IObject* const root = getRootParent(getObject());
        if (root)
        {
            IObject* const engine = internalFindFirstChildOfCID(
                root->getChild(), CID_EngineTemplate, CID_EngineTemplate);
            if (engine)
            {
                answer = static_cast<Engine*>(engine)->getEngineType() == 33;
                m_shouldTestDeathMaterial = answer;
            }
        }
        answer = m_shouldTestDeathMaterial;
    }
    return answer > 0;
}

// Combines adjustment components: zero takes value, matching signs keep the larger
// magnitude, and opposite signs add.
void setAdjust(float& accumulator, float value)
{
    const float current = accumulator;
    if (current == 0.0f)
    {
        accumulator = value;
        return;
    }
    if (current > 0.0f)
    {
        if (value > 0.0f)
        {
            if (!(value <= current))
                accumulator = value;
            return;
        }
        accumulator = value + current;
        return;
    }
    if (value >= 0.0f)
    {
        accumulator = value + current;
        return;
    }
    if (!(current <= value))
        accumulator = value;
}

// world::setAdjustVector(Vec3*, const Vec3*) - 0x8365470, 115 B: setAdjust per component, x y z.
void setAdjustVector(Vec3* accumulator, const Vec3* value)
{
    float x = accumulator->x;
    setAdjust(x, value->x);
    float y = accumulator->y;
    setAdjust(y, value->y);
    float z = accumulator->z;
    setAdjust(z, value->z);
    accumulator->x = x;
    accumulator->y = y;
    accumulator->z = z;
}

// Adds value to the running mean over count prior samples, snapping components below 1e-6 to zero.
void addAdjustVector(Vec3* accumulator, const Vec3* value, int count)
{
    const float n  = static_cast<float>(count);
    const float n1 = static_cast<float>(count + 1);
    float x = (accumulator->x * n + value->x) / n1;
    if (fabsf(x) < 0.000001f) x = 0.0f;
    float y = (accumulator->y * n + value->y) / n1;
    if (fabsf(y) < 0.000001f) y = 0.0f;
    float z = (n * accumulator->z + value->z) / n1;
    accumulator->x = x;
    accumulator->y = y;
    if (fabsf(z) < 0.000001f) z = 0.0f;
    accumulator->z = z;
}

// Records a contact and accumulates position/speed adjustments and mean material properties.
// Disabled responses ignore it; no object moves until solveImpulse. Negative depth pushes outward,
// and child responses inherit root positional adjustments.
void ResponsePhysics::impulseOn(const Vec3& position, const Vec3& speed, const Vec3& normal,
                                float depth, int ownMaterial, int otherMaterial)
{
    if (!m_enabled)
        return;

    MaterialManager* const materials = static_cast<MaterialManager*>(materialManager);
    const float closing = -(speed.x * normal.x + speed.y * normal.y + speed.z * normal.z);
    const float ownElasticity = materials->getElasticityForMaterial(ownMaterial);         // +0x70
    const float elasticity = 0.5f * (materials->getElasticityForMaterial(otherMaterial) + ownElasticity);
    const float ownFriction = materials->getFrictionForMaterial(ownMaterial);             // +0x6C
    const float friction = materials->getFrictionForMaterial(otherMaterial) + ownFriction;

    Contact contact;
    contact.position = position;
    contact.normal = normal;
    contact.negativeSpeed.x = -speed.x;
    contact.negativeSpeed.y = -speed.y;
    contact.negativeSpeed.z = -speed.z;
    contact.normalSpeed = closing;
    contact.elasticity = elasticity;
    contact.friction = friction * 0.5f;
    m_contacts.push_back(contact);

    // the normal component of -speed
    Vec3 speedAdjust;
    const float nn = normal.y * normal.y + normal.x * normal.x + normal.z * normal.z;
    if (fabsf(nn) > FLT_EPSILON)
    {
        const float k = (-speed.x * normal.x + -speed.y * normal.y + -speed.z * normal.z) / nn;
        speedAdjust.x = normal.x * k;
        speedAdjust.y = normal.y * k;
        speedAdjust.z = k * normal.z;
    }
    else
    {
        speedAdjust.x = speedAdjust.y = speedAdjust.z = 0.0f;
    }
    Vec3 positionalAdjust;
    positionalAdjust.x = normal.x * -depth;
    positionalAdjust.y = normal.y * -depth;
    positionalAdjust.z = normal.z * -depth;
    setAdjustVector(&m_positionalAdjusts, &positionalAdjust);
    setAdjustVector(&m_speedAdjusts, &speedAdjust);
    addAdjustVector(&m_averageNormal, &normal, m_contactCount);
    addAdjustVector(&m_averagePosition, &position, m_contactCount);
    addAdjustVector(&m_averageSpeed, &speed, m_contactCount);

    m_grip = static_cast<unsigned char>(getPermanentGrip() | (m_grip & 0x40));
    const float f0 = materials->getFrictionForMaterial(ownMaterial);
    m_averageFriction = (materials->getFrictionForMaterial(otherMaterial) + f0) * 0.5f;
    const float e0 = materials->getElasticityForMaterial(ownMaterial);
    m_averageElasticity = (materials->getElasticityForMaterial(otherMaterial) + e0) * 0.5f;
    const float r0 = materials->getResistanceForMaterial(ownMaterial);                    // +0x74
    m_averageResistance = (materials->getResistanceForMaterial(otherMaterial) + r0) * 0.5f;

    IResponsePhysics* const root = getRootResponse();                                     // +0x94
    if (root && root != this)
        m_rootPositionalAdjusts = root->getPositionalAdjusts();                            // +0x90
    ++m_contactCount;
}

// ResponsePhysics::addEffect(ownMaterial, otherMaterial, speed, speedSq, point, normal)
// , read in full. Books a collision effect for solveImpulse's
// tail to fire: with a sound for the material pair (MaterialManager::getSound(a, b, 0, true)),
// record 1 (+0x160) is the candidate - taken when inactive and this contact's speed^2 beats the
// one on record; without a sound, record 0 (+0x124) the same way. The record gets the two
// materials, the sound (record 1) or 0 (record 0), the speed^2, a timer of
// 0.5 + ((random >> 7) | 1) + 1) * 2^-24 seconds (dice::hfe::Random::getRaw - the inlined
// Fishman-Moore step), the point, the normal, and the speed normalised as its direction.
void ResponsePhysics::addEffect(int ownMaterial, int otherMaterial, Vec3 speed, float speedSq,
                                const Vec3& point, const Vec3& normal)
{
    void* const sound = static_cast<MaterialManager*>(materialManager)
        ->getSound(static_cast<unsigned int>(ownMaterial), static_cast<unsigned int>(otherMaterial), 0, true);   // +0x68
    EffectRecord* record;
    if (sound)
    {
        if (m_effect1.active)
            return;
        if (!(speedSq > m_effect1.speedSq))
            return;
        m_effect1.sound = sound;
        m_effect1.id0 = ownMaterial;
        m_effect1.active = 1;
        m_effect1.speedSq = speedSq;
        m_effect1.id1 = otherMaterial;
        record = &m_effect1;
    }
    else
    {
        if (m_effect0.active)
            return;
        if (!(speedSq > m_effect0.speedSq))
            return;
        m_effect0.id0 = ownMaterial;
        m_effect0.id1 = otherMaterial;
        m_effect0.active = 1;
        m_effect0.speedSq = speedSq;
        m_effect0.sound = 0;
        record = &m_effect0;
    }
    const unsigned int raw = Rand.getRaw();
    record->timer = static_cast<float>(static_cast<int>(((raw >> 7) | 1u) + 1u)) * 0.000000059604645f + 0.5f;
    record->position = point;
    record->normal = normal;
    const float inv = 1.0f / sqrtf(speedSq);
    record->direction.x = speed.x * inv;
    record->direction.y = speed.y * inv;
    record->direction.z = inv * speed.z;
}

// Collects hull terrain/water contacts, updates submersion and applies root-body buoyancy.
// Requires an enabled response, permitted collision group and LOD 1. dt is unused.
// Preserve the named-submarine water clamp, spring/root transform choice and material-7000 water
// test. Contact handling order affects effects, impulses and the averaged terrain-intersection point.
// Function-static scratch transforms make this path non-reentrant.
void ResponsePhysics::checkVsTerrain(float /*dt*/)
{
    typedef object_detail::LayoutAccess L;

    if (!m_enabled)
        return;
    if ((getCollisionGroups() & 1) != 0)                                  // +0x54
        return;
    if (!hasCollisionLod(1))                                              // +0x74
        return;

    IObject* const object = m_object;
    IHeightmapCluster* const cluster = static_cast<IHeightmapCluster*>(heightmapCluster);

    // 1. the two submarines
    const IObjectTemplate* const objectTemplate = L::objectTemplate(object);   // +0x14
    if (compareIgnoreCase(objectTemplate->getName().c_str(), "xpak_seascooter") == 0 ||
        compareIgnoreCase(L::objectTemplate(m_object)->getName().c_str(), "ve_sdv") == 0)
    {
        Vec3 p = m_object->getAbsolutePosition();                         // slot 27
        const float water = cluster->getWaterLevelInWorldCoords(p.x, p.z);   // slot 38
        if (p.y > water)
        {
            p.y = water;
            m_object->setAbsolutePosition(p);                             // slot 28
        }
    }

    // 2. never below the terrain
    Vec3 normal;
    normal.x = normal.y = normal.z = 0.0f;
    Vec3 adjustMean;
    adjustMean.x = adjustMean.y = adjustMean.z = 0.0f;
    const Vec3& position = object->getAbsolutePosition();
    {
        const float terrain = cluster->getHeightInWorldCoords(position);  // slot 28
        if (0.0f > position.y - terrain)
        {
            Vec3 lifted;
            lifted.x = position.x;
            lifted.y = terrain;
            lifted.z = position.z;
            object->setAbsolutePosition(lifted);
        }
    }

    IPhysicsNode* const node = static_cast<IPhysicsNode*>(L::physicsNode(object));   // +0x44
    IPhysicsNode* const rootNode = node->getRootNode();                   // +0x150
    float lowestY = 3.4028235e38f;
    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(L::collisionMesh(object));   // +0x40
    const int vertexCount = mesh->getVertexCount(1);                      // slot 16
    if (!vertexCount)
        return;
    ++collisionStatsData->unknown04;                                      // +0x04
    bool testVertices = true;

    // 4. the prefilter
    if (vertexCount > 10)
    {
        const LocalBoundingBox* const box =
            static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(1));   // slot 22
        float extent = fabsf(box->m_max.x);
        if (fabsf(box->m_min.x) > extent)
            extent = fabsf(box->m_min.x);
        float ey = fabsf(box->m_min.y);
        if (ey <= fabsf(box->m_max.y))
            ey = fabsf(box->m_max.y);
        if (ey > extent)
            extent = ey;
        const float reach = extent * 1.4299999f;

        const Mat4& t1 = L::parent(object) ? object->getRelativeTransformation()   // slot 58
                                           : object->getAbsoluteTransformation(); // slot 33
        Vec3 front;
        front.x = t1.m[8] * box->m_max.z + position.x;
        front.y = t1.m[9] * box->m_max.z + position.y;
        front.z = box->m_max.z * t1.m[10] + position.z;
        const Mat4& t2 = L::parent(object) ? object->getRelativeTransformation()
                                           : object->getAbsoluteTransformation();
        Vec3 back;
        back.x = t2.m[8] * box->m_min.z + position.x;
        back.y = t2.m[9] * box->m_min.z + position.y;
        back.z = box->m_min.z * t2.m[10] + position.z;
        unsigned char material = 0;
        if (front.y - reach
                - cluster->getHeightAndNormalInWorldCoords(front.x, front.z, normal, material) > 0.0f)   // slot 33
        {
            if (back.y - reach
                    - cluster->getHeightAndNormalInWorldCoords(back.x, back.z, normal, material) > 0.0f)
                testVertices = false;
        }
    }

    bool intersecting = false;
    if (!s_springTransformInitialised)
    {
        s_springTransform = Mat4::getIdentity();
        s_springTransformInitialised = true;
    }

    // 5. the transform
    const float* m;
    if (L::objectTemplate(object)->getClassID() == CID_SpringTemplate)  // +0x10
    {
        const Mat4& rootTransform = getRootParent(object)->getAbsoluteTransformation();
        s_springTransform.m[0]  = rootTransform.m[0];
        s_springTransform.m[1]  = rootTransform.m[1];
        s_springTransform.m[2]  = rootTransform.m[2];
        s_springTransform.m[4]  = rootTransform.m[4];
        s_springTransform.m[5]  = rootTransform.m[5];
        s_springTransform.m[6]  = rootTransform.m[6];
        s_springTransform.m[8]  = rootTransform.m[8];
        s_springTransform.m[9]  = rootTransform.m[9];
        s_springTransform.m[10] = rootTransform.m[10];
        s_springTransform.m[12] = position.x;
        s_springTransform.m[13] = position.y;
        s_springTransform.m[14] = position.z;
        m = s_springTransform.m;
    }
    else
    {
        m = object->getAbsoluteTransformation().m;
    }

    // 6. the vertices
    int material = 0;
    int contactCount = 0;
    const float* vertices = static_cast<const float*>(mesh->getVertexVector(1));   // slot 17
    for (int i = 0; i < vertexCount; ++i)
    {
        const float* const v = vertices + i * 3;
        Vec3 world;
        world.x = m[4] * v[1] + m[0] * v[0] + m[8]  * v[2] + position.x;
        world.y = m[9] * v[2] + (m[5] * v[1] + m[1] * v[0]) + position.y;
        world.z = v[2] * m[10] + v[1] * m[6] + v[0] * m[2] + position.z;

        if (testVertices)
        {
            bool handled = false;
            material = mesh->getMaterialForVertex(1, i) & 0xFFFF;         // slot 18, a u16
            unsigned char surfaceMaterial = 0;
            float surface = cluster->getHeightAndNormalInWorldCoords(world.x, world.z, normal,
                                                                     surfaceMaterial);
            if (material == 7000)
            {
                const float water = cluster->getWaterLevelInWorldCoords(world.x, world.z);
                if (water > surface)
                {
                    surface = water;
                    surfaceMaterial = 1;
                }
            }
            const float depth = world.y - surface;
            if (!(0.0f < depth) && (vertexCount > 3 || -depth > m_positionalAdjusts.y))
            {
                intersecting = true;
                Vec3 point;
                point.x = world.x;
                point.y = surface;
                point.z = world.z;
                Vec3 relative;
                relative.x = world.x - position.x;
                relative.y = surface - position.y;
                relative.z = world.z - position.z;

                Vec3 speed = rootNode->getTangentSpeed(point);            // +0xA4
                const float speedSq = speed.y * speed.y + speed.x * speed.x + speed.z * speed.z;
                if (speedSq > node->getHandleCollisionSpeed())           // +0x158
                {
                    handled = true;
                    object->handleCollision(0, speed, normal, relative, material,
                                            surfaceMaterial, 0, 0);     // slot 45
                }
                rootNode->setIsInAir(false);                              // +0x1AC
                if (!handled)
                    material = mesh->getMaterialForVertex(1, i) & 0xFFFF;
                const float speedSq2 = speed.x * speed.x + speed.y * speed.y + speed.z * speed.z;
                const float hcs = node->getHandleCollisionSpeed();
                if (speedSq2 >= hcs * hcs)
                    addEffect(material, surfaceMaterial, speed, speedSq2, point, normal);
                impulseOn(relative, speed, normal, depth, material, surfaceMaterial);   // +0x30
                addAdjustVector(&adjustMean, &point, contactCount);
                ++contactCount;
            }
        }
        if (lowestY > world.y)
            lowestY = world.y;
    }

    // 7. the water
    const float water = cluster->getWaterLevelInWorldCoords(position.x, position.z);
    const float lowestAboveWater = lowestY - water;
    const float floaterMod = rootNode->getFloaterMod();                   // +0x168
    if (lowestAboveWater >= 0.0f || material == 7000)
    {
        node->setUnderWater(0.0f);                                        // +0x108
    }
    else
    {
        if (0.0f >= node->getUnderWater())                                // +0x10C - entering
        {
            const int firstMaterial = mesh->getMaterialForVertex(1, 0) & 0xFFFF;
            Vec3 penetration;
            penetration.x = 0.0f;
            penetration.y = water - position.y;
            penetration.z = 0.0f;
            Vec3 up;
            up.x = 0.0f;
            up.y = 1.0f;
            up.z = 0.0f;
            object->handleCollision(0, rootNode->getPositionalSpeed(), up, penetration,
                                    firstMaterial, 1, 0, 0);
        }
        float depth = -lowestAboveWater;
        if (!(depth < 1.0f))
            depth = 1.0f;
        node->setUnderWater(depth);
        node->setIsInAir(false);

        if (node == rootNode && fabsf(floaterMod - 0.0f) > FLT_EPSILON)
        {
            const LocalBoundingBox* const box =
                static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(1));
            const float sizeX = box->m_max.x - box->m_min.x;
            const float sizeY = box->m_max.y - box->m_min.y;
            const float sizeZ = box->m_max.z - box->m_min.z;
            const float upX = fabsf(m[1]);
            const float upY = fabsf(m[5]);
            const float upZ = fabsf(m[9]);
            const Mat4& inv = m_object->getInvAbsoluteTransformation();   // slot 34
            const float lx = inv.m[4];                                    // world up, locally
            const float ly = inv.m[5];
            const float lz = inv.m[6];
            const float aboveWater = position.y - water;
            const float gravity = static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity();   // +0x20
            const float g = node->getGravityModifier() * gravity;         // +0xFC

            float total = 0.0f;
            float maxWeight = 0.0f;
            Vec3 mean;
            mean.x = mean.y = mean.z = 0.0f;
            for (int corner = 0; corner <= 7; ++corner)
            {
                Vec3 c;
                c.x = (corner & 4) ? box->m_max.x : box->m_min.x;
                c.y = (corner & 2) ? box->m_max.y : box->m_min.y;
                c.z = (corner & 1) ? box->m_max.z : box->m_min.z;
                const float h = c.x * lx + c.y * ly + lz * c.z + aboveWater;
                if (0.0f < h)
                    continue;
                const float d = -h;
                float a = d / sizeX;
                if (a >= 1.0f) a = 1.0f;
                float b = d / sizeY;
                if (b >= 1.0f) b = 1.0f;
                float cc = 1.0f;
                if (d / sizeZ < 1.0f) cc = d / sizeZ;
                const float w = cc * upZ + (b * upY + a * upX);
                if (maxWeight <= w)
                    maxWeight = w;
                total = total + w;
                mean.x = c.x * w + mean.x;
                mean.y = c.y * w + mean.y;
                mean.z = c.z * w + mean.z;
            }
            if (total > 0.0099999998f)
            {
                const float inv2 = 1.0f / total;
                const float cx = mean.x * inv2;
                const float cy = mean.y * inv2;
                const float cz = inv2 * mean.z;
                Vec3 at;
                at.x = m[4] * cy + m[0] * cx + m[8] * cz;
                at.y = m[9] * cz + (m[5] * cy + m[1] * cx);
                at.z = cz * m[10] + (cy * m[6] + cx * m[2]);
                Vec3 accel;
                accel.x = 0.0f;
                accel.y = -g * (maxWeight * floaterMod);
                accel.z = 0.0f;
                node->addAccelerationAtRelativePosition(at, accel);       // +0x94
            }
        }
    }

    // 8.
    m_isIntersectingTerrain = intersecting;
    m_intersectingTerrainPoint = adjustMean;
}

// Applies accumulated contacts and positional correction, then clears the contact state.
// Spring wheels only correct position; their spring supplies impulses. Child corrections move the root.
// Preserve hard-push accounting, impulse weighting and effect-record selection, including the second
// effect arm's writes to record zero. [PLATFORM] call the two-argument ISound::playAt through its
// typed interface: overload slot order differs between MSVC and Itanium.
void ResponsePhysics::solveImpulse(IPhysicsNode* node)
{
    typedef object_detail::LayoutAccess L;

    if (!m_enabled)
        return;

    m_hardPush = 0.0f;
    int level = m_pushLevel - 1;
    m_pushLevel = level;
    int clamped = -1;
    if (level >= -1)
    {
        clamped = level;
        if (level >= 4)
            clamped = 3;
    }
    m_pushLevel = clamped;

    if (m_contactCount > 0 &&
        L::objectTemplate(getObject())->getClassID() == CID_SpringTemplate)
    {
        // 1. a wheel
        const float nn = m_averageNormal.y * m_averageNormal.y + m_averageNormal.x * m_averageNormal.x
                       + m_averageNormal.z * m_averageNormal.z;
        if (fabsf(nn - 1.0f) >= FLT_EPSILON)
        {
            if (fabsf(nn) < FLT_EPSILON)
            {
                m_averageNormal.x = 0.0f;
                m_averageNormal.y = 1.0f;
                m_averageNormal.z = 0.0f;
            }
            else
            {
                const float inv = 1.0f / sqrtf(nn);
                m_averageNormal.x = inv * m_averageNormal.x;
                m_averageNormal.y = m_averageNormal.y * inv;
                m_averageNormal.z = m_averageNormal.z * inv;
            }
        }
        const float* dir;
        if (s_wheelAdjustAlongNormal == 1)
            dir = &m_averageNormal.x;
        else
            dir = getRootParent(m_object)->getAbsoluteTransformation().m + 4;   // row 1: up
        float t = m_positionalAdjusts.y * dir[1] + m_positionalAdjusts.x * dir[0] + m_positionalAdjusts.z * dir[2]
                - (m_rootPositionalAdjusts.x * dir[0] + m_rootPositionalAdjusts.y * dir[1] + m_rootPositionalAdjusts.z * dir[2]);
        if (!(1.0f > t))
            t = 1.0f;
        if (t <= 0.0f)
            t = 0.0f;
        Vec3 adjust;
        adjust.x = dir[0] * t;
        adjust.y = dir[1] * t;
        adjust.z = dir[2] * t;
        if (adjust.x != 0.0f || adjust.y != 0.0f || adjust.z != 0.0f)
        {
            const Vec3& p = node->getAbsolutePosition();                  // +0x48
            Vec3 moved;
            moved.x = p.x + adjust.x;
            moved.y = p.y + adjust.y;
            moved.z = p.z + adjust.z;
            node->setAbsolutePosition(moved);                             // +0x4C
        }
    }
    else
    {
        // 2. a hull
        Vec3 adjust = m_positionalAdjusts;
        if (!(adjust.x == 0.0f && adjust.z == 0.0f && adjust.y == 0.0f))
        {
            if (gameLogic->isServer() == true)
            {
                const float adjustSq = adjust.x * adjust.x + adjust.y * adjust.y + adjust.z * adjust.z;
                if (adjustSq > s_hardPushAdjust * s_hardPushAdjust)
                    m_pushLevel += 2;
                float peak = m_peakAdjustSq;
                if (m_pushLevel >= s_hardPushLevel)
                {
                    float p = peak;
                    if (peak <= adjustSq)
                        p = adjustSq;
                    m_hardPush = (sqrtf(p) - s_hardPushAdjust) * s_hardPushScale;
                }
                if (peak <= adjustSq)
                    peak = adjustSq;
                m_peakAdjustSq = peak;
            }
            if (getObject() != getRootParent(getObject()))
                node = static_cast<IPhysicsNode*>(L::physicsNode(getRootParent(getObject())));
            const Vec3& p = node->getAbsolutePosition();
            Vec3 moved;
            moved.x = p.x + adjust.x;
            moved.y = p.y + adjust.y;
            moved.z = p.z + adjust.z;
            node->setAbsolutePosition(moved);

            const int count = static_cast<int>(m_contacts.size());
            float total = 0.0f;
            for (int i = 0; i < count; ++i)
            {
                float w = m_contacts[i].normalSpeed;
                if (w <= 0.0f)
                    w = 0.0f;
                total = w + total;
            }
            float inv = total;
            if (total > 0.000099999997f)
                inv = 1.0f / total;

            for (int i = 0; i < count; ++i)
            {
                const Contact& c = m_contacts[i];
                float w = c.normalSpeed;
                if (w <= 0.0f)
                    w = 0.0f;
                Vec3 v;
                v.x = c.negativeSpeed.x * w * inv;
                v.y = c.negativeSpeed.y * w * inv;
                v.z = c.negativeSpeed.z * w * inv;
                const float s = w * c.normalSpeed * inv;
                Vec3 tangent;
                tangent.x = v.x - c.normal.x * s;
                tangent.y = v.y - c.normal.y * s;
                tangent.z = v.z - c.normal.z * s;
                const float tangentSq = tangent.z * tangent.z + tangent.y * tangent.y + tangent.x * tangent.x;
                float friction = c.friction;
                if (tangentSq <= 0.001f)
                {
                    tangent.x = tangent.y = tangent.z = 0.0f;
                }
                else
                {
                    const float it = 1.0f / sqrtf(tangentSq);
                    tangent.x = tangent.x * it;
                    tangent.y = tangent.y * it;
                    tangent.z = it * tangent.z;
                    Vec3 vn = v;
                    const float vv = v.z * v.z + v.y * v.y + v.x * v.x;
                    if (fabsf(vv - 1.0f) >= FLT_EPSILON)
                    {
                        if (fabsf(vv) >= FLT_EPSILON)
                        {
                            const float iv = 1.0f / sqrtf(vv);
                            vn.x = vn.x * iv;
                            vn.y = vn.y * iv;
                            vn.z = iv * vn.z;
                        }
                        else
                        {
                            vn.x = vn.y = vn.z = 0.0f;
                        }
                    }
                    const float d = c.normal.y * vn.y + c.normal.x * vn.x + c.normal.z * vn.z;
                    friction = (1.0f - d * (d * d)) * friction * s_frictionScale;
                }
                const float scale = s * (c.elasticity * s_elasticityScale + 1.0f);
                Vec3 impulse;
                impulse.x = (friction * tangent.x + c.normal.x) * scale;
                impulse.y = (friction * tangent.y + c.normal.y) * scale;
                impulse.z = scale * (friction * tangent.z + c.normal.z);
                const float impulseSq = impulse.y * impulse.y + impulse.x * impulse.x + impulse.z * impulse.z;
                if (impulseSq > s_maxImpulseSq * s_maxImpulseSq)
                {
                    const float k = sqrtf(s_maxImpulseSq / impulseSq);
                    impulse.x = impulse.x * k;
                    impulse.y = impulse.y * k;
                    impulse.z = k * impulse.z;
                }
                node->addRelativeImpulse(c.position, impulse);            // +0x160
            }
            if (node->getShouldGravityCompensateOnContact() && count >= s_contactsForHasContact)   // +0x1A0
                node->setHasContact(true);                                // +0x1A4
            m_contactCount = 0;
            m_averageNormal.x = m_averageNormal.y = m_averageNormal.z = 0.0f;
            m_averageSpeed.x = m_averageSpeed.y = m_averageSpeed.z = 0.0f;
            m_averagePosition.x = m_averagePosition.y = m_averagePosition.z = 0.0f;
        }
    }

    // 3.
    m_positionalAdjusts.x = m_positionalAdjusts.y = m_positionalAdjusts.z = 0.0f;
    m_rootPositionalAdjusts.x = m_rootPositionalAdjusts.y = m_rootPositionalAdjusts.z = 0.0f;
    m_speedAdjusts.x = m_speedAdjusts.y = m_speedAdjusts.z = 0.0f;
    m_contacts.resize(0);

    // 4. the effects
    // (fx_impact): the template the test fetched is what is handed on, not
    // a null. ELF 0x83663CD..0x8366402 keeps getEffectTemplate's eax in edx and stores it as the
    // second argument ([esp+8]); BF2.exe 0x6E73DA..0x6E73F9 pushes the same eax. The literal 0 was
    // invisible while the callee was the server's empty body - the client body spawns it.
    MaterialManager* const materials = static_cast<MaterialManager*>(materialManager);
    if (m_effect0.active == 1)
    {
        const IObjectTemplate* const effectTemplate = static_cast<const IObjectTemplate*>(
            materials->getEffectTemplate(static_cast<unsigned int>(m_effect0.id0),
                                         static_cast<unsigned int>(m_effect0.id1), 0));   // +0x5C
        if (effectTemplate)
        {
            playCollisionEffect(m_effect0.position, effectTemplate, m_effect0.normal,
                                m_effect0.direction);                           // BF2.exe 0x6E5EF0
            m_effect0.active = 2;
        }
    }
    if (m_effect1.active == 1)
    {
        const int id0 = m_effect1.id0;
        if (id0 != 61 && id0 != 67)
        {
            ISound* const sound = static_cast<ISound*>(
                materials->getSound(static_cast<unsigned int>(id0),
                                    static_cast<unsigned int>(m_effect1.id1), 0, true));
            if (sound)
            {
                sound->setSoundFlags(1);                                  // slot +0x08 on both ports
                Vec3 at;
                at.x = m_effect1.normal.x * 0.050000001f + m_effect1.position.x;
                at.y = m_effect1.normal.y * 0.050000001f + m_effect1.position.y;
                at.z = 0.050000001f * m_effect1.normal.z + m_effect1.position.z;
                sound->playAt(at, 1);                                     // ELF +0x1C, PE +0x18
                m_effect1.active = 2;
            }
            // The same correction: the template goes on (ELF 0x8366351..0x8366393 edx -> [esp+8];
            // BF2.exe 0x6E74AC..0x6E74E3 re-reads id0/id1 from the record and pushes eax).
            const IObjectTemplate* const effectTemplate = static_cast<const IObjectTemplate*>(
                materials->getEffectTemplate(static_cast<unsigned int>(id0),
                                             static_cast<unsigned int>(m_effect1.id1), 0));
            if (effectTemplate)
            {
                playCollisionEffect(m_effect1.position, effectTemplate, m_effect1.normal,
                                    m_effect1.direction);
                m_effect0.active = 2;                                     // as shipped
            }
        }
    }
}

// Friction tuning values; flattened pointer pairs and descriptive names are [CHOSEN].
namespace {
float s_slidingFrictionFactor   = 0.80000001f;
float s_grippingFrictionFactor  = 1.2f;
float s_carFrictionScale        = 1.0f;
float s_frictionForceScale      = 1.0f;
int   s_normalPowerExtra        = 3;
float s_lateralAmplification    = 5.0f;
float s_uphillDriveReduction    = 1.0f;
float s_slopeDriveGain          = 1.0f;
int   s_normaliseSlipBySpeed    = 0;
float s_firstGearSlopeTorqueMod = 0.0f;
bool  s_projectTrackForce       = false;

// The GCC-inlined Vec3::normalize the friction bodies use: untouched within FLT_EPSILON of unit
// length, zero within FLT_EPSILON of zero length.
inline void normalizeInPlace(float& x, float& y, float& z)
{
    const float lengthSq = y * y + x * x + z * z;
    if (fabsf(lengthSq - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(lengthSq) >= FLT_EPSILON)
        {
            const float inv = 1.0f / sqrtf(lengthSq);
            x = x * inv;
            y = y * inv;
            z = inv * z;
        }
        else
        {
            x = y = z = 0.0f;
        }
    }
}

// The component of v along n - zero when n is degenerate (|n|^2 within FLT_EPSILON of zero).
inline void alongNormal(const Vec3& v, const Vec3& n, Vec3& out)
{
    const float nn = n.y * n.y + n.x * n.x + n.z * n.z;
    if (FLT_EPSILON < fabsf(nn))
    {
        const float k = (v.z * n.z + v.y * n.y + v.x * n.x) / nn;
        out.x = n.x * k;
        out.y = n.y * k;
        out.z = n.z * k;
    }
    else
    {
        out.x = out.y = out.z = 0.0f;
    }
}

// Scale v down onto the sphere of radius |limit| when it is longer than that and not negligible.
// The two-test shape every clamp in addFriction uses; returns whether it scaled.
inline bool clampLength(Vec3& v, float limit)
{
    const float lengthSq = v.z * v.z + v.y * v.y + v.x * v.x;
    if (lengthSq <= limit * limit || lengthSq <= 0.0000010000001f)
        return false;
    const float k = fabsf(limit) / sqrtf(lengthSq);
    v.x = v.x * k;
    v.y = v.y * k;
    v.z = k * v.z;
    return true;
}

inline Vec3 collisionAdd(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = a.x + b.x;
    out.y = a.y + b.y;
    out.z = a.z + b.z;
    return out;
}

inline Vec3 collisionSubtract(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = a.x - b.x;
    out.y = a.y - b.y;
    out.z = a.z - b.z;
    return out;
}

inline Vec3 collisionScale(const Vec3& v, float scale)
{
    Vec3 out;
    out.x = v.x * scale;
    out.y = v.y * scale;
    out.z = scale * v.z;
    return out;
}

inline float collisionDot(const Vec3& a, const Vec3& b)
{
    return a.z * b.z + a.y * b.y + a.x * b.x;
}

inline float collisionLengthSq(const Vec3& v)
{
    return v.z * v.z + v.y * v.y + v.x * v.x;
}

inline Vec3 collisionRotate(const Mat4& transform, const Vec3& v)
{
    Vec3 out;
    out.x = transform.m[4] * v.y + transform.m[0] * v.x + transform.m[8] * v.z;
    out.y = transform.m[9] * v.z + transform.m[5] * v.y + transform.m[1] * v.x;
    out.z = v.z * transform.m[10] + v.y * transform.m[6] + v.x * transform.m[2];
    return out;
}

inline Vec3 collisionTransformPoint(const Mat4& transform, const Vec3& v)
{
    Vec3 out = collisionRotate(transform, v);
    out.x = out.x + transform.m[12];
    out.y = out.y + transform.m[13];
    out.z = out.z + transform.m[14];
    return out;
}

inline Vec3 collisionTranslation(const Mat4& transform)
{
    Vec3 out;
    out.x = transform.m[12];
    out.y = transform.m[13];
    out.z = transform.m[14];
    return out;
}

inline Vec3 collisionRow(const Mat4& transform, int row)
{
    Vec3 out;
    const int offset = row * 4;
    out.x = transform.m[offset];
    out.y = transform.m[offset + 1];
    out.z = transform.m[offset + 2];
    return out;
}

inline void collisionNormalize(Vec3& v)
{
    normalizeInPlace(v.x, v.y, v.z);
}

// Store exactly the one-word WeakPtr control block used by ResponsePhysics::m_sharedF4. The
// temporary returned by getWeakPtr owns one reference; retain before replacing, then its dtor
// drops that temporary reference, matching the ELF's explicit inc / old dec / temporary dec.
inline void collisionAssignWeak(int*& destination, IObject* object)
{
    WeakPtr<IObject> weak = object->getWeakPtr();
    WeakPtr<IObject>::Ctrl* const incoming = weak.m_ctrl;
    if (incoming)
        ++incoming->refCount;
    if (destination && (*destination)-- == 1)
        ::operator delete(destination);
    destination = reinterpret_cast<int*>(incoming);
}

inline float collisionClampedShare(float share)
{
    if (share < s_minImpulseShare)
        return s_minImpulseShare;
    const float maximum = 1.0f - s_minImpulseShare;
    if (share > maximum)
        return maximum;
    return share;
}
} // namespace

// Runs mesh vertex, edge and tree-face tests after broad-phase rejection, then queues both impulses.
// Preserve mobile LOD selection, server death-material rays, sibling-first engine search, repeated
// virtual calls and same-order material arguments. The 16-byte face payload remains opaque.
void ResponsePhysics::checkObjectVsObject(IObject* object, IObject* other, float dt,
                                          bool treePassOnly)
{
    typedef object_detail::LayoutAccess L;

    if (!m_enabled)
        return;

    IResponsePhysics* const response =
        static_cast<IResponsePhysics*>(L::responsePhysics(object));
    IResponsePhysics* const otherResponse =
        static_cast<IResponsePhysics*>(L::responsePhysics(other));
    if (otherResponse->getClassID() != CID_ResponsePhysics
        && otherResponse->getClassID() != CID_StaticResponsePhysics)
        return;
    if (!object->hasCollisionLod(1))
        return;

    IPhysicsNode* const rootNode =
        static_cast<IPhysicsNode*>(L::physicsNode(getRootParent(object)));
    IPhysicsNode* const otherRootNode =
        static_cast<IPhysicsNode*>(L::physicsNode(getRootParent(other)));
    IPhysicsNode* const otherNode = static_cast<IPhysicsNode*>(L::physicsNode(other));

    int otherLod = rootNode->getOtherCollisionLod();
    if (otherLod < 0)
        otherLod = 1;
    if (otherLod == 0 && (otherRootNode->getIsMobile() == true || otherNode->getIsMobile() == true))
        otherLod = 2;
    if (!other->hasCollisionLod(otherLod))
        return;

    const Mat4& otherTransform = other->getAbsoluteTransformation();
    const Mat4& otherInverse = other->getInvAbsoluteTransformation();

    if (!s_objectSpringTransformInitialised)
    {
        s_objectSpringTransform = Mat4::getIdentity();
        s_objectSpringTransformInitialised = true;
    }

    const unsigned int objectTemplateCid = object->getTemplate()->getClassID();
    const Mat4* objectTransform;
    if (objectTemplateCid == CID_SpringTemplate)
    {
        const Mat4& rootTransform = getRootParent(object)->getAbsoluteTransformation();
        s_objectSpringTransform.m[0] = rootTransform.m[0];
        s_objectSpringTransform.m[1] = rootTransform.m[1];
        s_objectSpringTransform.m[2] = rootTransform.m[2];
        s_objectSpringTransform.m[4] = rootTransform.m[4];
        s_objectSpringTransform.m[5] = rootTransform.m[5];
        s_objectSpringTransform.m[6] = rootTransform.m[6];
        s_objectSpringTransform.m[8] = rootTransform.m[8];
        s_objectSpringTransform.m[9] = rootTransform.m[9];
        s_objectSpringTransform.m[10] = rootTransform.m[10];
        const Vec3& position = object->getAbsolutePosition();
        s_objectSpringTransform.m[12] = position.x;
        s_objectSpringTransform.m[13] = position.y;
        s_objectSpringTransform.m[14] = position.z;
        objectTransform = &s_objectSpringTransform;
    }
    else
    {
        objectTransform = &object->getAbsoluteTransformation();
    }

    const bool responseIsStatic = response->getClassID() == CID_StaticResponsePhysics;
    const Vec3& rootVelocity = rootNode->getLinearIntegrationVelocity();
    const Vec3& otherRootVelocity = otherRootNode->getLinearIntegrationVelocity();
    Vec3 motion;
    motion.x = (rootVelocity.x - otherRootVelocity.x) * dt;
    motion.y = (rootVelocity.y - otherRootVelocity.y) * dt;
    motion.z = (rootVelocity.z - otherRootVelocity.z) * dt;

    const Vec3 rotatedCollisionOffset =
        collisionRotate(*objectTransform, rootNode->getCenterOfCollisionOffset());
    motion.x = motion.x - rotatedCollisionOffset.x;
    motion.y = motion.y - rotatedCollisionOffset.y;
    motion.z = motion.z - rotatedCollisionOffset.z;
    if (!responseIsStatic)
    {
        const Vec3& objectPosition = object->getAbsolutePosition();
        const Vec3& rootPosition = rootNode->getAbsolutePosition();
        motion.x = objectPosition.x - rootPosition.x + motion.x;
        motion.y = objectPosition.y - rootPosition.y + motion.y;
        motion.z = objectPosition.z - rootPosition.z + motion.z;
    }

    const Vec3& objectPosition = object->getAbsolutePosition();
    Vec3 previousPosition;
    previousPosition.x = objectPosition.x - motion.x;
    previousPosition.y = objectPosition.y - motion.y;
    previousPosition.z = objectPosition.z - motion.z;

    ICollisionMesh* const mesh = static_cast<ICollisionMesh*>(L::collisionMesh(object));
    unsigned int vertexCount = mesh->getVertexCount(1);
    if (!vertexCount)
        return;
    if (s_collectMeshVertexStats == 1)
    {
        ++s_meshQueryCount;
        s_meshVertexQueries += vertexCount;
    }

    IPhysicsNode* const objectNode = static_cast<IPhysicsNode*>(L::physicsNode(object));
    ICollisionMesh* const otherMesh = static_cast<ICollisionMesh*>(L::collisionMesh(other));
    Vec3 normal;
    normal.x = normal.y = normal.z = 0.0f;
    Vec3 hitPosition;
    hitPosition.x = hitPosition.y = hitPosition.z = 0.0f;
    float planeDistance;
    float alongDistance;
    int otherMaterial = 0;
    unsigned int faceInfoStorage[4];
    faceInfoStorage[0] = 0;                         // the other three dwords are not initialised
    CollisionFaceInfo* const faceInfo =
        reinterpret_cast<CollisionFaceInfo*>(faceInfoStorage);

    // The retail tiny-object correction occurs before the frame collision counter and changes
    // both this test's relative motion and the object's absolute position.
    if (objectNode->getIsMobile()
        && mesh->getBoundingRadius(1) < 0.30000001f
        && otherMesh->getDistance(otherLod, false, otherTransform, otherInverse,
                                  previousPosition, motion, normal, hitPosition,
                                  planeDistance, alongDistance, &otherMaterial, faceInfo,
                                  true, false))
    {
        Vec3 correctionDirection = motion;
        collisionNormalize(correctionDirection);
        Vec3 correction = collisionScale(correctionDirection, -alongDistance);
        motion = collisionSubtract(motion, correction);
        object->setAbsolutePosition(collisionSubtract(object->getAbsolutePosition(), correction));
    }

    ++collisionStatsData->unknown00;
    const bool otherResponseIsStatic =
        otherResponse->getClassID() == CID_StaticResponsePhysics;
    bool isParticle = false;
    if (objectTemplateCid == CID_ParticleTemplate
        || other->getTemplate()->getClassID() == CID_ParticleTemplate)
        isParticle = true;

    IResponsePhysics* const rootResponse = response->getRootResponse();
    IResponsePhysics* const otherRootResponse = otherResponse->getRootResponse();
    const unsigned int rootResponseCid = rootResponse->getClassID();
    const unsigned int otherRootResponseCid = otherRootResponse->getClassID();

    bool playerControlAgainstStatic = false;
    if (otherRootResponseCid == CID_StaticResponsePhysics && objectNode == rootNode)
    {
        IObject* const rootObject = getRootParent(object);
        playerControlAgainstStatic =
            rootObject->queryInterface(IID_IPlayerControlObject) != 0;
    }

    if (playerControlAgainstStatic && gameLogic->isServer() == true
        && getShouldTestDeathMaterial())
    {
        const Mat4& previousTransform = objectNode->getPrevTransformation();
        Vec3 start;
        start.x = previousTransform.m[12];
        start.y = previousTransform.m[13];
        start.z = previousTransform.m[14];
        Vec3 direction = collisionSubtract(object->getAbsolutePosition(), start);
        if (otherMesh->getDistance(otherLod, false, otherTransform, otherInverse,
                                   start, direction, normal, hitPosition,
                                   planeDistance, alongDistance, &otherMaterial, faceInfo,
                                   true, false) == true)
        {
            if (otherResponse->getClassID() != CID_StaticResponsePhysics
                && otherRootNode->getMass() > rootNode->getMass())
                collisionAssignWeak(m_sharedF4, other);

            const Vec3& zeroVector = Vec3::getZero();
            object->handleCollision(other, zeroVector, normal, zeroVector,
                                    666, 666, 0, 0);
            return;
        }
    }

    bool wholeObjectRay = false;
    if (responseIsStatic && vertexCount <= static_cast<unsigned int>(s_smallStaticVertexLimit)
        && !mesh->hasLod(4))
    {
        const float objectRadius = mesh->getBoundingRadius(1);
        if (otherMesh->getBoundingRadius(otherLod) * 0.5f > objectRadius)
        {
            wholeObjectRay = true;
            if (objectTemplateCid == CID_LadderTemplate)
                return;
        }
    }

    // The original is a guarded function-local vector at 0x8ABAAA0. Pass 0 fills rotation-only
    // vertices, pass 1 indexes the same storage with the mesh's u16 edge pairs.
    static std::vector<Vec3> transformedVertices;
    transformedVertices.clear();

    unsigned short syntheticFace[4] = { 0, 1, 2, 1 };
    Vec3 syntheticVertices[3];
    for (int i = 0; i != 3; ++i)
        syntheticVertices[i].x = syntheticVertices[i].y = syntheticVertices[i].z = 0.0f;

    const int firstMode = treePassOnly ? 2 : 0;
    for (int mode = firstMode; mode <= 2; ++mode)
    {
        const unsigned short* indexData = 0;
        const unsigned short* faceData = 0;
        const Vec3* vertices = 0;
        int iterationCount = 0;
        float treeRadius = 0.0f;
        Vec3 otherCollisionCentre;
        otherCollisionCentre.x = otherCollisionCentre.y = otherCollisionCentre.z = 0.0f;

        if (mode == 1)
        {
            if (!playerControlAgainstStatic || !s_enableEdgePushPass)
                continue;
            indexData = static_cast<const unsigned short*>(mesh->getEdgeIndexVector(1));
            iterationCount = mesh->getEdgeIndexVectorSize(1) / 2;
            faceData = static_cast<const unsigned short*>(mesh->getFaceVector(1));
            vertices = static_cast<const Vec3*>(mesh->getVertexVector(1));
            (void)faceData;
            (void)vertices;
        }
        else if (mode == 2)
        {
            if (otherNode != otherRootNode)
                continue;
            treeRadius = objectNode->getTreeCollisionRadius();
            if (!object->hasCollisionLod(4) && treeRadius <= 0.0f)
                continue;
            if (object->hasCollisionLod(4))
            {
                if (treeRadius < 0.0099999998f)
                    treeRadius = 0.30000001f;
                iterationCount = mesh->getFaceCount(4);
                faceData = static_cast<const unsigned short*>(mesh->getFaceVector(4));
                vertices = static_cast<const Vec3*>(mesh->getVertexVector(4));
            }
            else
            {
                if (!object->hasCollisionLod(1))
                    continue;
                iterationCount = 1;
                faceData = syntheticFace;
                const LocalBoundingBox* const box =
                    static_cast<const LocalBoundingBox*>(mesh->getBoundingBox(1));
                const float centreX = (box->m_max.x + box->m_min.x) * 0.5f;
                syntheticVertices[0].x = centreX;
                syntheticVertices[0].y = box->m_min.y;
                syntheticVertices[0].z = 0.0f;
                syntheticVertices[1].x = centreX;
                syntheticVertices[1].y = box->m_max.y;
                syntheticVertices[1].z = 0.0f;
                syntheticVertices[2].x = centreX + 0.050000001f;
                syntheticVertices[2].y = box->m_min.y;
                syntheticVertices[2].z = 0.0f;
                vertices = syntheticVertices;
            }
            otherCollisionCentre = collisionTransformPoint(
                otherTransform, otherRootNode->getCenterOfCollisionOffset());
        }
        else
        {
            iterationCount = static_cast<int>(vertexCount);
        }

        for (int iteration = 0; iteration < iterationCount; ++iteration)
        {
            Vec3 rayStart;
            rayStart.x = rayStart.y = rayStart.z = 0.0f;
            Vec3 rayDirection;
            rayDirection.x = rayDirection.y = rayDirection.z = 0.0f;
            Vec3 rotatedVertex;
            rotatedVertex.x = rotatedVertex.y = rotatedVertex.z = 0.0f;
            Vec3 treeContactPoint;
            treeContactPoint.x = treeContactPoint.y = treeContactPoint.z = 0.0f;
            float separatingFloor = 0.0f;

            if (mode == 2)
            {
                const unsigned short* const face = faceData + 4 * iteration;
                const Vec3 first = collisionTransformPoint(*objectTransform, vertices[face[0]]);
                Vec3 second = collisionTransformPoint(*objectTransform, vertices[face[1]]);
                const Vec3 planeAxis = collisionRow(otherTransform, 1);
                const float firstSide = collisionDot(
                    collisionSubtract(first, otherCollisionCentre), planeAxis);
                float secondSide = collisionDot(
                    collisionSubtract(second, otherCollisionCentre), planeAxis);
                if ((secondSide < 0.0f) == (firstSide < 0.0f))
                {
                    second = collisionTransformPoint(*objectTransform, vertices[face[2]]);
                    secondSide = collisionDot(
                        collisionSubtract(second, otherCollisionCentre), planeAxis);
                }
                if ((firstSide < 0.0f) == (secondSide < 0.0f))
                    continue;

                const float fraction = secondSide / (secondSide - firstSide);
                treeContactPoint.x = second.x + (first.x - second.x) * fraction;
                treeContactPoint.y = second.y + (first.y - second.y) * fraction;
                treeContactPoint.z = second.z + (first.z - second.z) * fraction;
                rayStart = collisionSubtract(treeContactPoint, motion);
                rotatedVertex = collisionSubtract(otherCollisionCentre, rayStart);
                rayDirection = motion;                 // the optional debug line uses this arm
            }
            else if (mode == 1)
            {
                // Its actual two edge casts occur after the shared optional debug-ray arm below.
                // rotatedVertex intentionally remains zero while that arm is constructed.
            }
            else
            {
                const Vec3* const sourceVertices =
                    static_cast<const Vec3*>(mesh->getVertexVector(1));
                rotatedVertex = collisionRotate(*objectTransform, sourceVertices[iteration]);
                transformedVertices.push_back(rotatedVertex);
            }

            if (mode != 2)
            {
                if (wholeObjectRay)
                {
                    rayStart = previousPosition;
                    rayDirection = collisionAdd(motion, rotatedVertex);
                }
                else if (vertexCount <= 3)
                {
                    const Vec3* const sourceVertices =
                        static_cast<const Vec3*>(mesh->getVertexVector(1));
                    const float halfHeight = fabsf(sourceVertices[0].y);
                    const Vec3 localUp = collisionRow(*objectTransform, 1);
                    rotatedVertex = collisionScale(localUp, halfHeight * -2.0f);
                    const Vec3 translation = collisionTranslation(*objectTransform);
                    if (iteration == 0)
                    {
                        rayStart = collisionAdd(translation,
                                                collisionScale(localUp, halfHeight));
                    }
                    else
                    {
                        rayStart = collisionAdd(
                            translation, collisionScale(localUp, halfHeight + 0.1f));
                        const Vec3 localForward = collisionRow(*objectTransform, 2);
                        if (iteration == 1)
                            rayStart = collisionAdd(rayStart,
                                                    collisionScale(localForward, 0.1f));
                        else
                            rayStart = collisionSubtract(rayStart,
                                                         collisionScale(localForward, 0.1f));
                    }

                    const Mat4& nodeTransform = rootNode->getAbsoluteTransformation();
                    const Vec3 offset = collisionSubtract(
                        collisionTranslation(*objectTransform), rootNode->getAbsolutePosition());
                    const Vec3 nodeRight = collisionRow(nodeTransform, 0);
                    const float side = collisionDot(offset, nodeRight);
                    if (side >= 0.5f)
                    {
                        rayStart = collisionSubtract(rayStart, collisionScale(nodeRight, 0.5f));
                        rotatedVertex = collisionAdd(rotatedVertex,
                                                     collisionScale(nodeRight, 0.5f));
                    }
                    else if (side <= -0.5f)
                    {
                        rayStart = collisionAdd(rayStart, collisionScale(nodeRight, 0.5f));
                        rotatedVertex = collisionSubtract(rotatedVertex,
                                                          collisionScale(nodeRight, 0.5f));
                    }
                    motion.x = motion.y = motion.z = 0.0f;
                    // ELF 0x8368CDE -> 0x83670B4/0x83670C1 and PE 0x69C3B3..0x69C405
                    // clear motion only. A non-static response still adds the wheel span;
                    // clearing the final ray loses tyre contact/friction on carrier decks.
                    rayDirection = responseIsStatic ? motion : collisionAdd(motion, rotatedVertex);
                }
                else if (responseIsStatic)
                {
                    rayStart = collisionAdd(rotatedVertex, previousPosition);
                    rayDirection = motion;
                }
                else
                {
                    rayStart = previousPosition;
                    rayDirection = collisionAdd(motion, rotatedVertex);
                }
            }

            if (s_drawObjectCollisionLines)
            {
                const Vec3 rayEnd = collisionAdd(rayStart, rayDirection);
                collisionManager->drawLine(rayStart, rayEnd, 0xFF00FF00u);
            }

            if (mode == 1)
            {
                const unsigned short firstIndex = indexData[2 * iteration];
                const unsigned short secondIndex = indexData[2 * iteration + 1];
                const Vec3* const first = &transformedVertices[firstIndex];
                const Vec3* const second = &transformedVertices[secondIndex];
                mesh->getMaterialForVertex(1, firstIndex);       // return discarded, as shipped

                Vec3 firstPoint;
                firstPoint.x = first->x + objectTransform->m[12];
                firstPoint.y = first->y + objectTransform->m[13];
                firstPoint.z = first->z + objectTransform->m[14];
                Vec3 secondPoint;
                secondPoint.x = second->x + objectTransform->m[12];
                secondPoint.y = second->y + objectTransform->m[13];
                secondPoint.z = second->z + objectTransform->m[14];
                const Vec3 edge = collisionSubtract(secondPoint, firstPoint);

                Vec3 firstNormal;
                firstNormal.x = firstNormal.y = firstNormal.z = 0.0f;
                Vec3 firstHit;
                firstHit.x = firstHit.y = firstHit.z = 0.0f;
                const bool firstDirectionHit = otherMesh->getDistance(
                    otherLod, false, otherTransform, otherInverse, firstPoint, edge,
                    firstNormal, firstHit, planeDistance, alongDistance, &otherMaterial, faceInfo,
                    true, false);

                Vec3 reverseEdge;
                reverseEdge.x = -edge.x;
                reverseEdge.y = -edge.y;
                reverseEdge.z = -edge.z;
                const bool reverseDirectionHit = otherMesh->getDistance(
                    otherLod, false, otherTransform, otherInverse, secondPoint, reverseEdge,
                    normal, hitPosition, planeDistance, alongDistance, &otherMaterial, faceInfo,
                    true, false);

                if (firstDirectionHit)
                {
                    if (reverseDirectionHit)
                    {
                        normal.x = firstNormal.x + normal.x;
                        normal.y = 0.0f;
                        normal.z = firstNormal.z + normal.z;
                        hitPosition.x = (firstHit.x + hitPosition.x) * 0.5f;
                        hitPosition.y = (firstHit.y + hitPosition.y) * 0.5f;
                        hitPosition.z = (firstHit.z + hitPosition.z) * 0.5f;
                        const float horizontalSq = normal.z * normal.z + normal.x * normal.x;
                        if (horizontalSq < s_edgeNormalFallback * s_edgeNormalFallback)
                            normal = collisionSubtract(collisionTranslation(*objectTransform),
                                                       hitPosition);
                        collisionNormalize(normal);
                    }
                    else
                    {
                        normal = firstNormal;
                        hitPosition = firstHit;
                    }
                    rootNode->addAccelerationAtAbsolutePosition(
                        hitPosition, collisionScale(normal, s_edgePushScale));
                }
                continue;
            }

            bool collisionFound;
            int ownMaterial;
            if (mode == 2)
            {
                Ray3 ray;
                ray.origin = rayStart;
                ray.direction = collisionAdd(motion, rotatedVertex);
                const LocalBoundingBox* const otherBox =
                    static_cast<const LocalBoundingBox*>(otherMesh->getBoundingBox(otherLod));
                collisionFound = otherBox->intersectRay(
                    ray, otherTransform, hitPosition, &normal);
                if (!collisionFound)
                    continue;

                collisionNormalize(normal);
                normal.x = -normal.x;
                normal.y = -normal.y;
                normal.z = -normal.z;
                separatingFloor = collisionDot(
                    collisionSubtract(hitPosition, treeContactPoint), normal);
                if (fabsf(separatingFloor) > treeRadius)
                    continue;
                planeDistance = treeRadius - separatingFloor;
                alongDistance = planeDistance;

                IObject* const otherRootObject = getRootParent(other);
                if (otherRootObject)
                {
                    IArmorGLComp* const armor = static_cast<IArmorGLComp*>(
                        L::armorComponent(otherRootObject));
                    if (armor)
                        otherMaterial = armor->getDefaultMaterial();
                }
                if (object->hasCollisionLod(4))
                    ownMaterial = mesh->getMaterialForVertex(4, 0);
                else
                    ownMaterial = mesh->getMaterialForVertex(1, 0);
            }
            else
            {
                collisionFound = otherMesh->getDistance(
                    otherLod, false, otherTransform, otherInverse, rayStart, rayDirection,
                    normal, hitPosition, planeDistance, alongDistance, &otherMaterial, faceInfo,
                    true, false);
                if (!collisionFound)
                    continue;
                ownMaterial = mesh->getMaterialForVertex(1, iteration);
            }

            const Vec3& objectTangent = rootNode->getTangentSpeed(hitPosition);
            const Vec3& otherTangent = otherRootNode->getTangentSpeed(hitPosition);
            Vec3 relativeSpeed;
            relativeSpeed.x = objectTangent.x - otherTangent.x;
            relativeSpeed.y = objectTangent.y - otherTangent.y;
            relativeSpeed.z = objectTangent.z - otherTangent.z;

            if ((mode == 2 || s_rejectSeparatingHits == 1)
                && separatingFloor > collisionDot(relativeSpeed, normal))
                continue;
            const float penetration = planeDistance;
            const float speedSq = collisionLengthSq(relativeSpeed);

            if (speedSq > 0.1f || otherMaterial == 666)
            {
                if (otherResponse->getClassID() != CID_StaticResponsePhysics
                    && otherRootNode->getMass() > rootNode->getMass())
                    collisionAssignWeak(m_sharedF4, other);

                const Vec3 objectRelativeHit = collisionSubtract(
                    hitPosition, collisionTranslation(*objectTransform));
                const Vec3 objectReportedSpeed = collisionScale(
                    relativeSpeed, m_hardPush + 1.0f);
                const bool objectRejected = !object->handleCollision(
                    other, objectReportedSpeed, normal, objectRelativeHit,
                    ownMaterial, otherMaterial, 0, faceInfo);

                const Vec3 otherRelativeHit = collisionSubtract(
                    hitPosition, collisionTranslation(otherTransform));
                Vec3 otherReportedSpeed = collisionScale(
                    relativeSpeed, m_hardPush + 1.0f);
                otherReportedSpeed.x = -otherReportedSpeed.x;
                otherReportedSpeed.y = -otherReportedSpeed.y;
                otherReportedSpeed.z = -otherReportedSpeed.z;
                if (other->handleCollision(object, otherReportedSpeed, normal, otherRelativeHit,
                                           otherMaterial, ownMaterial, faceInfo, 0) != true
                    || objectRejected)
                    continue;
            }

            if (responseIsStatic)
                otherRootNode->setIsInAir(false);
            else if (otherResponseIsStatic)
                rootNode->setIsInAir(false);

            IResponsePhysics* effectResponse = otherRootResponse;
            if (rootResponseCid == CID_ResponsePhysics)
            {
                effectResponse = rootResponse;
                if (otherRootResponseCid == rootResponseCid)
                {
                    if (reinterpret_cast<uintptr_t>(otherRootResponse)
                        < reinterpret_cast<uintptr_t>(rootResponse))
                        effectResponse = otherRootResponse;
                }
            }

            const float effectSpeedSq = collisionLengthSq(relativeSpeed);
            const float objectEffectSpeed = objectNode->getHandleCollisionSpeed();
            const float otherEffectSpeed = otherNode->getHandleCollisionSpeed();
            if (effectSpeedSq >= objectEffectSpeed * objectEffectSpeed
                && effectSpeedSq >= otherEffectSpeed * otherEffectSpeed)
            {
                static_cast<ResponsePhysics*>(effectResponse)->addEffect(
                    ownMaterial, otherMaterial, relativeSpeed, effectSpeedSq,
                    hitPosition, normal);
            }

            float ownShare;
            float otherShare;
            if (rootNode->getIsSleeping())
            {
                ownShare = 0.0f;
                otherShare = -1.0f;
            }
            else if (otherRootNode->getIsSleeping())
            {
                ownShare = 1.0f;
                otherShare = 0.0f;
            }
            else
            {
                const float otherMass = otherRootNode->getMass();
                const float objectMass = rootNode->getMass();
                ownShare = otherMass / (otherRootNode->getMass() + objectMass);
                if (!isParticle)
                    ownShare = collisionClampedShare(ownShare);
                otherShare = ownShare - 1.0f;
            }

            float impulseOwnShare = ownShare;
            float impulseOtherShare = otherShare;
            float depthOwnShare = ownShare;
            float depthOtherShare = otherShare;
            if (!isParticle)
            {
                const float normalSpeed = collisionDot(normal, relativeSpeed);
                if (rootNode->getSleepiness() || responseIsStatic)
                {
                    if (!otherRootNode->getSleepiness() && !otherResponseIsStatic)
                    {
                        const float objectMass = rootNode->getMass();
                        const float impact = normalSpeed * -objectMass;
                        if (impact > otherRootNode->getMass() * s_collisionWakeMassFactor)
                        {
                            otherRootNode->setIsAwake();
                            const float otherMass = otherRootNode->getMass();
                            impulseOwnShare = collisionClampedShare(
                                otherMass / (objectMass + otherMass));
                            impulseOtherShare = impulseOwnShare - 1.0f;
                        }
                    }
                }
                else
                {
                    const float otherMass = otherRootNode->getMass();
                    const float impact = normalSpeed * -otherMass;
                    if (impact > rootNode->getMass() * s_collisionWakeMassFactor)
                    {
                        rootNode->setIsAwake();
                        const float objectMass = rootNode->getMass();
                        impulseOwnShare = collisionClampedShare(
                            otherMass / (objectMass + otherMass));
                        impulseOtherShare = impulseOwnShare - 1.0f;
                    }
                }

                if (fabsf(normalSpeed) > s_equalShareNormalSpeed
                    && !responseIsStatic && !otherResponseIsStatic)
                {
                    depthOwnShare = 0.5f;
                    depthOtherShare = -0.5f;
                }
            }

            if (rootNode->getIsPlatformPhysics())
                impulseOwnShare = 0.0f;
            if (otherRootNode->getIsPlatformPhysics())
                impulseOtherShare = 0.0f;

            if (fabsf(impulseOwnShare) > FLT_EPSILON)
            {
                const Vec3 relativeHit = collisionSubtract(
                    hitPosition, collisionTranslation(*objectTransform));
                response->impulseOn(relativeHit,
                                    collisionScale(relativeSpeed, impulseOwnShare), normal,
                                    penetration * depthOwnShare, ownMaterial, otherMaterial);
            }
            if (fabsf(impulseOtherShare) > FLT_EPSILON)
            {
                const Vec3 relativeHit = collisionSubtract(
                    hitPosition, collisionTranslation(otherTransform));
                otherResponse->impulseOn(relativeHit,
                                         collisionScale(relativeSpeed, impulseOtherShare), normal,
                                         penetration * depthOtherShare,
                                         ownMaterial, otherMaterial);
            }
        }
    }
}

// Returns signed friction from slip magnitude: linear rise to one at peakAt, then
// a fall toward minDynamic at slip one, floored at minDynamic.
float frictionCurve(float slip, float peakAt, float minDynamic)
{
    float sign = 1.0f;
    if (!(slip > 0.0f))
    {
        sign = 0.0f;
        if (slip < 0.0f)
            sign = -1.0f;
    }
    const float magnitude = fabsf(slip);
    float value;
    if (peakAt < magnitude)
    {
        value = (1.0f - minDynamic) / (peakAt - 1.0f) * (magnitude - 1.0f) + minDynamic;
        if (value <= minDynamic)
            value = minDynamic;
    }
    else
    {
        value = magnitude / peakAt;
    }
    return value * sign;
}

// Returns wheel torque from gravity projected onto the contact plane and forward axis,
// scaled by radius and massTerm.
float computeSlopeTorque(const Vec3& normal, const Vec3& forward, float radius, float massTerm)
{
    const float zero = 0.0f;
    const float g = static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity() * massTerm;
    const float along = g * normal.y + normal.x * zero + normal.z * zero;
    return ((g - normal.y * along) * forward.y + (zero - normal.x * along) * forward.x
            + (zero - normal.z * along) * forward.z) * radius;
}

// world::addTorque(float, float) - 0x836B490, 11 B.
float addTorque(float a, float b)
{
    return b + a;
}

// world::computeWheelSpeed(spin, torque, frictionTorque, inertia, dt) - 0x836B3C0, 90 B, read in
// full. One Euler step of the wheel's angular speed under the drive torque, then the friction
// torque taken off the magnitude (never past zero), sign preserved.
float computeWheelSpeed(float spin, float torque, float frictionTorque, float inertia, float dt)
{
    float sign = 1.0f;
    const float next = torque / inertia * dt + spin;
    if (next <= 0.0f)
    {
        sign = -1.0f;
        if (next >= 0.0f)
            sign = 0.0f;
    }
    float magnitude = fabsf(next) - dt * (frictionTorque / inertia);
    if (magnitude <= 0.0f)
        magnitude = 0.0f;
    return magnitude * sign;
}

// Computes tyre friction along wheel forward/right axes, with optional normalized slip and driven-wheel scaling.
// Writes total force and, when provided, the separate curve outputs. Force includes slope and material modifiers.
void ResponsePhysics::computeFriction(Vec3* out, float* outLong, float* outLat,
                                      const EngineTemplate* engineTemplate,
                                      const Vec3& relativeSpeed, float forwardSpeed,
                                      const Vec3& forward, const Vec3& right, bool drive)
{
    float ny = 0.0f;
    if (m_averageNormal.y >= 0.0f)
        ny = m_averageNormal.y;
    float power = ny;
    for (int i = s_normalPowerExtra; i > 0; --i)
        power = power * ny;

    float longSlip = forward.y * relativeSpeed.y + forward.x * relativeSpeed.x + forward.z * relativeSpeed.z;
    if (s_normaliseSlipBySpeed == 1)
    {
        const float speed = fabsf(forwardSpeed);
        if (speed > 1.0f)
            longSlip = longSlip / speed;
    }
    float longForce = frictionCurve(longSlip * engineTemplate->m_newCar2WheelLongScale,
                                    engineTemplate->m_newCar2WheelLongPeakAt,
                                    engineTemplate->m_newCar2WheelLongMinDynamicFriction)
                    * engineTemplate->m_newCar2WheelLongFrictionMod;
    const float latSlip = (relativeSpeed.y * right.y + relativeSpeed.x * right.x + relativeSpeed.z * right.z)
                        * engineTemplate->m_newCar2WheelLatScale;
    float latForce = frictionCurve(latSlip, engineTemplate->m_newCar2WheelLatPeakAt,
                                   engineTemplate->m_newCar2WheelLatMinDynamicFriction)
                   * engineTemplate->m_newCar2WheelLatFrictionMod;
    if (drive)
    {
        latForce = latForce * engineTemplate->m_newCar2WheelLatDriveFrictionMod;
        longForce = longForce * engineTemplate->m_newCar2WheelLongDriveFrictionMod;
    }
    const float scale = power * m_averageFriction * s_frictionForceScale;
    out->x = (forward.x * longForce + right.x * latForce) * scale;
    out->y = (forward.y * longForce + right.y * latForce) * scale;
    out->z = scale * (forward.z * longForce + right.z * latForce);
    if (outLong)
        *outLong = longForce;
    if (outLat)
        *outLat = latForce;
}

// Applies spring-wheel traction and rolling resistance to the root physics node.
// Other templates return immediately; newCar2 engines delegate to addFrictionNew.
// Grip flags select geared, tracked or lateral behavior and transition between grip and sliding.
// Updates engine feedback, wheel animation and contact state in order, then clears the accumulators.
void ResponsePhysics::addFriction(IPhysicsNode* node)
{
    typedef object_detail::LayoutAccess L;
    static float lat_lim = 1.0f;                                          // 0x898BC04, the ELF's name

    if (!m_enabled)
        return;
    if (L::objectTemplate(m_object)->getClassID() != CID_SpringTemplate)  // +0x14 -> +0x10
        return;

    EngineBase* const engine =
        static_cast<EngineBase*>(static_cast<Spring*>(m_object)->getEngine());   // +0x180
    if (engine && (engine->getEngineType() & 0x80) != 0)                 // +0x1AC, `test al, al; js`
    {
        addFrictionNew(node);
        return;
    }

    Vec3 force;
    force.x = force.y = force.z = 0.0f;
    Vec3 relative;
    relative.x = relative.y = relative.z = 0.0f;

    if ((getPermanentGrip() & 0x80) != 0)                                 // +0x80, `js`
        return;
    if (m_contactCount == 0 || getPermanentGrip() == 0)
    {
        m_grip = 1;
        return;
    }

    IPhysicsNode* const rootNode = node->getRootNode();                   // +0x150
    getRootResponse();                                                    // +0x94 - result unused, as shipped

    Vec3 speed = m_averageSpeed;
    speed.y = static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity() / 30.0f + speed.y;
    Vec3 normalPart;
    alongNormal(speed, m_averageNormal, normalPart);
    Vec3 tangent;
    tangent.x = speed.x - normalPart.x;
    tangent.y = speed.y - normalPart.y;
    tangent.z = speed.z - normalPart.z;

    IObject* const parent = L::parent(m_object);
    const Mat4& transform = parent ? parent->getAbsoluteTransformation()
                                   : m_object->getAbsoluteTransformation();   // slot 33
    Vec3 forward;
    forward.x = transform.m[8];
    forward.y = transform.m[9];
    forward.z = transform.m[10];

    if (m_averageResistance > 0.0f)
    {
        Vec3 drag;
        drag.x = -tangent.x * m_averageResistance;
        drag.y = -tangent.y * m_averageResistance;
        drag.z = m_averageResistance * -tangent.z;
        rootNode->addAccelerationAtRelativePosition(Vec3::getZero(), drag);   // +0x94
    }

    float drive = 0.0f;
    const unsigned char grip = m_grip;
    bool haveForce = true;
    if ((grip & 8) != 0)
    {
        if (!engine)
        {
            haveForce = false;                                            // force and relV stay zero
        }
        else if ((engine->getEngineType() & 0x40) != 0)
        {
            // the geared (car) drive
            const float spin = static_cast<PhysicsSpring*>(L::physicsNode(m_object))->m_wheelSpin;   // node +0x250
            Vec3 dir = forward;
            Vec3 np;
            alongNormal(forward, m_averageNormal, np);
            if (fabsf(np.x) < 0.001f) np.x = 0.0f;
            if (fabsf(np.y) < 0.001f) np.y = 0.0f;
            if (fabsf(np.z) < 0.001f) np.z = 0.0f;
            dir.x = dir.x - np.x;
            dir.y = dir.y - np.y;
            dir.z = dir.z - np.z;
            if (fabsf(dir.x) < 0.001f) dir.x = 0.0f;
            if (fabsf(dir.y) < 0.001f) dir.y = 0.0f;
            if (fabsf(dir.z) < 0.001f) dir.z = 0.0f;
            normalizeInPlace(dir.x, dir.y, dir.z);

            force.x = spin * dir.x;
            force.y = spin * dir.y;
            force.z = spin * dir.z;
            float slope = spin >= 0.0f ? dir.y : -dir.y;
            if (slope > 1.0f) slope = 1.0f;
            if (slope < -1.0f) slope = -1.0f;
            const float reduction = 1.0f - slope * s_uphillDriveReduction;
            force.x = force.x * reduction;
            force.y = force.y * reduction;
            force.z = reduction * force.z;
            relative.x = force.x - tangent.x;
            relative.y = force.y - tangent.y;
            relative.z = force.z - tangent.z;
            const float slip = spin - (dir.z * tangent.z + (tangent.y * dir.y + dir.x * tangent.x));
            float cubed = slip;
            if (fabsf(spin) > 0.1f)
            {
                const float r = slip / spin;
                cubed = r * r * r * spin;
            }
            float gain = dir.y;
            if (gain > 1.0f) gain = 1.0f;
            if (gain < -1.0f) gain = -1.0f;
            drive = (gain * s_slopeDriveGain + 1.0f) * cubed;
        }
        else
        {
            // the tracked drive
            const float ratio = 3.5f * engine->m_template->m_differential                 // +0x1B0 -> +0x3CC
                              / engine->getGearRatio(engine->m_gear);                     // +0x1A4 (+0x1E8)
            const float rpm = engine->getCurrentDifferentialRPM(node->getRelativePosition().x) * ratio;   // +0x5C
            if (engine->m_byte1DC != true)                                                // +0x1DC
            {
                const float k = 1.0f - engine->m_soundInput * 0.5f;                       // +0x1E4
                force.x = forward.x * rpm * k;
                force.y = forward.y * rpm * k;
                force.z = k * (rpm * forward.z);
            }
            Vec3 alongForward;
            {
                const float ff = forward.z * forward.z + forward.y * forward.y + forward.x * forward.x;
                if (FLT_EPSILON < fabsf(ff))
                {
                    const float k = (tangent.z * forward.z + tangent.y * forward.y + tangent.x * forward.x) / ff;
                    alongForward.x = forward.x * k;
                    alongForward.y = forward.y * k;
                    alongForward.z = k * forward.z;
                }
                else
                {
                    alongForward.x = alongForward.y = alongForward.z = 0.0f;
                }
            }
            const float half = engine->m_soundInput * 0.5f;
            force.x = force.x + alongForward.x * half;
            force.y = force.y + alongForward.y * half;
            force.z = force.z + alongForward.z * half;
            Vec3 np;
            alongNormal(force, m_averageNormal, np);
            force.x = force.x - np.x;
            force.y = force.y - np.y;
            force.z = force.z - np.z;
            relative.x = force.x - tangent.x;
            relative.y = force.y - tangent.y;
            relative.z = force.z - tangent.z;
            static_cast<PhysicsSpring*>(L::physicsNode(m_object))->m_wheelSpin =
                force.z * forward.z + force.y * forward.y + force.x * forward.x;

            if (!static_cast<const SpringTemplate*>(L::objectTemplate(m_object))->m_rotateUV)   // template +0x19C
            {
                const float degrees = 57.295776f * static_cast<PhysicsSpring*>(node)->m_wheelAngle;   // node +0x254
                IObject* const o = m_object;
                const Mat4& base = L::parent(o) ? o->getRelativeTransformation()   // slot 58
                                                : o->getAbsoluteTransformation(); // slot 33
                Mat4 turned = base;
                if (fabsf(degrees) > FLT_EPSILON)
                {
                    Mat4 rotation = Mat4::getIdentity();
                    setRotateAboutLine(rotation, degrees, *reinterpret_cast<const Vec3*>(&turned.m[0]));
                    const Mat4 copy(turned);
                    turned.rotMult(copy, rotation);
                }
                if (L::parent(o))
                    o->setRelativeTransformation(turned);                  // slot 59
                else
                    o->setAbsoluteTransformation(turned);                  // slot 35
            }
        }
    }
    else if ((grip & 4) != 0)
    {
        const Mat4& nodeTransform = node->getAbsoluteTransformation();    // +0x3C
        Vec3 right;
        right.x = nodeTransform.m[0];
        right.y = nodeTransform.m[1];
        right.z = nodeTransform.m[2];
        Vec3 np;
        alongNormal(right, m_averageNormal, np);
        const Mat4& nodeTransform2 = node->getAbsoluteTransformation();
        Vec3 lateral;
        lateral.x = nodeTransform2.m[0] - np.x;
        lateral.y = nodeTransform2.m[1] - np.y;
        lateral.z = nodeTransform2.m[2] - np.z;
        Vec3 lateralPart;
        const float ll = lateral.z * lateral.z + lateral.y * lateral.y + lateral.x * lateral.x;
        if (fabsf(ll) > FLT_EPSILON)
        {
            const float k = (tangent.y * lateral.y + tangent.x * lateral.x + tangent.z * lateral.z) / ll;
            lateralPart.x = lateral.x * k;
            lateralPart.y = lateral.y * k;
            lateralPart.z = lateral.z * k;
        }
        else
        {
            lateralPart.x = lateralPart.y = lateralPart.z = 0.0f;
        }
        relative.x = -lateralPart.x;
        relative.y = -lateralPart.y;
        relative.z = -lateralPart.z;
        force.x = tangent.x - lateralPart.x;
        force.y = tangent.y - lateralPart.y;
        force.z = tangent.z - lateralPart.z;
        if (L::objectTemplate(getObject())->getClassID() == CID_SpringTemplate)
            static_cast<PhysicsSpring*>(node)->m_wheelSpin =
                forward.y * force.y + forward.x * force.x + forward.z * force.z;
    }
    else
    {
        relative.x = 0.0f - tangent.x;
        relative.y = 0.0f - tangent.y;
        relative.z = 0.0f - tangent.z;
    }

    if (haveForce && engine && (engine->getEngineType() & 0x40) != 0)
    {
        const float along = relative.y * forward.y + relative.x * forward.x + relative.z * forward.z;
        if (fabsf(along) > lat_lim)
        {
            const float rx = transform.m[0];
            const float ry = transform.m[1];
            const float rz = transform.m[2];
            const float rr = rx * rx + ry * ry + rz * rz;
            Vec3 sideways;
            if (fabsf(rr) > FLT_EPSILON)
            {
                const float k = (relative.z * rz + relative.y * ry + relative.x * rx) / rr;
                sideways.x = rx * k;
                sideways.y = ry * k;
                sideways.z = rz * k;
            }
            else
            {
                sideways.x = sideways.y = sideways.z = 0.0f;
            }
            const float gain = s_lateralAmplification - 1.0f;
            relative.x = relative.x + sideways.x * gain;
            relative.y = relative.y + sideways.y * gain;
            relative.z = relative.z + sideways.z * gain;
        }
    }

    // the force limit
    Vec3 out;
    out.x = out.y = out.z = 0.0f;
    if (engine && (engine->getEngineType() & 0x40) != 0)
    {
        out.x = relative.x * s_frictionForceScale;
        out.y = relative.y * s_frictionForceScale;
        out.z = s_frictionForceScale * relative.z;
        if (fabsf(out.x) < 0.001f) out.x = 0.0f;
        if (fabsf(out.y) < 0.001f) out.y = 0.0f;
        if (fabsf(out.z) < 0.001f) out.z = 0.0f;
        float ny = m_averageNormal.y;
        if (0.0f > ny)
            ny = 0.0f;
        float power = ny;
        for (int i = s_normalPowerExtra; i > 0; --i)
            power = power * ny;
        const float gripping = s_grippingFrictionFactor * m_averageFriction * s_carFrictionScale * power;
        const float sliding = power * (s_slidingFrictionFactor * m_averageFriction * s_carFrictionScale);
        if ((m_grip & 0x40) != 0)
        {
            if (clampLength(out, gripping))
            {
                m_grip = static_cast<unsigned char>(m_grip & ~0x40u);
                clampLength(out, sliding);
            }
        }
        else
        {
            if (!clampLength(out, sliding))
                m_grip = static_cast<unsigned char>(m_grip | 0x40u);
        }
    }
    else
    {
        const float ny = m_averageNormal.y;
        const float gripping = m_averageFriction * 2.25f * 9.8199997f / 30.0f * ny;
        const float sliding = ny * (9.8199997f * (m_averageFriction * 1.5f) / 30.0f);
        out = relative;
        if ((m_grip & 0x40) == 0)
        {
            if (!clampLength(out, sliding))
                m_grip = static_cast<unsigned char>(m_grip | 0x40u);
        }
        else if (clampLength(out, gripping))
        {
            m_grip = static_cast<unsigned char>(m_grip & ~0x40u);
            clampLength(out, sliding);
        }
    }

    // the engine feedback
    if (engine)
    {
        if ((engine->getEngineType() & 0x40) != 0)
        {
            engine->m_dword1EC = drive * engine->m_soundInput + engine->m_dword1EC;   // +0x1EC (a float)
            ++engine->m_dword1F0;                                                      // +0x1F0
        }
        else
        {
            if (s_projectTrackForce)
            {
                const float ff = forward.y * forward.y + forward.x * forward.x + forward.z * forward.z;
                if (fabsf(ff) > FLT_EPSILON)
                {
                    const float k = (out.x * forward.x + out.y * forward.y + out.z * forward.z) / ff;
                    out.x = forward.x * k;
                    out.y = forward.y * k;
                    out.z = forward.z * k;
                }
                else
                {
                    out.x = out.y = out.z = 0.0f;
                }
            }
            Engine* const tracked = static_cast<Engine*>(engine);
            Vec3 trackForce = out;
            tracked->feedbackLoop(trackForce, forward);
            tracked->m_dword210 = m_averageFriction + tracked->m_dword210;            // +0x210 (a float)
            ++tracked->m_dword214;                                                     // +0x214
            tracked->setTrackHasContact(getObject()->getRelativePosition().x);        // slot 56
        }
    }

    Vec3 impulse;
    impulse.x = out.x * 30.0f;
    impulse.y = out.y * 30.0f;
    impulse.z = 30.0f * out.z;
    const Vec3& at = node->getAbsolutePosition();                         // +0x48
    Vec3 where;
    where.x = m_averagePosition.x + at.x;
    where.y = m_averagePosition.y + at.y;
    where.z = m_averagePosition.z + at.z;
    rootNode->addFrictionAtAbsolutePosition(where, impulse);              // +0x98
    if (m_averageSpeed.x * m_averageSpeed.x + m_averageSpeed.y * m_averageSpeed.y
            + m_averageSpeed.z * m_averageSpeed.z > 0.1f
        && rootNode->getSleepiness() >= 0)                                // +0x128
        rootNode->setIsAwake();                                           // +0x130
    m_contactCount = 0;
    m_averageNormal.x = m_averageNormal.y = m_averageNormal.z = 0.0f;
    m_averageSpeed.x = m_averageSpeed.y = m_averageSpeed.z = 0.0f;
    m_averagePosition.x = m_averagePosition.y = m_averagePosition.z = 0.0f;
}

// Computes newCar2 wheel friction; the spring must have an engine.
// Driven wheels integrate spin twice, feeding friction torque into the second step, then apply
// gear-sign and RPM limits. Grip bit 7 suppresses root force; other branches use driven or lateral
// relative velocity as appropriate. Applies force at the mean contact, wakes the root and resets contact sums.
void ResponsePhysics::addFrictionNew(IPhysicsNode* node)
{
    typedef object_detail::LayoutAccess L;

    Vec3 force;
    force.x = force.y = force.z = 0.0f;
    Vec3 relative;
    relative.x = relative.y = relative.z = 0.0f;

    IPhysicsNode* const rootNode = node->getRootNode();                   // +0x150
    getRootResponse();                                                    // dead, as shipped
    PhysicsSpring* const wheel = static_cast<PhysicsSpring*>(L::physicsNode(m_object));   // +0x44

    Vec3 speed = m_averageSpeed;
    speed.y = static_cast<IPhysicsSystem*>(basicPhysicsSystem)->getGravity() / 30.0f + speed.y;
    Vec3 normalPart;
    alongNormal(speed, m_averageNormal, normalPart);
    Vec3 tangent;
    tangent.x = speed.x - normalPart.x;
    tangent.y = speed.y - normalPart.y;
    tangent.z = speed.z - normalPart.z;

    IObject* const parent = L::parent(m_object);
    const Mat4& transform = parent ? parent->getAbsoluteTransformation()
                                   : m_object->getAbsoluteTransformation();
    const Vec3& forward = *reinterpret_cast<const Vec3*>(&transform.m[8]);
    const Vec3& right   = *reinterpret_cast<const Vec3*>(&transform.m[0]);
    const float forwardSpeed = transform.m[10] * tangent.z + (transform.m[9] * tangent.y + transform.m[8] * tangent.x);

    Vec3 friction;
    friction.x = friction.y = friction.z = 0.0f;
    float longForce = 0.0f;
    float latForce = 0.0f;
    EngineBase* const engine =
        static_cast<EngineBase*>(static_cast<Spring*>(m_object)->getEngine());   // +0x180, not null-tested
    const unsigned char grip = m_grip;

    if ((grip & 8) != 0)
    {
        Vec3 dir = forward;
        Vec3 np;
        alongNormal(forward, m_averageNormal, np);
        if (fabsf(np.x) < 0.001f) np.x = 0.0f;
        if (fabsf(np.y) < 0.001f) np.y = 0.0f;
        if (fabsf(np.z) < 0.001f) np.z = 0.0f;
        dir.x = dir.x - np.x;
        dir.y = dir.y - np.y;
        dir.z = dir.z - np.z;
        if (fabsf(dir.x) < 0.001f) dir.x = 0.0f;
        if (fabsf(dir.y) < 0.001f) dir.y = 0.0f;
        if (fabsf(dir.z) < 0.001f) dir.z = 0.0f;
        normalizeInPlace(dir.x, dir.y, dir.z);

        const float massTerm = rootNode->getMass() * 0.25f;                 // +0xDC
        const float gearRatio = engine->getGearRatio(engine->m_gear);      // +0x1A4 (+0x1E8)
        const EngineTemplate* const engineTemplate = engine->m_template;   // +0x1B0
        const float driveFactor = 3.5f * engineTemplate->m_differential * gearRatio;
        const float driveTorque = engine->getCurrentTorque() * driveFactor * 0.5f;   // +0x1A8
        const float spin = wheel->m_wheelSpin;                            // +0x250
        float slopeMod = engineTemplate->m_newCar2SlopeTorqueMod;         // +0x644
        if (engine->m_gear == 1)
            slopeMod = slopeMod * s_firstGearSlopeTorqueMod;
        const float radius = wheel->getWheelRadius();
        const float slopeTorque =
            computeSlopeTorque(m_averageNormal, forward, radius, massTerm) * slopeMod;
        const float torque = addTorque(driveTorque, slopeTorque);
        float resist = engineTemplate->m_newCar2FrictionTorque;           // +0x63C
        if (engine->m_rotationInput.z == 0.0f)                            // +0x160 - no throttle
            resist = resist + engineTemplate->m_newCar2EngineBrakeTorque; // +0x638
        if (engine->m_byte1DC)                                            // +0x1DC - braking
            resist = resist + engineTemplate->m_newCar2BrakeTorque;       // +0x634
        const float inertia = engineTemplate->m_newCar2WheelInertia + engineTemplate->m_newCar2WheelInertia;   // +0x62C

        const float spin1 = computeWheelSpeed(spin, torque, resist, inertia, 0.033333335f);
        Vec3 w;
        w.x = dir.x * spin1;
        w.y = dir.y * spin1;
        w.z = spin1 * dir.z;
        const float radius2 = wheel->getWheelRadius();
        force.x = w.x * radius2;
        force.y = w.y * radius2;
        force.z = radius2 * w.z;
        relative.x = force.x - tangent.x;
        relative.y = force.y - tangent.y;
        relative.z = force.z - tangent.z;
        computeFriction(&friction, &longForce, &latForce, engineTemplate, relative, forwardSpeed,
                        forward, right, true);
        const float frictionMod = engineTemplate->m_newCar2FrictionMod;   // +0x640
        friction.x = friction.x * frictionMod;
        friction.y = friction.y * frictionMod;
        friction.z = frictionMod * friction.z;
        const float feedback = -engineTemplate->m_newCar2EngineFeedbackMod                // +0x630
                             * (friction.z * forward.z + friction.y * forward.y + friction.x * forward.x)
                             * 30.0f * massTerm;
        const float feedbackTorque = wheel->getWheelRadius() * feedback + slopeTorque;
        const float torque2 = addTorque(driveTorque, feedbackTorque);
        const float spin2 = computeWheelSpeed(spin, torque2, resist, inertia, 0.033333335f);

        const float gearRatio2 = engine->getGearRatio(engine->m_gear);
        const float rpmFactor = 3.5f * engineTemplate->m_differential * gearRatio2 * 9.5492964f;
        const float maxSpin = engineTemplate->m_newCar2MaxRpm / rpmFactor;   // +0x624
        float sign = 1.0f;
        if (spin2 <= 0.0f)
        {
            sign = -1.0f;
            if (spin2 >= 0.0f)
                sign = 0.0f;
        }
        float magnitude = fabsf(spin2);
        if (!(magnitude <= maxSpin))
            magnitude = maxSpin;
        float kept = sign * magnitude;
        float rpm = rpmFactor * kept;
        const bool agrees = engine->m_gear <= 0 ? (rpm <= 0.0f) : (rpm >= 0.0f);
        if (!agrees)
        {
            rpm = 0.0f;
            kept = 0.0f;
        }
        wheel->m_wheelSpin = kept;
        engine->m_dword1EC = rpm + engine->m_dword1EC;                     // +0x1EC (a float)
        ++engine->m_dword1F0;                                             // +0x1F0
    }
    else
    {
        bool lateralArm = true;
        if ((grip & 4) == 0)
        {
            if ((getPermanentGrip() & 0x80) == 0)
            {
                lateralArm = false;
                relative.x = 0.0f - tangent.x;
                relative.y = 0.0f - tangent.y;
                relative.z = 0.0f - tangent.z;
                computeFriction(&friction, &longForce, &latForce, engine->m_template, relative,
                                forwardSpeed, forward, right, false);
            }
        }
        if (lateralArm)
        {
            const Mat4& nodeTransform = node->getAbsoluteTransformation();   // +0x3C
            Vec3 nodeRight;
            nodeRight.x = nodeTransform.m[0];
            nodeRight.y = nodeTransform.m[1];
            nodeRight.z = nodeTransform.m[2];
            Vec3 np;
            alongNormal(nodeRight, m_averageNormal, np);
            const Mat4& nodeTransform2 = node->getAbsoluteTransformation();
            Vec3 lateral;
            lateral.x = nodeTransform2.m[0] - np.x;
            lateral.y = nodeTransform2.m[1] - np.y;
            lateral.z = nodeTransform2.m[2] - np.z;
            Vec3 lateralPart;
            const float ll = lateral.z * lateral.z + lateral.y * lateral.y + lateral.x * lateral.x;
            if (fabsf(ll) > FLT_EPSILON)
            {
                const float k = (tangent.y * lateral.y + tangent.x * lateral.x + tangent.z * lateral.z) / ll;
                lateralPart.x = lateral.x * k;
                lateralPart.y = lateral.y * k;
                lateralPart.z = lateral.z * k;
            }
            else
            {
                lateralPart.x = lateralPart.y = lateralPart.z = 0.0f;
            }
            relative.x = -lateralPart.x;
            relative.y = -lateralPart.y;
            relative.z = -lateralPart.z;
            force.x = tangent.x - lateralPart.x;
            force.y = tangent.y - lateralPart.y;
            force.z = tangent.z - lateralPart.z;
            if (L::objectTemplate(getObject())->getClassID() == CID_SpringTemplate)
            {
                const float along = forward.y * force.y + forward.x * force.x + forward.z * force.z;
                static_cast<PhysicsSpring*>(node)->m_wheelSpin = along / wheel->getWheelRadius();
            }
            if ((m_grip & 4) != 0)
                computeFriction(&friction, &longForce, &latForce, engine->m_template, relative,
                                forwardSpeed, forward, right, false);
        }
    }

    if ((m_grip & 0x80) == 0)
    {
        Vec3 impulse;
        impulse.x = friction.x * 30.0f;
        impulse.y = friction.y * 30.0f;
        impulse.z = 30.0f * friction.z;
        const Vec3& at = node->getAbsolutePosition();                     // +0x48
        Vec3 where;
        where.x = m_averagePosition.x + at.x;
        where.y = m_averagePosition.y + at.y;
        where.z = m_averagePosition.z + at.z;
        rootNode->addFrictionAtAbsolutePosition(where, impulse);          // +0x98
    }
    if (m_averageSpeed.x * m_averageSpeed.x + m_averageSpeed.y * m_averageSpeed.y
            + m_averageSpeed.z * m_averageSpeed.z > 0.1f
        && rootNode->getSleepiness() >= 0)
        rootNode->setIsAwake();
    m_contactCount = 0;
    m_averageNormal.x = m_averageNormal.y = m_averageNormal.z = 0.0f;
    m_averageSpeed.x = m_averageSpeed.y = m_averageSpeed.z = 0.0f;
    m_averagePosition.x = m_averagePosition.y = m_averagePosition.z = 0.0f;
}

} // namespace world
} // namespace hfe
} // namespace dice
