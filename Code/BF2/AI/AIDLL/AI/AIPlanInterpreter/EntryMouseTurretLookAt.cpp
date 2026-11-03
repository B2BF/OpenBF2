// EntryMouseTurretLookAt (attested TU `EntryMouseTurretLookAt.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the soldier mouse-look entry for BAPALook and its subclasses (statement type 23). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2BA240 (__tcf_0 0x2BA2A0, _GLOBAL__I 0x2BA2D0): <iostream>'s Init object only.
#include <iostream>

#include "EntryMouseTurretLookAt.h"

#include "MouseControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBotActionPlan/BAPALook.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "InfanteryControl.h"

namespace dice {
namespace hfe {
namespace ai {

EntryMouseTurretLookAt::EntryMouseTurretLookAt()
{
}

EntryMouseTurretLookAt::~EntryMouseTurretLookAt()
{
}

int EntryMouseTurretLookAt::getTargetId()
{
    return 23;
}

// BAPALook fields as this entry uses them (0x2B9E90): +0x18 (m_lookSpeed) is
// mouseControlLookAtDirection's allowed deviation, +0x1C (m_deviation) the last yaw error and
// +0x20 (m_lastControl) the last pitch error.

// ELF 0x2B9E90. Mouse-look towards BAPALook's direction; the end condition finishes the look.
// A direction that is not available fails the statement softly (not done, yaw error reset to pi).
bool EntryMouseTurretLookAt::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPALook* look = static_cast<BAPALook*>(script);
    bot->getSensingAgent()->setOverride(true);
    if (!look->m_endCondition->evaluate(bot))
    {
        done = false;
        Information* info = informationManager->getInformation(bot->getCurrentEquipment());
        if (info != 0)
        {
            Vec3 dir = look->getLookDirection(ok);
            infantry_detail::normaliseInline(dir);
            if (!ok)
            {
                done = false;
                look->m_deviation = 3.14159274f;
                return true;
            }
            IPIControlInfo* controlInfo = info->getControlInfo();
            ok = true;
            io::PlayerInput* input = bot->getInput();
            const float allowed = look->m_lookSpeed;
            const float lastYaw = look->m_deviation;
            const float lastPitch = look->m_lastControl;
            const Mat4& camera = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
            float pitchError;
            float yawError;
            mouseControlLookAtDirection(bot, dir, camera, pitchError, lastPitch, yawError, lastYaw, allowed,
                                        controlInfo, input, 0.0f, 0.0f, 0.0f);
            look->m_deviation = yawError;
            look->m_lastControl = pitchError;
            const Mat4& after = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
            look->m_cameraForward.x = after.m[8];
            look->m_cameraForward.y = after.m[9];
            look->m_cameraForward.z = after.m[10];
            return true;
        }
        done = true;
        ok = false;
    }
    else
    {
        done = true;
        ok = true;
    }
    look->m_deviation = 3.14159274f;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
