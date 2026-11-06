// Bot Action Plan registry and AI commands, recovered from the symbolized libaidll.so.
// AI/Main/AIConsole.cpp is attested; this header path is [INFERRED].

#ifndef AI_AIDLL_AI_MAIN_AICONSOLE_H
#define AI_AIDLL_AI_MAIN_AICONSOLE_H

#include <map>
#include <string>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

class BAPEntry;
class Bot;
class IAIMain;
class ISensingAgentPattern;
class MapProperties;
class Strategy;

// Free enums in `dice::hfe::ai`, from the mangled parameter lists. [UNRESOLVED] enumerators.
enum BAPStatementType : int;
enum BotLodLevel : int;

class AIMain;   // init constructs it and caches the pointer at retail offset +60

// Owns 42 BAP primitives and four sensing patterns registered by the constructor.
// All four maps are case-sensitive (std::less<std::string>); preserve keys such as "Infantery".
// ABI: reset is the sole virtual (slot 0); the destructor is nonvirtual.
class AIConsole
{
public:
    AIConsole();
    // Required teardown: reset, delete SquadManager::m_instance, clear m_instance, then release
    // BAP entries, modifier arrays and sensing patterns. Preserve this cross-singleton order.
    ~AIConsole();                                   // ; nonvirtual

    // Retail reset deletes modifier arrays and strategies, retaining the BAP and sensing registries.
    virtual bool reset();                           // ; sole virtual, slot 0

    // Registry lookups.
    BAPEntry* getBAPEntryFromName(std::string);
    ISensingAgentPattern* getSensingAgentPatternFromName(std::string);

    // Bot management. init constructs AIMain, re-reads getActiveAIMain for its init call, then
    // constructs SquadManager. It returns void: the binding emits no result
    // and the two exits do not materialize a common return value.
    void init(int);
    int addBot(std::string, int, std::string, std::string);              // ConsoleClass186 stores the int
    int addUnspawnedBot(std::string, int, std::string);                  // ConsoleClass187 stores the int
    void updateBot(int);
    std::string killBotName(const std::string&);                         // ConsoleClass191 assigns the string
    std::string killBotId(int);                                          // ConsoleClass190 assigns the string
    void killAllBots();
    Bot* getBotFromId(int);
    void botMoveTo(float, Vec3, int);
    void botFlyTo(int, float, float, float, Vec3);
    // ConsoleClass196/197 pass a zero Vec3 for an omitted sixth argument [INFERRED spelling: value-initialised].
    void botSetWaypoints_Patrol(int, float, int, Vec3, Vec3, Vec3 = Vec3());
    void botSetWaypoints_Patrol3d(int, float, int, Vec3, Vec3, Vec3 = Vec3());
    void botGiveOrder(int, std::string, std::string);

    // Behaviors and modifier weights.
    void setBehaviour(int, std::string);
    std::string getBehaviour(int);
    void setVehicleDefaultBehaviour(std::string&, std::string&);
    void setVehicleBehaviour(std::string, std::string, std::string, std::string, int,
                             std::string, std::string);
    void setBasicBotWeights(std::string);
    void createBehaviourModifiers(std::string);
    void setActiveBehaviourModifiers(std::string);
    std::string getActiveBehaviourModifiers();
    float getBehaviourModifier(std::string);
    void setBehaviourModifier(std::string, float);
    std::string listBehaviourModifiers();                                // ConsoleClass310 assigns the string

    // Plan interpreter.
    void addInterpreterEntry(std::string, std::string);
    void addInterpreterEntry(std::string, std::string, BotLodLevel);
    void removeInterpreterEntry(std::string, std::string, BotLodLevel);
    void activateDefaultInterpreter(std::string, BAPStatementType, BotLodLevel);

    // Strategy, sensing and allowed areas.
    void addSAIStrategy(int, std::string);
    void addSensingAgentPattern(const std::string&, const std::string&);
    void setSensingAgentPatternName(const std::string&);
    void setSensingAgentNullPattern(const std::string&);
    void sensingAgentDebug();
    void addAllowedUnit(const std::string&);
    void addExpelledUnit(const std::string&);
    void addLandingZoneUnit(const std::string&);
    void setVehicleMods(const std::string&, const std::string&);
    void setStandardPersonality(const std::string&, int);

    // Camera, diagnostics and persistence.
    float getFastThreshold();
    void setFastThreshold(float);
    Vec3 getRendCameraPosition();                                   // ; retail returns zero
    Vec3 getRendCameraRotation();                                   // ; retail returns zero
    void freeCameraFire();
    void removeCameraText();                                        // ; retail no-op
    std::string showAISettings();                                        // ConsoleClass311 assigns the string
    std::string createLODManager(float, float);                         // ConsoleClass315 discards the string
    bool saveAll(const std::string&, const std::string&);                // ConsoleClass476 stores the bool
    void debug();                                                   // ; retail no-op
    float getCurrentPitchAngle() const;
    float getCurrentYawAngle() const;

    // Neither body uses this. Keep member declarations: getFastThreshold calls through an object;
    // identical-code folding prevents establishing staticness from these bodies alone.
    AIConsole* getSinglePlayerInstance();                           // ; retail returns null
    IAIMain* getActiveAIMain();                                     // ; reads IAIMain::m_instance

    // Retail ELF layout: , including four 12-byte libstdc++ 3.x trees. These offsets do
    // not constrain native STL layouts. Destructor instantiations establish the map value types.
    // Modifier values are owned float arrays released with delete[].
    /*  +4 */ std::map<std::string, float*> m_behaviourModifiers;   // [INFERRED] name
    // Cursor into the modifier map; retail teardown resets it to end after clearing that map.
    /* +16 */ std::map<std::string, float*>::iterator m_activeBehaviourModifiers;   // [INFERRED]
    /* +20 */ std::map<std::string, BAPEntry*> m_bapEntries;              // 42 owned primitives
    /* +32 */ std::map<std::string, Strategy*> m_strategies;              // [INFERRED] name
    /* +44 */ char m_gap44[4];      // [UNRESOLVED]; constructor zeroes this word
    /* +48 */ std::map<std::string, ISensingAgentPattern*> m_sensingAgentPatterns;  // 4 owned patterns
    // Retail construction allocates MapProperties and calls setMaxSlope, then loses the pointer.
    // init writes +60; getActiveAIMain reads the static instance instead. Other readers of this
    // cached pointer are [UNRESOLVED]; do not assume it is dead.
    /* +60 */ AIMain* m_aiMain;
    /* +64 */ char m_gap64[4];      // [UNRESOLVED]; unwritten by the constructor

    // Published by the constructor ( via GOT ), cleared by retail teardown.
    // BF2AI::clear deletes through this pointer.
    static AIConsole* m_instance;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AICONSOLE_H
