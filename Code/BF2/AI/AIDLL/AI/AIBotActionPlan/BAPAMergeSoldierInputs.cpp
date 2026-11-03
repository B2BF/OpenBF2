// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D4BA0 (__tcf_0 0x1D4C00, _GLOBAL__I 0x1D4C30): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMergeSoldierInputs.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensingAgent.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../AIPlanInterpreter/MouseControl.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Game/Objects/IObject.h"
#include "../../../../Game/Objects/ObjectLayoutAccess.h"
#include "../../../../Physics/IPhysicsNode.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The function-static `lookAheadFrames` of execute (ELF .data 0x4B7EA4).
int lookAheadFrames = 3;

float signedSqrtClamped(float v)
{
    const float sign = v <= 0.0f ? (0.0f <= v ? 0.0f : -1.0f) : 1.0f;
    const float r = std::sqrt(std::fabs(v)) * sign;
    if (r < -1.0f)
        return -1.0f;
    if (1.0f < r)
        return 1.0f;
    return r;
}

} // namespace

// ELF 0x1D3DD0 (C1; C2 0x1D3D40)
BAPAMergeSoldierInputs::BAPAMergeSoldierInputs(int patternId)
{
    m_lastYawError = 0.0f;
    m_lastPitchError = 0.0f;
    m_patternId = patternId;
}

// ELF 0x1D3E70 (D1; D0 0x1D3EB0): base only.
BAPAMergeSoldierInputs::~BAPAMergeSoldierInputs()
{
}

// ELF 0x1D3F00. Turns the sensing agent's move direction (+0x50) into soldier inputs.
// Walking (no sprint input, not swimming): the direction is split onto the soldier's forward and
// right axes (XZ, normalised, turned ahead by the body's yaw rate over lookAheadFrames frames),
// each as a clamped signed square root, scaled by |throttle|; sprint released.
// Sprinting or swimming: the mouse turns the camera to the direction; throttle is pressed while
// the yaw error is small (< 5 deg) or the sprint focus (+0x58 point, or the +0x64 target's
// position) is at least 4 m away (1 m for a human-occupied vehicle within 3 m); sprint is kept
// while the error is below 40 deg and the focus is beyond that distance. A larger error releases
// the throttle input directly. Always ok and done.
bool BAPAMergeSoldierInputs::execute(Bot* bot, bool& ok, bool& done)
{
    io::PlayerInput* input = bot->getInput();
    SensingAgent* sensing = bot->getSensingAgent();
    const Vec2 move = sensing->getMoveDirection();
    Information* self = bapinline::info(bot->getCurrentEquipment());   // not null-checked
    const Mat4& t = self->getTransformation();
    IPIControlInfo* controlInfo = self->getControlInfo();
    float pitchError = 0.0f;
    float yawError = 0.0f;
    const SoldierPose pose = self->getSoldierPose();
    const bool sprint = bapinline::mappedInput(input, io::PISprint) != 0.0f;

    io::PlayerInputMap lastInput;
    float lastValue;
    if (!sprint && pose != SPSwimming) {
        Vec2 forward;
        forward.x = 0.0f;
        forward.y = 0.0f;
        Vec2 right;
        right.x = 0.0f;
        right.y = 0.0f;
        vec3ToVec2XZ(*reinterpret_cast<const Vec3*>(&t.m[8]), forward);
        vec3ToVec2XZ(*reinterpret_cast<const Vec3*>(&t.m[0]), right);
        bapinline::normalize(forward);
        bapinline::normalize(right);
        IAIObject* object = self->getAIObject();
        if (object) {
            world::IPhysicsNode* node = static_cast<world::IPhysicsNode*>(
                world::object_detail::LayoutAccess::physicsNode(object->getObject()));
            if (node) {
                const float step = static_cast<float>(lookAheadFrames)
                                   * static_cast<float>(bapinline::timeIncrease());
                const float rate = node->getRotationalSpeed().y * 0.017453292f;
                if (rate != 0.0f) {
                    // A 2D rotation by -angle (rows (c, -s), (s, c)) applied as v' = (c*x + s*y,
                    // -s*x + c*y) to both axes (0x1D48B8..0x1D4AA0).
                    const float angle = rate * step;
                    const float c = std::cos(angle);
                    const float s = std::sin(angle);
                    const Vec2 f = forward;
                    forward.x = c * f.x + s * f.y;
                    forward.y = f.x * -s + f.y * c;
                    const Vec2 r = right;
                    right.y = -s * r.x + c * r.y;
                    right.x = s * r.y + c * r.x;
                }
            }
        }
        const float throttle = bapinline::mappedInput(input, io::PIThrottle);
        const float f = signedSqrtClamped(forward.x * move.x + forward.y * move.y);
        const float r = signedSqrtClamped(right.x * move.x + move.y * right.y);
        input->setInput(io::PIThrottle, f * std::fabs(throttle));
        input->setInput(io::PIYaw, std::fabs(throttle) * r);
        lastInput = io::PISprint;
        lastValue = 0.0f;
    } else {
        Vec3 dir = bapinline::vec3(0.0f, 0.0f, 0.0f);
        vec2ToVec3XZ(move, dir);
        const Mat4& camera = *static_cast<const Mat4*>(bot->getPlayer()->getCameraTransformation());
        mouseControlLookAtDirection(bot, dir, camera, pitchError, m_lastPitchError, yawError, m_lastYawError, 0.0f,
                                    controlInfo, input, 0.0f, 0.0f, 0.0f);
        bool humanVehicle = false;
        float focusDistSqr = 0.0f;
        if (sprint) {
            const Vec3 pos = bapinline::vec3(t.m[12], t.m[13], t.m[14]);
            Vec3 focus = bapinline::vec3(0.0f, 0.0f, 0.0f);
            const unsigned int handle = bot->getSensingAgent()->getSprintFocusTarget();
            Information* focusInfo = handle == 0xFFFFFFFFu ? 0 : bapinline::info(handle);
            if (!focusInfo) {
                const Vec3& p = bot->getSensingAgent()->getSprintFocus();
                focus.x = p.x;
                focus.z = p.z;
            } else {
                focus = focusInfo->getPosition();
                humanVehicle = focusInfo->getAIObject() && !IBotManager::instance->getBot(focusInfo->getAIObject())
                               && focusInfo->getUnit() && focusInfo->getUnit()->isOccupied();
            }
            const float dx = pos.x - focus.x;
            const float dz = pos.z - focus.z;
            focusDistSqr = dx * dx + 0.0f + dz * dz;
        }
        const bool close = bot->getSensingAgent()->getSprintFocusTarget() != 0xFFFFFFFFu && focusDistSqr < 9.0f
                           && humanVehicle;
        const float limit = close ? 1.0f : 16.0f;
        if (limit <= focusDistSqr || close || std::fabs(yawError) <= 0.08726646f) {
            if ((std::fabs(yawError) < 0.6981317f || close) && limit < focusDistSqr) {
                if (sprint)
                    input->setInput(io::PISprint, 1.0f);
                lastInput = io::PIThrottle;
                lastValue = 1.0f;
            } else {
                input->setInput(io::PIThrottle, 1.0f);
                lastInput = io::PISprint;
                lastValue = 0.0f;
            }
        } else {
            if ((input->mappingFlags >> 3) & 1ULL)
                input->inputs[io::PIThrottle] = 0.0f;   // Input::setInput inlined (0x1D4180)
            lastInput = io::PISprint;
            lastValue = 0.0f;
        }
    }
    input->setInput(lastInput, lastValue);
    m_lastPitchError = pitchError;
    m_lastYawError = yawError;
    ok = true;
    done = true;
    return true;
}

// ELF 0x1D4C50
BAPStatementType BAPAMergeSoldierInputs::getId() const
{
    return BAPSTMergeSoldierInputs;   // 36
}

// ELF 0x1D4B90
bool BAPAMergeSoldierInputs::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
