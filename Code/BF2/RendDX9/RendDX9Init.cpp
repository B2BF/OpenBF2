// D3D device/subsystem initialization, swap chains, reset handling and display-mode probes.

#ifdef BF2_IMAGE_RENDDX9

#if defined(__APPLE__)
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"
#include "Ra/RaShader.h"
#include "Texture.h"                   // textureGetLevelSurface (sub_10014950)
#include "../RendMisc/IRenderView.h"   // the transparency-AA probe's getViewport
#include "../Game/VideoSettings.h"     // IVideoSettings - initializeD3DSubsystems' options handoff
#if defined(__APPLE__)
#include "../Platform/Gfx/Metal/RefTrace.h" // [TEMP] [RETIRE-WHEN: rend-deinit-reftrace]
#endif
#include "../Core/SettingsRepostitory.h"
#include "../GeomMisc/Debug.h"
#include "../Core/EventManager.h"   // the renderer events the base registers and unregisters

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include "../../RendDX9Abi/D3dx9Sdk.h"
#if defined(_WIN32)
#include <dxerr9.h>
#include <dxdiag.h>
#include <oleauto.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "dxerr9.lib")
#endif
#include <string.h>                    // strlen / memcpy - the display-mode probes
#include <new>                         // the base storage's std containers, constructed in place
#include <vector>
#include <cstdio>
#include <cstdlib>

namespace dice {
namespace hfe {

// PluginEntry.cpp, the host "LoadStat" singleton. initDll fetches it but does not
// require it. The device-lost list's lost/reset broadcasts write its byte at +0x08 (see
// setLoadStatDeviceLostByte below).
class LoadStat;
extern LoadStat* loadStat;   // the type Core/LoadStat.h and ShaderManager.cpp use (MSVC mangles it)

namespace geom { void destroyLightmapGenerationSingleton(); }   // Geom/LightmapGeneration - sub_100A8770

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
void videoOptionsDestruct(void* base);
// The teardown owners RendDX9_shutdownD3DSubsystems calls, each in its own TU.
void destroyRenderEntryPool();   // RenderQueues.cpp - dword_1023DE08
void destroyRenderQueues();      // RenderQueues.cpp - sub_10041A50
namespace Ra { void raReleaseVertexDeclarations(); }   // Ra/RaVertexStreams.cpp - sub_1004A080
namespace Ra { void destroyEffectManager(); }           // Ra/EffectManager.cpp
bool shaderManagerShutdown();    // ShaderManager.cpp - sub_10022380
void destroyShaderManager();     // ShaderManager.cpp - sub_100248E0 + free
extern void* g_bufferManager;    // BufferManager.cpp - dword_1023B978
bool bufferManagerShutdown();    // sub_10011390
void destroyBufferManager();     // sub_10012800 + free
void releaseStateManager();      // StateManager.cpp - dword_1023B984 +0x08
bool textureManagerShutdown();   // TextureManager.cpp - sub_10017BF0
void destroyTextureManager();    // primary +0x14(1)
void destroyResourceManager();   // ResourceManager.cpp - dword_1023B988 +0x04(1)
void destroyQueryManager();      // QueryManager.cpp - sub_100044D0 + free
void rendererDeactivateRenderPath(void* renderer);   // sub_1000D100, below
extern void* g_rendererBase;   // dword_1023B970, defined with the other base globals below
// BF2Render.cpp, dword_1023D098 - the Renderer singleton. The render probe reads the Renderer's
// back-buffer reference (+0x76C) through it, as the shipped instruction does (0x1000A1D6).
extern IBase* g_Renderer;


// Reads and dereferences a host setting result across the string ABI boundary: the shape of
// every inline retail read (PE 0x1001092D..0x10010940: `mov ecx, [0x10244A90]`, the key string
// and the default's address pushed, `call [vtbl+0x10]`, `mov ecx, [eax]`).
// [CHOSEN, not recovered] memory-safety exception (docs/macos-port.md): retail tests neither the settings
// singleton (dword_10244A90) nor the answer; a null singleton faults at the vtable load. The native
// hosts reach these helpers before or without a settings object (tools, the vtable audit), so the
// null test stays and answers the caller's default. The answer is a reference and is read as-is.
int readIntSetting(const char* name, int defaultValue)
{
    if (!settings)
        return defaultValue;

    int          def   = defaultValue;
    renddx9abi::HostString key(name);
    return settings->intGet(key, def);
}

// +0x14, the setter. VideoOptions::apply writes GSSkipMipLevels and GSWindowSizeX/Y back through
// it. Unlike the +0x10 getter this returns nothing the callers use - they discard it.
// [CHOSEN, not recovered] the null test is readIntSetting's memory-safety exception.
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
// [CHOSEN, not recovered] the null test is readIntSetting's memory-safety exception.
bool readBoolSetting(const char* name, bool defaultValue)
{
    if (!settings)
        return defaultValue;

    bool          def = defaultValue;
    renddx9abi::HostString key(name);
    return settings->boolGet(key, def);
}

// +0x40, the string getter. Read from TextureManager_loadTextureFromFile's level-directory
// fallback (0x1001860C / 0x10018627): TWO string arguments - name, then a default the caller
// builds as "" - and the return value in EAX is a string the caller copy-constructs from, i.e. a
// reference to the stored value. Same pointer-return shape as the int/bool getters.
void readStringSetting(const char* name, std::string& out)
{
    readStringSettingDefault(name, "", out);
}

// [CHOSEN, not recovered] the null test is readIntSetting's memory-safety exception.
void readStringSettingDefault(const char* name, const char* defaultValue, std::string& out)
{
    out = defaultValue;
    if (!settings)
        return;

    renddx9abi::HostString key(name);
    renddx9abi::HostString def(defaultValue);
    out = settings->stringGet(key, def).c_str();
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

// Returns the pixel-shader major version (byte 1 of PixelShaderVersion, +0xCC). PE 0x100010D0.
// Memory-safety exception (docs/macos-port.md): retail reads through dword_1023B980 without a null test; the
// existing test stays, because the native ports reach callers before device creation.
int deviceCapsPixelShaderMajor()
{
    if (!g_deviceCaps)
        return 0;
    const unsigned int psVersion = deviceCaps()->caps.PixelShaderVersion;           // +0xCC
    return static_cast<int>((psVersion >> 8) & 0xFF);
}

// PE 0x100010B0 (`[ecx+0xC4] >> 8 & 0xFF`). Returns the vertex-shader major version, or zero when
// device caps are unavailable.
int deviceCapsVertexShaderMajor()
{
    if (!g_deviceCaps)
        return 0;
    const unsigned int vsVersion = deviceCaps()->caps.VertexShaderVersion;          // +0xC4
    return static_cast<int>((vsVersion >> 8) & 0xFF);
}

// The retail helper has no null guard; every caller reaches it only after the
// DeviceCaps object has been published.
// PE 0x100011B0.
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

// PE 0x10001460. Copy the published adapter's two driver-version words.
unsigned int* deviceCapsCopyDriverVersion(unsigned int* out)
{
    const DeviceCaps* const caps = deviceCaps();
    out[0] = caps->adapter.DriverVersion.LowPart;
    out[1] = static_cast<unsigned int>(caps->adapter.DriverVersion.HighPart);
    return out;
}

// CheckDeviceFormat(0, HAL, X8R8G8B8, rendertarget, texture, format).
// PE 0x100013D0.
bool deviceCapsSupportsTextureFormat(unsigned int format)
{
    IDirect3D9* const d3d = deviceCaps()->d3d9;                                       // +0x580
    return d3d->CheckDeviceFormat(
        0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_RENDERTARGET,
        D3DRTYPE_TEXTURE, static_cast<D3DFORMAT>(format)) == D3D_OK;
}

// permit viewport-sized targets for unrestricted npot hardware and for the
// conditional-npot class; otherwise Renderer uses its fixed 512/128 fallback sizes.
// PE 0x10001430.
bool deviceCapsSupportsFullSizeRenderTargets()
{
    const unsigned int textureCaps = deviceCaps()->caps.TextureCaps;                  // +0x3C
    return (textureCaps & D3DPTEXTURECAPS_POW2) == 0
        || (textureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) != 0;
}

// The dword at +0x584 (unknown584). Requires the published DeviceCaps object.
unsigned int deviceCapsUnknown584()
{
    return deviceCaps()->unknown584;
}

// Returns D3DCAPS9::MaxPrimitiveCount. Requires the published DeviceCaps object.
// PE 0x10001100.
unsigned int deviceCapsMaxPrimitiveCount()
{
    return deviceCaps()->caps.MaxPrimitiveCount;                                     // +0xB4
}

// Returns D3DCAPS9::MaxVertexIndex. Unreachable in 1.50: no code or data references.
// PE 0x10001110.
unsigned int deviceCapsMaxVertexIndex()
{
    return deviceCaps()->caps.MaxVertexIndex;                                        // +0xB8
}

// Returns D3DCAPS9::MaxTextureWidth. Requires the published DeviceCaps object.
// PE 0x10001160 (the reader Renderer's top-down capture size clamp calls).
unsigned int deviceCapsMaxTextureWidth()
{
    return deviceCaps()->caps.MaxTextureWidth;                                       // +0x58
}

// Checks a cube-texture format against the X8R8G8B8 adapter format with no usage flags. Unreachable
// in 1.50: no code or data references. Requires the published DeviceCaps object.
// PE 0x10001400.
bool deviceCapsSupportsCubeTextureFormat(unsigned int format)
{
    IDirect3D9* const d3d = deviceCaps()->d3d9;                                       // +0x580
    return d3d->CheckDeviceFormat(
        0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, 0,
        D3DRTYPE_CUBETEXTURE, static_cast<D3DFORMAT>(format)) == D3D_OK;
}

// The platform SDK's bounded string copy (strsafe StringCchCopyA, inlined by the compiler into its
// only caller below): E_INVALIDARG (0x80070057) for an empty buffer, STRSAFE_E_INSUFFICIENT_BUFFER
// (0x8007007A) after truncating, S_OK otherwise.
// PE 0x10001030.
long displayDriverStringCopy(char* destination, unsigned int capacity, const char* source)
{
    if (capacity == 0)
        return static_cast<long>(0x80070057u);
    long result = 0;
    for (;;)
    {
        const char c = *source;
        if (!c)
            break;
        *destination++ = c;
        ++source;
        if (--capacity == 0)
        {
            --destination;
            result = static_cast<long>(0x8007007Au);
            break;
        }
    }
    *destination = 0;
    return result;
}

// One enumerated display driver: its GUID and a 32-byte name; a list holds ten and their count (+0x208).
struct DisplayDriverEntry { GUID guid; char name[32]; };
struct DisplayDriverList { DisplayDriverEntry entries[10]; unsigned int count; };

// Display-driver enumeration callback (the DDENUMCALLBACKEX shape: GUID, description, driver name,
// context, monitor). Appends the driver to the list and asks for more while fewer than ten are held.
// Shipped quirk: the GUID of the first entry is never copied. Unreachable in 1.50: no code or data
// references. [INFERRED] name and entry layout (52-byte stride, name at +0x10).
// PE 0x10001480.
int enumDisplayDriverCallback(const GUID* guid, const char* /*description*/, const char* driverName,
                              DisplayDriverList* list, void* /*monitor*/)
{
    if (list->count)
        list->entries[list->count].guid = *guid;
    if (::strlen(driverName) < 0x1F)
        displayDriverStringCopy(list->entries[list->count].name, 0x20, driverName);
    else
        list->entries[list->count].name[0] = 0;
    ++list->count;
    return list->count != 10;
}

// Copies the adapter's DeviceIdentifier GUID. Requires the published DeviceCaps object.
// PE 0x10001260 (returned through the caller's buffer).
GUID deviceCapsDeviceIdentifier()
{
    return deviceCaps()->adapter.DeviceIdentifier;                                   // +0x568
}

namespace {

// Maps a D3DDECLTYPE to its D3DDTCAPS bit: UBYTE4 (5) -> 0x1, UBYTE4N (8) .. FLOAT16_4 (16) ->
// 0x2 .. 0x200; every other type, SHORT2 and SHORT4 included, has no bit and maps to 0.
// PE 0x100011D0 (jump table 0x10001220).
unsigned int declTypeCapsBit(unsigned int declType)
{
    switch (declType)
    {
    case 5u:  return 0x001u;   // D3DDECLTYPE_UBYTE4    -> D3DDTCAPS_UBYTE4
    case 8u:  return 0x002u;   // D3DDECLTYPE_UBYTE4N   -> D3DDTCAPS_UBYTE4N
    case 9u:  return 0x004u;   // D3DDECLTYPE_SHORT2N   -> D3DDTCAPS_SHORT2N
    case 10u: return 0x008u;   // D3DDECLTYPE_SHORT4N   -> D3DDTCAPS_SHORT4N
    case 11u: return 0x010u;   // D3DDECLTYPE_USHORT2N  -> D3DDTCAPS_USHORT2N
    case 12u: return 0x020u;   // D3DDECLTYPE_USHORT4N  -> D3DDTCAPS_USHORT4N
    case 13u: return 0x040u;   // D3DDECLTYPE_UDEC3     -> D3DDTCAPS_UDEC3
    case 14u: return 0x080u;   // D3DDECLTYPE_DEC3N     -> D3DDTCAPS_DEC3N
    case 15u: return 0x100u;   // D3DDECLTYPE_FLOAT16_2 -> D3DDTCAPS_FLOAT16_2
    case 16u: return 0x200u;   // D3DDECLTYPE_FLOAT16_4 -> D3DDTCAPS_FLOAT16_4
    default:                    return 0u;
    }
}

} // anonymous namespace

// Returns whether the device supports a vertex-declaration type. Types without a D3DDTCAPS bit
// count as supported. Requires the published DeviceCaps object.
// PE 0x100013A0.
bool deviceCapsSupportsDeclType(unsigned int declType)
{
    const unsigned int bit = declTypeCapsBit(declType);
    if (bit == 0u)
        return true;
    return (deviceCaps()->caps.DeclTypes & bit) != 0u;                               // +0xEC
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
}

const char* initDeviceErrorDescription(HRESULT result)
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

void initDeviceError(int line, const char* message)
{
    ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\RendDX9Init.cpp",
        line, "") << message;
}

void initDeviceError(int line, const char* message, HRESULT result)
{
    ::renddx9abi::Debug diagnostic(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\RendDX9Init.cpp",
        line, "");
    const char* const description = initDeviceErrorDescription(result);
    diagnostic << message << description;
}

} // anonymous namespace


// Clears the renderer's deep copies of host display-mode ranges.
// PE 0x1000C9B0: frees and zeroes the window, multisample and depth-format vectors, in that order.
void clearDisplayModeCopies(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    std::vector<decltype(b->windows)::value_type>().swap(b->windows);
    clearMultiSampleList(b);
    std::vector<decltype(b->depthFormats)::value_type>().swap(b->depthFormats);
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

// Sorts ascending by priority, retaining registration order for ties. The three broadcasts inline
// the same body. Retail calls std::stable_sort over the list's iterator range (0x10003140; VC7.1
// accepts bidirectional iterators there). [CHOSEN, not recovered] std::list::sort, the stable sort
// every current library accepts for list iterators: for a strict weak order both produce the same
// unique stable order.
// The outlined copy is unreachable in 1.50: no code or data references.
// PE 0x10002500
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

// PE 0x100024B0, native list-entry spelling.
void registerDeviceLostResetListener(DeviceLostResetListener* listener, int priority)
{
    DeviceLostResetList& list = deviceLostResetList();
    // PE 0x100024B0 appends and marks the ordering dirty.
    const DeviceLostResetEntry entry = { priority, listener };
    list.entries.push_back(entry);
    list.dirty = 1;
}

// PE 0x10002410. Remove only the first matching listener; leave the dirty flag alone.
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
// (0x1000ADB0, at 0x1000AE98).
void notifyDeviceLostResetListenersDestroy()
{
    DeviceLostResetList& list = deviceLostResetList();
    sortDeviceLostResetList(list);
    for (DeviceLostResetEntries::reverse_iterator it = list.entries.rbegin();
         it != list.entries.rend(); ++it)
        it->listener->onDeviceDestroy();
}

// PE 0x1000D140 (listing read); the sentinel node is the std::list default constructor (0x1000CF10).
void renderPathListCtor(void* list)
{
    new (list) RenderPathList();
}

// PE 0x1000D080. List construction is the caller's precondition.
void setActiveRenderPath(RenderPathList& list, int path)
{
    for (std::list<RenderPathListener*>::iterator it = list.listeners.begin();
         it != list.listeners.end(); ++it)
        (*it)->onRenderPathActivated(path);

    list.activeRenderPath = path;
    list.changed          = 1;
}

// Stores a new active path and notifies every listener's slot +0x08 in registration order; an
// unchanged path notifies nobody. Unlike setActiveRenderPath it leaves the changed byte alone.
// [INFERRED] name.
// unreachable in 1.50: no code or data references
// PE 0x1000D0C0
void changeActiveRenderPath(RenderPathList& list, int path)
{
    if (path == list.activeRenderPath)
        return;
    list.activeRenderPath = path;
    for (std::list<RenderPathListener*>::iterator it = list.listeners.begin();
         it != list.listeners.end(); ++it)
        (*it)->onRenderPathChanged(path);
}

// PE 0x1000D100. Notify in reverse registration order, then clear only the changed byte.
void rendererDeactivateRenderPath(void* renderer)
{
    RenderPathList& list = baseOf(renderer)->renderPathList;
    for (std::list<RenderPathListener*>::reverse_iterator it = list.listeners.rbegin();
         it != list.listeners.rend(); ++it)
        (*it)->onRenderPathDeactivated();
    list.changed = 0;
}

// PE 0x1000D170 (also 0x10009D80).  Insert immediately before the circular sentinel; if a render path
// has already been selected, notify slot +0 now.  The public registration wrapper then invokes
// listener slot +0x08 with the Renderer listener's current-path getter (+0x10), even though both
// shipped Renderer and LightManager implementations make that +0x08 callback a nullsub.
void registerRenderPathListener(RenderPathListener* listener)
{
    RenderPathList& list = static_cast<RendDX9Base*>(g_rendererBase)->renderPathList;
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

// dword 0x10239664: a float that starts at 100000.0f and is only ever read by its adjacent reader (PE
// 0x10051990) and written by rendererSetUnreadFloat; neither has a caller in 1.50. [INFERRED] name.
float g_unreadRendererFloat = 100000.0f;

// PE 0x100519A0. Unreachable in 1.50: no code or data references. Stores the float.
void rendererSetUnreadFloat(float value)
{
    g_unreadRendererFloat = value;
}

// PE 0x10009D10. Unreachable in 1.50: no code or data references. Creates an A8R8G8B8 texture
// (one level, usage 1) of the present parameters' back-buffer size with the manager's managed flag
// cleared for the creation. [INFERRED] name.
void* rendererCreateBackBufferSizedTexture(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);
    const int savedManaged = textureManagerSwapManaged(0);
    void* const texture = textureManagerCreateEmpty(b->pp.BackBufferWidth, b->pp.BackBufferHeight,
                                                    D3DFMT_A8R8G8B8, 1u, 1u, 0);
    textureManagerSwapManaged(savedManaged);
    return texture;
}

// PE 0x10009FA0. Unreachable in 1.50: no code or data references. Stores three words at +0x10 and
// the opaque colour built from their low bytes (red, green, blue) at +0x1C, where the frame-time
// overlay reads it. [INFERRED] name.
void rendererSetDebugTextColour(void* renderer, const int* rgb)
{
    RendDX9Base* b = baseOf(renderer);
    b->debugTextRgb[0] = static_cast<unsigned int>(rgb[0]);
    b->debugTextRgb[1] = static_cast<unsigned int>(rgb[1]);
    b->debugTextRgb[2] = static_cast<unsigned int>(rgb[2]);
    b->debugTextColour = ((((static_cast<unsigned int>(rgb[0]) | 0xFFFFFF00u) << 8)
                           | (static_cast<unsigned int>(rgb[1]) & 0xFFu)) << 8)
                         | (static_cast<unsigned int>(rgb[2]) & 0xFFu);
}

#if defined(_WIN32)
// A window procedure that hands every message to the default handler (a real frame, then a tail
// jump to DefWindowProcA). [INFERRED] name and placement; the _r image has the same body
// (0x1002ACA0), reached there only from an unused incremental-link jump. Windows only: no native
// window class uses it.
// unreachable in 1.50: no code or data references
// PE 0x10009A10
LRESULT CALLBACK rendererDefaultWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    return ::DefWindowProcA(window, message, wParam, lParam);
}
#endif

// PE 0x10009A30. Unreachable in 1.50: no code or data references. The first argument arrives in
// eax and the second on the stack. [INFERRED] meaning: a version of at least 9.99, true above major 9
// and, at major 9, when the signed minor byte is 99 or more.
bool versionAtLeast9_99(unsigned int major, signed char minor)
{
    if (major < 9u)
        return false;
    if (major > 9u)
        return true;
    return minor >= 0x63;
}

// RendererBase +0x518 is the performance-feature bitset.  The original performs no
// range check; all recovered callers pass a literal in [0, 20].
// PE 0x10009F70 (listing read: shl/and/cmp against +0x518).
bool rendererPerformanceFlag(unsigned int bit)
{
    const unsigned int flags = static_cast<const RendDX9Base*>(g_rendererBase)->performanceFlags;
    const unsigned int mask = 1u << bit;
    return (flags & mask) == mask;
}


// [CHOSEN, not recovered] The first depth candidate. PE 0x1000A52C dereferences the first element of
// the vector without a guard; the native ports keep the fatal empty-list precondition with a
// diagnostic instead of reading past the vector.
static D3DFORMAT firstDepthCandidate(RendDX9Base* b)
{
#if !defined(_WIN32)
    if (b->depthFormats.empty())
    {
        std::fprintf(stderr, "[CHOSEN, not recovered] dice::hfe::dx9rend::buildPresentParameters: "
                             "fatal empty depth-candidate list (retail unchecked dereference)\n");
        std::fflush(stderr);
        std::abort();
    }
#endif
    return static_cast<D3DFORMAT>(b->depthFormats[0]);
}

// PE 0x1000A500. Builds D3D presentation parameters. The depth-candidate vector must contain an entry;
// the multisample vector may be empty and is checked separately.
void buildPresentParameters(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    ::memset(&b->pp, 0, sizeof(b->pp));

    b->pp.Flags                  = 2;   // D3DPRESENTFLAG_DISCARD_DEPTHSTENCIL
    b->pp.EnableAutoDepthStencil = TRUE;

#if defined(__APPLE__)
    // [CHOSEN, not recovered] G3 native policy: the initial host carrier has no
    // depth candidates yet. Select only the backend's explicitly supported D24S8.
    // Other images require the caller to supply its depth candidates.
    if (b->depthFormats.empty() && b->d3d9 &&
        SUCCEEDED(b->d3d9->CheckDeviceFormat(b->adapter, static_cast<D3DDEVTYPE>(b->deviceType),
            D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8)))
        b->depthFormats.push_back(D3DFMT_D24S8);
#endif

    b->pp.AutoDepthStencilFormat = firstDepthCandidate(b);

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

    // PE 0x1000CD20 (inlined): D3DFMT_A8R8G8B8 (21) at 32bpp, D3DFMT_R5G6B5
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
    b->pp.hDeviceWindow = reinterpret_cast<HWND>(b->windows[0]);

    ::renddx9abi::logf("presentParameters: %ux%u fmt=%d depth=%d windowed=%d refresh=%u "
                       "interval=0x%08X hWnd=0x%08X",
                       b->pp.BackBufferWidth, b->pp.BackBufferHeight,
                       static_cast<int>(b->pp.BackBufferFormat),
                       static_cast<int>(b->pp.AutoDepthStencilFormat),
                       b->pp.Windowed, b->pp.FullScreen_RefreshRateInHz,
                       b->pp.PresentationInterval,
                       ::renddx9abi::addr(b->pp.hDeviceWindow));
}


// Creates the fullscreen device, retrying only D3DERR_NOTAVAILABLE without PUREDEVICE.
// PE 0x1000A780.
bool createPrimaryD3DDevice(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    // Keep the window-info query even though the result is unused.
#if !defined(__APPLE__)
    // shipped defect: cbSize is never initialized before GetWindowInfo.
    WINDOWINFO pwi;
    ::GetWindowInfo(reinterpret_cast<HWND>(b->windows[0]), &pwi);
#else
    // [CHOSEN, not recovered] Cocoa has no Win32 window-info query; no result is consumed.
#endif

    HRESULT hr = b->d3d9->CreateDevice(b->adapter,
                                       static_cast<D3DDEVTYPE>(b->deviceType),
                                       reinterpret_cast<HWND>(b->windows[0]),
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
            initDeviceError(485, "Failed to create d3d device. Unknown cause. ", hr);
            return false;
        }

        hr = b->d3d9->CreateDevice(b->adapter,
                                   static_cast<D3DDEVTYPE>(b->deviceType),
                                   reinterpret_cast<HWND>(b->windows[0]),
                                   b->behaviorFlags & 0xFFFFFFEFu,
                                   &b->pp,
                                   &b->device);
        if (hr != D3D_OK)
        {
            initDeviceError(494, "Failed to create d3d device. Unknown cause. ", hr);
            return false;
        }
    }

    // GetSwapChain is IDirect3DDevice9 vtable +56 (slot 14). The original checks `>= 0`, i.e.
    // succeeded, not `== D3D_OK`.
    IDirect3DSwapChain9* swapChain = 0;
    if (FAILED(b->device->GetSwapChain(0, &swapChain)))
    {
        initDeviceError(504, "Failed to get swap chain 0 ptr.");
        return false;
    }

    // sub_1000C630(&swapChain) - push_back onto the swap-chain list (retail base+0xE0; the shipped
    // caller does `lea ecx,[esi+0E0h]`). Renderer::present indexes this list by currentSwapChain,
    // so leaving it empty is a null dereference the first time the menu presents - which is
    // exactly the crash a retail run hit.
    swapChainVectorPushBack(b, swapChain);
    g_primarySwapChain = swapChain;

    return true;
}


// Creates the primary device and swap chain, plus a chain for each extra window.
// Windowed mode uses the desktop back-buffer format and validates the depth/format pair.
// PE 0x1000A9C0.
bool createD3DDeviceAndSwapChains(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    // IDirect3D9::GetAdapterDisplayMode - vtable +32, slot 8.
    D3DDISPLAYMODE desktop;
    if (FAILED(b->d3d9->GetAdapterDisplayMode(0, &desktop)))
    {
        initDeviceError(517, "Failed to get desktop mode.");
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
            initDeviceError(526, "Failed to find valid format for windowed mode.");
        else
            initDeviceError(528, "Unknown error in d3d init.");
        return false;
    }

    // The same CreateDevice and the same single puredevice retry as the fullscreen path, with its
    // own pair of line numbers.
    hr = b->d3d9->CreateDevice(b->adapter,
                               static_cast<D3DDEVTYPE>(b->deviceType),
                               reinterpret_cast<HWND>(b->windows[0]),
                               b->behaviorFlags,
                               &b->pp,
                               &b->device);
    if (hr != D3D_OK)
    {
        if (hr != D3DERR_NOTAVAILABLE)
        {
            initDeviceError(538, "Failed to create d3d device. Unknown cause. ", hr);
            return false;
        }

        hr = b->d3d9->CreateDevice(b->adapter,
                                   static_cast<D3DDEVTYPE>(b->deviceType),
                                   reinterpret_cast<HWND>(b->windows[0]),
                                   b->behaviorFlags & 0xFFFFFFEFu,
                                   &b->pp,
                                   &b->device);
        if (hr != D3D_OK)
        {
            initDeviceError(547, "Failed to create d3d device. Unknown cause. ", hr);
            return false;
        }
    }

    // IDirect3DDevice9::GetSwapChain - vtable +56, slot 14.
    IDirect3DSwapChain9* swapChain0 = 0;
    if (FAILED(b->device->GetSwapChain(0, &swapChain0)))
    {
        initDeviceError(557, "Failed to get swap chain 0 ptr.");
        return false;
    }
    swapChainVectorPushBack(b, swapChain0);     // see createPrimaryD3DDevice's note
    g_primarySwapChain = swapChain0;

    // One additional swap chain per remaining window. i starts at 1 - swap chain 0 belongs to the
    // window the device was created against.
    for (unsigned int i = 1; i < static_cast<unsigned int>(b->windows.size()); ++i)
    {
        b->pp.hDeviceWindow = reinterpret_cast<HWND>(b->windows[i]);

        // IDirect3DDevice9::CreateAdditionalSwapChain - vtable +52, slot 13.
        IDirect3DSwapChain9* extra = 0;
        HRESULT chainHr = b->device->CreateAdditionalSwapChain(&b->pp, &extra);
        if (chainHr != D3D_OK)
        {
            initDeviceError(568, "Failed to create additional swap chains. ", chainHr);
            return false;
        }

        // sub_1000C630 again - push the extra chain onto the same list.
        swapChainVectorPushBack(b, extra);
    }

    return true;
}


// Classifies supported GPUs by vendor and device id; unknown devices return -1.
namespace {

// PE 0x10001520.
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

// PE 0x100017A0, allocated by 0x1000B23C. Adapter enumeration precedes construction.
bool createDeviceCaps(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    if (!b->d3d9)
        return false;

    DeviceCaps* dc = static_cast<DeviceCaps*>(::renddx9abi::engineAlloc(sizeof(DeviceCaps)));
    if (!dc)
        return false;

    // The constructor's own order: store the IDirect3D9, AddRef it, then fill the caps.
    dc->d3d9 = b->d3d9;
    b->d3d9->AddRef();

    // (*(vtbl+56))(d3d9, 0, base[33], this) - IDirect3D9::GetDeviceCaps(Adapter, DevType, pCaps),
    // adapter hard-coded to 0 and the device type taken from base+0x84. The caps land at `this+0`.
    b->d3d9->GetDeviceCaps(0, static_cast<D3DDEVTYPE>(b->deviceType), &dc->caps);

    g_deviceCaps = dc;

    // The first identifier is copied unguarded; a missing adapter faults in the shipped body.
    dc->adapter = b->adapterInfo[0];

    dc->gpuFamily  = classifyGpu(dc->adapter);
    if (dc->adapter.VendorId == 0x10DEu)
    {
        IDirect3DSurface9* surface = 0;
        const HRESULT hr = b->device->CreateOffscreenPlainSurface(
            1, 1, static_cast<D3DFORMAT>(0x4642564Eu), D3DPOOL_DEFAULT, &surface, 0);
        if (hr == 0 && surface)
            surface->Release();
    }
    dc->unknown584 = b->opaque08C;

    ::renddx9abi::logf("createDeviceCaps: VendorId=0x%04X DeviceId=0x%04X \"%s\" -> gpuFamily=%d, "
                       "PS=%u.%u VS=%u.%u",
                       dc->adapter.VendorId, dc->adapter.DeviceId, dc->adapter.Description,
                       dc->gpuFamily,
                       (dc->caps.PixelShaderVersion >> 8) & 0xFF, dc->caps.PixelShaderVersion & 0xFF,
                       (dc->caps.VertexShaderVersion >> 8) & 0xFF, dc->caps.VertexShaderVersion & 0xFF);
    return true;
}


// Enumerates adapters, builds presentation parameters and selects the device-creation path.
// PE 0x1000B0D0.
bool createD3DDevice(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);

    b->adapter = 0;

    for (UINT i = 0; i < b->d3d9->GetAdapterCount(); ++i)
    {
        D3DADAPTER_IDENTIFIER9 identifier;
        b->d3d9->GetAdapterIdentifier(i, 0, &identifier);
        b->adapterInfo.push_back(identifier);
    }

    buildPresentParameters(renderer);

    if (b->fullscreen)
        return createPrimaryD3DDevice(renderer);

    // Windowed mode validates against the desktop and may create additional swap chains.
    return createD3DDeviceAndSwapChains(renderer);
}


#if defined(_WIN32)
namespace {

// [INFERRED] Helper names and placement beside their subsystem-initialization caller.
// [INFERRED] Record and member names follow the queried DxDiag properties. The POD groups
// bracket the retail record's unused vector so native STL storage never uses retail offsets.
struct DxDiagDisplayHeader
{
    char szDeviceName[100];
    char szDescription[200];
    char szKeyDeviceID[200];
    char szKeyDeviceKey[200];
    char szManufacturer[200];
    char szChipType[100];
    char szDACType[100];
    char szRevision[100];
    char szDisplayMemoryLocalized[100];
    char szDisplayMemoryEnglish[100];
    char szDisplayModeLocalized[100];
    char szDisplayModeEnglish[100];
    DWORD dwWidth, dwHeight, dwBpp, dwRefreshRate;
    char szMonitorName[100];
    char szMonitorMaxRes[100];
    char szDriverName[100];
    char szDriverVersion[100];
    char szDriverAttributes[100];
    char szDriverLanguageEnglish[100];
    char szDriverLanguageLocalized[100];
    char szDriverDateEnglish[100];
    char szDriverDateLocalized[100];
    LONG lDriverSize;
    char szMiniVdd[100];
    char szMiniVddDateLocalized[100];
    char szMiniVddDateEnglish[100];
    LONG lMiniVddSize;
    char szVdd[100];
    BOOL bCanRenderWindow, bDriverBeta, bDriverDebug, bDriverSigned, bDriverSignedValid;
    DWORD dwDDIVersion;
    char szDDIVersionEnglish[100];
    char szDDIVersionLocalized[100];
    DWORD iAdapter;
    char szVendorId[50], szDeviceId[50], szSubSysId[50], szRevisionId[50];
    DWORD dwWHQLLevel;
    char szDeviceIdentifier[100];
    char szDriverSignDate[50];
    BOOL bNoHardware, bDDAccelerationEnabled, b3DAccelerationExists, b3DAccelerationEnabled;
    BOOL bAGPEnabled, bAGPExists, bAGPExistenceValid;
    char szDXVAModes[100];
};

struct DxDiagDisplayTail
{
    char szDDStatusLocalized[100], szDDStatusEnglish[100];
    char szD3DStatusLocalized[100], szD3DStatusEnglish[100];
    char szAGPStatusLocalized[100], szAGPStatusEnglish[100];
    char szNotesLocalized[3000], szNotesEnglish[3000], szRegHelpText[3000];
    char szTestResultDDLocalized[3000], szTestResultDDEnglish[3000];
    char szTestResultD3D7Localized[3000], szTestResultD3D7English[3000];
    char szTestResultD3D8Localized[3000], szTestResultD3D8English[3000];
    char szTestResultD3D9Localized[3000], szTestResultD3D9English[3000];
    // [CHOSEN, not recovered] Name for the four trailing bytes only the record's memset writes.
    unsigned char unused9184[4];
};

struct DxDiagDisplayInfo
{
    DxDiagDisplayInfo() : header(), tail() {}
    DxDiagDisplayHeader header;
    // +0xE34 in VC7.1. [UNRESOLVED] Element type: no instruction populates this vector.
    // [CHOSEN, not recovered] void* carries its empty native storage; the only operations are
    // zero construction and destruction at 0x10007239.
    std::vector<void*> unusedCaps;
    DxDiagDisplayTail tail;
};

// PE 0x10005D50. Requires a nonempty output buffer. Wrong-type and GetProp failure paths
// deliberately do not clear the variant.
HRESULT __fastcall readDxDiagString(IDxDiagContainer* container, LPCWSTR name,
                                   char* output, unsigned int bytes)
{
    VARIANT value;
    ::VariantInit(&value);
    const HRESULT result = container->GetProp(name, &value);
    if (FAILED(result)) return result;
    if (value.vt != VT_BSTR) return E_INVALIDARG;
    ::wcstombs(output, value.bstrVal, bytes);
    output[bytes - 1] = 0;
    ::VariantClear(&value);
    return S_OK;
}

// PE 0x10005DC0.
HRESULT __fastcall readDxDiagDword(IDxDiagContainer* container, LPCWSTR name, DWORD* output)
{
    VARIANT value;
    ::VariantInit(&value);
    const HRESULT result = container->GetProp(name, &value);
    if (FAILED(result)) return result;
    if (value.vt != VT_UI4) return E_INVALIDARG;
    *output = value.ulVal;
    ::VariantClear(&value);
    return S_OK;
}

// PE 0x10005E20.
HRESULT __fastcall readDxDiagLong(IDxDiagContainer* container, LPCWSTR name, LONG* output)
{
    VARIANT value;
    ::VariantInit(&value);
    const HRESULT result = container->GetProp(name, &value);
    if (FAILED(result)) return result;
    if (value.vt != VT_I4) return E_INVALIDARG;
    *output = value.lVal;
    ::VariantClear(&value);
    return S_OK;
}

// PE 0x10005E80.
HRESULT __fastcall readDxDiagBool(IDxDiagContainer* container, LPCWSTR name, BOOL* output)
{
    VARIANT value;
    ::VariantInit(&value);
    const HRESULT result = container->GetProp(name, &value);
    if (FAILED(result)) return result;
    if (value.vt != VT_BOOL) return E_INVALIDARG;
    *output = value.boolVal != 0;
    ::VariantClear(&value);
    return S_OK;
}

} // namespace

// The 64-bit reader beside the other four (a BSTR property parsed with _wtoi64). Like them it
// leaves the variant uncleared on the GetProp failure and wrong-type paths.
// unreachable in 1.50: no code or data references
// PE 0x10005EE0
HRESULT __fastcall readDxDiagUInt64(IDxDiagContainer* container, LPCWSTR name, ULONGLONG* output)
{
    VARIANT value;
    ::VariantInit(&value);
    const HRESULT result = container->GetProp(name, &value);
    if (FAILED(result)) return result;
    if (value.vt != VT_BSTR) return E_INVALIDARG;
    *output = static_cast<ULONGLONG>(::_wtoi64(value.bstrVal));
    ::VariantClear(&value);
    return S_OK;
}

namespace {

#define RD9_DXDIAG_WIDE_I(text) L##text
#define RD9_DXDIAG_WIDE(text) RD9_DXDIAG_WIDE_I(text)
#define RD9_DXDIAG_STRING(part, member) \
    do { result = readDxDiagString(child, RD9_DXDIAG_WIDE(#member), info->part.member, \
             static_cast<unsigned int>(sizeof(info->part.member))); \
         if (FAILED(result)) goto cleanup; } while (false)
#define RD9_DXDIAG_VALUE(kind, member) \
    do { result = readDxDiag##kind(child, RD9_DXDIAG_WIDE(#member), &info->header.member); \
         if (FAILED(result)) goto cleanup; } while (false)

// PE 0x10005F40. The receiver is unused; the caller prepares it in ECX nonetheless.
HRESULT RD9_THUNK collectDxDiagDisplays(void* RD9_THUNK_EDX, IDxDiagContainer* root,
                                        std::vector<DxDiagDisplayInfo*>& output)
{
    IDxDiagContainer* displays = 0;
    IDxDiagContainer* child = 0;
    DWORD count = 0;
    HRESULT result = root->GetChildContainer(L"DxDiag_DisplayDevices", &displays);
    if (FAILED(result)) goto cleanup;
    result = displays->GetNumberOfChildContainers(&count);
    if (FAILED(result)) goto cleanup;
    for (DWORD index = 0; index < count; ++index)
    {
        void* storage = ::renddx9abi::engineAlloc(
            static_cast<unsigned int>(sizeof(DxDiagDisplayInfo)));
        // shipped defect: allocation failure leaks the display container reference.
        if (!storage) return E_OUTOFMEMORY;
        DxDiagDisplayInfo* info = new (storage) DxDiagDisplayInfo;
        output.push_back(info);
        WCHAR name[256];
        result = displays->EnumChildContainerNames(index, name, 256);
        if (FAILED(result)) goto cleanup;
        result = displays->GetChildContainer(name, &child);
        if (!child) result = E_FAIL;
        if (FAILED(result)) goto cleanup;

        RD9_DXDIAG_STRING(header, szDeviceName);
        RD9_DXDIAG_STRING(header, szDescription);
        RD9_DXDIAG_STRING(header, szKeyDeviceID);
        RD9_DXDIAG_STRING(header, szKeyDeviceKey);
        RD9_DXDIAG_STRING(header, szManufacturer);
        RD9_DXDIAG_STRING(header, szChipType);
        RD9_DXDIAG_STRING(header, szDACType);
        RD9_DXDIAG_STRING(header, szRevision);
        RD9_DXDIAG_STRING(header, szDisplayMemoryLocalized);
        RD9_DXDIAG_STRING(header, szDisplayMemoryEnglish);
        RD9_DXDIAG_STRING(header, szDisplayModeLocalized);
        RD9_DXDIAG_STRING(header, szDisplayModeEnglish);
        RD9_DXDIAG_VALUE(Dword, dwWidth);
        RD9_DXDIAG_VALUE(Dword, dwHeight);
        RD9_DXDIAG_VALUE(Dword, dwBpp);
        RD9_DXDIAG_VALUE(Dword, dwRefreshRate);
        RD9_DXDIAG_STRING(header, szMonitorName);
        RD9_DXDIAG_STRING(header, szMonitorMaxRes);
        RD9_DXDIAG_STRING(header, szDriverName);
        RD9_DXDIAG_STRING(header, szDriverVersion);
        RD9_DXDIAG_STRING(header, szDriverAttributes);
        RD9_DXDIAG_STRING(header, szDriverLanguageEnglish);
        RD9_DXDIAG_STRING(header, szDriverLanguageLocalized);
        RD9_DXDIAG_STRING(header, szDriverDateEnglish);
        RD9_DXDIAG_STRING(header, szDriverDateLocalized);
        RD9_DXDIAG_VALUE(Long, lDriverSize);
        RD9_DXDIAG_STRING(header, szMiniVdd);
        RD9_DXDIAG_STRING(header, szMiniVddDateLocalized);
        RD9_DXDIAG_STRING(header, szMiniVddDateEnglish);
        RD9_DXDIAG_VALUE(Long, lMiniVddSize);
        RD9_DXDIAG_STRING(header, szVdd);
        RD9_DXDIAG_VALUE(Bool, bCanRenderWindow);
        RD9_DXDIAG_VALUE(Bool, bDriverBeta);
        RD9_DXDIAG_VALUE(Bool, bDriverDebug);
        RD9_DXDIAG_VALUE(Bool, bDriverSigned);
        RD9_DXDIAG_VALUE(Bool, bDriverSignedValid);
        RD9_DXDIAG_STRING(header, szDeviceIdentifier);
        RD9_DXDIAG_STRING(header, szDriverSignDate);
        RD9_DXDIAG_VALUE(Dword, dwDDIVersion);
        RD9_DXDIAG_STRING(header, szDDIVersionEnglish);
        RD9_DXDIAG_STRING(header, szDDIVersionLocalized);
        RD9_DXDIAG_VALUE(Dword, iAdapter);
        RD9_DXDIAG_STRING(header, szVendorId);
        RD9_DXDIAG_STRING(header, szDeviceId);
        RD9_DXDIAG_STRING(header, szSubSysId);
        RD9_DXDIAG_STRING(header, szRevisionId);
        RD9_DXDIAG_VALUE(Dword, dwWHQLLevel);
        RD9_DXDIAG_VALUE(Bool, bNoHardware);
        RD9_DXDIAG_VALUE(Bool, bDDAccelerationEnabled);
        RD9_DXDIAG_VALUE(Bool, b3DAccelerationExists);
        RD9_DXDIAG_VALUE(Bool, b3DAccelerationEnabled);
        RD9_DXDIAG_VALUE(Bool, bAGPEnabled);
        RD9_DXDIAG_VALUE(Bool, bAGPExists);
        RD9_DXDIAG_VALUE(Bool, bAGPExistenceValid);
        RD9_DXDIAG_STRING(header, szDXVAModes);
        RD9_DXDIAG_STRING(tail, szDDStatusLocalized);
        RD9_DXDIAG_STRING(tail, szDDStatusEnglish);
        RD9_DXDIAG_STRING(tail, szD3DStatusLocalized);
        RD9_DXDIAG_STRING(tail, szD3DStatusEnglish);
        RD9_DXDIAG_STRING(tail, szAGPStatusLocalized);
        RD9_DXDIAG_STRING(tail, szAGPStatusEnglish);
        RD9_DXDIAG_STRING(tail, szNotesLocalized);
        RD9_DXDIAG_STRING(tail, szNotesEnglish);
        RD9_DXDIAG_STRING(tail, szRegHelpText);
        RD9_DXDIAG_STRING(tail, szTestResultDDLocalized);
        RD9_DXDIAG_STRING(tail, szTestResultDDEnglish);
        RD9_DXDIAG_STRING(tail, szTestResultD3D7Localized);
        RD9_DXDIAG_STRING(tail, szTestResultD3D7English);
        RD9_DXDIAG_STRING(tail, szTestResultD3D8Localized);
        RD9_DXDIAG_STRING(tail, szTestResultD3D8English);
        RD9_DXDIAG_STRING(tail, szTestResultD3D9Localized);
        RD9_DXDIAG_STRING(tail, szTestResultD3D9English);
        if (child)
        {
            child->Release();
            child = 0;
        }
    }
cleanup:
    if (child) child->Release();
    if (displays) displays->Release();
    return result;
}

#undef RD9_DXDIAG_VALUE
#undef RD9_DXDIAG_STRING
#undef RD9_DXDIAG_WIDE
#undef RD9_DXDIAG_WIDE_I

// PE 0x10006EB0. The version result requires all three properties, even though the caller
// ignores the minor version when deciding whether the installed runtime is new enough.
bool RD9_THUNK queryInstalledDirectX(RendDX9Base* b RD9_THUNK_EDX,
                                     DWORD* major, DWORD* minor, char* letter)
{
    bool haveMajor = false, haveMinor = false, haveLetter = false, success = false;
    const bool initializedCom = SUCCEEDED(::CoInitialize(0));
    const CLSID providerClass = { 0xA65B8071, 0x3BFE, 0x4213,
        { 0x9A, 0x5B, 0x49, 0x1D, 0xA4, 0x46, 0x1C, 0xA7 } };
    const IID providerInterface = { 0x9C6B4CB0, 0x23F8, 0x49CC,
        { 0xA3, 0xED, 0x45, 0xA5, 0x50, 0x00, 0xA6, 0xD2 } };
    IDxDiagProvider* provider = 0;
    if (SUCCEEDED(::CoCreateInstance(providerClass, 0, CLSCTX_INPROC_SERVER, providerInterface,
                                     reinterpret_cast<void**>(&provider))))
    {
        DXDIAG_INIT_PARAMS parameters = {};
        // [CHOSEN, not recovered] The native SDK widens pReserved on x64; x86 still passes 16.
        parameters.dwSize = static_cast<DWORD>(sizeof(parameters));
        parameters.dwDxDiagHeaderVersion = 0x6F;
        parameters.bAllowWHQLChecks = FALSE;
        parameters.pReserved = 0;
        if (SUCCEEDED(provider->Initialize(&parameters)))
        {
            IDxDiagContainer* root = 0;
            if (SUCCEEDED(provider->GetRootContainer(&root)))
            {
                {
                    std::vector<DxDiagDisplayInfo*> displays;
                    collectDxDiagDisplays(b RD9_THUNK_ARG, root, displays); // result discarded
                    for (size_t i = 0; i < displays.size(); ++i)
                    {
                        DxDiagDisplayInfo* info = displays[i];
                        b->string51C = std::string(info->header.szDescription) + " Mem:"
                            + info->header.szDisplayMemoryEnglish + " Driver:"
                            + info->header.szDriverVersion + " " + info->header.szDriverAttributes
                            + " AGP:" + info->tail.szAGPStatusEnglish;
                        if (info)
                        {
                            info->~DxDiagDisplayInfo();
                            ::renddx9abi::engineFree(info);
                        }
                    }
                } // the pointer vector's storage is freed before the system-info query
                IDxDiagContainer* system = 0;
                if (SUCCEEDED(root->GetChildContainer(L"DxDiag_SystemInfo", &system)))
                {
                    VARIANT value;
                    ::VariantInit(&value);
                    if (SUCCEEDED(system->GetProp(L"dwDirectXVersionMajor", &value))
                        && value.vt == VT_UI4)
                    {
                        if (major) *major = value.ulVal;
                        haveMajor = true;
                    }
                    ::VariantClear(&value);
                    if (SUCCEEDED(system->GetProp(L"dwDirectXVersionMinor", &value))
                        && value.vt == VT_UI4)
                    {
                        if (minor) *minor = value.ulVal;
                        haveMinor = true;
                    }
                    ::VariantClear(&value);
                    if (SUCCEEDED(system->GetProp(L"szDirectXVersionLetter", &value))
                        && value.vt == VT_BSTR && value.bstrVal)
                    {
                        char converted[12];
                        ::WideCharToMultiByte(0, 0, value.bstrVal, -1, converted, 10, 0, 0);
                        if (letter) *letter = converted[0];
                        haveLetter = true;
                    }
                    ::VariantClear(&value);
                    if (haveMajor && haveMinor && haveLetter) success = true;
                    system->Release();
                }
                IDxDiagContainer* display = 0;
                if (SUCCEEDED(root->GetChildContainer(L"DxDiag_DisplayDevices.0", &display)))
                {
                    VARIANT value;
                    ::VariantInit(&value);
                    if (SUCCEEDED(display->GetProp(L"szDisplayMemoryEnglish", &value))
                        && value.vt == VT_BSTR)
                        b->opaque08C = static_cast<unsigned int>(::_wtoi(value.bstrVal));
                    ::VariantClear(&value);
                    display->Release();
                }
                root->Release();
            }
        }
        provider->Release();
    }
    if (initializedCom) ::CoUninitialize();
    return success;
}

} // namespace
#endif // _WIN32

// RendDX9_initializeD3DSubsystems, PE 0x1000B160. RendDX9Init.cpp:210-298. The shipped body takes the
// hWndBf2TempSlt pointer and starts with the display-mode copy sub_1000CA40 (0x1000B177); here the
// caller, constructD3DHalf, makes that copy immediately before the call. The call order after it
// matches the listing.
// Initializes renderer subsystems, returning false on failure.
// The base constructor intentionally discards the result and continues.
bool initializeD3DSubsystems(void* renderer)
{
    RendDX9Base* b = baseOf(renderer);
#if defined(_WIN32)
    // PE 0x1000B17C..0x1000B1C2, refusal at 0x1000B7BD..0x1000B7FB.
    DWORD major = 0, minor = 0;
    char letter = 0;
    const bool available = queryInstalledDirectX(b RD9_THUNK_ARG, &major, &minor, &letter);
    if (!available || major < 9u || (major == 9u && static_cast<signed char>(letter) < 'c'))
    {
        char message[1024];
        ::_snprintf(message, 0x400,
            "DirectX version 9.0c or higher is required to run BF2.\n"
            "Your system seems to have DirectX version %d.%d%c. Please\n"
            "upgrade to at least 9.0c, and try again.",
            static_cast<int>(major), static_cast<int>(minor),
            static_cast<int>(static_cast<signed char>(letter)));
        ::MessageBoxA(0, message, "Error!", 0);
        return false;
    }
#else
    // [CHOSEN, not recovered] Native backends have no installed Windows DirectX version
    // or DxDiag COM provider. Do not invent version numbers or diagnostic records.
    std::fprintf(stderr, "[CHOSEN, not recovered] dice::hfe::dx9rend::initializeD3DSubsystems: "
                         "Windows DxDiag version check unavailable; checking native backend presence\n");
    std::fflush(stderr);
#endif
#if !defined(_WIN32)
    // [CHOSEN, not recovered] Refuse a missing native backend before calling its interface.
    if (!b->d3d9)
    {
        ::renddx9abi::logf("initializeD3DSubsystems: Direct3DCreate9 returned null - native backend unavailable.");
        return false;
    }
#endif

    if (!createD3DDevice(renderer))
    {
        initDeviceError(210, "Failed to init D3D device.");
        return false;
    }
#if !defined(_WIN32)
    // [PLATFORM] [CHOSEN, not recovered] Retail fills +0x08C with the DxDiag display memory in MB
    // (szDisplayMemoryEnglish, queryInstalledDirectX above); createDeviceCaps copies it to
    // DeviceCaps +0x584, where the presets' texture rule (PE 0x1000F310) drops a level under
    // 240 MB. Native backends have no DxDiag; the device's texture memory stands in (Metal:
    // MTLDevice.recommendedMaxWorkingSetSize). Nothing else reads the field.
    if (b->device)
        b->opaque08C = static_cast<unsigned int>(b->device->GetAvailableTextureMem() >> 20);
#endif

    // Construct subsystems in retail order; each failure retains its shipped diagnostic line.
    if (!createDeviceCaps(renderer))
    {
        initDeviceError(218, "Failed to create devicecaps.");
        return false;
    }

    // VideoOptions_initialize(this + 254) - the shipped call site, immediately after DeviceCaps and
    // before the StateManager. It needs the caps, which is why it sits here and not in the base
    // constructor.
    videoOptionsInitialize(renderer);

    // PE 0x1000B2EE..0x1000B31B: the host VideoSettings singleton pushes its pending settings (the
    // default profile filled them before the renderer existed) into this VideoOptions (slot +0x28,
    // applyToOptions). The singleton is not null-tested.
    IVideoSettings* videoSettings = static_cast<IVideoSettings*>(
        classManager->getSingleton(renddx9abi::HostString("VideoSettings")));
    videoSettings->applyToOptions(b->embeddedVideoOptions());

    // Construct StateManager before EffectManager so subsequent draws can publish state.
    if (!createStateManager(b->device))
    {
        initDeviceError(232, "Failed to create StateManager.");
        return false;
    }

    // The 168-byte class between the StateManager and the QueryManager, with no error line of its
    // own - dword_1023D954, now identified as Ra::EffectManager from the __FILE__ its own failure
    // path quotes (Ra/EffectManager.cpp:438).
    Ra::createEffectManager(b->device, readBoolSetting("GSDisableShaderCache", false));

    // Register effect loss handling before constructing queues and shader constants.
    // A failed effect-manager allocation leaves a null listener, matching the unchecked path.
    registerDeviceLostResetListener(Ra::effectManagerDeviceLostListener(), 5);

    createRenderEntryPool();
    // PE 0x1000B405..0x1000B40B. Construction seeds capability-dependent normal formats.
    // Its vector is destroyed on every subsequent return (0x1000CA10).
    Ra::RaDeclarationBuilder declarationBuilder;
    Ra::raDeclarationBuilderConstruct(declarationBuilder);
    createRenderQueues();
    createRaShaderConstants(b->device);

    // Initialize query ownership before Renderer allocates its frame queries.
    if (!createQueryManager(b->device))
    {
        initDeviceError(249, "Failed to create querymanager.");
        return false;
    }

    // RendDX9Init.cpp:256. The order is the shipped one - the ResourceManager is constructed
    // before the TextureManager below, and its ready flag is set as soon as it exists.
    if (!createResourceManager())
    {
        initDeviceError(256, "Failed to create resourcemanager.");
        return false;
    }

    // RendDX9Init.cpp:267 / :272 - create then init, both in their shipped positions.
    if (!createTextureManager(b->device))
    {
        initDeviceError(267, "Failed to create texturemanager.");
        return false;
    }
    if (!textureManagerInit())
    {
        initDeviceError(272, "Failed to init texturemanager.");
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
    // The helper owns the allocation/init diagnostics; propagate its refusal here.
    if (!createBufferManager(b->device))
        return false;

    // RendDX9Init.cpp:293 / :298 - create then init, both in their shipped positions.
    if (!createShaderManager(renderer, b->device))
    {
        initDeviceError(293, "Failed to create shadermanager.");
        return false;
    }
    if (!shaderManagerInit(renderer))
    {
        initDeviceError(298, "Failed to init shadermanager.");
        return false;
    }

    // 0x1000B78D..0x1000B7A2: `dword_1023B97C ? dword_1023B97C + 8 : 0`,
    // priority 5, direct call the body's last act before the stack temp's destructor
    // (0x1000CA10) and `return 1`. The ShaderManager's listener (off_101DFE50) runs
    // ID3DXEffect::OnLostDevice / OnResetDevice over every cached Effect.
    registerDeviceLostResetListener(shaderManagerDeviceLostListener(), 5);

    return true;
}

// PE 0x10009C20. A live device and DeviceCaps are required; accumulate every HRESULT.
// PE 0x10001120 / 0x10001140 / 0x10001150: inlined MaxStreams, texture-stage and render-target getters.
bool rendererNullSetResources(void* renderer)
{
    RendDX9Base* const b = baseOf(renderer);
    HRESULT result = b->device->SetVertexShader(0);
    result |= b->device->SetPixelShader(0);
    result |= b->device->SetIndices(0);
    for (unsigned int stream = 0; stream < deviceCaps()->caps.MaxStreams; ++stream)
        result |= b->device->SetStreamSource(stream, 0, 0, 0);
    for (unsigned int stage = 0; stage < deviceCaps()->caps.MaxSimultaneousTextures; ++stage)
        result |= b->device->SetTexture(stage, 0);
    for (unsigned int target = 1; target < deviceCaps()->caps.NumSimultaneousRTs; ++target)
        result |= b->device->SetRenderTarget(target, 0);
    return result >= 0;
}

// PE 0x10001090 and the inline destruction at 0x1000B0A3..0x1000B0BD.
// [CHOSEN, not recovered] Owner-TU seam around the destructor and heap free.
void destroyDeviceCaps()
{
    if (DeviceCaps* const caps = static_cast<DeviceCaps*>(g_deviceCaps))
    {
        caps->d3d9->Release();
        g_deviceCaps = 0;
        ::renddx9abi::engineFree(caps);
    }
}

// PE 0x1000A5E0. Swap-chain pointers and the device remain stored after release.
bool rendererReleaseD3DDevice(void* renderer)
{
    RendDX9Base* const b = baseOf(renderer);
    for (std::vector<IDirect3DSwapChain9*>::iterator it = b->swapChains.begin();
         it != b->swapChains.end(); ++it)
        (*it)->Release();

#if defined(__APPLE__)
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: rend-deinit-reftrace] BF2_METAL_REFTRACE=1
    // lists the holders of device references (Platform/Gfx/Metal/RefTrace.h); no effect otherwise.
    bf2metalRefTraceReport(b->device);
#endif
    ULONG remaining = b->device->Release();
    if (remaining)
    {
        // shipped defect: drain references owned elsewhere, then report the failed first release.
        do
        {
            remaining = b->device->Release();
        } while (remaining);
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\RendDX9Init.cpp",
            738, "") << "Direct3DDevice release unsuccessful this must be fixed...";
    }
    return remaining == 0;
}


// PE 0x1000ADB0. Severity-6 failures are fatal through the renderer's Debug stream; their
// `return false` arms are kept as shipped.
bool RendDX9_shutdownD3DSubsystems(void* renderer)
{
    static const char* const kFile =
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\RendDX9Init.cpp";

    geom::destroyLightmapGenerationSingleton();                                  // sub_100A8770
    if (!rendererNullSetResources(renderer))                                      // sub_10009C20
    {
        ::renddx9abi::Debug(DebugTypeError, kFile, 624, "")
            << "Failed to NULL-set all resources.";
        return false;
    }

    destroyRenderEntryPool();                                                     // dword_1023DE08
    Ra::raReleaseVertexDeclarations();                                            // sub_1004A080
    destroyRenderQueues();                                                        // sub_10041A50
    destroyRaShaderConstants();                                                   // sub_1003AD10

    unregisterDeviceLostResetListener(Ra::effectManagerDeviceLostListener());
    Ra::destroyEffectManager();                                                   // +0x14(1)

    rendererDeactivateRenderPath(renderer);                                       // sub_1000D100
    notifyDeviceLostResetListenersDestroy();                                      // sub_10002600

    unregisterDeviceLostResetListener(shaderManagerDeviceLostListener());
    if (!shaderManagerShutdown())                                                 // sub_10022380
    {
        ::renddx9abi::Debug(DebugTypeError, kFile, 642, "")
            << "Failed to release shadermanager.";
        return false;
    }
    destroyShaderManager();                                                       // sub_100248E0

    // The BufferManager is its own listener: `push dword_1023B978`, no adjustment, no null test.
    unregisterDeviceLostResetListener(g_bufferManager);
    if (!bufferManagerShutdown())                                                 // sub_10011390
    {
        ::renddx9abi::Debug(DebugTypeError, kFile, 651, "")
            << "Failed to release Buffermanager.";
        return false;
    }
    destroyBufferManager();                                                       // sub_10012800

    releaseStateManager();                                                        // dword_1023B984

    unregisterDeviceLostResetListener(textureManagerDeviceLostListener());
    if (!textureManagerShutdown())                                                // sub_10017BF0
    {
        ::renddx9abi::Debug(DebugTypeError, kFile, 663, "")
            << "Failed to release texturemanager.";
        return false;
    }
    destroyTextureManager();                                                      // +0x14(1)

    destroyResourceManager();                                                     // dword_1023B988
    destroyQueryManager();                                                        // dword_1023B98C
    destroyDeviceCaps();                                                          // dword_1023B980

    return rendererReleaseD3DDevice(renderer);                                    // sub_1000A5E0
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
// PE 0x10009DF0.
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
// PE 0x10009E20.
bool rendererGetVSync(const void* renderer)
{
    return reinterpret_cast<const RendDX9Base*>(renderer)->vsync != 0;
}

// store the byte, nothing else - no reset; present picks it up on the
// next mode-2 present.
// PE 0x10009E30.
void rendererSetPresentAsync(void* renderer, bool enabled)
{
    baseOf(renderer)->presentAsync = enabled ? 1u : 0u;
}

// (7 B): `mov al, [ecx+3ACh]; ret`.
// PE 0x10009E40.
bool rendererGetPresentAsync(const void* renderer)
{
    return reinterpret_cast<const RendDX9Base*>(renderer)->presentAsync != 0;
}

// store the byte.
// PE 0x10009E50.
void rendererSetPresentSpinIfBusy(void* renderer, bool enabled)
{
    baseOf(renderer)->presentSpinIfBusy = enabled ? 1u : 0u;
}

// (7 B): `mov al, [ecx+3ADh]; ret`.
// PE 0x10009E60.
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


// PE 0x10009FF0.
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
#if defined(__APPLE__)
        firstColumn = bf2x87::toIntTruncate(rect[0]);
#else
        firstColumn = static_cast<int>(rect[0]);
#endif
#if defined(__APPLE__)
        row         = bf2x87::toIntTruncate(rect[1]);
#else
        row         = static_cast<int>(rect[1]);
#endif
#if defined(__APPLE__)
        lastColumn  = bf2x87::toIntTruncate(rect[2]);
#else
        lastColumn  = static_cast<int>(rect[2]);
#endif
#if defined(__APPLE__)
        lastRow     = bf2x87::toIntTruncate(rect[3]);
#else
        lastRow     = static_cast<int>(rect[3]);
#endif
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
// PE 0x1000A3E0.
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
// PE 0x1000A700 (listing read: probe render with MULTISAMPLEANTIALIAS, force/disable bytes +0x514/+0x515).
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
// PE 0x1000B940 (listing read in full: the member constructions, the scalar stores, the three event
// names, Direct3DCreate9, the two device settings, the display-mode singleton, initializeD3DSubsystems,
// then the render mode store and the rodSpecial flag).
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
    // The screenshot directory and request name, 0x1000BA38 / 0x1000BA55.
    new (&b->screenDumpPath) std::string("@HOME@/Screenshots");
    new (&b->screenDumpName) std::string("");
    new (&b->string51C) std::string();             // 0x1000BAAE, default-constructed

    // DeviceLostResetListenerList_ctor(this + 92), PE 0x10002490.
    deviceLostResetListCtor(&b->deviceLostList);

    // RenderPathListenerList_ctor(this + 108). Built here because Renderer::initialize step 5
    // pushes the GSRenderPath setting into it.
    renderPathListCtor(&b->renderPathList);      // PE 0x1000D140: active path 0, empty std::list, changed byte 0

    // PE 0x1000D900 at base + 0x108 (after the scalar stores above, before the string members):
    // turns the draw counters on (+0x284) and clears the counter ranges. Without it every
    // stats[644] test in the draw wrappers failed and no counter ever advanced.
    rendererStatsConstruct(b->renderStats);

    // two of the base constructor's scalar stores the frame bracket and the
    // device-loss core read (the Renderer constructor has already zeroed the whole base storage, so
    // only the NON-zero stores need repeating; the zero ones - +0x100, +0x3AC, +0x3AD, +0x3E8 - are
    // 0x1000B9F6 / 0x1000BA43 / 0x1000BA49 / 0x1000BA6C and already hold):
    //   0x1000B9FC  mov byte ptr [esi+101h], 1   - "the frame has been presented": the first
    //               beginScene therefore does NOT prime a present of an empty back buffer, and
    //               present() answers false until an endScene clears it.
    b->framePresented = 1;
    //   0x1000B964  mov dword ptr [esi+1Ch], 0FFFF0000h - the debug-text colour the frame-time
    //               overlay hands the DebugTextWriter (sub_10029C50).
    b->debugTextColour = 0xFFFF0000u;

    // PE 0x1000BA72 / 0x1000BA78, before the embedded VideoOptions constructor.
    b->occlusionTerrainMaxFrames = 1;
    b->occlusionObjectsMaxFrames = 1;

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

    // 0x1000BAB4..0x1000BB2B: the renderer's three event names, category 11, registered before
    // the base is published. The base destructor unregisters them (0x1000BC3D..0x1000BC67).
    eventManager->registerEvent(static_cast<EventCategory>(11), 1,
                                std::string("ECRendererREToggleFullscreen"));
    eventManager->registerEvent(static_cast<EventCategory>(11), 2,
                                std::string("ECRendererREChangeResolution"));
    eventManager->registerEvent(static_cast<EventCategory>(11), 3,
                                std::string("ECRendererREPreChangeResolution"));

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

    // PE 0x1000BBE7: the hemi render mode starts at 0. PE 0x1000BBDF..0x1000BC10: the byte at +0x102 is
    // whether the host publishes a "rodSpecial" singleton.
    b->renderMode = 0;
    b->flag102 = classManager->getSingleton(renddx9abi::HostString("rodSpecial")) != 0 ? 1 : 0;
}

void resourceTrackerWalk(unsigned int kind);   // BufferManager.cpp - PE 0x10047DF0

namespace {

// PE 0x10002460. Before its vector, the shipped list destructor walks the four resource-tracker maps
// at 0x1023EAA0 (PE 0x10047DF0 with indices 0..3), which reads and writes nothing but the iteration.
void destroyDeviceLostResetList(DeviceLostResetList& list)
{
    resourceTrackerWalk(0);
    resourceTrackerWalk(1);
    resourceTrackerWalk(2);
    resourceTrackerWalk(3);
    list.~DeviceLostResetList();
}

} // namespace

// PE 0x1000BC20 past its vptr store (the base table 0x101DDA38). The renderer's own members are
// already destroyed when this runs.
void destructD3DHalf(void* renderer)
{
    typedef std::vector<Mode40>                  ModeList;
    typedef std::vector<unsigned int>            DepthFormatList;
    typedef std::vector<void*>                   WindowList;
    typedef std::vector<IDirect3DSwapChain9*>    SwapChainList;
    typedef std::vector<D3DADAPTER_IDENTIFIER9>  AdapterList;
    typedef std::vector<MultiSample8>            MultiSampleList;

    RendDX9Base* b = baseOf(renderer);
    destroyMovieRenderer(b);                            // sub_100051D0, a direct call
    b->d3d9->Release();                                 // unguarded
    eventManager->unregisterEvent(static_cast<EventCategory>(11), 1);
    eventManager->unregisterEvent(static_cast<EventCategory>(11), 2);
    eventManager->unregisterEvent(static_cast<EventCategory>(11), 3);
    if (b->renderView2)                                 // +0xFC
        static_cast<IBase*>(b->renderView2)->release();

    // The members, in reverse declaration order.
    b->string51C.~basic_string();                       // 0x1000BC7B
    videoOptionsDestruct(renderer);                    // sub_10010630
    b->screenDumpName.~basic_string();                  // 0x1000BC92
    b->screenDumpPath.~basic_string();                  // 0x1000BC9E
    b->adapterInfo.~AdapterList();                      // +0x39C
    b->swapChains.~SwapChainList();                     // +0xE0
    // PE 0x1000C9B0: display-mode member destruction frees windows before multisamples and depth.
    b->windows.~WindowList();
    b->multiSamples.~MultiSampleList();
    b->depthFormats.~DepthFormatList();
    b->modeList.~ModeList();                            // sub_10009510 over the modes, then free
    b->renderPathList.~RenderPathList();                // sub_1000D130
    destroyDeviceLostResetList(b->deviceLostList);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
