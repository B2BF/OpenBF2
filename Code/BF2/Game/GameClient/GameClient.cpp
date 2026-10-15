// Client-only game loop and network session. Unimplemented calls retain named
// [FRONTIER] diagnostics and stop execution. [TEMP] [CHOSEN, not recovered]
// [RETIRE-WHEN: game-client-bodies] Replace remaining GAMECLIENT_FRONTIER sites
// with their implementations.
#include "GameClient.h"
#if !DEDICATED

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "../../Core/Ids.h"
#include "../../Core/ClassManager.h"
#include "../../Core/EventManager.h"
#include "../../Core/SettingsRepostitory.h"
#include "../../Core/Debug.h"
#include "../../Core/StringUtil.h"                   // formatString (PE 4310D0)
#include "../../Core/LoadStat.h"
#include "../../Core/System.h"                       // g_system->getSimulationRate
#include "../IGameLogic.h"
#include "../Main/BF2Engine.h"                       // bf2Engine->demo (isPlayingDemo)
#include "../Main/Demo.h"
#include "../Main/BF2ClientSeams.h"                  // hudManager, g_distributedLightRenderer
#include "../Main/StatsMonitor.h"
#include "../Utility/BuildNrUtil.h"
#include "../Common/ClientConnection.h"
#include "../Common/GhostManager.h"            // ECNetwork 17: removeGhostable
#include "../Common/ClientVoteManager.h"             // update's per-frame kick-vote ticker (599290)
#include "../Common/GameEventManager.h"
#include "../Common/StringManager.h"
#include "../Common/GameEvent.h"
#include "../GameEvents/ChallengeEvent.h"
#include "../GameEvents/ChallengeResponseEvent.h"    // handleChallenge builds one (or two) of these
#include "../GameEvents/ConnectionTypeEvent.h"
#include "../GameEvents/RadioMessageEvent.h"
#include "../MatchMaking/Client/CdKeyStore.h"        // the registry key store (59F4E0 / 59F580 / 59F640 / 59F6A0)
#include "../MatchMaking/Server/MatchMakingServer.h" // gamespy::g_gameID - the ds:8E66BC handleChallenge reads
#include "../GameServer/ServerInfo.h"
#include "../GameLogic/ActionBuffer.h"           // connectionWatchdog: the buffer size gate
#include "../Common/PlayerActionManager.h"        // queueLocalAction: the send array
#include "../Common/PlayerAction.h"               // sampleLocalInput: the set/get round trip
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"  // preHandlePlayerInput
#include "../GameLogic/SquadManager/ISquadManager.h"
#include "../Objects/IPlayer.h"
#include "../Objects/Base/FreeCamera.h"   // enterVehicle sets the camera's trace object
#include "../GameLogic/Player.h"              // update(float) - PE 505340, the per-frame body
#include "../Objects/IObject.h"
#include "../Objects/IPlayerControlObjectTemplate.h" // enterVehicle: the control/vehicle category
#include "../Objects/ICameraObject.h"         // enterVehicle: get/setViewMode on the active camera
#include "../Objects/ISoldier.h"              // enterVehicle: the ladder arm asks getLadderId
#include "../Objects/Game/Ladder.h"           // enterVehicle: ladderMap (ds:0x9ED63C) + detachSoldier (5C1780)
#include "../Objects/Game/Kit.h"              // createKit: m_chosenUnlockLevel (+0x18C) + updateUnlockStatus (596080)
#include "../Objects/Game/Soldier.h"          // enterVehicle: setLookUpDownAngle on the occupant (5A99F0)
#include "../Objects/Base/Camera.h"           // enterVehicle: the camera's +0x178 = setCurrentRotation
#include "../Objects/ObjectLayoutAccess.h"    // enterVehicle: the m_parent walks (IObject +0x28)
#include "../GameLogic/ClientGameLogic.h"     // enterVehicle: clearPendingExit (PE vtable +0x304)
#include "../../IO/Input/Input.h"             // enterVehicle: io::IInputManager +0x2C
#include "../Objects/IObjectTemplate.h"
#include "../DemoRecorder/DemoRecorder.h"
#include "../../Scene/Object/PlayerManager.h"
#include "../../Scene/Object/ObjectManager.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Math/Mat4.h"                          // createObject's transform + setRotationZXY
#include "../../Physics/PhysicsUtil.h"                // createObject's resetPhysics (PE 5B3260)
#include "../../IO/BitStream.h"
#include "../../IO/Network/NetworkManager.h"
#include "../../IO/Network/INetworkable.h"
#include "../../IO/Network/INetworkableObject.h"  // predictRemoteObjects: predict() + the latch
#include "../../IO/Network/NetworkStats.h"
#include "../../IO/Network/ProtocolLayer/NetClient.h"   // the UDP client - joinServer creates it
#include "../../IO/Network/PacketFilter.h"               // the PunkBuster packet queue receive() drains
#include <list>
#include "../../IO/Console/Console.h"
#include "../../IO/File/FileManager.h"
#include "../../Physics/ResponsePhysicsManager.h"
#include "../../Physics/PhysicsUtil.h"                // performMobileCollisionUpdate (5B3130)
#include "../../Sound/BfSound.h"
#include "../../Menu/BF2HudManager.h"
#include "../../Menu/ISwiffHost.h"
#include "../Common/DataBlockManager.h"              // DataBlockReadyEvent (the node), enqueueDataBlock
#include "../GameEvents/ContentCheckEvent.h"         // the 0x3C event block 2 sends when content checking is on
#include "../GameServer/ClientInfo.h"                // block 0's answer, data block 1
#include "../EndOfRoundInfo.h"                       // block 8's compressed post-round payload
#include "../MapList.h"                              // block 5
#include "../Persistence/AntiHacks.h"                // the name hash / GameSpy auth the ClientInfo carries
#include "../GeneralSettings.h"                   // ECNetwork 3: the connection-type setting
#include "../ProfileManager.h"                       // IProfileManager::getProfileId
#include "../../IO/ChecksumContext.h"                // m_checksumContext
#include "../GameLogic/GameLogic.h"               // stepWorld: sendFireMessages (PE 4EC9A0)
#include "../GameLogic/VoiceMessages.h"            // radioMessage: namesToIds (PE 4F7E60)
#include "../../Physics/PhysicsNodeManager.h"     // stepWorld: update / postFrameUpdate

namespace dice {
namespace hfe {

extern ClassManager* classManager;   // Core/Globals.cpp
namespace world { extern ResponsePhysicsManager* responsePhysicsManager; }   // Core/Globals.cpp (ELF ds:0x8B27848) - PE dword_A05F98
namespace world { extern PhysicsNodeManager* physicsNodeManager; }          // Core/Globals.cpp (ELF ds:0x8B27844) - PE dword_A05F94
// stepWorld's tail is the twin of GameServer::simulateFrame's, so it needs the same empty
// declaration that file makes: 0x8050B80, 5 B - the memory DLL's arena boundary, unmodelled here.
namespace memory { inline void nextPhysicFrame() {} }
extern IBase*     squadManager;      // Core/Globals.cpp (ELF ds:0x8ACBFA4) - PE dword_99EF84
extern BF2Engine* bf2Engine;         // Core/Globals.cpp - PE dword_986F48
namespace io { extern OldConsole* mainConsole; }   // PE off_978F14, the "MainConsole" simple singleton
namespace io { extern IBase* inputManager; }       // ds:0x8AC8CA4 - Core/Globals.cpp defines it as IBase*

} // namespace hfe
} // namespace dice

// C-linkage GameSpy response builder. Local declaration avoids SDK macro/header
// collisions; int carries the x86 CDResponseMethod enum, with 0 selecting newauth.
extern "C" void gcd_compute_response(char* cdkey, char* challenge, char* response, int method);

namespace dice {
namespace hfe {

#define GAMECLIENT_FILE "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\GameClient\\GameClient.cpp"

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: game-client-bodies]
#define GAMECLIENT_FRONTIER(method, text)                                                              \
    do {                                                                                               \
        std::fprintf(stderr, "[FRONTIER] dice::hfe::GameClient::" method ": " text "\n");             \
        std::fflush(stderr);                                                                           \
        std::abort();                                                                                  \
    } while (0)

// Rejects aggregation and releases an object whose interface query fails.
// The original unchecked query after failed allocation remains.
IBase* GameClient::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    GameClient* client = 0;
    if (void* allocation = ::operator new(sizeof(GameClient)))
        client = new (allocation) GameClient();
    void* result = client->queryInterface(iid);
    if (!result)
        client->release();
    return static_cast<IBase*>(result);
}

// GameClient::GameClient - 4D28D0, 409 B (named). Game::Game first; the three vtable pointers; the
// scalar members in address order; the two strings; the 0x34-byte allocation; then the two event
// registrations and, last, the one-byte stats monitor. The order is kept.
GameClient::GameClient()
    : Game()
    , m_refCount(1)                       // +0xB0
    , m_connectionState(4)                // +0xB4
    , m_field0B8(4)                       // +0xB8
    , m_connectionType(0)                 // +0xBC
    , m_connection(0)                     // +0xC0
    , m_netClient(0)                      // +0xC4
    , m_port(16567)                       // +0xC8
    , m_currentInputIndex(0)              // +0xCC
    , m_stringD0()                        // +0xD0
    , m_floatEC(2.0f)                     // +0xEC
    , m_frameCount(0)                     // +0xF0
    , m_fieldF4(-1)                       // +0xF4
    , m_fieldF8(-1)                       // +0xF8
    , m_isRunning(false)                  // +0xFC
    , m_useClientSidePrediction(true)     // +0xFD
    , m_byteFE(false)                     // +0xFE
    , m_float100(50.0f)                   // +0x100
    , m_serverAddress()                   // +0x104
    , m_disconnectRequested(false)        // +0x120
    , m_disconnectReason(0)               // +0x124
    , m_byte128(false), m_byte129(false), m_byte12A(false), m_byte12B(false)
    , m_connectionProblem(false)          // +0x12C
    , m_showNetGraph(false)               // +0x12D
    , m_field134(0)                       // +0x134
    , m_bitStream(0)                      // +0x138
    , m_networkableStats(0)               // +0x13C
    , m_demoRecorder(0)                   // +0x140
    , m_paused(false)                     // +0x144
    , m_shutdownComplete(false)           // +0x145
    , m_catchUpSteps(3)                   // +0x148
    , m_statsMonitor(0)                   // +0x14C
    , m_checksumContext(0)                // +0x150
    , m_commanderEnabled(true)            // +0x154 (a dword 1 - bytes +0x155..+0x157 are 0)
    , m_isReady(false)                    // +0x158
    , m_punkBuster(false)                 // +0x159
{
    // Checksum context starts with ordinal zero; map information and completed loading
    // supply the challenge and content digests.
    m_checksumContext = new (std::nothrow) io::ChecksumContext();
    //  / 4D2A2E: registerEventHandler(ECNetwork, this, 0), then (ECCore, this, 0) - the
    // IEventListener sub-object (+0xAC) is what is registered, as in GameServer's constructor.
    eventManager->registerEventHandler(ECNetwork, this, 0);
    eventManager->registerEventHandler(ECCore, this, 0);
    // : operator new(1) with no constructor - IGame::getStatsMonitor (504C70) hands this out.
    // The client has no StatsMonitor; a one-byte object stands where the server keeps its monitor.
    m_statsMonitor = reinterpret_cast<StatsMonitor*>(new char);
}

// ~GameClient - D1 4D2BB0, 95 B (D0 4D3400 calls it, then frees the 0x160 bytes). The vtable
// pointers are restored, the 0x34-byte object is freed if present, the one-byte monitor is freed
// unconditionally, the two strings are destroyed, then Game::~Game (444920). No event-handler
// unregistration here: shutdown() does that.
GameClient::~GameClient()
{
    if (m_checksumContext)                       // the null test, as shipped; the class has no dtor
        delete m_checksumContext;
    ::operator delete(reinterpret_cast<void*>(m_statsMonitor));
}

// the quartet
//  / 6BBD40 / 4D2B20 are the bodies GameServer's table points at too (comdat-folded: both
// classes keep the refcount at +0xB0), so the shapes are GameServer's.
void  GameClient::addRef() { ++m_refCount; }
int   GameClient::getRef() { return m_refCount; }
int   GameClient::release()
{
    const int r = --m_refCount;
    if (r == 0)
        delete this;
    return r;
}
// : IID_IBase (1) and IID_IGame (120000) answer the object; IID_IGameClient (120002)
// answers this+0xA8 - with a `this` null test the other two arms lack; anything else null.
void* GameClient::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IGame)
        return static_cast<IGame*>(const_cast<GameClient*>(this));
    if (iid == IID_IGameClient && this)
        return static_cast<IGameClient*>(const_cast<GameClient*>(this));
    return 0;
}

// init - 5A5E20, 3 B: `mov al, 1; ret`. The client's game object needs nothing set up until it
// joins; BF2Engine::startGame tests the result.
bool GameClient::init() { return true; }

// unload - 5AB2B0, 3 B: `xor al, al; ret`. Shipped: the client game never "unloads" through
// IGame::unload; disconnect() and the MapInfo arm of handleDataBlock do the work.
bool GameClient::unload() { return false; }

// Runs once: releases squads, disables local night vision, unloads logic/players,
// unregisters static networkables, disconnects, and removes event handlers.
// A surviving squad-manager reference count triggers the original breakpoint.
void GameClient::shutdown()
{
    if (m_shutdownComplete)
        return;
    if (squadManager)
    {
        if (squadManager->release())
            __debugbreak();                          // retail INT3; deliberately no recovery
        squadManager = 0;
    }
    if (world::IPlayer* local = world::playerManager->getLocalHumanPlayer())
    {
        if (gameLogic->isLevelLoadedXP())            // +0x80
            local->setNightVision(false, true);      // PE +0x2D8 = IPlayer +0x2DC on the ELF
    }
    gameLogic->unload();                             // +0x18
    destroyAllPlayers();                             // Game:: 4444C0
    unregisterStaticNetworkables();                  // Game:: 440600
    m_shutdownComplete = true;
    requestDisconnect(1);                            // : `call [vptr+0x120]` - virtual, kept virtual
    disconnect();
    eventManager->unregisterEventHandler(ECNetwork, this);
    eventManager->unregisterEventHandler(ECCore, this);
}

// requestDisconnect - 4D1C90, 23 B, the +0x120 slot: the flag (a dword 1) and the reason. update()
// acts on it next frame; BF2Engine::mainLoop reads it through IGameClient +0x70 and shuts the game.
void GameClient::requestDisconnect(int reason)
{
    m_disconnectRequested = true;
    m_disconnectReason    = reason;
}

// Stops gameplay and VO, unloads players/network state, then frees transport,
// connection, stream, and statistics. Returns the resulting state 4.
int GameClient::disconnect()
{
    m_isRunning = false;
    if (world::IPlayer* local = world::playerManager->getLocalHumanPlayer())
    {
        if (gameLogic->isLevelLoadedXP())
            local->setNightVision(false, true);
    }
    if (gameLogic)
    {
        gameLogic->stopPlayingVoMessages();          // +0x230
        gameLogic->unload();                         // +0x18
        destroyAllPlayers();
    }
    if (io::networkManager)
        io::networkManager->clear();
    if (m_netClient)
    {
        m_netClient->closeConnection(13);            // : `call [vptr+0x1C]`
        m_netClient->shutDown();                     // : `call [vptr+0x18]`
        if (m_netClient)
            delete m_netClient;                      // : the +0x74 deleting destructor
        m_netClient = 0;
    }
    if (m_connection)
    {
        delete m_connection;                         // : the deleting destructor through the vtable
        m_connection = 0;
    }
    if (m_bitStream)
    {
        delete m_bitStream;                          //  frees the owned buffer, then operator delete
        m_bitStream = 0;
    }
    if (m_networkableStats)
        delete m_networkableStats;                   // : the deleting destructor
    m_fieldF4          = -1;
    m_fieldF8          = -1;
    m_networkableStats = 0;
    m_isRunning        = false;
    m_connectionType   = 0;
    m_floatEC          = 2.0f;
    m_field0B8         = 4;
    m_connectionState  = 4;
    return 4;
}

// Loads the current map after preparing squads and notifying listeners. Clears
// readiness first and restores file resolve mode after load success or failure.
// The recorder's playback-skip flag bypasses loading and returns true.
bool GameClient::load()
{
    DemoRecorder* const recorder = m_demoRecorder;
    m_isReady = false;
    if (recorder)
    {
        // ..4D2EBD: `recorder+0xC4 && !recorder+0xCD` (54B8F0) and the byte at +0x105 - the
        // DemoRecorder's PE layout is not mapped onto the tree's ELF-recovered class.
        GAMECLIENT_FRONTIER("load", "DemoRecorder playback predicate (54B8F0, bytes +0xC4/+0xCD/+0x105) is unmapped");
    }

    if (squadManager)
        static_cast<ISquadManager*>(squadManager)->reAddDynamicSpawnGroups();     // +0x14
    else
    {
        squadManager = classManager->createInstance(CID_ClientSquadManager, IID_ISquadManager, 0);
        if (!squadManager)
            Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 2083, "") << "Failed to init squadManager";
    }

    std::string path(m_mapInfo.getLevelPath());     // : &mapInfo+0x1C
    path += m_mapInfo.getMapName();                  // : &mapInfo+0x38
    std::string levelPath(path);
    levelPath += "/";
    settings->stringSet(std::string("GSLoadLevel"), m_mapInfo.getMapName());
    eventManager->postEvent(ECCore, 2, const_cast<char*>(m_mapInfo.getMapName().c_str()), 0.0f);

    const int resolveMode = io::fileManager->getFileResolveMode();
    io::fileManager->setFileResolveMode(1);
    if (!gameLogic->load(levelPath))                 // +0x14
    {
        //  / 4D3072: two LoadStat calls on the singleton (45D6D0, then 45D540) - the failed
        // load's report. [UNRESOLVED] which two of LoadStat's methods those PE bodies are.
        GAMECLIENT_FRONTIER("load", "IGameLogic::load refused; the LoadStat pair 45D6D0/45D540 is unmapped");
        io::fileManager->setFileResolveMode(resolveMode);
        return false;
    }
    io::fileManager->setFileResolveMode(resolveMode);
    const int resolveMode2 = io::fileManager->getFileResolveMode();
    io::fileManager->setFileResolveMode(1);
    // [TEMP] [CHOSEN, not recovered] - poster identity. [RETIRE-WHEN: bone-staging-index]
    // The other poster of ECCore 3; see the twin note in GameServer::loadPath. Both posts are
    // attested (this one from PE 4D2E90), and handleEvent has no re-entry guard, so whichever
    // configuration runs both in one process destroys every loaded animation on the second pass.
    std::fprintf(stderr, "[TEMP][ANIMSWEEP] ECCore 3 posted by GameClient::load (path=%s)\n",
                 levelPath.c_str());
    std::fflush(stderr);

    eventManager->postEvent(ECCore, 3, 0, 0.0f);
    eventManager->postEvent(ECCore, 7, 0, 0.0f);
    io::fileManager->setFileResolveMode(resolveMode2);
    return true;
}

// Receives network/demo state, advances fixed simulation steps, updates sound on the
// last step, then transmits and clears sampled input. A requested disconnect returns
// false. Demo playback and lightmap rendering use their own receive/update paths.
bool GameClient::update(int steps, float dt)
{
    bool ok = true;
    if (world::playerManager)                        // : dword_A08F60 - the PlayerManager, not the
        world::playerManager->getNumberOfPlayers();  // ObjectManager; PE +0x88 == ELF +0x88 (the dtor is
                                                     // declared last). The count is computed and dropped.
    const double delta = getDeltaTime();             // Game:: 440590
    m_useClientSidePrediction = settings->boolGet(std::string("GSUseClientSidePrediction"), true);
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (isLightmapRenderingNode())
    {
        // : g_distributedLightRenderer->+0x28(); then the quit latch on byte_9A7758 and
        // mainConsole->executeLine("quit\n", ignoredString_, 0, 0, 0, 0, 0).
        GAMECLIENT_FRONTIER("update", "the lightmap-rendering node's DistributedLightRenderer +0x28 is unrecovered");
    }
    else if (local && gameLogic->getGameStatus() == 1)   // +0x158
    {
        static_cast<Player*>(local)->update(static_cast<float>(delta));
    }

    const bool demoPlaying = m_demoRecorder != 0;    // : recorder && 54B8F0(recorder)
    if (demoPlaying)
        GAMECLIENT_FRONTIER("update", "DemoRecorder playback predicate (54B8F0) is unmapped");
    if (!demoPlaying)
    {
        if (steps)
            ok = receive(dt);
    }
    else if (!m_isRunning && steps)
        ok = receive(dt);

    if (m_disconnectRequested)
    {
        disconnect();
        return false;
    }
    if (m_isRunning)
    {
        const int allowed = m_catchUpSteps;
        if (steps <= allowed)
        {
            if (allowed < 3)
                m_catchUpSteps = allowed + 1;
        }
        else
        {
            steps          = 1;
            m_catchUpSteps = 1;
        }
        if (!connectionWatchdog(delta))
            return true;
        for (int i = 0; i < steps; ++i)
        {
            // Count each simulated frame through the shared engine field.
            ++bf2Engine->dword54;
            if (demoPlaying)
                receive(dt);
            world::responsePhysicsManager->resetCachedCollisionObjects();   // +0x28
            stepWorld(dt);
            world::responsePhysicsManager->startFrame();                    // +0x2C
            if (i == steps - 1)
            {
                if (demoPlaying)
                    GAMECLIENT_FRONTIER("update", "the demo listener transform (440B20 / 54BC20) is unmapped");
                else
                    g_bfSound->update(steps);                               // PE +0x18
            }
        }
    }
    // Voting uses the frame's dt argument, not the separately sampled delta time.
    ClientVoteManager::getInstance()->update(dt);
    transmit(steps, delta);
    m_playerInputs.clear();
    if (m_showNetGraph && m_bitStream)
        GAMECLIENT_FRONTIER("update", "BitStream PE +0x04 (the net-graph reset) is unmapped");
    return ok;
}

// Local input is quantized through PlayerAction before prediction so it matches server input.
// One queued sample is consumed per step; an empty queue supplies zero input.
// The current index comes from ActionBuffer's last-input index.
namespace {
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: client-spawn-camera-trace]
// Opt-in observation of the camera/occupant handoff. No extra queries on the default path.
// Each phase reports its first eight samples and subsequent association/mode/gate changes.
void traceLocalSpawnCamera(world::IPlayer* local, bool afterUpdate,
                          bool prediction, bool connectionProblem)
{
    static const bool enabled = []() {
        const char* const value = std::getenv("BF2_SPAWN_TRACE");
        return value && value[0] == '1';
    }();
    if (!enabled)
        return;

    struct State {
        world::IPlayer* local;
        world::IPlayer* occupant;
        world::IObject* vehicle;
        world::IObject* defaultVehicle;
        world::IObject* freeCamera;
        world::ICameraObject* camera;
        unsigned int flags;
        int mode;
        bool freeEnabled, overheadEnabled, firstPerson, prediction, connectionProblem;
    };
    State state = {};
    state.local = local;
    state.vehicle = local->getVehicle();
    state.defaultVehicle = local->getDefaultVehicle();
    state.freeCamera = local->getFreeCamera();
    state.camera = local->getCamera();
    state.mode = state.camera ? static_cast<int>(state.camera->getViewMode()) : -1;
    state.freeEnabled = local->getFreeCameraEnabled();
    state.overheadEnabled = local->getOverheadCameraEnabled();
    state.firstPerson = local->hasFirstPersonView();
    state.prediction = prediction;
    state.connectionProblem = connectionProblem;
    if (state.vehicle)
    {
        state.flags = state.vehicle->getFlags();
        world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
            state.vehicle->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            state.occupant = pco->getOccupingPlayer();
    }

    const unsigned int phase = afterUpdate ? 1u : 0u;
    static State previous[2] = {};
    static unsigned int samples[2] = {};
    const State& old = previous[phase];
    const bool changed = state.local != old.local || state.occupant != old.occupant
        || state.vehicle != old.vehicle || state.defaultVehicle != old.defaultVehicle
        || state.freeCamera != old.freeCamera || state.camera != old.camera
        || state.flags != old.flags || state.mode != old.mode
        || state.freeEnabled != old.freeEnabled || state.overheadEnabled != old.overheadEnabled
        || state.firstPerson != old.firstPerson || state.prediction != old.prediction
        || state.connectionProblem != old.connectionProblem;
    if (samples[phase]++ >= 8 && !changed)
        return;
    previous[phase] = state;

    world::IObject* const cameraObject = state.camera ? static_cast<world::IObject*>(
        state.camera->queryInterface(world::IID_IObject)) : 0;
    const Vec3 position = state.vehicle ? state.vehicle->getAbsolutePosition() : Vec3::getZero();
    const Vec3 cameraPosition = cameraObject ? cameraObject->getAbsolutePosition() : Vec3::getZero();
    std::fprintf(stderr,
        "[SPAWN-CAMERA] %s sample=%u local=%p id=%d occupant=%p vehicle=%p default=%p free=%p "
        "flags=%08X camera=%p template=%s mode=%d freeOn=%d overhead=%d firstPerson=%d "
        "prediction=%d connectionProblem=%d position=(%.3f,%.3f,%.3f) cameraObjectPosition=(%.3f,%.3f,%.3f)\n",
        afterUpdate ? "after" : "before", samples[phase], static_cast<void*>(local), local->getId(),
        static_cast<void*>(state.occupant), static_cast<void*>(state.vehicle),
        static_cast<void*>(state.defaultVehicle), static_cast<void*>(state.freeCamera), state.flags,
        static_cast<void*>(state.camera),
        cameraObject ? cameraObject->getTemplate()->getName().c_str() : "<none>", state.mode,
        state.freeEnabled, state.overheadEnabled, state.firstPerson, prediction, connectionProblem,
        position.x, position.y, position.z, cameraPosition.x, cameraPosition.y, cameraPosition.z);
    std::fflush(stderr);
}
} // namespace

bool GameClient::sampleLocalInput(float dt)
{
    const bool noQueuedInput = m_playerInputs.size() < 1;            // Game +0x84

    io::PlayerInput input;
    input.mappingFlags = 0;
    input.dword0 = 0;
    input.dword1 = 0;
    input.flag   = 0;
    for (int i = 0; i < 64; ++i)
        input.inputs[i] = 0.0f;

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // +0x30
    if (!local)
        return true;

    m_currentInputIndex = local->getActionBuffer()->m_lastInputIndex;  // +0x94, then PE +0x0C

    if (!noQueuedInput)
        input = m_playerInputs.front();                                // 0x46 dwords = 0x118 B

    local->getAutoInput(input, false);                                 // PE +0x40 = ELF +0x44

    if (local->getVehicle())                                           // PE +0x44 = ELF +0x48
    {
        if (void* const pco =
                local->getVehicle()->queryInterface(world::IID_IPlayerControlObject))
        {
            static_cast<world::IPlayerControlObject*>(pco)->preHandlePlayerInput(input, dt);  // +0x15C
        }
    }

    // the round trip - see the note above.
    PlayerAction action;
    action.set(input);
    action.get(input);

    traceLocalSpawnCamera(local, false, m_useClientSidePrediction, m_connectionProblem);
    if (!m_connectionProblem && m_useClientSidePrediction)             // +0x12C / +0xFD
    {
        if (gameLogic->getGameStatus() != 4)                           // +0x158
        {
            gameLogic->checkPlayerTriggers(local, local->getVehicle(), input, false);  // +0x154
            local->handleInput(input, dt, 0);                          // PE +0x3C = ELF +0x40
        }
        performHandleUpdate(dt, local->getVehicle(), 0, false);
    }

    // the same slot again, now with true.
    traceLocalSpawnCamera(local, true, m_useClientSidePrediction, m_connectionProblem);
    local->getAutoInput(input, true);                                  // PE +0x40

    queueLocalAction(local, action);

    if (!noQueuedInput)
        m_playerInputs.pop_front();                                    // hand-inlined in the PE

    return true;
}

// Stamps and stores a sampled action, then sends the stored copy through the action
// manager. Requires the player's action buffer and an initialized connection.
// [INFERRED] Method name.
void GameClient::queueLocalAction(world::IPlayer* player, PlayerAction& action)
{
    ActionBufferEntry* const entry = player->getActionBuffer()->add(action);   // +0x94, then 5BC4D0
    m_fieldF8 = entry->m_action.m_inputIndex;                                  // +0xF8
    m_connection->m_playerActionManager->add(entry->m_action);                 // +0xC0 / +0x24
}

// Predicts ghosted remote objects, excluding the local player's vehicle and soldier.
// Uses world time in milliseconds; dt is ignored. Descriptors must contain a first
// property and networkable because those entries are read before any emptiness check.
// Repeated vehicle/networkable queries retain their original side effects.
bool GameClient::predictRemoteObjects(float /*dt - DEAD, see the note above*/)
{
    const float worldTimeMs = static_cast<float>(WorldPref::getWorldTime() * 1000.0);

    unsigned short vehicleId = 0;
    unsigned short defaultVehicleId = 0;
    if (world::IPlayer* const local = world::playerManager->getLocalHumanPlayer())      // +0x30
    {
        // the repeated calls are the shipped ones - see defect 2 above.
        if (local->getVehicle() && local->getVehicle()->getNetworkable())
            vehicleId = local->getVehicle()->getNetworkable()->m_networkableId;
        if (local->getDefaultVehicle() && local->getDefaultVehicle()->getNetworkable())
            defaultVehicleId = local->getDefaultVehicle()->getNetworkable()->m_networkableId;
    }

    std::map<unsigned short, io::NetworkableDescriptor*>& active =
        io::networkManager->m_activeDescriptors;                                        // +0x44
    for (std::map<unsigned short, io::NetworkableDescriptor*>::iterator it = active.begin();
         it != active.end(); ++it)
    {
        io::NetworkableDescriptor* const descriptor = it->second;

        // unchecked subscripts, before the empty test below - see defect 1.
        if (!descriptor->properties[0].networkables[0]->m_ghostActive)                  // +0x0C
            continue;
        if (descriptor->descId == vehicleId)                                            // +0x04
            continue;
        if (descriptor->descId == defaultVehicleId)
            continue;

        for (std::vector<io::UniqueNetProperty>::iterator p = descriptor->properties.begin();
             p != descriptor->properties.end(); ++p)
        {
            for (std::vector<io::INetworkable*>::iterator q = p->networkables.begin();
                 q != p->networkables.end(); ++q)
            {
                if (io::INetworkableObject* const object = (*q)->m_networkableObject)   // +0x18
                {
                    // the latch lives on NetworkableObjectBase, not on the interface; the PE
                    // writes the raw +0x0C byte, which is that field on every concrete networkable.
                    static_cast<io::NetworkableObjectBase*>(object)->m_dword0C = 0;     // +0x0C
                    object->predict(worldTimeMs);                                       // +0x14
                }
            }
        }
    }
    return true;                                                                        // `mov al, 1`
}

// Prediction and local-position latch evidence: PE 0x5C01A0 and related helpers.
// [UNRESOLVED] The purpose of the latched Vec3 at ds:0x9ED5D8 is unknown;
// it is updated each eligible prediction step; the first update reads and stores twice.
// Historical tracking: [RETIRE-WHEN: client-local-freecamera]
static float s_latchedLocalPosition[3] = { 0.0f, 0.0f, 0.0f };   // ds:0x9ED5D8 / 5DC / 5E0
static unsigned int s_latchedLocalPositionFlags = 0;             // ds:0x9ED5E4

bool GameClient::stepLocalPrediction(float dt)
{
    world::objectManager->updateObjectsInGrid();                        // +0xA4
    world::responsePhysicsManager->reset();                             // PE +0x20 = ELF +0x24

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // +0x30
    if (!local)
        return true;
    if (m_connectionProblem)                                            // +0x12C
        return true;
    if (!m_useClientSidePrediction)                                     // +0xFD
        return true;
    if (m_byte12B)                                                      // +0x12B
        return true;

    world::IObject* const vehicle = local->getVehicle();                // PE +0x44 = ELF +0x48
    performMobileCollisionUpdate(dt, vehicle, false);

    if (!(s_latchedLocalPositionFlags & 1u))
    {
        s_latchedLocalPositionFlags |= 1u;
        const Vec3& first = vehicle->getAbsolutePosition();             // PE +0x68 = ELF +0x6C
        s_latchedLocalPosition[0] = first.x;
        s_latchedLocalPosition[1] = first.y;
        s_latchedLocalPosition[2] = first.z;
    }
    // the same store again, unconditionally - see the note above.
    const Vec3& current = vehicle->getAbsolutePosition();
    s_latchedLocalPosition[0] = current.x;
    s_latchedLocalPosition[1] = current.y;
    s_latchedLocalPosition[2] = current.z;
    return true;
}

// Advance events, world physics and client prediction in the shipped order (PE 0x5C0460).
// Local input is quantized through PlayerAction before prediction; see sampleLocalInput.
bool GameClient::stepWorld(float dt)
{
    eventManager->processEvents(dt);                                   // ds:9FF964, +0x20
    updateTriggerManager(dt);                                          // Game::
    gameLogic->update(dt);                                             // ds:99EF80, +0x20

    // THE remote-player half OF checkPlayerTriggers: every other player's weapon, sprint and
    // reload state is replayed onto the vehicle they are in - but only for a vehicle the server is
    // actually ghosting to us.
    const std::list<world::IPlayer*>& players =
        world::playerManager->getPlayersSortedByScore();                // +0x40
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        world::IObject* const vehicle = player->getVehicle();           // PE +0x44 = ELF +0x48
        if (!vehicle)
            continue;
        io::INetworkable* const networkable = vehicle->getNetworkable();   // +0xD4
        if (!networkable || !networkable->m_ghostActive)                // +0x0C
            continue;

        if (player->getAndResetReloadState())                           // PE +0x1C8 = ELF +0x1CC
            vehicle->handleMessage(static_cast<world::TemplateMessage>(10), player, false);   // +0x100
        if (player->getSprintState())                                   // PE +0x1B8 = ELF +0x1BC
            vehicle->handleMessage(
                static_cast<world::TemplateMessage>(player->getSprintStateLastTick() ? 41 : 42),
                player, false);                                         // PE +0x1C0 = ELF +0x1C4

        unsigned int fireMask = 0;
        if (player->getFlareFiringState()) fireMask |= 4u;              // PE +0x1B0 = ELF +0x1B4
        if (player->getAltFiringState())   fireMask |= 2u;              // PE +0x1A8 = ELF +0x1AC
        if (player->getFiringState())      fireMask |= 1u;              // PE +0x1A0 = ELF +0x1A4

        // [UNRESOLVED] The original Player state-clear has no mapped member in this layout
        // and is omitted to avoid writing an unrelated field.
        // [RETIRE-WHEN: player-pe-layout] Map the client Player member before restoring the store.

        if (fireMask)
            sendFireMessages(fireMask, player, vehicle, false);
    }

    sampleLocalInput(dt);
    world::objectManager->updateObjects(dt, static_cast<world::UpdateFreqencyType>(0), 3);   // +0x28

    // the same four gates as stepLocalPrediction, re-tested rather than shared.
    if (world::IPlayer* const local = world::playerManager->getLocalHumanPlayer())   // +0x30
    {
        if (!m_connectionProblem && m_useClientSidePrediction && !m_byte12B)   // +0x12C / +0xFD / +0x12B
            performMobilePhysicsUpdate(dt, local->getVehicle(), false);
    }
    world::physicsNodeManager->update(dt, 0, 0);                        // PE +0x20 = ELF +0x24

    if (!isLightmapRenderingNode())
    {
        predictRemoteObjects(dt);
        world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
        // getVehicle() three times, and the gate is the vehicle's parent (+0x28), so yaw
        // stabilisation only runs on a vehicle that is itself a child of something.
        if (local && local->getVehicle() && local->getVehicle()->getParent())
            performYawStabilisation(local->getVehicle());
    }

    // resetCachedCollisionObjects brackets the world-collision pass - called on both sides.
    world::responsePhysicsManager->resetCachedCollisionObjects();       // PE +0x24 = ELF +0x28
    updateWorldCollision(dt);                                           // Game::
    world::responsePhysicsManager->resetCachedCollisionObjects();

    stepLocalPrediction(dt);
    world::physicsNodeManager->postFrameUpdate();                       // PE +0x24 = ELF +0x28
    world::responsePhysicsManager->postFrameUpdate();                   // PE +0x30 = ELF +0x34

    ++m_frameCount;                                                     // +0xF0
    memory::nextPhysicFrame();                                          // Memory.dll, ds:87F640
    return true;                                                        // `mov al, 1`
}

// Update connection-trouble UI and poll diagnostic accessors every ten seconds.
// The countdown is shared across GameClient instances; non-positive or NaN time
// triggers the polling arm. Lightmap work clears the local action buffer.
static double s_connectionWatchdogTimer = 0.0;

bool GameClient::connectionWatchdog(double delta)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // PE +0x30
    if (!local)
        return true;                                    // the only early exit

    if (isLightmapRenderingNode())
    {
        // ..4D2C4A. and the "contradiction" this arm was
        // parked on turned out to be A misreading OF mine - see the note below.
        local->getActionBuffer()->clear(0);                                     // +0x94, then 5BC3C0
    }

    if (local->getActionBuffer()->size() <= 45)                        // +0x94, then 68E7B0
    {
        if (hudManager)
        {
            hudManager->clearDisconnectMessage();                      // +0x104
            hudManager->setMenuLoading(false);                         // +0x114
        }
        m_connectionProblem = false;                                   // +0x12C
    }
    else
    {
        if (hudManager && gameLogic->getGameStatus() == 1)             // PE +0x158
        {
            hudManager->showDisconnectMessage(                         // +0x100
                hudManager->getLocalizedString(std::string("GAME_MESSAGE_CONNECTION_PROBLEM")));  // +0x38
            hudManager->setMenuLoading(true);                          // +0x114 - push 1
        }
        m_connectionProblem = true;
    }

    s_connectionWatchdogTimer -= delta;                                // ds:9A7750
    if (!(s_connectionWatchdogTimer > 0.0))                            // a NaN takes this arm too
    {
        if (m_netClient)                                               // +0xC4
        {
            // All three results are discarded by the shipped body.
            local->getActionBuffer();                                  // +0x94
            local->getPing();                                          // +0x164
            io::ConnectionStatus status;
            m_netClient->getConnectionStatus(status);                  // +0x6C
        }
        s_connectionWatchdogTimer = 10.0;                              // ds:89C2A8
    }
    return true;                                                       // `mov al, 1` - unconditional
}

// Drains client packets and connection notifications, returning whether level loading remains active.
// Packet decode failures request disconnection. PunkBuster packets require the enabled client path.
// Notification codes 23/24 set a message without disconnecting; 25 disconnects without a reason.
bool GameClient::receive(float dt)
{
    (void) dt;
    if (!m_connection)
        return true;
    ui::ISwiffHost* const host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    int  reason       = 0;
    bool disconnected = false;
    if (m_netClient)
    {
        std::list<io::StoredPacket> packets;
        m_netClient->m_packetFilter.getPackets(io::FILTEREDPACKETTYPE_0, &packets);
        for (std::list<io::StoredPacket>::iterator it = packets.begin(); it != packets.end(); ++it)
        {
            if (m_punkBuster)
                GAMECLIENT_FRONTIER("receive", "the PunkBuster client packet handler (4D9DE0) is unrecovered");
            else
                Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 672, "") << "PunkBuster packets on a disabled client";
            std::free(it->data);
        }
    }
    int code;
    do
    {
        if (m_netClient)
            code = m_netClient->receive(m_bitStream);
        else
        {
            // ..4D4FF8: the demo playback read - 4C4420()'s recorder stream through 54C3B0 into
            // m_bitStream; a non-empty stream is a packet (0), else 8.
            GAMECLIENT_FRONTIER("receive", "the demo playback packet read (4C4420 / 54C3B0) is unmapped");
        }
        switch (code)
        {
        case 0:
            if (!m_connection->processReceivedPacket(m_bitStream))
            {
                disconnected = true;
                reason = 8;
                host->setMessageId(23);
            }
            if (!m_netClient)
            {
                bool delivered = true;
                m_connection->handlePacketStatusNotification(delivered);
            }
            m_showNetGraph = settings->boolGet(std::string("GSShowNetGraph"), false);
            if (m_showNetGraph)
            {
                m_networkableStats->sampleNetworkableReadStats();
                m_networkableStats->sampleStreamManagerReadStats(m_connection, false);
            }
            break;
        case 10:
        case 13:
            disconnected = true;
            if (!host->getMessageId())
                host->setMessageId(3);
            break;
        case 11:
        case 14:
            break;
        case 19: disconnected = true; reason = 3;  host->setMessageId(4);  break;
        case 20:
            disconnected = true; reason = 2;
            if (host->getExpansionPack(std::string("")) == 1)
                host->setMessageId(28);
            else if (host->getExpansionPack(std::string("")) == 2)
                host->setMessageId(29);
            else
                host->setMessageId(5);
            break;
        case 21: disconnected = true; reason = 4;  host->setMessageId(1);  break;
        case 22: disconnected = true; reason = 6;  host->setMessageId(2);  break;
        case 23:                                   host->setMessageId(8);  break;
        case 24:                                   host->setMessageId(9);  break;
        case 25: disconnected = true;              host->setMessageId(10); break;
        case 26: disconnected = true; reason = 10; host->setMessageId(7);  break;
        case 27: disconnected = true; reason = 11; host->setMessageId(6);  break;
        case 34:
            disconnected = true; reason = 5;
            host->setMessageId(22);
            host->setServerMod(m_netClient->getDisconnectText());               // +0x350 of +0x70
            break;
        case 35: disconnected = true; reason = 15; host->setMessageId(24); break;
        case 37: disconnected = true; reason = 16; host->setMessageId(25); break;
        default:
            break;
        }
    }
    while (code != 8);
    if (!disconnected)
        return !m_abortLoadingLevel;
    if (hudManager)
        hudManager->showDisconnectMessage(
            hudManager->getLocalizedString(std::string("GAME_MESSAGE_DISCONNECTED")));
    if (m_field134)
    {
        m_field134 = 0;
        requestDisconnect(0);
    }
    else
        requestDisconnect(reason);
    return !m_abortLoadingLevel;
}

// Processes packet acknowledgments and sends when the timer or stepped-send flag
// requires it. Refused acknowledgments disconnect with reason 13; a full buffer
// forces an immediate send. [UNRESOLVED] Client stepped-send field identity.
void GameClient::transmit(int steps, double delta)
{
    if (!m_netClient)
        return;
    int ping = 0;
    if (m_netClient->getPing(&ping))
        m_connection->m_lastPingMs = ping;                    // PE +0x140
    bool delivered = false;
    while (m_netClient->getPacketDeliveryStatus(delivered))
    {
        if (!m_connection->handlePacketStatusNotification(delivered))
            m_netClient->closeConnection(13);
    }
    m_connection->m_timer1C -= static_cast<float>(delta);
    // : `steps > 0 && ClientConnection PE +0x118 != 0` - the tree's ELF layout has no name at
    // the PE's +0x118 (a double, m_pingValue, sits at the ELF's); the timer alone drives the send
    // until that dword is mapped. [UNRESOLVED]
    if (m_connection->m_timer1C <= 0.0f)
    {
        (void) steps;
        m_connection->m_timer1C = 1.0f / static_cast<float>(m_connection->m_dword14);
        m_connection->transmitMsgs();                          // its bool is dropped here
        io::BitStream* const packet = m_connection->m_bitStream;   // PE +0x78
        const unsigned int bytes = (packet->m_highWaterBits >> 3) + ((packet->m_highWaterBits & 7) != 0 ? 1u : 0u);
        if (bytes)
        {
            const int code = m_netClient->bufferPacket(packet, 0);
            if (code)
            {
                if (code == 15)
                {
                    m_connection->m_sentData = true;           // PE +0x74
                    m_connectionState = 1;
                    m_netClient->send();
                    return;
                }
            }
            else
            {
                m_connection->m_sentData = false;
                m_connectionState = 0;
                if (m_showNetGraph)
                {
                    m_networkableStats->sampleStreamManagerWriteStats(m_connection, false);
                    m_networkableStats->sampleNetworkableWriteStats();
                }
            }
        }
    }
    m_netClient->send();
}

// Selects client bandwidth settings and notifies the connection; type 6 is ignored.
// Client and server use different columns of the shared table. This also resets
// interpolation and extrapolation timing through the [CHOSEN] NetworkManager setters.
void GameClient::setConnectionType(int type)
{
    if (type == 6)
        return;
    ClientConnection* const conn = m_connection;     // : read before the type store
    m_connectionType = type;
    if (!conn)
        return;
    const int* const row = g_connectionTypes[type];  // stride 24 - unchecked, as shipped
    m_connection->setConnectionType(static_cast<ConnectionType>(type));
    if (m_connection)
        m_connection->m_dword14 = static_cast<int>(static_cast<float>(row[0]));   // ..55, via the FPU
    if (m_connection)
        m_connection->m_packetSizeOverride = row[2];
    io::networkManager->setInterpolationTime(row[4]);                          //   +0x50
    io::networkManager->setExtrapolationTime(row[5]);                          //   +0x54
    GameEvent* const event = new (std::nothrow) ConnectionTypeEvent(static_cast<ConnectionType>(type));   // 0x10 B
    m_connection->m_gameEventManager->addEvent(event);                         // PE +0x38 (ELF +0x3C)
}

// say - 4D1FB0, 107 B. The local player's id (-1 without one) and the connection's StringManager:
// enqueueString(m_currentChannel, ChannelMessageType 2, localId, text, -1 (dword_970514), m_currentFlags).
// Always true.
bool GameClient::say(const std::string& text)
{
    int localId = -1;
    if (world::playerManager->getLocalHumanPlayer())
        localId = world::playerManager->getLocalHumanPlayer()->getId();
    if (m_connection)
        m_connection->getStringManager()->enqueueString(m_currentChannel,
                                                        static_cast<ChannelMessageType>(2),
                                                        localId, text, -1, m_currentFlags);
    return true;
}

// Send a radio message, optionally playing it locally for the local human player.
// Unknown names, missing players or missing connections fail. Spam refusal also
// updates the HUD notice unless bypassSpamCheck is set. Preserve the distinct
// team/id query order for event construction and local playback. gameInfoMessage
// retains its duplicate argument store and swapped sender/team playback arguments.
bool GameClient::radioMessage(TargetChannel channel, const std::string& message,
                              world::IPlayer* player, unsigned int extra,
                              bool localPlayback, bool bypassSpamCheck)
{
    if (!player || !m_connection)                                     // then +0xC0 at 4D2036
        return false;
    if (player != world::playerManager->getLocalHumanPlayer())        // PE +0x30 = ELF +0x34
        localPlayback = false;

    int messageId;
    const std::string emptyVoice;
    const std::string emptyFilter;
    if (!static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, 0, 0, message, emptyVoice, emptyFilter))
        return false;

    if (!bypassSpamCheck && player->getIsThisASpam(true))             // PE +0x308 = ELF +0x30C
    {
        if (localPlayback && !player->getIsAIPlayer())                // PE +0x60 = ELF +0x64
        {
            localGameInfoMessage(static_cast<TargetChannel>(14), std::string("spam"));  // PE +0x60
            if (hudManager)
            {
                const int blockedDuration = serverInfo->m_block.radioBlockedDuration;
                const int seconds = static_cast<int>(
                    player->getRadioSpamFilterFlagTimer()             // PE +0x320 = ELF +0x324
                    + blockedDuration + 1.0f
                    - WorldPref::getWorldTime());
                const std::wstring text = hudManager->replaceWideTokenFromInt(           // +0x44
                    hudManager->getLocalizedString(                                      // +0x38
                        std::string("HUD_HUD_MENU_CHAT_SPAM_MESSAGE")),
                    std::string("#SEC#"), seconds);
                hudManager->addServerChat(text, player->getTeam());   // +0x130, team read last
            }
        }
        return true;
    }

    RadioMessageEvent* event = new (std::nothrow) RadioMessageEvent();  // operator new(0x18), 4D21EC
    if (event)
    {
        const int team = player->getTeam();                           // PE +0xE4 - called first here
        const int id   = player->getId();                             // PE +0x50 = ELF +0x54
        event->m_messageId = static_cast<unsigned char>(messageId);
        event->m_senderId  = static_cast<unsigned char>(id);
        event->m_team      = static_cast<unsigned char>(team);
        event->m_field0F   = static_cast<unsigned char>(extra);
        event->m_channel   = static_cast<int>(channel);               // +0x10 - the channel
        event->m_type      = 3;
    }
    sendGameEvent(0xFFFFFFFFu, event);                                // PE +0x8C = ELF +0x90

    if (localPlayback)
    {
        const int id   = player->getId();                             // getId first in this arm
        const int team = player->getTeam();
        playbackRadioMessage(channel, static_cast<ChannelMessageType>(3), team, id, messageId);
    }
    return true;
}

bool GameClient::radioMessageFromPlayer(TargetChannel channel, const std::string& message,
                                        unsigned int playerId, unsigned int extra,
                                        bool localPlayback, bool bypassSpamCheck)
{
    world::IPlayer* const player =
        world::playerManager->getPlayerFromId(static_cast<int>(playerId));   // PE +0x20 = ELF +0x24
    if (!player)
        return false;
    if (!m_connection)                                                // +0xC0 at 4D22A7
        return false;

    int messageId;
    const std::string emptyVoice;
    const std::string emptyFilter;
    if (!static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, 0, 0, message, emptyVoice, emptyFilter))
        return false;

    if (!bypassSpamCheck && player->getIsThisASpam(true))
    {
        // The extra guard this overload has and radioMessage does not: 440510.
        if (!isDedicated() && localPlayback && !player->getIsAIPlayer())
        {
            localGameInfoMessage(static_cast<TargetChannel>(14), std::string("spam"));
            if (hudManager)
            {
                const int blockedDuration = serverInfo->m_block.radioBlockedDuration;
                const int seconds = static_cast<int>(
                    player->getRadioSpamFilterFlagTimer()
                    + blockedDuration + 1.0f
                    - WorldPref::getWorldTime());
                const std::wstring text = hudManager->replaceWideTokenFromInt(
                    hudManager->getLocalizedString(
                        std::string("HUD_HUD_MENU_CHAT_SPAM_MESSAGE")),
                    std::string("#SEC#"), seconds);
                hudManager->addServerChat(text, player->getTeam());
            }
        }
        return true;
    }

    RadioMessageEvent* event = new (std::nothrow) RadioMessageEvent();
    if (event)
    {
        const int team = player->getTeam();                           // the only virtual read here
        event->m_messageId = static_cast<unsigned char>(messageId);
        // The argument, not getId() - the caller already names the speaker.
        event->m_senderId  = static_cast<unsigned char>(playerId);
        event->m_team      = static_cast<unsigned char>(team);
        event->m_field0F   = static_cast<unsigned char>(extra);
        event->m_channel   = static_cast<int>(channel);
        event->m_type      = 3;
    }
    sendGameEvent(0xFFFFFFFFu, event);

    if (localPlayback)
    {
        const int team = player->getTeam();
        playbackRadioMessage(channel, static_cast<ChannelMessageType>(3), team,
                             static_cast<int>(playerId), messageId);
    }
    return true;
}

bool GameClient::gameInfoMessage(TargetChannel channel, const std::string& message,
                                 unsigned int a, unsigned int b, bool localPlayback)
{
    int messageId;
    const std::string emptyVoice;
    const std::string emptyFilter;
    if (!static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, 0, 0, message, emptyVoice, emptyFilter))
        return false;                                                 // the server answers true

    RadioMessageEvent* event = new (std::nothrow) RadioMessageEvent();
    if (event)
    {
        event->m_messageId = static_cast<unsigned char>(messageId);
        event->m_senderId  = static_cast<unsigned char>(a);
        event->m_team      = static_cast<unsigned char>(b);
        event->m_field0F   = static_cast<unsigned char>(b);           // `b` again, shipped
        event->m_channel   = static_cast<int>(channel);
        event->m_type      = 0;                                       // 0, not 3
    }
    sendGameEvent(0xFFFFFFFFu, event);

    // ..4D25B2 - `a` lands in the team position and `b` in the sender position, the reverse
    // of the event's own field order and of radioMessage's playback call. Shipped; see the header.
    if (localPlayback)
        playbackRadioMessage(channel, static_cast<ChannelMessageType>(0),
                             static_cast<int>(a), static_cast<int>(b), messageId);
    return true;
}

int  GameClient::getCurrentInputIndex() { return m_currentInputIndex; }      // : +0xCC

// Marks an ID ghostable on the client's sole connection, if present.
// The connection-ID argument is unused; objectId narrows to 16 bits without an object lookup.
void GameClient::confirmGhost(int, unsigned int objectId)
{
    if (!m_connection)                                                  // +0xC0
        return;
    m_connection->m_ghostManager->addGhostable(                         // +0x2C, then 5B7CC0
        static_cast<unsigned short>(objectId));
}
void GameClient::confirmReset(int) {}                                           // : empty
world::IObject* GameClient::createObjectOnAllClients(const world::IObjectTemplate*, Math::Vec3,
                                                     Math::Vec3, bool, int, int)
{
    return 0;                                                                   // : `xor eax,eax`
}

// The six send* slots - 4D2B40 (event only) and 4D2B70 (id, event), each folded three ways: with a
// NetClient AND a connection the event goes to the connection's GameEventManager::addEvent (PE
// +0x38); otherwise it is released (+0x08). The id is never read on the client.
void GameClient::sendGameEventToAllPlaying(GameEvent* event)
{
    if (m_netClient && m_connection)
        m_connection->m_gameEventManager->addEvent(event);
    else
        event->release();
}
void GameClient::sendGameEventIfPlaying(unsigned int, GameEvent* event)     { sendGameEventToAllPlaying(event); }
void GameClient::sendGameEventToAll(GameEvent* event)                       { sendGameEventToAllPlaying(event); }
void GameClient::sendGameEvent(unsigned int, GameEvent* event)              { sendGameEventToAllPlaying(event); }
void GameClient::sendGameEventToAllConnected(GameEvent* event)              { sendGameEventToAllPlaying(event); }
void GameClient::sendGameEventIfConnected(unsigned int, GameEvent* event)   { sendGameEventToAllPlaying(event); }
void GameClient::sendMissileMessageToOccupyingPlayers(world::IObject*, world::MissileLockMessage) {}   // : empty

// Enter a vehicle on the client, searching its parents for the controlling object.
// Null targets return false. A player currently on a ladder detaches through the
// soldier path; a successful entry cancels pending exit and updates camera/control
// state. Maintain-camera entry preserves only the middle orientation component.
// [RETIRE-WHEN: client-entervehicle-seams] Exercise live parachute deployment
// and death on a ladder.
bool GameClient::enterVehicle(world::IPlayer* player, world::IObject* object, bool setAsDefaultVehicle)
{
    typedef world::object_detail::LayoutAccess L;

    // 1. the control object at or above the target. A null target simply exhausts the loop.
    world::IPlayerControlObject* pco = 0;
    for (world::IObject* node = object; node; node = L::parent(node))       // m_parent, +0x28
    {
        pco = static_cast<world::IPlayerControlObject*>(
                  node->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }
    if (!pco)
        return false;

    // Handle ladder exit before entering the next vehicle; preserve both getVehicle calls.
    if (player->getVehicle() &&
        player->getVehicle()->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
    {
        // Detach the default soldier from its ladder before entering another vehicle.
        // The default vehicle must exist and implement ISoldier; the ladder id must be registered.
        // Failed assertions break into the debugger and do not provide a recovery path.
        world::ISoldier* const soldier = static_cast<world::ISoldier*>(
            player->getDefaultVehicle()->queryInterface(world::IID_ISoldier));  // +0xA0, ds:91C45C
        if (!soldier)
            __debugbreak();                                  // retail INT3, no recovery
        world::Ladder* const ladder =
            world::ladderMap[static_cast<int>(soldier->getLadderId())];     // ISoldier +0x7C, 4D6780
        if (!ladder)
            __debugbreak();                                  // retail INT3, no recovery
        ladder->detachSoldier(soldier);
    }

    world::IPlayerControlObjectTemplate* const pcoTemplate =
        static_cast<world::IPlayerControlObjectTemplate*>(
            object->getTemplate()->queryInterface(world::IID_IPlayerControlObjectTemplate));

    // 3. the outgoing camera's orientation and view mode.
    world::ICameraObject* oldCamera = 0;
    float negatedY = 0.0f, negatedX = 0.0f, negatedZ = 0.0f;
    if (world::IObject* const defaultVehicle = player->getDefaultVehicle())      // +0xA0
    {
        // NOT NULL-tested between the two calls, exactly as shipped (4D68F9 -> 4D6900).
        world::IPlayerControlObject* const defaultPco =
            static_cast<world::IPlayerControlObject*>(
                defaultVehicle->queryInterface(world::IID_IPlayerControlObject));
        oldCamera = defaultPco->getActiveCamera();                              // ipco +0x44
        if (oldCamera)
        {
            world::IObject* const cameraObject = static_cast<world::IObject*>(
                oldCamera->queryInterface(world::IID_IObject));
            float rotZ = 0.0f, rotX = 0.0f, rotY = 0.0f;
            getRotationZXY(cameraObject->getRelativeTransformation(), rotZ, rotX, rotY);   // slot 58
            negatedY = -rotY;
            negatedX = -rotX;                   // the only one that survives - see the note
            negatedZ = -rotZ;
        }
    }
    (void) negatedY;                            // computed and discarded, as shipped
    (void) negatedZ;
    const float carriedRotation = negatedX;     // 's `[ebp-0x18 + 4]` latch

    // Uses view mode 8 when there is no outgoing camera.
    int viewMode = 8;
    if (oldCamera)
        viewMode = static_cast<int>(oldCamera->getViewMode());                  // ICamera +0x1C

    // 5. the object being left is told what is being entered - onEnter, on the OLD control object,
    //    told the NEW target. The name reads backwards; it is what both ports do.
    world::IPlayerControlObject* oldPco = 0;
    for (world::IObject* node = player->getVehicle(); node; node = L::parent(node))
    {
        oldPco = static_cast<world::IPlayerControlObject*>(
                     node->queryInterface(world::IID_IPlayerControlObject));
        if (oldPco)
            break;
    }
    if (oldPco)
        oldPco->onEnter(object);                                               // ipco +0x28

    pco->enter(player);                                                        // ipco +0x1C

    // Reread the vehicle after enter before assigning the default.
    if (setAsDefaultVehicle)
        player->setDefaultVehicle(player->getVehicle());                       // +0x9C

    // 7. physics and the input map - local players only. A remote player skips the whole block.
    if (!player->getIsRemote())                                                // +0x58
    {
        disablePhysics(player->getVehicle(), false);
        enableObject(object, false);                                           // bool dead

        if (pcoTemplate)
        {
            // getControlsCategory() IS asked twice on the non-4 path, as shipped, and category 4
            // means "ask the vehicle category instead".
            const int category = (pcoTemplate->getControlsCategory() == 4)
                                     ? pcoTemplate->getVehicleCategory()       // +0x24
                                     : pcoTemplate->getControlsCategory();     // +0x2C

            // A live child soldier selects infantry controls regardless of vehicle category.
            const char* controlMap = "LandPlayerInputControlMap";
            if (pco->isChildSoldierActive())                                   // ipco +0x140
                controlMap = "InfantryPlayerInputControlMap";
            else if (category == 2)
                controlMap = "AirPlayerInputControlMap";
            else if (category == 3)
                controlMap = "HelicopterPlayerInputControlMap";
            else if (category == 1)
                controlMap = "SeaPlayerInputControlMap";
            static_cast<io::IInputManager*>(io::inputManager)
                ->setPlayerControlMap(std::string(controlMap));                // +0x2C
        }
    }

    // 6. carry the orientation across, when the template asks for it.
    world::ICameraObject* const newCamera = pco->getActiveCamera();            // ipco +0x44
    if (newCamera && pcoTemplate && pcoTemplate->getMaintainCameraOnEnter())   // +0x108
    {
        // A GSDeveloper setting changes behaviour here, and only for something that is NOT the
        // player's own free camera: with it on, the new camera is forced into the OLD camera's view
        // mode. Off - the shipped default - the view mode is left alone.
        bool carryViewMode = false;
        if (settings->boolGet(std::string("GSDeveloper"), false))              // +0x50
            carryViewMode = (object != player->getFreeCamera());               // +0xA4
        if (carryViewMode)
            newCamera->setViewMode(static_cast<world::CameraViewMode>(viewMode), 0, false);   // +0x18

        world::IObject* const cameraObject = static_cast<world::IObject*>(
            newCamera->queryInterface(world::IID_IObject));

        // Client entry preserves only pitch, with the opposite sign from the server's
        // three-axis carried rotation. Keep the image-specific transformation.
        Vec3 carried;
        carried.x = 0.0f;
        carried.y = carriedRotation;
        carried.z = 0.0f;
        {
            Mat4 transform = L::parent(cameraObject)
                                 ? cameraObject->getRelativeTransformation()    // slot 58, PE +0xE4
                                 : cameraObject->getAbsoluteTransformation();   // slot 33, PE +0x80
            setRotationZXY(transform, -carried.z, -carried.y, -carried.x);
            if (L::parent(cameraObject))
                cameraObject->setRelativeTransformation(transform);             // slot 59, PE +0xE8
            else
                cameraObject->setAbsoluteTransformation(transform);             // slot 35, PE +0x88
        }

        // maintainCameraOnEnter requires an active camera derived from RotationalBundle.
        // The cast is unchecked; stock parachutes supply the occupant's Camera, while FreeCamera
        // does not satisfy this contract. Preserve only the carried yaw in the reconstructed rotation.
        Vec3 currentRotation;
        currentRotation.x = 0.0f;
        currentRotation.y = carriedRotation;
        currentRotation.z = 0.0f;
        static_cast<world::Camera*>(cameraObject)->setCurrentRotation(currentRotation);   // PE +0x178

        // Updates the occupying soldier's look angle. Preserve both ISoldier queries
        // and use native base adjustment for the concrete call.
        if (world::IObject* const occupied = pco->getOccupingObject())                  // ipco +0x14
        {
            if (occupied->queryInterface(world::IID_ISoldier))
                static_cast<world::Soldier*>(static_cast<world::ISoldier*>(
                    occupied->queryInterface(world::IID_ISoldier)))
                        ->setLookUpDownAngle(carriedRotation);
        }
    }

    eventManager->postEvent(ECGame, 9, player, 0.0f);                          // +0x10

    // Cancel a pending exit for the local human player.
    if (player == world::playerManager->getLocalHumanPlayer())                 // +0x30
        static_cast<ClientGameLogic*>(gameLogic)->clearPendingExit();          //  +0x304

    return true;                                                               // `mov al, 1`
}

// Requests player-id exit with the explicit-release flag true. Requires a player
// and ignores both incoming Boolean arguments.
bool GameClient::exitVehicle(world::IPlayer* player, bool, bool)
{
    return static_cast<IGameClient*>(this)->exitVehicle(player->getId(), true);   // +0x50, then +0x38
}

// listPlayers - 4D38E0, 229 B. Over PlayerManager::getPlayersSortedByScore (+0x40): "id:%d " +
// name + " is remote." / " is local." + (" is an AI bot.") + "\n" per player.
std::string GameClient::listPlayers() const
{
    std::string out;
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayersSortedByScore();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        out += formatString("id:%d ", player->getId());
        out += player->getName();
        if (player->getIsRemote())
            out += " is remote.";
        else
            out += " is local.";
        if (player->getIsAIPlayer())
            out += " is an AI bot.";
        out += "\n";
    }
    return out;
}

// recordDemo - 4D25F0, 50 B: the console refusal. stopRecording - 83D270: empty.
void GameClient::recordDemo(DemoRecorder*)
{
    io::mainConsole->output(std::string("can't record demos as a client\n"));   // OldConsole PE +0x10
}
void GameClient::stopRecording() {}

// isPlayingDemo - 4D2630, 27 B: the engine's Demo object and its playback flag (+0x10).
bool GameClient::isPlayingDemo()
{
    Demo* const demo = bf2Engine->demo;
    return demo != 0 && demo->isPlayback;
}

// setSimulationRate - 4D3110, 62 B: only while a demo plays - a rate other than 100 runs
// IBfSound PE +0xC4, then 5965B0(rate) (the recorder's rate).
void GameClient::setSimulationRate(int rate)
{
    (void) rate;
    if (!m_demoRecorder)
        return;
    GAMECLIENT_FRONTIER("setSimulationRate", "DemoRecorder playback predicate and rate (54B8F0 / 5965B0) are unmapped");
}
int  GameClient::getSimulationRate() { return g_system->getSimulationRate(); }   //  (11 B, shared with GameServer)
void GameClient::debugShowActiveGhosts(int) {}                                  // : empty
const io::BasicNetworkStats* GameClient::getNetworkStats() const { return m_networkableStats; }

// sendUDPPacket - 4D2840, 23 B (and IGameClient +0x7C through 4D2B00): with a NetClient,
// (address, port, data, size) - its mutex-guarded socket send. Both tables land here.
void GameClient::sendUDPPacket(const char* address, unsigned short port, void* data, int size)
{
    if (!m_netClient)
        return;
    m_netClient->sendRawPacket(address, port, data, size);
}

// Converts text to wide characters and posts server chat for team zero.
// Requires a live HUD manager.
void GameClient::addTextToChatWindow(const char* text)
{
    const std::string narrow(text);
    hudManager->addServerChat(toWideString(narrow), 0);
}
bool GameClient::isPunkBusterEnabled() const { return m_punkBuster; }         // : +0x159
StatsMonitor* GameClient::getStatsMonitor() const { return m_statsMonitor; }  // : +0x14C

// Clears player visual effects, destroys/exits the current vehicle, announces the
// disconnect, unregisters network/player state, and deletes the player. Null is ignored;
// vehicle transitions retain debugger breaks when the expected free camera is absent.
void GameClient::destroyPlayer(world::IPlayer* player)
{
    if (!player)
        return;
    player->disableBlurryVision();                   // PE +0x2EC
    player->disableFlashBangEffect();                // PE +0x2E8
    if (world::IObject* vehicle = player->getVehicle())            // PE +0x44
    {
        if (vehicle != player->getFreeCamera() || player->getIsAlive())    // PE +0xA4 / +0x68
            gameLogic->killPlayer(player, true, false);                    // +0x6C
        if (world::IObject* still = player->getVehicle())
        {
            if (still != player->getFreeCamera())
                __debugbreak();                      // retail INT3
            game->destroyObject(still);              // IGame PE +0x9C
            game->exitVehicle(player, true, true);   // IGame PE +0xA4
        }
    }
    if (world::IObject* remaining = player->getVehicle())
    {
        if (remaining->getTemplate()->getClassID() != world::CID_FreeCameraTemplate)
            __debugbreak();                          // retail INT3
    }
    if (hudManager)
    {
        // BF2.exe 0x4D3A91..0x4D3B3F. Keep the nested value calls: retail constructs the player
        // name replacement first, then the token and localization key, and consumes each returned
        // wide string in the next typed HUD call before finally posting the message.
        hudManager->addTeamChat(
            hudManager->replaceWideTokenFromString(
                hudManager->replaceWideInputKeys(
                    hudManager->getLocalizedString(
                        std::string("GAME_MESSAGE_PLAYER_DISCONNECTED"))),
                std::string("#PLAYERNAME#"),
                std::string(player->getName().c_str())),
            nullptr,
            0);
    }
    if (io::INetworkable* networkable = player->getNetworkable())  // PE +0x18C
        io::networkManager->removeNetworkable(networkable->m_networkableId);   // : `movzx ecx, word ptr [eax+4]`
    world::playerManager->removePlayer(player);      // +0x14
    delete player;                                   // : the deleting destructor through the vtable
}

// Acknowledges an input index and replays newer local actions over received server state.
// The tick resets world time; each replay advances it. Repeated state and vehicle queries
// retain their side effects. This implementation zero-initializes the full PlayerInput,
// including trailing bytes the retail client leaves uninitialized.
bool GameClient::handleControlObjectState(io::BitStream*, unsigned short networkableId,
                                          int acknowledgedIndex, unsigned int tick, float deltaTime)
{
    m_fieldF4 = acknowledgedIndex;                                  // view +0x4C = +0xF4

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (!local)
        return false;

    ActionBuffer* const buffer = local->getActionBuffer();          // IPlayer PE +0x94
    buffer->removeOlderThanIndex(acknowledgedIndex);

    if (io::networkManager->getObject(networkableId)
        && m_useClientSidePrediction                                // view +0x55 = +0xFD
        && !buffer->m_entries.empty())                              // the list size at +0x08
    {
        WorldPref::resetTickCount(tick);
        buffer->popFront();
        io::networkManager->getDescriptor(networkableId);           // result discarded

        for (std::list<ActionBufferEntry>::const_iterator it = buffer->m_entries.begin();
             it != buffer->m_entries.end(); ++it)
        {
            const int status = gameLogic->getGameStatus();          // +0x158
            if (status == 1)
            {
                // the PE passes the status as the delta (`mov ecx, eax`), which is 1 on this arm.
                WorldPref::advanceTickCount(static_cast<unsigned int>(status));
            }

            PlayerAction action;
            action.set(it->m_action);

            io::PlayerInput input = io::PlayerInput();               // see the note on the tail
            action.get(input);
            m_currentInputIndex = action.m_inputIndex;              // view +0x24 = +0xCC

            if (gameLogic->getGameStatus() != 4)                    // +0x158, asked again
            {
                gameLogic->checkPlayerTriggers(local, local->getVehicle(), input, true);   // +0x154
                local->handleInput(input, deltaTime, 4);            // IPlayer PE +0x3C
            }

            performHandleUpdate(deltaTime, local->getVehicle(), 4, false);
            performMobilePhysicsUpdate(deltaTime, local->getVehicle(), false);
            world::objectManager->updateObjectsInGrid();             // +0xA4
            world::responsePhysicsManager->reset();                  // PE +0x20
            performMobileCollisionUpdate(deltaTime, local->getVehicle(), false);
        }
    }

    return true;                                                     // `mov al, 1`
}

// Prepares demo playback with infantry controls, a mode-2 connection, static
// networkables, and client statistics. Returns false if connection setup fails.
bool GameClient::playDemo(DemoRecorder* recorder)
{
    // ..4D41EA: io::inputManager->+0x2C("InfantryPlayerInputControlMap") - the tree's
    // InputManager carries no such slot yet.
    GAMECLIENT_FRONTIER("playDemo", "io::InputManager PE +0x2C (the control-map selector) is unmapped");
    m_demoRecorder = recorder;
    world::objectManager->destroyObjects();
    m_netClient = 0;
    m_bitStream = new (std::nothrow) io::BitStream(0, 0);
    if (!m_bitStream)
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 2468, "") << formatString("failed to create bitstream");
    m_connection = new (std::nothrow) ClientConnection(static_cast<ConnectionDemoMode>(2));
    if (!m_connection)
    {
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 2475, "") << formatString("Failed to create clientConnection");
        return false;
    }
    m_connection->setConnectionId(0);
    setConnectionType(m_connectionType);
    m_isRunning = false;
    m_fieldF4   = -1;
    m_connectionState = 0;
    m_serverAddress = "demo_playback";
    registerStaticNetworkables();
    m_networkableStats = new (std::nothrow) io::BasicNetworkStats;   // the PE news 0x88 bytes through 6BB270 - a
                                                                     // client-side derivative with four more dwords
                                                                     // and its own table (9135C8); [UNRESOLVED] type
    if (!m_networkableStats)
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 2494, "") << "Failed to create networkableStats";
    return true;
}

// Build CD-key responses to the server challenge, adding a second response for
// an expansion product. Requires a connection, event manager and free allocation.
// Missing keys request disconnect; allocated responses are not freed on that path.
// Challenge generation must supply its terminator within the ten-byte buffer.
// The response field is XORed as received; this constructor's zero initialization
// differs from retail's uninitialized field, which the server does not read.
void GameClient::handleChallenge(ChallengeEvent* event)
{
    // 1..4 - the base game's answer. No null test on the allocation; see the block above.
    ChallengeResponseEvent* const response = new (std::nothrow) ChallengeResponseEvent();
    response->m_netVersion = static_cast<unsigned int>(BuildNrUtil::getNetVersionNumber());
    response->m_field58   ^= *reinterpret_cast<const unsigned int*>(event->m_challengeString);  // `^=`, as shipped
    response->m_gameId     = gamespy::g_gameID;                        // : ds:8E66BC

    // 5 - 4D1D40. The client keeps the server's challenge on its own connection.
    m_connection->initChallengeString(event->m_challengeString);

    // 6 - the base game's key: hklm\software\Electronic Arts\EA Games\Battlefield 2\ergc.
    const std::string cdKey = translateCdKey(getRegistryCdKey(std::string("")));
    if (cdKey.empty())
    {
        requestDisconnect(7);                   // : the +0x120 slot, virtual as shipped
        return;                                 // `response` leaks here
    }

    // 7 - the expansion, if this server runs one. Only "xpack" answers with a product name.
    const std::string expansion = getExpansionProductName(std::string(event->m_modName));
    std::string expansionKey;

    if (!expansion.empty())
    {
        const std::string storedExpansionKey = getRegistryCdKey(expansion);
        if (!storedExpansionKey.empty())
            expansionKey = translateCdKey(storedExpansionKey);

        //  / 4D1E0F - both are tested, in this order, and either one disconnects.
        if (expansionKey.empty() || storedExpansionKey.empty())
        {
            requestDisconnect(7);
            return;                             // `response` leaks here too
        }
    }

    // 8 - 4D1E32 / 4D1E3A. MD5(key) in the first 32 bytes, the random tag in the next 8, and
    // MD5(key + random + challenge) in the last 32: 73 bytes with the terminator.
    gcd_compute_response(const_cast<char*>(cdKey.c_str()),
                         event->m_challengeString,
                         reinterpret_cast<char*>(response->m_response),
                         0);                                           // CDResponseMethod_NEWAUTH
    m_connection->m_gameEventManager->addEvent(response);              // PE +0x38 (ELF +0x3C)

    // 9 - 4D1E4D onwards. A whole second event, built the same way, for the expansion's product id.
    if (!expansion.empty())
    {
        ChallengeResponseEvent* const expansionResponse = new (std::nothrow) ChallengeResponseEvent();
        expansionResponse->m_netVersion =
            static_cast<unsigned int>(BuildNrUtil::getNetVersionNumber());   // called again
        expansionResponse->m_field58   ^= *reinterpret_cast<const unsigned int*>(event->m_challengeString);
        expansionResponse->m_gameId     = getExpansionGameId(std::string(event->m_modName));

        gcd_compute_response(const_cast<char*>(expansionKey.c_str()),
                             event->m_challengeString,
                             reinterpret_cast<char*>(expansionResponse->m_response),
                             0);
        m_connection->m_gameEventManager->addEvent(expansionResponse);
    }
}

// Starts a connection attempt and creates client session state on success.
// Returns 1 for an initiated join, translated failure codes otherwise; an existing NetClient refuses retry.
// Timeout seconds are truncated to milliseconds before forwarding.
int GameClient::joinServer(const std::string& address, int port, const std::string& password,
                           float timeoutSeconds, bool punkBuster)
{
    if (m_netClient)
        return 0;
    world::objectManager->destroyObjects();          // : ObjectManager PE +0x98
    m_netClient = new (std::nothrow) io::NetClient();   // operator new(0x158) + 6BE4D0
    m_netClient->setVersion(static_cast<unsigned int>(BuildNrUtil::getNetVersionNumber()));   // : +0x44 - on the pointer before its null test, as shipped
    if (!m_netClient)
        return 0;
    m_netClient->init(true);                         // : +0x10(1) - the code is dropped, as shipped
    m_connectionState = 3;
    m_punkBuster = punkBuster;
    ui::ISwiffHost* const host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    const int code = m_netClient->joinServer(address, port, true, password,
                                             static_cast<unsigned int>(timeoutSeconds * 1000.0f),
                                             std::string(""), punkBuster);   // : +0x14
    if (code)
    {
        if (code == 36)
            host->setServerMod(m_netClient->getDisconnectText());   // +0x350 of +0x70
        if (m_netClient)
            delete m_netClient;                      // : the +0x74 deleting destructor
        m_netClient = 0;
        m_connectionState = 4;
        switch (code)
        {
        case 17: return 2;
        case 30: return 3;
        case 24: return 5;
        case 23: return 4;
        case 2:  return 6;
        case 31: return 7;
        case 36: return 8;
        case 22: return 9;
        default: return 0;
        }
    }
    m_bitStream = new (std::nothrow) io::BitStream(0, 0);
    if (!m_bitStream)
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 608, "") << formatString("failed to create bitstream");
    m_connection = new (std::nothrow) ClientConnection(static_cast<ConnectionDemoMode>(0));
    if (!m_connection)
    {
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 615, "") << formatString("Failed to create clientConnection");
        return 0;
    }
    m_connection->setConnectionId(0);
    setConnectionType(m_connectionType);
    m_isRunning       = false;
    m_fieldF4         = -1;
    m_port            = port;
    m_connectionState = 0;
    m_serverAddress   = address + formatString(":%d", port);
    registerStaticNetworkables();                    // Game:: 4405C0
    m_networkableStats = new (std::nothrow) io::BasicNetworkStats;   // 0x88 bytes through 6BB270 in the PE - a
                                                                     // client-side derivative; [UNRESOLVED] type
    if (!m_networkableStats)
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 635, "") << "Failed to create networkableStats";
    return 1;
}

IClientConnection* GameClient::getConnection() { return m_connection; }       // : +0xC0

// Create or reuse the object for a server-assigned network id. An active cached
// object is returned unchanged. Zipline rotation carries a second endpoint.
// Fresh non-kit objects start with ghosting disabled until server replication
// activates them; a missing networkable still permits returning the object.
// Missing templates and mismatched ids produce fatal diagnostics.
world::IObject* GameClient::createNetworkedObject(const world::IObjectTemplate* objectTemplate,
                                                  unsigned short networkableId,
                                                  const Vec3* position, const Vec3* rotation)
{
    if (!objectTemplate)
        return 0;

    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: objectdb-trace] - off unless BF2_TRACE_OBJECTDB
    // is set, in the same shape as BF2_TRACE_CONSOLE / BF2_TRACE_MUSIC. The object database arrives
    // as a burst of these and nothing else on the client says so.
    static int s_objectDbTrace = -1;
    static int s_objectDbCount = 0;
    if (s_objectDbTrace < 0)
        s_objectDbTrace = std::getenv("BF2_TRACE_OBJECTDB") ? 1 : 0;

    // The cached object already standing in for this id is the answer.
    world::IObject* object = io::networkManager->getObject(networkableId);
    if (object && object->getObjectCacheID() != 0                                   // +0x28
        && !(object->getFlags() & world::IObject::OFIsInObjectCache))               // +0x04, bit 5
    {
        return object;
    }

    Mat4 transform = Mat4::getIdentity();
    if (objectTemplate->getClassID() == world::CID_ZiplineTemplate)                 // +0x10, ds:91C3B0
    {
        transform.m[4] = rotation->x;                   // the zipline's second point, row 1
        transform.m[5] = rotation->y;
        transform.m[6] = rotation->z;
    }
    else
        setRotationZXY(transform, -rotation->z, -rotation->y, -rotation->x);
    transform.m[12] = position->x;
    transform.m[13] = position->y;
    transform.m[14] = position->z;

    object = world::objectManager->createObject(objectTemplate, networkableId, &transform);   // +0x10
    if (!object)
        return 0;

    object->updateFlags(0x20000u, 0);                                               // +0x5C
    resetPhysics(object, true);

    io::INetworkable* const networkable = object->getNetworkable();                 // +0xD4
    if (!networkable)
        return object;                                  // still a success to the caller

    if (networkable->m_networkableId == 0)              // 0 = not registered yet
    {
        networkable->m_isInNetwork = false;             // +0x14 - "the other side owns this"
        // getNetworkable again, as shipped.
        io::networkManager->addNetworkable(object->getNetworkable(), networkableId);
    }

    // The object's OWN template, re-read from +0x14, not the argument.
    if (object->getTemplate()->getClassID() != world::CID_KitTemplate)              // ds:91C244
    {
        disableGhosting(object, true);
        io::networkManager->removeActiveDescriptor(networkable->m_networkableId);
        // A negative base priority is c_NIGhostAlways (-1.0f) - a static/always ghost is
        // exactly the case that is NOT disabled. NaN takes the same arm as negative, as the
        // shipped `fcomp` + `test ah, 1` does.
        if (networkable->m_basePriority >= 0.0f)                                    // +0x10 vs ds:93C850
            disableObject(object);
    }

    if (s_objectDbTrace)
    {
        std::fprintf(stderr, "[OBJECTDB] #%d templateId=%u netId=%u -> object %p (nw id %u)\n",
                     ++s_objectDbCount, static_cast<unsigned>(objectTemplate->getId()),
                     static_cast<unsigned>(networkableId), static_cast<void*>(object),
                     static_cast<unsigned>(networkable->m_networkableId));
        std::fflush(stderr);
    }

    if (networkable->m_networkableId != networkableId)
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 1693, "")
            << "Networkable IDs not same, server:" << networkableId
            << " client:" << networkable->m_networkableId;

    return object;
}

bool GameClient::createObject(unsigned int templateId, unsigned short networkableId,
                              const Vec3* position, const Vec3* rotation)
{
    const world::IObjectTemplate* const objectTemplate =
        world::objectTemplateManager->getTemplate(templateId);                      // PE +0x24
    if (!objectTemplate)
    {
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 1587, "") << "Template id " << templateId << " not found";
        return false;
    }
    return createNetworkedObject(objectTemplate, networkableId, position, rotation) != 0;
}

// Creates a networked kit and applies its unlock level. Ignores the creation index.
// Requires successful creation of a Kit; the unlock update has no null or class check.
bool GameClient::createKit(unsigned int templateId, unsigned short networkableId,
                           unsigned int /*creationIndex - DEAD, see above*/, int unlockLevel,
                           const Vec3* position, const Vec3& rotation)
{
    const world::IObjectTemplate* const objectTemplate =
        world::objectTemplateManager->getTemplate(templateId);                      // PE +0x24
    if (!objectTemplate)
    {
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 1620, "") << "Template id " << templateId << " not found";
        return false;
    }
    world::IObject* const object =
        createNetworkedObject(objectTemplate, networkableId, position, &rotation);
    if (unlockLevel != -1)
    {
        world::Kit* const kit = static_cast<world::Kit*>(object);                  // unguarded
        kit->m_chosenUnlockLevel = unlockLevel;                                    // +0x18C
        kit->updateUnlockStatus();
    }
    return object != 0;
}

// destroyPlayer(id) - 4D1F00, 56 B: the player by id, then Game's destroyPlayer(IPlayer*) through
// the primary table (+0x11C). No player answers false.
bool GameClient::destroyPlayer(int playerId)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return false;
    destroyPlayer(player);
    return true;
}

// destroyObject(id) - 4D1F40, 61 B: the object behind the networkable id (6A6C10) is destroyed
// through IGame::destroyObject (PE +0x9C) and ObjectManager +0x98 flushes; always true.
bool GameClient::destroyObject(unsigned short networkableId)
{
    if (world::IObject* object = io::networkManager->getObject(networkableId))
    {
        game->destroyObject(object);
        world::objectManager->destroyObjects();
    }
    return true;
}

// Resolve the player and network object, record the object in the player's free
// camera trace handle, then enter it. Missing players or objects return false.
// The player must have a free camera; low network ids report its absence as fatal,
// while higher ids reach the same unchecked camera access.
bool GameClient::enterVehicle(int playerId, unsigned short networkableId, bool flag)
{
    // This is the body between a done click and a soldier.
    // PlayerManager has NO destructor slot in its table - the IBase quartet runs straight into
    // addPlayer at +0x10 - so the one-slot shift that applies to IPlayer does NOT apply here, and
    // the client's +0x20 is this tree's +0x20.
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return false;

    if (networkableId < 0x282 && !player->getFreeCamera())
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 1944, "") << "TBM: no freeCamera for player "
                                                      << playerId;

    world::IObject* const object = io::networkManager->getObject(networkableId);
    if (!object)
        return false;

    // Requires a free camera; records the entered object through a weak reference
    // so the camera can follow it. No null guard is present.
    static_cast<world::FreeCamera*>(player->getFreeCamera())->m_traceObject = object->getWeakPtr();

    return game->enterVehicle(player, object, flag);   // client +0xA0 = this tree's +0xA4
}

//  IS already IN this tree - `world::findOwningPlayerControlObject` (Game/Objects/
// IObject.h, PE 0x440060, written). An earlier draft of this block re-implemented it in an
// anonymous namespace, which is exactly the duplication IObject.h's own note warns about for two
// other files. Use the shared declaration.
namespace {
// [INFERRED] Shared exit helper. Prefer a soldier's used vehicle, then search the
// object's own PCO chain. exitPlayerInstead selects exitPlayer rather than exit.
void releaseFromPlayerControlObject(world::IObject* object, bool exitPlayerInstead)
{
    if (!object)
        return;

    world::IPlayerControlObject* pco = 0;
    if (void* const soldier = object->queryInterface(world::IID_ISoldier))                   // 50391
        pco = world::findOwningPlayerControlObject(
            static_cast<world::ISoldier*>(soldier)->getUsedVehicle());                // PE +0x30
    if (!pco)
        pco = world::findOwningPlayerControlObject(object);
    if (!pco)
        return;

    // The occupant's own PCO is told it exited before this one is exited - two different objects.
    if (world::IPlayerControlObject* const occupant =
            world::findOwningPlayerControlObject(pco->getOccupingObject()))                  // +0x14
        occupant->onExit();                                                           // +0x2C

    if (exitPlayerInstead)
        pco->exitPlayer();                                                            // +0x24
    else
        pco->exit();                                                                  // +0x20
}
}

// Exits the selected player's vehicle and reenables physics for local players.
// Keep separate getVehicle calls: releaseFromPlayerControlObject can change the
// answer midway, and the initial call's result is intentionally discarded.
bool GameClient::exitVehicle(int playerId, bool exitPlayerInstead)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);   // +0x20
    if (!player)
        return false;

    player->getVehicle();                                                             // discarded

    // The two container hand-offs. A ladder is released through the PCO pair; a zipline is exited
    // directly, without the occupant's onExit.
    if (player->getVehicle()
            && player->getVehicle()->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
        releaseFromPlayerControlObject(player->getVehicle(), false);
    if (player->getVehicle()
            && player->getVehicle()->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate)
        world::findOwningPlayerControlObject(player->getVehicle())->exit();                   // unguarded

    world::IObject* const vehicle = player->getVehicle();
    if (vehicle && !player->getIsRemote())                                             // +0x58
        reenablePhysics(vehicle, false);
    if (!vehicle)
        return false;

    // A vehicle with no PlayerControlObject anywhere above it leaves through a bare false,
    // before anything is released and before the event is posted.
    if (!world::findOwningPlayerControlObject(vehicle))
        return false;

    releaseFromPlayerControlObject(vehicle, exitPlayerInstead);

    if (player->getVehicle())
    {
        if (player->getIsRemote() || !player->getVehicle())
        {
            // Freeze a remote player's current vehicle until its next ghost update when ghosting
            // is inactive. Re-read the vehicle after exit; this path requires a nonnull result
            // and preserves the repeated weak-reference lookups.
            if (io::INetworkable* const networkable = player->getVehicle()->getNetworkable())   // +0xD4
            {
                if (!networkable->m_ghostActive)                                            // +0x0C
                    disableObject(player->getVehicle(), false);
            }
        }
        else
        {
            disablePhysics(player->getVehicle(), false);
            resetPhysics(player->getVehicle(), false);
            // The camera follows what was just left - the same store enterVehicle makes, and the
            // measured PE +0x2EC / ELF +0x28C member. See FreeCamera.h.
            static_cast<world::FreeCamera*>(player->getFreeCamera())->m_traceObject =
                vehicle->getWeakPtr();
        }
    }

    // Restore infantry controls when the local player leaves a vehicle.
    if (!player->getIsRemote())
        static_cast<io::IInputManager*>(io::inputManager)
            ->setPlayerControlMap(std::string("InfantryPlayerInputControlMap"));        // +0x2C

    eventManager->postEvent(ECGame, 11, player, 0.0f);

    if (player == world::playerManager->getLocalHumanPlayer())                          // +0x30
        static_cast<ClientGameLogic*>(gameLogic)->clearPendingExit();                   // +0x304

    return true;
}

// getConnectionStatus - 4D25D0, 24 B: NetClient +0x6C(out) when there is one, else false.
bool GameClient::getConnectionStatus(io::ConnectionStatus& out)
{
    if (!m_netClient)
        return false;
    return m_netClient->getConnectionStatus(out);            // +0x6C
}

std::string GameClient::getServerAddress()     { return m_serverAddress; }     // : +0x104
std::string GameClient::getServerAddressCopy() { return m_serverAddress; }     // the folded twin, +0x78
void        GameClient::slot44(int)            {}                              // > nullsub_2

// The demo group - 4D2650 (109 B), 4D26C0 (57 B), 4D2700 (36 B), 4D2730 (39 B), 4D2760 (38 B),
//  (25 B), 4D3D60 (257 B). Every one reads DemoRecorder bytes by PE offset (+0xC4, +0xCD,
// +0xE8, +0xEC, +0x105 and the object at +0xE8) that the tree's ELF-recovered DemoRecorder does
// not map; a null recorder is the only arm transcribed.
void GameClient::restartDemo()
{
    if (!m_demoRecorder) return;
    GAMECLIENT_FRONTIER("restartDemo", "DemoRecorder PE members (+0xC4/+0xCD/+0x105) are unmapped");
}
void GameClient::setDemoPaused(bool)
{
    if (!m_demoRecorder) return;
    GAMECLIENT_FRONTIER("setDemoPaused", "DemoRecorder pause/resume (54BC90 / 54BCB0) are unmapped");
}
void GameClient::slot50()
{
    if (!m_demoRecorder) return;
    GAMECLIENT_FRONTIER("slot50", "DemoRecorder +0xE8 and 62AA10 are unmapped");
}
int GameClient::slot54()
{
    if (!m_demoRecorder) return -1;
    GAMECLIENT_FRONTIER("slot54", "DemoRecorder +0xE8 and 68E7B0 are unmapped");
}
bool GameClient::slot58()
{
    if (!m_demoRecorder) return false;
    GAMECLIENT_FRONTIER("slot58", "DemoRecorder +0xE8 (+0x10) is unmapped");
}
bool GameClient::isDemoPlaying()
{
    if (!m_demoRecorder) return false;
    GAMECLIENT_FRONTIER("isDemoPlaying", "DemoRecorder +0xEC is unmapped");
}
void GameClient::slot6C(int, int)
{
    GAMECLIENT_FRONTIER("slot6C", "LocalProfileManager +0xA4 and DemoLibrary +0x44 (the recorded-demo registration) are unmapped");
}

// togglePause - 4D2790: setPause(!getPause()), both through the +0xA8 table.
void GameClient::togglePause() { setPause(!getPause()); }
// setPause - 4D27B0, 71 B: pausing always sticks; unpausing only while no demo plays. The rate goes
// to 0 or 100 through IGame::setSimulationRate (primary +0xE0).
void GameClient::setPause(bool paused)
{
    if (paused || !isDemoPlaying())
    {
        m_paused = paused;
        setSimulationRate(paused ? 0 : 100);
    }
}
bool GameClient::getPause()               { return m_paused; }                 //  (folded): +0x144
bool GameClient::getDisconnectRequested() { return m_disconnectRequested; }    // : +0x120
bool GameClient::getIsReady()             { return m_isReady; }                // : +0x158

namespace {
// The payload pointer a PostRemoteEvent stores in its notification node (PE node +0x2C). The named
// member keeps the retail x86 offset and follows the native layout elsewhere.
#if defined(_M_IX86)
static_assert(offsetof(NotificationEventNode, m_data) == 0x2C, "retail PE notification payload");
#endif
inline void* remoteEventPayload(EventNode* node)
{
    return static_cast<NotificationEventNode*>(node)->m_data;
}
} // namespace

// Dispatches client core/network events for join progress, player state, chat and object removal.
// Database acknowledgements preserve network-ID regression order; mismatched IDs are fatal.
// Event-specific node and data payloads must match the selected ID.
void GameClient::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    if (category == ECCore)
    {
        if (id == 1)
        {
            if (node && *reinterpret_cast<unsigned int*>(node) == 1007)
            {
                if (world::playerManager->getLocalHumanPlayer() && m_netClient)
                    GAMECLIENT_FRONTIER("handleEvent", "ECCore 1/1007: the ChangePlayerNameEvent resend (53D2E0) through the connection is unmapped");
            }
        }
        else if (id == 14)
            m_abortLoadingLevel = true;
        return;
    }
    if (category != ECNetwork)
        return;
    switch (id)
    {
    case 1:
        handleDataBlock(node, data);
        break;
    case 3:
    {
        m_isRunning = true;
        int type = 5;
        if (!isDedicated())
        {
            // Requires the registered GeneralSettings singleton to read the connection type.
            GeneralSettings* const generalSettings = static_cast<GeneralSettings*>(
                classManager->getSingleton(std::string("GeneralSettings")));    // PE +0x48
            type = generalSettings->getConnectionType();                        // +0x100
        }
        setConnectionType(type);
        break;
    }
    case 4:
    {
        // Send remote readiness without constructor-time local execution. A failed
        // nothrow allocation is passed to sendGameEvent as null.
        GameEvent* const ready =
            new (std::nothrow) PostRemoteEvent(ECNetwork, 4, false, 0, 0, 0.0f);   // 0x54
        sendGameEvent(0xFFFFFFFFu, ready);                                         // PE +0x8C = ELF +0x90
        if (isLightmapRenderingNode())
        {
            GameEvent* const lightmaps =
                new (std::nothrow) PostRemoteEvent(ECNetwork, 16, false, 0, 0, 0.0f);
            sendGameEvent(0xFFFFFFFFu, lightmaps);
        }
        // The store runs on both paths - the lightmap test's `je` jumps straight to it.
        m_isReady = true;                                                          // +0x158
        break;
    }
    case 5:
    {
        // Rewind the id allocator before comparing the highest used network id with the
        // server's watermark. The rewind result is discarded; the separate query can return -1.
        io::networkManager->regressNetworkableIds();                    // discarded
        const int theirs = *static_cast<const int*>(remoteEventPayload(node));   // node +0x2C
        const int ours   = io::networkManager->getHighestUsedNetID();
        if (theirs != ours)
            Debug(DEBUGTYPE_7, GAMECLIENT_FILE, 1404, "")
                << "Networkable RegressionStatus invalid c:" << ours
                << " != s:" << theirs;
        GameEvent* const regressed =
            new (std::nothrow) PostRemoteEvent(ECNetwork, 5, false, 0, 0, 0.0f);   // 0x54
        sendGameEvent(0xFFFFFFFFu, regressed);                                     // PE +0x8C
        break;
    }
    case 6:
        if (world::playerManager->getLocalHumanPlayer())
            eventManager->postEvent(ECGame, 7, 0, 0.0f);
        break;
    case 9:
    {
        const int playerId = *static_cast<const int*>(remoteEventPayload(node));   // node +0x2C
        if (world::IPlayer* player = world::playerManager->getPlayerFromId(playerId))
        {
            player->setIsAlive(true);                // PE +0x64
            eventManager->postEvent(ECGame, 6, player, 0.0f);
        }
        break;
    }
    case 10:
    {
        const char* const payload = static_cast<const char*>(remoteEventPayload(node));   // node +0x2C
        const int  playerId = *reinterpret_cast<const int*>(payload);
        const bool flag     = payload[4] != 0;
        world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
        if (!player)
            break;
        // Exit a killed player's zipline before proceeding. Preserve three vehicle reads;
        // the owning PCO is required and not null-checked.
        if (player->getVehicle()
                && player->getVehicle()->getTemplate()->getClassID() == world::CID_ZiplineContainerTemplate)
            world::findOwningPlayerControlObject(player->getVehicle())->exit();      // +0x20
        player->setIsGasMaskOn(false, true);         // PE +0x2C0
        player->setNightVision(false, true);         // PE +0x2D8
        player->rechargeNightVision();               // PE +0x2DC
        player->disableFlashBangEffect();            // PE +0x2E8
        player->resetOnDeath();                      // PE +0x2F4
        gameLogic->killPlayer(player, false, flag);  // +0x6C
        break;
    }
    case 11:
    {
        // Forward the received string using typed fields; leave the event node unchanged.
        StringReceivedEvent* const e = static_cast<StringReceivedEvent*>(node);
        outputChatString(static_cast<TargetChannel>(e->m_channel),
                         static_cast<ChannelMessageType>(e->m_type),
                         e->m_senderId, e->m_targetId,
                         std::string(reinterpret_cast<const char*>(e->m_message), e->m_messageLength));
        break;
    }
    case 13:
    {
        // Play the received radio message through the shared local playback path.
        RadioMessageReceivedEvent* const e = static_cast<RadioMessageReceivedEvent*>(node);
        playbackRadioMessage(static_cast<TargetChannel>(e->m_channel),
                             static_cast<ChannelMessageType>(e->m_type),
                             e->m_team, e->m_senderId, e->m_messageId);
        break;
    }
    case 17:
    {
        // Remove this object's network registration. Cached objects retain their allocation
        // and return after removing the active descriptor; other objects follow normal teardown.
        world::IObject* const object = static_cast<world::IObject*>(data);
        io::INetworkable* const networkable = object->getNetworkable();      // PE +0xD4
        if (!networkable)
            break;                                       // no networkable: nothing happens
        if (object->getObjectCacheID() != 0)             // PE +0x28
        {
            // The cached object goes back to the pool: its whole subtree stops ghosting, the
            // root/PCO gets the disabled bit, and its descriptor leaves the active map.
            // true here descends through player-control children too - see disableGhosting.
            disableGhosting(object, true);
            // m_basePriority < 0 means c_NIGhostAlways (-1.0f): a static/always ghost is exactly
            // the case that is NOT disabled here.
            if (networkable->m_basePriority >= 0.0f)                        // nw +0x10
                disableObject(object);
            io::networkManager->removeActiveDescriptor(networkable->m_networkableId);
            break;                                                          // the cached arm returns
        }
        // An existing connection must own a ghost manager.
        if (m_connection)
            m_connection->m_ghostManager->removeGhostable(networkable->m_networkableId);
        io::networkManager->removeNetworkable(networkable->m_networkableId);
        break;
    }
    case 18:
    {
        // Send remote-console text as block 6 without its terminator. Requires a live
        // connection. The server forwards it to Python RemoteCommand; block 7 carries the reply.
        const char* const text = static_cast<const char*>(data);
        m_connection->getDataBlockManager()->enqueueDataBlock(
            6, text, static_cast<int>(std::strlen(text)));                         // PE +0x0C, slot 0
        break;
    }
    default:
        break;
    }
}

// Dispatches completed connection data blocks, skipping them during demo playback.
// Server/map information drives client setup, level loading and the loaded acknowledgement;
// map-load failure stops the client and unloads the logic.
// Requires the SwiffHost, connection and data-block manager on the relevant paths.
// Read event fields through DataBlockReadyEvent because base-class layout differs between builds.
void GameClient::handleDataBlock(EventNode* node, void* data)
{
    (void) data;
    if (m_demoRecorder)
        GAMECLIENT_FRONTIER("handleDataBlock", "DemoRecorder playback predicate (54B8F0) is unmapped");

    DataBlockReadyEvent* const block = static_cast<DataBlockReadyEvent*>(node);
    const unsigned int   type  = block->m_blockEventId;            // PE +0x2C
    const unsigned char* bytes = block->m_data;                    // PE +0x34
    const unsigned int   size  = block->m_size;                    // PE +0x30

    switch (type)
    {
    case 0:                                                        // ServerInfo
    {
        serverInfo->updateFromDataBlock(bytes, size);
        if (!isDedicated())
        {
            ui::ISwiffHost* const host = static_cast<ui::ISwiffHost*>(
                classManager->getSingleton(std::string("SwiffHost")));
            const int queryPort = serverInfo->m_block.gameSpyPort;              // : movzx word +0x50
            host->setIntE58(queryPort);                                         // +0x25C
            host->addServerToHistory(host->getJoinAddress(), queryPort,
                                     serverInfo->m_block.gameName);             // +0x268(+0x258, ...)
            host->setStringED8(serverInfo->getWelcomeMsg(0));                   // +0x2E4
            host->setStringE5C(serverInfo->m_block.gameName);                   // +0x2FC
        }
        if (serverInfo->m_block.voipEnabled)                                    // +0x31
            gameLogic->startupVoipClient();                                     // +0x25C
        if (serverInfo->getRequestResponse())                                   // the server wants our ClientInfo
        {
            ClientInfo info;
            info.m_playerNameHash = AntiHacks::instance()->m_playerNameHash;   //  +0x00 -> info +0x38
            info.setPlayerName(settings->stringGet(std::string("GSPlayerNameNoClanTag"),
                                                   std::string("player")));
            info.setClanTag(settings->stringGet(std::string("GSPlayerClanTag"),
                                                std::string("")));
            info.m_usesVoip = serverInfo->m_block.voipEnabled != 0;             // +0x31 -> info +0x5C

            IProfileManager* const profiles = static_cast<IProfileManager*>(
                classManager->getSingleton(std::string("ProfileManager")));
            int profileId = 0;
            if (profiles && profiles->getProfileId(profileId))                  // +0x30
                info.m_profileId = profileId;
            else
                info.m_profileId = 0;

            if (!AntiHacks::instance()->m_gsAuth.empty())                       // : the string's size
                info.setGameSpyAuth(AntiHacks::instance()->m_gsAuth);           // instance() again, as shipped
            else
                info.setGameSpyAuth(std::string(""));

            if (info.m_playerNameHash == 0)
                Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 1238, "")
                    << "About to send ClientInfo with namehash == 0";

            DataBlockManager* const blocks = m_connection->getDataBlockManager();   // PE +0x0C = ELF +0x10
            blocks->enqueueDataBlock(1, info.getNetBuffer(), info.getNetBufferSize());   // slot 0
        }
        break;
    }
    case 2:                                                        // MapInfo
    {
        m_mapInfo.setFromDataBlock(bytes, size);
        m_checksumContext->setChallengeOrdinal(m_mapInfo.getChallengeOrdinal());   // : +0x150's +0
        if (m_mapInfo.getMapName().empty())                        // the string's size, then LABEL_24
        {
            m_isRunning = false;                                   // +0xFC
            gameLogic->unload();                                   // PE +0x18
            break;
        }
        const int maxPlayers = m_mapInfo.getMaxNumberOfPlayers();
        settings->intSet(std::string("GSMaxNrOfPlayers"), maxPlayers);           // +0x14
        settings->stringSet(std::string("GSGameMode"), m_mapInfo.getGameMode());  // +0x44
        m_commanderEnabled = m_mapInfo.getCommanderEnabled();                     // > +0x154
        gameLogic->setNoVehiclesMode(serverInfo->m_block.noVehicles != 0);        // PE +0x88 <- serverInfo +0x58
        eventManager->postEvent(ECHud, 63, &m_commanderEnabled, 4.0f);            // +0x10
        if (load())                                                               // PE +0x1C = ELF +0x20
        {
            // the demo predicate again (a null recorder here - see the top), then the proceed event
            GameEvent* const loaded =
                new (std::nothrow) PostRemoteEvent(ECNetwork, 2, false, 0, 0, 0.0f);   // 0x54
            sendGameEvent(0xFFFFFFFFu, loaded);                                   // PE +0x8C = ELF +0x90
            if (serverInfo->m_block.useContentChecking)                           // +0x52
            {
                m_checksumContext->runMiscChecksum();
                m_checksumContext->runArchiveChecksum();
                if (m_connection)
                {
                    GameEvent* const check =
                        new (std::nothrow) ContentCheckEvent(m_checksumContext);  // 0x3C
                    sendGameEvent(0xFFFFFFFFu, check);                            // a failed new sends NULL
                }
            }
        }
        else
        {
            m_isRunning = false;
            gameLogic->unload();
        }
        break;
    }
    case 4:
        if (isLightmapRenderingNode())
            GAMECLIENT_FRONTIER("handleDataBlock", "block 4: DistributedLightRenderer +0x24 is unrecovered");
        Debug(DEBUGTYPE_6, GAMECLIENT_FILE, 1300, "")
            << "Server asked us to generate lightmaps, and we arent a lightmap rendering node";
        break;
    case 5:                                                        // MapList
        MapList::instance()->updateFromDataBlock(bytes, size);
        break;
    case 7:
    {
        std::string text(reinterpret_cast<const char*>(bytes), size);
        io::mainConsole->output(text + "\n");
        break;
    }
    case 8:
    {
        EndOfRoundInfo endOfRound;
        endOfRound.setFromDataBlock(bytes, size);
        ui::ISwiffHost* const host = static_cast<ui::ISwiffHost*>(
            classManager->getSingleton(std::string("SwiffHost")));
        host->setEndOfRoundInfo(endOfRound.getData(),
                      static_cast<float>(endOfRound.getDelay()));    // PE +0x170
        break;
    }
    case 9:
        GAMECLIENT_FRONTIER("handleDataBlock", "block 9: the 5BCC50 block and IGameClient +0x6C are unmapped");
    default:
        break;
    }
}

} // namespace hfe
} // namespace dice

#endif // !dedicated
