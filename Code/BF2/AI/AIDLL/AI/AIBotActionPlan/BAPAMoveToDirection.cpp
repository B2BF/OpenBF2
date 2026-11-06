// Direction-move base recovered from libaidll.so.
// static init: ELF 0x1D8600 (__tcf_0 0x1D8660, _GLOBAL__I 0x1D8690): <iostream>'s Init object only.
#include <iostream>
#include "BAPAMoveToDirection.h"
#include "BAPCondition.h"
#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D8520 (C1; C2 0x1D8440)
BAPAMoveToDirection::BAPAMoveToDirection()
    : BAPAction(UninitializedPatternId()),
      BAPAComponentSoldierPose(SPStanding, SPLying, 1.0f, SPCrouching, 0.0f, 0xFFFFFFFFu, 5.0f),
      m_point(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_direction(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_state68(bapinline::vec3(0.0f, 0.0f, 0.0f))
{
}

// ELF 0x1D8740 (D1; D0 0x1D87D0)
BAPAMoveToDirection::~BAPAMoveToDirection()
{
}

// ELF 0x1D86B0
unsigned long long BAPAMoveToDirection::getControls()
{
    return m_controls;
}

// ELF 0x1D86C0
BAPStatementType BAPAMoveToDirection::getId() const
{
    return BAPSTMoveToDirection;
}

// ELF 0x1D86D0
bool BAPAMoveToDirection::hasDefault()
{
    return false;
}

// ELF 0x1D86E0: the Boolean reference is deliberately untouched.
int BAPAMoveToDirection::update(Vec2& point, bool&)
{
    point.x = m_point.x;
    point.y = m_point.z;
    return 1;
}

// ELF 0x1D8700
bool BAPAMoveToDirection::isFinished(Bot* bot)
{
    return m_endCondition && m_endCondition->evaluate(bot);
}

} // namespace ai
} // namespace hfe
} // namespace dice
