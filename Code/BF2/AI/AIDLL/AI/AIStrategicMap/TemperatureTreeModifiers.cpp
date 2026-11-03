// Temperature-tree modifiers. Evidence: libaidll.so (ia-32) 0x366A60..0x366B20.

#include "TemperatureTreeModifiers.h"

namespace dice {
namespace hfe {
namespace ai {

TemperatureTreeModifiers::TemperatureTreeModifiers()
{
    reset();
}

// ELF 0x366AC0.
void TemperatureTreeModifiers::reset()
{
    for (unsigned int node = 0; node <= 40; ++node)
        m_nodeModifiers[node] = 1.0f;
    for (unsigned int node = 0; node <= 55; ++node)
        for (unsigned int type = 0; type <= 5; ++type)
            m_combatStrengthModifiers[node][type] = 1.0f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
