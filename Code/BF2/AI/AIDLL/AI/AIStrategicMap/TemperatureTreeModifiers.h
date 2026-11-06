// Per-strategy multipliers for the strategic temperature tree: one per tree node and one per
// (node, combat-strength type). Evidence: libaidll.so (ia-32) 0x366A60..0x366B20 (constructor,
// reset) and Strategy's accessors (0x333930..0x333A80); operator new(0x5E4) in Strategy's
// constructor. Source file attested as TemperatureTreeModifiers.cpp (AIStrategicMap link block).

#ifndef AI_AIDLL_AI_AISTRATEGICMAP_TEMPERATURETREEMODIFIERS_H
#define AI_AIDLL_AI_AISTRATEGICMAP_TEMPERATURETREEMODIFIERS_H

namespace dice {
namespace hfe {
namespace ai {

class TemperatureTreeModifiers
{
public:
    TemperatureTreeModifiers();
    void reset();                                 // every multiplier to 1.0f

    /* +0x000 */ float m_nodeModifiers[41];               // TemperatureTreeNode 0..40
    /* +0x0A4 */ float m_combatStrengthModifiers[56][6];  // [node][CombatStrengthType]; reset
                                                          // covers 56 rows, setters only 0..40
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICMAP_TEMPERATURETREEMODIFIERS_H
