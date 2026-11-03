// Heterogeneous condition. Evidence: libaidll.so (ia-32) 0x332B20..0x332DD0.

#include "SCHeterogeneous.h"

#include "../Main/StrategyManager.h"

namespace dice {
namespace hfe {
namespace ai {

SCHeterogeneous::SCHeterogeneous(const std::string& name, CompareType compareType,
                                 Compare compare, ConditionSide side, Conditional first,
                                 Conditional second, float target)
    : StrategyCondition(name, target, compare, side, compareType, first),
      m_secondConditional(second)
{
}

SCHeterogeneous::~SCHeterogeneous()
{
}

// ELF 0x332D30. An unknown side answers 0 without comparing.
float SCHeterogeneous::compare(float* friendly, float* enemy, int) const
{
    float* a;
    float* b;
    switch (m_side)
    {
    case Friendly:
        a = friendly;
        b = friendly;
        break;
    case Enemy:
        a = enemy;
        b = enemy;
        break;
    case Both:
        a = friendly;
        b = enemy;
        break;
    default:
        return 0.0f;
    }
    const float result = internalCompare(a[m_conditional], b[m_secondConditional]);
    StrategyManager::getInstance()->isDebugEnabled();   // empty debug branch in the image
    return result;
}

} // namespace ai
} // namespace hfe
} // namespace dice
