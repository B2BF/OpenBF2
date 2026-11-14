// Voice-server diagnostics implementation: bf2 ia-32 ELF 0x84F2660..0x84F4170 (symbols).
//
// CDebugStandard::Output is a bare return in the shipped images, so the standard messages are
// discarded. The routing dump and the voice trace write plain-text logs under
// ./mods/bf2/Logs/Voip/, relative to the server's working directory.

#if defined(_WIN32)
#  define _WINSOCK_DEPRECATED_NO_WARNINGS   // inet_ntoa, as the shipped code calls it
#  include <winsock2.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#endif
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "VoipDebug.h"
#include "VoipPackets.h"
#include "VoipServerInstance.h"
#include "../Core/SystemInfo.h"

namespace BF2VoipServer {

// The singleton lives here so Close() can reset the pointer GetInstance() caches (ELF
// CDebugStandard::_instance, 0x8AC8290).
static CDebugStandard* s_standardInstance = 0;

namespace {

const char kVoipLogDir[] = "./mods/bf2/Logs/Voip/";
const char kRule[] =
    "-----------------------------------------------------------------------------------------";

// The routing byte, decoded the way the dump reads it: bit7 team 2 (else 1), bit6 commander,
// bit5 squad leader, bits0-3 squad index. The ELF builds it in an 8-byte SetPlayerData local with
// the session id and channel zeroed.
BF2VoipPacketData::SetPlayerData decodeRouting(unsigned char flags)
{
    BF2VoipPacketData::SetPlayerData data;
    std::memset(&data, 0, sizeof(data));
    data.m_team          = static_cast<unsigned char>(2 - (static_cast<unsigned char>(~flags) >> 7));
    data.m_isCommander   = (flags >> 6) & 1;
    data.m_isSquadLeader = (flags >> 5) & 1;
    data.m_squadIndex    = flags & 0x0F;
    return data;
}

// "  Team: t  Is Commander: c  Is Squad Leader: s  Squad Id: q" and endl. The fields go through
// operator<<(int), as the shipped flag tests show.
void writeRoutingFields(std::ostream& os, const BF2VoipPacketData::SetPlayerData& d)
{
    os << "  Team: " << static_cast<int>(d.m_team);
    os << "  Is Commander: " << static_cast<int>(d.m_isCommander);
    os << "  Is Squad Leader: " << static_cast<int>(d.m_isSquadLeader);
    os << "  Squad Id: " << static_cast<int>(d.m_squadIndex) << std::endl;
}

} // namespace

// ---- CDebugStandard ----

CDebugStandard::CDebugStandard()
    : m_stream(0)
{
}

CDebugStandard::~CDebugStandard()
{
    if (m_stream)
    {
        m_stream->close();
        delete m_stream;
        m_stream = 0;
    }
}

CDebugStandard* CDebugStandard::GetInstance()
{
    if (!s_standardInstance)
        s_standardInstance = new CDebugStandard();
    return s_standardInstance;
}

void CDebugStandard::Close()
{
    if (s_standardInstance)
    {
        delete s_standardInstance;
        s_standardInstance = 0;
    }
}

// The shipped Output is a single retn; every message is discarded.
void CDebugStandard::Output(StandardDebugOutputType type, const char* message)
{
    (void)type;
    (void)message;
}

std::string CDebugStandard::CTimeDateStringNoSpaces() const
{
    std::string text = dice::hfe::systemInfo()->getDateTimeString();
    std::string::size_type pos;
    while ((pos = text.find_first_not_of("0123456789_")) != std::string::npos)
        text[pos] = '_';
    return text;
}

// ---- CDebugRoutingState ----

CDebugRoutingState::CDebugRoutingState()
    : m_stream(0)
{
}

CDebugRoutingState::~CDebugRoutingState()
{
    if (m_stream)
    {
        m_stream->close();
        delete m_stream;
        m_stream = 0;
    }
}

bool CDebugRoutingState::Enabled()
{
    return m_stream != 0;
}

bool CDebugRoutingState::Start()
{
    if (Enabled())
        return false;

    m_stream = new std::ofstream();
    char path[512];
    std::string stamp = CDebugStandard::GetInstance()->CTimeDateStringNoSpaces();
    std::sprintf(path, "%sVoipServerRouting_Instance_%s%s", kVoipLogDir, stamp.c_str(), ".log");
    m_stream->open(path, std::ios::out);
    return true;
}

bool CDebugRoutingState::OutputRDMismatch(CBF2VoipServerInstance* instance, int channel,
                                          const BF2VoipPacketData::RoutingDataPlayer& serverSide,
                                          const BF2VoipPacketData::RoutingDataPlayer& packetSide)
{
    if (!instance || !Enabled())
        return true;

    std::ostream& os = *m_stream;
    os << std::endl << kRule << std::endl;
    std::string when = dice::hfe::systemInfo()->getDateTimeString();
    os << when.c_str() << std::endl;
    os << "Routing Mismatch for Player: " << channel << ", ";
    const char* name = instance->GetVoipConnectionPlayerNameFromID(channel);
    os << (name ? name : "NAME NOT FOUND  ") << std::endl;
    os << "Last Routing Byte: " << static_cast<int>(serverSide.m_flags) << std::endl;
    os << "New  Routing Byte: " << static_cast<int>(packetSide.m_flags) << std::endl;
    return true;
}

bool CDebugRoutingState::OutputFullState(CBF2VoipServerInstance* instance, unsigned char packetType)
{
    if (!instance || !Enabled())
        return true;

    std::ostream& os = *m_stream;
    os << std::endl << kRule << std::endl;
    std::string when = dice::hfe::systemInfo()->getDateTimeString();
    os << when.c_str() << std::endl;
    std::string packetName = instance->MapPacketTypeToName(packetType);
    os << "PACKET: " << packetName.c_str() << std::endl;

    // [ROUTING BLOCK]: the server slot (64, printed as 255) when it has a name and a routing byte,
    // then every channel 0..63 with a routing byte.
    os << std::endl << std::endl << "[ROUTING BLOCK]" << std::endl << std::endl;
    if (instance->m_playerNames[64][0] != 0 && instance->m_routing[64].m_flags != 0)
    {
        os << "Player: 255" << ", " << instance->m_playerNames[64];
        writeRoutingFields(os, decodeRouting(instance->m_routing[64].m_flags));
    }
    for (int channel = 0; channel <= 63; ++channel)
    {
        const unsigned char flags = instance->m_routing[channel].m_flags;
        if (flags == 0)
            continue;
        os << "Player: " << channel << ", " << instance->m_playerNames[channel];
        writeRoutingFields(os, decodeRouting(flags));
    }

    // [CLIENT TABLES]: every client, keyed by session id.
    os << std::endl << std::endl << "[CLIENT TABLES]" << std::endl << std::endl;
    std::map<unsigned short, VOIP::ClientData>& clients = instance->m_clients.m_clients;
    for (std::map<unsigned short, VOIP::ClientData>::iterator it = clients.begin(); it != clients.end(); ++it)
    {
        const unsigned short key = it->first;
        VOIP::ClientData& client = it->second;
        const char* name = instance->GetVoipConnectionPlayerNameFromID(client.m_playerId);
        os << std::endl << (name ? name : "NAME NOT FOUND  ");

        std::string incoming = instance->MapChannelToName(client.m_channel);
        const bool multi = client.m_multi != 0;
        // The shipped code hands the 16-bit map key to inet_ntoa as the address.
        in_addr address;
        address.s_addr = key;
        os << "  IP: " << inet_ntoa(address) << "  ID: " << client.m_playerId;
        os << "  Single Channel Set Selected: " << client.m_singleChannel;
        os << "  Is Multi Channel Set Selected: " << multi << std::endl;
        os << "  Incoming Channel:" << std::endl << "    " << incoming.c_str() << std::endl;
        os << "  Outgoing Channels: " << std::endl;

        for (unsigned int set = 0; set < client.m_outgoingSets.size(); ++set)
        {
            os << "    Set " << set << std::endl;
            for (unsigned int i = 0; i < client.m_outgoingSets[set].size(); ++i)
            {
                std::string channelName = instance->MapChannelToName(client.m_outgoingSets[set][i]);
                os << "      " << channelName.c_str() << std::endl;
            }
        }
    }

    // [OUTGOING CHANNEL TABLES]: every channel and the players it reaches.
    os << std::endl << std::endl << "[OUTGOING CHANNEL TABLES]" << std::endl << std::endl;
    std::map<unsigned char, VOIP::SessionIdArray>& channels = instance->m_outgoingChannels.m_channels;
    for (std::map<unsigned char, VOIP::SessionIdArray>::iterator it = channels.begin(); it != channels.end(); ++it)
    {
        std::string channelName = instance->MapChannelToName(it->first);
        os << "" << channelName.c_str() << std::endl;
        for (unsigned int i = 0; i < it->second.size(); ++i)
        {
            VOIP::ClientData* client = instance->m_clients.GetClientData(it->second[i]);
            const char* name = client ? instance->GetVoipConnectionPlayerNameFromID(client->m_playerId) : 0;
            os << "  " << (name ? name : "NAME NOT FOUND") << std::endl;
        }
    }
    os << std::flush;
    return true;
}

// ---- CDebugVoiceTrace ----

CDebugVoiceTrace::CDebugVoiceTrace()
    : m_stream(0), m_connectionId(0), m_frameCount(0)
{
}

CDebugVoiceTrace::~CDebugVoiceTrace()
{
    Reset();
}

bool CDebugVoiceTrace::Start(const char* name, int playerId, unsigned int connectionId)
{
    if (m_stream)
        return false;

    m_connectionId = connectionId;
    std::string stamp = CDebugStandard::GetInstance()->CTimeDateStringNoSpaces();
    char path[512];
    std::sprintf(path, "%sVoiceTraffic_Player_%s_%d_%s%s", kVoipLogDir, name ? name : "NO NAME",
                 playerId, stamp.c_str(), ".log");
    m_stream = new std::ofstream();
    m_stream->open(path, std::ios::out);
    return true;
}

bool CDebugVoiceTrace::Output(unsigned int fromConnection, unsigned int toConnection,
                              const char* fromName, const char* toName, char* data, int length)
{
    if (m_connectionId != fromConnection && m_connectionId != toConnection)
        return true;
    if (!m_stream)
        return true;

    std::string line("Voice Frame From: ");
    char number[32];
    line += fromName ? fromName : "NONAME";
    line += " To: ";
    line += toName ? toName : "NONAME";
    std::sprintf(number, "%d", length);
    line += " Packet Size: ";
    line += number;
    if (length > 4)
    {
        // The frame number: the signed 16-bit word four bytes from the end.
        short frame;
        std::memcpy(&frame, data + length - 4, sizeof(frame));
        std::sprintf(number, "%d", static_cast<int>(frame));
        line += " Frame: ";
        line += number;
    }
    else
    {
        line += " *** Packet Size less than 5 bytes ***";
    }

    std::string stamp = CDebugStandard::GetInstance()->CTimeDateStringNoSpaces();
    *m_stream << stamp.c_str() << " " << line.c_str() << std::endl;
    if (++m_frameCount > 29999)
        Stop(m_connectionId);
    return true;
}

void CDebugVoiceTrace::Reset()
{
    if (m_stream)
    {
        m_stream->close();
        delete m_stream;
    }
    m_stream = 0;
    m_frameCount = 0;
    m_connectionId = 0;
}

bool CDebugVoiceTrace::Stop(unsigned int connectionId)
{
    if (!m_stream || m_connectionId != connectionId)
        return false;
    Reset();
    return true;
}

bool CDebugVoiceTrace::IsRunning()
{
    return m_stream != 0;
}

} // namespace BF2VoipServer
