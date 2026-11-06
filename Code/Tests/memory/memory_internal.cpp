// [TEMP] Test scaffolding, not recovered source: white-box checks of the rebuilt Memory library's
// internal functions, linked from the static object set. The shipped configuration never writes
// the leak report, the histogram or the trace report, so their formats are checked here directly.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "MemoryInternal.h"

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

#define CHECK_EQ_STR(actual, expected)                                                       \
    do {                                                                                     \
        const std::string a_ = (actual);                                                     \
        const std::string e_ = (expected);                                                   \
        if (a_ != e_) {                                                                      \
            std::fprintf(stderr, "FAIL %s:%d:\n--- got\n%s\n--- expected\n%s\n", __FILE__,   \
                         __LINE__, a_.c_str(), e_.c_str());                                  \
            ++g_failures;                                                                    \
        }                                                                                    \
    } while (0)

std::string readFile(const char* path)
{
    std::string text;
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return "<missing>";
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
        text.append(buf, n);
    std::fclose(f);
    return text;
}

void testFormatNumber()
{
    CHECK_EQ_STR(formatNumber(0, true), "0");
    CHECK_EQ_STR(formatNumber(999, false), "999");
    CHECK_EQ_STR(formatNumber(1000, false), "1.000");
    CHECK_EQ_STR(formatNumber(1234567, false), "1.234.567");
    CHECK_EQ_STR(formatNumber(-1234, false), "-1.234");
    const char* first = formatNumber(5, true);
    const char* second = formatNumber(6, false);
    CHECK(second == first + 2); // strings of one line follow each other
}

void testHash()
{
    const unsigned int frames[] = {1, 2};
    CHECK(hashCallstack(frames, 2) == 0xdae8739fu);
    CHECK(hashCallstack(frames, 0) == 0x719fdae9u);
}

void testReports()
{
    CHECK_EQ_STR(traceReport(), "no traces found");
    CHECK_EQ_STR(resourceReport(), "no resources found");
    traceReportToFile("bf2mem_internal_trace.txt");
    CHECK_EQ_STR(readFile("bf2mem_internal_trace.txt"), "no traces found");

    // Trace scopes: allocations go to the innermost scope of this thread.
    int outer = -1, inner = -1;
    pushMemoryTrace(outer, "Game/Objects");
    CHECK(outer == 0);
    CHECK(traceAlloc(300) == 1);
    pushMemoryTrace(inner, "Game\\Sound");
    CHECK(inner == 1);
    CHECK(traceAlloc(2000) == 2);
    CHECK(traceAlloc(4000) == 2);
    popMemoryTrace(inner);
    CHECK(traceAlloc(100) == 1);
    traceFree(2, 2000);
    popMemoryTrace(outer);
    CHECK(g_traceStackTop == -1);
    CHECK(traceAlloc(5) == 0);
    popMemoryTrace(outer); // an empty stack resets to depth 0
    CHECK(g_traceStackTop == 0);
    g_traceStackTop = -1;

    CHECK_EQ_STR(traceReport(),
                 "Total bytes allocated:4400\n"
                 "Bytes:4.000(6.000) MinBlock:2.000 MaxBlock:4.000 NumAllocs:1(2)  Game\\Sound  [1]\n"
                 "Bytes:400(400) MinBlock:100 MaxBlock:300 NumAllocs:2(2)  Game/Objects  [0]\n");

    int unused = -1;
    pushMemoryTrace(unused, "Empty");
    popMemoryTrace(unused);
    // A zero-byte identifier stays out of the lines but not out of the total.
    CHECK_EQ_STR(std::string(traceReport()).substr(0, 27), "Total bytes allocated:4400\n");

    int res = -1;
    resourceAllocate(res, "Textures", 1234567);
    CHECK(res == 0);
    resourceAllocate(res, "ignored", 1);
    int res2 = -1;
    resourceAllocate(res2, "textures", 1); // case-insensitive: same slot
    CHECK(res2 == 0);
    CHECK_EQ_STR(resourceReport(),
                 "Total bytes allocated:1234569\n"
                 "Bytes:1.234.569(1.234.569) MinBlock:1 MaxBlock:1.234.567 NumAllocs:3(3)  Textures  [0]\n");
    resourceFree(res, "Textures", 1234569);
    resourceFree(res, "Textures", 0);
    resourceFree(res, "Textures", 0);
    CHECK_EQ_STR(resourceReport(), "Total bytes allocated:0\n");
}

void testHistogram()
{
    CHECK(histogramCurrentTotal() == 0);
    std::remove("bf2mem_internal_hist.txt");
    histogramDump("bf2mem_internal_hist.txt");
    CHECK_EQ_STR(readFile("bf2mem_internal_hist.txt"), "<missing>"); // inactive: no file

    histogramAlloc(16);
    histogramAlloc(16);
    histogramAlloc(40);
    histogramAlloc(20000);
    histogramFree(40);
    CHECK(histogramCurrentTotal() == 20032);
    CHECK(histogramPeakTotal() == 20072);
    histogramDump("bf2mem_internal_hist.txt");
    CHECK_EQ_STR(readFile("bf2mem_internal_hist.txt"),
                 "Allocations greater than 10000 bytes\n"
                 "Current:1 Peak:1\n"
                 "Total bytes allocated. Current:20032 Peak:20072\n\n"
                 "Size:16 Current:2 Peak:2\n"
                 "\nBlocks where current is zero\n\n"
                 "Size:40 Peak:1\n");

    histogramFree(20000);
    histogramFree(20000); // the large count stays at zero
    histogramResetPeaks();
    histogramDump("bf2mem_internal_hist.txt");
    CHECK_EQ_STR(readFile("bf2mem_internal_hist.txt"),
                 "Allocations greater than 10000 bytes\n"
                 "Current:0 Peak:1\n"
                 "Total bytes allocated. Current:4294947328 Peak:20072\n\n"
                 "Size:16 Current:2 Peak:0\n"
                 "\nBlocks where current is zero\n\n");
    histogramPeaksToCurrent();
    histogramResetCurrent();
    histogramDump("bf2mem_internal_hist.txt");
    CHECK(readFile("bf2mem_internal_hist.txt").find("\nBlocks where current is zero\n\nSize:16 Peak:2\n") !=
          std::string::npos);
    histogramResetAll();
    std::remove("bf2mem_internal_hist.txt");
}

void testAllocInfo()
{
    initAllocInfoHash(3);
    AllocInfo* a = findAllocInfo(64);
    CHECK(a != 0);
    CHECK(a->numFrames == 0); // the capture is compiled out
    CHECK(findAllocInfo(64) == a);
    a->update(100);
    a->update(50);
    a->update(-50);
    CHECK(a->traceId == -1);
    dumpAllocInfo("bf2mem_internal_leaks.txt", false, false, -1);
    CHECK_EQ_STR(readFile("bf2mem_internal_leaks.txt"),
                 "# Leak report format v1.10\n"
                 "\n# Module dump\n"
                 "\n# Allocation history (leaks only)\n"
                 "alloc depth=0 peakSize=150 peakNumAllocs=2 currentNumAllocs=1 currentTotalSize=100 "
                 "minAllocSize=50 maxAllocSize=100 \n"
                 "\n# AllocInfo hash table summary:\n"
                 "# hash size: 3\n"
                 "# populated bucket indices: 1 (   33%)\n"
                 "# total elements: 1\n"
                 "# longest chain: 1\n");

    dumpTraceAllocInfo("bf2mem_internal_leaks.txt", 5); // no record of identifier 5
    CHECK(readFile("bf2mem_internal_leaks.txt").find("alloc depth") == std::string::npos);
    CHECK(readFile("bf2mem_internal_leaks.txt").find("# longest chain: 0\n") != std::string::npos);

    clearCurrentAllocInfo();
    CHECK(a->currentNumAllocs == 0 && a->peakSize == 150);
    dumpAllAllocInfo("bf2mem_internal_leaks.txt");
    CHECK(readFile("bf2mem_internal_leaks.txt").find("alloc depth") == std::string::npos);
    dumpLeaks("bf2mem_internal_leaks.txt", true); // all records listed, then released
    CHECK(readFile("bf2mem_internal_leaks.txt").find("(all allocations)\nalloc depth=0 peakSize=150") !=
          std::string::npos);

    writeAllocInfoNotActive("bf2mem_internal_leaks.txt");
    CHECK_EQ_STR(readFile("bf2mem_internal_leaks.txt"), "not active in final build");
    CHECK_EQ_STR(allocInfoNotActive(), "not active in final build");
    std::remove("bf2mem_internal_leaks.txt");
    std::remove("bf2mem_internal_trace.txt");
}

void testChunkPoolStatus()
{
    char status[1024];
    std::string expectedSizes, expectedCounts;
    const int sizes[] = {4, 8, 12, 16, 20, 24, 28, 32, 48, 58, 64, 68, 72, 76, 80, 84, 88};
    char cell[32];
    for (int i = 0; i < 17; ++i) {
        std::snprintf(cell, sizeof(cell), "%-16d", sizes[i]);
        expectedSizes += cell;
    }
    void* p = 0;
    if (!BF2_MEMORY_PASSTHROUGH)
        p = bf_malloc(88 - kHeaderSize, 0);
    for (int i = 0; i < 17; ++i) {
        std::snprintf(cell, sizeof(cell), "%-16u", (i == 16 && p) ? 1u : 0u);
        expectedCounts += cell;
    }
    char* end = ChunkPools::getStatus(status);
    CHECK(ChunkPools::isActive());
    CHECK_EQ_STR(std::string(status, end), "ChunkPools\n" + expectedSizes + "\n" + expectedCounts + "\n");
    bf_free(p);
}

void testNameStats()
{
    MemoryNameStats stats;
    CHECK(stats.numAllocs == 0 && stats.name[0] == 0);
    stats.alloc(0); // counts, but touches nothing else
    CHECK(stats.numAllocs == 1 && stats.peakNumAllocs == 0 && stats.bytes == 0);
    stats.alloc(30);
    stats.alloc(10);
    CHECK(stats.bytes == 40 && stats.minBlock == 10 && stats.maxBlock == 30);
    CHECK(stats.peakNumAllocs == 3 && stats.peakBytes == 40);
    stats.free(30);
    CHECK(stats.numAllocs == 2 && stats.bytes == 10);
}

struct CountingThread : Thread
{
    Event* done;
    int ran;
    void run() override
    {
        ++ran;
        done->signal();
    }
};

void testThreading()
{
    Mutex mutex;
    mutex.lock();
    mutex.lock(); // recursive, as a critical section
    mutex.unlock();
    mutex.unlock();

    Event event;
    CHECK(!event.wait(10));
    CountingThread thread;
    thread.done = &event;
    thread.ran = 0;
    CHECK(thread.start(0, Thread::PriorityNormal));
    CHECK(event.wait(0));
    CHECK(thread.join());
    CHECK(thread.ran == 1);
    CHECK(threadingCurrentThreadId() == currentThreadId());
    CHECK(memoryCurrentThreadId() == currentThreadId());
}

} // namespace

int main()
{
#if defined(_WIN32)
    ChunkPools::init(); // DllMain does this in the DLL; this executable links the objects
#endif
    testFormatNumber();
    testHash();
    testReports();
    testHistogram();
    testAllocInfo();
    testChunkPoolStatus();
    testNameStats();
    testThreading();
    if (g_failures) {
        std::fprintf(stderr, "memory_internal: %d failure(s)\n", g_failures);
        return 1;
    }
    std::printf("memory_internal: all checks passed\n");
    return 0;
}
