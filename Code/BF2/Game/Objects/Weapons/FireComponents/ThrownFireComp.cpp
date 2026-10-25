// Thrown-projectile firing and template properties.

#include "ThrownFireComp.h"

#include <cfloat>
#include <cmath>
#include <sstream>
#include <string>

#include "../../ThrownFireCompTemplate.h"
#include "../GenericFireArm.h"
#include "../../ICameraObject.h"
#include "../../IObject.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../Projectiles/GrenadeTrace.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace world {

// ThrownFireComp runtime

// ELF C2/C1 0x826EE80/0x826EF40.  PE uses the same source order; its inherited STL members make
// absolute offsets unsuitable as cross-port assertions.
ThrownFireComp::ThrownFireComp(ThrownFireCompTemplate* objectTemplate)
    : FireComp(objectTemplate)
    , m_refCount(1)
    , m_fireStrength(1.0f)
    , m_pullbackStartedAtTick(0)
    , m_launchDelayStartedAtTick(0)
    , m_requestingFire(false)
    , m_reloading(false)
    , m_thrownProjectileOrigin(Mat4::getIdentity())
    , m_fireCompleteAtTick(0)
    , m_pendingFireStop(false)
    , m_thrownTemplate(objectTemplate)
{
}

ThrownFireComp::~ThrownFireComp()
{
}

void ThrownFireComp::addRef()
{
    ++m_refCount;
}

int ThrownFireComp::getRef()
{
    return m_refCount;
}

int ThrownFireComp::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

// .  The adjusted IWeaponFireComp answer is +0x10 in both shipped x86 images.
void* ThrownFireComp::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return const_cast<ThrownFireComp*>(this);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponFireComp)
    {
        return static_cast<IWeaponFireComp*>(
            const_cast<ThrownFireComp*>(this));
    }
    return 0;
}

unsigned int ThrownFireComp::getInterfaceID() const
{
    return IID_IWeaponFireComp;
}

// .  The pullback expiry is inclusive, whereas the launch-delay arm waits only while
// its completion tick is strictly in the future.
bool ThrownFireComp::preUpdate()
{
    if (m_requestingFire)
        return false;

    if (static_cast<int>(getFireState()) == 2)
    {
        const unsigned int pullbackComplete =
            m_pullbackStartedAtTick
            + static_cast<unsigned int>(m_thrownTemplate->getPullbackTimeInTicks());
        if (pullbackComplete <= static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            if (setThrowState(static_cast<FireState>(3), false))
            {
                m_launchDelayStartedAtTick =
                    static_cast<unsigned int>(WorldPref::getTickCount());
                return true;
            }
        }
    }

    // Retail repeats this guard after the pullback arm.
    if (m_requestingFire)
        return false;
    if (static_cast<int>(getFireState()) != 3)
        return false;

    const unsigned int launchComplete =
        m_launchDelayStartedAtTick
        + static_cast<unsigned int>(m_thrownTemplate->getFireLaunchDelayInTicks());
    if (launchComplete > static_cast<unsigned int>(WorldPref::getTickCount()))
        return false;
    return setThrowState(static_cast<FireState>(4), false);
}

// .  The three getPlayer calls and the strict reload-show comparison are preserved.
void ThrownFireComp::updateComponent(float, unsigned int)
{
    if (m_pendingFireStop)
    {
        if (firearm()->getPlayer())
        {
            if (!firearm()->getPlayer()->getSprintState()
                && !firearm()->getPlayer()->getSprintStateLastTick())
            {
                if (m_altFireButtonLastHeldAtTick
                    < static_cast<unsigned int>(WorldPref::getTickCount()))
                {
                    fireStop(true);
                }
                m_pendingFireStop = false;
            }
        }
    }

    if (m_requestingFire)
    {
        fireUpdateStrength();
        return;
    }

    if (static_cast<int>(getFireState()) > 3
        && m_fireCompleteAtTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        fireComplete(m_fireCompleteAtTick);
    }

    if (m_reloading)
    {
        const unsigned int showAt =
            m_reloadAtTick
            + static_cast<unsigned int>(
                m_thrownTemplate->getShowWeaponAfterReloadDelayInTicks());
        if (showAt < static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            firearm()->updateFlags(2, 0);
            m_reloading = false;
        }
    }
}

void ThrownFireComp::reload(ReloadState)
{
    m_reloading = true;
    m_reloadAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
}

bool ThrownFireComp::requestFire(bool first, bool second)
{
    if (first || !setThrowState(static_cast<FireState>(2), true))
        return false;
    m_requestingFire = second;
    return true;
}

float ThrownFireComp::getAcceptDelay(bool) const
{
    float delay = m_thrownTemplate->getFireLaunchDelay();
    if (delay <= m_thrownTemplate->getFireLaunchDelaySoft())
        delay = m_thrownTemplate->getFireLaunchDelaySoft();
    return delay + m_thrownTemplate->getShowWeaponAfterReloadDelay();
}

bool ThrownFireComp::launchesProjectiles(int) const
{
    return static_cast<int>(getFireState()) == 0;
}

// / PE counterpart: the shipped leaf override is exactly empty.
void ThrownFireComp::outOfAmmo(bool)
{
}

int ThrownFireComp::fire(bool, int)
{
    m_firedProjectiles.clear();
    bool result = false;

    if (setThrowState(static_cast<FireState>(2), false))
    {
        if (m_thrownTemplate->getPullbackTimeInTicks() > 0)
        {
            m_pullbackStartedAtTick =
                static_cast<unsigned int>(WorldPref::getTickCount());
            return true;
        }
        result = true;
    }

    // Simulation is intentional: the 4 -> 0 transition is validated without changing state;
    // FireComp::fireComplete later owns the actual completion transition.
    if (setThrowState(static_cast<FireState>(0), true))
    {
        launchProjectile();
        m_fireAtTick = static_cast<unsigned int>(WorldPref::getTickCount());
        m_fireCompleteAtTick =
            m_fireAtTick + static_cast<unsigned int>(m_thrownTemplate->getTimePerRound());
    }
    return result;
}

void ThrownFireComp::fireUpdateStrength()
{
    const unsigned int elapsed =
        static_cast<unsigned int>(WorldPref::getTickCount()) - m_pullbackStartedAtTick;
    float strength = static_cast<float>(static_cast<int>(elapsed))
        * m_thrownTemplate->getInverseStrengthPerTick();
    if (strength > 1.0f)
        strength = 1.0f;
    m_fireStrength = strength;

    float heat = strength;
    if (heat > 0.99000001f)
        heat = 0.99000001f;
    firearm()->setHeat(heat);
    firearm()->m_showHudIcon = true;
}

bool ThrownFireComp::setThrowState(FireState state, bool simulate)
{
    bool valid = false;
    if (static_cast<int>(state) == static_cast<int>(getFireState()) + 1
        || (static_cast<int>(getFireState()) == 4 && static_cast<int>(state) == 0)
        || (static_cast<int>(getFireState()) == 0 && static_cast<int>(state) == 2))
    {
        valid = true;
        if (!simulate)
            setFireState(state);
    }
    return valid;
}

void ThrownFireComp::exit()
{
    if (firearm()->getPlayer() && static_cast<int>(getFireState()) > 2)
        reset();
}

void ThrownFireComp::enable()
{
    if (m_fireCompleteAtTick
        && m_fireCompleteAtTick <= static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        FireComp::fireComplete(m_fireCompleteAtTick);
    }
    reset();
}

void ThrownFireComp::disable()
{
    reset();
}

void ThrownFireComp::fireStop(bool value)
{
    IPlayer* const player = firearm()->getPlayer();
    if (player && player->getSprintState())
    {
        m_pendingFireStop = true;
    }
    else
    {
        FireComp::fireStop(value);
        if (m_requestingFire && value)
            m_requestingFire = false;
    }
}

void ThrownFireComp::fireComplete(unsigned int tick)
{
    FireComp::fireComplete(tick);
    reset();
}

void ThrownFireComp::reset()
{
    m_fireStrength = 1.0f;
    firearm()->setHeat(0.0f);
    firearm()->m_showHudIcon = false;
    m_fireCompleteAtTick = 0;
    m_pendingFireStop = false;
}

// (1,726 B).  The rotational seed is the projectile object id and the inherited root
// speed deliberately has its vertical component discarded before it is added.
bool ThrownFireComp::launchProjectile()
{
    const bool result = FireComp::launchProjectile(-1);

    IObject* const projectile =
        m_firedProjectiles.empty() ? 0 : m_firedProjectiles.back();
    if (projectile)
    {
        IPhysicsNode* const projectileNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(projectile));

        const Mat4& projectileTransform = projectile->getAbsoluteTransformation();
        const float weaponVelocity = firearm()->getVelocity();
        Vec3 velocity = {
            projectileTransform.m[8] * weaponVelocity,
            projectileTransform.m[9] * weaponVelocity,
            projectileTransform.m[10] * weaponVelocity
        };

        Vec3 inheritedVelocity = {0.0f, 0.0f, 0.0f};
        IPhysicsNode* const rootNode = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(getRootParent(firearm())));
        if (rootNode)
            inheritedVelocity = rootNode->getPositionalSpeed();
        inheritedVelocity.y = 0.0f;

        Vec3 launchVelocity = {
            velocity.x * m_fireStrength + inheritedVelocity.x,
            velocity.y * m_fireStrength + inheritedVelocity.y,
            velocity.z * m_fireStrength + inheritedVelocity.z
        };
        // Retail does not null-guard a fired projectile's physics node.
        projectileNode->setPositionalSpeed(launchVelocity);

        if (grenade_trace::armed() && grenade_trace::begin(projectile))
        {
            grenade_trace::log(projectile, "launch",
                "origin=%.9g/%.9g/%.9g origin_forward=%.9g/%.9g/%.9g "
                "position=%.9g/%.9g/%.9g forward=%.9g/%.9g/%.9g "
                "weapon_speed=%.9g strength=%.9g inherited=%.9g/%.9g/%.9g "
                "velocity=%.9g/%.9g/%.9g",
                m_thrownProjectileOrigin.m[12], m_thrownProjectileOrigin.m[13],
                m_thrownProjectileOrigin.m[14], m_thrownProjectileOrigin.m[8],
                m_thrownProjectileOrigin.m[9], m_thrownProjectileOrigin.m[10],
                projectileTransform.m[12], projectileTransform.m[13], projectileTransform.m[14],
                projectileTransform.m[8], projectileTransform.m[9], projectileTransform.m[10],
                weaponVelocity, m_fireStrength,
                inheritedVelocity.x, inheritedVelocity.y, inheritedVelocity.z,
                launchVelocity.x, launchVelocity.y, launchVelocity.z);
        }

        const Vec3& startRotation = m_thrownTemplate->getProjectileStartRotation();
        if (startRotation.x * startRotation.x
                + startRotation.y * startRotation.y
                + startRotation.z * startRotation.z
            != 0.0f)
        {
            const Mat4 original = projectile->getAbsoluteTransformation();
            Mat4 rotation = Mat4::getIdentity();
            setRotationZXY(rotation,
                           -startRotation.z,
                           -startRotation.y,
                           -startRotation.x);

            Mat4 composed = Mat4::getIdentity();
            composed.mult(rotation, original);
            const Vec3& position = projectile->getAbsolutePosition();
            composed.m[12] = position.x;
            composed.m[13] = position.y;
            composed.m[14] = position.z;
            projectile->setAbsoluteTransformation(composed);
        }

        const float randomRotationSpeed = m_thrownTemplate->getRandomRotationSpeed();
        if (randomRotationSpeed > 0.0f)
        {
            Random random;
            random.m_seed = projectile->getObjectId();

            const unsigned int firstRaw = random.getRaw();
            const float firstSample =
                static_cast<float>(((firstRaw >> 7) | 1u) + 1u) * 0.000000059604645f;
            const float first = std::ceil(randomRotationSpeed * firstSample);

            const unsigned int secondRaw = random.getRaw();
            const float secondSample =
                static_cast<float>(((secondRaw >> 7) | 1u) + 1u) * 0.000000059604645f;
            const float second = std::ceil(randomRotationSpeed * secondSample);

            Vec3 rotationalSpeed = {
                0.0f,
                -first * m_fireStrength,
                second * m_fireStrength
            };
            projectileNode->setRotationalSpeed(rotationalSpeed);
        }
    }

    firearm()->updateFlags(0, 2);
    return result;
}

const Mat4& ThrownFireComp::getProjectileOrigin() const
{
    IPlayerControlObject* playerControlObject = 0;
    for (IObject* object = firearm(); object; object = object->getParent())
    {
        playerControlObject = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (playerControlObject)
            break;
    }

    IObject* originObject = firearm();
    if (playerControlObject)
    {
        ICameraObject* const camera = playerControlObject->getActiveCamera();
        originObject = 0;
        if (camera)
            originObject = static_cast<IObject*>(camera->queryInterface(IID_IObject));
    }

    // Retail dereferences the camera's IObject answer without a fallback once a PCO was found.
    m_thrownProjectileOrigin = originObject->getAbsoluteTransformation();

    const Vec3& position = m_thrownTemplate->getProjectileStartPosition();
    m_thrownProjectileOrigin.m[12] += position.z * m_thrownProjectileOrigin.m[8];
    m_thrownProjectileOrigin.m[13] += position.z * m_thrownProjectileOrigin.m[9];
    m_thrownProjectileOrigin.m[14] += position.z * m_thrownProjectileOrigin.m[10];
    m_thrownProjectileOrigin.m[12] += position.y * m_thrownProjectileOrigin.m[4];
    m_thrownProjectileOrigin.m[13] += position.y * m_thrownProjectileOrigin.m[5];
    m_thrownProjectileOrigin.m[14] += position.y * m_thrownProjectileOrigin.m[6];
    m_thrownProjectileOrigin.m[12] += position.x * m_thrownProjectileOrigin.m[0];
    m_thrownProjectileOrigin.m[13] += position.x * m_thrownProjectileOrigin.m[1];
    m_thrownProjectileOrigin.m[14] += position.x * m_thrownProjectileOrigin.m[2];
    return m_thrownProjectileOrigin;
}

bool ThrownFireComp::busy()
{
    return static_cast<int>(getFireState()) != 0;
}

// ThrownFireCompTemplate

ThrownFireCompTemplate::ThrownFireCompTemplate()
    : FireCompTemplate()
    , m_refCount(1)
    , m_timeToReachMaxStrengthSlow(1.0f)
    , m_inverseStrengthPerTick(1.0f / 30.0f)
    , m_pullbackTime(0.2f)
    , m_pullbackTimeTimes30(6)
    , m_fireLaunchDelaySoft(0.0f)
    , m_fireLaunchDelaySoftTimes30(0)
    , m_showWeaponAfterReloadDelay(0.0f)
    , m_showWeaponAfterReloadDelayTimes30(0)
    , m_randomRotationSpeed(0.0f)
    , m_projectileStartRotation{0.0f, 0.0f, 0.0f}
{
    // These two setters are called even though the initializer stores the same defaults.
    setFireLaunchDelaySoft(0.0f);
    setShowWeaponAfterReloadDelay(0.0f);
}

ThrownFireCompTemplate::~ThrownFireCompTemplate()
{
}

void ThrownFireCompTemplate::addRef()
{
    ++m_refCount;
}

int ThrownFireCompTemplate::getRef()
{
    return m_refCount;
}

int ThrownFireCompTemplate::release()
{
    const int remaining = m_refCount - 1;
    const bool last = m_refCount == 1;
    m_refCount = remaining;
    if (last && this)
        delete this;
    return remaining;
}

void* ThrownFireCompTemplate::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase
        || iid == IID_IObjectTemplateComponent
        || iid == IID_IWeaponCompTemplate)
    {
        return const_cast<ThrownFireCompTemplate*>(this);
    }
    if (iid == io::IID_IConsoleSaveable)
    {
        return static_cast<io::IConsoleSaveable*>(
            const_cast<ThrownFireCompTemplate*>(this));
    }
    return 0;
}

unsigned int ThrownFireCompTemplate::getClassID() const
{
    return CID_ThrownFireCompTemplate;
}

void* ThrownFireCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_ThrownFireCompTemplate || classId == CID_FireCompTemplate)
        return this;
    return 0;
}

IObjectComponent* ThrownFireCompTemplate::createComponent()
{
    return new ThrownFireComp(this);
}

void ThrownFireCompTemplate::setFireLaunchDelaySoft(float value)
{
    m_fireLaunchDelaySoft = value;
    m_fireLaunchDelaySoftTimes30 = static_cast<int>(value * 30.0f);
}

void ThrownFireCompTemplate::setShowWeaponAfterReloadDelay(float value)
{
    m_showWeaponAfterReloadDelay = value;
    m_showWeaponAfterReloadDelayTimes30 = static_cast<int>(value * 30.0f);
}

void ThrownFireCompTemplate::setPullbackTime(float value)
{
    m_pullbackTime = value;
    m_pullbackTimeTimes30 = static_cast<int>(value * 30.0f);
}

void ThrownFireCompTemplate::setTimeToReachMaxStrengthSlow(float value)
{
    m_timeToReachMaxStrengthSlow = value;
    // No zero guard exists in retail; authored zero deliberately produces the platform's ieee
    // divide-by-zero result.
    m_inverseStrengthPerTick = 1.0f / (value * 30.0f);
}

// (1,769 B), PE 0x5AEFC0 (577 B).  PE confirms the same six gates/order and the
// otherwise surprising capital-B output spelling "pullBackTime".
bool ThrownFireCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (!FireCompTemplate::makeScript(stream))
        return false;

    if (m_fireLaunchDelaySoft != 0.0f)
        script << "ObjectTemplate.fire.fireLaunchDelaySoft "
               << m_fireLaunchDelaySoft << std::endl;
    if (m_showWeaponAfterReloadDelay != 0.0f)
        script << "ObjectTemplate.fire.showWeaponAfterReloadDelay "
               << m_showWeaponAfterReloadDelay << std::endl;
    if (m_timeToReachMaxStrengthSlow != 1.0f)
        script << "ObjectTemplate.fire.timeToReachMaxStrengthSlow "
               << m_timeToReachMaxStrengthSlow << std::endl;
    if (m_pullbackTime != 0.2f)
        script << "ObjectTemplate.fire.pullBackTime " << m_pullbackTime << std::endl;

    const bool rotationIsZero =
        std::fabs(m_projectileStartRotation.x) <= FLT_EPSILON
        && std::fabs(m_projectileStartRotation.y) <= FLT_EPSILON
        && std::fabs(m_projectileStartRotation.z) <= FLT_EPSILON;
    if (!rotationIsZero)
    {
        script << "ObjectTemplate.fire.projectileStartRotation "
               << m_projectileStartRotation.x << '/'
               << m_projectileStartRotation.y << '/'
               << m_projectileStartRotation.z << std::endl;
    }

    if (m_randomRotationSpeed != 0.0f)
        script << "ObjectTemplate.fire.randomRotationSpeed "
               << m_randomRotationSpeed << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
