// Primary renderer interface shared across the DLL boundary.
// Preserve its 55-slot order; unresolved methods retain positional names.

#ifndef RENDMISC_IRENDERER_H
#define RENDMISC_IRENDERER_H

#ifdef BF2_IMAGE_RENDDX9
#include "../../RendDX9Abi/HfeAbi.h"
#else
#include "../Core/IBase.h"
#endif

// A reference parameter needs only the declaration. Keep the client/renderer's x86
// display-mode storage out of server consumers such as ObjectManager; owners of a
// HostDisplayMode include DisplayModeAbi.h themselves. No virtual signature changes.
namespace renddx9abi { struct HostDisplayMode; }

namespace dice {
namespace hfe {

class IVideoOptions;

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
    virtual bool renderScene(double time, const RenderFlags* flags,
                             float blurAmount) = 0;                           // +0x2C
    virtual void slot_0x30() = 0;
    virtual void slot_0x34() = 0;
    virtual void slot_0x38() = 0;
    // +0x3C  signature the shipped body ends
    // `retn 4` on both exits (0x10007478, 0x100075A0) and reads its one argument as the filter
    // record above (0x1000747B `mov esi, [ebp+8]`); every caller pushes that record's address. The
    // old `void* slot_0x3C()` had the wrong arity for a callee-cleanup slot. Builds the adapter's
    // display-mode list whenever it is null or empty (in practice once,
    // by VideoOptions::initialize) and returns RendDX9's own container (retail base +0x90): a
    // std::vector<renddx9abi::DisplayModeRecord> BF2.exe reads directly (DisplayModeAbi.h, issue #7).
    virtual void* slot_0x3C(const DisplayModeFilter* filter) = 0;
    virtual void slot_0x40() = 0;
    virtual bool setRenderView(IBase* view) = 0;                              // +0x44
    virtual IRenderView* getRenderView() = 0;                                 // +0x48
    virtual bool setRenderViewDup(IBase* view) = 0;                           // +0x4C
    virtual IRenderView* getRenderViewDup() = 0;                              // +0x50
    virtual unsigned int getFrameOrdinal() = 0;                               // +0x54
    virtual void slot_0x58() = 0;
    virtual void slot_0x5C() = 0;
    virtual void slot_0x60() = 0;
    virtual void slot_0x64() = 0;
    virtual void slot_0x68() = 0;
    // +0x6C  one instruction, `lea eax, [ecx+108h]`: the statistics block embedded
    // at +0x108 (Texture.cpp counts textures at its +0x2C; RenderQueues.cpp counts draws through
    // it).
    virtual void* getRenderStats() = 0;
    virtual void slot_0x70() = 0;
    virtual IVideoOptions* getVideoOptions() = 0;                             // +0x74
    virtual void slot_0x78() = 0;
    virtual void slot_0x7C() = 0;
    virtual void slot_0x80() = 0;
    virtual void slot_0x84() = 0;
    virtual void slot_0x88() = 0;
    virtual void slot_0x8C() = 0;
    virtual void slot_0x90() = 0;
    virtual void slot_0x94() = 0;
    virtual void slot_0x98() = 0;
    virtual void slot_0x9C() = 0;
    virtual double getFrameRate() = 0;                                        // +0xA0
    virtual void setRenderAheadLimit(unsigned int limit) = 0;                 // +0xA4
    virtual unsigned int getRenderAheadLimit() = 0;                           // +0xA8
    virtual bool isExpansionPack() = 0;                                       // +0xAC
    // +0xB0  one instruction, `mov eax, [ecx+768h]`: the dword the constructor seeds
    // with 2 and nothing in the image rewrites.  (the nametag update calls it
    // and discards the answer).  The name is still unknown; the return is a full 32-bit load.
    virtual unsigned int slot_0xB0() = 0;
    virtual void slot_0xB4() = 0;
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
};

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // RENDMISC_IRENDERER_H
