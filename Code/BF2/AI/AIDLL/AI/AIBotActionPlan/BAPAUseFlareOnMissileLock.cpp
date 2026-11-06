// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1E3180 (__tcf_0 0x1E31E0, _GLOBAL__I 0x1E3210): <iostream>'s Init object only.
#include <iostream>

#include "BAPAUseFlareOnMissileLock.h"

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

// ELF 0x1E2EE0 (C1)
BAPAUseFlareOnMissileLock::BAPAUseFlareOnMissileLock(Bot* bot, unsigned long long controls, int patternId)
{
    m_bot = bot;
    m_controls = controls;
    m_lockTime = -1.0;
    m_locked = false;
    m_patternId = patternId;
}

// ELF 0x1E2F90 (D0 0x1E2FD0)
BAPAUseFlareOnMissileLock::~BAPAUseFlareOnMissileLock()
{
}

// ELF 0x1E3020. `time` is a function-static double (data 0x4B8E68 = 1.0). The flare input is
// written inline (mapped bit 47 -> inputs[47] = 1). The information is not null-checked.
bool BAPAUseFlareOnMissileLock::execute(Bot* bot, bool& ok, bool& failed)
{
    static double time = 1.0;
    Information* info = bapinline::info(bot->getCurrentEquipment());
    IPIUnit* unit = info->getUnit();
    const int status = unit->getMissileLockStatus();
    const double now = bapinline::gameTime();
    bool locked = m_locked;
    if (!locked && 0 < status) {
        m_locked = true;
        locked = true;
        m_lockTime = now;
    }
    double wait = time;
    if (!info->isOfType(ITAir))
        wait = 0.0;
    if (locked && wait <= now - m_lockTime) {
        io::PlayerInput* input = bot->getInput();
        if ((input->mappingFlags >> io::PIFlareFire) & 1ULL)
            input->inputs[io::PIFlareFire] = 1.0f;
        unit->setMissileLockStatus(0);
        m_locked = false;
    }
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1E3160
BAPStatementType BAPAUseFlareOnMissileLock::getId() const
{
    return BAPSTUseFlareOnMissileLock;   // 40
}

// ELF 0x1E3170
bool BAPAUseFlareOnMissileLock::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
