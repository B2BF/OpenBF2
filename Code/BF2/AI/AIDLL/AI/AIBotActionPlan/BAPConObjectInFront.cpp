// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// [PLATFORM] precision: the x87 build adds the scaled forward axis to the position in extended
// precision before rounding the point.

// static init: ELF 0x1EE9E0 (__tcf_0 0x1EEA40, _GLOBAL__I 0x1EEA70): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectInFront.h"

#include "../../../../Math/Mat4.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EE6D0 (C1; C2 0x1EE690)
BAPConObjectInFront::BAPConObjectInFront(unsigned int object, float distance, float minDot)
{
    m_object = object;
    m_distance = distance;
    m_minDot = minDot;
}

// ELF 0x1EE730 (D1 0x1EE710, D0 0x1EE750): base only.
BAPConObjectInFront::~BAPConObjectInFront()
{
}

// ELF 0x1EE790. No information on the object or the bot's vehicle: false.
bool BAPConObjectInFront::evaluate(Bot* bot)
{
    Information* const target = bapinline::info(m_object);
    if (!target)
        return false;
    Information* const self = bapinline::info(bot->getCurrentEquipment());
    if (!self)
        return false;
    const Mat4& transformation = self->getTransformation();
    const Vec3 forward = bapinline::vec3(transformation.m[8], transformation.m[9], transformation.m[10]);
    const Vec3& position = self->getPosition();
    const Vec3 ahead = bapinline::vec3(forward.x * m_distance, forward.y * m_distance, forward.z * m_distance);
    const Vec3 front = bapinline::add(ahead, position);
    Vec3 direction = bapinline::sub(target->getPosition(), front);
    bapinline::normalize(direction);
    return bapinline::dot(forward, direction) > m_minDot;
}

// ELF 0x1EE9D0
BAPConditionType BAPConObjectInFront::getConditionType() const
{
    return BAPCTObjectInFront;
}

} // namespace ai
} // namespace hfe
} // namespace dice
