// Named allocation counters: trace identifiers (scoped with a per-process trace stack) and resource
// identifiers, with a text report.
//
// In the shipped configuration only the resource counters are reachable (resourceTraceAllocate and
// resourceTraceFree); the trace exports are stubs and no export returns a report. Placement
// [INFERRED].

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <utility>

#include "MemoryInternal.h"

#if defined(_WIN32)
#  define bf2MemoryStricmp _stricmp
#else
#  include <strings.h>
// [CHOSEN, not recovered] POSIX spelling of the CRT's case-insensitive compare.
#  define bf2MemoryStricmp strcasecmp
#endif

namespace dice {
namespace hfe {
namespace memory {

TraceIdentifier* g_traceIdentifiers[kMaxTraceIdentifiers];
int g_traceIdentifierCount;
int g_visualFrame;
int g_physicsFrame;
TraceStackEntry g_traceStack[kMaxTraceDepth + 1];
int g_traceStackTop = -1;

namespace {

const char* s_unnamed = "nameNOTset";

char s_report[0x100000];
TraceIdentifier* s_resourceIdentifiers[kMaxTraceIdentifiers];
int s_resourceIdentifierCount;
char s_numberBuffer[0x400];
char* s_numberCursor;

struct SortedTrace
{
    unsigned int bytes;
    int index;
};

// Records `size` against `entry`: counts, bytes, block range and peaks.
inline void recordAllocation(TraceIdentifier* entry, int size)
{
    ++entry->numAllocs;
    entry->bytes += size;
    if (size >= 0) {
        if (size < entry->minBlock || entry->peakBytes == 0)
            entry->minBlock = size;
        if (size > entry->maxBlock)
            entry->maxBlock = size;
    }
    if (entry->numAllocs > entry->peakNumAllocs)
        entry->peakNumAllocs = entry->numAllocs;
    if (entry->bytes > entry->peakBytes)
        entry->peakBytes = entry->bytes;
}

} // namespace

// PE 0x100029E0. Formats `value` with '.' between groups of three digits into a shared buffer and
// returns it. Strings of one report line follow each other in the buffer; `restart` begins again
// at its start.
const char* formatNumber(int value, bool restart)
{
    char* out = (!restart && s_numberCursor) ? s_numberCursor : s_numberBuffer;
    char* const start = out;
    bool negative = false;
    if (value < 0) {
        *out++ = '-';
        negative = true;
        value = static_cast<int>(0u - static_cast<unsigned int>(value));
    }
    int digits = 0;
    do {
        if (digits == 3) {
            digits = 0;
            *out++ = '.';
        }
        *out++ = static_cast<char>('0' + value % 10);
        ++digits;
        value /= 10;
    } while (value != 0);

    char* low = negative ? start + 1 : start;
    char* high = out - 1;
    while (low < high) {
        const char c = *high;
        *high = *low;
        *low = c;
        ++low;
        --high;
    }
    *out++ = 0;
    s_numberCursor = out;
    return start;
}

// PE 0x10002A80.
TraceStackEntry::TraceStackEntry()
    : id(-1)
{
}

// PE 0x10002A90.
TraceIdentifier::TraceIdentifier()
{
    std::memset(this, 0, sizeof(*this));
    std::strcpy(name, s_unnamed);
}

// PE 0x10002AC0. Returns the slot of `name` (case-insensitive), adding it when new. A full table
// renames slot 0 to an error marker and returns 0.
int findTraceIdentifier(const char* name, TraceIdentifier** table, int* count)
{
    for (int i = 0; i < *count; ++i) {
        if (bf2MemoryStricmp(name, table[i]->name) == 0)
            return i;
    }
    if (*count == kMaxTraceIdentifiers) {
        std::strncpy(table[0]->name, "ERROR. TOO MANY IDENTIFIERS", 0x100);
        return 0;
    }
    table[*count] = new TraceIdentifier;
    std::strncpy(table[*count]->name, name, 0x100);
    return (*count)++;
}

// PE 0x10002B90. Opens a trace scope on the calling thread, resolving `id` from `name` first.
void pushMemoryTrace(int& id, const char* name)
{
    if (g_traceStackTop >= kMaxTraceDepth)
        return;
    if (id == -1)
        id = findTraceIdentifier(name, g_traceIdentifiers, &g_traceIdentifierCount);
    if (id < 0 || id >= kMaxTraceIdentifiers)
        return;
    ++g_traceStackTop;
    g_traceStack[g_traceStackTop].id = id;
    g_traceStack[g_traceStackTop].thread = currentThreadId();
}

// PE 0x10002C00. Closes the innermost scope when it is `id`'s on the calling thread. An empty stack
// is reset to depth 0.
void popMemoryTrace(int id)
{
    if (g_traceStackTop < 0) {
        g_traceStackTop = 0;
        return;
    }
    if (g_traceStack[g_traceStackTop].id == id && g_traceStack[g_traceStackTop].thread == currentThreadId()) {
        g_traceStack[g_traceStackTop].id = -1;
        --g_traceStackTop;
    }
}

// PE 0x10002C50. Records an allocation against the innermost scope; returns its identifier plus
// one, or 0 when no scope is open.
int traceAlloc(int size)
{
    if (g_traceStackTop < 0)
        return 0;
    const int id = g_traceStack[g_traceStackTop].id;
    if (id < 0)
        return 0;
    recordAllocation(g_traceIdentifiers[id], size);
    return id + 1;
}

// PE 0x10002CF0.
void traceFree(int idPlusOne, int size)
{
    if (idPlusOne < 1)
        return;
    TraceIdentifier* const entry = g_traceIdentifiers[idPlusOne - 1];
    --entry->numAllocs;
    entry->bytes -= size;
}

// PE 0x10002D20. qsort order: descending bytes.
int compareTraceBytes(const void* a, const void* b)
{
    const unsigned int left = static_cast<const SortedTrace*>(a)->bytes;
    const unsigned int right = static_cast<const SortedTrace*>(b)->bytes;
    if (left == right)
        return 0;
    return left < right ? 1 : -1;
}

std::string traceFileName(const std::string& path);

// PE 0x10002D40. [INFERRED] File-name part of `path`; nothing calls it. Both separators must occur
// for the cut to happen: the larger find wins and a missing separator reads as npos.
std::string traceFileName(const std::string& path)
{
    std::string::size_type cut = path.rfind("\\", std::string::npos, 1);
    const std::string::size_type slash = path.rfind("/", std::string::npos, 1);
    if (slash > cut)
        cut = slash;
    if (cut == std::string::npos)
        return path;
    return path.substr(cut + 1, std::string::npos);
}

// PE 0x10002DC0. Resolves `id` from `name` on first use and records the allocation.
void resourceAllocate(int& id, const char* name, int size)
{
    if (id == -1) {
        id = findTraceIdentifier(name, s_resourceIdentifiers, &s_resourceIdentifierCount);
        if (id < 0)
            return;
    }
    recordAllocation(s_resourceIdentifiers[id], size);
}

// PE 0x10002E60.
void resourceFree(int& id, const char* name, int size)
{
    if (id == -1) {
        id = findTraceIdentifier(name, s_resourceIdentifiers, &s_resourceIdentifierCount);
        if (id < 0)
            return;
    }
    TraceIdentifier* const entry = s_resourceIdentifiers[id];
    --entry->numAllocs;
    entry->bytes -= size;
}

// PE 0x10002EB0. Builds the report of `table` in the shared report buffer: the byte total, then one
// line per identifier with live allocations, largest first. Returns null for an empty table.
// The per-directory totals of '/'- or '\\'-separated names are collected and then discarded, as
// shipped; they never reach the report.
char* buildTraceReport(TraceIdentifier** table, int count)
{
    SortedTrace sorted[kMaxTraceIdentifiers];
    char prefix[0x400];
    if (count <= 0)
        return 0;

    unsigned int totalBytes = 0;
    for (int i = 0; i < count; ++i) {
        sorted[i].bytes = table[i]->bytes;
        sorted[i].index = i;
        totalBytes += table[i]->bytes;
    }
    std::qsort(sorted, count, sizeof(SortedTrace), compareTraceBytes);

    std::map<std::string, TraceIdentifier> groups;
    std::sprintf(s_report, "Total bytes allocated:%u\n", totalBytes);
    char* out = s_report + std::strlen(s_report);
    for (int k = 0; k < count; ++k) {
        const int index = sorted[k].index;
        TraceIdentifier* const entry = table[index];
        if (entry->numAllocs == 0 || entry->bytes == 0)
            continue;

        int level = 0;
        for (int pos = 0; entry->name[pos]; ++pos) {
            const char c = entry->name[pos];
            if (c != '/' && c != '\\')
                continue;
            std::strncpy(prefix, entry->name, pos);
            prefix[pos] = 0;
            std::string key(prefix);
            std::map<std::string, TraceIdentifier>::iterator group = groups.find(key);
            if (group == groups.end()) {
                TraceIdentifier total;
                total.numAllocs += entry->numAllocs;
                total.bytes += entry->bytes;
                total.minBlock = level;
                groups.insert(std::make_pair(key, total));
            } else {
                group->second.bytes += entry->bytes;
                group->second.numAllocs += entry->numAllocs;
            }
            ++level;
        }

        const char* const bytes = formatNumber(entry->bytes, true);
        const char* const peakNumAllocs = formatNumber(entry->peakNumAllocs, false);
        const char* const numAllocs = formatNumber(entry->numAllocs, false);
        const char* const maxBlock = formatNumber(entry->maxBlock, false);
        const char* const minBlock = formatNumber(entry->minBlock, false);
        const char* const peakBytes = formatNumber(entry->peakBytes, false);
        std::sprintf(out, "Bytes:%s(%s) MinBlock:%s MaxBlock:%s NumAllocs:%s(%s)  %s  [%d]\n", bytes,
                     peakBytes, minBlock, maxBlock, numAllocs, peakNumAllocs, entry->name, index);
        out += std::strlen(out);
    }
    *out = 0;
    return s_report;
}

// PE 0x10003230.
const char* traceReport()
{
    const char* report = buildTraceReport(g_traceIdentifiers, g_traceIdentifierCount);
    return report ? report : "no traces found";
}

// PE 0x10003250.
const char* resourceReport()
{
    const char* report = buildTraceReport(s_resourceIdentifiers, s_resourceIdentifierCount);
    return report ? report : "no resources found";
}

// PE 0x10003270.
void traceReportToFile(const char* fileName)
{
    const char* report = buildTraceReport(g_traceIdentifiers, g_traceIdentifierCount);
    if (!report)
        report = "no traces found";
    FILE* file = std::fopen(fileName, BF2_MEMORY_TEXT_WRITE);
    if (file) {
        std::fprintf(file, "%s", report);
        std::fclose(file);
    }
}

} // namespace memory
} // namespace hfe
} // namespace dice
