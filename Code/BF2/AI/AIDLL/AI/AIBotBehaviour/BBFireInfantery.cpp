// Fire behaviour of infantry (AI/AIDLL/AI/AIBotBehaviour/BBFireInfantery.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x20F310 (__tcf_0 0x20F370, _GLOBAL__I 0x20F3A0): <iostream>'s Init object only.
#include <iostream>

#include "BBFireInfantery.h"

#include "../AIBotBehaviourPattern/BBPFireInfantery.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x20F190
BBFireInfantery::BBFireInfantery(unsigned char index, unsigned char queue, BBPFireInfantery* pattern,
                                 UrgeCurve* curve, float* modifiers)
    : BBFire(index, queue, pattern, curve, modifiers)
    , m_infantryPattern(pattern)
{
}

// ELF 0x20F240
BBFireInfantery::~BBFireInfantery()
{
}

// ELF 0x20F2C0: forwards to BBFire's.
void BBFireInfantery::initializeGenerator(BotMain* bot, int i, bool a, bool b)
{
    BBFire::initializeGenerator(bot, i, a, b);
}

} // namespace ai
} // namespace hfe
} // namespace dice
