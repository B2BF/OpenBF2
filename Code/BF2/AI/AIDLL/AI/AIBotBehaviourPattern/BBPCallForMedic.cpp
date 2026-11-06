// Medic-request radio pattern, recovered from libaidll.so (Linux 1.5, ia-32).

// static init: ELF 0x235730 (__tcf_0 0x235790, _GLOBAL__I 0x2357C0): <iostream>'s Init object only.
#include <iostream>

#include "BBPCallForMedic.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPASendRadioMessage.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2352E0 (C2), 0x235320 (C1).
BBPCallForMedic::BBPCallForMedic(int patternId)
    : m_patternId(patternId)
{
}

// ELF 0x235360 (D2), 0x2353A0 (D1), 0x2353E0 (D0).
BBPCallForMedic::~BBPCallForMedic()
{
}

// ELF 0x235450
BotScript* BBPCallForMedic::createPlan(Bot* bot)
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
    bot->getSquadMember()->clearRequestAction();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    BAPASendRadioMessage* const plan = checkPointer(new BAPASendRadioMessage(static_cast<AIRadioMessageType>(3),
        static_cast<AIRadioChannelType>(0), 5, 0.0f, m_patternId, -1));
    plan->mpOwner = this;
    return plan;
}

// ELF 0x235420
void BBPCallForMedic::init(Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
