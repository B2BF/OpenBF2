// Path: header [INFERRED] beside the attested Game/GameServer/GameServer.cpp.
// Contents: recovered declarations and layout from the symbolized ELF.
// Preserve the Game, IGameServer and IEventListener base order and virtual declaration order.
// Offsets document the retail ELF; STL members have different layouts in other builds.

#ifndef GAME_GAMESERVER_GAMESERVER_H     // [INFERRED] guard style
#define GAME_GAMESERVER_GAMESERVER_H

#include <string>
#include <map>
#include <list>
#include <set>

#include "ServerSettings.h"
#include "../Main/Game.h"
#include "../Main/StatsMonitor.h"
#include "../IEventListener.h"
#include "../IGameServer.h"                       // the second base
#include "../MapList.h"
#include "../MapInfo.h"
#include "../../Core/Debug.h"
#include "../../IO/Input/PlayerInput.h"
namespace dice {
namespace hfe {

namespace io { class NetServer; class BitStream; class BasicNetworkStats; }
namespace world { class IPlayer; }
class IClientConnection;
class DemoRecorder;                               // Game/DemoRecorder/DemoRecorder.h (declaration only)
class MatchMakingServer;                          // MatchMakingServer.h (declaration only)
class IStatsManager;                              // Game/Persistence/IStatsManager.h
class IPlayerUnlockInfo;
class HTTPRequest;                                // Game/Persistence/HTTPRequest.h (declaration only)
class AutoRecorder;                               // Game/GameServer/AutoRecorder.h (attested path)
class EventNode;                                  // Core/EventManager.h - queued network-event base
class StringReceivedEvent;                        // Game/Common/StringManager.h - handleNetworkEvent arm 11
class ServerConnection;                           // Game/Common/ServerConnection.h - IEventWrapper::sendMessage
class RadioMessageReceivedEvent;                  // Game/GameEvents/RadioMessageEvent.h - arm 13
class ServerConnection;
class ClientInfo;

struct DisconnectType
{
    DisconnectType() : connectionId(0), reason(0), text() {}
    DisconnectType(int id, unsigned int why, const std::string& message)
        : connectionId(id), reason(why), text(message) {}

    int connectionId;
    unsigned int reason;
    std::string text;
};

// Dedicated/listen-server Game implementation. Preserve the Game, IGameServer,
// and IEventListener base order and adjusted pointers returned by queryInterface.
class GameServer : public Game, public IGameServer, public IEventListener
{
public:
    GameServer();
    virtual ~GameServer();
    static IBase* create(unsigned int iid, IBase* outer);

    // the IBase quartet + queryInterface (primary slots 0..3; the +0x50 table thunks to them)
    // addRef 0x80A....: `inc dword ptr [this+58h]`; release deletes on the 1->0 edge through the
    // D0 slot; queryInterface (): IBase / IGame -> this, IGameServer -> the +0x50
    // sub-object, else 0. NOT IEventListener - the +0x54 base is not answered.
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // IGame overrides with recovered bodies
    virtual bool init();                                      // +0x18
    virtual void shutdown();                                  // +0x1C
    virtual bool load();                                      // +0x20
    virtual bool loadPath(const std::string&);                // +0x24   - this batch
    virtual StatsMonitor* getStatsMonitor() const;            // +0x2C
    virtual int getCurrentInputIndex();                        // +0x6C
    virtual bool destroyObject(world::IObject*);               // +0xA0
    virtual std::string listPlayers() const;                   // +0xAC
    // Ticks simulation and services; negative dt runs networking only. Always returns true.
    virtual bool update(int steps, float dt);                 // +0x30
    virtual bool radioMessage(TargetChannel, const std::string&, world::IPlayer*,
                              unsigned int, bool, bool);        // +0x54
    virtual bool radioMessageFromPlayer(TargetChannel, const std::string&, unsigned int,
                                        unsigned int, bool, bool); // +0x58
    virtual bool gameInfoMessage(TargetChannel, const std::string&, unsigned int,
                                 unsigned int, bool);           // +0x60
    virtual bool unload();                                    // +0x28
    // Creates and broadcasts an object, returning the created instance.
    virtual world::IObject* createObjectOnAllClients(const world::IObjectTemplate* objectTemplate,
                                                     Vec3 position, Vec3 rotation, bool playingOnly,
                                                     int unused, int defaultTeam);   // +0x78
    // Also enters a free camera for a player who has no vehicle.
    virtual bool enterVehicle(world::IPlayer* player, world::IObject* object,
                              bool setAsDefaultVehicle);              // +0xA4
    virtual void confirmGhost(int connectionId, unsigned int objectId);   // +0x70
    virtual void confirmReset(int connectionId);                          // +0x74
    virtual void sendGameEventToAllPlaying(GameEvent* event);       // +0x84
    virtual void sendGameEventIfPlaying(unsigned int connectionId, GameEvent* event); // +0x88
    virtual void sendGameEventToAll(GameEvent* event);              // +0x8C
    virtual void sendGameEvent(unsigned int connectionId, GameEvent* event);   // +0x90
    virtual void sendGameEventToAllConnected(GameEvent* event);     // +0x94
    virtual void sendGameEventIfConnected(unsigned int connectionId, GameEvent* event); // +0x98
    virtual void sendMissileMessageToOccupyingPlayers(world::IObject*,
                                                       world::MissileLockMessage); // +0x9C
    virtual bool exitVehicle(world::IPlayer*, bool, bool);          // +0xA8
    virtual void recordDemo(DemoRecorder*);                         // +0xD8
    virtual void stopRecording();                                   // +0xDC
    virtual bool isPlayingDemo();                                   // +0xE0
    virtual void setSimulationRate(int);                            // +0xE4
    virtual int getSimulationRate();                                // +0xE8
    virtual void debugShowActiveGhosts(int connectionId);     // +0xEC
    virtual const io::BasicNetworkStats* getNetworkStats() const; // +0x108
    virtual void sendUDPPacket(const char*, unsigned short, void*, int); // +0x10C
    virtual void addTextToChatWindow(const char*);             // +0x110
    virtual bool isPunkBusterEnabled() const;                  // +0x118
    virtual bool say(const std::string&);                      // +0x34

    // Game owns the already-present +0x120 slot; this declaration replaces its pure target and
    // therefore does not append a new entry to the ELF primary table.
    virtual void destroyPlayer(world::IPlayer* player);

    // derived-only/secondary-base overrides, in exact ELF primary append order
    // Itanium copies these into the primary vtable in derived declaration order. Do not reorder.
    // MSVC keeps them in the secondary tables, whose order continues to come from the two bases.
    virtual bool sayAll(const std::string& text);                                      // +0x124
    virtual bool sayTeam(int team, const std::string& text);                           // +0x128
    virtual bool say(const std::string&, TargetChannel, ChannelMessageType,
                     unsigned int, unsigned int);                                     // +0x12C
    virtual int createPlayer(const std::string& name, bool isAI);                      // +0x130
    virtual int createAndSpawnAIPlayer(const std::string&, int, int);                  // +0x134
    virtual void resetConnections();                                                   // +0x138
    virtual void addScopableGhost(io::NetworkableDescriptor*);                         // +0x13C
    virtual void removeGhost(io::NetworkableDescriptor*);                              // +0x140
    virtual void addStaticGhost(io::NetworkableDescriptor*);                           // +0x144
    virtual bool startServer(const std::string&, int, int);                            // +0x148
    virtual void challengeResponse(int, GameEvent*);                                   // +0x14C
    virtual void setConnectionType(int, ConnectionType);                               // +0x150
    virtual void chokeBandwidth();                                                     // +0x154
    virtual IClientConnection* getConnection(int) const;                               // +0x158
    virtual bool getConnectionStatus(int, io::ConnectionStatus&);                      // +0x15C
    virtual int getConnectionState(int) const;                                         // +0x160
    virtual void handleEvent(EventCategory, unsigned int, EventNode*, void*);           // +0x164

    // Handles HUD IDs 18, 19, and 63. Load notifications 3 and 4 are intentional no-ops.
    // This dispatcher is non-virtual.
    void handleHudEvent(unsigned int, EventNode*, void*);
    // Dispatches network events. Object destruction posts event 17 even during unload with no clients.
    void handleNetworkEvent(unsigned int, EventNode*, void*);
    // handleNetworkEvent's three substantial dispatch helpers are now written: handleDataBlock,
    // handleStringReceivedEvent, and handleRadioMessageReceivedEvent.
    void handleDataBlock(EventNode*, void*);
    // Common chat/radio dispatch interface. Preserve slot order; admin routing reads
    // getFlags for its receiver match. The default target ID is -1.
    class IEventWrapper
    {
    public:
        virtual ~IEventWrapper() {}                                // +0x00 / +0x04
        virtual int  getSenderId() = 0;                            // +0x08
        virtual int  getTargetId();                                // +0x0C  : -1
        virtual int  getChannel() = 0;                             // +0x10
        virtual int  getFlags() = 0;                               // +0x14
        virtual void sendMessage(ServerConnection* connection) = 0;   // +0x18
    };

    // Dispatches chat through the applicable channel and per-recipient filter before sending.
    void handleStringReceivedEvent(StringReceivedEvent*, void*);
    void adminToPlayerEvent(IEventWrapper& wrapper);
    void playerMessageEvent(IEventWrapper& wrapper);
    void serverPlayerMessageEvent(IEventWrapper& wrapper);
    bool filterMessage(IEventWrapper& wrapper, world::IPlayer* sender,
                       world::IPlayer* receiver);
    // filterMessage's two shared tails (the jump table's LABEL_112 / LABEL_86 blocks), split out
    // so each arm is written once. Not ELF symbols - they are inline blocks of filterMessage.
    bool filterTeamTail(IEventWrapper& wrapper, world::IPlayer* sender, world::IPlayer* receiver);
    bool filterLocalTail(IEventWrapper& wrapper, world::IPlayer* sender, world::IPlayer* receiver);
    void handleRadioMessageReceivedEvent(RadioMessageReceivedEvent*, void*);
    void sendRadioMessageToAI(IEventWrapper&, int);                                     // ELF 0x080B5A80
    // : `if (m_isWaitingForClients) ++m_clientAckedEndOfRoundGuard` -
    void clientAckedEndOfRoundGuard(int connectionId);
    virtual bool changePlayerName(int, const std::string&);                            // +0x168
    virtual bool getPlayerConnectionIp(world::IPlayer*, std::string&);                 // +0x16C
    virtual bool getPlayerConnectionIpAndPort(world::IPlayer*, std::string&);          // +0x170
    virtual void toggleFreeCamera(world::IPlayer*);                                    // +0x174
    virtual void debugShowStreamManagerSizes();                                        // +0x178
    virtual void debugShowGhostPriorities(int);                                        // +0x17C
    virtual int getTotalActiveGhosts();                                                // +0x180
    virtual void kickPlayer(int, io::PResult, const std::string&);                     // +0x184
    virtual std::string consoleKickPlayer(const std::string&);                         // +0x188
    virtual void updatePlayersVoipStatusOnAllClients(int, bool);                       // +0x18C
    virtual void* getStatsManager();                                                   // +0x190
    virtual int getNumberOfConnectedClients() const;                                   // +0x194
    virtual int getNumberOfConnectingClients() const;                                  // +0x198
    virtual int getNumDroppedFrames() const;                                           // +0x19C
    virtual int getNumOvershotFrames() const;                                          // +0x1A0
    virtual bool getInitComplete() const;                                              // +0x1A4
    virtual bool getIsInternetServer() const;                                          // +0x1A8
    virtual std::string getServerAddress() const;                                     // +0x1AC
    virtual std::string getName() const;                                              // +0x1B0
    virtual bool addPersistenceConnection(HTTPRequest*);                              // +0x1B4
    virtual bool removePersistenceConnection(HTTPRequest*);                           // +0x1B8
    virtual void setCurrentPersistenceObject(IBase*);                                  // +0x1BC
    virtual IBase* getCurrentPersistenceObject();                                      // +0x1C0
    virtual AutoRecorder* getAutoRecorder();                                           // +0x1C4
    virtual io::NetServer* getNetServer();                                             // +0x1C8
    virtual void onContentCheckEvent(int, const unsigned char*, const unsigned char*,
                                     const unsigned char*);                            // +0x1CC
    virtual bool useContentChecking();                                                 // +0x1D0
    virtual bool isClientReady(int);                                                   // +0x1D4
    virtual void enqueueRadioMessage(EventNode*);                                      // +0x1D8
    virtual IPlayerUnlockInfo* getPlayerUnlockInfo();                                   // +0x1DC
    virtual bool isPythonValid();                                                      // +0x1E0
    virtual void setPythonValid(int, bool);                                            // +0x1E4

    // Not virtual - createPlayer is its only caller. 0x80AFCA0.
    world::IPlayer* createPlayerWithValidName(const std::string& name, int id);

    // non-virtual recovered methods
    void limitCoopPlayers();                                  // this batch
    void waitForClients();                                    // this batch
    bool handleUnloadLevel();                                 // this batch
    void findNetworkableObjects();                            // this batch
    void clientLoadLevel(int);                                // 9 B: a tail-call
                                                              // forwarder to sendClientMapInfo(int)
    void clientUnloadLevel(int);                              // empty MapInfo block + state reset
    void sendClientMapInfo(int connectionId);                 // this batch (-1 = all)
    void sendDataBlockToAll(unsigned int, const void*, unsigned int, bool onlyStarted);
    void sendDataBlock(int, unsigned int, const void*, unsigned int, bool onlyStarted);
    bool onNewConnection(int connectionId);
    // Recovered handshake payoff. onNewConnection calls it directly for the demo
    // pseudo-connection; a real client reaches it through challengeResponse.
    void authOk(int connectionId);
    void authFailed(int connectionId, unsigned int reason);    // queues a DisconnectType
    void handleClientInfo(int, ClientInfo*);
                                                              //   (the join orchestrator; several
                                                              //   callees are still declared-only)
    void flushDemoPackets(ServerConnection*, int);

    //  / dedicated PE 0x448840, 292 B. AutoRecorder::update calls it once a hook
    // has published a demo: it substitutes the file into sv.demoDownloadURL's "{file}" token and
    // broadcasts data block 9. [FRONTIER] until DemoAvailableInfo is recovered.
    void onAutoRecordingAvailable(const std::string& fileName, const std::string& mapName);
    void clientLoadComplete(int);
    void clientSendPlayerDatabase(int);                       // handleClientInfo's tail
    void clientSendDatabase(int);
    void clientSendDatabaseComplete(int);
                                                              //   the client's DB ack: content gate,
                                                              //   state 4, the two proceed events
    // THE parameter IS A connection ID used AS A player ID - createPlayer gives a new player the
    // id of the connection it arrived on. 31 bytes: a forward to playerManager->getPlayerFromId.
    world::IPlayer* getPlayer(int id) const;

    // the disconnect half of processReceivedPackets' deferred teardown.
    void closeClientConnection(int connectionId, unsigned int reason, const std::string& text);

    bool getConnectionIpAndPort(int playerId, std::string& out);
    IClientConnection* getClient(int connectionId) const;     // const
                                                              // Send helpers consume the event reference. resetGhost refreshes existing networkables on connections past state 2.
    void _sendGameEventToAll(GameEvent* event, bool connectedOnly, bool playingOnly);
    void _sendGameEvent(unsigned int connectionId, GameEvent* event, bool connectedOnly,
                        bool playingOnly);
    void resetGhost(io::NetworkableDescriptor* descriptor);
    bool LoadLevelFingerprint();                              // this batch
    bool loadStdArchiveFingerprints(const std::string&, std::map<int, std::string>&);
    bool loadMiscFingerprints();
                                                              // [sic: capital L, as in the symbol]

    // the frame loop - update()'s callees, in call order
    // Every method below has a current body. Remaining narrow collaborator frontiers are documented
    // at their exact call sites in GameServer.cpp.
    void processLocalPlayersInputs(int steps);                // the local-human-player path
    void processReceivedPackets();
    void processEnqueuedRadioMessages();                      // written
    void removeAllEnquedRadioMessages();                      //  [sic: Enqued] - written
    void updateStateMasks();                                  // written (per-connection split)
    void updateConnections(float dt);
    void processGameStateAndSendPackets(float dt);            // written in full
    bool isRecordingDemo() const;                             // written
    void clearPlayerActions();                                // written (per-player split)
    void destroyPlayers();                                    // written
    bool simulateFrame(float dt);                             // THE PER-frame simulation order, written
    bool simulatePlayersUpdate(float dt);                     // written (per-player body split)
    bool simulatePlayerUpdate(world::IPlayer* player, float dt);      // written
    void simulatePlayersPhysics(float dt);                    // written
    // returns bool - all four exits pass through one epilogue that does `mov eax, 1`, so it
    // always answers true. Was declared void; the caller ignores it either way.
    bool simulatePlayerPhysics(world::IPlayer* player, float dt);     // written
    void simulatePlayersCollisions(float dt);                 // written
    // void, unlike ITS twin - its early exits materialise no return value at all and the live
    // path is a tail call, which is only consistent with void. The asymmetry is recorded, not
    // explained; see the bodies.
    void simulatePlayerCollisions(world::IPlayer* player, float dt);  // written
    void updatePortals(float dt);                             // written
    // recovered logical field order
    // Offsets are the retail ELF layout. Native STL sizes make rebuilt MSVC offsets different;
    // source code must use these members, never hard-code the comments as offsets.
    int     m_refCount;                                       // +0x58 = 1
    std::list<world::IPlayer*> m_playersToDestroy;            // +0x5C
    int     m_frameCount;                                     // +0x60 = 0
    io::NetServer* m_netServer;                               // +0x64 = 0
    std::map<int, DisconnectType> m_disconnectTypes;          // +0x68
    std::map<unsigned char, IClientConnection*> m_connections;// +0x74
    int     m_numPlayers;                                     // +0x80 = 0
    // Maximum connection-type index; requests above it are clamped to this value.
    int     m_maxConnectionType;                              // +0x84 = 5
    int     m_bandwidthChoke;                                 // +0x88 = 0
    int     m_currentInputIndex;                              // +0x8C deliberately uninitialized
    int     m_svStartTime;                                    // +0x90 = 0
    int     m_port;                                           // +0x94 deliberately uninitialized
    double  m_initTime;                                       // +0x98 deliberately uninitialized; init writes wall clock
    int     m_loadState;                                      // +0xA0 = 0 (idle)
    io::BitStream* m_bitStream;                               // +0xA4 = 0
    float   m_messageDistanceSqA;                             // +0xA8 = 10000
    float   m_messageDistanceSqB;                             // +0xAC = 225
    float   m_messageDistanceSqC;                             // +0xB0 = 10000
    float   m_messageDistanceSqD;                             // +0xB4 = 10000
    bool    m_sampleStreamManagerReadStats;                   // +0xB8 = false
    io::BasicNetworkStats* m_networkableStats;                // +0xBC; owned, allocated after event registration
    int     m_mapListSendInterval;                            // +0xC0 = 900
    int     m_mapListSendCounter;                             // +0xC4 = 0
    int     m_serverInfoSendInterval;                         // +0xC8 = 30
    int     m_serverInfoSendCounter;                          // +0xCC = 0
    DemoRecorder* m_demoRecorder;                             // +0xD0 = 0
    MatchMakingServer* m_matchMakingServer;                   // +0xD4 = 0
    IStatsManager* m_statsManager;                            // +0xD8 = 0
    std::set<HTTPRequest*> m_httpRequests;                    // +0xDC
    std::list<HTTPRequest*> m_httpRequestsToRemove;           // +0xE8
    IBase* m_currentPersistenceObject;                        // +0xEC = 0
    int     m_httpUpdateCounter;                              // +0xF0 = 0
    int     m_httpUpdateInterval;                             // +0xF4 = 30
    bool    m_shutdownComplete;                               // +0xF8 = false
    AutoRecorder* m_autoRecorder;                             // +0xFC = 0
    int     m_numberOfDroppedFrames;                          // +0x100 = 0
    int     m_numberOvershotFrames;                           // +0x104 = 0
    bool    m_isInternetServer;                               // +0x108 = false
    bool    m_initComplete;                                   // +0x109 = false
    StatsMonitor* m_statsMonitor;                             // +0x10C, one-byte empty object
    std::string m_miscHashMd5;                               // +0x110
    std::map<int, std::string> m_stdArchiveMd5;               // +0x114
    std::map<int, std::string> m_stdArchiveModMd5;            // +0x120
    std::map<int, std::string> m_bstArchiveMd5;               // +0x12C
    std::map<int, std::string> m_bstArchiveModMd5;            // +0x138
    std::map<int, std::string> m_levelFingerprintStrings;     // +0x144
    bool    m_useContentChecking;                             // +0x150 = false
    unsigned char m_levelFingerprint;                         // +0x151 = 0
    std::list<EventNode*> m_radioMessages;                    // +0x154
    int     m_radioMessagesPerTick;                           // +0x158 = 2
    bool    m_isPunkBusterEnabled;                            // +0x15C deliberately uninitialized until init
    bool    m_isWaitingForClients;                            // +0x15D = false
    unsigned int m_clientAckedEndOfRoundGuard;                // +0x160 = 0
    IPlayerUnlockInfo* m_playerUnlockInfo;                    // +0x164 = 0
    bool    m_pythonValid[2];                                 // +0x168/+0x169 = false
};

} // namespace hfe
} // namespace dice

#endif
