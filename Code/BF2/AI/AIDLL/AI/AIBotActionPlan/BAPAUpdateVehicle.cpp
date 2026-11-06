// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E2DA0 (__tcf_0 0x1E2E00, _GLOBAL__I 0x1E2E30): <iostream>'s Init object only.
#include <iostream>

#include "BAPAUpdateVehicle.h"

#include "../AIBot/BotMain.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E2C30 (C1; C2 0x1E2BE0): +4/+8/+0xC = 0, +0x10 = the pattern id.
BAPAUpdateVehicle::BAPAUpdateVehicle(int patternId)
    : BAPAction()
{
    m_controls = 0;
    m_patternId = patternId;
}

// ELF 0x1E2CC0 (D0 0x1E2D00): the inlined ~BAPStatement only.
BAPAUpdateVehicle::~BAPAUpdateVehicle()
{
}

// ELF 0x1E2D50: BotMain::setUpdateCurrentEquipment, then ok = finished = true.
bool BAPAUpdateVehicle::execute(Bot* bot, bool& ok, bool& finished)
{
    static_cast<BotMain*>(bot)->setUpdateCurrentEquipment();
    ok = true;
    finished = true;
    return true;
}

// ELF 0x1E2E50
unsigned long long BAPAUpdateVehicle::getControls()
{
    return 0;
}

// ELF 0x1E2E60
BAPStatementType BAPAUpdateVehicle::getId() const
{
    return BAPSTUpdateVehicle;   // 27
}

// ELF 0x1E2D90
bool BAPAUpdateVehicle::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
