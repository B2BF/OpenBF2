// Client-only Win32 game-window implementation. [INFERRED] source placement.

#include "BF2Window.h"

// [CHOSEN, not recovered] The macOS client uses BF2Window_native.cpp instead.
#if !DEDICATED && defined(_WIN32)

#include "BF2Engine.h"                              // menuLogic - the active menu (Window::wndProc)
#include "../../Core/EventManager.h"                // eventManager->postEvent on WM_ACTIVATEAPP
#include "../IEventListener.h"                      // EventCategory
#include "../Utility/Vars.h"                        // opt-window-x / opt-window-y
#include "../../Platform/Windows/WinInputScript.h"  // [TEMP] BF2_INPUT_SCRIPT focus
#include <cstring>

namespace dice {
namespace hfe {

Window* mainWindow = 0;                             // dword_9EB078

// Window - the base class. Bodies at 0x58DDC0..0x58E560 plus the four folded 4-byte getters.

// . Builds the handler list (0x58DE30, size 0), zeroes the handle, constructs the
// title, zeroes width/height and every flag except m_visible, and publishes `this`.
Window::Window()
    : m_hwnd(0)
    , m_width(0)
    , m_height(0)
    , m_fullscreen(false)                           // +0x38 = 0
    , m_hasFocus(false)                             // +0x39 = 0
    , m_visible(true)                               // +0x3A = 1
    , m_hideCursor(false)                           // +0x3B = 0
{
    mainWindow = this;
}

// . Resets the vptr (implicit here), hides and destroys the window if one exists -
// the handle is re-tested inside, as shipped - clears the global, destroys the title and the list.
Window::~Window()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_HIDE);
        if (m_hwnd)
        {
            PostQuitMessage(0);
            DestroyWindow(m_hwnd);
            m_hwnd = 0;
        }
    }
    mainWindow = 0;
}

// . The title doubles as the window class name. x / y == -1 mean 50. The `this`
// passed as lpParam is never read back: the wndproc finds the window through `mainWindow`.
bool Window::create(const std::string& title, int width, int height, bool fullscreen,
                    bool hideCursor, int iconId, int x, int y)
{
    if (m_hwnd)
        destroy();                                  // through the vtable (+0x1C)

    // SPI_SETFOREGROUNDLOCKTIMEOUT = 0, so the SetForegroundWindow below is honoured.
    SystemParametersInfoA(0x2001, 0, 0, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);

    m_hideCursor = hideCursor;                      // +0x3B
    m_title      = title;                           // +0x14

    const int icon = (iconId != -1) ? iconId : 32512;       // IDI_APPLICATION

    WNDCLASSEXA wc;
    memset(&wc, 0, sizeof(wc));
    wc.cbSize        = sizeof(WNDCLASSEXA);         // 48
    wc.style         = CS_DBLCLKS;                  // 8
    wc.lpfnWndProc   = wndProc;
    wc.cbClsExtra    = 0;
    wc.cbWndExtra    = 0;
    wc.hInstance     = GetModuleHandleA(0);
    wc.hIcon         = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(icon));
    wc.hIconSm       = 0;
    wc.hCursor       = LoadCursorA(0, IDC_ARROW);   // 0x7F00
    wc.hbrBackground = CreateSolidBrush(0);         // black
    wc.lpszClassName = m_title.c_str();
    if (!RegisterClassExA(&wc))
        return false;

    const int posX = (x != -1) ? x : 50;
    const int posY = (y != -1) ? y : 50;

    HWND hwnd;
    if (fullscreen)
    {
        hwnd = CreateWindowExA(WS_EX_TOPMOST, m_title.c_str(), m_title.c_str(),
                               WS_POPUP | WS_VISIBLE,
                               0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                               0, 0, GetModuleHandleA(0), this);
    }
    else
    {
        const int outerWidth  = width + 2 * GetSystemMetrics(SM_CXFRAME);                               // 32
        const int cyFrame     = GetSystemMetrics(SM_CYFRAME);                                           // 33
        const int outerHeight = height + GetSystemMetrics(SM_CYCAPTION) + 2 * cyFrame;                  // 4
        hwnd = CreateWindowExA(0, m_title.c_str(), m_title.c_str(),
                               WS_OVERLAPPEDWINDOW,
                               posX, posY, outerWidth, outerHeight,
                               0, 0, GetModuleHandleA(0), this);
    }
    m_hwnd = hwnd;                                  // stored before the null test, as shipped
    if (!hwnd)
        return false;

    if (m_hideCursor)
    {
        SetCursor(0);
        ShowCursor(FALSE);
    }
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    UpdateWindow(m_hwnd);
    return true;
}

bool               Window::isCreated() const      { return m_hwnd != 0; }
// [TEMP] [CHOSEN, not recovered] A BF2_INPUT_SCRIPT run reports focus (WinInputScript.h).
// [RETIRE-WHEN: client-test-automation-replaced]
bool               Window::getHasFocus() const    { return m_hasFocus || inputscript::winInputScriptActive(); }
const std::string& Window::getTitle() const       { return m_title; }
int                Window::getWidth() const       { return m_width; }
int                Window::getHeight() const      { return m_height; }
bool               Window::unresolved18() const   { return true; }          //  (folded constant)
bool               Window::getVisible() const     { return m_visible; }
bool               Window::getFullscreen() const  { return m_fullscreen; }
HWND               Window::getHwnd() const        { return m_hwnd; }

void Window::destroy()
{
    if (m_hwnd)
    {
        PostQuitMessage(0);
        DestroyWindow(m_hwnd);
        m_hwnd = 0;
    }
}

// . The flag is written whether or not a window exists.
void Window::show(bool visible)
{
    m_visible = visible;
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
        if (m_visible)
            SetFocus(m_hwnd);
    }
}

// : an insert at the list's end (0x58DEC0) and a size bump. 0x58E490, 19 B: the
// 77-byte erase-by-value helper at 0x72FC40. [INFERRED] that the helper removes one entry; a
// std::list::remove drops every equal one, which only differs if a handler was added twice.
void Window::addMessageHandler(MessageHandler handler)      { m_handlers.push_back(handler); }
void Window::removeMessageHandler(MessageHandler handler)   { m_handlers.remove(handler); }

// . `handled` starts true and is cleared on the fall-through path; only the messages
// that jump straight to the dispatch keep it. Every registered handler is called regardless of the
// message, and its return value is ignored (the call at 0x58E098 discards eax).
LRESULT CALLBACK Window::wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!hwnd)
        return 0;

    Window* w = mainWindow;                         // dword_9EB078 - never GWL_USERDATA
    bool handled = true;

    if (message > WM_ACTIVATEAPP)                   // 0x1C
    {
        switch (message)
        {
        case WM_SETCURSOR:                          // 0x20
            if (!menuLogic && w->m_hideCursor)
                SetCursor(0);
            goto dispatch;

        case WM_NCHITTEST:                          // 0x84 - the only message that skips the handlers
            return HTCLIENT;                        // 1

        case WM_SYSKEYDOWN:                         // 0x104
            if (wParam == VK_F10)                   // F10 would open the system menu; swallowed
                goto dispatch;
            break;

        case WM_COMMAND:                            // 0x111
            if (wParam != 2)                        // idcancel
                break;
            DestroyWindow(hwnd);
            goto dispatch;

        case WM_SYSCOMMAND:                         // 0x112
            if (wParam <= SC_MAXIMIZE)              // 0xF030
            {
                if (wParam != SC_MAXIMIZE && wParam != SC_SIZE)       // 0xF000
                {
                    if (wParam != SC_MOVE)                            // 0xF010
                        break;
                    if (!w->m_fullscreen)           // windowed: let Windows move it
                        return DefWindowProcA(hwnd, message, SC_MOVE, lParam);
                }
                goto dispatch;                      // maximize, size, and a fullscreen move: swallowed
            }
            if (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER)     // 0xF140, 0xF170
                goto dispatch;                      // swallowed
            break;

        default:
            break;
        }
    }
    else if (message == WM_ACTIVATEAPP)             // 0x1C
    {
        w->m_hasFocus = (wParam != 0);
        if (w->m_fullscreen)
        {
            bool activated = (wParam != 0);         // hibyte(wParam) on the stack, passed by address
            eventManager->postEvent(ECCore, 15, &activated, 0.0f);   // PE +0x10: the id/data overload
        }
    }
    else
    {
        switch (message)
        {
        case WM_DESTROY:                            // 2
            PostQuitMessage(0);
            goto dispatch;

        case WM_SIZE:                               // 5
            w->m_width  = LOWORD(lParam);           //  `movzx edx, cx` then the +0x30 store
            w->m_height = HIWORD(lParam);           // `shr ecx, 10h` then the +0x34 store
            if (wParam == SIZE_MAXHIDE || wParam == SIZE_MINIMIZED)   // 4, 1
                break;
            w->m_hasFocus = true;
            break;

        case WM_ACTIVATE:                           // 6
            // The whole wParam is compared with WA_ACTIVE, so WA_CLICKACTIVE (2) reads as
            // "not focused" until the WM_SETFOCUS that follows it. Reproduced.
            w->m_hasFocus = (wParam == WA_ACTIVE);
            break;

        case WM_SETFOCUS:                           // 7
            w->m_hasFocus = true;
            break;

        case WM_KILLFOCUS:                          // 8
            w->m_hasFocus = false;
            break;

        default:
            break;
        }
    }
    handled = false;

dispatch:
    for (std::list<MessageHandler>::iterator it = w->m_handlers.begin(); it != w->m_handlers.end(); ++it)
        (*it)(hwnd, message, wParam, lParam);       // return value ignored, as shipped

    return handled ? 0 : DefWindowProcA(hwnd, message, wParam, lParam);
}

// BF2Window - the derived class. Bodies at 0x45D770..0x45D8B0.

BF2Window::BF2Window()                              // Window() then the vptr
    : Window()
{
}

BF2Window::~BF2Window()                             // vptr, Window::~Window, free if (flags & 1)
{
}

// . y is read before x, and create() is called directly (a non-virtual call to the
// base body), not through the vtable.
bool BF2Window::createFromSettings(const std::string& title, int width, int height,
                                   bool fullscreen, bool hideCursor, int iconId)
{
    const int y = *Vars::getInt("opt-window-y", -1);
    const int x = *Vars::getInt("opt-window-x", -1);
    return Window::create(title, width, height, fullscreen, hideCursor, iconId, x, y);
}

void BF2Window::setHwnd(HWND hwnd)                  //  (folded)
{
    m_hwnd = hwnd;
}

// . Client size -> outer size with the frame metrics, keeping the position.
void BF2Window::resizeClient(int width, int height)
{
    const int outerWidth  = width + 2 * GetSystemMetrics(SM_CXFRAME);           // 32
    const int cyFrame     = GetSystemMetrics(SM_CYFRAME);                       // 33
    const int outerHeight = height + GetSystemMetrics(SM_CYCAPTION) + 2 * cyFrame;   // 4
    SetWindowPos(m_hwnd, 0, 0, 0, outerWidth, outerHeight, SWP_NOMOVE);        // 2
}

// . Reads the current flag to pick the other style, then flips the flag last.
void BF2Window::toggleFullscreenStyle()
{
    if (m_fullscreen)
    {
        SetWindowLongA(m_hwnd, GWL_EXSTYLE, 0);
        SetWindowLongA(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
    }
    else
    {
        SetWindowLongA(m_hwnd, GWL_EXSTYLE, WS_EX_TOPMOST);                    // 8
        SetWindowLongA(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    }
    ShowWindow(m_hwnd, SW_SHOW);
    SetForegroundWindow(m_hwnd);
    UpdateWindow(m_hwnd);
    m_fullscreen = !m_fullscreen;
}

} // namespace hfe
} // namespace dice

#endif // !dedicated && _WIN32
