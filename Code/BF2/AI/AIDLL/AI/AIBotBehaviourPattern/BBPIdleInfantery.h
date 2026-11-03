// Idle pattern of infantry: what a soldier does while nothing more urgent is chosen.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLEINFANTERY_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLEINFANTERY_H

#include "BBPIdle.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC340 (7 words), operator new 0xC, package D-behaviours. No members of its own.
class BBPIdleInfantery : public BBPIdle
{
public:
    explicit BBPIdleInfantery(int id);   // 0x28F520
    virtual ~BBPIdleInfantery();   // 0x28F5A0

    virtual BotScript* createPlan(Bot*);   // 0x28F650, slot +0x08
    virtual int getType();   // 0x290A10, slot +0x18: 3

    void init(Bot*);   // 0x28F620
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLEINFANTERY_H
