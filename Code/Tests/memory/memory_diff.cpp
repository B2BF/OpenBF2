// [TEMP] Test scaffolding, not recovered source: differential harness for Memory.dll.
//
//   memory_diff.exe <path to Memory.dll>
//
// Loads the given DLL (the retail one or the rebuilt MSVC x86 build), resolves the 32 exports by
// their decorated names, runs one deterministic script and prints everything the API makes
// observable: return values, block headers, element strides and reuse inside the pools, chain
// positions, resource ids and which files get written. Absolute addresses are never printed. Run
// it once per DLL and compare the two outputs (memory_diff.cmd).
//
// The script avoids the paths that open the DLL's message box (sanity limit exceeded, fatal
// errors), because the box would block the run.

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

typedef void* (__cdecl* BfMalloc)(unsigned int, int);
typedef void (__cdecl* BfFree)(void*);
typedef int (__cdecl* GetSize)(void*);
typedef void (__cdecl* SetUInt)(unsigned int);
typedef unsigned int (__cdecl* GetUInt)();
typedef void (__cdecl* Void)();
typedef int (__cdecl* GetInt)();
typedef long long (__cdecl* GetInt64)();
typedef char* (__cdecl* GetStatus)(const char*);
typedef void (__cdecl* CallstackStr)(char*, int, int);
typedef void (__cdecl* FileFn)(const char*);
typedef int (__cdecl* AddName)(const char*);
typedef void (__cdecl* StartTrace)(int&, const char*);
typedef void (__cdecl* EndTrace)(int);
typedef const char* (__cdecl* Report)();
typedef const char* (__cdecl* DumpAll)(const char*, int);
typedef const char* (__cdecl* DumpActive)(const char*);
typedef void (__cdecl* ResourceFn)(int&, const char*, int);
typedef void (__cdecl* ErrCallback)(bool, const char*);
typedef void (__cdecl* RegisterCb)(ErrCallback);

HMODULE g_dll;
int g_missing;

FARPROC resolve(const char* name)
{
    FARPROC p = GetProcAddress(g_dll, name);
    if (!p) {
        std::printf("missing export %s\n", name);
        ++g_missing;
    }
    return p;
}

#define RESOLVE(type, var, name) type var = reinterpret_cast<type>(resolve(name))

unsigned int headerOf(void* p)
{
    return *reinterpret_cast<unsigned int*>(static_cast<char*>(p) - 4);
}

bool fileExists(const char* path)
{
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

BfMalloc g_malloc;
BfFree g_free;
GetSize g_size;

struct ThreadResult
{
    unsigned int header;
    int size;
};

DWORD WINAPI otherThread(void* param)
{
    ThreadResult* result = static_cast<ThreadResult*>(param);
    void* p = g_malloc(24, 0);
    result->header = headerOf(p);
    result->size = g_size(p);
    g_free(p);
    return 0;
}

void __cdecl ignoredCallback(bool, const char*)
{
    std::printf("callback ran\n");
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: memory_diff <Memory.dll>\n");
        return 2;
    }
    g_dll = LoadLibraryA(argv[1]);
    if (!g_dll) {
        std::fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError());
        return 2;
    }

    RESOLVE(BfMalloc, bfMalloc, "?bf_malloc@memory@hfe@dice@@YAPAXIH@Z");
    RESOLVE(BfFree, bfFree, "?bf_free@memory@hfe@dice@@YAXPAX@Z");
    RESOLVE(GetSize, getSize, "?getSizeOfAllocation@memory@hfe@dice@@YAHPAX@Z");
    RESOLVE(SetUInt, setMax, "?set_max_alloc_size@memory@hfe@dice@@YAXI@Z");
    RESOLVE(GetUInt, getMax, "?get_max_alloc_size@memory@hfe@dice@@YAIXZ");
    RESOLVE(Void, resetMax, "?reset_max_alloc_size@memory@hfe@dice@@YAXXZ");
    RESOLVE(RegisterCb, registerCb, "?register_err_callback@memory@hfe@dice@@YAXP6AX_NPBD@Z@Z");
    RESOLVE(GetStatus, getStatus, "?getStatus@memory@hfe@dice@@YAPADPBD@Z");
    RESOLVE(GetInt, totalBytes, "?get_total_bytes_allocated@memory@hfe@dice@@YAHXZ");
    RESOLVE(GetInt64, accBytes, "?get_accumulated_bytes_allocated@memory@hfe@dice@@YA_JXZ");
    RESOLVE(GetInt, totalCalls, "?get_total_allocation_call_count@memory@hfe@dice@@YAHXZ");
    RESOLVE(GetInt, accCalls, "?get_accumulated_allocation_call_count@memory@hfe@dice@@YAHXZ");
    RESOLVE(CallstackStr, callstackStr, "?get_callstack_debug_str@memory@hfe@dice@@YAXPADHH@Z");
    RESOLVE(GetUInt, histCurrent, "?histogramCurrentUsedMemory@memory@hfe@dice@@YAIXZ");
    RESOLVE(GetUInt, histPeak, "?histogramPeakUsedMemory@memory@hfe@dice@@YAIXZ");
    RESOLVE(FileFn, histDump, "?histogramDumpToFile@memory@hfe@dice@@YAXPBD@Z");
    RESOLVE(Void, histClearPeaks, "?histogramClearPeaks@memory@hfe@dice@@YAXXZ");
    RESOLVE(Void, histClearCurrent, "?histogramClearCurrent@memory@hfe@dice@@YAXXZ");
    RESOLVE(AddName, addName, "?addMemoryName@memory@hfe@dice@@YAHPBD@Z");
    RESOLVE(StartTrace, startTrace, "?startMemoryTrace@memory@hfe@dice@@YAXAAHPBD@Z");
    RESOLVE(EndTrace, endTrace, "?endMemoryTrace@memory@hfe@dice@@YAXH@Z");
    RESOLVE(Void, traceReset, "?memoryTraceReset@memory@hfe@dice@@YAXXZ");
    RESOLVE(Report, traceReport, "?memoryTraceReport@memory@hfe@dice@@YAPBDXZ");
    RESOLVE(FileFn, traceReportFile, "?memoryTraceReportToFile@memory@hfe@dice@@YAXPBD@Z");
    RESOLVE(Void, nextVisual, "?nextVisualFrame@memory@hfe@dice@@YAXXZ");
    RESOLVE(Void, nextPhysic, "?nextPhysicFrame@memory@hfe@dice@@YAXXZ");
    RESOLVE(DumpAll, dumpAll, "?dump_allocations_to_file@memory@hfe@dice@@YAPBDPBDH@Z");
    RESOLVE(DumpActive, dumpActive, "?dump_active_allocations_to_file@memory@hfe@dice@@YAPBDPBD@Z");
    RESOLVE(FileFn, updateModules, "?memory_update_modules@memory@hfe@dice@@YAXPBD@Z");
    RESOLVE(Void, allToZero, "?set_all_allocations_to_zero@memory@hfe@dice@@YAXXZ");
    RESOLVE(ResourceFn, resAlloc, "?resourceTraceAllocate@memory@hfe@dice@@YAXAAHPBDH@Z");
    RESOLVE(ResourceFn, resFree, "?resourceTraceFree@memory@hfe@dice@@YAXAAHPBDH@Z");
    if (g_missing)
        return 3;
    g_malloc = bfMalloc;
    g_free = bfFree;
    g_size = getSize;

    // 1. Fresh slabs: three elements per block size, strides and headers.
    for (unsigned int size = 0; size <= 90; ++size) {
        void* a = bfMalloc(size, 0);
        void* b = bfMalloc(size, 0);
        void* c = bfMalloc(size, 0);
        const bool pooled = (headerOf(a) & 2) != 0;
        std::printf("fresh size=%u hdr=%08x %08x %08x getSize=%d stride=%d,%d align4=%d\n", size,
                    headerOf(a), headerOf(b), headerOf(c), getSize(a),
                    pooled ? static_cast<int>(static_cast<char*>(b) - static_cast<char*>(a)) : 0,
                    pooled ? static_cast<int>(static_cast<char*>(c) - static_cast<char*>(b)) : 0,
                    static_cast<int>(reinterpret_cast<UINT_PTR>(a) & 3));
        bfFree(c);
        bfFree(b);
        bfFree(a);
    }

    // 2. Sizes and headers across the range, and LIFO reuse.
    for (unsigned int size = 0; size <= 4200; size += (size < 120 ? 1 : 97)) {
        void* a = bfMalloc(size, 0);
        std::memset(a, 0x5A, size ? size : 1);
        const unsigned int header = headerOf(a);
        const int got = getSize(a);
        bfFree(a);
        void* b = bfMalloc(size, 0);
        std::printf("size=%u hdr=%08x getSize=%d reuse=%d\n", size, header, got,
                    (header & 2) ? (a == b ? 1 : 0) : -1);
        bfFree(b);
    }
    void* big = bfMalloc(3u << 20, 0);
    std::printf("big hdr=%08x getSize=%d\n", headerOf(big), getSize(big));
    bfFree(big);
    std::printf("null getSize=%d\n", getSize(0));
    bfFree(0);

    // 3. Chain overflow of the class serving 1-byte requests.
    {
        std::vector<void*> blocks;
        int perChain[3] = {0, 0, 0};
        for (int i = 0; i < 131072 * 2 + 5; ++i) {
            void* p = bfMalloc(1, 0);
            const unsigned int chain = headerOf(p) >> 17;
            if (chain < 3)
                ++perChain[chain];
            blocks.push_back(p);
        }
        std::printf("chain counts %d %d %d last=%u\n", perChain[0], perChain[1], perChain[2],
                    headerOf(blocks.back()) >> 17);
        void* victim0 = blocks[7];
        void* victim1 = blocks[131072 + 9];
        bfFree(victim0);
        bfFree(victim1);
        void* r1 = bfMalloc(1, 0);
        void* r2 = bfMalloc(1, 0);
        void* r3 = bfMalloc(1, 0);
        std::printf("reuse r1=%s chain=%u r2=%s chain=%u r3chain=%u\n",
                    r1 == victim1 ? "victim1" : (r1 == victim0 ? "victim0" : "other"), headerOf(r1) >> 17,
                    r2 == victim1 ? "victim1" : (r2 == victim0 ? "victim0" : "other"), headerOf(r2) >> 17,
                    headerOf(r3) >> 17);
        blocks[7] = 0;
        blocks[131072 + 9] = 0;
        bfFree(r1);
        bfFree(r2);
        bfFree(r3);
        for (size_t i = 0; i < blocks.size(); ++i)
            bfFree(blocks[i]);
        void* after = bfMalloc(1, 0);
        std::printf("after release chain=%u\n", headerOf(after) >> 17);
        bfFree(after);
    }

    // 4. Another thread allocates from the CRT heap.
    {
        ThreadResult result = {0, 0};
        HANDLE thread = CreateThread(0, 0, otherThread, &result, 0, 0);
        WaitForSingleObject(thread, INFINITE);
        CloseHandle(thread);
        std::printf("thread hdr=%08x getSize=%d\n", result.header, result.size);
    }

    // 5. Limits, status and stubs.
    std::printf("max=%u\n", getMax());
    setMax(12345);
    std::printf("max=%u\n", getMax());
    void* under = bfMalloc(12345, 0);
    std::printf("at limit hdr=%08x\n", headerOf(under));
    bfFree(under);
    resetMax();
    std::printf("max=%u\n", getMax());
    registerCb(ignoredCallback);
    std::printf("status=%s\n", getStatus("x"));
    std::printf("counters %d %lld %d %d\n", totalBytes(), accBytes(), totalCalls(), accCalls());
    char buffer[64];
    std::memset(buffer, '#', sizeof(buffer));
    callstackStr(buffer, 8, 4);
    buffer[63] = 0;
    std::printf("callstack=%s\n", buffer);
    std::printf("hist %u %u\n", histCurrent(), histPeak());
    histClearPeaks();
    histClearCurrent();
    DeleteFileA("memdiff_hist.txt");
    histDump("memdiff_hist.txt");
    std::printf("hist file=%d\n", fileExists("memdiff_hist.txt") ? 1 : 0);
    std::printf("addMemoryName=%d\n", addName("Name"));
    int traceId = -1;
    startTrace(traceId, "Trace");
    std::printf("trace id=%d\n", traceId);
    endTrace(traceId);
    traceReset();
    nextVisual();
    nextPhysic();
    std::printf("trace report=%s\n", traceReport());
    DeleteFileA("memdiff_trace.txt");
    traceReportFile("memdiff_trace.txt");
    std::printf("trace file=%d\n", fileExists("memdiff_trace.txt") ? 1 : 0);
    DeleteFileA("memdiff_dump.txt");
    std::printf("dump=%s\n", dumpAll("memdiff_dump.txt", 1));
    std::printf("dump active=%s\n", dumpActive("memdiff_dump.txt"));
    std::printf("dump file=%d\n", fileExists("memdiff_dump.txt") ? 1 : 0);
    updateModules("x");
    allToZero();

    // 6. Resource ids.
    int r[6] = {-1, -1, -1, -1, -1, 1};
    resAlloc(r[0], "Textures", 100);
    resAlloc(r[1], "Meshes", 10);
    resAlloc(r[2], "textures", 5);
    resFree(r[3], "MESHES", 10);
    resAlloc(r[4], "Sounds", 1);
    resAlloc(r[5], "Ignored", 1);
    std::printf("resource ids %d %d %d %d %d %d\n", r[0], r[1], r[2], r[3], r[4], r[5]);

    std::printf("done\n");
    return 0;
}
