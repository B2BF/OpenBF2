// One voice server instance: the BF-server transport, authentication and routing handlers.
//
// Recovered from fh2 (CBF2VoipServerInstance 0x841430-0x844e50): construct/destruct,
// NullifyConnection, the getters, the client-table overrides, the name tables, the UDP transport
// (Start/Stop/Send/Receive/Update), the challenge/authentication handshake (MD5Digest/systemInfo)
// and the BF2 routing packet handlers. Only the debug-dump bodies (CDebug*) remain frontiers.

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "VoipServerInstance.h"
#include "../Core/SystemInfo.h"
#include "../External/GameSpy/md5.h"
#include "../IO/Network/NetworkLayer/Socket/UDPSocket.h"

#if defined(_WIN32)
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#endif

namespace BF2VoipServer {

// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: voip-instance] Named stop for a body that is not yet
// recovered. Remove each call when its real body lands (the transport, authentication or routing
// phase). Prefer stderr here because BF2Log may not be up on these paths.
#define BF2_VOIP_INSTANCE_FRONTIER(name)                                                 \
    do {                                                                                 \
        std::fprintf(stderr, "[FRONTIER] BF2VoipServer::%s reached\n", name);            \
        std::fflush(stderr);                                                             \
        std::abort();                                                                    \
    } while (0)

CBF2VoipServerInstance::CBF2VoipServerInstance()
{
    std::memset(&m_field08D8, 0, sizeof(m_field08D8));
    std::memset(&m_challengeString, 0, sizeof(m_challengeString));
    std::memset(&m_field09B8, 0, sizeof(m_field09B8));
    std::memset(&m_versionString, 0, sizeof(m_versionString));
    std::memset(m_routing, 0, sizeof(m_routing));
    std::memset(m_playerNames, 0, sizeof(m_playerNames));
    m_instanceId = -1;
    m_listenPort = 0;
    m_sendToClientPort = 0;
    m_bfServerPort = 0;
    m_bfServerIPAddr = 0;
    m_socket = 0;
    m_connectionFlag0 = 0;
    m_connectionFlag1 = 0;
    m_connectionFlag2 = 0;
    NullifyConnection();

    // The shipped constructor seeds the version string with twenty spaces.
    for (int i = 0; i < 20; ++i)
    {
        int count = m_versionString.m_count;
        if (count <= 0x63)
        {
            m_versionString.m_data[count] = ' ';
            m_versionString.m_count = count + 1;
            m_versionString.m_data[count + 1] = 0;
        }
    }
}

// Members are destroyed by the compiler in reverse declaration order (debug traces, then the two
// timers, the routing table, the socket pointer, the names and strings, then the router base).
CBF2VoipServerInstance::~CBF2VoipServerInstance()
{
}

// Clears the BF-server connection: port, address and the three trailing flags.
VOIP::VRESULT CBF2VoipServerInstance::NullifyConnection()
{
    m_bfServerPort = 0;
    m_bfServerIPAddr = 0;
    m_connectionFlag0 = 0;
    m_connectionFlag1 = 0;
    m_connectionFlag2 = 0;
    return VOIP::VRESULT_SUCCESS;
}

double CBF2VoipServerInstance::GetAuthenticBFServerPacketElapsedTime()
{
    m_timer2.update();
    return m_timer2.getTime();
}

int CBF2VoipServerInstance::GetNumClients()
{
    return static_cast<int>(m_clients.GetClientDataMap()->size());
}

std::map<unsigned short, VOIP::ClientData>* CBF2VoipServerInstance::GetClientDataMap()
{
    return m_clients.GetClientDataMap();
}

int CBF2VoipServerInstance::GetListenPort()
{
    return m_listenPort;
}

int CBF2VoipServerInstance::GetSendToClientPort()
{
    return m_sendToClientPort;
}

int CBF2VoipServerInstance::GetBFServerPort()
{
    return m_bfServerPort;
}

unsigned int CBF2VoipServerInstance::GetBFServerIPAddr()
{
    return m_bfServerIPAddr;
}

int CBF2VoipServerInstance::GetInstanceId() const
{
    return m_instanceId;
}

// Returns a 25-byte name record for the given slot, or null when debug routing is off or the slot
// is out of range. Slot 64 (player id -1 or 255) is the server record.
const char* CBF2VoipServerInstance::GetVoipConnectionPlayerNameFromID(int playerId)
{
    if (!m_debugRouting.Enabled())
        return 0;
    if (static_cast<unsigned int>(playerId) > 0x3F)
    {
        if (playerId == -1 || playerId == 255)
            return m_playerNames[64];
        return 0;
    }
    return m_playerNames[playerId];
}

// ---- client-table overrides ----

VOIP::VRESULT CBF2VoipServerInstance::RemoveClient(unsigned short sessionId)
{
    VOIP::VRESULT result = VOIP::CVoipRouterInstance::RemoveClient(sessionId);
    if (result == VOIP::VRESULT_SUCCESS)
    {
        char message[512];
        std::sprintf(message, "  Remove Client Session Id: %d", sessionId);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, message);
    }
    return result;
}

VOIP::VRESULT CBF2VoipServerInstance::SetClientData(const VOIP::ClientData& data)
{
    VOIP::VRESULT result = VOIP::CVoipRouterInstance::SetClientData(data);
    char message[536];
    if (result == VOIP::VRESULT_SUCCESS)
    {
        std::sprintf(message, "  Update Client Data - Client Id: %d  Session Id: %d",
                     data.m_clientId, data.m_sessionId);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, message);
    }
    else
    {
        std::sprintf(message, "  Update Client Data Failed! - Client Id: %d  Session Id: %d ",
                     data.m_clientId, data.m_sessionId);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, message);
    }
    return result;
}

VOIP::VRESULT CBF2VoipServerInstance::RemoveAllClients()
{
    VOIP::VRESULT result = VOIP::CVoipRouterInstance::RemoveAllClients();
    if (result == VOIP::VRESULT_SUCCESS)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "  Remove All Clients");
    }
    else
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, "  Remove All Clients Failed");
    }
    return result;
}

// ---- name tables ----

std::string CBF2VoipServerInstance::MapPacketTypeToName(unsigned char packetType)
{
    switch (packetType)
    {
    case 1:  return "VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_DEFAULT_CHANNEL_SET";
    case 2:  return "VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_ALTERNATE_CHANNEL_SET";
    case 3:  return "VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_MULTI_CHANNEL_SET";
    case 6:  return "VOIP_PACKET_SERVER_INCOMING_KEEP_ALIVE";
    case 100: return "BF2_VOIP_PACKET_SET_PLAYER_DATA";
    case 101: return "BF2_VOIP_PACKET_REMOVE_PLAYER_DATA";
    case 102: return "BF2_VOIP_PACKET_RESET_PLAYER_ROUTING";
    case 103: return "BF2_VOIP_PACKET_FULL_ROUTING_DATA_SET";
    case 200: return "BF2_VOIP_PACKET_DEBUG_DATA";
    default:  return "INVALID PACKET ID";
    }
}

std::string CBF2VoipServerInstance::MapChannelToName(int channel)
{
    switch (channel)
    {
    case 0:  return "CHANNEL_TEAM_1_COMMANDER";
    case 1:  return "CHANNEL_TEAM_1_SQUAD_A_LEADER";
    case 2:  return "CHANNEL_TEAM_1_SQUAD_B_LEADER";
    case 3:  return "CHANNEL_TEAM_1_SQUAD_C_LEADER";
    case 4:  return "CHANNEL_TEAM_1_SQUAD_D_LEADER";
    case 5:  return "CHANNEL_TEAM_1_SQUAD_E_LEADER";
    case 6:  return "CHANNEL_TEAM_1_SQUAD_F_LEADER";
    case 7:  return "CHANNEL_TEAM_1_SQUAD_G_LEADER";
    case 8:  return "CHANNEL_TEAM_1_SQUAD_H_LEADER";
    case 9:  return "CHANNEL_TEAM_1_SQUAD_I_LEADER";
    case 10: return "CHANNEL_TEAM_1_SQUAD_A_MEMBER";
    case 11: return "CHANNEL_TEAM_1_SQUAD_B_MEMBER";
    case 12: return "CHANNEL_TEAM_1_SQUAD_C_MEMBER";
    case 13: return "CHANNEL_TEAM_1_SQUAD_D_MEMBER";
    case 14: return "CHANNEL_TEAM_1_SQUAD_E_MEMBER";
    case 15: return "CHANNEL_TEAM_1_SQUAD_F_MEMBER";
    case 16: return "CHANNEL_TEAM_1_SQUAD_G_MEMBER";
    case 17: return "CHANNEL_TEAM_1_SQUAD_H_MEMBER";
    case 18: return "CHANNEL_TEAM_1_SQUAD_I_MEMBER";
    case 19: return "CHANNEL_TEAM_2_COMMANDER";
    case 20: return "CHANNEL_TEAM_2_SQUAD_A_LEADER";
    case 21: return "CHANNEL_TEAM_2_SQUAD_B_LEADER";
    case 22: return "CHANNEL_TEAM_2_SQUAD_C_LEADER";
    case 23: return "CHANNEL_TEAM_2_SQUAD_D_LEADER";
    case 24: return "CHANNEL_TEAM_2_SQUAD_E_LEADER";
    case 25: return "CHANNEL_TEAM_2_SQUAD_F_LEADER";
    case 26: return "CHANNEL_TEAM_2_SQUAD_G_LEADER";
    case 27: return "CHANNEL_TEAM_2_SQUAD_H_LEADER";
    case 28: return "CHANNEL_TEAM_2_SQUAD_I_LEADER";
    case 29: return "CHANNEL_TEAM_2_SQUAD_A_MEMBER";
    case 30: return "CHANNEL_TEAM_2_SQUAD_B_MEMBER";
    case 31: return "CHANNEL_TEAM_2_SQUAD_C_MEMBER";
    case 32: return "CHANNEL_TEAM_2_SQUAD_D_MEMBER";
    case 33: return "CHANNEL_TEAM_2_SQUAD_E_MEMBER";
    case 34: return "CHANNEL_TEAM_2_SQUAD_F_MEMBER";
    case 35: return "CHANNEL_TEAM_2_SQUAD_G_MEMBER";
    case 36: return "CHANNEL_TEAM_2_SQUAD_H_MEMBER";
    case 37: return "CHANNEL_TEAM_2_SQUAD_I_MEMBER";
    default: return "INVALID CHANNEL";
    }
}

// Pops a 20-byte server session id from the tail into out and stores 20 in its count.
VOIP::VRESULT CBF2VoipServerInstance::ExtractServerSessionId(char* data, int& length, VOIP::DataString& out)
{
    if (length < out.m_count)
        return VOIP::VRESULT_FAILURE;
    int index = length - 20;
    length = index;
    out.m_count = 20;
    char* source = data + index;
    std::memcpy(out.m_data, source, 16);
    out.m_data[20] = 0;
    std::memcpy(out.m_data + 16, source + 16, 4);
    return VOIP::VRESULT_SUCCESS;
}

// ---- transport ----

VOIP::VRESULT CBF2VoipServerInstance::Start(unsigned int listenPort, unsigned int sendPort, std::string password, int instanceId)
{
    VOIP::CVoipRouterInstance::Start();
    m_listenPort = static_cast<int>(listenPort);
    m_sendToClientPort = static_cast<int>(sendPort);
    if (!listenPort)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, "Listen on port value is zero");
        return VOIP::VRESULT_FAILURE;
    }
    if (!sendPort)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, "Send to Port value is zero");
        return VOIP::VRESULT_FAILURE;
    }
    m_password = password;
    m_instanceId = instanceId;

    m_socket = new dice::hfe::io::UDPSocket();
    if (!m_socket)
    {
        char message[256];
        int error = errno;
        errno = 0;
        std::sprintf(message, "create socket failed - Last Error: %d  ", error);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, message);
        return static_cast<VOIP::VRESULT>(-5);
    }

    char addressAndPort[64];
    std::sprintf(addressAndPort, ":%d", m_listenPort);
    if (!m_socket->listen(std::string(addressAndPort), false))
    {
        char message[256];
        int error = errno;
        errno = 0;
        std::sprintf(message, "Socket listen failed - Last Error: %d  ", error);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, message);
        return static_cast<VOIP::VRESULT>(-5);
    }

    InitChallengeString();
    m_timer2.reset(28.0);
    m_timer1.reset(0.0);

    char started[256];
    std::sprintf(started, "Instance %d Started  Port: %d  BF2 ", m_instanceId, m_listenPort);
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_INFO, started);

    char challenge[256];
    std::sprintf(challenge, "%06d%06d%06d%06d", 1, 5, 3153, 0);
    std::size_t challengeLength = std::strlen(challenge);
    if (challengeLength <= 0x63)
    {
        m_challengeString.m_count = static_cast<int>(challengeLength);
        std::memcpy(m_challengeString.m_data, challenge, challengeLength);
        m_challengeString.m_data[challengeLength] = 0;
    }
    return VOIP::VRESULT_SUCCESS;
}

VOIP::VRESULT CBF2VoipServerInstance::Stop()
{
    if (m_socket)
    {
        m_socket->close();
        delete m_socket;
        m_socket = 0;
    }
    if (VOIP::CVoipRouterInstance::Stop() == VOIP::VRESULT_SUCCESS)
        return VOIP::VRESULT_SUCCESS;
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, "Failed to Stop instance");
    return VOIP::VRESULT_INVALID_STATE;
}

VOIP::VRESULT CBF2VoipServerInstance::Update()
{
    VOIP::CVoipRouterInstance::Update();
    if (m_bfServerIPAddr && GetAuthenticBFServerPacketElapsedTime() > 28.0)
    {
        int port = m_bfServerPort;
        struct in_addr address;
        address.s_addr = m_bfServerIPAddr;
        char* ip = inet_ntoa(address);
        char message[256];
        std::sprintf(message, "BF2 server %s:%d disconnected", ip, port);
        pushOutputString(std::string(message));
        NullifyConnection();
    }
    return VOIP::VRESULT_SUCCESS;
}

VOIP::VRESULT CBF2VoipServerInstance::Send(bool loopback, const VOIP::ClientData* from, const VOIP::ClientData* to, char* data, int length)
{
    dice::hfe::io::ISocket::Address address;
    std::memset(&address, 0, sizeof(address));
    *reinterpret_cast<unsigned short*>(&address.sockaddr[0]) = 2;                       // AF_INET
    unsigned short port = to->m_clientId;
    *reinterpret_cast<unsigned short*>(&address.sockaddr[2]) =
        static_cast<unsigned short>((port >> 8) | (port << 8));                          // host -> network order
    *reinterpret_cast<unsigned int*>(&address.sockaddr[4]) = to->m_connectionId;
    address.length = 16;

    m_socket->setPeerAddress(address);
    if (m_socket->send(reinterpret_cast<const unsigned char*>(data), length))
    {
        if (m_debugRouting.Enabled() && loopback)
        {
            const char* toName = GetVoipConnectionPlayerNameFromID(to->m_playerId);
            const char* fromName = GetVoipConnectionPlayerNameFromID(from->m_playerId);
            m_debugVoiceTrace.Output(from->m_connectionId, to->m_connectionId, fromName, toName, data, length);
        }
        return VOIP::VRESULT_SUCCESS;
    }

    char message[256];
    int error = errno;
    errno = 0;
    std::sprintf(message, "sendto on socket failed - Last Error: %d ", error);
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, message);
    return VOIP::VRESULT_INVALID_STATE;
}

VOIP::VRESULT CBF2VoipServerInstance::Receive(unsigned& packetType, unsigned short& sessionId, char* buffer, int bufferSize, int& length)
{
    int received = m_socket->receiveFrom(reinterpret_cast<unsigned char*>(buffer), bufferSize);
    const dice::hfe::io::ISocket::Address& source = m_socket->getPeerAddress();
    if (received == -1)
    {
        int error = errno;
        errno = 0;
        if (error == 11 || error == 32)      // EAGAIN / EWOULDBLOCK: no datagram waiting
        {
            length = 0;
            return VOIP::VRESULT_SUCCESS;
        }
        char message[256];
        std::sprintf(message, "recvfrom on socket failed - Last Error: %d", error);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, message);
        return static_cast<VOIP::VRESULT>(-5);
    }

    length = received;
    packetType = *reinterpret_cast<const unsigned int*>(&source.sockaddr[4]);
    unsigned short rawPort = *reinterpret_cast<const unsigned short*>(&source.sockaddr[2]);
    sessionId = static_cast<unsigned short>((rawPort >> 8) | (rawPort << 8));             // network -> host order
    return VOIP::VRESULT_SUCCESS;
}

// ---- authentication and dispatch ----

VOIP::VRESULT CBF2VoipServerInstance::HandleIncomingPacket(unsigned char packetType, unsigned int connectionId, unsigned short clientId, char* data, int length)
{
    int remaining = length;
    if (!BF2ServerPacketAuthentication(packetType, connectionId, clientId, data, remaining))
        return VOIP::VRESULT_AUTH_FAILED;

    if (packetType == 101)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "");
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "BEGIN Handling Packet Remove Player Data");
        BF2VoipPacketData::RemovePlayerData removeData;
        removeData.m_sessionId = 0;
        if (data)
            removeData.m_sessionId = *reinterpret_cast<unsigned short*>(data);
        HandlePacketBF2RemovePlayer(removeData);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "END Handling Packet Remove Player Data");
        m_debugRouting.OutputFullState(this, 0x65);
        return VOIP::VRESULT_SUCCESS;
    }

    if (packetType > 0x65)
    {
        if (packetType == 103)
        {
            HandlePacketBF2FullRoutingDataSet(data, remaining);
            return VOIP::VRESULT_SUCCESS;
        }
        if (packetType < 0x67)
        {
            CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "");
            CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "BEGIN Handling Packet Reset Player Routing");
            HandlePacketBF2ResetRouting();
            CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "END Handling Packet Reset Player Routing");
            m_debugRouting.OutputFullState(this, 0x66);
            return VOIP::VRESULT_SUCCESS;
        }
        if (packetType == 0xC8)
        {
            HandlePacketDebugRoutingData(data, remaining);
            return VOIP::VRESULT_SUCCESS;
        }
        return VOIP::CVoipRouterInstance::HandleIncomingPacket(packetType, connectionId, clientId, data, remaining);
    }

    if (packetType == 5)
    {
        if (m_debugRouting.Enabled())
            m_debugVoiceTrace.Stop(connectionId);
        return VOIP::VRESULT_SUCCESS;
    }

    if (packetType != 100)
    {
        if (packetType != 4)
            return VOIP::CVoipRouterInstance::HandleIncomingPacket(packetType, connectionId, clientId, data, remaining);

        if (m_debugRouting.Enabled() && remaining == 2)
        {
            unsigned short sessionId = 0;
            ExtractClientSessionId(data, remaining, sessionId);
            VOIP::ClientData* client = m_clients.GetClientData(sessionId);
            if (client)
            {
                const char* name = GetVoipConnectionPlayerNameFromID(client->m_playerId);
                if (name)
                {
                    m_debugVoiceTrace.Start(name, client->m_playerId, connectionId);
                    return VOIP::VRESULT_SUCCESS;
                }
            }
        }
        return VOIP::VRESULT_SUCCESS;
    }

    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "");
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "BEGIN Handling Packet Set Player Data");
    BF2VoipPacketData::SetPlayerData setData;
    std::memset(&setData, 0, sizeof(setData));
    if (data)
        std::memcpy(&setData, data, 8);
    HandlePacketBF2SetPlayerData(setData);
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "END Handling Packet Set Player Data");
    m_debugRouting.OutputFullState(this, 0x64);
    return VOIP::VRESULT_SUCCESS;
}

// Gates every packet: voice frames and keep-alives pass through; BF-server packets (100..103, 200)
// must carry the session id and version the challenge handshake established. Returns whether the
// caller may proceed; packetType < 100 returns true for the six voice/keep-alive ids.
bool CBF2VoipServerInstance::BF2ServerPacketAuthentication(unsigned char packetType, unsigned int connectionId, unsigned short clientId, char* data, int& length)
{
    if (packetType > 0x67u)
    {
        if (packetType != 0xC8u)
            return false;
    }
    else if (packetType < 0x64u)
    {
        return static_cast<unsigned char>(packetType - 1) <= 5u;
    }

    m_timer2.update();
    m_timer1.update();
    if (m_timer1.getTime() > 28.0)
    {
        InitChallengeString();
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "BF2 Server Connection Timed Out");
    }
    m_timer1.reset(0.0);

    VOIP::DataString serverSession;
    std::memset(&serverSession, 0, sizeof(serverSession));
    if (ExtractServerSessionId(data, length, serverSession) != VOIP::VRESULT_SUCCESS)
    {
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "Failed extracting server session string");
        return false;
    }

    if (m_bfServerPort && m_bfServerIPAddr
        && (m_bfServerPort != clientId || connectionId != m_bfServerIPAddr)
        && GetAuthenticBFServerPacketElapsedTime() < 28.0)
    {
        if (static_cast<unsigned int>(m_connectionFlag2) <= 0xF9u)
        {
            struct in_addr address;
            address.s_addr = connectionId;
            char message[512];
            std::sprintf(message, "BF2 server %s:%d attempting to connect to instance %d in use",
                         inet_ntoa(address), clientId, m_instanceId);
            pushOutputString(std::string(message));
        }
        ++m_connectionFlag2;
        return false;
    }

    if (m_field08D8.m_count == serverSession.m_count
        && std::memcmp(m_field08D8.m_data, serverSession.m_data, m_field08D8.m_count) == 0)
    {
        length -= 24;
        VOIP::DataString version;
        std::memset(&version, 0, sizeof(version));
        version.m_count = 24;
        std::memcpy(version.m_data, data + length, 24);
        version.m_data[24] = 0;

        if (m_challengeString.m_count == 24
            && std::memcmp(version.m_data, m_challengeString.m_data, 0x18) == 0)
        {
            if (m_bfServerPort != clientId && connectionId != m_bfServerIPAddr)
            {
                m_connectionFlag0 = 0;
                struct in_addr address;
                address.s_addr = connectionId;
                char message[512];
                std::sprintf(message, "BF2 server %s:%d connected", inet_ntoa(address), clientId);
                pushOutputString(std::string(message));
            }
            m_bfServerPort = clientId;
            m_bfServerIPAddr = connectionId;
            m_timer2.reset(0.0);
            return true;
        }

        if (static_cast<unsigned int>(m_connectionFlag1) <= 0xF9u)
        {
            struct in_addr address;
            address.s_addr = connectionId;
            char message[512];
            std::sprintf(message, "BF2 server %s:%d attempted to connect with an invalid version",
                         inet_ntoa(address), clientId);
            pushOutputString(std::string(message));
        }
        ++m_connectionFlag1;
        return false;
    }

    SendChallengeString(connectionId, clientId);

    if (serverSession.m_count == m_versionString.m_count
        && std::memcmp(serverSession.m_data, m_versionString.m_data, serverSession.m_count) == 0)
        return false;

    if (static_cast<unsigned int>(m_connectionFlag0 - 1) <= 0xF8u)
    {
        struct in_addr address;
        address.s_addr = connectionId;
        char message[512];
        std::sprintf(message, "BF2 server %s:%d attempting to connect with invalid authentication",
                     inet_ntoa(address), clientId);
        pushOutputString(std::string(message));
    }
    ++m_connectionFlag0;
    return false;
}

VOIP::VRESULT CBF2VoipServerInstance::InitChallengeString()
{
    m_field09B8.m_count = 0;

    std::string dateTime = dice::hfe::systemInfo()->getDateTimeString();
    if (!dateTime.empty())
    {
        const char* text = dateTime.c_str();
        int length = static_cast<int>(dateTime.size());
        std::srand(text[length - 5] * text[length - 4] * text[length - 2] * text[length - 1]);
    }

    for (int i = 0; i < 10; ++i)
    {
        int random = std::rand();
        int count = m_field09B8.m_count;
        if (count <= 0x63)
        {
            m_field09B8.m_data[count] = static_cast<char>(random % 26 + 'a');
            m_field09B8.m_count = count + 1;
            m_field09B8.m_data[count + 1] = 0;
        }
    }

    char joined[256];
    std::sprintf(joined, "%s%s", m_field09B8.m_data, m_password.c_str());
    char digest[48];
    MD5Digest(reinterpret_cast<unsigned char*>(joined),
              static_cast<unsigned int>(m_password.size() + 10), digest);

    int length = static_cast<int>(std::strlen(digest));
    if (length <= 0x63)
    {
        m_field08D8.m_count = length;
        std::memcpy(m_field08D8.m_data, digest, length);
        m_field08D8.m_data[length] = 0;
    }
    if (m_field08D8.m_count > 20)
    {
        m_field08D8.m_count = 20;
        m_field08D8.m_data[20] = 0;
    }
    return VOIP::VRESULT_SUCCESS;
}

// Sends the 10-letter challenge string plus a trailing 0x68 marker to the BF server at (a, b).
VOIP::VRESULT CBF2VoipServerInstance::SendChallengeString(unsigned int a, unsigned short b)
{
    dice::hfe::io::ISocket::Address address;
    std::memset(&address, 0, sizeof(address));
    *reinterpret_cast<unsigned short*>(&address.sockaddr[0]) = 2;                       // AF_INET
    *reinterpret_cast<unsigned short*>(&address.sockaddr[2]) =
        static_cast<unsigned short>((b >> 8) | (b << 8));                                // host -> network order
    *reinterpret_cast<unsigned int*>(&address.sockaddr[4]) = a;
    address.length = 16;
    m_socket->setPeerAddress(address);

    int count = m_field09B8.m_count;
    std::memcpy(m_packetBuffer, m_field09B8.m_data, count);
    m_packetBuffer[count] = 104;
    if (!m_socket->send(reinterpret_cast<const unsigned char*>(m_packetBuffer), count + 1))
    {
        char message[256];
        int error = errno;
        errno = 0;
        std::sprintf(message, "SendChallengeString - sendto on socket failed - Last Error: %d ", error);
        CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ERROR, message);
        return VOIP::VRESULT_INVALID_STATE;
    }
    return VOIP::VRESULT_SUCCESS;
}

// Pops the oldest queued output string, or an empty string when the queue is empty.
std::string CBF2VoipServerInstance::popOutputString()
{
    if (m_outputStrings.empty())
        return std::string();
    std::string result = m_outputStrings.front();
    m_outputStrings.pop_front();
    return result;
}

// Appends to the output queue; silently drops once it holds more than 127 entries.
void CBF2VoipServerInstance::pushOutputString(std::string text)
{
    if (m_outputStrings.size() > 127)
        return;
    m_outputStrings.push_back(text);
}

// ---- BF2 routing packet handlers ----

// Empties the client tables and zeroes every routing slot.
VOIP::VRESULT CBF2VoipServerInstance::HandlePacketBF2ResetRouting()
{
    VOIP::VRESULT result = RemoveAllClients();
    std::memset(m_routing, 0, sizeof(m_routing));
    return result;
}

// Removes the session; when the client is known, also clears the routing slot named by its player id
// (slot 64 when the id is out of range).
VOIP::VRESULT CBF2VoipServerInstance::HandlePacketBF2RemovePlayer(const BF2VoipPacketData::RemovePlayerData& data)
{
    VOIP::ClientData* client = m_clients.GetClientData(data.m_sessionId);
    if (!client)
        return RemoveClient(data.m_sessionId);

    unsigned int slot = client->m_playerId;
    if (slot > 0x3F)
    {
        m_routing[64].m_flags = 0;
        m_routing[64].m_sessionId = 0;
        return RemoveClient(data.m_sessionId);
    }
    m_routing[slot].m_flags = 0;
    m_routing[slot].m_sessionId = 0;
    return RemoveClient(data.m_sessionId);
}

// Clears one routing slot, then removes the session.
VOIP::VRESULT CBF2VoipServerInstance::HandlePacketBF2RemovePlayer(const BF2VoipPacketData::RoutingDataPlayer& data, int index)
{
    if (static_cast<unsigned int>(index) > 0x3F)
    {
        m_routing[64].m_flags = 0;
        m_routing[64].m_sessionId = 0;
    }
    else
    {
        m_routing[index].m_flags = 0;
        m_routing[index].m_sessionId = 0;
    }
    BF2VoipPacketData::RemovePlayerData removeData;
    removeData.m_sessionId = data.m_sessionId;
    return HandlePacketBF2RemovePlayer(removeData);
}

// Expands a 4-byte routing entry into an 8-byte SetPlayerData for the given channel slot and
// dispatches it.
VOIP::VRESULT CBF2VoipServerInstance::HandlePacketBF2SetPlayerData(const BF2VoipPacketData::RoutingDataPlayer& data, unsigned short index)
{
    BF2VoipPacketData::SetPlayerData setData;
    std::memset(&setData, 0, sizeof(setData));
    setData.m_team = static_cast<unsigned char>(1 - (static_cast<signed char>(data.m_flags) >> 7)); // team1 = 1, team2 = 2
    setData.m_isCommander = (data.m_flags & 0x40) != 0;
    setData.m_isSquadLeader = (data.m_flags & 0x20) != 0;
    setData.m_squadIndex = data.m_flags & 0x0F;
    setData.m_sessionId = data.m_sessionId;
    setData.m_channel = index;
    return HandlePacketBF2SetPlayerData(setData);
}

// Updates one routing slot from the packet, then rebuilds the client's outgoing channel sets and
// registers it. Channel 255 (and the caller-supplied client ids outside 0..63) map to slot 64; a
// squad index outside 1..9 is refused.
VOIP::VRESULT CBF2VoipServerInstance::HandlePacketBF2SetPlayerData(const BF2VoipPacketData::SetPlayerData& data)
{
    unsigned short channel = data.m_channel;
    if (channel > 0x3F)
    {
        if (channel == 255)
        {
            m_routing[64].m_flags = 0;
            m_routing[64].m_sessionId = data.m_sessionId;
            if (data.m_team == 2)
                m_routing[64].m_flags = 0x80;
            if (data.m_isCommander)
                m_routing[64].m_flags |= 0x40;
            if (data.m_isSquadLeader)
                m_routing[64].m_flags |= 0x20;
            m_routing[64].m_flags = static_cast<unsigned char>(m_routing[64].m_flags | data.m_squadIndex);
        }
        channel = data.m_channel;
    }
    else
    {
        m_routing[channel].m_flags = 0;
        m_routing[channel].m_sessionId = data.m_sessionId;
        if (data.m_team == 2)
            m_routing[channel].m_flags = 0x80;
        if (data.m_isCommander)
            m_routing[channel].m_flags |= 0x40;
        if (data.m_isSquadLeader)
            m_routing[channel].m_flags |= 0x20;
        m_routing[channel].m_flags = static_cast<unsigned char>(m_routing[channel].m_flags | data.m_squadIndex);
        channel = data.m_channel;
    }

    VOIP::ClientData client;
    std::memset(&client, 0, sizeof(client));
    client.m_sessionId = data.m_sessionId;
    client.m_playerId = data.m_channel;

    // The shipped code leaves this uninitialised for a team value other than 1 or 2.
    unsigned char outgoingChannel = 0;
    if (data.m_team == 1)
    {
        if (data.m_isCommander)
        {
            outgoingChannel = 0;
        }
        else if (data.m_isSquadLeader)
        {
            if (static_cast<unsigned char>(data.m_squadIndex - 1) <= 8)
                outgoingChannel = data.m_squadIndex;
            else
                goto squadOutOfRange;
        }
        else
        {
            if (static_cast<unsigned char>(data.m_squadIndex - 1) > 8)
                goto squadOutOfRange;
            outgoingChannel = static_cast<unsigned char>(data.m_squadIndex + 9);
        }
    }
    else if (data.m_team == 2)
    {
        if (data.m_isCommander)
        {
            outgoingChannel = 19;
        }
        else if (data.m_isSquadLeader)
        {
            if (static_cast<unsigned char>(data.m_squadIndex - 1) <= 8)
                outgoingChannel = static_cast<unsigned char>(data.m_squadIndex + 19);
            else
                goto squadOutOfRange;
        }
        else
        {
            if (static_cast<unsigned char>(data.m_squadIndex - 1) > 8)
                goto squadOutOfRange;
            outgoingChannel = static_cast<unsigned char>(data.m_squadIndex + 28);
        }
    }
    client.m_channel = outgoingChannel;

    {
        VOIP::ChannelArray channels;
        if (data.m_team == 1)
        {
            if (!data.m_isCommander)
            {
                if (data.m_isSquadLeader)
                {
                    if (static_cast<unsigned char>(data.m_squadIndex - 1) > 8)
                        goto squadOutOfRange;
                    channels.push_back(static_cast<unsigned char>(data.m_squadIndex + 9));
                    client.m_outgoingSets.push_back(channels);
                    channels.clear();
                    channels.push_back(0);
                    client.m_outgoingSets.push_back(channels);
                }
                else
                {
                    if (static_cast<unsigned char>(data.m_squadIndex - 1) > 8)
                        goto squadOutOfRange;
                    channels.push_back(static_cast<unsigned char>(data.m_squadIndex + 9));
                    channels.push_back(data.m_squadIndex);
                    client.m_outgoingSets.push_back(channels);
                }
            }
            else
            {
                for (unsigned char i = 1; i <= 9; ++i)
                    channels.push_back(i);
                client.m_outgoingSets.push_back(channels);
                channels.clear();
                for (unsigned char i = 0; i < 9; ++i)
                {
                    channels.push_back(static_cast<unsigned char>(i + 1));
                    client.m_outgoingSets.push_back(channels);
                }
            }
        }

        if (data.m_team == 2)
        {
            if (data.m_isCommander)
            {
                for (unsigned char i = 20; i <= 28; ++i)
                    channels.push_back(i);
                client.m_outgoingSets.push_back(channels);
                channels.clear();
                for (unsigned char i = 0; i < 9; ++i)
                {
                    channels.push_back(static_cast<unsigned char>(i + 20));
                    client.m_outgoingSets.push_back(channels);
                }
            }
            else if (!data.m_isSquadLeader)
            {
                if (static_cast<unsigned char>(data.m_squadIndex - 1) > 8)
                    goto squadOutOfRange;
                channels.push_back(static_cast<unsigned char>(data.m_squadIndex + 28));
                channels.push_back(static_cast<unsigned char>(data.m_squadIndex + 19));
                client.m_outgoingSets.push_back(channels);
            }
            else
            {
                if (static_cast<unsigned char>(data.m_squadIndex - 1) > 8)
                    goto squadOutOfRange;
                channels.push_back(static_cast<unsigned char>(data.m_squadIndex + 28));
                client.m_outgoingSets.push_back(channels);
                channels.clear();
                channels.push_back(19);
                client.m_outgoingSets.push_back(channels);
            }
        }
    }

    client.m_singleChannel = 0;
    return SetClientData(client);

squadOutOfRange:
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_WARN, "Squad out of range");
    return VOIP::VRESULT_FAILURE;
}

// Reconciles the whole 65-entry routing table against the packet, removing and re-adding the rows
// that differ.
VOIP::VRESULT CBF2VoipServerInstance::HandlePacketBF2FullRoutingDataSet(const char* data, int length)
{
    if (!data || length != 260)
        return static_cast<VOIP::VRESULT>(-4);

    unsigned char packet[260];
    std::memset(packet, 0, sizeof(packet));
    std::memcpy(packet, data, sizeof(packet));

    if (std::memcmp(packet, m_routing, sizeof(packet)) == 0)
        return VOIP::VRESULT_SUCCESS;

    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "");
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "BEGIN Handling Packet Full Routing Data Set - Mismatch Detected ");

    if (std::memcmp(packet + 256, &m_routing[64], 4) != 0)
    {
        m_debugRouting.OutputRDMismatch(this, 255, m_routing[64],
                                        *reinterpret_cast<BF2VoipPacketData::RoutingDataPlayer*>(packet + 256));
        if (m_routing[64].m_flags)
        {
            char message[512];
            std::sprintf(message, "  Routing Data Mismatch - Removing Server's Client Session Id: %d", m_routing[64].m_sessionId);
            CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, message);
            HandlePacketBF2RemovePlayer(m_routing[64], 255);
        }
        if (packet[256])
        {
            char message[512];
            std::sprintf(message, "  Routing Data Mismatch - Adding Server's Client Session Id: %d",
                         reinterpret_cast<BF2VoipPacketData::RoutingDataPlayer*>(packet + 256)->m_sessionId);
            CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, message);
            HandlePacketBF2SetPlayerData(*reinterpret_cast<BF2VoipPacketData::RoutingDataPlayer*>(packet + 256), 255);
        }
    }

    for (int index = 0; index < 64; ++index)
    {
        BF2VoipPacketData::RoutingDataPlayer* packetSlot =
            reinterpret_cast<BF2VoipPacketData::RoutingDataPlayer*>(packet + index * 4);
        if (std::memcmp(packetSlot, &m_routing[index], 4) != 0)
        {
            m_debugRouting.OutputRDMismatch(this, index, m_routing[index], *packetSlot);
            if (m_routing[index].m_flags)
            {
                char message[512];
                std::sprintf(message, "  Routing Data Mismatch - Removing Client Id: %d  Session Id: %d",
                             index, m_routing[index].m_sessionId);
                CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, message);
                HandlePacketBF2RemovePlayer(m_routing[index], index);
            }
            if (packetSlot->m_flags)
            {
                char message[512];
                std::sprintf(message, "  Routing Data Mismatch - Adding Client Id: %d   Session Id: %d",
                             index, packetSlot->m_sessionId);
                CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, message);
                HandlePacketBF2SetPlayerData(*packetSlot, static_cast<unsigned short>(index));
            }
        }
    }

    m_debugRouting.OutputFullState(this, 0x67);
    CDebugStandard::GetInstance()->Output(CDebugStandard::DEBUG_OUTPUT_ROUTING, "END Handling Packet Full Routing Data Set ");
    return VOIP::VRESULT_SUCCESS;
}

VOIP::VRESULT CBF2VoipServerInstance::HandlePacketDebugRoutingData(const char* data, int length)
{
    (void)data; (void)length;
    BF2_VOIP_INSTANCE_FRONTIER("CBF2VoipServerInstance::HandlePacketDebugRoutingData");
}

} // namespace BF2VoipServer
