// Fire pattern base (AI/AIDLL/AI/AIBotBehaviourPattern/BBPFire.cpp in the image). Recovered from
// libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address. Package D.

// static init: ELF 0x244F10 (__tcf_0 0x244F70, _GLOBAL__I 0x244FA0): <iostream>'s Init object only.
#include <iostream>

#include "BBPFire.h"

#include "../AIUtils/checkPointer.h"
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

// ELF 0x244910. m_patternId, m_flag20 and m_urgencyGenerator are left to the concrete constructor
// and BBFire's constructor (setUrgencyGenerator).
BBPFire::BBPFire()
{
    m_targets = checkPointer(new unsigned int[settings()->getMaxNBots()]);
    m_targetChanged = checkPointer(new bool[settings()->getMaxNBots()]);
    m_weapons = checkPointer(new int[settings()->getMaxNBots()]);
    m_positionArgs = checkPointer(new unsigned int[settings()->getMaxNBots()]);
    // [CHOSEN, not recovered] value-initialisation stands in for the image's zeroing Vec3
    // constructor loop (0x244A4B: three zero stores per element); Math::Vec3 is a POD here.
    m_targetPositions = checkPointer(new Vec3[settings()->getMaxNBots()]());
}

// ELF 0x2441C0
BBPFire::~BBPFire()
{
    if (m_targets)
        delete[] m_targets;
    if (m_targetChanged)
        delete[] m_targetChanged;
    if (m_weapons)
        delete[] m_weapons;
    if (m_positionArgs)
        delete[] m_positionArgs;
    if (m_targetPositions)
        delete[] m_targetPositions;
}

// ELF 0x244FC0
int BBPFire::getImplementationType()
{
    return 0;
}

// ELF 0x244F00
void BBPFire::setUrgencyGenerator(BotBehaviour* behaviour)
{
    m_urgencyGenerator = behaviour;
}

} // namespace ai
} // namespace hfe
} // namespace dice
