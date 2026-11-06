// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E9400 (__tcf_0 0x1E9460, _GLOBAL__I 0x1E9490): <iostream>'s Init object only.
#include <iostream>

#include "BAPConPointDistance.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"

#include <cmath>
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E8ED0 (C1)
BAPConPointDistance::BAPConPointDistance(const Vec3& point, float maxDistance, bool flat)
{
    m_minDistanceSqr = 0.0f;
    m_lastDistanceSqr = 0.0f;
    m_2d = flat;
    m_point = point;
    m_maxDistanceSqr = maxDistance * maxDistance;
}

// ELF 0x1E8FD0 (C1)
BAPConPointDistance::BAPConPointDistance(const Vec3& point, float minDistance, float maxDistance, bool flat)
{
    m_2d = flat;
    m_lastDistanceSqr = 0.0f;
    m_point = point;
    m_maxDistanceSqr = maxDistance * maxDistance;
    m_minDistanceSqr = minDistance * minDistance;
}

// ELF 0x1E9070 (D0 0x1E9090)
BAPConPointDistance::~BAPConPointDistance()
{
}

// ELF 0x1E90D0 (the information is not null-checked)
bool BAPConPointDistance::evaluate(Bot* bot)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    float a, b;
    if (!m_2d) {
        const Vec3& p = self->getPosition();
        b = (m_point.z - p.z) * (m_point.z - p.z);
        a = (m_point.x - p.x) * (m_point.x - p.x) + (m_point.y - p.y) * (m_point.y - p.y);
    } else {
        const float px = self->getPosition().x;
        const float dz = m_point.z - self->getPosition().z;
        a = (m_point.x - px) * (m_point.x - px);
        b = dz * dz;
    }
    const float d = a + b;
    m_lastDistanceSqr = d;
    return m_minDistanceSqr <= d && d <= m_maxDistanceSqr;
}

// ELF 0x1E9270
BAPConditionType BAPConPointDistance::getConditionType() const
{
    return BAPCTPointDistance;   // 12
}

// ELF 0x1E9280 (the label says "ObjectDistance" in the image)
void BAPConPointDistance::streamSelf(std::ostream& out) const
{
    out << "ObjectDistance[";
    if (m_lastDistanceSqr == 0.0f)
        out << "not calculated";
    else
        out << static_cast<double>(std::sqrt(m_lastDistanceSqr));
    out << "] < " << static_cast<double>(std::sqrt(m_maxDistanceSqr));
    if (m_minDistanceSqr != 0.0f)
        out << " && ObjectDistance[" << static_cast<double>(std::sqrt(m_lastDistanceSqr)) << "] > "
            << static_cast<double>(std::sqrt(m_minDistanceSqr));
}

} // namespace ai
} // namespace hfe
} // namespace dice
