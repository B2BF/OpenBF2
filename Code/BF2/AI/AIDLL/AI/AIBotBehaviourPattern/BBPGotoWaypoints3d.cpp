// Waypoint-following pattern of aircraft (AI/AIDLL/AI/AIBotBehaviourPattern/BBPGotoWaypoints3d.cpp in
// the image). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its
// ELF address. createPlan was checked against the listing for constructor arguments and the
// waypoint fields each arm reads.

// static init: ELF 0x280C40 (__tcf_0 0x280CA0, _GLOBAL__I 0x280CD0): <iostream>'s Init object only.
#include <iostream>

#include "BBPGotoWaypoints3d.h"

#include "../../../../Math/Vec3.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WPAltitudeCloseTo.h"
#include "../AIBot/WPAltitudeMoveTo.h"
#include "../AIBot/WPAltitudePatrol.h"
#include "../AIBot/WaypointList.h"
#include "../AIBotActionPlan/BAPAMoveTo3d.h"
#include "../AIBotActionPlan/BAPAMoveTo3dObject.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConPosition.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

void dropPlan(Bot* bot)
{
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
}

// `parallel(reset controls, flares on missile lock)`, the head every arm of createPlan builds.
BAPCombinerParallel* newFlightPlan(Bot* bot, int patternId)
{
    BAPCombinerParallel* const plan = checkPointer(new BAPCombinerParallel(patternId, true, -1, false));
    plan->addStatement(checkPointer(new BAPAResetControls(true, true, true, patternId)));
    // No checkPointer on this allocation in the image.
    plan->addStatement(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, patternId));
    return plan;
}

} // namespace

// ELF 0x27F630 (C2 0x27F5F0): BBPPattern(), the vtable store, then the argument at +0x08.
BBPGotoWaypoint3d::BBPGotoWaypoint3d(int id)
    : m_patternId(id)
{
}

// ELF 0x280CF0 (weak; D0 0x280D30): the BBPGotoWaypoint vtable store and BBPPattern's destructor.
BBPGotoWaypoint3d::~BBPGotoWaypoint3d()
{
}

// ELF 0x27F6A0. Keeps a plan of this pattern unless the waypoints were renewed (their +4 flag,
// cleared here, drops the plan); a plan of another pattern that hasPlan reports is not deleted,
// as compiled. Builds `parallel(reset controls, flares on missile lock, move)` for the flight
// waypoint lists: WPAltitudeCloseTo (5) follows the target object, WPAltitudeMoveTo (6) and
// WPAltitudePatrol (7) fly to the goal point, each with the list's distance argument. The speed is
// the list's +8 value, or the vehicle's maximum when it is negative; the arrival radius is four
// times the own physical radius (plus the target's, or 2 m for an equipment with type flag 0x20
// on a WPAltitudeMoveTo). Any other list, like an uninitiated pattern, clears the debugger, the
// context and the plan and builds nothing. The equipment information is not null-checked.
BotScript* BBPGotoWaypoint3d::createPlan(Bot* bot)
{
    if (m_initiated[bot->getBotId()])
    {
        Information* const self = informationManager->getInformation(bot->getCurrentEquipment());
        if (bot->hasPlan())
        {
            if (bot->getWaypoints()->m_flag4)
            {
                bot->getWaypoints()->m_flag4 = false;
                dropPlan(bot);
            }
            else if (static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId)
                return bot->getPlan();
        }
        else
            dropPlan(bot);
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();

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
        case 5:
        {
            WPAltitudeCloseTo* const closeTo = static_cast<WPAltitudeCloseTo*>(waypoints);
            Information* const target = informationManager->getInformation(closeTo->m_target);
            if (!target)
                return 0;
            BAPCombinerParallel* const plan = newFlightPlan(bot, m_patternId);
            if (target->getPhysical())
                radius = target->getPhysical()->getRadius() + radius;
            plan->addStatement(checkPointer(new BAPAMoveTo3dObject(closeTo->m_target, radius, speed, speed,
                                                                   closeTo->m_third, checkPointer(new BAPConFalse()),
                                                                   controls, m_patternId)));
            plan->setParent(this);   // +0x04 (BotScript::mpOwner)
            return plan;
        }
        case 6:
        {
            Vec3 goal;
            goal.x = goal.y = goal.z = 0.0f;
            if (!waypoints->getGoalPoint(goal))
                return 0;
            BAPCombinerParallel* const plan = newFlightPlan(bot, m_patternId);
            BAPConPosition* const reached = self->isOfType(ITHelicopter)
                ? checkPointer(new BAPConPosition(goal, 2.0f))
                : checkPointer(new BAPConPosition(goal, radius));
            plan->addStatement(checkPointer(new BAPAMoveTo3d(bot, goal, speed, speed, reached, controls,
                                                             static_cast<WPAltitudeMoveTo*>(waypoints)->m_third,
                                                             m_patternId)));
            plan->setParent(this);
            return plan;
        }
        case 7:
        {
            Vec3 goal;
            goal.x = goal.y = goal.z = 0.0f;
            if (!waypoints->getGoalPoint(goal))
                return 0;
            BAPCombinerParallel* const plan = newFlightPlan(bot, m_patternId);
            BAPConPosition* const reached = checkPointer(new BAPConPosition(goal, radius));
            plan->addStatement(checkPointer(new BAPAMoveTo3d(bot, goal, speed, speed, reached, controls,
                                                             static_cast<WPAltitudePatrol*>(waypoints)->m_third,
                                                             m_patternId)));
            plan->setParent(this);
            return plan;
        }
        default:
            break;
        }
    }

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    dropPlan(bot);
    return 0;
}

// ELF 0x27F670
void BBPGotoWaypoint3d::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
