// Path: attested by GameServer::init diagnostics; contents recovered from the symbolized ELF.
// GameServer owns level loading, client admission, simulation and outgoing game state.
// Keep the recovered lifecycle and dispatch ordering; method-level exceptions remain at their sites.

#include "GameServer.h"
#include "VoipServerConnection.h"
#include "ClientInfo.h"                    // handleDataBlock parses a block-id-1 payload into one
#include "../Persistence/AntiHacks.h"      // handleClientInfo's ranked name-hash record
#include "../Persistence/GSServerSideNameValidation.h"  // ...and the GameSpy validator it fires (staged)
#include "../Common/ClientConnection.h"
#include "../Common/ServerConnection.h"    // ditto - m_loadUnload lives here, not on the base
#include "../GameLogic/SquadManager/ISquadManager.h"
#if !DEDICATED
#include "../GeneralSettings.h"          // limitCoopPlayers' two client-only bot-count pushes
#endif
#include "../Common/GameEventManager.h"    // onNewConnection queues the ChallengeEvent
#include "../Common/GhostManager.h"        // the per-connection ghost update
#include "../../Core/System.h"             // WorldPref::getTickCount
#include "../DemoRecorder/DemoRecorder.h"  // a recording connection never sends
#include "../DemoAvailableInfo.h"          // onAutoRecordingAvailable's block-9 payload

#include <algorithm>                       // std::sort - the fairness queue
#include "../GameEvents/ChallengeEvent.h"  // the first thing a server ever sends
#include "../Common/GameEvent.h"
#include "../Common/CreateObjectEvent.h"          // createObjectOnAllClients' two events
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"   // setDefaultTeam on the created PCO
#include "../Objects/IObjectTemplate.h"          // getClassID / getId
#include "../IGameLogic.h"                     // gameLogic - load is vtable +0x14
#include "../Python/PythonHost.h"              // pythonHost() / initializeGameMode
#include "../Python/PyRef.h"                   // the buildArgs result
#include "../Common/PlayerAction.h"             // processLocalPlayersInputs
#include "../GameLogic/ActionBuffer.h"          // ditto - pushBack
#include "../../Core/System.h"                  // WorldPref::getTickCount
#include "../GameEvents/CreatePlayerEvent.h"   // createPlayer's broadcast
#include "../GameEvents/ChangePlayerNameEvent.h"
#include "../GameEvents/VoipOnOffEvent.h"       // updatePlayersVoipStatusOnAllClients' broadcast
#include "../GameEvents/VoipSessionEvent.h"    // createPlayer's unicast
#include "../GameEvents/RadioMessageEvent.h"   // radio trio -> RadioMessageReceivedEvent seam
#include "../GameEvents/EnterVehicleEvent.h"   // enterVehicle replication
#include "../Objects/Game/Soldier.h"           // setLookUpDownAngle - the wired hook
#include "../Objects/Game/KitTemplate.h"       // the two CreateKitEvent operands
#include "../GameEvents/ExitVehicleEvent.h"    // exitVehicle replication
#include "../GameLogic/GameLogic.h"            // getVoiceMessages concrete boundary
#include "../GameLogic/ServerGameLogic.h"      // toggleFreeCamera's direct server-logic call
#include "../GameLogic/VoiceMessages.h"
#include "VoipServerConnection.h"              // CreateClientSessionId
#include "../GameLogic/SpawnSystem/SpawnManager.h"   // createAndSpawnAIPlayer
#include "../GameLogic/SpawnSystem/SpawnGroup.h"     // ISpawnGroup::getActive
#include "../../AI/AIInterface/AIInstance.h"   // g_aiInstance - the "AI enabled" test
#include "../../AI/AIInterface/IAIEventCourier.h" // missile-lock delivery to AI occupants
#include "../../AI/AIInterface/SingletonServer.h" // cached AI courier refresh table
#if defined(_WIN32)
#include "../../AI/AIDLL/AI/Main/BF2AI.h"      // PE unload's AI isInitialized/clear pair
#endif
#include "../Main/BF2Engine.h"                 // loadAI / quitNextFrame / updateStatusMonitor
#include "../Utility/BuildNrUtil.h"            // getNetVersionNumber - the protocol stamp
#include "../../Core/ClassManager.h"           // createInstance2 +0x34 / getSimpleSingleton +0x5C
#include "../../Core/EventManager.h"           // postEvent +0x14
#include "../Objects/ICameraObject.h"          // enterVehicle carries the old camera across
#include "../Objects/Base/FreeCamera.h"        // simulatePlayerUpdate's freecam dispatch
#include "../Objects/Base/Camera.h"            // enterVehicle's RotationalBundle rotation state
#include "../Objects/Base/SimpleObjectTemplate.h"   // getRelativePositionOffset
#include "../../Physics/PhysicsUtil.h"        // disablePhysics
#include "../Python/PythonHost.h"             // buildArgs / handleEvent - the two script events
#include "../../Core/Ids.h"                    // CID_SquadManager / IID_ISquadManager
#include "../../Core/IProfilerClient.h"        // getEnable - startServer's profiler gate; retires
                                               //   the ProfilerSlots raw-slot shim
#include "../../Core/LoadStat.h"               // handleUnloadLevel resets it
#include "../../Core/SettingsRepostitory.h"    // settings - the GS* keys
#include "../../Core/StringUtil.h"             // compareIgnoreCase / formatString
#include "../../Core/System.h"                 // g_system - wall clock, sleep
#include "../../IO/BitStream.h"                // startServer news one (operator new(0x64))
#include "../../IO/Input/Input.h"              // native IInputManager by-value control map
#include "../../IO/ChecksumContext.h"          // onContentCheckEvent's three 16-byte hash conversions
#include "../../IO/File/FileManager.h"         // LoadLevelFingerprint reads archive.md5
#include "../../IO/Console/Console.h"          // addTextToChatWindow -> mainConsole->output
#include "../../IO/Stream/IStream.h"           // LoadLevelFingerprint's read/getAvailableBytes
#include "../../IO/Network/INetworkable.h"     // findNetworkableObjects hands these to the manager
#include "../../IO/Network/NetworkManager.h"
#include "../GameEvents/DestroyObjectEvent.h"  // handleNetworkEvent arm 17's broadcast
#include "../Common/StringManager.h"           // StringReceivedEvent - handleNetworkEvent arm 11
#include "../Common/DataBlockManager.h"        // DataBlockReadyEvent - handleDataBlock's node
#include <cstring>                             // strlen - arm 19's "\n"
#include <cctype>                              // isupper / tolower - handleClientInfo's name hash
#include "../../IO/Network/NetworkStats.h"     // BasicNetworkStats lifetime, samplers and aggregate update
#include "../Common/IClientConnection.h"       // ABI-portable connection vtable
#include "../../Scene/Object/ObjectManager.h"  // destroyObjects / the grid + registry accessors
#include "../Objects/IObject.h"
#include "../GameEvents/DestroyPlayerEvent.h"   // destroyPlayer's broadcast
#include "../Objects/ObjectLayoutAccess.h"      // objectTemplate / abilityGLComponent - destroyPlayer, filterMessage
#include "../Objects/GameLogicComp/IAbilityGLComp.h"   // getGLInfo - filterMessage's medic channel
#include "../Objects/ISoldier.h"                // getKit - filterMessage's medic channel
#include "../Objects/Game/Kit.h"                // the kit object behind it
#include "../Objects/PlayerControlObjectTemplate.h"   // getVehicleCategory - filterMessage's vehicle channels
#include "../Main/Game.h"                       // Game::outputChatString - ServerStringMessageEW
#if !DEDICATED
#include "../Common/ClientVoteManager.h"         // update()'s listen-server vote tick (BF2.exe 4D11AD)
#endif
// the frame loop
#include "ServerInfo.h"                         // the server-info broadcast (real class since this batch)
#include "../MatchMaking/Server/MatchMakingServer.h"   // the CD-key authenticator + MatchMakingServer
#include "ServerVoteManager.h"                  // declaration only - getInstance()->update
#include "BanList.h"                            // declaration only - getInstance()->update
#include "../GameEvents/ChallengeResponseEvent.h"  // challengeResponse reads its fields
#include <cstdio>                              // cdkey_auth_callback frontier diagnostic
#include "../DemoRecorder/DemoRecorder.h"
#include "AutoRecorder.h"
#include "../Persistence/HTTPRequest.h"         // declaration only - update / isUpdating / dtor
#include "../Persistence/IStatsManager.h"       // think()
#include "../Persistence/PlayerUnlockInfo.h"
#include "../Objects/IPlayer.h"                 // the real interface (this batch) - the player loops
#include "../Objects/IWeaponObject.h"           // typed firearm targeting/owner calls
#include "../Objects/Game/StickyProjectileManager.h" // ctor/shutdown-owned singleton
#include "../Objects/ObjectLayoutAccess.h"      // simulatePlayersUpdate reads the vehicle's parent link
#include "../Utility/Vars.h"                    // coll-dump-all-tests
#include "../../Scene/Object/PlayerManager.h"   // playerManager - the local human player, the player list
#include "../../Physics/PhysicsNodeManager.h"   // physicsNodeManager->update / postFrameUpdate
#include "../../Physics/ResponsePhysicsManager.h"   // responsePhysicsManager - the frame protocol
#include "../../IO/Network/ProtocolLayer/NetServer.h" // real io::NetServer + io::INetServer -
                                                      //   retires the NetServerSlots raw-slot shim

#include <cstdlib>                             // srand / rand - the challenge ordinal
#include <iostream>                            // the empty-maplist refusal writes to std::cout
#include <sstream>

namespace dice {
namespace hfe {

extern ServerSettings* serverSettings;      // ds:0x8AAB7A0 - attested type (ServerSettings::load)
extern ReservedSlots*  reservedSlots;       // ds:0x8AAB768 - attested type (ReservedSlots::load)
extern MapList*        mapList;             // ds:0x8AAAA00 - attested type (MapList::load)
extern ClassManager*   classManager;        // ds:0x8AC85A0 - Core/Globals.cpp
extern BF2Engine*      bf2Engine;           // ds:0x8AACBD8
extern IBase*          profiler;            // ds:0x8ABBCAC - Core/Globals.cpp; the startServer
                                            //   continue-after-failed-load gate (see the body)
extern IBase*          squadManager;        // ds:0x8ACBFA4 - Core/Globals.cpp, added this batch;
                                            // Local declarations cover remaining slot-only collaborators; shared interfaces
                                            // are used directly where available.
namespace {

// [CHOSEN] The LAN MatchMakingServer destructor performs service teardown.
void destroyMatchMakingServer(MatchMakingServer* mm) { delete mm; }


inline ServerConnection* asServerConnection(IClientConnection* c)
{
    return reinterpret_cast<ServerConnection*>(c);
}

// GameSpy CD-key hash lookup. Kept as a narrow C declaration to avoid importing
// SDK Winsock headers into this translation unit.
extern "C" char* gcd_getkeyhash(int gameid, int localid);

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: PB-ADMISSION-SDK]
// Stops before optional PunkBuster admission until its SDK state and loader are
// recovered; a null SDK must not be treated as successful authentication.
const char* punkBusterAuthClient(const char*, int, const char*)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::(anon)::punkBusterAuthClient (PbAuthClient SDK state/loader) reached\n");
    std::fflush(stderr);
    //std::abort();
    return nullptr;
}

void cdkey_auth_callback(int localId, AuthResult result, void* context)
{
    GameServer* const gs = static_cast<GameServer*>(context);
    switch (result)
    {
    case AR_AUTHORIZED:
        if (!serverSettings->getSvPunkBuster())
        {
            gs->authOk(localId);
        }
        else
        {
            std::string ipAndPort;
            gs->getConnectionIpAndPort(localId, ipAndPort);
            const char* hash = gcd_getkeyhash(gamespy::g_gameID, localId);
            const char* reason = punkBusterAuthClient(ipAndPort.c_str(), 1, hash);
            if (reason) gs->kickPlayer(localId, static_cast<io::PResult>(34), std::string(reason));
            else gs->authOk(localId);
        }
        break;
    case AR_NO_AUTH:      gs->authOk(localId);          break;
    case AR_KEY_INVALID:  gs->authFailed(localId, 19u); break;
    case AR_KEY_DISABLED: gs->authFailed(localId, 20u); break;
    case AR_KEY_IN_USE:   gs->authFailed(localId, 37u); break;
    }
}

} // namespace

// Initializes server services, matchmaking, and the Python fingerprint.
// LAN initialization still follows this service setup.
bool GameServer::init()
{
    m_numPlayers = 0;
    m_initComplete = false;
    m_initTime = g_system->getWallClockTime();
    serverSettings->load();
    reservedSlots->load();

    m_isInternetServer = serverSettings->m_internet;
    if (m_isInternetServer)
        m_maxConnectionType = serverSettings->m_maxConnectionType;
    else
    {
        m_maxConnectionType = 5;
        serverSettings->m_punkBuster = false;
    }

    // PB_get_sv_punkbusterInstalled: ELF 0x84F8A10 / PE 0x453B80 is only fileExists.
    // [CHOSEN, placement only] inline its read here until the PB integration TU is recovered.
    // [PLATFORM] the shipped Linux path is pb/pbsv.so; Windows uses pb/pbsv.dll.
#ifdef _WIN32
    const char* const pbServerLibrary = "pb/pbsv.dll";
#else
    const char* const pbServerLibrary = "pb/pbsv.so";
#endif
    if (serverSettings->getRanked()
        && !io::fileManager->fileExists(std::string(pbServerLibrary)))
        Debug(DEBUGTYPE_7, "Game/GameServer/GameServer.cpp", 617)
            << "Cannot run a ranked server without punkbuster installed and enabled.";
    m_isPunkBusterEnabled = serverSettings->getSvPunkBuster();

    // (ELF 0x80A68A0..0x80A68D5). unconditional in retail - a dedicated
    // server always owns an AutoRecorder, and `sv.autoRecord` is tested later, inside roundStarted.
    // `operator new(0x48)` here is what fixes the class's ELF size. The Demo comes from the engine's
    // own +0xA8 field, not from a parameter.
    m_autoRecorder = new AutoRecorder(this, bf2Engine->demo);
    mapList->clear(false);
    mapList->load(false);
    if (mapList->getMapCount() > 0)
        mapList->setCurrentMap(0, true);
    limitCoopPlayers();

    m_matchMakingServer = new MatchMakingServer(this);
    if (!m_matchMakingServer->initializeServices(m_isInternetServer))
    {
        // Preserve both diagnostics and the subsequent demotion. Debug type 6 is fatal under
        // normal retail logging, despite the message saying "Changing server to unranked".
        // Retail streams the ostringstream object (operator const void*), not its str().
        std::ostringstream message;
        message << "FATAL ERROR: Your server cannot communicate with the required internet backends.\n"
                << "Please read the documentation for the dedicated server to find out what ports you\n"
                << "need to open in your firewall and/or NAT products. Changing server to unranked.\n";
        Debug(DEBUGTYPE_8, "Game/GameServer/GameServer.cpp", 646)
            << (message.fail() ? nullptr : static_cast<const void*>(static_cast<std::ios*>(&message)));
        Debug(DEBUGTYPE_6, "Game/GameServer/GameServer.cpp", 647)
            << (message.fail() ? nullptr : static_cast<const void*>(static_cast<std::ios*>(&message)));
        serverSettings->setRanked(false);
    }
    // The failure arm rejoins here if its fatal diagnostic was suppressed. No null guard exists.
    m_matchMakingServer->getAuthenticator()->setStatusCallback(&cdkey_auth_callback, this);

    m_statsManager = static_cast<IStatsManager*>(classManager->createInstance(
        std::string("dice.hfe.StatsManager"), IID_IStatsManager, 0));
    const bool ranked = serverSettings->getRanked();
    if (ranked)
        m_statsManager->connect(); // result deliberately discarded before Python initialization

    // [CHOSEN, not recovered] Run the active mod's optional openbf2.con before Python init, so an
    // openbf2.* command in it (e.g. addModDll of a dice_py-hooking shim such as bf2hub) takes effect
    // before the Python host comes up. Absent file is not an error. Loaded with relative paths off
    // and full access, mirroring ServerSettings::load. B2BF/OpenBF2#39.
    {
        const std::string openBf2Con =
            settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2")) + "/openbf2.con";
        if (io::fileManager->fileExists(openBf2Con))
        {
            const bool useRelative = io::mainConsole->getUseRelativePaths();
            io::mainConsole->setUseRelativePaths(false);
            std::string out;
            io::mainConsole->runFullAccess(openBf2Con, "", "", "", "", "", "", "", "", out);
            io::mainConsole->setUseRelativePaths(useRelative);
        }
    }

    if (!python::pythonHost()->initialize(false))
        Debug(DEBUGTYPE_6, "Game/GameServer/GameServer.cpp", 669)
            << "failed to initialize python host";

    if (ranked)
    {
        IPlayerUnlockInfo* unlockInfo = getPlayerUnlockInfo();
        if (unlockInfo)
        {
            // Both script probes execute, but only providerID.sec's result participates in the
            // final decision. getRanked and isPythonValid are both called before testing either.
            io::fileManager->fileExists(std::string("python/bf2/stats/medals.py"));
            io::fileManager->fileExists(std::string("python/bf2/stats/snapshot.py"));
            const bool providerExists =
                io::fileManager->fileExists(std::string("python/bf2/stats/providerID.sec"));
            const bool authorized = unlockInfo->getRanked();
            const bool pythonValid = isPythonValid();
            if (!authorized || !pythonValid || !providerExists)
            {
                //Debug(DEBUGTYPE_7, "Game/GameServer/GameServer.cpp", 691)
                //    << "Server trying to use unauthorized ranking services";
                //return false;// FIX/TODO: tell CHC that openspy must support this
            }
        }
    }

    if (!serverSettings->m_adminScript.empty())
        python::pythonHost()->initializeAdminMod(serverSettings->m_adminScript);

    m_useContentChecking = false;
    const bool standard = loadStdArchiveFingerprints("std_archive.md5", m_stdArchiveMd5);
    const bool standardMod = loadStdArchiveFingerprints("std_archive_mod.md5", m_stdArchiveModMd5);
    const bool booster = loadStdArchiveFingerprints("bst_archive.md5", m_bstArchiveMd5);
    const bool boosterMod = loadStdArchiveFingerprints("bst_archive_mod.md5", m_bstArchiveModMd5);
    m_useContentChecking = standard && standardMod && booster && boosterMod;
    loadMiscFingerprints();
    serverInfo->m_block.useContentChecking = m_useContentChecking;
    m_initComplete = true;
    return true;
}
// Level startup constructs and initializes NetServer before validating the map list.
// GSLoadLevel appends to an empty rotation or inserts at the front; both select map zero.
// Client admission starts with a mod-specific challenge; the connection map uses byte keys.
extern IBase* spawnManager;

// Creates a player using a name truncated to 23 characters, retrying with ID/numeric suffixes.
// Retries have no failure limit, so persistent failures other than name collisions can loop forever.
// The player's network ID is player ID plus one.
world::IPlayer* GameServer::createPlayerWithValidName(const std::string& name, int id)
{
    // Enforce the configured player limit before allocating another player.
    if (m_numPlayers >= settings->intGet("GSMaxNrOfPlayers", 64))
        return 0;

    std::string::size_type keep = 23;
    if (name.size() < 23)
        keep = name.size();
    const std::string base(name, 0, keep);

    world::IPlayer* player = world::playerManager->createPlayer(world::CID_Player, base, id);
    if (!player)
    {
        player = world::playerManager->createPlayer(world::CID_Player,
                                                    base + formatString("_%d", id), id);
        if (!player)
        {
            // See the header block: no failure exit. Faithful.
            int n = 0;
            do
            {
                player = world::playerManager->createPlayer(world::CID_Player,
                                                            base + formatString("_%d", n), id);
                ++n;
            }
            while (!player);
        }
    }

    io::networkManager->addNetworkable(player->getNetworkable(),
                                      static_cast<unsigned short>(player->getId() + 1));
    return player;
}

// Creates the local listen-server human (ID 255) or a bot using a downward free-ID
// search. Returns the ID or -1. Remote humans arrive through the handshake instead.
// Bot creation requires the AI singleton.
int GameServer::createPlayer(const std::string& name, bool isAI)
{
    int id = 255;

    if (isAI)
    {
        if (!ai::g_aiInstance)  // ds:0x8AC37F8
        {
            Debug(DEBUGTYPE_1, "Game/GameServer/GameServer.cpp", 3546, __FILE__)
                << "Failed to create ai player, AI not enabled.";
            return -1;
        }

        // First free id, counting down. The start point already skips as many slots as there are
        // players, so the search is usually one test long.
        for (id = 255 - world::playerManager->getNumberOfPlayers();
             world::playerManager->getPlayerFromId(id);
             --id)
        {
        }
    }

    world::IPlayer* player = createPlayerWithValidName(name, id);
    if (!player)
        return -1;

    // both take isAI. An AI player is flagged remote; a human made here is local. That is not a
    // transcription slip - it is what makes the local human the only non-remote player.
    player->setIsAIPlayer(isAI);
    player->setIsRemote(isAI);

    eventManager->postEvent(static_cast<EventCategory>(1), 4, player, 0.0f);   // +0x14

    sendGameEventToAllConnected(new CreatePlayerEvent(player, true));

    if (!isAI)
    {
        world::playerManager->setLocalHumanPlayer(id);              // PlayerManager +0x38 (int)
        player->setIsConnected(true);
    }

    // The fourth Python callback, and the first that hands the game mode a live engine object
    // rather than an id.
    {
        python::PyRef args = python::buildArgs(static_cast<world::IPlayer*>(player));
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(14), args.get());
    }

    // Voice identity is allocated with the player, not on demand.
    int sessionId = player->getVoipSessionId();
    if (!sessionId)
    {
        // ONE call, NOT three. Hex-Rays showed GetInstance(), getId() and
        // CreateClientSessionId() as separate statements because it had split a thiscall's
        // `this` and argument; the mangling (...CreateClientSessionIdEi) settles it.
        sessionId = VoipServerConnection::GetInstance()->CreateClientSessionId(player->getId());
    }
    player->setVoipSessionId(static_cast<unsigned short>(sessionId));

    sendGameEvent(player->getId(), new VoipSessionEvent(sessionId));
    return player->getId();
}

// Spawns an AI player at the team's first active spawn group; returns its ID or -1.
// No player is created when the team has no active spawn group.
int GameServer::createAndSpawnAIPlayer(const std::string& name, int teamId, int kitId)
{
    // `spawnManager` is ds:0x8AD3A00 at dice::hfe scope, declared IBase* - NOT in world:: and
    // not at its concrete type. Same idiom ServerGameLogic.cpp:944 uses.
    SpawnManager* spawn = static_cast<SpawnManager*>(spawnManager);
    const std::list<int>& groups = spawn->getGroupsForTeam(teamId);                 // +0x2C

    std::list<int>::const_iterator it = groups.begin();
    for (; it != groups.end(); ++it)
    {
        ISpawnGroup* group = spawn->getSpawnGroup(*it);                            // +0x28
        if (group && group->getActive(true))                                        // +0x2C, isAI
            break;
    }
    if (it == groups.end())
        return -1;

    const int id = createPlayer(name, true);
    world::IPlayer* player = world::playerManager->getPlayerFromId(id);
    if (!player)
        return -1;

    player->setTeam(teamId, false);     // +0xE4
    player->setKit(kitId);              // +0x16C
    player->setSpawnGroup(*it);         // +0x154

    eventManager->postEvent(static_cast<EventCategory>(1), 5, player, 0.0f);
    return id;
}

// Starts the listener before validating and loading the selected map rotation entry,
// then initializes voice routing. Refuses when already running or the rotation is
// empty. NetServer::init returns zero on success; failure destroys the new listener.
bool GameServer::startServer(const std::string& a1, int port, int /* dead - never read */)
{
    if (m_netServer)                                        // already running
        return false;

    m_netServer = new io::NetServer();                      // operator new(0x500) + ctor 0x8473370
    m_netServer->setVersion(BuildNrUtil::getNetVersionNumber());   // Network protocol version.
    if (!m_netServer)
    {
        // Unreachable in practice (stored three instructions earlier); preserved as shipped.
        std::string msg = formatString("Failed to create server instance");
        Debug(DEBUGTYPE_6, "Game/GameServer/GameServer.cpp", 1354) << msg;
        return false;
    }

    m_netServer->setPassword(serverSettings->getPassword());        // +0x40
#if !DEDICATED
    // [PLATFORM] BF2.exe 0x4CACC2..0x4CACF9, folded away in the dedicated ELF (): a
    // listen server keeps one connection for its own player - getMaxPlayers minus one, and 1 when
    // that goes negative (`sub eax,1 / jns`). The dedicated arm is the ELF's line below.
    if (!isDedicated())
    {
        int connections = serverSettings->getMaxPlayers() - 1;
        if (connections < 0)
            connections = 1;
        m_netServer->setMaxNumberOfConnections(connections);                           // +0x50
    }
    else
#endif
    m_netServer->setMaxNumberOfConnections(
        serverSettings->getMaxPlayers());                                              // +0x50

    m_bitStream = new io::BitStream(0, 0);                  // operator new(0x64)
    if (!m_bitStream)
    {
        // NON-fatal: logs and continues with a null bitstream (0x80A9F8D rejoins at 0x80A9910).
        std::string msg = formatString("failed to create bitstream");
        Debug(DEBUGTYPE_6, "Game/GameServer/GameServer.cpp", 1376) << msg;
    }

    if (m_netServer)
    {
        // NetServer initialization returns zero on success; other results destroy the listener and fail startup.
        if (m_netServer->init(a1, port, true) != 0)      // +0x10
        {
            if (m_netServer)
                delete m_netServer;
            m_netServer = 0;
            return false;
        }
    }

    if (!m_connections.empty())                             // the SGI-layout clear at 0x80A9F12
        m_connections.clear();

    m_port = port;                                          // +0x94 - the first int argument
    if (m_netServer)
        m_svStartTime = m_netServer->getTime();          // +0x64 -> +0x90

    registerStaticNetworkables();                           // Game::, direct call with this

    std::string level = settings->stringGet("GSLoadLevel", "");             // +0x40
    if (!level.empty())
    {
        bool menu = settings->boolGet("GSMenu", false);                     // +0x50
        if (!menu)
        {
            // Fold the requested level into the rotation. empty list -> append (insertEntry
            // structurally cannot seed an empty deque - its validateIndex refuses); NON-empty ->
            // insert AT front. both arms then select entry 0 (the shared tail).
            std::string mode = settings->stringGet("GSGameMode", "gpm_cq");
            if (mapList->getMapCount() <= 0)
                mapList->appendEntry(level, mode, 0, false);
            else
                mapList->insertEntry(0, level, mode, 0, false);
            mapList->setCurrentMap(0, false);               // respects isModifiable - no override
        }
    }

    if (mapList->getMapCount() == 0)
    {
        // The refusal: both messages verbatim ("atleast" is dice's spelling), then quit.
        std::cout << "No maps in maplist. Please add atleast one map, or use +loadLevel command.";
        Debug(DEBUGTYPE_8, "Game/GameServer/GameServer.cpp", 1423)
            << "No maps in maplist, exiting.";
        bf2Engine->quitNextFrame();
        return false;
    }

    // Resolve the current entry and write ITS name back into GSLoadLevel - after this call the
    // setting always names the actual current map, even when it started empty.
    const MapList::Entry* entry = mapList->getEntry(mapList->getCurrentMap());
    std::string levelName = entry->name;
    settings->stringSet("GSLoadLevel", levelName);                          // +0x44

    // An enabled profiler permits startup to succeed after a failed map load and still initializes VoIP.
    bool profilerKeepAlive = false;
    if (profiler)
        profilerKeepAlive = static_cast<IProfilerClient*>(profiler)->getEnable();   // +0x28

    if (!levelName.empty())
    {
        if (!game->loadPath(levelName))                     // IGame vtable +0x24 - THE handoff
        {
#if !DEDICATED
            // [PLATFORM] A listen server's failed map load disables the required profiler and fails startup without quitting the engine.
            if (!isDedicated())
            {
                Debug(DEBUGTYPE_8, "Game/GameServer/GameServer.cpp", 1454)
                    << "Couldn't load the first map in the maplist, exiting.";
                static_cast<IProfilerClient*>(profiler)->setEnable(false);
                return false;
            }
#endif
            bf2Engine->quitNextFrame();
            if (!profilerKeepAlive)
                return false;
        }
    }

#if !DEDICATED
    // [PLATFORM] Listen servers create the local human player; dedicated servers accept remote humans only.
    // Failure to create the local player fails startup.
    if (!isDedicated())
    {
        const int localPlayer = createPlayer(settings->stringGet("GSPlayerName", "player"), false);
        if (localPlayer == -1)
            return false;
    }
#endif

    // VoIP last, once the current map is known - then the five info fields to the game logic
    // (GameLogic vtable +0x254, storeVoipServerInfo). Fresh-instance values: 0,0,0,0,3.
    VoipServerConnection::GetInstance()->Init();
    VoipServerConnection* voip = VoipServerConnection::GetInstance();
    gameLogic->storeVoipServerInfo(voip->m_infoByte4314, voip->m_infoDword4318,
                                   static_cast<unsigned short>(voip->m_infoWord4400),
                                   static_cast<unsigned short>(voip->m_infoWord4408),
                                   voip->m_infoDword4410);
#if !DEDICATED
    // [PLATFORM] BF2.exe 0x4CB1B7..0x4CB1CC (): with voice enabled (the info byte) a
    // listen server starts its own voice client, IGameLogic +0x25C startupVoipClient.
    if (voip->m_infoByte4314 && !isDedicated())
        gameLogic->startupVoipClient();
#endif
    return true;
}

// Loads the selected level and initializes its server state.
bool GameServer::loadPath(const std::string& levelName)
{
    limitCoopPlayers();

    // The challenge ordinal is random 0..9, seeded from the wall clock in milliseconds.
    srand((int)(g_system->getWallClockTime() * 1000.0));    // System +0x1C; flt_888B168 = 1000.0f
    m_mapInfo.setChallengeOrdinal(rand() % 10);             // MapInfo at +0x04, by value

    if (m_loadState)                                        // +0xA0 nonzero = something is loaded
        unload();                                           // virtual, IGame slot +0x28

    // Event (5, 2) carries the path's c_str (the _S_terminal store at 0x80B1345 is the inlined
    // c_str()); then (2, 3). Both through EventManager +0x14.
    eventManager->postEvent(static_cast<EventCategory>(5), 2, (void*)levelName.c_str(), 0.0f);
    eventManager->postEvent(static_cast<EventCategory>(2), 3, 0, 0.0f);
    m_loadState = 1;                                        // loading

    if (squadManager)
    {
        // An existing squad manager: re-add dynamic spawn groups unless dropDynamicSpawns asks
        // otherwise (default false -> re-add).
        if (!settings->boolGet("dropDynamicSpawns", false))
            static_cast<ISquadManager*>(squadManager)->reAddDynamicSpawnGroups();   // +0x14
    }
    else
    {
        squadManager = classManager->createInstance(
            CID_SquadManager, IID_ISquadManager, 0);        // ClassManager +0x34 (the cid,iid
                                                            // overload IDA labels createInstance2)
        if (!squadManager)
        {
            // NON-fatal - logs and continues (0x80B1E60 rejoins at 0x80B13FC).
            Debug(DEBUGTYPE_6, "Game/GameServer/GameServer.cpp", 3789)
                << "Failed to init squadManager";
        }
    }

    // The current rotation entry. loadPath trusts it exists (no null guard - only reached with a
    // non-empty rotation); limitCoopPlayers above guards ITS OWN read.
    const MapList::Entry* entry = mapList->getEntry(mapList->getCurrentMap());

    // THE coop/useai resolution: coop - by the setting or by the entry's mode - force-writes
    // UseAI=true; then UseAI alone decides loadAI.
    std::string modeSetting = settings->stringGet("GSGameMode", "gpm_cq");
    std::string entryMode   = entry->gameMode;
    if (compareIgnoreCase(modeSetting, "gpm_coop") == 0)
        settings->boolSet("UseAI", true);                   // +0x54
    else if (compareIgnoreCase(entryMode, "gpm_coop") == 0)
        settings->boolSet("UseAI", true);
    if (settings->boolGet("UseAI", false))
        bf2Engine->loadAI();                                // direct call, this = ds:0x8AACBD8

    // NON-fatal (the correction): a failed game-mode init logs and the load continues.
    // The mode handed to Python is the entry's mode, not the setting.
    if (!python::pythonHost()->initializeGameMode(entry->gameMode))
    {
        Debug(DEBUGTYPE_6, "Game/GameServer/GameServer.cpp", 3822)
            << "failed to initialize gamemode";
    }

    // Fill the MapInfo from the entry; a zero entry size falls back to the settings clamp.
    int maxPlayers = entry->mapSize;                        // the word at entry+8
    if (!maxPlayers)
        maxPlayers = serverSettings->getMaxPlayers();
    m_mapInfo.setMaxNumberOfPlayers(maxPlayers);
    m_mapInfo.setGameMode(entry->gameMode);
    m_mapInfo.setLevelPath(settings->stringGet("GSLevelDirectory", ""));
    m_mapInfo.setMapName(entry->name);

    // ...and write the resolved values back into settings - GSMaxNrOfPlayers and GSGameMode now
    // reflect the level actually being loaded.
    int resolvedMax = m_mapInfo.getMaxNumberOfPlayers();
    settings->intSet("GSMaxNrOfPlayers", resolvedMax);      // +0x14, by pointer
    settings->stringSet("GSGameMode", m_mapInfo.getGameMode());

    // The path handed to GameLogic::load: GSLevelDirectory + levelName + "/". this is why load()
    // strips min(7,len) leading chars ("levels/") plus the trailing slash to recover the name.
    std::string path = settings->stringGet("GSLevelDirectory", "") + levelName;
    path += "/";

    clientLoadLevel(-1);                                    // 1 = every connection
    waitForClients();

    // Initialise the external AI before loading its level scripts. Typed dispatch is required:
    // init is ELF +0x08 but PE +0x04 because MSVC emits one destructor slot instead of D1/D0.
    ai::IAIInstance* aiSingleton = static_cast<ai::IAIInstance*>(
        classManager->getSimpleSingleton("AI"));                         // ClassManager +0x5C
    if (aiSingleton)
        aiSingleton->init();

    bool loaded = gameLogic->load(path);                    // IGameLogic +0x14 - THE .con files RUN
    if (!loaded)
        return handleUnloadLevel();                         // returns handleUnloadLevel's OWN bool
                                                            // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: bone-staging-index]
                                                            // Identifies the deferred-animation event poster. A duplicate dispatch can sweep
                                                            // animations still referenced by live bundles because loading is one-shot.
    std::fprintf(stderr, "[TEMP][ANIMSWEEP] ECCore 3 posted by GameServer::loadPath (path=%s)\n",
                 path.c_str());
    std::fflush(stderr);

    eventManager->postEvent(static_cast<EventCategory>(5), 3, 0, 0.0f);
    eventManager->postEvent(static_cast<EventCategory>(5), 7, 0, 0.0f);
    findNetworkableObjects();
    eventManager->postEvent(static_cast<EventCategory>(2), 4, 0, 0.0f);

    if (m_abortLoadingLevel)                                // +0x48 - raised mid-load
        return handleUnloadLevel();

    m_levelFingerprint = LoadLevelFingerprint() ? 1 : 0;    // the al byte -> +0x151
    m_loadState = 2;                                        // loaded
    return loaded;                                          // true
}

// limitCoopPlayers - ELF 0x80B65D0, 513 bytes. Coop-only clamps: max
// players to 32, then the bot count topped up so players+bots <= 64 (bounded 1..48), mirrored
// into the "GeneralSettings" singleton through two setter slots.
void GameServer::limitCoopPlayers()
{
    const MapList::Entry* entry = mapList->getEntry(mapList->getCurrentMap());
    if (!entry)                                             // null-guarded here (unlike loadPath)
        return;
    if (compareIgnoreCase(entry->gameMode, "gpm_coop") != 0)
        return;

    if (serverSettings->getMaxPlayers() > 32)
        serverSettings->setMaxPlayers(32);                  // the status-string return, discarded

    int maxPlayers = serverSettings->getMaxPlayers();
    if (maxPlayers + serverSettings->getCoopBotCount() > 64)
    {
        int room = 64 - serverSettings->getMaxPlayers();
        int bots = 1;
        if (room > 0)
        {
            bots = room;
            if (room >= 49)
                bots = 48;
        }
        serverSettings->m_coopBotCount = bots;              // RAW field write, as shipped

#if !DEDICATED
        // Client-only: the dedicated image never registers this singleton, so the shipped lookup
        // always answered null there and the two pushes never happened.
        GeneralSettings* generalSettings =
            static_cast<GeneralSettings*>(classManager->getSingleton("GeneralSettings"));   // +0x4C
        if (generalSettings)
        {
            generalSettings->setMaxBots(bots);        // +0xB4
            generalSettings->setCoopBotCount(bots);   // +0xF4
        }
#endif
    }
}

// Requests remote completion and pumps the engine until every client acknowledges or ten seconds elapse.
// With no connections, returns immediately.
void GameServer::waitForClients()
{
    m_isWaitingForClients = true;                           // +0x15D
    m_clientAckedEndOfRoundGuard = 0;                       // +0x160

    PostRemoteEvent* ev = new PostRemoteEvent(
        static_cast<EventCategory>(6), 20, 0, 0, 0, 0.0f);  // operator new(0x54)
    sendGameEventToAll(ev);                                 // virtual, IGame +0x8C

    unsigned int count = (unsigned int)m_connections.size();    // +0x78
    double deadline = g_system->getWallClockTime() + 10.0;      // flt_888B588
    for (;;)
    {
        double now = g_system->getWallClockTime();
        if (deadline <= now || m_clientAckedEndOfRoundGuard >= count)
            break;
        game->update(0, -1.0f);                             // IGame +0x30, the catch-up pump
        if (bf2Engine)
            bf2Engine->updateStatusMonitor(true);           // null-checked, as shipped
        g_system->sleep(0);                                 // System +0x20
    }
    m_isWaitingForClients = false;
}

// handleUnloadLevel - ELF 0x80B1220, 66 bytes. The failure/abort exit:
// unload, clear the MapInfo, drop the load callback and stats. returns false - which is what
// loadPath hands back on its failure paths.
bool GameServer::handleUnloadLevel()
{
    unload();                                               // virtual, IGame +0x28
    m_mapInfo.clear();
    loadStat->unregisterLoadCallback();
    loadStat->resetLoadStats();
    return false;
}

// Python is considered initialized after scripts query both ranked and hosted settings.
bool GameServer::isPythonValid()
{
    if (m_pythonValid[0])
        return m_pythonValid[1];
    return false;
}

void GameServer::setPythonValid(int which, bool valid)
{
    m_pythonValid[which] = valid;
}

// Destroys pending objects, then assigns missing network IDs in object-ID order.
void GameServer::findNetworkableObjects()
{
    world::objectManager->destroyObjects();                             // +0x98
    std::map<unsigned int, world::IObject*>& all =
        world::objectManager->getAllObjectsFromGrid();                  // +0x34
    for (std::map<unsigned int, world::IObject*>::iterator it = all.begin();
         it != all.end(); ++it)
    {
        io::INetworkable* networkable = it->second->getNetworkable();  // IObject slot 54 (+0xD8)
        if (networkable && networkable->m_networkableId == 0)           // the word at +0x04
            io::networkManager->addNetworkable(networkable);
    }
}

// clientLoadLevel - ELF 0x80B21C0, 9 bytes: a tail-call forwarder. sendClientMapInfo - ELF
// ship the MapInfo net-block (block id 2) and mark
// every targeted connection loading (++loadUnload, connectionState = 1). -1 targets everyone.
void GameServer::clientLoadLevel(int connectionId)
{
    sendClientMapInfo(connectionId);
}

//  / PE 0x447AA0.  A level unload sends an empty MapInfo block and invalidates the
// recipient's content state.  The all-clients arm also returns every connection to state 1.
void GameServer::clientUnloadLevel(int connectionId)
{
    MapInfo empty;
    empty.clear();

    if (connectionId == -1)
    {
        const int size = empty.getNetBufferSize();
        const void* data = empty.getNetBuffer();
        sendDataBlockToAll(2, data, static_cast<unsigned int>(size), false);

        for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            reinterpret_cast<ClientConnection*>(it->second)->m_connectionState = 1;
            it->second->setContentValid(false);
        }
        return;
    }

    const int size = empty.getNetBufferSize();
    const void* data = empty.getNetBuffer();
    sendDataBlock(connectionId, 2, data, static_cast<unsigned int>(size), false);
    IClientConnection* connection = getClient(connectionId);
    if (connection)
        connection->setContentValid(false);
}

void GameServer::sendClientMapInfo(int connectionId)
{
    if (connectionId == -1)
    {
        int         size = m_mapInfo.getNetBufferSize();    // rebuilds if dirty
        const void* data = m_mapInfo.getNetBuffer();
        sendDataBlockToAll(2, data, (unsigned int)size, false);
        for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            ClientConnection* conn = (ClientConnection*)it->second;
            ++asServerConnection(it->second)->m_loadUnload;  // +0x180
            conn->m_connectionState = 1;                      // +0x0C
        }
    }
    else
    {
        int         size = m_mapInfo.getNetBufferSize();
        const void* data = m_mapInfo.getNetBuffer();
        sendDataBlock(connectionId, 2, data, (unsigned int)size, false);
        IClientConnection* const raw = getClient(connectionId);
        ClientConnection* conn = (ClientConnection*)raw;
        ++asServerConnection(raw)->m_loadUnload;            // +0x180, no null guard as shipped
        conn->m_connectionState = 1;
    }
}

// Queues the block on each connection, optionally restricting to state > 0.
// Enqueue results are discarded. Typed calls preserve PE/ELF destructor-slot differences.
void GameServer::sendDataBlockToAll(unsigned int blockId, const void* data, unsigned int size,
                                    bool onlyStarted)
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        ClientConnection* conn = (ClientConnection*)it->second;
        if (!onlyStarted || conn->m_connectionState > 0)
        {
            DataBlockManager* mgr = it->second->getDataBlockManager();         // conn +0x10
            mgr->enqueueDataBlock(blockId, data, size);                        // +0x00
        }
    }
}

void GameServer::sendDataBlock(int connectionId, unsigned int blockId, const void* data,
                               unsigned int size, bool onlyStarted)
{
    IClientConnection* c = getClient(connectionId);
    if (!c)
        return;
    ClientConnection* conn = (ClientConnection*)c;
    if (!onlyStarted || conn->m_connectionState > 0)
    {
        DataBlockManager* mgr = c->getDataBlockManager();                      // conn +0x10
        mgr->enqueueDataBlock(blockId, data, size);                            // +0x00
    }
}

const io::BasicNetworkStats* GameServer::getNetworkStats() const { return m_networkableStats; } // +0xBC

int GameServer::getNumDroppedFrames() const   { return m_numberOfDroppedFrames; }   // +0x100
int GameServer::getNumOvershotFrames() const  { return m_numberOvershotFrames; }    // +0x104

// Reports whether server initialization has completed.
bool GameServer::getInitComplete() const      { return m_initComplete; }            // +0x109

// Routes HUD, core and network events. Other categories, including registered
// ECGameLogic notifications, are ignored.
void GameServer::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    switch (category)
    {
    case ECHud:
        handleHudEvent(id, node, data);
        break;

    case ECNetwork:
        handleNetworkEvent(id, node, data);
        break;

    case ECCore:
        switch (id)
        {
        case 1:
        {
            // Read payload discriminator 1007 before checking the local human player.
            // [UNRESOLVED] The discriminator's meaning. This requires a readable integer payload.
            if (*static_cast<const int*>(data) == 1007
                && world::playerManager->getLocalHumanPlayer())          // vt +0x30
            {
                const std::string& name =
                    settings->stringGet(std::string("GSPlayerName"), std::string("Player"));
                world::IPlayer* local = world::playerManager->getLocalHumanPlayer();  // asked twice
                changePlayerName(local->getId(), name);                  // vt +0x168
            }
            break;
        }

        case 9:
        {
            // The diagnostic-info collector, the same event BF2Engine::handleEvent's case 9 answers
            // and the same reinterpretation of `data`: a vector of key/value rows. Three rows, in
            // this order - a header with an empty value, then two counters formatted "%d".
            typedef std::vector<std::pair<std::string, std::string> > InfoRows;
            InfoRows& rows = *static_cast<InfoRows*>(data);

            rows.push_back(std::make_pair(std::string("- GameServer -"), std::string("")));
            rows.push_back(std::make_pair(std::string("# Connected Clients"),
                                          formatString("%d", getNumberOfConnectedClients())));
            rows.push_back(std::make_pair(std::string("# Active Ghosts"),
                                          formatString("%d", getTotalActiveGhosts())));
            break;
        }

        case 14:
            //  `cmp edx,0Eh` -> 0x80ACDB9 `mov byte ptr [edi+48h],1`. +0x48 is
            // Game::m_abortLoadingLevel, and GameServer::loadPath is its reader (0x80B1AD4, the
            // `cmp byte [eax+48h]` this header already records) - so ECCore event 14 is how a load
            // in progress is told to give up.
            m_abortLoadingLevel = true;                     // +0x48, on the Game base
            break;

        default:
            break;                                          // ids 2-8, 10-13 and 15+ do nothing
        }
        break;

    default:
        break;                                              // including ECGameLogic - see above
    }
}

// Handle HUD ids 18/19 by notifying the payload's connection id; 255 suppresses
// the send. Id 63 sets commander availability from a nonzero integer. All other
// ids are ignored, including loading notifications. node is unused.
// [CHOSEN] The callback return is void; callers discard any result.
void GameServer::handleHudEvent(unsigned int id, EventNode* node, void* data)
{
    (void)node;                                             // never read - see above

    if (id == 19)
    {
        const unsigned int connectionId = *reinterpret_cast<unsigned int*>(data);
        if (connectionId != 255)                            // the no-client sentinel
        {
            // the bool-isLocal ctor, isLocal = false - never asks PlayerManager
            game->sendGameEventIfConnected(connectionId,
                                           new PostRemoteEvent(ECHud, 19, false, 0, 0, 0.0f));
        }
    }
    else if (id > 19)
    {
        if (id == 63)
            m_mapInfo.setCommanderEnabled(*reinterpret_cast<int*>(data) != 0);   // tail call
    }
    else if (id == 18)
    {
        const unsigned int connectionId = *reinterpret_cast<unsigned int*>(data);
        if (connectionId != 255)
        {
            game->sendGameEventIfConnected(connectionId,
                                           new PostRemoteEvent(ECHud, 18, false, 0, 0, 0.0f));
        }
    }
    // every other id - including THE 3 AND 4 that loadPath posts - falls through to here and
    // returns. See the block above; this is the whole reason writing it unblocks the map load.
}

// Dispatches completed blocks: id 1 supplies client identity, id 6 invokes Python
// RemoteCommand, and other ids are ignored. The node owns the block; data is unused.
// Embedded NUL bytes truncate the string passed into Python.
void GameServer::handleDataBlock(EventNode* node, void* /*data*/)
{
    DataBlockReadyEvent* const block = static_cast<DataBlockReadyEvent*>(node);
    const unsigned int blockEventId = block->m_blockEventId;
    const int          connectionId = block->m_connectionId;

    if (blockEventId == 1)                                      // client info -> the join
    {
        ClientInfo clientInfo;
        clientInfo.setFromDataBlock(block->m_data, block->m_size);
        handleClientInfo(connectionId, &clientInfo);           // the join step itself
    }
    else if (blockEventId == 6)                                 // a python message -> script event 38
    {
        const std::string text(reinterpret_cast<const char*>(block->m_data), block->m_size);
        python::PyRef args = python::buildArgs(connectionId, text);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(38), args.get());
    }
}

// Validates joining identity and capacity, creates the remote player, then sends database/load data.
// Recording connections skip player validation. Ranked joins validate the lowercased name hash
// and backend identity; unranked joins trust the name. A duplicate-profile disconnect
// is queued without returning, so later checks still run.
namespace {
// The djb2 hash handleClientInfo computes inline over the lowercased claimed name (seed 5381,
// h = c ^ (33*h), walked to the NUL) to check the client's playerNameHash. Same hash AntiHacks uses.
// Unsigned arithmetic makes the 32-bit wrap defined; the result is compared as a signed int.
int djb2LowerName(const char* s)
{
    unsigned int h = 5381;
    for (int c = static_cast<unsigned char>(*s); c; c = static_cast<unsigned char>(*++s))
    {
        if (isupper(c))
            c = tolower(c);
        h = static_cast<unsigned int>(c) ^ (33u * h);
    }
    return static_cast<int>(h);
}
} // namespace

void GameServer::handleClientInfo(int id, ClientInfo* clientInfo)
{
    ServerConnection* const conn = reinterpret_cast<ServerConnection*>(getClient(id));
    if (!conn)
        return;

    if (!conn->getIsRecordingConnection())
    {
        // name validation: non-ranked is trusted; ranked hashes and defers to GameSpy
        bool validated;
        if (!serverSettings->getRanked())
        {
            validated = true;                                            // LAN: the name is trusted
        }
        else
        {
            AntiHacks::instance();
            if (djb2LowerName(clientInfo->m_playerName.c_str())
                == static_cast<int>(clientInfo->m_playerNameHash))
            {
                GSServerSideNameValidation* const validator = new GSServerSideNameValidation();
                addPersistenceConnection(static_cast<HTTPRequest*>(validator));
                validated = validator->validateSoldierName(
                    clientInfo->m_playerName, clientInfo->m_clanTag, clientInfo->m_profileId,
                    clientInfo->m_gameSpyAuthString, id, this);
            }
            else
            {
                validated = false;
            }
        }

        if (!validated)
        {
            kickPlayer(id, static_cast<io::PResult>(36), std::string());
            return;
        }

        // The displayed name is the clan tag, a space, then the name; the block is re-sent.
        clientInfo->m_playerName = clientInfo->m_clanTag + " " + clientInfo->m_playerName;
        clientInfo->m_shouldUpdateNetBuffer = true;

        // A ranked client with no profile id is not created yet - it is parked for disconnect.
        if (serverSettings->getRanked() && !clientInfo->m_profileId)
        {
            m_disconnectTypes[id] = DisconnectType(id, 13, std::string());
            return;
        }

        // reserved-slot gate
        const int maxPlayers   = settings->intGet("GSMaxNrOfPlayers", 64);
        const int humanPlayers = world::playerManager->getNumberOfHumanPlayers();
        const int reserved     = serverSettings->getNumReservedSlots();
        if (reserved && humanPlayers >= maxPlayers - reserved
            && !reservedSlots->isNickReserved(clientInfo->m_playerName))
        {
            kickPlayer(id, static_cast<io::PResult>(35), std::string());
            return;
        }

        // NO return after this kick - the ELF falls through to the duplicate-id test.
        if (clientInfo->m_profileId > 0
            && world::playerManager->getPlayerFromProfileId(clientInfo->m_profileId))
        {
            kickPlayer(id, static_cast<io::PResult>(32), std::string());
        }

        if (world::playerManager->getPlayerFromId(id))
        {
            kickPlayer(id, static_cast<io::PResult>(13), std::string());
            return;
        }

        // create and wire the remote player
        world::IPlayer* const player = createPlayerWithValidName(clientInfo->m_playerName, id);
        if (player)
        {
            player->setIsRemote(true);
            player->setProfileId(clientInfo->m_profileId);
            player->setVoipEnabled(clientInfo->m_usesVoip);
            conn->setPlayer(player);

            eventManager->postEvent(static_cast<EventCategory>(1), 4, player, 0.0f);

            sendGameEventToAllConnected(new CreatePlayerEvent(player, true));
            conn->m_gameEventManager->addEvent(new CreatePlayerEvent(player, false));

            std::string ip;
            if (getPlayerConnectionIp(player, ip))
                player->setPeerAddress(ip);

            python::PyRef args = python::buildArgs(player);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(14), args.get());

            int sessionId = player->getVoipSessionId();
            if (!sessionId)
                sessionId = VoipServerConnection::GetInstance()->CreateClientSessionId(player->getId());
            player->setVoipSessionId(static_cast<unsigned short>(sessionId));

            conn->m_gameEventManager->addEvent(new VoipSessionEvent(sessionId));
        }
    }

    // tail (recording or created): player-DB push, then level load if one is up
    clientSendPlayerDatabase(id);                                       // the player database (not the
                                                                        //   object DB clientSendDatabase)
    if (m_loadState)                                                     // ELF `if (this->loadPath)`;
        clientLoadLevel(id);                                            //   m_loadState is the load
}                                                                       // state member [INFERRED]

// Dispatch ECNetwork requests. Payload-bearing requests use RemoteEvent nodes;
// data is their payload pointer. Selection requests require an associated player.
// Object destruction also runs with zero clients. Event 19 deliberately falls
// through to the end-of-round acknowledgement in event 20.
// [UNRESOLVED] The ECGame event 1 posted during destruction has no identified handler.
void GameServer::handleNetworkEvent(unsigned int id, EventNode* node, void* data)
{
    RemoteEvent* const remote = static_cast<RemoteEvent*>(node);    // +0x20 m_data / +0x24 m_connectionId

    switch (id)
    {
    case 1:
        handleDataBlock(node, data);
        break;

    case 2:
        clientLoadComplete(remote->m_connectionId);
        break;

    case 4:
        clientSendDatabaseComplete(remote->m_connectionId);
        break;

    case 5:
        confirmReset(remote->m_connectionId);                                    // vt +0x74
        break;

    case 6:
    {
        const int spawnGroup = *static_cast<const int*>(remote->m_data);
        IClientConnection* c = getClient(remote->m_connectionId);
        if (c && c->getPlayer())
            gameLogic->selectSpawnGroup(c->getPlayer(), spawnGroup);             // IGameLogic +0x74
        break;
    }

    case 7:
    {
        const int team = *static_cast<const int*>(remote->m_data);
        IClientConnection* c = getClient(remote->m_connectionId);
        if (c && c->getPlayer())
            gameLogic->selectTeam(c->getPlayer(), team);                         // IGameLogic +0x78
        break;
    }

    case 8:
    {
        const int packed = *static_cast<const int*>(data);
        IClientConnection* c = getClient(remote->m_connectionId);
        if (c && c->getPlayer())
            gameLogic->selectKitAndUnlockLevel(c->getPlayer(), packed & 0xF, (packed & 0xF0) >> 4);   // +0x7C
        break;
    }

    case 11:
        handleStringReceivedEvent(static_cast<StringReceivedEvent*>(node), data);
        break;

    case 12:
    {
        IClientConnection* c = getClient(remote->m_connectionId);
        if (c && c->getPlayer())
        {
            world::IPlayer* player = c->getPlayer();
            if (player->getIsAlive())                                            // IPlayer +0x6C
            {
                world::IObject* const none = 0;
                python::PyRef args = python::buildArgs(player, player, none, none, none);
                python::pythonHost()->handleEvent(static_cast<ScriptEvent>(1), args.get());
            }
            gameLogic->killPlayer(player, true, true);                           // IGameLogic +0x6C
        }
        break;
    }

    case 13:
        handleRadioMessageReceivedEvent(static_cast<RadioMessageReceivedEvent*>(node), data);
        break;

    case 17:
    {
        world::IObject* const object = static_cast<world::IObject*>(data);
        eventManager->postEvent(ECGame, 1, object, 0.0f);                        // EventManager +0x14
        io::INetworkable* const networkable = object->getNetworkable();          // IObject slot 54
        if (networkable)
        {
            const unsigned short netId = networkable->m_networkableId;           // +0x04
            if (netId)
            {
                io::NetworkableDescriptor* const desc = io::networkManager->getDescriptor(netId);
                for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
                     it != m_connections.end(); ++it)
                    asServerConnection(it->second)->updateGhostable(object, false);
                removeGhost(desc);                                                   // vt +0x140
                DestroyObjectEvent* event =
                    new DestroyObjectEvent(object->getNetworkable()->m_networkableId);   // 0x10 B
                sendGameEventToAllPlaying(event);                                    // vt +0x84
                if (!object->getObjectCacheID())                                     // IObject slot 11
                    io::networkManager->removeNetworkable(networkable->m_networkableId);
            }
        }
        break;
    }

    case 18:
    {
        const int minusOne = -1;
        const char* const text = static_cast<const char*>(data);
        python::PyRef args = python::buildArgs(minusOne, text);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(38), args.get());
        break;
    }

    case 19:
    {
        struct SendDataBlockRequest { int connectionId; std::string text; };      // [INFERRED] shape: +0x00 int, +0x04 string
        SendDataBlockRequest* const request = static_cast<SendDataBlockRequest*>(data);
        if (request->connectionId == -1)
        {
            std::string line(request->text);
            line.append("\n", std::strlen("\n"));
            io::mainConsole->output(line);                                       // OldConsole +0x14
        }
        else
        {
            sendDataBlock(request->connectionId, 7u, request->text.c_str(),
                          static_cast<unsigned int>(request->text.size()), true);
        }
        // no break - falls through into 20, as shipped (0x80ACAAB -> 0x80ACAB0)
    }
    case 20:
        clientAckedEndOfRoundGuard(remote->m_connectionId);
        break;

    default:
        break;
    }
}

// : `cmp byte [this+15Dh],0; jz; inc dword [this+160h]`. One more client has acked
// the end of the round - counted only while waitForClients has the server waiting for them.
void GameServer::clientAckedEndOfRoundGuard(int /*connectionId*/)
{
    if (m_isWaitingForClients)                                                   // +0x15D
        ++m_clientAckedEndOfRoundGuard;                                          // +0x160
}

// . NOT const in the ELF, and the header agrees - the status screen's local shape
// declaration had it const, which is why it could never link.
io::NetServer* GameServer::getNetServer()     { return m_netServer; }               // +0x64

// Counts state-4 connections as connected and all other states as connecting; excludes demo recorders.
int GameServer::getNumberOfConnectedClients() const
{
    int count = 0;
    for (std::map<unsigned char, IClientConnection*>::const_iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        IClientConnection* connection = it->second;
        if (reinterpret_cast<ClientConnection*>(connection)->m_connectionState == 4 &&
            !connection->getIsRecordingConnection())
            ++count;
    }
    return count;
}

int GameServer::getNumberOfConnectingClients() const
{
    int count = 0;
    for (std::map<unsigned char, IClientConnection*>::const_iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        IClientConnection* connection = it->second;
        if (reinterpret_cast<ClientConnection*>(connection)->m_connectionState != 4 &&
            !connection->getIsRecordingConnection())
            ++count;
    }
    return count;
}


// getClient - ELF 0x80B3880, 31 bytes (_ZNK...9getClientEi - a const member): a one-line forwarder
// to the virtual getConnection (primary vtable +0x158, the slot the class dump names). The
// GameServer override of getConnection is unread - the virtual stays a frontier symbol, and this
// forwarder is never reached by the boot probe (only the per-connection send arm calls it).
IClientConnection* GameServer::getClient(int connectionId) const
{
    return getConnection(connectionId);                     // virtual +0x158
}

// Narrows the ID to an unsigned byte before lookup, so ID 256 aliases ID 0.
IClientConnection* GameServer::getConnection(int connectionId) const
{
    const unsigned char key = (unsigned char)connectionId;   // silent narrowing - see above

    std::map<unsigned char, IClientConnection*>::const_iterator it = m_connections.find(key);
    if (it == m_connections.end())
        return 0;
    return it->second;
}

// getConnectionState - ELF 0x80B38A0, 39 B / PE 0x4437A0. Both ports perform the virtual
// getConnection/getClient lookup and return zero for a missing connection; otherwise they return
// ClientConnection +0x0C verbatim. ServerVoteManager uses this to require state 4 before accepting
// a kick-vote request.
int GameServer::getConnectionState(int connectionId) const
{
    IClientConnection* const connection = getClient(connectionId);
    if (!connection)
        return 0;
    return static_cast<ClientConnection*>(connection)->m_connectionState;
}

// Small concrete IGame overrides. These bodies are shared by the ELF and PE builds; each address
// below is the ELF entry point. They close real GameServer vtable slots rather than relying on the
// boot probe's former placeholders.
bool GameServer::load()
{
    return false;
}

bool GameServer::unload()
{
    eventManager->postEvent(ECHud, 3, 0, 0.0f);

    const std::list<world::IPlayer*> players(world::playerManager->getPlayers());
    std::list<world::IPlayer*> aiPlayers;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
    {
        if ((*it)->getIsAIPlayer())
            aiPlayers.push_back(*it);
    }
    for (std::list<world::IPlayer*>::iterator it = aiPlayers.begin();
         it != aiPlayers.end(); ++it)
    {
        destroyPlayer(*it);
    }

    const bool unloaded = gameLogic->unload();
    if (unloaded)
        m_loadState = 0;

    clientUnloadLevel(-1);
#if defined(_WIN32)
    // PE-only retail tail (0x448637..0x448672). The Linux dedicated server goes directly from
    // clientUnloadLevel to Python teardown; the Windows build clears a live aidll instance first.
    ai::IAI* aiInstance = static_cast<ai::IAI*>(
        classManager->getSimpleSingleton(std::string("AI")));
    if (aiInstance && aiInstance->isInitialized())
        aiInstance->clear();
#endif
    python::pythonHost()->uninitializeGameMode();
    return unloaded;
}

StatsMonitor* GameServer::getStatsMonitor() const
{
    return m_statsMonitor;
}

int GameServer::getCurrentInputIndex()
{
    return m_currentInputIndex;
}

bool GameServer::destroyObject(world::IObject* object)
{
    return Game::destroyObject(object);
}

bool GameServer::radioMessage(TargetChannel channel, const std::string& message,
                              world::IPlayer* player, unsigned int extra,
                              bool /*unused*/, bool bypassSpamCheck)
{
    if (!player)
        return true;

    // Retail performs this virtual call even though it discards the result.
    world::playerManager->getLocalHumanPlayer();

    int messageId;
    const std::string empty1;
    const std::string empty2;
    if (static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, 0, 0, message, empty1, empty2) &&
        (bypassSpamCheck || !player->getIsThisASpam(true)))
    {
        EventNode* node = createRadioMessageReceivedEvent(
            -1,
            static_cast<unsigned char>(messageId),
            static_cast<unsigned char>(player->getId()),
            static_cast<unsigned char>(player->getTeam()),
            static_cast<unsigned char>(extra),
            static_cast<int>(channel), 3);
        eventManager->postEvent(ECNetwork, node, 0.0f);
    }
    return true;
}

bool GameServer::radioMessageFromPlayer(TargetChannel channel, const std::string& message,
                                        unsigned int playerId, unsigned int extra,
                                        bool /*unused*/, bool bypassSpamCheck)
{
    world::IPlayer* player = world::playerManager->getPlayerFromId(static_cast<int>(playerId));
    if (!player)
        return true;

    int messageId;
    const std::string empty1;
    const std::string empty2;
    if (static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, 0, 0, message, empty1, empty2) &&
        (bypassSpamCheck || !player->getIsThisASpam(true)))
    {
        const int connectionId = player->getIsAIPlayer()
                                   ? static_cast<unsigned char>(playerId)
                                   : 0xFF;
        EventNode* node = createRadioMessageReceivedEvent(
            connectionId,
            static_cast<unsigned char>(messageId),
            static_cast<unsigned char>(playerId),
            static_cast<unsigned char>(player->getTeam()),
            static_cast<unsigned char>(extra),
            static_cast<int>(channel), 3);
        eventManager->postEvent(ECNetwork, node, 0.0f);
    }
    return true;
}

bool GameServer::gameInfoMessage(TargetChannel channel, const std::string& message,
                                 unsigned int a, unsigned int b, bool /*unused*/)
{
    int messageId;
    const std::string empty1;
    const std::string empty2;
    if (static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, 0, 0, message, empty1, empty2))
    {
        EventNode* node = createRadioMessageReceivedEvent(
            -1,
            static_cast<unsigned char>(messageId),
            static_cast<unsigned char>(a),
            static_cast<unsigned char>(b),
            static_cast<unsigned char>(b),
            static_cast<int>(channel), 0);
        eventManager->postEvent(ECNetwork, node, 0.0f);
    }
    return true;
}

// Marks a known connection reset-complete, allowing ghost replication in playing
// state. Unknown connection IDs are ignored.
void GameServer::confirmReset(int connectionId)
{
    IClientConnection* connection = getClient(connectionId);
    if (connection)
        static_cast<ClientConnection*>(connection)->m_bReset = true;      // ELF +0x10
}

// Decrements the load/unload balance and advances to state 2 only when it reaches
// zero. An already-zero balance still decrements after reporting a mismatch.
// Preserve repeated player reads and the VOIP-before-ID order.
void GameServer::clientLoadComplete(int connectionId)
{
    ServerConnection* const conn = asServerConnection(getClient(connectionId));
    if (!conn)
        return;

    if (conn->m_loadUnload == 0)                         // a load-complete with nothing outstanding
        Debug(DEBUGTYPE_6, "Game/Common/ServerConnection.h", 54) << "Mismatching load/unload";

    conn->m_loadUnload = conn->m_loadUnload - 1;         // one pending load finished
    if (conn->m_loadUnload != 0)                         // more still pending - wait for them
        return;

    conn->m_connectionState = 2;                         // fully loaded - playing-ready

    if (conn->getPlayer())                               // getPlayer #1 (the null test)
    {
        const bool voip = conn->getPlayer()->getVoipEnabled();  // #2, then IPlayer +0x26C
        const int  pid  = conn->getPlayer()->getId();           // #3, then IPlayer +0x54
        updatePlayersVoipStatusOnAllClients(pid, voip);
    }
}

// updatePlayersVoipStatusOnAllClients - 0x80B6060, 135 B. IGameServer +0x68. Looks the player up by
// id, stores the VoIP flag on it (Player::setVoipEnabled, IPlayer +0x290), and broadcasts a
// VoipOnOffEvent to every connected client so their rosters agree. clientLoadComplete calls it once a
// joining player's connection reaches the loaded state; VoipOnOffEvent::executeServer relays it too.
void GameServer::updatePlayersVoipStatusOnAllClients(int playerId, bool status)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (player)
    {
        player->setVoipEnabled(status);                 // IPlayer +0x290
        sendGameEventToAllConnected(new VoipOnOffEvent(playerId, status));
    }
}

// Sends object and squad state, advances to state 3, arms reset, and marks the player connected.
// Requires an existing connection and squad manager.
void GameServer::clientSendDatabase(int connectionId)
{
    ServerConnection* const conn = asServerConnection(getClient(connectionId));
    conn->clientSendDatabase();                          // the object/networkable DB push
    static_cast<ISquadManager*>(squadManager)->sendSquadManagerState(connectionId);
    conn->m_connectionState = 3;                         // database-sent
    conn->m_bReset          = true;                      // +0x10, re-armed on every DB push
    if (conn->getPlayer())
        conn->getPlayer()->setIsConnected(true);         // IPlayer +0x1B4
}

// Accepts the database acknowledgement when content checks pass, are disabled, or this is the demo connection.
// Moves the connection to playing and sends proceed/game-status events. A failed content check
// queues disconnect reason 27; a missing connection is ignored.
void GameServer::clientSendDatabaseComplete(int connectionId)
{
    IClientConnection* const client = getClient(connectionId);
    if (!client)
        return;

    if (connectionId == 129
        || !serverInfo->m_block.useContentChecking
        || client->getContentValid())                                            // vslot +0x30
    {
        asServerConnection(client)->m_connectionState = 4;                       // database acked

        sendGameEvent(connectionId,
                      new PostRemoteEvent(ECNetwork, 3, false, 0, 0, 0.0f));     // vt +0x90

        int gameStatus = gameLogic->getGameStatus();                             // IGameLogic +0x158
        sendGameEvent(connectionId,
                      new PostRemoteEvent(ECGameLogic, 1, false, &gameStatus, 4, 0.0f));
    }
    else
    {
        m_disconnectTypes[connectionId] = DisconnectType(connectionId, 27, std::string());
    }
}

//  / dedicated PE 0x4499B0. Preserve the missing-iterator and
// null-connection quirks.
void GameServer::onContentCheckEvent(int connectionId, const unsigned char* miscHash,
                                   const unsigned char* archiveHash, const unsigned char* levelHash)
{
    IClientConnection* const client = getClient(connectionId);
    if (client && static_cast<ClientConnection*>(client)->m_connectionState <= 1)
        return;
    if (!serverInfo->m_block.useContentChecking)
        return;

    const std::string misc = io::hash_to_string(miscHash);
    const std::string archive = io::hash_to_string(archiveHash);
    const std::string level = io::hash_to_string(levelHash);
    const int ordinal = m_mapInfo.getChallengeOrdinal();
    bool valid = false;
    const auto standard = m_stdArchiveMd5.find(ordinal);
    const auto standardMod = m_stdArchiveModMd5.find(ordinal);
    const auto alternate = m_bstArchiveMd5.find(ordinal);
    const auto alternateMod = m_bstArchiveModMd5.find(ordinal);
    if ((standard != m_stdArchiveMd5.end() || standardMod != m_stdArchiveModMd5.end() ||
         alternate != m_bstArchiveMd5.end() || alternateMod != m_bstArchiveModMd5.end()) &&
        m_miscHashMd5 == misc)
    {
        if (!m_levelFingerprint)
            valid = true; // Retail skips both archive and level comparison in this arm.
        else
        {
            const auto fingerprint = m_levelFingerprintStrings.find(ordinal);
            // Retail checks only whether ANY archive entry exists above. It does not guard
            // the individual iterators before these short-circuited dereferences.
            if (fingerprint != m_levelFingerprintStrings.end() &&
                (standard->second == archive || standardMod->second == archive ||
                 alternate->second == archive || alternateMod->second == archive) &&
                fingerprint->second == level)
                valid = true;
        }
    }

    // This virtual lookup is retained even on success, when its result is discarded.
    world::IPlayer* const player = world::playerManager->getPlayerFromId(connectionId);
    if (valid)
    {
        IClientConnection* const connection = getConnection(connectionId);
        if (connection)
            connection->setContentValid(true);
    }
    else
    {
        std::stringstream message;
        std::string playerName;
        if (player)
            playerName = std::string(" (") + player->getName() + std::string(")");
        message << "dep: kicking client " << connectionId << playerName << " for using modified data";
        io::mainConsole->output(message.str());
        m_disconnectTypes[connectionId] = DisconnectType(connectionId, 27, std::string());
    }
}

void GameServer::sendGameEventIfPlaying(unsigned int connectionId, GameEvent* event)
{
    _sendGameEvent(connectionId, event, true, true);
}

void GameServer::sendGameEventIfConnected(unsigned int connectionId, GameEvent* event)
{
    _sendGameEvent(connectionId, event, true, false);
}

bool GameServer::isPlayingDemo()
{
    return false;
}

// Drains demo connection 129 through normal transmit. Requires a connection and
// recorder; packet length uses the stream's high-water mark, not its cursor.
void GameServer::flushDemoPackets(ServerConnection* conn, int tick)
{
    while (conn->transmitMsgs())
    {
        const unsigned int bits = conn->m_bitStream->m_highWaterBits;
        unsigned int bytes = bits >> 3;
        if ((bits & 7) != 0)
            ++bytes;
        if (!bytes)                                          // nothing assembled - stop pumping
            break;

        m_demoRecorder->writeFakePacketData(conn->m_bitStream, tick);

        // Retire the packet we just captured. The flag goes in true; handlePacketStatusNotification
        // walks its pending list and returns false once it has nothing left to report.
        bool delivered = true;
        while (conn->handlePacketStatusNotification(delivered))
        {
        }
    }
}

// Announces a processed auto-recording to every connection that has started joining, as data
// block 9. The URL is sv.demoDownloadURL with its first "{file}" replaced by the file name, or with
// the name appended when there is no placeholder. Retail sends through the global game, not this.
void GameServer::onAutoRecordingAvailable(const std::string& fileName,
                                          const std::string& mapName)
{
    const std::string placeholder("{file}");
    std::string url(serverSettings->m_demoDownloadURL);
    const std::string::size_type at = url.find(placeholder);
    if (at == std::string::npos)
        url.append(fileName);
    else
        url.replace(at, placeholder.size(), fileName);

    DemoAvailableInfo info;
    info.setUrl(url);
    info.setMapName(mapName);
    const unsigned int size = info.getNetBufferSize();      // both images pack before taking the pointer
    const void* const buffer = info.getNetBuffer();
    static_cast<GameServer*>(game)->sendDataBlockToAll(9, buffer, size, true);
}

void GameServer::recordDemo(DemoRecorder* recorder)
{
    io::mainConsole->output(std::string("recording demo...\n"));
    m_demoRecorder = recorder;

    const int demoConnectionId = 129;
    onNewConnection(demoConnectionId);
    ServerConnection* connection =
        reinterpret_cast<ServerConnection*>(getClient(demoConnectionId));
    handleClientInfo(demoConnectionId, 0);
    flushDemoPackets(connection, -1);
    clientLoadComplete(demoConnectionId);
    flushDemoPackets(connection, -1);
    clientSendDatabase(demoConnectionId);
    flushDemoPackets(connection, WorldPref::getTickCount());
    clientSendDatabaseComplete(demoConnectionId);
    flushDemoPackets(connection, WorldPref::getTickCount());
    m_demoRecorder->writeSpecialPacket(DemoRecorderSpecialPacket_InitialState);

    bool status = true;
    while (connection->handlePacketStatusNotification(status))
    {
    }
}

void GameServer::stopRecording()
{
    io::mainConsole->output(std::string("stopping recording"));
    m_disconnectTypes[129] = DisconnectType(129, 13, std::string());
    m_demoRecorder = 0;
}

void GameServer::setSimulationRate(int)
{
}

int GameServer::getSimulationRate()
{
    return g_system->getSimulationRate();
}

void GameServer::addTextToChatWindow(const char* text)
{
    io::mainConsole->output(std::string(text));
}

bool GameServer::isPunkBusterEnabled() const
{
    return m_isPunkBusterEnabled;
}

// : `mov eax, [this+0FCh]`. for ServerGameLogic::restartMap, whose
// first act is to ask for the recorder and tell it the round finished. This tree's init never news
// one (see the header's +0xFC note), so the answer is 0 and restartMap skips the recorder arm.
AutoRecorder* GameServer::getAutoRecorder()                  // +0x1C4
{
    return m_autoRecorder;                                   // +0xFC
}

void GameServer::shutdown()
{
    if (m_shutdownComplete)
        return;

    unload();
    Game::destroyAllPlayers();
    Game::unregisterStaticNetworkables();
    VoipServerConnection::ReleaseInstance();

    if (python::pythonHost()->isInitialized())
        python::pythonHost()->shutdown();

    io::networkManager->clear();

    delete m_bitStream;
    m_bitStream = 0;

    if (m_netServer)
    {
        m_netServer->shutDown();                             // +0x14
        delete m_netServer;                                  // vt +0x94 (D0, 0x80A84C8) / MSVC's deleting dtor
        m_netServer = 0;
    }

    if (m_matchMakingServer)
        destroyMatchMakingServer(m_matchMakingServer);
    if (world::stickyProjectileManager)
        delete world::stickyProjectileManager;

    if (squadManager)
    {
        if (squadManager->release() != 0)
            std::abort();
        squadManager = 0;
    }
    if (m_statsManager)
        m_statsManager->release();

    m_shutdownComplete = true;
    eventManager->unregisterEventHandler(ECGameLogic, this);
    eventManager->unregisterEventHandler(ECNetwork, this);
    eventManager->unregisterEventHandler(ECCore, this);
    eventManager->unregisterEventHandler(ECHud, this);
}

GameServer::GameServer()
{
    m_refCount = 1;                                          // +0x58
    m_frameCount = 0;                                        // +0x60
    m_netServer = 0;                                         // +0x64
    m_numPlayers = 0;                                        // +0x80
    m_maxConnectionType = 5;                                           // +0x84
    m_bandwidthChoke = 0;                                    // +0x88
    // m_currentInputIndex (+0x8C) is deliberately not initialized by retail.
    m_svStartTime = 0;                                       // +0x90
    // m_port (+0x94) is deliberately not initialized by retail.
    m_loadState = 0;                                         // +0xA0
    m_bitStream = 0;                                         // +0xA4
    m_messageDistanceSqA = 10000.0f;                         // +0xA8
    m_messageDistanceSqB = 225.0f;                           // +0xAC
    m_messageDistanceSqC = 10000.0f;                         // +0xB0
    m_messageDistanceSqD = 10000.0f;                         // +0xB4
    m_sampleStreamManagerReadStats = false;                  // +0xB8
    m_networkableStats = 0;                                  // +0xBC (allocated after event registration)
    m_mapListSendInterval = 900;                             // +0xC0
    m_mapListSendCounter = 0;                                // +0xC4
    m_serverInfoSendInterval = 30;                           // +0xC8
    m_serverInfoSendCounter = 0;                             // +0xCC
    m_demoRecorder = 0;                                      // +0xD0
    m_matchMakingServer = 0;                                 // +0xD4
    m_statsManager = 0;                                      // +0xD8
    m_currentPersistenceObject = 0;                          // +0xEC
    m_httpUpdateCounter = 0;                                 // +0xF0
    m_httpUpdateInterval = 30;                               // +0xF4
    m_shutdownComplete = false;                              // +0xF8
    m_autoRecorder = 0;                                      // +0xFC
    m_numberOfDroppedFrames = 0;                             // +0x100
    m_numberOvershotFrames = 0;                              // +0x104
    m_isInternetServer = false;                              // +0x108
    m_initComplete = false;                                  // +0x109
    m_statsMonitor = 0;                                      // +0x10C (allocated after event registration)
    m_useContentChecking = false;                            // +0x150
    m_levelFingerprint = 0;                                  // +0x151
    m_radioMessagesPerTick = 2;                              // +0x158
    // m_isPunkBusterEnabled (+0x15C) is assigned by init(), not by either retail ctor.
    m_isWaitingForClients = false;                           // +0x15D
    m_clientAckedEndOfRoundGuard = 0;                        // +0x160
    m_playerUnlockInfo = 0;                                  // +0x164

    eventManager->registerEventHandler(ECNetwork, this, 0);
    eventManager->registerEventHandler(ECCore, this, 0);
    eventManager->registerEventHandler(ECHud, this, 0);
    eventManager->registerEventHandler(ECGameLogic, this, 0);

    (void)new world::StickyProjectileManager;
    m_networkableStats = new io::BasicNetworkStats;
    if (!m_networkableStats)
        Debug(DebugTypeError, "Game/GameServer/GameServer.cpp", 552, "")
            << "Failed to create networkableStats";
    m_statsMonitor = new StatsMonitor;
    m_pythonValid[0] = false;                                // +0x168
    m_pythonValid[1] = false;                                // +0x169
}

GameServer::~GameServer()
{
    // shutdown() owns the protocol objects. The destructor only tears down these four owned
    // allocations before C++ destroys the containers and the Game base in reverse field order.
    if (m_autoRecorder)
        delete m_autoRecorder;
    if (m_networkableStats)
    {
        delete m_networkableStats;
        m_networkableStats = 0;
    }
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        if (it->second)
            delete it->second;
    }
    delete m_statsMonitor;
}

IBase* GameServer::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    GameServer* server = new GameServer;
    IBase* result = static_cast<IBase*>(server->queryInterface(iid));
    if (!result)
        server->release();
    return result;
}

void GameServer::addRef()  { ++m_refCount; }
int  GameServer::getRef()  { return m_refCount; }
int  GameServer::release()
{
    const int r = --m_refCount;
    if (r == 0) delete this;
    return r;
}
void* GameServer::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase || iid == IID_IGame)
        return static_cast<IGame*>(const_cast<GameServer*>(this));
    if (iid == IID_IGameServer)
        return static_cast<IGameServer*>(const_cast<GameServer*>(this));   // this + 0x50
    return 0;
}

// Routes descriptor changes to per-connection ghosts according to connection state.
// Scopable objects use a 500-unit distance range; static ghosts use priority -1.
namespace {
void serverConnectionAddScopableGhost(IClientConnection* connection, io::NetworkableDescriptor* descriptor, float range)
{
    reinterpret_cast<ServerConnection*>(connection)->addScopableGhost(descriptor, range);
}
void ghostManagerRemoveGhostObject(IClientConnection* connection, io::NetworkableDescriptor* descriptor)
{
    reinterpret_cast<ServerConnection*>(connection)->m_ghostManager->removeGhostObject(descriptor);
}
void ghostManagerGhostObject(IClientConnection* connection, io::NetworkableDescriptor* descriptor, float priority, int flag)
{
    reinterpret_cast<ServerConnection*>(connection)->m_ghostManager
        ->ghostObject(descriptor, priority, flag != 0);
}

// Reads concrete connection state behind the shared client-connection interface.
int clientConnectionState(IClientConnection* connection)
{
    return reinterpret_cast<ClientConnection*>(connection)->m_connectionState;
}
} // namespace

void GameServer::addScopableGhost(io::NetworkableDescriptor* descriptor)
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        if (clientConnectionState(it->second) > 3)
            serverConnectionAddScopableGhost(it->second, descriptor, 500.0f);
    }
}
void GameServer::removeGhost(io::NetworkableDescriptor* descriptor)
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
        ghostManagerRemoveGhostObject(it->second, descriptor);
}
void GameServer::addStaticGhost(io::NetworkableDescriptor* descriptor)
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        if (clientConnectionState(it->second) > 2)
            ghostManagerGhostObject(it->second, descriptor, -1.0f, 0);
    }
}

// Object creation registers networkables before broadcasting their creation events.
// Event sends consume the caller's reference even when state filters suppress delivery.
// Playing/connected filters and end-of-round exclusions are shared by single and broadcast sends.
namespace {
void ghostManagerResetGhost(IClientConnection* connection, io::NetworkableDescriptor* descriptor)
{
    reinterpret_cast<ServerConnection*>(connection)->m_ghostManager->resetGhost(descriptor);
}
// Clears the connection's reset-complete flag before resetting its GhostManager.
void serverConnectionReset(IClientConnection* connection)
{
    ClientConnection* const c = static_cast<ClientConnection*>(connection);
    c->m_bReset = false;                                                  // +0x10
    c->m_ghostManager->reset();                                           // +0x2C
}

// Borrowed radio/string event adapters for per-connection relay and server-local
// playback. Typed server-interface casts preserve secondary-base adjustments on each ABI.
class RadioMessageEW : public GameServer::IEventWrapper
{
public:
    explicit RadioMessageEW(RadioMessageReceivedEvent* event) : m_event(event) {}
    virtual int getSenderId() { return m_event->m_connectionId; }             //  +0x1C
    virtual int getChannel()  { return m_event->m_channel; }                  //  +0x24
    virtual int getFlags()    { return m_event->m_field0F; }                  //  +0x20
    virtual void sendMessage(ServerConnection* connection)
    {
        if (!connection)
            return;
        world::IPlayer* const player = connection->getPlayer();
        if (!player)
            return;

        RadioMessageEvent* const event = new RadioMessageEvent();
        event->m_messageId = m_event->m_messageId;
        event->m_senderId  = m_event->m_connectionId;                         // anti-spoof stamp
        event->m_team      = m_event->m_team;
        event->m_field0F   = m_event->m_field0F;
        event->m_channel   = m_event->m_channel;
        event->m_type      = m_event->m_type;
        // Retail dereferences the query result without a second guard. A live GameServer must
        // answer IID_IGame; keeping this as one call expression also leaves the pre-C++17
        // query/getId evaluation order to each compiler, as in the two shipped ports.
        static_cast<IGame*>(game ? game->queryInterface(IID_IGame) : 0)
            ->sendGameEventIfPlaying(player->getId(), event);                 // IGame +0x88
    }
protected:
    RadioMessageReceivedEvent* m_event;
};

class ServerRadioMessageEW : public RadioMessageEW
{
public:
    explicit ServerRadioMessageEW(RadioMessageReceivedEvent* event) : RadioMessageEW(event) {}
    virtual void sendMessage(ServerConnection*)
    {
        IGameServer* const serverInterface = game
            ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer))
            : 0;
        GameServer* const server = serverInterface
            ? static_cast<GameServer*>(serverInterface)
            : 0;
        // The unguarded call after the conditional adjustment is shipped behaviour.
        server->playbackRadioMessage(static_cast<TargetChannel>(m_event->m_channel),
                                     static_cast<ChannelMessageType>(m_event->m_type),
                                     m_event->m_team, m_event->m_senderId,
                                     m_event->m_messageId);
    }
};

class StringMessageEW : public GameServer::IEventWrapper
{
public:
    explicit StringMessageEW(StringReceivedEvent* event) : m_event(event) {}
    virtual int  getSenderId() { return m_event->m_senderId; }              //   +0x1C
    virtual int  getTargetId() { return m_event->m_targetId; }              //   +0x20
    virtual int  getChannel()  { return m_event->m_channel; }               //   +0x28
    virtual int  getFlags()    { return m_event->m_flags; }                 //   +0x24
    virtual void sendMessage(ServerConnection* connection)
    {
        const std::string text(reinterpret_cast<const char*>(m_event->m_message),
                               m_event->m_messageLength);
        connection->getStringManager()->enqueueString(                       // +0x14 -> slot 0
            static_cast<TargetChannel>(m_event->m_channel),
            static_cast<ChannelMessageType>(m_event->m_type),
            m_event->m_senderId, text, m_event->m_flags, 0);
    }
protected:
    StringReceivedEvent* m_event;                                           // +0x04
};

class ServerStringMessageEW : public StringMessageEW
{
public:
    explicit ServerStringMessageEW(StringReceivedEvent* event) : StringMessageEW(event) {}
    virtual void sendMessage(ServerConnection*)
    {
        const std::string text(reinterpret_cast<const char*>(m_event->m_message),
                               m_event->m_messageLength);
        Game* const theGame = game ? static_cast<Game*>(game) : 0;         // called even when null, as shipped
        theGame->outputChatString(static_cast<TargetChannel>(m_event->m_channel),
                                  static_cast<ChannelMessageType>(m_event->m_type),
                                  m_event->m_senderId, m_event->m_targetId, text);
    }
};

const int GEDestroyObject = 7;     // .rodata 0x8889124
const int GEEnterVehicle  = 9;     // .rodata 0x88893E8
const int GEExitVehicle   = 10;    // .rodata 0x8889480

inline bool eventSuppressedAtRoundEnd(GameEvent* event)
{
    if (!gameLogic || gameLogic->getGameStatus() != 3)
        return false;
    const int type = event->getType();
    return type == GEDestroyObject || type == GEEnterVehicle || type == GEExitVehicle;
}
} // namespace

world::IObject* GameServer::createObjectOnAllClients(const world::IObjectTemplate* objectTemplate,
                                                     Vec3 position, Vec3 rotation, bool playingOnly,
                                                     int /*unused*/, int defaultTeam)
{
    if (!objectTemplate)
        return 0;
    Mat4 transform = Mat4::getIdentity();
    if (objectTemplate->getClassID() == world::CID_ZiplineTemplate)
    {
        transform.m[4] = rotation.x;                       // the zipline's second point, row 1
        transform.m[5] = rotation.y;
        transform.m[6] = rotation.z;
    }
    else
        setRotationZXY(transform, -rotation.z, -rotation.y, -rotation.x);
    transform.m[12] = position.x;
    transform.m[13] = position.y;
    transform.m[14] = position.z;

    world::IObject* object = world::objectManager->createObject(objectTemplate, 0, &transform);   // +0x10
    if (!object)
        return 0;
    object->updateFlags(0x20000u, 0);                                                    // slot 24
    for (world::IObject* o = object; o; o = o->getParent())
    {
        if (world::IPlayerControlObject* pco = static_cast<world::IPlayerControlObject*>(
                o->queryInterface(world::IID_IPlayerControlObject)))
        {
            pco->setDefaultTeam(defaultTeam);                                             // ipco +0x90
            break;
        }
    }
    eventManager->postEvent(ECGame, 3, object, 0.0f);                                    // +0x14

    io::INetworkable* networkable = object->getNetworkable();                            // slot 54
    if (!networkable)
        return object;
    if (networkable->m_networkableId)
    {
        resetGhost(io::networkManager->getDescriptor(networkable->m_networkableId));
    }
    else
    {
        networkable->m_isInNetwork = true;                                                // +0x14
        io::networkManager->addNetworkable(networkable);
    }
    const bool isKit = object->getTemplate()->getClassID() == world::CID_KitTemplate;
    GameEvent* event;
    if (isKit)
        event = new CreateKitEvent(objectTemplate->getId(), networkable->m_networkableId,
                                   world::kitCreationIndex(object), world::kitChosenUnlockLevel(object),
                                   position);
    else
        event = new CreateObjectEvent(objectTemplate->getId(), networkable->m_networkableId,
                                      defaultTeam, position, rotation);
    if (playingOnly)
        sendGameEventToAllPlaying(event);                                                 // +0x84
    else
        sendGameEventToAllConnected(event);                                               // +0x94
    return object;
}

void GameServer::sendGameEventToAllPlaying(GameEvent* event)   { _sendGameEventToAll(event, true, true); }
void GameServer::sendGameEventToAll(GameEvent* event)          { _sendGameEventToAll(event, false, false); }
void GameServer::sendGameEvent(unsigned int connectionId, GameEvent* event) { _sendGameEvent(connectionId, event, false, false); }
void GameServer::sendGameEventToAllConnected(GameEvent* event) { _sendGameEventToAll(event, true, false); }

void GameServer::_sendGameEventToAll(GameEvent* event, bool connectedOnly, bool playingOnly)
{

    if (!eventSuppressedAtRoundEnd(event))
    {
        for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            const int state = clientConnectionState(it->second);
            const bool send = playingOnly ? (state > 2) : (!connectedOnly || state > 0);
            if (send)
            {
                event->addRef();
                reinterpret_cast<ClientConnection*>(it->second)->m_gameEventManager->addEvent(event);  // +0x28 -> addEvent (+0x3C)
            }
        }
    }
    if (event)
        event->release();
}

// Clears reset latches and resets each connection's ghosts after network IDs regress.
void GameServer::resetConnections()
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
        serverConnectionReset(it->second);
}

void GameServer::_sendGameEvent(unsigned int connectionId, GameEvent* event, bool connectedOnly, bool playingOnly)
{
    IClientConnection* connection = getClient(static_cast<int>(connectionId));
    if (connection && !eventSuppressedAtRoundEnd(event))
    {
        const int state = clientConnectionState(connection);
        const bool send = playingOnly ? (state > 2) : (!connectedOnly || state > 0);
        if (send)
        {
            event->addRef();
            reinterpret_cast<ClientConnection*>(connection)->m_gameEventManager->addEvent(event);  // +0x28 -> addEvent (+0x3C)
        }
    }
    if (event)
        event->release();
}

//  / PE 0x446100, 181 B: every connection past state 2 - reset the ghost,
// then re-add it as a scopable ghost when the networkable's base priority (+0x10) is nonnegative,
// as a static ghost otherwise
// (-1.0f = a destroyable / static). The networkable is the descriptor's first property's first.
void GameServer::resetGhost(io::NetworkableDescriptor* descriptor)
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        if (clientConnectionState(it->second) > 2)
        {
            ghostManagerResetGhost(it->second, descriptor);
            io::INetworkable* networkable = descriptor->properties[0].networkables[0];
            if (networkable->m_basePriority >= 0.0f)
                addScopableGhost(descriptor);                        // virtual
            else
                addStaticGhost(descriptor);                          // virtual
        }
    }
}

//  / PE 0x448970 (mislabelled PB_get_sv_punkbusterInstalled in the PE IDB).
// Existing entries survive; duplicates overwrite. A token ending exactly at EOF is discarded
// because retail tests good(), not merely !fail(), after extraction. Opening alone means success.
bool GameServer::loadStdArchiveFingerprints(const std::string& name,
                                           std::map<int, std::string>& fingerprints)
{
    const std::string mod = settings->stringGet("GSModDirectory", "mods/bf2");
    io::IStream* stream = io::fileManager->open(mod + '/' + name,
        static_cast<io::IFileSystem::FileMode>(2), static_cast<io::IFileSystem::FileAccess>(0), 0);
    if (!stream)
        return false;
    std::vector<char> bytes(static_cast<unsigned int>(stream->getAvailableBytes()), 0);
    stream->read(bytes.data(), static_cast<unsigned int>(bytes.size()));
    stream->release();
    bytes.push_back(0);
    std::istringstream in(bytes.data());
    while (in)
    {
        int ordinal;
        std::string hash;
        in >> ordinal >> hash;
        if (in.good())
            fingerprints[ordinal] = hash;
    }
    return true;
}

//  / PE 0x4430A0: always returns true after calculating the misc hash.
bool GameServer::loadMiscFingerprints()
{
    io::ChecksumContext context;
    context.runMiscChecksum();
    m_miscHashMd5 = io::hash_to_string(context.getMiscHash());
    return true;
}

// LoadLevelFingerprint - ELF 0x80A7CE0, 1622 bytes.
// Opens <levelPath>\<mapName>\archive.md5 using the shipped backslash separators and parses
// <name> <id> <md5>, discarding the first token. Missing files return false.
bool GameServer::LoadLevelFingerprint()
{
    std::string path = m_mapInfo.getLevelPath();
    path += "\\";
    path += m_mapInfo.getMapName();
    path += "\\";
    path += "archive.md5";

    // Raw args (path, 2, 0, 0); [CHOSEN] the enum spellings - IFileSystem.h leaves the enumerator
    // names unresolved (2 is the read mode). Same device as demo.cpp's open call.
    io::IStream* f = io::fileManager->open(path, (io::IFileSystem::FileMode)2,
                                           (io::IFileSystem::FileAccess)0, 0);
    if (!f)
        return false;

    unsigned int size = (unsigned int)f->getAvailableBytes();   // IStream +0x1C - the vcall the
                                                                // raw body makes (NOT getSize,
                                                                // which is INonlinearStream +0x38)
    std::vector<char> buf;
    buf.resize(size);
    if (size)
        f->read(&buf[0], size);                             // +0x10
    f->release();                                           // +0x08
    buf.push_back(0);

    std::istringstream in(&buf[0]);
    while (in.good())
    {
        std::string name, md5;
        int id = 0;
        in >> name >> id >> md5;                            // first token read and discarded
        if (!in.fail())
            m_levelFingerprintStrings[id] = md5;
    }
    return true;
}

namespace world {
extern PlayerManager*          playerManager;          // ds:0x8B1B928 (PlayerManager.h)
extern PhysicsNodeManager*     physicsNodeManager;     // ds:0x8B27844
extern ResponsePhysicsManager* responsePhysicsManager; // ds:0x8B27848
extern IBase*                  portalManager;          // ds:0x8B1ED80 - ditto
} // namespace world

namespace {
// [FRONTIER] [TEMP] [CHOSEN, not recovered] Bot portal updates diagnose and abort at this seam.
void portalManagerUpdate(IBase* manager, float dt);                             // world::PortalManager::update(float) - vtable +0x40 (0x88CABE0 slot 16)

void portalManagerUpdate(IBase* /*manager*/, float /*dt*/)
{
    std::fprintf(stderr, "[FRONTIER] dice::hfe::(anon)::portalManagerUpdate"
                         " (world::PortalManager::update, vtable +0x40) reached (bots game)\n");
    std::fflush(stderr);
    std::abort();
}
// Typed bridge from the shared connection interface to packet processing.
inline bool clientConnectionProcessReceivedPacket(IClientConnection* c, io::BitStream* stream)
{
    return reinterpret_cast<ClientConnection*>(c)->processReceivedPacket(stream);
}
inline bool clientConnectionHandlePacketStatusNotification(IClientConnection* c, bool& delivered)
{
    return reinterpret_cast<ClientConnection*>(c)->handlePacketStatusNotification(delivered);
}
void clearPlayerActionsPerConnection(IClientConnection* connection);            // the rest of 0x80B4AD0 (325 B): the ActionBuffer trim
} // namespace

// Advances server simulation and network traffic. Negative dt runs only receive,
// connection update, and send during loading. Simulation uses fixed dt; networking
// uses elapsed time from getDeltaTime. Recording sends after each simulation step.
bool GameServer::update(int steps, float dt)
{
    if (dt < 0.0f)                                              // fldz / fucomip: 0 <= dt is the full path
    {
        processReceivedPackets();
        const float elapsed = getDeltaTime();
        updateConnections(elapsed);
        processGameStateAndSendPackets(elapsed);
        return true;
    }

    if (world::playerManager)
        world::playerManager->getNumberOfPlayers();             // the result is dropped; the vcall is kept
    if (steps > 10)
    {
        steps = 1;                                              // the second clamp: past ten steps behind, one step
        ++m_numberOfDroppedFrames;
    }
    if (steps > 1)
        m_numberOvershotFrames += steps - 1;

    processLocalPlayersInputs(steps);
    processReceivedPackets();
    if (steps > 0)
        processEnqueuedRadioMessages();

    for (int i = 0; i < steps; ++i)
    {
        ++bf2Engine->dword54;                                   // BF2Engine +0x54 - the engine's frame counter
        world::responsePhysicsManager->startFrame();            // +0x2C
        simulateFrame(dt);                                      // the parameter, not getDeltaTime()
        world::responsePhysicsManager->stopFrame();             // +0x30
        if (isRecordingDemo())                                  // an extra send per iteration while recording
        {
            updateStateMasks();
            const float elapsed = getDeltaTime();
            updateConnections(elapsed);
            processGameStateAndSendPackets(elapsed);
        }
        // [PLATFORM] NOT  - BF2.exe 0x4D1138..0x4D1151, a listen-server arm the dedicated ELF
        // folded away: `if (!isDedicated() && i == steps - 1)` the sound owner (ds:0xA23DC0) vslot
        // +0x18(steps). Its identity is unread.
    }
    // [PLATFORM] NOT  - BF2.exe 0x4D1162..0x4D1173: `if (isLightmapRenderingNode())`
    // (0x440530) ds:0x987350 vslot +0x28() - a lightmap-farm tool arm the dedicated ELF folded away.
    // Its identity remains unread.
    if (!isRecordingDemo())
    {
        updateStateMasks();
        const float elapsed = getDeltaTime();
        updateConnections(elapsed);
        processGameStateAndSendPackets(elapsed);
    }

#if !DEDICATED
    // [PLATFORM] Listen servers also tick the local client's vote timers and prompts.
    if (!isDedicated())
        ClientVoteManager::getInstance()->update(dt);
#endif

    VoipServerConnection::GetInstance()->Update(dt);
    clearPlayerActions();
    destroyPlayers();

    if (m_matchMakingServer)
    {
        m_matchMakingServer->update();
        if (m_matchMakingServer->getAuthenticator())            // asked twice, as shipped
        {
            GameSpyCDKeyAuthenticator* authenticator = m_matchMakingServer->getAuthenticator();
            authenticator->update();                             // its vtable +0x18 - typed now that the class is real
        }
    }
    if (m_statsManager)
        m_statsManager->think();                                // IStatsManager +0x2C

    m_mapListSendCounter += steps;
    if (m_mapListSendCounter >= m_mapListSendInterval)
    {
        m_mapListSendCounter = 0;
        if (mapList->isDirty())
        {
            const int size = mapList->getDataBlockSize();
            const unsigned char* block = mapList->getDataBlock();
            sendDataBlockToAll(5, block, static_cast<unsigned int>(size), false);
            mapList->setDirty(false);
        }
    }
    m_serverInfoSendCounter += steps;
    if (m_serverInfoSendCounter >= m_serverInfoSendInterval)
    {
        m_serverInfoSendCounter = 0;
        if (serverInfo->isDirty())
        {
            const void* data = "";                              // the ELF seeds the out-pointer with its empty literal
            unsigned int size = 0;
            serverInfo->getDataBlock(&data, &size, false);
            sendDataBlockToAll(0, data, size, false);
            serverInfo->setDirty(false);
        }
    }

    ServerVoteManager::getInstance()->update(dt);
    BanList::getInstance()->update(dt);

    m_httpUpdateCounter += steps;
    if (m_httpUpdateCounter >= m_httpUpdateInterval)
    {
        m_httpUpdateCounter = 0;
        if (!m_httpRequestsToRemove.empty())
        {
            for (std::list<HTTPRequest*>::iterator it = m_httpRequestsToRemove.begin();
                 it != m_httpRequestsToRemove.end(); ++it)
            {
                std::set<HTTPRequest*>::iterator found = m_httpRequests.find(*it);
                if (found != m_httpRequests.end())
                {
                    if (*it)
                        delete *it;                             // through its virtual dtor (vtable +0x04)
                    m_httpRequests.erase(found);
                }
            }
            m_httpRequestsToRemove.clear();
        }
        for (std::set<HTTPRequest*>::iterator it = m_httpRequests.begin(); it != m_httpRequests.end(); ++it)
        {
            (*it)->update();
            if (!(*it)->isUpdating())
                removePersistenceConnection(*it);               // virtual - queues it for the drain above
        }
    }

    if (m_sampleStreamManagerReadStats)
        m_networkableStats->update();                            // typed ABI: ELF +0x08 / PE +0x04
    if (m_autoRecorder)
        m_autoRecorder->update();
    return true;
}

// Simulation updates game logic and events before player/world physics. Collision
// cache resets bracket world collision; post-frame physics precedes portals and
// frame-arena advancement. Paused games skip world simulation.
namespace {
int* const g_collDumpAllTests = Vars::getInt("coll-dump-all-tests", 0);   // ds:0x8AAB758 - static-init _91 of this TU
int* const g_connPacketSizeOverride = Vars::getInt("conn-packetsize-override", 0);   // ds:0x8AAB75C - the
                                                        //   very next initialiser of the same static-init run
                                                        //   (0x80B6B35..0x80B6B47, right after the one above),
                                                        //   which is what places it in this TU. chokeBandwidth
                                                        //   is its only reader.
namespace memory { inline void nextPhysicFrame() {} }   // empty here, like nextVisualFrame:
                                                        //   the memory DLL's arena boundary (see BF2Engine.cpp)
int g_collisionDumpFlag = 0;                            // ds:0x8ABA820 [UNRESOLVED name] - the physics side's
                                                        //   collision-dump flag; defined here until its TU is recovered
} // namespace

bool GameServer::simulateFrame(float dt)
{
    gameLogic->update(dt);                                      // +0x20
    if (gameLogic->getGameStatus() == 1)                        // +0x158: playing
    {
        if (ai::g_aiInstance && ai::aiInstance()->isBotsGame())
            ai::aiInstance()->update(static_cast<double>(dt));
    }
    eventManager->processEvents(dt);                            // +0x20
    simulatePlayersUpdate(dt);
    if (gameLogic->getGameStatus() != 4)
    {
        updateTriggerManager(dt);
        updateTriggerableManager(dt);
        world::objectManager->updateObjects(dt, static_cast<world::UpdateFreqencyType>(0), 3);
        simulatePlayersPhysics(dt);
        world::physicsNodeManager->update(dt, 0, 0);
        world::responsePhysicsManager->resetCachedCollisionObjects();
        updateWorldCollision(dt);
        world::responsePhysicsManager->resetCachedCollisionObjects();
        simulatePlayersCollisions(dt);
        world::physicsNodeManager->postFrameUpdate();
        world::responsePhysicsManager->postFrameUpdate();
        updatePortals(dt);
    }
    ++m_frameCount;                                             // +0x60
    memory::nextPhysicFrame();
    if (*g_collDumpAllTests == 1)
    {
        Vars::set(std::string("coll-dump-all-tests"), std::string(""));
        g_collisionDumpFlag = 0;
    }
    return true;
}

// Queues exactly steps local actions, using zero input after the sample queue empties.
// Auto-input affects only the first queued sample. Later dry steps clear input values
// but retain mapping flags from the last real sample. No local player means no work.
void GameServer::processLocalPlayersInputs(int steps)
{
    if (!world::playerManager->getLocalHumanPlayer())            // +0x30
        return;
    world::IPlayer* localPlayer = world::playerManager->getLocalHumanPlayer();   // again

    io::PlayerInput input;
    input.mappingFlags = 0;
    input.dword0 = 0;
    input.dword1 = 0;
    input.flag     = 0;
    for (int i = 0; i <= 0x3F; ++i)
        input.inputs[i] = 0.0f;

    std::list<io::PlayerInput>::const_iterator node = m_playerInputs.begin();

    for (int pushed = 0; pushed < steps; ++pushed)
    {
        if (node == m_playerInputs.end())
        {
            // only the analogue values - see the header block
            for (int j = 0; j <= 0x3F; ++j)
                input.inputs[j] = 0.0f;
        }
        else
        {
            input = *node;
            if (node == m_playerInputs.begin())
                localPlayer->getAutoInput(input, false);         // the head only
            ++node;
        }

        PlayerAction action;
        action.set(input);
        action.m_inputIndex = WorldPref::getTickCount();
        localPlayer->getActionBuffer()->pushBack(action);
    }
}

// Publish a new connection before queuing its challenge. Connection ids are stored
// as byte keys. Demo connection 129 uses demorecord and authenticates immediately;
// ordinary connections receive a generated challenge with the mod identity.
// Then set the connection type and recompute bandwidth limits.
bool GameServer::onNewConnection(int connectionId)
{
    // 129 is the demo pseudo-connection - see above. `connectionId == 129` IS the demo mode.
    ServerConnection* const conn = new ServerConnection(
        static_cast<ConnectionDemoMode>(connectionId == 129 ? 1 : 0));       // operator new(0x188)
    if (!conn)
        return false;                                    // dead - operator new throws

    conn->setConnectionId(connectionId);
    m_connections.insert(std::make_pair(static_cast<unsigned char>(connectionId),
                                        static_cast<IClientConnection*>(conn)));   // byte key

    const std::string modName = getModName();            // both arms

    if (connectionId == 129)
    {
        conn->initChallengeString("DEMORECORD");         // exactly 10 bytes
        conn->m_lastPingMs = 0;
        authOk(129);                                     // it authenticates itself
    }
    else
    {
        const char* const challenge = conn->initChallengeString(0);
        conn->m_gameEventManager->addEvent(new ChallengeEvent(challenge, modName));   // event id 1
    }

    setConnectionType(connectionId, static_cast<ConnectionType>(0));         // virtual, +0x150
    chokeBandwidth();                                                        // virtual, +0x154
    return true;
}

// Sends ServerInfo and MapList blocks after authentication. Base-game servers
// always send them; expansion servers send them only after the first connection.
// Every authentication increments the connection count.
void GameServer::authOk(int connectionId)
{
    ClientConnection* conn = static_cast<ClientConnection*>(getClient(connectionId));
    if (!conn)
        return;

    ++conn->m_nbConnections;                                          // +0x160

    if (isExpansionPack() && conn->m_nbConnections <= 1)              // the quirk - see above
        return;

    const void*  serverInfoData = 0;
    unsigned int serverInfoSize = 0;
    serverInfo->getDataBlock(&serverInfoData, &serverInfoSize, true);          // ds:0x8AAAA60
    sendDataBlock(connectionId, 0, serverInfoData, serverInfoSize, false);     // block type 0

    const unsigned int   mapSize = mapList->getDataBlockSize();                // ds:0x8AAAA00
    const unsigned char* mapData = mapList->getDataBlock();
    sendDataBlock(connectionId, 5, mapData, mapSize, false);                   // block type 5
}

// authFailed - ELF 0x80B2C80, 515 B. Queues a disconnect for a client that failed CD-key auth
// (19 invalid / 20 disabled / 37 in-use); processReceivedPackets tears it down after the receive
// loop. Only cdkey_auth_callback calls it.
void GameServer::authFailed(int connectionId, unsigned int reason)
{
    if (getClient(connectionId))
        m_disconnectTypes[connectionId] = DisconnectType(connectionId, reason, std::string());
}

// kickPlayer - ELF 0x80B2E90 / PE 0x44A670. The kick notification is unconditional and precedes
// the connection lookup. A live connection is not torn down here: retail records the requested
// result/text in m_disconnectTypes for processReceivedPackets to consume after its receive loop.
// BanList::_instance is deliberately dereferenced without a guard in both ports.
void GameServer::kickPlayer(int connectionId, io::PResult reason, const std::string& text)
{
    BanList::_instance->alertOnKicked(connectionId);
    if (getClient(connectionId))
    {
        m_disconnectTypes[connectionId] =
            DisconnectType(connectionId, static_cast<unsigned int>(reason), text);
    }
}

// consoleKickPlayer - ELF 0x80B3020 / PE 0x4436E0. The argument is PlayerManager's id-string
// syntax despite the generated console metadata simply calling it std::string. A found player is
// queued for reason 21 and produces an empty console result; failure returns the exact retail text.
std::string GameServer::consoleKickPlayer(const std::string& idText)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromIdString(idText);
    if (!player)
        return std::string("Player not found.");

    kickPlayer(player->getId(), static_cast<io::PResult>(21), std::string());
    return std::string();
}

// listPlayers - ELF 0x80B3120 / PE 0x445920. Exact admin.listPlayers text formatter. The IP scratch
// string is constructed once and reused; its value is appended only when getConnectionIp succeeds.
// GameSpy is queried for every remote player even when there is no NetServer/IP result.
std::string GameServer::listPlayers() const
{
    std::string result;
    std::string ip;
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        result += formatString("Id:%3d - ", player->getId());
        result += player->getName();

        if (player->getIsRemote())
        {
            result += " is remote ";
            if (m_netServer && m_netServer->getConnectionIp(player->getId(), ip))
                result += formatString("ip: %s", ip.c_str());

            const std::string keyHash(gcd_getkeyhash(gamespy::g_gameID, player->getId()));
            if (!keyHash.empty())
                result += formatString(" ->\n         CD-key hash: %s ", keyHash.c_str());
        }
        else
        {
            result += " is local.";
        }

        if (player->getIsAIPlayer())
            result += " is an AI bot.";
        result += "\n";
    }
    return result;
}

// Rejects incompatible network versions, then starts CD-key authentication for
// a live connection and checks the key ban list. LAN authentication calls back
// synchronously. Requires the matchmaking service initialized by init.
void GameServer::challengeResponse(int connectionId, GameEvent* ev)
{
    ChallengeResponseEvent* const event = static_cast<ChallengeResponseEvent*>(ev);
    IClientConnection* const conn = getClient(connectionId);
    const unsigned int netVersion = BuildNrUtil::getNetVersionNumber();

    if (event->m_netVersion != netVersion)
    {
        const unsigned int reason = 24u - (event->m_netVersion < netVersion ? 1u : 0u);   // 23 old / 24 new
        m_disconnectTypes[connectionId] = DisconnectType(connectionId, reason, std::string());
        return;
    }

    if (conn)
    {
        const unsigned int challengeIp = m_netServer->getChallengeIp(connectionId);
        if (m_matchMakingServer->getAuthenticator() && challengeIp)
        {
            ServerConnection* const sc = asServerConnection(conn);
            if (event->m_gameId != gamespy::g_gameID)
                sc->m_gameId = event->m_gameId;                                  // +0x184
            m_matchMakingServer->getAuthenticator()->connectPlayer(
                event->m_gameId, connectionId, challengeIp,
                static_cast<ClientConnection*>(conn)->m_challengeString,          // +0x13C, the server challenge
                reinterpret_cast<const char*>(event->m_response));               // +0x0C, the client 73-byte answer
        }
        const std::string keyHash = gcd_getkeyhash(gamespy::g_gameID, connectionId);
        if (BanList::getInstance()->isKeyBanned(keyHash))
            m_disconnectTypes[connectionId] = DisconnectType(connectionId, 22u, std::string());
    }
}

// Drain received packets, then execute queued disconnects. Do not tear down
// connections inside the receive loop: NetServer retains a map cursor. Connection
// 129 is the demo stream. Packet payload lengths arrive without a packet-size clamp.
// [FRONTIER] PunkBuster packet delivery remains omitted; its filtered payloads
// are still drained and freed.
void GameServer::processReceivedPackets()
{
    if (!m_netServer)
        return;

    // 1. the netgraph switch, re-read every frame rather than cached.
    m_sampleStreamManagerReadStats =
        settings->boolGet(std::string("GSShowNetGraph"), false);

    // 2. the demo recorder's own pseudo-connection.
    if (m_demoRecorder)
    {
        IClientConnection* const demo = getClient(129);
        if (demo)
            clientConnectionProcessReceivedPacket(demo, 0);
    }

    // 3. drain whatever PacketFilter buffered for PunkBuster. THE free IS NOT optional - see above.
    {
        std::list<io::StoredPacket> filtered;
        m_netServer->m_packetFilter.getPackets(io::FILTEREDPACKETTYPE_0, &filtered);
        for (std::list<io::StoredPacket>::iterator it = filtered.begin();
             it != filtered.end(); ++it)
        {
            // disabled BY project decision - the guard and the call, both reproduced as comments:
            //     if (m_isPunkbusterEnabled)
            //         PB_handlePacket(it->m_data, it->m_size, &it->m_address);
            std::free(it->data);
        }
    }

    // Receive at least once and continue until result 8; other results dispatch or queue disconnects.
    int result;
    do
    {
        int connectionId = 0;
        result = m_netServer->receive(&connectionId, m_bitStream);

        switch (result)
        {
        case 0:                                                           // a data packet
        {
            IClientConnection* const conn = getClient(connectionId);
            if (conn)
            {
                if (!clientConnectionProcessReceivedPacket(conn, m_bitStream))
                    m_disconnectTypes[connectionId] =
                        DisconnectType(connectionId, 29, std::string());   // 29 = bad payload
                if (m_sampleStreamManagerReadStats)
                    m_networkableStats->sampleStreamManagerReadStats(conn, true);
            }
            break;
        }

        case 1:                                                           // a NEW connection
            if (!onNewConnection(connectionId))
                m_disconnectTypes[connectionId] = DisconnectType(connectionId, 13, std::string());
            break;

        case 10:                                                          // timed OUT
            m_disconnectTypes[connectionId] =
                DisconnectType(connectionId, 10, std::string());
            break;

        case 13:                                                          // refused / dropped
            m_disconnectTypes[connectionId] =
                DisconnectType(connectionId, 13, std::string());
            break;

        default:                                                          // 8, and anything else
            break;
        }
    }
    while (result != 8);

    // 5. NOW tear down whatever the loop queued. The player goes first, the connection second.
    if (!m_disconnectTypes.empty())
    {
        for (std::map<int, DisconnectType>::iterator it = m_disconnectTypes.begin();
             it != m_disconnectTypes.end(); ++it)
        {
            const int          id     = it->second.connectionId;
            const unsigned int reason = it->second.reason;
            const std::string  text   = it->second.text;               // copied, not referenced

            // Destroy registered players or release the reserved ID when connection setup never created a player.
            world::IPlayer* const player = getPlayer(id);
            if (player)
                destroyPlayer(player);                                 // virtual, GameServer +0x134
            else
                world::playerManager->removePlayerId(id);              // +0x18

            closeClientConnection(id, reason, text);
        }
        m_disconnectTypes.clear();
    }

    // 6. the netgraph's second sample, taken once per frame rather than per connection.
    if (m_sampleStreamManagerReadStats)
        m_networkableStats->sampleNetworkableReadStats();
}

// Looks up a player by its connection ID; player creation assigns both the same ID.
world::IPlayer* GameServer::getPlayer(int id) const
{
    return world::playerManager->getPlayerFromId(id);                  // +0x20
}

// : while playing, post up to m_radioMessagesPerTick queued radio EventNodes on
// ECNetwork (postEvent(6, node, 0.0f)) and pop them; when NOT playing (or no game logic), drop the
// whole queue through removeAllEnquedRadioMessages.
void GameServer::processEnqueuedRadioMessages()
{
    if (gameLogic && gameLogic->getGameStatus() != 1)
    {
        removeAllEnquedRadioMessages();
        return;
    }
    int budget = m_radioMessagesPerTick;
    while (!m_radioMessages.empty() && budget)
    {
        --budget;
        EventNode* node = m_radioMessages.front();
        eventManager->postEvent(ECNetwork, node, 0.0f);         // +0x10
        m_radioMessages.erase(m_radioMessages.begin());
    }
}

// Queues radio event nodes of type 13; ignores null nodes and other event types.
void GameServer::enqueueRadioMessage(EventNode* node)
{
    if (!node)
        return;
    if (node->getType() != 13)                                    // EventNode +0x0C
        return;
    m_radioMessages.insert(m_radioMessages.end(), node);           // +0x154
}

//  [sic: Enqued]: release() every queued node (EventNode +0x08), clear the list.
void GameServer::removeAllEnquedRadioMessages()
{
    for (std::list<EventNode*>::iterator it = m_radioMessages.begin(); it != m_radioMessages.end(); ++it)
        (*it)->release();
    m_radioMessages.clear();
}

// : nothing without a connection; else the NetworkManager computes the updated
// descriptors once and every connection's GhostManager applies them.
void GameServer::updateStateMasks()
{
    if (m_connections.empty())
        return;
    // Build fresh networkable state masks and merge them into each connection's ghost state.
    const std::vector<std::pair<const io::NetworkableDescriptor*, io::NetworkableStateMask> >* updated =
        io::networkManager->updateStateMasks();
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
        asServerConnection(it->second)->m_ghostManager->updateStateMasks(*updated);
}

// Service connection timers, pending database delivery, ping and packet statuses.
// Failures queue disconnects for the next receive pass; do not erase connections
// while iterating. The bandwidth-choke countdown is shared across server instances.
namespace { float g_chokeCountdown = 10.0f; }                   // ds:0x8AAB760 [UNRESOLVED seed] - counts down by dt
void GameServer::updateConnections(float dt)
{
    if (!m_netServer)
        return;

    g_chokeCountdown -= dt;
    if (g_chokeCountdown <= 0.0f)
    {
        g_chokeCountdown = 10.0f;
        chokeBandwidth();                                       // virtual +0x154
    }

    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        ClientConnection* const conn = reinterpret_cast<ClientConnection*>(it->second);
        const int connId = it->first;                           // the byte, widened

        // the server's load state, not the connection's - see the note above.
        if (m_loadState == 2 && conn->m_connectionState == 2)
            clientSendDatabase(connId);

        conn->m_timer1C -= dt;                                  //   fsubr/fstp [eax+1Ch]
        conn->m_timer20 -= dt;                                  //   fsubr/fstp [eax+20h]

        int ping = 0;
        if (m_netServer->getPing(connId, &ping))                //   vslot +0x80
            conn->m_lastPingMs = ping;                          //   [edx+148h]

        // a while: one frame can retire many in-flight packets.
        bool delivered = false;
        while (m_netServer->getPacketDeliveryStatus(connId, delivered))   //   vslot +0x58
        {
            if (!clientConnectionHandlePacketStatusNotification(it->second, delivered))
                m_disconnectTypes[connId] = DisconnectType(connId, 28, std::string());
        }
    }
}

// Apply a connection-type row and rerun bandwidth limiting. Type 6 is refused;
// values above the configured maximum retry through the virtual method. Demo
// connection 129 retains its constructor's packet size and rate.
void GameServer::setConnectionType(int connectionId, ConnectionType type)
{
    IClientConnection* const conn = getClient(connectionId);

    if (static_cast<int>(type) == 6 || !conn)
        return;                                      // the table's bound - see above

    if (static_cast<int>(type) > m_maxConnectionType)
    {
        setConnectionType(connectionId,
                          static_cast<ConnectionType>(m_maxConnectionType));  // virtual re-entry
        return;
    }

    ClientConnection* const cc = reinterpret_cast<ClientConnection*>(conn);
    cc->setConnectionType(type);

    if (connectionId != 129)                         // the demo recorder keeps its own numbers
    {
        const int* const row = g_connectionTypes[static_cast<int>(type)];
        cc->m_packetSizeOverride = row[3];           // +0x18
        cc->m_dword14            = row[1];           // +0x14
    }

    chokeBandwidth();
}

void GameServer::chokeBandwidth()
{
    typedef std::map<unsigned char, IClientConnection*>::iterator ConnIt;

    // 1. the cvar override
    if (*g_connPacketSizeOverride)
    {
        for (ConnIt it = m_connections.begin(); it != m_connections.end(); ++it)
            reinterpret_cast<ClientConnection*>(it->second)->m_packetSizeOverride =
                *g_connPacketSizeOverride;
    }

    // 2. the budget, recomputed from the settings on every firing
    m_bandwidthChoke = serverSettings->m_bandwidthChoke << 7;        // +0x88 = ServerSettings +0x10 * 128
    if (!m_bandwidthChoke)
        return;

    // 3. what the connections are asking for
    int demand = 0;
    for (ConnIt it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        const ClientConnection* const conn = reinterpret_cast<const ClientConnection*>(it->second);
        demand += conn->m_packetSizeOverride * conn->m_dword14;      // bytes * Hz = bytes/s
    }

    if (demand <= m_bandwidthChoke)
        return;

    // 4. the cut, equal shares with a 320-byte floor
    const int share = m_bandwidthChoke / static_cast<int>(m_connections.size());
    for (ConnIt it = m_connections.begin(); it != m_connections.end(); ++it)
    {
        ClientConnection* const conn = reinterpret_cast<ClientConnection*>(it->second);
        const int rate = conn->m_dword14;
        if (rate * conn->m_packetSizeOverride > share)
        {
            const int size = share / rate;
            conn->m_packetSizeOverride = size > 319 ? size : 320;    // cmovg against 0x13F / 0x140
        }
    }
}

// Send due connections in scope-time order, with at most twenty normal ghost
// scope updates per frame and a view-distance floor of 500 metres. Delayed scope
// updates become more overdue for the next frame. Demo recording bypasses those
// limits and acknowledges its recorded packets immediately without socket delivery.
// A full send buffer defers transmission; other send errors queue disconnection.
void GameServer::processGameStateAndSendPackets(float dt)
{
    (void)dt;                                            // the ELF never reads it - see below

    if (!m_netServer)
        return;

    // 1. GSViewDistance, floored at 500 - see above. Read as an int, compared as a float.
    float viewDistance = static_cast<float>(settings->intGet(std::string("GSViewDistance"), 500));
    if (!(500.0f <= viewDistance))                       // `cmovbe`, 0x80A8EED
        viewDistance = 500.0f;

    // 2. flatten the byte-keyed map into a vector so it can be ordered.
    std::vector<ServerConnection*> ordered;
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        ordered.push_back(asServerConnection(it->second));
    }

    // the fairness sort - most overdue first. See ServerConnection.h for the comparator.
    std::sort(ordered.begin(), ordered.end(), ScopeTimeSort());

    int ghostBudget = 20;                                // twenty scope passes per frame

    for (std::vector<ServerConnection*>::iterator it = ordered.begin(); it != ordered.end(); ++it)
    {
        ServerConnection* const conn = *it;

        // the packet rate gate. A demo bypasses it; everyone else waits their turn.
        const bool recording = conn->getIsRecordingConnection();
        if (!recording && !(conn->m_timer1C <= 0.0f))
            continue;

        if (conn->m_timer1C <= 0.0f)
        {
            // one period back on the clock. m_dword14 is the rate in Hz - default 20.
            conn->m_timer1C += 1.0f / static_cast<float>(conn->m_dword14);
        }

        if (conn->m_connectionState == 4 && conn->m_bReset)
        {
            conn->updatePlayerControlObject(m_frameCount);           // bool result discarded, as shipped

            if (recording || (ghostBudget > 0 && conn->m_timer20 <= 0.0f))
            {
                --ghostBudget;
                conn->updateGhostManager(viewDistance);              // the scope pass, 10 Hz max
                conn->m_timer20 = 0.1f;                              // 10 Hz
            }
            else
            {
                // missed its turn - go further overdue so the sort favours it next frame.
                conn->m_timer20 -= 0.1f;
            }

            conn->m_ghostManager->update(WorldPref::getTickCount()); // the send-heap build
        }

        if (!conn->transmitMsgs())                                   // returns bool - see the header
        {
            m_disconnectTypes[conn->m_connectionId] =
                DisconnectType(conn->m_connectionId, 28, std::string());
            // the ELF does NOT `continue` here - it falls through to the send block below.
        }

        // the byte count, rounded up from the stream's high-water mark.
        unsigned int bytes = conn->m_bitStream->m_highWaterBits >> 3;
        if (conn->m_bitStream->m_highWaterBits & 7)
            ++bytes;

        if (recording)
        {
            // a demo never touches the socket - see above.
            if (bytes)
            {
                const int tick = (conn->m_connectionState == 4) ? WorldPref::getTickCount() : -1;
                if (m_demoRecorder)
                    m_demoRecorder->writeData(tick, conn->m_bitStream);
                bool delivered = true;                   // a demo cannot lose a packet
                conn->handlePacketStatusNotification(delivered);
            }
            continue;
        }

        if (!bytes)
            continue;

        // the three-way back-pressure result - see above.
        const int queued = m_netServer->bufferPacket(conn->m_connectionId, conn->m_bitStream, bytes);
        if (!queued)
        {
            conn->m_sentData = false;
        }
        else if (queued == 15)
        {
            conn->m_sentData = true;                     // the buffer is full; skip next frame
        }
        else
        {
            m_disconnectTypes[conn->m_connectionId] =
                DisconnectType(conn->m_connectionId, 28, std::string());
        }

        if (m_sampleStreamManagerReadStats)
            m_networkableStats->sampleStreamManagerWriteStats(conn, true);

        m_netServer->send(conn->m_connectionId);
    }

    if (m_sampleStreamManagerReadStats)
        m_networkableStats->sampleNetworkableWriteStats();
}

bool GameServer::isRecordingDemo() const
{
    if (m_demoRecorder)
        return m_demoRecorder->isOpenForRecord();               // [FRONTIER] - DemoRecorder is declaration-only
    return false;
}

// : for every connection with a player, trim its ActionBuffer down to 4 entries
// (popping the front while size() > 4) and resetTickCounter(); then clear the queued local inputs.
// A connection without a Player advances to the next map node at 0x80B4BE0; it does not bypass the
// remaining connections or the final m_playerInputs clear.
void GameServer::clearPlayerActions()
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it) {
        world::IPlayer* const player = it->second->getPlayer();      // ServerConnection::getPlayer, vtable +0x18
        if (!player)
            continue;

        if (player->getActionBuffer()->size() > 4)                   // something older than the last 4?
        {
            while (player->getActionBuffer()->size() > 4)            // drop the oldest until 4 remain
                player->getActionBuffer()->m_entries.pop_front();
            player->resetTickCounter();                              // IPlayer +0xDC
        }
    }
    m_playerInputs.clear();
}

// : the pending-destroy list - a still-alive player stays, the rest are destroyed
// (destroyPlayer, virtual +0x120) and dropped.
void GameServer::destroyPlayers()
{
    for (std::list<world::IPlayer*>::iterator it = m_playersToDestroy.begin(); it != m_playersToDestroy.end(); )
    {
        if ((*it)->getIsAlive())                                // IPlayer +0x6C
        {
            ++it;
            continue;
        }
        destroyPlayer(*it);
        it = m_playersToDestroy.erase(it);
    }
}

// Input manager access is restricted to the local-human vehicle path; dedicated
// images have no input-manager instance.
namespace io { extern IBase* inputManager; }

namespace {

// Allocates an enter-vehicle event for transfer to the broadcast queue.
GameEvent* createEnterVehicleEvent(int playerId, unsigned short netId, bool setAsDefaultVehicle)
{
    return new EnterVehicleEvent(playerId, netId, setAsDefaultVehicle);
}

// Updates a Soldier's look angle through its typed implementation, ignoring other
// object types. Soldier itself ignores the request while ragdolling.
void soldierSetLookUpDownAngle(world::IObject* soldier, float angle)
{
    if (!soldier)
        return;
    // Guard on the template's class id and then cast directly - the idiom PlayerControlObject.cpp
    // already uses for its four Soldier reaches, and the one retail uses: Soldier's IObject is its
    // primary base, so the cast is an identity rather than a sub-object adjustment.
    if (soldier->getTemplate()->getClassID() != world::CID_SoldierTemplate)
        return;
    static_cast<world::Soldier*>(soldier)->setLookUpDownAngle(angle);
}

// /80AFC69 and PE 44551A/444875: IInputManager +2C, std::string BY value.
// Keep the copy/temporary lifetime at the typed virtual boundary; its bool result is discarded.
void inputManagerSetControlMap(IBase* inputManager, const std::string& mapName)
{
    static_cast<io::IInputManager*>(inputManager)->setPlayerControlMap(mapName);
}

//   IPlayerControlObjectTemplate's +0x2C/+0x24 pair is now typed as getControlsCategory and
//   getVehicleCategory. enterVehicle uses the first to pick a control map and treats 4 as the
//   sentinel that defers to the second.

world::IPlayer* weaponObjectGetPlayer(void* weapon)
{
    return static_cast<world::IWeaponObject*>(weapon)->getPlayer();
}

int weaponObjectGetTargetSystemType(const void* weapon)
{
    return static_cast<const world::IWeaponObject*>(weapon)->getTargetSystemType();
}

// Dispatches player input through the concrete free-camera interface.
void freeCameraHandlePlayerInput(world::IObject* camera, world::IPlayer* player,
                                 const io::PlayerInput& input, float dt, int flags)
{
    static_cast<world::FreeCamera*>(camera)->handlePlayerInput(
        player, input, dt, static_cast<unsigned int>(flags));
}

} // anonymous namespace

//  / PE 0x44BB30.  MissileLockMessage is a 16-byte by-value payload.  Human and
// remote occupants receive HUD event 44; bot occupants take the AI courier path unless field04 is
// the -1 sentinel.  The PCO map is deliberately snapshotted before iteration.
void GameServer::sendMissileMessageToOccupyingPlayers(
    world::IObject* object, world::MissileLockMessage message)
{
    typedef world::object_detail::LayoutAccess L;

    world::IObject* node = world::getRootParent(object);
    if (!node)
        return;

    world::IPlayerControlObject* rootPco = 0;
    for (; node && !rootPco; node = L::parent(node))
    {
        rootPco = static_cast<world::IPlayerControlObject*>(
            node->queryInterface(world::IID_IPlayerControlObject));
    }
    if (!rootPco)
        return;

    const std::map<int, world::IPlayerControlObject*> pcos(*rootPco->getPcos());
    for (std::map<int, world::IPlayerControlObject*>::const_iterator it = pcos.begin();
         it != pcos.end(); ++it)
    {
        world::IPlayer* player = it->second->getOccupingPlayer();
        if (!player)
            continue;

        if (!player->getIsAIPlayer() || message.field04 == -1)
        {
            if (player->getIsRemote())
            {
                PostRemoteEvent* event =
                    new PostRemoteEvent(ECHud, 44, false, &message, 16, 0.0f);
                game->sendGameEventIfConnected(static_cast<unsigned int>(player->getId()), event);
            }
            else
            {
                eventManager->postEvent(ECHud, 44, &message, 0.0f);
            }
            continue;
        }

        world::IObject* weaponObject =
            world::objectManager->getObjectFromGrid(message.field08);
        if (!weaponObject)
            continue;

        void* weapon = weaponObject->queryInterface(world::IID_IWeaponObject);
        if (!weapon)
            continue;

        world::IPlayer* weaponPlayer = weaponObjectGetPlayer(weapon);
        if (!weaponPlayer || weaponPlayer->getTeam() == player->getTeam())
            continue;

        world::IObject* vehicle = player->getVehicle();
        world::IPlayerControlObjectTemplate* pcoTemplate =
            static_cast<world::IPlayerControlObjectTemplate*>(
                vehicle->getTemplate()->queryInterface(
                    world::IID_IPlayerControlObjectTemplate));
        if (!pcoTemplate)
            continue;

        const int vehicleCategory = pcoTemplate->getVehicleCategory();
        const int targetSystemType = weaponObjectGetTargetSystemType(weapon);
        if ((targetSystemType == 3 &&
             static_cast<unsigned int>(vehicleCategory - 2) <= 1u) ||
            (targetSystemType == 1 && vehicleCategory == 0))
        {
            continue;
        }

        ai::BFSingletonServer::m_iaiEventCourier->eventMissileLock(
            static_cast<ai::IAIObject*>(L::aiObject(vehicle)), &message);
    }
}

// Consumes one player action or replays the last input for up to 30 empty-queue ticks
// before using neutral input. Queued actions use their own stamp; replay uses the
// buffer index plus one. Repeated getVehicle calls must remain separate because
// resolving an expired vehicle can clear its weak reference.
bool GameServer::simulatePlayerUpdate(world::IPlayer* player, float dt)
{
    io::PlayerInput input;
    input.mappingFlags = 0;
    input.dword0 = 0;
    input.dword1 = 0;
    input.flag     = 0;
    for (int i = 0; i <= 63; ++i)
        input.inputs[i] = 0.0f;

    player->setHasSimulatedFrame(false);
    player->updateGhostFiringState();

    io::PlayerInput* chosen;
    if (player->getIsAIPlayer())
    {
        // The AI slot exposes its PlayerInput through an opaque interface result.
        chosen = static_cast<io::PlayerInput*>(player->getAIPlayerInput());
    }
    else
    {
        ActionBuffer* buffer = player->getActionBuffer();

        if (buffer->m_entries.empty())
        {
            // nothing queued - replay m_current, see the header block
            const int replays = buffer->m_replayCount;
            buffer->m_current.get(input);

            const int current = buffer->m_currentInputIndex;
            m_currentInputIndex = (current == -1) ? 0 : current + 1;

            if (replays <= 29)
                buffer->m_replayCount = replays + 1;
            else
                for (int i = 0; i <= 63; ++i)           // give up: neutral input
                    input.inputs[i] = 0.0f;
        }
        else
        {
            // consume the oldest queued action and make it current
            PlayerAction& front = buffer->m_entries.front().m_action;
            front.get(input);

            m_currentInputIndex         = front.m_inputIndex;
            buffer->m_currentInputIndex = front.m_inputIndex;
            buffer->m_current           = front;

            buffer->m_entries.erase(buffer->m_entries.begin());
            buffer->m_replayCount = 0;                  // a real action resets the run
        }
        chosen = &input;
    }

    const int status = gameLogic->getGameStatus();      // asked up to three times in the ELF
    if (status == 1 || (status == 3 && player->getIsAIPlayer()))
    {
        world::IObject* vehicle = player->getVehicle();     // first of two - mutates

        // PlayerInput is passed by value; trigger checks receive a copy.
        gameLogic->checkPlayerTriggers(player, vehicle, *chosen, false);

        player->handleInput(*chosen, dt, 0);

        if (player->getVehicle())                           // second call - see the header block
            performHandleUpdate(dt, player->getVehicle(), 0, false);
    }
    else if (status == 4 && player->getFreeCameraEnabled())
    {
        world::IObject* freeCamera = player->getFreeCamera();
        freeCameraHandlePlayerInput(freeCamera, player, *chosen, dt, 0);
    }

    player->setHasSimulatedFrame(true);

    if (m_netServer)
        m_svStartTime = m_netServer->getTime();             // +0x64; per player, not per frame

    return true;
}

// Updates players in root vehicles or on foot before passengers, skipping non-playing connections.
// Failed simulation adds a tick; the result is exposed only while m_playersUpdateCheck is enabled.
bool GameServer::simulatePlayersUpdate(float dt)
{
    typedef world::object_detail::LayoutAccess L;
    bool result = false;
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();   // +0x3C
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;
        IClientConnection* connection = getClient(player->getId());
        if (connection && clientConnectionState(connection) != 4)
            continue;
        world::IObject* vehicle = player->getVehicle();
        if (vehicle && L::parent(vehicle))
            continue;                                           // a passenger - the second pass
        if (simulatePlayerUpdate(player, dt))
            result = true;
        else
            player->addTick();
    }
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;
        IClientConnection* connection = getClient(player->getId());
        if (connection && clientConnectionState(connection) != 4)
            continue;
        world::IObject* vehicle = player->getVehicle();
        if (!vehicle || !L::parent(vehicle))
            continue;
        if (simulatePlayerUpdate(player, dt))
            result = true;
        else
            player->addTick();
    }
    return m_numPlayers ? result : false;                    // +0x80 - see GameServer.h
}

// The player's simulated-frame latch sequences update, physics and collisions.
// Only a completed update permits physics; collisions consume the latch before checking
// the vehicle. Running collisions first would suppress physics. Both passes update the vehicle.
namespace {


} // anonymous namespace

bool GameServer::simulatePlayerPhysics(world::IPlayer* player, float dt)
{
    if (player && player->getHasSimulatedFrame())
    {
        if (world::IObject* vehicle = player->getVehicle())
            performMobilePhysicsUpdate(dt, vehicle, false);
    }
    return true;                                    // the single `mov eax, 1` epilogue
}

void GameServer::simulatePlayerCollisions(world::IPlayer* player, float dt)
{
    if (!player || !player->getHasSimulatedFrame())
        return;

    player->setHasSimulatedFrame(false);            // before the vehicle test - see the block

    if (world::IObject* vehicle = player->getVehicle())
        performMobileCollisionUpdate(dt, vehicle, false);   // a tail call in the ELF
}

//  / 0x80B4DE0, 89 B: every player, one call each.
void GameServer::simulatePlayersPhysics(float dt)
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        simulatePlayerPhysics(*it, dt);                         // the bool is ignored
}

void GameServer::simulatePlayersCollisions(float dt)
{
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
        simulatePlayerCollisions(*it, dt);
}

// : only in a bots game - the portal manager's update (vtable +0x40).
void GameServer::updatePortals(float dt)
{
    if (ai::g_aiInstance && ai::aiInstance()->isBotsGame())
        portalManagerUpdate(world::portalManager, dt);          // [FRONTIER]
}

// Marks an acknowledged object ghostable for one connection. Missing connection
// or object is ignored. Object lookup narrows the supplied id to 16 bits.
void GameServer::confirmGhost(int connectionId, unsigned int objectId)
{
    IClientConnection* connection = getClient(connectionId);
    if (!connection)
        return;

    world::IObject* object =
        io::networkManager->getObject(static_cast<unsigned short>(objectId));
    if (!object)
        return;

    asServerConnection(connection)->updateGhostable(object, true);      // ServerConnection::updateGhostable 0x8089E60
}

// Gets the player's peer address without its port; returns false when no address is available.
// A colon at position zero is retained.
bool GameServer::getPlayerConnectionIp(world::IPlayer* player, std::string& out)
{
    const int playerId = player->getId();
    IClientConnection* const conn = getClient(playerId);
    if (!conn || !m_netServer->getConnectionIp(asServerConnection(conn)->m_connectionId, out))
        return false;

    const std::string::size_type colon = out.find(":");
    if (colon != 0)
        out = out.substr(0, colon);
    return true;
}

// Queues the player database and unlocks, then advances to state 1. Requires an existing connection.
void GameServer::clientSendPlayerDatabase(int connectionId)
{
    ServerConnection* const conn = asServerConnection(getClient(connectionId));
    conn->clientSendPlayerDatabase();
    conn->m_connectionState = 1;
}


// Enters a player into the first controllable object down the target's child chain.
// Ladder containers are rejected fatally. The old control object receives onEnter with the new target.
// Camera carry retains the outgoing pitch; developer mode also preserves view mode.
// Network and script notifications occur only when the new vehicle has a networkable.
bool GameServer::enterVehicle(world::IPlayer* player, world::IObject* object, bool setAsDefaultVehicle)
{
    typedef world::object_detail::LayoutAccess L;

    // 1. A ladder IS NOT enterable, and the refusal is abort() - a process kill, not a Debug.
    if (object->getTemplate()->getClassID() == world::CID_LadderContainerTemplate)
        abort();

    // 2. the control object at or below the target
    world::IPlayerControlObject* pco = 0;
    for (world::IObject* node = object; node; node = node->getChild())
    {
        pco = static_cast<world::IPlayerControlObject*>(
                  node->queryInterface(world::IID_IPlayerControlObject));
        if (pco)
            break;
    }

    world::IPlayerControlObjectTemplate* pcoTemplate =
        static_cast<world::IPlayerControlObjectTemplate*>(
            object->getTemplate()->queryInterface(
                world::IID_IPlayerControlObjectTemplate));

    // 3. the outgoing camera's orientation and view mode
    world::ICameraObject* oldCamera = 0;
    world::IObject* defaultVehicle = player->getDefaultVehicle();          // IPlayer +0xA4
    if (defaultVehicle)
    {
        world::IPlayerControlObject* defaultPco =
            static_cast<world::IPlayerControlObject*>(
                defaultVehicle->queryInterface(world::IID_IPlayerControlObject));
        oldCamera = defaultPco->getActiveCamera();                         // ipco +0x44
    }

    float rotZ = 0.0f, rotX = 0.0f, rotY = 0.0f;
    if (oldCamera)
    {
        world::IObject* cameraObject = static_cast<world::IObject*>(
            oldCamera->queryInterface(world::IID_IObject));
        getRotationZXY(cameraObject->getRelativeTransformation(), rotZ, rotX, rotY);
        // all three negated by sign flip, and kept for step 6
        rotZ = -rotZ;
        rotX = -rotX;
        rotY = -rotY;
    }

    int viewMode = 8;                                                      // the default is 8
    if (oldCamera)
        viewMode = static_cast<int>(oldCamera->getViewMode());             // ICameraObject +0x1C

    // 4. an unoccupied control object holding an object: destroy that object first
    if (!pco->getOccupingPlayer() && pco->getOccupingObject())
        game->destroyObject(pco->getOccupingObject());

    // 5. the object being left is told what is being entered
    world::IPlayerControlObject* oldPco = 0;
    for (world::IObject* node = player->getVehicle(); node; node = node->getChild())
    {
        oldPco = static_cast<world::IPlayerControlObject*>(
                     node->queryInterface(world::IID_IPlayerControlObject));
        if (oldPco)
            break;
    }
    if (oldPco)
        oldPco->onEnter(object);                                           // ipco +0x28

    pco->enter(player);                                                    // ipco +0x1C

    // 6. carry the orientation across. Both ports dispatch interface-relative +0x108 here
    // (ELF 0x80AF286, PE 0x44520B): getMaintainCameraOnEnter, not SimpleObjectTemplate's
    // getRelativePositionOffset. Keeping the adjusted secondary pointer typed is load-bearing.
    world::ICameraObject* newCamera = pco->getActiveCamera();
    if (newCamera && pcoTemplate && pcoTemplate->getMaintainCameraOnEnter())
    {
        bool forceViewMode = false;
        if (settings->boolGet(std::string("GSDeveloper"), false))          // see the header note
            forceViewMode = (object != player->getFreeCamera());
        if (forceViewMode)
            newCamera->setViewMode(static_cast<world::CameraViewMode>(viewMode), 0, false);

        world::IObject* cameraObject = static_cast<world::IObject*>(
            newCamera->queryInterface(world::IID_IObject));

        const Vec3 rotation = {0.0f, rotX, 0.0f};
        Mat4 transform = L::parent(cameraObject)
                             ? cameraObject->getRelativeTransformation()
                             : cameraObject->getAbsoluteTransformation();
        setRotationZXY(transform, -rotation.z, -rotation.y, -rotation.x);
        if (L::parent(cameraObject))
            cameraObject->setRelativeTransformation(transform);
        else
            cameraObject->setAbsoluteTransformation(transform);

        // ELF Camera primary +0x180 -> 81DFA80; PE +0x178 -> 5F2F70: const Vec3& input.
        // The shipped maintain-camera arm assumes Camera/RotationalBundle, without a type guard.
        static_cast<world::Camera*>(cameraObject)->setCurrentRotation(rotation);

        if (pco->getOccupingObject())
            soldierSetLookUpDownAngle(pco->getOccupingObject(), rotX);     // the ghost hook
    }

    // 7. the new vehicle stops simulating, and the world is told
    disablePhysics(player->getVehicle(), false);

    if (setAsDefaultVehicle)
        player->setDefaultVehicle(player->getVehicle());                   // IPlayer +0xA0
    eventManager->postEvent(ECGame, 9, player, 0.0f);                      // EventManager +0x14

    // 8. replication and script, only for an object that replicates
    if (!player->getVehicle()->getNetworkable())                           // IObject slot 54
        return true;

    if (player->getVehicle()->getTemplate()->getClassID() != world::CID_FreeCameraTemplate)
    {
        const int playerId = player->getId();
        const unsigned short netId = static_cast<unsigned short>(
            player->getVehicle()->getNetworkable()->m_networkableId);

        GameEvent* event = createEnterVehicleEvent(playerId, netId, setAsDefaultVehicle);
        game->sendGameEventToAllPlaying(event);                            // IGame +0x84

        // Soldiers raise Python event 23 with player/object; other vehicles raise event 21 with child-soldier state too.
        if (player->getVehicle()->getTemplate()->getClassID() == world::CID_SoldierTemplate)
        {
            world::IObject* vehicle = player->getVehicle();
            python::PyRef args = python::buildArgs(player, vehicle);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(23), args.get());
        }
        else
        {
            const bool childSoldierActive = pco->isChildSoldierActive();   // ipco +0x140
            world::IObject* vehicle = player->getVehicle();
            python::PyRef args = python::buildArgs(player, vehicle, childSoldierActive);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(21), args.get());
        }
    }

    // 9. the local player's control map follows the vehicle type
    if (player != world::playerManager->getLocalHumanPlayer())             // +0x30
        return true;

    world::IPlayerControlObjectTemplate* enteredTemplate =
        static_cast<world::IPlayerControlObjectTemplate*>(
            object->getTemplate()->queryInterface(
                world::IID_IPlayerControlObjectTemplate));
    if (!enteredTemplate)
        return true;

    int controlsCategory;
    if (enteredTemplate->getControlsCategory() == 4)                       // 4 defers to the other slot
        controlsCategory = enteredTemplate->getVehicleCategory();
    else
        controlsCategory = enteredTemplate->getControlsCategory();         // repeated virtual call, as shipped

    std::string controlMap;
    if (pco->isChildSoldierActive())
    {
        controlMap = "InfantryPlayerInputControlMap";                      // on foot beats the type
    }
    else
    {
        switch (controlsCategory)
        {
        case 1:  controlMap = "SeaPlayerInputControlMap";        break;
        case 2:  controlMap = "AirPlayerInputControlMap";        break;
        case 3:  controlMap = "HelicopterPlayerInputControlMap"; break;
        default: controlMap = "LandPlayerInputControlMap";       break;
        }
    }
    inputManagerSetControlMap(io::inputManager, controlMap);
    return true;
}

// GameServer::exitVehicle - ELF 0x80AF980 / PE 0x444810. Both builds converge on a literal true
// result. The two booleans independently control replication and the Python notification.
bool GameServer::exitVehicle(world::IPlayer* player, bool sendReplication, bool fireScript)
{
    world::IObject* oldVehicle = player->getVehicle();

    // Retail performs this parent walk/QI even though the located interface is not retained.
    for (world::IObject* node = oldVehicle; node; node = node->getParent())
    {
        if (node->queryInterface(world::IID_IPlayerControlObject))
            break;
    }

    if (player == world::playerManager->getLocalHumanPlayer())
        inputManagerSetControlMap(io::inputManager,
                                 std::string("InfantryPlayerInputControlMap"));

    reenablePhysics(oldVehicle, false);
    if (player->getIsAlive())
    {
        Game::internal_exitPco(oldVehicle);
        world::IObject* currentVehicle = player->getVehicle();
        if (currentVehicle)
        {
            disablePhysics(currentVehicle, false);
            resetPhysics(currentVehicle, false);
        }
    }
    else
    {
        Game::internal_exitPlayerPco(oldVehicle);
    }

    const unsigned int classId = oldVehicle->getTemplate()->getClassID();
    if (classId != world::CID_FreeCameraTemplate &&
        classId != world::CID_LadderContainerTemplate)
    {
        eventManager->postEvent(ECGame, 11, player, 0.0f);

        if (sendReplication)
        {
            GameEvent* event = new ExitVehicleEvent(player->getId(), !player->getIsAlive());
            sendGameEventToAllPlaying(event);
        }

        if (fireScript && classId != world::CID_SoldierTemplate)
        {
            python::pythonHost()->updateTriggersFromObject(player->getVehicle());
            python::PyRef args = python::buildArgs(player, oldVehicle);
            python::pythonHost()->handleEvent(static_cast<ScriptEvent>(22), args.get());
        }
    }

    return true;
}

// IEventWrapper::getTargetId - ELF 0x80B95F0, 10 B. The base answer is -1; the string wrapper
// overrides it with the event's +0x20.
int GameServer::IEventWrapper::getTargetId()
{
    return -1;
}

// Tears down a disconnected player, ignoring null. Death and vehicle exit precede
// squad removal, script/event notifications and network/player-manager removal.
// Any remaining controlled vehicle must be the free camera; other types are fatal.
// The optional AI remote-player cleanup is omitted until its entry interface is available.
void GameServer::destroyPlayer(world::IPlayer* player)
{
    typedef world::object_detail::LayoutAccess L;

    if (!player)
        return;

    if (world::IObject* vehicle = player->getVehicle())                      // +0x48
    {
        if (vehicle != player->getFreeCamera() || player->getIsAlive())      // +0xA8 / +0x6C
            gameLogic->killPlayer(player, true, false);                      // IGameLogic +0x6C
        vehicle = player->getVehicle();
        if (vehicle)
        {
            if (vehicle != player->getFreeCamera())
                std::abort();                                                // as shipped
            game->destroyObject(vehicle);                                    // IGame +0xA0
            game->exitVehicle(player, true, true);                           // IGame +0xA8
        }
    }
    if (world::IObject* const left = player->getVehicle())
    {
        if (L::objectTemplate(left)->getClassID() != world::CID_FreeCameraTemplate)
            std::abort();                                                    // as shipped
    }

    if (squadManager)                                                        // ds:0x8ACBFA4
    {
        ISquadManager* const squads = static_cast<ISquadManager*>(squadManager);
        if (player->getSquadId())                                            // +0x10C
        {
            const int playerId = player->getId();                            // +0x54
            const int squadId  = player->getSquadId();                       // +0x10C
            const int team     = player->getTeam();                          // +0xE8
            squads->removeFromSquad(team, squadId, playerId, false);         // +0x34
        }
        if (player->getIsCommander())                                        // +0x114
            squads->resignCommander(player->getTeam());                      // +0x68
    }

    {
        python::PyRef args = python::buildArgs(player);                      // <IPlayer*>
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(15), args.get());
    }

    if (player->getFreeCamera())
        destroyObject(player->getFreeCamera());                              // this, +0xA0

    eventManager->postEvent(ECGame, 2, player, 0.0f);                       // +0x14
    if (!player->getIsAlive())                                               // +0x6C
        --m_numPlayers;                                                      // +0x80

    sendGameEventToAllConnected(new DestroyPlayerEvent(player->getId()));   // this, +0x94
    io::networkManager->removeNetworkable(player->getNetworkable()->m_networkableId);   // +0x190 -> +0x04
    world::playerManager->removePlayer(player);                              // +0x14

    // [UNRESOLVED] the ai::CommonSingletonServer remote-player drop - omitted; see above.

    delete player;                                                           // the deleting destructor
}

// Removes and deletes the client connection before closing its protocol connection
// and disconnecting authentication sessions. Unknown ids are ignored; known clients
// require the live listener and authenticator.
void GameServer::closeClientConnection(int connectionId, unsigned int reason, const std::string& text)
{
    IClientConnection* const connection = getClient(connectionId);
    if (!connection)
        return;

    const int gameId = static_cast<ServerConnection*>(connection)->m_gameId;   // +0x184, read first
    m_connections.erase(static_cast<unsigned char>(connectionId));            // +0x74
    delete connection;

    m_netServer->closeConnection(connectionId, reason, text);                 // +0x64 -> +0x5C
    m_matchMakingServer->getAuthenticator()->disconnectPlayer(gamespy::g_gameID, connectionId);   // +0x14
    if (gameId != -1)
        m_matchMakingServer->getAuthenticator()->disconnectPlayer(gameId, connectionId);
}

// Relays using the authenticated connection ID. Channels 12 and 13 temporarily use sender 0xFF,
// then restore the original ID after creating server and AI copies.
void GameServer::handleRadioMessageReceivedEvent(RadioMessageReceivedEvent* event, void*)
{
    RadioMessageEW wrapper(event);
    const unsigned char connectionId = event->m_connectionId;
    if (static_cast<unsigned int>(event->m_channel - 12) <= 1u)   // TCServerMessage / TCServerTeamMessage
        event->m_connectionId = static_cast<unsigned char>(-1);

    playerMessageEvent(wrapper);

    ServerRadioMessageEW serverWrapper(event);
    serverPlayerMessageEvent(serverWrapper);

    if (ai::g_aiInstance && ai::aiInstance()->isBotsGame())
        sendRadioMessageToAI(wrapper, event->m_messageId);

    event->m_connectionId = connectionId;
}

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: gameserver-radio-ai-courier]
// The no-bot production path above never enters this arm. Keeping a fail-loud callable boundary
// prevents a bots-enabled server from silently dropping radio commands while the 309-byte courier
// body and its AI-event payload remain unrecovered.
void GameServer::sendRadioMessageToAI(IEventWrapper&, int)
{
    std::fprintf(stderr,
                 "[FRONTIER] dice::hfe::GameServer::sendRadioMessageToAI reached\n");
    std::fflush(stderr);
    std::abort();
}

// Forwards chat to player/admin and server-console listeners, temporarily hiding
// the sender on server channels. Restores the sender before the Python callback,
// which receives sender, message, channel name, and flags.
void GameServer::handleStringReceivedEvent(StringReceivedEvent* event, void*)
{
    const int senderId = event->m_senderId;                                  // +0x1C
    if (static_cast<unsigned int>(event->m_channel - 12) <= 1u)              // 12 or 13
        event->m_senderId = -1;

    StringMessageEW wrapper(event);
    if (event->m_channel == 14)
        adminToPlayerEvent(wrapper);
    else
        playerMessageEvent(wrapper);

    ServerStringMessageEW serverWrapper(event);
    serverPlayerMessageEvent(serverWrapper);

    event->m_senderId = senderId;

    std::string channelName("(unknown)");
    switch (event->m_channel)                                                // +0x28
    {
    case 0:  channelName = "Global";               break;
    case 1:  channelName = "Team";                 break;
    case 2:  channelName = "Commander";            break;
    case 3:  channelName = "Squad";                break;
    case 4:  channelName = "SquadLeader";          break;
    case 5:  channelName = "SquadId";              break;
    case 6:  channelName = "LocalTeamMedic";       break;
    case 7:  channelName = "LocalGroundVehicle";   break;
    case 8:  channelName = "LocalAirVehicle";      break;
    case 9:  channelName = "Local";                break;
    case 10: channelName = "LocalTeam";            break;
    case 11: channelName = "Vehicle";              break;
    case 12: channelName = "ServerMessage";        break;
    case 13: channelName = "ServerTeamMessage";    break;
    case 14: channelName = "Player";               break;
    case 15:
    case 16: channelName = "LocalRevive";          break;
    case 17: channelName = "TeamAndLocalEnemies";  break;
    case 18: channelName = "SquadAndLocalEnemies"; break;
    case 19: channelName = "Invalid";              break;
    default: break;
    }

    const std::string message(reinterpret_cast<const char*>(event->m_message), event->m_messageLength);   // +0x34 / +0x30
    python::PyRef args = python::buildArgs(event->m_senderId, message, channelName, event->m_flags);   // <int,string,string,int>
    python::pythonHost()->handleEvent(static_cast<ScriptEvent>(39), args.get());
}

// adminToPlayerEvent - ELF 0x80AEA80, 223 B. The Player channel (14): a message aimed at ONE
// player. Allowed when the sender is the server (-1) or a known player who is not spamming
// (getIsThisASpam(true) - this one counts); then every connection past state 0 whose player's id
// equals the wrapper's +0x14 (getFlags - see the header) receives it.
void GameServer::adminToPlayerEvent(IEventWrapper& wrapper)
{
    if (wrapper.getSenderId() != -1)
    {
        world::IPlayer* const sender = world::playerManager->getPlayerFromId(wrapper.getSenderId());   // +0x20
        if (!sender)
            return;
        if (sender->getIsThisASpam(true))                                    // +0x30C
            return;
    }
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        IClientConnection* const connection = it->second;
        if (clientConnectionState(connection) <= 0)                          // +0x0C
            continue;
        world::IPlayer* const player = connection->getPlayer();              // +0x18
        if (!player)
            continue;
        if (player->getId() == wrapper.getFlags())                           // +0x54 vs +0x14
            wrapper.sendMessage(static_cast<ServerConnection*>(connection));
    }
}

namespace {
// The "is this player out of the fight" test the two player-message dispatchers and filterMessage
// share: dead, or man-down (with the two LocalRevive channels exempting a man-down sender so a
// downed player can still call for a medic).
inline bool chatSenderIsDown(GameServer::IEventWrapper& wrapper, world::IPlayer* sender)
{
    if (!sender->getIsAlive())                                               // +0x6C
        return true;
    return sender->getIsManDown() && wrapper.getChannel() != 15 && wrapper.getChannel() != 16;   // +0x100
}
inline bool chatReceiverIsDown(world::IPlayer* receiver)
{
    return !receiver->getIsAlive() || receiver->getIsManDown();
}
// filterMessage's own dead test (cases 4, 5, 14): alive / man-down only, no channel exemption.
inline bool chatSenderIsDownNoRevive(world::IPlayer* sender)
{
    return !sender->getIsAlive() || sender->getIsManDown();
}
} // namespace

// Routes non-spam messages to started connections. Server channels bypass sender
// requirements; other channels require a sender and restrict dead senders to dead
// receivers before filterMessage decides delivery.
void GameServer::playerMessageEvent(IEventWrapper& wrapper)
{
    world::IPlayer* const sender = wrapper.getSenderId() == -1
        ? world::playerManager->getLocalHumanPlayer()                        // +0x30
        : world::playerManager->getPlayerFromId(wrapper.getSenderId());      // +0x20

    const bool serverChannel = wrapper.getChannel() == 12 || wrapper.getChannel() == 13;
    if (!serverChannel)
    {
        if (!sender)
            return;
        if (sender->getIsThisASpam(sender->getId() != -1))                   // +0x30C
            return;
    }

    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        IClientConnection* const connection = it->second;
        if (clientConnectionState(connection) <= 0)                          // +0x0C
            continue;
        world::IPlayer* const player = connection->getPlayer();              // +0x18
        if (!player)
            continue;
        if (wrapper.getChannel() != 12 && wrapper.getChannel() != 13)
        {
            const bool senderDown   = chatSenderIsDown(wrapper, sender);
            const bool receiverDown = chatReceiverIsDown(player);
            if (senderDown && receiverDown != senderDown)
                continue;
        }
        if (filterMessage(wrapper, sender, player))
            wrapper.sendMessage(static_cast<ServerConnection*>(connection));
    }
}

// serverPlayerMessageEvent - ELF 0x80AED50, 338 B. The server's own copy of the line: the sender
// as above (and it must exist), server channels or a non-spam sender (getIsThisASpam(false) -
// NOT counted), and the receiver is the local human player - null on a dedicated server, which
// ends it. With one: the dead-to-dead rule, filterMessage, then sendMessage(0).
void GameServer::serverPlayerMessageEvent(IEventWrapper& wrapper)
{
    world::IPlayer* const sender = wrapper.getSenderId() == -1
        ? world::playerManager->getLocalHumanPlayer()
        : world::playerManager->getPlayerFromId(wrapper.getSenderId());
    if (!sender)
        return;
    if (wrapper.getChannel() != 12 && wrapper.getChannel() != 13 && sender->getIsThisASpam(false))
        return;

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // +0x30
    if (!local)
        return;
    const bool senderDown = chatSenderIsDown(wrapper, sender);
    const bool localDown  = chatReceiverIsDown(local);
    if (senderDown && localDown != senderDown)
        return;
    if (filterMessage(wrapper, sender, local))
        wrapper.sendMessage(0);
}

namespace {
inline float chatDistanceSq(world::IObject* a, world::IObject* b)
{
    // the ELF's operand order: z first, then x, then y, summed z + y + x
    const Vec3& pa = a->getAbsolutePosition();                               // +0x6C
    const Vec3& pb = b->getAbsolutePosition();
    const float dz = pb.z - pa.z;
    const float dx = pb.x - pa.x;
    const float dy = pb.y - pa.y;
    return dz * dz + dy * dy + dx * dx;
}
} // namespace

// Returns whether a receiver may hear the sender on the selected message channel.
// Applies channel-specific team, squad, vehicle and distance rules; laser-designation
// messages can also post a HUD event. SquadLeader, SquadId and Player require a nonnull sender.
bool GameServer::filterMessage(IEventWrapper& wrapper, world::IPlayer* sender, world::IPlayer* receiver)
{
    typedef world::object_detail::LayoutAccess L;

    const int channel = wrapper.getChannel();                                // +0x10
    switch (channel)
    {
    case 0:
        return sender != receiver;

    case 1:
    case 17:
        return filterTeamTail(wrapper, sender, receiver);

    case 2:
    {
        if (sender == receiver)
            return false;
        if (!receiver->getIsCommander() && !receiver->getIsSquadLeader())   // +0x114 / +0x13C
            return false;
        if (!sender)
            return true;
        return receiver->getTeam() == sender->getTeam();                     // +0xE8
    }

    case 3:
    case 18:
    {
        if (!sender || sender == receiver)
            return false;
        world::IObject* const sv = sender->getVehicle();                     // +0x48
        world::IObject* const rv = receiver->getVehicle();
        if (!sv || !rv)
            return false;
        const float distanceSq = chatDistanceSq(sv, rv);
        if (wrapper.getChannel() == 18 && m_messageDistanceSqB >= distanceSq)   // +0xAC
            return true;
        if (receiver->getIsCommander())
            return false;
        if (sender->getSquadId() != receiver->getSquadId())                  // +0x10C
            return false;
        return filterTeamTail(wrapper, sender, receiver);
    }

    case 4:
    {
        const bool senderDown   = chatSenderIsDownNoRevive(sender);
        const bool receiverDown = chatReceiverIsDown(receiver);
        if (sender == receiver || (senderDown && receiverDown != senderDown))
            return false;
        if (wrapper.getFlags() == receiver->getSquadId() && receiver->getIsSquadLeader())
            return receiver->getTeam() == sender->getTeam();
        return false;
    }

    case 5:
    {
        const bool senderDown   = chatSenderIsDownNoRevive(sender);
        const bool receiverDown = chatReceiverIsDown(receiver);
        if (sender == receiver || (senderDown && receiverDown != senderDown))
            return false;
        if (wrapper.getFlags() == receiver->getSquadId())
            return receiver->getTeam() == sender->getTeam();
        return false;
    }

    case 6:
    case 15:
    {
        if (!sender || !receiver)
            return false;
        if (sender == receiver)
            return false;
        if (receiver->getDefaultVehicle()                                    // +0xA4
            && sender->getTeam() == receiver->getTeam())
        {
            world::ISoldier* const soldier = static_cast<world::ISoldier*>(
                receiver->getDefaultVehicle()->queryInterface(world::IID_ISoldier));
            if (soldier)
            {
                world::Kit* const kit = soldier->getKit();                   // +0x54
                if (kit)
                {
                    world::IObjectComponent* const ability = L::abilityGLComponent(kit);   // +0x64
                    if (ability)
                    {
                        world::IObject* const sv = sender->getVehicle();
                        world::IObject* const rv = receiver->getVehicle();
                        if (!sv || !rv)
                            return false;
                        const world::AbilityGLInfo* const info =
                            static_cast<world::IAbilityGLComp*>(ability)->getGLInfo();   // +0x38
                        if (info->hasHealingAbility)
                        {
                            if (m_messageDistanceSqA >= chatDistanceSq(sv, rv))   // +0xA8
                                return true;
                        }
                    }
                }
            }
        }
        return filterLocalTail(wrapper, sender, receiver);
    }

    case 7:
    case 8:
    {
        if (!sender)
            return false;
        if (sender == receiver)
            return false;
        world::IObject* const sv = sender->getVehicle();
        world::IObject* const rv = receiver->getVehicle();
        if (!sv || !rv)
            return false;
        const float distanceSq = chatDistanceSq(sv, rv);
        if (wrapper.getChannel() == 7 && m_messageDistanceSqB >= distanceSq)   // +0xAC
            return true;
        if (sender->getTeam() != receiver->getTeam())
            return false;
        if (sender->getSquadId() && sender->getSquadId() == receiver->getSquadId())
            return true;
        if (rv == receiver->getDefaultVehicle())                             // +0xA4
            return false;
        world::PlayerControlObjectTemplate* const templ = static_cast<world::PlayerControlObjectTemplate*>(
            const_cast<world::IObjectTemplate*>(L::objectTemplate(rv))
                ->queryImplementation(world::CID_PlayerControlObjectTemplate));   // the +380 face
        if (templ && templ->getVehicleCategory() == 0)                       // +0x24 on that face
            return m_messageDistanceSqD >= chatDistanceSq(sv, rv);           // +0xB4
        if (!templ || (templ->getVehicleCategory() != 2 && templ->getVehicleCategory() != 3))
            return false;
        return m_messageDistanceSqC >= chatDistanceSq(sv, rv);               // +0xB0
    }

    case 9:
    case 10:
    case 16:
        return filterLocalTail(wrapper, sender, receiver);

    case 11:
    {
        if (sender == receiver || !sender)
            return false;
        world::IObject* const sv = sender->getVehicle();
        world::IObject* const rv = receiver->getVehicle();
        if (!sv || !rv)
            return false;
        return world::getRootParent(sv) == world::getRootParent(rv);
    }

    case 12:
        return true;

    case 13:
        return wrapper.getFlags() == receiver->getTeam();                    // +0x14 vs +0xE8

    case 14:
    {
        const bool senderDown   = chatSenderIsDownNoRevive(sender);
        const bool receiverDown = chatReceiverIsDown(receiver);
        if (sender == receiver || (senderDown && receiverDown != senderDown))
            return false;
        return wrapper.getFlags() == receiver->getId();                      // +0x14 vs +0x54
    }

    default:
        return false;
    }
}

// The Team tail (cases 1 / 17, and 3 / 18 fall into it) - 0x80AE750..0x80AE8FC.
bool GameServer::filterTeamTail(IEventWrapper& wrapper, world::IPlayer* sender, world::IPlayer* receiver)
{
    if (!sender || sender == receiver)
        return false;
    world::IObject* const sv = sender->getVehicle();                         // +0x48
    world::IObject* const rv = receiver->getVehicle();
    if (!sv || !rv)
        return false;
    const float distanceSq = chatDistanceSq(sv, rv);
    if (wrapper.getChannel() == 17 && m_messageDistanceSqB >= distanceSq)   // +0xAC
        return true;
    if (sender->getTeam() != receiver->getTeam())                            // +0xE8
        return false;
    if ((wrapper.getFlags() & 1) == 0)                                       // +0x14
        return true;
    if (playerControlsLaserGuidedWeapon(receiver))                           // Game's
    {
        sendGameEventIfPlaying(receiver->getId(),                            // +0x88
            new PostRemoteEvent(ECHud, 66, 58, 0, 0, 0.0f));
        return true;
    }
    return false;
}

// The Local tail (cases 9 / 10 / 16, and 6 / 15 fall into it) - 0x80AE41B..0x80AE5C1.
bool GameServer::filterLocalTail(IEventWrapper& wrapper, world::IPlayer* sender, world::IPlayer* receiver)
{
    if (!sender || sender == receiver)
        return false;
    world::IObject* const sv = sender->getVehicle();
    world::IObject* const rv = receiver->getVehicle();
    if (!sv || !rv)
        return false;
    const Vec3& sp = sv->getAbsolutePosition();                              // +0x6C, read first
    const Vec3& rp = rv->getAbsolutePosition();
    bool sameTeam = true;
    if (wrapper.getChannel() == 10)
        sameTeam = sender->getTeam() == receiver->getTeam();
    const float limit = (wrapper.getChannel() == 6 || wrapper.getChannel() == 15 || wrapper.getChannel() == 16)
        ? m_messageDistanceSqB : m_messageDistanceSqA;                       // +0xAC : +0xA8
    const float dz = rp.z - sp.z;
    const float dx = rp.x - sp.x;
    const float dy = rp.y - sp.y;
    const float distanceSq = dz * dz + dy * dy + dx * dx;
    if (wrapper.getChannel() == 10 && !sameTeam && m_messageDistanceSqB >= distanceSq)
        return true;                                                         // an enemy within B hears LocalTeam
    if (limit >= distanceSq)
        return sameTeam;
    return false;
}

// Returns false without a listener, leaving the output untouched.
bool GameServer::getConnectionStatus(int connectionId, io::ConnectionStatus& out)
{
    return m_netServer ? m_netServer->getConnectionStatus(connectionId, out) : false;
}

//  / PE 0x444CF0. Signed state >= 4, including states beyond playing.
bool GameServer::isClientReady(int connectionId)
{
    IClientConnection* const connection = getClient(connectionId);
    return connection && static_cast<ClientConnection*>(connection)->m_connectionState > 3;
}

//  / PE 0x446B30. Walk every connection, including recording/unready ones.
int GameServer::getTotalActiveGhosts()
{
    int total = 0;
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        if (it->second)
            total += it->second->debugGetNumActiveGhosts();
    }
    return total;
}

//  / PE 0x444F60. This public wrapper has a player-null guard.
bool GameServer::getPlayerConnectionIpAndPort(world::IPlayer* player, std::string& out)
{
    return player ? getConnectionIpAndPort(player->getId(), out) : false;
}

//  / PE 0x443AD0. Resolve player id to the connection's separate network id.
// No listener-null guard after finding a connection, and no stripping of the port suffix.
bool GameServer::getConnectionIpAndPort(int playerId, std::string& out)
{
    IClientConnection* const connection = getClient(playerId);
    if (!connection)
        return false;
    return m_netServer->getConnectionIp(static_cast<ClientConnection*>(connection)->m_connectionId, out);
}

//  / PE 0x443B20 and ELF 0x80B6000 / PE 0x443B60 respectively.
// Both construct the hidden return object, including an empty string without a listener.
std::string GameServer::getServerAddress() const
{
    return m_netServer ? m_netServer->getServerAddress() : std::string("");
}

std::string GameServer::getName() const
{
    return m_netServer ? m_netServer->getName() : std::string("");
}

//  / PE 0x44FB40. Stored sv.internet value, not a listener query.
bool GameServer::getIsInternetServer() const
{
    return m_isInternetServer;
}

//  / PE 0x443C20. No invented payload or size guards.
void GameServer::sendUDPPacket(const char* address, unsigned short port, void* data, int size)
{
    if (m_netServer)
        m_netServer->sendRawPacket(address, port, static_cast<const char*>(data), size);
}

//  / PE 0x4434D0. All wrappers preserve the virtual dispatch and bool result.
bool GameServer::say(const std::string& text)
{
    return say(text, m_currentChannel, m_currentType, 0, m_currentFlags);
}


bool GameServer::sayAll(const std::string& text)
{
    return say(text, static_cast<TargetChannel>(12), static_cast<ChannelMessageType>(1),
               0, m_currentFlags);
}

//  / PE 0x443530. Setting flags persists after sending; do not restore them.
bool GameServer::sayTeam(int team, const std::string& text)
{
    setCurrentFlags(team);
    return say(text, static_cast<TargetChannel>(13), static_cast<ChannelMessageType>(1),
               0, m_currentFlags);
}

//  / PE 0x443570. The event owns a copied, NUL-terminated byte buffer. Its length
// still includes embedded NULs. Preserve the two local-player queries and full-width target/flags.
bool GameServer::say(const std::string& text, TargetChannel channel, ChannelMessageType type,
                     unsigned int targetId, unsigned int flags)
{
    int senderId = -1;
    if (world::playerManager->getLocalHumanPlayer())
        senderId = world::playerManager->getLocalHumanPlayer()->getId();

    const unsigned int length = static_cast<unsigned int>(text.size());
    unsigned char* const message = new unsigned char[length + 1];
    for (unsigned int i = 0; i < length; ++i)
        message[i] = text.c_str()[i];
    message[length] = 0;

    eventManager->postEvent(ECNetwork,
        new StringReceivedEvent(senderId, targetId, flags, channel, type, length, message), 0.0f);
    return true;
}

//  / PE 0x444D20. Compare the untruncated request first, then try its first 23 bytes.
// setName may refuse the name. Retry with _0, _1, ... only while the player's name remains equal
// to its original name; no retry limit or alternate uniqueness check exists in either binary.
bool GameServer::changePlayerName(int playerId, const std::string& name)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player || name == player->getName())
        return false;

    const std::string baseName(name, 0, 23);
    const std::string oldName(player->getName());
    player->setName(baseName);
    int suffix = 0;
    while (player->getName() == oldName)
    {
        player->setName(baseName + formatString("_%d", suffix));
        ++suffix;
    }
    sendGameEventToAll(new ChangePlayerNameEvent(player));
    return true;
}

// Server controls and request bookkeeping.
// These methods do not drive the deferred HTTP backend.

//  / PE 0x443920: direct tail-call, with no gameLogic or player guard here.
void GameServer::toggleFreeCamera(world::IPlayer* player)
{
    static_cast<ServerGameLogic*>(gameLogic)->do_toggleFreeCamera(player);
}

//  / PE 0x447990 (secondary-interface thunk 0x44FDF0).
void GameServer::debugShowActiveGhosts(int connectionId)
{
    if (connectionId == -1)
    {
        for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
             it != m_connections.end(); ++it)
        {
            // The all-connections debug body is absent in both shipped images; the walk remains.
        }
    }
    else
    {
        std::map<unsigned char, IClientConnection*>::iterator it =
            m_connections.find(static_cast<unsigned char>(connectionId));
        if (it != m_connections.end() && it->second)
            it->second->debugShowActiveGhosts();
    }
}

//  / PE 0x446AC0: direct concrete call for each non-null map entry.
void GameServer::debugShowStreamManagerSizes()
{
    for (std::map<unsigned char, IClientConnection*>::iterator it = m_connections.begin();
         it != m_connections.end(); ++it)
    {
        if (it->second)
            static_cast<ClientConnection*>(it->second)->outputStreamManagerStats();
    }
}

//  is a five-byte empty body; PE 0x4439E0 is just retn 4.
void GameServer::debugShowGhostPriorities(int /*connectionId*/)
{
}

//  (+0xD8) / PE 0x44FB20 (IGameServer +0x90): no addRef.
void* GameServer::getStatsManager()
{
    return m_statsManager;
}

//  / PE 0x448180. The capacity test precedes insertion, even for a duplicate;
// insert's result is discarded. Both success and failure explicitly materialize a boolean.
bool GameServer::addPersistenceConnection(HTTPRequest* request)
{
    if (m_httpRequests.size() >= 256)
    {
        Debug(DebugTypeError, "Game/GameServer/GameServer.cpp", 5533, "")
            << "jlo: Too many http connections (only " << 256 << " allowed).";
        return false;
    }
    m_httpRequests.insert(request);
    return true;
}

namespace {
// Crosscasts PlayerStats or PlayerAwards to their secondary HTTPRequest interface without changing references.
// Native RTTI accounts for compiler-specific base offsets; null remains null.
HTTPRequest* persistenceHttpView(IBase* object)
{
    return dynamic_cast<HTTPRequest*>(object);
}
}

//  / PE 0x448240. Clear the matching current object before testing membership.
// Queueing does not erase, delete or deduplicate; GameServer::update performs the later drain.
bool GameServer::removePersistenceConnection(HTTPRequest* request)
{
    if (persistenceHttpView(m_currentPersistenceObject) == request)
        m_currentPersistenceObject = 0;
    if (m_httpRequests.find(request) == m_httpRequests.end())
        return false;
    m_httpRequestsToRemove.push_back(request);
    return true;
}

//  / PE 0x443C90: lazy factory lookup, then synchronous init. Retail neither
// checks the factory result nor consumes init's result; the object is cached either way.
IPlayerUnlockInfo* GameServer::getPlayerUnlockInfo()
{
    if (!m_playerUnlockInfo)
    {
        m_playerUnlockInfo = static_cast<IPlayerUnlockInfo*>(classManager->createInstance(
            std::string("dice.hfe.PlayerUnlockInfo"), IID_IPlayerUnlockInfo, 0));
        m_playerUnlockInfo->init();
    }
    return m_playerUnlockInfo;
}

//  / PE 0x5F2F30: unowned store, including null.
void GameServer::setCurrentPersistenceObject(IBase* object)
{
    m_currentPersistenceObject = object;
}

//  / PE 0x44FB50: unowned read.
IBase* GameServer::getCurrentPersistenceObject()
{
    return m_currentPersistenceObject;
}

//  / PE 0x44FB70: return the stored content-check byte.
bool GameServer::useContentChecking()
{
    return m_useContentChecking;
}

} // namespace hfe
} // namespace dice
