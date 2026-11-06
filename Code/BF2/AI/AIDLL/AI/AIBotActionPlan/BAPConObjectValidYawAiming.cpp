// Aiming condition restricted to the camera's yaw range. Recovered from libaidll.so 1.5.3153
// (ia-32); ELF addresses on every body.

#include "BAPConObjectValidYawAiming.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/Information.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1F1830, __tcf_0 0x1F1890, _GLOBAL__I 0x1F18C0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F1640 (C1; C2 0x1F15E0): the bot's own equipment shoots; no static-attack tracking.
BAPConObjectValidYawAiming::BAPConObjectValidYawAiming(unsigned int object, BAPVariable<bool>* aimValid,
                                                       BAPVariable<Vec3>* aimDirection)
    : BAPConObjectValidAiming(object, aimValid, aimDirection, false)
{
}

// ELF 0x1F1700 (C1; C2 0x1F16A0)
BAPConObjectValidYawAiming::BAPConObjectValidYawAiming(unsigned int object, unsigned int shooter, BAPVariable<bool>* aimValid,
                                                       BAPVariable<Vec3>* aimDirection)
    : BAPConObjectValidAiming(object, shooter, aimValid, aimDirection, false)
{
}

// ELF 0x1F18E0 (weak D1; D0 0x1F1930): the Aimer and the base only.
BAPConObjectValidYawAiming::~BAPConObjectValidYawAiming()
{
}

// ELF 0x1F1760. The aim must be solvable (internalAiming) and the camera must be able to turn
// to the direction in yaw (IPIControlInfo::validateCameraDirectionYaw on m_shooter's, or the
// bot's, control info; the information is not null-checked).
bool BAPConObjectValidYawAiming::evaluate(Bot* bot)
{
    Vec3 dir = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (!internalAiming(bot, dir))
        return false;
    const unsigned int handle = m_shooter == 0xFFFFFFFFu ? bot->getCurrentEquipment() : m_shooter;
    return bapinline::info(handle)->getControlInfo()->validateCameraDirectionYaw(dir);
}

// ELF 0x1F1820
BAPConditionType BAPConObjectValidYawAiming::getConditionType() const
{
    return BAPCTObjectValidAiming;   // 32, as the base
}

} // namespace ai
} // namespace hfe
} // namespace dice
