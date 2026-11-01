// Client session lifecycle, connection state, world stepping and prediction.
// [INFERRED] Header beside Game/GameClient/GameClient.cpp, attested by PE __FILE__.
// Recovered from the retail 1.50 client; dedicated images do not contain this class.
// Live [FRONTIER] stops remain at unrecovered calls; preserve interface declaration order.

#ifndef GAME_GAMECLIENT_GAMECLIENT_H     // [INFERRED] guard style
#define GAME_GAMECLIENT_GAMECLIENT_H

#include <string>

#include "../Main/Game.h"
#include "../IGameClient.h"
#include "../IEventListener.h"
#include "../../IO/ChecksumContext.h"     // ChecksumContext storage.

namespace dice {
namespace hfe {

class ClientConnection;                          // Game/Common/ClientConnection.h
class StatsMonitor;                              // Game/Main/StatsMonitor.h
class DemoRecorder;                              // Game/DemoRecorder/DemoRecorder.h
class PlayerAction;                              // Game/Common/PlayerAction.h - queueLocalAction
namespace io {
    class BitStream;
    class BasicNetworkStats;
    class NetClient;                             // [FRONTIER] IO/Network/ProtocolLayer/NetClient.cpp is
                                                 // attested and unrecovered - declaration only here
}

class GameClient : public Game, public IGameClient, public IEventListener
{
public:
    GameClient();                                          // named in the PE
    virtual ~GameClient();
    static IBase* create(unsigned int iid, IBase* outer);  // the class-table factory

    // the IBase quartet (shared bodies with GameServer: 4D2B10 / 6BBD40 / 4D2B20 / 4D2A70) --
    virtual void  addRef();
    virtual int   getRef();
    virtual int   release();
    virtual void* queryInterface(unsigned int iid) const;

    // IGame slots this class overrides (Game/IGame.h order; the rest resolve to Game::)
    virtual bool  init();                                  // `return true`
    virtual void  shutdown();
    virtual bool  load();
    virtual bool  unload();                                // `return false`
    virtual StatsMonitor* getStatsMonitor() const;         // the +0x14C byte
    virtual bool  update(int steps, float dt);
    virtual bool  say(const std::string& text);
    virtual bool  radioMessage(TargetChannel, const std::string&, world::IPlayer*,
                               unsigned int, bool, bool);
    virtual bool  radioMessageFromPlayer(TargetChannel, const std::string&, unsigned int,
                                         unsigned int, bool, bool);
    virtual bool  gameInfoMessage(TargetChannel, const std::string&, unsigned int,
                                  unsigned int, bool);
    virtual int   getCurrentInputIndex();                  // +0xCC
    virtual void  confirmGhost(int, unsigned int);
    virtual void  confirmReset(int);                       // empty
    virtual world::IObject* createObjectOnAllClients(const world::IObjectTemplate*, Math::Vec3,
                                                     Math::Vec3, bool, int, int);  // `return 0`
    virtual void  sendGameEventToAllPlaying(GameEvent*);
    virtual void  sendGameEventIfPlaying(unsigned int, GameEvent*);
    virtual void  sendGameEventToAll(GameEvent*);          //  (folded)
    virtual void  sendGameEvent(unsigned int, GameEvent*); //  (folded)
    virtual void  sendGameEventToAllConnected(GameEvent*); //  (folded)
    virtual void  sendGameEventIfConnected(unsigned int, GameEvent*);  //  (folded)
    virtual void  sendMissileMessageToOccupyingPlayers(world::IObject*,
                                                       world::MissileLockMessage);   // empty
    virtual bool  enterVehicle(world::IPlayer*, world::IObject*, bool);
    virtual bool  exitVehicle(world::IPlayer*, bool, bool);
    virtual std::string listPlayers() const;
    virtual void  recordDemo(DemoRecorder*);               // the console refusal
    virtual void  stopRecording();                         // empty
    virtual bool  isPlayingDemo();
    virtual void  setSimulationRate(int rate);
    virtual int   getSimulationRate();                     // g_system's, as GameServer
    virtual void  debugShowActiveGhosts(int);              // empty
    virtual const io::BasicNetworkStats* getNetworkStats() const;   // +0x13C
    virtual void  sendUDPPacket(const char* address, unsigned short port, void* data,
                                int size);                 // ONE body for both tables
    virtual void  addTextToChatWindow(const char* text);
    virtual bool  isPunkBusterEnabled() const;             // +0x159
    virtual void  destroyPlayer(world::IPlayer* player);   // Game's pure slot

    // GameClient's ONE own primary virtual, +0x120
    virtual void  requestDisconnect(int reason);           //   [INFERRED] name

    // IGameClient (Game/IGameClient.h order)
    virtual bool handleControlObjectState(io::BitStream*, unsigned short, int, unsigned int, float);
    virtual bool playDemo(DemoRecorder* recorder);
    virtual void handleChallenge(ChallengeEvent* event);
    virtual int  joinServer(const std::string& address, int port, const std::string& password,
                            float timeoutSeconds, bool punkBuster);
    virtual IClientConnection* getConnection();
    virtual bool createObject(unsigned int templateId, unsigned short networkableId,
                              const Vec3* position, const Vec3* rotation);
    virtual bool createKit(unsigned int templateId, unsigned short networkableId,
                           unsigned int creationIndex, int unlockLevel,
                           const Vec3* position, const Vec3& rotation);
    virtual bool destroyPlayer(int playerId);
    virtual bool destroyObject(unsigned short networkableId);
    virtual bool enterVehicle(int playerId, unsigned short networkableId, bool setAsDefaultVehicle);
    virtual bool exitVehicle(int playerId, bool exitPlayerInstead);
    virtual bool getConnectionStatus(io::ConnectionStatus& out);
    virtual std::string getServerAddress();
    virtual void slot44(int);
    virtual void restartDemo();
    virtual void selectDemoPlayer(bool next);
    virtual void nextDemoCamera();
    virtual int  getCurrentDemoPlayerId();
    virtual bool getIsDemoFreeFlying();
    virtual void togglePause();
    virtual void setPause(bool paused);
    virtual bool getPause();
    virtual bool isDemoAtEnd();
    virtual void slot6C(int a, int b);
    virtual bool getDisconnectRequested();
    virtual bool getIsReady();
    virtual std::string getServerAddressCopy();
    // (IGameClient::sendUDPPacket has the same signature as IGame's; one body serves both tables.)

    // IEventListener
    virtual void handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data);   // named

    // the non-virtual helpers the bodies call (all in the GameClient TU, 4D1C10..4D6DFF)
    void handleDataBlock(EventNode* node, void* data);     // named
    int  disconnect();                                     //   [INFERRED] name
    bool receive(float dt);                                //  [INFERRED] name - the NetClient
                                                           //   message loop and the SwiffHost error mapping
    void transmit(int steps, double delta);                //   [INFERRED] name
    bool connectionWatchdog(double delta);                 //   [INFERRED] name
    void setConnectionType(int type);                      //   [INFERRED] name
    // [INFERRED] Shared object/kit creation helper handling reuse, transforms, and
    // network registration. Retains member calling convention despite not reading this.
    world::IObject* createNetworkedObject(const world::IObjectTemplate* objectTemplate,
                                          unsigned short networkableId, const Vec3* position,
                                          const Vec3* rotation);
    // . [INFERRED] name. One simulation step of the local player's input:
    // take the queued sample, round-trip it through a PlayerAction, predict with it,
    // and queue it for sending.
    bool sampleLocalInput(float dt);
    // . [INFERRED] name. Stamp the sampled action into the player's action
    // buffer and hand the stored copy to the connection's PlayerActionManager to send.
    void queueLocalAction(world::IPlayer* player, PlayerAction& action);
    // . [INFERRED] name. Once per step: predict() every ghost-active remote
    // object, skipping the ones the local player is driving.
    bool predictRemoteObjects(float dt);
    // . [INFERRED] name. The tail of stepWorld: the object grid, the response-physics
    // reset, and - only while client-side prediction is on - the local vehicle's collision step
    // and the position latch at ds:0x9ED5D8.
    bool stepLocalPrediction(float dt);
    bool stepWorld(float dt);                              //   [INFERRED] name - shared with
                                                           //   GameServer's TU neighbourhood, this is ITS entry

private:
    int                    m_refCount;               // +0x0B0
    int                    m_connectionState;        // +0x0B4
    int                    m_field0B8;               // +0x0B8
    int                    m_connectionType;         // +0x0BC
    ClientConnection*      m_connection;             // +0x0C0
    io::NetClient*         m_netClient;              // +0x0C4
    int                    m_port;                   // +0x0C8
    int                    m_currentInputIndex;      // +0x0CC
    std::string            m_stringD0;               // +0x0D0
    float                  m_floatEC;                // +0x0EC
    int                    m_frameCount;             // +0x0F0
    int                    m_fieldF4;                // +0x0F4
    int                    m_fieldF8;                // +0x0F8
    bool                   m_isRunning;              // +0x0FC
    bool                   m_useClientSidePrediction;// +0x0FD
    bool                   m_byteFE;                 // +0x0FE
    float                  m_float100;               // +0x100
    std::string            m_serverAddress;          // +0x104
    bool                   m_disconnectRequested;    // +0x120
    int                    m_disconnectReason;       // +0x124
    bool                   m_byte128;                // +0x128
    bool                   m_byte129;                // +0x129
    bool                   m_byte12A;                // +0x12A
    bool                   m_byte12B;                // +0x12B
    bool                   m_connectionProblem;      // +0x12C
    bool                   m_showNetGraph;           // +0x12D
    void*                  m_field134;               // +0x134
    io::BitStream*         m_bitStream;              // +0x138
    io::BasicNetworkStats* m_networkableStats;       // +0x13C
    DemoRecorder*          m_demoRecorder;           // +0x140
    bool                   m_paused;                 // +0x144
    bool                   m_shutdownComplete;       // +0x145
    int                    m_catchUpSteps;           // +0x148
    StatsMonitor*          m_statsMonitor;           // +0x14C
    io::ChecksumContext*   m_checksumContext;        // +0x150  the 0x34-byte object
                                                     //   is io::ChecksumContext (ordinal at +0, three
                                                     //   MD5s at +4/+0x14/+0x24); handleDataBlock's
                                                     //   MapInfo arm seeds it and its load-success arm
                                                     //   runs both checksums into a ContentCheckEvent
    bool                   m_commanderEnabled;       // +0x154
    bool                   m_isReady;                // +0x158
    bool                   m_punkBuster;             // +0x159
};

} // namespace hfe
} // namespace dice

#endif // GAME_GAMECLIENT_GAMECLIENT_H
