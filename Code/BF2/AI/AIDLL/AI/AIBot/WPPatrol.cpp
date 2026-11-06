// A closed patrol loop. Evidence: libaidll.so 1.5.3153 (ia-32), see WPPatrol.h.

#include "WPPatrol.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1BB370, __tcf_0 0x1BB3D0, _GLOBAL__I 0x1BB400).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1BAC40 (C2; C1 0x1BADF0). The base fields, +8 = -1.0f, the clamp, +0x10 / +0x14, the empty
// list, +0xC = 8, a clear, then each point appended.
WPPatrol::WPPatrol(float radius, unsigned int flags, std::list<Vec3>& points)
{
    if (radius <= 5.0f)
        radius = 5.0f;
    m_radius = radius;
    m_flags = flags;
    m_int0C = 8;
    m_points.clear();
    for (std::list<Vec3>::iterator it = points.begin(); it != points.end(); ++it)
        m_points.insert(m_points.end(), *it);
}

// ELF 0x1BAEF0 (C2; C1 0x1BAFD0). As above, from a vector.
WPPatrol::WPPatrol(float radius, unsigned int flags, std::vector<Vec3>& points)
{
    if (radius <= 5.0f)
        radius = 5.0f;
    m_radius = radius;
    m_flags = flags;
    m_int0C = 8;
    m_points.clear();
    for (std::vector<Vec3>::iterator it = points.begin(); it != points.end(); ++it)
        m_points.insert(m_points.end(), *it);
}

// ELF 0x1BB0B0 (D2; D1 0x1BB110, D0 0x1BB170): the list, then the base vptr.
WPPatrol::~WPPatrol()
{
}

// ELF 0x1BB1E0. The test is radius^2 > distXZSqr(position, front); the front point is re-inserted
// at the end before the front is erased.
float WPPatrol::getUrgency(const Vec3& position, Bot*)
{
    if (m_wayPointError)
        return 0.0f;
    const float radiusSqr = m_radius * m_radius;
    if (radiusSqr > distXZSqr(position, m_points.front()))
    {
        m_points.insert(m_points.end(), m_points.front());
        m_points.erase(m_points.begin());
        m_flag4 = true;
    }
    return 1.0f;
}

// ELF 0x1BB2F0.
bool WPPatrol::getGoalPoint(Vec3& out)
{
    out = m_points.front();
    return true;
}

// ELF 0x1BB330: list assignment.
void WPPatrol::getCoords(std::list<Vec3>& out) const
{
    out = m_points;
}

// ELF 0x1BB360.
float WPPatrol::getWayPointRadius() const
{
    return m_radius;
}

} // namespace ai
} // namespace hfe
} // namespace dice
