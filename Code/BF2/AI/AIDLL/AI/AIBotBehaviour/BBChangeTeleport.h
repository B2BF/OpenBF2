// Urgency for changing seats through the teleport change pattern.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGETELEPORT_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGETELEPORT_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class PlanGenerator;
class UrgeCurve;

// ELF: vtable 0x4BA9C0 (13 words), operator new 0x24, package G-vehicles.
class BBChangeTeleport : public BotBehaviour
{
public:
    BBChangeTeleport(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x205010
    virtual ~BBChangeTeleport();   // 0x205120

    virtual float calculateUrgency(Bot*, bool, float);   // 0x2051C0, slot +0x28; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGETELEPORT_H
