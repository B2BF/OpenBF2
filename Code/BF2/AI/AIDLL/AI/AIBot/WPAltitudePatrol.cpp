// A flight patrol loop. Evidence: libaidll.so 1.5.3153 (ia-32), see WPAltitudePatrol.h.

#include "WPAltitudePatrol.h"

#include <cmath>

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B8220, __tcf_0 0x1B8280, _GLOBAL__I 0x1B82B0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B7E60 (C1; C2 0x1B7CB0): the base fields (+8 = -1.0f), the arguments, the empty list,
// +0xC = 5, a clear, then each position appended.
WPAltitudePatrol::WPAltitudePatrol(float first, float second, float third, unsigned int flags,
                                   std::list<Vec3>& positions)
{
    m_first = first;
    m_second = second;
    m_third = third;
    m_flags = flags;
    m_int0C = 5;
    m_positions.clear();
    for (std::list<Vec3>::iterator it = positions.begin(); it != positions.end(); ++it)
        m_positions.insert(m_positions.end(), *it);
}

// ELF 0x1B7FC0 (D1; D2 0x1B7F60, D0 0x1B8020): the list, then the base vptr.
WPAltitudePatrol::~WPAltitudePatrol()
{
}

// ELF 0x1B8090. The front position is reached when m_first^2 > dx^2 + dz^2 and |dy| < m_second
// (differences position - front); it is then re-inserted at the end before the front is erased
// and +4 is set. The urgency is 1 either way, 0 on a waypoint error.
// [PLATFORM] retail keeps the XZ sum in x87 extended precision and takes |dy| through SSE
// (`andnps` with the sign mask); native builds round each step to float.
float WPAltitudePatrol::getUrgency(const Vec3& position, Bot*)
{
    if (m_wayPointError)
        return 0.0f;

    const float radiusSqr = m_first * m_first;
    const float dx = position.x - m_positions.front().x;
    const float dz = position.z - m_positions.front().z;
    if (radiusSqr > dx * dx + dz * dz)
    {
        const float dy = std::fabs(position.y - m_positions.front().y);
        if (m_second > dy)
        {
            m_positions.insert(m_positions.end(), m_positions.front());
            m_positions.erase(m_positions.begin());
            m_flag4 = true;
        }
    }
    return 1.0f;
}

// ELF 0x1B81A0: the front position (no empty check).
bool WPAltitudePatrol::getGoalPoint(Vec3& out)
{
    out = m_positions.front();
    return true;
}

// ELF 0x1B81E0: list assignment.
void WPAltitudePatrol::getCoords(std::list<Vec3>& out) const
{
    out = m_positions;
}

// ELF 0x1B8210: `flds 0x10(this)`.
float WPAltitudePatrol::getWayPointRadius() const
{
    return m_first;
}

} // namespace ai
} // namespace hfe
} // namespace dice
