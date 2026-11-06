// Tests the inverse of the object's physical in-air state.

// static init: ELF 0x1F03E0 (__tcf_0 0x1F0440, _GLOBAL__I 0x1F0470): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectTouchingLand.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F02A0 (C1; C2 0x1F0260)
BAPConObjectTouchingLand::BAPConObjectTouchingLand(unsigned int object)
{
    m_object = object;
}

// ELF 0x1F0300 (D1; D2 0x1F02E0, D0 0x1F0320)
BAPConObjectTouchingLand::~BAPConObjectTouchingLand()
{
}

// ELF 0x1F0360
bool BAPConObjectTouchingLand::evaluate(Bot*)
{
    Information* object = bapinline::info(m_object);
    if (!object)
        return false;
    IPIPhysical* physical = object->getPhysical();
    if (!physical)
        return false;
    return !physical->isInAir();
}

// ELF 0x1F0490
BAPConditionType BAPConObjectTouchingLand::getConditionType() const
{
    return BAPCTObjectTouchingLand;
}

} // namespace ai
} // namespace hfe
} // namespace dice
