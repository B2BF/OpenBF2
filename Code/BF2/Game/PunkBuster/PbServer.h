// The PunkBuster server glue object: BF2.exe ds:0x96E758, ELF ds:0x8AC82A0 (the ELF names the type
// stPbSv). pbsv receives a pointer to it and reads the callbacks at their x86 offsets, so the x86 layout
// is the ABI. Both images use the same offsets.
#ifndef GAME_PUNKBUSTER_PBSERVER_H
#define GAME_PUNKBUSTER_PBSERVER_H
#include <cstddef>

struct stPbSv;
// The vendor library's two entry points and the hooks it writes back (all cdecl).
typedef int (*PbSvEventFn)(stPbSv*, int type, int clientIndex, int length, char* data, int flag);  // "sb"
typedef int (*PbSvProcessFn)(stPbSv*, int);                                                       // "sa"
typedef void (*PbSvConnectStringFn)(stPbSv*, char*, char*);
typedef char* (*PbSvAuthClientFn)(stPbSv*, char* address, int, char* keyHash);

struct stPbSv {
    unsigned int m_magic;                          // +0x00 0x357AFE20 in the static image
    int m_unresolved04;                            // +0x04 [UNRESOLVED]
    void* m_library;                               // +0x08 pbsv.dll / pbsv.so
    void* m_library2;                              // +0x0C released only at exit [UNRESOLVED]
    void* m_library3;                              // +0x10 released only at exit [UNRESOLVED]
    char m_gameName[0x20];                         // +0x14 "PunkBuster Server", the message prefix
    char m_path[0x104];                            // +0x34 "<home>/pb/" (makefn's base)
    int m_reload;                                  // +0x138 1 in the static image: the next event (re)loads
    int (*m_gameCommand)(char* command, char* args);                  // +0x13C
    char* (*m_gameQuery)(int type, char* buffer);                     // +0x140 (an error text or 0)
    int (*m_gameMessage)(char* text, int flag);                       // +0x144
    int (*m_sendToClient)(int length, char* data, int clientIndex);   // +0x148
    PbSvEventFn m_dllEvent;                                           // +0x14C symbol "sb"
    PbSvProcessFn m_dllProcess;                                       // +0x150 symbol "sa"
    int (*m_sendToAddrPort)(char* address, unsigned short port, int length, char* data);  // +0x154
    PbSvConnectStringFn m_connectString;           // +0x158 written by pbsv
    PbSvAuthClientFn m_authClient;                 // +0x15C written by pbsv
    void* m_dllHook160;                            // +0x160 [UNRESOLVED]
    void* m_dllHook164;                            // +0x164 cleared at exit [UNRESOLVED]
    int m_state;                                   // +0x168 999 until the not-installed notice
    // ELF only: the console capture buffer pbsv hands over by "ConCapBuf" / "ConCapBufLen". BF2.exe keeps
    // the pair in globals of its own (ds:0x99EF78 / ds:0x99EF7C, PbServer.cpp).
    char* m_captureBuffer;                         // +0x16C
    int m_captureCapacity;                         // +0x170
};
#if defined(_M_IX86) || defined(__i386__)
static_assert(offsetof(stPbSv, m_gameName) == 0x14 && offsetof(stPbSv, m_path) == 0x34, "pbsv ABI");
static_assert(offsetof(stPbSv, m_reload) == 0x138 && offsetof(stPbSv, m_gameCommand) == 0x13C, "pbsv ABI");
static_assert(offsetof(stPbSv, m_dllEvent) == 0x14C && offsetof(stPbSv, m_authClient) == 0x15C, "pbsv ABI");
static_assert(offsetof(stPbSv, m_state) == 0x168 && offsetof(stPbSv, m_captureCapacity) == 0x170, "pbsv ABI");
#endif

extern stPbSv pbsv;  // BF2.exe ds:0x96E758, ELF ds:0x8AC82A0

// [CHOSEN, not recovered] Library seam, the same as the client glue's. Windows: LoadLibraryA /
// GetProcAddress / FreeLibrary, as BF2.exe does. Elsewhere load is null: no pbsv exists for these
// builds (the shipped pbsv.so is 32-bit x86), so the glue reports the load failure before it touches
// the pb folder. The fake-pbsv test installs its own functions.
struct PbServerLibraryApi {
    void* (*load)(const char* path);
    void* (*symbol)(void* library, const char* name);
    void (*release)(void* library);
};
extern PbServerLibraryApi g_pbServerLibrary;

#endif
