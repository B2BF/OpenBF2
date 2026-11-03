// Rates cover against incoming shots and sensed occupied vehicles, retaining one threat per bot.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVER_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVER_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class PlanGenerator;
class UrgeCurve;
class DangerObject;

// ELF: vtable 0x4BB2C0 (13 words), operator new 0x2C, package H-deferred.
class BBTakeCover : public BotBehaviour
{
public:
    BBTakeCover(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x21DC00
    virtual ~BBTakeCover();   // 0x21E090

    virtual void reset(Bot*);   // 0x220440, slot +0x24; from the base declaration
    virtual float calculateUrgency(Bot*, bool, float);   // 0x21E190, slot +0x28; from the base declaration

private:
    // [INFERRED] names; both constructors and reset/destruction establish these owned arrays.
    /* +0x24 */ int m_count;
    /* +0x28 */ DangerObject** m_dangers;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVER_H
