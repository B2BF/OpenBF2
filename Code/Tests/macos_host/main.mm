// [TEMP] Scaffolding for package S2.4: opens the game window through ClientHost, pumps events,
// drives NativeInputDeviceManager with the engine's update protocol (update(-1) to poll, then the
// final positive pass InputManager::update makes) and logs DIK codes, mouse axes and buttons.
// Synthetic NSEvents are posted to this application's own queue (never system-wide) so the whole
// path is exercised without a person at the keyboard; real input is logged the same way.
// The run ends by clicking the window's close button programmatically.
//
//   macos_host_test [--seconds N] [--exclusive] [--fullscreen-cycle] [--no-synthetic]
//                   [--simulate-focus]
//
// --simulate-focus reports focus through the host's test seam when AppKit cannot activate the
// application (a locked screen); without it, input is only routed while the window really has
// focus.
//
// --exclusive acquires the mouse exclusively (relative mode: the cursor is hidden and held for the
// run). [RETIRE-WHEN: macos-client-window] see CMakeLists.txt.

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <ApplicationServices/ApplicationServices.h>

#include "../../BF2/IO/Input/NativeInputDeviceManager.h"
#include "../../BF2/Platform/Host/ClientHost.h"
#include "../../BF2/Platform/macOS/CocoaHostTestHooks.h"
#include "../../BF2/Platform/Host/Gamepad.h"
#include "../../BF2/Core/Ids.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <algorithm>
#include <Carbon/Carbon.h>

namespace dice { namespace hfe { namespace io { void installControlMapFixture(); extern IBase* inputDeviceManager; } } }

using namespace dice::hfe;
using host::ClientHost;
using macos::CocoaHostTestHooks;

namespace {

double now() { return [[NSProcessInfo processInfo] systemUptime]; }

std::string narrow(const std::wstring& w)
{
    std::string s;
    for (wchar_t c : w) s.push_back(c < 0x80 ? static_cast<char>(c) : '?');
    return s;
}

// Window-level listener: logs what the engine's BF2Window adapter would turn into messages.
struct WindowLog : host::IHostEventListener
{
    void onFocusChanged(bool key, bool active) override
    { std::printf("[window] focus key=%d appActive=%d\n", key, active); }
    void onResize(int w, int h, bool minimized) override
    { std::printf("[window] resize %dx%d minimized=%d\n", w, h, minimized); }
    void onCloseRequested() override { std::printf("[window] close requested\n"); }
    void onText(const host::HostTextEvent& e) override
    {
        std::printf("[window] text dik=0x%02X chars=", e.dik);
        for (unsigned short c : e.characters) std::printf("U+%04X ", c);
        std::printf("\n");
    }
    void onMouseButton(const host::HostMouseButtonEvent& e) override
    { std::printf("[window] button %d %s at (%.0f,%.0f) clicks=%d\n", e.button, e.down ? "down" : "up", e.x, e.y, e.clickCount); }
};

NSInteger windowNumber() { return ((__bridge NSWindow*)CocoaHostTestHooks::nativeWindow(ClientHost::instance())).windowNumber; }

// Key events go through CoreGraphics so they carry this machine's keyboard type, as real
// keystrokes do; the host decides the ISO swap from that field.
SInt16 g_keyboardType = 0;

void postKey(NSEventType type, unsigned short keyCode, NSEventModifierFlags flags, NSString* chars)
{
    if (type == NSEventTypeFlagsChanged)
    {
        NSEvent* e = [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags
                                     timestamp:now() windowNumber:windowNumber() context:nil
                                    characters:chars charactersIgnoringModifiers:chars
                                     isARepeat:NO keyCode:keyCode];
        CGEventRef cg = CGEventCreateCopy(e.CGEvent);
        CGEventSetIntegerValueField(cg, kCGKeyboardEventKeyboardType, g_keyboardType);
        NSEvent* typed = [NSEvent eventWithCGEvent:cg];
        CFRelease(cg);
        [NSApp postEvent:typed atStart:NO];
        return;
    }
    CGEventRef cg = CGEventCreateKeyboardEvent(0, keyCode, type == NSEventTypeKeyDown);
    CGEventSetFlags(cg, static_cast<CGEventFlags>(flags));
    CGEventSetIntegerValueField(cg, kCGKeyboardEventKeyboardType, g_keyboardType);
    unichar c[4];
    const NSUInteger n = std::min<NSUInteger>(chars.length, 4);
    [chars getCharacters:c range:NSMakeRange(0, n)];
    CGEventKeyboardSetUnicodeString(cg, n, c);
    NSEvent* e = [NSEvent eventWithCGEvent:cg];
    CFRelease(cg);
    // Re-wrap as an NSEvent for our window so AppKit dispatches it to the game window.
    NSEvent* targeted = [NSEvent keyEventWithType:type location:NSZeroPoint modifierFlags:flags
                                        timestamp:now() windowNumber:windowNumber() context:nil
                                       characters:e.characters charactersIgnoringModifiers:e.charactersIgnoringModifiers
                                        isARepeat:NO keyCode:keyCode];
    CGEventRef typed = CGEventCreateCopy(targeted.CGEvent);
    CGEventSetIntegerValueField(typed, kCGKeyboardEventKeyboardType, g_keyboardType);
    NSEvent* out = [NSEvent eventWithCGEvent:typed];
    CFRelease(typed);
    [NSApp postEvent:out atStart:NO];
}

void postCG(CGEventRef cg)
{
    NSEvent* e = [NSEvent eventWithCGEvent:cg];
    CFRelease(cg);
    if (e) [NSApp postEvent:e atStart:NO];
}

NSPoint viewCentreInWindow()
{
    NSView* view = (__bridge NSView*)CocoaHostTestHooks::nativeView(ClientHost::instance());
    const NSRect b = [view convertRect:view.bounds toView:nil];
    return NSMakePoint(NSMidX(b), NSMidY(b));
}

void postMouse(NSEventType type, int button)
{
    NSEvent* e = [NSEvent mouseEventWithType:type location:viewCentreInWindow() modifierFlags:0
                                   timestamp:now() windowNumber:windowNumber() context:nil
                                 eventNumber:0 clickCount:1 pressure:type == NSEventTypeLeftMouseUp ? 0.0f : 1.0f];
    (void)button;
    [NSApp postEvent:e atStart:NO];
}

// The synthetic script, one step per frame so presses and releases land in different input
// samples: W, left Shift (flagsChanged), key 0x32 (left of 1 on ANSI, beside left Shift on ISO),
// Return, Caps Lock, 37 points right and 11 down of motion, a left click, two wheel notches away.
// Returns false when the script is finished.
bool postSyntheticStep(int step)
{
    switch (step)
    {
    case 0: postKey(NSEventTypeKeyDown, 0x0D, 0, @"w"); return true;
    case 1: postKey(NSEventTypeKeyUp,   0x0D, 0, @"w"); return true;
    case 2: postKey(NSEventTypeFlagsChanged, 0x38, NSEventModifierFlagShift | 0x2, @""); return true;
    case 3: postKey(NSEventTypeFlagsChanged, 0x38, 0, @""); return true;
    case 4: postKey(NSEventTypeKeyDown, 0x32, 0, @"`"); return true;
    case 5: postKey(NSEventTypeKeyUp,   0x32, 0, @"`"); return true;
    case 6: postKey(NSEventTypeKeyDown, 0x24, 0, @"\r"); return true;
    case 7: postKey(NSEventTypeKeyUp,   0x24, 0, @"\r"); return true;
    case 8: postKey(NSEventTypeFlagsChanged, 0x39, NSEventModifierFlagCapsLock, @""); return true;
    case 9:
    {
        // CoreGraphics motion carries the device deltas; the host routes motion by focus.
        CGEventRef move = CGEventCreateMouseEvent(0, kCGEventMouseMoved, CGPointZero, kCGMouseButtonLeft);
        CGEventSetIntegerValueField(move, kCGMouseEventDeltaX, 37);
        CGEventSetIntegerValueField(move, kCGMouseEventDeltaY, 11);
        postCG(move);
        return true;
    }
    case 10: postMouse(NSEventTypeLeftMouseDown, 0); return true;
    case 11: postMouse(NSEventTypeLeftMouseUp, 0); return true;
    case 12: postCG(CGEventCreateScrollWheelEvent(0, kCGScrollEventUnitLine, 1, 2)); return true;
    case 13: postKey(NSEventTypeFlagsChanged, 0x39, 0, @""); return true;
    default: return false;
    }
}

void logDevices(io::IInputDeviceManager* manager)
{
    io::IKeyboardDevice* keyboard = static_cast<io::IKeyboardDevice*>(manager->getInputDevice(io::IDFKeyboard));
    io::IAdvancedInputDevice* mouse = static_cast<io::IAdvancedInputDevice*>(manager->getInputDevice(io::IDFMouse));
    io::IAdvancedInputDevice* pad = static_cast<io::IAdvancedInputDevice*>(manager->getInputDevice(io::IDFGameController_0));
    if (keyboard)
        for (int k = 1; k < 255; ++k)
        {
            const io::InputDeviceKeys key = static_cast<io::InputDeviceKeys>(k);
            if (keyboard->keyPressed(key))
                std::printf("[keyboard] DIK %3d (0x%02X) %-12s down\n", k, k, narrow(keyboard->getKeyName(key)).c_str());
            if (keyboard->keyReleased(key))
                std::printf("[keyboard] DIK %3d (0x%02X) %-12s up\n", k, k, narrow(keyboard->getKeyName(key)).c_str());
        }
    if (mouse)
    {
        const float x = mouse->readAxis(io::IDAxis_0, true), y = mouse->readAxis(io::IDAxis_1, true),
                    z = mouse->readAxis(io::IDAxis_2, true);
        if (x != 0.0f || y != 0.0f || z != 0.0f)
            std::printf("[mouse] axes x=%g y=%g wheel=%g\n", x, y, z);
        for (int b = 0; b < mouse->getNumButtons(); ++b)
        {
            const io::InputDeviceButtons bit = static_cast<io::InputDeviceButtons>(1u << b);
            if (mouse->buttonPressed(bit))  std::printf("[mouse] button %d down\n", b);
            if (mouse->buttonReleased(bit)) std::printf("[mouse] button %d up\n", b);
        }
    }
    if (pad)
        for (int b = 0; b < pad->getNumButtons(); ++b)
        {
            const io::InputDeviceButtons bit = static_cast<io::InputDeviceButtons>(1u << b);
            if (pad->buttonPressed(bit)) std::printf("[pad] button %d down\n", b);
        }
}

} // namespace

int main(int argc, char** argv)
{
    std::setvbuf(stdout, 0, _IONBF, 0);
    double seconds = 3.0;
    bool exclusive = false, fullscreenCycle = false, synthetic = true, simulateFocus = false;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seconds") && i + 1 < argc) seconds = std::atof(argv[++i]);
        else if (!std::strcmp(argv[i], "--exclusive")) exclusive = true;
        else if (!std::strcmp(argv[i], "--fullscreen-cycle")) fullscreenCycle = true;
        else if (!std::strcmp(argv[i], "--no-synthetic")) synthetic = false;
        else if (!std::strcmp(argv[i], "--simulate-focus")) simulateFocus = true;
    }

    g_keyboardType = static_cast<SInt16>(LMGetKbdType());
    ClientHost& host = ClientHost::instance();
    if (!host.initApplication()) { std::printf("FAIL initApplication\n"); return 1; }

    const host::HostDisplayModeInfo desktop = host.desktopMode();
    std::printf("[display] desktop %dx%d@%d, %zu modes, keyboard type %d ISO=%d, gamepads=%d\n",
                desktop.width, desktop.height, desktop.refreshHz, host.displayModes().size(),
                g_keyboardType, CocoaHostTestHooks::isISOKeyboard(host), host::gamepadCount());

    WindowLog windowLog;
    host.addListener(&windowLog);
    host::HostWindowDesc desc;
    desc.title = "OpenBF2 S2.4 host test";
    desc.width = 800; desc.height = 600; desc.x = 50; desc.y = 50;
    desc.fullscreen = false; desc.hideCursor = false;
    if (!host.createWindow(desc)) { std::printf("FAIL createWindow\n"); return 1; }
    CALayer* layer = (__bridge CALayer*)CocoaHostTestHooks::metalLayer(host);
    std::printf("[window] created view=%p layer=%s drawable=%dx%d\n", CocoaHostTestHooks::nativeView(host),
                layer ? object_getClassName(layer) : "null", host.clientWidth(), host.clientHeight());

    // The engine's order: InputManager::init -> initDevices, with the globals already published.
    io::installControlMapFixture();
    io::IInputDeviceManager* manager = static_cast<io::IInputDeviceManager*>(
        io::NativeInputDeviceManager::create(io::IID_IInputDeviceManager, 0));
    io::inputDeviceManager = manager;
    unsigned int mask = io::IDFMouse | io::IDFKeyboard | io::IDFGameController_0;
    const bool allOpened = manager->initDevices(&mask);
    std::printf("[input] initDevices all=%d opened mask=0x%X\n", allOpened, mask);
    if (!exclusive)
        manager->getInputDevice(io::IDFMouse)->setCooperativeMode(io::IDM_NONEXCLUSIVE);

    if (simulateFocus && !host.hasFocus())
    {
        std::printf("[test] AppKit focus key=%d active=%d; simulating focus (test seam)\n",
                    ((__bridge NSWindow*)CocoaHostTestHooks::nativeWindow(host)).isKeyWindow ? 1 : 0, [NSApp isActive] ? 1 : 0);
        CocoaHostTestHooks::overrideFocusForTest(host, true, true);
    }
    const double start = now();
    bool posted = false;
    int step = 0;
    bool fullscreenOn = false, fullscreenDone = !fullscreenCycle;
    double fullscreenAt = 0.0;
    bool quit = false;
    while (!quit && now() - start < seconds)
    {
        quit = host.waitEvents(1.0 / 60.0);
        // BF2::handleInGameWinMessages: focus decides activation every frame.
        if (host.hasFocus()) manager->activateDevices(0xFFFFFFFFu);
        else manager->deActivateDevices(0xFFFFFFFFu);
        manager->update(-1.0f);
        manager->update(10.0f);
        logDevices(manager);

        if (synthetic && host.hasFocus() && now() - start > 0.3 && step >= 0)
        {
            if (!posted)
                std::printf("[test] posting synthetic input\n");
            posted = true;
            step = postSyntheticStep(step) ? step + 1 : -1;
        }
        if (!fullscreenDone && !fullscreenOn && now() - start > 0.8)
        {
            host.setFullscreen(true);
            fullscreenOn = true;
            fullscreenAt = now();
            std::printf("[window] fullscreen=%d drawable=%dx%d\n", host.isFullscreen(), host.clientWidth(), host.clientHeight());
        }
        if (fullscreenOn && now() - fullscreenAt > 0.7)
        {
            host.setFullscreen(false);
            fullscreenOn = false;
            fullscreenDone = true;
            std::printf("[window] fullscreen=%d drawable=%dx%d\n", host.isFullscreen(), host.clientWidth(), host.clientHeight());
        }
    }
    if (!posted && synthetic)
        std::printf("[test] window never had focus; synthetic input not posted\n");

    // Close the way a user would: the title-bar close button.
    if (!quit)
    {
        [(__bridge NSWindow*)CocoaHostTestHooks::nativeWindow(host) performClose:nil];
        quit = host.pumpEvents();
    }
    std::printf("[test] quit requested=%d\n", quit);

    manager->deActivateDevices(0xFFFFFFFFu);
    manager->release();
    io::inputDeviceManager = 0;
    host.removeListener(&windowLog);
    host.destroyWindow();
    host.pumpEvents();
    std::printf("[test] done\n");
    return quit ? 0 : 2;
}
