// Compares the bot's forward direction with a normalized object-position-plus-offset vector.

// static init: ELF 0x1E8DA0 (__tcf_0 0x1E8E00, _GLOBAL__I 0x1E8E30): <iostream>'s Init object only.
#include <iostream>

#include "BAPConPointAt.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E8AA0 (C1; C2 0x1E8A30)
BAPConPointAt::BAPConPointAt(unsigned int object, Vec3& offset, float angle)
    : m_object(object), m_cosAngle(std::cos(angle)), m_offset(offset)
{
}

// ELF 0x1E8B30 (D1; D2 0x1E8B10, D0 0x1E8B50)
BAPConPointAt::~BAPConPointAt()
{
}

// ELF 0x1E8B90
bool BAPConPointAt::evaluate(Bot* bot)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return true;
    // shipped defect: the bot's position is never subtracted from this world-space point.
    Vec3 direction = bapinline::add(object->getPosition(), m_offset);
    bapinline::normalize(direction);
    Information* self = bapinline::info(bot->getCurrentEquipment());
    const Mat4& transform = self->getTransformation();
    const Vec3 forward = bapinline::vec3(transform.m[8], transform.m[9], transform.m[10]);
    return bapinline::dot(direction, forward) >= m_cosAngle;
}

// ELF 0x1E8D90
BAPConditionType BAPConPointAt::getConditionType() const
{
    return BAPCTPointAt;
}

} // namespace ai
} // namespace hfe
} // namespace dice
