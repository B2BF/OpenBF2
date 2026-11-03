// Weapon selection helpers shared by the fire patterns.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// [INFERRED] file name: the three functions form a translation unit without a static initialiser
// that links between BBPTriggerable.cpp and AIPlanInterpreter/BAPDebugger.cpp; no __FILE__ string
// names it.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_WEAPONCHOICE_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_WEAPONCHOICE_H

#include <vector>

namespace dice {
namespace hfe {
namespace ai {

// Picks the strongest weapon (WeaponInfo::getStrength(strengthType)) among `weapons` (handles)
// whose type matches the three type flags exactly (type 1, 3, 2), with ammunition unless one of
// the type flags is set, and not indirect when `noIndirect`. A weapon whose range brackets
// `distance` beats every weapon out of range; the `current` index counts triple. Returns the
// index into `weapons`, or -1.
int choseWeapon(const std::vector<unsigned int>& weapons, int current, float distance, int strengthType,
                bool noIndirect, bool type1, bool type3, bool type2);   // 0x2A8030

// Picks the strongest weapon with ammunition regardless of range. `allowed` bit 0 admits direct
// weapons, bit 1 indirect weapons, bit 2 weapons of type 1; the `current` index counts triple.
// Returns the index into `weapons`, or -1.
int choseWeaponDistanceIndependent(int current, int strengthType, const std::vector<unsigned int>& weapons,
                                   unsigned int allowed);   // 0x2A8220

// The first thrown weapon with ammunition and strength whose minimum range is at most
// `distance`; the int argument is not read. Returns the index into `weapons`, or -1.
int soldier_choseThrownWeapon(const std::vector<unsigned int>& weapons, int, float distance,
                              int strengthType);   // 0x2A8330

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_WEAPONCHOICE_H
