// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCOMBINERPARALLEL_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCOMBINERPARALLEL_H

#include <iosfwd>
#include <list>
#include "BAPCombiner.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;

// Runs all children every tick, in order. Completion rules (execute 0x1E57B0):
// * ok = true and finished = waitForChild before the loop; the children share one ok/finished
//   pair that is not reset between them (child ok starts true, child finished false);
// * after each child: ok &= child ok; finished = waitForChild ? (finished && child finished)
//   : (finished || child finished), i.e. all children finished, or any child finished;
// * neverFinish forces finished = false at the end. An empty combiner reports ok and
//   finished = waitForChild (unless neverFinish).
// Each child's finished flag is stored into the FIRST element of m_isFinished only (the image never
// advances that iterator, 0x1E58CE). Each child's getControls() is called and discarded first.
// sizeof 0x2C.
// ELF: vtable 0x4B9040 (9 words), operator new 0x2C, package A-core.
class BAPCombinerParallel : public BAPCombiner
{
    // [INFERRED] friendship: PrettyPrinter::prettyPrintScriptNode (libaidll ELF 0x1CAE60) reads the
    // statement list, cursor and debug id inline.
    friend class PrettyPrinter;
public:
    BAPCombinerParallel(int patternId, int debugId, bool neverFinish);   // 0x1E4AA0
    BAPCombinerParallel(int patternId, bool waitForChild, int debugId, bool neverFinish);   // 0x1E4C50
    BAPCombinerParallel(std::list<BAPStatement*>& statements, int patternId, int debugId, bool neverFinish);   // 0x1E4F10
    BAPCombinerParallel(std::list<BAPStatement*>& statements, int patternId, bool waitForChild, int debugId,
                        bool neverFinish);   // 0x1E52E0
    virtual ~BAPCombinerParallel();   // 0x1E55C0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E57B0, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E5940, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E6140, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E5950, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1E5A70, slot +0x1C; from the base declaration
    virtual void addStatement(BAPStatement*);   // 0x1E5960, slot +0x20; from the base declaration

    const std::list<bool>& getIsFinished() const;   // 0x1E5A60: returns this + 0x20

private:
    /* +0x20 */ std::list<bool> m_isFinished;   // one false per child (addStatement / list constructors)
    /* +0x24 */ int m_debugId;                  // second constructor int, -1 from the creators; streamSelf only [INFERRED] name
    /* +0x28 */ bool m_neverFinish;             // last constructor bool [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCOMBINERPARALLEL_H
