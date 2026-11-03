// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D6F80 (__tcf_0 0x1D6FE0, _GLOBAL__I 0x1D7010): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveTo3d.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {
Vec3 row(const Mat4& m, int r)
{
    return bapinline::vec3(m.m[r * 4], m.m[r * 4 + 1], m.m[r * 4 + 2]);
}
} // namespace

// ELF 0x1D6B90 (C1; C2 0x1D6950). Store order of the image: +0x14 = 0, speeds, condition, point,
// +0x3C = 0, +0x4C/+0x58 = 0, +0x48 distance, the rest 0; then the bot's current equipment (no null
// test): +0x10 pattern id, condition +4 = this, +0x54 = getAltitude (vfn +0x38) of its
// IPIMobile::getSpeed (vfn +0x1C), +8 controls, +0x58/+0x64/+0x70 = transformation rows 2/1/0,
// +0x3C = 0 again.
BAPAMoveTo3d::BAPAMoveTo3d(Bot* bot, const Vec3& point, float maxSpeed, float maxArriveSpeed,
                           BAPCondition* breakCondition, unsigned long long controls, float distance, int patternId)
{
    m_vec14 = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_maxSpeed = maxSpeed;
    m_maxArriveSpeed = maxArriveSpeed;
    m_breakCondition = breakCondition;
    m_point = point;
    m_vec3C = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_int4C = 0;
    m_distance = distance;
    m_int50 = 0;
    m_forward = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_up = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_right = bapinline::vec3(0.0f, 0.0f, 0.0f);
    Information* equipment = bapinline::info(bot->getCurrentEquipment());
    m_patternId = patternId;
    if (m_breakCondition)
        m_breakCondition->setOwner(this);
    m_altitude = equipment->getAltitude(equipment->getMobile()->getSpeed());
    m_controls = controls;
    m_forward = row(equipment->getTransformation(), 2);
    m_right = row(equipment->getTransformation(), 0);
    m_up = row(equipment->getTransformation(), 1);
    m_vec3C = bapinline::vec3(0.0f, 0.0f, 0.0f);
}

// ELF 0x1D6E30 (D0 0x1D6E90): deletes the condition (vfn +0x10).
BAPAMoveTo3d::~BAPAMoveTo3d()
{
    if (m_breakCondition)
        delete m_breakCondition;
}

// ELF 0x1D6F00: the entry drives the move; executed directly it answers ok = false, finished = true.
bool BAPAMoveTo3d::execute(Bot*, bool& ok, bool& finished)
{
    ok = false;
    finished = true;
    return false;
}

// ELF 0x1D6F20
unsigned long long BAPAMoveTo3d::getControls()
{
    return m_controls;
}

// ELF 0x1D7030
BAPStatementType BAPAMoveTo3d::getId() const
{
    return BAPSTMoveTo;   // 7
}

// ELF 0x1D6F30
bool BAPAMoveTo3d::hasDefault()
{
    return false;
}

// ELF 0x1D7040: the break condition's evaluate (vfn +0), false without one.
bool BAPAMoveTo3d::isFinished(Bot* bot)
{
    bool finished = false;
    if (m_breakCondition)
        finished = m_breakCondition->evaluate(bot);
    return finished;
}

// ELF 0x1D6F40
float BAPAMoveTo3d::getMaxSpeed()
{
    return m_maxSpeed;
}

// ELF 0x1D6F50
float BAPAMoveTo3d::getMaxArriveSpeed()
{
    return m_maxArriveSpeed;
}

// ELF 0x1D6F60
BAPCondition* BAPAMoveTo3d::getBreakCondition()
{
    return m_breakCondition;
}

// ELF 0x1D6F70
Vec3* BAPAMoveTo3d::getFirstPoint()
{
    return &m_point;
}

} // namespace ai
} // namespace hfe
} // namespace dice
