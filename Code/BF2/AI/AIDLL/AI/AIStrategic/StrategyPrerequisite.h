// A weighted list of strategy conditions; Strategy uses one as its prerequisite and one as its
// abort condition. Evidence: libaidll.so (ia-32) 0x334630..0x334AA0. Conditions are borrowed
// (the destructor frees only the list nodes); the StrategyManager owns them.

#ifndef AI_AIDLL_AI_AISTRATEGIC_STRATEGYPREREQUISITE_H
#define AI_AIDLL_AI_AISTRATEGIC_STRATEGYPREREQUISITE_H

#include <list>
#include <string>
#include <utility>

namespace dice {
namespace hfe {
namespace ai {

class StrategyCondition;

class StrategyPrerequisite
{
public:
    explicit StrategyPrerequisite(const std::string& name);
    ~StrategyPrerequisite();

    void addCondition(StrategyCondition* condition, float weight);
    StrategyCondition* popFirstCondition();
    // isActive: the strategy being scored is the current one (Strategy passes previous == this);
    // only then do abort conditions count.
    float evaluate(float* friendly, float* enemy, bool isActive, int side);

    /* +0x00 */ std::list<std::pair<StrategyCondition*, float> > m_conditions;
    /* +0x04 */ std::string m_name;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_STRATEGYPREREQUISITE_H
