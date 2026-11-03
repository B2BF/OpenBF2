// RandomSquareMerger. Recovered from libaidll.so 1.5.3153 (ia-32), 0x1A2480..0x1A279F.

#include "RandomSquareMerger.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1A27A0, __tcf_0 0x1A2800, _GLOBAL__I 0x1A2830).
#include <iostream>

#include "../Main/AIRandom.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1A24D0 (C1; C2 0x1A2480): the base, then the vptr.
RandomSquareMerger::RandomSquareMerger(int count, float* weights, UrgencyGenerator** generators)
    : UrgencyMerger(count, weights, generators)
{
}

// ELF 0x1A2560 (D1; D2 0x1A2520, D0 0x1A25A0): the base.
RandomSquareMerger::~RandomSquareMerger()
{
}

// ELF 0x1A25E0. Every generator updates; the squares of the updated urgencies are summed. A draw
// in [0, sum) then walks the generators, subtracting each one's active urgency (not squared);
// a generator whose share (active / sum) is above 0.01 becomes the chosen one (its own chosen
// generator, +0x20). The walk stops at the first generator after which the draw is no longer
// positive and a choice exists. Answers that generator's active urgency. Shipped defects: when
// the walk runs through, the index is the count (one past the array), and a null generator at
// the stop index is called; with no generators index 0 is used.
float RandomSquareMerger::updateActiveUrgency(Bot* bot)
{
    float total = 0.0f;
    for (int i = 0; i < m_count; ++i)
    {
        if (m_generators[i])
        {
            const float urgency = m_generators[i]->updateActiveUrgency(bot);     // +0x10
            total = urgency * urgency + total;
        }
    }
    float pick = static_cast<float>(AIRandom::m_instance->next()) * 2.3283064e-10f * total;
    m_chosen = 0;
    int index = 0;
    for (; index < m_count; ++index)
    {
        if (m_generators[index])
        {
            const float active = m_generators[index]->getActiveUrgency(bot);    // +0x0C
            pick = pick - active;
            if (active / total > 0.01f)
                m_chosen = m_generators[index]->getChosenGenerator();         // +0x20
        }
        if (pick <= 0.0f && m_chosen != 0)
            break;
    }
    return m_generators[index]->getActiveUrgency(bot);
}

} // namespace ai
} // namespace hfe
} // namespace dice
