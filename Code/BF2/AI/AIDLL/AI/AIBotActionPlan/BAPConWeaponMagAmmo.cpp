// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

#include "BAPConWeaponMagAmmo.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F2C40 (C1)
BAPConWeaponMagAmmo::BAPConWeaponMagAmmo(BAPVariable<WeaponInfo*>* weapon, BAPVariable<int>* count, bool notEqual)
{
    m_weapon = weapon;
    m_notEqual = notEqual;
    m_count = count;
}

// ELF 0x1F2D80 (D0 0x1F2DA0)
BAPConWeaponMagAmmo::~BAPConWeaponMagAmmo()
{
}

// ELF 0x1F2C80. Default form: no mags left counts as true, otherwise shots in mag <= n. The
// flag form: no mags: n != 0; negative mags: false; else shots in mag != n. Every getter call of
// the image is kept.
bool BAPConWeaponMagAmmo::evaluate(Bot*)
{
    WeaponInfo* weapon = m_weapon->m_value;
    if (!m_notEqual) {
        bool result = false;
        if (weapon->getNumberOfMags() >= 0) {
            result = true;
            if (weapon->getNumberOfMags() != 0) {
                result = false;
                if (weapon->getShotsInMag(0) >= 0)
                    result = weapon->getShotsInMag(0) <= m_count->m_value;
            }
        }
        return result;
    }
    if (weapon->getNumberOfMags() == 0)
        return m_count->m_value != 0;
    if (weapon->getNumberOfMags() < 0)
        return false;
    return weapon->getShotsInMag(0) != m_count->m_value;
}

// ELF 0x1F2DE0
BAPConditionType BAPConWeaponMagAmmo::getConditionType() const
{
    return BAPCTWeaponMagAmmo;   // 41
}

} // namespace ai
} // namespace hfe
} // namespace dice
