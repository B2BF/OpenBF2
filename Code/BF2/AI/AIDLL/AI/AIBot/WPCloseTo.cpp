// A waypoint list that closes in on one object. Evidence: libaidll.so 1.5.3153 (ia-32), see
// WPCloseTo.h.

#include "WPCloseTo.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B8F90, __tcf_0 0x1B8FF0, _GLOBAL__I 0x1B9020).
#include <iostream>

#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../../Math/Mat4.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {
// Inlined in the constructor and getUrgency: without a physical plug-in the object's position,
// else the sphere's local offset rotated by the transformation (rows 0..2) plus the position.
Vec3 closeToPosition(Information* info)
{
    IPIPhysical* const physical = info->getPhysical();
    if (!physical)
        return info->getPosition();
    const Mat4& m = info->getTransformation();
    const Vec3 offset = physical->getSphereLocalOffset();
    const float* const r = m.m;
    Vec3 point;
    point.x = r[8] * offset.z + r[0] * offset.x + r[4] * offset.y;
    point.y = r[1] * offset.x + r[5] * offset.y + r[9] * offset.z;
    point.z = offset.x * r[2] + offset.y * r[6] + offset.z * r[10];
    const Vec3& position = info->getPosition();
    point.x = position.x + point.x;
    point.y = position.y + point.y;
    point.z = position.z + point.z;
    return point;
}
} // namespace

// ELF 0x1B8780: the radius is at least 5.0f, and with a physical plug-in at least twice its
// smallest radius (x87 compare against 2r, stored as float).
WPCloseTo::WPCloseTo(unsigned int target, float radius)
    : m_int12(2)
    , m_target(target)
    , m_radius(radius <= 5.0f ? 5.0f : radius)
    , m_distanceSq(25.0f)
{
    m_flag4 = true;
    m_flag5 = false;
    m_wayPointError = false;
    m_goal.x = 0.0f;
    m_goal.y = 0.0f;
    m_goal.z = 0.0f;
    Information* const info = informationManager->getInformation(target);
    if (!info)
        return;
    if (IPIPhysical* const physical = info->getPhysical())
    {
        const float smallest = physical->getSmallestRadius();
        const float twice = smallest + smallest;
        if (m_radius <= twice)
            m_radius = twice;
    }
    m_goal = closeToPosition(info);
}

WPCloseTo::~WPCloseTo()
{
}

// ELF 0x1B8BA0. 0 after a waypoint error, or (marking +5) when the object is gone. Otherwise the
// squared distance d from `position` to the object is kept (+0x18) and the urgency is
// min(1, d / (4 r^2)); inside r^2 the list is marked (+4) and the urgency is 0; outside, a goal
// that moved more than r (squared distances) is refreshed and the list marked.
float WPCloseTo::getUrgency(const Vec3& position, Bot*)
{
    if (m_wayPointError)
        return 0.0f;
    Information* const info = informationManager->getInformation(m_target);
    if (!info)
    {
        m_flag5 = true;
        return 0.0f;
    }
    const Vec3 point = closeToPosition(info);
    const float dx = point.x - position.x;
    const float dy = point.y - position.y;
    const float dz = point.z - position.z;
    const float distanceSq = dx * dx + dy * dy + dz * dz;
    m_distanceSq = distanceSq;
    const float radiusSq = m_radius * m_radius;
    float urgency = distanceSq / (radiusSq * 4.0f);
    if (1.0f < urgency)
        urgency = 1.0f;
    if (radiusSq <= distanceSq)
    {
        const float gx = m_goal.x - point.x;
        const float gy = m_goal.y - point.y;
        const float gz = m_goal.z - point.z;
        if (radiusSq < gx * gx + gy * gy + gz * gz)
        {
            m_goal = point;
            m_flag4 = true;
        }
        return urgency;
    }
    m_flag4 = true;
    return 0.0f;
}

// ELF 0x1B8F00: the goal while the object exists.
bool WPCloseTo::getGoalPoint(Vec3& out)
{
    if (!informationManager->getInformation(m_target))
        return false;
    out = m_goal;
    return true;
}

// ELF 0x1B8E30. Without the object the speed stands. Without a mobile plug-in: 0 once the radius
// reaches the kept distance (+0x14 against +0x18 as stored), else the speed. With one: within
// that, at most the object's speed magnitude (IPIMobile +0x24); beyond twice the distance the
// speed stands; in between at most magnitude * 1.33 (0x49D240) * d / r.
float WPCloseTo::modifyMaxSpeed(float speed)
{
    Information* const info = informationManager->getInformation(m_target);
    if (!info)
        return speed;
    IPIMobile* const mobile = info->getMobile();
    if (!mobile)
        return m_radius < m_distanceSq ? speed : 0.0f;
    float limit;
    if (m_radius >= m_distanceSq)
    {
        limit = mobile->getSpeedMagnitude();
    }
    else
    {
        if (m_radius < m_distanceSq + m_distanceSq)
            return speed;
        limit = mobile->getSpeedMagnitude() * 1.33f * m_distanceSq / m_radius;
    }
    return limit > speed ? speed : limit;
}

} // namespace ai
} // namespace hfe
} // namespace dice
