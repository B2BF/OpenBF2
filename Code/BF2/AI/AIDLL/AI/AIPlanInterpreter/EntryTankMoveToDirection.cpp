// EntryTankMoveToDirection (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2C5110 (__tcf_0 0x2C5170, _GLOBAL__I 0x2C51A0): <iostream>'s Init object only.
#include <iostream>

#include "EntryTankMoveToDirection.h"

#include "ControlFunctions.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveToDirection.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C4760 (C1; C2 0x2C4740).
EntryTankMoveToDirection::EntryTankMoveToDirection()
{
}

// ELF 0x2C47A0 (D1; D2 0x2C4780, D0 0x2C47C0).
EntryTankMoveToDirection::~EntryTankMoveToDirection()
{
}

// ELF 0x2C51C0.
int EntryTankMoveToDirection::getTargetId()
{
    return 8;
}

// ELF 0x2C4800.
bool EntryTankMoveToDirection::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveToDirection* const moveTo = static_cast<BAPAMoveToDirection*>(script);
    Vec2 position = {0.0f, 0.0f};
    Vec2 direction = {0.0f, 0.0f};
    Vec2 normal = {0.0f, 0.0f};
    Vec2 facing = {0.0f, 0.0f};
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObjectPhysical* const physical = info->getAIObject()->getPhysical();
    if (!physical->hasPhysicsEnabled())
        physical->enablePhysics(info->getSide());
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    vec3ToVec2XZ(info->getPosition(), position);
    const Vec3 face = info->getFaceing();
    vec3ToVec2XZ(face, facing);
    getNormal(facing, normal);
    vehiclecontrol::normalizeVec2(normal);
    const float magnitude = info->getMobile()->getSpeedMagnitude();
    const Vec3 facing3 = info->getFaceing();
    const Vec3& speed = info->getMobile()->getSpeed();
    const float speedDot = speed.x * facing3.x + speed.y * facing3.y + speed.z * facing3.z;
    const float sign = speedDot > 0.0f ? 1.0f : (speedDot < 0.0f ? -1.0f : 0.0f);
    const float signedSpeed = sign * magnitude;
    const auto clampCos = [](float value) {
        const float upper = value > 1.0f ? 1.0f : value;
        return upper > -1.0f ? upper : -1.0f;
    };
    float targetAngle;
    float targetSpeed;
    if (moveTo->m_condition64->evaluate(bot))
    {
        vec3ToVec2XZ(moveTo->m_direction, direction);
        targetSpeed = 0.0f;
        const float dot = direction.x * normal.x + direction.y * normal.y;
        if (facing.x * direction.x + facing.y * direction.y > 0.0f)
            targetAngle = std::acos(clampCos(dot)) - 1.5707964f;
        else
        {
            const float clamped = clampCos(dot);
            const float directionSign = clamped > 0.0f ? 1.0f : (clamped < 0.0f ? -1.0f : 0.0f);
            targetAngle = directionSign * 1.5707964f;
        }
    }
    else
    {
        Vec2 point = {0.0f, 0.0f};
        ok = moveTo->update(point, done) != 0;
        if (!ok)
        {
            done = true;
            return true;
        }
        direction = {point.x - position.x, point.y - position.y};
        const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 1e-06f)
        {
            const float inverse = 1.0f / length;
            direction.x *= inverse;
            direction.y *= inverse;
        }
        targetAngle = std::acos(clampCos(direction.x * normal.x + direction.y * normal.y)) - 1.5707964f;
        const float angleSign = targetAngle > 0.0f ? 1.0f : (targetAngle < 0.0f ? -1.0f : 0.0f);
        const float absoluteAngle = targetAngle < 0.0f ? -targetAngle : targetAngle;
        const float facingDot = facing.x * direction.x + facing.y * direction.y;
        vehiclecontrol::normalizeVec2(facing);
        if (absoluteAngle > 0.87266463f || facingDot < 0.0f)
            targetAngle = angleSign * 0.017453292f * 90.0f;
        const float maximum = info->getMobile()->getMaxSpeed();
        targetSpeed = moveTo->m_maxSpeed > maximum ? maximum : moveTo->m_maxSpeed;
        if (facingDot < 0.0f)
            targetSpeed = 0.0f;
    }
    io::PlayerInput* const input = bot->getInput();
    float turnInput = (input->mappingFlags & 1ULL) ? input->inputs[0] : 0.0f;
    float throttle = (input->mappingFlags & 8ULL) ? input->inputs[3] : 0.0f;
    const float facingDot = facing.x * moveTo->m_state68.y + facing.y * moveTo->m_state68.z;
    updateAngle(targetAngle, std::acos(clampCos(facingDot)), turnInput, 1.0f, 1.0f, 0.8f);
    updateLinear(targetSpeed, signedSpeed, moveTo->m_state68.x, throttle, -0.9f, 1.0f, 1.0f);
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
