// Strategy condition base: the crisp and fuzzy comparisons shared by all condition kinds.
// Evidence: libaidll.so (ia-32) 0x334150..0x334570.

#include "StrategyCondition.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF 0x334580,
// __tcf_0 0x3345E0, _GLOBAL__I 0x334610).
#include <iostream>

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

StrategyCondition::StrategyCondition(const std::string& name, float target, Compare compare,
                                     ConditionSide side, CompareType compareType,
                                     Conditional conditional)
    : m_isAbortCondition(false),
      m_targetValue(target),
      m_compare(compare),
      m_compareType(compareType),
      m_side(side),
      m_conditional(conditional),
      m_strength(Required),
      m_name(name)
{
}

float StrategyCondition::setTargetValue(float value)
{
    return m_targetValue = value;
}

float StrategyCondition::getTargetValue() const
{
    return m_targetValue;
}

void StrategyCondition::setConditionStrength(ConditionStrength strength)
{
    m_strength = strength;
}

StrategyCondition::ConditionStrength StrategyCondition::getConditionStrength() const
{
    return m_strength;
}

// ELF 0x334270. FuzzySqr keeps the sign of the fuzzy distance and squares its magnitude.
float StrategyCondition::internalCompare(float a, float b) const
{
    switch (m_compareType)
    {
    case Crisp:
        return internalCompareCrisp(a, b);
    case Fuzzy:
        return internalCompareFuzzy(a, b);
    case FuzzySqr:
    {
        const float r = internalCompareFuzzy(a, b);
        const float sign = r > 0.0f ? 1.0f : (r < 0.0f ? -1.0f : 0.0f);
        return sign * (r * r);
    }
    default:
        return 0.0f;
    }
}

// ELF 0x334320: 1 when the relation to the target holds, -1 otherwise. A zero divisor answers
// -1, except QuotientGreater, which answers 1.
float StrategyCondition::internalCompareCrisp(float a, float b) const
{
    float x;
    switch (m_compare)
    {
    case Equal:
    case Difference:
        x = a - b;
        return x == m_targetValue ? 1.0f : -1.0f;
    case EqualGreater:
    case DifferenceGreater:
        x = a - b;
        return x < m_targetValue ? -1.0f : 1.0f;
    case EqualSmaller:
    case DifferenceSmaller:
        x = a - b;
        return m_targetValue < x ? -1.0f : 1.0f;
    case Quotient:
        if (b == 0.0f)
            return -1.0f;
        x = a / b;
        return x == m_targetValue ? 1.0f : -1.0f;
    case QuotientGreater:
        if (b == 0.0f)
            return 1.0f;
        x = a / b;
        return x < m_targetValue ? -1.0f : 1.0f;
    case QuotientSmaller:
        if (b == 0.0f)
            return -1.0f;
        x = a / b;
        return m_targetValue < x ? -1.0f : 1.0f;
    default:
        return 0.0f;
    }
}

// ELF 0x334440: the signed distance to the target (negated absolute distance for the Equal
// kinds). A zero divisor answers -1000, except QuotientGreater, which answers 1000.
float StrategyCondition::internalCompareFuzzy(float a, float b) const
{
    switch (m_compare)
    {
    case Equal:
    case Difference:
        return -std::fabs(a - b - m_targetValue);
    case EqualGreater:
    case DifferenceGreater:
        return a - b - m_targetValue;
    case EqualSmaller:
    case DifferenceSmaller:
        return -(a - b - m_targetValue);
    case Quotient:
        if (b == 0.0f)
            return -1000.0f;
        return -std::fabs(a / b - m_targetValue);
    case QuotientGreater:
        if (b == 0.0f)
            return 1000.0f;
        return a / b - m_targetValue;
    case QuotientSmaller:
        if (b == 0.0f)
            return -1000.0f;
        return -(a / b - m_targetValue);
    default:
        return 0.0f;
    }
}

bool StrategyCondition::setIsAbortCondition(bool abort)
{
    return m_isAbortCondition = abort;
}

bool StrategyCondition::getIsAbortCondition() const
{
    return m_isAbortCondition;
}

bool StrategyCondition::isAbortCondition() const
{
    return m_isAbortCondition;
}

} // namespace ai
} // namespace hfe
} // namespace dice
