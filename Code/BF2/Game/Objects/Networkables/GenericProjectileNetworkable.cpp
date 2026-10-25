// Replicates projectile transform, detonation and armor, and blends local prediction into server state.
// Uses a 5-bit mask and a 12-bit quaternion. Cached projectiles still consume deterministic network ids.

#include "Networkables.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../Projectiles/GenericProjectile.h"
#include "../Projectiles/GenericProjectileTemplate.h"
#include "../Projectiles/GrenadeTrace.h"
#include "../Projectiles/TVMissileTrace.h"
#include "../GameLogicComp/ArmorGLComp.h"
#include "../GameLogicComp/IArmorGLComp.h"            // predict's hit-point hand-over
#include "../IObjectTemplate.h"                       // createObject - the interpolation proxy
#include "../ObjectLayoutAccess.h"                    // + world::getRootParent via IObject.h
#include "../../../Core/Debug.h"
#include "../../../Core/Ids.h"
#include "../../../Core/StringUtil.h"                 // formatString - the Debug payload, as shipped
#include "../../../Core/System.h"                     // WorldPref::getTickCount, g_system->getExactTime
#include "../../../Core/WeakPtr.h"                    // the proxy / stand-in weak blocks
#include "../../../Game/Utility/Vars.h"               // the two local-prediction cvar handles
#include "../../../IO/BitStream.h"
#include "../../../IO/Network/NetworkManager.h"       // networkManager - the target-netId resolve
#include "../../../IO/Network/NetworkableInfo.h"      // getPredictionMode - predict's mode fork
#include "../../../Physics/IPhysicsNode.h"            // predict's typed physics calls
#include "../../../Physics/PointPhysicsNode.h"        // the apply arm's six zero-sets
#include "../../../Scene/Object/ObjectManager.h"      // objectManager - predict's two destroys
#include "../../../Math/Mat4.h"
#include "../../../Math/Quat.h"
#include "../../../Math/Vec3.h"
#include "../Fx/FxTrace.h"                           // [TEMP] BF2_TRACE_FX - the detonate receive

namespace dice {
namespace hfe {

// Client prediction timing in seconds: minimum local flight delay and interpolation handover duration.
namespace {

float* const g_localPredictionLerpTime = Vars::getFloat("local-prediction-lerpTime", 1.0f);
float* const g_localPredictionMinDelay = Vars::getFloat("local-prediction-minDelay", 0.15f);

Vec3 projectileStatsColour()
{
    Vec3 c; c.x = 0.0f; c.y = 250.0f; c.z = 0.0f;
    return c;
}

// getIsAttached - 0x8239440, 80 B, a TU-local free function in the ELF's own mangling
// (dice::hfe::getIsAttached(IPhysicsNode*, GenericProjectile*)). "Attached" = parented (stuck to
// something), or physics-less, or parked out of the physics world (separate-update AND disabled).
// setNetUpdate's apply arm refuses to teleport an attached projectile.
bool getIsAttached(world::IPhysicsNode* node, world::GenericProjectile* projectile)
{
    typedef world::object_detail::LayoutAccess L;
    if (L::parent(projectile))
        return true;
    if (!node)
        return true;
    return node->getHasSeparatePhysicsUpdate() && !node->getEnabled();
}

// Build the identity-rotation matrix a record describes: quat -> rotation, position -> cols 12..14.
Mat4 recordTransform(const GenericProjectileNetworkable::Record& record)
{
    Mat4 m = Mat4::getIdentity();
    record.rotation.toMat(m);
    m.m[12] = record.position.x;
    m.m[13] = record.position.y;
    m.m[14] = record.position.z;
    return m;
}

// Builds the rotation/position transform and applies it in parent-relative or world space.
// Position is passed by value. [INFERRED] Helper name.
void applyTransform(world::IObject* object, const Quat& rotation, Vec3 position)
{
    Mat4 m = Mat4::getIdentity();
    rotation.toMat(m);
    m.m[12] = position.x;
    m.m[13] = position.y;
    m.m[14] = position.z;
    if (object->getParent())
        object->setRelativeTransformation(m);        // slot 59 +0xEC
    else
        object->setAbsoluteTransformation(m);        // slot 35 +0x8C
}

// Zero all six motion channels of a physics node, in the shipped order, with a fresh zero vector
// per call. BF2.exe 0x636550 (209 B) is this helper out of line; its only two callers are
// predict's two hand-over arms, so it belongs to this file. [INFERRED] name.
void stopPhysicsNode(world::IPhysicsNode* node)
{
    Vec3 zero;
    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
    node->setPositionalSpeed(zero);                  // ELF +0x68
    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
    node->setPositionalAcceleration(zero);           // ELF +0x78
    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
    node->setRotationalSpeed(zero);                  // ELF +0x6C
    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
    node->setRotationalAcceleration(zero);           // ELF +0x7C
    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
    node->setRotationalFriction(zero);               // ELF +0x84
    zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
    node->setPositionalFriction(zero);               // ELF +0x80
}

// The nearest armor component up the parent chain (IObject +0x3C via the +0x28 links) - the walk
// GameLogicCollision.cpp and ClientGameLogic.cpp each carry under the same name. BF2.exe shares one
// out-of-line copy across its TUs (0x4DB1B0, 21 B, eleven callers).
world::IArmorGLComp* findArmor(world::IObject* start)
{
    for (world::IObject* o = start; o; o = o->getParent())
        if (world::IObjectComponent* c = o->getArmorComponent())
            return static_cast<world::IArmorGLComp*>(c);
    return 0;
}

// Reads the weak target through the native control-block layout.
world::GenericProjectile* weakProjectile(void* block)
{
    if (!block)
        return 0;
    return static_cast<world::GenericProjectile*>(
        static_cast<WeakPtr<world::IObject>::Ctrl*>(block)->target);
}

} // namespace

io::RegistredNetworkStats GenericProjectileNetworkable::m_stats("GenericProjectileNetworkable",
                                                                projectileStatsColour());

// Creates the requested interface and rejects aggregation.
// [CHOSEN] Prediction time is zeroed here before init/reset establishes -1.0f.
IBase* GenericProjectileNetworkable::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    GenericProjectileNetworkable* self = new GenericProjectileNetworkable();
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result)
        self->release();
    return result;
}

// [INFERRED] from create's stores - no separate ELF constructor exists (it is inlined there).
GenericProjectileNetworkable::GenericProjectileNetworkable()
{
    m_refCount = 1;                                  // +0x50
    m_records  = 0;                                  // +0x54
    m_predictTime = 0.0f;                            // +0x58 [CHOSEN] - see the create block
}

// D2/D1 0x8237780/0x82377D0, 65 B: revector, delete[] the ring AND null it, chain.
GenericProjectileNetworkable::~GenericProjectileNetworkable()
{
    if (m_records)
    {
        delete[] m_records;
        m_records = 0;
    }
}

void  GenericProjectileNetworkable::addRef() { ++m_refCount; }
int   GenericProjectileNetworkable::getRef() { return m_refCount; }
int   GenericProjectileNetworkable::release()
{
    const bool last = (m_refCount == 1);
    const int  r    = --m_refCount;
    if (last)
        delete this;
    return r;
}

// DON's merged-first-test shape.
void* GenericProjectileNetworkable::queryInterface(unsigned int iid) const
{
    GenericProjectileNetworkable* self = const_cast<GenericProjectileNetworkable*>(this);
    if (iid == IID_IBase || iid == io::IID_INetworkable)
        return static_cast<io::INetworkable*>(self);
    if (iid == io::IID_INetworkableObject)
        return static_cast<io::INetworkableObject*>(self);
    return 0;
}

// Seeds pose, detonation state, replicated fields, target state, and rounded armor health.
// Uses relative transformation when parented and absolute transformation otherwise.
#define BF2_GPN_PRIME_RECORD(record, projectile)                                                   \
    do                                                                                             \
    {                                                                                              \
        const Mat4& primeTransform = L::parent(projectile)                                         \
                                         ? (projectile)->getRelativeTransformation()               \
                                         : (projectile)->getAbsoluteTransformation();              \
        (record).rotation.fromMat(primeTransform);                                                 \
        (record).position.x = primeTransform.m[12];                                                \
        (record).position.y = primeTransform.m[13];                                                \
        (record).position.z = primeTransform.m[14];                                                \
        (record).detonate = (projectile)->getDetonate() ? 1 : 0;                                   \
        (record).field1C4 = (projectile)->m_dword1C4;                                              \
        (record).vector1CC = (projectile)->m_vector1CC;                                            \
        (record).hasTargetNetworkable = 0;                                                         \
        (record).targetNetworkableId = 0;                                                          \
        world::ArmorGLComp* const primeArmor =                                                     \
            static_cast<world::ArmorGLComp*>(L::armorComponent(projectile));                       \
        if (primeArmor)                                                                            \
            (record).hitPointsCeil = static_cast<int>(std::ceil(primeArmor->getHitPoints()));      \
    } while (0)

// init - 0x8237870, 530 B. Bind the projectile, cache the +0x18 interface, build the ring - four
// 80-byte records zeroed field-by-field with the quaternion seeded to identity (w = 1.0) - then
// m_next = 0, m_predictTime = -1.0f, and record 0 primed from the live object. Returns whether the
// ring allocated.
bool GenericProjectileNetworkable::init(void* object)
{
    m_object = static_cast<world::IObject*>(object);                              // +0x44
    m_networkableObject =
        static_cast<io::INetworkableObject*>(queryInterface(io::IID_INetworkableObject));   // +0x18

    Record* const ring = new Record[4];                                           // 0x140 B
    for (int i = 0; i < 4; ++i)
    {
        ring[i].time = 0.0f;
        ring[i].byte04 = false;
        ring[i].position.x = 0.0f; ring[i].position.y = 0.0f; ring[i].position.z = 0.0f;
        ring[i].rotation.x = 0.0f; ring[i].rotation.y = 0.0f; ring[i].rotation.z = 0.0f;
        ring[i].rotation.w = 1.0f;                                                // identity
        ring[i].detonate = 0;
        ring[i].field1C4 = 0;
        ring[i].vector1CC.x = 0.0f; ring[i].vector1CC.y = 0.0f; ring[i].vector1CC.z = 0.0f;
        ring[i].hitPointsCeil = 0;
        ring[i].hasTargetNetworkable = 0;
        ring[i].targetNetworkableId = 0;
        // predictVelocity (+0x38..+0x40) deliberately untouched - the ELF's loop skips it.
    }
    m_records = ring;                                                             // +0x54
    m_predictTime = -1.0f;                                                        // +0x58
    if (!ring)
        return false;

    m_next = 0;                                                                   // +0x38
    {
        typedef world::object_detail::LayoutAccess L;
        world::GenericProjectile* const projectile =
            static_cast<world::GenericProjectile*>(m_object);
        BF2_GPN_PRIME_RECORD(ring[0], projectile);
    }
    return true;
}

// the base reset, then m_next = 0, the predict stamp cleared, and slot 0
// RE-primed from the live object (the same fourteen stores as init's tail).
void GenericProjectileNetworkable::reset()
{
    typedef world::object_detail::LayoutAccess L;
    io::NetworkableBase::reset();
    m_next = 0;
    m_predictTime = -1.0f;
    world::GenericProjectile* const projectile =
        static_cast<world::GenericProjectile*>(m_object);
    BF2_GPN_PRIME_RECORD(m_records[m_next], projectile);
}

// the unique override: clear the predict stamp too.
void GenericProjectileNetworkable::setGhostActive(bool active)
{
    m_predictTime = -1.0f;                           // +0x58
    m_ghostActive = active;                          // +0x0C
}

unsigned int GenericProjectileNetworkable::getMinimumBitSize() const
{
    return 20;
}

// the house average, over the 80-byte stride.
float GenericProjectileNetworkable::getAverageUpdateDeltaTime() const
{
    if (m_count == 0)
        return 0.0f;
    return (m_records[m_head].time - m_records[m_tail].time) / static_cast<float>(m_count);
}

// the house ring search (byte-identical mechanics to SoldierNetworkable's).
bool GenericProjectileNetworkable::findEmbracingIndices(float t, unsigned int* lo,
                                                        unsigned int* hi) const
{
    if (static_cast<unsigned int>(m_count) <= 1u)
        return false;
    const unsigned int tail = static_cast<unsigned int>(m_tail);
    if (m_records[tail].time > t)
        return false;
    *lo = tail;
    *hi = tail;
    if (static_cast<unsigned int>(m_count) <= 1u)
        return false;
    for (unsigned int step = 1;;)
    {
        const unsigned int idx = (step + static_cast<unsigned int>(m_tail)) & 3;
        *hi = idx;
        if (m_records[idx].time >= t)
            return true;
        ++step;
        *lo = idx;
        if (step >= static_cast<unsigned int>(m_count))
            return false;
    }
}

// Snapshots projectile state and marks transform, detonation and armor changes.
// The object latch gates transform capture; detonation and armor checks run on every pass.
void GenericProjectileNetworkable::updateStateMask(unsigned int* stateMask)
{
    typedef world::object_detail::LayoutAccess L;
    world::GenericProjectile* const projectile =
        static_cast<world::GenericProjectile*>(m_object);
    const float eps = 1.1920929e-7f;

    m_prev = m_head;                                       // +0x34
    m_head = (m_head + 1) & 3;                             // +0x28
    m_next = m_head;                                       // +0x38

    Record* const rec  = &m_records[m_next];
    Record* const prev = &m_records[m_prev];

    // the ELF's `test byte ptr [object+7], 1` = flags & LayoutAccess's documented
    // bit 24, "is a root". A parented (stuck) projectile skips the whole transform half.
    if (L::flags(projectile) & 0x01000000u)
    {
        const Mat4& transform = L::parent(projectile)
                                    ? projectile->getRelativeTransformation()
                                    : projectile->getAbsoluteTransformation();
        rec->rotation.fromMat(transform);
        if (std::memcmp(&rec->rotation, &prev->rotation, 0x10) != 0)
            *stateMask |= 0x1;

        rec->position.x = transform.m[12];
        rec->position.y = transform.m[13];
        rec->position.z = transform.m[14];
        const bool same = std::fabs(rec->position.x - prev->position.x) <= eps
                       && std::fabs(rec->position.y - prev->position.y) <= eps
                       && std::fabs(rec->position.z - prev->position.z) <= eps;
        if (!same)
            *stateMask |= 0x2;
    }

    const bool detonate = projectile->getDetonate();
    rec->detonate = detonate ? 1 : 0;
    if (prev->detonate != rec->detonate)
        *stateMask |= 0x4;
    projectile->setDetonate(detonate);                     // the shipped read-back

    rec->hasTargetNetworkable = 0;
    if (detonate)
    {
        world::IObject* target = 0;
        if (projectile->m_targetObjectWeak)
            // the collision target's WeakPtr control.
            // Its target member is +4 in retail x86, +8 in the native x64 producer.
            target = static_cast<WeakPtr<world::IObject>::Ctrl*>(
                projectile->m_targetObjectWeak)->target;
        io::INetworkable* net = 0;
        if (target)
            net = world::getRootParent(target)->getNetworkable();
        if (net)
        {
            rec->hasTargetNetworkable = 1;
            rec->targetNetworkableId = net->m_networkableId;       // INetworkable +0x04
        }
    }

    rec->field1C4 = projectile->m_dword1C4;
    if (rec->field1C4 != prev->field1C4)
        *stateMask |= 0x4;

    rec->vector1CC = projectile->m_vector1CC;
    {
        const bool same = std::fabs(rec->vector1CC.x - prev->vector1CC.x) <= eps
                       && std::fabs(rec->vector1CC.y - prev->vector1CC.y) <= eps
                       && std::fabs(rec->vector1CC.z - prev->vector1CC.z) <= eps;
        if (!same)
            *stateMask |= 0x4;
    }

    world::ArmorGLComp* const armor =
        static_cast<world::ArmorGLComp*>(L::armorComponent(projectile));
    if (armor)
    {
        rec->hitPointsCeil = static_cast<int>(std::ceil(armor->getHitPoints()));
        if (rec->hitPointsCeil != prev->hitPointsCeil)
            *stateMask |= 0x10;
    }
    if (world::tv_missile_trace::armed())
        world::tv_missile_trace::log(projectile, "snapshot",
            "launch_tick=%d mask=%u root=%u record=%u pos=%.9g,%.9g,%.9g "
            "quat=%.9g,%.9g,%.9g,%.9g",
            projectile->m_dword1BC, *stateMask, (L::flags(projectile) & 0x01000000u) ? 1u : 0u,
            static_cast<unsigned int>(m_next), rec->position.x, rec->position.y, rec->position.z,
            rec->rotation.x, rec->rotation.y, rec->rotation.z, rec->rotation.w);
}

// getNetUpdate - 0x8238020, 1,725 B - THE wire, sender side. See the file header for the map.
// The BaseLineData pointer is unused - a projectile always encodes absolutely.
bool GenericProjectileNetworkable::getNetUpdate(io::BitStream& stream, io::BaseLineData* /*baseline*/,
                                                unsigned int stateMask, int /*precision*/)
{
    typedef world::object_detail::LayoutAccess L;

    m_updateStartBitPos = stream.getBitPosition();                    // +0x30
    m_next = m_head;                                                  // pin the record to send
    Record* const rec = &m_records[m_next];

    unsigned int mask = stateMask;
    stream.writeBits(&mask, 5);

    if (mask & 0x2)
    {
        if (stream.hasCompressionVector())
        {
            stream.writeCompressedVector(rec->position, stream.getCompressionVector(),
                                         0.0099999998f);
        }
        else
        {
            unsigned int selector = 0;                 // the hand-written selector-0 escape
            stream.writeBits(&selector, 2);
            stream.writeBits(&rec->position.x, 32);
            stream.writeBits(&rec->position.y, 32);
            stream.writeBits(&rec->position.z, 32);
        }
    }

    if (mask & 0x1)
        stream.writeUnitQuaternion(rec->rotation, 12);

    if (mask & 0x4)
    {
        unsigned int flag = rec->detonate ? 1u : 0u;
        stream.writeBits(&flag, 1);
    }
    if (mask & 0x4)
    {
        if (rec->detonate == 1)
        {
            const int value = rec->field1C4;
            if (value < 0)
            {
                unsigned int sign = 1;
                stream.writeBits(&sign, 1);
                unsigned int magnitude = static_cast<unsigned int>(-value);
                stream.writeBits(&magnitude, 15);
            }
            else
            {
                unsigned int sign = 0;
                stream.writeBits(&sign, 1);
                unsigned int magnitude = static_cast<unsigned int>(value);
                stream.writeBits(&magnitude, 15);
            }
            stream.writeHighCompressedVector(rec->vector1CC, Vec3::getZero(), 0.0099999998f);
            {
                unsigned int flag = rec->hasTargetNetworkable ? 1u : 0u;
                stream.writeBits(&flag, 1);
            }
            if (rec->hasTargetNetworkable)
            {
                unsigned int targetId = static_cast<unsigned int>(rec->targetNetworkableId);
                stream.writeBits(&targetId, 16);
            }
        }
    }

    world::ArmorGLComp* const armor =
        static_cast<world::ArmorGLComp*>(L::armorComponent(m_object));
    if (armor && (mask & 0x10))
    {
        // the DON five-call range idiom, in the shipped call order
        const int value = rec->hitPointsCeil + static_cast<int>(armor->getMaxWreckHitPoints());
        const int lower = static_cast<int>(-armor->getMaxWreckHitPoints())
                        + static_cast<int>( armor->getMaxWreckHitPoints());
        const int upper = static_cast<int>( armor->getMaxHitPoints())
                        + static_cast<int>( armor->getMaxWreckHitPoints());
        if (value >= lower && value <= upper)
        {
            const unsigned int range = static_cast<unsigned int>(upper - lower);
            unsigned int bits = 0;
            if (range != 0xFFFFFFFFu)
            {
                do
                    ++bits;
                while (range + 1 > (1u << bits) - 1);
            }
            unsigned int scratch = static_cast<unsigned int>(value - lower);
            stream.writeBits(&scratch, bits);
        }
        else
        {
            stream.setError(4);
            Debug(DEBUGTYPE_7, "IO/BitStream.inl", 59, "")
                << "(fatal error) Value out of bounds val: " << value
                << " lower " << lower << " upper: " << upper;
        }
    }

    if (stream.hasError())
    {
        stream.clearError();
        return false;
    }
    m_stats.m_writeBits += static_cast<int>(stream.getBitPosition() - m_updateStartBitPos);
    if (world::tv_missile_trace::armed())
        world::tv_missile_trace::log(m_object, "encode_candidate",
            "stream=%p mask=%u pos=%.9g,%.9g,%.9g quat=%.9g,%.9g,%.9g,%.9g "
            "bit_start=%u bit_end=%u",
            static_cast<void*>(&stream), mask, rec->position.x, rec->position.y, rec->position.z,
            rec->rotation.x, rec->rotation.y, rec->rotation.z, rec->rotation.w,
            m_updateStartBitPos, stream.getBitPosition());
    if (world::grenade_trace::armed() && world::grenade_trace::begin(m_object))
    {
        const Vec3& reference = stream.getCompressionVector();
        world::grenade_trace::log(m_object, "net_send",
            "mask=%u pos_requested=%u rot_requested=%u pos=%.9g,%.9g,%.9g "
            "quat=%.9g,%.9g,%.9g,%.9g record_time=%.9g "
            "compression=%u reference=%.9g,%.9g,%.9g bit_start=%u bit_end=%u",
            mask, (mask & 2u) ? 1u : 0u, (mask & 1u) ? 1u : 0u,
            rec->position.x, rec->position.y, rec->position.z,
            rec->rotation.x, rec->rotation.y, rec->rotation.z, rec->rotation.w,
            rec->time, stream.hasCompressionVector() ? 1u : 0u,
            reference.x, reference.y, reference.z,
            m_updateStartBitPos, stream.getBitPosition());
    }
    return true;
}

// Decodes and applies projectile state, preserving omitted fields.
// Full unattached updates reset transform and physics; detonation resolves the target and applies impact state.
void GenericProjectileNetworkable::setNetUpdate(io::BitStream& stream, float updateTime,
                                                bool ghostActive, io::BaseLineData* /*baseline*/,
                                                int /*precision*/)
{
    typedef world::object_detail::LayoutAccess L;
    world::GenericProjectile* const projectile =
        static_cast<world::GenericProjectile*>(m_object);
    world::IPhysicsNode* const physicsNode =
        static_cast<world::IPhysicsNode*>(L::physicsNode(projectile));

    m_updateStartBitPos = stream.getBitPosition();                    // +0x30

    const bool isFullUpdate = makeRoomForNextUpdate(ghostActive);
    Record* const rec = &m_records[m_next];
    if (m_prev != m_next)
        *rec = m_records[m_prev];                                     // the 80-byte carry-forward
    rec->time = updateTime;

    unsigned int mask = 0;
    stream.readBits(&mask, 5);

    if (mask & 0x2)
        rec->position = stream.readCompressedVector(stream.getCompressionVector(), 0.0099999998f);

    if (mask & 0x1)
        rec->rotation = stream.readUnitQuaternion(12);

    if (mask & 0x4)
    {
        unsigned int flag = 0;
        stream.readBits(&flag, 1);
        rec->detonate = (flag == 1);
    }
    if ((mask & 0x4) && rec->detonate == 1)
    {
        unsigned int sign = 0;
        stream.readBits(&sign, 1);
        if (sign == 1)
        {
            unsigned int magnitude = 0;
            stream.readBits(&magnitude, 15);
            rec->field1C4 = -static_cast<int>(magnitude);
        }
        else
        {
            unsigned int magnitude = 0;
            stream.readBits(&magnitude, 15);
            rec->field1C4 = static_cast<int>(magnitude);
        }
        rec->vector1CC = stream.readHighCompressedVector(Vec3::getZero(), 0.0099999998f);
        unsigned int hasTarget = 0;
        stream.readBits(&hasTarget, 1);
        rec->hasTargetNetworkable = (hasTarget == 1);
        if (rec->hasTargetNetworkable)
        {
            unsigned int targetId = 0;
            stream.readBits(&targetId, 16);
            rec->targetNetworkableId = static_cast<int>(targetId);
        }
    }

    world::ArmorGLComp* const armor =
        static_cast<world::ArmorGLComp*>(L::armorComponent(projectile));
    if (armor && (mask & 0x10))
    {
        // the mirror of the sender's five-call bounds, in the shipped call order
        const int lower = static_cast<int>(-armor->getMaxWreckHitPoints())
                        + static_cast<int>( armor->getMaxWreckHitPoints());
        const int range = static_cast<int>( armor->getMaxHitPoints())
                        + static_cast<int>( armor->getMaxWreckHitPoints()) - lower;
        unsigned int bits = 0;
        if (range != -1)
        {
            do
                ++bits;
            while (static_cast<unsigned int>(range) + 1 > (1u << bits) - 1);
        }
        unsigned int raw = 0;
        stream.readBits(&raw, bits);
        rec->hitPointsCeil = static_cast<int>(raw) + lower
                           - static_cast<int>(armor->getMaxWreckHitPoints());
    }

    if (stream.hasError())
    {
        // attested AND fatal - see the header block. Note the capital reading.
        Debug(DEBUGTYPE_6, "Game/Objects/Networkables/GenericProjectileNetworkable.cpp", 265,
              __FUNCTION__) << formatString("error READING data");
        stream.clearError();
        return;
    }

    m_stats.m_readBits += static_cast<int>(stream.getBitPosition() - m_updateStartBitPos);
    if (world::grenade_trace::armed() && world::grenade_trace::begin(m_object))
    {
        const Vec3& reference = stream.getCompressionVector();
        world::grenade_trace::log(m_object, "net_receive",
            "mask=%u pos_requested=%u rot_requested=%u pos=%.9g,%.9g,%.9g "
            "quat=%.9g,%.9g,%.9g,%.9g record_time=%.9g full=%u "
            "compression=%u reference=%.9g,%.9g,%.9g bit_start=%u bit_end=%u",
            mask, (mask & 2u) ? 1u : 0u, (mask & 1u) ? 1u : 0u,
            rec->position.x, rec->position.y, rec->position.z,
            rec->rotation.x, rec->rotation.y, rec->rotation.z, rec->rotation.w,
            rec->time, isFullUpdate ? 1u : 0u, stream.hasCompressionVector() ? 1u : 0u,
            reference.x, reference.y, reference.z,
            m_updateStartBitPos, stream.getBitPosition());
    }

    // THE apply
    if (isFullUpdate)
    {
        m_predictTime = -1.0f;                                        // +0x58
        if (!getIsAttached(physicsNode, projectile))
        {
            const Mat4 transform = recordTransform(*rec);
            if (L::parent(projectile))
                projectile->setRelativeTransformation(transform);
            else
                projectile->setAbsoluteTransformation(transform);

            if (physicsNode)
            {
                world::PointPhysicsNode* const node =
                    static_cast<world::PointPhysicsNode*>(physicsNode);
                Vec3 zero;
                zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
                node->setPositionalSpeed(zero);                       // +0x68
                node->setPositionalAcceleration(zero);                // +0x78
                node->setRotationalSpeed(zero);                       // +0x6C
                node->setRotationalAcceleration(zero);                // +0x7C
                node->setRotationalFriction(zero);                    // +0x84
                node->setPositionalFriction(zero);                    // +0x80
            }
        }
    }

    if ((mask & 0x4) && rec->detonate == 1)
    {
        if (physicsNode)
        {
            if (!getIsAttached(physicsNode, projectile))
            {
                const Mat4 transform = recordTransform(*rec);
                if (L::parent(projectile))
                    projectile->setRelativeTransformation(transform);
                else
                    projectile->setAbsoluteTransformation(transform);
            }
            projectile->m_dword1C4 = rec->field1C4;                   // +0x1C4
            projectile->m_vector1CC = rec->vector1CC;                 // +0x1CC
            projectile->m_tick1C8 = WorldPref::getTickCount();        // +0x1C8 the detonate stamp

            if (rec->hasTargetNetworkable)
            {
                world::IObject* const target = io::networkManager->getObject(
                    static_cast<unsigned short>(rec->targetNetworkableId));
                if (target)
                {
                    // publish the target's weak pointer into projectile +0x1E4, dropping any old one
                    WeakPtr<world::IObject> weak = target->getWeakPtr();
                    void* const block = weak.m_ctrl;
                    if (block)
                        ++*static_cast<int*>(block);
                    void* const old = projectile->m_targetObjectWeak;
                    if (old)
                    {
                        int* refCount = static_cast<int*>(old);
                        if ((*refCount)-- == 1)
                            ::operator delete(old);
                    }
                    projectile->m_targetObjectWeak = block;
                }
            }
        }
        // [TEMP] [RETIRE-WHEN: fx-explosion-diagnosed] - FxTrace.h
        BF2_FX_TRACE("net detonate: %s tick=%u material=%d target=%d physicsNode=%d",
                     projectile->getTemplate()->getName().c_str(), WorldPref::getTickCount(),
                     rec->field1C4, rec->hasTargetNetworkable ? 1 : 0, physicsNode ? 1 : 0);
        projectile->detonate(true);                                   // +0x184 the receive trigger
    }

    if (armor && (mask & 0x10))
        armor->setHitPoints(static_cast<float>(rec->hitPointsCeil));
}

// Predicts the networked projectile at time and blends a local projectile into that trajectory.
// Attached rounds retire the local stand-in; other rounds wait for the configured delay and blend duration.
// Linear prediction interpolates or extrapolates available records; the stand-in requires armor.
void GenericProjectileNetworkable::predict(float time)
{
    typedef world::object_detail::LayoutAccess L;

    world::GenericProjectile* const projectile =
        static_cast<world::GenericProjectile*>(m_object);                          // +0x44
    world::GenericProjectile* const localProjectile =
        static_cast<world::GenericProjectile*>(projectile->getLocalPredictedProjectile());
    world::IPhysicsNode* const physicsNode =
        static_cast<world::IPhysicsNode*>(L::physicsNode(projectile));
    m_next = m_head;

    if (getIsAttached(physicsNode, projectile))
    {
        if (localProjectile)
            world::objectManager->destroyObject(localProjectile);                  // +0x14
        projectile->updateFlags(2u, 0);
        return;
    }

    (void)findArmor(projectile);                     // the dead walk - see step 2

    if (m_forceFullUpdate)
        return;

    if (localProjectile)
    {
        projectile->stopEffects();
        projectile->updateFlags(0, 0x100u);
        projectile->updateFlags(0, 2u);
    }
    else
    {
        projectile->updateFlags(2u, 0);
    }

    if (localProjectile)
    {
        const float now = static_cast<float>(g_system->getExactTime());           // ELF +0x18
        if (*g_localPredictionMinDelay + localProjectile->m_exactLaunchTime > now)
            return;
    }

    const float lerpTime = 1000.0f * *g_localPredictionLerpTime;                  // ms
    if (m_predictTime < 0.0f)
    {
        m_predictTime = time;
        if (localProjectile)
        {
            // WeakPtr assignment in GenericProjectile's raw-block idiom (setLocalPredictedProjectile
            // spells it the same way): retain the incoming block, release the held one, store; the
            // temporary's destructor then drops the reference getWeakPtr handed back. BF2.exe does
            // the same through its out-of-line WeakPtr::operator= (0x464B90).
            WeakPtr<world::IObject> proxyWeak =
                createInterpolationProxy(localProjectile)->getWeakPtr();
            WeakPtr<world::IObject>::Ctrl* const incoming = proxyWeak.m_ctrl;
            if (incoming)
                ++incoming->refCount;
            WeakPtr<world::IObject>::Ctrl* const held =
                static_cast<WeakPtr<world::IObject>::Ctrl*>(localProjectile->m_interpolationProxyWeak);
            if (held && held->refCount-- == 1)
                ::operator delete(held);
            localProjectile->m_interpolationProxyWeak = incoming;
        }
    }
    const float blend = (time - m_predictTime) / lerpTime;

    switch (m_networkableInfo->getPredictionMode())                                // vptr +0x24
    {
    case io::PMNone:
    {
        const Record& rec = m_records[m_next];
        applyTransform(projectile, rec.rotation, rec.position);
        if (physicsNode)                             // the PE drops this test: step 1 proved it
            stopPhysicsNode(physicsNode);
        break;
    }

    case io::PMLinear:
    {
        const double predictTime = static_cast<double>(time)
            - static_cast<double>(
                  static_cast<unsigned int>(io::networkManager->getInterpolationTime()));   // +0x50
        float deltaTime = 0.0f;

        if (physicsNode && physicsNode->getIsMobile())                             // ELF +0x134
        {
            Record& rec = m_records[m_next];
            if (static_cast<unsigned int>(m_count) > 1u)
            {
                const Record& prev = m_records[(static_cast<unsigned int>(m_next) + 3) & 3];
                deltaTime = rec.time - prev.time;
                if (deltaTime > 0.0f)
                {
                    const float perSecond = 1.0f / (deltaTime / 1000.0f);
                    rec.predictVelocity.x = (rec.position.x - prev.position.x) * perSecond;
                    rec.predictVelocity.y = (rec.position.y - prev.position.y) * perSecond;
                    rec.predictVelocity.z = (rec.position.z - prev.position.z) * perSecond;
                }
                else
                {
                    rec.predictVelocity.x = 0.0f;
                    rec.predictVelocity.y = 0.0f;
                    rec.predictVelocity.z = 0.0f;
                }
            }
            else
            {
                rec.predictVelocity.x = 0.0f;
                rec.predictVelocity.y = 0.0f;
                rec.predictVelocity.z = 0.0f;
            }

            physicsNode->setPositionalSpeed(rec.predictVelocity);                  // ELF +0x68
            Vec3 zero;
            zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
            physicsNode->setPositionalAcceleration(zero);                          // ELF +0x78
            zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
            physicsNode->setRotationalSpeed(zero);                                 // ELF +0x6C
            zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
            physicsNode->setRotationalAcceleration(zero);                          // ELF +0x7C
            zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
            physicsNode->setRotationalFriction(zero);                              // ELF +0x84
            zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
            physicsNode->setPositionalFriction(zero);                              // ELF +0x80
        }

        const Record& rec = m_records[m_next];
        if (predictTime > rec.time)
        {
            // extrapolate, at most extrapolationTime ms past the newest record
            const double over = predictTime - rec.time;
            const unsigned int extrapolationTime =
                static_cast<unsigned int>(io::networkManager->getExtrapolationTime());   // +0x54
            if (static_cast<double>(extrapolationTime) >= over)
            {
                if (deltaTime == 0.0f)
                    deltaTime = static_cast<float>(extrapolationTime);
                const float fraction =
                    (static_cast<float>(predictTime) - rec.time) / deltaTime;
                const float span = deltaTime / 1000.0f;
                Vec3 position;
                position.x = rec.position.x + rec.predictVelocity.x * span * fraction;
                position.y = rec.position.y + rec.predictVelocity.y * span * fraction;
                position.z = rec.position.z + rec.predictVelocity.z * span * fraction;
                applyTransform(projectile, rec.rotation, position);
                break;
            }
        }
        else
        {
            // interpolate between the two records that bracket predictTime
            unsigned int lo;
            unsigned int hi;
            if (deltaTime < 500.0f
                && findEmbracingIndices(static_cast<float>(predictTime), &lo, &hi))
            {
                deltaTime = m_records[hi].time - m_records[lo].time;
                if (deltaTime > 0.0f && deltaTime < 500.0f)
                {
                    const float fraction = (deltaTime <= 1.0f)
                        ? 1.0f
                        : (static_cast<float>(predictTime) - m_records[lo].time) / deltaTime;

                    Quat rotation;
                    rotation.x = 0.0f; rotation.y = 0.0f; rotation.z = 0.0f; rotation.w = 1.0f;
                    rotation.slerp(m_records[lo].rotation, m_records[hi].rotation, fraction);

                    const Vec3& from = m_records[lo].position;
                    const Vec3& to   = m_records[hi].position;
                    Vec3 position;
                    position.x = from.x + (to.x - from.x) * fraction;
                    position.y = from.y + (to.y - from.y) * fraction;
                    position.z = from.z + (to.z - from.z) * fraction;
                    applyTransform(projectile, rotation, position);
                    break;
                }
            }
        }

        // too far past the newest record, no bracket, or a degenerate gap: snap to the newest
        applyTransform(projectile, rec.rotation, rec.position);
        break;
    }

    case io::PMCubic:
    case io::PMUsePhysics:
        break;
    }

    if (!localProjectile)
        return;

    if (blend < 1.0f)
    {
        Quat standInRotation;
        standInRotation.x = 0.0f; standInRotation.y = 0.0f; standInRotation.z = 0.0f;
        standInRotation.w = 1.0f;
        Vec3 proxyPosition;
        proxyPosition.x = 0.0f; proxyPosition.y = 0.0f; proxyPosition.z = 0.0f;

        Quat networkRotation;
        networkRotation.x = 0.0f; networkRotation.y = 0.0f; networkRotation.z = 0.0f;
        networkRotation.fromMat(projectile->getAbsoluteTransformation());

        Quat rotation;
        rotation.x = 0.0f; rotation.y = 0.0f; rotation.z = 0.0f; rotation.w = 1.0f;

        world::GenericProjectile* const proxy =
            weakProjectile(localProjectile->m_interpolationProxyWeak);
        if (proxy)
        {
            proxyPosition = proxy->getAbsolutePosition();                          // ELF +0x6C
            standInRotation.fromMat(localProjectile->getAbsoluteTransformation()); // the stand-IN's
            proxy->stopEffects();
            proxy->setPlayedImpactEffectAtTick(WorldPref::getTickCount());
        }

        rotation.slerp(standInRotation, networkRotation, blend);

        const Vec3& networkPosition = projectile->getAbsolutePosition();
        Vec3 position;
        position.x = proxyPosition.x + (networkPosition.x - proxyPosition.x) * blend;
        position.y = proxyPosition.y + (networkPosition.y - proxyPosition.y) * blend;
        position.z = proxyPosition.z + (networkPosition.z - proxyPosition.z) * blend;

        if (world::IPhysicsNode* const node =
                static_cast<world::IPhysicsNode*>(L::physicsNode(localProjectile)))
            stopPhysicsNode(node);
        applyTransform(localProjectile, rotation, position);
    }
    else
    {
        localProjectile->setAbsoluteTransformation(projectile->getAbsoluteTransformation());
        // the block is read twice, with its own repeated null test, to produce the argument -
        // the same double read setLocalPredictedProjectile carries (ELF 0x8239B27 / 0x8239B3C).
        if (weakProjectile(localProjectile->m_interpolationProxyWeak))
            world::objectManager->destroyObject(
                weakProjectile(localProjectile->m_interpolationProxyWeak));
        if (world::IPhysicsNode* const node =
                static_cast<world::IPhysicsNode*>(L::physicsNode(localProjectile)))
            stopPhysicsNode(node);
        localProjectile->setAbsoluteTransformation(projectile->getAbsoluteTransformation());
    }

    // The hit points follow the networked round. Only the source walk is null-checked; the
    // stand-in's result is dereferenced as found (ELF 0x8239AD3, PE 0x6379DF).
    world::IArmorGLComp* const source = findArmor(projectile);
    if (!source)
        return;
    world::IArmorGLComp* const target = findArmor(localProjectile);
    target->setHitPoints(source->getHitPoints());                                  // +0x38 +0x3C
}

// Creates a hidden local projectile that continues the stand-in's trajectory during interpolation.
// Copies its template, transform, velocity and player, disables effects and returns the new object.
world::GenericProjectile* GenericProjectileNetworkable::createInterpolationProxy(
    world::GenericProjectile* projectile)
{
    typedef world::object_detail::LayoutAccess L;

    world::GenericProjectile* const proxy =
        static_cast<world::GenericProjectile*>(projectile->getTemplate()->createObject());   // +0x54
    proxy->setLocalPredict(true);                                                  // +0x280
    proxy->init();                                                                 // +0x20
    if (world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(L::physicsNode(proxy)))
        node->setIsAwake();                                                        // ELF +0x130
    proxy->setTeam(projectile->getTeam());                                         // +0x1B8 +0x1BC
    proxy->activate();                                                             // +0x18C
    proxy->stopEffects();

    world::IPhysicsNode* const proxyNode =
        static_cast<world::IPhysicsNode*>(L::physicsNode(proxy));
    world::IPhysicsNode* const sourceNode =
        static_cast<world::IPhysicsNode*>(L::physicsNode(projectile));
    if (proxyNode && sourceNode)
    {
        proxyNode->reset(true);                                                    // ELF +0x118
        proxyNode->setPositionalSpeed(sourceNode->getPositionalSpeed());           // +0x68 +0x60
    }

    proxy->setAbsoluteTransformation(projectile->getAbsoluteTransformation());     // +0x8C +0x84
    proxy->updateFlags(2u, 0x402u);
    proxy->setPlayer(projectile->getPlayer());                                     // +0x1A8 +0x1AC
    proxy->m_byte206 = false;                                                      // ELF +0x206
    return proxy;
}

} // namespace hfe
} // namespace dice
