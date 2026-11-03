// EntryBoatResetControls (split out of BAPEntries.cpp by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

// static init: ELF 0x2B3AC0 (__tcf_0 0x2B3B20, _GLOBAL__I 0x2B3B50): <iostream>'s Init object only.
#include <iostream>

#include "EntryBoatResetControls.h"

#include "BoatControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/AIDllFrontier.h"
#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

EntryBoatResetControls::EntryBoatResetControls()
{
}

EntryBoatResetControls::~EntryBoatResetControls()
{
}

int EntryBoatResetControls::getTargetId()
{
    return 14;
}

// ELF 0x2B3890 (Ghidra). A single-input reset releases that one input and succeeds. The group
// form hands `done` to BoatControl::resetControls, which clears it while the boat still has to
// brake; `ok` is that call's answer.
bool EntryBoatResetControls::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAResetControls* const reset = static_cast<BAPAResetControls*>(script);
    if (reset->hasSingleInput())
    {
        io::PlayerInput* const input = bot->getInput();
        input->setInput(static_cast<io::PlayerInputMap>(reset->getInput()), 0.0f);
        ok = true;
        done = true;
        return true;
    }
    ok = BoatControl::resetControls(bot, reset->m_resetMove, reset->m_resetLook, reset->m_resetTrigger,
                                    done) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
