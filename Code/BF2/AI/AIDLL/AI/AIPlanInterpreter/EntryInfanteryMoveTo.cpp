// EntryInfanteryMoveTo (attested TU `EntryInfanteryMoveTo.cpp`, split out of BAPEntries.cpp by S0).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32): the interpreter entry that executes a
// BAPAMoveTo for a soldier at full LOD. Package B; see docs/macos-ai-behaviour-plan.md.
//
// execute's two flags follow the BotScript convention: `ok` (the step ran) and `done` (the
// statement is finished; with ok == false it failed).

// static init: ELF 0x2B7DC0 (__tcf_0 0x2B7E20, _GLOBAL__I 0x2B7E50): <iostream>'s Init object only.
#include <iostream>

#include "EntryInfanteryMoveTo.h"

#include "InfanteryControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Vec3.h"
#include "../AIBot/SquadMember.h"

namespace dice {
namespace hfe {
namespace ai {

EntryInfanteryMoveTo::EntryInfanteryMoveTo()
{
}

EntryInfanteryMoveTo::~EntryInfanteryMoveTo()
{
}

int EntryInfanteryMoveTo::getTargetId()
{
    return 7;
}

// ELF 0x2B7AA0
bool EntryInfanteryMoveTo::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* moveTo = static_cast<BAPAMoveTo*>(script);
    Vec2 dir;
    dir.x = dir.y = 0.0f;
    Vec2 unused;   // [INFERRED] a zeroed local the image never reads (-0x48)
    unused.x = unused.y = 0.0f;
    Vec2 targetDir;
    targetDir.x = targetDir.y = 0.0f;
    Vec2 position;
    position.x = position.y = 0.0f;

    Information* info = informationManager->getInformation(bot->getCurrentEquipment());
    IPIMobile* mobile;
    if (info == 0 || (mobile = info->getMobile()) == 0)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* physical = info->getAIObject()->getPhysical();
    if (!physical->hasPhysicsEnabled())
        physical->enablePhysics(info->getSide());
    vec3ToVec2XZ(info->getPosition(), position);

    if (bot->isFireAllowed() && moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    bool arrived;
    ok = moveTo->update(dir, done, arrived, 0.0f) != 0;
    if (ok)
    {
        if (arrived || done)
        {
            ok = infanteryResetControls(bot, bot->getInput(), true, false, false);
            return true;
        }
        if (!moveTo->getTargetDir(bot, dir, targetDir, 0, false))
        {
            infantry_detail::setSensingMoveDirection(bot->getSensingAgent(), targetDir);
            removeZoom(bot);
            const bool sprint = moveTo->getIsSprinting();
            infanteryControlThrottle(bot, bot->getInput(),
                                     info->getControlInfo(), mobile->getMaxSpeed(), sprint);
            if (info->getSoldierPose() == SPSwimming)
                return true;
            bot->requestSoldierPose(SPStanding, true);
            return true;
        }
    }
    ok = infanteryResetControls(bot, bot->getInput(), true, false, false);
    done = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
