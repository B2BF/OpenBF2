// BAPConIteration: recovered by package A-core from libaidll.so (TU without a FILE symbol,
// 0x1E7830..0x1E7940). Every body carries its ELF address.

#include "BAPConIteration.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E7830 (C2; C1 0x1E7860).
BAPConIteration::BAPConIteration(int count)
    : m_count(count)
{
}

// ELF 0x1E7890 (D2; D1 0x1E78B0, D0 0x1E78D0): the vptr store only.
BAPConIteration::~BAPConIteration()
{
}

// ELF 0x1E7910.
bool BAPConIteration::evaluate(Bot*)
{
    return --m_count <= 0;
}

// ELF 0x1E7930.
BAPConditionType BAPConIteration::getConditionType() const
{
    return BAPCTIteration;
}

} // namespace ai
} // namespace hfe
} // namespace dice
