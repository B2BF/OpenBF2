// Tests whether the object's speed is strictly below a fixed threshold.

// static init: ELF 0x1F01B0 (__tcf_0 0x1F0210, _GLOBAL__I 0x1F0240): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectSpeed.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F0060 (C1; C2 0x1F0020)
BAPConObjectSpeed::BAPConObjectSpeed(unsigned int object, float speed) : m_speed(speed)
{
    m_object = object;
}

// ELF 0x1F00C0 (D1; D2 0x1F00A0, D0 0x1F00E0)
BAPConObjectSpeed::~BAPConObjectSpeed()
{
}

// ELF 0x1F0120
bool BAPConObjectSpeed::evaluate(Bot*)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return false;
    IPIMobile* mobile = object->getMobile();
    if (!mobile)
        return true;
    return mobile->getSpeedMagnitude() < m_speed;
}

// ELF 0x1F01A0
BAPConditionType BAPConObjectSpeed::getConditionType() const
{
    return BAPCTObjectSpeed;
}

} // namespace ai
} // namespace hfe
} // namespace dice
