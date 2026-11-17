// [TEMP] [CHOSEN, not recovered] Cocoa-only host fixture access; never part of the shared host.
// [RETIRE-WHEN: macos-client-window] Replace with equivalent native host/input coverage before removal.
#ifndef PLATFORM_MACOS_COCOAHOSTTESTHOOKS_H
#define PLATFORM_MACOS_COCOAHOSTTESTHOOKS_H

#if defined(__APPLE__) && defined(OPENBF2_HOST_TEST_SEAMS)
#include "../Host/ClientHost.h"

namespace dice { namespace hfe { namespace macos {

class CocoaHostTestHooks
{
public:
    // Borrowed Cocoa objects for the window fixture; null when the host has no window.
    static void* nativeWindow(host::ClientHost& host);
    static void* nativeView(host::ClientHost& host);
    static void* metalLayer(host::ClientHost& host);
    static bool isISOKeyboard(const host::ClientHost& host);

    // Simulates focus when AppKit activation is unavailable in the test session.
    static void overrideFocusForTest(host::ClientHost& host, bool windowKey, bool appActive);
    // Routes injected NSEvents without creating a window or touching the hardware cursor.
    static void enableHeadlessForTest(host::ClientHost& host);
    static void injectEventForTest(host::ClientHost& host, void* nsEvent);
    static void cursorStateForTest(const host::ClientHost& host, bool& hidden, bool& disassociated);
    static void pumpRawMouseForTest(host::ClientHost& host);
};

} } }
#endif
#endif
