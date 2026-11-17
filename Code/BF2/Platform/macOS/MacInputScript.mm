// [TEMP] [CHOSEN, not recovered] macOS port test tool: the kVK_ key table of the scripted input
// engine (Platform/InputScript.h, docs/macos-c-input-script.md). CocoaHost.mm is its host.
// Compile with ARC (-fobjc-arc); no Objective-C objects are used.
// [RETIRE-WHEN: client-test-automation-replaced]
#include "MacInputScript.h"
#include <Carbon/Carbon.h>                  // kVK_ key codes

namespace dice {
namespace hfe {
namespace inputscript {
namespace keys {

namespace {

struct KeyName { const char* name; unsigned short code; };

// Physical key positions (kVK_ codes, ANSI). Names follow Controls.con's IDKey_ names without the
// prefix, case-insensitive, plus a few aliases.
const KeyName kKeys[] = {
    {"a", kVK_ANSI_A}, {"b", kVK_ANSI_B}, {"c", kVK_ANSI_C}, {"d", kVK_ANSI_D}, {"e", kVK_ANSI_E},
    {"f", kVK_ANSI_F}, {"g", kVK_ANSI_G}, {"h", kVK_ANSI_H}, {"i", kVK_ANSI_I}, {"j", kVK_ANSI_J},
    {"k", kVK_ANSI_K}, {"l", kVK_ANSI_L}, {"m", kVK_ANSI_M}, {"n", kVK_ANSI_N}, {"o", kVK_ANSI_O},
    {"p", kVK_ANSI_P}, {"q", kVK_ANSI_Q}, {"r", kVK_ANSI_R}, {"s", kVK_ANSI_S}, {"t", kVK_ANSI_T},
    {"u", kVK_ANSI_U}, {"v", kVK_ANSI_V}, {"w", kVK_ANSI_W}, {"x", kVK_ANSI_X}, {"y", kVK_ANSI_Y},
    {"z", kVK_ANSI_Z},
    {"0", kVK_ANSI_0}, {"1", kVK_ANSI_1}, {"2", kVK_ANSI_2}, {"3", kVK_ANSI_3}, {"4", kVK_ANSI_4},
    {"5", kVK_ANSI_5}, {"6", kVK_ANSI_6}, {"7", kVK_ANSI_7}, {"8", kVK_ANSI_8}, {"9", kVK_ANSI_9},
    {"escape", kVK_Escape}, {"esc", kVK_Escape}, {"enter", kVK_Return}, {"return", kVK_Return},
    {"space", kVK_Space}, {"tab", kVK_Tab}, {"backspace", kVK_Delete}, {"grave", kVK_ANSI_Grave},
    {"minus", kVK_ANSI_Minus}, {"equals", kVK_ANSI_Equal}, {"leftbracket", kVK_ANSI_LeftBracket},
    {"rightbracket", kVK_ANSI_RightBracket}, {"semicolon", kVK_ANSI_Semicolon},
    {"apostrophe", kVK_ANSI_Quote}, {"backslash", kVK_ANSI_Backslash}, {"comma", kVK_ANSI_Comma},
    {"period", kVK_ANSI_Period}, {"slash", kVK_ANSI_Slash},
    {"f1", kVK_F1}, {"f2", kVK_F2}, {"f3", kVK_F3}, {"f4", kVK_F4}, {"f5", kVK_F5}, {"f6", kVK_F6},
    {"f7", kVK_F7}, {"f8", kVK_F8}, {"f9", kVK_F9}, {"f10", kVK_F10}, {"f11", kVK_F11}, {"f12", kVK_F12},
    {"up", kVK_UpArrow}, {"down", kVK_DownArrow}, {"left", kVK_LeftArrow}, {"right", kVK_RightArrow},
    {"arrowup", kVK_UpArrow}, {"arrowdown", kVK_DownArrow}, {"arrowleft", kVK_LeftArrow},
    {"arrowright", kVK_RightArrow},
    {"home", kVK_Home}, {"end", kVK_End}, {"pageup", kVK_PageUp}, {"pagedown", kVK_PageDown},
    {"delete", kVK_ForwardDelete}, {"insert", kVK_Help}, {"numpadenter", kVK_ANSI_KeypadEnter},
    {"leftshift", kVK_Shift}, {"shift", kVK_Shift}, {"rightshift", kVK_RightShift},
    {"leftctrl", kVK_Control}, {"ctrl", kVK_Control}, {"rightctrl", kVK_RightControl},
    {"leftalt", kVK_Option}, {"alt", kVK_Option}, {"rightalt", kVK_RightOption},
    {"capital", kVK_CapsLock},
};

} // namespace

bool lookup(const std::string& name, unsigned short& code)
{
    for (const KeyName& k : kKeys)
        if (name == k.name) { code = k.code; return true; }
    return false;
}

// NSEvent.characters for keys UCKeyTranslate reports as control characters (ClientHost's
// keyCharacter drops them). Function keys carry private-use characters the host strips anyway.
std::u16string controlCharacters(unsigned short code)
{
    switch (code)
    {
    case kVK_Return:            return std::u16string(1, u'\r');
    case kVK_Tab:               return std::u16string(1, u'\t');
    case kVK_Escape:            return std::u16string(1, u'\x1B');
    case kVK_Delete:            return std::u16string(1, u'\x7F');
    case kVK_ANSI_KeypadEnter:  return std::u16string(1, u'\x03');
    default:                    return std::u16string();
    }
}

bool isModifier(unsigned short code)
{
    return code == kVK_Shift || code == kVK_RightShift || code == kVK_Control ||
           code == kVK_RightControl || code == kVK_Option || code == kVK_RightOption ||
           code == kVK_Command || code == kVK_RightCommand || code == kVK_CapsLock;
}


bool isShift(unsigned short code) { return code == kVK_Shift || code == kVK_RightShift; }
unsigned short leftShift() { return kVK_Shift; }
unsigned short grave()     { return kVK_ANSI_Grave; }
unsigned short enter()     { return kVK_Return; }
unsigned short w()         { return kVK_ANSI_W; }

} // namespace keys
} // namespace inputscript
} // namespace hfe
} // namespace dice
