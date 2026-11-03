// Base of the avoid-collision patterns (BBPAvoidCollisionInfantery, BBPAvoidCollision2d, 3d): the
// per-bot arrays their init (slot +0x18) fills from BBAvoid's calculateUrgency.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32); F-urgency package, see
// docs/macos-ai-behaviour-plan.md.

// static init: ELF 0x2297F0 (__tcf_0 0x229850, _GLOBAL__I 0x229880): <iostream>'s Init object only.
#include <iostream>

#include "BBPAvoidCollision.h"

#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x229090 (C2) / 0x229440 (C1). One getMaxNBots (IAISettings slot 0) sizes all three arrays:
// +0x0C new[n] of 4 bytes, +0x10 new[n * 12] with every Vec3 zeroed (rodata 0x49CB34) before the
// check, +0x14 new[n] of 4 bytes; each is checkPointer'd. +0x08 is left to the subclasses.
BBPAvoidCollision::BBPAvoidCollision()
    : BBPPattern()
{
    const int count = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getMaxNBots();
    m_obstacle = checkPointer(new unsigned int[count]);
    m_relativeSpeed = checkPointer(new Vec3[count]());
    m_value = checkPointer(new float[count]);
}

// ELF 0x228F10 (D2) / 0x228F90 (D1) / 0x229010 (D0): delete[] +0x0C, +0x10, +0x14 when set, then
// ~BBPPattern.
BBPAvoidCollision::~BBPAvoidCollision()
{
    if (m_obstacle)
        delete[] m_obstacle;
    if (m_relativeSpeed)
        delete[] m_relativeSpeed;
    if (m_value)
        delete[] m_value;
}

} // namespace ai
} // namespace hfe
} // namespace dice
