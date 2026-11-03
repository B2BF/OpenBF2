// Condition comparing one state value of the friendly or the enemy side with a constant.
// Evidence: libaidll.so (ia-32) 0x332880..0x332B10, vtable 0x4BF4A0.

#ifndef AI_AIDLL_AI_AISTRATEGIC_SCCONSTANT_H
#define AI_AIDLL_AI_AISTRATEGIC_SCCONSTANT_H

#include "StrategyCondition.h"

namespace dice {
namespace hfe {
namespace ai {

class SCConstant : public StrategyCondition
{
public:
    // ELF 0x332880: the base gets (name, target, compare, side, compareType, conditional); the
    // first float is the constant operand at +0x24.
    SCConstant(const std::string& name, CompareType compareType, Compare compare,
               ConditionSide side, Conditional conditional, float constant, float target);
    virtual ~SCConstant();
    virtual int getType() const { return Constant; }                   // ELF 0x332B10 (weak)
    virtual float compare(float* friendly, float* enemy, int side) const;

    /* +0x24 */ float m_constant;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_SCCONSTANT_H
