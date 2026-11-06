// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCWHILE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCWHILE_H

#include <iosfwd>
#include "BAPFlowControl.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class BAPStatement;
class Bot;

// While (execute 0x1E4580). Without a loop statement it reports ok and finished = !condition.
// Otherwise: breakLoop() ends it on the next tick (ok, finished); the condition is tested only
// when m_testCondition is set (always, or with testOnFinish only after the body reported
// finished) and a false condition ends it (ok, finished); else the body runs and the while itself
// reports not finished. Condition and body are owned. sizeof 0x20.
// ELF: vtable 0x4B9000 (8 words), operator new 0x8/0x20, package A-core.
class BAPFlowCWhile : public BAPFlowControl
{
public:
    BAPFlowCWhile(BAPCondition* condition, BAPStatement* loopStatement, int patternId);   // 0x1E4280
    BAPFlowCWhile(BAPCondition* condition, BAPStatement* loopStatement, bool testOnFinish, int patternId);   // 0x1E4390
    virtual ~BAPFlowCWhile();   // 0x1E4490

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E4580, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E4690, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E4900, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E46C0, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1E4700, slot +0x1C; from the base declaration

    void breakLoop();   // 0x1E46D0: +0x1C = 1
    BAPCondition* getCondition();   // 0x1E46E0: +0x14
    BAPStatement* getLoopStatement();   // 0x1E46F0: +0x18

private:
    /* +0x14 */ BAPCondition* m_condition;
    /* +0x18 */ BAPStatement* m_loopStatement;
    /* +0x1C */ bool m_break;            // = false
    /* +0x1D */ bool m_testCondition;    // = true [INFERRED] name
    /* +0x1E */ bool m_testOnFinish;     // = true, or the bool argument [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCWHILE_H
