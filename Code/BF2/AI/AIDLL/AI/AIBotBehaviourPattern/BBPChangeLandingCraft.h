// Seat-change pattern for landing craft: moves the bot to another seat or out of the vehicle.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGELANDINGCRAFT_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGELANDINGCRAFT_H

#include "BBPPattern.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BotScript;

// ELF: vtable 0x4BBA80 (6 words), operator new 0x14, package G-vehicles.
class BBPChangeLandingCraft : public BBPPattern
{
public:
    explicit BBPChangeLandingCraft(int);   // 0x23DFD0
    virtual ~BBPChangeLandingCraft();   // 0x23E2C0

    virtual BotScript* createPlan(Bot*);   // 0x23E400, slot +0x08; from the base declaration

    void init(unsigned int target, bool sameVehicle, Bot*);   // 0x23E390; ELF void (no return value materialized)

    /* +0x08 */ int m_patternId;          // the constructor argument
    /* +0x0C */ unsigned int* m_target;   // per bot: init's handle [INFERRED] name
    /* +0x10 */ bool* m_sameVehicle;      // per bot: init's bool [INFERRED] name (as BBPChange's)
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPCHANGELANDINGCRAFT_H
