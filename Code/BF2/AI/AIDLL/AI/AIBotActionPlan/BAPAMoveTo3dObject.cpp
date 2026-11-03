// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D7EF0 (__tcf_0 0x1D7F50, _GLOBAL__I 0x1D7F80): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveTo3dObject.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D7B80 (C1; C2 0x1D7A00). Zeroes +0x28..+0x7C and +0x84, stores the arguments, condition
// +4 = this, +0x80 = object has an IPIMobile (only when the information exists), then the three
// axes (+0x74 (1,0,0), +0x68 (0,0,1), +0x5C (0,1,0)) and +0x34 = 0.
BAPAMoveTo3dObject::BAPAMoveTo3dObject(unsigned int object, float f18, float maxSpeed, float maxArriveSpeed,
                                       float distance, BAPCondition* breakCondition, unsigned long long controls,
                                       int patternId)
{
    m_object = object;
    m_float18 = f18;
    m_maxSpeed = maxSpeed;
    m_breakCondition = breakCondition;
    m_point = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_maxArriveSpeed = maxArriveSpeed;
    m_vec34 = bapinline::vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4; ++i)
        m_int40[i] = 0;
    m_distance = distance;
    m_up = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_forward = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_right = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_int84 = 0;
    m_patternId = patternId;
    m_controls = controls;
    if (breakCondition)
        breakCondition->setOwner(this);
    Information* info = bapinline::info(m_object);
    if (info)
        m_objectMobile = info->getMobile() != 0;
    m_right = bapinline::vec3(1.0f, 0.0f, 0.0f);
    m_forward = bapinline::vec3(0.0f, 0.0f, 1.0f);
    m_up = bapinline::vec3(0.0f, 1.0f, 0.0f);
    m_vec34 = bapinline::vec3(0.0f, 0.0f, 0.0f);
}

// ELF 0x1D7D60 (D0 0x1D7DC0): deletes the condition (vfn +0x10).
BAPAMoveTo3dObject::~BAPAMoveTo3dObject()
{
    if (m_breakCondition)
        delete m_breakCondition;
}

// ELF 0x1D7E30: as BAPAMoveTo3d::execute.
bool BAPAMoveTo3dObject::execute(Bot*, bool& ok, bool& finished)
{
    ok = false;
    finished = true;
    return false;
}

// ELF 0x1D7FA0
unsigned long long BAPAMoveTo3dObject::getControls()
{
    return m_controls;
}

// ELF 0x1D7FB0
BAPStatementType BAPAMoveTo3dObject::getId() const
{
    return BAPSTMoveToObject;   // 9
}

// ELF 0x1D7FC0
bool BAPAMoveTo3dObject::hasDefault()
{
    return false;
}

// ELF 0x1D7FD0
bool BAPAMoveTo3dObject::isFinished(Bot* bot)
{
    bool finished = false;
    if (m_breakCondition)
        finished = m_breakCondition->evaluate(bot);
    return finished;
}

// ELF 0x1D7E50: refresh +0x28 from the object (getPosition vfn +0x30, or with +0x80 the IPIMobile
// position estimate vfn +0x2C) when it still exists; return it.
Vec3* BAPAMoveTo3dObject::getFirstPoint()
{
    Information* info = bapinline::info(m_object);
    if (info) {
        if (!m_objectMobile)
            m_point = info->getPosition();
        else
            info->getMobile()->getPositionEstimate(m_point);
    }
    return &m_point;
}

} // namespace ai
} // namespace hfe
} // namespace dice
