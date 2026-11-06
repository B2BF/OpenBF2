// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1DDBE0 (__tcf_0 0x1DDC40, _GLOBAL__I 0x1DDC70): <iostream>'s Init object only.
#include <iostream>

#include "BAPAOverrideSensingAgent.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DDA70 (C1; C2 0x1DDA20): the inline base only (+0x04, +0x08 = 0; +0x10 untouched).
BAPAOverrideSensingAgent::BAPAOverrideSensingAgent()
{
    m_controls = 0;
}

// ELF 0x1DDB00 (D1; D2 0x1DDAC0, D0 0x1DDB40)
BAPAOverrideSensingAgent::~BAPAOverrideSensingAgent()
{
}

// ELF 0x1DDB90: hands the look to the plan (SensingAgent::setOverride(true)); done at once.
bool BAPAOverrideSensingAgent::execute(Bot* bot, bool& ok, bool& failed)
{
    bot->getSensingAgent()->setOverride(true);
    ok = true;
    failed = true;
    return false;
}

// ELF 0x1DDCA0
BAPStatementType BAPAOverrideSensingAgent::getId() const
{
    return BAPSTOverrideSensingAgent;   // 21
}

// ELF 0x1DDC90
bool BAPAOverrideSensingAgent::hasDefault()
{
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
