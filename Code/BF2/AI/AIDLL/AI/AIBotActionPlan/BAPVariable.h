// Typed plan variable owned by a BAPContext (inline template in the image).
// Layout from the inlined constructions in BBPFireInfantery::createPlanInternal (0x2650F0):
// new 0x10 for <bool> and 0x18 for <Vec3>; each new variable is pushed onto the context's
// std::list<BotScriptVariable*>. Instantiated for bool, int, unsigned int, float, Vec3 and
// WeaponInfo* (vtables 0x4BBD38, 0x4BBC68, 0x4BBC28, 0x4BBC58, 0x4BBD28, 0x4BB908).
// S0 skeleton (docs/macos-ai-behaviour-plan.md). Owner: package A-core.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPVARIABLE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPVARIABLE_H

#include "../AIPlanInterpreter/BAPContext.h"

namespace dice {
namespace hfe {
namespace ai {

template <class T>
class BAPVariable : public BotScriptVariable
{
public:
    // [INFERRED] parameter order: the inlined stores are +4 = a pointer the caller keeps in a
    // register (dereferenced before), +8 = the initial value, then the trailing word = 0.
    BAPVariable(void* owner, const T& value) : m_owner(owner), m_value(value), m_tail(0) {}
    virtual ~BAPVariable() {}     // D1/D0 only: vptr store and BotScriptVariable

    /* +0x04 */ void* m_owner;    // [UNRESOLVED] type and name
    /* +0x08 */ T     m_value;    // bool: false, Vec3: (0, 1, 0) at the construction above
    /* after */ int   m_tail;     // = 0; +0x0C for bool, +0x14 for Vec3 [UNRESOLVED] meaning
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPVARIABLE_H
