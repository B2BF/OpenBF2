// [TEMP] [CHOSEN, not recovered] Opt-in diagnostics for TV missile stutter.
// [RETIRE-WHEN: tv-missile-stutter-diagnosed] Remove this helper and its call sites
// after a joined-client trace identifies the cause and the resulting fix is validated.
#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "../IObject.h"
#include "../IObjectTemplate.h"
#include "../../../Core/System.h"
#include "../../../IO/Network/INetworkable.h"

namespace dice { namespace hfe { namespace world { namespace tv_missile_trace {

inline bool armed()
{
    static const bool enabled = []() {
        const char* value = std::getenv("BF2_TRACE_TV_MISSILES");
        return value && value[0] == '1' && value[1] == '\0';
    }();
    return enabled;
}

namespace detail {

inline bool selected(const IObject* object)
{
    if (!object)
        return false;
    const IObjectTemplate* objectTemplate = object->getTemplate();
    if (!objectTemplate)
        return false;
    const char* name = objectTemplate->getName().c_str();
    const char* expected = "agm114_hellfire_tv";
    while (*name && *expected)
    {
        const char c = *name >= 'A' && *name <= 'Z'
            ? static_cast<char>(*name + ('a' - 'A')) : *name;
        if (c != *expected)
            return false;
        ++name;
        ++expected;
    }
    return *name == '\0' && *expected == '\0';
}

struct State
{
    std::FILE* file = 0;
    bool triedOpen = false;
    unsigned int lines = 0;
    char buffer[65536];
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    ~State() { if (file) std::fclose(file); }
};

// These hooks run on the simulation thread. Inline storage is shared across its TUs.
inline State& state()
{
    static State value;
    return value;
}

} // namespace detail

// Existing locals only: callers must guard extra diagnostic work with armed().
// Buffered output is flushed every 64 rows; the final partial batch closes at normal exit.
inline void log(const IObject* object, const char* event, const char* format, ...)
{
    if (!armed() || !detail::selected(object))
        return;
    detail::State& state = detail::state();
    if (state.lines == 8192)
        return;
    if (!state.triedOpen)
    {
        state.triedOpen = true;
        const char* path = std::getenv("BF2_TV_MISSILE_TRACE_FILE");
        if (!path || !*path)
            path = "bf2-tv-missiles.log";
        state.file = std::fopen(path, "a");
        if (!state.file)
        {
            std::fprintf(stderr, "[TVTRACE] Cannot open trace file: %s\n", path);
            std::fflush(stderr);
            return;
        }
        std::setvbuf(state.file, state.buffer, _IOFBF, sizeof(state.buffer));
        std::fprintf(state.file, "[TVTRACE] begin version=1 dedicated=%d limit=8192\n", DEDICATED);
    }
    if (!state.file)
        return;

    char payload[768];
    va_list args;
    va_start(args, format);
    std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    const long long micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - state.start).count();
    const io::INetworkable* net = object->IObject::getNetworkable();
    ++state.lines;
    std::fprintf(state.file,
        "[TVTRACE] us=%lld tick=%u object=%u net=%u event=%s %s%s\n",
        micros, WorldPref::mTickCount, object->getObjectId(),
        net ? static_cast<unsigned int>(net->m_networkableId) : 0u,
        event, payload, state.lines == 8192 ? " trace_line_limit_reached=1" : "");
    if ((state.lines & 63u) == 0)
        std::fflush(state.file);
}

} } } } // namespace dice::hfe::world::tv_missile_trace
