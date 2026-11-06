// A strategy of the strategic AI: the number of attacks/defences it runs, its aggression, the
// prerequisite and abort conditions that score it, which strategies may precede it, and the
// temperature and strategic-object modifiers it applies while active. The StrategyManager
// (Main) builds strategies from the ai/Strategies.ai scripts and owns them; SAI borrows them.
// Evidence: libaidll.so (ia-32) Strategy.cpp 0x3330A0..0x333DC0.

#ifndef AI_AIDLL_AI_AISTRATEGIC_STRATEGY_H
#define AI_AIDLL_AI_AISTRATEGIC_STRATEGY_H

#include <set>
#include <string>
#include <vector>

#include "StatusMap.h"
#include "StrategyCondition.h"
#include "StrategyPrerequisite.h"
#include "../AIStrategicEntity/AIStrategicObject.h"
#include "../AIStrategicMap/TemperatureTreeModifiers.h"

namespace dice {
namespace hfe {
namespace ai {

enum TemperatureTreeNode : int;           // index 0..40 (setTreeModifier checks > 0x28)
enum CombatStrengthType : int;            // 0..5 per tree node

class Strategy
{
public:
    explicit Strategy(const std::string& name);   // ELF 0x3332E0 (complete) / 0x3330A0
    ~Strategy();                                  // ELF 0x333460 / 0x3335A0

    TemperatureTreeModifiers* getTemperatureTreeModifiers() const;
    void setTemperatureTreeModifiers(TemperatureTreeModifiers* modifiers);
    float getAggression() const;
    void setAggression(float aggression);
    unsigned int getNumberOfAttacks() const;
    void setNumberOfAttacks(unsigned int count);
    unsigned int getNumberOfAttackConsiderations() const;
    void setNumberOfAttackConsiderations(unsigned int count);
    unsigned int getNumberOfDefences() const;
    void setNumberOfDefences(unsigned int count);
    unsigned int getNumberOfDefenceConsiderations() const;
    void setNumberOfDefenceConsiderations(unsigned int count);
    void setPrerequisite(StrategyPrerequisite* prerequisite);
    void setAbortCondition(StrategyPrerequisite* condition);
    float calculatePrerequisite(float* friendly, float* enemy, Strategy* previous, int side);
    float calculateAbortCondition(float* friendly, float* enemy, Strategy* previous, int side);
    float setAbortLevel(float level);             // fsts: the value stays in st0
    float getAbortLevel() const;
    float setPrerequisiteWeight(float weight);    // fsts: the value stays in st0
    float getPrerequisiteWeight() const;

    void setTreeModifier(TemperatureTreeNode node, float value);
    float getTreeModifier(TemperatureTreeNode node) const;
    void setTreeModifier(TemperatureTreeNode node, CombatStrengthType type, float value);
    float getTreeModifier(TemperatureTreeNode node, CombatStrengthType type) const;
    void setTreeModifierCombatStrength(TemperatureTreeNode node, CombatStrengthType type, float value);
    float getTreeModifierCombatStrength(TemperatureTreeNode node, CombatStrengthType type) const;
    bool addRequiredPrecedingStrategy(Strategy* strategy);     // null -> false
    bool addProhibitedPrecedingStrategy(Strategy* strategy);   // null -> false
    void setStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, float value);   // all three statuses
    float getStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type) const;         // mean of the three
    void setStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, float value, StatusMap::Status status);
    float getStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, StatusMap::Status status) const;
    void setSpecificStrategicObjectModifier(int object, float value);
    float getSpecificStrategicObjectModifier(int object) const;   // 1.0f outside the vector
    const float* getStrategicObjectsModifiers(StatusMap::Status status) const;
    void clearAllStrategicObjectsModifiers();                  // 3 x 27 floats to 1.0f
    const std::string& getName() const;

    // ELF layout (sizeof 0x1A8 on ia-32). Names [INFERRED] from the accessors.
    /* +0x000 */ TemperatureTreeModifiers* m_treeModifiers;   // owned, new'ed by the constructor
    /* +0x004 */ float m_aggression;                         // 0.5f
    /* +0x008 */ unsigned int m_attacks;                     // not written by the constructor
    /* +0x00C */ unsigned int m_defences;
    /* +0x010 */ unsigned int m_attackConsiderations;
    /* +0x014 */ unsigned int m_defenceConsiderations;
    /* +0x018 */ float m_abortLevel;                         // 0.0f
    /* +0x01C */ float m_prerequisiteWeight;                 // 1.0f
    /* +0x020 */ float m_f20;                                // 1e9f [UNRESOLVED] no reader in Strategy.cpp
    /* +0x024 */ StrategyPrerequisite* m_prerequisite;       // 0; borrowed
    /* +0x028 */ StrategyPrerequisite* m_abortCondition;     // 0; borrowed
    /* +0x02C */ int m_i2C;                                  // not written by the constructor [UNRESOLVED]
    /* +0x030 */ std::set<Strategy*> m_requiredPreceding;
    /* +0x03C */ std::set<Strategy*> m_prohibitedPreceding;
    /* +0x048 */ float m_objectModifiers[3][27];             // [status][object type], stride 0x6C
    /* +0x18C */ std::vector<float> m_specificModifiers;     // per strategic-object index
    /* +0x198 */ std::string m_name;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGIC_STRATEGY_H
