// Application entry points and engine-facade lifecycle. [INFERRED] source placement.

#include "BF2.h"

#include "BF2Engine.h"                              // [INFERRED] path: Game/Main/BF2Engine.cpp is
                                                    // attested, so a matching header is assumed.
#include "../../Core/System.h"                      // [INFERRED] Core/System.cpp attested (anon-ns)
#include "../../IO/SettingsManager.h"               // [INFERRED] IO/SettingsManager.cpp attested
#include "../../Core/SettingsRepostitory.h"

// [UNRESOLVED] memory_update_modules takes const char*, but this call omits it.
// The declaration's default preserves the current call until argument evidence resolves it.
namespace dice { namespace hfe {
namespace memory { void memory_update_modules(const char* = 0); }
} }

// Release-build memory instrumentation hook; intentionally has no effect.
namespace dice { namespace hfe {
namespace memory { void memory_update_modules(const char*) { } }
} }

#include <sstream>
#include <iostream>
#include <cstdio>                                    // setvbuf/stdout - unbuffered server logging

#if defined(_WIN32)
#  include <windows.h>
#  include <cstring>                                 // strstr, memset - the Win32 entry chain
#  include <cstdlib>                                 // exit(0) on WM_QUIT
#  include "../../Core/SystemInfo.h"                 // systemInfo()->getIsDebuggerInstalled()
#  include "../../Core/StringUtil.h"                 // toWideString - the crash-dump file name
#  include "../../IO/Input/Input.h"                  // IInputManager::setHasFocus (both PEs)
#  include "../Utility/BuildNrUtil.h"                // getBuildNrString - the crash-dump file name
#  include "../Utility/Vars.h"                       // Vars::enableProfiling around the main loop
#  include "BF2Window.h"                             // the game window at BF2Engine+0x10
#  if !DEDICATED
#    include <dbghelp.h>                             // EnumerateLoadedModules (client only)
#    include <psapi.h>                               // EnumProcesses / EmptyWorkingSet (client only)
#    include "../../Sound/BfSound.h"                 // g_bfSound - the focus-loss mute
#    include "../../Sound/NullSound.h"               // ISoundEngine::setMasterVolume
#    include "../../Menu/IMenuLogic.h"               // menuLogic - the active menu
#    include "BF2Console.h"                          // IMenuLogic::getConsole()->getConsoleActive()
#  endif
#endif

// Rebuilds the single command-line string the engine expects. The C runtime has already stripped the
// shell's quotes from argv, so an argument containing a space must be quoted again before the
// arguments are rejoined. Without this, a path such as
// "D:\Battlefield 2\mods\fh2\fh2settings\beta_maplist.con" splits at its space and the parser stores
// only "D:\Battlefield". WinMain receives lpCmdLine with those quotes intact, which is why the retail
// server accepts such paths; the re-quoting restores that input contract for the console entry point.
static void appendCommandLineArg(std::string& out, const char* arg, bool first)
{
    if (!first)
        out += ' ';
    const std::string value(arg ? arg : "");
    if (value.empty() || value.find_first_of(" \t") != std::string::npos)
    {
        out += '"';
        out += value;
        out += '"';
    }
    else
    {
        out += value;
    }
}

namespace dice {
namespace hfe {

// correction: an earlier version of this block claimed "globals live in .bss and are unnamed in both
// databases, so these names are chosen, not recovered". That was false for all three. Each resolves
// to a real ELF symbol, so none carries a marker:
extern System*              g_system;               // _ZN4dice3hfe8g_systemE  @ 0x8AC0D20
                                                    // (there is NO _ZN4dice3hfe6systemE)
extern BF2Engine*           bf2Engine;              // _ZN4dice3hfe9bf2EngineE @ 0x8AACBD8
extern SettingsRepostitory* settings;               // _ZN4dice3hfe8settingsE  @ 0x8ABBCBC
                                                    // the misspelling is dice's, it is in the
                                                    // mangled name (SettingsRepostitory)

#if defined(_WIN32)
class IBase;
namespace io { extern IBase* inputManager; }        // ds:0x8AC8CA4 - Core/Globals.cpp defines it as IBase*

// The Win32 entry chain lives at the bottom of this file (both PEs' BF2.cpp); these are the pieces
// of it that BF2::init and BF2::handleInGameWinMessages call.
static void allowAccessibilityShortcutKeys(bool allow);
static void restoreDesktopSettings();
#  if !DEDICATED
static BOOL CALLBACK lockLoadedModule(PCSTR moduleName, ULONG moduleBase, ULONG moduleSize, PVOID userContext);
static void trimOtherProcessesAndFileCache();
static int  bf2MessageHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
#  endif
#endif

BF2::BF2()
    : m_engine(0)
{
}

// Shuts down the engine. [PLATFORM] Win32 deletes this facade on normal exit;
// the original Linux main returns without deleting it.
BF2::~BF2()
{
    shutdown();
}

bool BF2::init(const std::string& commandLine)
{
    m_engine = new BF2Engine(this);                 // operator new(216) - sizeof(BF2Engine) == 216
    if (!m_engine)
        return false;

    bf2Engine = m_engine;

    if (!m_engine->init(commandLine))
        return false;

#if defined(_WIN32) && !DEDICATED
    // Client only (BF2.exe 0x401E9A; the dedicated PE imports neither EnumerateLoadedModules nor
    // EnumProcesses): lock every loaded module's pages, then empty every other process's working
    // set and flush the system file cache. Unconditional: the `lockMemory 0` option does not gate this.
    EnumerateLoadedModules(GetCurrentProcess(), lockLoadedModule, 0);
    trimOtherProcessesAndFileCache();
#endif
    memory::memory_update_modules();

    // Any of these launch options starts the game without going through the menu.
    const bool hasLevel  = !settings->stringGet("GSLoadLevel",   std::string()).empty();
    const bool hasJoin   = !settings->stringGet("GSJoinAddress", std::string()).empty();
    const bool playNow   =  settings->intGet("playNow", 0) == 1;

    m_engine->getMenuActive();                      // [UNRESOLVED] result discarded in both builds.
                                                    // Likely a side effect, or a dropped condition
                                                    // left over from the client build.
                                                    // (Was written as a static call; it is an
                                                    // instance call and getMenuActive is non-static.)

    bool startImmediately = false;
    // boolGet is NOT an out-parameter API: the attested signature is
    // _ZN4dice3hfe19SettingsRepostitory7boolGetERKSsRKb (vptr+0x50), taking
    // (const std::string&, const bool&) and returning a reference.
    if (settings->boolGet("GSDedicated", false) || hasLevel || hasJoin || playNow)
        startImmediately = true;

    if (startImmediately)
        m_engine->startGame(false, true);           // TWO bools: 0x8052979 xor eax,eax -> [esp+4];
                                                    // mov edx,1 -> [esp+8]; call at 0x805298B

#if defined(_WIN32) && !DEDICATED
    // BF2.exe 0x401FE5: hand the window BF2's message handler (slot +0x30). The window exists only
    // when BF2Engine::init ran with isDedicated() false; `+dedicated 1` leaves it null.
    if (Window* window = m_engine->getWindow())
        window->addMessageHandler(bf2MessageHandler);
#endif
    return true;
}

void BF2::shutdown()
{
    m_engine->shutdown();
    //  is `shutdown(); if (p) call vt[+4](p)` - vtable slot +4 is the Itanium deleting
    // destructor, not a release() method. There is no BF2Engine::release symbol in the binary.
    if (m_engine)
        delete m_engine;
}

bool BF2::run()
{
    handleInGameWinMessages();
    return m_engine->mainLoop();
}

void BF2::startDemo(const std::string& demoName)
{
    if (m_engine)
        m_engine->playDemo(true, demoName);
}

// [PLATFORM] Win32 pumps messages, handles quit and focus, and updates client input/audio focus. Linux is empty.
void BF2::handleInGameWinMessages()
{
#if defined(_WIN32)
    MSG msg;
    while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE | PM_NOYIELD))            // 3
    {
        if (msg.message == WM_QUIT)                                          // 18
        {
            restoreDesktopSettings();               // the same five calls bf2WinMain makes on exit
            exit(0);                                // no engine shutdown at all, as shipped
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    unsigned int deviceMask = static_cast<unsigned int>(-1);
#  if !DEDICATED
    if (menuLogic)
        deviceMask = static_cast<unsigned int>(-2);                          // every device but bit 0 while a menu is up
#  endif
    if (m_engine)
    {
        if (Window* window = m_engine->getWindow())                          // +0x10 - null in a dedicated image
        {
            const bool hasFocus = window->getHasFocus();                     // slot +0x08
            if (io::inputManager)
                static_cast<io::IInputManager*>(io::inputManager)->setHasFocus(hasFocus, deviceMask);   // slot +0x18
#  if !DEDICATED
            // Mute while the window has no focus, unless GSDeveloper. engine() is asked twice, as
            // shipped (once in the condition, once for the call).
            if (!settings->boolGet("GSDeveloper", false) && g_bfSound && g_bfSound->engine())
            {
                ISoundEngine* engine = g_bfSound->engine();                  // IBfSound +0x24 in the PE
                engine->setMasterVolume(hasFocus ? 1.0f : 0.0f);             // ISoundEngine +0x1C
            }
#  endif
        }
    }
#endif
}

} // namespace hfe
} // namespace dice


#if defined(_WIN32)

// Windows GUI entry and desktop-state handling shared by both images. The dedicated
// build uses the [CHOSEN] console main below; client startup uses this WinMain chain.

namespace dice {
namespace hfe {

// the desktop state saved around the game
// .data in both PEs and initialised there: BF2.exe 0x96E264 / 0x96E26C / 0x96E274 hold 8, 8 and
// 0x18 - the cbSize members - so the source carried the initialisers below. Mouse trails at 0x986F10
// (IDA's `uiParam`), the screensaver flag at 0x986F14. [INFERRED] names.
namespace {
    STICKYKEYS s_startupStickyKeys  = { sizeof(STICKYKEYS), 0 };
    TOGGLEKEYS s_startupToggleKeys  = { sizeof(TOGGLEKEYS), 0 };
    FILTERKEYS s_startupFilterKeys  = { sizeof(FILTERKEYS), 0, 0, 0, 0, 0 };
    UINT       s_startupMouseTrails = 0;
    int        s_screenSaverActive  = 0;
}

//  (client) / 0x401010 (dedicated), 248 B in both. The DirectX SDK's
// AllowAccessibilityShortcutKeys: `true` restores the saved structures and the mouse-trail count;
// `false` clears the hot-key and confirmation flags of each feature that is not currently ON (the
// `and byte, 0F3h` at each site) and turns mouse trails off if more than one was set. [INFERRED] name.
static void allowAccessibilityShortcutKeys(bool allow)
{
    if (allow)
    {
        SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s_startupStickyKeys, 0);   // 0x3B
        SystemParametersInfoA(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &s_startupToggleKeys, 0);   // 0x35
        SystemParametersInfoA(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &s_startupFilterKeys, 0);   // 0x33
        SystemParametersInfoA(SPI_SETMOUSETRAILS, s_startupMouseTrails, 0, 0);                  // 0x5D
        return;
    }

    STICKYKEYS stickyKeys = s_startupStickyKeys;
    if ((stickyKeys.dwFlags & SKF_STICKYKEYSON) == 0)
    {
        stickyKeys.dwFlags &= ~(SKF_HOTKEYACTIVE | SKF_CONFIRMHOTKEY);
        SystemParametersInfoA(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stickyKeys, 0);
    }
    TOGGLEKEYS toggleKeys = s_startupToggleKeys;
    if ((toggleKeys.dwFlags & TKF_TOGGLEKEYSON) == 0)
    {
        toggleKeys.dwFlags &= ~(TKF_HOTKEYACTIVE | TKF_CONFIRMHOTKEY);
        SystemParametersInfoA(SPI_SETTOGGLEKEYS, sizeof(TOGGLEKEYS), &toggleKeys, 0);
    }
    FILTERKEYS filterKeys = s_startupFilterKeys;
    if ((filterKeys.dwFlags & FKF_FILTERKEYSON) == 0)
    {
        filterKeys.dwFlags &= ~(FKF_HOTKEYACTIVE | FKF_CONFIRMHOTKEY);
        SystemParametersInfoA(SPI_SETFILTERKEYS, sizeof(FILTERKEYS), &filterKeys, 0);
    }
    if (s_startupMouseTrails > 1)
        SystemParametersInfoA(SPI_SETMOUSETRAILS, 0, 0, 0);
}

// The five restore calls both PEs repeat inline at both exits (the end of bf2WinMain and WM_QUIT):
// the screensaver back on if it was on, then the four SPI_SET calls above. [INFERRED] that it was a
// helper - the PEs show the sequence expanded at every site.
static void restoreDesktopSettings()
{
    if (s_screenSaverActive)
        SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, 1, 0, SPIF_SENDCHANGE);                 // 0x11
    allowAccessibilityShortcutKeys(true);
}

#if !DEDICATED
// the memory-locking trio of BF2::init (client only)

// . The IDB named it after its use: every loaded module's pages get locked.
static BOOL CALLBACK lockLoadedModule(PCSTR /*moduleName*/, ULONG moduleBase, ULONG moduleSize, PVOID /*userContext*/)
{
    VirtualLock(reinterpret_cast<LPVOID>(moduleBase), moduleSize);
    return TRUE;
}

// . The SDK's SetPrivilege shape: look the luid up, adjust with the attribute
// cleared to learn the previous state, then adjust the previous state with the attribute set or
// cleared. [INFERRED] name.
static BOOL enablePrivilege(HANDLE token, LPCSTR privilegeName, bool enable)
{
    TOKEN_PRIVILEGES newState;
    TOKEN_PRIVILEGES previousState;
    LUID  luid;
    DWORD returnLength = sizeof(TOKEN_PRIVILEGES);
    if (!LookupPrivilegeValueA(0, privilegeName, &luid))
        return FALSE;
    newState.PrivilegeCount           = 1;
    newState.Privileges[0].Luid       = luid;
    newState.Privileges[0].Attributes = 0;
    if (!AdjustTokenPrivileges(token, FALSE, &newState, sizeof(TOKEN_PRIVILEGES), &previousState, &returnLength))
        return FALSE;
    previousState.PrivilegeCount           = 1;
    previousState.Privileges[0].Luid       = luid;
    previousState.Privileges[0].Attributes = enable ? SE_PRIVILEGE_ENABLED : 0;
    return AdjustTokenPrivileges(token, FALSE, &previousState, returnLength, 0, 0);
}

typedef LONG (WINAPI* NtQuerySystemInformationFn)(ULONG, PVOID, ULONG, PULONG);
typedef LONG (WINAPI* NtSetSystemInformationFn)(ULONG, PVOID, ULONG);

// . NtQuerySystemInformation / NtSetSystemInformation(SystemFileCacheInformation =
// 21) with both working-set bounds -1 - the "flush the system file cache" trick, which needs
// SeIncreaseQuotaPrivilege. Two shipped quirks kept: the token is not closed on the successful
// path, and the failure text goes to stdout, which a GUI image has no console for. [INFERRED] name.
static void flushSystemFileCache()
{
    static NtSetSystemInformationFn   s_ntSetSystemInformation;
    static NtQuerySystemInformationFn s_ntQuerySystemInformation;

    s_ntSetSystemInformation = reinterpret_cast<NtSetSystemInformationFn>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtSetSystemInformation"));
    if (!s_ntSetSystemInformation)
        return;
    s_ntQuerySystemInformation = reinterpret_cast<NtQuerySystemInformationFn>(
        GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQuerySystemInformation"));
    if (!s_ntQuerySystemInformation)
        return;

    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))  // 0x28
    {
        printf("You do not have the necessary privilege to run this program\n");
        return;
    }
    if (!enablePrivilege(token, "SeIncreaseQuotaPrivilege", true))
    {
        CloseHandle(token);
        return;
    }

    ULONG fileCacheInformation[9];                  // SYSTEM_FILECACHE_INFORMATION, 36 bytes
    ULONG returnLength;
    if (s_ntQuerySystemInformation(21, fileCacheInformation, sizeof(fileCacheInformation), &returnLength) == 0)
    {
        fileCacheInformation[3] = static_cast<ULONG>(-1);     // MinimumWorkingSet
        fileCacheInformation[4] = static_cast<ULONG>(-1);     // MaximumWorkingSet
        s_ntSetSystemInformation(21, fileCacheInformation, sizeof(fileCacheInformation));
    }
}

// . Every other process gets its working set emptied, then the file cache goes.
static void trimOtherProcessesAndFileCache()
{
    DWORD processIds[1024];
    DWORD bytesReturned;
    if (!EnumProcesses(processIds, sizeof(processIds), &bytesReturned))
        return;
    const unsigned int count = bytesReturned / sizeof(DWORD);
    for (unsigned int i = 0; i < count; ++i)
    {
        const DWORD processId = processIds[i];
        if (processId == GetCurrentProcessId())
            continue;
        HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA | PROCESS_VM_READ, FALSE, processId);   // 0x510
        if (process)
        {
            EmptyWorkingSet(process);
            CloseHandle(process);
        }
    }
    flushSystemFileCache();
}

// the two key rings (declared in BF2.h)
// The console ring's four definitions moved to BF2Console.cpp - ONE ring for both
// images, drained there by BF2Console::updateConsole; pushConsoleChar below still fills it.
char menuKeyBuffer[256];
int  menuKeyCount     = 0;
int  menuKeyRead      = 0;
int  menuKeyWrite     = 0;

//  and 0x401040, 61 B each - the first two functions of BF2.exe's text. The write index
// wraps at 256; the count also wraps to 0 at 256 instead of saturating, as shipped. [INFERRED] names.
static int pushConsoleChar(char c)
{
    int index = consoleCharWrite;
    if (consoleCharWrite > 255)
        index = 0;
    consoleCharBuffer[index] = c;
    consoleCharWrite = index + 1;
    if (++consoleCharCount > 255)
        consoleCharCount = 0;
    return 0;
}

static int pushMenuKey(char key)
{
    int index = menuKeyWrite;
    if (menuKeyWrite > 255)
        index = 0;
    menuKeyBuffer[index] = key;
    menuKeyWrite = index + 1;
    if (++menuKeyCount > 255)
        menuKeyCount = 0;
    return 0;
}

// The Caps Lock trick the handler uses twice: while the LED is on, press and release VK_CAPITAL
// through SendInput so the game never runs with Caps Lock engaged. Both INPUTs are zeroed, both
// are VK_CAPITAL, the second carries KEYEVENTF_KEYUP (the `mov [ebp+var_20], 2`).
static void toggleCapsLockOff()
{
    INPUT inputs[2];
    memset(inputs, 0, sizeof(inputs));
    inputs[0].type       = INPUT_KEYBOARD;
    inputs[0].ki.wVk     = VK_CAPITAL;
    inputs[1].type       = INPUT_KEYBOARD;
    inputs[1].ki.wVk     = VK_CAPITAL;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, inputs, sizeof(INPUT));
}

// 0x2D0 B. Added to the window by BF2::init and called by Window::wndProc for every
// message; the return value is ignored there. Every arm re-reads "GSDedicated" from the settings
// repository: it is the same handler in the client's own `+dedicated 1` mode, although in that mode
// BF2Engine::init never creates the window, so the handler is never installed. [INFERRED] name.
static int bf2MessageHandler(HWND /*hwnd*/, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message > WM_LBUTTONDOWN)                   // 0x201
    {
        if (message == WM_LBUTTONUP)                // 0x202
        {
            if (settings && !settings->boolGet("GSDedicated", false) && menuLogic)
            {
                menuLogic->onMouseButtonUp();       // +0x54
                return 0;
            }
        }
        else if (message == WM_LBUTTONDBLCLK)       // 0x203
        {
            if (settings && !settings->boolGet("GSDedicated", false) && menuLogic)
            {
                menuLogic->onMouseDoubleClick();    // +0x58
                return 0;
            }
        }
        return 0;
    }
    if (message == WM_LBUTTONDOWN)                  // 0x201
    {
        if (settings && !settings->boolGet("GSDedicated", false) && menuLogic)
            menuLogic->onMouseButtonDown();         // +0x50
        return 0;
    }
    if (message == WM_MOUSEMOVE)                    // 0x200
    {
        if (settings && !settings->boolGet("GSDedicated", false) && menuLogic)
            menuLogic->onMouseMove(LOWORD(lParam), HIWORD(lParam));   // +0x4C: movzx ax / shr 16
        return 0;
    }

    switch (message)
    {
    case WM_ACTIVATEAPP:                            // 0x1C
        if (wParam && (GetKeyState(VK_CAPITAL) & 1))
            toggleCapsLockOff();
        return 0;

    case WM_INPUTLANGCHANGEREQUEST:                 // 0x50
        return 1;                                   // refused - and ignored by Window::wndProc anyway

    case WM_KEYDOWN:                                // 0x100
        if (!menuLogic && wParam == VK_CAPITAL && (GetKeyState(VK_CAPITAL) & 1))
            toggleCapsLockOff();                    // in-game, Caps Lock is switched straight back off
        if (!settings)
            return 0;
        if (settings->boolGet("GSDedicated", false))
            return 0;
        if (!menuLogic || wParam != VK_DELETE)      // 46
            return 0;
        pushMenuKey(127);                           // DEL reaches the menu as 127
        return 0;

    case WM_CHAR:                                   // 0x102
        if (!settings)
            return 0;
        if (settings->boolGet("GSDedicated", false))
            return 0;
        pushConsoleChar(static_cast<char>(wParam)); // every character, console open or not
        if (!menuLogic || (menuLogic->getConsole() && menuLogic->getConsole()->getConsoleActive()))
            return 0;                               // an open console owns the keys
        if (!GetAsyncKeyState(VK_CONTROL) || (GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_MENU)))
        {
            // No Ctrl, or Ctrl+Alt (AltGr): ESC goes to the menu, anything else to its key ring.
            if (wParam == 27)
                menuLogic->onEscape();              // +0x48
            else
                pushMenuKey(static_cast<char>(wParam));
            return 0;
        }
        {
            // Ctrl held: the control character arrives as 1..26; folded to its lowercase letter,
            // and only paste / copy / cut are let through.
            const char key = static_cast<char>(wParam | 0x60);
            if (key == 'v' || key == 'c' || key == 'x')
                pushMenuKey(key);
        }
        return 0;

    default:
        return 0;
    }
}
#endif // !dedicated

// Crash-dump helpers create the output directory and write through a joined worker.
// [INFERRED] Helper names.
const wchar_t* getCrashDumpDirectory();

//  (client, 353 B) / 0x401410 (dedicated, 369 B). Built once into a function-local static
// (its guard word is at 0x986F3C): "<directory>\bf2_v<build number>.dmp". [INFERRED] name.
static const wchar_t* crashDumpFileName(const wchar_t* dumpDirectory)
{
    static std::wstring s_fileName;
    if (s_fileName.empty())
        s_fileName = std::wstring(dumpDirectory) + L"\\bf2_v"
                   + toWideString(BuildNrUtil::getBuildNrString()) + L".dmp";
    return s_fileName.c_str();
}

// bf2Main: create BF2, run it, tear it down (0x402910 / 0x401590)
// `int __stdcall (HINSTANCE, HINSTANCE, LPSTR)` in both PEs (`retn 0Ch`) - the WinMain triple
// minus nShowCmd. [INFERRED] name - this file already called it that.
static int __stdcall bf2Main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine)
{
    (void)hInstance;
    (void)hPrevInstance;

    g_system->restoreFPUExceptions();               // PE slot +0x2C (ELF +0x30) - empty in every image

    BF2* bf2 = new BF2();                           // operator new(4)
    if (!bf2->init(std::string(lpCmdLine)))
        return 42;

    Vars::enableProfiling(true);                    //  / 0x40D460 - a flag no image reads

    // +demo <con-file>: the name IS a config-file name, hence the `true`.
    const std::string demo = settings->stringGet("GSDemo", std::string());
    if (!demo.empty() && bf2Engine)
        bf2Engine->playDemo(true, demo);

    do
    {
        g_system->restoreFPUExceptions();
        bf2->handleInGameWinMessages();
    }
    while (bf2Engine->mainLoop());                  // the PE reads bf2's member; bf2Engine IS that pointer

    Vars::enableProfiling(false);

#if !DEDICATED
    if (Window* window = bf2Engine->getWindow())    // absent from the dedicated PE
        window->removeMessageHandler(bf2MessageHandler);    // slot +0x34
#endif

    delete bf2;                                     // ~BF2 -> shutdown(): engine->shutdown(), delete engine
    g_system->storeAndTurnOffFPUExceptions();       // PE slot +0x28 (ELF +0x2C)
    return 0;
}

// Desktop entry wrapper and SEH frame. Do not create objects with C++ destructors
// in this function because it contains __try. [INFERRED] name.
static int bf2WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine)
{
#if !DEDICATED
    // The largest working-set minimum Windows will grant: 128 MB down to 64 MB in 16 MB steps,
    // with the maximum passed as (SIZE_T)-1, as shipped.
    for (SIZE_T size = 0x8000000; size > 0x4000000; size -= 0x1000000)
        if (SetProcessWorkingSetSize(GetCurrentProcess(), size, static_cast<SIZE_T>(-1)))
            break;
#endif

    SystemParametersInfoA(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s_startupStickyKeys, 0);      // 0x3A
    SystemParametersInfoA(SPI_GETTOGGLEKEYS, sizeof(TOGGLEKEYS), &s_startupToggleKeys, 0);      // 0x34
    SystemParametersInfoA(SPI_GETFILTERKEYS, sizeof(FILTERKEYS), &s_startupFilterKeys, 0);      // 0x32
    SystemParametersInfoA(SPI_GETMOUSETRAILS, 0, &s_startupMouseTrails, 0);                    // 0x5E
    s_screenSaverActive = 0;
    SystemParametersInfoA(SPI_GETSCREENSAVEACTIVE, 0, &s_screenSaverActive, 0);                // 0x10
    if (s_screenSaverActive)
        SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, 0, 0, SPIF_SENDCHANGE);                 // 0x11
    allowAccessibilityShortcutKeys(false);

    const bool sehDisabled = strstr(lpCmdLine, "+seh 0") != 0;
    strstr(lpCmdLine, "lockMemory 0");              // result discarded as shipped
    if (strstr(lpCmdLine, "+useFpuExceptions"))
        g_system->setUseFpuExceptions(true);        // System +0x20 in the PE (+0x1C in the ELF)

    int result;
    // ..0x4017A6 queries the debugger even when +seh 0 was present.
    const bool debuggerInstalled = systemInfo()->getIsDebuggerInstalled();
    if (sehDisabled || debuggerInstalled)
    {
        result = bf2Main(hInstance, hPrevInstance, lpCmdLine);
    }
    else
    {
        strstr(lpCmdLine, "+seh");                  // result discarded as shipped
        const wchar_t* dumpFileName = crashDumpFileName(getCrashDumpDirectory());
        //  installs the SEH scope before the single call at 0x4017DE.
        __try
        {
            result = bf2Main(hInstance, hPrevInstance, lpCmdLine);
        }
        __except (writeCrashDump(GetExceptionInformation(), crashDumpTargetName(dumpFileName)),
                  EXCEPTION_EXECUTE_HANDLER)
        {
            result = 42;                            //  / 0x40180C: `mov edi, 2Ah`
        }
    }

    restoreDesktopSettings();
    return result;
}

} // namespace hfe
} // namespace dice

//  / 0x401890, 83 B in both. The System singleton is constructed first and destroyed last,
// through its deleting destructor (slot +0x10 in the PE), exactly as the Linux main does.
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int /*nShowCmd*/)
{
    dice::hfe::g_system = new dice::hfe::System();  // operator new(0x30) == 48 bytes on Win32
    const int result = dice::hfe::bf2WinMain(hInstance, hPrevInstance, lpCmdLine);
    if (dice::hfe::g_system)
        delete dice::hfe::g_system;
    return result;
}

#if DEDICATED
// [PLATFORM] The rebuilt Windows dedicated server uses a console entry point.
// Runs BF2 until shutdown, destroys it to unwind engine/network state, then releases System.
// The diagnostic harness supplies its own entry point; the production target uses this one.
int main(int argc, char** argv)
{
    // [CHOSEN, not recovered] Keep headless stdout unbuffered so piped diagnostics
    // appear immediately and survive abnormal termination.
    std::setvbuf(stdout, 0, _IONBF, 0);
    std::setvbuf(stderr, 0, _IONBF, 0);

    dice::hfe::g_system = new dice::hfe::System();       // operator new(0x30) == 48 bytes on Win32

    // Join argv[1..] into the single command-line string the engine expects (as WinMain would hand
    // it), re-quoting any argument that contains a space.
    std::string commandLine;
    for (int i = 1; i < argc; ++i)
        appendCommandLineArg(commandLine, argv[i], i == 1);

    dice::hfe::BF2* bf2 = new dice::hfe::BF2();

    if (!bf2->init(commandLine))
    {
        std::cerr << "fatal error: argument parsing failed" << std::endl;
        return 42;
    }

    while (bf2->run())
        ;

    delete bf2;                                          // the clean shutdown + rebind path
    if (dice::hfe::g_system)
        delete dice::hfe::g_system;                      // the System is destroyed last, as on both ports
    return 0;
}

#endif // dedicated - the headless console entry

#else

// [PLATFORM] The ELF entry point. ELF 0x8052ab0.
int main(int argc, const char** argv, const char** envp)
{
    dice::hfe::g_system = new dice::hfe::System();   // operator new(40) == 40 bytes on Linux

    // Join argv[1..] with spaces, so the rest of the engine can take the same single
    // command-line string that Windows hands to WinMain. Re-quote any argument with a space so a
    // path such as "/media/Battlefield 2/mods/fh2/.../maplist.con" survives the rejoin.
    std::string commandLine;
    for (int i = 1; i < argc; ++i)
        appendCommandLineArg(commandLine, argv[i], i == 1);

    dice::hfe::BF2* bf2 = new dice::hfe::BF2();      // operator new(4)

    if (!bf2->init(commandLine))
    {
        std::cerr << "fatal error: argument parsing failed" << std::endl;
        return 42;
    }

    while (bf2->run())
        ;

    return 0;
}

#endif
