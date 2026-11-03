// [INFERRED] Source placement.

// static init: ELF 0x3E5BD0 (__tcf_0 0x3E5C30, _GLOBAL__I 0x3E5C60): <iostream>'s Init object only.
#include <iostream>

#include "CommunicationHub.h"

#include <cmath>
#include <cstring>
#include <map>
#include <string>

#include "../AIStrategicEntity/AIStrategicObjectManager.h"     // reset: the area count
#include "../AIStrategicEntity/AIStrategicArea.h"              // enemySpottedByAI: area centres

#include "CommonSingletonServer.h"                                 // enemySpottedByHuman's timer
#include "IBotManager.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/InformationManager.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"            // IAIPlayer
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"     // IAIPlayerFactory::getPlayerFromId
#include "../../../AIInterface/BFKnowledge/BFGame.h"              // IAIGame::enemySpottedByAI
#include "../../../AIInterface/BFKnowledge/BFRadioComm.h"         // IBFRadioComm::sendAIRadioMessage
#include "../../../AIInterface/Module/BFConsole.h"                // IBFModuleCourier, AIRadioMessageSettings
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

CommunicationHub* CommunicationHub::m_instance = 0;

// Constructs two empty strength vectors and publishes only the first hub instance.
// Call reset before using the three timestamps, which construction leaves uninitialized.
CommunicationHub::CommunicationHub()
{
    // m_time4, m_time12 and m_time44 deliberately not initialised - the binary writes none of
    // them. The two vectors default-construct on their own.
    if (!m_instance)
        m_instance = this;
}

// it does test `== this` before nulling the static, unlike `AerialHeightMap` one step earlier.
// The two vectors are then torn down in reverse, which is the compiler's own work.
CommunicationHub::~CommunicationHub()
{
    if (m_instance == this)
        m_instance = 0;
}

// ELF 0x3E5170. The three timestamps go to -1000.0. Each side's strength vector gets one entry
// per strategic area: appended as {6, -1000.0} while side 0's vector is empty, otherwise the
// first areaCount entries of both are overwritten in place (sizes are not checked again).
void CommunicationHub::reset()
{
    const double never = -1000.0;
    m_type2SentTime[0] = never;
    m_type2SentTime[1] = never;
    m_time44 = never;
    const int areas = static_cast<int>(AIStrategicObjectManager::getInstance()->getStrategicAreas().size());
    if (m_combatStrength[0].empty())
    {
        for (int i = 0; i < areas; ++i)
        {
            m_combatStrength[0].push_back(std::make_pair(static_cast<CombatStrengthType>(6), static_cast<double>(-1000)));
            m_combatStrength[1].push_back(std::make_pair(static_cast<CombatStrengthType>(6), static_cast<double>(-1000)));
        }
        return;
    }
    for (int i = 0; i < areas; ++i)
    {
        m_combatStrength[0][i].first = static_cast<CombatStrengthType>(6);
        m_combatStrength[1][i].first = static_cast<CombatStrengthType>(6);
        m_combatStrength[0][i].second = never;
        m_combatStrength[1][i].second = never;
    }
}

// ELF 0x3E57E0 (776 bytes). The side's information about the object must be primary (Information
// ::isPrimary is called on the lookup result without a null test) and have a physical plug-in.
// The object's XZ position is tested against every strategic area's centre (AIStrategicArea
// ::getCenterPosition), within 100 m for strength types 4 and 5, else 30 m. For the first area in
// range that has news the game is told (IAIGame +0x6C) and the scan ends:
//   * types 4/5: more than 60 s since the last such report (+44, shared by both sides);
//   * the area's entry for the side is older than 60 s: it takes the type and the time;
//   * the entry holds type 0 or 1 and the enemy is type 2: it becomes 2 with the time.
// The object's world object is fetched before the bot's player, as shipped.
void CommunicationHub::enemySpottedByAI(IAIPlayer* player, IAIObject* object, int side)
{
    Information* const information = informationManager->getInformation(object->getInformation(side));
    if (!information->isPrimary())
        return;
    IPIPhysical* const physical = information->getPhysical();
    if (!physical)
        return;
    const int strengthType = physical->getStrengthType();
    const std::map<std::string, AIStrategicArea*>& areas =
        AIStrategicObjectManager::getInstance()->getStrategicAreas();
    const bool largeType = static_cast<unsigned int>(strengthType - 4) <= 1;
    const float rangeSq = largeType ? 10000.0f : 900.0f;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    std::vector<std::pair<CombatStrengthType, double> >& strengths = m_combatStrength[side - 1];

    unsigned int index = 0;
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = areas.begin(); it != areas.end();
         ++it, ++index)
    {
        Vec2 position;
        position.x = 0.0f;
        position.y = 0.0f;
        vec3ToVec2XZ(object->getPosition(), position);
        const Vec2& centre = it->second->getCenterPosition();
        const float dx = position.x - centre.x;
        const float dy = position.y - centre.y;
        if (!(rangeSq > dx * dx + dy * dy))
            continue;
        if (largeType && now - m_time44 > 60.0f)
        {
            m_time44 = now;
            break;
        }
        std::pair<CombatStrengthType, double>& entry = strengths[index];
        if (now - entry.second > 60.0f)
        {
            entry.first = static_cast<CombatStrengthType>(strengthType);
            entry.second = now;
            break;
        }
        if ((entry.first == 0 || entry.first == 1) && strengthType == 2)
        {
            strengths[index].first = static_cast<CombatStrengthType>(2);
            strengths[index].second = now;
            break;
        }
    }
    if (index == areas.size())
        return;   // no area had news: the shipped loop exit skips the report

    world::IObject* const spotted = object->getObject();
    world::IPlayer* const spotter = player->getPlayer();
    static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->enemySpottedByAI(spotter, spotted);
}

// ELF 0x3E5AF0. The side's information about the object (IAIObject +0xB8 getInformation),
// else forced into existence (+0xB4 forceGetInformation(side, true)), is stamped with the game
// time (Information +0xC setTime). Neither lookup result is tested before the stamp.
void CommunicationHub::enemySpottedByHuman(IAIObject* object, unsigned char side) const
{
    Information* information = informationManager->getInformation(object->getInformation(side));
    if (!information)
        information = informationManager->getInformation(object->forceGetInformation(side, true));
    information->setTime(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime());
}

// ELF 0x3E5330 (395 bytes). The game time is read first in every case. A type-2 message is sent
// only when more than 20 s have passed since the side's last one, whose time is then renewed from
// a second timer read. Inside the 20 s it is not sent: when `argument` names an AI player, the
// settings of message type 2 (IBFModuleCourier +0x1C, then +0x18; the voice lookup's result is
// not tested) go straight to filterAndReceiveAIRadioMessage(settings, playerId, argument, true).
// Everything else reaches IBFRadioComm +8 without the message type; its result is discarded.
void CommunicationHub::filterAndSendAIRadioMessage(BotMain*, int senderPlayerId,
                                                   ChannelMessageType channelMessage,
                                                   AIRadioChannelType channel, AIRadioMessageType type,
                                                   unsigned char voice, int playerId, unsigned char side,
                                                   unsigned char argument)
{
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    if (type == 2)
    {
        if (!(20.0f > now - m_type2SentTime[side - 1]))
        {
            m_type2SentTime[side - 1] = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        }
        else
        {
            IAIPlayer* const receiver =
                static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getPlayerFromId(argument);
            if (!receiver || !receiver->getIsAIPlayer())
                return;
            IBFModuleCourier* const courier =
                static_cast<IBFModuleCourier*>(CommonSingletonServer::m_iBFModuleCourier);
            int typeVoice;
            courier->getVoiceFromAiMessageType(static_cast<AIRadioMessageType>(2), typeVoice);
            AIRadioMessageSettings settings;
            courier->getAIMessageSettings(typeVoice, settings);
            filterAndReceiveAIRadioMessage(settings, playerId, argument, true);
            return;
        }
    }
    static_cast<IBFRadioComm*>(CommonSingletonServer::m_iBFRadioComm)
        ->sendAIRadioMessage(senderPlayerId, channelMessage, channel, voice, playerId, side, argument);
}

// ELF 0x3E54C0 (795 bytes). The receiver is IBotManager +0x1C getBotFromPlayerId(receiver), used
// without a null test; its own messages (BotMain +0xB4 getPlayerId == sender) are dropped. A
// message of type 0, 1 or 2 (settings +0) whose sender is a human player (IAIPlayerFactory +0x20,
// IAIPlayer +0x88 false) is filtered first; an unknown or AI sender is not:
//   * type 2 is dropped when the receiver's current equipment lies more than 90 degrees off the
//     human's camera forward axis (row 2), seen from the camera position (row 3). The direction
//     is normalised as shipped: kept when |d.d - 1| < FLT_EPSILON, zero when |d.d| < FLT_EPSILON.
//     The equipment information is not null-tested;
//   * types 1 and 0 set the human's current vehicle's primary object allowed (1) or not allowed
//     (0) for bots (IAIObject +0xFC, +0x9C); the vehicle itself is not null-tested.
// Then CommunicationAgent::receiveRadioMessage(settings, sender, flag) files it.
// [CHOSEN, not recovered] x87 extended intermediates are carried in double here.
void CommunicationHub::filterAndReceiveAIRadioMessage(const AIRadioMessageSettings& settings,
                                                      int senderPlayerId, int receiverPlayerId, bool flag)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(receiverPlayerId);
    if (senderPlayerId == bot->getPlayerId())
        return;
    int type;
    std::memcpy(&type, settings.m_opaque0, sizeof(type));   // +0, opaque in BFConsole.h
    IAIPlayerFactory* const players = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
    if (type == 0 || type == 1)
    {
        IAIPlayer* const sender = players->getPlayerFromId(senderPlayerId);
        if (sender && !sender->getIsAIPlayer())
        {
            if (IAIObject* const primary = sender->getCurrentVehicle()->getPrimaryObject())
                primary->setIsAllowedByBots(type == 1);
        }
    }
    else if (type == 2)
    {
        IAIPlayer* const sender = players->getPlayerFromId(senderPlayerId);
        if (sender && !sender->getIsAIPlayer())
        {
            const Mat4& camera = *static_cast<const Mat4*>(sender->getCameraTransformation());
            Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
            const Vec3& target = equipment->getPosition();
            Vec3 direction;
            direction.x = target.x - camera.m[12];
            direction.y = target.y - camera.m[13];
            direction.z = target.z - camera.m[14];
            const double lengthSq = static_cast<double>(direction.x) * direction.x
                                  + static_cast<double>(direction.y) * direction.y
                                  + static_cast<double>(direction.z) * direction.z;
            if (!(std::fabs(lengthSq - 1.0f) < 1.1920928955078125e-07f))
            {
                if (!(std::fabs(static_cast<float>(lengthSq)) < 1.1920928955078125e-07f))
                {
                    const double scale = 1.0f / std::sqrt(lengthSq);
                    direction.x = static_cast<float>(direction.x * scale);
                    direction.y = static_cast<float>(direction.y * scale);
                    direction.z = static_cast<float>(direction.z * scale);
                }
                else
                    direction.x = direction.y = direction.z = 0.0f;
            }
            const Mat4& view = *static_cast<const Mat4*>(sender->getCameraTransformation());
            double cosine = static_cast<double>(view.m[8]) * direction.x
                          + static_cast<double>(view.m[9]) * direction.y
                          + static_cast<double>(view.m[10]) * direction.z;
            if (-1.0f > cosine)
                cosine = -1.0f;
            else if (cosine > 1.0f)
                cosine = 1.0f;
            if (std::acos(static_cast<float>(cosine)) > 1.5707963705062866f)
                return;
        }
    }
    bot->getCommunicationAgent()->receiveRadioMessage(settings, senderPlayerId, flag);
}

} // namespace ai
} // namespace hfe
} // namespace dice
