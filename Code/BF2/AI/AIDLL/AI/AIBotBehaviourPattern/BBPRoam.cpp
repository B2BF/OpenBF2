// Shared roaming pattern state, recovered from libaidll.so (Linux 1.5, ia-32).

// static init: ELF 0x29B4A0 (__tcf_0 0x29B500, _GLOBAL__I 0x29B530): <iostream>'s Init object only.
#include <iostream>

#include "BBPRoam.h"

#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Vec3.h"

#include <string>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x29B3A0 (C2), 0x29B420 (C1).
BBPRoam::BBPRoam()
{
    m_positions = new Vec3[static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots()]();
}

// ELF 0x29B2B0 (D2), 0x29B300 (D1), 0x29B350 (D0).
BBPRoam::~BBPRoam()
{
    if (m_positions)
        delete[] m_positions;
}

// ELF 0x29B550
int BBPRoam::getRoamType()
{
    return 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
