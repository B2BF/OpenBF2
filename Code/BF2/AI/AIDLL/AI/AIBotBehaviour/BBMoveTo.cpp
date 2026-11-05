// MoveTo behaviour (AI/AIDLL/AI/AIBotBehaviour/BBMoveTo.cpp in the image). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address. Package D.

// static init: ELF 0x219960 (__tcf_0 0x2199C0, _GLOBAL__I 0x2199F0): <iostream>'s Init object only.
#include <iostream>

#include "BBMoveTo.h"

#include "BBPendingCalls.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/SquadCoordinator.h"
#include "../AIBot/SquadFormation.h"
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"
#include "../AIBot/WaypointList.h"
#include "../AIBotBehaviourPattern/BBPGotoWaypoint.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
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

// ELF 0x2193A0
BBMoveTo::BBMoveTo(unsigned char index, unsigned char queue, BBPGotoWaypoint* pattern, UrgeCurve* curve,
                   float* modifiers)
    : BotBehaviour(modifiers, curve, settings()->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_targets = 0;
    m_pattern = pattern;

    const int bots = settings()->getMaxNBots();
    for (int i = 0; i < bots; ++i)
        m_urgency[i] = 0.0f;
}

// ELF 0x219490
BBMoveTo::~BBMoveTo()
{
}

// ELF 0x219510
float BBMoveTo::calculateUrgency(Bot* bot, bool recalculate, float modifier)
{
    BBPGotoWaypoint* const pattern = static_cast<BBPGotoWaypoint*>(m_pattern);

    // A squad member that holds its formation slot does not move until the squad moves out.
    SquadMember* const member = bot->getSquadMember();
    if (bbpending::isAtFormationSlot(member)
        && !member->getSquad()->getSquadFormation()->shouldSquadMoveOut())
    {
        return m_urgency[bot->getBotId()] = 0.0f;
    }

    if (!recalculate)
    {
        m_targetChanged[bot->getBotId()] = false;
        if (!pattern->isInitiated(bot))
            m_urgency[bot->getBotId()] = 0.0f;
        bot->addToMoveInclination(m_urgency[bot->getBotId()]);
        return m_urgency[bot->getBotId()];
    }

    float factor = 1.0f;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders())
    {
        const float weight = bot->getMemory()->m_float8;
        m_urgency[bot->getBotId()] = weight * bot->getWaypoints()->getUrgency(equipment->getPosition(), bot);
    }
    else
    {
        m_urgency[bot->getBotId()] = bot->getWaypoints()->getUrgency(equipment->getPosition(), bot);
    }

    const int side = bot->getSide();
    Vec3 position = equipment->getPosition();
    double sinceStart;
    int artillerySide;
    if (SquadManager::m_instance->isInArtilleryDangerZone(position, side, sinceStart, artillerySide))
    {
        SquadMember* const avoiding = bot->getSquadMember();
        if (avoiding && avoiding->isAvoidingArtillery())
            m_urgency[bot->getBotId()] = 1000.0f;
    }

    factor = factor * modifier;
    m_urgency[bot->getBotId()] = factor * m_urgency[bot->getBotId()];
    if (m_urgency[bot->getBotId()] <= 0.0f)
    {
        m_targetChanged[bot->getBotId()] = pattern->isInitiated(bot);
        pattern->unInit(bot);
        m_urgency[bot->getBotId()] = 0.0f;
        return 0.0f;
    }

    m_targetChanged[bot->getBotId()] = bot->getWaypoints()->m_flag4;
    pattern->init(bot);
    bot->addToMoveInclination(m_urgency[bot->getBotId()]);

    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    if (bot->getWaypoints()->getGoalPoint(position))
        bot->insertBehaviourInformation(0.5f, position, m_index);
    return m_urgency[bot->getBotId()];
}

} // namespace ai
} // namespace hfe
} // namespace dice
