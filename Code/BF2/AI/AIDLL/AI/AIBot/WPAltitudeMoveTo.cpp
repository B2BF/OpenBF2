// A flight waypoint list. Evidence: libaidll.so 1.5.3153 (ia-32), see WPAltitudeMoveTo.h.

#include "WPAltitudeMoveTo.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B6CC0, __tcf_0 0x1B6D20, _GLOBAL__I 0x1B6D50).
#include <iostream>

#include "BotMain.h"

#include <sstream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B6230 (C1; C2 0x1B6420): base fields, +8 = -1.0f, the arguments, +0x18 = 1e9f
// (0x4E6E6B28), the empty list, +0xC = 4, a clear and the copy.
WPAltitudeMoveTo::WPAltitudeMoveTo(float first, float second, float third, unsigned int flags,
                                   std::list<Vec3>& positions)
{
    m_first = first;
    m_second = second;
    m_float18 = 1e9f;
    m_third = third;
    m_flags = flags;
    m_int0C = 4;
    m_positions.clear();
    for (std::list<Vec3>::iterator it = positions.begin(); it != positions.end(); ++it)
        m_positions.push_back(*it);
}

// ELF 0x1B6560: the list, then the base vptr.
WPAltitudeMoveTo::~WPAltitudeMoveTo()
{
}

// ELF 0x1B6690. f2 = m_first^2, s2 = m_second^2, d = distXZSqr(position, front), dy2 = the squared
// height difference to the front position. With several positions the front is dropped (+4 set)
// when f2 > d and s2 > dy2, or when d grew beyond the last stored distance (+0x18), which then
// takes d; the urgency is (d + dy2) / (f2 + s2) when f2 > d and dy2 > s2 (the height test is
// reversed against the drop test, as compiled), else 1. With one position nothing is dropped: the
// urgency is 0 inside 50 m (2500) for an airborne bot, else (d + dy2) / (f2 + s2) when f2 > d
// and s2 > dy2, else 1; that arm also formats the values into a stringstream that is never read
// (the image's dead debug output). No positions: +5 set, urgency 0. Unordered compares answer 1.
// [PLATFORM] retail keeps dy2 and the sums in x87 extended precision (dy2 is rounded to float only
// on the drop path).
float WPAltitudeMoveTo::getUrgency(const Vec3& position, Bot* bot)
{
    if (m_wayPointError)
        return 0.0f;
    if (m_positions.empty())
    {
        m_flag5 = true;
        return 0.0f;
    }

    const float firstSqr = m_first * m_first;
    const float secondSqr = m_second * m_second;
    const float distanceSqr = distXZSqr(position, m_positions.front());
    const float dy = position.y - m_positions.front().y;
    const float heightSqr = dy * dy;
    if (m_positions.size() == 1)
    {
        float urgency;
        if (2500.0f > distanceSqr && bot->isAirBorn())
            urgency = 0.0f;
        else if (firstSqr > distanceSqr && secondSqr > heightSqr)
            urgency = (distanceSqr + heightSqr) / (firstSqr + secondSqr);
        else
            urgency = 1.0f;
        std::stringstream debug;
        debug << "distLeftSquare: " << distanceSqr << ", MinDist: " << m_first << ", Urg: " << urgency;
        return urgency;
    }

    if ((firstSqr > distanceSqr && secondSqr > heightSqr) || distanceSqr > m_float18)
    {
        m_positions.erase(m_positions.begin());
        m_flag4 = true;
    }
    m_float18 = distanceSqr;
    if (firstSqr > distanceSqr && heightSqr > secondSqr)
        return (distanceSqr + heightSqr) / (firstSqr + secondSqr);
    return 1.0f;
}

// ELF 0x1B6C40: the first position when there is one.
bool WPAltitudeMoveTo::getGoalPoint(Vec3& out)
{
    const bool any = !m_positions.empty();
    if (any)
        out = m_positions.front();
    return any;
}

// ELF 0x1B6C80: `out = positions` (std::list::operator=).
void WPAltitudeMoveTo::getCoords(std::list<Vec3>& out) const
{
    out = m_positions;
}

// ELF 0x1B6CB0: `flds 0x10(this)`.
float WPAltitudeMoveTo::getWayPointRadius() const
{
    return m_first;
}

} // namespace ai
} // namespace hfe
} // namespace dice
