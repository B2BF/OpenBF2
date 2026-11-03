// GuardPositionBotDCB. Evidence: libaidll.so (ia-32) 0x..0x34FFA0.
// [INFERRED] Source placement.

#include "GuardPositionBotDCB.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x34FF40, __tcf_0 0x34FFA0, _GLOBAL__I 0x34FFD0).
#include <iostream>

#include "GuardPosition.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x (C2; C1 0x34FDB0).
GuardPositionBotDCB::GuardPositionBotDCB(Bot* bot, GuardPosition* guardPosition)
    : IBotDeathCallBack(bot)
    , m_guardPosition(guardPosition)
{
}

// ELF 0x34FE00 (D2; D1 0x34FE50, D0 0x34FE90).
GuardPositionBotDCB::~GuardPositionBotDCB()
{
}

// ELF 0x34FED0. Neither pointer is tested.
void GuardPositionBotDCB::reportDeath()
{
    m_guardPosition->botIsDead(m_bot);
}

} } }
