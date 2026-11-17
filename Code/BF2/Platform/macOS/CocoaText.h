// [CHOSEN, not recovered] Cocoa text policy at the native host boundary, issue #36.
#ifndef PLATFORM_MACOS_COCOATEXT_H
#define PLATFORM_MACOS_COCOATEXT_H

#include "../Host/HostEvents.h"

namespace dice { namespace hfe { namespace macos {

// Appends a Cocoa UTF-16 unit after editing-key and Command-shortcut normalization.
// The source DIK distinguishes keypad Enter's ETX from Ctrl+C, which must remain ETX.
inline void appendNormalizedCocoaText(host::HostTextEvent& event, unsigned short character)
{
    if (character >= 0xF700 && character <= 0xF8FF)
        return;
    if (character == 0x7F) {
        if (event.dik != 14) return;
        character = 0x08;
    } else if (event.dik == 156 && character == 0x03) {
        character = 0x0D;
    }
    if ((event.modifiers & host::HostModSuper) &&
        (character | 0x20) >= 'a' && (character | 0x20) <= 'z')
        character = static_cast<unsigned short>(character & 0x1F);
    event.characters.push_back(character);
}

} } }
#endif
