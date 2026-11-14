// [TEMP] [CHOSEN, not recovered] PunkBuster client glue and command hook against a fake pbcl.dll
// (CTest bf2_punkbuster_client_check). The actual PbClient.cpp and PbCommandHook.cpp run; the vendor
// DLL ("ca"/"cb" through the library seam), the BF2-side PB_* functions and the server glue are fakes
// that record what the glue asks of them. Files are written only under a fresh temporary directory.
#include "../BF2/Game/PunkBuster/PbClient.h"
#include "../BF2/Game/PunkBuster/PbGlue.h"
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

// ---- fake BF2-side game functions and server glue -------------------------------------------
static char s_home[0x200];
static char s_setting[0x100] = "answer";
char* PB_basepath() { calls.push_back("basepath"); return const_cast<char*>("base-dir"); }
char* PB_homepath() { calls.push_back("homepath"); return s_home; }
void PB_set_cl_punkbuster(int v) { calls.push_back("set_cl_punkbuster " + std::to_string(v)); }
void PB_outputText(char* text) { calls.push_back(std::string("output ") + text); }
void PB_cl_sendToServer(char* data, int length) { calls.push_back("toServer " + std::string(data, length)); }
void PB_sendUDPPacket(char* address, unsigned short port, char* data, int length)
{
    calls.push_back(std::string("udp ") + address + ":" + std::to_string(port) + " " + std::string(data, length));
}
char* PB_getVersion() { calls.push_back("version"); return const_cast<char*>("v1.50"); }
int PB_GlQuery(char*) { calls.push_back("cb194"); return 0; }
char* PB_examineSetting(char* name) { calls.push_back(std::string("examine ") + name); return s_setting; }
static unsigned char s_serverGlue[4];
void PbServerTrapConsole(char* command, int length) { calls.push_back("svtrap " + std::string(command, length)); }
void* PbServerGlueObject() { return s_serverGlue; }

// ---- fake pbcl.dll ---------------------------------------------------------------------------
static bool s_missingCb;
static int s_loads, s_releases;
static std::string s_loadedPath;
static void fakeConsoleHook(stPbCl*, char* command, int length) { calls.push_back("dllConsole " + std::string(command, length)); }
static int fakeCb(stPbCl* pb, int type, int a, char* b, int c)
{
    calls.push_back("cb " + std::to_string(type) + " " + std::to_string(a) + " " + (b ? std::string(b) : "null")
                    + " " + std::to_string(c) + (pb == &pbcl ? "" : " other"));
    if (type == 0x10) pb->m_consoleHook = fakeConsoleHook;  // the DLL installs its console hook
    return 7;
}
static int fakeCa(stPbCl* pb, int x) { calls.push_back("ca " + std::to_string(x) + (pb == &pbcl ? "" : " other")); return 3; }
static int s_libraryToken;
static void* fakeLoad(const char* path) { ++s_loads; s_loadedPath = path; return &s_libraryToken; }
static void* fakeSymbol(void* library, const char* name)
{
    calls.push_back(std::string("symbol ") + name + (library == &s_libraryToken ? "" : " other"));
    if (!std::strcmp(name, "ca")) return reinterpret_cast<void*>(fakeCa);
    if (!std::strcmp(name, "cb")) return s_missingCb ? nullptr : reinterpret_cast<void*>(fakeCb);
    return nullptr;
}
static void fakeRelease(void* library) { ++s_releases; calls.push_back(library == &s_libraryToken ? "release" : "release other"); }

static void resetGlue(int state)
{
    if (pbcl.m_library) pbcl.m_library = nullptr;
    pbcl.m_reload = 1;
    pbcl.m_state = state;
    pbcl.m_dllEvent = nullptr;
    pbcl.m_dllProcess = nullptr;
    pbcl.m_consoleHook = nullptr;
    calls.clear();
}
static std::string readFile(const std::string& path)
{
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return "<missing>";
    char buffer[64] = {};
    const size_t n = std::fread(buffer, 1, sizeof(buffer) - 1, f);
    std::fclose(f);
    return std::string(buffer, n);
}
static void writeFile(const std::string& path, const char* text)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    std::fputs(text, f);
    std::fclose(f);
}

int main()
{
    // ---- static image ----
    check(pbcl.m_magic == 0x264B8BABu && pbcl.m_reload == 1 && pbcl.m_state == 999, "static image: magic, reload, 999");
    check(!std::strcmp(pbcl.m_gameName, "PunkBuster Client"), "static image: message prefix");

    // ---- the command hook, before any glue exists ----
    check(!HandleCommandHookPB("PB", 2), "hook: shorter than PB_ is not PB");
    check(!HandleCommandHookPB("PX_foo", 6) && !HandleCommandHookPB("PBXfoo", 6), "hook: other prefixes fall through");
    calls.clear();
    check(HandleCommandHookPB("pb_sv_ban 3", 11), "hook: pb_sv_ (any case) is a PB command");
    check(joined() == "svtrap pb_sv_ban 3;", "hook: PB_SV_ goes to the server trap only");
    calls.clear();
    check(HandleCommandHookPB("PB_SVX", 6) && HandleCommandHookPB("PB_s", 4), "hook: other PB_ commands are taken");
    check(joined().empty(), "hook: client trap before initialize sends nothing (no game callbacks)");

    // ---- no vendor DLL (the macOS product path): the not-installed notice, cl_punkbuster off ----
    g_pbClientLibrary.load = nullptr;
    resetGlue(999);
    PbClientInitialize(nullptr);
    check(pbcl.m_gameCommand && pbcl.m_gameQuery && pbcl.m_gameMessage && pbcl.m_sendToServer && pbcl.m_sendUdp
          && pbcl.m_callback194, "initialize installs the six game callbacks");
    check(pbcl.m_serverGlue == s_serverGlue, "initialize records the server glue object");
    check(joined() == "output PunkBuster Client: PunkBuster Client Not Installed\n;set_cl_punkbuster 0;",
          "no DLL: one not-installed notice, then cl_punkbuster 0");
    check(pbcl.m_state == 0, "no DLL: state 999 -> 0");
    calls.clear();
    PbClientConnect();
    PbClAddEvent(0x0D, 3, const_cast<char*>("abc"));
    PbClientProcessEvents();
    check(joined().empty(), "no DLL: state 0 silences every later event");

    // ---- a fake DLL, with a downloaded pbclnew.dll to install ----
    char tmpl[] = "/tmp/bf2pbXXXXXX";
    const std::string root = mkdtemp(tmpl);
    const std::string work = root + "/work";
    mkdir(work.c_str(), 0700);
    check(chdir(work.c_str()) == 0, "temporary working directory");
    // fs_homepath answers empty, so the glue falls back to the working directory: "<cwd>\pb\".
    s_home[0] = 0;
    char cwd[0x200];
    getcwd(cwd, sizeof(cwd));
    const std::string base = std::string(cwd) + "\\pb\\";
    writeFile(base + "pbcl.dll", "old");
    writeFile(base + "pbclnew.dll", "new");
    g_pbClientLibrary.load = fakeLoad;
    g_pbClientLibrary.symbol = fakeSymbol;
    g_pbClientLibrary.release = fakeRelease;
    pbcl.m_path[0] = 0;
    resetGlue(999);
    PbClientInitialize(&s_loads);
    check(s_loadedPath == base + "pbcl.dll", "loads <home>\\pb\\pbcl.dll (" + s_loadedPath + ")");
    check(readFile(base + "pbcl.dll") == "new" && readFile(base + "pbclold.dll") == "old"
          && readFile(base + "pbclnew.dll") == "<missing>", "pbclnew.dll installed as pbcl.dll, old kept as pbclold.dll");
    check(joined() == "homepath;symbol ca;symbol cb;cb 16 0  0;", "initialize: path query, ca then cb, event 0x10 with \"\"");
    check(pbcl.m_reload == 0 && pbcl.m_library == &s_libraryToken, "a loaded DLL clears the reload request");

    calls.clear();
    PbClientConnect();
    check(joined() == "cb 117 0 null 0;", "connect: event 0x75 with a null pointer");
    calls.clear();
    char* result = PbClAddEvent(0x0D, 5, const_cast<char*>("hello"));
    check(joined() == "cb 13 5 hello 0;" && result == reinterpret_cast<char*>(7), "PbClAddEvent forwards to cb and returns its result");
    calls.clear();
    check(HandleCommandHookPB("PB_SCREEN", 9), "hook: client PB_ command");
    check(joined() == "cb 14 9 PB_SCREEN 0;dllConsole PB_SCREEN;", "client trap: event 0x0E, then the DLL's console hook");
    calls.clear();
    PbClientProcessEvents();
    check(joined() == "ca 0;", "the frame pump calls ca(this, 0)");

    // ---- the game callbacks, as the DLL calls them ----
    char buffer[0x100];
    calls.clear();
    std::strcpy(buffer, "version"); pbcl.m_gameQuery(0x67, buffer);
    check(!std::strcmp(buffer, "v1.50") && joined() == "version;", "query 0x67 version");
    std::strcpy(buffer, "fs_basepath"); pbcl.m_gameQuery(0x67, buffer);
    check(!std::strcmp(buffer, "base-dir"), "query 0x67 fs_basepath");
    calls.clear();
    std::strcpy(buffer, "sv_hostname"); pbcl.m_gameQuery(0x67, buffer);
    check(!std::strcmp(buffer, "answer") && joined() == "examine sv_hostname;", "query 0x67 other: PB_examineSetting(name)");
    calls.clear();
    std::strcpy(buffer, "mapname"); pbcl.m_gameQuery(0x68, buffer);
    check(joined() == "examine mapname;" && !std::strcmp(buffer, "answer"), "query 0x68: PB_examineSetting(buffer)");
    calls.clear();
    std::strcpy(buffer, "ignored"); pbcl.m_gameQuery(0x69, buffer);
    check(joined() == "examine server;", "query 0x69: PB_examineSetting(\"server\")");
    std::strcpy(buffer, "kept?"); pbcl.m_gameQuery(0x70, buffer);
    check(buffer[0] == 0, "other queries answer empty");
    check(pbcl.m_gameQuery(0x67, nullptr) == 0, "a null buffer is ignored");
    calls.clear();
    pbcl.m_gameCommand(const_cast<char*>("SET_CL_PUNKBUSTER"), const_cast<char*>("1"));
    pbcl.m_gameCommand(const_cast<char*>("kick"), const_cast<char*>("2"));
    check(joined() == "set_cl_punkbuster 1;", "game command: only set_cl_punkbuster (any case)");
    calls.clear();
    pbcl.m_gameMessage(const_cast<char*>("hello"), 0);
    check(joined() == "output PunkBuster Client: hello\n;", "message: prefix, \": \", text, newline");
    calls.clear();
    pbcl.m_sendToServer(3, const_cast<char*>("abc"));
    pbcl.m_sendUdp(const_cast<char*>("1.2.3.4"), 99, 3, const_cast<char*>("xyz"));
    check(pbcl.m_callback194(const_cast<char*>("x")) == 0, "callback +0x194 answers 0");
    check(joined() == "toServer abc;udp 1.2.3.4:99 xyz;cb194;", "send callbacks forward length and data in order");

    // ---- reload: the next event unloads, the one after loads again (no swap without pbclnew) ----
    pbcl.m_reload = 1;
    calls.clear();
    check(PbClAddEvent(0x0D, 1, const_cast<char*>("a")) == nullptr && pbcl.m_library == nullptr, "reload: event unloads");
    check(joined() == "release;" && !pbcl.m_dllEvent && !pbcl.m_dllProcess && !pbcl.m_consoleHook, "unload clears the entry points");
    calls.clear();
    PbClAddEvent(0x0D, 1, const_cast<char*>("b"));
    check(joined() == "symbol ca;symbol cb;cb 13 1 b 0;" && readFile(base + "pbcl.dll") == "new", "next event loads and delivers");
    pbcl.m_reload = 1;
    calls.clear();
    PbClientProcessEvents();
    check(joined() == "release;", "pump with a reload request unloads");
    calls.clear();
    PbClientProcessEvents();
    check(joined() == "symbol ca;symbol cb;cb 16 0  0;", "pump without a DLL but reloading sends 0x10 (which loads)");

    // ---- a DLL without "cb": unload, the procedure error and the notice once ----
    resetGlue(999);
    s_missingCb = true;
    char* error = PbClAddEvent(0x0D, 0, const_cast<char*>(""));
    check(error && !std::strcmp(error, "PB Error: Client DLL Get Procedure Failure"), "missing cb: procedure failure");
    check(joined() == "symbol ca;symbol cb;release;output PunkBuster Client: PunkBuster Client Not Installed\n;",
          "missing cb: unloaded, notice shown");
    resetGlue(999);
    check(PbClAddEvent(0x71, 0, const_cast<char*>("")) == nullptr && pbcl.m_state == 999,
          "types 0x71/0x72 fail silently and keep the notice pending");
    s_missingCb = false;

    // clean up the temporary tree (the glue's files are named "work\pb\..." beside "work")
    for (const char* name : { "pbcl.dll", "pbclold.dll", "pbclnew.dll" }) std::remove((base + name).c_str());
    chdir("/");
    rmdir(work.c_str());
    rmdir(root.c_str());

    std::printf("punkbuster client runtime %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
