// Behaviour-pattern base: the per-bot "initiated" flags every pattern keeps. Recovered by package
// A-core (docs/macos-ai-behaviour-plan.md) from libaidll.so; the virtual order is checked by
// tools/macos/ai_behaviour_vtables.py.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPPATTERN_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPPATTERN_H

#include "PlanGenerator.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// Behaviour-pattern base. Vtable 0x4BC400.
// ELF: vtable 0x4BC400 (6 words), package A-core.
class BBPPattern : public PlanGenerator
{
public:
    BBPPattern();   // 0x297160
    virtual ~BBPPattern();   // 0x296F20

    // The three are weak in the image (inline in the header, emitted beside BBPAvoidCollision2d);
    // defined out of line in BBPPattern.cpp so this header needs no Bot definition. [CHOSEN]
    virtual bool isInitiated(Bot*);   // 0x2298D0, slot +0x0C; from the base declaration
    virtual void unInit(Bot*);   // 0x2298A0, slot +0x10; from the base declaration
    virtual void forceInitiated(Bot*);   // 0x229900, slot +0x14; from the base declaration

protected:
    // BBPPattern() 0x296FD0: checkPointer(new bool[IAISettings::getMaxNBots()]), all false; indexed by
    // Bot::getBotId(). Freed by the destructor 0x296ED0.
    /* +0x04 */ bool* m_initiated;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPPATTERN_H
