// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F2B50 (__tcf_0 0x1F2BB0, _GLOBAL__I 0x1F2BE0): <iostream>'s Init object only.
#include <iostream>

#include "BAPConWeaponIsZoomed.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F29E0 (C1)
BAPConWeaponIsZoomed::BAPConWeaponIsZoomed()
{
    setOwner(0);
}

// ELF 0x1F2A30 (D0 0x1F2A50)
BAPConWeaponIsZoomed::~BAPConWeaponIsZoomed()
{
}

// ELF 0x1F2A90: the active weapon of the bot's current vehicle/soldier. The image calls
// getWeapon(getActiveWeapon()) twice and drops the first result; both calls are kept. The
// information and its armament are not null-checked.
bool BAPConWeaponIsZoomed::evaluate(Bot* bot)
{
    Information* info = bapinline::info(bot->getCurrentEquipment());
    IPIArmament* armament = info->getArmament();
    armament->getWeapon(armament->getActiveWeapon());
    const unsigned int weapon = armament->getWeapon(armament->getActiveWeapon());
    WeaponInfo* weaponInfo = WeaponInfoManager::m_instance->getInformation(weapon);
    return weaponInfo ? weaponInfo->isZoomed() : false;
}

// ELF 0x1F2B40
BAPConditionType BAPConWeaponIsZoomed::getConditionType() const
{
    return BAPCTWeaponIsZoomed;   // 52
}

} // namespace ai
} // namespace hfe
} // namespace dice
