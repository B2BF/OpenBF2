// [CHOSEN, not recovered] macOS port: Cocoa implementation of CocoaHost. Compile with ARC
// (-fobjc-arc). See CocoaHost.h for the contract and docs/macos-s2-4.md for the design.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#include <unistd.h>                         // getppid: the launcher's pid (activation retries)

#include "CocoaHost.h"
#include "MacRawMouse.h"
#include "MacInputScript.h"          // [TEMP] BF2_INPUT_SCRIPT (docs/macos-c-input-script.md)
#include "MacCursorFile.h"

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include <Carbon/Carbon.h>                  // LMGetKbdType, TIS input sources, UCKeyTranslate
#include <IOKit/pwr_mgt/IOPMLib.h>          // display-sleep assertion


#if !__has_feature(objc_arc)
#error "CocoaHost.mm must be compiled with -fobjc-arc"
#endif

// HIToolbox exports KBGetLayoutType but no current SDK header declares it. The layout-type
// constants are four-character codes ('ANSI', 'ISO ', 'JIS ').
extern "C" UInt32 KBGetLayoutType(SInt16 keyboardType);

namespace {
const UInt32 kLayoutTypeISO = 0x49534F20u;  // 'ISO '

unsigned int msFromSeconds(NSTimeInterval seconds)
{
    return static_cast<unsigned int>(static_cast<unsigned long long>(seconds * 1000.0));
}
} // namespace

using dice::hfe::macos::CocoaHost;

namespace {
// [CHOSEN, not recovered] A cursor token (the counterpart of an HCURSOR). The two shape tokens
// show the system arrow; a token from loadCursorFromFile carries the decoded file and the
// NSCursors built from it for the current view scale.
struct HostCursor
{
    bool custom = false;
    dice::hfe::macos::CursorAnimation anim;
    int totalJiffies = 0;
    CGFloat builtScale = 0.0;           // points per frame pixel the cursors were built for
    CGFloat builtBacking = 0.0;
    NSArray<NSCursor*>* cursors = nil;  // one per frame; NSNull-free, empty when building failed
};

// The view's cursor rect asks the host for the cursor it shows (defined after CocoaHost::Impl).
NSCursor* currentViewCursor();
} // namespace

@interface OpenBF2HostWindow : NSWindow
@end

@implementation OpenBF2HostWindow
// Borderless windows refuse key status by default; the full-screen game window needs it.
- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

@interface OpenBF2HostView : NSView
@end

@implementation OpenBF2HostView
- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]))
    {
        self.wantsLayer = YES;
        self.layerContentsRedrawPolicy = NSViewLayerContentsRedrawNever;
    }
    return self;
}
- (CALayer*)makeBackingLayer
{
    CAMetalLayer* layer = [CAMetalLayer layer];
    layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    layer.framebufferOnly = YES;
    layer.opaque = YES;
    layer.contentsScale = 1.0;
    layer.contentsGravity = kCAGravityResize;
    // [CHOSEN, not recovered] The internal-resolution drawable is enlarged by Core Animation
    // on Retina. Preserve its pixel-snapped bitmap text instead of applying a second linear
    // filter after the D3D font sampler. Keep linear minification for windows scaled to fit.
    layer.magnificationFilter = kCAFilterNearest;
    CGColorRef black = CGColorCreateGenericGray(0.0, 1.0);
    layer.backgroundColor = black;
    CGColorRelease(black);
    return layer;
}
- (BOOL)wantsUpdateLayer       { return YES; }
// [CHOSEN, not recovered] A loaded game cursor is the view's cursor rect, so AppKit shows it when
// the pointer enters the view and restores the arrow when it leaves (title bar, other windows).
- (void)resetCursorRects
{
    if (NSCursor* cursor = currentViewCursor())
        [self addCursorRect:self.bounds cursor:cursor];
}
- (BOOL)acceptsFirstResponder  { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
// Keys are consumed by the host pump before dispatch; swallowing them here prevents the
// "unhandled key" alert sound. Key equivalents (Cmd-Q, Cmd-H) still reach the menu.
- (void)keyDown:(NSEvent*)event       { (void)event; }
- (void)keyUp:(NSEvent*)event         { (void)event; }
- (void)flagsChanged:(NSEvent*)event  { (void)event; }
// AppKit resets the layer scale when the window moves between displays; the drawable keeps
// its internal resolution and Core Animation scales it to the view.
- (void)viewDidChangeBackingProperties
{
    [super viewDidChangeBackingProperties];
    self.layer.contentsScale = 1.0;
}
@end

@interface OpenBF2HostDelegate : NSObject <NSWindowDelegate, NSApplicationDelegate>
- (void)syncFocus;
@end

namespace dice {
namespace hfe {
namespace macos {

struct CocoaHost::Impl : IInputScriptSink
{
    OpenBF2HostDelegate* delegate = nil;
    OpenBF2HostWindow*   window   = nil;
    OpenBF2HostView*     view     = nil;
    CAMetalLayer*        layer    = nil;

    bool appInitialized = false;
    bool quit           = false;
    bool fullscreen     = false;
    bool visible        = false;
    bool minimized      = false;
    bool windowKey      = false;
    bool appActive      = false;

    int  backWidth  = 0;            // drawableSize
    int  backHeight = 0;
    NSRect windowedFrame = NSZeroRect;

    int  cursorCounter    = 0;      // ShowCursor display counter
    bool cursorHiddenInView = false;
    bool mouseInView      = false;
    bool relativeMouse    = false;
    bool mouseDisassociated = false;
    bool cursorHidden     = false;  // the [NSCursor hide] balance this host owns
    HostCursorShape cursorShape = HostCursorArrow;
    HostCursor* activeCursor = nullptr;     // the last SetCursor token (null: none or hidden)
    double cursorAnimStart = 0.0;           // systemUptime when activeCursor was selected
    NSCursor* rectCursor = nil;             // the cursor the view's cursor rect holds
    NSCursor* shownCursor = nil;            // the cursor last set while the pointer was inside

    unsigned int modifiers = 0;
    // [CHOSEN, not recovered] Native GetAsyncKeyState(VK_RETURN) adapter.
    bool enterHeld[2] = { false, false };
    bool enterPressed = false;
    bool capsReleasePending = false;
    HostKeyEvent capsRelease;
    unsigned int buttonsDown = 0;   // buttons whose press was reported
    double wheelRemainder = 0.0;

    IOPMAssertionID sleepAssertion = 0;
    bool sleepAssertionHeld = false;

    std::vector<IHostEventListener*> listeners;

    // [TEMP] BF2_INPUT_SCRIPT (MacInputScript.h): scripted input replaces the real devices, focus
    // is reported as held, and the real cursor is left alone. [RETIRE-WHEN: client-test-automation-replaced]
    bool scripted = false;
    // [TEMP] BF2_INPUT_SCRIPT_REAL_FOCUS=1: a scripted run still follows the real window and
    // application focus (the app is activated at window creation and focus changes from outside,
    // e.g. activating another app, reach the engine), so deactivate/reactivate paths can be tested
    // unattended. Scripted input and the cursor rules are unchanged. [RETIRE-WHEN: client-test-automation-replaced]
    bool scriptedFocus = false;     // scripted && !BF2_INPUT_SCRIPT_REAL_FOCUS: focus is forced held
    unsigned int deactivations = 0; // deactivationCount(): active -> inactive transitions

    // [CHOSEN, not recovered] Activation after a start from another application (the setup app
    // runs the client through NSTask). Since macOS 14 activation is cooperative: a request made
    // while another application is active may be refused, and a refused game window stays in
    // front of the launcher without key status, so its clicks never reach the menu. The request
    // is repeated for a few seconds, but only while the application that started this process (or
    // nobody) is frontmost: an application the user went to meanwhile keeps the front.
    bool   activationPending = false;
    bool   fullscreenPending = false;
    // [CHOSEN, not recovered] The system reports this process active (another app activated it
    // during launch) while NSApp never received the activation; AppKit then keeps isActive NO
    // and its windows non-key, so the engine's focus gate drops every click. When that state
    // lasts, the host trusts the system's answer until NSApp catches up or the user leaves.
    bool   systemActive = false; // a fullscreen start waiting for activation (createWindow)
    CocoaHost* owner = nullptr;       // for becameActive's deferred setFullscreen
    double activationStarted = 0.0;  // systemUptime of the first request
    double activationLastTry = 0.0;
    float scriptX = 0.0f, scriptY = 0.0f;
    bool scriptPointerSet = false;

#if defined(OPENBF2_HOST_TEST_SEAMS)
    bool testHeadless = false;      // enableHeadlessForTest: no window, no real cursor changes
#else
    static constexpr bool testHeadless = false;
#endif

    std::vector<IHostEventListener*> snapshot() const { return listeners; }

    bool hasFocus() const { return (window != nil || testHeadless) && windowKey && appActive; }

    static double uptime() { return [[NSProcessInfo processInfo] systemUptime]; }

    // -[NSApplication activate] from macOS 14 on; activateIgnoringOtherApps: before (its
    // "ignoring" flag is ignored since macOS 14 and the selector deprecated).
    static void requestActivation()
    {
        if (@available(macOS 14.0, *))
        {
            [NSApp activate];
        }
        else
        {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [NSApp activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
        }
    }

    // The first request, made when the game window is shown. Scripted runs never take the
    // user's focus ([TEMP] scriptedFocus).
    void beginActivation()
    {
        if (scriptedFocus || testHeadless)
            return;
        if ([NSApp isActive])
            return;
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        std::fprintf(stderr, "[host] activation requested (frontmost: %s pid %d, launcher pid %d)\n",
                     front.localizedName.UTF8String ?: "none", front ? front.processIdentifier : 0,
                     static_cast<int>(getppid()));
        activationPending = true;
        activationStarted = activationLastTry = uptime();
        requestActivation();
    }

    // Called on every pump and on applicationDidBecomeActive: reports the outcome once, and
    // repeats the request every half second for ten seconds while the launcher, nobody or this
    // process is frontmost.
    void updateActivation()
    {
        if (!activationPending)
            return;
        const double now = uptime();
        if ([NSApp isActive])
        {
            activationPending = false;
            std::fprintf(stderr, "[host] application active after %d ms\n",
                         static_cast<int>((now - activationStarted) * 1000.0));
            return;
        }
        if (!systemActive && now - activationStarted > 1.0 &&
            [NSRunningApplication currentApplication].active)
        {
            NSRunningApplication* f = [[NSWorkspace sharedWorkspace] frontmostApplication];
            if (f && f.processIdentifier == getpid())
            {
                systemActive = true;
                std::fprintf(stderr, "[host] system reports the application active; NSApp does not - using the system's state\n");
                becameActive();
                [delegate syncFocus];
            }
        }
        if (now - activationStarted > 10.0)
        {
            activationPending = false;
            std::fprintf(stderr, "[host] application not activated; click the game window or switch to it\n");
            return;
        }
        if (now - activationLastTry < 0.5)
            return;
        activationLastTry = now;
        // Retry while the launcher, nobody, or this process itself is frontmost. A child started
        // by the launcher can already be reported frontmost (its window ordered front) while
        // NSApp is still inactive; that state needs the retry most, and the window must be made
        // key again with it.
        NSRunningApplication* front = [[NSWorkspace sharedWorkspace] frontmostApplication];
        const pid_t frontPid = front ? front.processIdentifier : 0;
        if (front && frontPid != getppid() && frontPid != getpid())
            return;
        if (frontPid == getpid())
        {
            [[NSRunningApplication currentApplication] activateWithOptions:NSApplicationActivateAllWindows];
            if (window && visible && !minimized && !window.isKeyWindow)
            {
                [window makeKeyAndOrderFront:nil];
                [window makeFirstResponder:view];
            }
        }
        requestActivation();
    }

    // The application became active: the game window takes key status if it lost it (a window
    // ordered front while the application was inactive), so input reaches the engine at once.
    void becameActive()
    {
        if (fullscreenPending)
        {
            fullscreenPending = false;
            if (owner)
                owner->setFullscreen(true);
        }
        if (window && visible && !minimized && !window.isKeyWindow)
        {
            [window makeKeyAndOrderFront:nil];
            [window makeFirstResponder:view];
        }
        updateActivation();
    }

    // [CHOSEN, not recovered] Win32 sends WM_SETCURSOR and WM_MOUSEMOVE when a window under the
    // pointer is activated; the menu learns the pointer position before the first click instead of
    // after the first move. A view-relative move without motion, only when the pointer is inside.
    void reportPointer()
    {
        if (!window || !view || scripted || testHeadless)
            return;
        const NSPoint inWindow = [window convertPointFromScreen:[NSEvent mouseLocation]];
        const NSPoint p = [view convertPoint:inWindow fromView:nil];
        const NSRect b = view.bounds;
        if (b.size.width <= 0 || b.size.height <= 0 || !NSPointInRect(p, b))
            return;
        HostMouseMoveEvent move;
        move.dx = move.dy = 0.0f;
        move.x = static_cast<float>(p.x * backWidth / b.size.width);
        move.y = static_cast<float>((b.size.height - p.y) * backHeight / b.size.height);
        move.insideView = true;
        move.captured = relativeMouse && hasFocus();
        move.timeMs = CocoaHost::tickCountMs();
        if (!mouseInView)
        {
            mouseInView = true;
            applyCursor();
        }
        for (IHostEventListener* l : snapshot())
            l->onMouseMove(move);
    }

    void updateAsyncEnter(unsigned short keyCode, bool down, bool repeat)
    {
        const int index = keyCode == kVK_Return ? 0 : keyCode == kVK_ANSI_KeypadEnter ? 1 : -1;
        if (index < 0) return;
        // A non-repeat Cocoa down is a fresh press even when its preceding
        // release happened outside this application and was not delivered.
        if (down && !repeat) enterPressed = true;
        enterHeld[index] = down;
    }

    // [CHOSEN, not recovered] Windowed content size in points for a back buffer of
    // width x height pixels. A back buffer that fits the screen's visible area keeps one point
    // per pixel (the size a 96-dpi Windows desktop shows). A larger one is shown at the screen's
    // backing scale (one physical pixel per back-buffer pixel on Retina), and if it still does
    // not fit it is scaled down to fit, keeping its aspect. The drawable stays width x height.
    static NSSize windowedContentSize(int width, int height, NSScreen* screen, NSWindowStyleMask style)
    {
        NSSize size = NSMakeSize(width, height);
        if (!screen)
            return size;
        const NSRect visible = screen.visibleFrame;
        const NSRect probe = [NSWindow frameRectForContentRect:NSMakeRect(0, 0, 100, 100) styleMask:style];
        const CGFloat maxW = NSWidth(visible) - (NSWidth(probe) - 100);
        const CGFloat maxH = NSHeight(visible) - (NSHeight(probe) - 100);
        if (size.width <= maxW && size.height <= maxH)
            return size;
        const CGFloat scale = screen.backingScaleFactor > 1.0 ? screen.backingScaleFactor : 1.0;
        size = NSMakeSize(size.width / scale, size.height / scale);
        if (size.width > maxW || size.height > maxH)
        {
            const CGFloat k = std::min(maxW / size.width, maxH / size.height);
            size = NSMakeSize(std::floor(size.width * k), std::floor(size.height * k));
        }
        return size;
    }

    void updateDrawable()
    {
        if (!layer)
            return;
        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        layer.contentsScale = 1.0;
        layer.drawableSize = CGSizeMake(backWidth, backHeight);
        [CATransaction commit];
    }

    // [CHOSEN, not recovered] Builds a token's NSCursors for the current view scale. Retail shows
    // the 32x32 cursor at one back-buffer pixel per cursor pixel (a full-screen mode change on
    // Windows), so its size follows the game image: points per back-buffer pixel, times the frame
    // size. Pixels are enlarged nearest-neighbour to the next whole multiple and, when the
    // Retina target is not a whole multiple, reduced to it with high-quality filtering.
    static NSCursor* buildCursorFrame(const CursorFrame& f, CGFloat scale, CGFloat backing)
    {
        const NSSize points = NSMakeSize(f.width * scale, f.height * scale);
        const NSPoint hot = NSMakePoint(f.hotX * scale, f.hotY * scale);
        if (!f.png.empty())
        {
            NSData* data = [NSData dataWithBytes:f.png.data() length:f.png.size()];
            NSImage* image = [[NSImage alloc] initWithData:data];
            if (!image)
                return nil;
            image.size = points;
            return [[NSCursor alloc] initWithImage:image hotSpot:hot];
        }
        const int w = f.width, h = f.height;
        const int pw = std::max(1, static_cast<int>(std::lround(points.width * backing)));
        const int ph = std::max(1, static_cast<int>(std::lround(points.height * backing)));
        const int m = std::max(1, (pw + w - 1) / w);
        const int uw = w * m, uh = h * m;
        // The reps own their pixel buffers (no provider referring to a temporary).
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:nullptr pixelsWide:uw pixelsHigh:uh bitsPerSample:8
                     samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
                        bitmapFormat:NSBitmapFormatAlphaNonpremultiplied bytesPerRow:uw * 4 bitsPerPixel:32];
        if (!rep || !rep.bitmapData)
            return nil;
        std::uint8_t* up = rep.bitmapData;
        for (int y = 0; y < uh; ++y)
            for (int x = 0; x < uw; ++x)
                std::memcpy(&up[(static_cast<size_t>(y) * uw + x) * 4],
                            &f.rgba[(static_cast<size_t>(y / m) * w + x / m) * 4], 4);
        if (uw != pw || uh != ph)
        {
            NSBitmapImageRep* scaled = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr pixelsWide:pw pixelsHigh:ph bitsPerSample:8
                         samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
                            bitmapFormat:0 bytesPerRow:pw * 4 bitsPerPixel:32];
            NSGraphicsContext* gc = scaled ? [NSGraphicsContext graphicsContextWithBitmapImageRep:scaled] : nil;
            if (!gc)
                return nil;
            std::memset(scaled.bitmapData, 0, static_cast<size_t>(pw) * ph * 4);
            [NSGraphicsContext saveGraphicsState];
            [NSGraphicsContext setCurrentContext:gc];
            gc.imageInterpolation = NSImageInterpolationHigh;
            [rep drawInRect:NSMakeRect(0, 0, pw, ph) fromRect:NSZeroRect
                  operation:NSCompositingOperationCopy fraction:1.0 respectFlipped:NO hints:nil];
            [gc flushGraphics];
            [NSGraphicsContext restoreGraphicsState];
            rep = scaled;
        }
        rep.size = points;
        NSImage* nsImage = [[NSImage alloc] initWithSize:points];
        [nsImage addRepresentation:rep];
        return [[NSCursor alloc] initWithImage:nsImage hotSpot:hot];
    }

    // The NSCursor for the active token's current animation step, or nil for the system arrow.
    NSCursor* customCursor()
    {
        HostCursor* c = activeCursor;
        if (!c || !c->custom || !view || backHeight <= 0)
            return nil;
        const NSRect b = view.bounds;
        if (b.size.height <= 0)
            return nil;
        const CGFloat scale = std::min<CGFloat>(4.0, std::max<CGFloat>(0.5, b.size.height / backHeight));
        const CGFloat backing = window && window.backingScaleFactor > 0 ? window.backingScaleFactor : 1.0;
        if (!c->cursors || std::fabs(c->builtScale - scale) > 0.001 || c->builtBacking != backing)
        {
            NSMutableArray<NSCursor*>* built = [NSMutableArray arrayWithCapacity:c->anim.frames.size()];
            for (const CursorFrame& f : c->anim.frames)
            {
                NSCursor* cursor = buildCursorFrame(f, scale, backing);
                if (!cursor)
                {
                    built = [NSMutableArray array];
                    break;
                }
                [built addObject:cursor];
            }
            c->cursors = built;
            c->builtScale = scale;
            c->builtBacking = backing;
        }
        if (c->cursors.count == 0 || c->anim.steps.empty())
            return nil;
        // Windows plays an animated cursor's steps for their jiffies (1/60 s) and loops.
        size_t step = 0;
        if (c->anim.steps.size() > 1 && c->totalJiffies > 0)
        {
            const double elapsed = ([NSProcessInfo processInfo].systemUptime - cursorAnimStart) * 60.0;
            long t = static_cast<long>(std::fmod(std::max(0.0, elapsed), static_cast<double>(c->totalJiffies)));
            while (step + 1 < c->anim.steps.size() && t >= c->anim.jiffies[step])
                t -= c->anim.jiffies[step++];
        }
        const int frame = c->anim.steps[step];
        return frame >= 0 && static_cast<NSUInteger>(frame) < c->cursors.count ? c->cursors[frame] : nil;
    }

    bool pointerOverView() const
    {
        if (!window || !view || !window.visible)
            return false;
        const NSPoint screen = [NSEvent mouseLocation];
        if ([NSWindow windowNumberAtPoint:screen belowWindowWithWindowNumber:0] != window.windowNumber)
            return false;
        const NSPoint p = [view convertPoint:[window convertPointFromScreen:screen] fromView:nil];
        return NSPointInRect(p, view.bounds);
    }

    // [CHOSEN, not recovered] Keeps the cursor image in step with SetCursor's token: the view's
    // cursor rect holds the current frame, and while the active application's pointer is over the
    // view the frame is set directly so animation steps and token changes show at once. Outside
    // the view and in other applications AppKit's own cursors apply. Visibility is applyCursor's.
    void updateCursorImage()
    {
        if (testHeadless || !window || !view)
        {
            rectCursor = shownCursor = nil;
            return;
        }
        NSCursor* cursor = customCursor();
        if (cursor != rectCursor)
        {
            rectCursor = cursor;
            [window invalidateCursorRectsForView:view];
        }
        if (!appActive || !windowKey || !pointerOverView())
        {
            shownCursor = nil;
            return;
        }
        NSCursor* target = cursor ? cursor : [NSCursor arrowCursor];
        if (target != shownCursor)
        {
            [target set];
            shownCursor = target;
        }
    }

    // Applies the combined cursor-visibility rule. The cursor is only ever hidden while the
    // application is active, so other applications never inherit a hidden cursor.
    void applyCursor()
    {
        const bool focus = hasFocus();
        const bool wantHidden = appActive && (window != nil || testHeadless) &&
            (cursorCounter < 0 || (relativeMouse && focus) || (cursorHiddenInView && mouseInView));
        const bool realCursor = !testHeadless && !scripted;
        if (wantHidden != cursorHidden && realCursor)
        {
            if (wantHidden)
                [NSCursor hide];
            else
                [NSCursor unhide];
        }
        cursorHidden = wantHidden;
        const bool wantDisassociated = relativeMouse && focus;
        if (wantDisassociated != mouseDisassociated)
        {
            if (realCursor)
            {
                if (wantDisassociated)
                    warpToCentre();
                CGAssociateMouseAndMouseCursorPosition(wantDisassociated ? false : true);
            }
            mouseDisassociated = wantDisassociated;
        }
    }

    void warpToCentre()
    {
        if (!window || !view || scripted)
            return;
        const NSRect inWindow = [view convertRect:view.bounds toView:nil];
        const NSRect onScreen = [window convertRectToScreen:inWindow];
        NSArray<NSScreen*>* screens = [NSScreen screens];
        if (screens.count == 0)
            return;
        // CoreGraphics global coordinates have their origin at the primary display's top left.
        const CGFloat primaryHeight = screens[0].frame.size.height;
        const CGPoint centre = CGPointMake(NSMidX(onScreen), primaryHeight - NSMidY(onScreen));
        CGWarpMouseCursorPosition(centre);
    }

    // View-relative location in back-buffer pixels, origin top left.
    void locate(NSEvent* event, float& x, float& y, bool& inside) const
    {
        x = y = 0.0f;
        inside = false;
        if (testHeadless && !view)
        {
            x = static_cast<float>(event.locationInWindow.x);
            y = static_cast<float>(event.locationInWindow.y);
            inside = true;
            return;
        }
        if (!view || event.window != window)
            return;
        const NSPoint p = [view convertPoint:event.locationInWindow fromView:nil];
        const NSRect b = view.bounds;
        if (b.size.width <= 0 || b.size.height <= 0)
            return;
        inside = NSPointInRect(p, b);
        x = static_cast<float>(p.x * backWidth / b.size.width);
        y = static_cast<float>((b.size.height - p.y) * backHeight / b.size.height);
    }

    void setFocusState(bool key, bool active)
    {
        if (scriptedFocus)
            key = active = true;                 // [TEMP] scripted runs keep focus
        const bool activeChanged = active != appActive;
        const bool changed = key != windowKey || activeChanged;
        const bool hadFocus = hasFocus();
        windowKey = key;
        appActive = active;
        if (!changed)
            return;
        // [TEMP] [CHOSEN, not recovered] BF2_HOST_TRACE=1: each focus transition, for unattended
        // deactivate/reactivate runs. [RETIRE-WHEN: macos-options-validated]
        static const bool trace = [] { const char* e = std::getenv("BF2_HOST_TRACE"); return e && e[0] == '1'; }();
        if (trace)
            std::fprintf(stderr, "[host] focus key=%d active=%d fullscreen=%d\n",
                         key ? 1 : 0, active ? 1 : 0, fullscreen ? 1 : 0);
        if (activeChanged && !active)
            ++deactivations;
        if (!hasFocus())
            wheelRemainder = 0.0;
        applyCursor();
        for (IHostEventListener* l : snapshot())
        {
            if (activeChanged)
                l->onAppActivationChanged(active);
            l->onFocusChanged(key, active);
        }
        if (!hadFocus && hasFocus())
            reportPointer();
    }

    void requestClose()
    {
        for (IHostEventListener* l : snapshot())
            l->onCloseRequested();
        quit = true;
    }

    void notifyResize()
    {
        // [TEMP] [CHOSEN, not recovered] BF2_HOST_TRACE=1: window geometry after each resize or
        // full-screen switch, for unattended option runs. [RETIRE-WHEN: macos-options-validated]
        static const bool trace = [] { const char* e = std::getenv("BF2_HOST_TRACE"); return e && e[0] == '1'; }();
        if (trace && window)
        {
            NSScreen* s = window.screen ?: [NSScreen mainScreen];
            const NSRect c = [window contentRectForFrameRect:window.frame];
            const NSRect backing = [view convertRectToBacking:view.bounds];
            std::fprintf(stderr, "[TEMP] [HOST] resize back=%dx%d fullscreen=%d content=%.0fx%.0f@(%.0f,%.0f) "
                         "screen=%.0fx%.0f scale=%.1f drawable=%.0fx%.0f backing=%.0fx%.0f "
                         "magnification=%s window=%ld borderless=%d\n",
                         backWidth, backHeight, fullscreen ? 1 : 0, NSWidth(c), NSHeight(c), NSMinX(c), NSMinY(c),
                         NSWidth(s.frame), NSHeight(s.frame), s.backingScaleFactor,
                         layer ? layer.drawableSize.width : 0.0, layer ? layer.drawableSize.height : 0.0,
                         NSWidth(backing), NSHeight(backing),
                         layer ? layer.magnificationFilter.UTF8String : "none",
                         static_cast<long>(window.windowNumber),
                         window.styleMask == NSWindowStyleMaskBorderless ? 1 : 0);
            std::fflush(stderr);
        }
        for (IHostEventListener* l : snapshot())
            l->onResize(backWidth, backHeight, minimized);
    }

    // The keyboard type travels with each key event (kCGKeyboardEventKeyboardType); LMGetKbdType
    // only reflects the last event, which need not be this keyboard's.
    static bool eventIsISO(NSEvent* event)
    {
        CGEventRef cg = event.CGEvent;
        const SInt16 type = cg ? static_cast<SInt16>(CGEventGetIntegerValueField(cg, kCGKeyboardEventKeyboardType))
                               : static_cast<SInt16>(LMGetKbdType());
        return KBGetLayoutType(type) == kLayoutTypeISO;
    }

    void flushCapsRelease()
    {
        if (!capsReleasePending)
            return;
        capsReleasePending = false;
        capsRelease.timeMs = CocoaHost::tickCountMs();
        for (IHostEventListener* l : snapshot())
            l->onKey(capsRelease);
    }

    void handleKey(NSEvent* event, bool down)
    {
        HostKeyEvent key;
        key.keyCode   = event.keyCode;
        key.down      = down;
        key.repeat    = down && event.isARepeat;
        key.isoLayout = eventIsISO(event);
        key.modifiers = modifiers = static_cast<unsigned int>(event.modifierFlags);
        key.timeMs    = msFromSeconds(event.timestamp);
        updateAsyncEnter(key.keyCode, key.down, key.repeat);
        for (IHostEventListener* l : snapshot())
            l->onKey(key);
        if (!down)
            return;
        HostTextEvent text;
        text.keyCode   = event.keyCode;
        text.modifiers = key.modifiers;
        text.repeat    = key.repeat;
        NSString* chars = event.characters;
        for (NSUInteger i = 0; i < chars.length; ++i)
        {
            const unichar c = [chars characterAtIndex:i];
            if (c >= 0xF700 && c <= 0xF8FF)          // NSUpArrowFunctionKey and friends
                continue;
            text.characters.push_back(c);
        }
        if (text.characters.empty())
            return;
        for (IHostEventListener* l : snapshot())
            l->onText(text);
    }

    // Modifier keys: flagsChanged reports the new flag set; the device-dependent bit of the key
    // that changed tells press from release.
    void handleFlags(NSEvent* event)
    {
        const unsigned int flags = static_cast<unsigned int>(event.modifierFlags);
        const unsigned short keyCode = event.keyCode;
        const unsigned int previous = modifiers;
        modifiers = flags;
        HostKeyEvent key;
        key.keyCode   = keyCode;
        key.repeat    = false;
        key.isoLayout = eventIsISO(event);
        key.modifiers = flags;
        key.timeMs    = msFromSeconds(event.timestamp);

        unsigned int deviceMask = 0;
        unsigned int genericMask = 0;
        switch (keyCode)
        {
        case 0x38: deviceMask = HostModLeftShift;    genericMask = HostModShift;   break;
        case 0x3C: deviceMask = HostModRightShift;   genericMask = HostModShift;   break;
        case 0x3B: deviceMask = HostModLeftControl;  genericMask = HostModControl; break;
        case 0x3E: deviceMask = HostModRightControl; genericMask = HostModControl; break;
        case 0x3A: deviceMask = HostModLeftOption;   genericMask = HostModOption;  break;
        case 0x3D: deviceMask = HostModRightOption;  genericMask = HostModOption;  break;
        case 0x37: deviceMask = HostModLeftCommand;  genericMask = HostModCommand; break;
        case 0x36: deviceMask = HostModRightCommand; genericMask = HostModCommand; break;
        case 0x3F: genericMask = HostModFunction; break;
        case 0x39:                                   // Caps Lock: a lock-state toggle
            if ((flags ^ previous) & HostModCapsLock)
            {
                flushCapsRelease();
                key.down = true;
                for (IHostEventListener* l : snapshot())
                    l->onKey(key);
                capsRelease = key;
                capsRelease.down = false;
                capsReleasePending = true;
            }
            return;
        default:
            return;
        }
        bool down;
        if (deviceMask != 0 && ((flags | previous) & 0x207Fu) != 0)
            down = (flags & deviceMask) != 0;
        else                                         // no device bits: fall back to the generic flag
            down = (flags & genericMask) != 0;
        key.down = down;
        for (IHostEventListener* l : snapshot())
            l->onKey(key);
    }

    void handleMouseMove(NSEvent* event)
    {
        HostMouseMoveEvent move;
        move.dx = static_cast<float>(event.deltaX);
        move.dy = static_cast<float>(event.deltaY);
        move.timeMs = msFromSeconds(event.timestamp);
        move.captured = relativeMouse && hasFocus();
        locate(event, move.x, move.y, move.insideView);
        if (move.insideView != mouseInView)
        {
            mouseInView = move.insideView;
            applyCursor();
        }
        for (IHostEventListener* l : snapshot())
            l->onMouseMove(move);
    }

    void handleMouseButton(NSEvent* event, bool down)
    {
        HostMouseButtonEvent button;
        button.button = static_cast<int>(event.buttonNumber);
        button.down = down;
        button.clickCount = static_cast<int>(event.clickCount);
        button.timeMs = msFromSeconds(event.timestamp);
        button.captured = relativeMouse && hasFocus();
        bool inside = false;
        locate(event, button.x, button.y, inside);
        if (button.button < 0 || button.button > 31)
            return;
        const unsigned int bit = 1u << button.button;
        if (down)
        {
            // A press on the title bar or outside the view is window management, not game input.
            if (!inside && !relativeMouse)
                return;
            buttonsDown |= bit;
        }
        else
        {
            if (!(buttonsDown & bit))
                return;
            buttonsDown &= ~bit;
        }
        for (IHostEventListener* l : snapshot())
            l->onMouseButton(button);
    }

    // DirectInput's wheel axis reports whole notches of WHEEL_DELTA (120). Line-based wheels give
    // deltaY in lines (accelerated); trackpads and continuous wheels give scrollingDeltaY in points,
    // counted as a notch per 10 points. Fractions of a notch are carried until a whole notch has
    // accumulated, so a trackpad swipe reads as the notches it amounts to rather than as a small
    // wheel value on every input sample. Momentum-phase events are the inertia AppKit synthesizes
    // after the fingers lift, not device motion, and are dropped; a new gesture starts from zero.
    void handleWheel(NSEvent* event)
    {
        if (event.momentumPhase != NSEventPhaseNone)
            return;
        if (event.phase & NSEventPhaseBegan)
            wheelRemainder = 0.0;
        double delta = event.hasPreciseScrollingDeltas ? event.scrollingDeltaY * 12.0
                                                       : event.deltaY * 120.0;
        if (event.isDirectionInvertedFromDevice)
            delta = -delta;
        delta += wheelRemainder;
        const double whole = std::trunc(delta / 120.0) * 120.0;
        wheelRemainder = delta - whole;
        if (whole == 0.0)
            return;
        HostMouseWheelEvent wheel;
        wheel.delta = static_cast<int>(whole);
        wheel.captured = relativeMouse && hasFocus();
        wheel.timeMs = msFromSeconds(event.timestamp);
        for (IHostEventListener* l : snapshot())
            l->onMouseWheel(wheel);
    }

    // Routes one event to the listeners before AppKit dispatches it. Only events for the focused
    // game window are game input; everything else goes to AppKit alone.
    void route(NSEvent* event)
    {
        if (!window && !testHeadless)
            return;
        if (scripted)
            return;                              // [TEMP] scripted runs ignore the real devices
        const bool focus = hasFocus();
        switch (event.type)
        {
        case NSEventTypeKeyDown:
            if (focus) handleKey(event, true);
            break;
        case NSEventTypeKeyUp:
            if (focus) handleKey(event, false);
            break;
        case NSEventTypeFlagsChanged:
            if (focus) handleFlags(event);
            else modifiers = static_cast<unsigned int>(event.modifierFlags);
            break;
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
            if (focus) handleMouseMove(event);
            break;
        case NSEventTypeLeftMouseDown:
        case NSEventTypeRightMouseDown:
        case NSEventTypeOtherMouseDown:
            if (focus) handleMouseButton(event, true);
            break;
        case NSEventTypeLeftMouseUp:
        case NSEventTypeRightMouseUp:
        case NSEventTypeOtherMouseUp:
            handleMouseButton(event, false);       // releases after a focus change still pair up
            break;
        case NSEventTypeScrollWheel:
            if (focus) handleWheel(event);
            break;
        default:
            break;
        }
    }

    // Hands the GCMouse samples published since the last pump to the listeners, on this (the
    // engine) thread. Without focus they are discarded, as the routed NSEvents are.
    void drainRawMouse()
    {
        RawMouseSample samples[256];
        for (;;)
        {
            const std::size_t n = rawMouseDrain(samples, 256);
            if (n == 0)
                break;
            if (!hasFocus() || scripted)
                continue;
            for (std::size_t i = 0; i < n; ++i)
            {
                HostRawMouseEvent raw;
                raw.dx = samples[i].dx;
                raw.dy = samples[i].dy;
                raw.timeMs = samples[i].timeMs;
                for (IHostEventListener* l : snapshot())
                    l->onRawMouseMove(raw);
            }
        }
    }

    bool pump(double timeout)
    {
        if (!appInitialized)
            return quit;
        flushCapsRelease();
        @autoreleasepool
        {
            NSDate* until = timeout > 0.0 ? [NSDate dateWithTimeIntervalSinceNow:timeout]
                                          : [NSDate distantPast];
            for (;;)
            {
                NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                    untilDate:until
                                                       inMode:NSDefaultRunLoopMode
                                                      dequeue:YES];
                if (!event)
                    break;
                route(event);
                [NSApp sendEvent:event];
                until = [NSDate distantPast];
            }
            [NSApp updateWindows];
            updateActivation();
            updateCursorImage();
        }
        drainRawMouse();
        if (scripted)
            inputScript().tick(*this);
        return quit;
    }

    // [TEMP] IInputScriptSink: the scripted counterparts of handleKey/handleFlags/handleMouse*/
    // handleWheel/drainRawMouse, built with the same fields those fill from an NSEvent.
    void scriptKey(unsigned short keyCode, bool down, const std::u16string& text) override
    {
        unsigned int deviceMask = 0, genericMask = 0;
        switch (keyCode)
        {
        case 0x38: deviceMask = HostModLeftShift;    genericMask = HostModShift;   break;
        case 0x3C: deviceMask = HostModRightShift;   genericMask = HostModShift;   break;
        case 0x3B: deviceMask = HostModLeftControl;  genericMask = HostModControl; break;
        case 0x3E: deviceMask = HostModRightControl; genericMask = HostModControl; break;
        case 0x3A: deviceMask = HostModLeftOption;   genericMask = HostModOption;  break;
        case 0x3D: deviceMask = HostModRightOption;  genericMask = HostModOption;  break;
        case 0x37: deviceMask = HostModLeftCommand;  genericMask = HostModCommand; break;
        case 0x36: deviceMask = HostModRightCommand; genericMask = HostModCommand; break;
        default: break;
        }
        if (deviceMask)
        {
            if (down) modifiers |= deviceMask | genericMask;
            else
            {
                modifiers &= ~deviceMask;
                // The generic flag stays while the other side's key is held.
                const unsigned int sides = genericMask == HostModShift   ? (HostModLeftShift | HostModRightShift)
                                         : genericMask == HostModControl ? (HostModLeftControl | HostModRightControl)
                                         : genericMask == HostModOption  ? (HostModLeftOption | HostModRightOption)
                                                                         : (HostModLeftCommand | HostModRightCommand);
                if (!(modifiers & sides)) modifiers &= ~genericMask;
            }
        }
        HostKeyEvent key;
        key.keyCode   = keyCode;
        key.down      = down;
        key.repeat    = false;
        key.isoLayout = false;                   // scripts name ANSI positions
        key.modifiers = modifiers;
        key.timeMs    = CocoaHost::tickCountMs();
        updateAsyncEnter(key.keyCode, key.down, key.repeat);
        for (IHostEventListener* l : snapshot())
            l->onKey(key);
        if (!down || deviceMask)
            return;
        HostTextEvent chars;
        chars.keyCode   = keyCode;
        chars.modifiers = modifiers;
        chars.repeat    = false;
        for (char16_t c : text)
            if (!(c >= 0xF700 && c <= 0xF8FF))
                chars.characters.push_back(static_cast<unsigned short>(c));
        if (chars.characters.empty())
            return;
        for (IHostEventListener* l : snapshot())
            l->onText(chars);
    }

    void scriptPointerInit()
    {
        if (scriptPointerSet)
            return;
        scriptPointerSet = true;
        scriptX = backWidth * 0.5f;
        scriptY = backHeight * 0.5f;
    }

    void scriptMove(float x, float y, float dx, float dy)
    {
        HostMouseMoveEvent move;
        move.dx = dx;
        move.dy = dy;
        move.x = x;
        move.y = y;
        move.insideView = x >= 0 && y >= 0 && x < backWidth && y < backHeight;
        move.captured = relativeMouse && hasFocus();
        move.timeMs = CocoaHost::tickCountMs();
        mouseInView = move.insideView;
        for (IHostEventListener* l : snapshot())
            l->onMouseMove(move);
    }

    void scriptMouseTo(float x, float y) override
    {
        scriptPointerInit();
        const float dx = x - scriptX, dy = y - scriptY;
        scriptX = x;
        scriptY = y;
        scriptMove(x, y, dx, dy);
    }

    void scriptMouseBy(float dx, float dy) override
    {
        scriptPointerInit();
        if (!relativeMouse)
        {
            scriptX += dx;
            scriptY += dy;
        }
        scriptMove(scriptX, scriptY, dx, dy);    // the parked cursor stays put in relative mode
        if (hasFocus() && rawMouseAvailable())   // a GCMouse reports the same motion raw
        {
            HostRawMouseEvent raw;
            raw.dx = dx;
            raw.dy = dy;
            raw.timeMs = CocoaHost::tickCountMs();
            for (IHostEventListener* l : snapshot())
                l->onRawMouseMove(raw);
        }
    }

    void scriptButton(int number, bool down) override
    {
        scriptPointerInit();
        if (number < 0 || number > 31)
            return;
        const unsigned int bit = 1u << number;
        if (down) buttonsDown |= bit;
        else
        {
            if (!(buttonsDown & bit))
                return;
            buttonsDown &= ~bit;
        }
        HostMouseButtonEvent button;
        button.button = number;
        button.down = down;
        button.clickCount = 1;
        button.x = scriptX;
        button.y = scriptY;
        button.captured = relativeMouse && hasFocus();
        button.timeMs = CocoaHost::tickCountMs();
        for (IHostEventListener* l : snapshot())
            l->onMouseButton(button);
    }

    void scriptWheel(int delta) override
    {
        HostMouseWheelEvent wheel;
        wheel.delta = delta;
        wheel.captured = relativeMouse && hasFocus();
        wheel.timeMs = CocoaHost::tickCountMs();
        for (IHostEventListener* l : snapshot())
            l->onMouseWheel(wheel);
    }

    std::u16string scriptKeyCharacter(unsigned short keyCode, bool shift) override
    {
        // [TEMP] Scripts name ANSI positions (isoLayout = false in the scripted key path), but
        // keyCharacter translates with the machine's keyboard type. On an ISO keyboard macOS swaps
        // the two codes: the key left of 1 is kVK_ISO_Section (0x0A), the key beside left Shift
        // kVK_ANSI_Grave (0x32). Swap them back so a scripted key's character belongs to the same
        // physical key as its DIK (`grave`: DIK_GRAVE and the console key's own character, which
        // BF2Console::updateConsole swallows when it is U+00A7, as retail 0x812E540 does).
        CocoaHost& host = CocoaHost::instance();
        if (host.isISOKeyboard() && (keyCode == 0x0A || keyCode == 0x32))
            keyCode = keyCode == 0x0A ? 0x32 : 0x0A;
        return host.keyCharacter(keyCode, shift ? HostModShift : 0u);
    }

    void scriptQuit() override
    {
        if (!quit)
            requestClose();
    }

    static bool rawMouseAvailable() { return rawMouseCount() > 0; }
};

} // namespace macos
} // namespace hfe
} // namespace dice

@implementation OpenBF2HostDelegate
{
@public
    dice::hfe::macos::CocoaHost::Impl* impl;
}
- (void)syncFocus
{
    NSWindow* window = impl ? (NSWindow*)impl->window : nil;
    if (!impl)
        return;
    if (impl->systemActive)
    {
        // Leave the fallback once AppKit has caught up, or once another application is in front.
        NSRunningApplication* f = [[NSWorkspace sharedWorkspace] frontmostApplication];
        if ([NSApp isActive] || !f || f.processIdentifier != getpid())
            impl->systemActive = false;
    }
    const bool active = [NSApp isActive] || impl->systemActive;
    const bool key = window != nil && (window.isKeyWindow || (impl->systemActive && window.isVisible));
    impl->setFocusState(key, active);
}
- (BOOL)windowShouldClose:(NSWindow*)sender
{
    (void)sender;
    CocoaHost::instance().requestQuit();
    return NO;                                   // the engine tears the window down itself
}
- (void)windowDidBecomeKey:(NSNotification*)note     { (void)note; [self syncFocus]; }
- (void)windowDidResignKey:(NSNotification*)note     { (void)note; [self syncFocus]; }
- (void)windowDidMiniaturize:(NSNotification*)note   { (void)note; if (impl) { impl->minimized = true;  impl->notifyResize(); } }
- (void)windowDidDeminiaturize:(NSNotification*)note { (void)note; if (impl) { impl->minimized = false; impl->notifyResize(); } }
- (void)applicationDidBecomeActive:(NSNotification*)note
{
    (void)note;
    if (impl)
        impl->becameActive();
    [self syncFocus];
}
- (void)applicationDidResignActive:(NSNotification*)note { (void)note; [self syncFocus]; }
- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication*)sender
{
    (void)sender;
    CocoaHost::instance().requestQuit();
    return NSTerminateCancel;                    // shutdown runs through the engine, not exit()
}
- (void)quitRequested:(id)sender
{
    (void)sender;
    CocoaHost::instance().requestQuit();
}
@end

namespace dice {
namespace hfe {
namespace macos {

} // namespace macos
} // namespace hfe
} // namespace dice

namespace {
CocoaHost::Impl* s_hostImpl = nullptr;   // the singleton's state, for the view's cursor rect
NSCursor* currentViewCursor()
{
    return s_hostImpl ? s_hostImpl->rectCursor : nil;
}
} // namespace

namespace dice {
namespace hfe {
namespace macos {

CocoaHost& CocoaHost::instance()
{
    static CocoaHost* host = new CocoaHost();    // lives for the process, like the Win32 class
    return *host;
}

CocoaHost::CocoaHost() : m_impl(new Impl()) { s_hostImpl = m_impl; m_impl->owner = this; }
CocoaHost::~CocoaHost() { delete m_impl; }

bool CocoaHost::initApplication()
{
    if (m_impl->appInitialized)
        return true;
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        // [CHOSEN, not recovered] The client is a helper executable inside the launcher's bundle
        // (Contents/MacOS/bf2_client), not the bundle's CFBundleExecutable, so the Dock would show
        // a generic icon for it. Use the enclosing bundle's icon when there is one.
        {
            NSBundle* bundle = [NSBundle mainBundle];
            NSString* iconName = [bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
            if ([bundle.bundlePath hasSuffix:@".app"] && iconName.length > 0)
            {
                NSImage* icon = [bundle imageForResource:iconName];
                if (icon)
                    [NSApp setApplicationIconImage:icon];
            }
        }
        m_impl->delegate = [[OpenBF2HostDelegate alloc] init];
        m_impl->delegate->impl = m_impl;
        [NSApp setDelegate:m_impl->delegate];

        NSMenu* menuBar = [[NSMenu alloc] init];
        NSMenuItem* appItem = [[NSMenuItem alloc] init];
        [menuBar addItem:appItem];
        NSMenu* appMenu = [[NSMenu alloc] init];
        NSString* name = [[NSProcessInfo processInfo] processName];
        [appMenu addItemWithTitle:[@"Hide " stringByAppendingString:name]
                           action:@selector(hide:) keyEquivalent:@"h"];
        [appMenu addItem:[NSMenuItem separatorItem]];
        NSMenuItem* quitItem = [appMenu addItemWithTitle:[@"Quit " stringByAppendingString:name]
                                                  action:@selector(quitRequested:)
                                           keyEquivalent:@"q"];
        quitItem.target = m_impl->delegate;
        appItem.submenu = appMenu;
        [NSApp setMainMenu:menuBar];

        [NSApp finishLaunching];
        m_impl->scripted = inputScript().loadFromEnvironment();   // [TEMP] BF2_INPUT_SCRIPT
        {
            const char* realFocus = std::getenv("BF2_INPUT_SCRIPT_REAL_FOCUS");
            m_impl->scriptedFocus = m_impl->scripted && !(realFocus && realFocus[0] == '1');
        }
        rawMouseStart();                         // GCMouse connections, observed for the run
    }
    m_impl->appInitialized = true;
    m_impl->appActive = (m_impl->scriptedFocus || [NSApp isActive]) ? true : false;
    return true;
}

bool CocoaHost::createWindow(const HostWindowDesc& desc)
{
    if (!initApplication() || m_impl->window)
        return false;
    @autoreleasepool
    {
        NSScreen* screen = [NSScreen mainScreen];
        if (!screen)
            return false;
        const NSRect screenFrame = screen.frame;
        const int width  = desc.width  > 0 ? desc.width  : 800;
        const int height = desc.height > 0 ? desc.height : 600;

        const NSWindowStyleMask windowedStyle =
            NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
        const NSSize points = Impl::windowedContentSize(width, height, screen, windowedStyle);
        const NSRect content = NSMakeRect(0, 0, points.width, points.height);
        OpenBF2HostWindow* window =
            [[OpenBF2HostWindow alloc] initWithContentRect:content
                                                 styleMask:windowedStyle
                                                   backing:NSBackingStoreBuffered
                                                     defer:NO];
        if (!window)
            return false;
        window.releasedWhenClosed = NO;
        window.title = [NSString stringWithUTF8String:desc.title.c_str()] ?: @"BF2";
        window.backgroundColor = [NSColor blackColor];
        window.acceptsMouseMovedEvents = YES;
        window.collectionBehavior = NSWindowCollectionBehaviorFullScreenNone;
        window.delegate = m_impl->delegate;
        // Win32 positions are the frame's top left from the screen's top left.
        [window setFrameTopLeftPoint:NSMakePoint(NSMinX(screenFrame) + desc.x,
                                                 NSMaxY(screenFrame) - desc.y)];

        OpenBF2HostView* view = [[OpenBF2HostView alloc] initWithFrame:content];
        view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        window.contentView = view;

        m_impl->window = window;
        m_impl->view = view;
        m_impl->layer = static_cast<CAMetalLayer*>(view.layer);
        m_impl->windowedFrame = window.frame;
        m_impl->backWidth = width;
        m_impl->backHeight = height;
        m_impl->fullscreen = false;
        m_impl->quit = false;
        m_impl->cursorHiddenInView = desc.hideCursor;
        m_impl->updateDrawable();

        // [CHOSEN, not recovered] A fullscreen start goes fullscreen only once the application is
        // active: presentation options and the screen-size borderless window applied while a
        // freshly launched process is still inactive leave it frontmost without NSApp ever
        // becoming active, and the menu ignores clicks until the user switches apps. Until then
        // the window stays windowed; becameActive() completes the switch.
        if (desc.fullscreen)
        {
            if ([NSApp isActive] || m_impl->scriptedFocus)
                setFullscreen(true);
            else
                m_impl->fullscreenPending = true;
        }

        m_impl->visible = true;
        m_impl->beginActivation();               // [TEMP] not in scripted runs (scriptedFocus)
        [window makeKeyAndOrderFront:nil];
        [window makeFirstResponder:view];
    }
    pumpEvents();                                // deliver the activation before returning
    [m_impl->delegate syncFocus];
    return true;
}

void CocoaHost::destroyWindow()
{
    if (!m_impl->window)
        return;
    m_impl->relativeMouse = false;
    m_impl->cursorCounter = 0;
    m_impl->cursorHiddenInView = false;
    @autoreleasepool
    {
        if (m_impl->fullscreen)
            [NSApp setPresentationOptions:NSApplicationPresentationDefault];
        m_impl->window.delegate = nil;
        [m_impl->window orderOut:nil];
        [m_impl->window close];
        m_impl->window = nil;
        m_impl->view = nil;
        m_impl->layer = nil;
    }
    m_impl->fullscreen = false;
    m_impl->visible = false;
    m_impl->windowKey = false;
    m_impl->buttonsDown = 0;
    m_impl->applyCursor();                       // window gone: unhide and re-associate
}

bool  CocoaHost::hasWindow() const    { return m_impl->window != nil; }
void* CocoaHost::nativeWindow() const { return (__bridge void*)m_impl->window; }
void* CocoaHost::nativeView() const   { return (__bridge void*)m_impl->view; }
void* CocoaHost::metalLayer() const   { return (__bridge void*)m_impl->layer; }
int   CocoaHost::clientWidth() const  { return m_impl->backWidth; }
int   CocoaHost::clientHeight() const { return m_impl->backHeight; }

void CocoaHost::show(bool visible)
{
    if (!m_impl->window)
        return;
    if (visible)
    {
        [m_impl->window makeKeyAndOrderFront:nil];
        [m_impl->window makeFirstResponder:m_impl->view];
    }
    else
    {
        [m_impl->window orderOut:nil];
    }
    m_impl->visible = visible;
}

bool CocoaHost::isVisible() const { return m_impl->window != nil && m_impl->visible; }

void CocoaHost::minimize()
{
    if (m_impl->window)
        [m_impl->window miniaturize:nil];
}

void CocoaHost::setTitle(const std::string& title)
{
    if (m_impl->window)
        m_impl->window.title = [NSString stringWithUTF8String:title.c_str()] ?: @"";
}

void CocoaHost::resizeClient(int width, int height)
{
    if (!m_impl->window || width <= 0 || height <= 0)
        return;
    m_impl->backWidth = width;
    m_impl->backHeight = height;
    if (!m_impl->fullscreen)
    {
        const NSRect frame = m_impl->window.frame;
        const NSPoint topLeft = NSMakePoint(NSMinX(frame), NSMaxY(frame));
        NSScreen* screen = m_impl->window.screen ?: [NSScreen mainScreen];
        [m_impl->window setContentSize:Impl::windowedContentSize(width, height, screen,
                                                                  m_impl->window.styleMask)];
        [m_impl->window setFrameTopLeftPoint:topLeft];
        m_impl->windowedFrame = m_impl->window.frame;
    }
    m_impl->updateDrawable();
    m_impl->notifyResize();
}

void CocoaHost::setFullscreen(bool fullscreen)
{
    if (!m_impl->window || fullscreen == m_impl->fullscreen)
        return;
    OpenBF2HostWindow* window = m_impl->window;
    if (fullscreen)
    {
        m_impl->windowedFrame = window.frame;
        NSScreen* screen = window.screen ?: [NSScreen mainScreen];
        window.styleMask = NSWindowStyleMaskBorderless;
        [window setFrame:screen.frame display:YES];
        [NSApp setPresentationOptions:NSApplicationPresentationHideDock |
                                      NSApplicationPresentationHideMenuBar];
    }
    else
    {
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
        window.styleMask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                           NSWindowStyleMaskMiniaturizable;
        [window setFrame:m_impl->windowedFrame display:YES];
        // A resolution change made while full screen applies to the restored window too.
        const NSPoint topLeft = NSMakePoint(NSMinX(window.frame), NSMaxY(window.frame));
        [window setContentSize:Impl::windowedContentSize(m_impl->backWidth, m_impl->backHeight,
                                                         window.screen ?: [NSScreen mainScreen],
                                                         window.styleMask)];
        [window setFrameTopLeftPoint:topLeft];
        m_impl->windowedFrame = window.frame;
    }
    m_impl->fullscreen = fullscreen;
    [window makeKeyAndOrderFront:nil];
    [window makeFirstResponder:m_impl->view];
    m_impl->updateDrawable();
    if (m_impl->mouseDisassociated)
        m_impl->warpToCentre();
    m_impl->notifyResize();
}

bool CocoaHost::isFullscreen() const { return m_impl->fullscreen; }
bool CocoaHost::hasFocus() const     { return m_impl->hasFocus(); }
bool CocoaHost::isAppActive() const  { return m_impl->appActive; }
unsigned int CocoaHost::deactivationCount() const { return m_impl->deactivations; }

bool CocoaHost::pumpEvents()                     { return m_impl->pump(0.0); }
bool CocoaHost::waitEvents(double timeoutSeconds){ return m_impl->pump(timeoutSeconds); }
bool CocoaHost::quitRequested() const            { return m_impl->quit; }
void CocoaHost::postQuitMessage()                { m_impl->quit = true; }

void CocoaHost::requestQuit()
{
    if (!m_impl->quit)
        m_impl->requestClose();
}

void CocoaHost::addListener(IHostEventListener* listener)
{
    if (listener && std::find(m_impl->listeners.begin(), m_impl->listeners.end(), listener)
                    == m_impl->listeners.end())
        m_impl->listeners.push_back(listener);
}

void CocoaHost::removeListener(IHostEventListener* listener)
{
    m_impl->listeners.erase(std::remove(m_impl->listeners.begin(), m_impl->listeners.end(), listener),
                            m_impl->listeners.end());
}

int CocoaHost::showCursor(bool show)
{
    m_impl->cursorCounter += show ? 1 : -1;
    m_impl->applyCursor();
    return m_impl->cursorCounter;
}

void CocoaHost::setCursorHiddenInView(bool hidden)
{
    m_impl->cursorHiddenInView = hidden;
    m_impl->applyCursor();
}

void CocoaHost::setCursorShape(HostCursorShape shape)
{
    // AppKit has no public busy cursor; both shapes show the arrow (docs/macos-s2-4.md).
    setCursor(cursorHandle(shape));
}

void* CocoaHost::cursorHandle(HostCursorShape shape)
{
    static HostCursor shapes[2];
    return &shapes[shape == HostCursorBusy ? 1 : 0];
}

void* CocoaHost::loadCursorFromFile(const char* path)
{
    if (!path || !*path)
        return nullptr;
    FILE* file = std::fopen(path, "rb");
    if (!file)
        return nullptr;                          // LoadCursorFromFileA answers null
    std::vector<std::uint8_t> bytes;
    std::uint8_t chunk[16384];
    size_t got;
    while ((got = std::fread(chunk, 1, sizeof chunk, file)) > 0 && bytes.size() < (16u << 20))
        bytes.insert(bytes.end(), chunk, chunk + got);
    std::fclose(file);
    std::unique_ptr<HostCursor> cursor(new HostCursor());
    if (!decodeCursorFile(bytes.data(), bytes.size(), cursor->anim))
    {
        std::fprintf(stderr, "[host] cursor %s: not a decodable .ani/.cur, the arrow is shown\n", path);
        return cursorHandle(HostCursorArrow);
    }
    cursor->custom = true;
    for (int j : cursor->anim.jiffies)
        cursor->totalJiffies += j;
    static std::vector<std::unique_ptr<HostCursor>> loaded;   // tokens live for the process
    loaded.push_back(std::move(cursor));
    return loaded.back().get();
}

void CocoaHost::setCursor(void* handle)
{
    if (!handle)
    {
        m_impl->activeCursor = nullptr;
        setCursorHiddenInView(true);
        m_impl->updateCursorImage();
        return;
    }
    HostCursor* cursor = static_cast<HostCursor*>(handle);
    if (cursor != m_impl->activeCursor)
    {
        // SetCursor with the current handle keeps an animation running; a new one starts it.
        m_impl->activeCursor = cursor;
        m_impl->cursorAnimStart = [NSProcessInfo processInfo].systemUptime;
        m_impl->cursorShape = cursor == cursorHandle(HostCursorBusy) ? HostCursorBusy : HostCursorArrow;
    }
    setCursorHiddenInView(false);
    m_impl->updateCursorImage();
}

void CocoaHost::setRelativeMouseMode(bool enabled)
{
    m_impl->relativeMouse = enabled;
    m_impl->applyCursor();
}

bool CocoaHost::relativeMouseMode() const { return m_impl->relativeMouse; }

unsigned short CocoaHost::consumeAsyncEnterState()
{
    // [CHOSEN, not recovered] Cocoa supplies the consuming press bit. Quartz
    // supplies hardware state independently of gameswf delivery/focus. Scripted
    // and headless tests read their injected keys instead of the real keyboard.
    const bool held = (m_impl->scripted || m_impl->testHeadless)
        ? (m_impl->enterHeld[0] || m_impl->enterHeld[1])
        : (CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_Return)
            || CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, kVK_ANSI_KeypadEnter));
    const unsigned short state = (held ? 0x8000 : 0) | (m_impl->enterPressed ? 1 : 0);
    m_impl->enterPressed = false;
    return state;
}
bool CocoaHost::rawMouseAvailable() const { return rawMouseCount() > 0; }

unsigned int CocoaHost::modifierFlags() const
{
    return static_cast<unsigned int>([NSEvent modifierFlags]);
}

bool CocoaHost::isISOKeyboard() const
{
    return KBGetLayoutType(static_cast<SInt16>(LMGetKbdType())) == kLayoutTypeISO;
}

std::u16string CocoaHost::keyCharacter(unsigned short keyCode, unsigned int modifiers) const
{
    // UCKeyTranslate takes the Carbon modifier byte (shiftKey >> 8, alphaLock >> 8).
    UInt32 modifierState = 0;
    if (modifiers & HostModShift)    modifierState |= (shiftKey >> 8) & 0xFF;
    if (modifiers & HostModCapsLock) modifierState |= (alphaLock >> 8) & 0xFF;
    std::u16string result;
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source)
        return result;
    CFDataRef data = static_cast<CFDataRef>(
        TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    if (data)
    {
        const UCKeyboardLayout* layout =
            reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(data));
        UInt32 deadKeyState = 0;
        UniChar chars[8];
        UniCharCount length = 0;
        if (UCKeyTranslate(layout, keyCode, kUCKeyActionDisplay, modifierState, LMGetKbdType(),
                           kUCKeyTranslateNoDeadKeysBit, &deadKeyState, 8, &length, chars) == noErr)
        {
            for (UniCharCount i = 0; i < length; ++i)
                if (chars[i] >= 0x20 && chars[i] != 0x7F)
                    result.push_back(static_cast<char16_t>(chars[i]));
        }
    }
    CFRelease(source);
    return result;
}

bool CocoaHost::setClipboardText(const std::string& text)
{
    @autoreleasepool
    {
        NSString* string = [[NSString alloc] initWithBytes:text.data()
                                                    length:text.size()
                                                  encoding:NSWindowsCP1252StringEncoding];
        if (!string)
            return false;
        NSPasteboard* board = [NSPasteboard generalPasteboard];
        [board clearContents];
        return [board setString:string forType:NSPasteboardTypeString] ? true : false;
    }
}

bool CocoaHost::getClipboardText(std::string& text) const
{
    @autoreleasepool
    {
        NSString* string = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        if (!string)
            return false;
        NSData* bytes = [string dataUsingEncoding:NSWindowsCP1252StringEncoding
                             allowLossyConversion:YES];
        if (!bytes)
            return false;
        text.assign(static_cast<const char*>(bytes.bytes), bytes.length);
        return true;
    }
}

// [CHOSEN, not recovered] Core Graphics reports 0 Hz for the modes of built-in panels; their
// real rate is the screen's maximum frame rate (60 on non-ProMotion panels).
static int builtInPanelRefreshHz()
{
    if (@available(macOS 12.0, *))
    {
        NSScreen* screen = [NSScreen mainScreen];
        if (screen && screen.maximumFramesPerSecond > 0)
            return static_cast<int>(screen.maximumFramesPerSecond);
    }
    return 0;
}

std::vector<HostDisplayModeInfo> CocoaHost::displayModes() const
{
    std::vector<HostDisplayModeInfo> modes;
    const void* keys[] = { kCGDisplayShowDuplicateLowResolutionModes };
    const void* values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(0, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef list = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), options);
    CFRelease(options);
    if (!list)
        return modes;
    for (CFIndex i = 0; i < CFArrayGetCount(list); ++i)
    {
        CGDisplayModeRef mode = (CGDisplayModeRef)CFArrayGetValueAtIndex(list, i);
        if (!CGDisplayModeIsUsableForDesktopGUI(mode))
            continue;
        HostDisplayModeInfo info;
        info.width = static_cast<int>(CGDisplayModeGetPixelWidth(mode));
        info.height = static_cast<int>(CGDisplayModeGetPixelHeight(mode));
        info.refreshHz = static_cast<int>(std::lround(CGDisplayModeGetRefreshRate(mode)));
        if (info.refreshHz == 0)
            info.refreshHz = builtInPanelRefreshHz();
        info.bitsPerPixel = 32;
        bool duplicate = false;
        for (const HostDisplayModeInfo& m : modes)
            duplicate = duplicate || (m.width == info.width && m.height == info.height &&
                                      m.refreshHz == info.refreshHz);
        if (!duplicate)
            modes.push_back(info);
    }
    CFRelease(list);
    std::sort(modes.begin(), modes.end(), [](const HostDisplayModeInfo& a, const HostDisplayModeInfo& b) {
        if (a.width != b.width) return a.width < b.width;
        if (a.height != b.height) return a.height < b.height;
        return a.refreshHz < b.refreshHz;
    });
    return modes;
}

HostDisplayModeInfo CocoaHost::desktopMode() const
{
    HostDisplayModeInfo info = { 0, 0, 0, 32 };
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
    if (mode)
    {
        // [PLATFORM] Points, not pixels (see the declaration).
        info.width = static_cast<int>(CGDisplayModeGetWidth(mode));
        info.height = static_cast<int>(CGDisplayModeGetHeight(mode));
        info.refreshHz = static_cast<int>(std::lround(CGDisplayModeGetRefreshRate(mode)));
        if (info.refreshHz == 0)
            info.refreshHz = builtInPanelRefreshHz();
        CGDisplayModeRelease(mode);
    }
    return info;
}

void CocoaHost::preventDisplaySleep(bool prevent)
{
    if (prevent == m_impl->sleepAssertionHeld)
        return;
    if (prevent)
    {
        m_impl->sleepAssertionHeld = IOPMAssertionCreateWithName(
            kIOPMAssertionTypePreventUserIdleDisplaySleep, kIOPMAssertionLevelOn,
            CFSTR("OpenBF2 game window"), &m_impl->sleepAssertion) == kIOReturnSuccess;
    }
    else
    {
        IOPMAssertionRelease(m_impl->sleepAssertion);
        m_impl->sleepAssertionHeld = false;
    }
}

bool CocoaHost::openURL(const std::string& url)
{
    @autoreleasepool
    {
        NSString* string = [NSString stringWithUTF8String:url.c_str()];
        if (!string)
            return false;
        NSURL* target = [NSURL URLWithString:string];
        if (!target || !target.scheme)
            target = [NSURL fileURLWithPath:string];
        return [[NSWorkspace sharedWorkspace] openURL:target] ? true : false;
    }
}

#if defined(OPENBF2_HOST_TEST_SEAMS)
void CocoaHost::overrideFocusForTest(bool windowKey, bool appActive)
{
    m_impl->setFocusState(windowKey, appActive);
}

void CocoaHost::enableHeadlessForTest()
{
    m_impl->testHeadless = true;
}

void CocoaHost::injectEventForTest(void* nsEvent)
{
    @autoreleasepool
    {
        m_impl->flushCapsRelease();             // pumpEvents' order: pending Caps Lock release first
        m_impl->route((__bridge NSEvent*)nsEvent);
    }
}

void CocoaHost::pumpRawMouseForTest()
{
    m_impl->drainRawMouse();
}

void CocoaHost::cursorStateForTest(bool& hidden, bool& disassociated) const
{
    hidden = m_impl->cursorHidden;
    disassociated = m_impl->mouseDisassociated;
}
#endif

unsigned int CocoaHost::tickCountMs()
{
    return msFromSeconds([[NSProcessInfo processInfo] systemUptime]);
}

} // namespace macos
} // namespace hfe
} // namespace dice
