// Path:     [INFERRED] beside the attested LPTargetComp.cpp.
// Contents: TargetComp and TargetCompTemplate, recovered from ELF 0x8278020..0x8279C13 and
// cross-checked against the BF2 1.50 Win32 dedicated-server image.

#include "TargetComp.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "../GenericFireArm.h"
#include "../../Game/TargetObject.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../ObjectLayoutAccess.h"
#include "../../Projectiles/ProjectileCompTemplates.h"
#include "../../../IGame.h"
#include "../../../IGameLogic.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/StringUtil.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace world {

namespace {

IPlayerControlObject* findPlayerControlObject(IObject* object)
{
    for (; object; object = object->getParent())
    {
        IPlayerControlObject* const result = static_cast<IPlayerControlObject*>(
            object->queryInterface(IID_IPlayerControlObject));
        if (result)
            return result;
    }
    return 0;
}

// IGeometry is not reconstructed far enough to name this slot yet. Both shipped x86 tables were
// read: the same operation is ELF +0xD8 and PE +0xD4 (the Itanium destructor-pair shift).
// The slot follows the compiler's destructor ABI, so native MSVC x64 takes the PE arm too.
void geometrySetConnectionEffect(void* geometry, int enabled)
{
#if defined(_MSC_VER)
    typedef void (__thiscall *Method)(void*, int);
    const unsigned int slot = 0xD4 / 4;
#else
    typedef void (*Method)(void*, int);
    const unsigned int slot = 0xD8 / 4;
#endif
    void** const vtable = *reinterpret_cast<void***>(geometry);
    reinterpret_cast<Method>(vtable[slot])(geometry, enabled);
}

} // namespace

TargetComp::TargetComp(TargetCompTemplate* objectTemplate)
    : m_targetRefCount(1)
    , m_targetPoint()
    , m_onLockWeapon(0)
    , m_onLockWeaponLastTick(0)
    , m_targetObject()
    , m_targetLock(false)
    , m_lockVehicle()
    , m_reloadLatch(false)
    , m_signalChangeTarget(false)
    , m_dword3C(0)
    , m_connectionEffect(0)
    , m_zoomed(false)
    , m_allowTargetSeek(false)
    , m_targetTemplate(objectTemplate)
{
    m_refCount = 1;
    m_targetPoint.x = 0.0f;
    m_targetPoint.y = 0.0f;
    m_targetPoint.z = 0.0f;
}

TargetComp::~TargetComp()
{
}

void TargetComp::addRef() { ++m_targetRefCount; }
int TargetComp::getRef() { return m_targetRefCount; }

int TargetComp::release()
{
    const int remaining = --m_targetRefCount;
    if (remaining == 0 && this)
        delete this;
    return remaining;
}

void* TargetComp::queryInterface(unsigned int iid) const
{
    TargetComp* const self = const_cast<TargetComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return self;
    if (iid == IID_IWeaponComp || iid == IID_IWeaponTargetComp)
        return static_cast<IWeaponTargetComp*>(self);
    return 0;
}

unsigned int TargetComp::getInterfaceID() const { return IID_IWeaponTargetComp; }

void TargetComp::initComponent(WeaponInitEvent&)
{
    if (!m_targetTemplate->getOnLockChangeToWeaponName().empty())
    {
        IObject* const parent = firearm()->getParent();
        if (parent)
        {
            for (IObject* child = parent->getChild(); child; child = child->getSibling())
            {
                if (compareIgnoreCase(child->getTemplate()->getName(),
                                      m_targetTemplate->getOnLockChangeToWeaponName()) == 0)
                {
                    m_onLockWeapon = static_cast<IWeaponObject*>(
                        child->queryInterface(IID_IWeaponObject));
                }
            }
        }
    }

    if (!m_targetTemplate->getConnectionEffect().empty())
    {
        for (IObject* child = firearm()->getChild(); child; child = child->getSibling())
        {
            if (compareIgnoreCase(child->getTemplate()->getName(),
                                  m_targetTemplate->getConnectionEffect()) == 0)
            {
                m_connectionEffect = static_cast<IObject*>(child->queryInterface(IID_IObject));
                void* const geometry = object_detail::LayoutAccess::geometry(m_connectionEffect);
                if (geometry)
                    geometrySetConnectionEffect(geometry, 1);
                m_connectionEffect->updateFlags(0, 2);
            }
        }
    }
}

bool TargetComp::preUpdate() { return false; }

void TargetComp::updateComponent(float, unsigned int)
{
    IObject* const target = getTargetObject();
    if (target)
    {
        const Vec3& targetPosition = target->getAbsolutePosition();
        const Vec3& weaponPosition = firearm()->getAbsolutePosition();
        const float x = targetPosition.x - weaponPosition.x;
        const float y = targetPosition.y - weaponPosition.y;
        const float z = targetPosition.z - weaponPosition.z;
        const float distance = static_cast<float>(std::sqrt(x * x + y * y + z * z));

        if (m_targetLock
            && (!static_cast<TargetObject*>(target)->getTargetActive()
                || distance > m_targetTemplate->getMaxDistance()))
        {
            setTargetObject(getTargetObject(), false, 0);
        }
    }

    if (m_connectionEffect)
    {
        if (hasMissileConnection())
            m_connectionEffect->updateFlags(2, 0);
        else
            m_connectionEffect->updateFlags(0, 2);
    }
}

bool TargetComp::isSelectable() const { return true; }
void TargetComp::fireStop(bool) {}

bool TargetComp::requestFire(bool released, bool)
{
    if (released || !m_onLockWeapon)
        return true;
    if (m_onLockWeaponLastTick == static_cast<unsigned int>(WorldPref::getTickCount()))
        return true;
    if (m_targetLock != m_targetTemplate->getOnLockChangeToWeapon())
        return true;

    IPlayer* const player = firearm()->getPlayer();
    if (!player)
        return true;
    IObject* const vehicle = player->getVehicle();
    if (!vehicle || vehicle->getTemplate()->getClassID() != CID_PlayerControlObjectTemplate)
        return true;

    IPlayerControlObject* const pco = findPlayerControlObject(vehicle);
    GenericFireArm* const alternate = static_cast<GenericFireArm*>(
        static_cast<IObject*>(m_onLockWeapon->queryInterface(IID_IObject)));
    alternate->enableWeapon();
    alternate->handleUpdate(0.0f, 0);
    alternate->disableWeapon();
    if (!alternate->busy(false))
    {
        m_onLockWeaponLastTick = static_cast<unsigned int>(WorldPref::getTickCount());
        IObject* const alternateObject = static_cast<IObject*>(
            m_onLockWeapon->queryInterface(IID_IObject));
        pco->toggleOnWeaponLock(alternateObject);
        alternate->handleMessage(static_cast<TemplateMessage>(7), player, false);
    }
    return false;
}

void TargetComp::postFire(WeaponFireEvent&) {}
void TargetComp::fireComplete(unsigned int) {}
void TargetComp::postSwitchFireRate(FireRate) {}
bool TargetComp::requestReload(bool) { return true; }
void TargetComp::reload(ReloadState) { m_reloadLatch = true; }
void TargetComp::postReload() {}
void TargetComp::outOfAmmo(bool) {}
void TargetComp::stoppedTracking() {}
void TargetComp::postChangeTarget(IObject*) {}
void TargetComp::projectileWasReset(IObject*) {}

void TargetComp::enable()
{
    IPlayer* const player = firearm()->getPlayer();
    if (player && getTargetObject())
        static_cast<TargetObject*>(getTargetObject())->setTargetPlayerId(player->getId());
    m_lockVehicle = WeakPtr<IObject>();
    m_zoomed = false;
}

void TargetComp::disable()
{
    setTargetObject(0, false, 0);
}

void TargetComp::stop() {}
bool TargetComp::busy() { return false; }
void TargetComp::exit() {}
void TargetComp::destroy() {}
void TargetComp::pickup() {}
void TargetComp::reset() {}

void TargetComp::setWeaponState(unsigned int state)
{
    if (m_targetTemplate->getTargetSystem() == TSWireGuided)
        m_targetLock = state == 1;
}

unsigned int TargetComp::getWeaponState() const
{
    if (m_targetTemplate->getTargetSystem() == TSWireGuided)
        return m_targetLock ? 1u : 0u;
    return 0;
}

void TargetComp::setZoom(int camera)
{
    m_zoomed = camera > 0;
}

void TargetComp::setAllowTargetSeek(bool allow)
{
    m_allowTargetSeek = allow;
}

IObject* TargetComp::getTargetObject() const
{
    return m_targetObject.get();
}

bool TargetComp::hasTargetLock() const
{
    return getTargetObject() != 0 && m_targetLock;
}

void TargetComp::setTargetObject(IObject* target, bool locked, IObject* vehicle)
{
    const bool lockChanged = m_targetLock != locked && (target || !locked);
    const bool targetChanged = target != getTargetObject() || vehicle != m_lockVehicle.get();

    if (!m_targetTemplate->getOnLockChangeToWeaponName().empty() && !m_onLockWeapon)
    {
        WeaponInitEvent event = { 0, 0 };
        initComponent(event);
    }

    if (targetChanged)
        m_targetObject = target ? target->getWeakPtr() : WeakPtr<IObject>();

    if (m_targetTemplate->getTargetSystem() == TSWireGuided && !gameLogic->isServer())
        return;

    if (lockChanged)
        m_targetLock = locked;
    if (m_targetTemplate->getSendVehicleLockWarning() && (lockChanged || targetChanged))
        lockVehicle(vehicle);
}

void TargetComp::setTargetDirection(Vec3, Vec3) {}

bool TargetComp::getSignalChangeTarget()
{
    const bool result = m_signalChangeTarget;
    m_signalChangeTarget = false;
    return result;
}

void TargetComp::setCurrentProjectile(IObject*) {}

int TargetComp::getTargetObjectNetworkId() const
{
    IObject* const target = getTargetObject();
    return target ? static_cast<TargetObject*>(target)->getTargetObjectId() : 0;
}

// template slot +0x60 / +0x5C is
// getComponentFromClassID. The seek component is not an implementation of the projectile template.
int TargetComp::getTargetType() const
{
    IObjectTemplate* const projectileTemplate = firearm()->getProjectileTemplate();
    if (!projectileTemplate)
        return 0;
    SeekClosestTargetCompTemplate* const seek =
        static_cast<SeekClosestTargetCompTemplate*>(
            projectileTemplate->getComponentFromClassID(CID_SeekClosestTargetCompTemplate));
    return seek ? static_cast<int>(seek->getTargetType()) : 0;
}

int TargetComp::getTargetSystem() const
{
    return static_cast<int>(m_targetTemplate->getTargetSystem());
}

// uses the same attached-component lookup before limiting the range.
float TargetComp::getMaxTargetDistance() const
{
    const float authored = m_targetTemplate->getMaxDistance();
    IObjectTemplate* const projectileTemplate = firearm()->getProjectileTemplate();
    if (!projectileTemplate)
        return authored;
    SeekClosestTargetCompTemplate* const seek =
        static_cast<SeekClosestTargetCompTemplate*>(
            projectileTemplate->getComponentFromClassID(CID_SeekClosestTargetCompTemplate));
    return seek ? std::min(authored, seek->getMaxTargetDistance()) : authored;
}

IObject* TargetComp::getProjectile() const { return 0; }
float TargetComp::getDynamicVelocity() const { return -1.0f; }

bool TargetComp::hasMissileConnection() const
{
    return m_targetLock
        && (m_zoomed || !m_targetTemplate->getConnectionEffectOnlyWhenZoomed());
}

io::PlayerInputMap TargetComp::getTargetInput() const
{
    return m_targetTemplate->getTargetInput();
}

bool TargetComp::getLaunched() { return false; }

void TargetComp::lockVehicle(IObject* vehicle)
{
    if (!vehicle && !m_lockVehicle.get())
        return;

    MissileLockMessage message;
    unsigned char* const bytes = reinterpret_cast<unsigned char*>(&message);
    bytes[1] = 1;
    bytes[2] = m_targetLock;
    message.field08 = firearm()->getObjectId();
    bytes[12] = static_cast<unsigned char>(getTargetType());

    IObject* const oldVehicle = m_lockVehicle.get();
    if (oldVehicle && oldVehicle != vehicle)
    {
        bytes[0] = static_cast<unsigned char>(getLaunched());
        message.field04 = -1;
        if (gameLogic->isServer())
            game->sendMissileMessageToOccupyingPlayers(oldVehicle, message);
        m_lockVehicle = WeakPtr<IObject>();
    }

    if (vehicle)
    {
        bytes[0] = static_cast<unsigned char>(getLaunched());
        message.field04 = getTargetObjectNetworkId();
        if (gameLogic->isServer())
            game->sendMissileMessageToOccupyingPlayers(vehicle, message);
        m_lockVehicle = vehicle->getWeakPtr();
    }
}

TargetCompTemplate::TargetCompTemplate()
    : m_refCount(1)
    , m_targetInput(io::PINone)
    , m_unresolvedString()
    , m_onLockChangeToWeaponName()
    , m_onLockChangeToWeapon(false)
    , m_sendVehicleLockWarning(true)
    , m_connectionEffect()
    , m_connectionEffectOnlyWhenZoomed(false)
    , m_maxDistance(100.0f)
    , m_setTargetAtMaxDistance(false)
    , m_useCameraDirectionToTarget(false)
    , m_targetSystem(TSNone)
{
    // The twelve-byte region and three padding ranges are deliberately untouched.
}

void TargetCompTemplate::addRef() { ++m_refCount; }
int TargetCompTemplate::getRef() { return m_refCount; }

int TargetCompTemplate::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* TargetCompTemplate::queryInterface(unsigned int iid) const
{
    TargetCompTemplate* const self = const_cast<TargetCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IWeaponCompTemplate)
        return static_cast<IObjectTemplateComponent*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int TargetCompTemplate::getClassID() const { return CID_TargetCompTemplate; }
unsigned int TargetCompTemplate::getInterfaceID() { return IID_IWeaponTargetCompTemplate; }
void TargetCompTemplate::objectCreated(IObject*) {}
void TargetCompTemplate::objectDestroyed(IObject*) {}
void TargetCompTemplate::setTemplate(IObjectTemplate*) {}

void* TargetCompTemplate::queryImplementation(unsigned int classId)
{
    return classId == CID_TargetCompTemplate ? this : 0;
}

void TargetCompTemplate::setOnLockChangeToWeapon(bool enabled, std::string weaponName)
{
    m_onLockChangeToWeaponName = weaponName;
    m_onLockChangeToWeapon = enabled;
}

bool TargetCompTemplate::makeScript(io::IStream* stream)
{
    std::ostringstream script;
    if (m_targetInput != io::PINone)
        script << "ObjectTemplate.target.targetInput " << m_targetInput << std::endl;
    if (!m_onLockChangeToWeaponName.empty())
        script << "ObjectTemplate.target.onLockChangeToWeapon "
               << m_onLockChangeToWeapon << ' ' << m_onLockChangeToWeaponName << std::endl;
    if (m_maxDistance != 100.0f)
        script << "ObjectTemplate.target.maxDistance " << m_maxDistance << std::endl;
    if (m_setTargetAtMaxDistance)
        script << "ObjectTemplate.target.setTargetAtMaxDistance "
               << m_setTargetAtMaxDistance << std::endl;
    if (m_targetSystem != TSNone)
        script << "ObjectTemplate.target.targetSystem " << m_targetSystem << std::endl;
    if (!m_connectionEffect.empty())
        script << "ObjectTemplate.target.connectionEffect " << m_connectionEffect << std::endl;
    if (m_connectionEffectOnlyWhenZoomed)
        script << "ObjectTemplate.target.connectionEffectOnlyWhenZoomed "
               << m_connectionEffectOnlyWhenZoomed << std::endl;
    if (m_useCameraDirectionToTarget)
        script << "ObjectTemplate.target.useCameraDirectionToTarget "
               << m_useCameraDirectionToTarget << std::endl;
    if (!m_sendVehicleLockWarning)
        script << "ObjectTemplate.target.sendVehicleLockWarning "
               << m_sendVehicleLockWarning << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

std::ostream& operator<<(std::ostream& stream, TargetSystem value)
{
    switch (value)
    {
    case TSNone:        stream << "TSNone";        break;
    case TSHeatSeeking: stream << "TSHeatSeeking"; break;
    case TSLaserGuided: stream << "TSLaserGuided"; break;
    case TSWireGuided:  stream << "TSWireGuided";  break;
    case TSTVGuided:    stream << "TSTVGuided";    break;
    case TSParabellic:  stream << "TSParabellic";  break;
    default: break;
    }
    return stream;
}

std::istream& operator>>(std::istream& stream, TargetSystem& value)
{
    std::string token;
    stream >> token;
    if (token.length() == 1)
    {
        std::ostringstream normalized;
        normalized << static_cast<TargetSystem>(std::atoi(token.c_str()));
        token = normalized.str();
    }

    if (equalsIgnoreCase(token, "TSNone")) value = TSNone;
    else if (equalsIgnoreCase(token, "TSWireGuided")) value = TSWireGuided;
    else if (equalsIgnoreCase(token, "TSTVGuided")) value = TSTVGuided;
    else if (equalsIgnoreCase(token, "TSHeatSeeking")) value = TSHeatSeeking;
    else if (equalsIgnoreCase(token, "TSLaserGuided")) value = TSLaserGuided;
    else if (equalsIgnoreCase(token, "TSParabellic")) value = TSParabellic;
    return stream;
}

} // namespace world
} // namespace hfe
} // namespace dice
