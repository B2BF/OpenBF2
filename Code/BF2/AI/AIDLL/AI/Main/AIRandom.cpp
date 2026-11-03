
// static init: ELF 0x3B36E0 (__tcf_0 0x3B3740, _GLOBAL__I 0x3B3770): <iostream>'s Init object only.
#include <iostream>

#include "AIRandom.h"

#include "CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"   // IBFTimer::getWallClockTime

namespace dice {
namespace hfe {
namespace ai {

AIRandom* AIRandom::m_instance = 0;

namespace {

// MT19937 matrix constants, stored under the original anonymous-namespace name A.
const unsigned int A[2] = { 0u, 0x9908B0DFu };

} // anonymous namespace

// Clears the index before the seed and leaves the 624-word state uninitialized.
// Call init before generating random values.
AIRandom::AIRandom()
{
    m_index = 0;
    m_seed  = 0;

    // first wins - the same guard as ai::IAI and BFConsole, and the opposite of IAIMain's
    // unconditional store one layer down.
    if (!m_instance)
        m_instance = this;
}

// The exact mirror, and nothing else: no state to free.
AIRandom::~AIRandom()
{
    if (m_instance == this)
        m_instance = 0;
}

// Seeds the generator from the clock only when no explicit seed has been supplied.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
void AIRandom::init()
{
    if (!m_seed)
    {
        // Use the AI timer's wall clock for automatic seeding.
        const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
        m_seed = static_cast<int>(timer->getWallClockTime() * 1000000.0);
    }

    int x = m_seed & 0x7FFFFFFF;
    for (int i = 0; i < kStateSize; ++i)
    {
        unsigned int p = static_cast<unsigned int>(24112 * x);
        unsigned int v = ((static_cast<unsigned int>(24112 * (x & 0xFFFF)) >> 16)
                          + static_cast<unsigned int>(24112 * (x >> 16))) >> 15;
        v += ((p >> 16) & 0x7FFF) << 16;
        v += p & 0xFFFF;
        int y = static_cast<int>(v - 0x7FFFFFFF);
        if (y < 0)
            y = static_cast<int>(v);

        p = static_cast<unsigned int>(26143 * y);
        v = ((static_cast<unsigned int>(26143 * (y & 0xFFFF)) >> 16)
             + static_cast<unsigned int>(26143 * (y >> 16))) >> 15;
        v += ((p >> 16) & 0x7FFF) << 16;
        v += p & 0xFFFF;
        x = static_cast<int>(v - 0x7FFFFFFF);
        if (x < 0)
            x = static_cast<int>(v);

        m_state[i] = static_cast<unsigned int>(x);
    }

    m_index = kStateSize;
}

// Advances the MT19937 state using the standard twist operation.
unsigned int AIRandom::generateNumbers() const
{
    unsigned int* const state = const_cast<unsigned int*>(m_state);
    const_cast<AIRandom*>(this)->m_index = 0;

    unsigned int y;
    int i;
    for (i = 0; i <= 226; ++i)
    {
        y = (state[i] & 0x80000000u) | (state[i + 1] & 0x7FFFFFFFu);
        state[i] = state[i + kOffset] ^ (y >> 1) ^ A[state[i + 1] & 1u];
    }
    for (i = 227; i <= 622; ++i)
    {
        y = (state[i] & 0x80000000u) | (state[i + 1] & 0x7FFFFFFFu);
        state[i] = state[i + kOffset - kStateSize] ^ (y >> 1) ^ A[state[i + 1] & 1u];
    }
    y = (state[kStateSize - 1] & 0x80000000u) | (state[0] & 0x7FFFFFFFu);
    state[kStateSize - 1] = state[kOffset - 1] ^ (y >> 1) ^ A[state[0] & 1u];
    return state[kStateSize - 1];
}

} // namespace ai
} // namespace hfe
} // namespace dice
