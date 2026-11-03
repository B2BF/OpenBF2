// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCIF_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCIF_H

#include <iosfwd>
#include <string>
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "BAPFlowControl.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class BAPStatement;
class Bot;

// If (execute 0x1E3850). Without reevaluate the condition is tested once, the chosen branch then
// runs until it reports finished, after which the next tick tests again. With reevaluate the
// condition is tested every tick (the branch may switch), except that with keepElse a running
// else-branch is kept until it finishes. A missing branch: ok = finished = true. The branch's
// finished drops it (the next tick tests again). Both branches and the condition are owned.
// sizeof 0x2C.
// ELF: vtable 0x4B8F40 (8 words), operator new 0x2C, package A-core.
class BAPFlowCIf : public BAPFlowControl
{
public:
    BAPFlowCIf(BAPCondition* condition, BAPStatement* trueStatement, BAPStatement* falseStatement, int patternId,
               bool reevaluate, bool keepElse);   // 0x1E35F0
    virtual ~BAPFlowCIf();   // 0x1E3720

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E3850, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E3950, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E3F70, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E39B0, slot +0x14; from the base declaration
    virtual void debug(BotScriptDebugger::DebugLevel, std::string&);   // 0x1E3A10, slot +0x18; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1E3B60, slot +0x1C; from the base declaration

    BAPCondition* getCondition() const;   // 0x1E39C0: +0x14
    BAPStatement* getTrueStatement() const;   // 0x1E39D0: +0x18
    BAPStatement* getFalseStatement() const;   // 0x1E39E0: +0x1C
    BAPStatement* getActiveStatement() const;   // 0x1E39F0: +0x20
    BAPStatement* getActiveStatementDebug() const;   // 0x1E3A00: +0x24

private:
    /* +0x14 */ BAPCondition* m_condition;
    /* +0x18 */ BAPStatement* m_trueStatement;
    /* +0x1C */ BAPStatement* m_falseStatement;
    /* +0x20 */ BAPStatement* m_activeStatement;        // 0 = test the condition next tick
    /* +0x24 */ BAPStatement* m_activeStatementDebug;   // last chosen branch, kept for debug()/streamSelf
    /* +0x28 */ bool m_reevaluate;                      // [INFERRED] name
    /* +0x29 */ bool m_keepElse;                        // [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCIF_H
