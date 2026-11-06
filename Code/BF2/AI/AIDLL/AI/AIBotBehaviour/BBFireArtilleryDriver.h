// Artillery driver fire behaviour: delegates to the occupied gun seat or the unarmed behaviour.
// Recovered from the symbolized Linux 1.50 libaidll.so; offsets refer to the ia-32 image.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIREARTILLERYDRIVER_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIREARTILLERYDRIVER_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class BBPFire;
class BBFireLargeBore;
class BBFireUnarmed;
class Bot;
class UrgeCurve;

// ELF: vtable 0x4BAC40 (13 words), operator new 0x2C, package H-deferred.
class BBFireArtilleryDriver : public BotBehaviour
{
public:
    BBFireArtilleryDriver(unsigned char, unsigned char, BBPFire*, UrgeCurve*, float*);   // 0x20E870
    virtual ~BBFireArtilleryDriver();   // 0x20ED70

    virtual float calculateUrgency(Bot*, bool, float);   // 0x20EE70, slot +0x28; from the base declaration

private:
    /* +0x24 */ BBFireUnarmed* m_unarmed;
    /* +0x28 */ BBFireLargeBore* m_gunner;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBFIREARTILLERYDRIVER_H
