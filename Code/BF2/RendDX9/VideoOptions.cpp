// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\RendDX9\VideoOptions.cpp"
//           appears verbatim in RendDX9.dll at 0x101DDEB8, and initialize quotes it with line 129,
//           "Failed to register VideoOptions as event handler for ECCore."
// Contents: recovered from RendDX9.dll. The data initialisation is complete; three trailing calls
//           are named gaps, and the note below says exactly what each would have changed.

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Geom/XpLightManager.h"
#include "../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../Geom/UndergrowthSystem.h"   // typed vegetation quality calls
#include "RendDX9Base.h"
#include "../RendMisc/IRenderer.h"     // sub_1000F8D0's typed Renderer calls
#include "../RendMisc/IRenderView.h"   //   (+0x10 setDisplayMode, +0x48 / +0x50 and the views' +0x40 / +0x10)
#include "../Core/EventManager.h"      // sub_1000F8D0's two ECRenderer posts
#include <cstddef>      // offsetof - the retail-ABI display-block assertions in sub_1000F8D0
#include <intrin.h>     // __rdtsc / __cpuid - the CPU-speed benchmark inside detectQualityLevel (+0x34)
#include <cstdio>       // _snprintf - the "%ix%i@%iHz" / "%dSamples" formatters
#include <cstdlib>
#include <cstring>      // _strnicmp - setDynamicOption's prefix match
#if BF2_PLUGIN_ABI_NATIVE
#include <string>       // the host-shaped dynamic-option carriers, below
#include <vector>
#else
#include <type_traits>  // the retail-flavour assertion on HostString, below
#endif

namespace dice {
namespace hfe {
namespace dx9rend {


// Cross-image dynamic options use HostString while internal storage keeps VC7.1 layout.
// [CHOSEN, not recovered] [RETIRE-WHEN: RW-49] Native getters return conversion caches.
// References last until the next getter call; use only on the render thread. Retire the
// caches when storage directly matches the host ABI and reference lifetimes are validated.

#if BF2_PLUGIN_ABI_NATIVE
typedef std::vector< ::renddx9abi::HostString > HostDynamicOptionList;
#else
typedef ::vc71::vector_layout HostDynamicOptionList;
// The retail arms of the three slots hand back this object's own storage unchanged, which is only
// correct while the host's string IS the string these lists store. If that ever stops being true
// this fails here rather than four bytes into a stranger's buffer.
static_assert(std::is_same< ::renddx9abi::HostString, ::vc71::string>::value,
              "the retail-ABI host string must be the VC7.1 string the dynamic-option lists store");
#endif

// Plugin-side typed view of the hand-built 15-slot table.  It deliberately selects the string and
// vector carrier for the ABI flavour: current STL for the rebuilt native pair, VC7.1 layouts for
// the retail-compatible DLL.  Keeping this view here lets internal virtual calls retain the PE's
// dispatch order without casting vtable entries or accidentally passing a VC7.1 string to a native
// std::string slot.
class PluginVideoOptions
{
public:
    virtual bool setQuality(int index, int value) = 0;                                  // +0x00
    virtual int getQuality(int index) = 0;                                               // +0x04
    virtual int getMinQuality(int index) = 0;                                            // +0x08
    virtual int getMaxQualityLevel(int index) = 0;                                       // +0x0C
    virtual int getMaxQuality(int index) = 0;                                            // +0x10
    virtual bool getRestartFlag(int index) = 0;                                          // +0x14
    virtual const HostDynamicOptionList& getDynamicOptionList(int index) = 0;             // +0x18
    virtual bool setDynamicOption(int index, const ::renddx9abi::HostString& value) = 0;  // +0x1C
    virtual const ::renddx9abi::HostString& getDynamicOption(int index) = 0;              // +0x20
    virtual void setViewDistanceScale(float value) = 0;                                  // +0x24
    virtual float getViewDistanceScale() = 0;                                             // +0x28
    virtual void apply() = 0;                                                             // +0x2C
    virtual int applyQualityPreset(int preset) = 0;                                       // +0x30
    virtual int detectQualityLevel() = 0;                                                 // +0x34
    virtual bool getPerGpuFlag() = 0;                                                     // +0x38
};

#if BF2_PLUGIN_ABI_NATIVE
static ::renddx9abi::HostString s_nativeOption[2];
static HostDynamicOptionList    s_nativeOptionList[2];
#endif

// The globals apply reaches. Each is defined where its class lives; declared here rather than
// pulled in through a header because these are image globals, not an interface.
extern IBase* g_Renderer;                    // BF2Render.cpp,   0x1023B96C
extern void*  g_textureManager;              // TextureManager.cpp, dword_1023B974
namespace Ra {
extern void* g_effectManager;
void effectManagerResetEffects();
}
extern void* g_shaderManager;
void updateShaderDefines(void* shaderManager, void* base);
void shaderManagerReloadEffects(void* shaderManager);
void textureManagerSweep();
void resourceManagerSetReady(unsigned char value);

// BF2Render.cpp, dword_10258F38. The ECCore handler's case 2 publishes the level-scoped
// OverGrowth owner. apply changes its recovered +0xF4 fog/view-distance scalar when present.
extern void* g_OverGrowth;

} // namespace dx9rend

namespace geom {
void geometryTemplateManagerClear();
void geometryTemplateManagerRearmTechniques();
extern LightManager* g_LightManager;
extern IBase* g_ParticleSystemManager;
extern IBase* g_DecalManager;
extern IBase* g_RoadCompiledRenderer;
extern IBase* g_SkyDome;
extern IBase* g_UndergrowthSystem;
// Applying video settings dirties the terrain's lighting constants.
extern IBase* g_Terrain;
} // namespace geom

namespace dx9rend {


extern void* g_deviceCaps;
extern void* g_rendererBase;   // RendDX9Init.cpp - dword_1023B970, the base renderer (has the mode list)
void* rendererGetModeList(void* base);   // BF2Render.cpp - builds (once) and returns the base's Mode40 list
int  readIntSetting(const char* name, int defaultValue);

namespace {

struct CapsView { D3DCAPS9 caps; D3DADAPTER_IDENTIFIER9 adapter; };

const CapsView* capsOf()     { return static_cast<const CapsView*>(g_deviceCaps); }
unsigned int vsMajor()       { const CapsView* c = capsOf(); return c ? ((c->caps.VertexShaderVersion >> 8) & 0xFF) : 0u; }
unsigned int psMajor()       { const CapsView* c = capsOf(); return c ? ((c->caps.PixelShaderVersion  >> 8) & 0xFF) : 0u; }
unsigned int maxAnisotropy() { const CapsView* c = capsOf(); return c ? c->caps.MaxAnisotropy : 0u; }

} // anonymous namespace


// Video-quality interface embedded in the renderer at its asserted +0x3F8 offset.

namespace {

// MSVC will not let a free function be __thiscall; __fastcall with an unused EDX is the same
// convention for these slots, whose arguments are all stack-passed after `this`.

// +0x04, sub_100112B0, read in full: `return this[index + 10]`. Dword index 10 is byte offset
// 0x28, which is row B - the only row anything recovered reads.
int __fastcall videoOptions_getQuality(void* self, void*, int index)
{
    const VideoOptions* vo = static_cast<const VideoOptions*>(self);
    if (index < 0 || index > 7)
        return 0;               // the original does not range-check; nothing recovered calls it out of range
    return vo->quality[index];
}


// Applies differences between requested and applied video settings.
// Requires the render subsystems, including the compiled-road renderer, to exist.
// Applying texture quality also writes GSSkipMipLevels back to the settings repository.

namespace {

// clamp(v - 1, 0, 1), the shape the original open-codes five times in a row.
inline int clampMinusOne(int v)
{
    const int t = v - 1;
    if (t < 0) return 0;
    return t > 1 ? 1 : t;
}

// The Renderer fields apply writes. They live inside BF2Render.cpp's opaque tail rather than being
// named members, so they are reached by documented raw offset - the same treatment RendDX9Init.cpp
// gives the base object.
inline void writeRendererInt(void* renderer, unsigned offset, int value)
{
    *reinterpret_cast<int*>(static_cast<unsigned char*>(renderer) + offset) = value;
}
inline void writeRendererFloat(void* renderer, unsigned offset, float value)
{
    *reinterpret_cast<float*>(static_cast<unsigned char*>(renderer) + offset) = value;
}
inline void writeRendererByte(void* renderer, unsigned offset, unsigned char value)
{
    *(static_cast<unsigned char*>(renderer) + offset) = value;
}

// sub_100278A0: `this->vslot_0xC4()`, which is a one-line forward to
// ShaderManager::updateShaderDefines(g_shaderManager, base). Written already, so this is real.
void rendererUpdateShaderDefines(void* renderer)
{
    if (!renderer)
        return;
    void** vtbl = *reinterpret_cast<void***>(renderer);
    typedef void (__thiscall* Fn)(void*);
    reinterpret_cast<Fn>(vtbl[0xC4 / 4])(renderer);
}

// A virtual call with one integer argument, on an object reached through a raw vtable offset.
inline void callSlotInt(void* obj, unsigned slot, int arg)
{
    void** vtbl = *reinterpret_cast<void***>(obj);
    typedef void (__thiscall* Fn)(void*, int);
    reinterpret_cast<Fn>(vtbl[slot / 4])(obj, arg);
}
inline void callSlotFloat(void* obj, unsigned slot, float arg)
{
    void** vtbl = *reinterpret_cast<void***>(obj);
    typedef void (__thiscall* Fn)(void*, float);
    reinterpret_cast<Fn>(vtbl[slot / 4])(obj, arg);
}

#if !BF2_PLUGIN_ABI_NATIVE
// The retail-ABI host carrier IS the layout of the renderer's own display-mode block at base+0xA0
// (sub_1000CA40 copies one onto the other member for member), which is what lets sub_1000F8D0 hand
// that block to Renderer +0x10 as a carrier. Held here rather than trusted.
static_assert(offsetof(RendDX9Base, modeWidth) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, width), "block width at +0x04");
static_assert(offsetof(RendDX9Base, modeHeight) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, height), "block height at +0x06");
static_assert(offsetof(RendDX9Base, modeRefresh) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, refreshHz), "block refresh at +0x08");
static_assert(offsetof(RendDX9Base, modeBpp) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, bpp), "block bpp at +0x0A");
static_assert(offsetof(RendDX9Base, depthFormatsAlloc) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, depthFormats), "block depth list at +0x0C");
static_assert(offsetof(RendDX9Base, multiSampleAlloc) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, multiSamples), "block multisample list at +0x1C");
static_assert(offsetof(RendDX9Base, vsync) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, vsync), "block vsync at +0x2C");
static_assert(offsetof(RendDX9Base, hwndAlloc) - offsetof(RendDX9Base, fullscreen) ==
              offsetof(::renddx9abi::HostDisplayMode, windows), "block window list at +0x30");
#endif


// sub_10009250 on base+0xA4 - the Mode40 copy-assignment, with the renderer's own record on the left
// (base+0xA4 is laid out as a Mode40: width +0xA4, height +0xA6, refresh +0xA8, bpp +0xAA, the depth
// list at +0xAC, the multisample list at +0xBC). The four scalars (NOT the pad byte), then each list
// assigned from the source's range - VC7.1 `assign(first, last)`: erase everything, insert the range
// at begin (0x10008A00 / 0x10008C30). [CHOSEN] the source-level assignment; the tree's shared
// display-vector helper replaces the storage rather than reusing it, which nothing can observe.

void assignRendererModeRecord(RendDX9Base* b, const Mode40& src)
{
    b->modeWidth   = src.width;
    b->modeHeight  = src.height;
    b->modeRefresh = src.refresh;
    b->modeBpp     = src.bpp;
    ::renddx9abi::assignOwnedDisplayVector<unsigned int>(
        *reinterpret_cast<vc71::vector_layout*>(&b->depthFormatsAlloc),
        reinterpret_cast<const unsigned int*>(src.depthFormats.first),
        reinterpret_cast<const unsigned int*>(src.depthFormats.last));
    ::renddx9abi::assignOwnedDisplayVector<MultiSample8>(
        *reinterpret_cast<vc71::vector_layout*>(&b->multiSampleAlloc),
        reinterpret_cast<const MultiSample8*>(src.multiSample.first),
        reinterpret_cast<const MultiSample8*>(src.multiSample.last));
}

// Applies a display mode and selected antialiasing settings. [INFERRED] helper name.
// Requires initialized renderer services, a depth format and valid selected mode/AA indices.
// Window size/refresh settings can override the mode. Resize precedes device reset.
// Resolution events, capability probes and shader-define rebuild still run if reset fails.
void videoOptionsChangeResolution(VideoOptions* vo, const Mode40* mode)
{
    RendDX9Base* const b = static_cast<RendDX9Base*>(g_rendererBase);

    // 1 - 0x1000F8DE..0x1000F8E4 `mov eax, [esi+0B0h]; mov eax, [eax]` unguarded.
    const unsigned int keptDepthFormat = *b->depthFormatsFirst;

    // 2 - 0x1000F8FC `call sub_10009250` with ecx = base+0xA4.
    assignRendererModeRecord(b, *mode);

    // 3 - free and null the triple (VC7.1 clear() is _Tidy), then
    //     sub_10009070 = push_back. The chosen mode's own depth candidates are discarded.
    vc71::vector_layout& depth = *reinterpret_cast<vc71::vector_layout*>(&b->depthFormatsAlloc);
    vectorClear<unsigned int>(&depth);
    vc71::vectorRef<unsigned int>(&depth).push_back(keptDepthFormat);

    // 4 - the same on the multisample list, then sub_100090E0 =
    //     push_back(aaDescriptors[selected AA]). The index (+0xD8) is not range-checked.
    vc71::vector_layout& multiSample =
        *reinterpret_cast<vc71::vector_layout*>(&b->multiSampleAlloc);
    vectorClear<MultiSample8>(&multiSample);
    vc71::vectorRef<MultiSample8>(&multiSample).push_back(
        reinterpret_cast<const MultiSample8*>(vo->aaDescriptors.first)[vo->minusOneD8]);

    // 5 - 0x1000F966..0x1000F9A0: 16-bit stores of the low halves.
    if (vo->windowSizeX != 0 && vo->windowSizeY != 0)
    {
        b->modeWidth  = static_cast<unsigned short>(vo->windowSizeX);  // `mov cx, word [eax+104h]`
        b->modeHeight = static_cast<unsigned short>(vo->windowSizeY);  // `mov dx, word [eax+108h]`
    }
    if (vo->refreshHz != 0)
        b->modeRefresh = static_cast<unsigned short>(vo->refreshHz);    // `mov ax, word [eax+10Ch]`

    // 6 - SettingsRepostitory +0x14, each value the zero-extended word.
    writeIntSetting("GSWindowSizeX", b->modeWidth);
    writeIntSetting("GSWindowSizeY", b->modeHeight);

    // 7 - 0x1000FA05..0x1000FAB5. The quotient is formed before the g_Renderer test, as shipped.
    const float height = static_cast<float>(b->modeHeight);            // fild, fst [ebp-4]
    const float width  = static_cast<float>(b->modeWidth);             // fild, fst [ebp+8]
    const float aspect = height / width;                               // fdiv, fstp [ebp-8]
    if (g_Renderer)                                                     // dword_1023D098
    {
        // The Renderer global is re-read and the view re-fetched for every call, and the views
        //   are not null-tested - unlike setDisplayMode's own three.
        static_cast<IRenderer*>(g_Renderer)->getRenderView()->setAspect(aspect);        // +0x48, +0x40
        static_cast<IRenderer*>(g_Renderer)->getRenderView()->setViewport(
            0.0f, 0.0f, width, height);                                                 // +0x48, +0x10
        static_cast<IRenderer*>(g_Renderer)->getRenderViewDup()->setAspect(aspect);     // +0x50, +0x40
        static_cast<IRenderer*>(g_Renderer)->getRenderViewDup()->setViewport(
            0.0f, 0.0f, width, height);                                                 // +0x50, +0x10
    }

    // 8 - the three metrics in this order, then the resize. The
    //     sizing frame is added whatever the window's style (a fixed-frame or fullscreen popup
    //     window gets the same arithmetic), and hWndInsertAfter is 0 (HWND_TOP) without
    //     SWP_NOZORDER, so the window is also raised - as shipped.
    const int sizeFrameX = ::GetSystemMetrics(SM_CXSIZEFRAME);          // 0x20
    const int windowWidth = b->modeWidth + sizeFrameX * 2;
    const int sizeFrameY = ::GetSystemMetrics(SM_CYSIZEFRAME);          // 0x21
    const int caption    = ::GetSystemMetrics(SM_CYCAPTION);            // 4
    const int windowHeight = caption + sizeFrameY * 2 + b->modeHeight;
    ::SetWindowPos(static_cast<HWND>(*b->hwndFirst), 0, 0, 0, windowWidth, windowHeight,
                   SWP_NOMOVE);                                         // window list unguarded

    // 9 - EventManager (PE +0x10) postEvent(11, 3, 0, 0.0f). With no delay
    //     and no dispatch in progress the host delivers it at once, to every category-11 handler.
    ::dice::hfe::eventManager->postEvent(static_cast<EventCategory>(11), 3, 0, 0.0f);

    // 10 - 0x1000FB0E..0x1000FB17: `mov ecx, dword_1023B970; push esi (= base+0xA0); call [eax+10h]`.
#if BF2_PLUGIN_ABI_NATIVE
    // [CHOSEN, not recovered] The native host carrier is not the block's type; the retail call's
    // copy is a self-assignment that leaves the block unchanged (BF2Render.cpp), so this runs the
    // rest of +0x10 without one. Its answer is discarded, as the virtual call's is.
    rendererApplyCurrentDisplayMode(g_rendererBase);
#else
    static_cast<IRenderer*>(g_rendererBase)->setDisplayMode(
        *reinterpret_cast<const ::renddx9abi::HostDisplayMode*>(&b->fullscreen));   // bool dropped
#endif

    // 11 - 0x1000FB1A..0x1000FB3B: `shl ecx, 10h; add ecx, edx` into a stack dword whose address is
    //      the payload. Unsigned only so a width above 0x7FFF cannot overflow a signed shift.
    unsigned int packedSize = (static_cast<unsigned int>(b->modeWidth) << 16) + b->modeHeight;
    ::dice::hfe::eventManager->postEvent(static_cast<EventCategory>(11), 2, &packedSize, 0.0f);

    // 12 - 0x1000FB3E..0x1000FB4F, both on dword_1023B970.
    rendererProbeMultisample(g_rendererBase);
    rendererProbeTransparencyAA(g_rendererBase);

    // 13 - sub_10033240 on dword_1023D954.
    effectManagerRebuildShaderDefines();
}

} // anonymous namespace

// Applies quality state internally, including during the level-load notification.
// Public apply has a separate loaded-game refusal; the event handler bypasses only that wrapper.
void videoOptionsApplySettings(VideoOptions* vo)
{
    void* const renderer = ::dice::hfe::dx9rend::g_Renderer;

    // index 6: the mip level
    const int wanted6 = vo->quality[6];
    if (vo->rowA[6] != wanted6)
    {
        vo->dirty114 = 1;
        vo->rowA[6]  = wanted6;
    }

    // GSSkipMipLevels drives it directly when it is >= 0; otherwise it is derived from quality[6]
    // by an inverted mapping - 1 -> 2, 2 -> 1, 3 -> 0. Higher quality means fewer skipped levels.
    int mip = vo->skipMipLevels;
    if (mip < 0)
    {
        if      (wanted6 == 1) mip = 2;
        else if (wanted6 == 2) mip = 1;
        else if (wanted6 == 3) mip = 0;
    }

    // The fourteen mip-class records. The original reloads the TextureManager pointer and
    // reassigns its scratch on every iteration; both are no-ops because the two values are always
    // equal by this point, and the loop is written here as what it does.
    unsigned char* const tm = static_cast<unsigned char*>(::dice::hfe::dx9rend::g_textureManager);
    if (tm)
    {
        for (int i = 0; i < 112; i += 8)
        {
            if (*(tm + i + 40))
                *reinterpret_cast<int*>(tm + i + 44) = mip;
        }
    }

    writeIntSetting("GSSkipMipLevels", mip);

    // Five records then get a clamped value instead - 2, 3, 5, 8 and 9, by the same arithmetic
    // five times over. Offsets 60/68/84/108/116 are those records' value fields.
    if (tm)
    {
        const int clamped = clampMinusOne(mip);
        *reinterpret_cast<int*>(tm + 60)  = clamped;
        *reinterpret_cast<int*>(tm + 68)  = clamped;
        *reinterpret_cast<int*>(tm + 84)  = clamped;
        *reinterpret_cast<int*>(tm + 108) = clamped;
        *reinterpret_cast<int*>(tm + 116) = clamped;
    }

    // index 7: texture filtering
    const int wanted7 = vo->quality[7];
    if (vo->rowA[7] != wanted7)
    {
        vo->rowA[7]  = wanted7;
        vo->dirty116 = 1;
        vo->dirty117 = 1;
        vo->dirty118 = 1;
    }

    // index 2: the shader path
    const int wanted2 = vo->quality[2];
    if (vo->rowA[2] != wanted2)
    {
        vo->rowA[2]  = wanted2;
        vo->dirty117 = 1;
        vo->dirty115 = 1;
    }

    void* const effectManager = ::dice::hfe::dx9rend::Ra::g_effectManager;
    if (wanted2 == 1)
    {
        rendererSetShaderPathAndUpdate(renderer, 3);
        if (effectManager) writeRendererByte(effectManager, 64, 0);
        // vslot +0x00 on ourselves - setQuality(4, 0). Only the lowest setting does this.
        reinterpret_cast<PluginVideoOptions*>(vo)->setQuality(4, 0);
    }
    else if (wanted2 == 2)
    {
        rendererSetShaderPathAndUpdate(renderer, 1);
        if (effectManager) writeRendererByte(effectManager, 64, 0);
    }
    else if (wanted2 == 3)
    {
        rendererSetShaderPathAndUpdate(renderer, 0);
        if (effectManager) writeRendererByte(effectManager, 64, 0);
    }

    // Terrain quality also updates the road renderer, guarded only by g_Renderer (RW-77).
    const int wanted0 = vo->quality[0];
    if (vo->rowA[0] != wanted0)
    {
        vo->rowA[0]  = wanted0;
        vo->dirty118 = 1;
    }

    ::dice::hfe::geom::UndergrowthSystem* const undergrowth =
        static_cast< ::dice::hfe::geom::UndergrowthSystem*>(::dice::hfe::geom::g_UndergrowthSystem);
    void* const skyDome     = ::dice::hfe::geom::g_SkyDome;
    void* const roads       = ::dice::hfe::geom::g_RoadCompiledRenderer;

    if (wanted0 == 1)
    {
        if (undergrowth) { undergrowth->setSimpleShaderEnable(true); undergrowth->setViewDistanceScale(0.5f); }
        if (renderer)    { writeRendererByte(renderer, 1700, 0); callSlotInt(roads, 24, 0); }
        if (skyDome)     callSlotInt(skyDome, 148, 0);
    }
    else if (wanted0 == 2)
    {
        if (undergrowth) { undergrowth->setSimpleShaderEnable(true); undergrowth->setViewDistanceScale(0.75f); }
        if (renderer)    { writeRendererByte(renderer, 1700, 1); callSlotInt(roads, 24, 1); }
        if (skyDome)     callSlotInt(skyDome, 148, 1);
    }
    else if (wanted0 == 3)
    {
        if (undergrowth) { undergrowth->setSimpleShaderEnable(false); undergrowth->setViewDistanceScale(1.0f); }
        if (renderer)    { writeRendererByte(renderer, 1700, 1); callSlotInt(roads, 24, 1); }
        if (skyDome)     callSlotInt(skyDome, 148, 2);
    }

    // index 1: view distance / fog
    // The dirty-flag logic here is genuinely convoluted in the original: the flag is set unless the
    // change is between two non-1 values or from 1 to 1 - i.e. it latches only when the setting
    // crosses the "1" boundary in either direction. Reproduced by its own structure.
    const int applied1 = vo->rowA[1];
    const int wanted1  = vo->quality[1];
    if (applied1 != wanted1)
    {
        unsigned char flag = 1;
        if (!vo->dirty115)
        {
            const bool wasOne = (applied1 == 1);
            const bool isOne  = (wanted1 == 1);
            if (wasOne == isOne)
                flag = 0;
        }
        vo->dirty115 = flag;
        vo->rowA[1]  = wanted1;
    }

    void* const fogObject = ::dice::hfe::dx9rend::g_OverGrowth;

    if (wanted1 == 1)
    {
        if (renderer)
        {
            writeRendererFloat(renderer, 1728, 6.0f);
            writeRendererFloat(renderer, 1732, 6.0f);
            writeRendererFloat(renderer, 1736, 40.0f);
            writeRendererFloat(renderer, 2124, 0.5f);
            writeRendererFloat(renderer, 2224, 2.0f);
            writeRendererInt  (renderer, 2020, 1);
            writeRendererInt  (renderer, 2028, 0);
        }
        // Apply the exact 0.4 fog setting.
        if (fogObject) writeRendererFloat(fogObject, 244, 0.4f);
    }
    else if (wanted1 == 2)
    {
        if (renderer)
        {
            writeRendererFloat(renderer, 1728, 7.0f);
            writeRendererFloat(renderer, 1732, 8.0f);
            writeRendererFloat(renderer, 1736, 60.0f);
            writeRendererFloat(renderer, 2124, 0.8f);
            writeRendererFloat(renderer, 2224, 1.0f);
            writeRendererInt  (renderer, 2020, 0);
            writeRendererInt  (renderer, 2028, 0);
        }
        if (fogObject) writeRendererFloat(fogObject, 244, 0.7f);
    }
    else if (wanted1 == 3)
    {
        if (renderer)
        {
            writeRendererFloat(renderer, 1728, 8.0f);
            writeRendererFloat(renderer, 1732, 10.0f);
            writeRendererFloat(renderer, 1736, 80.0f);
            writeRendererFloat(renderer, 2124, 1.0f);
            writeRendererFloat(renderer, 2224, 1.0f);
            writeRendererInt  (renderer, 2020, 0);
            writeRendererInt  (renderer, 2028, -1);
        }
        if (fogObject) writeRendererFloat(fogObject, 244, 1.0f);
    }

    // indices 3 and 4: lighting and shadows, both through the LightManager
    ::dice::hfe::geom::LightManager* const lightManager =
        ::dice::hfe::geom::g_LightManager;
    if (lightManager)
    {
        const int wanted3 = vo->quality[3];
        if (vo->rowA[3] != wanted3)
        {
            if (wanted3 >= 0 && wanted3 <= 3)
            {
                // PE 0x100101E1..0x1001026A open-codes these four cases.  Preserve both the
                // repeated calls and their order; only the literal distance differs per case.
                static const float kEffectLightDistance[4] =
                {
                    0.0f, 50.0f, 100.0f, 200.0f
                };
                lightManager->setMaxLightsPerStaticMesh(wanted3);
                lightManager->setMaxLightsPerBundledMesh(wanted3);
                lightManager->setMaxLightsPerSkinnedMesh(wanted3);
                lightManager->setLightCullDistance(kEffectLightDistance[wanted3]);
            }
            vo->rowA[3] = vo->quality[3];
        }

        const int wanted4 = vo->quality[4];
        if (vo->rowA[4] != wanted4)
        {
            if (wanted4 == 0)
                lightManager->setDynamicShadowsEnabled(false);
            else if (wanted4 >= 1 && wanted4 <= 3)
                lightManager->setDynamicShadowsEnabled(true);
            vo->rowA[4] = vo->quality[4];
        }
    }

    // index 5: particles and decals
    ::dice::hfe::geom::IParticleSystemManager* const particles =
        ::dice::hfe::geom::asParticleSystemManager(
            ::dice::hfe::geom::g_ParticleSystemManager);
    void* const decals    = ::dice::hfe::geom::g_DecalManager;
    if (particles)
    {
        // sub_1000FCB0 calls ParticleSystemManager::applySettings (vtable +0x3C) with a pointer into
        // the 60-byte-strided per-quality profile table at byte_10237DB8. All four rows are copied
        // verbatim below; quality[5] remains the shipped unchecked index into that table.
        static const ::dice::hfe::geom::ParticleSystemSettings s_particleProfiles[4] =
        {
            { 0.00f,    0,   0,   0, { 0.00f, 0.00f, 0.00f },
              { 0.00f, 0.00f, 0.00f }, 0.00f, 0.00f, 0, 1, 0 },
            { 0.50f,  300, 300, 300, { 0.40f, 0.55f, 0.70f },
              { 0.30f, 0.60f, 1.00f }, 0.20f, 0.70f, 2, 1, 0 },
            { 0.75f,  500, 500, 500, { 0.40f, 0.55f, 0.70f },
              { 0.30f, 0.60f, 1.00f }, 0.30f, 0.85f, 1, 3, 1 },
            { 1.00f, 1000, 700, 600, { 0.40f, 0.55f, 0.70f },
              { 0.30f, 0.60f, 1.00f }, 0.40f, 0.85f, 0, 3, 2 }
        };
        particles->applySettings(&s_particleProfiles[vo->quality[5]]);
    }
    if (decals)
        *(static_cast<unsigned char*>(decals) + 678) = (vo->quality[5] > 1) ? 1 : 0;

    // the Terrain's lighting-dirty byte, every apply
    // 0x10010330..0x10010339, read: `mov eax, dword_1024B930; test; je;
    // mov byte ptr [eax+41Ch], 1` - Terrain::m_lightingDirty (Terrain.h asserts +0x41C), so the next
    // frame's applyFrameConstants recomputes the terrain lighting. It was missing here. Only a live
    // terrain sees it: none at boot or in the main menu of a first session.
    void* const terrain = ::dice::hfe::geom::g_Terrain;
    if (terrain)
        writeRendererByte(terrain, 0x41C, 1);

    // Apply the selected mode before advancing the applied resolution/AA indices.
    // A combined change resets once; an AA-only change takes the second branch.
    if (!vo->gameLoaded && vo->minusOneD4 != vo->minusOneDC)
    {
        videoOptionsChangeResolution(vo,
            reinterpret_cast<const Mode40*>(vo->modeCache.first) + vo->minusOneDC);
        vo->minusOneD4 = vo->minusOneDC;   // applied resolution = selected
        vo->minusOneD0 = vo->minusOneD8;   // applied AA = selected
    }
    if (!vo->gameLoaded && vo->minusOneD0 != vo->minusOneD8)
    {
        videoOptionsChangeResolution(vo,
            reinterpret_cast<const Mode40*>(vo->modeCache.first) + vo->minusOneDC);
        vo->minusOneD0 = vo->minusOneD8;   // applied AA = selected
    }

    ::renddx9abi::logf("VideoOptions::apply: mip=%d (GSSkipMipLevels written back), quality "
                       "terrain=%d viewdist=%d shaders=%d lighting=%d shadows=%d effects=%d "
                       "textures=%d filter=%d; roads=0x%08X shaderDefines refreshed",
                       mip, vo->quality[0], vo->quality[1], vo->quality[2], vo->quality[3],
                       vo->quality[4], vo->quality[5], vo->quality[6], vo->quality[7],
                       ::renddx9abi::addr(roads));

    // The return expression: one more updateShaderDefines.
    rendererUpdateShaderDefines(renderer);
}

void __fastcall videoOptions_apply(void* self, void*)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::VideoOptions::apply", self, 0x2C);
    VideoOptions* const vo = static_cast<VideoOptions*>(self);
    if (vo->gameLoaded)
    {
        ::renddx9abi::logf("VideoOptions.cpp:243 Not allowed to apply video options when a game "
                           "is loaded!");
        return;
    }
    videoOptionsApplySettings(vo);
}


// The display-mode / dynamic-option builders. buildResolutionList and buildAAList fill the two
// VideoOptions string lists from the Renderer's Mode40 list; setDynamicOption (+0x1C) selects an entry
// and, for the resolution, rebuilds the AA list; slot +0x20 returns the selected string.

namespace {

// VideoOptions is embedded in the base renderer at +1016; recover the base from it.
inline RendDX9Base* baseOfVo(VideoOptions* vo)
{
    return reinterpret_cast<RendDX9Base*>(reinterpret_cast<unsigned char*>(vo) - 1016);
}

// bpp -> a representative D3DFORMAT, as in the Renderer-side enumeration.
inline D3DFORMAT voFormatFromBpp(unsigned int bpp)
{
    return (bpp == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
}

// sub_10006DB0 - (re)enumerate one mode's multisample support into its +24 descriptor list. i runs
// 0..16; a supported type stores {type, max(quality-1, 0)}.
void fillMultiSample(RendDX9Base* b, Mode40* rec)
{
    vectorClear<MultiSample8>(&rec->multiSample);
    if (!b || !b->d3d9)
        return;
    const D3DFORMAT fmt = voFormatFromBpp(rec->bpp);
    vc71::vectorRef<MultiSample8> ms(&rec->multiSample);
    for (unsigned int i = 0; i <= 16u; ++i)
    {
        DWORD quality = 0;
        if (SUCCEEDED(b->d3d9->CheckDeviceMultiSampleType(b->adapter, static_cast<D3DDEVTYPE>(b->deviceType),
                                                          fmt, b->pp.Windowed,
                                                          static_cast<D3DMULTISAMPLE_TYPE>(i), &quality)))
        {
            MultiSample8 e;
            e.type    = i;
            e.quality = quality ? (quality - 1) : 0;
            ms.push_back(e);
        }
    }
}

// sub_1000F7F0 - rebuild the anti-aliasing string list for the selected mode. type 0 -> "Off", type 1
// (nonmaskable) skipped, n>1 -> "%dSamples". aaDescriptors keeps the 8-byte descriptors parallel to
// the strings so a chosen string maps back to a type.
void buildAAList(VideoOptions* vo, Mode40* rec)
{
    vectorClear< ::vc71::string >(&vo->aaList);
    vectorClear<MultiSample8>(&vo->aaDescriptors);
    if (!rec)
        return;
    RendDX9Base* b = baseOfVo(vo);
    fillMultiSample(b, rec);

    vc71::vectorRef< ::vc71::string > strs(&vo->aaList);
    vc71::vectorRef<MultiSample8>     desc(&vo->aaDescriptors);
    for (const MultiSample8* d = reinterpret_cast<const MultiSample8*>(rec->multiSample.first);
         d != reinterpret_cast<const MultiSample8*>(rec->multiSample.last); ++d)
    {
        if (d->type == 1)
            continue;                       // nonmaskable - not offered
        desc.push_back(*d);
        if (d->type == 0)
        {
            strs.push_back(::vc71::string("Off"));
        }
        else
        {
            char buf[32];
            _snprintf(buf, sizeof(buf), "%dSamples", d->type);
            buf[sizeof(buf) - 1] = 0;
            strs.push_back(::vc71::string(buf));
        }
    }
}

// sub_10010710 - build the resolution string list. Ask the Renderer for its Mode40 list (vslot +0x3C),
// keep the modes that support the required depth-stencil format, are 32bpp and >= 800 wide, and format
// each as "%ix%i@%iHz". The Mode40 is cached parallel to the string so setDynamicOption can hand the
// chosen mode to the AA builder.
void buildResolutionList(VideoOptions* vo)
{
    RendDX9Base* b = baseOfVo(vo);
    vectorClear< ::vc71::string >(&vo->resList);
    vectorClear<Mode40>(&vo->modeCache);
    if (!b || !b->d3d9)
        return;

    // Build the base's Mode40 list (Renderer slot +0x3C). Called through the non-virtual seam rather
    // than the base vtable because this runs during construction, before the vptr is guaranteed set.
    rendererGetModeList(b);

    // The required depth-stencil format is the base's first depth-stencil candidate (D3DFMT_D24S8).
    // [INFERRED] the shipped filter reads *(*(base+0xB0)); on modern hardware every 32bpp mode supports it.
    unsigned int requiredFmt = D3DFMT_D24S8;
    if (b->depthFormatsFirst && b->depthFormatsFirst != b->depthFormatsLast)
        requiredFmt = b->depthFormatsFirst[0];

    vc71::vectorRef< ::vc71::string > strs(&vo->resList);
    vc71::vectorRef<Mode40>           cache(&vo->modeCache);
    for (Mode40* rec = reinterpret_cast<Mode40*>(b->modeList.first);
         rec != reinterpret_cast<Mode40*>(b->modeList.last); ++rec)
    {
        bool supported = false;
        for (const unsigned int* f = reinterpret_cast<const unsigned int*>(rec->depthFormats.first);
             f != reinterpret_cast<const unsigned int*>(rec->depthFormats.last); ++f)
            if (*f == requiredFmt) { supported = true; break; }

        if (supported && rec->bpp == 32 && rec->width >= 800)
        {
            char buf[32];
            _snprintf(buf, sizeof(buf), "%ix%i@%iHz",
                      (int)rec->width, (int)rec->height, (int)rec->refresh);
            buf[sizeof(buf) - 1] = 0;
            strs.push_back(::vc71::string(buf));
            cache.push_back(*rec);
        }
    }
}

} // anonymous namespace

#define RD9_VO_SLOT(name, slotOffset, addr)                                                 \
    void __fastcall name(void* self, void*)                                                 \
    {                                                                                       \
        RD9_FRONTIER_AT("dice::hfe::dx9rend::VideoOptions::slot_" #slotOffset                \
                        " (" #addr ")", self, 0x00);                                        \
    }

// +0x00, sub_1000F620 - setQuality(index, value). Clamp `value` into [rowC[index], maxQuality[index]]
// (rowC is the per-setting floor, maxQuality the ceiling), store it in the quality row (this+0x28),
// and return whether it differs from the committed row at this[index+54] (byte 0xD8). On a first
// apply that committed row still holds the -1/0 initial values, so every setting reports "changed",
// which is correct. Out-of-range index is the shipped "Unknown quality setting" guard (returns 0).
char __fastcall videoOptions_setQuality(void* self, void*, int index, int value)
{
    VideoOptions* vo = static_cast<VideoOptions*>(self);
    if (index < 0 || index >= 8)
        return 0;
    int v = vo->maxQuality[index];
    if (vo->rowC[index] <= value)
    {
        if (v >= value)
            v = value;
    }
    else
    {
        v = vo->rowC[index];
    }
    const int committed =
        *reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(vo) + 216 + index * 4);
    vo->quality[index] = v;
    return (committed == v) ? 0 : 1;
}

// Returns the setting's minimum quality from its initialized eight-entry row.
// These wrappers guard indices; the original slots assume indices 0..7.
int __fastcall videoOptions_slot08(void* self, void*, int index)
{
    const VideoOptions* vo = static_cast<const VideoOptions*>(self);
    if (index < 0 || index > 7)
        return 0;
    return vo->rowC[index];
}

// +0x0C, sub_100112D0: `return this[index + 26]` - dword row D at byte 0x68.
int __fastcall videoOptions_slot0C(void* self, void*, int index)
{
    const VideoOptions* vo = static_cast<const VideoOptions*>(self);
    if (index < 0 || index > 7)
        return 0;
    return vo->rowD[index];
}

// +0x10, sub_100112E0: `return this[index + 34]` - dword maxQuality at byte 0x88. setQuality clamps
// the requested value up to it, so this is the per-setting maximum.
int __fastcall videoOptions_slot10(void* self, void*, int index)
{
    const VideoOptions* vo = static_cast<const VideoOptions*>(self);
    if (index < 0 || index > 7)
        return 0;
    return vo->maxQuality[index];
}

// +0x14, sub_10011300: `return *(BYTE*)(this + index + 168)` - the byte row `flags` at byte 0xA8.
// Returned as char (AL), matching the shipped signature.
char __fastcall videoOptions_slot14(void* self, void*, int index)
{
    const VideoOptions* vo = static_cast<const VideoOptions*>(self);
    if (index < 0 || index > 7)
        return 0;
    return static_cast<char>(vo->flags[index]);
}

// +0x18, sub_10011320 - `return this + 16*index + 176`: a pointer to the index-th dynamic-option
// string list (index 0 = aaList @0xB0, index 1 = resList @0xC0), so the menu can enumerate every
// option for a dropdown. The list is a vc71::vector_layout (begin @+4), which is what the menu walks.
const HostDynamicOptionList& __fastcall
videoOptions_getDynamicOptionList(void* self, void*, int index)
{
#if BF2_PLUGIN_ABI_NATIVE
    // The shipped body is unguarded pointer arithmetic into the object; that has no meaning once
    // the host's element type differs from the stored one, so the native branch serves the two
    // lists this object actually owns. Both callers (VideoSettings' resolution and antialiasing
    // getters) pass 1 and 0. A copy, not a view: the list is rebuilt whenever the resolution
    // changes, and the caller copies it immediately anyway.
    const int slot = (index == 0) ? 0 : 1;
    VideoOptions* vo = static_cast<VideoOptions*>(self);
    const vc71::vector_layout* list = (slot == 0) ? &vo->aaList : &vo->resList;
    s_nativeOptionList[slot].clear();
    for (const ::vc71::string* s = reinterpret_cast<const ::vc71::string*>(list->first);
         s && s != reinterpret_cast<const ::vc71::string*>(list->last); ++s)
        s_nativeOptionList[slot].push_back(std::string(s->c_str()));
    return s_nativeOptionList[slot];
#else
    return *reinterpret_cast<const HostDynamicOptionList*>(
        reinterpret_cast<const unsigned char*>(self) + 16 * index + 176);
#endif
}
// +0x1C, sub_1000FB70 - setDynamicOption(index, value). Prefix-searches the index-th string list
// (index 0 = anti-aliasing @0xB4, index 1 = resolution @0xC4) for `value` - strnicmp over the search
// string's length, so "800x600" matches "800x600@60Hz". On a match it records the chosen entry index
// at this[index+54] (0xD8 for AA, 0xDC for resolution) and, when the resolution changed, rebuilds the
// AA list for the newly-selected mode. Returns whether the selection changed.
bool __fastcall videoOptions_setDynamicOption(
    void* self, void*, int index, const ::renddx9abi::HostString* value)
{
    VideoOptions* vo = static_cast<VideoOptions*>(self);
    if (index < 0 || index > 1 || !value)
        return false;

    vc71::vector_layout* list      = (index == 0) ? &vo->aaList : &vo->resList;
    const char*          needle    = value->c_str();
    const unsigned int   needleLen = value->size();

    int found = -1;
    int i = 0;
    for (const ::vc71::string* s = reinterpret_cast<const ::vc71::string*>(list->first);
         s != reinterpret_cast<const ::vc71::string*>(list->last); ++s, ++i)
    {
        if (_strnicmp(s->c_str(), needle, needleLen) == 0)
        {
            found = i;
            break;
        }
    }
    if (found < 0)
    {
        ::renddx9abi::logf("VideoOptions::setDynamicOption: '%s' not present in list %d", needle, index);
        return false;   // the shipped "Unknown DynamicOption value" result
    }

    int* selected = (index == 0) ? &vo->minusOneD8 : &vo->minusOneDC;
    const bool changed = (*selected != found);
    *selected = found;

    if (changed && index == 1)   // resolution changed -> rebuild the AA list for the new mode
    {
        Mode40* rec = reinterpret_cast<Mode40*>(vo->modeCache.first) + found;
        buildAAList(vo, rec);
    }
    return changed;
}
// +0x20, sub_10011350 - return the selected dynamic-option string BY reference:
// listBegin + 28*selectedIndex (index 0 = AA @0xB4/idx@0xD8, index 1 = resolution
// @0xC4/idx@0xDC). The PE materialises that reference as the selected object's address in EAX;
// spelling it as an integer was only an ABI disguise and made typed callers unnecessarily fragile.
// PE 0x10011350 intentionally has no index, empty-list or negative-selection guard: preserve that
// behavior here.
const ::renddx9abi::HostString& __fastcall
videoOptions_getDynamicOption(void* self, void*, int index)
{
    VideoOptions* vo = static_cast<VideoOptions*>(self);
    vc71::vector_layout* list = (index == 0) ? &vo->aaList : &vo->resList;
    const ::vc71::string* begin = reinterpret_cast<const ::vc71::string*>(list->first);
    const int sel = (index == 0) ? vo->minusOneD8 : vo->minusOneDC;
#if BF2_PLUGIN_ABI_NATIVE
    const int slot = (index == 0) ? 0 : 1;
    s_nativeOption[slot] = (begin + sel)->c_str();
    return s_nativeOption[slot];
#else
    return begin[sel];
#endif
}
// +0x24, sub_1000ED70 - store a float, clamped to [0.5, 1.0], at this+0x100. Its exact role is
// [UNRESOLVED] (aspect/gamma-ish); applyQualityPreset feeds it 0.75 / 0.9 / 1.0. The shipped body
// passes the value as the dword bit pattern (1061158912 == 0x3F400000 == 0.75f, etc.), which is the
// same four stack bytes as passing the float, so this takes a plain float.
void __fastcall videoOptions_setViewDistanceScale(void* self, void*, float value)
{
    float v = value;
    if (v < 0.5f)      v = 0.5f;
    else if (v > 1.0f) v = 1.0f;
    *reinterpret_cast<float*>(reinterpret_cast<unsigned char*>(self) + 0x100) = v;
}
// +0x28, sub_10011340 - return the +0x100 float (the value slot +0x24 clamps and stores).
// The PE is one `fld dword ptr [ecx+100h]`; BF2.exe consumes it with an `fstp dword`, matching
// IVideoOptions::getViewDistanceScale's float contract rather than Hex-Rays' ambiguous x87 double.
float __fastcall videoOptions_getViewDistanceScale(void* self, void*)
{
    return static_cast<VideoOptions*>(self)->dynFloat;
}
unsigned int systemRamMB();   // defined with the detectQualityLevel helpers below (slot +0x34)

// sub_1000F310 - the texture-quality value (setting index 6) for a preset. Starts at 3, drops to 2
// when the DeviceCaps dword at +0x584 (this rebuild's DeviceCaps::unknown584, byte 1412) is < 0xF0,
// drops one more for preset 0, and is forced to 1 when system RAM is under 750 MB.
int textureQualityForPreset(int preset)
{
    unsigned int v = 3;
    if (g_deviceCaps &&
        *reinterpret_cast<const unsigned int*>(
            static_cast<const unsigned char*>(g_deviceCaps) + 0x584) < 0xF0u)
        v = 2;
    if (preset == 0 && v > 2)
        --v;
    if (systemRamMB() >= 750u)
        return (int)v;
    return 1;
}

// Applies low/medium/high presets (0/1/2) through virtual quality and dynamic-option setters.
// HostString must match the receiving ABI; dynamic option 1 is applied before option 0.
int __fastcall videoOptions_applyQualityPreset(void* self, void*, int preset)
{
    PluginVideoOptions* const options = reinterpret_cast<PluginVideoOptions*>(self);

    if (preset == 0)            // LOW
    {
        options->setQuality(0, 1); options->setQuality(1, 1); options->setQuality(2, 1);
        options->setQuality(3, 0); options->setQuality(4, 0); options->setQuality(5, 1);
        options->setQuality(6, textureQualityForPreset(0));
        options->setQuality(7, 1);
        options->setDynamicOption(1, ::renddx9abi::HostString("800x600"));
        options->setDynamicOption(0, ::renddx9abi::HostString("Off"));
        options->setViewDistanceScale(0.75f);
    }
    else if (preset == 1)       // medium
    {
        options->setQuality(0, 2); options->setQuality(1, 2);
        options->setQuality(2, 2); options->setQuality(3, 2);
        if (options->getPerGpuFlag()) options->setQuality(4, 2);
        else                          options->setQuality(4, 0);
        options->setQuality(5, 2);
        options->setQuality(6, textureQualityForPreset(1));
        options->setQuality(7, 2);
        options->setDynamicOption(1, ::renddx9abi::HostString("800x600"));
        options->setDynamicOption(0, ::renddx9abi::HostString("Off"));
        options->setViewDistanceScale(0.9f);
    }
    else if (preset == 2)       // high
    {
        options->setQuality(2, (systemRamMB() >= 1100u) ? 3 : 2);
        options->setQuality(0, 3); options->setQuality(1, 3);
        options->setQuality(3, 3); options->setQuality(4, 3);
        options->setQuality(5, 3);
        options->setQuality(6, textureQualityForPreset(2));
        options->setQuality(7, 3);
        options->setDynamicOption(1, ::renddx9abi::HostString("1024x768"));
        options->setDynamicOption(0, ::renddx9abi::HostString("Off"));
        options->setViewDistanceScale(1.0f);
    }
    // Unknown preset: the shipped body returns `preset - 2`; the value is unused by the caller.
    return 0;
}

// Reads the CPU timestamp after a serialization barrier for the quality benchmark.
inline unsigned __int64 cpuTimestamp()
{
    int regs[4];
    __cpuid(regs, 0);           // the shipped `xor eax,eax; cpuid` barrier
    return __rdtsc();
}

// sub_1000EE70: rdtsc ticks per wall-second, measured on a single core across a 1000 ms window, then
// folded down by 1e-6 / 1e-3 into the MHz-ish range the caller expects. Returns 0.0 on timer failure
// (the shipped early-outs), and restores the original affinity mask before returning.
double cpuBenchmarkSpeed()
{
    HANDLE proc = GetCurrentProcess();
    DWORD_PTR procMask = 0, sysMask = 0;
    GetProcessAffinityMask(proc, &procMask, &sysMask);
    DWORD_PTR oneCore = 1;
    while (oneCore && (oneCore & procMask) == 0)
        oneCore *= 2;
    SetProcessAffinityMask(proc, oneCore);
    Sleep(0);

    LARGE_INTEGER freq, startQpc, endQpc;
    if (!QueryPerformanceFrequency(&freq) || !QueryPerformanceCounter(&startQpc))
        return 0.0;
    const unsigned __int64 startTsc = cpuTimestamp();
    if (!startTsc)
        return 0.0;
    Sleep(1000);
    const unsigned __int64 tscDelta = cpuTimestamp() - startTsc;
    if (!QueryPerformanceCounter(&endQpc))
        return 0.0;

    SetProcessAffinityMask(proc, procMask);
    Sleep(0);

    const double seconds = (double)(endQpc.QuadPart - startQpc.QuadPart) / (double)freq.QuadPart;
    const double result  = (double)(__int64)tscDelta / seconds;
    if (result > 1000000.0) return result * 0.000001;
    if (result > 1000.0)    return result * 0.001;
    return result;
}

// sub_1000EF90: the benchmark rounded to an int with the shipped +0.55 bias.
int cpuBenchmarkScore()
{
    return (int)(cpuBenchmarkSpeed() + 0.55000001);
}

// sub_1000F0A0: the shipped cpuid-availability probe. Runs cpuid and returns 1 (cpuid exists on any
// CPU that can execute this image).
inline int cpuidAvailable()
{
    int regs[4];
    __cpuid(regs, 0);
    return 1;
}

// sub_1000F120: an IPC multiplier from cpuid vendor + family/model. The constants are the shipped
// ones; an unrecognised vendor returns 0.2 (the shipped "reverting to lower settings scheme" value,
// minus the log line).
double cpuFamilyMultiplier()
{
    if (!cpuidAvailable())
        return 0.2;

    int r0[4];
    __cpuid(r0, 0);                                  // vendor string: EBX, EDX, ECX
    char vendor[16] = { 0 };
    *reinterpret_cast<int*>(vendor + 0) = r0[1];     // EBX
    *reinterpret_cast<int*>(vendor + 4) = r0[3];     // EDX
    *reinterpret_cast<int*>(vendor + 8) = r0[2];     // ECX

    int r1[4];
    __cpuid(r1, 1);
    const unsigned int eax = (unsigned int)r1[0];
    const unsigned int ebx = (unsigned int)r1[1];

    const unsigned int baseFamily = (eax >> 8) & 0xF;
    const unsigned int family = (baseFamily == 15) ? (((eax >> 20) & 0xFF) + 15) : baseFamily;
    const unsigned int model  = (baseFamily == 15)
                                    ? (((eax >> 4) & 0xF) + 16 * ((eax >> 16) & 0xF))
                                    : ((eax >> 4) & 0xF);
    const unsigned char brand = (unsigned char)ebx;  // brand index (low byte of EBX)

    if (!strncmp("AuthenticAMD", vendor, 12))
    {
        if ((family == 15 && (model == 4 || model == 5)) || brand)
            return 1.5;
        return 1.4;
    }
    if (!strncmp("GenuineIntel", vendor, 12))
    {
        switch (brand)
        {
            case 1: case 3: case 7: case 10: case 15:
            case 18: case 19: case 20: case 23:
                return 0.75;
            case 6: case 22:
                return 1.6;
            default:
                return 1.0;
        }
    }
    return 0.2;   // unknown vendor - shipped logs and reverts to the low multiplier
}

// sub_1000F010: total physical RAM in MB, rounded up to a multiple of 4 (GlobalMemoryStatus, the
// non-Ex 32-bit form the shipped body uses).
unsigned int systemRamMB()
{
    MEMORYSTATUS ms;
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatus(&ms);
    unsigned int mb = (unsigned int)(((unsigned __int64)ms.dwTotalPhys + 0x7FFFF) >> 20);
    if (mb & 3u)
        mb += 4u - (mb & 3u);
    return mb;
}

// [FRONTIER] Legacy GPU whitelist data is absent; always falls through to capability heuristics.
// Restoring the tables is required for exact quality selection on listed older GPUs.
// [RETIRE-WHEN: the VideoOptions GPU whitelist tables are extracted]
bool gpuWhitelistLevel(int* /*outLevel*/)
{
    return false;
}

// The heuristic. `self` is ignored, matching the shipped no-`this` body. The redundant RAM re-reads
// the decompiler shows before each return are side-effect-free and omitted.
int __fastcall videoOptions_slot34(void* /*self*/, void*)
{
    const int          benchmark = cpuBenchmarkScore();
    const int          cpuScore  = (int)(cpuFamilyMultiplier() * (double)benchmark);
    const unsigned int ramMB     = systemRamMB();

    const CapsView* c = capsOf();
    const unsigned int psMajorVer = c ? ((c->caps.PixelShaderVersion >> 8) & 0xFFu) : 0u;
    const bool isATI = c && c->adapter.VendorId == 0x1002u;
    const bool isNV  = c && c->adapter.VendorId == 0x10DEu;   // computed by sub_100011C0; unused after, as shipped
    (void)isNV;

    int        whitelistLevel = 0;
    const bool inWhitelist    = gpuWhitelistLevel(&whitelistLevel);

    int level = 2;
    if (cpuScore < 2350 || ramMB < 1024u)
        level = 0;
    else if (cpuScore < 2950)
        level = 1;

    if (psMajorVer == 1)                 // pixel shader 1.x -> forced low
        return 0;

    if (inWhitelist)
    {
        if (level >= whitelistLevel)
            return whitelistLevel;
    }
    else if (isATI && psMajorVer >= 3)
    {
        if (level >= 2)
            return 2;
    }
    else
    {
        level = (level != 0) ? 1 : 0;    // not whitelisted and not a PS3 ATI part: cap at medium
    }
    return level;
}
// +0x38, sub_1000F360 - reads a per-GPU byte from the whitelist (sub_10001320 -> matched record +16)
// and returns it, or 1 when the GPU is not in the list. That is the same fixed 2009 whitelist
// gpuWhitelistLevel documents; a modern card misses it, so this returns the shipped default 1.
// applyQualityPreset's medium preset uses it to decide whether setting 4 is 2 (true) or 0.
// [RETIRE-WHEN: the VideoOptions GPU whitelist tables are extracted]
char __fastcall videoOptions_slot38(void* self, void*)
{
    (void)self;
    return 1;
}

#undef RD9_VO_SLOT

void* const kVideoOptionsVtable[15] =
{
    reinterpret_cast<void*>(&videoOptions_setQuality),   // +0x00
    reinterpret_cast<void*>(&videoOptions_getQuality),   // +0x04
    reinterpret_cast<void*>(&videoOptions_slot08),
    reinterpret_cast<void*>(&videoOptions_slot0C),
    reinterpret_cast<void*>(&videoOptions_slot10),
    reinterpret_cast<void*>(&videoOptions_slot14),
    reinterpret_cast<void*>(&videoOptions_getDynamicOptionList),
    reinterpret_cast<void*>(&videoOptions_setDynamicOption),
    reinterpret_cast<void*>(&videoOptions_getDynamicOption),
    reinterpret_cast<void*>(&videoOptions_setViewDistanceScale),
    reinterpret_cast<void*>(&videoOptions_getViewDistanceScale),
    reinterpret_cast<void*>(&videoOptions_apply),        // +0x2C  apply
    reinterpret_cast<void*>(&videoOptions_applyQualityPreset),
    reinterpret_cast<void*>(&videoOptions_slot34),
    reinterpret_cast<void*>(&videoOptions_slot38)
};

// 101DDFFC is exactly one slot, 100103F0. The ECCore subobject is at +04;
// its void(category, id, EventNode*, userData) call uses four stack words.
// Level load applies pending quality values before refreshing their dependent resource caches.
void __fastcall videoOptions_handleECCoreEvent(void* self, void*, int category, unsigned int id,
                                               void*, void*)
{
    if (category != ECCore)
        return;
    VideoOptions* const vo = reinterpret_cast<VideoOptions*>(
        static_cast<unsigned char*>(self) - offsetof(VideoOptions, vptrEventHandler));
    if (id == 2)
    {
        videoOptionsApplySettings(vo);
        vo->gameLoaded = 1;
        if (vo->dirty115)
        {
            geom::geometryTemplateManagerClear();
            vo->dirty115 = 0;
        }
        if (vo->dirty114)
        {
            const bool wasReady = resourceManagerIsReady();
            resourceManagerSetReady(1);
            textureManagerSweep();
            resourceManagerSetReady(wasReady);
            vo->dirty114 = 0;
        }
        if (vo->dirty117)
        {
            if (vo->dirty116)
                Ra::effectManagerResetEffects();
            geom::geometryTemplateManagerRearmTechniques();
            vo->dirty117 = 0;
            vo->dirty116 = 0;
        }
        if (vo->dirty118)
        {
            updateShaderDefines(g_shaderManager, g_rendererBase);
            shaderManagerReloadEffects(g_shaderManager);
            vo->dirty118 = 0;
        }
    }
    else if (id == 3)
        videoOptionsApplySettings(vo);
    else if (id == 13)
        vo->gameLoaded = 0;
}

void* const kVideoOptionsEventVtable[1] =
{
    reinterpret_cast<void*>(&videoOptions_handleECCoreEvent)
};

} // anonymous namespace

// VideoOptions_ctor 10010590, called by base ctor 1000BA84 before D3D setup.
// The ECCore base's temporary purecall table is superseded by the derived table;
// no intervening callback observes it. Do not initialize fields the ctor leaves
// untouched: quality rows, list allocator words, selection indices, dynFloat/padding.
void videoOptionsConstruct(void* base)
{
    VideoOptions* vo = reinterpret_cast<VideoOptions*>(
        static_cast<unsigned char*>(base) + 0x3F8);
    vo->vptr = const_cast<void**>(kVideoOptionsVtable);
    vo->vptrEventHandler = const_cast<void**>(kVideoOptionsEventVtable);
    vo->aaList.first = nullptr;
    vo->aaList.last = nullptr;
    vo->aaList.end = nullptr;
    vo->resList.first = nullptr;
    vo->resList.last = nullptr;
    vo->resList.end = nullptr;
    vo->modeCache.first = nullptr;
    vo->modeCache.last = nullptr;
    vo->modeCache.end = nullptr;
    vo->aaDescriptors.first = nullptr;
    vo->aaDescriptors.last = nullptr;
    vo->aaDescriptors.end = nullptr;
    vo->windowSizeX = 0;
    vo->windowSizeY = 0;
    vo->refreshHz = 0;
    vo->skipMipLevels = 0;
    vo->dirty114 = 0;
    vo->dirty115 = 0;
    vo->dirty116 = 0;
    vo->dirty117 = 0;
    vo->dirty118 = 0;
    vo->gameLoaded = 0;
}

// Initializes quality tables and hardware-dependent limits for the renderer's VideoOptions.
// base must be an initialized RendDX9Base. Quality queries use the second eight-entry row;
// some other row roles remain [UNRESOLVED]. Deferred initialization calls are marked below.
void videoOptionsInitialize(void* base)
{
    RendDX9Base* b = static_cast<RendDX9Base*>(base);
    VideoOptions* vo = reinterpret_cast<VideoOptions*>(
        reinterpret_cast<unsigned char*>(b) + 1016);

    IEventListener* const listener = reinterpret_cast<IEventListener*>(&vo->vptrEventHandler);
    if (!eventManager->registerEventHandler(ECCore, listener, -1000000))
        ::renddx9abi::logf("VideoOptions.cpp:129 Failed to register VideoOptions as event handler for ECCore.");

    // The four settings, with the defaults the body passes.
    vo->windowSizeX    = readIntSetting("GSWindowSizeX",   0);
    vo->windowSizeY    = readIntSetting("GSWindowSizeY",   0);
    vo->skipMipLevels  = readIntSetting("GSSkipMipLevels", -1);
    vo->refreshHz      = readIntSetting("GSRefresh",       0);

    // The five rows, written in the body's own interleaved order but laid out here by row because
    // the interleaving carries no information - each row is filled with one value per column.
    for (int i = 0; i < 8; ++i)
    {
        vo->rowA[i]       = 4;
        vo->quality[i]    = 2;
        vo->rowC[i]       = 1;
        vo->rowD[i]       = 3;
        vo->maxQuality[i] = 3;
        vo->flags[i]      = 0;
    }
    // ROW C IS NOT uniform - entries 3 and 4 are 0 where the rest are 1. Read, not a slip:
    // `*(this + 21) = 0` and `*(this + 22) = 0` sit among six stores of 1.
    vo->rowC[3] = 0;
    vo->rowC[4] = 0;

    // The hardware ceiling adjustments on row E.
    if (vsMajor() < 2 || psMajor() < 2)
    {
        vo->maxQuality[0] = 1;
        vo->maxQuality[2] = 1;
        vo->maxQuality[4] = 0;
        vo->maxQuality[3] = 1;
    }

    if (maxAnisotropy())
    {
        if (maxAnisotropy() <= 4)
            vo->maxQuality[7] = 3;
    }
    else
    {
        vo->maxQuality[7] = 2;
    }

    // Clear both dynamic-option string lists (aaList @0xB0, resList @0xC0). On a freshly zeroed object
    // these are empty and this is a no-op; vectorClear also handles a re-initialise by destroying the
    // strings and freeing the buffer, matching the shipped `if (*v4)` element-destruction guard.
    vectorClear< ::vc71::string >(&vo->aaList);
    vectorClear< ::vc71::string >(&vo->resList);

    // ALL four OF these ARE -1, AND apply depends ON IT. The loop writes +0xD8 and +0xDC; the
    // two explicit stores below write +0xD4 and +0xD0. apply's resolution-change guard compares
    // +0xD4 against +0xDC and +0xD0 against +0xD8, so four equal values mean both guards are false
    // and the device-reset path is correctly skipped on a first boot.
    vo->minusOneD8 = -1;
    vo->minusOneDC = -1;

    // sub_10010710 - build the "%ix%i@%iHz" resolution list by enumerating the adapter modes through
    // the base's vslot +0x3C and filtering to 32bpp / >=800-wide / depth-format-supported. The menu's
    // setDynamicOption("800x600") then matches an entry and the +0x20 readback is valid.
    buildResolutionList(vo);

    vo->minusOneD4 = -1;
    vo->minusOneD0 = -1;

    // [CHOSEN, not recovered] Skip automatic detection and preset 1 at initialization.
    // This avoids the startup benchmark and its 800x600/AA-off reset; profile application
    // supplies requested settings later.

    ::renddx9abi::logf("VideoOptions::initialize: %dx%d @%dHz skipMip=%d; quality row = all 2 "
                       "(getQuality(0)=%d terrain, getQuality(7)=%d filter); maxQuality[7]=%d",
                       vo->windowSizeX, vo->windowSizeY, vo->refreshHz, vo->skipMipLevels,
                       vo->quality[0], vo->quality[7], vo->maxQuality[7]);
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
