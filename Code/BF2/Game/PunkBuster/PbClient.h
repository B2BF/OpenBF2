// The PunkBuster client glue object (BF2.exe ds:0x96E5A8). [INFERRED] type name after the ELF's server
// counterpart stPbSv. pbcl.dll receives a pointer to it and reads the callbacks at their x86 offsets, so
// the x86 layout is the ABI.
#ifndef GAME_PUNKBUSTER_PBCLIENT_H
#define GAME_PUNKBUSTER_PBCLIENT_H
#include <cstddef>

struct stPbCl;
// The vendor DLL's two entry points and the game callbacks it is handed (all cdecl).
typedef int (*PbClEventFn)(stPbCl*, int type, int a, char* b, int c);   // "cb"
typedef int (*PbClProcessFn)(stPbCl*, int);                             // "ca"

struct stPbCl {
    unsigned int m_magic;                          // +0x00 0x264B8BAB in the static image
    int m_unresolved04;                            // +0x04 [UNRESOLVED]
    void* m_library;                               // +0x08 pbcl.dll
    void* m_library2;                              // +0x0C freed only at exit (0x873463) [UNRESOLVED]
    int m_reload;                                  // +0x10 1 in the static image: the next event (re)loads
    char m_md5[0x21];                              // +0x14 [UNRESOLVED] use (an unreferenced md5 helper writes it)
    char m_gameName[0x20];                         // +0x35 "PunkBuster Client", the message prefix
    char m_path[0x123];                            // +0x55 "<home>\pb\" (makefn's base)
    int (*m_gameCommand)(char* command, char* args);           // +0x178 0x435C50
    int (*m_gameQuery)(int type, char* buffer);                // +0x17C 0x435CC0
    int (*m_gameMessage)(char* text, int flag);                // +0x180 0x435DD0
    int (*m_sendToServer)(int length, char* data);             // +0x184 0x435EA0
    PbClEventFn m_dllEvent;                                     // +0x188 GetProcAddress "cb"
    PbClProcessFn m_dllProcess;                                 // +0x18C GetProcAddress "ca"
    int (*m_sendUdp)(char* address, unsigned short port, int length, char* data);  // +0x190 0x435EB0
    int (*m_callback194)(char*);                                // +0x194 0x435DC0
    void* m_dllHook198;                                         // +0x198 written by the DLL [UNRESOLVED]
    void* m_dllHook19C;                                         // +0x19C written by the DLL [UNRESOLVED]
    void (*m_consoleHook)(stPbCl*, char* command, int length);  // +0x1A0 written by the DLL; console trap
    void* m_serverGlue;                                         // +0x1A4 &stPbSv (ds:0x96E758)
    int m_state;                                                // +0x1A8 999 until the not-installed notice
};
#if defined(_M_IX86)
static_assert(offsetof(stPbCl, m_gameName) == 0x35 && offsetof(stPbCl, m_path) == 0x55, "pbcl ABI");
static_assert(offsetof(stPbCl, m_gameCommand) == 0x178 && offsetof(stPbCl, m_dllEvent) == 0x188, "pbcl ABI");
static_assert(offsetof(stPbCl, m_consoleHook) == 0x1A0 && offsetof(stPbCl, m_state) == 0x1A8, "pbcl ABI");
#endif

extern stPbCl pbcl;  // ds:0x96E5A8

// [CHOSEN, not recovered] Library seam. Windows: LoadLibraryA / GetProcAddress / FreeLibrary, as
// retail. Elsewhere load is null: no vendor DLL exists, so the glue reports the load failure without
// touching the pb folder (the macOS client never writes into the game folder). The fake-DLL test
// installs its own functions.
struct PbClientLibraryApi {
    void* (*load)(const char* path);
    void* (*symbol)(void* library, const char* name);
    void (*release)(void* library);
};
extern PbClientLibraryApi g_pbClientLibrary;

#endif
