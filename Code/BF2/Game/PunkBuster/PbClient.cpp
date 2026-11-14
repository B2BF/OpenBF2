// PunkBuster client glue, BF2.exe 0x435900..0x436434 (client image only). [INFERRED] path and names;
// the ELF dedicated server carries only the server half (stPbSv), whose structure this mirrors.
#include "PbClient.h"
#include "PbGlue.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
const char kNotInstalled[] = "PunkBuster Client Not Installed";  // ds:0x893BC8
const char kLoadFailure[] = "PB Error: Client DLL Load Failure";  // ds:0x893B7C
const char kProcFailure[] = "PB Error: Client DLL Get Procedure Failure";  // ds:0x893B48

#if defined(_WIN32)
void* loadLibrary(const char* path) { return reinterpret_cast<void*>(LoadLibraryA(path)); }
void* librarySymbol(void* library, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
}
void releaseLibrary(void* library) { FreeLibrary(static_cast<HMODULE>(library)); }
int changeMode(const char* path, int mode) { return _chmod(path, mode); }
char* currentDirectory(char* buffer, int size) { return _getcwd(buffer, size); }
int compareNoCase(const char* a, const char* b) { return _stricmp(a, b); }
#else
void releaseLibrary(void*) {}
int changeMode(const char* path, int mode) { return chmod(path, static_cast<mode_t>(mode)); }
char* currentDirectory(char* buffer, int size) { return getcwd(buffer, static_cast<size_t>(size)); }
int compareNoCase(const char* a, const char* b) { return strcasecmp(a, b); }
#endif
} // namespace

#if defined(_WIN32)
PbClientLibraryApi g_pbClientLibrary = { loadLibrary, librarySymbol, releaseLibrary };
#else
PbClientLibraryApi g_pbClientLibrary = { 0, 0, releaseLibrary };
#endif

// The static image (ds:0x96E5A8..0x96E753): magic, the reload request, the prefix and state 999.
stPbCl pbcl = { 0x264B8BABu, 0, 0, 0, 1, { 0 }, "PunkBuster Client", { 0 },
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 999 };
void* pbClientInstance = 0;  // ds:0x99EF74, PbClientInitialize's argument [INFERRED] name

namespace {
// The exit destructor (0x873420): entry points cleared, both libraries released.
struct PbClientExit {
    ~PbClientExit()
    {
        pbcl.m_dllProcess = 0;
        pbcl.m_dllEvent = 0;
        pbcl.m_dllHook198 = 0;
        pbcl.m_dllHook19C = 0;
        pbcl.m_consoleHook = 0;
        if (pbcl.m_library) { g_pbClientLibrary.release(pbcl.m_library); pbcl.m_library = 0; }
        if (pbcl.m_library2) { g_pbClientLibrary.release(pbcl.m_library2); pbcl.m_library2 = 0; }
    }
} s_pbClientExit;

// 0x435900: "<fs_homepath>\pb\", or the working directory's when the game answers nothing.
void fillPath(stPbCl* pb)
{
    if (!pb->m_gameQuery) return;
    std::strcpy(pb->m_path, "fs_homepath");
    pb->m_gameQuery(0x67, pb->m_path);
    if (!pb->m_path[0]) currentDirectory(pb->m_path, 0xFF);
    if (pb->m_path[0] && pb->m_path[std::strlen(pb->m_path) - 1] != '\\') std::strcat(pb->m_path, "\\");
    std::strcat(pb->m_path, "pb\\");
}

// 0x4359A0
char* makeFileName(stPbCl* pb, char* out, const char* name)
{
    if (!pb->m_path[0]) fillPath(pb);
    std::strcpy(out, pb->m_path);
    std::strcat(out, name);
    return out;
}

// 0x435A00
void unloadDll(stPbCl* pb)
{
    pb->m_dllProcess = 0;
    pb->m_dllEvent = 0;
    pb->m_dllHook198 = 0;
    pb->m_dllHook19C = 0;
    pb->m_consoleHook = 0;
    if (pb->m_library) {
        g_pbClientLibrary.release(pb->m_library);
        pb->m_library = 0;
    }
}

// 0x435A40: install a downloaded pbclnew.dll as pbcl.dll (keeping pbclold.dll), then load it.
const char* loadDll(stPbCl* pb)
{
    if (pb->m_library) return 0;
    pb->m_dllProcess = 0;
    pb->m_dllEvent = 0;
    pb->m_dllHook198 = 0;
    pb->m_dllHook19C = 0;
    pb->m_consoleHook = 0;
    // [CHOSEN, not recovered] No vendor DLL outside Windows: fail as a missing pbcl.dll would,
    // before any file in the pb folder is touched.
    if (!g_pbClientLibrary.load) return kLoadFailure;
    char a[0x200], b[0x200];
    if (FILE* fresh = std::fopen(makeFileName(pb, a, "pbclnew.dll"), "rb")) {
        std::fclose(fresh);
        changeMode(makeFileName(pb, a, "pbclold.dll"), 0x180);
        std::remove(makeFileName(pb, a, "pbclold.dll"));
        std::rename(makeFileName(pb, a, "pbcl.dll"), makeFileName(pb, b, "pbclold.dll"));
        changeMode(makeFileName(pb, a, "pbcl.dll"), 0x180);
        std::remove(makeFileName(pb, a, "pbcl.dll"));
        std::rename(makeFileName(pb, a, "pbclnew.dll"), makeFileName(pb, b, "pbcl.dll"));
    }
    pb->m_library = g_pbClientLibrary.load(makeFileName(pb, a, "pbcl.dll"));
    if (!pb->m_library) return kLoadFailure;
    pb->m_dllProcess = reinterpret_cast<PbClProcessFn>(g_pbClientLibrary.symbol(pb->m_library, "ca"));
    pb->m_dllEvent = reinterpret_cast<PbClEventFn>(g_pbClientLibrary.symbol(pb->m_library, "cb"));
    if (!pb->m_dllProcess || !pb->m_dllEvent) {
        unloadDll(pb);
        return kProcFailure;
    }
    pb->m_reload = 0;
    return 0;
}

// The first failed load shows the not-installed notice once (state 999 -> 0).
void notifyNotInstalled(stPbCl* pb)
{
    if (pb->m_state == 999) {
        pb->m_state = 0;
        pb->m_gameMessage(const_cast<char*>(kNotInstalled), 1);
    }
}

// 0x436320
char* addPbEvent(stPbCl* pb, int type, int a, char* b, int c)
{
    if (!pb->m_gameCommand || !pb->m_state) return 0;
    if (pb->m_reload || !pb->m_library) {
        if (pb->m_library) {
            unloadDll(pb);
            return 0;
        }
        const char* error = loadDll(pb);
        if (error) {
            if (type == 0x71 || type == 0x72) return 0;
            notifyNotInstalled(pb);
            return const_cast<char*>(error);
        }
    }
    return reinterpret_cast<char*>(static_cast<std::intptr_t>(pb->m_dllEvent(pb, type, a, b, c)));
}

// 0x4363E0
int processPbEvents(stPbCl* pb, int argument)
{
    if (!pb->m_gameCommand || !pb->m_state) return 0;
    if (!pb->m_library) {
        if (pb->m_reload) addPbEvent(pb, 0x10, 0, const_cast<char*>(""), 0);
        return 0;
    }
    if (pb->m_reload) {
        unloadDll(pb);
        return 0;
    }
    return pb->m_dllProcess(pb, argument);
}

// Shared shape of 0x436060 and 0x436220: with the game callbacks installed and the glue enabled,
// (re)load as needed and send `type` to the DLL (initialize passes "", connect a null pointer).
void sendLifecycleEvent(stPbCl* pb, int type, char* data)
{
    if (!pb->m_gameCommand || !pb->m_state) return;
    if (pb->m_reload) {
        if (pb->m_library) {
            unloadDll(pb);
            return;
        }
    } else if (pb->m_library) {
        pb->m_dllEvent(pb, type, 0, data, 0);
        return;
    }
    if (loadDll(pb)) {
        if (pb->m_state != 999) return;
        pb->m_state = 0;
        pb->m_gameMessage(const_cast<char*>(kNotInstalled), 1);
        return;
    }
    pb->m_dllEvent(pb, type, 0, data, 0);
}

// ---- the game callbacks handed to pbcl.dll ------------------------------------------------------
// 0x435C50: the DLL's game commands; only set_cl_punkbuster is honoured. (Retail also walks the
// argument past its first word and discards the result.)
int gameCommand(char* command, char* args)
{
    if (!compareNoCase(command, "set_cl_punkbuster")) PB_set_cl_punkbuster(std::atoi(args));
    return 0;
}

// 0x435CC0: the DLL's queries, answered into its 256-byte buffer.
int gameQuery(int type, char* buffer)
{
    if (!buffer) return 0;
    buffer[0xFF] = 0;
    const char* value;
    switch (type) {
    case 0x67:
        if (!compareNoCase(buffer, "fs_homepath")) value = PB_homepath();
        else if (!compareNoCase(buffer, "fs_basepath")) value = PB_basepath();
        else if (!compareNoCase(buffer, "version")) value = PB_getVersion();
        else value = PB_examineSetting(buffer);
        break;
    case 0x68:
        value = PB_examineSetting(buffer);
        break;
    case 0x69:
        value = PB_examineSetting(const_cast<char*>("server"));
        break;
    default:
        buffer[0] = 0;
        buffer[0xFF] = 0;
        return 0;
    }
    std::strncpy(buffer, value, 0xFF);
    buffer[0xFF] = 0;
    return 0;
}

// 0x435DD0: "<prefix>: <text>\n" to the game's output (the flag is unused).
int gameMessage(char* text, int)
{
    char line[0x801];  // [CHOSEN] one byte past retail's 0x800, which the final "\n" can overrun
    std::strcpy(line, pbcl.m_gameName);
    std::strcat(line, ": ");
    const std::size_t used = std::strlen(line);
    std::strncpy(line + used, text, 0x7FF - used);
    line[0x7FF] = 0;
    std::strcat(line, "\n");
    PB_outputText(line);
    return 0;
}

int sendToServer(int length, char* data) { PB_cl_sendToServer(data, length); return 0; }            // 0x435EA0
int sendUdp(char* address, unsigned short port, int length, char* data)                              // 0x435EB0
{
    PB_sendUDPPacket(address, port, data, length);
    return 0;
}
int callback194(char* text) { return PB_GlQuery(text); }                                             // 0x435DC0
} // namespace

// ---- entry points --------------------------------------------------------------------------------
// 0x436060: record the instance, hand over the callbacks and send the initialize event (0x10);
// without a loaded DLL the profile's cl_punkbuster is switched off.
void PbClientInitialize(void* instance)
{
    pbClientInstance = instance;
    pbcl.m_serverGlue = PbServerGlueObject();
    pbcl.m_gameCommand = gameCommand;
    pbcl.m_gameQuery = gameQuery;
    pbcl.m_gameMessage = gameMessage;
    pbcl.m_sendToServer = sendToServer;
    pbcl.m_sendUdp = sendUdp;
    pbcl.m_callback194 = callback194;
    sendLifecycleEvent(&pbcl, 0x10, const_cast<char*>(""));
    if (!pbcl.m_library) PB_set_cl_punkbuster(0);
}

// 0x436220: the connect event (0x75), sent right after initialize when the server runs PunkBuster.
void PbClientConnect() { sendLifecycleEvent(&pbcl, 0x75, 0); }

// 0x436000
char* PbClAddEvent(int type, int length, char* data) { return addPbEvent(&pbcl, type, length, data, 0); }

// 0x436020: a PB_ console command for the DLL, then the DLL's own console hook if it set one.
void PbClientTrapConsole(char* command, int length)
{
    addPbEvent(&pbcl, 0x0E, length, command, 0);
    if (pbcl.m_consoleHook) pbcl.m_consoleHook(&pbcl, command, length);
}

// 0x436150: the per-frame pump.
void PbClientProcessEvents() { processPbEvents(&pbcl, 0); }
