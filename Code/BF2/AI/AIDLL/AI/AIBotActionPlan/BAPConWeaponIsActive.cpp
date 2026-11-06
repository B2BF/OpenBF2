// Condition: the plan's weapon variable holds the bot's active weapon. Recovered from libaidll.so
// (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1F2900 (__tcf_0 0x1F2960, _GLOBAL__I 0x1F2990): <iostream>'s Init object only.
#include <iostream>

#include "BAPConWeaponIsActive.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F2790 (C1; C2 0x1F2760): +0x04 = 0 by the base, +0x08 = the weapon variable.
BAPConWeaponIsActive::BAPConWeaponIsActive(BAPVariable<WeaponInfo*>* weapon)
{
    m_weapon = weapon;
}

// ELF 0x1F27E0 (D1; D2 0x1F27C0, D0 0x1F2800): base only.
BAPConWeaponIsActive::~BAPConWeaponIsActive()
{
}

// ELF 0x1F2840: true when the variable's weapon is the WeaponInfo of the active weapon of the
// bot's current equipment. The equipment information is not null-checked, and the active weapon
// handle is fetched once without use before the compared fetch; as compiled.
bool BAPConWeaponIsActive::evaluate(Bot* bot)
{
    IPIArmament* armament = bapinline::info(bot->getCurrentEquipment())->getArmament();
    armament->getWeapon(armament->getActiveWeapon());
    WeaponInfo* active = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
    return m_weapon->m_value == active;
}

// ELF 0x1F28F0
BAPConditionType BAPConWeaponIsActive::getConditionType() const
{
    return BAPCTWeaponIsActive;
}

} // namespace ai
} // namespace hfe
} // namespace dice
