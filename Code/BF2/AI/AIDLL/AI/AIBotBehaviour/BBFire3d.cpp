// Aircraft fire behaviour, recovered from the symbolized Linux 1.50 libaidll.so.

// static init: ELF 0x20E060 (__tcf_0 0x20E0C0, _GLOBAL__I 0x20E0F0): <iostream>'s Init object only.
#include <iostream>

#include "BBFire3d.h"

#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/SensedObject.h"
#include "../AIBotBehaviourPattern/BBPFire.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"
#include "../AIStrategic/SAI.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/VehicleOccupancy.h"
#include "../Main/AIMain.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x20BB00 (C2) / 0x20BCF0 (C1); vtable GOT -0x964. The base gets getMaxNBots; +0x28 = 0.0
// (fld of the 0.0f constant, fstpl), the empty map at +0x30 (a 0x18-byte header node); a second
// getMaxNBots sizes the +0x08 and +0x24 arrays (plain operator new[], no checkPointer); every bot
// starts with urgency 0.0f and both slots -1; the pattern gets this behaviour as its urgency
// generator. Unlike BBFire, no CustomCurve.
BBFire3d::BBFire3d(unsigned char index, unsigned char queue, BBPFire* pattern, UrgeCurve* curve,
                   float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
    , m_double28(0.0)
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int bots = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = new unsigned int[bots];
    m_unknown24 = new unsigned int[bots];
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_unknown24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    pattern->setUrgencyGenerator(this);
}

// ELF 0x20BE30 (D2) / 0x20BEF0 (D1) / 0x20BFB0 (D0): delete[] +0x08, then +0x24 (each when set),
// then the map's destructor (_M_erase of the nodes; the BBPFeedback objects are not deleted) and
// ~BotBehaviour.
BBFire3d::~BBFire3d()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unknown24)
        delete[] m_unknown24;
}

// ELF 0x20C070
float BBFire3d::calculateUrgency(Bot* bot, bool recalculate, float modifier)
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

    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
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

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIUnit* const unit = equipment->getUnit();
    IPIPhysical* const physical = equipment->getPhysical();
    IPIArmament* const armament = equipment->getArmament();
    IPIMobile* const mobile = equipment->getMobile();
    IPIControlInfo* const controls = equipment->getControlInfo();
    const unsigned long long aimControls = controls->getAimControls();
    const unsigned long long driveControls = controls->getDriveControls();
    if ((aimControls & driveControls) && !bot->isAirBorn())
    {
        m_pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        return 0.0f;
    }

    const int strengthType = physical->getStrengthType();
    float indirectScore = 0.0f; // never updated in the shipped body
    float bestScore = 0.0f;
    float currentScore = 0.0f;
    unsigned int countedOrders = 0;
    int bestWeapon = -1;
    const int side = bot->getSide();
    const unsigned int currentTarget = m_targets[bot->getBotId()];
    const int currentWeapon = m_unknown24[bot->getBotId()];
    bool keepCurrent = false;
    std::vector<IAIObject*> unusedObjects;
    unusedObjects.push_back(equipment->getAIObject());
    unusedObjects.push_back(0);

    const Vec3& position = equipment->getPosition();
    const unsigned int botId = bot->getBotId();
    const int orderedArea = IAIMain::m_instance->getSAI(side)->getBotOrderedPos(botId);
    AIStrategicObject* const area = AIStrategicObjectManager::getInstance()->getStrategicObject(orderedArea);
    const float areaFactor = area ? (static_cast<AIStrategicArea*>(area)->isInside(position) ? 1.0f : 0.75f) : 1.0f;
    equipment->getTransformation(); // result unused
    Vec3 indirectPosition;
    indirectPosition.x = indirectPosition.y = indirectPosition.z = 0.0f;

    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end(); ++it)
    {
        SensedObject* const object = it->second;
        if (!(object->m_flags & 2) && !(object->m_flags & 4))
            continue;
        Information* const info = informationManager->getInformation(object->m_handle);
        if (!info)
            continue;
        Bot* const other = IBotManager::instance->getBotFromPlayerId(info->getPlayerId());
        if (other && info->isOfType(ITAir))
        {
            Information* const otherTarget = informationManager->getInformation(other->getFiringTarget());
            if (otherTarget && otherTarget->getPlayerId() == bot->getPlayerId())
                continue;
        }
        if (20.0 > now - getBBPFeedback(object->m_handle)->m_time)
            continue;
        int weapon = -1;
        const double seenTime = (object->m_flags & 8) ? object->m_firstSeenTime : 0.0;
        const float score = updateUrgencyWithTarget(object->m_handle, info, seenTime, now, weapon,
            static_cast<BotMain*>(bot), position, equipment, armament, mobile, unit, strengthType,
            areaFactor, area, countedOrders);
        if (currentTarget == object->m_handle)
        {
            keepCurrent = true;
            currentScore = score;
        }
        if (score > bestScore)
        {
            bestScore = score;
            m_targets[bot->getBotId()] = object->m_handle;
            bestWeapon = weapon;
        }
    }

    if (!keepCurrent)
    {
        std::vector<unsigned int> enemies;
        static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
            ->getEnemyObjects(enemies, bot, -1.0f);
        for (std::vector<unsigned int>::const_iterator it = enemies.begin(); it != enemies.end(); ++it)
        {
            if (20.0 > now - getBBPFeedback(*it)->m_time)
                continue;
            Information* const info = informationManager->getInformation(*it);
            if (!info)
                continue;
            Bot* const other = IBotManager::instance->getBotFromPlayerId(info->getPlayerId());
            if (other && info->isOfType(ITAir))
            {
                Information* const otherTarget = informationManager->getInformation(other->getFiringTarget());
                if (otherTarget && otherTarget->getPlayerId() == bot->getPlayerId())
                    continue;
            }
            int weapon = -1;
            const float score = updateUrgencyWithTarget(*it, info, 0.0, now, weapon,
                static_cast<BotMain*>(bot), position, equipment, armament, mobile, unit, strengthType,
                areaFactor, area, countedOrders);
            if (currentTarget == *it)
            {
                keepCurrent = true;
                currentScore = score;
            }
            if (score > bestScore)
            {
                bestScore = score;
                m_targets[bot->getBotId()] = *it;
                bestWeapon = weapon;
            }
        }
    }

    float scale = 1.0f;
    // The point-target branch survives in ia-32, but its handle remains -1.
    const unsigned int indirectTarget = static_cast<unsigned int>(-1);
    if (indirectTarget != static_cast<unsigned int>(-1))
    {
        m_urgency[bot->getBotId()] = 5.0f;
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        initializeGenerator(indirectTarget, indirectPosition, static_cast<BotMain*>(bot),
                            m_unknown24[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }
    if (bestScore > 0.0f)
    {
        if (keepCurrent)
            currentScore = currentScore * 1.2f;
        if (keepCurrent && currentScore > bestScore && currentWeapon == bestWeapon)
        {
            // Shipped quirk: restore the target, but keep the best candidate's score.
            m_targets[bot->getBotId()] = currentTarget;
            m_unknown24[bot->getBotId()] = bestWeapon;
        }
        else
        {
            bot->getBotId(); // result unused
            m_unknown24[bot->getBotId()] = bestWeapon;
            keepCurrent = false;
        }
        initializeGenerator(static_cast<BotMain*>(bot), bestWeapon, false, false);
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
            bestScore = bestScore * bot->getMemory()->m_float8;
        m_targetChanged[bot->getBotId()] = !keepCurrent;
        scale = scale * modifier;
        m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(bestScore + bestScore) * scale;
        bot->addToFireInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    m_targetChanged[bot->getBotId()] = !keepCurrent;
    if (!(indirectScore > 0.0f))
    {
        m_pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        return 0.0f;
    }
    m_targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    initializeGenerator(static_cast<BotMain*>(bot), -1, false, false);
    const float weighted = scale * indirectScore;
    indirectScore = indirectScore + indirectScore;
    m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(indirectScore + indirectScore) * weighted;
    bot->addToFireInclination(m_urgency[bot->getBotId()]);
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        m_urgency[bot->getBotId()] = m_urgency[bot->getBotId()] * bot->getMemory()->m_float8;
    return m_urgency[bot->getBotId()];
}

// ELF 0x20D0A0
void BBFire3d::initializeGenerator(BotMain* bot, int weapon, bool a, bool b)
{
    bot->setFiringTarget(m_targets[bot->getBotId()]);
    static_cast<BBPFire*>(m_pattern)->init(bot, m_targets[bot->getBotId()], weapon, a, b);
}

// ELF 0x20D130
void BBFire3d::initializeGenerator(unsigned int target, const Vec3& position, BotMain* bot, int weapon)
{
    bot->setFiringTarget(target);
    static_cast<BBPFire*>(m_pattern)->init(bot, target, position, weapon);
}

// ELF 0x20D190
BBFire3d::BBPFeedback* BBFire3d::getBBPFeedback(unsigned int target)
{
    std::map<unsigned int, BBPFeedback*>::iterator it = m_feedback.find(target);
    if (it != m_feedback.end())
        return it->second;
    BBPFeedback* const feedback = new BBPFeedback;
    feedback->m_time = -10000.0;
    m_feedback.insert(std::make_pair(target, feedback));
    return feedback;
}

// ELF 0x20D230
float BBFire3d::updateUrgencyWithTarget(unsigned int target, Information* info, double seenTime,
    double now, int& bestWeapon, BotMain* bot, const Vec3& position, Information* equipment,
    IPIArmament* armament, IPIMobile* mobile, IPIUnit* unit, int strengthType, float areaFactor,
    AIStrategicObject* area, unsigned int& countedOrders)
{
    if (!info || info->getSide() == bot->getSide())
        return 0.0f;
    Information* const primary = informationManager->getInformation(info->getPrimaryObject(bot->getSide()));
    IPIPhysical* const physical = info->getPhysical();
    if (!physical || info->isOfType(ITLowPriority) || info->isOfType(ITVegetation)
        || info->isOfType(ITNoTemperature) || 0.0f >= physical->getHealth())
        return 0.0f;

    float attackFactor = 1.0f;
    double attackTime;
    if (bot->isAttacking(target, attackTime))
    {
        float ramp = static_cast<float>(now - attackTime) / 30.0f;
        if (0.0f > ramp)
            ramp = 0.0f;
        else if (ramp > 1.0f)
            ramp = 1.0f;
        attackFactor = 1.5f - ramp * 0.5f;
    }

    IPIMobile* const targetMobile = primary->getMobile();
    Vec3 targetPosition;
    targetPosition.x = targetPosition.y = targetPosition.z = 0.0f;
    if (targetMobile)
        targetMobile->getPositionEstimate(targetPosition);
    else
        targetPosition = primary->getPosition();
    Vec3 direction;
    direction.x = targetPosition.x - position.x;
    direction.y = targetPosition.y - position.y;
    direction.z = targetPosition.z - position.z;
    float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (0.5f > distance)
        distance = 0.5f;
    if (distance > 0.00001f)
    {
        const float reciprocal = 1.0f / distance;
        direction.x = direction.x * reciprocal;
        direction.y = direction.y * reciprocal;
        direction.z = direction.z * reciprocal;
    }

    float weaponScores[8]; // shipped body has no upper bound on the weapon count
    float bestStrength = 0.0f;
    float maxRange = 0.0f;
    const std::vector<unsigned int>& weapons = armament->getWeapons();
    unsigned int i = 0;
    for (; i < weapons.size(); ++i)
    {
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        if (!weapon)
        {
            weaponScores[i] = 0.0f;
            continue;
        }
        const float strength = static_cast<float>(weapon->getStrength(physical->getStrengthType()));
        int ammo = weapon->getAmmo();
        if (ammo == -1)
            ammo = 0x10000;
        weaponScores[i] = ammo > 0 ? strength : 0.0f;
        if (weaponScores[i] > bestStrength)
        {
            bestWeapon = i;
            bestStrength = weaponScores[i];
        }
        if (weapon->getMaxRange() > maxRange)
            maxRange = weapon->getMaxRange();
    }
    if (bestStrength == 0.0f)
        return 0.0f;
    for (; i < 8; ++i)
        weaponScores[i] = 0.0f;

    float ageFactor = 1.0f;
    if (seenTime > 0.0)
    {
        const float age = static_cast<float>(now - seenTime);
        ageFactor = 1.0f / (age * 0.05f + 1.0f);
    }
    Vec3 targetSpeed;
    targetSpeed.x = targetSpeed.y = targetSpeed.z = 0.0f;
    if (targetMobile)
        targetSpeed = targetMobile->getSpeed();
    const float speed = std::sqrt(targetSpeed.x * targetSpeed.x + targetSpeed.y * targetSpeed.y
                                 + targetSpeed.z * targetSpeed.z);

    bool calculateApproach = true;
    float approachFactor = 1.0f;
    if (primary->isOfType(ITAir))
    {
        if (armament->isAntiAircraft() == 0)
        {
            if (primary->getAltitude() > 5.0f)
                return 0.0f;
            if (distance > maxRange * 0.5f || speed > 15.0f)
            {
                approachFactor = 0.0f;
                calculateApproach = false;
            }
        }
        else if (!equipment->isOfType(ITAir) && maxRange * 0.9f > distance)
        {
            calculateApproach = false;
        }
    }
    if (calculateApproach && targetMobile)
    {
        if (distance > maxRange)
        {
            float maxSpeed = 0.0f;
            Vec3 ownSpeed;
            ownSpeed.x = ownSpeed.y = ownSpeed.z = 0.0f;
            if (mobile)
            {
                maxSpeed = mobile->getMaxSpeed();
                ownSpeed = mobile->getSpeed();
            }
            if (targetMobile->getMaxSpeed() - maxSpeed > 5.0f)
            {
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
                const float targetSpeedMagnitude = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y
                                                           + velocity.z * velocity.z);
                approachFactor = 0.25f / (radial * 2.0f + 1.0f + transverse * 2.0f
                                         + targetSpeedMagnitude * 0.5f);
            }
            else
                approachFactor = 0.25f;
        }
        else
        {
            const Vec3& velocity = targetMobile->getSpeed();
            approachFactor = 1.0f / (std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y
                                             + velocity.z * velocity.z) * 0.5f + 1.0f);
        }
    }

    float targetAreaFactor = areaFactor;
    if (mobile && !equipment->isOfType(ITAir) && area
        && !static_cast<AIStrategicArea*>(area)->isInside(targetPosition))
        targetAreaFactor = targetAreaFactor * 0.75f;
    if (!info->getUnit() || !vehicleOccupied(target, bot->getSide()))
        return 0.0f;

    float strengthSum = 0.0f;
    float score = 0.0f;
    unsigned int partHandle = target;
    int secondary = info->getFirstSecondaryObjectIterator();
    const int targetStrengthType = info->getPhysical()->getStrengthType();
    const float* const armourValues = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getArmourClassValues();
    Information* part = info;
    do
    {
        IPIUnit* const partUnit = part->getUnit();
        strengthSum = strengthSum + partUnit->getBattleStrengths()[strengthType];
        float orderFactor = 1.0f;
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders()
            && countedOrders < bot->getOrderList(m_index)->size())
        {
            std::list<unsigned int>::iterator end = bot->getOrderList(m_index)->end();
            std::list<unsigned int>::iterator begin = bot->getOrderList(m_index)->begin();
            std::list<unsigned int>::iterator searchEnd = bot->getOrderList(m_index)->end();
            std::list<unsigned int>::iterator found = std::find(begin, searchEnd, partHandle);
            if (found != end)
            {
                ++countedOrders;
                orderFactor = bot->getMemory()->m_float8;
            }
        }
        score = (partUnit->getBattleStrengths()[strengthType] + part->getStrength()
                 + unit->getBattleStrengths()[targetStrengthType]) * armourValues[targetStrengthType]
                 * orderFactor * targetAreaFactor + score;
        partHandle = info->getSecondaryObject(secondary, bot->getSide());
        part = informationManager->getInformation(partHandle);
    }
    while (part);

    float rangeFactor;
    if (primary->isOfType(ITAir))
    {
        rangeFactor = 1.0f - distance / (maxRange * 1.5f);
        if (!(rangeFactor > 0.0f))
            rangeFactor = 0.0f;
        if (armament->isAntiAircraft())
            score = score + score;
    }
    else
    {
        float fraction = distance / (maxRange * 3.0f);
        if (fraction > 1.0f)
            fraction = 1.0f;
        rangeFactor = 1.0f - fraction;
    }
    const Mat4& transformation = equipment->getTransformation();
    float facing = direction.x * transformation.m[8] + direction.y * transformation.m[9]
                 + direction.z * transformation.m[10] + 1.0f;
    if (!(facing > 0.5f))
        facing = 0.5f;
    rangeFactor = facing * 0.5f * rangeFactor;
    rangeFactor = bestStrength * rangeFactor;
    rangeFactor = rangeFactor * ageFactor;
    rangeFactor = rangeFactor * approachFactor;
    score = rangeFactor * attackFactor * score;
    if (strengthSum <= static_cast<float>(bot->getHarmlessThrsh()))
        score = score * 0.01f;
    return score;
}

} // namespace ai
} // namespace hfe
} // namespace dice
