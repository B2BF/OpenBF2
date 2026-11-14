// [CHOSEN, not recovered] Non-fatal fallback log for the native D3D9/D3DX layer.
// A D3D9/D3DX input that retail BF2 1.50 cannot select
// takes a documented fallback instead of aborting: the call answers the error,
// default or no-op its comment names, and the first occurrence of each distinct message is logged
// as "[UNSUPPORTED] <what>" so a run that reaches one is still visible in client.log.
#ifndef BF2_PLATFORM_GFX_UNSUPPORTED_H
#define BF2_PLATFORM_GFX_UNSUPPORTED_H

#include <cstdio>
#include <mutex>
#include <set>
#include <string>

namespace bf2gfx {

inline void unsupported(const std::string& what)
{
    static std::mutex lock;
    static std::set<std::string> seen;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (!seen.insert(what).second)
            return;
    }
    std::fprintf(stderr, "[UNSUPPORTED] %s (fallback; logged once)\n", what.c_str());
    std::fflush(stderr);
}

} // namespace bf2gfx

#endif // BF2_PLATFORM_GFX_UNSUPPORTED_H
