// Requests a horn notification for one information handle; execution returns false as shipped.

// static init: ELF 0x1D2780 (__tcf_0 0x1D27E0, _GLOBAL__I 0x1D2810): <iostream>'s Init object only.
#include <iostream>

#include "BAPAHonkHorn.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D25D0 (C1; C2 0x1D2570)
BAPAHonkHorn::BAPAHonkHorn(unsigned int object, int tag) : BAPAction(UninitializedPatternId())
{
    m_patternId = tag;
    m_object = object;
}

// ELF 0x1D2670 (D1; D2 0x1D2630, D0 0x1D26B0)
BAPAHonkHorn::~BAPAHonkHorn()
{
}

// ELF 0x1D2700
bool BAPAHonkHorn::execute(Bot* bot, bool& success, bool& finished)
{
    Information* object = bapinline::info(m_object);
    bot->getCommunicationAgent()->honkHorn(object);
    success = false;
    finished = true;
    return false;
}

// ELF 0x1D2830
BAPStatementType BAPAHonkHorn::getId() const
{
    return BAPSTHonkHorn;
}

// ELF 0x1D2840
bool BAPAHonkHorn::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
