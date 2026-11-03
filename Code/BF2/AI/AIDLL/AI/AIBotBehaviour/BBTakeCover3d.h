// Aircraft cover urgency and initialization of its evasion pattern.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVER3D_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVER3D_H

#include "BotBehaviour.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class PlanGenerator;
class UrgeCurve;

// ELF: vtable 0x4BB3E0 (13 words), operator new 0x24, package H-deferred.
class BBTakeCover3d : public BotBehaviour
{
public:
    BBTakeCover3d(unsigned char, unsigned char, PlanGenerator*, UrgeCurve*, float*);   // 0x220810
    virtual ~BBTakeCover3d();   // 0x220920

    virtual float calculateUrgency(Bot*, bool, float);   // 0x2209C0, slot +0x28; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_BBTAKECOVER3D_H
