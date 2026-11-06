// Owns strategies and their conditions.

#ifndef AI_AIDLL_AI_MAIN_STRATEGYMANAGER_H
#define AI_AIDLL_AI_MAIN_STRATEGYMANAGER_H

#include <list>
#include <map>
#include <string>

#include "AIDllTypes.h"
#include "../AIStrategic/StrategyCondition.h"   // the condition creators' enum parameters
#include "../AIStrategic/StatusMap.h"

namespace dice {
namespace hfe {
namespace ai {

class SCArea;
class Strategy;
class StrategyCondition;
class StrategyPrerequisite;

class StrategyManager
{
public:
    // Object, strategy and modifier association.
    class ObjectStrategyModifierTuple
    {
    public:
        // ELF 0x3E8F20 constructs these in order; names are [INFERRED].
        /* +0 */ std::string m_objectName;
        /* +4 */ Strategy* m_strategy;
        /* +8 */ float m_modifier;
    };

    StrategyManager();
    ~StrategyManager();                             // nonvirtual

    // The only virtual, vtable slot 0. ELF 0x3EA7D0 materializes `mov eax, 1` before its return,
    // so it is not void; its direct callers (the D1/D2 destructors, ELF 0x3E7F95 / 0x3E8185) drop
    // the value. The mangled name carries no return type: bool is [INFERRED] from the sibling
    // sole-virtual resets (AIConsole, AIStrategicObjectManager, AILandingZoneManager,
    // BotBehaviourManager), not from the PE.
    virtual bool reset();

    static StrategyManager* getInstance();
    static void setInstance(StrategyManager *);

    bool getEnableDebug();
    bool setEnableDebug(bool);
    bool isDebugEnabled();

    Strategy* getActiveStrategy();
    std::string getActiveStrategyName();            // , returns by value
    StrategyPrerequisite* getActivePrerequisite();
    StrategyCondition* getActiveCondition();
    Strategy* getForcedStrategy(int);

    std::string getActivePrerequisiteName();
    std::string getActiveConditionName();
    Strategy* getStrategy(std::string);
    StrategyCondition* getCondition(std::string);
    StrategyPrerequisite* getPrerequisite(std::string);
    Strategy* setActiveStrategy(std::string);
    StrategyPrerequisite* setActivePrerequisite(std::string);
    // The shipped setter dereferences the resulting iterator even when the name is absent.
    StrategyCondition* setActiveCondition(std::string);
    void setPrerequisite(std::string);
    std::string forceStrategy(std::string, int);    // ConsoleClass407 assigns the string
    bool createStrategy(std::string);               // ELF 0x3E8390: false when the name exists
    bool createPrerequisite(std::string);           // ELF 0x3E90C0: likewise
    void addCondition(std::string);
    void addCondition(std::string, float);
    bool addSpecificObjectModifier(std::string, float);   // ConsoleClass381 stores the bool
    bool addRequiredPrecedingStrategy(std::string);       // ConsoleClass376 stores the bool
    bool addProhibitedPrecedingStrategy(std::string);     // ConsoleClass377 stores the bool
    void completeStrategies();
    void completeConditions();
    // ConsoleClass368/388/394 assign the strings.
    std::string listStrategies();
    std::string listPrerequisites();
    std::string listConditions();

    // ELF 0x3E9850: false (and the active condition cleared) when the name exists.
    bool createConstantCondition(std::string name, StrategyCondition::CompareType compareType,
                                 StrategyCondition::Compare compare,
                                 StrategyCondition::ConditionSide side,
                                 StrategyCondition::Conditional conditional, float constant);
    // Each creator clears the active condition and returns false for a duplicate name.
    bool createHomogenousCondition(std::string name, StrategyCondition::CompareType compareType,
                                   StrategyCondition::Compare compare,
                                   StrategyCondition::Conditional conditional);
    bool createHeterogeneousCondition(std::string name, StrategyCondition::CompareType compareType,
                                      StrategyCondition::Compare compare,
                                      StrategyCondition::ConditionSide side,
                                      StrategyCondition::Conditional first,
                                      StrategyCondition::Conditional second);
    bool createAreaCondition(std::string name, std::string areaName, StatusMap::Status status);

    // the layout, from the 545-byte constructor cross-checked against the destructor
    // the three registries each come as A (name, active-pointer, map) triple, in the same order.
    /*  +4 */ std::string m_activeStrategyName;      // = "No Strategies"
    /*  +8 */ Strategy* m_activeStrategy;            // = 0
    /* +12 */ std::map<std::string, Strategy*> m_strategies;
    /* +24 */ std::list<ObjectStrategyModifierTuple> m_objectModifiers;
    /* +28 */ std::string m_activePrerequisiteName;  // = ""
    /* +32 */ StrategyPrerequisite* m_activePrerequisite;   // = 0
    /* +36 */ std::map<std::string, StrategyPrerequisite*> m_prerequisites;
    // Active condition iterator, initialized to end() and checked before dereference.
    /* +48 */ std::map<std::string, StrategyCondition*>::iterator m_activeCondition;
    /* +52 */ std::map<std::string, StrategyCondition*> m_conditions;
    /* +64 */ std::list<SCArea*> m_areas;
    /* +68 */ bool m_enableDebug;                    // = false
    // Forced strategy by team - 1. Callers must supply team 1 or 2; no bounds check is performed.
    /* +72 */ Strategy* m_forcedStrategy[2];         // = { 0, 0 }, indexed [team - 1]

    // first wins, and the destructor does test `== this`.
    static StrategyManager* m_instance;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_STRATEGYMANAGER_H
