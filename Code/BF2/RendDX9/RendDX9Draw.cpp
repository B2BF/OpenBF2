// Renderer scene begin/end, presentation and per-frame statistics.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"

#include <cstring>

namespace dice {
namespace hfe {
namespace geom {
extern IBase* g_DebugTextWriterManager;
void debugTextWriterManagerDraw(IBase* manager);
}
namespace dx9rend {

extern void* g_rendererBase;    // dword_1023B970 - RendDX9Init.cpp
extern void* g_stateManager;    // dword_1023B984 - StateManager.cpp
void flushRenderTargetsSeam();  // StateManager.cpp - sub_10026240, run before a Clear

namespace {

inline unsigned char* base() { return static_cast<unsigned char*>(g_rendererBase); }
inline IDirect3DDevice9* device()
{
    return *reinterpret_cast<IDirect3DDevice9**>(base() + 0x0C);
}


// Accumulates a 41-word frame-stat row into its peak/total row.
void statsAccumulate(unsigned int* dst, const unsigned int* src)
{
    for (int i = 0; i < 41; ++i)
        dst[i] += src[i];
}

// sub_1000D390 - roll the stats a frame forward: bump the frame counter, fold the current row into
// the totals, publish it as last-frame, and clear the working rows.
void statsBeginFrame()
{
    unsigned char* stats = base() + 0x108;
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

// sub_1000D360 - swap the counters-enabled byte, returning the old value. renderFrame brackets the
// AI overlay event with it so those draws stay out of the frame's counters.
unsigned char statsSwapEnabled(unsigned char value)
{
    unsigned char* flag = base() + 0x108 + 644;
    const unsigned char old = *flag;
    *flag = value;
    return old;
}


// Presents the active swap chain. Mode 3 only marks it presented; mode 2 uses default flags.
// Device loss and driver errors attempt restoration but still fail this frame; only
// device loss clears in-scene state. Retry policy depends on the HRESULT and spin flag.
// [CHOSEN, not recovered] Unknown errors log the HRESULT rather than a DXErr description.

bool rendererPresent(unsigned int mode)
{
    unsigned char* b = base();
    if (b[257])
        return false;
    if (mode == 3)
    {
        b[257] = 1;
        return true;
    }
    if (mode == 2)
        mode = b[940];

    IDirect3DSwapChain9** chains = *reinterpret_cast<IDirect3DSwapChain9***>(b + 228);
    IDirect3DSwapChain9* chain = chains[*reinterpret_cast<unsigned int*>(b + 240)];

    HRESULT hr = chain->Present(0, 0, 0, 0, mode);
    while (hr)
    {
        if (hr == static_cast<HRESULT>(0x88760868))            // D3DERR_DEVICELOST
        {
            b[256] = 0;                                         // 0x10005C55, then falls through
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
            if (!b[941])
                return false;
        }
        else if (hr != static_cast<HRESULT>(0x8876086C)        // D3DERR_INVALIDCALL
              && hr != static_cast<HRESULT>(0x8876017C)        // D3DERR_OUTOFVIDEOMEMORY
              && hr != static_cast<HRESULT>(0x8007000E))       // E_OUTOFMEMORY
        {
            ::renddx9abi::logf("RendDX9Draw.cpp:200 Present returned unknown error.  hr=0x%08X",
                               static_cast<unsigned int>(hr));
        }
        chains = *reinterpret_cast<IDirect3DSwapChain9***>(b + 228);
        chain  = chains[*reinterpret_cast<unsigned int*>(b + 240)];
        hr = chain->Present(0, 0, 0, 0, mode);
        if (!hr)
            break;
    }

    b[257] = 1;
    return true;
}


// Begins a frame, rolls statistics, and optionally clears color/depth/stencil.
// A null clearRgb selects base defaults, currently black from zero-initialized storage.

bool rendererBeginScene(const unsigned int* clearRgb, bool doClear)
{
    unsigned char* b = base();
    if (!b[257])
    {
        if (!rendererPresent(2))
            return false;
        b[257] = 1;
    }

    statsBeginFrame();

    if (device()->BeginScene() < 0)
    {
        ::renddx9abi::logf("RendDX9Draw.cpp:86 Failed to begin scene");
        return false;
    }
    ++*reinterpret_cast<unsigned int*>(b + 260);

    if (doClear)
    {
        unsigned int colour;
        if (clearRgb)
            colour = (clearRgb[2] & 0xFF)
                   | ((clearRgb[1] & 0xFF) << 8)
                   | ((clearRgb[0] & 0xFF) << 16);
        else
            colour = (*reinterpret_cast<unsigned int*>(b + 920) & 0xFF)
                   | (static_cast<unsigned int>(b[916]) << 8)
                   | (static_cast<unsigned int>(b[912]) << 16);
        flushRenderTargetsSeam();
        device()->Clear(0, 0, 7, colour, 1.0f, 0);
    }

    b[256] = 1;
    return true;
}


// Ends the scene, clears frame flags and handles pending screenshot capture.

bool rendererEndScene()
{
    unsigned char* b = base();
    if (!b[256])
        return false;

    if (geom::g_DebugTextWriterManager)
        geom::debugTextWriterManagerDraw(geom::g_DebugTextWriterManager);

    if (device()->EndScene() < 0)
    {
        ::renddx9abi::logf("RendDX9Draw.cpp:130 Failed to end scene: FrameInProgress %u",
                           static_cast<unsigned int>(b[256]));
        return false;
    }

    const bool wantReset = b[1000] != 0;
    b[257] = 0;
    b[256] = 0;
    if (wantReset)
    {
        // sub_10005200 - the screenshot capture (see the corrected note above). Unwritten; only a
        // screenshot request (0x10004F89) sets base+0x3E8.
        RD9_FRONTIER_AT("dice::hfe::dx9rend::RendDX9 - the screenshot capture "
                        "(sub_10005200)", b, 0xCC);
        b[1000] = 0;
    }
    return true;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
