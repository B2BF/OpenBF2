// [TEMP] [CHOSEN, not recovered] Typed collaborators for actual event-dispatch code.
// Exercises rebuilt interfaces only; no sockets, retail DLLs or production session are started.
#include "../BF2/Core/Debug.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/IGameClient.h"
#include "../BF2/Game/MapInfo.h"
#include "../BF2/Game/Common/IClientConnection.h"
#include "../BF2/Game/Common/DataBlockManager.h"
#include "../BF2/Game/Common/StringManager.h"
#include "../BF2/Game/GameEvents/ChallengeEvent.h"
#include "../BF2/Game/GameEvents/DataBlockEvent.h"
#include "../BF2/Game/GameEvents/StringBlockEvent.h"
#include "../BF2/Game/GameEvents/DestroyPlayerEvent.h"
#include "../BF2/Game/GameEvents/DestroyObjectEvent.h"
#include "../BF2/Game/GameEvents/EnterVehicleEvent.h"
#include "../BF2/Game/GameEvents/ExitVehicleEvent.h"
#include "../BF2/IO/Network/NetworkStats.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <string>
#include <vector>

namespace {
std::vector<std::string> calls;

[[noreturn]] void unexpected(const char* method)
{
    std::fprintf(stderr, "[FRONTIER] client event fixture unexpected %s\n", method);
    std::fflush(stderr);
    std::abort();
}

void require(bool condition, const char* message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL %s\n", message);
        std::exit(1);
    }
}

void expectCalls(std::initializer_list<const char*> expected)
{
    require(calls == std::vector<std::string>(expected.begin(), expected.end()), "dispatch sequence");
    calls.clear();
}
}

namespace dice { namespace hfe {
IGame* game = nullptr;
EventManager* eventManager = nullptr;
namespace world { class PlayerManager; PlayerManager* playerManager = nullptr; }

Debug::Debug(DebugType, const std::string&, int, const std::string&)
{
    unexpected("Debug");
}
Debug::~Debug() { unexpected("~Debug"); }

// Only manager dispatch is under test. Concrete manager implementations are not linked;
// observers below override every exercised method and reject all other calls.
IClientConnection::~IClientConnection() {}
DataBlockManager::DataBlockManager(GameEventManager*, bool)
    : m_sending(nullptr), m_receiving(nullptr), m_gameEvents(nullptr), m_isRecording(false) {}
DataBlockManager::~DataBlockManager() {}
bool DataBlockManager::enqueueDataBlock(unsigned int, const void*, int) { unexpected("enqueueDataBlock"); }
void DataBlockManager::update() { unexpected("DataBlockManager::update"); }
bool DataBlockManager::newDataBlock(unsigned int, unsigned int) { unexpected("newDataBlock"); }
bool DataBlockManager::addDataChunk(void*, int) { unexpected("addDataChunk"); }
StringManager::StringManager(GameEventManager*)
    : m_sending(nullptr), m_receiving(nullptr), m_gameEvents(nullptr) {}
StringManager::~StringManager() {}
bool StringManager::enqueueString(TargetChannel, ChannelMessageType, int, const std::string&, int, int)
{ unexpected("enqueueString"); }
void StringManager::update() { unexpected("StringManager::update"); }
bool StringManager::newStringDataBlock(unsigned int) { unexpected("newStringDataBlock"); }
bool StringManager::addStringDataChunk(void*, int, bool) { unexpected("addStringDataChunk"); }
}}

using namespace dice::hfe;

namespace {
// Keep unrelated game operations fail-loud while supplying the real interface's complete shape.
#define UNUSED(Return, Name, Args, Qualifiers) \
    Return Name Args Qualifiers override { unexpected(#Name); }
struct UnusedGameMethods : IGame {
    UNUSED(void, addRef, (), )
    UNUSED(int, getRef, (), )
    UNUSED(int, release, (), )
    UNUSED(void*, queryInterface, (unsigned int), const)
    UNUSED(bool, init, (), )
    UNUSED(void, shutdown, (), )
    UNUSED(bool, load, (), )
    UNUSED(bool, loadPath, (const std::string&), )
    UNUSED(bool, unload, (), )
    UNUSED(StatsMonitor*, getStatsMonitor, (), const)
    UNUSED(bool, update, (int, float), )
    UNUSED(bool, say, (const std::string&), )
    UNUSED(TargetChannel, getCurrentChannel, (), )
    UNUSED(void, setCurrentChannel, (TargetChannel), )
    UNUSED(int, getCurrentFlags, (), )
    UNUSED(void, setCurrentFlags, (int), )
    UNUSED(ChannelMessageType, getCurrentType, (), )
    UNUSED(void, setCurrentType, (ChannelMessageType), )
    UNUSED(MapInfo, getMapInfo, (), const)
    UNUSED(bool, radioMessage, (TargetChannel, const std::string&, world::IPlayer*, unsigned int, bool, bool), )
    UNUSED(bool, radioMessageFromPlayer, (TargetChannel, const std::string&, unsigned int, unsigned int, bool, bool), )
    UNUSED(bool, localRadioMessage, (TargetChannel, const std::string&), )
    UNUSED(bool, gameInfoMessage, (TargetChannel, const std::string&, unsigned int, unsigned int, bool), )
    UNUSED(bool, localGameInfoMessage, (TargetChannel, const std::string&), )
    UNUSED(void, addPlayerInput, (const io::PlayerInput&), )
    UNUSED(int, getCurrentInputIndex, (), )
    UNUSED(void, confirmGhost, (int, unsigned int), )
    UNUSED(void, confirmReset, (int), )
    UNUSED(world::IObject*, createObjectOnAllClients, (const world::IObjectTemplate*, Math::Vec3, Math::Vec3, bool, int, int), )
    UNUSED(world::IObject*, getObjectFromNetworkableId, (const unsigned short&), const)
    UNUSED(void, createAndDestroyObject, (const world::IObjectTemplate*), )
    UNUSED(void, sendGameEventToAllPlaying, (GameEvent*), )
    UNUSED(void, sendGameEventIfPlaying, (unsigned int, GameEvent*), )
    UNUSED(void, sendGameEventToAll, (GameEvent*), )
    UNUSED(void, sendGameEvent, (unsigned int, GameEvent*), )
    UNUSED(void, sendGameEventToAllConnected, (GameEvent*), )
    UNUSED(void, sendGameEventIfConnected, (unsigned int, GameEvent*), )
    UNUSED(void, sendMissileMessageToOccupyingPlayers, (world::IObject*, world::MissileLockMessage), )
    UNUSED(bool, destroyObject, (world::IObject*), )
    UNUSED(bool, enterVehicle, (world::IPlayer*, world::IObject*, bool), )
    UNUSED(bool, exitVehicle, (world::IPlayer*, bool, bool), )
    UNUSED(std::string, listPlayers, (), const)
    UNUSED(void, joinSquad, (int), )
    UNUSED(void, leaveSquad, (int), )
    UNUSED(std::string, listSquadMembers, (int, int), )
    UNUSED(std::string, listSquads, (int), )
    UNUSED(void, changeSquadName, (const std::string&, bool), )
    UNUSED(void, makeMeCommander, (), )
    UNUSED(void, popNextCommanderApplicant, (int), )
    UNUSED(std::string, getSpottedMessage, (unsigned short), )
    UNUSED(void, sendSpottedMessage, (world::IPlayer*, unsigned short, Vec2, world::VehicleType), )
    UNUSED(void, callForSpecialist, (world::IPlayer*, HudNodeTypes), )
    UNUSED(void, recordDemo, (DemoRecorder*), )
    UNUSED(void, stopRecording, (), )
    UNUSED(bool, isPlayingDemo, (), )
    UNUSED(void, setSimulationRate, (int), )
    UNUSED(int, getSimulationRate, (), )
    UNUSED(void, debugShowActiveGhosts, (int), )
    UNUSED(const std::string&, getCurrentMapName, (), const)
    UNUSED(const std::string&, getCurrentMapGameMode, (), const)
    UNUSED(const std::string&, getCurrentMapLevelPath, (), const)
    UNUSED(int, getCurrentMapMaxPlayers, (), const)
    UNUSED(int, getCurrentMapSize, (), const)
    UNUSED(void, setCurrentMapSize, (unsigned int), )
    UNUSED(const io::BasicNetworkStats*, getNetworkStats, (), const)
    UNUSED(void, sendUDPPacket, (const char*, unsigned short, void*, int), )
    UNUSED(void, addTextToChatWindow, (const char*), )
    UNUSED(void, failedExitVehicle, (int), )
    UNUSED(bool, isPunkBusterEnabled, (), const)
    UNUSED(bool, getAbortLoadLevel, (), const)
};

struct ClientGame : UnusedGameMethods, IGameClient {
    bool exposeClient = true;
    IClientConnection* connection = nullptr;
    ChallengeEvent* challenge = nullptr;
    int player = 0;
    unsigned short networkId = 0;
    bool flag = false;
    bool statusResult = false;

    UNUSED(void, addRef, (), )
    UNUSED(int, getRef, (), )
    UNUSED(int, release, (), )
    void* queryInterface(unsigned int iid) const override
    {
        calls.push_back("queryClient");
        require(iid == IID_IGameClient, "queries the client IID");
        return exposeClient ? const_cast<IGameClient*>(static_cast<const IGameClient*>(this)) : nullptr;
    }
    UNUSED(bool, handleControlObjectState, (io::BitStream*, unsigned short, int, unsigned int, float), )
    UNUSED(bool, playDemo, (DemoRecorder*), )
    void handleChallenge(ChallengeEvent* event) override { calls.push_back("challenge"); challenge = event; }
    UNUSED(int, joinServer, (const std::string&, int, const std::string&, float, bool), )
    IClientConnection* getConnection() override { calls.push_back("connection"); return connection; }
    UNUSED(bool, createObject, (unsigned int, unsigned short, const Vec3*, const Vec3*), )
    UNUSED(bool, createKit, (unsigned int, unsigned short, unsigned int, int, const Vec3*, const Vec3&), )
    bool destroyPlayer(int id) override { calls.push_back("destroyPlayer"); player = id; return false; }
    bool destroyObject(unsigned short id) override { calls.push_back("destroyObject"); networkId = id; return false; }
    bool enterVehicle(int id, unsigned short netId, bool value) override
    { calls.push_back("enterVehicle"); player = id; networkId = netId; flag = value; return false; }
    bool exitVehicle(int id, bool value) override
    { calls.push_back("exitVehicle"); player = id; flag = value; return false; }
    bool getConnectionStatus(io::ConnectionStatus& status) override
    { calls.push_back("status"); status.ping = 0x12345678u; return statusResult; }
    UNUSED(std::string, getServerAddress, (), )
    UNUSED(void, slot44, (int), )
    UNUSED(void, restartDemo, (), )
    UNUSED(void, selectDemoPlayer, (bool), )
    UNUSED(void, nextDemoCamera, (), )
    UNUSED(int, getCurrentDemoPlayerId, (), )
    UNUSED(bool, getIsDemoFreeFlying, (), )
    UNUSED(void, togglePause, (), )
    UNUSED(void, setPause, (bool), )
    UNUSED(bool, getPause, (), )
    UNUSED(bool, isDemoAtEnd, (), )
    UNUSED(void, slot6C, (int, int), )
    UNUSED(bool, getDisconnectRequested, (), )
    UNUSED(bool, getIsReady, (), )
    UNUSED(std::string, getServerAddressCopy, (), )
    UNUSED(void, sendUDPPacket, (const char*, unsigned short, void*, int), )
};

struct DataObserver : DataBlockManager {
    unsigned int id = 0, length = 0;
    std::vector<unsigned char> bytes;
    DataObserver() : DataBlockManager(nullptr, false) {}
    bool newDataBlock(unsigned int value, unsigned int size) override
    { calls.push_back("newData"); id = value; length = size; return false; }
    bool addDataChunk(void* data, int size) override
    {
        calls.push_back("dataChunk");
        bytes.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
        return false;
    }
};

struct StringObserver : StringManager {
    unsigned int length = 0;
    bool trust = false;
    std::vector<unsigned char> bytes;
    StringObserver() : StringManager(nullptr) {}
    bool newStringDataBlock(unsigned int size) override
    { calls.push_back("newString"); length = size; return false; }
    bool addStringDataChunk(void* data, int size, bool trustSender) override
    {
        calls.push_back("stringChunk"); trust = trustSender;
        bytes.assign(static_cast<unsigned char*>(data), static_cast<unsigned char*>(data) + size);
        return false;
    }
};

struct Connection : IClientConnection {
    DataObserver data;
    StringObserver strings;
    UNUSED(void, addStreamManager, (IStreamManager*), )
    UNUSED(IStreamManager*, getStreamManager, (int), )
    DataBlockManager* getDataBlockManager() override { calls.push_back("dataManager"); return &data; }
    StringManager* getStringManager() override { calls.push_back("stringManager"); return &strings; }
    UNUSED(world::IPlayer*, getPlayer, (), )
    UNUSED(void, setPlayer, (world::IPlayer*), )
    UNUSED(bool, getIsRecordingConnection, (), const)
    UNUSED(int, debugGetNumActiveGhosts, (), )
    UNUSED(void, debugShowActiveGhosts, (), )
    UNUSED(void, setContentValid, (bool), )
    UNUSED(bool, getContentValid, (), const)
};
#undef UNUSED

struct Stats : io::BasicNetworkStats {
    Stats() { m_connectionStatus = {}; m_connectionStatus.ping = 77; }
};
}

int main()
{
    ClientGame client;
    Connection connection;
    game = static_cast<IGame*>(&client);
    require(static_cast<void*>(game) != static_cast<void*>(static_cast<IGameClient*>(&client)),
            "fixture has a secondary client subobject");
    client.connection = &connection;

    ChallengeEvent challenge;
    DestroyPlayerEvent destroyPlayer(-1);
    DestroyObjectEvent destroyObject(0xFEDCu);
    EnterVehicleEvent enterVehicle(0x1E1, 0xD00Du, true);
    ExitVehicleEvent exitVehicle(0x1FF, true);
    DataBlockEvent dataStart(0xE1234567u, 0x81234567u);
    StringBlockEvent stringStart(static_cast<unsigned char>(201));
    GameEvent* events[] = {&challenge, &destroyPlayer, &destroyObject, &enterVehicle,
                          &exitVehicle, &dataStart, &stringStart};

    client.exposeClient = false;
    for (GameEvent* event : events) {
        event->executeClient();
        expectCalls({"queryClient"});
    }
    client.exposeClient = true;
    client.connection = nullptr;
    dataStart.executeClient();
    expectCalls({"queryClient", "connection"});
    stringStart.executeClient();
    expectCalls({"queryClient", "connection"});
    client.connection = &connection;

    challenge.executeClient();
    expectCalls({"queryClient", "challenge"});
    require(client.challenge == &challenge, "challenge object identity");
    destroyPlayer.executeClient();
    expectCalls({"queryClient", "destroyPlayer"});
    require(client.player == 255, "player ID widens from its stored byte");
    destroyObject.executeClient();
    expectCalls({"queryClient", "destroyObject"});
    require(client.networkId == 0xFEDCu, "object ID preserves its high bit");
    enterVehicle.executeClient();
    expectCalls({"queryClient", "enterVehicle"});
    require(client.player == 0xE1 && client.networkId == 0xD00Du && client.flag, "vehicle entry arguments");
    exitVehicle.executeClient();
    expectCalls({"queryClient", "exitVehicle"});
    require(client.player == 255 && client.flag, "vehicle exit arguments");
    EnterVehicleEvent enterFalse(3, 4, false);
    ExitVehicleEvent exitFalse(5, false);
    enterFalse.executeClient();
    expectCalls({"queryClient", "enterVehicle"});
    require(client.player == 3 && client.networkId == 4 && !client.flag, "false entry flag");
    exitFalse.executeClient();
    expectCalls({"queryClient", "exitVehicle"});
    require(client.player == 5 && !client.flag, "false exit flag");

    dataStart.executeClient();
    expectCalls({"queryClient", "connection", "dataManager", "newData"});
    require(connection.data.id == 0xE1234567u && connection.data.length == 0x81234567u,
            "data announcement preserves unsigned ID and length");
    stringStart.executeClient();
    expectCalls({"queryClient", "connection", "stringManager", "newString"});
    require(connection.strings.length == 201, "string announcement size");

    unsigned char payload[63];
    for (unsigned int i = 0; i < sizeof(payload); ++i) payload[i] = static_cast<unsigned char>(i * 37);
    for (unsigned int size : {0u, 5u, 63u}) {
        DataBlockEvent data(payload, size);
        StringBlockEvent strings(payload, static_cast<unsigned char>(size));
        data.executeClient();
        expectCalls({"queryClient", "connection", "dataManager", "dataChunk"});
        strings.executeClient();
        expectCalls({"queryClient", "connection", "stringManager", "stringChunk"});
        const std::vector<unsigned char> expected(payload, payload + size);
        require(connection.data.bytes == expected, "data chunk contents and length");
        require(connection.strings.bytes == expected && connection.strings.trust, "string chunk contents and trusted sender");
    }

    Stats stats;
    game = nullptr;
    stats.updateClientStats();
    expectCalls({});
    require(stats.getConnectionStatus().ping == 77, "no game retains previous stats");
    game = static_cast<IGame*>(&client);
    client.exposeClient = false;
    stats.updateClientStats();
    expectCalls({"queryClient"});
    require(stats.getConnectionStatus().ping == 77, "no client retains previous stats");
    client.exposeClient = true;
    for (bool result : {false, true}) {
        client.statusResult = result;
        stats.updateClientStats();
        expectCalls({"queryClient", "status"});
        require(stats.getConnectionStatus().ping == 0x12345678u, "status output survives either Boolean result");
    }
    game = nullptr;
    std::printf("PASS client event and status dispatch (%u-bit), secondary interface and null boundaries\n",
                static_cast<unsigned int>(sizeof(void*) * 8));
    return 0;
}
