// Diagnostic frontier support, [CHOSEN, not recovered]. Default behavior logs and aborts.
// BF2_RENDDX9_FRONTIER=continue permits call-order investigation only; it does not validate behavior.
// Unknown-arity placeholders can unbalance the x86 stack when allowed to return, so only the first
// frontier reached in such a run is reliable. Keep abort as the default.
// [RETIRE-WHEN: RW-48] Delete this helper when the last RD9_FRONTIER in the image is recovered.

#ifndef RENDDX9ABI_FRONTIER_H
#define RENDDX9ABI_FRONTIER_H

namespace renddx9abi {

// Opens the log beside the running executable (i.e. in the game directory, which is BF2's working
// directory) and writes a banner. Safe to call more than once. Called from initDll.
void logOpen();
void logClose();

// printf-style, newline appended. Every line is timestamped and carries the calling thread id, so
// an out-of-order sequence in the log is visible rather than confusing.
void logf(const char* fmt, ...);

// Logs and flushes [FRONTIER] with method, object, slot, call count, and thread.
// Aborts unless explicitly configured to continue. slot is a primary-vtable byte offset.
void frontier(const char* qualifiedName, const void* self, unsigned int slot);

// Logs entry without stopping for methods with implemented empty or constant behavior.
void trace(const char* qualifiedName, const void* self, unsigned int slot);

bool frontierTrapEnabled();

// The first-chance crash reporter - see CrashReport.cpp. Installed from initDll; report-only, it
// never swallows an exception. Writes registers, the frame-pointer chain and an annotated stack
// window to the log, in the `module+0xRVA` form the .map and .pdb both resolve.
void installCrashReporter();
void removeCrashReporter();

// A pointer as a 32-bit integer for "0x%08X" log lines. On x86 that is the whole pointer. On x64 it
// is only the low 32 bits: a log-line tag, never converted back to a pointer. New log lines print
// the pointer itself with "0x%p", which reads identically on x86 and shows all 16 digits on x64.
unsigned int addr(const void* p);

} // namespace renddx9abi

// The two macros every vtable body in this image uses. `this` is captured here rather than passed,
// so a body cannot accidentally report someone else's object.
#define RD9_FRONTIER(qualifiedName, slot) \
    ::renddx9abi::frontier((qualifiedName), this, (slot))

#define RD9_TRACE(qualifiedName, slot) \
    ::renddx9abi::trace((qualifiedName), this, (slot))

// Free-function variants accept the owning object explicitly for diagnostic context.
#define RD9_FRONTIER_AT(qualifiedName, self, slot) \
    ::renddx9abi::frontier((qualifiedName), (self), (slot))

#define RD9_TRACE_AT(qualifiedName, self, slot) \
    ::renddx9abi::trace((qualifiedName), (self), (slot))

#endif // RENDDX9ABI_FRONTIER_H
