// EntryTankMoveToObject (attested TU `EntryTankMoveToObject.cpp`, split out of BAPEntries.cpp by S0).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32): the interpreter entry that drives a tank
// to an object (a BAPAMoveTo with a target object). execute's flags follow the BotScript
// convention: `ok` (the step ran) and `done` (the statement is finished; with ok == false it failed).
//
// static init: ELF 0x2C5940 (__tcf_0 0x2C59A0, _GLOBAL__I 0x2C59D0): the <iostream> guard object only.

#include "EntryTankMoveToObject.h"

#include "TankControl.h"
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

EntryTankMoveToObject::EntryTankMoveToObject()
{
}

EntryTankMoveToObject::~EntryTankMoveToObject()
{
}

int EntryTankMoveToObject::getTargetId()
{
    return 9;
}

// ELF 0x2C5290 (listing; Ghidra cross-check). EntryTankMoveTo's steering for a move to an object,
// with these differences: physics is enabled only when off (no alwaysEnable static); it fails
// (done, not ok) without the target's information; isFinished is asked on every call (no
// one-second throttle); update gets the target's radius (its physical part +0x2C, unchecked); the
// direction is normalised above 1e-06. Shipped quirks kept: the equipment's parts are read before
// its null test, the fall-off compares the SQUARED distance (static fallOffDistanceSqr 196).
bool EntryTankMoveToObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    static float fallOffDistanceSqr = 196.0f;   // .data 0x4BD984

    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = info->getMobile();               // read before the null test (0x2C52F6)
    IPIControlInfo* const controlInfo = info->getControlInfo();
    IPIPhysical* const physical = info->getPhysical();
    if (!info || !mobile || !controlInfo || !physical)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
    if (!object->hasPhysicsEnabled())
        object->enablePhysics(info->getSide());
    Information* const target = informationManager->getInformation(moveTo->getObject());
    if (!target)
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
    done = false;

    Vec2 point;
    point.x = point.y = 0.0f;
    bool arrived;
    ok = moveTo->update(point, done, arrived, target->getPhysical()->getRadius()) != 0;
    if (!ok)
    {
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = TankControl::resetControls(bot, bot->getInput(), true, false, false,
                                        speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        done = true;
        return true;
    }
    if (arrived || done)
    {
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = TankControl::resetControls(bot, bot->getInput(), true, false, false,
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
    toTarget.x = point.x - position.x;
    toTarget.y = point.y - position.y;
    Vec2 direction = toTarget;
    const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    if (distance > 1e-06f)
    {
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = inverse * direction.y;
    }
    float turnFactor = 1.0f;
    float largestAngle;
    if (std::fabs(mobile->getSpeedMagnitude()) > 3.0f && moveTo->getLargestAngleDiff(largestAngle))
    {
        float excess = 0.0f;
        if (0.0f <= std::fabs(largestAngle) - 0.35f)
            excess = std::fabs(largestAngle) - 0.35f;
        float scaled = excess * 2.5f;
        if (1.0f < excess * 2.5f)
            scaled = 1.0f;
        turnFactor = 1.0f - scaled * 0.7f;
    }

    float mode;
    float angle;
    float reference;
    float limit;
    ok = TankControl::actionStatusDecision(bot, info, direction, distance, moveTo, mode, angle, reference, limit) != 0;
    if (mode == 666.0f)
    {
        const Vec3 facing = info->getFaceing();
        const Vec3& speed = mobile->getSpeed();
        ok = TankControl::resetControls(bot, bot->getInput(), true, false, false,
                                        speed.x * facing.x + speed.y * facing.y + speed.z * facing.z) != 0;
        done = true;
        return true;
    }
    if (!ok)
        return true;

    const float maxSpeed = mobile->getMaxSpeed();
    Vec2 remaining;
    remaining.x = point.x - position.x;
    remaining.y = point.y - position.y;
    const float remainingSqr = remaining.y * remaining.y + remaining.x * remaining.x;
    float approachFactor = 1.0f;
    const float gap = remainingSqr - physical->getSmallestRadius() - 0.5f;
    if (gap < fallOffDistanceSqr)
    {
        approachFactor = gap / fallOffDistanceSqr;
        if (approachFactor <= 0.35f)
            approachFactor = 0.35f;
    }
    const float factor = (approachFactor < turnFactor) ? approachFactor : turnFactor;
    ok = TankControl::controlTowardsDirection(info, controlInfo, mobile, physical, bot->getInput(), direction,
                                              factor * maxSpeed, mode, angle, reference, limit) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
