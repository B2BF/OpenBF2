// Change behaviour: rates the bot's current vehicle (and its seats), getting out on foot, and every
// nearby vehicle and seat (calculateVehicleUrgency), then initialises the BBPChange pattern with the best
// one. Radio requests (message types 0 and 1) and the human player calling for a pick-up weigh in.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md. Information flag bits (InformationType, AIDLL_r enum switch
// 0x100D3010): 0x10 ITAir (the air-vehicle tests: altitude, runway), 0x20 ITHelicopter, 0x40 ITGround,
// 0x80 ITNaval, 0x8000 ITMobile, 0x200000 ITSoldier.

// static init: ELF 0x2038C0 (__tcf_0 0x203920, _GLOBAL__I 0x203950): <iostream>'s Init object only.
#include <iostream>

#include "BBChange.h"
#include "VehicleUrgencyCalculator.h"

#include "UrgeCurve.h"
#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotBehaviourPattern/BBPChange.h"
#include "../AIBotBehaviourPattern/PlanGenerator.h"
#include "../AIKnowledge/AIInformationGrid.h"
#include "../AIKnowledge/BFInformationEnvironment.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIPathFinding/AerialHeightMap/AerialHeightMap.h"
#include "../AIStrategicMap/BFStrategicEnvironment.h"
#include "../AIUtils/VehicleOccupancy.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Math/Frustum.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <list>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF 0x203970 (W, this TU): the frustum query filter of runwayClear. +0x04 / +0x08 are the two
// informations runwayClear was given; vtable _ZTVN4dice3hfe2ai56_GLOBAL__N_AI_AIDLL_AI_AIBotBehaviour_
// BBChange.cppI8HLwd25RunwayObstructedPredicateE.
class RunwayObstructedPredicate : public IAIInformationGridPredicate
{
public:
    RunwayObstructedPredicate(Information* vehicle, Information* other) : m_vehicle(vehicle), m_other(other) {}

    // Neither of the two; flag 0x8000 set, 0x200000 and 0x80 clear.
    virtual bool includeInformation(Information* information) const
    {
        if (information == m_vehicle || information == m_other)
            return false;
        if (!information->isOfType(ITMobile))
            return false;
        if (information->isOfType(ITSoldier))
            return false;
        if (information->isOfType(ITNaval))
            return false;
        return true;
    }

private:
    /* +0x04 */ Information* m_vehicle;
    /* +0x08 */ Information* m_other;
};

// [CHOSEN, not recovered] helper: the image inlines this check at five sites (radio message types 0
// and 1): the sender of the list's first message, if that player exists, is human (IAIPlayer slot
// +0x88 false), alive (+0x24) and not man-down (+0x2C).
IAIPlayer* liveHumanSender(const std::list<AIRadioMessageInfo>& messages)
{
    IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                            ->getPlayerFromId(messages.front().m_senderPlayerId);
    if (!player || player->getIsAIPlayer() || !player->getIsAlive() || player->isManDown())
        return 0;
    return player;
}

} // namespace

// ELF 0x200000 (C2) / 0x200240 (C1). The base gets getMaxNBots (IAISettings slot 0); +0x24/+0x30 empty
// vectors, +0x3C/+0x40 = 0; then index, queue, pattern; +0x08 = checkPointer(new unsigned int[getMaxNBots])
// and, per bot, urgency (+0x10) = 0.0f (rodata 0x49CB34) and +0x08 = 0xFFFFFFFF.
BBChange::BBChange(unsigned char index, unsigned char queue, PlanGenerator* pattern, UrgeCurve* curve,
                   float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()),
      m_vehicles(), m_runwayObstacles(), m_unknown3C(0), m_unknown40(0)
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[count]);
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = 0xFFFFFFFFu;
    }
}

// ELF 0x200480 (D2) / 0x200530 (D1) / 0x2005E0 (D0): delete[] +0x08, delete +0x3C, delete +0x40 (each
// when set, slot +4), then the vectors (+0x30, +0x24) and the base.
BBChange::~BBChange()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unknown3C)
        delete m_unknown3C;
    if (m_unknown40)
        delete m_unknown40;
}

// ELF 0x200770. Ghidra decompile + asm (the locals carry their ELF frame offsets).
float BBChange::calculateUrgency(Bot* bot, bool evaluate, float scale)
{
    unsigned int* const targets = m_targets;
    PlanGenerator* const pattern = m_pattern;

    // The fire/move inclinations (Bot +0x190 / +0x18C) split into shares that sum to 1.
    float fireShare = 0.0f;   // -0x1B4
    float moveShare = 0.0f;   // -0x1B8
    int side = bot->getSide();                     // -0x1A4 (passed by reference to isMannedByEnemy)
    const unsigned int botId = bot->getBotId();    // -0x1BC
    bot->getSquadMember();                         // ELF 0x2007DC: result unused
    const float fire = bot->getFireInclination();
    const float move = bot->getMoveInclination();
    if (move > 0.0f)
    {
        if (fire > 0.0f)
        {
            fireShare = fire / (fire + move);
            moveShare = move / (fire + move);
        }
        else
        {
            moveShare = 1.0f;
        }
    }
    else if (fire > 0.0f)
    {
        fireShare = 1.0f;
    }
    else
    {
        fireShare = 0.5f;   // ELF 0x202E03: fsts + fstps of rodata 0x49CB44
        moveShare = 0.5f;
    }

    IAIStrategicEnvironment* const strategic =
        static_cast<IAIStrategicEnvironment*>(AISingletonServer::m_iaiStrategicEnvironment);
    CommunicationAgent* const comm = bot->getCommunicationAgent();   // -0x1C0
    const float* const enemyTypes = strategic->getEnemyTypes(bot);      // slot +0x0C, -0x1C4
    const float* const enemyStrengths = strategic->getEnemyStrengths(bot);   // slot +0x08, -0x1C8

    if (!evaluate)
    {
        // ---- ELF 0x200889: only re-check the bot's current choice.
        const unsigned int equipment = bot->getCurrentEquipment();
        const unsigned int soldier = bot->getPlayer()->getSoldier()->getInformation(side);
        const bool onFoot = soldier == equipment;   // -0x1CD

        if (!onFoot && isBailAllowed(bot))
        {
            Information* equipmentInfo = informationManager->getInformation(equipment);   // -0x1D4
            float current = calculateVehicleUrgency(bot, equipmentInfo, enemyStrengths, enemyTypes, false,
                                                             equipmentInfo, fireShare, moveShare, side) * 2.5f;
            if (!equipmentInfo->isPrimary())
                current *= modifyForDriver(
                    informationManager->getInformation(equipmentInfo->getPrimaryObject(side)), false);
            const float getOut = calculateVehicleUrgency(bot, informationManager->getInformation(soldier),
                                                                  enemyStrengths, enemyTypes, false, equipmentInfo,
                                                                  fireShare, moveShare, side) * 0.2f;
            if (current > getOut)
            {
                // ELF 0x200A27: keep the current target while it is free and the pattern runs.
                Information* target = informationManager->getInformation(targets[botId]);
                if (target)
                {
                    if (!target->getUnit()->isOccupied())
                    {
                        if (pattern->isInitiated(bot))
                        {
                            m_targetChanged[botId] = false;
                            m_urgency[botId] = checkBookings(bot);
                            return m_urgency[botId];
                        }
                    }
                    m_targetChanged[botId] = true;   // ELF 0x200A80: overwritten at once below (retail)
                }
            }
            else
            {
                // ELF 0x200B51: get out - the soldier itself is the target.
                m_targetChanged[botId] = targets[botId] != soldier;
                targets[botId] = soldier;
                static_cast<BBPChange*>(m_pattern)->init(soldier, false, false, bot);
                const float ratio = current != 0.0f ? getOut * 0.5f / current : getOut * 0.5f;
                m_urgency[botId] = DecleiningSlopeCurve::calculate(ratio) * (scale * 4.0f);
                m_urgency[botId] = checkBookings(bot);
                return m_urgency[botId];
            }
        }
        else
        {
            // ELF 0x200CC0
            Information* target = informationManager->getInformation(targets[botId]);
            if (onFoot && target)
            {
                if (target->getUnit() && !target->getUnit()->isOccupied() && pattern->isInitiated(bot))
                {
                    m_targetChanged[botId] = false;
                    m_urgency[botId] = checkBookings(bot);
                    // ELF 0x200D88: no null test of the equipment information (retail).
                    Information* equipmentInfo = informationManager->getInformation(equipment);
                    if (equipmentInfo->isOfType(ITSoldier))
                    {
                        const std::list<AIRadioMessageInfo>& calls =
                            comm->getReceivedMessages(static_cast<AIRadioMessageType>(1));
                        if (!calls.empty() && isCallingVehicle(targets[botId], calls, side))
                            static_cast<BBPChange*>(m_pattern)->setGetInConfirm(true, bot);
                    }
                    m_urgency[botId] = checkBookings(bot);
                    return m_urgency[botId];
                }
                m_targetChanged[botId] = true;   // ELF 0x200A80
            }
        }

        // ELF 0x200A93: drop the choice.
        m_targetChanged[botId] = pattern->isInitiated(bot);
        targets[botId] = 0xFFFFFFFFu;
        pattern->unInit(bot);
        m_urgency[botId] = 0.0f;
        return 0.0f;
    }

    // ---- ELF 0x200E68: full evaluation.
    float currentUrgency = 0.0f;   // -0x1EC: the current vehicle/seat's own urgency
    int seatIterator = 0;          // -0x19C, shared by every secondary-object walk below
    Vec3 entryPos;                 // -0x148
    Vec3 bestEntryPos;             // -0x138
    entryPos.x = entryPos.y = entryPos.z = -666.0f;   // rodata 0x49CC98
    bestEntryPos.x = bestEntryPos.y = bestEntryPos.z = -666.0f;
    SquadManager* const squadManager = SquadManager::m_instance;
    float orderModifier = 0.0f;    // -0x1F4
    bool wasCalled = false;        // -0x1F9: the chosen vehicle answered a radio call
    bool chooseSoldier = false;    // -0x1FA: getting out won

    const unsigned int equipment = bot->getCurrentEquipment();   // -0x1D8
    Information* equipmentInfo = informationManager->getInformation(equipment);   // -0x208, no null test
    Vec3& pos = equipmentInfo->getPosition();   // -0x20C
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);   // -0x210
    const bool isAir = equipmentInfo->isOfType(ITSoldier);   // -0x1FB
    bool inArtillery = false;      // -0x211
    double artillerySinceStart = 0.0;   // -0x198
    int artillerySide = -1;        // -0x190
    if (!equipmentInfo->isOfType(ITAir))
    {
        // ELF 0x202D27
        if (squadManager->isInArtilleryDangerZone(pos, bot->getSide(), artillerySinceStart, artillerySide)
            && artillerySinceStart > 4.0f)
            inArtillery = true;
    }

    if (!isAir && !isBailAllowed(bot))
    {
        // ELF 0x202D10 -> 0x200AB4
        m_targetChanged[botId] = pattern->isInitiated(bot);
        targets[botId] = 0xFFFFFFFFu;
        pattern->unInit(bot);
        m_urgency[botId] = 0.0f;
        return 0.0f;
    }

    // ELF 0x200FB2: the current vehicle's primary object and its seats.
    const unsigned int primary = equipmentInfo->getPrimaryObject(side);
    Information* primaryInfo = informationManager->getInformation(primary);   // -0x218
    Information* primaryInfoAgain = informationManager->getInformation(equipmentInfo->getPrimaryObject(side));   // -0x21C
    unsigned int bestHandle = primary;   // -0x1DC
    float candidate;                     // -0x1E4
    int vehicleNumber;                   // -0x200
    if (equipment == primary)
    {
        // ELF 0x202C23
        candidate = 2.0f * calculateVehicleUrgency(bot, primaryInfo, enemyStrengths, enemyTypes, false,
                                                            equipmentInfo, fireShare, moveShare, side);   // fadd st,st
        currentUrgency = candidate;
        if (0.0f > currentUrgency)
            currentUrgency = 0.0f;
        vehicleNumber = -1;
        if (primaryInfo->getMobile())
        {
            vehicleNumber = primaryInfo->getMobile()->getVehicleNumber();
            primaryInfo->getUnit()->getEquipmentType();   // ELF 0x202CEA: result unused
        }
    }
    else
    {
        candidate = 2.0f * calculateVehicleUrgency(bot, primaryInfo, enemyStrengths, enemyTypes, true,
                                                            equipmentInfo, fireShare, moveShare, side);
        vehicleNumber = -1;
    }

    float radioStrength;   // -0x1F8
    bool upsideDown;       // -0x212
    if (!isAir && isUpsideDown(primaryInfo, terrain, 0.7071f))   // rodata 0x49F580
    {
        candidate = 0.0f;
        upsideDown = true;
        radioStrength = 1.0f;
        if (primaryInfo->isOfType(ITAir) && !primaryInfo->getPhysical()->isInAir())
        {
            // ELF 0x202BB9: a grounded air vehicle counts as upright again.
            radioStrength = getRadioStrength(primaryInfo, bot);
            upsideDown = false;
            candidate *= radioStrength;
        }
    }
    else
    {
        radioStrength = getRadioStrength(primaryInfo, bot);   // ELF 0x202BE0
        upsideDown = false;
        candidate *= radioStrength;
    }

    float best = candidate;              // -0x1E8
    Information* bestInfo = primaryInfo; // -0x1E0
    if (!upsideDown)
    {
        // ELF 0x2028A7: the current vehicle's seats.
        unsigned int seatHandle = bestInfo->getFirstSecondaryObject(seatIterator, side);
        Information* seat = informationManager->getInformation(seatHandle);   // -0x220
        const float driverModifier = modifyForDriver(primaryInfo, primary == equipment);   // -0x1F0
        while (seat)
        {
            if (seatHandle == equipment)
            {
                // ELF 0x202AE4
                candidate = 2.0f * calculateVehicleUrgency(bot, seat, enemyStrengths, enemyTypes, false,
                                                                    equipmentInfo, fireShare, moveShare, side)
                            * driverModifier * radioStrength;
                currentUrgency = candidate;
                if (0.0f > currentUrgency)
                    currentUrgency = 0.0f;
            }
            else
            {
                candidate = calculateVehicleUrgency(bot, seat, enemyStrengths, enemyTypes, true,
                                                             equipmentInfo, fireShare, moveShare, side)
                            * driverModifier * radioStrength;
            }
            // ELF 0x202A80: an idle flag-taking squad member prefers an unarmed (+0x2C) vehicle's seats.
            if (!isAir && bot->getSquadMember()->isTakingFlag() && bot->getSquadMember()->getIsIdle()
                && !primaryInfo->hasArmament())
                candidate *= 5.0f;
            if (candidate > best)
            {
                bestHandle = seatHandle;
                bestInfo = seat;
                bestEntryPos = entryPos;
                best = candidate;
            }
            seatHandle = primaryInfo->getSecondaryObject(seatIterator, side);
            seat = informationManager->getInformation(seatHandle);
        }
    }

    // ELF 0x201175: getting out, against the radio message type 0 list.
    const unsigned int soldier = bot->getPlayer()->getSoldier()->getInformation(side);
    const std::list<AIRadioMessageInfo>& requests = comm->getReceivedMessages(static_cast<AIRadioMessageType>(0));   // -0x224
    bool requestValid = false;   // -0x225
    if (!requests.empty() && liveHumanSender(requests))
        requestValid = true;
    if (soldier != equipment)
    {
        Information* soldierInfo = informationManager->getInformation(soldier);   // -0x22C
        candidate = calculateVehicleUrgency(bot, soldierInfo, enemyStrengths, enemyTypes, false,
                                                     equipmentInfo, fireShare, moveShare, side) * 0.2f;
        if (!requests.empty() && requestValid && !equipmentInfo->isOfType(ITSoldier))
            candidate = 100.0f * requests.front().m_modifier * candidate;
        if (!requests.empty() && requestValid && equipmentInfo->isOfType(ITAir))
            candidate = 10000.0f;   // rodata 0x49F4C0
        if (inArtillery)
            candidate = 10000.0f;
        if (bot->isBotStuck() && soldierInfo)
        {
            bot->setBotStuck(false);
            candidate = 10000.0f;
        }
        AIStrategicArea* area = bot->getSquadMember()->getSquad()->getSquadStrategicAreaAssignment();
        if (area)
        {
            // ELF 0x201388: getOrderPos (slot +0x14) for 0 and 1; a height step over 3 checks avoidance.
            const Vec3 orderPos0 = area->getOrderPos(0, bot->getSide());
            const Vec3 orderPos1 = area->getOrderPos(1, bot->getSide());
            if (std::fabs(orderPos0.y - orderPos1.y) > 3.0f)
            {
                if (closeAndShouldAvoidVehicles(primaryInfoAgain, primaryInfoAgain->getSide(), bot))
                    candidate = 10000.0f;
            }
        }
        if (0.001f > currentUrgency)   // rodata 0x49CC94
            candidate = 10000.0f;
        soldierInfo->getMobile()->getVehicleNumber();    // ELF 0x201448: results unused
        soldierInfo->getUnit()->getEquipmentType();
        if (candidate > best)
        {
            chooseSoldier = true;
            best = candidate;
            bestHandle = soldier;
            bestInfo = soldierInfo;
        }
    }

    // ELF 0x20149A: an ordered "change" with an empty order list uses the memory's modifier (+0x08).
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        orderModifier = bot->getMemory()->m_float8;
    else
        orderModifier = 1.0f;

    // ELF 0x2014C7: a human (radio message type 1) asks for the vehicle the bot's target belongs to.
    bool requestedByHuman = false;     // -0x22D
    unsigned int requestedPrimary = 0xFFFFFFFFu;   // -0x234
    Information* currentTarget = informationManager->getInformation(targets[botId]);
    if (currentTarget)
    {
        requestedPrimary = currentTarget->getPrimaryObject(side);
        const std::list<AIRadioMessageInfo>& calls = comm->getReceivedMessages(static_cast<AIRadioMessageType>(1));
        if (!calls.empty() && equipmentInfo->isOfType(ITSoldier))
        {
            IAIPlayer* caller = liveHumanSender(calls);
            if (caller && caller->getCurrentVehicle()->getPrimaryObject()->getInformation(side) == requestedPrimary)
                requestedByHuman = true;
        }
    }

    unsigned int callingHandle = 0xFFFFFFFFu;   // -0x238: the last scanned vehicle's caller state (retail)
    if (!chooseSoldier && (requests.empty() || !requestValid) && !equipmentInfo->isOfType(ITAir)
        && !inArtillery && !bot->getSquadMember()->isCloseToNonOwnedTarget())
    {
        // ---- ELF 0x201AF9: every vehicle around.
        m_vehicles.resize(0);
        // ELF 0x201B6B: slot +0x14 returns the search radius in st0 (stored at -0x23C).
        const float searchRadius =
            static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
                ->getHardware(m_vehicles, pos, bot);
        bool reachable = false;   // -0x241: not reset per vehicle (retail)
        const std::list<AIRadioMessageInfo>& calls = comm->getReceivedMessages(static_cast<AIRadioMessageType>(1));   // -0x24C
        const float searchRadiusSq = searchRadius * searchRadius;   // -0x23C
        Information* equipmentInfoAgain = informationManager->getInformation(bot->getCurrentEquipment());   // -0x250

        for (std::vector<unsigned int>::iterator it = m_vehicles.begin(); it != m_vehicles.end(); ++it)
        {
            callingHandle = 0xFFFFFFFFu;
            unsigned int handle = *it;   // -0x1A8 (passed by reference to isMannedByEnemy)
            if (!calls.empty() && equipmentInfo->isOfType(ITSoldier)
                && isCallingVehicle(*it, calls, side))
                callingHandle = *it;

            Information* vehicle = informationManager->getInformation(handle);   // -0x254
            if (!vehicle || vehicle == primaryInfoAgain || vehicle->isOfType(ITSoldier))
                continue;
            if (requestedByHuman && handle != requestedPrimary)
                continue;
            if (vehicle->isOfType(ITNaval))
            {
                // ELF 0x201CF2: only the vehicle the calling human sits in, airborne and in water.
                if (calls.empty())
                    continue;
                IAIPlayer* caller = liveHumanSender(calls);
                if (!caller)
                    continue;
                const unsigned int callerVehicle =
                    caller->getCurrentVehicle()->getPrimaryObject()->getInformation(side);
                Information* callerVehicleInfo = informationManager->getInformation(callerVehicle);
                if (handle != callerVehicle || !callerVehicleInfo || !callerVehicleInfo->getPhysical())
                    continue;
                IPIPhysical* physical = callerVehicleInfo->getPhysical();
                if (!physical->isInAir() || !physical->isInWater())
                    continue;
            }
            if (!vehicle->getAIObject()->getIsAllowedByBots())
                continue;
            if (isMannedByEnemy(handle, vehicle, side))
                continue;

            Vec3& vehiclePos = vehicle->getPosition();   // -0x258
            IPIMobile* mobile = vehicle->getMobile();    // -0x248
            if (mobile && !vehicle->isOfType(ITAir))
            {
                // ELF 0x202570: no valid navmesh position - only the calling human's vehicle.
                const IPIMobile::ValidPos lastValid = mobile->getLastValidPosition();
                if (lastValid.island == 0xFFFFFFFFu)
                {
                    if (calls.empty())
                        continue;
                    IAIPlayer* caller = liveHumanSender(calls);
                    if (!caller)
                        continue;
                    const unsigned int callerPrimary =
                        caller->getCurrentVehicle()->getPrimaryObject()->getInformation(side);
                    const unsigned int callerVehicle = caller->getCurrentVehicle()->getInformation(side);
                    if (handle != callerPrimary || handle != callerVehicle)
                        continue;
                }
            }
            if (mobile && !vehicle->isOfType(ITAir) && !vehicle->isOfType(ITSoldier)
                && isUpsideDown(vehicle, terrain, 0.7071f))
                continue;

            // ELF 0x201E9A: distance falloff, 1 at the bot down to 0 beyond the search radius.
            IPIUnit* unit = vehicle->getUnit();
            Vec3 delta;
            delta.x = vehiclePos.x - pos.x;
            delta.y = vehiclePos.y - pos.y;
            delta.z = vehiclePos.z - pos.z;
            const float distSq = delta.z * delta.z + delta.y * delta.y + delta.x * delta.x;   // -0x240
            float falloff = (searchRadiusSq - distSq * 0.8f) / searchRadiusSq;
            if (0.5f <= falloff)
                falloff = 0.5f;
            float distanceFactor = 0.0f;   // -0x25C
            if (0.0f <= falloff + 0.5f)
                distanceFactor = falloff + 0.5f;

            if (vehicleNumber == -1)
            {
                reachable = true;   // ELF 0x202530
            }
            else if (unit)
            {
                if (unit->useNoPathfindingToGetToObject() || !vehicle->getMobile())
                {
                    reachable = true;   // ELF 0x2024FE
                    entryPos = unit->getEntryPositionOfNonMobile();
                }
                else
                {
                    // ELF 0x201FA9: vehicle number 0, not vehicleNumber (retail).
                    const IPIMobile::ValidPos& valid = mobile->getLastValidPosition(0);
                    if (vehicle->getPhysical())
                    {
                        const float radius = vehicle->getPhysical()->getRadius();
                        if (equipmentInfoAgain->getPhysical())
                        {
                            const float ownRadius = equipmentInfoAgain->getPhysical()->getRadius();
                            Vec3 gap;
                            gap.x = vehiclePos.x - valid.position.x;
                            gap.y = vehiclePos.y - valid.position.y;
                            gap.z = vehiclePos.z - valid.position.z;
                            reachable = gap.x * gap.x + gap.y * gap.y + gap.z * gap.z
                                        < (radius + ownRadius) * (radius + ownRadius);
                        }
                    }
                }
            }
            bool usable;
            if (!vehicle->getMobile() || vehicle->isOfType(ITAir))
                usable = true;
            else
                usable = reachable;
            if (!usable || !reachable)
                continue;

            // ELF 0x2020CB: the vehicle's primary seat.
            radioStrength = getRadioStrength(vehicle, bot);
            float urgency = calculateVehicleUrgency(bot, vehicle, enemyStrengths, enemyTypes, true,
                                                             equipmentInfo, fireShare, moveShare, side)
                            * distanceFactor * radioStrength;
            candidate = urgency;
            if (requestedByHuman)
                candidate = urgency * 5.0f;
            // ELF 0x2024AD: no null test of the physical plug-in here (the seat loop has one).
            if (isAir && 400.0f > distSq && 0.25f > equipmentInfo->getPhysical()->getHealth())
                candidate *= 50.0f;
            if (callingHandle != 0xFFFFFFFFu)
                candidate *= calls.front().m_modifier;
            if (candidate > best)
            {
                if (!vehicle->isOfType(ITAir) || vehicle->isOfType(ITHelicopter) || runwayClear(vehicle, equipmentInfo, side))
                {
                    bestHandle = handle;
                    wasCalled = callingHandle != 0xFFFFFFFFu;
                    best = candidate;
                    bestEntryPos = entryPos;
                    bestInfo = vehicle;
                }
            }

            // ELF 0x20222D: its seats.
            unsigned int seatHandle = vehicle->getFirstSecondaryObject(seatIterator, side);
            Information* seat = informationManager->getInformation(seatHandle);
            const float driverModifier = modifyForDriver(vehicle, false);   // -0x1F0
            while (seat)
            {
                urgency = calculateVehicleUrgency(bot, seat, enemyStrengths, enemyTypes, true, equipmentInfo,
                                                           fireShare, moveShare, side)
                          * radioStrength * distanceFactor * driverModifier;
                candidate = urgency;
                if (requestedByHuman)
                    candidate = urgency * 5.0f;
                else if (isAir && 400.0f > distSq && equipmentInfo->getPhysical()
                         && 0.25f > equipmentInfo->getPhysical()->getHealth())
                    candidate = candidate * 50.0f;
                if (callingHandle != 0xFFFFFFFFu)
                    candidate *= calls.front().m_modifier;
                if (candidate > best)
                {
                    bestHandle = seatHandle;
                    wasCalled = callingHandle != 0xFFFFFFFFu;
                    bestEntryPos = entryPos;
                    best = candidate;
                    bestInfo = seat;
                }
                seatHandle = vehicle->getSecondaryObject(seatIterator, side);
                seat = informationManager->getInformation(seatHandle);
            }
        }
    }

    // ---- ELF 0x2015B0: commit the choice.
    if (bestHandle == equipment)
    {
        // ELF 0x201A50: nothing better than staying - refuse a caller and drop the choice.
        if (isAir && callingHandle != 0xFFFFFFFFu)
            denyGetIn(equipmentInfo, callingHandle, comm);
        m_targetChanged[botId] = pattern->isInitiated(bot);
        targets[botId] = 0xFFFFFFFFu;
        pattern->unInit(bot);
        m_urgency[botId] = 0.0f;
        return 0.0f;
    }

    const bool samePrimary = bestInfo->getPrimaryObject(side) == equipmentInfo->getPrimaryObject(side);   // -0x25D
    m_targetChanged[botId] = targets[botId] != bestHandle;
    targets[botId] = bestHandle;
    if (isAir && callingHandle != 0xFFFFFFFFu && !botChoiceInVehicle(targets[botId], callingHandle, side))
        denyGetIn(equipmentInfo, callingHandle, comm);
    static_cast<BBPChange*>(m_pattern)->init(bestHandle, bestEntryPos, samePrimary, wasCalled, bot);

    // ELF 0x2016B2: the 1.0f (rodata 0x49CB2C) is a separate factor of the scale in the image.
    const float unitFactor = 1.0f;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const float ratio = currentUrgency != 0.0f ? best * 0.5f / currentUrgency : best * 0.5f;
    m_urgency[botId] = DecleiningSlopeCurve::calculate(ratio) * (unitFactor * scale * 4.0f) * orderModifier;
    const double sincePossessed = now - equipmentInfo->getUnit()->getLastPlayerPossesionTime();   // fstpl
    if (15.0f > sincePossessed)
        m_urgency[botId] *= 6.0f;

    if (chooseSoldier)
    {
        // ELF 0x201930: bailing out of a damaged vehicle says so on the radio.
        Information* current = informationManager->getInformation(bot->getCurrentEquipment());
        if (current)
        {
            if (IPIPhysical* physical = current->getPhysical())
            {
                const float health = physical->getHealth();
                if (IBotManager::instance->getVehicleBailValue() + 0.1f >= health)
                    comm->sendImmediately(static_cast<AIRadioMessageType>(0),
                                             static_cast<AIRadioChannelType>(7), 10, 0);
            }
            m_urgency[botId] *= 5.0f;
        }
    }
    if (isAir)
    {
        // ELF 0x201843
        const Vec3& bestPos = bestInfo->getPosition();
        Vec3 delta;
        delta.x = bestPos.x - pos.x;
        delta.y = bestPos.y - pos.y;
        delta.z = bestPos.z - pos.z;
        if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z < 400.0f && equipmentInfo->getPhysical()
            && equipmentInfo->getPhysical()->getHealth() < 0.25f)
            m_urgency[botId] *= 5.0f;
    }

    // ELF 0x2017AC: no null test of the target information (retail).
    Information* target = informationManager->getInformation(targets[bot->getBotId()]);
    bot->insertBehaviourInformation(0.5f, target->getPosition(), m_index);
    m_urgency[botId] = checkBookings(bot);
    return m_urgency[botId];
}

// ELF 0x2006A0: a positive urgency drops to 0 while the chosen vehicle is booked by someone else
// (Bot::getPlayerId, slot +0xB4).
float BBChange::checkBookings(Bot* bot)
{
    const unsigned int botId = bot->getBotId();
    float urgency = m_urgency[botId];
    Information* target = informationManager->getInformation(m_targets[botId]);
    if (target && target->isBooked() && !target->isBookedBy(bot->getPlayerId()))
    {
        if (m_urgency[botId] > 0.0f)   // fcmovnbe against rodata 0x49CB34
            urgency = 0.0f;
    }
    return urgency;
}

// ELF 0x202E20: a driver seat recently given up by a player (less than 15 s, rodata 0x49CDC4) keeps
// 1.0f; otherwise by flag 0x80 -> 1.0f, 0x40 -> 0.77f (rodata 0x49F58C), 0x10 -> 0.5f, else 1.0f.
float BBChange::modifyForDriver(Information* vehicle, bool current)
{
    if (!vehicle)
        return 1.0f;
    IPIUnit* unit = vehicle->getUnit();
    // ELF 0x202E54: the time is taken before the unit's null test (retail order).
    const double sincePossessed = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime()
                                  - vehicle->getUnit()->getLastPlayerPossesionTime();
    if (!unit)
        return 1.0f;
    if (!current && unit->isOccupied())
        return 1.0f;
    if (!vehicle->getMobile())
        return 1.0f;
    if (15.0f > sincePossessed)
        return 1.0f;
    if (vehicle->isOfType(ITNaval))
        return 1.0f;
    if (vehicle->isOfType(ITGround))
        return 0.77f;
    if (vehicle->isOfType(ITAir))
        return 0.5f;
    return 1.0f;
}

// ELF 0x202F00: an orthographic frustum along the aircraft's flattened forward axis (box size x/y
// times 1.2, rodata 0x49F590; near z * -0.6, rodata 0x49CFA4; far z * 12, rodata 0x49F594) must hold
// no obstacle (RunwayObstructedPredicate) for the given side.
bool BBChange::runwayClear(Information* aircraft, Information* other, int side) const
{
    LocalBoundingBox* box = const_cast<LocalBoundingBox*>(aircraft->getPhysical()->getLocalBoundingBox());
    const float sizeX = box->getSize().x;
    const float sizeZ = box->getSize().z;
    const float sizeY = box->getSize().y;
    aircraft->getPosition();   // ELF 0x202F6F: slot +0x30, result unused

    // ELF 0x202F7D: the matrix is zeroed (16 x 0.0f) before the copy, then row 2's y is cleared and the
    // rows are re-orthonormalised: row 1 receives the third axis, rows 2 and 0 are rewritten.
    Mat4 transform;
    for (int i = 0; i < 16; ++i)
        transform.m[i] = 0.0f;
    transform = aircraft->getTransformation();
    transform.m[9] = 0.0f;
    reinterpret_cast<Vec3*>(&transform.m[4])->makeOrthonormalBasis(*reinterpret_cast<Vec3*>(&transform.m[8]),
                                                                  *reinterpret_cast<Vec3*>(&transform.m[0]));

    const float widthScale = 1.2f;
    const float farScale = 12.0f;
    Frustum frustum;
    frustum.setOrtho(sizeX * widthScale, sizeY * widthScale, sizeZ * -0.6f, sizeZ * farScale, transform);

    m_runwayObstacles.resize(0);
    RunwayObstructedPredicate predicate(aircraft, other);
    static_cast<IAIInformationEnvironment*>(AISingletonServer::m_iaiInformationEnvironment)
        ->getObjectsWithinFrustum(m_runwayObstacles, frustum, side, true, &predicate);
    return m_runwayObstacles.size() == 0;
}

// ELF 0x203210: 1.0f for flag 0x200000, else 1.0f squared (fmul st,st of rodata 0x49CB2C).
float BBChange::getRadioStrength(Information* vehicle, Bot*) const
{
    const float strength = 1.0f;
    if (vehicle->isOfType(ITSoldier))
        return strength;
    return strength * strength;
}

// ELF 0x203240: a vehicle of another side (Information slot +0x44) that is occupied for our side.
bool BBChange::isMannedByEnemy(unsigned int& handle, Information* vehicle, int& side) const
{
    bool manned = false;
    if (vehicle->getSide() != side && vehicleOccupied(handle, side))
        manned = true;
    return manned;
}

// ELF 0x2032A0: the transformation's m[5] (row 1, y; +0x14) below 0.01f (rodata 0x49CB30). The terrain
// and the threshold argument are not read.
bool BBChange::isUpsideDown(Information* vehicle, IAITerrain*, float) const
{
    return 0.01f > vehicle->getTransformation().m[5];
}

// ELF 0x2032E0: always true while the bot has its soldier information, except an air vehicle (flag
// 0x10) that is primary and between 2 and 12 (rodata 0x49CC74 / 0x49F594) above the aerial height map.
bool BBChange::isBailAllowed(Bot* bot) const
{
    Information* equipment = informationManager->getInformation(bot->getCurrentEquipment());
    Information* soldier =
        informationManager->getInformation(bot->getPlayer()->getSoldier()->getInformation(bot->getSide()));
    if (!soldier)
        return false;
    if (equipment->isPrimary())
    {
        // ELF 0x2033D3: the position and two unused queries, then the altitude test.
        const Vec3& position = equipment->getPosition();
        equipment->getUnit()->useNoPathfindingToGetToObject();
        equipment->isPrimary();
        if (equipment->isOfType(ITAir))
        {
            const float altitude = AerialHeightMap::m_instance->getAltitude(position);
            if (altitude > 2.0f && 12.0f > altitude)
                return false;
        }
    }
    // ELF 0x2033A1: both outcomes of the remaining tests answer true (retail).
    if (equipment->getMobile())
        return true;
    if (equipment->isPrimary())
        return true;
    return true;
}

// ELF 0x203460: a not-yet-handled (m_flag16) message whose sender owns the vehicle or one of its seats
// (IAIObject slot +0x148, inlined Information::getPlayerId). False when the vehicle is manned by an enemy.
bool BBChange::isCallingVehicle(unsigned int handle, const std::list<AIRadioMessageInfo>& messages, int side) const
{
    Information* vehicle = informationManager->getInformation(handle);
    int seatIterator = 0;
    if (isMannedByEnemy(handle, vehicle, side))
        return false;
    for (std::list<AIRadioMessageInfo>::const_iterator it = messages.begin(); it != messages.end(); ++it)
    {
        if (it->m_flag16)
            continue;
        if (vehicle->getAIObject()->getPlayerId() == it->m_senderPlayerId)
            return true;
        Information* seat = informationManager->getInformation(vehicle->getFirstSecondaryObject(seatIterator, side));
        while (seat)
        {
            if (seat->getAIObject()->getPlayerId() == it->m_senderPlayerId)
                return true;
            seat = informationManager->getInformation(vehicle->getSecondaryObject(seatIterator, side));
        }
    }
    return false;
}

// ELF 0x203610: after a confirmed get-in request (message type 1), a caller closer than 15 (225 squared,
// rodata 0x49CF4C) hears message 7 on channel 6, priority 3, delay 1.0f, -1.
void BBChange::denyGetIn(Information* vehicle, unsigned int caller, CommunicationAgent* comm) const
{
    if (!comm->generalConfirmReceivedMessage(static_cast<AIRadioMessageType>(1)))
        return;
    // ELF 0x20366C: no null test of the caller's information (retail).
    const Vec3 callerPos = informationManager->getInformation(caller)->getPosition();
    const Vec3 vehiclePos = vehicle->getPosition();
    Vec3 delta;
    delta.x = vehiclePos.x - callerPos.x;
    delta.y = vehiclePos.y - callerPos.y;
    delta.z = vehiclePos.z - callerPos.z;
    if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z < 225.0f)
        comm->enqueueRadioMessage(static_cast<AIRadioMessageType>(7), static_cast<AIRadioChannelType>(6), 3, 1.0f, -1);
}

// ELF 0x203740: true when the bot's target is the calling vehicle's primary object or one of its seats.
bool BBChange::botChoiceInVehicle(unsigned int target, unsigned int caller, int side) const
{
    Information* callerPrimary =
        informationManager->getInformation(informationManager->getInformation(caller)->getPrimaryObject(side));
    if (callerPrimary == informationManager->getInformation(target))
        return true;
    int seatIterator;   // ELF -0x10: not initialised (getFirstSecondaryObject sets it)
    unsigned int seatHandle = callerPrimary->getFirstSecondaryObject(seatIterator, side);
    Information* seat = informationManager->getInformation(seatHandle);
    while (seat)
    {
        if (seatHandle == target)
            return true;
        seatHandle = callerPrimary->getSecondaryObject(seatIterator, side);
        seat = informationManager->getInformation(seatHandle);
    }
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
