// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1EE350 (__tcf_0 0x1EE3B0, _GLOBAL__I 0x1EE3E0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectExists.h"

#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EE240 (C1; C2 0x1EE210)
BAPConObjectExists::BAPConObjectExists(unsigned int object)
{
    m_object = object;
}

// ELF 0x1EE2E0 (C1 0x1EE2C0, D0 0x1EE300): base only.
BAPConObjectExists::~BAPConObjectExists()
{
}

// ELF 0x1EE270
bool BAPConObjectExists::evaluate(Bot*)
{
    return bapinline::info(m_object) != 0;
}

// ELF 0x1EE340
BAPConditionType BAPConObjectExists::getConditionType() const
{
    return BAPCTObjectExists;
}

} // namespace ai
} // namespace hfe
} // namespace dice
