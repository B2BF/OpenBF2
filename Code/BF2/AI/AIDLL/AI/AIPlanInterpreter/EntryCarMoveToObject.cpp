// EntryCarMoveToObject (attested TU `EntryCarMoveToObject.cpp`, split out of BAPEntries.cpp by S0).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32): the interpreter entry that drives a car
// to an object (a BAPAMoveTo with a target object). execute's flags follow the BotScript convention: `ok` (the step ran) and
// `done` (the statement is finished; with ok == false it failed).
//
// static init: ELF 0x2B4E50 (__tcf_0 0x2B4EB0, _GLOBAL__I 0x2B4EE0): the <iostream> guard object only.

#include "EntryCarMoveToObject.h"

#include "CarControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

EntryCarMoveToObject::EntryCarMoveToObject()
{
}

EntryCarMoveToObject::~EntryCarMoveToObject()
{
}

int EntryCarMoveToObject::getTargetId()
{
    return 9;
}

// ELF 0x2B46C0 (Ghidra; listing for the statics). EntryCarMoveTo's steering for a move to an
// object: fails (done, not ok) without the target's information, the equipment or its mobile or
// physical part (the control part is not tested); asks isFinished on every call (no one-second
// throttle); the direction is normalised above 1e-05.
bool EntryCarMoveToObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    static float startAngle = 0.2f;   // .data 0x4BCE24
    static float speedScale = 0.8f;   // .data 0x4BCE28

    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    ok = true;
    done = false;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* mobile;
    IPIPhysical* physical;
    if (!informationManager->getInformation(moveTo->getObject()) || !info || !(mobile = info->getMobile())
        || !(physical = info->getPhysical()))
    {
        done = true;
        ok = false;
        return true;
    }
    IPIControlInfo* const controlInfo = info->getControlInfo();
    IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
    if (!object->hasPhysicsEnabled())
        object->enablePhysics(info->getSide());
    if (!informationManager->getInformation(moveTo->getObject()))
    {
        done = true;
        ok = false;
        return true;
    }
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }

    Vec2 target;
    target.x = target.y = 0.0f;
    done = false;
    bool arrived;
    ok = moveTo->update(target, done, arrived, 0.0f) != 0;
    if (!ok)
    {
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = CarControl::resetControls(bot, bot->getInput(), true, false, false,
                                       speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        done = true;
        return true;
    }
    if (arrived || done)
    {
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = CarControl::resetControls(bot, bot->getInput(), true, false, false,
                                       speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        return true;
    }

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
    const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    if (distance > 1e-05f)
    {
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = inverse * direction.y;
    }
    float turnFactor = 1.0f;
    float largestAngle;
    if (std::fabs(mobile->getSpeedMagnitude()) > 1.0f && moveTo->getLargestAngleDiff(largestAngle))
    {
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
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = CarControl::resetControls(bot, bot->getInput(), true, false, false,
                                       speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        done = true;
        return true;
    }
    if (!ok)
        return true;

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
