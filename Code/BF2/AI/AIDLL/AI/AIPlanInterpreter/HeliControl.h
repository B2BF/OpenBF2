// Helicopter controls. Rotational-speed arguments use the vehicle's local axes; movement and
// acceleration arguments use world coordinates. Most helpers require valid current equipment.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_HELICONTROL_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_HELICONTROL_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// [INFERRED] a namespace: no vtable, no constructor, and the first argument is never `this`.
namespace HeliControl
{
    void quickyMoveForward(Bot*, float, const Vec3&);   // 0x2C8A80; [INFERRED] ELF void
    int gotoHeight(Bot*, float, const Vec3&);   // 0x2C8D00; [INFERRED] ELF int
    void strafeTo(Bot*, const Vec3&, const Vec3&);   // 0x2C9060; [INFERRED] ELF void
    void keepRollAt(Bot*, float, float, float, const Vec3&);   // 0x2C9220; [INFERRED] ELF void
    void keepPitchAt(Bot*, float, float, float, const Vec3&, float);   // 0x2C9280; [INFERRED] ELF void
    void keepDotAt(Bot*, int, float, float, float, const Vec3&, float);   // 0x2C92D0; [INFERRED] ELF void
    void limitSpeedTo(Bot*, float, float, float, const Vec3&);   // 0x2C9450; [INFERRED] ELF void
    float keepHeight(Bot*, float, float, const Vec3&);   // 0x2C95D0; [INFERRED] ELF float
    int moveToPoint(Bot*, const Vec3&, float, const Vec3&);   // 0x2C9760; [INFERRED] ELF int
    bool avoidCrash(Bot*, const Vec3&, float&);   // 0x2C9F30; [INFERRED] ELF int, callers {'bool': 2}
    int aimAtDirection(Bot*, const Vec3&, float, float, const Vec3&, float);   // 0x2CA7D0; [INFERRED] ELF int
    float turnTowardsDirection(Bot*, const Vec3&, const Vec3&);   // 0x2CAC20; [INFERRED] ELF float
    int resetControls(Bot*, bool, bool, bool);   // 0x2CAF00; [INFERRED] ELF int
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_HELICONTROL_H
