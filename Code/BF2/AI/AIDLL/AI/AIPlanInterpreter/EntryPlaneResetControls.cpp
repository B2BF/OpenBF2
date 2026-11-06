// EntryPlaneResetControls (split out of BAPEntries.cpp by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

// static init: ELF 0x2C3330 (__tcf_0 0x2C3390, _GLOBAL__I 0x2C33C0): <iostream>'s Init object only.
#include <iostream>

#include "EntryPlaneResetControls.h"

#include "PlaneControl.h"
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

EntryPlaneResetControls::EntryPlaneResetControls()
{
}

EntryPlaneResetControls::~EntryPlaneResetControls()
{
}

int EntryPlaneResetControls::getTargetId()
{
    return 14;
}

// ELF 0x2C30C0 (Ghidra). The bot's current equipment is looked up first (not null-tested). A
// single-input reset releases that one input; the group form resets through
// PlaneControl::resetControls with that information, ignores its answer, and sets `done` then `ok`.
bool EntryPlaneResetControls::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    BAPAResetControls* const reset = static_cast<BAPAResetControls*>(script);
    if (reset->hasSingleInput())
    {
        io::PlayerInput* const input = bot->getInput();
        input->setInput(static_cast<io::PlayerInputMap>(reset->getInput()), 0.0f);
        ok = true;
        done = true;
        return true;
    }
    PlaneControl::resetControls(bot, equipment, bot->getInput(), reset->m_resetMove, reset->m_resetLook,
                                reset->m_resetTrigger);
    done = true;
    ok = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
