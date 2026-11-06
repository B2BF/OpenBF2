// Seat-change pattern for landing craft (AI/AIDLL/AI/AIBotBehaviourPattern/BBPChangeLandingCraft.cpp in
// the image). Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its
// ELF address. createPlan was checked against the listing for the trigger arguments.

// static init: ELF 0x23F620 (__tcf_0 0x23F680, _GLOBAL__I 0x23F6B0): <iostream>'s Init object only.
#include <iostream>

#include "BBPChangeLandingCraft.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPATrigger.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConFalse.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPICChangeVehicle.h"
#include "../AIBotActionPlan/BAPIWChange.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x23DFD0: BBPPattern(), the vtable store (GOT -0x280 + 8), `new bool[getMaxNBots()]` into
// +0x10, the argument at +0x08, then `new unsigned int[getMaxNBots()]` into +0x0C (both not
// initialised, each through the inlined checkPointer; operator new 0x14). The image's order.
BBPChangeLandingCraft::BBPChangeLandingCraft(int id)
{
    m_sameVehicle = checkPointer(new bool[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
    m_patternId = id;
    m_target = checkPointer(new unsigned int[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]);
}

// ELF 0x23E260 (D2) / 0x23E2C0 (D1) / 0x23E320 (D0): delete[] +0x0C, then +0x10 (each when set), then
// ~BBPPattern.
BBPChangeLandingCraft::~BBPChangeLandingCraft()
{
    if (m_target)
        delete[] m_target;
    if (m_sameVehicle)
        delete[] m_sameVehicle;
}

// ELF 0x23E400. Keeps a BAPIWChange of this pattern for the same target; otherwise drops the plan
// and builds `IWChange(target, serial(trigger, change vehicle))`: the trigger presses the target
// seat's select key when init named a seat of the same vehicle, else enter/exit (input 10, controls
// 0x400). The target's information is not null-checked before its unit is read; without own
// equipment information nothing is built.
BotScript* BBPChangeLandingCraft::createPlan(Bot* bot)
{
    const unsigned int botId = bot->getBotId();
    if (!m_initiated[bot->getBotId()])
    {
        if (bot->getDebugger())
            static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
        bot->getScriptContext()->clear();
        if (bot->getPlan())
        {
            delete bot->getPlan();
            bot->setPlan(0);
        }
        return 0;
    }

    bool otherPattern = false;
    if (!bot->getPlan() || static_cast<BAPStatement*>(bot->getPlan())->getPatternId() != m_patternId)
        otherPattern = true;
    bool otherTarget = false;
    if (otherPattern || static_cast<BAPIWChange*>(bot->getPlan())->getTarget() != m_target[botId])
        otherTarget = true;
    if (!otherTarget)
        return bot->getPlan();

    bot->getSquadMember()->clearRequestAction();
    Information* const current = informationManager->getInformation(bot->getCurrentEquipment());
    Information* const target = informationManager->getInformation(m_target[bot->getBotId()]);
    IPIUnit* const targetUnit = target->getUnit();
    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }
    bot->getScriptContext()->clear();
    if (!current)
        return 0;

    BAPCombinerSerial* const sequence = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    if (m_sameVehicle[bot->getBotId()])
    {
        const int patternId = m_patternId;
        const unsigned long long controls = 1ULL << targetUnit->getSelectKey();   // 64-bit shift (0x23E618)
        BAPConFalse* const stop = checkPointer(new BAPConFalse());
        BAPConTrue* const start = checkPointer(new BAPConTrue());
        sequence->addStatement(checkPointer(new BAPATrigger(targetUnit->getSelectKey(), start, stop, controls,
                                                            patternId, 0.0, false)));
    }
    else
    {
        const int patternId = m_patternId;
        BAPConFalse* const stop = checkPointer(new BAPConFalse());
        BAPConTrue* const start = checkPointer(new BAPConTrue());
        sequence->addStatement(checkPointer(new BAPATrigger(static_cast<io::PlayerInputMap>(10), start, stop,
                                                            0x400ULL, patternId, 0.0, false)));
    }
    sequence->addStatement(checkPointer(new BAPICChangeVehicle(m_target[bot->getBotId()])));
    const int playerId = bot->getPlayerId();
    BAPIWChange* const wrapper =
        checkPointer(new BAPIWChange(m_target[bot->getBotId()], sequence, m_patternId, playerId));
    wrapper->setParent(this);   // +0x04 (BotScript::mpOwner)
    return wrapper;
}

// ELF 0x23E390: three getBotId calls, one per store.
void BBPChangeLandingCraft::init(unsigned int target, bool sameVehicle, Bot* bot)
{
    m_initiated[bot->getBotId()] = true;
    m_sameVehicle[bot->getBotId()] = sameVehicle;
    m_target[bot->getBotId()] = target;
}

} // namespace ai
} // namespace hfe
} // namespace dice
