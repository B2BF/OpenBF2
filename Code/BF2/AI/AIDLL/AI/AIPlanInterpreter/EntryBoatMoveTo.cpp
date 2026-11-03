// Boat movement entry for BAPAMoveTo. Attested TU: EntryBoatMoveTo.cpp.

// static init: ELF 0x2B2520 (__tcf_0 0x2B2580, _GLOBAL__I 0x2B25B0): <iostream>'s Init object only.
#include <iostream>

#include "EntryBoatMoveTo.h"

#include "BoatControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B2150 (C1; C2 0x2B2130).
EntryBoatMoveTo::EntryBoatMoveTo()
{
}

// ELF 0x2B2190 (D1; D2 0x2B2170, D0 0x2B21B0).
EntryBoatMoveTo::~EntryBoatMoveTo()
{
}

// ELF 0x2B2630.
int EntryBoatMoveTo::getTargetId()
{
    return 7;
}

// ELF 0x2B21F0. The boat entry updates the path, releases movement inputs on arrival or failure,
// and otherwise steers towards the next point. resetControls may also update done.
bool EntryBoatMoveTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    Vec2 point;
    point.x = point.y = 0.0f;
    Vec2 position;
    position.x = position.y = 0.0f;
    done = false;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info || !info->getMobile() || !info->getPhysical())
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* const physical = info->getAIObject()->getPhysical();
    if (!physical->hasPhysicsEnabled())
        physical->enablePhysics(info->getSide());
    if (!info->getMobile() || !info->getControlInfo())
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
    bool arrived;
    ok = static_cast<unsigned char>(moveTo->update(point, done, arrived, 0.0f)) != 0;
    if (!ok)
    {
        ok = static_cast<unsigned char>(BoatControl::resetControls(bot, true, false, false, done)) != 0;
        done = true;
        return true;
    }
    if (arrived || done)
    {
        ok = static_cast<unsigned char>(BoatControl::resetControls(bot, true, false, false, done)) != 0;
        return true;
    }
    vec3ToVec2XZ(info->getPosition(), position);
    Vec2 direction;
    direction.x = point.x - position.x;
    direction.y = point.y - position.y;
    const float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (distance > 1e-05f)
    {
        const float inverse = 1.0f / distance;
        direction.x = direction.x * inverse;
        direction.y = inverse * direction.y;
    }
    ok = static_cast<unsigned char>(BoatControl::towardsDirection(bot, info, direction, distance, moveTo, bot->getInput(), 1e9f)) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
