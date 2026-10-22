// Client view of the game object, obtained through queryInterface. No virtual
// destructor belongs to this interface. Names are [INFERRED] where client symbols
// are absent. Keep differently named server-address slots: overloading would
// change MSVC's virtual ordering.

#ifndef GAME_IGAMECLIENT_H     // [INFERRED] guard style
#define GAME_IGAMECLIENT_H

#include <string>

#include "../Core/IBase.h"
#include "../Math/Vec3.h"

namespace dice {
namespace hfe {

class ChallengeEvent;                              // Game/GameEvents/ChallengeEvent.h
class DemoRecorder;                                // Game/DemoRecorder/DemoRecorder.h
class IClientConnection;                           // Game/Common/IClientConnection.h
namespace io {
    class  BitStream;
    struct ConnectionStatus;                       // IO/Network/ProtocolLayer/NetServer.h
}

class IGameClient : public IBase
{
public:
    // Replays local control-object prediction after a received ghost update.
    virtual bool handleControlObjectState(io::BitStream* stream, unsigned short networkableId,
                                          int actionBufferEntries, unsigned int tick, float period) = 0;
    // +0x14  Demo::playDemo's handoff (ELF 0x814167A `vcall [vptr+0x14]` with the recorder). PE 4D41C0.
    virtual bool playDemo(DemoRecorder* recorder) = 0;
    // +0x18  ChallengeEvent::executeClient hands the whole event over. PE 4D1CF0.
    virtual void handleChallenge(ChallengeEvent* event) = 0;
    // +0x1C  BF2Engine::startGame's client branch: (address, port, password, 5.0f, clPunkBuster),
    //        1 = joined, 2..9 the failure ids the branch maps onto SwiffHost message ids, 0 = other.
    virtual int  joinServer(const std::string& address, int port, const std::string& password,
                            float timeoutSeconds, bool punkBuster) = 0;
    // +0x20  the ClientConnection (null until joined). PE 659ED0 - `return this->+0x18`.
    virtual IClientConnection* getConnection() = 0;
    // +0x24  CreateObjectEvent / CreateObjectMultiEvent. PE 4D3FC0.
    virtual bool createObject(unsigned int templateId, unsigned short networkableId,
                              const Vec3* position, const Vec3* rotation) = 0;
    // +0x28  CreateKitEvent. PE 4D4090. creationIndex is not read by the PE body.
    virtual bool createKit(unsigned int templateId, unsigned short networkableId,
                           unsigned int creationIndex, int unlockLevel,
                           const Vec3* position, const Vec3& rotation) = 0;
    // +0x2C  DestroyPlayerEvent. PE 4D1F00.
    virtual bool destroyPlayer(int playerId) = 0;
    // +0x30  DestroyObjectEvent. PE 4D1F40 - always true.
    virtual bool destroyObject(unsigned short networkableId) = 0;
    // +0x34  EnterVehicleEvent. PE 4D6D20.
    virtual bool enterVehicle(int playerId, unsigned short networkableId, bool setAsDefaultVehicle) = 0;
    // +0x38  ExitVehicleEvent. PE 4D4180.
    virtual bool exitVehicle(int playerId, bool field0D) = 0;
    // +0x3C  BasicNetworkStats::updateClientStats's sampler: bool(ConnectionStatus&). PE 4D25D0.
    virtual bool getConnectionStatus(io::ConnectionStatus& out) = 0;
    // +0x40  the "host:port" string joinServer stored ("demo_playback" for a demo). PE 4D2820.
    virtual std::string getServerAddress() = 0;
    // +0x44  a one-argument no-op in the PE (4D2AF0 -> nullsub). [UNRESOLVED] role.
    virtual void slot44(int) = 0;
    // +0x48  demo playback restart (gameLogic reset / restartDemoPlayback, the recorder rewound). PE 4D2650.
    virtual void restartDemo() = 0;
    // +0x4C..+0x58: names [INFERRED] from the DemoRecorder methods they reach.
    // +0x4C  the next (true) or previous (false) player.
    virtual void selectDemoPlayer(bool next) = 0;
    // +0x50
    virtual void nextDemoCamera() = 0;
    // +0x54  -1 without a playing demo.
    virtual int  getCurrentDemoPlayerId() = 0;
    // +0x58
    virtual bool getIsDemoFreeFlying() = 0;
    // +0x5C  setPause(!getPause()). PE 4D2790.
    virtual void togglePause() = 0;
    // +0x60  BF2Engine's pause state machine (ELF 0x4038A0 / 0x403060 callers). PE 4D27B0.
    virtual void setPause(bool paused) = 0;
    // +0x64   - the +0x144 byte (comdat-folded with Game::getAbortLoadLevel).
    virtual bool getPause() = 0;
    // +0x68  true once the demo stream has run out. [INFERRED] name.
    virtual bool isDemoAtEnd() = 0;
    // +0x6C  a recorded demo handed to the profile (LocalProfileManager +0xA4, DemoLibrary
    //        refresh). PE 4D3D60. [UNRESOLVED] name and both int roles.
    virtual void slot6C(int a, int b) = 0;
    // +0x70  BF2Engine::mainLoop's pre-frame gate: true once requestDisconnect ran. PE 4D28B0.
    virtual bool getDisconnectRequested() = 0;
    // +0x74  EngineBase's client-only ghost hook: the +0x158 byte, set by ECNetwork event 4.
    //        . [INFERRED] role.
    virtual bool getIsReady() = 0;
    // +0x78  the same body as +0x40 (PE 4D2820, folded). [UNRESOLVED] which caller wants which.
    virtual std::string getServerAddressCopy() = 0;
    // +0x7C  forwards to IGame::sendUDPPacket. PE 4D2B00 -> 4D2840. Declared with IGame's exact
    //        signature so one GameClient body overrides both slots.
    virtual void sendUDPPacket(const char* address, unsigned short port, void* data, int size) = 0;
};

} // namespace hfe
} // namespace dice

#endif // GAME_IGAMECLIENT_H
