// [CHOSEN, not recovered] Native client input events shared by Cocoa and SDL (issue #36).
#ifndef PLATFORM_HOST_HOSTEVENTS_H
#define PLATFORM_HOST_HOSTEVENTS_H

#include <vector>

namespace dice { namespace hfe { namespace host {

// Backend-independent modifier flags. Backends translate native state into this vocabulary.
enum HostModifier
{
    HostModCapsLock     = 1u << 16,
    HostModShift        = 1u << 17,
    HostModControl      = 1u << 18,
    HostModAlt          = 1u << 19,
    HostModSuper        = 1u << 20,
    HostModNumericPad   = 1u << 21,
    HostModFunction     = 1u << 23,
    // Side-specific held modifiers supplement the aggregate flags.
    HostModLeftControl  = 0x0001,
    HostModLeftShift    = 0x0002,
    HostModRightShift   = 0x0004,
    HostModLeftSuper    = 0x0008,
    HostModRightSuper   = 0x0010,
    HostModLeftAlt      = 0x0020,
    HostModRightAlt     = 0x0040,
    HostModRightControl = 0x2000
};

// Key transition from the hardware. Auto-repeat keyDown events are reported with repeat set;
// modifier keys arrive as press/release pairs derived from flagsChanged events. Caps Lock is a
// toggle on macOS: every change of its lock state is reported as a press, and the matching
// release is delivered at the start of the next pumpEvents call, so the two land in different
// input samples.
struct HostKeyEvent
{
    unsigned char  dik;             // BF2 DIK physical key; 0 means unmapped
    bool           down;
    bool           repeat;
    unsigned int   modifiers;       // HostModifier bits after the event
    unsigned int   timeMs;          // HostClock milliseconds (see tickCountMs)
};

// Text in UTF-16 units, independently of physical key bindings. Backends normalize native
// editing keys and shortcuts before delivery; Ctrl+C remains U+0003, Enter is U+000D.
struct HostTextEvent
{
    unsigned char         dik;      // source physical key, or 0 for unassociated text
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

// Button transition. button uses DirectInput's numbering:
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

// Unaccelerated motion from a separate raw input source, in device counts, x right, y down,
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

} } }

#endif
