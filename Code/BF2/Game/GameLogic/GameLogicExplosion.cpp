// Queued explosion processing and authoritative damage/force application.
#include "GameLogicExplosion.h"

#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdlib>
#include <map>

#include "GameLogicDamage.h"
#include "../IGameLogic.h"
#include "../Objects/IObject.h"
#include "../Objects/IObjectTemplate.h"
#include "../Objects/IPlayer.h"
#include "../Objects/ISoldier.h"
#include "../Objects/ICameraObject.h"
#include "../Objects/ObjectLayoutAccess.h"
#include "../Objects/Game/Soldier.h"
#include "../Objects/Game/TriggerManager.h"
#include "../Objects/Game/DestroyableWindowsBundle.h"
#include "../Objects/GameLogicComp/IArmorGLComp.h"
#include "../Objects/Projectiles/GenericProjectile.h"
#include "../Objects/Fx/PersistentExplosionEffectManager.h"
#include "../Python/PythonHost.h"
#include "../Python/PyRef.h"
#include "../Utility/Vars.h"
#include "../../Core/EventManager.h"
#include "../../Core/Ids.h"
#include "../../Math/LocalBoundingBox.h"
#include "../../Physics/ICollisionMesh.h"
#include "../../Physics/IPhysicsNode.h"
#include "../../Physics/LatencyCompensation.h"
#include "../../Physics/Material.h"
#include "../../Physics/MaterialManager.h"
#include "../../Scene/Object/ObjectManager.h"
#include "../../Scene/Object/ObjectPredicator.h"
#include "../../Scene/Object/PlayerManager.h"
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../../Sound/BfSound.h"
#include "../../AI/AIInterface/BFKnowledge/BFCollisionHandler.h"
#include "../../AI/AIDLL/AI/Main/CommonSingletonServer.h"   // m_iBFCollisionHandler

namespace dice {
namespace hfe {

namespace world {

extern IBase* materialManager;

// The eight double-buffered queues and the buffer index. Every name is the ELF's own symbol; the
// addresses are in the header.
std::vector<WeakPtr<IObject> > GameLogicExplosion::m_explosionObject[2];
std::vector<WeakPtr<IObject> > GameLogicExplosion::m_explosionSourceObject[2];
std::vector<float>             GameLogicExplosion::m_explosionDamage[2];
std::vector<WeakPtr<IPlayer> > GameLogicExplosion::m_explosionPlayer[2];
std::vector<unsigned int>      GameLogicExplosion::m_explosionTeam[2];
std::vector<WeakPtr<IObject> > GameLogicExplosion::m_explosionWeapon[2];
std::vector<WeakPtr<IObject> > GameLogicExplosion::m_explosionVehicle[2];
std::vector<Vec3>              GameLogicExplosion::m_explosionPos[2];
int                            GameLogicExplosion::m_currentExplosion = 0;

namespace {

float* const g_ownVehicleExpMod = Vars::getFloat("explosion-camshake-ownvehicle-mod", 0.1f);
float* const g_expShakeMod = Vars::getFloat("explosion-camshake-mod", 4.0f);
float* const g_expShakeMaxForce = Vars::getFloat("explosion-camshake-maxforce", 20.0f);
float* const g_expShakeMaxDamage = Vars::getFloat("explosion-camshake-maxdamage", 150.0f);
float* const g_expShakeInsideVehicleMod =
    Vars::getFloat("explosion-camshake-insidevehicle-mod", 0.2f);
float* const g_soundTinnitusRange = Vars::getFloat("sound-tinnitus-range", 0.6f);
float* const g_explosionHotspot = Vars::getFloat("explosion-hotspot", 0.2f);
float* const g_explosionHotspotRay = Vars::getFloat("explosion-hotspot-ray", 0.02f);
float* const g_explosionHotspotRayMax = Vars::getFloat("explosion-hotspot-ray-max", 0.5f);
float* const g_explosionSoldierForceYMod =
    Vars::getFloat("explosion-soldier-expforce-ymod", 5.0f);
int* const g_explosionLatencyComp = Vars::getInt("explosion-latencyComp", 1);

inline MaterialManager* materials()
{
    return static_cast<MaterialManager*>(materialManager);
}

IArmorGLComp* findArmor(IObject* start)
{
    for (IObject* object = start; object; object = object->getParent())
    {
        IObjectComponent* const component = object_detail::LayoutAccess::armorComponent(object);
        if (component)
            return static_cast<IArmorGLComp*>(component);
    }
    return 0;
}

IPlayerControlObject* findPco(const IObject* start)
{
    for (const IObject* object = start; object; object = object->getParent())
    {
        void* const face = object->queryInterface(IID_IPlayerControlObject);
        if (face)
            return static_cast<IPlayerControlObject*>(face);
    }
    return 0;
}

inline IPhysicsNode* physicsNode(IObject* object)
{
    return static_cast<IPhysicsNode*>(object_detail::LayoutAccess::physicsNode(object));
}

inline ICollisionMesh* collisionMesh(IObject* object)
{
    return static_cast<ICollisionMesh*>(object_detail::LayoutAccess::collisionMesh(object));
}

inline float lengthSquared(const Vec3& value)
{
    return value.x * value.x + value.y * value.y + value.z * value.z;
}

inline float length(const Vec3& value)
{
    return sqrtf(lengthSquared(value));
}

void normaliseIfNeeded(Vec3& value)
{
    const float squared = lengthSquared(value);
    if (fabsf(squared - 1.0f) >= FLT_EPSILON)
    {
        if (fabsf(squared) >= FLT_EPSILON)
        {
            const float inverse = 1.0f / sqrtf(squared);
            value.x *= inverse;
            value.y *= inverse;
            value.z *= inverse;
        }
        else
        {
            value.x = 0.0f;
            value.y = 0.0f;
            value.z = 0.0f;
        }
    }
}

Vec3 transformPoint(const Mat4& transform, const Vec3& point)
{
    Vec3 result;
    result.x = transform.m[0] * point.x + transform.m[4] * point.y
             + transform.m[8] * point.z + transform.m[12];
    result.y = transform.m[1] * point.x + transform.m[5] * point.y
             + transform.m[9] * point.z + transform.m[13];
    result.z = transform.m[2] * point.x + transform.m[6] * point.y
             + transform.m[10] * point.z + transform.m[14];
    return result;
}

class FlagIgnoreWaterPredicator : public ObjectFlagPredicator
{
public:
    explicit FlagIgnoreWaterPredicator(unsigned int flags) : ObjectFlagPredicator(flags) {}

    virtual bool predicateSurface(int surface)
    {
        return !materials()->getMaterialPtr(surface)->getHasWaterPhysics();
    }
};

GenericProjectile* projectileFromObject(IObject* object)
{
    if (!object)
        return 0;
    void* const face = object->queryInterface(IID_IProjectileObject);
    return face
        ? static_cast<GenericProjectile*>(static_cast<IProjectileObject*>(face))
        : 0;
}

// isObjectWithinCone - ELF 0x80E2040, 656 B / PE recovered alongside serverHandleExplosion.
// The source projectile's detonation component supplies an optional forward cone. The target's
// centre is transformed into projectile-local space and its LOD-1 box half-extents enlarge the
// two transverse tests. The seemingly useless source-position fetch is present in both ports.
bool isObjectWithinCone(IObject* object, IObject* sourceProjectile)
{
    GenericProjectile* const projectile = projectileFromObject(sourceProjectile);
    const float angle = projectile ? projectile->getExplosionConeAngle() : 0.0f;
    if (angle <= 0.0f)
        return true;

    const Vec3 objectPosition = object->getAbsolutePosition();
    sourceProjectile->getAbsolutePosition(); // shipped, result discarded
    const Vec3 localPosition =
        transformPoint(sourceProjectile->getInvAbsoluteTransformation(), objectPosition);

    if (!object->hasCollisionLod(1) || localPosition.z < 0.0f)
        return false;

    const float radians = angle * 0.017453292f;
    const float coneRadius = tanf(radians) * fabsf(localPosition.z);
    LocalBoundingBox* const firstBox = const_cast<LocalBoundingBox*>(
        collisionMesh(object)->getBoundingBox(1));
    const Vec3& firstSize = firstBox->getSize();
    if (fabsf(localPosition.x) - firstSize.x * 0.5f > coneRadius)
        return false;

    const float secondConeRadius = fabsf(tanf(radians) * localPosition.z);
    LocalBoundingBox* const secondBox = const_cast<LocalBoundingBox*>(
        collisionMesh(object)->getBoundingBox(1));
    const Vec3& secondSize = secondBox->getSize();
    return fabsf(localPosition.y) - secondSize.y * 0.5f <= secondConeRadius;
}
} // namespace

// handleExplosion - ELF 0x80E4080, 1,240 B / PE 0x56CCB0, 878 B. The common presentation half:
// notify gameplay triggers, shake remote occupants of the directly affected vehicle, then drive
// the local-player camera/persistent-effect/tinnitus path. The dedicated image normally has no
// local human, but this remains the same source body in both shipped dedicated ports.
void GameLogicExplosion::handleExplosion(const Vec3& position, float radius, int /*material*/,
                                         float damage, IPlayer* /*player*/,
                                         IArmorGLComp* /*ignoredArmor*/, int /*team*/,
                                         IObject* weapon, IObject* /*vehicle*/, float force,
                                         IObject* sourceProjectile, IObject* affectedObject,
                                         float /*yMod*/, bool /*canTK*/, bool /*unused*/)
{
    if (triggerManager)
        // ELF80E4092..40B7 / PE56CCB9..cccb: type 8, source projectile, wildcard id.
        triggerManager->handleEvent(TriggerType_Object, sourceProjectile, -1);

    if (affectedObject)
    {
        IPlayerControlObject* const pco = findPco(getRootParent(affectedObject));
        if (pco)
        {
            const float forceRatio = force / *g_expShakeMaxForce;
            const float damageRatio = damage / *g_expShakeMaxDamage;
            const float shake = *g_expShakeMod
                              * (forceRatio < 1.0f ? forceRatio : 1.0f)
                              * (damageRatio < 1.0f ? damageRatio : 1.0f);

            std::map<int, IPlayerControlObject*>* const pcos = pco->getPcos();
            for (std::map<int, IPlayerControlObject*>::iterator it = pcos->begin();
                 it != pcos->end(); ++it)
            {
                IPlayer* const occupant = it->second->getOccupingPlayer();
                if (occupant && occupant->getIsRemote())
                    occupant->updateCameraShake(shake);
            }
        }
    }

    IPlayer* const localPlayer = playerManager->getLocalHumanPlayer();
    if (!localPlayer)
        return;

    IObject* const controlledObject = localPlayer->getVehicle();
    if (!controlledObject)
        return;

    if (localPlayer->getIsCommander() && localPlayer->getOverheadCameraEnabled())
    {
        explosionEffectManager->handleExplosionEffect(localPlayer, position, sourceProjectile);
        return;
    }

    const bool isSoldier =
        controlledObject->getTemplate()->getClassID() == CID_SoldierTemplate;
    bool insideMineBox = false;
    if (!isSoldier)
    {
        const Vec3 localPosition =
            transformPoint(controlledObject->getInvAbsoluteTransformation(), position);
        LocalBoundingBox* const box = controlledObject->getMineBoundingBox();
        insideMineBox = localPosition.x >= box->m_min.x
                     && localPosition.y >= box->m_min.y
                     && localPosition.z >= box->m_min.z
                     && localPosition.x <= box->m_max.x
                     && localPosition.y <= box->m_max.y
                     && localPosition.z <= box->m_max.z;
    }

    bool ownObject = getRootParent(affectedObject) == controlledObject;
    if (insideMineBox)
        ownObject = true;

    const Vec3& controlledPosition = controlledObject->getAbsolutePosition();
    Vec3 delta;
    delta.x = controlledPosition.x - position.x;
    delta.y = controlledPosition.y - position.y;
    delta.z = controlledPosition.z - position.z;
    const float distance = length(delta);

    float shake = *g_expShakeMod;
    if (!isSoldier && !ownObject)
        shake *= *g_expShakeInsideVehicleMod;

    const float forceRatio = force / *g_expShakeMaxForce;
    const float damageRatio = damage / *g_expShakeMaxDamage;
    shake *= forceRatio < 1.0f ? forceRatio : 1.0f;
    shake *= damageRatio < 1.0f ? damageRatio : 1.0f;
    if (!weapon)
        shake *= *g_ownVehicleExpMod;

    if (!ownObject)
    {
        const float distanceRatio = distance / (radius * 3.0f);
        shake *= distanceRatio >= 1.0f ? 0.0f : 1.0f - distanceRatio;
    }

    if (shake > 0.0f)
        localPlayer->getCamera()->handleExplosion(shake);

    explosionEffectManager->handleExplosionEffect(localPlayer, position, sourceProjectile);

    if (controlledObject->getTemplate()->getClassID() == CID_SoldierTemplate
        && distance < radius * *g_soundTinnitusRange)
    {
        g_bfSound->startTinnitus();
        eventManager->postEvent(ECGameLogic, 13, g_bfSound->getTinnitusSetup(), 0.0f);
    }
}

// clientHandleExplosion - ELF 0x80E3D20 / dedicated PE 0x56D420, complete paired reads.
// The line-query second vector really is the absolute object position, not the usual
// position-minus-explosion delta. Preserve this shipped quirk and the second position read.
void GameLogicExplosion::clientHandleExplosion(
        const Vec3& position, float radius, int material, float damage, IPlayer* player,
        IArmorGLComp* ignoredArmor, int team, IObject* weapon, IObject* vehicle,
        float force, IObject* sourceProjectile, IObject* otherObject, float yMod,
        bool flag1, bool flag2)
{
    handleExplosion(position, radius, material, damage, player, ignoredArmor, team,
                    weapon, vehicle, force, sourceProjectile, otherObject, yMod, flag1, flag2);
    static std::vector<IObject*> objects;
    ObjectFlagPredicator predicator(0x01000000u);
    objectManager->getObjectsFromGrid(position, radius, objects, &predicator);
    const float inverseRadius = 1.0f / radius;
    for (std::vector<IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        IObject* const object = *it;
        if (object && object_detail::LayoutAccess::objectTemplate(object)
            && object_detail::LayoutAccess::objectTemplate(object)->getClassID()
                == CID_DestroyableWindowsBundleTemplate)
            static_cast<DestroyableWindowsBundle*>(object)->updateStates(0);

        // The first branch guards object/template; this second branch does not.
        if (object_detail::LayoutAccess::objectTemplate(object)->getClassID() != CID_SoldierTemplate
            || !isObjectWithinCone(object, sourceProjectile)
            || !object_detail::LayoutAccess::armorComponent(object))
            continue;

        FlagIgnoreWaterPredicator ignoreWater(0x100u);
        Vec3 hitPoint = {0.0f, 0.0f, 0.0f};
        Vec3 hitNormal = {0.0f, 0.0f, 0.0f};
        IObject* hitObject = 0;
        int hitPart;
        if (!objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                position, object->getAbsolutePosition(), &ignoreWater,
                true, false, true, true, 0)
            || getRootParent(hitObject) != object)
        {
            hitPoint = Vec3{9999.0f, 9999.0f, 9999.0f};
        }
        handleExplosionForceOnObject(object, position, object->getAbsolutePosition(),
                hitPoint, inverseRadius, radius, ignoredArmor, force, sourceProjectile,
                yMod, 1.0f, false);
    }
}

// getExplosionRadiusMod - ELF 0x80E56D0, 79 B / PE 0x56C5F0, 77 B. Full strength inside the
// hotspot fraction, then a linear falloff to zero at radius, clamped to [0,1].
float GameLogicExplosion::getExplosionRadiusMod(float radius, float distance)
{
    const float innerRadius = radius * *g_explosionHotspot;
    float result = 1.0f;
    if (innerRadius <= distance)
        result = 1.0f - (distance - innerRadius) / (radius - innerRadius);
    if (result < 0.0f)
        result = 0.0f;
    if (result > 1.0f)
        result = 1.0f;
    return result;
}

// Calculates explosion damage and queues the deferred hit, with optional AI
// notification. Requires a source projectile when damage is queued.
bool GameLogicExplosion::serverHandleExplosionOnObject(
    IObject* object, const Vec3& explosionPosition, const Vec3& objectPosition,
    const Vec3& nearestPosition, float /*inverseRadius*/, float radius, int material,
    float damage, IPlayer* player, IArmorGLComp* ignoredArmor, int team, IObject* weapon,
    IObject* vehicle, IObject* sourceProjectile, float yMod, float& visibility)
{
    IArmorGLComp* const armor = findArmor(object);
    if (!armor || armor == ignoredArmor)
        return false;

    const float damageMod = materials()->getDamageMod(material, armor->getDefaultMaterial());

    Vec3 objectDelta;
    objectDelta.x = objectPosition.x - explosionPosition.x;
    objectDelta.y = (objectPosition.y - explosionPosition.y) * yMod;
    objectDelta.z = objectPosition.z - explosionPosition.z;
    Vec3 nearestDelta;
    nearestDelta.x = nearestPosition.x - explosionPosition.x;
    nearestDelta.y = (nearestPosition.y - explosionPosition.y) * yMod;
    nearestDelta.z = nearestPosition.z - explosionPosition.z;

    const float objectDistance = length(objectDelta);
    const float nearestDistance = length(nearestDelta);
    const float distance = nearestDistance <= objectDistance ? nearestDistance : objectDistance;
    if (radius <= distance)
        return false;

    float calculatedDamage =
        getExplosionRadiusMod(radius, distance) * damage * damageMod * visibility;
    if (calculatedDamage <= 0.1f)
        return false;

    calculatedDamage = GameLogicDamage::calcDamage(object, calculatedDamage, team, true, false);
    if (calculatedDamage > 0.0f)
    {
        const int buffer = m_currentExplosion;
        m_explosionObject[buffer].push_back(object->getWeakPtr());
        m_explosionSourceObject[buffer].push_back(sourceProjectile->getWeakPtr());
        m_explosionDamage[buffer].push_back(calculatedDamage);
        m_explosionTeam[buffer].push_back(static_cast<unsigned int>(team));
        m_explosionPos[buffer].push_back(explosionPosition);
        m_explosionPlayer[buffer].push_back(player ? player->getWeakPtr() : WeakPtr<IPlayer>());
        m_explosionWeapon[buffer].push_back(weapon ? weapon->getWeakPtr() : WeakPtr<IObject>());
        m_explosionVehicle[buffer].push_back(vehicle ? vehicle->getWeakPtr() : WeakPtr<IObject>());
    }

    IObject* const root = getRootParent(object);
    if (!root)
        return false;
    IPlayerControlObject* const pco = findPco(root);

    // ELF 0x80E375E: the AI hears the queued damage (+0x20), the explosion position by value.
    ai::IBFCollisionHandler* const collision =
        static_cast<ai::IBFCollisionHandler*>(ai::CommonSingletonServer::m_iBFCollisionHandler);
    if (player && collision && weapon)
        collision->handleExplosionDamage(object, calculatedDamage, player->getId(),
                                         weapon->getObjectId(), explosionPosition);

    if (!pco)
        return false;

    ISoldier* const soldier = static_cast<ISoldier*>(pco->queryInterface(IID_ISoldier));
    if (soldier)
    {
        IPlayer* const occupant = soldier->getOccupingPlayer();
        if (occupant && (!occupant->getIsAlive() || occupant->getIsManDown()))
            return false;
    }

    if (pco->getTeam() == team)
        return false;
    return pco->getTeam() > 0;
}

// handleExplosionForceOnObject - ELF 0x80E4560, 2,501 B / PE 0x56C7D0, 1,242 B. Selects the
// target basis axis most aligned with the blast, combines it with the radial direction, applies
// the armor/default force caps, parks a soldier ragdoll impulse, and optionally wakes/pushes the
// physics node. `inverseRadius` and `sourceProjectile` are shipped-unused.
bool GameLogicExplosion::handleExplosionForceOnObject(
    IObject* object, const Vec3& explosionPosition, const Vec3& objectPosition,
    const Vec3& nearestPosition, float /*inverseRadius*/, float radius,
    IArmorGLComp* ignoredArmor, float force, IObject* /*sourceProjectile*/, float yMod,
    float visibility, bool applyPhysics)
{
    if (!object || object->getParent())
        return false;

    Vec3 radial;
    radial.x = objectPosition.x - explosionPosition.x;
    radial.y = (objectPosition.y - explosionPosition.y) * yMod;
    radial.z = objectPosition.z - explosionPosition.z;
    Vec3 nearestDelta;
    nearestDelta.x = nearestPosition.x - explosionPosition.x;
    nearestDelta.y = (nearestPosition.y - explosionPosition.y) * yMod;
    nearestDelta.z = nearestPosition.z - explosionPosition.z;

    const float objectDistance = length(radial);
    const float nearestDistance = length(nearestDelta);
    const float distance = nearestDistance <= objectDistance ? nearestDistance : objectDistance;
    if (radius <= distance)
        return false;

    IPhysicsNode* const node = physicsNode(object);
    if (!node)
        return false;

    float forceMod;
    float maxForce;
    IArmorGLComp* const armor = findArmor(object);
    if (armor)
    {
        if (armor == ignoredArmor)
            return false;
        forceMod = armor->getExplosionForceMod();
        maxForce = armor->getExplosionForceMax();
    }
    else
    {
        const float mass = node->getMass();
        forceMod = *Vars::getFloat("p_forceMod", 10000.0f) / (mass >= 100.0f ? mass : 100.0f);
        maxForce = *Vars::getFloat("p_forceMax", 500.0f);
    }

    float strength = getExplosionRadiusMod(radius, distance) * forceMod * force * visibility;
    if (node->getUnderWater() > 0.0f
        && object->getTemplate()->getClassID() == CID_SoldierTemplate)
        strength *= 0.1f;
    strength = strength <= maxForce ? strength : maxForce;

    const Mat4& transformY = object->getAbsoluteTransformation();
    const float dotY = transformY.m[4] * radial.x + transformY.m[5] * radial.y
                     + transformY.m[6] * radial.z;
    const Mat4& transformX = object->getAbsoluteTransformation();
    const float dotX = transformX.m[0] * radial.x + transformX.m[1] * radial.y
                     + transformX.m[2] * radial.z;
    const Mat4& transformZ = object->getAbsoluteTransformation();
    const float dotZ = transformZ.m[8] * radial.x + transformZ.m[9] * radial.y
                     + transformZ.m[10] * radial.z;

    int selectedAxis;
    float selectedDot;
    if (fabsf(dotY) <= fabsf(dotX))
    {
        selectedAxis = fabsf(dotZ) <= fabsf(dotX) ? 0 : 2;
        selectedDot = selectedAxis == 0 ? dotX : dotZ;
    }
    else
    {
        selectedAxis = fabsf(dotY) <= fabsf(dotZ) ? 2 : 1;
        selectedDot = selectedAxis == 1 ? dotY : dotZ;
    }

    const Mat4& selectedTransform = object->getAbsoluteTransformation();
    Vec3 axis;
    const int axisOffset = selectedAxis * 4;
    axis.x = selectedTransform.m[axisOffset];
    axis.y = selectedTransform.m[axisOffset + 1];
    axis.z = selectedTransform.m[axisOffset + 2];
    if (selectedDot < 0.0f)
    {
        axis.x = -axis.x;
        axis.y = -axis.y;
        axis.z = -axis.z;
    }
    normaliseIfNeeded(axis);
    normaliseIfNeeded(radial);

    const bool isSoldier = object->getTemplate()->getClassID() == CID_SoldierTemplate;
    if (isSoldier)
    {
        radial.y += *g_explosionSoldierForceYMod;
        strength *= static_cast<float>(std::rand() % 100) / 200.0f + 0.8f;
    }

    const float existingForce = length(node->getPositionalAcceleration());
    if (strength + existingForce > maxForce)
        strength = maxForce - existingForce;

    Vec3 direction;
    direction.x = axis.x + radial.x;
    direction.y = axis.y + radial.y;
    direction.z = axis.z + radial.z;
    normaliseIfNeeded(direction);

    Vec3 acceleration;
    acceleration.x = direction.x * strength;
    acceleration.y = direction.y * strength;
    acceleration.z = direction.z * strength;
    if (fabsf(acceleration.x) < 0.001f) acceleration.x = 0.0f;
    if (fabsf(acceleration.y) < 0.001f) acceleration.y = 0.0f;
    if (fabsf(acceleration.z) < 0.001f) acceleration.z = 0.0f;
    if (!(lengthSquared(acceleration) >= 0.01f))
        return false;

    if (isSoldier)
        static_cast<Soldier*>(object)->applyExplosionForce(acceleration);

    if (applyPhysics)
    {
        node->setIsAwake();
        const float divisor = distance < 1.0f ? 1.0f : distance;
        acceleration.x /= divisor;
        acceleration.y /= divisor;
        acceleration.z /= divisor;
        Vec3 relativePosition;
        relativePosition.x = (explosionPosition.x - objectPosition.x) / divisor;
        relativePosition.y = (explosionPosition.y - objectPosition.y) / divisor;
        relativePosition.z = (explosionPosition.z - objectPosition.z) / divisor;
        node->addAccelerationAtRelativePosition(relativePosition, acceleration);
    }
    return true;
}

// checkExplosionOnSoldier - ELF 0x80E4F30, 1,943 B / PE 0x56D020, 1,015 B. Tests head/torso/feet
// from the centre and four offset rays. An obstruction already doomed by this explosion grants
// half visibility, allowing blast propagation through an object queued to be destroyed.
float GameLogicExplosion::checkExplosionOnSoldier(const Vec3& explosionPosition,
                                                  IObject* soldierObject, float rayOffset,
                                                  IObject* sourceProjectile)
{
    static const float hitMods[3] = { 0.5f, 0.35f, 0.15f };

    Vec3 direction;
    const Vec3& soldierPosition = soldierObject->getAbsolutePosition();
    direction.x = soldierPosition.x - explosionPosition.x;
    direction.y = soldierPosition.y - explosionPosition.y;
    direction.z = soldierPosition.z - explosionPosition.z;
    normaliseIfNeeded(direction);

    Vec3 side;
    side.x = -direction.z * rayOffset;
    side.y = 0.0f;
    side.z = direction.x * rayOffset;
    Vec3 second;
    // The second offset is cross(direction, cross(direction, up)) * rayOffset. Use the
    // unscaled first cross here: the ELF keeps those three temporaries separately from `side`.
    second.x = direction.y * direction.x * rayOffset;
    second.y = (-direction.z * direction.z - direction.x * direction.x) * rayOffset;
    second.z = direction.y * direction.z * rayOffset;

    bool centreBlocked = false;
    bool secondKnown = false;
    bool secondBlocked = false;
    bool negativeSideKnown = false;
    bool negativeSideBlocked = false;
    bool positiveSideKnown = false;
    bool positiveSideBlocked = false;
    bool negativeSecondKnown = false;
    bool negativeSecondBlocked = false;
    float visible = 0.0f;

    for (int pointIndex = 0; pointIndex <= 2; ++pointIndex)
    {
        const Vec3 point = static_cast<Soldier*>(soldierObject)->getSkeletonRayTestPoint(
            static_cast<Soldier::SkeletonRayTestPoint>(pointIndex));
        IObject* hitObject = 0;
        bool blocked = true;

        if (!centreBlocked)
        {
            blocked = checkIfRayHitAnyObjectOrGround(explosionPosition, point,
                                                      sourceProjectile, hitObject,
                                                      true, false, true, 0);
            if (!blocked)
                goto score_visibility;
            centreBlocked = true;
        }

        {
            Vec3 start;
            start.x = explosionPosition.x + second.x;
            start.y = explosionPosition.y + second.y;
            start.z = explosionPosition.z + second.z;
            if (secondBlocked
                || (!secondKnown
                    && !checkIfRayHitAnyObjectOrGround(explosionPosition, start,
                                                       sourceProjectile, hitObject,
                                                       true, false, true, 0)))
            {
                blocked = checkIfRayHitAnyObjectOrGround(start, point, sourceProjectile,
                                                         hitObject, true, false, true, 0);
                secondBlocked = true;
            }
            secondKnown = true;
        }

        if (blocked)
        {
            Vec3 start;
            start.x = explosionPosition.x - side.x;
            start.y = explosionPosition.y - side.y;
            start.z = explosionPosition.z - side.z;
            if (negativeSideBlocked
                || (!negativeSideKnown
                    && !checkIfRayHitAnyObjectOrGround(explosionPosition, start,
                                                       sourceProjectile, hitObject,
                                                       true, false, true, 0)))
            {
                blocked = checkIfRayHitAnyObjectOrGround(start, point, sourceProjectile,
                                                         hitObject, true, false, true, 0);
                negativeSideBlocked = true;
            }
            negativeSideKnown = true;

            if (blocked)
            {
                start.x = explosionPosition.x + side.x;
                start.y = explosionPosition.y + side.y;
                start.z = explosionPosition.z + side.z;
                if (positiveSideBlocked
                    || (!positiveSideKnown
                        && !checkIfRayHitAnyObjectOrGround(explosionPosition, start,
                                                           sourceProjectile, hitObject,
                                                           true, false, true, 0)))
                {
                    blocked = checkIfRayHitAnyObjectOrGround(start, point, sourceProjectile,
                                                             hitObject, true, false, true, 0);
                    positiveSideBlocked = true;
                }
                positiveSideKnown = true;
            }
        }

score_visibility:
        if (!secondBlocked && blocked)
        {
            Vec3 start;
            start.x = explosionPosition.x - second.x;
            start.y = explosionPosition.y - second.y;
            start.z = explosionPosition.z - second.z;
            if (negativeSecondBlocked
                || (!negativeSecondKnown
                    && !checkIfRayHitAnyObjectOrGround(explosionPosition, start,
                                                       sourceProjectile, hitObject,
                                                       true, false, true, 0)))
            {
                blocked = checkIfRayHitAnyObjectOrGround(start, point, sourceProjectile,
                                                         hitObject, true, false, true, 0);
                negativeSecondBlocked = true;
            }
            negativeSecondKnown = true;
        }

        if (!blocked)
        {
            visible += hitMods[pointIndex];
            continue;
        }

        bool obstructionWillBeDestroyed = false;
        IArmorGLComp* const armor = findArmor(hitObject);
        if (armor)
        {
            const int buffer = m_currentExplosion;
            for (unsigned int i = 0; i < m_explosionObject[buffer].size(); ++i)
            {
                if (m_explosionObject[buffer][i].get() == hitObject
                    && m_explosionDamage[buffer][i] >= armor->getHitPoints())
                {
                    obstructionWillBeDestroyed = true;
                }
            }
        }
        if (obstructionWillBeDestroyed)
            visible += hitMods[pointIndex] * 0.5f;
    }
    return visible;
}

// serverHandleExplosion - ELF 0x80E2B00, 2,633 B / PE 0x56DDB0, 1,667 B. The authoritative
// explosion traversal: lag-aware root query, non-soldier line/force pass, occupant collection,
// soldier multi-ray visibility pass, and the type-1/type-3 Python projectile event.
bool GameLogicExplosion::serverHandleExplosion(
    const Vec3& position, float radius, int material, float damage, IPlayer* player,
    IArmorGLComp* ignoredArmor, int team, IObject* weapon, IObject* vehicle, float force,
    IObject* sourceProjectile, IObject* affectedObject, float yMod, bool canTK,
    bool includeSource)
{
    if (material != -1)
    {
        handleExplosion(position, radius, material, damage, player, ignoredArmor, team,
                        weapon, vehicle, force, sourceProjectile, affectedObject,
                        yMod, canTK, includeSource);
    }

    std::vector<IObject*> objects;
    std::vector<IObject*> soldiers;
    ObjectFlagPredicator rootsOnly(0x1000000u);
    GenericProjectile* queryProjectile = 0;
    if (*g_explosionLatencyComp == 1 && LatencyCompensation::m_isEnabled)
        queryProjectile = projectileFromObject(sourceProjectile);

    if (queryProjectile
        && LatencyCompensation::getLatency(queryProjectile->getPlayer()) > 0.0f)
    {
        objectManager->getObjectsFromGridWithLatency(position, radius, objects, &rootsOnly);
    }
    else
    {
        objectManager->getObjectsFromGrid(position, radius, objects, &rootsOnly);
    }

    bool hitSomething = false;
    for (std::vector<IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        IObject* const object = *it;
        if (!includeSource && object == sourceProjectile)
            continue;
        if (object->getTemplate()->getClassID() == CID_SoldierTemplate)
        {
            soldiers.push_back(object);
            continue;
        }

        if (!isObjectWithinCone(object, sourceProjectile))
            continue;

        FlagIgnoreWaterPredicator ignoreWater(0x100u);
        Vec3 objectPosition = object->getAbsolutePosition();
        if (*g_explosionLatencyComp == 1 && LatencyCompensation::m_isEnabled)
        {
            IPhysicsNode* const node = physicsNode(object);
            if (node && node->hasLatencyCompensation())
            {
                GenericProjectile* const projectile = projectileFromObject(sourceProjectile);
                const float latency = projectile
                    ? LatencyCompensation::getLatency(projectile->getPlayer()) : 0.0f;
                const Mat4& transform = node->getLatencyCompensatedTransformation(latency, 0);
                objectPosition.x = transform.m[12];
                objectPosition.y = transform.m[13];
                objectPosition.z = transform.m[14];
            }
        }

        Vec3 direction;
        direction.x = objectPosition.x - position.x;
        direction.y = objectPosition.y - position.y;
        direction.z = objectPosition.z - position.z;
        IObject* hitObject = 0;
        Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
        Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
        int hitPart = 0;
        const bool hit = objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                                                       position, direction, &ignoreWater,
                                                       true, false, true, true, 0);
        Vec3 nearestPosition = objectPosition;
        if (hit && getRootParent(hitObject) == object)
            nearestPosition = hitPoint;

        const float visibility = 1.0f;
        if (material != -1)
        {
            float damageVisibility = visibility;
            if (serverHandleExplosionOnObject(object, position, objectPosition, nearestPosition,
                                              1.0f / radius, radius, material, damage, player,
                                              ignoredArmor, team, weapon, vehicle, sourceProjectile,
                                              yMod, damageVisibility))
            {
                hitSomething = true;
            }
        }

        float forceVisibility = visibility;
        handleExplosionForceOnObject(object, position, objectPosition, nearestPosition,
                                     1.0f / radius, radius, ignoredArmor, force,
                                     sourceProjectile, yMod, forceVisibility, true);

        if (object->getTemplate()->getClassID() == CID_DestroyableWindowsBundleTemplate)
            static_cast<DestroyableWindowsBundle*>(object)->updateStates(0);

        IPlayerControlObject* const pco = findPco(object);
        if (pco)
        {
            std::map<int, IPlayerControlObject*>* const seats = pco->getPcos();
            for (std::map<int, IPlayerControlObject*>::iterator seat = seats->begin();
                 seat != seats->end(); ++seat)
            {
                IObject* const occupant = seat->second->getOccupingObject();
                if (occupant && !(object_detail::LayoutAccess::flags(occupant) & 1u)
                    && occupant->getTemplate()->getClassID() == CID_SoldierTemplate)
                {
                    soldiers.push_back(occupant);
                }
            }
        }
    }

    GenericProjectile* const projectile = projectileFromObject(sourceProjectile);
    for (std::vector<IObject*>::iterator it = soldiers.begin(); it != soldiers.end(); ++it)
    {
        IObject* const soldierObject = *it;
        if (!isObjectWithinCone(soldierObject, sourceProjectile))
            continue;

        const float requestedOffset = radius * *g_explosionHotspotRay;
        const float rayOffset = requestedOffset > *g_explosionHotspotRayMax
                              ? *g_explosionHotspotRayMax : requestedOffset;
        float visibility = checkExplosionOnSoldier(position, soldierObject, rayOffset,
                                                   sourceProjectile);
        const Vec3 soldierPosition = soldierObject->getAbsolutePosition();
        const Vec3 nearestPosition = soldierPosition;
        if (visibility > 0.0f && material != -1)
        {
            if (serverHandleExplosionOnObject(soldierObject, position, soldierPosition,
                                              nearestPosition, 1.0f / radius, radius,
                                              material, damage, player, ignoredArmor, team,
                                              weapon, vehicle, sourceProjectile, yMod, visibility))
            {
                hitSomething = true;
            }
            handleExplosionForceOnObject(soldierObject, position, soldierPosition,
                                         nearestPosition, 1.0f / radius, radius, ignoredArmor,
                                         force, sourceProjectile, yMod, visibility, true);

            if (projectile && projectile->getExplosionType() == 1
                && static_cast<Soldier*>(soldierObject)->getOccupingPlayer()
                       != projectile->getPlayer())
            {
                hitSomething = true;
            }
        }
    }

    if (projectile)
    {
        const int explosionType = projectile->getExplosionType();
        if (explosionType == 1 || explosionType == 3)
        {
            IPlayer* const owner = projectile->getPlayer();
            if (owner)
            {
                python::PyRef args = python::buildArgs(owner);
                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(10), args.get());
            }
        }
    }

    return hitSomething && !includeSource;
}

// Switches buffers before applying queued explosion damage, so new explosions go
// to the other buffer. Skips expired targets and clears every drained vector.
void GameLogicExplosion::serverUpdateExplosionDamage()
{
    // Flip, and drain what the other side filled.
    const int buffer = m_currentExplosion ? 1 : 0;
    m_currentExplosion = m_currentExplosion ? 0 : 1;

    const std::size_t count = m_explosionObject[buffer].size();
    for (std::size_t i = 0; i < count; ++i)
    {
        // The projectile's already-hit list, if the source object is one.
        std::list<WeakPtr<IPlayer> >* alreadyHit = 0;
        IObject* source = m_explosionSourceObject[buffer][i].get();
        if (source)
        {
            GenericProjectile* const projectile = projectileFromObject(source);      // slot +0x0C
            if (projectile)
                alreadyHit = projectile->getAssistPlayers();                         // +0x5C
        }

        IObject* target = m_explosionObject[buffer][i].get();
        if (!target)
            continue;                                                                // the dead arm

        const Vec3& position = m_explosionPos[buffer][i];
        const Vec3& here = target->getAbsolutePosition();                            // slot 27
        Vec3 delta;                                                                  // : fsubr
        delta.x = here.x - position.x;
        delta.y = here.y - position.y;
        delta.z = here.z - position.z;

        gameLogic->giveDamage(target,                                                // IGameLogic +0xE4
                              m_explosionDamage[buffer][i],
                              m_explosionPlayer[buffer][i].get(),
                              alreadyHit,
                              source,
                              static_cast<int>(m_explosionTeam[buffer][i]),
                              m_explosionWeapon[buffer][i].get(),
                              m_explosionVehicle[buffer][i].get(),
                              position, delta,
                              -1, true, true, false);
    }

    // The eight clears - the erase / _M_fill_insert pairs in the decompile are GCC's inlined
    // vector::clear(), one per queue.
    m_explosionObject[buffer].clear();
    m_explosionSourceObject[buffer].clear();
    m_explosionDamage[buffer].clear();
    m_explosionPlayer[buffer].clear();
    m_explosionTeam[buffer].clear();
    m_explosionWeapon[buffer].clear();
    m_explosionVehicle[buffer].clear();
    m_explosionPos[buffer].clear();
}

} // namespace world
} // namespace hfe
} // namespace dice
