// Homogeneous condition. Evidence: libaidll.so (ia-32) 0x332DE0..0x333090.

#include "SCHomogeneous.h"

#include "../Main/StrategyManager.h"

namespace dice {
namespace hfe {
namespace ai {

SCHomogeneous::SCHomogeneous(const std::string& name, CompareType compareType, Compare compare,
                             Conditional conditional, float target)
    : StrategyCondition(name, target, compare, Both, compareType, conditional)
{
}

SCHomogeneous::~SCHomogeneous()
{
}

// ELF 0x332FF0. An unknown side answers 0 without comparing.
float SCHomogeneous::compare(float* friendly, float* enemy, int) const
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
    const float result = internalCompare(a[m_conditional], b[m_conditional]);
    StrategyManager::getInstance()->isDebugEnabled();   // empty debug branch in the image
    return result;
}

} // namespace ai
} // namespace hfe
} // namespace dice
