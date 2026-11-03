// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F24B0 (__tcf_0 0x1F2510, _GLOBAL__I 0x1F2540): <iostream>'s Init object only.
#include <iostream>

#include "BAPConWeaponHasLockOnTarget.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F2350 (C1)
BAPConWeaponHasLockOnTarget::BAPConWeaponHasLockOnTarget(BAPVariable<WeaponInfo*>* weapon, unsigned int unused)
{
    m_weapon = weapon;
    m_unusedC = unused;
}

// ELF 0x1F23B0 (D0 0x1F23D0): base only.
BAPConWeaponHasLockOnTarget::~BAPConWeaponHasLockOnTarget()
{
}

// ELF 0x1F2410: `getLockedTarget()->getInformation(bot->getSide())` (IAIObject +0xB8) resolves.
bool BAPConWeaponHasLockOnTarget::evaluate(Bot* bot)
{
    IAIObject* target = m_weapon->m_value->getLockedTarget();
    bool result = false;
    if (target)
        result = bapinline::info(target->getInformation(bot->getSide())) != 0;
    return result;
}

// ELF 0x1F24A0
BAPConditionType BAPConWeaponHasLockOnTarget::getConditionType() const
{
    return BAPCTWeaponHasLockOnTarget;
}

} // namespace ai
} // namespace hfe
} // namespace dice
