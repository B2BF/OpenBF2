// [CHOSEN, not recovered] Frontier logging avoids allocating engine memory and flushes
// every write so the diagnostic survives an immediate abort.

#include "Frontier.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// shared ABI KIT - three per-image constants, parameterized so a second plugin image (SwiffPlayer)
// can reuse this file verbatim with its own log file, env var and banner. The defaults are the
// original RendDX9 literals, so renddx9.cmd - which passes none of these - is byte-for-byte
// unchanged. swiffplayer.cmd passes all three. [RETIRE-WHEN: RW-49] applies to the file as a whole.
#ifndef RD9ABI_LOG_FILENAME
#define RD9ABI_LOG_FILENAME "RendDX9Rebuilt.log"
#endif
#ifndef RD9ABI_FRONTIER_ENV
#define RD9ABI_FRONTIER_ENV "BF2_RENDDX9_FRONTIER"
#endif
#ifndef RD9ABI_LOG_BANNER
#define RD9ABI_LOG_BANNER "RendDX9 replacement"
#endif

namespace renddx9abi {

namespace {

FILE*           s_log       = 0;
CRITICAL_SECTION s_lock;
bool            s_lockReady = false;
int             s_trap      = -1;   // -1 unknown, 1 abort, 0 continue

// Per-name call counts. A fixed table, not a map: this runs on the path where the allocator itself
// is under test, and a 128-entry linear scan costs nothing at the rate these are hit.
struct CallCount { const char* name; unsigned int n; };
CallCount    s_counts[128];
unsigned int s_countUsed = 0;

unsigned int bump(const char* name)
{
    for (unsigned int i = 0; i < s_countUsed; ++i)
    {
        // Pointer compare first: every caller passes a string literal, so the common case never
        // reaches strcmp.
        if (s_counts[i].name == name || ::strcmp(s_counts[i].name, name) == 0)
            return ++s_counts[i].n;
    }
    if (s_countUsed < 128)
    {
        s_counts[s_countUsed].name = name;
        s_counts[s_countUsed].n    = 1;
        ++s_countUsed;
    }
    return 1;
}

void ensureLock()
{
    if (!s_lockReady)
    {
        ::InitializeCriticalSection(&s_lock);
        s_lockReady = true;
    }
}

void writeLine(const char* text)
{
    // Always to the debugger - that channel works even when the file could not be opened, which is
    // the case a frontier hit during DLL_PROCESS_ATTACH would otherwise vanish into.
    ::OutputDebugStringA(text);
    ::OutputDebugStringA("\n");

    if (s_log)
    {
        ::fputs(text, s_log);
        ::fputc('\n', s_log);
        ::fflush(s_log);
    }
}

void formatPrefix(char* out, size_t cap)
{
    SYSTEMTIME t;
    ::GetLocalTime(&t);
    ::_snprintf_s(out, cap, _TRUNCATE, "%02u:%02u:%02u.%03u [%5lu] ",
                  t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                  ::GetCurrentThreadId());
}

} // anonymous namespace

bool frontierTrapEnabled()
{
    if (s_trap < 0)
    {
        char  value[32];
        DWORD n = ::GetEnvironmentVariableA(RD9ABI_FRONTIER_ENV, value, sizeof(value));
        // Anything other than the exact word "continue" leaves the trap armed. An unset variable,
        // a typo, and a truncated value all mean abort - which is the safe direction.
        s_trap = (n > 0 && n < sizeof(value) && ::_stricmp(value, "continue") == 0) ? 0 : 1;
    }
    return s_trap != 0;
}

void logOpen()
{
    ensureLock();
    ::EnterCriticalSection(&s_lock);
    if (!s_log)
    {
        // The game runs with the game directory as its working directory, so a relative name lands
        // beside BF2.exe where a tester will look for it.
        if (::fopen_s(&s_log, RD9ABI_LOG_FILENAME, "w") != 0)
            s_log = 0;

        char prefix[64];
        formatPrefix(prefix, sizeof(prefix));

        char banner[256];
        ::_snprintf_s(banner, sizeof(banner), _TRUNCATE,
                      "%s%s - log opened. Frontier trap: %s.",
                      prefix, RD9ABI_LOG_BANNER, frontierTrapEnabled() ? "ARMED (abort)" : "DISARMED (continue)");
        writeLine(banner);
    }
    ::LeaveCriticalSection(&s_lock);
}

void logClose()
{
    if (!s_lockReady)
        return;
    ::EnterCriticalSection(&s_lock);
    if (s_log)
    {
        ::fclose(s_log);
        s_log = 0;
    }
    ::LeaveCriticalSection(&s_lock);
}

void logf(const char* fmt, ...)
{
    ensureLock();
    ::EnterCriticalSection(&s_lock);

    char prefix[64];
    formatPrefix(prefix, sizeof(prefix));

    char body[1024];
    va_list args;
    va_start(args, fmt);
    ::_vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, args);
    va_end(args);

    char line[1152];
    ::_snprintf_s(line, sizeof(line), _TRUNCATE, "%s%s", prefix, body);
    writeLine(line);

    ::LeaveCriticalSection(&s_lock);
}

unsigned int addr(const void* p)
{
    return static_cast<unsigned int>(reinterpret_cast<UINT_PTR>(p));
}

void trace(const char* qualifiedName, const void* self, unsigned int slot)
{
    const unsigned int n = bump(qualifiedName);
    logf("[trace]    %s  this=0x%p slot=+0x%02X call#%u", qualifiedName, self, slot, n);
}

void frontier(const char* qualifiedName, const void* self, unsigned int slot)
{
    const unsigned int n = bump(qualifiedName);
    logf("[FRONTIER] %s  this=0x%p slot=+0x%02X call#%u", qualifiedName, self, slot, n);

    if (!frontierTrapEnabled())
        return;

    logf("[FRONTIER] trap armed - aborting. Set %s=continue to walk past this "
         "and keep looking for the next divergence.", RD9ABI_FRONTIER_ENV);
    logClose();

    // A debugger gets the break; a bare run gets the abort. Both are unmistakable, which is the
    // whole requirement - what must not happen is execution continuing with a made-up value.
    if (::IsDebuggerPresent())
        ::DebugBreak();

    ::abort();
}

} // namespace renddx9abi
