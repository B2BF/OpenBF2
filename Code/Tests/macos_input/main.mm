// [TEMP] macOS input review harness. Runs the Cocoa host headless
// (CocoaHost::enableHeadlessForTest: no window, no NSApplication, the real cursor untouched),
// routes NSEvents built from CoreGraphics events through it exactly as pumpEvents routes them,
// drives MacInputDeviceManager with InputManager::update's protocol (update(-1) polls, then one
// positive pass per input tick, 10.0 on the last) and checks what the engine-facing device
// interfaces report against the DirectInput semantics of the recovered DX8 devices
// (KeyboardDevice.cpp, MouseDevice.cpp). [RETIRE-WHEN: macos-client-window]

#import <AppKit/AppKit.h>
#include <ApplicationServices/ApplicationServices.h>
#include <Carbon/Carbon.h>

#include "../../BF2/IO/Input/MacInputDeviceManager.h"
#include "../../BF2/Platform/macOS/CocoaHost.h"
#include "../../BF2/Platform/macOS/MacKeyMap.h"
#include "../../BF2/Platform/macOS/MacRawMouse.h"
#include "../../BF2/Core/Ids.h"

#include <cmath>
#include <cstdio>
#include <ctime>
#include <atomic>
#include <string>
#include <chrono>
#include <thread>
#include <vector>

namespace dice { namespace hfe { namespace io { void installControlMapFixture(); extern IBase* inputDeviceManager; } } }

using namespace dice::hfe;
using macos::CocoaHost;

namespace {

int g_failures = 0;
int g_checks = 0;
std::string g_test;

void check(bool ok, const char* what)
{
    ++g_checks;
    if (!ok)
    {
        ++g_failures;
        std::printf("  FAIL [%s] %s\n", g_test.c_str(), what);
    }
}

void begin(const char* name) { g_test = name; std::printf("[test] %s\n", name); }

// --- event construction ----------------------------------------------------------------------

// Event times on the host clock (system uptime), as hardware events carry them.
CGEventTimestamp nowNs(int offsetMs = 0)
{
    return clock_gettime_nsec_np(CLOCK_UPTIME_RAW) + static_cast<long long>(offsetMs) * 1000000LL;
}

const SInt16 kKeyboardANSI = 40;    // KBGetLayoutType(40) == 'ANSI' (Apple Extended/MacBook ANSI)
const SInt16 kKeyboardISO  = 41;    // KBGetLayoutType(41) == 'ISO '

void inject(CGEventRef cg)
{
    NSEvent* e = [NSEvent eventWithCGEvent:cg];
    CFRelease(cg);
    CocoaHost::instance().injectEventForTest((__bridge void*)e);
}

void key(unsigned short keyCode, bool down, bool repeat = false, SInt16 kbType = kKeyboardANSI,
         CGEventFlags flags = 0, int offsetMs = 0)
{
    CGEventRef cg = CGEventCreateKeyboardEvent(0, keyCode, down);
    CGEventSetFlags(cg, flags);
    CGEventSetIntegerValueField(cg, kCGKeyboardEventKeyboardType, kbType);
    if (repeat) CGEventSetIntegerValueField(cg, kCGKeyboardEventAutorepeat, 1);
    CGEventSetTimestamp(cg, nowNs(offsetMs));
    inject(cg);
}

// flagsChanged: the modifier key's code with the flag set after the change.
void flags(unsigned short keyCode, CGEventFlags after)
{
    CGEventRef cg = CGEventCreateKeyboardEvent(0, keyCode, true);
    CGEventSetType(cg, kCGEventFlagsChanged);
    CGEventSetFlags(cg, after);
    CGEventSetIntegerValueField(cg, kCGKeyboardEventKeyboardType, kKeyboardANSI);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

void move(int dx, int dy)
{
    CGEventRef cg = CGEventCreateMouseEvent(0, kCGEventMouseMoved, CGPointMake(100, 100), kCGMouseButtonLeft);
    CGEventSetIntegerValueField(cg, kCGMouseEventDeltaX, dx);
    CGEventSetIntegerValueField(cg, kCGMouseEventDeltaY, dy);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

void button(int number, bool down, int offsetMs = 0)
{
    CGEventType type;
    if (number == 0)      type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
    else if (number == 1) type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
    else                  type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    CGEventRef cg = CGEventCreateMouseEvent(0, type, CGPointMake(100, 100), static_cast<CGMouseButton>(number));
    CGEventSetIntegerValueField(cg, kCGMouseEventButtonNumber, number);
    CGEventSetTimestamp(cg, nowNs(offsetMs));
    inject(cg);
}

// A wheel event; returns the sign AppKit gives the device direction (natural scrolling undone).
void wheelLines(int lines)
{
    CGEventRef cg = CGEventCreateScrollWheelEvent2(0, kCGScrollEventUnitLine, 1, lines, 0, 0);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

void wheelPixels(int pixels, CGMomentumScrollPhase momentum = kCGMomentumScrollPhaseNone)
{
    CGEventRef cg = CGEventCreateScrollWheelEvent2(0, kCGScrollEventUnitPixel, 1, pixels, 0, 0);
    if (momentum != kCGMomentumScrollPhaseNone)
        CGEventSetIntegerValueField(cg, kCGScrollWheelEventMomentumPhase, momentum);
    CGEventSetTimestamp(cg, nowNs());
    inject(cg);
}

// --- engine side -----------------------------------------------------------------------------

io::IInputDeviceManager* g_manager = 0;

io::IKeyboardDevice* keyboard()
{ return static_cast<io::IKeyboardDevice*>(g_manager->getInputDevice(io::IDFKeyboard)); }
io::IAdvancedInputDevice* mouse()
{ return static_cast<io::IAdvancedInputDevice*>(g_manager->getInputDevice(io::IDFMouse)); }

// One frame: the raw-mouse drain at the end of pumpEvents, then one InputManager::update with
// one input tick: the poll, then the final pass (10.0).
void frame()
{
    CocoaHost::instance().pumpRawMouseForTest();
    g_manager->update(-1.0f);
    g_manager->update(10.0f);
}

// A GCMouse report as MacRawMouse's mouseMovedHandler publishes it (y already down positive).
void raw(float dx, float dy, int offsetMs = 0)
{
    macos::rawMousePublish(dx, dy, CocoaHost::tickCountMs() + offsetMs);
}

// The focus part of BF2::handleInGameWinMessages: setHasFocus(window focus, mask), the mouse bit
// cleared while a menu logic exists (always, on the client).
void engineFocus(bool focus)
{
    const unsigned int mask = static_cast<unsigned int>(-2);
    if (focus) g_manager->activateDevices(mask);
    else       g_manager->deActivateDevices(mask);
}

void setFocus(bool focus)
{
    CocoaHost::instance().overrideFocusForTest(focus, focus);
    engineFocus(focus);
}

// Clears every device: releases all keys and buttons and runs two frames.
void settle()
{
    frame(); frame();
}

// Records the mouse events every host listener sees, as BF2Window_macos's WindowMessageSource does.
struct MouseLog final : macos::IHostEventListener
{
    int moves = 0, buttons = 0, wheels = 0;
    int capturedMoves = 0, capturedButtons = 0, capturedWheels = 0;
    void onMouseMove(const macos::HostMouseMoveEvent& e) override { ++moves; capturedMoves += e.captured; }
    void onMouseButton(const macos::HostMouseButtonEvent& e) override { ++buttons; capturedButtons += e.captured; }
    void onMouseWheel(const macos::HostMouseWheelEvent& e) override { ++wheels; capturedWheels += e.captured; }
    double rawX = 0.0, rawY = 0.0;
    void onRawMouseMove(const macos::HostRawMouseEvent& e) override { rawX += e.dx; rawY += e.dy; }
    void reset() { *this = MouseLog(); }
};

// --- tests -----------------------------------------------------------------------------------

// Every key the shipped Controls.con files bind (mods/bf2 and mods/xpack, Settings and Default)
// is reachable from a macOS key code, and a press and release arrive as DirectInput reports them.
void testControlsConKeys()
{
    begin("Controls.con keys reach their DIK_ codes");
    struct K { const char* name; unsigned char dik; };
    static const K keys[] = {
        {"0",11},{"1",2},{"2",3},{"3",4},{"4",5},{"5",6},{"6",7},{"7",8},{"8",9},{"9",10},
        {"A",30},{"B",48},{"C",46},{"D",32},{"E",18},{"F",33},{"G",34},{"J",36},{"K",37},{"L",38},
        {"M",50},{"N",49},{"P",25},{"Q",16},{"R",19},{"S",31},{"T",20},{"V",47},{"W",17},{"X",45},
        {"Z",44},{"Add",78},{"ArrowDown",208},{"ArrowLeft",203},{"ArrowRight",205},{"ArrowUp",200},
        {"Backspace",14},{"Delete",211},{"End",207},{"Enter",28},{"Escape",1},{"F1",59},{"F2",60},
        {"F3",61},{"F4",62},{"F5",63},{"F6",64},{"F7",65},{"F8",66},{"F9",67},{"F10",68},{"F11",87},
        {"F12",88},{"Grave",41},{"Home",199},{"Insert",210},{"PageDown",209},{"PageUp",201},
        {"PrintScreen",183},{"Space",57},{"Tab",15},
    };
    for (const K& k : keys)
    {
        const unsigned short code = macos::macKeyCodeFromDik(k.dik, false);
        char what[96];
        std::snprintf(what, sizeof what, "IDKey_%s (DIK %u) has a macOS key code", k.name, k.dik);
        check(code != 0xFFFF, what);
        if (code == 0xFFFF) continue;
        key(code, true);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s pressed after its key down", k.name);
        check(keyboard()->keyPressed(static_cast<io::InputDeviceKeys>(k.dik)), what);
        key(code, false);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s released after its key up", k.name);
        check(keyboard()->keyReleased(static_cast<io::InputDeviceKeys>(k.dik)) &&
              !keyboard()->keyDown(static_cast<io::InputDeviceKeys>(k.dik)), what);
    }
    settle();

    // Modifiers come from flagsChanged with the device-dependent bits.
    struct M { const char* name; unsigned short code; CGEventFlags down; unsigned char dik; };
    static const M mods[] = {
        {"LeftShift", 0x38, kCGEventFlagMaskShift | 0x02, 42},
        {"RightShift", 0x3C, kCGEventFlagMaskShift | 0x04, 54},
        {"LeftCtrl", 0x3B, kCGEventFlagMaskControl | 0x01, 29},
        {"RightCtrl", 0x3E, kCGEventFlagMaskControl | 0x2000, 157},
        {"LeftAlt", 0x3A, kCGEventFlagMaskAlternate | 0x20, 56},
        {"RightAlt", 0x3D, kCGEventFlagMaskAlternate | 0x40, 184},
    };
    for (const M& m : mods)
    {
        char what[96];
        flags(m.code, m.down);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s pressed by flagsChanged", m.name);
        check(keyboard()->keyPressed(static_cast<io::InputDeviceKeys>(m.dik)), what);
        flags(m.code, 0);
        frame();
        std::snprintf(what, sizeof what, "IDKey_%s released by flagsChanged", m.name);
        check(keyboard()->keyReleased(static_cast<io::InputDeviceKeys>(m.dik)), what);
    }
    // Caps Lock (IDKey_Capital): a toggle; each change is a press, the release follows next pump.
    flags(0x39, kCGEventFlagMaskAlphaShift);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Capital), "IDKey_Capital pressed when Caps Lock locks");
    inject(CGEventCreate(0));                   // any event: the pump flushes the pending release first
    frame();
    check(keyboard()->keyReleased(io::IDKey_Capital), "IDKey_Capital released on the next pump");
    flags(0x39, 0);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Capital), "IDKey_Capital pressed again when Caps Lock unlocks");
    settle();
}

// DirectInput reports transitions only: auto-repeat key-downs are not device data.
void testKeyRepeat()
{
    begin("auto-repeat is not device data");
    key(kVK_ANSI_W, true);
    frame();
    check(keyboard()->keyPressed(io::IDKey_W), "W pressed");
    for (int i = 0; i < 5; ++i) { key(kVK_ANSI_W, true, true); frame(); }
    check(keyboard()->keyDown(io::IDKey_W) && !keyboard()->keyPressed(io::IDKey_W),
          "W held, not re-pressed, through repeats");
    key(kVK_ANSI_W, false);
    frame();
    check(keyboard()->keyReleased(io::IDKey_W), "W released");
    settle();
}

// The key left of 1 is DIK_GRAVE (the console key) on ANSI and ISO keyboards; the ISO key beside
// left Shift is DIK_OEM_102. macOS swaps the two key codes on ISO keyboards.
void testIsoSwap()
{
    begin("ISO keyboards: console key and OEM_102 by position");
    key(kVK_ANSI_Grave, true, false, kKeyboardANSI);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Grave), "ANSI key left of 1 is DIK_GRAVE");
    key(kVK_ANSI_Grave, false, false, kKeyboardANSI);
    frame();
    key(kVK_ISO_Section, true, false, kKeyboardISO);
    frame();
    check(keyboard()->keyPressed(io::IDKey_Grave), "ISO key left of 1 (kVK_ISO_Section) is DIK_GRAVE");
    key(kVK_ISO_Section, false, false, kKeyboardISO);
    frame();
    key(kVK_ANSI_Grave, true, false, kKeyboardISO);
    frame();
    check(keyboard()->keyPressed(io::IDKey_OEM_102), "ISO key beside left Shift is DIK_OEM_102");
    key(kVK_ANSI_Grave, false, false, kKeyboardISO);
    settle();
}

// Events are consumed by timestamp within a frame's passes (KeyboardDevice::updateCurrentState).
void testSubFrameSampling()
{
    begin("buffered key events are split across input ticks by timestamp");
    key(kVK_ANSI_A, true, false, kKeyboardANSI, 0, -60);
    key(kVK_ANSI_A, false, false, kKeyboardANSI, 0, -10);
    g_manager->update(-1.0f);
    g_manager->update(0.03f);
    check(keyboard()->keyDown(io::IDKey_A), "first pass sees only the press");
    g_manager->update(10.0f);
    check(!keyboard()->keyDown(io::IDKey_A) && keyboard()->keyReleased(io::IDKey_A),
          "last pass sees the release");
    settle();
}

// Focus loss: the window loses focus, the engine deactivates the keyboard and every held key is
// released (KeyboardDevice::deActivate); keys held across a loss/regain inside one pump are dropped
// on the next poll (DIERR_INPUTLOST).
void testFocusLoss()
{
    begin("focus loss clears held keys and buttons");
    key(kVK_ANSI_W, true);
    button(0, true);
    frame();
    check(keyboard()->keyDown(io::IDKey_W) && mouse()->buttonDown(io::IDButton_0), "W and left button held");
    setFocus(false);
    frame();
    check(!keyboard()->keyDown(io::IDKey_W), "W released by deactivation");
    check(!mouse()->buttonDown(io::IDButton_0), "left button released (input lost)");
    key(kVK_ANSI_W, false);                     // the release happens elsewhere
    button(0, false);
    setFocus(true);
    frame();
    check(!keyboard()->keyDown(io::IDKey_W) && !keyboard()->keyPressed(io::IDKey_W), "no stale W after regain");

    key(kVK_ANSI_S, true);
    CocoaHost::instance().overrideFocusForTest(false, false);   // lost and regained within one pump
    CocoaHost::instance().overrideFocusForTest(true, true);
    frame();
    check(!keyboard()->keyDown(io::IDKey_S), "a press before a loss inside one pump is dropped");
    key(kVK_ANSI_S, false);
    settle();
}

// Mouse: raw relative counts, Y down, buttons 0..7 as DIMOFS_BUTTON0..7, sample latency on the
// event clock.
void testMouseBasics()
{
    begin("mouse deltas, buttons and latency");
    move(5, -3);
    move(2, 4);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 7.0f, "X sums deltas (7)");
    check(mouse()->readAxis(io::IDAxis_1, true) == 1.0f, "Y sums deltas, down positive (1)");
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 0.0f, "axes are per sample");
    const io::InputDeviceButtons bits[5] = { io::IDButton_0, io::IDButton_1, io::IDButton_2,
                                             io::IDButton_3, io::IDButton_4 };
    for (int b = 0; b < 5; ++b)
    {
        char what[64];
        button(b, true);
        frame();
        std::snprintf(what, sizeof what, "button %d is IDButton_%d", b, b);
        check(mouse()->buttonPressed(bits[b]), what);
        button(b, false);
        frame();
        check(mouse()->buttonReleased(bits[b]), "and releases");
    }
    button(0, true, -15);
    frame();
    const float latency = mouse()->buttonPressedSampleLatency(io::IDButton_0);
    check(latency > 0.005f && latency < 0.03f, "press latency measured on the event clock (~15 ms)");
    button(0, false);
    settle();
}

// A wheel notch is +120 away from the user, -120 towards (DirectInput lZ, WHEEL_DELTA).
void testWheelNotches()
{
    begin("wheel notches are +-120");
    wheelLines(1);
    frame();
    check(mouse()->readAxis(io::IDAxis_2, true) == 120.0f, "one notch away is +120");
    wheelLines(-1);
    frame();
    check(mouse()->readAxis(io::IDAxis_2, true) == -120.0f, "one notch towards is -120");
    settle();
}

// Trackpad scrolling: DirectInput's wheel reports whole notches, and Controls.con turns any
// positive or negative wheel sample into c_PINextItem / c_PIPrevItem (addAxisToTriggerMapping,
// ControlMap::axisToTrigger fires on value > 0). A 40-point swipe is four notches; it must not
// select the next item on every sample it spans.
void testPreciseScroll()
{
    begin("trackpad scrolling reports whole notches");
    int samples = 0, nonNotch = 0;
    float total = 0.0f;
    for (int i = 0; i < 20; ++i)
    {
        wheelPixels(2);
        frame();
        const float z = mouse()->readAxis(io::IDAxis_2, true);
        if (z != 0.0f) ++samples;
        if (z != 0.0f && std::fabs(z) != 120.0f) ++nonNotch;
        total += z;
    }
    std::printf("  40 points over 20 samples: %d wheel samples, total %.0f\n", samples, total);
    check(samples == 4, "a 40-point swipe fires 4 wheel samples (one per 10 points)");
    check(nonNotch == 0, "each wheel sample is a whole notch");
    check(std::fabs(total) == 480.0f, "the swipe totals 4 notches");
    settle();
}

// The momentum (inertia) phase AppKit synthesizes after the fingers lift is not device motion.
void testMomentumScroll()
{
    begin("momentum scrolling is not wheel input");
    int samples = 0;
    wheelPixels(5, kCGMomentumScrollPhaseBegin);
    frame();
    if (mouse()->readAxis(io::IDAxis_2, true) != 0.0f) ++samples;
    for (int i = 0; i < 30; ++i)
    {
        wheelPixels(10, kCGMomentumScrollPhaseContinue);
        frame();
        if (mouse()->readAxis(io::IDAxis_2, true) != 0.0f) ++samples;
    }
    wheelPixels(0, kCGMomentumScrollPhaseEnd);
    frame();
    std::printf("  31 momentum events: %d wheel samples\n", samples);
    check(samples == 0, "no wheel samples from momentum events");
    settle();
}

// An exclusively acquired DirectInput mouse (MouseDevice's default cooperative level) hides and
// holds the system cursor, so Windows generates no window mouse messages in-game. Host mouse
// events in that state are marked captured for the window-message source; in menus (mouse
// deactivated by MainMenuLogic::activate) they are not.
void testCapturedMouse(MouseLog& log)
{
    begin("in-game mouse events are captured, menu mouse events are not");
    bool hidden = false, disassociated = false;
    log.reset();
    move(3, 3);
    button(0, true);
    button(0, false);
    wheelLines(1);
    frame();
    CocoaHost::instance().cursorStateForTest(hidden, disassociated);
    check(hidden && disassociated, "in-game: cursor hidden and held (relative mode)");
    check(log.moves == 1 && log.buttons == 2 && log.wheels == 1, "in-game: the host reports the events");
    check(log.capturedMoves == 1 && log.capturedButtons == 2 && log.capturedWheels == 1,
          "in-game: every mouse event is captured");

    mouse()->deActivate();                      // MainMenuLogic::activate
    log.reset();
    move(3, 3);
    button(0, true);
    button(0, false);
    wheelLines(1);
    frame();
    CocoaHost::instance().cursorStateForTest(hidden, disassociated);
    check(!hidden && !disassociated, "menu: cursor visible and free");
    check(log.capturedMoves == 0 && log.capturedButtons == 0 && log.capturedWheels == 0,
          "menu: no mouse event is captured");
    check(log.moves == 1 && log.buttons == 2, "menu: the window still gets moves and clicks");

    mouse()->activate();                        // MainMenuLogic::deactivate
    settle();
}

// With a GCMouse connected the in-game counts are its unaccelerated deltas, summed over every
// mouse; the NSEvent deltas (accelerated) are ignored. Raw motion never reaches window listeners.
void testRawMouse(MouseLog& log)
{
    begin("GCMouse deltas drive the in-game mouse");
    macos::rawMouseSetCountForTest(2);          // two mice
    check(CocoaHost::instance().rawMouseAvailable(), "raw mouse available with a GCMouse connected");
    log.reset();
    move(100, 100);                             // the NSEvent of the same motion, accelerated
    raw(5.0f, 3.0f);                            // mouse A
    raw(-2.0f, 1.0f);                           // mouse B
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 3.0f, "X is the sum of the raw deltas (3)");
    check(mouse()->readAxis(io::IDAxis_1, true) == 4.0f, "Y is the sum of the raw deltas, down positive (4)");
    check(log.moves == 1, "the window still sees the NSEvent move (captured), never raw motion");
    for (int i = 0; i < 4; ++i) raw(0.5f, -0.5f);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 2.0f && mouse()->readAxis(io::IDAxis_1, true) == -2.0f,
          "sub-count raw motion is carried (4 x 0.5 = 2)");
    raw(0.0f, 0.0f);
    frame();

    // Timestamps split raw motion across input ticks like any buffered event: MouseDevice's first
    // tick takes what is older than the previous poll plus the tick (30 ms).
    frame();
    raw(7.0f, 0.0f, 5);
    raw(1.0f, 0.0f, 60);
    CocoaHost::instance().pumpRawMouseForTest();
    g_manager->update(-1.0f);
    g_manager->update(0.03f);
    check(mouse()->readAxis(io::IDAxis_0, true) == 7.0f, "first tick sees the older raw report");
    g_manager->update(10.0f);
    check(mouse()->readAxis(io::IDAxis_0, true) == 1.0f, "last tick sees the newer one");

    // Focus: raw reports arriving without focus are discarded.
    CocoaHost::instance().overrideFocusForTest(false, false);
    raw(50.0f, 50.0f);
    frame();
    CocoaHost::instance().overrideFocusForTest(true, true);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 0.0f, "raw motion without focus is dropped");

    // Disconnect at run time: back to the NSEvent deltas; pending raw reports are ignored.
    macos::rawMouseSetCountForTest(0);
    check(!CocoaHost::instance().rawMouseAvailable(), "no raw mouse after the last disconnect");
    raw(9.0f, 9.0f);
    move(4, 2);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 4.0f && mouse()->readAxis(io::IDAxis_1, true) == 2.0f,
          "fallback: NSEvent deltas drive the mouse");
    macos::rawMouseSetCountForTest(1);          // reconnect
    move(4, 2);
    raw(1.0f, 1.0f);
    frame();
    check(mouse()->readAxis(io::IDAxis_0, true) == 1.0f, "after a reconnect the raw deltas are used again");
    macos::rawMouseSetCountForTest(0);
    settle();
}

// The handler queue publishes from another thread while the engine thread pumps and samples.
void testRawMouseThreads(MouseLog& log)
{
    log.reset();
    begin("raw mouse publication across threads");
    macos::rawMouseSetCountForTest(1);
    // A burst of 200000 reports (ring wrap-around and the full-ring path), then 20000 in lockstep
    // with the engine thread (each waits for the previous one to reach the device), which the
    // device must accept completely: a burst can overflow its 128-record buffer, as in DX8.
    const int burst = 200000, paced = 20000, total = burst + paced;
    std::atomic<bool> done(false), burstDone(false);
    std::atomic<int> accepted(0);
    std::thread producer([&]() {
        for (int i = 0; i < burst; ++i)
            while (!macos::rawMousePublish(1.0f, -1.0f, CocoaHost::tickCountMs()))
                std::this_thread::yield();      // full ring: the handler would drop; the test waits
        while (!burstDone.load()) std::this_thread::yield();
        for (int i = 0; i < paced; ++i)
        {
            macos::rawMousePublish(1.0f, -1.0f, CocoaHost::tickCountMs());
            while (accepted.load() <= i) std::this_thread::yield();
        }
        done.store(true);
    });
    double sumX = 0.0, sumY = 0.0;
    int frames = 0;
    while (log.rawX < burst) frame();           // drain the burst; the device may overflow here
    frame(); frame();
    burstDone.store(true);
    for (;;)
    {
        const bool finished = done.load();
        frame();
        sumX += mouse()->readAxis(io::IDAxis_0, true);
        sumY += mouse()->readAxis(io::IDAxis_1, true);
        accepted.store(static_cast<int>(sumX));
        ++frames;
        if (finished)
            break;
    }
    producer.join();
    frame();
    sumX += mouse()->readAxis(io::IDAxis_0, true);
    sumY += mouse()->readAxis(io::IDAxis_1, true);
    std::printf("  host delivered %.0f raw; %d samples over %d frames: sum (%.0f, %.0f), %u full-ring retries\n", log.rawX, total, frames,
                sumX, sumY, macos::rawMouseDropped());
    // The device itself clears on a DirectInput buffer overflow (128 records per poll), as DX8
    // does, and this producer runs far faster than any mouse, so the device keeps only part.
    check(log.rawX == total && log.rawY == -total, "every published report reaches the engine thread once, in pairs");
    check(sumX == -sumY, "X and Y stay paired in the device");
    check(sumX == paced && sumY == -paced, "the paced reports all reach the device's axes");
    macos::rawMouseSetCountForTest(0);
    settle();
}

} // namespace

static void testAsyncEnter()
{
    begin("asynchronous Enter held/consuming press bits");
    CocoaHost& host = CocoaHost::instance();
    (void)host.consumeAsyncEnterState();
    key(kVK_Return, true);
    check(host.consumeAsyncEnterState() == 0x8001, "first Enter query sees held and press");
    check(host.consumeAsyncEnterState() == 0x8000, "second Enter query consumes the press bit");
    key(kVK_Return, true, true);
    check(host.consumeAsyncEnterState() == 0x8000, "auto-repeat creates no second press");
    host.overrideFocusForTest(false, false); host.overrideFocusForTest(true, true);
    key(kVK_Return, true);
    check(host.consumeAsyncEnterState() == 0x8001, "fresh down after an undelivered off-focus release creates a press");
    key(kVK_Return, false);
    check(host.consumeAsyncEnterState() == 0, "release clears held state");
    key(kVK_Return, true); key(kVK_Return, false);
    check(host.consumeAsyncEnterState() == 1, "release before query retains only the press bit");
    check(host.consumeAsyncEnterState() == 0, "released press bit is consumed too");
    key(kVK_ANSI_KeypadEnter, true);
    check(host.consumeAsyncEnterState() == 0x8001, "keypad Enter contributes to VK_RETURN");
    key(kVK_Return, true); key(kVK_ANSI_KeypadEnter, false);
    check(host.consumeAsyncEnterState() == 0x8001, "either Enter keeps the aggregate held");
    key(kVK_Return, false);
    key(kVK_ANSI_A, true);
    check(host.consumeAsyncEnterState() == 0, "ordinary character does not affect Enter");
    key(kVK_ANSI_A, false);
}

int main(int argc, char** argv)
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    @autoreleasepool
    {
        CocoaHost& host = CocoaHost::instance();
        host.enableHeadlessForTest();
        host.overrideFocusForTest(true, true);
        if (argc == 2 && std::string(argv[1]) == "--async-enter-only")
        {
            testAsyncEnter();
            std::printf("[result] %d checks, %d failures\n", g_checks, g_failures);
            return g_failures ? 1 : 0;
        }
        MouseLog log;
        host.addListener(&log);

        io::installControlMapFixture();
        g_manager = static_cast<io::IInputDeviceManager*>(
            io::MacInputDeviceManager::create(io::IID_IInputDeviceManager, 0));
        io::inputDeviceManager = g_manager;
        unsigned int mask = 1087;               // BF2Engine::init: inputManager->init(1087)
        const bool all = g_manager->initDevices(&mask);
        std::printf("[input] initDevices(1087) all=%d opened=0x%X\n", all, mask);
        check(!all && (mask & 3u) == 3u, "keyboard and mouse open; the Falcon never does");
        check(!(mask & io::IDFFalcon), "no Falcon on macOS");
        engineFocus(true);
        mouse()->activate();                    // in-game: MainMenuLogic::deactivate activates the mouse
        check(keyboard()->isActive() && mouse()->isActive(), "keyboard and mouse acquired with focus");

        testControlsConKeys();
        testKeyRepeat();
        testIsoSwap();
        testSubFrameSampling();
        testFocusLoss();
        testMouseBasics();
        testWheelNotches();
        testPreciseScroll();
        testMomentumScroll();
        testCapturedMouse(log);
        testRawMouse(log);
        testRawMouseThreads(log);
        testAsyncEnter();

        host.removeListener(&log);
        g_manager->release();
    }
    std::printf("[result] %d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
