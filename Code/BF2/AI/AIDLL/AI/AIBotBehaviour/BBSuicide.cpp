// Suicide urgency is the remaining fraction of the current equipment's man-down interval.

// static init: ELF 0x21D740 (__tcf_0 0x21D7A0, _GLOBAL__I 0x21D7D0): <iostream>'s Init object only.
#include <iostream>

#include "BBSuicide.h"

#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/BBPSuicide.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x21D460 (C2), 0x21D510 (C1)
BBSuicide::BBSuicide(unsigned char index, unsigned char queue, BBPSuicide* pattern,
                     UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve,
                   static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targetChanged[i] = false;
    }
}

// ELF 0x21D5C0 (D2), 0x21D600 (D1), 0x21D640 (D0)
BBSuicide::~BBSuicide()
{
}

// ELF 0x21D680
float BBSuicide::calculateUrgency(Bot* bot, bool, float)
{
    Information* equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IPIPhysical* physical = equipment->getPhysical();
    const unsigned int index = bot->getBotId();
    const float remaining = physical->getManDownTimeLeft();
    const float maximum = physical->getMaxManDownTime();
    m_urgency[index] = remaining / maximum;
    static_cast<BBPSuicide*>(m_pattern)->init(bot);
    return m_urgency[bot->getBotId()];
}

} // namespace ai
} // namespace hfe
} // namespace dice
