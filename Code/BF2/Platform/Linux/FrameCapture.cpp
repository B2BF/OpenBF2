// [TEMP] [CHOSEN, not recovered] Native client capture instrumentation, issue #36.
#if defined(__linux__) && defined(BF2_LINUX_CLIENT) && defined(BF2_IMAGE_RENDDX9)
#include "FrameCapture.h"
#include "../Gfx/D3DX9Subset.h"
#include "../InputScript.h"
#include "../macOS/HitchTrace.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>

void bf2LinuxCaptureBeforePresent(IDirect3DSwapChain9* chain)
{
    const char* directory = std::getenv("BF2_CAPTURE_DIR");
    if (!directory || directory[0] != '/') return;
    static unsigned long long present = 0;
    ++present;
    char requested[256] = {};
    if (!openbf2InputScriptTakeCapture(requested, sizeof(requested))) return;
    std::string label;
    for (const unsigned char c : std::string(requested))
        label += (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '-' || c == '_' ? static_cast<char>(c) : '_';
    if (label.empty()) label = "capture";
    char prefix[64];
    std::snprintf(prefix, sizeof(prefix), "present-%08llu-", present);
    const std::string filename = prefix + label + ".png";
    const std::string path = std::string(directory) + "/" + filename;
    const auto start = std::chrono::steady_clock::now();
    IDirect3DSurface9* surface = nullptr;
    IDirect3DSurface9* opaque = nullptr;
    IDirect3DDevice9* device = nullptr;
    D3DSURFACE_DESC description = {};
    HRESULT result = chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &surface);
    if (SUCCEEDED(result))
        result = surface->GetDesc(&description);
    if (SUCCEEDED(result))
        result = surface->GetDevice(&device);
    // [CHOSEN, not recovered] Issue #36: the displayed window is opaque, while
    // its backbuffer alpha may hold render-pass data. Preserve resolved RGB in
    // display captures without changing general D3DX image-export semantics.
    if (SUCCEEDED(result))
        result = device->CreateOffscreenPlainSurface(description.Width, description.Height,
            D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &opaque, nullptr);
    if (SUCCEEDED(result))
        result = D3DXLoadSurfaceFromSurface(opaque, nullptr, nullptr, surface, nullptr, nullptr,
                                          D3DX_FILTER_NONE, 0);
    if (SUCCEEDED(result))
        result = D3DXSaveSurfaceToFileA(path.c_str(), D3DXIFF_PNG, opaque, nullptr, nullptr);
    if (opaque) opaque->Release();
    if (device) device->Release();
    if (surface) surface->Release();
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    char note[512];
    std::snprintf(note, sizeof(note), "linux-capture present=%llu label=%s success=%d ms=%.3f hr=%d",
                  present, label.c_str(), SUCCEEDED(result) ? 1 : 0, milliseconds, static_cast<int>(result));
    bf2hitch::note(note);
    std::fprintf(stderr, "[CAPTURE] %s: %s\n", note, path.c_str());
    std::fflush(stderr);
    const std::string records = std::string(directory) + "/captures.jsonl";
    if (FILE* file = std::fopen(records.c_str(), "a")) {
        std::fprintf(file, "{\"present\":%llu,\"file\":\"%s\",\"success\":%s,\"duration_ms\":%.3f,\"hresult\":%d}\n",
                     present, filename.c_str(), SUCCEEDED(result) ? "true" : "false", milliseconds, static_cast<int>(result));
        std::fclose(file);
    } else {
        std::fprintf(stderr, "[CAPTURE] failed to append capture record: %s\n", records.c_str());
        std::fflush(stderr);
    }
}
#endif
