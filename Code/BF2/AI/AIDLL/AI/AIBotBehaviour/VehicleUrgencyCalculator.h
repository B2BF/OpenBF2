// Vehicle urgency helpers shared by the change/enter-vehicle behaviours (BBChange). Package A-core
// (docs/macos-ai-behaviour-plan.md); TU VehicleUrgencyCalculator.cpp, libaidll.so 0x2265E0..0x228EF0.
// The image has no header symbol; the free functions are in namespace dice::hfe::ai.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOUR_VEHICLEURGENCYCALCULATOR_H
#define AI_AIDLL_AI_AIBOTBEHAVIOUR_VEHICLEURGENCYCALCULATOR_H

namespace dice {
namespace hfe {
namespace ai {

class Bot;
class Information;

// ELF 0x2265E0 (318 instructions; returns `al`).
bool closeAndShouldAvoidVehicles(Information* vehicle, int side, Bot* bot);

// ELF 0x2269E0 (175 instructions; x87 float return). Callers: BBChange (9 sites, 0x20096F..0x202C7C).
// The two `const float*` are per-weapon-class rows indexed by IAISettings slot +0x34 counts;
// [UNRESOLVED] parameter names.
float calculateVehicleUrgency(Bot* bot, Information* vehicle, const float* strengths, const float* weights,
                              bool flag, Information* current, float a, float b, int side);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOUR_VEHICLEURGENCYCALCULATOR_H
