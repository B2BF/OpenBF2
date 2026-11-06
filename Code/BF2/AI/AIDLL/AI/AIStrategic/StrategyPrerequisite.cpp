// Strategy prerequisite. Evidence: libaidll.so (ia-32) 0x334630..0x334AA0.

#include "StrategyPrerequisite.h"

#include "StrategyCondition.h"
#include "../Main/StrategyManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x334630: the list is cleared again after the name is copied.
StrategyPrerequisite::StrategyPrerequisite(const std::string& name)
    : m_name(name)
{
    m_conditions.clear();
}

StrategyPrerequisite::~StrategyPrerequisite()
{
}

// ELF 0x334880: null conditions are ignored.
void StrategyPrerequisite::addCondition(StrategyCondition* condition, float weight)
{
    if (condition)
        m_conditions.push_back(std::make_pair(condition, weight));
}

// ELF 0x3348E0: the image counts the list (size(), not empty()).
StrategyCondition* StrategyPrerequisite::popFirstCondition()
{
    if (m_conditions.size() == 0)
        return 0;
    StrategyCondition* const condition = m_conditions.begin()->first;
    m_conditions.erase(m_conditions.begin());
    return condition;
}

// ELF 0x334960: the sum of weighted condition scores. A Required(Positive) condition scoring
// below zero, or a RequiredNegative one above zero, makes the whole prerequisite 0 at once;
// the Advisory kinds clamp their contribution instead.
float StrategyPrerequisite::evaluate(float* friendly, float* enemy, bool isActive, int side)
{
    float sum = 0.0f;
    for (std::list<std::pair<StrategyCondition*, float> >::iterator it = m_conditions.begin();
         it != m_conditions.end(); ++it)
    {
        if (!isActive && it->first->isAbortCondition())
            continue;
        float value = it->first->compare(friendly, enemy, side) * it->second;
        switch (it->first->getConditionStrength())
        {
        case StrategyCondition::Required:
        case StrategyCondition::RequiredPositive:
            if (0.0f > value)
                return 0.0f;
            break;
        case StrategyCondition::RequiredNegative:
            if (value > 0.0f)
                return 0.0f;
            break;
        case StrategyCondition::Advisory:
            break;
        case StrategyCondition::AdvisoryPositive:
            if (0.0f > value)
                value = 0.0f;
            break;
        case StrategyCondition::AdvisoryNegative:
            if (value > 0.0f)
                value = 0.0f;
            break;
        default:
            break;
        }
        sum += value;
    }
    StrategyManager::getInstance()->isDebugEnabled();   // empty debug branch in the image
    return sum;
}

} // namespace ai
} // namespace hfe
} // namespace dice
