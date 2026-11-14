// [TEMP] [CHOSEN, not recovered] Test tool: a dinput8.dll proxy that drives a Battlefield 2 client
// (retail BF2.exe or the rebuilt one) from a script, in the background, for unattended tests in a
// VM where mouse input synthesized inside the guest never reaches DirectInput.
//
// Put the built dinput8.dll next to a COPY of BF2.exe (never into the original install) and set
// DI8PROXY_SCRIPT=<file>. Without the variable every call is forwarded unchanged to the system
// dinput8.dll. With it:
//   - DirectInput8Create wraps IDirectInput8A; CreateDevice wraps the system mouse and keyboard.
//   - The wrapped devices report Acquire success even without focus, and GetDeviceData /
//     GetDeviceState return scripted data only (the real device's input is discarded).
//   - Keys are also posted to the game window (the window given to SetCooperativeLevel) as
//     WM_KEYDOWN / WM_KEYUP, so the window-message menus see them; TranslateMessage adds WM_CHAR.
//   - "click X Y" posts WM_MOUSEMOVE / WM_LBUTTONDOWN / WM_LBUTTONUP at client coordinates (menus).
// Script format: the BF2_INPUT_SCRIPT format (docs/macos-c-input-script.md) without the commands
// that need game knowledge (aim, face, walk-to). Commands: wait-ms, wait-frames, timeout, wait-log,
// wait-log-else, label, key, key-down, key-up, type, console, move-rel, button-down, button-up,
// wheel, pointer, hud-click, pointer-scale, move, click, capture, log, quit, end.
//   - "move X Y" posts one WM_MOUSEMOVE at client coordinates (menu hover without a press).
//   - "type TEXT" presses, for each character, the key that produces it on the current keyboard
//     layout without modifiers (upper-case letters are typed lower-case; characters that need
//     Shift/AltGr are skipped with a report, because a posted key message cannot carry the
//     modifier state TranslateMessage reads). "console CMD" is Grave, type CMD, Enter, Grave.
//   - "wait-pixel-else S LABEL X Y R G B TOL" waits until the game window's client pixel (X, Y) is
//     within TOL of (R, G, B) on every channel (a screen read, as capture), else after S seconds
//     jumps to "label LABEL"; for a retail client, whose load end is in no log.
// wait-log reads DI8PROXY_LOG (a file the game writes, e.g. the rebuilt client's redirected stdout, or a
// server-side log), from its size when the script starts.
// Reports go to DI8PROXY_REPORT (default: dinput8proxy.log next to the DLL).
// capture LABEL writes the game window's client area as present_<n>_<LABEL>.bmp into
// DI8PROXY_CAPTURE_DIR (default: the report's directory).
// [RETIRE-WHEN: client-test-automation-replaced]
#define _CRT_SECURE_NO_WARNINGS
#define DIRECTINPUT_VERSION 0x0800
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dinput.h>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifndef MAPVK_VSC_TO_VK_EX
#define MAPVK_VSC_TO_VK_EX 3
#endif

namespace {

// ---------------------------------------------------------------------------------------------
// The system dinput8.dll
// ---------------------------------------------------------------------------------------------
HMODULE g_real = 0;
HMODULE g_self = 0;
typedef HRESULT(WINAPI* CreateFn)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(WINAPI* GetClassObjectFn)(REFCLSID, REFIID, LPVOID*);
typedef HRESULT(WINAPI* VoidFn)();

FARPROC realProc(const char* name)
{
    if (!g_real)
    {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        strcat_s(path, "\\dinput8.dll");
        g_real = LoadLibraryA(path);
    }
    return g_real ? GetProcAddress(g_real, name) : 0;
}

// ---------------------------------------------------------------------------------------------
// Reports
// ---------------------------------------------------------------------------------------------
std::mutex g_reportLock;
std::string g_reportPath;
DWORD g_startTick = 0;

std::string moduleDir()
{
    char path[MAX_PATH] = {};
    GetModuleFileNameA(g_self, path, MAX_PATH);
    std::string p(path);
    const std::size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? std::string(".") : p.substr(0, slash);
}

void report(const char* fmt, ...)
{
    char text[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt, args);
    va_end(args);
    std::lock_guard<std::mutex> lock(g_reportLock);
    if (g_reportPath.empty())
    {
        const char* env = std::getenv("DI8PROXY_REPORT");
        g_reportPath = env && *env ? env : moduleDir() + "\\dinput8proxy.log";
    }
    if (FILE* f = std::fopen(g_reportPath.c_str(), "a"))
    {
        std::fprintf(f, "[TEMP] [DI8PROXY] t=%.2fs %s\n", (GetTickCount() - g_startTick) / 1000.0, text);
        std::fclose(f);
    }
}

// ---------------------------------------------------------------------------------------------
// Scripted device data
// ---------------------------------------------------------------------------------------------
struct Feed
{
    std::deque<DIDEVICEOBJECTDATA> events;
    bool acquired = false;
};

std::mutex g_feedLock;
Feed g_mouse, g_keyboard;
DWORD g_sequence = 0;
BYTE g_keys[256] = {};
LONG g_stateX = 0, g_stateY = 0, g_stateZ = 0;       // motion since the last GetDeviceState
BYTE g_buttons[8] = {};
HWND g_window = 0;                                   // from SetCooperativeLevel
bool g_scripted = false;

void push(Feed& feed, DWORD offset, DWORD data)
{
    DIDEVICEOBJECTDATA e;
    std::memset(&e, 0, sizeof(e));
    e.dwOfs = offset;
    e.dwData = data;
    e.dwTimeStamp = GetTickCount();
    e.dwSequence = ++g_sequence;
    if (!feed.acquired)
        return;                                      // as a real buffer, only while acquired
    feed.events.push_back(e);
    while (feed.events.size() > 256)
        feed.events.pop_front();
}

void scriptKey(unsigned short dik, bool down)
{
    {
        std::lock_guard<std::mutex> lock(g_feedLock);
        g_keys[dik & 0xFF] = down ? 0x80 : 0;
        push(g_keyboard, dik, down ? 0x80u : 0u);
    }
    if (!g_window)
        return;
    const UINT scan = dik & 0x7Fu;
    UINT vk = MapVirtualKeyA((dik & 0x80u) ? (0xE000u | scan) : scan, MAPVK_VSC_TO_VK_EX);
    if (vk == VK_LSHIFT || vk == VK_RSHIFT) vk = VK_SHIFT;
    if (vk == VK_LCONTROL || vk == VK_RCONTROL) vk = VK_CONTROL;
    if (vk == VK_LMENU || vk == VK_RMENU) vk = VK_MENU;
    LPARAM lParam = 1 | (static_cast<LPARAM>(scan) << 16);
    if (dik & 0x80u) lParam |= 1 << 24;
    if (!down) lParam |= static_cast<LPARAM>(3u << 30);
    if (vk)
        PostMessageA(g_window, down ? WM_KEYDOWN : WM_KEYUP, vk, lParam);
}

float g_remX = 0, g_remY = 0;
void scriptMove(float dx, float dy)
{
    g_remX += dx;
    g_remY += dy;
    const LONG ix = static_cast<LONG>(g_remX), iy = static_cast<LONG>(g_remY);
    g_remX -= ix;
    g_remY -= iy;
    std::lock_guard<std::mutex> lock(g_feedLock);
    g_stateX += ix;
    g_stateY += iy;
    if (ix) push(g_mouse, DIMOFS_X, static_cast<DWORD>(ix));
    if (iy) push(g_mouse, DIMOFS_Y, static_cast<DWORD>(iy));
}

void scriptButton(int b, bool down)
{
    if (b < 0 || b > 7)
        return;
    std::lock_guard<std::mutex> lock(g_feedLock);
    g_buttons[b] = down ? 0x80 : 0;
    push(g_mouse, DIMOFS_BUTTON0 + b, down ? 0x80u : 0u);
}

void scriptWheel(int delta)
{
    std::lock_guard<std::mutex> lock(g_feedLock);
    g_stateZ += delta;
    push(g_mouse, DIMOFS_Z, static_cast<DWORD>(static_cast<LONG>(delta)));
}

// The game window's client area as a 24-bit BMP.
void captureScaled(const std::string& label, unsigned int index);

// The retail copy runs DPI-unaware (its HIGHDPIAWARE compatibility layer is keyed to the original
// install path), so window coordinates are scaled; capture with a per-monitor-aware thread context
// so the client rectangle and the screen copy use the same physical pixels.
void capture(const std::string& label, unsigned int index)
{
    typedef void*(WINAPI* SetContextFn)(void*);
    static SetContextFn setContext = reinterpret_cast<SetContextFn>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "SetThreadDpiAwarenessContext"));
    void* const perMonitorAware = reinterpret_cast<void*>(static_cast<INT_PTR>(-3));
    void* const previous = setContext ? setContext(perMonitorAware) : 0;
    captureScaled(label, index);
    if (setContext && previous)
        setContext(previous);
}

void captureScaled(const std::string& label, unsigned int index)
{
    RECT r;
    if (!g_window || !GetClientRect(g_window, &r) || r.right <= 0 || r.bottom <= 0)
    {
        report("capture %s: no window", label.c_str());
        return;
    }
    const int w = r.right, h = r.bottom;
    HDC windowDc = GetDC(g_window);
    HDC memory = CreateCompatibleDC(windowDc);
    BITMAPINFO info;
    std::memset(&info, 0, sizeof(info));
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = w;
    info.bmiHeader.biHeight = h;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 24;
    void* bits = 0;
    HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, 0, 0);
    bool ok = false;
    if (bitmap)
    {
        HGDIOBJ old = SelectObject(memory, bitmap);
        // A screen copy of the client area (PrintWindow returns black for a Direct3D swap chain);
        // PrintWindow only as the fallback.
        POINT origin = {0, 0};
        ClientToScreen(g_window, &origin);
        HDC screen = GetDC(0);
        ok = BitBlt(memory, 0, 0, w, h, screen, origin.x, origin.y, SRCCOPY | CAPTUREBLT) != FALSE;
        ReleaseDC(0, screen);
        if (!ok)
            ok = PrintWindow(g_window, memory, 3) != FALSE;   // PW_CLIENTONLY | PW_RENDERFULLCONTENT
        GdiFlush();
        SelectObject(memory, old);
    }
    if (ok)
    {
        const char* env = std::getenv("DI8PROXY_CAPTURE_DIR");
        std::string dir = env && *env ? env : moduleDir();
        char path[MAX_PATH];
        std::snprintf(path, sizeof(path), "%s\\present_%05u_%s.bmp", dir.c_str(), index, label.c_str());
        const DWORD stride = (static_cast<DWORD>(w) * 3 + 3) & ~3u;
        BITMAPFILEHEADER file;
        std::memset(&file, 0, sizeof(file));
        file.bfType = 0x4D42;
        file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        file.bfSize = file.bfOffBits + stride * h;
        if (FILE* f = std::fopen(path, "wb"))
        {
            std::fwrite(&file, sizeof(file), 1, f);
            std::fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, f);
            std::fwrite(bits, 1, stride * h, f);
            std::fclose(f);
            report("capture %s", path);
        }
    }
    else
        report("capture %s: screen copy failed", label.c_str());
    if (bitmap) DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(g_window, windowDc);
}

// ---------------------------------------------------------------------------------------------
// The script (BF2_INPUT_SCRIPT format subset)
// ---------------------------------------------------------------------------------------------
struct KeyName { const char* name; unsigned short code; };
const KeyName kKeys[] = {
    {"a", DIK_A}, {"b", DIK_B}, {"c", DIK_C}, {"d", DIK_D}, {"e", DIK_E}, {"f", DIK_F},
    {"g", DIK_G}, {"h", DIK_H}, {"i", DIK_I}, {"j", DIK_J}, {"k", DIK_K}, {"l", DIK_L},
    {"m", DIK_M}, {"n", DIK_N}, {"o", DIK_O}, {"p", DIK_P}, {"q", DIK_Q}, {"r", DIK_R},
    {"s", DIK_S}, {"t", DIK_T}, {"u", DIK_U}, {"v", DIK_V}, {"w", DIK_W}, {"x", DIK_X},
    {"y", DIK_Y}, {"z", DIK_Z},
    {"0", DIK_0}, {"1", DIK_1}, {"2", DIK_2}, {"3", DIK_3}, {"4", DIK_4},
    {"5", DIK_5}, {"6", DIK_6}, {"7", DIK_7}, {"8", DIK_8}, {"9", DIK_9},
    {"escape", DIK_ESCAPE}, {"esc", DIK_ESCAPE}, {"enter", DIK_RETURN}, {"return", DIK_RETURN},
    {"space", DIK_SPACE}, {"tab", DIK_TAB}, {"backspace", DIK_BACK}, {"grave", DIK_GRAVE},
    {"minus", DIK_MINUS}, {"equals", DIK_EQUALS}, {"comma", DIK_COMMA}, {"period", DIK_PERIOD},
    {"f1", DIK_F1}, {"f2", DIK_F2}, {"f3", DIK_F3}, {"f4", DIK_F4}, {"f5", DIK_F5}, {"f6", DIK_F6},
    {"f7", DIK_F7}, {"f8", DIK_F8}, {"f9", DIK_F9}, {"f10", DIK_F10}, {"f11", DIK_F11}, {"f12", DIK_F12},
    {"up", DIK_UP}, {"down", DIK_DOWN}, {"left", DIK_LEFT}, {"right", DIK_RIGHT},
    {"leftshift", DIK_LSHIFT}, {"shift", DIK_LSHIFT}, {"rightshift", DIK_RSHIFT},
    {"leftctrl", DIK_LCONTROL}, {"ctrl", DIK_LCONTROL}, {"leftalt", DIK_LMENU}, {"alt", DIK_LMENU},
};

bool lookupKey(std::string name, unsigned short& code)
{
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (name.compare(0, 6, "idkey_") == 0) name = name.substr(6);
    if (name.size() > 2 && name[0] == '0' && name[1] == 'x')
    {
        code = static_cast<unsigned short>(std::strtoul(name.c_str(), 0, 16));
        return true;
    }
    for (const KeyName& k : kKeys)
        if (name == k.name) { code = k.code; return true; }
    return false;
}

std::string trim(const std::string& s)
{
    std::size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string restArgument(const std::string& rest)
{
    std::string t = trim(rest);
    if (t.size() >= 2 && t.front() == '"' && t.back() == '"')
        t = t.substr(1, t.size() - 2);
    return t;
}

enum Op { OpWaitMs, OpWaitLog, OpWaitLogElse, OpLabel, OpTimeout, OpKeyDown, OpKeyUp, OpMoveBy,
          OpButtonDown, OpButtonUp, OpWheel, OpClick, OpMoveTo, OpCapture, OpLog, OpQuit, OpEnd,
          OpWaitPixelElse };
struct Step { Op op; int line; int a; float x, y; std::string text, label; int rgb[4]; };

// The client pixel (x, y) of the game window as 0x00BBGGRR, or CLR_INVALID (per-monitor-aware
// screen read, as capture).
COLORREF windowPixel(int x, int y)
{
    if (!g_window)
        return CLR_INVALID;
    typedef void*(WINAPI* SetContextFn)(void*);
    static SetContextFn setContext = reinterpret_cast<SetContextFn>(
        GetProcAddress(GetModuleHandleA("user32.dll"), "SetThreadDpiAwarenessContext"));
    void* const previous = setContext ? setContext(reinterpret_cast<void*>(static_cast<INT_PTR>(-3))) : 0;
    POINT at = {x, y};
    ClientToScreen(g_window, &at);
    HDC screen = GetDC(0);
    const COLORREF c = GetPixel(screen, at.x, at.y);
    ReleaseDC(0, screen);
    if (setContext && previous)
        setContext(previous);
    return c;
}

class Script
{
public:
    bool load(const char* path)
    {
        std::ifstream in(path);
        if (!in)
        {
            report("cannot read %s; script off", path);
            return false;
        }
        std::string line;
        int number = 0;
        while (std::getline(in, line))
            if (!parseLine(line, ++number))
                return false;
        if (const char* log = std::getenv("DI8PROXY_LOG"))
        {
            // Only text written after the script starts counts: the log may be a server's file that
            // earlier client runs already wrote to.
            m_logPath = log;
            WIN32_FILE_ATTRIBUTE_DATA info;
            if (GetFileAttributesExA(log, GetFileExInfoStandard, &info))
                m_logOffset = (static_cast<long long>(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        }
        report("loaded %s: %u steps; log %s", path, static_cast<unsigned>(m_steps.size()),
               m_logPath.empty() ? "(none: wait-log unavailable)" : m_logPath.c_str());
        return true;
    }

    void run()
    {
        for (std::size_t pc = 0; pc < m_steps.size(); ++pc)
        {
            Step& s = m_steps[pc];
            switch (s.op)
            {
            case OpWaitMs: Sleep(static_cast<DWORD>(s.a)); break;
            case OpWaitLog:
            case OpWaitLogElse:
            {
                report("line %d: wait-log \"%s\"", s.line, s.text.c_str());
                const DWORD start = GetTickCount();
                const double limit = s.op == OpWaitLogElse ? s.x : m_timeout;
                bool found = false;
                while (!(found = logContains(s.text)) && (GetTickCount() - start) / 1000.0 < limit)
                    Sleep(100);
                if (found)
                    report("line %d: matched \"%s\"", s.line, s.text.c_str());
                else if (s.op == OpWaitLogElse)
                {
                    report("line %d: no \"%s\" after %.0f s; jumping to label %s", s.line, s.text.c_str(), s.x, s.label.c_str());
                    while (pc + 1 < m_steps.size() && !(m_steps[pc + 1].op == OpLabel && m_steps[pc + 1].label == s.label))
                        ++pc;
                }
                else
                {
                    report("line %d: TIMEOUT after %.0f s waiting for \"%s\"; script stopped", s.line, m_timeout, s.text.c_str());
                    return;
                }
                break;
            }
            case OpWaitPixelElse:
            {
                report("line %d: wait-pixel %d,%d ~ %d/%d/%d", s.line, static_cast<int>(s.x), static_cast<int>(s.y), s.rgb[0], s.rgb[1], s.rgb[2]);
                const DWORD start = GetTickCount();
                bool found = false;
                COLORREF c = CLR_INVALID;
                while ((GetTickCount() - start) / 1000.0 < s.a)
                {
                    c = windowPixel(static_cast<int>(s.x), static_cast<int>(s.y));
                    if (c != CLR_INVALID && std::abs(GetRValue(c) - s.rgb[0]) <= s.rgb[3] &&
                        std::abs(GetGValue(c) - s.rgb[1]) <= s.rgb[3] && std::abs(GetBValue(c) - s.rgb[2]) <= s.rgb[3])
                    {
                        found = true;
                        break;
                    }
                    Sleep(250);
                }
                if (found)
                    report("line %d: pixel matched after %.1f s", s.line, (GetTickCount() - start) / 1000.0);
                else
                {
                    report("line %d: pixel not matched after %d s (last %d/%d/%d); jumping to label %s", s.line, s.a,
                           c == CLR_INVALID ? -1 : GetRValue(c), c == CLR_INVALID ? -1 : GetGValue(c),
                           c == CLR_INVALID ? -1 : GetBValue(c), s.label.c_str());
                    while (pc + 1 < m_steps.size() && !(m_steps[pc + 1].op == OpLabel && m_steps[pc + 1].label == s.label))
                        ++pc;
                }
                break;
            }
            case OpLabel: break;
            case OpTimeout: m_timeout = s.x > 0 ? s.x : 180.0; break;
            case OpKeyDown: scriptKey(static_cast<unsigned short>(s.a), true); break;
            case OpKeyUp: scriptKey(static_cast<unsigned short>(s.a), false); break;
            case OpMoveBy: scriptMove(s.x, s.y); break;
            case OpButtonDown: scriptButton(s.a, true); break;
            case OpButtonUp: scriptButton(s.a, false); break;
            case OpWheel: scriptWheel(s.a * 120); break;
            case OpClick:
                if (g_window)
                {
                    const LPARAM at = MAKELPARAM(static_cast<int>(s.x), static_cast<int>(s.y));
                    PostMessageA(g_window, WM_MOUSEMOVE, 0, at);
                    Sleep(100);
                    PostMessageA(g_window, WM_LBUTTONDOWN, MK_LBUTTON, at);
                    Sleep(100);
                    PostMessageA(g_window, WM_LBUTTONUP, 0, at);
                }
                break;
            case OpMoveTo:
                if (g_window)
                    PostMessageA(g_window, WM_MOUSEMOVE, 0, MAKELPARAM(static_cast<int>(s.x), static_cast<int>(s.y)));
                break;
            case OpCapture: capture(s.text.empty() ? "capture" : s.text, ++m_captures); break;
            case OpLog: report("line %d: %s", s.line, s.text.c_str()); break;
            case OpQuit:
                report("line %d: quit", s.line);
                if (g_window) PostMessageA(g_window, WM_CLOSE, 0, 0);
                return;
            case OpEnd: pc = m_steps.size(); continue;
            }
        }
        report("script complete");
    }

private:
    void add(Op op, int line, int a = 0, float x = 0, float y = 0)
    {
        Step s; s.op = op; s.line = line; s.a = a; s.x = x; s.y = y; s.rgb[0] = s.rgb[1] = s.rgb[2] = s.rgb[3] = 0;
        m_steps.push_back(s);
    }
    void addHold(int line, int ms) { add(OpWaitMs, line, ms); }
    void addPress(unsigned short code, int line, int hold)
    {
        add(OpKeyDown, line, code); addHold(line, hold); add(OpKeyUp, line, code); addHold(line, 40);
    }

    // Each character as the unmodified key that produces it on the current layout (see "type").
    void addText(const std::string& text, int number)
    {
        const HKL layout = GetKeyboardLayout(0);
        for (char ch : text)
        {
            const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            const SHORT r = VkKeyScanExA(c, layout);
            const UINT scan = r == -1 ? 0 : MapVirtualKeyExA(r & 0xFF, 0 /* MAPVK_VK_TO_VSC */, layout);
            if (r == -1 || (r >> 8) & 7 || !scan)
            {
                report("line %d: type: '%c' needs a modifier or has no key; skipped", number, ch);
                continue;
            }
            addPress(static_cast<unsigned short>(scan), number, 40);
        }
    }

    bool parseLine(const std::string& raw, int number)
    {
        const std::string line = trim(raw);
        if (line.empty() || line[0] == '#')
            return true;
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        std::string rest;
        std::getline(in, rest);
        std::istringstream args(rest);
        if (cmd == "wait-ms") { int ms = 0; args >> ms; add(OpWaitMs, number, ms); }
        else if (cmd == "wait-frames") { int n = 0; args >> n; add(OpWaitMs, number, n * 16); }
        else if (cmd == "timeout") { float t = 0; args >> t; add(OpTimeout, number, 0, t); }
        else if (cmd == "wait-log") { add(OpWaitLog, number); m_steps.back().text = restArgument(rest); }
        else if (cmd == "wait-log-else")
        {
            float seconds = 0; std::string label, text;
            args >> seconds >> label;
            std::getline(args, text);
            add(OpWaitLogElse, number, 0, seconds);
            m_steps.back().label = label;
            m_steps.back().text = restArgument(text);
        }
        else if (cmd == "wait-pixel-else")
        {
            float seconds = 0, x = 0, y = 0; std::string label; int r = 0, g = 0, b = 0, tol = 0;
            args >> seconds >> label >> x >> y >> r >> g >> b >> tol;
            add(OpWaitPixelElse, number, static_cast<int>(seconds), x, y);
            Step& st = m_steps.back();
            st.label = label; st.rgb[0] = r; st.rgb[1] = g; st.rgb[2] = b; st.rgb[3] = tol;
        }
        else if (cmd == "label") { std::string label; args >> label; add(OpLabel, number); m_steps.back().label = label; }
        else if (cmd == "key" || cmd == "key-down" || cmd == "key-up")
        {
            std::string name; int hold = 60;
            args >> name >> hold;
            unsigned short code;
            if (!lookupKey(name, code)) { report("line %d: unknown key '%s'", number, name.c_str()); return false; }
            if (cmd == "key") addPress(code, number, hold);
            else add(cmd == "key-down" ? OpKeyDown : OpKeyUp, number, code);
        }
        else if (cmd == "move-rel") { float x = 0, y = 0; args >> x >> y; add(OpMoveBy, number, 0, x, y); }
        else if (cmd == "button-down" || cmd == "button-up")
        {
            int b = 0; args >> b; add(cmd == "button-down" ? OpButtonDown : OpButtonUp, number, b);
        }
        else if (cmd == "wheel") { int n = 0; args >> n; add(OpWheel, number, n); }
        else if (cmd == "click") { float x = 0, y = 0; args >> x >> y; add(OpClick, number, 0, x, y); addHold(number, 200); }
        else if (cmd == "move") { float x = 0, y = 0; args >> x >> y; add(OpMoveTo, number, 0, x, y); }
        else if (cmd == "type") addText(restArgument(rest), number);
        else if (cmd == "console")
        {
            addPress(DIK_GRAVE, number, 60); addHold(number, 250);
            addText(restArgument(rest), number); addHold(number, 100);
            addPress(DIK_RETURN, number, 60); addHold(number, 250);
            addPress(DIK_GRAVE, number, 60); addHold(number, 250);
        }
        else if (cmd == "pointer-scale") { args >> m_pointerScale; if (m_pointerScale <= 0) m_pointerScale = 1.2f; }
        else if (cmd == "pointer" || cmd == "hud-click")
        {
            // The in-game HUD pointer moves by relative mouse input and clamps to 800x600: home it
            // to the top-left corner, then move by X/scale, Y/scale counts.
            float x = 0, y = 0; int button = 0;
            args >> x >> y >> button;
            add(OpMoveBy, number, 0, -4000, -4000); addHold(number, 150);
            add(OpMoveBy, number, 0, x / m_pointerScale, y / m_pointerScale); addHold(number, 150);
            if (cmd == "hud-click")
            {
                add(OpButtonDown, number, button); addHold(number, 120);
                add(OpButtonUp, number, button); addHold(number, 150);
            }
        }
        else if (cmd == "capture") { add(OpCapture, number); m_steps.back().text = restArgument(rest); }
        else if (cmd == "log") { add(OpLog, number); m_steps.back().text = restArgument(rest); }
        else if (cmd == "quit") add(OpQuit, number);
        else if (cmd == "end") add(OpEnd, number);
        else if (cmd == "aim" || cmd == "face" || cmd == "walk-to")
        {
            add(OpLog, number);
            m_steps.back().text = "skipped (needs game knowledge): " + line;
        }
        else
        {
            report("line %d: unknown command '%s'", number, cmd.c_str());
            return false;
        }
        return true;
    }

    bool logContains(const std::string& needle)
    {
        if (m_logPath.empty())
            return false;
        std::ifstream log(m_logPath.c_str(), std::ios::binary);
        if (!log)
            return false;
        log.seekg(static_cast<std::streamoff>(m_logOffset));
        char buf[65536];
        while (log)
        {
            log.read(buf, sizeof(buf));
            const std::streamsize n = log.gcount();
            if (n <= 0) break;
            m_logOffset += n;
            m_logTail.append(buf, static_cast<std::size_t>(n));
        }
        const std::size_t at = m_logTail.find(needle);
        if (at != std::string::npos)
        {
            m_logTail.erase(0, at + needle.size());
            return true;
        }
        if (m_logTail.size() > (1u << 20))
            m_logTail.erase(0, m_logTail.size() - needle.size());
        return false;
    }

    std::vector<Step> m_steps;
    double m_timeout = 180.0;
    float m_pointerScale = 1.2f;
    std::string m_logPath, m_logTail;
    long long m_logOffset = 0;
    unsigned int m_captures = 0;
};

Script g_script;

void startScript()
{
    static bool started = false;
    if (started)
        return;
    started = true;
    const char* path = std::getenv("DI8PROXY_SCRIPT");
    if (!path || !*path)
        return;
    g_startTick = GetTickCount();
    if (!g_script.load(path))
        return;
    g_scripted = true;
    std::thread([] { g_script.run(); }).detach();
}

// ---------------------------------------------------------------------------------------------
// IDirectInputDevice8A wrapper
// ---------------------------------------------------------------------------------------------
enum DeviceKind { KindMouse, KindKeyboard };

class ProxyDevice : public IDirectInputDevice8A
{
public:
    ProxyDevice(IDirectInputDevice8A* real, DeviceKind kind) : m_real(real), m_kind(kind), m_ref(1) {}

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* out) override
    {
        if (riid == IID_IUnknown || riid == IID_IDirectInputDevice8A)
        {
            *out = this;
            AddRef();
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG n = InterlockedDecrement(&m_ref);
        if (n == 0)
        {
            m_real->Release();
            delete this;
        }
        return n;
    }

    // Scripted
    HRESULT STDMETHODCALLTYPE SetCooperativeLevel(HWND window, DWORD flags) override
    {
        if (window)
            g_window = window;
        const HRESULT hr = m_real->SetCooperativeLevel(window, flags);
        if (FAILED(hr))
        {
            report("SetCooperativeLevel(%s, 0x%lX) failed 0x%08lX; reported DI_OK", name(), flags, hr);
            return DI_OK;
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE Acquire() override
    {
        m_real->Acquire();                               // the real device's input is discarded
        std::lock_guard<std::mutex> lock(g_feedLock);
        Feed& f = feed();
        if (!f.acquired)
        {
            f.acquired = true;
            f.events.clear();
        }
        return DI_OK;
    }
    HRESULT STDMETHODCALLTYPE Unacquire() override
    {
        m_real->Unacquire();
        std::lock_guard<std::mutex> lock(g_feedLock);
        feed().acquired = false;
        feed().events.clear();
        return DI_OK;
    }
    HRESULT STDMETHODCALLTYPE Poll() override { return DI_OK; }
    HRESULT STDMETHODCALLTYPE GetDeviceData(DWORD size, LPDIDEVICEOBJECTDATA data, LPDWORD inOut, DWORD flags) override
    {
        if (!inOut)
            return DIERR_INVALIDPARAM;
        std::lock_guard<std::mutex> lock(g_feedLock);
        Feed& f = feed();
        if (!f.acquired)
            return DIERR_NOTACQUIRED;
        const bool peek = (flags & DIGDD_PEEK) != 0;
        DWORD n = 0;
        const DWORD capacity = *inOut;
        std::deque<DIDEVICEOBJECTDATA>::iterator it = f.events.begin();
        while (n < capacity && it != f.events.end())
        {
            if (data)
            {
                BYTE* to = reinterpret_cast<BYTE*>(data) + n * size;
                std::memset(to, 0, size);
                std::memcpy(to, &*it, size < sizeof(DIDEVICEOBJECTDATA) ? size : sizeof(DIDEVICEOBJECTDATA));
            }
            ++n;
            ++it;
        }
        if (!peek)
            f.events.erase(f.events.begin(), it);
        *inOut = n;
        return DI_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceState(DWORD size, LPVOID data) override
    {
        if (!data)
            return DIERR_INVALIDPARAM;
        std::lock_guard<std::mutex> lock(g_feedLock);
        std::memset(data, 0, size);
        if (m_kind == KindKeyboard)
        {
            std::memcpy(data, g_keys, size < 256 ? size : 256);
            return DI_OK;
        }
        if (size >= sizeof(DIMOUSESTATE))
        {
            DIMOUSESTATE2 state;
            std::memset(&state, 0, sizeof(state));
            state.lX = g_stateX; state.lY = g_stateY; state.lZ = g_stateZ;
            std::memcpy(state.rgbButtons, g_buttons, 8);
            std::memcpy(data, &state, size < sizeof(state) ? size : sizeof(state));
            g_stateX = g_stateY = g_stateZ = 0;
        }
        return DI_OK;
    }

    // Forwarded
    HRESULT STDMETHODCALLTYPE GetCapabilities(LPDIDEVCAPS a) override { return m_real->GetCapabilities(a); }
    HRESULT STDMETHODCALLTYPE EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA a, LPVOID b, DWORD c) override { return m_real->EnumObjects(a, b, c); }
    HRESULT STDMETHODCALLTYPE GetProperty(REFGUID a, LPDIPROPHEADER b) override { return m_real->GetProperty(a, b); }
    HRESULT STDMETHODCALLTYPE SetProperty(REFGUID a, LPCDIPROPHEADER b) override { return m_real->SetProperty(a, b); }
    HRESULT STDMETHODCALLTYPE SetDataFormat(LPCDIDATAFORMAT a) override { return m_real->SetDataFormat(a); }
    HRESULT STDMETHODCALLTYPE SetEventNotification(HANDLE a) override { return m_real->SetEventNotification(a); }
    HRESULT STDMETHODCALLTYPE GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA a, DWORD b, DWORD c) override { return m_real->GetObjectInfo(a, b, c); }
    HRESULT STDMETHODCALLTYPE GetDeviceInfo(LPDIDEVICEINSTANCEA a) override { return m_real->GetDeviceInfo(a); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND a, DWORD b) override { return m_real->RunControlPanel(a, b); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE a, DWORD b, REFGUID c) override { return m_real->Initialize(a, b, c); }
    HRESULT STDMETHODCALLTYPE CreateEffect(REFGUID a, LPCDIEFFECT b, LPDIRECTINPUTEFFECT* c, LPUNKNOWN d) override { return m_real->CreateEffect(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE EnumEffects(LPDIENUMEFFECTSCALLBACKA a, LPVOID b, DWORD c) override { return m_real->EnumEffects(a, b, c); }
    HRESULT STDMETHODCALLTYPE GetEffectInfo(LPDIEFFECTINFOA a, REFGUID b) override { return m_real->GetEffectInfo(a, b); }
    HRESULT STDMETHODCALLTYPE GetForceFeedbackState(LPDWORD a) override { return m_real->GetForceFeedbackState(a); }
    HRESULT STDMETHODCALLTYPE SendForceFeedbackCommand(DWORD a) override { return m_real->SendForceFeedbackCommand(a); }
    HRESULT STDMETHODCALLTYPE EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK a, LPVOID b, DWORD c) override { return m_real->EnumCreatedEffectObjects(a, b, c); }
    HRESULT STDMETHODCALLTYPE Escape(LPDIEFFESCAPE a) override { return m_real->Escape(a); }
    HRESULT STDMETHODCALLTYPE SendDeviceData(DWORD a, LPCDIDEVICEOBJECTDATA b, LPDWORD c, DWORD d) override { return m_real->SendDeviceData(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE EnumEffectsInFile(LPCSTR a, LPDIENUMEFFECTSINFILECALLBACK b, LPVOID c, DWORD d) override { return m_real->EnumEffectsInFile(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE WriteEffectToFile(LPCSTR a, DWORD b, LPDIFILEEFFECT c, DWORD d) override { return m_real->WriteEffectToFile(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE BuildActionMap(LPDIACTIONFORMATA a, LPCSTR b, DWORD c) override { return m_real->BuildActionMap(a, b, c); }
    HRESULT STDMETHODCALLTYPE SetActionMap(LPDIACTIONFORMATA a, LPCSTR b, DWORD c) override { return m_real->SetActionMap(a, b, c); }
    HRESULT STDMETHODCALLTYPE GetImageInfo(LPDIDEVICEIMAGEINFOHEADERA a) override { return m_real->GetImageInfo(a); }

private:
    Feed& feed() { return m_kind == KindMouse ? g_mouse : g_keyboard; }
    const char* name() const { return m_kind == KindMouse ? "mouse" : "keyboard"; }

    IDirectInputDevice8A* m_real;
    DeviceKind m_kind;
    LONG m_ref;
};

// ---------------------------------------------------------------------------------------------
// IDirectInput8A wrapper
// ---------------------------------------------------------------------------------------------
class ProxyInput : public IDirectInput8A
{
public:
    explicit ProxyInput(IDirectInput8A* real) : m_real(real), m_ref(1) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, LPVOID* out) override
    {
        if (riid == IID_IUnknown || riid == IID_IDirectInput8A)
        {
            *out = this;
            AddRef();
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG n = InterlockedDecrement(&m_ref);
        if (n == 0)
        {
            m_real->Release();
            delete this;
        }
        return n;
    }
    HRESULT STDMETHODCALLTYPE CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICE8A* out, LPUNKNOWN outer) override
    {
        const HRESULT hr = m_real->CreateDevice(guid, out, outer);
        if (FAILED(hr) || !out || !*out)
            return hr;
        if (guid == GUID_SysMouse || guid == GUID_SysMouseEm || guid == GUID_SysMouseEm2)
        {
            *out = new ProxyDevice(*out, KindMouse);
            report("CreateDevice: system mouse wrapped");
        }
        else if (guid == GUID_SysKeyboard || guid == GUID_SysKeyboardEm || guid == GUID_SysKeyboardEm2)
        {
            *out = new ProxyDevice(*out, KindKeyboard);
            report("CreateDevice: system keyboard wrapped");
        }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE EnumDevices(DWORD a, LPDIENUMDEVICESCALLBACKA b, LPVOID c, DWORD d) override { return m_real->EnumDevices(a, b, c, d); }
    HRESULT STDMETHODCALLTYPE GetDeviceStatus(REFGUID a) override { return m_real->GetDeviceStatus(a); }
    HRESULT STDMETHODCALLTYPE RunControlPanel(HWND a, DWORD b) override { return m_real->RunControlPanel(a, b); }
    HRESULT STDMETHODCALLTYPE Initialize(HINSTANCE a, DWORD b) override { return m_real->Initialize(a, b); }
    HRESULT STDMETHODCALLTYPE FindDevice(REFGUID a, LPCSTR b, LPGUID c) override { return m_real->FindDevice(a, b, c); }
    HRESULT STDMETHODCALLTYPE EnumDevicesBySemantics(LPCSTR a, LPDIACTIONFORMATA b, LPDIENUMDEVICESBYSEMANTICSCBA c, LPVOID d, DWORD e) override { return m_real->EnumDevicesBySemantics(a, b, c, d, e); }
    HRESULT STDMETHODCALLTYPE ConfigureDevices(LPDICONFIGUREDEVICESCALLBACK a, LPDICONFIGUREDEVICESPARAMSA b, DWORD c, LPVOID d) override { return m_real->ConfigureDevices(a, b, c, d); }

private:
    IDirectInput8A* m_real;
    LONG m_ref;
};

} // namespace

// ---------------------------------------------------------------------------------------------
// Exports (dinput8.def)
// ---------------------------------------------------------------------------------------------
extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE hinst, DWORD version, REFIID riid, LPVOID* out, LPUNKNOWN outer)
{
    CreateFn real = reinterpret_cast<CreateFn>(realProc("DirectInput8Create"));
    if (!real)
        return DIERR_GENERIC;
    const HRESULT hr = real(hinst, version, riid, out, outer);
    startScript();
    if (SUCCEEDED(hr) && g_scripted && out && *out && riid == IID_IDirectInput8A)
    {
        *out = static_cast<IDirectInput8A*>(new ProxyInput(static_cast<IDirectInput8A*>(*out)));
        report("DirectInput8Create: IDirectInput8A wrapped (version 0x%lX)", version);
    }
    return hr;
}

extern "C" HRESULT WINAPI ProxyDllCanUnloadNow()
{
    VoidFn f = reinterpret_cast<VoidFn>(realProc("DllCanUnloadNow"));
    return f ? f() : S_FALSE;
}

extern "C" HRESULT WINAPI ProxyDllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* out)
{
    GetClassObjectFn f = reinterpret_cast<GetClassObjectFn>(realProc("DllGetClassObject"));
    return f ? f(clsid, riid, out) : CLASS_E_CLASSNOTAVAILABLE;
}

extern "C" HRESULT WINAPI ProxyDllRegisterServer()
{
    VoidFn f = reinterpret_cast<VoidFn>(realProc("DllRegisterServer"));
    return f ? f() : E_FAIL;
}

extern "C" HRESULT WINAPI ProxyDllUnregisterServer()
{
    VoidFn f = reinterpret_cast<VoidFn>(realProc("DllUnregisterServer"));
    return f ? f() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
