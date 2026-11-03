// EntrySoldierPose (attested TU `EntrySoldierPose.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the soldier entry for BAPASoldierPose (statement type 26). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2C3E50 (__tcf_0 0x2C3EB0, _GLOBAL__I 0x2C3EE0): <iostream>'s Init object only.
#include <iostream>

#include "EntrySoldierPose.h"

#include "InfanteryControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPASoldierPose.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"

namespace dice {
namespace hfe {
namespace ai {

EntrySoldierPose::EntrySoldierPose()
{
}

EntrySoldierPose::~EntrySoldierPose()
{
}

int EntrySoldierPose::getTargetId()
{
    return 26;
}

// ELF 0x2C3C60. Wanted pose: the statement's normal pose, or standing while the bot presses
// throttle, yaw or sprint. Done when the soldier is in it; otherwise the pose is requested (not while
// swimming). Without the bot's own information the image returns false and leaves the flags alone.
bool EntrySoldierPose::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPASoldierPose* pose = static_cast<BAPASoldierPose*>(script);
    Information* info = informationManager->getInformation(bot->getCurrentEquipment());
    if (info == 0)
        return false;
    SoldierPose wanted = static_cast<SoldierPose>(static_cast<BAPAComponentSoldierPose*>(pose)->getNormalPose());
    const io::PlayerInput* input = bot->getInput();
    if (infantry_detail::inputValue(input, io::PIThrottle) != 0.0f
        || infantry_detail::inputValue(input, io::PIYaw) != 0.0f
        || infantry_detail::inputValue(input, io::PISprint) != 0.0f)
        wanted = SPStanding;
    if (info->getSoldierPose() == wanted)
    {
        ok = true;
        done = true;
        return true;
    }
    const SoldierPose current = info->getSoldierPose();
    if (current != SPSwimming && current != wanted)
        bot->requestSoldierPose(wanted, pose->getFlag());
    ok = true;
    done = false;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
