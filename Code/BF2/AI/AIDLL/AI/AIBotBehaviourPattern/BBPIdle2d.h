// Idle pattern of ground vehicles (2d navigation): waits, optionally for a passenger to pick up.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE2D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE2D_H

#include "BBPIdle.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC240 (7 words), operator new 0x10, package D-behaviours.
class BBPIdle2d : public BBPIdle
{
public:
    explicit BBPIdle2d(int id);   // 0x28C880
    virtual ~BBPIdle2d();   // 0x28CA70

    virtual BotScript* createPlan(Bot*);   // 0x28CB60, slot +0x08
    virtual int getType();   // 0x28DA00, slot +0x18: 1

    void init(Bot*, int pickupPlayerId);   // 0x28CB10

protected:
    /* +0x0C */ int* m_pickupPlayerId;   // per bot slot, -1 = none (BBIdle::calculateUrgency passes it)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE2D_H
