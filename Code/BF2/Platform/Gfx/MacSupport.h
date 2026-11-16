// [CHOSEN, not recovered] Renderer-local host/CRT spellings for the macOS port.
#ifndef BF2_GFX_MACSUPPORT_H
#define BF2_GFX_MACSUPPORT_H
#if (defined(__APPLE__) || defined(BF2_LINUX_CLIENT)) && defined(BF2_IMAGE_RENDDX9)
#include "D3D9Subset.h"
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <sys/stat.h>
#include <cstring>
using LONGLONG = std::int64_t;
extern "C" int __cdecl _purecall(void);
constexpr std::size_t _TRUNCATE = static_cast<std::size_t>(-1);

// The renderer uses only bounded output and _TRUNCATE. Keep the CRT's -1 on truncation.
inline int _snprintf_s(char* out, std::size_t size, std::size_t count, const char* format, ...)
{
    if (!out || !size) return -1;
    const std::size_t capacity = count == _TRUNCATE ? size : std::min(size, count + 1);
    va_list args; va_start(args, format);
    const int n = std::vsnprintf(out, capacity, format, args);
    va_end(args);
    return n < 0 || static_cast<std::size_t>(n) >= capacity ? -1 : n;
}
template<std::size_t N, class... Args>
inline int sprintf_s(char (&out)[N], const char* format, Args... args)
{
    const int n = std::snprintf(out, N, format, args...);
    if (n < 0 || static_cast<std::size_t>(n) >= N) { out[0] = '\0'; return -1; }
    return n;
}
template<class... Args>
inline int sprintf_s(char* out, std::size_t size, const char* format, Args... args)
{
    if (!out || !size) return -1;
    const int n = std::snprintf(out, size, format, args...);
    if (n < 0 || static_cast<std::size_t>(n) >= size) { out[0] = '\0'; return -1; }
    return n;
}
inline BOOL QueryPerformanceFrequency(LARGE_INTEGER* value)
{ value->QuadPart = 1000000000; return TRUE; }
inline BOOL QueryPerformanceCounter(LARGE_INTEGER* value)
{
    value->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return TRUE;
}
inline BOOL InflateRect(RECT* rect, int dx, int dy)
{ rect->left-=dx; rect->right+=dx; rect->top-=dy; rect->bottom+=dy; return TRUE; }
inline BOOL CreateDirectoryA(const char* path, void*)
{ return ::mkdir(path, 0777) == 0; }
inline DWORD GetEnvironmentVariableA(const char* name, char* out, DWORD capacity)
{
    const char* value = std::getenv(name);
    if (!value) return 0;
    const std::size_t n = std::strlen(value);
    if (capacity <= n) return static_cast<DWORD>(n + 1);
    std::memcpy(out, value, n + 1);
    return static_cast<DWORD>(n);
}
inline int strncpy_s(char* out, std::size_t size, const char* in, std::size_t count)
{
    if (!out || !size || !in) return 22;
    const std::size_t n = std::strlen(in);
    const std::size_t requested = count == _TRUNCATE ? n : std::min(n, count);
    if (requested >= size && count != _TRUNCATE) { out[0]='\0'; return 34; }
    const std::size_t copied = std::min(requested, size-1);
    std::memcpy(out, in, copied); out[copied]='\0';
    return copied < requested ? 80 : 0;
}

// [TEMP] [RETIRE-WHEN: macos-renderer-host] Replace a host boundary only after
// its native implementation and caller-visible behavior have been validated.
[[noreturn]] inline void bf2MacRendererStop(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] %s reached: native renderer host operation is unimplemented\n", name);
    std::fflush(stderr);
    std::abort();
}
#endif
#endif
