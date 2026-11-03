// Suicide input pattern, recovered from libaidll.so (Linux 1.5, ia-32).

// static init: ELF 0x29D240 (__tcf_0 0x29D2A0, _GLOBAL__I 0x29D2D0): <iostream>'s Init object only.
#include <iostream>

#include "BBPSuicide.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29CB90 (C2), 0x29CBD0 (C1).
BBPSuicide::BBPSuicide(int patternId)
    : BBPPattern(), m_patternId(patternId)
{
}

// ELF 0x29CC10 (D2), 0x29CC50 (D1), 0x29CC90 (D0).
BBPSuicide::~BBPSuicide()
{
}

// ELF 0x29CD00
BotScript* BBPSuicide::createPlan(Bot* bot)
{
    if (!m_initiated[bot->getBotId()]) {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan()) {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    bot->getSquadMember()->clearRequestAction();
    bot->getCurrentEquipment();
    if (!bot->hasPlan()) {
        if (bot->getPlan()) {
            delete bot->getPlan();
            bot->setPlan(0);
        }
    } else if (static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId) {
        return bot->getPlan();
    }
    // shipped defect: a valid plan of another pattern is not deleted here.
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getSquadMember()->clearRequestAction();
    bot->getScriptContext()->clear();

    BAPConFalse* const stop = checkPointer(new BAPConFalse());
    BAPConTrue* const start = checkPointer(new BAPConTrue());
    // shipped defect: the signed input-31 mask is widened with every upper bit set.
    BAPATrigger* const plan = checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(31), start, stop,
                                                        0xFFFFFFFF80000000ULL, m_patternId, 0.0, false));
    plan->m_owner = this;
    return plan;
}

// ELF 0x29CCD0
void BBPSuicide::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
