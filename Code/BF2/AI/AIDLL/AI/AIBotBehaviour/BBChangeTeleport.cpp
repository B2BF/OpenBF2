// Rates teleporting between the current vehicle's seats and initializes its change pattern.

// static init: ELF 0x205D70 (__tcf_0 0x205DD0, _GLOBAL__I 0x205E00): <iostream>'s Init object only.
#include <iostream>

#include "BBChangeTeleport.h"

#include "VehicleUrgencyCalculator.h"
#include "../AIUtils/CustomCurve.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBotBehaviourPattern/BBPChangeTeleport.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIStrategicMap/BFStrategicEnvironment.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

#include <list>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x204F50 (C2) / 0x205010 (C1); vtable GOT -0x1624. The BBAvoid shape: the base gets getMaxNBots
// (IAISettings slot 0); a second getMaxNBots sizes the +0x08 target array (plain operator new[]),
// and every bot starts with urgency 0.0f (rodata 0x49CB34) and target 0xFFFFFFFF.
BBChangeTeleport::BBChangeTeleport(unsigned char index, unsigned char queue, PlanGenerator* pattern,
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

// ELF 0x2050D0 (D2) / 0x205120 (D1) / 0x205170 (D0): delete[] +0x08 when set, then ~BotBehaviour.
BBChangeTeleport::~BBChangeTeleport()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x2051C0
float BBChangeTeleport::calculateUrgency(Bot* bot, bool evaluate, float)
{
    if (!evaluate)
    {
        m_urgency[bot->getBotId()] = 0.0f;
        const bool initiated = m_pattern->isInitiated(bot);
        m_targetChanged[bot->getBotId()] = initiated;
        m_pattern->unInit(bot);
        return m_urgency[bot->getBotId()];
    }
    std::list<unsigned int> unused;
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
        else
        {
            fireShare = 0.0f;
            moveShare = 1.0f;
        }
    }
    else if (bot->getFireInclination() > 0.0f)
    {
        fireShare = 1.0f;
        moveShare = 0.0f;
    }
    else
        fireShare = moveShare = 0.5f;

    const int side = bot->getSide();
    Information* equipment = informationManager->getInformation(bot->getCurrentEquipment());
    equipment->getPrimaryObject(side);
    float orderModifier = 1.0f;
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
        orderModifier = bot->getMemory()->m_float8;
    IAIStrategicEnvironment* strategic =
        static_cast<IAIStrategicEnvironment*>(AISingletonServer::m_iaiStrategicEnvironment);
    const float* types = strategic->getEnemyTypes(bot);
    const float* strengths = strategic->getEnemyStrengths(bot);
    unsigned int bestHandle = bot->getCurrentEquipment();
    float currentModifier;
    float secondaryModifier;
    float primaryModifier;
    if (equipment->isPrimary())
    {
        currentModifier = 1.0f;
        secondaryModifier = 0.5f;
        primaryModifier = 1.0f;
    }
    else
    {
        Information* primary = informationManager->getInformation(equipment->getPrimaryObject(side));
        if (primary->getUnit()->isOccupied())
        {
            currentModifier = 1.0f;
            secondaryModifier = primaryModifier = 0.5f;
        }
        else
        {
            if (equipment->isOfType(ITNaval))
            {
                currentModifier = 0.5f;
                secondaryModifier = 0.65f;
                primaryModifier = 1.0f;
            }
            else if (equipment->isOfType(ITGround))
            {
                currentModifier = 0.5f;
                secondaryModifier = 0.7f;
                primaryModifier = 1.0f;
            }
            else if (equipment->isOfType(ITAir))
            {
                currentModifier = secondaryModifier = 0.5f;
                primaryModifier = 1.5f;
            }
            else
                currentModifier = secondaryModifier = primaryModifier = 1.0f;
        }
    }
    bool isPrimary = equipment->isPrimary();
    Information* primary = equipment;
    if (!isPrimary)
    {
        bestHandle = equipment->getPrimaryObject(side);
        primary = informationManager->getInformation(bestHandle);
    }
    float current = calculateVehicleUrgency(bot, primary, strengths, types, !isPrimary,
                                            equipment, fireShare, moveShare, side);
    current *= isPrimary ? currentModifier : primaryModifier;
    float best = current;
    if (!isPrimary)
    {
        best *= orderModifier;
        current = 0.0f;
    }
    int iterator;
    unsigned int handle = primary->getFirstSecondaryObject(iterator, side);
    Information* seat = informationManager->getInformation(handle);
    while (seat)
    {
        const bool other = equipment != seat;
        // shipped defect: each secondary is rated using the primary information record.
        float candidate = calculateVehicleUrgency(bot, primary, strengths, types, other,
                                                   equipment, fireShare, moveShare, side);
        candidate *= other ? secondaryModifier : currentModifier;
        if (other)
            candidate *= orderModifier;
        else
            current = candidate;
        if (candidate > best)
        {
            best = candidate;
            bestHandle = handle;
        }
        handle = primary->getSecondaryObject(iterator, side);
        seat = informationManager->getInformation(handle);
    }
    if (bestHandle == bot->getCurrentEquipment() && fireShare < 0.05f && bot->getLodLevel() != 0)
    {
        const unsigned int changedIndex = bot->getBotId();
        m_targetChanged[changedIndex] = m_targets[bot->getBotId()] != 0xFFFFFFFFu;
        m_targets[bot->getBotId()] = 0xFFFFFFFFu;
        static_cast<BBPChangeTeleport*>(m_pattern)->init(bestHandle, false, bot);
        m_urgency[bot->getBotId()] = 2.0f;
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
        const unsigned int changedIndex = bot->getBotId();
        m_targetChanged[changedIndex] = m_targets[bot->getBotId()] != bestHandle;
        m_targets[bot->getBotId()] = bestHandle;
        static_cast<BBPChangeTeleport*>(m_pattern)->init(bestHandle, true, bot);
        const unsigned int urgencyIndex = bot->getBotId();
        const float ratio = current > 0.0f ? (best * 0.5f) / current : best * 0.5f;
        m_urgency[urgencyIndex] = DecleiningSlopeCurve::calculate(ratio) * 4.0f;
    }
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const double elapsed = now - equipment->getUnit()->getLastPlayerPossesionTime();
    if (elapsed < 15.0f)
    {
        const unsigned int index = bot->getBotId();
        m_urgency[index] = 6.0f * m_urgency[index];
    }
    return m_urgency[bot->getBotId()];
}

} // namespace ai
} // namespace hfe
} // namespace dice
