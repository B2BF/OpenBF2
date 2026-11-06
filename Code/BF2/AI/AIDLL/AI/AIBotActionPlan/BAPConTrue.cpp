// BAPConTrue: recovered by ai-main from libaidll.so (0x1C18A0..0x1C1910; S0 skeleton file).
// Every body carries its ELF address.

#include "BAPConTrue.h"

#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1C18C0 (D1, D0 0x1C18E0): only the vtable store of the (empty) BAPCondition base
// (GOT -0x1550 + 8); nothing of its own.
BAPConTrue::~BAPConTrue()
{
}

// ELF 0x1C18A0: `movl $0x1, %eax; ret`.
bool BAPConTrue::evaluate(Bot*)
{
    return true;
}

// ELF 0x1C18B0: `movl $0x28, %eax; ret` (40 = BAPCTTrue).
BAPConditionType BAPConTrue::getConditionType() const
{
    return BAPCTTrue;
}

} // namespace ai
} // namespace hfe
} // namespace dice
