// Voice-server client: settings copy, channel bring-up, the challenge handshake and the routing
// packets the engine sends to the voice server. Remote and in-process (local) servers are both
// supported; disabled voice allocates nothing.

#include "VoipServerConnection.h"

#if defined(_WIN32)
# include <winsock2.h>
#else
# include <arpa/inet.h>
#endif

#include <cstdio>    // the InitEnabledVoip named stop
#include <cstdlib>   // rand - CreateClientSessionId
#include <cmath>     // powf - ditto
#include "ServerSettings.h"
#if !DEDICATED
#include "../GameServerSettings.h"          // Init's listen-server arm (BF2.exe 0x5B3820)
#include "../Main/Game.h"                   // isDedicated
#endif
#include "../../Core/ClassManager.h"
#include "../../Core/Ids.h"
#include "../../Core/SystemInfo.h"
#include "../../IO/Network/NetworkLayer/Socket/UDPSocket.h"
#include "../../IO/Network/ProtocolLayer/NetServer.h"
#include "../../Scene/Object/PlayerManager.h"
#include "../IGame.h"
#include "../IGameServer.h"
#include "../Objects/IPlayer.h"
#include "../../External/GameSpy/md5.h"            // MD5Digest - the challenge response
#include "VoipServerInternal.h"

#include <cerrno>
#include <cstring>
#include <fstream>

namespace dice {
namespace hfe {

extern ClassManager* classManager;                    // Core/Globals.cpp; PE ds:0x892D58

// ds:0x8AAC980 - the lazily-created singleton pointer. The ELF global carries no symbol;
// [CHOSEN] a file-static here.
static VoipServerConnection* s_instance = 0;

namespace {
void clearFailedVoipSocketError()
{
#if defined(_WIN32)
    // PE calls WSAGetLastError even though it discards the value, then clears it.
    (void)WSAGetLastError();
    WSASetLastError(0);
#else
    errno = 0;
#endif
}

void voipPacketInvariantFailure()
{
#if defined(_MSC_VER)
    // The PE's assertion arm is an int3 and may resume under a debugger. The caller therefore
    // repeats the test before copying, exactly as the shipped body does.
    __debugbreak();
#else
    std::abort();
#endif
}

void storeNativeWord(unsigned char* destination, unsigned short value)
{
    std::memcpy(destination, &value, sizeof(value));
}
} // namespace

// GetInstance - ELF 0x8126600. new(0x4418) + ctor on first use.
VoipServerConnection* VoipServerConnection::GetInstance()
{
    if (!s_instance)
        s_instance = new VoipServerConnection();
    return s_instance;
}

void VoipServerConnection::ReleaseInstance()
{
    if (s_instance)
    {
        delete s_instance;
        s_instance = 0;
    }
}

// Releases the local voice owner, routing log, and channel in that order when present.
// [CHOSEN, not recovered] Cleanup is split into a resource helper.
VoipServerConnection::~VoipServerConnection()
{
    const bool localResourceLive = !m_infoByte4315 && m_localVoipServer440C != 0;
    if (localResourceLive || m_routingLog4414 || m_channel)
        ReleaseEnabledVoipResources();
}

void VoipServerConnection::ReleaseEnabledVoipResources()
{
    if (!m_infoByte4315 && m_localVoipServer440C)
    {
        m_localVoipServer440C->Stop();
        delete m_localVoipServer440C;
        m_localVoipServer440C = 0;
    }

    if (m_routingLog4414)
    {
        m_routingLog4414->close();
        delete m_routingLog4414;
        m_routingLog4414 = 0;
    }

    if (m_channel)
    {
        m_channel->close();
        delete m_channel;
        m_channel = 0;
    }
}

// Starts with no channel or shared secret, voice quality 3, and a twenty-space text buffer.
VoipServerConnection::VoipServerConnection()
{
    m_channel = 0;                                     // +0x04
    memset(m_packet, 0, sizeof(m_packet));             // covers the ELF's +0x4208 loop+memset zeroing
    m_fullRoutingTimer = 0.0f;                         // +0x430C
    memset(_pad4310, 0, sizeof(_pad4310));             // +0x4310
    m_infoByte4314  = 0;                               // +0x4314
    m_infoByte4315  = 0;                               // +0x4315
    memset(_pad4316, 0, sizeof(_pad4316));
    m_infoDword4318 = 0;                               // +0x4318
    memset(_pad431C, 0, sizeof(_pad431C));
    m_infoWord4400  = 0;                               // +0x4400
    m_infoDword4404 = 0;                               // +0x4404
    m_infoWord4408  = 0;                               // +0x4408
    m_localVoipServer440C  = 0;                        // +0x440C
    m_infoDword4410 = 3;                               // Default voice quality.
    m_routingLog4414 = 0;                              // +0x4414

    m_sharedSecret  = "";                              // ELF +0x43FC from c_defaultSharedSecret = ""

    // The twenty-space fill (ELF +0x438C buffer, +0x43F4 counter), verbatim.
    m_textCount = 0;
    memset(m_textBuffer, 0, sizeof(m_textBuffer));
    for (int i = 19; i >= 0; --i)
    {
        unsigned int c = m_textCount;
        if (c <= 0x63)
        {
            m_textBuffer[c] = ' ';
            m_textCount = ++c;
            m_textBuffer[c] = 0;
        }
    }

    m_versionCount = 0;                                // +0x4384; populated by enabled Init
    memset(m_versionBuffer, 0, sizeof(m_versionBuffer));
}

// Copies voice settings; returns false only for a null settings pointer.
// Local voice advertises address 1, while remote voice converts its address with inet_addr.
bool VoipServerConnection::GetVoipServerSettings(ServerSettings* value)
{
    if (!value)
        return false;

    m_infoByte4314  = (value->m_voipEnabled == true);
    m_infoByte4315  = (value->m_voipServerRemote == true);
    m_infoDword4410 = value->m_voipQuality;

    if (m_infoByte4315)
        m_infoDword4318 = (int)inet_addr(value->m_voipServerRemoteIP.c_str());
    else
        m_infoDword4318 = 1;

    m_infoWord4400  = value->m_voipServerPort;
    m_infoDword4404 = value->m_voipBFServerPort;
    m_infoWord4408  = value->m_voipBFClientPort;
    m_sharedSecret  = value->m_voipSharedPassword;
    return true;
}

#if !DEDICATED
// Copies listen-server voice settings through the settings interface in the same order as the record overload.
bool VoipServerConnection::GetVoipServerSettings(IGameServerSettings* value)
{
    if (!value)
        return false;

    m_infoByte4314  = (value->getVoipEnabled() == 1);
    m_infoByte4315  = (value->getVoipServerRemote() == 1);
    m_infoDword4410 = value->getVoipQuality();

    if (!m_infoByte4315)
        m_infoDword4318 = 1;
    else
        m_infoDword4318 = (int)inet_addr(value->getVoipServerRemoteIP().c_str());

    m_infoWord4400  = value->getVoipServerPort();
    m_infoDword4404 = value->getVoipBFServerPort();
    m_infoWord4408  = value->getVoipBFClientPort();
    m_sharedSecret  = value->getVoipSharedPassword();
    return true;
}
#endif

// Copies settings after the singleton lookup. Disabled voice succeeds without allocating resources;
// enabled voice builds the channel (and the in-process server) through InitEnabledVoip.
bool VoipServerConnection::Init()
{
    // Both binaries discard this exact call's return. ELF: classManager ds:0x8AC85A0, vslot
    // +0x4C at 0x8126C86; PE: ds:0x892D58, vslot +0x48 at 0x54B160. The one-slot difference is
    // the MSVC/Itanium destructor-pair ABI split. The global serverSettings below is a separate
    // load in both binaries; substituting the lookup result would be an infidelity.
#if !DEDICATED
    // [PLATFORM] BF2.exe 0x5B3C50 (): the client keeps the lookup's answer - a listen
    // server copies its voice settings out of that GameServerSettings object (0x5B3820), a dedicated
    // one out of the ServerSettings record (0x5B3920), picked by isDedicated().
    IBase* gameServerSettings = classManager->getSingleton("GameServerSettings");
    if (isDedicated())
    {
        if (!GetVoipServerSettings(serverSettings))
            return false;
    }
    else if (!GetVoipServerSettings(static_cast<IGameServerSettings*>(gameServerSettings)))
    {
        return false;
    }
#else
    classManager->getSingleton("GameServerSettings");

    if (!GetVoipServerSettings(serverSettings))
        return false;
#endif

    if (!m_infoByte4314)
        return true;

    return InitEnabledVoip();
}

// Voice-enabled initialization: writes the fixed version string, opens and binds the voice
// channel, points it at the BF server, and (unless voice is remote) creates and starts the
// in-process voice server with a random shared password.
// Recovered from fh2 Init 0x4CCCA0 and the x86 PE exe 0x54B130.
bool VoipServerConnection::InitEnabledVoip()
{
    char version[256];
    sprintf(version, "%06d%06d%06d%06d", 1, 5, 3153, 0);
    std::size_t versionLength = std::strlen(version);
    if (versionLength <= 0x63)
    {
        m_versionCount = static_cast<int>(versionLength);
        std::memcpy(m_versionBuffer, version, versionLength);
        m_versionBuffer[versionLength] = 0;
    }

    m_channel = new io::UDPSocket();
    if (!m_channel)
        return false;

    char listenAddress[128];
    sprintf(listenAddress, ":%d", m_infoDword4404);
    if (!m_channel->listen(std::string(listenAddress), false))
    {
        // The shipped code clears the last error here rather than failing.
#if defined(_WIN32)
        (void)WSAGetLastError();
        WSASetLastError(0);
#else
        errno = 0;
#endif
    }

    io::ISocket::Address address;
    std::memset(&address, 0, sizeof(address));
    *reinterpret_cast<unsigned short*>(&address.sockaddr[0]) = 2;                       // AF_INET
    unsigned short peerPort = static_cast<unsigned short>(m_infoWord4400);
    *reinterpret_cast<unsigned short*>(&address.sockaddr[2]) =
        static_cast<unsigned short>((peerPort >> 8) | (peerPort << 8));                  // host -> network order
    unsigned int peerAddress = static_cast<unsigned int>(m_infoDword4318);
    if (peerAddress == 1)
    {
        // Address 1 selects local voice: resolve this server's own address.
        std::string localAddress("127.0.0.1");
        IGameServer* gameServer = static_cast<IGameServer*>(game->queryInterface(IID_IGameServer));
        if (gameServer)
        {
            io::NetServer* netServer = gameServer->getNetServer();
            if (netServer)
                localAddress = netServer->getServerAddress();
        }
        peerAddress = static_cast<unsigned int>(inet_addr(localAddress.c_str()));
    }
    *reinterpret_cast<unsigned int*>(&address.sockaddr[4]) = peerAddress;
    address.length = 16;
    m_channel->setPeerAddress(address);

    if (m_infoByte4315)
        return true;                                   // remote voice server: nothing local to start

    m_localVoipServer440C = new VoipServerInternal();
    if (!m_localVoipServer440C)
        return true;

    // The generated one-time secret replaces the settings value: the challenge handler hashes
    // m_sharedSecret, and the in-process server must expect the same bytes. The PE stores it in the
    // shared-secret field (this+0x43FC) rather than a temporary, then hands that same string to the
    // server's Start.
    const int letterCount = rand() % 10 + 8;
    m_sharedSecret = static_cast<char>(118);            // leading 'v'
    for (int i = 0; i < letterCount; ++i)
        m_sharedSecret += static_cast<char>(rand() % 26 + 'a');
    m_localVoipServer440C->Start(m_sharedSecret, static_cast<unsigned int>(m_infoWord4400));
    return true;
}

// Appends 24 version bytes and 20 session bytes, excluding terminators.
// [PLATFORM] Length assertions abort on ELF; PE may resume and skip the invalid copy.
void VoipServerConnection::PacketAppendVersionAndSessionId(int& cursor)
{
    if (m_versionCount != 24)
        voipPacketInvariantFailure();
    if (m_versionCount == 24)
    {
        std::memcpy(m_packet + cursor, m_versionBuffer, 24);
        cursor += m_versionCount;
    }

    if (m_textCount != 20)
        voipPacketInvariantFailure();
    if (m_textCount == 20)
    {
        std::memcpy(m_packet + cursor, m_textBuffer, 20);
        cursor += m_textCount;
    }
}

// Sends team/commander/leader/squad/session/player routing as message 100, or
// message 101 to remove a player outside squads 1..10. Words use native little
// endian. Retains the server-interface query and optional routing debug dump.
bool VoipServerConnection::UpdatePlayerRouting(world::IPlayer* player)
{
    if (!m_channel || !world::playerManager)
        return false;

    unsigned char routing[8];
    std::memset(routing, 0, sizeof(routing));

    unsigned short playerId = 255;
    if (player->getId() != -1)
        playerId = static_cast<unsigned short>(player->getId());
    storeNativeWord(routing + 6, playerId);
    routing[0] = static_cast<unsigned char>(player->getTeam());

    // Both ports make this query and discard the result. Neither checks `game` for null.
    (void)game->queryInterface(IID_IGameServer);
    std::string unusedAddress;
    (void)unusedAddress;

    const unsigned short sessionId = player->getVoipSessionId();
    int cursor = 0;

    if (player->getIsCommander())
    {
        routing[1] = 1;
        routing[2] = 0;
        routing[3] = 0;
        storeNativeWord(routing + 4, sessionId);
    }
    else if (player->getIsSquadLeader()
          && static_cast<unsigned int>(player->getSquadId()) != 0
          && static_cast<unsigned int>(player->getSquadId()) <= 10u)
    {
        routing[1] = 0;
        routing[2] = 1;
        routing[3] = static_cast<unsigned char>(player->getSquadId());
        storeNativeWord(routing + 4, sessionId);
    }
    else
    {
        if (static_cast<unsigned int>(player->getSquadId()) == 0
         || static_cast<unsigned int>(player->getSquadId()) > 10u)
        {
            storeNativeWord(m_packet, sessionId);
            cursor = 2;
            PacketAppendVersionAndSessionId(cursor);
            m_packet[cursor] = 101;
            goto packet_ready;
        }

        routing[1] = 0;
        routing[2] = 0;
        routing[3] = static_cast<unsigned char>(player->getSquadId());
        storeNativeWord(routing + 4, sessionId);
    }

    std::memcpy(m_packet, routing, sizeof(routing));
    cursor = 8;
    PacketAppendVersionAndSessionId(cursor);
    m_packet[cursor] = 100;

packet_ready:
    ++cursor;
    if (!m_channel->send(m_packet, cursor))
        clearFailedVoipSocketError();

    if (m_routingLog4414)
    {
        *m_routingLog4414 << std::endl
                          << "-----------------------------------------------------------------------------------------"
                          << std::endl;
        *m_routingLog4414 << systemInfo()->getDateTimeString().c_str() << std::endl;

        const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
        for (std::list<world::IPlayer*>::const_iterator it = players.begin();
             it != players.end(); ++it)
        {
            world::IPlayer* const current = *it;
            if (!current->getIsAIPlayer())
            {
                *m_routingLog4414 << "Player: " << current->getId() << ", "
                                  << current->getName()
                                  << "  Commander: " << current->getIsCommander()
                                  << "  SquadLeader: " << current->getIsSquadLeader()
                                  << "  SquadID: "
                                  << static_cast<unsigned int>(current->getSquadId())
                                  << std::endl;
            }
        }
    }

    return true;
}

// ResetPlayerRouting - ELF 0x8126BB0, 129 bytes.
//   * null channel -> return 0 (the boot state; nothing sent).
//   * otherwise: header via PacketAppendVersionAndSessionId, then message byte 102, then one
//     channel send; a failed send clears errno (yes - clears, not reads; preserved).
int VoipServerConnection::ResetPlayerRouting()
{
    if (!m_channel)
        return 0;

    int cursor = 0;
    PacketAppendVersionAndSessionId(cursor);
    m_packet[cursor] = 102;                            // *(this + 8 + cursor) = 102
    ++cursor;
    if (!m_channel->send(m_packet, cursor))
        clearFailedVoipSocketError();
    return 1;
}

// PE 0x54B900, 78 B. Gated on the enabled byte: a server that never Init'ed VoIP does nothing.
// Past the gate it resends the full routing table, then drains the channel.
void VoipServerConnection::Update(float dt)
{
    if (!m_infoByte4314)
        return;

    FullRoutingUpdate(dt);
    Receive();
}

// PE 0x54B700, 502 B. Decrements the resend timer; on expiry, and while a PlayerManager exists,
// rebuilds the 65-slot routing table, appends the invariant version/session header, tags message
// 103 and sends it on the channel. Slot 64 holds players whose id is -1 or 255; other ids outside
// 0..63 are dropped. A failed send clears the socket error rather than reporting it. AI players
// and players with neither a command nor a valid squad contribute nothing.
void VoipServerConnection::FullRoutingUpdate(float dt)
{
    m_fullRoutingTimer -= dt;
    if (m_fullRoutingTimer >= 0.0f)
        return;

    m_fullRoutingTimer += 5.0f;
    if (!world::playerManager)
        return;

    unsigned int table[65];
    std::memset(table, 0, sizeof(table));

    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        if (player->getIsAIPlayer())
            continue;

        unsigned char flags = 0;
        if (player->getTeam() == 2)
            flags = 0x80;

        const unsigned int squadId = static_cast<unsigned int>(player->getSquadId());
        if (player->getIsCommander())
        {
            flags = static_cast<unsigned char>(flags | 0x40);
        }
        else if (player->getIsSquadLeader() && squadId != 0 && squadId <= 10u)
        {
            flags = static_cast<unsigned char>(flags | (squadId | 0x20u));
        }
        else if (squadId != 0 && squadId <= 10u)
        {
            flags = static_cast<unsigned char>(flags | (squadId & 0x0Fu));
        }
        else
        {
            continue;
        }

        const unsigned short sessionId = player->getVoipSessionId();
        const unsigned int entry = (static_cast<unsigned int>(sessionId) << 16) | flags;

        const int id = player->getId();
        if (id == 255 || id == -1)
            table[64] = entry;
        else if (id >= 0 && id < 64)
            table[id] = entry;
    }

    std::memcpy(m_packet, table, sizeof(table));
    int cursor = 260;
    PacketAppendVersionAndSessionId(cursor);
    m_packet[cursor] = 103;
    ++cursor;
    if (!m_channel->send(m_packet, cursor))
        clearFailedVoipSocketError();
}

// ELF 0x81275d0. Trailing packet-id extraction: reads the last byte of an n-byte datagram into
// packetId and decrements length to n-1, the message-body length. A zero length reads nothing and
// reports false.
int VoipServerConnection::ExtractPacketId(unsigned char* buffer, int& length, unsigned char& packetId)
{
    if (length > 0)
    {
        packetId = buffer[length - 1];
        --length;
        return 1;
    }
    return 0;
}

// PE 0x54B000, ~0x7e / ELF 0x8127810. Non-blocking recv on the channel, then a materialisation of
// the peer address that the shipped body never reads back. An empty read or a socket error returns
// without dispatching; the error arm clears the socket error first. A datagram whose trailing id
// byte is 104 is the BFVoipChallengeString handshake, and its response recomputes the session every
// routing packet must carry.
void VoipServerConnection::Receive()
{
    if (!m_channel)
        return;

    const int received = m_channel->receiveFrom(m_packet, 16896);

    // getPeerAddress (vslot +0x20): the shipped body copies the 17-byte address into a local and
    // discards it. [UNRESOLVED] No consumer is evident in the recovered body; the call is preserved
    // for ordering.
    const io::ISocket::Address peerAddress = m_channel->getPeerAddress();
    (void)peerAddress;

    if (received == -1)
        clearFailedVoipSocketError();

    if (received <= 0)
        return;

    int length = received;
    unsigned char packetId = 0;
    ExtractPacketId(m_packet, length, packetId);
    if (packetId == 104)
        HandlePacketChallengeString(reinterpret_cast<char*>(m_packet), length);
}

// PE 0x54AEA0, ELF 0x8127600 (HandlePacketChallengeString). The valid arm concatenates the 10-byte
// challenge with the shared secret, hashes it and stores the truncated session in m_textBuffer,
// which PacketAppendVersionAndSessionId then publishes on every routing packet.
//
// Any other length takes retail's assertion path:
//   Debug(DebugTypeError, "VoipServerConnection.cpp", 631)
//       << "BFVoipChallengeString packet length is invalid"
// which aborts the process. The datagram is unauthenticated and remote, so any UDP packet ending in
// 0x68 whose total length is not 11 kills the server - a remote denial of service.
//
// [CHOSEN, not recovered] A non-10 length is dropped silently instead of aborting. Such a datagram
// cannot be a challenge, so ignoring it preserves every legitimate outcome while removing the DoS.
// It is deliberately not logged: a diagnostic per malformed datagram would itself be a log-flood
// vector. This is an intentional departure from retail, not recovered behavior.
void VoipServerConnection::HandlePacketChallengeString(char* data, int length)
{
    if (length != 10)
        return;

    char challenge[11];
    std::memcpy(challenge, data, 10);
    challenge[10] = 0;

    char joined[256];
    std::sprintf(joined, "%s%s", challenge, m_sharedSecret.c_str());

    char digest[33];
    MD5Digest(reinterpret_cast<unsigned char*>(joined),
              static_cast<unsigned int>(m_sharedSecret.size()) + 10u, digest);

    const int digestLength = static_cast<int>(std::strlen(digest));
    if (digestLength <= 0x63)
    {
        m_textCount = static_cast<unsigned int>(digestLength);
        std::memcpy(m_textBuffer, digest, static_cast<std::size_t>(digestLength));
        m_textBuffer[digestLength] = 0;
    }
    if (m_textCount > 20)
    {
        m_textCount = 20;
        m_textBuffer[20] = 0;
    }
}

// Chooses a random voice-session ID independently of playerId, restarting the player scan on collision.
// Retries have no limit. Collision increments can exceed 16 bits; later storage narrows
// the result, so collisions introduced by that narrowing are not detected.
int VoipServerConnection::CreateClientSessionId(int playerId)
{
    (void)playerId;                                     // deliberate - see above

    if (!world::playerManager)
        return 0;

    const int range = (int)powf(256.0f, 2.0f);          // 65536, computed not folded
    int id = rand() % range;

    const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();

    bool clash = true;
    while (clash)
    {
        clash = false;
        for (std::list<world::IPlayer*>::const_iterator it = players.begin();
             it != players.end(); ++it)
        {
            if ((*it)->getVoipSessionId() == id)        // vslot 624 = +0x270
            {
                id += 1 + (rand() % 500);               // random bump, not +1
                clash = true;                           // and rescan from the head
                break;
            }
        }
    }
    return id;
}

} // namespace hfe
} // namespace dice
