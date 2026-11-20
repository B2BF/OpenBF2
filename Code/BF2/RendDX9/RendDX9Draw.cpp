// Renderer scene begin/end, presentation and per-frame statistics.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../Core/StringUtil.h"         // formatString (sub_10074AB0)
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../../RendDX9Abi/PluginEntry.h"    // dice::hfe::fileManager (dword_10244AC4)
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "RendDX9Base.h"
#include "../RendMisc/IRenderer.h"
#include "../GeomMisc/Debug.h"
#if defined(BF2_LINUX_CLIENT)
#include "../Platform/Linux/FrameCapture.h"
#endif
#include "../Core/LoadStat.h"           // loadStat - rendererPresent's level-load present policy
#if defined(_WIN32)
#include <dxerr9.h>
#pragma comment(lib, "dxerr9.lib")
#endif

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace dice {
namespace hfe {
namespace geom {
extern IBase* g_DebugTextWriterManager;                 // dword_10247C58, ModuleGeomMisc.cpp
void debugTextWriterManagerDraw(IBase* manager);       // DebugTextWriter.cpp
}
namespace dx9rend {

extern void* g_rendererBase;    // dword_1023B970 - RendDX9Init.cpp
extern void* g_stateManager;    // dword_1023B984 - StateManager.cpp
void flushRenderTargetsSeam();  // StateManager.cpp - sub_10026240, run before a Clear

namespace {

inline RendDX9Base* base() { return static_cast<RendDX9Base*>(g_rendererBase); }
inline IDirect3DDevice9* device()
{
    return base()->device;                                       // +0x0C
}

// The base's byte at the given retail offset inside its +0x108 statistics run.
inline unsigned char* statsByte(RendDX9Base* b, unsigned int retailOffset)
{
    return b->renderStats + (retailOffset - 0x108u);
}


// Accumulates a 41-word frame-stat row into its peak/total row.
// PE 0x1000D1C0.
void statsAccumulate(unsigned int* dst, const unsigned int* src)
{
    for (int i = 0; i < 41; ++i)
        dst[i] += src[i];
}

const char* presentErrorDescription(HRESULT result)
{
#if defined(_WIN32)
    return ::DXGetErrorDescription9A(result);
#else
    // [CHOSEN, not recovered] DXErr9 is a Windows SDK library; native diagnostics retain HRESULT.
    static char description[32];
    std::snprintf(description, sizeof(description), "HRESULT 0x%08X",
                  static_cast<unsigned int>(result));
    return description;
#endif
}

const char kDrawFile[] =
    "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\RendDX9Draw.cpp";

// sub_1000D390 - roll the stats a frame forward: bump the frame counter, fold the current row into
// the totals, publish it as last-frame, and clear the working rows.
void statsBeginFrame()
{
    unsigned char* stats = base()->renderStats;                  // +0x108
    ++*reinterpret_cast<unsigned int*>(stats + 24);
    unsigned int* current = reinterpret_cast<unsigned int*>(stats + 95 * 4);
    statsAccumulate(reinterpret_cast<unsigned int*>(stats + 13 * 4), current);
    ::memcpy(stats + 54 * 4, current, 0xA4);
    ::memset(current, 0, 0xA4);
    ::memset(stats + 136 * 4, 0, 0x64);
    ::memset(stats + 4, 0, 16);
}

} // anonymous namespace

// Published for LightmapGeneration.cpp, whose trace rolls the statistics once per batch on the
// block IRenderer::getRenderStats answers.
void rendererStatsBeginFrame()
{
    statsBeginFrame();
}

// PE 0x1000D900 (RenderStats constructor, past its vptr store at +0x00, which the native raw block does
// not model) and PE 0x1000D300 (reset). Clears the category word (+0x14), turns the counters on
// (+0x284) and zeroes the counter ranges: +0x18..+0x30 (7 words), three 41-word rows at +0x34, +0xD8
// and +0x17C, 25 words at +0x220 and the four words at +0x04..+0x10. `stats` is the base's +0x108 run.
void rendererStatsConstruct(unsigned char* stats)
{
    *reinterpret_cast<unsigned int*>(stats + 0x14) = 0;
    stats[0x284] = 1;
    ::memset(stats + 0x18, 0, 7 * 4);
    ::memset(stats + 0x34, 0, 41 * 4);
    ::memset(stats + 0xD8, 0, 41 * 4);
    ::memset(stats + 0x17C, 0, 41 * 4);
    ::memset(stats + 0x220, 0, 25 * 4);
    ::memset(stats + 0x04, 0, 4 * 4);
}

// Binds a render target through the StateManager (+0x60) and answers true. PE 0x10005770.
bool rendererSetRenderTarget(unsigned int index, IDirect3DSurface9* surface)
{
    stateManagerSetRenderTarget(index, surface);
    return true;
}

// Sets the statistics category word (+0x14) that the draw counters index. [INFERRED] name.
// Unreachable in 1.50: no code or data references. PE 0x1000D5E0 (RenderStats member, stats = base
// +0x108).
void rendererStatsSetCategory(unsigned char* stats, unsigned int category)
{
    *reinterpret_cast<unsigned int*>(stats + 0x14) = category;
}

// RenderStats counter members that are unreachable in 1.50 (no code or data references). Each one counts
// only while the statistics byte (+0x284) is set; `stats` is the base's +0x108 run. [INFERRED] names.
// PE 0x1000D450: ++(+0x188), and ++(+0x18C) as well when the argument is zero.
void renderStatsCountA(unsigned char* stats, int flag)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x188);
        if (!flag)
            ++*reinterpret_cast<int*>(stats + 0x18C);
    }
}
// PE 0x1000D480: ++(+0x190), and ++(+0x194) as well when the argument is zero.
void renderStatsCountB(unsigned char* stats, int flag)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x190);
        if (!flag)
            ++*reinterpret_cast<int*>(stats + 0x194);
    }
}
// PE 0x1000D550: ++(+0x1AC), (+0x1B0) += amount.
void renderStatsCountC(unsigned char* stats, int amount)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x1AC);
        *reinterpret_cast<int*>(stats + 0x1B0) += amount;
    }
}
// PE 0x1000D590: ++(+0x1B8), (+0x1BC) += amount.
void renderStatsCountD(unsigned char* stats, int amount)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x1B8);
        *reinterpret_cast<int*>(stats + 0x1BC) += amount;
    }
}
// PE 0x1000D5F0: reads the counter word at +4 + 4 * index.
unsigned int renderStatsWord(const unsigned char* stats, unsigned int index)
{
    return *reinterpret_cast<const unsigned int*>(stats + 4 + 4 * index);
}
// PE 0x1000D6E0: ++(+0x1E8), and ++(+0x1EC) as well when the argument is zero.
void renderStatsCountE(unsigned char* stats, int flag)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x1E8);
        if (!flag)
            ++*reinterpret_cast<int*>(stats + 0x1EC);
    }
}
// PE 0x1000D710: ++(+0x1F0), and ++(+0x1F4) as well when the argument is zero.
void renderStatsCountF(unsigned char* stats, int flag)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x1F0);
        if (!flag)
            ++*reinterpret_cast<int*>(stats + 0x1F4);
    }
}
// PE 0x1000D7E0: ++(+0x20C), (+0x210) += amount.
void renderStatsCountG(unsigned char* stats, int amount)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x20C);
        *reinterpret_cast<int*>(stats + 0x210) += amount;
    }
}
// PE 0x1000D820: ++(+0x218), (+0x21C) += amount.
void renderStatsCountH(unsigned char* stats, int amount)
{
    if (stats[0x284])
    {
        ++*reinterpret_cast<int*>(stats + 0x218);
        *reinterpret_cast<int*>(stats + 0x21C) += amount;
    }
}

// The Renderer wrapper over it: `add ecx, 0x108` then a tail jump. [INFERRED] name.
// Unreachable in 1.50: no code or data references. PE 0x10005910.
void rendererSetStatsCategory(unsigned int category)
{
    rendererStatsSetCategory(base()->renderStats, category);
}

// sub_1000D360 - swap the counters-enabled byte, returning the old value. renderFrame brackets the
// AI overlay event with it so those draws stay out of the frame's counters.
unsigned char statsSwapEnabled(unsigned char value)
{
    unsigned char* flag = base()->renderStats + 644;             // +0x108 + 644
    const unsigned char old = *flag;
    *flag = value;
    return old;
}


// Presents the active swap chain. Mode 3 only marks it presented; mode 2 uses default flags.
// Device loss and driver errors attempt restoration but still fail this frame; only
// device loss clears in-scene state. Retry policy depends on the HRESULT and spin flag.
// PE 0x10005AE0.
bool rendererPresent(unsigned int mode)
{
    RendDX9Base* b = base();
    if (b->framePresented)                                       // +0x101
        return false;
    if (mode == 3)
    {
        b->framePresented = 1;
        return true;
    }
    if (mode == 2)
    {
        mode = b->presentAsync;                                  // +0x3AC
        // [CHOSEN, not recovered] https://github.com/kr4uzi/OpenBF2/issues/41: while a level loads
        // (LoadStat's callback is registered, GameLogic::beginLoadStats .. unregisterLoadCallback),
        // present as with renderer.presentAsync: D3DPRESENT_DONOTWAIT. Retail repaints the loading
        // screen on every effect load (shaderLoadingEvent, outside LoadStat's 1/30 s throttle),
        // ~1,700 presents per level; with vsync each blocking Present waited for a free back buffer,
        // so the load ran at the display's refresh rate. A busy device answers WASSTILLDRAWING; the
        // retail handling below keeps the frame and presents it at the next beginScene, which skips
        // that event's draw, so every drawn loading frame (the first and the last included) still
        // reaches the screen. Per backend: D3D9 (Windows) answers busy while its present queue is
        // full; the Metal device (macOS) acquires the drawable off the main thread and is busy until
        // one is free and the previous frame was shown; DXVK Native (Linux) ignores the flag and
        // presents as before. Presents outside a level load are unchanged.
        if (loadStat && loadStat->m_loadCallback)
            mode |= 1;                                           // D3DPRESENT_DONOTWAIT
    }

    IDirect3DSwapChain9* chain = b->swapChains[b->currentSwapChain];   // +0xE4[+0xF0]

#if defined(BF2_LINUX_CLIENT)
    bf2LinuxCaptureBeforePresent(chain);
#endif
    HRESULT hr = chain->Present(0, 0, 0, 0, mode);
    while (hr)
    {
        if (hr == static_cast<HRESULT>(0x88760868))            // D3DERR_DEVICELOST
        {
            b->inScene = 0;                                     // 0x10005C55, then falls through
            handleLostDevice(b);                                // result discarded
            return false;
        }
        if (hr == static_cast<HRESULT>(0x88760827))            // D3DERR_DRIVERINTERNALERROR
        {
            handleLostDevice(b);
            return false;
        }
        if (hr == static_cast<HRESULT>(0x8876021C))            // D3DERR_WASSTILLDRAWING
        {
            if (!b->presentSpinIfBusy)                          // +0x3AD
                return false;
        }
        else if (hr != static_cast<HRESULT>(0x8876086C)        // D3DERR_INVALIDCALL
              && hr != static_cast<HRESULT>(0x8876017C)        // D3DERR_OUTOFVIDEOMEMORY
              && hr != static_cast<HRESULT>(0x8007000E))       // E_OUTOFMEMORY
        {
            ::renddx9abi::Debug diagnostic(DebugTypeError, kDrawFile, 200, "");
            const char* const description = presentErrorDescription(hr);
            diagnostic << "Present returned unknown error.  " << description;
        }
        chain = b->swapChains[b->currentSwapChain];
        hr = chain->Present(0, 0, 0, 0, mode);
        if (!hr)
            break;
    }

    b->framePresented = 1;
    return true;
}


// Begins a frame, rolls statistics, and optionally clears color/depth/stencil.
// A null clearRgb selects base defaults, currently black from zero-initialized storage.
// PE 0x100059A0. The render-target flush is the recovered StateManager body at 0x10026240.
bool rendererBeginScene(const unsigned int* clearRgb, bool doClear)
{
    RendDX9Base* b = base();
    if (!b->framePresented)
    {
        if (!static_cast<IRenderer*>(g_rendererBase)->present(2))
            return false;
        b->framePresented = 1;
    }

    statsBeginFrame();

    if (device()->BeginScene() < 0)
    {
        ::renddx9abi::Debug(DebugTypeError, kDrawFile, 86, "") << "Failed to begin scene";
        return false;
    }
    ++b->frameOrdinal;                                           // +0x104

    if (doClear)
    {
        unsigned int colour;
        if (clearRgb)
            colour = (clearRgb[2] & 0xFF)
                   | ((clearRgb[1] & 0xFF) << 8)
                   | ((clearRgb[0] & 0xFF) << 16);
        else
            colour = (*reinterpret_cast<unsigned int*>(statsByte(b, 920)) & 0xFF)
                   | (static_cast<unsigned int>(*statsByte(b, 916)) << 8)
                   | (static_cast<unsigned int>(*statsByte(b, 912)) << 16);
        flushRenderTargetsSeam();
        device()->Clear(0, 0, 7, colour, 1.0f, 0);
    }

    b->inScene = 1;                                              // +0x100
    return true;
}


// The screenshot request and its two strings. [INFERRED] placement: the five functions sit
// between the frame bracket's helpers and endScene (0x10005640, RendDX9Draw.cpp per its
// __FILE__ string) in the shipped image.

// PE 0x10004FB0 - the screenshot directory, `renderer.screenDumpPath`'s read arm.
const std::string& rendererGetScreenDumpPath(void* renderer)
{
    return static_cast<RendDX9Base*>(renderer)->screenDumpPath;            // +0x3B0
}

// PE 0x10004FA0 - `renderer.screenDumpPath`'s write arm.
void rendererSetScreenDumpPath(void* renderer, const std::string& path)
{
    static_cast<RendDX9Base*>(renderer)->screenDumpPath = path;
}

// PE 0x10004F70 - `renderer.dumpScreen`: records the file name (empty for an automatically
// numbered capture) and requests the capture the next endScene performs.
void rendererRequestScreenDump(void* renderer, const std::string& name)
{
    RendDX9Base* b = static_cast<RendDX9Base*>(renderer);
    b->screenDumpName = name;                                               // +0x3CC
    b->screenDumpRequest = 1;                                               // +0x3E8
}

namespace {


// PE 0x10004E50 - the first "<directory>/screenNNN" for which neither the .png nor the .jpg
// exists, without extension and with every separator turned into a backslash.
std::string screenDumpNextName(const std::string& directory)
{
    int index = -1;
    bool taken;
    do
    {
        ++index;
        std::string png;
        png = formatString("%s/screen%03i.png", directory.c_str(), index);
        taken = ::dice::hfe::fileManager->fileExists(png);                  // +0x1C
        if (!taken)
        {
            std::string jpg;
            jpg = formatString("%s/screen%03i.jpg", directory.c_str(), index);
            taken = ::dice::hfe::fileManager->fileExists(jpg);
        }
    } while (taken);
    std::string name;
    name = formatString("%s/screen%03i", directory.c_str(), index);
    pathReplaceSeparators(name, '\\');                                      // sub_100751C0
    return name;
}

// PE 0x10005200 - the screenshot capture. Copies swap chain 0's back buffer into an A8R8G8B8
// scratch surface of the back-buffer size and writes it twice, as <name>.png and <name>.jpg under
// the home directory: <screenDumpPath>/<requested name>, or the next free screenNNN. The
// file-manager open of the .png creates the directory. As shipped, no call's result is checked.
void rendererCaptureScreen(RendDX9Base* b)
{
#if defined(BF2_LINUX_CLIENT)
    // [CHOSEN, not recovered] Issue #36: display screenshots are opaque RGB;
    // backbuffer alpha is render-pass data, not window transparency.
    const D3DFORMAT captureFormat = D3DFMT_X8R8G8B8;
#else
    const D3DFORMAT captureFormat = D3DFMT_A8R8G8B8;
#endif
    IDirect3DSurface9* capture = 0;
    b->device->CreateOffscreenPlainSurface(b->pp.BackBufferWidth, b->pp.BackBufferHeight,
                                           captureFormat, D3DPOOL_SCRATCH, &capture, 0);   // +0x90
    IDirect3DSurface9* backBuffer = 0;
    b->swapChains[0]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);               // +0x14
    D3DXLoadSurfaceFromSurface(capture, 0, 0, backBuffer, 0, 0, D3DX_FILTER_NONE, 0);

    std::string pngName;
    std::string jpgName;
    if (b->screenDumpName.size() != 0)                                       // +0x3E0
    {
        pngName = b->screenDumpPath + "/" + b->screenDumpName;
        jpgName = pngName + ".jpg";
    }
    else
    {
        pngName = screenDumpNextName(b->screenDumpPath);
        jpgName = pngName + ".jpg";
    }
    pngName += ".png";
    if (::renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
            pngName, static_cast<::renddx9abi::HostFileMode>(3),
            static_cast<::renddx9abi::HostFileAccess>(2), 0))                // +0x18
        stream->release();

    static const std::string home("@HOME@");                                // 0x1023B654
    if (pngName.find(home.c_str(), 0, home.size()) == 0)
        pngName.erase(0, home.size());
    if (jpgName.find(home.c_str(), 0, home.size()) == 0)
        jpgName.erase(0, home.size());

    const std::string pngPath = ::dice::hfe::fileManager->getHomeSubstitution() + pngName;  // +0x48
    D3DXSaveSurfaceToFileA(pngPath.c_str(), D3DXIFF_PNG, capture, 0, 0);
    const std::string jpgPath = ::dice::hfe::fileManager->getHomeSubstitution() + jpgName;
    D3DXSaveSurfaceToFileA(jpgPath.c_str(), D3DXIFF_JPG, capture, 0, 0);
    backBuffer->Release();
    capture->Release();
}

} // anonymous namespace


// Ends the scene, clears frame flags and handles pending screenshot capture.
// PE 0x10005640.
bool rendererEndScene()
{
    RendDX9Base* b = base();
    if (!b->inScene)
        return false;

    // PE 0x10005659: dword_10247C58, the GeomMisc DebugTextWriterManager, draws its writers'
    // queued text before EndScene (manager slot +0x18).
    if (geom::g_DebugTextWriterManager)
        geom::debugTextWriterManagerDraw(geom::g_DebugTextWriterManager);

    if (device()->EndScene() < 0)
    {
        ::renddx9abi::Debug diagnostic(DebugTypeError, kDrawFile, 130, "");
        const bool inScene = b->inScene != 0;
        diagnostic << "Failed to end scene: FrameInProgress " << inScene;
        return false;
    }

    // PE 0x100056F3..0x10005711
    const bool capture = b->screenDumpRequest != 0;                         // +0x3E8
    b->framePresented = 0;
    b->inScene = 0;
    if (capture)
    {
        rendererCaptureScreen(b);
        b->screenDumpRequest = 0;
    }
    return true;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
