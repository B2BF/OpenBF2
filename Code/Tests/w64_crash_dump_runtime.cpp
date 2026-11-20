// [TEMP] [CHOSEN, not recovered] Real System.cpp/StringUtil.cpp plus the extracted
// BF2Log::addLogEntry body. Only its owning class/File lifecycle are bounded test models.
// This writes dumps of this fixture process using real Win32 threads and DbgHelp.
// The last case runs the extracted crashDumpFileName and bf2WinMain bodies from BF2.cpp:
// a real access violation reaches the SEH filter, which writes the dump through the real
// crashDumpTargetName/writeCrashDump into a retail-shaped "<base>\dmp\" directory.
#include <windows.h>
#include <dbghelp.h>
#include "Core/System.h"
#include "Core/SystemInfo.h"
#include "Core/StringUtil.h"
#include "Core/Debug.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* label) {
    ++checks;
    if (!ok) { ++failures; std::printf("FAIL %s (%lu)\n", label, GetLastError()); }
}
}

namespace dice { namespace hfe {
// Fixed clock for crashDumpTargetName; no debugger, so bf2WinMain installs its SEH frame.
class CrashFixtureSystemInfo : public ISystemInfo {
public:
    const std::string& getComputerName() const override { return name; }
    std::string getDateTimeString() const override { return "2026-11-20 12:34:56"; }
    unsigned int getNumberOfPageFaults() const override { return 0; }
    unsigned int getMemoryUsage() const override { return 0; }
    void getMemoryInfo(unsigned int& total, unsigned int& available) const override
    { total = available = 0; }
    bool getIsDebuggerInstalled() const override { return false; }
private:
    std::string name;
};
ISystemInfo::ISystemInfo() {}
ISystemInfo::~ISystemInfo() {}
ISystemInfo* systemInfo() { static CrashFixtureSystemInfo info; return &info; }
System* g_system;
namespace BuildNrUtil { const char* getBuildNrString() { return "1.5.3153-802.0"; } }

// Desktop and engine collaborators of the extracted bf2WinMain body.
static STICKYKEYS s_startupStickyKeys;
static TOGGLEKEYS s_startupToggleKeys;
static FILTERKEYS s_startupFilterKeys;
static UINT s_startupMouseTrails;
static BOOL s_screenSaverActive;
static int entryRuns, entryRestores;
static DWORD entryThread;
static std::wstring entryDumpDirectory;
static BOOL WINAPI fixtureSystemParametersInfo(UINT, UINT, PVOID, UINT) { return TRUE; }
static void allowAccessibilityShortcutKeys(bool) {}
static void restoreDesktopSettings() { ++entryRestores; }
const wchar_t* getCrashDumpDirectory() { return entryDumpDirectory.c_str(); }
static int __stdcall bf2Main(HINSTANCE, HINSTANCE, LPSTR)
{
    ++entryRuns;
    entryThread = GetCurrentThreadId();
    volatile int* volatile target = nullptr;
    *target = 1;                        // the RendDX9 case: a real access violation
    return 17;
}
#define SystemParametersInfoA fixtureSystemParametersInfo
#include "crash_startup_body.inc"
#undef SystemParametersInfoA
namespace io {
struct CrashFixtureFile {
    bool open = false;
    unsigned probes = 0;
    std::vector<std::string> writes;
    bool isOpen() { ++probes; return open; }
    bool write(const std::string& entry) { writes.push_back(entry); return false; }
};
}
class BF2Log {
public:
    io::CrashFixtureFile* m_logFile = nullptr;
    void addLogEntry(DebugType, const std::string&);
};
#include "crash_log_body.inc"
} }

void verifyDump(const std::wstring& name, EXCEPTION_POINTERS& exception,
                DWORD threadId, const std::string& history)
{
    dice::hfe::writeCrashDump(&exception, name.c_str());
    HANDLE file = CreateFileW(name.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    check(file != INVALID_HANDLE_VALUE, "worker joined and closed file before return");
    if (file == INVALID_HANDLE_VALUE) return;
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    void* base = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : nullptr;
    check(base != nullptr, "dump has readable content");
    if (base) {
        const MINIDUMP_HEADER& header = *static_cast<const MINIDUMP_HEADER*>(base);
        check(header.Signature == MINIDUMP_SIGNATURE, "real minidump signature");
        // The host DbgHelp adds flags (currently 0x200000); validate the requested bit.
        check((header.Flags & MiniDumpWithProcessThreadData) != 0, "retail 0x100 flag present");
        PMINIDUMP_DIRECTORY directory = nullptr;
        void* data = nullptr;
        ULONG size = 0;
        bool read = MiniDumpReadDumpStream(base, ExceptionStream, &directory, &data, &size) != FALSE;
        check(read && size >= sizeof(MINIDUMP_EXCEPTION_STREAM), "exception stream present");
        if (read && size >= sizeof(MINIDUMP_EXCEPTION_STREAM)) {
            const auto& stream = *static_cast<const MINIDUMP_EXCEPTION_STREAM*>(data);
            check(stream.ThreadId == threadId, "exception belongs to caller not worker");
            check(stream.ExceptionRecord.ExceptionAddress ==
                reinterpret_cast<uintptr_t>(exception.ExceptionRecord->ExceptionAddress),
                "exception address preserves native pointer width");
            check(stream.ExceptionRecord.ExceptionCode == 0xE0424242u,
                  "exception code preserved");
        }
        read = MiniDumpReadDumpStream(base, SystemInfoStream, &directory, &data, &size) != FALSE;
        check(read && size >= sizeof(MINIDUMP_SYSTEM_INFO), "system info stream present");
        if (read && size >= sizeof(MINIDUMP_SYSTEM_INFO)) {
            const auto& info = *static_cast<const MINIDUMP_SYSTEM_INFO*>(data);
            check(info.ProcessorArchitecture == (sizeof(void*) == 8 ?
                  PROCESSOR_ARCHITECTURE_AMD64 : PROCESSOR_ARCHITECTURE_INTEL),
                  "dump architecture matches fixture");
        }
        read = MiniDumpReadDumpStream(base, CommentStreamA, &directory, &data, &size) != FALSE;
        char address[32];
        std::sprintf(address, "%p", exception.ExceptionRecord->ExceptionAddress);
        const std::string expected = std::string("Battlefield2 MiniDump. Address: ") + address +
            " (In Windbg type: .ecxr)\nASSERT INFORMATION:\n[...]\n" + history + "[EOF]";
        check(read && size == expected.size() + 1, "comment includes exact byte count and terminator");
        if (read && size == expected.size() + 1)
            check(std::memcmp(data, expected.c_str(), size) == 0, "comment matches retail text and history order");
        UnmapViewOfFile(base);
    }
    if (mapping) CloseHandle(mapping);
    CloseHandle(file);
}

// The client/dedicated path: getCrashDumpDirectory() ends in "\\dmp\\" as retail's does.
void verifyEntryDump(const std::wstring& base)
{
    using namespace dice::hfe;
    g_system = new System();
    entryDumpDirectory = base + L"\\dmp\\";
    CreateDirectoryW((base + L"\\dmp").c_str(), nullptr);
    char commandLine[] = "+fixture";
    check(bf2WinMain(nullptr, nullptr, commandLine) == 42, "access violation maps to exit 42");
    check(entryRuns == 1 && entryRestores == 1, "engine ran once and desktop was restored");
    const std::wstring name = entryDumpDirectory + L"bf2_v1.5.3153-802.0_2026-11-20_12.34.56.dmp";
    HANDLE file = CreateFileW(name.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    check(file != INVALID_HANDLE_VALUE, "SEH filter wrote <dmp>\\bf2_v<build>_<time>.dmp");
    if (file == INVALID_HANDLE_VALUE) return;
    LARGE_INTEGER size = {};
    GetFileSizeEx(file, &size);
    std::printf("Entry dump: %ls (%lld bytes)\n", name.c_str(), size.QuadPart);
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    void* view = mapping ? MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0) : nullptr;
    check(view != nullptr, "entry dump has readable content");
    if (view) {
        check(static_cast<const MINIDUMP_HEADER*>(view)->Signature == MINIDUMP_SIGNATURE,
              "entry dump signature");
        PMINIDUMP_DIRECTORY directory = nullptr;
        void* data = nullptr;
        ULONG streamSize = 0;
        const bool read = MiniDumpReadDumpStream(view, ExceptionStream, &directory, &data,
                                                 &streamSize) != FALSE;
        check(read && streamSize >= sizeof(MINIDUMP_EXCEPTION_STREAM), "entry exception stream");
        if (read && streamSize >= sizeof(MINIDUMP_EXCEPTION_STREAM)) {
            const auto& stream = *static_cast<const MINIDUMP_EXCEPTION_STREAM*>(data);
            check(stream.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION,
                  "entry dump records the access violation");
            check(stream.ThreadId == entryThread, "entry dump names the faulting thread");
        }
        UnmapViewOfFile(view);
    }
    if (mapping) CloseHandle(mapping);
    CloseHandle(file);
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 2) return 2;
    using namespace dice::hfe;
    const std::wstring base = argv[1];
    CONTEXT context = {};
    RtlCaptureContext(&context);
    EXCEPTION_RECORD record = {};
    record.ExceptionCode = 0xE0424242u;
    record.ExceptionAddress = &record; // x64 stack address must survive above 4 GiB
    EXCEPTION_POINTERS exception = { &record, &context };
    const DWORD threadId = GetCurrentThreadId();
    if (sizeof(void*) == 8)
        check(reinterpret_cast<uintptr_t>(&record) > 0xFFFFFFFFu, "x64 fixture uses a high address");

    // Failed CreateFile must return without dereferencing a null exception. A later
    // successful write exercises the initialized lock left behind by that branch.
    const std::wstring bad = base + L"\\missing-directory\\dump.dmp";
    writeCrashDump(nullptr, bad.c_str());
    check(GetFileAttributesW(bad.c_str()) == INVALID_FILE_ATTRIBUTES, "failed create returns without output");
    verifyDump(base + L"\\empty.dmp", exception, threadId, "");

    BF2Log log;
    for (unsigned type = 0; type != 9; ++type)
        log.addLogEntry(static_cast<DebugType>(type), "type" + std::to_string(type) + "\n");
    log.addLogEntry(DebugTypeAssert, "four\n");
    log.addLogEntry(DebugTypeError, "five\n");
    io::CrashFixtureFile file;
    log.m_logFile = &file;
    log.addLogEntry(DebugTypeInfo, "closed");
    check(file.probes == 1 && file.writes.empty(), "closed file is probed without writing");
    file.open = true;
    log.addLogEntry(DebugTypeWarning, "open");
    check(file.probes == 2 && file.writes == std::vector<std::string>({"open"}),
          "open file gets original bytes and false write is ignored");
    const std::wstring history = base + L"\\history.dmp";
    verifyDump(history, exception, threadId, "type5\ntype6\nfour\nfive\n");
    verifyDump(history, exception, threadId, "five\nfour\ntype6\ntype5\n");
    log.addLogEntry(DebugTypeDxAssert, "six\n");
    verifyDump(base + L"\\after.dmp", exception, threadId, "type6\nfour\nfive\nsix\n");
    verifyEntryDump(base);
    std::printf("Crash dump runtime (%u-bit): %d checks, %d failures\n",
                static_cast<unsigned>(sizeof(void*) * 8), checks, failures);
    return failures ? 1 : 0;
}
