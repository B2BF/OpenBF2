// Idle behaviour (AI/AIDLL/AI/AIBotBehaviour/BBIdle.cpp in the image). Recovered from libaidll.so
// (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address. Package D.
// Radio message and channel numbers are kept as the image's integers: the enumerator names of
// AIRadioMessageType/AIRadioChannelType are not recoverable from either binary ([UNRESOLVED]).

// static init: ELF 0x215C30 (__tcf_0 0x215C90, _GLOBAL__I 0x215CC0): <iostream>'s Init object only.
#include <iostream>

#include "BBIdle.h"

#include "BBPendingCalls.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBotBehaviourPattern/BBPIdle.h"
#include "../AIBotBehaviourPattern/BBPIdle2d.h"
#include "../AIBotBehaviourPattern/BBPIdle3d.h"
#include "../AIBotBehaviourPattern/BBPIdleInfantery.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/AIDllFrontier.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IBotManager.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Math/Mat4.h"

#include <list>

namespace dice {
namespace hfe {
namespace ai {

namespace {

IAISettings* settings()
{
    return static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
}

// Object-type tests use Information::isOfType (InformationType bits; AIDLL_r enum switch 0x100D3010):
// ITAir and ITHelicopter select the larger pickup radius; ITSoldier marks a soldier (a requester on
// foot, whose "vehicle" is the soldier itself); the bot's own equipment must not be one to give a lift.

// The sender id that means "anyone" in a pickup request; a real one makes the request personal.
const int kAnySender = 0xFF;

} // namespace

// ELF 0x214C10
BBIdle::BBIdle(unsigned char index, unsigned char queue, BBPIdle* pattern, UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, settings()->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;

    const int bots = settings()->getMaxNBots();
    m_waitStartTime = new double[bots];
    m_waitingForPickup = new bool[bots];
    m_healthAtWaitStart = new float[bots];
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_waitStartTime[i] = 0.0;
        m_waitingForPickup[i] = false;
        m_healthAtWaitStart[i] = 0.0f;
    }
    m_targets = 0;
}

// ELF 0x214D90
BBIdle::~BBIdle()
{
    if (m_waitStartTime)
        delete[] m_waitStartTime;
    if (m_waitingForPickup)
        delete[] m_waitingForPickup;
    if (m_healthAtWaitStart)
        delete[] m_healthAtWaitStart;
}

// ELF 0x215BA0. Retail quirk: it resets every bot slot, not only the bot it is given, and it does
// not call BotBehaviour::reset.
void BBIdle::reset(Bot*)
{
    const int bots = settings()->getMaxNBots();
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 1.0f;
        m_targetChanged[i] = false;
        m_waitStartTime[i] = 0.0;
        m_waitingForPickup[i] = false;
        m_healthAtWaitStart[i] = 0.0f;
    }
    m_targets = 0;
}

// ELF 0x214E90
float BBIdle::calculateUrgency(Bot* bot, bool, float modifier)
{
    BBPIdle* const pattern = static_cast<BBPIdle*>(m_pattern);
    const unsigned int equipment = bot->getCurrentEquipment();
    Information* const info = informationManager->getInformation(equipment);
    info->getPrimaryObject(bot->getSide());   // 0x214F1D: called, result unused
    IPIMobile* const mobile = info->getMobile();

    bool active = false;
    float urgency = modifier;
    int pickupPlayerId = -1;

    if (info->isPrimary() && !info->isOfType(ITSoldier) && mobile)
    {
        bool moving = false;
        if (bot->isAirBorn())
            moving = true;
        else if (info->isOfType(ITAir) && mobile->getSpeedMagnitude() > 3.0f)
            moving = true;

        IPIPhysical* const physical = info->getPhysical();
        bool damaged = false;
        if (physical && physical->getHealth() < 0.9f)
            damaged = true;

        bool waitForRepairs = false;
        if (!moving && damaged)
        {
            BotMain* const main = static_cast<BotMain*>(bot);
            if (main->getIsRepaired() || main->getIsEngineerTarget())
                waitForRepairs = true;
        }
        static_cast<BotMain*>(bot)->setShouldWaitForRepairs(waitForRepairs);

        if (!waitForRepairs)
        {
            // 0x215581: a repair that ended with a real gain is acknowledged once.
            if (m_healthAtWaitStart[bot->getBotId()] > 0.0f && physical)
            {
                const float gained = physical->getHealth() - m_healthAtWaitStart[bot->getBotId()];
                if (gained > 0.25f)
                {
                    bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(8),
                                                                      static_cast<AIRadioChannelType>(6), 3, 1.0f,
                                                                      -1);
                    m_healthAtWaitStart[bot->getBotId()] = 0.0f;
                }
            }
        }

        const std::list<AIRadioMessageInfo>& messages =
            bot->getCommunicationAgent()->getReceivedMessages(static_cast<AIRadioMessageType>(11));
        bool hadMessages = false;
        if (waitForRepairs)
        {
            double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            if (m_waitingForPickup[bot->getBotId()] == true || m_waitStartTime[bot->getBotId()] == 0.0)
            {
                m_waitStartTime[bot->getBotId()] = now;
                m_healthAtWaitStart[bot->getBotId()] = physical->getHealth();
            }
            now = now - m_waitStartTime[bot->getBotId()];
            if (now > 3.0)
                static_cast<BotMain*>(bot)->setIsRepaired(false);
            if (!messages.empty())
                hadMessages = true;
            m_waitingForPickup[bot->getBotId()] = false;
            urgency = modifier * 60.0f;
            active = true;
        }
        else
        {
            waitForPickup(bot, equipment, info, pattern, moving, modifier, urgency, pickupPlayerId, active);
        }

        // 0x21515F: answer the senders of message 11 that are in the bot's own vehicle.
        if (!messages.empty()
            && bbpending::generalConfirmReceivedMessage(bot->getCommunicationAgent(),
                                                        static_cast<AIRadioMessageType>(11))
            && pickupPlayerId != kAnySender)
        {
            for (std::list<AIRadioMessageInfo>::const_iterator it = messages.begin(); it != messages.end(); ++it)
            {
                IAIPlayer* const player =
                    static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                        ->getPlayerFromId(it->m_senderPlayerId);
                if (!player)
                    continue;
                IAIObject* const vehicle = player->getCurrentVehicle();
                if (!vehicle)
                    continue;
                Information* const vehicleInfo = informationManager->getInformation(vehicle->getHandle(bot->getSide()));
                if (!vehicleInfo)
                    continue;
                Information* const ownPrimary =
                    informationManager->getInformation(info->getPrimaryObject(bot->getSide()));
                Information* const senderPrimary =
                    informationManager->getInformation(vehicleInfo->getPrimaryObject(bot->getSide()));
                if (senderPrimary != ownPrimary)
                    continue;
                if (!hadMessages)
                {
                    bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(6),
                                                                      static_cast<AIRadioChannelType>(7), 3, 1.0f,
                                                                      -1);
                    active = false;
                }
                else
                {
                    bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(7),
                                                                      static_cast<AIRadioChannelType>(7), 3, 1.0f,
                                                                      -1);
                }
            }
        }
    }

    // 0x215170
    if (!active)
    {
        static_cast<BotMain*>(bot)->setIsRepaired(false);
        static_cast<BotMain*>(bot)->setShouldWaitForRepairs(false);
        m_waitStartTime[bot->getBotId()] = 0.0;
        m_waitingForPickup[bot->getBotId()] = false;
        m_healthAtWaitStart[bot->getBotId()] = 0.0f;
        urgency = modifier;
    }

    m_urgency[bot->getBotId()] = urgency;
    if (pattern->getType() == 1)
        static_cast<BBPIdle2d*>(pattern)->init(bot, pickupPlayerId);
    else if (pattern->getType() == 2)
        static_cast<BBPIdle3d*>(pattern)->init(50.0f, bot, pickupPlayerId);
    else if (pattern->getType() == 3)
        static_cast<BBPIdleInfantery*>(pattern)->init(bot);
    return urgency;
}

// ELF 0x2156C0. Walks the bot's received pickup offers (message 2) and accepts the first that
// fits: a carrier vehicle close enough to the bot's own (primary) object.
void BBIdle::waitForPickup(Bot* bot, unsigned int equipment, Information* equipmentInfo, BBPIdle* pattern,
                           bool moving, float modifier, float& urgency, int& pickupPlayerId, bool& active)
{
    const std::list<AIRadioMessageInfo>& messages =
        bot->getCommunicationAgent()->getReceivedMessages(static_cast<AIRadioMessageType>(2));
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();

    for (std::list<AIRadioMessageInfo>::const_iterator it = messages.begin(); it != messages.end(); ++it)
    {
        Bot* const sender = IBotManager::instance->getBotFromPlayerId(it->m_senderPlayerId);
        Information* const primary =
            informationManager->getInformation(equipmentInfo->getPrimaryObject(bot->getSide()));
        const int primaryPlayerId = primary->getAIObject()->getPlayerId();
        CommunicationAgent* const senderAgent = sender ? sender->getCommunicationAgent() : 0;
        const bool fromAnyone = it->m_senderPlayerId == kAnySender;

        if (pickupPlayerId >= 0 && !fromAnyone)
            continue;
        if (equipmentInfo->isOfType(ITSoldier))
            continue;
        // 0x21584E: `cmpl 0x5C(agent), primaryPlayerId` (CommunicationAgent +0x5C).
        if (senderAgent && bbpending::communicationAgentInt92(senderAgent) != primaryPlayerId)
            continue;

        IAIPlayer* const player =
            static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(it->m_senderPlayerId);
        if (!player)
            continue;
        IAIObject* const vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* const vehicleInfo = informationManager->getInformation(vehicle->getHandle(bot->getSide()));
        if (!vehicleInfo)
            continue;
        if (!vehicleInfo->isOfType(ITSoldier))
            continue;

        float maxDistanceSqr;
        if (equipmentInfo->isOfType(ITHelicopter) || equipmentInfo->isOfType(ITAir) || fromAnyone)
            maxDistanceSqr = 2500.0f;
        else
            maxDistanceSqr = 625.0f;

        // Information slot +0x3C (getTransformation); the translation row is m[12..14].
        const Mat4& own = primary->getTransformation();
        const Mat4& carrier = vehicleInfo->getTransformation();
        Vec3 delta;
        delta.x = own.m[12] - carrier.m[12];
        delta.y = own.m[13] - carrier.m[13];
        delta.z = own.m[14] - carrier.m[14];
        const float distanceSqr = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;

        bool decline;
        if (fromAnyone)
            decline = !pattern->freePosition(equipment, bot->getSide()) || distanceSqr > maxDistanceSqr || moving;
        else
            decline = distanceSqr > maxDistanceSqr || moving;
        if (decline)
        {
            // 0x21598F
            if (!bbpending::confirmReceivedMessage(bot->getCommunicationAgent(), *it))
                continue;
            bot->getCommunicationAgent()->enqueueRadioMessage(static_cast<AIRadioMessageType>(7),
                                                              static_cast<AIRadioChannelType>(6), 3, 1.0f, -1);
            return;
        }

        // 0x215A00
        if (!m_waitingForPickup[bot->getBotId()] || m_waitStartTime[bot->getBotId()] == 0.0)
            m_waitStartTime[bot->getBotId()] = now;
        const double waited = now - m_waitStartTime[bot->getBotId()];
        if (!vehicleInfo->isOfType(ITSoldier) || primary != equipmentInfo)
            continue;
        if (!equipmentInfo->getMobile() || equipmentInfo->isOfType(ITSoldier) || moving)
            continue;
        if (!(waited < 10.0))
            continue;
        if (pickupPlayerId >= 0 && !fromAnyone)
            continue;

        urgency = modifier * it->m_modifier;
        active = true;
        pickupPlayerId = it->m_senderPlayerId;
        m_waitingForPickup[bot->getBotId()] = true;
        if (pickupPlayerId == kAnySender)
            return;
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
