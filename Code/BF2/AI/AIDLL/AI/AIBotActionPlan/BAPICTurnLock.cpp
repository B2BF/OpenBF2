// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F8850 (__tcf_0 0x1F88B0, _GLOBAL__I 0x1F88E0): <iostream>'s Init object only.
#include <iostream>

#include "BAPICTurnLock.h"

#include "../AIBot/BotMain.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F86D0 (C1): base only, +0x10 untouched in the image (the shared base ctor zeroes it).
BAPICTurnLock::BAPICTurnLock(bool lock)
{
    m_lock = lock;
}

// ELF 0x1F8760 (D0 0x1F87A0)
BAPICTurnLock::~BAPICTurnLock()
{
}

// ELF 0x1F87F0
bool BAPICTurnLock::execute(Bot* bot, bool& ok, bool& failed)
{
    bot->setPlanLock(m_lock);
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1F8830
BAPStatementType BAPICTurnLock::getId() const
{
    return BAPSTICTurnLock;   // 31
}

// ELF 0x1F8840
bool BAPICTurnLock::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
