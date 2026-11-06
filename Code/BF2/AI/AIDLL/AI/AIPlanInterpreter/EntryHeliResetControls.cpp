// EntryHeliResetControls (split out of BAPEntries.cpp by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

// static init: ELF 0x2B7920 (__tcf_0 0x2B7980, _GLOBAL__I 0x2B79B0): <iostream>'s Init object only.
#include <iostream>

#include "EntryHeliResetControls.h"

#include "HeliControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/AIDllFrontier.h"
#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

EntryHeliResetControls::EntryHeliResetControls()
{
}

EntryHeliResetControls::~EntryHeliResetControls()
{
}

int EntryHeliResetControls::getTargetId()
{
    return 14;
}

// ELF 0x2B7700 (Ghidra). The bot's current equipment is looked up first and not used (only the
// virtual getCurrentEquipment call survives in the image). A single-input reset releases that one
// input; the group form resets through HeliControl::resetControls, ignores its answer, and sets
// `done` then `ok`.
bool EntryHeliResetControls::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    (void)equipment;
    BAPAResetControls* const reset = static_cast<BAPAResetControls*>(script);
    if (reset->hasSingleInput())
    {
        io::PlayerInput* const input = bot->getInput();
        input->setInput(static_cast<io::PlayerInputMap>(reset->getInput()), 0.0f);
        ok = true;
        done = true;
        return true;
    }
    HeliControl::resetControls(bot, reset->m_resetMove, reset->m_resetLook, reset->m_resetTrigger);
    done = true;
    ok = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
