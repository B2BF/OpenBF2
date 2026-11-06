// Internal declarations shared by the Memory.dll translation units.
//
// Placement and the names of internal types, functions and globals are [INFERRED]: the shipped DLL
// carries no symbols or source paths beyond its 32 exports. The split into translation units
// follows the shipped function and data order, seven contiguous groups:
//   AllocInfo.cpp    0x10001010-0x10001630  call-stack keyed allocation records, leak report v1.10
//   Callstack.cpp    0x10001670-0x100016B0  compiled-out capture, its unused lock
//   ChunkPools.cpp   0x100016C0-0x10001D80  MemoryPool slabs, ChunkPool classes, ChunkPools
//   Memory.cpp       0x10001DB0-0x100022F0  DllMain, error handler, MemoryNameStats, 32 exports
//   Histogram.cpp    0x10002300-0x100025E0  size histogram (never fed in Final)
//   Threading.cpp    0x10002650-0x100029D0  Mutex, Event, Thread
//   MemoryTrace.cpp  0x100029E0-0x10003270  trace/resource identifiers, report builder
// Internal names follow the DLL's own strings (ChunkPools::free, MemoryPool, AllocInfo) and, for the
// call-stack functions, the 2006 Memory_r.dll debug build; the rest are [INFERRED].

#ifndef BF2_MEMORY_MEMORYINTERNAL_H
#define BF2_MEMORY_MEMORYINTERNAL_H

#include <cstddef>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <pthread.h>
#endif

#include "Memory.h"

// [CHOSEN, not recovered] Pass-through switch for sanitizer builds. When on, bf_malloc never uses the
// chunk pools, so every block is its own CRT allocation that AddressSanitizer can see. The block
// header, getSizeOfAllocation, the resource counters and every other export behave as without it.
// Defaults to on when the compiler reports AddressSanitizer, off otherwise.
#ifndef BF2_MEMORY_PASSTHROUGH
#  if defined(__SANITIZE_ADDRESS__)
#    define BF2_MEMORY_PASSTHROUGH 1
#  elif defined(__has_feature)
#    if __has_feature(address_sanitizer)
#      define BF2_MEMORY_PASSTHROUGH 1
#    endif
#  endif
#endif
#ifndef BF2_MEMORY_PASSTHROUGH
#  define BF2_MEMORY_PASSTHROUGH 0
#endif

namespace dice {
namespace hfe {
namespace memory {

// ------------------------------------------------------------------------------------------------
// Platform seam. The MSVC x86 values are the shipped ones.
// ------------------------------------------------------------------------------------------------

#if defined(_M_IX86) || defined(__i386__)
// The block header is one 32-bit word in front of the user pointer.
const unsigned int kHeaderSize = 4;
// Pool element sizes are rounded up to a multiple of four.
const int kPoolAlignment = 4;
#else
// [CHOSEN, not recovered] 64-bit ports keep the 32-bit header word directly in front of the user
// pointer but reserve 16 bytes for it and round pool elements to 16, so blocks keep the platform
// malloc alignment. Every size class therefore pools 12 fewer user bytes than on x86.
const unsigned int kHeaderSize = 16;
const int kPoolAlignment = 16;
#endif

#if defined(_WIN32)
typedef DWORD ThreadId;
inline ThreadId currentThreadId() { return GetCurrentThreadId(); }
#else
// [CHOSEN, not recovered] POSIX thread identity for the main-thread checks.
typedef unsigned long long ThreadId;
ThreadId currentThreadId();
#endif

// fopen mode of every text file the DLL writes.
#if defined(_WIN32)
#  define BF2_MEMORY_TEXT_WRITE "wt"
#else
// [CHOSEN, not recovered] POSIX has no text mode; "w" is the same file.
#  define BF2_MEMORY_TEXT_WRITE "w"
#endif

// ------------------------------------------------------------------------------------------------
// Threading primitives (Threading.cpp). Only Mutex has users, and those only lock the histogram.
// ------------------------------------------------------------------------------------------------

class Mutex
{
public:
    Mutex();
    ~Mutex();
    void lock();
    void unlock();

private:
#if defined(_WIN32)
    CRITICAL_SECTION* m_section;
#else
    pthread_mutex_t* m_section;
#endif
};

// A manual-reset event, initially clear.
class Event
{
public:
    Event();
    ~Event();
    void signal();
    // Waits up to `milliseconds` (0 waits forever); returns whether the event was signalled.
    bool wait(unsigned int milliseconds);

private:
#if defined(_WIN32)
    HANDLE* m_handle;
#else
    struct Impl;
    Impl* m_handle;
#endif
};

class Thread
{
public:
    // Thread priorities accepted by start().
    enum Priority
    {
        PriorityIdle,
        PriorityBelowNormal,
        PriorityNormal,
        PriorityAboveNormal,
        PriorityTimeCritical
    };

    Thread();
    virtual ~Thread();
    // Body run on the new thread.
    virtual void run() = 0;

    // Creates the thread and applies `priority`. The first argument is ignored.
    bool start(int unused, int priority);
    // Waits for the thread to finish and releases its handle.
    bool join();

private:
    struct Data
    {
#if defined(_WIN32)
        HANDLE handle;
        DWORD id;
#else
        pthread_t handle;
        unsigned long id;
#endif
        bool running;
    };

#if defined(_WIN32)
    static DWORD WINAPI threadProc(void* thread);
#else
    static void* threadProc(void* thread);
#endif

    Data* m_data;
};

// [INFERRED] Out-of-line thread-id helpers of the threading and main units; nothing calls them.
ThreadId threadingCurrentThreadId();
ThreadId memoryCurrentThreadId();

// ------------------------------------------------------------------------------------------------
// Errors (Memory.cpp).
// ------------------------------------------------------------------------------------------------

// Reports `message` through the error handler. A non-continuable error first switches the DLL to
// plain CRT allocation; the built-in handler then stops the process.
void memoryError(bool continuable, const char* message);

// ------------------------------------------------------------------------------------------------
// Chunk pools (ChunkPools.cpp).
// ------------------------------------------------------------------------------------------------

// One 1 MiB slab of equal-size elements with an intrusive free list. Slabs of a size class form a
// chain; the first slab also counts the live elements of the whole class.
class MemoryPool
{
public:
    MemoryPool();
    ~MemoryPool();

    void init(int elementSize, int chunkSize);
    // Takes an element, walking to (and creating) later slabs of the chain when this one is full.
    // `chainIndex` holds this slab's position on entry and the serving slab's on return.
    void* alloc(unsigned int size, int* chainIndex);
    void free(void* element);

    int m_numAllocs;             // live elements of the size class; first slab only
    int m_elementSize;
    int m_chunkSize;
    MemoryPool* m_next;          // next slab of the chain
    MemoryPool* m_prev;
    char* m_memory;              // the slab, allocated on first use
    void* m_freeList;
    int m_numUsed;
    int m_capacity;
    int m_chainIndex;
    bool m_inFreeList;
    MemoryPool* m_nextFree;      // ChunkPool free-list link
};

// One size class: its first slab, the list of slabs with room, and the slabs by chain position.
struct ChunkPool
{
    enum { kMaxChains = 512 };

    ChunkPool();
    void addToFreeList(MemoryPool* pool);
    // Drops the head slab from the free list when the next allocation fills it.
    void updateFreeList();

    MemoryPool* m_first;
    MemoryPool* m_freeHead;
    MemoryPool* m_chains[kMaxChains];
};

class ChunkPools
{
public:
    static void init();
    static void shutdown();
    static bool isActive();
    // Returns an element for a block of `size` bytes (header included) and its chain position, or
    // null when the size has no pool.
    static void* alloc(unsigned int size, int* chainIndex);
    static bool free(void* block, int size, int chainIndex);
    static void setInternalPoolByIndex(int size, int chainIndex, MemoryPool* pool);
    // Appends the per-class table to `buffer`; returns the new end.
    static char* getStatus(char* buffer);
};

// ------------------------------------------------------------------------------------------------
// Allocation records and leak report (AllocInfo.cpp), call stacks (Callstack.cpp).
// ------------------------------------------------------------------------------------------------

// Statistics of all allocations made from one call stack.
struct AllocInfo
{
    void update(int size);

    int numFrames;
    unsigned int* frames;
    unsigned int hash;
    int traceId;
    int peakSize;
    int peakNumAllocs;
    int currentNumAllocs;
    int currentTotalSize;
    int minAllocSize;
    int maxAllocSize;
    int visualFrameAllocs;
    int peakVisualFrameAllocs;
    int lastVisualFrame;
    int physicsFrameAllocs;
    int peakPhysicsFrameAllocs;
    int lastPhysicsFrame;
    AllocInfo* next;
};

unsigned int hashCallstack(const unsigned int* frames, int numFrames);
void initAllocInfoHash(int hashSize);
void resetTraceAllocInfo();
const char* allocInfoNotActive();
void dumpAllocInfo(const char* fileName, bool includeAll, bool release, int traceId);
void clearCurrentAllocInfo();
AllocInfo* findAllocInfo(int depth);
void allocInfoNoOp();
void dumpLeaks(const char* fileName, bool includeAll);
void dumpAllAllocInfo(const char* fileName);
void dumpTraceAllocInfo(const char* fileName, int traceId);
void writeAllocInfoNotActive(const char* fileName);

bool isCallstackActive();
void initCallstack();
void shutdownCallstack();
int captureCallstack(unsigned int* frames, int maxFrames, int skipFrames);
int callstackModuleCount();

// ------------------------------------------------------------------------------------------------
// Size histogram (Histogram.cpp).
// ------------------------------------------------------------------------------------------------

unsigned int histogramCurrentTotal();
unsigned int histogramPeakTotal();
void histogramDump(const char* fileName);
void histogramResetPeaks();
void histogramPeaksToCurrent();
void histogramResetCurrent();
void histogramResetAll();
void histogramAlloc(unsigned int size);
void histogramFree(unsigned int size);

// ------------------------------------------------------------------------------------------------
// Trace identifiers and resource counters (MemoryTrace.cpp).
// ------------------------------------------------------------------------------------------------

// Named counter. The resource and trace tables hold up to 2048 of them.
struct TraceIdentifier
{
    TraceIdentifier();

    char name[256];
    int unused;                  // [UNRESOLVED] zeroed by the constructor, never read or written
    int peakBytes;
    int peakNumAllocs;
    int numAllocs;
    int bytes;
    int minBlock;
    int maxBlock;
};

// One open trace scope of the per-process trace stack.
struct TraceStackEntry
{
    TraceStackEntry();

    int id;
    ThreadId thread;
};

enum
{
    kMaxTraceIdentifiers = 0x800,
    kMaxTraceDepth = 0x80
};

extern TraceIdentifier* g_traceIdentifiers[kMaxTraceIdentifiers];
extern int g_traceIdentifierCount;
extern int g_visualFrame;
extern int g_physicsFrame;
extern TraceStackEntry g_traceStack[kMaxTraceDepth + 1];
extern int g_traceStackTop;

const char* formatNumber(int value, bool restart);
int findTraceIdentifier(const char* name, TraceIdentifier** table, int* count);
void pushMemoryTrace(int& id, const char* name);
void popMemoryTrace(int id);
int traceAlloc(int size);
void traceFree(int idPlusOne, int size);
int compareTraceBytes(const void* a, const void* b);
void resourceAllocate(int& id, const char* name, int size);
void resourceFree(int& id, const char* name, int size);
char* buildTraceReport(TraceIdentifier** table, int count);
const char* traceReport();
const char* resourceReport();
void traceReportToFile(const char* fileName);

// ------------------------------------------------------------------------------------------------
// Per-name statistics (Memory.cpp).
// ------------------------------------------------------------------------------------------------

struct MemoryNameStats
{
    MemoryNameStats();
    void alloc(int size);
    void free(int size);

    char name[64];
    int unused;                  // [UNRESOLVED] zeroed by the constructor, never read or written
    int peakBytes;
    int peakNumAllocs;
    int numAllocs;
    int bytes;
    int minBlock;
    int maxBlock;
};

} // namespace memory
} // namespace hfe
} // namespace dice

#endif // BF2_MEMORY_MEMORYINTERNAL_H
