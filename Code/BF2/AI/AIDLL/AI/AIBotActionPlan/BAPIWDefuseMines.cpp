// Owns a mine-defusing plan and brackets its lifetime with the bot's firing/mine-target state.

// static init: ELF 0x1F6C90 (__tcf_0 0x1F6CF0, _GLOBAL__I 0x1F6D20): <iostream>'s Init object only.
#include <iostream>

#include "BAPIWDefuseMines.h"

#include "../AIBot/BotMain.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F6A70 (C1; C2 0x1F69E0)
BAPIWDefuseMines::BAPIWDefuseMines(Bot* bot, unsigned int mine, BAPStatement* statement, int tag)
    : BAPInfoWrapper(statement, tag), m_mine(mine), m_bot(bot)
{
    m_statement->setParent(this);
    bot->setIsFiring(true);
    static_cast<BotMain*>(m_bot)->setMineTarget(mine);
}

// ELF 0x1F6B80 (D1; D2 0x1F6B00, D0 0x1F6C00)
BAPIWDefuseMines::~BAPIWDefuseMines()
{
    delete m_statement;
    m_bot->setIsFiring(false);
    static_cast<BotMain*>(m_bot)->setMineTarget(0xFFFFFFFFu);
}

// ELF 0x1F6D40
int BAPIWDefuseMines::getInfoType() const
{
    return 1;
}

} // namespace ai
} // namespace hfe
} // namespace dice
