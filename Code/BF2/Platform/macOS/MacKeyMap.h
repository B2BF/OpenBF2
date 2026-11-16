// [CHOSEN, not recovered] macOS port: translation between macOS virtual key codes (the kVK_
// values of HIToolbox/Events.h, reported by NSEvent.keyCode) and the DirectInput scan codes
// (DIK_ values) that io::InputDeviceKeys, control maps and .con bindings use. There was no
// retail macOS image; the table and every choice in it are documented in docs/macos-s2-4.md.
//
// kVK codes name physical key positions, not characters, so the table is layout-independent
// except for the two keys Apple swaps on ISO keyboards (see dikFromMacKeyCode).

#ifndef PLATFORM_MACOS_MACKEYMAP_H
#define PLATFORM_MACOS_MACKEYMAP_H

namespace dice {
namespace hfe {
namespace macos {

// Returns the DirectInput scan code for a macOS virtual key code, or 0 when the key has no
// DirectInput counterpart (fn, F16-F20, codes above 0x7F). isoLayout selects the ISO
// assignment of kVK_ISO_Section (0x0A) and kVK_ANSI_Grave (0x32): on ISO keyboards the key left
// of 1 reports 0x0A and the key beside left Shift reports 0x32, so they map to DIK_GRAVE and
// DIK_OEM_102 respectively; on ANSI and JIS keyboards 0x32 is the key left of 1.
unsigned char dikFromMacKeyCode(unsigned short keyCode, bool isoLayout);

// Returns the macOS virtual key code that produces dik, or 0xFFFF when no key does.
// The Linux host reserves 0x100|dik for PC keys outside the Cocoa vocabulary; the forward
// translation accepts these tokens only in the Linux client. This does not change DIK bindings.
unsigned short macKeyCodeFromDik(unsigned char dik, bool isoLayout);

// Returns the Win32 virtual-key code a US-layout Windows keyboard reports in WM_KEYDOWN for this
// scan code, or 0 when there is none. Used only to synthesize window messages on macOS.
unsigned int win32VirtualKeyFromDik(unsigned char dik);

// Returns an English display name in the style of DirectInput's DIPROP_KEYNAME on an English
// Windows system, using macOS names for the Command and Option keys. Returns 0 for codes the
// table does not name.
const char* dikDisplayName(unsigned char dik);

} // namespace macos
} // namespace hfe
} // namespace dice

#endif // PLATFORM_MACOS_MACKEYMAP_H
