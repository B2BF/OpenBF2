// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EAC00 (__tcf_0 0x1EAC60, _GLOBAL__I 0x1EAC90): <iostream>'s Init object only.
#include <iostream>

#include "BAPConStrategicObjectStatus.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../AIStrategicEntity/AIStrategicObject.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EAB00 (C1)
BAPConStrategicObjectStatus::BAPConStrategicObjectStatus(AIStrategicObject* object, int side, bool zero)
{
    m_object = object;
    m_side = side;
    m_zero = zero;
}

// ELF 0x1EAB90 (D0 0x1EABB0): base only.
BAPConStrategicObjectStatus::~BAPConStrategicObjectStatus()
{
}

// ELF 0x1EAB40: `object->+0x70 + 4 * side` (AIStrategicObject::getStatus(side), 1-based side).
bool BAPConStrategicObjectStatus::evaluate(Bot*)
{
    const int status = static_cast<int>(m_object->getStatus(m_side));
    return m_zero ? status == 0 : status != 0;
}

// ELF 0x1EABF0
BAPConditionType BAPConStrategicObjectStatus::getConditionType() const
{
    return BAPCTStrategicObjectStatus;
}

} // namespace ai
} // namespace hfe
} // namespace dice
