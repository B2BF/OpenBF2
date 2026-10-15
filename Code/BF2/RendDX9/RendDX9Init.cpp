// D3D device/subsystem initialization, swap chains, reset handling and display-mode probes.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"
#include "Texture.h"                   // textureGetLevelSurface (sub_10014950)
#include "../RendMisc/IRenderView.h"   // the transparency-AA probe's getViewport
#include "../Core/SettingsRepostitory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include <string.h>                    // strlen / memcpy - the display-mode probes
#include <new>                         // the base storage's std containers, constructed in place
#include <vector>

namespace dice {
namespace hfe {

// PluginEntry.cpp, the host "LoadStat" singleton. initDll fetches it but does not
// require it. The device-lost list's lost/reset broadcasts write its byte at +0x08 (see
// setLoadStatDeviceLostByte below).
extern IBase* loadStat;

namespace dx9rend {

namespace Ra { bool createEffectManager(IDirect3DDevice9* device, bool disableShaderCache); }
// the three listener subobjects initializeD3DSubsystems registers directly
// (0x1000B3F3 / 0x1000B5F7 / 0x1000B78D). Each owner keeps its class layout private, so each TU
// exports the shipped `object ? object + N : 0` expression - the null arm included, see below.
namespace Ra { void* effectManagerDeviceLostListener(); }   // Ra/EffectManager.cpp - EM + 4
void* textureManagerDeviceLostListener();                   // TextureManager.cpp   - TM + 4
void* shaderManagerDeviceLostListener();                    // ShaderManager.cpp    - SM + 8
bool createStateManager(void* device);   // StateManager.cpp - dword_1023B984, RendDX9Init.cpp:232
void createRenderEntryPool();     // RenderQueues.cpp - dword_1023DE08
void createRenderQueues();      // RenderQueues.cpp - dword_1023E408
bool createResourceManager();   // ResourceManager.cpp
bool createBufferManager(IDirect3DDevice9* device);
bool createQueryManager(IDirect3DDevice9* device);
bool createTextureManager(IDirect3DDevice9* device);
bool textureManagerInit();
bool createShaderManager(void* base, IDirect3DDevice9* device);
bool shaderManagerInit(void* base);
extern void* g_rendererBase;   // dword_1023B970, defined with the other base globals below
// BF2Render.cpp, dword_1023D098 - the Renderer singleton. The render probe reads the Renderer's
// back-buffer reference (+0x76C) through it, as the shipped instruction does (0x1000A1D6).
extern IBase* g_Renderer;


// Reads and dereferences a host setting result across the string ABI boundary.
int readIntSetting(const char* name, int defaultValue)
{
    if (!settings)
        return defaultValue;

    int          def   = defaultValue;
    renddx9abi::HostString key(name);
    const int* result = &settings->intGet(key, def);
    return result ? *result : defaultValue;
}

// +0x14, the setter. VideoOptions::apply writes GSSkipMipLevels and GSWindowSizeX/Y back through
// it. Unlike the +0x10 getter this returns nothing the callers use - they discard it.
void writeIntSetting(const char* name, int value)
{
    if (!settings)
        return;

    int          v    = value;
    renddx9abi::HostString key(name);
    settings->intSet(key, v);
}

// +0x50, the bool twin of readIntSetting. Same shape: returns a pointer, caller dereferences.
// BF2Render::initRenderer reads GSFullScreen through it.
bool readBoolSetting(const char* name, bool defaultValue)
{
    if (!settings)
        return defaultValue;

    bool          def = defaultValue;
    renddx9abi::HostString key(name);
    const bool* result = &settings->boolGet(key, def);
    return result ? *result : defaultValue;
}

// +0x40, the string getter. Read from TextureManager_loadTextureFromFile's level-directory
// fallback (0x1001860C / 0x10018627): TWO string arguments - name, then a default the caller
// builds as "" - and the return value in EAX is a string the caller copy-constructs from, i.e. a
// reference to the stored value. Same pointer-return shape as the int/bool getters.
void readStringSetting(const char* name, std::string& out)
{
    readStringSettingDefault(name, "", out);
}

void readStringSettingDefault(const char* name, const char* defaultValue, std::string& out)
{
    out = defaultValue;
    if (!settings)
        return;

    renddx9abi::HostString key(name);
    renddx9abi::HostString def(defaultValue);
    const renddx9abi::HostString* result = &settings->stringGet(key, def);
    if (result)
        out = result->c_str();
}

extern void* g_deviceCaps;   // defined below, next to the createDeviceCaps body

// The device-caps object (dword_1023B980). Other TUs still read its D3DCAPS9 head by offset, which
// is pointer-free and identical on x64; everything past the caps is reached by member here.
struct DeviceCaps
{
    D3DCAPS9               caps;        // +0x000
    D3DADAPTER_IDENTIFIER9 adapter;     // +0x130
    int                    gpuFamily;   // +0x57C
    IDirect3D9*            d3d9;        // +0x580
    unsigned int           unknown584;  // +0x584
};

static_assert(sizeof(D3DCAPS9) == 304, "D3DCAPS9 is 304 bytes; GetDeviceCaps writes it at +0");
// Retail x86 layout evidence. createDeviceCaps allocates sizeof(DeviceCaps), and the x64 object
// (8-aligned adapter identifier, 8-byte d3d9) is reached only through these members (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(DeviceCaps) == 1416,
              "initializeD3DSubsystems allocates 1416 for it - see RendDX9Init.cpp:218");
static_assert(offsetof(DeviceCaps, adapter) == 0x130 && offsetof(DeviceCaps, d3d9) == 0x580 &&
              offsetof(DeviceCaps, unknown584) == 0x584, "the DeviceCaps tail");
#endif

namespace {
inline const DeviceCaps* deviceCaps() { return static_cast<const DeviceCaps*>(g_deviceCaps); }
}

// Returns the pixel-shader major version, or zero when device caps are unavailable.
int deviceCapsPixelShaderMajor()
{
    if (!g_deviceCaps)
        return 0;
    const unsigned int psVersion = deviceCaps()->caps.PixelShaderVersion;           // +0xCC
    return static_cast<int>((psVersion >> 8) & 0xFF);
}

// Returns the vertex-shader major version, or zero when device caps are unavailable.
int deviceCapsVertexShaderMajor()
{
    if (!g_deviceCaps)
        return 0;
    const unsigned int vsVersion = deviceCaps()->caps.VertexShaderVersion;          // +0xC4
    return static_cast<int>((vsVersion >> 8) & 0xFF);
}

// The retail helper has no null guard; every caller reaches it only after the
// DeviceCaps object has been published.
bool deviceCapsIsAti()
{
    return deviceCaps()->adapter.VendorId == 0x1002u;                                 // +0x558
}

bool deviceCapsIsNvidia()
{
    if (!g_deviceCaps)
        return false;
    return deviceCaps()->adapter.VendorId == 0x10DEu;                                 // +0x558
}

// CheckDeviceFormat(0, HAL, X8R8G8B8, rendertarget, texture, format).
bool deviceCapsSupportsTextureFormat(unsigned int format)
{
    if (!g_deviceCaps)
        return false;
    IDirect3D9* const d3d = deviceCaps()->d3d9;                                       // +0x580
    return d3d && SUCCEEDED(d3d->CheckDeviceFormat(
        0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET,
        D3DRTYPE_TEXTURE, static_cast<D3DFORMAT>(format)));
}

// permit viewport-sized targets for unrestricted npot hardware and for the
// conditional-npot class; otherwise Renderer uses its fixed 512/128 fallback sizes.
bool deviceCapsSupportsFullSizeRenderTargets()
{
    if (!g_deviceCaps)
        return false;
    const unsigned int textureCaps = deviceCaps()->caps.TextureCaps;                  // +0x3C
    return (textureCaps & D3DPTEXTURECAPS_POW2) == 0
        || (textureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0;
}

// The dword at +0x584 (unknown584). Requires the published DeviceCaps object.
unsigned int deviceCapsUnknown584()
{
    return deviceCaps()->unknown584;
}

namespace {

// The Renderer's base object, overlaid at offset 0. See RendDX9Base.h for why it is a view rather
// than a base class.
inline RendDX9Base* baseOf(void* renderer)
{
    return reinterpret_cast<RendDX9Base*>(renderer);
}

// [CHOSEN, not recovered] - a redundant handle to the primary swap chain.
// the real store is the base's swapChains vector (see swapChainVectorPushBack); this global is kept
// only as a convenience/debug handle. RW-55 is retired - the offset is read.
IDirect3DSwapChain9* g_primarySwapChain = 0;


// sub_1000C630 - push_back onto the base's swap-chain list.
void swapChainVectorPushBack(RendDX9Base* b, IDirect3DSwapChain9* chain)
{
    b->swapChains.push_back(chain);

    // the current swap-chain index present indexes by. The base ctor leaves it 0; make that
    // explicit so present's chains[index] lands on the primary chain regardless.
    b->currentSwapChain = 0;
}

} // anonymous namespace


// Clears the renderer's deep copies of host display-mode ranges.

void clearDisplayModeCopies(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    // sub_1000C9B0, the display subobject's destructor, uses this exact reverse member order.
    b->windows.clear();
    clearMultiSampleList(b);
    b->depthFormats.clear();
}

void copyDisplayModeFrom(void* renderer, const HWndBf2TempSlt* src)
{
    RendDX9Base* b = baseOf(renderer);

    clearDisplayModeCopies(renderer);

    b->fullscreen  = 0;
    b->modeWidth   = 0;
    b->modeHeight  = 0;
    b->modeRefresh = 0;
    b->modeBpp     = 0;
    b->vsync       = 0;

    if (!src)
    {
        ::renddx9abi::logf("copyDisplayModeFrom: hWndBf2TempSlt not registered - no display mode");
        return;
    }

    b->fullscreen  = src->fullscreen;
    b->modeWidth   = src->width;
    b->modeHeight  = src->height;
    b->modeRefresh = src->refreshHz;
    b->modeBpp     = src->bpp;
    b->vsync       = src->vsync;

    b->depthFormats.assign(src->depthFormats.begin(), src->depthFormats.end());
    assignMultiSampleList(b, renddx9abi::hostMultiSampleFirst(*src),
                          renddx9abi::hostMultiSampleLast(*src));
    b->windows.assign(src->windows.begin(), src->windows.end());

    const unsigned int depthCount = static_cast<unsigned int>(b->depthFormats.size());
    const unsigned int hwndCount  = static_cast<unsigned int>(b->windows.size());

    const unsigned int multiSampleCount = static_cast<unsigned int>(b->multiSamples.size());

    ::renddx9abi::logf("copyDisplayModeFrom: %ux%u @%uHz %ubpp fullscreen=%u vsync=%u "
                       "depthFormats=%u (first=%d) multisample=%u hwnds=%u",
                       b->modeWidth, b->modeHeight, b->modeRefresh, b->modeBpp,
                       b->fullscreen, b->vsync, depthCount,
                       depthCount ? static_cast<int>(b->depthFormats[0]) : -1,
                       multiSampleCount,
                       hwndCount);
}


// Render-path listeners form a list and receive each active-path change.

namespace {


// Device listeners are ordered by unsigned priority with stable registration ties.
// Loss and destruction traverse backward; reset traverses forward. Loss is notified once
// until a reset clears the flag, while reset notifications are unconditional.

typedef std::list<DeviceLostResetEntry> DeviceLostResetEntries;

// PE 0x10002490, the list constructor: empty, both bytes clear.
void deviceLostResetListCtor(DeviceLostResetList* storage)
{
    new (storage) DeviceLostResetList();
}

// unsigned, as 0x10002CF3's `jae` compares. Every shipped priority is a small positive immediate
// (1..5), so the signedness never shows; it is kept for the record.
struct DeviceLostResetPriorityLess
{
    bool operator()(const DeviceLostResetEntry& a, const DeviceLostResetEntry& b) const
    {
        return static_cast<unsigned int>(a.priority) < static_cast<unsigned int>(b.priority);
    }
};

// Sorts ascending by priority, retaining registration order for ties (std::list::sort is stable).
void sortDeviceLostResetList(DeviceLostResetList& list)
{
    if (!list.dirty)
        return;
    list.entries.sort(DeviceLostResetPriorityLess());
    list.dirty = 0;
}

// Suppresses host loading-screen callbacks while the device is lost.
// Requires LoadStat; its flag at +8 is shared by native and compatibility layouts.
void setLoadStatDeviceLostByte(unsigned char value)
{
    unsigned char* const raw = static_cast<unsigned char*>(static_cast<void*>(::dice::hfe::loadStat));
    raw[8] = value;
}

// [CHOSEN, not recovered] - diagnostics only: the shipped broadcasts log nothing. Naming every
// listener in walk order is what a failed IDirect3DDevice9::Reset has to be diagnosed from, but a
// loss is NOT always rare: while the app is inactive, Renderer +0xBC runs the loss loop every 100 ms,
// and on a healthy windowed device each run is a full lost/Reset/reset cycle (dcv_core R2). So the
// full walk is logged for the first two broadcasts of each kind only, then one summary line at hits
// that are powers of two. Reads only; no control flow depends on it.
void logDeviceLostResetWalk(const char* what, const DeviceLostResetList& list, bool reverse,
                            unsigned int& hits)
{
    ++hits;
    const unsigned int size = static_cast<unsigned int>(list.entries.size());
    if (hits > 2u)
    {
        if ((hits & (hits - 1u)) == 0u)
            ::renddx9abi::logf("DeviceLostResetListenerList: %s broadcast #%u over %u listener(s)",
                               what, hits, size);
        return;
    }
    ::renddx9abi::logf("DeviceLostResetListenerList: %s broadcast #%u over %u listener(s)",
                       what, hits, size);
    struct Local
    {
        static void line(const DeviceLostResetEntry& entry)
        {
            ::renddx9abi::logf("    priority %d  listener 0x%08X  vtable 0x%08X", entry.priority,
                               ::renddx9abi::addr(entry.listener),
                               entry.listener
                                   ? ::renddx9abi::addr(*reinterpret_cast<void* const*>(entry.listener))
                                   : 0u);
        }
    };
    if (reverse)
        for (DeviceLostResetEntries::const_reverse_iterator it = list.entries.rbegin();
             it != list.entries.rend(); ++it)
            Local::line(*it);
    else
        for (DeviceLostResetEntries::const_iterator it = list.entries.begin();
             it != list.entries.end(); ++it)
            Local::line(*it);
}

DeviceLostResetList& deviceLostResetList()
{
    return static_cast<RendDX9Base*>(g_rendererBase)->deviceLostList;
}

} // anonymous namespace

void registerDeviceLostResetListener(DeviceLostResetListener* listener, int priority)
{
    DeviceLostResetList& list = deviceLostResetList();
    // PE 0x100024B0 appends and marks the ordering dirty.
    const DeviceLostResetEntry entry = { priority, listener };
    list.entries.push_back(entry);
    list.dirty = 1;
}

void unregisterDeviceLostResetListener(DeviceLostResetListener* listener)
{
    DeviceLostResetList& list = deviceLostResetList();
    for (DeviceLostResetEntries::iterator it = list.entries.begin(); it != list.entries.end(); ++it)
    {
        if (it->listener != listener)
            continue;
        list.entries.erase(it);
        return;
    }
}

void registerDeviceLostResetListener(void* listener, int priority)
{
    registerDeviceLostResetListener(
        reinterpret_cast<DeviceLostResetListener*>(listener), priority);
}

void unregisterDeviceLostResetListener(void* listener)
{
    unregisterDeviceLostResetListener(
        reinterpret_cast<DeviceLostResetListener*>(listener));
}

namespace {

// Broadcasts a device loss once until reset, setting LoadStat before sorting.
// Every registered listener must be non-null.
void listBroadcastLost(DeviceLostResetList& list)
{
    if (list.deviceLost)
        return;
    setLoadStatDeviceLostByte(1);
    sortDeviceLostResetList(list);
    static unsigned int lostHits = 0;                                            // [CHOSEN] diagnostics
    logDeviceLostResetWalk("onDeviceLost (descending priority)", list, true, lostHits);
    for (DeviceLostResetEntries::reverse_iterator it = list.entries.rbegin();
         it != list.entries.rend(); ++it)
        it->listener->onDeviceLost();
    list.deviceLost = 1;
}

// PE 0x10002590, read in full. No entry test of the lost byte - a reset broadcast always walks.
// The LoadStat byte is cleared after the walk, on both the empty and the non-empty exit.
void listBroadcastReset(DeviceLostResetList& list)
{
    sortDeviceLostResetList(list);
    static unsigned int resetHits = 0;                                           // [CHOSEN] diagnostics
    logDeviceLostResetWalk("onDeviceReset (ascending priority)", list, false, resetHits);
    for (DeviceLostResetEntries::iterator it = list.entries.begin(); it != list.entries.end(); ++it)
        it->listener->onDeviceReset();
    setLoadStatDeviceLostByte(0);
    list.deviceLost = 0;
}

} // anonymous namespace

void notifyDeviceLostResetListenersLost()
{
    listBroadcastLost(deviceLostResetList());
}

void notifyDeviceLostResetListenersReset()
{
    listBroadcastReset(deviceLostResetList());
}

// PE 0x10002600, read in full: sort, then the reverse walk calling slot +0x08. Neither flag byte
// nor the LoadStat byte is touched. Its only shipped caller is RendDX9_shutdownD3DSubsystems
// (0x1000ADB0, at 0x1000AE98), which this tree does not have yet - so it is published for that
// body and has no caller today.
void notifyDeviceLostResetListenersDestroy()
{
    DeviceLostResetList& list = deviceLostResetList();
    sortDeviceLostResetList(list);
    for (DeviceLostResetEntries::reverse_iterator it = list.entries.rbegin();
         it != list.entries.rend(); ++it)
        it->listener->onDeviceDestroy();
}

void renderPathListCtor(void* list)
{
    new (list) RenderPathList();
}

void setActiveRenderPath(RenderPathList& list, int path)
{
    if (!list.constructed)
    {
        RD9_FRONTIER_AT("dice::hfe::dx9rend::RenderPathListenerList::setActiveRenderPath "
                        "(the list was never constructed)", &list, 0x00);
        return;
    }

    for (std::list<RenderPathListener*>::iterator it = list.listeners.begin();
         it != list.listeners.end(); ++it)
        (*it)->onRenderPathActivated(path);

    list.activeRenderPath = path;
    list.changed          = 1;
}

// PE 0x1000D170 + 0x10009D80.  Insert immediately before the circular sentinel; if a render path
// has already been selected, notify slot +0 now.  The public registration wrapper then invokes
// listener slot +0x08 with the Renderer listener's current-path getter (+0x10), even though both
// shipped Renderer and LightManager implementations make that +0x08 callback a nullsub.
void registerRenderPathListener(RenderPathListener* listener)
{
    RenderPathList& list = static_cast<RendDX9Base*>(g_rendererBase)->renderPathList;
    if (!list.constructed)
    {
        RD9_FRONTIER_AT("dice::hfe::dx9rend::RendDX9_registerRenderPathListener "
                        "(render-path list was never constructed)", listener, 0x00);
        return;
    }

    list.listeners.push_back(listener);

    if (list.changed)
        listener->onRenderPathActivated(list.activeRenderPath);

    // The Renderer's +0x540 subobject, named by its owner TU (issue #7).
    RenderPathListener* const rendererListener = rendererRenderPathListener();
    listener->onRenderPathChanged(rendererListener->getCurrentRenderPath());
}

// PE 0x1000D020 + public wrapper 0x10009DC0.  Removal is followed by the listener's +0x04
// notification only after a render path has been published; LightManager's shipped +0x04 body is
// a nullsub, but preserving the dispatch matters for the shared listener contract.
void unregisterRenderPathListener(RenderPathListener* listener)
{
    RenderPathList& list = static_cast<RendDX9Base*>(g_rendererBase)->renderPathList;
    for (std::list<RenderPathListener*>::iterator it = list.listeners.begin();
         it != list.listeners.end(); ++it)
    {
        if (*it == listener)
        {
            list.listeners.erase(it);
            break;
        }
    }

    if (list.changed)
        listener->onRenderPathDeactivated();
}

void registerRenderPathListener(void* listener)
{
    registerRenderPathListener(reinterpret_cast<RenderPathListener*>(listener));
}

void unregisterRenderPathListener(void* listener)
{
    unregisterRenderPathListener(reinterpret_cast<RenderPathListener*>(listener));
}

// RendererBase +0x518 is the performance-feature bitset.  The original performs no
// range check; all recovered callers pass a literal in [0, 20].
bool rendererPerformanceFlag(unsigned int bit)
{
    const unsigned int flags = static_cast<const RendDX9Base*>(g_rendererBase)->performanceFlags;
    const unsigned int mask = 1u << bit;
    return (flags & mask) == mask;
}


// Builds D3D presentation parameters. The depth-candidate vector must contain an entry;
// the multisample vector may be empty and is checked separately.

void buildPresentParameters(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    ::memset(&b->pp, 0, sizeof(b->pp));

    b->pp.Flags                  = 2;   // D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL
    b->pp.EnableAutoDepthStencil = TRUE;

    // *(this+76) = *v3, where v3 = *(this+176) - the depth vector's _First, dereferenced unguarded.
    if (!b->depthFormats.empty())
    {
        b->pp.AutoDepthStencilFormat = static_cast<D3DFORMAT>(b->depthFormats[0]);
    }
    else
    {
        // The original would fault here. We are not in a position to invent a depth format, and a
        // device built with the wrong one is worse than none.
        // [FRONTIER] dice::hfe::dx9rend::buildPresentParameters - empty depth-candidate list
        RD9_FRONTIER_AT("dice::hfe::dx9rend::buildPresentParameters "
                        "(BF2 supplied no depth-stencil format; the original dereferences null "
                        "here)", renderer, 0x00);
        return;
    }

    // The multisample pair, 8 bytes, only if the vector is non-empty.
    if (!b->multiSamples.empty())
    {
        b->pp.MultiSampleType    = static_cast<D3DMULTISAMPLE_TYPE>(b->multiSamples[0].type);
        b->pp.MultiSampleQuality = b->multiSamples[0].quality;
    }

    b->pp.SwapEffect      = D3DSWAPEFFECT_DISCARD;   // 1
    b->pp.BackBufferCount = 1;

    // 1 == D3DPRESENT_INTERVAL_ONE, 0x80000000 == D3DPRESENT_INTERVAL_IMMEDIATE
    b->pp.PresentationInterval = b->vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;

    // sub_1000CD20: `return 2 * (bpp != 32) + 21` - D3DFMT_A8R8G8B8 (21) at 32bpp, D3DFMT_R5G6B5
    // (23) otherwise. Reproduced as the arithmetic, because that is how it is written.
    const D3DFORMAT backBufferFormat =
        static_cast<D3DFORMAT>(2 * (b->modeBpp != 32) + 21);

    if (!b->fullscreen)
    {
        // Zero dimensions let D3D size the windowed back buffer from the window.
        b->pp.BackBufferFormat = backBufferFormat;
        b->pp.Windowed         = TRUE;
    }
    else
    {
        b->pp.BackBufferFormat            = backBufferFormat;
        b->pp.BackBufferHeight            = b->modeHeight;
        b->pp.BackBufferWidth             = b->modeWidth;
        b->pp.FullScreen_RefreshRateInHz  = b->modeRefresh;
    }

    // hDeviceWindow = *_First of the hwnd vector - the window BF2 pushed in at 0x45468A.
    b->pp.hDeviceWindow = !b->windows.empty() ? reinterpret_cast<HWND>(b->windows[0]) : 0;

    ::renddx9abi::logf("presentParameters: %ux%u fmt=%d depth=%d windowed=%d refresh=%u "
                       "interval=0x%08X hWnd=0x%08X",
                       b->pp.BackBufferWidth, b->pp.BackBufferHeight,
                       static_cast<int>(b->pp.BackBufferFormat),
                       static_cast<int>(b->pp.AutoDepthStencilFormat),
                       b->pp.Windowed, b->pp.FullScreen_RefreshRateInHz,
                       b->pp.PresentationInterval,
                       ::renddx9abi::addr(b->pp.hDeviceWindow));
}


// RendDX9_createPrimaryD3DDevice, 0x1000A780. RendDX9Init.cpp:485/494/504. complete.

bool createPrimaryD3DDevice(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    if (!b->d3d9 || b->windows.empty())
        return false;

    HWND hwnd = reinterpret_cast<HWND>(b->windows[0]);

    // Keep the window-info query even though the result is unused.
    WINDOWINFO pwi;
    ::memset(&pwi, 0, sizeof(pwi));
    pwi.cbSize = sizeof(pwi);
    ::GetWindowInfo(hwnd, &pwi);

    HRESULT hr = b->d3d9->CreateDevice(b->adapter,
                                       static_cast<D3DDEVTYPE>(b->deviceType),
                                       hwnd,
                                       b->behaviorFlags,
                                       &b->pp,
                                       &b->device);

    if (hr != D3D_OK)
    {
        // exactly ONE error IS retried, AND exactly ONE BIT IS cleared. The original tests
        // `v2 != -2005530518`, which is D3DERR_NOTAVAILABLE - and on that one error
        // retries with `behaviourFlags & 0xFFFFFFEF`, i.e. D3DCREATE_PUREDEVICE (0x10) removed.
        // Any other HRESULT is fatal at RendDX9Init.cpp:485.
        if (hr != D3DERR_NOTAVAILABLE)
        {
            ::renddx9abi::logf("RendDX9Init.cpp:485 Failed to create d3d device. Unknown cause. "
                               "hr=0x%08X", static_cast<unsigned int>(hr));
            return false;
        }

        hr = b->d3d9->CreateDevice(b->adapter,
                                   static_cast<D3DDEVTYPE>(b->deviceType),
                                   hwnd,
                                   b->behaviorFlags & 0xFFFFFFEFu,
                                   &b->pp,
                                   &b->device);
        if (hr != D3D_OK)
        {
            ::renddx9abi::logf("RendDX9Init.cpp:494 Failed to create d3d device. Unknown cause. "
                               "hr=0x%08X (retry without D3DCREATE_PUREDEVICE)",
                               static_cast<unsigned int>(hr));
            return false;
        }
        ::renddx9abi::logf("createPrimaryD3DDevice: D3DERR_NOTAVAILABLE on the first attempt; "
                           "succeeded with D3DCREATE_PUREDEVICE cleared");
    }

    // GetSwapChain is IDirect3DDevice9 vtable +56 (slot 14). The original checks `>= 0`, i.e.
    // succeeded, not `== D3D_OK`.
    IDirect3DSwapChain9* swapChain = 0;
    if (FAILED(b->device->GetSwapChain(0, &swapChain)))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:504 Failed to get swap chain 0 ptr.");
        return false;
    }

    // sub_1000C630(&swapChain) - push_back onto the swap-chain list (retail base+0xE0; the shipped
    // caller does `lea ecx,[esi+0E0h]`). Renderer::present indexes this list by currentSwapChain,
    // so leaving it empty is a null dereference the first time the menu presents - which is
    // exactly the crash a retail run hit.
    swapChainVectorPushBack(b, swapChain);
    g_primarySwapChain = swapChain;

    ::renddx9abi::logf("createPrimaryD3DDevice: device=0x%08X swapChain=0x%08X",
                       ::renddx9abi::addr(b->device), ::renddx9abi::addr(swapChain));
    return true;
}


// Creates the primary device and swap chain, plus a chain for each extra window.
// Windowed mode uses the desktop back-buffer format and validates the depth/format pair.

bool createD3DDeviceAndSwapChains(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    if (!b->d3d9 || b->windows.empty())
        return false;

    // IDirect3D9::GetAdapterDisplayMode - vtable +32, slot 8.
    D3DDISPLAYMODE desktop;
    ::memset(&desktop, 0, sizeof(desktop));
    if (FAILED(b->d3d9->GetAdapterDisplayMode(0, &desktop)))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:517 Failed to get desktop mode.");
        return false;
    }

    // The desktop format becomes the back-buffer format. This deliberately overwrites what
    // buildPresentParameters derived from the bpp.
    b->pp.BackBufferFormat = desktop.Format;

    // IDirect3D9::CheckDeviceType - vtable +36, slot 9. AdapterFormat and BackBufferFormat are both
    // the desktop's here, and bWindowed is true.
    HRESULT hr = b->d3d9->CheckDeviceType(b->adapter,
                                          static_cast<D3DDEVTYPE>(b->deviceType),
                                          desktop.Format,
                                          desktop.Format,
                                          TRUE);
    if (hr != D3D_OK)
    {
        if (hr == D3DERR_NOTAVAILABLE)
            ::renddx9abi::logf("RendDX9Init.cpp:526 Failed to find valid format for windowed mode. "
                               "(desktop format %d, deviceType %u)",
                               static_cast<int>(desktop.Format), b->deviceType);
        else
            ::renddx9abi::logf("RendDX9Init.cpp:528 Unknown error in d3d init. hr=0x%08X",
                               static_cast<unsigned int>(hr));
        return false;
    }

    HWND hwnd = reinterpret_cast<HWND>(b->windows[0]);

    // The same CreateDevice and the same single puredevice retry as the fullscreen path, with its
    // own pair of line numbers.
    hr = b->d3d9->CreateDevice(b->adapter,
                               static_cast<D3DDEVTYPE>(b->deviceType),
                               hwnd,
                               b->behaviorFlags,
                               &b->pp,
                               &b->device);
    if (hr != D3D_OK)
    {
        if (hr != D3DERR_NOTAVAILABLE)
        {
            ::renddx9abi::logf("RendDX9Init.cpp:538 Failed to create d3d device. Unknown cause. "
                               "hr=0x%08X", static_cast<unsigned int>(hr));
            return false;
        }

        hr = b->d3d9->CreateDevice(b->adapter,
                                   static_cast<D3DDEVTYPE>(b->deviceType),
                                   hwnd,
                                   b->behaviorFlags & 0xFFFFFFEFu,
                                   &b->pp,
                                   &b->device);
        if (hr != D3D_OK)
        {
            ::renddx9abi::logf("RendDX9Init.cpp:547 Failed to create d3d device. Unknown cause. "
                               "hr=0x%08X (retry without D3DCREATE_PUREDEVICE)",
                               static_cast<unsigned int>(hr));
            return false;
        }
        ::renddx9abi::logf("createD3DDeviceAndSwapChains: D3DERR_NOTAVAILABLE on the first attempt; "
                           "succeeded with D3DCREATE_PUREDEVICE cleared");
    }

    // IDirect3DDevice9::GetSwapChain - vtable +56, slot 14.
    IDirect3DSwapChain9* swapChain0 = 0;
    if (FAILED(b->device->GetSwapChain(0, &swapChain0)))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:557 Failed to get swap chain 0 ptr.");
        return false;
    }
    swapChainVectorPushBack(b, swapChain0);     // see createPrimaryD3DDevice's note
    g_primarySwapChain = swapChain0;

    ::renddx9abi::logf("createD3DDeviceAndSwapChains: device=0x%08X swapChain0=0x%08X "
                       "desktop=%ux%u fmt=%d",
                       ::renddx9abi::addr(b->device), ::renddx9abi::addr(swapChain0),
                       desktop.Width, desktop.Height, static_cast<int>(desktop.Format));

    // One additional swap chain per remaining window. i starts at 1 - swap chain 0 belongs to the
    // window the device was created against.
    const unsigned int windowCount = static_cast<unsigned int>(b->windows.size());

    for (unsigned int i = 1; i < windowCount; ++i)
    {
        b->pp.hDeviceWindow = reinterpret_cast<HWND>(b->windows[i]);

        // IDirect3DDevice9::CreateAdditionalSwapChain - vtable +52, slot 13.
        IDirect3DSwapChain9* extra = 0;
        HRESULT chainHr = b->device->CreateAdditionalSwapChain(&b->pp, &extra);
        if (chainHr != D3D_OK)
        {
            ::renddx9abi::logf("RendDX9Init.cpp:568 Failed to create additional swap chains. "
                               "hr=0x%08X (window %u of %u)",
                               static_cast<unsigned int>(chainHr), i, windowCount);
            return false;
        }

        // sub_1000C630 again - push the extra chain onto the same list.
        swapChainVectorPushBack(b, extra);
        ::renddx9abi::logf("createD3DDeviceAndSwapChains: additional swap chain %u = 0x%08X",
                           i, ::renddx9abi::addr(extra));
    }

    return true;
}


// Classifies supported GPUs by vendor and device id; unknown devices return -1.
namespace {

int classifyGpu(const D3DADAPTER_IDENTIFIER9& id)
{
    if (id.VendorId == 4318u)            // 0x10DE - nvidia
    {
        const unsigned int dev = id.DeviceId;

        if (dev >= 0x301u && dev <= 0x332u)
            return 5;

        if (dev > 0xFFu)
        {
            // The body subtracts 320 (0x140) and switches on the remainder.
            const unsigned int k = dev - 320u;
            switch (k)
            {
            case 0x00u: case 0x01u: case 0x02u: case 0x03u:
            case 0x0Bu: case 0x0Cu: case 0x0Du: case 0x0Eu: case 0x0Fu:
            case 0x20u: case 0x21u: case 0x22u: case 0x23u: case 0x25u:
            case 0x2Eu:
            case 0xE0u: case 0xE1u: case 0xE2u:
                return 6;
            default:
                return 4;
            }
        }

        if (dev < 0xF8u)
        {
            const unsigned int k = dev - 64u;
            switch (k)
            {
            case 0x00u: case 0x01u: case 0x02u: case 0x03u:
            case 0x05u: case 0x09u: case 0x0Eu:
            case 0x80u: case 0x81u: case 0x82u:
            case 0x8Cu: case 0x8Du: case 0x8Eu:
            case 0xB0u: case 0xB1u: case 0xB2u: case 0xB3u:
                return 6;
            default:
                return 4;
            }
        }

        return 6;
    }

    if (id.VendorId == 4098u)            // 0x1002 - ATI / AMD
    {
        const unsigned int dev = id.DeviceId;
        if (dev == 20037u || dev == 20042u)
            return 1;
        if (dev >= 0x4A48u && dev <= 0x4A70u)
            return 2;
        return 0;
    }

    // Unrecognized vendors retain the default -1 result.
    return -1;
}

} // anonymous namespace

// dword_1023B980 in the shipped image. ShaderManager and Ra::EffectManager read it directly.
void* g_deviceCaps = 0;

// dword_1023B970 - the base object itself. RendDX9_base_ctor publishes it (`dword_1023B970 = this`)
// and several subsystems reach the display mode, the multisample list and the VideoOptions through
// it rather than being handed a pointer.
void* g_rendererBase = 0;

bool createDeviceCaps(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    if (!b->d3d9)
        return false;

    DeviceCaps* dc = static_cast<DeviceCaps*>(::renddx9abi::engineAlloc(sizeof(DeviceCaps)));
    if (!dc)
        return false;

    ::memset(dc, 0, sizeof(DeviceCaps));

    // The constructor's own order: store the IDirect3D9, AddRef it, then fill the caps.
    dc->d3d9 = b->d3d9;
    b->d3d9->AddRef();

    // (*(vtbl+56))(d3d9, 0, base[33], this) - IDirect3D9::GetDeviceCaps(Adapter, DevType, pCaps),
    // adapter hard-coded to 0 and the device type taken from base+0x84. The caps land at `this+0`.
    b->d3d9->GetDeviceCaps(0, static_cast<D3DDEVTYPE>(b->deviceType), &dc->caps);

    g_deviceCaps = dc;

    // qmemcpy(this + 304, base[232], 0x44C) - the first adapter's identifier, copied whole.
    if (!b->adapterInfo.empty())
    {
        dc->adapter = b->adapterInfo[0];
    }
    else
    {
        // The original indexes the list without checking it. Ours says so instead.
        // [FRONTIER] dice::hfe::dx9rend::DeviceCaps - empty adapter list
        ::renddx9abi::logf("createDeviceCaps: the adapter list is empty - VendorId/DeviceId stay 0 "
                           "and the GPU classification will be -1");
    }

    dc->gpuFamily  = classifyGpu(dc->adapter);
    dc->unknown584 = 0;   // *(this+353) = base[35]; base+0x8C is zero on every written path

    // NOT reproduced: an nvidia-only CreateQuery probe. The original, when VendorId is 0x10DE,
    // calls device vtable +144 (CreateQuery) with 0x4642414E and releases whatever comes back - a
    // driver feature handshake. It is skipped here because it only runs on nvidia hardware and its
    // purpose is unread; on anything else the original skips it too.
    // [FRONTIER] dice::hfe::dx9rend::DeviceCaps - the nvidia CreateQuery probe

    ::renddx9abi::logf("createDeviceCaps: VendorId=0x%04X DeviceId=0x%04X \"%s\" -> gpuFamily=%d, "
                       "PS=%u.%u VS=%u.%u",
                       dc->adapter.VendorId, dc->adapter.DeviceId, dc->adapter.Description,
                       dc->gpuFamily,
                       (dc->caps.PixelShaderVersion >> 8) & 0xFF, dc->caps.PixelShaderVersion & 0xFF,
                       (dc->caps.VertexShaderVersion >> 8) & 0xFF, dc->caps.VertexShaderVersion & 0xFF);
    return true;
}


// sub_1000B0D0, enumerate adapters, build the present parameters, pick a path.

bool createD3DDevice(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    b->adapter = 0;

    if (b->d3d9)
    {
        const UINT count = b->d3d9->GetAdapterCount();

        // Capture complete adapter identifiers for subsequent DeviceCaps reads; only the
        // identifiers that were read are kept.
        if (count)
        {
            b->adapterInfo.reserve(count);

            for (UINT i = 0; i < count; ++i)
            {
                D3DADAPTER_IDENTIFIER9 identifier;
                ::memset(&identifier, 0, sizeof(identifier));
                if (SUCCEEDED(b->d3d9->GetAdapterIdentifier(i, 0, &identifier)))
                {
                    ::renddx9abi::logf("adapter %u: %s / %s", i,
                                       identifier.Description, identifier.Driver);
                    b->adapterInfo.push_back(identifier);
                }
            }
        }
    }

    buildPresentParameters(renderer);

    if (b->fullscreen)
        return createPrimaryD3DDevice(renderer);

    // Windowed mode validates against the desktop and may create additional swap chains.
    return createD3DDeviceAndSwapChains(renderer);
}


// RendDX9_initializeD3DSubsystems, 0x1000B160. RendDX9Init.cpp:210-298.

// Initializes renderer subsystems, returning false on failure.
// The base constructor intentionally discards the result and continues.

bool initializeD3DSubsystems(void* renderer)
{
    // [CHOSEN, not recovered] D3D9 creation proves runtime presence only.
    // [FRONTIER] The stricter DirectX >= 9.0c version check remains unavailable.
    RendDX9Base* b = baseOf(renderer);
    if (!b->d3d9)
    {
        ::renddx9abi::logf("initializeD3DSubsystems: Direct3DCreate9 returned null - no Direct3D 9 "
                           "runtime. The original would have shown its 9.0c MessageBox here.");
        return false;
    }

    if (!createD3DDevice(renderer))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:210 Failed to init D3D device.");
        return false;
    }

    // The eight subsystems, in the shipped order, with the sizes the original allocates and the
    // line number of each failure diagnostic. None is written. They are listed one per line rather
    // than as one vague gap because each is an independent next target.
    // [FRONTIER] dice::hfe::dx9rend::initializeD3DSubsystems - the eight subsystems
    // RendDX9Init.cpp:218 - the first subsystem, and now the only one of the eight that is built.
    if (!createDeviceCaps(renderer))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:218 Failed to create devicecaps.");
        return false;
    }

    // VideoOptions_initialize(this + 254) - the shipped call site, immediately after DeviceCaps and
    // before the StateManager. It needs the caps, which is why it sits here and not in the base
    // constructor.
    videoOptionsInitialize(renderer);

    // [FRONTIER] VideoSettings still lacks the VideoOptions handoff.
    // Construct StateManager before EffectManager so subsequent draws can publish state.
    if (!createStateManager(b->device))
        return false;

    // The 168-byte class between the StateManager and the QueryManager, with no error line of its
    // own - dword_1023D954, now identified as Ra::EffectManager from the __FILE__ its own failure
    // path quotes (Ra/EffectManager.cpp:438).
    Ra::createEffectManager(b->device, readBoolSetting("GSDisableShaderCache", false));

    // Register effect loss handling before constructing queues and shader constants.
    // A failed effect-manager allocation leaves a null listener, matching the unchecked path.
    registerDeviceLostResetListener(Ra::effectManagerDeviceLostListener(), 5);

    createRenderEntryPool();
    createRenderQueues();
    createRaShaderConstants(b->device);

    // Initialize query ownership before Renderer allocates its frame queries.
    if (!createQueryManager(b->device))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:249 Failed to create querymanager.");
        return false;
    }

    // RendDX9Init.cpp:256. The order is the shipped one - the ResourceManager is constructed
    // before the TextureManager below, and its ready flag is set as soon as it exists.
    if (!createResourceManager())
    {
        ::renddx9abi::logf("RendDX9Init.cpp:256 Failed to create resourcemanager.");
        return false;
    }

    // RendDX9Init.cpp:267 / :272 - create then init, both in their shipped positions.
    if (!createTextureManager(b->device))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:267 Failed to create texturemanager.");
        return false;
    }
    if (!textureManagerInit())
    {
        ::renddx9abi::logf("RendDX9Init.cpp:272 Failed to init texturemanager.");
        return false;
    }

    // 0x1000B5F7..0x1000B60C: `dword_1023B974 ? dword_1023B974 + 4 : 0`,
    // priority 3, direct call right after init succeeded and before the BufferManager
    // is allocated. The TextureManager's listener (off_101DE7AC) drops the D3D texture of every
    // cached pool-0 (default) texture on loss and re-reads it from disk on reset.
    registerDeviceLostResetListener(textureManagerDeviceLostListener(), 3);

    // RendDX9Init.cpp:280/285 - written, and in its shipped position. BufferManager.cpp's
    // createBufferManager performs the shipped priority-3 registration of the manager itself
    // (0x1000B6C6..0x1000B6D0, `push 3; push dword_1023B978` - no adjustment, no null test) right
    // after init, which keeps it after the TextureManager above: equal priorities walk in
    // registration order, so the order of these two lines is part of the reset order.
    createBufferManager(b->device);

    // RendDX9Init.cpp:293 / :298 - create then init, both in their shipped positions. The class and
    // its define table are written; the effect cache behind getEffect is not, which is why the
    // overlay texture is still blocked. See the note at the end of ShaderManager.cpp.
    if (!createShaderManager(renderer, b->device))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:293 Failed to create shadermanager.");
        return false;
    }
    if (!shaderManagerInit(renderer))
    {
        ::renddx9abi::logf("RendDX9Init.cpp:298 Failed to init shadermanager.");
        return false;
    }

    // 0x1000B78D..0x1000B7A2: `dword_1023B97C ? dword_1023B97C + 8 : 0`,
    // priority 5, direct call the body's last act before the stack temp's destructor
    // (0x1000CA10) and `return 1`. The ShaderManager's listener (off_101DFE50) runs
    // ID3DXEffect::OnLostDevice / OnResetDevice over every cached Effect.
    registerDeviceLostResetListener(shaderManagerDeviceLostListener(), 5);

    ::renddx9abi::logf("initializeD3DSubsystems: device, DeviceCaps, VideoOptions, StateManager, "
                       "EffectManager, QueryManager, ResourceManager, TextureManager, "
                       "BufferManager and ShaderManager are up - ALL NINE subsystems.");
    return true;
}


// Device-reset helpers and cooperative-level loop.

namespace {

// [CHOSEN, not recovered] - diagnostics only. The shipped reset returns false silently for its five
// known failure codes. The loss loop below then retries the Reset for as long as the device keeps
// answering D3DERR_DEVICENOTRESET - which is forever when some D3DPOOL_DEFAULT resource was not
// released by a listener (Reset answers D3DERR_INVALIDCALL every time). Retail never reaches that
// state; this rebuild reaches it whenever a class that owns default-pool resources has no wired
// listener yet, and then the only evidence is this line. Rate-limited (hits 1..4, then powers of
// two) because the loop spins; no control flow depends on it.
void noteResetFailure(HRESULT hr)
{
    static unsigned int hits = 0;
    ++hits;
    if (hits > 4u && (hits & (hits - 1u)) != 0u)
        return;
    ::renddx9abi::logf("resetDevice: IDirect3DDevice9::Reset failed with a known code, hr=0x%08X "
                       "(hit #%u)%s", static_cast<unsigned int>(hr), hits,
                       hr == D3DERR_INVALIDCALL
                           ? " - D3DERR_INVALIDCALL: a D3DPOOL_DEFAULT resource or an un-lost "
                             "ID3DXEffect is still alive (no listener released it)"
                           : "");
}

} // anonymous namespace


// Resets the device with current presentation parameters. Returns true only for D3D_OK;
// expected failure codes are silent, while unknown results log the HRESULT.

bool resetDevice(void* renderer)
{
    RendDX9Base* const b = baseOf(renderer);
    const HRESULT hr = b->device->Reset(&b->pp);

    if (hr == D3DERR_DEVICELOST || hr == E_OUTOFMEMORY || hr == D3DERR_OUTOFVIDEOMEMORY ||
        hr == D3DERR_DRIVERINTERNALERROR || hr == D3DERR_INVALIDCALL)
    {
        noteResetFailure(hr);                           // [CHOSEN] diagnostics, see above
        return false;
    }
    if (hr == D3D_OK)
        return true;

    // Retail's Debug line fires on every failed attempt, and the loss loop retries without bound;
    // [CHOSEN] this mirror is rate-limited like noteResetFailure (hits 1..4, then powers of two).
    static unsigned int unknownHits = 0;
    ++unknownHits;
    if (unknownHits <= 4u || (unknownHits & (unknownHits - 1u)) == 0u)
        ::renddx9abi::logf("RendDX9Init.cpp:401 ...unknown error resetting D3D device. hr=0x%08X "
                           "(hit #%u)", static_cast<unsigned int>(hr), unknownHits);
    return false;
}


// Resets a device after notifying its listeners that resources were lost.
// Returns false while the device is lost or reports an unexpected cooperative state.
// A ready device is also reset. Failed resets retry until success or device loss;
// listeners must release all default-pool resources before reset can succeed.

bool handleLostDevice(void* renderer)
{
    RendDX9Base* const b = baseOf(renderer);
    DeviceLostResetList& list = b->deviceLostList;

    bool lostSent  = false;     // bl
    bool resetDone = false;     // [ebp-1]

    HRESULT hr = b->device->TestCooperativeLevel();
    if (hr == D3DERR_DEVICELOST)
        return false;

    for (;;)
    {
        if (hr != D3DERR_DEVICENOTRESET)
        {
            if (hr != D3D_OK)
                return false;
            if (resetDone)
                return true;
        }

        if (!lostSent)
        {
            listBroadcastLost(list);
            lostSent = true;
        }

        if (resetDevice(renderer))
        {
            listBroadcastReset(list);
            lostSent  = false;
            resetDone = true;
        }

        hr = b->device->TestCooperativeLevel();
        if (hr == D3DERR_DEVICELOST)
            return false;
    }
}


// Changes the presentation interval and resets the device only when vsync changes.
// Device-reset failure is not propagated by this console setter.
void rendererSetVSync(void* renderer, bool enabled)
{
    RendDX9Base* const b = baseOf(renderer);
    const unsigned char value = enabled ? 1u : 0u;
    if (b->vsync == value)
        return;
    b->vsync = value;
    b->pp.PresentationInterval = value ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    handleLostDevice(renderer);
}

// (7 B): `mov al, [ecx+0CCh]; ret`.
bool rendererGetVSync(const void* renderer)
{
    return reinterpret_cast<const RendDX9Base*>(renderer)->vsync != 0;
}

// store the byte, nothing else - no reset; present picks it up on the
// next mode-2 present.
void rendererSetPresentAsync(void* renderer, bool enabled)
{
    baseOf(renderer)->presentAsync = enabled ? 1u : 0u;
}

// (7 B): `mov al, [ecx+3ACh]; ret`.
bool rendererGetPresentAsync(const void* renderer)
{
    return reinterpret_cast<const RendDX9Base*>(renderer)->presentAsync != 0;
}

// store the byte.
void rendererSetPresentSpinIfBusy(void* renderer, bool enabled)
{
    baseOf(renderer)->presentSpinIfBusy = enabled ? 1u : 0u;
}

// (7 B): `mov al, [ecx+3ADh]; ret`.
bool rendererGetPresentSpinIfBusy(const void* renderer)
{
    return reinterpret_cast<const RendDX9Base*>(renderer)->presentSpinIfBusy != 0;
}


// Display-mode probes run after reset and require valid render targets and readback resources.
// Allocation and most D3D results are unchecked.

namespace {

// the probe's vertex declaration: one FLOAT3 position in stream 0, then D3DDECL_END
// ({0xFF, 0, unused, 0, 0, 0}). Handed to the device's CreateVertexDeclaration as it stands.
const D3DVERTEXELEMENT9 kProbeDeclaration[2] =
{
    { 0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
};

// The four shader texts, byte for byte (the tabs are the source's own indentation). A position
// pass-through vertex shader in both; the multisample probe's pixel shader writes opaque white, the
// transparency-AA probe's writes white at alpha 0.5.
const char kMultisampleProbeVs[] =
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tvs.1.1\t\t\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\tdcl_position0 v0\t\n"
    "\t\t\t\t\t\t\t\t\t\t\tmov oPos, v0\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\t";
const char kMultisampleProbePs[] =
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tps.1.1\t\t\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\tdef c0, 1, 1, 1, 0\t\n"
    "\t\t\t\t\t\t\t\t\t\t\tmov r0, c0\t\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\t";
const char kTransparencyProbeVs[] =
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tvs.1.1\t\t\t\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\t\tdcl_position0 v0\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\t\tmov oPos, v0\t\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\t\t";
const char kTransparencyProbePs[] =
    "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\tps.1.1\t\t\t\t\t\n"
    "\t\t\t\t\t\t\t\t\t\t\t\tdef c0, 1,1,1,0.5\t\t\n"
    "  \t\t\t\t\t\t\t\t\t\t\tmov r0, c0\t\t\t\t\n"
    "  \t\t\t\t\t\t\t\t\t\t\t";
static_assert(sizeof(kMultisampleProbeVs) - 1 == 93,  "0x101DD530 is 93 bytes");
static_assert(sizeof(kMultisampleProbePs) - 1 == 94,  "0x101DD590 is 94 bytes");
static_assert(sizeof(kTransparencyProbeVs) - 1 == 101, "0x101DD418 is 101 bytes");
static_assert(sizeof(kTransparencyProbePs) - 1 == 103, "0x101DD480 is 103 bytes");

// A shipped probe store of a 4-byte code: `mov dword ptr [0x1023D090], imm32` and friends.
inline void storeTransparencyAACode(unsigned char (&code)[4], unsigned int value)
{
    ::memcpy(code, &value, sizeof(value));
}

inline void releaseIfSet(IUnknown* object)
{
    if (object)
        object->Release();
}

} // anonymous namespace


// Draws a test triangle and reports a nonblack pixel outside the inclusive colour range.
// rect limits readback scanning; dumpFile optionally saves the surface. Assembly errors
// fail without cleanup, and shader bindings remain in the state-manager mirror afterward.

bool rendererRenderProbe(void* renderer, const char* vertexShader, const char* pixelShader,
                         unsigned int colourLow, unsigned int colourHigh, const float* rect,
                         const char* dumpFile)
{
    RendDX9Base* const b = baseOf(renderer);

    ID3DXBuffer* vsBuffer = 0;                                          // [ebp-8]
    ID3DXBuffer* vsErrors = 0;                                          // [ebp-0Ch]
    D3DXAssembleShader(vertexShader, static_cast<unsigned int>(::strlen(vertexShader)),
                       0, 0, 0, &vsBuffer, &vsErrors);
    if (vsErrors)
    {
        vsErrors->GetBufferPointer();                                   // 0x1000A08C, discarded
        return false;                                                   // leaks, as shipped
    }

    IDirect3DVertexShader9* vs = 0;                                     // [ebp-10h]
    b->device->CreateVertexShader(static_cast<const DWORD*>(vsBuffer->GetBufferPointer()),
                                  &vs);                                 // +0x16C,

    ID3DXBuffer* psBuffer = 0;                                          // [ebp-14h]
    ID3DXBuffer* psErrors = 0;                                          // [ebp-18h]
    D3DXAssembleShader(pixelShader, static_cast<unsigned int>(::strlen(pixelShader)),
                       0, 0, 0, &psBuffer, &psErrors);
    if (psErrors)
    {
        psErrors->GetBufferPointer();                                   // 0x1000A08C, discarded
        return false;                                                   // leaks, as shipped
    }

    IDirect3DPixelShader9* ps = 0;                                      // [ebp-1Ch]
    b->device->CreatePixelShader(static_cast<const DWORD*>(psBuffer->GetBufferPointer()),
                                 &ps);                                  // +0x1A8,

    IDirect3DVertexDeclaration9* declaration = 0;                       // [ebp-20h]
    b->device->CreateVertexDeclaration(kProbeDeclaration, &declaration);  // +0x158,

    // three FLOAT3 positions, 12-byte stride.
    const float triangle[9] =
    {
        -1.0f,  1.0f, 0.5f,
        -1.0f, -1.0f, 0.5f,
         1.0f, -1.0f, 0.5f
    };

    b->device->BeginScene();                                            // +0xA4,
    stateManagerPush();
    stateManagerSetRenderState(D3DRS_ZENABLE, 0);                       // SM +0x1C (7, 0)
    stateManagerSetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);           // SM +0x1C (0x16, 1)
    stateManagerSetVertexShader(vs);                                    // SM +0x34
    stateManagerSetPixelShader(ps);                                     // SM +0x44
    stateManagerSetVertexDeclaration(declaration);                      // SM +0x5C
    stateManagerPreDrawApply();
    b->device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);   // +0xAC
    b->device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, triangle, 12);   // +0x14C
    stateManagerPop();
    b->device->EndScene();                                              // +0xA8,

    // 0x1000A1C0..0x1000A1D1 - `mov eax, dword_1023D098; mov ecx, [eax+774h]` -> sub_10014950.
    IDirect3DSurface9* const target =
        textureGetLevelSurface(rendererGetHemiScratchTexture(), 0, 0x7FFFFFFFu);      // edi
    // the source is the Renderer's back-buffer reference, also read
    // through dword_1023D098 (the Renderer's reset listener fills +0x76C with GetBackBuffer).
    IDirect3DSurface9* const backBuffer = rendererBackBufferSurface();             // +0x76C
    b->device->StretchRect(backBuffer, 0, target, 0, D3DTEXF_NONE);     // +0x88

    D3DSURFACE_DESC desc;                                               // [ebp-4Ch]
    target->GetDesc(&desc);                                             // +0x30
    IDirect3DSurface9* readback = 0;                                    // [ebp-4]
    b->device->CreateOffscreenPlainSurface(desc.Width, desc.Height, D3DFMT_A8R8G8B8,
                                           D3DPOOL_SYSTEMMEM, &readback, 0);   // +0x90
    b->device->GetRenderTargetData(target, readback);                   // +0x80
    D3DLOCKED_RECT locked;                                              // [ebp-2Ch]
    readback->LockRect(&locked, 0, D3DLOCK_READONLY);                   // +0x34

    // the scan. The four rect corners are converted with the rounding
    // mode forced to truncate (`or ah, 0Ch` around each fistp), i.e. a C cast.
    const unsigned int low  = colourLow  & 0xFFFFFFu;
    const unsigned int high = colourHigh & 0xFFFFFFu;
    unsigned char found     = 0;                                        // [ebp+0Bh]
    int firstColumn = 0;                                                // [ebp+10h]
    int row         = 0;
    int lastColumn  = static_cast<int>(desc.Width);                     // ebx
    int lastRow     = static_cast<int>(desc.Height);                    // [ebp+14h]
    if (rect)
    {
        firstColumn = static_cast<int>(rect[0]);
        row         = static_cast<int>(rect[1]);
        lastColumn  = static_cast<int>(rect[2]);
        lastRow     = static_cast<int>(rect[3]);
    }
    if (row < lastRow)
    {
        const unsigned char* line =
            static_cast<const unsigned char*>(locked.pBits) + locked.Pitch * row;   // imul, add
        for (; row < lastRow; ++row, line += locked.Pitch)
        {
            if (found)
                break;
            for (int column = firstColumn; column < lastColumn; ++column)
            {
                if (found)
                    break;
                const unsigned int pixel =
                    reinterpret_cast<const unsigned int*>(line)[column] & 0xFFFFFFu;
                if (pixel != 0 && (pixel < low || pixel > high))       // jb / jbe - unsigned
                    found = 1;
            }
        }
    }

    readback->UnlockRect();                                             // +0x38
    if (dumpFile)
        D3DXSaveSurfaceToFileA(dumpFile, D3DXIFF_DDS, readback, 0, 0);

    target->Release();                                                  // unguarded
    releaseIfSet(vsBuffer);
    releaseIfSet(vsErrors);
    releaseIfSet(vs);
    releaseIfSet(psBuffer);
    releaseIfSet(psErrors);
    releaseIfSet(ps);
    releaseIfSet(declaration);
    releaseIfSet(readback);
    return found != 0;
}


// Detects transparency AA using the atoc format check or a rendered alpha test.
// Requires the primary viewport and a secondary view. Failed rendered probes clear the
// support flag but retain the prior vendor state codes.

void rendererProbeTransparencyAA(void* renderer)
{
    RendDX9Base* const b = baseOf(renderer);
    if (!b->renderView2)
        return;

    const HRESULT hr = b->d3d9->CheckDeviceFormat(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0,
                                                  D3DRTYPE_SURFACE,
                                                  static_cast<D3DFORMAT>(0x434F5441u));  // +0x28
    if (hr == D3D_OK)                                                   // `test eax, eax; jne`
    {
        g_transparencyAAState = 0xB5u;
        storeTransparencyAACode(g_transparencyAAOn, 0x434F5441u);       // 'atoc'
        storeTransparencyAACode(g_transparencyAAOff, static_cast<unsigned int>(hr));   // eax = 0
        b->hasAlphaToCoverage = 1;                                      // bl = 1
        return;
    }

    // the viewport through the first render view's +0x14 (getViewport),
    // the products stored as floats (0x101DD4F0 = 0.5, 0x101DD4EC = 0.25, 0x101DD4E8 = 0.75).
    const float* const viewport = static_cast<IRenderView*>(b->renderView)->getViewport();
    float rect[4];
    rect[0] = 1.0f;                                                     // [ebp-10h]
    rect[1] = viewport[3] * 0.5f;                                       // [ebp-0Ch]
    rect[2] = viewport[2] * 0.25f;                                      // [ebp-8]
    rect[3] = viewport[3] * 0.75f;                                      // [ebp-4]

    stateManagerPush();
    stateManagerSetRenderState(D3DRS_MULTISAMPLEANTIALIAS, FALSE);      // SM +0x1C (0xA1, 0)
    unsigned int onCode;
    ::memcpy(&onCode, g_transparencyAAOn, sizeof(onCode));              // dword [0x1023D090]
    stateManagerSetRenderState(g_transparencyAAState, onCode);          // dword [0x1023D08C]
    const bool found = rendererRenderProbe(renderer, kTransparencyProbeVs, kTransparencyProbePs,
                                           0xFFFFFFFFu, 0xFFFFFFFFu, rect, 0);
    stateManagerPop();

    if (found)
    {
        g_transparencyAAState = 0x9Au;
        storeTransparencyAACode(g_transparencyAAOn, 0x314D3241u);       // 'A2M1'
        storeTransparencyAACode(g_transparencyAAOff, 0x304D3241u);      // 'A2M0'
    }
    b->hasAlphaToCoverage = found ? 1 : 0;
}


// Compares requested multisampling with rendered edge blending.
// Force/disable flags only become set and survive later probes. Requires a first mode descriptor.

void rendererProbeMultisample(void* renderer)
{
    RendDX9Base* const b = baseOf(renderer);
    if (!b->renderView2)
        return;

    stateManagerPush();
    stateManagerSetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);       // SM +0x1C (0xA1, 1)
    const bool found = rendererRenderProbe(renderer, kMultisampleProbeVs, kMultisampleProbePs,
                                           0xFFFBFBFBu, 0xFFFFFFFFu, 0, 0);
    stateManagerPop();

    // Unguarded, as shipped: the list's first type is read even when it holds no storage.
    const bool requested = *reinterpret_cast<const int*>(b->multiSamples.data()) > 0;
    if (!requested)
    {
        if (found)
            b->forceMultisample = 1;
    }
    else if (!found)
    {
        b->disableMultisample = 1;
    }
}


// Constructs device-facing renderer state after VideoOptions initialization.
// [FRONTIER] Remaining base initialization, including renderer-event registration, is incomplete.

void constructD3DHalf(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    // [CHOSEN, not recovered] The base storage's std containers (issue #7) are constructed first,
    // the way a C++ constructor builds its members before its body; destructD3DHalf destroys them.
    // The embedded VideoOptions builds its own below.
    new (&b->modeList) std::vector<Mode40>();
    new (&b->depthFormats) std::vector<unsigned int>();
    new (&b->multiSamples) std::vector<MultiSample8>();
    new (&b->windows) std::vector<void*>();
    new (&b->swapChains) std::vector<IDirect3DSwapChain9*>();
    new (&b->adapterInfo) std::vector<D3DADAPTER_IDENTIFIER9>();

    // DeviceLostResetListenerList_ctor(this + 92), PE 0x10002490.
    deviceLostResetListCtor(&b->deviceLostList);

    // RenderPathListenerList_ctor(this + 108). Built here because Renderer::initialize step 5
    // pushes the GSRenderPath setting into it.
    renderPathListCtor(&b->renderPathList);

    // two of the base constructor's scalar stores the frame bracket and the
    // device-loss core read (the Renderer constructor has already zeroed the whole base storage, so
    // only the NON-zero stores need repeating; the zero ones - +0x100, +0x3AC, +0x3AD, +0x3E8 - are
    // 0x1000B9F6 / 0x1000BA43 / 0x1000BA49 / 0x1000BA6C and already hold):
    //   0x1000B9FC  mov byte ptr [esi+101h], 1   - "the frame has been presented": the first
    //               beginScene therefore does NOT prime a present of an empty back buffer, and
    //               present() answers false until an endScene clears it.
    b->framePresented = 1;

    // 1000BA84 constructs the embedded VideoOptions before Direct3DCreate9 and
    // before initializeD3DSubsystems can fail. Hoisting its vptr into the later
    // initialize body left a null table on device failure at Renderer step15.
    videoOptionsConstruct(renderer);

    //   0x1000BAA1  mov byte ptr [esi+517h], 1   - the application-active byte CEActivateApp
    //               (ECCore 15) rewrites and Renderer +0xC0 tests. load-bearing: the Renderer's
    //               ECCore handler is registered in Renderer::initialize, typically after the
    //               window's first WM_ACTIVATEAPP, so without this store the byte would read
    //               "inactive" until the first alt-tab and Renderer +0xBC (reached from every
    //               cached Ra effect compile, 0x10035459) would spin posting CEPeakWinMessages
    //               forever.
    b->appActive = 1;

    // dword_1023B970 = this, at the instruction immediately before Direct3DCreate9,
    // and the first thing this constructor does. DX9RendModule::getSingletonList writes the same
    // global earlier with a null; this write lands second and is the one ~750 readers see. They
    // reach the D3D device through it as `dword_1023B970->[+0x0C]`.
    g_rendererBase = renderer;

    b->d3d9 = ::Direct3DCreate9(D3D_SDK_VERSION);   // the original passes 0x20 == D3D_SDK_VERSION
    ::renddx9abi::logf("RendDX9_base_ctor: Direct3DCreate9(0x20) -> 0x%08X",
                       ::renddx9abi::addr(b->d3d9));

    // Defaults read from the shipped body: GSDeviceType 1 (D3DDEVTYPE_HAL), GSDeviceBehavior 80.
    // 80 == 0x50 == D3DCREATE_HARDWARE_VERTEXPROCESSING (0x40) | D3DCREATE_PUREDEVICE (0x10),
    // which is exactly why the puredevice retry in createPrimaryD3DDevice exists.
    b->deviceType    = static_cast<unsigned int>(readIntSetting("GSDeviceType", 1));
    b->behaviorFlags = static_cast<unsigned int>(readIntSetting("GSDeviceBehavior", 80));
    ::renddx9abi::logf("RendDX9_base_ctor: GSDeviceType=%u GSDeviceBehavior=0x%02X",
                       b->deviceType, b->behaviorFlags);

    // The display mode, copied out of BF2's struct - sub_1000CA40 / sub_10009250. The lookup is the
    // base constructor's own (`getSingleton("hWndBf2TempSlt")` then straight into
    // initializeD3DSubsystems), so doing it here retires the hoist RW-54 recorded.
    const HWndBf2TempSlt* slot = 0;
    if (classManager)
    {
        // flags==4, so this is BF2's raw struct and NOT an IBase. No virtual may be called on it.
        slot = reinterpret_cast<const HWndBf2TempSlt*>(
                   classManager->getSingleton(renddx9abi::HostString("hWndBf2TempSlt")));
    }
    copyDisplayModeFrom(renderer, slot);

    // External loaders may lack BF2's display-mode singleton. Skip construction when
    // absent; normal BF2 plugin loading publishes it before this call.
    if (!slot)
    {
        ::renddx9abi::logf("RendDX9_base_ctor: no hWndBf2TempSlt - skipping D3D initialisation "
                           "entirely. This is not BF2; there is no display mode to build a device "
                           "from.");
        return;
    }

    initializeD3DSubsystems(renderer);   // result discarded, as shipped
}

// Runs the destructors of the std containers in the base storage, in reverse member order. The
// shipped base destructor (beyond its sub_1000C9B0 display-copy part, clearDisplayModeCopies)
// remains unrecovered; this only ends the containers' lifetimes before the object is freed.
void destructD3DHalf(void* renderer)
{
    typedef std::vector<Mode40>                  ModeList;
    typedef std::vector<unsigned int>            DepthFormatList;
    typedef std::vector<void*>                   WindowList;
    typedef std::vector<IDirect3DSwapChain9*>    SwapChainList;
    typedef std::vector<D3DADAPTER_IDENTIFIER9>  AdapterList;
    typedef std::vector<MultiSample8>            MultiSampleList;

    // Reverse declaration order.
    RendDX9Base* b = baseOf(renderer);
    b->videoOptions.~VideoOptions();
    b->adapterInfo.~AdapterList();
    b->swapChains.~SwapChainList();
    b->windows.~WindowList();
    b->multiSamples.~MultiSampleList();
    b->depthFormats.~DepthFormatList();
    b->modeList.~ModeList();
    b->renderPathList.~RenderPathList();
    b->deviceLostList.~DeviceLostResetList();
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
