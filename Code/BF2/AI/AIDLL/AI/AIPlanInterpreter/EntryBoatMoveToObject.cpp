// Boat movement entry for an object target. Attested TU: EntryBoatMoveToObject.cpp.

// static init: ELF 0x2B3710 (__tcf_0 0x2B3770, _GLOBAL__I 0x2B37A0): <iostream>'s Init object only.
#include <iostream>

#include "EntryBoatMoveToObject.h"

#include "BoatControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B32C0 (C1; C2 0x2B32A0).
EntryBoatMoveToObject::EntryBoatMoveToObject()
{
}

// ELF 0x2B3300 (D1; D2 0x2B32E0, D0 0x2B3320).
EntryBoatMoveToObject::~EntryBoatMoveToObject()
{
}

// ELF 0x2B37C0.
int EntryBoatMoveToObject::getTargetId()
{
    return 9;
}

// ELF 0x2B3360. A missing target finishes successfully. On the movement path the position is
// read both before isFinished and after update; a stuck move asks the game effector to suicide.
bool EntryBoatMoveToObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    Vec2 point;
    point.x = point.y = 0.0f;
    Vec2 position;
    position.x = position.y = 0.0f;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info || !info->getMobile() || !info->getPhysical())
    {
        done = true;
        ok = false;
        return true;
    }
    if (!informationManager->getInformation(moveTo->getObject()))
    {
        done = true;
        ok = true;
        return true;
    }
    IAIObjectPhysical* const physical = info->getAIObject()->getPhysical();
    if (!physical->hasPhysicsEnabled())
        physical->enablePhysics(info->getSide());
    vec3ToVec2XZ(info->getPosition(), position);
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
    if (moveTo->isStuck())
    {
        IAIPlayer* const player = bot->getPlayer();
        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->suicide(player);
    }
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
