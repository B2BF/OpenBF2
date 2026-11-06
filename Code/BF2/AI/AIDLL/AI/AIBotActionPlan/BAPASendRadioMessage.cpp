// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1DFEB0 (__tcf_0 0x1DFF10, _GLOBAL__I 0x1DFF40): <iostream>'s Init object only.
#include <iostream>

#include "BAPASendRadioMessage.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DFCF0 (C1; C2 0x1DFC80): +4/+8/+0xC = 0, then +0x14 type, +0x18 channel, +0x1C priority,
// +0x20 the last int, +0x24 the float, +0x10 the pattern id (the image's store order).
BAPASendRadioMessage::BAPASendRadioMessage(AIRadioMessageType type, AIRadioChannelType channel,
                                           unsigned int priority, float delay, int patternId, int target)
{
    m_controls = 0;
    m_type = type;
    m_channel = channel;
    m_priority = priority;
    m_target = target;
    m_delay = delay;
    m_patternId = patternId;
}

// ELF 0x1DFDA0 (D0 0x1DFDE0)
BAPASendRadioMessage::~BAPASendRadioMessage()
{
}

// ELF 0x1DFE30: getCommunicationAgent (Bot +0xB0)->enqueueRadioMessage(+0x14, +0x18, +0x1C, +0x24, +0x20).
bool BAPASendRadioMessage::execute(Bot* bot, bool& ok, bool& finished)
{
    bot->getCommunicationAgent()->enqueueRadioMessage(m_type, m_channel, m_priority, m_delay, m_target);
    ok = true;
    finished = true;
    return true;
}

// ELF 0x1DFF60
BAPStatementType BAPASendRadioMessage::getId() const
{
    return BAPSTSendRadioMessage;   // 37
}

// ELF 0x1DFEA0
bool BAPASendRadioMessage::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
