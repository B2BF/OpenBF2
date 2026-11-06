// Condition: the plan weapon variable holds the active weapon. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONISACTIVE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONISACTIVE_H

#include "BAPConWeapon.h"
#include "BAPTypes.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class WeaponInfo;

// ELF: vtable 0x4B9C28 (5 words), operator new 0x8/0xC, package F-infantry.
class BAPConWeaponIsActive : public BAPConWeapon
{
public:
    explicit BAPConWeaponIsActive(BAPVariable<WeaponInfo*>*);   // 0x1F2790
    virtual ~BAPConWeaponIsActive();   // 0x1F27E0

    virtual bool evaluate(Bot*);   // 0x1F2840, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F28F0, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONISACTIVE_H
