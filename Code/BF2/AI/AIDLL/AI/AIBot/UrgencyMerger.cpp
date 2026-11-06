// Source placement: AIDLL_r.dll __FILE__ "Code\BF2\AI\AIDLL\AI\AIBot\UrgencyMerger.cpp" (assert "mpMaxGenerator != 0", 0x101F9180).
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining UrgencyMerger operations.

// static init: ELF 0x1B5CD0 (__tcf_0 0x1B5D30, _GLOBAL__I 0x1B5D60): <iostream>'s Init object only.
#include <iostream>

#include "UrgencyMerger.h"


namespace dice {
namespace hfe {
namespace ai {

// one instruction in the binary - the vptr store C++ emits on its own. The class owns nothing
// which is what makes it a pure interface rather than a partial implementation.
UrgencyGenerator::~UrgencyGenerator()
{
}

// Four stores after the vptr. `m_weights` is whatever the caller passed and BotManager passes
// null, so a merger from the bot pool has no weight array at all - whatever `getUrgency` does with
// that is the next thing to read, and it is not guessed at here.
UrgencyMerger::UrgencyMerger(int count, float* weights, UrgencyGenerator** generators)
    : m_count(count)
    , m_weights(weights)
    , m_generators(generators)
    , mpMaxGenerator(0)
{
}

// ELF 0x1B5930: only the vptr store; the generators belong to the caller.
UrgencyMerger::~UrgencyMerger()
{
}

// ELF 0x1B59B0. The chosen generator's urgency, 0 when none is chosen.
float UrgencyMerger::getUrgency(Bot* bot)
{
    if (mpMaxGenerator == 0)
        return 0.0f;
    return mpMaxGenerator->getUrgency(bot);
}

// ELF 0x1B5A00.
float UrgencyMerger::getActiveUrgency(Bot* bot)
{
    if (mpMaxGenerator == 0)
        return 0.0f;
    return mpMaxGenerator->getActiveUrgency(bot);
}

// ELF 0x1B5A40. Updates every generator; the strictly most urgent one's own chosen generator
// becomes this merger's. Returns that urgency (0 when none is positive). The weights are not read.
float UrgencyMerger::updateActiveUrgency(Bot* bot)
{
    float best = 0.0f;
    mpMaxGenerator = 0;
    for (int i = 0; i < m_count; ++i)
    {
        const float urgency =
            m_generators[i] != 0 ? m_generators[i]->updateActiveUrgency(bot) : 0.0f;
        if (urgency > best)
        {
            best = urgency;
            mpMaxGenerator = m_generators[i]->getChosenGenerator();
        }
    }
    return best;
}

// ELF 0x1B5AF0. True while every present generator reports a changed target (stops at the first
// that does not); true with no generators.
bool UrgencyMerger::hasChangedTarget(Bot* bot)
{
    bool changed = true;
    for (int i = 0; i < m_count && changed; ++i)
    {
        if (m_generators[i] != 0)
            changed = m_generators[i]->hasChangedTarget(bot);
    }
    return changed;
}

// ELF 0x1B5B60. The same conjunction over quotientChanged.
bool UrgencyMerger::quotientChanged(Bot* bot, float lower, float upper)
{
    bool changed = true;
    for (int i = 0; i < m_count && changed; ++i)
    {
        if (m_generators[i] != 0)
            changed = m_generators[i]->quotientChanged(bot, lower, upper);
    }
    return changed;
}

// ELF 0x1B5BE0. True as soon as one present generator's quotient or target changed.
bool UrgencyMerger::hasChanged(Bot* bot, float lower, float upper)
{
    bool changed = false;
    for (int i = 0; i < m_count && !changed; ++i)
    {
        if (m_generators[i] != 0)
            changed = m_generators[i]->quotientChanged(bot, lower, upper) ||
                      m_generators[i]->hasChangedTarget(bot);
    }
    return changed;
}

// ELF 0x1A2850.
UrgencyGenerator* UrgencyMerger::getChosenGenerator()
{
    return mpMaxGenerator;
}

// ELF 0x1B5C80. Resets every present generator.
void UrgencyMerger::reset(Bot* bot)
{
    for (int i = 0; i < m_count; ++i)
    {
        if (m_generators[i] != 0)
            m_generators[i]->reset(bot);
    }
}

// ELF 0x1B59F0.
int UrgencyMerger::getSize()
{
    return m_count;
}

} // namespace ai
} // namespace hfe
} // namespace dice
