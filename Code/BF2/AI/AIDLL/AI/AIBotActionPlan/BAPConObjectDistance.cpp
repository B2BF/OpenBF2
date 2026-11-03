// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1ED840 (__tcf_0 0x1ED8A0, _GLOBAL__I 0x1ED8D0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectDistance.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"

#include <cmath>
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1ED0D0 (C1): max distance only.
BAPConObjectDistance::BAPConObjectDistance(unsigned int object, float maxDistance, bool estimate, bool flat,
                                           bool outside)
{
    m_object = object;
    m_minDistanceSqr = 0.0f;
    m_estimate = estimate;
    m_flat = flat;
    m_lastDistanceSqr = 0.0f;
    m_maxDistanceSqr = maxDistance * maxDistance;
    m_outside = outside;
}

// ELF 0x1ED190 (C1)
BAPConObjectDistance::BAPConObjectDistance(unsigned int object, float minDistance, float maxDistance, bool estimate,
                                           bool flat)
{
    m_object = object;
    m_estimate = estimate;
    m_outside = false;
    m_maxDistanceSqr = maxDistance * maxDistance;
    m_flat = flat;
    m_minDistanceSqr = minDistance * minDistance;
    m_lastDistanceSqr = 0.0f;
}

// ELF 0x1ED210 (D0 0x1ED230)
BAPConObjectDistance::~BAPConObjectDistance()
{
}

// ELF 0x1ED270 (the bot's information is not null-checked)
bool BAPConObjectDistance::evaluate(Bot* bot)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return true;
    Information* self = bapinline::info(bot->getCurrentEquipment());
    float a, b;   // the two partial sums the image adds last
    if (!m_estimate) {
        if (!m_flat) {
            const Vec3& s = self->getPosition();
            const Vec3& o = object->getPosition();
            const float dx = o.x - s.x;
            const float dy = o.y - s.y;
            const float dz = o.z - s.z;
            b = dz * dz;
            a = dx * dx + dy * dy;
        } else {
            const float sx = self->getPosition().x;
            const float sz = self->getPosition().z;
            const float ox = object->getPosition().x;
            const float oz = object->getPosition().z;
            b = (ox - sx) * (ox - sx);
            a = (oz - sz) * (oz - sz);
        }
    } else {
        Vec3 p = bapinline::vec3(0.0f, 0.0f, 0.0f);
        IPIMobile* mobile = object->getMobile();
        if (!mobile)
            p = object->getPosition();
        else
            mobile->getPositionEstimate(p);
        const Vec3& s = self->getPosition();
        if (!m_flat) {
            const float dx = p.x - s.x;
            const float dy = p.y - s.y;
            const float dz = p.z - s.z;
            b = dz * dz;
            a = dx * dx + dy * dy;
        } else {
            const float dx = p.x - s.x;
            const float dz = p.z - s.z;
            b = dx * dx;
            a = dz * dz;
        }
    }
    const float d = a + b;
    m_lastDistanceSqr = d;
    if (!m_outside)
        return m_minDistanceSqr <= d && d <= m_maxDistanceSqr;
    return m_maxDistanceSqr < d;
}

// ELF 0x1ED640
BAPConditionType BAPConObjectDistance::getConditionType() const
{
    return BAPCTObjectDistance;   // 18
}

// ELF 0x1ED650
void BAPConObjectDistance::streamSelf(std::ostream& out) const
{
    if (!m_outside) {
        out << "ObjectDistance(" << static_cast<double>(std::sqrt(m_minDistanceSqr)) << " <= [";
        if (m_lastDistanceSqr == 0.0f)
            out << "not calculated";
        else
            out << static_cast<double>(std::sqrt(m_lastDistanceSqr));
        out << "] <= " << static_cast<double>(std::sqrt(m_maxDistanceSqr)) << ")";
    } else {
        out << "ObjectDistance[";
        if (m_lastDistanceSqr == 0.0f)
            out << "not calculated";
        else
            out << static_cast<double>(std::sqrt(m_lastDistanceSqr));
        out << "] > " << static_cast<double>(std::sqrt(m_maxDistanceSqr));
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
