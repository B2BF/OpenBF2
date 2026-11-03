// EntryCarMoveToDirection (split out of BAPEntries.cpp by S0, bodies unchanged).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate execute.

// static init: ELF 0x2B4540 (__tcf_0 0x2B45A0, _GLOBAL__I 0x2B45D0): <iostream>'s Init object only.
#include <iostream>

#include "EntryCarMoveToDirection.h"

#include "../Main/AIDllFrontier.h"

namespace dice {
namespace hfe {
namespace ai {

EntryCarMoveToDirection::EntryCarMoveToDirection()
{
}

EntryCarMoveToDirection::~EntryCarMoveToDirection()
{
}

int EntryCarMoveToDirection::getTargetId()
{
    return 8;
}

// A retail stub - see the block above. , and all it does is succeed.
bool EntryCarMoveToDirection::execute(BotScript *, Bot *, bool&, bool&)
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
