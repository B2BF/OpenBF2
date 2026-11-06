// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D24C0 (__tcf_0 0x1D2520, _GLOBAL__I 0x1D2550): <iostream>'s Init object only.
#include <iostream>

#include "BAPAForgetTarget.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotSenses.h"
#include "../AIBot/SensingAgent.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D2330 (C1)
BAPAForgetTarget::BAPAForgetTarget(unsigned int target)
{
    m_target = target;
}

// ELF 0x1D23C0 (D0 0x1D2400)
BAPAForgetTarget::~BAPAForgetTarget()
{
}

// ELF 0x1D2450: `bot->getSensingAgent()->+0x68` (getSenses) ->forgetObject(target).
bool BAPAForgetTarget::execute(Bot* bot, bool& ok, bool& failed)
{
    bot->getSensingAgent()->getSenses()->forgetObject(m_target);
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1D24A0
BAPStatementType BAPAForgetTarget::getId() const
{
    return BAPSTForgetTarget;   // 39
}

// ELF 0x1D24B0
bool BAPAForgetTarget::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
