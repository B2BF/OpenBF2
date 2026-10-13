// [TEMP] Runtime checks for CBF2VoipServerInstance state and getters (Phase 3, self-contained part).
//
// Exercises the constructor's recovered initialisation, the client-table overrides, the getters and
// the name tables. The UDP transport, challenge handshake and BF2 packet handlers are named
// frontiers and are not driven here.

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

#include "../BF2/VoipServer/VoipServerInstance.h"

static int g_checks = 0;
static int g_failures = 0;

static void expect(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

static VOIP::ClientData makeClient(unsigned short sessionId, unsigned char channel,
                                   unsigned short playerId, unsigned int connId, unsigned short clientId)
{
    VOIP::ClientData d;
    d.m_sessionId = sessionId;
    d.m_connectionId = connId;
    d.m_clientId = clientId;
    d.m_playerId = playerId;
    d.m_channel = channel;
    d.m_outgoingSets.resize(1);
    d.m_singleChannel = 0;
    d.m_multi = 0;
    return d;
}

int main()
{
    using namespace BF2VoipServer;

    CBF2VoipServerInstance instance;

    // Constructor: ports zero, no clients, empty map, instance id -1 (via a successful first Add).
    expect(instance.GetListenPort() == 0, "ctor clears listen port");
    expect(instance.GetSendToClientPort() == 0, "ctor clears send-to-client port");
    expect(instance.GetBFServerPort() == 0, "ctor clears BF server port");
    expect(instance.GetBFServerIPAddr() == 0, "ctor clears BF server IP");
    expect(instance.GetNumClients() == 0, "ctor starts with no clients");
    std::map<unsigned short, VOIP::ClientData>* map = instance.GetClientDataMap();
    expect(map != 0, "GetClientDataMap returns the table");
    expect(map != 0 && map->empty(), "GetClientDataMap table is empty");

    // Client-table overrides.
    expect(instance.SetClientData(makeClient(10, 5, 0x1111, 1, 1)) == VOIP::VRESULT_SUCCESS,
           "SetClientData accepts a client");
    expect(instance.SetClientData(makeClient(20, 5, 0x2222, 2, 2)) == VOIP::VRESULT_SUCCESS,
           "SetClientData accepts a second client");
    expect(instance.GetNumClients() == 2, "two clients present");
    expect(map != 0 && map->size() == 2, "map reports two rows");

    expect(instance.RemoveClient(10) == VOIP::VRESULT_SUCCESS, "RemoveClient drops a client");
    expect(instance.GetNumClients() == 1, "one client after removal");
    expect(instance.RemoveClient(10) == VOIP::VRESULT_FAILURE, "RemoveClient on a miss");

    expect(instance.RemoveAllClients() == VOIP::VRESULT_SUCCESS, "RemoveAllClients");
    expect(instance.GetNumClients() == 0, "no clients after RemoveAllClients");

    // Debug routing is off by default, so no name is returned.
    expect(instance.GetVoipConnectionPlayerNameFromID(0) == 0, "player name is null while debug routing is off");

    // Name tables.
    expect(instance.MapChannelToName(0) == "CHANNEL_TEAM_1_COMMANDER", "channel 0 name");
    expect(instance.MapChannelToName(19) == "CHANNEL_TEAM_2_COMMANDER", "channel 19 name");
    expect(instance.MapChannelToName(37) == "CHANNEL_TEAM_2_SQUAD_I_MEMBER", "channel 37 name");
    expect(instance.MapChannelToName(38) == "INVALID CHANNEL", "channel 38 is invalid");
    expect(instance.MapPacketTypeToName(1) == "VOIP_PACKET_SERVER_INCOMING_VOICE_FRAME_DEFAULT_CHANNEL_SET",
           "packet 1 name");
    expect(instance.MapPacketTypeToName(100) == "BF2_VOIP_PACKET_SET_PLAYER_DATA", "packet 100 name");
    expect(instance.MapPacketTypeToName(250) == "INVALID PACKET ID", "packet 250 is invalid");

    // ExtractServerSessionId copies the trailing 20 bytes and sets the count.
    {
        char buffer[32];
        for (int i = 0; i < 32; ++i)
            buffer[i] = static_cast<char>(i);
        VOIP::DataString out;
        std::memset(&out, 0, sizeof(out));
        int length = 32;
        expect(instance.ExtractServerSessionId(buffer, length, out) == VOIP::VRESULT_SUCCESS,
               "ExtractServerSessionId succeeds");
        expect(length == 12, "ExtractServerSessionId shortens the buffer by 20");
        expect(out.m_count == 20, "ExtractServerSessionId sets the count to 20");
        expect(out.m_data[0] == 12 && out.m_data[19] == 31, "ExtractServerSessionId copies the tail");
        expect(out.m_data[20] == 0, "ExtractServerSessionId terminates the string");
    }

    expect(instance.GetAuthenticBFServerPacketElapsedTime() >= 0.0,
           "authentication elapsed time is non-negative");

    std::printf("voip instance: %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
