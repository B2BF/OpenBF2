// [TEMP] [CHOSEN, not recovered] Optional first-fault logger, enabled by BF2_SWIFFPLAYER_CRASHLOG.
// Records registers and stack addresses, then continues exception search without swallowing it.
// Frame-chain tracing requires frame pointers; stack scans can include stale return addresses.
// [RETIRE-WHEN: RW-89] Remove with the other SwiffPlayer tracers.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include "../../../../RendDX9Abi/Frontier.h"

namespace {

bool moduleOf(const void* addr, char* name, size_t nameCap, unsigned* rva)
{
    HMODULE m = 0;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)addr, &m) || !m)
        return false;
    char path[MAX_PATH];
    path[0] = 0;
    GetModuleFileNameA(m, path, MAX_PATH);
    const char* base = strrchr(path, '\\');
    base = base ? base + 1 : path;
    strncpy(name, base, nameCap - 1);
    name[nameCap - 1] = 0;
    *rva = (unsigned)((const char*)addr - (const char*)m);
    return true;
}

bool readable(const void* p, size_t n)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(p, &mbi, sizeof mbi) != sizeof mbi) return false;
    if (mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    return ((const char*)p + n) <= ((const char*)mbi.BaseAddress + mbi.RegionSize);
}

volatile long g_reported = 0;

LONG WINAPI crashHandler(EXCEPTION_POINTERS* ep)
{
    const DWORD code = ep->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION
        && code != EXCEPTION_STACK_OVERFLOW && code != EXCEPTION_INT_DIVIDE_BY_ZERO
        && code != EXCEPTION_PRIV_INSTRUCTION && code != EXCEPTION_ARRAY_BOUNDS_EXCEEDED)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedExchange(&g_reported, 1) != 0) return EXCEPTION_CONTINUE_SEARCH;

    char mod[64];
    unsigned rva = 0;
    const void* at = ep->ExceptionRecord->ExceptionAddress;
    if (moduleOf(at, mod, sizeof mod, &rva))
        ::renddx9abi::logf("[crash] code=0x%08X at %s+0x%X (%p)", (unsigned)code, mod, rva, at);
    else
        ::renddx9abi::logf("[crash] code=0x%08X at %p (no module)", (unsigned)code, at);
    if (code == EXCEPTION_ACCESS_VIOLATION && ep->ExceptionRecord->NumberParameters >= 2)
        ::renddx9abi::logf("[crash]   %s address %p",
                           ep->ExceptionRecord->ExceptionInformation[0] ? "writing" : "reading",
                           (void*)ep->ExceptionRecord->ExceptionInformation[1]);

    const CONTEXT* c = ep->ContextRecord;
#if defined(_M_IX86)
    ::renddx9abi::logf("[crash]   eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X ebp=%08X esp=%08X",
                       (unsigned)c->Eax, (unsigned)c->Ebx, (unsigned)c->Ecx, (unsigned)c->Edx,
                       (unsigned)c->Esi, (unsigned)c->Edi, (unsigned)c->Ebp, (unsigned)c->Esp);
    const uintptr_t stackPointer = (uintptr_t)c->Esp;

    // EBP chain.
    const void* const* frame = (const void* const*)c->Ebp;
    for (int i = 0; i < 48 && frame; ++i)
    {
        if (!readable(frame, 8)) break;
        const void* ret = frame[1];
        if (moduleOf(ret, mod, sizeof mod, &rva))
            ::renddx9abi::logf("[crash]   frame %2d: %s+0x%X", i, mod, rva);
        else
            ::renddx9abi::logf("[crash]   frame %2d: %p", i, ret);
        const void* const* next = (const void* const*)frame[0];
        if (next <= frame) break;
        frame = next;
    }
#else
    // [CHOSEN, not recovered] x64: the 64-bit registers; x64 code keeps no frame-pointer chain, so
    // only the return-address scan below runs.
    ::renddx9abi::logf("[crash]   rax=%016llX rbx=%016llX rcx=%016llX rdx=%016llX rsi=%016llX rdi=%016llX",
                       (unsigned long long)c->Rax, (unsigned long long)c->Rbx, (unsigned long long)c->Rcx,
                       (unsigned long long)c->Rdx, (unsigned long long)c->Rsi, (unsigned long long)c->Rdi);
    ::renddx9abi::logf("[crash]   rbp=%016llX rsp=%016llX rip=%016llX",
                       (unsigned long long)c->Rbp, (unsigned long long)c->Rsp, (unsigned long long)c->Rip);
    const uintptr_t stackPointer = (uintptr_t)c->Rsp;
#endif

    // Return-address scan (frame-pointer independent; includes stale entries).
    const void* const* sp = (const void* const*)(stackPointer & ~(uintptr_t)(sizeof(void*) - 1));
    int hits = 0;
    for (int i = 0; i < 4096 && hits < 80; ++i, ++sp)
    {
        if (!readable(sp, sizeof(void*))) break;
        const void* v = *sp;
        if (!moduleOf(v, mod, sizeof mod, &rva)) continue;
        if (_strnicmp(mod, "SwiffPlayer", 11) != 0 && _strnicmp(mod, "RendDX9", 7) != 0
            && _strnicmp(mod, "BF2", 3) != 0)
            continue;
        ::renddx9abi::logf("[crash]   stack[%4d]: %s+0x%X", i, mod, rva);
        ++hits;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

} // namespace

// The handler must leave with the DLL. BF2 unloads SwiffPlayer.dll when the menu is deactivated
// (the join path: deactivateMenu -> deinitDll -> FreeLibrary) and reloads it for the next menu; a
// vectored handler left registered points into unloaded code, and the next exception anywhere in the
// process - the join path's own first stop, for instance - becomes a BEX crash at
// "SwiffPlayer.dll_unloaded" that hides the real one. atexit inside the DLL runs at
// its CRT teardown, i.e. on unload.
static void* s_crashHandlerHandle = 0;
static void swiffCrashLogUninstall()
{
    if (!s_crashHandlerHandle) return;
    RemoveVectoredExceptionHandler(s_crashHandlerHandle);
    s_crashHandlerHandle = 0;
    ::renddx9abi::logf("[crash] logger removed (DLL unload)");
}
void swiffCrashLogInstall()
{
    static bool installed = false;
    if (installed) return;
    const char* e = getenv("BF2_SWIFFPLAYER_CRASHLOG");
    if (!e || !*e || *e == '0') return;
    installed = true;
    s_crashHandlerHandle = AddVectoredExceptionHandler(1, &crashHandler);
    ::atexit(&swiffCrashLogUninstall);
    ::renddx9abi::logf("[crash] logger installed (BF2_SWIFFPLAYER_CRASHLOG)");
}
