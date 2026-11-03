// Condition comparing the same state value of two sides (friendly/friendly, enemy/enemy or
// friendly/enemy). Evidence: libaidll.so (ia-32) 0x332DE0..0x333090, vtable 0x4BF4F0.

#ifndef AI_AIDLL_AI_AISTRATEGIC_SCHOMOGENEOUS_H
#define AI_AIDLL_AI_AISTRATEGIC_SCHOMOGENEOUS_H

#include "StrategyCondition.h"

namespace dice {
namespace hfe {
namespace ai {

class SCHomogeneous : public StrategyCondition
{
public:
    // ELF 0x332DE0: the side is always Both.
    SCHomogeneous(const std::string& name, CompareType compareType, Compare compare,
                  Conditional conditional, float target);
    virtual ~SCHomogeneous();
    virtual int getType() const { return Homogeneous; }                // ELF 0x333090 (weak)
    virtual float compare(float* friendly, float* enemy, int side) const;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_SCHOMOGENEOUS_H
