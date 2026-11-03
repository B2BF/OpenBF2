// Constant condition. Evidence: libaidll.so (ia-32) 0x332880..0x332B10.

#include "SCConstant.h"

#include "../Main/StrategyManager.h"

namespace dice {
namespace hfe {
namespace ai {

SCConstant::SCConstant(const std::string& name, CompareType compareType, Compare compare,
                       ConditionSide side, Conditional conditional, float constant, float target)
    : StrategyCondition(name, target, compare, side, compareType, conditional),
      m_constant(constant)
{
}

SCConstant::~SCConstant()
{
}

// ELF 0x332A90: Friendly reads the friendly array, every other side the enemy array. The
// debug query's branch is empty in the shipped image; only the call remains.
float SCConstant::compare(float* friendly, float* enemy, int) const
{
    float* const values = m_side == Friendly ? friendly : enemy;
    const float result = internalCompare(values[m_conditional], m_constant);
    StrategyManager::getInstance()->isDebugEnabled();
    return result;
}

} // namespace ai
} // namespace hfe
} // namespace dice
