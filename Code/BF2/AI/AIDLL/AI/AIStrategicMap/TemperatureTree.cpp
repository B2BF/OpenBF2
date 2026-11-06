// Strategic temperature tree. Evidence: libaidll.so (ia-32) 0x366510..0x366A55. The TU has no
// static initialisation in the image. Placement next to TemperatureTreeModifiers is [INFERRED]
// from the address range.

#include "TemperatureTree.h"

#include "TemperatureTreeModifiers.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x366510 (C2; C1 0x366540).
TemperatureTree::TemperatureTree()
{
    reset();
}

// ELF 0x366570 (C2; C1 0x3665C0): the ten words in order. The memberwise spelling is [INFERRED].
TemperatureTree::TemperatureTree(const TemperatureTree& other)
{
    *this = other;
}

// ELF 0x366610.
void TemperatureTree::reset()
{
    for (int i = 0; i <= 4; ++i)
    {
        m_temperatures[i] = 0.0f;
        m_weights[i] = 1.0f;
    }
}

// ELF 0x366650.
void TemperatureTree::addTemperature(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float temperature, float weight)
{
    propagateToParents(modifiers, node, temperature, weight);
}

// ELF 0x3666A0.
void TemperatureTree::addTemperature(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float* strengths, float weight)
{
    float temperature = 0.0f;
    for (int i = 0; i <= 5; ++i)
        temperature += strengths[i] * modifiers->m_combatStrengthModifiers[node][i];
    propagateToParents(modifiers, node, temperature, weight);
}

// ELF 0x366720.
void TemperatureTree::addTemperature(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float* strengths, float scale, float weight)
{
    float temperature = 0.0f;
    for (int i = 0; i <= 5; ++i)
        temperature += strengths[i] * modifiers->m_combatStrengthModifiers[node][i];
    propagateToParents(modifiers, node, temperature * scale, weight);
}

// ELF 0x3667A0.
void TemperatureTree::addTemperature(const TemperatureTree& other)
{
    for (int i = 0; i <= 4; ++i)
        m_temperatures[i] += other.m_temperatures[i];
}

// ELF 0x3667D0: a jump table over the 41 nodes; the image turns the parent calls into a loop.
// Nodes 0..4 are stored (temperature += t * modifier, weight *= w); every node except the root
// passes t * its modifier on to its parent. Nodes above 40 are ignored.
// [CHOSEN, not recovered] each level's product is rounded to float; the ELF keeps it in x87.
void TemperatureTree::propagateToParents(TemperatureTreeModifiers* modifiers, TemperatureTreeNode node, float temperature, float weight)
{
    const float* const m = modifiers->m_nodeModifiers;
    int parent;
    switch (static_cast<int>(node))
    {
    case 0:
        m_temperatures[0] += temperature * m[0];
        m_weights[0] *= weight;
        return;
    case 1: case 2: case 3: case 4:
        m_temperatures[node] += temperature * m[node];
        m_weights[node] *= weight;
        parent = (node <= 2) ? 0 : 1;
        break;
    case 5: case 6:                                   parent = 2; break;
    case 7: case 9: case 13:                          parent = 3; break;
    case 8: case 10: case 15:                         parent = 4; break;
    case 29: case 30: case 31: case 32:               parent = 5; break;
    case 33: case 34: case 35: case 36: case 37: case 38: parent = 6; break;
    case 11: case 12:                                 parent = 7; break;
    case 14: case 16:                                 parent = 8; break;
    case 17: case 18:                                 parent = 11; break;
    case 19: case 20:                                 parent = 12; break;
    case 21: case 22:                                 parent = 13; break;
    case 23: case 24:                                 parent = 14; break;
    case 27: case 28:                                 parent = 15; break;
    case 25: case 26:                                 parent = 16; break;
    case 39: case 40:                                 parent = 38; break;
    default:
        return;
    }
    propagateToParents(modifiers, static_cast<TemperatureTreeNode>(parent), temperature * m[node], weight);
}

} // namespace ai
} // namespace hfe
} // namespace dice
