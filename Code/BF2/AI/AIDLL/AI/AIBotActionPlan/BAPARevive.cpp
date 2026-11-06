// Revive action. Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries
// its ELF address.

// static init: ELF 0x1DEB00 (__tcf_0 0x1DEB60, _GLOBAL__I 0x1DEB90): <iostream>'s Init object only.
#include <iostream>

#include "BAPARevive.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../../../../IO/Input/Input.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DE8B0 (C1; C2 0x1DE860): the inline BAPStatement constructor (+0x04, +0x08 zeroed,
// instance counter), +0x10 = pattern id, +0x14 = false.
BAPARevive::BAPARevive(int patternId)
    : BAPStatement(patternId)
{
    m_firePressed = false;
}

// ELF 0x1DE940 (D1; D2 0x1DE900, D0 0x1DE980): base destructors only.
BAPARevive::~BAPARevive()
{
}

// ELF 0x1DE9D0: requests the crouching pose (Bot +0x194 requestSoldierPose(2, true)). Once the
// bot's equipment is crouching, alternate executions press fire (written inline: the value 1 when
// the fire input is mapped) and release it through io::Input::setInput(PIFire, 0) with the second
// flag set. Always sets the first flag and returns true. The equipment information is not
// null-checked, as compiled.
bool BAPARevive::execute(Bot* bot, bool& ok, bool& finished)
{
    bot->requestSoldierPose(2, true);
    if (bapinline::info(bot->getCurrentEquipment())->getSoldierPose() == SPCrouching)
    {
        if (!m_firePressed)
        {
            io::PlayerInput* const input = bot->getInput();
            if ((input->mappingFlags >> io::PIFire) & 1ULL)
                input->inputs[io::PIFire] = 1.0f;
            m_firePressed = true;
        }
        else
        {
            finished = true;
            bot->getInput()->setInput(io::PIFire, 0.0f);
            m_firePressed = false;
        }
    }
    ok = true;
    return true;
}

// ELF 0x1DEAE0
BAPStatementType BAPARevive::getId() const
{
    return BAPSTRevive;   // 44
}

// ELF 0x1DEAF0
bool BAPARevive::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
