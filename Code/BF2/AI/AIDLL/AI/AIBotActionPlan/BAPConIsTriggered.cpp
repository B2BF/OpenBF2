// Tests the borrowed trigger action's started flag.

// static init: ELF 0x1E7780 (__tcf_0 0x1E77E0, _GLOBAL__I 0x1E7810): <iostream>'s Init object only.
#include <iostream>

#include "BAPConIsTriggered.h"

#include "BAPATrigger.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E76B0 (C1; C2 0x1E7680)
BAPConIsTriggered::BAPConIsTriggered(BAPATrigger* trigger) : m_trigger(trigger)
{
}

// ELF 0x1E7710 (D1; D2 0x1E76F0, D0 0x1E7730)
BAPConIsTriggered::~BAPConIsTriggered()
{
}

// ELF 0x1E76E0
bool BAPConIsTriggered::evaluate(Bot*)
{
    return m_trigger->m_started;
}

// ELF 0x1E7770
BAPConditionType BAPConIsTriggered::getConditionType() const
{
    return BAPCTIsTriggered;
}

} // namespace ai
} // namespace hfe
} // namespace dice
