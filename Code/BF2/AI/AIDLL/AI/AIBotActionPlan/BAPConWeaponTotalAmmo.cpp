// Compares current weapon ammunition with a plan variable using != or <=.

#include "BAPConWeaponTotalAmmo.h"

#include "../AIKnowledge/WeaponInfo.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F3A00 (C1; C2 0x1F39C0)
BAPConWeaponTotalAmmo::BAPConWeaponTotalAmmo(BAPVariable<WeaponInfo*>* weapon,
    BAPVariable<int>* ammo, bool notEqual) : m_notEqual(notEqual), m_ammo(ammo)
{
    m_weapon = weapon;
}

// ELF 0x1F3AA0 (D1; D2 0x1F3A80, D0 0x1F3AC0)
BAPConWeaponTotalAmmo::~BAPConWeaponTotalAmmo()
{
}

// ELF 0x1F3A40
bool BAPConWeaponTotalAmmo::evaluate(Bot*)
{
    const int ammo = m_weapon->m_value->getCurrentAmmo();
    if (m_notEqual)
        return ammo != m_ammo->m_value;
    return ammo <= m_ammo->m_value;
}

// ELF 0x1F3B00
BAPConditionType BAPConWeaponTotalAmmo::getConditionType() const
{
    return BAPCTWeaponTotalAmmo;
}

} // namespace ai
} // namespace hfe
} // namespace dice
