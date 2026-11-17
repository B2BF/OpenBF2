// [CHOSEN, not recovered] Application, window and input host for native clients (issue #36).
// CocoaHost.mm and SDLHost.cpp implement this contract with their platform's event source.
//
// Threading: every member must be called on the main thread, which is the thread that runs
// BF2::handleInGameWinMessages. Event callbacks are delivered synchronously from pumpEvents().

#ifndef PLATFORM_HOST_CLIENTHOST_H
#define PLATFORM_HOST_CLIENTHOST_H

#include "HostEvents.h"

#include <string>
#include <vector>

namespace dice {
namespace hfe {
#if defined(__APPLE__) && defined(OPENBF2_HOST_TEST_SEAMS)
namespace macos { class CocoaHostTestHooks; }
#endif
namespace host {

struct HostDisplayModeInfo
{
    int width;                      // pixels
    int height;
    int refreshHz;                  // 0 when the display does not report one (most LCD panels report 60)
    int bitsPerPixel;               // 32 for every mode macOS offers
};

enum HostCursorShape { HostCursorArrow = 0, HostCursorBusy = 1 };

// Window creation parameters, matching Window::create's arguments.
struct HostWindowDesc
{
    std::string title;
    int  width;                     // client (back-buffer) size in pixels
    int  height;
    int  x;                         // top-left of the window frame from the screen's top-left;
    int  y;                         //   Window::create passes 50/50 when the setting is -1
    bool fullscreen;                // a borderless window covering the main screen
    bool hideCursor;
};

class ClientHost
{
public:
    static ClientHost& instance();

    // Initializes the native application/event system. Safe to call more than once.
    bool initApplication();

    // Creates and activates the game window. False if one already exists or creation fails.
    bool createWindow(const HostWindowDesc& desc);
    // Closes and releases the window. Restores the cursor and the presentation options.
    void destroyWindow();
    bool hasWindow() const;

    // Borrowed renderer window: NSView* on macOS, SDL_Window* on Linux. Null without a window.
    void* rendererWindowHandle() const;

    // Renderable client dimensions. The backend translates native window coordinates to this
    // pixel space; fullscreen presentation may scale it to the display.
    int  clientWidth() const;
    int  clientHeight() const;

    void show(bool visible);
    bool isVisible() const;
    void minimize();
    void setTitle(const std::string& title);
    // Resizes the windowed client area, keeping the top-left corner.
    void resizeClient(int width, int height);
    // Switches between the windowed frame and the borderless full-screen window. The back-buffer
    // size stays width x height in both.
    void setFullscreen(bool fullscreen);
    bool isFullscreen() const;

    // True while the game window is the key window and the application is active.
    bool hasFocus() const;
    bool isAppActive() const;
    // How many times the application has lost activation (the WM_ACTIVATEAPP FALSE transitions
    // the listeners receive). The Metal D3D9 device compares it to detect the focus loss that
    // loses an exclusive full-screen device on Windows. Forced focus (scripted runs) never counts.
    unsigned int deactivationCount() const;

    // Drains every pending event without blocking, delivering callbacks. Returns true once a quit
    // was requested (close button, Cmd-Q, requestQuit), as WM_QUIT does for PeekMessage.
    bool pumpEvents();
    // Blocks for up to timeoutSeconds for an event, then drains. Returns as pumpEvents.
    bool waitEvents(double timeoutSeconds);
    // A user-initiated close: notifies onCloseRequested, then sets the quit flag.
    void requestQuit();
    // PostQuitMessage: sets the quit flag only.
    void postQuitMessage();
    bool quitRequested() const;

    void addListener(IHostEventListener* listener);
    void removeListener(IHostEventListener* listener);

    // Cursor. showCursor follows Win32 ShowCursor: it increments or decrements a display counter,
    // returns the new value, and the cursor is visible while the counter is >= 0.
    int  showCursor(bool show);
    // SetCursor(0) inside the client area (Window::wndProc's WM_SETCURSOR arm): hides the cursor
    // image while it is over the view.
    void setCursorHiddenInView(bool hidden);
    void setCursorShape(HostCursorShape shape);
    // Win32 cursor handles: cursorHandle answers a stable, non-null token for a shape (the
    // counterpart of a loaded HCURSOR), and setCursor follows SetCursor: a null handle hides the
    // cursor image over the view, a token shows its shape.
    static void* cursorHandle(HostCursorShape shape);
    // LoadCursorFromFileA: a stable token for the .ani/.cur/.ico at path, shown with its own image,
    // animation and hotspot while the pointer is over the view. Null, as LoadCursorFromFileA
    // answers, when the file cannot be read; the arrow's token when its contents cannot be decoded.
    // Tokens live for the rest of the process. [CHOSEN, not recovered]
    static void* loadCursorFromFile(const char* path);
    void setCursor(void* handle);
    // Relative mouse mode, the counterpart of an acquired exclusive DirectInput mouse:
    // the cursor is hidden, disassociated from the mouse (it stays put; deltas keep arriving)
    // and parked in the view's centre. Leaving the mode re-associates and restores visibility.
    void setRelativeMouseMode(bool enabled);
    bool relativeMouseMode() const;
    // True when onRawMouseMove supplies a separate unaccelerated stream. When false, devices
    // consume onMouseMove deltas; SDL delivers its relative motion there without a second stream.
    bool rawMouseAvailable() const;

    // Current HostModifier bits.
    unsigned int modifierFlags() const;
    // [CHOSEN, not recovered] VK_RETURN asynchronous state for SwiffPlayer:
    // 0x8000 while either Enter key is down, bit 0 for a press since last query.
    // Consumes bit 0; auto-repeat does not create another press.
    unsigned short consumeAsyncEnterState();
    // Whether the platform's Control shortcut modifier is held (Control or Command on macOS).
    bool controlShortcutDown() const;
    // Printable text for a DIK physical key in the current layout. Only Shift/Caps Lock apply.
    // Empty for unsupported keys or keys that produce no printable character.
    std::u16string keyCharacter(unsigned char dik, unsigned int modifiers = 0) const;
    // English fallback label with native modifier-key names; null for unnamed keys.
    const char* keyDisplayName(unsigned char dik) const;

    // Clipboard text as Windows-1252 bytes, the CF_TEXT counterpart for western code pages.
    bool setClipboardText(const std::string& text);
    bool getClipboardText(std::string& text) const;

    // Display modes of the main display (duplicates removed) and the current desktop mode.
    // macOS desktopMode reports the desktop's logical size in points (e.g. 1440x900 on a
    // 2880x1800 Retina panel), the size a Windows user would call the desktop resolution;
    // displayModes lists pixel sizes, which include the same sizes as low-resolution modes.
    std::vector<HostDisplayModeInfo> displayModes() const;
    HostDisplayModeInfo desktopMode() const;

    // Keeps the display awake while the game runs (the SPI_SETSCREENSAVEACTIVE counterpart).
    void preventDisplaySleep(bool prevent);

    // Opens a URL or file with the default application (ShellExecute "open").
    bool openURL(const std::string& url);

    // Monotonic backend milliseconds, truncated to 32 bits like GetTickCount. Input event times
    // use the same clock; callers must compare elapsed times with unsigned wraparound.
    static unsigned int tickCountMs();

private:
#if defined(__APPLE__) && defined(OPENBF2_HOST_TEST_SEAMS)
    friend class dice::hfe::macos::CocoaHostTestHooks;
#endif
    ClientHost();
    ~ClientHost();
    ClientHost(const ClientHost&);
    ClientHost& operator=(const ClientHost&);

public:
    struct Impl;                    // backend-owned state, defined in CocoaHost.mm or SDLHost.cpp
private:
    Impl* m_impl;
};

} // namespace host
} // namespace hfe
} // namespace dice

#endif // PLATFORM_HOST_CLIENTHOST_H
