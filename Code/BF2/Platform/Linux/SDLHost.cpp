// [CHOSEN, not recovered] SDL implementation of the shared native client host.
// https://github.com/kr4uzi/OpenBF2/issues/36; docs/linux-vulkan-client.md.
// Physical keys use BF2 DIK codes; renderer handles are borrowed SDL_Window pointers.
#if defined(__linux__) && defined(BF2_LINUX_CLIENT) && !DEDICATED
#include "../Host/ClientHost.h"
#include "../Host/KeyMap.h"
#include "../Host/Gamepad.h"
#include "../Host/CursorFile.h"
#include "../InputScript.h"
#include "ClientFilePaths.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <codecvt>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <locale>
#include <memory>
#include <stdexcept>

namespace dice { namespace hfe { namespace host {
namespace {
struct PhysicalKey { SDL_Scancode scan; unsigned char dik; };
const PhysicalKey keys[] = {
    {SDL_SCANCODE_ESCAPE,1}, {SDL_SCANCODE_1,2}, {SDL_SCANCODE_2,3},
    {SDL_SCANCODE_3,4}, {SDL_SCANCODE_4,5}, {SDL_SCANCODE_5,6},
    {SDL_SCANCODE_6,7}, {SDL_SCANCODE_7,8}, {SDL_SCANCODE_8,9},
    {SDL_SCANCODE_9,10}, {SDL_SCANCODE_0,11}, {SDL_SCANCODE_MINUS,12},
    {SDL_SCANCODE_EQUALS,13}, {SDL_SCANCODE_BACKSPACE,14}, {SDL_SCANCODE_TAB,15},
    {SDL_SCANCODE_Q,16}, {SDL_SCANCODE_W,17}, {SDL_SCANCODE_E,18},
    {SDL_SCANCODE_R,19}, {SDL_SCANCODE_T,20}, {SDL_SCANCODE_Y,21},
    {SDL_SCANCODE_U,22}, {SDL_SCANCODE_I,23}, {SDL_SCANCODE_O,24},
    {SDL_SCANCODE_P,25}, {SDL_SCANCODE_LEFTBRACKET,26}, {SDL_SCANCODE_RIGHTBRACKET,27},
    {SDL_SCANCODE_RETURN,28}, {SDL_SCANCODE_LCTRL,29}, {SDL_SCANCODE_A,30},
    {SDL_SCANCODE_S,31}, {SDL_SCANCODE_D,32}, {SDL_SCANCODE_F,33},
    {SDL_SCANCODE_G,34}, {SDL_SCANCODE_H,35}, {SDL_SCANCODE_J,36},
    {SDL_SCANCODE_K,37}, {SDL_SCANCODE_L,38}, {SDL_SCANCODE_SEMICOLON,39},
    {SDL_SCANCODE_APOSTROPHE,40}, {SDL_SCANCODE_GRAVE,41}, {SDL_SCANCODE_LSHIFT,42},
    {SDL_SCANCODE_BACKSLASH,43}, {SDL_SCANCODE_Z,44}, {SDL_SCANCODE_X,45},
    {SDL_SCANCODE_C,46}, {SDL_SCANCODE_V,47}, {SDL_SCANCODE_B,48},
    {SDL_SCANCODE_N,49}, {SDL_SCANCODE_M,50}, {SDL_SCANCODE_COMMA,51},
    {SDL_SCANCODE_PERIOD,52}, {SDL_SCANCODE_SLASH,53}, {SDL_SCANCODE_RSHIFT,54},
    {SDL_SCANCODE_KP_MULTIPLY,55}, {SDL_SCANCODE_LALT,56}, {SDL_SCANCODE_SPACE,57},
    {SDL_SCANCODE_CAPSLOCK,58}, {SDL_SCANCODE_F1,59}, {SDL_SCANCODE_F2,60},
    {SDL_SCANCODE_F3,61}, {SDL_SCANCODE_F4,62}, {SDL_SCANCODE_F5,63},
    {SDL_SCANCODE_F6,64}, {SDL_SCANCODE_F7,65}, {SDL_SCANCODE_F8,66},
    {SDL_SCANCODE_F9,67}, {SDL_SCANCODE_F10,68}, {SDL_SCANCODE_NUMLOCKCLEAR,69},
    {SDL_SCANCODE_SCROLLLOCK,70}, {SDL_SCANCODE_KP_7,71}, {SDL_SCANCODE_KP_8,72},
    {SDL_SCANCODE_KP_9,73}, {SDL_SCANCODE_KP_MINUS,74}, {SDL_SCANCODE_KP_4,75},
    {SDL_SCANCODE_KP_5,76}, {SDL_SCANCODE_KP_6,77}, {SDL_SCANCODE_KP_PLUS,78},
    {SDL_SCANCODE_KP_1,79}, {SDL_SCANCODE_KP_2,80}, {SDL_SCANCODE_KP_3,81},
    {SDL_SCANCODE_KP_0,82}, {SDL_SCANCODE_KP_PERIOD,83}, {SDL_SCANCODE_NONUSBACKSLASH,86},
    {SDL_SCANCODE_F11,87}, {SDL_SCANCODE_F12,88}, {SDL_SCANCODE_F13,100},
    {SDL_SCANCODE_F14,101}, {SDL_SCANCODE_F15,102}, {SDL_SCANCODE_KP_EQUALS,141},
    {SDL_SCANCODE_KP_ENTER,156}, {SDL_SCANCODE_RCTRL,157}, {SDL_SCANCODE_KP_DIVIDE,181},
    {SDL_SCANCODE_PRINTSCREEN,183}, {SDL_SCANCODE_RALT,184}, {SDL_SCANCODE_PAUSE,197},
    {SDL_SCANCODE_HOME,199}, {SDL_SCANCODE_UP,200}, {SDL_SCANCODE_PAGEUP,201},
    {SDL_SCANCODE_LEFT,203}, {SDL_SCANCODE_RIGHT,205}, {SDL_SCANCODE_END,207},
    {SDL_SCANCODE_DOWN,208}, {SDL_SCANCODE_PAGEDOWN,209}, {SDL_SCANCODE_INSERT,210},
    {SDL_SCANCODE_DELETE,211}, {SDL_SCANCODE_LGUI,219}, {SDL_SCANCODE_RGUI,220},
    {SDL_SCANCODE_APPLICATION,221}, {SDL_SCANCODE_MUTE,160},
    {SDL_SCANCODE_VOLUMEDOWN,174}, {SDL_SCANCODE_VOLUMEUP,176},
    {SDL_SCANCODE_INTERNATIONAL3,125}, {SDL_SCANCODE_INTERNATIONAL1,115},
    {SDL_SCANCODE_LANG1,112}, {SDL_SCANCODE_LANG2,123}, {SDL_SCANCODE_KP_COMMA,179}
};

unsigned char dikFromScancode(SDL_Scancode scan)
{
    for (const PhysicalKey& key : keys)
        if (key.scan == scan) return key.dik;
    return 0;
}

unsigned modifiers(SDL_Keymod mod)
{
    unsigned result = 0;
    if (mod & SDL_KMOD_SHIFT) result |= HostModShift;
    if (mod & SDL_KMOD_CTRL) result |= HostModControl;
    if (mod & SDL_KMOD_ALT) result |= HostModAlt;
    if (mod & SDL_KMOD_GUI) result |= HostModSuper;
    if (mod & SDL_KMOD_CAPS) result |= HostModCapsLock;
    if (mod & SDL_KMOD_NUM) result |= HostModNumericPad;
    if (mod & SDL_KMOD_LSHIFT) result |= HostModLeftShift;
    if (mod & SDL_KMOD_RSHIFT) result |= HostModRightShift;
    if (mod & SDL_KMOD_LCTRL) result |= HostModLeftControl;
    if (mod & SDL_KMOD_RCTRL) result |= HostModRightControl;
    if (mod & SDL_KMOD_LALT) result |= HostModLeftAlt;
    if (mod & SDL_KMOD_RALT) result |= HostModRightAlt;
    if (mod & SDL_KMOD_LGUI) result |= HostModLeftSuper;
    if (mod & SDL_KMOD_RGUI) result |= HostModRightSuper;
    return result;
}

std::u16string utf16(const char* text)
{
    try { return std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().from_bytes(text); }
    catch (const std::range_error&) { return {}; }
}

const char16_t cp1252High[] = {
    0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,
    0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
    0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178
};

void report(const char* operation)
{
    std::fprintf(stderr, "[SDL host] %s: %s\n", operation, SDL_GetError());
    std::fflush(stderr);
}

struct Cursor
{
    std::vector<SDL_Cursor*> frames;
    std::vector<int> steps, jiffies;
    ~Cursor() { for (SDL_Cursor* frame : frames) SDL_DestroyCursor(frame); }
};

struct Pad
{
    SDL_JoystickID id;
    SDL_Gamepad* handle;
};
std::vector<Pad> pads;
void updatePads()
{
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (auto it = pads.begin(); it != pads.end();) {
        if (!SDL_GamepadConnected(it->handle)) {
            SDL_CloseGamepad(it->handle);
            it = pads.erase(it);
        } else ++it;
    }
    for (int i = 0; i < count; ++i) {
        if (std::find_if(pads.begin(), pads.end(), [=](const Pad& p) { return p.id == ids[i]; }) != pads.end())
            continue;
        if (SDL_Gamepad* handle = SDL_OpenGamepad(ids[i])) pads.push_back({ids[i], handle});
    }
    SDL_free(ids);
}
}

struct ClientHost::Impl : inputscript::IInputScriptSink
{
    SDL_Window* window = nullptr;
    int width = 0, height = 0, cursorCount = 0;
    bool initialized = false, quit = false, focused = false, relative = false, hidden = false;
    bool enter[2] = {}, enterPressed = false;
    unsigned deactivations = 0;
    float wheelRemainder = 0;
    unsigned char lastKey = 0;
    bool lastRepeat = false;
    std::vector<IHostEventListener*> listeners;
    std::vector<std::unique_ptr<Cursor>> cursors;
    Cursor* arrow = nullptr;
    Cursor* busy = nullptr;
    Cursor* cursor = nullptr;
    Uint64 cursorStart = 0;
    // [TEMP] Shared input-script isolation; the hardware cursor remains under user control.
    // [RETIRE-WHEN: client-test-automation-replaced] Replace with equivalent host input tests.
    bool scripted = false, scriptedFocus = false, scriptPointerSet = false;
    SDL_Keymod scriptModifiers = SDL_KMOD_NONE;
    float scriptX = 0, scriptY = 0;
    unsigned scriptButtons = 0;

    template<class Callback> void dispatch(Callback callback)
    {
        const auto snapshot = listeners;
        for (IHostEventListener* listener : snapshot)
            if (std::find(listeners.begin(), listeners.end(), listener) != listeners.end()) callback(*listener);
    }
    bool captured() const { return focused && (scripted ? relative : window && SDL_GetWindowRelativeMouseMode(window)); }
    void updateCursor()
    {
        if (!initialized || scripted) return;
        if (cursor && !cursor->frames.empty()) {
            std::size_t frame = 0;
            if (!cursor->steps.empty()) {
                Uint64 duration = 0;
                for (int jiffies : cursor->jiffies) duration += jiffies;
                Uint64 elapsed = ((SDL_GetTicks() - cursorStart) * 60 / 1000) % duration;
                std::size_t step = 0;
                while (elapsed >= static_cast<Uint64>(cursor->jiffies[step])) elapsed -= cursor->jiffies[step++];
                frame = cursor->steps[step];
            }
            SDL_SetCursor(cursor->frames[frame]);
        }
        if (captured() || (focused && (hidden || cursorCount < 0))) SDL_HideCursor();
        else SDL_ShowCursor();
    }
    void focus(bool value)
    {
        if (scriptedFocus) value = true;
        if (focused == value) return;
        focused = value;
        if (!value) { ++deactivations; enter[0] = enter[1] = enterPressed = false; }
        if (window && !scripted && !SDL_SetWindowRelativeMouseMode(window, relative && focused)) report("relative mouse focus");
        dispatch([=](IHostEventListener& listener) { listener.onAppActivationChanged(value); });
        dispatch([=](IHostEventListener& listener) { listener.onFocusChanged(value, value); });
        updateCursor();
    }
    void position(float& x, float& y) const
    {
        int w = 0, h = 0;
        if (window && SDL_GetWindowSize(window, &w, &h) && w > 0 && h > 0) {
            x *= static_cast<float>(width) / w;
            y *= static_cast<float>(height) / h;
        }
    }
    void text(const std::u16string& chars, unsigned char dik, bool repeat, unsigned mod)
    {
        if (chars.empty()) return;
        HostTextEvent event{dik, mod, {}, repeat};
        event.characters.assign(chars.begin(), chars.end());
        dispatch([&](IHostEventListener& listener) { listener.onText(event); });
    }
    void event(const SDL_Event& event)
    {
        if (scripted && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP ||
            event.type == SDL_EVENT_TEXT_INPUT || event.type == SDL_EVENT_MOUSE_MOTION ||
            event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
            event.type == SDL_EVENT_MOUSE_WHEEL)) return;
        const unsigned time = static_cast<unsigned>(event.common.timestamp / 1000000);
        switch (event.type) {
        case SDL_EVENT_QUIT: ClientHost::instance().requestQuit(); break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: ClientHost::instance().requestQuit(); break;
        case SDL_EVENT_WINDOW_FOCUS_GAINED: focus(true); break;
        case SDL_EVENT_WINDOW_FOCUS_LOST: focus(false); break;
        case SDL_EVENT_WINDOW_MINIMIZED:
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_RESIZED:
            dispatch([&](IHostEventListener& listener) {
                listener.onResize(width, height, event.type == SDL_EVENT_WINDOW_MINIMIZED);
            });
            break;
        case SDL_EVENT_KEY_DOWN:
        case SDL_EVENT_KEY_UP: {
            const SDL_KeyboardEvent& key = event.key;
            if (key.down && !key.repeat && key.scancode == SDL_SCANCODE_F12) {
                const char* directory = std::getenv("BF2_CAPTURE_DIR");
                if (directory && *directory) inputscript::inputScript().requestCapture("manual");
            }
            const unsigned char dik = dikFromScancode(key.scancode);
            if (key.scancode == SDL_SCANCODE_RETURN || key.scancode == SDL_SCANCODE_KP_ENTER) {
                enter[key.scancode == SDL_SCANCODE_KP_ENTER] = key.down;
                if (key.down && !key.repeat) enterPressed = true;
            }
            if (dik) {
                HostKeyEvent translated{dik, key.down, key.repeat, modifiers(key.mod), time};
                dispatch([&](IHostEventListener& listener) { listener.onKey(translated); });
            }
            if (!key.down) break;
            lastKey = dik;
            lastRepeat = key.repeat;
            char16_t control = 0;
            if (key.scancode == SDL_SCANCODE_RETURN || key.scancode == SDL_SCANCODE_KP_ENTER) control = 13;
            else if (key.scancode == SDL_SCANCODE_ESCAPE) control = 27;
            else if (key.scancode == SDL_SCANCODE_TAB) control = 9;
            else if (key.scancode == SDL_SCANCODE_BACKSPACE) control = 8;
            else if ((key.mod & SDL_KMOD_CTRL) && !(key.mod & SDL_KMOD_ALT) && key.key >= SDLK_A && key.key <= SDLK_Z)
                control = static_cast<char16_t>(key.key - SDLK_A + 1);
            if (control) text(std::u16string(1, control), dik, key.repeat, modifiers(key.mod));
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            text(utf16(event.text.text), lastKey, lastRepeat, modifiers(SDL_GetModState()));
            break;
        case SDL_EVENT_MOUSE_MOTION: {
            HostMouseMoveEvent translated{event.motion.xrel, event.motion.yrel, event.motion.x, event.motion.y,
                                          false, captured(), time};
            position(translated.x, translated.y);
            translated.insideView = translated.x >= 0 && translated.y >= 0 && translated.x < width && translated.y < height;
            dispatch([&](IHostEventListener& listener) { listener.onMouseMove(translated); });
            break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            const int button = event.button.button == SDL_BUTTON_LEFT ? 0 : event.button.button == SDL_BUTTON_RIGHT ? 1 :
                               event.button.button == SDL_BUTTON_MIDDLE ? 2 : event.button.button - 1;
            HostMouseButtonEvent translated{button, event.button.down, event.button.clicks, event.button.x, event.button.y, captured(), time};
            position(translated.x, translated.y);
            dispatch([&](IHostEventListener& listener) { listener.onMouseButton(translated); });
            break;
        }
        case SDL_EVENT_MOUSE_WHEEL: {
            wheelRemainder += event.wheel.y * (event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -120 : 120);
            const int delta = static_cast<int>(wheelRemainder);
            wheelRemainder -= delta;
            if (delta) {
                HostMouseWheelEvent translated{delta, captured(), time};
                dispatch([&](IHostEventListener& listener) { listener.onMouseWheel(translated); });
            }
            break;
        }
        default: break;
        }
    }
    void scriptKey(unsigned short keyCode, bool down, const std::u16string& characters) override
    {
        if (!keyCode || keyCode > 0xFF) return;
        const unsigned char dik = static_cast<unsigned char>(keyCode);
        SDL_Keymod mask = SDL_KMOD_NONE;
        switch (dik) {
        case 42: mask = SDL_KMOD_LSHIFT; break; case 54: mask = SDL_KMOD_RSHIFT; break;
        case 29: mask = SDL_KMOD_LCTRL; break; case 157: mask = SDL_KMOD_RCTRL; break;
        case 56: mask = SDL_KMOD_LALT; break; case 184: mask = SDL_KMOD_RALT; break;
        case 219: mask = SDL_KMOD_LGUI; break; case 220: mask = SDL_KMOD_RGUI; break;
        case 58: if (down) scriptModifiers ^= SDL_KMOD_CAPS; break;
        default: break;
        }
        if (down) scriptModifiers |= mask;
        else scriptModifiers &= ~mask;
        if (dik == 28 || dik == 156) {
            enter[dik == 156] = down;
            if (down) enterPressed = true;
        }
        const unsigned mod = modifiers(scriptModifiers);
        const HostKeyEvent translated{dik, down, false, mod, ClientHost::tickCountMs()};
        dispatch([&](IHostEventListener& listener) { listener.onKey(translated); });
        if (down && !mask) {
            std::u16string chars = characters;
            if ((mod & HostModControl) && !(mod & HostModAlt))
                for (char16_t& c : chars) if ((c | 0x20) >= 'a' && (c | 0x20) <= 'z') c &= 0x1F;
            text(chars, dik, false, mod);
        }
    }
    void scriptPointerInit()
    {
        if (scriptPointerSet) return;
        scriptPointerSet = true;
        scriptX = width * 0.5f;
        scriptY = height * 0.5f;
    }
    void scriptMove(float dx, float dy)
    {
        const HostMouseMoveEvent translated{dx, dy, scriptX, scriptY,
            scriptX >= 0 && scriptY >= 0 && scriptX < width && scriptY < height,
            captured(), ClientHost::tickCountMs()};
        dispatch([&](IHostEventListener& listener) { listener.onMouseMove(translated); });
    }
    void scriptMouseTo(float x, float y) override
    {
        scriptPointerInit();
        const float dx = x - scriptX, dy = y - scriptY;
        scriptX = x; scriptY = y;
        scriptMove(dx, dy);
    }
    void scriptMouseBy(float dx, float dy) override
    {
        scriptPointerInit();
        if (!relative) { scriptX += dx; scriptY += dy; }
        scriptMove(dx, dy);
    }
    void scriptButton(int button, bool down) override
    {
        scriptPointerInit();
        if (button < 0 || button > 31) return;
        const unsigned bit = 1u << button;
        if (down) scriptButtons |= bit;
        else { if (!(scriptButtons & bit)) return; scriptButtons &= ~bit; }
        const HostMouseButtonEvent translated{button, down, 1, scriptX, scriptY, captured(), ClientHost::tickCountMs()};
        dispatch([&](IHostEventListener& listener) { listener.onMouseButton(translated); });
    }
    void scriptWheel(int delta) override
    {
        const HostMouseWheelEvent translated{delta, captured(), ClientHost::tickCountMs()};
        dispatch([&](IHostEventListener& listener) { listener.onMouseWheel(translated); });
    }
    std::u16string scriptKeyCharacter(unsigned short code, bool shift) override
    {
        if (code > 0xFF) return {};
        return ClientHost::instance().keyCharacter(static_cast<unsigned char>(code),
                                                   shift ? static_cast<unsigned>(HostModShift) : 0u);
    }
    void scriptQuit() override { ClientHost::instance().requestQuit(); }
};

ClientHost& ClientHost::instance() { static ClientHost host; return host; }
ClientHost::ClientHost() : m_impl(new Impl) {}
ClientHost::~ClientHost()
{
    destroyWindow();
    for (const Pad& pad : pads) SDL_CloseGamepad(pad.handle);
    pads.clear();
    const bool initialized = m_impl->initialized;
    delete m_impl;
    if (initialized) SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD);
}
bool ClientHost::initApplication()
{
    if (m_impl->initialized) return true;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { report("initialize"); return false; }
    m_impl->initialized = true;
    m_impl->scripted = inputscript::inputScript().loadFromEnvironment();
    const char* realFocus = std::getenv("BF2_INPUT_SCRIPT_REAL_FOCUS");
    m_impl->scriptedFocus = m_impl->scripted && !(realFocus && realFocus[0] == '1');
    if (m_impl->scriptedFocus) m_impl->focus(true);
    return true;
}
bool ClientHost::createWindow(const HostWindowDesc& desc)
{
    if (hasWindow() || !initApplication() || desc.width <= 0 || desc.height <= 0) return false;
    m_impl->window = SDL_CreateWindow(desc.title.c_str(), desc.width, desc.height,
                                     SDL_WINDOW_VULKAN | (desc.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
                                     (m_impl->scriptedFocus ? SDL_WINDOW_NOT_FOCUSABLE : 0));
    if (!m_impl->window) { report("create window"); return false; }
    m_impl->width = desc.width;
    m_impl->height = desc.height;
    m_impl->hidden = desc.hideCursor;
    m_impl->quit = false;
    if (!desc.fullscreen) SDL_SetWindowPosition(m_impl->window, desc.x, desc.y);
    if (!SDL_StartTextInput(m_impl->window)) { report("start text input"); destroyWindow(); return false; }
    setCursorShape(HostCursorArrow);
    m_impl->focus((SDL_GetWindowFlags(m_impl->window) & SDL_WINDOW_INPUT_FOCUS) != 0);
    return true;
}
void ClientHost::destroyWindow()
{
    if (!m_impl->window) return;
    m_impl->scriptedFocus = false;
    m_impl->focus(false);
    SDL_StopTextInput(m_impl->window);
    SDL_DestroyWindow(m_impl->window);
    m_impl->window = nullptr;
    m_impl->relative = false;
    m_impl->width = m_impl->height = 0;
}
bool ClientHost::hasWindow() const { return m_impl->window != nullptr; }
void* ClientHost::rendererWindowHandle() const { return m_impl->window; }
int ClientHost::clientWidth() const { return m_impl->width; }
int ClientHost::clientHeight() const { return m_impl->height; }
void ClientHost::show(bool visible)
{
    if (!hasWindow()) return;
    if (!(visible ? SDL_ShowWindow(m_impl->window) : SDL_HideWindow(m_impl->window))) report("show window");
    if (visible && !m_impl->scriptedFocus) SDL_RaiseWindow(m_impl->window);
}
bool ClientHost::isVisible() const { return hasWindow() && !(SDL_GetWindowFlags(m_impl->window) & SDL_WINDOW_HIDDEN); }
void ClientHost::minimize() { if (hasWindow() && !SDL_MinimizeWindow(m_impl->window)) report("minimize"); }
void ClientHost::setTitle(const std::string& title) { if (hasWindow() && !SDL_SetWindowTitle(m_impl->window, title.c_str())) report("set title"); }
void ClientHost::resizeClient(int width, int height)
{
    if (!hasWindow() || width <= 0 || height <= 0) return;
    if (!isFullscreen() && !SDL_SetWindowSize(m_impl->window, width, height)) { report("resize"); return; }
    m_impl->width = width;
    m_impl->height = height;
    m_impl->dispatch([=](IHostEventListener& listener) { listener.onResize(width, height, false); });
}
void ClientHost::setFullscreen(bool fullscreen)
{
    if (!hasWindow()) return;
    if (!SDL_SetWindowFullscreen(m_impl->window, fullscreen)) { report("fullscreen"); return; }
    if (!fullscreen && !SDL_SetWindowSize(m_impl->window, m_impl->width, m_impl->height)) report("restore window size");
}
bool ClientHost::isFullscreen() const { return hasWindow() && (SDL_GetWindowFlags(m_impl->window) & SDL_WINDOW_FULLSCREEN); }
bool ClientHost::hasFocus() const { return m_impl->focused; }
bool ClientHost::isAppActive() const { return m_impl->focused; }
unsigned ClientHost::deactivationCount() const { return m_impl->deactivations; }
bool ClientHost::pumpEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) m_impl->event(event);
    if (m_impl->scripted) inputscript::inputScript().tick(*m_impl);
    m_impl->updateCursor();
    return m_impl->quit;
}
bool ClientHost::waitEvents(double seconds)
{
    SDL_Event event;
    if (SDL_WaitEventTimeout(&event, static_cast<int>(std::max(0.0, std::min(seconds * 1000.0, 2147483647.0)))))
        m_impl->event(event);
    return pumpEvents();
}
void ClientHost::requestQuit()
{
    if (m_impl->quit) return;
    m_impl->quit = true;
    m_impl->dispatch([](IHostEventListener& listener) { listener.onCloseRequested(); });
}
void ClientHost::postQuitMessage() { m_impl->quit = true; }
bool ClientHost::quitRequested() const { return m_impl->quit; }
void ClientHost::addListener(IHostEventListener* listener)
{
    if (listener && std::find(m_impl->listeners.begin(), m_impl->listeners.end(), listener) == m_impl->listeners.end())
        m_impl->listeners.push_back(listener);
}
void ClientHost::removeListener(IHostEventListener* listener)
{
    auto& listeners = m_impl->listeners;
    listeners.erase(std::remove(listeners.begin(), listeners.end(), listener), listeners.end());
}
int ClientHost::showCursor(bool show) { m_impl->cursorCount += show ? 1 : -1; m_impl->updateCursor(); return m_impl->cursorCount; }
void ClientHost::setCursorHiddenInView(bool hidden) { m_impl->hidden = hidden; m_impl->updateCursor(); }
void ClientHost::setCursorShape(HostCursorShape shape) { setCursor(cursorHandle(shape)); }
void* ClientHost::cursorHandle(HostCursorShape shape)
{
    ClientHost& host = instance();
    if (!host.initApplication()) return nullptr;
    Cursor*& cursor = shape == HostCursorBusy ? host.m_impl->busy : host.m_impl->arrow;
    if (!cursor) {
        std::unique_ptr<Cursor> value(new Cursor);
        SDL_Cursor* frame = SDL_CreateSystemCursor(shape == HostCursorBusy ? SDL_SYSTEM_CURSOR_WAIT : SDL_SYSTEM_CURSOR_DEFAULT);
        if (!frame) { report("create cursor"); return nullptr; }
        value->frames.push_back(frame);
        cursor = value.get();
        host.m_impl->cursors.push_back(std::move(value));
    }
    return cursor;
}
void* ClientHost::loadCursorFromFile(const char* path)
{
    if (!path || !instance().initApplication()) return nullptr;
    // FileManager returns the requested spelling after its case-insensitive existence check.
    // Resolve that spelling again at this direct OS read (Linux client policy, issue #36).
    std::string physicalPath(path);
    linuxclient::resolveClientPath(physicalPath);
    std::ifstream input(physicalPath, std::ios::binary);
    if (!input) return nullptr;
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CursorAnimation animation;
    if (!decodeCursorFile(bytes.data(), bytes.size(), animation)) return cursorHandle(HostCursorArrow);
    std::unique_ptr<Cursor> cursor(new Cursor);
    for (const CursorFrame& frame : animation.frames) {
        if (frame.rgba.empty()) {
            std::fprintf(stderr, "[SDL host] PNG cursor unsupported, using arrow: %s\n", path);
            return cursorHandle(HostCursorArrow);
        }
        SDL_Surface* surface = SDL_CreateSurfaceFrom(frame.width, frame.height, SDL_PIXELFORMAT_RGBA32,
                                                     const_cast<std::uint8_t*>(frame.rgba.data()), frame.width * 4);
        SDL_Cursor* value = surface ? SDL_CreateColorCursor(surface, frame.hotX, frame.hotY) : nullptr;
        SDL_DestroySurface(surface);
        if (!value) { report("load cursor"); return cursorHandle(HostCursorArrow); }
        cursor->frames.push_back(value);
    }
    cursor->steps = animation.steps;
    cursor->jiffies = animation.jiffies;
    Cursor* token = cursor.get();
    instance().m_impl->cursors.push_back(std::move(cursor));
    return token;
}
void ClientHost::setCursor(void* handle)
{
    m_impl->hidden = handle == nullptr;
    if (m_impl->cursor != handle) {
        m_impl->cursor = static_cast<Cursor*>(handle);
        m_impl->cursorStart = SDL_GetTicks();
    }
    m_impl->updateCursor();
}
void ClientHost::setRelativeMouseMode(bool enabled)
{
    m_impl->relative = enabled;
    if (hasWindow() && !m_impl->scripted && !SDL_SetWindowRelativeMouseMode(m_impl->window, enabled && hasFocus())) report("relative mouse");
    m_impl->updateCursor();
}
bool ClientHost::relativeMouseMode() const { return m_impl->captured(); }
bool ClientHost::rawMouseAvailable() const { return false; } // SDL relative counts arrive through onMouseMove; no second source.
unsigned ClientHost::modifierFlags() const { return modifiers(m_impl->scripted ? m_impl->scriptModifiers : SDL_GetModState()); }
unsigned short ClientHost::consumeAsyncEnterState()
{
    const unsigned short result = (m_impl->enter[0] || m_impl->enter[1] ? 0x8000 : 0) | (m_impl->enterPressed ? 1 : 0);
    m_impl->enterPressed = false;
    return result;
}
bool ClientHost::controlShortcutDown() const { return (modifierFlags() & HostModControl) != 0; }
const char* ClientHost::keyDisplayName(unsigned char dik) const { return dikDisplayName(dik); }
std::u16string ClientHost::keyCharacter(unsigned char dik, unsigned mod) const
{
    for (const PhysicalKey& key : keys) {
        if (key.dik != dik) continue;
        SDL_Keymod translated = SDL_KMOD_NONE;
        if (mod & HostModShift) translated |= SDL_KMOD_SHIFT;
        if (mod & HostModCapsLock) translated |= SDL_KMOD_CAPS;
        const SDL_Keycode code = SDL_GetKeyFromScancode(key.scan, translated, false);
        if (code >= 32 && code <= 0xFFFF && code != 127) return std::u16string(1, static_cast<char16_t>(code));
        break;
    }
    return {};
}
bool ClientHost::setClipboardText(const std::string& text)
{
    std::u16string wide;
    for (unsigned char c : text) wide += c >= 0x80 && c < 0xA0 ? cp1252High[c - 0x80] : c;
    const std::string encoded = std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>().to_bytes(wide);
    return SDL_SetClipboardText(encoded.c_str());
}
bool ClientHost::getClipboardText(std::string& text) const
{
    if (!SDL_HasClipboardText()) return false;
    char* value = SDL_GetClipboardText();
    if (!value) return false;
    const std::u16string wide = utf16(value);
    SDL_free(value);
    text.clear();
    for (char16_t c : wide) {
        if (c < 0x80 || (c >= 0xA0 && c <= 0xFF)) text += static_cast<char>(c);
        else {
            const auto at = std::find(std::begin(cp1252High), std::end(cp1252High), c);
            text += at == std::end(cp1252High) ? '?' : static_cast<char>(0x80 + at - std::begin(cp1252High));
        }
    }
    return true;
}
std::vector<HostDisplayModeInfo> ClientHost::displayModes() const
{
    std::vector<HostDisplayModeInfo> result;
    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(SDL_GetPrimaryDisplay(), &count);
    for (int i = 0; i < count; ++i) {
        HostDisplayModeInfo mode{modes[i]->w, modes[i]->h, static_cast<int>(std::lround(modes[i]->refresh_rate)), 32};
        if (std::find_if(result.begin(), result.end(), [&](const HostDisplayModeInfo& m) {
            return m.width == mode.width && m.height == mode.height && m.refreshHz == mode.refreshHz;
        }) == result.end()) result.push_back(mode);
    }
    SDL_free(modes);
    return result;
}
HostDisplayModeInfo ClientHost::desktopMode() const
{
    const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
    if (!mode) { report("desktop mode"); return {}; }
    return {mode->w, mode->h, static_cast<int>(std::lround(mode->refresh_rate)), 32};
}
void ClientHost::preventDisplaySleep(bool prevent)
{
    if (m_impl->initialized && !(prevent ? SDL_DisableScreenSaver() : SDL_EnableScreenSaver())) report("screensaver");
}
bool ClientHost::openURL(const std::string& url) { return SDL_OpenURL(url.c_str()); }
unsigned ClientHost::tickCountMs() { return static_cast<unsigned>(SDL_GetTicks()); }

int gamepadCount() { updatePads(); return static_cast<int>(pads.size()); }
const void* gamepadIdentity(int index)
{
    updatePads();
    return index >= 0 && static_cast<std::size_t>(index) < pads.size() ? pads[index].handle : nullptr;
}
bool readGamepad(int index, GamepadState& state)
{
    state = {};
    state.dpad = -1;
    SDL_Gamepad* pad = static_cast<SDL_Gamepad*>(const_cast<void*>(gamepadIdentity(index)));
    if (!pad) return false;
    state.connected = true;
    auto axis = [&](SDL_GamepadAxis value) { const int n = SDL_GetGamepadAxis(pad, value); return n / (n < 0 ? 32768.0f : 32767.0f); };
    state.leftX = axis(SDL_GAMEPAD_AXIS_LEFTX); state.leftY = -axis(SDL_GAMEPAD_AXIS_LEFTY);
    state.rightX = axis(SDL_GAMEPAD_AXIS_RIGHTX); state.rightY = -axis(SDL_GAMEPAD_AXIS_RIGHTY);
    state.leftTrigger = std::max(0.0f, axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER));
    state.rightTrigger = std::max(0.0f, axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER));
    const SDL_GamepadButton buttons[] = {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST,
        SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
        SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_BACK, SDL_GAMEPAD_BUTTON_START,
        SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK};
    for (int i = 0; i < GamepadButtonCount; ++i) if (SDL_GetGamepadButton(pad, buttons[i])) state.buttons |= 1u << i;
    const int x = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) - SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    const int y = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN) - SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    if (x || y) state.dpad = x == 0 ? (y < 0 ? 0 : 18000) : x > 0 ? (y < 0 ? 4500 : y > 0 ? 13500 : 9000) : (y < 0 ? 31500 : y > 0 ? 22500 : 27000);
    return true;
}
} } }
#endif
