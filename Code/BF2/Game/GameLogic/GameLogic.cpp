// Shared game lifecycle, level scripts and player actions.
// Path attested by __FILE__ in load (ELF 0x80D01B8 / 0x80D214C).
// Script order and event registration are part of the content contract.

#include "GameLogic.h"
#include "../Main/Game.h"        // isDedicated - loadCallback's client/server split
#include "GameLogicCollision.h"                        // doImpact's exact static forwarding target
#include "GameLogicExplosion.h"                        // common explosion presentation forwarder
#include "../Objects/ObjectLayoutAccess.h"           // heal / replenishAmmo walk +0x3C / +0x28
#include "../Objects/GameLogicComp/IArmorGLComp.h"    // heal's hit-point tests
#include "../Objects/GameLogicComp/ArmorGLCompTemplate.h"   // getCanBeRepairedWhenWreck (+0x100)
#include "../Objects/IWeaponObject.h"                 // reloadAmmoPercent (+0x80)
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // getPcos / getWeapons / getAmmoStatus
#include "../../Physics/MaterialManager.h"             // getDamageMod (+0x58); getMaterialPtr (+0x24)
#include "../../Physics/Material.h"                    // getIsSeeThrough - ProjectileVsTargetPredicator
#include "../../Scene/Object/ObjectPredicator.h"       // ProjectileVsTargetPredicator - its bodies live here
#include "../Objects/IObjectTemplate.h"               // queryImplementation / getComponentFromClassID
#include "../Objects/IPlayerControlObjectTemplate.h"  // typed PCO-template category dispatch
#include "../Objects/IObject.h"                       // getRootParent
#include "../Objects/IPlayer.h"
#include "../../Core/Debug.h"                         // the two client refusals
#include <map>
#include <vector>
#include <cstdio>                // the [TEMP] [KILL] probe
#include <cstdlib>               // [TEMP] fire-mode toggle tracer
#include "CombatAreaManager.h"   // the ctor builds one - see the bottom of this file
#include "../Objects/Game/AmbientEffectAreaManager.h"
#include "../Objects/Game/TriggerManager.h"
#include "../Objects/Game/TriggerableManager.h"
#include "../Objects/Game/SupplyDropManager.h"   // ~GameLogic's reset
#include "../Objects/Game/ArtilleryHandler.h"    // ArtilleryManager lifecycle / RCO refresh
#include "../Objects/Game/UAVVehicleManager.h"   // UAVVehicleManager lifecycle
#include "../Objects/Game/DropVehicle.h"         // ECGameLogic/22 controller attachment
#include "../Common/ClientVoteManager.h"   // ~GameLogic's getInstance / release
#include "../../AI/AIInterface/AIInstance.h"   // cross-port IAI vtable view
#include "../../IO/Input/PlayerInput.h"   // getIsSprintRequested takes one BY value
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // the weapon slots
#include "../Objects/IObject.h"                       // handleMessage / getTemplate / getParent
#include "../Objects/ISoldier.h"
#include "../Objects/Game/Soldier.h"       // CID-gated concrete Soldier dispatches
#include "../Objects/IWeaponObject.h"
#include "../Objects/IItemObject.h"
#include "../Objects/ObjectTemplate.h"                // getClassID
#include "../Objects/GenericFireArmTemplate.h"        // m_isGasMask / m_isNightVision
#include <algorithm>                                  // sort / find - the next-weapon walk
#include <list>                                       // m_noVehiclesOmitList
#include <set>                                        // preCacheSpawnedTemplates' template set
#include <vector>
#include "../Objects/RemoteControlledObjectTemplate.h"     // rcType (+0x180) - the RCO registry's artillery test
#include "../Objects/Game/RemoteControlledObject.h"

#include "../../IO/Console/Console.h"          // mainConsole - run +0x28
#include "../GameServer/ServerSettings.h"  // serverSettings->getNoVehicles - setTeamDropVehicle
#include "../../IO/File/FileManager.h"         // fileManager - fileExists, findFiles +0x34; FileInfo
#include "VoiceMessages.h"
#include "../Objects/Game/TargetObject.h"   // getTargetObjectId - the registry key                     // loadVoiceMessages builds one
#include "../../Sound/BfSound.h"               // g_bfSound - engine() +0x28, the finalize target
#include "../../Sound/NullSound.h"              // ISound::stop - stopPlayingVoMessages
#include "../../Core/Debug.h"
#include "../../Core/SettingsRepostitory.h"    // settings - intSet / stringSet
#include "../../Core/EventManager.h"           // eventManager - postEvent(cat, id, void*, float)
#include "../../Core/StringUtil.h"             // getLowerCase, compareIgnoreCase, formatString
#include "../../Core/LoadStat.h"               // loadStat - beginLoadStats resets it
#include "../SingletonServer.h"                // m_iNametagManager - the CEPostLoad arm
#include "../../IO/Network/NetworkManager.h"   // getObject / regressNetworkableIds
#include "../../IO/Network/NetworkableInfo.h"  // unload -> deleteAllInfos
#include "../../Physics/MaterialManager.h"     // unload -> removeAll
#include "SpawnSystem/SpawnManager.h"          // unload -> removeAll
#include "../../Core/System.h"                 // g_system - getExactTime
#include "../../Scene/Object/ObjectManager.h"  // objectManager - initGrid/destroyObjects/...
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"  // preCacheObjects +0x74
#include "../../Physics/HeightmapCluster.h"    // IHeightmapCluster - getHeightmapSize +0x30
#include "SquadManager/ISquadManager.h"          // reset (+0x10) - the shim it replaced
#include "../IGame.h"                          // game - getCurrentMap*/setCurrentMapSize/update
#include "../Objects/ObjectTemplate.h"         // ObjectTemplate::setIsPreCaching (static)
#include "../Main/BF2Engine.h"                 // updateStatusMonitor - loadCallback's refresh
#include "../GameServer/VoipServerConnection.h"
#include "../Objects/Game/ControlPoint.h"      // add/removeControlPoint - getControlerId
#include "../Objects/ObjectLayoutAccess.h"     // removeControlPoint's raw template read (+0x14)
#include "../Objects/IStrategicObjectComp.h"
#include "../Objects/StrategicObjectComp.h"
#include "../GameEvents/PythonCommandEvent.h"
#include "../Objects/ObjectSpawnerTemplate.h"  // convertDynamicObjects' synthesised spawners
#include "../Objects/DestroyableObjectTemplate.h"   // its respawnDelay / nrOfRespawns arm
#include "../Objects/TemplateLeaves.h"         // DynamicBundleTemplate / DestroyableWindowsBundleTemplate
#include "../../Physics/IPhysicsNode.h"        // getIsDynamicObject's getIsMobile
#include "../../Core/Ids.h"                    // the five CIDs the conversion tests
// the round-reset slice
#include "../../Scene/Object/PlayerManager.h"  // playerManager - getPlayers / getLocalHumanPlayer
#include "../Objects/IPlayer.h"                // reset / getFreeCamera
#include "../Objects/Game/ObjectSpawner.h"     // ObjectSpawner::reset (direct call, as shipped)
#include "../Objects/Game/SupplyDepot.h"       // round-reset registry's direct SupplyDepot::reset
#include "../Objects/ICameraObject.h"          // the free camera's setViewMode
#include "../Objects/Base/FreeCamera.h"        // resetPlayerFreeCameraPosition's primary dispatch
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // the occupied checks
#include "../GameServer/ServerVoteManager.h"   // reset()'s vote reset
#include "../../Math/Vec3.h"
#include "../../Math/Mat4.h"                   // getIdentity / setRotationZXY
#include "../../Core/ClassManager.h"           // createSunTemplate's getSingleton
#include "../../Geom/DecalSystem/IDecalManager.h"      // the ctor's ground decal object (createClient +0x14)
#include "../../Geom/DecalSystem/DecalObjectAccess.h"  // ~GameLogic's DecalObject slot 10 (release)
#if !DEDICATED
#include "../../Menu/ISwiffHost.h"             // PE-only load-state / rounds-left publication
#include "../../Menu/BF2HudManager.h"          // handleEvent id 20's UAV reload reset (client arm)
#include "../../Menu/Bf2MenuMapNode.h"
#endif
#include <cfloat>                              // FLT_EPSILON - the dynamic bundle's -1 test
// the killPlayer slice
#include "../Common/GameEvent.h"               // PostRemoteEvent - the death broadcast (id 10)
#include "../Objects/GameLogicComp/IArmorGLComp.h"   // getLastHitPlayer / the abandoned-soldier arm
#include "Player.h"                            // the +0xD8 kill latch (friend) + hfe::getPlayer
#include <cmath>

#include <list>
#include <map>
#include <string>
#include <sstream>
#include <vector>

namespace dice {
namespace hfe {

extern SettingsRepostitory* settings;          // ds:0x8ABBCBC
extern ClassManager*        classManager;      // ds:0x8AC85A0 - createSunTemplate's getSingleton
extern IGame*               game;              // ds:dice::hfe::game
extern BF2Engine*           bf2Engine;         // ds:0x8AACBD8
extern const unsigned int   IID_IGameServer;
namespace world { extern IBase* heightmapCluster; }   // ds:0x8ABAF24 - Core/Globals.cpp
extern IBase* spawnManager;                           // ds:0x8AD3A00 - Core/Globals.cpp
namespace io { extern IBase* networkableInfoManager; }
namespace world {
extern IBase* materialManager;
}
#if !DEDICATED
extern hud::BF2HudManager* hudManager;         // BF2.exe ds:0xA10890 - handleEvent id 20's client arm
#endif

// GameLogic::init - ELF 0x80CD370 (28 B) / PE 0x464B20 (14 B).
// Both builds make the same unguarded virtual call and then return true; the manager's result is
// deliberately ignored. The object-template manager is installed before BF2Engine starts a game.
bool GameLogic::init()
{
    world::objectTemplateManager->setEngineIsInitialized();
    return true;
}

// the level-scoped managers load() talks to
// Level and commander managers use shared concrete declarations. Retail null guards remain.
extern IBase*               squadManager;      // ds:0x8ACBFA4 - Core/Globals.cpp (IBase*, no layout)

// Destroys level objects before clearing registries, drops networkable metadata
// before unmounting the archive, then resets level managers. Client images also
// flush sound triggers. Returns true.
bool GameLogic::unload()
{
    m_isLevelLoadedXP = false;
    destroyAllControlPoints();
    reset();
    world::objectManager->deleteAllRegisteredObjects();
    world::objectTemplateManager->deleteAll(true);

    m_controlPointGroups.clear();
    m_controlPointMap.clear();
    m_flagMap.clear();
    m_objectSpawnerMap.clear();
    m_supplyDepotMap.clear();
    m_projectileMap.clear();
    m_targetObjectMap.clear();
    m_remoteControlledObjectMap.clear();
    m_strategicObjectMap.clear();

    static_cast<world::MaterialManager*>(world::materialManager)->removeAll();
    static_cast<world::IHeightmapCluster*>(world::heightmapCluster)->reset();
    // [PLATFORM] Clients remove sound triggers after deleting templates and before
    // posting the unload event. Requires the sound module and engine.
    if (!isDedicated())
        g_bfSound->engine()->removeAllTriggers();
    eventManager->postEvent(ECCore, 13, 0, 0.0f);
    reinterpret_cast<io::INetworkableInfoManager*>(io::networkableInfoManager)->deleteAllInfos();
    io::fileManager->unmountArchive(std::string("*levels"));

    if (ai::g_aiInstance)
        bf2Engine->shutdownAI();

    static_cast<SpawnManager*>(spawnManager)->removeAll();
    if (CombatAreaManager::getInstance())
        CombatAreaManager::getInstance()->reset();
    if (world::ambientEffectAreaManager)
        world::ambientEffectAreaManager->removeAll();
    if (world::triggerManager)
        world::triggerManager->removeAll();
    if (world::artilleryManager)
        world::artilleryManager->removeAll();
    if (world::uavVehicleManager)
        world::uavVehicleManager->removeAll();

    m_targetObjectIdCounter = 0;
    m_kitTemplates.clear();
    m_kits.clear();
    m_isLevelLoaded = false;
    io::networkManager->m_networkedTemplates.clear();
    return true;
}

void GameLogic::destroyAllControlPoints()
{
    const std::map<unsigned int, world::IObject*> points(m_controlPointMap);
    for (std::map<unsigned int, world::IObject*>::const_iterator it = points.begin();
         it != points.end(); ++it)
    {
        game->destroyObject(it->second);
    }
}

// Runs the level's Init.con and initializes the object grid from the heightmap size.
// A missing file is fatal. Requires the heightmap singleton and publishes the same
// square-world size to GLSWorldSizeX and GLSWorldSizeZ.
bool GameLogic::loadInitFile(const std::string& path)
{
    std::string file = path;
    file.append("Init.con");

    if (!io::fileManager->fileExists(file))
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 1714)
            << "Level init.con file not found:" << file;
        return false;
    }

    std::string out;
    io::mainConsole->run(file, "", "", "", "", "", "", "", "", out);          // +0x28

    const int worldSize =
        reinterpret_cast<world::IHeightmapCluster*>(world::heightmapCluster)
            ->getHeightmapSize();                                             // +0x30
    const float half = worldSize * 0.5f;
    Vec2 gridMin;  gridMin.x = -half;  gridMin.y = -half;    // Vec2/Vec3 are the tree's POD
    Vec2 gridMax;  gridMax.x =  half;  gridMax.y =  half;    // structs - no (x,y) ctor exists
    Vec3 gridOrigin;  gridOrigin.x = 0.0f;  gridOrigin.y = 200.0f;  gridOrigin.z = 2000.0f;
    world::objectManager->initGrid(gridMin, gridMax, 7, gridOrigin);          // +0x20
    settings->intSet(std::string("GLSWorldSizeX"), worldSize);
    settings->intSet(std::string("GLSWorldSizeZ"), worldSize);
    return true;
}

// Recursively executes nonempty content scripts in case-insensitive path order.
// Extension matching is case-sensitive substring matching on the original name; AI scripts require bots.
// The backslash common-directory rewrite gives common content priority where separators match.
// Abort requests return false during discovery or execution. [CHOSEN] Progress payload member names.
bool GameLogic::loadAllConFiles(const std::string& path)
{
    const std::string common    = "\\common\\";
    const std::string sortFirst = "\\aaaaaaaaaaaaaaa\\";

    if (m_byte1B0)
        return false;

    std::list<io::FileInfo> files;
    io::fileManager->findFiles(path, true, files);                            // +0x34, recursive

    bool botsGame = false;
    if (ai::g_aiInstance)
        botsGame = ai::aiInstance()->isBotsGame();

    std::map<std::string, int, NoCaseStringCompare> sorted;
    for (std::list<io::FileInfo>::iterator it = files.begin(); it != files.end(); ++it)
    {
        if (it->size == 0)                     // the shipped gate - FileInfo +0x08, see above
            continue;

        std::string lower = getLowerCase(it->name);
        if (it->name.find(".con") != std::string::npos
            || (botsGame && it->name.find(".ai") != std::string::npos))
        {
            const std::string::size_type p = lower.find(common);
            if (p != std::string::npos)
                lower = lower.substr(0, p) + sortFirst + lower.substr(p + common.size());

            sorted.insert(std::make_pair(path + lower, 0));

            if (m_byte1B0)
                return false;
        }
    }

    // Two progress steps per object; payload member names are [CHOSEN].
    NextLoadPart payload = { 2 * static_cast<int>(sorted.size()), 1.0f, "Objects" };
    eventManager->postEvent(static_cast<EventCategory>(5), 4, &payload, 0.0f);

    for (std::map<std::string, int, NoCaseStringCompare>::iterator it = sorted.begin();
         it != sorted.end(); ++it)
    {
        std::string file = it->first;
        const std::string::size_type p = file.find(sortFirst);
        if (p != std::string::npos)
            file = file.substr(0, p) + common + file.substr(p + sortFirst.size());

        std::string out;
        io::mainConsole->run(file, "", "", "", "", "", "", "", "", out);      // +0x28

        if (m_byte1B0)
            return false;
    }
    return true;
}

// Pump server updates and report changed loading percentages. Clients also post
// a loading-screen event; LoadStat throttles callbacks to thirty per second.
// [UNRESOLVED] Some LoadStat member names remain provisional.
// [RETIRE-WHEN: loadcallback-client-preamble] Recover the client message-pump
// return contract and working-set operations before restoring the omitted preamble.
static void loadCallback()
{
    if (game != 0 && game->queryInterface(IID_IGameServer) != 0)
        game->update(0, -1.0f);                            // pump the server

    if (bf2Engine)
        bf2Engine->updateStatusMonitor(true);

    float percent;
    if (loadStat->m_unknown0B)                             // +0x0B - the complete flag
    {
        percent = 100.0f;
    }
    else if (loadStat->m_unknown24 != 0)                   // +0x24 - the expected total
    {
        if (loadStat->m_loadCounter >= loadStat->m_unknown24)
            loadStat->m_loadCounter = loadStat->m_unknown24;   // clamp, AND store the clamp back
        const float frac = static_cast<float>(loadStat->m_loadCounter)
                         / static_cast<float>(loadStat->m_unknown24);
        percent = (loadStat->m_unknown38
                   + frac * (loadStat->m_unknown3C - loadStat->m_unknown38)) * 100.0f;
    }
    else
    {
        percent = 100.0f * loadStat->m_unknown38;
    }

    // BF2.exe 0x4EBF3B gates the whole block on isDedicated(); the ELF does not, because there the
    // test is constant-true. The client prints no "Loaded N%" lines.
    if (isDedicated())
    {
        static int prevPercent;                            // function-local static, .bss zero
        const int whole = static_cast<int>(percent);
        if (whole != prevPercent)
        {
            prevPercent = whole;
            Debug(DEBUGTYPE_1, "Game/GameLogic/GameLogic.cpp", 1541)
                << "Loaded " << whole << "%";              // the console's load-progress lines
        }
    }

#if !DEDICATED
    // the tail call: repaint the loading screen. The percent goes by address, and the
    // delay is 0.0f so it is dispatched on this same processEvents pass.
    eventManager->postEvent(static_cast<EventCategory>(10), 4, &percent, 0.0f);
#endif
}

// Resets progress, records the start time, announces the level on HUD/frame
// channels, and arms the callback at the exact widened float interval 1/30.
void GameLogic::beginLoadStats(const std::string& levelPath)
{
    LoadStat* stat = loadStat;                             // ds:0x8ABBC98
    const float now = static_cast<float>(g_system->getExactTime()); // System +0x18; float spill
    stat->m_loadCounter = 0;                               // +0x20
    stat->m_unknown1C = now;                               // +0x1C - the load start time

#if !DEDICATED
    // [PLATFORM] BF2.exe 0x4EC051-0x4EC08C publishes the loading state before the menu events
    // below can construct endOfRound.swf. The SwiffHost lookup and result are both unguarded in
    // retail. Without this write the movie sees getIsLoading()==false on its first frame and skips
    // the progress bar and the EOR_finishedloading ("validating client data") state entirely.
    if (!isDedicated())
    {
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(
            classManager->getSingleton(std::string("SwiffHost")));
        host->setFlag7B8(true);                             // +0xD0
    }
#endif

    eventManager->postEvent(static_cast<EventCategory>(2), 5,
                            const_cast<std::string*>(&levelPath), 0.0f);
    eventManager->postEvent(static_cast<EventCategory>(10), 3,
                            const_cast<std::string*>(&levelPath), 0.0f);

    stat->setSecondsBetweenCallbacks(static_cast<double>(1.0f / 30.0f));
    stat->registerLoadCallback(&loadCallback);
}

namespace {

// The nine-string run/runFullAccess call with all v_arg slots empty - the shape every console
// drive in load() uses bar GamePlayObjects.con and Triggerables.con (which pass the side tag as
// v_arg1). A helper rather than nine spelled ctors per site; the call is identical.
bool runScript(const std::string& file)
{
    const std::string none;
    std::string out;
    return io::mainConsole->run(file, none, none, none, none, none, none, none, none, out);
}

bool runScriptArg1(const std::string& file, const std::string& arg1)
{
    const std::string none;
    std::string out;
    return io::mainConsole->run(file, arg1, none, none, none, none, none, none, none, out);
}

} // namespace

// Loads level content with explicit abort checkpoints. After loading, players sorted by score
// receive free cameras before the client prediction path can use their vehicles.
bool GameLogic::load(const std::string& levelPath)
{
    // teardown of the previous level, and the re-entry gates (0x80CE18A-0x80CE1B0)
    if (world::triggerableManager)
        world::triggerableManager->reset();
    ClearDropVehicles();                                   // virtual +0x2D0
    if (m_isLevelLoaded || m_byte1B0)
        return false;

    // the bare level name: strip the leading "Levels/" (first min(7, len) chars) and the
    //      trailing '/' (0x80CE1C0-0x80CE26A)
    std::string levelName(levelPath);
    levelName.erase(0, levelName.size() <= 7 ? levelName.size() : 7);
    if (!levelName.empty())
        levelName.erase(levelName.size() - 1);

    // the Armored Fury booster block (0x80CE271-0x80CE302 + the hoisted else)
    io::fileManager->unmountArchive(std::string("*levels"));
    if (compareIgnoreCase(levelName, "Midnight_Sun") != 0
        && compareIgnoreCase(levelName, "OperationHarvest") != 0
        && compareIgnoreCase(levelName, "OperationRoadRage") != 0)
    {
        // NOT a booster map: actively unmount both booster archives so their content cannot
        // leak across a map change.
        io::fileManager->unmountArchive(std::string("Booster_server.zip"));
        io::fileManager->unmountArchive(std::string("Booster_client.zip"));
    }
    else
    {
        io::fileManager->permitMountArchives(true);
        io::fileManager->mountArchive(std::string("Booster_server.zip"), std::string("Objects"), true);
        // [PLATFORM] BF2.exe 0x4F3299-0x4F32D9 keeps the same permit window open and mounts the
        // client booster archive only when isDedicated() is false. The common close is 0x4F32EE.
        if (!isDedicated())
            io::fileManager->mountArchive(std::string("Booster_client.zip"), std::string("Objects"), true);
        io::fileManager->permitMountArchives(false);
    }

#if !DEDICATED
    // [PLATFORM] BF2.exe 0x4F32FB-0x4F33BE. The client seeds the loading-screen rounds-left value
    // with 999 before the EndOfRoundEvent supplies the real value. The host itself is deliberately
    // unguarded; only ModInfo and the requested map are tested. A missing map aborts through the
    // same ECCore/14 + message 21 path as retail.
    if (!isDedicated())
    {
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(
            classManager->getSingleton(std::string("SwiffHost")));
        host->setIntF4C(999);                               // +0x35C

        IModInfo* modInfo = static_cast<IModInfo*>(host->getModInfo());
        if (!modInfo || !modInfo->getMap(levelName))
        {
            eventManager->postEvent(ECCore, 14, 0, 0.0f);
            host->setMessageId(21);                         // +0xC0
            return false;
        }
    }
#endif

    // Mount level archives and run their scripts within the shared permit window.
    // Client runtime also mounts client.zip. Store the level path without its trailing slash.
    m_levelPath = levelPath.substr(0, levelPath.size() - 1);
    io::fileManager->permitMountArchives(true);
    io::fileManager->mountArchive(m_levelPath + "/server.zip", m_levelPath, true);
    {
        const std::string none;
        std::string out;
        io::mainConsole->runFullAccess(m_levelPath + "/ServerArchives.con",
                                       none, none, none, none, none, none, none, none, out);
    }
    if (!isDedicated())
    {
        io::fileManager->mountArchive(m_levelPath + "/client.zip", m_levelPath, true);
        const std::string none;
        std::string out;
        io::mainConsole->runFullAccess(m_levelPath + "/ClientArchives.con",
                                       none, none, none, none, none, none, none, none, out);
    }
    io::fileManager->permitMountArchives(false);

    // the map-size search (0x80CE7AE-0x80CEB90): probe GameModes/<mode>/<size>/ for
    //      GamePlayObjects.con at each of the four supported sizes
    const int maxPlayers = game->getCurrentMapMaxPlayers();           // +0xFC
    const std::string gameMode(game->getCurrentMapGameMode());        // +0xF4
    const std::string modeDir = std::string("GameModes/") + gameMode + "/";

    static const short sizes[] = { 16, 32, 64, 128, -1, -1 };  // the function-local static at
                                                               // sentinels unused
    std::vector<int> found;
    for (int i = 0; i <= 3; ++i)                               // `cmp esi,3 / jle` - four sizes
    {
        std::ostringstream path;
        path << levelPath << modeDir << sizes[i] << "/GamePlayObjects.con";
        if (io::fileManager->fileExists(path.str()))
            found.push_back(sizes[i]);
    }
    int chosenSize = maxPlayers;                               // v212's initial value
    if (found.empty())
    {
        // Severity 6 - fatal through the debug callback on this build, exactly as a level with no
        // gameplay objects should be.
        Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 1147)
            << "Unable to find GamePlayObjects.con";
    }
    else if (maxPlayers >= found.back())                       //  + hoisted 0x80D2106
    {
        chosenSize = found.back();
    }
    else
    {
        // The first found size >= maxPlayers (the 0x80CEB70 walk).
        for (unsigned i = 0; i < found.size(); ++i)
        {
            if (found[i] >= maxPlayers)
            {
                chosenSize = found[i];
                break;
            }
        }
    }
    game->setCurrentMapSize(chosenSize);                       // +0x104
    settings->stringSet(std::string("GLGameLevelPath"), levelPath);

    // stats, sun, and the first scripted phase (0x80CEC4A-0x80CEE12)
    beginLoadStats(levelPath);
    createSunTemplate();                                       // virtual +0x2F8
    eventManager->postEvent(static_cast<EventCategory>(10), 6,
                            const_cast<char*>("Loading Templates.."), 0.0f);
    runScript(std::string("Settings/ScoreManagerSetup.con"));

    // the AIDefault detour (0x80CEF05-0x80CEF19 -> the hoisted 0x80D19BB block): a
    //      conditional block between ScoreManagerSetup and the object sweep, not a trailing
    //      phase (the dominator analysis's "third path", now placed exactly).
    if (ai::g_aiInstance && ai::aiInstance()->isBotsGame())
    {
        runScript(std::string("AI/AIDefault.ai"));
        ai::aiInstance()->setFlag14(true);                     // settings-from-menu, literal true
        runScript(std::string("AI/AIBehaviours.ai"));
    }

    // the object sweep and the material settings (0x80CEF28-0x80CF1B0)
    if (!loadAllConFiles(std::string("Objects/")))             // virtual +0x2EC - ITS bool gates
        return false;
    runScript(std::string("Common/Material/MaterialManagerSettings.con"));
    if (m_byte1B0)
        return false;

    // the level's own Init.con, voices, and the side tag (0x80CF264-0x80CF2B9)
    if (!loadInitFile(levelPath))                              // virtual +0x2E8 - gates the rest
        return false;
    loadVoiceMessages();                                       // virtual +0x2F0
    if (m_byte1B0)
        return false;
    const char* side = isServer() ? "host" : "client";         // the cmov at 0x80CF2B9

    // the AI chain (0x80D0514-0x80D1D5A, hoisted; runs here, before the object scripts)
    if (ai::g_aiInstance && ai::aiInstance()->isBotsGame())
    {
        runScript(std::string("AI/AIRadioMessages.ai"));
        if (chosenSize)
        {
            const std::string aiDir = formatString("GameModes/%s/%d", gameMode.c_str(), chosenSize);
            runScript(levelPath + aiDir + "/AI/StrategicAreas.ai");
            // This script belongs to the mounted level archive and requires the level prefix;
            // a bare AI/AI.ai would resolve against the mod root.
            runScript(levelPath + std::string("AI/AI.ai"));
            const std::string strategies = levelPath + aiDir + "/AI/Strategies.ai";
            if (io::fileManager->fileExists(strategies))           // optional, silently skipped
                runScript(strategies);
            runScript(std::string("AI/AIDefaultStrategies.ai"));
            runScript(std::string("AI/AIDefaultStrategiesAdd.ai"));
        }
        if (m_byte1B0)
            return false;
    }

    // the object scripts (0x80CF2E0-0x80CFA92): tear down the old world at the fork point,
    //      then StaticObjects -> convert -> GamePlayObjects -> score
    world::objectManager->destroyObjects();                    // +0x98
    runScript(levelPath + "StaticObjects.con");
    if (m_byte1B0 || !convertDynamicObjects())
        return false;

    const std::string sizeDir = formatString("GameModes/%s/%d/", gameMode.c_str(), chosenSize);
    const bool gpoLoaded = runScriptArg1(levelPath + sizeDir + "GamePlayObjects.con",
                                         std::string(side));   // v_arg1 = host/client
    if (!gpoLoaded)
    {
        // dice's own stray 'e' in the failure message only - preserved, do not fix.
        Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 1272)
            << "Unable to load " << levelPath << sizeDir << "GamePlayeObjects.con";
    }
    if (m_byte1B0)
        return false;
    runScript(std::string("Common/Score/ScoreManagerSettings.con"));

    // server-only triggers (0x80CFB95: isServer() true reaches the load)
    if (isServer())
        runScriptArg1(levelPath + "Triggerables.con", std::string(side));

    // the precache bracket (0x80CFBB2-0x80CFC8C)
    world::objectManager->destroyObjects();
    world::objectManager->getAllRegisteredObjects();           // +0xB4, result discarded
    world::ObjectTemplate::setIsPreCaching(true);              // the static global twin
    m_isPrecaching = true;
    world::objectTemplateManager->preCacheObjects();           // +0x74
    if (m_byte1B0 || !preCacheSpawnedTemplates()               // +0x2DC
                  || !setCreatedOnLoadFlag()                   // +0x2F4
                  || !loadUpdateComponents())                  // +0x2E4
        return false;
    clearConsole();                                            // +0x2E0
    world::objectManager->destroyObjects();
    world::objectManager->getAllRegisteredObjects();
    m_isPrecaching = false;
    world::ObjectTemplate::setIsPreCaching(false);

    // the tail (0x80CFD0C-0x80CFD90)
    if (isServer())
        VoipServerConnection::GetInstance()->ResetPlayerRouting();
    if (m_byte1B0)
        return false;

    // Allocate free cameras after level scripts have created their template. The
    // constructor may run before that template exists and leave allocation pending.
    const std::list<world::IPlayer*>& players =
        world::playerManager->getPlayersSortedByScore();        // +0x40 - NOT getPlayers (+0x3C)
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
    {
        (*it)->allocateFreeCamera();                            // ELF IPlayer +0x308 = PE +0x304
    }

    if (world::triggerManager)
        world::triggerManager->setupTriggerLinks();
    if (world::artilleryManager)
        world::artilleryManager->init();
    if (world::uavVehicleManager)
        world::uavVehicleManager->init();

    m_isLevelLoaded   = true;                                  // +0x38
    m_isLevelLoadedXP = true;                                  // +0x39
    return true;
}

// Treats destroyables and destroyable-window bundles as dynamic; other objects
// require a mobile physics node.
bool getIsDynamicObject(world::IObject* object, const world::IObjectTemplate* objectTemplate)
{
    if (objectTemplate->getClassID() != world::CID_DestroyableObjectTemplate
        && objectTemplate->getClassID() != world::CID_DestroyableWindowsBundleTemplate)
    {
        world::IPhysicsNode* physicsNode = static_cast<world::IPhysicsNode*>(
            world::object_detail::LayoutAccess::physicsNode(object));
        if (!physicsNode || !physicsNode->getIsMobile())
            return false;
    }
    return true;
}

// Replaces dynamic root objects with cached spawner templates and server-side spawner instances.
// Walks the live registry: insertion preserves iterators, and destruction is queued until the final sweep.
// An abort request returns false during the walk. Non-camera networkables at this stage are diagnosed.
bool GameLogic::convertDynamicObjects()
{
    const std::map<unsigned int, world::IObject*>& all =
        world::objectManager->getAllRegisteredObjects();               // +0xB4
    SpawnerTemplateMap spawners;                                       // the local, keyed by template id

    int destroyableLimit = 100;
    settings->intGet("GSDestroyableObjectsLimit", destroyableLimit);   // read, then never used -
    int windowsLimit = 50;                                             // preserved as shipped
    settings->intGet("GSDestroyableWindowsBundlesLimit", windowsLimit);
    int dynamicLimit = 50;
    settings->intGet("GSDynamicObjectsLimit", dynamicLimit);

    for (std::map<unsigned int, world::IObject*>::const_iterator it = all.begin();
         it != all.end(); ++it)
    {
        if (m_byte1B0)                                                 // the abort poll
            return false;
        convertOneDynamicObject(it->second, spawners);
    }

    world::objectManager->destroyObjects();                            // +0x98
    return true;
}

// Converts one eligible dynamic object to a spawner using the caller's shared
// template map. Destroyed objects are ignored.
void GameLogic::convertOneDynamicObject(world::IObject* object, SpawnerTemplateMap& spawners)
{
    if (object->isObjectDestroyed())                                   // +0x3C
        return;
    const world::IObjectTemplate* objectTemplate =
        world::object_detail::LayoutAccess::objectTemplate(object);    // +0x14, raw

    const bool isRoot = (world::object_detail::LayoutAccess::flags(object) & 0x01000000u) != 0;
    if (isRoot && getIsDynamicObject(object, objectTemplate))
    {
        world::ObjectSpawnerTemplate* spawner = 0;
        const int templateId = static_cast<int>(objectTemplate->getId());   // +0x44
        SpawnerTemplateMap::iterator found = spawners.find(templateId);
        if (found == spawners.end())
        {
            std::string name(objectTemplate->getName());               // +0x2C
            name.append("_dynamic_objectSpawner");
            spawner = static_cast<world::ObjectSpawnerTemplate*>(
                world::objectTemplateManager->createTemplate(world::CID_ObjectSpawnerTemplate, name));   // +0x58
            if (spawner)
            {
                spawner->setObjectTemplate(std::string(objectTemplate->getName()));   // +0x118
                spawner->setDistance(15.0f);                           // +0x14C
                spawner->setMaxSpawnDelay(0.0f);                       // +0x12C
                spawner->setMinSpawnDelay(0.0f);                       // +0x134
                spawner->setMaxNrOfObjectSpawned(1);                   // +0x174
                spawner->setNrOfObjectToSpawn(1);                      // +0x164
                if (objectTemplate->getClassID() == world::CID_DestroyableObjectTemplate)
                {
                    const world::DestroyableObjectTemplate* t =
                        static_cast<const world::DestroyableObjectTemplate*>(objectTemplate);
                    spawner->m_byte1C2 = false;
                    spawner->setSpawnDelay(t->respawnDelay);           // +0x124 <- +0x180
                    spawner->setNrOfObjectToSpawn(t->nrOfRespawns);    // +0x164 <- +0x184
                }
                if (objectTemplate->getClassID() == world::CID_DynamicBundleTemplate)
                {
                    const world::DynamicBundleTemplate* t =
                        static_cast<const world::DynamicBundleTemplate*>(objectTemplate);
                    // "never respawns" = the delay is exactly -1: |delay + 1| <= FLT_EPSILON
                    spawner->m_byte1C2 = std::fabs(t->respawnDelay + 1.0f) <= FLT_EPSILON;
                    spawner->setSpawnDelay(t->respawnDelay);           // +0x124 <- +0x17C
                }
                if (objectTemplate->getClassID() == world::CID_DestroyableWindowsBundleTemplate)
                {
                    const world::DestroyableWindowsBundleTemplate* t =
                        static_cast<const world::DestroyableWindowsBundleTemplate*>(objectTemplate);
                    spawner->m_byte1C2 = false;
                    spawner->setSpawnDelay(t->respawnDelay);           // +0x124 <- +0x180
                }
                spawners.insert(std::make_pair(static_cast<int>(objectTemplate->getId()), spawner));
            }
        }
        else
        {
            spawner = found->second;
        }

        if (isServer())                                                // +0x13C
        {
            // (getClassID() twice, compared against CID_DestroyableObjectTemplate, result unused)
            world::IObject* spawnerObject = spawner->createObject();   // +0x54 - an ObjectSpawner
            if (spawnerObject)
            {
                spawnerObject->setAbsoluteTransformation(object->getAbsoluteTransformation());   // +0x8C <- +0x84
                spawnerObject->init();                                 // +0x20
            }
        }
        object->destroy(true);                                         // +0x34 - queued; swept after the walk
    }
    else if (object->getNetworkable()                                  // +0xD8
             && objectTemplate->getClassID() != world::CID_FreeCameraTemplate)
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 1410)
            << "Networkable objects can't be created at this stage, possible staticObjects.con holds vehicles. Template:"
            << objectTemplate->getName();
    }
}

// Registers the point's template-defined controller ID in its group. Duplicate IDs
// do not replace entries. The controller-ID call is deliberately non-virtual.
void GameLogic::addControlPoint(world::IObject* cp, int group)
{
    const unsigned int id =
        static_cast<world::ControlPoint*>(cp)->world::ControlPoint::getControlerId();

    m_controlPointMap.insert(std::make_pair(id, cp));
    m_controlPointGroups[group].insert(static_cast<int>(id));
}

// getControlPointMap - ELF 0x80D5B10 (11 B), PE 0x464F30 (4 B), slot +0x174. Both ports return
// the address of the registry itself. The source-level map operations stay native to each port;
// no retail STL representation crosses this interface.
std::map<unsigned int, world::IObject*>* GameLogic::getControlPointMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_controlPointMap);
}

// getControlPoint - ELF 0x80D5D10 (67 B). A straight map lookup; an absent id is represented by
// null and does not mutate either registry.
world::IObject* GameLogic::getControlPoint(unsigned int id)
{
    std::map<unsigned int, world::IObject*>::iterator it = m_controlPointMap.find(id);
    return it == m_controlPointMap.end() ? 0 : it->second;
}

// getControlPointsInGroup - ELF 0x80D5D60 (994 B), dedicated PE 0x46B930 and client PE 0x4F6010
// (391 B). operator[] deliberately creates an empty group for an unknown id. Each listed control
// point is looked up twice in the success arm, as shipped; a stale group entry emits the retail
// fatal diagnostic and returns false. The client map painter tests AL after the +0x180 dispatch.
bool GameLogic::getControlPointsInGroup(std::vector<world::IObject*>& out, int group)
{
    const std::set<int>& ids = m_controlPointGroups[group];
    for (std::set<int>::const_iterator it = ids.begin(); it != ids.end(); ++it)
    {
        if (!getControlPoint(static_cast<unsigned int>(*it)))
        {
            Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 2205)
                << "Control point not found even though its in group " << group;
            return false;
        }
        out.push_back(getControlPoint(static_cast<unsigned int>(*it)));
    }
    return true;
}

// getNumberOfControlPointGroups - ELF 0x80D6150 (14 B), PE 0x4657F0 (4 B), slot +0x184. The
// different raw member offsets in the two images are their different std::map layouts; both return
// map::size(), including empty groups deliberately materialised by getControlPointsInGroup.
int GameLogic::getNumberOfControlPointGroups()
{
    return static_cast<int>(m_controlPointGroups.size());
}

// Removes a registered point by ID, then erases it from the template's current
// group. operator[] may create an empty group set on this removal path.
void GameLogic::removeControlPoint(world::IObject* cp)
{
    typedef world::object_detail::LayoutAccess L;

    const unsigned int id =
        static_cast<world::ControlPoint*>(cp)->world::ControlPoint::getControlerId();

    std::map<unsigned int, world::IObject*>::iterator it = m_controlPointMap.find(id);
    if (it == m_controlPointMap.end())
        return;
    m_controlPointMap.erase(it);

    // The ELF vcalls the template's slot +0x1E4 = getGroupId(). The template pointer is the raw
    // IObject +0x14 field; getGroupId is non-const, hence the cast pair.
    const world::ControlPointTemplate* t =
        static_cast<const world::ControlPointTemplate*>(L::objectTemplate(cp));
    const int group = const_cast<world::ControlPointTemplate*>(t)->getGroupId();
    m_controlPointGroups[group].erase(static_cast<int>(id));
}

// Registers by object ID, retaining the first pointer for duplicate IDs. Removal
// uses only the ID even if the supplied object differs from the registered pointer.
void GameLogic::addFlag(world::IObject* flag)
{
    m_flagMap.insert(std::make_pair(flag->getObjectId(), flag));
}

void* GameLogic::getFlagMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_flagMap);
}

void GameLogic::removeFlag(world::IObject* flag)
{
    const auto it = m_flagMap.find(flag->getObjectId());
    if (it != m_flagMap.end())
        m_flagMap.erase(it);
}

void GameLogic::addPco(world::IObject* pco)
{
    m_playerControlObjectMap.insert(std::make_pair(pco->getObjectId(), pco));
}

// Returns borrowed mutable storage for the player-control-object registry.
std::map<unsigned int, world::IObject*>* GameLogic::getPcoMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_playerControlObjectMap);
}

void GameLogic::removePco(world::IObject* pco)
{
    std::map<unsigned int, world::IObject*>::iterator it =
        m_playerControlObjectMap.find(pco->getObjectId());
    if (it != m_playerControlObjectMap.end())
        m_playerControlObjectMap.erase(it);
}

// Returns the mutable strategic-object map; callers update its holders.
std::map<std::string, world::StrategicObjectHolder*>*
GameLogic::getStrategicObjectMap()
{
    return &m_strategicObjectMap;
}

//  / PE 0x467FC0. Keys append one wrapping byte ('0' + index), including
// punctuation after index nine. A hole at vector index zero admits another piece regardless
// of distance; isOnePiece prevents any reuse. Equality at twice the radius sum does not join.
void GameLogic::addStrategicObject(world::IObject* object)
{
    if (m_isPrecaching)
        return;
    auto* component = static_cast<world::IStrategicObjectComp*>(
        world::object_detail::LayoutAccess::strategicObjectComponent(object));
    if (!component)
        return;

    const std::string& name = component->getStrategicObjectName();
    for (unsigned int index = 0; ; ++index)
    {
        const std::string key = name + static_cast<char>(index + '0');
        const auto it = m_strategicObjectMap.find(key);
        if (it == m_strategicObjectMap.end())
        {
            auto* holder = new world::StrategicObjectHolder;
            holder->addObject(object);
            holder->m_index = static_cast<int>(index);
            m_strategicObjectMap.insert(std::make_pair(key, holder));
            component->setIndex(static_cast<int>(index));
            static_cast<world::StrategicObjectComp*>(component)->m_holder = holder;
            if (component->has3dMapIcon())
                SingletonServer::m_iNametagManager->addTouchItem(object);
            return;
        }
        if (!component->isOnePiece())
        {
            world::IObject* first = it->second->getObjectByIndex(0);
            if (first)
            {
                const float radius = object->getPhysicBoundingRadius();
                const float firstRadius = first->getPhysicBoundingRadius();
                // [PLATFORM] Both call orders are read: PE resolves first's position before
                // object's; ELF resolves object's before first's. Keep those virtual calls.
#if defined(_WIN32)
                const Vec3& firstPosition = first->getAbsolutePosition();
                const Vec3& position = object->getAbsolutePosition();
#else
                const Vec3& position = object->getAbsolutePosition();
                const Vec3& firstPosition = first->getAbsolutePosition();
#endif
                const float x = position.x - firstPosition.x;
                const float y = position.y - firstPosition.y;
                const float z = position.z - firstPosition.z;
#if defined(_WIN32)
                const float distance = std::sqrt(z*z + x*x + y*y);
#else
                const float distance = std::sqrt(x*x + y*y + z*z);
#endif
                if (!((radius + firstRadius) * 2.0f > distance))
                    continue;
            }
            it->second->addObject(object);
            component->setIndex(static_cast<int>(index));
            static_cast<world::StrategicObjectComp*>(component)->m_holder = it->second;
            if (component->has3dMapIcon())
                SingletonServer::m_iNametagManager->addTouchItem(object);
            return;
        }
    }
}

//  / PE 0x4684F0. Remove the nametag before looking up the holder. Deleting the
// last live piece leaves the component's holder pointer/index untouched, as both ports do.
void GameLogic::removeStrategicObject(world::IObject* object)
{
    auto* component = static_cast<world::IStrategicObjectComp*>(
        world::object_detail::LayoutAccess::strategicObjectComponent(object));
    if (!component)
        return;
    SingletonServer::m_iNametagManager->removeItem(2, object);
    const std::string name = component->getStrategicObjectName();
    const std::string key = name + static_cast<char>(static_cast<unsigned int>(component->getIndex()) + '0');
    const auto it = m_strategicObjectMap.find(key);
    if (it != m_strategicObjectMap.end())
    {
        it->second->removeObject(object);
        if (it->second->m_nrObjects == 0)
        {
            delete it->second;
            m_strategicObjectMap.erase(it);
        }
    }
}

//  / PE 0x464B30: unsigned comparison/subtraction and TWO tick reads on the
// future-state arm. PE uses the single-precision reciprocal of 30; ELF divides by 30.
float GameLogic::getTimeToNextStateChange()
{
    if (m_nextStateChangeAtTick > WorldPref::getTickCount())
    {
        const unsigned int ticks = m_nextStateChangeAtTick - WorldPref::getTickCount();
#if defined(_WIN32)
        return static_cast<float>(ticks) * 0.033333335f;
#else
        return static_cast<float>(ticks) / 30.0f;
#endif
    }
    return 0.0f;
}

//  / PE 0x464B80: guard only the global manager. Its real refresh body remains
// a named declaration-only frontier, already called by reset; this wrapper adds no fallback.
void GameLogic::refreshTriggerables()
{
    if (world::triggerableManager)
        world::triggerableManager->refresh();
}

//  / PE 0x465400. The local player is optional; 255 is the absent-player id.
// Return type corrected from void: PE explicitly sets AL=1 after the send (ELF sets EAX=1).
bool GameLogic::remoteCommand(int command, int arg0, int arg1, int arg2, int arg3)
{
    int playerId = 255;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player)
        playerId = player->getId();
    game->sendGameEvent(-1, new PythonCommandEvent(command, playerId, true, false,
                                                  arg0, arg1, arg2, arg3));
    return true;
}

// Spawner registry keyed by object ID. Duplicate insertion retains the first
// pointer; failed lookup returns null.
void GameLogic::addObjectSpawner(world::IObject* spawner)
{
    m_objectSpawnerMap.insert(std::make_pair(spawner->getObjectId(), spawner));
}

void* GameLogic::getObjectSpawnerMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_objectSpawnerMap);
}

void GameLogic::removeObjectSpawner(world::IObject* spawner)
{
    std::map<unsigned int, world::IObject*>::iterator it =
        m_objectSpawnerMap.find(spawner->getObjectId());
    if (it != m_objectSpawnerMap.end())
        m_objectSpawnerMap.erase(it);
}

world::IObject* GameLogic::getObjectSpawner(unsigned int id) const
{
    std::map<unsigned int, world::IObject*>::const_iterator it = m_objectSpawnerMap.find(id);
    return it != m_objectSpawnerMap.end() ? it->second : 0;
}

// and its `movzx eax, byte [this+14Ch]` is an independent confirmation of
// m_isPrecaching's ELF offset. load() brackets the precache tail with this flag.
bool GameLogic::isPrecaching() const
{
    return m_isPrecaching;
}

//  / PE 0x4EC780: one byte store. The paired getter is ELF 0x80D8500 /
//  and is the IGameLogic +0x22C slot.
void GameLogic::setEnableVoHelp(bool enable)
{
    m_enableVoHelp = enable;
}

bool GameLogic::getEnableVoHelp()
{
    return m_enableVoHelp;
}

// Clears the two teams' drop-vehicle names while retaining counts and scatter radii.
// Team accessors use IDs 1 and 2. [UNRESOLVED] The reason for retaining the other state is unknown.
// [CHOSEN] The return type is void.
void GameLogic::ClearDropVehicles()
{
    m_teamDropVehicle[0].clear();                    // +0x0F0  (team 1)
    m_teamDropVehicle[1].clear();                    // +0x0F4  (team 2)
}

// Sets a team's drop vehicle, then clears it when either no-vehicles mode is active.
// Requires team 1 or 2: the assignment checks the range, but the later clear does not.
void GameLogic::setTeamDropVehicle(int team, const std::string& name)
{
    if (static_cast<unsigned int>(team - 1) <= 1)
        m_teamDropVehicle[team - 1].assign(name);

    // NO range check ON this path - see the block above.
    if (serverSettings->getNoVehicles() || getNoVehiclesMode())
        m_teamDropVehicle[team - 1].assign("", 0);
}

// Returns the team's drop-vehicle name by reference for teams 1–2, otherwise a
// static empty string.
const std::string& GameLogic::getTeamDropVehicle(int team) const
{
    if (static_cast<unsigned int>(team - 1) <= 1)
        return m_teamDropVehicle[team - 1];

    static const std::string s_noDropVehicle;    // ds:0x8AAC318 - see getTeamFlag's note
    return s_noDropVehicle;
}

// Stores or reads a team's drop-vehicle count. Invalid teams ignore writes and read zero.
void GameLogic::setNumDropVehiclesForTeam(int team, int count)
{
    if (static_cast<unsigned int>(team - 1) <= 1)
        m_numDropVehiclesForTeam[team - 1] = count;
}

int GameLogic::getNumDropVehiclesForTeam(int team)
{
    if (static_cast<unsigned int>(team - 1) <= 1)
        return m_numDropVehiclesForTeam[team - 1];
    return 0;
}

// Stores minimum radius first and outer radius second, matching console argument order.
void GameLogic::setDropVehicleDeviationRadius(int minRadius, int radius)
{
    m_dropVehicleDeviationMinRadius = minRadius;             // +0x104  arg ONE
    m_dropVehicleDeviationRadius    = radius;                // +0x100  arg TWO
}

int GameLogic::getDropVehicleDeviationRadius()    { return m_dropVehicleDeviationRadius; }
int GameLogic::getDropVehicleDeviationMinRadius() { return m_dropVehicleDeviationMinRadius; }

//  / 0x80C6970 and PE 0x45BB70 / 0x45BB80. The generated
// gameLogic.extraUAVVehicleFlightHeight property dispatches through these two virtual slots.
void GameLogic::setExtraUAVVehicleFlightHeight(float height)
{
    m_extraUAVVehicleFlightHeight = height;
}

float GameLogic::getExtraUAVVehicleFlightHeight() const
{
    return m_extraUAVVehicleFlightHeight;
}

// Reads view distance and fog from settings on every call. View distance defaults
// to 850 here, independently of other callers' defaults. [UNRESOLVED] Lifetime of
// the fog reference when the repository returns the supplied local fallback.
float GameLogic::getMaximumLevelViewDistance() const
{
    return static_cast<float>(settings->intGet(std::string("GSViewDistance"), 850));
}

//  / PE 0x465670, including PE clamp helper 0x402500. Clamp to [20,3000],
// truncate toward zero, then store the integer GSViewDistance setting. No renderer call follows.
// [PLATFORM] PE compares against the maximum first, then takes the minimum on unordered; its
// NaN result is 20. ELF compares the minimum first and leaves NaN for cvttss2si to consume.
namespace {
// [CHOSEN factoring] the two retail clamps above followed by their integer conversion.
int maximumLevelViewDistanceSetting(float distance)
{
#if defined(_WIN32)
    if (distance >= 3000.0f)
        distance = 3000.0f;
    if (!(distance >= 20.0f))
        distance = 20.0f;
#else
    if (distance < 20.0f)
        distance = 20.0f;
    else if (distance > 3000.0f)
        distance = 3000.0f;
    // ELF's cvttss2si returns the signed indefinite value for unordered input. Spell that
    // result explicitly so the native C++ conversion never receives an out-of-range NaN.
    if (distance != distance)
        return (-2147483647 - 1);
#endif
    return static_cast<int>(distance);
}
}

void GameLogic::setMaximumLevelViewDistance(float distance)
{
    const int value = maximumLevelViewDistanceSetting(distance);
    settings->intSet(std::string("GSViewDistance"), value);
}

// Direct field accessors without validation or clamping.
bool GameLogic::getAirMouseInverted()
{
    return m_airMouseInverted;
}

float GameLogic::getAirMouseSensitivity()
{
    return m_airMouseSensitivity;
}

void GameLogic::setAirMouseSensitivity(float value)
{
    m_airMouseSensitivity = value;
}

int GameLogic::getFeslEnvironment()
{
    return m_feslEnvironment;
}

void GameLogic::setFeslEnvironment(int environment)
{
    m_feslEnvironment = environment;
}

// [CHOSEN] Cast preserves the frozen interface signature for an opaque DecalObject.
// Only decal helpers may consume it; it is not an IObject.
world::IObject* GameLogic::getGroundDecalObject()    //  / PE folded 0x5B4C80
{
    return static_cast<world::IObject*>(m_groundDecalObject);
}

const Vec4& GameLogic::getFogStartEndAndBase() const
{
    const Vec4 fallback = { 0.0f, 0.0f, 0.0f, 0.0f };

    return settings->Vec4Get(std::string("GSFogStartEndAndBase"), fallback);
}

// Stores fog settings and forwards them to PostProductionFrameWork when registered.
// The renderer call uses the client ABI's raw Vec4 setter seam; dedicated images
// have no such singleton.
void GameLogic::setFogStartEndAndBase(const Vec4& value)
{
    settings->Vec4Set(std::string("GSFogStartEndAndBase"), value);

    IBase* const framework = classManager->getSingleton("PostProductionFrameWork");
    if (framework)
    {
        typedef void (__thiscall* SetStateVec4Fn)(void* self, const Vec4* value);
        void** const vtbl = *reinterpret_cast<void***>(framework);
        reinterpret_cast<SetStateVec4Fn>(vtbl[0x24 / 4])(framework, &value);
    }
}

// ..0x80C6B40 / PE 0x45BC80..0x45BCD0. These six virtuals are
// intentionally getters only: retail exposes no matching setters on IGameLogic, and its
// constructor installs the fixed gameplay values recorded beside the members in GameLogic.h.
float GameLogic::getUAVActiveTime() const { return m_uavActiveTime; }
float GameLogic::getSatelliteScanActiveTime() const { return m_satelliteScanActiveTime; }
float GameLogic::getUAVReloadTime() const { return m_uavReloadTime; }
float GameLogic::getUAVRadius() const { return m_uavRadius; }
float GameLogic::getSatelliteScanReloadTime() const { return m_satelliteScanReloadTime; }
float GameLogic::getSatelliteScanVisibleTime() const { return m_satelliteScanVisibleTime; }

// ..0x80C6A0E / PE 0x45BB90..0x45BBE7. These are six literal field
// stores/loads. The physical order is supply height, lifetime, vehicle height even though the
// vtable exposes the vehicle pair before the lifetime pair.
void GameLogic::setSupplyDropHeight(float height) { m_supplyDropHeight = height; }
float GameLogic::getSupplyDropHeight() { return m_supplyDropHeight; }
void GameLogic::setVehicleDropHeight(float height) { m_vehicleDropHeight = height; }
float GameLogic::getVehicleDropHeight() { return m_vehicleDropHeight; }
void GameLogic::setSupplyDropNumSecsToLive(float seconds)
{
    m_supplyDropNumSecsToLive = seconds;
}
float GameLogic::getSupplyDropNumSecsToLive() { return m_supplyDropNumSecsToLive; }

//  / PE 0x468430. The by-value string is copied into a pair and appended; names are
// neither normalized nor de-duplicated.
void GameLogic::AddInvalidDropVehicleObject(std::string name, bool invalid)
{
    m_invalidVehicleDropObjects.insert(
        m_invalidVehicleDropObjects.end(), std::make_pair(name, invalid));
}

//  / PE 0x45BB40: a direct address return (+0x15C / +0x20C respectively).
std::list<std::pair<std::string, bool> >* GameLogic::GetInvalidVehicleDropObjectList()
{
    return &m_invalidVehicleDropObjects;
}

// Returns the team's flag-template name by reference for teams 0–2, otherwise a
// static empty string.
const std::string& GameLogic::getTeamFlag(int team) const
{
    if (static_cast<unsigned int>(team) <= 2)
        return m_teamFlag[team];

    static const std::string s_noTeamFlag;      // stands in for ds:0x8AAC318 - see the note above
    return s_noTeamFlag;
}

// setTeamFlag - ELF 0x80D76D0 (48 B), PE 0x465020 (29 B), slot +0x200. Stock level Init.con
// files use this command for teams 0..2 before ControlPointTemplate::preCache resolves each
// otherwise-empty per-template flag name through getTeamFlag. The unsigned range test is shipped:
// a negative team is ignored rather than indexing before the array.
void GameLogic::setTeamFlag(int team, const std::string& flag)
{
    if (static_cast<unsigned int>(team) <= 2)
        m_teamFlag[team] = flag;
}

// Team-language access for teams 1–2. Invalid reads return a static empty string;
// invalid writes are ignored.
const std::string& GameLogic::getTeamLanguage(int team) const
{
    if (static_cast<unsigned int>(team - 1) <= 1)
        return m_teamLanguage[team - 1];

    static const std::string s_noTeamLanguage;  // ds:0x8AAC318 - see the note above
    return s_noTeamLanguage;
}

void GameLogic::setTeamLanguage(int team, const std::string& language)
{
    if (static_cast<unsigned int>(team - 1) <= 1)
        m_teamLanguage[team - 1] = language;                   // std::string::assign, 0x80D7688
}

// getVoiceMessages - ELF 0x80D81F0, 14 B / PE 0x465980: `mov eax, [this+108h]`.
VoiceMessages* GameLogic::getVoiceMessages() const
{
    return m_voiceMessages;
}

// stopPlayingVoMessages - ELF 0x80C6A40, 56 B / PE 0x4E17A0. The null test and the second
// getVoiceMessages() call are both observable in both shipped bodies. Once the registry exists,
// its help/VO sound at ELF +0x2090 / PE +0x20EC is stopped through ISound +0x30; retail does not
// null-check that sound pointer.
void GameLogic::stopPlayingVoMessages()
{
    if (getVoiceMessages())
        getVoiceMessages()->m_helpSound->stop();
}

// Loads team and local-language voice definitions, falling back to English when the probe file is absent.
// Replaces the owned VoiceMessages instance, runs its configuration and finalizes it
// through the sound engine, including the null engine on dedicated servers.
void GameLogic::loadVoiceMessages()
{
    const std::string base("common/sound/");
    std::string language = settings->stringGet(std::string("GSLanguage"),
                                               std::string("English"));

    {
        std::string probe = base + language;                                      // operator+ 0x80D4C5D
        probe += "/Help_VO/HUD_HELP_BATTLERECORDER_controlsQ.ogg";                // append 0x80D4C9E
        io::FileInfo info;
        if (!io::fileManager->getFileInfo(probe, info))                           // +0x38
            language = "English";                                                 // assign 0x80D4D1B
    }

    std::string team1Root = base;
    team1Root += getTeamLanguage(1);                                              // virtual +0x1FC
    std::string team2Root = base;
    team2Root += getTeamLanguage(2);
    std::string voRoot = base;
    voRoot += language;

    VoiceMessages* messages = new VoiceMessages(team1Root, team2Root, voRoot);   // new(0x20C8), 0x80D4D8A
    if (messages != m_voiceMessages)
    {
        if (m_voiceMessages)
            delete m_voiceMessages;                                               // ~VoiceMessages + delete
        m_voiceMessages = messages;                                               // [this+0x108], 0x80D4DCA
    }

    runScript(std::string("Common/Sound/VoiceMessages.con"));                     // OldConsole::run, 0x80D4F07
    m_voiceMessages->finalize(g_bfSound->engine());
}

// Spawner creation checks this flag as well as the ServerSettings noVehicles setting.
bool GameLogic::getNoVehiclesMode()
{
    return m_noVehiclesMode;                                       // movzx eax, byte ptr [this+1E8h]
}

void GameLogic::setNoVehiclesMode(bool enabled)
{
    m_noVehiclesMode = enabled;                                    // mov [this+1E8h], dl
}

// Lowercases the by-value name before list access. Append allows duplicates;
// lookup returns on the first exact match.
void GameLogic::noVehiclesOmit(std::string name)
{
    name = getLowerCase(name);
    m_noVehiclesOmitList.insert(m_noVehiclesOmitList.end(), name);
}

bool GameLogic::isOmitted(std::string name)
{
    name = getLowerCase(name);
    for (std::list<std::string>::const_iterator it = m_noVehiclesOmitList.begin();
         it != m_noVehiclesOmitList.end(); ++it)
    {
        if (*it == name)
            return true;
    }
    return false;
}

// addPreStoredTemplate - ELF 0x80CC550, 188 B. The object-cache pre-warm list createPreStoredObjects
// drains (m_preStoredObjects, +0x164): find-or-insert the template id at 0, add `count`, and when
// `limit` is non-zero clamp the running total to it.
void GameLogic::addPreStoredTemplate(unsigned int templateId, unsigned int count, unsigned int limit)
{
    std::map<unsigned int, unsigned int>::iterator it = m_preStoredObjects.find(templateId);
    if (it == m_preStoredObjects.end())
    {
        m_preStoredObjects.insert(std::make_pair(templateId, 0u));
        it = m_preStoredObjects.find(templateId);
    }
    it->second += count;
    if (limit != 0 && it->second > limit)
        it->second = limit;
}

// Pre-caches unique kit, soldier, and available order-marker templates. Kits and
// soldiers also reserve 64 cached instances. Polls the load-abort flag before each
// lookup and pre-cache operation; returns false if aborted.
bool GameLogic::preCacheSpawnedTemplates()
{
    std::set<const world::IObjectTemplate*> templates;

    for (std::map<int, std::string>::const_iterator it = m_kits.begin(); it != m_kits.end(); ++it)
    {
        if (m_byte1B0)
            return false;
        const world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(it->second);
        if (t)
        {
            templates.insert(t);
            addPreStoredTemplate(t->getId(), 64, 64);
        }
    }
    for (std::map<int, std::string>::const_iterator it = m_soldierSkins.begin();
         it != m_soldierSkins.end(); ++it)
    {
        if (m_byte1B0)
            return false;
        const world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(it->second);
        if (t)
        {
            templates.insert(t);
            addPreStoredTemplate(t->getId(), 64, 64);
        }
    }

    const world::IObjectTemplate* move =
        world::objectTemplateManager->getTemplate(std::string("ordermarker_move"));
    const world::IObjectTemplate* defend =
        world::objectTemplateManager->getTemplate(std::string("ordermarker_defend"));
    const world::IObjectTemplate* attack =
        world::objectTemplateManager->getTemplate(std::string("ordermarker_attack"));
    if (move)
        templates.insert(move);
    if (defend)
        templates.insert(defend);
    if (attack)
        templates.insert(attack);

    for (std::set<const world::IObjectTemplate*>::const_iterator it = templates.begin();
         it != templates.end(); ++it)
    {
        if (m_byte1B0)
            return false;
        (*it)->preCache();                                         // +0x6C
    }
    return true;
}

// clearConsole - ELF 0x80D3590, 166 B. Step 17 of load. Flushes the console's history by setting
// its maximum size to zero and straight back to what it was, then prints thirty lines of a single
// space (the counter runs 29..0) so the screen is blank once the load is done.
void GameLogic::clearConsole()
{
    const int maxHistory = io::mainConsole->getMaxHistorySize();    // +108
    io::mainConsole->setMaxHistorySize(0);                           // +104
    io::mainConsole->setMaxHistorySize(maxHistory);
    for (int i = 29; i >= 0; --i)
        io::mainConsole->output(std::string(" "));                   // +20
}

// getDefaultLevel / setDefaultLevel - ELF 0x80C68C0 (13 B) / 0x80C68A0 (20 B): the std::string at
// +0x1E4, returned BY reference (`add eax, 1E4h; retn` - no sret) and assigned in place (a tail
// jump to std::string::assign). The constructor seeds it "gulf_of_oman".
const std::string& GameLogic::getDefaultLevel() const
{
    return m_defaultLevel;
}

void GameLogic::setDefaultLevel(const std::string& level)
{
    m_defaultLevel = level;
}

// Registers targets by TargetObject's designation id, not the object's network id.
// Duplicate ids retain the first object. getTargetObjectMap exposes the map through void*.
void GameLogic::addTargetObject(world::IObject* target)
{
    const unsigned int id = static_cast<unsigned int>(
        static_cast<world::TargetObject*>(target)->getTargetObjectId());
    m_targetObjectMap.insert(std::make_pair(id, target));        // insert_unique - no overwrite
}

void* GameLogic::getTargetObjectMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_targetObjectMap);
}

world::IObject* GameLogic::getTargetObject(unsigned int id) const
{
    std::map<unsigned int, world::IObject*>::const_iterator it = m_targetObjectMap.find(id);
    if (it == m_targetObjectMap.end())
        return 0;
    return it->second;
}

void GameLogic::removeTargetObject(world::IObject* target)
{
    const unsigned int id = static_cast<unsigned int>(
        static_cast<world::TargetObject*>(target)->getTargetObjectId());
    std::map<unsigned int, world::IObject*>::iterator it = m_targetObjectMap.find(id);
    if (it != m_targetObjectMap.end())
        m_targetObjectMap.erase(it);      // BY ID, not by pointer: a stale duplicate can unhook
                                          //   the registered one. The ELF does exactly this.
}

// Selects the first registry entry with the lowest score below 9999.
// Type 3 must match the weapon's current designator target. The trailing flags
// control terrain occlusion, object occlusion, and same-team rejection.
// Terrain hits within 0.1 m are clear; object tests exclude both endpoint roots.
// Keep the repeated position/root reads and dot-product clamps for NaN behavior.
world::IObject* GameLogic::findBestTargetObject(
    const Vec3& position, const Vec3& direction, world::TargetType targetType,
    float maxDistance, float maxAngle, world::IObject* launcherRoot,
    world::IObject* launcherWeapon, float directionBonus,
    const world::IObject* ignoredObject, bool testTerrain,
    bool testObjects, bool ignoreFriendly)
{
    typedef std::map<unsigned int, world::IObject*> TargetObjectMap;

    float bestScore = 9999.0f;
    world::IObject* bestTarget = 0;
    TargetObjectMap* const targets = static_cast<TargetObjectMap*>(
        gameLogic->getTargetObjectMap());

    for (TargetObjectMap::iterator it = targets->begin(); it != targets->end(); ++it)
    {
        world::TargetObject* const target = static_cast<world::TargetObject*>(it->second);
        if (target->getTargetType() != static_cast<int>(targetType))
            continue;
        if (!target->getTargetActive())
            continue;

        if (static_cast<int>(targetType) == 3)
        {
            if (!launcherWeapon)
                continue;
            world::IWeaponObject* const weapon = static_cast<world::IWeaponObject*>(
                launcherWeapon->queryInterface(world::IID_IWeaponObject));
            if (!weapon)
                continue;
            world::IObject* const designatedTarget = weapon->getTargetObject();
            if (!designatedTarget || designatedTarget != target)
                continue;
        }

        world::IObject* const targetRoot = world::getRootParent(target);
        if (launcherRoot == targetRoot)
            continue;

        if (ignoreFriendly)
        {
            world::IPlayerControlObject* targetPco = 0;
            for (world::IObject* object = targetRoot; object; object = object->getParent())
            {
                targetPco = static_cast<world::IPlayerControlObject*>(
                    object->queryInterface(world::IID_IPlayerControlObject));
                if (targetPco)
                    break;
            }

            world::IPlayerControlObject* launcherPco = 0;
            for (world::IObject* object = launcherRoot; object; object = object->getParent())
            {
                launcherPco = static_cast<world::IPlayerControlObject*>(
                    object->queryInterface(world::IID_IPlayerControlObject));
                if (launcherPco)
                    break;
            }

            if (launcherPco && targetPco
                && launcherPco->getTeam() == targetPco->getTeam())
                continue;
        }

        const Vec3& targetPosition = target->getAbsolutePosition();
        Vec3 toTarget = {
            targetPosition.x - position.x,
            targetPosition.y - position.y,
            targetPosition.z - position.z
        };
        const float distance = static_cast<float>(std::sqrt(
            toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z));
        if (distance > maxDistance)
            continue;

        const float lengthSquared =
            toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
        if (std::fabs(lengthSquared - 1.0f) >= FLT_EPSILON)
        {
            if (std::fabs(lengthSquared) < FLT_EPSILON)
            {
                toTarget.x = 0.0f;
                toTarget.y = 0.0f;
                toTarget.z = 0.0f;
            }
            else
            {
                const float inverseLength =
                    1.0f / static_cast<float>(std::sqrt(lengthSquared));
                toTarget.x *= inverseLength;
                toTarget.y *= inverseLength;
                toTarget.z *= inverseLength;
            }
        }

        float cosine = toTarget.x * direction.x
                     + toTarget.y * direction.y
                     + toTarget.z * direction.z;
        for (int clampPass = 0; clampPass != 3; ++clampPass)
        {
            if (cosine >= 1.0f)
                cosine = 1.0f;
            if (cosine < -1.0f)
                cosine = -1.0f;
        }
        const float angle = std::acos(cosine);
        if (angle > maxAngle)
            continue;

        if (testTerrain)
        {
            Vec3 terrainHit = { 0.0f, 0.0f, 0.0f };
            Vec3 terrainNormal = { 0.0f, 0.0f, 0.0f };
            float terrainFraction = 0.0f;
            unsigned char terrainMaterial;
            if (static_cast<world::IHeightmapCluster*>(world::heightmapCluster)
                    ->intersectRayInWorldCoords(position, target->getAbsolutePosition(),
                                                terrainHit, terrainFraction, terrainNormal,
                                                terrainMaterial, false))
            {
                const Vec3& repeatedTargetPosition = target->getAbsolutePosition();
                const float hitX = terrainHit.x - repeatedTargetPosition.x;
                const float hitY = terrainHit.y - repeatedTargetPosition.y;
                const float hitZ = terrainHit.z - repeatedTargetPosition.z;
                if (!(std::sqrt(hitX * hitX + hitY * hitY + hitZ * hitZ) < 0.1f))
                    continue;
            }
        }

        if (testObjects)
        {
            world::IObject* hitObject = 0;
            Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
            Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
            int hitPart = 0;
            const Vec3& rayTargetPosition = target->getAbsolutePosition();
            Vec3 rayDirection = {
                rayTargetPosition.x - position.x,
                rayTargetPosition.y - position.y,
                rayTargetPosition.z - position.z
            };
            world::IObject* const ignoredRoot = world::getRootParent(ignoredObject);
            world::IObject* const repeatedTargetRoot = world::getRootParent(target);
            world::ProjectileVsTargetPredicator predicator(
                0x100u, 8u, ignoredRoot, repeatedTargetRoot);
            if (world::objectManager->intersectLine(
                    hitObject, hitPoint, hitNormal, hitPart, position, rayDirection,
                    &predicator, true, false, false, true, 0u))
                continue;
        }

        const float score = angle * 500.0f * directionBonus
                          + (1.0f - directionBonus) * distance;
        if (bestScore > score)
        {
            bestScore = score;
            bestTarget = target;
        }
    }
    return bestTarget;
}

// storeVoipServerInfo / getVoipServerInfo - 0x80D5290 (57 B) / 0x80D52D0 (68 B),
// five stores / five loads on +0x170 / +0x174 / +0x178 / +0x17A / +0x180 (GameLogic.h).
void GameLogic::storeVoipServerInfo(bool enabled, unsigned long address, unsigned short serverPort,
                                    unsigned short bfClientPort, int quality)
{
    m_voipEnabled       = enabled;                    // +0x170 (byte)
    m_voipServerAddress = (unsigned int)address;      // +0x174
    m_voipServerPort    = serverPort;                 // +0x178
    m_voipBFClientPort  = bfClientPort;               // +0x17A
    m_voipQuality       = quality;                    // +0x180
}

void GameLogic::getVoipServerInfo(bool& enabled, unsigned int& address, unsigned short& serverPort,
                                  unsigned short& bfClientPort, int& quality)
{
    enabled      = m_voipEnabled;
    address      = m_voipServerAddress;
    serverPort   = m_voipServerPort;
    bfClientPort = m_voipBFClientPort;
    quality      = m_voipQuality;
}

// Reads client VOIP state separately from the stored server-info fields.
bool GameLogic::isVoipClientRunning()
{
    return m_isVoipClientRunning;
}

namespace {
} // namespace

// : `ret`. The base's per-tick update is empty; ServerGameLogic::update does the work
// and calls this first.
void GameLogic::update(float)
{
}

//  / 0x80CCDF0 / 0x80CCE00 / 0x80CCE50 - the combat-area quartet on +0x1D0..+0x1E0.
// setActiveCombatArea first dispatches setUseActiveCombatArea(true) (vslot +0x128 - through the
// vtable, so an override sees it) and then stores the four floats.
void GameLogic::setUseActiveCombatArea(bool use)
{
    m_useActiveCombatArea = use;                       // +0x1D0
}

bool GameLogic::getUseActiveCombatArea()
{
    return m_useActiveCombatArea;
}

void GameLogic::setActiveCombatArea(float a, float b, float c, float d)
{
    setUseActiveCombatArea(true);                      // vslot +0x128
    m_activeCombatArea1 = a;                           // +0x1D4
    m_activeCombatArea2 = b;                           // +0x1D8
    m_activeCombatArea3 = c;                           // +0x1DC
    m_activeCombatArea4 = d;                           // +0x1E0
}

void GameLogic::getActiveCombatArea(float* a, float* b, float* c, float* d)
{
    *a = m_activeCombatArea1;
    *b = m_activeCombatArea2;
    *c = m_activeCombatArea3;
    *d = m_activeCombatArea4;
}

//  (12 B, byte +0x3A) / 0x80C67B0 (11 B, +0x34) / 0x80C67C0 (14 B, +0x190) / 0x80C67D0
// (17 B) / 0x80C6940 (12 B, byte +0x38) - the plain accessors.
bool GameLogic::isServer() const
{
    return m_isServer;
}

int GameLogic::getGameStatus()
{
    return m_gameStatus;
}

BeginRoundState GameLogic::getBeginRoundState()
{
    return static_cast<BeginRoundState>(m_beginRoundState);
}

// Next scheduled game-state transition tick.
unsigned int GameLogic::getNextStateChangeAtTick()
{
    return m_nextStateChangeAtTick;
}

void GameLogic::setNextStateChangeAtTick(unsigned int tick)
{
    m_nextStateChangeAtTick = tick;
}

void GameLogic::setBeginRoundState(BeginRoundState state)
{
    m_beginRoundState = static_cast<int>(state);
}

bool GameLogic::isLevelLoaded() const
{
    return m_isLevelLoaded;
}

// This accessor is non-const, unlike isLevelLoaded.
bool GameLogic::isLevelLoadedXP()
{
    return m_isLevelLoadedXP;                                  // +0x39
}

//  / 0x80C6910, 41 B each: three loads through the sret pointer - Vec3 BY value.
Vec3 GameLogic::getBeforeSpawnCameraPosition() const
{
    return m_beforeSpawnCameraPosition;                // +0x198
}

Vec3 GameLogic::getBeforeSpawnCameraDirection() const
{
    return m_beforeSpawnCameraDirection;               // +0x1A4
}

// Linux64 server ELF 0x475CF0 / win32 client 0x4ECBA0. Writes the position and direction, then
// dispatches the virtual resetPlayerFreeCameraPosition (vslot +0x214) on
// playerManager->getLocalHumanPlayer (+0x30). The shipped body does not null-check playerManager.
void GameLogic::setBeforeSpawnCamera(Vec3 position, Vec3 direction)
{
    m_beforeSpawnCameraPosition = position;            // +0x198
    m_beforeSpawnCameraDirection = direction;          // +0x1A4
    resetPlayerFreeCameraPosition(world::playerManager->getLocalHumanPlayer());   // vslot +0x214
}

//  / 0x80D6580 / 0x80D6590 and 0x80D6360 / 0x80D63A0 / 0x80D63B0 - the projectile and
// supply-depot registries, byte-for-byte the object-spawner trio's shape (insert_unique keyed by
// the object's id +0x10; find + erase; the map's own address behind the opaque void*).
void GameLogic::addProjectile(world::IObject* projectile)
{
    m_projectileMap.insert(std::make_pair(projectile->getObjectId(), projectile));
}

void* GameLogic::getProjectileMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_projectileMap);
}

void GameLogic::removeProjectile(world::IObject* projectile)
{
    std::map<unsigned int, world::IObject*>::iterator it =
        m_projectileMap.find(projectile->getObjectId());
    if (it != m_projectileMap.end())
        m_projectileMap.erase(it);
}

// Keys remote objects by their preserved ID, which survives ObjectCache parking.
// Adding/removing artillery also refreshes an existing ArtilleryManager.
void GameLogic::addRemoteControlledObject(world::IObject* object)
{
    const unsigned int id = static_cast<world::RemoteControlledObject*>(object)->m_objectId;
    m_remoteControlledObjectMap.insert(std::make_pair(id, object));
    if (static_cast<const world::RemoteControlledObjectTemplate*>(object->getTemplate())->rcType == 1)   // +0x180
        if (world::artilleryManager)
            world::artilleryManager->refresh();
}

void* GameLogic::getRemoteControlledObjectMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_remoteControlledObjectMap);
}

void GameLogic::removeRemoteControlledObject(world::IObject* object)
{
    const unsigned int id = static_cast<world::RemoteControlledObject*>(object)->m_objectId;
    std::map<unsigned int, world::IObject*>::iterator it =
        m_remoteControlledObjectMap.find(id);
    if (it != m_remoteControlledObjectMap.end())
        m_remoteControlledObjectMap.erase(it);
    if (static_cast<const world::RemoteControlledObjectTemplate*>(object->getTemplate())->rcType == 1)
        if (world::artilleryManager)
            world::artilleryManager->refresh();
}

world::IObject* GameLogic::getRemoteControlledObject(unsigned int id) const
{
    std::map<unsigned int, world::IObject*>::const_iterator it = m_remoteControlledObjectMap.find(id);
    return it == m_remoteControlledObjectMap.end() ? 0 : it->second;
}

void GameLogic::addSupplyDepot(world::IObject* depot)
{
    m_supplyDepotMap.insert(std::make_pair(depot->getObjectId(), depot));
}

void* GameLogic::getSupplyDepotMap() const
{
    return const_cast<std::map<unsigned int, world::IObject*>*>(&m_supplyDepotMap);
}

void GameLogic::removeSupplyDepot(world::IObject* depot)
{
    std::map<unsigned int, world::IObject*>::iterator it =
        m_supplyDepotMap.find(depot->getObjectId());
    if (it != m_supplyDepotMap.end())
        m_supplyDepotMap.erase(it);
}

// Resets an existing player's free camera to the before-spawn position and negated
// ZXY orientation, then selects view mode 1. Missing player/camera is ignored.
// [UNRESOLVED] Original name of view mode 1.
void GameLogic::resetPlayerFreeCameraPosition(world::IPlayer* player)
{
    if (!player)
        return;
    world::IObject* camera = player->getFreeCamera();                          // IPlayer +0xA8
    if (!camera)
        return;

    Mat4 m = Mat4::getIdentity();
    const Vec3 direction = getBeforeSpawnCameraDirection();                    // vslot +0x210
    setRotationZXY(m, -direction.z, -direction.y, -direction.x);
    const Vec3 position = getBeforeSpawnCameraPosition();                      // vslot +0x20C
    m.m[12] = position.x;
    m.m[13] = position.y;
    m.m[14] = position.z;
    camera->setAbsoluteTransformation(m);                                      // IObject +0x8C
    static_cast<world::FreeCamera*>(camera)->setViewMode(
        static_cast<world::CameraViewMode>(1), 0, false);                       // primary +0x188
}

// Drops kits from eligible living registered soldiers, excluding free cameras and
// flag-0x20 objects. Preserve both template class-ID reads.
// [PLATFORM] Typed item-container dispatch accounts for different PE/ELF base adjustments.
void GameLogic::dropAllKits()
{
    const std::map<unsigned int, world::IObject*>& objects = world::objectManager->getAllRegisteredObjects();   // +0xB4
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* object = it->second;
        if (!object->isObjectDestroyed()
            && object->getTemplate()->getClassID() != world::CID_FreeCameraTemplate
            && (object->getFlags() & 0x20) == 0
            && object->getTemplate()->getClassID() == world::CID_SoldierTemplate)
        {
            static_cast<world::Soldier*>(object)->handleDropOnItem(0, 0);      // typed [PLATFORM] dispatch above
        }
    }
}

//  / 0x80D3C90, 119 B - two of load()'s precache-bracket steps. Both: an early
// `return true` on an empty registry, else per object while !m_byte1B0 (the abort flag): the
// first marks roots (flags bit 24 = byte +0x07 bit 0) createdOnLoad (+0x78 = 1); the second calls
// updateGeometry() (slot 46, +0xB8). Aborted -> false.
bool GameLogic::setCreatedOnLoadFlag()
{
    const std::map<unsigned int, world::IObject*>& objects = world::objectManager->getAllRegisteredObjects();   // +0xB4
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (m_byte1B0)
            return false;
        world::IObject* object = it->second;
        if (object->getFlags() & 0x01000000u)
            world::object_detail::LayoutAccess::createdOnLoad(object) = true;   // +0x78
    }
    return true;
}

bool GameLogic::loadUpdateComponents()
{
    const std::map<unsigned int, world::IObject*>& objects = world::objectManager->getAllRegisteredObjects();   // +0xB4
    for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        if (m_byte1B0)
            return false;
        it->second->updateGeometry();                                          // slot 46, +0xB8
    }
    return true;
}

// the sun template: classManager->getSingleton("ObjectTemplateManager") (+0x4C),
// and when it answers, its createTemplate(className, name) (+0x5C, the string overload) with
// ("LightSource", "sunlight"). No return, no failure path - a missing manager just skips it.
void GameLogic::createSunTemplate()
{
    IBase* manager = classManager->getSingleton(std::string("ObjectTemplateManager"));
    if (manager)
        static_cast<world::ObjectTemplateManager*>(manager)->createTemplate(std::string("LightSource"),
                                                                            std::string("sunlight"));
}

// : killPlayer(player, false, true) (vslot +0x6C - ServerGameLogic's) for every
// player. 0x80CDB50, 76 B: player->reset() (IPlayer +0x19C) for every player.
void GameLogic::killAllPlayers()
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();   // +0x3C
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        killPlayer(*it, false, true);
}

void GameLogic::resetAllPlayers()
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();   // +0x3C
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        (*it)->reset();                                                          // IPlayer +0x19C
}

// Resets world objects for a round after players have relinquished soldiers, kits and vehicles.
// Occupied objects are fatal. Dynamic roots/projectiles are destroyed or returned to cache,
// then control points, depots and spawners reset before the destroy queue drains.
// Unexpected surviving dynamic roots trigger the fatal leak diagnostic.
void GameLogic::resetAllObjects()
{
    {
        const std::map<unsigned int, world::IObject*>& objects = world::objectManager->getAllRegisteredObjects();
        for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
        {
            world::IObject* object = it->second;
            if (object->isObjectDestroyed())                                   // slot 15
                continue;

            world::ISoldier* soldier =
                static_cast<world::ISoldier*>(object->queryInterface(world::IID_ISoldier));
            if (soldier)
            {
                if (soldier->getUsedVehicle() || soldier->getOccupingPlayer())  // ISoldier +0x2C / +0x30
                    abort();
                if (soldier->getKit())                                         // ISoldier +0x54
                    abort();
            }
            else if (object->getTemplate()->getClassID() == world::CID_PlayerControlObjectTemplate)
            {
                world::IPlayerControlObject* pco = 0;
                for (world::IObject* o = object; o; o = o->getParent())        // +0x28 walk
                {
                    pco = static_cast<world::IPlayerControlObject*>(o->queryInterface(world::IID_IPlayerControlObject));
                    if (pco)
                        break;
                }
                if (pco)
                {
                    if (pco->getOccupingPlayer())                              // +0x10
                        abort();
                    if (pco->isOccupied())                                     // +0x18
                        abort();
                }
            }

            const bool isRoot = (object->getFlags() & 0x01000000u) != 0;      // byte +0x07 bit 0
            if ((isRoot || object->queryInterface(world::IID_IProjectileObject))
                && !world::object_detail::LayoutAccess::createdOnLoad(object))    // +0x78
            {
                if (object->getObjectCacheID())                                // slot 11
                    game->destroyObject(object);                               // IGame +0xA0
                else
                    object->forceDestroy();                                    // slot 14
            }
        }
    }

    for (std::map<unsigned int, world::IObject*>::iterator it = m_controlPointMap.begin();
         it != m_controlPointMap.end(); ++it)
        static_cast<world::ControlPoint*>(it->second)->reset();                // primary +0x180
    for (std::map<unsigned int, world::IObject*>::iterator it = m_supplyDepotMap.begin();
         it != m_supplyDepotMap.end(); ++it)
        // > 0x81BB3D0; PE 0x46693B -> 0x5736F0 (misnamed in the IDB).
        static_cast<world::SupplyDepot*>(it->second)->reset();                 // direct, no null/QI guard
    for (std::map<unsigned int, world::IObject*>::iterator it = m_objectSpawnerMap.begin();
         it != m_objectSpawnerMap.end(); ++it)
        static_cast<world::ObjectSpawner*>(it->second)->reset();

    world::objectManager->destroyObjects();                                    // +0x98

    {
        const std::map<unsigned int, world::IObject*>& objects = world::objectManager->getAllRegisteredObjects();
        for (std::map<unsigned int, world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
        {
            world::IObject* root = world::getRootParent(it->second);
            if (!world::object_detail::LayoutAccess::createdOnLoad(root) && (root->getFlags() & 0x20) == 0)
            {
                const std::string& name = root->getTemplate()->getName();      // template +0x2C
                Debug(DEBUGTYPE_7, "Game/GameLogic/GameLogic.cpp", 970)   // 4th arg = "" (the .rodata empty string IDA labels `filename`)
                    << "leaked object during reset (possible hierarchy change during destroy):" << name;
            }
        }
    }
}

// Resets round objects and players before restarting world time and tick count at zero.
// Requires empty control-object and projectile registries after teardown; otherwise
// aborts. Restores combat bounds/camera and resets the applicable vote managers.
void GameLogic::reset()
{
    eventManager->postEvent(static_cast<EventCategory>(0), 6, 0, 0.0f);         // +0x14
    dropAllKits();
    killAllPlayers();                                                          // +0x2D8
    world::objectManager->destroyObjects();                                    // +0x98
    resetAllPlayers();                                                         // +0x2FC
    resetAllObjects();                                                         // +0x300
    if (!m_playerControlObjectMap.empty() || !m_projectileMap.empty())         // +0x94 / +0xA0
        abort();
    WorldPref::resetWorldTime(0.0);
    WorldPref::resetTickCount(0);
    if (squadManager)
        static_cast<ISquadManager*>(squadManager)->reset();                    // +0x10
    setActiveCombatArea(0.0f, 0.0f, 512.0f, 512.0f);                           // vslot +0x130
    if (world::playerManager)
        resetPlayerFreeCameraPosition(world::playerManager->getLocalHumanPlayer());   // +0x30 / vslot +0x214
#if !DEDICATED
    // [PLATFORM] Client round reset also clears the active vote.
    if (!isDedicated())
        ClientVoteManager::getInstance()->resetActiveVote();
#endif
    if (isServer())                                                            // vslot +0x13C
    {
        ServerVoteManager::getInstance()->reset();
        if (world::triggerableManager)
            world::triggerableManager->refresh();                              // ELF81C4BF0
    }

#if !DEDICATED
    // [PLATFORM] Absolute BF2.exe reset tail, 0x4EEAEC-0x4EEB23. Retail clears the client loading
    // flag after the server-only vote/trigger refresh arm; the SwiffHost result is not null-tested.
    if (!isDedicated())
    {
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(
            classManager->getSingleton(std::string("SwiffHost")));
        host->setFlag7B8(false);                            // +0xD0
    }
#endif
}


namespace {

// ELF GameLogic::handleEvent 0x80D5803..0x80D5890 / PE 0x467DD8..0x467E47. Retail does not
// null-check either the looked-up template or the created controller before init/setVehicle.
void dropVehicleAttach(world::IObjectTemplate* dropVehicleTemplate, world::IObject* object)
{
    world::DropVehicle* const controller =
        static_cast<world::DropVehicle*>(dropVehicleTemplate->createObject());
    controller->init();
    controller->setVehicle(object);
}

} // namespace

// Registers the same adjusted listener for core, game, and game-logic events at
// priority zero. The base handler intentionally ignores ECGame.
GameLogic::GameLogic()
{
    // Initialize level-load and server state before publishing the game logic to listeners.
    m_isLevelLoaded   = false;                                  //   byte [this+0x38]
    m_isLevelLoadedXP = false;                                  //   byte [this+0x39]
    m_isServer        = false;                                  //   byte [this+0x3A]
    m_voiceMessages   = 0;                                      //   [this+0x108] = edx,
                                                                //   cleared at 0x80C89D6
    m_noVehiclesMode  = false;                                  //   byte [this+0x1E8]
    m_teamLanguage[0] = "English";                             //   assign("English", 7) on +0xDC
    m_teamLanguage[1] = "English";                             //   ... and on +0xE0 - both teams'
                                                                //   radio languages default to English
    m_teamFlag[0]     = "flag_neutral";                         // > +0xE4 (var_160 = lea [edi+0E4h])
    m_teamFlag[1]     = "flag_ch";                              // > +0xE8
    m_teamFlag[2]     = "flag_";                                // > +0xEC - dice's own half-default;
                                                                //   every level's Init.con overwrites all three
    // +0xF0 / +0xF4 (m_teamDropVehicle) are assigned the shared empty string ds:0x8AAC318 at
    //  / 0x80C8CD1 - observationally the default-constructed value, not repeated here.
    m_numDropVehiclesForTeam[0] = 2;                            //   +0xF8
    m_numDropVehiclesForTeam[1] = 2;                            //   +0xFC
    m_dropVehicleDeviationRadius    = 6;                        //   +0x100 (ebx = 6, 0x80C89CF)
    m_dropVehicleDeviationMinRadius = 0;                        //   +0x104 (ecx cleared, 0x80C89D4)
    m_defaultLevel    = "gulf_of_oman";                         // > +0x1E4 (esi = lea [edi+1E4h],
    m_isPrecaching    = false;                                  //   byte [this+0x14C]
    m_enableVoHelp    = false;                                  // +0x14D / +0x1FD
    m_feslEnvironment = 0;
#if defined(_WIN32)
    m_airMouseInverted = false;
    m_airMouseSensitivity = 1.0f;
#else
    m_airMouseSensitivity = 1.0f;                               //  (esi = 0x3F800000)
    m_airMouseInverted = false;
#endif
    m_byte1B0         = false;                                  //   byte [this+0x1B0]
    m_uavActiveTime   = 30.0f;                                  //   [this+0x1B4]
    m_uavReloadTime   = 5.0f;                                   //   [this+0x1B8]
    m_uavRadius       = 100.0f;                                 //   [this+0x1BC]
    m_extraUAVVehicleFlightHeight = 0.0f;                       //   [this+0x1C0] = edx (zero)
    m_satelliteScanActiveTime  = 11.0f;                         //   [this+0x1C4]
    m_satelliteScanReloadTime  = 16.0f;                         //   [this+0x1C8]
    m_satelliteScanVisibleTime = 7.0f;                          //   [this+0x1CC]

    // High ECCore priority runs GameLogicInit.con before lower-priority preload listeners.
    // ECGame is registered even though handleEvent currently ignores that category.
    eventManager->registerEventHandler(ECCore, this, 10000);    //   priority 10000
    eventManager->registerEventHandler(ECGame, this, 0);
    eventManager->registerEventHandler(ECGameLogic, this, 0);

    // Clients require the renderer's DecalManager singleton before game-logic construction.
    // Its returned decal client is owned through the renderer's decal-object interface.
    if (!isDedicated())
    {
        geom::IDecalManager* const decalManager = static_cast<geom::IDecalManager*>(
            classManager->getSingleton(std::string("DecalManager")));
        m_groundDecalObject = decalManager->createClient(0);
    }

    // Constructors publish these managers. CombatAreaManager registration is first-wins;
    // this caller deliberately discards allocations without reclaiming duplicates.
    new world::AmbientEffectAreaManager();                       // ELF80C8D83 / PE46C85B
    new world::TriggerableManager();                             // ELF80C8D97 / PE46C870
    new world::TriggerManager();                                 // ELF80C8DAB / PE46C885
    new CombatAreaManager();
}

// Unregisters game, core and game-logic handlers before releasing owned managers.
// Requires a live heightmap cluster. Lazy vote managers may be created solely to release them.
// The ground decal is released through its renderer interface; TriggerableManager is not deleted.
GameLogic::~GameLogic()
{
    eventManager->unregisterEventHandler(ECGame, this);
    eventManager->unregisterEventHandler(ECCore, this);
    eventManager->unregisterEventHandler(ECGameLogic, this);

    if (m_groundDecalObject)                                            // +0xCC - see the note
        geom::decal_object::release(m_groundDecalObject);               // DecalObject slot 10 (PE +0x28 / ELF +0x2C)

    if (world::ambientEffectAreaManager)
        delete world::ambientEffectAreaManager;
    if (world::triggerManager)
        delete world::triggerManager;
    if (CombatAreaManager::getInstance())
        delete CombatAreaManager::getInstance();                           // D1 uninstalls m_instance
    if (world::artilleryManager)
        delete world::artilleryManager;
    if (world::uavVehicleManager)
        delete world::uavVehicleManager;

    m_voipServerAddress   = 0;                                          // +0x174
    m_voipServerPort      = 0;                                          // +0x178
    m_voipBFClientPort    = 0;                                          // +0x17A
    m_voipQuality         = 3;                                          // +0x180
    m_voipEnabled         = false;                                      // +0x170
    m_isVoipClientRunning = false;                                      // +0x17C

    if (world::supplyDropManager)                                       // ds:0x8AADAAC
        world::supplyDropManager->reset();

    ClientVoteManager::getInstance();
    ClientVoteManager::release();
    if (isServer())
    {
        ServerVoteManager::getInstance();
        ServerVoteManager::release();
        VoipServerConnection::ReleaseInstance();
    }

    static_cast<world::IHeightmapCluster*>(world::heightmapCluster)->reset();   // vt +0x20, unguarded

    if (m_voiceMessages)                                                // +0x108
        delete m_voiceMessages;
}

// Handles core loading and game-logic notifications; other categories are ignored.
// [PLATFORM] Client UAV events add/remove team nametags and reset the local team's reload display.
// Dedicated images retain the lookups but omit the UI operations.
void GameLogic::handleEvent(EventCategory category, unsigned int id, EventNode* /*node*/, void* data)
{
    if (category == ECGameLogic)                                       // 0
    {
        switch (id)
        {
        case 19:
            {
                // The id is read through a pointer to an unsigned short, not passed by value.
                world::IObject* const object =
                    io::networkManager->getObject(*static_cast<unsigned short*>(data));
#if !DEDICATED
                if (object && SingletonServer::m_iNametagManager)
                    SingletonServer::m_iNametagManager->addTouchItem(
                        object, static_cast<int*>(data)[1]);            // +0x1C, the team
#else
                (void)object;
#endif
            }
            break;

        case 20:
            {
                world::IObject* const object =
                    io::networkManager->getObject(*static_cast<unsigned short*>(data));
#if !DEDICATED
                if (object && SingletonServer::m_iNametagManager)
                    SingletonServer::m_iNametagManager->removeItem(2, object);   // +0x50
#else
                (void)object;
#endif
                world::IPlayer* const local =
                    world::playerManager->getLocalHumanPlayer();        // +0x30
#if !DEDICATED
                if (local && static_cast<int*>(data)[1] == local->getTeam())   // +0xE4
                    hudManager->getMap()->resetUAVReload();             // +0x250, 772E90 - unguarded
#else
                (void)local;
#endif
            }
            break;

        case 22:
            {
                world::IObject* object =
                    io::networkManager->getObject(*static_cast<unsigned short*>(data));
                if (object)
                {
                    world::IObjectTemplate* tmpl = world::objectTemplateManager->getTemplate(
                        std::string("dropVehicle"));                    // +0x5C by name
                    dropVehicleAttach(tmpl, object);
                }
            }
            break;

        case 23:
            {
                // A supply drop has landed: destroy its parachute children BY template name.
                world::IObject* object =
                    io::networkManager->getObject(*static_cast<unsigned short*>(data));
                if (object)
                {
                    std::vector<world::IObject*> childs;
                    world::getChilds(object, childs, 0, 0);
                    for (unsigned int i = 0; i < childs.size(); ++i)
                    {
                        world::IObject* child = childs[i];
                        if (!child)
                            continue;
                        if (child->getTemplate()->getName() == "supply_parachute")
                            world::objectManager->destroyObject(child);
                    }
                }
            }
            break;

        default:
            break;
        }
    }
    else if (category == ECCore)                                       // 5
    {
        switch (id)
        {
        case 2:     // CEPreLoad - Core/Module.cpp's enum
            {
                // runFullAccess, not run: GameLogicInit.con is one of the scripts allowed outside
                // the console's sandbox, the same exemption /ServerArchives.con gets.
                const std::string empty;
                std::string out;
                io::mainConsole->runFullAccess(std::string("GameLogicInit.con"),
                                               empty, empty, empty, empty,
                                               empty, empty, empty, empty, out);
            }
            break;

        case 3:     // CEPostLoad
            // Team-name lookups are pure; C++ determines their argument evaluation order.
            if (SingletonServer::m_iNametagManager)
                SingletonServer::m_iNametagManager->init(getTeamName(1), getTeamName(2));  // +0x10
            createPreStoredObjects();                                   // +0x38
            break;

        case 4:     // CEUpdateLoadStats - the payload IS the next load part
            // The event carries a `void*`; the callee's parameter type is attested as
            // dice::hfe::NextLoadPart* by its mangling, so the cast is the ELF's implicit one.
            if (data)
                loadStat->setNextLoadPart(static_cast<NextLoadPart*>(data));
            break;

        case 7:     // CEResourcesLoaded
            endLoadStats();
#if !DEDICATED
            // [PLATFORM] BF2.exe 0x4F12FD-0x4F1340. This is the normal load-complete transition:
            // the movie has already reached its "validating client data" frame from progress, and
            // clearing the flag lets it advance to the interactive join controls. No host guard.
            if (!isDedicated())
            {
                ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(
                    classManager->getSingleton(std::string("SwiffHost")));
                host->setFlag7B8(false);                    // +0xD0
            }
#endif
            break;

        case 14:    // CEAbortLoadLevel
            // THE writer OF m_byte1B0, found AT last. GameLogic::load polls that flag eleven times
            // between phases and this is what raises it - the header called its writer [UNRESOLVED]
            // and said "nothing recovered writes it yet". Now something does.
            m_byte1B0 = true;
            break;

        default:
            break;
        }
    }
}

// Maintains both directions of the case-insensitive team-name mapping.
// An invalid team index returns a static empty string; name lookup requires
// an existing entry and does not guard a failed find.
void GameLogic::setTeamName(int team, const std::string& name)
{
    m_teamNameMap[name] = team;
    m_teamNames[team] = name;
}

const std::string& GameLogic::getTeamName(int team)
{
    std::map<int, std::string>::iterator it = m_teamNames.find(team);
    if (it != m_teamNames.end())
        return it->second;

    static const std::string s_noTeamName;
    return s_noTeamName;
}

int GameLogic::getTeamFromName(const std::string& name)
{
    return m_teamNameMap.find(name)->second;
}

// endLoadStats - 0x80D2FF0, 70 B. Two statements: drop the throttled load callback, then tell the
// HUD the load is over (ECHud/8).
void GameLogic::endLoadStats()
{
    loadStat->unregisterLoadCallback();
    eventManager->postEvent(ECHud, 8, 0, 0.0f);                         // +0x14
}

// Warms the object cache by creating and destroying each requested template count.
// Cached objects retain network descriptors and IDs; regression brackets record the occupied range.
// GSUseObjectCache disables warming, but the request map is always cleared afterward.
void GameLogic::createPreStoredObjects()
{
    if (settings->boolGet(std::string("GSUseObjectCache"), true))       // +0x50
    {
        m_startPreStoredNetID = io::networkManager->regressNetworkableIds();

        for (std::map<unsigned int, unsigned int>::iterator it = m_preStoredObjects.begin();
             it != m_preStoredObjects.end(); ++it)
        {
            world::IObjectTemplate* tmpl =
                world::objectTemplateManager->getTemplate(it->first);    // +0x60, BY ID
            for (unsigned int n = 0; n < it->second; ++n)
                game->createAndDestroyObject(tmpl);                      // IGame +0x80
        }

        world::objectManager->destroyObjects();                          // +0x98
        m_endPreStoredNetID = io::networkManager->regressNetworkableIds();
    }

    // outside the gate: the map is cleared whether or not the cache was warmed.
    if (!m_preStoredObjects.empty())
        m_preStoredObjects.clear();
}

//  / 0x80C6790, 14 B each - IGameLogic +0x3C / +0x40, the warm-up bracket
// createPreStoredObjects records. clientSendDatabase calls both at the top of its create walk and
// discards the results (the ELF's own dead reads). they were declaration-only,
// which the netid probe's bracket print flushed out.
unsigned int GameLogic::getStartPreStoredNetID() const
{
    return m_startPreStoredNetID;                                       // +0xD0
}

unsigned int GameLogic::getEndPreStoredNetID() const
{
    return m_endPreStoredNetID;                                         // +0xD4
}

// Changes local-human squad leadership. Promotion replaces only an existing AI
// leader; relinquishing chooses a replacement excluding the local player.
// Requires the manager singletons and preserves team/squad/ID query order.
void GameLogic::becomeSquadLeader(bool become)
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();

    if (become)
    {
        if (player && !player->getIsSquadLeader())
        {
            const int team = player->getTeam();
            const int squad = player->getSquadId();
            world::IPlayer* const leader = world::playerManager->getPlayerFromId(
                static_cast<ISquadManager*>(squadManager)->getSquadLeader(team, squad));
            if (leader && leader->getIsAIPlayer())
            {
                static_cast<ISquadManager*>(squadManager)->setSquadLeader(
                    team, squad, player->getId(), false);
            }
        }
    }
    else if (player && player->getIsSquadLeader())
    {
        const int team = player->getTeam();
        const int squad = player->getSquadId();
        const int playerId = player->getId();
        static_cast<ISquadManager*>(squadManager)->findNewSquadLeader(
            team, squad, playerId);
    }
}


namespace {
// Typed weapon-interface helpers used by weapon selection.
inline bool weaponObjectIsSelectable(IBase* w)    { return static_cast<world::IWeaponObject*>(w)->isSelectable(); }
inline bool weaponObjectIsUnselectable(IBase* w)  { return static_cast<world::IWeaponObject*>(w)->isUnselectable(); }
inline bool weaponObjectGetWorksInWater(IBase* w) { return static_cast<world::IWeaponObject*>(w)->getWorksInWater(); }
inline int  weaponObjectGetItemIndex(IBase* w)    { return static_cast<world::IWeaponObject*>(w)->getItemIndex(); }

// world::IItemObject - the other face of a weapon, and the one the sort and the index match use.
inline int itemObjectGetItemIndex(IBase* i)       { return static_cast<world::IItemObject*>(i)->getItemIndex(); }

// Concrete Soldier calls follow a template-class check. Typed calls let each
// compiler apply its own multiple-inheritance adjustment.
}

// Tests sprint input using the current control mode. Input 3 must be positive
// and mapped; foot controls also use inputs 12/13, while vehicle controls use 13.
bool GameLogic::getIsSprintRequested(io::PlayerInput input, world::IObject* vehicle)
{
    world::IPlayerControlObjectTemplate* pcoTemplate = 0;
    if (vehicle)
        pcoTemplate = static_cast<world::IPlayerControlObjectTemplate*>(
            vehicle->getTemplate()->queryInterface(world::IID_IPlayerControlObjectTemplate));

    const bool inControlObject = pcoTemplate != 0 &&
                                 !pcoTemplate->getVehicleCategory();

    // THE axis IS read AS A value, NOT AS A button - no 0.5f threshold - and an unmapped
    // PIThrottle reads as 0.0f, so a control map with nothing bound to it can never sprint.
    const float throttle = (input.mappingFlags & (1ULL << io::PIThrottle))
                               ? input.inputs[io::PIThrottle] : 0.0f;
    if (throttle <= 0.0f)
        return false;

    // The press test is written out here rather than through the file-local helper: this body
    // sits above the anonymous namespace that declares it, and dice inlined it at both sites anyway.
    if ((input.mappingFlags & (1ULL << io::PISprint)) && input.inputs[io::PISprint] > 0.5f)
        return true;

    // PIAltSprint IS refused inside A control object. On foot either binding sprints; in a
    // vehicle only PISprint does.
    if (inControlObject)
        return false;

    return (input.mappingFlags & (1ULL << io::PIAltSprint)) != 0 &&
           input.inputs[io::PIAltSprint] > 0.5f;
}

// Tests firing buttons independently: one input sample can send several fire messages.
void sendFireMessages(unsigned int fireMask,
                      world::IPlayer* player,
                      world::IObject* vehicle,
                      bool flag)
{
    if (fireMask & 1u)
    {
        player->setCurrentFiringButton(io::PIFire);
        vehicle->handleMessage(static_cast<world::TemplateMessage>(6), player, flag);
    }
    if (fireMask & 2u)
    {
        player->setCurrentFiringButton(io::PIAltFire);
        vehicle->handleMessage(static_cast<world::TemplateMessage>(6), player, flag);
    }
    if (fireMask & 4u)
    {
        player->setCurrentFiringButton(io::PIFlareFire);
        vehicle->handleMessage(static_cast<world::TemplateMessage>(6), player, flag);
    }
}


namespace {

// The same three idioms ServerGameLogic.cpp carries, because selectWeapon inlines them too. Kept
// file-local in both places rather than shared: dice emitted a fresh copy at every site, and a
// shared header would be a claim about their source that nothing supports.
inline unsigned long long inputMask(const io::PlayerInput& input)
{
    return input.mappingFlags;
}

// [TEMP] [CHOSEN, not recovered] fire-mode toggle tracer, armed by BF2_TRACE_FIRERATE=1.
// Remove with the fire-mode investigation. [RETIRE-WHEN: firerate-toggle-diagnosed]
inline bool fireRateTraceArmed()
{
    static int armed = -1;
    if (armed < 0)
        armed = std::getenv("BF2_TRACE_FIRERATE") ? 1 : 0;
    return armed != 0;
}

inline bool inputPressed(const io::PlayerInput& input, io::PlayerInputMap index)
{
    if (!(inputMask(input) & (1ULL << index)))
        return false;
    return input.inputs[index] > 0.5f;
}

world::IPlayerControlObject* controlObjectAtOrAbove(world::IObject* object)
{
    for (world::IObject* node = object; node; node = node->getParent())        // +0x28
    {
        world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
            node->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            return pco;
    }
    return 0;
}

world::IObject* effectiveControlObject(world::IPlayer* player)
{
    world::IObject* object = player->getVehicle();                             // +0x48
    if (object->getTemplate()->getClassID() != world::CID_SoldierTemplate)
    {
        world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        if (pco && pco->isChildSoldierActive())                                // +0x140
            object = pco->getOccupingObject();                                 // +0x14
    }
    return object;
}

// True while the player holds a weapon that CAN be selected - and every weapon-changing path in
// this file is refused while it is true. Same polarity trap as in checkPlayerTriggers.
bool playerHoldsSelectableWeapon(world::IObject* object)
{
    if (!object)
        return false;
    world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
        object->queryInterface(world::IID_IPlayerControlObject));
    world::IObject* weapon = pco->getActiveWeapon(0);                          // +0x70
    if (!weapon)
        return false;
    IBase* weaponObject = static_cast<IBase*>(weapon->queryInterface(world::IID_IWeaponObject));
    if (!weaponObject)
        return false;
    return !weaponObjectIsUnselectable(weaponObject);                          // +0x150
}

// Orders weapons by item index. Requires non-null objects implementing the weapon interface.
bool itemIndexSort(world::IObject* a, world::IObject* b)
{
    IBase* ia = 0;
    if (a)
        ia = static_cast<IBase*>(a->queryInterface(world::IID_IItemObject));
    const int ka = itemObjectGetItemIndex(ia);                                 // unchecked

    IBase* ib = 0;
    if (b)
        ib = static_cast<IBase*>(b->queryInterface(world::IID_IItemObject));
    return ka < itemObjectGetItemIndex(ib);                                    // unchecked
}

} // anonymous namespace

// Selects matching item indices across all three weapon slots. Underwater selection
// requires water support except for item 1. Re-selecting the active weapon sends
// vehicle message 11 and returns false; scanning continues so later matches can win.
bool GameLogic::switchToWeaponFromIndex(world::IPlayerControlObject* pco, world::IPlayer* player,
                                        world::IObject* vehicle, bool flag, int itemIndex)
{
    bool switched = false;

    for (int slot = 0; slot <= 2; ++slot)
    {
        std::vector<world::IObject*>* weapons =
            static_cast<std::vector<world::IObject*>*>(pco->getWeapons(slot));     // +0x74

        for (std::vector<world::IObject*>::iterator it = weapons->begin();
             it != weapons->end(); ++it)
        {
            world::IObject* weapon = *it;

            IBase* itemObject = 0;
            if (weapon)
                itemObject = static_cast<IBase*>(weapon->queryInterface(world::IID_IItemObject));

            IBase* weaponObject = 0;
            if (*it)                                       // re-read, exactly as compiled
                weaponObject = static_cast<IBase*>((*it)->queryInterface(world::IID_IWeaponObject));

            if (!itemObject || itemObjectGetItemIndex(itemObject) != itemIndex)    // +0x30
                continue;
            if (!weaponObject || !weaponObjectIsSelectable(weaponObject))          // +0x14C
                continue;
            if (pco->getIsUnderwater() &&                                          // +0x110
                !weaponObjectGetWorksInWater(weaponObject) &&                      // +0x154
                itemIndex != 1)
                continue;

            if (pco->getActiveWeapon(slot) == *it)                                 // +0x70
            {
                vehicle->handleMessage(static_cast<world::TemplateMessage>(11), player, flag);
            }
            else
            {
                pco->setActiveWeapon(*it, slot);                                   // +0x78
                switched = true;
            }
        }
    }

    return switched;
}

// Applies weapon selection, underwater fallback, cycling and fire-rate input each tick.
// Gas-mask and night-vision slots toggle devices; passenger hand weapons target the soldier.
// Requires firearm templates for device probes. A selected weapon missing from its vector is fatal.
// Template access uses the typed interface so adjusted subobject layouts remain portable.
void GameLogic::selectWeapon(world::IPlayer* player, world::IObject* vehicle,
                             io::PlayerInput input, bool flag)
{
    // 1. which slot the number keys are asking for
    int itemIndex = -1;
    if      (inputPressed(input, io::PIWeaponSelect1)) itemIndex = 1;
    else if (inputPressed(input, io::PIWeaponSelect2)) itemIndex = 2;
    else if (inputPressed(input, io::PIWeaponSelect3)) itemIndex = 3;
    else if (inputPressed(input, io::PIWeaponSelect4)) itemIndex = 4;
    else if (inputPressed(input, io::PIWeaponSelect5)) itemIndex = 5;
    else if (inputPressed(input, io::PIWeaponSelect6)) itemIndex = 6;
    else if (inputPressed(input, io::PIWeaponSelect7)) itemIndex = 7;
    else if (inputPressed(input, io::PIWeaponSelect8)) itemIndex = 8;
    else if (inputPressed(input, io::PIWeaponSelect9)) itemIndex = 9;

    // 2. is that slot a device rather than a weapon?
    world::IObject* soldier = player->getDefaultVehicle();                     // +0xA4
    if (soldier && soldier->getTemplate()->getClassID() == world::CID_SoldierTemplate)
    {
        world::IObject* weapon =
            static_cast<world::Soldier*>(soldier)->getWeaponFromIndex(itemIndex);
        if (weapon)
        {
            // unchecked downcast - see the note above.
            const world::GenericFireArmTemplate* weaponTemplate =
                static_cast<const world::GenericFireArmTemplate*>(weapon->getTemplate());

            if (weaponTemplate->m_isGasMask)                                   // template +0x1EE
            {
                if (itemIndex != -1)
                {
                    bool clearedTheOther = false;
                    if (player->getNightVision())                              // +0x2E8
                    {
                        clearedTheOther = true;
                        player->setNightVision(false, true);                   // +0x2DC
                    }
                    player->setIsGasMaskOn(!player->getIsGasMaskOn(),          // +0x2C4 / +0x2C0
                                           clearedTheOther);
                }
                return;
            }

            if (weaponTemplate->m_isNightVision)                               // template +0x1EF
            {
                if (itemIndex != -1)
                {
                    bool clearedTheOther = false;
                    // asymmetric: the gas mask is only taken off on a night MAP.
                    if (player->getIsGasMaskOn() && player->isPlayerInNightMap())   // +0x2C0/+0x2FC
                    {
                        clearedTheOther = true;
                        player->setIsGasMaskOn(false, true);                   // +0x2C4
                    }
                    player->setNightVision(!player->getNightVision(),          // +0x2DC / +0x2E8
                                           clearedTheOther);
                }
                return;
            }
        }
    }

    // 3. which control object do the keys belong to?
    bool handWeaponActive = false;
    bool specialToggle    = false;
    world::IPlayerControlObject* pco = 0;

    if (vehicle)
    {
        if (vehicle->getTemplate()->getClassID() == world::CID_PlayerControlObjectTemplate)
        {
            world::IPlayerControlObject* vehiclePco = controlObjectAtOrAbove(vehicle);
            if (vehiclePco)
                specialToggle = vehiclePco->checkSpecialToggle(input);         // PCO +0x84

        }

        if (vehicle != player->getDefaultVehicle())
        {
            world::IObject* ownSoldier = player->getDefaultVehicle();
            world::IPlayerControlObject* soldierPco = controlObjectAtOrAbove(ownSoldier);
            if (soldierPco && ownSoldier &&
                ownSoldier->getTemplate()->getClassID() == world::CID_SoldierTemplate)
            {
                handWeaponActive =
                    static_cast<world::Soldier*>(ownSoldier)->hasHandWeaponActive();
                if (handWeaponActive)
                    pco = controlObjectAtOrAbove(player->getDefaultVehicle());
            }
        }
    }
    if (!handWeaponActive)
        pco = controlObjectAtOrAbove(vehicle);

    // 4. underwater: get off a weapon that does not work here
    if (pco && pco->getIsUnderwater())                                         // +0x110
    {
        world::IObject* held = pco->getActiveWeapon(0);                        // +0x70
        if (held)
        {
            IBase* heldWeapon = static_cast<IBase*>(
                held->queryInterface(world::IID_IWeaponObject));
            if (heldWeapon &&
                !weaponObjectGetWorksInWater(heldWeapon) &&                    // +0x154
                weaponObjectGetItemIndex(heldWeapon) != 1)                     // +0x158
            {
                for (int i = 1; i <= 9; ++i)
                    if (switchToWeaponFromIndex(pco, player, vehicle, flag, i))
                        break;
            }
        }
    }

    // 5. an explicit slot was asked for
    if (itemIndex != -1)
    {
        // [TEMP] fire-mode toggle tracer. [RETIRE-WHEN: firerate-toggle-diagnosed]
        if (fireRateTraceArmed())
        {
            std::fprintf(stderr,
                         "[FRTRACE] press p%d idx=%d pco=%d holdsSelectable=%d\n",
                         player->getId(), itemIndex, pco ? 1 : 0,
                         playerHoldsSelectableWeapon(effectiveControlObject(player)) ? 1 : 0);
            std::fflush(stderr);
        }

        if (!pco)
        {
            vehicle->handleMessage(static_cast<world::TemplateMessage>(11), player, flag);
            return;
        }
        if (!playerHoldsSelectableWeapon(effectiveControlObject(player)))
            switchToWeaponFromIndex(pco, player, vehicle, flag, itemIndex);
        return;
    }

    // 6. next weapon (input 45, or the control object's own toggle)
    if (inputPressed(input, io::PIToggleWeapon) || specialToggle)
    {
        pco = handWeaponActive ? controlObjectAtOrAbove(player->getDefaultVehicle())
                               : controlObjectAtOrAbove(vehicle);
        if (!pco)
            return;
        if (playerHoldsSelectableWeapon(effectiveControlObject(player)))
            return;

        for (int slot = 0; slot <= 2; ++slot)
        {
            world::IObject* current = pco->getActiveWeapon(slot);              // +0x70
            if (!current)
                continue;

            // A copy, sorted IN place. The control object's own vector is left alone; the cycle
            // order is item index, not storage order.
            std::vector<world::IObject*>* weapons =
                static_cast<std::vector<world::IObject*>*>(pco->getWeapons(slot));   // +0x74
            std::vector<world::IObject*> ordered(*weapons);
            std::sort(ordered.begin(), ordered.end(), itemIndexSort);

            std::vector<world::IObject*>::iterator at =
                std::find(ordered.begin(), ordered.end(), current);
            if (at == ordered.end())
            {
                // The site that attests this file's line 3259. fatal.
                Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 3259, __FILE__)
                    << "Current weapon not found in weapon vector.";
                continue;
            }

            std::vector<world::IObject*>::iterator it = at + 1;
            if (it == ordered.end())
                it = ordered.begin();
            if (it == at)
                continue;                                                      // only one weapon

            bool found = false;
            for (;;)
            {
                IBase* candidate = 0;
                if (*it)
                    candidate = static_cast<IBase*>(
                        (*it)->queryInterface(world::IID_IWeaponObject));

                if (candidate &&
                    (weaponObjectIsSelectable(candidate) || specialToggle))     // +0x14C
                {
                    // Weapon cycling skips the gas mask and goggles; their dedicated keys select them.
                    const world::GenericFireArmTemplate* t =
                        static_cast<const world::GenericFireArmTemplate*>((*it)->getTemplate());
                    if (!t->m_isGasMask && !t->m_isNightVision)
                    {
                        found = true;
                        break;
                    }
                }

                ++it;
                if (it == ordered.end())
                    it = ordered.begin();
                if (it == at)
                    break;                                                     // all the way round
            }

            if (found)
                pco->setActiveWeapon(*it, slot);                               // +0x78
        }
        return;
    }

    // 7. input 46 - hand the message to whoever owns the weapon
    if (inputPressed(input, io::PIToggleFireRate))
    {
        // [TEMP] fire-mode toggle tracer. [RETIRE-WHEN: firerate-toggle-diagnosed]
        if (fireRateTraceArmed())
        {
            std::fprintf(stderr,
                         "[FRTRACE] press p%d PIToggleFireRate handWeapon=%d\n",
                         player->getId(), handWeaponActive ? 1 : 0);
            std::fflush(stderr);
        }

        if (handWeaponActive)
            player->getDefaultVehicle()->handleMessage(
                static_cast<world::TemplateMessage>(11), player, flag);
        else
            vehicle->handleMessage(static_cast<world::TemplateMessage>(11), player, flag);
    }
}


// Registers the kit and soldier template names for a team/kit pair. Re-registration
// replaces both names but leaves any previously resolved kit-template pointer cached.
void GameLogic::setKit(int team, int kit,
                       const std::string& kitTemplate,
                       const std::string& soldierTemplate)
{
    const int key = kit + (team << 8);

    std::map<int, std::string>::iterator k = m_kits.find(key);          // +0x1C
    if (k == m_kits.end())
        m_kits.insert(std::make_pair(key, kitTemplate));
    else
        k->second.assign(kitTemplate);                                  // the overwrite arm

    std::map<int, std::string>::iterator sk = m_soldierSkins.find(key); // +0x10
    if (sk == m_soldierSkins.end())
        m_soldierSkins.insert(std::make_pair(key, soldierTemplate));
    else
        sk->second.assign(soldierTemplate);
}

// Returns a cached kit template or resolves and caches its registered name.
// Missing names are not cached; a registered name resolving to null caches null.
world::IObjectTemplate* GameLogic::getKitTemplate(int team, int kit)
{
    const int key = kit + (team << 8);

    std::map<int, const world::IObjectTemplate*>::iterator c = m_kitTemplates.find(key);
    if (c != m_kitTemplates.end())
        return const_cast<world::IObjectTemplate*>(c->second);

    std::map<int, std::string>::iterator n = m_kits.find(key);
    if (n == m_kits.end())
        return 0;

    const world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(n->second);
    m_kitTemplates.insert(std::make_pair(key, t));                      // caches a null too
    return const_cast<world::IObjectTemplate*>(t);
}

// . IGameLogic +0x148. A miss returns the empty string, not an error - the caller
// cannot tell "no such kit" from "a kit registered under an empty name".
std::string GameLogic::getKitTemplateString(int team, int kit)
{
    std::map<int, std::string>::iterator it = m_kits.find(kit + (team << 8));   // +0x1C
    if (it == m_kits.end())
        return std::string();
    return it->second;
}

// . IGameLogic +0x14C. The same shape one map over.
std::string GameLogic::getSoldierSkin(int team, int kit)
{
    std::map<int, std::string>::iterator it = m_soldierSkins.find(kit + (team << 8));   // +0x10
    if (it == m_soldierSkins.end())
        return std::string();
    return it->second;
}

// `mov eax, this; add eax, 1Ch; ret`. IGameLogic +0x150.
// IT IS this function that pins m_kits TO +0x1C: it hands the map's address out untyped, with no
// copy and no const, so a caller can mutate the registry behind setKit's back.
void* GameLogic::getKits() const
{
    return const_cast<std::map<int, std::string>*>(&m_kits);
}


namespace {
// ds:0x8AAC284 and ds:0x8AAC288. NOT members - both debug counters are file-scope globals shared
// by every GameLogic instance, which is why their accessors never touch `this`. Reproduced as
// globals rather than folded into the class, because making them members would change what two
// coexisting game logics see.
int g_debugMissiles  = 0;                                   // ds:0x8AAC284
int g_debugReplenish = 0;                                   // ds:0x8AAC288
} // namespace

// Intentional base no-ops; server subclasses supply gameplay behavior.
void GameLogic::handleDrop(world::IObject*, world::IPlayer*, world::IObject*, bool) {}
void GameLogic::handlePickup(world::IObject*, world::IPlayer*, world::IObject*, bool) {}
void GameLogic::restartDemoPlayback()                       {}
void GameLogic::setHitIndicationThreshold(float)            {}
void GameLogic::suicide()                                   {}
void GameLogic::triggerSpecialGameEvent(SpecialGameEventType, void*) {}

// GameLogic::handleExplosion - ELF 0x80D8170 / PE cross-port twin, 110 B. The base/client-neutral
// virtual is the exact fifteen-argument forwarder into the common presentation half.
void GameLogic::handleExplosion(Vec3 position, float radius, int material, float damage,
                                world::IPlayer* player, world::IArmorGLComp* ignoredArmor,
                                int team, world::IObject* weapon, world::IObject* vehicle,
                                float force, world::IObject* sourceProjectile,
                                world::IObject* affectedObject, float yMod,
                                bool canTK, bool includeSource)
{
    world::GameLogicExplosion::handleExplosion(position, radius, material, damage, player,
                                               ignoredArmor, team, weapon, vehicle, force,
                                               sourceProjectile, affectedObject, yMod,
                                               canTK, includeSource);
}

// Marks a player dead, publishes death events, dismounts vehicles and updates the death camera.
// Returns false only for a null player. Vehicle exit repeats until the free camera is reached.
// Ladder/zipline parents must expose a player-control object; required camera and vehicle
// references are used without null guards. Killer credit and abandoned-soldier damage follow.
bool GameLogic::killPlayer(world::IPlayer* player, bool a4, bool a5)
{
    if (!player)
        return false;

    player->setIsAlive(false);                                                 // vt +0x68

    int payload[2];
    payload[0] = player->getId();                                              // vt +0x54
    payload[1] = a5 ? 1 : 0;   // retail stores only the byte and ships three stack-garbage pad
                               // bytes inside the 8-byte payload; zeroed here - the receiver reads
                               // only the byte, so the difference is dead wire content.

    if (isServer())                                                            // vt +0x13C
    {
        PostRemoteEvent* const event =
            new PostRemoteEvent(static_cast<EventCategory>(6), 10, false, payload, 8, 0.0f);
        game->sendGameEventToAllConnected(event);                              // IGame +0x94
    }
    eventManager->postEvent(ECGame, 8, payload, 0.0f);                         // +0x14 (the id overload)

    world::IObject* const originalVehicle = player->getVehicle();              // vt +0x48
    if (originalVehicle)
    {
        const world::IObjectTemplate* const templ =
            world::object_detail::LayoutAccess::objectTemplate(originalVehicle);   // +0x14
        if (templ->getClassID() == world::CID_LadderContainerTemplate)
        {
            world::IPlayerControlObject* pco = 0;
            for (world::IObject* walk = originalVehicle; walk;
                 walk = world::object_detail::LayoutAccess::parent(walk))      // +0x28
            {
                pco = static_cast<world::IPlayerControlObject*>(
                    walk->queryInterface(world::IID_IPlayerControlObject));
                if (pco)
                    break;
            }
            pco->exit();                             // ipco +0x20  null pco faults, as shipped
        }
        if (originalVehicle                          // re-tested though provably non-null - as compiled
            && world::object_detail::LayoutAccess::objectTemplate(originalVehicle)->getClassID()
                   == world::CID_ZiplineContainerTemplate)
        {
            world::IPlayerControlObject* pco = 0;
            for (world::IObject* walk = originalVehicle; walk;
                 walk = world::object_detail::LayoutAccess::parent(walk))
            {
                pco = static_cast<world::IPlayerControlObject*>(
                    walk->queryInterface(world::IID_IPlayerControlObject));
                if (pco)
                    break;
            }
            pco->exit();                             // same shipped null risk
        }
    }

    // the exit loop - the final fetch is reused below (retail's edi)
    world::IObject* postLoopVehicle = 0;
    for (;;)
    {
        postLoopVehicle = player->getVehicle();
        if (!postLoopVehicle || postLoopVehicle == player->getFreeCamera())    // vt +0xA8
            break;
        game->exitVehicle(player, false, a4);                                  // IGame +0xA8
    }

    player->setDefaultVehicle(0);                                              // vt +0xA0

    if (m_gameStatus != 2 && !player->getFreeCameraEnabled())                  // +0x34 / vt +0xB0
    {
        if (player->getCamera())                                               // vt +0x180
        {
            world::ICameraObject* const camera = player->getCamera();          // fetched again - as shipped
            camera->setViewMode(static_cast<world::CameraViewMode>(6), originalVehicle, false);   // +0x18
            const Mat4& transform = postLoopVehicle->getAbsoluteTransformation();   // slot 33 null risk, as shipped
            player->getVehicle()->setAbsoluteTransformation(transform);        // slot 35 same
        }
    }

    static_cast<Player*>(getPlayer(player))->m_fieldD8 = true;                 // the kill latch (+0xD8)

    const int status = m_gameStatus;
    if (status == 2 || status == 5)
        return true;

    // killer credit - the armor walk starts from the post-loop vehicle
    world::IArmorGLComp* armor = 0;
    for (world::IObject* o = postLoopVehicle; o;
         o = world::object_detail::LayoutAccess::parent(o))
    {
        armor = static_cast<world::IArmorGLComp*>(
            static_cast<world::IObjectComponent*>(
                world::object_detail::LayoutAccess::armorComponent(o)));       // +0x3C
        if (armor)
            break;
    }
    if (armor)
    {
        world::IPlayer* const hitter = armor->getLastHitPlayer();              // +0xD0
        if (hitter && hitter->getIsAlive() && hitter != player)                // vt +0x6C
        {
            player->setTracePlayerId(armor->getLastHitPlayer()->getId());      // +0xD0 again / +0x54 / +0x300
        }
    }

    world::IObject* const soldier = player->getDefaultVehicle();               // vt +0xA4
    if (!soldier)
        return true;
    if (world::object_detail::LayoutAccess::objectTemplate(soldier)->getClassID()
            == world::CID_SoldierTemplate)
    {
        if (!armor)
            return true;
        if (armor->isDestroyed())                                              // +0x54
            return true;

        Vec3 up;   up.x = 0.0f;   up.y = 1.0f;   up.z = 0.0f;
        Vec3 zero; zero.x = 0.0f; zero.y = 0.0f; zero.z = 0.0f;
        const float damage = armor->getMaxHitPoints()                          // +0x84
                           + armor->getMaxWreckHitPoints()                     // +0x88
                           + 100000.0f;                                        // flt_888F0AC
        giveDamage(soldier, damage, 0, 0, 0, -1, 0, 0, zero, up, -1, false, true, false);   // vt +0xE4

        if (postLoopVehicle != soldier)
        {
            if (postLoopVehicle->isObjectDestroyed())                          // slot 15
                armor->fullDestroy();                                          // +0x100
        }
    }
    return true;
}

// Base defaults. Keep declared return types until callers or both images establish
// a different contract; a cleared return register alone does not prove a Boolean API.
bool GameLogic::close()                                     { return false; }
bool GameLogic::giveDamage(world::IObject*, float, world::IPlayer*,
                           std::list<WeakPtr<world::IPlayer> >*,
                           world::IObject*, int, world::IObject*,
                           world::IObject*, Vec3, Vec3, int,
                           bool, bool, bool)                { return false; }              // xor eax,eax
bool GameLogic::reSpawnPlayer(world::IPlayer*)              { return false; }            // xor eax,eax
bool GameLogic::selectSpawnGroup(world::IPlayer*, int)      { return false; }            // xor eax,eax
bool GameLogic::selectTeam(world::IPlayer*, int)            { return false; }            // xor eax,eax
void GameLogic::startupVoipClient()                         {}

// Forwards all impact arguments to GameLogicCollision. Dedicated images retain
// its empty effect implementation. The final flag skips the underwater test.
void GameLogic::doImpact(int materialA, int materialB,
                         world::MaterialCollisionType collisionType,
                         const Vec3& position, const Vec3& normal, const Vec3& direction,
                         world::IObject* objectA, world::IObject* objectB,
                         world::IProjectileObject* projectile,
                         world::CollisionFaceInfo* face, Vec3 effectRotation,
                         bool ignoreUnderWater)
{
    world::GameLogicCollision::doImpact(materialA, materialB, collisionType,
                                        position, normal, direction,
                                        objectA, objectB, projectile, face,
                                        effectRotation, ignoreUnderWater);
}

// Collision effects are inactive on dedicated images. The client forwards to
// GameLogicCollision and discards its object result; the current interface remains
// void even though original bodies materialize a return value.
#if DEDICATED
void GameLogic::playCollisionEffect(Vec3, const world::IObjectTemplate*, Vec3, Vec3) {}
#else
void GameLogic::playCollisionEffect(Vec3 position, const world::IObjectTemplate* effectTemplate,
                                    Vec3 up, Vec3 normal)
{
    if (isDedicated())
        return;
    world::GameLogicCollision::playCollisionEffect(position, effectTemplate, up, normal);
}
#endif

// constant returns
// Both are `fldz` - literal 0.0f, no field read. getHitIndicationThreshold IS overridden by
// ServerGameLogic (14 B, a real field read), so the constant is the base's answer only.
float GameLogic::getHitIndicationThreshold()                { return 0.0f; }
float GameLogic::getTimeUntilRestartMap()                   { return 0.0f; }

// the two debug counters
// Straight global loads and stores; no `this` is touched by any of the four.
int  GameLogic::getDebugMissiles()                          { return g_debugMissiles; }
void GameLogic::setDebugMissiles(int value)                 { g_debugMissiles = value; }
int  GameLogic::getDebugReplenish()                         { return g_debugReplenish; }
void GameLogic::setDebugReplenish(int value)                { g_debugReplenish = value; }

// Checks a heal/repair and computes its material-scaled amount; the server override applies it.
// Rejects missing armor, inactive rounds and nonpositive amounts. Own vehicles require permission,
// except soldiers. Amount-only queries write outAmount; clients require applyIt.
// [INFERRED] The two Boolean parameter names describe these effects.
bool GameLogic::heal(world::IObject* object, float amount, world::IPlayer* player,
                     int materialA, int materialB, float* outAmount, bool applyIt,
                     bool allowOwnVehicle)
{
    typedef world::object_detail::LayoutAccess L;

    if (!isServer() && !applyIt)                                         // +0x13C
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 2864, __FILE__)
            << "Can't heal from client!";
        return false;
    }
    if (!object || m_gameStatus != 1)                                    // +0x34
        return false;

    world::IArmorGLComp* armor = 0;
    for (world::IObject* o = object; o; o = L::parent(o))                // +0x3C, then +0x28
    {
        if (L::armorComponent(o))
        {
            armor = static_cast<world::IArmorGLComp*>(L::armorComponent(o));
            break;
        }
    }
    if (!armor)
        return false;

    const world::IObjectTemplate* const templ = L::objectTemplate(object);   // +0x14
    if (!const_cast<world::IObjectTemplate*>(templ)->queryImplementation(world::CID_SimpleObjectTemplate))   // +0x50
        return false;

    const float healAmount =
        static_cast<world::MaterialManager*>(world::materialManager)
            ->getDamageMod(static_cast<unsigned int>(materialA),
                           static_cast<unsigned int>(materialB)) * amount;   // +0x58
    if (healAmount <= 0.0f)
        return false;

    if (!(allowOwnVehicle || !player
          || world::getRootParent(player->getVehicle()) != object            // +0x48
          || templ->getClassID() == world::CID_SoldierTemplate))
        return false;

    if (!applyIt)
    {
        if (outAmount)
            *outAmount = healAmount;
        return true;
    }

    world::ArmorGLCompTemplate* const armorTemplate = static_cast<world::ArmorGLCompTemplate*>(
        templ->getComponentFromClassID(world::CID_ArmorGLCompTemplate));     // +0x60
    if (!armorTemplate)
        return false;
    if (armor->getHitPoints() > 0.0f || armorTemplate->getCanBeRepairedWhenWreck())   // +0x3C / +0x100
        return armor->getMaxHitPoints() > armor->getHitPoints();             // +0x84 / +0x3C
    return false;
}

// Queries or replenishes vehicle-seat ammunition during a playing round. Clients
// may only query; the query requires ammo status at most 14. Applying clamps the
// material-adjusted amount to 100 and returns whether any weapon accepted ammo.
// outAmount is zero unless another occupant can earn friendly resupply credit.
bool GameLogic::replenishAmmo(world::IObject* object, float amount, world::IPlayer* player,
                              int materialA, int materialB, float* outAmount, bool queryOnly)
{
    if (!isServer() && !queryOnly)                                       // +0x13C
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/GameLogic.cpp", 2918, __FILE__)
            << "Can't repl from client!";
        return false;
    }
    if (!object || m_gameStatus != 1)                                    // +0x34
        return false;

    float replenish =
        static_cast<world::MaterialManager*>(world::materialManager)
            ->getDamageMod(static_cast<unsigned int>(materialA),
                           static_cast<unsigned int>(materialB)) * amount;   // +0x58
    if (replenish > 100.0f)
        replenish = 100.0f;
    if (replenish <= 0.0f)
        return false;

    world::IPlayerControlObject* pco = 0;
    for (world::IObject* o = object; o; o = world::object_detail::LayoutAccess::parent(o))
    {
        pco = static_cast<world::IPlayerControlObject*>(
            o->queryInterface(world::IID_IPlayerControlObject));           // +0x0C
        if (pco)
            break;
    }

    if (queryOnly)
    {
        if (!pco)
            return false;
        return pco->getAmmoStatus() <= 14;                                 // +0x14C
    }

    bool anyTaken = false;
    bool someoneElse = false;
    if (pco)
    {
        std::map<int, world::IPlayerControlObject*>* const pcos = pco->getPcos();   // +0x60
        for (std::map<int, world::IPlayerControlObject*>::iterator it = pcos->begin();
             it != pcos->end(); ++it)
        {
            world::IPlayerControlObject* const seat = it->second;
            if (seat->getOccupingPlayer() != player)                       // +0x10
                someoneElse = true;
            std::vector<world::IObject*>* const weapons =
                static_cast<std::vector<world::IObject*>*>(seat->getWeapons());   // +0x6C
            for (std::vector<world::IObject*>::iterator w = weapons->begin(); w != weapons->end(); ++w)
            {
                world::IWeaponObject* const weapon = static_cast<world::IWeaponObject*>(
                    (*w)->queryInterface(world::IID_IWeaponObject));
                if (!weapon)
                    continue;
                const float left = weapon->reloadAmmoPercent(replenish, queryOnly);   // +0x80
                if (!anyTaken)
                    anyTaken = replenish - left > 0.0f;
            }
        }
    }

    if (outAmount)
    {
        if (player && someoneElse && (!pco || player->getTeam() == pco->getTeam()))   // +0xE8 / +0x98
            *outAmount = replenish;
        else
            *outAmount = 0.0f;
    }
    return anyTaken;
}

// Accepts requested object flags only when neither excluded object is the root
// and root-exclusion flags are absent. Preserves three root lookups. Surface
// filtering requires a valid material and rejects see-through surfaces.
bool world::ProjectileVsTargetPredicator::predicate(world::IObject* object)
{
    typedef world::object_detail::LayoutAccess L;
    if (world::getRootParent(object) == m_excluded)
        return false;
    if (world::getRootParent(object) == m_excluded2)
        return false;
    if ((m_flags & L::flags(object)) == 0)                               // +0x04
        return false;
    world::IObject* const root = world::getRootParent(object);
    return !root || (m_rootExcludeFlags & L::flags(root)) == 0;
}

unsigned int world::ProjectileVsTargetPredicator::getFlags() const
{
    return m_flags;
}

bool world::ProjectileVsTargetPredicator::performCulling(world::IObject*)
{
    return true;
}

bool world::ProjectileVsTargetPredicator::predicateSurface(int surface)
{
    world::Material* const material = static_cast<world::MaterialManager*>(world::materialManager)
        ->getMaterialPtr(static_cast<unsigned int>(surface));            // +0x24
    return !material->getIsSeeThrough();                                 // slot 31 (+0x7C)
}

} // namespace hfe
} // namespace dice
