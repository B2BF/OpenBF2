// Helicopter attitude, height and obstacle-avoidance controls.

// static init: ELF 0x2CB150 (__tcf_0 0x2CB1B0, _GLOBAL__I 0x2CB1E0): <iostream>'s Init object only.
#include <iostream>

#include "HeliControl.h"

#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIPathFinding/AerialHeightMap/AerialHeightMap.h"
#include "VehicleControlInline.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include <cmath>
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// [INFERRED] Names for inline scalar/vector operations in the control bodies.
float clampHeliInput(float value, float minimum, float maximum)
{
    if (value < minimum)
        return minimum;
    return value > maximum ? maximum : value;
}

Vec3 rotateHeliVector(const Mat4& matrix, const Vec3& v)
{
    // The retail expansions (moveToPoint 0x2C9A30..0x2C9A7D, aimAtDirection 0x2CA8CD..0x2CA91C) add the
    // x and y products before the z product in every component.
    return Vec3{matrix.m[0] * v.x + matrix.m[4] * v.y + matrix.m[8] * v.z,
                matrix.m[1] * v.x + matrix.m[5] * v.y + matrix.m[9] * v.z,
                matrix.m[2] * v.x + matrix.m[6] * v.y + matrix.m[10] * v.z};
}

} // namespace

bool avoidRight(Bot* bot, float& clearance);

// ELF 0x2C8A80
void HeliControl::quickyMoveForward(Bot* bot, float heightDifference, const Vec3& rotationalSpeed)
{
    static float maxHeightDiff = 10.0f;
    static float wantedPitchAtMaxHeight = -0.45f;
    static float wantedPitchAtMinHeight = -0.1f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    io::PlayerInput* input = bot->getInput();
    input->setInput(information->getControlInfo()->getDriveThrottleControl(), 1.0f);
    const float factor = clampHeliInput(heightDifference / maxHeightDiff, -1.0f, 1.0f);
    keepPitchAt(bot, wantedPitchAtMinHeight + (wantedPitchAtMaxHeight - wantedPitchAtMinHeight) *
                    (1.0f - (factor + 1.0f) * 0.5f),
                1.0f, 0.3f, rotationalSpeed, 0.0f);
}

// ELF 0x2C8D00
int HeliControl::gotoHeight(Bot* bot, float height, const Vec3& rotationalSpeed)
{
    static float maxHeightDiff = 5.0f;
    static float wantedElevationAtMaxHeightDiff = 5.0f;
    static float maxElevationDiff = 2.0f;
    static float minThrottleInput = 0.0f;
    static float maxAngle = 0.3f;
    static float wantedRotationSpeedAtMaxAngle = 0.5f;
    static float maxRotationSpeedDiff = 0.4f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    io::PlayerInput* input = bot->getInput();
    if (!bot->isAirBorn() && information->getAltitude() > 5.0f)
        bot->event_airborn();
    const Mat4& transformation = information->getTransformation();
    IPIMobile* mobile = information->getMobile();
    IPIControlInfo* controls = information->getControlInfo();
    const float elevation = clampHeliInput((height - transformation.m[13]) / maxHeightDiff,
                                          -1.0f, 1.0f) * wantedElevationAtMaxHeightDiff;
    const Vec3& speed = mobile->getSpeed();
    float throttle = clampHeliInput(clampHeliInput((elevation - speed.y) / maxElevationDiff,
                                                  -1.0f, 1.0f) + minThrottleInput, 0.0f, 1.0f);
    if (elevation > 0.0f && speed.y < 0.0f && transformation.m[5] > 0.0f)
        throttle = 1.0f;
    input->setInput(controls->getDriveThrottleControl(), throttle);
    float difference = (clampHeliInput(transformation.m[9] / maxAngle, -1.0f, 1.0f) *
                        wantedRotationSpeedAtMaxAngle - rotationalSpeed.x) / maxRotationSpeedDiff;
    float value = difference * difference;
    if (value > 1.0f)
        value = 1.0f;
    if (difference < 0.0f)
        value = -value;
    input->setInput(controls->getDrivePitchControl(), value);
    difference = (-clampHeliInput(transformation.m[1] / maxAngle, -1.0f, 1.0f) *
                  wantedRotationSpeedAtMaxAngle - rotationalSpeed.z) / maxRotationSpeedDiff;
    value = difference * difference;
    if (value > 1.0f)
        value = 1.0f;
    if (difference > 0.0f)
        value = -value;
    input->setInput(controls->getDriveRollControl(), value);
    input->setInput(controls->getDriveTurnControl(), 0.0f);
    return 1;
}

// ELF 0x2C9060
void HeliControl::strafeTo(Bot* bot, const Vec3& target, const Vec3& rotationalSpeed)
{
    static float maxDist = 20.0f;
    static float maxValue = 0.25f;
    static float maxInput = 0.15f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    const Mat4& transformation = information->getTransformation();
    const Vec3 delta = {target.x - transformation.m[12], target.y - transformation.m[13],
                        target.z - transformation.m[14]};
    const float side = delta.x * transformation.m[0] + delta.y * transformation.m[1] +
                       delta.z * transformation.m[2];
    const float forward = delta.x * transformation.m[8] + delta.y * transformation.m[9] +
                          delta.z * transformation.m[10];
    keepRollAt(bot, -clampHeliInput(side / maxDist, -1.0f, 1.0f) * maxValue,
               maxInput, 0.2f, rotationalSpeed);
    keepPitchAt(bot, -clampHeliInput(forward / maxDist, -1.0f, 1.0f) * maxValue,
                maxInput, 0.2f, rotationalSpeed, 0.0f);
}

// ELF 0x2C9220
void HeliControl::keepRollAt(Bot* bot, float dot, float maxInput, float maxSpeed,
                           const Vec3& rotationalSpeed)
{
    keepDotAt(bot, 2, dot, maxInput, maxSpeed, rotationalSpeed, 0.0f);
}

// ELF 0x2C9280
void HeliControl::keepPitchAt(Bot* bot, float dot, float maxInput, float maxSpeed,
                            const Vec3& rotationalSpeed, float feedForward)
{
    keepDotAt(bot, 0, dot, maxInput, maxSpeed, rotationalSpeed, feedForward);
}

// ELF 0x2C92D0
void HeliControl::keepDotAt(Bot* bot, int axis, float dot, float maxInput, float maxSpeed,
                          const Vec3& rotationalSpeed, float feedForward)
{
    static float maxDiff = 0.02f;
    static float maxAngleSpeedDiff = 0.1f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    io::PlayerInput* input = bot->getInput();
    IPIControlInfo* controls = information->getControlInfo();
    const Mat4& transformation = information->getTransformation();
    const float currentDot = axis == 0 ? transformation.m[9] : transformation.m[1];
    const float speed = axis == 0 ? rotationalSpeed.x : rotationalSpeed.z;
    float wantedSpeed = clampHeliInput((dot - currentDot) / maxDiff, -1.0f, 1.0f);
    if (axis == 0)
        wantedSpeed = -wantedSpeed;
    float value = clampHeliInput(((wantedSpeed * maxSpeed - feedForward) - speed) /
                                maxAngleSpeedDiff, -1.0f, 1.0f) * maxInput;
    if (axis == 2)
        input->setInput(controls->getDriveRollControl(), -value);
    else
        input->setInput(controls->getDrivePitchControl(), value);
}

// ELF 0x2C9450
void HeliControl::limitSpeedTo(Bot* bot, float speed, float limit, float maxRotationSpeed,
                             const Vec3& rotationalSpeed)
{
    static float minPitch = 0.0f;
    static float additional = 0.3f;
    static float maxInput = 1.0f;
    static float maxPitch = 0.5f;
    if (limit > std::fabs(speed))
        return;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    information->getTransformation();
    float factor = (std::fabs(speed) - limit) / 5.0f;
    float sign = speed > 0.0f ? 1.0f : (speed < 0.0f ? -1.0f : 0.0f);
    if (factor > 1.0f)
        factor = 1.0f;
    keepPitchAt(bot, factor * factor * (limit == 0.0f ? maxPitch : additional) * sign + minPitch,
                maxInput, maxRotationSpeed, rotationalSpeed, 0.0f);
}

// ELF 0x2C95D0
float HeliControl::keepHeight(Bot* bot, float clearance, float minimumHeight,
                             const Vec3& rotationalSpeed)
{
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    const Vec3& speed = information->getMobile()->getSpeed();
    const Mat4& transformation = information->getTransformation();
    (void)std::sqrt(speed.x * speed.x + speed.y * speed.y + speed.z * speed.z);
    float highest = -999999.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float time = static_cast<float>(i);
        const Vec3 position = {speed.x * time + transformation.m[12],
                               speed.y * time + transformation.m[13],
                               speed.z * time + transformation.m[14]};
        const float height = AerialHeightMap::m_instance->getGroundHeight(position);
        if (height > highest)
            highest = height;
    }
    float height = clearance + highest;
    if (!(height > minimumHeight))
        height = minimumHeight;
    gotoHeight(bot, height, rotationalSpeed);
    return height - transformation.m[13];
}

// ELF 0x2C9760
int HeliControl::moveToPoint(Bot* bot, const Vec3& point, float height, const Vec3& acceleration)
{
    static float minAltitudeDiff = 5.0f;
    static float maxAltitudeDiff = 15.0f;
    static float maxDistanceForStrafe = 20.0f;
    static float minDist = 5.0f;
    static float speedAtMax = 5.0f;
    static float speedAtMin = 3.0f;
    static float maxSpeed = 35.0f;
    static float altitudeFactorTweak = 0.9f;
    static float maxSpeedDiff = 10.0f;
    static float maxAcceleration = 15.0f;
    static float maxAccelerationDiff = 6.0f;
    static float maxDiff = 0.2f;
    static float maxInput = 1.0f;
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    io::PlayerInput* const input = bot->getInput();
    if (!info)
        return false;
    IPIMobile* const mobile = info->getMobile();
    const Vec3& speed = mobile->getSpeed();
    const Mat4& transform = info->getTransformation();
    IPIControlInfo* const controls = info->getControlInfo();
    const Vec3& position = info->getPosition();
    Vec3 direction = {point.x - position.x, 0.0f, point.z - position.z};
    const float distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
    if (distance < 0.001f)
        direction = {0.0f, 0.0f, 1.0f};
    else
    {
        const float inverse = 1.0f / distance;
        direction.x *= inverse;
        direction.y *= inverse;
        direction.z *= inverse;
    }
    const float altitude = AerialHeightMap::m_instance->getAltitude(info->getPosition());
    Mat4 inverseTransform = {{1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 1.0f}};
    inverseTransform.setInverse(transform);
    Vec3 facing = info->getFaceing();
    facing.y = 0.0f;
    vehiclecontrol::normalizeVec3(facing);
    const float forwardAcceleration = facing.x * acceleration.x + facing.y * acceleration.y + facing.z * acceleration.z;
    const float forwardSpeed = facing.x * speed.x + facing.y * speed.y + facing.z * speed.z;
    const Vec3& rotation = mobile->getRotationalSpeed();
    const Vec3 localRotation = {
        rotation.x * inverseTransform.m[0] + rotation.y * inverseTransform.m[4] + rotation.z * inverseTransform.m[8],
        rotation.x * inverseTransform.m[1] + rotation.y * inverseTransform.m[5] + rotation.z * inverseTransform.m[9],
        rotation.x * inverseTransform.m[2] + rotation.y * inverseTransform.m[6] + rotation.z * inverseTransform.m[10]};
    float groundHeight;
    if (avoidCrash(bot, localRotation, groundHeight))
        return true;
    const float clearanceHeight = height + groundHeight;
    const float desiredHeight = clearanceHeight > point.y ? clearanceHeight : point.y;
    gotoHeight(bot, desiredHeight, localRotation);
    const float heightDifference = desiredHeight - transform.m[13];
    float altitudeFactor = 0.0f;
    if (!(minAltitudeDiff > heightDifference))
    {
        altitudeFactor = (heightDifference - minAltitudeDiff) / (maxAltitudeDiff - minAltitudeDiff);
        if (altitudeFactor > 1.0f)
            altitudeFactor = 1.0f;
    }
    if (maxDistanceForStrafe > distance && altitudeFactor < 0.1f)
    {
        float desiredSpeed = speedAtMin;
        if (!(minDist > distance))
            desiredSpeed = ((distance - minDist) / (maxDistanceForStrafe - minDist)) * (speedAtMax - speedAtMin) + speedAtMin;
        if (std::fabs(forwardSpeed) > desiredSpeed)
            limitSpeedTo(bot, forwardSpeed, desiredSpeed, 0.3f, localRotation);
        else
            strafeTo(bot, point, localRotation);
        return true;
    }
    float turn = 0.0f;
    if (altitude > 5.0f)
        turn = turnTowardsDirection(bot, direction, localRotation);
    if (turn > 45.0f)
    {
        limitSpeedTo(bot, forwardSpeed, 5.0f, 0.3f, localRotation);
        return true;
    }
    if (!(altitude > 5.0f))
        return true;
    // [INFERRED] inline clamp expansion; unordered values pass through.
    const auto clamp = [](float value, float low, float high) {
        return low > value ? low : (value > high ? high : value);
    };
    const float distanceFactor = distance / 50.0f < 1.0f ? distance / 50.0f : 1.0f;
    const float desiredSpeed = distanceFactor * ((1.0f - altitudeFactor * altitudeFactorTweak) * maxSpeed);
    const float speedFactor = clamp((desiredSpeed - forwardSpeed) / maxSpeedDiff, -1.0f, 1.0f);
    const float accelerationFactor = clamp((speedFactor * maxAcceleration - forwardAcceleration) / maxAccelerationDiff, -0.1f, 1.0f);
    const float pitchDifference = clamp(accelerationFactor * -0.4f - transform.m[9], -1.0f, 1.0f);
    const float pitchFactor = clamp(pitchDifference / maxDiff, -1.0f, 1.0f);
    const float damping = clamp(4.0f * localRotation.x, -1.2f, 1.2f);
    const float pitchInput = clamp(pitchFactor * -maxInput - damping, -maxInput, maxInput);
    input->setInput(controls->getDrivePitchControl(), pitchInput);
    return true;
}

// ELF 0x2C9F30
bool HeliControl::avoidCrash(Bot* bot, const Vec3& rotationalSpeed, float& ground)
{
    static float tiltLimit = 0.2f;
    static float safetyConstant = 0.08f;
    static bool testAvoidCrash = false;
    static float minDiffToTryToTurn = -10.0f;
    static float roll = -0.2f;
    static float maxInput = 0.5f;
    static float maxRotSpeed = 0.5f;
    static float maxDiff = 10.0f;
    static float minValue = 0.1f;
    static float addition = 0.3f;
    static float speedLimit = 3.0f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    io::PlayerInput* input = bot->getInput();
    IPIMobile* mobile = information->getMobile();
    const Mat4& transformation = information->getTransformation();
    const Vec3& speed = mobile->getSpeed();
    IPIControlInfo* controls = information->getControlInfo();
    ground = -99999.0f;
    const bool tilted = std::fabs(transformation.m[1]) > tiltLimit ||
                         std::fabs(transformation.m[9]) > 0.6f;
    const Vec3 facing = information->getFaceing();
    float forwardSpeed = facing.x * speed.x + facing.y * speed.y + facing.z * speed.z;
    if (!(forwardSpeed > 4.0f))
        forwardSpeed = 4.0f;
    float lowest = 999999.0f;
    if (!tilted)
    {
        const Vec3 currentFacing = information->getFaceing();
        Vec3 direction = {currentFacing.x * forwardSpeed, currentFacing.y * forwardSpeed,
                           currentFacing.z * forwardSpeed};
        if (forwardSpeed > 0.00001f)
        {
            const float inverse = 1.0f / forwardSpeed;
            direction.x *= inverse;
            direction.y *= inverse;
            direction.z *= inverse;
        }
        int count = static_cast<int>((forwardSpeed * safetyConstant * forwardSpeed) / 15.0f) + 2;
        if (count < 6)
            count = 5;
        for (int i = 0; i < count; ++i)
        {
            const float sample = static_cast<float>(i);
            const Vec3 position = {(direction.x * sample) * 15.0f + transformation.m[12],
                                   (direction.y * sample) * 15.0f + transformation.m[13],
                                   (direction.z * sample) * 15.0f + transformation.m[14]};
            const float altitude = AerialHeightMap::m_instance->getAltitude(position);
            if (position.y - altitude > ground)
                ground = position.y - altitude;
            if (lowest > altitude)
                lowest = altitude;
        }
        if (!(lowest < 5.0f && forwardSpeed > 4.0f) && !testAvoidCrash)
            return false;
    }
    if (!(lowest < minDiffToTryToTurn) || tilted)
        keepRollAt(bot, 0.0f, 1.0f, 0.5f, rotationalSpeed);
    else
    {
        input->setInput(static_cast<io::PlayerInputMap>(0), 0.5f);
        float sideClearance;
        const bool right = avoidRight(bot, sideClearance);
        keepRollAt(bot, right ? roll : -roll, maxInput, maxRotSpeed, rotationalSpeed);
        if (sideClearance > -10.0f)
        {
            const float forward = speed.x * transformation.m[8] + speed.y * transformation.m[9] +
                                   speed.z * transformation.m[10];
            limitSpeedTo(bot, forward, 5.0f, 0.3f, rotationalSpeed);
            input->setInput(controls->getDriveThrottleControl(), 1.0f);
            return true;
        }
    }
    const float forward = speed.x * transformation.m[8] + speed.y * transformation.m[9] +
                           speed.z * transformation.m[10];
    float factor = 0.0f;
    if (forward > 0.1f)
    {
        factor = std::fabs(forward) / maxDiff;
        if (factor > 1.0f)
            factor = 1.0f;
    }
    limitSpeedTo(bot, forward, speedLimit, factor * addition + minValue, rotationalSpeed);
    if (transformation.m[9] > 0.0f)
        input->setInput(controls->getDriveThrottleControl(), 1.0f);
    return true;
}

// ELF 0x2CA4B0
bool avoidRight(Bot* bot, float& clearance)
{
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    const Mat4& transformation = information->getTransformation();
    const Vec3 firstFacing = information->getFaceing();
    const Vec3 first = {firstFacing.x * 0.7f, firstFacing.y * 0.7f, firstFacing.z * 0.7f};
    const Mat4& firstTransformation = information->getTransformation();
    const Vec3 right = {0.1f * firstTransformation.m[0] + first.x,
                        0.1f * firstTransformation.m[1] + first.y,
                        0.1f * firstTransformation.m[2] + first.z};
    const Vec3 secondFacing = information->getFaceing();
    const Vec3 second = {secondFacing.x * 0.7f, secondFacing.y * 0.7f, secondFacing.z * 0.7f};
    const Mat4& secondTransformation = information->getTransformation();
    const Vec3 left = {second.x - 0.1f * secondTransformation.m[0],
                       second.y - 0.1f * secondTransformation.m[1],
                       second.z - 0.1f * secondTransformation.m[2]};
    // shipped defect: positive clearances cannot raise either minimum above its initial zero.
    float minimumRight = 0.0f;
    float minimumLeft = 0.0f;
    float sumRight = 0.0f;
    float sumLeft = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        const float sample = static_cast<float>(i);
        const Vec3 position = {(right.x * sample) * 22.0f + transformation.m[12],
                               (right.y * sample) * 22.0f + transformation.m[13],
                               (right.z * sample) * 22.0f + transformation.m[14]};
        const float altitude = AerialHeightMap::m_instance->getAltitude(position);
        sumRight += altitude;
        if (minimumRight > altitude)
            minimumRight = altitude;
    }
    for (int i = 0; i < 12; ++i)
    {
        const float sample = static_cast<float>(i);
        const Vec3 position = {(left.x * sample) * 22.0f + transformation.m[12],
                               (left.y * sample) * 22.0f + transformation.m[13],
                               (left.z * sample) * 22.0f + transformation.m[14]};
        const float altitude = AerialHeightMap::m_instance->getAltitude(position);
        sumLeft += altitude;
        if (minimumLeft > altitude)
            minimumLeft = altitude;
    }
    clearance = sumRight > sumLeft ? minimumRight : minimumLeft;
    return sumRight > sumLeft;
}

// ELF 0x2CA7D0
int HeliControl::aimAtDirection(Bot* bot, const Vec3& direction, float, float clearance,
                               const Vec3& acceleration, float feedForward)
{
    static float minDot = -0.6f;
    static float maxDot = 0.35f;
    static float maxAngleSpeed = 0.2f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    io::PlayerInput* input = bot->getInput();
    if (!information)
        return 0;
    IPIMobile* mobile = information->getMobile();
    const Mat4& transformation = information->getTransformation();
    Mat4 inverse = Mat4::getIdentity();
    inverse.setInverse(transformation);
    const Vec3 rotationalSpeed = rotateHeliVector(inverse, mobile->getRotationalSpeed());
    float ground;
    if (!avoidCrash(bot, rotationalSpeed, ground))
    {
        gotoHeight(bot, ground + 10.0f, rotationalSpeed);
        if (!(direction.y > minDot) || !(maxDot > direction.y))
        {
            Vec3 forward = {transformation.m[8], 0.0f, transformation.m[10]};
            vehiclecontrol::normalizeVec3(forward);
            const Vec3 target = {forward.x * 150.0f + transformation.m[12],
                                 forward.y * 150.0f + transformation.m[13],
                                 forward.z * 150.0f + transformation.m[14]};
            moveToPoint(bot, target, clearance, acceleration);
        }
        else
        {
            const float turn = turnTowardsDirection(bot, direction, rotationalSpeed);
            if (turn < 10.0f && std::fabs(rotationalSpeed.y) < 0.1f)
                keepPitchAt(bot, direction.y, 1.0f, maxAngleSpeed, rotationalSpeed, feedForward);
        }
    }
    else
    {
        IPIControlInfo* controls = information->getControlInfo();
        if (transformation.m[9] > -0.15f && mobile->getSpeedMagnitude() > 2.0f)
            input->setInput(controls->getDriveThrottleControl(), 1.0f);
    }
    return 1;
}

// ELF 0x2CAC20
float HeliControl::turnTowardsDirection(Bot* bot, const Vec3& direction, const Vec3& rotationalSpeed)
{
    static bool dontTurn = false;
    static float maxAngleVelocity = 1.0f;
    static float maxAngleVelocityDiff = 0.1f;
    static float maxInput = 1.0f;
    Information* information = informationManager->getInformation(bot->getCurrentEquipment());
    IPIControlInfo* controls = information->getControlInfo();
    io::PlayerInput* input = bot->getInput();
    if (dontTurn)
        return 0.0f;
    const Mat4& transformation = information->getTransformation();
    float z, x, yaw;
    getRotationZXY(transformation, z, x, yaw);
    if (yaw < 0.0f)
        yaw += 360.0f;
    Mat4 look = Mat4::getIdentity();
    const Vec3 up = {0.0f, 1.0f, 0.0f};
    const Vec3 target = {direction.x + transformation.m[12], direction.y + transformation.m[13],
                         direction.z + transformation.m[14]};
    const Vec3& position = *reinterpret_cast<const Vec3*>(&transformation.m[12]);
    calcLookAtMatrix(look, position, target, up);
    float targetZ, targetX, targetYaw;
    getRotationZXY(look, targetZ, targetX, targetYaw);
    if (targetYaw < 0.0f)
        targetYaw += 360.0f;
    float difference = targetYaw - yaw;
    if (difference < -180.0f)
        difference += 360.0f;
    else if (difference > 180.0f)
        difference -= 360.0f;
    const float wantedSpeed = -clampHeliInput(difference / 30.0f, -1.0f, 1.0f) * maxAngleVelocity;
    const float value = clampHeliInput((wantedSpeed - rotationalSpeed.y) / maxAngleVelocityDiff,
                                      -1.0f, 1.0f);
    const io::PlayerInputMap turnControl = controls->getDriveTurnControl();
    input->setInput(turnControl, value * maxInput);
    return std::fabs(difference);
}

// ELF 0x2CAF00 (Ghidra). Moving releases inputs 0, 2, 1 and 3; look 4 and 5; trigger 8, 31,
// 23..28, 14..16, 38, 37, 10, 32 and 47. Returns 1.
int HeliControl::resetControls(Bot* bot, bool move, bool look, bool trigger)
{
    io::PlayerInput* const input = bot->getInput();
    if (move)
    {
        input->setInput(static_cast<io::PlayerInputMap>(0), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(2), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(1), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(3), 0.0f);
    }
    if (look)
    {
        input->setInput(static_cast<io::PlayerInputMap>(4), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(5), 0.0f);
    }
    if (trigger)
    {
        input->setInput(static_cast<io::PlayerInputMap>(8), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(31), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(23), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(24), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(25), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(26), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(27), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(28), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(14), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(15), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(16), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(38), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(37), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(10), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(32), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(47), 0.0f);
    }
    return 1;
}

} // namespace ai
} // namespace hfe
} // namespace dice
