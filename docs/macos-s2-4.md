# macOS port, package S2.4: Cocoa host and input

This package gives the macOS client a window, an event pump and keyboard, mouse and
game-controller input. It replaces `WinMain`'s message pump, the Win32 window class and the
DirectInput 8 devices, and leaves the engine above them unchanged or changed only behind
`__APPLE__` arms. It is an intentional port change requested by the project owner (no linked
issue; see [macos-port.md](macos-port.md)). Every file and arm described here is
`[CHOSEN, not recovered]`: there was no retail macOS image. Retail PE/ELF behaviour remains
the reference for everything above the platform seam.

Plan context: [macOSplan.md](../macOSplan.md), package S2.4. The client target (package S2.0)
does not link yet, so the host was developed against a standalone test app (see
[Test app](#test-app)).

## Status

| Item | State |
| --- | --- |
| Cocoa host (`NSApplication`, `NSWindow`, `CAMetalLayer` view, pump, focus, fullscreen, cursor, clipboard, display modes) | Implemented; window, pump, focus, fullscreen round trip, close and quit exercised by the test app |
| Keyboard (`DIK_` codes, modifiers, ANSI/ISO/JIS) | Implemented; synthetic key events exercised, including the ISO swap on an ISO keyboard |
| Mouse (relative deltas, eight buttons, wheel) | Implemented; synthetic motion, click and wheel exercised, non-exclusive and exclusive (relative) modes |
| Game controllers (`GCController`) | Implemented, compile-tested only: no controller was connected |
| Engine wiring: `BF2Window` on macOS, `BF2::handleInGameWinMessages`, message handler, client `main` | Implemented behind `__APPLE__ && !DEDICATED`; compiled with `DEDICATED=0` and `=1`, not linked (the client target does not link yet) |
| Remaining Win32 call sites in client TUs (cursor, clipboard, URL, renderer handle) | Done on `mac/s2-58-platform` for the client TUs (see [macos-port.md](macos-port.md#win32-and-crt-items-s25s28)); the two renderer rows wait for stage 3 |
| Clipboard, display-mode list, `openURL`, display-sleep assertion | Implemented, not exercised (the test app does not touch the user's clipboard) |

## Files

| File | Role |
| --- | --- |
| `Code/BF2/Platform/macOS/CocoaHost.h`, `.mm` | The host. A plain C++ header (no Objective-C types) over an ARC Objective-C++ implementation. |
| `Code/BF2/Platform/macOS/MacKeyMap.h`, `.cpp` | `kVK_` to `DIK_` table, reverse lookup, `DIK_` to Win32 virtual key, display names. |
| `Code/BF2/Platform/macOS/MacGamepad.h`, `.mm` | Polled `GCController` extended-gamepad state. |
| `Code/BF2/Platform/macOS/HostWindowTypes.h` | `HWND`/`UINT`/`WPARAM`/`LPARAM`/`LRESULT` and the Win32 message ids, scoped to `dice::hfe`, for `BF2Window.h` on macOS. |
| `Code/BF2/IO/Input/MacInputDeviceManager.h`, `.cpp` | The device manager, beside `DX8InputDeviceManager` and `NullInputDeviceManager`. |
| `Code/BF2/IO/Input/MacKeyboardDevice.cpp`, `MacMouseDevice.cpp`, `MacGameControllerDevice.cpp` | The three devices. |
| `Code/BF2/Game/Main/BF2Window_macos.cpp` | `Window`/`BF2Window` on the host (beside the Win32 `BF2Window.cpp`, which is now `_WIN32`-only). |
| `Code/BF2/Game/Main/BF2Window.h` | macOS arm of the includes and the `wndProc` declaration. |
| `Code/BF2/Game/Main/BF2.cpp`, `BF2.h` | macOS arms: pump, message handler, key rings, client `main`. |
| `Code/BF2/Game/Main/BF2Engine.cpp` | The menu key ring is flushed on macOS too (one `#if`). |
| `Code/Tests/macos_host/` | `[TEMP]` standalone test app with its own `CMakeLists.txt` and `CMakePresets.json`. |

### Why `Code/BF2/Platform/macOS`

The host has no retail counterpart and no attested source path, so placing it under
`Game/Main` or `IO/Input` would suggest a recovered location. It is also shared: the window
code (`Game/Main`) and the input devices (`IO/Input`) both use it, and it has no engine
dependency, which lets the test app build it alone. A new top-level `Platform/macOS`
directory keeps port code visibly separate from reconstructed code. The engine-facing classes
stay beside their Win32 counterparts, following the tree's `_win32`/`_posix` suffix and
`DX8`/`Null` sibling conventions: `BF2Window_macos.cpp` next to `BF2Window.cpp`,
`MacInputDeviceManager` next to `DX8InputDeviceManager`.

## Design

```
NSApplication event queue
        |
CocoaHost::pumpEvents()  -- called by BF2::handleInGameWinMessages, once per frame
        |  route(): key / flags / mouse / wheel events, only while the window has focus
        |  then [NSApp sendEvent:] for AppKit (menus, window management)
        v
IHostEventListener list
   |-- BF2Window_macos.cpp: WindowMessageSource -> synthesized Win32 messages
   |        -> Window::wndProc (state rules) -> engine message handlers (bf2MessageHandler)
   |-- MacKeyboardDevice / MacMouseDevice: buffered device events (the DirectInput buffer)
        v
IInputDeviceManager::update(-1) polls, update(dt > 0) consumes  (InputManager::update)
```

### Host

- **Application.** `initApplication` creates `NSApplication`, sets the regular activation
  policy, installs an application menu with Hide (Cmd-H) and Quit (Cmd-Q), and calls
  `finishLaunching`. The engine never calls `[NSApp run]`; the pump drains the queue with
  `nextEventMatchingMask:untilDate:distantPast` each frame, the counterpart of
  `PeekMessage(PM_REMOVE)`. Quit (Cmd-Q, the Dock menu, `applicationShouldTerminate:`) and the
  close button never terminate the process from AppKit; they set the quit flag, which the
  next pump reports as `WM_QUIT` would.
- **Window.** Windowed: titled, closable, miniaturizable, not resizable and without native
  full-screen (Win32 swallowed `SC_SIZE`/`SC_MAXIMIZE`, so the retail window could not be
  resized by the user either). The frame's top left is placed at (x, y) from the screen's top
  left, as Win32 positions do. The content view is layer-backed with a `CAMetalLayer`
  (`BGRA8Unorm`, `framebufferOnly`, opaque, black, no device). The layer stays empty until the
  stage 3 renderer attaches through `CocoaHost::metalLayer()` (or `nativeView()`).
- **Back-buffer size.** `contentsScale` is pinned to 1, so the drawable is one pixel per point
  and mouse coordinates in points equal back-buffer pixels. [CHOSEN] Rendering at 1x keeps the
  engine's "client size == back-buffer size" assumption; Retina output is an open item.
- **Fullscreen.** A borderless window covering the main screen, with the Dock and menu bar
  hidden through presentation options. [CHOSEN] The display mode is never changed: the
  drawable stays at the game resolution and the compositor scales it to the screen
  (stretched; no letterboxing). On Win32 the renderer switches the display mode instead.
- **Focus.** `hasFocus()` is "game window is key and the application is active". Changes
  arrive through the window and application delegates and are reported to listeners as
  `onAppActivationChanged` then `onFocusChanged`, the Win32 order of `WM_ACTIVATEAPP` and
  `WM_ACTIVATE`.
- **Cursor.** `showCursor` reproduces Win32 `ShowCursor`'s display counter (visible while
  >= 0). `setCursorHiddenInView` is `SetCursor(0)` over the client area. Relative mouse mode
  (an exclusively acquired DirectInput mouse) hides the cursor, parks it in the view's centre
  and disassociates it from the mouse (`CGAssociateMouseAndMouseCursorPosition(false)`); deltas
  keep arriving. All three are combined in one place and apply only while the application is
  active, so focus loss always gives the cursor back. A separate clip rectangle is not needed:
  a disassociated cursor does not move.
- **Clipboard.** `NSPasteboard` text converted to and from Windows-1252 bytes, the `CF_TEXT`
  counterpart on western Windows. Unrepresentable characters are replaced lossily.
- **Display modes.** `displayModes()` lists the main display's modes in pixels with refresh
  rate (duplicates removed, sorted); `desktopMode()` the current one. Built-in panels report
  0 Hz. These are for `IVideoOptions` later; nothing switches modes.
- **Desktop state.** Win32 saves and restores accessibility hot keys, mouse trails and the
  screensaver flag around the game. macOS has no counterpart for the first two; the screensaver
  switch becomes a display-sleep power assertion held while the game runs.
- **Clock.** Event times and `tickCountMs()` are milliseconds of system uptime, truncated to
  32 bits like `GetTickCount`, so the devices compare them exactly as the DX8 devices compare
  `dwTimeStamp` with `GetTickCount()`.

### Window and message pump (engine side)

`BF2Window.h` keeps its declarations, virtual order and members. On macOS it includes
`HostWindowTypes.h`, which defines `HWND` (an opaque pointer to the content `NSView`),
`UINT`, `WPARAM`, `LPARAM`, `LRESULT` and the Win32 message ids inside `dice::hfe`, never
globally. `BF2Window_macos.cpp` implements each method with the host call that matches the
Win32 body. A listener turns host events into the Win32 messages the engine inspects and runs
them through `Window::wndProc`, which keeps the Win32 state rules (focus flags, `WM_SIZE`
sizes, the `WM_ACTIVATEAPP` event in fullscreen, the in-game `WM_SETCURSOR` hide) and calls
every registered handler:

| Cocoa source | Message(s) |
| --- | --- |
| App activation change | `WM_ACTIVATEAPP` |
| Key window gained / lost | `WM_ACTIVATE` (`WA_ACTIVE`/`WA_INACTIVE`), `WM_SETFOCUS`/`WM_KILLFOCUS` |
| Window created, resized, (de)miniaturized | `WM_SIZE` with the back-buffer size |
| Close button, Cmd-Q, Dock Quit | `WM_CLOSE`, `WM_DESTROY` (quit flag set) |
| Key down / up | `WM_KEYDOWN`/`WM_KEYUP`, or `WM_SYSKEY*` with Option (Alt) held or for F10; US-layout virtual key in `wParam`, scan code in `lParam` bits 16-24 |
| Characters | `WM_CHAR`, one per Windows-1252 character. Backspace gives 8 (macOS reports 0x7F), keypad Enter 13. [CHOSEN] Command+letter is sent as the control character. |
| Mouse motion over the view | `WM_SETCURSOR`, then `WM_MOUSEMOVE` with client coordinates |
| Buttons 0/1/2 | `WM_LBUTTON*` (second click of a double click: `WM_LBUTTONDBLCLK`), `WM_RBUTTON*`, `WM_MBUTTON*` |
| Wheel | `WM_MOUSEWHEEL` |

Mouse messages are not sent while the mouse is captured (host relative mode with focus, the
acquired exclusive DirectInput mouse in-game): Win32 generates none then (c-input-review, finding 1).

`BF2.cpp` gains `__APPLE__ && !DEDICATED` arms:

- `handleInGameWinMessages`: `CocoaHost::pumpEvents()`; a posted quit restores the desktop
  state and calls `exit(0)` without engine shutdown, as the Win32 `WM_QUIT` arm does. Then the
  unchanged focus logic: `IInputManager::setHasFocus(window->getHasFocus(), mask)` and the
  focus-loss mute.
- `bf2MessageHandler`: the Win32 handler's arms (menu mouse callbacks, `DEL` as 127, `WM_CHAR`
  into the console and menu rings, Esc, Ctrl-C/V/X). `GetAsyncKeyState(VK_CONTROL)` reads
  Control or Command, `VK_MENU` reads Option. Omitted: switching Caps Lock off on activation and
  on an in-game press (`SendInput`), which macOS has no supported counterpart for.
- `main`: the POSIX argv join, then the Win32 client entry chain (`bf2Main`/`bf2WinMain`):
  application init, display-sleep assertion, `BF2::init`, the profiling flag, `+demo`, the
  `handleInGameWinMessages`/`mainLoop` loop, handler removal, `delete bf2`, FPU state, restore.
  The ELF `main` stays as it is for Linux and every dedicated image.

### Input devices

`MacInputDeviceManager` mirrors `DX8InputDeviceManager` line for line, including its leak of
devices whose `init` fails and its ignored duplicate inserts. The devices keep the DX8
devices' interfaces and sampling bodies; only the event source differs. Each choice:

| Behaviour | DX8 | macOS |
| --- | --- | --- |
| Update protocol | `update(dt <= 0)` reads the DirectInput buffer (`GetDeviceData`); `update(dt > 0)` consumes events older than the first event + dt (keyboard) or the previous poll time + dt (mouse). `InputManager::update` polls with -1, then makes `tick` passes, the last with 10. | Identical. The host buffers events between polls; the poll moves them into the same queues and the consuming code is the DX8 code. |
| Buffer size and overflow | 100 keyboard / 128 mouse events; overflow clears state | Same sizes; overflow sets a flag that the next poll treats as `DI_BUFFEROVERFLOW`. |
| Input lost | `DIERR_INPUTLOST` clears state; reacquire fails without the foreground | Focus loss sets an input-lost flag with the same effect at the next poll; reacquire succeeds only with focus. |
| Acquire | `activate` acquires (foreground only) and returns true even when acquisition fails | `activate` sets active only with focus and returns true. `BF2::handleInGameWinMessages` calls it every frame, so devices come back when focus returns. |
| Keyboard data | Scan code offset, `0x80` press | `kVK_` code mapped to `DIK_` (table below), `0x80` press. Auto-repeat is dropped: DirectInput reports transitions only. |
| Modifiers | Separate left/right scan codes | From `flagsChanged`: the device-dependent left/right bits decide press and release. Command is `DIK_LWIN`/`DIK_RWIN`, Option `DIK_LMENU`/`DIK_RMENU` (Alt). fn has no code. |
| Caps Lock | Press and release | macOS reports only the lock toggle. Each toggle is delivered as a press, with the release on the next pump, so the two land in different samples. |
| `getKeyName` | `DIPROP_KEYNAME` (localized) | Printable keys: the character the current layout puts on the key (`UCKeyTranslate`), upper-cased, so an AZERTY Q key reads "A" as on Windows. Others: English names in the `GetKeyNameText` style, Mac names for Command and Option. |
| Mouse format | `c_dfDIMouse2`: X, Y, Z (wheel), eight buttons | Same offsets. `getNumButtons` is 8 and `getNumAxes` 3 ([CHOSEN]; macOS reports no button count). |
| Mouse motion | Raw counts from the device | With a `GCMouse` connected: its unaccelerated `mouseMovedHandler` deltas, summed over all mice, Y negated to grow downwards (c-raw-mouse). Fallback without one (trackpad-only Macs): `NSEvent` `deltaX`/`deltaY`, one point = one count, which include the system pointer acceleration. Fractions carried to the next report in both. |
| Mouse buttons | DirectInput order: left, right, middle, extras | `NSEvent.buttonNumber`, the same order. |
| Wheel | +120 per notch away from the user | Line-based wheels: `deltaY` x 120; trackpads: `scrollingDeltaY` x 12 per point (10 points a notch). Natural scrolling is undone (`isDirectionInvertedFromDevice`), so the sign follows the device. Only whole notches (multiples of 120) are reported; fractions are carried, and reset when a gesture begins. Momentum-phase events are dropped (macos-c-input-review.md, finding 2). |
| Axis scaling | `MouseDevice` multiplies by the active control map's axis scale (and sensitivity in `peakXYAxis`) | Identical code. |
| Exclusive mouse | Hidden, held cursor while acquired | Host relative mode while acquired and exclusive. Non-exclusive leaves the cursor alone. |
| Controller | `DIJOYSTATE2`, axes in -1000..1000 x 0.001 x scale, buttons, POV hats; POV cardinals also as buttons | An extended gamepad presented as DirectInput shows an Xbox controller: X/Y left stick (Y down), Z = left trigger - right trigger, Rx/Ry right stick (Y down); buttons A, B, X, Y, LB, RB, View, Menu, LS, RS (0-9); one POV from the d-pad (diagonals included), its cardinals as buttons 10-13. Values are rounded to the DirectInput range before the DX8 scaling. |
| Controller discovery | Enumerated once at `initDevices` | `GCController.controllers` at `initDevices`; unplugging stops updates; hot-plugging is not picked up (as on DX8). |
| Falcon | Novint HFX | Not opened; `getInputDevice(IDFFalcon)` returns null, which `BF2FrameInterpolator` already handles. |

### Keyboard layouts

`kVK_` codes are physical positions, so the table is the same for every layout, except for
the two keys Apple swaps on ISO keyboards: on an ISO keyboard the key left of 1 reports
`kVK_ISO_Section` (0x0A) and the key beside left Shift reports `kVK_ANSI_Grave` (0x32). The
host decides per key event from the event's own keyboard type
(`kCGKeyboardEventKeyboardType` through `KBGetLayoutType`), not from `LMGetKbdType`, which
reflects whichever keyboard sent the last event. JIS keys map to their DirectInput Japanese
counterparts.

### DIK mapping table

Mac key labels are ANSI legends. DIK values are the `IDKey_` values in `IO/Input/Input.h`.
[CHOSEN] rows: F13/F14/F15 sit where PrintScreen/ScrollLock/Pause sit on a PC keyboard and
map there (BF2 binds screenshots to PrintScreen); Help (the Insert position on Apple extended
keyboards) maps to Insert; keypad Clear (the NumLock position) to NumLock; the PowerBook Enter
(0x34) to keypad Enter; JIS Eisu to NoConvert, Kana to Kana and the JIS underscore key to
`DIK_ABNT_C1` (115), which `Input.h` has no enumerator for. F16-F20 and fn have no DirectInput
code.

| kVK | Mac key | DIK | `IDKey_` name | Display name |
|---|---|---|---|---|
| 0x00 | A | 30 (0x1E) | A | A |
| 0x01 | S | 31 (0x1F) | S | S |
| 0x02 | D | 32 (0x20) | D | D |
| 0x03 | F | 33 (0x21) | F | F |
| 0x04 | H | 35 (0x23) | H | H |
| 0x05 | G | 34 (0x22) | G | G |
| 0x06 | Z | 44 (0x2C) | Z | Z |
| 0x07 | X | 45 (0x2D) | X | X |
| 0x08 | C | 46 (0x2E) | C | C |
| 0x09 | V | 47 (0x2F) | V | V |
| 0x0A | ISO only: the key left of 1 (§ / ^) | 41 ISO (86 if reported on ANSI) | Grave | `` ` `` |
| 0x0B | B | 48 (0x30) | B | B |
| 0x0C | Q | 16 (0x10) | Q | Q |
| 0x0D | W | 17 (0x11) | W | W |
| 0x0E | E | 18 (0x12) | E | E |
| 0x0F | R | 19 (0x13) | R | R |
| 0x10 | Y | 21 (0x15) | Y | Y |
| 0x11 | T | 20 (0x14) | T | T |
| 0x12 | 1 | 2 (0x02) | 1 | 1 |
| 0x13 | 2 | 3 (0x03) | 2 | 2 |
| 0x14 | 3 | 4 (0x04) | 3 | 3 |
| 0x15 | 4 | 5 (0x05) | 4 | 4 |
| 0x16 | 6 | 7 (0x07) | 6 | 6 |
| 0x17 | 5 | 6 (0x06) | 5 | 5 |
| 0x18 | = | 13 (0x0D) | Equals | = |
| 0x19 | 9 | 10 (0x0A) | 9 | 9 |
| 0x1A | 7 | 8 (0x08) | 7 | 7 |
| 0x1B | - | 12 (0x0C) | Minus | - |
| 0x1C | 8 | 9 (0x09) | 8 | 8 |
| 0x1D | 0 | 11 (0x0B) | 0 | 0 |
| 0x1E | ] | 27 (0x1B) | RightBracket | ] |
| 0x1F | O | 24 (0x18) | O | O |
| 0x20 | U | 22 (0x16) | U | U |
| 0x21 | [ | 26 (0x1A) | LeftBracket | [ |
| 0x22 | I | 23 (0x17) | I | I |
| 0x23 | P | 25 (0x19) | P | P |
| 0x24 | Return | 28 (0x1C) | Enter | Enter |
| 0x25 | L | 38 (0x26) | L | L |
| 0x26 | J | 36 (0x24) | J | J |
| 0x27 | ' | 40 (0x28) | Apostrophe | ' |
| 0x28 | K | 37 (0x25) | K | K |
| 0x29 | ; | 39 (0x27) | Semicolon | ; |
| 0x2A | \ | 43 (0x2B) | Backslash | \ |
| 0x2B | , | 51 (0x33) | Comma | , |
| 0x2C | / | 53 (0x35) | Slash | / |
| 0x2D | N | 49 (0x31) | N | N |
| 0x2E | M | 50 (0x32) | M | M |
| 0x2F | . | 52 (0x34) | Period | . |
| 0x30 | Tab | 15 (0x0F) | Tab | Tab |
| 0x31 | Space | 57 (0x39) | Space | Space |
| 0x32 | ANSI/JIS: `` ` `` left of 1; ISO: the key beside left Shift (< >) | 41 ANSI/JIS, 86 ISO | Grave / OEM_102 | from the layout (`<` on a German or Swiss ISO layout) |
| 0x33 | Delete | 14 (0x0E) | Backspace | Backspace |
| 0x34 | PB Enter | 156 (0x9C) | NumpadEnter | Num Enter |
| 0x35 | Escape | 1 (0x01) | Escape | Esc |
| 0x36 | RCommand | 220 (0xDC) | RightWin | Right Command |
| 0x37 | Command | 219 (0xDB) | LeftWin | Command |
| 0x38 | Shift | 42 (0x2A) | LeftShift | Shift |
| 0x39 | CapsLock | 58 (0x3A) | Capital | Caps Lock |
| 0x3A | Option | 56 (0x38) | LeftAlt | Option |
| 0x3B | Control | 29 (0x1D) | LeftCtrl | Ctrl |
| 0x3C | RShift | 54 (0x36) | RightShift | Right Shift |
| 0x3D | ROption | 184 (0xB8) | RightAlt | Right Option |
| 0x3E | RControl | 157 (0x9D) | RightCtrl | Right Ctrl |
| 0x3F | fn | — | — | — |
| 0x40 | F17 | — | — | — |
| 0x41 | Num . | 83 (0x53) | Decimal | Num Del |
| 0x42 | (unused) | — | — | — |
| 0x44 | (unused) | — | — | — |
| 0x45 | Num + | 78 (0x4E) | Add | Num + |
| 0x46 | (unused) | — | — | — |
| 0x47 | Clear | 69 (0x45) | Numlock | Clear |
| 0x48 | VolUp | 176 (0xB0) | VolumeUp | Volume Up |
| 0x49 | VolDown | 174 (0xAE) | VolumeDown | Volume Down |
| 0x4A | Mute | 160 (0xA0) | Mute | Mute |
| 0x4B | Num / | 181 (0xB5) | Divide | Num / |
| 0x4C | Num Enter | 156 (0x9C) | NumpadEnter | Num Enter |
| 0x4D | (unused) | — | — | — |
| 0x4E | Num - | 74 (0x4A) | Subtract | Num - |
| 0x4F | F18 | — | — | — |
| 0x50 | F19 | — | — | — |
| 0x51 | Num = | 141 (0x8D) | NumpadEquals | Num = |
| 0x52 | Num 0 | 82 (0x52) | Numpad0 | Num 0 |
| 0x53 | Num 1 | 79 (0x4F) | Numpad1 | Num 1 |
| 0x54 | Num 2 | 80 (0x50) | Numpad2 | Num 2 |
| 0x55 | Num 3 | 81 (0x51) | Numpad3 | Num 3 |
| 0x56 | Num 4 | 75 (0x4B) | Numpad4 | Num 4 |
| 0x57 | Num 5 | 76 (0x4C) | Numpad5 | Num 5 |
| 0x58 | Num 6 | 77 (0x4D) | Numpad6 | Num 6 |
| 0x59 | Num 7 | 71 (0x47) | Numpad7 | Num 7 |
| 0x5A | F20 | — | — | — |
| 0x5B | Num 8 | 72 (0x48) | Numpad8 | Num 8 |
| 0x5C | Num 9 | 73 (0x49) | Numpad9 | Num 9 |
| 0x5D | JIS Yen | 125 (0x7D) | Yen | Yen |
| 0x5E | JIS _ | 115 (0x73) | (no enumerator) | Ro |
| 0x5F | JIS Num , | 179 (0xB3) | NumpadComma | Num , |
| 0x60 | F5 | 63 (0x3F) | F5 | F5 |
| 0x61 | F6 | 64 (0x40) | F6 | F6 |
| 0x62 | F7 | 65 (0x41) | F7 | F7 |
| 0x63 | F3 | 61 (0x3D) | F3 | F3 |
| 0x64 | F8 | 66 (0x42) | F8 | F8 |
| 0x65 | F9 | 67 (0x43) | F9 | F9 |
| 0x66 | JIS Eisu | 123 (0x7B) | NoConvert | Eisu |
| 0x67 | F11 | 87 (0x57) | F11 | F11 |
| 0x68 | JIS Kana | 112 (0x70) | Kana | Kana |
| 0x69 | F13 | 183 (0xB7) | PrintScreen | Prnt Scrn |
| 0x6A | F16 | — | — | — |
| 0x6B | F14 | 70 (0x46) | ScrollLock | Scroll Lock |
| 0x6C | (unused) | — | — | — |
| 0x6D | F10 | 68 (0x44) | F10 | F10 |
| 0x6E | Menu | 221 (0xDD) | AppMenu | Menu |
| 0x6F | F12 | 88 (0x58) | F12 | F12 |
| 0x70 | (unused) | — | — | — |
| 0x71 | F15 | 197 (0xC5) | Pause | Pause |
| 0x72 | Help | 210 (0xD2) | Insert | Insert |
| 0x73 | Home | 199 (0xC7) | Home | Home |
| 0x74 | PageUp | 201 (0xC9) | PageUp | Page Up |
| 0x75 | FwdDel | 211 (0xD3) | Delete | Delete |
| 0x76 | F4 | 62 (0x3E) | F4 | F4 |
| 0x77 | End | 207 (0xCF) | End | End |
| 0x78 | F2 | 60 (0x3C) | F2 | F2 |
| 0x79 | PageDown | 209 (0xD1) | PageDown | Page Down |
| 0x7A | F1 | 59 (0x3B) | F1 | F1 |
| 0x7B | Left | 203 (0xCB) | ArrowLeft | Left |
| 0x7C | Right | 205 (0xCD) | ArrowRight | Right |
| 0x7D | Down | 208 (0xD0) | ArrowDown | Down |
| 0x7E | Up | 200 (0xC8) | ArrowUp | Up |
| 0x7F | (unused) | — | — | — |

## Integration for S2.0 (client target)

Steps 1 to 3 are done (package S2.7's wiring, branch `mac/s2-7-vc71`). Step 5 is done for the
client TUs on `mac/s2-58-platform` (packages S2.5/S2.8); the `RendDX9/VideoOptions.cpp` and
`RendDX9/BF2Render.cpp` rows wait for the renderer (stage 3). `FalconDevice.cpp` is archived
(`Code/Archive/NovintFalcon`).

1. **Sources.** The engine's recursive glob already picks up the new `.cpp` files
   (`Mac*.cpp`, `MacKeyMap.cpp`, `BF2Window_macos.cpp`); they compile to empty objects outside
   the macOS client. Add the two Objective-C++ files to the macOS client target explicitly,
   compiled with ARC:
   ```cmake
   target_sources(bf2_client PRIVATE
       "${BF2_ROOT}/Platform/macOS/CocoaHost.mm"
       "${BF2_ROOT}/Platform/macOS/MacGamepad.mm")
   set_source_files_properties(
       "${BF2_ROOT}/Platform/macOS/CocoaHost.mm" "${BF2_ROOT}/Platform/macOS/MacGamepad.mm"
       PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
   target_link_libraries(bf2_client PRIVATE
       "-framework AppKit" "-framework QuartzCore" "-framework Metal"
       "-framework GameController" "-framework Carbon" "-framework IOKit")
   ```
   The project must enable the `OBJCXX` language. The `.mm` files need no engine flags but
   accept the force-included `BuildConfig.h`/`PlatformCompat.h`.
2. **Exclude the DirectInput TUs on macOS:** `IO/Input/DX8InputDeviceManager.cpp`,
   `KeyboardDevice.cpp`, `MouseDevice.cpp`, `GameControllerDevice.cpp`, `FalconDevice.cpp`.
   `BF2Window.cpp` is already empty there.
3. **Register the manager** in `Core/ModuleRegistry.cpp`, `InputDX8Module::getClassList`,
   under the same CID and name so `BF2EngineSetup`'s `setSingletonClass("InputDeviceManager",
   CID_DX8InputDeviceManager)` and every caller stay unchanged:
   ```cpp
   #if defined(__APPLE__)
       out.push_back(ClassRegInfo{CID_DX8InputDeviceManager, "dice.hfe.io.InputDeviceManager.DX8",
                                  &MacInputDeviceManager::create});   // [CHOSEN, not recovered]
   #else
       out.push_back(ClassRegInfo{CID_DX8InputDeviceManager, "dice.hfe.io.InputDeviceManager.DX8",
                                  &DX8InputDeviceManager::create});
   #endif
   ```
   with `#include "../IO/Input/MacInputDeviceManager.h"` beside the DX8 include.
4. **Entry point.** Nothing to do: `BF2.cpp`'s macOS client `main` is selected by
   `__APPLE__ && !DEDICATED`.
5. **Remaining Win32 calls in client TUs**, each with its host counterpart (all `CocoaHost::
   instance()`):

   | Call site | Win32 | macOS |
   | --- | --- | --- |
   | `BF2Engine.cpp` main loop | `ShowCursor(FALSE/TRUE)` | `showCursor(false/true)` (same counter semantics) |
   | `BF2Engine::browseModHomepage` | `ShowWindow(SW_MINIMIZE)`, `ShellExecuteA("open", url)` | `minimize()`, `openURL(url)` (true on success) |
   | `BF2Engine::restartBF2`, `activateMod` | `GetModuleFileNameA`, `ShellExecuteA(exe, args)` | `_NSGetExecutablePath` + `posix_spawn`; not provided by the host |
   | `MainMenuLogic.cpp`, `BetweenRoundsLogic.cpp` | `ShowCursor`, `SetCursor(host->getCursor(i))`, `SetCursor(0)` | `showCursor`, `setCursorShape(HostCursorArrow/Busy)`, `setCursorHiddenInView(true)` |
   | `MainMenuLogic::handleClipboardShortcut` | `GetAsyncKeyState`, `OpenClipboard`/`GetClipboardData`/`SetClipboardData(CF_TEXT)` | `modifierFlags()` (Control or Command as Ctrl, Option as Alt), `getClipboardText`/`setClipboardText` |
   | `BetweenRoundsLogic.cpp menuScanCodeToChar` | `ToAsciiEx(MapVirtualKeyEx(scan))` | `macKeyCodeFromDik` + `keyCharacter(keyCode)` (no modifiers; shift state is an open item) |
   | `SwiffHost.cpp` | `UpdateWindow(hwnd)` | nothing to do (return true) |
   | `Game/Main/BF2Render.cpp` | `GetCurrentProcessId`, `getHwnd()` into the display-mode carrier | `getpid()`; `getHwnd()` is the content `NSView*`, and the renderer finds the layer through `CocoaHost::metalLayer()` |
   | `RendDX9/VideoOptions.cpp` | `SetWindowPos` with frame metrics | `BF2Window::resizeClient` / `CocoaHost::resizeClient` (client size, no frame metrics) |
   | `RendDX9/BF2Render.cpp enumDevModes` | `EnumDisplaySettingsA` | `displayModes()` (all 32 bpp) |
   | `IO/Input/FalconDevice.cpp` | `GetForegroundWindow` | not built on macOS |

## Test app

`Code/Tests/macos_host` (`[TEMP]`, `[RETIRE-WHEN: macos-client-window]`) links the production
host and devices with a control-map fixture that answers only what the mouse device asks
(axis scale 1, sensitivity 1, no inversion). It opens the window through the host, runs the
engine's update protocol, posts synthetic events to its own queue (never system-wide), logs
DIK codes, mouse axes and buttons, and ends by pressing the close button programmatically.

```sh
cd Code/Tests/macos_host
cmake --preset macos-host-test && cmake --build --preset macos-host-test
../../../out/build/macos-host-test/macos_host_test --seconds 3 [--simulate-focus] [--exclusive] [--fullscreen-cycle] [--no-synthetic]
```

`--simulate-focus` reports focus through a test-only host method that exists only when
`OPENBF2_HOST_TEST_SEAMS` is defined, which only this project defines. It is needed when the
application cannot be activated: the validation runs were made while the Mac's session sat
at the login window (`loginwindow` frontmost), where no application can become active. Without
the flag the host correctly routes no input, and the app logs that the window never had focus.

Result on macOS 15.7.7, Intel, ISO keyboard (type 83), no controller, runs of 2-3 s:

```
[display] desktop 2880x1800@0, 10 modes, keyboard type 83 ISO=1, gamepads=0
[window] created view=0x7fd4bb320a60 layer=CAMetalLayer drawable=800x600
[input] initDevices all=0 opened mask=0x3
[keyboard] DIK  17 (0x11) W            down / up
[keyboard] DIK  42 (0x2A) Shift        down / up
[keyboard] DIK  86 (0x56) <            down / up        (kVK 0x32 on an ISO keyboard)
[keyboard] DIK  28 (0x1C) Enter        down / up
[keyboard] DIK  58 (0x3A) Caps Lock    down / up        (lock on, then lock off)
[mouse] axes x=37 y=11 wheel=0
[mouse] button 0 down / up
[mouse] axes x=0 y=0 wheel=240                           (two notches away from the user)
[window] fullscreen=1 drawable=800x600 ... fullscreen=0 drawable=800x600
[window] close requested
[test] quit requested=1
```

`initDevices` returns false with mask 0x3 because the requested controller is absent, as DX8
reports. The exclusive run (`--exclusive`) produced the same log with the cursor in relative
mode. Not exercised: real keystrokes and mouse motion from a person, a connected controller,
clipboard, `openURL`, Retina scaling, multiple displays, and anything engine-side
(`BF2Window_macos.cpp`, the `BF2.cpp` arms), which compiles but does not link until S2.0 links.

Validation also run: the full `macos-x64` dedicated build and link of `bf2` in a separate
build directory, and `-fsyntax-only` checks of every changed engine TU with `DEDICATED=0` and
`=1`. MSVC and Linux builds were not run on this Mac; every shared-file change is behind
`__APPLE__` or `_WIN32` and leaves the Win32 and Linux text paths unchanged.

## Open items

- **Raw mouse deltas.** Done with `GCMouse` (c-raw-mouse, `Platform/macOS/MacRawMouse.h`); the
  trackpad fallback still carries pointer acceleration. The feel against the retail client is
  still to be compared.
- **Retina.** The back buffer is rendered at 1x. A 2x back buffer would change the
  window-size/back-buffer-size relation the engine and the menus assume.
- **Fullscreen scaling.** The game resolution is stretched to the screen; aspect-correct
  letterboxing (and mapping the mouse through it) is not done. No display-mode switching.
- **Busy cursor.** AppKit has no public busy cursor; `HostCursorBusy` shows the arrow.
- **Caps Lock.** The Win32 client switches Caps Lock off in-game and on activation; macOS has no
  supported way, so the lock state is left as the user set it.
- **Text input.** `WM_CHAR` comes from `NSEvent.characters`: dead keys compose as macOS composes
  them, but there is no input-method (IME) support, and characters outside Windows-1252 are
  dropped. `keyCharacter` takes Shift and Caps Lock (`menuScanCodeToChar` passes them); Option
  and dead keys are not applied there.
- **Short controller presses.** Controllers are polled; DX8 also merged buffered presses between
  polls. A press shorter than a frame can be missed. Controller hot-plug is not handled.
- **Laptop function keys.** F1-F12 reach the game only when macOS sends them as function keys
  (fn held, or the "Use F1, F2, etc. keys as standard function keys" setting).
- **Command key.** Command is reported as `DIK_LWIN`/`DIK_RWIN` and acts as Ctrl for the menu
  clipboard shortcuts; Command combinations also reach the application menu (Cmd-Q quits, Cmd-H
  hides).
- **S2.0 wiring** (above): done except the two renderer call sites. `CocoaHost` gained
  `cursorHandle`/`setCursor` (Win32 `HCURSOR`/`SetCursor` semantics) for `SwiffHost::getCursor`
  and the menu logics.
