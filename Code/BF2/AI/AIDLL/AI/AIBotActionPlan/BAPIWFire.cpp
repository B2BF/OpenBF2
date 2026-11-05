// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F72F0 (__tcf_0 0x1F7350, _GLOBAL__I 0x1F7380): <iostream>'s Init object only.
#include <iostream>

#include "BAPIWFire.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../Main/IBotManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F6E20 (C1): Bot::setIsFiring(true) (+0x12C), setFiringTarget(target) (+0x7C), and
// BotMain::getUseStaticAttack (+0x288) is remembered.
BAPIWFire::BAPIWFire(Bot* bot, unsigned int target, bool flag24, bool flag25, int int1C, int int20,
                     BAPStatement* statement, int patternId)
    : BAPInfoWrapper(statement, patternId)
{
    m_target = target;
    m_int1C = int1C;
    m_int20 = int20;
    m_flag24 = flag24;
    m_int28 = -1;
    m_flag25 = flag25;
    m_bot = bot;
    m_statement->mpOwner = this;
    bot->setIsFiring(true);
    bot->setFiringTarget(target);
    m_useStaticAttack = static_cast<BotMain*>(bot)->getUseStaticAttack();
}

// ELF 0x1F7040 (D1; D0 0x1F7190): deletes the child, then resets the bot (+0x12C, +0x1D0,
// +0x230, +0x1C0, +0x278, +0x7C) and clears the target player's bot "engineer target" flag (+0x1E8).
BAPIWFire::~BAPIWFire()
{
    if (m_statement)
        delete m_statement;
    BotMain* bot = static_cast<BotMain*>(m_bot);
    bot->setIsFiring(false);
    bot->setBridgeRepairTarget(0xFFFFFFFFu);
    bot->setBotIsSimulatingArtillery(false);
    bot->setRepairTarget(0xFFFFFFFFu);
    bot->setIsGoingForFlag(false);
    bot->setFiringTarget(0xFFFFFFFFu);
    if (m_target != 0xFFFFFFFFu) {
        Information* info = bapinline::info(m_target);
        if (info) {
            Bot* other = IBotManager::instance->getBotFromPlayerId(info->getPlayerId());
            if (other)
                static_cast<BotMain*>(other)->setIsEngineerTarget(false);
        }
    }
}

// ELF 0x1F73A0: "Fire"
int BAPIWFire::getInfoType() const
{
    return 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
