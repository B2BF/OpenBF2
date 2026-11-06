// The "no waypoints" list. Evidence: libaidll.so 1.5.3153 (ia-32), see WPEmpty.h.

#include "WPEmpty.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B9230, __tcf_0 0x1B9290, _GLOBAL__I 0x1B92C0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B9050.
WPEmpty::WPEmpty(const Vec3& position)
    : m_int12(1)
    , m_position(position)
{
    m_flag4 = true;
    m_flag5 = false;
    m_wayPointError = false;
}

// ELF 0x1B9130.
WPEmpty::~WPEmpty()
{
}

// ELF 0x1B91B0: marks the list (+4) and follows the bot's position; urgency 0.
float WPEmpty::getUrgency(const Vec3& position, Bot*)
{
    m_flag4 = true;
    m_position = position;
    return 0.0f;
}

// ELF 0x1B9200.
bool WPEmpty::getGoalPoint(Vec3& out)
{
    out = m_position;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
