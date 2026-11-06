// Public interface of Memory.dll, the engine heap the shipped Battlefield 2 1.50 images import.
//
// The 32 declarations below are the DLL's whole export table. Their parameter and return types come
// from the decorated names (`?bf_malloc@memory@hfe@dice@@YAPAXIH@Z` and so on); an MSVC x86 build
// of this header mangles to the same strings, so a retail importer binds to the rebuilt DLL by name.
//
// The shipped DLL is the "Final" configuration (version string "Memory.dll Ver:1.01
// Date:2005/01/10 - Final"): only the allocator, the max-allocation sanity limit, the status string
// and the resource counters do work. The tracing, histogram, leak-dump and counter exports are
// compiled-out stubs in that configuration, and this reconstruction keeps them so.
//
// Not linked into any game image: the game allocates through the shared CRT (issue #7).

#ifndef BF2_MEMORY_MEMORY_H
#define BF2_MEMORY_MEMORY_H

#if defined(_WIN32)
#  if defined(BF2_MEMORY_BUILD)
#    define BF2_MEMORY_API __declspec(dllexport)
#  else
#    define BF2_MEMORY_API __declspec(dllimport)
#  endif
#else
// [CHOSEN, not recovered] POSIX shared-library visibility for the native ports.
#  define BF2_MEMORY_API __attribute__((visibility("default")))
#endif

namespace dice {
namespace hfe {
namespace memory {

// Allocator.
// Returns a block of at least `size` bytes (a zero size is treated as one byte). The second
// argument is accepted and ignored by the shipped DLL. Returns null only when every allocation
// attempt failed, after the error handler ran.
BF2_MEMORY_API void* bf_malloc(unsigned int size, int unused);
// Releases a block from bf_malloc. Null is ignored. Blocks from the main thread's pools must be
// freed on that thread; anything else is a fatal memory error.
BF2_MEMORY_API void bf_free(void* p);
// Returns the size requested from bf_malloc for `p` (one for a zero-size request), or 0 for null.
BF2_MEMORY_API int getSizeOfAllocation(void* p);

// Sanity limit: a request above it reports a continuable error and is still allocated.
BF2_MEMORY_API void set_max_alloc_size(unsigned int size);
BF2_MEMORY_API unsigned int get_max_alloc_size();
// Restores the default limit of 16 MiB.
BF2_MEMORY_API void reset_max_alloc_size();

// Error reporting. The shipped DLL ignores the registration and keeps its built-in handler.
BF2_MEMORY_API void register_err_callback(void (*callback)(bool continuable, const char* message));

// Status and counters. The counters always read zero in the shipped configuration.
BF2_MEMORY_API char* getStatus(const char* unused);
BF2_MEMORY_API int get_total_bytes_allocated();
BF2_MEMORY_API long long get_accumulated_bytes_allocated();
BF2_MEMORY_API int get_total_allocation_call_count();
BF2_MEMORY_API int get_accumulated_allocation_call_count();
BF2_MEMORY_API void get_callstack_debug_str(char* buffer, int size, int depth);

// Size histogram. Nothing feeds it in the shipped configuration, so the queries read zero and the
// dump writes no file.
BF2_MEMORY_API unsigned int histogramCurrentUsedMemory();
BF2_MEMORY_API unsigned int histogramPeakUsedMemory();
BF2_MEMORY_API void histogramDumpToFile(const char* fileName);
BF2_MEMORY_API void histogramClearPeaks();
BF2_MEMORY_API void histogramClearCurrent();

// Allocation tracing and leak dumps; inactive in the shipped configuration.
BF2_MEMORY_API int addMemoryName(const char* name);
BF2_MEMORY_API void startMemoryTrace(int& id, const char* name);
BF2_MEMORY_API void endMemoryTrace(int id);
BF2_MEMORY_API void memoryTraceReset();
BF2_MEMORY_API const char* memoryTraceReport();
BF2_MEMORY_API void memoryTraceReportToFile(const char* fileName);
BF2_MEMORY_API void nextVisualFrame();
BF2_MEMORY_API void nextPhysicFrame();
BF2_MEMORY_API const char* dump_allocations_to_file(const char* fileName, int includeAll);
BF2_MEMORY_API const char* dump_active_allocations_to_file(const char* fileName);
BF2_MEMORY_API void memory_update_modules(const char* unused);
BF2_MEMORY_API void set_all_allocations_to_zero();

// Named resource counters. `id` starts at -1; the first call resolves `name` (case-insensitive)
// to a slot and stores it.
BF2_MEMORY_API void resourceTraceAllocate(int& id, const char* name, int size);
BF2_MEMORY_API void resourceTraceFree(int& id, const char* name, int size);

} // namespace memory
} // namespace hfe
} // namespace dice

#endif // BF2_MEMORY_MEMORY_H
