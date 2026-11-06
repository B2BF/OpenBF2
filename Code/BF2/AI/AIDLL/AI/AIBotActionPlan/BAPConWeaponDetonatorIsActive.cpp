// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F2260 (__tcf_0 0x1F22C0, _GLOBAL__I 0x1F22F0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConWeaponDetonatorIsActive.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F20E0 (C1)
BAPConWeaponDetonatorIsActive::BAPConWeaponDetonatorIsActive(BAPVariable<WeaponInfo*>* weapon)
{
    m_weapon = weapon;
}

// ELF 0x1F2130 (D0 0x1F2150): base only.
BAPConWeaponDetonatorIsActive::~BAPConWeaponDetonatorIsActive()
{
}

// ELF 0x1F2190
bool BAPConWeaponDetonatorIsActive::evaluate(Bot* bot)
{
    IPIArmament* armament = bapinline::info(bot->getCurrentEquipment())->getArmament();
    armament->getWeapon(armament->getActiveWeapon());
    WeaponInfo* active = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    bool result = false;
    if (m_weapon->m_value == active)
        result = m_weapon->m_value->getWeaponState() == 1;
    return result;
}

// ELF 0x1F2250
BAPConditionType BAPConWeaponDetonatorIsActive::getConditionType() const
{
    return BAPCTWeaponDetonatorIsActive;
}

} // namespace ai
} // namespace hfe
} // namespace dice
