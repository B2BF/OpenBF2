// The plan interpreter: per statement type and LOD level, either a registered BAPEntry (the
// vehicle-specific control code) or the statement's own execute. Recovered by package A-core
// (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order is checked by
// tools/macos/ai_behaviour_vtables.py.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_BAPINTERPRETER_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_BAPINTERPRETER_H

#include "../AIBotActionPlan/BAPTypes.h"
#include "PlanInterpreter.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPEntry;
class Bot;
class BotScript;

// sizeof 0x10 (`operator new(0x10)` in AISettings::preparePlanInterpreterPool). Layout from the
// constructor 0x2A95E0: +4 = the argument, +0xC = new BAPEntry**[n] each new BAPEntry*[3] zeroed,
// +8 = new u64[3] zeroed. Three LOD rows (BotLodFull 0, Medium 1, Light 2): every loop stops at 2.
// ELF: vtable 0x4BCA48 (3 words), operator new 0x10, package A-core.
class BAPInterpreter : public PlanInterpreter
{
public:
    explicit BAPInterpreter(int numStatementTypes);   // 0x2A96A0
    virtual ~BAPInterpreter();   // 0x2A9830

    virtual bool execute(BotScript*, Bot*, bool& ok, bool& finished);   // 0x2A99D0, slot +0x08; from the base declaration

    // The four return `al`: 0 when the type is out of range or (add/activate) the slot at the given
    // LOD is already taken, 1 otherwise (also when the LOD is above 2 and nothing is done).
    bool addEntry(BAPEntry*);   // 0x2A9D10: addEntry(BotLodFull, entry)
    bool addEntry(BotLodLevel, BAPEntry*);   // 0x2A9D50
    bool removeEntry(BotLodLevel, BAPEntry*);   // 0x2A9E10
    bool activateEntry(BAPStatementType, BotLodLevel);   // 0x2A9ED0

private:
    enum { NumLodLevels = 3 };   // [CHOSEN] name for the literal `cmpl $0x2` loop bound

    /* +0x04 */ int m_numStatementTypes;        // the constructor argument (45 from AISettings) [INFERRED] name
    /* +0x08 */ unsigned long long* m_active;   // [lod]: bit `type` set = the type may be interpreted at that LOD
    /* +0x0C */ BAPEntry*** m_entries;          // [type][lod]: entry or 0 (not owned)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_BAPINTERPRETER_H
