// [CHOSEN, not recovered] Window-message vocabulary for native client BF2Window declarations.
// Window and its message handlers keep their Win32 shape (handle, message id, two parameters).
// WindowMessageSource translates normalized host events using Win32 numeric message values.
//
// The names live in namespace dice::hfe only, never at global scope, so they cannot collide with
// another platform header. They are window-messaging types, not serialized widths: HWND is an
// opaque pointer to the game window's renderer handle, and WPARAM/LPARAM are
// pointer-sized as on Win64. Only the constants the engine's window code reads are defined.

#ifndef PLATFORM_HOST_HOSTWINDOWTYPES_H
#define PLATFORM_HOST_HOSTWINDOWTYPES_H

#include <cstdint>

namespace dice {
namespace hfe {

typedef struct HostNativeView__* HWND;      // borrowed NSView* on macOS, SDL_Window* on Linux
typedef unsigned int             UINT;
typedef std::uintptr_t           WPARAM;
typedef std::intptr_t            LPARAM;
typedef std::intptr_t            LRESULT;

namespace hostmsg {

// Window messages (winuser.h values).
const UINT WM_DESTROY       = 0x0002;
const UINT WM_SIZE          = 0x0005;
const UINT WM_ACTIVATE      = 0x0006;
const UINT WM_SETFOCUS      = 0x0007;
const UINT WM_KILLFOCUS     = 0x0008;
const UINT WM_CLOSE         = 0x0010;
const UINT WM_ACTIVATEAPP   = 0x001C;
const UINT WM_SETCURSOR     = 0x0020;
const UINT WM_KEYDOWN       = 0x0100;
const UINT WM_KEYUP         = 0x0101;
const UINT WM_CHAR          = 0x0102;
const UINT WM_SYSKEYDOWN    = 0x0104;
const UINT WM_SYSKEYUP      = 0x0105;
const UINT WM_MOUSEMOVE     = 0x0200;
const UINT WM_LBUTTONDOWN   = 0x0201;
const UINT WM_LBUTTONUP     = 0x0202;
const UINT WM_LBUTTONDBLCLK = 0x0203;
const UINT WM_RBUTTONDOWN   = 0x0204;
const UINT WM_RBUTTONUP     = 0x0205;
const UINT WM_MBUTTONDOWN   = 0x0207;
const UINT WM_MBUTTONUP     = 0x0208;
const UINT WM_MOUSEWHEEL    = 0x020A;

// WM_ACTIVATE and WM_SIZE parameters.
const WPARAM WA_INACTIVE    = 0;
const WPARAM WA_ACTIVE      = 1;
const WPARAM SIZE_RESTORED  = 0;
const WPARAM SIZE_MINIMIZED = 1;

// Virtual keys the window code compares against.
const WPARAM VK_CAPITAL     = 0x14;
const WPARAM VK_DELETE      = 0x2E;
const WPARAM VK_F10         = 0x79;

// MAKELPARAM: low word x / width, high word y / height.
inline LPARAM makeLParam(int low, int high)
{
    return static_cast<LPARAM>((static_cast<std::uint32_t>(high & 0xFFFF) << 16) |
                               static_cast<std::uint32_t>(low & 0xFFFF));
}

} // namespace hostmsg

} // namespace hfe
} // namespace dice

#endif // PLATFORM_HOST_HOSTWINDOWTYPES_H
