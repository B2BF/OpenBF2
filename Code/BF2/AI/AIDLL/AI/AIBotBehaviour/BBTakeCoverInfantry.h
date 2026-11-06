// Rates infantry cover against incoming shots and sensed occupied objects.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVERINFANTRY_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVERINFANTRY_H

#include "../../../../Math/Vec3.h"
#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class Information;
class PlanGenerator;
class UrgeCurve;
class DangerObject;
class FireDanger;
class InfoDanger;

// ELF: vtable 0x4BB480 (13 words), operator new 0x3C, package H-deferred.
class BBTakeCoverInfantry : public BotBehaviour
{
public:
    BBTakeCoverInfantry(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x2218F0
    virtual ~BBTakeCoverInfantry();   // 0x2220A0

    virtual float calculateUrgency(Bot*, bool, float);   // 0x222340, slot +0x28; from the base declaration

    void unInit(Bot*);   // 0x222290; [INFERRED] ELF void
    bool getCoverObject(Bot*, const Vec3&, Information*, bool, unsigned int&, Vec3&, float&, float&, bool);   // 0x223920; [INFERRED] ELF int, callers {'bool': 1}

private:
    // [INFERRED] names; constructors and urgency bookkeeping establish the array types.
    /* +0x24 */ int m_count;
    /* +0x28 */ DangerObject** m_dangers;
    /* +0x2C */ FireDanger* m_fireDangers;
    /* +0x30 */ InfoDanger* m_infoDangers;
    /* +0x34 */ double m_lastIncomingFireTime; // shared by all bots, not initialized by construction
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVERINFANTRY_H
