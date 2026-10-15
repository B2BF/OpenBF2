// Layout view over renderer storage, not a C++ base class. A second vptr would shift fields.
// [RETIRE-WHEN: RW-56] Replace the overlay after the base vtable is fully typed and
// inheritance preserves the validated offsets.


#ifndef BF2_RENDDX9_RENDDX9BASE_H
#define BF2_RENDDX9_RENDDX9BASE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include <list>
#include <string>
#include "../../RendDX9Abi/DisplayModeAbi.h"

#include <vector>

namespace dice {
namespace hfe {
class IVideoOptions;

namespace dx9rend {

struct EngineTexture;

// The two listener contracts embedded throughout RendDX9.dll.  Neither interface has a virtual
// destructor in the retail image: the device listener table is exactly three slots and the render
// path listener table is exactly six.  Owners destroy concrete objects through their own type.
class DeviceLostResetListener
{
public:
    virtual void onDeviceLost() = 0;
    virtual void onDeviceReset() = 0;
    virtual void onDeviceDestroy() = 0;
};

class RenderPathListener
{
public:
    virtual void onRenderPathActivated(int path) = 0;
    virtual void onRenderPathDeactivated() = 0;
    virtual void onRenderPathChanged(int path) = 0;
    virtual void onRenderPathNotification(int path) = 0;
    virtual int  getCurrentRenderPath() const = 0;
    virtual std::string getRenderPathListenerName() const = 0;
};

#if defined(_M_IX86)
static_assert(sizeof(DeviceLostResetListener) == 4,
              "device-lost listener is one three-slot vptr");
static_assert(sizeof(RenderPathListener) == 4,
              "render-path listener is one six-slot vptr");
#endif

// The device-lost/reset listeners (retail +0x5C: a std::list of {priority, listener}, then the lost
// and sort-dirty bytes). Kept in registration order and sorted by unsigned priority, stably, before
// each broadcast. Constructed in place by constructD3DHalf.
struct DeviceLostResetEntry
{
    int                      priority;
    DeviceLostResetListener* listener;
};

struct DeviceLostResetList
{
    DeviceLostResetList() : deviceLost(0), dirty(0) {}

    std::list<DeviceLostResetEntry> entries;
    unsigned char                   deviceLost;
    unsigned char                   dirty;
};

// The render-path listeners (retail +0x6C: the active path, a std::list of listeners, the changed
// byte). Constructed in place by renderPathListCtor. `constructed` is [CHOSEN, not recovered]: the
// Renderer zeroes this storage first, and the paths that can run before construction report it.
struct RenderPathList
{
    RenderPathList() : activeRenderPath(0), changed(0), constructed(1) {}

    int                            activeRenderPath;
    std::list<RenderPathListener*> listeners;
    unsigned char                  changed;
    unsigned char                  constructed;
};

// HemiMapManager.cpp's editor/offline bake is deliberately routed through typed owner-TU seams.
// The renderer owns all of the byte-offset state below; keeping that knowledge in BF2Render.cpp
// prevents the recovered Hemi class from becoming another raw-vtable/offset facade.
struct HemiRenderState
{
    unsigned char bytes[0x2C];
};


// The display-mode records the enumeration subsystem builds, held in std::vectors (issue #7).


// An 8-byte multisample descriptor from CheckDeviceMultiSampleType: the D3DMULTISAMPLE_TYPE and the
// (quality-count - 1). Held per-mode and, parallel to the AA string list, inside VideoOptions.
typedef renddx9abi::DisplayModeMultiSample MultiSample8;

// The display-mode record (retail: 40 bytes - the sub_1000CEE0 header plus the two nested lists
// sub_100091E0 deep-copies and sub_10009510 frees). Shared with BF2.exe, which reads the list
// Renderer +0x3C returns; see DisplayModeAbi.h.
typedef renddx9abi::DisplayModeRecord Mode40;

static_assert(sizeof(MultiSample8) == 8, "multisample descriptor is 8 bytes");


// VideoOptions - constructed inside the base object at retail +0x3F8 (1016) and reached through
// the base's vtable slot +0x74, which is one instruction: `return this + 1016`.

// Five parallel eight-entry rows. getQuality(i) - VideoOptions vtable +0x04, `sub_100112B0` - is
// `return this[i + 10]`, so it reads `quality[]` and nothing else.

struct VideoOptions
{
    /* 0x000 */ void*         vptr;              // VideoOptions_vftable_primary, 15 slots
    /* 0x004 */ void*         vptrEventHandler;  // the ECCoreEventHandler subobject
    /* 0x008 */ int           rowA[8];           // all 4          [UNRESOLVED] role
    /* 0x028 */ int           quality[8];        // all 2   <- the one getQuality reads
    /* 0x048 */ int           rowC[8];           // 1,1,1,0,0,1,1,1   [UNRESOLVED] role
    /* 0x068 */ int           rowD[8];           // all 3          [UNRESOLVED] role
    /* 0x088 */ int           maxQuality[8];     // all 3, then clamped by hardware caps
    /* 0x0A8 */ unsigned char flags[8];          // all 0
    // The two dynamic-option string lists. setDynamicOption (sub_1000FB70) and the readback slot
    // +0x20 (sub_10011350) read their retail begin pointers at +0xB4/+0xC4.
    /* 0x0B0 */ std::vector<std::string> aaList;    // anti-aliasing option strings ("Off", "%dSamples")
    /* 0x0C0 */ std::vector<std::string> resList;   // resolution strings ("%ix%i@%iHz")
    // AND THE correction IS load-bearing. The initialize tail writes -1 to
    // 0xD0, 0xD4, 0xD8 AND 0xDC. apply guards its resolution-change path on `*(this+212)!=*(this+220)`
    // (0xD4 vs 0xDC); four equal -1 values make the guard false, so the device-reset path is correctly
    // skipped on a first boot - which is what makes apply writable at all.

    // 0xD8/0xDC are also the selected dynamic-option indices: setDynamicOption writes this[optIdx+54]
    // (0xD8 for AA, 0xDC for resolution) and slot +0x20 reads them. -1 means "nothing selected yet".
    /* 0x0D0 */ int           minusOneD0;        // dword 52
    /* 0x0D4 */ int           minusOneD4;        // dword 53   <- apply's guard, left side
    /* 0x0D8 */ int           minusOneD8;        // dword 54   <- selected AA index (init -1)
    /* 0x0DC */ int           minusOneDC;        // dword 55   <- apply's guard right / selected resolution index
    // The Mode40 kept parallel to resList (sub_10010710 pushes here as it pushes each resolution
    // string; setDynamicOption reads *(this+0xE4)+40*resIndex to hand the chosen mode to the AA
    // builder), and the MultiSample8 descriptors kept parallel to aaList.
    /* 0x0E0 */ std::vector<Mode40>       modeCache;
    /* 0x0F0 */ std::vector<MultiSample8> aaDescriptors;
    /* 0x100 */ float         dynFloat;          // the float slot +0x24 clamps into [0.5,1.0]
    /* 0x104 */ int           windowSizeX;       // dword 65, GSWindowSizeX
    /* 0x108 */ int           windowSizeY;       // dword 66, GSWindowSizeY
    /* 0x10C */ int           refreshHz;         // dword 67, GSRefresh
    /* 0x110 */ int           skipMipLevels;     // dword 68, GSSkipMipLevels (default -1)
    // The dirty flags apply sets when a setting actually changes. +0x119 is the one apply's first
    // statement tests - "Not allowed to apply video options when a game is loaded!".
    /* 0x114 */ unsigned char dirty114;
    /* 0x115 */ unsigned char dirty115;
    /* 0x116 */ unsigned char dirty116;
    /* 0x117 */ unsigned char dirty117;
    /* 0x118 */ unsigned char dirty118;
    /* 0x119 */ unsigned char gameLoaded;
    /* 0x11A */ unsigned char opaque11A[0x11C - 0x11A];
};

struct RendDX9Base
{
    /* 0x000 */ void*                  vptr;              // 0x101DDA38 in the shipped image
    /* 0x004 */ int                    refCount;          // base ctor: 1
    /* 0x008 */ IDirect3D9*            d3d9;              // Direct3DCreate9(0x20)
    /* 0x00C */ IDirect3DDevice9*      device;            // CreateDevice's out-parameter
    /* 0x010 */ unsigned char          opaque010[0x020 - 0x010];   // +0x01C is set to -65536
    // [INFERRED] name. The mode rendererSetHemiRenderMode stores (0..5; 2 and 3 select the
    // wireframe fill mode) and rendererGetHemiRenderMode returns.
    /* 0x020 */ int                    renderMode;

    // The present parameters live inside the object, at +0x24, and CreateDevice is handed `this+36`
    // directly. sub_1000A500 memsets exactly 0x38 bytes here, which is sizeof(D3DPRESENT_PARAMETERS)
    // and the check that the offset is right.
    /* 0x024 */ D3DPRESENT_PARAMETERS  pp;

    /* 0x05C */ DeviceLostResetList    deviceLostList;
    /* 0x06C */ RenderPathList         renderPathList;

    /* 0x080 */ unsigned int           adapter;           // base ctor: 0, set in sub_1000B0D0
    /* 0x084 */ unsigned int           deviceType;        // GSDeviceType,     default 1
    /* 0x088 */ unsigned int           behaviorFlags;     // GSDeviceBehavior, default 80 (0x50)
    /* 0x08C */ unsigned int           opaque08C;
    // The display-mode list slot_0x3C (sub_10007420) builds and returns (as `this+0x90`). Its retail
    // begin is at +0x94, which is what sub_10007420 and its caller sub_10010710 read.
    /* 0x090 */ std::vector<Mode40>    modeList;

    // the display mode, copied wholesale out of BF2's hWndBf2TempSlt
    // sub_1000CA40 targets +0xA0 (`lea ecx, [esi+0A0h]`), sub_10009250 lays out the scalars.
    /* 0x0A0 */ unsigned char          fullscreen;
    /* 0x0A1 */ unsigned char          padA1[3];
    /* 0x0A4 */ unsigned short         modeWidth;
    /* 0x0A6 */ unsigned short         modeHeight;
    /* 0x0A8 */ unsigned short         modeRefresh;
    /* 0x0AA */ unsigned char          modeBpp;
    /* 0x0AB */ unsigned char          padAB;

    // The depth-stencil candidates. BF2 pushes exactly one, D3DFMT_D24S8 (75).
    /* 0x0AC */ std::vector<unsigned int> depthFormats;

    // The multisample candidates, 8-byte entries. BF2 leaves this empty; VideoOptions' resolution
    // change (sub_1000F8D0, 0x1000F92C..0x1000F961) replaces it with the ONE selected AA descriptor,
    // which buildPresentParameters then hands to the Reset. Retail clears it by freeing its storage,
    // so an empty list holds no storage (data() is null); the helpers below keep that.
    /* 0x0BC */ std::vector<MultiSample8> multiSamples;

    /* 0x0CC */ unsigned char          vsync;
    /* 0x0CD */ unsigned char          padCD[3];

    // The window list. hDeviceWindow is its first entry.
    /* 0x0D0 */ std::vector<void*>     windows;

    // The swap chains: the device's chain 0, then one additional chain per further window. present
    // and the render-target copy index it by currentSwapChain.
    /* 0x0E0 */ std::vector<IDirect3DSwapChain9*> swapChains;
    /* 0x0F0 */ unsigned int           currentSwapChain;  // base ctor: 0
    // The temporary back buffer the render-target selector sub_10005920 prefers over the Renderer's
    // back buffer (+0x76C); the 2006 build's assert names it m_tempBackBuffer.
    /* 0x0F4 */ IDirect3DSurface9*     tempBackBuffer;
    /* 0x0F8 */ void*                  renderView;        // zeroed here, filled by Renderer's ctor
    // A second render view, and it is a different object from the one above. The constructor builds
    // renderView directly; this one comes from createInstance(820301, 820201) in initialize step 8.
    // Same class, two instances - getRenderView() returns the first, never this.
    /* 0x0FC */ void*                  renderView2;
    // The frame bracket's two bytes (RendDX9Draw.cpp). [INFERRED] names, from their writers:
    // beginScene sets +0x100 after a successful BeginScene, endScene clears both, present's
    // D3DERR_DEVICELOST arm clears +0x100; present(3) and a successful present set +0x101, present
    // refuses to run while it is set, and beginScene primes a present only while it is clear. The
    // base constructor stores 0 and 1 (0x1000B9F6 / 0x1000B9FC).
    /* 0x100 */ unsigned char          inScene;
    /* 0x101 */ unsigned char          framePresented;
    // [UNRESOLVED] role. Renderer::initialize step 14, the hemi top-down capture and RaShader's
    // permutation creation test it; nothing recovered writes it.
    /* 0x102 */ unsigned char          flag102;
    /* 0x103 */ unsigned char          pad103;
    // The frame ordinal Renderer::update increments and Renderer +0x54 returns.
    /* 0x104 */ unsigned int           frameOrdinal;
    // The draw/resource statistics block Renderer slot +0x6C returns (`lea eax, [ecx+108h]`): +0x1C
    // counts vertex buffers, +0x2C textures, +448..+472 the draw counters gated by the byte at +644.
    // [INFERRED] extent: it runs to the next known member.
    /* 0x108 */ unsigned char          renderStats[0x39C - 0x108];

    // Adapter identifiers populated during device creation; each raw D3D record is 1100 bytes.
    /* 0x39C */ std::vector<D3DADAPTER_IDENTIFIER9> adapterInfo;

    // The Present flags byte present(mode 2) substitutes and the wasstilldrawing-retry byte, set
    // only by the `renderer.presentAsync` / `renderer.presentSpinIfBusy` console properties
    // (0x10009E30 / 0x10009E50; attested names from the bindings). Base ctor: both 0.
    /* 0x3AC */ unsigned char          presentAsync;
    /* 0x3AD */ unsigned char          presentSpinIfBusy;
    /* 0x3AE */ unsigned char          opaque3AE[0x3F4 - 0x3AE];
    // The EnvMapManager renderFrame's environment-map loop reads; withheld in this build.
    /* 0x3F4 */ void*                  envMapManager;

    // VideoOptions_ctor(this + 1016), running to retail +0x513. Constructed in place by
    // videoOptionsConstruct and destroyed by destructD3DHalf.
    /* 0x3F8 */ VideoOptions           videoOptions;

    dice::hfe::IVideoOptions* embeddedVideoOptions()
    {
        return reinterpret_cast<dice::hfe::IVideoOptions*>(&videoOptions);
    }

    // sub_1000A6B0 and the alpha-to-coverage probe use these three adjacent bytes directly.
    /* 0x514 */ unsigned char          forceMultisample;
    /* 0x515 */ unsigned char          disableMultisample;
    /* 0x516 */ unsigned char          hasAlphaToCoverage;
    // [INFERRED] name. The ONE writer after construction is the Renderer's ECCore handler, id 15
    // CEActivateApp, which copies the event's payload byte here (0x1002F758: `mov [esi-21h], cl`,
    // esi = the +0x538 handler subobject);
    // the one reader is Renderer +0xC0 (0x100055A0: `return this[0x517] == 0`), which +0xBC polls
    // to wait out a deactivated application. The base constructor stores 1 (0x1000BAA1).
    /* 0x517 */ unsigned char          appActive;
    // The performance-feature bitset rendererPerformanceFlag tests.
    /* 0x518 */ unsigned int           performanceFlags;
    /* 0x51C */ unsigned char          opaque51C[0x538 - 0x51C];
};

// The editor host (Tools/BF2Editor EngineHost) reads the device through the head
// {vptr, int refCount, IDirect3D9*, IDirect3DDevice9*}: +0x0C on x86, +0x18 on x64 (issue #7).
static_assert(offsetof(RendDX9Base, device) == (sizeof(void*) == 4 ? 0x0C : 0x18),
              "the renderer head the editor host reads");

// The x86 offsets ahead of the first std container, kept as evidence. The members from the
// device-lost list on are native and no longer sit at their retail offsets. The D3D structure
// sizes are retail x86 evidence only: every consumer uses sizeof and the named members (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(D3DPRESENT_PARAMETERS) == 0x38,
              "sub_1000A500 memsets 0x38 bytes over the present parameters");
static_assert(offsetof(RendDX9Base, d3d9)              == 0x008, "d3d9 at +0x08");
static_assert(offsetof(RendDX9Base, device)            == 0x00C, "device at +0x0C");
static_assert(offsetof(RendDX9Base, pp)                == 0x024, "present parameters at +0x24");
static_assert(offsetof(RendDX9Base, deviceLostList)    == 0x05C, "device-lost list at +0x5C");
static_assert(offsetof(D3DPRESENT_PARAMETERS, PresentationInterval) == 0x34,
              "the vsync setter writes base+0x58 == pp+0x34");
static_assert(sizeof(D3DADAPTER_IDENTIFIER9) == 1100,
              "sub_1000C920 strides by 1100 - the element IS the raw D3D structure");
#endif


// The multisample list's operations (RendDX9Base::multiSamples).

// Empties the list and frees its storage.
inline void clearMultiSampleList(RendDX9Base* b)
{
    std::vector<MultiSample8>().swap(b->multiSamples);
}

inline void pushMultiSample(RendDX9Base* b, const MultiSample8& value)
{
    b->multiSamples.push_back(value);
}

// Replaces the list with [first, last). The replacement is built before the old storage is freed;
// an empty range leaves no storage.
inline void assignMultiSampleList(RendDX9Base* b, const MultiSample8* first,
                                  const MultiSample8* last)
{
    std::vector<MultiSample8>(first, last).swap(b->multiSamples);
}

// The list half of sub_1000A6B0's multisample probe: a first descriptor whose type, read as a
// signed int, is positive.
inline bool multiSampleListActive(const RendDX9Base* b)
{
    return !b->multiSamples.empty() && static_cast<int>(b->multiSamples[0].type) > 0;
}


void videoOptionsConstruct(void* base);  // 10010590, before any D3D initialization
void videoOptionsInitialize(void* base);


// hWndBf2TempSlt - the host's display-mode carrier (retail: the recovered 0x40-byte record). See
// DisplayModeAbi.h.

typedef renddx9abi::HostDisplayMode HWndBf2TempSlt;

// Implemented in RendDX9Init.cpp.
void  constructD3DHalf(void* renderer);
// [CHOSEN, not recovered] Destroys the std containers constructD3DHalf and videoOptionsConstruct
// built in the base storage (issue #7); the Renderer's release calls it before freeing the object.
void  destructD3DHalf(void* renderer);
void  copyDisplayModeFrom(void* renderer, const HWndBf2TempSlt* src);
void  clearDisplayModeCopies(void* renderer);
void  renderPathListCtor(void* list);
void  setActiveRenderPath(RenderPathList& list, int path);
void  registerDeviceLostResetListener(DeviceLostResetListener* listener, int priority);
void  unregisterDeviceLostResetListener(DeviceLostResetListener* listener);
// Several recovered classes still expose their listener subobject only as an attested byte
// offset.  Keep that cast in these compatibility overloads; list storage and dispatch stay typed.
void  registerDeviceLostResetListener(void* listener, int priority);
void  unregisterDeviceLostResetListener(void* listener);
void  notifyDeviceLostResetListenersLost();      // 0x10002530 on the global renderer's list
void  notifyDeviceLostResetListenersReset();
void  notifyDeviceLostResetListenersDestroy();   // shutdown only; no caller yet
// Device reset and cooperative-level handling.
bool  resetDevice(void* renderer);               // sub_10009A60, RendDX9Init.cpp:401
bool  handleLostDevice(void* renderer);          // sub_10009BA0, the TestCooperativeLevel loop
// The `renderer.vsync` / `renderer.presentAsync` / `renderer.presentSpinIfBusy` properties' pairs.
void  rendererSetVSync(void* renderer, bool enabled);             // resets the device
bool  rendererGetVSync(const void* renderer);
void  rendererSetPresentAsync(void* renderer, bool enabled);
bool  rendererGetPresentAsync(const void* renderer);
void  rendererSetPresentSpinIfBusy(void* renderer, bool enabled);
bool  rendererGetPresentSpinIfBusy(const void* renderer);
void  registerRenderPathListener(RenderPathListener* listener);
void  unregisterRenderPathListener(RenderPathListener* listener);
void  registerRenderPathListener(void* listener);
void  unregisterRenderPathListener(void* listener);
bool  rendererPerformanceFlag(unsigned int bit);
unsigned int rendererFrameOrdinal();
int   readIntSetting(const char* name, int defaultValue);
bool  readBoolSetting(const char* name, bool defaultValue);
// Settings vslot +0x40 - const std::string& getString(name, default), read from
// TextureManager_loadTextureFromFile's two GSLoadLevel/GSLevelDirectory calls at 0x1001860C.
void  readStringSetting(const char* name, std::string& out);
// Same Settings +0x40 getter with the caller-provided default preserved.  HemiMapManager uses the
// retail non-empty default " ERROR ", so routing it through readStringSetting's empty default
// would be observable on a missing key.
void  readStringSettingDefault(const char* name, const char* defaultValue, std::string& out);
// sub_100010D0 on dword_1023B980 - BYTE1 of D3DCAPS9.PixelShaderVersion (+0xCC), the PS major.
int   deviceCapsPixelShaderMajor();
// sub_100010B0 on dword_1023B980 - BYTE1 of D3DCAPS9.VertexShaderVersion (+0xC4), the VS major.
// Terrain::init picks the low render style when it is below 2.
int   deviceCapsVertexShaderMajor();
bool  deviceCapsIsAti();
bool  deviceCapsIsNvidia();
bool  deviceCapsSupportsTextureFormat(unsigned int format);
bool  deviceCapsSupportsFullSizeRenderTargets();
// The DeviceCaps dword at retail +0x584 (RendDX9Init.cpp's DeviceCaps::unknown584). Requires the
// published DeviceCaps object.
unsigned int deviceCapsUnknown584();


// TextureManager.cpp - the loader's cross-TU surface. The shipped callers reach these
// as __thiscall members on dword_1023B974 / direct calls; these seams carry the same behaviour.

// sub_10019260 - the by-name loader. Returns an EngineTexture* (never null once the manager
// exists: even a missing file yields the wantu.dds fallback wrapper), addRef'd for the caller.
void* textureManagerGetTexture(const std::string& name);
// sub_10017E00 / sub_10017E20 / sub_10017E40 - swap-style setters returning the OLD value.
// Consumers bracket loads with them: Meme forces class 1 + managed, Font::Ported forces managed=0
// for "_def", and Meme's createTexture forces recreate=1 so its render targets survive a reset.
int   textureManagerSwapDefaultMipClass(int value);
int   textureManagerSwapManaged(int value);
int   textureManagerSwapRecreateMode(int value);
// VideoOptions::apply's two stores into the manager's fourteen mip-class records (TM+0x28, 8 bytes
// each: an enabled byte, then the value). The first gives every enabled record `value`; the second
// sets record `index` regardless of its flag. Both require a live manager.
void  textureManagerSetEnabledMipClassValues(int value);
void  textureManagerSetMipClassValue(int index, int value);
// sub_10018550 - create a code-made 2D texture (depth 1) through the normal create path; `existing`
// non-null re-creates into that wrapper (the onDeviceReset arm).
void* textureManagerCreateEmpty(unsigned int w, unsigned int h, unsigned int format,
                                unsigned int mips, unsigned int usage, void* existing);
void* textureManagerCreateCube(unsigned int side, unsigned int format,
                               unsigned int mips, unsigned int usage, void* existing);
void* createRaShaderConstants(IDirect3DDevice9* device);
void  destroyRaShaderConstants();
// [CHOSEN, not recovered] The address of the RaShaderConstants block (g_raShaderConstants) word
// that sits at `retailByteOffset` in the retail x86 0xAFC-byte layout (issue #7). The x86 block
// keeps that layout, so this is block + offset there; the x64 block widens its pointer words, and
// this maps each retail offset onto the native one. A record's value words stay contiguous in both
// layouts. Requires the published block.
unsigned char* raShaderConstantsAt(unsigned int retailByteOffset);
void  raShaderConstantsSetTexture(unsigned int holderOffset,
                                  IDirect3DBaseTexture9* texture);
void  raShaderConstantsSetEngineTexture(unsigned int holderOffset,
                                        EngineTexture* texture);
// Renderer::renderFrame's PE 0x100300A2..0x10030257 world-pass constants. These owner-TU
// operations retain the texture ownership and one-time projection-cache semantics without
// exposing the private RaShaderConstants record layout to the renderer.
void  raShaderConstantsSetAccumulationMap(EngineTexture* texture);
void  raShaderConstantsSetHemiMapTexture(EngineTexture* texture);
void  raShaderConstantsSetHemiMapInfo(const float constants[4]);
// The fog constants (RenderQueues.cpp). sub_10037F90 stores the FogColor holder (+0xBC..+0xC8)
// AND packs its xyz to an opaque D3DCOLOR for D3DRS_FOGCOLOR through the state manager's
// persistent setter - the fixed-function fog stage every Ra pass with `fogenable = true` blends
// by. sub_10037F60 / sub_10038110 / sub_10038140 are plain stores (FogRange +0xA8, WaterFogRange
// +0xF8, WaterFogColor +0x10C).
void  raShaderConstantsSetFogColor(const float rgbw[4]);
void  raShaderConstantsSetFogRange(const float range[4]);
void  raShaderConstantsSetWaterFogRange(const float range[4]);
void  raShaderConstantsSetWaterFogColor(const float rgb[3]);
// Terrain::setupWater's four float stores (+0x168 / +0x170 / +0x178 / +0x180): the value words of
// the holders the constructor names uvLevelAddX / uvLevelMulX / uvLevelAddY / uvLevelMulY.
void  raShaderConstantsSetUvLevel(float addX, float mulX, float addY, float mulY);
// The owned texture of an EngineTexture holder (holderOffset + 4) - Terrain::setupWater tests
// WaterMap (+0x1A0) before loading the water volume into it.
EngineTexture* raShaderConstantsGetEngineTexture(unsigned int holderOffset);
// PostProductionFrameWork.cpp - the owner-block fog record ("THE FOG RECORD" there). The console
// colours are 0..255 on the wire and stored normalised; the per-frame update derives the four
// constants above from the record. BF2Render.cpp's frame function calls the update where the
// shipped renderFrame does (PE 0x10030036..0x10030048).
void  postProductionSetFogColor255(const float rgb[3]);
void  postProductionGetFogColor255(float rgb[3]);
void  postProductionSetWaterFogColor255(const float rgb[3]);
void  postProductionGetWaterFogColor255(float rgb[3]);
void  postProductionUpdateFog(float underwaterDepth);
// BF2Render.cpp - sub_100F7B00 / sub_100F7AD0: the camera's depth below the water plane (0 when
// above it) and the under-water test the fog update forks on.
float underwaterDepth();
bool  cameraIsUnderWater();
// sub_100191C0 - drop the cache's reference to `tex` and erase its node (destroys the texture if
// the cache held the last reference).
void  textureManagerEvictFromCache(void* tex);
// sub_10017F90 - save `texture` as "<name>.dds" through the FileManager's write path (D3DXIFF_DDS);
// true only on D3D_OK. A TextureManager member whose body never reads the manager (TextureManager.cpp).
bool  textureManagerSaveTexture(EngineTexture* texture, const std::string& name);
// Reads level `level` (face `face`) of render target `source` back into the same
// level of `destination` through GetRenderTargetData; a null `source` reads the current swap
// chain's back buffer. False when `destination` is null or the copy fails (logged).
bool  textureManagerCopyRenderTarget(EngineTexture* source, EngineTexture* destination,
                                     unsigned int level, unsigned int face);

// The menu atlas cache (dword_10249EF0, a 0x2C-byte object owned by TextureManager.cpp).
bool  atlasCacheExists();                                  // dword_10249EF0 != 0
void* atlasCacheEnsure();                                  // create-if-null; returns the cache
void  atlasCacheAddRef();                                  // vslot +0x00 - addRef-first table
bool  atlasCacheHasAtlases();                              // the byte at cache+0x18
bool  atlasCacheLoadAtlases(const std::string& prefix);
// sub_10017440 - first atlas whose map holds `name`; returns the 20-byte entry or null.
struct TextureAtlasEntry;
TextureAtlasEntry* atlasCacheFind(const std::string& name);
// The cache vtable's slot +0x08 (0x10017B50) - release; destroys the cache (and clears the global)
// at zero. Returns the remaining count, which is what Meme slot +0x60 keys on.
int   atlasCacheRelease();

// The shipped path/string helpers (bodies at 0x10074D10 / 0x10075730 / 0x10075A50 / 0x10075580;
// defined in TextureManager.cpp, used by the atlas and the Meme texture as well).
void pathToLower(std::string& s);                             // tolower + the five Swedish maps
void pathReplaceSeparators(std::string& s, char with);        // '/' and '\\' -> `with`
void pathStripExtension(std::string& out, const std::string& in);
void pathGetExtension(std::string& out, const std::string& in);
// [CHOSEN] rebuild seam: std::string publishes no (ptr, len) assign, so substring builders route
// through this. Behaviour, not ABI - the original inlines MSVCP71's substr.
void pathAssignSubstring(std::string& out, const char* s, unsigned int len);

// Default priority assigned to new textures; atlas loading temporarily uses priority 15.
int  resourceManagerSwapDefaultPriority(int value);
int  resourceManagerDefaultPriority();
// The byte at RM+0x04 (sub_1000D950) - 1 defers file reads during init, 0 after step 16.
bool resourceManagerIsReady();

// ShaderManager.cpp - what the effect compiler reads out of it. See those bodies for the offsets.
struct ShaderManagerDefines
{
    std::string* first;
    unsigned int  count;      // elements, not pairs - the vector alternates name, value, name, ...
    const char*   at(unsigned int i) const;
};
ShaderManagerDefines shaderManagerDefines(void* shaderManager);
void*         shaderManagerInclude(void* shaderManager);
unsigned long shaderManagerEffectFlags(void* shaderManager);
void*         renderDevice();
void*         compileEffectFromFile(const char* rawPath);   // Effect.cpp, sub_1001E010
void*         shaderManagerGetEffect(void* shaderManager, const char* rawPath);
class Effect;
class ShaderParameter;
void*         effectGetShaderParameter(Effect* effect, const std::string& name);
ShaderParameter* effectGetShaderParameterTyped(Effect* effect, const std::string& name);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect* effect,
                                                       const std::string& name);
void          effectAddRef(Effect* effect);
int           effectRelease(Effect* effect);
// Clears every cached effect's registered texture bindings during level teardown.
void          shaderManagerClearEffectTextureBindings(void* shaderManager);
void          effectClearTextureParameters(Effect* effect);
bool          effectSetTechniqueByName(void* effect, const std::string& name);
// BufferManager.cpp, sub_10012260 plus the inline release/0x10014790 tail.  Mesh-renderer
// constructors own these wrappers exactly like Effect's annotation-driven declarations do.
void*         vertexDeclarationCreate(const void* elements);
void          vertexDeclarationRelease(void* wrapper);
// These three calls form the shadow
// submission bracket: begin the selected technique, begin one pass, and end
// the effect (which also closes the active pass in the shipped wrapper).
unsigned int  effectBegin(void* effect);
void          effectBeginPass(void* effect, unsigned int index);
void          effectEnd(void* effect);
// Effect.cpp - sub_1001DC20: a cached technique record with a live D3DX handle exists for `name`.
bool          effectHasTechnique(void* effect, const std::string& name);
// Effect.cpp - sub_1001DB00: the effect's cached int parameter "ShaderVersion" (0 when absent).
int*          effectShaderVersion(void* effect);

// StateManager.cpp - narrow, typed forms of the ordinary deferred-state slots used by the
// LightManager shadow pass.  These are deliberately distinct from
// stateManagerSetRenderStatePersistent(): PE 0x101679A0/0x10160D50 dispatch through the normal
// +0x1C setter and let the next flush issue the state.
void stateManagerPush();
void stateManagerPop();
void stateManagerSetRenderState(unsigned int state, unsigned int value);
// sub_10025BA0 - the persistent set: device mirror AND base value, issued at once (StateManager.cpp).
void stateManagerSetRenderStatePersistent(unsigned int state, unsigned int value);
// sub_10026880 - the pre-draw apply, published for the Ra shader's draw (RaShader::draw calls it
// on the state manager before the device draw).  StateManager.cpp.
void stateManagerPreDrawApply();
// sub_10022420 - ShaderManager +0xA0, the active Ra effect the pre-draw apply commits first.
// RaShader::begin installs it, RaShader::end clears it.  ShaderManager.cpp.
void shaderManagerSetActiveRaEffect(void* effect);
void stateManagerSetTransparencyAA(bool enabled);
void stateManagerSetRenderTarget(unsigned int index, IDirect3DSurface9* surface);
// StateManager +0x1E90, the desired render-target-0 mirror (no AddRef). [CHOSEN, not recovered]
// typed seam for readers that took it by raw offset (issue #7).
IDirect3DSurface9* stateManagerRenderTarget0();
// StateManager +0x04, its DeviceLostResetListener subobject (dword_1023B984 + 4 on x86).
DeviceLostResetListener* stateManagerDeviceLostListener();
void stateManagerSetDepthStencilSurface(IDirect3DSurface9* surface);
void stateManagerSetIndices(IDirect3DIndexBuffer9* indices);
void stateManagerSetStreamSource(unsigned int stream, IDirect3DVertexBuffer9* vertexBuffer,
                                 unsigned int offset, unsigned int stride);
void stateManagerSetVertexDeclaration(IDirect3DVertexDeclaration9* declaration);
// the +0x34 SetVertexShader (0x10025BF0) and +0x44 SetPixelShader (0x10025C20)
// slots as owner-TU seams, like the three dice binding slots above. Their one new caller is the
// display-mode render probe sub_10009FF0 (RendDX9Init.cpp), which binds the shaders it assembles.
void stateManagerSetVertexShader(IDirect3DVertexShader9* shader);
void stateManagerSetPixelShader(IDirect3DPixelShader9* shader);
void stateManagerClear(unsigned int flags, unsigned int colour, float depth,
                       unsigned int stencil);
void flushRenderTargetsSeam();
// StateManager.cpp - dword_1023D08C and the two 4-byte codes at the
// transparency-antialiasing render-state index and its on / off values. ONE set of globals with
// four readers in the image - the StateManager's issuer and reset (0x10025FBB, 0x10027618),
// renderFrame's atoc brackets (0x1003027D..0x10030388), ShaderManager::updateShaderDefines' "%X"
// pair (0x10023BE7 / 0x10023C3C) and the Ra EffectManager's A2MEnable / A2MDisable defines
// (0x100333A5 / 0x100333CF) - and ONE writer, the transparency-AA probe sub_1000A3E0
// (RendDX9Init.cpp). .bss, so all three read zero until that probe runs.
extern unsigned int  g_transparencyAAState;
extern unsigned char g_transparencyAAOn[4];
extern unsigned char g_transparencyAAOff[4];

// BF2Render.cpp, typed seams for Renderer methods owned by that TU.
class IRenderView;
IRenderView* rendererGetRenderView();
IRenderView* rendererGetRenderViewDup();
IVideoOptions* rendererGetVideoOptions();
// [CHOSEN, not recovered] Fields of the Renderer's unrecovered tail that other subsystems read and
// write, named through BF2Render.cpp rather than by raw byte offset (issue #7). The x86 Renderer
// keeps the retail layout, but the x64 one is native, so a raw `g_Renderer + offset` is wrong there.
// Each accessor names its field by the retail x86 offset and maps it onto the named member; an
// offset (or a value type) that names no mapped field stops with a [FRONTIER] diagnostic instead
// of reading a guess. Each requires the published Renderer (g_Renderer == g_rendererBase).
//
// The per-pass render switch byte at retail `retailOffset` (+0x69C..+0x6AB and +0x6B0..+0x6B7;
// +0x69D the world/object pass, +0x6A4 draws roads, +0x6B1 is renderer.drawHud).
unsigned char&     rendererRenderSwitch(unsigned int retailOffset);
// Any mapped byte, 32-bit integer, float or double field of the tail, by retail offset. The float
// tables (+0x70C/+0x71C/+0x72C level-of-detail distances, the FPS histogram) are addressed per
// element: `rendererTailFloat(0x72C + 4 * i)`.
unsigned char&     rendererTailByte(unsigned int retailOffset);
int&               rendererTailInt(unsigned int retailOffset);
float&             rendererTailFloat(unsigned int retailOffset);
double&            rendererTailDouble(unsigned int retailOffset);
unsigned int       rendererRaPath();                    // +0x6CC, the RAPATH shader define
void*              rendererFrameTimer();                // +0x5DC, the 48-byte QPC frame timer
IDirect3DSurface9* rendererBackBufferSurface();         // +0x76C, the device back buffer reference
IDirect3DSurface9* rendererDepthStencilSurface();       // +0x770, the depth-stencil reference
// The render-path target texture at retail `retailOffset` (+0x778..+0x7D4).
EngineTexture*     rendererPathTarget(unsigned int retailOffset);
// The Renderer's RenderPathListener subobject (retail +0x540).
RenderPathListener* rendererRenderPathListener();

// PE HemiMapManager::makeHemiMap collaborators.
IRenderView* rendererGetHemiPrimaryView();
IRenderView* rendererGetHemiSecondaryView();
void rendererInstallHemiView(IRenderView* view);
void rendererRestoreHemiViews(IRenderView* primary, IRenderView* secondary);
void rendererSetHemiTarget(EngineTexture* texture);
EngineTexture* rendererGetHemiScratchTexture();
void rendererSetHemiScratchTexture(EngineTexture* texture);
int rendererGetHemiRenderMode();
void rendererSetHemiRenderMode(int mode);
void rendererSetHemiRoadPass(bool enabled);
void rendererRenderHemiFrame();
void rendererPresentHemiResult();
float rendererSwapMinCullDistance(float value);
void rendererCaptureHemiState(HemiRenderState& state);
void rendererApplyHemiState();
void rendererRestoreHemiState(const HemiRenderState& state);
void postProductionGetHemiState(float outState[4]);
void postProductionSetHemiState(const float state[4]);

// Ra/EffectManager.cpp and RenderQueues.cpp.  makeHemiMap changes the effect channel between its
// terrain and static-geometry renders, then invalidates every queue effect exactly as retail does.
void effectManagerSetShowChannel(unsigned int channel);
// Rebuilds effect-manager shader defines after resolution changes.
void effectManagerRebuildShaderDefines();


// [CHOSEN, not recovered] Reapplies the current display mode without copying its native
// carrier onto the incompatible internal layout. Returns the reset/loss-loop result.
bool  rendererApplyCurrentDisplayMode(void* renderer);
// Draws a probe triangle and scans rect (or the full surface) for a nonblack RGB value
// outside [colourLow, colourHigh]. Used by multisample and transparency-AA probes.
bool  rendererRenderProbe(void* renderer, const char* vertexShader, const char* pixelShader,
                          unsigned int colourLow, unsigned int colourHigh, const float* rect,
                          const char* dumpFile);
void  rendererProbeMultisample(void* renderer);
void  rendererProbeTransparencyAA(void* renderer);

// The metaFx cache, It hangs off the Ra EffectManager
// (`dword_1023D954`) at +0x54, NOT off the ShaderManager - both shipped call sites, 0x10115D08 and
// 0x10192204, load `dword_1023D954` straight into ecx in the instruction before the call. Neither
// guards the result, and neither does this seam.
namespace Ra { class MetaFx; }
Ra::MetaFx* effectManagerGetMetaFx(const std::string& name);

void renderQueuesResetEffects();
void renderQueuesFlushWorldPass();
bool  readBoolSetting(const char* name, bool defaultValue);
// Settings vslot +0x14 - the setter twin of readIntSetting's +0x10 getter.
void  writeIntSetting(const char* name, int value);
bool  readBoolSetting(const char* name, bool defaultValue);
void  buildPresentParameters(void* renderer);
bool  createPrimaryD3DDevice(void* renderer);
bool  createD3DDeviceAndSwapChains(void* renderer);
bool  createDeviceCaps(void* renderer);
bool  createD3DDevice(void* renderer);
bool  initializeD3DSubsystems(void* renderer);

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_RENDDX9_RENDDX9BASE_H
