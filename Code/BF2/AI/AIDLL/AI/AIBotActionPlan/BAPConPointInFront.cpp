// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// [PLATFORM] precision: the x87 build adds the scaled forward axis to the position in extended
// precision before rounding the point.

// static init: ELF 0x1E9830 (__tcf_0 0x1E9890, _GLOBAL__I 0x1E98C0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConPointInFront.h"

#include "../../../../Math/Mat4.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E9520 (C1; C2 0x1E94B0)
BAPConPointInFront::BAPConPointInFront(const Vec3& point, float distance, float minDot)
{
    m_distance = distance;
    m_minDot = minDot;
    m_point = point;
}

// ELF 0x1E95B0 (D1 0x1E9590, D0 0x1E95D0): base only.
BAPConPointInFront::~BAPConPointInFront()
{
}

// ELF 0x1E9610. No information on the bot's vehicle: false.
bool BAPConPointInFront::evaluate(Bot* bot)
{
    Information* const self = bapinline::info(bot->getCurrentEquipment());
    if (!self)
        return false;
    const Mat4& transformation = self->getTransformation();
    const Vec3 forward = bapinline::vec3(transformation.m[8], transformation.m[9], transformation.m[10]);
    const Vec3& position = self->getPosition();
    const Vec3 ahead = bapinline::vec3(forward.x * m_distance, forward.y * m_distance, forward.z * m_distance);
    const Vec3 front = bapinline::add(position, ahead);
    Vec3 direction = bapinline::sub(m_point, front);
    bapinline::normalize(direction);
    return bapinline::dot(forward, direction) > m_minDot;
}

// ELF 0x1E9820: ObjectInFront, as shipped.
BAPConditionType BAPConPointInFront::getConditionType() const
{
    return BAPCTObjectInFront;
}

} // namespace ai
} // namespace hfe
} // namespace dice
