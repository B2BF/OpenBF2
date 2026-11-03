// EntryPlaneMoveToDirection (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2C2950 (__tcf_0 0x2C29B0, _GLOBAL__I 0x2C29E0): <iostream>'s Init object only.
#include <iostream>

#include "EntryPlaneMoveToDirection.h"

#include "ControlFunctions.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo3dDirection.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../../Core/Debug.h"
#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2C0F60 (C1; C2 0x2C0F40).
EntryPlaneMoveToDirection::EntryPlaneMoveToDirection()
{
}

// ELF 0x2C0FA0 (D1; D2 0x2C0F80, D0 0x2C0FC0).
EntryPlaneMoveToDirection::~EntryPlaneMoveToDirection()
{
}

// ELF 0x2C2A00.
int EntryPlaneMoveToDirection::getTargetId()
{
    return 8;
}

// ELF 0x2C1000.
bool EntryPlaneMoveToDirection::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo3dDirection* const moveTo = static_cast<BAPAMoveTo3dDirection*>(script);
    Mat4 transform = {{1.0f, 0.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f, 0.0f,
                       0.0f, 0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 0.0f, 1.0f}};
    Mat4 transpose = transform;
    io::PlayerInput* const input = bot->getInput();
    float yawInput = (input->mappingFlags & 1ULL) ? input->inputs[0] : 0.0f;
    float pitchInput = (input->mappingFlags & 2ULL) ? input->inputs[1] : 0.0f;
    float rollInput = (input->mappingFlags & 4ULL) ? input->inputs[2] : 0.0f;
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
        // shipped defect: the completed statement continues through the controls.
    }
    else
        done = false;
    transform = info->getTransformation();
    const Vec3 right = {transform.m[0], transform.m[1], transform.m[2]};
    const Vec3 up = {transform.m[4], transform.m[5], transform.m[6]};
    const Vec3 forward = {transform.m[8], transform.m[9], transform.m[10]};
    transpose.setTranspose(transform);
    const Vec3 position = info->getPosition();
    info->getMobile()->getSpeedMagnitude();
    Vec3 point = moveTo->m_target;
    const Vec3& speed = info->getMobile()->getSpeed();
    const Vec3 lookAhead = {speed.x * 10.0f, speed.y * 10.0f, speed.z * 10.0f};
    const float altitude = info->getAltitude(lookAhead);
    moveTo->m_currentAltitude = altitude;
    const float heightError = moveTo->m_altitude - altitude;
    const float desiredHeight = heightError + position.y;
    point.y = desiredHeight > point.y ? desiredHeight : point.y;
    Vec2 position2, point2, forward2, targetDirection2, normal, targetNormal;
    vec3ToVec2XZ(position, position2);
    vec3ToVec2XZ(point, point2);
    vec3ToVec2XZ(forward, forward2);
    vec3ToVec2XZ(moveTo->m_direction, targetDirection2);
    getNormal(forward2, normal);
    vehiclecontrol::normalizeVec2(normal);
    getNormal(targetDirection2, targetNormal);
    vehiclecontrol::normalizeVec2(targetNormal);
    const float radius = 80.0f;
    const Vec2 targetCentre = {radius * targetNormal.x + point2.x, targetNormal.y * radius + point2.y};
    const Vec2 centreDifference = {targetCentre.x - (normal.x * radius + position2.x),
                                  targetCentre.y - (normal.y * radius + position2.y)};
    const float centreDistance = std::sqrt(centreDifference.x * centreDifference.x + centreDifference.y * centreDifference.y);
    const float pi = 3.1415927f;
    const float halfPi = 1.5707964f;
    const float twoPi = 6.2831855f;
    // [INFERRED] inline comparison shapes. The cosine clamp maps unordered input to -1.
    const auto clampUnit = [](float value) {
        return -1.0f > value ? -1.0f : (value > 1.0f ? 1.0f : value);
    };
    const auto clampCos = [](float value) {
        const float upper = value > 1.0f ? 1.0f : value;
        return upper > -1.0f ? upper : -1.0f;
    };
    Vec3 direction;
    float yawTarget;
    // shipped defect: the height slope uses world-position magnitude, not target distance.
    const auto perpendicular = [&]() {
        return Vec3{normal.x, (point.y - position.y) /
            std::sqrt(position.x * position.x + position.y * position.y + position.z * position.z), normal.y};
    };
    if (radius * 0.075f > centreDistance)
    {
        direction = perpendicular();
        moveTo->m_state = 2;
        yawTarget = halfPi;
        Debug(DebugTypeError, "AI/AIDLL/AI/AIPlanInterpreter/EntryPlaneMoveToDirection.cpp", 210, "")
            << "We don't think that we are using this code..., check yaw angle.";
    }
    else
    {
        float theta = std::acos(clampUnit(clampUnit(std::fabs(centreDifference.y) / centreDistance)));
        if (centreDifference.x < 0.0f)
            theta = twoPi - theta;
        float normalTheta = std::acos(clampUnit(clampUnit(normal.y)));
        if (normal.x < 0.0f)
            normalTheta = twoPi - normalTheta;
        float difference = theta - normalTheta;
        if (difference < 0.0f)
            difference = -difference;
        if (difference > pi)
            difference = twoPi - difference;
        else if (difference < 0.0f)
            difference += twoPi;
        if (moveTo->m_state == 1 || 0.18325958f > difference)
        {
            const float sine = std::sin(theta);
            const Vec3 turnPoint = {targetCentre.x - sine * radius, point.y,
                                    targetCentre.y - radius * std::cos(theta)};
            direction = {turnPoint.x - position.x, turnPoint.y - position.y, turnPoint.z - position.z};
            Vec2 direction2;
            vec3ToVec2XZ(direction, direction2);
            vehiclecontrol::normalizeVec2(direction2);
            const float dot = normal.x * direction2.x + direction2.y * normal.y;
            yawTarget = std::acos(clampCos(dot)) - halfPi;
            vehiclecontrol::normalizeVec3(direction);
            moveTo->m_state = 1;
        }
        else
        {
            direction = perpendicular();
            yawTarget = halfPi;
        }
    }
    vehiclecontrol::normalizeVec3(direction);
    float rollTarget = -(std::acos(clampCos(right.y)) - halfPi);
    const float yawChange = std::acos(clampCos(right.x * moveTo->m_forward.x + right.y * moveTo->m_forward.y + right.z * moveTo->m_forward.z)) - halfPi;
    const float rollChange = rollTarget + (std::acos(clampCos(right.y)) - halfPi);
    const float pitchChange = std::acos(clampCos(up.x * moveTo->m_forward.x + up.y * moveTo->m_forward.y + up.z * moveTo->m_forward.z)) - halfPi;
    const Vec3 localUp = {transpose.m[0] * 0.0f + transpose.m[4] + transpose.m[8] * 0.0f,
                          transpose.m[1] * 0.0f + transpose.m[5] + transpose.m[9] * 0.0f,
                          transpose.m[2] * 0.0f + transpose.m[6] + transpose.m[10] * 0.0f};
    const float currentPitch = std::asin(clampUnit(localUp.z));
    float pitchTarget;
    const auto dot = [](const Vec3& a, const Vec3& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };
    if (heightError > moveTo->m_altitude * 0.1f)
    {
        pitchTarget = -(0.43633232f - currentPitch);
        yawTarget = 0.0f;
        pitchTarget /= 1.0001f - std::sin(rollTarget);
    }
    else if (!(0.08726646f > std::fabs(yawTarget)))
    {
        if (!(0.21816616f > std::fabs(yawTarget)))
        {
            const float angle = std::atan(dot(direction, right));
            yawTarget = 0.0f;
            pitchTarget = -halfPi;
            const float upDot = dot(up, direction);
            const float absoluteUpDot = upDot < 0.0f ? -upDot : upDot;
            rollTarget = (angle / absoluteUpDot) * 0.9f + 0.1f * rollTarget;
        }
        else
        {
            const float angle = std::acos(clampCos(dot(direction, up))) - halfPi;
            pitchTarget = -(0.43633232f - currentPitch) / (1.0001f - std::sin(rollTarget));
            pitchTarget = pitchTarget > angle ? pitchTarget : angle;
            const float rightAngle = std::acos(clampCos(dot(direction, right))) - halfPi;
            rollTarget = rightAngle * -0.1f + rollTarget * 0.9f;
        }
    }
    else
    {
        const float absoluteZ = direction.z < 0.0f ? -direction.z : direction.z;
        const float targetPitch = std::atan(direction.y / absoluteZ);
        const float forwardAbsoluteZ = forward.z < 0.0f ? -forward.z : forward.z;
        const float facingPitch = std::atan(forward.y / forwardAbsoluteZ);
        const float pitchDifference = targetPitch - facingPitch;
        const float pitchLimit = (1.0f - std::cos(std::fabs(rollTarget))) * 1.2f * halfPi + (0.43633232f - currentPitch);
        pitchTarget = -(pitchLimit > pitchDifference ? pitchDifference : pitchLimit);
        const float rollSign = rollTarget > 0.0f ? 1.0f : (rollTarget < 0.0f ? -1.0f : 0.0f);
        rollTarget = rollSign * rollTarget * rollTarget;
        const float yawSign = yawTarget > 0.0f ? 1.0f : (yawTarget < 0.0f ? -1.0f : 0.0f);
        yawTarget = yawSign * yawTarget * yawTarget;
    }
    updateAngle(yawTarget, yawChange, yawInput, 1.0f, 1.0f, 1.5f);
    updateAngle(pitchTarget, pitchChange, pitchInput, -1.0f, 1.0f, 1.5f);
    updateAngle(rollTarget, rollChange, rollInput, -0.5f, 1.0f, 1.5f);
    io::PlayerInput* output = bot->getInput();
    if (output->mappingFlags & 1ULL)
        output->inputs[0] = yawInput;
    bot->getInput()->setInput(static_cast<io::PlayerInputMap>(1), pitchInput);
    bot->getInput()->setInput(static_cast<io::PlayerInputMap>(2), rollInput);
    bot->getInput()->setInput(static_cast<io::PlayerInputMap>(3), 1.0f);
    ok = true;
    moveTo->m_forward = forward;
    moveTo->m_up = up;
    moveTo->m_right = right;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
