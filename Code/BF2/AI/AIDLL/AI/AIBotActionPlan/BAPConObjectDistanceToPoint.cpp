// Object-to-point distance condition recovered from libaidll.so.
// static init: ELF 0x1EDF10 (__tcf_0 0x1EDF70, _GLOBAL__I 0x1EDFA0): <iostream>'s Init object only.
#include <iostream>
#include "BAPConObjectDistanceToPoint.h"
#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include <cmath>
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1ED980 (C1; C2 0x1ED8F0)
BAPConObjectDistanceToPoint::BAPConObjectDistanceToPoint(const Vec3& point, unsigned int object,
                                                       float maxDistance, bool estimate, bool flat)
    : m_minDistanceSqr(0.0f), m_maxDistanceSqr(maxDistance * maxDistance),
      m_estimate(estimate), m_flat(flat), m_point(point), m_lastDistanceSqr(0.0f)
{
    m_object = object;
}

// ELF 0x1EDAA0 (C1; C2 0x1EDA10)
BAPConObjectDistanceToPoint::BAPConObjectDistanceToPoint(const Vec3& point, unsigned int object,
                                                       float minDistance, float maxDistance,
                                                       bool estimate, bool flat)
    : m_minDistanceSqr(minDistance * minDistance), m_maxDistanceSqr(maxDistance * maxDistance),
      m_estimate(estimate), m_flat(flat), m_point(point), m_lastDistanceSqr(0.0f)
{
    m_object = object;
}

// ELF 0x1EDB50 (D1; D2 0x1EDB30, D0 0x1EDB70)
BAPConObjectDistanceToPoint::~BAPConObjectDistanceToPoint()
{
}

// ELF 0x1EDBB0
bool BAPConObjectDistanceToPoint::evaluate(Bot*)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return true;
    float distanceSqr;
    if (m_estimate) {
        IPIMobile* mobile = object->getMobile();
        Vec3 position = bapinline::vec3(0.0f, 0.0f, 0.0f);
        if (mobile)
            mobile->getPositionEstimate(position);
        else
            position = object->getPosition();
        if (m_flat)
            distanceSqr = distXZSqr(position, m_point);
        else {
            const Vec3 delta = bapinline::sub(position, m_point);
            distanceSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
        }
    } else if (m_flat) {
        distanceSqr = distXZSqr(object->getPosition(), m_point);
    } else {
        const Vec3 delta = bapinline::sub(object->getPosition(), m_point);
        distanceSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    }
    m_lastDistanceSqr = distanceSqr;
    return m_minDistanceSqr <= distanceSqr && distanceSqr <= m_maxDistanceSqr;
}

// ELF 0x1EDF00
BAPConditionType BAPConObjectDistanceToPoint::getConditionType() const
{
    return BAPCTObjectDistanceToPoint;
}

// ELF 0x1EDD80
void BAPConObjectDistanceToPoint::streamSelf(std::ostream& out) const
{
    out << "PointDistance[";
    if (m_lastDistanceSqr == 0.0f)
        out << "not calculated";
    else
        out << static_cast<double>(std::sqrt(m_lastDistanceSqr));
    out << "] < " << static_cast<double>(std::sqrt(m_maxDistanceSqr));
    if (m_minDistanceSqr != 0.0f)
        out << " && PointDistance[" << static_cast<double>(std::sqrt(m_lastDistanceSqr))
            << "] > " << static_cast<double>(std::sqrt(m_minDistanceSqr));
}

} // namespace ai
} // namespace hfe
} // namespace dice
