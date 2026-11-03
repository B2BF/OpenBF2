// EntryCarMoveTo (attested TU `EntryCarMoveTo.cpp`, split out of BAPEntries.cpp by S0).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32): the interpreter entry that drives a car
// along a BAPAMoveTo. execute's flags follow the BotScript convention: `ok` (the step ran) and
// `done` (the statement is finished; with ok == false it failed).
//
// static init: ELF 0x2B43A0 (__tcf_0 0x2B4400, _GLOBAL__I 0x2B4430): the <iostream> guard object only.

#include "EntryCarMoveTo.h"

#include "CarControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

EntryCarMoveTo::EntryCarMoveTo()
{
}

EntryCarMoveTo::~EntryCarMoveTo()
{
}

int EntryCarMoveTo::getTargetId()
{
    return 7;
}

// ELF 0x2B3C40 (listing; Ghidra cross-check). Every second at most it asks the move whether it is
// finished; otherwise it updates the move and steers towards the returned point:
// CarControl::actionStatusDecision chooses the action (a first output of 666 means stop: reset the
// controls and finish), then controlTowardsDirection drives at the mobile's max speed scaled by
// the slower of a turn factor (1 - min(1, 1.8 * max(0, |largest angle diff| - startAngle)) *
// speedScale, while faster than 1 m/s) and an approach factor ((distance - smallest radius - 0.5)^2
// / 225, at least 0.2, inside 15 m). Arrival, a finished move and a failed update reset the
// controls with the speed along the facing.
bool EntryCarMoveTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    static float startAngle = 0.2f;   // .data 0x4BCD64
    static float speedScale = 0.8f;   // .data 0x4BCD68

    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    ok = true;
    done = false;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
    if (!object->hasPhysicsEnabled())
        object->enablePhysics(info->getSide());
    IPIMobile* const mobile = info->getMobile();
    IPIControlInfo* const controlInfo = info->getControlInfo();
    IPIPhysical* const physical = info->getPhysical();
    if (!mobile || !controlInfo || !physical)
    {
        done = true;
        ok = false;
        return true;
    }

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double last = static_cast<BotMain*>(bot)->getLastCheckMoveToFinishedTime();   // Bot +0x274 (pure in Bot, declared on BotMain in this tree)
    done = false;
    if (now - last > 1.0)
    {
        static_cast<BotMain*>(bot)->setLastCheckMoveToFinishedTime(now);   // +0x270
        if (moveTo->isFinished(bot))
        {
            done = true;
            ok = true;
            return true;
        }
    }

    Vec2 target;
    target.x = target.y = 0.0f;
    bool arrived;
    ok = moveTo->update(target, done, arrived, 0.0f) != 0;
    if (!ok)
    {
        // 0x2B42CD
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = CarControl::resetControls(bot, bot->getInput(), true, false, false,
                                       speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        done = true;
        return true;
    }
    if (arrived || done)
    {
        // 0x2B3DF3
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = CarControl::resetControls(bot, bot->getInput(), true, false, false,
                                       speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        return true;
    }

    // 0x2B3E6E
    Vec2 position;
    position.x = info->getPosition().x;
    position.y = info->getPosition().z;
    Vec2 forward;   // read, never used
    forward.x = info->getTransformation().m[8];
    forward.y = info->getTransformation().m[10];
    (void)forward;
    Vec2 toTarget;
    toTarget.x = target.x - position.x;
    toTarget.y = target.y - position.y;
    Vec2 direction = toTarget;
    float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    if (distance > 1e-06f)
    {
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = inverse * direction.y;
    }
    float turnFactor = 1.0f;
    // A -0.0f store to an otherwise unread local follows here (0x2B3F81).
    float largestAngle;
    if (std::fabs(mobile->getSpeedMagnitude()) > 1.0f && moveTo->getLargestAngleDiff(largestAngle))
    {
        // 0x2B4266
        float excess = 0.0f;
        if (0.0f <= std::fabs(largestAngle) - startAngle)
            excess = std::fabs(largestAngle) - startAngle;
        float scaled = excess * 1.8f;
        if (1.0f < excess * 1.8f)
            scaled = 1.0f;
        turnFactor = 1.0f - scaled * speedScale;
    }

    float mode;
    float angle;
    float reference;
    float limit;
    ok = CarControl::actionStatusDecision(bot, info, direction, distance, moveTo, mode, angle, reference, limit) != 0;
    if (mode == 666.0f)
    {
        // 0x2B41C2
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = CarControl::resetControls(bot, bot->getInput(), true, false, false,
                                       speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        done = true;
        return true;
    }
    if (!ok)
        return true;

    // 0x2B4012
    const float maxSpeed = mobile->getMaxSpeed();
    float approachFactor = 1.0f;
    Vec2 speed2d;   // computed, never read
    speed2d.x = speed2d.y = 0.0f;
    vec3ToVec2XZ(mobile->getSpeed(), speed2d);
    Vec2 remaining;
    remaining.x = target.x - position.x;
    remaining.y = target.y - position.y;
    const float remainingDistance = std::sqrt(remaining.x * remaining.x + remaining.y * remaining.y);
    float gap = remainingDistance - physical->getSmallestRadius() - 0.5f;
    if (gap <= 0.0f)
        gap = 0.0f;
    if (gap * gap < 225.0f)
    {
        float scale = gap * gap / 225.0f;
        if (scale <= 0.2f)
            scale = 0.2f;
        approachFactor = scale;
    }
    if (turnFactor <= approachFactor)
        approachFactor = turnFactor;
    ok = CarControl::controlTowardsDirection(info, controlInfo, mobile, physical, bot->getInput(),
                                             approachFactor * maxSpeed, mode, angle, reference, limit) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
