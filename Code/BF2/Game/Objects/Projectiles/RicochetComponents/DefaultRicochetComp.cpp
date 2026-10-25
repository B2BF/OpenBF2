// Ricochet decisions use a random stream separate from penetration.

#include "DefaultRicochetComp.h"

#include <cfloat>       // FLT_EPSILON
#include <cmath>
#include <sstream>
#include <string>

#include "../ProjectileCompTemplates.h"
#include "../GenericProjectile.h"
#include "../../IObject.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Scene/Object/ObjectManager.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

// THE inlined LCG, AND ITS stream IS THE point. DefaultRicochetComp does NOT call Random - the
// 24112 / 26143 generator is inlined into `ricochet` and advances a global of its own at
// ds:0x8AB0660. DefaultPenetrateComp inlines the same generator against ds:0x8AB0700, so the two
// components draw from separate sequences. Collapsing either into a shared Random would change
// every bounce and every penetration deviation the game rolls. See RW-68.
int g_ricochetRandomState = 0;                                 // ds:0x8AB0660

int ricochetRandomStep(int seed)
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

// THE cone IS A fixed 0.2 here. DefaultPenetrateComp scales its identical draw by the surface
// material's penetrationDeviation; ricochet does not consult the material at all - the spread is
// the same for every surface in the game.
const float kRicochetConeScale = 0.2f;

float ricochetRandomDeviation(int state)
{
    const float raw = static_cast<float>(((state >> 7) | 1) + 1);
    return (0.5f - raw * 5.9604645e-8f) * kRicochetConeScale;
}

void normaliseInPlace(Vec3& v)
{
    const float lengthSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (fabsf(lengthSq - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(lengthSq) >= FLT_EPSILON)
        {
            const float inverse = 1.0f / sqrtf(lengthSq);
            v.x *= inverse; v.y *= inverse; v.z *= inverse;
        }
        else
        {
            v.x = 0.0f; v.y = 0.0f; v.z = 0.0f;
        }
    }
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

// Construction and the leaf boilerplate. 28 bytes: ProjectileComp's 16, the second vptr, this
// leaf's reference count, and the template. No per-instance state - ricochet is a pure transform.
DefaultRicochetComp::DefaultRicochetComp(DefaultRicochetCompTemplate* objectTemplate)
    : m_refCount(1)
    , m_template(objectTemplate)
{
    ProjectileComp::m_refCount = 1;
    m_object = 0;
    m_initObject = 0;
}

DefaultRicochetComp::~DefaultRicochetComp() {}

void DefaultRicochetComp::addRef() { ++m_refCount; }
int  DefaultRicochetComp::getRef() { return m_refCount; }

int DefaultRicochetComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = (m_refCount == 1);
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* DefaultRicochetComp::queryInterface(unsigned int iid) const
{
    DefaultRicochetComp* const self = const_cast<DefaultRicochetComp*>(this);

    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IProjectileComp || iid == IID_IProjectileRicochetComp)
        return static_cast<IProjectileRicochetComp*>(self);
    return 0;
}

unsigned int DefaultRicochetComp::getInterfaceID() const
{
    return IID_IProjectileRicochetComp;
}

bool DefaultRicochetComp::hasUpdate() const { return false; }
void DefaultRicochetComp::updateComponent(float, unsigned int) {}
void DefaultRicochetComp::activate() {}

// Attempts a reflected, randomly perturbed bounce and returns whether energy is sufficient.
// Writes reduced currentDamage before the gate and never restores it on failure.
// Preserves speed magnitude, adds physics-frame velocity correction and draws a fixed 0.2 deviation cone.
// On success updates position/clock and schedules lifetime scaled by retainedFraction from original launch time.
bool DefaultRicochetComp::ricochet(const Vec3& impactOffset,
                                   const Vec3& incomingVelocity,
                                   const Vec3& surfaceNormal,
                                   float retainedFraction,
                                   float baseProjectileDamage,
                                   float sampledLifetime,
                                   float& lastRicochetTime,
                                   Vec3& ricochetWorldPosition,
                                   float& currentDamage)
{
    IObject* const projectile = m_initObject;

    // Written unconditionally - see the note above.
    const Vec3 here = projectile->getAbsolutePosition();
    currentDamage =
        static_cast<GenericProjectile*>(projectile)->getDamage(here) * retainedFraction;

    if (baseProjectileDamage * 0.1f > currentDamage)
        return false;

    IPhysicsNode* const node = static_cast<IPhysicsNode*>(
        object_detail::LayoutAccess::physicsNode(projectile));

    // stage 2: reflect, then correct into the surface's frame
    const float along = incomingVelocity.x * surfaceNormal.x +
                        incomingVelocity.y * surfaceNormal.y +
                        incomingVelocity.z * surfaceNormal.z;

    Vec3 direction;
    direction.x = incomingVelocity.x - (surfaceNormal.x + surfaceNormal.x) * along;
    direction.y = incomingVelocity.y - (surfaceNormal.y + surfaceNormal.y) * along;
    direction.z = incomingVelocity.z - (surfaceNormal.z + surfaceNormal.z) * along;

    const Vec3& nodeSpeed = node->getPositionalSpeed();
    direction.x += nodeSpeed.x - incomingVelocity.x;
    direction.y += nodeSpeed.y - incomingVelocity.y;
    direction.z += nodeSpeed.z - incomingVelocity.z;

    // stage 3: rebuild the frame and perturb it
    // Mat4 is a flat float[16] in this tree; row 2 is the direction of travel.
    Mat4 frame = projectile->getAbsoluteTransformation();

    Vec3 row2 = direction;
    normaliseInPlace(row2);

    Vec3 row1;
    row1.x = frame.m[4]; row1.y = frame.m[5]; row1.z = frame.m[6];

    Vec3 row0 = cross(row1, row2);
    row1 = cross(row2, row0);

    // The speed the bounce keeps - taken from the unperturbed reflected vector, before the cone.
    const float speed = sqrtf(direction.x * direction.x +
                              direction.y * direction.y +
                              direction.z * direction.z);

    const unsigned int tick = static_cast<unsigned int>(WorldPref::getTickCount());
    const int first = ricochetRandomStep(static_cast<int>(tick % 0x3FF));
    const int second = ricochetRandomStep(first);
    g_ricochetRandomState = second;

    const float deviationA = ricochetRandomDeviation(first);
    const float deviationB = ricochetRandomDeviation(second);

    row2.x += row0.x * deviationA + row1.x * deviationB;
    row2.y += row0.y * deviationA + row1.y * deviationB;
    row2.z += row0.z * deviationA + row1.z * deviationB;
    normaliseInPlace(row2);

    row0 = cross(row1, row2);
    normaliseInPlace(row0);

    row1 = cross(row2, row0);
    normaliseInPlace(row1);

    frame.m[0] = row0.x; frame.m[1] = row0.y; frame.m[2] = row0.z;
    frame.m[4] = row1.x; frame.m[5] = row1.y; frame.m[6] = row1.z;
    frame.m[8] = row2.x; frame.m[9] = row2.y; frame.m[10] = row2.z;

    // stage 4: commit
    Vec3 newSpeed;
    newSpeed.x = row2.x * speed;
    newSpeed.y = row2.y * speed;
    newSpeed.z = row2.z * speed;

    const Vec3& current = projectile->getAbsolutePosition();
    Vec3 world;
    world.x = impactOffset.x + current.x;
    world.y = impactOffset.y + current.y;
    world.z = impactOffset.z + current.z;

    // The bounce starts from the impact point: it is the stored translation, not the end of this
    // tick's sweep.
    frame.m[12] = world.x;
    frame.m[13] = world.y;
    frame.m[14] = world.z;
    projectile->setAbsoluteTransformation(frame);
    node->setPositionalSpeed(newSpeed);

    ricochetWorldPosition = world;

    const float remaining = sampledLifetime * retainedFraction +
        static_cast<GenericProjectile*>(projectile)->getWorldLaunchTime() -
        static_cast<float>(WorldPref::getWorldTime());
    objectManager->postMessage(projectile, static_cast<TemplateMessage>(29), remaining);

    lastRicochetTime = static_cast<float>(g_system->getExactTime());
    return true;
}

// DefaultRicochetCompTemplate::DefaultRicochetCompTemplate - 0x8299A00, 28 B. The two vptrs and a
// reference count; this template carries NO tuning at all - which is consistent with the component
// never consulting it, and with the cone scale being the literal 0.2 above.
DefaultRicochetCompTemplate::DefaultRicochetCompTemplate()
    : m_refCount(1)
{
}

// The complete template face. ELF primary vtable 0x88BF628 and PE primary vtable 0x7DCE60 agree:
// this leaf carries no authored state, all three lifecycle callbacks are genuine empty bodies, and
// makeScript writes an empty buffer through the IConsoleSaveable secondary face.
void DefaultRicochetCompTemplate::addRef() { ++m_refCount; }
int DefaultRicochetCompTemplate::getRef() { return m_refCount; }

int DefaultRicochetCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        ::operator delete(this);
    return remaining;
}

void* DefaultRicochetCompTemplate::queryInterface(unsigned int iid) const
{
    DefaultRicochetCompTemplate* const self =
        const_cast<DefaultRicochetCompTemplate*>(this);
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

unsigned int DefaultRicochetCompTemplate::getClassID() const
{
    return CID_DefaultRicochetCompTemplate;
}

unsigned int DefaultRicochetCompTemplate::getInterfaceID()
{
    return IID_IProjectileRicochetCompTemplate;
}

IObjectComponent* DefaultRicochetCompTemplate::createComponent()
{
    return new DefaultRicochetComp(this);
}

void DefaultRicochetCompTemplate::objectCreated(IObject*) {}
void DefaultRicochetCompTemplate::objectDestroyed(IObject*) {}
void DefaultRicochetCompTemplate::setTemplate(IObjectTemplate*) {}

void* DefaultRicochetCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == getClassID() ? this : 0;
}

bool DefaultRicochetCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
