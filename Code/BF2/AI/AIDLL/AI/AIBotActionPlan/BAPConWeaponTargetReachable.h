// Weapon/target reachability condition with an owned ballistic aimer and a position output.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONTARGETREACHABLE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONTARGETREACHABLE_H

#include "../../../../Math/Vec3.h"
#include "BAPConWeapon.h"
#include "BAPTypes.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class Aimer;
class WeaponInfo;

// ELF: vtable 0x4B9DA8 (5 words), package H-deferred.
class BAPConWeaponTargetReachable : public BAPConWeapon
{
public:
    BAPConWeaponTargetReachable(BAPVariable<Vec3>*, BAPVariable<WeaponInfo*>*, BAPVariable<unsigned int>*);   // 0x1F30B0
    virtual ~BAPConWeaponTargetReachable();   // 0x1F3180

    virtual bool evaluate(Bot*);   // 0x1F3250, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F3900, slot +0x04; from the base declaration
private:
    /* +0x0C */ BAPVariable<unsigned int>* m_target;
    /* +0x10 */ BAPVariable<Vec3>* m_position;
    /* +0x14 */ Aimer* m_aimer;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONTARGETREACHABLE_H
