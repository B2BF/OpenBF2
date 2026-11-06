// Copies magazine ammunition or heat from a borrowed weapon into a plan variable.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPWRAPPERWEAPON_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPWRAPPERWEAPON_H

#include "BAPStatement.h"
#include "BAPTypes.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class WeaponInfo;

// ELF: vtable 0x4BA4C0 (8 words), operator new 0x28, package G-vehicles.
class BAPWrapperWeapon : public BAPStatement
{
public:
    enum ValueId : int;   // [UNRESOLVED] enumerators
    BAPWrapperWeapon(BAPWrapperWeapon::ValueId, BAPVariable<int>*, BAPVariable<WeaponInfo*>*, bool);   // 0x1FB050
    BAPWrapperWeapon(BAPWrapperWeapon::ValueId, BAPVariable<float>*, BAPVariable<WeaponInfo*>*, bool);   // 0x1FB310
    virtual ~BAPWrapperWeapon();   // 0x1FB610

    virtual bool execute(Bot*, bool&, bool&);   // 0x1FB470, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1FB5F0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1FB600, slot +0x14; from the base declaration
private:
    /* +0x14 */ ValueId m_valueId;
    /* +0x18 */ BAPVariable<int>* m_integer;
    /* +0x1C */ BAPVariable<float>* m_float;
    /* +0x20 */ BAPVariable<WeaponInfo*>* m_weapon;
    /* +0x24 */ bool m_repeat;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPWRAPPERWEAPON_H
