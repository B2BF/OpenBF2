// Server rules: round state, tickets, spawning and authoritative gameplay events.
// Path attested by __FILE__; recovery uses the symbolized dedicated ELF.
// Keep the retail event order and unchecked assumptions at each call site.

#include "ServerGameLogic.h"

#include <list>
#include <map>
#include <vector>
#include <cstring>
#include <cfloat>

#include "../IGame.h"                             // game - createObjectOnAllClients (+0x78), the event sends
#include "../IGameServer.h"                       // resetConnections (+0x4C) through game->queryInterface
#include "../../Core/EventManager.h"              // eventManager->postEvent(cat, id, void*, float)
#include "../../Core/Ids.h"                       // IID_IGameServer
#include "../../Core/System.h"                    // WorldPref::getTickCount - uPreGame's spawn-at tick
#include "../../AI/AIInterface/AIInstance.h"          // g_aiInstance - uPlaying's "have AI?" gate
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "GameLogicCollision.h"                   // handleCollision's two arms
#include "GameLogicDamage.h"                      // giveDamage's calcDamage / serverGiveDamage
#include "../Objects/IObjectTemplate.h"
#include "../Objects/ObjectLayoutAccess.h"        // giveDamage's armor walk (+0x3C)
#include "../GameEvents/HandlePickupEvent.h"      // handlePickup's broadcast
#include "../GameEvents/HandleDropEvent.h"        // handleDrop's broadcast
#include "../Objects/IItemContainerObject.h"       // handlePickupOnItem +0x24 / handleDropOnItem +0x28
#include "../../Core/StringUtil.h"                 // compareIgnoreCase - uPlaying's "gpm_coop"
#include "../GameEvents/BeginRoundEvent.h"          // uPlaying's round-start broadcast
#include "SpawnSystem/SpawnManager.h"                // spawnManager - uPlayingSpawning
#include <cmath>                                    // floor - uPlayingInsideGameArea
#include "SpawnSystem/SpawnGroup.h"                  // ISpawnGroup::getActive / getTeam / getSpawnPoint
#include "GameLogicExplosion.h"                      // the per-tick explosion drain
#include "../Python/py/PyArg.h"                      // py_arg - sgl_setParam's unpackers
#include "../IGameLogic.h"                           // gameLogic - the global sgl_setParam casts
#include "CombatAreaManager.h"                       // isPlayerInside - uPlayingInsideGameArea
#include "../../Scene/Object/PlayerManager.h"     // playerManager->getPlayers
#include "../../Scene/Object/PlayerScoreData.h"   // addBulletsGivingDamage - the projectile hit credit
#include "../Objects/Projectiles/GenericProjectile.h"  // the sole IProjectileObject implementor
#include "../Objects/IPlayer.h"                   // setSpawnAtTick (+0xF0)
#include "../Objects/ISoldier.h"                  // resurrect-projectile victim lookup
#include "../../AI/AIInterface/IAIPlayerCourier.h" // typed AI resurrect-target slot
#include "../../AI/AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../AI/AIInterface/SingletonServer.h" // AI resurrect target courier
#include "Player.h"                              // the concrete class - the direct +0x24C write
#include "../Objects/Game/ControlPoint.h"         // uFirstPreGame's cp->reset()
#include "../Objects/Game/ObjectSpawner.h"        // uFirstPreGame's ObjectSpawner::reset()
#include "../Objects/Game/SupplyDepot.h"          // uFirstPreGame's direct depot reset
#include "../GameServer/ServerVoteManager.h"      // updateKickPlayers - update's tail
#include "../GameServer/ServerSettings.h"         // uEndGame / uFirstEndGame: roundsPerMap, endDelay, endOfRoundDelay
#include "../MapList.h"                           // loadNextLevel / calcNextMapIndex - mapList
#include "../GameServer/AutoRecorder.h"           // restartMap's roundFinished hook
#include "../GameEvents/EndOfRoundEvent.h"        // endGame's broadcast
#include "../GameServer/BanList.h"                // endGame's BanList::getInstance()->endOfRound()
#include "../GameLogic/ScoreSystem/ScoreManager.h"   // reset()'s scoreManager->reset() (+0x50)
#include "../Objects/SpawnPoint.h"                   // ISpawnPoint - spawnPlayer's spawn transform
#include "../../Physics/IPhysicsNode.h"              // getTangentSpeed - a moving spawn platform
#include "../Objects/GameLogicComp/IArmorGLComp.h"   // setLastCollisionHeight
#include "../../Core/SettingsRepostitory.h"          // settings->boolGet("UseAI")
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getTemplate by name
#include "ScoreSystem/TeamScore.h"                   // m_tickets - the spawn gate
#include "../../Core/Debug.h"                     // spawnPlayer's three fatal kit sites
#include "../../Math/Mat4.h"                         // getRotationZXY - the free-camera spawn
#include "../../Geom/ParticleSystem/ParticleSystemManager.h" // typed togglePause +0x34
#include "../Common/GameEvent.h"                  // PostRemoteEvent
#include "../GameEvents/PythonCommandEvent.h"      // sgl_sendPythonEvent
#include "../GameEvents/MedalEvent.h"              // sgl_sendMedalEvent
#include "../GameEvents/RankEvent.h"               // sgl_sendRankEvent
#include "../GameEvents/UnlockEvent.h"             // setUnlocks
#include "../GameEvents/RadioMessageEvent.h"       // resurrect's AI get-up notification
#include "../Python/PythonHost.h"                 // buildArgs<int> + handleEvent(ScriptEvent 0)
#include "../Python/PyRef.h"
#include "../../Core/ClassManager.h"
#include "../../IO/Network/NetworkManager.h"      // regressNetworkableIds / getHighestUsedNetID
#include "../../IO/Input/PlayerInput.h"            // checkPlayerTriggers takes one BY value
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // the seat/weapon slots
#include "SquadManager/ISquadManager.h"           // _destroyPlayer's three typed squad calls
#include "../Objects/IObject.h"                    // handleMessage / getTemplate / getParent
#include "../Objects/IWeaponObject.h"              // active-weapon selection gate
#include "SquadManager/ISquadInfo.h"              // killPlayer's removeSquadLeaderSpawnPoint
#include "../Objects/IVehicleHudComp.h"           // replenishAmmo's per-seat useChildHud read
#include "../Objects/VehicleHudCompTemplate.h"    // VehicleHudInfo
#include "../../Scene/Object/ObjectPredicator.h"   // ObjectFlagPredicator - findKitObject
#include "../Objects/Game/KitTemplate.h"          // getNewKit's CID_KitTemplate implementation
#include "../Objects/IItemObject.h"               // pickUpClosestKit's item faces
#include "../../Scene/Object/ObjectManager.h"     // objectManager - findKitObject\'s grid query
#include "../Objects/ICameraObject.h"              // toggleViewMode's two slots
#include "../Objects/ObjectTemplate.h"             // getClassID / getName on a template
#include "../Objects/Game/Soldier.h"                // typed Soldier trigger/prone calls
#include "../GameServer/ServerInfo.h"              // serverInfo->m_block.allowNoseCam - the nose cam
#include "VoiceMessages.h"                         // resurrect's "get_up" name lookup
#include "../../Physics/MaterialManager.h"         // resurrect's material damage modifier
#include <algorithm>                               // std::find - the seat whitelist
#include "../Objects/ObjectLayoutAccess.h"         // aiObject - IObject +0x34
// toggleEntryPoint
#include "../Objects/PlayerControlObjectTemplate.h"  // m_positionalSpeedModifier +0x2A8 - exitSpeedMod
#include "../Objects/TemplateLeaves.h"               // ZiplineTemplate search radius / checkForEntry
#include "../Objects/Game/GrapplingHookRope.h"       // grapplingHookRopeMap + recovered checkForEntry
#include "../Objects/Game/Zipline.h"                 // drop retrieval / far endpoint / kit type
#include "../Objects/Game/RopeLink.h"                // drop retrieval / parent rope / kit type
#include "../Objects/Weapons/GenericFireArm.h"       // non-virtual secondary projectile lookup
#include "../Objects/Game/EntryPoint.h"              // BFfindEntryPoint - the seat search
#include "../Objects/Game/ArtilleryHandler.h"         // ArtilleryManager lifecycle
#include "../Objects/Game/CommanderToolsManager.h"    // commander tool state / lifecycle
#include "../Objects/Game/UAVVehicleManager.h"        // UAV object lifecycle
#include "../Objects/Game/SupplyDropManager.h"        // TU-static manager tick

namespace dice {
namespace hfe {
extern ClassManager* classManager;

extern IBase* squadManager;                       // ds:0x8ACBFA4 - Core/Globals.cpp
extern IBase* spawnManager;                       // ds:0x8AD3A00 - Core/Globals.cpp (dice::hfe scope, NOT world)

namespace world {
extern IBase* scoreManager;                       // ds:0x8AD1740 - Core/Globals.cpp (IBase*; ScoreManager is the class)
extern IBase* materialManager;                    // ds:0x8AD2820 - Core/Globals.cpp (MaterialManager)
// The four per-tick commander managers have recovered shared interfaces. Artillery,
// CommanderTools and UAV are constructed by ServerGameLogic below; SupplyDropManager is created by
// its own TU static initializer, matching the shipped ownership split.
} // namespace world

namespace {

// [CHOSEN, not recovered] native carriers for the existing synchronous ecai notifications.
// These are process-local event data, not wire records: the IDs remain int, while the vehicle's
// AI object is an address. x86 retains the observed two-/three-dword layouts; the native-x64
// no-bot image must never exchange these payloads with the shipped x86 AI DLL.
struct AiPlayerExitPayload
{
    int playerId;
    void* aiObject;
};
struct AiPlayerVehiclePayload
{
    int team;
    int playerId;
    void* aiObject;
};
#if !defined(_WIN64) && !defined(__LP64__)
static_assert(sizeof(AiPlayerExitPayload) == 8, "retail AI exit payload");
static_assert(sizeof(AiPlayerVehiclePayload) == 12, "retail AI vehicle payload");
#endif


} // namespace

// ServerGameLogic::init - ELF 0x80FE180 (39 B) / PE 0x454530 (25 B).
// The call order and unconditional success result are identical in both retail builds.
bool ServerGameLogic::init()
{
    io::networkManager->clear();
    GameLogic::init();
    return true;
}

//  / PE 0x454570: false, with no other work. PE explicitly clears AL.
bool ServerGameLogic::close()
{
    return false;
}

//  / PE 0x455740. Mark suicide before testing alive; only a living player
// emits ScriptEvent 1. The argument reference is released before the virtual killPlayer call.
void ServerGameLogic::suicide()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 2376, "")
            << "No local player found.";
        return;
    }
    player->setSuicide(true);
    if (player->getIsAlive())
    {
        world::IObject* object = 0;
        python::PyRef args = python::buildArgs(player, player, object, object, object);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(1), args.get());
    }
    if (player)                                     // ELF retains this second test
        killPlayer(player, true, true);
}

//  / PE 0x4558A0 (helper inlined).
void ServerGameLogic::toggleLocalFreeCamera()
{
    do_toggleFreeCamera(world::playerManager->getLocalHumanPlayer());
}

// . Only the player is guarded. Vehicle and camera must exist.
void ServerGameLogic::do_toggleFreeCamera(world::IPlayer* player)
{
    if (!player)
        return;
    world::IObject* vehicle = player->getVehicle();
    // [PLATFORM] PE obtains the free camera before the vehicle's matrix (0x4558C5/0x4558D6);
    // ELF obtains the matrix first (0x81044F0/0x81044FD). These are observable virtual calls.
#if defined(_WIN32)
    world::IObject* camera = player->getFreeCamera();
    const Mat4& transformation = vehicle->getAbsoluteTransformation();
#else
    const Mat4& transformation = vehicle->getAbsoluteTransformation();
    world::IObject* camera = player->getFreeCamera();
#endif
    camera->setAbsoluteTransformation(transformation);
    player->toggleFreeCameraEnabled();
}

//  / dedicated PE 0x454EA0 / client PE 0x4DBD10. The saved status is not initialized by the constructor:
// the first transition into pause writes it. setGameStatus is a direct call in both ports.
void ServerGameLogic::togglePause()
{
    if (m_gameStatus == 4)
        ServerGameLogic::setGameStatus(static_cast<GameStatus>(m_statusBeforePause), true, 0.0f);
    else
    {
        m_statusBeforePause = m_gameStatus;
        ServerGameLogic::setGameStatus(static_cast<GameStatus>(4), true, 0.0f);
    }
    geom::IParticleSystemManager* const particles = geom::asParticleSystemManager(
        classManager->getSingleton(std::string("ParticleSystemManager")));
    if (particles)
        particles->togglePause();
}

// /0x8104630 / dedicated PE 0x454F10/0x454F30. Test the stored status directly;
// when a change is required, togglePause is virtual. There is no virtual getPause query.
void ServerGameLogic::setPause(bool pause)
{
    if ((m_gameStatus == 4) != pause)
        togglePause();
}

bool ServerGameLogic::getPause() const
{
    return m_gameStatus == 4;
}

// /0x8104670 / PE 0x454F40/0x454F50. Independent stored flag.
void ServerGameLogic::setOutputPlayerStats(bool output)
{
    m_outputPlayerStats = output;
}

bool ServerGameLogic::getOutputPlayerStats()
{
    return m_outputPlayerStats;
}

bool ServerGameLogic::unload()
{
    if (!GameLogic::unload())
        return false;

    eventManager->postEvent(ECGameLogic, 6, 0, 0.0f);
    return true;
}

// Initializes fields, resets to pre-game state, then constructs the three manager
// singletons. Construction requires game, event, network, score, and object managers
// to be initialized. Manager constructors publish globals their destructors clear.
ServerGameLogic::ServerGameLogic()
{
    m_refCount              = 1;                     // +0x1EC
    m_roundStartTick              = -1;                    // +0x1F4
    m_tickCount             = 0;                     // +0x1F8
    m_startDelay            = 0.0f;                  // +0x1FC
    m_dword204              = 0;                     // +0x204
    m_dword20C              = 0;                     // +0x20C
    m_roundsPlayed          = 0;                     // +0x210
    m_defaultTimeToNextAIWave = 8.0f;                // +0x208
    m_roundGoing            = false;                 // +0x214
    m_ticketLossAtEndPerMin = 1000.0f;               // +0x234
    // +0x244 / +0x250: the two std::map<int,int> headers are constructed empty (the 0x18-byte
    // header-node loop, 2 iterations) - m_ticketLimits[2] does that.
    m_byte268               = true;                  // +0x268
    m_hitIndicationThreshold = 0.02f;                // +0x264
    m_nextLevel             = -1;                    // +0x27C
    m_dword274              = 0;                     // +0x274
    // +0x280..+0x288: the ticket-history vector zeroed - m_ticketHistory is empty.
    m_currentLevel          = 0;                     // +0x278
    m_byte269               = false;                 // +0x269
    m_unlockAll             = false;                 // +0x26A
    m_byte26B               = false;                 // +0x26B
    m_outputPlayerStats     = false;                 // +0x270
    m_shouldUpdateTicketSystem = false;              // +0x28C
    m_byte28D               = true;                  // +0x28D
    m_isServer              = true;                  // +0x3A  (GameLogic's byte - this is what makes isServer() true)
    // The fields reset() seeds unconditionally are left to it (the ELF's ctor does not touch them):
    m_preGame = m_firstPlaying = m_firstEndGame = false;
    m_endOfRoundDelay = 0.0f;
    m_ticketChangeRemainder[0] = m_ticketChangeRemainder[1] = 0.0f;
    m_ticketChangePerSecond[0] = m_ticketChangePerSecond[1] = 0.0f;
    m_byte228 = m_byte229 = false;
    m_ticketLossPerMin[0] = m_ticketLossPerMin[1] = 0.0f;
    m_defaultNumberOfTickets[0] = m_defaultNumberOfTickets[1] = 0;
    m_byte240 = m_byte241 = false;
    m_previousTicketCount[0] = m_previousTicketCount[1] = 0;

    reset();

    new world::ArtilleryManager();                   //  (8 B)
    new world::CommanderToolsManager();              //  (0x8C B)
    new world::UAVVehicleManager();                  //  (0x10 B)
}

// Stores status and broadcasts it remotely and locally with the supplied delay.
// [UNRESOLVED] The Boolean argument is unused.
void ServerGameLogic::setGameStatus(GameStatus status, bool, float delay)
{
    m_gameStatus = static_cast<int>(status);                                   // +0x34
    int payload = static_cast<int>(status);
    PostRemoteEvent* event = new PostRemoteEvent(ECGameLogic, 1, false, &payload, 4, delay);   // 0x54 B
    game->sendGameEventToAllConnected(event);                                  // IGame +0x94
    eventManager->postEvent(ECGameLogic, 1, &payload, delay);                  // +0x14
}

// reset - 0x80FE210, 754 B. The server half of the round reset - see the header's summary; the
// order below is the ELF's. GameLogic::reset() (the object sweep, the clock, the combat area)
// runs in the middle, after the status broadcast.
void ServerGameLogic::reset()
{
    m_defaultNumberOfTickets[0] = 50;                // +0x238
    m_ticketLossPerMin[0]       = 10.0f;             // +0x22C
    m_ticketLossPerMin[1]       = 10.0f;             // +0x230
    m_defaultNumberOfTickets[1] = 50;                // +0x23C
    m_byte240 = false;                               // +0x240
    m_byte241 = false;                               // +0x241
    m_ticketChangePerSecond[0]  = 0.0f;              // +0x220
    m_ticketChangePerSecond[1]  = 0.0f;              // +0x224
    m_ticketChangeRemainder[0] = 0.0f;               // +0x218
    m_ticketChangeRemainder[1] = 0.0f;               // +0x21C
    if (!m_ticketLimits[0].empty())                  // +0x248 (the size word) - _M_erase + re-seat the header
        m_ticketLimits[0].clear();
    if (!m_ticketLimits[1].empty())                  // +0x254
        m_ticketLimits[1].clear();
    m_previousTicketCount[0] = static_cast<int>(m_defaultNumberOfTickets[0]); // +0x25C = +0x238
    m_previousTicketCount[1] = static_cast<int>(m_defaultNumberOfTickets[1]); // +0x260 = +0x23C

    game->sendGameEventToAllPlaying(new PostRemoteEvent(ECGameLogic, 6, true, 0, 0, 0.0f));   // IGame +0x84
    setGameStatus(GameStatus_PreGame, false, 0.0f);
    GameLogic::reset();
    if (world::artilleryManager)
        world::artilleryManager->reset();
    if (world::commanderToolsManager)
        world::commanderToolsManager->reset();
    if (world::uavVehicleManager)
        world::uavVehicleManager->reset();

    {
        const int zero = 0;
        python::PyRef args = python::buildArgs(zero);                          // buildArgs<int>(0)
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(0), args.get());   // ScriptEvent 0 [UNRESOLVED name]
    }

    io::networkManager->regressNetworkableIds();
    int highestUsedNetId = io::networkManager->getHighestUsedNetID();
    game->sendGameEventToAllPlaying(new PostRemoteEvent(ECNetwork, 5, false, &highestUsedNetId, 4, 0.0f));   // +0x84

    IGameServer* gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
    if (gameServer)
        gameServer->resetConnections();                                        // IGameServer +0x4C

    m_preGame      = true;                           // +0x1F0
    m_firstPlaying = true;                           // +0x1F1
    m_firstEndGame = true;                           // +0x1F2
    m_byte229 = false;                               // +0x229
    m_byte228 = false;                               // +0x228
    // ScoreManager +0x50 - NOT null-checked (the ELF's, nor this). The global is IBase* (its
    // ELF type) and ScoreManager declares the quartet itself rather than deriving from IBase, so
    // the view is a reinterpret_cast onto the same object - vtable +0x50 either way.
    reinterpret_cast<world::ScoreManager*>(world::scoreManager)->reset();
}

// update - 0x80FE520, 216 B. THE game-state machine (the read map at the top of this file).
// SupplyDropManager::update() is called on ds:0x8AADAAC without a null check in the ELF
// (0x80FE58D..0x80FE595). Its TU static initializer publishes the singleton before game startup.
void ServerGameLogic::update(float deltaTime)
{
    if (!m_isLevelLoaded)                                                      // +0x38
        return;
    GameLogic::update(deltaTime);
    if (world::artilleryManager)
        world::artilleryManager->update(deltaTime);
    if (world::commanderToolsManager)
        world::commanderToolsManager->update(deltaTime);
    if (world::uavVehicleManager)
        world::uavVehicleManager->update(deltaTime);
    world::supplyDropManager->update();

    int status = m_gameStatus;                                                 // +0x34
    if (status == GameStatus_EndGame || status == GameStatus_Restart)          // 2 or 5
    {
        uEndGame(deltaTime);
        status = m_gameStatus;
    }
    else if (status == GameStatus_Playing)                                     // 1
    {
        uPlaying(deltaTime);                                                   // recovered 1,981-byte gameplay tick
        status = m_gameStatus;
    }
    if (status == GameStatus_PreGame)                                          // 3 - re-read after the handler
        uPreGame(deltaTime);
    ServerVoteManager::getInstance()->updateKickPlayers();                     // always
}


// Advances round announcement, player waiting, countdown and active-play states each fixed tick.
// Runs first-playing setup on entry, then always updates players, spawns, explosions and combat areas.
// Ticket updates require active play and their enable flag. Delays use 30 Hz tick conversions;
// the player threshold counts connected humans rather than living soldiers.
void ServerGameLogic::uPlaying(float deltaTime)
{
    if (m_firstPlaying)                                                              // +0x1F1
        uFirstPlaying();
    WorldPref::incTickCount(1);

    const BeginRoundState state = getBeginRoundState();                              // +0x15C
    if (state == 0)
    {
        m_roundGoing = true;                                                         // +0x214
        setBeginRoundState(static_cast<BeginRoundState>(1));                         // +0x160
        setNextStateChangeAtTick(0);                                                 // +0x168

        BeginRoundEvent* event = new BeginRoundEvent(getBeginRoundState(),
                                                     getNextStateChangeAtTick(),
                                                     serverSettings->getNoVehicles(), true);
        game->sendGameEventToAllConnected(event);                           // IGame +0x94

        // the mode string is fetched before the event is sent in the ELF's ordering of the
        // temporaries, but nothing observable depends on it; the reset is what matters.
        std::string mode;
        if (game)
            mode = game->getCurrentMapGameMode();
        if (compareIgnoreCase(mode.c_str(), "gpm_coop") == 0)
            reset();
    }
    else if (state == 1)
    {
        std::string mode;
        if (game)
            mode = game->getCurrentMapGameMode();
        const bool coop = (compareIgnoreCase(mode.c_str(), "gpm_coop") == 0);
        const bool haveAI = (ai::g_aiInstance != 0);                                     // ds:0x8AC37F8

        int needed = serverSettings->getNumPlayersNeededToStart();
        if (coop && needed > serverSettings->getMaxPlayers())
            needed = serverSettings->getMaxPlayers();

        if (world::playerManager->getNumberOfAliveHumanPlayers() >= needed || (haveAI && !coop))
        {
            reset();
            m_startDelay = haveAI ? 0.0f                                             // +0x1FC
                                  : static_cast<float>(serverSettings->m_startDelay);
            m_roundGoing = false;
            setBeginRoundState(static_cast<BeginRoundState>(2));
            setNextStateChangeAtTick(static_cast<unsigned int>(30.0f * m_startDelay)
                                     + WorldPref::getTickCount());

            BeginRoundEvent* event = new BeginRoundEvent(getBeginRoundState(),
                                                         getNextStateChangeAtTick(),
                                                         serverSettings->getNoVehicles(), true);
            game->sendGameEventToAllConnected(event);                           // IGame +0x94
        }
    }
    else if (state == 2)
    {
        m_startDelay -= deltaTime;
        if (m_startDelay < 0.0f)
        {
            m_startDelay = 0.0f;
            m_roundGoing = true;
            m_roundStartTick = WorldPref::getTickCount();                            // +0x1F4
            setBeginRoundState(static_cast<BeginRoundState>(3));

            if (serverSettings->m_timeLimit <= 0)
                setNextStateChangeAtTick(0);
            else
                setNextStateChangeAtTick(WorldPref::getTickCount()
                                         + 30 * serverSettings->m_timeLimit);

        BeginRoundEvent* event = new BeginRoundEvent(getBeginRoundState(),
                                                     getNextStateChangeAtTick(),
                                                     serverSettings->getNoVehicles(), true);
        game->sendGameEventToAllConnected(event);                           // IGame +0x94

            // (ELF 0x80FECEF..0x80FED15). Same shape as restartMap's
            // roundFinished hook, including retail asking getAutoRecorder() twice.
            {
                IGameServer* gameServer = 0;
                if (game)
                    gameServer = static_cast<IGameServer*>(
                        game->queryInterface(IID_IGameServer));                  // IBase +0x0C
                if (gameServer && gameServer->getAutoRecorder())                 // +0xB8
                    gameServer->getAutoRecorder()->roundStarted();
            }
        }
    }
    else if (state == 3)
    {
        // The not-enough-players restart timer. The AI singleton counts as "enough".
        if (ai::g_aiInstance
            || world::playerManager->getNumberOfPlayers() >= serverSettings->getNumPlayersNeededToStart())
        {
            if (m_tickCount)                                                         // +0x1F8
                m_tickCount = 0;
        }
        else
        {
            if (!m_tickCount)
                m_tickCount = WorldPref::getTickCount();                             // arm it

            const unsigned int restartTicks =
                static_cast<unsigned int>(30.0f * serverSettings->m_notEnoughPlayersRestartDelay);
            if (restartTicks
                && restartTicks + m_tickCount < static_cast<unsigned int>(WorldPref::getTickCount()))
            {
                const int arg = 0;
                python::PyRef args = python::buildArgs(arg);
                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(27), args.get());
            }
        }

        // The round time limit, measured from the tick the round went live.
        const unsigned int limitTicks = 30 * serverSettings->m_timeLimit;
        if (limitTicks)
        {
            if (m_roundStartTick != -1
                && limitTicks + m_roundStartTick < static_cast<unsigned int>(WorldPref::getTickCount()))
            {
                const int arg = 0;
                python::PyRef args = python::buildArgs(arg);
                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(27), args.get());
            }
        }
    }

    // the common tail, whatever the state did
    world::playerManager->updatePlayers(deltaTime);                                  // +0x64 - written
    uPlayingSpawning(deltaTime);
    world::GameLogicExplosion::serverUpdateExplosionDamage();
    uPlayingInsideGameArea(deltaTime);

    if (getBeginRoundState() == 3 && m_shouldUpdateTicketSystem)                     // +0x28C
        uPlayingTicketSystem(deltaTime);
}


// Checks downward ticket-threshold crossings in ascending ID order, using a
// zero-based team index. The duplicate predicate makes the negative-ID arm win;
// preserve both tests and the signed result.
int ServerGameLogic::getTicketLimitReachedId(unsigned int teamIndex, int tickets)
{
    if (teamIndex > 1)
        return 0;

    std::map<int, int>& limits = m_ticketLimits[teamIndex];
    std::map<int, int>::iterator negative = limits.begin();
    std::map<int, int>::iterator positive = negative;
    while (negative != limits.end())
    {
        const int negativeLimit = negative->second;
        if (tickets <= negativeLimit && negativeLimit < m_previousTicketCount[teamIndex])
            return -negative->first;

        const int positiveLimit = positive->second;
        if (tickets <= positiveLimit && positiveLimit < m_previousTicketCount[teamIndex])
            return positive->first;

        ++negative;
        positive = negative;
    }
    return 0;
}

// Carries fractional ticket changes across ticks and applies only negative whole
// changes, rounding with ceil and capping loss at remaining tickets. Checks limits
// even at zero rate, then saves the count for the next comparison.
void ServerGameLogic::uPlayingTicketSystem(float deltaTime)
{
    for (unsigned int teamIndex = 0; teamIndex < 2; ++teamIndex)
    {
        const int team = static_cast<int>(teamIndex + 1);
        const float rate = m_ticketChangePerSecond[teamIndex];
        if (std::fabs(rate) > 0.0f)
        {
            float pending = rate * deltaTime + m_ticketChangeRemainder[teamIndex];

            if (-pending > static_cast<float>(
                    reinterpret_cast<world::ScoreManager*>(world::scoreManager)
                        ->getTeamScore(team)->m_tickets))
            {
                pending = -static_cast<float>(
                    reinterpret_cast<world::ScoreManager*>(world::scoreManager)
                        ->getTeamScore(team)->m_tickets);
            }

            const float rounded = static_cast<float>(
                std::ceil(static_cast<double>(pending)));
            const int whole = static_cast<int>(rounded);
            if (whole < 0)
            {
                reinterpret_cast<world::ScoreManager*>(world::scoreManager)
                    ->getTeamScore(team)->subTicket(-whole);
            }

            m_ticketChangeRemainder[teamIndex] = pending - static_cast<float>(whole);
        }

        const int tickets = reinterpret_cast<world::ScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->m_tickets;
        if (tickets != m_previousTicketCount[teamIndex])
        {
            const int limitReachedId = getTicketLimitReachedId(teamIndex, tickets);
            if (limitReachedId != 0)
            {
                const int oneBasedTeam = team;
                python::PyRef args = python::buildArgs(oneBasedTeam, limitReachedId);
                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(26), args.get());
            }
        }
        m_previousTicketCount[teamIndex] = tickets;
    }
}


// Revalidates dead players' spawn-group choices every tick and clears stale choices.
// Runs due spawns every third call, after two initial skips. The cadence is a
// function-local static shared by every ServerGameLogic instance.

void ServerGameLogic::uPlayingSpawning(float)                                        // dt is unread
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();  // +0x3C

    // pass 1: is the chosen spawn group still valid?
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;
        if (player->getIsAlive() || player->getSpawnGroup() <= 0)                    // +0x6C, +0x158
            continue;

        ISpawnGroup* group = static_cast<SpawnManager*>(spawnManager)
                                 ->getSpawnGroup(player->getSpawnGroup());   // +0x28
        const bool stale = !group
                        || !group->getActive(player->getIsAIPlayer())                // +0x2C, +0x64
                        || group->getTeam() != player->getTeam()                     // +0x1C, +0xE8
                        || !group->getSpawnPoint(player->getIsAIPlayer());           // +0x44
        if (stale)
        {
            player->setSpawnGroup(0);                                                // +0x154
            // "force player to select new spawn point" - the ELF's own comment position.
            PostRemoteEvent* event = new PostRemoteEvent(static_cast<EventCategory>(6), 6u,
                                                         false, 0, 0, 0.0f);
            game->sendGameEventIfPlaying(player->getId(), event);                    // IGame +0x88
        }
    }

    // pass 2: every third tick
    static int spawnDelay = 2;                                                       // ds:0x8986844
    if (spawnDelay)
    {
        --spawnDelay;
        return;
    }

    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;

        if (player->getIsAlive() && !player->getIsManDown())                         // +0x6C, +0x100
            continue;
        if (player->getSpawnAtTick() > static_cast<unsigned int>(WorldPref::getTickCount()))   // +0xEC
            continue;

        bool ready = !player->getIsAlive();
        if (!ready)
        {
            // Man down but still alive: end it first, then only spawn if the tick is still due.
            killPlayer(player, true, false);
            ready = player->getSpawnAtTick()
                        <= static_cast<unsigned int>(WorldPref::getTickCount()) + 1;
        }
        if (ready && player->getSpawnGroup())
            spawnPlayer(player, 0);
    }
    spawnDelay = 2;
}


// Updates out-of-bounds timers and damages living players' default vehicles after
// the current vehicle's allowance expires. Damage grows with the square of elapsed
// allowances. Returning inside or dying resets the timer.
void ServerGameLogic::uPlayingInsideGameArea(float deltaTime)
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();  // +0x3C

    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;

        if (!player->getIsAlive()                                                    // +0x6C
            || CombatAreaManager::getInstance()->isPlayerInside(player))
        {
            static_cast<Player*>(player)->m_timeOutSideWorld = 0.0f;                 // Player +0x24C
            continue;
        }

        const float outside = player->getTimeOutSideWorld() + deltaTime;             // +0xE0
        const float allowed =
            CombatAreaManager::getInstance()->getTimeAllowedOutside(player->getVehicle());   // +0x48
        if (outside > allowed)
        {
            // the allowance is asked a second time here, from a second getVehicle() - as compiled.
            const float again =
                CombatAreaManager::getInstance()->getTimeAllowedOutside(player->getVehicle());
            const float mult  = std::floor(outside / again);

            Vec3 position; position.x = 0.0f; position.y = 0.0f; position.z = 0.0f;
            Vec3 direction; direction.x = 0.0f; direction.y = 1.0f; direction.z = 0.0f;

            const float damage =
                CombatAreaManager::getInstance()->getDamage(player->getVehicle()) * deltaTime
                               * mult * mult;

            giveDamage(player->getDefaultVehicle(), damage, 0, 0, 0, -1, 0, 0,       // +0xA4
                       position, direction, -1, false, true, false);                 // IGameLogic +0xE4
        }
        static_cast<Player*>(player)->m_timeOutSideWorld = outside;                  // Player +0x24C
    }
}

// uPreGame - 0x80FE6E0, 201 B / uFirstPreGame - 0x80FE600, 223 B. Pre-game is ONE tick long:
// the first entry resets every control point (-> ControlPoint::setTeam -> the spawners' teams),
// depot and spawner; then the status flips to Playing and every player's free camera is reseated
// and their spawn tick set to now + 30 * m_startDelay.
void ServerGameLogic::uPreGame(float)
{
    if (m_preGame)                                                             // +0x1F0
        uFirstPreGame();
    setGameStatus(GameStatus_Playing, false, 0.0f);
    const float spawnDelayTicks = 30.0f * m_startDelay;                        // +0x1FC
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();   // +0x3C
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;
        resetPlayerFreeCameraPosition(player);                                 // vslot +0x214
        const int tick = WorldPref::getTickCount();
        player->setSpawnAtTick(static_cast<unsigned int>(spawnDelayTicks + static_cast<float>(tick)));   // IPlayer +0xF0
    }
}

void ServerGameLogic::uFirstPreGame()
{
    m_preGame = false;                                                         // +0x1F0
    for (std::map<unsigned int, world::IObject*>::iterator it = m_controlPointMap.begin();
         it != m_controlPointMap.end(); ++it)
        static_cast<world::ControlPoint*>(it->second)->reset();                // primary +0x180
    for (std::map<unsigned int, world::IObject*>::iterator it = m_supplyDepotMap.begin();
         it != m_supplyDepotMap.end(); ++it)
        // > 0x81BB3D0; PE 0x45624B -> 0x5736F0 (misnamed in the IDB).
        static_cast<world::SupplyDepot*>(it->second)->reset();                 // direct, no null/QI guard
    for (std::map<unsigned int, world::IObject*>::iterator it = m_objectSpawnerMap.begin();
         it != m_objectSpawnerMap.end(); ++it)
        static_cast<world::ObjectSpawner*>(it->second)->reset();
}

void ServerGameLogic::uFirstPlaying()
{
    m_firstPlaying = false;                                                    // +0x1F1
}

// Starts the end-of-round countdown, adding endDelay on the last round. Expiry
// restarts the map or loads the next one, then parks the countdown at 9999.
void ServerGameLogic::uFirstEndGame()
{
    m_firstEndGame = false;                                                    // +0x1F2
    const int endOfRoundDelay = serverSettings->m_endOfRoundDelay;             // ServerSettings +0xF8
    m_endOfRoundDelay = static_cast<float>(endOfRoundDelay);                   // +0x200
    if (m_roundsPlayed == serverSettings->m_roundsPerMap - 1)                  // +0x210 vs +0x74
        m_endOfRoundDelay = static_cast<float>(serverSettings->m_endDelay + endOfRoundDelay);   // +0x64
}

void ServerGameLogic::uEndGame(float deltaTime)
{
    if (m_firstEndGame)                                                        // +0x1F2
        uFirstEndGame();
    m_shouldUpdateTicketSystem = false;                                        // +0x28C
    m_endOfRoundDelay -= deltaTime;                                            // +0x200
    if (m_endOfRoundDelay <= 0.0f)
    {
        m_endOfRoundDelay = 9999.0f;
        const int roundsPerMap = serverSettings->m_roundsPerMap;               // +0x74
        if (roundsPerMap == -1)
        {
            restartMap();
            return;
        }
        ++m_roundsPlayed;                                                      // +0x210
        if (roundsPerMap > m_roundsPlayed)
            restartMap();
        else
            loadNextLevel();
    }
}

// Restart the round without unloading the map. loadNextLevel performs a separate
// unload/reload and leaves the server unloaded if the selected entry is missing.
// runNextLevel requests end-of-round, then the normal countdown advances the map.
// calcNextMapIndex requires a nonempty map list.
void ServerGameLogic::restartMap()
{
    IGameServer* gameServer = 0;
    if (game)
        gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));   // IBase +0x0C
    if (gameServer && gameServer->getAutoRecorder())                                     // +0xB8
        gameServer->getAutoRecorder()->roundFinished();                                  // [FRONTIER] 0x80A05C0
    m_roundStartTick = static_cast<unsigned int>(-1);                                    // +0x1F4
    setBeginRoundState(static_cast<BeginRoundState>(0));                                 // vt +0x160
    reset();                                                                             // vt +0x24
}

void ServerGameLogic::loadNextLevel()
{
    if (!mapList->getMapCount())
        return;

    m_roundStartTick = static_cast<unsigned int>(-1);                                    // +0x1F4
    setBeginRoundState(static_cast<BeginRoundState>(0));                                 // vt +0x160
    game->unload();                                                                      // IGame +0x28

    m_currentLevel = calcNextMapIndex();                                                 // +0x278
    m_nextLevel    = -1;                                                                 // +0x27C
    mapList->setCurrentMap(m_currentLevel, true);
    const MapList::Entry* entry = mapList->getEntry(mapList->getCurrentMap());           // MapList +0x04
    if (entry)
    {
        settings->stringSet(std::string("GSLoadLevel"), entry->name);                    // vt +0x44
        game->loadPath(entry->name);                                                     // IGame +0x24
    }
    m_roundsPlayed = 0;                                                                  // +0x210
}

void ServerGameLogic::runNextLevel()
{
    if (getGameStatus() == 2)                                                            // vt +0x158
        return;

    m_roundsPlayed = serverSettings->m_roundsPerMap - 1;                                 // +0x210 <- +0x74

    const int arg = 0;
    python::PyRef args = python::buildArgs(arg);
    python::pythonHost()->handleEvent(static_cast<ScriptEvent>(27), args.get());

    if (getGameStatus() != 2)                                                            // re-tested
        endGame(0, 0);
}

void ServerGameLogic::setNextLevel(int index)
{
    m_nextLevel = index;                                                                 // +0x27C
}

int ServerGameLogic::calcNextMapIndex() const
{
    int next = m_currentLevel + 1;
    if (m_nextLevel >= 0)
        next = m_nextLevel;
    return next % mapList->getMapCount();                                                // no zero guard
}

// . This is intentionally the computed next map index rather than the raw
// m_nextLevel override; like calcNextMapIndex it has no empty-map-list guard in retail.
int ServerGameLogic::getNextLevel() const
{
    return calcNextMapIndex();
}

// Ends the round once, setting score results before changing game status.
// Broadcasts rounds remaining, kills players, resets free-camera positions and advances bans.
// The score manager narrows result values to bytes; the event retains full integers.
void ServerGameLogic::endGame(unsigned int winnerTeam, int victoryType)
{
    if (getGameStatus() == GameStatus_EndGame)                                           // vt +0x158
        return;

    world::ScoreManager* const scores = reinterpret_cast<world::ScoreManager*>(world::scoreManager);
    scores->setWinner(static_cast<unsigned char>(winnerTeam));                           // vt +0x14
    scores->setVictoryType(static_cast<unsigned char>(victoryType));                     // vt +0x20

    EndOfRoundEvent* event = new EndOfRoundEvent(serverSettings->m_roundsPerMap - m_roundsPlayed - 1,
                                                 TicketHistory(m_ticketHistory),       // by value - copied
                                                 static_cast<int>(winnerTeam), victoryType);
    game->sendGameEventToAllConnected(event);                                            // IGame +0x94

    setGameStatus(GameStatus_EndGame, false, 0.0f);
    killAllPlayers();                                                                    // vt +0x2D8

    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();      // +0x3C
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        resetPlayerFreeCameraPosition(*it);                                              // vt +0x214

    BanList::getInstance()->endOfRound();
}

// : `lea eax, [this+280h]`. / 0x8104910, 34 B: a tail call into
// std::vector<TicketHistory::Ticket>::operator= on +0x280 - i.e. the struct's own assignment.
TicketHistory& ServerGameLogic::getTicketHistory()
{
    return m_ticketHistory;                                                              // +0x280
}

void ServerGameLogic::setTicketHistory(const TicketHistory& history)
{
    m_ticketHistory = history;                                                           // +0x280
}

// : the server plays no effect (`return 0`).
void ServerGameLogic::playEffect(Vec3, const world::IObjectTemplate*, Vec3)
{
}

// : a TU-static std::vector<IObject*> (0x8AAC718, guarded by 0x8AAC710) is sized to
// `count` and filled with objects created at the origin - createObjectOnAllClients(template,
// (0,0,0), (0,0,0), true, 0, 0). The vector is the function's own; nothing returns it (the ELF
// returns its address in eax, an artifact of the last store).
void ServerGameLogic::spawnMultipleObjects(const world::IObjectTemplate* objectTemplate, int count)
{
    static std::vector<world::IObject*> spawned;    // ds:0x8AAC718
    spawned.resize(count > 0 ? (size_t)count : 0u);
    for (int i = 0; i < count; ++i)
    {
        Vec3 zero;
        zero.x = zero.y = zero.z = 0.0f;
        spawned[i] = game->createObjectOnAllClients(objectTemplate, zero, zero, true, 0, 0);
    }
}

// : the by-name form does nothing on the server (`return 0`).
world::IObject* ServerGameLogic::spawnObject(const std::string&, Vec3, Vec3, int)
{
    return 0;
}

// Creates a vehicle requested by an ObjectSpawner.
world::IObject* ServerGameLogic::spawnObject(const world::IObjectTemplate* objectTemplate, Vec3 position,
                                             Vec3 rotation, int team)
{
    return game->createObjectOnAllClients(objectTemplate, position, rotation, true, 0, team);   // IGame +0x78
}

// gameLogic.createObject - the ConsoleClass195 executor's only callee (ELF 0x4a8020,
// dedicated PE 0x455B60). Looks the template up by name and spawns it at the local player's
// vehicle, or free camera when enabled. The transform's third row is scaled before the
// translation is added: soldiers use 1.0, every other class 10.0. A projectile result is
// activated, and the +0x274 debug scale (0 unless a script sets it) launches the new object.
void ServerGameLogic::createObject(std::string name)
{
    world::IObjectTemplate* const objectTemplate = world::objectTemplateManager->getTemplate(name);
    if (!objectTemplate)
        return;

    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
    {
        // No local human player: only proceed when exactly one player is connected.
        if (world::playerManager->getNumberOfPlayers() != 1)
            return;
        const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
        player = players.empty() ? 0 : players.front();
    }

    if (player && !player->getVehicle())
        return;

    // The spawn transform is the identity unless a player (and its vehicle/camera) exists.
    Mat4 transform = Mat4::getIdentity();
    if (player)
    {
        world::IObject* const source = player->getFreeCameraEnabled()
            ? player->getFreeCamera()
            : player->getVehicle();
        transform = source->getAbsoluteTransformation();
    }

    float zDegrees = 0.0f, xDegrees = 0.0f, yDegrees = 0.0f;
    getRotationZXY(transform, zDegrees, xDegrees, yDegrees);

    Vec3 rotation;
    rotation.x = -yDegrees;
    rotation.y = -xDegrees;
    rotation.z = -zDegrees;

    const float scale = (objectTemplate->getClassID() == world::CID_SoldierTemplate) ? 1.0f : 10.0f;

    Vec3 position;
    position.x = scale * transform.m[8] + transform.m[12];
    position.y = scale * transform.m[9] + transform.m[13];
    position.z = scale * transform.m[10] + transform.m[14];

    world::IObject* const object = game->createObjectOnAllClients(objectTemplate, position, rotation,
                                                                  true, 0, 0);
    world::objectManager->setActiveObject(object);                                    // ObjectManager +0x94

    if (object->queryInterface(world::IID_IProjectileObject))
        static_cast<world::GenericProjectile*>(object)->activate();

    // Debug scale, 0 unless a script sets it. When positive it launches the new object by
    // scaling its transform's third row into the physics node's positional speed.
    const float spawnScale = *reinterpret_cast<const float*>(&m_dword274);
    if (spawnScale > 0.0f)
    {
        if (void* const physics = world::object_detail::LayoutAccess::physicsNode(object))
        {
            world::IPhysicsNode* const node = static_cast<world::IPhysicsNode*>(physics);
            const Mat4& objectTransform = object->getAbsoluteTransformation();
            Vec3 speed;
            speed.x = spawnScale * objectTransform.m[8];
            speed.y = spawnScale * objectTransform.m[9];
            speed.z = spawnScale * objectTransform.m[10];
            node->setPositionalSpeed(speed);

            Vec3 zero;
            zero.x = zero.y = zero.z = 0.0f;
            node->setPositionalAcceleration(zero);
            node->setRotationalSpeed(zero);
        }
    }
}

// Returns the current map-list index.
int ServerGameLogic::getCurrentLevel() const
{
    return m_currentLevel;                               // +0x278
}


namespace {

// io::PlayerInput's two read idioms, written once. dice emitted them inline at every one of the
// twenty-odd sites below: `shrd ecx, ebx, N / and eax, 1` to test bit N of the 64-bit mapping mask,
// then a compare against 0.5f. The mask gates the read - an unmapped input reads as absent, not as
// zero, and that distinction is the whole point of the mask (see IO/Input/PlayerInput.h).
inline unsigned long long inputMask(const io::PlayerInput& input)
{
    return input.mappingFlags;
}

inline bool inputPressed(const io::PlayerInput& input, io::PlayerInputMap index)
{
    if (!(inputMask(input) & (1ULL << index)))
        return false;
    return input.inputs[index] > 0.5f;
}

// Uses the active child soldier for vehicle input when the vehicle itself is not
// a soldier; otherwise uses the vehicle.
world::IObject* effectiveControlObject(world::IPlayer* player)
{
    world::IObject* object = player->getVehicle();                       // IPlayer +0x48
    if (object->getTemplate()->getClassID() != world::CID_SoldierTemplate)
    {
        world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        if (pco && pco->isChildSoldierActive())                          // PCO +0x140
            object = pco->getOccupingObject();                           // PCO +0x14
    }
    return object;
}

// Returns true when a selectable weapon is held. Vehicle entry/exit and seat
// actions are suppressed while this condition holds.
bool playerHoldsSelectableWeapon(world::IObject* object)
{
    if (!object)
        return false;

    world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
        object->queryInterface(world::IID_IPlayerControlObject));
    world::IObject* weapon = pco->getActiveWeapon(0);                    // PCO +0x70
    if (!weapon)
        return false;

    world::IWeaponObject* weaponObject = static_cast<world::IWeaponObject*>(
        weapon->queryInterface(world::IID_IWeaponObject));
    if (!weaponObject)
        return false;

    return !weaponObject->isUnselectable();                              // IWeaponObject +0x150
}

// The parent-chain walk for a control object, done twice in the seat block: from the root of the
// hierarchy and from the player's current vehicle.
world::IPlayerControlObject* controlObjectAtOrAbove(world::IObject* object)
{
    for (world::IObject* node = object; node; node = node->getParent())  // +0x28
    {
        world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
            node->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            return pco;
    }
    return 0;
}

} // anonymous namespace

namespace world {

// If cycling is allowed, steps forward or backward through modes 0..7 until
// one is accepted or the search returns to its starting mode.
void toggleViewMode(ICameraObject* camera, bool forward)
{
    if (!camera->allowCameraModeCycle())                                 // +0x7C
        return;

    const int start = static_cast<int>(camera->getViewMode());           // +0x1C
    int mode = start;
    bool accepted;
    do
    {
        const int backward = mode - 1;
        ++mode;
        if (!forward)
            mode = backward;
        if (mode >= 8)
            mode = 0;
        if (mode < 0)
            mode = 7;
        accepted = camera->setViewMode(static_cast<CameraViewMode>(mode), 0, false);   // +0x18
    }
    while (!accepted && mode != start);
}

} // namespace world

// The inline drop retrieval arm of checkPlayerTriggers. ELF 0x80FD2A9..0x80FDF8E /
// ..0x458C29, both read before recovery. [CHOSEN factoring]: no standalone
// retail symbol exists. Keep the repeated virtual reads, query append behavior, asymmetric
// projectile lookups, and per-weapon refund (only the inner magazine loop breaks).
void ServerGameLogic::checkSpecialForcesTriggers(world::IPlayer* player, world::IObject* soldier,
                                                const io::PlayerInput& input)
{
    using namespace world;
    std::vector<IObject*> candidates;
    ObjectFlagPredicator gadgetPredicate(0x200);
    std::vector<IObject*>& scratch = objectManager->getObjectTempVector();
    scratch.erase(scratch.begin(), scratch.end());
    Zipline* zipline = 0;
    RopeLink* ropeLink = 0;
    float closestSquared = FLT_MAX;
    const Vec3 soldierPosition = player->getVehicle()->getAbsolutePosition();
    objectManager->getObjectsFromGrid(player->getVehicle()->getAbsolutePosition(),
                                      4.0f, scratch, &gadgetPredicate);
    for (std::size_t i = 0; i < scratch.size(); ++i)
        candidates.push_back(scratch[i]);

    Soldier* currentSoldier = static_cast<Soldier*>(player->getVehicle());
    for (std::vector<IObject*>::iterator it = candidates.begin(); it != candidates.end(); ++it)
    {
        IObject* candidate = *it;
        if (candidate->getTemplate()->getClassID() == CID_ZiplineTemplate)
        {
            float standingHeight = 0.0f;
            if (currentSoldier)
                standingHeight = currentSoldier->getStandingHeight();
            if (std::fabs(candidate->getAbsolutePosition().y - soldierPosition.y) > standingHeight)
                continue;
            const Vec3& position = candidate->getAbsolutePosition();
            const Vec3 delta = { position.x - soldierPosition.x, position.y - soldierPosition.y,
                                 position.z - soldierPosition.z };
            const float squared = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
            if (closestSquared > squared)
            {
                closestSquared = squared;
                zipline = static_cast<Zipline*>(candidate);
            }
        }
        if ((candidate->getTemplate()->getClassID() == CID_RopeLinkTemplate ||
             candidate->getTemplate()->getClassID() == CID_HookLinkTemplate) &&
            currentSoldier->m_readyToClimbTime <= 0.0f && !inputPressed(input, io::PIAltFire))
        {
            ObjectFlagPredicator obstructionPredicate(0x100);
            IObject* hitObject = 0;
            Vec3 hitPoint = { 0.0f, 0.0f, 0.0f };
            Vec3 hitNormal = { 0.0f, 0.0f, 0.0f };
            int hitPart; // output only; neither shipped port initializes this local
            const Vec3& position = candidate->getAbsolutePosition();
            const Vec3 direction = { soldierPosition.x - position.x, soldierPosition.y - position.y,
                                     soldierPosition.z - position.z };
            if (!objectManager->intersectLine(hitObject, hitPoint, hitNormal, hitPart,
                    candidate->getAbsolutePosition(), direction, &obstructionPredicate,
                    true, false, false, false, CID_SoldierTemplate) &&
                static_cast<RopeLink*>(candidate)->m_rope->_float1C0 <= 0.0f)
                ropeLink = static_cast<RopeLink*>(candidate); // last eligible link, no distance rank
        }
    }

    if (!zipline)
    {
        currentSoldier = static_cast<Soldier*>(player->getVehicle());
        const ZiplineTemplate* ziplineTemplate = static_cast<const ZiplineTemplate*>(
            objectTemplateManager->getTemplate("Zipline"));
        if (currentSoldier && ziplineTemplate)
        {
            // Neither vector is cleared by this caller. The query replaces scratch, then its
            // contents are appended to candidates, retaining the entire first query's snapshot.
            objectManager->getObjectsFromGrid(soldierPosition, ziplineTemplate->m_searchRadius,
                                              scratch, &gadgetPredicate);
            for (std::size_t i = 0; i < scratch.size(); ++i)
                candidates.push_back(scratch[i]);

            float closest = FLT_MAX;
            for (std::vector<IObject*>::iterator it = candidates.begin(); it != candidates.end(); ++it)
            {
                if ((*it)->getTemplate()->getClassID() != CID_ZiplineTemplate)
                    continue;
                Zipline* candidate = static_cast<Zipline*>(*it);
                const Vec3 start = candidate->getAbsolutePosition();
                const Vec3 end = candidate->m_endPoint;
                const Vec3 segment = { end.x - start.x, end.y - start.y, end.z - start.z };
                const float lengthSquared = segment.x * segment.x + segment.y * segment.y +
                                            segment.z * segment.z;
                const Vec3 position = currentSoldier->getAbsolutePosition();
                const Vec3 toEnd = { end.x - position.x, end.y - position.y, end.z - position.z };
                const float endDistance = std::sqrt(toEnd.x * toEnd.x + toEnd.y * toEnd.y +
                                                    toEnd.z * toEnd.z);
                if (endDistance < 3.0f && closest > endDistance)
                {
                    closest = endDistance;
                    zipline = candidate;
                }
                const float fraction = ((position.x - start.x) * segment.x +
                                        (position.y - start.y) * segment.y +
                                        (position.z - start.z) * segment.z) / lengthSquared;
                // Retail has no zero-length guard. Endpoint distance is strict; segment distance
                // is inclusive. Ordered comparisons also leave a NaN fraction unselected.
                if (fraction >= 0.0f && fraction <= 1.0f)
                {
                    const Vec3 projected = { start.x + segment.x * fraction,
                                             start.y + segment.y * fraction,
                                             start.z + segment.z * fraction };
                    const Vec3 delta = { position.x - projected.x, position.y - projected.y,
                                         position.z - projected.z };
                    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y +
                                                     delta.z * delta.z);
                    if (distance <= 3.0f && closest > distance)
                    {
                        closest = distance;
                        zipline = candidate;
                    }
                }
            }
        }
    }

    if (zipline)
    {
        currentSoldier = static_cast<Soldier*>(player->getVehicle());
        if (currentSoldier->getKitType() == zipline->_dword180 &&
            !player->getIsManDown() && player->getIsAlive())
        {
            const std::string name = zipline->getTemplate()->getName();
            std::vector<IObject*>& weapons =
                *static_cast<std::vector<IObject*>*>(currentSoldier->getWeapons());
            for (std::vector<IObject*>::iterator it = weapons.begin(); it != weapons.end(); ++it)
            {
                IWeaponObject* weapon = *it ? static_cast<IWeaponObject*>(
                    (*it)->queryInterface(IID_IWeaponObject)) : 0;
                if (!weapon)
                    continue;
                IObjectTemplate* projectile =
                    static_cast<GenericFireArm*>(weapon)->getSecondaryProjectileTemplate();
                if (projectile && projectile->getName() == name)
                {
                    for (int magazine = 0; magazine < weapon->getCurrentNrOfMags(); ++magazine)
                    {
                        const int ammo = weapon->getAmmoInMag(magazine);
                        if (ammo < weapon->getMagSize())
                        {
                            weapon->setAmmoInMag(magazine, ammo + 1);
                            zipline->destroyZipline();
                            break;
                        }
                    }
                }
            }
        }
    }
    if (ropeLink)
    {
        currentSoldier = static_cast<Soldier*>(player->getVehicle());
        if (currentSoldier->getKitType() == ropeLink->_dword14C &&
            !player->getIsManDown() && player->getIsAlive())
        {
            const std::string name = ropeLink->m_rope->getTemplate()->getName();
            std::vector<IObject*>& weapons =
                *static_cast<std::vector<IObject*>*>(currentSoldier->getWeapons());
            for (std::vector<IObject*>::iterator it = weapons.begin(); it != weapons.end(); ++it)
            {
                IWeaponObject* weapon = *it ? static_cast<IWeaponObject*>(
                    (*it)->queryInterface(IID_IWeaponObject)) : 0;
                if (!weapon)
                    continue;
                IObjectTemplate* projectile = weapon->getProjectileTemplate();
                if (projectile && projectile->getName() == name)
                {
                    for (int magazine = 0; magazine < weapon->getCurrentNrOfMags(); ++magazine)
                    {
                        const int ammo = weapon->getAmmoInMag(magazine);
                        if (ammo < weapon->getMagSize())
                        {
                            weapon->setAmmoInMag(magazine, ammo + 1);
                            ropeLink->m_rope->destroyGrapplingHook();
                            break;
                        }
                    }
                }
            }
        }
    }
    if (!zipline && !ropeLink)
        soldier->handleMessage(static_cast<TemplateMessage>(26), player, false);
}

// Processes a player's per-tick fire, vehicle, seat, device and camera input.
// Dead players proceed only while using their free camera. The drop-key path requires a vehicle.
// Entering or leaving rereads the current vehicle for later actions; repeated queries are intentional.
void ServerGameLogic::checkPlayerTriggers(world::IPlayer* player, world::IObject* vehicle,
                                          io::PlayerInput input, bool flag)
{
    // 1. the two guards
    if (!player->getIsAlive() &&                                         // +0x6C
        player->getVehicle() != player->getFreeCamera())                 // +0x48 / +0xA8
        return;
    if (player->getOverheadCameraEnabled())                              // +0xBC
        return;

    // 2. PIUse - the enter / exit key
    if (inputPressed(input, io::PIUse))
    {
        if (!playerHoldsSelectableWeapon(effectiveControlObject(player)))
        {
            toggleEntryPoint(player, true);
            vehicle = player->getVehicle();                              // the parameter changes
        }
    }

    // Sends sprint message 42 on entry and 41 while continuing; stopping sends nothing.
    const bool wasSprinting = player->getSprintState();                  // +0x1BC
    const bool wantsSprint  = GameLogic::getIsSprintRequested(input, vehicle);

    player->setSprintStateLastTick(player->getSprintState());            // +0x1C0 <- +0x1BC (again)
    player->setSprintState(wantsSprint);                                 // +0x1B8
    if (vehicle && player->getSprintState())                             // +0x1BC (a third read)
        vehicle->handleMessage(static_cast<world::TemplateMessage>(wasSprinting ? 41 : 42),
                               player, false);                           // slot 65

    // 4. the three fire buttons
    unsigned int fireMask = 0;
    if (inputPressed(input,  io::PIFire)) fireMask |= 1u;
    if (inputPressed(input, io::PIAltFire)) fireMask |= 2u;
    if (inputPressed(input, io::PIFlareFire)) fireMask |= 4u;

    bool firing;
    if (vehicle)
    {
        if (fireMask)
        {
            player->updateFiringState(fireMask, 0);                      // +0x1D0
            sendFireMessages(fireMask, player, vehicle, false);
        }
        // A soldier that cannot fire reports NOT firing, whatever the button says - and only a
        // soldier: any other vehicle falls through to the raw button.
        if (vehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate &&
            !static_cast<world::Soldier*>(vehicle)->getCanFireWeapon())
            firing = false;
        else
            firing = inputPressed(input, io::PIFire);
    }
    else
    {
        firing = inputPressed(input, io::PIFire);
    }

    player->setFiringState(firing);                                      // +0x1A0
    player->setAltFiringState(inputPressed(input, io::PIAltFire));                  // +0x1A8
    player->setFlareFiringState(inputPressed(input, io::PIFlareFire));                // +0x1B0

    // 5. PIDrop - the kit swap and the Special Forces rope gear
    if (!inputPressed(input, io::PIDrop))
    {
        // A write-only field. m_byte28D is set here and by the constructor, and read nowhere IN
        // THE binary - a full scan of every function finds exactly three stores and no loads. It
        // reads as a released-key latch that nothing ever consumed. Reproduced, and named for what
        // it is rather than for what it looks like it was for.
        m_byte28D = true;                                                // +0x28D
    }
    else if (vehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate)
    {
        // KIT pickup IS ON A TWO-second cooldown, and the clock is the player's own kit
        // timestamp - so swapping kits twice in a row is refused by the player, not by the kit.
        if (static_cast<world::Soldier*>(vehicle)->isNotFireingOrHaveRecoil())
        {
            if (WorldPref::getWorldTime() - player->getKitTimeStamp() > 2.0)   // +0x200
            {
                if (vehicle->queryInterface(world::IID_IItemContainerObject))
                    pickUpClosestKit(player, vehicle, 3.0f);
            }
        }

        checkSpecialForcesTriggers(player, vehicle, input);
    }

    // 6/7. PIReload and PISelectFunc
    if (inputPressed(input, io::PIReload) && vehicle)
    {
        vehicle->handleMessage(static_cast<world::TemplateMessage>(10), player, false);
        player->setReloadState();                                        // +0x1C8
    }
    if (inputPressed(input, io::PISelectFunc) && player->getVehicle())
    {
        world::IObject* current = player->getVehicle();                  // asked twice
        current->handleMessage(static_cast<world::TemplateMessage>(12), player, false);
    }

    // 8. weapon selection
    // unconditional, and the only use of this function's trailing bool.
    selectWeapon(player, vehicle, input, flag);

    // 9. PIPositionSelect1..8 - the seat keys
    int seat = 0;
    if      (inputPressed(input, io::PIPositionSelect1)) seat = 1;
    else if (inputPressed(input, io::PIPositionSelect2)) seat = 2;
    else if (inputPressed(input, io::PIPositionSelect3)) seat = 3;
    else if (inputPressed(input, io::PIPositionSelect4)) seat = 4;
    else if (inputPressed(input, io::PIPositionSelect5)) seat = 5;
    else if (inputPressed(input, io::PIPositionSelect6)) seat = 6;
    else if (inputPressed(input, io::PIPositionSelect7)) seat = 7;
    else if (inputPressed(input, io::PIPositionSelect8)) seat = 8;

    if (seat != 0)
    {
        const int requestedPcoId = seat - 1;

        // Search seats from the hierarchy root, while identifying the current seat from the player's vehicle.
        world::IPlayerControlObject* rootPco =
            controlObjectAtOrAbove(world::getRootParent(vehicle));
        if (rootPco)
        {
            world::IPlayerControlObject* currentPco = controlObjectAtOrAbove(vehicle);
            const bool blocked = playerHoldsSelectableWeapon(effectiveControlObject(player));

            bool sendSwitchAck = true;
            if (currentPco->getPcoId() != requestedPcoId && !blocked)    // PCO +0x30
            {
                sendSwitchAck = false;                                   // every path below returns

                std::map<int, world::IPlayerControlObject*>* pcos = rootPco->getPcos();   // +0x60
                // AN empty whitelist means "ANY SEAT". getPcoPosId returns a vector<int> of the
                // positions this control object may switch to; only a NON-empty one is enforced.
                const std::vector<int>* allowed =
                    currentPco->getPcoPosId();                                          // +0x64

                bool permitted = true;
                if (!allowed->empty())
                    permitted = std::find(allowed->begin(), allowed->end(),
                                          requestedPcoId) != allowed->end();

                std::map<int, world::IPlayerControlObject*>::iterator it =
                    permitted ? pcos->find(requestedPcoId) : pcos->end();

                if (permitted && it != pcos->end())
                {
                    world::IPlayerControlObject* target = it->second;

                    if (!target->isOccupied())                           // PCO +0x18
                    {
                        // The seat is free - leave the old one and take it.
                        static_cast<Player*>(player)->m_worldTime = 0.0f;                  // Player +0xE4, raw
                        game->exitVehicle(player, true, true);           // IGame +0xA8
                        static_cast<Player*>(player)->m_worldTime = 0.0f;                  // written twice
                        game->enterVehicle(player,
                            static_cast<world::IObject*>(
                                target->queryInterface(world::IID_IObject)), false);
                    }
                    else if (!player->getIsAIPlayer())                   // +0x64 - bots never swap
                    {
                        // THE seat IS taken: the player is told (event 5), and only then is a
                        // swap even considered.
                        if (player == world::playerManager->getLocalHumanPlayer())
                        {
                            eventManager->postEvent(ECGameLogic, 5, 0, 0.0f);
                        }
                        else
                        {
                            PostRemoteEvent* event =
                                new PostRemoteEvent(ECGameLogic, 5, false, 0, 0, 0.0f);
                            game->sendGameEvent(player->getId(), event);  // IGame +0x90
                        }

                        // A human can swap seats with a bot; occupied human seats refuse the request.
                        world::IPlayer* occupant = target->getOccupingPlayer();   // PCO +0x10
                        if (occupant && occupant->getIsAIPlayer())
                        {
                            static_cast<Player*>(player)->m_worldTime = 0.0f;
                            game->exitVehicle(player, true, true);

                            // Clear the vacated AI unit's recent player-possession time.
                            ai::IAIObject* aiObject = static_cast<ai::IAIObject*>(
                                world::object_detail::LayoutAccess::aiObject(
                                static_cast<world::IObject*>(
                                    currentPco->queryInterface(world::IID_IObject))));
                            if (aiObject && aiObject->isUnit())
                                aiObject->getUnit()->setLastPlayerPossesionTime(-1000.0);

                            static_cast<Player*>(occupant)->m_worldTime = 0.0f;
                            game->exitVehicle(occupant, true, true);

                            static_cast<Player*>(player)->m_worldTime = 0.0f;
                            game->enterVehicle(player,
                                static_cast<world::IObject*>(
                                    target->queryInterface(world::IID_IObject)), false);

                            static_cast<Player*>(occupant)->m_worldTime = 0.0f;
                            game->enterVehicle(occupant,
                                static_cast<world::IObject*>(
                                    currentPco->queryInterface(world::IID_IObject)), false);
                        }
                        else
                        {
                            sendSwitchAck = true;    // a human is sitting there - nothing happens
                        }
                    }
                }
            }

            // gcc emitted this block twice (0x80FCB52 and 0x80FCFE4, the second jumping back into
            // the first's tail). One source statement, two copies.
            if (sendSwitchAck)
            {
                PostRemoteEvent* event =
                    new PostRemoteEvent(ECGameLogic, 5, player->getId(), 0, 0, 0.0f);
                game->sendGameEventIfPlaying(player->getId(), event);    // IGame +0x88
            }
        }
    }

    // Camera controls require a living player who is not man-down or in their default vehicle.
    if (player->getVehicle() == player->getDefaultVehicle())             // +0x48 / +0xA4
        return;
    if (!player->getIsAlive())                                           // +0x6C
        return;
    if (player->getIsManDown())                                         // +0x100
        return;

    if (inputPressed(input, io::PIToggleCameraMode))
    {
        // The cycle key does not change a spectator free camera's view mode.
        if (player->getVehicle() != player->getFreeCamera())
            world::toggleViewMode(player->getCamera(), true);            // +0x180
    }

    if (inputPressed(input, io::PICameraMode1))
    {
        // Select nose-cam mode 2 only from mode 1 when the server permits it; otherwise select mode 1.
        const bool noseCam = player->getCamera()->getViewMode() == 1 &&  // +0x1C
                             serverInfo->m_block.allowNoseCam != 0;
        player->getCamera()->setViewMode(
            static_cast<world::CameraViewMode>(noseCam ? 2 : 1), 0, false);   // +0x18
    }

    if (inputPressed(input, io::PICameraMode2))
        player->getCamera()->setViewMode(static_cast<world::CameraViewMode>(3), 0, false);
    if (inputPressed(input, io::PICameraMode3))
        player->getCamera()->setViewMode(static_cast<world::CameraViewMode>(4), 0, false);
    if (inputPressed(input, io::PICameraMode4))
        player->getCamera()->setViewMode(static_cast<world::CameraViewMode>(5), 0, false);
}

// Exit and entry run sequentially, allowing transfer in one action. Preserve the
// one-second cooldown and vehicle/parent rereads. enter uses a 3 m search, false
// uses 5 m; ladder momentum retains the original out-of-bounds template read.
void ServerGameLogic::toggleEntryPoint(world::IPlayer* playerArg, bool enter)
{
    if (!playerArg)
        return;

    world::IPlayer* const player = getPlayer(playerArg);        // the identity

    // gate 1: a spectator toggles nothing
    if (player->getVehicle() == player->getFreeCamera())         // +0x48 / +0xA8
        return;

    // gate 2: a dead or wrecked body
    // the template is read raw (+0x14) and dereferenced with NO null check, unlike the enter half.
    if (player->getVehicle() &&
        world::object_detail::LayoutAccess::objectTemplate(player->getVehicle())->getClassID()
            == world::CID_SoldierTemplate)
    {
        world::IArmorGLComp* const armor = static_cast<world::IArmorGLComp*>(
            world::object_detail::LayoutAccess::armorComponent(player->getVehicle()));   // +0x3C
        if (armor && armor->getStatus())                         // +0x5C: 0 alive, 1 wreck, 2 gone
            return;
    }

    // gate 3: the one-second world-time cooldown
    if (WorldPref::getWorldTime() - static_cast<Player*>(player)->m_worldTime < 1.0f)     // +0xE4
        return;

    world::IObject* vehicle = player->getVehicle();              // +0x48
    world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
        vehicle->queryInterface(world::IID_IPlayerControlObject));                        // +0x0C
    if (!pco)
        return;

    // `proceed` is decided before the exit half runs and is not recomputed afterwards: a player
    // whose seat forbids entering another vehicle still gets to leave it, they just do not get the
    // enter half in the same call.
    bool inVehicle = (vehicle != player->getDefaultVehicle());   // +0xA4
    bool proceed   = false;
    if (!inVehicle || pco->allowEnterOtherVehicle())             // +0x13C
        proceed = true;

    // the exit half
    if (inVehicle)
    {
        if (world::object_detail::LayoutAccess::objectTemplate(vehicle)
                ->queryInterface(world::IID_IPlayerControlObjectTemplate))                // +0x0C
        {
            if (!pco->isAllowedToExit())                                                  // +0xA4
            {
                game->failedExitVehicle(player->getId());        // IGame +0x114 / IPlayer +0x54
            }
            else if (game->exitVehicle(player, true, true))                            // +0xA8
            {
                static_cast<Player*>(player)->m_worldTime = static_cast<float>(WorldPref::getWorldTime());

                // the momentum handoff - see the header block, including the Ladder OOB note.
                if (world::object_detail::LayoutAccess::objectTemplate(player->getVehicle())
                        ->getClassID() == world::CID_SoldierTemplate)
                {
                    // getRootParent's result is dereferenced unguarded, as shipped.
                    world::IPhysicsNode* const from = static_cast<world::IPhysicsNode*>(
                        world::object_detail::LayoutAccess::physicsNode(
                            world::getRootParent(vehicle)));                              // +0x44
                    if (from)
                    {
                        world::IPhysicsNode* const to = static_cast<world::IPhysicsNode*>(
                            world::object_detail::LayoutAccess::physicsNode(
                                player->getVehicle()));                                   // +0x44
                        if (to)
                        {
                            // getClassID() is asked twice off the same raw template read, as
                            // compiled; only the redundant field loads are folded.
                            const world::IObjectTemplate* const vt =
                                world::object_detail::LayoutAccess::objectTemplate(vehicle);
                            float modifier = 0.7f;                              // flt_888FFF0
                            if (vt->getClassID() == world::CID_PlayerControlObjectTemplate ||
                                vt->getClassID() == world::CID_LadderTemplate)
                            {
                                // OUT OF bounds FOR A LadderTemplate (364 B) - shipped defect,
                                // reproduced deliberately. reinterpret_cast rather than a downcast
                                // so the lie is visible; the offset is identical either way because
                                // IObjectTemplate is PlayerControlObjectTemplate's primary base.
                                modifier = reinterpret_cast<const world::PlayerControlObjectTemplate*>(
                                    vt)->m_positionalSpeedModifier;                       // +0x2A8
                            }

                            const Vec3& speed = from->getPositionalSpeed();               // +0x60
                            Vec3 carried;
                            carried.x = modifier * speed.x;
                            carried.y = modifier * speed.y;
                            carried.z = modifier * speed.z;
                            to->setPositionalSpeed(carried);                              // +0x68
                        }
                    }
                }

                // the AI notify - dormant on a no-bot server. TWO ints, not reSpawnPlayer's three.
                if (ai::g_aiInstance && ai::aiInstance()->isInitialized())     // ds:0x8AC37F8 / +0x1C
                {
                    AiPlayerExitPayload payload;
                    payload.playerId = player->getId();                                  // +0x54
                    payload.aiObject = world::object_detail::LayoutAccess::aiObject(vehicle); // +0x34
                    eventManager->postEvent(ECAI, 3, &payload, 0.0f);
                }

                // only a successful exit refreshes these two
                inVehicle = false;
                vehicle   = player->getVehicle();                                         // +0x48
            }
        }
    }

    if (!proceed)
        return;

    // the enter half
    // here the raw template read IS null-checked, unlike gate 2 above.
    {
        const world::IObjectTemplate* const vt =
            world::object_detail::LayoutAccess::objectTemplate(vehicle);                  // +0x14
        if (vt && vt->getClassID() == world::CID_SoldierTemplate &&
            !static_cast<world::Soldier*>(vehicle)->isNotFireingOrHaveRecoil())           // +0x298
            return;
    }

    const float radius = enter ? 3.0f : 5.0f;      // cmovnz flt_888E6B8 (3.0) over 0x40A00000 (5.0)

    Vec3 position;
    {
        const Vec3& from = vehicle->getAbsolutePosition();                                // +0x6C
        position.x = from.x;
        position.y = from.y;
        position.z = from.z;
    }
    if (inVehicle && pco->getOccupingObject())     // +0x14 dice's spelling; asked twice, as shipped
    {
        const Vec3& from = pco->getOccupingObject()->getAbsolutePosition();               // +0x6C
        position.x = from.x;
        position.y = from.y;
        position.z = from.z;
    }

    // The entry point is a vehicle seat, exposed as IPlayerControlObject.
    world::IPlayerControlObject* entry = 0;
    if (player->getVehicle() &&
        world::object_detail::LayoutAccess::objectTemplate(player->getVehicle())->getClassID()
            != world::CID_ZiplineContainerTemplate &&
        world::object_detail::LayoutAccess::objectTemplate(player->getVehicle())->getClassID()
            != world::CID_GrapplingHookRopeContainerTemplate)
    {
        // the trailing trio: useEntryPointRadius = `enter`, acceptAiOccupied, requireLineOfSight.
        entry = world::BFfindEntryPoint(position, radius, player->getTeam(),              // +0xE8
                                        player, enter, true, true);
    }

    if (entry)
    {
        // a second exit, unconditional on the flag - the exit half above may have been skipped
        // (no PCO template) or refused, and retail leaves the seat here regardless of that outcome.
        if (inVehicle)
            game->exitVehicle(player, true, true);                                     // +0xA8

        world::IObject* const target = static_cast<world::IObject*>(
            entry->queryInterface(world::IID_IObject));                                   // +0x0C
        if (game->enterVehicle(player, target, false))                                 // +0xA4
            static_cast<Player*>(player)->m_worldTime = static_cast<float>(WorldPref::getWorldTime());
    }
    else
    {
        // the Special Forces fallback
        // the parent test is read twice on purpose - the zipline sweep can attach the soldier,
        // and that must stop the rope sweep. Do not hoist it into one local.
        if (!world::object_detail::LayoutAccess::parent(player->getDefaultVehicle()))      // +0x28
        {
            world::IObjectTemplate* const ziplineTemplate =
                world::objectTemplateManager->getTemplate(std::string("Zipline"));         // +0x24
            if (ziplineTemplate)
                static_cast<const world::ZiplineTemplate*>(ziplineTemplate)
                    ->checkForEntry(player);                       // [FRONTIER] - 0x81D2509
        }
        if (!world::object_detail::LayoutAccess::parent(player->getDefaultVehicle()))      // +0x28
        {
            for (std::map<int, world::GrapplingHookRope*>::iterator it =
                     world::grapplingHookRopeMap.begin();
                 it != world::grapplingHookRopeMap.end(); ++it)
            {
                it->second->checkForEntry(player);                 // recovered: ELF 0x817A9C0 / PE 0x578400
            }
        }
    }
}

bool ServerGameLogic::selectKitAndUnlockLevel(world::IPlayer* player, int kit, int unlockLevel)
{
    if (!player)
        return false;

    const int team = player->getTeam();

    if (team < 0 || team > 2 || kit < 0 || kit > 255)
        return false;
    if (m_kits.find((team << 8) + kit) == m_kits.end())
        return false;

    player->setKit(kit);                                            // vt +0x16C
    player->setChosenUnlockLevel(kit, team, unlockLevel);           // vt +0x250
    return true;
}

bool ServerGameLogic::selectSpawnGroup(world::IPlayer* player, int group)
{
    if (!player)
        return false;

    if (group < 0 || group > 255)
        return false;
    if (group != 0 &&
        (!spawnManager || !static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(group)))
        return false;

    player->setSpawnGroup(group);                                   // vt +0x154
    return true;
}

// Drops through the vehicle's item-container interface, then broadcasts the item's
// current position without a Python notification. Requires a valid vehicle/interface;
// the final Boolean is unused. IDs narrow to their wire widths.
void ServerGameLogic::handleDrop(world::IObject* item, world::IPlayer* player,
                                 world::IObject* playerVehicle, bool /*unused*/)
{
    world::IItemContainerObject* container = 0;
    if (playerVehicle)
    {
        container = static_cast<world::IItemContainerObject*>(
            playerVehicle->queryInterface(world::IID_IItemContainerObject));
    }
    container->handleDropOnItem(player, item);             // +0x28  unchecked, as shipped

    HandleDropEvent* const event = new HandleDropEvent(
        static_cast<unsigned char>(player->getId()),
        static_cast<unsigned short>(item->getNetworkable()->m_networkableId),
        static_cast<unsigned short>(playerVehicle->getNetworkable()->m_networkableId),
        item->getAbsolutePosition());
    game->sendGameEventToAllPlaying(event);                // IGame +0x84
}

// Transfers an item through the vehicle's item-container interface, broadcasts the
// pickup, and raises PickupKit for kit templates. Requires a vehicle implementing
// IItemContainerObject. The final bool is unused; player ids are narrowed to one byte.
void ServerGameLogic::handlePickup(world::IObject* item, world::IPlayer* player,
                                   world::IObject* playerVehicle, bool /*unused - see above*/)
{
    world::IItemContainerObject* container = 0;
    if (playerVehicle)
    {
        container = static_cast<world::IItemContainerObject*>(
            playerVehicle->queryInterface(world::IID_IItemContainerObject));
    }
    container->handlePickupOnItem(player, item);          // +0x24  unchecked, as shipped

    HandlePickupEvent* const event = new HandlePickupEvent(
        static_cast<unsigned char>(player->getId()),
        static_cast<unsigned short>(item->getNetworkable()->m_networkableId),
        static_cast<unsigned short>(playerVehicle->getNetworkable()->m_networkableId));
    game->sendGameEventToAllPlaying(event);               // IGame +0x84

    if (world::object_detail::LayoutAccess::objectTemplate(item)->getClassID()
            == world::CID_KitTemplate)
    {
        python::PyRef args = python::buildArgs(player, item);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(4), args.get());   // PickupKit
    }
}

// Requests a team change, notifies Python, and sends a correction if the player
// setter refused it. Null players return false. Keep repeated player-id reads on
// the correction path.
bool ServerGameLogic::selectTeam(world::IPlayer* player, int team)
{
    if (!player)
        return false;

    if (team <= 0 || team >= 3)
        return false;

    player->setTeam(team, false);                                   // vt - the validating setter

    {
        python::PyRef args = python::buildArgs(player, m_shouldUpdateTicketSystem);   // +0x28C
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(7), args.get());
    }

    if (team != player->getTeam())                                  // vt +0xE8 - the change was refused
    {
        int payload[2];
        payload[0] = player->getId();                               // getId, the first of three
        payload[1] = -1;
        PostRemoteEvent* const event = new PostRemoteEvent(
            ECGameLogic, 8, player->getId(), payload, 8, 0.0f);     // the second
        game->sendGameEventIfPlaying(player->getId(), event);       // the third; IGame +0x88
    }
    return true;
}

// Schedules respawn from the configured delay, clears man-down state, notifies AI
// when initialized, and calls the base kill path. Returns false for null players.
// The AI notification requires the player's current vehicle.
bool ServerGameLogic::reSpawnPlayer(world::IPlayer* player)
{
    if (!player)
        return false;

    const int delayTicks =
        static_cast<int>(serverSettings->getSpawnTime() * 30.0f);   // cvttss2si - truncation
    player->getVehicle();                                           // discarded, as shipped
    player->setIsManDown(false);                                    // vt +0xFC
    player->setSpawnAtTick(WorldPref::getTickCount() + delayTicks); // vt +0xF0

    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())      // ds:0x8AC37F8 / vt +0x1C
    {
        world::IObject* const vehicle = player->getVehicle();
        AiPlayerVehiclePayload payload;
        payload.team = player->getTeam();                          // vt +0xE8
        payload.playerId = player->getId();                        // vt +0x54
        payload.aiObject = world::object_detail::LayoutAccess::aiObject(vehicle); // +0x34, unchecked
        eventManager->postEvent(ECAI, 0, &payload, 0.0f);
    }

    return GameLogic::killPlayer(player, false, false);             // the direct base call
}

// Spawns the player's soldier and kit for an active round. The team/kit key selects both.
// Missing soldier skins fall back to us_light_soldier. An explicit transform takes precedence
// over the local-camera override and the spawn point; the invalid-position sentinel refuses spawn.
// Moving platforms contribute tangent speed. Kit creation failure is fatal.
// [UNRESOLVED] The failed-kit branch's original expression is unknown; its null dereference remains.
void ServerGameLogic::spawnPlayer(world::IPlayer* playerArg, std::pair<Vec3, Vec3>* at)
{
    if (!m_roundGoing)                                                  // +0x214
        return;

    world::IPlayer* player = getPlayer(playerArg);
    if (!player || getGameStatus() != 1)
        return;

    // UseAI restricts bots only in the first spawn wave, before ticket updates begin.
    bool refuseBecauseAI = false;
    if (!m_shouldUpdateTicketSystem)                                    // +0x28C
    {
        if (settings->boolGet(std::string("UseAI"), false) == 1)
            refuseBecauseAI = (player->getIsAIPlayer() == 1);           // +0x64
    }
    if (refuseBecauseAI)
        return;

    m_shouldUpdateTicketSystem = true;

    if (!player->getSpawnGroup() && !at)                                // +0x158
        return;
    if (!player->getTeam())                                             // +0xE8 - team 0 is nobody
        return;

    // Teams with zero or negative tickets cannot respawn.
    world::TeamScore* teamScore =
        reinterpret_cast<world::ScoreManager*>(world::scoreManager)->getTeamScore(player->getTeam());   // VMT +0x10
    if (teamScore->m_tickets <= 0)
        return;

    player->allocateFreeCamera();                                       // +0x308

    world::IObject* localCamera = 0;
    if (m_byte26B)                                                      // +0x26B
    {
        world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
        if (!local)
        {
            player->setSpawnGroup(0);                                   // +0x154
            return;
        }
        localCamera = local->getVehicle();                              // +0x48
    }

    // Build the object name from the player's name followed by the template name.
    std::string objectName;
    objectName.append(player->getName());                               // +0x24

    const int kitKey = (player->getTeam() << 8) + player->getKit();     // +0xE8 / +0x170

    const world::IObjectTemplate* soldierTemplate = 0;
    std::map<int, std::string>::iterator kt = m_soldierSkins.find(kitKey);
    if (kt == m_soldierSkins.end())
    {
        // Use the built-in fallback soldier template.
        soldierTemplate = world::objectTemplateManager->getTemplate(std::string("us_light_soldier"));
        objectName.append("US_soldier", strlen("US_soldier"));
    }
    else
    {
        soldierTemplate = world::objectTemplateManager->getTemplate(kt->second);
        objectName.append(kt->second);
    }

    // pick a spawn point unless the caller supplied a transform
    ISpawnPoint* spawnPoint = 0;
    if (!at)
    {
        ISpawnGroup* group = static_cast<SpawnManager*>(spawnManager)
                                 ->getSpawnGroup(player->getSpawnGroup());   // +0x28
        if (!group)                                    { player->setSpawnGroup(0); return; }
        if (!group->getActive(player->getIsAIPlayer())) { player->setSpawnGroup(0); return; }   // +0x2C
        if (group->getTeam() != player->getTeam())      { player->setSpawnGroup(0); return; }   // +0x1C
        if (group->getSquad() > 0 &&                                                            // +0x24
            group->getSquad() != player->getSquadId())  { player->setSpawnGroup(0); return; }   // +0x10C
        spawnPoint = group->getSpawnPoint(player->getIsAIPlayer());                              // +0x44
        if (!spawnPoint)                               { player->setSpawnGroup(0); return; }
    }

    Vec3 rotation; rotation.x = 0.0f; rotation.y = 0.0f; rotation.z = 0.0f;
    Vec3 position; position.x = 0.0f; position.y = 0.0f; position.z = 0.0f;
    Vec3 tangentSpeed; tangentSpeed.x = 0.0f; tangentSpeed.y = 0.0f; tangentSpeed.z = 0.0f;

    if (at)
    {
        position = at->first;
        rotation = at->second;
    }
    else if (m_byte26B)
    {
        // Spawn two metres ahead of the local camera, negating its Euler angles.
        float zDeg, xDeg, yDeg;
        getRotationZXY(localCamera->getAbsoluteTransformation(), zDeg, xDeg, yDeg);
        rotation.x = -yDeg;
        rotation.y = -xDeg;
        rotation.z = -zDeg;

        const Vec3& cameraPos = localCamera->getAbsolutePosition();
        const Mat4& cameraXf  = localCamera->getAbsoluteTransformation();
        position.x = cameraPos.x + cameraXf.m[8]  * 2.0f;
        position.y = cameraPos.y + cameraXf.m[9]  * 2.0f;
        position.z = cameraPos.z + cameraXf.m[10] * 2.0f;
    }
    else
    {
        world::IObject* spawnObject =
            static_cast<world::IObject*>(spawnPoint->queryInterface(world::IID_IObject));
        if (!spawnObject)                                                 { player->setSpawnGroup(0); return; }

        ISpawnPoint* point = getSpawnPoint(spawnObject);
        point->getSpawnPosition(position, player->getIsAIPlayer());       // +0x34

        // THE (-4000,-4000,-4000) sentinel - see the header block.
        const float dx = -4000.0f - position.x;
        const float dy = -4000.0f - position.y;
        const float dz = -4000.0f - position.z;
        if (dx * dx + dy * dy + dz * dz < 0.001f)                         { player->setSpawnGroup(0); return; }

        rotation = point->getSpawnRotation();                             // +0x38

        // a moving spawn platform contributes its tangent speed at the spawn position
        world::IObject* root = world::getRootParent(spawnObject);
        if (root)
        {
            world::IPhysicsNode* node = static_cast<world::IPhysicsNode*>(
                world::object_detail::LayoutAccess::physicsNode(root));    // +0x44
            if (node)
                tangentSpeed = node->getTangentSpeed(spawnObject->getAbsolutePosition());   // +0xA4
        }
    }

    // build the body
    world::IObject* body = game->createObjectOnAllClients(soldierTemplate, position, rotation,
                                                         true, 0, 0);     // IGame +0x78
    if (!body)                                                            { player->setSpawnGroup(0); return; }

    body->setName(objectName);
    player->setFreeCameraEnabled(false);                                  // +0xAC
    game->enterVehicle(player, body, true);                               // IGame +0xA4
    player->setIsAlive(true);                                             // +0x68  alive AT last

    // and the kit at its feet
    const int kitKey2 = player->getKit() + (player->getTeam() << 8);
    std::map<int, std::string>::iterator ki = m_kits.find(kitKey2);
    if (ki == m_kits.end())
    {
        // The site that attests this file's line 2056. fatal.
        Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 2056, __FILE__)
            << "TBM: Unable to create the requested kit!";
        return;
    }

    const world::IObjectTemplate* kitOwner = world::objectTemplateManager->getTemplate(ki->second);
    // const_cast: queryImplementation is non-const in the ELF too - see ObjectTemplate.h's note.
    void* kitTemplate = const_cast<world::IObjectTemplate*>(kitOwner)
                            ->queryImplementation(world::CID_KitTemplate);
    if (!kitTemplate)
    {
        // The site that attests this file's line 2063. fatal, and the only one that prints the
        // name it could not build.
        Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 2063, __FILE__)
            << "TBM: Unable to create the requested kit! " << ki->second;
        return;
    }

    world::IObject* kitObject = game->createObjectOnAllClients(
        static_cast<const world::IObjectTemplate*>(kitTemplate), position, rotation, true, 0, 0);
    if (!kitObject)
    {
        // [UNRESOLVED] The original path dereferences a proven-null pointer before this
        // fatal diagnostic. That folded source expression remains omitted.
        Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 2073, __FILE__)
            << "TBM: Unable to create the requested kit!";
        return;
    }

    handlePickup(kitObject, player, player->getVehicle(), true);          // IGameLogic +0xEC

    // and everything that follows a successful spawn
    // A RAW write TO Player +0xE4, the same field the seat-switch clears to 0.0f. Here it is
    // 9999.0f, a sentinel meaning "long ago".
    static_cast<Player*>(player)->m_worldTime = -9999.0f;

    if (spawnPoint)
    {
        spawnPoint->queryInterface(world::IID_IObject);   // result discarded - compiled that way

        if (spawnPoint->getEnterOnSpawn() ||                                          // +0x20
            (player->getIsAIPlayer() && spawnPoint->getAIEnterOnSpawn()))              // +0x24
        {
            toggleEntryPoint(player, false);
        }
        else if (spawnPoint->getProneOnSpawn() && !player->getIsAIPlayer())            // +0x48
        {
            // queryInterface is deliberately repeated. Both shipped ports guard the first
            // result, then use a fresh second result for the downcast/call; if that result changed
            // to null, retail would fault at the virtual call. Keep that observable quirk.
            if (body->queryInterface(world::IID_ISoldier))
                static_cast<world::Soldier*>(static_cast<world::ISoldier*>(
                    body->queryInterface(world::IID_ISoldier)))
                    ->setRequestedPose(world::SoldierPose_Prone);
        }
    }

    // event 9 IN category 6 carries THE player ID AS A 4-byte payload - "this player spawned".
    const int spawnedId = player->getId();                                            // +0x54
    PostRemoteEvent* event = new PostRemoteEvent(static_cast<EventCategory>(6), 9, false,
                                                 const_cast<int*>(&spawnedId), 4, 0.0f);
    game->sendGameEventToAllConnected(event);                                         // IGame +0x94
    eventManager->postEvent(ECGame, 6, player, 0.0f);                                 // +0x14

    // Seed the ancestor armor component's collision height to avoid immediate fall damage after spawning.
    world::IArmorGLComp* armor = 0;
    for (world::IObject* node = body; node; node = node->getParent())
    {
        armor = static_cast<world::IArmorGLComp*>(
            world::object_detail::LayoutAccess::armorComponent(node));                 // +0x3C
        if (armor)
            break;
    }
    if (armor)
        armor->setLastCollisionHeight(position.y);                                     // +0xA0

    if (player->getIsSquadLeader())                                                    // +0x13C
    {
        ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(
            player->getTeam(), player->getSquadId());                                  // +0x28
        if (info)
            info->addSquadLeaderSpawnPoint(player);                                    // +0x20
    }

    m_shouldUpdateTicketSystem = true;
}

// Class-owned IBase implementation with non-atomic reference counting.
void ServerGameLogic::addRef()                { ++m_refCount; }          // +0x1EC

int ServerGameLogic::getRef()                 { return m_refCount; }

// Drops one reference, deletes virtually at zero, and returns the remaining count.
// Use the C++ destructor dispatch so each platform applies its own virtual ABI.
int ServerGameLogic::release()
{
    const int r = --m_refCount;                       // stored back at 0x8105B15
    if (r == 0 && this)                               //  jnz / 0x8105B1D test edx,edx
        delete this;                                  // vt +0x2C8 = D0, the deleting destructor
    return r;
}

// Deletes the commander-tools and UAV managers. Their destructors must clear
// the globals before the base GameLogic destructor checks the UAV manager again.
ServerGameLogic::~ServerGameLogic()
{
    if (world::commanderToolsManager)
        delete world::commanderToolsManager;
    if (world::uavVehicleManager)
        delete world::uavVehicleManager;
}

// queryInterface - 0x8105B40, 32 B, and it is const (the _ZNK mangling says so).
// Answers exactly two ids and hands back `this` unadjusted for both - single inheritance all the way
// down, so the IGameLogic subobject sits at offset 0. Everything else, including IID_IGameServer,
// gets null: MapList::isModifiable asking the game for IID_IGameServer is a different object.
void* ServerGameLogic::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<ServerGameLogic*>(this);
    if (iid == IID_IGameLogic)                        // via cmovz
        return const_cast<ServerGameLogic*>(this);
    return 0;
}

// Creates a caller-owned interface reference. Rejects aggregation and releases
// the new object if the requested interface is unsupported.
IBase* ServerGameLogic::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;                                     // no aggregation

    ServerGameLogic* logic = new ServerGameLogic;     // operator new(0x290) + C1 at 0x8531221
    IBase* result = static_cast<IBase*>(logic->queryInterface(iid));   // +0x0C
    if (!result)
        logic->release();                             // +0x08
    return result;
}

// Set a team's ticket count through ScoreManager. Only teams 1 and 2 are valid;
// team-indexed ticket arrays use team - 1.
void ServerGameLogic::setNumberOfTickets(unsigned int team, unsigned int tickets)
{
    if (team - 1 <= 1)
    {
        reinterpret_cast<world::ScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->setTickets(static_cast<int>(tickets));
    }
}

// getNumberOfTickets - 0x8103760, 57 B: the TeamScore +0x84 field, raw as shipped.
unsigned int ServerGameLogic::getNumberOfTickets(unsigned int team) const
{
    if (team - 1 <= 1)
    {
        return static_cast<unsigned int>(
            reinterpret_cast<world::ScoreManager*>(world::scoreManager)
                ->getTeamScore(team)->m_tickets);
    }
    return 0;
}

// setDefaultNumberOfTickets - 0x81037A0, 29 B: the per-team default store.
void ServerGameLogic::setDefaultNumberOfTickets(unsigned int team, unsigned int tickets)
{
    if (team - 1 <= 1)
        m_defaultNumberOfTickets[team - 1] = tickets;
}

// The per-team size->tickets maps at ds:0x8AAC718 - three 12-byte
// std::map<unsigned int, unsigned int> values in a row, indexed BY team (slot 0 unused).
namespace {
std::map<unsigned int, unsigned int> g_defaultTicketsBySize[3];   // ds:0x8AAC718 [team]
} // namespace

// setDefaultNumberOfTicketsEx - ELF 0x81038D0 (161 B), PE 0x457A30. Teams outside 1/2 are
// ignored. A repeated max-player threshold overwrites its ticket count; no duplicate node remains.
void ServerGameLogic::setDefaultNumberOfTicketsEx(unsigned int maxPlayers, unsigned int team,
                                                   unsigned int tickets)
{
    if (team - 1 <= 1)
    {
        std::pair<std::map<unsigned int, unsigned int>::iterator, bool> inserted =
            g_defaultTicketsBySize[team].insert(std::make_pair(maxPlayers, tickets));
        if (!inserted.second)
            inserted.first->second = tickets;
    }
}

// getDefaultNumberOfTicketsEx - 0x8103980, 192 B: the size map's lower-bound walk - the first
// entry whose key >= maxPlayers, the last entry when none is, the flat default when empty.
unsigned int ServerGameLogic::getDefaultNumberOfTicketsEx(unsigned int maxPlayers, unsigned int team)
{
    if (team - 1 > 1)
        return 0;

    const std::map<unsigned int, unsigned int>& bySize = g_defaultTicketsBySize[team];
    if (bySize.empty())
        return m_defaultNumberOfTickets[team - 1];
    if (bySize.size() == 1)
        return static_cast<unsigned int>(bySize.begin()->second);

    std::map<unsigned int, unsigned int>::const_iterator it = bySize.begin();
    for (; it != bySize.end(); ++it)
    {
        if (maxPlayers <= it->first)
            return static_cast<unsigned int>(it->second);
    }
    --it;                                                // past the end: the last entry answers
    return static_cast<unsigned int>(it->second);
}

// getDefaultNumberOfTickets - 0x81037C0, 272 B: every game mode asks the map with the MAP's max players,
// coop always uses 16.
unsigned int ServerGameLogic::getDefaultNumberOfTickets(unsigned int team)
{
    std::string mode;
    if (game)
        mode = game->getCurrentMapGameMode();
    if (compareIgnoreCase(mode.c_str(), "gpm_coop") == 0) // if coop then force to 16
        return getDefaultNumberOfTicketsEx(16, team);
    return getDefaultNumberOfTicketsEx(game->getCurrentMapMaxPlayers(), team);
}

// setTicketChangePerSecond - 0x80FF010, 100 B: the rate store AND the TeamScore bleed flag - the
// only caller of setTicketBleed, which is what makes a Conquest round end at all.
void ServerGameLogic::setTicketChangePerSecond(unsigned int team, float rate)
{
    if (team - 1 <= 1)
    {
        m_ticketChangePerSecond[team - 1] = rate;
        reinterpret_cast<world::ScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->setTicketBleed(rate != 0.0f);
    }
}

// getTicketChangePerSecond - 0x80FF080, 34 B. -1.0f for an invalid team, not 0.
float ServerGameLogic::getTicketChangePerSecond(unsigned int team)
{
    if (team - 1 > 1)
        return -1.0f;
    return m_ticketChangePerSecond[team - 1];
}

// setTicketState - 0x80FF0B0, 66 B / getTicketState - 0x80FF100, 51 B (TeamScore +0x8C raw;
// 1 for an invalid team).
void ServerGameLogic::setTicketState(unsigned int team, int state)
{
    if (team - 1 <= 1)
    {
        reinterpret_cast<world::ScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->setTicketState(state);
    }
}

int ServerGameLogic::getTicketState(unsigned int team)
{
    if (team - 1 <= 1)
    {
        return reinterpret_cast<world::ScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->m_ticketState;
    }
    return -1;
}

// setTicketLimit - 0x80FF140, 85 B: insert {id -> limit}; a repeated ID is refused
// (insert_unique), as shipped. getTicketLimit - 0x80FF1A0, 95 B: find; miss -> -1... NO:
// miss -> 0 (`v2 = 0` before the compare), -1 only for a bad team. The asymmetry is the ELF's.
void ServerGameLogic::setTicketLimit(unsigned int team, int id, int limit)
{
    if (team - 1 <= 1)
        m_ticketLimits[team - 1].insert(std::make_pair(id, limit));
}

int ServerGameLogic::getTicketLimit(unsigned int team, int key)
{
    if (team - 1 > 1)
        return -1;
    std::map<int, int>::const_iterator it = m_ticketLimits[team - 1].find(key);
    if (it == m_ticketLimits[team - 1].end())
        return 0;
    return it->second;
}

// set/getTicketLossPerMin - 0x8103A40, 29 B / 0x8103A60, 34 B (-1.0f for a bad team).
void ServerGameLogic::setTicketLossPerMin(unsigned int team, float rate)
{
    if (team - 1 <= 1)
        m_ticketLossPerMin[team - 1] = rate;
}

float ServerGameLogic::getTicketLossPerMin(unsigned int team)
{
    if (team - 1 > 1)
        return -1.0f;
    return m_ticketLossPerMin[team - 1];
}

// get/setTicketLossAtEndPerMin - 0x8105BB0, tiny pair on the single float at +0x234.
float ServerGameLogic::getTicketLossAtEndPerMin()
{
    return m_ticketLossAtEndPerMin;
}

void ServerGameLogic::setTicketLossAtEndPerMin(float rate)
{
    m_ticketLossAtEndPerMin = rate;
}

namespace {

// . The helper is in ServerGameLogic.cpp's anonymous namespace and applies the same
// level to keys 0..7 inclusive in both team maps. setUnlocks calls it with 99 for unlock-all mode.
void setAllUnlockLevels(std::map<int, int>* unlocks, int level)
{
    for (int key = 0; key <= 7; ++key)
    {
        unlocks[0][key] = level;
        unlocks[1][key] = level;
    }
}

} // anonymous namespace

// ServerGameLogic::setUnlocks - ELF 0x8103CC0 (141 B). Applies the two-map array to the player,
// then sends UnlockEvent type 1 only to that player's connection. No null guards in retail.
void ServerGameLogic::setUnlocks(world::IPlayer* player, std::map<int, int>* unlocks)
{
    if (m_unlockAll)
        setAllUnlockLevels(unlocks, 99);

    player->setUnlocks(unlocks);
    game->sendGameEventIfConnected(static_cast<unsigned int>(player->getId()),
        new UnlockEvent(static_cast<UnlockEvent::UnlockEventTypes>(1), player->getId(), unlocks));
}

namespace python {

PyObject* sgl_setParam(PyObject*, PyObject* args)
{
    std::string key;
    if (py_arg<std::string>(args, 0, key))               // 1 means failed - see py/PyArg.h
        return 0;

    unsigned int team  = 0;
    double       value = 0.0;
    int          id    = 0;

    // all three run; py_arg's sticky error guard is what makes that cheap. See above.
    const int failed = py_arg<int>(args, 1, reinterpret_cast<int&>(team))
                     + py_arg<double>(args, 2, value)
                     + py_arg<int>(args, 3, id);
    if (failed)
    {
        PyErr_SetString(PyExc_RuntimeError, "function needs team, value and id parameter");
        return 0;
    }

    ServerGameLogic* const sgl = static_cast<ServerGameLogic*>(gameLogic);      // ds:0x8ACBFA0
    // the tree's established form for this global: it is declared `IBase*` (Core/Globals.cpp) and
    //   cast at the use site, the same way reset() and the score path above do it.
    world::ScoreManager* const scores =
        reinterpret_cast<world::ScoreManager*>(world::scoreManager);            // ds:0x8AD1740

    // four keys to the game logic, five to the score manager - and each narrows the double
    //    differently. See the note above.
    if (key.compare("tickets") == 0)
        sgl->setNumberOfTickets(team, static_cast<unsigned int>(value));
    else if (key.compare("ticketLimit") == 0)
        // GameLogic.py passes (team, limitId, threshold) here: the generic `value` is
        // the limit ID and the generic `id` argument is its threshold. Keep their order.
        sgl->setTicketLimit(team, static_cast<int>(value), id);
    else if (key.compare("ticketState") == 0)
        sgl->setTicketState(team, static_cast<int>(value));                  // virtual, +0x8C
    else if (key.compare("ticketChangePerSecond") == 0)
        sgl->setTicketChangePerSecond(team, static_cast<float>(value));      // the bleed
    else if (key.compare("healScoreLimit") == 0)
        scores->setHealScoreLimit(static_cast<float>(value));
    else if (key.compare("repairScoreLimit") == 0)
        scores->setRepairScoreLimit(static_cast<float>(value));
    else if (key.compare("giveAmmoScoreLimit") == 0)
        scores->setAmmoScoreLimit(static_cast<float>(value));
    else if (key.compare("teamDamageScoreLimit") == 0)
        scores->setTeamDamageLimit(static_cast<float>(value));
    else if (key.compare("teamVehicleDamageScoreLimit") == 0)
        scores->setTeamVehicleDamageLimit(static_cast<float>(value));
    else
    {
        PyErr_SetString(PyExc_RuntimeError, "bad attribute");
        return 0;
    }

    Py_INCREF(Py_None);                                  // the ELF's `++_Py_NoneStruct`
    return Py_None;
}

// sgl_endGame - ELF 0x8154C90 (121 B), PE 0x533810 (90 B).
// Parse the winning team and victory type, dispatch the concrete server game-logic round end, and
// return None. The downstream reset selects FreeCamera::setViewMode's recovered mode-1 arm; it does
// not enter that function's still-named mode-6 death/follow-camera frontiers.
PyObject* sgl_endGame(PyObject*, PyObject* args)
{
    unsigned int winnerTeam = 0;
    int victoryType = 0;
    const int failed = py_arg<int>(args, 0, reinterpret_cast<int&>(winnerTeam))
                     + py_arg<int>(args, 1, victoryType);
    if (failed)
        return 0;

    static_cast<ServerGameLogic*>(gameLogic)->endGame(winnerTeam, victoryType);
    Py_INCREF(Py_None);
    return Py_None;
}

// sgl_getControlPointsInGroup - ELF 0x81550D0 (271 B), PE 0x534280. The host wrapper is complete
// and typed; its concrete GameLogic::getControlPointsInGroup backing call is recovered too.
PyObject* sgl_getControlPointsInGroup(PyObject*, PyObject* args)
{
    int group = 0;
    if (py_arg<int>(args, 0, group))
        return 0;

    std::vector<world::IObject*> controlPoints;
    gameLogic->getControlPointsInGroup(controlPoints, group);

    PyObject* const result = PyTuple_New(static_cast<int>(controlPoints.size()));
    if (!result)
        return 0;

    for (std::vector<world::IObject*>::const_iterator it = controlPoints.begin();
         it != controlPoints.end(); ++it)
        PyTuple_SET_ITEM(result, static_cast<int>(it - controlPoints.begin()), wrapObject(*it));
    return result;
}

// sgl_getModDirectory - ELF 0x81558A0 (240 B), PE 0x5330F0 (138 B). The setting default is the
// empty string, not "mods/bf2" (several other consumers deliberately use a different default).
PyObject* sgl_getModDirectory(PyObject*, PyObject*)
{
    const std::string value = settings->stringGet(std::string("GSModDirectory"), std::string());
    return PyString_FromString(value.c_str());
}

// sgl_getMapName - ELF 0x8155990 (293 B), PE 0x533180 (168 B). Prefer live game state; before a
// map is live, fall back to the GSLoadLevel request using the shipped empty default.
PyObject* sgl_getMapName(PyObject*, PyObject*)
{
    std::string value = game->getCurrentMapName();
    if (value.empty())
        value = settings->stringGet(std::string("GSLoadLevel"), std::string());
    return PyString_FromString(value.c_str());
}

// sgl_getWorldSize - ELF 0x8155AC0 (290 B), PE 0x533230 (169 B). A two-element (X,Z) tuple;
// PyTuple_SetItem steals both integer references exactly as in the shipped wrapper.
PyObject* sgl_getWorldSize(PyObject*, PyObject*)
{
    int fallback = 1024;
    const int x = settings->intGet(std::string("GLSWorldSizeX"), fallback);
    fallback = 1024;
    const int z = settings->intGet(std::string("GLSWorldSizeZ"), fallback);

    PyObject* result = PyTuple_New(2);
    PyTuple_SetItem(result, 0, PyInt_FromLong(x));
    PyTuple_SetItem(result, 1, PyInt_FromLong(z));
    return result;
}

// The two PostRemoteEvent senders are near twins. Their bounds are the actual registered category
// maxima in the ELF (24 and 78), despite the shared and misleading "one byte" error literal.
PyObject* sgl_sendGameLogicEvent(PyObject*, PyObject* args)
{
    int playerId = 0;
    unsigned int eventId = 0;
    int payload = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, reinterpret_cast<int&>(eventId))
                     + py_arg<int>(args, 2, payload);
    if (failed)
        return 0;
    if (eventId > 24)
    {
        PyErr_SetString(PyExc_RuntimeError, "only one event ID byte allowed");
        return 0;
    }

    game->sendGameEventIfConnected(static_cast<unsigned int>(playerId),
        new PostRemoteEvent(ECGameLogic, eventId, playerId, &payload, 4, 0.0f));
    Py_INCREF(Py_None);
    return Py_None;
}

// Requires at most PythonCommandEvent::MaxArgs integers; the original wrapper
// does not bound its local array. Type errors still allow event construction/send
// and return None with the Python error pending.
PyObject* sgl_sendPythonEvent(PyObject*, PyObject* args)
{
    int playerId = 0;
    int commandId = 0;
    PyObject* valuesTuple = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, commandId)
                     + py_arg<PyObject*>(args, 2, valuesTuple);
    if (failed)
        return 0;

    int values[PythonCommandEvent::MaxArgs] = { 0, 0, 0, 0 };
    const int count = PyTuple_Size(valuesTuple);
    for (int i = 0; i < count; ++i)
    {
        PyObject* const item = PyTuple_GetItem(valuesTuple, i);
        if (item && PyInt_Check(item))
        {
            values[i] = static_cast<int>(PyInt_AsLong(item));     // no i < 4 check, as shipped
            PyErr_Occurred();                                     // result deliberately discarded
        }
        else
            PyErr_SetString(PyExc_RuntimeError, "expected an integer");
    }

    if (playerId == -1)
    {
        game->sendGameEventToAllConnected(new PythonCommandEvent(
            commandId, playerId, true, true, values[0], values[1], values[2], values[3]));
    }
    else
    {
        bool executeLocally = false;
        world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
        if (local)
            executeLocally = local->getId() == playerId;
        game->sendGameEventIfConnected(static_cast<unsigned int>(playerId),
            new PythonCommandEvent(commandId, playerId, executeLocally, true,
                                   values[0], values[1], values[2], values[3]));
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* sgl_sendMedalEvent(PyObject*, PyObject* args)
{
    int playerId = 0;
    int medal = 0;
    int value = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, medal)
                     + py_arg<int>(args, 2, value);
    if (failed)
        return 0;

    game->sendGameEventToAllPlaying(
        new MedalEvent(static_cast<world::Medal>(medal), playerId, value));
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* sgl_sendRankEvent(PyObject*, PyObject* args)
{
    int playerId = 0;
    unsigned int newRank = 0;
    unsigned int value = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, reinterpret_cast<int&>(newRank))
                     + py_arg<int>(args, 2, reinterpret_cast<int&>(value));
    if (failed)
        return 0;

    game->sendGameEventToAllPlaying(new RankEvent(
        static_cast<RankEvent::RankEventTypes>(1), newRank, value, playerId));
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* sgl_sendHudEvent(PyObject*, PyObject* args)
{
    int playerId = 0;
    unsigned int eventId = 0;
    int payload = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, reinterpret_cast<int&>(eventId))
                     + py_arg<int>(args, 2, payload);
    if (failed)
        return 0;
    if (eventId > 78)
    {
        PyErr_SetString(PyExc_RuntimeError, "only one event ID byte allowed");
        return 0;
    }

    game->sendGameEventIfConnected(static_cast<unsigned int>(playerId),
        new PostRemoteEvent(ECHud, eventId, playerId, &payload, 4, 0.0f));
    Py_INCREF(Py_None);
    return Py_None;
}

// sgl_sendTextMessage - ELF 0x8155730 (438 B), PE 0x533D20. All five py_arg extractions run and
// their error counts are summed. Target channel 14 (TCPlayer) forces the target id to the sender /
// player id; every other channel uses argument four unchanged. Retail does not null-check the
// IGameServer query result before dispatching its five-argument say slot.
PyObject* sgl_sendTextMessage(PyObject*, PyObject* args)
{
    int playerId = 0;
    int channel = 0;
    int type = 0;
    std::string message;
    int target = 0;
    const int failed = py_arg<int>(args, 0, playerId)
                     + py_arg<int>(args, 1, channel)
                     + py_arg<int>(args, 2, type)
                     + py_arg<std::string>(args, 3, message)
                     + py_arg<int>(args, 4, target);
    if (failed)
        return 0;

    if (channel == TCPlayer)
        target = playerId;

    IGameServer* gameServer = 0;
    if (game)
        gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
    gameServer->say(message, static_cast<TargetChannel>(channel),
                    static_cast<ChannelMessageType>(type),
                    static_cast<unsigned int>(playerId), static_cast<unsigned int>(target));
    Py_INCREF(Py_None);
    return Py_None;
}

// sgl_getSettingsBool - ELF 0x8156020 (193 B), PE 0x5341F0 (131 B).
PyObject* sgl_getSettingsBool(PyObject*, PyObject* args)
{
    std::string key;
    if (py_arg<std::string>(args, 0, key))
        return 0;
    const bool fallback = false;
    return PyInt_FromLong(settings->boolGet(key, fallback) != 0);
}

// sgl_getParam - ELF 0x81549D0, read in full. setParam's read twin: TEN
// keys against setParam's nine, three required arguments whatever the key (only ticketLimit uses
// the third), and the shipped typo in the error text - "team an id parameter" - preserved.
PyObject* sgl_getParam(PyObject*, PyObject* args)
{
    std::string key;
    if (py_arg<std::string>(args, 0, key))
        return 0;

    unsigned int team = 0;
    int          id   = 0;
    const int failed = py_arg<int>(args, 1, reinterpret_cast<int&>(team))
                     + py_arg<int>(args, 2, id);
    if (failed)
    {
        PyErr_SetString(PyExc_RuntimeError, "function needs team an id parameter");   // sic
        return 0;
    }

    ServerGameLogic* const sgl = static_cast<ServerGameLogic*>(gameLogic);      // ds:0x8ACBFA0

    if (key.compare("tickets") == 0)
        return PyInt_FromLong(sgl->getNumberOfTickets(team));                // non-virtual
    if (key.compare("startTickets") == 0)
        return PyInt_FromLong(sgl->getDefaultNumberOfTickets(team));         // virtual +0x98
    if (key.compare("ticketLimit") == 0)
        return PyInt_FromLong(sgl->getTicketLimit(team, id));                // the one id user
    if (key.compare("ticketState") == 0)
        return PyInt_FromLong(sgl->getTicketState(team));                    // virtual +0x90
    if (key.compare("ticketChangePerSecond") == 0)
        return PyFloat_FromDouble(sgl->getTicketChangePerSecond(team));
    if (key.compare("defaultTicketLossPerMin") == 0)
        return PyFloat_FromDouble(sgl->getTicketLossPerMin(team));           // virtual +0xB8
    if (key.compare("defaultTicketLossAtEndPerMin") == 0)
        return PyFloat_FromDouble(sgl->getTicketLossAtEndPerMin());          // virtual +0xB0
    if (key.compare("teamName") == 0)
        return PyString_FromString(sgl->getTeamName(team).c_str());          // GameLogic virtual
    if (key.compare("winner") == 0)
        return PyInt_FromLong(
            reinterpret_cast<world::ScoreManager*>(world::scoreManager)->getWinner());
    if (key.compare("victoryType") == 0)
        return PyInt_FromLong(
            reinterpret_cast<world::ScoreManager*>(world::scoreManager)->getVictoryType());

    PyErr_SetString(PyExc_RuntimeError, "bad attribute");
    return 0;
}

// Reads server settings for Python. ranked/hosted also latch validation state,
// and hosted returns 1. Preserve the duplicate globUnlocks comparison and the
// distinction between raw field reads and policy-adjusted getters.
PyObject* ss_getParam(PyObject*, PyObject* args)
{
    std::string key;
    if (py_arg<std::string>(args, 0, key))
        return 0;

    if (key.compare("ticketRatio") == 0)
        return PyInt_FromLong(serverSettings->getTicketRatio());
    if (key.compare("maxPlayers") == 0)
        return PyInt_FromLong(serverSettings->getMaxPlayers());
    if (key.compare("gameMode") == 0)
        return PyString_FromString(game->getCurrentMapGameMode().c_str());
    if (key.compare("mapName") == 0)
        return PyString_FromString(game->getCurrentMapName().c_str());
    if (key.compare("timeLimit") == 0)
        return PyInt_FromLong(serverSettings->m_timeLimit);
    if (key.compare("scoreLimit") == 0)
        return PyInt_FromLong(serverSettings->m_scoreLimit);
    if (key.compare("autoBalance") == 0)
        return PyInt_FromLong(serverSettings->m_autoBalanceTeam != 0);
    if (key.compare("teamRatioPct") == 0)
        return PyFloat_FromDouble(serverSettings->getTeamRatioPercent());
    if (key.compare("tkpEnabled") == 0)
        return PyInt_FromLong(serverSettings->m_tkPunishEnabled != 0);
    if (key.compare("tkpNeeded") == 0)
        return PyInt_FromLong(serverSettings->m_tkNumPunishToKick);
    if (key.compare("tkpDefault") == 0)
        return PyInt_FromLong(serverSettings->m_tkPunishDefault != 0);
    if (key.compare("globRank") == 0)
        return PyInt_FromLong(serverSettings->getUseGlobalRank() != 0);
    if (key.compare("globUnlocks") == 0
        || key.compare("globUnlocks") == 0)              // the shipped double compare - see above
        return PyInt_FromLong(serverSettings->getUseGlobalUnlocks() != 0);
    if (key.compare("noVehicles") == 0)
        return PyInt_FromLong(serverSettings->getNoVehicles() != 0);
    if (key.compare("ranked") == 0)
    {
        const bool ranked = serverSettings->getRanked();
        IGameServer* gameServer = 0;
        if (game)
            gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
        gameServer->setPythonValid(0, true);             // no null check, as shipped
        return PyInt_FromLong(ranked != 0);
    }
    if (key.compare("hosted") == 0)
    {
        IGameServer* gameServer = 0;
        if (game)
            gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
        gameServer->setPythonValid(1, true);             // likewise
        return PyInt_FromLong(1);                        // hardcoded - a server IS hosted
    }

    PyErr_SetString(PyExc_RuntimeError, "unknown serversetting");
    return 0;
}

// sgl_getIsAIGame - ELF 0x81550B0, read in full. the bare ai-singleton
// pointer test - NOT isInitialized(), just "was an AIDLL loaded at all". Permanently 0 on this
// no-bot server (g_aiInstance stays null by mission mandate), which is exactly the answer the
// conquest scripts should get.
PyObject* sgl_getIsAIGame(PyObject*, PyObject*)
{
    return PyInt_FromLong(ai::g_aiInstance != 0);        // ds:0x8AC37F8
}

// sh_setEnableCommander - ELF 0x8155FB0, read in full. one int argument
// posted as ECHud event 63 - the commander-enable toggle rides the HUD event channel.
PyObject* sh_setEnableCommander(PyObject*, PyObject* args)
{
    int enabled = 0;
    if (py_arg<int>(args, 0, enabled))
        return 0;
    eventManager->postEvent(ECHud, 63, &enabled, 0.0f);
    Py_INCREF(Py_None);
    return Py_None;
}

} // namespace python

// Dispatches object collisions when other exists, otherwise terrain/water collisions.
// The face-info arguments are not forwarded.
void ServerGameLogic::handleCollision(world::IObject* other, world::IObject* self,
                                      const Vec3& negatedFirst, const Vec3& second, const Vec3& third,
                                      int materialId, int firstInt,
                                      world::CollisionFaceInfo* /*firstFace*/,
                                      world::CollisionFaceInfo* /*secondFace*/)
{
    if (other)
        world::GameLogicCollision::serverHandleCollisionObjectVsObject(other, self, negatedFirst, second,
                                                                        third, materialId, firstInt);
    else
        world::GameLogicCollision::serverHandleCollisionLandOrWater(0, self, negatedFirst, second, third,
                                                                     materialId, firstInt);
}

// Threshold for notifying occupied seats of accumulated damage.
void  ServerGameLogic::setHitIndicationThreshold(float v) { m_hitIndicationThreshold = v; }
float ServerGameLogic::getHitIndicationThreshold()        { return m_hitIndicationThreshold; }

//  / 0x8105B80 and PE 0x45BD10 / 0x45BD20: one float store/load,
// no clamp or AI/DLL dispatch. Field is ELF +0x208 / PE +0x2F0; constructor seeds 8.0f.
void ServerGameLogic::setDefaultTimeToNextAIWave(float value)
{
    m_defaultTimeToNextAIWave = value;
}

float ServerGameLogic::getDefaultTimeToNextAIWave()
{
    return m_defaultTimeToNextAIWave;
}

// Applies damage of at least 0.1 to the ancestor that owns armor. Missing attribution
// can come from the last-hit record or source control object. Team-damage thresholds
// produce script and network notifications. Soldier/vehicle paths require their control interfaces.
// The result is the original team-hit flag, not a general damage-success result.
bool ServerGameLogic::giveDamage(world::IObject* object, float damage, world::IPlayer* player,
                                 std::list<WeakPtr<world::IPlayer> >* assistList,
                                 world::IObject* sourceObject, int team, world::IObject* weapon,
                                 world::IObject* vehicle, Vec3 position, Vec3 direction, int material,
                                 bool a13, bool canTK, bool noFriendlyFireMod)
{
    if (!object)
        return false;
    if (m_gameStatus != 1)                                                            // +0x34
        return false;

    world::IArmorGLComp* armor = 0;
    for (; object; object = object->getParent())
    {
        armor = static_cast<world::IArmorGLComp*>(
            world::object_detail::LayoutAccess::armorComponent(object));             // +0x3C
        if (armor)
            break;
    }
    if (!armor)
        return false;

    bool teamHit = false;
    if (!(damage >= 0.1f))
        return false;

    world::IPlayerControlObject* sourcePco = 0;
    if (sourceObject)
    {
        sourcePco = static_cast<world::IPlayerControlObject*>(
            sourceObject->queryInterface(world::IID_IPlayerControlObject));
        if (sourcePco)
            team = sourcePco->getTeam();                                              // +0x98
    }

    bool vehicleKill = false;
    bool resolved = false;            // the ELF's `goto LABEL_23` - the record is already complete
    bool viaVehicleOnly = false;      // the ELF's `goto LABEL_96` - skip the source/last-hit vehicle fill
    if (player && weapon)
    {
        if (vehicle)
        {
            if (world::IObject* root = world::getRootParent(object))
            {
                world::IPlayerControlObject* rootPco = 0;
                for (world::IObject* o = root; o; o = o->getParent())
                {
                    rootPco = static_cast<world::IPlayerControlObject*>(
                        o->queryInterface(world::IID_IPlayerControlObject));
                    if (rootPco)
                        break;
                }
                if (rootPco && player && rootPco->getTeam())                          // +0x98
                    teamHit = true;
            }
            resolved = true;
        }
    }
    else if (vehicle)
        viaVehicleOnly = true;
    if (!resolved)
    {
        if (!viaVehicleOnly)
        {
            if (sourcePco)
            {
                vehicle = sourceObject;
                if (!player)
                    player = sourcePco->getOccupingPlayer();                          // +0x10
            }
            else
                vehicle = armor->getLastHitVehicle();                                 // +0xE8
        }
        // LABEL_96
        if (!player)
            player = armor->getLastHitPlayer();                                       // +0xD0
        bool complete = false;
        if (vehicle)
        {
            if (weapon)
                complete = true;
            else
                vehicleKill = true;
        }
        if (!complete && !weapon)
            weapon = armor->getLastHitWeapon();                                       // +0xE0
    }

    // LABEL_23
    if (vehicle && !weapon)
        vehicleKill = true;
    const bool lastDamageType = armor->getLastDamageType() == 1;                      // +0xF0
    damage = world::GameLogicDamage::calcDamage(object, damage, team, false, noFriendlyFireMod);
    world::GameLogicDamage::serverGiveDamage(object, damage, player, assistList, sourceObject, team,
                                             weapon, vehicle, position, direction, material,
                                             lastDamageType, a13, canTK, vehicleKill);

    // the team-damage bookkeeping
    world::IObject* armorObject = armor->getObject();                                 // +0x20
    if (armorObject && armorObject->getTemplate()->getClassID() == world::CID_SoldierTemplate)
    {
        world::IPlayerControlObject* pco = 0;
        for (world::IObject* o = armorObject; o; o = o->getParent())
        {
            pco = static_cast<world::IPlayerControlObject*>(
                o->queryInterface(world::IID_IPlayerControlObject));
            if (pco)
                break;
        }
        world::IPlayer* victim = pco->getOccupingPlayer();                            // unchecked pco
        std::list<WeakPtr<world::IPlayer> >* teamDamage = armor->getTeamDamagePlayers();   // +0xFC
        if (victim && !armor->isDestroyed())                                          // +0x60
        {
            std::list<WeakPtr<world::IPlayer> >::iterator it = teamDamage->begin();
            for (; it != teamDamage->end(); ++it)
            {
                world::IPlayer* p = it->get();
                if (p && p == player)
                    break;
            }
            if (it == teamDamage->end())
                return teamHit;
            if (player->getTeam() == victim->getTeam())
            {
                {
                    python::PyRef args = python::buildArgs(player, object);
                    python::pythonHost()->handleEvent(static_cast<ScriptEvent>(32), args.get());
                }
                int payload[3] = { 0, victim->getId(), player->getId() };
                PostRemoteEvent* toAttacker = new PostRemoteEvent(static_cast<EventCategory>(0), 11,
                                                                  player->getId(), payload, 12, 0.0f);
                game->sendGameEventIfPlaying(player->getId(), toAttacker);             // +0x88
                PostRemoteEvent* toVictim = new PostRemoteEvent(static_cast<EventCategory>(0), 11,
                                                                victim->getId(), payload, 12, 0.0f);
                game->sendGameEventIfPlaying(victim->getId(), toVictim);
                teamDamage->erase(it);
                std::list<world::DamageFromPlayer>* ledger = armor->getDamageFromPlayers();   // +0xF4
                for (std::list<world::DamageFromPlayer>::iterator r = ledger->begin(); r != ledger->end();)
                {
                    std::list<world::DamageFromPlayer>::iterator cur = r++;
                    if (cur->player.get() == player)
                        ledger->erase(cur);
                }
            }
        }
    }
    else
    {
        std::list<world::DamageFromPlayer>* ledger = armor->getVehicleDamageFromPlayers();   // +0xF8
        if (!ledger->empty())
        {
            world::IObject* obj = armor->getObject();
            world::IPlayerControlObject* pco = 0;
            for (world::IObject* o = obj; o; o = o->getParent())
            {
                pco = static_cast<world::IPlayerControlObject*>(
                    o->queryInterface(world::IID_IPlayerControlObject));
                if (pco)
                    break;
            }
            if (pco)
            {
                bool found = false;
                int victimId = -1;
                int victimTeam = -1;
                world::IObject* root = world::getRootParent(armor->getObject());
                world::IPlayerControlObject* rootPco = 0;
                for (world::IObject* o = root; o; o = o->getParent())
                {
                    rootPco = static_cast<world::IPlayerControlObject*>(
                        o->queryInterface(world::IID_IPlayerControlObject));
                    if (rootPco)
                        break;
                }
                std::map<int, world::IPlayerControlObject*>* pcos = rootPco->getPcos();   // unchecked
                for (std::map<int, world::IPlayerControlObject*>::iterator seat = pcos->begin();
                     seat != pcos->end(); ++seat)
                {
                    world::IPlayer* occupant = seat->second->getOccupingPlayer();
                    if (occupant)
                    {
                        found = true;
                        victimId = occupant->getId();
                        victimTeam = occupant->getTeam();
                        break;
                    }
                }
                if (player)
                {
                    const bool sameTeam = found ? victimTeam == player->getTeam()
                                                : rootPco->getDefaultTeam() == player->getTeam();   // +0x94
                    if (sameTeam)
                    {
                        float sum = 0.0f;
                        for (std::list<world::DamageFromPlayer>::iterator r = ledger->begin();
                             r != ledger->end(); ++r)
                        {
                            if (player == r->player.get())
                                sum = sum + r->amount;
                        }
                        const float limit = reinterpret_cast<world::ScoreManager*>(world::scoreManager)
                                                ->getTeamVehicleDamageLimit();                // +0x4C
                        if (sum >= armor->getMaxHitPoints() * limit / 100.0f)                // +0x84
                        {
                            {
                                python::PyRef args = python::buildArgs(player, object);
                                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(32),
                                                                  args.get());
                            }
                            int payload[3] = { 1, victimId, player->getId() };
                            PostRemoteEvent* toAttacker = new PostRemoteEvent(
                                static_cast<EventCategory>(0), 11, player->getId(), payload, 12, 0.0f);
                            game->sendGameEventIfPlaying(player->getId(), toAttacker);
                            PostRemoteEvent* toVictim = new PostRemoteEvent(
                                static_cast<EventCategory>(0), 11, victimId, payload, 12, 0.0f);
                            game->sendGameEventIfPlaying(victimId, toVictim);
                            for (std::list<world::DamageFromPlayer>::iterator r = ledger->begin();
                                 r != ledger->end();)
                            {
                                std::list<world::DamageFromPlayer>::iterator cur = r++;
                                if (player == cur->player.get())
                                    ledger->erase(cur);
                            }
                        }
                    }
                }
            }
        }
    }
    return teamHit;
}

// Handles server player/object lifecycle and delegates other events to GameLogic.
// Commander-election events consume their heap payload; team-change events remove
// old membership before the player's team store. Spotted events execute then release their event object.
void ServerGameLogic::handleEvent(EventCategory category, unsigned int id,
                                  EventNode* node, void* data)
{
    if (category == ECGameLogic)
    {
        switch (id)
        {
            case 7:
            {
                int* const payload = static_cast<int*>(data);
                static_cast<ISquadManager*>(squadManager)->declineCommander(payload[0], -1);
                delete[] payload;
                return;
            }

            case 8:
            {
                int* const payload = static_cast<int*>(data);
                world::IPlayer* const player = world::playerManager->getPlayerFromId(payload[0]);
                if (player)
                {
                    if (player->getSquadId())
                    {
                        const int playerId = player->getId();
                        const int squadId = player->getSquadId();
                        static_cast<ISquadManager*>(squadManager)->removeFromSquad(
                            payload[1] == 1 ? 2 : 1, squadId, playerId, false);
                    }

                    if (player->getIsCommander())
                        static_cast<ISquadManager*>(squadManager)->resignCommander(
                            payload[1] == 1 ? 2 : 1);

                    player->setSquadInviteMask(0);
                    static_cast<ISquadManager*>(squadManager)->removeCommanderApplicant(
                        1, player->getId());
                    static_cast<ISquadManager*>(squadManager)->removeCommanderApplicant(
                        2, player->getId());
                }
                return;
            }

            case 15:
            {
                GameEvent* const event = static_cast<GameEvent*>(data);
                event->executeServer(-1);
                event->release();
                return;
            }

            default:
                break;
        }
    }

    if (category == ECGame)                          // category 1 - ServerGameLogic's own arm
    {
        switch (id)
        {
            case 1: _destroyObject(static_cast<world::IObject*>(data)); break;
            case 2: _destroyPlayer(static_cast<world::IPlayer*>(data)); break;
            case 3: _createObject(static_cast<world::IObject*>(data)); break;
            case 4: _createPlayer(static_cast<world::IPlayer*>(data)); break;    // default team 2
            case 5: spawnPlayer(static_cast<world::IPlayer*>(data), 0); break;
            case 6: break;                            // player spawned - the ELF's empty case
            default:
                GameLogic::handleEvent(category, id, node, data);
                break;
        }
        return;
    }

    GameLogic::handleEvent(category, id, node, data);
}

// Initializes a newly created player before its creation event is serialized:
// team 2, kit 2, no spawn group, and not alive.
int ServerGameLogic::_createPlayer(world::IPlayer* player)
{
    player->setTeam(2, false);                       // IPlayer +0xE4  THE FIX
    player->setSpawnGroup(0);                        // IPlayer +0x154
    player->setIsAlive(false);                       // IPlayer +0x68
    player->setKit(2);                               // IPlayer +0x16C
    return 1;
}

// . `return 1` - the ELF ignores its argument entirely. The object is already built by
// the time ECGame/3 fires; this hook exists only for symmetry with _createPlayer.
int ServerGameLogic::_createObject(world::IObject* /*object*/)
{
    return 1;
}

// Kills occupants of every seat on the first control object at or above the destroyed
// object. Objects without a control object do nothing. Requires valid seat-map
// entries; killPlayer dispatches virtually through server game logic.
int ServerGameLogic::_destroyObject(world::IObject* object)
{
    world::IPlayerControlObject* const pco = controlObjectAtOrAbove(object);   // qi +0x0C / +0x28
    if (pco)
    {
        std::map<int, world::IPlayerControlObject*>* const pcos = pco->getPcos();   // +0x60
        for (std::map<int, world::IPlayerControlObject*>::iterator it = pcos->begin();
             it != pcos->end(); ++it)
        {
            world::IPlayer* const occupant = it->second->getOccupingPlayer();       // +0x10
            if (occupant)
                killPlayer(occupant, true, false);       // virtual, IGameLogic +0x6C - see above
        }
    }
    return 1;
}

// Removes commander/squad membership and withdraws any squad application.
// Preserve repeated getSquadId reads and the player/squad/team argument order.
int ServerGameLogic::_destroyPlayer(world::IPlayer* player)
{
    if (squadManager)                                    // ds:0x8ACBFA4 - the whole body's gate
    {
        ISquadManager* const squads = static_cast<ISquadManager*>(squadManager);

        if (player->getIsCommander())                    // IPlayer +0x114
            squads->resignCommander(player->getTeam());  // +0x68

        if (player->getSquadId())                        // tested here, read again below
        {
            const int playerId = player->getId();        // +0x54
            const int squadId  = player->getSquadId();   // +0x10C
            const int team     = player->getTeam();      // +0xE8
            squads->removeFromSquad(team, squadId, playerId, false);   // +0x34
        }

        {
            const int playerId = player->getId();
            const int team     = player->getTeam();
            squads->removeCommanderApplicant(team, playerId);          // +0x84
        }
    }
    return 1;
}

// Sends hit indicators when enabled by server settings. Other special-event types are ignored.
// SGE_HitIndicator requires an IPlayer payload; the pointer is checked only for null.
void ServerGameLogic::triggerSpecialGameEvent(SpecialGameEventType type, void* payload)
{
    if (type == SGE_HitIndicator && payload)
    {
        if (serverSettings->m_hitIndicator)                                   // +0x53
            static_cast<world::IPlayer*>(payload)->setHitIndicator(true);     // IPlayer +0x1D8
    }
}

// ServerGameLogic::handleExplosion - ELF 0x8103620, 235 B / PE cross-check. The server virtual
// enters the authoritative traversal, then mirrors projectile collision scoring only when that
// traversal classified an enemy hit. As shipped, the weapon template is unguarded once weapon is
// non-null, and the projectile (not the damaged object) is credited to the shot.
void ServerGameLogic::handleExplosion(Vec3 position, float radius, int material, float damage,
                                      world::IPlayer* player,
                                      world::IArmorGLComp* ignoredArmor, int team,
                                      world::IObject* weapon, world::IObject* vehicle,
                                      float force, world::IObject* sourceProjectile,
                                      world::IObject* affectedObject, float yMod,
                                      bool canTK, bool includeSource)
{
    if (world::GameLogicExplosion::serverHandleExplosion(
            position, radius, material, damage, player, ignoredArmor, team, weapon, vehicle,
            force, sourceProjectile, affectedObject, yMod, canTK, includeSource))
    {
        triggerSpecialGameEvent(SGE_HitIndicator, player);
        if (player && weapon)
        {
            world::PlayerScoreData* const score = player->getScoreData();
            if (score)
            {
                score->addBulletsGivingDamage(weapon->getTemplate()->getId(), 1,
                                              sourceProjectile);
            }
        }
    }
}

bool ServerGameLogic::handleCollisionForProjectile(world::IObject* projectile, world::IObject* hitObject,
                                                   const Vec3& velocity, const Vec3& normal,
                                                   const Vec3& collisionOffset, int materialA,
                                                   int materialB, world::CollisionFaceInfo* faceA,
                                                   world::CollisionFaceInfo* faceB)
{
    if (world::GameLogicCollision::serverHandleCollisionForProjectile(
            projectile, hitObject, velocity, normal, collisionOffset, materialA, materialB,
            faceA, faceB))
    {
        world::IObject* const root = world::getRootParent(projectile);
        if (root)
        {
            world::IProjectileObject* const proj = static_cast<world::IProjectileObject*>(
                root->queryInterface(world::IID_IProjectileObject));
            if (proj)
            {
                world::GenericProjectile* const gp = static_cast<world::GenericProjectile*>(proj);

                // +0x120 IS triggerSpecialGameEvent, not a damage-dealer setter: the ELF passes
                // the literal 1 as the SpecialGameEventType and the projectile's owner as the
                // payload. The event id's meaning is [UNRESOLVED]; the call is not.
                triggerSpecialGameEvent(static_cast<SpecialGameEventType>(1),
                                        gp->getPlayer());                     // +0x120, +0x54

                world::IObject* const weapon = gp->getLauncherWeapon();           // +0x34
                if (gp->getPlayer() && weapon)
                {
                    world::PlayerScoreData* const score =
                        gp->getPlayer()->getScoreData();                          // +0x174
                    if (score)
                    {
                        score->addBulletsGivingDamage(weapon->getTemplate()->getId(), 1,
                                                      projectile);
                    }
                }
            }
        }
    }
    return true;
}

//  / PE 0x454980. The collision solver owns service and bounce/penetration; this
// wrapper emits the hit indicator and credits one damaging projectile to the launching weapon.
bool ServerGameLogic::handleCollisionForReplenisher(bool isHeal, world::IObject* projectile,
                                                     world::IObject* hitObject,
                                                     const Vec3& velocity, const Vec3& normal,
                                                     const Vec3& collisionOffset, int materialA,
                                                     int materialB)
{
    if (!world::GameLogicCollision::serverHandleCollisionForReplenisher(
            isHeal, projectile, hitObject, velocity, normal, collisionOffset, materialA, materialB))
    {
        return false;
    }

    world::IObject* const root = world::getRootParent(projectile);
    if (root)
    {
        world::IProjectileObject* const proj = static_cast<world::IProjectileObject*>(
            root->queryInterface(world::IID_IProjectileObject));
        if (proj)
        {
            world::GenericProjectile* const gp = static_cast<world::GenericProjectile*>(proj);
            triggerSpecialGameEvent(static_cast<SpecialGameEventType>(1), gp->getPlayer());

            world::IObject* const weapon = gp->getLauncherWeapon();
            if (gp->getPlayer() && weapon)
            {
                world::PlayerScoreData* const score = gp->getPlayer()->getScoreData();
                if (score)
                {
                    score->addBulletsGivingDamage(weapon->getTemplate()->getId(), 1,
                                                  projectile);
                }
            }
        }
    }
    return true;
}

//  / PE 0x454A40. Human targets are resolved through the hit object's nearest PCO.
// For an AI owner without such a target, retail asks the AI courier for an object, re-reads that
// object's material, and then applies the deliberately asymmetric team/man-down gate below.
bool ServerGameLogic::handleCollisionForResurrecter(world::IObject* projectile,
                                                     world::IObject* hitObject,
                                                     float resurrectDamage, float restoreHP,
                                                     const Vec3& velocity, const Vec3& normal,
                                                     const Vec3& collisionOffset, int materialA,
                                                     int materialB)
{
    world::IObject* const projectileRoot = world::getRootParent(projectile);
    (void)world::getRootParent(hitObject); // shipped dead call
    if (!projectileRoot)
        return false;

    world::IProjectileObject* const proj = static_cast<world::IProjectileObject*>(
        projectileRoot->queryInterface(world::IID_IProjectileObject));
    if (!proj)
        return false;

    world::GenericProjectile* const gp = static_cast<world::GenericProjectile*>(proj);
    world::IPlayer* const owner = gp->getPlayer();
    if (!owner)
        return false;

    world::IPlayerControlObject* playerControl = 0;
    for (world::IObject* object = hitObject; object; object = object->getParent())
    {
        playerControl = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        if (playerControl)
            break;
    }

    world::ISoldier* soldier = playerControl
        ? static_cast<world::ISoldier*>(playerControl->queryInterface(world::IID_ISoldier))
        : 0;
    world::IObject* effectiveHitObject = hitObject;

    if (!soldier && owner->getIsAIPlayer())
    {
        ai::IAIPlayerCourier* const courier = static_cast<ai::IAIPlayerCourier*>(
            ai::BFSingletonServer::m_iaiPlayerCourier);
        world::IObject* const aiTarget = courier->getResurrectTarget(owner->getId());
        if (!aiTarget)
            return false;

        world::IObject* const aiTargetRoot = world::getRootParent(aiTarget);
        soldier = static_cast<world::ISoldier*>(aiTargetRoot->queryInterface(world::IID_ISoldier));
        if (!soldier)
            return false;

        effectiveHitObject = aiTarget;
        world::IArmorGLComp* const targetArmor = static_cast<world::IArmorGLComp*>(
            world::object_detail::LayoutAccess::armorComponent(aiTarget));
        materialB = targetArmor->getDefaultMaterial();
    }

    if (!soldier)
        return false;

    world::IPlayer* const victim = soldier->getOccupingPlayer();
    if (!victim)
        return false;

    // Preserve the repeated virtual calls and retail's asymmetric predicate exactly: a teammate
    // must be man-down, while an enemy must not be man-down.
    if (victim->getTeam() != owner->getTeam() || !victim->getIsManDown())
    {
        if (victim->getTeam() == owner->getTeam() || victim->getIsManDown())
            return false;
    }

    if (!world::GameLogicCollision::serverHandleCollisionForResurrecter(
            projectile, effectiveHitObject, resurrectDamage, restoreHP, velocity, normal,
            collisionOffset, materialA, materialB))
    {
        return false;
    }

    triggerSpecialGameEvent(static_cast<SpecialGameEventType>(1), gp->getPlayer());
    world::IObject* const weapon = gp->getLauncherWeapon();
    if (gp->getPlayer() && weapon)
    {
        world::PlayerScoreData* const score = gp->getPlayer()->getScoreData();
        if (score)
            score->addBulletsGivingDamage(weapon->getTemplate()->getId(), 1, projectile);
    }
    return true;
}

// Applies server death state, returning false for an already-dead player.
// Credits time spent man-down against the respawn delay, removes leader spawn points and resets devices
// before the base kill. Optional Python notification receives the vehicle captured before dismount.
// Requires valid vehicle and squad services on their unguarded paths.
bool ServerGameLogic::killPlayer(world::IPlayer* player, bool a, bool b)
{
    typedef world::object_detail::LayoutAccess L;

    if (!player->getIsAlive())                                               // +0x6C
        return false;

    const bool wasManDown = player->getIsManDown();                         // +0x100
    const int spawnDelayTicks = static_cast<int>(serverSettings->getSpawnTime() * 30.0f);   // truncated

    if (player->getVehicle())                                               // +0x48
    {
        world::IObject* const vehicle = player->getVehicle();
        if (L::objectTemplate(vehicle)->getClassID() == world::CID_ZiplineContainerTemplate)
        {
            world::IPlayerControlObject* pco = 0;
            for (world::IObject* o = player->getVehicle(); o; o = L::parent(o))   // +0x28
            {
                pco = static_cast<world::IPlayerControlObject*>(
                    o->queryInterface(world::IID_IPlayerControlObject));       // +0x0C
                if (pco)
                    break;
            }
            pco->exit();                                                     // +0x20 - unchecked
        }
    }

    unsigned int spawnAtTick;
    if (wasManDown)
    {
        const unsigned int downFor = WorldPref::getTickCount() - player->getManDownAtTick();   // +0x104
        player->setIsManDown(false);                                         // +0xFC
        const unsigned int now = WorldPref::getTickCount();
        int credit = spawnDelayTicks - static_cast<int>(downFor);
        if (credit < 0)
            credit = 0;                                                      // the cmovl floor
        spawnAtTick = now + static_cast<unsigned int>(credit);
    }
    else
    {
        spawnAtTick = WorldPref::getTickCount() + static_cast<unsigned int>(spawnDelayTicks);
    }
    player->setSpawnAtTick(spawnAtTick);                                     // +0xF0

    world::IObject* const vehicle = player->getVehicle();                    // captured for 5 and 11
    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())              // ds:0x8AC37F8 / +0x1C
    {
        AiPlayerVehiclePayload payload;
        payload.team = player->getTeam();                                  // +0xE8
        payload.playerId = player->getId();                                // +0x54
        payload.aiObject = L::aiObject(vehicle);                           // vehicle +0x34, unchecked
        eventManager->postEvent(ECAI, 0, &payload, 0.0f);                    // +0x14
    }

    if (player->getIsSquadLeader())                                          // +0x13C
    {
        const int squadId = player->getSquadId();                            // +0x10C - read first
        const int team    = player->getTeam();                               // +0xE8
        ISquadInfo* const squad =
            static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squadId);   // ds:0x8ACBFA4 +0x28 - unguarded
        if (squad)
            squad->removeSquadLeaderSpawnPoint(player);                      // ISquadInfo +0x24
    }

    player->setNightVision(false, true);                                     // +0x2DC
    player->rechargeNightVision();                                           // +0x2E0
    player->setIsGasMaskOn(false, true);                                     // +0x2C4
    player->disableFlashBangEffect();                                        // +0x2EC
    player->resetOnDeath();                                                  // +0x2F8

    const bool result = GameLogic::killPlayer(player, a, b);                 // the base, non-virtual
    player->setGaveAmmoAtTick(0, true);                                      // +0x230

    {
        std::string gameMode;                                                // built and never read
        if (game)
            gameMode = game->getCurrentMapGameMode();                        // IGame +0xF4
    }

    if (a)
    {
        python::PyRef args = python::buildArgs(player, vehicle);             // <IPlayer*, IObject*>
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(3), args.get());
    }
    return result;
}

// Finds the nearest loose kit within radius, excluding kits parented to soldiers.
// Uses a squared-distance sentinel of 100000. An object's null template is allowed;
// a parent's template is assumed valid.
world::IObject* findKitObject(const Vec3& position, float radius)
{
    typedef world::object_detail::LayoutAccess L;

    std::vector<world::IObject*>& objects = world::objectManager->getObjectTempVector();   // +0xA8
    float bestDistanceSq = 100000.0f;
    world::IObject* best = 0;
    world::ObjectFlagPredicator predicator(0x200u);
    world::objectManager->getObjectsFromGrid(position, radius, objects, &predicator);   // the radius overload

    for (std::vector<world::IObject*>::iterator it = objects.begin(); it != objects.end(); ++it)
    {
        world::IObject* const object = *it;
        world::IObject* const parent = L::parent(object);                    // +0x28
        if (parent && L::objectTemplate(parent)->getClassID() == world::CID_SoldierTemplate)
            continue;
        const world::IObjectTemplate* const templ = L::objectTemplate(object);   // +0x14
        if (!templ || templ->getClassID() != world::CID_KitTemplate)
            continue;
        const Vec3& p = object->getAbsolutePosition();                       // +0x6C
        const float dz = p.z - position.z;
        const float dx = p.x - position.x;
        const float dy = p.y - position.y;
        const float distanceSq = dz * dz + dy * dy + dx * dx;
        if (bestDistanceSq > distanceSq)
        {
            bestDistanceSq = distanceSq;
            best = object;
        }
    }
    return best;
}

// Returns whether a nearby kit exists, even if its type prevents pickup. Requires
// the soldier's item-container interface. Successful pickup updates kit time.
bool ServerGameLogic::pickUpClosestKit(world::IPlayer* player, world::IObject* soldier, float radius)
{
    world::IObject* const kit = findKitObject(soldier->getAbsolutePosition(), radius);   // +0x6C
    world::IItemObject* const item =
        kit ? static_cast<world::IItemObject*>(kit->queryInterface(world::IID_IItemObject)) : 0;
    if (!item)
        return false;

    world::IItemContainerObject* const container = static_cast<world::IItemContainerObject*>(
        soldier->queryInterface(world::IID_IItemContainerObject));           // +0x0C
    world::IItemObject* const kitItem =
        static_cast<world::IItemObject*>(kit->queryInterface(world::IID_IItemObject));   // asked again
    if (container->getItemContainerType() == kitItem->getItemType())        // +0x20 / +0x24
    {
        handlePickup(kit, player, soldier, true);                            // virtual +0xEC
        player->setKitTimeStamp(WorldPref::getWorldTime());                  // +0x1FC
    }
    return true;
}

// Creates and broadcasts the selected team's kit, then creates its pickup.
// Requires a registered KitTemplate and successful object creation; the existing
// failure paths do not guard every null result.
void ServerGameLogic::getNewKit(world::IPlayer* player)
{
    typedef world::object_detail::LayoutAccess L;

    const int key = (player->getTeam() << 8) + player->getKit();             // +0xE8 / +0x160
    std::map<int, std::string>::iterator it = m_kits.find(key);              // +0x1C
    if (it == m_kits.end())
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 3757, __FILE__)
            << "TBM: Unable to create the requested kit!";
        return;
    }

    world::IObjectTemplate* const templ = world::objectTemplateManager->getTemplate(it->second);   // +0x24
    world::KitTemplate* const kitTemplate =
        static_cast<world::KitTemplate*>(templ->queryImplementation(world::CID_KitTemplate));   // templ unchecked
    if (!kitTemplate)
    {
        Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 3764, __FILE__)
            << "TBM: Unable to create the requested kit! " << it->second;
        return;
    }

    Vec3 zeroA;
    Vec3 zeroB;
    zeroA.x = zeroA.y = zeroA.z = 0.0f;
    zeroB.x = zeroB.y = zeroB.z = 0.0f;
    world::IObject* const kit = game->createObjectOnAllClients(kitTemplate, zeroB, zeroA, true, 0, 0);   // IGame +0x78
    if (kit)
    {
        handlePickup(kit, player, player->getVehicle(), true);              // virtual +0xEC
        return;
    }
    // kit is NULL here - see the block above. As shipped.
    if (L::objectTemplate(kit)->getClassID() == world::CID_KitTemplate)
        return;
    Debug(DEBUGTYPE_6, "Game/GameLogic/ServerGameLogic.cpp", 3774, __FILE__)
        << "TBM: Unable to create the requested kit!";
}

// Validates through GameLogic::heal; applyIt == true only queries eligibility.
// Otherwise heals the ancestor owning armor and credits eligible friendly healing
// or repair. Returns false when no hit points change. Score events name the armor owner.
bool ServerGameLogic::heal(world::IObject* object, float amount, world::IPlayer* player,
                           int materialA, int materialB, float* outAmount, bool applyIt,
                           bool allowOwnVehicle)
{
    typedef world::object_detail::LayoutAccess L;

    float healAmount = 0.0f;
    const bool ok = GameLogic::heal(object, amount, player, materialA, materialB, &healAmount,
                                    applyIt, allowOwnVehicle);
    if (applyIt || !ok)
        return ok;

    world::getRootParent(object);                                            // result discarded

    world::IArmorGLComp* armor = 0;
    for (world::IObject* o = object; o; o = L::parent(o))                    // +0x3C / +0x28
    {
        if (L::armorComponent(o))
        {
            armor = static_cast<world::IArmorGLComp*>(L::armorComponent(o));
            object = o;                                                      // rebinds `object`
            break;
        }
    }

    world::IPlayerControlObject* pco = 0;
    for (world::IObject* o = object; o; o = L::parent(o))
    {
        pco = static_cast<world::IPlayerControlObject*>(
            o->queryInterface(world::IID_IPlayerControlObject));             // +0x0C
        if (pco)
            break;
    }
    world::IPlayer* const occupant = pco ? pco->getOccupingPlayer() : 0;     // +0x10

    if (player && occupant && occupant->getIsAIPlayer())                     // +0x64
    {
        int occupantId = occupant->getId();                                  // +0x54
        eventManager->postEvent(ECAI, 23, &occupantId, 0.0f);               // +0x14
    }

    const float before = armor->getHitPoints();                              // +0x3C - armor unchecked
    armor->heal(healAmount);                                                 // +0x44
    const float healed  = armor->getHitPoints() - before;
    const float percent = healed * 100.0f / armor->getMaxHitPoints();       // +0x84

    if (!(std::fabs(healed) > 0.0f))
        return false;
    if (!player)
        return true;

    if (L::objectTemplate(object)->getClassID() == world::CID_SoldierTemplate)
    {
        if (occupant == player)
            return true;
        if (player->getTeam() != pco->getTeam())                             // +0xE8 / +0x98 - pco unchecked
            return true;
        if (occupant)
            player->setHealRepairedAtTick(WorldPref::getTickCount(), true, false);   // +0x228
        player->getScoreData()->addToHealScoreAmount(percent, object);       // +0x174
        if (!player->getScoreData()->receiveHealScore(
                static_cast<world::IScoreManager*>(world::scoreManager)->getHealScoreLimit()))   // +0x2C
            return true;
        python::PyRef args = python::buildArgs(player, object);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(30), args.get());
        return true;
    }

    const int pcoTeam = pco ? pco->getTeam() : 0;                            // +0x98
    if (occupant == player || player->getTeam() != pcoTeam)
        return true;
    if (occupant)
        player->setHealRepairedAtTick(WorldPref::getTickCount(), true, true);   // +0x228
    player->getScoreData()->addToRepairScoreAmount(percent, object);
    if (!player->getScoreData()->receiveRepairScore(
            static_cast<world::IScoreManager*>(world::scoreManager)->getRepairScoreLimit()))   // +0x34
        return true;
    python::PyRef args = python::buildArgs(player, object);
    python::pythonHost()->handleEvent(static_cast<ScriptEvent>(29), args.get());
    return true;
}

// Applies base resupply behavior, awards friendly ammo credit, and marks affected
// occupants as resupplied. A refusal clears the giver's ammo tick; queries return
// without scoring. Soldier occupant handling requires its current vehicle.
bool ServerGameLogic::replenishAmmo(world::IObject* object, float amount, world::IPlayer* player,
                                    int materialA, int materialB, float* outAmount, bool queryOnly)
{
    typedef world::object_detail::LayoutAccess L;

    float given = 0.0f;
    const bool ok = GameLogic::replenishAmmo(object, amount, player, materialA, materialB, &given, queryOnly);
    if (!ok)
    {
        if (player)
            player->setGaveAmmoAtTick(0, true);                              // +0x230
        return ok;
    }
    if (queryOnly)
        return ok;
    if (!player)
        return true;

    world::IPlayerControlObject* pco = 0;
    for (world::IObject* o = object; o; o = L::parent(o))
    {
        pco = static_cast<world::IPlayerControlObject*>(
            o->queryInterface(world::IID_IPlayerControlObject));             // +0x0C
        if (pco)
            break;
    }
    world::IPlayer* const occupant = pco ? pco->getOccupingPlayer() : 0;     // +0x10

    if (occupant != player && occupant && occupant->getTeam() == player->getTeam())   // +0xE8
    {
        player->setGaveAmmoAtTick(WorldPref::getTickCount(), true);          // +0x230
        player->getScoreData()->addToAmmoScoreAmount(given, object);         // +0x174
        if (player->getScoreData()->receiveAmmoScore(
                static_cast<world::IScoreManager*>(world::scoreManager)->getAmmoScoreLimit()))   // +0x3C
        {
            python::PyRef args = python::buildArgs(player, object);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(31), args.get());
        }
    }

    if (pco)
    {
        if (occupant && L::objectTemplate(occupant->getVehicle())->getClassID() == world::CID_SoldierTemplate)   // +0x48 / +0x14
        {
            occupant->setGaveAmmoAtTick(WorldPref::getTickCount(), false);
        }
        else
        {
            std::map<int, world::IPlayerControlObject*>* const pcos = pco->getPcos();   // +0x60
            for (std::map<int, world::IPlayerControlObject*>::iterator it = pcos->begin();
                 it != pcos->end(); ++it)
            {
                world::IPlayer* const seated = it->second->getOccupingPlayer();          // +0x10
                if (!seated)
                    continue;
                world::IObject* const seatObject = static_cast<world::IObject*>(
                    it->second->queryInterface(world::IID_IObject));
                if (!seatObject)
                    continue;
                world::IVehicleHudComp* const hud =
                    static_cast<world::IVehicleHudComp*>(L::vehicleHudComponent(seatObject));   // +0x54
                if (!hud)
                    continue;
                if (!hud->getHudInfo()->useChildHud)                                     // +0x64 -> +29
                    seated->setGaveAmmoAtTick(WorldPref::getTickCount(), false);
            }
        }
    }
    return true;
}

namespace python {

// buildArgs<IPlayer*, IPlayer*> - emitted in ServerGameLogic's TU for resurrect's ScriptEvent 2.
// ..0x8105982 (178 B), PE 0x45A8F0. Like every recovered buildArgs arm, it does no
// conversion or allocation until the Python host is initialized; PyTuple_SetItem steals the two
// conversion references, while PyRef takes and retains its own reference before the local decref.
PyObject* to_py(world::IPlayer* player);

PyRef buildArgs(world::IPlayer* const& a0, world::IPlayer* const& a1)
{
    if (!pythonHost()->isInitialized())
        return PyRef();

    PyObject* tuple = PyTuple_New(2);
    PyTuple_SetItem(tuple, 0, to_py(a0));
    PyTuple_SetItem(tuple, 1, to_py(a1));

    PyRef result(tuple);
    Py_DECREF(tuple);
    return result;
}

} // namespace python

// Damages a living target with an empty assist list, or revives a man-down target
// and applies its HP floor and notifications. Living targets require a player;
// weapon lookup requires a PCO. Preserve repeated lookups and the remote message's
// full 36-byte copy, including bytes beyond its initialized string prefix.
bool ServerGameLogic::resurrect(world::IProjectileObject* projectile, world::IObject* object,
                                float amount, float resurrectDamage, float restoreHitPoints,
                                world::IPlayer* player, int materialA, int materialB)
{
    typedef world::object_detail::LayoutAccess L;

    if (!object || m_gameStatus != GameStatus_Playing)
        return false;

    world::IArmorGLComp* armor = 0;
    for (world::IObject* current = object; current; current = L::parent(current))
    {
        world::IObjectComponent* const component = L::armorComponent(current);
        if (component)
        {
            armor = static_cast<world::IArmorGLComp*>(component);
            break;
        }
    }
    if (!armor || armor->getStatus() == 2)
        return false;

    world::getRootParent(object);                                      // result discarded in retail

    const world::IObjectTemplate* const objectTemplate = L::objectTemplate(object);
    if (!const_cast<world::IObjectTemplate*>(objectTemplate)->queryImplementation(
            world::CID_SimpleObjectTemplate))
        return false;

    const float damageModifier =
        static_cast<world::MaterialManager*>(world::materialManager)->getDamageMod(
            static_cast<unsigned int>(materialA), static_cast<unsigned int>(materialB));
    if (damageModifier <= 0.0f)                                       // unordered/NaN continues
        return false;

    world::IPlayerControlObject* controlObject = 0;
    for (world::IObject* current = object; current; current = L::parent(current))
    {
        controlObject = static_cast<world::IPlayerControlObject*>(
            current->queryInterface(world::IID_IPlayerControlObject));
        if (controlObject)
            break;
    }
    world::IPlayer* const occupant = controlObject ? controlObject->getOccupingPlayer() : 0;

    if (armor->getStatus() != 1)
    {
        world::IObject* activeWeapon = 0;
        if (player->getVehicle())                                     // player deliberately unchecked
        {
            world::IPlayerControlObject* playerControlObject = 0;
            for (world::IObject* current = player->getVehicle(); current;
                 current = L::parent(current))                         // repeated getVehicle is shipped
            {
                playerControlObject = static_cast<world::IPlayerControlObject*>(
                    current->queryInterface(world::IID_IPlayerControlObject));
                if (playerControlObject)
                    break;
            }
            activeWeapon = playerControlObject->getActiveWeapon(0);    // no null guard in retail
        }

        std::list<WeakPtr<world::IPlayer> > assistPlayers;
        Vec3 zero = { 0.0f, 0.0f, 0.0f };
        Vec3 forward = { 0.0f, 0.0f, 1.0f };
        const int team = player->getTeam();
        world::IObject* const projectileObject = static_cast<world::IObject*>(
            static_cast<world::GenericProjectile*>(projectile)->queryInterface(world::IID_IObject));

        const float adjustedAmount = damageModifier * amount;
        const float hitPoints = armor->getHitPoints();
        const float damage = adjustedAmount < hitPoints ? adjustedAmount : hitPoints;
        giveDamage(object, damage, player, &assistPlayers, projectileObject, team,
                   activeWeapon, 0, zero, forward, materialA, true, true, false);
        return true;
    }

    armor->revive(damageModifier * resurrectDamage);
    if (armor->getStatus() != 0)
        return false;

    if (restoreHitPoints > armor->getHitPoints())
        armor->setHitPoints(restoreHitPoints);

    if (player && L::objectTemplate(object)->getClassID() == world::CID_SoldierTemplate)
    {
        char voiceMessage[36];
        std::strcpy(voiceMessage, "get_up");                           // leaves the stack tail untouched

        if (player->getIsAIPlayer())
        {
            int messageId = 0;
            const std::string emptyFilter;
            const std::string emptyVoice;
            const std::string message(voiceMessage);
            if (static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
                    &messageId, 0, 0, message, emptyVoice, emptyFilter))
            {
                EventNode* const node = createRadioMessageReceivedEvent(
                    player->getId(), static_cast<unsigned char>(messageId),
                    static_cast<unsigned char>(player->getId()),
                    static_cast<unsigned char>(player->getTeam()), 0, TCLocal, 3);
                eventManager->postEvent(ECNetwork, node, 0.0f);
            }
        }
        else if (player == world::playerManager->getLocalHumanPlayer())
        {
            game->radioMessage(TCLocal, std::string(voiceMessage), player, 0, true, false);
        }
        else
        {
            PostRemoteEvent* const event = new PostRemoteEvent(
                ECHud, 51, false, voiceMessage, 36, 0.0f);
            game->sendGameEventIfPlaying(static_cast<unsigned int>(player->getId()), event);
        }

        if (occupant)
        {
            occupant->setIsManDown(false);
            occupant->setSpawnAtTick(0);
            python::PyRef args = python::buildArgs(occupant, player);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(2), args.get());
        }
    }

    return true;
}

} // namespace hfe
} // namespace dice
