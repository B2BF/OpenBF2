// Waypoint pattern of ground vehicles (AI/AIDLL/AI/AIBotBehaviourPattern/BBPGotoWaypoints2d.cpp in the
// image; class BBPGotoWaypoint2d). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols);
// every body carries its ELF address.
//
// static init: ELF 0x27F440 (__tcf_0 0x27F4A0, _GLOBAL__I 0x27F4D0): the <iostream> guard object only.

#include "BBPGotoWaypoints2d.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WaypointList.h"
#include "../AIBot/WPCloseTo.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPASetSensingAgentFocus.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotActionPlan/BAPIWMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Math/Vec3.h"

#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Waypoint list ids (WaypointList::getId, slot +0x10): WPCloseTo 0, WPMoveTo 1, WPPatrol 2.
const int kWaypointCloseTo = 0;
const int kWaypointMoveTo = 1;
const int kWaypointPatrol = 2;

// BAPInfoWrapper::getInfoType of a BAPIWMoveTo.
const int kInfoTypeMoveTo = 2;

void clearScript(Bot* bot)
{
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
}

void dropPlan(Bot* bot)
{
    delete bot->getPlan();
    bot->setPlan(0);
}

// The larger of the waypoint radius and the approach range; unordered gives the range
// (fcmovbe 0x27C84C).
float approachDistance(float wayPointRadius, float range)
{
    return wayPointRadius > range ? wayPointRadius : range;
}

// The serial body every arm starts with: the sensing focus (0.5) towards the goal. Every arm of
// createPlan builds it inline in this order (serial, focus, add).
BAPCombinerSerial* beginRoute(Information* equipment, const Vec3& goal, int patternId)
{
    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(patternId, -1));
    Vec3 look;
    look.x = look.y = look.z = 0.0f;
    const Vec3& position = equipment->getPosition();
    look.x = goal.x - position.x;
    look.y = goal.y - position.y;
    look.z = goal.z - position.z;
    BAPASetSensingAgentFocus* const focus = checkPointer(new BAPASetSensingAgentFocus(0.5f, look, patternId, false));
    serial->addStatement(focus);
    return serial;
}

// The common tail of every arm: parallel(use flares on missile lock, serial) inside a BAPIWMoveTo at
// the goal. The flare action is not checkPointer'd (no null test in any arm).
BotScript* finishRoute(Bot* bot, BAPCombinerSerial* serial, const Vec3& goal, int patternId, void* owner)
{
    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(patternId, true, -1, false));
    parallel->addStatement(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, patternId));
    parallel->addStatement(serial);
    BAPIWMoveTo* const root = checkPointer(new BAPIWMoveTo(goal, parallel, patternId));
    root->setParent(owner);   // +0x04 (BotScript::mpOwner)
    return root;
}

} // namespace

// ELF 0x27BC50: BBPPattern(), the vtable store (GOT -0x2FC + 8), then the argument at +0x08.
BBPGotoWaypoint2d::BBPGotoWaypoint2d(int id)
    : m_patternId(id)
{
}

// ELF 0x27F4F0 (D0 0x27F530): nothing of its own; the BBPGotoWaypoint vtable store and
// BBPPattern's destructor.
BBPGotoWaypoint2d::~BBPGotoWaypoint2d()
{
}

// ELF 0x27BCC0 (listing). Drives the vehicle along its waypoint list:
//   BAPIWMoveTo(goal, parallel { BAPAUseFlareOnMissileLock, serial { BAPASetSensingAgentFocus(0.5,
//   goal - position), BAPAMoveToFinding or BAPAMoveToObjectFinding } })
// WPPatrol moves with the full waypoint radius, WPMoveTo with half of it; WPCloseTo approaches the
// object until it moved (BAPConObjectMoved). An existing plan of this pattern is rebuilt only once
// the waypoints changed; a WPPatrol or WPCloseTo plan is then kept while its goal is farther than
// 4 * the squad member's max speed from the new goal (the reverse of the soldier pattern's test),
// and the WPPatrol test reads the x components twice; both as shipped.
BotScript* BBPGotoWaypoint2d::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        // 0x27F3FE
        clearScript(bot);
        if (bot->getPlan())
            dropPlan(bot);
        return 0;
    }

    const float maxSpeed = bot->getSquadMember()->getMaxSpeed();   // 0x27BD0F
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* const controlInfo = equipment->getControlInfo();   // 0x27BD5B, read before the plan test

    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId)
    {
        // 0x27D7A3: the current plan is ours. Keep it unless the waypoints changed.
        if (!bot->getWaypoints()->m_flag4 || bot->getWaypoints()->getWayPointError())
            return bot->getPlan();
        bot->getWaypoints()->m_flag4 = false;

        if (static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == kInfoTypeMoveTo)
        {
            WaypointList* const waypoints = bot->getWaypoints();
            const unsigned long long controls = controlInfo->getDriveControls();
            float range = equipment->getPhysical()->getRadius() * 4.0f;
            const int id = waypoints->getId();
            if (id == kWaypointMoveTo)
            {
                // 0x27EB97: always rebuilt.
                Vec3 goal;
                goal.x = goal.y = goal.z = 0.0f;
                Vec2 goal2d;
                goal2d.x = goal2d.y = 0.0f;
                if (!waypoints->getGoalPoint(goal))
                    return 0;
                vec3ToVec2XZ(goal, goal2d);
                dropPlan(bot);
                clearScript(bot);
                BAPCombinerSerial* const serial = beginRoute(equipment, goal, m_patternId);
                BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
                    bot, goal, maxSpeed, maxSpeed, 0, controls, m_patternId, waypoints->getWayPointRadius() * 0.5f,
                    0.0f));
                serial->addStatement(move);
                return finishRoute(bot, serial, goal, m_patternId, this);
            }
            if (id < kWaypointMoveTo)
            {
                if (id != kWaypointCloseTo)
                {
                    // 0x27D860
                    if (bot->getPlan())
                        dropPlan(bot);
                    return 0;
                }
                // 0x27E12D
                Vec3 target;
                target.x = target.y = target.z = 0.0f;
                Information* const targetInfo =
                    informationManager->getInformation(static_cast<WPCloseTo*>(waypoints)->getTarget());
                if (!targetInfo)
                {
                    dropPlan(bot);
                    return 0;
                }
                target = targetInfo->getPosition();
                const Vec3& current = static_cast<BAPIWMoveTo*>(bot->getPlan())->getPosition();
                Vec3 delta;
                delta.x = delta.y = delta.z = 0.0f;
                delta.x = target.x - current.x;
                delta.y = target.y - current.y;
                delta.z = target.z - current.z;
                const float keep = maxSpeed * 4.0f;
                if (delta.x * delta.x + delta.y * delta.y + delta.z * delta.z > keep * keep)   // 0x27E248
                    return bot->getPlan();
                dropPlan(bot);
                clearScript(bot);
                if (targetInfo->getPhysical())
                    range = targetInfo->getPhysical()->getRadius() + range;
                BAPCombinerSerial* const serial = beginRoute(equipment, target, m_patternId);
                // The end condition is built inside the argument list, after the action's allocation.
                BAPAMoveToObjectFinding* const move = checkPointer(new BAPAMoveToObjectFinding(
                    bot, static_cast<WPCloseTo*>(waypoints)->getTarget(), range, maxSpeed, maxSpeed, 0, controls,
                    m_patternId,
                    checkPointer(new BAPConObjectMoved(static_cast<WPCloseTo*>(waypoints)->getTarget(), range)),
                    0.0f, -1));
                serial->addStatement(move);
                return finishRoute(bot, serial, target, m_patternId, this);
            }
            if (id != kWaypointPatrol)
            {
                // 0x27D860
                if (bot->getPlan())
                    dropPlan(bot);
                return 0;
            }
            // 0x27D89B
            Vec3 goal;
            goal.x = goal.y = goal.z = 0.0f;
            Vec2 goal2d;
            goal2d.x = goal2d.y = 0.0f;
            if (!waypoints->getGoalPoint(goal))
                return 0;
            vec3ToVec2XZ(goal, goal2d);
            const float dx = static_cast<BAPIWMoveTo*>(bot->getPlan())->getPosition().x - goal2d.x;
            const float dx2 = static_cast<BAPIWMoveTo*>(bot->getPlan())->getPosition().x - goal2d.x;   // sic: x again
            const float keep = 4.0f * maxSpeed;
            if (dx * dx + dx2 * dx2 > keep * keep)   // 0x27D960
                return bot->getPlan();
            dropPlan(bot);
            clearScript(bot);
            BAPCombinerSerial* const serial = beginRoute(equipment, goal, m_patternId);
            BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
                bot, goal, maxSpeed, maxSpeed, 0, controls, m_patternId, waypoints->getWayPointRadius(), 0.0f));
            serial->addStatement(move);
            return finishRoute(bot, serial, goal, m_patternId, this);
        }
        // 0x27D778: another wrapper; build a new plan.
        dropPlan(bot);
    }
    else if (bot->getPlan())
    {
        dropPlan(bot);
    }

    // 0x27BD94
    clearScript(bot);
    if (bot->getWaypoints()->getWayPointError())
        return 0;
    WaypointList* const waypoints = bot->getWaypoints();
    const unsigned long long controls = controlInfo->getDriveControls();
    float range = equipment->getPhysical()->getRadius() * 4.0f;
    bot->getWaypoints()->m_flag4 = false;   // 0x27BE2A
    bot->getSquadMember()->clearRequestAction();

    const int id = waypoints->getId();
    if (id == kWaypointMoveTo)
    {
        // 0x27CF80: the goal point is asked for twice, as compiled.
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Vec2 goal2d;
        goal2d.x = goal2d.y = 0.0f;
        if (!waypoints->getGoalPoint(goal))
            return 0;
        if (!waypoints->getGoalPoint(goal))
            return 0;
        BAPCombinerSerial* const serial = beginRoute(equipment, goal, m_patternId);
        vec3ToVec2XZ(goal, goal2d);
        BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
            bot, goal, maxSpeed, maxSpeed, 0, controls, m_patternId, waypoints->getWayPointRadius() * 0.5f, 0.0f));
        serial->addStatement(move);
        return finishRoute(bot, serial, goal, m_patternId, this);
    }
    if (id < kWaypointMoveTo)
    {
        if (id != kWaypointCloseTo)
            return 0;
        // 0x27C62D
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Information* const targetInfo =
            informationManager->getInformation(static_cast<WPCloseTo*>(waypoints)->getTarget());
        if (!targetInfo)
            return 0;
        goal = targetInfo->getPosition();
        if (targetInfo->getPhysical())
            range = targetInfo->getPhysical()->getRadius() + range;
        BAPCombinerSerial* const serial = beginRoute(equipment, goal, m_patternId);
        // The end condition is built inside the argument list, after the action's allocation.
        BAPAMoveToObjectFinding* const move = checkPointer(new BAPAMoveToObjectFinding(
            bot, static_cast<WPCloseTo*>(waypoints)->getTarget(),
            approachDistance(waypoints->getWayPointRadius(), range), maxSpeed, maxSpeed, 0, controls, m_patternId,
            checkPointer(new BAPConObjectMoved(static_cast<WPCloseTo*>(waypoints)->getTarget(), range)), 0.0f, -1));
        serial->addStatement(move);
        return finishRoute(bot, serial, goal, m_patternId, this);
    }
    if (id != kWaypointPatrol)
        return 0;
    // 0x27BE72
    Vec3 goal;
    goal.x = goal.y = goal.z = 0.0f;
    Vec2 goal2d;
    goal2d.x = goal2d.y = 0.0f;
    if (!waypoints->getGoalPoint(goal))
        return 0;
    BAPCombinerSerial* const serial = beginRoute(equipment, goal, m_patternId);
    vec3ToVec2XZ(goal, goal2d);
    BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
        bot, goal, maxSpeed, maxSpeed, 0, controls, m_patternId, waypoints->getWayPointRadius(), 0.0f));
    serial->addStatement(move);
    return finishRoute(bot, serial, goal, m_patternId, this);
}

// ELF 0x27BC90: marks the bot's slot of the initiated array (+0x04).
void BBPGotoWaypoint2d::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
