// Layout view over renderer storage, not a C++ base class. A second vptr would shift fields.
// [RETIRE-WHEN: RW-56] Replace the overlay after the base vtable is fully typed and
// inheritance preserves the validated offsets.


#ifndef BF2_RENDDX9_RENDDX9BASE_H
#define BF2_RENDDX9_RENDDX9BASE_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>

#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/DisplayModeAbi.h"

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
    virtual vc71::string getRenderPathListenerName() const = 0;
};

#if defined(_M_IX86)
static_assert(sizeof(DeviceLostResetListener) == 4,
              "device-lost listener is one three-slot vptr");
static_assert(sizeof(RenderPathListener) == 4,
              "render-path listener is one six-slot vptr");
#endif

// HemiMapManager.cpp's editor/offline bake is deliberately routed through typed owner-TU seams.
// The renderer owns all of the byte-offset state below; keeping that knowledge in BF2Render.cpp
// prevents the recovered Hemi class from becoming another raw-vtable/offset facade.
struct HemiRenderState
{
    unsigned char bytes[0x2C];
};


// The display-mode records the enumeration subsystem builds. The container everywhere here is the
// shipped 16-byte dice array (vc71::vector_layout: alloc@+0, begin@+4, end@+8, cap@+12), driven
// through vc71::vectorRef<T>.


// An 8-byte multisample descriptor from CheckDeviceMultiSampleType: the D3DMULTISAMPLE_TYPE and the
// (quality-count - 1). Held per-mode and, parallel to the AA string list, inside VideoOptions.
typedef renddx9abi::DisplayModeMultiSample MultiSample8;

// Clears an owned fixed-layout vector: destroy each element, release its operator-new buffer
// through the shared native CRT, and null the pointer triple.
template <typename T>
inline void vectorClear(vc71::vector_layout* v)
{
    if (v->first)
    {
        for (T* p = reinterpret_cast<T*>(v->first); p != reinterpret_cast<T*>(v->last); ++p)
            p->~T();
        ::operator delete(v->first);
    }
    v->first = 0;
    v->last  = 0;
    v->end   = 0;
}

// The 40-byte display-mode record (sub_1000CEE0 header + the two nested lists sub_100091E0 deep-copies
// and sub_10009510 frees). Value semantics: copy deep-copies both nested lists so a Mode40 can live in
// a vc71::vectorRef<Mode40>; the dtor frees them.
struct Mode40
{
    /* +0  */ unsigned short      width;
    /* +2  */ unsigned short      height;
    /* +4  */ unsigned short      refresh;
    /* +6  */ unsigned char       bpp;
    /* +7  */ unsigned char       pad;
    /* +8  */ vc71::vector_layout depthFormats;   // list<dword>        - CheckDeviceFormat results
    /* +24 */ vc71::vector_layout multiSample;    // list<MultiSample8> - CheckDeviceMultiSampleType

    Mode40()
    {
        width = 0; height = 0; refresh = 0; bpp = 0; pad = 0;
        depthFormats.alloc = 0; depthFormats.first = 0; depthFormats.last = 0; depthFormats.end = 0;
        multiSample.alloc  = 0; multiSample.first  = 0; multiSample.last  = 0; multiSample.end  = 0;
    }
    Mode40(const Mode40& o)
    {
        width = o.width; height = o.height; refresh = o.refresh; bpp = o.bpp; pad = 0;
        depthFormats.alloc = 0; depthFormats.first = 0; depthFormats.last = 0; depthFormats.end = 0;
        multiSample.alloc  = 0; multiSample.first  = 0; multiSample.last  = 0; multiSample.end  = 0;
        vc71::vectorRef<unsigned int> d(&depthFormats);
        for (const unsigned int* p = reinterpret_cast<const unsigned int*>(o.depthFormats.first);
             p != reinterpret_cast<const unsigned int*>(o.depthFormats.last); ++p)
            d.push_back(*p);
        vc71::vectorRef<MultiSample8> m(&multiSample);
        for (const MultiSample8* p = reinterpret_cast<const MultiSample8*>(o.multiSample.first);
             p != reinterpret_cast<const MultiSample8*>(o.multiSample.last); ++p)
            m.push_back(*p);
    }
    ~Mode40()
    {
        vectorClear<unsigned int>(&depthFormats);
        vectorClear<MultiSample8>(&multiSample);
    }
private:
    Mode40& operator=(const Mode40&);   // unused: vectorRef only copy-constructs
};

static_assert(sizeof(Mode40) == 40, "Mode40 is the shipped 40-byte display-mode record");
static_assert(sizeof(MultiSample8) == 8, "multisample descriptor is 8 bytes");

struct RendDX9Base
{
    /* 0x000 */ void*                  vptr;              // 0x101DDA38 in the shipped image
    /* 0x004 */ int                    refCount;          // base ctor: 1
    /* 0x008 */ IDirect3D9*            d3d9;              // Direct3DCreate9(0x20)
    /* 0x00C */ IDirect3DDevice9*      device;            // CreateDevice's out-parameter
    /* 0x010 */ unsigned char          opaque010[0x024 - 0x010];   // +0x01C is set to -65536

    // The present parameters live inside the object, at +0x24, and CreateDevice is handed `this+36`
    // directly. sub_1000A500 memsets exactly 0x38 bytes here, which is sizeof(D3DPRESENT_PARAMETERS)
    // and the check that the offset is right.
    /* 0x024 */ D3DPRESENT_PARAMETERS  pp;

    // Device-lost list: unused word, circular head, size, lost flag, sort-dirty flag.
    // Render-path list: active path at +0x00, head at +0x08, flag at +0x10.
    /* 0x05C */ unsigned char          deviceLostList[0x06C - 0x05C];
    /* 0x06C */ unsigned char          renderPathList[0x080 - 0x06C];

    /* 0x080 */ unsigned int           adapter;           // base ctor: 0, set in sub_1000B0D0
    /* 0x084 */ unsigned int           deviceType;        // GSDeviceType,     default 1
    /* 0x088 */ unsigned int           behaviorFlags;     // GSDeviceBehavior, default 80 (0x50)
    /* 0x08C */ unsigned int           opaque08C;
    // The display-mode list slot_0x3C (sub_10007420) builds and returns (as `this+0x90`): a
    // vc71::vector_layout of Mode40. Its begin is at +0x94, which is what sub_10007420 and its caller
    // sub_10010710 read.
    /* 0x090 */ vc71::vector_layout    modeList;

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
    /* 0x0AC */ void*                  depthFormatsAlloc;
    /* 0x0B0 */ unsigned int*          depthFormatsFirst;
    /* 0x0B4 */ unsigned int*          depthFormatsLast;
    /* 0x0B8 */ unsigned int*          depthFormatsEnd;

    // The multisample candidates, 8-byte entries. BF2 leaves this empty; VideoOptions' resolution
    // change (sub_1000F8D0, 0x1000F92C..0x1000F961) replaces it with the ONE selected AA descriptor,
    // which buildPresentParameters then hands to the Reset.
    /* 0x0BC */ void*                  multiSampleAlloc;
    /* 0x0C0 */ unsigned int*          multiSampleFirst;
    /* 0x0C4 */ unsigned int*          multiSampleLast;
    /* 0x0C8 */ unsigned int*          multiSampleEnd;

    /* 0x0CC */ unsigned char          vsync;
    /* 0x0CD */ unsigned char          padCD[3];

    // The window list. hDeviceWindow is *hwndFirst.
    /* 0x0D0 */ void*                  hwndAlloc;
    /* 0x0D4 */ void**                 hwndFirst;
    /* 0x0D8 */ void**                 hwndLast;
    /* 0x0DC */ void**                 hwndEnd;

    /* 0x0E0 */ unsigned char          opaque0E0[0x0F8 - 0x0E0];
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
    /* 0x102 */ unsigned char          opaque102[0x39C - 0x102];

    // Adapter identifiers populated during device creation; each raw D3D record is 1100 bytes.
    /* 0x39C */ void*                  adapterInfoAlloc;
    /* 0x3A0 */ D3DADAPTER_IDENTIFIER9* adapterInfoFirst;
    /* 0x3A4 */ D3DADAPTER_IDENTIFIER9* adapterInfoLast;
    /* 0x3A8 */ D3DADAPTER_IDENTIFIER9* adapterInfoEnd;

    // The Present flags byte present(mode 2) substitutes and the wasstilldrawing-retry byte, set
    // only by the `renderer.presentAsync` / `renderer.presentSpinIfBusy` console properties
    // (0x10009E30 / 0x10009E50; attested names from the bindings). Base ctor: both 0.
    /* 0x3AC */ unsigned char          presentAsync;
    /* 0x3AD */ unsigned char          presentSpinIfBusy;
    /* 0x3AE */ unsigned char          opaque3AE[0x3F8 - 0x3AE];

    // VideoOptions_ctor(this + 1016). 284 bytes, running to +0x513.
    /* 0x3F8 */ unsigned char          videoOptions[0x514 - 0x3F8];

    dice::hfe::IVideoOptions* embeddedVideoOptions()
    {
        return reinterpret_cast<dice::hfe::IVideoOptions*>(videoOptions);
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
    /* 0x518 */ unsigned char          opaque518[0x538 - 0x518];
};

static_assert(sizeof(D3DPRESENT_PARAMETERS) == 0x38,
              "sub_1000A500 memsets 0x38 bytes over the present parameters");
static_assert(sizeof(RendDX9Base) == 0x538,
              "the base object runs +0x000..+0x537; the derived vptrs start at +0x538");

// Every offset the device path depends on, checked rather than trusted.
static_assert(offsetof(RendDX9Base, d3d9)              == 0x008, "d3d9 at +0x08");
static_assert(offsetof(RendDX9Base, device)            == 0x00C, "device at +0x0C");
static_assert(offsetof(RendDX9Base, pp)                == 0x024, "present parameters at +0x24");
static_assert(offsetof(RendDX9Base, adapter)           == 0x080, "adapter at +0x80");
static_assert(offsetof(RendDX9Base, deviceType)        == 0x084, "deviceType at +0x84");
static_assert(offsetof(RendDX9Base, behaviorFlags)     == 0x088, "behaviorFlags at +0x88");
static_assert(offsetof(RendDX9Base, fullscreen)        == 0x0A0, "fullscreen at +0xA0");
static_assert(offsetof(RendDX9Base, modeWidth)         == 0x0A4, "width at +0xA4");
static_assert(offsetof(RendDX9Base, modeHeight)        == 0x0A6, "height at +0xA6");
static_assert(offsetof(RendDX9Base, modeRefresh)       == 0x0A8, "refresh at +0xA8");
static_assert(offsetof(RendDX9Base, modeBpp)           == 0x0AA, "bpp at +0xAA");
static_assert(offsetof(RendDX9Base, depthFormatsFirst) == 0x0B0, "depth _First at +0xB0");
static_assert(offsetof(RendDX9Base, multiSampleFirst)  == 0x0C0, "multisample _First at +0xC0");
static_assert(offsetof(RendDX9Base, vsync)             == 0x0CC, "vsync at +0xCC");
static_assert(offsetof(RendDX9Base, hwndFirst)         == 0x0D4, "HWND _First at +0xD4");
static_assert(offsetof(RendDX9Base, renderView)        == 0x0F8, "render view at +0xF8");
static_assert(offsetof(RendDX9Base, renderView2)       == 0x0FC, "second render view at +0xFC");
static_assert(offsetof(RendDX9Base, adapterInfoFirst)  == 0x3A0, "adapter list _First at +0x3A0");
static_assert(offsetof(RendDX9Base, videoOptions)      == 0x3F8, "VideoOptions at +0x3F8");
static_assert(offsetof(RendDX9Base, forceMultisample) == 0x514, "forced-MSAA byte at +0x514");
static_assert(offsetof(RendDX9Base, disableMultisample) == 0x515, "disabled-MSAA byte at +0x515");
static_assert(offsetof(RendDX9Base, hasAlphaToCoverage) == 0x516, "A2C capability byte at +0x516");
static_assert(offsetof(RendDX9Base, deviceLostList)    == 0x05C, "device-lost list at +0x5C");
static_assert(offsetof(RendDX9Base, inScene)           == 0x100, "in-scene byte at +0x100");
static_assert(offsetof(RendDX9Base, framePresented)    == 0x101, "presented byte at +0x101");
static_assert(offsetof(RendDX9Base, presentAsync)      == 0x3AC, "Present flags byte at +0x3AC");
static_assert(offsetof(RendDX9Base, presentSpinIfBusy) == 0x3AD, "WASSTILLDRAWING retry at +0x3AD");
static_assert(offsetof(RendDX9Base, appActive)         == 0x517, "application-active byte at +0x517");
static_assert(offsetof(D3DPRESENT_PARAMETERS, PresentationInterval) == 0x34,
              "the vsync setter writes base+0x58 == pp+0x34");
static_assert(sizeof(D3DADAPTER_IDENTIFIER9) == 1100,
              "sub_1000C920 strides by 1100 - the element IS the raw D3D structure");


// VideoOptions - 284 bytes, constructed inside the base object at +0x3F8 (1016) and reached through
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
    // The two dynamic-option string lists, 16-byte dice arrays of vc71::string. begin @0xB4/0xC4 is
    // exactly what setDynamicOption (sub_1000FB70) and the readback slot +0x20 (sub_10011350) read.
    /* 0x0B0 */ vc71::vector_layout aaList;       // anti-aliasing option strings ("Off", "%dSamples")
    /* 0x0C0 */ vc71::vector_layout resList;      // resolution strings ("%ix%i@%iHz")
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
    /* 0x0E0 */ vc71::vector_layout modeCache;     // list<Mode40>
    /* 0x0F0 */ vc71::vector_layout aaDescriptors; // list<MultiSample8>
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

static_assert(sizeof(VideoOptions) == 284,
              "VideoOptions occupies base+1016..base+1299 - the base ctor puts 1300 next");

void videoOptionsConstruct(void* base);  // 10010590, before any D3D initialization
void videoOptionsInitialize(void* base);


// hWndBf2TempSlt - the ABI-selected host carrier. Retail is the recovered 0x40-byte record;
// the rebuilt native pair uses current-toolset std::vectors. See DisplayModeAbi.h.

typedef renddx9abi::HostDisplayMode HWndBf2TempSlt;

// Implemented in RendDX9Init.cpp.
void  constructD3DHalf(void* renderer);
void  copyDisplayModeFrom(void* renderer, const HWndBf2TempSlt* src);
void  clearDisplayModeCopies(void* renderer);
void  renderPathListCtor(void* list);
void  setActiveRenderPath(void* list, int path);
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
void  readStringSetting(const char* name, vc71::string& out);
// Same Settings +0x40 getter with the caller-provided default preserved.  HemiMapManager uses the
// retail non-empty default " ERROR ", so routing it through readStringSetting's empty default
// would be observable on a missing key.
void  readStringSettingDefault(const char* name, const char* defaultValue, vc71::string& out);
// sub_100010D0 on dword_1023B980 - BYTE1 of D3DCAPS9.PixelShaderVersion (+0xCC), the PS major.
int   deviceCapsPixelShaderMajor();
// sub_100010B0 on dword_1023B980 - BYTE1 of D3DCAPS9.VertexShaderVersion (+0xC4), the VS major.
// Terrain::init picks the low render style when it is below 2.
int   deviceCapsVertexShaderMajor();
bool  deviceCapsIsAti();
bool  deviceCapsIsNvidia();
bool  deviceCapsSupportsTextureFormat(unsigned int format);
bool  deviceCapsSupportsFullSizeRenderTargets();


// TextureManager.cpp - the loader's cross-TU surface. The shipped callers reach these
// as __thiscall members on dword_1023B974 / direct calls; these seams carry the same behaviour.

// sub_10019260 - the by-name loader. Returns an EngineTexture* (never null once the manager
// exists: even a missing file yields the wantu.dds fallback wrapper), addRef'd for the caller.
void* textureManagerGetTexture(const vc71::string& name);
// sub_10017E00 / sub_10017E20 / sub_10017E40 - swap-style setters returning the OLD value.
// Consumers bracket loads with them: Meme forces class 1 + managed, Font::Ported forces managed=0
// for "_def", and Meme's createTexture forces recreate=1 so its render targets survive a reset.
int   textureManagerSwapDefaultMipClass(int value);
int   textureManagerSwapManaged(int value);
int   textureManagerSwapRecreateMode(int value);
// sub_10018550 - create a code-made 2D texture (depth 1) through the normal create path; `existing`
// non-null re-creates into that wrapper (the onDeviceReset arm).
void* textureManagerCreateEmpty(unsigned int w, unsigned int h, unsigned int format,
                                unsigned int mips, unsigned int usage, void* existing);
void* textureManagerCreateCube(unsigned int side, unsigned int format,
                               unsigned int mips, unsigned int usage, void* existing);
void* createRaShaderConstants(IDirect3DDevice9* device);
void  destroyRaShaderConstants();
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
bool  textureManagerSaveTexture(EngineTexture* texture, const vc71::string& name);
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
bool  atlasCacheLoadAtlases(const vc71::string& prefix);
// sub_10017440 - first atlas whose map holds `name`; returns the 20-byte entry or null.
struct TextureAtlasEntry;
TextureAtlasEntry* atlasCacheFind(const vc71::string& name);
// The cache vtable's slot +0x08 (0x10017B50) - release; destroys the cache (and clears the global)
// at zero. Returns the remaining count, which is what Meme slot +0x60 keys on.
int   atlasCacheRelease();

// The shipped path/string helpers (bodies at 0x10074D10 / 0x10075730 / 0x10075A50 / 0x10075580;
// defined in TextureManager.cpp, used by the atlas and the Meme texture as well).
void pathToLower(vc71::string& s);                             // tolower + the five Swedish maps
void pathReplaceSeparators(vc71::string& s, char with);        // '/' and '\\' -> `with`
void pathStripExtension(vc71::string& out, const vc71::string& in);
void pathGetExtension(vc71::string& out, const vc71::string& in);
// [CHOSEN] rebuild seam: vc71::string publishes no (ptr, len) assign, so substring builders route
// through this. Behaviour, not ABI - the original inlines MSVCP71's substr.
void pathAssignSubstring(vc71::string& out, const char* s, unsigned int len);

// Default priority assigned to new textures; atlas loading temporarily uses priority 15.
int  resourceManagerSwapDefaultPriority(int value);
int  resourceManagerDefaultPriority();
// The byte at RM+0x04 (sub_1000D950) - 1 defers file reads during init, 0 after step 16.
bool resourceManagerIsReady();

// ShaderManager.cpp - what the effect compiler reads out of it. See those bodies for the offsets.
struct ShaderManagerDefines
{
    vc71::string* first;
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
void*         effectGetShaderParameter(Effect* effect, const vc71::string& name);
ShaderParameter* effectGetShaderParameterTyped(Effect* effect, const vc71::string& name);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect* effect,
                                                       const vc71::string& name);
void          effectAddRef(Effect* effect);
int           effectRelease(Effect* effect);
// Clears every cached effect's registered texture bindings during level teardown.
void          shaderManagerClearEffectTextureBindings(void* shaderManager);
void          effectClearTextureParameters(Effect* effect);
bool          effectSetTechniqueByName(void* effect, const vc71::string& name);
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
bool          effectHasTechnique(void* effect, const vc71::string& name);
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
// [CHOSEN, not recovered] owner-TU access to concrete Renderer +0x6CC, the mapped shader-path
// index emitted as RAPATH. It is not the independent GSRenderPath listener selection. Callers
// preserve their original null handling.
int rendererGetShaderPath(const void* renderer);
// PE 0x100286C0 stores the path and immediately refreshes Renderer shader defines.
void rendererSetShaderPathAndUpdate(void* renderer, int path);

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
Ra::MetaFx* effectManagerGetMetaFx(const vc71::string& name);

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
