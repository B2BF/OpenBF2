// Waypoint-following pattern of boats (AI/AIDLL/AI/AIBotBehaviourPattern/BBPGotoWaypointsBoat.cpp in
// the image). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its
// ELF address. createPlan was checked against the listing for constructor arguments (bools and
// the 64-bit masks), allocation order and the waypoint fields each arm reads.

#include "BBPGotoWaypointsBoat.h"

#include "../../../../IO/Input/Input.h"
#include "../../../../Math/Vec3.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WPBeachLanding.h"
#include "../AIBot/WPCloseTo.h"
#include "../AIBot/WaypointList.h"
#include "../AIBotActionPlan/BAPALookAhead.h"
#include "../AIBotActionPlan/BAPAMoveToDirect.h"
#include "../AIBotActionPlan/BAPAMoveToFinding.h"
#include "../AIBotActionPlan/BAPAMoveToObjectFinding.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPATriggerContinously.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConObjectMoved.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWMoveTo.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIStrategicEntity/AILandingZone.h"
#include "../AIUtils/checkPointer.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x284510, __tcf_0 0x284570, _GLOBAL__I 0x2845A0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [CHOSEN, not recovered] packaging: the image repeats these blocks inline in each arm.

void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

void clearScript(Bot* bot)
{
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
}

// The look-ahead built when the vehicle looks with another control than it steers with: 5 degrees
// per update (0x3DB2B8C2), a never-ending condition, the look controls.
BAPALookAhead* newLookAhead(Bot* bot, Information* self, int patternId)
{
    return checkPointer(new BAPALookAhead(bot, 0.08726646f, checkPointer(new BAPConFalse()),
                                          self->getControlInfo()->getLookControls(), patternId));
}

} // namespace

// ELF 0x280DB0 (C1; C2 0x280D70): BBPPattern(), the vtable store (GOT -0xC5C + 8), then the argument
// at +0x08.
BBPGotoWaypointBoat::BBPGotoWaypointBoat(int id)
    : m_patternId(id)
{
}

// ELF 0x280E30 (D1; D2 0x280DF0, D0 0x280E70): nothing of its own; the BBPGotoWaypoint vtable store
// and BBPPattern's destructor.
BBPGotoWaypointBoat::~BBPGotoWaypointBoat()
{
}

// ELF 0x280EE0. An uninitiated pattern clears the debugger and the context, drops the plan and
// builds nothing. A plan of this pattern is kept unless the waypoints were renewed (their +4 flag,
// cleared here); otherwise the plan is dropped, and a waypoint error builds nothing. The plan is a
// BAPIWMoveTo over the waypoint list's goal:
//   0 WPCloseTo: object-finding move to the target, ended when the target moved by the radius;
//   1 WPMoveTo: finding move to the goal point;
//   2 WPPatrol: serial(parallel(finding move ended at the goal in XZ), reset controls);
//   3/4 beach landing: outside the zone as 1; inside, serial(while(true, parallel(direct move,
//       pitch held continuously))) towards the zone's box centre.
// Every move except 0 gets a parallel look-ahead when the look control differs from the drive turn
// control. The arrival radius is four times the own physical radius (plus the target's in arm 0);
// the speed is the list's +8 value, or the vehicle's maximum when it is negative. The equipment
// information is not null-checked; any other list id returns null without clearing.
BotScript* BBPGotoWaypointBoat::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()])
    {
        clearScript(bot);
        dropPlan(bot);
        return 0;
    }

    Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* const controlInfo = self->getControlInfo();
    const bool lookSeparately = controlInfo->getLookHorizontalControl() != controlInfo->getDriveTurnControl();
    if (bot->hasPlan() && static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId
        && !bot->getWaypoints()->m_flag4)
        return bot->getPlan();

    bot->getWaypoints()->m_flag4 = false;
    dropPlan(bot);
    clearScript(bot);
    if (bot->getWaypoints()->getWayPointError())
        return 0;

    IPIMobile* const mobile = self->getMobile();
    WaypointList* const waypoints = bot->getWaypoints();
    const unsigned long long controls = self->getControlInfo()->getDriveControls();
    float radius = self->getPhysical()->getRadius() * 4.0f;
    const float maxSpeed = mobile->getMaxSpeed();
    const float waypointSpeed = bot->getWaypoints()->m_float8;
    const float speed = waypointSpeed < 0.0f ? maxSpeed : waypointSpeed;   // NaN keeps the list's value
    bot->getSquadMember()->clearRequestAction();

    switch (waypoints->getId())
    {
    case 0:
    {
        WPCloseTo* const closeTo = static_cast<WPCloseTo*>(waypoints);
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Information* const target = informationManager->getInformation(closeTo->m_target);
        if (!target)
            return 0;
        goal = target->getPosition();
        if (target->getPhysical())
            radius = target->getPhysical()->getRadius() + radius;
        BAPConFalse* const never = checkPointer(new BAPConFalse());
        const float listRadius = waypoints->getWayPointRadius();
        const float distance = listRadius > radius ? listRadius : radius;   // unordered: radius
        BAPAMoveToObjectFinding* const move = checkPointer(new BAPAMoveToObjectFinding(
            bot, closeTo->m_target, distance, speed, speed, never, controls, m_patternId,
            checkPointer(new BAPConObjectMoved(closeTo->m_target, radius)), 0.0f, -1));
        BAPIWMoveTo* const plan = checkPointer(new BAPIWMoveTo(goal, move, m_patternId));
        plan->setParent(this);
        return plan;
    }
    case 1:
    {
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Vec2 goal2d;
        goal2d.x = goal2d.y = 0.0f;
        BAPALookAhead* look = 0;
        if (!waypoints->getGoalPoint(goal))
            return 0;
        vec3ToVec2XZ(goal, goal2d);   // not read in this arm
        if (lookSeparately)
            look = newLookAhead(bot, self, m_patternId);
        BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
            bot, goal, speed, speed, 0, controls, m_patternId, waypoints->getWayPointRadius(), 0.0f));
        BAPIWMoveTo* plan;
        if (lookSeparately)
        {
            BAPCombinerParallel* const both = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
            both->addStatement(move);
            both->addStatement(look);
            plan = checkPointer(new BAPIWMoveTo(goal, both, m_patternId));
        }
        else
            plan = checkPointer(new BAPIWMoveTo(goal, move, m_patternId));
        plan->setParent(this);
        return plan;
    }
    case 2:
    {
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Vec2 goal2d;
        goal2d.x = goal2d.y = 0.0f;
        if (!waypoints->getGoalPoint(goal))
            return 0;
        vec3ToVec2XZ(goal, goal2d);
        BAPCombinerSerial* const steps = checkPointer(new BAPCombinerSerial(m_patternId, -1));
        BAPCombinerParallel* const both = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
        // The list radius is read once for the move's distance and once more for the arrival test.
        const float listRadius = waypoints->getWayPointRadius();
        const float arrivalListRadius = waypoints->getWayPointRadius();
        const float arrival = arrivalListRadius > radius ? arrivalListRadius : radius;   // unordered: radius
        BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
            bot, goal, speed, speed, checkPointer(new BAPConPosition(goal2d, arrival)), controls, m_patternId,
            listRadius, 0.0f));
        both->addStatement(move);
        if (lookSeparately)
            both->addStatement(newLookAhead(bot, self, m_patternId));
        steps->addStatement(both);
        steps->addStatement(checkPointer(new BAPAResetControls(true, true, true, m_patternId)));
        BAPIWMoveTo* const plan = checkPointer(new BAPIWMoveTo(goal, steps, m_patternId));
        plan->setParent(this);
        return plan;
    }
    case 3:
    case 4:
    {
        Vec3 goal;
        goal.x = goal.y = goal.z = 0.0f;
        Vec2 goal2d;
        goal2d.x = goal2d.y = 0.0f;
        BAPALookAhead* look = 0;
        // The goal point is asked twice; the second answer is the one used.
        if (!waypoints->getGoalPoint(goal))
            return 0;
        if (!waypoints->getGoalPoint(goal))
            return 0;
        vec3ToVec2XZ(goal, goal2d);   // not read in this arm
        WPBeachLanding* const landing = static_cast<WPBeachLanding*>(waypoints);
        if (landing->insideZone())
        {
            BAPCombinerSerial* const steps = checkPointer(new BAPCombinerSerial(m_patternId, -1));
            BAPCombinerParallel* const both = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
            both->addStatement(checkPointer(new BAPAMoveToDirect(bot, goal, speed, speed,
                                                                 checkPointer(new BAPConFalse()), controls,
                                                                 m_patternId)));
            if (lookSeparately)
                both->addStatement(newLookAhead(bot, self, m_patternId));
            // Pitch (input 1, mask bit 1) held for as long as the loop runs.
            both->addStatement(checkPointer(new BAPATriggerContinously(
                io::PIPitch, checkPointer(new BAPConTrue()), checkPointer(new BAPConFalse()),
                checkPointer(new BAPConFalse()), 2ULL, m_patternId, 0.0)));
            steps->addStatement(checkPointer(new BAPFlowCWhile(checkPointer(new BAPConTrue()), both, false,
                                                               m_patternId)));

            Vec3 centre3d;
            centre3d.x = centre3d.y = centre3d.z = 0.0f;
            Vec2 centre2d;
            centre2d.x = centre2d.y = 0.0f;
            Vec2 centre;
            centre.x = centre.y = 0.0f;
            getBoxCentre(landing->m_zone->getMinCorner(), centre, landing->m_zone->getMaxCorner());
            centre2d = centre;
            vec2ToVec3XZ(centre2d, centre3d);
            BAPIWMoveTo* const plan = checkPointer(new BAPIWMoveTo(centre3d, steps, m_patternId));
            plan->setParent(this);
            return plan;
        }

        if (lookSeparately)
            look = newLookAhead(bot, self, m_patternId);
        BAPAMoveToFinding* const move = checkPointer(new BAPAMoveToFinding(
            bot, goal, speed, speed, 0, controls, m_patternId, waypoints->getWayPointRadius(), 0.0f));
        BAPIWMoveTo* plan;
        if (lookSeparately)
        {
            BAPCombinerParallel* const both = checkPointer(new BAPCombinerParallel(m_patternId, false, -1, false));
            both->addStatement(move);
            both->addStatement(look);
            plan = checkPointer(new BAPIWMoveTo(goal, both, m_patternId));
        }
        else
            plan = checkPointer(new BAPIWMoveTo(goal, move, m_patternId));
        plan->setParent(this);
        return plan;
    }
    default:
        return 0;
    }
}

// ELF 0x280EB0
void BBPGotoWaypointBoat::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
