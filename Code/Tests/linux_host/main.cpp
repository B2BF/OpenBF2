// [TEMP] [CHOSEN, not recovered] SDL queue-to-engine contract fixture; no window or gameplay.
// [RETIRE-WHEN: linux-host-contract-suite-replaced] Keep until equivalent native input coverage exists.
#include "../../BF2/Platform/macOS/CocoaHost.h"
#include "../../BF2/Platform/macOS/MacKeyMap.h"
#include "../../BF2/Platform/macOS/MacSystem.h"
#include "../../BF2/Platform/InputScript.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace dice::hfe::macos;
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
int main(int argc, char**)
{
    CocoaHost& host = CocoaHost::instance();
    check(host.initApplication(), "initialize headless SDL");
    Listener listener;
    host.addListener(&listener);
    if (argc > 1) {
        using namespace dice::hfe::inputscript;
        check(inputScript().active(), "shared script loaded");
        unsigned short name = 0;
        check(keys::lookup("f13", name) && dikFromMacKeyCode(name, false) == 100, "Linux script PC key mapping");
        check(!keys::lookup("missing-key", name), "unknown script key rejected");
        key(SDL_SCANCODE_F5, SDLK_F5, true);
        SDL_Event loss{}; loss.type = SDL_EVENT_WINDOW_FOCUS_LOST;
        check(SDL_PushEvent(&loss), "push real focus loss during script");
        host.pumpEvents();
        check(listener.keys.empty() && host.hasFocus(), "script isolates real keys and forces focus");
        host.pumpEvents();
        check(listener.keys.size() == 8 && !listener.keys[3].down, "script key presses and releases");
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
        check(host.quitRequested() && listener.closes == 1, "script quit");
        char capture[64];
        check(openbf2InputScriptTakeCapture(capture, sizeof(capture)) && std::string(capture) == "menu___one", "script capture request sanitized");
        check(!openbf2InputScriptTakeCapture(capture, sizeof(capture)), "capture request consumed once");
        host.removeListener(&listener);
        std::puts("PASS: shared Linux input script menu, capture request and isolation contracts");
        return 0;
    }
    key(SDL_SCANCODE_F12, SDLK_F12, true);
    key(SDL_SCANCODE_F12, SDLK_F12, false);
    key(SDL_SCANCODE_F13, SDLK_F13, true);
    key(SDL_SCANCODE_PRINTSCREEN, SDLK_PRINTSCREEN, true);
    key(SDL_SCANCODE_NONUSBACKSLASH, SDLK_BACKSLASH, true);
    key(SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, true);
    key(SDL_SCANCODE_BACKSPACE, SDLK_BACKSPACE, true);
    key(SDL_SCANCODE_C, SDLK_C, true, false, SDL_KMOD_LCTRL);
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, true);
    host.pumpEvents();
    check(listener.keys.size() == 9, "all physical key transitions delivered");
    const unsigned char expected[] = {88,88,100,183,86,1,14,46,28};
    for (unsigned i = 0; i < 9; ++i) check(dikFromMacKeyCode(listener.keys[i].keyCode, false) == expected[i], "DIK physical mapping");
    check(!listener.keys[1].down, "key release");
    check(listener.characters == std::vector<unsigned short>({27,8,3,13}), "control text, Escape and Backspace");
    check(host.consumeAsyncEnterState() == 0x8001, "Enter held and pressed");
    check(host.consumeAsyncEnterState() == 0x8000, "Enter press bit consumed");
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, true, true);
    host.pumpEvents();
    check(host.consumeAsyncEnterState() == 0x8000, "Enter repeat is not a fresh press");
    key(SDL_SCANCODE_KP_ENTER, SDLK_KP_ENTER, true);
    key(SDL_SCANCODE_RETURN, SDLK_RETURN, false);
    host.pumpEvents();
    check(host.consumeAsyncEnterState() == 0x8001, "keypad Enter stays held independently");
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
    const auto args = splitCommandLine("+modPath \"mods/a b\" +playerName \"hello\\\"world\"");
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
    std::puts("PASS: Linux host event, key, text, clipboard, focus and quit contracts");
}
