// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

#include "BAPConWeaponHeat.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F25A0 (C1): one limit for both.
BAPConWeaponHeat::BAPConWeaponHeat(BAPVariable<WeaponInfo*>* weapon, float limit)
{
    m_overheat = limit;
    m_cooled = limit;
    m_weapon = weapon;
    m_overheated = false;
}

// ELF 0x1F2620 (C1)
BAPConWeaponHeat::BAPConWeaponHeat(BAPVariable<WeaponInfo*>* weapon, float overheat, float cooled)
{
    m_weapon = weapon;
    m_overheat = overheat;
    m_overheated = false;
    m_cooled = cooled;
}

// ELF 0x1F2680 (D0 0x1F26A0)
BAPConWeaponHeat::~BAPConWeaponHeat()
{
}

// ELF 0x1F26E0: WeaponInfo::getHeat (+0x24).
bool BAPConWeaponHeat::evaluate(Bot*)
{
    if (!m_overheated) {
        if (m_overheat <= m_weapon->m_value->getHeat()) {
            m_overheated = true;
            return false;
        }
    } else {
        if (m_cooled < m_weapon->m_value->getHeat())
            return false;
        m_overheated = false;
    }
    return true;
}

// ELF 0x1F2750
BAPConditionType BAPConWeaponHeat::getConditionType() const
{
    return BAPCTWeaponHeat;   // 46
}

} // namespace ai
} // namespace hfe
} // namespace dice
