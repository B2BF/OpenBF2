// Idle behaviour: the fallback every vehicle type gets (aibehaviours.ai `setVehicleDefaultBehaviour`).
// Its urgency is the modifier, raised while a bot waits for repairs or for a pickup it was offered.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBIDLE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBIDLE_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class BBPIdle;
class Bot;
class Information;
class UrgeCurve;

// ELF: vtable 0x4BAE80 (13 words), operator new 0x30, package D-behaviours.
class BBIdle : public BotBehaviour
{
public:
    BBIdle(unsigned char index, unsigned char queue, BBPIdle* pattern, UrgeCurve* curve, float* modifiers);   // 0x214C10
    virtual ~BBIdle();   // 0x214D90

    virtual void reset(Bot*);   // 0x215BA0, slot +0x24
    virtual float calculateUrgency(Bot*, bool, float);   // 0x214E90, slot +0x28

    void waitForPickup(Bot* bot, unsigned int equipment, Information* equipmentInfo, BBPIdle* pattern,
                       bool moving, float modifier, float& urgency, int& pickupPlayerId, bool& active);   // 0x2156C0

private:
    // The three per-bot arrays (one entry per bot slot, IAISettings::getMaxNBots), from the constructor.
    /* +0x24 */ double* m_waitStartTime;       // 0.0 = not waiting; the game time the current wait began
    /* +0x28 */ bool*   m_waitingForPickup;    // set when a pickup offer was accepted (waitForPickup)
    /* +0x2C */ float*  m_healthAtWaitStart;   // vehicle health when a repair wait began; 0 = none
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBIDLE_H
