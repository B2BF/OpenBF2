// Fixed move-to behaviour (AI/AIDLL/AI/AIBotBehaviour/BBMoveToFixed.cpp in the image). Recovered
// from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x21A1A0 (__tcf_0 0x21A200, _GLOBAL__I 0x21A230): <iostream>'s Init object only.
#include <iostream>

#include "BBMoveToFixed.h"

#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../AIUtils/checkPointer.h"
#include "../../../../Math/Vec3.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotMemory.h"
#include "../AIBot/WaypointList.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../Main/AIMain.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x219A10 (C2) / 0x219C20 (C1); vtable GOT -0x1190. BotBehaviour(null modifiers, null curve,
// getMaxNBots); no targets and no pattern (+0x08 = +0x0C = 0); +0x24 = checkPointer(new
// float[getMaxNBots()]) (operator new 0x28); every bot starts with urgency and +0x24 at 0.0f.
BBMoveToFixed::BBMoveToFixed(unsigned char index, unsigned char queue)
    : BotBehaviour(0, 0, static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots())
{
    m_index = index;
    m_targets = 0;
    m_pattern = 0;
    m_queue = queue;
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_value24 = checkPointer(new float[count]);
    for (int i = 0; i < count; ++i)
    {
        m_urgency[i] = 0.0f;
        m_value24[i] = 0.0f;
    }
}

// ELF 0x219E30 (D2) / 0x219E80 (D1) / 0x219ED0 (D0): delete[] +0x24 when set, then ~BotBehaviour.
BBMoveToFixed::~BBMoveToFixed()
{
    if (m_value24)
        delete[] m_value24;
}

// ELF 0x219F20 (Ghidra; listing cross-check). Always returns 0.0; the urgency goes to +0x24 and to
// the bot's move inclination. With `recalculate` it is the waypoint list's urgency at the
// equipment's position (times the memory's +0x08 for an ordered behaviour); while positive, the
// target-changed flag follows the list's +0x04 flag and the list's goal point is reported to the
// sensing agent with weight 0.5. The SAI of the bot's side is fetched and not used, and a zero
// Vec3 is built and never read, as compiled.
float BBMoveToFixed::calculateUrgency(Bot* bot, bool recalculate, float)
{
    if (!recalculate)
    {
        m_targetChanged[bot->getBotId()] = false;
        bot->addToMoveInclination(m_value24[bot->getBotId()]);
        return 0.0f;
    }

    Vec3 unused;
    unused.x = 0.0f;
    unused.y = 0.0f;
    unused.z = 0.0f;
    (void)unused;
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    IAIMain::m_instance->getSAI(bot->getSide());   // result unused
    const Vec3& position = equipment->getPosition();
    m_value24[bot->getBotId()] = bot->getWaypoints()->getUrgency(position, bot);
    if (bot->isOrderedBehaviour(m_index) && bot->isTakingOrders())
        m_value24[bot->getBotId()] = m_value24[bot->getBotId()] * bot->getMemory()->m_float8;
    if (0.0f < m_value24[bot->getBotId()])
    {
        m_targetChanged[bot->getBotId()] = bot->getWaypoints()->m_flag4;
        bot->addToMoveInclination(m_value24[bot->getBotId()]);
        Vec3 goal;
        goal.x = 0.0f;
        goal.y = 0.0f;
        goal.z = 0.0f;
        if (bot->getWaypoints()->getGoalPoint(goal))
            bot->insertBehaviourInformation(0.5f, goal, m_index);
    }
    else
    {
        m_targetChanged[bot->getBotId()] = false;
    }
    return 0.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
