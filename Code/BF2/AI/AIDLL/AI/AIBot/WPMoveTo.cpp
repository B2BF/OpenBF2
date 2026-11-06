// A walk-through waypoint list. Evidence: libaidll.so 1.5.3153 (ia-32), see WPMoveTo.h.

#include "WPMoveTo.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1BA040, __tcf_0 0x1BA0A0, _GLOBAL__I 0x1BA0D0).
#include <iostream>

#include "../Main/AIDllFrontier.h"
#include "BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B96C0 (C1; C2 0x1B97E0). The base fields (+4 true, +5 / +6 false), then +8 = -1.0f,
// the clamp, +0x10..+0x18, the empty list, +0x20 = false, +0xC = 0, a clear, and the copy.
WPMoveTo::WPMoveTo(float radius, float stepRadius, unsigned int flags, const GoalList& goals)
{
    if (stepRadius <= 1.0f)
        stepRadius = 1.0f;
    m_radius = radius;
    m_stepRadius = stepRadius;
    m_flags = flags;
    m_flag20 = false;
    m_int0C = 0;
    m_goals.clear();
    for (GoalList::const_iterator it = goals.begin(); it != goals.end(); ++it)
        m_goals.push_back(*it);
}

// ELF 0x1B92F0 (C1; C2 0x1B9530). As above with a clamp at 2.0; every position is paired with -1
// and the last goal then takes `lastArea`.
WPMoveTo::WPMoveTo(float radius, float stepRadius, unsigned int flags, const std::list<Vec3>& positions, int lastArea)
{
    if (stepRadius <= 2.0f)
        stepRadius = 2.0f;
    m_radius = radius;
    m_stepRadius = stepRadius;
    m_flags = flags;
    m_flag20 = false;
    m_int0C = 0;
    m_goals.clear();
    for (std::list<Vec3>::const_iterator it = positions.begin(); it != positions.end(); ++it)
        m_goals.push_back(std::make_pair(*it, -1));
    if (!m_goals.empty())
        m_goals.back().second = lastArea;
}

// ELF 0x1B9900: the list, then the base vptr.
WPMoveTo::~WPMoveTo()
{
}

// ELF 0x1B9A30 (1344 bytes). r is max(ceil(radius), the equipment's smallest radius) for the last
// goal; max(ceil(step radius), smallest radius) for an intermediate goal, or max(smallest radius, 10)
// for one without an area. d^2 is the squared distance from `position` to the front goal.
// * No goals: done (+5), 0.
// * The last goal within r: done (+5), +6 cleared, 0. Beyond r: clamp(d^2 / (1.5 r^2), 0.1, 1) times
//   2 for an area the side holds (status != 0), 0 inside an area it does not hold (d^2 = 0), else 1
//   with d^2 reduced by the side's area radius squared; without an area 1, or 0 when +6 is set.
//   A set +6 moves the goal inside the area (randomizePos(side, 0.9)) and is cleared. +5 = d^2 < 1.5 r^2.
// * An intermediate goal is popped (+4 set, +6 cleared) within r, or beyond it when the bot is
//   inside the goal's area, or with +6 set and no area; with +6 set outside the area it is moved
//   instead. Always 1.
float WPMoveTo::getUrgency(const Vec3& position, Bot* bot)
{
    if (m_goals.empty())
    {
        m_flag5 = true;
        return 0.0f;
    }
    IPIPhysical* const physical =
        informationManager->getInformation(bot->getCurrentEquipment())->getPhysical();
    const Vec3& goal = m_goals.front().first;
    const float dx = goal.x - position.x;
    const float dy = goal.y - position.y;
    const float dz = goal.z - position.z;
    float distanceSq = dz * dz + dy * dy + dx * dx;

    if (m_goals.size() == 1)
    {
        float radius = static_cast<float>(std::ceil(static_cast<double>(m_radius)));
        const float smallest = physical->getSmallestRadius();
        if (radius < smallest)
            radius = smallest;
        const float radiusSq = radius * radius;
        if (!(radiusSq <= distanceSq))
        {
            m_flag5 = true;
            m_wayPointError = false;
            return 0.0f;
        }
        float factor;
        AIStrategicArea* const area =
            AIStrategicObjectManager::getInstance()->getStrategicArea(m_goals.front().second);
        if (!area)
        {
            factor = m_wayPointError ? 0.0f : 1.0f;
        }
        else
        {
            const int side = bot->getSide();
            if (static_cast<int>(area->getStatus(side)) == 0)
            {
                if (!area->isInside(position))
                {
                    // +0x98 + 4 * side: m_size[side].
                    const float sideRadius = area->m_size[bot->getSide()];
                    distanceSq = distanceSq - sideRadius * sideRadius;
                    factor = 1.0f;
                }
                else
                {
                    factor = 0.0f;
                    distanceSq = 0.0f;
                }
            }
            else
            {
                factor = 2.0f;
            }
            if (m_wayPointError)
            {
                area->randomizePos(bot->getSide(), 0.9f, m_goals.front().first);
                m_wayPointError = false;
            }
        }
        const float scaled = radiusSq * 1.5f;
        float urgency = distanceSq / scaled;
        m_flag5 = !(scaled <= distanceSq);
        if (1.0f < urgency)
            urgency = 1.0f;
        if (!(0.1f <= urgency))
            urgency = 0.1f;
        return urgency * factor;
    }

    float radius = static_cast<float>(std::ceil(static_cast<double>(m_stepRadius)));
    float smallest = physical->getSmallestRadius();
    if (radius < smallest)
        radius = smallest;
    if (m_goals.front().second == -1)
    {
        radius = physical->getSmallestRadius();
        if (radius <= 10.0f)
            radius = 10.0f;
    }
    if (radius * radius <= distanceSq)
    {
        AIStrategicArea* const area =
            AIStrategicObjectManager::getInstance()->getStrategicArea(m_goals.front().second);
        if (!area)
        {
            if (!m_wayPointError)
                return 1.0f;
        }
        else if (!area->isInside(position))
        {
            if (!m_wayPointError)
                return 1.0f;
            area->randomizePos(bot->getSide(), 0.9f, m_goals.front().first);
            m_wayPointError = false;
            return 1.0f;
        }
    }
    m_goals.erase(m_goals.begin());
    m_flag4 = true;
    m_wayPointError = false;
    return 1.0f;
}

// ELF 0x1B9F70: the first goal's position when there is one.
bool WPMoveTo::getGoalPoint(Vec3& out)
{
    const bool any = !m_goals.empty();
    if (any)
        out = m_goals.front().first;
    return any;
}

// ELF 0x1B9FB0: each goal position appended to `out`.
void WPMoveTo::getCoords(std::list<Vec3>& out) const
{
    for (GoalList::const_iterator it = m_goals.begin(); it != m_goals.end(); ++it)
        out.push_back(it->first);
}

// ELF 0x1BA030: `flds 0x10(this)`.
float WPMoveTo::getWayPointRadius() const
{
    return m_radius;
}

} // namespace ai
} // namespace hfe
} // namespace dice
