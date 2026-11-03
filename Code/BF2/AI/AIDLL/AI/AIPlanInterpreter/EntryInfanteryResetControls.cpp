// EntryInfanteryResetControls (attested TU `EntryInfanteryResetControls.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the soldier entry for BAPAResetControls (statement type 14). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2B8900 (__tcf_0 0x2B8960, _GLOBAL__I 0x2B8990): <iostream>'s Init object only.
#include <iostream>

#include "EntryInfanteryResetControls.h"

#include "InfanteryControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

EntryInfanteryResetControls::EntryInfanteryResetControls()
{
}

EntryInfanteryResetControls::~EntryInfanteryResetControls()
{
}

int EntryInfanteryResetControls::getTargetId()
{
    return 14;
}

// ELF 0x2B86D0. A single-input reset releases that one input (the image inlines setInput(index,
// 0.0f) with its "1 setInput out of range" diagnostic); the group form resets through
// infanteryResetControls. Always succeeds and finishes at once.
bool EntryInfanteryResetControls::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAResetControls* reset = static_cast<BAPAResetControls*>(script);
    if (reset->hasSingleInput())
    {
        io::PlayerInput* input = bot->getInput();
        input->setInput(static_cast<io::PlayerInputMap>(reset->getInput()), 0.0f);
    }
    else
    {
        // 0x2B88C0: movzbl +0x14 / +0x15 / +0x16 -> (move, look, trigger)
        infanteryResetControls(bot, bot->getInput(), reset->m_resetMove, reset->m_resetLook,
                               reset->m_resetTrigger);
    }
    ok = true;
    done = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
