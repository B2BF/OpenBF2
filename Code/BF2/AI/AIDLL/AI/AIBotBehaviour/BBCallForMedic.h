// Urgency for a medic call while a nearby friendly medic can reach the bot before man-down expires.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCALLFORMEDIC_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCALLFORMEDIC_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class BBPCallForMedic;
class Bot;
class UrgeCurve;

// ELF: vtable 0x4BA7A0 (13 words), operator new 0x24, package H-deferred.
class BBCallForMedic : public BotBehaviour
{
public:
    BBCallForMedic(unsigned char, unsigned char, BBPCallForMedic*, UrgeCurve*, float*);   // 0x1FF8E0
    virtual ~BBCallForMedic();   // 0x1FFA00

    virtual float calculateUrgency(Bot*, bool, float);   // 0x1FFAA0, slot +0x28; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCALLFORMEDIC_H
