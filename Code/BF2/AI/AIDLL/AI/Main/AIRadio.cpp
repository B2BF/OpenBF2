// [INFERRED] Source placement; see AIRadio.h.

// static init: ELF 0x3B3390 (__tcf_0 0x3B33F0, _GLOBAL__I 0x3B3420): <iostream>'s Init object only.
#include <iostream>

#include "AIRadio.h"

#include "CommonSingletonServer.h"      // m_iaiSettings
#include "AISettingsInterfaces.h"       // IAISettings::getMaxNBots (slot 0)
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// .bss: zero until a constructor runs, and none does in the shipped image.
AIRadio* AIRadio::m_instance = 0;

// ELF 0x3B3190 (C1; C2 0x3B3140 is the same): stores 0.0 (the float constant at ELF 0x49CB34) into
// the update time, asks the settings for the bot count once and discards it, then publishes
// itself. +0x04 is left unwritten.
AIRadio::AIRadio()
{
    m_lastUpdateTime = 0.0f;
    const int bots = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    (void)bots;
    m_instance = this;
}

// ELF 0x3B3240 (D1; D2 0x3B31E0 is the same): asks the settings for the bot count once and clears
// each bot's messages (an empty body, so the shipped loop only counts down), frees the +0x04
// array, then zeroes m_instance unconditionally.
AIRadio::~AIRadio()
{
    const int bots = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    for (int bot = 0; bot < bots; ++bot)
        clearMessagesForBot(bot);
    delete[] m_unresolved4;
    m_instance = 0;
}

// ELF 0x3B3330: push ebp / mov ebp, esp / pop ebp / ret.
void AIRadio::clearMessagesForBot(int)
{
}

// ELF 0x3B32A0: the time is stored unless it is less than 0.5 s past the last one.
void AIRadio::update()
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!(0.5f > now - m_lastUpdateTime))
        m_lastUpdateTime = now;
}

// ELF 0x3B32F0.
bool AIRadio::reset()
{
    const int bots = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    (void)bots;
    return true;
}

// ELF 0x3B3340.
void AIRadio::setLastUpdateTime()
{
    m_lastUpdateTime = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
}

// ELF 0x3B3380: the float widened into the double.
void AIRadio::setLastUpdateTime(float time)
{
    m_lastUpdateTime = time;
}

} // namespace ai
} // namespace hfe
} // namespace dice
