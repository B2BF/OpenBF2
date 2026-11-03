// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONSHAREDCONDITION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONSHAREDCONDITION_H

#include <iosfwd>
#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B93E0 (5 words), operator new 0x8/0x10, package A-core.
// A condition evaluated once and read by several plan nodes: the original (built with the wrapped
// condition) evaluates it and stores the result in a shared, reference-counted cell; copies
// (getNewSharedCondition) only read the cell. sizeof 0x10.
class BAPConSharedCondition : public BAPCondition
{
public:
    explicit BAPConSharedCondition(BAPCondition*);   // 0x1E9DF0
    explicit BAPConSharedCondition(const BAPConSharedCondition&);   // 0x1E9EA0
    virtual ~BAPConSharedCondition();   // 0x1E9F50

    virtual bool evaluate(Bot*);   // 0x1EA190, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EA1D0, slot +0x04; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1EA1E0, slot +0x08; from the base declaration

    BAPConSharedCondition* getNewSharedCondition() const;   // 0x1EA040: checkPointer(new copy)

private:
    // `operator new(8)` in the constructor 0x1E9D80: {false, 1}.
    struct SharedValue
    {
        bool value;
        int references;
    };

    /* +0x08 */ SharedValue* m_shared;
    /* +0x0C */ BAPCondition* m_condition;   // owned; 0 in copies
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONSHAREDCONDITION_H
