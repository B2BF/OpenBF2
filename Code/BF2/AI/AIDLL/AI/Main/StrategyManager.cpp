// static init: ELF 0x3EAC80 (__tcf_0 0x3EACE0, _GLOBAL__I 0x3EAD10): <iostream>'s Init object only.
#include <iostream>

#include "StrategyManager.h"

#include "../../../../Core/Debug.h"
#include "../AIUtils/checkPointer.h"
#include "../AIStrategic/Strategy.h"
#include "../AIStrategic/StrategyPrerequisite.h"
#include "../AIStrategic/SCConstant.h"
#include "../AIStrategic/SCHomogeneous.h"
#include "../AIStrategic/SCHeterogeneous.h"
#include "../AIStrategic/SCArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicEntity/AIStrategicArea.h"

namespace dice {
namespace hfe {
namespace ai {

StrategyManager* StrategyManager::m_instance = 0;

// ELF 0x3E7A60 / 0x3E7D40. Initializes collections and installs the first singleton.
StrategyManager::StrategyManager()
    : m_activeStrategy(0)
    , m_activePrerequisite(0)
    , m_enableDebug(false)
{
    m_forcedStrategy[0] = 0;
    m_forcedStrategy[1] = 0;

    m_activeCondition = m_conditions.end();
    m_activeStrategyName.assign("No Strategies");

    if (!m_instance)
        m_instance = this;
}

// ELF 0x3E7F70 / 0x3E8160. Resets state and releases the owned collections.
StrategyManager::~StrategyManager()
{
    reset();

    if (m_instance == this)
        m_instance = 0;
}

// ELF 0x3E8350. Static access to the manager, whose destructor is nonvirtual.
StrategyManager* StrategyManager::getInstance()
{
    return m_instance;
}

// ELF 0x3E8370.
void StrategyManager::setInstance(StrategyManager* instance)
{
    m_instance = instance;
}

// ELF 0x3EA7B0.
bool StrategyManager::getEnableDebug()
{
    return m_enableDebug;
}

// ELF 0x3EA790. The stored byte is explicitly returned in EAX.
bool StrategyManager::setEnableDebug(bool on)
{
    m_enableDebug = on;
    return on;
}

// ELF 0x3E8730.
Strategy* StrategyManager::getActiveStrategy()
{
    return m_activeStrategy;
}

// ELF 0x3E8740. The shipped function has a hidden return slot, i.e. it copies the
// member rather than handing back a reference to it.
std::string StrategyManager::getActiveStrategyName()
{
    return m_activeStrategyName;
}

// ELF 0x3E9410.
StrategyPrerequisite* StrategyManager::getActivePrerequisite()
{
    return m_activePrerequisite;
}

// ELF 0x3EA3B0. A missing active condition returns null.
StrategyCondition* StrategyManager::getActiveCondition()
{
    if (m_activeCondition == m_conditions.end())
        return 0;
    return m_activeCondition->second;
}

// ELF 0x3EA770. 1-based and unclamped. `team == 0` reads the dword that
// holds `m_enableDebug`.
Strategy* StrategyManager::getForcedStrategy(int team)
{
    return m_forcedStrategy[team - 1];
}

// ELF 0x3EA7D0 (1192 B; recovered on the client track, docs/macos-c-singleplayer.md). Clears the
// forced strategies and the debug flag, then empties the three owning maps one node at a time:
// value deleted first (Strategy and StrategyPrerequisite through their nonvirtual destructors,
// conditions through the virtual deleting destructor, vtable +4), then the node erased. The
// active name/pointer pairs are reset from a temporary {name, 0}; the shipped strategy text is
// "No Strategiws" (sic, the constructor says "No Strategies"). The modifier and area lists are
// left alone. Returns 1 (`mov eax, 1` at 0x3EAB7C).
bool StrategyManager::reset()
{
    m_forcedStrategy[0] = 0;                                                  // +0x48
    m_forcedStrategy[1] = 0;                                                  // +0x4C
    m_enableDebug = false;                                                    // +0x44

    std::map<std::string, Strategy*>::iterator strategy = m_strategies.begin();
    while (strategy != m_strategies.end())
    {
        delete strategy->second;
        m_strategies.erase(strategy++);
    }
    m_activeStrategyName = std::string("No Strategiws");                     // +0x04
    m_activeStrategy = 0;                                                     // +0x08

    std::map<std::string, StrategyPrerequisite*>::iterator prerequisite = m_prerequisites.begin();
    while (prerequisite != m_prerequisites.end())
    {
        delete prerequisite->second;
        m_prerequisites.erase(prerequisite++);
    }
    m_activePrerequisiteName = std::string("No Prerequisites");              // +0x1C
    m_activePrerequisite = 0;                                                 // +0x20

    std::map<std::string, StrategyCondition*>::iterator condition = m_conditions.begin();
    while (condition != m_conditions.end())
    {
        delete condition->second;                                             // virtual, slot +4
        m_conditions.erase(condition++);
    }
    m_activeCondition = m_conditions.end();                                   // +0x30
    return true;
}

// ELF 0x3EA7C0: `movzbl 0x44(this)`, the same body as getEnableDebug.
bool StrategyManager::isDebugEnabled()
{
    return m_enableDebug;
}

// ELF 0x3E9420: a copy of the +0x1C string.
std::string StrategyManager::getActivePrerequisiteName()
{
    return m_activePrerequisiteName;
}

// ELF 0x3EA3D0: the active condition's key, or "" (the empty tail of a string literal at
// ELF GOT-0xFD12) while the iterator is end().
std::string StrategyManager::getActiveConditionName()
{
    if (m_activeCondition == m_conditions.end())
        return "";
    return m_activeCondition->first;
}

// ELF 0x3E8930: map find, null when absent.
Strategy* StrategyManager::getStrategy(std::string name)
{
    const std::map<std::string, Strategy*>::iterator it = m_strategies.find(name);
    return it == m_strategies.end() ? 0 : it->second;
}

// ELF 0x3EA4C0: map find, null when absent.
StrategyCondition* StrategyManager::getCondition(std::string name)
{
    const std::map<std::string, StrategyCondition*>::iterator it = m_conditions.find(name);
    return it == m_conditions.end() ? 0 : it->second;
}

// ELF 0x3E9610: map find, null when absent.
StrategyPrerequisite* StrategyManager::getPrerequisite(std::string name)
{
    const std::map<std::string, StrategyPrerequisite*>::iterator it = m_prerequisites.find(name);
    return it == m_prerequisites.end() ? 0 : it->second;
}

// ELF 0x3E8780. Both images explicitly return the selected pointer.
Strategy* StrategyManager::setActiveStrategy(std::string name)
{
    const std::map<std::string, Strategy*>::iterator it = m_strategies.find(name);
    const std::pair<std::string, Strategy*> active = it == m_strategies.end()
        ? std::pair<std::string, Strategy*>("NULL", 0)
        : std::pair<std::string, Strategy*>(it->first, it->second);
    m_activeStrategyName = active.first;
    m_activeStrategy = active.second;
    return m_activeStrategy;
}

// ELF 0x3E9460.
StrategyPrerequisite* StrategyManager::setActivePrerequisite(std::string name)
{
    const std::map<std::string, StrategyPrerequisite*>::iterator it = m_prerequisites.find(name);
    const std::pair<std::string, StrategyPrerequisite*> active = it == m_prerequisites.end()
        ? std::pair<std::string, StrategyPrerequisite*>("", 0)
        : std::pair<std::string, StrategyPrerequisite*>(it->first, it->second);
    m_activePrerequisiteName = active.first;
    m_activePrerequisite = active.second;
    return m_activePrerequisite;
}

// ELF 0x3EA440.
StrategyCondition* StrategyManager::setActiveCondition(std::string name)
{
    m_activeCondition = m_conditions.find(name);
    // shipped defect: a missing condition leaves end(), which is still dereferenced.
    return m_activeCondition->second;
}

// ELF 0x3E8A50: the named prerequisite (null when unknown) goes to the active strategy; with
// none an Error debug (line 182) is raised.
void StrategyManager::setPrerequisite(std::string name)
{
    if (!m_activeStrategy)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/Main/StrategyManager.cpp", 182, "")
            << "Cannot set prerequisite " << name << " - there is no active strategy.";
        return;
    }
    m_activeStrategy->setPrerequisite(getPrerequisite(name));
}

// ELF 0x3EA650.
std::string StrategyManager::forceStrategy(std::string name, int side)
{
    std::string result;
    if (side != 1 && side != 2)
    {
        result += "Side must be 1 or 2, current side: ";
        // shipped defect: append the side as one character, rather than a decimal number.
        result += static_cast<char>(side);
        return result;
    }
    const std::map<std::string, Strategy*>::iterator it = m_strategies.find(name);
    if (it == m_strategies.end())
    {
        m_forcedStrategy[side - 1] = 0;
        result += "No active strategy chosen : ";
    }
    else
    {
        m_forcedStrategy[side - 1] = it->second;
        result += "Strategy is activated : ";
    }
    result += name;
    return result;
}

// ELF 0x3E8390. An existing name clears the active strategy (name "NULL") and answers false;
// otherwise the new strategy is registered and becomes active.
bool StrategyManager::createStrategy(std::string name)
{
    if (m_strategies.find(name) != m_strategies.end())
    {
        m_activeStrategyName.assign("NULL");
        m_activeStrategy = 0;
        return false;
    }
    Strategy* const strategy = checkPointer(new Strategy(name));
    m_strategies[name] = strategy;
    m_activeStrategyName = name;
    m_activeStrategy = strategy;
    return true;
}

// ELF 0x3E90C0: the createStrategy shape for prerequisites; an existing name answers false and
// leaves the active prerequisite as it is.
bool StrategyManager::createPrerequisite(std::string name)
{
    if (m_prerequisites.find(name) != m_prerequisites.end())
        return false;
    StrategyPrerequisite* const prerequisite = checkPointer(new StrategyPrerequisite(name));
    m_prerequisites[name] = prerequisite;
    m_activePrerequisiteName = name;
    m_activePrerequisite = prerequisite;
    return true;
}

// ELF 0x3E97C0: weight 1.0f (ELF 0x49CB2C). Neither overload tests the active prerequisite.
void StrategyManager::addCondition(std::string name)
{
    m_activePrerequisite->addCondition(getCondition(name), 1.0f);
}

// ELF 0x3E9730.
void StrategyManager::addCondition(std::string name, float weight)
{
    m_activePrerequisite->addCondition(getCondition(name), weight);
}

// ELF 0x3E8F20. The active strategy is recorded unchecked and resolved against an area later.
bool StrategyManager::addSpecificObjectModifier(std::string name, float modifier)
{
    ObjectStrategyModifierTuple tuple;
    tuple.m_objectName = name;
    tuple.m_strategy = m_activeStrategy;
    tuple.m_modifier = modifier;
    m_objectModifiers.insert(m_objectModifiers.end(), tuple);
    return true;
}

// ELF 0x3E8BE0.
bool StrategyManager::addRequiredPrecedingStrategy(std::string name)
{
    if (!m_activeStrategy)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/Main/StrategyManager.cpp", 193, "")
            << "Cannot add required preceding strategy " << name << " - there is no active strategy.";
        return false;
    }
    return m_activeStrategy->addRequiredPrecedingStrategy(getStrategy(name));
}

// ELF 0x3E8D80.
bool StrategyManager::addProhibitedPrecedingStrategy(std::string name)
{
    if (!m_activeStrategy)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/Main/StrategyManager.cpp", 204, "")
            << "Cannot add prohibited preceding strategy " << name << " - there is no active strategy.";
        return false;
    }
    return m_activeStrategy->addProhibitedPrecedingStrategy(getStrategy(name));
}

// ELF 0x3E8FD0. Retains the tuples and leaves the last area selected.
void StrategyManager::completeStrategies()
{
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    for (std::list<ObjectStrategyModifierTuple>::iterator it = m_objectModifiers.begin();
         it != m_objectModifiers.end(); ++it)
    {
        manager->setActive(it->m_objectName);
        AIStrategicArea* const area = manager->getActiveArea();
        if (area)
            it->m_strategy->setSpecificStrategicObjectModifier(area->m_index, it->m_modifier);
    }
}

// ELF 0x3EA5E0. The singleton lookup is retained even for an empty list.
void StrategyManager::completeConditions()
{
    AIStrategicObjectManager::getInstance();
    for (std::list<SCArea*>::iterator it = m_areas.begin(); it != m_areas.end(); ++it)
        (*it)->updateArea();
}

// ELF 0x3E89A0.
std::string StrategyManager::listStrategies()
{
    std::string result;
    for (std::map<std::string, Strategy*>::iterator it = m_strategies.begin();
         it != m_strategies.end(); ++it)
    {
        result += it->first;
        result += "\n";
    }
    return result;
}

// ELF 0x3E9680.
std::string StrategyManager::listPrerequisites()
{
    std::string result;
    for (std::map<std::string, StrategyPrerequisite*>::iterator it = m_prerequisites.begin();
         it != m_prerequisites.end(); ++it)
    {
        result += it->first;
        result += "\n";
    }
    return result;
}

// ELF 0x3EA530.
std::string StrategyManager::listConditions()
{
    std::string result;
    for (std::map<std::string, StrategyCondition*>::iterator it = m_conditions.begin();
         it != m_conditions.end(); ++it)
    {
        result += it->first;
        result += "\n";
    }
    return result;
}

// ELF 0x3E9850. The target value starts at 0.0f (ELF 0x49CB34); a new condition becomes the
// active one (the insert's iterator).
bool StrategyManager::createConstantCondition(std::string name,
                                              StrategyCondition::CompareType compareType,
                                              StrategyCondition::Compare compare,
                                              StrategyCondition::ConditionSide side,
                                              StrategyCondition::Conditional conditional,
                                              float constant)
{
    if (m_conditions.find(name) != m_conditions.end())
    {
        m_activeCondition = m_conditions.end();
        return false;
    }
    StrategyCondition* const condition = checkPointer(
        new SCConstant(name, compareType, compare, side, conditional, constant, 0.0f));
    m_activeCondition =
        m_conditions.insert(std::pair<const std::string, StrategyCondition*>(name, condition)).first;
    return true;
}

// ELF 0x3E9B20
bool StrategyManager::createHomogenousCondition(std::string name,
                                               StrategyCondition::CompareType compareType,
                                               StrategyCondition::Compare compare,
                                               StrategyCondition::Conditional conditional)
{
    if (m_conditions.find(name) != m_conditions.end())
    {
        m_activeCondition = m_conditions.end();
        return false;
    }
    StrategyCondition* const condition = checkPointer(
        new SCHomogeneous(name, compareType, compare, conditional, 0.0f));
    m_activeCondition =
        m_conditions.insert(std::pair<const std::string, StrategyCondition*>(name, condition)).first;
    return true;
}

// ELF 0x3E9DE0
bool StrategyManager::createHeterogeneousCondition(std::string name,
                                                  StrategyCondition::CompareType compareType,
                                                  StrategyCondition::Compare compare,
                                                  StrategyCondition::ConditionSide side,
                                                  StrategyCondition::Conditional first,
                                                  StrategyCondition::Conditional second)
{
    if (m_conditions.find(name) != m_conditions.end())
    {
        m_activeCondition = m_conditions.end();
        return false;
    }
    StrategyCondition* const condition = checkPointer(
        new SCHeterogeneous(name, compareType, compare, side, first, second, 0.0f));
    m_activeCondition =
        m_conditions.insert(std::pair<const std::string, StrategyCondition*>(name, condition)).first;
    return true;
}

// ELF 0x3EA0B0
bool StrategyManager::createAreaCondition(std::string name, std::string areaName,
                                         StatusMap::Status status)
{
    if (m_conditions.find(name) != m_conditions.end())
    {
        m_activeCondition = m_conditions.end();
        return false;
    }
    SCArea* const condition = checkPointer(new SCArea(name, areaName, -1, status));
    m_activeCondition =
        m_conditions.insert(std::pair<const std::string, StrategyCondition*>(name, condition)).first;
    m_areas.insert(m_areas.end(), condition);
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
