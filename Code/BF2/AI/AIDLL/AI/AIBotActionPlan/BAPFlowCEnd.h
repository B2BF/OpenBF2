// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCEND_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCEND_H

#include "BAPFlowControl.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// Plan end (execute 0x1E3460), by EndType (ok, finished):
//   0: condition true -> (false, true), false -> (true, false); no condition -> (false, true);
//   1: (true, condition); no condition -> (false, true);
//   2: (true, !condition); no condition -> (false, false);
//   other values: neither flag is written. Always returns 1.
// The condition is owned. sizeof 0x1C.
// ELF: vtable 0x4B8EC0 (8 words), operator new 0x1C, package A-core.
class BAPFlowCEnd : public BAPFlowControl
{
public:
    enum EndType : int;   // [UNRESOLVED] enumerators: values 0, 1, 2 are handled (see above)
    BAPFlowCEnd(int patternId, BAPFlowCEnd::EndType type, BAPCondition* condition);   // 0x1E32A0
    virtual ~BAPFlowCEnd();   // 0x1E3380

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E3460, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E3520, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E3540, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E3530, slot +0x14; from the base declaration

private:
    /* +0x14 */ BAPCondition* m_condition;   // may be 0
    /* +0x18 */ EndType m_type;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCEND_H
