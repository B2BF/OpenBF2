// Fixed-wing flight and attitude controls.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_PLANECONTROL_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_PLANECONTROL_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {

struct Mat4;
namespace io { class PlayerInput; }
namespace ai {

class Bot;
class IPIControlInfo;
class IPIMobile;
class Information;

// [INFERRED] a namespace: no vtable, no constructor, and the first argument is never `this`.
namespace PlaneControl
{
    bool towardsPoint(Bot*, Information*, Vec3, float, const Vec3&, const Vec3&, const Vec3&, float, io::PlayerInput*);   // 0x2CDB00; [INFERRED] ELF bool
    bool aimAtDirection(Bot*, Information*, const Vec3&, float, float, float, const Vec3&, const Vec3&, const Vec3&, io::PlayerInput*);   // 0x2CE3C0; [INFERRED] ELF bool
    bool towardsDirection(const Vec3&, float, io::PlayerInput*, const Mat4&, float, float, bool, const Vec3&, const Vec3&, const Vec3&, IPIMobile*, IPIControlInfo*, float, float, bool);   // 0x2CE6D0; [INFERRED] ELF int, callers {'bool': 1}
    int roll(float, float, io::PlayerInput*, const Mat4&, float, bool, const Vec3&, const Vec3&, const Vec3&, const Vec3&, const Vec3&, const Vec3&, IPIControlInfo*, float, bool, bool&);   // 0x2CF9F0; [INFERRED] ELF int
    int resetControls(Bot*, Information*, io::PlayerInput*, bool, bool, bool);   // 0x2D03E0; [INFERRED] ELF int
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_PLANECONTROL_H
