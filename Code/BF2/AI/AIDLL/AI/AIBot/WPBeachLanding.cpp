// A boat waypoint list for beach landings. Evidence: libaidll.so 1.5.3153 (ia-32), see
// WPBeachLanding.h.

#include "WPBeachLanding.h"

#include "BotMain.h"
#include "../AIStrategicEntity/AILandingZone.h"
#include "../Main/AILandingZoneManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B86C0, __tcf_0 0x1B8720, _GLOBAL__I 0x1B8750).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B8370 (C1; C2 0x1B82E0): the base fields (+8 = -1.0f), the arguments, the zone by index,
// +0xC = 6, then the zone into BotMain +0xE8 through the Bot pointer.
WPBeachLanding::WPBeachLanding(int zoneIndex, float value, int map, int approachType, Bot* bot)
{
    m_map = map;
    m_inside = false;
    m_approachType = approachType;
    m_value = value;
    m_zone = AILandingZoneManager::m_instance->getAILandingZone(zoneIndex);
    m_int0C = 6;
    static_cast<BotMain*>(bot)->m_ptr232 = m_zone;
}

// ELF 0x1B8470 (C1; C2 0x1B8400): as above with the zone given.
WPBeachLanding::WPBeachLanding(AILandingZone* zone, float value, int map, int approachType, Bot* bot)
{
    m_map = map;
    m_inside = false;
    m_approachType = approachType;
    m_zone = zone;
    m_int0C = 6;
    m_value = value;
    static_cast<BotMain*>(bot)->m_ptr232 = zone;
}

void WPBeachLanding::storeLandingZone(Bot* bot, AILandingZone* zone)
{
    static_cast<BotMain*>(bot)->m_ptr232 = zone;
}

// ELF 0x1B8500 (D1; D2 0x1B84E0, D0 0x1B8520): the base vptr only.
WPBeachLanding::~WPBeachLanding()
{
}

// ELF 0x1B8560. Inside means a squared zone distance of exactly 0 (unordered is outside); outside,
// the distance is queried a second time for the store. +4 is set when inside changes.
float WPBeachLanding::getUrgency(const Vec3& position, Bot*)
{
    if (m_wayPointError)
        return 0.0f;

    const bool wasInside = m_inside;
    m_inside = m_zone->getDistanceSqr(position) == 0.0f;
    m_distanceSqr = m_inside ? 0.0f : m_zone->getDistanceSqr(position);
    m_flag4 = m_inside != wasInside;
    return 1.0f;
}

// ELF 0x1B8600: the beach position inside the zone, the approach position (10 m) outside it.
bool WPBeachLanding::getGoalPoint(Vec3& out)
{
    if (m_inside)
        return m_zone->getBeachPosition(m_map, out);
    return m_zone->getApproachPosition(m_map, m_approachType, out, 10.0f);
}

// ELF 0x1B8670
bool WPBeachLanding::insideZone()
{
    return m_inside;
}

// ELF 0x1B8680: distance^2 >= the last squared zone distance; unordered is false.
bool WPBeachLanding::closeToZone(float distance)
{
    return distance * distance >= m_distanceSqr;
}

// ELF 0x1B86A0: a constant 10.
float WPBeachLanding::getWayPointRadius() const
{
    return 10.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
