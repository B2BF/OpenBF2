// PatrolPathBotSCB. Evidence: libaidll.so (ia-32) 0x3512D0..0x3514F0.
// [INFERRED] Source placement.

#include "PatrolPathBotSCB.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x351460, __tcf_0 0x3514C0, _GLOBAL__I 0x3514F0).
#include <iostream>

#include "PatrolPath.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x3512D0 (C2; C1 0x351320).
PatrolPathBotSCB::PatrolPathBotSCB(Bot* bot, PatrolPath* patrolPath)
    : IBotSpawnCallBack(bot)
    , m_patrolPath(patrolPath)
{
}

// ELF 0x351370 (D2; D1 0x3513B0, D0 0x3513F0).
PatrolPathBotSCB::~PatrolPathBotSCB()
{
}

// ELF 0x351430. Neither pointer is tested.
void PatrolPathBotSCB::reportSpawn()
{
    m_patrolPath->botIsAlive(m_bot);
}

} } }
