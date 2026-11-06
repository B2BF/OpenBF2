// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EE160 (__tcf_0 0x1EE1C0, _GLOBAL__I 0x1EE1F0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectEnemyOccupied.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIUtils/VehicleOccupancy.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EDFF0 (C1; C2 0x1EDFC0): +4 = 0, +8 = the handle.
BAPConObjectEnemyOccupied::BAPConObjectEnemyOccupied(unsigned int object)
{
    setOwner(0);
    m_object = object;
}

// ELF 0x1EE0F0 (D0 0x1EE110)
BAPConObjectEnemyOccupied::~BAPConObjectEnemyOccupied()
{
}

// ELF 0x1EE020: vehicleOccupied(handle, bot side) (Bot +0xE0), then the information's side
// (vfn +0x44, Information::getSide) against the bot's side again.
bool BAPConObjectEnemyOccupied::evaluate(Bot* bot)
{
    if (!vehicleOccupied(m_object, bot->getSide()))
        return false;
    Information* info = bapinline::info(m_object);
    if (!info)
        return false;
    return info->getSide() != bot->getSide();
}

// ELF 0x1EE150
BAPConditionType BAPConObjectEnemyOccupied::getConditionType() const
{
    return BAPCTObjectEnemyOccupied;   // 20
}

} // namespace ai
} // namespace hfe
} // namespace dice
