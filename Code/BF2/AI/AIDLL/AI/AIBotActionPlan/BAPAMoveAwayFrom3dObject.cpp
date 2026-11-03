// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1D5030 (__tcf_0 0x1D5090, _GLOBAL__I 0x1D50C0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAMoveAwayFrom3dObject.h"

#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "BAPActionInline.h"
#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D4D50 (C1; C2 0x1D4C60)
BAPAMoveAwayFrom3dObject::BAPAMoveAwayFrom3dObject(unsigned int object, float distance, BAPCondition* condition,
                                                   unsigned long long controls, int patternId)
{
    m_condition = condition;
    m_point = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_object = object;
    m_vec30 = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_distance = distance;
    m_patternId = patternId;
    m_controls = controls;
    if (condition)
        condition->setOwner(this);
    if (Information* const info = bapinline::info(m_object))
        m_mobile = info->getMobile() != 0;
    m_vec30 = bapinline::vec3(0.0f, 0.0f, 0.0f);
}

// ELF 0x1D4EA0 (D1 0x1D4E40, D0 0x1D4F00)
BAPAMoveAwayFrom3dObject::~BAPAMoveAwayFrom3dObject()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1D4F70. Only a vehicle move entry executes it: on its own it fails at once.
bool BAPAMoveAwayFrom3dObject::execute(Bot*, bool& ok, bool& finished)
{
    ok = false;
    finished = true;
    return false;
}

// ELF 0x1D4F90. Without information the previous point is returned unchanged.
Vec3* BAPAMoveAwayFrom3dObject::getFirstPoint()
{
    if (Information* const info = bapinline::info(m_object))
    {
        if (!m_mobile)
            m_point = info->getPosition();
        else
            info->getMobile()->getPositionEstimate(m_point);
    }
    return &m_point;
}

// ELF 0x1D50E0 (weak)
unsigned long long BAPAMoveAwayFrom3dObject::getControls()
{
    return m_controls;
}

// ELF 0x1D50F0 (weak)
BAPStatementType BAPAMoveAwayFrom3dObject::getId() const
{
    return BAPSTMoveAwayFromObject;
}

// ELF 0x1D5100 (weak)
bool BAPAMoveAwayFrom3dObject::hasDefault()
{
    return false;
}

// ELF 0x1D5110 (weak)
bool BAPAMoveAwayFrom3dObject::isFinished(Bot* bot)
{
    bool finished = false;
    if (m_condition)
        finished = m_condition->evaluate(bot);
    return finished;
}

} // namespace ai
} // namespace hfe
} // namespace dice
