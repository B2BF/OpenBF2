// [TEMP] [CHOSEN, not recovered] Native host boundary regression for issue #36.
// Exercises the production tables, Cocoa normalization and window translator without OS SDKs.
#include "WindowMessageRecorder.h"
#include "../../BF2/Platform/macOS/MacKeyMap.h"
#include "../../BF2/Platform/macOS/CocoaText.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace dice::hfe;
namespace {
void check(bool value, const char* name)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
}
void cocoaText(WindowMessageRecorder& recorder, unsigned char dik, unsigned int modifiers,
               unsigned short character, const std::vector<unsigned int>& expected)
{
    host::HostTextEvent event{};
    event.dik = dik;
    event.modifiers = modifiers;
    macos::appendNormalizedCocoaText(event, character);
    recorder.messages.clear();
    recorder.onText(event);
    check(recorder.values(hostmsg::WM_CHAR) == expected, "Cocoa text reaches WM_CHAR unchanged after normalization");
}
}
int main()
{
    struct Mapping { unsigned short key; unsigned char ansi, iso; };
    const Mapping mappings[] = {
        {0x00,30,30}, {0x08,46,46}, {0x0A,86,41}, {0x32,41,86},
        {0x24,28,28}, {0x4C,156,156}, {0x33,14,14},
        {0x5D,125,125}, {0x5E,115,115}, {0x5F,179,179}, {0x66,123,123}, {0x68,112,112},
        {0x69,183,183}, {0x6B,70,70}, {0x71,197,197}
    };
    for (const auto& mapping : mappings) {
        check(macos::dikFromMacKeyCode(mapping.key, false) == mapping.ansi, "Cocoa ANSI/JIS physical key");
        check(macos::dikFromMacKeyCode(mapping.key, true) == mapping.iso, "Cocoa ISO physical key");
    }
    for (bool iso : {false, true}) {
        for (unsigned short key = 0; key < 128; ++key) {
            const unsigned char dik = macos::dikFromMacKeyCode(key, iso);
            if (dik) check(macos::dikFromMacKeyCode(macos::macKeyCodeFromDik(dik, iso), iso) == dik,
                           "Cocoa canonical reverse mapping preserves DIK");
        }
        check(macos::macKeyCodeFromDik(156, iso) == 0x4C, "canonical keypad Enter");
        check(macos::macKeyCodeFromDik(0, iso) == 0xFFFF, "unknown DIK has no Cocoa key");
    }
    check(macos::dikFromMacKeyCode(0x100 | 100, false) == 0, "no synthetic Linux token in Cocoa map");
    struct VirtualKey { unsigned char dik; unsigned int vk; };
    const VirtualKey virtualKeys[] = {
        {46,'C'}, {28,13}, {156,13}, {14,8}, {86,0xE2}, {100,0x7C}, {101,0x7D}, {102,0x7E},
        {219,0x5B}, {220,0x5C}, {56,0x12}, {184,0x12}, {0,0}
    };
    for (const auto& value : virtualKeys)
        check(host::win32VirtualKeyFromDik(value.dik) == value.vk, "shared DIK to virtual key");
    struct Label { unsigned char dik; const char* text; };
    const Label labels[] = {{56,"Alt"}, {184,"Right Alt"}, {219,"Super"}, {220,"Right Super"},
                            {69,"Num Lock"}, {100,"F13"}, {101,"F14"}, {102,"F15"}};
    for (const auto& label : labels)
        check(std::strcmp(host::dikDisplayName(label.dik), label.text) == 0, "shared key labels are platform neutral");

    WindowMessageRecorder recorder;
    cocoaText(recorder, 46, host::HostModControl, 3, {3});
    cocoaText(recorder, 156, 0, 3, {13});
    cocoaText(recorder, 28, 0, 3, {3});
    cocoaText(recorder, 14, 0, 0x7F, {8});
    cocoaText(recorder, 211, 0, 0x7F, {});
    cocoaText(recorder, 46, host::HostModSuper, 'c', {3});
    cocoaText(recorder, 46, host::HostModSuper, 'C', {3});
    cocoaText(recorder, 47, host::HostModSuper, 'v', {22});
    cocoaText(recorder, 46, 0, 'c', {'c'});
    cocoaText(recorder, 0, 0, 0xF700, {});
    cocoaText(recorder, 0, 0, 0xF8FF, {});
    cocoaText(recorder, 0, 0, 0x20AC, {0x80});
    cocoaText(recorder, 0, 0, 0x3042, {});
    cocoaText(recorder, 0, 0, 0xD800, {});

    recorder.messages.clear();
    recorder.onKey({156,true,false,0,0});
    check(recorder.messages.size() == 1 && recorder.messages.back().id == hostmsg::WM_KEYDOWN &&
          recorder.messages.back().wParam == 13 && recorder.messages.back().lParam == 0x011C0001,
          "keypad Enter retains extended scan code");
    recorder.onKey({46,true,true,host::HostModAlt,0});
    check(recorder.messages.back().id == hostmsg::WM_SYSKEYDOWN && recorder.messages.back().lParam == 0x602E0001,
          "Alt repeat has system-key context and previous-state bits");
    recorder.onKey({46,false,false,host::HostModAlt,0});
    check(recorder.messages.back().id == hostmsg::WM_SYSKEYUP &&
          static_cast<std::uint32_t>(recorder.messages.back().lParam) == 0xE02E0001u, "release has transition and previous-state bits");
    recorder.onKey({68,true,false,0,0});
    check(recorder.messages.back().id == hostmsg::WM_SYSKEYDOWN, "F10 is a system key");
    recorder.onKey({46,true,false,host::HostModSuper,0});
    check(recorder.messages.back().id == hostmsg::WM_KEYDOWN && recorder.messages.back().lParam == 0x002E0001,
          "Super is not Alt in the shared translator");
    const auto keyCount = recorder.messages.size();
    recorder.onKey({0,true,false,0,0});
    check(recorder.messages.size() == keyCount, "unmapped key has no window message");

    recorder.messages.clear();
    recorder.onFocusChanged(true, true);
    recorder.onFocusChanged(true, true);
    recorder.onFocusChanged(false, true);
    check(recorder.messages.size() == 4 && recorder.messages[1].id == hostmsg::WM_SETFOCUS &&
          recorder.messages[3].id == hostmsg::WM_KILLFOCUS, "focus transitions are emitted once");
    recorder.messages.clear();
    recorder.onMouseMove({6,-3,40,20,true,true,0});
    recorder.onMouseButton({0,true,1,40,20,true,0});
    recorder.onMouseWheel({120,true,0});
    check(recorder.messages.empty(), "captured gameplay mouse does not drive menu messages");
    recorder.onMouseMove({0,0,40,20,true,false,0});
    check(recorder.messages.size() == 2 && recorder.messages[0].id == hostmsg::WM_SETCURSOR &&
          recorder.messages[1].id == hostmsg::WM_MOUSEMOVE && recorder.messages[1].lParam == 0x00140028,
          "menu motion sets cursor then delivers back-buffer coordinates");
    std::puts("PASS: native host DIK maps, Cocoa text and window-message contracts");
}
