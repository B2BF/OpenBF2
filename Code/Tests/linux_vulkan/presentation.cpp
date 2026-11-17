// [TEMP] [CHOSEN, not recovered] Window/presentation boundary check for issue #36.
// This is a graphics test, not the BF2 client or a gameplay acceptance test.
#include "../../BF2/Platform/Gfx/D3D9Subset.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <sys/resource.h>

static bool screenshot(IDirect3DDevice9* device, unsigned width, unsigned height, const std::string& path)
{
    IDirect3DSurface9 *back = nullptr, *copy = nullptr;
    HRESULT hr = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back);
    if (SUCCEEDED(hr)) hr = device->CreateOffscreenPlainSurface(width, height, D3DFMT_X8R8G8B8,
        D3DPOOL_SYSTEMMEM, &copy, nullptr);
    if (SUCCEEDED(hr)) hr = device->GetRenderTargetData(back, copy);
    bool saved = false;
    D3DLOCKED_RECT locked{};
    if (SUCCEEDED(hr)) hr = copy->LockRect(&locked, nullptr, D3DLOCK_READONLY);
    if (SUCCEEDED(hr)) {
        SDL_Surface* surface = SDL_CreateSurfaceFrom(int(width), int(height), SDL_PIXELFORMAT_XRGB8888,
            locked.pBits, locked.Pitch);
        saved = surface && SDL_SaveBMP(surface, path.c_str());
        if (surface) SDL_DestroySurface(surface);
        if (FAILED(copy->UnlockRect())) saved = false;
    }
    if (copy) copy->Release();
    if (back) back->Release();
    if (!saved) std::fprintf(stderr, "screenshot failed: 0x%08x %s\n", unsigned(hr), SDL_GetError());
    return saved;
}

int main(int argc, char** argv)
{
    if (argc < 2 || argc > 3) {
        std::fprintf(stderr, "usage: %s EXISTING_RUN_DIRECTORY [FRAME_LIMIT]\n", argv[0]);
        return 2;
    }
    unsigned long frameLimit = 0;
    if (argc == 3) {
        char* end = nullptr;
        frameLimit = std::strtoul(argv[2], &end, 10);
        if (!*argv[2] || *end || argv[2][0] == '-' || !frameLimit) return 2;
    }
    const std::string directory(argv[1]);
    FILE* metrics = std::fopen((directory + "/frames.csv").c_str(), "wx");
    if (!metrics) { std::perror("frames.csv"); return 2; }
    std::fprintf(metrics, "frame,start_ns,interval_ms,draw_ms,present_ms,capture_ms,captured,width,height,max_rss_kib\n");
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "DXVK_WSI_DRIVER", "SDL3", true);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        std::fclose(metrics);
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("OpenBF2 Linux presentation check", 1280, 800,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        std::fclose(metrics);
        return 1;
    }
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    IDirect3DDevice9* device = nullptr;
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = 1280;
    pp.BackBufferHeight = 800;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    // Public D3DCREATE_HARDWARE_VERTEXPROCESSING (not declared by the subset).
    constexpr DWORD hardwareVertexProcessing = 0x40;
    HRESULT hr = d3d ? d3d->CreateDevice(0, D3DDEVTYPE_HAL, window,
        hardwareVertexProcessing, &pp, &device) : E_FAIL;
    int result = 0;
    if (FAILED(hr) || !device) {
        std::fprintf(stderr, "CreateDevice: 0x%08x\n", unsigned(hr));
        result = 1;
    } else {
        bool running = true, resize = false, capture = false, save = false;
        unsigned long frame = 0;
        Uint64 previous = SDL_GetTicksNS();
        std::fprintf(stderr, "F11: fullscreen; F6: mouse capture; F12: screenshot; Escape: exit\n");
        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) running = false;
                if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) resize = true;
                if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                    SDL_SetWindowRelativeMouseMode(window, false);
                    capture = false;
                }
                if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                    if (event.key.scancode == SDL_SCANCODE_ESCAPE) running = false;
                    if (event.key.scancode == SDL_SCANCODE_F12) save = true;
                    if (event.key.scancode == SDL_SCANCODE_F11 &&
                        !SDL_SetWindowFullscreen(window, !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN))) {
                        std::fprintf(stderr, "fullscreen: %s\n", SDL_GetError());
                        result = 1; running = false;
                    }
                    if (event.key.scancode == SDL_SCANCODE_F6) {
                        if (!SDL_SetWindowRelativeMouseMode(window, !capture)) {
                            std::fprintf(stderr, "mouse capture: %s\n", SDL_GetError());
                            result = 1; running = false;
                        } else capture = !capture;
                    }
                }
                if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST)
                    std::fprintf(stderr, "event ns=%llu frame=%lu type=%u\n",
                        static_cast<unsigned long long>(SDL_GetTicksNS()), frame, unsigned(event.type));
            }
            if (!running) break;
            int width = 0, height = 0;
            if (!SDL_GetWindowSizeInPixels(window, &width, &height)) {
                std::fprintf(stderr, "window size: %s\n", SDL_GetError());
                result = 1; break;
            }
            if ((SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) || width <= 0 || height <= 0) {
                SDL_Delay(20);
                continue;
            }
            hr = device->TestCooperativeLevel();
            if (hr == D3DERR_DEVICELOST) { SDL_Delay(20); continue; }
            if (hr == D3DERR_DEVICENOTRESET) resize = true;
            else if (FAILED(hr)) { result = 1; break; }
            if (resize) {
                pp.BackBufferWidth = unsigned(width); pp.BackBufferHeight = unsigned(height);
                hr = device->Reset(&pp);
                if (FAILED(hr)) { result = 1; break; }
                resize = false;
            }
            const Uint64 start = SDL_GetTicksNS();
            hr = device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff18334fu, 1.0f, 0);
            if (FAILED(hr)) { result = 1; break; }
            D3DRECT edges[] = {{0, 0, width, 2}, {0, height - 2, width, height},
                              {0, 0, 2, height}, {width - 2, 0, width, height}};
            hr = device->Clear(4, edges, D3DCLEAR_TARGET, 0xffffffffu, 1.0f, 0);
            if (FAILED(hr)) { result = 1; break; }
            struct Vertex { float x, y, z, rhw; DWORD color; };
            const float shift = std::sin(float(double(start) / 1e9)) * width * 0.1f;
            const Vertex triangle[] = {
                {width * 0.5f + shift, height * 0.2f, 0, 1, 0xffff0000u},
                {width * 0.8f + shift, height * 0.8f, 0, 1, 0xff00ff00u},
                {width * 0.2f + shift, height * 0.8f, 0, 1, 0xff0000ffu}};
            hr = device->BeginScene();
            if (FAILED(hr)) { result = 1; break; }
            hr = device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
            if (SUCCEEDED(hr)) hr = device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, sizeof(Vertex));
            const HRESULT end = device->EndScene();
            if (FAILED(hr) || FAILED(end)) { if (FAILED(end)) hr = end; result = 1; break; }
            const Uint64 drawEnd = SDL_GetTicksNS();
            const bool captured = save;
            if (save) {
                if (!screenshot(device, unsigned(width), unsigned(height),
                                directory + "/frame-" + std::to_string(frame) + ".bmp")) {
                    result = 1; break;
                }
                save = false;
            }
            const Uint64 captureEnd = SDL_GetTicksNS();
            hr = device->Present(nullptr, nullptr, nullptr, nullptr);
            const Uint64 presentEnd = SDL_GetTicksNS();
            if (hr == D3DERR_DEVICELOST) continue;
            if (FAILED(hr)) { result = 1; break; }
            rusage usage{};
            const long rss = getrusage(RUSAGE_SELF, &usage) == 0 ? usage.ru_maxrss : -1;
            if (std::fprintf(metrics, "%lu,%llu,%.6f,%.6f,%.6f,%.6f,%d,%d,%d,%ld\n", frame,
                static_cast<unsigned long long>(start), double(start - previous) / 1e6,
                double(drawEnd - start) / 1e6, double(presentEnd - captureEnd) / 1e6,
                double(captureEnd - drawEnd) / 1e6, captured ? 1 : 0, width, height, rss) < 0) {
                result = 1; break;
            }
            previous = start;
            if (++frame == frameLimit) running = false;
        }
        if (result) std::fprintf(stderr, "presentation failed: 0x%08x\n", unsigned(hr));
    }
    if (device) device->Release();
    if (d3d) d3d->Release();
    SDL_DestroyWindow(window);
    SDL_Quit();
    if (std::fclose(metrics) != 0) result = 1;
    return result;
}
