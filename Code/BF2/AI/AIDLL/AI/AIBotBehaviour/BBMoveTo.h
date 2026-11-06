// MoveTo behaviour: follow the bot's waypoint list; the urgency comes from the waypoints.
// Recovered from libaidll.so (Linux 1.5.3153); every body carries its ELF address. Package D.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBMOVETO_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBMOVETO_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class BBPGotoWaypoint;
class Bot;
class UrgeCurve;

// ELF: vtable 0x4BAF80 (13 words), operator new 0x24, package D-behaviours. No members of its own.
class BBMoveTo : public BotBehaviour
{
public:
    BBMoveTo(unsigned char index, unsigned char queue, BBPGotoWaypoint* pattern, UrgeCurve* curve, float* modifiers);   // 0x2193A0
    virtual ~BBMoveTo();   // 0x219490

    virtual float calculateUrgency(Bot*, bool, float);   // 0x219510, slot +0x28
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBMOVETO_H
