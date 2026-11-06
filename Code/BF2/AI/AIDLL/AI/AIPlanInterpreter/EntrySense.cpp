// EntrySense (attested TU `EntrySense.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the entry for BAPASense (statement type 20). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2C3AE0 (__tcf_0 0x2C3B40, _GLOBAL__I 0x2C3B70): <iostream>'s Init object only.
#include <iostream>

#include "EntrySense.h"

#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPASense.h"
#include "../AIBotActionPlan/BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C3990 (C1; C2 0x2C3970)
EntrySense::EntrySense()
{
}

// ELF 0x2C39D0 (D1; D2 0x2C39B0, D0 0x2C39F0)
EntrySense::~EntrySense()
{
}

// ELF 0x2C3B90
int EntrySense::getTargetId()
{
    return 20;
}

// ELF 0x2C3A30. The condition at +0x18 ends the statement at once. Otherwise the statement keeps
// running until the condition at +0x14 holds; then the bot's inverse attention is reset to 0 and it
// finishes.
bool EntrySense::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPASense* sense = static_cast<BAPASense*>(script);
    if (sense->m_secondCondition->evaluate(bot))
    {
        ok = true;
        done = true;
        return true;
    }
    if (!sense->m_firstCondition->evaluate(bot))
    {
        ok = true;
        done = false;
        return true;
    }
    static_cast<BotMain*>(bot)->setInvAttention(0.0f);
    ok = true;
    done = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
