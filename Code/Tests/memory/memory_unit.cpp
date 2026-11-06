// [TEMP] Test scaffolding, not recovered source: black-box checks of the rebuilt Memory library
// through its 32 exports. Links the shared library, so the library's
// load hook runs on this process's main thread, as DllMain attach does for the game.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if !defined(_WIN32)
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

#include "Memory.h"

// Mirrors the library's pass-through default (MemoryInternal.h); CMake passes an explicit value
// when BF2_MEMORY_PASSTHROUGH is ON or OFF.
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

using namespace dice::hfe::memory;

namespace {

int g_failures;

#define CHECK(cond)                                                                          \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

// The platform constants of MemoryInternal.h, restated for an independent model.
#if defined(_M_IX86) || defined(__i386__)
const unsigned int kHeader = 4;
const unsigned int kAlign = 4;
#else
const unsigned int kHeader = 16;
const unsigned int kAlign = 16;
#endif
const int kClassSizes[] = {4, 8, 12, 16, 20, 24, 28, 32, 48, 58, 64, 68, 72, 76, 80, 84, 88};
const int kNumClasses = sizeof(kClassSizes) / sizeof(kClassSizes[0]);
const unsigned int kMaxPooled = 88;

unsigned int headerOf(void* p)
{
    unsigned int word;
    std::memcpy(&word, static_cast<char*>(p) - 4, 4);
    return word;
}

bool isPooled(void* p) { return (headerOf(p) & 2) != 0; }
unsigned int chainOf(void* p) { return headerOf(p) >> 17; }

// Smallest class serving a block of `blockSize` bytes, or -1.
int classFor(unsigned int blockSize)
{
    for (int i = 0; i < kNumClasses; ++i)
        if (blockSize <= static_cast<unsigned int>(kClassSizes[i]))
            return i;
    return -1;
}

unsigned int strideOf(int cls)
{
    const unsigned int size = kClassSizes[cls];
    return (size + kAlign - 1) / kAlign * kAlign;
}

// Captures what `fn` writes to stderr.
template <class Fn> std::string captureStderr(Fn fn)
{
#if defined(_WIN32)
    fn();
    return std::string();
#else
    std::fflush(stderr);
    char path[] = "/tmp/bf2mem_stderr_XXXXXX";
    const int fd = mkstemp(path);
    const int saved = dup(2);
    dup2(fd, 2);
    fn();
    std::fflush(stderr);
    dup2(saved, 2);
    close(saved);
    std::string text;
    FILE* f = std::fopen(path, "r");
    if (f) {
        char buf[512];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
            text.append(buf, n);
        std::fclose(f);
    }
    close(fd);
    unlink(path);
    return text;
#endif
}

bool fileExists(const char* path)
{
    FILE* f = std::fopen(path, "r");
    if (!f)
        return false;
    std::fclose(f);
    return true;
}

// Must run first: every class's first slab is still untouched, so its elements come out in
// address order, one stride apart.
void testFreshSlabStrides()
{
    for (int cls = 0; cls < kNumClasses; ++cls) {
        const unsigned int block = kClassSizes[cls];
        if (block <= kHeader)
            continue; // no user size reaches this class
        const unsigned int user = block - kHeader;
        CHECK(classFor(user + kHeader) == cls);
        void* a = bf_malloc(user, 0);
        void* b = bf_malloc(user, 0);
        void* c = bf_malloc(user, 0);
        CHECK(isPooled(a) && isPooled(b) && isPooled(c));
        CHECK(static_cast<char*>(b) - static_cast<char*>(a) == static_cast<long>(strideOf(cls)));
        CHECK(static_cast<char*>(c) - static_cast<char*>(b) == static_cast<long>(strideOf(cls)));
        CHECK(chainOf(a) == 0);
        CHECK(reinterpret_cast<std::size_t>(static_cast<char*>(a) - kHeader) % kAlign == 0);
        bf_free(c);
        bf_free(b);
        bf_free(a);
    }
}

void testSizesAndHeaders()
{
    for (unsigned int size = 0; size <= 300; ++size) {
        void* p = bf_malloc(size, 0);
        CHECK(p != 0);
        const unsigned int effective = size == 0 ? 1 : size;
        CHECK(getSizeOfAllocation(p) == static_cast<int>(effective));
        const unsigned int block = effective + kHeader;
        const bool pooled = !BF2_MEMORY_PASSTHROUGH && block <= kMaxPooled;
        CHECK(isPooled(p) == pooled);
        CHECK((headerOf(p) & 1) == 0);
        if (pooled)
            CHECK(((headerOf(p) >> 2) & 0x7fff) == block);
        else
            CHECK((headerOf(p) >> 2) == block);
        std::memset(p, 0xA5, effective);
        bf_free(p);
    }
    CHECK(getSizeOfAllocation(0) == 0);
    bf_free(0);

    void* big = bf_malloc(1u << 20, 0);
    CHECK(!isPooled(big));
    CHECK(getSizeOfAllocation(big) == (1 << 20));
    CHECK((headerOf(big) >> 2) == (1u << 20) + kHeader);
    std::memset(big, 1, 1u << 20);
    bf_free(big);
}

void testLifoReuse()
{
    for (unsigned int user = 1; user + kHeader <= kMaxPooled; ++user) {
        void* a = bf_malloc(user, 0);
        bf_free(a);
        void* b = bf_malloc(user, 0);
        CHECK(a == b);
        bf_free(b);
    }
}

// Fills the first slab of one class so the next element comes from chain position 1, then shows
// that a freed chain-0 element is served next (the freed slab heads the free list).
void testChainOverflow()
{
    const unsigned int user = 1;
    const int cls = classFor(user + kHeader);
    const unsigned int capacity = 0x100000 / strideOf(cls);
    std::vector<void*> blocks;
    blocks.reserve(capacity + 2);
    void* firstChain1 = 0;
    for (unsigned int i = 0; i < capacity + 1; ++i) {
        void* p = bf_malloc(user, 0);
        if (!firstChain1 && chainOf(p) == 1)
            firstChain1 = p;
        blocks.push_back(p);
    }
    CHECK(firstChain1 != 0);
    CHECK(chainOf(blocks.back()) == 1);
    int chain0 = 0;
    for (size_t i = 0; i < blocks.size(); ++i)
        if (chainOf(blocks[i]) == 0)
            ++chain0;
    CHECK(chain0 == static_cast<int>(capacity));

    void* victim = blocks[10];
    CHECK(chainOf(victim) == 0);
    bf_free(victim);
    void* again = bf_malloc(user, 0);
    CHECK(again == victim);
    CHECK(chainOf(again) == 0);
    blocks[10] = again;
    for (size_t i = 0; i < blocks.size(); ++i)
        bf_free(blocks[i]);
}

void testOtherThread()
{
    void* fromThread = 0;
    int sizeSeen = 0;
    bool pooled = true;
    std::thread worker([&] {
        fromThread = bf_malloc(16, 0);
        sizeSeen = getSizeOfAllocation(fromThread);
        pooled = isPooled(fromThread);
        bf_free(fromThread);
    });
    worker.join();
    CHECK(fromThread != 0);
    CHECK(sizeSeen == 16);
    CHECK(!pooled);
}

void testMaxAllocAndCallback()
{
    CHECK(get_max_alloc_size() == 0x1000000u);
    set_max_alloc_size(1000);
    CHECK(get_max_alloc_size() == 1000u);

    static bool s_callbackRan;
    s_callbackRan = false;
    register_err_callback([](bool, const char*) { s_callbackRan = true; });

    void* p = 0;
    const std::string text = captureStderr([&] { p = bf_malloc(2000, 0); });
    CHECK(p != 0);
    CHECK(getSizeOfAllocation(p) == 2000);
    bf_free(p);
#if !defined(_WIN32)
    CHECK(text.find("BF2 Memory Error: memory.dll: sanity check: block size 2000 (") != std::string::npos);
#endif
    // The shipped DLL ignores the registration.
    CHECK(!s_callbackRan);

    void* q = 0;
    const std::string quiet = captureStderr([&] { q = bf_malloc(1000, 0); });
    CHECK(quiet.empty());
    bf_free(q);

    reset_max_alloc_size();
    CHECK(get_max_alloc_size() == 0x1000000u);
}

void testStubs()
{
    CHECK(std::strcmp(getStatus("x"), "Memory.dll Ver:1.01 Date:2005/01/10 - Final") == 0);
    CHECK(get_total_bytes_allocated() == 0);
    CHECK(get_accumulated_bytes_allocated() == 0);
    CHECK(get_total_allocation_call_count() == 0);
    CHECK(get_accumulated_allocation_call_count() == 0);

    char buffer[64];
    get_callstack_debug_str(buffer, sizeof(buffer), 8);
    CHECK(std::strcmp(buffer, "Only available in DEBUG builds") == 0);

    CHECK(histogramCurrentUsedMemory() == 0);
    CHECK(histogramPeakUsedMemory() == 0);
    histogramClearPeaks();
    histogramClearCurrent();
    std::remove("bf2mem_hist.txt");
    histogramDumpToFile("bf2mem_hist.txt");
    CHECK(!fileExists("bf2mem_hist.txt"));

    CHECK(addMemoryName("Textures") == 0);
    int traceId = -1;
    startMemoryTrace(traceId, "Scope");
    CHECK(traceId == -1);
    endMemoryTrace(traceId);
    memoryTraceReset();
    nextVisualFrame();
    nextPhysicFrame();
    CHECK(std::strcmp(memoryTraceReport(), "use_traceIdentifiers not enabled") == 0);
    std::remove("bf2mem_trace.txt");
    memoryTraceReportToFile("bf2mem_trace.txt");
    CHECK(!fileExists("bf2mem_trace.txt"));

    std::remove("bf2mem_dump.txt");
    CHECK(std::strcmp(dump_allocations_to_file("bf2mem_dump.txt", 1), "no active in final build") == 0);
    CHECK(std::strcmp(dump_active_allocations_to_file("bf2mem_dump.txt"), "no active in final build") == 0);
    CHECK(!fileExists("bf2mem_dump.txt"));
    memory_update_modules("x");
    set_all_allocations_to_zero();
}

void testResourceIds()
{
    int textures = -1, meshes = -1, again = -1, freed = -1;
    resourceTraceAllocate(textures, "Textures", 100);
    resourceTraceAllocate(meshes, "Meshes", 50);
    resourceTraceAllocate(again, "TEXTURES", 10);
    resourceTraceFree(freed, "meshes", 50);
    CHECK(textures == 0);
    CHECK(meshes == 1);
    CHECK(again == 0);
    CHECK(freed == 1);
    // A resolved id is used as is; the name is not looked at again.
    int fixed = 1;
    resourceTraceAllocate(fixed, "Textures", 1);
    CHECK(fixed == 1);
}

#if !defined(_WIN32)
// A pooled block freed on another thread is a non-continuable error: the built-in handler reports
// it and stops the process.
void testFatalCrossThreadFree()
{
    void* pooled = bf_malloc(8, 0);
    CHECK(isPooled(pooled));
    int pipeFd[2];
    CHECK(pipe(pipeFd) == 0);
    const pid_t child = fork();
    if (child == 0) {
        dup2(pipeFd[1], 2);
        close(pipeFd[0]);
        std::thread worker([&] { bf_free(pooled); });
        worker.join();
        _exit(0); // not reached
    }
    close(pipeFd[1]);
    std::string text;
    char buf[256];
    ssize_t n;
    while ((n = read(pipeFd[0], buf, sizeof(buf))) > 0)
        text.append(buf, static_cast<size_t>(n));
    close(pipeFd[0]);
    int status = 0;
    waitpid(child, &status, 0);
    CHECK(WIFSIGNALED(status));
    CHECK(text.find("is deallocating main thread memory") != std::string::npos);
    bf_free(pooled);
}
#endif

} // namespace

int main()
{
    if (!BF2_MEMORY_PASSTHROUGH) {
        testFreshSlabStrides();
    } else {
        void* p = bf_malloc(8, 0);
        CHECK(!isPooled(p));
        bf_free(p);
    }
    testSizesAndHeaders();
    if (!BF2_MEMORY_PASSTHROUGH) {
        testLifoReuse();
        testChainOverflow();
    }
    testOtherThread();
    testMaxAllocAndCallback();
    testStubs();
    testResourceIds();
#if !defined(_WIN32)
    if (!BF2_MEMORY_PASSTHROUGH)
        testFatalCrossThreadFree();
#endif
    std::printf("memory_unit: pass-through %s\n", BF2_MEMORY_PASSTHROUGH ? "on" : "off");
    if (g_failures) {
        std::fprintf(stderr, "memory_unit: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("memory_unit: all checks passed\n");
    return 0;
}
