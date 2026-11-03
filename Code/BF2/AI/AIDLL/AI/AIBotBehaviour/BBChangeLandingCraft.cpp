// Landing-craft seat selection and disembark urgency, recovered from libaidll.so.

// static init: ELF 0x204EA0 (__tcf_0 0x204F00, _GLOBAL__I 0x204F30): <iostream>'s Init object only.
#include <iostream>

#include "BBChangeLandingCraft.h"

#include "CustomCurve.h"
#include "VehicleUrgencyCalculator.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIBot/SquadMember.h"
#include "../AIBotBehaviourPattern/BBPChange.h"
#include "../AIBotBehaviourPattern/BBPChangeLandingCraft.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIStrategicMap/BFStrategicEnvironment.h"
#include "../Main/IBotManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

#include <string>
#include <list>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2039C0 (C2) / 0x203A80 (C1); vtable GOT -0xDE0. The BBAvoid shape: the base gets getMaxNBots
// (IAISettings slot 0); a second getMaxNBots sizes the +0x08 target array (plain operator new[]),
// and every bot starts with urgency 0.0f (rodata 0x49CB34) and target 0xFFFFFFFF.
BBChangeLandingCraft::BBChangeLandingCraft(unsigned char index, unsigned char queue, PlanGenerator* pattern,
                                           UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = new unsigned int[count];
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = 0xFFFFFFFFu;
    }
}

// ELF 0x203B40 (D2) / 0x203B90 (D1) / 0x203BE0 (D0): delete[] +0x08 when set, then ~BotBehaviour.
BBChangeLandingCraft::~BBChangeLandingCraft()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x203DD0
float BBChangeLandingCraft::calculateUrgency(Bot* bot, bool evaluate, float)
{
    if (!evaluate)
    {
        m_urgency[bot->getBotId()] = 0.0f;
        const bool initiated = m_pattern->isInitiated(bot);
        m_targetChanged[bot->getBotId()] = initiated;
        m_pattern->unInit(bot);
        const float urgency = checkLandingCraftBookings(bot);
        m_urgency[bot->getBotId()] = urgency;
        return m_urgency[bot->getBotId()];
    }
    std::list<unsigned int> unusedObjects;
    float fireShare;
    float moveShare;
    if (bot->getMoveInclination() > 0.0f)
    {
        if (bot->getFireInclination() > 0.0f)
        {
            const float fire = bot->getFireInclination();
            const float sum = bot->getMoveInclination() + fire;
            fireShare = bot->getFireInclination() / sum;
            moveShare = bot->getMoveInclination() / sum;
        }
        else { fireShare = 0.0f; moveShare = 1.0f; }
    }
    else if (bot->getFireInclination() > 0.0f)
    { fireShare = 1.0f; moveShare = 0.0f; }
    else { fireShare = 0.5f; moveShare = 0.5f; }

    const int side = bot->getSide();
    unsigned int bestHandle = bot->getCurrentEquipment();
    Information* current = informationManager->getInformation(bestHandle);
    float orderFactor = 1.0f;
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders()
        && bot->getOrderList(m_index)->empty())
        orderFactor = bot->getMemory()->m_float8;
    IAIStrategicEnvironment* strategic = static_cast<IAIStrategicEnvironment*>(AISingletonServer::m_iaiStrategicEnvironment);
    const float* types = strategic->getEnemyTypes(bot);
    const float* strengths = strategic->getEnemyStrengths(bot);
    Information* primary = informationManager->getInformation(current->getPrimaryObject(side));
    IPIMobile* primaryMobile = primary ? primary->getMobile() : 0;
    float primaryFactor = 1.0f;
    float currentFactor = 1.0f;
    const float secondaryFactor = 0.25f;
    if (current->isPrimary())
        primary = current;
    else if (!primary->getUnit()->isOccupied() && !(current->getObjectTypeFlags() & 0x80))
    {
        primaryFactor = (current->getObjectTypeFlags() & 0x10) ? 1.5f : 1.0f;
        currentFactor = 0.25f;
    }
    const bool currentIsPrimary = current == primary;
    int iterator = primary->getFirstSecondaryObjectIterator();
    unsigned int handle = current->getPrimaryObject(side);
    IPIUnit* primaryUnit = primary->getUnit();
    bool invalidPosition = false;
    float best = 0.0f;
    float currentUrgency = 0.0f;
    Information* candidate = primary;
    if (candidate)
    {
        do
        {
            const bool isCurrent = current == candidate;
            float urgency = calculateVehicleUrgency(bot, candidate, strengths, types, !isCurrent,
                                                    current, fireShare, moveShare, side);
            urgency *= isCurrent ? currentFactor : (candidate->isPrimary() ? primaryFactor : secondaryFactor);
            urgency *= orderFactor;
            if (candidate == primary && !primaryUnit->isOccupied() && primaryMobile)
            {
                const IPIMobile::ValidPos valid = primaryMobile->getLastValidPosition();
                if (valid.quadTree == 0xFFFFFFFFu)
                { urgency = 0.0f; invalidPosition = true; }
                else urgency += 100.0f;
            }
            if (isCurrent)
                currentUrgency = urgency;
            if (urgency > best)
            { best = urgency; bestHandle = handle; }
            handle = primary->getSecondaryObject(iterator, side);
            candidate = informationManager->getInformation(handle);
        } while (candidate && !invalidPosition);
    }

    bool disembark = false;
    const bool air = (primary->getObjectTypeFlags() & 0x10) != 0;
    const Vec3& position = current->getPosition();
    bool canDisembark = false;
    if (air)
    {
        SquadMember* member = bot->getSquadMember();
        if (member && currentIsPrimary)
        {
            const bool close = member->isCloseToNonOwnedTarget(10000.0f, 22500.0f);
            const bool alone = isOnlyPrimaryInVehicle(current, side);
            if (alone)
                canDisembark = member->isCloseToNonOwnedTarget(625.0f, 10000.0f);
            else if (close)
            {
                CommunicationAgent* communication = bot->getCommunicationAgent();
                if (!communication->getSentMessage(static_cast<AIRadioMessageType>(0)))
                    communication->enqueueRadioMessage(static_cast<AIRadioMessageType>(0),
                        static_cast<AIRadioChannelType>(7), 10, 0.0f, -1);
            }
        }
    }
    else if (IPIPhysical* physical = primary->getPhysical())
    {
        Vec3 contact = {0.0f, 0.0f, 0.0f};
        canDisembark = !physical->isInWater() || physical->isTouchingTerrain(contact);
    }
    if (canDisembark)
    {
        IPIMobile* mobile = primary->getMobile();
        canDisembark = air || mobile->getSpeedMagnitude() < 2.0f;
        const int playerSide = bot->getSide();
        bot->getPlayer()->getSoldier()->getInformation(playerSide);
        if (canDisembark)
        {
            if (air)
                disembark = true;
            else
            {
                const Vec3& valid = mobile->getLastValidInfantryPos().position;
                const float dx = valid.x - position.x;
                const float dy = valid.y - position.y;
                const float dz = valid.z - position.z;
                disembark = 16.0f > dx * dx + dy * dy + dz * dz;
            }
        }
    }
    if (!air && !(primary->getObjectTypeFlags() & 0x200000))
    {
        Vec3 normal = {0.0f, 0.0f, 0.0f};
        IAITerrain* terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
        terrain->getWaterLevel(position.x, position.z);
        terrain->getHeight(position.x, position.z);
        terrain->getNormal(position.x, position.z, normal);
    }
    const std::list<AIRadioMessageInfo>& messages = bot->getCommunicationAgent()->getReceivedMessages(static_cast<AIRadioMessageType>(0));
    if (air && !messages.empty())
        disembark = true;
    if (current->getPhysical())
    {
        const float health = current->getPhysical()->getHealth();
        if (IBotManager::instance->getVehicleBailValue() + 0.1f >= health)
        {
            disembark = true;
            bot->getCommunicationAgent()->sendImmediately(static_cast<AIRadioMessageType>(0),
                static_cast<AIRadioChannelType>(7), 10, 0);
        }
    }
    if (disembark || invalidPosition)
    {
        const int playerSide = bot->getSide();
        bestHandle = bot->getPlayer()->getSoldier()->getInformation(playerSide);
        const unsigned int botId = bot->getBotId();
        m_targetChanged[botId] = m_targets[bot->getBotId()] != 0xFFFFFFFFu;
        m_targets[bot->getBotId()] = 0xFFFFFFFFu;
        if (air)
            static_cast<BBPChange*>(m_pattern)->init(bestHandle, false, false, bot);
        else
            static_cast<BBPChangeLandingCraft*>(m_pattern)->init(bestHandle, false, bot);
        primary->getAIObject()->getPhysical(); // ELF 0x2044FC: result discarded.
        static_cast<BotMain*>(bot)->resetAllControls(true, true, true);
        m_urgency[bot->getBotId()] = 4.0f;
    }
    else if (bestHandle == bot->getCurrentEquipment())
    {
        const bool initiated = m_pattern->isInitiated(bot);
        m_targetChanged[bot->getBotId()] = initiated;
        m_pattern->unInit(bot);
        m_targets[bot->getBotId()] = 0xFFFFFFFFu;
        m_urgency[bot->getBotId()] = 0.0f;
    }
    else
    {
        const unsigned int botId = bot->getBotId();
        m_targetChanged[botId] = m_targets[bot->getBotId()] != bestHandle;
        m_targets[bot->getBotId()] = bestHandle;
        if (air)
            static_cast<BBPChange*>(m_pattern)->init(bestHandle, true, false, bot);
        else
            static_cast<BBPChangeLandingCraft*>(m_pattern)->init(bestHandle, true, bot);
        const unsigned int urgencyId = bot->getBotId();
        const float ratio = currentUrgency > 0.0f ? (best * 0.5f) / currentUrgency : best * 0.5f;
        m_urgency[urgencyId] = DecleiningSlopeCurve::calculate(ratio) * 4.0f;
        const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
        const double sincePossessed = now - current->getUnit()->getLastPlayerPossesionTime();
        if (15.0f > sincePossessed)
            m_urgency[bot->getBotId()] *= 6.0f;
    }
    const float urgency = checkLandingCraftBookings(bot);
    m_urgency[bot->getBotId()] = urgency;
    return m_urgency[bot->getBotId()];
}

// ELF 0x203C30
float BBChangeLandingCraft::checkLandingCraftBookings(Bot* bot)
{
    const unsigned int id = bot->getBotId();
    float urgency = m_urgency[id];
    Information* target = informationManager->getInformation(m_targets[id]);
    if (target && target->isBooked() && !target->isBookedBy(bot->getPlayerId()))
        if (m_urgency[id] > 0.0f)
            urgency = 0.0f;
    return urgency;
}

// ELF 0x203D00
bool BBChangeLandingCraft::isOnlyPrimaryInVehicle(Information* primary, int side)
{
    int iterator;
    Information* seat = informationManager->getInformation(primary->getFirstSecondaryObject(iterator, side));
    while (seat)
    {
        if (seat->getUnit() && seat->getUnit()->isOccupied())
            return false;
        seat = informationManager->getInformation(primary->getSecondaryObject(iterator, side));
    }
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
