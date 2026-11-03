// BAPConFalse: recovered by ai-main from libaidll.so (0x1C1920..0x1C1990; S0 skeleton file).
// Every body carries its ELF address.

#include "BAPConFalse.h"

#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1C1940 (D1, D0 0x1C1960): only the vtable store of the (empty) BAPCondition base
// (GOT -0x1550 + 8); nothing of its own.
BAPConFalse::~BAPConFalse()
{
}

// ELF 0x1C1920: `xorl %eax, %eax; ret`.
bool BAPConFalse::evaluate(Bot*)
{
    return false;
}

// ELF 0x1C1930: `movl $0x6, %eax; ret` (6 = BAPCTFalse).
BAPConditionType BAPConFalse::getConditionType() const
{
    return BAPCTFalse;
}

} // namespace ai
} // namespace hfe
} // namespace dice
