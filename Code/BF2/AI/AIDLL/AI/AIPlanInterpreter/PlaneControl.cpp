// Fixed-wing flight and attitude controls, recovered from libaidll.so.

// static init: ELF 0x2D0700 (__tcf_0 0x2D0760, _GLOBAL__I 0x2D0790): <iostream>'s Init object only.
#include <iostream>

#include "PlaneControl.h"

#include "ControlFunctions.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIStrategicMap/AerialHeightMap.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include <cmath>
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {
// [CHOSEN, not recovered] Local packaging of the inline vector/scalar operations in
// ELF 0x2CDB00, 0x2CE3C0, 0x2CE6D0 and 0x2CF9F0.
float clampPlane(float value, float minimum, float maximum)
{
    return value < minimum ? minimum : (value > maximum ? maximum : value);
}

float planeSign(float value)
{
    return value > 0.0f ? 1.0f : (value < 0.0f ? -1.0f : 0.0f);
}

float dotPlane(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 crossPlane(const Vec3& a, const Vec3& b)
{
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

Vec3 rotatePlane(const Mat4& matrix, const Vec3& value)
{
    return Vec3{matrix.m[0] * value.x + matrix.m[4] * value.y + matrix.m[8] * value.z,
                matrix.m[1] * value.x + matrix.m[5] * value.y + matrix.m[9] * value.z,
                matrix.m[2] * value.x + matrix.m[6] * value.y + matrix.m[10] * value.z};
}

Mat4 planeIdentity()
{
    Mat4 matrix = {};
    matrix.m[0] = matrix.m[5] = matrix.m[10] = matrix.m[15] = 1.0f;
    return matrix;
}

float planeMappedInput(io::PlayerInput* input, io::PlayerInputMap control)
{
    const int index = static_cast<int>(control);
    if (index < 64 && ((input->mappingFlags >> (static_cast<unsigned int>(index) & 63)) & 1ULL))
        return input->inputs[index];
    return 0.0f;
}

float planeImpulse(float value)
{
    if (value < 0.0f && value > -0.15f)
        return -0.15f;
    if (value > 0.0f && value < 0.15f)
        return 0.15f;
    return value;
}

float planeLogResponse(float value)
{
    return value < 0.0f ? -std::log10(1.0f - value * 9.0f) : std::log10(1.0f + value * 9.0f);
}
}

// ELF 0x2CDB00
bool PlaneControl::towardsPoint(Bot* bot, Information* equipment, Vec3 target, float,
                                const Vec3& previousForward, const Vec3& previousRight,
                                const Vec3& previousUp, float clearance, io::PlayerInput* input)
{
    IPIMobile* mobile = equipment->getMobile();
    IPIControlInfo* controls = equipment->getControlInfo();
    const Vec3 position = equipment->getPosition();
    const Vec3 velocity = mobile->getSpeed();
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    const Mat4& transformation = equipment->getTransformation();
    const float dx = transformation.m[12] - target.x;
    const float dz = transformation.m[14] - target.z;
    const float horizontalDistanceSq = dx * dx + dz * dz;
    if (horizontalDistanceSq < 10000.0f)
    {
        const float ground = AerialHeightMap::m_instance->getGroundHeight(target) + clearance;
        const float floor = ground > transformation.m[13] ? ground : transformation.m[13];
        const float raised = (target.y - floor) * horizontalDistanceSq * 0.0001f + floor;
        if (raised > target.y)
            target.y = raised;
    }
    Vec3 prediction = velocity;
    const float speed = mobile->getSpeedMagnitude();
    if (speed > 0.00001f)
    {
        const float inverse = 1.0f / speed;
        prediction.x *= inverse;
        prediction.y *= inverse;
        prediction.z *= inverse;
    }
    prediction.x *= 100.0f;
    prediction.y *= 100.0f;
    prediction.z *= 100.0f;
    const bool takingOff = !bot->isAirBorn();
    if (takingOff && equipment->getAltitude() > 50.0f && speed / mobile->getMaxSpeed() > 0.5f)
        bot->event_airborn();
    float altitude = equipment->getAltitude(prediction);
    if (prediction.y < -50.0f)
    {
        Vec3 horizontal = {velocity.x, 0.0f, velocity.z};
        vehiclecontrol::normalizeVec3(horizontal);
        horizontal.x *= 100.0f;
        horizontal.y *= 100.0f;
        horizontal.z *= 100.0f;
        const float horizontalAltitude = equipment->getAltitude(horizontal);
        if (!(horizontalAltitude > altitude))     // 0x2CDDEA..0x2CDDF4: unordered keeps the sample
            altitude = horizontalAltitude;
    }
    if (takingOff)
    {
        target = static_cast<BotMain*>(bot)->getTakeOffPosition();
        const Vec3& zero = Vec3::getZero();
        if (std::fabs(target.x - zero.x) <= 1.1920929e-07f
            && std::fabs(target.y - zero.y) <= 1.1920929e-07f
            && std::fabs(target.z - zero.z) <= 1.1920929e-07f)
        {
            Vec3 forward = equipment->getFaceing();
            forward.y = 0.0f;
            vehiclecontrol::normalizeVec3(forward);
            target = Vec3{forward.x * 500.0f + position.x,
                          forward.y * 500.0f + position.y + 200.0f,
                          forward.z * 500.0f + position.z};
            static_cast<BotMain*>(bot)->setTakeOffPosition(target);
        }
    }
    direction = Vec3{target.x - position.x, target.y - position.y, target.z - position.z};
    const float distance = std::sqrt(dotPlane(direction, direction));
    if (distance > 0.0001f)
    {
        const float inverse = 1.0f / distance;
        direction.x *= inverse;
        direction.y *= inverse;
        direction.z *= inverse;
    }
    else
    {
        const Mat4& current = equipment->getTransformation();
        direction = Vec3{current.m[8], current.m[9], current.m[10]};
    }
    const float heightError = clearance - altitude;
    IAITerrain* terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    if (!takingOff && terrain->isInsideCombatArea(bot->getPlayer()))
    {
        bool reverse = direction.y < -0.7f;
        if (!reverse && horizontalDistanceSq < 250000.0f)
        {
            const Mat4& current = equipment->getTransformation();
            reverse = direction.x * current.m[8] + current.m[9] * direction.y
                    + direction.z * current.m[10] < 0.5f;
        }
        if (reverse)
        { direction.x = -direction.x; direction.z = -direction.z; }
    }
    return towardsDirection(direction, distance, input, transformation, heightError, clearance,
                             false, previousForward, previousRight, previousUp, mobile, controls,
                             1.0f, speed, false);
}

// ELF 0x2CE3C0
bool PlaneControl::aimAtDirection(Bot* bot, Information* equipment, const Vec3& wanted,
                                  float distance, float throttle, float clearance,
                                  const Vec3& previousForward, const Vec3& previousRight,
                                  const Vec3& previousUp, io::PlayerInput* input)
{
    IPIMobile* mobile = equipment->getMobile();
    if (!mobile)
        return false;
    const Mat4& transformation = equipment->getTransformation();
    const Vec3& velocity = mobile->getSpeed();
    const float speed = std::sqrt(dotPlane(velocity, velocity));
    float lowest = 999999.0f;
    for (int i = 0; i < 5; ++i)
    {
        const float step = static_cast<float>(i);
        const Vec3 sample = {step * velocity.x * 0.25f + transformation.m[12],
                             velocity.y * step * 0.25f + transformation.m[13],
                             velocity.z * step * 0.25f + transformation.m[14]};
        const float aboveGround = sample.y - AerialHeightMap::m_instance->getGroundHeight(sample);
        if (lowest > aboveGround)
            lowest = aboveGround;
    }
    const float heightError = (clearance - lowest) + (clearance - lowest);
    IPIControlInfo* controls = equipment->getControlInfo();
    const bool takingOff = !bot->isAirBorn();
    Vec3 direction = wanted;
    if (takingOff)
    {
        direction.y = 0.3333f;
        if (equipment->getAltitude() > 50.0f && speed / mobile->getMaxSpeed() > 0.5f)
            bot->event_airborn();
    }
    const Mat4& current = equipment->getTransformation();
    return towardsDirection(direction, distance, input, current, heightError, clearance, takingOff,
                             previousForward, previousRight, previousUp, mobile, controls,
                             throttle, speed, true);
}

// ELF 0x2CE6D0
bool PlaneControl::towardsDirection(const Vec3& wanted, float, io::PlayerInput* input,
                                    const Mat4& transformation, float heightError, float clearance,
                                    bool takingOff, const Vec3&, const Vec3&, const Vec3&,
                                    IPIMobile* mobile, IPIControlInfo* controls, float throttle,
                                    float speed, bool aiming)
{
    static bool useAngle = true; // ELF .data 0x4BDD44; only read at 0x2CF088.
    const Vec3 up = {transformation.m[4], transformation.m[5], transformation.m[6]};
    const Vec3 right = {transformation.m[0], transformation.m[1], transformation.m[2]};
    const Vec3 forward = {transformation.m[8], transformation.m[9], transformation.m[10]};
    Vec3 direction = wanted;
    vehiclecontrol::normalizeVec3(direction);
    const Vec3 worldUp = {0.0f, 1.0f, 0.0f};
    Vec3 levelRight = crossPlane(worldUp, forward);
    vehiclecontrol::normalizeVec3(levelRight);
    const Vec3 levelUp = crossPlane(forward, levelRight);
    const bool behind = dotPlane(forward, direction) < 0.0f;
    Mat4 inverse = planeIdentity();
    inverse.setInverse(transformation);
    const Vec3 rotation = rotatePlane(inverse, mobile->getRotationalSpeed());
    const float upright = dotPlane(up, levelUp);
    float bank = dotPlane(right, levelUp);
    const float ratio = heightError / (clearance - 10.0f);
    const float altitudeFactor = clampPlane(planeSign(ratio) * ratio * ratio, 0.0f, 1.0f);
    const float blend = 1.0f - altitudeFactor;
    float pitch = altitudeFactor + altitudeFactor;
    const float speedFraction = clampPlane(speed / 43.0f, 0.0f, 1.0f);
    const float exponential = std::exp(clampPlane((1.0f - speedFraction) * 2.302585125f, -10.0f, 10.0f));
    const float climb = ((exponential - 1.0f) / 9.0f) * -0.833f + 0.333f;
    const float maxClimb = controls->getMaxClimbAngle();
    const float climbLimit = clampPlane(climb, -1.0f, maxClimb);
    float pitchLimit = 1.0f;
    if (levelUp.y > 0.0f)
    {
        if (forward.y > 0.0f)
            pitchLimit = climbLimit < 0.0f ? (1.0f - levelUp.y) * climbLimit : climbLimit * levelUp.y;
        else
            pitchLimit = climbLimit / levelUp.y;
    }
    if (!(throttle > pitch))
        throttle = pitch;
    const float wantedPitch = dotPlane(levelUp, direction);
    if (!(wantedPitch > pitchLimit))
        pitchLimit = wantedPitch;
    pitch += pitchLimit * blend;
    float yaw = 0.0f;
    if (!takingOff)
    {
        const float wantedYaw = dotPlane(levelRight, direction);
        yaw = behind ? planeSign(wantedYaw) : wantedYaw;
    }
    if (aiming && pitch < 0.0f && dotPlane(forward, direction) > 0.9f && yaw < 0.1f)
        pitch = -clampPlane(std::log10(1.0f - pitch * 18.0f), 0.3f, 1.0f);
    if (climbLimit < forward.y)
    {
        const float limited = -std::log10(1.0f + (forward.y - climbLimit) * 9.0f);
        if (limited < pitch)
            pitch = limited;
    }
    pitch = clampPlane(pitch + clampPlane(rotation.x, -1.0f, 1.0f), -1.0f, 1.0f);
    yaw = clampPlane(yaw - clampPlane(rotation.y, -1.0f, 1.0f) * 0.1f, -1.0f, 1.0f);
    const float maxRoll = controls->getMaxRollAngle();
    const float pitchInput = -(dotPlane(up, levelRight) * yaw + upright * pitch) * 4.0f;
    const float pitchBank = dotPlane(right, levelUp) * pitch;
    const float yawInput = pitchBank + pitchBank + dotPlane(right, levelRight) * yaw;
    if (upright < 0.0f)
        bank = planeSign(bank) * 2.0f - bank;
    const float angle = std::asin(clampPlane(yaw, -1.0f, 1.0f));
    const float wantedBank = useAngle ? angle * 3.0f : yaw + yaw;
    // 0x2CF0F8 then 0x2CF14B: bank + clamped wanted bank first, then the half rotation added.
    const float rollInput = clampPlane(rotation.z, -1.0f, 1.0f) * 0.5f
                          + (bank + clampPlane(wantedBank, -maxRoll, maxRoll));
    const float roll = planeImpulse(clampPlane(planeLogResponse(clampPlane(rollInput, -1.0f, 1.0f)), -1.0f, 1.0f));
    const float turn = planeImpulse(clampPlane(planeLogResponse(clampPlane(yawInput, -1.0f, 1.0f)), -1.0f, 1.0f));
    const float pitchOutput = clampPlane(clampPlane(pitchInput, -1.0f, 1.0f), -1.0f, 1.0f);
    const float throttleOutput = clampPlane(throttle, 0.0f, 1.0f);
    input->setInput(controls->getDriveThrottleControl(), throttleOutput);
    input->setInput(controls->getDrivePitchControl(), pitchOutput);
    input->setInput(controls->getDriveRollControl(), roll);
    input->setInput(controls->getDriveTurnControl(), turn);
    return true;
}

// ELF 0x2CF9F0
int PlaneControl::roll(float wantedRoll, float, io::PlayerInput* input, const Mat4& transformation,
                       float, bool takingOff, const Vec3& referenceForward,
                       const Vec3& referenceRight, const Vec3& referenceUp, const Vec3&,
                       const Vec3& previousRight, const Vec3& previousUp,
                       IPIControlInfo* controls, float throttle, bool left, bool& wrapped)
{
    Mat4 transpose = planeIdentity();
    float turnInput = planeMappedInput(input, controls->getDriveTurnControl());
    float pitchInput = planeMappedInput(input, controls->getDrivePitchControl());
    float rollInput = planeMappedInput(input, controls->getDriveRollControl());
    const Vec3 up = {transformation.m[4], transformation.m[5], transformation.m[6]};
    const Vec3 right = {transformation.m[0], transformation.m[1], transformation.m[2]};
    transpose.setTranspose(transformation);
    const float halfPi = 1.570796371f;
    const float pi = 3.141592741f;
    const float twoPi = 6.283185482f;
    const float firstPitch = std::acos(clampPlane(clampPlane(dotPlane(up, referenceForward), -1.0f, 1.0f), -1.0f, 1.0f)) - halfPi;
    // The second pitch sample uses the same current up vector, as shipped.
    const float secondPitch = std::acos(clampPlane(clampPlane(dotPlane(up, referenceForward), -1.0f, 1.0f), -1.0f, 1.0f)) - halfPi;
    const Vec3 worldUp = {0.0f, 1.0f, 0.0f};
    const Vec3 localUp = rotatePlane(transpose, worldUp);
    const float elevation = std::asin(clampPlane(localUp.z, -1.0f, 1.0f));
    float pitchDemand;
    float pitchChange = 0.0f;
    float yawDemand = 0.0f;
    float yawChange = 0.0f;
    float rollDemand;
    float rollChange = 0.0f;
    if (takingOff)
    {
        throttle = 1.0f;
        const float maxClimb = controls->getMaxClimbAngle();
        float climb = (maxClimb - elevation) / (1.00001f - std::fabs(right.y));
        if (climb > halfPi)
            climb = halfPi;
        pitchDemand = -climb;
        float angle = std::acos(clampPlane(clampPlane(right.y, -1.0f, 1.0f), -1.0f, 1.0f)) - halfPi;
        if (up.y < 0.0f)
            angle = planeSign(angle) * pi - angle;
        // 0x2D02FC/0x2D032C store the angle as the roll change, 0x2D0345 its negation as the demand.
        rollChange = angle;
        rollDemand = -angle;
        // The take-off arm leaves the caller's wrapped flag untouched.
    }
    else
    {
        pitchDemand = firstPitch * up.y;
        pitchChange = pitchDemand - previousUp.y * secondPitch;
        yawDemand = (1.0f - up.y) * firstPitch;
        yawChange = yawDemand - (1.0f - previousUp.y) * secondPitch;
        float angle = std::acos(clampPlane(clampPlane(dotPlane(right, referenceUp), -1.0f, 1.0f), -1.0f, 1.0f)) - halfPi;
        if (dotPlane(referenceRight, right) < 0.0f)
            angle = twoPi - angle;
        float previous = std::acos(clampPlane(clampPlane(dotPlane(previousRight, referenceUp), -1.0f, 1.0f), -1.0f, 1.0f)) - halfPi;
        if (dotPlane(referenceRight, previousRight) < 0.0f)
            previous = twoPi - previous;
        rollChange = angle - previous;
        rollDemand = angle - wantedRoll;
        if (left ? rollChange < -pi : rollChange > pi)
        {
            rollChange = left ? twoPi + rollChange : twoPi - rollChange;
            wrapped = true;
        }
        else
            wrapped = false;
    }
    const float pitchScale = controls->getPitchScale();
    const float pitchLookAhead = controls->getPitchLookAhead();
    const float pitchSensitivity = controls->getPitchSensitivity();
    updateAngle(pitchDemand, pitchChange, pitchInput, pitchSensitivity, pitchLookAhead, pitchScale);
    const float rollScale = controls->getRollScale();
    const float rollLookAhead = controls->getRollLookAhead();
    const float rollSensitivity = controls->getRollSensitivity();
    updateAngle(rollDemand, rollChange, rollInput, rollSensitivity, rollLookAhead, rollScale);
    if (takingOff)
        turnInput = 0.0f;
    else
    {
        const float yawScale = controls->getYawScale();
        const float yawLookAhead = controls->getYawLookAhead();
        const float yawSensitivity = controls->getYawSensitivity();
        updateAngle(yawDemand, yawChange, turnInput, yawSensitivity, yawLookAhead, yawScale);
    }
    input->setInput(controls->getDriveThrottleControl(), throttle);
    input->setInput(controls->getDrivePitchControl(), pitchInput);
    input->setInput(controls->getDriveRollControl(), rollInput);
    input->setInput(controls->getDriveTurnControl(), turnInput);
    return 1;
}

// ELF 0x2D03E0 (listing; Ghidra cross-check). On the ground (BotMain +0x1A4 isAirBorn false) and
// slower than 0.1 (IPIMobile +0x24, asked before `move` is tested), moving releases inputs 0, 2,
// 1 and 3. Look releases 4 and 5; trigger 8, 31, 14..30, 38, 37, 10, 32 and 47. The information
// is not null-tested. Returns 1.
int PlaneControl::resetControls(Bot* bot, Information* equipment, io::PlayerInput* input, bool move, bool look, bool trigger)
{
    IPIMobile* const mobile = equipment->getMobile();                     // +0x38
    if (!bot->isAirBorn() && mobile->getSpeedMagnitude() < 0.1f && move)
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
        input->setInput(static_cast<io::PlayerInputMap>(14), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(15), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(16), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(17), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(18), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(19), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(20), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(21), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(22), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(23), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(24), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(25), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(26), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(27), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(28), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(29), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(30), 0.0f);
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
