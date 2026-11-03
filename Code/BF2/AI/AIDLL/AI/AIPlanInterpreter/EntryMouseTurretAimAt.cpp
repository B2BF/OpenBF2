// EntryMouseTurretAimAt (attested TU `EntryMouseTurretAimAt.cpp`, split out of BAPEntries.cpp by S0). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32): the soldier mouse-aim entry for BAPAAimAt and its subclasses (statement type 6). Package B; see docs/macos-ai-behaviour-plan.md.
// execute's flags follow the BotScript convention: `ok` (the step ran) and `done` (finished; with
// ok == false the statement failed).

// static init: ELF 0x2B9D10 (__tcf_0 0x2B9D70, _GLOBAL__I 0x2B9DA0): <iostream>'s Init object only.
#include <iostream>

#include "EntryMouseTurretAimAt.h"

#include "InfanteryControl.h"
#include "MouseControl.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotSubObjects.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBotActionPlan/BAPAAimAt.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

EntryMouseTurretAimAt::EntryMouseTurretAimAt()
{
}

EntryMouseTurretAimAt::~EntryMouseTurretAimAt()
{
}

int EntryMouseTurretAimAt::getTargetId()
{
    return 6;
}

// ELF 0x2B9700. Mouse-aims the soldier's active weapon with BAPAAimAt's ballistic aim vector
// (falling back to the plain vector to the target), with the target's angular speed as feed-forward
// and the average of the last ten pitch errors as the vertical bias. The end condition finishes it.
bool EntryMouseTurretAimAt::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAAimAt* aim = static_cast<BAPAAimAt*>(script);
    Vec3 dir;
    dir.x = dir.y = dir.z = 0.0f;
    Vec3 ownSpeed;
    ownSpeed.x = ownSpeed.y = ownSpeed.z = 0.0f;
    bot->getSensingAgent()->setOverride(true);
    if (!aim->m_endCondition->evaluate(bot))
    {
        done = false;
        Information* info = informationManager->getInformation(bot->getCurrentEquipment());
        if (info != 0)
        {
            IPIControlInfo* controlInfo = info->getControlInfo();
            IPIArmament* armament = info->getArmament();
            WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
            const Mat4& weaponTransformation = weapon->getAbsoluteTransformation();
            IPIMobile* mobile = info->getMobile();
            if (mobile == 0 && !info->isPrimary())
            {
                Information* primary = informationManager->getInformation(info->getPrimaryObject(bot->getSide()));
                if (primary != 0)
                    mobile = primary->getMobile();
            }
            if (mobile != 0)
                ownSpeed = mobile->getSpeed();
            const Mat4& origin = weapon->getProjectileOrigin();
            Vec3 lastAim;
            lastAim.x = lastAim.y = lastAim.z = 0.0f;
            const bool haveLastAim = aim->getLastAimVec(lastAim) != 0;
            const bool indirect = weapon->getIndirect();
            const float gravity = weapon->getGravityModifier();
            const float exitVelocity = weapon->getExitVelocity();
            const float drag = weapon->getDrag();
            if (!aim->getAimVec(origin, ownSpeed, dir, 0.5f, drag, exitVelocity, gravity, indirect, bot))
            {
                Vec3 from;
                from.x = weaponTransformation.m[12];
                from.y = weaponTransformation.m[13];
                from.z = weaponTransformation.m[14];
                if (!aim->getVectorToTarget(from, dir))   // slot +0x28, 0x2B9985 test al
                {
                    done = true;
                    ok = false;
                    aim->m_lastYawError = 3.14159274f;
                    return true;
                }
                infantry_detail::normaliseInline(dir);
            }
            Vec3 predicted;
            predicted.x = predicted.y = predicted.z = 0.0f;
            aim->getPredictedPosition(predicted);
            // 0x2B9A5C: BotMain +0xB4; only while a correction shot is out (+6).
            FireCorrectionData* correction = static_cast<BotMain*>(bot)->getFireCorrection();
            if (correction->m_flag6)
            {
                correction->m_aimDirection = dir;          // +0x20
                correction->m_predictedTarget = predicted; // +0x38
            }

            float yawSpeed = 0.0f;
            float pitchSpeed = 0.0f;
            if (haveLastAim)
            {
                const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
                yawSpeed = getSignedDiffAngleToDirXZ(lastAim, dir) / dt;
                pitchSpeed = getSignedDiffAngleToDirYZ(lastAim, dir) / dt;
            }
            ok = true;
            const float averagePitch = aim->getAveragePitchAngle();
            float pitchError;
            float yawError;
            mouseControlLookAtDirection(bot, dir, origin, pitchError, aim->m_pitchErrors[aim->m_pitchIndex], yawError,
                                        aim->m_lastYawError, 0.0043633231f, controlInfo, bot->getInput(),
                                        averagePitch, yawSpeed, pitchSpeed);
            aim->m_pitchIndex = (aim->m_pitchIndex + 1) % 10;
            aim->m_pitchErrors[aim->m_pitchIndex] = pitchError;
            aim->m_lastYawError = yawError;
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
    aim->m_lastYawError = 3.14159274f;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
