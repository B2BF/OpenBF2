// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E75D0 (__tcf_0 0x1E7630, _GLOBAL__I 0x1E7660): <iostream>'s Init object only.
#include <iostream>

#include "BAPConIsSprinting.h"

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

// ELF 0x1E74A0 (C1)
BAPConIsSprinting::BAPConIsSprinting()
{
}

// ELF 0x1E74F0 (D0 0x1E7510): base only.
BAPConIsSprinting::~BAPConIsSprinting()
{
}

// ELF 0x1E7550
bool BAPConIsSprinting::evaluate(Bot* bot)
{
    return bapinline::mappedInput(bot->getInput(), io::PISprint) != 0.0f;
}

// ELF 0x1E75C0
BAPConditionType BAPConIsSprinting::getConditionType() const
{
    return BAPCTIsSprinting;
}

} // namespace ai
} // namespace hfe
} // namespace dice
