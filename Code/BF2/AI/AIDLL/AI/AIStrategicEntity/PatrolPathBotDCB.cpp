// PatrolPathBotDCB. Evidence: libaidll.so (ia-32) 0x351090..0x3512B0.
// [INFERRED] Source placement.

#include "PatrolPathBotDCB.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x351220, __tcf_0 0x351280, _GLOBAL__I 0x3512B0).
#include <iostream>

#include "PatrolPath.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x351090 (C2; C1 0x3510E0).
PatrolPathBotDCB::PatrolPathBotDCB(Bot* bot, PatrolPath* patrolPath)
    : IBotDeathCallBack(bot)
    , m_patrolPath(patrolPath)
{
}

// ELF 0x351130 (D2; D1 0x351170, D0 0x3511B0).
PatrolPathBotDCB::~PatrolPathBotDCB()
{
}

// ELF 0x3511F0. Neither pointer is tested.
void PatrolPathBotDCB::reportDeath()
{
    m_patrolPath->botIsDead(m_bot);
}

} } }
