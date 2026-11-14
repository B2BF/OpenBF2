// Primary renderer interface shared across the DLL boundary.
// Preserve its 55-slot order; unresolved methods retain positional names.

#ifndef RENDMISC_IRENDERER_H
#define RENDMISC_IRENDERER_H

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/HfeAbi.h"
#else
#include "../Core/IBase.h"
#endif

#include <string>

// A reference parameter needs only the declaration. Keep the client/renderer's x86
// display-mode storage out of server consumers such as ObjectManager; owners of a
// HostDisplayMode include DisplayModeAbi.h themselves. No virtual signature changes.
namespace renddx9abi { struct HostDisplayMode; }

namespace dice {
namespace hfe {

class IVideoOptions;
class DemoConfig;       // Game/DemoRecorder/DemoConfig.h - slot +0x64's render settings

namespace dx9rend {

class IRenderView;
struct RenderFlags;

// Display-mode filters: approximately 4:3 only, Win32 mode cross-check, and minimum
// 640x480 at 60 Hz. Passed by address as three consecutive bytes.
struct DisplayModeFilter
{
    bool aspect4by3;
    bool crossCheckDisplaySettings;
    bool minimumResolution;
};

class IRenderer : public IBase
{
public:
    virtual bool setDisplayMode(const renddx9abi::HostDisplayMode& mode) = 0; // +0x10
    virtual bool slot_0x14() = 0;                                            // +0x14
    virtual bool slot_0x18() = 0;                                            // +0x18
    virtual bool initialize() = 0;                                           // +0x1C
    virtual void beginFrame() = 0;                                           // +0x20
    virtual void update(double time) = 0;                                    // +0x24
    // deltaTime is the clamped frame duration forwarded to HUD updates.
    virtual bool renderFrame(double time, float deltaTime) = 0;               // +0x28
    // +0x2C  0x1002BC50, `retn 10h`: the binary64 time, the flags record and one pointer the
    // body tests and hands to ObjectDrawer +0x20. Materializes AL=1 on every exit.
    virtual bool renderScene(double time, const RenderFlags* flags,
                             void* drawObject) = 0;                           // +0x2C
    // +0x30  0x1002F770, no arguments, `mov al, 1`: closes the frame renderScene opened.
    virtual bool slot_0x30() = 0;
    // +0x34  0x1002D040, `retn 8`: the ObjectDrawer pointer and the flags record (its +0x14 is
    // the clear colour beginScene receives). Materializes AL=1.
    virtual bool slot_0x34(void* drawObject, const RenderFlags* flags) = 0;
    // +0x38  0x1002A4A0, no arguments, no result materialized: the HUD-only frame.
    virtual void slot_0x38() = 0;
    // +0x3C  signature the shipped body ends
    // `retn 4` on both exits (0x10007478, 0x100075A0) and reads its one argument as the filter
    // record above (0x1000747B `mov esi, [ebp+8]`); every caller pushes that record's address. The
    // old `void* slot_0x3C()` had the wrong arity for a callee-cleanup slot. Builds the adapter's
    // display-mode list whenever it is null or empty (in practice once,
    // by VideoOptions::initialize) and returns RendDX9's own container (retail base +0x90): a
    // std::vector<renddx9abi::DisplayModeRecord> BF2.exe reads directly (DisplayModeAbi.h, issue #7).
    virtual void* slot_0x3C(const DisplayModeFilter* filter) = 0;
    // +0x40  0x10005C70, `retn 4`: select swap chain `index` as render target 0. AL is 0 when
    // its back buffer cannot be fetched, 1 otherwise.
    virtual bool slot_0x40(unsigned int index) = 0;
    virtual bool setRenderView(IBase* view) = 0;                              // +0x44
    virtual IRenderView* getRenderView() = 0;                                 // +0x48
    virtual bool setRenderViewDup(IBase* view) = 0;                           // +0x4C
    virtual IRenderView* getRenderViewDup() = 0;                              // +0x50
    virtual unsigned int getFrameOrdinal() = 0;                               // +0x54
    virtual void slot_0x58() = 0;
    // +0x5C  0x10005010, `retn 18h`, AL = 1: copies the named texture's top level into the GDI
    // bitmap `bitmap` (an HBITMAP; with `alphaToGrey` each pixel's alpha becomes its grey value)
    // and reports the texture's size through the non-null out pointers. `reupload` re-uploads
    // the texture first. [INFERRED] parameter names.
    virtual bool slot_0x5C(const std::string& name, void* bitmap, bool reupload,
                           bool alphaToGrey, unsigned int* width, unsigned int* height) = 0;
    // +0x60  0x1002C820, `retn 1Ch`: arms a top-down capture for the next beginFrame. A non-zero
    // size stores all seven arguments at +0x7F4..+0x80C and arms +0x7F0; a zero size replays the
    // stored set through sub_1002A120, which renders an orthographic top-down frame of that size
    // (clamped to the device's texture limits) at the position, `orthoExtent` wide and high, into
    // the target's texture, with the objects switch set to `drawObjects`, and disarms +0x7F0.
    // `target` is [INFERRED] a Meme::Texture (sub_1002A120 reads its +0x2C render target);
    // `orthoExtent` goes to RenderView +0x50/+0x58 unchanged. [INFERRED] parameter names.
    virtual void slot_0x60(unsigned int size, void* target, float x, float y, float z,
                           float orthoExtent, bool drawObjects) = 0;
    // +0x64  0x10004FC0, `retn 4`: starts rendering a demo to files - drops the current movie
    // recorder (slot +0x68), then builds a new one (base +0x3F4, MovieRenderer.cpp) from the
    // demo's render settings. The recorder keeps `config`, which must outlive it. The argument
    // is read as a DemoConfig (renderFile +0x40, width/height/fps +0x5C..+0x64, compressed
    // +0x6C, panorama +0x6D, slices +0x70 in the x86 layout); [INFERRED] pointer spelling.
    virtual void slot_0x64(const DemoConfig* config) = 0;
    // +0x68  0x100051D0: destroys the movie recorder, if any (renderer.stopRenderMovie).
    virtual void slot_0x68() = 0;
    // +0x6C  one instruction, `lea eax, [ecx+108h]`: the statistics block embedded
    // at +0x108 (Texture.cpp counts textures at its +0x2C; RenderQueues.cpp counts draws through
    // it).
    virtual void* getRenderStats() = 0;
    // +0x70  0x10032700, `mov al, [ecx+7F0h]`: the byte slot +0x60 arms.
    virtual bool slot_0x70() = 0;
    virtual IVideoOptions* getVideoOptions() = 0;                             // +0x74
    // +0x78 / +0x7C  0x100326E0 / 0x100326F0: store / `fld` the float at +0x6C8.
    virtual void slot_0x78(float value) = 0;
    virtual float slot_0x7C() = 0;
    // +0x80..+0x8C  0x100285A0..0x10028600: the RoadCompiledRenderer's +0x8C and +0x90 floats,
    // set when the road renderer exists and read back as 0.0f when it does not.
    virtual void slot_0x80(float value) = 0;
    virtual float slot_0x84() = 0;
    virtual void slot_0x88(float value) = 0;
    virtual float slot_0x8C() = 0;
    // +0x90 / +0x94  0x10028620 / 0x10028640: tail calls into LightManager +0xEC / +0xF0
    // (set/getDynamicShadowsEnabled); the getter answers false without a LightManager.
    virtual void slot_0x90(bool enabled) = 0;
    virtual bool slot_0x94() = 0;
    // +0x98 / +0x9C  0x10028660 / 0x10028680: the Terrain's +0x35E byte, through its setter
    // sub_100D89C0 and by direct read; false without a Terrain.
    virtual void slot_0x98(bool value) = 0;
    virtual bool slot_0x9C() = 0;
    virtual double getFrameRate() = 0;                                        // +0xA0
    virtual void setRenderAheadLimit(unsigned int limit) = 0;                 // +0xA4
    virtual unsigned int getRenderAheadLimit() = 0;                           // +0xA8
    virtual bool isExpansionPack() = 0;                                       // +0xAC
    // +0xB0  one instruction, `mov eax, [ecx+768h]`: the dword the constructor seeds
    // with 2 and nothing in the image rewrites.  (the nametag update calls it
    // and discards the answer).  The name is still unknown; the return is a full 32-bit load.
    virtual unsigned int slot_0xB0() = 0;
    // +0xB4  the scalar deleting destructor (RendDX9 0x10032C80): destroy, free when bit 0 of
    // flags is set, answer `this`. release (+0x08) calls it with 1 when the count reaches zero.
    // [CHOSEN, not recovered] spelled as a named slot so that every ABI keeps one entry here
    // (an Itanium virtual destructor would occupy two).
    virtual void* destroy(unsigned int flags) = 0;
    // +0xB8  (89 B), signature the shipped body reads
    // ONE byte argument (`mov bl, [ebp+8]`) and ends `retn 4`, so the old `void()` placeholder had
    // the wrong arity for a callee-cleanup slot. The only recovered caller is the renderer's own
    // CEUnloadLevel arm, which pushes 0. The body is referenced from exactly two slots: the
    // RendDX9Base table 0x101DDA38's +0xB8 (0x101DDAF0) and the Renderer table 0x101E0468's +0xB8
    // (0x101E0520). [INFERRED] parameter name; the slot's name is unknown.
    virtual void slot_0xB8(bool includeEffectManager) = 0;
    // Pumps messages and waits for activation/device recovery; returns true only if no wait occurred.
    // isAppInactive reports the current application-activation flag.
    virtual bool waitWhileAppInactive() = 0;                                  // +0xBC
    virtual bool isAppInactive() = 0;                                         // +0xC0
    virtual void refreshShaderDefines() = 0;                                  // +0xC4
    virtual bool beginScene(const unsigned int* rgb, char doClear) = 0;        // +0xC8
    virtual bool endScene() = 0;                                              // +0xCC
    virtual bool present(unsigned int mode) = 0;                              // +0xD0
    virtual IRenderView* getRenderView2() = 0;                                // +0xD4
    virtual void shutdown() = 0;                                              // +0xD8
    // +0xDC [CHOSEN, not recovered] Port addition after retail's last slot (+0xD8): the adapter's
    // current display mode, IDirect3D9::GetAdapterDisplayMode, for VideoSettings' new-profile
    // resolution. False when the device reports none (the outputs are then unchanged).
    virtual bool getDesktopMode(unsigned int& width, unsigned int& height,
                                unsigned int& refreshHz) = 0;
};

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // RENDMISC_IRENDERER_H
