// [INFERRED] Source placement.

#include "CommunicationAgent.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1C54D0, __tcf_0 0x1C5530, _GLOBAL__I 0x1C5560).
#include <iostream>

#include <cstring>

#include "BotMain.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../../../../IO/Input/PlayerInput.h"

#include "../Main/IBotManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../Main/CommunicationHub.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"          // IAIPlayer::getIsAIPlayer
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"   // getPlayerFromId
#include "../../../AIInterface/Module/BFConsole.h"   // IBFModuleCourier, AIRadioMessageSettings

namespace dice {
namespace hfe {
namespace ai {

// Caches voice timing settings for the eighteen radio message types.
CommunicationAgent::CommunicationAgent(BotMain* owner)
    : m_bot(owner)
{
    // eighteen. See the header block.
    m_receivedMessages.resize(18);
    m_receiveTimeToLive.resize(18);
    m_maxSendPeriod.resize(18);

    IBFModuleCourier* const courier =
        static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier);

    for (unsigned int type = 0; type <= 17; ++type)
    {
        int voice;
        if (courier->getVoiceFromAiMessageType(static_cast<AIRadioMessageType>(type), voice))
        {
            AIRadioMessageSettings settings;
            courier->getAIMessageSettings(voice, settings);
            // Crossed, and in this order - see the block above.
            m_receiveTimeToLive[type] = settings.m_receiveTimeToLive;
            m_maxSendPeriod[type]     = settings.m_maxSendPeriod;
        }
        else
        {
            m_receiveTimeToLive[type] = 0.0f;
            m_maxSendPeriod[type]     = 0.0f;
        }
    }

    reset();
}

CommunicationAgent::~CommunicationAgent()
{
}

// Resets message state and acquires the shared communication hub.
void CommunicationAgent::reset()
{
    // The shared hub, re-read on every reset. See the block above.
    m_hub = CommunicationHub::m_instance;

    m_lastSentTime.resize(0);
    m_lastSentTime.resize(18);

    for (unsigned int type = 0; type <= 17; ++type)
        m_receivedMessages[type].clear();

    m_map20.clear();

    // No clear on a priority_queue - see the block above.
    while (!m_queue.empty())
        m_queue.pop();

    m_lastPollTime = 0.0;
    m_int92 = -1;
    // This order is the binary's.
    m_hornTime = 0.0;
    m_double96 = 0.0;
}

// ELF 0x1C4100: old messages are aged out every tick; the radio is polled once a second
// (a game time that went backwards restarts the interval).
void CommunicationAgent::update()
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    clearOutOldMessages(now);
    if (now < m_lastPollTime)
        m_lastPollTime = now;
    if (now - m_lastPollTime > 1.0)
    {
        m_lastPollTime = now;
        pollForRadioMessage(now);
    }
}

// ELF 0x1C4190: the agent's own messages are queued first, then every due message is sent.
void CommunicationAgent::pollForRadioMessage(double now)
{
    sendCommunicationAgentMessages(now);
    CAPrioMessage message;
    while (peekRadioMessage(message) && !waitWithSendingMessage(message, now))
    {
        postEvent(message);
        dequeueRadioMessage(message);
    }
}

// ELF 0x1C4DD0 (396 bytes), all eighteen types: an expired send block is cleared, and the
// inbox (newest first) is cut at the first message older than the type's time to live.
void CommunicationAgent::clearOutOldMessages(double now)
{
    for (unsigned int type = 0; type < 18; ++type)
    {
        if (m_lastSentTime[type] < now)
            m_lastSentTime[type] = 0.0;
        std::list<AIRadioMessageInfo>& inbox = m_receivedMessages[type];
        std::list<AIRadioMessageInfo>::iterator it = inbox.begin();
        while (it != inbox.end() && now < it->m_time + m_receiveTimeToLive[type])
            ++it;
        while (it != inbox.end())
            it = inbox.erase(it);
    }
}

// ELF 0x1C4F60: both checks use the bot's current equipment.
void CommunicationAgent::sendCommunicationAgentMessages(double)
{
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    sendCallForMedic(equipment);
    shutOffHorn(equipment);
}

// ELF 0x1C4FE0 (552 bytes). A wounded primary object (0 < health < 0.5 for strength type 0
// else 0.75) calls for a medic - type 3 on channel 0 for infantry, type 4 on channel 6
// otherwise - unless the bot is airborne, rides a fast (speed > 3) vehicle-flagged primary, or
// carries a weapon of type 1 or 3 with strength against its own type. The primary object's
// mobile plug-in is read before its null check (no guard in the ELF).
void CommunicationAgent::sendCallForMedic(Information* equipment)
{
    Information* const primary =
        informationManager->getInformation(equipment->getPrimaryObject(m_bot->getSide()));
    IPIMobile* const mobile = primary->getMobile();
    bool skip = false;
    if (m_bot->isAirBorn()
        || (primary && (primary->getObjectTypeFlags() & 0x10) != 0 && mobile
            && mobile->getSpeedMagnitude() > 3.0f))
        skip = true;
    if (!equipment->isPrimary() || skip)
        return;
    IPIPhysical* const physical = equipment->getPhysical();
    const float health = physical->getHealth();
    const int strengthType = physical->getStrengthType();
    const float threshold = strengthType == 0 ? 0.5f : 0.75f;
    if (!(0.0f < health && health < threshold))
        return;
    if (IPIArmament* const armament = equipment->getArmament())
    {
        const std::vector<unsigned int>& weapons = armament->getWeapons();
        for (unsigned int i = 0; i < weapons.size(); ++i)
        {
            WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(weapons[i]);
            if (weapon)
            {
                const int type = weapon->getWeaponType();
                if ((type == 1 || type == 3) && static_cast<float>(weapon->getStrength(strengthType)) > 0.0f)
                    return;
            }
        }
    }
    if (strengthType == 0)
        enqueueRadioMessage(static_cast<AIRadioMessageType>(3), static_cast<AIRadioChannelType>(0), 5, 0.0f, -1);
    else
        enqueueRadioMessage(static_cast<AIRadioMessageType>(4), static_cast<AIRadioChannelType>(6), 5, 0.0f, 0);
}

// ELF 0x1C5460: the side (Bot +0xE0) is read first, then the information's object (+0x20) and the
// player (Bot +0xBC); the hub is +4.
void CommunicationAgent::setSeenNewEnemy(Information* enemy)
{
    const int side = m_bot->getSide();
    IAIObject* const object = enemy->getAIObject();
    m_hub->enemySpottedByAI(m_bot->getPlayer(), object, side);
}

// ELF 0x1C44A0 (812 bytes). The record: the settings' modifier (+4), the sender, the game time,
// false, `flag`, the type (settings +0). A type-1 message drops the sender's first type-0 entry
// and everything after it; a type-0 message does the same in the type-1 list. In the message's
// own list an entry from the sender is refreshed (modifier, time, +16 false), else the record is
// appended.
void CommunicationAgent::receiveRadioMessage(const AIRadioMessageSettings& settings, int senderPlayerId, bool flag)
{
    int type;
    std::memcpy(&type, settings.m_opaque0, sizeof(type));   // +0, opaque in BFConsole.h
    AIRadioMessageInfo info;
    info.m_modifier = settings.m_modifier;
    info.m_senderPlayerId = senderPlayerId;
    info.m_time = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    info.m_flag16 = false;
    info.m_flag17 = flag;
    info.m_int20 = type;
    if (type == 1 || type == 0)
    {
        std::list<AIRadioMessageInfo>& other = m_receivedMessages[type == 1 ? 0 : 1];
        for (std::list<AIRadioMessageInfo>::iterator it = other.begin(); it != other.end(); ++it)
        {
            if (it->m_senderPlayerId == senderPlayerId)
            {
                while (it != other.end())
                    it = other.erase(it);
                break;
            }
        }
    }
    std::list<AIRadioMessageInfo>& inbox = m_receivedMessages[type];
    for (std::list<AIRadioMessageInfo>::iterator it = inbox.begin(); it != inbox.end(); ++it)
    {
        if (it->m_senderPlayerId == senderPlayerId)
        {
            it->m_modifier = settings.m_modifier;
            it->m_flag16 = false;
            it->m_time = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
            return;
        }
    }
    inbox.push_back(info);
}

// ELF 0x1C53F0: horn-capable equipment (type flag 0x04000000) releases fire after 2 s.
void CommunicationAgent::shutOffHorn(Information* equipment)
{
    if ((equipment->getObjectTypeFlags() & 0x04000000) == 0)
        return;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (now - m_hornTime > 2.0)
        m_bot->m_playerInput->setInput(io::PIFire, 0.0f);
}

// ELF 0x1C5210 (478 bytes). Nothing without a target, or when the target's player
// (Information::getPlayerId, IAIPlayerFactory +0x20) is unknown. The interval since the last horn
// (+104) must reach 20 s towards an AI player and 5 s towards a human. With horn-capable current
// equipment (type flag 0x04000000; the equipment information is not null-tested) the bot presses
// fire (PIFire 1.0); otherwise it radios type 11 on channel 6, priority 3, naming the target's
// player. The horn time is then renewed, and a target flagged 0x00200000 whose object is a bot
// (IBotManager +0x24) gets the time in its own agent (+96).
void CommunicationAgent::honkHorn(Information* target)
{
    if (!target)
        return;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const int targetPlayerId = target->getPlayerId();
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
                                  ->getPlayerFromId(target->getPlayerId());
    if (!player)
        return;
    const float interval = player->getIsAIPlayer() ? 20.0f : 5.0f;
    if (interval > now - m_hornTime)
        return;
    Information* const equipment = informationManager->getInformation(m_bot->getCurrentEquipment());
    if ((equipment->getObjectTypeFlags() & 0x04000000) != 0)
        m_bot->m_playerInput->setInput(io::PIFire, 1.0f);
    else
        sendImmediately(static_cast<AIRadioMessageType>(11), static_cast<AIRadioChannelType>(6), 3,
                        targetPlayerId);
    m_hornTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if ((target->getObjectTypeFlags() & 0x00200000) == 0)
        return;
    Bot* const horned = IBotManager::instance->getBot(target->getAIObject());
    if (!horned)
        return;
    const double time = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    horned->getCommunicationAgent()->m_double96 = time;
}

// ELF 0x1C4A50: `0.0 > now - m_lastSentTime[type]`.
bool CommunicationAgent::getSentMessage(AIRadioMessageType type) const
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    return 0.0f > now - m_lastSentTime[type];
}

// ELF 0x1C4AA0: IBFModuleCourier +0x1C for the voice, +0x18 for its settings (modifier at +4).
float CommunicationAgent::getModifierForMessageType(AIRadioMessageType type) const
{
    IBFModuleCourier* const courier = static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier);
    int voice;
    if (!courier->getVoiceFromAiMessageType(type, voice))
        return 1.0f;
    AIRadioMessageSettings settings;
    courier->getAIMessageSettings(voice, settings);
    return settings.m_modifier;
}

// ELF 0x1C4C00: the queue top, by copy.
bool CommunicationAgent::peekRadioMessage(CAPrioMessage& out) const
{
    if (m_queue.empty())
        return false;
    out = m_queue.top();
    return true;
}

// ELF 0x1C4C50: copies the top, drops its type's pending entry (+20 map) and pops it.
bool CommunicationAgent::dequeueRadioMessage(CAPrioMessage& out)
{
    if (m_queue.empty())
        return false;
    out = m_queue.top();
    m_map20.erase(m_map20.find(out.m_type));
    m_queue.pop();
    return true;
}

// ELF 0x1C4D40: wait until the send time, and while the newest received message of the type is
// younger than its max send period.
bool CommunicationAgent::waitWithSendingMessage(CAPrioMessage& message, double now)
{
    if (message.m_sendTime > now)
        return true;
    const std::list<AIRadioMessageInfo>& inbox = getReceivedMessages(message.m_type);
    if (inbox.empty())
        return false;
    const AIRadioMessageInfo& newest = inbox.front();
    return !(static_cast<double>(m_maxSendPeriod[newest.m_int20]) <= now - newest.m_time);
}

// ELF 0x1C4290 (514 bytes). Outside the type's send block (+48) one message per type is queued
// (the +20 map holds the pending type and its send time); the block is then renewed by the
// type's max send period.
void CommunicationAgent::enqueueRadioMessage(AIRadioMessageType type, AIRadioChannelType channel,
                                             unsigned int priority, float delay, int c)
{
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (!(m_lastSentTime[type] <= now))
        return;
    CAPrioMessage message;
    message.m_type = type;
    message.m_channel = channel;
    message.m_priority = static_cast<int>(priority);
    message.m_int12 = c;
    message.m_sendTime = now + static_cast<double>(delay);
    message.m_delay = delay;
    if (m_map20.insert(std::make_pair(type, message.m_sendTime)).second)
        m_queue.push(message);
    m_lastSentTime[type] = now + static_cast<double>(m_maxSendPeriod[type]);
}

// ELF 0x1C4B10. The bot's player id (twice, as bytes), side and the message's last argument
// (byte); IBFModuleCourier +0x1C getVoiceFromAiMessageType must know the type, whose voice's low
// byte goes to the hub with channel message type 3.
void CommunicationAgent::postEvent(const CAPrioMessage& message) const
{
    const unsigned char sender = static_cast<unsigned char>(m_bot->getPlayerId());
    const unsigned char player = static_cast<unsigned char>(m_bot->getPlayerId());
    const unsigned char side = static_cast<unsigned char>(m_bot->getSide());
    const unsigned char argument = static_cast<unsigned char>(message.m_int12);
    int voice;
    if (!static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier)
             ->getVoiceFromAiMessageType(message.m_type, voice))
        return;
    m_hub->filterAndSendAIRadioMessage(m_bot, sender, static_cast<ChannelMessageType>(3), message.m_channel,
                                       message.m_type, static_cast<unsigned char>(voice), player, side, argument);
}

bool CommunicationAgent::getReceivedMessage(AIRadioMessageInfo& out, AIRadioMessageType type,
                                            int senderPlayerId) const
{
    const std::list<AIRadioMessageInfo>& messages = m_receivedMessages[type];
    for (std::list<AIRadioMessageInfo>::const_iterator it = messages.begin(); it != messages.end(); ++it)
    {
        if (it->m_senderPlayerId == senderPlayerId)
        {
            out = *it;
            return true;
        }
    }
    return false;
}

bool CommunicationAgent::receivedMessageExists(AIRadioMessageType type, int senderPlayerId) const
{
    const std::list<AIRadioMessageInfo>& messages = m_receivedMessages[type];
    for (std::list<AIRadioMessageInfo>::const_iterator it = messages.begin(); it != messages.end(); ++it)
    {
        if (senderPlayerId == -1 || it->m_senderPlayerId == senderPlayerId)
            return true;
    }
    return false;
}

bool CommunicationAgent::generalConfirmReceivedMessage(AIRadioMessageType type)
{
    bool confirmed = false;
    std::list<AIRadioMessageInfo>& messages = m_receivedMessages[type];
    for (std::list<AIRadioMessageInfo>::iterator it = messages.begin(); it != messages.end(); ++it)
    {
        if (!it->m_flag16 && !it->m_flag17)
        {
            it->m_flag16 = true;
            confirmed = true;
        }
    }
    return confirmed;
}

bool CommunicationAgent::confirmReceivedMessage(AIRadioMessageType type, int senderPlayerId)
{
    bool confirmed = false;
    std::list<AIRadioMessageInfo>& messages = m_receivedMessages[type];
    for (std::list<AIRadioMessageInfo>::iterator it = messages.begin(); it != messages.end(); ++it)
    {
        if (it->m_senderPlayerId == senderPlayerId && !it->m_flag16 && !it->m_flag17)
        {
            it->m_flag16 = true;
            confirmed = true;
        }
    }
    return confirmed;
}

// The search compares type (+20), sender (+4) and the time (+8, `fucompi`, unordered is unequal).
bool CommunicationAgent::confirmReceivedMessage(const AIRadioMessageInfo& info)
{
    std::list<AIRadioMessageInfo>& messages = m_receivedMessages[info.m_int20];
    std::list<AIRadioMessageInfo>::iterator it = messages.begin();
    for (; it != messages.end(); ++it)
    {
        if (it->m_int20 == info.m_int20 && it->m_senderPlayerId == info.m_senderPlayerId
            && it->m_time == info.m_time)
            break;
    }
    if (it == messages.end() || it->m_flag16 || it->m_flag17)
        return false;
    it->m_flag16 = true;
    return true;
}

void CommunicationAgent::sendImmediately(AIRadioMessageType type, AIRadioChannelType channel,
                                         unsigned int priority, int c)
{
    CAPrioMessage message;
    message.m_type = type;
    message.m_channel = channel;
    message.m_priority = static_cast<int>(priority);
    message.m_sendTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    message.m_int12 = c;
    message.m_delay = 0.0f;
    postEvent(message);
}

} // namespace ai
} // namespace hfe
} // namespace dice
