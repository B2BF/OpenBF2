// Change pattern: per-bot target vehicle/seat, entry position and get-in flags.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x23DC90 (__tcf_0 0x23DCF0, _GLOBAL__I 0x23DD20): <iostream>'s Init object only.
#include <iostream>

#include "BBPChange.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIBotActionPlan/BAPAMoveTo3dObject.h"
#include "../AIBotActionPlan/BAPASendRadioMessage.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPICTurnLock.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../../Math/Mat4.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPAUpdateVehicle.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConAnd.h"
#include "../AIBotActionPlan/BAPConNot.h"
#include "../AIBotActionPlan/BAPConObjectBehind.h"
#include "../AIBotActionPlan/BAPConObjectDistance.h"
#include "../AIBotActionPlan/BAPConObjectEnemyOccupied.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPConObjectOccupied.h"
#include "../AIBotActionPlan/BAPConOr.h"
#include "../AIBotActionPlan/BAPFlowCIf.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPICChangeVehicle.h"
#include "../AIBotActionPlan/BAPIWChange.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2357E0 (C2) / 0x235CB0 (C1). After the BBPPattern base, four getMaxNBots-sized arrays, each
// checkPointer'd (the inlined "AI::Allocation failed" Debug blocks): +0x10 Vec3 (the new[] loop stores
// 0.0f, rodata 0x49CB34, into all three floats before the null test), +0x14 and +0x18 bool (left
// uninitialised), then +0x08 = the argument and +0x0C unsigned int (uninitialised).
BBPChange::BBPChange(int value)
{
    m_entryPositions = checkPointer(
        new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]());
    m_sameVehicle = checkPointer(new bool[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_getInConfirm = checkPointer(new bool[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_patternId = value;
    m_targets = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x236180 (D2) / 0x236210 (D1) / 0x2362A0 (D0): delete[] +0x0C, +0x14, +0x10, +0x18 (each when
// set), then the BBPPattern base.
BBPChange::~BBPChange()
{
    if (m_targets)
        delete[] m_targets;
    if (m_sameVehicle)
        delete[] m_sameVehicle;
    if (m_entryPositions)
        delete[] m_entryPositions;
    if (m_getInConfirm)
        delete[] m_getInConfirm;
}

namespace {

// The plan-reset idiom of createPlan (debugger slot +0x164 -> clear vfn +0xC; plan slot +0x60, deleted
// through vfn +8, then setPlan(0) slot +0x64; script context slot +0x168 -> BAPContext::clear). The
// image inlines it three times with two different orders; the callers keep their order.
void clearDebugger(Bot* bot)
{
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
}

void deletePlan(Bot* bot)
{
    if (bot->getPlan()) {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

// Radius of an information's physical plug-in (+0x3C, IPIPhysical::getRadius vfn +0x2C); a seat that is
// not the primary object uses its primary's (getPrimaryObject(bot side), no null test in the image).
float radiusOf(Information* info, Bot* bot)
{
    if (info->isPrimary())
        return info->getPhysical()->getRadius();
    return informationManager->getInformation(info->getPrimaryObject(bot->getSide()))->getPhysical()->getRadius();
}

} // namespace

// ELF 0x236A20. Stack-slot map and branch order in docs/macos-ai-w1-f-urgency.md.
BotScript* BBPChange::createPlan(Bot* bot)
{
    const unsigned int botId = bot->getBotId();                                  // esi
    CommunicationAgent* const communication = bot->getCommunicationAgent();      // -0x180
    if (!m_initiated[bot->getBotId()]) {
        // 0x23DC07: debugger, context, plan.
        clearDebugger(bot);
        bot->getScriptContext()->clear();
        deletePlan(bot);
        return 0;
    }

    // 0x236A8A..0x236AC5, -0x181 / -0x182: keep the running plan when it is this pattern's
    // (BAPStatement +0x10) and wraps the same target (BAPIWChange +0x18).
    bool otherPattern = false;
    if (!bot->getPlan() || static_cast<BAPStatement*>(bot->getPlan())->getPatternId() != m_patternId)
        otherPattern = true;
    bool otherTarget = false;
    if (otherPattern || static_cast<BAPIWChange*>(bot->getPlan())->getTarget() != m_targets[botId])
        otherTarget = true;
    if (!otherTarget) {
        if (m_getInConfirm[botId] && communication->generalConfirmReceivedMessage(static_cast<AIRadioMessageType>(1)))
            communication->enqueueRadioMessage(static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(6), 3,
                                               1.0f, -1);
        return bot->getPlan();
    }

    static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
    // -0x188 current equipment, -0x18C its IPIMobile, -0x190 its IPIPhysical, -0x198/-0x194 its drive
    // controls. The image reads +0x38/+0x3C/+0x30 before the null test at 0x236CEE.
    Information* const current = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = current->getMobile();
    const unsigned long long driveControls = current->getControlInfo()->getDriveControls();
    // -0x19C target (the vehicle/seat), -0x1A0 its IPIUnit.
    Information* const target = informationManager->getInformation(m_targets[bot->getBotId()]);
    if (!target) {
        // 0x23DB20: debugger, plan, context.
        clearDebugger(bot);
        deletePlan(bot);
        bot->getScriptContext()->clear();
        return 0;
    }
    IPIUnit* const targetUnit = target->getUnit();
    // -0x1A8 = target radius (-0x1A4) + current radius (-0x1AC), float adds (0x236C98).
    const float targetRadius = radiusOf(target, bot);
    const float currentRadius = radiusOf(current, bot);
    float radiusSum = targetRadius + currentRadius;

    // 0x236CAF: debugger, plan, context.
    clearDebugger(bot);
    deletePlan(bot);
    bot->getScriptContext()->clear();
    if (!current)
        return 0;
    bot->getSquadMember()->clearRequestAction();

    BAPStatement* plan;   // the node the tail wraps
    if (!(current->getObjectTypeFlags() & 0x200000)) {
        // 0x236D12 `testb $0x20, 0x6(%esi)`: the bot sits in a vehicle or stationary weapon.
        BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_patternId, -1));   // -0x1B0
        // Getting out to the bot's own soldier: confirm a pending type-0 message with 6 on channel 7.
        if (m_targets[bot->getBotId()] == bot->getPlayer()->getSoldier()->getInformation(bot->getSide())
            && communication->generalConfirmReceivedMessage(static_cast<AIRadioMessageType>(0)))
            sequence->addStatement(checkPointer(new BAPASendRadioMessage(
                static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(7), 3, 1.0f, m_patternId, -1)));

        if (m_sameVehicle[bot->getBotId()]) {
            // 0x23A260: another seat of the same vehicle: press its select key, then hand over the target.
            BAPCombinerSerial* const seatChange = checkPointer(new BAPCombinerSerial(m_patternId, -1));
            const int patternId = m_patternId;
            const unsigned long long controls = 1ULL << targetUnit->getSelectKey();   // 64-bit shift (0x23A2CA)
            BAPConFalse* const stop = checkPointer(new BAPConFalse());
            BAPConTrue* const start = checkPointer(new BAPConTrue());
            seatChange->addStatement(checkPointer(new BAPATrigger(targetUnit->getSelectKey(), start, stop, controls,
                                                                  patternId, 0.0, false)));
            seatChange->addStatement(checkPointer(new BAPICChangeVehicle(m_targets[bot->getBotId()])));
            sequence->addStatement(seatChange);
            plan = sequence;
        } else if (!current->getMobile()) {
            // 0x236DEB: a stationary weapon: press enter/exit (input 10, controls 0x400).
            BAPCombinerSerial* const leave = checkPointer(new BAPCombinerSerial(m_patternId, -1));
            const int patternId = m_patternId;
            BAPConFalse* const stop = checkPointer(new BAPConFalse());
            BAPConTrue* const start = checkPointer(new BAPConTrue());
            leave->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(10), start, stop, 0x400ULL,
                                                             patternId, 0.0, false)));
            leave->addStatement(checkPointer(new BAPICChangeVehicle(m_targets[bot->getBotId()])));
            sequence->addStatement(leave);
            plan = sequence;
        } else {
            // 0x237822: drive (or fly) to the target first. -0x1A8 becomes radiusSum * 1.2 + 50.
            radiusSum = radiusSum * 1.2f + 50.0f;
            Vec2 targetXZ;
            targetXZ.x = target->getPosition().x;
            targetXZ.y = target->getPosition().z;
            BAPConPosition* arrived = checkPointer(new BAPConPosition(targetXZ, radiusSum));   // -0x26C

            bool toEntryPosition = false;                                         // -0x276 / -0x275
            if (targetUnit->useNoPathfindingToGetToObject() || !target->hasMobile())
                toEntryPosition = true;
            if (toEntryPosition && !target->isPrimary())
                toEntryPosition =
                    !informationManager->getInformation(target->getPrimaryObject(bot->getSide()))->hasMobile();

            BAPAction* move;
            if (toEntryPosition) {
                if (!(current->getObjectTypeFlags() & 0x10)) {
                    // 0x239CF0
                    BAPConAnd* const arrivedBehind = checkPointer(new BAPConAnd());
                    arrivedBehind->addCondition(arrived);
                    arrivedBehind->addCondition(
                        checkPointer(new BAPConObjectBehind(m_targets[bot->getBotId()], -0.95f, false)));
                    const int patternId = m_patternId;
                    const float arriveSpeed = mobile->getMaxSpeed() * 0.75f;
                    const float speed = mobile->getMaxSpeed();
                    move = checkPointer(new BAPAMoveToFinding(bot, m_entryPositions[bot->getBotId()], speed, arriveSpeed,
                                                              arrivedBehind, driveControls, patternId, 0.0f, 0.0f));
                } else {
                    // 0x239B40: flag 0x10 (air): 3d move to the target's position.
                    const int patternId = m_patternId;
                    const float arriveSpeed = mobile->getMaxSpeed() * 0.75f;
                    const float speed = mobile->getMaxSpeed();
                    move = checkPointer(new BAPAMoveTo3d(bot, target->getPosition(), speed, arriveSpeed, arrived,
                                                         driveControls, 25.0f, patternId));
                }
            } else if (!(current->getObjectTypeFlags() & 0x10)) {
                bool aroundAircraft = false;
                Vec3 position;
                const unsigned int targetFlags = target->getObjectTypeFlags();
                if ((targetFlags & 0x10) && !(targetFlags & 0x20)) {
                    // 0x239064: the target is an aircraft (0x10 without 0x20): a navigable point behind
                    // or beside it.
                    position = target->getPosition();
                    const int equipmentType = current->getUnit()->getEquipmentType();
                    const int vehicleNumber = current->getMobile()->getVehicleNumber();
                    aroundAircraft = getPosAroundAircraft(target, vehicleNumber, equipmentType, position);
                }
                if (aroundAircraft) {
                    // 0x2390EF: radius = the current equipment's own radius (-0x1AC); the first
                    // BAPConPosition is not freed.
                    Vec2 positionXZ;
                    positionXZ.x = position.x;
                    positionXZ.y = position.z;
                    arrived = checkPointer(new BAPConPosition(positionXZ, currentRadius));
                    const int patternId = m_patternId;
                    const float arriveSpeed = mobile->getMaxSpeed() * 0.5f;
                    const float speed = mobile->getMaxSpeed();
                    move = checkPointer(new BAPAMoveToFinding(bot, position, speed, arriveSpeed, arrived, driveControls,
                                                              patternId, 0.0f, 0.0f));
                } else {
                    // 0x23791C / 0x2393C0
                    BAPConAnd* const repath = checkPointer(new BAPConAnd());
                    repath->addCondition(
                        checkPointer(new BAPConObjectDistance(m_targets[bot->getBotId()], 20.0f, true, false, false)));
                    repath->addCondition(checkPointer(new BAPConObjectMoved(m_targets[bot->getBotId()], 10.0f)));
                    const int patternId = m_patternId;
                    const float arriveSpeed = 0.5f * mobile->getMaxSpeed();
                    const float speed = mobile->getMaxSpeed();
                    move = checkPointer(new BAPAMoveToObjectFinding(bot, m_targets[bot->getBotId()], 0.5f, speed,
                                                                    arriveSpeed, arrived, driveControls, patternId,
                                                                    repath, 0.0f, -1));
                }
            } else {
                // 0x239950: flying: 3d move to the object.
                const int patternId = m_patternId;
                const float arriveSpeed = 0.5f * mobile->getMaxSpeed();
                const float speed = mobile->getMaxSpeed();
                move = checkPointer(new BAPAMoveTo3dObject(m_targets[bot->getBotId()], 0.5f, speed, arriveSpeed, 25.0f,
                                                           arrived, driveControls, patternId));
            }
            sequence->addStatement(move);

            // 0x237B2A: then press exit/enter (input 10, controls 0x400) unless an enemy took the target.
            BAPAUpdateVehicle* const update = checkPointer(new BAPAUpdateVehicle(m_patternId));
            BAPConObjectEnemyOccupied* const enemy =
                checkPointer(new BAPConObjectEnemyOccupied(m_targets[bot->getBotId()]));
            const int patternId = m_patternId;
            BAPConTrue* const start = checkPointer(new BAPConTrue());
            BAPATrigger* const enter = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(10), start, enemy,
                                                                    0x400ULL, patternId, 0.0, false));
            // 0x237C66: getting out to the soldier from the primary seat of a ground vehicle whose health
            // (IPIPhysical vfn +0x14) is below IBotManager::getVehicleBailValue: also press input 0x2F
            // (controls bit 47).
            if ((target->getObjectTypeFlags() & 0x200000) && current->isPrimary()
                && !(current->getObjectTypeFlags() & 0x10) && !(current->getObjectTypeFlags() & 0x20)) {
                if (current->getPhysical()->getHealth() < IBotManager::instance->getVehicleBailValue()) {
                    const int bailPatternId = m_patternId;
                    BAPConFalse* const bailStop = checkPointer(new BAPConFalse());
                    BAPConTrue* const bailStart = checkPointer(new BAPConTrue());
                    sequence->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(0x2F), bailStart,
                                                                        bailStop, 0x800000000000ULL, bailPatternId, 0.0,
                                                                        false)));
                }
            }
            sequence->addStatement(checkPointer(new BAPICTurnLock(true)));
            sequence->addStatement(enter);
            sequence->addStatement(update);
            sequence->addStatement(checkPointer(new BAPICTurnLock(false)));

            BAPCombinerParallel* const withChange = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
            withChange->addStatement(checkPointer(new BAPICChangeVehicle(m_targets[bot->getBotId()])));
            withChange->addStatement(sequence);
            plan = withChange;
        }
    } else {
        // 0x23AF40: on foot, walk to the target and get in.
        BAPCombinerParallel* const approach =                                    // -0x3A8
            checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));

        // -0x3AE / -0x3AD: walk straight to the stored entry position when the target's unit wants no
        // pathfinding or the target cannot move; a seat counts as movable when its primary is.
        bool toEntryPosition = false;
        if (targetUnit->useNoPathfindingToGetToObject() || !target->hasMobile())
            toEntryPosition = true;
        if (toEntryPosition && !target->isPrimary())
            toEntryPosition = !informationManager->getInformation(target->getPrimaryObject(bot->getSide()))->hasMobile();

        BAPAction* move;
        if (toEntryPosition) {
            // 0x23D384
            BAPConObjectOccupied* const occupied = checkPointer(new BAPConObjectOccupied(m_targets[bot->getBotId()]));
            BAPConObjectEnemyOccupied* const enemy =
                checkPointer(new BAPConObjectEnemyOccupied(m_targets[bot->getBotId()]));
            BAPConOr* const taken = checkPointer(new BAPConOr());
            taken->addCondition(occupied);
            taken->addCondition(enemy);
            const int patternId = m_patternId;
            const float arriveSpeed = mobile->getMaxSpeed() * 0.75f;
            const float speed = mobile->getMaxSpeed();
            move = checkPointer(new BAPAMoveToFinding(bot, m_entryPositions[bot->getBotId()], speed, arriveSpeed, taken,
                                                      driveControls, patternId, 0.5f, 0.0f));
        } else {
            // 0x23AFEB
            BAPConObjectOccupied* const occupied = checkPointer(new BAPConObjectOccupied(m_targets[bot->getBotId()]));
            BAPConObjectEnemyOccupied* const enemy =
                checkPointer(new BAPConObjectEnemyOccupied(m_targets[bot->getBotId()]));
            BAPConOr* const taken = checkPointer(new BAPConOr());                  // -0x3C4
            taken->addCondition(occupied);
            taken->addCondition(enemy);
            BAPConAnd* const repath = checkPointer(new BAPConAnd());               // -0x3CC
            repath->addCondition(checkPointer(new BAPConObjectDistance(m_targets[bot->getBotId()], 10.0f, true, false, false)));
            repath->addCondition(checkPointer(new BAPConObjectMoved(m_targets[bot->getBotId()], 5.0f)));
            const int patternId = m_patternId;
            const float arriveSpeed = mobile->getMaxSpeed() * 0.5f;
            const float speed = mobile->getMaxSpeed();
            move = checkPointer(new BAPAMoveToObjectFinding(bot, m_targets[bot->getBotId()], 0.0f, speed, arriveSpeed,
                                                            taken, driveControls, patternId, repath, 0.0f, -1));
        }
        approach->addStatement(move);

        // 0x23B2E3: press the enter key (input 10, controls 0x400) once within reach (radius sum + 1),
        // and, for objects entered without pathfinding, from behind (dot <= -0.8).
        BAPConObjectDistance* const reach =                                      // -0x41C
            checkPointer(new BAPConObjectDistance(m_targets[bot->getBotId()], radiusSum + 1.0f, false, false, false));
        BAPCondition* enterCondition = reach;
        if (targetUnit->useNoPathfindingToGetToObject()) {
            BAPConAnd* const both = checkPointer(new BAPConAnd());                 // -0x424
            both->addCondition(reach);
            both->addCondition(checkPointer(new BAPConObjectBehind(m_targets[bot->getBotId()], -0.8f, false)));
            enterCondition = both;
        }
        BAPATrigger* const enter = checkPointer(new BAPATrigger(                  // -0x434
            static_cast<io::PlayerInputMap>(10), enterCondition, 0, 0x400ULL, m_patternId, 0.0, false));
        BAPAUpdateVehicle* const update = checkPointer(new BAPAUpdateVehicle(m_patternId));   // -0x43C
        approach->addStatement(enter);

        BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_patternId, -1));   // -0x444
        // -0x449 / -0x44A: messages of type 1 / 0 waiting (std::list not empty).
        const bool type1Waiting = !communication->getReceivedMessages(static_cast<AIRadioMessageType>(1)).empty();
        const bool type0Waiting = !communication->getReceivedMessages(static_cast<AIRadioMessageType>(0)).empty();
        // -0x450 the target's primary, -0x454 its IAIObject (read before the null test), -0x458 its bot.
        Information* const primary = informationManager->getInformation(target->getPrimaryObject(bot->getSide()));
        // shipped defect: the primary is dereferenced before its null test, and the distance
        // calculation below runs only for a null primary.
        IAIObject* const primaryObject = primary->getAIObject();
        Bot* const primaryBot = IBotManager::instance->getBot(primaryObject);
        float primaryDistanceSqr = 0.0f;
        if (!primary) {
            const Mat4& primaryTransform = primary->getTransformation();
            const Mat4& currentTransform = current->getTransformation();
            const Vec3 delta = {primaryTransform.m[12] - currentTransform.m[12],
                                primaryTransform.m[13] - currentTransform.m[13],
                                primaryTransform.m[14] - currentTransform.m[14]};
            primaryDistanceSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        }
        // 0x23B558: ask the driver for a ride when the bot walks into a manned vehicle's seat.
        if (!m_sameVehicle[bot->getBotId()] && (current->getObjectTypeFlags() & 0x200000)
            && primaryObject->getPlayerId() >= 0 && primary && primary->hasMobile() && primaryDistanceSqr < 400.0f
            && !type1Waiting && !type0Waiting) {
            BAPConObjectOccupied* const driverIn = checkPointer(new BAPConObjectOccupied(primary->getHandle()));   // -0x464
            BAPASendRadioMessage* const ask = checkPointer(new BAPASendRadioMessage(                             // -0x46C
                static_cast<AIRadioMessageType>(2), static_cast<AIRadioChannelType>(6), 3, 0.0f, m_patternId,
                primaryObject->getPlayerId()));
            BAPFlowCIf* const askIfManned = checkPointer(new BAPFlowCIf(driverIn, ask, 0, m_patternId, false, false));
            sequence->addStatement(askIfManned);
            // 0x23C453: CommunicationAgent +0x5C = the driver's player id (IAIObject +0x148) when the
            // driver is a bot, else 0xFF.
            if (primaryBot)
                communication->setInt92(primaryObject->getPlayerId());
            else
                communication->setInt92(0xFF);
        }

        // 0x23B57D: loop the approach until the target is occupied, then re-read the equipment.
        BAPFlowCWhile* const untilInside = checkPointer(new BAPFlowCWhile(                                         // -0x47C
            checkPointer(new BAPConNot(checkPointer(new BAPConObjectOccupied(m_targets[bot->getBotId()])))), approach,
            false, m_patternId));
        sequence->addStatement(untilInside);
        sequence->addStatement(update);

        BAPCombinerParallel* const withChange = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));   // -0x498
        withChange->addStatement(checkPointer(new BAPICChangeVehicle(m_targets[bot->getBotId()])));
        withChange->addStatement(sequence);

        BAPCombinerSerial* const root = checkPointer(new BAPCombinerSerial(m_patternId, -1));   // -0x4A8
        if (m_getInConfirm[bot->getBotId()]
            && communication->generalConfirmReceivedMessage(static_cast<AIRadioMessageType>(1)))
            root->addStatement(checkPointer(new BAPASendRadioMessage(
                static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(6), 2, 1.0f, m_patternId, -1)));
        root->addStatement(withChange);
        plan = root;
    }

    // Shared tail (inlined after every branch, 0x23B7B3 here): in a vehicle, run the plan beside
    // BAPAUseFlareOnMissileLock (controls bit 47); wrap it in BAPIWChange (books the target), owner = this
    // (0x2370E0).
    BAPStatement* wrapped = plan;
    if (!(current->getObjectTypeFlags() & 0x200000)) {
        BAPCombinerParallel* const flares = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
        flares->addStatement(checkPointer(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId)));
        flares->addStatement(plan);
        wrapped = flares;
    }
    const int playerId = bot->getPlayerId();
    BAPIWChange* const wrapper =
        checkPointer(new BAPIWChange(m_targets[bot->getBotId()], wrapped, m_patternId, playerId));
    wrapper->m_owner = this;
    return wrapper;
}

// ELF 0x236340: five getBotId (Bot slot +0xE8) calls, one per store: initiated (+0x04) = true,
// +0x14 = the first bool, +0x0C = handle, +0x10 = position (three floats), +0x18 = the second bool.
void BBPChange::init(unsigned int handle, const Vec3& position, bool sameVehicle, bool getInConfirm, Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_sameVehicle[bot->getBotId()] = sameVehicle;
    m_targets[bot->getBotId()] = handle;
    m_entryPositions[bot->getBotId()] = position;
    m_getInConfirm[bot->getBotId()] = getInConfirm;
}

// ELF 0x2363F0: as above without the position.
void BBPChange::init(unsigned int handle, bool sameVehicle, bool getInConfirm, Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_sameVehicle[bot->getBotId()] = sameVehicle;
    m_targets[bot->getBotId()] = handle;
    m_getInConfirm[bot->getBotId()] = getInConfirm;
}

// ELF 0x236470: +0x18[getBotId] = confirm.
void BBPChange::setGetInConfirm(bool confirm, Bot* bot)
{
    m_getInConfirm[bot->getBotId()] = confirm;
}

// ELF 0x2364A0: IAIPathfinding::findClosestValidPolygon (vfn +0x94) on map `map` from `from` towards
// `to` with 10 as the search argument and `type` last; the found point (zero-initialised) goes to `out`
// in XZ whatever the answer. The Vec3 arguments arrive by invisible reference (GCC 3.3 class by value).
bool BBPChange::findValidNavPosToAir(int map, int type, Vec3 from, Vec3 to, Vec2& out)
{
    Vec3 found;
    found.x = 0.0f;
    found.y = 0.0f;
    found.z = 0.0f;
    unsigned int island;
    unsigned int polygon;
    const bool valid = static_cast<IAIPathfinding*>(IAIPathfinding::instance)
                           ->findClosestValidPolygon(polygon, island, found, map, from, to, 10, type);
    vec3ToVec2XZ(found, out);
    return valid;
}

// ELF 0x236540: three candidate segments around the aircraft, r = its IPIPhysical radius (vfn +0x2C),
// p = transformation row 3, f = row 2, s = row 0: behind (p - 2r f -> p - 3r f), right (p + r s ->
// p + 3r s), left (p - r s -> p - 3r s). The first valid one is written to `position` in XZ (y kept).
bool BBPChange::getPosAroundAircraft(Information* aircraft, int map, int type, Vec3& position)
{
    const Mat4& m = aircraft->getTransformation();
    const float r = aircraft->getPhysical()->getRadius();
    const Vec3 p = {m.m[12], m.m[13], m.m[14]};
    Vec2 found;
    found.x = 0.0f;
    found.y = 0.0f;

    Vec3 f = {m.m[8], m.m[9], m.m[10]};
    Vec3 to = {p.x - f.x * 3.0f * r, p.y - f.y * 3.0f * r, p.z - f.z * 3.0f * r};
    Vec3 from = {p.x - f.x * 2.0f * r, p.y - f.y * 2.0f * r, p.z - f.z * 2.0f * r};
    if (!findValidNavPosToAir(map, type, from, to, found)) {
        const Vec3 s = {m.m[0], m.m[1], m.m[2]};
        to = {s.x * 3.0f * r + p.x, s.y * 3.0f * r + p.y, s.z * 3.0f * r + p.z};
        from = {p.x + s.x * r, p.y + s.y * r, p.z + s.z * r};
        if (!findValidNavPosToAir(map, type, from, to, found)) {
            const Vec3 s2 = {m.m[0], m.m[1], m.m[2]};
            to = {p.x - s2.x * 3.0f * r, p.y - s2.y * 3.0f * r, p.z - s2.z * 3.0f * r};
            from = {p.x - s2.x * r, p.y - s2.y * r, p.z - s2.z * r};
            if (!findValidNavPosToAir(map, type, from, to, found))
                return false;
        }
    }
    vec2ToVec3XZ(found, position);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
