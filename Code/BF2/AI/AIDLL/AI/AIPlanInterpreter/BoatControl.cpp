// Boat movement and input reset, recovered from libaidll.so.

// static init: ELF 0x2AAE90 (__tcf_0 0x2AAEF0, _GLOBAL__I 0x2AAF20): <iostream>'s Init object only.
#include <iostream>

#include "BoatControl.h"

#include "CarControl.h"
#include "ControlFunctions.h"
#include "VehicleControlInline.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include <cmath>
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// Inline sign and clamp expansions in ELF 0x2AA090; NaN has sign zero and passes the clamp.
float sign(float value)
{
    return value > 0.0f ? 1.0f : (0.0f > value ? -1.0f : 0.0f);
}

float clamp(float value, float minimum, float maximum)
{
    return minimum > value ? minimum : (value > maximum ? maximum : value);
}

float mappedInput(io::PlayerInput* input, io::PlayerInputMap control)
{
    const int index = static_cast<int>(control);
    if (index <= 63 && ((input->mappingFlags >> (static_cast<unsigned int>(index) & 63)) & 1ULL))
        return input->inputs[index];
    return 0.0f;
}

} // namespace

// ELF 0x2AA090
bool BoatControl::speedControl(Bot* bot, Information* equipment, const Vec2& direction,
                               float speedFraction, float angle, BAPAMoveTo* move,
                               float speedLimit, io::PlayerInput* input, float selectedFraction)
{
    IPIMobile* const mobile = equipment->getMobile();
    const float speedMagnitude = mobile->getSpeedMagnitude();
    mobile->getSpeed();
    const Vec3& rotation = mobile->getRotationalSpeed();
    const Mat4& transform = equipment->getTransformation();
    const float yawSpeed = transform.m[4] * rotation.x + transform.m[5] * rotation.y
                         + transform.m[6] * rotation.z;
    Vec2 position = {};
    Vec2 facing = {};
    Vec2 normal = {};
    vec3ToVec2XZ(equipment->getPosition(), position);
    vec3ToVec2XZ(equipment->getFaceing(), facing);
    getNormal(facing, normal);
    vehiclecontrol::normalizeVec2(normal);
    equipment->getAIObject()->releaseObjectFromSpawner();

    if (speedFraction == 0.0f
        || (static_cast<BotMain*>(bot)->getMoveToStatus() == 0 && std::fabs(angle) > 0.5235988f))
    {
        float turn = normal.x * direction.x + normal.y * direction.y;
        const float alignment = facing.x * direction.x + facing.y * direction.y;
        if (alignment <= 0.0f)
            turn = sign(turn);
        const Mat4& currentTransform = equipment->getTransformation();
        const Vec3& velocity = mobile->getSpeed();
        const float forwardSpeed = velocity.y * currentTransform.m[9] + velocity.x * currentTransform.m[8]
                                 + velocity.z * currentTransform.m[10];
        input->setInput(static_cast<io::PlayerInputMap>(0), 0.0f);
        const float throttle = mappedInput(input, static_cast<io::PlayerInputMap>(3));
        if (alignment > 0.996f)
        {
            float brake = 0.0f;
            if (std::fabs(forwardSpeed) > 1.0f)
                brake = -sign(forwardSpeed);
            else if (std::fabs(forwardSpeed) > 0.03f)
                brake = -sign(forwardSpeed) * log10f(std::fabs(forwardSpeed) * 9.0f + 1.0f);
            input->setInput(static_cast<io::PlayerInputMap>(3), brake);
            return true;
        }
        if (std::fabs(forwardSpeed) > 3.0f)
            input->setInput(static_cast<io::PlayerInputMap>(3), -sign(forwardSpeed));
        else if (std::fabs(throttle) != 1.0f)
            input->setInput(static_cast<io::PlayerInputMap>(3), sign(forwardSpeed));
        input->setInput(static_cast<io::PlayerInputMap>(0), sign(turn) * sign(forwardSpeed));
        return true;
    }

    if (speedFraction != 0.0f && speedFraction != selectedFraction && speedMagnitude > 1.0f)
        angle = -angle;
    const float maximumSpeed = mobile->getMaxSpeed();
    const float targetSpeed = (maximumSpeed > speedLimit ? speedLimit : maximumSpeed) * speedFraction;
    float throttle = mappedInput(input, equipment->getControlInfo()->getDriveThrottleControl());
    simpleReg(targetSpeed - speedMagnitude, move->m_previousSpeed - speedMagnitude, 1.0f, throttle);
    const float turn = clamp(sinf(clamp(angle, -1.5707964f, 1.5707964f))
                             - clamp(yawSpeed, -1.0f, 1.0f) * 0.1f, -1.0f, 1.0f);
    move->m_previousHeading = facing;
    move->m_previousSpeed = speedMagnitude;
    // The template selects the input to read above; the shipped writes still use inputs 0 and 3.
    input->setInput(static_cast<io::PlayerInputMap>(0), sign(turn) * log10f(std::fabs(turn) * 9.0f + 1.0f));
    input->setInput(static_cast<io::PlayerInputMap>(3), throttle);
    return true;
}

// ELF 0x2AA910
bool BoatControl::towardsDirection(Bot* bot, Information* equipment, Vec2 direction, float distance,
                                   BAPAMoveTo* move, io::PlayerInput* input, float speedLimit)
{
    float speedFraction;
    float angle;
    float selectedFraction;
    float unused;
    if (!CarControl::actionStatusDecision(bot, equipment, direction, distance, move,
                                          speedFraction, angle, selectedFraction, unused))
        return false;
    return speedControl(bot, equipment, direction, speedFraction, angle, move, speedLimit, input, selectedFraction);
}

// ELF 0x2AA9D0 (listing; Ghidra cross-check). `done` is set first. Moving: the forward speed is
// the mobile's speed dot the transformation's third row (+0x20..+0x28); inputs 0, 2 and 1 are
// released; up to 1.0 the throttle (input 3) is released, above it the throttle is
// -sign(speed) * log10(|speed| * 9 + 1) (-sign alone at or below 0.03, unreachable after the 1.0
// test, kept as compiled) and `done` is cleared. The information lookup is not null-tested.
// Look releases 4 and 5; trigger 8, 31, 14..30, 38, 37, 10 and 32. Returns 1.
int BoatControl::resetControls(Bot* bot, bool move, bool look, bool trigger, bool& done)
{
    io::PlayerInput* const input = bot->getInput();
    done = true;
    if (move)
    {
        Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
        IPIMobile* const mobile = equipment->getMobile();                 // +0x38
        const Mat4& transformation = equipment->getTransformation();      // +0x3C
        const Vec3& speed = mobile->getSpeed();                           // +0x1C
        const float forwardSpeed = speed.y * transformation.m[9] + speed.x * transformation.m[8]
                                 + speed.z * transformation.m[10];
        input->setInput(static_cast<io::PlayerInputMap>(0), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(2), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(1), 0.0f);
        if (std::fabs(forwardSpeed) > 1.0f)
        {
            const float sign = (forwardSpeed > 0.0f) ? 1.0f : ((0.0f > forwardSpeed) ? -1.0f : 0.0f);
            float throttle;
            if (std::fabs(forwardSpeed) > 0.03f)
                throttle = log10f(std::fabs(forwardSpeed) * 9.0f + 1.0f) * -sign;
            else
                throttle = -sign;
            input->setInput(static_cast<io::PlayerInputMap>(3), throttle);
            done = false;
        }
        else
        {
            input->setInput(static_cast<io::PlayerInputMap>(3), 0.0f);
        }
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
    }
    return 1;
}

} // namespace ai
} // namespace hfe
} // namespace dice
