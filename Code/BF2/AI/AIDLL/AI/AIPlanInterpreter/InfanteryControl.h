// InfanteryControl: the soldier input helpers of the infantry plan-interpreter entries
// (attested TU `AI/AIDLL/AI/AIPlanInterpreter/InfanteryControl.cpp`). Package B
// (docs/macos-ai-behaviour-plan.md). Signatures from the mangled ELF names; return types checked
// against the bodies and their callers.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_INFANTERYCONTROL_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_INFANTERYCONTROL_H

#include "../../../AIInterface/BFKnowledge/SoldierPose.h"
#include "../../../../Math/Vec3.h"
#include "../../../../IO/Input/Input.h"

namespace dice {
namespace hfe {
struct Mat4;
namespace io { class PlayerInput; }
namespace ai {

class Bot;
class BAPAMoveTo;
class IPIControlInfo;
class IPIMobile;
class IPIPhysical;
class Information;
class SensingAgent;

// Presses or releases the zoom input so that the bot's active weapon leaves zoom.
void removeZoom(Bot*);   // 0x2CB200
// Throttle (and sprint) for a soldier whose own top speed is maxSpeed; `sprint` comes from
// BAPAMoveTo::getIsSprinting.
void infanteryControlThrottle(Bot*, io::PlayerInput*, IPIControlInfo*, float maxSpeed, bool sprint);   // 0x2CB550
// Releases the movement inputs (`movement`), levels the look (`look`) and releases the fire,
// weapon-select and use inputs (`weapons`). Always true; the callers store it into their ok flag.
bool infanteryResetControls(Bot*, io::PlayerInput*, bool movement, bool look, bool weapons);   // 0x2CB660
// Presses PICrouch / PILie to go from `current` to `wanted`. 0 only after an unknown pose (an
// Error Debug, which aborts).
int infanteryPose(io::PlayerInput*, SoldierPose current, SoldierPose wanted);   // 0x2CB9B0
// True when the object `handle` left the ring [radius, radius + 1.5] around `point`.
bool needsUpdate(const Vec2& point, unsigned int handle, float radius);   // 0x2CBC90
bool updateInfantryMoveToPosWithObstacles(Bot*, const Vec2&, const Vec3&, unsigned int, Vec2&, unsigned int&, float&, Vec2&, float&, bool&, Information*, bool);   // 0x2CBD70

namespace infantry_detail {

// [INFERRED] The image expands an inline input reader at every site (`[input+0x100/0x104]`,
// shrd, `test 1`, then `fld [input+4*i]`): the value when the input is mapped, else 0. Indices
// above 63 read 0 (`cmp idx,0x3f; jg`). The Input template in IO/ has no such member, so the
// reader lives here.
float inputValue(const io::PlayerInput* input, io::PlayerInputMap index);

// [INFERRED] The inline Vec3 normalise the entries expand: unchanged when |v|^2 is within
// FLT_EPSILON of 1, zeroed when |v|^2 is within FLT_EPSILON of 0, else scaled by 1/|v|.
void normaliseInline(Vec3& v);
// a x b, component order as the image computes it
Vec3 crossInline(const Vec3& a, const Vec3& b);

// [CHOSEN, not recovered] Bridge to the SensingAgent move direction: the move entries store the
// direction inline at SensingAgent +0x50 (Vec2) and zero the double at +0x7C (both private in
// AIBot/SensingAgent.h). Stops by name until ai-bot adds the writer.
void setSensingMoveDirection(SensingAgent* agent, const Vec2& direction);

} // namespace infantry_detail

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_INFANTERYCONTROL_H
