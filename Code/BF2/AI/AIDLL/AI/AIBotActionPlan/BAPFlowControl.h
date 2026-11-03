// Recovered by package A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order
// is checked by tools/macos/ai_behaviour_vtables.py. Every body carries its ELF address.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCONTROL_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCONTROL_H

#include "BAPStatement.h"

namespace dice {
namespace hfe {

namespace ai {


// Base of If/While/Wait/End: typeinfo only in the image (no vtable, no data; the derived
// constructors go straight from the BAPStatement vptr to their own).
// ELF: package A-core.
class BAPFlowControl : public BAPStatement
{
protected:
    BAPFlowControl() {}
    explicit BAPFlowControl(int patternId) : BAPStatement(patternId) {}
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPFLOWCONTROL_H
