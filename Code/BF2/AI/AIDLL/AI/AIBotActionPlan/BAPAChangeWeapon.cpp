// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D1450 (__tcf_0 0x1D14B0, _GLOBAL__I 0x1D14E0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAChangeWeapon.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D0F00 (C1): the weapon is resolved now; controls = the 64-bit `1 << getWeaponActivate()`.
// Neither the information nor the weapon is null-checked.
BAPAChangeWeapon::BAPAChangeWeapon(Bot* bot, int weaponIndex, int patternId)
{
    m_bot = bot;
    m_weaponIndex = weaponIndex;
    m_patternId = patternId;
    IPIArmament* armament = bapinline::info(bot->getCurrentEquipment())->getArmament();
    m_weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(weaponIndex));
    m_controls = 1ULL << (static_cast<unsigned int>(m_weapon->getWeaponActivate()) & 63u);
}

// ELF 0x1D1030 (D0 0x1D1070)
BAPAChangeWeapon::~BAPAChangeWeapon()
{
}

// ELF 0x1D10C0. Soldiers: while another slot is active, release fire / alt fire / reload and
// press the activation input (ok, not failed); once it is active, release them all (done).
// Vehicles: press the activation input (Input::setInput inlined with its range check) and finish.
bool BAPAChangeWeapon::execute(Bot* bot, bool& ok, bool& failed)
{
    Information* info = bapinline::info(bot->getCurrentEquipment());
    if (!info) {
        ok = false;
        failed = true;
        return true;
    }
    io::PlayerInput* input = bot->getInput();
    if (info->isOfType(ITSoldier)) {
        if (info->getArmament()->getActiveWeapon() != m_weaponIndex) {
            input->setInput(io::PIFire, 0.0f);
            input->setInput(io::PIAltFire, 0.0f);
            input->setInput(io::PIReload, 0.0f);
            input->setInput(static_cast<io::PlayerInputMap>(m_weapon->getWeaponActivate()), 1.0f);
            ok = true;
            failed = false;
            return true;
        }
        input->setInput(static_cast<io::PlayerInputMap>(m_weapon->getWeaponActivate()), 0.0f);
        input->setInput(io::PIFire, 0.0f);
        input->setInput(io::PIAltFire, 0.0f);
        input->setInput(io::PIReload, 0.0f);
        ok = true;
        failed = true;
        return true;
    }
    input->setInput(static_cast<io::PlayerInputMap>(m_weapon->getWeaponActivate()), 1.0f);
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1D1430
BAPStatementType BAPAChangeWeapon::getId() const
{
    return BAPSTChangeWeapon;   // 28
}

// ELF 0x1D1440
bool BAPAChangeWeapon::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
