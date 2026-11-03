// BAPCondition: recovered by package A-core from libaidll.so (TU BAPCondition.cpp, 0x1F8090..0x1F8210).
// Every body carries its ELF address.

// static init: ELF 0x1F8100 (__tcf_0 0x1F8160, _GLOBAL__I 0x1F8190): <iostream>'s Init object only.
#include <iostream>

#include "BAPCondition.h"
#include "../Main/AIStreamOperators.h"


#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F81F0 (D1; D0 0x1F81B0): the vptr store only.
BAPCondition::~BAPCondition()
{
}

// ELF 0x1F8090: os << getConditionType() (slot +4) with operator<<(ostream&, BAPConditionType)
// 0x3C7AE0 (Main/AIStreamOperators.cpp). No endl. Only plan dumps reach it.
void BAPCondition::streamSelf(std::ostream& os) const
{
    os << getConditionType();
}

// ELF 0x1F80D0.
std::ostream& operator<<(std::ostream& os, const BAPCondition& condition)
{
    condition.streamSelf(os);
    return os;
}

} // namespace ai
} // namespace hfe
} // namespace dice
