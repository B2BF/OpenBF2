// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONITERATION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONITERATION_H

#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9290 (5 words), package A-core.
// True on the n-th evaluate and on every later one (counts down, no reset). sizeof 0xC.
class BAPConIteration : public BAPCondition
{
public:
    explicit BAPConIteration(int);   // 0x1E7860
    virtual ~BAPConIteration();   // 0x1E78B0

    virtual bool evaluate(Bot*);   // 0x1E7910, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E7930, slot +0x04; from the base declaration

private:
    /* +0x08 */ int m_count;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONITERATION_H
