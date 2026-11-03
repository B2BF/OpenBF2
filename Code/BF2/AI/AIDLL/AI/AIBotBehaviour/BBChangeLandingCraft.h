// Landing-craft seat selection and disembark urgency.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGELANDINGCRAFT_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGELANDINGCRAFT_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class Information;
class PlanGenerator;
class UrgeCurve;

// ELF: vtable 0x4BA920 (13 words), operator new 0x24, package G-vehicles.
class BBChangeLandingCraft : public BotBehaviour
{
public:
    BBChangeLandingCraft(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x203A80
    virtual ~BBChangeLandingCraft();   // 0x203B90

    virtual float calculateUrgency(Bot*, bool, float);   // 0x203DD0, slot +0x28; from the base declaration

    float checkLandingCraftBookings(Bot*);   // 0x203C30
    bool isOnlyPrimaryInVehicle(Information*, int);   // 0x203D00
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBCHANGELANDINGCRAFT_H
