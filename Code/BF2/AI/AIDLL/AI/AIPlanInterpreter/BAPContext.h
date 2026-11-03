// Variable scope for a bot action plan. [INFERRED] Source placement.
// Call clear to delete the owned variables before destruction; the destructor frees only list nodes.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_BAPCONTEXT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIPLANINTERPRETER_BAPCONTEXT_H

#include <list>

namespace dice {
namespace hfe {
namespace ai {

// Base for owned script variables; deletion uses the virtual destructor.
class BotScriptVariable
{
public:
    virtual ~BotScriptVariable();       // - the vptr store, nothing more
};

class BAPContext
{
public:
    BAPContext();
    ~BAPContext();                      // - , and it leaks; see above
    void clear();

    // [CHOSEN, not recovered] added by ai-w1-e-fire (coord/ai-requests.md 2026-11-01); the owner may
    // rename it. Every plan variable the fire patterns create is put at the front of the list:
    // BBPFireInfantery::createPlanInternal calls `std::list<BotScriptVariable*>::insert(begin(), &v)`
    // right after each inlined BAPVariable construction (0x2654C6, 0x265547, ... 0x27127A).
    void addVariable(BotScriptVariable* variable) { m_variables.insert(m_variables.begin(), variable); }

private:
    /* 0 */ std::list<BotScriptVariable*> m_variables;   // the whole object
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_BAPCONTEXT_H
