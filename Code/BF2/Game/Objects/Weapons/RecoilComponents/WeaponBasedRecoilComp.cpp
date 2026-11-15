// Path:     [INFERRED] Game/Objects/Weapons/RecoilComponents/WeaponBasedRecoilComp.cpp.
// Contents: complete recoil base and WeaponBased leaf recovered from ELF 0x827BF20..0x827F88A
// and cross-checked against PE 0x5A8770..0x5A9166.

#include "WeaponBasedRecoilComp.h"

#include <cmath>
#include <sstream>
#include <string>

#include "../../Base/Camera.h"
#include "../../IObject.h"
#include "../../ObjectLayoutAccess.h"
#include "../../../GameLogic/GameLogicExplosion.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Physics/IPhysicsNode.h"
#include "../../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

const float kTicksPerSecond = 30.0f;
const std::vector<float> g_emptyRecoilVector;

} // namespace

IWeaponRecoilComp::~IWeaponRecoilComp()
{
}

RecoilComp::RecoilComp(RecoilCompTemplate* objectTemplate)
    : m_recoilRefCount(1)
    , m_defaultDistribution()
    , m_recoilTemplate(objectTemplate)
{
    // WeaponComp has no emitted constructor.  Retail seeds the primary count here and leaves both
    // attachment pointers untouched until Object::setComponent calls setObject/init.
    WeaponComp::m_refCount = 1;
}

RecoilComp::~RecoilComp()
{
}

void RecoilComp::addRef() { ++m_recoilRefCount; }
int RecoilComp::getRef() { return m_recoilRefCount; }

int RecoilComp::release()
{
    const int remaining = --m_recoilRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* RecoilComp::queryInterface(unsigned int iid) const
{
    RecoilComp* const self = const_cast<RecoilComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponRecoilComp)
        return static_cast<IWeaponRecoilComp*>(self);
    return 0;
}

unsigned int RecoilComp::getInterfaceID() const { return IID_IWeaponRecoilComp; }
void RecoilComp::initComponent(WeaponInitEvent&) {}
bool RecoilComp::preUpdate() { return false; }
void RecoilComp::updateComponent(float, unsigned int) {}
bool RecoilComp::isSelectable() const { return true; }
void RecoilComp::fireStop(bool) {}
bool RecoilComp::requestFire(bool, bool) { return true; }
void RecoilComp::postFire(WeaponFireEvent&) {}
void RecoilComp::fireComplete(unsigned int) {}
void RecoilComp::postSwitchFireRate(FireRate) {}
bool RecoilComp::requestReload(bool) { return true; }
void RecoilComp::reload(ReloadState) {}
void RecoilComp::postReload() {}
void RecoilComp::outOfAmmo(bool) {}
void RecoilComp::stoppedTracking() {}
void RecoilComp::postChangeTarget(IObject*) {}
void RecoilComp::projectileWasReset(IObject*) {}
void RecoilComp::enable() {}
void RecoilComp::disable() {}
void RecoilComp::stop() {}
bool RecoilComp::busy() { return false; }
void RecoilComp::exit() {}
void RecoilComp::destroy() {}
void RecoilComp::pickup() {}
void RecoilComp::reset() {}
void RecoilComp::setZoom(int) {}
float RecoilComp::getRecoilForceModifier() const { return 1.0f; }
float RecoilComp::getPitchRecoil() { return 0.0f; }
float RecoilComp::getYawRecoil() { return 0.0f; }
const RandomDistribution& RecoilComp::getRandomRecoilForceLeftRight() const
{
    return m_defaultDistribution;
}
const RandomDistribution& RecoilComp::getRandomRecoilForceUp() const
{
    return m_defaultDistribution;
}
bool RecoilComp::getGoBackOnRecoil() const { return false; }
bool RecoilComp::getHasRandomRecoilForce() const { return false; }
const std::vector<float>& RecoilComp::getRecoilVector() const
{
    return g_emptyRecoilVector;
}

WeaponBasedRecoilComp::WeaponBasedRecoilComp(WeaponBasedRecoilCompTemplate* objectTemplate)
    : RecoilComp(objectTemplate)
    , m_weaponRefCount(1)
    , m_savedRelativePosition(Vec3::getZero())
    , m_recoilEndTick(0)
    , m_returnEndTick(0)
    , m_weaponTemplate(objectTemplate)
{
}

WeaponBasedRecoilComp::~WeaponBasedRecoilComp()
{
}

void WeaponBasedRecoilComp::addRef() { ++m_weaponRefCount; }
int WeaponBasedRecoilComp::getRef() { return m_weaponRefCount; }

int WeaponBasedRecoilComp::release()
{
    const int remaining = --m_weaponRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* WeaponBasedRecoilComp::queryInterface(unsigned int iid) const
{
    WeaponBasedRecoilComp* const self = const_cast<WeaponBasedRecoilComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return static_cast<IObjectComponent*>(self);
    if (iid == IID_IWeaponComp || iid == IID_IWeaponRecoilComp)
        return static_cast<IWeaponRecoilComp*>(self);
    return 0;
}

unsigned int WeaponBasedRecoilComp::getInterfaceID() const
{
    return IID_IWeaponRecoilComp;
}

void WeaponBasedRecoilComp::initComponent(WeaponInitEvent&)
{
}

// , 563 B.  The size difference is compiler matrix-copy
// expansion; both ports implement the same two-phase translation along the weapon's relative Z
// row, then restore the exact saved translation and clear both tick latches.
void WeaponBasedRecoilComp::updateComponent(float, unsigned int)
{
    if (m_weaponTemplate->getRecoilSpeed() <= 0.0f || m_recoilEndTick == 0)
        return;

    if (m_recoilEndTick < static_cast<unsigned int>(WorldPref::getTickCount()))
    {
        if (m_returnEndTick < static_cast<unsigned int>(WorldPref::getTickCount()))
        {
            Mat4 transform = m_initObject->getRelativeTransformation();
            transform.m[12] = m_savedRelativePosition.x;
            transform.m[13] = m_savedRelativePosition.y;
            transform.m[14] = m_savedRelativePosition.z;
            m_initObject->setRelativeTransformation(transform);
            m_recoilEndTick = 0;
            m_returnEndTick = 0;
            return;
        }

        const float seconds = static_cast<float>(
            m_returnEndTick - static_cast<unsigned int>(WorldPref::getTickCount()))
            / kTicksPerSecond;
        Mat4 transform = m_initObject->getRelativeTransformation();
        const float phase = seconds / m_weaponTemplate->getRecoilSpeedGBPeriod();

        float x = transform.m[8] * m_weaponTemplate->getRecoilSize();
        float y = transform.m[9] * m_weaponTemplate->getRecoilSize();
        float z = transform.m[10] * m_weaponTemplate->getRecoilSize();
        x *= 0.050000001f;
        y *= 0.050000001f;
        z *= 0.050000001f;
        x *= -0.5f;
        y *= -0.5f;
        z *= -0.5f;

        transform.m[12] = m_savedRelativePosition.x + x * phase;
        transform.m[13] = m_savedRelativePosition.y + y * phase;
        transform.m[14] = m_savedRelativePosition.z + z * phase;
        m_initObject->setRelativeTransformation(transform);
        return;
    }

    const float seconds = static_cast<float>(
        m_recoilEndTick - static_cast<unsigned int>(WorldPref::getTickCount()))
        / kTicksPerSecond;
    Mat4 transform = m_initObject->getRelativeTransformation();

    float x = transform.m[8] * m_weaponTemplate->getRecoilSize();
    float y = transform.m[9] * m_weaponTemplate->getRecoilSize();
    float z = transform.m[10] * m_weaponTemplate->getRecoilSize();
    x *= 0.050000001f;
    y *= 0.050000001f;
    z *= 0.050000001f;
    x *= -0.5f;
    y *= -0.5f;
    z *= -0.5f;

    const float phase = sinf(
        (0.5f * m_weaponTemplate->getRecoilSpeedPeriod() + seconds)
        * m_weaponTemplate->getRecoilSpeed());
    transform.m[12] = m_savedRelativePosition.x + x * phase;
    transform.m[13] = m_savedRelativePosition.y + y * phase;
    transform.m[14] = m_savedRelativePosition.z + z * phase;
    m_initObject->setRelativeTransformation(transform);
}

// , 493 B.  Ordering is significant: wake/apply force and the
// optional explosion precede the saved-relative-position snapshot and both timer writes; the PCO
// walk and camera impulse come last.
void WeaponBasedRecoilComp::postFire(WeaponFireEvent& event)
{
    IObject* const root = getRootParent(m_initObject);
    const float force = m_weaponTemplate->getRecoilForce();
    if (force > 0.0f)
    {
        IPhysicsNode* const physics = static_cast<IPhysicsNode*>(
            object_detail::LayoutAccess::physicsNode(root));
        physics->setIsAwake();

        const Mat4& transform = m_initObject->getAbsoluteTransformation();
        Vec3 acceleration;
        acceleration.x = -transform.m[8] * force;
        acceleration.y = -transform.m[9] * force;
        acceleration.z = -transform.m[10] * force;
        physics->addAccelerationAtAbsolutePosition(
            m_initObject->getAbsolutePosition(), acceleration);

        if (force > 5.0f)
        {
            const float explosionRadius = force > 12.0f ? 12.0f : force;
            GameLogicExplosion::handleExplosion(
                m_initObject->getAbsolutePosition(), explosionRadius, 0, 0.0f,
                0, 0, 0, 0, 0, force, 0, 0, 1.0f, true, false);
        }
    }

    if (m_recoilEndTick == 0 && m_returnEndTick == 0)
    {
        const Mat4& transform = m_initObject->getRelativeTransformation();
        m_savedRelativePosition.x = transform.m[12];
        m_savedRelativePosition.y = transform.m[13];
        m_savedRelativePosition.z = transform.m[14];
    }

    // BF2.exe 0x60A077..0x60A4FE (ledger N2 17b) gives the local occupant's Falcon a "RecoilPunch" here,
    // between the snapshot and the timers; it needs a device (archived, Code/Archive/NovintFalcon).

    const unsigned int tick = static_cast<unsigned int>(WorldPref::getTickCount());
    const int recoilTicks = static_cast<int>(
        (m_weaponTemplate->getRecoilSpeedPeriod() * kTicksPerSecond) * 0.5f);
    m_recoilEndTick = tick + recoilTicks;
    const int returnTicks = static_cast<int>(
        m_weaponTemplate->getRecoilSpeedGBPeriod() * kTicksPerSecond);
    m_returnEndTick = m_recoilEndTick + returnTicks;

    IPlayerControlObject* pco = 0;
    for (IObject* object = m_initObject; object && !pco; object = object->getParent())
    {
        pco = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
    }

    if (pco)
    {
        ICameraObject* const cameraInterface = pco->getActiveCamera();
        Camera* const camera = cameraInterface
            ? static_cast<Camera*>(cameraInterface)
            : 0;
        if (camera && m_weaponTemplate->getCameraRecoilSize() > 0.0f)
        {
            camera->addRecoilImpulse(m_weaponTemplate->getCameraRecoilSpeed(),
                                     m_weaponTemplate->getCameraRecoilSize());
        }
    }
}

RecoilCompTemplate::RecoilCompTemplate()
{
}

RecoilCompTemplate::~RecoilCompTemplate()
{
}

unsigned int RecoilCompTemplate::getClassID() const { return CID_RecoilCompTemplate; }
unsigned int RecoilCompTemplate::getInterfaceID() { return IID_IWeaponRecoilCompTemplate; }
void RecoilCompTemplate::objectCreated(IObject*) {}
void RecoilCompTemplate::objectDestroyed(IObject*) {}
void RecoilCompTemplate::setTemplate(IObjectTemplate*) {}

void* RecoilCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == CID_RecoilCompTemplate ? this : 0;
}

bool RecoilCompTemplate::makeScript(io::IStream* stream)
{
    const std::string text;
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

WeaponBasedRecoilCompTemplate::WeaponBasedRecoilCompTemplate()
    : RecoilCompTemplate()
    , m_refCount(1)
    , m_recoilForce(0.0f)
    , m_recoilSpeed(0.0f)
    , m_recoilSpeedGB(0.0f)
    , m_recoilSize(0.0f)
    , m_recoilSpeedPeriod(0.0f)
    , m_recoilSpeedGBPeriod(0.0f)
    , m_cameraRecoilSpeed(0.0f)
    , m_cameraRecoilSize(0.0f)
{
    // The ELF emits the setters; PE folds the same zero results into the constructor.
    setRecoilSpeed(0.0f);
    setRecoilSpeedGB(0.0f);
}

WeaponBasedRecoilCompTemplate::~WeaponBasedRecoilCompTemplate()
{
}

void WeaponBasedRecoilCompTemplate::addRef() { ++m_refCount; }
int WeaponBasedRecoilCompTemplate::getRef() { return m_refCount; }

int WeaponBasedRecoilCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* WeaponBasedRecoilCompTemplate::queryInterface(unsigned int iid) const
{
    WeaponBasedRecoilCompTemplate* const self =
        const_cast<WeaponBasedRecoilCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IWeaponCompTemplate)
        return static_cast<IWeaponCompTemplate*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int WeaponBasedRecoilCompTemplate::getClassID() const
{
    return CID_WeaponBasedRecoilCompTemplate;
}

IObjectComponent* WeaponBasedRecoilCompTemplate::createComponent()
{
    return new WeaponBasedRecoilComp(this);
}

void* WeaponBasedRecoilCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_WeaponBasedRecoilCompTemplate || classId == CID_RecoilCompTemplate)
        return this;
    return 0;
}

void WeaponBasedRecoilCompTemplate::setRecoilSpeed(float value)
{
    m_recoilSpeed = value;
    m_recoilSpeedPeriod = value != 0.0f ? 3.1400001f / value : 0.0f;
}

void WeaponBasedRecoilCompTemplate::setRecoilSpeedGB(float value)
{
    m_recoilSpeedGB = value;
    m_recoilSpeedGBPeriod = value != 0.0f ? 1.5700001f / value : 0.0f;
}

bool WeaponBasedRecoilCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (m_recoilSize != 0.0f)
        script << "ObjectTemplate.recoil.recoilSize " << m_recoilSize << std::endl;
    if (m_recoilSpeed != 0.0f)
        script << "ObjectTemplate.recoil.recoilSpeed " << m_recoilSpeed << std::endl;
    // Retail tests the derived +0x20 period, then serialises the authored +0x14 speed.
    if (m_recoilSpeedGBPeriod != 0.0f)
        script << "ObjectTemplate.recoil.recoilSpeedGB " << m_recoilSpeedGB << std::endl;
    if (m_recoilForce != 0.0f)
        script << "ObjectTemplate.recoil.recoilForce " << m_recoilForce << std::endl;
    if (m_cameraRecoilSpeed != 0.0f)
        script << "ObjectTemplate.recoil.cameraRecoilSpeed " << m_cameraRecoilSpeed << std::endl;
    if (m_cameraRecoilSize != 0.0f)
        script << "ObjectTemplate.recoil.cameraRecoilSize " << m_cameraRecoilSize << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
