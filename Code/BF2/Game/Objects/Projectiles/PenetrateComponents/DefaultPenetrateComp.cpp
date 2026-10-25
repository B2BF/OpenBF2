// Material damageLoss == -1 refuses penetration.
// Random penetration decisions use a separate stream from ricochet.

#include "DefaultPenetrateComp.h"

#include <cfloat>       // FLT_EPSILON
#include <cmath>
#include <sstream>
#include <string>

#include "../ProjectileCompTemplates.h"
#include "../GenericProjectile.h"
#include "../../IObject.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../IGameLogic.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Physics/ICollisionMesh.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Physics/Material.h"
#include "../../../../Physics/MaterialManager.h"
#include "../../../../Scene/Object/ObjectManager.h"

namespace dice {
namespace hfe {
namespace world {

// THE global IS defined AS `IBase*` in Core/Globals.cpp, and that spelling is the one the
// linker sees - MSVC encodes a data symbol's type into its decorated name, so declaring it as
// `MaterialManager*` here would silently be a different symbol. Declare it as it is defined
// and downcast at the use site; MaterialManager.h gives the downcast a real type to land on.
extern IBase* materialManager;                       // ds:0x8AD2820 - Core/Globals.cpp

inline MaterialManager* materials()
{
    return static_cast<MaterialManager*>(materialManager);
}

namespace {

// Penetration's private random stream, separate from ricochet (RW-68).
// Preserve its tick-derived seed and combined multiplicative step; sharing Random would change deviation draws.
int g_penetrateRandomState = 0;                      // ds:0x8AB0700

int penetrateRandomStep(int seed)
{
    int state = (((24112 * seed) >> 16) + 24112 * ((seed & 0x7FFFFFFF) >> 16)) >> 15;
    state += (((24112 * (seed & 0x7FFFFFFF)) >> 16) & 0x7FFF) << 16;
    state += 24112 * seed;
    if (state - 0x7FFFFFFF >= 0)
        state -= 0x7FFFFFFF;

    int next = ((((26143 * state) >> 16) + 26143 * (state >> 16)) >> 15);
    next += ((((26143 * state) >> 16) & 0x7FFF) << 16);
    next += 26143 * state;
    if (next - 0x7FFFFFFF >= 0)
        next -= 0x7FFFFFFF;

    return next;
}

// The signed deviation the cone uses: centred on zero, scaled by the material's deviation.
float penetrateRandomDeviation(int state, float scale)
{
    const float raw = static_cast<float>(((state >> 7) | 1) + 1);
    return (0.5f - raw * 5.9604645e-8f) * scale;
}

// Renormalise in place with the epsilon guard this tree uses everywhere: a length-squared already
// within FLT_EPSILON of 1 is left alone, a length-squared under FLT_EPSILON collapses to zero.
void normaliseInPlace(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (fabsf(lengthSq - 1.0f) < FLT_EPSILON)
        return;

    if (fabsf(lengthSq) < FLT_EPSILON)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return;
    }

    const float inverse = 1.0f / sqrtf(lengthSq);
    v.x *= inverse;
    v.y *= inverse;
    v.z *= inverse;
}

Vec3 cross(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = a.y * b.z - a.z * b.y;
    out.y = a.z * b.x - a.x * b.z;
    out.z = a.x * b.y - a.y * b.x;
    return out;
}

} // namespace

// Construction / the leaf boilerplate.
DefaultPenetrateComp::DefaultPenetrateComp(DefaultPenetrateCompTemplate* objectTemplate)
    : m_refCount(1)
    , m_template(objectTemplate)
{
    ProjectileComp::m_refCount = 1;
    m_object = 0;
    m_initObject = 0;
}

DefaultPenetrateComp::~DefaultPenetrateComp() {}

void DefaultPenetrateComp::addRef() { ++m_refCount; }
int  DefaultPenetrateComp::getRef() { return m_refCount; }

int DefaultPenetrateComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* DefaultPenetrateComp::queryInterface(unsigned int iid) const
{
    DefaultPenetrateComp* const self = const_cast<DefaultPenetrateComp*>(this);

    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IProjectileComp || iid == IID_IProjectilePenetrateComp)
        return static_cast<IProjectilePenetrateComp*>(self);
    return 0;
}

unsigned int DefaultPenetrateComp::getInterfaceID() const
{
    return IID_IProjectilePenetrateComp;
}

bool DefaultPenetrateComp::hasUpdate() const { return false; }
void DefaultPenetrateComp::updateComponent(float, unsigned int) {}
void DefaultPenetrateComp::activate() {}

bool DefaultPenetrateComp::penetrate(IObject* target, const Vec3& direction,
                                     int a, int collisionType,
                                     float damageScale, float minDamage, float timeScale,
                                     float& outTime, Vec3& outPosition, float& outDamage)
{
    typedef object_detail::LayoutAccess L;

    IObject* const projectile = m_initObject;
    Material* const material =
        materials()->getMaterialPtr(static_cast<unsigned int>(collisionType));

    // A NULL target IS only allowed through FOR water. Shooting at nothing is how the water
    // surface itself arrives here, and the material is what says so.
    if (!target && !material->getHasWaterPhysics())
        return false;

    // THE three gates, skipped wholesale when the material overrides them.
    if (!material->getOverrideNeverPenetrate())
    {
        if (material->getHasWaterPhysics() && !m_template->getCanPenetrateWater())
            return false;
        if (material->getHasWaterPhysics() && m_template->getCanPenetrateWater())
            return true;        // straight through, no cost, no probe
        if (!material->getHasWaterPhysics() && !m_template->getCanPenetrateSolid())
            return false;
    }

    const float damageLoss = material->getDamageLoss();
    if (damageLoss == -1.0f)    // the refusal sentinel
        return false;

    const Vec3& current = projectile->getAbsolutePosition();
    outDamage = static_cast<GenericProjectile*>(projectile)->getDamage(current) * damageScale;
    if (outDamage < minDamage * 0.1f)
        return false;

    // The entry point: the projectile's position offset by the caller's direction.
    Vec3 entry;
    entry.x = direction.x + current.x;
    entry.y = direction.y + current.y;
    entry.z = direction.z + current.z;

    // THE basis comes OUT OF THE projectile'S OWN transform, and the shipped code zeroes the whole
    // 64-byte block before copying the matrix in - four rows of four, including the translation.
    Mat4 transform = projectile->getAbsoluteTransformation();

    IPhysicsNode* const physics = static_cast<IPhysicsNode*>(L::physicsNode(projectile));
    const Vec3& velocity = physics->getPositionalSpeed();
    const float speed = sqrtf(velocity.x * velocity.x +
                              velocity.y * velocity.y +
                              velocity.z * velocity.z);

    const float deviation = material->getPenetrationDeviation();

    // TWO draws, ONE cone. The first perturbs along the transform's second row, the second along
    // its first; the result is renormalised, crossed twice to rebuild an orthonormal basis, and
    // finally rescaled to the incoming speed.
    const unsigned int tick = static_cast<unsigned int>(WorldPref::getTickCount());
    const int first = penetrateRandomStep(static_cast<int>(tick % 0x3FF));
    const int second = penetrateRandomStep(first);
    g_penetrateRandomState = second;

    const float deviationA = penetrateRandomDeviation(first, deviation);
    const float deviationB = penetrateRandomDeviation(second, deviation);

    // THE cone IS built ON THE projectile'S OWN frame. Mat4 is a flat float[16] in this tree, so
    // the rows are m[0..3], m[4..7], m[8..11], m[12..15]. Row 2 is the direction of travel; rows 0
    // and 1 are the two axes the deviation is spread across.
    Vec3 forward;
    forward.x = transform.m[0] * deviationA + transform.m[4] * deviationB + transform.m[8];
    forward.y = transform.m[1] * deviationA + transform.m[5] * deviationB + transform.m[9];
    forward.z = transform.m[2] * deviationA + transform.m[6] * deviationB + transform.m[10];
    normaliseInPlace(forward);

    // rebuild THE frame around THE NEW direction - cross with the OLD row 1 first, then cross
    // back. Each result is renormalised with the same epsilon guard.
    Vec3 right;
    {
        Vec3 oldRow1;
        oldRow1.x = transform.m[4];
        oldRow1.y = transform.m[5];
        oldRow1.z = transform.m[6];
        right = cross(oldRow1, forward);
    }
    normaliseInPlace(right);

    Vec3 up = cross(forward, right);
    normaliseInPlace(up);

    transform.m[0] = right.x;   transform.m[1] = right.y;   transform.m[2] = right.z;
    transform.m[4] = up.x;      transform.m[5] = up.y;      transform.m[6] = up.z;
    transform.m[8] = forward.x; transform.m[9] = forward.y; transform.m[10] = forward.z;

    Vec3 exitVelocity;
    exitVelocity.x = forward.x * speed;
    exitVelocity.y = forward.y * speed;
    exitVelocity.z = forward.z * speed;

    projectile->setAbsoluteTransformation(transform);
    physics->setPositionalSpeed(exitVelocity);

    // the geometry probe
    Vec3 hitNormal;
    Vec3 hitPosition;
    float hitDistance = 0.0f;
    int hitMaterial = -1;
    float planeDistance = 0.0f;
    // CollisionFaceInfo's layout is intentionally opaque. The shipped caller reserves four
    // dwords, clears only the first, and passes the same storage to getDistance and doImpact.
    unsigned int faceInfoStorage[4];
    faceInfoStorage[0] = 0;
    CollisionFaceInfo* const faceInfo =
        reinterpret_cast<CollisionFaceInfo*>(faceInfoStorage);
    bool probed = false;
    bool hasGeometry = false;

    Vec3 exit;

    if (!target)
    {
        // No target at all: the exit point is nudged one tenth along the new direction.
        exit.x = forward.x * 0.1f + entry.x;
        exit.y = forward.y * 0.1f + entry.y;
        exit.z = forward.z * 0.1f + entry.z;
    }
    else
    {
        int lod = 1;
        if (physics->getOtherCollisionLod() >= 0)
            lod = physics->getOtherCollisionLod();

        if (!target->hasCollisionLod(lod))
            return false;

        hasGeometry = true;
        ICollisionMesh* const geometry =
            static_cast<ICollisionMesh*>(L::collisionMesh(target));

        // 2.1 x the collision radius is the sweep length - deliberately over-long so the probe
        // exits the far side of anything the round could plausibly cross.
        const float halfExtent = geometry->getBoundingRadius(lod) * 2.0999999f;
        Vec3 extent;
        extent.x = halfExtent * forward.x;
        extent.y = halfExtent * forward.y;
        extent.z = halfExtent * forward.z;

        float cost;
        if (material->getIsOneSided())
        {
            // A ONE-sided material skips THE probe entirely and pays the flat minimum.
            cost = material->getMinDamageLoss();
            exit.x = forward.x * 0.050000001f + entry.x;
            exit.y = forward.y * 0.050000001f + entry.y;
            exit.z = forward.z * 0.050000001f + entry.z;
        }
        else if (geometry->getDistance(lod, false,
                                      target->getAbsoluteTransformation(),
                                      target->getInvAbsoluteTransformation(),
                                      entry, extent,
                                      hitNormal, hitPosition,
                                      planeDistance, hitDistance, &hitMaterial,
                                      faceInfo, false, false))
        {
            // THE thickness actually crossed, clamped into the material's own window.
            const float thickness = fabsf(halfExtent + hitDistance) * damageLoss;
            float clamped = material->getMaxDamageLoss();
            if (clamped > thickness)
                clamped = thickness;
            cost = material->getMinDamageLoss();
            if (cost <= clamped)
                cost = clamped;

            probed = true;
            exit.x = forward.x * 0.0099999998f + hitPosition.x;
            exit.y = forward.y * 0.0099999998f + hitPosition.y;
            exit.z = forward.z * 0.0099999998f + hitPosition.z;
        }
        else
        {
            // A failed exit probe pays the material's maximum loss; with the 100000 default the
            // round stops here.
            cost = material->getMaxDamageLoss();
            exit.x = forward.x * 0.050000001f + entry.x;
            exit.y = forward.y * 0.050000001f + entry.y;
            exit.z = forward.z * 0.050000001f + entry.z;
        }

        outDamage -= cost;
    }

    // THE floor IS checked after THE debit, and it is the caller's minimum scaled by 0.1 - the
    // same threshold the early test used, so a round that only just qualified can still be stopped
    // by the material it just crossed.
    if (minDamage * 0.1f > outDamage)
        return false;

    // The exit point becomes the stored translation, so the round resumes just past the far
    // face. Left at the end of this tick's sweep, it would skip everything behind the surface.
    transform.m[12] = exit.x;
    transform.m[13] = exit.y;
    transform.m[14] = exit.z;
    projectile->setAbsoluteTransformation(transform);
    outPosition = exit;

    // The remaining time-to-live, posted to the object manager as TemplateMessage 29.
    const float remaining = timeScale * damageScale +
        static_cast<GenericProjectile*>(projectile)->getWorldLaunchTime() -
        static_cast<float>(WorldPref::getWorldTime());
    objectManager->postMessage(projectile, static_cast<TemplateMessage>(29), remaining);

    outTime = static_cast<float>(g_system->getExactTime());

    // THE impact IS only reported when THE probe actually found AN exit face. A one-sided
    // material or a failed sweep penetrates silently - no doImpact, no effect, no damage event.
    // The impact direction is the negated new forward, and the material id falls back to the
    // caller's collisionType when the probe did not report one.
    if (probed && hasGeometry)
    {
        Vec3 impactDirection;
        impactDirection.x = forward.x * -1.0f;
        impactDirection.y = forward.y * -1.0f;
        impactDirection.z = forward.z * -1.0f;

        IProjectileObject* const asProjectile = projectile
            ? static_cast<IProjectileObject*>(static_cast<GenericProjectile*>(projectile))
            : 0;

        const int impactMaterial = (hitMaterial == -1) ? collisionType : hitMaterial;

        // the eleventh argument (doImpact's effectRotation) is
        // a zero Vec3 in both images, not the impact direction. BF2.exe 0x61A00C..0x61A017 pushes
        // `false` and stores 0, 0, 0 into the by-value Vec3 before the call at 0x61A06C; the ELF
        // (0x829AF2E..0x829AF63) zeroes [ebp-108h..-100h] and passes its address - the Itanium
        // invisible reference - with ecx = 0 for the bool. Harmless while doImpact was empty; with
        // the client body a non-zero value rotates every penetration exit effect by
        // setRotationZXY(-dir.z, -dir.y, -dir.x) degrees.
        const Vec3 noRotation = { 0.0f, 0.0f, 0.0f };
        gameLogic->doImpact(a, impactMaterial,
                            static_cast<MaterialCollisionType>(2),
                            hitPosition, hitNormal, impactDirection,
                            projectile, target, asProjectile,
                            faceInfo,
                            noRotation, false);
    }

    return true;
}

// both permission bytes default TO true - the opposite of DefaultCollisionCompTemplate's two,
// which both start false. A projectile that says nothing about penetration may cross both solids
// and water; the material is what usually stops it.
DefaultPenetrateCompTemplate::DefaultPenetrateCompTemplate()
    : m_refCount(1)
    , m_canPenetrateSolid(true)
    , m_canPenetrateWater(true)
{
}

// The complete 12-slot template face. ELF primary vtable 0x88BF928 and PE 0x7DCD78 agree on the
// source order; only the IConsoleSaveable adjustor differs. The three lifecycle callbacks are
// genuine empty shipped bodies. The named class id is 38206 (0x953E), not 38207.
void DefaultPenetrateCompTemplate::addRef() { ++m_refCount; }
int DefaultPenetrateCompTemplate::getRef() { return m_refCount; }

int DefaultPenetrateCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::operator delete(this);
    return remaining;
}

void* DefaultPenetrateCompTemplate::queryInterface(unsigned int iid) const
{
    DefaultPenetrateCompTemplate* const self =
        const_cast<DefaultPenetrateCompTemplate*>(this);
    if (iid == IID_IBase
        || iid == IID_IObjectTemplateComponent
        || iid == IID_IProjectileCompTemplate)
    {
        return self;
    }
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int DefaultPenetrateCompTemplate::getClassID() const
{
    return CID_DefaultPenetrateCompTemplate;
}

unsigned int DefaultPenetrateCompTemplate::getInterfaceID()
{
    return IID_IProjectilePenetrateCompTemplate;
}

IObjectComponent* DefaultPenetrateCompTemplate::createComponent()
{
    return new DefaultPenetrateComp(this);
}

void DefaultPenetrateCompTemplate::objectCreated(IObject*) {}
void DefaultPenetrateCompTemplate::objectDestroyed(IObject*) {}
void DefaultPenetrateCompTemplate::setTemplate(IObjectTemplate*) {}

void* DefaultPenetrateCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == getClassID() ? this : 0;
}

// . Each line is emitted only when its permission differs from the
// constructor's true default; neverPenetrate is not serialised because it has no stored state.
bool DefaultPenetrateCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (!m_canPenetrateSolid)
        script << "ObjectTemplate.penetrate.allowSolidPenetration "
               << m_canPenetrateSolid << std::endl;
    if (!m_canPenetrateWater)
        script << "ObjectTemplate.penetrate.allowLiquidPenetration "
               << m_canPenetrateWater << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
