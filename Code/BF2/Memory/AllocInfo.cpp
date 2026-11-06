// Allocation records keyed by call stack, and the leak report written from them.
//
// In the shipped configuration nothing creates records: the call-stack capture is compiled out
// (Callstack.cpp) and the only caller, the DLL detach path, dumps an empty table without a file.
// The bodies are kept as shipped. Placement [INFERRED].

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MemoryInternal.h"

namespace dice {
namespace hfe {
namespace memory {

namespace {

int s_hashSize;
AllocInfo** s_hashTable;

} // namespace

// PE 0x10001010. Rotating XOR over the return addresses.
unsigned int hashCallstack(const unsigned int* frames, int numFrames)
{
    unsigned int hash = 0x719fdae9;
    for (int i = numFrames; i > 0; --i) {
        hash ^= *frames++;
        hash = (hash << 8) | (hash >> 24);
    }
    return hash;
}

// PE 0x10001040.
void initAllocInfoHash(int hashSize)
{
    s_hashSize = hashSize;
    s_hashTable = static_cast<AllocInfo**>(std::malloc(hashSize * sizeof(AllocInfo*)));
    std::memset(s_hashTable, 0, hashSize * sizeof(AllocInfo*));
}

// PE 0x10001080. Clears the counters of every record that belongs to a trace identifier, identifier
// by identifier.
void resetTraceAllocInfo()
{
    for (int traceId = 0; traceId < g_traceIdentifierCount; ++traceId) {
        for (int i = 0; i < s_hashSize; ++i) {
            for (AllocInfo* info = s_hashTable[i]; info; info = info->next) {
                if (info->traceId != traceId)
                    continue;
                info->currentNumAllocs = 0;
                info->currentTotalSize = 0;
                info->peakSize = 0;
                info->peakNumAllocs = 0;
                info->visualFrameAllocs = 0;
                info->physicsFrameAllocs = 0;
                info->peakVisualFrameAllocs = 0;
                info->peakPhysicsFrameAllocs = 0;
            }
        }
    }
}

// PE 0x100010F0.
const char* allocInfoNotActive()
{
    return "not active in final build";
}

// PE 0x10001100. Writes the leak report to `fileName` (when given) for the records of `traceId`
// (-1 for all), listing only records with live allocations unless `includeAll`. `release` frees
// each listed record and then the table; the records' frame arrays are not freed.
// The report header is written without checking that the file opened.
void dumpAllocInfo(const char* fileName, bool includeAll, bool release, int traceId)
{
    FILE* file = 0;
    if (fileName) {
        file = std::fopen(fileName, BF2_MEMORY_TEXT_WRITE);
        std::fprintf(file, "# Leak report format v1.10\n");
    }

    int populated = 0;
    int longestChain = 0;
    if (file) {
        std::fprintf(file, "\n# Module dump\n");
        std::fprintf(file, "\n# Allocation history (%s)\n", includeAll ? "all allocations" : "leaks only");
    }

    int totalElements = 0;
    for (int bucket = 0; bucket < s_hashSize; ++bucket) {
        AllocInfo* info = s_hashTable[bucket];
        if (info)
            ++populated;
        int chain = 0;
        if (!info)
            continue;
        for (;;) {
            AllocInfo* next = info->next;
            if (traceId != -1 && info->traceId != traceId) {
                // A non-matching record ends the bucket's walk without updating the longest chain
                // when it is the last one.
                info = next;
                if (!info)
                    break;
                continue;
            }
            if (file && (info->currentNumAllocs != 0 || includeAll)) {
                std::fprintf(file,
                             "alloc depth=%d peakSize=%d peakNumAllocs=%d currentNumAllocs=%d "
                             "currentTotalSize=%d minAllocSize=%d maxAllocSize=%d ",
                             info->numFrames, info->peakSize, info->peakNumAllocs,
                             info->currentNumAllocs, info->currentTotalSize, info->minAllocSize,
                             info->maxAllocSize);
                for (int i = 0; i < info->numFrames; ++i)
                    std::fprintf(file, "0x%08x%s", info->frames[i], i + 1 < info->numFrames ? "<-" : "");
                std::fprintf(file, "\n");
            }
            ++totalElements;
            if (release)
                std::free(info);
            ++chain;
            info = next;
            if (!info) {
                if (chain > longestChain)
                    longestChain = chain;
                break;
            }
        }
    }

    if (file) {
        std::fprintf(file, "\n# AllocInfo hash table summary:\n");
        std::fprintf(file, "# hash size: %d\n", s_hashSize);
        // x87: the int product with 100.0f is divided by the int size at extended precision and
        // stored as a double.
        std::fprintf(file, "# populated bucket indices: %d (%5.2g%%)\n", populated,
                     static_cast<double>(populated) * 100.0 / static_cast<double>(s_hashSize));
        std::fprintf(file, "# total elements: %d\n", totalElements);
        std::fprintf(file, "# longest chain: %d\n", longestChain);
        std::fclose(file);
    }
    if (release)
        std::free(s_hashTable);
}

// PE 0x10001310. Clears the live counters of every record. The longest chain is counted and then
// discarded.
void clearCurrentAllocInfo()
{
    int longestChain = 0;
    for (int bucket = 0; bucket < s_hashSize; ++bucket) {
        int chain = 0;
        AllocInfo* info = s_hashTable[bucket];
        if (!info)
            continue;
        do {
            info->currentNumAllocs = 0;
            info->currentTotalSize = 0;
            info = info->next;
            ++chain;
        } while (info);
        if (chain > longestChain)
            longestChain = chain;
    }
    (void)longestChain;
}

// PE 0x10001360. Returns the record of the caller's call stack (at most 128 frames, skipping 3),
// creating it at the end of its bucket. A found record moves to the front of its bucket. Returns
// null when the capture fails.
AllocInfo* findAllocInfo(int depth)
{
    unsigned int frames[128];
    if (depth > 128)
        depth = 128;
    const int numFrames = captureCallstack(frames, depth, 3);
    if (numFrames == -1)
        return 0;

    const unsigned int hash = hashCallstack(frames, numFrames);
    const unsigned int bucket = hash % static_cast<unsigned int>(s_hashSize);
    AllocInfo* prev = 0;
    AllocInfo* const head = s_hashTable[bucket];
    for (AllocInfo* info = head; info; info = info->next) {
        if (info->hash == hash && info->numFrames == numFrames &&
            std::memcmp(info->frames, frames, numFrames * sizeof(unsigned int)) == 0) {
            if (head != info) {
                AllocInfo* const next = info->next;
                s_hashTable[bucket] = info;
                info->next = head;
                if (prev)
                    prev->next = next;
            }
            return info;
        }
        prev = info;
    }

    AllocInfo* info = static_cast<AllocInfo*>(std::malloc(sizeof(AllocInfo)));
    std::memset(info, 0, sizeof(AllocInfo));
    info->hash = hash;
    info->frames = static_cast<unsigned int*>(std::malloc(numFrames * sizeof(unsigned int)));
    std::memcpy(info->frames, frames, numFrames * sizeof(unsigned int));
    info->numFrames = numFrames;
    if (prev)
        prev->next = info;
    else
        s_hashTable[hash % static_cast<unsigned int>(s_hashSize)] = info;
    return info;
}

// PE 0x100014C0. Empty in the shipped configuration.
void allocInfoNoOp()
{
}

// PE 0x100014D0. Records an allocation of `size` bytes (a negative size records a free of -size).
// An allocation is attributed to the innermost trace scope opened on the calling thread, and its
// per-frame count restarts when the visual or physics frame advanced.
void AllocInfo::update(int size)
{
    if (!(size >= 0)) {
        currentNumAllocs += -1;
        currentTotalSize -= -size;
        return;
    }

    traceId = -1;
    if (g_traceStackTop != -1) {
        const TraceStackEntry& entry = g_traceStack[g_traceStackTop];
        if (entry.thread == currentThreadId())
            traceId = entry.id;
    }

    visualFrameAllocs += 1;
    if (visualFrameAllocs > peakVisualFrameAllocs)
        peakVisualFrameAllocs = visualFrameAllocs;
    if (lastVisualFrame != g_visualFrame) {
        visualFrameAllocs = 0;
        lastVisualFrame = g_visualFrame;
    }
    physicsFrameAllocs += 1;
    if (physicsFrameAllocs > peakPhysicsFrameAllocs)
        peakPhysicsFrameAllocs = physicsFrameAllocs;
    if (lastPhysicsFrame != g_physicsFrame) {
        physicsFrameAllocs = 0;
        lastPhysicsFrame = g_physicsFrame;
    }

    currentNumAllocs += 1;
    currentTotalSize += size;
    if (peakSize == 0) {
        minAllocSize = size;
        maxAllocSize = size;
    } else {
        if (minAllocSize > size)
            minAllocSize = size;
        if (maxAllocSize < size)
            maxAllocSize = size;
    }
    if (peakSize < currentTotalSize)
        peakSize = currentTotalSize;
    if (peakNumAllocs < currentNumAllocs)
        peakNumAllocs = currentNumAllocs;
}

// PE 0x100015C0. Writes the leak report and releases the records.
void dumpLeaks(const char* fileName, bool includeAll)
{
    dumpAllocInfo(fileName, includeAll, true, -1);
}

// PE 0x100015E0.
void dumpAllAllocInfo(const char* fileName)
{
    dumpAllocInfo(fileName, false, false, -1);
}

// PE 0x10001600. A negative identifier lists every record of the identifier, live or not.
void dumpTraceAllocInfo(const char* fileName, int traceId)
{
    dumpAllocInfo(fileName, traceId < 0, false, traceId);
}

// PE 0x10001630.
void writeAllocInfoNotActive(const char* fileName)
{
    FILE* file = std::fopen(fileName, BF2_MEMORY_TEXT_WRITE);
    if (file) {
        std::fprintf(file, "%s", "not active in final build");
        std::fclose(file);
    }
}

} // namespace memory
} // namespace hfe
} // namespace dice
