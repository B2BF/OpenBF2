// [CHOSEN, not recovered] macOS port: Window and BF2Window on top of the Cocoa host
// (Platform/macOS/CocoaHost.h). There was no retail macOS image. The class declarations, virtual
// order and member state are BF2Window.h's; each method does what its Win32 body in
// BF2Window.cpp does, with the Win32 call replaced by the host's counterpart. Cocoa events are
// turned into the Win32 messages the engine's handlers inspect and run through Window::wndProc,
// which keeps the Win32 wndProc's state rules and dispatches every message to the handler list.
// Choices that differ from Win32 are listed in docs/macos-s2-4.md.

#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))

#include "BF2Window.h"

#include "BF2Engine.h"                              // menuLogic - the active menu (wndProc)
#include "../../Core/EventManager.h"                // eventManager->postEvent on WM_ACTIVATEAPP
#include "../IEventListener.h"                      // EventCategory
#include "../Utility/Vars.h"                        // opt-window-x / opt-window-y
#include "../../Platform/macOS/CocoaHost.h"
#include "../../Platform/macOS/MacKeyMap.h"

namespace dice {
namespace hfe {

using namespace hostmsg;

Window* mainWindow = 0;

namespace {

macos::CocoaHost& host() { return macos::CocoaHost::instance(); }

// Windows-1252 byte for a UTF-16 unit, or -1 when the code page has none (WM_CHAR delivers
// ANSI characters to this narrow-character window class).
int cp1252FromUtf16(unsigned short c)
{
    if (c < 0x80 || (c >= 0xA0 && c <= 0xFF))
        return c;
    static const unsigned short high[32] = {
        0x20AC, 0,      0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
        0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0,      0x017D, 0,
        0,      0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0,      0x017E, 0x0178 };
    for (int i = 0; i < 32; ++i)
        if (high[i] == c)
            return 0x80 + i;
    return -1;
}

// Translates host events into Win32 messages for mainWindow, the way the Win32 wndProc finds its
// window through mainWindow rather than through the handle.
class WindowMessageSource : public macos::IHostEventListener
{
public:
    void send(UINT message, WPARAM wParam, LPARAM lParam)
    {
        if (mainWindow)
            Window::wndProc(mainWindow->getHwnd(), message, wParam, lParam);
    }

    void onAppActivationChanged(bool appActive) override
    {
        send(WM_ACTIVATEAPP, appActive ? 1 : 0, 0);
    }

    void onFocusChanged(bool windowKey, bool appActive) override
    {
        const bool focused = windowKey && appActive;
        if (focused == m_focused)
            return;
        m_focused = focused;
        send(WM_ACTIVATE, focused ? WA_ACTIVE : WA_INACTIVE, 0);
        send(focused ? WM_SETFOCUS : WM_KILLFOCUS, 0, 0);
    }

    void onResize(int width, int height, bool minimized) override
    {
        send(WM_SIZE, minimized ? SIZE_MINIMIZED : SIZE_RESTORED, makeLParam(width, height));
    }

    // The close button and Cmd-Q: WM_CLOSE, whose default handling destroys the window
    // (WM_DESTROY) and posts the quit message.
    void onCloseRequested() override
    {
        send(WM_CLOSE, 0, 0);
        send(WM_DESTROY, 0, 0);
    }

    // WM_KEYDOWN/WM_KEYUP with the US-layout virtual key and the scan code in lParam bits 16-24.
    // Option is Alt, so Option combinations and F10 arrive as WM_SYSKEY* as they do on Windows.
    void onKey(const macos::HostKeyEvent& event) override
    {
        const unsigned char dik = macos::dikFromMacKeyCode(event.keyCode, event.isoLayout);
        const unsigned int vk = macos::win32VirtualKeyFromDik(dik);
        if (!vk)
            return;
        const bool alt = (event.modifiers & macos::HostModOption) != 0;
        const bool sys = alt || vk == VK_F10;
        std::uint32_t lParam = 1u | (static_cast<std::uint32_t>(dik & 0x7F) << 16);
        if (dik & 0x80) lParam |= 1u << 24;                 // extended key
        if (alt)        lParam |= 1u << 29;                 // context code
        if (event.repeat || !event.down) lParam |= 1u << 30;
        if (!event.down) lParam |= 1u << 31;
        const UINT message = event.down ? (sys ? WM_SYSKEYDOWN : WM_KEYDOWN)
                                        : (sys ? WM_SYSKEYUP : WM_KEYUP);
        send(message, vk, static_cast<LPARAM>(lParam));
    }

    // WM_CHAR. Backspace reports DEL (0x7F) on macOS and keypad Enter ETX (0x03); Windows sends
    // BS and CR. [CHOSEN] Command+letter is delivered as the control character, so the Mac
    // Command-C/V/X shortcuts reach the engine's Ctrl-C/V/X handling (the handler's Ctrl test
    // treats Command as Ctrl, see BF2.cpp).
    void onText(const macos::HostTextEvent& event) override
    {
        for (unsigned short c : event.characters)
        {
            if (event.keyCode == 0x33 && c == 0x7F) c = 0x08;
            else if (c == 0x03) c = 0x0D;
            else if (c == 0x7F) continue;
            if ((event.modifiers & macos::HostModCommand) && ((c | 0x20) >= 'a' && (c | 0x20) <= 'z'))
                c = static_cast<unsigned short>(c & 0x1F);
            const int ansi = cp1252FromUtf16(c);
            if (ansi < 0)
                continue;
            send(WM_CHAR, static_cast<WPARAM>(ansi), 1);
        }
    }

    // Windows sends WM_SETCURSOR, then WM_MOUSEMOVE, while the pointer is over the client area.
    // While the mouse is captured (the exclusively acquired DirectInput mouse in-game) the system
    // cursor does not move and Windows sends no mouse messages at all; the menu handlers
    // (bf2MessageHandler -> MainMenuLogic::onMouse*, which queue into the SwiffPlayer) must not see
    // in-game motion or clicks.
    void onMouseMove(const macos::HostMouseMoveEvent& event) override
    {
        if (!event.insideView || event.captured)
            return;
        send(WM_SETCURSOR, 0, 1);                               // HTCLIENT
        send(WM_MOUSEMOVE, 0, makeLParam(static_cast<int>(event.x), static_cast<int>(event.y)));
    }

    // CS_DBLCLKS: the second press of a double click arrives as WM_LBUTTONDBLCLK.
    void onMouseButton(const macos::HostMouseButtonEvent& event) override
    {
        if (event.captured)
            return;
        const LPARAM where = makeLParam(static_cast<int>(event.x), static_cast<int>(event.y));
        switch (event.button)
        {
        case 0: send(event.down ? (event.clickCount == 2 ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN) : WM_LBUTTONUP, 0, where); break;
        case 1: send(event.down ? WM_RBUTTONDOWN : WM_RBUTTONUP, 0, where); break;
        case 2: send(event.down ? WM_MBUTTONDOWN : WM_MBUTTONUP, 0, where); break;
        default: break;
        }
    }

    void onMouseWheel(const macos::HostMouseWheelEvent& event) override
    {
        if (event.captured)
            return;
        send(WM_MOUSEWHEEL, static_cast<WPARAM>(static_cast<std::uint32_t>(event.delta & 0xFFFF) << 16), 0);
    }

    void resetFocus() { m_focused = false; }

private:
    bool m_focused = false;
};

WindowMessageSource s_messageSource;

} // namespace

Window::Window()
    : m_hwnd(0)
    , m_width(0)
    , m_height(0)
    , m_fullscreen(false)
    , m_hasFocus(false)
    , m_visible(true)
    , m_hideCursor(false)
{
    mainWindow = this;
}

// Win32: hides and destroys the window (PostQuitMessage, DestroyWindow), then clears the global.
Window::~Window()
{
    if (m_hwnd)
    {
        host().show(false);
        if (m_hwnd)
        {
            host().postQuitMessage();
            wndProc(m_hwnd, WM_DESTROY, 0, 0);
            host().removeListener(&s_messageSource);
            host().destroyWindow();
            m_hwnd = 0;
        }
    }
    mainWindow = 0;
}

// Win32 registers a class named after the title and creates a WS_POPUP (fullscreen) or
// WS_OVERLAPPEDWINDOW window at (x, y), -1 meaning 50. On macOS the host creates the NSWindow;
// m_fullscreen stays false here, as in Win32, until toggleFullscreenStyle flips it.
bool Window::create(const std::string& title, int width, int height, bool fullscreen,
                    bool hideCursor, int /*iconId*/, int x, int y)
{
    if (m_hwnd)
        destroy();

    m_hideCursor = hideCursor;
    m_title      = title;

    macos::HostWindowDesc desc;
    desc.title      = m_title;
    desc.width      = width;
    desc.height     = height;
    desc.x          = (x != -1) ? x : 50;
    desc.y          = (y != -1) ? y : 50;
    desc.fullscreen = fullscreen;
    desc.hideCursor = false;                        // the WM_SETCURSOR arm decides, as on Win32

    host().addListener(&s_messageSource);
    s_messageSource.resetFocus();
    const bool created = host().createWindow(desc);
    m_hwnd = created ? reinterpret_cast<HWND>(host().nativeView()) : 0;
    if (!m_hwnd)
    {
        host().removeListener(&s_messageSource);
        return false;
    }

    // CreateWindowEx/ShowWindow deliver WM_SIZE with the client size.
    s_messageSource.onResize(host().clientWidth(), host().clientHeight(), false);
    s_messageSource.onFocusChanged(host().hasFocus(), host().isAppActive());

    if (m_hideCursor)
    {
        host().setCursorHiddenInView(true);         // SetCursor(0)
        host().showCursor(false);                   // ShowCursor(FALSE)
    }
    host().show(true);                              // ShowWindow, SetForegroundWindow, UpdateWindow
    return true;
}

bool               Window::isCreated() const      { return m_hwnd != 0; }
bool               Window::getHasFocus() const    { return m_hasFocus; }
const std::string& Window::getTitle() const       { return m_title; }
int                Window::getWidth() const       { return m_width; }
int                Window::getHeight() const      { return m_height; }
bool               Window::unresolved18() const   { return true; }
bool               Window::getVisible() const     { return m_visible; }
bool               Window::getFullscreen() const  { return m_fullscreen; }
HWND               Window::getHwnd() const        { return m_hwnd; }

// Win32: PostQuitMessage; DestroyWindow (which sends WM_DESTROY); clear the handle.
void Window::destroy()
{
    if (m_hwnd)
    {
        host().postQuitMessage();
        wndProc(m_hwnd, WM_DESTROY, 0, 0);
        host().removeListener(&s_messageSource);
        host().destroyWindow();
        m_hwnd = 0;
    }
}

// The flag is written whether or not a window exists; showing also focuses (SetFocus).
void Window::show(bool visible)
{
    m_visible = visible;
    if (m_hwnd)
        host().show(visible);
}

void Window::addMessageHandler(MessageHandler handler)      { m_handlers.push_back(handler); }
void Window::removeMessageHandler(MessageHandler handler)   { m_handlers.remove(handler); }

// The Win32 wndProc's state rules for the messages macOS produces, then the handler dispatch.
// The Win32-only arms (WM_NCHITTEST, WM_SYSCOMMAND, WM_COMMAND, DefWindowProc) have no macOS
// counterpart: the host window has no resize border, no system menu and no default processing.
LRESULT Window::wndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (!hwnd)
        return 0;

    Window* w = mainWindow;
    if (!w)
        return 0;

    switch (message)
    {
    case WM_SETCURSOR:
        // Win32 calls SetCursor(0) only in-game; with a menu up the class/menu cursor shows.
        host().setCursorHiddenInView(!menuLogic && w->m_hideCursor);
        break;

    case WM_ACTIVATEAPP:
        w->m_hasFocus = (wParam != 0);
        if (w->m_fullscreen)
        {
            bool activated = (wParam != 0);
            eventManager->postEvent(ECCore, 15, &activated, 0.0f);
        }
        break;

    case WM_DESTROY:
        host().postQuitMessage();
        break;

    case WM_SIZE:
        w->m_width  = static_cast<int>(lParam & 0xFFFF);
        w->m_height = static_cast<int>((lParam >> 16) & 0xFFFF);
        if (wParam != SIZE_MINIMIZED)
            w->m_hasFocus = true;                   // the Win32 quirk: any restore reads as focus
        break;

    case WM_ACTIVATE:
        w->m_hasFocus = (wParam == WA_ACTIVE);
        break;

    case WM_SETFOCUS:
        w->m_hasFocus = true;
        break;

    case WM_KILLFOCUS:
        w->m_hasFocus = false;
        break;

    default:
        break;
    }

    for (std::list<MessageHandler>::iterator it = w->m_handlers.begin(); it != w->m_handlers.end(); ++it)
        (*it)(hwnd, message, wParam, lParam);
    return 0;
}

BF2Window::BF2Window()
    : Window()
{
}

BF2Window::~BF2Window()
{
}

bool BF2Window::createFromSettings(const std::string& title, int width, int height,
                                   bool fullscreen, bool hideCursor, int iconId)
{
    const int y = *Vars::getInt("opt-window-y", -1);
    const int x = *Vars::getInt("opt-window-x", -1);
    return Window::create(title, width, height, fullscreen, hideCursor, iconId, x, y);
}

void BF2Window::setHwnd(HWND hwnd)
{
    m_hwnd = hwnd;
}

// Win32 resizes the outer frame to fit the client size; the host keeps the top-left corner and
// reports the new size back as WM_SIZE.
void BF2Window::resizeClient(int width, int height)
{
    host().resizeClient(width, height);
}

// Swaps the windowed and full-screen styles, shows and foregrounds, then flips the flag last.
void BF2Window::toggleFullscreenStyle()
{
    host().setFullscreen(!m_fullscreen);
    host().show(true);
    m_fullscreen = !m_fullscreen;
}

} // namespace hfe
} // namespace dice

#endif // !DEDICATED && __APPLE__
