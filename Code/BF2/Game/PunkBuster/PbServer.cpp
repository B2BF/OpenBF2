// PunkBuster server glue: BF2.exe 0x436440..0x437349 and the ELF dedicated server 0x84F6020..0x84F7340
// (whose symbols give the names; the stPbSv members, PbCopyFile and ltoa are weak there, so inline in
// the vendor SDK header the glue was built from). Both images, one body: they differ only in the vendor library's file
// names (pbsv.dll / pbsv.so, '\' / '/', chmod 0x180 / 0x1FF), which follow the platform, and in where
// the console capture buffer lives, which follows the image. [INFERRED] path.
#include "PbServer.h"
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
const char kNotInstalled[] = "PunkBuster Server Not Installed";              // ds:0x893D14, ELF 0x88E8440
const char kLoadFailure[] = "PB Error: Server DLL Load Failure";             // ds:0x893CD8, ELF 0x88E8460
const char kProcFailure[] = "PB Error: Server DLL Get Procedure Failure";    // ds:0x893CA4, ELF 0x88E84A0
const char kQueryFailed[] = "PB Error: Query Failed";                        // ds:0x893C3C, ELF 0x88E83CA

#if defined(_WIN32)
const char kSeparator[] = "\\";
const char kPbFolder[] = "pb\\";
const char kLibrary[] = "pbsv.dll";
const char kLibraryOld[] = "pbsvold.dll";
const char kLibraryNew[] = "pbsvnew.dll";
const char kClientLibrary[] = "pbcl.dll";
const char kAgentLibrary[] = "pbag.dll";
const int kLibraryMode = 0x180;
void* loadLibrary(const char* path) { return reinterpret_cast<void*>(LoadLibraryA(path)); }
void* librarySymbol(void* library, const char* name)
{
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(library), name));
}
void releaseLibrary(void* library) { FreeLibrary(static_cast<HMODULE>(library)); }
int changeMode(const char* path, int mode) { return _chmod(path, mode); }
int makeDirectory(const char* path) { return _mkdir(path); }
char* currentDirectory(char* buffer, int size) { return _getcwd(buffer, size); }
int compareNoCase(const char* a, const char* b) { return _stricmp(a, b); }
int compareNoCaseN(const char* a, const char* b, std::size_t n) { return _strnicmp(a, b, n); }
void longToText(long value, char* out) { _ltoa(value, out, 10); }
#else
const char kSeparator[] = "/";
const char kPbFolder[] = "pb/";
const char kLibrary[] = "pbsv.so";
const char kLibraryOld[] = "pbsvold.so";
const char kLibraryNew[] = "pbsvnew.so";
const char kClientLibrary[] = "pbcl.so";
const char kAgentLibrary[] = "pbag.so";
const int kLibraryMode = 0x1FF;
void releaseLibrary(void*) {}
int changeMode(const char* path, int mode) { return chmod(path, static_cast<mode_t>(mode)); }
int makeDirectory(const char* path) { return mkdir(path, 0x1FF); }
char* currentDirectory(char* buffer, int size) { return getcwd(buffer, static_cast<size_t>(size)); }
int compareNoCase(const char* a, const char* b) { return strcasecmp(a, b); }
int compareNoCaseN(const char* a, const char* b, std::size_t n) { return strncasecmp(a, b, n); }
// The ELF carries its own ltoa (0x84F6940); the server asks it for base 10 only.
void longToText(long value, char* out) { std::sprintf(out, "%ld", value); }
#endif
} // namespace

#if defined(_WIN32)
PbServerLibraryApi g_pbServerLibrary = { loadLibrary, librarySymbol, releaseLibrary };
#else
PbServerLibraryApi g_pbServerLibrary = { 0, 0, releaseLibrary };
#endif

// The static image: magic, the prefix, the reload request and state 999 (BF2.exe ds:0x96E758..,
// written by the ELF's static initializer 0x84F67C0).
stPbSv pbsv = { 0x357AFE20u, 0, 0, 0, 0, "PunkBuster Server", { 0 }, 1,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 999, 0, 0 };

namespace {
#if !DEDICATED
// BF2.exe keeps the console capture pair outside the object (the ELF uses pbsv +0x16C / +0x170).
char* s_captureBuffer = 0;   // ds:0x99EF78
int s_captureCapacity = 0;   // ds:0x99EF7C
char*& captureBuffer() { return s_captureBuffer; }
int& captureCapacity() { return s_captureCapacity; }
#else
char*& captureBuffer() { return pbsv.m_captureBuffer; }
int& captureCapacity() { return pbsv.m_captureCapacity; }
#endif

// The exit destructor (BF2.exe 0x873470, ELF __tcf_0 0x84F68A0): entry points cleared, the three
// libraries released.
struct PbServerExit {
    ~PbServerExit()
    {
        pbsv.m_dllProcess = 0;
        pbsv.m_dllEvent = 0;
        if (pbsv.m_library) g_pbServerLibrary.release(pbsv.m_library);
        pbsv.m_library = 0;
        if (pbsv.m_library2) g_pbServerLibrary.release(pbsv.m_library2);
        pbsv.m_library2 = 0;
        pbsv.m_dllHook164 = 0;
        if (pbsv.m_library3) g_pbServerLibrary.release(pbsv.m_library3);
        pbsv.m_library3 = 0;
    }
} s_pbServerExit;

// BF2.exe 0x436440, ELF 0x84F7200: copy src to dst when it is non-empty and below `limit` (0: any
// size); true when every byte was written.
bool PbCopyFile(char* source, char* destination, long limit)
{
    bool copied = false;
    FILE* in = std::fopen(source, "rb");
    if (!in) return false;
    if (FILE* out = std::fopen(destination, "wb")) {
        std::fseek(in, 0, SEEK_END);
        const long size = std::ftell(in);
        if (size > 0 && (!limit || size < limit)) {
            if (char* buffer = new char[size]) {
                std::fseek(in, 0, SEEK_SET);
                const std::size_t read = std::fread(buffer, 1, static_cast<std::size_t>(size), in);
                const std::size_t written = std::fwrite(buffer, 1, read, out);
                delete[] buffer;
                copied = written == static_cast<std::size_t>(size);
            }
        }
        std::fclose(out);
    }
    std::fclose(in);
    return copied;
}

void appendSeparator(char* path)
{
    if (path[std::strlen(path) - 1] != kSeparator[0]) std::strcat(path, kSeparator);
}

// BF2.exe 0x436520 (inlined in the ELF): copy <base><name> into the pb folder unless it is there.
void copyIfMissing(stPbSv* pb, const char* name, const char* base)
{
    char target[0x200];
    std::strcpy(target, pb->m_path);
    std::strcat(target, name);
    if (FILE* present = std::fopen(target, "rb")) {
        std::fclose(present);
        return;
    }
    char source[0x200];
    std::strcpy(source, base);
    std::strcat(source, name);
    PbCopyFile(source, target, 0);
}

// BF2.exe 0x436630 (inlined in the ELF): "<fs_homepath>/pb/", or the working directory's when the game
// answers nothing.
void fillPath(stPbSv* pb)
{
    if (!pb->m_gameQuery) return;
    std::strcpy(pb->m_path, "fs_homepath");
    pb->m_gameQuery(0x67, pb->m_path);
    if (!pb->m_path[0]) currentDirectory(pb->m_path, 0xFB);
    if (pb->m_path[0]) appendSeparator(pb->m_path);
    std::strcat(pb->m_path, kPbFolder);
}

// BF2.exe 0x4366D0, ELF stPbSv::makefn 0x84F6E10. The first call fixes the pb folder; when the home
// and base folders differ it is created and seeded with the base folder's three libraries.
char* makefn(stPbSv* pb, char* out, const char* name)
{
    if (!pb->m_path[0]) {
        fillPath(pb);
        // [CHOSEN, not recovered] starts empty: retail leaves the buffer unset when no query
        // callback exists (unreachable: PbServerInitialize installs it before any event).
        char base[0x100] = "";
        if (pb->m_gameQuery) {
            std::strcpy(base, "fs_basepath");
            pb->m_gameQuery(0x67, base);
        }
        if (base[0]) appendSeparator(base);  // [CHOSEN] guard: retail reads base[-1] when empty
        std::strcat(base, kPbFolder);
        if (compareNoCase(base, pb->m_path) && base[0] && pb->m_path[0]) {
            makeDirectory(pb->m_path);
            copyIfMissing(pb, kLibrary, base);
            copyIfMissing(pb, kClientLibrary, base);
            copyIfMissing(pb, kAgentLibrary, base);
        }
    }
    std::strcpy(out, pb->m_path);
    std::strcat(out, name);
    return out;
}

// BF2.exe 0x436830
void unloadDll(stPbSv* pb)
{
    pb->m_dllProcess = 0;
    pb->m_dllEvent = 0;
    if (pb->m_library) g_pbServerLibrary.release(pb->m_library);
    pb->m_library = 0;
}

// BF2.exe 0x436860, ELF stPbSv::LoadServerDll 0x84F6BD0: install a downloaded pbsvnew as pbsv
// (keeping pbsvold), then load it.
const char* loadServerDll(stPbSv* pb)
{
    if (pb->m_library) return 0;
    pb->m_dllProcess = 0;
    pb->m_dllEvent = 0;
    // [CHOSEN, not recovered] No vendor library on these builds: fail as a missing pbsv would, before
    // any file in the pb folder is touched (see PbServerLibraryApi).
    if (!g_pbServerLibrary.load) return kLoadFailure;
    char a[0x200], b[0x200];
    if (FILE* fresh = std::fopen(makefn(pb, a, kLibraryNew), "rb")) {
        std::fclose(fresh);
        changeMode(makefn(pb, a, kLibraryOld), kLibraryMode);
        std::remove(makefn(pb, a, kLibraryOld));
        std::rename(makefn(pb, a, kLibrary), makefn(pb, b, kLibraryOld));
        changeMode(makefn(pb, a, kLibrary), kLibraryMode);
        std::remove(makefn(pb, a, kLibrary));
        std::rename(makefn(pb, a, kLibraryNew), makefn(pb, b, kLibrary));
    }
    pb->m_library = g_pbServerLibrary.load(makefn(pb, a, kLibrary));
    if (!pb->m_library) return kLoadFailure;
    pb->m_dllProcess = reinterpret_cast<PbSvProcessFn>(g_pbServerLibrary.symbol(pb->m_library, "sa"));
    pb->m_dllEvent = reinterpret_cast<PbSvEventFn>(g_pbServerLibrary.symbol(pb->m_library, "sb"));
    if (!pb->m_dllProcess || !pb->m_dllEvent) {
        unloadDll(pb);
        return kProcFailure;
    }
    pb->m_reload = 0;
    return 0;
}

// BF2.exe 0x437230, ELF stPbSv::AddPbEvent 0x84F6A10: an event for pbsv, (re)loading it as needed. A
// failed load answers its error text; the first one also shows the not-installed notice, except for the
// two status queries 0x71 / 0x72.
char* addPbEvent(stPbSv* pb, int type, int clientIndex, int length, char* data, int flag)
{
    if (!pb->m_gameCommand || !pb->m_state) return 0;
    if (pb->m_reload || !pb->m_library) {
        if (pb->m_library) {
            unloadDll(pb);
            return 0;
        }
        const char* error = loadServerDll(pb);
        if (error) {
            if (type == 0x71 || type == 0x72) return 0;
            if (pb->m_state == 999) {
                pb->m_state = 0;
                pb->m_gameMessage(const_cast<char*>(kNotInstalled), 1);
            }
            return const_cast<char*>(error);
        }
    }
    return reinterpret_cast<char*>(static_cast<std::intptr_t>(
        pb->m_dllEvent(pb, type, clientIndex, length, data, flag)));
}

// BF2.exe 0x4372F0, ELF stPbSv::ProcessPbEvents 0x84F6B20
int processPbEvents(stPbSv* pb, int argument)
{
    if (!pb->m_gameCommand || !pb->m_state) return 0;
    if (pb->m_library) {
        if (pb->m_reload) {
            unloadDll(pb);
            return 0;
        }
        return pb->m_dllProcess(pb, argument);
    }
    if (pb->m_reload) addPbEvent(pb, 0x10, -1, 0, const_cast<char*>(""), 0);
    return 0;
}

// ---- the game callbacks handed to pbsv ------------------------------------------------------------
// BF2.exe 0x437040, ELF PbSvGameCommand 0x84F6430. "ConCapBufLen" passes its number in the pointer.
int gameCommand(char* command, char* args)
{
    if (!compareNoCase(command, "set_sv_punkbuster")) {
        PB_set_sv_punkbuster(std::atoi(args));
    } else if (!compareNoCase(command, "ConCapBufLen")) {
        captureCapacity() = static_cast<int>(reinterpret_cast<std::intptr_t>(args));
    } else if (!compareNoCase(command, "ConCapBuf")) {
        captureBuffer() = args;
    } else if (!compareNoCase(command, "Cmd_Exec")) {
        const bool pbCommand = !compareNoCaseN(args, "pb_", 3);
        PB_issueConsoleCommand(args);
        if (pbCommand) PbServerForceProcess();
    } else {
        // "<client> <reason>": the reason starts after the first word and its spaces.
        char* reason = args;
        while (*reason == ' ') ++reason;
        while (*reason && *reason != ' ') ++reason;
        while (*reason == ' ') ++reason;
        if (!compareNoCase(command, "DropClient")) PB_DropClient(std::atoi(args), reason);
    }
    return 0;
}

// BF2.exe 0x436B30, ELF PbSvGameQuery 0x84F6580: pbsv's queries, answered into its 256-byte buffer.
// (The ELF tests the buffer for null only after writing through it.)
char* gameQuery(int type, char* buffer)
{
    buffer[0xFE] = 0;
    switch (type) {
    case 0x65:
        longToText(PB_maxClients(), buffer);
        break;
    case 0x66:
        if (!PB_getPlayerInfo(std::atoi(buffer), buffer)) return const_cast<char*>(kQueryFailed);
        break;
    case 0x67: {
        const char* value;
        if (!compareNoCase(buffer, "fs_homepath")) value = PB_homepath();
        else if (!compareNoCase(buffer, "fs_basepath")) value = PB_basepath();
        else if (!compareNoCase(buffer, "version")) value = PB_getVersion();
        else value = PB_examineSetting(buffer);
        std::strncpy(buffer, value, 0xFF);
        break;
    }
    default:
        break;
    }
    buffer[0xFE] = 0;
    return 0;
}

// BF2.exe 0x436C50, ELF PbSvGameMsg 0x84F6670: "<prefix>: <text>\n" to the game's output (the flag is
// unused).
int gameMessage(char* text, int)
{
    char line[0x801];  // [CHOSEN] one byte past retail's 0x800, which the final "\n" can overrun
    std::strcpy(line, pbsv.m_gameName);
    std::strcat(line, ": ");
    const std::size_t used = std::strlen(line);
    std::strncpy(line + used, text, 0x7FF - used);
    line[0x7FF] = 0;
    std::strcat(line, "\n");
    PB_outputText(line);
    return 0;
}

// BF2.exe 0x436D20, ELF PbSvSendToClient 0x84F6760
int sendToClient(int length, char* data, int clientIndex)
{
    PB_sv_sendToClient(clientIndex, data, length);
    return 0;
}

// BF2.exe 0x436D40, ELF PbSvSendToAddrPort 0x84F6790
int sendToAddrPort(char* address, unsigned short port, int length, char* data)
{
    PB_sendUDPPacket(address, port, data, length);
    return 0;
}
} // namespace

// ---- entry points --------------------------------------------------------------------------------
// BF2.exe 0x437160, ELF 0x84F6170: hand over the callbacks and send the initialize event (0x10);
// without a loaded pbsv the server's PunkBuster setting is switched off.
void PbServerInitialize()
{
    pbsv.m_gameCommand = gameCommand;
    pbsv.m_gameQuery = gameQuery;
    pbsv.m_gameMessage = gameMessage;
    pbsv.m_sendToClient = sendToClient;
    pbsv.m_sendToAddrPort = sendToAddrPort;
    addPbEvent(&pbsv, 0x10, -1, 0, const_cast<char*>(""), 0);
    if (!pbsv.m_dllEvent) PB_set_sv_punkbuster(0);
}

// BF2.exe 0x436D60, ELF 0x84F6020
char* PbSvAddEvent(int type, int clientIndex, int length, char* data)
{
    return addPbEvent(&pbsv, type, clientIndex, length, data, 0);
}

// BF2.exe 0x436D90, ELF 0x84F6060: "<first> <second>" as event 0xF; dropped above 0x800 characters.
// Unreferenced in both images.
void PbSvAddStatEvent(int clientIndex, char* first, char* second)
{
    if (std::strlen(first) + std::strlen(second) > 0x800) return;
    char text[0x802];  // [CHOSEN] holds retail's maximum; its 0x800-byte buffer can be overrun by two
    std::strcpy(text, first);
    std::strcat(text, " ");
    std::strcat(text, second);
    addPbEvent(&pbsv, 0x0F, clientIndex, static_cast<int>(std::strlen(text)), text, 0);
}

// BF2.exe 0x436E70, ELF 0x84F6140: a PB_SV_ console command for pbsv.
void PbServerTrapConsole(char* command, int length) { addPbEvent(&pbsv, 0x0E, -1, length, command, 0); }

// BF2.exe 0x436E90 / 0x436EA0, ELF 0x84F61F0 / 0x84F6210: the per-frame pump, and the forced pass a
// pb_ console command triggers.
void PbServerProcessEvents() { processPbEvents(&pbsv, 0); }
void PbServerForceProcess() { processPbEvents(&pbsv, -1); }

// BF2.exe 0x436EB0, ELF 0x84F6230. Unreferenced in both images.
void PbServerCompleteCommand(char* text, int length) { addPbEvent(&pbsv, 0x33, -1, length, text, 0); }

// BF2.exe 0x436A70, ELF 0x84F6270: pbsv's own hook, when it set one. Unreferenced in both images.
void PbPassConnectString(char* first, char* second)
{
    if (pbsv.m_connectString) pbsv.m_connectString(&pbsv, first, second);
}

// BF2.exe 0x436A90, ELF 0x84F62A0: pbsv's admission check for a CD-key-authorized client; 0 admits,
// a text is the kick reason. 0 when pbsv set no hook.
char* PbAuthClient(char* address, int flag, char* keyHash)
{
    if (!pbsv.m_authClient) return 0;
    return pbsv.m_authClient(&pbsv, address, flag, keyHash);
}

// BF2.exe 0x436AC0, ELF 0x84F63B0: append a console line to pbsv's capture buffer while it fits.
// Unreferenced in both images.
void PbCaptureConsoleOutput(char* text)
{
    char* buffer = captureBuffer();
    if (!buffer) return;
    const std::size_t used = std::strlen(buffer);
    if (used + std::strlen(text) + 1 >= static_cast<std::size_t>(captureCapacity())) return;
    std::strcpy(buffer + used, text);
    std::strcat(buffer, "\n");
}

// BF2.exe 0x436ED0, ELF 0x84F62E0: pbsv's status query 0x71; false when it is not loaded.
extern "C" bool isPbSvEnabled() { return addPbEvent(&pbsv, 0x71, -1, 0, 0, 1) != 0; }

// BF2.exe 0x436F40 / 0x436FC0, ELF 0x84F6330 / 0x84F6370: events 0x75 / 0x76. Unreferenced in both
// images.
void EnablePbSv() { addPbEvent(&pbsv, 0x75, -1, 0, 0, 0); }
void DisablePbSv() { addPbEvent(&pbsv, 0x76, -1, 0, 0, 0); }

void* PbServerGlueObject() { return &pbsv; }
