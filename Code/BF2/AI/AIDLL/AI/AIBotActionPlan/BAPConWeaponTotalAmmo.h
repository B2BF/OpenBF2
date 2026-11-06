// Compares current weapon ammunition with a plan variable using != or <=.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONTOTALAMMO_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONTOTALAMMO_H

#include "BAPConWeapon.h"
#include "BAPTypes.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class WeaponInfo;

// ELF: vtable 0x4B9E08 (5 words), package H-deferred.
class BAPConWeaponTotalAmmo : public BAPConWeapon
{
public:
    BAPConWeaponTotalAmmo(BAPVariable<WeaponInfo*>*, BAPVariable<int>*, bool);   // 0x1F3A00
    virtual ~BAPConWeaponTotalAmmo();   // 0x1F3AA0

    virtual bool evaluate(Bot*);   // 0x1F3A40, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F3B00, slot +0x04; from the base declaration
private:
    /* +0x0C */ bool m_notEqual;
    /* +0x10 */ BAPVariable<int>* m_ammo;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONWEAPONTOTALAMMO_H
