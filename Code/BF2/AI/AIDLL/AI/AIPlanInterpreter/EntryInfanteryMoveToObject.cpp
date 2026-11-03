// EntryInfanteryMoveToObject (attested TU `EntryInfanteryMoveToObject.cpp`, split out of
// BAPEntries.cpp by S0). Recovered from libaidll.so (Linux 1.5.3153, ia-32): the full-LOD soldier
// entry for BAPAMoveToObject (statement type 9). Package B; see docs/macos-ai-behaviour-plan.md.
// Flags as in EntryInfanteryMoveTo: `ok` (the step ran), `done` (finished; failed when !ok).

// static init: ELF 0x2B8550 (__tcf_0 0x2B85B0, _GLOBAL__I 0x2B85E0): <iostream>'s Init object only.
#include <iostream>

#include "EntryInfanteryMoveToObject.h"

#include "InfanteryControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

EntryInfanteryMoveToObject::EntryInfanteryMoveToObject()
{
}

EntryInfanteryMoveToObject::~EntryInfanteryMoveToObject()
{
}

int EntryInfanteryMoveToObject::getTargetId()
{
    return 9;
}

// ELF 0x2B80D0
bool EntryInfanteryMoveToObject::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* moveTo = static_cast<BAPAMoveTo*>(script);
    BotMain* botMain = static_cast<BotMain*>(bot);
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

    if (informationManager->getInformation(static_cast<unsigned int>(moveTo->getObject())) == 0)
    {
        done = true;
        ok = false;
        return true;
    }
    if (bot->isFireAllowed() && moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;

    // BotMain +0x21C/+0x218/+0x220/+0x22C/+0x228, named as in AIBot/BotMain.h
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now - botMain->getTimeOfLastFailedStaticSkyAttack() < 1.5)
    {
        botMain->setTimeOfLastFailedStaticAttack(now);
    }
    else if (botMain->getBotWantsToSimulateArtillery())
    {
        botMain->setTimeOfLastFailedStaticSkyAttack(now);
        botMain->setBotWantsToSimulateArtillery(false);
    }

    vec3ToVec2XZ(info->getPosition(), position);
    bool arrived;
    ok = moveTo->update(dir, done, arrived, 0.0f) != 0;
    if (ok)
    {
        if (arrived || done)
        {
            ok = infanteryResetControls(bot, bot->getInput(), true, false, false);
            return true;
        }
        Information* target = informationManager->getInformation(static_cast<unsigned int>(moveTo->getObject()));
        const bool movingAway = moveTo->getIsMovingAway();
        if (!moveTo->getTargetDir(bot, dir, targetDir, target, movingAway))
        {
            infantry_detail::setSensingMoveDirection(bot->getSensingAgent(), targetDir);
            removeZoom(bot);
            const bool sprint = moveTo->getIsSprinting();
            infanteryControlThrottle(bot, bot->getInput(),
                                     info->getControlInfo(), mobile->getMaxSpeed(), sprint);
            if (info->getSoldierPose() != SPSwimming)
                bot->requestSoldierPose(SPStanding, true);
            if (moveTo->isStuck())
            {
                // a stuck soldier gives up: IAIGameEffector +0x30
                static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->suicide(bot->getPlayer());
            }
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
