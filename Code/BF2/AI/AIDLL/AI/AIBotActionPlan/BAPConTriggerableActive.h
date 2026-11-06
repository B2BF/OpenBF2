// Reads the borrowed triggerable's active-state byte without a null guard.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTRIGGERABLEACTIVE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTRIGGERABLEACTIVE_H

#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace world { class Triggerable; }
namespace ai {

class Bot;

// ELF: vtable 0x4B94E8 (5 words), operator new 0xC/0x2C, package F-infantry.
class BAPConTriggerableActive : public BAPCondition
{
public:
    explicit BAPConTriggerableActive(world::Triggerable*);   // 0x1EAF20
    virtual ~BAPConTriggerableActive();   // 0x1EAF90

    virtual bool evaluate(Bot*);   // 0x1EAF50, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EAFF0, slot +0x04; from the base declaration
private:
    /* +0x08 */ world::Triggerable* m_triggerable;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONTRIGGERABLEACTIVE_H
