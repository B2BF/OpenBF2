// [TEMP] [CHOSEN, not recovered] The PunkBuster server glue (Game/PunkBuster/PbServer.cpp) against a
// fake pbsv installed through the library seam, fake PB_* game functions and a scratch pb folder
// (CTests bf2_punkbuster_server_check, dedicated image, and bf2_punkbuster_server_client_check, client
// image). The fake library "loads" a file only when it exists, as the vendor loader would.
#include "../BF2/Game/PunkBuster/PbGlue.h"
#include "../BF2/Game/PunkBuster/PbServer.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

static unsigned checks, failures;
static void check(bool value, const std::string& label)
{
    ++checks;
    if (!value) { ++failures; std::printf("FAIL %s\n", label.c_str()); }
}
static std::vector<std::string> calls;
static std::string joined()
{
    std::string r;
    for (const std::string& c : calls) r += c + ";";
    return r;
}
static std::string text(const char* data, int length) { return data ? std::string(data, length) : "(null)"; }

// ---- fake PB_* game functions ------------------------------------------------------------------
static std::string s_home, s_base;
static int s_playerInfoResult = 1;
void PB_set_sv_punkbuster(int value) { calls.push_back("set_sv " + std::to_string(value)); }
void PB_issueConsoleCommand(char* command) { calls.push_back(std::string("console ") + command); }
void PB_DropClient(int index, char* reason) { calls.push_back("drop " + std::to_string(index) + " [" + reason + "]"); }
int PB_maxClients() { return 12; }
int PB_getPlayerInfo(int index, char* info)
{
    calls.push_back("playerInfo " + std::to_string(index));
    if (s_playerInfoResult) std::strcpy(info, "info");
    return s_playerInfoResult;
}
char* PB_homepath() { return const_cast<char*>(s_home.c_str()); }
char* PB_basepath() { return const_cast<char*>(s_base.c_str()); }
char* PB_getVersion() { return const_cast<char*>("1.5.3153-802.0"); }
char* PB_examineSetting(char* name) { calls.push_back(std::string("examine ") + name); return const_cast<char*>("setting"); }
void PB_outputText(char* line) { calls.push_back(std::string("out ") + line); }
void PB_sv_sendToClient(int index, char* data, int length)
{
    calls.push_back("toClient " + std::to_string(index) + " " + text(data, length));
}
void PB_sendUDPPacket(char* address, unsigned short port, char* data, int length)
{
    calls.push_back(std::string("udp ") + address + ":" + std::to_string(port) + " " + text(data, length));
}

// ---- the fake pbsv -----------------------------------------------------------------------------
static int s_library;  // its address is the handle
static bool s_exportProcess = true;
static int s_eventResult = 0;
static int fakeEvent(stPbSv* pb, int type, int index, int length, char* data, int flag)
{
    calls.push_back("sb " + std::string(pb == &pbsv ? "" : "BAD ") + std::to_string(type) + " " + std::to_string(index)
                    + " " + text(data, length) + " " + std::to_string(flag));
    return s_eventResult;
}
static int fakeProcess(stPbSv*, int argument) { calls.push_back("sa " + std::to_string(argument)); return 7; }
static void* fakeLoad(const char* path)
{
    calls.push_back(std::string("load ") + path);
    FILE* f = std::fopen(path, "rb");
    if (!f) return nullptr;
    std::fclose(f);
    return &s_library;
}
static void* fakeSymbol(void* library, const char* name)
{
    if (library != &s_library) return nullptr;
    if (!std::strcmp(name, "sa")) return s_exportProcess ? reinterpret_cast<void*>(&fakeProcess) : nullptr;
    if (!std::strcmp(name, "sb")) return reinterpret_cast<void*>(&fakeEvent);
    return nullptr;
}
static void fakeRelease(void* library) { calls.push_back(library == &s_library ? "release" : "release BAD"); }
static void fakeConnectString(stPbSv*, char* a, char* b) { calls.push_back(std::string("connect ") + a + " " + b); }
static char s_kick[] = "kicked";
static char* fakeAuth(stPbSv*, char* address, int flag, char* hash)
{
    calls.push_back(std::string("auth ") + address + " " + std::to_string(flag) + " " + hash);
    return s_kick;
}

static std::string readFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "(missing)";
    char buffer[64] = {};
    const std::size_t n = std::fread(buffer, 1, sizeof(buffer), f);
    std::fclose(f);
    return std::string(buffer, n);
}
static void writeFile(const std::string& path, const char* content)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fputs(content, f);
    std::fclose(f);
}
static bool exists(const std::string& path) { struct stat st; return stat(path.c_str(), &st) == 0; }

int main()
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    char scratch[] = "/tmp/pbsv_check_XXXXXX";
    if (!mkdtemp(scratch)) { std::printf("FAIL no scratch directory\n"); return 1; }
    s_home = std::string(scratch) + "/home";
    s_base = std::string(scratch) + "/base";
    mkdir(s_home.c_str(), 0700);
    mkdir(s_base.c_str(), 0700);
    mkdir((s_base + "/pb").c_str(), 0700);
    writeFile(s_base + "/pb/pbsv.so", "base pbsv");
    writeFile(s_base + "/pb/pbcl.so", "base pbcl");

    // ---- the static image ----
    check(pbsv.m_magic == 0x357AFE20u && !std::strcmp(pbsv.m_gameName, "PunkBuster Server") && pbsv.m_reload == 1
          && pbsv.m_state == 999 && !pbsv.m_library && !pbsv.m_gameCommand, "static image: magic, prefix, reload 1, state 999");
    check(PbServerGlueObject() == &pbsv, "the glue object is pbsv");

    // ---- before initialize: no callbacks, so nothing happens ----
    calls.clear();
    check(PbSvAddEvent(0x0D, 1, 2, const_cast<char*>("ab")) == nullptr && !isPbSvEnabled(), "before initialize: events answer 0");
    PbServerProcessEvents();
    check(joined().empty(), "before initialize: no load, no output");

    // ---- no vendor library on this platform (the default seam): the not-installed path ----
    PbServerInitialize();
    check(pbsv.m_gameCommand && pbsv.m_gameQuery && pbsv.m_gameMessage && pbsv.m_sendToClient && pbsv.m_sendToAddrPort,
          "initialize installs the five game callbacks");
    check(joined() == "out PunkBuster Server: PunkBuster Server Not Installed\n;set_sv 0;",
          "no library: the notice once, then sv_punkbuster switched off");
    check(pbsv.m_state == 0 && !pbsv.m_path[0], "no library: state 0, the pb folder untouched");
    calls.clear();
    check(PbSvAddEvent(0x0D, 1, 2, const_cast<char*>("ab")) == nullptr && !isPbSvEnabled() && joined().empty(),
          "state 0: the glue stays silent");

    // ---- a fake pbsv through the seam ----
    PbServerLibraryApi saved = g_pbServerLibrary;
    g_pbServerLibrary.load = fakeLoad;
    g_pbServerLibrary.symbol = fakeSymbol;
    g_pbServerLibrary.release = fakeRelease;
    pbsv.m_state = 999;
    calls.clear();
    // isPbSvEnabled loads first: the home pb folder is created and seeded from the base folder.
    s_eventResult = 1;
    check(isPbSvEnabled(), "status query 0x71 answered nonzero: enabled");
    const std::string home = s_home + "/pb/";
    check(!std::strcmp(pbsv.m_path, home.c_str()), "pb folder: <fs_homepath>/pb/");
    check(readFile(home + "pbsv.so") == "base pbsv" && readFile(home + "pbcl.so") == "base pbcl" && !exists(home + "pbag.so"),
          "home differs from base: pbsv/pbcl copied, missing pbag not created");
    check(joined() == "load " + home + "pbsv.so;sb 113 -1 (null) 1;", "load from the home folder, then event 0x71 with flag 1");
    check(pbsv.m_reload == 0 && pbsv.m_library == &s_library && pbsv.m_state == 999, "loaded: reload cleared, state kept");
    s_eventResult = 0;
    check(!isPbSvEnabled(), "status query answered 0: disabled");

    calls.clear();
    s_eventResult = 5;
    char* result = PbSvAddEvent(0x0D, 2, 4, const_cast<char*>("abcd"));
    check(reinterpret_cast<std::intptr_t>(result) == 5 && joined() == "sb 13 2 abcd 0;", "PbSvAddEvent forwards, answers pbsv's value");
    s_eventResult = 0;
    calls.clear();
    PbServerTrapConsole(const_cast<char*>("pb_sv_plist"), 11);
    PbServerCompleteCommand(const_cast<char*>("pb_sv_"), 6);
    EnablePbSv();
    DisablePbSv();
    PbSvAddStatEvent(3, const_cast<char*>("kills"), const_cast<char*>("12"));
    check(joined() == "sb 14 -1 pb_sv_plist 0;sb 51 -1 pb_sv_ 0;sb 117 -1 (null) 0;sb 118 -1 (null) 0;sb 15 3 kills 12 0;",
          "trap console 0xE, complete 0x33, enable 0x75, disable 0x76, stat event 0xF \"a b\"");
    std::string big(0x7FF, 'x');
    calls.clear();
    PbSvAddStatEvent(1, const_cast<char*>(big.c_str()), const_cast<char*>("y"));
    check(joined().size() == std::string("sb 15 1 ").size() + 0x801 + std::string(" 0;").size(), "stat event at 0x800 characters is sent");
    calls.clear();
    PbSvAddStatEvent(1, const_cast<char*>(big.c_str()), const_cast<char*>("yz"));
    check(joined().empty(), "stat event above 0x800 characters is dropped");
    calls.clear();
    PbServerProcessEvents();
    PbServerForceProcess();
    check(joined() == "sa 0;sa -1;", "process 0, forced process -1");

    // pbsv's own hooks.
    calls.clear();
    check(PbAuthClient(const_cast<char*>("1.2.3.4:5"), 1, const_cast<char*>("hash")) == nullptr, "no auth hook: admitted (0)");
    PbPassConnectString(const_cast<char*>("a"), const_cast<char*>("b"));
    check(joined().empty(), "no connect hook: nothing");
    pbsv.m_authClient = fakeAuth;
    pbsv.m_connectString = fakeConnectString;
    check(PbAuthClient(const_cast<char*>("1.2.3.4:5"), 1, const_cast<char*>("hash")) == s_kick, "auth hook: its kick text");
    PbPassConnectString(const_cast<char*>("a"), const_cast<char*>("b"));
    check(joined() == "auth 1.2.3.4:5 1 hash;connect a b;", "hooks receive their arguments");
    pbsv.m_authClient = nullptr;
    pbsv.m_connectString = nullptr;

    // A reload request unloads on the next event, which then answers 0; the one after reloads.
    pbsv.m_reload = 1;
    calls.clear();
    check(PbSvAddEvent(0x0D, 0, 1, const_cast<char*>("a")) == nullptr && !pbsv.m_library && !pbsv.m_dllEvent,
          "reload: the next event unloads and answers 0");
    PbServerProcessEvents();  // no library and reload set: the initialize event reloads it
    check(joined() == "release;load " + home + "pbsv.so;sb 16 -1  0;", "reload: the pump sends the initialize event, which loads");
    pbsv.m_reload = 1;
    calls.clear();
    PbServerProcessEvents();
    check(joined() == "release;" && !pbsv.m_library, "reload with a library: the pump unloads");

    // A downloaded pbsvnew replaces pbsv, keeping the previous one as pbsvold.
    writeFile(home + "pbsvnew.so", "new pbsv");
    calls.clear();
    PbSvAddEvent(0x0D, 0, 1, const_cast<char*>("a"));
    check(readFile(home + "pbsv.so") == "new pbsv" && readFile(home + "pbsvold.so") == "base pbsv" && !exists(home + "pbsvnew.so"),
          "pbsvnew.so installed as pbsv.so, the old one kept as pbsvold.so");
    check(joined() == "load " + home + "pbsv.so;sb 13 0 a 0;", "the event after the swap reaches the new library");

    // ---- the game callbacks ----
    calls.clear();
    pbsv.m_gameCommand(const_cast<char*>("SET_SV_PUNKBUSTER"), const_cast<char*>("1"));
    pbsv.m_gameCommand(const_cast<char*>("Cmd_Exec"), const_cast<char*>("PB_SV_PLIST"));
    pbsv.m_gameCommand(const_cast<char*>("Cmd_Exec"), const_cast<char*>("kick 3"));
    pbsv.m_gameCommand(const_cast<char*>("DropClient"), const_cast<char*>("  5   cheating badly"));
    pbsv.m_gameCommand(const_cast<char*>("unknown"), const_cast<char*>("x y"));
    check(joined() == "set_sv 1;console PB_SV_PLIST;sa -1;console kick 3;drop 5 [cheating badly];",
          "commands: set_sv_punkbuster, Cmd_Exec (pb_ forces a pass), DropClient <client> <reason>");
    char capture[16] = "";
    pbsv.m_gameCommand(const_cast<char*>("ConCapBufLen"), reinterpret_cast<char*>(static_cast<std::intptr_t>(14)));
    pbsv.m_gameCommand(const_cast<char*>("ConCapBuf"), capture);
#if DEDICATED
    check(pbsv.m_captureBuffer == capture && pbsv.m_captureCapacity == 14, "dedicated: the capture pair lives in pbsv +0x16C/+0x170");
#else
    check(!pbsv.m_captureBuffer && !pbsv.m_captureCapacity, "client: the capture pair lives outside pbsv");
#endif
    PbCaptureConsoleOutput(const_cast<char*>("line1"));
    PbCaptureConsoleOutput(const_cast<char*>("line2"));
    PbCaptureConsoleOutput(const_cast<char*>("x"));
    check(!std::strcmp(capture, "line1\nline2\n"), "capture: lines appended with \\n while they fit (\"x\" would reach 14)");

    char buffer[0x100];
    std::memset(buffer, 'q', sizeof(buffer));
    check(pbsv.m_gameQuery(0x65, buffer) == nullptr && !std::strcmp(buffer, "12"), "query 0x65: max clients in decimal");
    std::strcpy(buffer, "4");
    calls.clear();
    check(pbsv.m_gameQuery(0x66, buffer) == nullptr && !std::strcmp(buffer, "info") && joined() == "playerInfo 4;",
          "query 0x66: the player info of the index in the buffer");
    s_playerInfoResult = 0;
    std::strcpy(buffer, "9");
    char* error = pbsv.m_gameQuery(0x66, buffer);
    check(error && !std::strcmp(error, "PB Error: Query Failed"), "query 0x66 without the player: the error text");
    s_playerInfoResult = 1;
    const char* names[] = { "fs_homepath", "FS_BASEPATH", "version", "sv_hostname" };
    const std::string expected[] = { s_home, s_base, "1.5.3153-802.0", "setting" };
    for (int i = 0; i < 4; ++i) {
        std::strcpy(buffer, names[i]);
        pbsv.m_gameQuery(0x67, buffer);
        check(buffer == expected[i], std::string("query 0x67 ") + names[i]);
    }
    std::memset(buffer, 'z', sizeof(buffer));
    pbsv.m_gameQuery(0x99, buffer);
    check(buffer[0] == 'z' && buffer[0xFD] == 'z' && buffer[0xFE] == 0 && buffer[0xFF] == 'z',
          "unknown query: only byte 0xFE is cleared");

    calls.clear();
    pbsv.m_gameMessage(const_cast<char*>("hello"), 1);
    check(joined() == "out PunkBuster Server: hello\n;", "message: \"<prefix>: <text>\\n\"");
    calls.clear();
    pbsv.m_gameMessage(const_cast<char*>(std::string(0x900, 'm').c_str()), 0);
    check(calls.size() == 1 && calls[0].size() == 4 + 0x7FF + 1 && calls[0].back() == '\n', "long message: 0x7FF characters and \\n");
    calls.clear();
    check(pbsv.m_sendToClient(3, const_cast<char*>("abc"), 7) == 0
          && pbsv.m_sendToAddrPort(const_cast<char*>("5.6.7.8"), 9, 2, const_cast<char*>("de")) == 0
          && joined() == "toClient 7 abc;udp 5.6.7.8:9 de;", "send callbacks reorder their arguments for the PB_ functions");

    // ---- a library without "sa": the procedure failure, with the notice ----
    pbsv.m_reload = 1;
    PbSvAddEvent(0x0D, 0, 1, const_cast<char*>("a"));  // unloads
    s_exportProcess = false;
    calls.clear();
    error = PbSvAddEvent(0x0D, 0, 1, const_cast<char*>("a"));
    check(error && !std::strcmp(error, "PB Error: Server DLL Get Procedure Failure") && !pbsv.m_library && !pbsv.m_dllEvent,
          "missing \"sa\": released, the procedure failure text");
    check(joined() == "load " + home + "pbsv.so;release;out PunkBuster Server: PunkBuster Server Not Installed\n;",
          "missing \"sa\": the notice once");

    g_pbServerLibrary = saved;
    std::string cleanup = std::string("rm -rf ") + scratch;
    std::system(cleanup.c_str());
    std::printf("punkbuster server runtime %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
