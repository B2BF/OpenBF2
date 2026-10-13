// One voice server instance: the BF-server transport, authentication and routing handlers.
//
// CBF2VoipServerInstance derives from VOIP::CVoipRouterInstance and overrides the transport
// virtuals (Send, Receive) and the packet pump with UDP and BF-server authentication. The virtual
// declaration order mirrors the recovered 31-slot instance vtable (fh2 0xbb7220): the base declares
// slots 0..14, this class overrides 0/1/2/9/10/11/12/14 and appends slots 15..30. Start() (0-arg)
// stays the inherited base routine; the four-argument Start is a separate non-virtual method.
//
// Member offsets are the recovered x86-64 ELF instance (4776 bytes in CreateInstance's operator new).
// Fields whose meaning is still unread are marked [UNRESOLVED].

#ifndef BF2_VOIPSERVER_VOIPSERVERINSTANCE_H   // [INFERRED] guard style
#define BF2_VOIPSERVER_VOIPSERVERINSTANCE_H

#include <deque>
#include <map>
#include <string>

#include "../Core/Timer.h"
#include "VoipClients.h"
#include "VoipDebug.h"
#include "VoipPackets.h"
#include "VoipResult.h"
#include "VoipRouter.h"

namespace dice { namespace hfe { namespace io { class UDPSocket; } } }

namespace BF2VoipServer {

class CBF2VoipServerInstance : public VOIP::CVoipRouterInstance
{
public:
    CBF2VoipServerInstance();
    virtual ~CBF2VoipServerInstance();

    // ---- VOIP::CVoipRouterInstance overrides ----
    virtual VOIP::VRESULT Send(bool loopback, const VOIP::ClientData* from, const VOIP::ClientData* to, char* data, int length);
    virtual VOIP::VRESULT Receive(unsigned& packetType, unsigned short& sessionId, char* buffer, int bufferSize, int& length);
    virtual VOIP::VRESULT HandleIncomingPacket(unsigned char packetType, unsigned int connectionId, unsigned short clientId, char* data, int length);
    virtual VOIP::VRESULT RemoveClient(unsigned short sessionId);
    virtual VOIP::VRESULT RemoveAllClients();
    virtual VOIP::VRESULT SetClientData(const VOIP::ClientData& data);
    virtual VOIP::VRESULT Update();
    virtual VOIP::VRESULT Stop();
    // GetClientDataMap .. BF2ServerPacketAuthentication are appended virtuals (slots 15..30).

    virtual std::map<unsigned short, VOIP::ClientData>* GetClientDataMap();
    virtual int GetNumClients();
    virtual int GetListenPort();
    virtual int GetSendToClientPort();
    virtual const char* GetVoipConnectionPlayerNameFromID(int playerId);
    virtual std::string MapPacketTypeToName(unsigned char packetType);
    virtual std::string MapChannelToName(int channel);
    virtual double GetAuthenticBFServerPacketElapsedTime();
    virtual int GetBFServerPort();
    virtual unsigned int GetBFServerIPAddr();
    virtual std::string popOutputString();
    virtual void pushOutputString(std::string text);
    virtual VOIP::VRESULT ExtractServerSessionId(char* data, int& length, VOIP::DataString& out);
    virtual VOIP::VRESULT InitChallengeString();
    virtual VOIP::VRESULT SendChallengeString(unsigned int a, unsigned short b);
    virtual bool BF2ServerPacketAuthentication(unsigned char packetType, unsigned int connectionId, unsigned short clientId, char* data, int& length);

    // ---- non-virtual members ----
    // [CHOSEN, not recovered] The facade reads this id by raw offset to find an instance to release.
    int GetInstanceId() const;

    // Binds the UDP socket on listenPort, records sendPort and the shared password, seeds the
    // challenge string and starts routing. VRESULT_FAILURE for a zero port, VRESULT -5 when the
    // socket cannot listen.
    VOIP::VRESULT Start(unsigned int listenPort, unsigned int sendPort, std::string password, int instanceId);

    // Clears the BF-server connection fields (port, address and three trailing flags).
    VOIP::VRESULT NullifyConnection();

    VOIP::VRESULT HandlePacketBF2RemovePlayer(const BF2VoipPacketData::RemovePlayerData& data);
    VOIP::VRESULT HandlePacketBF2RemovePlayer(const BF2VoipPacketData::RoutingDataPlayer& data, int index);
    VOIP::VRESULT HandlePacketBF2SetPlayerData(const BF2VoipPacketData::SetPlayerData& data);
    VOIP::VRESULT HandlePacketBF2SetPlayerData(const BF2VoipPacketData::RoutingDataPlayer& data, unsigned short index);
    VOIP::VRESULT HandlePacketBF2ResetRouting();
    VOIP::VRESULT HandlePacketBF2FullRoutingDataSet(const char* data, int length);
    VOIP::VRESULT HandlePacketDebugRoutingData(const char* data, int length);

private:
    std::string              m_password;              // +0x880  shared secret from Start
    std::deque<std::string>  m_outputStrings;         // +0x888  push/popOutputString queue
    VOIP::DataString         m_field08D8;             // +0x8D8  [UNRESOLVED]
    VOIP::DataString         m_challengeString;       // +0x948  seeded by Start
    VOIP::DataString         m_field09B8;             // +0x9B8  [UNRESOLVED]
    VOIP::DataString         m_versionString;         // +0xA28  ctor fills 20 spaces
    int                      m_instanceId;            // +0xA98  ctor -1
    int                      m_listenPort;            // +0xA9C
    int                      m_sendToClientPort;      // +0xAA0
    int                      m_bfServerPort;          // +0xAA4  cleared by NullifyConnection
    unsigned int             m_bfServerIPAddr;        // +0xAA8  in_addr; cleared by NullifyConnection
    BF2VoipPacketData::RoutingDataPlayer m_routing[65];  // +0xAAC  260 bytes; slot 64 = overflow
    dice::hfe::io::UDPSocket* m_socket;               // +0xBB0  created by Start
    char                     m_playerNames[65][25];   // +0xBB8  25-byte name records, slot 64 = server
    dice::hfe::Timer         m_timer1;                // +0x1218
    dice::hfe::Timer         m_timer2;                // +0x1248  authentication timer
    int                      m_connectionFlag0;       // +0x1278  [UNRESOLVED]; cleared by NullifyConnection
    int                      m_connectionFlag1;       // +0x127C  [UNRESOLVED]; cleared by NullifyConnection
    int                      m_connectionFlag2;       // +0x1280  [UNRESOLVED]; cleared by NullifyConnection
    CDebugRoutingState       m_debugRouting;          // +0x1288
    CDebugVoiceTrace         m_debugVoiceTrace;       // +0x1298
};

} // namespace BF2VoipServer

#endif // BF2_VOIPSERVER_VOIPSERVERINSTANCE_H
