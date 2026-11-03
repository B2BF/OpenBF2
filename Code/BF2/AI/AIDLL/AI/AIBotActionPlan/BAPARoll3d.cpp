// Roll-action state recovered from libaidll.so.
// static init: ELF 0x1DF350 (__tcf_0 0x1DF3B0, _GLOBAL__I 0x1DF3E0): <iostream>'s Init object only.
#include <iostream>
#include "BAPARoll3d.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DEEB0 (C1; C2 0x1DEBB0)
BAPARoll3d::BAPARoll3d(Bot* bot, int direction, float roll, float maxSpeed,
    BAPCondition* condition, unsigned long long controls, float, int tag)
    : BAPAction(UninitializedPatternId())
{
    m_maxSpeed = maxSpeed;
    m_roll = roll;
    m_vector1C = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_float28 = 0.0f;
    m_direction = direction;
    m_forward = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_up = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_right = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_flatFacing = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_vector6C = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_vector78 = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_breakCondition = condition;
    Information* self = bapinline::info(bot->getCurrentEquipment());
    Vec2 facing;
    facing.x = 0.0f;
    facing.y = 0.0f;
    const Vec3 direction3 = self->getFaceing();
    vec3ToVec2XZ(direction3, facing);
    bapinline::normalize(facing);
    addVec2ToVec3XZ(facing, m_flatFacing);
    const Mat4& first = self->getTransformation();
    m_vector78 = bapinline::vec3(first.m[4], first.m[5], first.m[6]);
    const Mat4& second = self->getTransformation();
    m_vector6C = bapinline::vec3(second.m[4], second.m[5], second.m[6]);
    m_patternId = tag;
    m_breakCondition->setOwner(this);
    m_initialAltitude = self->getAltitude(self->getMobile()->getSpeed());
    m_controls = controls;
}

// ELF 0x1DF210 (D1; D2 0x1DF1A0, D0 0x1DF280)
BAPARoll3d::~BAPARoll3d()
{
    delete m_breakCondition;
}

// ELF 0x1DF2F0
bool BAPARoll3d::execute(Bot*, bool& success, bool& finished)
{
    success = false;
    finished = true;
    return false;
}

// ELF 0x1DF310
unsigned long long BAPARoll3d::getControls()
{
    return m_controls;
}

// ELF 0x1DF400
BAPStatementType BAPARoll3d::getId() const
{
    return BAPSTRoll3d;
}

// ELF 0x1DF320
bool BAPARoll3d::hasDefault()
{
    return false;
}

// ELF 0x1DF330
float BAPARoll3d::getMaxSpeed()
{
    return m_maxSpeed;
}

// ELF 0x1DF340
BAPCondition* BAPARoll3d::getBreakCondition()
{
    return m_breakCondition;
}

} // namespace ai
} // namespace hfe
} // namespace dice
