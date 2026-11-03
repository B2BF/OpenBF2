// EntryCarResetControls (split out of BAPEntries.cpp by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

// static init: ELF 0x2B52A0 (__tcf_0 0x2B5300, _GLOBAL__I 0x2B5330): <iostream>'s Init object only.
#include <iostream>

#include "EntryCarResetControls.h"

#include "CarControl.h"
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

EntryCarResetControls::EntryCarResetControls()
{
}

EntryCarResetControls::~EntryCarResetControls()
{
}

int EntryCarResetControls::getTargetId()
{
    return 14;
}

// ELF 0x2B4FD0 (listing). A single-input reset releases that one input (setInput inlined with its
// range diagnostic) and succeeds. The group form resets through CarControl::resetControls with
// the equipment's forward speed (mobile speed dot facing); the information lookup is not
// null-tested, and the mobile's speed, the facing and the transformation are asked for once more
// beforehand and discarded, as compiled. `ok` is that call's answer; `done` is always set.
bool EntryCarResetControls::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
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
    mobile->getSpeed();                                               // +0x1C, discarded
    equipment->getFaceing();                                          // +0x40, discarded
    equipment->getTransformation();                                   // +0x3C, discarded
    const Vec3 facing = equipment->getFaceing();
    const Vec3& speed = mobile->getSpeed();
    const float forwardSpeed = speed.y * facing.y + speed.x * facing.x + speed.z * facing.z;
    ok = CarControl::resetControls(bot, bot->getInput(), reset->m_resetMove, reset->m_resetLook,
                                   reset->m_resetTrigger, forwardSpeed) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
