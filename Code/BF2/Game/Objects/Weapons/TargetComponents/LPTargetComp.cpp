// Path:     attested - retail Debug sites name
// Game/Objects/Weapons/TargetComponents/LPTargetComp.cpp.
// Contents: complete laser-painter target component and template, recovered from ELF
// ..0x82759CF and cross-checked against the Win32 PE.

#include "LPTargetComp.h"

#include <cfloat>
#include <cmath>
#include <sstream>

#include "../GenericFireArm.h"
#include "../../Game/TargetObject.h"
#include "../../TargetObjectTemplate.h"
#include "../../ICameraObject.h"
#include "../../IObject.h"
#include "../../IObjectTemplate.h"
#include "../../IPlayer.h"
#include "../../Projectiles/GenericProjectile.h"
#include "../../Projectiles/TVMissileTrace.h"
#include "../../../GameEvents/TargetDirectionEvent.h"
#include "../../../IGame.h"
#include "../../../IGameLogic.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/Ids.h"
#include "../../../../Core/System.h"
#include "../../../../IO/Stream/IStream.h"
#include "../../../../Physics/HeightmapCluster.h"
#include "../../../../Scene/Object/ObjectManager.h"
#include "../../../../Scene/Object/ObjectPredicator.h"
#include "../../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"

namespace dice {
namespace hfe {
namespace world {

extern IBase* heightmapCluster;

namespace {

IPlayerControlObject* findPco(IObject* object)
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

GenericProjectile* projectileFromObject(IObject* object)
{
    if (!object || !object->queryInterface(IID_IProjectileObject))
        return 0;
    return static_cast<GenericProjectile*>(object);
}

float length(const Vec3& value)
{
    return static_cast<float>(std::sqrt(
        value.x * value.x + value.y * value.y + value.z * value.z));
}

Vec3 subtract(const Vec3& lhs, const Vec3& rhs)
{
    Vec3 value = { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
    return value;
}

Vec3 addScaled(const Vec3& start, const Vec3& direction, float scale)
{
    Vec3 value = {
        start.x + direction.x * scale,
        start.y + direction.y * scale,
        start.z + direction.z * scale
    };
    return value;
}

void normalise(Vec3& value)
{
    const float lengthSq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (std::fabs(lengthSq - 1.0f) < FLT_EPSILON)
        return;
    if (std::fabs(lengthSq) < FLT_EPSILON)
    {
        value.x = value.y = value.z = 0.0f;
        return;
    }
    const float inverseLength = 1.0f / static_cast<float>(std::sqrt(lengthSq));
    value.x *= inverseLength;
    value.y *= inverseLength;
    value.z *= inverseLength;
}

bool terrainIntersection(const Vec3& start, const Vec3& end, Vec3& hitPoint)
{
    float fraction = 0.0f;
    Vec3 normal = { 0.0f, 0.0f, 0.0f };
    unsigned char material = 0;
    return static_cast<IHeightmapCluster*>(heightmapCluster)->intersectRayInWorldCoords(
        start, end, hitPoint, fraction, normal, material, true);
}

} // namespace

LPTargetComp::LPTargetComp(LPTargetCompTemplate* objectTemplate)
    : TargetComp(objectTemplate)
    , m_leafRefCount(1)
    , m_lastUpdateTick(0)
    , m_currentProjectile()
    , m_ownedTarget(0)
    , m_lpTemplate(objectTemplate)
{
}

LPTargetComp::~LPTargetComp()
{
}

void LPTargetComp::addRef() { ++m_leafRefCount; }
int LPTargetComp::getRef() { return m_leafRefCount; }

int LPTargetComp::release()
{
    const int remaining = --m_leafRefCount;
    if (remaining == 0 && this)
        delete this;
    return remaining;
}

void* LPTargetComp::queryInterface(unsigned int iid) const
{
    LPTargetComp* const self = const_cast<LPTargetComp*>(this);
    if (iid == IID_IBase || iid == IID_IObjectComponent)
        return self;
    if (iid == IID_IWeaponComp || iid == IID_IWeaponTargetComp)
        return static_cast<IWeaponTargetComp*>(self);
    return 0;
}

unsigned int LPTargetComp::getInterfaceID() const { return IID_IWeaponTargetComp; }
bool LPTargetComp::busy() { return false; }

void LPTargetComp::initComponent(WeaponInitEvent& event)
{
    if (gameLogic->isServer())
    {
        IObjectTemplate* const targetTemplate = m_lpTemplate->getTargetObjectTemplate();
        if (targetTemplate)
        {
            if (static_cast<const TargetObjectTemplate*>(targetTemplate)->targetType == 3)
            {
                m_ownedTarget = static_cast<TargetObject*>(targetTemplate->createObject());
                if (m_ownedTarget)
                    m_ownedTarget->init();
            }
            else
            {
                Vec3 zero = { 0.0f, 0.0f, 0.0f };
                m_ownedTarget = static_cast<TargetObject*>(
                    game->createObjectOnAllClients(targetTemplate, zero, zero, true, 0, 0));
                if (m_ownedTarget && m_lpTemplate->getTargetSystem() == TSLaserGuided)
                    m_ownedTarget->setTargetActive(false, true);
            }

            if (!m_ownedTarget)
            {
                Debug(DEBUGTYPE_6,
                      "Game/Objects/Weapons/TargetComponents/LPTargetComp.cpp", 86, __FUNCTION__)
                    << "TBM: unable to create TargetObject";
            }
        }
        else
        {
            Debug(DEBUGTYPE_6,
                  "Game/Objects/Weapons/TargetComponents/LPTargetComp.cpp", 94, __FUNCTION__)
                << "TBM: unable to create TargetObject";
        }
    }
    TargetComp::initComponent(event);
}

void LPTargetComp::projectileWasReset(IObject* projectile)
{
    if (m_currentProjectile.get() == projectile)
        setTargetObject(getTargetObject(), false, 0);

    if (m_currentProjectile.get())
    {
        m_currentProjectile = WeakPtr<IObject>();
        m_reloadLatch = true;

        if (m_onLockWeapon)
        {
            IPlayer* const player = firearm()->getPlayer();
            IObject* const vehicle = player ? player->getVehicle() : 0;
            if (vehicle && vehicle->getTemplate()->getClassID() == CID_PlayerControlObjectTemplate)
            {
                IPlayerControlObject* const pco = findPco(vehicle);
                IObject* const alternate = static_cast<IObject*>(
                    m_onLockWeapon->queryInterface(IID_IObject));
                pco->toggleOnWeaponLock(alternate);
            }
        }
    }
}

void LPTargetComp::destroy()
{
    if (m_ownedTarget)
        game->destroyObject(m_ownedTarget);
    setTargetObject(0, false, 0);
}

void LPTargetComp::enableTargetObject()
{
    TargetObject* const target = static_cast<TargetObject*>(getTargetObject());
    if (target)
    {
        target->setTargetActive(true, true);
        target->setTargetPlayerId(firearm()->getPlayer()->getId());
    }
}

void LPTargetComp::disableTargetObject()
{
    TargetObject* const target = static_cast<TargetObject*>(getTargetObject());
    if (target && target->getTargetActive())
        target->setTargetActive(false, true);
}

void LPTargetComp::fireStop(bool)
{
    const int targetSystem = getTargetSystem();
    IObject* const target = getTargetObject();
    if (targetSystem != TSWireGuided && targetSystem != TSTVGuided && target)
    {
        objectManager->postMessage(
            target, static_cast<TemplateMessage>(30),
            static_cast<const TargetObjectTemplate*>(target->getTemplate())->timeToLive);
    }
}

void LPTargetComp::enable()
{
    m_targetObject = m_ownedTarget ? m_ownedTarget->getWeakPtr() : WeakPtr<IObject>();
    TargetComp::enable();
}

void LPTargetComp::disable()
{
    GenericProjectile* const projectile = projectileFromObject(m_currentProjectile.get());
    if (projectile && projectile->isInProjectileCamera())
        projectile->disableProjectileCamera();
    m_currentProjectile = WeakPtr<IObject>();
    TargetComp::disable();
}

void LPTargetComp::reload(ReloadState state)
{
    setTargetObject(getTargetObject(), false, 0);
    if (m_lpTemplate->getDisableGuidanceOnReload())
        m_currentProjectile = WeakPtr<IObject>();
    TargetComp::reload(state);
}

void LPTargetComp::updateComponent(float dt, unsigned int tick)
{
    if (getTargetSystem() == TSWireGuided
        && static_cast<unsigned int>(m_lpTemplate->getUpdateIntervalTicks()
                                     + WorldPref::getTickCount()) > m_lastUpdateTick)
    {
        Vec3 start = { 0.0f, 0.0f, 0.0f };
        Vec3 end = { 0.0f, 0.0f, 0.0f };
        IPlayer* const player = firearm()->getPlayer();
        if (player)
        {
            ICameraObject* const camera = player->getCamera();
            IObject* const cameraObject = camera
                ? static_cast<IObject*>(camera->queryInterface(IID_IObject)) : 0;
            const Mat4& transform = cameraObject->getAbsoluteTransformation();
            start = cameraObject->getAbsolutePosition();
            Vec3 direction = { transform.m[8], transform.m[9], transform.m[10] };
            end = addScaled(start, direction, 100.0f);
        }
        else
        {
            const Mat4& transform = firearm()->getAbsoluteTransformation();
            start = firearm()->getAbsolutePosition();
            Vec3 direction = { transform.m[8], transform.m[9], transform.m[10] };
            end = addScaled(start, direction, 100.0f);
        }
        updateTargetObject(start, end);
    }
    TargetComp::updateComponent(dt, tick);
}

void LPTargetComp::postFire(WeaponFireEvent& event)
{
    TargetComp::postFire(event);
    setCurrentProjectile(event.projectile);
    GenericProjectile* const projectile = projectileFromObject(event.projectile);
    if (projectile)
        projectile->setTargetObject(getTargetObject());
    updateTargetObject(event.start, event.end);
}

void LPTargetComp::setCurrentProjectile(IObject* projectile)
{
    m_currentProjectile = projectile ? projectile->getWeakPtr() : WeakPtr<IObject>();
}

IObject* LPTargetComp::getProjectile() const
{
    return m_currentProjectile.get();
}

bool LPTargetComp::requestFire(bool released, bool alternate)
{
    if (alternate)
    {
        if (game->queryInterface(IID_IGameServer))
            setTargetObject(getTargetObject(), false, 0);
        return true;
    }

    if (!m_lpTemplate->getDisableFireWhenActiveMissile())
        return TargetComp::requestFire(released, alternate);
    if (m_currentProjectile.get())
        return false;
    return TargetComp::requestFire(released, alternate);
}

void LPTargetComp::setTargetObject(IObject* target, bool locked, IObject* vehicle)
{
    (void)getWeaponState();
    TargetComp::setTargetObject(target, locked, vehicle);
    if (locked)
        enableTargetObject();
    else
        disableTargetObject();
}

void LPTargetComp::setTargetDirection(Vec3 start, Vec3 end)
{
    if (tv_missile_trace::armed() && getTargetSystem() == TSTVGuided && gameLogic->isServer())
        tv_missile_trace::log(m_currentProjectile.get(), "steer_dispatch",
            "start=%.9g,%.9g,%.9g end=%.9g,%.9g,%.9g",
            start.x, start.y, start.z, end.x, end.y, end.z);
    if (getTargetSystem() == TSTVGuided && !gameLogic->isServer())
    {
        IObjectTemplate* const projectileTemplate = firearm()->getProjectileTemplate();
        if (!projectileTemplate->getName().empty())
            game->sendGameEvent(static_cast<unsigned int>(-1),
                                new TargetDirectionEvent(firearm(), start, end));
    }
    updateTargetObject(start, end);
}

bool LPTargetComp::getLaunched()
{
    return m_currentProjectile.get() != 0;
}

io::PlayerInputMap LPTargetComp::getTargetInput() const
{
    return m_lpTemplate->getTargetInput();
}

void LPTargetComp::updateTargetObject(Vec3 start, Vec3 end)
{
    // use the supplied camera/fire ray origin for both the
    // projectile-distance test and the obstruction segment. The weapon root is retained for its
    // bounding radius and the intersection predicator, but is not the segment origin.
    m_lastUpdateTick = static_cast<unsigned int>(WorldPref::getTickCount());
    if (std::fabs(start.x - end.x) <= FLT_EPSILON
        && std::fabs(start.y - end.y) <= FLT_EPSILON
        && std::fabs(start.z - end.z) <= FLT_EPSILON)
    {
        return;
    }

    GenericProjectile* projectile = projectileFromObject(m_currentProjectile.get());
    bool lockCandidate = false;
    IObject* const weaponRoot = getRootParent(firearm());

    if (getTargetSystem() != TSWireGuided)
    {
        lockCandidate = true;
    }
    else if (projectile)
    {
        const Vec3 projectilePosition = projectile->getAbsolutePosition();
        const Vec3 rootDelta = subtract(projectilePosition, start);
        Vec3 terrainHit = { 0.0f, 0.0f, 0.0f };
        if (length(rootDelta) > getMaxTargetDistance()
            || terrainIntersection(start, projectilePosition, terrainHit))
        {
            setTargetObject(getTargetObject(), false, 0);
            return;
        }

        const float rootRadius = weaponRoot->getPhysicBoundingRadius();
        const float projectileRadius = projectile->getPhysicBoundingRadius();
        const float rootToProjectile = length(rootDelta);
        if (rootToProjectile > rootRadius + projectileRadius)
        {
            Vec3 along = rootDelta;
            normalise(along);
            const Vec3 lineStart = addScaled(start, along, rootRadius);
            const Vec3 lineEnd = addScaled(projectilePosition, along, -projectileRadius);
            const Vec3 lineDirection = subtract(lineEnd, lineStart);
            IObject* hitObject = 0;
            Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
            Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
            int hitPart = 0;
            ProjectileVsTargetPredicator predicator(0x100, 0, weaponRoot, projectile);
            objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                         lineStart, lineDirection, &predicator,
                                         true, false, false, true, 0);
            if (hitObject == projectile)
                hitObject = 0;
            if (hitObject)
            {
                setTargetObject(getTargetObject(), false, 0);
                return;
            }
        }
        lockCandidate = true;
    }

    IObject* const target = getTargetObject();
    if (!target)
    {
        setTargetObject(0, false, 0);
        return;
    }

    Vec3 direction = subtract(end, start);
    normalise(direction);
    const Vec3 maximumEnd = addScaled(start, direction, getMaxTargetDistance());

    Vec3 terrainHit = { 0.0f, 0.0f, 0.0f };
    const bool hitTerrain = terrainIntersection(start, maximumEnd, terrainHit);
    const Vec3 rayEnd = hitTerrain ? terrainHit : maximumEnd;
    const Vec3 rayDirection = subtract(rayEnd, start);

    IObject* hitObject = 0;
    Vec3 objectHit = { 0.0f, 0.0f, 0.0f };
    Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
    int hitPart = 0;
    ProjectileVsTargetPredicator predicator(0x100, 0, weaponRoot, projectile);
    objectManager->intersectLine(hitObject, objectHit, hitNormal, hitPart,
                                 start, rayDirection, &predicator,
                                 true, false, false, true, 0);

    if (hitObject)
    {
        IObject* const hitRoot = getRootParent(hitObject);
        IPlayerControlObject* const hitPco = findPco(hitRoot);
        IPlayerControlObject* const weaponPco = findPco(firearm());
        if (hitPco && weaponPco && hitPco->getTeam() == weaponPco->getTeam())
            hitObject = 0;
    }

    Vec3 chosen = { 0.0f, 0.0f, 0.0f };
    if (hitObject)
    {
        chosen = objectHit;
        setTargetObject(target, lockCandidate, getRootParent(hitObject));
    }
    else if (hitTerrain)
    {
        chosen = terrainHit;
        setTargetObject(target, lockCandidate, 0);
    }
    else if (!m_targetTemplate->getSetTargetAtMaxDistance())
    {
        // Retail still writes the zero-initialized local to the target after clearing the lock.
        setTargetObject(target, false, 0);
    }
    else
    {
        float targetDistance = 1000.0f;
        if (projectile)
            targetDistance = projectile->getMaxTargetDistance();
        chosen = addScaled(start, direction,
                           targetDistance - weaponRoot->getPhysicBoundingRadius());
        setTargetObject(target, lockCandidate, 0);
    }

    if (getTargetObject())
        getTargetObject()->setAbsolutePosition(chosen);
}

LPTargetCompTemplate::LPTargetCompTemplate()
    : TargetCompTemplate()
    , m_leafRefCount(1)
    , m_targetObjectTemplateName()
    , m_targetObjectTemplate(0)
    , m_disableFireWhenActiveMissile(false)
    , m_disableGuidanceOnReload(false)
{
    setUpdateInterval(0.0f);
}

void LPTargetCompTemplate::addRef() { ++m_leafRefCount; }
int LPTargetCompTemplate::getRef() { return m_leafRefCount; }

int LPTargetCompTemplate::release()
{
    const int remaining = --m_leafRefCount;
    if (remaining == 0)
        delete this;
    return remaining;
}

void* LPTargetCompTemplate::queryInterface(unsigned int iid) const
{
    LPTargetCompTemplate* const self = const_cast<LPTargetCompTemplate*>(this);
    if (iid == IID_IBase || iid == IID_IObjectTemplateComponent || iid == IID_IWeaponCompTemplate)
        return static_cast<IObjectTemplateComponent*>(self);
    if (iid == io::IID_IConsoleSaveable)
        return static_cast<io::IConsoleSaveable*>(self);
    return 0;
}

unsigned int LPTargetCompTemplate::getClassID() const { return CID_LPTargetCompTemplate; }
unsigned int LPTargetCompTemplate::getInterfaceID() { return IID_IWeaponTargetCompTemplate; }
IObjectComponent* LPTargetCompTemplate::createComponent() { return new LPTargetComp(this); }
void LPTargetCompTemplate::objectCreated(IObject*) {}
void LPTargetCompTemplate::objectDestroyed(IObject*) {}
void LPTargetCompTemplate::setTemplate(IObjectTemplate*) {}

void* LPTargetCompTemplate::queryImplementation(unsigned int classId)
{
    if (classId == CID_LPTargetCompTemplate || classId == CID_TargetCompTemplate)
        return this;
    return 0;
}

void LPTargetCompTemplate::preCache() const
{
    IObjectTemplate* const objectTemplate = getTargetObjectTemplate();
    if (objectTemplate)
    {
        IObject* const object = objectTemplate->createObject();
        if (object)
        {
            object->init();
            object->forceDestroy();
        }
    }
}

IObjectTemplate* LPTargetCompTemplate::getTargetObjectTemplate() const
{
    if (!m_targetObjectTemplate)
        m_targetObjectTemplate = objectTemplateManager->getTemplate(m_targetObjectTemplateName);
    return m_targetObjectTemplate;
}

void LPTargetCompTemplate::setUpdateInterval(float seconds)
{
    m_updateInterval = seconds;
    m_updateIntervalTicks = static_cast<int>(seconds * 30.0f);
}

bool LPTargetCompTemplate::makeScript(io::IStream* stream)
{
    if (!TargetCompTemplate::makeScript(stream))
        return false;

    std::ostringstream script;
    if (m_updateInterval != 0.0f)
        script << "ObjectTemplate.target.updateInterval " << m_updateInterval << std::endl;
    if (!m_targetObjectTemplateName.empty())
        script << "ObjectTemplate.target.targetObjectTemplate "
               << m_targetObjectTemplateName << std::endl;
    if (!m_disableFireWhenActiveMissile)
        script << "ObjectTemplate.target.disableFireWhenActiveMissile "
               << m_disableFireWhenActiveMissile << std::endl;
    if (!m_disableGuidanceOnReload)
        script << "ObjectTemplate.target.disableGuidanceOnReload "
               << m_disableGuidanceOnReload << std::endl;

    const std::string text = script.str();
    return stream->write(text.c_str(), static_cast<unsigned int>(text.length())) == text.length();
}

} // namespace world
} // namespace hfe
} // namespace dice
