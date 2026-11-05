// A boat waypoint list for beach landings through waypoints. Evidence: libaidll.so 1.5.3153
// (ia-32), see WPMoveToBeachLanding.h.

#include "WPMoveToBeachLanding.h"

#include <cmath>

#include "BotMain.h"
#include "WPBeachLanding.h"                                       // storeLandingZone
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIStrategicEntity/AILandingZone.h"
#include "../Main/AILandingZoneManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1BAB80, __tcf_0 0x1BABE0, _GLOBAL__I 0x1BAC10).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// The radius the constructors keep: the argument when above 5, else 5 (unordered gives 5).
float atLeastFive(float radius)
{
    return radius > 5.0f ? radius : 5.0f;
}

} // anonymous namespace

// ELF 0x1BA320 (C1; C2 0x1BA150): the base fields, the arguments, the zone by index, the
// waypoints copy-constructed, +0xC = 7, then the zone into BotMain +0xE8 through the Bot pointer.
WPMoveToBeachLanding::WPMoveToBeachLanding(int zoneIndex, float radius, int map, int approachType,
                                           int value24,
                                           std::list<std::pair<Vec3, int> >& waypoints, Bot* bot)
    : m_map(map)
    , m_approachType(approachType)
    , m_inside(false)
    , m_radius(atLeastFive(radius))
    , m_int24(value24)
    , m_zone(AILandingZoneManager::m_instance->getAILandingZone(zoneIndex))
    , m_waypoints(waypoints)
{
    m_int0C = 7;
    WPBeachLanding::storeLandingZone(bot, m_zone);                      // BotMain +0xE8
}

// ELF 0x1BA560 (C1; C2 0x1BA450): as above with the zone given.
WPMoveToBeachLanding::WPMoveToBeachLanding(AILandingZone* zone, float radius, int map,
                                           int approachType, int value24,
                                           std::list<std::pair<Vec3, int> >& waypoints, Bot* bot)
    : m_map(map)
    , m_approachType(approachType)
    , m_inside(false)
    , m_radius(atLeastFive(radius))
    , m_int24(value24)
    , m_zone(zone)
    , m_waypoints(waypoints)
{
    m_int0C = 7;
    WPBeachLanding::storeLandingZone(bot, m_zone);                      // BotMain +0xE8
}

// ELF 0x1BA6D0 (D1; D2 0x1BA670, D0 0x1BA730): the waypoint list.
WPMoveToBeachLanding::~WPMoveToBeachLanding()
{
}

// ELF 0x1BA7A0. While waypoints remain, the first is dropped when reached - within the larger of
// the ceil'd radius and the bot's equipment's smallest radius (IPIPhysical +13), in XZ - or when
// the bot is inside its strategic area; dropping sets +4 and clears the error. A waypoint whose
// area does not exist is dropped only on a waypoint error; one whose area the bot is outside of
// is re-randomised within the area (0.9) on an error, which clears it. The equipment's record
// and physical plug-in are not null-tested. Without waypoints, the zone as WPBeachLanding but
// "inside" is a squared distance below 10. Always 1.
float WPMoveToBeachLanding::getUrgency(const Vec3& position, Bot* bot)
{
    if (!m_waypoints.empty())
    {
        Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment()); // +0xD8
        IPIPhysical* const physical = equipment->getPhysical();
        const float radius = static_cast<float>(std::ceil(static_cast<double>(m_radius)));
        const float smallest = physical->getSmallestRadius();
        float reach = radius > smallest ? radius : smallest;
        reach = reach * reach;
        const std::pair<Vec3, int>& front = m_waypoints.front();
        const float dx = position.x - front.first.x;
        const float dz = position.z - front.first.z;
        if (reach > dx * dx + dz * dz)
        {
            m_waypoints.erase(m_waypoints.begin());
            m_flag4 = true;
            m_wayPointError = false;
        }
        else
        {
            AIStrategicArea* const area =
                AIStrategicObjectManager::getInstance()->getStrategicArea(front.second);
            if (area)
            {
                if (area->isInside(position))
                {
                    m_waypoints.erase(m_waypoints.begin());
                    m_flag4 = true;
                    m_wayPointError = false;
                }
                else if (m_wayPointError)
                {
                    area->randomizePos(bot->getSide(), 0.9f, m_waypoints.front().first);   // +0xE0
                    m_wayPointError = false;
                }
            }
            else if (m_wayPointError)
            {
                m_waypoints.erase(m_waypoints.begin());
                m_flag4 = true;
                m_wayPointError = false;
            }
        }
    }
    if (!m_waypoints.empty())
        return 1.0f;

    const bool wasInside = m_inside;
    m_inside = 10.0f > m_zone->getDistanceSqr(position);
    m_flag4 = m_inside != wasInside;
    m_distanceSqr = m_inside ? 0.0f : m_zone->getDistanceSqr(position);
    return 1.0f;
}

// ELF 0x1BAA20: the first waypoint while any remain; then the beach position inside the zone,
// the approach position (10 m) outside it.
bool WPMoveToBeachLanding::getGoalPoint(Vec3& out)
{
    if (!m_waypoints.empty())
    {
        out = m_waypoints.front().first;
        return true;
    }
    if (m_inside)
        return m_zone->getBeachPosition(m_map, out);
    return m_zone->getApproachPosition(m_map, m_approachType, out, 10.0f);
}

// ELF 0x1BAAC0
bool WPMoveToBeachLanding::insideZone()
{
    return m_inside;
}

// ELF 0x1BAAD0: distance^2 >= the last squared zone distance; unordered is false.
bool WPMoveToBeachLanding::closeToZone(float distance)
{
    return distance * distance >= m_distanceSqr;
}

// ELF 0x1BAAF0
void WPMoveToBeachLanding::getCoords(std::list<Vec3>& out) const
{
    for (std::list<std::pair<Vec3, int> >::const_iterator it = m_waypoints.begin();
         it != m_waypoints.end(); ++it)
        out.push_back(it->first);
}

// ELF 0x1BAB70: the constructor's radius.
float WPMoveToBeachLanding::getWayPointRadius() const
{
    return m_radius;
}

} // namespace ai
} // namespace hfe
} // namespace dice
