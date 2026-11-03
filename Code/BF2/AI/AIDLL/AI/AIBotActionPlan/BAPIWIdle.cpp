// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F76E0 (__tcf_0 0x1F7740, _GLOBAL__I 0x1F7770): <iostream>'s Init object only.
#include <iostream>

#include "BAPIWIdle.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F7480 (C1; C2 0x1F7310 region)
BAPIWIdle::BAPIWIdle(const Vec3& position, BAPStatement* statement, int tag, Bot* bot)
    : BAPInfoWrapper(statement, tag)
{
    m_position = position;
    m_bot = bot;
    if (bot && bot->getSquadMember())
        m_bot->getSquadMember()->setIsIdle(true);
    m_statement->m_owner = this;
}

// ELF 0x1F75D0 (D1; D2 0x1F7510 region, D0 0x1F7650): clears the idle flag, deletes the child
// (virtual D0, slot +8).
BAPIWIdle::~BAPIWIdle()
{
    if (m_bot && m_bot->getSquadMember())
        m_bot->getSquadMember()->setIsIdle(false);
    if (m_statement)
        delete m_statement;
}

// ELF 0x1F7790: "Idle"
int BAPIWIdle::getInfoType() const
{
    return 4;
}

} // namespace ai
} // namespace hfe
} // namespace dice
