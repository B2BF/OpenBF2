// Reads the borrowed triggerable's active-state byte without a null guard.

// static init: ELF 0x1EB000 (__tcf_0 0x1EB060, _GLOBAL__I 0x1EB090): <iostream>'s Init object only.
#include <iostream>

#include "BAPConTriggerableActive.h"

#include "../../../../Game/Objects/Game/Triggerable.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EAF20 (C1; C2 0x1EAEF0)
BAPConTriggerableActive::BAPConTriggerableActive(world::Triggerable* triggerable)
    : m_triggerable(triggerable)
{
}

// ELF 0x1EAF90 (D1; D2 0x1EAF70, D0 0x1EAFB0)
BAPConTriggerableActive::~BAPConTriggerableActive()
{
}

// ELF 0x1EAF50
bool BAPConTriggerableActive::evaluate(Bot*)
{
    return m_triggerable->m_bool16C;
}

// ELF 0x1EAFF0
BAPConditionType BAPConTriggerableActive::getConditionType() const
{
    return BAPCTTriggerableActive;
}

} // namespace ai
} // namespace hfe
} // namespace dice
