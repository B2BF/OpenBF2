// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E9CD0 (__tcf_0 0x1E9D30, _GLOBAL__I 0x1E9D60): <iostream>'s Init object only.
#include <iostream>

#include "BAPConPosition.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E9950 (C1): `new Vec3(point)` (operator new 0xC) at +0x08.
BAPConPosition::BAPConPosition(const Vec3& point, float radius)
{
    setOwner(0);
    Vec3* p = new Vec3(point);
    m_point = &p->x;
    m_is3d = true;
    m_radiusSqr = radius * radius;
}

// ELF 0x1E9A30 (C1): `new Vec2(point)` (operator new 8).
BAPConPosition::BAPConPosition(const Vec2& point, float radius)
{
    setOwner(0);
    Vec2* p = new Vec2(point);
    m_point = &p->x;
    m_is3d = false;
    m_radiusSqr = radius * radius;
}

// ELF 0x1E9AF0 (D0 0x1E9B40): plain operator delete of the point.
BAPConPosition::~BAPConPosition()
{
    ::operator delete(m_point);
}

// ELF 0x1E9B90: 3d `<=`, 2d (x, z against the Vec2) strict `<`.
bool BAPConPosition::evaluate(Bot* bot)
{
    Information* info = bapinline::info(bot->getCurrentEquipment());
    if (!info)
        return true;
    if (!m_is3d) {
        const Vec3& pos = info->getPosition();
        const float dx = pos.x - m_point[0];
        const float dz = pos.z - m_point[1];
        return dz * dz + dx * dx < m_radiusSqr;
    }
    const Vec3& pos = info->getPosition();
    const float dx = pos.x - m_point[0];
    const float dy = pos.y - m_point[1];
    const float dz = pos.z - m_point[2];
    return dx * dx + dy * dy + dz * dz <= m_radiusSqr;
}

// ELF 0x1E9CC0
BAPConditionType BAPConPosition::getConditionType() const
{
    return BAPCTPosition;   // 37
}

} // namespace ai
} // namespace hfe
} // namespace dice
