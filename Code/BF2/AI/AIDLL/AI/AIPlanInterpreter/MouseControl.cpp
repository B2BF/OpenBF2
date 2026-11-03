// MouseControl (attested TU `AI/AIDLL/AI/AIPlanInterpreter/MouseControl.cpp`, FILE symbol and
// static initialiser beside 0x2CCEE0). Recovered from libaidll.so (Linux 1.5.3153, ia-32), read in
// Intel syntax and checked against the Ghidra decompilation. Parameter names are [INFERRED].

// static init: ELF 0x2CDA50 (__tcf_0 0x2CDAB0, _GLOBAL__I 0x2CDAE0): <iostream>'s Init object only.
#include <iostream>

#include "MouseControl.h"

#include <cmath>

#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "InfanteryControl.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Vec3.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The image's inline clamp (`fld -1; fucomi; ... fcmov`); NaN gives -1. The image clamps the
// direction cosines twice, which is the same value.
inline float clampUnit(float v)
{
    if (!(-1.0f <= v))
        return -1.0f;
    if (1.0f < v)
        return 1.0f;
    return v;
}

inline float signOf(float v)
{
    if (v <= 0.0f)
        return (v < 0.0f) ? -1.0f : 0.0f;
    return 1.0f;
}

// sign of the target speed: NaN counts as positive here (`fld 1; ... p <= 0`), as in the image
inline float speedSign(float v)
{
    if (v <= 0.0f)
        return (v < 0.0f) ? -1.0f : 0.0f;
    return 1.0f;
}

} // namespace

// ELF 0x2CCEE0. Drives the mouse-look inputs so that the camera `camera` turns towards the unit
// direction `dir`. The pitch and yaw errors (radians, from the camera's up and right rows) are
// returned through pitchError/yawError; the previous frame's errors give the derivative term, and
// the target's own angular speeds (targetYawSpeed, targetPitchSpeed) add a feed-forward term that is
// halved when the target moves the same way as the error. With useAllowedDeviation (never set in
// this build) errors below allowedDeviation give no input. The vertical output is damped by bot
// skill; both are kept above minInput and inside the per-axis limits, which grow with bot skill.
// Soldiers (object flag 0x200000) look at full speed vertically and, while idle without a firing
// target, use a softer horizontal gain (newP) and limit (1.5).
void mouseControlLookAtDirection(Bot* bot, const Vec3& dir, const Mat4& camera, float& pitchError, float lastPitchError,
                                 float& yawError, float lastYawError, float allowedDeviation,
                                 IPIControlInfo* controlInfo, io::PlayerInput* input,
                                 float pitchBias, float targetYawSpeed, float targetPitchSpeed)
{
    // function-local tuning statics (.data, never written in this build)
    static float maxYInputNew = 2.0f;           // ELF 0x4BDD14
    static float tweak = 0.05f;                 // ELF 0x4BDD18
    static float minInput = 0.01f;              // ELF 0x4BDD1C
    static bool useAllowedDeviation = false;    // ELF 0x4CE5E5 (.bss)

    const float x = dir.x;
    const float y = dir.y;
    const float z = dir.z;
    const float upCos = clampUnit(camera.m[6] * z + camera.m[5] * y + camera.m[4] * x);
    const float rightCos = clampUnit(camera.m[2] * z + camera.m[1] * y + camera.m[0] * x);
    const float forwardCos = clampUnit(z * camera.m[10] + camera.m[8] * x + y * camera.m[9]);

    pitchError = -(std::acos(clampUnit(upCos)) - 1.57079637f);
    const float rightAngle = std::acos(clampUnit(rightCos));
    yawError = rightAngle;
    if (forwardCos < 0.0f)
    {
        if (rightCos < 0.0f)
            yawError = 4.71238899f - rightAngle;
        else
            yawError = -(rightAngle + 1.57079637f);
    }
    else
    {
        yawError = rightAngle - 1.57079637f;
    }

    const float proportional = controlInfo->getProportionalConstant();
    float horizontalP = proportional;
    const float derivative = controlInfo->getDerivativeConstant();
    const float angleSpeed = controlInfo->getAngleSpeedConstant();

    float maxY = 4.0f;
    float maxX;
    Information* info = informationManager->getInformation(bot->getCurrentEquipment());
    const float skill = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getBotSkill();
    if (info == 0 || (info->getObjectTypeFlags() & 0x200000) == 0)
    {
        static float maxXInputNew = 1.0f;        // ELF 0x4BDD10
        maxY = (4.0f - maxYInputNew) * skill + maxYInputNew;
        maxX = (4.0f - maxXInputNew) * skill + maxXInputNew;
    }
    else if (bot->getFiringTarget() == static_cast<unsigned int>(-1)
             && infantry_detail::inputValue(input, io::PIThrottle) == 0.0f
             && infantry_detail::inputValue(input, io::PISprint) == 0.0f
             && infantry_detail::inputValue(input, io::PIYaw) == 0.0f)
    {
        static float newP = -3.0f;               // ELF 0x4BDD04
        static float maxXInputNew = 1.5f;        // ELF 0x4BDD08
        horizontalP = newP;
        maxX = maxXInputNew;
    }
    else if (infantry_detail::inputValue(input, io::PISprint) != 0.0f)
    {
        maxX = maxY;
    }
    else
    {
        static float maxXInputNew = 1.0f;        // ELF 0x4BDD0C
        maxX = (4.0f - maxXInputNew) * skill + maxXInputNew;
    }

    // vertical
    float feedForward = angleSpeed * targetPitchSpeed;
    if (signOf(pitchError) == speedSign(targetPitchSpeed))
        feedForward = feedForward * 0.5f;
    float bias = 0.0f;
    if (pitchBias < -0.0001f)
    {
        const float t = pitchBias / -tweak;
        bias = 0.75f;
        if (t < 0.75f)
            bias = t;
    }
    float vertical = proportional * pitchError + (pitchError - lastPitchError) * derivative + feedForward + bias;
    if (!useAllowedDeviation || allowedDeviation <= std::fabs(pitchError))
    {
        if (4.0f < std::fabs(vertical))
            vertical = 4.0f * signOf(vertical);
    }
    else
    {
        vertical = 0.0f;
    }
    vertical = vertical * (1.0f - skill * 0.5f) * 0.5f;
    if (std::fabs(vertical) < minInput && vertical != 0.0f)
        vertical = signOf(vertical) * minInput;
    float verticalInput = -maxY;
    if (-maxY <= vertical)
    {
        if (maxY < vertical)
            vertical = maxY;
        verticalInput = vertical;
    }
    // inlined setInput in the image (range diagnostic, then the mapped-bit store)
    input->setInput(controlInfo->getLookVerticalControl(), verticalInput);

    // horizontal
    float yawFeedForward = angleSpeed * targetYawSpeed;
    if (signOf(yawError) == speedSign(targetYawSpeed))
        yawFeedForward = yawFeedForward * 0.5f;
    const float horizontalRaw = horizontalP * yawError + (yawError - lastYawError) * derivative + yawFeedForward;
    float horizontal = 0.0f;
    if (!useAllowedDeviation || allowedDeviation <= std::fabs(yawError))
    {
        horizontal = horizontalRaw;
        if (4.0f < std::fabs(horizontalRaw))
            horizontal = signOf(horizontalRaw) * 4.0f;
    }
    if (std::fabs(horizontal) < minInput && horizontal != 0.0f)
        horizontal = signOf(horizontal) * minInput;
    float horizontalInput = -maxX;
    if (-maxX <= horizontal)
    {
        horizontalInput = horizontal;
        if (maxX < horizontal)
            horizontalInput = maxX;
    }
    input->setInput(controlInfo->getLookHorizontalControl(), horizontalInput);
}

} // namespace ai
} // namespace hfe
} // namespace dice
