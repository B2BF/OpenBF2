// Large-bore fire behaviour, recovered from the symbolized Linux 1.50 libaidll.so.

// static init: ELF 0x212EA0 (__tcf_0 0x212F00, _GLOBAL__I 0x212F30): <iostream>'s Init object only.
#include <iostream>

#include "BBFireLargeBore.h"

#include "CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/SensedObject.h"
#include "../AIBotBehaviourPattern/BBPFire.h"
#include "../AIBotBehaviourPattern/BBPFire2d.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/VehicleOccupancy.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIMain.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x20F3C0 (C2) / 0x20F7E0 (C1); vtable GOT -0x1514. As BBFire3d's, plus the bool at +0x3C,
// and both per-bot arrays go through the inlined checkPointer: +0x08 targets, then +0x24; every bot
// starts with urgency 0.0f and both slots -1; the pattern gets this behaviour as its urgency
// generator.
BBFireLargeBore::BBFireLargeBore(unsigned char index, unsigned char queue, BBPFire* pattern,
                                 UrgeCurve* curve, float* modifiers, bool flag)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
    , m_double28(0.0)
{
    m_flag3C = flag;
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int bots = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[bots]);
    m_unknown24 = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_unknown24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    pattern->setUrgencyGenerator(this);
}

// ELF 0x20FB80 (D2) / 0x20FC40 (D1) / 0x20FD00 (D0): delete[] +0x08, then +0x24 (each when set),
// then the map's destructor (nodes only) and ~BotBehaviour.
BBFireLargeBore::~BBFireLargeBore()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unknown24)
        delete[] m_unknown24;
}

// ELF 0x20FDC0
float BBFireLargeBore::calculateUrgency(Bot* bot, bool recalculate, float modifier)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    return calculateUrgency(bot, recalculate, modifier, equipment, equipment->getMobile());
}

// ELF 0x212BE0
void BBFireLargeBore::initializeGenerator(BotMain* bot, int weapon, bool a, bool b, bool spatial)
{
    bot->setFiringTarget(m_targets[bot->getBotId()]);
    BBPFire* const pattern = static_cast<BBPFire*>(m_pattern);
    if (pattern->getImplementationType() == 2)
        static_cast<BBPFire2d*>(m_pattern)->init(bot, m_targets[bot->getBotId()], weapon, a, b, spatial);
    else
        pattern->init(bot, m_targets[bot->getBotId()], weapon, a, b);
}

// ELF 0x20FE50
float BBFireLargeBore::calculateUrgency(Bot* bot, bool recalculate, float modifier,
                                      Information* equipment, IPIMobile* mobile)
{
    if (!recalculate)
    {
        if (!m_pattern->isInitiated(bot))
        {
            m_targetChanged[bot->getBotId()] = false;
            return m_urgency[bot->getBotId()] = 0.0f;
        }
        if (!informationManager->getInformation(m_targets[bot->getBotId()]))
        {
            m_targetChanged[bot->getBotId()] = true;
            m_pattern->unInit(bot);
            return m_urgency[bot->getBotId()] = 0.0f;
        }
        m_targetChanged[bot->getBotId()] = false;
        bot->addToFireInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    bool bestSpatial = false;
    bool secondarySpatial = false;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const float* const armourValues = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getArmourClassValues();
    const int equipmentType = equipment->getUnit()->getEquipmentType();
    if (now - m_double28 > 60.0)
    {
        std::map<unsigned int, BBPFeedback*>::iterator it = m_feedback.begin();
        while (it != m_feedback.end())
        {
            if (informationManager->getInformation(it->first))
                ++it;
            else
            {
                delete it->second;
                m_feedback.erase(it++);
            }
        }
        m_double28 = now;
    }

    IPIUnit* const unit = equipment->getUnit();
    IPIControlInfo* const controls = equipment->getControlInfo();
    IPIPhysical* const physical = equipment->getPhysical();
    IPIArmament* armament;
    if (m_flag3C)
    {
        int secondary;
        Information* const gunner = informationManager->getInformation(
            equipment->getFirstSecondaryObject(secondary, bot->getSide()));
        armament = gunner->getArmament(); // shipped: no secondary-object null guard
    }
    else
        armament = equipment->getArmament();
    const int strengthType = physical->getStrengthType();
    const Vec3 position = equipment->getPosition();
    float deadScore = 0.0f; // never written again in the image
    float bestScore = 0.0f;
    float secondaryScore = 0.0f;
    float currentScore = 0.0f;
    int bestWeapon = -1;
    int secondaryWeapon = -1;
    unsigned int secondaryTarget = static_cast<unsigned int>(-1);
    unsigned int countedOrders = 0;
    WeaponInfo* selectedWeaponInfo = 0;
    const int side = bot->getSide();
    const unsigned int currentTarget = m_targets[bot->getBotId()];
    const unsigned int currentWeapon = m_unknown24[bot->getBotId()];
    bool keepCurrent = false;
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    const unsigned int botId = bot->getBotId();
    const int orderedArea = IAIMain::m_instance->getSAI(side)->getBotOrderedPos(botId);
    AIStrategicArea* const area = static_cast<AIStrategicArea*>(
        AIStrategicObjectManager::getInstance()->getStrategicObject(orderedArea));
    const float areaFactor = area ? (area->isInside(position) ? 1.0f : 0.75f) : 1.0f;
    IAIObjectEnvironment* const environment = static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);

    // [INFERRED] local helper names for repeated inline expansions in this body.
    // ELF 0x212AA2 / 0x211830: attacking-target bonus, fading from 1.5 to 1 over 30 seconds.
    const auto attackFactorFor = [&](unsigned int handle) -> float
    {
        double attackTime;
        if (!static_cast<BotMain*>(bot)->isAttacking(handle, attackTime))
            return 1.0f;
        float ramp = static_cast<float>(now - attackTime) / 30.0f;
        if (0.0f > ramp)
            ramp = 0.0f;
        else if (ramp > 1.0f)
            ramp = 1.0f;
        return 1.5f - ramp * 0.5f;
    };

    // ELF 0x211CE0..0x211E25 / 0x212588..0x21289E, and 0x210CCD..0x210E1E /
    // 0x211318..0x211632: moving-target and anti-aircraft weights.
    const auto approachFactorFor = [&](Information* info, IPIMobile* targetMobile,
                                      const Vec3& direction, float distance, float maxRange,
                                      bool sensed) -> float
    {
        Vec3 targetSpeed;
        targetSpeed.x = targetSpeed.y = targetSpeed.z = 0.0f;
        if (targetMobile)
            targetSpeed = targetMobile->getSpeed();
        const float speed = std::sqrt(targetSpeed.x * targetSpeed.x + targetSpeed.y * targetSpeed.y
                                     + targetSpeed.z * targetSpeed.z);
        if (!(equipment->getObjectTypeFlags() & 0x10) && (info->getObjectTypeFlags() & 0x10))
        {
            if (armament->isAntiAircraft() == 0)
            {
                if (distance > maxRange * 0.5f || speed > 15.0f)
                    return 0.0f;
            }
            else if (maxRange * 0.9f > distance)
                return 1.0f;
        }
        if (!targetMobile)
            return 1.0f;
        if (!(distance > maxRange))
        {
            const Vec3& velocity = targetMobile->getSpeed();
            return 1.0f / (std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y
                                    + velocity.z * velocity.z) * 0.5f + 1.0f);
        }
        float maxSpeed = 0.0f;
        Vec3 ownSpeed;
        ownSpeed.x = ownSpeed.y = ownSpeed.z = 0.0f;
        if (mobile)
        {
            maxSpeed = mobile->getMaxSpeed();
            ownSpeed = mobile->getSpeed();
        }
        if (!(targetMobile->getMaxSpeed() - maxSpeed > 5.0f))
            return 0.25f;
        Vec3 lateral;
        lateral.x = direction.z - direction.y * 0.0f;
        lateral.y = direction.x * 0.0f - direction.z * 0.0f;
        lateral.z = direction.y * 0.0f - direction.x;
        const Vec3& velocity = targetMobile->getSpeed();
        float radial = (direction.x * velocity.x + direction.y * velocity.y + direction.z * velocity.z)
                     - (direction.x * ownSpeed.x + direction.y * ownSpeed.y + direction.z * ownSpeed.z);
        if (0.0f > radial)
            radial = 0.0f;
        float transverse = (lateral.x * velocity.x + lateral.y * velocity.y + lateral.z * velocity.z)
                         - (lateral.x * ownSpeed.x + lateral.y * ownSpeed.y + lateral.z * ownSpeed.z);
        if (0.0f > transverse)
            transverse = 0.0f;
        const float magnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z);
        const float relative = sensed ? transverse * 2.0f + radial * 2.0f + 1.0f
                                      : radial * 2.0f + 1.0f + transverse * 2.0f;
        return 0.25f / (relative + magnitude * 0.5f);
    };

    // ELF 0x211F0F..0x212063 and 0x210F05..0x21106E: target plus secondary-seat strengths.
    const auto strengthsFor = [&](Information* info, unsigned int target, float targetAreaFactor,
                                  float& strengthSum) -> float
    {
        float score = 0.0f;
        strengthSum = 0.0f;
        unsigned int partHandle = target;
        int secondary = info->getFirstSecondaryObjectIterator();
        const int targetStrengthType = info->getPhysical()->getStrengthType();
        Information* part = info;
        do
        {
            IPIUnit* const partUnit = part->getUnit();
            strengthSum = strengthSum + partUnit->getBattleStrengths()[strengthType];
            bool ordered = false;
            if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders()
                && countedOrders < bot->getOrderList(m_index)->size())
            {
                const std::list<unsigned int>::iterator end = bot->getOrderList(m_index)->end();
                const std::list<unsigned int>::iterator begin = bot->getOrderList(m_index)->begin();
                const std::list<unsigned int>::iterator searchEnd = bot->getOrderList(m_index)->end();
                ordered = std::find(begin, searchEnd, partHandle) != end;
            }
            float orderFactor = 1.0f;
            if (ordered)
            {
                ++countedOrders;
                orderFactor = bot->getMemory()->m_float8;
            }
            score = (partUnit->getBattleStrengths()[strengthType] + part->getStrength()
                     + unit->getBattleStrengths()[targetStrengthType]) * armourValues[targetStrengthType]
                     * orderFactor * targetAreaFactor + score;
            partHandle = info->getSecondaryObject(secondary, side);
            part = informationManager->getInformation(partHandle);
        }
        while (part);
        return score;
    };

    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end(); ++it)
    {
        SensedObject* const object = it->second;
        Information* const info = informationManager->getInformation(it->first);
        if (!info || info->getSide() == side)
            continue;
        IPIPhysical* const targetPhysical = info->getPhysical();
        if (!targetPhysical || (info->getObjectTypeFlags() & 0x20000) || (info->getObjectTypeFlags() & 0x100000)
            || (info->getObjectTypeFlags() & 0x800000) || 0.0f >= targetPhysical->getHealth())
            continue;
        if ((object->m_flags & 0x20) && object->m_fireRating < 0)
            continue;
        if (20.0 > now - getBBPFeedback(object->m_handle)->m_time)
            continue;
        const float attackFactor = attackFactorFor(object->m_handle);
        IPIMobile* const targetMobile = info->getMobile();
        Vec3 targetPosition;
        targetPosition.x = targetPosition.y = targetPosition.z = 0.0f;
        if (targetMobile)
            targetMobile->getPositionEstimate(targetPosition);
        else
            targetPosition = info->getPosition();
        Vec3 direction;
        direction.x = targetPosition.x - position.x;
        direction.y = targetPosition.y - position.y;
        direction.z = targetPosition.z - position.z;
        float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        if (0.5f > distance)
            distance = 0.5f;
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = direction.y * inverse;
        direction.z = direction.z * inverse;

        int weaponIndex = -1;
        float maxRange = 0.0f;
        float bestStrength = 0.0f;
        float weaponScores[8]; // shipped: no bound on the weapon count
        const std::vector<unsigned int>& weapons = armament->getWeapons();
        unsigned int i = 0;
        for (; i < weapons.size(); ++i)
        {
            if (!WeaponInfoManager::m_instance->getInformation(weapons[i]))
            {
                weaponScores[i] = 0.0f;
                continue;
            }
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            int ammo = weapon->getAmmo();
            if (ammo == -1)
                ammo = 0x10000;
            if (ammo > 0)
            {
                float strength = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
                if (!(weapon->getMaxRange() >= distance))
                    strength = strength * 0.25f;
                weaponScores[i] = strength;
            }
            else
                weaponScores[i] = 0.0f;
            if (weapon->getMinRange() > distance)
                weaponScores[i] = 0.0f;
            if (weaponScores[i] > bestStrength)
            {
                weaponIndex = i;
                bestStrength = weaponScores[i];
                selectedWeaponInfo = weapon;
            }
            if (weapon->getMaxRange() > maxRange)
                maxRange = weapon->getMaxRange();
        }
        if (bestStrength == 0.0f)
            continue;
        for (; i < 8; ++i)
            weaponScores[i] = 0.0f;

        float ageFactor = 1.0f;
        if (object->m_flags & 8)
        {
            const float age = static_cast<float>(now - object->m_firstSeenTime);
            ageFactor = 1.0f / (age * 0.05f + 1.0f);
        }
        else if (!mobile)
        {
            Vec3 unusedCameraPosition;
            unusedCameraPosition.x = unusedCameraPosition.y = unusedCameraPosition.z = 0.0f;
            controls->getCameraBasePos(unusedCameraPosition);
            const Mat4& transform = info->getTransformation();
            const Vec3& local = object->m_seenPosition;
            Vec3 point;
            point.x = local.x * transform.m[0] + local.y * transform.m[4] + local.z * transform.m[8] + transform.m[12];
            point.y = local.x * transform.m[1] + local.y * transform.m[5] + local.z * transform.m[9] + transform.m[13];
            point.z = local.x * transform.m[2] + local.y * transform.m[6] + local.z * transform.m[10] + transform.m[14];
            const Vec3& weaponPosition = selectedWeaponInfo->getPosition();
            Vec3 aim;
            aim.x = point.x - weaponPosition.x;
            aim.y = point.y - weaponPosition.y;
            aim.z = point.z - weaponPosition.z;
            if (!controls->validateCameraDirectionYaw(aim))
                continue;
        }

        const float approachFactor = approachFactorFor(info, targetMobile, direction, distance, maxRange, true);
        if (distance > maxRange * 0.89f)
        {
            if (!mobile)
                continue;
            const Vec3& target = info->getPosition();
            const int vehicle = mobile->getVehicleNumber();
            if (pathfinding->isValidPosition(vehicle, equipmentType, target, true))
                continue;
        }
        else if (!mobile)
        {
            Vec3 from;
            from.x = from.y = from.z = 0.0f;
            Vec3 to;
            to.x = to.y = to.z = 0.0f;
            const Mat4* const camera = static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
            from.x = camera->m[12];
            from.y = camera->m[13];
            from.z = camera->m[14];
            std::vector<IAIObject*> ignored;
            ignored.push_back(equipment->getAIObject());
            ignored.push_back(info->getAIObject());
            const Mat4& transform = info->getTransformation();
            const Vec3& local = object->m_seenPosition;
            to.x = local.x * transform.m[0] + local.y * transform.m[4] + local.z * transform.m[8] + transform.m[12];
            to.y = local.x * transform.m[1] + local.y * transform.m[5] + local.z * transform.m[9] + transform.m[13];
            to.z = local.x * transform.m[2] + local.y * transform.m[6] + local.z * transform.m[10] + transform.m[14];
            if (targetMobile)
            {
                Vec3 estimate;
                estimate.x = estimate.y = estimate.z = 0.0f;
                targetMobile->getPositionEstimate(now, estimate);
                const Vec3& currentPosition = info->getPosition();
                to.x = (estimate.x - currentPosition.x) + to.x;
                to.y = (estimate.y - currentPosition.y) + to.y;
                to.z = (estimate.z - currentPosition.z) + to.z;
            }
            bool hit;
            if (environment->collideLineWithWorld(from, to, ignored, false, 0.7f, false, hit))
                continue;
        }
        float targetAreaFactor = areaFactor;
        if (mobile && area && !area->isInside(targetPosition))
            targetAreaFactor = targetAreaFactor * 0.75f;
        if (!info->getUnit() || !vehicleOccupied(object->m_handle, side))
            continue;
        float strengthSum;
        float score = strengthsFor(info, object->m_handle, targetAreaFactor, strengthSum);
        float fraction = (distance * 1.5f) / maxRange;
        if (!(fraction > 0.1f))
            fraction = 0.1f;
        if (fraction > 1.0f)
            fraction = 1.0f;
        score = (1.0f - fraction) * bestStrength * ageFactor * approachFactor * attackFactor * score;
        if (strengthSum <= static_cast<float>(static_cast<BotMain*>(bot)->getHarmlessThrsh()))
            score = score * 0.33f;
        if (info->getUnit() && score > secondaryScore)
        {
            secondaryScore = score;
            secondaryTarget = it->first;
            secondaryWeapon = weaponIndex;
        }
    }

    if (armament->isAntiAircraft() == 0)
    {
        std::vector<unsigned int> enemies;
        static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
            ->getEnemyObjects(enemies, bot, 850.0f);
        for (std::vector<unsigned int>::const_iterator it = enemies.begin(); it != enemies.end(); ++it)
        {
            Information* const info = informationManager->getInformation(*it);
            if (!info || info->getSide() == side)
                continue;
            IPIPhysical* const targetPhysical = info->getPhysical();
            if (!targetPhysical || (info->getObjectTypeFlags() & 0x20000) || (info->getObjectTypeFlags() & 0x100000)
                || (info->getObjectTypeFlags() & 0x800000) || 0.0f >= targetPhysical->getHealth())
                continue;
            if (20.0 > now - getBBPFeedback(*it)->m_time)
                continue;
            const float attackFactor = attackFactorFor(*it);
            if (!mobile)
            {
                Vec3 cameraPosition;
                cameraPosition.x = cameraPosition.y = cameraPosition.z = 0.0f;
                controls->getCameraBasePos(cameraPosition);
                const Vec3& target = info->getPosition();
                Vec3 aim;
                aim.x = target.x - cameraPosition.x;
                aim.y = target.y - cameraPosition.y;
                aim.z = target.z - cameraPosition.z;
                const float lengthSquared = aim.x * aim.x + aim.y * aim.y + aim.z * aim.z;
                if (!(std::fabs(lengthSquared - 1.0f) < 1.19209290e-7f))
                {
                    if (std::fabs(lengthSquared) < 1.19209290e-7f)
                        aim.x = aim.y = aim.z = 0.0f;
                    else
                    {
                        const float reciprocal = 1.0f / std::sqrt(lengthSquared);
                        aim.x = aim.x * reciprocal;
                        aim.y = aim.y * reciprocal;
                        aim.z = aim.z * reciprocal;
                    }
                }
                if (!controls->validateCameraDirectionYaw(aim))
                    continue;
            }
            IPIMobile* const targetMobile = info->getMobile();
            Vec3 targetPosition;
            targetPosition.x = targetPosition.y = targetPosition.z = 0.0f;
            if (targetMobile)
                targetMobile->getPositionEstimate(targetPosition);
            else
                targetPosition = info->getPosition();
            Vec3 direction;
            direction.x = targetPosition.x - position.x;
            direction.y = targetPosition.y - position.y;
            direction.z = targetPosition.z - position.z;
            float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (0.5f > distance)
                distance = 0.5f;
            const float inverse = 1.0f / distance;
            direction.x = direction.x * inverse;
            direction.y = direction.y * inverse;
            direction.z = direction.z * inverse;
            int weaponIndex = -1;
            float maxRange = 0.0f;
            float bestStrength = 0.0f;
            float weaponScores[8]; // shipped: no upper bound on weapons.size()
            const std::vector<unsigned int>& weapons = armament->getWeapons();
            unsigned int i = 0;
            for (; i < weapons.size(); ++i)
            {
                if (!WeaponInfoManager::m_instance->getInformation(weapons[i]))
                {
                    weaponScores[i] = 0.0f;
                    continue;
                }
                WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
                const float strength = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
                int ammo = weapon->getAmmo();
                if (ammo == -1)
                    ammo = 0x10000;
                weaponScores[i] = ammo > 0 ? strength / (10.0f / static_cast<float>(ammo) + 1.0f) : 0.0f;
                if (weaponScores[i] > bestStrength)
                {
                    bestStrength = weaponScores[i];
                    weaponIndex = i;
                }
                if (weapon->getMaxRange() > maxRange)
                    maxRange = weapon->getMaxRange();
            }
            if (bestStrength == 0.0f)
                continue;
            for (; i < 8; ++i)
                weaponScores[i] = 0.0f;
            const float approachFactor = approachFactorFor(info, targetMobile, direction, distance, maxRange, false);
            if (distance > maxRange * 0.89f)
            {
                if (!mobile)
                    continue;
                const Vec3& target = info->getPosition();
                const int vehicle = mobile->getVehicleNumber();
                if (pathfinding->isValidPosition(vehicle, equipmentType, target, true))
                    continue;
            }
            float targetAreaFactor = areaFactor;
            if (mobile && area && !area->isInside(targetPosition))
                targetAreaFactor = targetAreaFactor * 0.75f;
            if (!info->getUnit() || !vehicleOccupied(*it, side))
                continue;
            float strengthSum;
            float score = strengthsFor(info, *it, targetAreaFactor, strengthSum);
            float fraction = distance / (maxRange * 1.5f);
            if (!(fraction > 0.1f))
                fraction = 0.1f;
            if (fraction > 1.0f)
                fraction = 1.0f;
            score = (1.0f - fraction) * bestStrength * approachFactor * attackFactor * 0.75f * score;
            if (!(strengthSum > static_cast<float>(static_cast<BotMain*>(bot)->getHarmlessThrsh())))
            {
                if (score > secondaryScore)
                {
                    secondaryScore = score;
                    secondaryWeapon = weaponIndex;
                    secondaryTarget = *it;
                    secondarySpatial = true;
                }
            }
            else
            {
                if (currentTarget == *it)
                {
                    currentScore = score;
                    keepCurrent = true;
                }
                if (score > bestScore)
                {
                    bestScore = score;
                    m_targets[bot->getBotId()] = *it;
                    bestWeapon = weaponIndex;
                    bestSpatial = true;
                }
            }
        }
    }

    float scale = 1.0f;
    if (bestScore > 0.0f)
    {
        if (keepCurrent)
            currentScore = currentScore * 1.2f;
        if (keepCurrent && currentScore > bestScore)
        {
            m_targets[bot->getBotId()] = currentTarget;
            m_unknown24[bot->getBotId()] = currentWeapon;
        }
        else
        {
            bot->getBotId(); // result unused
            // Shipped defect: load the previous slot instead of storing the newly selected weapon.
            bestWeapon = m_unknown24[bot->getBotId()];
            keepCurrent = false;
        }
        initializeGenerator(static_cast<BotMain*>(bot), bestWeapon, false, false, bestSpatial);
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
            bestScore = bestScore * bot->getMemory()->m_float8;
        m_targetChanged[bot->getBotId()] = !keepCurrent;
        scale = scale * modifier;
        m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(bestScore + bestScore) * scale;
        bot->addToFireInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    m_targetChanged[bot->getBotId()] = !keepCurrent;
    if (secondaryScore > 0.0f)
    {
        m_targets[bot->getBotId()] = secondaryTarget;
        initializeGenerator(static_cast<BotMain*>(bot), secondaryWeapon, false, false, secondarySpatial);
    }
    else if (deadScore > 0.0f)
    {
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        initializeGenerator(static_cast<BotMain*>(bot), -1, true, true, false);
    }
    else
    {
        m_pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        return 0.0f;
    }
    // Shipped quirk: the secondary/dead-score path omits the caller's modifier.
    const float weighted = scale * (secondaryScore > 0.0f ? secondaryScore : deadScore);
    secondaryScore = secondaryScore + deadScore;
    m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(secondaryScore + secondaryScore) * weighted;
    bot->addToFireInclination(m_urgency[bot->getBotId()]);
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        m_urgency[bot->getBotId()] = m_urgency[bot->getBotId()] * bot->getMemory()->m_float8;
    return m_urgency[bot->getBotId()];
}

// ELF 0x212CD0
BBFireLargeBore::BBPFeedback* BBFireLargeBore::getBBPFeedback(unsigned int target)
{
    std::map<unsigned int, BBPFeedback*>::iterator it = m_feedback.find(target);
    if (it != m_feedback.end())
        return it->second;
    BBPFeedback* const feedback = checkPointer(new BBPFeedback);
    feedback->m_time = -10000.0;
    m_feedback.insert(std::make_pair(target, feedback));
    return feedback;
}

} // namespace ai
} // namespace hfe
} // namespace dice
