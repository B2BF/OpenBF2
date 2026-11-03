// Key-driven turret aiming entry for BAPAAimAt. Attested TU: EntryKeyTurretAimAt.cpp.

// static init: ELF 0x2B9580 (__tcf_0 0x2B95E0, _GLOBAL__I 0x2B9610): <iostream>'s Init object only.
#include <iostream>

#include "EntryKeyTurretAimAt.h"

#include "ControlFunctions.h"
#include "InfanteryControl.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAAimAt.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B8BC0 (C1; C2 0x2B8BA0).
EntryKeyTurretAimAt::EntryKeyTurretAimAt()
{
}

// ELF 0x2B8C00 (D1; D2 0x2B8BE0, D0 0x2B8C20).
EntryKeyTurretAimAt::~EntryKeyTurretAimAt()
{
}

// ELF 0x2B9630.
int EntryKeyTurretAimAt::getTargetId()
{
    return 6;
}

// ELF 0x2B8C60. Drives mapped pitch and yaw inputs from the weapon-relative aim vector and
// their previous errors. The plain target-vector fallback is used without normalising it.
bool EntryKeyTurretAimAt::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAAimAt* const aim = static_cast<BAPAAimAt*>(script);
    Vec3 direction;
    direction.x = direction.y = direction.z = 0.0f;
    Vec3 ownSpeed;
    ownSpeed.x = ownSpeed.y = ownSpeed.z = 0.0f;
    if (aim->m_endCondition->evaluate(bot))
    {
        done = true;
        ok = true;
        aim->m_lastYawError = 3.141592741f;
        return true;
    }
    done = false;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        aim->m_lastYawError = 3.141592741f;
        return true;
    }
    IPIArmament* const armament = info->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    const Mat4& transformation = weapon->getAbsoluteTransformation();
    IPIMobile* mobile = info->getMobile();
    if (!mobile && !info->isPrimary())
    {
        Information* const primary = informationManager->getInformation(info->getPrimaryObject(bot->getSide()));
        if (primary)
            mobile = primary->getMobile();
    }
    if (mobile)
        ownSpeed = mobile->getSpeed();
    const Mat4& origin = weapon->getProjectileOrigin();
    const bool indirect = weapon->getIndirect();
    const float gravity = weapon->getGravityModifier();
    const float velocity = weapon->getExitVelocity();
    const float drag = weapon->getDrag();
    if (!aim->getAimVec(origin, ownSpeed, direction, 0.5f, drag, velocity, gravity, indirect, bot))
    {
        const Vec3& position = *reinterpret_cast<const Vec3*>(&transformation.m[12]);
        if (!aim->getVectorToTarget(position, direction))
        {
            done = true;
            ok = false;
            aim->m_lastYawError = 3.141592741f;
            return true;
        }
    }
    float pitchDot = direction.x * transformation.m[4] + direction.y * transformation.m[5]
                   + direction.z * transformation.m[6];
    if (pitchDot < -1.0f)
        pitchDot = -1.0f;
    else if (pitchDot > 1.0f)
        pitchDot = 1.0f;
    const float pitchError = acosf(pitchDot) - 1.570796371f;
    float yawDot = direction.x * transformation.m[0] + direction.y * transformation.m[1]
                 + direction.z * transformation.m[2];
    if (yawDot < -1.0f)
        yawDot = -1.0f;
    else if (yawDot > 1.0f)
        yawDot = 1.0f;
    float yawError = acosf(yawDot) - 1.570796371f;
    if (direction.x * transformation.m[8] + direction.y * transformation.m[9]
        + direction.z * transformation.m[10] < 0.0f)
        yawError = (yawError < 0.0f ? -3.141592741f : 3.141592741f) - yawError;

    IPIControlInfo* const controlInfo = info->getControlInfo();
    io::PlayerInput* const pitchInput = bot->getInput();
    const io::PlayerInputMap pitchMapping = controlInfo->getAimVerticalControl();
    float pitch = infantry_detail::inputValue(pitchInput, pitchMapping);
    io::PlayerInput* const yawInput = bot->getInput();
    const io::PlayerInputMap yawMapping = controlInfo->getAimHorizontalControl();
    float yaw = infantry_detail::inputValue(yawInput, yawMapping);
    const float pitchScale = controlInfo->getPitchScale();
    const float pitchLookAhead = controlInfo->getPitchLookAhead();
    const float pitchSensitivity = controlInfo->getPitchSensitivity();
    updateAngle(pitchError, pitchError - aim->m_pitchErrors[aim->m_pitchIndex], pitch,
                pitchSensitivity, pitchLookAhead, pitchScale);
    const float yawScale = controlInfo->getRollScale();
    const float yawLookAhead = controlInfo->getRollLookAhead();
    const float yawSensitivity = controlInfo->getRollSensitivity();
    updateAngle(yawError, yawError - aim->m_lastYawError, yaw, yawSensitivity, yawLookAhead, yawScale);
    io::PlayerInput* const output = bot->getInput();
    const io::PlayerInputMap vertical = controlInfo->getAimVerticalControl();
    output->setInput(vertical, pitch);
    const io::PlayerInputMap horizontal = controlInfo->getAimHorizontalControl();
    bot->getInput()->setInput(horizontal, yaw);
    aim->m_pitchIndex = (aim->m_pitchIndex + 1) % 10;
    aim->m_pitchErrors[aim->m_pitchIndex] = pitchError;
    aim->m_lastYawError = yawError;
    ok = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
