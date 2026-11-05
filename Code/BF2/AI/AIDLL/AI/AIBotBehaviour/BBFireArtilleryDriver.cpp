// Artillery driver fire behaviour, recovered from the symbolized Linux 1.50 libaidll.so.

// static init: ELF 0x20F070 (__tcf_0 0x20F0D0, _GLOBAL__I 0x20F100): <iostream>'s Init object only.
#include <iostream>

#include "BBFireArtilleryDriver.h"

#include "BBFireLargeBore.h"
#include "BBFireUnarmed.h"
#include "../AIBot/BotMain.h"
#include "../AIBotBehaviourPattern/BBPFire.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x20E3F0 (C2), 0x20E870 (C1)
BBFireArtilleryDriver::BBFireArtilleryDriver(unsigned char index, unsigned char queue, BBPFire* pattern,
                                           UrgeCurve* curve, float* modifiers)
    : BotBehaviour(modifiers, curve, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_unarmed = checkPointer(new BBFireUnarmed(index, queue));
    m_gunner = checkPointer(new BBFireLargeBore(index, queue, pattern, curve, modifiers, false));
    m_index = index;
    m_queue = queue;
    m_pattern = pattern;
    const int bots = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_targets = checkPointer(new unsigned int[bots]);
    for (int i = 0; i < bots; ++i)
    {
        m_urgency[i] = 0.0f;
        m_targets[i] = static_cast<unsigned int>(-1);
    }
    pattern->setUrgencyGenerator(this);
}

// ELF 0x20ECF0 (D2), 0x20ED70 (D1), 0x20EDF0 (D0)
BBFireArtilleryDriver::~BBFireArtilleryDriver()
{
    if (m_targets)
        delete[] m_targets;
    if (m_unarmed)
        delete m_unarmed;
    if (m_gunner)
        delete m_gunner;
    // Shipped defect: the gunner and this behaviour both own the same pattern through their bases.
}

// ELF 0x20EE70
float BBFireArtilleryDriver::calculateUrgency(Bot* bot, bool recalculate, float modifier)
{
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    int secondary;
    Information* const gunner = informationManager->getInformation(equipment->getFirstSecondaryObject(secondary, bot->getSide()));
    // Shipped defect: neither the secondary object nor its unit is checked for null.
    if (gunner->getUnit()->isOccupied())
    {
        const float urgency = m_gunner->calculateUrgency(bot, recalculate, modifier, gunner, equipment->getMobile());
        m_urgency[bot->getBotId()] = urgency;
        const bool changed = m_gunner->hasChangedTarget(bot);
        m_targetChanged[bot->getBotId()] = changed;
    }
    else
    {
        m_targetChanged[bot->getBotId()] = false;
        const float urgency = m_unarmed->calculateUrgency(bot, recalculate, modifier);
        m_urgency[bot->getBotId()] = urgency;
    }
    return m_urgency[bot->getBotId()];
}

} // namespace ai
} // namespace hfe
} // namespace dice
