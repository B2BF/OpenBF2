// Teleport and same-vehicle seat-change plans, recovered from libaidll.so (Linux 1.5, ia-32).

// static init: ELF 0x240ED0 (__tcf_0 0x240F30, _GLOBAL__I 0x240F60): <iostream>'s Init object only.
#include <iostream>

#include "BBPChangeTeleport.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTime.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCWait.h"
#include "../AIBotActionPlan/BAPICChangeVehicle.h"
#include "../AIBotActionPlan/BAPIWChange.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIStrategic/SAI.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIMain.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x23F960: BBPPattern(), the vtable store (GOT -0xFC0 + 8), `new bool[getMaxNBots()]` into
// +0x10, the argument at +0x08, then `new unsigned int[getMaxNBots()]` into +0x0C (both not
// initialised, each through the inlined checkPointer; operator new 0x14). The image's order.
BBPChangeTeleport::BBPChangeTeleport(int id)
{
    m_sameVehicle = checkPointer(new bool[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_patternId = id;
    m_target = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x23FBF0 (D2) / 0x23FC50 (D1) / 0x23FCB0 (D0): delete[] +0x0C, then +0x10 (each when set), then
// ~BBPPattern.
BBPChangeTeleport::~BBPChangeTeleport()
{
    if (m_target)
        delete[] m_target;
    if (m_sameVehicle)
        delete[] m_sameVehicle;
}

// ELF 0x23FD90
BotScript* BBPChangeTeleport::createPlan(Bot* bot)
{
    const unsigned int botId = bot->getBotId();
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
    const bool otherPattern = !bot->getPlan()
        || static_cast<BAPStatement*>(bot->getPlan())->getPatternId() != m_patternId;
    const bool otherTarget = otherPattern
        || static_cast<BAPIWChange*>(bot->getPlan())->getTarget() != m_target[botId];
    if (!otherTarget)
        return bot->getPlan();

    bot->getSquadMember()->clearRequestAction();
    Information* const current = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_target[bot->getBotId()]);
    IPIUnit* const targetUnit = target->getUnit();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    if (bot->getPlan()) {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    bot->getScriptContext()->clear();
    if (!current)
        return 0;

    bot->setChangeTarget(m_target[bot->getBotId()]);
    BAPCombinerParallel* plan;
    if (m_sameVehicle[bot->getBotId()]) {
        plan = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        plan->addStatement(checkPointer(new BAPICChangeVehicle(m_target[bot->getBotId()])));
        const unsigned long long controls = 1ULL << targetUnit->getSelectKey();
        BAPConFalse* const stop = checkPointer(new BAPConFalse());
        BAPConTrue* const start = checkPointer(new BAPConTrue());
        plan->addStatement(checkPointer(new BAPATrigger(targetUnit->getSelectKey(), start, stop, controls,
                                                       m_patternId, 0.0, false)));
    } else {
        const unsigned int teleporter = bot->getBotId();
        IAIMain::m_instance->getSAI(bot->getSide())->registerTeleporter(teleporter);
        plan = checkPointer(new BAPCombinerParallel(m_patternId, -1, false));
        plan->addStatement(checkPointer(new BAPICChangeVehicle(m_target[bot->getBotId()])));
        plan->addStatement(checkPointer(new BAPFlowCWait(checkPointer(new BAPConTime(2.0, false, false, false)),
                                                        m_patternId)));
    }
    const int playerId = bot->getPlayerId();
    BAPIWChange* const wrapper = checkPointer(new BAPIWChange(m_target[bot->getBotId()], plan, m_patternId, playerId));
    wrapper->m_owner = this;
    return wrapper;
}

// ELF 0x23FD20
void BBPChangeTeleport::init(unsigned int target, bool sameVehicle, Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_sameVehicle[bot->getBotId()] = sameVehicle;
    m_target[bot->getBotId()] = target;
}

} // namespace ai
} // namespace hfe
} // namespace dice
