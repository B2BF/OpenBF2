// EntryTankMoveTo (attested TU `EntryTankMoveTo.cpp`, split out of BAPEntries.cpp by S0).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32): the interpreter entry that drives a tank
// along a BAPAMoveTo. execute's flags follow the BotScript convention: `ok` (the step ran) and
// `done` (the statement is finished; with ok == false it failed).
//
// static init: ELF 0x2C4680 (__tcf_0 0x2C46E0, _GLOBAL__I 0x2C4710): the <iostream> guard object only.

#include "EntryTankMoveTo.h"

#include "TankControl.h"
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

EntryTankMoveTo::EntryTankMoveTo()
{
}

EntryTankMoveTo::~EntryTankMoveTo()
{
}

int EntryTankMoveTo::getTargetId()
{
    return 7;
}

// ELF 0x2C3FC0 (listing; Ghidra cross-check). The car entry's shape with the tank's tuning: no
// initial flag stores; the equipment's mobile/control/physical members are read before the
// equipment is null-tested (as shipped); physics is enabled also when already on while the static
// alwaysEnable is set; turn factor 1 - min(1, 2.5 * max(0, |largest angle diff| - 0.35)) * 0.7
// above 3 m/s; the approach factor uses the SQUARED distance minus the smallest radius and 0.5
// against fallOffDistanceSqr (196), at least 0.35; and TankControl gets the normalised direction.
bool EntryTankMoveTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    static bool alwaysEnable;                 // .bss 0x4CE5B1 (false)
    static float fallOffDistanceSqr = 196.0f;   // .data 0x4BD8C4

    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = info->getMobile();               // read before the null test (0x2C401E)
    IPIControlInfo* const controlInfo = info->getControlInfo();
    IPIPhysical* const physical = info->getPhysical();
    if (!info || !mobile || !controlInfo || !physical)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
    if (alwaysEnable || !object->hasPhysicsEnabled())
        object->enablePhysics(info->getSide());

    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double last = static_cast<BotMain*>(bot)->getLastCheckMoveToFinishedTime();   // Bot +0x274
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
    remaining.x = target.x - position.x;
    remaining.y = target.y - position.y;
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
