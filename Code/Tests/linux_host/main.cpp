// [TEMP] [CHOSEN, not recovered] SDL queue-to-engine contract fixture; no window or gameplay.
// [RETIRE-WHEN: linux-host-contract-suite-replaced] Keep until equivalent native input coverage exists.
#include "../../BF2/Platform/Host/ClientHost.h"
#include "../native_host/WindowMessageRecorder.h"
#include "../../BF2/Platform/macOS/MacSystem.h"
#include "../../BF2/Platform/InputScript.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace dice::hfe::host;
namespace hostmsg = dice::hfe::hostmsg;
namespace {
void check(bool value, const char* name)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s (%s)\n", name, SDL_GetError()); std::exit(1); }
}
struct Listener : IHostEventListener
{
    std::vector<HostKeyEvent> keys;
    std::vector<unsigned short> characters;
    std::vector<HostMouseMoveEvent> moves;
    std::vector<HostMouseButtonEvent> buttons;
    int focusChanges = 0, closes = 0, wheel = 0;
    void onKey(const HostKeyEvent& event) override { keys.push_back(event); }
    void onText(const HostTextEvent& event) override { characters.insert(characters.end(), event.characters.begin(), event.characters.end()); }
    void onFocusChanged(bool, bool) override { ++focusChanges; }
    void onCloseRequested() override { ++closes; }
    void onMouseWheel(const HostMouseWheelEvent& event) override { wheel += event.delta; }
    void onMouseMove(const HostMouseMoveEvent& event) override { moves.push_back(event); }
    void onMouseButton(const HostMouseButtonEvent& event) override { buttons.push_back(event); }
};
void key(SDL_Scancode scan, SDL_Keycode code, bool down, bool repeat = false, SDL_Keymod mod = SDL_KMOD_NONE)
{
    SDL_Event event{};
    event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    event.key.scancode = scan;
    event.key.key = code;
    event.key.down = down;
    event.key.repeat = repeat;
    event.key.mod = mod;
    check(SDL_PushEvent(&event), "push key");
}
}
int runInputLayoutCheck();
int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--layout") return runInputLayoutCheck();
    ClientHost& host = ClientHost::instance();
    check(host.initApplication(), "initialize headless SDL");
    Listener listener;
    WindowMessageRecorder messages;
    host.addListener(&listener);
    host.addListener(&messages);
    if (argc > 1) {
        using namespace dice::hfe::inputscript;
        check(inputScript().active(), "shared script loaded");
        unsigned short name = 0;
        check(keys::lookup("f13", name) && name == 100, "Linux script F13 is direct DIK");
        check(keys::lookup("f14", name) && name == 101, "Linux script F14 is direct DIK");
        check(keys::lookup("f15", name) && name == 102, "Linux script F15 is direct DIK");
        check(!keys::lookup("missing-key", name), "unknown script key rejected");
        key(SDL_SCANCODE_F5, SDLK_F5, true);
        SDL_Event loss{}; loss.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        check(SDL_PushEvent(&loss), "push real focus loss during script");
        host.pumpEvents();
        check(listener.keys.empty() && host.hasFocus(), "script isolates real keys and forces focus");
        host.pumpEvents();
        check(listener.keys.size() == 14 && !listener.keys[3].down, "script key presses and releases");
        check(listener.keys[8].dik == 100 && listener.keys[10].dik == 101 && listener.keys[12].dik == 102,
              "script F13-F15 events retain direct DIK");
        check(listener.characters == std::vector<unsigned short>({u'A',13,27}), "script Shift, Enter and Escape menu text");
        check(listener.moves.size() == 1 && listener.moves[0].x == 40 && listener.moves[0].y == 20 && !listener.moves[0].captured,
              "absolute menu mouse coordinates");
        check(listener.buttons.size() == 2 && listener.buttons[0].down && !listener.buttons[1].down && listener.wheel == 120,
              "script button release and wheel");
        check(host.consumeAsyncEnterState() == 1, "script Enter press latched after release");
        host.setRelativeMouseMode(true);
        host.pumpEvents();
        check(listener.moves.size() == 2 && listener.moves[1].dx == 6 && listener.moves[1].dy == -3 &&
              listener.moves[1].x == 40 && listener.moves[1].captured, "script relative counts with parked pointer");
        check(listener.characters.back() == 3 && host.modifierFlags() == 0, "script Ctrl-C and modifier release");
        check(messages.values(hostmsg::WM_CHAR) == std::vector<unsigned int>({'A',13,27,3}),
              "script text reaches production window adapter, preserving Ctrl-C ETX");
        check(host.quitRequested() && listener.closes == 1, "script quit");
        char capture[64];
        check(openbf2InputScriptTakeCapture(capture, sizeof(capture)) && std::string(capture) == "menu___one", "script capture request sanitized");
        check(!openbf2InputScriptTakeCapture(capture, sizeof(capture)), "capture request consumed once");
        host.removeListener(&listener);
        host.removeListener(&messages);
        std::puts("PASS: shared Linux input script menu, capture request and isolation contracts");
        return 0;
    }
    key(SDL_SCANCODE_F12, SDLK_F12, true);
    key(SDL_SCANCODE_F12, SDLK_F12, false);
    key(SDL_SCANCODE_F13, SDLK_F13, true);
    key(SDL_SCANCODE_F14, SDLK_F14, true);
    key(SDL_SCANCODE_F15, SDLK_F15, true);
    key(SDL_SCANCODE_PRINTSCREEN, SDLK_PRINTSCREEN, true);
    key(SDL_SCANCODE_NONUSBACKSLASH, SDLK_BACKSLASH, true);
    key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, true);
    key(SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, true);
    key(SDL_SCANCODE_C, SDLK_C, true, false, SDL_KMOD_LCTRL);
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, true);
    host.pumpEvents();
    check(listener.keys.size() == 11, "all physical key transitions delivered");
    const unsigned char expected[] = {88,88,100,101,102,183,86,1,14,46,28};
    for (unsigned i = 0; i < sizeof(expected); ++i) check(listener.keys[i].dik == expected[i], "direct DIK physical mapping");
    check(!listener.keys[1].down, "key release");
    check(listener.characters == std::vector<unsigned short>({27,8,3,13}), "control text, Escape and Backspace");
    check(messages.values(hostmsg::WM_KEYDOWN) == std::vector<unsigned int>({0x7B,0x7C,0x7D,0x7E,0x2C,0xE2,27,8,'C',13}),
          "SDL queue reaches production WM_KEYDOWN virtual-key mapping");
    check(messages.values(hostmsg::WM_CHAR) == std::vector<unsigned int>({27,8,3,13}),
          "SDL Ctrl-C remains ETX at the engine WM_CHAR boundary");
    check(host.consumeAsyncEnterState() == 0x8001, "Enter held and pressed");
    check(host.consumeAsyncEnterState() == 0x8000, "Enter press bit consumed");
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, true, true);
    messages.messages.clear();
    host.pumpEvents();
    check(host.consumeAsyncEnterState() == 0x8000, "Enter repeat is not a fresh press");
    check(messages.messages.front().id == hostmsg::WM_KEYDOWN && messages.messages.front().lParam == 0x401C0001,
          "SDL repeat reaches WM_KEYDOWN previous-state bit");
    key(SDL_SCANCODE_KP_ENTER, SDLK_KP_ENTER, true);
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, false);
    messages.messages.clear();
    host.pumpEvents();
    check(host.consumeAsyncEnterState() == 0x8001, "keypad Enter stays held independently");
    check(messages.messages.front().id == hostmsg::WM_KEYDOWN && messages.messages.front().lParam == 0x011C0001,
          "SDL keypad Enter preserves extended scan code");
    check(messages.values(hostmsg::WM_CHAR) == std::vector<unsigned int>({13}), "SDL keypad Enter reaches WM_CHAR as CR");
    struct Label { unsigned char dik; const char* value; };
    const Label labels[] = {{56,"Alt"}, {184,"Right Alt"}, {219,"Super"}, {220,"Right Super"},
                            {69,"Num Lock"}, {100,"F13"}, {101,"F14"}, {102,"F15"}};
    for (const auto& label : labels)
        check(host.keyDisplayName(label.dik) && std::string(host.keyDisplayName(label.dik)) == label.value,
              "Linux key labels use PC terminology");
    key(SDL_SCANCODE_LCTRL, SDLK_LCTRL, true, false, SDL_KMOD_LCTRL);
    // SDL_PushEvent delivers events without changing SDL's polled hardware state.
    SDL_SetModState(SDL_KMOD_LCTRL);
    host.pumpEvents();
    check(host.controlShortcutDown() && (host.modifierFlags() & (HostModControl | HostModLeftControl)) ==
          (HostModControl | HostModLeftControl), "Linux Ctrl activates shortcuts and retains side-specific state");
    key(SDL_SCANCODE_LCTRL, SDLK_LCTRL, false);
    key(SDL_SCANCODE_LGUI, SDLK_LGUI, true, false, SDL_KMOD_LGUI);
    SDL_SetModState(SDL_KMOD_LGUI);
    host.pumpEvents();
    check(!host.controlShortcutDown() && (host.modifierFlags() & (HostModSuper | HostModLeftSuper)) ==
          (HostModSuper | HostModLeftSuper), "Linux Super is not a Ctrl shortcut");
    key(SDL_SCANCODE_LGUI, SDLK_LGUI, false);
    key(SDL_SCANCODE_RALT, SDLK_RALT, true, false, SDL_KMOD_RALT);
    SDL_SetModState(SDL_KMOD_RALT);
    messages.messages.clear();
    host.pumpEvents();
    check((host.modifierFlags() & (HostModAlt | HostModRightAlt)) == (HostModAlt | HostModRightAlt) &&
          messages.messages.back().id == hostmsg::WM_SYSKEYDOWN && messages.messages.back().lParam == 0x21380001,
          "right Alt retains extended scan and system-key context");
    key(SDL_SCANCODE_RALT, SDLK_RALT, false);
    SDL_SetModState(SDL_KMOD_NONE);
    host.pumpEvents();
    check(!host.controlShortcutDown() && host.modifierFlags() == 0, "modifier release clears shortcut state");
    SDL_Event event{};
    event.type = SDL_EVENT_TEXT_INPUT;
    event.text.text = "\xC3\xA9\xE2\x82\xAC";
    check(SDL_PushEvent(&event), "push UTF-8 text");
    event = {};
    event.type = SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y = 1;
    event.wheel.direction = SDL_MOUSEWHEEL_FLIPPED;
    check(SDL_PushEvent(&event), "push wheel");
    host.pumpEvents();
    check(listener.characters[listener.characters.size()-2] == 0xE9 && listener.characters.back() == 0x20AC, "UTF-8 text decoding");
    check(listener.wheel == -120, "natural scroll normalization");
    check(host.setClipboardText("caf\xE9 \x80"), "set Windows-1252 clipboard");
    std::string clipboard;
    check(host.getClipboardText(clipboard) && clipboard == "caf\xE9 \x80", "Windows-1252 clipboard round trip");
    event = {};
    event.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    check(SDL_PushEvent(&event), "push focus gained");
    event.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    check(SDL_PushEvent(&event), "push focus lost");
    host.pumpEvents();
    check(listener.focusChanges == 2 && host.deactivationCount() == 1 && !host.hasFocus(), "focus transitions");
    check(host.consumeAsyncEnterState() == 0, "focus loss clears Enter state");
    const auto args = dice::hfe::macos::splitCommandLine("+modPath \"mods/a b\" +playerName \"hello\\\"world\"");
    check(args == std::vector<std::string>({"+modPath", "mods/a b", "+playerName", "hello\"world"}), "relaunch argument quoting");
    setenv("BF2_CAPTURE_DIR", "/tmp", 1);
    key(SDL_SCANCODE_F12, SDLK_F12, true);
    key(SDL_SCANCODE_F12, SDLK_F12, true, true);
    host.pumpEvents();
    char capture[32];
    check(openbf2InputScriptTakeCapture(capture, sizeof(capture)) && std::string(capture) == "manual", "manual F12 capture requested");
    check(!openbf2InputScriptTakeCapture(capture, sizeof(capture)), "F12 repeat does not queue capture");
    host.requestQuit();
    host.requestQuit();
    check(listener.closes == 1 && host.quitRequested(), "close notification once");
    host.removeListener(&listener);
    host.removeListener(&messages);
    std::puts("PASS: Linux host event, key, text, clipboard, focus and quit contracts");
}
