// A flight waypoint list that closes in on an object. Evidence: libaidll.so 1.5.3153 (ia-32), see
// WPAltitudeCloseTo.h.

#include "WPAltitudeCloseTo.h"

#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B60D0, __tcf_0 0x1B6130, _GLOBAL__I 0x1B6160).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B5E40 (C1; C2 0x1B5DE0): the base fields (+8 = -1.0f), the arguments, then +0xC = 3.
WPAltitudeCloseTo::WPAltitudeCloseTo(unsigned int target, float radius, float second, float third)
{
    m_target = target;
    m_radius = radius;
    m_second = second;
    m_int0C = 3;
    m_third = third;
}

// ELF 0x1B5EC0 (D1; D2 0x1B5EA0, D0 0x1B5EE0): the base vptr only.
WPAltitudeCloseTo::~WPAltitudeCloseTo()
{
}

// ELF 0x1B5F20. A missing target sets +5 and yields 0. Otherwise the waypoint is reached (+4 set,
// urgency 0) when radius^2 > dx^2 * dz^2 and position.y is below +0x18; every other case yields 1.
// The XZ test multiplies the squared components (`fmulp` at 0x1B5FEF) where a distance would add
// them; that shipped defect is kept. Unordered compares fall through to 1, as here.
// [PLATFORM] retail keeps the differences and products in x87 extended precision; native builds
// round each to float.
float WPAltitudeCloseTo::getUrgency(const Vec3& position, Bot*)
{
    if (m_wayPointError)
        return 0.0f;

    // The image zeroes two Vec3 temporaries (the difference and a copy of it) before the lookup;
    // only their components below are read.
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
    {
        m_flag5 = true;
        return 0.0f;
    }

    const Vec3& targetPosition = target->getPosition();
    const Vec3 from = position;
    const float dx = targetPosition.x - from.x;
    const float dz = targetPosition.z - from.z;
    if (m_radius * m_radius > dx * dx * (dz * dz) && m_second > from.y)
    {
        m_flag4 = true;
        return 0.0f;
    }
    return 1.0f;
}

// ELF 0x1B6040: the target's position, false when the handle is stale.
bool WPAltitudeCloseTo::getGoalPoint(Vec3& out)
{
    Information* const target = informationManager->getInformation(m_target);
    if (!target)
        return false;
    out = target->getPosition();
    return true;
}

// ELF 0x1B60C0: `flds 0x14(this)`.
float WPAltitudeCloseTo::getWayPointRadius() const
{
    return m_radius;
}

} // namespace ai
} // namespace hfe
} // namespace dice
