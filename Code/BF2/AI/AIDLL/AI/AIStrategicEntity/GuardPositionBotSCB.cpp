// GuardPositionBotSCB. Evidence: libaidll.so (ia-32) 0x..0x3501E0.
// [INFERRED] Source placement.

#include "GuardPositionBotSCB.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x350180, __tcf_0 0x3501E0, _GLOBAL__I 0x350210).
#include <iostream>

#include "GuardPosition.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x (C2; C1 0x34FFF0).
GuardPositionBotSCB::GuardPositionBotSCB(Bot* bot, GuardPosition* guardPosition)
    : IBotSpawnCallBack(bot)
    , m_guardPosition(guardPosition)
{
}

// ELF 0x350040 (D2; D1 0x350090, D0 0x3500D0).
GuardPositionBotSCB::~GuardPositionBotSCB()
{
}

// ELF 0x350110. Neither pointer is tested.
void GuardPositionBotSCB::reportSpawn()
{
    m_guardPosition->botIsAlive(m_bot);
}

} } }
