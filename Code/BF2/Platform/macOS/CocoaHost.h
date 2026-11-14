// [CHOSEN, not recovered] macOS port: the Cocoa application and window host. It stands where
// WinMain's message pump, the Win32 window class and the DirectInput event source stood on
// Windows. The header is plain C++ (no Objective-C types) so engine translation units can use it;
// the implementation is CocoaHost.mm. Design and integration notes: docs/macos-s2-4.md.
//
// Threading: every member must be called on the main thread, which is the thread that runs
// BF2::handleInGameWinMessages. Event callbacks are delivered synchronously from pumpEvents().

#ifndef PLATFORM_MACOS_COCOAHOST_H
#define PLATFORM_MACOS_COCOAHOST_H

#include <string>
#include <vector>

namespace dice {
namespace hfe {
namespace macos {

// NSEventModifierFlags bits the host reports, plus the device-dependent left/right bits.
enum HostModifier
{
    HostModCapsLock     = 1u << 16,
    HostModShift        = 1u << 17,
    HostModControl      = 1u << 18,
    HostModOption       = 1u << 19,
    HostModCommand      = 1u << 20,
    HostModNumericPad   = 1u << 21,
    HostModFunction     = 1u << 23,
    // Device-dependent bits (IOKit NX_DEVICE*KEYMASK) in the low word.
    HostModLeftControl  = 0x0001,
    HostModLeftShift    = 0x0002,
    HostModRightShift   = 0x0004,
    HostModLeftCommand  = 0x0008,
    HostModRightCommand = 0x0010,
    HostModLeftOption   = 0x0020,
    HostModRightOption  = 0x0040,
    HostModRightControl = 0x2000
};

// Key transition from the hardware. Auto-repeat keyDown events are reported with repeat set;
// modifier keys arrive as press/release pairs derived from flagsChanged events. Caps Lock is a
// toggle on macOS: every change of its lock state is reported as a press, and the matching
// release is delivered at the start of the next pumpEvents call, so the two land in different
// input samples.
struct HostKeyEvent
{
    unsigned short keyCode;         // kVK_ code
    bool           down;
    bool           repeat;
    bool           isoLayout;       // the keyboard that produced the event is an ISO keyboard
    unsigned int   modifiers;       // HostModifier bits after the event
    unsigned int   timeMs;          // HostClock milliseconds (see tickCountMs)
};

// Text produced by a key press, in UTF-16 units, as NSEvent.characters reports it. Function-key
// private-use characters (U+F700..U+F8FF) are removed.
struct HostTextEvent
{
    unsigned short        keyCode;
    unsigned int          modifiers;
    std::vector<unsigned short> characters;
    bool                  repeat;
};

// Pointer motion. dx/dy are the relative device deltas in points (x right, y down), present
// also while the cursor is disassociated. x/y are the cursor position in back-buffer pixels,
// origin top left, valid when insideView is true.
// captured (in this and the two events below) is true while the host is in relative mouse mode
// with focus, the counterpart of an exclusively acquired DirectInput mouse. Windows generates no
// window mouse messages then (the system cursor does not move), so the window-message source
// drops captured events; the input devices use them.
struct HostMouseMoveEvent
{
    float        dx, dy;
    float        x, y;
    bool         insideView;
    bool         captured;
    unsigned int timeMs;
};

// Button transition. button uses NSEvent.buttonNumber, which is DirectInput's numbering:
// 0 left, 1 right, 2 middle, 3 and up the extra buttons.
struct HostMouseButtonEvent
{
    int          button;
    bool         down;
    int          clickCount;
    float        x, y;
    bool         captured;
    unsigned int timeMs;
};

// Wheel motion in DirectInput units: whole notches of +120 away from the user, independent of the
// "natural scrolling" setting, as a DirectInput wheel reports them. Trackpad scrolling counts
// 10 points as a notch (12 units per point) and is reported once a whole notch has accumulated;
// the momentum (inertia) phase AppKit synthesizes after the fingers lift is not device motion and
// is not reported.
struct HostMouseWheelEvent
{
    int          delta;
    bool         captured;
    unsigned int timeMs;
};

// Unaccelerated motion from GCMouse devices (MacRawMouse), in device counts, x right, y down,
// delivered in pumpEvents while the window has focus. Raw motion never becomes a window message.
struct HostRawMouseEvent
{
    float        dx, dy;
    unsigned int timeMs;
};

// Receives host events. All callbacks are optional.
class IHostEventListener
{
public:
    virtual void onKey(const HostKeyEvent&) {}
    virtual void onText(const HostTextEvent&) {}
    virtual void onMouseMove(const HostMouseMoveEvent&) {}
    virtual void onMouseButton(const HostMouseButtonEvent&) {}
    virtual void onMouseWheel(const HostMouseWheelEvent&) {}
    virtual void onRawMouseMove(const HostRawMouseEvent&) {}
    // windowKey: the game window is the key window; appActive: the application is frontmost.
    virtual void onFocusChanged(bool windowKey, bool appActive) { (void)windowKey; (void)appActive; }
    virtual void onAppActivationChanged(bool appActive) { (void)appActive; }
    // Back-buffer size changed, or the window was minimized/restored.
    virtual void onResize(int width, int height, bool minimized) { (void)width; (void)height; (void)minimized; }
    // The user asked to close the window or quit (close button, Cmd-Q, Dock menu).
    virtual void onCloseRequested() {}
protected:
    ~IHostEventListener() {}
};

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

class CocoaHost
{
public:
    static CocoaHost& instance();

    // Creates NSApplication, sets the regular activation policy, installs the application menu
    // (Quit, Hide, fullscreen toggle) and finishes launching. Safe to call more than once.
    bool initApplication();

    // Creates the game window with a CAMetalLayer-backed content view, shows it and activates the
    // application. Returns false if a window already exists or creation failed.
    bool createWindow(const HostWindowDesc& desc);
    // Closes and releases the window. Restores the cursor and the presentation options.
    void destroyWindow();
    bool hasWindow() const;

    // Native handles for the renderer: the NSWindow*, the content NSView* and its CAMetalLayer*,
    // all borrowed (owned by the host). Null without a window.
    void* nativeWindow() const;
    void* nativeView() const;
    void* metalLayer() const;

    // Back-buffer size: the CAMetalLayer drawableSize. Windowed it equals the view size in points
    // (the layer's contentsScale is 1); fullscreen it is the requested game resolution, scaled to
    // the screen by the compositor.
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
    // True while at least one GCMouse device is connected: relative motion then comes from
    // onRawMouseMove (unaccelerated), and the NSEvent deltas of onMouseMove are only a fallback.
    bool rawMouseAvailable() const;

    // Current HostModifier bits.
    unsigned int modifierFlags() const;
    // [CHOSEN, not recovered] VK_RETURN asynchronous state for SwiffPlayer:
    // 0x8000 while either Enter key is down, bit 0 for a press since last query.
    // Consumes bit 0; auto-repeat does not create another press.
    unsigned short consumeAsyncEnterState();
    // True when the most recently used keyboard is an ISO keyboard
    // (KBGetLayoutType(LMGetKbdType()) == 'ISO '). Key events carry their own keyboard's answer.
    bool isISOKeyboard() const;
    // UCKeyTranslate of a key code with the current keyboard layout. modifiers holds HostModifier
    // bits; only HostModShift and HostModCapsLock are applied (none by default). Returns an empty
    // string for keys that produce no printable character.
    std::u16string keyCharacter(unsigned short keyCode, unsigned int modifiers = 0) const;

    // Clipboard text as Windows-1252 bytes, the CF_TEXT counterpart for western code pages.
    bool setClipboardText(const std::string& text);
    bool getClipboardText(std::string& text) const;

    // Display modes of the main display (duplicates removed) and the current desktop mode.
    // [PLATFORM] desktopMode reports the desktop's logical size in points (e.g. 1440x900 on a
    // 2880x1800 Retina panel), the size a Windows user would call the desktop resolution;
    // displayModes lists pixel sizes, which include the same sizes as low-resolution modes.
    std::vector<HostDisplayModeInfo> displayModes() const;
    HostDisplayModeInfo desktopMode() const;

    // Keeps the display awake while the game runs (the SPI_SETSCREENSAVEACTIVE counterpart).
    void preventDisplaySleep(bool prevent);

    // Opens a URL or file with the default application (ShellExecute "open").
    bool openURL(const std::string& url);

    // Milliseconds on the clock NSEvent timestamps use (system uptime), truncated to 32 bits like
    // GetTickCount. Input event times are on the same clock.
    static unsigned int tickCountMs();

#if defined(OPENBF2_HOST_TEST_SEAMS)
    // [TEMP] Test-only: report the given focus state as if AppKit had delivered it, for runs in
    // sessions where the application cannot be activated (locked screen, no console user).
    // Defined only by Code/Tests/macos_host. [RETIRE-WHEN: macos-client-window]
    void overrideFocusForTest(bool windowKey, bool appActive);
    // [TEMP] Test-only (Code/Tests/macos_input): run without a window. Focus is what
    // overrideFocusForTest reports, events injected with injectEventForTest are routed as
    // pumpEvents routes them (pointer positions are taken from locationInWindow and count as
    // inside the view), and the cursor rule is evaluated without touching the real cursor
    // (cursorStateForTest reports it). [RETIRE-WHEN: macos-client-window]
    void enableHeadlessForTest();
    void injectEventForTest(void* nsEvent);
    void cursorStateForTest(bool& hidden, bool& disassociated) const;
    // Delivers the raw mouse samples published so far, as pumpEvents does after its events.
    void pumpRawMouseForTest();
#endif

private:
    CocoaHost();
    ~CocoaHost();
    CocoaHost(const CocoaHost&);
    CocoaHost& operator=(const CocoaHost&);

public:
    struct Impl;                    // Objective-C++ state, CocoaHost.mm
private:
    Impl* m_impl;
};

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // PLATFORM_MACOS_COCOAHOST_H
