// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCWAIT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCWAIT_H

#include "BAPFlowControl.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// Wait (execute 0x1E4190): ok = true, finished = condition. The condition is owned. sizeof 0x18.
// ELF: vtable 0x4B8F80 (8 words), package A-core.
class BAPFlowCWait : public BAPFlowControl
{
public:
    BAPFlowCWait(BAPCondition* condition, int patternId);   // 0x1E3FF0
    virtual ~BAPFlowCWait();   // 0x1E40C0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E4190, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E41D0, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E41F0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E41E0, slot +0x14; from the base declaration

private:
    /* +0x14 */ BAPCondition* m_condition;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCWAIT_H
