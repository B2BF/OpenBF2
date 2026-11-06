// Idle pattern of aircraft (3d navigation): circles at an altitude, optionally waiting for a pickup.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE3D_H

#include "BBPIdle.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BC2C0 (7 words), operator new 0x14, package D-behaviours.
class BBPIdle3d : public BBPIdle
{
public:
    explicit BBPIdle3d(int id);   // 0x28DCF0
    virtual ~BBPIdle3d();   // 0x28E020

    virtual BotScript* createPlan(Bot*);   // 0x28E120, slot +0x08
    virtual int getType();   // 0x28F4D0, slot +0x18: 2

    void init(float altitude, Bot*, int pickupPlayerId);   // 0x28E0C0

protected:
    /* +0x0C */ float* m_altitude;         // per bot slot, constructed 100.0f
    /* +0x10 */ int*   m_pickupPlayerId;   // per bot slot, -1 = none
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPIDLE3D_H
