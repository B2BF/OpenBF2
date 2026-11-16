// [CHOSEN, not recovered] Window and BF2Window on the shared native ClientHost (issue #36).
// The class declarations, virtual
// order and member state are BF2Window.h's; each method does what its Win32 body in
// BF2Window.cpp does, with the Win32 call replaced by the host's counterpart. Host events are
// turned into the Win32 messages the engine's handlers inspect and run through Window::wndProc,
// which keeps the Win32 wndProc's state rules and dispatches every message to the handler list.
// Choices that differ from Win32 are listed in docs/macos-s2-4.md.

#if !DEDICATED && (defined(__APPLE__) || defined(BF2_LINUX_CLIENT))

#include "BF2Window.h"

#include "BF2Engine.h"                              // menuLogic - the active menu (wndProc)
#include "../../Core/EventManager.h"                // eventManager->postEvent on WM_ACTIVATEAPP
#include "../IEventListener.h"                      // EventCategory
#include "../Utility/Vars.h"                        // opt-window-x / opt-window-y
#include "../../Platform/Host/ClientHost.h"
#include "../../Platform/Host/WindowMessageSource.h"

namespace dice {
namespace hfe {

using namespace hostmsg;

Window* mainWindow = 0;

namespace {

host::ClientHost& clientHost() { return host::ClientHost::instance(); }

// Routes the shared message translation to the current engine window.
class WindowMessageSource : public host::WindowMessageSource
{
    void send(UINT message, WPARAM wParam, LPARAM lParam) override
    {
        if (mainWindow)
            Window::wndProc(mainWindow->getHwnd(), message, wParam, lParam);
    }
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
        clientHost().show(false);
        if (m_hwnd)
        {
            clientHost().postQuitMessage();
            wndProc(m_hwnd, WM_DESTROY, 0, 0);
            clientHost().removeListener(&s_messageSource);
            clientHost().destroyWindow();
            m_hwnd = 0;
        }
    }
    mainWindow = 0;
}

// Win32 registers a class named after the title and creates a WS_POPUP (fullscreen) or
// WS_OVERLAPPEDWINDOW window at (x, y), -1 meaning 50. The host creates the native window;
// m_fullscreen stays false here, as in Win32, until toggleFullscreenStyle flips it.
bool Window::create(const std::string& title, int width, int height, bool fullscreen,
                    bool hideCursor, int /*iconId*/, int x, int y)
{
    if (m_hwnd)
        destroy();

    m_hideCursor = hideCursor;
    m_title      = title;

    host::HostWindowDesc desc;
    desc.title      = m_title;
    desc.width      = width;
    desc.height     = height;
    desc.x          = (x != -1) ? x : 50;
    desc.y          = (y != -1) ? y : 50;
    desc.fullscreen = fullscreen;
    desc.hideCursor = false;                        // the WM_SETCURSOR arm decides, as on Win32

    clientHost().addListener(&s_messageSource);
    s_messageSource.resetFocus();
    const bool created = clientHost().createWindow(desc);
    m_hwnd = created ? reinterpret_cast<HWND>(clientHost().rendererWindowHandle()) : 0;
    if (!m_hwnd)
    {
        clientHost().removeListener(&s_messageSource);
        return false;
    }

    // CreateWindowEx/ShowWindow deliver WM_SIZE with the client size.
    s_messageSource.onResize(clientHost().clientWidth(), clientHost().clientHeight(), false);
    s_messageSource.onFocusChanged(clientHost().hasFocus(), clientHost().isAppActive());

    if (m_hideCursor)
    {
        clientHost().setCursorHiddenInView(true);         // SetCursor(0)
        clientHost().showCursor(false);                   // ShowCursor(FALSE)
    }
    clientHost().show(true);                              // ShowWindow, SetForegroundWindow, UpdateWindow
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
        clientHost().postQuitMessage();
        wndProc(m_hwnd, WM_DESTROY, 0, 0);
        clientHost().removeListener(&s_messageSource);
        clientHost().destroyWindow();
        m_hwnd = 0;
    }
}

// The flag is written whether or not a window exists; showing also focuses (SetFocus).
void Window::show(bool visible)
{
    m_visible = visible;
    if (m_hwnd)
        clientHost().show(visible);
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
        clientHost().setCursorHiddenInView(!menuLogic && w->m_hideCursor);
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
        clientHost().postQuitMessage();
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
    clientHost().resizeClient(width, height);
}

// Swaps the windowed and full-screen styles, shows and foregrounds, then flips the flag last.
void BF2Window::toggleFullscreenStyle()
{
    clientHost().setFullscreen(!m_fullscreen);
    clientHost().show(true);
    m_fullscreen = !m_fullscreen;
}

} // namespace hfe
} // namespace dice

#endif // !DEDICATED && __APPLE__
