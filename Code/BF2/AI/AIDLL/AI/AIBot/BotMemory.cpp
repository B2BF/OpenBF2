// [INFERRED] Source placement.

#include "BotMemory.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x18F320, __tcf_0 0x18F380, _GLOBAL__I 0x18F3B0).
#include <iostream>

#include "../AIBotBehaviour/BotBehaviour.h"
#include "../AIBotBehaviour/UrgeCurve.h"

namespace dice {
namespace hfe {
namespace ai {

// Initializes behavior-memory sentinels. [UNRESOLVED] The fields' precise meanings remain unknown.
BotMemory::BotMemory()
    : m_int0(-1)
    , m_int4(-1)
    , m_float8(1.0f)
    , m_int12(1)
{
    // m_double16 is deliberately not initialised here. `BotMain`'s constructor writes it from
    // outside, immediately after allocating this object. Adding it would be an invented guard.
}

// Intentionally empty destructor.
BotMemory::~BotMemory()
{
}

// ELF 0x18F2A0: the constructor's four stores again, in the image's order
// (+4 = -1, +0xC = 1, +8 = 1.0f from GOT-0x16598 -> 0x49CB2C 0x3F800000, +0 = -1).
// m_double16 is not touched.
void BotMemory::reset()
{
    m_int4 = -1;
    m_int12 = 1;
    m_float8 = 1.0f;
    m_int0 = -1;
}

// ELF 0x18F2D0: for the remembered behaviour (index byte +4 == m_int0) the behaviour's urge
// curve (+0x20, vtable +8 calculate) is evaluated at (float)(time - m_double16)
// (`fldl 0x10(%ebp); fsubl 0x10(%ecx); fstps`); any other behaviour gets 1.0f
// (`flds -0x16598(%ebx)`, 0x49CB2C = 0x3F800000).
float BotMemory::modifyBehaviourUrgency(BotBehaviour* behaviour, double time)
{
    if (static_cast<int>(behaviour->getIndex()) == m_int0)
        return behaviour->getUrgeCurve()->calculate(static_cast<float>(time - m_double16));
    return 1.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
