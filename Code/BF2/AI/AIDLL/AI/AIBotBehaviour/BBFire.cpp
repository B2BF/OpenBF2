// Fire behaviour base (AI/AIDLL/AI/AIBotBehaviour/BBFire.cpp in the image). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address. Package D.

// static init: ELF 0x20B720 (__tcf_0 0x20B780, _GLOBAL__I 0x20B7B0): <iostream>'s Init object only.
#include <iostream>

#include "BBFire.h"

#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/BBPFire.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/SensedObject.h"
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
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIUtils/checkPointer.h"
#include "../AIUtils/VehicleOccupancy.h"
#include "../Main/AIMain.h"
#include "../Main/AIRandom.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIBot/BotNavigation.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../../../AIInterface/BFKnowledge/IWeapon.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAISettings* settings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

// [INFERRED] the inline reader of io::Input: a mapped input's value, else 0. chooseWeapon
// (0x20B278) tests the 64-bit mapping mask and reads the value directly.
float mappedInput(const io::PlayerInput* input, io::PlayerInputMap index)
{
    return ((input->mappingFlags >> index) & 1) ? input->inputs[index] : 0.0f;
}

} // namespace

// ELF 0x2079B0
BBFire::BBFire(unsigned char index, unsigned char queue, BBPFire* pattern, UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, settings()->getMaxNBots())
    , m_double28(0.0)
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;

    const int bots = settings()->getMaxNBots();
    m_targets = checkPointer(new unsigned int[bots]);
    m_unknown24 = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_unknown24[i] = static_cast<unsigned int>(-1);
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    pattern->setUrgencyGenerator(this);
    m_curve = new CustomCurve(&CustomCurve::modifiedSoftStepFunc, 0.0f, 1.0f, 0.03f);
}

// ELF 0x207E80
BBFire::~BBFire()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unknown24)
        delete[] m_unknown24;
    if (m_curve)
        delete m_curve;
}

// ELF 0x208040 (listing block by block; Ghidra pseudo-code cross-check). Scores every sensed,
// hostile, living object as a target and keeps the best one in the per-bot target array
// (BotBehaviour +0x08), its weapon index in m_unknown24, and hands both to the pattern through
// initializeGenerator. Without a target the behaviour is un-initialised and its urgency is 0.
//
// A candidate's score is the summed strength of it and its secondary objects (the own battle
// strength against it, its information strength and its battle strength against the own
// strength type, times its armour class value, the order factor and the area factor), times
// the range factor, the best weapon's strength, the indirect-sense age factor, the approach
// factor, the attack factor, the area factor, a CustomCurve of an AIRandom draw and the
// weapon's range quality, and * 0.33 when the summed strength is at most the harmless
// threshold. The current target is kept while it scores more than 1 / 1.2 of the best.
// The weapon-score array, the up vector and several dead Vec3 temporaries are as compiled.
float BBFire::calculateUrgency(Bot* bot, bool recalculate, float modifier)
{
    static bool checkOpportunist = false;      // .bss; nothing in the image writes it
    static float enemyStrengthTweak = 2.0f;    // .data

    PlanGenerator* const pattern = m_pattern;
    unsigned int* const targets = m_targets;

    if (!recalculate)
    {
        if (!pattern->isInitiated(bot))
        {
            m_targetChanged[bot->getBotId()] = false;
            return m_urgency[bot->getBotId()] = 0.0f;
        }
        if (!informationManager->getInformation(targets[bot->getBotId()]))
        {
            m_targetChanged[bot->getBotId()] = true;
            pattern->unInit(bot);
            return m_urgency[bot->getBotId()] = 0.0f;
        }
        m_targetChanged[bot->getBotId()] = false;
        bot->addToFireInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    float areaFactor = 0.5f;
    const float* const armourClassValues = settings()->getArmourClassValues();

    // 0x20821A: once a minute, forget the feedback of objects that no longer exist.
    if (now - m_double28 > 60.0)
    {
        std::map<unsigned int, BBPFeedback*>::iterator it = m_feedback.begin();
        while (it != m_feedback.end())
        {
            if (informationManager->getInformation(it->first))
            {
                ++it;
                continue;
            }
            delete it->second;
            m_feedback.erase(it++);
        }
        m_double28 = now;
    }

    const int side = bot->getSide();
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const Vec3& position = equipment->getPosition();
    if (!equipment->isOfType(ITNaval) && !equipment->isOfType(ITAir) && equipment->isOfType(ITSoldier))
    {
        // 0x209FE0: a passenger seat whose soldier pose is 4 does not fire.
        if (equipment->isPrimary() && equipment->getSoldierPose() == 4)
        {
            m_targetChanged[bot->getBotId()] = true;
            pattern->unInit(bot);
            return m_urgency[bot->getBotId()] = 0.0f;
        }
    }

    IPIUnit* const unit = equipment->getUnit();
    const int equipmentType = unit->getEquipmentType();   // called before the null test (retail)
    IPIArmament* const armament = equipment->getArmament();
    IPIPhysical* const physical = equipment->getPhysical();
    IPIControlInfo* const controlInfo = equipment->getControlInfo();
    if (!unit || !physical || !armament || !controlInfo)
    {
        m_targetChanged[bot->getBotId()] = true;
        pattern->unInit(bot);
        return m_urgency[bot->getBotId()] = 0.0f;
    }

    IPIMobile* const mobile = equipment->getMobile();
    const bool exposedSoldier = unit->hasExposedSoldier();
    const int strengthType = physical->getStrengthType();
    int soldierStrengthType = 0;
    if (exposedSoldier)
    {
        // 0x209F74: the exposed soldier's own strength type. Retail quirk: no null test.
        Information* const soldier =
            informationManager->getInformation(bot->getPlayer()->getSoldier()->getInformation(side));
        soldierStrengthType = soldier->getPhysical()->getStrengthType();
    }

    const float zero = 0.0f;          // -0x19C: never written again
    unsigned int countedOrders = 0;   // -0x1A4
    WeaponInfo* bestWeaponInfo = 0;   // -0x204
    const unsigned int currentTarget = targets[bot->getBotId()];
    bot->getBotId();                  // called, result unused
    bool keepCurrentTarget = false;   // -0x1A9: the current target scored again

    const int orderedArea = IAIMain::m_instance->getSAI(side)->getBotOrderedPos(bot->getBotId());
    AIStrategicArea* const area =
        static_cast<AIStrategicArea*>(AIStrategicObjectManager::getInstance()->getStrategicObject(orderedArea));
    if (area && !area->isInside(position))
        areaFactor = 0.85f;

    Vec3 unused;                      // -0x158: zeroed, never read
    unused.x = zero;
    unused.y = zero;
    unused.z = zero;
    (void)unused;
    equipment->getTransformation();   // 0x2084A6: called, result unused here
    float bestDistance = zero;        // -0x208
    m_unknown24[bot->getBotId()] = static_cast<unsigned int>(-1);

    float currentTargetScore = zero;  // -0x1B0
    float bestScore = zero;           // -0x198
    const std::map<unsigned int, SensedObject*>& sensed = bot->getSensedObjects();
    for (std::map<unsigned int, SensedObject*>::const_iterator it = sensed.begin(); it != sensed.end(); ++it)
    {
        SensedObject* const object = it->second;
        if (!(object->m_flags & SensedObject::SENSE_SEEN) || !(object->m_flags & SensedObject::SENSE_SIDED))
            continue;
        Information* const info = informationManager->getInformation(object->m_handle);
        if (!info)
            continue;
        if (info->getSide() == side || info->getSide() == 0)
            continue;
        IPIUnit* const targetUnit = info->getUnit();
        IPIPhysical* const targetPhysical = info->getPhysical();
        if (!targetPhysical)
            continue;
        if (info->isOfType(ITLowPriority) || info->isOfType(ITVegetation) || info->isOfType(ITNoTemperature))
            continue;
        if (0.0f >= targetPhysical->getHealth())     // 0x208B46 jae: a NaN health is kept
            continue;
        if ((object->m_flags & 0x20) && object->m_fireRating < 0)
            continue;

        // 0x208B63: recent fire feedback on this object needs some movement to engage again.
        BBPFeedback* const feedback = getBBPFeedback(object->m_handle);
        if (now - feedback->m_time < 20.0)
        {
            float speed = 0.0f;
            if (mobile)
                speed = mobile->getSpeedMagnitude();
            if (info->getMobile())
                speed = speed + info->getMobile()->getSpeedMagnitude();
            if (speed < 2.0f)
                continue;
            static_cast<BotMain*>(bot)->setHadPrecisionTimeStamp(now);
        }

        // 0x208BDD: an object attacking the bot weighs up to 1.5 (3x for a human soldier), fading
        // to 1.0 over 30 seconds.
        float attackFactor;   // -0x1B4
        double attackTime;
        if (static_cast<BotMain*>(bot)->isAttacking(object->m_handle, attackTime))
        {
            const float age = static_cast<float>(now - attackTime);
            float ramp = age / 30.0f;
            if (0.0f > ramp)
                ramp = 0.0f;
            else if (ramp > 1.0f)
                ramp = 1.0f;
            attackFactor = 1.5f - ramp * 0.5f;
            if (!IBotManager::instance->getBotFromPlayerId(info->getPlayerId())
                && info->isOfType(ITSoldier))
                attackFactor = attackFactor * 3.0f;
        }
        else
        {
            attackFactor = 1.0f;
        }

        Vec3 targetPosition;   // -0x168
        targetPosition.x = zero;
        targetPosition.y = zero;
        targetPosition.z = zero;
        IPIMobile* const targetMobile = info->getMobile();
        if (targetMobile)
            targetMobile->getPositionEstimate(targetPosition);
        else
            targetPosition = info->getPosition();
        Vec3 direction;        // -0x148
        direction.x = targetPosition.x - position.x;
        direction.y = targetPosition.y - position.y;
        direction.z = targetPosition.z - position.z;
        float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y
                                   + direction.z * direction.z);
        if (distance > 0.0001f)
        {
            const float inverse = 1.0f / distance;
            direction.x = direction.x * inverse;
            direction.y = direction.y * inverse;
            direction.z = inverse * direction.z;
        }
        float indirectFactor = 1.0f;   // -0x224
        if (1.0f > distance)
            distance = 1.0f;

        if (object->m_flags & SensedObject::SENSE_INDIRECT)
        {
            // 0x208D65: an object not seen directly must stand on ground the bot's vehicle can use.
            if (!mobile)
                continue;
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            if (!targetMobile)
                continue;
            const IPIMobile::ValidPos& valid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
            if (valid.polygon == static_cast<unsigned int>(-1))
                continue;
            if (targetUnit->getEquipmentType() != equipmentType)
            {
                unsigned char material;
                pathfinding->getMaterial(mobile->getVehicleNumber(), valid.island, valid.polygon, material);
                if (!settings()->isValidMaterial(equipmentType, material))
                    continue;
            }
            const float age = static_cast<float>(now - object->m_firstSeenTime);
            indirectFactor = 1.0f / (age * 0.05f + 1.5f);
        }
        else if (!mobile)
        {
            // 0x209D64: a static weapon needs the seen part inside its camera's range of directions.
            Vec3 cameraBase;
            cameraBase.x = zero;
            cameraBase.y = zero;
            cameraBase.z = zero;
            controlInfo->getCameraBasePos(cameraBase);
            Vec3 seen;
            seen.x = zero;
            seen.y = zero;
            seen.z = zero;
            const Mat4& transformation = info->getTransformation();
            const Vec3& local = object->m_seenPosition;
            // 0x209DC3..0x209DEC: ((m4*y + m0*x) + m8*z) + m12.
            seen.x = transformation.m[4] * local.y + transformation.m[0] * local.x
                     + transformation.m[8] * local.z + transformation.m[12];
            seen.y = transformation.m[1] * local.x + transformation.m[5] * local.y
                     + transformation.m[9] * local.z + transformation.m[13];
            seen.z = local.x * transformation.m[2] + local.y * transformation.m[6]
                     + local.z * transformation.m[10] + transformation.m[14];
            if (!equipment->isOfType(ITAir) || equipment->isOfType(ITHelicopter))
            {
                if (!controlInfo->validateCameraDirectionByPos(seen))
                    continue;
            }
        }

        // 0x208E82: the strongest usable weapon against the target's strength type, and the
        // longest range of all weapons.
        float bestWeaponScore = 0.0f;   // -0x238
        int bestWeapon = -1;            // -0x234
        float maxRange = 0.0f;          // -0x230
        float weaponScores[8];          // -0x138
        std::vector<unsigned int>& weapons = armament->getWeapons();
        int i;
        for (i = 0; static_cast<unsigned int>(i) < weapons.size(); ++i)
        {
            if (!WeaponInfoManager::m_instance->getInformation(weapons[i]))
            {
                weaponScores[i] = 0.0f;
                continue;
            }
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            const int weaponType = weapon->getWeaponType();
            if (weaponType != 0 && weaponType != 4 && weaponType != 3)
            {
                weaponScores[i] = 0.0f;
                continue;
            }
            if (weaponType == 4 && targetMobile)
            {
                // Retail: the skipped weapon's score is left unwritten.
                const Vec3 faceing = equipment->getFaceing();
                const Vec3& speed = targetMobile->getSpeed();
                if (std::fabs(speed.y * faceing.y + speed.x * faceing.x + speed.z * faceing.z) > 4.0f)
                    continue;
            }
            int ammo = weapon->getAmmo();
            if (ammo == -1)
                ammo = 0x10000;
            if (ammo > 0)
                weaponScores[i] = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
            else
                weaponScores[i] = 0.0f;
            if (i == armament->getActiveWeapon())
                weaponScores[i] = weaponScores[i] * 1.03f;
            if (weaponScores[i] > bestWeaponScore)
            {
                bestWeapon = i;
                bestWeaponScore = weaponScores[i];
                bestWeaponInfo = weapon;
            }
            if (weapon->getMaxRange() > maxRange)
                maxRange = weapon->getMaxRange();
        }
        if (bestWeaponScore == 0.0f)
            continue;
        for (; i < 8; ++i)
            weaponScores[i] = 0.0f;

        Vec3 targetSpeed;   // -0x118
        targetSpeed.x = 0.0f;
        targetSpeed.y = 0.0f;
        targetSpeed.z = 0.0f;
        if (targetMobile)
            targetSpeed = targetMobile->getSpeed();
        const float targetSpeedMagnitude = std::sqrt(targetSpeed.x * targetSpeed.x + targetSpeed.y * targetSpeed.y
                                                     + targetSpeed.z * targetSpeed.z);

        float targetAreaFactor = areaFactor;   // -0x1A0
        if (mobile && !equipment->isOfType(ITAir) && area && !area->isInside(targetPosition))
            targetAreaFactor = targetAreaFactor * 0.85f;

        // 0x209247: the approach factor (-0x258).
        float approachFactor;
        bool closing = true;   // falls through to the closing-speed estimate below
        if (info->isOfType(ITAir))
        {
            // 0x209C55: an aircraft target.
            const int antiAircraft = armament->isAntiAircraft();
            if (!antiAircraft)
            {
                if (info->getAltitude() > 5.0f)
                    continue;
                if (distance > maxRange * 0.5f || targetSpeedMagnitude > 5.0f)     // 0x209D15 ja, 0x209D21 jbe
                {
                    approachFactor = 0.0f;
                    closing = false;
                }
            }
            else
            {
                if (checkOpportunist && antiAircraft == 2)
                {
                    const Mat4& transformation = equipment->getTransformation();
                    if (direction.y * transformation.m[5] + direction.x * transformation.m[4]
                            + direction.z * transformation.m[6]
                        > 0.5f)
                        continue;
                }
                if (maxRange * 0.9f > distance)     // 0x209C8D jbe
                {
                    approachFactor = 1.0f;
                    closing = false;
                }
            }
        }
        if (closing)
        {
            if (!targetMobile || !(distance > maxRange))
            {
                approachFactor = 1.0f;
                targetAreaFactor = 0.5f;
            }
            else
            {
                // 0x20927A: out of range; how fast the target can close in or cross.
                float ownMaxSpeed = 0.0f;
                Vec3 ownSpeed;   // -0x108
                ownSpeed.x = 0.0f;
                ownSpeed.y = 0.0f;
                ownSpeed.z = 0.0f;
                if (mobile)
                {
                    ownMaxSpeed = mobile->getMaxSpeed();
                    ownSpeed = mobile->getSpeed();
                }
                if (targetMobile->getMaxSpeed() - ownMaxSpeed > 5.0f)
                {
                    Vec3 up;     // -0xD8
                    up.x = 0.0f;
                    up.y = 1.0f;
                    up.z = 0.0f;
                    Vec3 across; // -0xF8: up x direction
                    across.x = up.y * direction.z - up.z * direction.y;
                    across.y = up.z * direction.x - up.x * direction.z;
                    across.z = up.x * direction.y - up.y * direction.x;
                    const Vec3& speed = targetMobile->getSpeed();
                    float towards = (direction.x * speed.x + direction.y * speed.y + direction.z * speed.z)
                                    - (direction.x * ownSpeed.x + direction.y * ownSpeed.y + direction.z * ownSpeed.z);
                    float sideways = (across.x * speed.x + across.y * speed.y + across.z * speed.z)
                                     - (ownSpeed.x * across.x + ownSpeed.y * across.y + ownSpeed.z * across.z);
                    if (0.0f > towards)     // 0x209BB4 fcmovbe keeps a NaN
                        towards = 0.0f;
                    if (0.0f > sideways)
                        sideways = 0.0f;
                    const float speedMagnitude = std::sqrt(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);
                    approachFactor = 0.25f / (towards + towards + up.y + (sideways + sideways)
                                              + speedMagnitude * 0.5f);
                }
                else
                {
                    approachFactor = 0.25f;
                }
            }
        }

        if (distance > maxRange)
        {
            // 0x209325: out of range; the bot must be able to drive to the target in 5 seconds.
            if (!targetMobile || !mobile)
                continue;
            const IPIMobile::ValidPos& valid = targetMobile->getLastValidPosition(mobile->getVehicleNumber());
            if (valid.polygon == static_cast<unsigned int>(-1))
                continue;
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            if (targetUnit->getEquipmentType() != equipmentType)
            {
                unsigned char material;
                pathfinding->getMaterial(mobile->getVehicleNumber(), valid.island, valid.polygon, material);
                if (!settings()->isValidMaterial(equipmentType, material))
                    continue;
            }
            if (distance - maxRange > mobile->getMaxSpeed() * 5.0f)
                continue;
        }

        // 0x209461: the strength of the object and its secondary objects (seats).
        if (!info->getUnit() || !vehicleOccupied(object->m_handle, side))
            continue;
        float strengthSum = 0.0f;   // -0x278
        float score = 0.0f;         // -0x194
        unsigned int partHandle = object->m_handle;
        Information* part = info;
        int secondary = info->getFirstSecondaryObjectIterator();
        const int targetStrengthType = info->getPhysical()->getStrengthType();
        do
        {
            IPIUnit* partUnit = part->getUnit();
            const unsigned int occupied = partUnit->getOccupiedObject(side);
            if (occupied != static_cast<unsigned int>(-1))
            {
                Information* const occupier = informationManager->getInformation(occupied);
                if (occupier)
                    partUnit = occupier->getUnit();
            }
            strengthSum = strengthSum + partUnit->getBattleStrengths()[strengthType];
            bool ordered = false;
            if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders()
                && bot->getOrderList(m_index)->size() > countedOrders)
            {
                if (bot->getOrderList(m_index)->end()
                    != std::find(bot->getOrderList(m_index)->begin(), bot->getOrderList(m_index)->end(),
                                 partHandle))
                    ordered = true;
            }
            float orderFactor;
            if (ordered)
            {
                ++countedOrders;
                orderFactor = bot->getMemory()->m_float8;
            }
            else
            {
                orderFactor = 1.0f;
            }
            float strength;
            if (!exposedSoldier)
                strength = partUnit->getBattleStrengths()[strengthType];
            else
                strength = std::max(partUnit->getBattleStrengths()[strengthType],
                                    partUnit->getBattleStrengths()[soldierStrengthType]);
            if (!info->isOfType(ITSoldier))
                strength = strength * enemyStrengthTweak;
            score = (strength + part->getStrength() + unit->getBattleStrengths()[targetStrengthType])
                        * armourClassValues[targetStrengthType] * orderFactor * targetAreaFactor
                    + score;
            partHandle = info->getSecondaryObject(secondary, side);
            part = informationManager->getInformation(partHandle);
        }
        while (part);

        // 0x209667
        float rangeFactor = 1.0f - distance / (maxRange * 1.2f);
        if (!(rangeFactor > 0.0f))
            rangeFactor = 0.0f;
        score = rangeFactor * bestWeaponScore * indirectFactor * approachFactor * attackFactor * targetAreaFactor
                * score;
        const float weaponMaxRange = bestWeaponInfo->getMaxRange();
        const float weaponMinRange = bestWeaponInfo->getMinRange();
        const float rangeQuality = ((weaponMaxRange - weaponMinRange) * 0.1f + weaponMinRange) / distance;
        score = m_curve->calculate(AIRandom::m_instance->nextFloat(), true) * score * rangeQuality;
        if (strengthSum <= static_cast<float>(static_cast<BotMain*>(bot)->getHarmlessThrsh()))
            score = score * 0.33f;
        if (!info->getUnit())
            continue;
        if (currentTarget == it->first)
        {
            currentTargetScore = score;
            keepCurrentTarget = true;
        }
        if (score > bestScore)
        {
            bestScore = score;
            targets[bot->getBotId()] = it->first;
            m_unknown24[bot->getBotId()] = bestWeapon;
            bestDistance = distance;
        }
    }

    float scale = 1.0f;   // -0x298
    if (bestScore > 0.0f)
    {
        if (keepCurrentTarget)
            currentTargetScore = currentTargetScore * 1.2f;
        if (keepCurrentTarget && currentTargetScore > bestScore)
        {
            if (targets[bot->getBotId()] != currentTarget)
            {
                // 0x20877F: stay with the current target; its distance replaces the best one.
                targets[bot->getBotId()] = currentTarget;
                Information* const target = informationManager->getInformation(currentTarget);
                const Vec3& from = equipment->getPosition();
                const Vec3& to = target->getPosition();
                const float dx = to.x - from.x;
                const float dy = to.y - from.y;
                const float dz = to.z - from.z;
                bestDistance = std::sqrt(dx * dx + dy * dy + dz * dz);
            }
        }
        else
        {
            bot->getBotId();   // called, result unused
            keepCurrentTarget = false;
        }
        const int weapon = chooseWeapon(bot, targets[bot->getBotId()], bestDistance);
        if (weapon != -1)
            m_unknown24[bot->getBotId()] = weapon;
        initializeGenerator(static_cast<BotMain*>(bot), m_unknown24[bot->getBotId()], false, false);

        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
            bestScore = bestScore * bot->getMemory()->m_float8;
        m_targetChanged[bot->getBotId()] = !keepCurrentTarget;
        scale = scale * modifier;
        m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(bestScore + bestScore) * scale;
        bot->addToFireInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    // 0x2088A8. -0x19C is never written after its 0.0, so the second branch (0x208936) is dead
    // code in the image; it is kept as compiled.
    m_targetChanged[bot->getBotId()] = !keepCurrentTarget;
    float indirectScore = zero;
    if (!(indirectScore > 0.0f))
    {
        pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        targets[bot->getBotId()] = static_cast<unsigned int>(-1);
        return 0.0f;
    }
    targets[bot->getBotId()] = static_cast<unsigned int>(-1);
    initializeGenerator(static_cast<BotMain*>(bot), -1, false, false);
    const float weighted = scale * indirectScore;
    indirectScore = indirectScore + indirectScore;
    m_urgency[bot->getBotId()] = DecleiningSlopeCurve::calculate(indirectScore + indirectScore) * weighted;
    bot->addToFireInclination(m_urgency[bot->getBotId()]);
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        m_urgency[bot->getBotId()] = m_urgency[bot->getBotId()] * bot->getMemory()->m_float8;
    return m_urgency[bot->getBotId()];
}

// ELF 0x20A090
void BBFire::initializeGenerator(BotMain* bot, int mode, bool a, bool b)
{
    unsigned int* const targets = m_targets;
    bot->setFiringTarget(targets[bot->getBotId()]);
    static_cast<BBPFire*>(m_pattern)->init(bot, targets[bot->getBotId()], mode, a, b);
}

// ELF 0x20A120
void BBFire::initializeGenerator(unsigned int target, const Vec3& position, BotMain* bot, int mode)
{
    bot->setFiringTarget(target);
    static_cast<BBPFire*>(m_pattern)->init(bot, target, position, mode);
}

// ELF 0x20A180
BBFire::BBPFeedback* BBFire::getBBPFeedback(unsigned int target)
{
    std::map<unsigned int, BBPFeedback*>::iterator it = m_feedback.find(target);
    if (it != m_feedback.end())
        return it->second;
    BBPFeedback* const feedback = checkPointer(new BBPFeedback);
    m_feedback.insert(std::make_pair(target, feedback));
    return feedback;
}

// ELF 0x20A350 (listing block by block; Ghidra pseudo-code cross-check). The index of the
// bot's weapon that scores best against the target at `distance`, or -1. A weapon scores its
// strength against the target's strength type, scaled down outside its range band and by 0.33
// with an empty magazine. Against a soldier the bot may pick a close-quarters weapon
// (spawn-kit dependent slot 0 or 5) and returns it at once when the pathfinding trace to the
// target's last valid position succeeds; a splash-damage weapon may be chosen for a static
// attack (score * 100, setUseStaticAttack(true)) when the target is in its band, not recently
// failed and not near the flag the bot is going for. setUseStaticAttack(false) comes first.
int BBFire::chooseWeapon(Bot* bot, unsigned int target, float distance)
{
    static bool allowMultipleC4 = false;   // .bss; nothing in the image writes it

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    const bool ownIsSoldier = equipment->isOfType(ITSoldier);
    IPIArmament* const armament = equipment->getArmament();
    static_cast<BotMain*>(bot)->setUseStaticAttack(false);
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    int bestWeapon = -1;
    float bestScore = 0.0f;

    Information* const targetInfo = informationManager->getInformation(target);
    if (!targetInfo)
        return -1;
    const bool targetIsSoldier = targetInfo->isOfType(ITSoldier);
    IPIPhysical* const targetPhysical = targetInfo->getPhysical();
    IPIMobile* const targetMobile = targetInfo->getMobile();
    if (!targetPhysical)
        return -1;

    // 0x20A478: the bot's active weapon. closeQuartersActive: a soldier holding the slot the
    // close-quarters weapon of its kit sits in; ownMagazineEmpty: the active magazine is empty.
    bool closeQuartersActive = false;   // -0x199
    bool ownMagazineEmpty = false;      // -0x19A
    IPIArmament* const ownArmament = equipment->getArmament();
    if (ownArmament && ownArmament->getActiveWeapon() != -1)
    {
        const int active = ownArmament->getActiveWeapon();
        WeaponInfo* const activeInfo = WeaponInfoManager::m_instance->getInformation(ownArmament->getWeapon(active));
        if (!activeInfo)
            return -1;
        IWeapon* const activeWeapon = activeInfo->getWeapon();
        if (ownIsSoldier
            && ((bot->getSpawnKit() != 5 && active == 0) || (bot->getSpawnKit() == 5 && active == 5)))
            closeQuartersActive = true;
        ownMagazineEmpty = false;
        if (activeWeapon->getAmmoInMag(0) == 0 && !closeQuartersActive)
            ownMagazineEmpty = true;
    }

    // 0x20A511: the target's active weapon and whether its magazine is empty.
    bool targetMagazineEmpty = false;   // -0x19B
    WeaponInfo* targetWeapon = 0;
    IPIArmament* const targetArmament = targetInfo->getArmament();
    if (targetArmament && targetArmament->getActiveWeapon() != -1)
    {
        targetWeapon = WeaponInfoManager::m_instance->getInformation(
            targetArmament->getWeapon(targetArmament->getActiveWeapon()));
        if (targetWeapon)
            targetMagazineEmpty = targetWeapon->getWeapon()->getAmmoInMag(0) == 0;
    }

    // 0x20A57D: whether the target fights the bot: a bot whose firing target is this bot's
    // player, or a human who attacked the bot (any recorded attack: within 9995 seconds).
    bool targetEngagesBot = false;      // -0x19C
    Bot* const targetBot = IBotManager::instance->getBotFromPlayerId(targetInfo->getPlayerId());
    if (!targetBot)
    {
        if (targetWeapon)
        {
            double attackTime;
            static_cast<BotMain*>(bot)->isAttacking(target, attackTime);
            targetEngagesBot = now - attackTime < 9995.0;
        }
    }
    else
    {
        Information* const firingTarget = informationManager->getInformation(targetBot->getFiringTarget());
        if (firingTarget && firingTarget->getPlayerId() == bot->getPlayerId())
            targetEngagesBot = true;
    }
    bool soldierAgainstSoldier = false; // -0x19D
    if (targetIsSoldier && ownIsSoldier)
        soldierAgainstSoldier = true;
    const bool targetIsHuman = !IBotManager::instance->getBotFromPlayerId(targetInfo->getPlayerId());   // -0x19E
    SensedObject* const sensed = bot->getSensedObjectFromHandle(target);

    std::vector<unsigned int>& weapons = armament->getWeapons();
    for (unsigned int i = 0; i < weapons.size(); ++i)
    {
        if (!WeaponInfoManager::m_instance->getInformation(weapons[i]))
            continue;
        WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
        const int weaponType = weapon->getWeaponType();
        if (weaponType != 0 && weaponType != 4 && weaponType != 3)
            continue;

        bool closeQuarters = false;     // -0x1B1
        bool meleeAttack = false;       // -0x1B2
        bool splash = false;            // -0x1C8: a vehicle's explosive weapon against a soldier
        if (!ownIsSoldier && targetInfo->isOfType(ITSoldier))
        {
            if (weapon->getWeaponGuidanceType() == 0 && weapon->getExplosionRadius() != 0.0f)
                splash = true;
        }
        if (!weapon->getFiresThroughTransparent() && !(sensed->m_flags & SensedObject::SENSE_INDIRECT)
            && !(sensed->m_flags & SensedObject::SENSE_FLAG40))
            continue;
        if (ownIsSoldier
            && ((bot->getSpawnKit() != 5 && i == 0) || (bot->getSpawnKit() == 5 && i == 5)))
            closeQuarters = true;
        if (targetInfo->getArmament() && targetInfo->getArmament()->getActiveWeapon() == 0)
            closeQuarters = false;
        if (weaponType == 4)
        {
            // 0x20B520: C4 once, and not against a target moving across the bot's facing.
            if (bot->isC4Placed() && !allowMultipleC4)
                continue;
            if (targetMobile)
            {
                const Vec3 faceing = equipment->getFaceing();
                const Vec3& speed = targetMobile->getSpeed();
                if (std::fabs(speed.y * faceing.y + speed.x * faceing.x + speed.z * faceing.z) > 4.0f)
                    continue;
            }
        }

        if (closeQuarters && ownIsSoldier && targetInfo->getMobile() && equipment->getMobile())
        {
            // 0x20B41F: no close quarters with a target off its last valid position or inside
            // an obstacle radius; that also ends soldierAgainstSoldier for the later weapons.
            const IPIMobile::ValidPos& valid =
                targetInfo->getMobile()->getLastValidPosition(equipment->getMobile()->getVehicleNumber());
            const Vec3& targetPosition = targetInfo->getPosition();
            Vec3 offset;
            offset.x = 0.0f;
            offset.y = 0.0f;
            offset.z = 0.0f;
            offset.x = targetPosition.x - valid.position.x;
            offset.y = targetPosition.y - valid.position.y;
            offset.z = targetPosition.z - valid.position.z;
            if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z > 1.0f
                || bot->getBotNavigation()->pointWithinObstacleRadius(targetInfo->getPosition()))
            {
                soldierAgainstSoldier = false;
                continue;
            }
        }

        // 0x20A810
        bool closeAndActive = false;    // -0x1D1
        if (closeQuartersActive && 10.0f > distance)
            closeAndActive = true;
        const float stamina = targetMobile ? targetMobile->getSprintStamina() : 0.0f;
        if ((stamina > 0.5f || (closeAndActive && stamina > 0.0f)) && closeQuarters
            && (targetMagazineEmpty || closeAndActive) && (ownMagazineEmpty || closeQuartersActive)
            && soldierAgainstSoldier && targetIsHuman && 20.0f > distance
            && (targetEngagesBot || closeQuartersActive))
            meleeAttack = true;

        int ammo = weapon->getAmmo();
        bool staticAttack = false;      // -0x1D2
        if (ammo == -1)
            ammo = 0x10000;
        if (!(ammo > 0) && weaponType != 3 && !meleeAttack)
            continue;
        float score = static_cast<float>(weapon->getStrength(targetPhysical->getStrengthType()));
        if (score == 0.0f)
            continue;
        if (ownIsSoldier && targetIsSoldier && score != 0.0f && 3.0f > distance && closeQuarters)
            meleeAttack = true;

        if (meleeAttack)
        {
            // 0x20B300: take the close-quarters weapon when the target's last valid position
            // can be reached.
            const IPIMobile::ValidPos& valid =
                targetInfo->getMobile()->getLastValidPosition(equipment->getMobile()->getVehicleNumber());
            IAIPathfinding* const pathfinding = IAIPathfinding::instance;
            Vec3 end;
            end.x = 0.0f;
            end.y = 0.0f;
            end.z = 0.0f;
            unsigned int polygon;
            const int equipmentType = equipment->getUnit()->getEquipmentType();
            if (pathfinding->trace(bot->getBotNavigation(), false, valid.position, end, polygon, equipmentType, -1,
                                   0.0f, 0, 0))
                return static_cast<int>(i);
        }

        if ((ownIsSoldier || splash) && targetInfo->isOfType(ITSoldier)
            && weapon->getWeaponGuidanceType() == 0 && weapon->getExplosionRadius() != 0.0f)
        {
            // 0x20ABCC: splash damage, unless the bot or the target is at the flag the bot goes for.
            bool goingForFlag = static_cast<BotMain*>(bot)->getIsGoingForFlag();   // -0x1D9
            bool targetNearFlag = false;                                           // -0x1DA
            if (!splash)
            {
                AIStrategicObject* const closest = AIStrategicObjectManager::getInstance()->getClosestObject(
                    equipment->getPosition(), bot->getSide(), false);
                Vec3 orderPosition;
                orderPosition.x = 0.0f;
                orderPosition.y = 0.0f;
                orderPosition.z = 0.0f;
                if (closest)
                {
                    orderPosition = closest->getOrderPos(equipment->getMobile()->getVehicleNumber(), bot->getSide());
                    float radius = closest->m_size[bot->getSide()];
                    Vec3 flag;
                    flag.x = 0.0f;
                    flag.y = 0.0f;
                    flag.z = 0.0f;
                    vec2ToVec3XZ(closest->getPosition(bot->getSide()), flag);
                    radius = radius * radius;
                    flag.y = orderPosition.y;
                    Vec3 offset;
                    offset.x = flag.x - orderPosition.x;
                    offset.y = flag.y - orderPosition.y;
                    offset.z = flag.z - orderPosition.z;
                    if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z < radius)
                    {
                        const float nearRadius = 40.0f;
                        if (closest->getStatus(bot->getSide()) != 0)
                        {
                            const Vec3& position = equipment->getPosition();
                            offset.x = position.x - orderPosition.x;
                            offset.y = position.y - orderPosition.y;
                            offset.z = position.z - orderPosition.z;
                            if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z
                                < nearRadius * nearRadius)
                            {
                                const io::PlayerInput* const input = bot->getInput();
                                if (mappedInput(input, io::PIThrottle) != 0.0f || mappedInput(input, io::PIYaw) != 0.0f
                                    || mappedInput(input, io::PISprint) != 0.0f)
                                    goingForFlag = true;
                            }
                        }
                    }
                    const Vec3& targetPosition = targetInfo->getPosition();
                    offset.x = flag.x - targetPosition.x;
                    offset.y = flag.y - targetPosition.y;
                    offset.z = flag.z - targetPosition.z;
                    if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z < radius)
                        targetNearFlag = true;
                }
            }
            if ((!goingForFlag && !targetNearFlag) || splash)
            {
                // 0x20AC13: the target in the weapon's band, not just failed, seen within the
                // window, and no grenade thrown in the last 10 seconds.
                const float heightDifference = targetInfo->getPosition().y - equipment->getPosition().y;
                float minRange = weapon->getMinRange();
                if (!splash)
                {
                    minRange = 5.0f;
                    if (!(heightDifference > 3.0f))
                        minRange = 10.0f;
                }
                float maxRange = weapon->getMaxRange();
                if (heightDifference > 5.0f && !weapon->getIsThrown())
                    maxRange = heightDifference * 1.25f + maxRange;
                if (static_cast<BotMain*>(bot)->getBotWantsToSimulateArtillery())
                {
                    minRange = minRange - 1.0f;
                    maxRange = 1.0f + maxRange;
                }
                bool aboveTarget = false;   // -0x208
                if (bot->getSpawnKit() != 1 && !weapon->getIsThrown() && heightDifference > 30.0f)
                    aboveTarget = true;
                aboveTarget = aboveTarget || splash;
                double seenWindow = 10.0;
                if (splash)
                    seenWindow = 5.0;
                int grenades = 1;
                if (!weapon->getIsThrown())
                {
                    if (weapon->getBurst())
                        grenades = 3;
                }
                const double sinceSeen = now - sensed->m_lastSeenTime;
                const double sinceGrenade = now - bot->getTimeOfGrenadeThrown(grenades - 1);
                const bool staticFailed =
                    now - static_cast<BotMain*>(bot)->getTimeOfLastFailedStaticAttack() < 10.0;
                bool recentlyFailed =
                    now - static_cast<BotMain*>(bot)->getTimeOfLastFailedSplashDamageAttack() < 5.0;
                const bool inBand = distance >= minRange && maxRange >= distance;
                if (!splash)
                    recentlyFailed = staticFailed;
                if (inBand && !recentlyFailed && (sensed->m_flags & SensedObject::SENSE_SEEN)
                    && ((sensed->m_flags & SensedObject::SENSE_INDIRECT) || aboveTarget) && seenWindow > sinceSeen
                    && (sinceGrenade > 10.0 || splash))
                {
                    staticAttack = true;
                    score = score * 100.0f;
                    static_cast<BotMain*>(bot)->setUseStaticAttack(true);
                }
            }
        }

        // 0x20A9E1: an empty magazine that reloads slowly is nearly worthless.
        IWeapon* const iweapon = weapon->getWeapon();
        if (iweapon->getAmmoInMag(0) == 0 && weapon->getReloadTime() > 1.9f && weapon->hasAutoReload())
        {
            static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();   // result unused
            if (iweapon->getWeaponGuidingSystem() == 0)
                score = 0.1f;
            else if (!iweapon->getProjectile())
                score = 0.1f;
        }
        const float maxRange = weapon->getMaxRange();
        const float minRange = weapon->getMinRange();
        if (!staticAttack)
        {
            if (minRange > distance)
            {
                float shortfall = (minRange - distance) / 5.0f;
                if (shortfall > 1.0f)
                    shortfall = 1.0f;
                score = ((1.0f - 0.9f) - shortfall * 0.09f) * score;
            }
            else if (distance > maxRange)
            {
                float excess = (distance - maxRange) / 5.0f;
                if (excess > 1.0f)
                    excess = 1.0f;
                score = (1.0f - excess * 0.9f) * score;
            }
        }
        if (iweapon->getAmmoInMag(0) == 0)
            score = score * 0.33f;
        if (score > bestScore)
        {
            bestScore = score;
            bestWeapon = static_cast<int>(i);
        }
    }
    return bestWeapon;
}

} // namespace ai
} // namespace hfe
} // namespace dice
