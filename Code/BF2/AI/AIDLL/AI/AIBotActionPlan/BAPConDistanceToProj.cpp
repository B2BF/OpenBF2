// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E70C0 (__tcf_0 0x1E7120, _GLOBAL__I 0x1E7150): <iostream>'s Init object only.
#include <iostream>

#include "BAPConDistanceToProj.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E6EF0 (C1)
BAPConDistanceToProj::BAPConDistanceToProj(IPIArmament* armament, float distance)
{
    m_distanceSqr = distance * distance;
    m_armament = armament;
}

// ELF 0x1E7050 (D0 0x1E7070)
BAPConDistanceToProj::~BAPConDistanceToProj()
{
}

// ELF 0x1E6F30: IPIArmament::getProjectilePosition (+0x38); the information is not null-checked.
bool BAPConDistanceToProj::evaluate(Bot* bot)
{
    Vec3 p = bapinline::vec3(0.0f, 0.0f, 0.0f);
    bool result = false;
    if (m_armament->getProjectilePosition(p)) {
        const Vec3& self = bapinline::info(bot->getCurrentEquipment())->getPosition();
        result = (p.z - self.z) * (p.z - self.z) + (p.x - self.x) * (p.x - self.x) + (p.y - self.y) * (p.y - self.y)
                 < m_distanceSqr;
    }
    return result;
}

// ELF 0x1E70B0
BAPConditionType BAPConDistanceToProj::getConditionType() const
{
    return BAPCTDistanceToProj;   // 15
}

} // namespace ai
} // namespace hfe
} // namespace dice
