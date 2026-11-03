// EntryTankResetControls (split out of BAPEntries.cpp by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

// static init: ELF 0x2C5D70 (__tcf_0 0x2C5DD0, _GLOBAL__I 0x2C5E00): <iostream>'s Init object only.
#include <iostream>

#include "EntryTankResetControls.h"

#include "TankControl.h"
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

EntryTankResetControls::EntryTankResetControls()
{
}

EntryTankResetControls::~EntryTankResetControls()
{
}

int EntryTankResetControls::getTargetId()
{
    return 14;
}

// ELF 0x2C5AC0 (Ghidra, listing for the dot product). A single-input reset releases that one
// input (setInput inlined with its range diagnostic) and succeeds. The group form resets through
// TankControl::resetControls with the equipment's forward speed (mobile speed dot facing); the
// information lookup is not null-tested. Unlike the car entry, nothing is asked for twice.
// `ok` is that call's answer; `done` is always set.
bool EntryTankResetControls::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
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
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* const mobile = equipment->getMobile();                 // +0x38
    done = true;
    const Vec3 facing = equipment->getFaceing();                      // +0x40
    const Vec3& speed = mobile->getSpeed();                           // +0x1C
    const float forwardSpeed = speed.y * facing.y + speed.x * facing.x + speed.z * facing.z;
    ok = TankControl::resetControls(bot, bot->getInput(), reset->m_resetMove, reset->m_resetLook,
                                    reset->m_resetTrigger, forwardSpeed) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
