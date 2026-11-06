// Aircraft cover urgency from missile-lock status.

// static init: ELF 0x220BC0 (__tcf_0 0x220C20, _GLOBAL__I 0x220C50): <iostream>'s Init object only.
#include <iostream>

#include "BBTakeCover3d.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBotBehaviourPattern/BBPTakeCover3d.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x220750 (C2), 0x220810 (C1)
BBTakeCover3d::BBTakeCover3d(unsigned char index, unsigned char queue, PlanGenerator* pattern,
                           UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve,
                   static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    // The allocated target elements are left uninitialized by the shipped constructor.
    m_targets = new unsigned int[count];
    for (int i = 0; i < count; ++i)
        m_urgency[i] = 0.0f;
}

// ELF 0x2208D0 (D2), 0x220920 (D1), 0x220970 (D0)
BBTakeCover3d::~BBTakeCover3d()
{
    if (m_targets)
        delete[] m_targets;
}

// ELF 0x2209C0
float BBTakeCover3d::calculateUrgency(Bot* bot, bool, float scale)
{
    Information* equipment = informationManager->getInformation(bot->getCurrentEquipment());
    float urgency = 0.0f;
    switch (equipment->getUnit()->getMissileLockStatus())
    {
    case 1:
    case 2:
        urgency = 0.0f;
        break;
    case 3:
        urgency = 20.0f;
        break;
    default:
        break;
    }
    if (urgency > 0.0f)
    {
        if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders() && bot->getOrderList(m_index)->empty())
            urgency *= bot->getMemory()->m_float8;
        m_urgency[bot->getBotId()] = urgency * scale;
        m_targetChanged[bot->getBotId()] = false;
        static_cast<BBPTakeCover3d*>(m_pattern)->init(bot);
        return m_urgency[bot->getBotId()];
    }
    m_pattern->unInit(bot);
    if (m_pattern->isInitiated(bot))
        m_targetChanged[bot->getBotId()] = true;
    else
        m_targetChanged[bot->getBotId()] = false;
    m_urgency[bot->getBotId()] = 0.0f;
    return 0.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
