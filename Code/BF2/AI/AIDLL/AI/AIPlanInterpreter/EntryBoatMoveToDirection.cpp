// EntryBoatMoveToDirection (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2B31E0 (__tcf_0 0x2B3240, _GLOBAL__I 0x2B3270): <iostream>'s Init object only.
#include <iostream>

#include "EntryBoatMoveToDirection.h"

#include "ControlFunctions.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveToDirection.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2B2660 (C1; C2 0x2B2640).
EntryBoatMoveToDirection::EntryBoatMoveToDirection()
{
}

// ELF 0x2B26A0 (D1; D2 0x2B2680, D0 0x2B26C0).
EntryBoatMoveToDirection::~EntryBoatMoveToDirection()
{
}

// ELF 0x2B3290.
int EntryBoatMoveToDirection::getTargetId()
{
    return 8;
}

// ELF 0x2B2700.
bool EntryBoatMoveToDirection::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveToDirection* const moveTo = static_cast<BAPAMoveToDirection*>(script);
    Vec2 position = {0.0f, 0.0f};
    Vec2 facing = {0.0f, 0.0f};
    Vec2 normal = {0.0f, 0.0f};
    Vec2 point = {0.0f, 0.0f};
    Vec2 targetFacing = {0.0f, 0.0f};
    Vec2 targetNormal = {0.0f, 0.0f};
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    const Mat4 transformation = info->getTransformation();
    const Vec3 forward = {transformation.m[8], transformation.m[9], transformation.m[10]};
    const Vec3 position3 = info->getPosition();
    const float magnitude = info->getMobile()->getSpeedMagnitude();
    const Vec3& speed = info->getMobile()->getSpeed();
    const float speedDot = forward.x * speed.x + forward.y * speed.y + forward.z * speed.z;
    const float sign = speedDot > 0.0f ? 1.0f : (speedDot < 0.0f ? -1.0f : 0.0f);
    const float signedSpeed = sign * magnitude;
    const float radius = info->getMobile()->getTurnRadius();
    // The shipped caller stores only al and continues regardless of the update result.
    ok = static_cast<unsigned char>(moveTo->update(point, done)) != 0;
    vec3ToVec2XZ(position3, position);
    vec3ToVec2XZ(forward, facing);
    vec3ToVec2XZ(moveTo->m_direction, targetFacing);
    getNormal(facing, normal);
    vehiclecontrol::normalizeVec2(normal);
    getNormal(targetFacing, targetNormal);
    vehiclecontrol::normalizeVec2(targetNormal);
    Vec2 direction = {(radius * targetNormal.x + point.x) - (radius * normal.x + position.x),
                      (radius * targetNormal.y + point.y) - (radius * normal.y + position.y)};
    const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 1e-05f)
    {
        const float inverse = 1.0f / length;
        direction.x *= inverse;
        direction.y *= inverse;
    }
    const auto clampCos = [](float value) {
        if (value < -1.0f) return -1.0f;
        if (value > 1.0f) return 1.0f;
        return value;
    };
    float targetAngle = 0.0f;
    if (length < radius * 0.2f)
        moveTo->unimplementedDirectionSlot11(2);
    else
    {
        targetAngle = std::acos(clampCos(direction.x * normal.x + direction.y * normal.y)) - 1.5707964f;
        if (direction.x * facing.x + direction.y * facing.y < 0.0f)
        {
            const float angleSign = targetAngle > 0.0f ? 1.0f : (targetAngle < 0.0f ? -1.0f : 0.0f);
            targetAngle = angleSign * 3.1415927f - targetAngle;
        }
        if (moveTo->unimplementedDirectionSlot10() == 1 || std::fabs(targetAngle) < 0.18325958f)
            moveTo->unimplementedDirectionSlot11(1);
        else
            targetAngle = -1.5707964f;
    }
    const float maximum = info->getMobile()->getMaxSpeed();
    info->getMobile()->getVehicleNumber();
    io::PlayerInput* const input = bot->getInput();
    float turnInput = (input->mappingFlags & 1ULL) ? input->inputs[0] : 0.0f;
    float throttle = (input->mappingFlags & 8ULL) ? input->inputs[3] : 0.0f;
    const float previousDot = normal.x * moveTo->m_state68.y + normal.y * moveTo->m_state68.z;
    updateAngle(targetAngle, std::acos(clampCos(previousDot)) - 1.5707964f, turnInput, 1.0f, 1.0f, 1.5f);
    updateLinear(maximum, signedSpeed, moveTo->m_state68.x, throttle, -1.0f, 1.0f, 1.0f);
    moveTo->m_state68.y = facing.x;
    moveTo->m_state68.z = facing.y;
    moveTo->m_state68.x = signedSpeed;
    io::PlayerInput* const output = bot->getInput();
    if (output->mappingFlags & 1ULL)
        output->inputs[0] = turnInput;
    bot->getInput()->setInput(static_cast<io::PlayerInputMap>(3), throttle);
    ok = true;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
