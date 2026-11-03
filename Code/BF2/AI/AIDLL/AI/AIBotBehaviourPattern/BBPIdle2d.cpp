// Idle pattern of ground vehicles (AI/AIDLL/AI/AIBotBehaviourPattern/BBPIdle2d.cpp in the image).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x28D950 (__tcf_0 0x28D9B0, _GLOBAL__I 0x28D9E0): <iostream>'s Init object only.
#include <iostream>

#include "BBPIdle2d.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotActionPlan/BAPAResetControls.h"
#include "../AIBotActionPlan/BAPASendRadioMessage.h"
#include "../AIBotActionPlan/BAPAUseFlareOnMissileLock.h"
#include "../AIBotActionPlan/BAPCombinerParallel.h"
#include "../AIBotActionPlan/BAPCombinerSerial.h"
#include "../AIBotActionPlan/BAPConTrue.h"
#include "../AIBotActionPlan/BAPFlowCWhile.h"
#include "../AIBotActionPlan/BAPIWIdle.h"
#include "../AIBotActionPlan/BAPStatement.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIPlanInterpreter/BAPContext.h"
#include "../AIPlanInterpreter/BotScriptDebugger.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Math/Mat4.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAISettings* settings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

} // namespace

// ELF 0x28C880
BBPIdle2d::BBPIdle2d(int id)
    : BBPIdle(id)
{
    m_pickupPlayerId = checkPointer(new int[settings()->getMaxNBots()]);
    for (int i = 0; i < settings()->getMaxNBots(); ++i)
        m_pickupPlayerId[i] = -1;
}

// ELF 0x28CA70
BBPIdle2d::~BBPIdle2d()
{
    if (m_pickupPlayerId)
        delete[] m_pickupPlayerId;
}

// ELF 0x28CB60 (listing). The plan is `parallel(use flares on missile lock, serial(...))` inside a
// BAPIWIdle at the equipment's position; the serial's first statement is `while (true) reset
// controls`. A vehicle waiting for a passenger (a pickup player id >= 0 on equipment whose primary
// object lacks the 0x200000 type flag) first queues a pickup radio message (BAPASendRadioMessage
// 6/6/2, 1.0) when the passenger's vehicle is within 40 m (1600 squared) of the primary object, the
// position is free (not asked for id 255), the equipment is primary and the passenger's request
// (type 2) is confirmed. An existing plan of this pattern is kept; for a waiting vehicle it also
// re-enqueues the radio message (6/6/3, 1.0) after a confirmed request. A plan of another pattern
// that hasPlan reports is not deleted here, as compiled. Builds the plan and clears the
// navigation progress.
BotScript* BBPIdle2d::createPlan(Bot* bot)
{
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

    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->hasPlan())
    {
        if (static_cast<BAPStatement*>(bot->getPlan())->getPatternId() == m_patternId)
        {
            const int pickupPlayerId = m_pickupPlayerId[bot->getBotId()];
            if (pickupPlayerId >= 0 && !(equipment->getObjectTypeFlags() & 0x200000)
                && bot->getCommunicationAgent()->confirmReceivedMessage(static_cast<AIRadioMessageType>(2),
                                                                        pickupPlayerId))
            {
                bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(6),
                                                                  static_cast<AIRadioChannelType>(6), 3, 1.0f, -1);
            }
            return bot->getPlan();
        }
    }
    else if (bot->getPlan())
    {
        delete bot->getPlan();
        bot->setPlan(0);
    }

    if (bot->getDebugger())
        static_cast<BotScriptDebugger*>(bot->getDebugger())->clear();
    bot->getScriptContext()->clear();
    bot->getSquadMember()->clearRequestAction();

    BAPCombinerSerial* const serial = checkPointer(new BAPCombinerSerial(m_patternId, -1));
    Information* const primary = informationManager->getInformation(equipment->getPrimaryObject(bot->getSide()));
    const int pickupPlayerId = m_pickupPlayerId[bot->getBotId()];
    const bool anyPosition = pickupPlayerId == 0xFF;
    if (pickupPlayerId >= 0 && !(primary->getObjectTypeFlags() & 0x200000))
    {
        IAIPlayer* const passenger =
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(pickupPlayerId);
        IAIObject* const passengerVehicle = passenger ? passenger->getCurrentVehicle() : 0;   // +0x54
        if (passengerVehicle)
        {
            const unsigned int equipmentHandle = bot->getCurrentEquipment();
            Information* const passengerInfo =
                informationManager->getInformation(passengerVehicle->getHandle(bot->getSide()));   // +0xD0
            if (passengerInfo)
            {
                const Mat4& own = primary->getTransformation();                                   // +0x3C
                const Mat4& other = passengerInfo->getTransformation();
                const float dx = own.m[12] - other.m[12];
                const float dy = own.m[13] - other.m[13];
                const float dz = own.m[14] - other.m[14];
                const float distanceSqr = dx * dx + dy * dy + dz * dz;
                if ((anyPosition || freePosition(equipmentHandle, bot->getSide()))
                    && 1600.0f > distanceSqr
                    && equipment->isPrimary()
                    && bot->getCommunicationAgent()->confirmReceivedMessage(static_cast<AIRadioMessageType>(2),
                                                                            pickupPlayerId))
                {
                    BAPASendRadioMessage* const message = checkPointer(new BAPASendRadioMessage(
                        static_cast<AIRadioMessageType>(6), static_cast<AIRadioChannelType>(6), 2, 1.0f,
                        m_patternId, -1));
                    serial->addStatement(message);
                }
            }
        }
    }

    BAPFlowCWhile* const loop = checkPointer(new BAPFlowCWhile(
        checkPointer(new BAPConTrue()), checkPointer(new BAPAResetControls(true, true, true, m_patternId)),
        m_patternId));
    serial->addStatement(loop);
    BAPCombinerParallel* const parallel = checkPointer(new BAPCombinerParallel(m_patternId, true, -1, false));
    BAPAUseFlareOnMissileLock* const flares =
        checkPointer(new BAPAUseFlareOnMissileLock(bot, 0x800000000000ULL, m_patternId));
    parallel->addStatement(flares);
    parallel->addStatement(serial);
    BAPIWIdle* const idle = checkPointer(new BAPIWIdle(equipment->getPosition(), parallel, m_patternId, bot));
    idle->setParent(this);   // +0x04 (BotScript::m_owner)
    bot->getBotNavigation()->clearProgress();
    return idle;
}

// ELF 0x28DA00
int BBPIdle2d::getType()
{
    return 1;
}

// ELF 0x28CB10
void BBPIdle2d::init(Bot* bot, int pickupPlayerId)
{
    m_initiated[bot->getBotId()] = true;
    m_pickupPlayerId[bot->getBotId()] = pickupPlayerId;
}

} // namespace ai
} // namespace hfe
} // namespace dice
