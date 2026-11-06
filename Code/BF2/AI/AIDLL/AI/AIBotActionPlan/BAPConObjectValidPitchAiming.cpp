// Aiming condition restricted to the camera's pitch range. Recovered from libaidll.so 1.5.3153
// (ia-32); ELF addresses on every body.

#include "BAPConObjectValidPitchAiming.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/Information.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1F1110, __tcf_0 0x1F1170, _GLOBAL__I 0x1F11A0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F0F20 (C1; C2 0x1F0EC0): the bot's own equipment shoots; no static-attack tracking.
BAPConObjectValidPitchAiming::BAPConObjectValidPitchAiming(unsigned int object, BAPVariable<bool>* aimValid,
                                                           BAPVariable<Vec3>* aimDirection)
    : BAPConObjectValidAiming(object, aimValid, aimDirection, false)
{
}

// ELF 0x1F0FE0 (C1; C2 0x1F0F80)
BAPConObjectValidPitchAiming::BAPConObjectValidPitchAiming(unsigned int object, unsigned int shooter, BAPVariable<bool>* aimValid,
                                                           BAPVariable<Vec3>* aimDirection)
    : BAPConObjectValidAiming(object, shooter, aimValid, aimDirection, false)
{
}

// ELF 0x1F11C0 (weak D1; D0 0x1F1210): the Aimer and the base only.
BAPConObjectValidPitchAiming::~BAPConObjectValidPitchAiming()
{
}

// ELF 0x1F1040. The aim must be solvable (internalAiming) and the camera must be able to turn
// to the direction in pitch (IPIControlInfo::validateCameraDirectionPitch on m_shooter's, or the
// bot's, control info; the information is not null-checked).
bool BAPConObjectValidPitchAiming::evaluate(Bot* bot)
{
    Vec3 dir = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (!internalAiming(bot, dir))
        return false;
    const unsigned int handle = m_shooter == 0xFFFFFFFFu ? bot->getCurrentEquipment() : m_shooter;
    return bapinline::info(handle)->getControlInfo()->validateCameraDirectionPitch(dir);
}

// ELF 0x1F1100
BAPConditionType BAPConObjectValidPitchAiming::getConditionType() const
{
    return BAPCTObjectValidAiming;   // 32, as the base
}

} // namespace ai
} // namespace hfe
} // namespace dice
