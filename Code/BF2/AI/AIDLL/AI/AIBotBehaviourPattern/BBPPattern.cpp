// BBPPattern: recovered by package A-core from libaidll.so (TU BBPPattern.cpp, 0x296ED0..0x297380; the
// three weak members at 0x2298A0). Every body carries its ELF address.

// static init: ELF 0x2972F0 (__tcf_0 0x297350, _GLOBAL__I 0x297380): <iostream>'s Init object only.
#include <iostream>

#include "BBPPattern.h"

#include "../AIBot/BotMain.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x296FD0 (C2; C1 0x297160). The flag array has one entry per possible bot.
BBPPattern::BBPPattern()
{
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_initiated = checkPointer(new bool[count]);
    for (int i = 0; i < count; ++i)
        m_initiated[i] = false;
}

// ELF 0x296ED0 (D2; D1 0x296F20, D0 0x296F70).
BBPPattern::~BBPPattern()
{
    delete[] m_initiated;
}

// ELF 0x2298D0 (weak).
bool BBPPattern::isInitiated(Bot* bot)
{
    return m_initiated[bot->getBotId()];
}

// ELF 0x2298A0 (weak).
void BBPPattern::unInit(Bot* bot)
{
    m_initiated[bot->getBotId()] = false;
}

// ELF 0x229900 (weak).
void BBPPattern::forceInitiated(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
