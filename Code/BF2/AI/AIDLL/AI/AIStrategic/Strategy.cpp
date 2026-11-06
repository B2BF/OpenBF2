// Strategy. Evidence: libaidll.so (ia-32) 0x3330A0..0x333DC0.

#include "Strategy.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF 0x333DD0,
// __tcf_0 0x333E30, _GLOBAL__I 0x333E60).
#include <iostream>

#include "../Main/StrategyManager.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x3330A0. The counts and +0x2C are left as allocated. The specific-modifier vector is
// resized to 0 (with fill 0.0f) after the object modifiers are reset.
Strategy::Strategy(const std::string& name)
    : m_treeModifiers(new TemperatureTreeModifiers()),
      m_aggression(0.5f),
      m_abortLevel(0.0f),
      m_prerequisiteWeight(1.0f),
      m_f20(1e9f),
      m_prerequisite(0),
      m_abortCondition(0),
      m_name(name)
{
    clearAllStrategicObjectsModifiers();
    m_specificModifiers.resize(0, 0.0f);
}

// ELF 0x333460: only the tree modifiers are owned (operator delete, no destructor call);
// prerequisite and abort condition belong to the StrategyManager.
Strategy::~Strategy()
{
    delete m_treeModifiers;
}

TemperatureTreeModifiers* Strategy::getTemperatureTreeModifiers() const
{
    return m_treeModifiers;
}

void Strategy::setTemperatureTreeModifiers(TemperatureTreeModifiers* modifiers)
{
    m_treeModifiers = modifiers;
}

float Strategy::getAggression() const
{
    return m_aggression;
}

void Strategy::setAggression(float aggression)
{
    m_aggression = aggression;
}

unsigned int Strategy::getNumberOfAttacks() const
{
    return m_attacks;
}

void Strategy::setNumberOfAttacks(unsigned int count)
{
    m_attacks = count;
}

unsigned int Strategy::getNumberOfAttackConsiderations() const
{
    return m_attackConsiderations;
}

void Strategy::setNumberOfAttackConsiderations(unsigned int count)
{
    m_attackConsiderations = count;
}

unsigned int Strategy::getNumberOfDefences() const
{
    return m_defences;
}

void Strategy::setNumberOfDefences(unsigned int count)
{
    m_defences = count;
}

unsigned int Strategy::getNumberOfDefenceConsiderations() const
{
    return m_defenceConsiderations;
}

void Strategy::setNumberOfDefenceConsiderations(unsigned int count)
{
    m_defenceConsiderations = count;
}

void Strategy::setPrerequisite(StrategyPrerequisite* prerequisite)
{
    m_prerequisite = prerequisite;
}

void Strategy::setAbortCondition(StrategyPrerequisite* condition)
{
    m_abortCondition = condition;
}

// ELF 0x3337C0: 0 when a required predecessor list exists and does not contain previous, or
// when previous is prohibited; otherwise the weighted prerequisite score. The prerequisite is
// not null-checked.
float Strategy::calculatePrerequisite(float* friendly, float* enemy, Strategy* previous, int side)
{
    StrategyManager::getInstance()->isDebugEnabled();   // empty debug branch in the image
    if (!m_requiredPreceding.empty() && m_requiredPreceding.find(previous) == m_requiredPreceding.end())
        return 0.0f;
    if (!m_prohibitedPreceding.empty() && m_prohibitedPreceding.find(previous) != m_prohibitedPreceding.end())
        return 0.0f;
    return m_prerequisite->evaluate(friendly, enemy, previous == this, side) * m_prerequisiteWeight;
}

// ELF 0x333890: the abort level minus the abort condition's score (fsubrs +0x18).
float Strategy::calculateAbortCondition(float* friendly, float* enemy, Strategy* previous, int side)
{
    return m_abortLevel - m_abortCondition->evaluate(friendly, enemy, previous == this, side);
}

float Strategy::setAbortLevel(float level)
{
    return m_abortLevel = level;
}

float Strategy::getAbortLevel() const
{
    return m_abortLevel;
}

float Strategy::setPrerequisiteWeight(float weight)
{
    return m_prerequisiteWeight = weight;
}

float Strategy::getPrerequisiteWeight() const
{
    return m_prerequisiteWeight;
}

// ELF 0x333930: nodes above 40 are ignored (unsigned compare).
void Strategy::setTreeModifier(TemperatureTreeNode node, float value)
{
    if (static_cast<unsigned int>(node) <= 40)
        m_treeModifiers->m_nodeModifiers[node] = value;
}

float Strategy::getTreeModifier(TemperatureTreeNode node) const
{
    return m_treeModifiers->m_nodeModifiers[node];
}

// ELF 0x333960. Types outside 0..5 are ignored; the image still carries a type-6 arm that
// sets all six types, unreachable behind the range check, kept as shipped.
void Strategy::setTreeModifier(TemperatureTreeNode node, CombatStrengthType type, float value)
{
    if (static_cast<unsigned int>(node) > 40 || type < 0 || type > 5)
        return;
    if (type == 6)
    {
        for (int i = 0; i <= 5; ++i)
            m_treeModifiers->m_combatStrengthModifiers[node][i] = value;
        return;
    }
    m_treeModifiers->m_combatStrengthModifiers[node][type] = value;
}

float Strategy::getTreeModifier(TemperatureTreeNode node, CombatStrengthType type) const
{
    return m_treeModifiers->m_combatStrengthModifiers[node][type];
}

// ELF 0x3339F0: the same body and the same array as the three-argument setTreeModifier.
void Strategy::setTreeModifierCombatStrength(TemperatureTreeNode node, CombatStrengthType type, float value)
{
    if (static_cast<unsigned int>(node) > 40 || type < 0 || type > 5)
        return;
    if (type == 6)
    {
        for (int i = 0; i <= 5; ++i)
            m_treeModifiers->m_combatStrengthModifiers[node][i] = value;
        return;
    }
    m_treeModifiers->m_combatStrengthModifiers[node][type] = value;
}

float Strategy::getTreeModifierCombatStrength(TemperatureTreeNode node, CombatStrengthType type) const
{
    return m_treeModifiers->m_combatStrengthModifiers[node][type];
}

bool Strategy::addRequiredPrecedingStrategy(Strategy* strategy)
{
    if (!strategy)
        return false;
    return m_requiredPreceding.insert(strategy).second;
}

bool Strategy::addProhibitedPrecedingStrategy(Strategy* strategy)
{
    if (!strategy)
        return false;
    return m_prohibitedPreceding.insert(strategy).second;
}

void Strategy::setStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, float value)
{
    for (int status = 0; status < 3; ++status)
        m_objectModifiers[status][type] = value;
}

// ELF 0x333B70: the mean over the three statuses, summed from 0.0f.
float Strategy::getStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type) const
{
    float sum = 0.0f;
    for (int status = 0; status < 3; ++status)
        sum += m_objectModifiers[status][type];
    return sum / 3.0f;
}

void Strategy::setStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, float value, StatusMap::Status status)
{
    m_objectModifiers[status][type] = value;
}

float Strategy::getStrategicObjectsModifier(AIStrategicObject::StrategicObjectType type, StatusMap::Status status) const
{
    return m_objectModifiers[status][type];
}

// ELF 0x333C20: negative indices are ignored; the vector grows with 1.0f to cover the index.
void Strategy::setSpecificStrategicObjectModifier(int object, float value)
{
    if (object < 0)
        return;
    if (object >= static_cast<int>(m_specificModifiers.size()))
        m_specificModifiers.resize(object + 1, 1.0f);
    m_specificModifiers[object] = value;
}

float Strategy::getSpecificStrategicObjectModifier(int object) const
{
    if (object >= 0 && object < static_cast<int>(m_specificModifiers.size()))
        return m_specificModifiers[object];
    return 1.0f;
}

const float* Strategy::getStrategicObjectsModifiers(StatusMap::Status status) const
{
    return m_objectModifiers[status];
}

// ELF 0x333D70.
void Strategy::clearAllStrategicObjectsModifiers()
{
    for (int type = 0; type <= 26; ++type)
        for (int status = 0; status < 3; ++status)
            m_objectModifiers[status][type] = 1.0f;
}

const std::string& Strategy::getName() const
{
    return m_name;
}

} // namespace ai
} // namespace hfe
} // namespace dice
