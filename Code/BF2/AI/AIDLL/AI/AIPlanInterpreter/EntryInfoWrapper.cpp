// EntryInfoWrapper (attested TU `EntryInfoWrapper.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the entry for BAPInfoWrapper (statement type 15). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2B8AE0 (__tcf_0 0x2B8B40, _GLOBAL__I 0x2B8B70): <iostream>'s Init object only.
#include <iostream>

#include "EntryInfoWrapper.h"

#include "PlanInterpreter.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPInfoWrapper.h"

namespace dice {
namespace hfe {
namespace ai {

EntryInfoWrapper::EntryInfoWrapper()
{
}

EntryInfoWrapper::~EntryInfoWrapper()
{
}

int EntryInfoWrapper::getTargetId()
{
    return 15;
}

// ELF 0x2B8A80. Runs the wrapped statement through the bot's plan interpreter (PlanInterpreter +8).
bool EntryInfoWrapper::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BotScript* inner = static_cast<BAPInfoWrapper*>(script)->m_statement;   // 0x2B8A98 +0x14
    return static_cast<BotMain*>(bot)->getPlanInterpreter()->execute(inner, bot, ok, done);
}

} // namespace ai
} // namespace hfe
} // namespace dice
