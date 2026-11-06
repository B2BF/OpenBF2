// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EFD00 (__tcf_0 0x1EFD60, _GLOBAL__I 0x1EFD90): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectMoved.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EFBA0 (C1; C2 0x1EFB50 region)
BAPConObjectMoved::BAPConObjectMoved(unsigned int object, float distance)
{
    setOwner(0);
    m_object = object;
    m_lastPosition = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_distanceSqr = distance * distance;
}

// ELF 0x1EFDB0 (D0 0x1EFDD0)
BAPConObjectMoved::~BAPConObjectMoved()
{
}

// ELF 0x1EFBF0
bool BAPConObjectMoved::evaluate(Bot*)
{
    Information* info = bapinline::info(m_object);
    if (info && m_distanceSqr < bapinline::lengthSqr(bapinline::sub(info->getPosition(), m_lastPosition))) {
        m_lastPosition = info->getPosition();
        return true;
    }
    return false;
}

// ELF 0x1EFCF0
BAPConditionType BAPConObjectMoved::getConditionType() const
{
    return BAPCTObjectMoved;   // 28
}

} // namespace ai
} // namespace hfe
} // namespace dice
