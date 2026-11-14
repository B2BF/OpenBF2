// [CHOSEN, not recovered] Frontier logging avoids allocating engine memory and flushes
// every write so the diagnostic survives an immediate abort.

#include "Frontier.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
// [CHOSEN, not recovered] Native POSIX arm (macOS port; docs/macos-port.md, S2.5/S2.8): a pthread
// mutex for the critical section, stderr for the debugger channel, gettimeofday/localtime_r for
// the prefix clock and the Mach thread id for GetCurrentThreadId. Output text is unchanged.
#include <pthread.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <stdint.h>
#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <unistd.h>
#endif
#endif

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

// [CHOSEN, not recovered] The trap's default when the env var is unset (owner decision 2026-11-14).
// 0 (dev builds, every gate): armed, a frontier logs and aborts. 1 (release packages built by CI):
// disarmed, a frontier logs and returns. CMake option BF2_FRONTIER_DEFAULT_CONTINUE sets it; the
// .cmd image builds (renddx9/swiffplayer/bf2audio) see it through the same-named env var, which
// the CMake plugin targets always pass explicitly. The [FRONTIER] lines are logged either way.
#ifndef BF2_FRONTIER_DEFAULT_CONTINUE
#define BF2_FRONTIER_DEFAULT_CONTINUE 0
#endif

namespace renddx9abi {

namespace {

FILE*           s_log       = 0;
#if defined(_WIN32)
CRITICAL_SECTION s_lock;
#else
pthread_mutex_t s_lock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER;  // CRITICAL_SECTION is recursive too
#endif
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

#if defined(_WIN32)
void lockEnter() { ::EnterCriticalSection(&s_lock); }
void lockLeave() { ::LeaveCriticalSection(&s_lock); }
#else
void lockEnter() { ::pthread_mutex_lock(&s_lock); }
void lockLeave() { ::pthread_mutex_unlock(&s_lock); }
#endif

void ensureLock()
{
    if (!s_lockReady)
    {
#if defined(_WIN32)
        ::InitializeCriticalSection(&s_lock);
#endif
        s_lockReady = true;
    }
}

void writeLine(const char* text)
{
    // Always to the debugger - that channel works even when the file could not be opened, which is
    // the case a frontier hit during DLL_PROCESS_ATTACH would otherwise vanish into.
#if defined(_WIN32)
    ::OutputDebugStringA(text);
    ::OutputDebugStringA("\n");
#else
    ::fputs(text, stderr);
    ::fputc('\n', stderr);
    ::fflush(stderr);
#endif

    if (s_log)
    {
        ::fputs(text, s_log);
        ::fputc('\n', s_log);
        ::fflush(s_log);
    }
}

#if defined(_WIN32)
void formatPrefix(char* out, size_t cap)
{
    SYSTEMTIME t;
    ::GetLocalTime(&t);
    ::_snprintf_s(out, cap, _TRUNCATE, "%02u:%02u:%02u.%03u [%5lu] ",
                  t.wHour, t.wMinute, t.wSecond, t.wMilliseconds,
                  ::GetCurrentThreadId());
}
#else
void formatPrefix(char* out, size_t cap)
{
    timeval now;
    ::gettimeofday(&now, 0);
    const time_t seconds = now.tv_sec;
    tm local;
    ::localtime_r(&seconds, &local);
    unsigned long thread = 0;
#if defined(__APPLE__)
    uint64_t id = 0;
    ::pthread_threadid_np(0, &id);
    thread = static_cast<unsigned long>(id);
#endif
    // snprintf truncates and terminates, as _snprintf_s with _TRUNCATE does.
    ::snprintf(out, cap, "%02u:%02u:%02u.%03u [%5lu] ",
               static_cast<unsigned>(local.tm_hour), static_cast<unsigned>(local.tm_min),
               static_cast<unsigned>(local.tm_sec), static_cast<unsigned>(now.tv_usec / 1000),
               thread);
}
#endif

} // anonymous namespace

bool frontierTrapEnabled()
{
    if (s_trap < 0)
    {
        char  value[32];
#if defined(_WIN32)
        DWORD n = ::GetEnvironmentVariableA(RD9ABI_FRONTIER_ENV, value, sizeof(value));
#else
        // GetEnvironmentVariableA's answer: the length on success, the required size (> buffer)
        // when the value does not fit, 0 when unset.
        size_t n = 0;
        if (const char* env = ::getenv(RD9ABI_FRONTIER_ENV))
        {
            const size_t length = ::strlen(env);
            n = length < sizeof(value) ? length : length + 1;
            if (length < sizeof(value))
                ::memcpy(value, env, length + 1);
        }
#endif
        // An explicit value wins in every build: "continue" disarms, "abort" arms (case-insensitive).
        // Anything unexpected - an unset variable, a typo, a truncated value - is the build's
        // default (BF2_FRONTIER_DEFAULT_CONTINUE above): abort in dev builds, continue in release
        // packages.
#if defined(_WIN32)
#define RD9ABI_STRICMP ::_stricmp
#else
#define RD9ABI_STRICMP ::strcasecmp
#endif
        const bool readable = n > 0 && n < sizeof(value);
        if (readable && RD9ABI_STRICMP(value, "continue") == 0)
            s_trap = 0;
        else if (readable && RD9ABI_STRICMP(value, "abort") == 0)
            s_trap = 1;
        else
            s_trap = BF2_FRONTIER_DEFAULT_CONTINUE ? 0 : 1;
#undef RD9ABI_STRICMP
    }
    return s_trap != 0;
}

void logOpen()
{
    ensureLock();
    lockEnter();
    if (!s_log)
    {
        // The game runs with the game directory as its working directory, so a relative name lands
        // beside BF2.exe where a tester will look for it. The macOS client is started in the
        // installed game folder and writes the log there too (docs/macos-write-paths.md).
#if defined(_WIN32)
        if (::fopen_s(&s_log, RD9ABI_LOG_FILENAME, "w") != 0)
            s_log = 0;
#else
        s_log = ::fopen(RD9ABI_LOG_FILENAME, "w");
#endif

        char prefix[64];
        formatPrefix(prefix, sizeof(prefix));

        char banner[256];
#if defined(_WIN32)
        ::_snprintf_s(banner, sizeof(banner), _TRUNCATE,
#else
        ::snprintf(banner, sizeof(banner),
#endif
                      "%s%s - log opened. Frontier trap: %s.",
                      prefix, RD9ABI_LOG_BANNER, frontierTrapEnabled() ? "ARMED (abort)" : "DISARMED (continue)");
        writeLine(banner);
    }
    lockLeave();
}

void logClose()
{
    if (!s_lockReady)
        return;
    lockEnter();
    if (s_log)
    {
        ::fclose(s_log);
        s_log = 0;
    }
    lockLeave();
}

void logf(const char* fmt, ...)
{
    ensureLock();
    lockEnter();

    char prefix[64];
    formatPrefix(prefix, sizeof(prefix));

    char body[1024];
    va_list args;
    va_start(args, fmt);
#if defined(_WIN32)
    ::_vsnprintf_s(body, sizeof(body), _TRUNCATE, fmt, args);
#else
    ::vsnprintf(body, sizeof(body), fmt, args);
#endif
    va_end(args);

    char line[1152];
#if defined(_WIN32)
    ::_snprintf_s(line, sizeof(line), _TRUNCATE, "%s%s", prefix, body);
#else
    ::snprintf(line, sizeof(line), "%s%s", prefix, body);
#endif
    writeLine(line);

    lockLeave();
}

unsigned int addr(const void* p)
{
#if defined(_WIN32)
    return static_cast<unsigned int>(reinterpret_cast<UINT_PTR>(p));
#else
    return static_cast<unsigned int>(reinterpret_cast<uintptr_t>(p));
#endif
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
#if defined(_WIN32)
    if (::IsDebuggerPresent())
        ::DebugBreak();
#elif defined(__APPLE__)
    {
        // IsDebuggerPresent: the P_TRACED flag of this process.
        int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(::getpid()) };
        struct kinfo_proc info;
        size_t size = sizeof(info);
        ::memset(&info, 0, sizeof(info));
        if (::sysctl(mib, 4, &info, &size, 0, 0) == 0 && (info.kp_proc.p_flag & P_TRACED))
            __builtin_debugtrap();
    }
#endif

    ::abort();
}

} // namespace renddx9abi
