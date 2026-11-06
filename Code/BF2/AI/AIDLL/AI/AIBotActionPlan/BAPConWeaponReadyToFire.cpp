// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F2F90 (__tcf_0 0x1F2FF0, _GLOBAL__I 0x1F3020): <iostream>'s Init object only.
#include <iostream>

#include "BAPConWeaponReadyToFire.h"

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

// ELF 0x1F2E20 (C1)
BAPConWeaponReadyToFire::BAPConWeaponReadyToFire(BAPVariable<WeaponInfo*>* weapon)
{
    m_weapon = weapon;
}

// ELF 0x1F2E70 (D0 0x1F2E90): base only.
BAPConWeaponReadyToFire::~BAPConWeaponReadyToFire()
{
}

// ELF 0x1F2ED0 (getWeapon(getActiveWeapon()) is called twice, the first result dropped).
bool BAPConWeaponReadyToFire::evaluate(Bot* bot)
{
    IPIArmament* armament = bapinline::info(bot->getCurrentEquipment())->getArmament();
    armament->getWeapon(armament->getActiveWeapon());
    WeaponInfo* active = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    bool result = false;
    WeaponInfo* weapon = m_weapon->m_value;
    if (weapon == active)
        result = weapon->isReadyToFire();
    return result;
}

// ELF 0x1F2F80
BAPConditionType BAPConWeaponReadyToFire::getConditionType() const
{
    return BAPCTWeaponReadyToFire;
}

} // namespace ai
} // namespace hfe
} // namespace dice
