// [TEMP] Runtime checks for the voice-server debug logs (frontier closure batch 3).
//
// Drives CBF2VoipServerInstance::HandlePacketDebugRoutingData, CDebugRoutingState
// (Start / OutputFullState / OutputRDMismatch) and CDebugVoiceTrace against a real instance, then
// reads back the files they write under ./mods/bf2/Logs/Voip/ in a scratch working directory and
// compares them with the text the bf2 ia-32 ELF bodies (0x84F2660..0x84F4170) produce. Date lines
// are matched as wildcards. No sockets are bound.
// macOS: CTest bf2_voip_debug_check (built on request); Windows: voip_debug_runtime.cmd.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#if defined(_WIN32)
#  include <direct.h>
#  include <io.h>
#  include <process.h>
#  define bf2_mkdir(p) _mkdir(p)
#  define bf2_chdir(p) _chdir(p)
#else
#  include <dirent.h>
#  include <sys/stat.h>
#  include <unistd.h>
#  define bf2_mkdir(p) mkdir(p, 0755)
#  define bf2_chdir(p) chdir(p)
#endif

#define private public
#define protected public
#include "../BF2/VoipServer/VoipServerInstance.h"
#undef private
#undef protected

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

static const char kLogDir[] = "mods/bf2/Logs/Voip";

// Every file in the log directory whose name starts with prefix.
static std::vector<std::string> logFiles(const std::string& prefix)
{
    std::vector<std::string> out;
#if defined(_WIN32)
    _finddata_t data;
    intptr_t handle = _findfirst((std::string(kLogDir) + "/" + prefix + "*").c_str(), &data);
    if (handle != -1)
    {
        do { out.push_back(data.name); } while (_findnext(handle, &data) == 0);
        _findclose(handle);
    }
#else
    if (DIR* dir = opendir(kLogDir))
    {
        while (dirent* entry = readdir(dir))
            if (std::strncmp(entry->d_name, prefix.c_str(), prefix.size()) == 0)
                out.push_back(entry->d_name);
        closedir(dir);
    }
#endif
    return out;
}

static std::vector<std::string> readLines(const std::string& name)
{
    std::vector<std::string> lines;
    std::ifstream in((std::string(kLogDir) + "/" + name).c_str());
    std::string line;
    while (std::getline(in, line))
        lines.push_back(line);
    return lines;
}

// Lines must match exactly, except that "<DATE>" matches any non-empty line.
static bool sameText(const std::vector<std::string>& got, const std::vector<std::string>& want)
{
    bool ok = got.size() == want.size();
    for (size_t i = 0; i < got.size() || i < want.size(); ++i)
    {
        const std::string g = i < got.size() ? got[i] : "<missing>";
        const std::string w = i < want.size() ? want[i] : "<missing>";
        const bool match = (w == "<DATE>") ? !g.empty() && g != "<missing>" : g == w;
        if (!match)
        {
            ok = false;
            std::printf("  line %u: got  '%s'\n           want '%s'\n", unsigned(i + 1), g.c_str(), w.c_str());
        }
    }
    return ok;
}

// "<stamp> " where the stamp is CTimeDateStringNoSpaces: digits and '_' only.
static bool stampedLine(const std::string& line, const std::string& rest)
{
    const std::string::size_type space = line.find(' ');
    if (space == std::string::npos || space == 0)
        return false;
    if (line.find_first_not_of("0123456789_") != space)
        return false;
    return line.substr(space + 1) == rest;
}

int main()
{
    using namespace BF2VoipServer;
    namespace P = BF2VoipPacketData;

    // Scratch working directory with the log tree the server expects.
    char scratch[256];
#if defined(_WIN32)
    std::snprintf(scratch, sizeof(scratch), "voip_debug_scratch_%d", (int)_getpid());
    bf2_mkdir(scratch);
#else
    std::snprintf(scratch, sizeof(scratch), "/tmp/voip_debug_XXXXXX");
    if (!mkdtemp(scratch)) { std::printf("FAIL: mkdtemp\n"); return 1; }
#endif
    bf2_chdir(scratch);
    bf2_mkdir("mods"); bf2_mkdir("mods/bf2"); bf2_mkdir("mods/bf2/Logs"); bf2_mkdir(kLogDir);

    // ---- HandlePacketDebugRoutingData (ELF 0x84EFAB0) ----
    CBF2VoipServerInstance& instance = *new CBF2VoipServerInstance;
    char names[65][25];
    std::memset(names, 0, sizeof(names));
    std::strcpy(names[3], "Alice");
    std::strcpy(names[7], "Bob");
    std::strcpy(names[64], "Server");

    expect(instance.HandlePacketDebugRoutingData(&names[0][0], 100) == VOIP::VRESULT_INVALID_STATE,
           "debug routing data: wrong length is -4");
    expect(!instance.m_debugRouting.Enabled(), "debug routing data: wrong length starts nothing");
    expect(logFiles("VoipServerRouting_Instance_").empty(), "no routing log before a valid packet");

    expect(instance.HandlePacketDebugRoutingData(&names[0][0], sizeof(names)) == VOIP::VRESULT_SUCCESS,
           "debug routing data: 1625 bytes succeed");
    expect(instance.m_debugRouting.Enabled(), "debug routing data starts the routing dump");
    expect(std::strcmp(instance.m_playerNames[7], "Bob") == 0 && std::strcmp(instance.m_playerNames[64], "Server") == 0,
           "debug routing data copies the name table");
    expect(!instance.m_debugRouting.Start(), "routing Start: false while running");
    expect(instance.HandlePacketDebugRoutingData(0, sizeof(names)) == VOIP::VRESULT_SUCCESS,
           "debug routing data: null payload keeps the names");
    expect(std::strcmp(instance.m_playerNames[3], "Alice") == 0, "null payload copies nothing");

    const std::vector<std::string> routingLogs = logFiles("VoipServerRouting_Instance_");
    expect(routingLogs.size() == 1, "one routing log");
    if (!routingLogs.empty())
    {
        const std::string& n = routingLogs[0];
        const std::string stamp = n.substr(std::strlen("VoipServerRouting_Instance_"),
                                           n.size() - std::strlen("VoipServerRouting_Instance_") - 4);
        expect(n.size() > 4 && n.compare(n.size() - 4, 4, ".log") == 0, "routing log ends .log");
        expect(!stamp.empty() && stamp.find_first_not_of("0123456789_") == std::string::npos,
               "routing log stamp is digits and '_'");
    }

    // ---- OutputFullState / OutputRDMismatch (ELF 0x84F2BF0 / 0x84F29A0) ----
    instance.m_routing[64].m_flags = 0x40;          // server slot: team 1 commander
    instance.m_routing[3].m_flags  = 0xC5;          // team 2, commander, squad 5
    instance.m_routing[7].m_flags  = 0x23;          // team 1, squad leader, squad 3
    VOIP::ClientData client = VOIP::ClientData();
    client.m_sessionId     = 0x0102;
    client.m_connectionId  = 42;
    client.m_clientId      = 1;
    client.m_playerId      = 3;
    client.m_channel       = 1;
    client.m_outgoingSets.push_back(VOIP::ChannelArray(1, 0));
    client.m_outgoingSets.back().push_back(10);
    client.m_outgoingSets.push_back(VOIP::ChannelArray(1, 2));
    client.m_singleChannel = 1;
    client.m_multi         = 0;
    instance.m_clients.m_clients[0x0102] = client;
    instance.m_outgoingChannels.AddSessionIdToChannel(1, 0x0102);
    instance.m_outgoingChannels.AddSessionIdToChannel(1, 0x7777);   // no client: NAME NOT FOUND

    expect(instance.m_debugRouting.OutputFullState(&instance, 0x64), "OutputFullState returns true");
    P::RoutingDataPlayer before; before.m_flags = 0x23; before.m_reserved = 0; before.m_sessionId = 0;
    P::RoutingDataPlayer after;  after.m_flags  = 0xA3; after.m_reserved  = 0; after.m_sessionId  = 0;
    expect(instance.m_debugRouting.OutputRDMismatch(&instance, 7, before, after), "OutputRDMismatch returns true");
    expect(instance.m_debugRouting.OutputRDMismatch(&instance, 50, before, after), "OutputRDMismatch, unnamed channel");
    expect(instance.m_debugRouting.OutputFullState(0, 0x64), "OutputFullState(null) returns true");
    {
        CDebugRoutingState idle;
        expect(idle.OutputFullState(&instance, 0x64) && idle.OutputRDMismatch(&instance, 7, before, after),
               "a stopped dump writes nothing and returns true");
    }

    if (!routingLogs.empty())
    {
        const char* full[] = {
            "", "-----------------------------------------------------------------------------------------",
            "<DATE>",
            "PACKET: BF2_VOIP_PACKET_SET_PLAYER_DATA",
            "", "", "[ROUTING BLOCK]", "",
            "Player: 255, Server  Team: 1  Is Commander: 1  Is Squad Leader: 0  Squad Id: 0",
            "Player: 3, Alice  Team: 2  Is Commander: 1  Is Squad Leader: 0  Squad Id: 5",
            "Player: 7, Bob  Team: 1  Is Commander: 0  Is Squad Leader: 1  Squad Id: 3",
            "", "", "[CLIENT TABLES]", "",
            "",
            "Alice  IP: 2.1.0.0  ID: 3  Single Channel Set Selected: 1  Is Multi Channel Set Selected: 0",
            "  Incoming Channel:",
            "    CHANNEL_TEAM_1_SQUAD_A_LEADER",
            "  Outgoing Channels: ",
            "    Set 0",
            "      CHANNEL_TEAM_1_COMMANDER",
            "      CHANNEL_TEAM_1_SQUAD_A_MEMBER",
            "    Set 1",
            "      CHANNEL_TEAM_1_SQUAD_B_LEADER",
            "", "", "[OUTGOING CHANNEL TABLES]", "",
            "CHANNEL_TEAM_1_SQUAD_A_LEADER",
            "  Alice",
            "  NAME NOT FOUND",
            // OutputRDMismatch, named channel
            "", "-----------------------------------------------------------------------------------------",
            "<DATE>",
            "Routing Mismatch for Player: 7, Bob",
            "Last Routing Byte: 35",
            "New  Routing Byte: 163",
            // OutputRDMismatch, unnamed channel (names[50] is empty: the lookup returns "")
            "", "-----------------------------------------------------------------------------------------",
            "<DATE>",
            "Routing Mismatch for Player: 50, ",
            "Last Routing Byte: 35",
            "New  Routing Byte: 163",
        };
        expect(sameText(readLines(routingLogs[0]), std::vector<std::string>(full, full + sizeof(full) / sizeof(full[0]))),
               "routing log text");
    }

    // ---- CDebugVoiceTrace (ELF 0x84F3B20..0x84F4170) ----
    CDebugVoiceTrace& trace = instance.m_debugVoiceTrace;
    expect(!trace.IsRunning() && !trace.Stop(42), "voice trace idle: not running, Stop false");
    expect(trace.Output(42, 5, "Bob", "Alice", (char*)"xx", 2), "Output with no stream returns true");
    expect(trace.Start("Bob", 7, 42), "voice trace Start");
    expect(trace.IsRunning() && trace.m_connectionId == 42 && trace.m_frameCount == 0, "voice trace state after Start");
    expect(!trace.Start("Carol", 9, 43), "voice trace Start: false while running");

    char frame[6] = { 1, 2, (char)0xFE, (char)0xFF, 3, 4 };    // word at length-4: -2
    expect(trace.Output(1, 2, "X", "Y", frame, 6), "Output for another connection returns true");
    expect(trace.Output(42, 5, "Bob", 0, frame, 3), "Output, short packet");
    expect(trace.Output(5, 42, 0, "Bob", frame, 6), "Output, frame number");
    expect(trace.m_frameCount == 2, "two frames counted");
    expect(!trace.Stop(9), "Stop for another connection is false");
    expect(trace.Stop(42) && !trace.IsRunning() && trace.m_connectionId == 0 && trace.m_frameCount == 0,
           "Stop resets the trace");

    const std::vector<std::string> bob = logFiles("VoiceTraffic_Player_Bob_7_");
    expect(bob.size() == 1, "one voice-trace log for Bob");
    if (!bob.empty())
    {
        const std::vector<std::string> lines = readLines(bob[0]);
        expect(lines.size() == 2, "voice-trace log has two lines");
        if (lines.size() == 2)
        {
            expect(stampedLine(lines[0], "Voice Frame From: Bob To: NONAME Packet Size: 3 *** Packet Size less than 5 bytes ***"),
                   "short-packet line");
            expect(stampedLine(lines[1], "Voice Frame From: NONAME To: Bob Packet Size: 6 Frame: -2"),
                   "frame line");
        }
    }

    // A null name; the 30000th frame stops the trace.
    expect(trace.Start(0, 11, 77), "voice trace Start with no name");
    expect(logFiles("VoiceTraffic_Player_NO NAME_11_").size() == 1, "NO NAME log");
    trace.m_frameCount = 29998;
    expect(trace.Output(77, 1, "a", "b", frame, 6) && trace.IsRunning(), "frame 29999 keeps the trace");
    expect(trace.Output(77, 1, "a", "b", frame, 6) && !trace.IsRunning(), "frame 30000 stops the trace");

    // ---- CDebugStandard ----
    const std::string stamp = CDebugStandard::GetInstance()->CTimeDateStringNoSpaces();
    expect(!stamp.empty() && stamp.find_first_not_of("0123456789_") == std::string::npos,
           "CTimeDateStringNoSpaces: digits and '_' only");
    CDebugStandard::Close();

    std::printf("voip debug runtime %d/%d checks passed (scratch %s)\n", g_checks - g_failures, g_checks, scratch);
    if (!g_failures)
    {
        // The logs are kept for inspection only when a check failed.
        bf2_chdir("..");
#if defined(_WIN32)
        std::system((std::string("rmdir /s /q ") + scratch).c_str());
#else
        std::system((std::string("rm -rf '") + scratch + "'").c_str());
#endif
    }
    return g_failures ? 1 : 0;
}
