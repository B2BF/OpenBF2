// [CHOSEN, not recovered] Shared native keyboard metadata, issue #36.
#ifndef PLATFORM_HOST_KEYMAP_H
#define PLATFORM_HOST_KEYMAP_H

namespace dice { namespace hfe { namespace host {

// US-layout Win32 virtual key for a BF2 DIK physical key, or zero when unmapped.
// Native window adapters use this to synthesize the engine's existing key messages.
unsigned int win32VirtualKeyFromDik(unsigned char dik);

// English fallback label for a DIK key, or null when unnamed. Backends provide native
// modifier-key labels and use layout translation for printable key names.
const char* dikDisplayName(unsigned char dik);

} } }
#endif
