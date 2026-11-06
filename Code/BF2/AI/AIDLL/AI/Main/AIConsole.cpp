// static init: ELF 0x3969C0 (__tcf_0 0x396A20, _GLOBAL__I 0x396A50): <iostream>'s Init object only.
#include <iostream>

#include "AIConsole.h"

#include "../AIBot/SensingAgentPatterns.h"     // the 4 registered patterns
#include "../AIBot/SquadManager.h"               // init's second allocation
#include "../AIUtils/checkPointer.h"             // init's two dead null checks
#include "../AIPlanInterpreter/BAPEntries.h"   // the 42 registered primitives
#include "../AIBot/BotMain.h"
#include "../AIBot/BotOrder.h"
#include "../AIBot/SensingAgent.h"
#include "../AIBot/WPMoveTo.h"
#include "../AIBot/WPAltitudeMoveTo.h"
#include "../AIBot/WPPatrol.h"
#include "../AIBot/WPAltitudePatrol.h"
#include "../AIKnowledge/Information.h"
#include "InformationManager.h"
#include "../AIPathFinding/NavMesh/MapProperties.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../AIStrategicEntity/AIStrategicObjectManager.h"
#include "../AIStrategicEntity/AILandingZone.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/BFGameEffector.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../../Core/Debug.h"
#include "../../../../Core/StringUtil.h"
#include "../../../../IO/File/File.h"
#include "../../../../Math/Mat4.h"
#include "IBotManager.h"
#include "AILODManager.h"
#include "IAIPathfinding.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include "AIMain.h"                                // IAIMain::m_instance
#include "CommonSingletonServer.h"
#include "AISettings.h"                                  // IAISettings / IAISettingsModifier
#include "AIStreamOperators.h"                           // the script-vocabulary extractors
#include "../AIBotBehaviour/BotBehaviour.h"              // setVehicleBehaviour's result
#include "BotBehaviourManager.h"                         // the behaviour commands
#include "StrategyManager.h"                             // the aiStrategy bindings
#include "../AIStrategic/SAI.h"                          // addSAIStrategy
#include "../AIStrategic/Strategy.h"

namespace dice {
namespace hfe {
namespace ai {

AIConsole* AIConsole::m_instance = 0;

// ELF 0x383470 / 0x389360. Registers Bot Action Plan primitives and sensing patterns.
AIConsole::AIConsole()
    : m_activeBehaviourModifiers(), m_gap44()
{
    // Publishes this console as the active singleton.
    m_instance = this;

    // the forty-two BAP primitives, in the binary's registration order
    m_bapEntries["TankMoveTo"] = new EntryTankMoveTo();
    m_bapEntries["TankMoveToObject"] = new EntryTankMoveToObject();
    m_bapEntries["TankMoveToDirection"] = new EntryTankMoveToDirection();
    m_bapEntries["TankResetControls"] = new EntryTankResetControls();
    m_bapEntries["TankTurnTo"] = new EntryTankTurnTo();
    m_bapEntries["CarMoveTo"] = new EntryCarMoveTo();
    m_bapEntries["CarMoveToDirection"] = new EntryCarMoveToDirection();
    m_bapEntries["CarMoveToObject"] = new EntryCarMoveToObject();
    m_bapEntries["CarResetControls"] = new EntryCarResetControls();
    m_bapEntries["PlaneMoveTo"] = new EntryPlaneMoveTo();
    m_bapEntries["PlaneMoveToDirection"] = new EntryPlaneMoveToDirection();
    m_bapEntries["PlaneMoveToObject"] = new EntryPlaneMoveToObject();
    m_bapEntries["PlaneAimAt"] = new EntryPlaneAimAt();
    m_bapEntries["PlaneResetControls"] = new EntryPlaneResetControls();
    m_bapEntries["BoatMoveTo"] = new EntryBoatMoveTo();
    m_bapEntries["BoatMoveToObject"] = new EntryBoatMoveToObject();
    m_bapEntries["BoatMoveToDirection"] = new EntryBoatMoveToDirection();
    m_bapEntries["BoatResetControls"] = new EntryBoatResetControls();
    m_bapEntries["InfanteryMoveTo"] = new EntryInfanteryMoveTo();
    m_bapEntries["InfanteryMoveToObject"] = new EntryInfanteryMoveToObject();
    m_bapEntries["InfanteryMoveToDirection"] = new EntryInfanteryMoveToDirection();
    m_bapEntries["InfanteryResetControls"] = new EntryInfanteryResetControls();
    m_bapEntries["Trigger"] = new EntryTrigger();
    m_bapEntries["TriggerContinously"] = new EntryTriggerContinously();
    m_bapEntries["KeyTurretAimAt"] = new EntryKeyTurretAimAt();
    m_bapEntries["MouseTurretAimAt"] = new EntryMouseTurretAimAt();
    m_bapEntries["MouseTurretLookAt"] = new EntryMouseTurretLookAt();
    m_bapEntries["Sense"] = new EntrySense();
    m_bapEntries["SoldierPose"] = new EntrySoldierPose();
    m_bapEntries["MoveToMedium"] = new EntryMoveToMedium();
    m_bapEntries["MoveToMediumBoat"] = new EntryMoveToMediumBoat();
    m_bapEntries["MoveToMediumSoldier"] = new EntryMoveToMediumSoldier();
    m_bapEntries["MoveToObjectMedium"] = new EntryMoveToObjectMedium();
    m_bapEntries["MoveToObjectMediumBoat"] = new EntryMoveToObjectMediumBoat();
    m_bapEntries["MoveToObjectMediumSoldier"] = new EntryMoveToObjectMediumSoldier();
    m_bapEntries["InfoWrapper"] = new EntryInfoWrapper();
    m_bapEntries["HeliMoveTo"] = new EntryHeliMoveTo();
    m_bapEntries["HeliMoveToDirection"] = new EntryHeliMoveToDirection();
    m_bapEntries["HeliMoveToObject"] = new EntryHeliMoveToObject();
    m_bapEntries["HeliAimAt"] = new EntryHeliAimAt();
    m_bapEntries["HeliResetControls"] = new EntryHeliResetControls();
    m_bapEntries["HeliMoveAwayFromObject"] = new EntryHeliMoveAwayFromObject();

    // the four sensing-agent patterns
    m_sensingAgentPatterns["Turret"] = new SensingAgentTurretPattern();
    m_sensingAgentPatterns["MouseFreeLook"] = new SensingAgentMouseFreeLookPattern();
    m_sensingAgentPatterns["Soldier"] = new SensingAgentSoldierPattern();
    m_sensingAgentPatterns["Null"] = new SensingAgentNullPattern();

    m_activeBehaviourModifiers = m_behaviourModifiers.end();
    m_aiMain = 0;
    // shipped defect: this allocation is never stored or released.
    MapProperties* const properties = new MapProperties();
    properties->setMaxSlope(0.36651915311813354f);
}

// ELF 0x38F5C0 (D1, 1047 B; recovered on the client track, docs/macos-c-singleplayer.md).
// reset, then the SquadManager singleton is deleted and this singleton zeroed unconditionally.
// The BAP entries are deleted through their virtual deleting destructor (+4) and erased one by
// one; the modifier arrays are freed with delete[] but their nodes stay (only the cursor is
// moved to end). The sensing-pattern loop advances twice per pass - `++it` in the loop header on
// top of `erase(it++)` - so it deletes the 1st and 3rd of the four patterns and leaves the 2nd
// and 4th to the map's own destructor, which frees their nodes without deleting them. Shipped
// (0x38F774 and 0x38F7DF are both _M_increment); with the four patterns the constructor
// registers, the iterator never passes end().
AIConsole::~AIConsole()
{
    reset();
    if (SquadManager::m_instance)
        delete SquadManager::m_instance;
    m_instance = 0;

    std::map<std::string, BAPEntry*>::iterator entry = m_bapEntries.begin();
    while (entry != m_bapEntries.end())
    {
        delete entry->second;                                   // virtual, slot +4
        m_bapEntries.erase(entry++);
    }

    for (std::map<std::string, float*>::iterator modifier = m_behaviourModifiers.begin();
         modifier != m_behaviourModifiers.end(); ++modifier)
        delete[] modifier->second;
    m_activeBehaviourModifiers = m_behaviourModifiers.end();    // +0x10

    for (std::map<std::string, ISensingAgentPattern*>::iterator pattern = m_sensingAgentPatterns.begin();
         pattern != m_sensingAgentPatterns.end(); ++pattern)
    {
        delete pattern->second;                                 // virtual, slot +4
        m_sensingAgentPatterns.erase(pattern++);
    }
}

// ELF 0x395F60 (528 B; recovered on the client track). Frees every behaviour-modifier array
// (delete[]) and erases its node, then deletes every Strategy (nonvirtual D1 + operator delete)
// and erases its node. The BAP and sensing registries and the modifier cursor are left alone.
// Returns 1.
bool AIConsole::reset()
{
    std::map<std::string, float*>::iterator modifier = m_behaviourModifiers.begin();
    while (modifier != m_behaviourModifiers.end())
    {
        delete[] modifier->second;
        m_behaviourModifiers.erase(modifier++);
    }
    std::map<std::string, Strategy*>::iterator strategy = m_strategies.begin();
    while (strategy != m_strategies.end())
    {
        delete strategy->second;
        m_strategies.erase(strategy++);
    }
    return true;
}

// ELF 0x383460. The dedicated image has no single-player instance.
AIConsole* AIConsole::getSinglePlayerInstance()
{
    return 0;
}

// ELF 0x38F9E0.
IAIMain* AIConsole::getActiveAIMain()
{
    return IAIMain::m_instance;
}

// ELF 0x396170. Empty in the dedicated image.
void AIConsole::debug()
{
}

// ELF 0x3934B0. Empty in the dedicated image.
void AIConsole::removeCameraText()
{
}

// ELF 0x392B40. A zeroed Vec3, by value.
Vec3 AIConsole::getRendCameraPosition()
{
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    return position;
}

// ELF 0x392B70.
Vec3 AIConsole::getRendCameraRotation()
{
    Vec3 rotation;
    rotation.x = 0.0f;
    rotation.y = 0.0f;
    rotation.z = 0.0f;
    return rotation;
}

// ELF 0x392610: `m_bapEntries[name]` (lower_bound + insert_unique: an unknown name is inserted
// null, case-sensitive).
BAPEntry* AIConsole::getBAPEntryFromName(std::string name)
{
    return m_bapEntries[name];
}

// ELF 0x3927B0: `m_sensingAgentPatterns[name]` (operator[]: an unknown name is inserted null).
ISensingAgentPattern* AIConsole::getSensingAgentPatternFromName(std::string name)
{
    return m_sensingAgentPatterns[name];
}

// ELF 0x38FA00. Initializes AI from a level script, unless a runtime already exists.
void AIConsole::init(int n)
{
    if (getActiveAIMain())
        return;

    AIMain* const aiMain = new AIMain();
    checkPointer(aiMain);

    m_aiMain = aiMain;

    // The second lookup is retail's - see above. The result is discarded.
    static_cast<AIMain*>(getActiveAIMain())->init(n);

    SquadManager* const squadManager = new SquadManager();
    checkPointer(squadManager);

    // SquadManager registers itself as a singleton; AIConsole owns it through that pointer.
    (void)squadManager;
}

// ELF 0x38FCF0. Unknown modifier names are inserted with a null value.
int AIConsole::addBot(std::string name, int side, std::string modifiers, std::string equipment)
{
    if (!IAIMain::m_instance || static_cast<unsigned int>(side) > 2)
        return -1;
    IAIObject* object = 0;
    if (equipment != "NoEquipment")
    {
        object = static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
            ->getObjectFromName(equipment);
        if (!object)
            return -1;
    }
    const int playerId = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
        ->getPlayerIdFromName(name);
    if (playerId == -1)
        return -1;
    float* const weights = m_behaviourModifiers[modifiers];
    if (!weights || !static_cast<AIMain*>(IAIMain::m_instance)->addBot(playerId, side, weights, object))
        return -1;
    return IBotManager::instance->getBotCount() - 1;
}

// ELF 0x38FF90. The shipped command refuses the operation with an Error diagnostic.
int AIConsole::addUnspawnedBot(std::string, int, std::string)
{
    Debug(DebugTypeError, "AI/AIDLL/AI/Main/AIConsole.cpp", 363, "") << "Does not work";
    return -1;
}

// ELF 0x3900A0.
void AIConsole::updateBot(int id)
{
    if (!getActiveAIMain())
        return;
    Bot* const bot = getBotFromId(id);
    if (bot)
        bot->updateBot();
}

// ELF 0x390100. The effector call is respawnBot, despite the command's name.
std::string AIConsole::killBotName(const std::string& name)
{
    std::stringstream out;
    if (!IBotManager::instance)
    {
        out << "No active BotManager found.";
        return out.str();
    }
    Bot* const bot = IBotManager::instance->getBotFromName(name);
    if (!bot)
        out << "Failed to find bot with name: " << name << ". Check caps.";
    else if (!bot->isDead())
    {
        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->respawnBot(bot->getPlayer());
        out << "Bot " << name << " id: " << bot->getBotId() << " has been killed.";
    }
    else
        out << "Failed to kill bot " << name << " id: " << bot->getBotId() << ".";
    return out.str();
}

// ELF 0x390760.
std::string AIConsole::killBotId(int id)
{
    std::stringstream out;
    if (!IBotManager::instance)
    {
        out << "No active BotManager found.";
        return out.str();
    }
    Bot* const bot = IBotManager::instance->getBotFromId(id);
    if (!bot)
        out << "Failed to find bot with id: " << id << ".";
    else if (!bot->isDead())
    {
        static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->respawnBot(bot->getPlayer());
        const std::string& name = bot->getPlayer()->getName();
        out << "Bot " << name << " id: " << id << " has been killed.";
    }
    else
    {
        const std::string& name = bot->getPlayer()->getName();
        out << "Failed to kill bot " << name << " id: " << id << ".";
    }
    return out.str();
}

// ELF 0x390CB0. Count is captured once; null entries are skipped.
void AIConsole::killAllBots()
{
    IBotManager* const manager = IBotManager::instance;
    if (!manager)
        return;
    const int count = manager->getBotCount();
    for (int i = 0; i < count; ++i)
    {
        Bot* const bot = manager->getBot(i);
        if (bot)
            static_cast<IAIGameEffector*>(CommonSingletonServer::m_iaiGameEffector)->respawnBot(bot->getPlayer());
    }
}

// ELF 0x392950.
Bot* AIConsole::getBotFromId(int id)
{
    return IBotManager::instance ? IBotManager::instance->getBotFromId(id) : 0;
}

// ELF 0x391D40.
void AIConsole::botMoveTo(float radius, Vec3 position, int id)
{
    Bot* const bot = getBotFromId(id);
    if (!bot)
        return;
    std::list<Vec3> positions;
    positions.push_back(position);
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    WaypointList* waypoints;
    if (equipment && equipment->isOfType(ITAir))
        waypoints = new WPAltitudeMoveTo(radius, radius, radius, 1, positions);
    else
        waypoints = new WPMoveTo(radius, 60.0f, 1, positions, -1);
    bot->setWaypoints(waypoints);
}

// ELF 0x391EE0.
void AIConsole::botFlyTo(int id, float first, float second, float third, Vec3 position)
{
    Bot* const bot = getBotFromId(id);
    if (!bot)
        return;
    std::list<Vec3> positions;
    positions.push_back(position);
    Information* const equipment = informationManager->getInformation(bot->getCurrentEquipment());
    if (equipment && equipment->isOfType(ITAir))
        bot->setWaypoints(new WPAltitudeMoveTo(first, second, third, 1, positions));
}

// ELF 0x392040. The flags retain count + 1 even when count exceeds the three supplied points.
void AIConsole::botSetWaypoints_Patrol(int id, float radius, int count, Vec3 a, Vec3 b, Vec3 c)
{
    Bot* const bot = getBotFromId(id);
    if (!bot)
        return;
    std::list<Vec3> positions;
    if (count > 0) positions.push_back(a);
    if (count > 1) positions.push_back(b);
    if (count > 2) positions.push_back(c);
    bot->setWaypoints(new WPPatrol(radius, static_cast<unsigned int>(count) + 1, positions));
}

// ELF 0x3921D0.
void AIConsole::botSetWaypoints_Patrol3d(int id, float radius, int count, Vec3 a, Vec3 b, Vec3 c)
{
    Bot* const bot = getBotFromId(id);
    if (!bot)
        return;
    std::list<Vec3> positions;
    if (count > 0) positions.push_back(a);
    if (count > 1) positions.push_back(b);
    if (count > 2) positions.push_back(c);
    bot->setWaypoints(new WPAltitudePatrol(radius, 10.0f, 100.0f,
                                          static_cast<unsigned int>(count) + 1, positions));
}

// ELF 0x392380.
void AIConsole::botGiveOrder(int id, std::string behaviour, std::string modifiers)
{
    Bot* const bot = getBotFromId(id);
    if (!bot)
        return;
    const int behaviourId = BotBehaviourManager::m_instance->getBehaviourId(behaviour);
    if (behaviourId == -1)
        return;
    float* const weights = m_behaviourModifiers[modifiers];
    if (weights)
        bot->recieveOrder(new BotOrder(weights, static_cast<unsigned char>(behaviourId)));
}

// ELF 0x390D50: names behaviour `id` (while AI settings exist).
void AIConsole::setBehaviour(int id, std::string name)
{
    if (CommonSingletonServer::m_iaiSettings)
        BotBehaviourManager::m_instance->setBehaviourName(id, name);
}

// ELF 0x390DE0.
std::string AIConsole::getBehaviour(int id)
{
    return CommonSingletonServer::m_iaiSettings ? BotBehaviourManager::m_instance->getBehaviourName(id) : "";
}

// ELF 0x390E60: nothing while the bot limit is unset (IAISettings slot 0 getMaxNBots() == -1)
// or for an unknown vehicle (+0x54 getVehicle) or behaviour; otherwise IAISettingsModifier +0x18.
void AIConsole::setVehicleDefaultBehaviour(std::string& vehicleName, std::string& behaviourName)
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    if (settings->getMaxNBots() == -1)
        return;
    const int vehicle = settings->getVehicle(vehicleName);
    if (vehicle == -1)
        return;
    const int behaviour = BotBehaviourManager::m_instance->getBehaviourId(behaviourName);
    if (behaviour == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setDefaultBehaviour(vehicle, behaviour);
}

// ELF 0x390F50 (761 bytes): the same two guards, then the named modifier set must exist. The
// behaviour "NULL" registers a null behaviour under getBehaviourId("NULL"); any other name goes
// through BotBehaviourManager::create, and only a non-null result is registered
// (IAISettingsModifier +0x14 addBehaviour). The create message is built and dropped.
void AIConsole::setVehicleBehaviour(std::string vehicleName, std::string behaviourName,
                                    std::string createFunctionName, std::string patternName,
                                    int mode, std::string urgeCurveName, std::string modifiersName)
{
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    if (settings->getMaxNBots() == -1)
        return;
    const int vehicle = settings->getVehicle(vehicleName);
    if (vehicle == -1)
        return;
    const std::map<std::string, float*>::iterator modifiers = m_behaviourModifiers.find(modifiersName);
    if (modifiers == m_behaviourModifiers.end())
        return;

    std::string message;
    BotBehaviour* behaviour = 0;
    if (behaviourName.compare("NULL") != 0)
    {
        behaviour = BotBehaviourManager::m_instance->create(behaviourName, createFunctionName,
                                                            patternName, mode, urgeCurveName,
                                                            modifiers->second, message);
        if (!behaviour)
            return;
    }
    const int id = BotBehaviourManager::m_instance->getBehaviourId(behaviourName);
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->addBehaviour(vehicle, id, behaviour);
}

// ELF 0x391500: `m_behaviourModifiers[name]` (operator[]: an unknown name is inserted with a
// null set); a non-null set becomes the bots' basic weights (IAISettingsModifier +0x30).
void AIConsole::setBasicBotWeights(std::string name)
{
    float* const weights = m_behaviourModifiers[name];
    if (weights)
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->setBasicBotWeights(weights);
}

// ELF 0x3916E0: a new set of getNumberOfBehaviours() zero weights (the count is re-read every
// iteration) under an unused name, which becomes the active set. An existing name does nothing.
void AIConsole::createBehaviourModifiers(std::string name)
{
    if (m_behaviourModifiers.find(name) != m_behaviourModifiers.end())
        return;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    float* const weights = new float[settings->getNumberOfBehaviours()];
    for (int i = 0; i < settings->getNumberOfBehaviours(); ++i)
        weights[i] = 0.0f;
    m_behaviourModifiers.insert(std::pair<const std::string, float*>(name, weights));
    m_activeBehaviourModifiers = m_behaviourModifiers.find(name);
}

// ELF 0x3918C0. A missing name leaves the current selection unchanged.
void AIConsole::setActiveBehaviourModifiers(std::string name)
{
    const std::map<std::string, float*>::iterator it = m_behaviourModifiers.find(name);
    if (it != m_behaviourModifiers.end())
        m_activeBehaviourModifiers = it;
}

// ELF 0x391920.
std::string AIConsole::getActiveBehaviourModifiers()
{
    return m_activeBehaviourModifiers == m_behaviourModifiers.end() ? "" : m_activeBehaviourModifiers->first;
}

// ELF 0x391990.
float AIConsole::getBehaviourModifier(std::string name)
{
    if (m_activeBehaviourModifiers == m_behaviourModifiers.end())
        return 0.0f;
    const int id = BotBehaviourManager::m_instance->getBehaviourId(name);
    return id == -1 ? 0.0f : m_activeBehaviourModifiers->second[id];
}

// ELF 0x391A50: one weight of the active set, by behaviour name; an unknown name or no active
// set does nothing.
void AIConsole::setBehaviourModifier(std::string name, float value)
{
    if (m_activeBehaviourModifiers == m_behaviourModifiers.end())
        return;
    const int id = BotBehaviourManager::m_instance->getBehaviourId(name);
    if (id == -1)
        return;
    m_activeBehaviourModifiers->second[id] = value;
}

// ELF 0x391B00. Names are looked up twice per row, as shipped.
std::string AIConsole::listBehaviourModifiers()
{
    if (m_activeBehaviourModifiers == m_behaviourModifiers.end())
        return "";
    std::string result;
    char value[256];
    for (int i = 0; i < static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getNumberOfBehaviours(); ++i)
    {
        result += BotBehaviourManager::m_instance->getBehaviourName(i);
        result += " = ";
        std::sprintf(value, "%f\n", getBehaviourModifier(BotBehaviourManager::m_instance->getBehaviourName(i)));
        result += value;
    }
    return result;
}

// ELF 0x391250: the entry is looked up first (an unknown name is inserted null and ignored),
// then the vehicle (IAISettings +0x54); IAISettingsModifier +0x1C.
void AIConsole::addInterpreterEntry(std::string vehicleName, std::string entryName)
{
    BAPEntry* const entry = getBAPEntryFromName(entryName);
    if (!entry)
        return;
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicleName);
    if (vehicle == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->addInterpreterEntry(vehicle, entry);
}

// ELF 0x391310: as above, IAISettingsModifier +0x20.
void AIConsole::addInterpreterEntry(std::string vehicleName, std::string entryName, BotLodLevel lod)
{
    BAPEntry* const entry = getBAPEntryFromName(entryName);
    if (!entry)
        return;
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicleName);
    if (vehicle == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->addInterpreterEntry(vehicle, lod, entry);
}

// ELF 0x3913D0: as above, IAISettingsModifier +0x24.
void AIConsole::removeInterpreterEntry(std::string vehicleName, std::string entryName, BotLodLevel lod)
{
    BAPEntry* const entry = getBAPEntryFromName(entryName);
    if (!entry)
        return;
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicleName);
    if (vehicle == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->removeInterpreterEntry(vehicle, lod, entry);
}

// ELF 0x391490: a known vehicle gets IAISettingsModifier +0x28.
void AIConsole::activateDefaultInterpreter(std::string vehicleName, BAPStatementType type, BotLodLevel lod)
{
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicleName);
    if (vehicle == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->activateDefaultInterpreter(vehicle, type, lod);
}

// ELF 0x392990: the side's SAI (IAIMain +0x3C) gets the named strategy (ISAI +0x08
// addStrategy); an absent AI main, SAI or strategy does nothing.
void AIConsole::addSAIStrategy(int side, std::string name)
{
    if (!getActiveAIMain())
        return;
    SAI* const sai = getActiveAIMain()->getSAI(side);
    if (!sai)
        return;
    Strategy* const strategy = StrategyManager::getInstance()->getStrategy(name);
    if (strategy)
        sai->addStrategy(strategy);
}

// ELF 0x3967A0: a known pattern for a known vehicle (IAISettings +0x54 getVehicle) is added
// through IAISettingsModifier +0x80.
void AIConsole::addSensingAgentPattern(const std::string& vehicle, const std::string& patternName)
{
    ISensingAgentPattern* const pattern = getSensingAgentPatternFromName(patternName);
    if (!pattern)
        return;
    const int id = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle);
    if (id == -1)
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->addSensingAgentPattern(id, pattern);
}

// ELF 0x396860: IAISettingsModifier +0x78.
void AIConsole::setSensingAgentPatternName(const std::string& name)
{
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setSensingAgentPatternName(name);
}

// ELF 0x3968A0: IAISettingsModifier +0x7C with the named pattern (null when unknown).
void AIConsole::setSensingAgentNullPattern(const std::string& name)
{
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setSensingAgentNullPattern(getSensingAgentPatternFromName(name));
}

// ELF 0x396930. Stops at the first absent bot id, rather than scanning the manager's count.
void AIConsole::sensingAgentDebug()
{
    int id = 0;
    while (Bot* bot = IBotManager::instance->getBotFromId(id))
    {
        ++id;
        bot->getSensingAgent()->m_flag134 = true;
    }
    if (id)
    {
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)->setDraw(true);
        static_cast<AIMain*>(IAIMain::m_instance)->showAll(false);
    }
}

// ELF 0x395E40. The vehicle lookup's -1 result is passed on unchecked.
void AIConsole::addAllowedUnit(const std::string& name)
{
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(name);
    AIStrategicObjectManager::getInstance()->getActiveArea()->addAllowedUnit(vehicle);
}

// ELF 0x395EA0.
void AIConsole::addExpelledUnit(const std::string& name)
{
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(name);
    AIStrategicObjectManager::getInstance()->getActiveArea()->addExpelledUnit(vehicle);
}

// ELF 0x395F00.
void AIConsole::addLandingZoneUnit(const std::string& name)
{
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(name);
    AIStrategicObjectManager::getInstance()->getActiveArea()->addToLandingZoneUsers(vehicle);
}

// ELF 0x396690: the vehicle id is taken unchecked (-1 is passed on); a known modifier set goes to
// IAISettingsModifier +0x10 setVehicleMods.
void AIConsole::setVehicleMods(const std::string& vehicleName, const std::string& modifiersName)
{
    const int vehicle = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicleName);
    const std::map<std::string, float*>::iterator it = m_behaviourModifiers.find(modifiersName);
    if (it == m_behaviourModifiers.end())
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setVehicleMods(vehicle, it->second);
}

// ELF 0x396720: a known modifier set becomes the side's standard personality
// (IAISettingsModifier +0x50). The ELF answers the low byte of that void call's EAX; the console
// class (297) is void and drops it.
void AIConsole::setStandardPersonality(const std::string& name, int side)
{
    const std::map<std::string, float*>::iterator it = m_behaviourModifiers.find(name);
    if (it == m_behaviourModifiers.end())
        return;
    static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
        ->setStandardPersonality(it->second, side);
}

// ELF 0x392AE0. The SAI lookup is discarded; the result is always -1.
float AIConsole::getFastThreshold()
{
    if (getActiveAIMain())
        getActiveAIMain()->getSAI(1);
    return -1.0f;
}

// ELF 0x392A70. The argument is unused and neither SAI is modified.
void AIConsole::setFastThreshold(float)
{
    if (getActiveAIMain())
    {
        getActiveAIMain()->getSAI(1);
        getActiveAIMain()->getSAI(2);
    }
}

// ELF 0x392BA0. The three diagnostic strings are constructed, then discarded.
void AIConsole::freeCameraFire()
{
    IAIObjectEnvironment* const environment =
        static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment);
    Mat4 camera = {};
    camera.m[0] = camera.m[5] = camera.m[10] = camera.m[15] = 1.0f;
    environment->getLocalPlayerCameraTransformation(camera);
    const Vec3 start = { camera.m[12], camera.m[13], camera.m[14] };
    const Vec3 end = { start.x + 1000.0f * camera.m[8],
                       start.y + 1000.0f * camera.m[9],
                       start.z + 1000.0f * camera.m[10] };
    Vec3 hit = {};
    if (environment->collideLineWithWorld(start, end, 0, &hit))
    {
        std::stringstream out;
        std::vector<std::string> text;
        out << "Start: " << start.x << '/' << start.y << '/' << start.z;
        text.push_back(out.str());
        out.str("");
        out << "End: " << hit.x << '/' << hit.y << '/' << hit.z;
        text.push_back(out.str());
        out.str("");
        const Vec3 difference = { hit.x - start.x, hit.y - start.y, hit.z - start.z };
        out << "Distance: " << std::sqrt(difference.x * difference.x + difference.y * difference.y
                                       + difference.z * difference.z) << "m";
        text.push_back(out.str());
    }
}

// ELF 0x3934C0. The shipped report leaves each vehicle's opening brace unmatched.
std::string AIConsole::showAISettings()
{
    std::stringstream out;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    out << "Number of vehicle types: " << settings->getNumberOfVehicles() << std::endl;
    out << "Maximum number of bots: " << settings->getMaxNBots() << std::endl;
    out << "Number of behaviours: " << settings->getNumberOfBehaviours() << std::endl;
    out << "World size (x,z): ("
        << static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getWorldSizeX() << ", "
        << static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)->getWorldSizeZ() << ")" << std::endl;
    for (int vehicle = 0; vehicle < settings->getNumberOfVehicles(); ++vehicle)
    {
        out << "Vehicle '" << static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings)->getVehicle(vehicle)
            << "'" << std::endl;
        out << "{" << std::endl;
        for (int behaviour = 0; behaviour < settings->getNumberOfBehaviours(); ++behaviour)
        {
            out << "  " << BotBehaviourManager::m_instance->getBehaviourName(behaviour) << ", ";
            out << (settings->getPlanGenerator(vehicle, behaviour) ? "HasGenerator=TRUE" : "HasGenerator=FALSE")
                << std::endl;
        }
    }
    return out.str();
}

// ELF 0x393B50. ConsoleClass315 discards this string result.
std::string AIConsole::createLODManager(float distance, float speedScale)
{
    std::stringstream out;
    if (!AILODManager::m_instance)
    {
        new AILODManager(distance, speedScale);
        out << "LODManager created.";
    }
    else
        out << "LODManager already created.";
    return out.str();
}

namespace {

// ELF 0x393E10. Tokenization deliberately uses the copied string's buffer.
bool checkLayerStrList(const std::string& list, int layer)
{
    std::string copy = list;
    const std::string wanted = formatString("%d", layer);
    char* token = std::strtok(const_cast<char*>(copy.c_str()), "|");
    while (token)
    {
        if (std::string(token) == wanted)
            return true;
        token = std::strtok(0, "|");
    }
    return false;
}

} // namespace

// ELF 0x394070. Writes the seven sections in retail order; write results are discarded.
bool AIConsole::saveAll(const std::string& directory, const std::string& layers)
{
    AIStrategicObjectManager* const manager = AIStrategicObjectManager::getInstance();
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const std::string filename = directory + "/AI/StrategicAreas.ai";
    const int groups = settings->getNumberOfVehicleGroups();
    io::File file;
    if (!file.open(filename, static_cast<io::FileMode>(0), io::FILEACCESS_2, 0))
        return false;

    std::ostringstream areas, assignments, landingZones, vehicleGroups, vehicles, categories;
    if (groups > 0)
    {
        vehicleGroups << "rem *** Create vehicle groups ***\n\n";
        for (int i = 0; i < groups; ++i)
        {
            const std::string& name = settings->getVehicleGroupName(i);
            vehicleGroups << "aiSettings.createVehicleGroup " << name << "\n";
        }
        vehicleGroups << "\n";
        vehicles << "rem * add vehicles to the groups *\n\n";
        for (int i = 0; i < settings->getNumberOfVehicles(); ++i)
        {
            const int group = settings->getVehicleGroup(i);
            const std::string& name = settings->getVehicleGroupName(group);
            vehicles << "aiSettings.addVehicleToVehicleGroup " << i << " " << name << "\n";
        }
        vehicles << "\n\n";
        // shipped defect: categories are emitted only when there are vehicle groups.
        categories << "rem *** Create categories ***\n\n";
        for (int i = 0; i < manager->getNumberOfCategories(); ++i)
        {
            const std::string& name = manager->getCategoryName(i);
            categories << "aiStrategicArea.createNewCategory " << name << "\n";
        }
        categories << "\n";
    }

    areas << "rem *** Create strategic areas ***\n";
    for (std::map<std::string, AIStrategicArea*>::const_iterator it = manager->getStrategicAreas().begin();
         it != manager->getStrategicAreas().end(); ++it)
    {
        AIStrategicArea* const area = it->second;
        if (!layers.empty() && !checkLayerStrList(layers, area->m_i400))
            continue;
        const std::string name = it->first;
        if (area->m_fromControlPoint)
            areas << "aiStrategicArea.createFromControlPoint " << name << " "
                  << area->m_controlPointId << " " << area->m_size[0];
        else
        {
            areas << "aiStrategicArea.create " << name << " " << area->m_centre.x << '/'
                  << area->m_centre.y << " ";
            const Vec2& position = area->getPosition(1);
            areas << position.x << '/' << position.y << " " << area->m_f148 << " " << area->m_size[0];
        }
        if (manager->getNumberOfCategories() > 0)
        {
            const std::string& category = manager->getCategoryName(area->m_category);
            areas << " " << category;
        }
        areas << "\n";
        areas << "aiStrategicArea.layer " << area->m_i400 << "\n\n";
        assignments << "aiStrategicArea.setActive " << name << "\n";

        IAISettings* const currentSettings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
        const int groupCount = currentSettings->getNumberOfVehicleGroups();
        const int allowed = area->getAllowedVehicleGroups();
        if (allowed != -1)
        {
            for (int i = 0; i < groupCount; ++i)
                if ((allowed >> (i & 31)) & 1)
                {
                    const std::string& group = currentSettings->getVehicleGroupName(i);
                    assignments << "AIStrategicArea.addAllowedVehicleGroup " << group << "\n";
                }
        }
        const int expelled = area->getExpelledVehicleTypes();
        const int vehicleCount = currentSettings->getNumberOfVehicles();
        if (expelled)
        {
            for (int i = 0; i < vehicleCount; ++i)
                if ((expelled >> (i & 31)) & 1)
                {
                    const std::string& vehicle = currentSettings->getVehicle(i);
                    assignments << "AIStrategicArea.addExpelledUnit " << vehicle << "\n";
                }
        }
        const int landingZoneVehicles = currentSettings->getNumberOfVehicles();
        for (int i = 0; i < landingZoneVehicles; ++i)
            if (area->isLandingZoneUser(i))
            {
                const std::string& vehicle = currentSettings->getVehicle(i);
                assignments << "AIStrategicArea.addLandingZoneUnit " << vehicle << "\n";
            }

        const std::vector<AILandingZone*> zones = area->getLandingZones();
        for (std::vector<AILandingZone*>::const_iterator zone = zones.begin(); zone != zones.end(); ++zone)
        {
            if (!*zone)
                continue;
            const LandingZoneDirection beach = (*zone)->getBeachDirection();
            landingZones << "AILandingZone.createLandingZone " << (*zone)->getName() << " ";
            const Vec2& minimum = (*zone)->getMinCorner();
            landingZones << minimum.x << '/' << minimum.y << " ";
            const Vec2& maximum = (*zone)->getMaxCorner();
            landingZones << maximum.x << '/' << maximum.y << " " << beach << "\n\n";
            assignments << "AIStrategicArea.attachLandingZone " << (*zone)->getName() << "\n";
        }
        if (!zones.empty() && landingZoneVehicles < 1)
            assignments << "AIStrategicArea.addLandingZoneUnit LandingCraft\n";

        const std::vector<AIStrategicObject*> neighbours = area->m_neighbours;
        for (std::vector<AIStrategicObject*>::const_iterator neighbour = neighbours.begin();
             neighbour != neighbours.end(); ++neighbour)
            assignments << "AIStrategicArea.addNeighbour "
                        << static_cast<AIStrategicArea*>(*neighbour)->m_name << "\n";
        const int flags = area->getObjectTypeFlags();
        for (int i = 1; i < 21; ++i)
            if ((flags >> i) & 1)
                assignments << "aiStrategicArea.addObjectTypeFlag "
                            << static_cast<AIStrategicObject::StrategicObjectType>(i) << "\n";
        for (int i = 0; i < IAIPathfinding::instance->getNumberOfMaps(); ++i)
            if (IAIPathfinding::instance->isMapUsed(i))
            {
                const std::string& map = IAIPathfinding::instance->getMapNameFromId(i);
                assignments << "AIStrategicArea.setOrderPosition " << map << " ";
                const Vec3 position = area->getOrderPos(i, 1);
                assignments << position.x << '/' << position.y << '/' << position.z << "\n";
            }
        assignments << "aiStrategicArea.setSide " << area->getSide() << "\n";
        if (std::fabs(area->m_f396 - area->m_size[0]) > 0.25f)
            assignments << "aiStrategicArea.vehicleSearchRadius " << area->m_f396 << "\n";
        if (!area->m_sideEnabled[0])
            assignments << "aiStrategicArea.setTakeable 1 0\n";
        if (!area->m_sideEnabled[1])
            assignments << "aiStrategicArea.setTakeable 2 0\n";
        assignments << "\n";
    }

    std::ostringstream waypoints;
    manager->streamWaypoints(waypoints, layers);
    file.write(vehicleGroups);
    file.write(vehicles);
    file.write(categories);
    file.write(areas);
    file.write(landingZones);
    file.write(assignments);
    file.write(waypoints);
    file.close();
    return true;
}

// ELF 0x396180. Projects both forward directions onto XZ before comparing them.
float AIConsole::getCurrentPitchAngle() const
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
        ->getPlayerFromLocalHumanPlayer();
    if (!player)
        return -1.0f;
    const Mat4& camera = *static_cast<const Mat4*>(player->getCameraTransformation());
    Vec3 first = { camera.m[8], 0.0f, camera.m[10] };
    const auto normalize = [](Vec3& v) {
        const float lengthSqr = v.x * v.x + v.y * v.y + v.z * v.z;
        if (!(std::fabs(lengthSqr - 1.0f) < 1.1920928955078125e-7f))
        {
            if (std::fabs(lengthSqr) < 1.1920928955078125e-7f)
                v = Vec3();
            else
            {
                const float scale = 1.0f / std::sqrt(lengthSqr);
                v.x *= scale; v.y *= scale; v.z *= scale;
            }
        }
    };
    normalize(first);
    IAIObject* const vehicle = player->getCurrentVehicle();
    Vec3 second;
    second.x = vehicle->getTransformation().m[8];
    second.y = 0.0f;
    second.z = vehicle->getTransformation().m[10];
    normalize(second);
    float dot = second.x * first.x + second.y * first.y + second.z * first.z;
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;
    return std::acos(dot) * 57.2957763671875f;
}

// ELF 0x396410. Projects onto YZ, retaining the shipped pitch/yaw naming.
float AIConsole::getCurrentYawAngle() const
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
        ->getPlayerFromLocalHumanPlayer();
    if (!player)
        return -1.0f;
    const Mat4& camera = *static_cast<const Mat4*>(player->getCameraTransformation());
    Vec3 first = { 0.0f, camera.m[9], camera.m[10] };
    const auto normalize = [](Vec3& v) {
        const float lengthSqr = v.x * v.x + v.y * v.y + v.z * v.z;
        if (!(std::fabs(lengthSqr - 1.0f) < 1.1920928955078125e-7f))
        {
            if (std::fabs(lengthSqr) < 1.1920928955078125e-7f)
                v = Vec3();
            else
            {
                const float scale = 1.0f / std::sqrt(lengthSqr);
                v.x *= scale; v.y *= scale; v.z *= scale;
            }
        }
    };
    normalize(first);
    IAIObject* const vehicle = player->getCurrentVehicle();
    Vec3 second;
    second.x = 0.0f;
    second.y = vehicle->getTransformation().m[9];
    second.z = vehicle->getTransformation().m[10];
    normalize(second);
    float dot = second.x * first.x + second.y * first.y + second.z * first.z;
    if (dot < -1.0f) dot = -1.0f;
    if (dot > 1.0f) dot = 1.0f;
    return std::acos(dot) * 57.2957763671875f;
}

} // namespace ai
} // namespace hfe
} // namespace dice
