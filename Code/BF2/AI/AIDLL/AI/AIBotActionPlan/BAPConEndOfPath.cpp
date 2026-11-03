// End-of-path condition from libaidll.so. The shipped evaluator always returns false.

// static init: ELF 0x1E7180 (__tcf_0 0x1E71E0, _GLOBAL__I 0x1E7210): <iostream>'s Init object only.
#include <iostream>

#include "BAPConEndOfPath.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E7230 (D1; D0 0x1E7250): base destruction only.
BAPConEndOfPath::~BAPConEndOfPath()
{
}

// ELF 0x1E7170
bool BAPConEndOfPath::evaluate(Bot*)
{
    return false;
}

// ELF 0x1E7290
BAPConditionType BAPConEndOfPath::getConditionType() const
{
    return BAPCTEndOfPath;
}

} // namespace ai
} // namespace hfe
} // namespace dice
