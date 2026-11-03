// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F8540 (__tcf_0 0x1F85A0, _GLOBAL__I 0x1F85D0): <iostream>'s Init object only.
#include <iostream>

#include "BAPICChangeVehicle.h"

#include "../AIBot/BotMain.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F8490 (C1; C2 0x1F8440): +4/+8/+0xC = 0, +0x14 = the handle (+0x10 is not written).
BAPICChangeVehicle::BAPICChangeVehicle(unsigned int target)
{
    m_target = target;
}

// ELF 0x1F85F0 (D0 0x1F8630): the inlined ~BAPStatement only.
BAPICChangeVehicle::~BAPICChangeVehicle()
{
}

// ELF 0x1F84E0: Bot vfn +0x1B8 (setChangeTarget) with +0x14; ok = finished = true.
bool BAPICChangeVehicle::execute(Bot* bot, bool& ok, bool& finished)
{
    bot->setChangeTarget(m_target);
    ok = true;
    finished = true;
    return true;
}

// ELF 0x1F8520
BAPStatementType BAPICChangeVehicle::getId() const
{
    return BAPSTICChangeVehicle;   // 29
}

// ELF 0x1F8530
bool BAPICChangeVehicle::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
