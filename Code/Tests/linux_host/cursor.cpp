// [TEMP] [CHOSEN, not recovered] Linux cursor file and visibility contract, issue #36.
// SDL cursor calls are observed below; the dummy driver never changes a real cursor/window.
// [RETIRE-WHEN: linux-host-contract-suite-replaced] Keep until equivalent native cursor coverage exists.
#include "../../BF2/Platform/Host/ClientHost.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

namespace {
struct Token { bool custom; unsigned red; };
SDL_Cursor* selected = nullptr;
bool visible = true;
unsigned customCount = 0;
void check(bool value, const char* message)
{
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
using Bytes = std::vector<unsigned char>;
void put16(Bytes& out, unsigned value) { out.push_back(value); out.push_back(value >> 8); }
void put32(Bytes& out, unsigned value) { put16(out, value); put16(out, value >> 16); }
Bytes cursor(unsigned red)
{
    // One 2x1 BGRA DIB cursor, with a zero AND mask and hotspot (1,0).
    Bytes image;
    for (unsigned value : {40u, 2u, 2u}) put32(image, value);
    put16(image, 1); put16(image, 32);
    for (int i = 0; i < 6; ++i) put32(image, 0);
    for (int i = 0; i < 2; ++i) { image.push_back(0); image.push_back(0); image.push_back(red); image.push_back(255); }
    put32(image, 0);
    Bytes file;
    put16(file, 0); put16(file, 2); put16(file, 1);
    file.insert(file.end(), {2, 1, 0, 0});
    put16(file, 1); put16(file, 0); put32(file, image.size()); put32(file, 22);
    file.insert(file.end(), image.begin(), image.end());
    return file;
}
void write(const std::string& path, unsigned red)
{
    const Bytes bytes = cursor(red);
    std::ofstream stream(path, std::ios::binary);
    stream.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    check(bool(stream), "write synthetic cursor");
}
}

extern "C" SDL_Cursor* __wrap_SDL_CreateColorCursor(SDL_Surface* surface, int x, int y)
{
    check(surface && surface->w == 2 && surface->h == 1 && x == 1 && y == 0, "decoded cursor dimensions and hotspot");
    const auto* pixels = static_cast<const unsigned char*>(surface->pixels);
    check(pixels[1] == 0 && pixels[2] == 0 && pixels[3] == 255, "decoded opaque RGBA cursor");
    ++customCount;
    return reinterpret_cast<SDL_Cursor*>(new Token{true, pixels[0]});
}
extern "C" SDL_Cursor* __wrap_SDL_CreateSystemCursor(SDL_SystemCursor)
{ return reinterpret_cast<SDL_Cursor*>(new Token{false, 0}); }
extern "C" void __wrap_SDL_DestroyCursor(SDL_Cursor* cursor)
{ delete reinterpret_cast<Token*>(cursor); }
extern "C" bool __wrap_SDL_SetCursor(SDL_Cursor* cursor) { selected = cursor; return true; }
extern "C" bool __wrap_SDL_ShowCursor() { visible = true; return true; }
extern "C" bool __wrap_SDL_HideCursor() { visible = false; return true; }

int main()
{
    using dice::hfe::host::ClientHost;
    char directory[] = "linux-cursor-XXXXXX";
    check(mkdtemp(directory) != nullptr, "create isolated cursor directory");
    const std::string root = directory;
    check(mkdir((root + "/FlashMenu").c_str(), 0700) == 0, "create mixed-case parent");
    check(mkdir((root + "/FlashMenu/Cursor").c_str(), 0700) == 0, "create mixed-case cursor directory");
    const std::string arrowPath = root + "/FlashMenu/Cursor/cursor_arrow.ani";
    const std::string busyPath = root + "/FlashMenu/Cursor/cursor_busy.ani";
    write(arrowPath, 255);
    write(busyPath, 128);
    ClientHost& host = ClientHost::instance();
    check(host.initApplication(), "initialize dummy SDL host");
    SDL_Event focus{}; focus.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    check(SDL_PushEvent(&focus), "queue synthetic focus");
    host.pumpEvents();
    const std::string requestedArrow = root + "/flashmenu/cursor/cursor_arrow.ani";
    const std::string requestedBusy = root + "\\flashmenu\\cursor\\cursor_busy.ani";
    void* arrow = host.loadCursorFromFile(requestedArrow.c_str());
    void* busy = host.loadCursorFromFile(requestedBusy.c_str());
    check(arrow && busy && arrow != busy && customCount == 2, "case-folded and Windows-separated cursor paths load custom images");
    for (void* token : {arrow, busy}) {
        for (int frame = 0; frame < 3; ++frame) {
            host.setCursor(token); // MainMenuLogic repeats this before every display.
            host.pumpEvents();     // No mouse motion: the cursor must stay visible.
            check(visible && selected && reinterpret_cast<Token*>(selected)->custom, "idle menu retains visible custom cursor");
        }
        check(reinterpret_cast<Token*>(selected)->red == (token == arrow ? 255u : 128u), "normal and busy select their own image");
    }
    check(host.loadCursorFromFile((root + "/missing.ani").c_str()) == nullptr, "missing cursor remains a failed load");
    host.setCursor(nullptr);
    host.pumpEvents();
    check(!visible, "explicit null cursor still hides");
    host.setCursor(arrow);
    host.pumpEvents();
    check(visible, "restoring normal cursor shows it without motion");
    unlink(arrowPath.c_str()); unlink(busyPath.c_str());
    rmdir((root + "/FlashMenu/Cursor").c_str()); rmdir((root + "/FlashMenu").c_str()); rmdir(root.c_str());
    std::puts("PASS: Linux cursor path spelling, custom images, idle visibility and busy selection");
}
