// Point-physics motion, acceleration, sleep, and latency state.

#include "PointPhysicsNode.h"

#include <math.h>                                    // fabsf, sqrtf

#include "BasicPhysicsSystem.h"                      // IPhysicsSystem + basicPhysicsSystem (0x8B2784C)
#include "PhysicsUtil.h"                             // world::wakeUpObjects
#include "../Core/Ids.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/Projectiles/GrenadeTrace.h"

namespace dice {
namespace hfe {
namespace world {

// Fixed sleep-policy settings and maximum speed. No code mutates them after initialization.
static const int   s_forceSleep = 0;                 // ds:0x8ABA6B8 (handle 0x8ABA6BC)
static const int   s_neverSleep = 0;                 // ds:0x8ABA6C0 (handle 0x8ABA6C4)
static const float s_maxPositionalSpeed = 1500.0f;   // ds:0x8ABA6D0 (handle 0x8ABA6D4)
// ds:0x8ABA6C8 = 1 (handle 0x8ABA6CC): initialised, referenced by no function - not carried.

static const float EPS = 0.000001f;                  // the family's snap threshold, a literal everywhere

// The epsilon snap every Vec3 mutation in the family ends with: |c| < 1e-6 -> 0, per component.
static inline void snapSmall(Vec3& v)
{
    if (fabsf(v.x) < EPS) v.x = 0.0f;
    if (fabsf(v.y) < EPS) v.y = 0.0f;
    if (fabsf(v.z) < EPS) v.z = 0.0f;
}

static inline IPhysicsSystem* physicsSystem()
{
    return static_cast<IPhysicsSystem*>(basicPhysicsSystem);
}

// StaticPhysicsNode ctor, then the field defaults.
PointPhysicsNode::PointPhysicsNode()
{
    m_refCount = 1;                                  // +0x1C
    m_positionalSpeed.x = m_positionalSpeed.y = m_positionalSpeed.z = 0.0f;
    m_positionalAcceleration.x = m_positionalAcceleration.y = m_positionalAcceleration.z = 0.0f;
    m_localLinearSpeed.x = m_localLinearSpeed.y = m_localLinearSpeed.z = 0.0f;
    m_linearIntegrationVelocity.x = m_linearIntegrationVelocity.y = m_linearIntegrationVelocity.z = 0.0f;
    m_mass = 1.0f;
    m_gravityModifier = 1.0f;
    m_drag = 0.0f;
    m_underWater = 0.0f;
    m_hasSeparatePhysicsUpdate = false;
    m_sleepTimeout = 30;
    m_sleepiness = 0;
    m_lastPosition.x = m_lastPosition.y = m_lastPosition.z = 0.0f;
    m_absolutePositionalSpeed.x = m_absolutePositionalSpeed.y = m_absolutePositionalSpeed.z = 0.0f;
    m_floaterMod = 0.0f;
    m_byte88 = false;
    m_otherCollisionLod = 0;
}

PointPhysicsNode::~PointPhysicsNode() {}

void PointPhysicsNode::addRef()  { ++m_refCount; }
int  PointPhysicsNode::getRef()  { return m_refCount; }
int  PointPhysicsNode::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}

void* PointPhysicsNode::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)         return const_cast<PointPhysicsNode*>(this);
    if (iid == IID_IPhysicsNode)  return const_cast<PointPhysicsNode*>(this);
    return 0;
}

IPhysicsNode* PointPhysicsNode::getParent() const   { return 0; }
const Vec3&   PointPhysicsNode::getPositionalSpeed() const { return m_positionalSpeed; }

// store, clamp the magnitude to the max speed, snap.
void PointPhysicsNode::setPositionalSpeed(const Vec3& v)
{
    m_positionalSpeed = v;
    const float max = s_maxPositionalSpeed;
    const float sq  = v.y * v.y + v.x * v.x + v.z * v.z;
    if (sq > max * max)
    {
        const float k = max / sqrtf(sq);
        m_positionalSpeed.x *= k;
        m_positionalSpeed.y *= k;
        m_positionalSpeed.z *= k;
    }
    snapSmall(m_positionalSpeed);
}

// the additive twin - same clamp, same snap, on the SUM.
void PointPhysicsNode::addPositionalSpeed(const Vec3& v)
{
    m_positionalSpeed.x += v.x;
    m_positionalSpeed.y += v.y;
    m_positionalSpeed.z += v.z;
    const float max = s_maxPositionalSpeed;
    const float sq  = m_positionalSpeed.y * m_positionalSpeed.y
                    + m_positionalSpeed.x * m_positionalSpeed.x
                    + m_positionalSpeed.z * m_positionalSpeed.z;
    if (sq > max * max)
    {
        const float k = max / sqrtf(sq);
        m_positionalSpeed.x *= k;
        m_positionalSpeed.y *= k;
        m_positionalSpeed.z *= k;
    }
    snapSmall(m_positionalSpeed);
}

// store + snap (no clamp on accelerations).
void PointPhysicsNode::setPositionalAcceleration(const Vec3& a)
{
    m_positionalAcceleration = a;
    snapSmall(m_positionalAcceleration);
}

// The four "at position" adders (0x83635B0 / 0x8363640 / 0x83636D0 / 0x8363760, 134 B each): a
// point has no lever arm, so the position is ignored and the vector is simply added, then snapped.
void PointPhysicsNode::addSpeedAtAbsolutePosition(const Vec3&, const Vec3& v)
{
    m_positionalSpeed.x += v.x; m_positionalSpeed.y += v.y; m_positionalSpeed.z += v.z;
    snapSmall(m_positionalSpeed);
}
void PointPhysicsNode::addSpeedAtRelativePosition(const Vec3&, const Vec3& v)
{
    m_positionalSpeed.x += v.x; m_positionalSpeed.y += v.y; m_positionalSpeed.z += v.z;
    snapSmall(m_positionalSpeed);
}
void PointPhysicsNode::addAccelerationAtAbsolutePosition(const Vec3&, const Vec3& a)
{
    m_positionalAcceleration.x += a.x; m_positionalAcceleration.y += a.y; m_positionalAcceleration.z += a.z;
    snapSmall(m_positionalAcceleration);
}
void PointPhysicsNode::addAccelerationAtRelativePosition(const Vec3&, const Vec3& a)
{
    m_positionalAcceleration.x += a.x; m_positionalAcceleration.y += a.y; m_positionalAcceleration.z += a.z;
    snapSmall(m_positionalAcceleration);
}

const Vec3& PointPhysicsNode::getTangentSpeed(const Vec3&)        { return m_positionalSpeed; }
const Vec3& PointPhysicsNode::getTangentAcceleration(const Vec3&) { return m_positionalAcceleration; }
const Vec3& PointPhysicsNode::getPositionalAcceleration() const   { return m_positionalAcceleration; }

// See the header for the order (gravity is added after the integration).
void PointPhysicsNode::updatePhysics(float dt)
{
    if (!m_enabled)
        return;
    if (m_drag > 0.0f)
    {
        float u = m_underWater / 0.1f;
        if (u >= 1.0f)
            u = 1.0f;
        const float dragMod = 1.0f - u + u * 25.0f;
        updatePositionalDragSimple(0.1f, dragMod);
    }
    updatePositionalPhysics(dt);
    m_positionalAcceleration.y = physicsSystem()->getGravity() * m_gravityModifier
                               + m_positionalAcceleration.y;
}

void  PointPhysicsNode::setDrag(float v)              { m_drag = v; }
float PointPhysicsNode::getDrag() const               { return m_drag; }
void  PointPhysicsNode::setMass(float v)              { m_mass = v; }
float PointPhysicsNode::getMass() const               { return m_mass; }
void  PointPhysicsNode::setGravityModifier(float v)   { m_gravityModifier = v; }
float PointPhysicsNode::getGravityModifier() const    { return m_gravityModifier; }
void  PointPhysicsNode::setHasSeparatePhysicsUpdate(bool v) { m_hasSeparatePhysicsUpdate = v; }
bool  PointPhysicsNode::getHasSeparatePhysicsUpdate() const { return m_hasSeparatePhysicsUpdate; }
void  PointPhysicsNode::setUnderWater(float v)        { m_underWater = v; }
float PointPhysicsNode::getUnderWater() const         { return m_underWater; }


void PointPhysicsNode::reset(bool full)
{
    m_positionalSpeed.x = m_positionalSpeed.y = m_positionalSpeed.z = 0.0f;
    m_positionalAcceleration.x = m_positionalAcceleration.y = m_positionalAcceleration.z = 0.0f;
    m_localLinearSpeed.x = m_localLinearSpeed.y = m_localLinearSpeed.z = 0.0f;
    m_linearIntegrationVelocity.x = m_linearIntegrationVelocity.y = m_linearIntegrationVelocity.z = 0.0f;
    m_absolutePositionalSpeed.x = m_absolutePositionalSpeed.y = m_absolutePositionalSpeed.z = 0.0f;
    if (full)
    {
        if (m_object)
            m_lastPosition = m_object->getAbsolutePosition();
        else
            m_lastPosition.x = m_lastPosition.y = m_lastPosition.z = 0.0f;
        m_underWater = 0.0f;
    }
    else
    {
        m_positionalAcceleration.y = physicsSystem()->getGravity() * m_gravityModifier;
    }
    m_byte88 = false;
}

// Sleep (0x8362C50..0x8362D9F) - see the header's account of the two switches
bool PointPhysicsNode::getIsSleeping() const
{
    if (!m_enabled)
        return true;
    if (s_neverSleep == 1)
        return false;
    if (s_forceSleep == 1)
        return true;
    return m_sleepiness <= 0;
}

void PointPhysicsNode::setIsSleeping()
{
    if (s_neverSleep == 1)
        m_sleepiness = m_sleepTimeout;
    else
        m_sleepiness = 0;
}

void PointPhysicsNode::setSleepiness(int n)
{
    if (s_neverSleep == 1)
        m_sleepiness = m_sleepTimeout;
    else
        m_sleepiness = (s_forceSleep != 1) ? n : 0;
}

int PointPhysicsNode::getSleepiness() const
{
    if (m_enabled)
    {
        if (s_neverSleep == 1) return m_sleepTimeout;
        if (s_forceSleep == 1) return 0;
        return m_sleepiness;
    }
    return m_sleepiness < 0 ? m_sleepiness : 0;
}

void PointPhysicsNode::setIsSleepier()
{
    if (s_neverSleep != 1 && m_sleepiness > 0)
        --m_sleepiness;
}

// reload the countdown; a node that was asleep and drives a root PCO wakes its
// neighbourhood too (getObject() is the virtual +0x2C call, reproduced as such).
void PointPhysicsNode::setIsAwake()
{
    if (s_forceSleep == 1)
        return;
    const int was = m_sleepiness;
    m_sleepiness = m_sleepTimeout;
    if (was > 0)
        return;
    IObject* object = getObject();
    if (!object)
        return;
    if (object == getRootParent(object)
        && object->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate)
    {
        const float radius = object->getPhysicBoundingRadius();
        wakeUpObjects(object->getAbsolutePosition(), radius, object, false);
    }
}

bool PointPhysicsNode::getIsMobile() const  { return true; }
bool PointPhysicsNode::getIsPoint() const   { return true; }
const Vec3& PointPhysicsNode::getAbsolutePositionalSpeed() { return m_absolutePositionalSpeed; }
IPhysicsNode* PointPhysicsNode::getRootNode() { return this; }

// an impulse on a point is a local linear speed to fold in next tick.
void PointPhysicsNode::addRelativeImpulse(const Vec3&, const Vec3& v)
{
    m_localLinearSpeed.x += v.x; m_localLinearSpeed.y += v.y; m_localLinearSpeed.z += v.z;
    snapSmall(m_localLinearSpeed);
}

void  PointPhysicsNode::setFloaterMod(float v)        { m_floaterMod = v; }
float PointPhysicsNode::getFloaterMod() const         { return m_floaterMod; }
const Vec3& PointPhysicsNode::getLinearIntegrationVelocity() const { return m_linearIntegrationVelocity; }
void  PointPhysicsNode::setLinearIntegrationVelocity(const Vec3& v) { m_linearIntegrationVelocity = v; }

// the measured speed - position delta times the 30 Hz tick, snapped.
void PointPhysicsNode::postFrameUpdate()
{
    const Vec3& pos = getAbsolutePosition();         // the virtual +0x48 call (-> the object's)
    const float px = pos.x, py = pos.y, pz = pos.z;
    float vx = (px - m_lastPosition.x) * 30.0f;
    float vy = 30.0f * (py - m_lastPosition.y);
    float vz = 30.0f * (pz - m_lastPosition.z);
    if (fabsf(vx) < EPS) vx = 0.0f;
    if (fabsf(vy) < EPS) vy = 0.0f;
    if (fabsf(vz) < EPS) vz = 0.0f;
    m_absolutePositionalSpeed.x = vx;
    m_absolutePositionalSpeed.y = vy;
    m_absolutePositionalSpeed.z = vz;
    m_lastPosition.x = px;
    m_lastPosition.y = py;
    m_lastPosition.z = pz;
}

void  PointPhysicsNode::setLocalLinearSpeed(const Vec3& v)   { m_localLinearSpeed = v; }
const Vec3& PointPhysicsNode::getLocalLinearSpeed() const    { return m_localLinearSpeed; }
void  PointPhysicsNode::setOtherCollisionLod(int v)          { m_otherCollisionLod = v; }
int   PointPhysicsNode::getOtherCollisionLod() const         { return m_otherCollisionLod; }
void  PointPhysicsNode::setSleepTimeout(int v)               { m_sleepTimeout = v; }

// quadratic drag against the wind, as an acceleration through the virtual
// addAccelerationAtAbsolutePosition (slot +0x90) at Vec3::getZero() - so PhysicsNode's override,
// which turns an off-centre acceleration into torque as well, receives it. -pi * r^2 * drag / mass
// times (speed * dragMod - wind), component-wise.
void PointPhysicsNode::updatePositionalDragSimple(float radius, float dragMod)
{
    if (!m_enabled)
        return;
    const Vec3 wind = physicsSystem()->getWind();
    const float rx = m_positionalSpeed.x * dragMod - wind.x;
    const float ry = m_positionalSpeed.y * dragMod - wind.y;
    const float rz = dragMod * m_positionalSpeed.z - wind.z;
    const float r2 = radius * radius;
    const float invMass = 1.0f / m_mass;
    Vec3 a;
    a.x = (rx * -3.1415927f) * r2 * m_drag * invMass;
    a.y = (ry * -3.1415927f) * r2 * m_drag * invMass;
    a.z = (rz * -3.1415927f) * r2 * m_drag * invMass;
    addAccelerationAtAbsolutePosition(Vec3::getZero(), a);
}

// the trapezoidal step. Both speed additions go through the virtual
// addPositionalSpeed (slot +0x70) - clamp and snap included.
void PointPhysicsNode::updatePositionalPhysics(float dt)
{
    if (!m_enabled)
        return;
    addPositionalSpeed(m_localLinearSpeed);
    const float ox = m_positionalSpeed.x, oy = m_positionalSpeed.y, oz = m_positionalSpeed.z;
    Vec3 dv;
    dv.x = dt * m_positionalAcceleration.x;
    dv.y = dt * m_positionalAcceleration.y;
    dv.z = dt * m_positionalAcceleration.z;
    addPositionalSpeed(dv);
    m_linearIntegrationVelocity.x = (m_positionalSpeed.x + ox) * 0.5f;
    m_linearIntegrationVelocity.y = (m_positionalSpeed.y + oy) * 0.5f;
    m_linearIntegrationVelocity.z = 0.5f * (m_positionalSpeed.z + oz);
    snapSmall(m_linearIntegrationVelocity);

    const Vec3& p = m_object->getAbsolutePosition();
    Vec3 np;
    np.x = p.x + dt * m_linearIntegrationVelocity.x;
    np.y = p.y + dt * m_linearIntegrationVelocity.y;
    np.z = p.z + dt * m_linearIntegrationVelocity.z;
    if (grenade_trace::selected(m_object))
    {
        grenade_trace::log(m_object, "physics",
            "dt=%.9g from=(%.9g,%.9g,%.9g) to=(%.9g,%.9g,%.9g) "
            "velocity=(%.9g,%.9g,%.9g) acceleration=(%.9g,%.9g,%.9g) "
            "impulse_speed=(%.9g,%.9g,%.9g) integration=(%.9g,%.9g,%.9g) "
            "drag=%.9g mass=%.9g gravity_modifier=%.9g",
            dt, p.x, p.y, p.z, np.x, np.y, np.z,
            m_positionalSpeed.x, m_positionalSpeed.y, m_positionalSpeed.z,
            m_positionalAcceleration.x, m_positionalAcceleration.y, m_positionalAcceleration.z,
            m_localLinearSpeed.x, m_localLinearSpeed.y, m_localLinearSpeed.z,
            m_linearIntegrationVelocity.x, m_linearIntegrationVelocity.y, m_linearIntegrationVelocity.z,
            m_drag, m_mass, m_gravityModifier);
    }
    m_object->setAbsolutePosition(np);

    m_positionalAcceleration.x = m_positionalAcceleration.y = m_positionalAcceleration.z = 0.0f;
    m_localLinearSpeed.x = m_localLinearSpeed.y = m_localLinearSpeed.z = 0.0f;
}

} // namespace world
} // namespace hfe
} // namespace dice
