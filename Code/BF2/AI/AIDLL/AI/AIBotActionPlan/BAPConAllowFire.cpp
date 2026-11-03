// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E6E00 (__tcf_0 0x1E6E60, _GLOBAL__I 0x1E6E90): <iostream>'s Init object only.
#include <iostream>

#include "BAPConAllowFire.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E6D20 (C1)
BAPConAllowFire::BAPConAllowFire()
{
}

// ELF 0x1E6D70 (D0 0x1E6D90): base only.
BAPConAllowFire::~BAPConAllowFire()
{
}

// ELF 0x1E6DD0
bool BAPConAllowFire::evaluate(Bot* bot)
{
    return bot->isFireAllowed();
}

// ELF 0x1E6DF0
BAPConditionType BAPConAllowFire::getConditionType() const
{
    return BAPCTAllowFire;
}

} // namespace ai
} // namespace hfe
} // namespace dice
