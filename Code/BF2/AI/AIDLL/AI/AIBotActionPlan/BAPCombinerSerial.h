// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCOMBINERSERIAL_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCOMBINERSERIAL_H

#include <iosfwd>
#include <list>
#include "BAPCombiner.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;

// Runs its children one after the other. Completion rules (execute 0x1E6840):
// * empty list, or the current child is past the end: ok = finished = true, rewind, done;
// * waitForChild (default): the current child runs each tick; when it reports finished the
//   combiner moves to the next one, and reports finished (and rewinds) after the last;
// * !waitForChild: every tick runs the current child once and moves on; a child that reports
//   finished ends the whole sequence (finished stays true, rewind).
// `ok` is whatever the last child reported. sizeof 0x24.
// ELF: vtable 0x4B90E0 (9 words), operator new 0x24, package A-core.
class BAPCombinerSerial : public BAPCombiner
{
    // [INFERRED] friendship: PrettyPrinter::prettyPrintScriptNode (libaidll ELF 0x1CAE60) reads the
    // statement list, cursor and debug id inline.
    friend class PrettyPrinter;
public:
    BAPCombinerSerial(int patternId, int debugId);   // 0x1E6200
    BAPCombinerSerial(int patternId, bool waitForChild, int debugId);   // 0x1E6360
    BAPCombinerSerial(std::list<BAPStatement*>& statements, int patternId, int debugId);   // 0x1E6500
    virtual ~BAPCombinerSerial();   // 0x1E66B0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E6840, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E6970, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E6CE0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E6990, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1E6A20, slot +0x1C; from the base declaration
    virtual void addStatement(BAPStatement*);   // 0x1E69A0, slot +0x20; from the base declaration

private:
    // +0x20: second constructor int; the creators pass -1. Only streamSelf reads it ("(n)" unless
    // -1). [INFERRED] name.
    /* +0x20 */ int m_debugId;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCOMBINERSERIAL_H
