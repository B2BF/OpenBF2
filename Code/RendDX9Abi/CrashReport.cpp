// First-chance hard-fault diagnostics for the rebuilt DLL.
// [TEMP] [CHOSEN, not recovered] Logs registers and a module-relative stack window without allocation
// or symbol loading. Always continues exception search; ordinary C++ exceptions are excluded.
// [RETIRE-WHEN: RW-48] Retire with the associated frontier instrumentation.

#include "Frontier.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>

namespace renddx9abi {
namespace {

// Reports hard faults while excluding ordinary C++ exceptions, debugger naming, and control codes.
bool isHardFault(DWORD code)
{
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:            // includes the DEP execute case
    case EXCEPTION_DATATYPE_MISALIGNMENT:
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
    case EXCEPTION_IN_PAGE_ERROR:
    case EXCEPTION_STACK_OVERFLOW:
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case 0xC0000409u:                           // STATUS_STACK_BUFFER_OVERRUN / __fastfail
    case 0xC0000374u:                           // STATUS_HEAP_CORRUPTION
        return true;
    default:
        return false;
    }
}

// `module+0xRVA` for an address that lies in a mapped image, or an empty string. VirtualQuery is
// used rather than a module list walk because it cannot deadlock against the loader lock, which a
// first-chance handler may well be holding.

// `codeOnly` restricts the answer to executable pages. That is what makes the stack scan readable:
// a return address points at code, while the thousands of data pointers a frame holds do not.
const char* describe(const void* p, char* out, size_t cap, bool codeOnly)
{
    out[0] = '\0';

    MEMORY_BASIC_INFORMATION mbi;
    if (::VirtualQuery(p, &mbi, sizeof(mbi)) != sizeof(mbi))
        return out;
    if (mbi.State != MEM_COMMIT || mbi.Type != MEM_IMAGE || mbi.AllocationBase == 0)
        return out;

    if (codeOnly)
    {
        const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE
                         | PAGE_EXECUTE_WRITECOPY;
        if ((mbi.Protect & exec) == 0)
            return out;
    }

    char path[MAX_PATH];
    path[0] = '\0';
    const DWORD n = ::GetModuleFileNameA(static_cast<HMODULE>(mbi.AllocationBase),
                                         path, static_cast<DWORD>(sizeof(path)));
    if (n == 0 || n >= sizeof(path))
        return out;

    const char* name = path;
    for (const char* c = path; *c; ++c)
    {
        if (*c == '\\' || *c == '/')
            name = c + 1;
    }

    const unsigned int rva = static_cast<unsigned int>(
        reinterpret_cast<const unsigned char*>(p)
        - static_cast<const unsigned char*>(mbi.AllocationBase));
    ::_snprintf_s(out, cap, _TRUNCATE, "%s+0x%X", name, rva);
    return out;
}

#if defined(_M_IX86)
void reportStack(const CONTEXT& ctx)
{
    const unsigned char* const esp = reinterpret_cast<const unsigned char*>(ctx.Esp);

    MEMORY_BASIC_INFORMATION mbi;
    if (::VirtualQuery(esp, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT)
    {
        logf("[CRASH] stack at 0x%08X is not readable", static_cast<unsigned int>(ctx.Esp));
        return;
    }

    // The committed region ESP sits in, capped. 4096 dwords is 16 KB of stack, which is deeper than
    // any frame on this front and short enough that the report stays one screenful of hits.
    const unsigned char* const regionEnd =
        static_cast<const unsigned char*>(mbi.BaseAddress) + mbi.RegionSize;
    const unsigned char* end = esp + 4096 * sizeof(unsigned int);
    if (end > regionEnd)
        end = regionEnd;

    logf("[CRASH] stack scan from esp=0x%08X to 0x%08X - every word that points into executable "
         "image memory, in stack order (deepest frame first):",
         static_cast<unsigned int>(ctx.Esp), static_cast<unsigned int>(
             reinterpret_cast<UINT_PTR>(end)));

    char where[MAX_PATH + 32];
    int shown = 0;
    for (const unsigned char* p = esp; p + sizeof(unsigned int) <= end; p += sizeof(unsigned int))
    {
        const void* const value =
            reinterpret_cast<const void*>(*reinterpret_cast<const UINT_PTR*>(p));
        if (value == 0)
            continue;
        if (describe(value, where, sizeof(where), true)[0] == '\0')
            continue;

        logf("[CRASH]   [esp+0x%04X] 0x%08X  %s",
             static_cast<unsigned int>(p - esp), addr(value), where);

        if (++shown >= 96)
        {
            logf("[CRASH]   ... stopped after 96 hits");
            break;
        }
    }
    if (shown == 0)
        logf("[CRASH]   (no code pointers found - the stack window is data only)");
}

// The raw words either side of ESP, unfiltered. This is what tells a call through a null pointer
// (the return address is sitting at [esp]) apart from a RET to zero (the popped registers are just
// below esp) and from a frame the callee overwrote (a run of zeroes where the saved registers were).
// The code-pointer scan above cannot answer that, because it skips every word that is not code.
void reportRawWindow(const CONTEXT& ctx)
{
    const unsigned char* const esp = reinterpret_cast<const unsigned char*>(ctx.Esp);

    MEMORY_BASIC_INFORMATION mbi;
    if (::VirtualQuery(esp, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT)
        return;

    const unsigned char* const base = static_cast<const unsigned char*>(mbi.BaseAddress);
    const unsigned char* const top  = base + mbi.RegionSize;

    const unsigned char* from = esp - 0x40;
    if (from < base)
        from = base;
    const unsigned char* to = esp + 0x80;
    if (to > top)
        to = top;

    logf("[CRASH] raw stack around esp (esp itself is marked):");
    for (const unsigned char* p = from; p + 16 <= to; p += 16)
    {
        const unsigned int* const w = reinterpret_cast<const unsigned int*>(p);
        logf("[CRASH]   %c 0x%08X: %08X %08X %08X %08X",
             (p <= esp && esp < p + 16) ? '>' : ' ',
             addr(p), w[0], w[1], w[2], w[3]);
    }
}
#else
// [PLATFORM] The x64 arms of the two stack reports: the same windows over RSP, in 64-bit words and
// 16-digit addresses.
inline unsigned long long full(const void* p)
{
    return static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(p));
}

void reportStack(const CONTEXT& ctx)
{
    const unsigned char* const rsp = reinterpret_cast<const unsigned char*>(ctx.Rsp);

    MEMORY_BASIC_INFORMATION mbi;
    if (::VirtualQuery(rsp, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT)
    {
        logf("[CRASH] stack at 0x%016llX is not readable", static_cast<unsigned long long>(ctx.Rsp));
        return;
    }

    // The committed region RSP sits in, capped at 4096 qwords (32 KB).
    const unsigned char* const regionEnd =
        static_cast<const unsigned char*>(mbi.BaseAddress) + mbi.RegionSize;
    const unsigned char* end = rsp + 4096 * sizeof(UINT_PTR);
    if (end > regionEnd)
        end = regionEnd;

    logf("[CRASH] stack scan from rsp=0x%016llX to 0x%016llX - every qword that points into "
         "executable image memory, in stack order (deepest frame first):",
         static_cast<unsigned long long>(ctx.Rsp), full(end));

    char where[MAX_PATH + 32];
    int shown = 0;
    for (const unsigned char* p = rsp; p + sizeof(UINT_PTR) <= end; p += sizeof(UINT_PTR))
    {
        const void* const value =
            reinterpret_cast<const void*>(*reinterpret_cast<const UINT_PTR*>(p));
        if (value == 0)
            continue;
        if (describe(value, where, sizeof(where), true)[0] == '\0')
            continue;

        logf("[CRASH]   [rsp+0x%04X] 0x%016llX  %s",
             static_cast<unsigned int>(p - rsp), full(value), where);

        if (++shown >= 96)
        {
            logf("[CRASH]   ... stopped after 96 hits");
            break;
        }
    }
    if (shown == 0)
        logf("[CRASH]   (no code pointers found - the stack window is data only)");
}

// The raw qwords either side of RSP, unfiltered, two per line.
void reportRawWindow(const CONTEXT& ctx)
{
    const unsigned char* const rsp = reinterpret_cast<const unsigned char*>(ctx.Rsp);

    MEMORY_BASIC_INFORMATION mbi;
    if (::VirtualQuery(rsp, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT)
        return;

    const unsigned char* const base = static_cast<const unsigned char*>(mbi.BaseAddress);
    const unsigned char* const top  = base + mbi.RegionSize;

    const unsigned char* from = (rsp - base >= 0x40) ? rsp - 0x40 : base;
    const unsigned char* to = (top - rsp >= 0x80) ? rsp + 0x80 : top;

    logf("[CRASH] raw stack around rsp (rsp itself is marked):");
    for (const unsigned char* p = from; p + 16 <= to; p += 16)
    {
        const unsigned long long* const w = reinterpret_cast<const unsigned long long*>(p);
        logf("[CRASH]   %c 0x%016llX: %016llX %016llX",
             (p <= rsp && rsp < p + 16) ? '>' : ' ', full(p), w[0], w[1]);
    }
}

// One step of the x64 unwind: RtlVirtualUnwind over the image's unwind data, or the leaf rule (the
// return address at [rsp]) for a function that has none. Reads only the faulting thread's stack and
// the loaded images; false if a read faults.
bool unwindStep(CONTEXT& c)
{
    __try
    {
        DWORD64 imageBase = 0;
        PRUNTIME_FUNCTION function = ::RtlLookupFunctionEntry(c.Rip, &imageBase, 0);
        if (function)
        {
            void*   handlerData = 0;
            DWORD64 establisher = 0;
            ::RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, c.Rip, function, &c, &handlerData,
                               &establisher, 0);
        }
        else
        {
            c.Rip = *reinterpret_cast<const DWORD64*>(c.Rsp);
            c.Rsp += sizeof(DWORD64);
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

// The registers and the unwound call chain. x64 code keeps no frame-pointer chain, so the walk uses
// the images' unwind tables; it stops when RSP stops climbing or leaves committed memory.
void reportRegistersAndFrames(const CONTEXT& ctx)
{
    char where[MAX_PATH + 32];

    logf("[CRASH] rax=0x%016llX rbx=0x%016llX rcx=0x%016llX rdx=0x%016llX",
         static_cast<unsigned long long>(ctx.Rax), static_cast<unsigned long long>(ctx.Rbx),
         static_cast<unsigned long long>(ctx.Rcx), static_cast<unsigned long long>(ctx.Rdx));
    logf("[CRASH] rsi=0x%016llX rdi=0x%016llX rbp=0x%016llX rsp=0x%016llX",
         static_cast<unsigned long long>(ctx.Rsi), static_cast<unsigned long long>(ctx.Rdi),
         static_cast<unsigned long long>(ctx.Rbp), static_cast<unsigned long long>(ctx.Rsp));
    logf("[CRASH] r8 =0x%016llX r9 =0x%016llX r10=0x%016llX r11=0x%016llX",
         static_cast<unsigned long long>(ctx.R8), static_cast<unsigned long long>(ctx.R9),
         static_cast<unsigned long long>(ctx.R10), static_cast<unsigned long long>(ctx.R11));
    logf("[CRASH] r12=0x%016llX r13=0x%016llX r14=0x%016llX r15=0x%016llX",
         static_cast<unsigned long long>(ctx.R12), static_cast<unsigned long long>(ctx.R13),
         static_cast<unsigned long long>(ctx.R14), static_cast<unsigned long long>(ctx.R15));
    logf("[CRASH] rip=0x%016llX  %s", static_cast<unsigned long long>(ctx.Rip),
         describe(reinterpret_cast<const void*>(ctx.Rip), where, sizeof(where), false));

    CONTEXT c = ctx;
    for (int i = 0; i < 24; ++i)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (::VirtualQuery(reinterpret_cast<const void*>(c.Rsp), &mbi, sizeof(mbi)) != sizeof(mbi)
            || mbi.State != MEM_COMMIT)
            break;

        const DWORD64 previousRsp = c.Rsp;
        if (!unwindStep(c) || c.Rip == 0)
            break;

        logf("[CRASH] frame %2d: rsp=0x%016llX ret=0x%016llX  %s", i,
             static_cast<unsigned long long>(c.Rsp), static_cast<unsigned long long>(c.Rip),
             describe(reinterpret_cast<const void*>(c.Rip), where, sizeof(where), true));

        if (c.Rsp <= previousRsp)
            break;
    }
}
#endif

LONG CALLBACK vectoredHandler(EXCEPTION_POINTERS* info)
{
    // Report at most four times, and never re-enter. A fault inside this handler would otherwise
    // fault again on the same path and take the log with it.
    static LONG s_reports = 0;
    static LONG s_inside  = 0;

    if (!info || !info->ExceptionRecord || !info->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;

    const EXCEPTION_RECORD& rec = *info->ExceptionRecord;
    if (!isHardFault(rec.ExceptionCode))
        return EXCEPTION_CONTINUE_SEARCH;

    if (::InterlockedIncrement(&s_inside) != 1)
    {
        ::InterlockedDecrement(&s_inside);
        return EXCEPTION_CONTINUE_SEARCH;
    }
    if (::InterlockedIncrement(&s_reports) > 4)
    {
        ::InterlockedDecrement(&s_inside);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    const CONTEXT& ctx = *info->ContextRecord;
    char where[MAX_PATH + 32];

#if defined(_M_IX86)
    logf("[CRASH] ===============================================================================");
    logf("[CRASH] first-chance 0x%08X at 0x%08X  %s   (flags 0x%X, %u parameters)",
         static_cast<unsigned int>(rec.ExceptionCode), addr(rec.ExceptionAddress),
         describe(rec.ExceptionAddress, where, sizeof(where), false),
         static_cast<unsigned int>(rec.ExceptionFlags),
         static_cast<unsigned int>(rec.NumberParameters));

    for (DWORD i = 0; i < rec.NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
    {
        logf("[CRASH]   ExceptionInformation[%u] = 0x%08X", static_cast<unsigned int>(i),
             static_cast<unsigned int>(rec.ExceptionInformation[i]));
    }

    // Access-violation parameter zero distinguishes read (0), write (1), and execute (8) faults.
    if (rec.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec.NumberParameters >= 2)
    {
        const ULONG_PTR kind = rec.ExceptionInformation[0];
        logf("[CRASH]   -> %s at 0x%08X",
             kind == 0 ? "READ" : kind == 1 ? "WRITE" : kind == 8 ? "EXECUTE (DEP)" : "access",
             static_cast<unsigned int>(rec.ExceptionInformation[1]));
    }

    logf("[CRASH] eax=0x%08X ebx=0x%08X ecx=0x%08X edx=0x%08X",
         static_cast<unsigned int>(ctx.Eax), static_cast<unsigned int>(ctx.Ebx),
         static_cast<unsigned int>(ctx.Ecx), static_cast<unsigned int>(ctx.Edx));
    logf("[CRASH] esi=0x%08X edi=0x%08X ebp=0x%08X esp=0x%08X",
         static_cast<unsigned int>(ctx.Esi), static_cast<unsigned int>(ctx.Edi),
         static_cast<unsigned int>(ctx.Ebp), static_cast<unsigned int>(ctx.Esp));
    logf("[CRASH] eip=0x%08X  %s", static_cast<unsigned int>(ctx.Eip),
         describe(reinterpret_cast<const void*>(ctx.Eip), where, sizeof(where), false));

    // The frame-pointer chain, for the frames that keep one. It stops the moment a link stops
    // climbing, which is what a corrupted or omitted frame pointer looks like.
    const UINT_PTR* frame = reinterpret_cast<const UINT_PTR*>(ctx.Ebp);
    for (int i = 0; i < 24 && frame; ++i)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (::VirtualQuery(frame, &mbi, sizeof(mbi)) != sizeof(mbi) || mbi.State != MEM_COMMIT)
            break;

        const UINT_PTR next = frame[0];
        const UINT_PTR ret  = frame[1];
        if (ret == 0)
            break;

        logf("[CRASH] frame %2d: ebp=0x%08X ret=0x%08X  %s", i,
             addr(frame), static_cast<unsigned int>(ret),
             describe(reinterpret_cast<const void*>(ret), where, sizeof(where), true));

        if (next <= reinterpret_cast<UINT_PTR>(frame))
            break;
        frame = reinterpret_cast<const UINT_PTR*>(next);
    }
#else
    logf("[CRASH] ===============================================================================");
    logf("[CRASH] first-chance 0x%08X at 0x%016llX  %s   (flags 0x%X, %u parameters)",
         static_cast<unsigned int>(rec.ExceptionCode), full(rec.ExceptionAddress),
         describe(rec.ExceptionAddress, where, sizeof(where), false),
         static_cast<unsigned int>(rec.ExceptionFlags),
         static_cast<unsigned int>(rec.NumberParameters));

    for (DWORD i = 0; i < rec.NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
    {
        logf("[CRASH]   ExceptionInformation[%u] = 0x%016llX", static_cast<unsigned int>(i),
             static_cast<unsigned long long>(rec.ExceptionInformation[i]));
    }

    // Access-violation parameter zero distinguishes read (0), write (1), and execute (8) faults.
    if (rec.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && rec.NumberParameters >= 2)
    {
        const ULONG_PTR kind = rec.ExceptionInformation[0];
        logf("[CRASH]   -> %s at 0x%016llX",
             kind == 0 ? "READ" : kind == 1 ? "WRITE" : kind == 8 ? "EXECUTE (DEP)" : "access",
             static_cast<unsigned long long>(rec.ExceptionInformation[1]));
    }

    reportRegistersAndFrames(ctx);
#endif

    reportRawWindow(ctx);
    reportStack(ctx);
    logf("[CRASH] ===============================================================================");

    ::InterlockedDecrement(&s_inside);

    // always. This instrument observes; it never changes what the process does next.
    return EXCEPTION_CONTINUE_SEARCH;
}

void* s_handle = 0;

} // anonymous namespace

void installCrashReporter()
{
    if (s_handle)
        return;

    // First in the chain, so the report is written before anything else can unwind the stack out
    // from under it.
    s_handle = ::AddVectoredExceptionHandler(1, &vectoredHandler);
    logf("crash reporter: %s", s_handle ? "installed (first-chance, report-only)"
                                        : "AddVectoredExceptionHandler FAILED");
}

void removeCrashReporter()
{
    if (!s_handle)
        return;
    ::RemoveVectoredExceptionHandler(s_handle);
    s_handle = 0;
}

} // namespace renddx9abi
