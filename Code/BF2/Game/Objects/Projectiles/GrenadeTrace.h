// [TEMP] [CHOSEN, not recovered] Opt-in grenade trajectory diagnostics for issue #20.
// BF2_TRACE_GRENADES=1 records up to 16 M67 launches and 8192 lines per process.
// [RETIRE-WHEN: issue20-grenade-drift-diagnosed] Remove the helper and its call sites
// after a joined-client trace identifies the cause and the resulting fix is validated.
#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

#include "../IObject.h"
#include "../IObjectTemplate.h"
#include "../../../Core/System.h"
#include "../../../IO/Network/INetworkable.h"

namespace dice {
namespace hfe {
namespace world {
namespace grenade_trace {

namespace detail {

struct Entry
{
    const IObject* object;
    unsigned int objectId;
    unsigned int serial;
    bool active;
};

struct State
{
    Entry entries[16];
    unsigned int launches;
    unsigned int lines;
    std::FILE* file;
    bool triedOpen;
};

// External-linkage inline storage is shared by every instrumented TU in this image.
inline State& state()
{
    static State value = {};
    return value;
}

inline bool enabledByEnvironment()
{
    const char* value = std::getenv("BF2_TRACE_GRENADES");
    return value && value[0] == '1' && value[1] == '\0';
}

inline bool isM67(const char* name)
{
    const char* expected = "ushgr_m67_projectile";
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

inline Entry* find(const IObject* object)
{
    State& s = state();
    const unsigned int objectId = object->getObjectId();
    for (unsigned int i = 0; i < s.launches; ++i)
    {
        Entry& entry = s.entries[i];
        if (entry.active && entry.object == object && entry.objectId == objectId)
            return &entry;
    }
    return 0;
}

} // namespace detail

inline bool armed()
{
    static const bool enabled = detail::enabledByEnvironment();
    return enabled;
}

inline bool selected(const IObject* object)
{
    return armed() && object && detail::find(object) != 0;
}

// Selection never owns an object. Object IDs distinguish reused addresses; retire ends a launch.
inline bool begin(const IObject* object)
{
    if (!armed() || !object)
        return false;
    if (detail::find(object))
        return true;
    detail::State& s = detail::state();
    if (s.launches == 16 || s.lines == 8192)
        return false;
    const IObjectTemplate* objectTemplate = object->getTemplate();
    if (!objectTemplate || !detail::isM67(objectTemplate->getName().c_str()))
        return false;
    detail::Entry& entry = s.entries[s.launches];
    entry.object = object;
    entry.objectId = object->getObjectId();
    entry.serial = ++s.launches;
    entry.active = true;
    return true;
}

// Callers supply trusted format strings and existing locals, never extra transform queries.
inline void log(const IObject* object, const char* event, const char* format, ...)
{
    if (!armed() || !object)
        return;
    detail::Entry* entry = detail::find(object);
    detail::State& s = detail::state();
    if (!entry || s.lines == 8192)
        return;
    if (!s.triedOpen)
    {
        s.triedOpen = true;
        const char* path = std::getenv("BF2_GRENADE_TRACE_FILE");
        if (!path || !*path)
        {
#if DEDICATED
            path = "bf2-grenades-server.log";
#else
            path = "bf2-grenades-client.log";
#endif
        }
        s.file = std::fopen(path, "a");
        if (!s.file)
        {
            std::fprintf(stderr, "[GRENADETRACE] Cannot open trace file: %s\n", path);
            std::fflush(stderr);
        }
    }
    if (!s.file)
        return;

    char payload[1024];
    va_list args;
    va_start(args, format);
    std::vsnprintf(payload, sizeof(payload), format, args);
    va_end(args);
    // The qualified base getter only reads the cached pointer, without virtual side effects.
    const io::INetworkable* net = object->IObject::getNetworkable();
    ++s.lines;
    std::fprintf(s.file,
        "[GRENADETRACE] tick=%u object=%u net=%u serial=%u event=%s %s%s\n",
        WorldPref::mTickCount, entry->objectId,
        net ? static_cast<unsigned int>(net->m_networkableId) : 0u,
        entry->serial, event, payload,
        s.lines == 8192 ? " trace_line_limit_reached=1" : "");
    std::fflush(s.file);
}

inline void retire(const IObject* object)
{
    if (!armed() || !object)
        return;
    if (detail::Entry* entry = detail::find(object))
        entry->active = false;
}

} // namespace grenade_trace
} // namespace world
} // namespace hfe
} // namespace dice
