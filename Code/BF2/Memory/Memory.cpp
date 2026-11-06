// Memory.dll entry point, error handling and the 32 exports.
//
// The shipped "Final" configuration: bf_malloc serves main-thread blocks of up to 88 bytes (header
// included) from ChunkPools and everything else from the CRT heap, behind a one-word header.
// Header bits: 0 always clear, 1 set for a pooled block, 2-16 the block size and 17-31 the slab's
// chain position for a pooled block, 2-31 the block size otherwise.
// Placement [INFERRED].

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MemoryInternal.h"

#if !defined(_WIN32)
#  if defined(__APPLE__)
#    include <pthread.h>
#  else
#    include <sys/syscall.h>
#    include <unistd.h>
#  endif
#endif

namespace dice {
namespace hfe {
namespace memory {

namespace {

// The shipped block header. The shipped code builds it field by field in an uninitialised local
// whose bit 0 it clears first; every other bit is then overwritten, so starting from zero gives the
// same word.
union BlockHeader
{
    struct
    {
        unsigned int reserved : 1;
        unsigned int pooled : 1;
        unsigned int size : 15;
        unsigned int chainIndex : 15;
    } pool;
    struct
    {
        unsigned int reserved : 1;
        unsigned int pooled : 1;
        unsigned int size : 30;
    } heap;
    unsigned int raw;
};

void defaultErrorCallback(bool continuable, const char* message);

const char* s_notActive = "no active in final build";
char* s_status = const_cast<char*>("Memory.dll Ver:1.01 Date:2005/01/10 - Final");
unsigned int s_maxAllocSize = 0x1000000;

// The handler pointer sits in read-only data: register_err_callback cannot replace it.
void (*const s_errorCallback)(bool, const char*) = defaultErrorCallback;

ThreadId s_mainThreadId;
bool s_disabled;
MemoryNameStats s_memoryNames[16];
Mutex s_memoryLock;          // constructed and destroyed with the DLL, never taken

} // namespace

#if !defined(_WIN32)
// [CHOSEN, not recovered] POSIX thread identity.
ThreadId currentThreadId()
{
#  if defined(__APPLE__)
    unsigned long long id = 0;
    pthread_threadid_np(0, &id);
    return id;
#  else
    return static_cast<ThreadId>(syscall(SYS_gettid));
#  endif
}
#endif

// PE 0x10001DB0. [INFERRED] An out-of-line copy of the thread-id helper; nothing calls it.
ThreadId memoryCurrentThreadId()
{
    return currentThreadId();
}

namespace {

// PE 0x10001DC0. Shows the message; a non-continuable error then stops the process with a write
// through a null pointer.
void defaultErrorCallback(bool continuable, const char* message)
{
#if defined(_WIN32)
    MessageBoxA(0, message, "BF2 Memory Error", MB_CANCELTRYCONTINUE | MB_ICONHAND);
#else
    // [CHOSEN, not recovered] No message box on the POSIX ports: the message goes to stderr.
    std::fprintf(stderr, "BF2 Memory Error: %s\n", message);
    std::fflush(stderr);
#endif
    if (!continuable)
        *static_cast<volatile int*>(0) = 0x7306b;
}

} // namespace

// PE 0x10001DF0 (Windows). Attach builds the pools and records the main thread; detach releases the
// pools and, when call stacks were active, the allocation records.
#if defined(_WIN32)
} // namespace memory
} // namespace hfe
} // namespace dice

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    using namespace dice::hfe::memory;
    switch (reason) {
    case DLL_PROCESS_ATTACH:
        ChunkPools::init();
        s_mainThreadId = currentThreadId();
        return TRUE;
    case DLL_PROCESS_DETACH:
        ChunkPools::shutdown();
        if (isCallstackActive()) {
            shutdownCallstack();
            dumpLeaks(0, false);
        }
        break;
    }
    return TRUE;
}

namespace dice {
namespace hfe {
namespace memory {
#else
namespace {

// [CHOSEN, not recovered] The POSIX ports run the DllMain attach and detach bodies from the shared
// library's constructor and destructor.
__attribute__((constructor)) void memoryLibraryLoad()
{
    ChunkPools::init();
    s_mainThreadId = currentThreadId();
}

__attribute__((destructor)) void memoryLibraryUnload()
{
    ChunkPools::shutdown();
    if (isCallstackActive()) {
        shutdownCallstack();
        dumpLeaks(0, false);
    }
}

} // namespace
#endif

// PE 0x10001E50.
MemoryNameStats::MemoryNameStats()
{
    std::memset(this, 0, sizeof(*this));
}

// PE 0x10001E70. Only positive sizes update the byte counters and the peaks.
void MemoryNameStats::alloc(int size)
{
    ++numAllocs;
    if (size <= 0)
        return;
    bytes += size;
    if (size < minBlock || peakBytes == 0)
        minBlock = size;
    if (size > maxBlock)
        maxBlock = size;
    if (numAllocs > peakNumAllocs)
        peakNumAllocs = numAllocs;
    if (bytes > peakBytes)
        peakBytes = bytes;
}

// PE 0x10001EC0.
void MemoryNameStats::free(int size)
{
    --numAllocs;
    bytes -= size;
}

// PE 0x10001EE0.
void memoryError(bool continuable, const char* message)
{
    if (!continuable)
        s_disabled = true;
    s_errorCallback(continuable, message);
}

// PE 0x10001F10. After a fatal error every request goes to the CRT without a header.
void* bf_malloc(unsigned int size, int unused)
{
    (void)unused;
    if (s_disabled)
        return std::malloc(size);

    char message[128];
    if (!(size > 0))
        ++size;
    if (size > s_maxAllocSize) {
        std::sprintf(message, "memory.dll: sanity check: block size %d (%5.2f MB) doesn't seem sane",
                     size, static_cast<double>(size) * (1.0 / 1048576.0));
        memoryError(true, message);
    }

    BlockHeader header;
    header.raw = 0;
    header.pool.reserved = 0;
    const unsigned int blockSize = size + kHeaderSize;
    void* block;
    if (!BF2_MEMORY_PASSTHROUGH && currentThreadId() == s_mainThreadId) {
        int chainIndex = 0; // left unset by a rejected size; the heap path overwrites the bits
        block = ChunkPools::alloc(blockSize, &chainIndex);
        header.pool.chainIndex = chainIndex;
        header.pool.size = blockSize & 0x7fff;
        header.pool.pooled = 1;
        if (block)
            goto store;
    }
    header.heap.pooled = 0;
    header.heap.size = blockSize;
    block = std::malloc(blockSize);
    if (!block) {
        std::sprintf(message, "memory.dll: all alloc attempts failed for size %d", size);
        memoryError(false, message);
        return 0;
    }

store:
    char* const user = static_cast<char*>(block) + kHeaderSize;
    *reinterpret_cast<unsigned int*>(user - 4) = header.raw;
    return user;
}

// PE 0x10002030.
int getSizeOfAllocation(void* p)
{
    if (!p)
        return 0;
    BlockHeader header;
    header.raw = *reinterpret_cast<unsigned int*>(static_cast<char*>(p) - 4);
    if (header.pool.pooled)
        return static_cast<int>(header.pool.size) - static_cast<int>(kHeaderSize);
    return static_cast<int>(header.heap.size) - static_cast<int>(kHeaderSize);
}

// PE 0x10002060. Nothing is released after a fatal error. A pooled block the pools reject is
// reported and then handed to the CRT free.
void bf_free(void* p)
{
    if (s_disabled || !p)
        return;

    char* const block = static_cast<char*>(p) - kHeaderSize;
    BlockHeader header;
    header.raw = *reinterpret_cast<unsigned int*>(static_cast<char*>(p) - 4);
    if (header.pool.pooled) {
        char message[128];
        if (currentThreadId() != s_mainThreadId) {
            std::sprintf(message, "memory.dll: thread %d is deallocating main thread memory",
                         static_cast<int>(currentThreadId()));
            memoryError(false, message);
        }
        if (ChunkPools::free(block, header.pool.size, header.pool.chainIndex))
            return;
        std::sprintf(message, "memory.dll: ChunkPools::free fails at memory 0x%lx",
                     static_cast<unsigned long>(reinterpret_cast<std::size_t>(block)));
        memoryError(false, message);
    }
    std::free(block);
}

// PE 0x10002120.
unsigned int histogramCurrentUsedMemory()
{
    return histogramCurrentTotal();
}

// PE 0x10002130.
unsigned int histogramPeakUsedMemory()
{
    return histogramPeakTotal();
}

// PE 0x10002140.
void histogramDumpToFile(const char* fileName)
{
    histogramDump(fileName);
}

// PE 0x10002150.
void histogramClearPeaks()
{
    histogramResetPeaks();
}

// PE 0x10002160.
void histogramClearCurrent()
{
    histogramResetCurrent();
}

// PE 0x10002170.
char* getStatus(const char* unused)
{
    (void)unused;
    return s_status;
}

// PE 0x10002180.
void memory_update_modules(const char* unused)
{
    (void)unused;
}

// PE 0x10002190.
void set_all_allocations_to_zero()
{
}

// PE 0x100021A0.
const char* dump_allocations_to_file(const char* fileName, int includeAll)
{
    (void)fileName;
    (void)includeAll;
    return s_notActive;
}

// PE 0x100021B0.
const char* dump_active_allocations_to_file(const char* fileName)
{
    (void)fileName;
    return s_notActive;
}

// PE 0x100021C0.
const char* memoryTraceReport()
{
    return "use_traceIdentifiers not enabled";
}

// PE 0x100021D0.
void memoryTraceReportToFile(const char* fileName)
{
    (void)fileName;
}

// PE 0x100021E0.
void startMemoryTrace(int& id, const char* name)
{
    (void)id;
    (void)name;
}

// PE 0x100021F0.
void endMemoryTrace(int id)
{
    (void)id;
}

// PE 0x10002200.
void memoryTraceReset()
{
}

// PE 0x10002210.
void nextVisualFrame()
{
}

// PE 0x10002220.
void nextPhysicFrame()
{
}

// PE 0x10002230.
int addMemoryName(const char* name)
{
    (void)name;
    return 0;
}

// PE 0x10002240.
void resourceTraceAllocate(int& id, const char* name, int size)
{
    resourceAllocate(id, name, size);
}

// PE 0x10002250.
void resourceTraceFree(int& id, const char* name, int size)
{
    resourceFree(id, name, size);
}

// PE 0x10002260. Copies the fixed notice; the size and depth are ignored.
void get_callstack_debug_str(char* buffer, int size, int depth)
{
    (void)size;
    (void)depth;
    std::strcpy(buffer, "Only available in DEBUG builds");
}

// PE 0x10002280. Ignored in the shipped configuration.
void register_err_callback(void (*callback)(bool, const char*))
{
    (void)callback;
}

// PE 0x10002290.
void set_max_alloc_size(unsigned int size)
{
    s_maxAllocSize = size;
}

// PE 0x100022A0.
unsigned int get_max_alloc_size()
{
    return s_maxAllocSize;
}

// PE 0x100022B0.
void reset_max_alloc_size()
{
    s_maxAllocSize = 0x1000000;
}

// PE 0x100022C0.
int get_total_bytes_allocated()
{
    return 0;
}

// PE 0x100022D0.
long long get_accumulated_bytes_allocated()
{
    return 0;
}

// PE 0x100022E0.
int get_total_allocation_call_count()
{
    return 0;
}

// PE 0x100022F0.
int get_accumulated_allocation_call_count()
{
    return 0;
}

} // namespace memory
} // namespace hfe
} // namespace dice
