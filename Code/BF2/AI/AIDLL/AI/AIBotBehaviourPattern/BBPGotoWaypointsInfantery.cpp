// Waypoint pattern of infantry (AI/AIDLL/AI/AIBotBehaviourPattern/BBPGotoWaypointsInfantery.cpp in
// the image; class BBPGotoWaypointSoldier). Recovered from libaidll.so (Linux 1.5.3153, ia-32,
// symbols); every body carries its ELF address. Package D.

// static init: ELF 0x287410 (__tcf_0 0x287470, _GLOBAL__I 0x2874A0): <iostream>'s Init object only.
#include <iostream>

#include "BBPGotoWaypointsInfantery.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WaypointList.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPASetSensingAgentFocus.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"
#include "../AIBotActionPlan/BAPIWMoveTo.h"
#include "../AIBotBehaviour/BBPendingCalls.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../../../../Math/Vec3.h"

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

} // namespace

// ELF 0x284600
BBPGotoWaypointSoldier::BBPGotoWaypointSoldier(int id)
    : m_patternId(id)
{
}

// ELF 0x284680
BBPGotoWaypointSoldier::~BBPGotoWaypointSoldier()
{
}

// ELF 0x284730. Moves the soldier along its waypoint list: for a point goal (WPMoveTo, WPPatrol)
//   BAPIWMoveTo(goal, serial { BAPASetSensingAgentFocus(0.5, goal - position), BAPAMoveToFinding })
// and for an object goal (WPCloseTo) the same with BAPAMoveToObjectFinding, which ends when the
// object moved (BAPConObjectMoved). A current plan of this pattern is kept while its goal is
// within 4 * the squad member's max speed of the new goal (the patrol and object cases; a WPMoveTo
// plan is always rebuilt once the list changed).
BotScript* BBPGotoWaypointSoldier::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        clearScript(bot);
        if (bot->getPlan())
            dropPlan(bot);
        return 0;
    }

    const float maxSpeed = bot->getSquadMember()->getMaxSpeed();   // 0x28477F
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());

    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId)
    {
        // 0x285CA3: the current plan is ours. Keep it unless the waypoints changed.
        if (!bot->getWaypoints()->m_flag4 || bot->getWaypoints()->getWayPointError())
            return bot->getPlan();
        bot->getWaypoints()->m_flag4 = false;

        if (static_cast<BAPInfoWrapper*>(bot->getPlan())->getInfoType() == kInfoTypeMoveTo)
        {
            WaypointList* const waypoints = bot->getWaypoints();
            const unsigned long long controls =
                equipment->getControlInfo()->getDriveControls();
            float range = equipment->getPhysical()->getRadius() * 4.0f;
            const int id = waypoints->getId();
            if (id == kWaypointPatrol || id == kWaypointMoveTo)
            {
                Vec3 goal;
                goal.x = goal.y = goal.z = 0.0f;
                Vec2 goal2d;
                goal2d.x = goal2d.y = 0.0f;
                if (!waypoints->getGoalPoint(goal))
                    return 0;
                vec3ToVec2XZ(goal, goal2d);
                if (id == kWaypointPatrol)
                {
                    // 0x285E21: keep the plan while its goal is close (XZ) to the new one.
                    const Vec3& current = static_cast<BAPIWMoveTo*>(bot->getPlan())->getPosition();
                    const float dx = current.x - goal.x;
                    const float dz = current.z - goal.z;
                    const float keep = 4.0f * maxSpeed;
                    if (keep * keep >= dx * dx + dz * dz)
                        return bot->getPlan();
                }
                dropPlan(bot);
                clearScript(bot);
                return buildMoveTo(bot, equipment, goal, maxSpeed, controls, waypoints->getWayPointRadius());
            }
            if (id == kWaypointCloseTo)
            {
                Vec3 goal;
                goal.x = goal.y = goal.z = 0.0f;
                const unsigned int target = bbpending::closeToTarget(waypoints);
                Information* const targetInfo = informationManager->getInformation(target);
                if (!targetInfo)
                {
                    dropPlan(bot);
                    return 0;
                }
                goal = targetInfo->getPosition();
                // 0x2864EB: keep the plan while its goal is close (3d) to the object.
                const Vec3& current = static_cast<BAPIWMoveTo*>(bot->getPlan())->getPosition();
                Vec3 delta;
                delta.x = goal.x - current.x;
                delta.y = goal.y - current.y;
                delta.z = goal.z - current.z;
                const float keep = maxSpeed * 4.0f;
                if (keep * keep >= delta.x * delta.x + delta.y * delta.y + delta.z * delta.z)
                    return bot->getPlan();
                dropPlan(bot);
                clearScript(bot);
                if (targetInfo->getPhysical())
                    range = targetInfo->getPhysical()->getRadius() + range;
                return buildMoveToObject(bot, equipment, waypoints, target, goal, range, maxSpeed, controls);
            }
            // 0x285D70
            if (bot->getPlan())
                dropPlan(bot);
            return 0;
        }
        // 0x285C78: another wrapper; build a new plan.
        if (bot->getPlan())
            dropPlan(bot);
    }
    else if (bot->getPlan())
    {
        dropPlan(bot);
    }

    // 0x2847F8
    clearScript(bot);
    if (bot->getWaypoints()->getWayPointError())
        return 0;
    WaypointList* const waypoints = bot->getWaypoints();
    const unsigned long long controls = equipment->getControlInfo()->getDriveControls();   // 0x284848 +0x30, 0x28484E
    float range = equipment->getPhysical()->getRadius() * 4.0f;
    bot->getSquadMember()->clearRequestAction();   // 0x28488A

    const int id = waypoints->getId();
    if (id == kWaypointPatrol || id == kWaypointMoveTo)
    {
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Vec2 goal2d;
        goal2d.x = goal2d.y = 0.0f;
        if (!waypoints->getGoalPoint(goal))
            return 0;
        vec3ToVec2XZ(goal, goal2d);
        return buildMoveTo(bot, equipment, goal, maxSpeed, controls, waypoints->getWayPointRadius());
    }
    if (id == kWaypointCloseTo)
    {
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        const unsigned int target = bbpending::closeToTarget(waypoints);
        Information* const targetInfo = informationManager->getInformation(target);
        if (!targetInfo)
            return 0;
        goal = targetInfo->getPosition();
        if (targetInfo->getPhysical())
            range = targetInfo->getPhysical()->getRadius() + range;
        return buildMoveToObject(bot, equipment, waypoints, target, goal, range, maxSpeed, controls);
    }
    return 0;
}

// The point-goal plan, built identically by the WPMoveTo and WPPatrol arms (0x2848C0, 0x285660,
// 0x285DAB, 0x286D45).
BotScript* BBPGotoWaypointSoldier::buildMoveTo(Bot* bot, Information* equipment, const Vec3& goal, float maxSpeed,
                                               unsigned long long controls, float wayPointRadius)
{
    BAPAMoveToFinding* const move = checkPointer(
        new BAPAMoveToFinding(bot, goal, maxSpeed, maxSpeed, 0, controls, m_patternId, wayPointRadius, 0.0f));
    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    Vec3 look;
    const Vec3& position = equipment->getPosition();
    look.x = goal.x - position.x;
    look.y = goal.y - position.y;
    look.z = goal.z - position.z;
    BAPASetSensingAgentFocus* const focus =
        checkPointer(new BAPASetSensingAgentFocus(0.5f, look, m_patternId, false));
    serial->addStatement(focus);
    serial->addStatement(move);
    BAPIWMoveTo* const root = checkPointer(new BAPIWMoveTo(goal, serial, m_patternId));
    root->setParent(this);   // +0x04 (BotScript::m_owner)
    return root;
}

// The object-goal plan of the WPCloseTo arm (0x284EC0, 0x286470): the approach distance is the
// larger of the waypoint radius and `range` (4 * own radius + the object's radius).
BotScript* BBPGotoWaypointSoldier::buildMoveToObject(Bot* bot, Information* equipment, WaypointList* waypoints,
                                                     unsigned int target, const Vec3& goal, float range,
                                                     float maxSpeed, unsigned long long controls)
{
    BAPConObjectMoved* const moved = checkPointer(new BAPConObjectMoved(target, range));
    const float wayPointRadius = waypoints->getWayPointRadius();
    const float distance = !(wayPointRadius > range) ? range : wayPointRadius;   // fcmovbe 0x284FEA (unordered: range)
    BAPAMoveToObjectFinding* const move = checkPointer(new BAPAMoveToObjectFinding(
        bot, target, distance, maxSpeed, maxSpeed, 0, controls, m_patternId, moved, 0.0f, -1));
    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    Vec3 look;
    const Vec3& position = equipment->getPosition();
    look.x = goal.x - position.x;
    look.y = goal.y - position.y;
    look.z = goal.z - position.z;
    BAPASetSensingAgentFocus* const focus =
        checkPointer(new BAPASetSensingAgentFocus(0.5f, look, m_patternId, false));
    serial->addStatement(focus);
    serial->addStatement(move);
    BAPIWMoveTo* const root = checkPointer(new BAPIWMoveTo(goal, serial, m_patternId));
    root->setParent(this);   // +0x04 (BotScript::m_owner)
    return root;
}

// ELF 0x284700
void BBPGotoWaypointSoldier::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
