// Tests the borrowed trigger action's started flag.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONISTRIGGERED_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONISTRIGGERED_H

#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPATrigger;
class Bot;

// ELF: vtable 0x4B9268 (5 words), package H-deferred.
class BAPConIsTriggered : public BAPCondition
{
public:
    explicit BAPConIsTriggered(BAPATrigger*);   // 0x1E76B0
    virtual ~BAPConIsTriggered();   // 0x1E7710

    virtual bool evaluate(Bot*);   // 0x1E76E0, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E7770, slot +0x04; from the base declaration

private:
    /* +0x08 */ BAPATrigger* m_trigger;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONISTRIGGERED_H
