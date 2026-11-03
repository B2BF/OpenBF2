// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EE5E0 (__tcf_0 0x1EE640, _GLOBAL__I 0x1EE670): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectHealth.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EE440 (C1)
BAPConObjectHealth::BAPConObjectHealth(unsigned int object, float health)
{
    m_object = object;
    m_health = health;
}

// ELF 0x1EE4A0 (D0 0x1EE4C0): base only.
BAPConObjectHealth::~BAPConObjectHealth()
{
}

// ELF 0x1EE500
bool BAPConObjectHealth::evaluate(Bot*)
{
    Information* info = bapinline::info(m_object);
    bool result = false;
    if (info)
        result = info->getPhysical()->getHealth() <= m_health;
    return result;
}

// ELF 0x1EE580
BAPConditionType BAPConObjectHealth::getConditionType() const
{
    return BAPCTObjectHealth;
}

// ELF 0x1EE590
void BAPConObjectHealth::streamSelf(std::ostream& out) const
{
    out << "ObjectHealth <= " << static_cast<double>(m_health);
}

} // namespace ai
} // namespace hfe
} // namespace dice
