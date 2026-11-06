// Boat steering, speed regulation and input reset.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_BOATCONTROL_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_BOATCONTROL_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {

namespace io { class PlayerInput; }
namespace ai {

class BAPAMoveTo;
class Bot;
class Information;

// [INFERRED] a namespace: no vtable, no constructor, and the first argument is never `this`.
namespace BoatControl
{
    bool speedControl(Bot*, Information*, const Vec2&, float, float, BAPAMoveTo*, float, io::PlayerInput*, float);   // 0x2AA090
    bool towardsDirection(Bot*, Information*, Vec2, float, BAPAMoveTo*, io::PlayerInput*, float);   // 0x2AA910
    int resetControls(Bot*, bool, bool, bool, bool&);   // 0x2AA9D0; [INFERRED] ELF int
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_BOATCONTROL_H
