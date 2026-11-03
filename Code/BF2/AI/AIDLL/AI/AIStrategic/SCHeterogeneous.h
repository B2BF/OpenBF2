// Condition comparing two different state values (of one side, or friendly against enemy).
// Evidence: libaidll.so (ia-32) 0x332B20..0x332DD0, vtable 0x4BF4C8.

#ifndef AI_AIDLL_AI_AISTRATEGIC_SCHETEROGENEOUS_H
#define AI_AIDLL_AI_AISTRATEGIC_SCHETEROGENEOUS_H

#include "StrategyCondition.h"

namespace dice {
namespace hfe {
namespace ai {

class SCHeterogeneous : public StrategyCondition
{
public:
    // ELF 0x332B20: the first conditional goes to the base, the second to +0x24.
    SCHeterogeneous(const std::string& name, CompareType compareType, Compare compare,
                    ConditionSide side, Conditional first, Conditional second, float target);
    virtual ~SCHeterogeneous();
    virtual int getType() const { return Heterogeneous; }              // ELF 0x332DD0 (weak)
    virtual float compare(float* friendly, float* enemy, int side) const;

    /* +0x24 */ Conditional m_secondConditional;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_SCHETEROGENEOUS_H
