// Path: attested by RendDX9.dll __FILE__ at 0x101E00A0 (initialize, line 437).
// Renderer implementation for RendDX9.dll, separate from Game/Main/BF2Render.cpp.
// Recovered from the PE; unread bodies remain named [FRONTIER] stops.
// [CHOSEN, not recovered] BF2_IMAGE_RENDDX9 isolates this image from recursive builds.
// [RETIRE-WHEN: RW-50] Remove the guard once build.cmd has a RendDX9 image manifest.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/PluginEntry.h"    // dice::hfe::classManager, for hWndBf2TempSlt
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Game/Main/IVideoOptions.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#if BF2_PLUGIN_ABI_NATIVE
// The ECCore CEPreLoad hook hands two of the host's own records to ModInfo::parseMapDesc. In the
// native flavour the host IS this toolchain, so the recovered declarations are the right types; the
// retail flavour cannot use them (VC7.1 string/vector layouts) and mirrors them instead - see
// selectShaderArchive below.
#include "../Game/IModInfo.h"
#endif
#include "../IO/Console/Console.h"
#include "../Geom/XpLightManager.h"
#include "../Geom/HemiMapManager/HemiMapManager.h"
#include "../Geom/HemiMapManager/HemiMapDependencies.h"
#include "../Geom/Overgrowth/OverGrowth.h"
#include "../Geom/UndergrowthSystem.h"
#include "../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../Geom/Terrain.h"
#include "../Geom/ObjectDrawer.h"
#include "../Geom/RoadEditableRenderer.h"   // IRoadEditableRenderer - the typed drawAll below
#include "../RendMisc/IRenderer.h"
#include "../RendMisc/IRenderView.h"
#include "../RendMisc/EnvMapSystem/EnvMapManager.h"
#include "../Scene/Object/ObjectManager.h"
// HM1: Renderer::setLods walks the geometry-template registry and the three mesh
// templates' live-instance sets; its object arm reads each object's geometry (IObject +0x38).
#include "../Geom/GeometryTemplateManager.h"
#include "../Geom/StaticMeshTemplate.h"
#include "../Geom/SkinnedMeshTemplate.h"
#include "../Geom/BundledMeshTemplate.h"
#if BF2_PLUGIN_ABI_NATIVE
#include "../Game/Objects/ObjectLayoutAccess.h"
#endif
#include "RendDX9Base.h"
#include "Texture.h"
#include "PostProductionFrameWork.h"
// RW-266: the host classes the frame loop dispatches into, typed - see hostCalls below.
// System, WindManager and PlayerManager carry no STL across the calls made on them, so both flavours
// include them; the Menu class, ISwiffHost and ModInfo are native-only (their calls carry the host's
// own STL types, which only the native flavour shares).
#include "../Core/System.h"
#include "../Scene/Object/WindManager.h"
#include "../Scene/Object/PlayerManager.h"
#if BF2_PLUGIN_ABI_NATIVE
#include "../Game/ModInfo.h"
#include "../Menu/BF2HudManager.h"
#include "../Menu/ISwiffHost.h"
#endif

#include <cstring>

namespace dice {
namespace hfe {
namespace geom {
extern IBase* g_DebugTextWriter;
unsigned fontPortedGetColour(IBase* font);
void debugTextWriterSetColour(IBase* writer, unsigned colour);
void debugTextWriterQueueText(IBase* writer, int x, int y, const vc71::string& text, int alignment);
}

// PluginEntry.cpp - four more cached host singletons the frame loop drives (defined there with the
// other nineteen; only the boot set is in PluginEntry.h).
class System;
extern System* g_system;        // matches shared Core/System.h
extern IBase* mainConsole;      // the is-console-open query
extern IBase* objectManager;    // the fog triple + the streaming frustum query
extern IBase* playerManager;    // the local player for the HUD handoff


// Host-call adapters use typed interfaces where layouts agree; compatibility builds retain
// raw slots where STL containers or strings cross the image boundary.

namespace hostCalls {

double systemWallClockTime()           { return g_system->getWallClockTime(); }        // +0x18
void   systemSleep(int milliseconds)   { g_system->sleep(milliseconds); }              // +0x1C
void   systemFrameBegin()              { g_system->storeAndTurnOffFPUExceptions(); }   // +0x28
void   systemFrameEnd()                { g_system->restoreFPUExceptions(); }           // +0x2C

void windUpdate(void* wind, float timeF)                                                // +0x10
{
    static_cast<world::WindManager*>(wind)->update(timeF);
}

void* localHumanPlayer()                                                                // +0x30
{
    return reinterpret_cast<world::PlayerManager*>(playerManager)->getLocalHumanPlayer();
}

void hudSetPaint(void* hudManager, unsigned char enabled)                              // +0x20
{
#if BF2_PLUGIN_ABI_NATIVE
    static_cast<hud::BF2HudManager*>(hudManager)->setPaint(enabled != 0);
#else
    typedef void (__fastcall* SetByteFn)(void*, void*, unsigned char);
    reinterpret_cast<SetByteFn>((*static_cast<void***>(hudManager))[8])(hudManager, 0, enabled);
#endif
}

void hudUpdate(void* hudManager, float timeF, void* player, float x, float y, float z) // +0x28
{
#if BF2_PLUGIN_ABI_NATIVE
    Vec3 look;
    look.x = x; look.y = y; look.z = z;
    static_cast<hud::BF2HudManager*>(hudManager)->update(
        timeF, static_cast<world::IPlayer*>(player), look);
#else
    typedef void (__fastcall* PrepFn)(void*, void*, float, void*, float, float, float);
    reinterpret_cast<PrepFn>((*static_cast<void***>(hudManager))[10])(
        hudManager, 0, timeF, player, x, y, z);
#endif
}

void hudPaint(void* hudManager)                                                         // +0x1C
{
#if BF2_PLUGIN_ABI_NATIVE
    static_cast<hud::BF2HudManager*>(hudManager)->paint();
#else
    typedef void (__fastcall* Fn)(void*, void*);
    reinterpret_cast<Fn>((*static_cast<void***>(hudManager))[7])(hudManager, 0);
#endif
}

// The streaming scan's frustum query. The renderer keeps the result as a VC7.1 vector at dwords
// 469..472 of itself - an allocator word, then first / last / end - and the retail flavour hands
// that whole object over. A native std::vector<IObject*> is exactly the three pointers, so the
// native flavour overlays it on dword 470; handing it dword 469 (as the raw call did) would have
// given the host a vector shifted by one word the day its body is written. In this tree the host
// body is still the empty ObjectManager::getObjectsFromGridNoCullConst [FRONTIER] forwarder
// (Scene/Object/ObjectManager.cpp), so the query answers nothing in either flavour.
void objectManagerQueryFrustum(void* frustum488, unsigned int* resultVectorAt469)      // +0x5C
{
#if BF2_PLUGIN_ABI_NATIVE
    // reinterpret_cast, as BundledMeshHeight.cpp reaches the same singleton: the published IBase*
    // is the object's start, which is what the raw dispatch assumed too.
    reinterpret_cast<world::ObjectManager*>(objectManager)->getObjectsFromGridNoCullConst(
        *static_cast<const Frustum*>(frustum488),
        *reinterpret_cast<std::vector<world::IObject*>*>(resultVectorAt469 + 1), 0, false, 0.0f,
        0.0f);
#else
    void* om = objectManager;
    typedef void (__fastcall* QueryFn)(void*, void*, void*, void*, int, int, int, int);
    reinterpret_cast<QueryFn>((*static_cast<void***>(om))[23])(
        om, 0, frustum488, resultVectorAt469, 0, 0, 0, 0);
#endif
}

} // namespace hostCalls

// RendMisc/RenderView.cpp. Declared rather than pulled through a header - two symbols, and this
// image has no RenderView.h to include.
namespace Rend {
class RenderView;
extern const unsigned int IID_IRenderView;
extern const unsigned int CID_RenderView;
IBase* createRenderViewDirect();
void   renderViewSetViewMatrix(RenderView* view, const float* m16);
void   renderViewSetAspect(RenderView* view, float a);
void   renderViewSetViewport(RenderView* view, float x, float y, float w, float h);
void   renderViewSetFov(RenderView* view, float fovRadians);
void   renderViewSetFarPlane(RenderView* view, float f);
// The streaming scan builds a stack frustum through the same perspective builder the view uses.
void   renderViewBuildFrustum(void* frustumStorage488, float fov, float aspect,
                              float nearZ, float farZ, const float* viewMatrix);
}

namespace geom {
extern const unsigned int CID_DebugLineGraph;
extern const unsigned int IID_IDebugLineGraph;
// ModuleGeom.cpp - the world singletons the frame loop drives (all null until their classes are
// built, except the RoadCompiledRenderer).
extern LightManager* g_LightManager;
extern IBase* g_SkyDome;
extern IBase* g_ParticleSystemManager;
extern IBase* g_DecalManager;
extern IBase* g_UndergrowthSystem;
// The ECCore CEPostLoad arm reaches this one; retail does NOT null-test it, and this build
// registers no NametagManager class, so it is null here. See renderer_handleECCoreEvent case 3.
extern IBase* g_NametagManager;
extern IBase* g_StaticMeshRenderer;
extern IBase* g_SkinnedMeshRenderer;
extern IBase* g_RoadCompiledRenderer;
extern IBase* g_RoadEditableRenderer;
extern IBase* g_GeometryTemplateManager;
extern IBase* g_Terrain;
// RoadCompiledRenderer.cpp - what roadQueuePass reads off the road renderer singleton: the fade pair
// (+0xB0/+0xB4) and the two depth-bias words (+0x90/+0x94) as their bit patterns.
const float* roadCompiledRendererFadeOut();
unsigned int roadCompiledRendererDepthBiasBits();
unsigned int roadCompiledRendererSlopeScaleDepthBiasBits();
}

namespace dx9rend {

// TextureManager.cpp / ResourceManager.cpp - the two calls Renderer::initialize step 16 makes.
void textureManagerSweep();
void resourceManagerSetReady(unsigned char value);

// ShaderManager.cpp - what vtable slot +0xC4 tail-calls into.
extern void* g_shaderManager;
void updateShaderDefines(void* shaderManager, void* base);
void shaderManagerReloadEffects(void* shaderManager);

// StateManager singleton and its device listener.
extern void* g_stateManager;

// QueryManager.cpp. Step 13 reaches the manager through the global exactly as the original does -
// `mov ecx, dword_1023B98C` inside the loop - rather than being handed a pointer.
void* queryManagerCreateQuery(int type);

// dword_1023B970, defined in RendDX9Init.cpp. Step 14 tests a byte at +0x102 of it.
extern void* g_rendererBase;

// Scene bracket and frame-stat operations.
bool rendererBeginScene(const unsigned int* clearRgb, bool doClear);
bool rendererEndScene();
bool rendererPresent(unsigned int mode);
unsigned char statsSwapEnabled(unsigned char value);
// StateManager.cpp - the render-target flush before a manual Clear; and this file's own
// getRenderView seam, defined below the class but used by helpers above it.
void flushRenderTargetsSeam();
IRenderView* rendererGetRenderView();
IRenderView* rendererGetRenderViewDup();
unsigned int rendererFrameOrdinal();
bool rendererIsExpansionPack();
IVideoOptions* rendererGetVideoOptions();
// StateManager.cpp.
bool stateManagerIsFullyRestored();
void stateManagerForceRestore();
void stateManagerSetRenderStatePersistent(unsigned int state, unsigned int value);
void stateManagerSetTransparencyAA(bool enabled);
#if BF2_PLUGIN_ABI_NATIVE
// RendererConsole.cpp - the `renderer` console object's list (the static at 0x10241668).
bool registerRendererConsoleObjects();
bool unregisterRendererConsoleObjects();
#endif
// RenderQueues.cpp - the mesh sort buckets and the withheld world-infrastructure globals.
void renderQueueFlush(unsigned int slotByteOffset, int flushArg);
extern void* g_renderQueues;

namespace Ra {
// Ra/RaShader.h declares both of these; they are forward-declared here rather than pulling
// that header in, exactly as this file forward-declares the rest of its cross-TU seams.
struct RaShaderTable;
int  raLoadAllShaders(RaShaderTable* table, unsigned char flags);
void effectManagerResetEffects();
}
extern void* g_renderEntryPool;
extern void* g_raShaderConstants;
extern void* g_renderTargetSystem;
// QueryManager.cpp - the GPU-throttle fences.
void queryIssueEnd(void* query);
void queryWaitDone(void* query);

// Code/BF2/Core/Ids.h already carries both, recovered from the ELF, and both are confirmed here:
// getSingletonList registers the pair (830001, 831001) and queryInterface tests 830001.
const unsigned int IID_IRenderer = 830001u;
const unsigned int CID_RendDX9   = 831001u;


// Per-pass render switches shared with the host. Preserve all 24 bytes, including
// fields this scene pass does not read.

struct RenderFlags
{
    unsigned char useRenderView;    // +0x00  pass renderView to beginScene rather than 0
    unsigned char field01;          // +0x01  gates the ds:1024B930 arm
    unsigned char world;            // +0x02  world geometry, pass A
    unsigned char worldAlt;         // +0x03  world geometry, pass B (either one enables the pair)
    unsigned char field04;          // +0x04  [UNRESOLVED] - unread by this body
    unsigned char staticMeshes;     // +0x05
    unsigned char skinnedMeshes;    // +0x06
    unsigned char decals;           // +0x07
    unsigned char particles;        // +0x08
    unsigned char field09;          // +0x09  [UNRESOLVED] - unread by this body
    unsigned char field0a;          // +0x0A  [UNRESOLVED] - unread by this body
    unsigned char skyDome;          // +0x0B
    unsigned char undergrowth;      // +0x0C
    unsigned char field0d;          // +0x0D  gates the ds:10258F38 arm
    unsigned char postProduction;   // +0x0E
    unsigned char colourCorrection; // +0x0F  the +0x80D latch and the constants block
    unsigned char hud;              // +0x10
    unsigned char aiDebug;          // +0x11  posts event (3, 4) when the AI module says so
    unsigned char field12;          // +0x12  copied into the ds:1024B930 object's byte +0x35A
    unsigned char pad13;            // +0x13  alignment for the pointer below
    void*         renderView;       // +0x14
};

class Renderer : public IRenderer
{
public:
    Renderer();

    // IBase, slots 0..3
    virtual void  addRef();                                     // +0x00
    virtual int   getRef();                                     // +0x04
    virtual int   release();                                    // +0x08
    virtual void* queryInterface(unsigned int iid) const;       // +0x0C  0x1000CAC0   read

    // slots 4..54
    // Declared `void f()` where the signature is unread. That is a placeholder for the slot, not a
    // claim about the method: every one of these bodies stops before returning, so neither the
    // absent return value nor the absent arguments is ever observed. See Frontier.h on why the
    // continue-mode escape hatch is unsound for exactly these.
    virtual bool setDisplayMode(const HWndBf2TempSlt& mode);     // +0x10  0x1000B800   read
    virtual bool slot_0x14();                                   // 0x10009DD0, mov al,1; ret
    virtual bool slot_0x18();                                   // 0x10009DE0, mov al,1; ret
    virtual bool initialize();                                  // +0x1C  0x1002C8C0   read
    // [INFERRED] name - it is the opening half of the frame bracket whose
    // closing half is at the end of renderFrame.
    virtual void beginFrame();                                  // +0x20
    virtual void update(double time);                           // +0x24
    // deltaTime is the clamped frame duration passed to HUD updates.
    virtual bool renderFrame(double time, float deltaTime);     // +0x28
    // [INFERRED] name and parameter names; blurAmount is consumed only when worldAlt is zero.
    virtual bool renderScene(double time, const RenderFlags* flags,
                             float blurAmount);                 // +0x2C  0x1002BC50  [FRONTIER]
    virtual void slot_0x30();
    virtual void slot_0x34();
    virtual void slot_0x38();
    virtual void* slot_0x3C(const DisplayModeFilter* filter);   // 0x10007420   build+return the display-mode list (retn 4)
    virtual void slot_0x40();
    virtual bool setRenderView(IBase* view);                    // +0x44
    virtual IRenderView* getRenderView();                       // +0x48
    virtual bool setRenderViewDup(IBase* view);                  // +0x4C
    virtual IRenderView* getRenderViewDup();                    // +0x50
                                                                //        (ICF-folded twin of +0x48)
    virtual unsigned int getFrameOrdinal();                      // +0x54
    // Draws the open in-game console.
    virtual void slot_0x58();
    virtual void slot_0x5C();
    // beginFrame passes seven DWORDs through a raw adapter; this placeholder lacks that signature.
    virtual void slot_0x60();                                   // (7 dwords of args)
    virtual void slot_0x64();
    virtual void slot_0x68();
    virtual void* getRenderStats();                             // 0x101432E0  return this + 0x108
    virtual void slot_0x70();
    virtual IVideoOptions* getVideoOptions();                    // +0x74  0x10005000   return this+1016
    virtual void slot_0x78();
    virtual void slot_0x7C();
    virtual void slot_0x80();
    virtual void slot_0x84();
    virtual void slot_0x88();
    virtual void slot_0x8C();
    virtual void slot_0x90();
    virtual void slot_0x94();
    virtual void slot_0x98();
    virtual void slot_0x9C();
    // [INFERRED] spelling; exact binary64 carrier and measured-FPS role are recovered.
    virtual double getFrameRate();                              // +0xA0
    virtual void setRenderAheadLimit(unsigned int limit);       // +0xA4
    virtual unsigned int getRenderAheadLimit();                 // +0xA8
    virtual bool isExpansionPack();                             // +0xAC  0x10032740   read
    virtual unsigned int slot_0xB0();                           // +0xB0
    virtual void slot_0xB4();
    // see the body. The byte argument and `retn 4` are
    // read, so the signature is no longer the void() placeholder. [INFERRED] parameter name.
    virtual void slot_0xB8(bool includeEffectManager);          // +0xB8
    // and retyped void -> bool: both bodies materialise AL
    // (`sete al` at 0x100055A8 / 0x1000562D) and all three RendDX9 callers of +0xBC test it.
    // [INFERRED] names - see RendMisc/IRenderer.h and the bodies below.
    virtual bool waitWhileAppInactive();                        // +0xBC
    virtual bool isAppInactive();                               // +0xC0
    virtual void refreshShaderDefines();                        // +0xC4  0x10009A20   read
    virtual bool beginScene(const unsigned int* rgb, char doClear);   // +0xC8
    virtual bool endScene();                                    // +0xCC
    virtual bool present(unsigned int mode);                    // +0xD0
    virtual IRenderView* getRenderView2();                      // +0xD4
    virtual void shutdown();                                    // +0xD8

    // Renderer-owned accumulation/Hemi scratch texture at +0x774. These nonvirtual owner methods
    // back the narrow cross-TU HemiMapManager seams without exposing or indexing a vtable.
    EngineTexture* getHemiScratchTexture() const;
    void setHemiScratchTexture(EngineTexture* texture);
    void releaseHemiScratchTexture();

    // sub_10028310 and sub_10028270.  Both are ordinary __thiscall Renderer members in the PE and
    // neither is in the vtable, so adding them changes no slot and no layout.  They are public
    // because the ECCoreEventHandler body below is a free function on the +0x538 subobject and
    // reaches the Renderer as `self - 0x538`, exactly as the shipped handler does.
    // [INFERRED] both names; the stripped PE gives neither.
    bool setExpansionPack(unsigned char value);      // sub_10028310, the +0x764 setter
    void handleLevelPreLoad(const char* levelName);  // sub_10028270, the ECCore CEPreLoad hook

    // setDisplayMode (+0x10, 0x1000B800) from 0x1000B819 on: everything after
    // the carrier copy. Non-virtual and not in the vtable, so no slot or layout changes. [CHOSEN,
    // not recovered] split: VideoOptions' resolution change hands +0x10 the renderer's own block,
    // which the native plugin cannot express as a host carrier - see rendererApplyCurrentDisplayMode.
    bool applyCurrentDisplayMode();

    // The RAPATH index selected from VideoOptions shader quality. This is distinct from the
    // GSRenderPath listener state. The setter is PE 0x100286C0 and refreshes shader defines.
    // [INFERRED] member and accessor names. The field layout and setter are recovered; the getter
    // is [CHOSEN, not recovered] owner-TU plumbing that keeps raw layout arithmetic out of callers.
    int getShaderPath() const;
    void setShaderPathAndUpdate(int path);

private:
    // Step 6 factors the two identical debug-text-writer calls; see its definition.
    void setDebugTextWriterFont(const char* singletonName, const char* fontPath);
    void initializeFontAndConsoleOverlay();   // BF2Render_initializeFontAndConsoleOverlay 0x1002BA40

    // The frame-loop helpers, all
    void hudManagerFramePrep(float timeF);
    void geometryStreamTick();                // sub_1002DF20 - the streaming scan (budget 25.0)
    void roadQueuePass();                     // sub_1002A560 - terrain-gated road queue flush

    // The base occupies +0x000..+0x537 and is accessed through RendDX9Base's overlay.
    // The primary vptr is already declared, so storage starts at +0x004.
    char         m_baseStorage[0x538 - 4];   // +0x004 .. +0x537
    void*        m_evHandlerVptr;        // +0x538  ECCoreEventHandler       - kECCoreEventHandlerVtable
    void*        m_deviceLostVptr;       // +0x53C  DeviceLostResetListener  - kRendererDeviceLostVtable
    void*        m_renderPathVptr;       // +0x540  RenderPathListener       - kRendererRenderPathVtable
    unsigned int m_unknown544;           // +0x544  [UNRESOLVED] - the ctor does not write it
    int          m_refCount;             // +0x548

    // A 4x4 matrix at dword indices 339..354. The constructor lays an identity into it twice (once
    // in the run of zeros, once at the very end), and initialize both writes its translation row
    // and hands the whole thing to RenderView::setViewMatrix - which is what identifies it.
    float        m_viewMatrix[16];       // +0x54C .. +0x58B
    float        m_f58C;                 // +0x58C  (355) ctor: 1.0
    float        m_f590;                 // +0x590  (356) ctor: 0x3D25F55E
    float        m_near;                 // +0x594  (357) ctor: 0.01
    float        m_far;                  // +0x598  (358) ctor: 15.0
    float        m_f59C;                 // +0x59C  (359) ctor: 6.0
    char         m_tail1aBeforeRenderAheadLimit[0x5B8 - 0x5A0]; // +0x5A0 .. +0x5B7
    unsigned int m_renderAheadLimit;                            // +0x5B8, ctor 2, max 5
    char         m_tail1aAfterRenderAheadLimit[0x5C0 - 0x5BC];  // +0x5BC .. +0x5BF
    // dword 368. initialize releases whatever is here, then replaces it with a fresh
    // dice.hfe.geom.ObjectDrawer and publishes the same pointer to a file-scope global.
    void*        m_objectDrawer;         // +0x5C0  (368)
    // The three initializeFontAndConsoleOverlay writes, at self+1476/1480/1484.
    unsigned char m_consoleOverlayReady; // +0x5C4  (1476) set to 1 at the end
    unsigned char m_pad5C5[3];
    void*        m_consoleOverlay;       // +0x5C8  (1480) a geom::OverlayTexture, cid 81008
    void*        m_consoleFont;          // +0x5CC  (1484) a geom::Font::Ported,   cid 81007
    // Three singletons initialize step 7 caches. They are stored and never dereferenced there, so
    // a null one is survivable at that point - which is why step 7 can run with all three absent.
    void*        m_hudManager;           // +0x5D0  (372)
    void*        m_weatherManager;       // +0x5D4  (373)
    void*        m_rainManager;          // +0x5D8  (374)
    char         m_tail1bBeforeFrameRate[0x600 - 0x5DC];   // +0x5DC .. +0x5FF [FRONTIER]
    double       m_frameRate;                              // +0x600 measured FPS
    char         m_tail1bBeforeShaderPath[0x6CC - 0x608];  // +0x608 .. +0x6CB [FRONTIER]
    int          m_shaderPath;                             // +0x6CC RAPATH index
    char         m_tail1bAfterShaderPath[0x774 - 0x6D0];   // +0x6D0 .. +0x773 [FRONTIER]
    EngineTexture* m_hemiScratchTexture;                 // +0x774 accumulation/Hemi scratch target
    char         m_tail1bAfterHemiScratch[0x884 - 0x778]; // +0x778 .. +0x883 [FRONTIER]
    void*        m_lazy884;              // +0x884  (545) ctor: 0; initialize lazily allocates it
    char         m_tail2a[0x894 - 0x888];// +0x888 .. +0x893   [FRONTIER] unread
    // Graph slots must match frame rendering: framerate, render time, then draw calls.
    void*        m_debugGraphs[3];       // +0x894 / +0x898 / +0x89C
    // +0x8A0 / +0x8A8, dbl[276] and dbl[277]. The frame bracket: slot_0x20 stamps the start, and
    // renderFrame's tail subtracts it from the same clock to get the frame's wall time, which is
    // then the sample fed to the Rendering-time graph. Named here because the pair is now written
    // at both ends; they used to be reached only through the raw `dbl` alias, which is how the
    // overlap above went unnoticed.
    double       m_frameStartTime;       // +0x8A0
    double       m_frameWallTime;        // +0x8A8
    char         m_tail2b[0x8E0 - 0x8B0];// +0x8B0 .. +0x8DF   [FRONTIER] unread
};

static_assert(sizeof(Renderer) == 0x8E0,
              "RefFactory_dx9rend_Renderer allocates 0x8E0 - see 0x10051940");


// The globals the registration records point at. Both are `IBase` slots in a SingletonRegInfo, so
// ClassManager writes through them - which is exactly how the renderer image learns its own
// Renderer pointer without ever calling getSingleton for it.

IBase* g_Renderer                     = 0;   // ctor writes it, registration republishes
IBase* g_PostProductionFrameWork      = 0;   // published by DX9RendModule::init, flags == 4
IBase* g_PostProductionFrameWorkOwner = 0;   // the 548-byte owner; the singleton is owner + 8

// Shared renderer instance, published by base construction before device initialization.
extern void* g_rendererBase;


// dword_1025A39C. initialize publishes the ObjectDrawer it creates here as well as into +0x5C0, so
// the rest of the image can reach it without a getSingleton.

IBase* g_ObjectDrawer = 0;

// Overgrowth exists from DLL initialization. Level unload deletes it; preload recreates it.
void* g_OverGrowth = geom::createOverGrowth();

// byte_10238249 is file-backed 1 and has no writers in the retail image.  Keep it mutable so each
// of renderFrame's four gates performs the same independent load the PE does.
unsigned char g_transparencyAARenderBracketsEnabled = 1;

// ECCore listener: one handleEvent slot at 0x101E0464, subobject at +0x538.
// PE 0x1002F440 handles load/unload, system info, performance tests and activation.
// Retain each case's own null checks; CEPostLoad calls NametagManager unconditionally.
namespace {

template <typename T>
inline T& rendererField(void* base, unsigned int offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(base) + offset);
}


// [FRONTIER] Unavailable ECCore callees report and continue without fabricating results.
// Each call site retains a rate-limited hit count; enabling kECCoreSeamStops makes them abort.
// [CHOSEN, not recovered] [TEMP] [RETIRE-WHEN: the named callee is written]
// Replace each seam after its callee is implemented and validated through the event path.

const bool kECCoreSeamStops = false;

void eccoreSeam(unsigned int& hits, void* self, const char* qualifiedName, const char* whatRetailDoes)
{
    if (kECCoreSeamStops)
        RD9_FRONTIER_AT(qualifiedName, self, 0x00);

    ++hits;
    // 1, 2, 3, 4, 8, 16, ... - loud at first, then quiet enough for a 30-minute session.
    if (hits > 4u && (hits & (hits - 1u)) != 0u)
        return;
    ::renddx9abi::logf("[FRONTIER] %s reached from "
                       "dice::hfe::dx9rend::Renderer::handleECCoreEvent (hit #%u, self=0x%08X) - "
                       "SKIPPED, no value fabricated. Retail would %s",
                       qualifiedName, hits, ::renddx9abi::addr(self), whatRetailDoes);
}

// MSVC will not let a free function be __thiscall, so the ECX/EDX pair is spelled out: __fastcall
// with an unused second parameter is byte-for-byte the same calling convention for a slot like
// this one, whose arguments are all stack-passed after `this`.
void __fastcall renderer_handleECCoreEvent(void* self, void* /*edx, unused*/,
                                           int category, unsigned int id,
                                           void* node, void* userData)
{
    (void)node;                                     // arg_8 - the EventNode; unread by this body

    if (category != 5)                              // cmp [ebp+arg_0], 5 / jnz - ECCore only
        return;

    // `lea ecx, [esi-538h]` - the handler's own `this` is the Renderer's +0x538 subobject.
    Renderer* const renderer =
        reinterpret_cast<Renderer*>(static_cast<unsigned char*>(self) - 0x538);

    switch (id)
    {
    // Preloads resources and creates overgrowth. Payload is a bare level-directory C string.
    case 2:
    {
        renderer->handleLevelPreLoad(static_cast<const char*>(userData));

        if (!g_OverGrowth)
        {
            g_OverGrowth = geom::createOverGrowth();
        }
        return;
    }

    // id 3, CEPostLoad
    //     g_NametagManager->vslot[+0x44]();          <-- NO null test in the shipped body
    //     if (dword_10267A34) sub_101BF110();        <-- ecx = dword_10267A34, so a method on it
    case 3:
    {
        // Post-load buffering requires the registered nametag manager.
        {
            typedef void (__thiscall* SlotFn)(void*);
            void** const vtbl = *reinterpret_cast<void***>(geom::g_NametagManager);
            reinterpret_cast<SlotFn>(vtbl[0x44 / sizeof(void*)])(geom::g_NametagManager);
        }

        // 1002F469 ->same optional guard, now backed by the real reflection owner.
        if (Rend::g_EnvMapManager)
            static_cast<Rend::EnvMapManager*>(Rend::g_EnvMapManager)->onPostLoad();
        return;
    }

    // [FRONTIER] System-info collection requires the event payload's row-sink interface.
    case 9:
        RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::handleECCoreEvent "
                        "(id 9 CECollectSystemInfo - sub_10030C00/sub_10032DB0/sub_100198A0 are "
                        "unread; this id is not posted on any load or activate path)", self, 0x00);
        return;

    // id 10, CEStartPerformanceTest / id 11, CEStopPerformanceTest
    // Pure field writes on the subobject, so the offsets stay relative to `self` exactly as the
    // instructions have them (esi+0x350/+0x354/+0x358 == Renderer +0x888/+0x88C/+0x890). They are
    // reached through rendererField rather than carved into the Renderer declaration on purpose:
    // carving a member out of an m_tail block removes it from that block's zeroing loop, which has
    // already cost this file one fault (see m_objectDrawer).
    case 10:
        rendererField<unsigned int >(self, 0x354) = 0;
        rendererField<unsigned int >(self, 0x358) = 0;
        rendererField<unsigned char>(self, 0x350) = 1;
        return;

    case 11:
        rendererField<unsigned char>(self, 0x350) = 0;
        return;

    // Unloads renderer state, environment/hemi maps, vegetation, and particle systems.
    case 13:
    {
        // 1002F4E2..1002F4F1: `mov edx, [esi-538h]; lea ecx, [esi-538h]; xor edi, edi; push edi;
        // call [edx+0B8h]` - a virtual call on the Renderer with the literal 0.
        renderer->slot_0xB8(false);

        // ClassManager must remain available during level unload.
        IBase* const envMap = classManager->getSingleton(renddx9abi::HostString("EnvMapManager"));
        if (envMap)
        {
            typedef void (__thiscall* SlotFn)(void*);
            reinterpret_cast<SlotFn>((*reinterpret_cast<void***>(envMap))[0x38 / sizeof(void*)])(
                envMap);
        }

        IBase* const hemiMap = classManager->getSingleton(renddx9abi::HostString("HemiMapManager"));
        if (hemiMap)
        {
            // +0x10 is IHemiMapManager::clear(), which IS written - Geom/HemiMapManager.
            typedef void (__thiscall* SlotFn)(void*);
            reinterpret_cast<SlotFn>((*reinterpret_cast<void***>(hemiMap))[0x10 / sizeof(void*)])(
                hemiMap);
        }

        if (geom::g_UndergrowthSystem)
            static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->unload();   // +0x1C

        if (g_OverGrowth)
        {
            // PE invokes the scalar deleting destructor (primary +0x00, argument 1), then clears
            // the global. OverGrowth's class delete returns this allocation to the engine heap.
            delete static_cast<geom::OverGrowth*>(g_OverGrowth);
            g_OverGrowth = 0;
        }

        // 1002F582..1002F590: `mov ecx, dword_1024B900; cmp ecx, edi; jz; call sub_100D4A70` - a
        // direct (non-virtual) call on the published manager, guarded by the shipped null test.
        if (geom::g_ParticleSystemManager)
        {
            geom::particleSystemManagerUnloadLevel(
                static_cast<geom::IParticleSystemManager*>(geom::g_ParticleSystemManager));
        }
        return;
    }

    // id 15, CEActivateApp
    //     *(byte*)(esi - 0x21) = *(byte*)userData;     i.e. Renderer +0x517 = the payload's byte
    case 15:
        rendererField<unsigned char>(renderer, 0x517) =
            *static_cast<const unsigned char*>(userData);
        return;

    default:
        // The shipped switch has no default arm; every other id falls straight out.
        return;
    }
}

// The one-slot table itself, now with its body behind it.
void* const kECCoreEventHandlerVtable[1] =
{
    reinterpret_cast<void*>(&renderer_handleECCoreEvent)
};

void releaseRendererPathResources(void* base)
{
    IDirect3DSurface9*& backBuffer = rendererField<IDirect3DSurface9*>(base, 0x76C);
    IDirect3DSurface9*& depth      = rendererField<IDirect3DSurface9*>(base, 0x770);
    if (backBuffer) backBuffer->Release();
    if (depth) depth->Release();
    backBuffer = 0;
    depth = 0;

    static_cast<Renderer*>(base)->releaseHemiScratchTexture();
    for (unsigned int offset = 0x778; offset <= 0x7C8; offset += 4)
        dx9rend::textureReleaseRef(rendererField<dx9rend::EngineTexture*>(base, offset));
    dx9rend::textureReleaseRef(rendererField<dx9rend::EngineTexture*>(base, 0x7CC));
    dx9rend::textureReleaseRef(rendererField<dx9rend::EngineTexture*>(base, 0x7D0));
    dx9rend::textureReleaseRef(rendererField<dx9rend::EngineTexture*>(base, 0x7D4));
}

// These are Renderer-owned path targets, created after LightManager's own shadow
// resources.  No failure checks were present in retail; null wrappers remain null and are handled
// by the same downstream target-selection fallbacks.
void createRendererPathResources(void* base)
{
    const int oldManaged = dx9rend::textureManagerSwapManaged(0);
    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(
        reinterpret_cast<RendDX9Base*>(base)->renderView);
    float* const viewport = view->getViewport();
    const float scale = rendererField<float>(base, 0x58C);
    const unsigned int scaledWidth = static_cast<unsigned int>(viewport[2] / scale);
    const unsigned int scaledHeight = static_cast<unsigned int>(viewport[3] / scale);
    static_cast<Renderer*>(base)->setHemiScratchTexture(
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            scaledWidth, scaledHeight, D3DFMT_A8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0)));

    const bool fullSize = dx9rend::deviceCapsSupportsFullSizeRenderTargets();
    rendererField<unsigned char>(base, 0x6BC) = fullSize ? 1u : 0u;
    const unsigned int width = fullSize ? static_cast<unsigned int>(viewport[2]) : 512u;
    const unsigned int height = fullSize ? static_cast<unsigned int>(viewport[3]) : 512u;
    const unsigned int quarterWidth = fullSize ? width / 4u : 128u;
    const unsigned int quarterHeight = fullSize ? height / 4u : 128u;

    rendererField<dx9rend::EngineTexture*>(base, 0x788) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            width, height, D3DFMT_A8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0));
    rendererField<dx9rend::EngineTexture*>(base, 0x7A0) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            width, fullSize ? height / 4u : 128u, D3DFMT_A8R8G8B8, 1,
            D3DUSAGE_RENDERTARGET, 0));
    for (unsigned int offset = 0x7A4; offset <= 0x7C8; offset += 4)
    {
        rendererField<dx9rend::EngineTexture*>(base, offset) =
            static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
                quarterWidth, quarterHeight, D3DFMT_A8R8G8B8, 1,
                D3DUSAGE_RENDERTARGET, 0));
    }
    dx9rend::textureManagerSwapManaged(oldManaged);
}

void __fastcall renderer_renderPathActivated(void* listener, void*, int path)
{
    unsigned char* const base = static_cast<unsigned char*>(listener) - 0x540;
    rendererField<unsigned int>(base, 0x544) = static_cast<unsigned int>(path);

    RendDX9Base* const renderer = reinterpret_cast<RendDX9Base*>(base);
    renderer->device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
                                    &rendererField<IDirect3DSurface9*>(base, 0x76C));
    renderer->device->GetDepthStencilSurface(
        &rendererField<IDirect3DSurface9*>(base, 0x770));
    dx9rend::stateManagerSetRenderTarget(
        0, rendererField<IDirect3DSurface9*>(base, 0x76C));
    dx9rend::stateManagerSetDepthStencilSurface(
        rendererField<IDirect3DSurface9*>(base, 0x770));

    if (geom::g_LightManager)
        geom::createShadowResources(geom::g_LightManager);
    createRendererPathResources(base);
}

void __fastcall renderer_renderPathDeactivated(void* listener, void*)
{
    releaseRendererPathResources(static_cast<unsigned char*>(listener) - 0x540);
}

void __fastcall renderer_renderPathNull(void*, void*) {}
void __fastcall renderer_renderPathNullWithPath(void*, void*, int) {}
int __fastcall renderer_getCurrentRenderPath(void* listener, void*)
{
    return *reinterpret_cast<int*>(static_cast<unsigned char*>(listener) + 4);
}
void* __fastcall renderer_getRenderPathName(void*, void*, void* out)
{
    RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::getRenderPathListenerName", out, 0x14);
    return out;
}

void* const kRendererRenderPathVtable[6] =
{
    reinterpret_cast<void*>(&renderer_renderPathActivated),
    reinterpret_cast<void*>(&renderer_renderPathDeactivated),
    reinterpret_cast<void*>(&renderer_renderPathNullWithPath),
    reinterpret_cast<void*>(&renderer_renderPathNull),
    reinterpret_cast<void*>(&renderer_getCurrentRenderPath),
    reinterpret_cast<void*>(&renderer_getRenderPathName)
};


// Replaces the owned overlay texture from its stored name; an empty name leaves it alone.
// Device reset and lazy frame loading share this operation.
void rendererReloadOverlayTexture(unsigned char* base)
{
    const vc71::string& name = *reinterpret_cast<const vc71::string*>(base + 0x6E0);
    if (name.size() == 0)                                   // cmp [edi+6F4h], 0 / jz
        return;
    dx9rend::textureReleaseRef(rendererField<dx9rend::EngineTexture*>(base, 0x8D8));
    rendererField<dx9rend::EngineTexture*>(base, 0x8D8) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(name));
}

// Releases overlay texture, then render-path resources on device loss.
// StateManager's higher-priority listener has already released its surface references.
void __fastcall renderer_onDeviceLost(void* listener, void*)
{
    unsigned char* const base = static_cast<unsigned char*>(listener) - 0x53C;
    dx9rend::textureReleaseRef(rendererField<dx9rend::EngineTexture*>(base, 0x8D8));
    releaseRendererPathResources(base);
}

// Reacquires back/depth surfaces, rebuilds path resources, and reloads the overlay.
// LightManager restores its own shadow resources; D3D acquisition results are unchecked.
void __fastcall renderer_onDeviceReset(void* listener, void*)
{
    unsigned char* const base = static_cast<unsigned char*>(listener) - 0x53C;
    RendDX9Base* const renderer = reinterpret_cast<RendDX9Base*>(base);

    renderer->device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO,
                                    &rendererField<IDirect3DSurface9*>(base, 0x76C));
    renderer->device->GetDepthStencilSurface(&rendererField<IDirect3DSurface9*>(base, 0x770));
    dx9rend::stateManagerSetRenderTarget(0, rendererField<IDirect3DSurface9*>(base, 0x76C));
    dx9rend::stateManagerSetDepthStencilSurface(rendererField<IDirect3DSurface9*>(base, 0x770));
    createRendererPathResources(base);
    rendererReloadOverlayTexture(base);
}

// 0x10027FC0 - `ret`.
void __fastcall renderer_onDeviceDestroy(void*, void*) {}

void* const kRendererDeviceLostVtable[3] =
{
    reinterpret_cast<void*>(&renderer_onDeviceLost),      // +0x00
    reinterpret_cast<void*>(&renderer_onDeviceReset),     // +0x04
    reinterpret_cast<void*>(&renderer_onDeviceDestroy)    // +0x08
};

} // anonymous namespace


// Initializes renderer state. Zero-filled opaque storage differs from allocator leftovers
// for fields the original constructor did not write.

Renderer::Renderer()
    : m_evHandlerVptr(0)
    , m_deviceLostVptr(0)
    , m_renderPathVptr(0)
    , m_unknown544(0)
    , m_refCount(1)          // mov dword ptr [esi+548h], 1
{
    for (unsigned int i = 0; i < sizeof(m_baseStorage); ++i) m_baseStorage[i] = 0;
    for (unsigned int i = 0; i < sizeof(m_tail1aBeforeRenderAheadLimit); ++i)
        m_tail1aBeforeRenderAheadLimit[i] = 0;
    m_renderAheadLimit = 0;
    for (unsigned int i = 0; i < sizeof(m_tail1aAfterRenderAheadLimit); ++i)
        m_tail1aAfterRenderAheadLimit[i] = 0;
    // must BE zeroed, AND IT IS NOT optional. initialize step 10 does `if (m_objectDrawer)
    // release it` before replacing it, so a member left holding allocator garbage is dispatched
    // through as a vtable on the first boot. It was inside m_tail1a's zeroing until +0x5C0 was
    // named; splitting it out without adding it here cost exactly that fault.
    m_objectDrawer = 0;
    m_consoleOverlayReady = 0; m_pad5C5[0] = 0; m_pad5C5[1] = 0; m_pad5C5[2] = 0;
    m_consoleOverlay = 0; m_consoleFont = 0;
    for (unsigned int i = 0; i < sizeof(m_tail1bBeforeFrameRate); ++i)
        m_tail1bBeforeFrameRate[i] = 0;
    m_frameRate = 0.0;
    for (unsigned int i = 0; i < sizeof(m_tail1bBeforeShaderPath); ++i)
        m_tail1bBeforeShaderPath[i] = 0;
    m_shaderPath = 0;       // PE 0x1002D721: mov [esi+6CCh], ebx (ebx == 0)
    for (unsigned int i = 0; i < sizeof(m_tail1bAfterShaderPath); ++i)
        m_tail1bAfterShaderPath[i] = 0;
    m_hemiScratchTexture = 0;
    for (unsigned int i = 0; i < sizeof(m_tail1bAfterHemiScratch); ++i)
        m_tail1bAfterHemiScratch[i] = 0;
    m_hudManager = 0; m_weatherManager = 0; m_rainManager = 0;
    for (unsigned int i = 0; i < sizeof(m_tail2a); ++i) m_tail2a[i] = 0;
    m_debugGraphs[0] = 0; m_debugGraphs[1] = 0; m_debugGraphs[2] = 0;
    // Carved out of m_tail2b, so they need their own zeroing - see the note beside m_objectDrawer.
    m_frameStartTime = 0.0; m_frameWallTime = 0.0;
    for (unsigned int i = 0; i < sizeof(m_tail2b); ++i) m_tail2b[i] = 0;

    // Base construction creates the D3D device before initialize().
    constructD3DHalf(this);

    // The matrix at +0x54C. The original writes sixteen zeros then four ones, twice - once in the
    // long run of member stores and again as the last thing the constructor does. The second pass
    // is redundant; both are reproduced by this one loop because they produce the same state and
    // nothing observes the interval between them.
    for (int i = 0; i < 16; ++i)
        m_viewMatrix[i] = 0.0f;
    m_viewMatrix[0]  = 1.0f;
    m_viewMatrix[5]  = 1.0f;
    m_viewMatrix[10] = 1.0f;
    m_viewMatrix[15] = 1.0f;

    m_f58C = 1.0f;
    m_f590 = 0.040508386f;   // 0x3D25F55E, exactly
    m_near = 0.0099999998f;
    m_far  = 15.0f;
    m_f59C = 6.0f;
    m_lazy884 = 0;

    // Initialize per-frame subsystem enable flags and state.
    {
        unsigned char* self = reinterpret_cast<unsigned char*>(this);
        unsigned int*  dw   = reinterpret_cast<unsigned int*>(this);
        float*         fl   = reinterpret_cast<float*>(this);

        self[1440] = 1;              // clear the frame at the world pass's end (flags 2, z-only)
        self[1441] = 1;              // post-production requires its active() query
        m_renderAheadLimit = 2;      // the GPU-throttle fence ring uses TWO of the five fences
        dw[367] = 0;                 // ring cursor

        // the frame timer at +0x5DC - 48 bytes, QueryPerformanceCounter-backed (sub_10077710):
        // {pad, lastCounter int64 +8, accum int64 +16, dt double +24, total double +32,
        // 1/frequency double +40}
        {
            unsigned char* timer = static_cast<unsigned char*>(::renddx9abi::engineAlloc(48));
            if (timer)
            {
                ::memset(timer, 0, 48);
                LARGE_INTEGER freq;
                ::QueryPerformanceFrequency(&freq);
                *reinterpret_cast<double*>(timer + 40) =
                    1.0 / static_cast<double>(freq.QuadPart);
                LARGE_INTEGER now;
                ::QueryPerformanceCounter(&now);
                *reinterpret_cast<LONGLONG*>(timer + 8) = now.QuadPart;
            }
            dw[375] = reinterpret_cast<unsigned int>(timer);
        }

        self[1693] = 1;              // fog block
        self[1694] = 1;
        self[1695] = 1;              // particles
        self[1696] = 1;              // decals
        self[1697] = 1;
        self[1698] = 1;
        self[1699] = 1;              // skinned meshes
        self[1700] = 1;
        self[1701] = 1;
        self[1702] = 1;              // undergrowth
        self[1703] = 1;
        self[1704] = 1;              // nametags
        self[1705] = 1;
        self[1706] = 0;
        self[1707] = 1;
        self[1712] = 0;              // the debug graphs stay hidden
        self[1713] = 1;              // the HudManager post-frame call
        self[1714] = 1;              // sky
        self[1715] = 1;
        self[1716] = 1;              // the console query
        self[1717] = 1;              // post-production
        self[1756] = 1;
        self[1757] = 0;

        fl[432] = 4.0f;              // the fog override triple the fog block swaps in
        fl[433] = 5.0f;
        fl[434] = 100.0f;
        fl[447] = 0.25f;
        fl[448] = 0.75f;
        fl[449] = 0.75f;
        fl[451] = 10.0f;
        fl[455] = 10.0f;
        fl[459] = 10.0f;
        fl[463] = 9999.0f;
        fl[464] = 9999.0f;
        fl[465] = 25.0f;             // the geometry-streaming budget - the scan runs every frame
        dw[466] = 10;
        dw[474] = 2;
        dw[502] = 0xFFFFFFFFu;       // the FPS histogram's -1 triple
        dw[503] = 0xFFFFFFFFu;
        dw[504] = 0xFFFFFFFFu;
        dw[506] = 100;
        fl[520] = 10.0f;             // the FPS bucket scale
        fl[531] = 1.0f;
        dw[536] = 8;
        fl[537] = 2.0f;
        fl[556] = 1.0f;
        fl[557] = 1.0f;
        fl[558] = 1.0f;
        // The two vc71::string members at dword 440 and 538 stay zeroed storage: a zeroed string
        // reads as "" through c_str(), and nothing recovered writes or reads them yet.
    }

    // The renderer owns and constructs its render view.
    m_evHandlerVptr  = const_cast<void**>(kECCoreEventHandlerVtable);
    m_deviceLostVptr = const_cast<void**>(kRendererDeviceLostVtable);
    m_renderPathVptr = const_cast<void**>(kRendererRenderPathVtable);
    g_Renderer = this;
    // 0x1002D8FE..0x1002D927: `push 1` ... `lea eax, [esi+53Ch]` ...
    // `mov ecx, esi; mov dword_1023D098, esi; call sub_10009D60` - the Renderer's own device-lost
    // listener at priority 1, registered right after g_Renderer is published and immediately
    // before the render-path listener (0x1002D935). ecx is this renderer, so the node joins its own
    // list at +0x5C - the one the base constructor above just built.
    registerDeviceLostResetListener(static_cast<void*>(&m_deviceLostVptr), 1);
    registerRenderPathListener(&m_renderPathVptr);
    reinterpret_cast<RendDX9Base*>(this)->renderView = Rend::createRenderViewDirect();

    // Not reproduced, and each is a separate gap rather than one vague one:
    //   - RendDX9_base_ctor (0x1000B940), the whole base object including the D3D device
    //   - the 48-byte sub-object at +0x5DC (bf_malloc(48) -> sub_10077710)
    //   - the two std::string members at +0x6E0 and +0x868
    //   - roughly 120 further scalar members between +0x5A0 and +0x8DF
    // [FRONTIER] dice::hfe::dx9rend::Renderer::Renderer - the base half and the tail members
}

void Renderer::addRef()
{
    ++m_refCount;
}

int Renderer::getRef()
{
    return m_refCount;
}

int Renderer::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        // The shipped release dispatches scalar deleting destructor slot +0xB4.  That wider
        // destructor remains a named frontier in this reconstruction, whose current release seam
        // frees the allocation directly.  Do not let that pre-existing shortcut leak the three
        // now-owned display vectors: this is the recovered sub_1000C9B0 portion of the base dtor.
        clearDisplayModeCopies(this);
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// 0x1000CAC0, read in full: `this` for IID_IBase and IID_IRenderer, null for anything else.
void* Renderer::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IRenderer)
        return const_cast<Renderer*>(this);
    return 0;
}


// Copies the display mode, updates views and presentation parameters, then resets the device.
// Returns the reset loop's result. Width must be nonzero for aspect calculation.

bool Renderer::setDisplayMode(const HWndBf2TempSlt& mode)
{
    copyDisplayModeFrom(this, &mode);
    return applyCurrentDisplayMode();
}

// setDisplayMode after sub_1000CA40, exactly as it was written inline above
// before the split; nothing below changed.
bool Renderer::applyCurrentDisplayMode()
{
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(this);
    const float height = static_cast<float>(b->modeHeight);
    const float width  = static_cast<float>(b->modeWidth);
    const float aspect = height / width;     // shipped body has no zero-width guard

    // Preserve the original's repeated virtual getters rather than caching their results.
    if (getRenderView())
    {
        Rend::renderViewSetAspect(reinterpret_cast<Rend::RenderView*>(getRenderView()), aspect);
        Rend::renderViewSetViewport(reinterpret_cast<Rend::RenderView*>(getRenderView()),
                                    0.0f, 0.0f, width, height);
    }
    if (getRenderViewDup())
    {
        Rend::renderViewSetAspect(reinterpret_cast<Rend::RenderView*>(getRenderViewDup()), aspect);
        Rend::renderViewSetViewport(reinterpret_cast<Rend::RenderView*>(getRenderViewDup()),
                                    0.0f, 0.0f, width, height);
    }
    if (getRenderView2())
    {
        Rend::renderViewSetAspect(reinterpret_cast<Rend::RenderView*>(getRenderView2()), aspect);
        Rend::renderViewSetViewport(reinterpret_cast<Rend::RenderView*>(getRenderView2()),
                                    0.0f, 0.0f, width, height);
    }

    buildPresentParameters(this);
    return handleLostDevice(this);         // sub_10009BA0, its answer returned as-is
}

// [CHOSEN, not recovered] Reapplies the current mode without copying its compatibility
// container into the native host carrier. The original copy-to-self leaves mode data unchanged.
bool rendererApplyCurrentDisplayMode(void* renderer)
{
    return static_cast<Renderer*>(renderer)->applyCurrentDisplayMode();
}


// Initializes renderer views, subsystem singletons, event handling and debug graphs.
// Requires the registered geometry and render-service factories.

bool Renderer::initialize()
{
    RD9_TRACE("dice::hfe::dx9rend::Renderer::initialize", 0x1C);

    // step 1: the lazy one-byte allocation at +0x884
    // if (!*((_DWORD *)self + 545)) *((_DWORD *)self + 545) = bf_malloc(1);
    // ONE byte, and that is what the instruction says - push 1; call sub_1005F320. What it is for
    // is [UNRESOLVED]; it is reproduced because it is cheap, idempotent, and its absence would
    // leave a null where the original leaves a pointer.
    if (!m_lazy884)
        m_lazy884 = ::renddx9abi::engineAlloc(1);

    // step 2: the view matrix translation row
    // Indices 351/352/353 are elements 12/13/14 of the matrix at +0x54C - the translation row of a
    // row-major 4x4. So this is the camera being placed at (0, 100, 0).
    m_viewMatrix[12] = 0.0f;
    m_viewMatrix[13] = 100.0f;
    m_viewMatrix[14] = 0.0f;

    // step 3: hand that matrix to the render view
    // v2 = getRenderView(self); (*(v2 + 96))(v2, (char *)self + 1356);
    // +96 is +0x60, RenderView::setViewMatrix, and self+1356 is exactly m_viewMatrix.
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(this);
    Rend::RenderView* view = static_cast<Rend::RenderView*>(b->renderView);
    if (!view)
    {
        // Cannot happen while the constructor above runs, and worth saying out loud rather than
        // dereferencing on faith - the original has no such check.
        RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize (no RenderView)", 0x1C);
        return false;
    }
    Rend::renderViewSetViewMatrix(view, m_viewMatrix);
    ::renddx9abi::logf("Renderer::initialize: steps 1-3 done - view matrix (0,100,0) pushed to the "
                       "RenderView at 0x%08X", ::renddx9abi::addr(view));

    // Aspect is height/width. Require a populated display mode before updating the view.
    if (b->modeWidth == 0 || b->modeHeight == 0)
    {
        RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize step4 "
                     "(display mode is 0x0 - hWndBf2TempSlt was not readable)", 0x1C);
        return false;
    }

    const float invAspect = static_cast<float>(static_cast<double>(b->modeHeight) /
                                               static_cast<double>(b->modeWidth));
    Rend::renderViewSetAspect(view, invAspect);
    Rend::renderViewSetViewport(view, 0.0f, 0.0f,
                                static_cast<float>(b->modeWidth),
                                static_cast<float>(b->modeHeight));
    Rend::renderViewSetFov(view, 1.2f);
    Rend::renderViewSetFarPlane(view, 400.0f);

    ::renddx9abi::logf("Renderer::initialize: step 4 done - %ux%u, aspect(h/w)=%.4f, fov=1.2rad, "
                       "far=400", b->modeWidth, b->modeHeight, invAspect);

    // Select the configured render path after constructing its listener list.
    const int renderPath = readIntSetting("GSRenderPath", 3);
    setActiveRenderPath(b->renderPathList, renderPath);
    ::renddx9abi::logf("Renderer::initialize: step 5 done - GSRenderPath=%d", renderPath);

    // Assign proportional and fixed-width fonts to the debug text writers.
    setDebugTextWriterFont("DebugTextWriter",      "Fonts/BF1942.font");
    setDebugTextWriterFont("DebugTextWriterFixed", "Fonts/system_8x11.font");

    // step 7: three singletons, cached and not dereferenced
    // All three are ModuleGeom's and none has a body, so all three are null here. That is
    // survivable precisely because initialize only stores them; nothing in this function calls
    // through them. A later consumer will fault, and that consumer will be its own frontier.
    m_hudManager     = classManager ? classManager->getSingleton(renddx9abi::HostString("HudManager"))     : 0;
    m_weatherManager = classManager ? classManager->getSingleton(renddx9abi::HostString("WeatherManager")) : 0;
    m_rainManager    = classManager ? classManager->getSingleton(renddx9abi::HostString("RainManager"))    : 0;
    // HudManager is NOT one of ours. It is BF2.exe's own singleton, so it comes back live even
    // with every geom class missing; WeatherManager and RainManager ARE ModuleGeom's and stay null
    // until those classes are built. An earlier version of this line called all three ours.
    ::renddx9abi::logf("Renderer::initialize: step 7 done - HudManager=0x%08X (BF2's own) "
                       "WeatherManager=0x%08X RainManager=0x%08X (ModuleGeom's, null until built)",
                       ::renddx9abi::addr(m_hudManager),
                       ::renddx9abi::addr(m_weatherManager),
                       ::renddx9abi::addr(m_rainManager));

    // Create a separate secondary view with far range m_far - m_near and FOV 0.82.
    IBase* view2 = classManager
                 ? classManager->createInstance(Rend::CID_RenderView, Rend::IID_IRenderView, 0)
                 : 0;
    b->renderView2 = view2;

    if (!view2)
    {
        // The original has no such check. Ours exists because a null here would be dispatched on
        // two lines later, and saying which registration is missing beats an access violation.
        RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize step8 "
                     "(createInstance(820301, 820201) returned null - the Rend::RenderView class is "
                     "not registered)", 0x1C);
        return false;
    }

    {
        const float farPlane = m_far - m_near;                 // 15.0 - 0.01
        IRenderView* const renderView = static_cast<IRenderView*>(view2);
        renderView->setFarPlane(farPlane);
        renderView->setFov(0.82f);

        ::renddx9abi::logf("Renderer::initialize: step 8 done - second RenderView 0x%08X, "
                           "far=%.4f fov=0.82rad", ::renddx9abi::addr(view2), farPlane);
    }

    // step 9: the console font and overlay
    initializeFontAndConsoleOverlay();

    // Release the previous drawer, create its replacement and publish the same pointer globally.
    if (m_objectDrawer)
    {
        void** vtbl = *reinterpret_cast<void***>(m_objectDrawer);
        typedef int (__thiscall* ReleaseFn)(void*);
        reinterpret_cast<ReleaseFn>(vtbl[0x08 / 4])(m_objectDrawer);
    }

    // Logged before the call on purpose. This is the first time this image asks BF2's ClassManager
    // for a class nobody registered - every earlier createInstance named a class one of our modules
    // had just registered. If the host's miss path is not the graceful null that ModuleGeom's
    // sixteen null singletons imply, the fault lands inside this call, and without a line here that
    // would be indistinguishable from a fault before it.
    ::renddx9abi::logf("Renderer::initialize: step 10 - createInstance(cid=0x%08X iid=0x%08X) for "
                       "dice.hfe.geom.ObjectDrawer, which is NOT registered in this build",
                       geom::CID_ObjectDrawer, geom::IID_IObjectDrawer);

    m_objectDrawer = classManager
                   ? classManager->createInstance(geom::CID_ObjectDrawer, geom::IID_IObjectDrawer, 0)
                   : 0;
    g_ObjectDrawer = static_cast<IBase*>(m_objectDrawer);

    ::renddx9abi::logf("Renderer::initialize: step 10 done - ObjectDrawer=0x%08X%s",
                       ::renddx9abi::addr(m_objectDrawer),
                       m_objectDrawer ? "" : " (NULL - dice.hfe.geom.ObjectDrawer has no factory "
                                             "body yet; the original stores the null too)");

    // Optimize eagerly when on-demand optimization is disabled, then always clear
    // optimizer registrations. Class names and registry keys are separate strings.
    if (geom::g_GeometryTemplateManager)
    {
        IBase*  gtmBase = geom::g_GeometryTemplateManager;
        void**  vtbl    = *reinterpret_cast<void***>(gtmBase);
        typedef bool (__thiscall* RegisterFn)(void*, const vc71::string&, const vc71::string&);
        typedef void (__thiscall* VoidFn)(void*);

        if (!readBoolSetting("GSOptimizeGeomOnDemand", false))
        {
            RegisterFn registerOptimizer = reinterpret_cast<RegisterFn>(vtbl[0x50 / 4]);
            registerOptimizer(gtmBase, vc71::string("BundledMeshOptimizer"),
                                       vc71::string("BundledMeshOptimizer"));
            registerOptimizer(gtmBase, vc71::string("StaticMeshOptimizer"),
                                       vc71::string("StaticMeshOptimizer"));
            reinterpret_cast<VoidFn>(vtbl[0x54 / 4])(gtmBase);   // runOptimizers
        }

        reinterpret_cast<VoidFn>(vtbl[0x58 / 4])(gtmBase);       // clearOptimizers
        ::renddx9abi::logf("Renderer::initialize: step 11 done - GeometryTemplateManager "
                           "0x%08X swept", ::renddx9abi::addr(gtmBase));
    }
    else
    {
        // The original has no such check - g_GeometryTemplateManager is a singleton it assumes is
        // live. Ours says which registration is missing rather than faulting.
        RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize step11 "
                     "(g_GeometryTemplateManager is null - ModuleGeom's singleton was not created)",
                     0x1C);
        return false;
    }

    // Register the renderer's ECCore subobject after installing its handler table.
    m_evHandlerVptr = const_cast<void**>(kECCoreEventHandlerVtable);

    if (!eventManager ||
        !eventManager->registerEventHandler(
             ECCore, reinterpret_cast<IEventListener*>(&m_evHandlerVptr), 1000))
    {
        // The shipped failure branch, word for word from BF2Render.cpp:437.
        ::renddx9abi::logf("BF2Render.cpp:437: Failed to register BF2Render as event handler for "
                           "ECCore. (eventManager=0x%08X, handler=0x%08X, priority 1000)",
                           ::renddx9abi::addr(eventManager),
                           ::renddx9abi::addr(&m_evHandlerVptr));
    }
    else
    {
        ::renddx9abi::logf("Renderer::initialize: step 12 done - ECCore handler 0x%08X registered "
                           "at priority 1000 (ids 2/3/9/10/11/13/15 are now live)",
                           ::renddx9abi::addr(&m_evHandlerVptr));
    }

    // Create the five event queries used as the frame-latency fence ring.
    {
        void** slot = reinterpret_cast<void**>(reinterpret_cast<char*>(this) + 0x5A4);
        for (int i = 0; i < 5; ++i)
            slot[i] = queryManagerCreateQuery(D3DQUERYTYPE_EVENT);

        ::renddx9abi::logf("Renderer::initialize: step 13 done - five D3DQUERYTYPE_EVENT fences at "
                           "+0x5A4: %08X %08X %08X %08X %08X",
                           ::renddx9abi::addr(slot[0]), ::renddx9abi::addr(slot[1]),
                           ::renddx9abi::addr(slot[2]), ::renddx9abi::addr(slot[3]),
                           ::renddx9abi::addr(slot[4]));
    }

    // [UNRESOLVED] Base flag +0x102 enables the following branch.
    // [FRONTIER] Its handler sub_10042150 remains unavailable.
    {
        const unsigned char* base = reinterpret_cast<const unsigned char*>(g_rendererBase);
        if (base && base[0x102])
        {
            RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize step14 "
                         "(the flag at base+0x102 is set; sub_10042150 is unwritten)", 0x1C);
        }
    }

    // VideoOptions is embedded at +0x3F8; the field address is the receiver.
    reinterpret_cast<RendDX9Base*>(this)->embeddedVideoOptions()->apply();

    // Loads deferred cached textures while the resource-ready flag is set, then ends loading.
    textureManagerSweep();
    resourceManagerSetReady(0);

    // Create the three debug graphs with initialized history before adding samples.
    // Graph creation must succeed; registration with the graph renderer is optional.
    {
        struct GraphSpec
        {
            const char*  name;
            float        rect[4];
            unsigned int colours[3];
            float        rangeLo, rangeHi;
            float        arg1C;
        };

        static const GraphSpec kGraphs[3] =
        {

            { "Framerate",      { 15.0f, 300.0f, 250.0f, 250.0f },
              { 0xFFFFC988u, 0xFFFF0000u, 0xFF00FF00u },  60.0f,  90.0f, 100.0f },

            { "Rendering time", { 274.0f, 300.0f, 250.0f, 250.0f },
              { 0xFFFFC988u, 0xFF00FF00u, 0xFFFF0000u },  10.0f,  20.0f,  60.0f },

            { "Draw calls",     { 520.0f, 300.0f, 250.0f, 250.0f },
              { 0xFFFFC988u, 0xFFFFC988u, 0xFFFF0000u }, 100.0f, 350.0f, 584.0f }
        };

        // The singleton, looked up once. The original does this between the first and second graph;
        // hoisting it changes nothing observable and keeps the loop uniform.
        IBase* graphRenderer = 0;
        if (classManager)
            graphRenderer = classManager->getSingleton(renddx9abi::HostString("DebugLineGraphRenderer"));

        for (int i = 0; i < 3; ++i)
        {
            const GraphSpec& spec = kGraphs[i];

            IBase* g = classManager
                     ? classManager->createInstance(geom::CID_DebugLineGraph,
                                                    geom::IID_IDebugLineGraph, 0)
                     : 0;
            m_debugGraphs[i] = g;

            if (!g)
            {
                // The original has no such check - it dereferences immediately. Ours exists because
                // a null here would fault on the next line and say nothing about why.
                RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize step17 "
                             "(createInstance(81038, 80039) returned null - the DebugLineGraph "
                             "class is registered, so this is a ClassManager failure)", 0x1C);
                return false;
            }

            void** v = *reinterpret_cast<void***>(g);
            typedef void (__thiscall* IntFn)(void*, int);
            typedef void (__thiscall* StrFn)(void*, const vc71::string&);
            typedef void (__thiscall* PtrFn)(void*, const float*);
            typedef void (__thiscall* Col3Fn)(void*, unsigned, unsigned, unsigned);
            typedef void (__thiscall* Flt2Fn)(void*, float, float);
            typedef void (__thiscall* FltFn)(void*, float);
            typedef void (__thiscall* IntFltFn)(void*, int, float);

            // +0x3C first, and it is the one that matters: it allocates the ring buffer every later
            // call writes into. See DebugLineGraphD3d.cpp - it is a named frontier.
            reinterpret_cast<IntFn>(v[0x3C / 4])(g, 250);

            reinterpret_cast<StrFn>(v[0x18 / 4])(g, vc71::string(spec.name));
            reinterpret_cast<PtrFn>(v[0x10 / 4])(g, spec.rect);
            reinterpret_cast<Col3Fn>(v[0x44 / 4])(g, spec.colours[0], spec.colours[1],
                                                  spec.colours[2]);
            reinterpret_cast<Flt2Fn>(v[0x4C / 4])(g, spec.rangeLo, spec.rangeHi);
            reinterpret_cast<IntFn>(v[0x50 / 4])(g, 1);
            reinterpret_cast<IntFltFn>(v[0x1C / 4])(g, 0, spec.arg1C);
            reinterpret_cast<FltFn>(v[0x40 / 4])(g, 1.0f);
            reinterpret_cast<IntFn>(v[0x28 / 4])(g, 0);

            if (graphRenderer)
            {
                void** rv = *reinterpret_cast<void***>(graphRenderer);
                typedef void (__thiscall* AddFn)(void*, void*);
                reinterpret_cast<AddFn>(rv[0x10 / 4])(graphRenderer, g);
            }
        }

        ::renddx9abi::logf("Renderer::initialize: step 17 done - three debug line graphs at "
                           "%08X %08X %08X; DebugLineGraphRenderer=%08X",
                           ::renddx9abi::addr(m_debugGraphs[0]),
                           ::renddx9abi::addr(m_debugGraphs[1]),
                           ::renddx9abi::addr(m_debugGraphs[2]),
                           ::renddx9abi::addr(graphRenderer));
    }

    // that IS THE whole OF initialize. Every statement of the shipped body at 0x1002C8C0 is now
    // written, and the original's final act is `return 1`.
    ::renddx9abi::logf("Renderer::initialize: COMPLETE - all 17 steps executed, returning true");
    return true;
}


// Creates the console font and overlay texture using the initialized renderer services.
// A font-load failure logs the existing message but still proceeds to overlay creation.

void Renderer::initializeFontAndConsoleOverlay()
{
    RD9_TRACE("dice::hfe::dx9rend::Renderer::initializeFontAndConsoleOverlay", 0x00);

    // createInstance(81007, 80001, 0) - dice.hfe.geom.Font.Ported. The classId is read as data at
    // (dword_1020AEAC == 0x13C6F == 81007).
    m_consoleFont = classManager ? classManager->createInstance(81007u, 80001u, 0) : 0;

    if (!m_consoleFont)
    {
        RD9_FRONTIER("dice::hfe::geom::Font::Ported "
                     "(createInstance(81007, 80001) returned null - the class is unregistered "
                     "because its constructor needs the BufferManager and the shader registry)",
                     0x00);
        return;
    }

    {
        // font->vslot+0x10(path). The bool decides only which message is logged.
        void** vtbl = *reinterpret_cast<void***>(m_consoleFont);
        typedef bool (__thiscall* LoadFn)(void*, const vc71::string&);
        vc71::string path("Fonts/BF1942.font");
        const bool loaded = reinterpret_cast<LoadFn>(vtbl[0x10 / 4])(m_consoleFont, path);

        if (!loaded)
        {
            // BF2Render.cpp:761. And then it carries on - see the note above.
            ::renddx9abi::logf("BF2Render.cpp:761 Failed to load font Fonts/BF1942.font, aborting.");
            ::renddx9abi::logf("  ... and does not abort. The shipped body falls through to the "
                               "overlay texture and returns 1; the message is inaccurate in the "
                               "original and is reproduced as-is.");
        }
    }

    // createInstance(81008, 80002, 0) - dice.hfe.geom.OverlayTexture, classId read at 0x1007D866.
    m_consoleOverlay = classManager ? classManager->createInstance(81008u, 80002u, 0) : 0;

    if (!m_consoleOverlay)
    {
        RD9_FRONTIER("dice::hfe::geom::OverlayTexture "
                     "(createInstance(81008, 80002) returned null - the class is unregistered "
                     "because its constructor needs the shader and parameter registries)",
                     0x00);
        return;
    }

    {
        void** vtbl = *reinterpret_cast<void***>(m_consoleOverlay);
        typedef void (__thiscall* SetTextureFn)(void*, const vc71::string&);
        typedef void (__thiscall* SetFloatFn)(void*, float);

        vc71::string texture("Menu/Console/white");
        reinterpret_cast<SetTextureFn>(vtbl[0x2C / 4])(m_consoleOverlay, texture);

        // 1061997773 == 0x3F4CCCCD == 0.8f
        reinterpret_cast<SetFloatFn>(vtbl[0x28 / 4])(m_consoleOverlay, 0.8f);
    }

    m_consoleOverlayReady = 1;
    ::renddx9abi::logf("initializeFontAndConsoleOverlay: font=0x%08X overlay=0x%08X",
                       ::renddx9abi::addr(m_consoleFont), ::renddx9abi::addr(m_consoleOverlay));
}


// Step 6's helper. Not a function dice had - the original writes the pair out longhand - but the
// two calls are identical apart from their two strings, and one helper keeps the raw-offset
// dispatch in a single place where its assumptions can be stated once.
// [CHOSEN, not recovered] - the factoring only.

void Renderer::setDebugTextWriterFont(const char* singletonName, const char* fontPath)
{
    IBase* writer = classManager ? classManager->getSingleton(renddx9abi::HostString(singletonName)) : 0;

    if (!writer)
    {
        // The original dispatches +0x10 on this unchecked. Naming the missing registration is worth
        // more than reproducing the access violation.
        RD9_FRONTIER("dice::hfe::dx9rend::Renderer::initialize step6 "
                     "(a debug text writer singleton is null; the original dispatches +0x10 on it "
                     "unchecked)", 0x1C);
        return;
    }

    void** vtbl = *reinterpret_cast<void***>(writer);
    typedef bool (__thiscall* SetFontFn)(void*, const vc71::string&);

    vc71::string path(fontPath);
    const bool ok = reinterpret_cast<SetFontFn>(vtbl[0x10 / 4])(writer, path);

    ::renddx9abi::logf("Renderer::initialize: step 6 - %s.setFont(\"%s\") -> %s "
                       "(the original discards this bool)",
                       singletonName, fontPath, ok ? "true" : "false");
}

// Historical device-creation evidence is archived below.
// The host supplies display-mode inputs: the depth-format vector must be nonempty;
// the multisample vector may be empty. Do not infer these inputs from the D3D device.
#if 0
static void initialize_step4_original_note()
{

}
#endif

// +0x44, the complete body stores its one pointer argument at +0xF8,
// materializes true in AL, then returns with ret 4. There is no addRef/release or null guard.
// [INFERRED] source name and IBase* spelling: BF2.exe Ref2System's paint brackets pass their
// RenderView object through this slot. The existing +0xF8 overlay remains RW-56's named debt.
bool Renderer::setRenderView(IBase* view)
{
    reinterpret_cast<RendDX9Base*>(this)->renderView = view;
    return true;
}

// +0x4C, byte-for-byte twin of +0x44 in the retail image.  HemiMapManager invokes
// both setters in order when installing and restoring its temporary orthographic RenderView.
bool Renderer::setRenderViewDup(IBase* view)
{
    reinterpret_cast<RendDX9Base*>(this)->renderView = view;
    return true;
}

// dx9rend_Renderer_getRenderView, one
// instruction returning the base's +0xF8 member. Confirmed by BF2's own camera/fog consumers at
// 0x722030, 0x7255D0 and 0x72CFD0, and now by the QuadDrawer flush's matrix-parameter upload.
IRenderView* Renderer::getRenderView()
{
    return static_cast<IRenderView*>(reinterpret_cast<RendDX9Base*>(this)->renderView);
}

// +0x50, the ICF-folded twin (IDA labels the shared body with a ConcRT name; it is
// `return this[62]`, the same +0xF8 read). +0xD4, the second view, +0xFC.
IRenderView* Renderer::getRenderViewDup()
{
    return static_cast<IRenderView*>(reinterpret_cast<RendDX9Base*>(this)->renderView);
}
IRenderView* Renderer::getRenderView2()
{
    return static_cast<IRenderView*>(reinterpret_cast<RendDX9Base*>(this)->renderView2);
}

// The scene-bracket slots forward to the RendDX9Draw.cpp bodies (the shipped vtable holds the base
// functions directly; the forward keeps the this-adjustment explicit).
bool Renderer::beginScene(const unsigned int* rgb, char doClear)
{
    return rendererBeginScene(rgb, doClear != 0);
}
bool Renderer::endScene()
{
    return rendererEndScene();
}
bool Renderer::present(unsigned int mode)
{
    return rendererPresent(mode);
}

namespace {

void viewSetNearPlane(void* view, float nearZ)
{
    static_cast<IRenderView*>(view)->setNearPlane(nearZ);
}
void viewSetFarPlane(void* view, float farZ)
{
    static_cast<IRenderView*>(view)->setFarPlane(farZ);
}
void viewSetViewMatrix(void* view, const float* m)
{
    static_cast<IRenderView*>(view)->setViewMatrix(m);
}
float* viewGetMatrixPtr(void* view)
{
    return static_cast<IRenderView*>(view)->getViewMatrixPtr()->m;
}
void* viewGetFrustumPtr(void* view)
{
    return static_cast<IRenderView*>(view)->getFrustumPtr();
}

// sub_1000A6B0 - is multisampling active: the forced byte at base+0x514, the disabled byte at
// +0x515, else the first entry of the multisample vector (+0xC0), which BF2 leaves empty.
bool isMultisampleActive()
{
    RendDX9Base* const base = static_cast<RendDX9Base*>(g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    unsigned int* const first = base->multiSampleFirst;
    unsigned int* const last  = base->multiSampleLast;
    if (first && ((last - first) >> 1))
        return *reinterpret_cast<int*>(first) > 0;
    return false;
}

// PE 0x10030257..0x100303A1 repeats this complete predicate before every atoc transition.  Do not
// cache the answer across OverGrowth, queue flushing, or Undergrowth: retail rereads all three
// inputs and calls isMultisampleActive independently at each site.
bool shouldToggleTransparencyAA()
{
    if (!g_transparencyAARenderBracketsEnabled)
        return false;
    if (!isMultisampleActive())
        return false;
    return static_cast<RendDX9Base*>(g_rendererBase)->hasAlphaToCoverage != 0;
}

// sub_10077640 - tick the 48-byte QPC frame timer at Renderer dword 375.
void frameTimerTick(void* timer)
{
    if (!timer)
        return;
    unsigned char* t = static_cast<unsigned char*>(timer);
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    const LONGLONG delta = now.QuadPart - *reinterpret_cast<LONGLONG*>(t + 8);
    *reinterpret_cast<LONGLONG*>(t + 16) += delta;
    *reinterpret_cast<LONGLONG*>(t + 8) = now.QuadPart;
    const double invFreq = *reinterpret_cast<double*>(t + 40);
    *reinterpret_cast<double*>(t + 24) = static_cast<double>(delta) * invFreq;
    *reinterpret_cast<double*>(t + 32) =
        static_cast<double>(*reinterpret_cast<LONGLONG*>(t + 16)) * invFreq;
}

// Reads the current postproduction owner flags.
bool postProductionActive()
{
    unsigned char* o = reinterpret_cast<unsigned char*>(g_PostProductionFrameWorkOwner);
    if (!o)
        return false;
    if (*reinterpret_cast<unsigned int*>(o + 512) || o[36])
        return true;
    return *reinterpret_cast<unsigned int*>(o + 356) == 1
        || o[39]
        || *reinterpret_cast<unsigned int*>(o + 544)
        || o[250]
        || *reinterpret_cast<float*>(o + 224) > 0.0f;
}

// sub_100F7AD0 - is the camera under water: view matrix Y against the constants block's
// WaterHeight. With the block withheld the answer is "above water", the menu truth.
bool isCameraUnderWater()
{
    if (!g_raShaderConstants)
        return false;
    float* m = viewGetMatrixPtr(rendererGetRenderView());
    return m[13] < *reinterpret_cast<float*>(
        static_cast<unsigned char*>(g_raShaderConstants) + 444);
}

// One-time notes for the guarded withheld-singleton arms, so the log shows each divergence once
// instead of sixty times a second.
void noteOnce(unsigned int bit, const char* what)
{
    static unsigned int noted = 0;
    if (noted & bit)
        return;
    noted |= bit;
    ::renddx9abi::logf("renderFrame: %s - the shipped call is skipped (withheld class); this note "
                       "prints once", what);
}

} // anonymous namespace

// sub_100F7B00, 0x100F7B00, 69 B - The camera's depth below the water plane:
// WaterHeight (constants block +0x1BC) minus the view matrix's Y, floored at zero (`fld 0.0;
// fcomp st(1); jnp` - the zero wins whenever the camera is on or above the plane, and on a NaN).
// The block is read unguarded in the shipped body; the guard is isCameraUnderWater's, and answers
// 0 - "above the surface" - the menu truth. [CHOSEN guard]
float underwaterDepth()
{
    if (!g_raShaderConstants)
        return 0.0f;
    const float waterHeight = *reinterpret_cast<float*>(
        static_cast<unsigned char*>(g_raShaderConstants) + 444);
    float* m = viewGetMatrixPtr(rendererGetRenderView());
    const float depth = waterHeight - m[13];
    return (0.0f < depth) ? depth : 0.0f;
}

// sub_100F7AD0, published: PostProductionFrameWork::updateFog calls it by address in the shipped
// body (0x1004FED9) and lives in another TU here.
bool cameraIsUnderWater()
{
    return isCameraUnderWater();
}


// +0x24, Renderer::update(time), the per-frame tick BF2 calls before renderFrame.
// The shipped body updates the world-side singletons unconditionally because a shipped ModuleGeom
// always creates them; here every withheld one is guarded with a one-time note. WindManager is
// resolved by name each call - BF2's own singleton, absent until a level registers it.

void Renderer::update(double time)
{
    unsigned char* self = reinterpret_cast<unsigned char*>(this);
    unsigned int*  dw   = reinterpret_cast<unsigned int*>(this);

    ++dw[65];
    *reinterpret_cast<double*>(self + 1512) = time;
    const float timeF = static_cast<float>(time);

    {
        void* wind = classManager ? classManager->getSingleton(renddx9abi::HostString("WindManager")) : 0;
        if (wind)
            hostCalls::windUpdate(wind, timeF);                                        // +0x10
    }

    if (geom::g_SkyDome)
    {
        typedef void (__fastcall* Fn)(void*, void*, float);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(geom::g_SkyDome))[4])(
            geom::g_SkyDome, 0, timeF);
    }
    else
        noteOnce(1u, "SkyDome::update");

    if (self[1702] && geom::g_UndergrowthSystem)
        static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->update(timeF);   // +0x24

    if (geom::g_LightManager)
    {
        geom::g_LightManager->update(time);
    }
    else
        noteOnce(2u, "LightManager::update");

    if (geom::g_ParticleSystemManager)
    {
        geom::asParticleSystemManager(geom::g_ParticleSystemManager)->update(timeF);
    }
    else
        noteOnce(4u, "ParticleSystemManager::update");

    if (geom::g_DecalManager)
    {
        typedef void (__fastcall* Fn)(void*, void*, float);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(geom::g_DecalManager))[12])(
            geom::g_DecalManager, 0, timeF);
    }
    else
        noteOnce(8u, "DecalManager::update");

    hudManagerFramePrep(timeF);

    // 1002BF2C ->environment texture selection, not a debug-camera hook.
    if (Rend::g_EnvMapManager)
        static_cast<Rend::EnvMapManager*>(Rend::g_EnvMapManager)->update(timeF);
}


// sub_10028EE0 - the HudManager per-frame handoff: hud->vslot+0x20(the byte at +1713), then
// hud->vslot+0x28(time, playerManager->vslot+0x30(), lookX, lookY, lookZ) with the look row taken
// from the render view's matrix. Both singletons are BF2's own live objects.

void Renderer::hudManagerFramePrep(float timeF)
{
    unsigned char* self = reinterpret_cast<unsigned char*>(this);
    void* hud = m_hudManager;
    if (!hud)
        return;

    hostCalls::hudSetPaint(hud, self[1713]);                                           // +0x20

    void* player = hostCalls::localHumanPlayer();                                       // +0x30

    float* m = viewGetMatrixPtr(getRenderView());
    hostCalls::hudUpdate(hud, timeF, player, m[8], m[9], m[10]);                        // +0x28
}


// Updates road constants and flushes the road queue with depth bias.
// Requires a road renderer and terrain. Adds frame time again after the main accumulation.

void Renderer::roadQueuePass()
{
    if (!geom::g_RoadCompiledRenderer || !geom::g_Terrain)
        return;

    IRenderView* const view = rendererGetRenderViewDup();                                   // +0x50
    const double* const dbl = reinterpret_cast<const double*>(this);
    unsigned char* const constants = static_cast<unsigned char*>(g_raShaderConstants);
    *reinterpret_cast<float*>(constants + 0x94) += static_cast<float>(dbl[189]);

    {
        const float* const m = view->getViewMatrixPtr()->m;                                  // +0x64
        float* const matrix = reinterpret_cast<float*>(constants + 0x1C);
        for (int i = 0; i < 16; ++i)
            matrix[i] = m[i];
        float* const camera = reinterpret_cast<float*>(constants + 0x60);
        camera[0] = matrix[12];
        camera[1] = matrix[13];
        camera[2] = matrix[14];
    }

    Vec3 sun;
    if (geom::g_Terrain)
        static_cast<geom::Terrain*>(geom::g_Terrain)->getSunColorRaw(&sun);                 // +0x78
    else
    {
        sun.x = 1.0f;
        sun.y = 1.0f;
        sun.z = 1.0f;
    }
    float* const sunColor = reinterpret_cast<float*>(constants + 0x11C);
    sunColor[0] = sun.x;
    sunColor[1] = sun.y;
    sunColor[2] = sun.z;

    const float* const fade = geom::roadCompiledRendererFadeOut();
    float* const roadFadeOut = reinterpret_cast<float*>(constants + 0x12C);
    roadFadeOut[0] = fade[0];
    roadFadeOut[1] = fade[1];

    postProductionUpdateFog(underwaterDepth());

    stateManagerPush();
    stateManagerSetRenderState(195u, geom::roadCompiledRendererDepthBiasBits());            // D3DRS_DEPTHBIAS
    stateManagerSetRenderState(175u, geom::roadCompiledRendererSlopeScaleDepthBiasBits());  // slopescaledepthbias
    renderQueueFlush(0x24, 1);                                                               // RSMTRoad
    stateManagerPop();
}


// sub_1002DF20 - the geometry-streaming scan, and it runs at the menu: the budget at float 465 is
// the constructor's 25.0. Build a forward-expanded frustum from the render view's, hand it to the
// host ObjectManager's frustum query, and touch the mesh templates of whatever comes back. With
// an empty world the result vector comes back empty and the touch loop never runs; the touch arms
// belong to the unbuilt mesh front and are a named frontier.

void Renderer::geometryStreamTick()
{
    unsigned char* self = reinterpret_cast<unsigned char*>(this);
    unsigned int*  dw   = reinterpret_cast<unsigned int*>(this);
    float*         fl   = reinterpret_cast<float*>(this);

    if (fl[465] <= 0.00000011920929f)
        return;

    dw[467] = 0;
    dw[468] = 0;
    dw[471] = dw[470];                       // resize the result vector to zero (first == last)

    void* view = getRenderViewDup();
    unsigned char* vf = static_cast<unsigned char*>(viewGetFrustumPtr(view));
    float* vfMatrix = reinterpret_cast<float*>(vf + 0x174);

    float m[16];
    for (int i = 0; i < 16; ++i)
        m[i] = vfMatrix[i];

    // slide the frustum origin half a budget along the look row, and stretch near/far by the
    // budget - the shipped -budget * row * 0.5 fold.
    const float budget = fl[465];
    m[12] += m[8]  * -budget * 0.5f;
    m[13] += m[9]  * -budget * 0.5f;
    m[14] += m[10] * -budget * 0.5f;

    const float scanFar  = budget * 1.5f + *reinterpret_cast<float*>(vf + 0x1E0);
    const float scanNear = budget * 0.5f;

    unsigned char scan[0x1E8];
    ::memset(scan, 0, sizeof(scan));
    Rend::renderViewBuildFrustum(scan, *reinterpret_cast<float*>(vf + 0x1CC),
                                 *reinterpret_cast<float*>(vf + 0x1D0), scanNear, scanFar, m);

    // host ObjectManager vslot +0x5C: query(frustum, &resultVector, 0, 0, 0, 0). The vector
    // fields live at dwords 469..471 of this object; the host fills them through the engine heap.
    // Typed in the native flavour with the vector correctly overlaid - see hostCalls (RW-266).
    hostCalls::objectManagerQueryFrustum(scan, dw + 469);                              // +0x5C

    unsigned int first = dw[470];
    unsigned int last  = dw[471];
    if (first != last)
    {
        RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::geometryStreamTick (the host query "
                        "returned objects - the template-touch walk belongs to the unbuilt mesh "
                        "front)", this, 0x28);
    }
    (void)self;
}


// +0x28, Renderer::renderFrame. The full shipped sequence with every withheld
// singleton's dereference guarded (one-time notes) and every flag-gated arm carried. The
// environment-map loop collapses to one iteration because the EnvMapManager at base+0x3F4 is
// withheld - exactly the paths a shipped build takes with those pieces absent.

void rendererDrawFpsOverlay(void* renderer);

bool Renderer::renderFrame(double time, float deltaTime)
{
    unsigned char* self = reinterpret_cast<unsigned char*>(this);
    unsigned int*  dw   = reinterpret_cast<unsigned int*>(this);
    float*         fl   = reinterpret_cast<float*>(this);
    double*        dbl  = reinterpret_cast<double*>(this);
    const float    timeF = static_cast<float>(time);

    // BF2Render.cpp:991 - the state-manager sanity check.
    if (!stateManagerIsFullyRestored())
    {
        ::renddx9abi::logf("BF2Render.cpp:991 State manager: render states not completely "
                           "restored at beginning of rendering of the frame");
        stateManagerForceRestore();
    }

    if (geom::g_StaticMeshRenderer)
        *reinterpret_cast<unsigned int*>(
            reinterpret_cast<unsigned char*>(geom::g_StaticMeshRenderer) + 92) = 0;

    // host System vslot +0x28: frame begin - storeAndTurnOffFPUExceptions (typed, RW-266).
    hostCalls::systemFrameBegin();

    // The view setup: near planes, the second view's far, its matrix from the first view, the
    // camera into the (withheld) constant block, the frame timer.
    viewSetNearPlane(getRenderView(),    fl[356]);
    viewSetNearPlane(getRenderViewDup(), fl[356]);
    viewSetNearPlane(getRenderView2(),   fl[357]);
    viewSetFarPlane(getRenderView2(),    fl[358] - fl[357]);
    viewSetViewMatrix(getRenderView2(),  viewGetMatrixPtr(getRenderViewDup()));
    if (g_raShaderConstants)
    {
        // sub_10037EE0: view matrix into block+28, camera position into block+96 (WorldSpaceCamPos)
        float* dst = reinterpret_cast<float*>(
            static_cast<unsigned char*>(g_raShaderConstants) + 28);
        float* src = viewGetMatrixPtr(getRenderViewDup());
        for (int i = 0; i < 16; ++i)
            dst[i] = src[i];
        float* cam = reinterpret_cast<float*>(
            static_cast<unsigned char*>(g_raShaderConstants) + 96);
        cam[0] = src[12]; cam[1] = src[13]; cam[2] = src[14];
    }
    frameTimerTick(reinterpret_cast<void*>(dw[375]));

    const double presentGate = time;

    if (isMultisampleActive())
        stateManagerSetRenderStatePersistent(161, 1);

    // base+0x3F4 - the EnvMapManager, withheld: one iteration, no capture arms.
    const unsigned int envMgr =
        *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(g_rendererBase) + 0x3F4);
    const int iterations = 1;
    (void)envMgr;

    for (int i = 0; i < iterations; ++i)
    {
        dbl[189] = time;

        if (isExpansionPack())
        {
            // sub_1002B8E0 + sub_100F2740 - the xpack light arm; the byte at +0x764 is never set
            // on the recovered paths.
            RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::renderFrame (the xpack light arm - "
                            "sub_1002B8E0 is unread)", this, 0x28);
        }

        if (g_renderTargetSystem)
        {
            RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::renderFrame (sub_1004DF30 - the "
                            "render-target select; its system is unbuilt)", this, 0x28);
        }

        {
            void* wind = classManager ? classManager->getSingleton(renddx9abi::HostString("WindManager"))
                                      : 0;
            if (wind)
                hostCalls::windUpdate(wind, timeF);                                    // +0x10
        }

        // The fog block: save the host ObjectManager's fog triple, swap in the renderer's
        // override, run the ObjectDrawer, restore. The drawer is withheld; the host fog churn is
        // the shipped behaviour and runs.
        if (self[1693])
        {
            void* om = objectManager;
            void*** omv = reinterpret_cast<void***>(om);
            typedef float (__fastcall* GetF)(void*, void*);
            typedef void  (__fastcall* SetF)(void*, void*, float);
            const float fogA = reinterpret_cast<GetF>((*omv)[49])(om, 0);   // +196
            const float fogB = reinterpret_cast<GetF>((*omv)[51])(om, 0);   // +204
            const float fogC = reinterpret_cast<GetF>((*omv)[47])(om, 0);   // +188
            reinterpret_cast<SetF>((*omv)[46])(om, 0, fl[434]);             // +184
            reinterpret_cast<SetF>((*omv)[48])(om, 0, fl[432]);             // +192
            reinterpret_cast<SetF>((*omv)[50])(om, 0, fl[433]);             // +200
            if (m_objectDrawer)
            {
                void*** dv = reinterpret_cast<void***>(m_objectDrawer);
                typedef void (__fastcall* SetViewFn)(void*, void*, void*);
                typedef void (__fastcall* TimeFn)(void*, void*, float);
                reinterpret_cast<SetViewFn>((*dv)[10])(m_objectDrawer, 0, getRenderView());
                reinterpret_cast<TimeFn>((*dv)[5])(m_objectDrawer, 0, timeF);
                reinterpret_cast<TimeFn>((*dv)[4])(m_objectDrawer, 0, timeF);
            }
            else
                noteOnce(16u, "ObjectDrawer (fog block)");
            reinterpret_cast<SetF>((*omv)[48])(om, 0, fogA);
            reinterpret_cast<SetF>((*omv)[50])(om, 0, fogB);
            reinterpret_cast<SetF>((*omv)[46])(om, 0, fogC);
        }

        if (self[1714])
        {
            if (geom::g_SkyDome)
            {
                typedef void (__fastcall* Fn)(void*, void*, float);
                reinterpret_cast<Fn>((*reinterpret_cast<void***>(geom::g_SkyDome))[4])(
                    geom::g_SkyDome, 0, timeF);
            }
            else
                noteOnce(32u, "SkyDome (frame update)");
        }

        if (m_weatherManager && self[1695])
        {
            typedef void (__fastcall* Fn)(void*, void*, double);
            reinterpret_cast<Fn>((*static_cast<void***>(m_weatherManager))[6])(
                m_weatherManager, 0, time);
        }
        if (m_rainManager && self[1695])
        {
            typedef void (__fastcall* Fn)(void*, void*, double);
            reinterpret_cast<Fn>((*static_cast<void***>(m_rainManager))[6])(
                m_rainManager, 0, time);
        }
        if (self[1702] && geom::g_UndergrowthSystem)
            static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->update(timeF);   // +0x24
        if (geom::g_LightManager)
        {
            geom::g_LightManager->update(time);
        }
        if (self[1695] && geom::g_ParticleSystemManager)
        {
            geom::asParticleSystemManager(geom::g_ParticleSystemManager)->update(timeF);
        }
        if (geom::g_DecalManager)
        {
            typedef void (__fastcall* Fn)(void*, void*, float);
            reinterpret_cast<Fn>((*reinterpret_cast<void***>(geom::g_DecalManager))[12])(
                geom::g_DecalManager, 0, timeF);
        }

        // 0x1002FDED `mov eax, [ebp+arg_8]; push eax` - the second argument is forwarded verbatim
        // and lands on BF2HudManager::update as its float dt. It IS a float: BF2.exe pushes its
        // BF2Engine +0x38, the clamped mainLoop delta (see RendMisc/IRenderer.h). The old carrier
        // here bit-cast an int "frame index", which reached the HUD as a denormal ~0 and froze
        // every HUD easing - most visibly the spawn map, which never grew to full size.
        hudManagerFramePrep(deltaTime);

        if (self[1717] && g_PostProductionFrameWorkOwner
            && (!self[1441] || postProductionActive()))
        {
            postProductionPrepare();
        }

        if (beginScene(0, 0))
        {
            // PE 0x1002FE52 passes XpLightManager.cpp's coordinator at 0x10259B20, not the
            // LightManager singleton.  The old guard/frontier conflated those two objects.
            geom::prepareLightFrame(time);
            // sub_1004DF10 - the render-target system's frame clear; system withheld.

            if (geom::g_LightManager)
            {
                geom::g_LightManager->renderShadowMaps();
            }
            else
                noteOnce(64u, "LightManager (shadow-map pass + light constants)");

            // sub_100057E0(7, 0, 1.0f, 0, 0, 0) - the frame clear: RT flush + Clear ALL, black.
            flushRenderTargetsSeam();
            {
                typedef IDirect3DDevice9* DevPtr;
                DevPtr dev = *reinterpret_cast<DevPtr*>(
                    static_cast<unsigned char*>(g_rendererBase) + 0x0C);
                dev->Clear(0, 0, 7, 0, 1.0f, 0);
            }

            if (self[1695] && geom::g_ParticleSystemManager)
            {
                geom::particleSystemManagerBeginFrame(
                    geom::asParticleSystemManager(geom::g_ParticleSystemManager));
            }

            // sub_100383D0 - GlobalTime accumulate on the withheld constants block.
            if (g_raShaderConstants)
                *reinterpret_cast<float*>(
                    static_cast<unsigned char*>(g_raShaderConstants) + 148) +=
                    static_cast<float>(dbl[189]);

            // The constants block is still unpublished in this
            // reconstruction, so retain its existing guard while using the exact LightManager
            // getters and destinations once it is present.
            if (g_raShaderConstants && geom::g_LightManager)
            {
                geom::LightManager* const lights = geom::g_LightManager;
                unsigned char* const constants =
                    static_cast<unsigned char*>(g_raShaderConstants);

                const Vec3& staticSky = lights->getStaticSkyColor();
                *reinterpret_cast<Vec3*>(constants + 0x1F8) = staticSky;

                const Vec3& treeSky = lights->getTreeSkyColor();
                Vec3& treeSkyConstant = *reinterpret_cast<Vec3*>(constants + 0x154);
                treeSkyConstant.x = treeSky.x * 0.5f;
                treeSkyConstant.y = treeSky.y * 0.5f;
                treeSkyConstant.z = treeSky.z * 0.5f;

                *reinterpret_cast<Vec3*>(constants + 0x214) =
                    lights->getStaticSpecularColor();
                *reinterpret_cast<Vec3*>(constants + 0x230) =
                    lights->getSinglePointColor();
                *reinterpret_cast<Vec3*>(constants + 0x24C) =
                    lights->getDynamicPointColor();
                *reinterpret_cast<unsigned int*>(constants + 0x25C) =
                    isExpansionPack() ? 1u : 0u;
                *reinterpret_cast<float*>(constants + 0x264) =
                    lights->getOccludedSkyNightIntensity();
            }

            // the road fade pair into the constants block (sub_10038810), and the underwater fog
            // pair (sub_100F7B00 + sub_1004FDA0) - all constants-block writers, all guarded.
            if (g_raShaderConstants && geom::g_RoadCompiledRenderer)
            {
                unsigned char* road = reinterpret_cast<unsigned char*>(
                    geom::g_RoadCompiledRenderer);
                float* dst = reinterpret_cast<float*>(
                    static_cast<unsigned char*>(g_raShaderConstants) + 300);
                dst[0] = *reinterpret_cast<float*>(road + 176);
                dst[1] = *reinterpret_cast<float*>(road + 180);
            }

            // the underwater fog pair, the camera's
            // depth below the water plane into PostProductionFrameWork::updateFog, which is what
            // writes the Ra FogRange/FogColor constants and D3DRS_FOGCOLOR every frame. The
            // shipped call is unguarded (ds:0x1023F7C0 straight into ecx); the guard follows this
            // function's convention for its singletons. [CHOSEN guard]
            if (g_raShaderConstants && g_PostProductionFrameWorkOwner)
                postProductionUpdateFog(underwaterDepth());

            if (self[1693] && m_objectDrawer)
            {
                typedef void (__fastcall* TimeFn)(void*, void*, float);
                reinterpret_cast<TimeFn>((*reinterpret_cast<void***>(m_objectDrawer))[6])(
                    m_objectDrawer, 0, timeF);
            }

            renderQueueFlush(0x30, 1);                       // RSMTZOnly

            // Terrain is available after the level's terrain.create command.
            IBase* terrain = geom::g_Terrain;
            if (self[1701] && terrain)
            {
                static_cast<geom::Terrain*>(terrain)->renderWorldPassBegin();   // +0xB8
                terrain = geom::g_Terrain;
            }

            // The world pass publishes the renderer-owned accumulation
            // target and its lazily initialized projection vectors, then the current base Hemi map
            // and its four constants. The shipped guard is only byte +0x69D; the old source also
            // tested dword 477 and thereby hid a real live path. Both texture records retain their
            // retail ownership semantics inside RenderQueues.cpp.
            if (self[1693])
            {
                EngineTexture* const accumulationMap =
                    static_cast<Renderer*>(g_Renderer)->getHemiScratchTexture();
                raShaderConstantsSetAccumulationMap(accumulationMap);

                geom::HemiMap* const hemiMap = geom::g_HemiMapManager->getBaseHemiMap();
                if (hemiMap)
                {
                    raShaderConstantsSetHemiMapTexture(hemiMap->texture);
                    const geom::HemiMapInfo& info =
                        geom::g_HemiMapManager->getHemiMapInfo(hemiMap);
                    raShaderConstantsSetHemiMapInfo(&info.positionX);
                }

                if (shouldToggleTransparencyAA())
                    stateManagerSetTransparencyAA(true);

                // fetch the view independently, then call +0x34.
                // Its genuine empty-path return is recovered in OverGrowth.cpp.
                if (self[1703] && g_OverGrowth)
                {
                    IRenderView* const overGrowthView = rendererGetRenderView();
                    static_cast<geom::OverGrowth*>(g_OverGrowth)->render(overGrowthView, timeF);
                }

                if (shouldToggleTransparencyAA())
                    stateManagerSetTransparencyAA(false);

                renderQueuesFlushWorldPass();

                if (shouldToggleTransparencyAA())
                    stateManagerSetTransparencyAA(true);

                // Draw grass inside the transparency-AA bracket.
                if (self[1702] && geom::g_UndergrowthSystem)
                    static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->render(timeF);

                if (shouldToggleTransparencyAA())
                    stateManagerSetTransparencyAA(false);

                terrain = geom::g_Terrain;
            }

            // PE 0x100303A7..0x100303BC mirrors the pre-detail Terrain arm at +0xBC.
            if (self[1701] && terrain)
                static_cast<geom::Terrain*>(terrain)->renderWorldPassEnd();     // +0xBC

            if (self[1700])
                roadQueuePass();
            if (self[1700])
            {
                if (geom::g_RoadCompiledRenderer)
                {
                    unsigned char* roadListener = reinterpret_cast<unsigned char*>(
                        geom::g_RoadCompiledRenderer) + 36;
                    typedef void (__fastcall* Fn)(void*, void*, int);
                    reinterpret_cast<Fn>((*reinterpret_cast<void***>(roadListener))[3])(
                        roadListener, 0, 0);
                }
                if (geom::g_RoadEditableRenderer)
                {
                    // IRoadEditableRenderer +0x18 drawAll, the slot is
                    // the same in every flavour - IBase's four, then add / remove / drawAll.
                    static_cast<geom::IRoadEditableRenderer*>(geom::g_RoadEditableRenderer)->drawAll();
                }
            }

            if (self[1714])
            {
                if (geom::g_SkyDome)
                {
                    unsigned char* skySub = reinterpret_cast<unsigned char*>(geom::g_SkyDome) + 4;
                    typedef void (__fastcall* Fn)(void*, void*, int);
                    reinterpret_cast<Fn>((*reinterpret_cast<void***>(skySub))[3])(skySub, 0, 0);
                    if (self[1715])
                    {
                        typedef void (__fastcall* SkyFn)(void*, void*, int);
                        reinterpret_cast<SkyFn>(
                            (*reinterpret_cast<void***>(geom::g_SkyDome))[42])(
                            geom::g_SkyDome, 0, 0);
                    }
                }
                else
                    noteOnce(128u, "SkyDome (draw pass)");
            }

            renderQueueFlush(0x3C, 1);                       // RSMTFogPass

            if (self[1693] && isCameraUnderWater())
            {
                renderQueueFlush(0x08, -1);                  // RSMTTransparency
                renderQueueFlush(0x48, -1);                  // RSMTAux0Transparency
                renderQueueFlush(0x18, -1);                  // RSMTDeltaTransparency
                renderQueueFlush(0x58, -1);                  // RSMTAux1Transparency
            }
            if (self[1696] && geom::g_DecalManager)
            {
                unsigned char* decalSub = reinterpret_cast<unsigned char*>(
                    geom::g_DecalManager) + 4;
                typedef void (__fastcall* Fn)(void*, void*, int);
                reinterpret_cast<Fn>((*reinterpret_cast<void***>(decalSub))[3])(decalSub, 0, 0);
            }
            if (self[1693] && !isCameraUnderWater())
                renderQueueFlush(0x2C, 1);                   // RSMTAdditive (above water)
            if (self[1695] && geom::g_ParticleSystemManager)
            {
                geom::asParticleSystemManager(geom::g_ParticleSystemManager)->onPathDraw(false, true);
            }
            renderQueueFlush(0x28, -1);                      // RSMTWater
            if (self[1693])
            {
                if (!isCameraUnderWater())
                {
                    renderQueueFlush(0x08, -1);
                    renderQueueFlush(0x48, -1);
                    renderQueueFlush(0x18, -1);
                    renderQueueFlush(0x58, -1);
                }
                if (!isCameraUnderWater())
                    renderQueueFlush(0x2C, 1);               // RSMTAdditive
            }
            if (self[1695] && geom::g_ParticleSystemManager)
            {
                geom::asParticleSystemManager(geom::g_ParticleSystemManager)->onPathDraw(false, false);
            }
            renderQueueFlush(0x40, 1);                       // RSMTFogTransparentPass
            if (m_weatherManager && self[1695])
            {
                typedef void (__fastcall* Fn)(void*, void*);
                reinterpret_cast<Fn>((*static_cast<void***>(m_weatherManager))[7])(
                    m_weatherManager, 0);
            }
            if (m_rainManager && self[1695])
            {
                typedef void (__fastcall* Fn)(void*, void*);
                reinterpret_cast<Fn>((*static_cast<void***>(m_rainManager))[7])(
                    m_rainManager, 0);
            }
            if (self[1714] && self[1715] && geom::g_SkyDome)
            {
                typedef void (__fastcall* SkyFn)(void*, void*, int);
                reinterpret_cast<SkyFn>((*reinterpret_cast<void***>(geom::g_SkyDome))[42])(
                    geom::g_SkyDome, 0, 1);
            }
            if (self[1717] && g_PostProductionFrameWorkOwner
                && (!self[1441] || postProductionActive()))
            {
                postProductionPass();
            }

            if (self[1440])
            {
                // sub_100057E0(2, 0, 1.0f, 0, 0, 0) - the z-only clear before the first-person
                // passes.
                flushRenderTargetsSeam();
                IDirect3DDevice9* dev = *reinterpret_cast<IDirect3DDevice9**>(
                    static_cast<unsigned char*>(g_rendererBase) + 0x0C);
                dev->Clear(0, 0, 2, 0, 1.0f, 0);
            }

            renderQueueFlush(0x0C, self[1440]);              // RSMT1Person
            renderQueueFlush(0x4C, -1);                      // RSMTAux0Person
            renderQueueFlush(0x1C, self[1440]);              // RSMTDelta1Person
            renderQueueFlush(0x5C, -1);                      // RSMTAux1Person
            renderQueueFlush(0x60, -1);                      // RSMTAux1PersonTransparency
            if (self[1695] && self[1440] && geom::g_ParticleSystemManager)
            {
                geom::asParticleSystemManager(geom::g_ParticleSystemManager)->onPathDraw(true, false);
            }
            renderQueueFlush(0x10, self[1440]);              // RSMT1PersonTransparency
            renderQueueFlush(0x50, -1);                      // RSMTAux0PersonTransparency
            renderQueueFlush(0x20, self[1440]);              // RSMTDelta1PersonTransparency
            renderQueueFlush(0x60, -1);                      // RSMTAux1PersonTransparency

            if (self[1714] && self[1715] && geom::g_SkyDome)
            {
                typedef void (__fastcall* SkyFn)(void*, void*, int);
                reinterpret_cast<SkyFn>((*reinterpret_cast<void***>(geom::g_SkyDome))[42])(
                    geom::g_SkyDome, 0, 2);
            }

            // The multisample depth detach/reattach pair skips (not multisampled), and the
            // depth-stencil round trip with it.

            if (self[1717] && g_PostProductionFrameWorkOwner
                && (!self[1441] || postProductionActive()))
                postProductionFinish();

            if (self[1704])
            {
                void* nametags = classManager
                    ? classManager->getSingleton(renddx9abi::HostString("NametagManager")) : 0;
                if (nametags)
                {
                    typedef void (__fastcall* Fn)(void*, void*);
                    reinterpret_cast<Fn>((*static_cast<void***>(nametags))[6])(nametags, 0);
                }
            }
            if (self[1713] && m_hudManager)
                hostCalls::hudPaint(m_hudManager);                                     // +0x1C

            // The three debug graphs: render-time (ms), FPS, draw calls. The draw-call sample
            // source (sub_10046E50 on the profiler counters) is the stat scaffolding this rebuild
            // does not reproduce - it samples 0 here. All three stay hidden (byte 1712 = 0).
            {
                typedef void (__fastcall* SampleFn)(void*, void*, float);
                typedef void (__fastcall* FlagFn)(void*, void*, unsigned char);
                void* gRender = reinterpret_cast<void*>(dw[550]);
                void* gFps    = reinterpret_cast<void*>(dw[549]);
                void* gDraw   = reinterpret_cast<void*>(dw[551]);
                if (gRender)
                    reinterpret_cast<SampleFn>((*static_cast<void***>(gRender))[16])(
                        gRender, 0, static_cast<float>(m_frameWallTime * 1000.0));
                if (gFps)
                    reinterpret_cast<SampleFn>((*static_cast<void***>(gFps))[16])(
                        gFps, 0, static_cast<float>(1.0 / dbl[189]));
                if (gDraw)
                    reinterpret_cast<SampleFn>((*static_cast<void***>(gDraw))[16])(gDraw, 0, 0.0f);

                // the ECRenderer / ECCore frame events - the posts BF2's own menu system draws
                // from. Delay-zero, so BF2's handlers run inside these calls.
                ::dice::hfe::eventManager->postEvent(
                    ECPhysics, 0, static_cast<void*>(0), 0.0f);
                ::dice::hfe::eventManager->postEvent(
                    ECCore, 5, static_cast<void*>(0), 0.0f);
                ::dice::hfe::eventManager->postEvent(
                    static_cast<EventCategory>(4), 6000, static_cast<void*>(0), 0.0f);

                const unsigned char statsWere = statsSwapEnabled(0);
                // dword_10247C3C - the AI-overlay hook; nothing recovered publishes it, so the
                // "AI" singleton arm and the hook call skip.
                statsSwapEnabled(statsWere);

                if (dw[437])
                {
                    RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::renderFrame (sub_1002E440 - "
                                    "the dword-437 arm)", this, 0x28);
                }

                // The FPS average: accumulate frame time and publish frames/seconds once the
                // total is ordered and strictly greater than the binary64 0.33 at 0x101E09B0.
                // Equal, less and unordered all skip publication (`fcom`; test C3|C0).
                dbl[193] += dbl[189];
                ++dw[388];
                if (dbl[193] > 0.33)
                {
                    m_frameRate = static_cast<double>(dw[388]) / dbl[193];
                    dw[388] = 0;
                    dbl[193] = 0.0;
                }
                if (m_frameRate == 0.0)
                    m_frameRate = 1.0 / dbl[189];
                for (int bucket = 9; bucket >= 0; --bucket)
                {
                    if (static_cast<double>(bucket) * fl[520] < m_frameRate)
                    {
                        ++dw[bucket + 521];
                        break;
                    }
                }

                // Separate debug flags gate frame-time and culling text overlays.
                if (dw[427])                                   // +0x6AC
                    rendererDrawFpsOverlay(this);
                // Native object vectors shift the fields following them; use the flag member.
                if (g_ObjectDrawer
                    && reinterpret_cast<const geom::ObjectDrawer*>(g_ObjectDrawer)->isCullingDebugEnabled())
                {
                    RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::renderFrame (sub_10029BB0 - the "
                                    "\"Culled:\" debug overlay, gated on the ObjectDrawer's debug flag, "
                                    "which its constructor clears)", this, 0x28);
                }

                if (gRender)
                    reinterpret_cast<FlagFn>((*static_cast<void***>(gRender))[10])(
                        gRender, 0, self[1712]);
                if (gFps)
                    reinterpret_cast<FlagFn>((*static_cast<void***>(gFps))[10])(
                        gFps, 0, self[1712]);
                if (gDraw)
                    reinterpret_cast<FlagFn>((*static_cast<void***>(gDraw))[10])(
                        gDraw, 0, self[1712]);
            }

            // Retail 0x10030AAF..0x10030ABA, just before the console gate: `cmp byte [esi+6AAh], 0;
            // je; mov ecx, esi; call 1002EAE0` - a 2,397-byte debug overlay, also run by slot_0x30
            // (0x1002F781). The only instruction that writes +0x6AA is the constructor's zero store
            // (0x1002D6BB, `self[1706] = 0` above), so the arm is off unless something sets the
            // byte. Its body is unread: named stop.
            if (self[1706])
            {
                RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::renderFrame (sub_1002EAE0 - "
                                "the byte-0x6AA debug overlay)", this, 0x28);
            }

            // Query console activation through its typed interface before drawing the overlay.
            {
                io::OldConsole* const con = reinterpret_cast<io::OldConsole*>(
                    ::dice::hfe::mainConsole);
                const bool consoleOpen = con->getConsoleActive();
                if (consoleOpen && self[1716])
                    slot_0x58();
            }

            if (self[1695] && geom::g_ParticleSystemManager)
            {
                geom::particleSystemManagerEndFrame(
                    geom::asParticleSystemManager(geom::g_ParticleSystemManager));
            }

            if (g_renderEntryPool)
                *static_cast<unsigned int*>(g_renderEntryPool) = 0;

            endScene();
        }

        if (presentGate != 0.0)
            present(2);
    }

    // The GPU-throttle fence ring: issue this frame's fence, block on the one from
    // ring-size frames ago.
    {
        const unsigned int count = dw[366];
        if (count)
        {
            queryIssueEnd(reinterpret_cast<void*>(dw[dw[367] % count + 361]));
            const unsigned int cursor = ++dw[367];
            queryWaitDone(reinterpret_cast<void*>(dw[(cursor - count) % count + 361]));
        }
    }

    geometryStreamTick();

    // host System vslot +0x2C: frame end; then the frame's wall time from its clock (+0x18,
    // returns a double) minus the mark at double 276.
    // +0x2C restoreFPUExceptions, then +0x18 getWallClockTime (typed, RW-266).
    hostCalls::systemFrameEnd();
    m_frameWallTime = hostCalls::systemWallClockTime() - m_frameStartTime;
    return true;
}

int rendererGetDrawFps()
{
    return rendererField<int>(g_Renderer, 0x6AC);
}

void rendererSetDrawFps(int value)
{
    rendererField<int>(g_Renderer, 0x6AC) = value;
}

// Queue the frame-rate display; the text manager draws it before EndScene.
void rendererDrawFpsOverlay(void* renderer)
{
    const unsigned char previousStats = statsSwapEnabled(0);
    IBase* const font = rendererField<IBase*>(renderer, 0x5CC);
    const unsigned previousColour = geom::fontPortedGetColour(font);
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, rendererField<unsigned>(renderer, 0x1C));
    if (rendererField<int>(renderer, 0x6AC) == 1)
    {
        const double fps = rendererField<double>(renderer, 0x600);
        const std::string formatted = formatString("%.1f / %.1f", fps, 1000.0 / fps);
        geom::debugTextWriterQueueText(geom::g_DebugTextWriter, 6, 6, vc71::string(formatted.c_str()), 0);
    }
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, previousColour);
    statsSwapEnabled(previousStats);
}

// The seam the flush's parameter upload uses. Renderer::getRenderView is recovered, so consumers
// use that class method instead of repeating its vtable position.
IRenderView* rendererGetRenderView()
{
    return static_cast<IRenderView*>(static_cast<Renderer*>(g_rendererBase)->getRenderView());
}

IRenderView* rendererGetRenderViewDup()
{
    return static_cast<IRenderView*>(static_cast<Renderer*>(g_rendererBase)->getRenderViewDup());
}

unsigned int rendererFrameOrdinal()
{
    return static_cast<Renderer*>(g_rendererBase)->getFrameOrdinal();
}

// Shared typed seam for renderer-side systems which use Renderer +0xAC.  The shipped callers do
// not null-guard the renderer singleton; preserving that precondition here matters because a false
// answer before renderer construction would otherwise look like valid base-game state.
bool rendererIsExpansionPack()
{
    return static_cast<Renderer*>(g_rendererBase)->isExpansionPack();
}

// Shared typed seam for renderer-side systems which consume Renderer +0x74. XpLight's policy
// test calls VideoOptions::getQuality(4); keep the real Renderer method in its owner TU.
IVideoOptions* rendererGetVideoOptions()
{
    return static_cast<Renderer*>(g_rendererBase)->getVideoOptions();
}


// HemiMapManager's editor/offline bake surface.

// These functions are deliberately ordinary typed seams.  The Renderer layout and its private
// collaborators stay in this owner TU; HemiMapManager.cpp never indexes a foreign vtable or reaches
// a renderer byte offset.  Addresses/order are from PE 0x1012B800 and its helpers


namespace {

world::ObjectManager* hemiObjectManager()
{
    return static_cast<world::ObjectManager*>(static_cast<void*>(::dice::hfe::objectManager));
}

// ObjectDrawer primary slot +0x28, setRenderView (0x100F7C50) - through the drawer's hand-built table
// exactly as renderFrame's fog block dispatches it (`(*dv)[10]`): ObjectDrawer keeps a table, not C++
// virtuals (Geom/ObjectDrawer.h). Unguarded, as every shipped caller is.
void objectDrawerSetRenderView(void* drawer, IRenderView* view)
{
    typedef void (__fastcall* SetViewFn)(void*, void*, void*);
    reinterpret_cast<SetViewFn>((*static_cast<void***>(drawer))[0x28 / sizeof(void*)])(
        drawer, 0, view);
}

} // anonymous namespace

IRenderView* rendererGetHemiPrimaryView()
{
    return static_cast<IRenderView*>(static_cast<Renderer*>(g_rendererBase)->getRenderView());
}

IRenderView* rendererGetHemiSecondaryView()
{
    return static_cast<IRenderView*>(static_cast<Renderer*>(g_rendererBase)->getRenderViewDup());
}

void rendererInstallHemiView(IRenderView* view)
{
    // The object pass forwards the new view to ObjectDrawer first.
    unsigned char* const raw = static_cast<unsigned char*>(g_rendererBase);
    if (raw[1693])
        objectDrawerSetRenderView(*reinterpret_cast<void**>(raw + 1472), view);
    Renderer* const renderer = static_cast<Renderer*>(g_rendererBase);
    renderer->setRenderView(view);
    renderer->setRenderViewDup(view);
}

void rendererRestoreHemiViews(IRenderView* primary, IRenderView* secondary)
{
    // PE 0x1012B800 restores the two saved views through Renderer +0x44 and +0x4C in this order.
    // Unlike the install arm, it does not hand anything to the ObjectDrawer.
    Renderer* const renderer = static_cast<Renderer*>(g_rendererBase);
    renderer->setRenderView(primary);
    renderer->setRenderViewDup(secondary);
}

void rendererSetHemiTarget(EngineTexture* texture)
{
    unsigned char* const raw = static_cast<unsigned char*>(g_rendererBase);
    IDirect3DSurface9*& fallback = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x0F4);
    fallback = texture ? textureGetLevelSurface(texture, 0, 0x7FFFFFFFu) : 0;

    // sub_10005920(0, 0, 0, 0x7fffffff): with a null texture argument the renderer takes +0xF4,
    // AddRefs it around StateManager +0x60, then releases that temporary reference.
    IDirect3DSurface9* selected = fallback;
    if (!selected)
        selected = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x76C);
    if (selected)
        selected->AddRef();
    stateManagerSetRenderTarget(0, selected);
    if (selected)
        selected->Release();
}

bool rendererPostMultisampleActive() { return isMultisampleActive(); }

EngineTexture* rendererPostSceneTexture()
{
    return *reinterpret_cast<EngineTexture**>(reinterpret_cast<char*>(g_Renderer) + 1928);
}

EngineTexture* rendererPostHistoryTexture(unsigned index)
{
    return reinterpret_cast<EngineTexture**>(reinterpret_cast<char*>(g_Renderer) + 1912)[index];
}

double rendererPostTime()
{
    const char* timer = *reinterpret_cast<char**>(reinterpret_cast<char*>(g_Renderer) + 1500);
    return *reinterpret_cast<const double*>(timer + 32);
}

void rendererPostSelectBackBuffer()
{
    unsigned char* raw = static_cast<unsigned char*>(g_rendererBase);
    IDirect3DSurface9* surface = *reinterpret_cast<IDirect3DSurface9**>(raw + 0xF4);
    if (!surface)
        surface = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x76C);
    if (surface)
        surface->AddRef();
    stateManagerSetRenderTarget(0, surface);
    if (surface)
        surface->Release();
}

void rendererPostSetTarget(EngineTexture* texture)
{
    EngineTexture*& retained = *reinterpret_cast<EngineTexture**>(
        reinterpret_cast<char*>(g_Renderer) + 2004);
    if (retained == texture)
        return;
    EngineTexture* previous = retained;
    textureReleaseRef(previous);
    retained = texture;
    if (texture)
        textureAddRef(texture);
    if (texture)
    {
        IDirect3DSurface9* surface = textureGetLevelSurface(texture, 0, 0x7FFFFFFFu);
        stateManagerSetRenderTarget(0, surface);
        if (surface)
            surface->Release();
    }
    else
        rendererPostSelectBackBuffer();
}

void rendererPostResolveScene()
{
    IDirect3DDevice9* device = *reinterpret_cast<IDirect3DDevice9**>(
        static_cast<char*>(g_rendererBase) + 12);
    IDirect3DSurface9* destination = textureGetLevelSurface(rendererPostSceneTexture(), 0, 0x7FFFFFFFu);
    IDirect3DSurface9* source = *reinterpret_cast<IDirect3DSurface9**>(
        reinterpret_cast<char*>(g_Renderer) + 1900);
    device->StretchRect(source, 0, destination, 0, D3DTEXF_NONE);
    destination->Release();
}

EngineTexture* Renderer::getHemiScratchTexture() const
{
    return m_hemiScratchTexture;
}

void Renderer::setHemiScratchTexture(EngineTexture* texture)
{
    m_hemiScratchTexture = texture;
}

void Renderer::releaseHemiScratchTexture()
{
    textureReleaseRef(m_hemiScratchTexture);
}

EngineTexture* rendererGetHemiScratchTexture()
{
    return static_cast<Renderer*>(g_Renderer)->getHemiScratchTexture();
}

void rendererSetHemiScratchTexture(EngineTexture* texture)
{
    static_cast<Renderer*>(g_Renderer)->setHemiScratchTexture(texture);
}

int rendererGetHemiRenderMode()
{
    return *reinterpret_cast<int*>(static_cast<unsigned char*>(g_rendererBase) + 0x20);
}

void rendererSetHemiRenderMode(int mode)
{
    if (static_cast<unsigned int>(mode) <= 5u)
    {
        stateManagerSetRenderState(8u, (mode == 2 || mode == 3) ? 2u : 3u);
        *reinterpret_cast<int*>(static_cast<unsigned char*>(g_rendererBase) + 0x20) = mode;
    }
}

void rendererSetHemiRoadPass(bool enabled)
{
    static_cast<unsigned char*>(g_rendererBase)[1700] = enabled ? 1u : 0u;
}

void rendererRenderHemiFrame()
{
    static_cast<Renderer*>(g_rendererBase)->renderFrame(0.0, 0);
}

void rendererPresentHemiResult()
{
    static_cast<Renderer*>(g_rendererBase)->present(2u);
}

float rendererSwapMinCullDistance(float value)
{
    // Host ObjectManager PE +0xBC getter, then +0xB8 setter. Both signatures/order are attested in
    // Scene/Object/ObjectManager.h; use that recovered class directly.
    world::ObjectManager* const manager = hemiObjectManager();
    const float old = manager->getMinCullDistance();
    manager->setMinCullDistance(value);
    return old;
}


// Forces an LOD on live instances of a geometry class, then matching objects within 10,000 units.
// Clamps to count - 1 with signed arithmetic; an instance may be visited through both routes.

namespace {

// The class ids setLods compares - GeometryTemplate_*_reflectionTypeId, read from .data in the PE.
const unsigned int kStaticMeshTemplateClassId  = 0x870CA653u;   // ds:1021AF04
const unsigned int kSkinnedMeshTemplateClassId = 0x870CA652u;   // ds:1021AF00
const unsigned int kBundledMeshTemplateClassId = 0x870CA651u;   // ds:1021AEFC

// The geometry instance slots setLods drives, by offset through each instance's own retail table - the
// XpLightManager.cpp / OverGrowthLoad.cpp geometrySlot precedent: IGeometry is not a C++ interface in
// this tree. StaticMesh (kStaticMeshVtable), SkinnedMesh and BundledMesh install a body of the same
// arity at each - +0x38 getTemplate and +0x80 getLodCount take nothing, +0x78 setLod one int - which is
// what makes the raw callee-cleanup dispatch stack-safe for all three.
inline void* lodGeometrySlot(void* geometry, unsigned int offset)
{
    return (*static_cast<void***>(geometry))[offset / sizeof(void*)];
}

inline geom::IGeometryTemplate* lodGeometryTemplate(void* geometry)                    // +0x38
{
    typedef void* (__fastcall* GetTemplateFn)(void*, void*);
    return static_cast<geom::IGeometryTemplate*>(
        reinterpret_cast<GetTemplateFn>(lodGeometrySlot(geometry, 0x38))(geometry, 0));
}

// The clamp all four arms inline.
void forceGeometryLod(void* geometry, int lod)
{
    typedef int (__fastcall* GetLodCountFn)(void*, void*);
    typedef void (__fastcall* SetLodFn)(void*, void*, int);
    int selected =
        reinterpret_cast<GetLodCountFn>(lodGeometrySlot(geometry, 0x80))(geometry, 0) - 1;  // +0x80
    if (selected >= lod)
        selected = lod;
    reinterpret_cast<SetLodFn>(lodGeometrySlot(geometry, 0x78))(geometry, 0, selected);     // +0x78
}

// One template's live-instance set, in key order (the VC7.1 set iterator's increment: inlined for
// StaticMesh and SkinnedMesh, sub_1009F6E0 for BundledMesh). The set's key IS the instance pointer.
void forceInstanceSetLod(const vc71::set_layout& instances, int lod)
{
    for (vc71::set_node* node = instances.begin(); node != instances.end();
         node = vc71::set_layout::next(node))
        forceGeometryLod(reinterpret_cast<void*>(node->key), lod);
}

// ObjectManager +0x44, getObjectsFromGrid(centre, radius, out, predicator) - the query setLods makes
// (0x1002D3A5). Split as hostCalls::objectManagerQueryFrustum is: the recovered declaration in the
// native flavour, whose result is this toolchain's std::vector; the retail slot in the compatibility
// flavour, handed the VC7.1 vector object - its unwritten allocator word first - the retail host fills.
#if BF2_PLUGIN_ABI_NATIVE
void queryObjectsInRadius(const Vec3& centre, float radius, std::vector<world::IObject*>& out)
{
    hemiObjectManager()->getObjectsFromGrid(centre, radius, out, 0);
}
#else
void queryObjectsInRadius(const Vec3& centre, float radius, vc71::vector_layout& out)
{
    void* const manager = ::dice::hfe::objectManager;
    typedef void (__fastcall* QueryFn)(void*, void*, const Vec3*, float, void*, void*);
    reinterpret_cast<QueryFn>((*static_cast<void***>(manager))[0x44 / sizeof(void*)])(
        manager, 0, &centre, radius, &out, 0);
}
#endif

} // anonymous namespace

void rendererSetLods(int lod, unsigned int classId)
{
    vc71::map_layout* const templates =
        static_cast<geom::IGeometryTemplateManager*>(
            static_cast<void*>(geom::g_GeometryTemplateManager))->getTemplateRegistry();   // +0x60
    for (vc71::map_node* node = templates->begin(); node != templates->end();
         node = vc71::map_layout::next(node))
    {
        geom::IGeometryTemplate* const geometryTemplate =
            static_cast<geom::IGeometryTemplate*>(vc71::mapNodeValue(node));                 // +0x28
        if (geometryTemplate->getClassId() != classId)                                      // +0x10
            continue;
        if (classId == kStaticMeshTemplateClassId)
            forceInstanceSetLod(
                static_cast<geom::StaticMeshTemplate*>(geometryTemplate)->m_instances, lod);
        else if (classId == kSkinnedMeshTemplateClassId)
            forceInstanceSetLod(
                static_cast<geom::SkinnedMeshTemplate*>(geometryTemplate)->m_instances, lod);
        else if (classId == kBundledMeshTemplateClassId)
            forceInstanceSetLod(
                static_cast<geom::BundledMeshTemplate*>(geometryTemplate)->m_instances, lod);
        else
            ::renddx9abi::logf("BF2Render.cpp:3315 unknown CID for setLods");  // Debug severity 6
    }

    Vec3 origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
#if BF2_PLUGIN_ABI_NATIVE
    std::vector<world::IObject*> objects;
    queryObjectsInRadius(origin, 10000.0f, objects);
    for (std::vector<world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        void* const geometry = world::object_detail::LayoutAccess::geometry(*it);           // +0x38
        if (geometry && lodGeometryTemplate(geometry)->getClassId() == classId)
            forceGeometryLod(geometry, lod);
    }
#else
    vc71::vector_layout objects;
    objects.first = 0;                                   // the allocator word is not written
    objects.last = 0;
    objects.end = 0;
    queryObjectsInRadius(origin, 10000.0f, objects);
    for (char* it = objects.first; it != objects.last; it += sizeof(void*))
    {
        // IObject +0x38, the geometry - a raw field in the retail host's layout (BundledMesh.cpp's
        // objectGeometryField precedent).
        unsigned char* const object = *reinterpret_cast<unsigned char**>(it);
        void* const geometry = *reinterpret_cast<void**>(object + 0x38);
        if (geometry && lodGeometryTemplate(geometry)->getClassId() == classId)
            forceGeometryLod(geometry, lod);
    }
    if (objects.first)
        ::renddx9abi::engineFree(objects.first);          // sub_1005F360, bf_free
#endif
}

// (29 B each, __thiscall on the renderer), the
// forced lod stored at Renderer +0x7D8 / +0x7DC / +0x7E0 (dwords 502 / 503 / 504), then setLods for
// the class. The StaticMesh one is the ground-hemi bake's (0x1012B1B3 restore, 0x1012B4B1 apply); the
// other two are the `renderer.forceSkinnedMeshLod` / `forceBundledMeshLod` SET arms (0x1005945B /
// 0x100595FB), whose GET arms read the stored dword back.
void rendererSetStaticMeshLod(int lod)
{
    *reinterpret_cast<int*>(static_cast<unsigned char*>(g_rendererBase) + 2008) = lod;
    rendererSetLods(lod, kStaticMeshTemplateClassId);
}

void rendererSetSkinnedMeshLod(int lod)
{
    *reinterpret_cast<int*>(static_cast<unsigned char*>(g_rendererBase) + 2012) = lod;
    rendererSetLods(lod, kSkinnedMeshTemplateClassId);
}

void rendererSetBundledMeshLod(int lod)
{
    *reinterpret_cast<int*>(static_cast<unsigned char*>(g_rendererBase) + 2016) = lod;
    rendererSetLods(lod, kBundledMeshTemplateClassId);
}

int rendererGetSkinnedMeshLod()
{
    return *reinterpret_cast<const int*>(static_cast<unsigned char*>(g_rendererBase) + 2012);
}

int rendererGetBundledMeshLod()
{
    return *reinterpret_cast<const int*>(static_cast<unsigned char*>(g_rendererBase) + 2016);
}

// renderer.drawHud (ctor 0x1005CB70, arms 0x1005CCC0): the byte the HudManager paint call and
// hudManagerFramePrep's hudSetPaint read; the SET arm stores the parsed bool unchanged.
bool rendererGetDrawHud()
{
    return static_cast<const unsigned char*>(g_rendererBase)[1713] != 0;
}

void rendererSetDrawHud(bool on)
{
    static_cast<unsigned char*>(g_rendererBase)[1713] = on;
}

void rendererCaptureHemiState(HemiRenderState& state)
{
    unsigned char* const renderer = static_cast<unsigned char*>(g_rendererBase);
    state.bytes[40] = renderer[1695];
    postProductionGetHemiState(reinterpret_cast<float*>(state.bytes + 24));
    *reinterpret_cast<unsigned int*>(state.bytes + 20) =
        *reinterpret_cast<unsigned int*>(renderer + 1728);
    *reinterpret_cast<unsigned int*>(state.bytes + 16) =
        *reinterpret_cast<unsigned int*>(renderer + 2008);
    state.bytes[0] = renderer[1717];
    state.bytes[1] = renderer[1693];
    state.bytes[2] = renderer[1697];
    state.bytes[3] = renderer[1698];
    state.bytes[4] = renderer[1714];
    state.bytes[5] = renderer[1700];
    state.bytes[6] = geom::roadCompiledRendererGetHemiDirectFlag() == 0 ? 1u : 0u;
    state.bytes[7] = renderer[1701];
    state.bytes[8] = renderer[1704];
    state.bytes[9] = renderer[1702];
    state.bytes[10] = renderer[1713];
    state.bytes[11] = *reinterpret_cast<unsigned int*>(renderer + 1708) != 0 ? 1u : 0u;
    state.bytes[15] = renderer[1716];
    // bytes 12..14 and 41..43 are intentionally left untouched by the retail helper.
}

void rendererApplyHemiState()
{
    unsigned char* const renderer = static_cast<unsigned char*>(g_rendererBase);
    renderer[1695] = 0;
    const float postState[4] = { 2000.0f, 2000.0f, 0.0f, 0.0f };
    postProductionSetHemiState(postState);
    *reinterpret_cast<unsigned int*>(renderer + 1728) = 1000u;
    rendererSetStaticMeshLod(0);
    renderer[1717] = 0;
    renderer[1693] = 1;
    renderer[1697] = 0;
    geom::bundledMeshSetHemiEnabled(0); // unguarded in retail
    renderer[1698] = 1;
    geom::staticMeshRendererSetHemiEnabled(1); // unguarded in retail
    renderer[1714] = 0;
    renderer[1700] = 1;
    geom::roadCompiledRendererSetHemiEnabled(1); // unguarded in retail
    renderer[1702] = 0;
    renderer[1703] = 0;
    geom::roadCompiledRendererSetHemiDirectFlag(1); // guarded in its owner seam
    renderer[1701] = 1;
    renderer[1704] = 0;
    renderer[1705] = 1;
    renderer[1713] = 0;
    *reinterpret_cast<unsigned int*>(renderer + 1708) = 0;
    renderer[1716] = 0;
}

void rendererRestoreHemiState(const HemiRenderState& state)
{
    unsigned char* const renderer = static_cast<unsigned char*>(g_rendererBase);
    renderer[1695] = state.bytes[40];
    postProductionSetHemiState(reinterpret_cast<const float*>(state.bytes + 24));
    *reinterpret_cast<unsigned int*>(renderer + 1728) =
        *reinterpret_cast<const unsigned int*>(state.bytes + 20);
    rendererSetStaticMeshLod(*reinterpret_cast<const int*>(state.bytes + 16));
    renderer[1717] = state.bytes[0];
    renderer[1693] = state.bytes[1];
    renderer[1697] = state.bytes[2];
    geom::bundledMeshSetHemiEnabled(state.bytes[2]); // unguarded in retail
    renderer[1698] = state.bytes[3];
    geom::staticMeshRendererSetHemiEnabled(state.bytes[3]); // unguarded in retail
    renderer[1714] = state.bytes[4];
    renderer[1700] = state.bytes[5];
    geom::roadCompiledRendererSetHemiEnabled(state.bytes[5]); // unguarded in retail
    geom::roadCompiledRendererSetHemiDirectFlag(state.bytes[6] == 0 ? 1u : 0u);
    renderer[1701] = state.bytes[7];
    renderer[1704] = state.bytes[8];
    renderer[1705] = state.bytes[9];
    renderer[1702] = state.bytes[9];
    renderer[1703] = state.bytes[9];
    renderer[1713] = state.bytes[10];
    *reinterpret_cast<unsigned int*>(renderer + 1708) = state.bytes[11];
    renderer[1716] = state.bytes[15];
}


// Top-down, orthographic, and cube-face captures used by editor commands.


extern void* g_deviceCaps;   // RendDX9Init.cpp, dword_1023B980 - a D3DCAPS9 at +0

// d3dx9_25!D3DXSaveSurfaceToFileA (IAT 0x101DB4C8), declared as RendDX9Init.cpp and HemiMapManager.cpp
// declare it; the decorated reference resolves through the same /alternatename.
extern "C" HRESULT WINAPI D3DXSaveSurfaceToFileA(const char* destinationFile, unsigned int fileFormat,
                                                 IDirect3DSurface9* source,
                                                 const PALETTEENTRY* sourcePalette,
                                                 const RECT* sourceRect);
#pragma comment(linker, "/alternatename:_D3DXSaveSurfaceToFileA@20=_D3DXSaveSurfaceToFileA")

// byte_10238870, file-backed 1: DebugBoxMesh's draw gate (its draw, 0x10083530, runs only while it is
// set). The cube-face render clears it around each face and restores it. [CHOSEN placement]: the
// owner, GeomMisc\DebugBoxMesh.cpp (attested at 0x1020A7B8), is not in this tree - the definition
// moves there with it, and nothing else in this image reads it yet (RW-273).
// [INFERRED] name.
unsigned char g_debugBoxMeshDrawEnabled = 1;

namespace {

// sub_10005920(0, 0, 0, 0x7FFFFFFF) - the render-target selector with no texture: target 0 becomes the
// temporary back buffer (+0xF4 - the 2006 build's assert names it m_tempBackBuffer) or, with none, the
// back buffer (+0x76C); the choice is AddRef'd around StateManager +0x60 and the temporary reference
// dropped. The same tail rendererSetHemiTarget ends with.
void rendererSelectTempBackBuffer()
{
    unsigned char* const raw = static_cast<unsigned char*>(g_rendererBase);
    IDirect3DSurface9* selected = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x0F4);
    if (!selected)
        selected = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x76C);
    if (selected)
        selected->AddRef();
    stateManagerSetRenderTarget(0, selected);
    if (selected)
        selected->Release();
}

// sub_10077E30 / sub_10077DA0 (both `retn 4`, ecx = the matrix): the whole matrix to identity, then a Y
// (m0 = cos, m2 = -sin, m8 = sin, m10 = cos) or X (m5 = cos, m6 = sin, m9 = -sin, m10 = cos) rotation
// of flt_10238738 (pi/180) times the angle - x87 fsincos on the float-rounded radians, exactly as
// Geom/Overgrowth/OverGrowthCells.cpp's copy of the Y one reproduces it. [CHOSEN] local copies: the
// Mat4 rotation helpers have no shared home yet (RW-273).
void setRotationYDegrees(Mat4& matrix, float degrees)
{
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
    __asm
    {
        fld radians
        fsincos
        fstp cosine
        fstp sine
    }
    for (unsigned int i = 0; i < 16; ++i)
        matrix.m[i] = 0.0f;
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    matrix.m[0] = cosine;
    matrix.m[2] = -sine;
    matrix.m[8] = sine;
    matrix.m[10] = cosine;
}

void setRotationXDegrees(Mat4& matrix, float degrees)
{
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
    __asm
    {
        fld radians
        fsincos
        fstp cosine
        fstp sine
    }
    for (unsigned int i = 0; i < 16; ++i)
        matrix.m[i] = 0.0f;
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    matrix.m[5] = cosine;
    matrix.m[6] = sine;
    matrix.m[9] = -sine;
    matrix.m[10] = cosine;
}

} // anonymous namespace


// Renders a square orthographic view into target using its height as the side length.
// Consumes a caller reference to target and restores views and depth state afterward.
// Requires valid surfaces and render subsystems. The prior depth-surface reference is retained.

void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil,
                                Vec3 position, float orthoExtent, RenderFlags flags)
{
    Renderer* const renderer = static_cast<Renderer*>(g_Renderer);
    unsigned char* const raw = reinterpret_cast<unsigned char*>(renderer);
    IRenderView* const view = static_cast<IRenderView*>(static_cast<void*>(
        classManager->createInstance(Rend::CID_RenderView, Rend::IID_IRenderView, 0)));
    if (!view)
        return;

    D3DSURFACE_DESC desc;
    target->GetDesc(&desc);                                                              // +0x30
    const unsigned int side = desc.Height;
    IRenderView* const savedPrimary = renderer->getRenderView();                        // +0x48
    IRenderView* const savedSecondary = renderer->getRenderViewDup();                   // +0x50

    Mat4 viewMatrix = Mat4::getIdentity();                                   // sub_100791E0 + copy
    const float sideF = static_cast<float>(static_cast<int>(side));                     // fild
    viewMatrix.m[0] = 1.0f;  viewMatrix.m[1] = 0.0f;  viewMatrix.m[2] = 0.0f;
    viewMatrix.m[8] = 0.0f;  viewMatrix.m[9] = -1.0f; viewMatrix.m[10] = 0.0f;
    viewMatrix.m[4] = 0.0f;  viewMatrix.m[5] = 0.0f;  viewMatrix.m[6] = 1.0f;
    viewMatrix.m[12] = position.x;
    viewMatrix.m[13] = position.y;
    viewMatrix.m[14] = position.z;
    view->setViewport(0.0f, 0.0f, sideF, sideF);                                        // +0x10
    view->setProjectionMode(1u);                                                         // +0x48
    view->setNearPlane(0.01f);                                                           // +0x30
    view->setFarPlane(1000.0f);                                                          // +0x38
    view->setOrthoWidth(orthoExtent);                                                    // +0x50
    view->setOrthoHeight(orthoExtent);                                                   // +0x58
    view->setViewMatrix(viewMatrix.m);                                                   // +0x60

    if (raw[1693])
        objectDrawerSetRenderView(*reinterpret_cast<void**>(raw + 1472), view);
    renderer->setRenderView(view);                                                       // +0x44
    renderer->setRenderViewDup(view);                                                    // +0x4C

    IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(renderer)->device;
    IDirect3DSurface9* previousDepthStencil = 0;
    device->GetDepthStencilSurface(&previousDepthStencil);                               // +0xA0
    device->SetDepthStencilSurface(depthStencil);                                        // +0x9C
    *reinterpret_cast<IDirect3DSurface9**>(raw + 0x0F4) = target;
    rendererSelectTempBackBuffer();

    EngineTexture* const savedScratch = renderer->getHemiScratchTexture();              // +0x774
    const int savedManaged = textureManagerSwapManaged(0);
    renderer->setHemiScratchTexture(static_cast<EngineTexture*>(
        textureManagerCreateEmpty(side, side, D3DFMT_A8R8G8B8, 1u, 1u, 0)));
    textureManagerSwapManaged(savedManaged);

    if (raw[0x102])
    {
        renderer->renderScene(0.0, &flags, 0.0f);                                        // +0x2C
        stateManagerSetRenderState(195u, geom::roadCompiledRendererDepthBiasBits());   // depthbias
        stateManagerSetRenderState(175u, geom::roadCompiledRendererSlopeScaleDepthBiasBits());
        // `mov edx, g_Renderer; cmp byte ptr [edx+6A4h], 0` - the same object, reached as the global.
        if (static_cast<unsigned char*>(static_cast<void*>(g_Renderer))[1700] &&
            geom::g_RoadEditableRenderer)
        {
            static_cast<geom::IRoadEditableRenderer*>(geom::g_RoadEditableRenderer)->drawAll();   // +0x18
        }
        stateManagerSetRenderState(195u, 0u);
        stateManagerSetRenderState(175u, 0u);
        renderer->slot_0x30();                                                           // +0x30
    }
    else
    {
        renderer->renderFrame(0.0, 0.0f);                                                // +0x28
    }

    device->SetDepthStencilSurface(previousDepthStencil);                                // +0x9C
    IDirect3DSurface9*& tempBackBuffer = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x0F4);
    if (tempBackBuffer)
        tempBackBuffer->Release();
    tempBackBuffer = 0;
    rendererSelectTempBackBuffer();
    renderer->setRenderView(savedPrimary);                                               // +0x44
    renderer->setRenderViewDup(savedSecondary);                                          // +0x4C
    renderer->present(2u);                                                               // +0xD0
    view->release();                                                                     // +0x08

    EngineTexture* const scratch = renderer->getHemiScratchTexture();
    if (--scratch->refCount == 0)
    {
        textureDtor(scratch);
        ::renddx9abi::engineFree(scratch);
    }
    renderer->setHemiScratchTexture(savedScratch);
}


// Saves a top-down world capture to name + ".dds", relative to the working directory.
// Capture size is always 2048 squared; resolution is inspected but does not change it.
// Requires terrain and valid device resources; leaves texture management enabled.

void rendererMakeTopWorldScreen(unsigned int resolution, renddx9abi::HostString name,
                                Vec3 position, float orthoExtent)
{
    const D3DCAPS9* const caps = static_cast<const D3DCAPS9*>(g_deviceCaps);
    if (resolution > caps->MaxTextureHeight)
        resolution = caps->MaxTextureHeight;
    if (resolution > caps->MaxTextureWidth)
    {
        const unsigned int droppedClamp = caps->MaxTextureWidth;     // asked again, never stored
        (void)droppedClamp;
    }
    (void)resolution;                                                // never read again (shipped)

    textureManagerSwapManaged(1);                                    // the old value dropped

    unsigned char* const self = static_cast<unsigned char*>(static_cast<void*>(g_Renderer));
    IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(self)->device;
    IDirect3DSurface9* target = 0;
    IDirect3DSurface9* depthStencil = 0;
    device->CreateRenderTarget(2048u, 2048u, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0u, FALSE,
                               &target, 0);                                              // +0x70
    device->CreateDepthStencilSurface(2048u, 2048u, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0u, FALSE,
                                      &depthStencil, 0);                                 // +0x74

    RenderFlags flags;
    flags.useRenderView    = 0;
    flags.field01          = self[1701];
    flags.world            = self[1693];
    flags.worldAlt         = 1;
    flags.field04          = self[1697];
    flags.staticMeshes     = self[1698];
    flags.skinnedMeshes    = self[1699];
    flags.decals           = self[1696];
    flags.particles        = self[1756];
    flags.field09          = 0;
    flags.field0a          = 0;
    flags.skyDome          = self[1714];
    flags.undergrowth      = self[1702];
    flags.field0d          = self[1703];
    flags.postProduction   = 0;
    flags.colourCorrection = 0;
    flags.hud              = self[1713];
    flags.aiDebug          = 0;
    flags.field12          = static_cast<geom::Terrain*>(geom::g_Terrain)->m_flag35A;
    flags.pad13            = 0;                                      // unwritten in retail
    flags.renderView       = 0;                                      // unwritten in retail
    rendererRenderOrthoTopDown(target, depthStencil, position, orthoExtent, flags);

    renddx9abi::HostString path(name);
    path += ".dds";
    renddx9abi::HostStream* const stream = ::dice::hfe::fileManager->open(
        path, static_cast<renddx9abi::HostFileMode>(3), static_cast<renddx9abi::HostFileAccess>(2),
        0);                                                                              // +0x18
    if (stream)
        stream->release();
    ::dice::hfe::fileManager->deleteFile(path);                                         // +0x20
    D3DXSaveSurfaceToFileA(path.c_str(), 4u, target, 0, 0);                             // D3DXIFF_DDS
    D3DSURFACE_DESC unusedDesc;
    target->GetDesc(&unusedDesc);                                                        // +0x30
    target->Release();
    depthStencil->Release();
}


// Renders one cube face at position, using a 90-degree view.
// Always returns false, retains the temporary view and leaves toggle +1701 enabled.
// Invalid face indices log and use an unrotated view.

bool rendererRenderEnvMapFace(unsigned int size, EngineTexture* cube, int face, const Vec3& position)
{
    Renderer* const renderer = static_cast<Renderer*>(g_Renderer);
    unsigned char* const raw = reinterpret_cast<unsigned char*>(renderer);
    IRenderView* const view = static_cast<IRenderView*>(static_cast<void*>(
        classManager->createInstance(Rend::CID_RenderView, Rend::IID_IRenderView, 0)));
    if (!view)
        return false;

    IRenderView* const savedPrimary = renderer->getRenderView();                        // +0x48
    IRenderView* const savedSecondary = renderer->getRenderViewDup();                   // +0x50
    Mat4 viewMatrix = Mat4::getIdentity();
    const float side = static_cast<float>(size);
    view->setViewport(0.0f, 0.0f, side, side);                                          // +0x10
    view->setProjectionMode(0u);                                                         // +0x48
    view->setNearPlane(0.01f);                                                           // +0x30
    view->setFarPlane(1000.0f);                                                          // +0x38
    view->setFov(0.01745329238474369f * 90.0f);                                         // +0x1C
    view->setAspect(1.0f);                                                               // +0x40

    switch (static_cast<unsigned int>(face))
    {
    case 0: setRotationYDegrees(viewMatrix, 90.0f);  break;
    case 1: setRotationYDegrees(viewMatrix, 270.0f); break;
    case 2: setRotationXDegrees(viewMatrix, -90.0f); break;
    case 3: setRotationXDegrees(viewMatrix, 90.0f);  break;
    case 4: break;
    case 5: setRotationYDegrees(viewMatrix, 180.0f); break;
    default:
        ::renddx9abi::logf("BF2Render.cpp:814 Cannot render face %u unsupported face ID",
                           static_cast<unsigned int>(face));                          // severity 6
        break;
    }
    viewMatrix.m[12] = position.x;
    viewMatrix.m[13] = position.y;
    viewMatrix.m[14] = position.z;
    view->setViewMatrix(viewMatrix.m);                                                   // +0x60

    const unsigned char savedObjects    = raw[1693];
    const unsigned char savedHud        = raw[1713];
    const unsigned char saved1705       = raw[1705];
    const unsigned char savedBundled    = raw[1697];
    const unsigned char savedDebugBoxes = g_debugBoxMeshDrawEnabled;
    const unsigned char saved1702       = raw[1702];
    const unsigned char saved1703       = raw[1703];
    const unsigned char saved1717       = raw[1717];
    raw[1713] = 0;
    raw[1705] = 0;
    raw[1701] = 1;
    raw[1697] = 0;
    geom::bundledMeshSetHemiEnabled(0);                            // dword_102597F4 +0x10, unguarded
    g_debugBoxMeshDrawEnabled = 0;
    raw[1702] = 0;
    raw[1703] = 0;
    raw[1717] = 0;
    if (raw[1693])
        objectDrawerSetRenderView(*reinterpret_cast<void**>(raw + 1472), view);
    renderer->setRenderView(view);                                                       // +0x44
    renderer->setRenderViewDup(view);                                                    // +0x4C

    IDirect3DSurface9*& tempBackBuffer = *reinterpret_cast<IDirect3DSurface9**>(raw + 0x0F4);
    tempBackBuffer = textureGetLevelSurface(cube, 0u, static_cast<unsigned int>(face));
    rendererSelectTempBackBuffer();
    renderer->renderFrame(0.0, 0.0f);                                                    // +0x28
    if (tempBackBuffer)
        tempBackBuffer->Release();
    tempBackBuffer = 0;
    rendererSelectTempBackBuffer();
    renderer->setRenderView(savedPrimary);
    renderer->setRenderViewDup(savedSecondary);

    raw[1697] = savedBundled;
    geom::bundledMeshSetHemiEnabled(savedBundled);
    raw[1693] = savedObjects;
    raw[1713] = savedHud;
    raw[1705] = saved1705;
    g_debugBoxMeshDrawEnabled = savedDebugBoxes;
    raw[1702] = saved1702;
    raw[1703] = saved1703;
    raw[1717] = saved1717;
    return false;
}

// The remaining slots. Each names itself and its offset, so the log line that appears when BF2
// reaches one IS the next reversing target - no guessing which of 55 was entered.
#define RD9_RENDERER_FRONTIER(method, slotOffset)                                   \
    void Renderer::method()                                                         \
    {                                                                               \
        RD9_FRONTIER("dice::hfe::dx9rend::Renderer::" #method, slotOffset);         \
    }

// Original 1.50 10009DD0 / 10009DE0 both explicitly materialize AL only.
bool Renderer::slot_0x14() { return true; }
bool Renderer::slot_0x18() { return true; }

// Stamps the frame clock and optionally dispatches the seven-argument frame hook.
// Requires the system singleton. [FRONTIER] The hook's enabling byte at +0x7F0 has no writer.

void Renderer::beginFrame()
{
    m_frameStartTime = hostCalls::systemWallClockTime();                              // +0x18

    if (m_tail1bAfterHemiScratch[0x7F0 - 0x778] != 0)
    {
        typedef void (__fastcall* Fn)(void*, void*, unsigned int, unsigned int, unsigned int,
                                      unsigned int, unsigned int, unsigned int, unsigned int);
        reinterpret_cast<Fn>((*static_cast<void***>(static_cast<void*>(this)))[0x60 / 4])(
            this, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}
// PE 0x1002BC50, slot +0x2C: render the world according to RenderFlags.
// [FRONTIER] The helper and render-queue contracts still block this body.
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: hud-manager-call-frontiers]
bool Renderer::renderScene(double, const RenderFlags*, float)
{
    RD9_FRONTIER("dice::hfe::dx9rend::Renderer::renderScene", 0x2C);
    // Unreachable unless BF2_RENDDX9_FRONTIER=continue. `true` is not a fabricated success: the
    // shipped body materializes AL=1 on every path and has no false arm at all.
    return true;
}
RD9_RENDERER_FRONTIER(slot_0x30,   0x30)
RD9_RENDERER_FRONTIER(slot_0x34,   0x34)
RD9_RENDERER_FRONTIER(slot_0x38,   0x38)

// The display-mode enumeration (Renderer slot +0x3C, 0x10007420, and its helpers). Builds the
// Renderer's Mode40 list from the D3D adapter modes, cross-checked against the Win32 display list and
// annotated with each mode's supported depth-stencil formats. VideoOptions::buildResolutionList
// (sub_10010710) reads this list to fill the menu's resolution strings.

int bitsPerPixel(unsigned int d3dFormat);   // RendDX9Types.cpp - D3DFORMAT -> bpp

namespace {

// sub_1000CD20 - bpp -> a representative D3DFORMAT (inverse of bitsPerPixel). Only 32/16-bit matter
// here (the enumeration passes X8R8G8B8 and R5G6B5); other depths fall back to 32-bit.
D3DFORMAT formatFromBpp(unsigned int bpp)
{
    return (bpp == 16) ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8;
}

// sub_10006990 - enumerate the adapter's modes for R5G6B5 (16-bit) then X8R8G8B8 (32-bit), keeping
// (when minRes) only >= 640x480 @ 60Hz. Appends each D3DDISPLAYMODE to `out`.
void enumAdapterModes(RendDX9Base* b, bool minRes, vc71::vector_layout* out)
{
    const D3DFORMAT passes[2] = { D3DFMT_R5G6B5, D3DFMT_X8R8G8B8 };
    vc71::vectorRef<D3DDISPLAYMODE> v(out);
    for (int p = 0; p < 2; ++p)
    {
        const UINT count = b->d3d9->GetAdapterModeCount(b->adapter, passes[p]);
        for (UINT i = 0; i < count; ++i)
        {
            D3DDISPLAYMODE m;
            if (FAILED(b->d3d9->EnumAdapterModes(b->adapter, passes[p], i, &m)))
                continue;
            if (!minRes || (m.Width >= 640 && m.Height >= 480 && m.RefreshRate >= 60))
                v.push_back(m);
        }
    }
}

// sub_10006B40 - the Win32 display list, used only to cross-check the D3D modes. Keeps 16/32bpp modes
// with no special flags, matching the min-resolution and (optional) 4:3 gates.
void enumDevModes(bool aspect43, bool minRes, vc71::vector_layout* out)
{
    vc71::vectorRef<DEVMODEA> v(out);
    for (DWORD i = 0; ; ++i)
    {
        DEVMODEA dm;
        ::memset(&dm, 0, sizeof(dm));
        dm.dmSize = sizeof(DEVMODEA);
        if (!EnumDisplaySettingsA(0, i, &dm))
            break;
        if (dm.dmDisplayFlags != 0)
            continue;
        if (dm.dmBitsPerPel != 16 && dm.dmBitsPerPel != 32)
            continue;
        if (minRes && !(dm.dmPelsWidth >= 640 && dm.dmPelsHeight >= 480 && dm.dmDisplayFrequency >= 60))
            continue;
        if (aspect43)
        {
            const double r = (double)dm.dmPelsWidth / (double)dm.dmPelsHeight - 1.3333334;
            if (r > 0.1 || r < -0.1)
                continue;
        }
        v.push_back(dm);
    }
}

// sub_10006C60 - fill each mode's depth-stencil format list by probing D3DFMT_D32 (71) and
// D3DFMT_D24S8 (75) with CheckDeviceFormat; the mode's own bpp gives the adapter format.
void fillDepthFormats(RendDX9Base* b)
{
    const D3DFORMAT probes[2] = { D3DFMT_D32, D3DFMT_D24S8 };
    for (Mode40* rec = reinterpret_cast<Mode40*>(b->modeList.first);
         rec != reinterpret_cast<Mode40*>(b->modeList.last); ++rec)
    {
        const D3DFORMAT adapterFmt = formatFromBpp(rec->bpp);
        vc71::vectorRef<unsigned int> d(&rec->depthFormats);
        for (int k = 0; k < 2; ++k)
        {
            if (SUCCEEDED(b->d3d9->CheckDeviceFormat(b->adapter, static_cast<D3DDEVTYPE>(b->deviceType),
                                                     adapterFmt, D3DUSAGE_DEPTHSTENCIL,
                                                     D3DRTYPE_SURFACE, probes[k])))
                d.push_back(static_cast<unsigned int>(probes[k]));
        }
    }
}

// sub_10007420's body, non-virtual so it can be reached both through the Renderer vtable (slot +0x3C)
// and directly (rendererGetModeList): VideoOptions::initialize builds the list during construction,
// when the object's vptr is not yet guaranteed to be the Renderer vtable. Rebuilds once: enumerate the
// adapter modes (>=640x480@60), cross-check each against the Win32 display list (kept only when that
// list is non-empty), push the survivors as Mode40 headers, then annotate each with its supported
// depth-stencil formats.
void buildModeListImpl(RendDX9Base* b)
{
    if (b->modeList.first && b->modeList.first != b->modeList.last)
        return;      // already built
    if (!b->d3d9)
        return;

    vc71::vector_layout devModes;     devModes.alloc = 0;     devModes.first = 0;     devModes.last = 0;     devModes.end = 0;
    vc71::vector_layout adapterModes; adapterModes.alloc = 0; adapterModes.first = 0; adapterModes.last = 0; adapterModes.end = 0;

    enumDevModes(false /*aspect43*/, true /*minRes*/, &devModes);
    enumAdapterModes(b, true /*minRes*/, &adapterModes);
    const bool haveDevModes = (devModes.first != devModes.last);

    vc71::vectorRef<Mode40> modes(&b->modeList);
    for (const D3DDISPLAYMODE* m = reinterpret_cast<const D3DDISPLAYMODE*>(adapterModes.first);
         m != reinterpret_cast<const D3DDISPLAYMODE*>(adapterModes.last); ++m)
    {
        Mode40 rec;
        rec.width   = static_cast<unsigned short>(m->Width);
        rec.height  = static_cast<unsigned short>(m->Height);
        rec.refresh = static_cast<unsigned short>(m->RefreshRate);
        rec.bpp     = static_cast<unsigned char>(bitsPerPixel(m->Format));

        bool keep = !haveDevModes;   // no Win32 list -> keep every enumerated mode (the shipped v29 gate)
        if (haveDevModes)
        {
            for (const DEVMODEA* dm = reinterpret_cast<const DEVMODEA*>(devModes.first);
                 dm != reinterpret_cast<const DEVMODEA*>(devModes.last); ++dm)
            {
                if (dm->dmPelsWidth == m->Width && dm->dmPelsHeight == m->Height &&
                    dm->dmDisplayFrequency == m->RefreshRate &&
                    dm->dmBitsPerPel == rec.bpp)
                {
                    keep = true;
                    break;
                }
            }
        }
        if (keep)
            modes.push_back(rec);
    }

    vectorClear<D3DDISPLAYMODE>(&adapterModes);
    vectorClear<DEVMODEA>(&devModes);

    fillDepthFormats(b);
}

} // anonymous namespace

// the mode-list builder (Renderer vtable slot +0x3C, `retn 4`: one filter argument,
// IRenderer.h). Returns `this+0x90`, building the list whenever it is null or empty
// (0x10007439..0x1000746E) - in practice once, by VideoOptions::initialize. `this` is the base
// object, as for slot +0x74. [CHOSEN] this rebuild keeps its fixed (false, true, true) policy and
// ignores the filter bytes; see IRenderer.h for what each byte means to the shipped body.
void* Renderer::slot_0x3C(const DisplayModeFilter* /*filter*/)
{
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(this);
    buildModeListImpl(b);
    return &b->modeList;
}

// Non-virtual seam: VideoOptions::buildResolutionList calls this instead of dispatching the base
// vtable, because it runs during construction (RendDX9_base_ctor -> initializeD3DSubsystems ->
// VideoOptions::initialize), when the object's vptr may still be a base vtable, not the Renderer's.
void* rendererGetModeList(void* base)
{
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(base);
    buildModeListImpl(b);
    return &b->modeList;
}
RD9_RENDERER_FRONTIER(slot_0x40,   0x40)
// the one-instruction frame ordinal getter. Renderer::update increments the same
// dword at +0x104, and the directional LightManager forwards it to IGeometry +0xAC.
unsigned int Renderer::getFrameOrdinal()
{
    return *reinterpret_cast<unsigned int*>(
        reinterpret_cast<unsigned char*>(this) + 0x104);
}


} // namespace dx9rend - reopened below: the four console-object seams are geom's

namespace geom {
// FontSystem/FontSystem.cpp and OverlayTexture.cpp - typed virtual forwards onto the two console
// objects initializeFontAndConsoleOverlay created (held here as IBase*, as createInstance returned).
int  fontPortedGetHeight(IBase* font);                                            // Font::Ported +0x1C
void fontPortedDrawText(IBase* font, float x, float y, const vc71::string& text);  // Font::Ported +0x18
void overlayTextureSetSize(IBase* overlay, const Vec2& size);                      // OverlayTexture +0x18
void overlayTextureDraw(IBase* overlay);                                           // OverlayTexture +0x10
} // namespace geom

namespace dx9rend {

#if !BF2_PLUGIN_ABI_NATIVE
namespace {

// Compatibility Console prefix through getLines, whose result uses VC7.1 strings/vectors.
// Reserved preceding slots preserve order and must never be called.
class RetailHostConsole
{
public:
    virtual void reserved_deletingDestructor() = 0;                          // +0x00
    virtual void reserved_setConsoleCompiler() = 0;                          // +0x04
    virtual void reserved_executeLine() = 0;                                 // +0x08
    virtual void reserved_executeLines() = 0;                                // +0x0C
    virtual void reserved_output() = 0;                                      // +0x10
    virtual void reserved_updateAsciiKey() = 0;                              // +0x14
    virtual void reserved_updateGameInput() = 0;                             // +0x18
    virtual void reserved_updateKeyMacros() = 0;                             // +0x1C
    virtual void getLines(int rows, vc71::vector_layout& out) const = 0;     // +0x20

protected:
    ~RetailHostConsole() {}
};

} // anonymous namespace
#endif // !BF2_PLUGIN_ABI_NATIVE

// Draw the last 20 console lines, including the current prompt, over a full-width background.
// Re-read font height per line and restore statistics before destroying the line vector.
// Compat strings use the VC7.1 CRT; their vector storage uses the engine heap.
// [CHOSEN, not recovered] native lines are copied to vc71::string at the font boundary.
void Renderer::slot_0x58()
{
    const unsigned char statsWere = statsSwapEnabled(0);

#if BF2_PLUGIN_ABI_NATIVE
    std::vector<std::string> lines;
    reinterpret_cast<io::Console*>(::dice::hfe::mainConsole)->getLines(20, lines);
    const unsigned int lineCount = static_cast<unsigned int>(lines.size());
#else
    vc71::vector_layout lines;
    lines.alloc = 0;
    lines.first = 0;
    lines.last  = 0;
    lines.end   = 0;
    reinterpret_cast<RetailHostConsole*>(::dice::hfe::mainConsole)->getLines(20, lines);
    vc71::string* const firstLine = reinterpret_cast<vc71::string*>(lines.first);
    vc71::string* const lastLine  = reinterpret_cast<vc71::string*>(lines.last);
    const unsigned int lineCount =
        firstLine ? static_cast<unsigned int>(lastLine - firstLine) : 0u;
#endif

    IBase* const font    = static_cast<IBase*>(m_consoleFont);                  // +0x5CC
    IBase* const overlay = static_cast<IBase*>(m_consoleOverlay);               // +0x5C8

    const int fontHeight = geom::fontPortedGetHeight(font);
    const float consoleHeight =
        (static_cast<float>(fontHeight) + 1.0f) * static_cast<float>(lineCount) + 4.0f;

    Vec2 overlaySize;
    overlaySize.x = 1.0f;
    overlaySize.y = consoleHeight / getRenderView()->getViewport()[3];
    geom::overlayTextureSetSize(overlay, overlaySize);
    geom::overlayTextureDraw(overlay);

    // [CHOSEN] one-shot diagnostic, NOT in retail: proves on a live run that the console draw ran.
    static bool s_firstDrawLogged = false;
    if (!s_firstDrawLogged)
    {
        s_firstDrawLogged = true;
        ::renddx9abi::logf("Renderer::slot_0x58 (console draw): first call - %u console lines, "
                           "line height %d, background %.1f px", lineCount, fontHeight,
                           static_cast<double>(consoleHeight));
    }

    int line = 0;
#if BF2_PLUGIN_ABI_NATIVE
    for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end();
         ++it, ++line)
    {
        const vc71::string text = ::renddx9abi::toVc71String(*it);
        const int height = geom::fontPortedGetHeight(font);                    // re-read per line
        geom::fontPortedDrawText(font, 2.0f, static_cast<float>((height + 1) * line + 2), text);
    }
#else
    for (vc71::string* text = firstLine; text != lastLine; ++text, ++line)
    {
        const int height = geom::fontPortedGetHeight(font);                    // re-read per line
        geom::fontPortedDrawText(font, 2.0f, static_cast<float>((height + 1) * line + 2), *text);
    }
#endif

    statsSwapEnabled(statsWere);

#if !BF2_PLUGIN_ABI_NATIVE
    if (firstLine)
    {
        for (vc71::string* text = firstLine; text != lastLine; ++text)
            text->~string();
        ::renddx9abi::engineFree(firstLine);
    }
#endif
}   // native: ~vector<std::string>

RD9_RENDERER_FRONTIER(slot_0x5C,   0x5C)
RD9_RENDERER_FRONTIER(slot_0x60,   0x60)
RD9_RENDERER_FRONTIER(slot_0x64,   0x64)
RD9_RENDERER_FRONTIER(slot_0x68,   0x68)
// 0x101432E0, slot +0x6C.  One instruction - `lea eax, [ecx+108h]` - the
// address of the draw/resource statistics block embedded at +0x108.  Texture.cpp already documents
// that block (its +0x1C counts vertex buffers, its +0x2C textures); RenderQueues.cpp bumps its
// per-category draw counters through this slot when an entry is added to a queue.
void* Renderer::getRenderStats()
{
    return reinterpret_cast<unsigned char*>(this) + 0x108;
}
RD9_RENDERER_FRONTIER(slot_0x70,   0x70)
// one instruction: `return this + 1016`. The embedded VideoOptions getter, identical to
// the base's own +0x74. VideoOptions is constructed inside this object at +0x3F8 (1016) - the same
// sub-object VideoOptions::apply reaches at `this + 0x3F8` - with its 15-slot vtable installed, so the
// pointer handed back is live. A menu screen fetches it here to read the current resolution/quality.
IVideoOptions* Renderer::getVideoOptions()
{
    return reinterpret_cast<RendDX9Base*>(this)->embeddedVideoOptions();
}
RD9_RENDERER_FRONTIER(slot_0x78,   0x78)
RD9_RENDERER_FRONTIER(slot_0x7C,   0x7C)
RD9_RENDERER_FRONTIER(slot_0x80,   0x80)
RD9_RENDERER_FRONTIER(slot_0x84,   0x84)
RD9_RENDERER_FRONTIER(slot_0x88,   0x88)
RD9_RENDERER_FRONTIER(slot_0x8C,   0x8C)
RD9_RENDERER_FRONTIER(slot_0x90,   0x90)
RD9_RENDERER_FRONTIER(slot_0x94,   0x94)
RD9_RENDERER_FRONTIER(slot_0x98,   0x98)
RD9_RENDERER_FRONTIER(slot_0x9C,   0x9C)

// +0xA0, 0x100286A0. Exact two-instruction body: `fld qword ptr [ecx+600h]; retn`.
// BF2.exe calls it twice in order at 0x453493/0x4534AB, first for the positive gate and then for
// `1.0 / rate`. [INFERRED] method spelling; the binary64 ABI and measured-FPS role are recovered.
double Renderer::getFrameRate()
{
    return m_frameRate;
}

// +0xA4/+0xA8, PE 0x100327A0/0x10032730. The console property registration names this uint
// `renderAheadLimit`. The setter's unsigned `jbe 5` test preserves zero and clamps only values
// above five; the getter is the one-instruction +0x5B8 read BF2 publishes for latency compensation.
void Renderer::setRenderAheadLimit(unsigned int limit)
{
    if (limit > 5u)
        limit = 5u;
    m_renderAheadLimit = limit;
}

unsigned int Renderer::getRenderAheadLimit()
{
    return m_renderAheadLimit;
}


// +0xAC, 0x10032740. ONE instruction: `return *(BYTE*)(this + 1892)`, which is +0x764.

// The effect path fixup calls it to choose between "mods/xpack/" and "mods/bf2/", so it is the flag
// that says whether the expansion pack is active. The byte lives inside the opaque tail this
// rebuild zeroes, and WHO sets IT IS unread - so it reads 0 here and every unprefixed shader path
// resolves under mods/bf2/, which is correct for a stock install and wrong for one running xpack.
// [FRONTIER] dice::hfe::dx9rend::Renderer - whoever writes the byte at +0x764

bool Renderer::isExpansionPack()
{
    return m_tail1bAfterShaderPath[0x764 - 0x6D0] != 0;
}


// Sets expansion mode and returns whether it changed, triggering shader remount decisions.

bool Renderer::setExpansionPack(unsigned char value)
{
    if (static_cast<unsigned char>(m_tail1bAfterShaderPath[0x764 - 0x6D0]) == value)
        return false;
    m_tail1bAfterShaderPath[0x764 - 0x6D0] = static_cast<char>(value);
    return true;
}


// Returns the constructor-initialized word at +0x768 (normally 2).
// [UNRESOLVED] Its meaning; the unsigned carrier is [CHOSEN].

unsigned int Renderer::slot_0xB0()
{
    return *reinterpret_cast<const unsigned int*>(&m_tail1bAfterShaderPath[0x768 - 0x6D0]);
}
RD9_RENDERER_FRONTIER(slot_0xB4,   0xB4)

// flush cached texture references while keeping the device alive.
// Clear effect bindings, optionally reset effects, then run StateManager lost/reset.
// Retail tests the flag twice; EffectManager's reset listener is empty. Receivers are
// unguarded globals. CEUnloadLevel passes false before the outgoing texture sweep.
void Renderer::slot_0xB8(bool includeEffectManager)
{
    shaderManagerClearEffectTextureBindings(g_shaderManager);

    DeviceLostResetListener* const stateListener = reinterpret_cast<DeviceLostResetListener*>(
        static_cast<unsigned char*>(g_stateManager) + 4);                      // dword_1023B984 + 4

    if (includeEffectManager)
        Ra::effectManagerResetEffects();       // EffectManager listener +0x00: 0x10037D50 -> sub_10034210
    stateListener->onDeviceLost();                                            // listener +0x00

    // the second flag test - the EffectManager listener's +0x04 is 0x10037D40, a bare
    // `ret`, so a set flag does nothing more here.
    stateListener->onDeviceReset();                                           // listener +0x04
}


// Pumps window events while inactive, then waits for device restoration.
// Returns true only when no wait was needed. Both waits are unbounded.
// Requires the event manager and system singleton.

namespace {

// Host System PE +0x1C - sleep(int milliseconds) (Core/System.h: ELF +0x20, one destructor slot
// fewer in the PE). The global is re-read on every call, as both shipped call sites do
// (`mov ecx, dword_10244AC0; mov edx, [ecx]; push 64h; call [edx+1Ch]`).
// this file now includes Core/System.h, whose MSVC table puts sleep at slot 7 (+0x1C) in both
// flavours.
void hostSystemSleep(int milliseconds)
{
    hostCalls::systemSleep(milliseconds);                                             // +0x1C
}

} // anonymous namespace

bool Renderer::waitWhileAppInactive()
{
    RendDX9Base* const b = reinterpret_cast<RendDX9Base*>(this);

    bool inactive = false;      // bl
    bool waited   = false;      // [ebp-1]
    do
    {
        ::dice::hfe::eventManager->postEvent(ECCore, 16, static_cast<void*>(0), 0.0f);
        if (inactive)
        {
            b->inScene = 0;
            hostSystemSleep(100);
            handleLostDevice(this);                         // answer discarded
            waited = true;
        }
        inactive = isAppInactive();                         // virtual +0xC0
    }
    while (inactive);

    if (waited)
    {
        do
        {
            hostSystemSleep(100);
        }
        while (!handleLostDevice(this));
    }
    return !waited;                                         // 0x1000562A `test bl, bl; sete al`
}

// +0xC0, (12 B) - [INFERRED] isAppInactive.
// `xor eax, eax; cmp byte ptr [ecx+517h], al; sete al; ret` - the CEActivateApp byte is zero.
bool Renderer::isAppInactive()
{
    return reinterpret_cast<const RendDX9Base*>(this)->appActive == 0;
}


// Returns the mapped shader-path index used by RAPATH and geometry quality-mask tables.

int Renderer::getShaderPath() const
{
    return m_shaderPath;
}


// PE 0x100286C0: store the path, then dispatch Renderer slot +0xC4.

void Renderer::setShaderPathAndUpdate(int path)
{
    m_shaderPath = path;
    refreshShaderDefines();
}


// [CHOSEN, not recovered] owner-TU seams keep the concrete Renderer layout out of renderer
// clients. The getter preserves retail's non-null precondition; the setter wrapper retains
// VideoOptions' existing null handling.

int rendererGetShaderPath(const void* renderer)
{
    return static_cast<const Renderer*>(renderer)->getShaderPath();
}

void rendererSetShaderPathAndUpdate(void* renderer, int path)
{
    if (renderer)
        static_cast<Renderer*>(renderer)->setShaderPathAndUpdate(path);
}


// Refreshes global shader defines using this renderer's base configuration.

void Renderer::refreshShaderDefines()
{
    RD9_TRACE("dice::hfe::dx9rend::Renderer::refreshShaderDefines", 0xC4);

    if (!g_shaderManager)
    {
        ::renddx9abi::logf("Renderer::refreshShaderDefines: the ShaderManager is null - the "
                           "original would jump into it regardless");
        return;
    }
    updateShaderDefines(g_shaderManager, this);
}


// First-level-load gates start enabled so shader archives mount and shader state reloads
// before geometry techniques are requested. Later loads repeat only after expansion-mode changes.

unsigned char g_shaderSystemReloadPending = 1;   // byte_10238248, initial value 1
unsigned char g_shaderArchiveMountPending = 1;   // byte_102383F8, initial value 1

namespace {


// Native containers use the host STL; the compatibility views retain VC7.1 layout.
// isNightMap is initialized only by a successful map-description parse.

#if BF2_PLUGIN_ABI_NATIVE

typedef ::dice::hfe::IModInfo::Map HostLevelMap;
typedef ::dice::hfe::IModInfo::Mod HostModRecord;

#else

// MSVC will not accept `p->~HostString()` through a typedef, so the element destructors below go
// through this one-liner instead. It is a spelling device, nothing more.
template <typename T>
inline void destroyHostElement(char* p) { reinterpret_cast<T*>(p)->~T(); }

// sub_10043C70 / sub_10043CD0 and sub_10043DA0 / sub_10043E40, as VC7.1-layout mirrors. They exist
// only to be handed to the host's parseMapDesc, which fills them with the host's own allocations -
// and that is safe across the DLL boundary for the same reason everything else here is: dice routed
// operator new/delete in every image through Memory.dll, so engineFree releases what the host's
// operator new took (see RendDX9Abi/BfMemory.h).
struct HostLevelMap
{
    HostLevelMap()
    {
        gameModes.first = 0;                 // 10043C9x - first/last/end only...
        gameModes.last  = 0;
        gameModes.end   = 0;
        gameSpyId       = 0;
        plasma          = 0;
        // ...and neither gameModes.alloc NOR isNightMap. Faithful; see the block above.
    }

    ~HostLevelMap()
    {
        if (gameModes.first)                 // sub_10043CD0's `if (*(this + 144))`
        {
            // sub_10043C10 destroys each element, then sub_1005F360 frees the buffer.
            for (char* p = gameModes.first; p != gameModes.last; p += sizeof(HostGameMode))
                reinterpret_cast<HostGameMode*>(p)->~HostGameMode();
            ::renddx9abi::engineFree(gameModes.first);
        }
        gameModes.first = 0;
        gameModes.last  = 0;
        gameModes.end   = 0;
    }

    // IModInfo::GameMode in VC7.1 - one string plus a vector<MapType>; 44 bytes. Only its size and
    // its teardown matter here, and the teardown of a MapType is three strings after a leading int.
    struct HostMapType
    {
        int                 players;
        renddx9abi::HostString type;
        renddx9abi::HostString locId;
        renddx9abi::HostString description;
        unsigned char       ai;
        unsigned char       pad[3];
    };

    struct HostGameMode
    {
        renddx9abi::HostString name;
        vc71::vector_layout    mapTypes;

        ~HostGameMode()
        {
            if (mapTypes.first)
            {
                for (char* p = mapTypes.first; p != mapTypes.last; p += sizeof(HostMapType))
                    reinterpret_cast<HostMapType*>(p)->~HostMapType();
                ::renddx9abi::engineFree(mapTypes.first);
            }
        }
    };

    renddx9abi::HostString path;             // +0    the level directory name
    renddx9abi::HostString name;             // +28
    renddx9abi::HostString briefing;         // +56
    renddx9abi::HostString briefingLocId;    // +84
    renddx9abi::HostString music;            // +112
    vc71::vector_layout    gameModes;        // +140
    int                    gameSpyId;        // +156
    unsigned char          plasma;           // +160
    unsigned char          isNightMap;       // +161  <- the byte selectShaderArchive tests
    unsigned char          pad[2];
};

static_assert(sizeof(HostLevelMap) == 164, "the PE frame reserves 164 bytes at ebp-0x114");

struct HostModRecord
{
    HostModRecord()
    {
        maps.first      = 0;                 // 10043E0x - 256/260/264
        maps.last       = 0;
        maps.end        = 0;
        gameModes.first = 0;                 //            272/276/280
        gameModes.last  = 0;
        gameModes.end   = 0;
    }

    ~HostModRecord()
    {
        // sub_100111E0(this + 268) - parseMapDesc appends to this one, so it really can be non-empty.
        if (gameModes.first)
        {
            for (char* p = gameModes.first; p != gameModes.last;
                 p += sizeof(renddx9abi::HostString))
                destroyHostElement<renddx9abi::HostString>(p);
            ::renddx9abi::engineFree(gameModes.first);
        }
        // sub_10043D50(this + 252)
        if (maps.first)
        {
            for (char* p = maps.first; p != maps.last; p += sizeof(HostLevelMap))
                reinterpret_cast<HostLevelMap*>(p)->~HostLevelMap();
            ::renddx9abi::engineFree(maps.first);
        }
    }

    renddx9abi::HostString path;             // +0
    renddx9abi::HostString name;             // +28
    renddx9abi::HostString title;            // +56
    renddx9abi::HostString desc;             // +84
    renddx9abi::HostString url;              // +112
    renddx9abi::HostString music;            // +140
    renddx9abi::HostString logo;             // +168
    renddx9abi::HostString icon;             // +196
    renddx9abi::HostString version;          // +224
    vc71::vector_layout    maps;             // +252
    vc71::vector_layout    gameModes;        // +268
    renddx9abi::HostString importLevels;     // +284
    renddx9abi::HostString spStartupMap;     // +312
};

static_assert(sizeof(HostModRecord) == 340, "the PE frame reserves 340 bytes at ebp-0x268");

#endif // BF2_PLUGIN_ABI_NATIVE

// PE 0x10042CD0; [INFERRED] placement beside its only caller (the retail receiver is unused).
// Select bf2/xpack shaders from the map's night flag and remount on change or first use.
// Preserve Shaders_client.zip versus Shaders_Client.zip spelling and the unguarded
// SwiffHost singleton access.
bool selectShaderArchive(const char* levelName)
{
    bool changed = false;

    // var_54 - built before the null test and destroyed after it, so it is read even for a null
    // level name. settings vslot +0x40 with an empty default, exactly as readStringSettingDefault
    // spells that call.
    vc71::string modDirectory;
    readStringSettingDefault("GSModDirectory", "", modDirectory);

    if (levelName)
    {
        HostLevelMap  map;
        map.path = levelName;
        HostModRecord mod;

        IBase* const swiffHost = classManager
            ? classManager->getSingleton(renddx9abi::HostString("SwiffHost"))
            : 0;

        // +0x120 getModInfo, then +0x50 parseMapDesc(Mod*, const string& levelsPath, Map*).
        // Typed in the native flavour (RW-266): ISwiffHost slot 72 and ModInfo slot 20, whose Mod /
        // Map / std::string ARE the host's here; the retail-ABI flavour keeps both retail slots.
#if BF2_PLUGIN_ABI_NATIVE
        ModInfo* const modInfo = static_cast<ModInfo*>(static_cast<IModInfo*>(
            static_cast<ui::ISwiffHost*>(swiffHost)->getModInfo()));
#else
        typedef void* (__thiscall* GetModInfoFn)(void*);
        typedef bool  (__thiscall* ParseMapDescFn)(void*, HostModRecord*,
                                                   const renddx9abi::HostString&, HostLevelMap*);

        void* const modInfo = reinterpret_cast<GetModInfoFn>(
            (*reinterpret_cast<void***>(swiffHost))[0x120 / sizeof(void*)])(swiffHost);
#endif

        // var_70 = modDir; var_70 += "/levels/"; var_1C = var_70 - the PE copies it too.
        renddx9abi::HostString levelsPath(modDirectory.c_str());
        levelsPath += "/levels/";

#if BF2_PLUGIN_ABI_NATIVE
        modInfo->parseMapDesc(&mod, levelsPath, &map);                                 // +0x50
#else
        reinterpret_cast<ParseMapDescFn>(
            (*reinterpret_cast<void***>(modInfo))[0x50 / sizeof(void*)])(
                modInfo, &mod, levelsPath, &map);
#endif

        // var_38 - default-constructed at 0x10042DDC, before the branch, then assigned in each arm.
        renddx9abi::HostString archive;

        if (map.isNightMap)
        {
            changed = static_cast<Renderer*>(g_Renderer)->setExpansionPack(1);
            archive = "mods/xpack/Shaders_Client.zip";
        }
        else
        {
            changed = static_cast<Renderer*>(g_Renderer)->setExpansionPack(0);
            archive = "mods/bf2/Shaders_Client.zip";
        }

        if (changed || g_shaderArchiveMountPending)
        {
            fileManager->permitMountArchives(true);
            fileManager->unmountArchive(renddx9abi::HostString("Shaders_client.zip"));
            fileManager->mountArchive(archive, renddx9abi::HostString("Shaders"), true);
            fileManager->permitMountArchives(false);
            g_shaderArchiveMountPending = 0;

            ::renddx9abi::logf("Renderer: level '%s' -> expansionPack=%d, mounted %s at \"Shaders\" "
                               "(changed=%d)", levelName, map.isNightMap ? 1 : 0,
                               archive.c_str(), changed ? 1 : 0);
        }
    }

    return changed;
}

} // anonymous namespace

// clear geometry templates, select the shader archive, refresh shader
// state when required, then re-arm the template technique gates. Preserve this order:
// the next material load needs masks built against the newly mounted archive.
// GeometryTemplateManager calls are unguarded; initialize requires the singleton.
void Renderer::handleLevelPreLoad(const char* levelName)
{
    typedef void (__thiscall* VoidFn)(void*);

    if (m_weatherManager)
    {
        void** const vtbl = *reinterpret_cast<void***>(m_weatherManager);
        reinterpret_cast<VoidFn>(vtbl[0x14 / sizeof(void*)])(m_weatherManager);
    }
    if (m_rainManager)
    {
        void** const vtbl = *reinterpret_cast<void***>(m_rainManager);
        reinterpret_cast<VoidFn>(vtbl[0x14 / sizeof(void*)])(m_rainManager);
    }

    IBase* const gtm = geom::g_GeometryTemplateManager;
    static_cast<geom::IGeometryTemplateManager*>(gtm)->clear();

    if (selectShaderArchive(levelName) || g_shaderSystemReloadPending)
    {
        // `mov ecx, dword_1023B97C` with nothing pushed: the callee re-reads the base object from
        // dword_1023B970, which is g_rendererBase - the same Renderer this method is running on.
        updateShaderDefines(g_shaderManager, g_rendererBase);

        shaderManagerReloadEffects(g_shaderManager);
        // Reset is repeated inside the load pass. Keep both calls; loading registers
        // metashaders required before geometry technique masks are built.
        Ra::effectManagerResetEffects();
        Ra::raLoadAllShaders(static_cast<Ra::RaShaderTable*>(g_renderQueues), 3);

        g_shaderSystemReloadPending = 0;
    }

    reinterpret_cast<VoidFn>(
        (*reinterpret_cast<void***>(gtm))[0x48 / sizeof(void*)])(gtm);       // the technique re-arm
}
RD9_RENDERER_FRONTIER(shutdown,    0xD8)

#undef RD9_RENDERER_FRONTIER


// Creates a renderer interface; aggregation is unsupported.
// Factory ABI requires __fastcall (iid in ECX, outer in EDX).
// Allocation failure throws with this allocator; the original null-allocation path faults.

IBase* __fastcall createRenderer(unsigned int iid, IBase* outer)
{
    // "if (parent) return nullptr" - no aggregation. Read, not assumed.
    if (outer)
        return 0;

    Renderer* self = new Renderer();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}


// dice::hfe::dx9rend::DX9RendModule - vtable 0x101E2E80, object 8 bytes


RD9_MODULE_IBASE_BODIES(DX9RendModule)


// getClassList, 0x10051BB0. ONE class, and the whole body is four statements.

void DX9RendModule::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::dx9rend::DX9RendModule::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    ClassRegInfo info;
    info.classId = CID_RendDX9;
    info.name    = "dice.hfe.dx9rend.Renderer";
    info.create  = RD9_CLASS_FACTORY(&createRenderer);      // RefFactory_dx9rend_Renderer
    out.push_back(info);
}


// getSingletonList, 0x10051AE0. Two entries, and one line between them that looks like a mistake
// and is reproduced because it is the shipped behaviour.

void DX9RendModule::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::dx9rend::DX9RendModule::getSingletonList", 0x14);

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    {
        SingletonRegInfo info;
        info.name     = "Renderer";
        info.instance = &g_Renderer;
        info.iid      = IID_IRenderer;
        info.classId  = CID_RendDX9;
        info.flags    = 2;                  // create immediately, inside registerSingleton
        out.push_back(info);
    }

    // Registration has not constructed the renderer yet. Its base constructor replaces
    // this initial null value before consumers run.
    g_rendererBase = g_Renderer;

    {
        SingletonRegInfo info;
        info.name     = "PostProductionFrameWork";
        info.instance = &g_PostProductionFrameWork;
        info.iid      = 0;                  // nothing to look up - flags 4 publishes, never creates
        info.classId  = 0;
        info.flags    = 4;
        out.push_back(info);
    }
}

// Initializes the preallocated owner, including shader resources and event/device listeners.
void* postProductionFrameWorkOwnerCtor(void* mem);


// init, 0x100519B0.

bool DX9RendModule::init()
{
    RD9_TRACE("dice::hfe::dx9rend::DX9RendModule::init", 0x18);

    // 548 bytes, and the singleton published is the interior pointer owner + 8 (a subobject in a
    // three-interface object). The constructor at PostProductionFrameWorkOwner_ctor -
    // is recovered in PostProductionFrameWork.cpp; it installs the three vtables so BF2's virtual
    // calls through the singleton (the per-frame gas-mask blend among them) dispatch into recovered
    // bodies rather than faulting on a null vptr.
    void* owner = ::renddx9abi::engineAlloc(548);
    if (owner)
    {
        postProductionFrameWorkOwnerCtor(owner);
        g_PostProductionFrameWorkOwner = reinterpret_cast<IBase*>(owner);
        g_PostProductionFrameWork      = reinterpret_cast<IBase*>(static_cast<char*>(owner) + 8);
    }
    else
    {
        g_PostProductionFrameWorkOwner = 0;
        g_PostProductionFrameWork      = 0;
    }

    // Register bindings only for the matching native string/container ABI.
    // [FRONTIER] DX9RendModule::init still lacks the 20-byte auxiliary object.
#if BF2_PLUGIN_ABI_NATIVE
    registerRendererConsoleObjects();
#endif

    ::renddx9abi::logf("DX9RendModule::init: PostProductionFrameWork published at 0x%08X "
                       "(owner 0x%08X + 8)",
                       ::renddx9abi::addr(g_PostProductionFrameWork),
                       ::renddx9abi::addr(g_PostProductionFrameWorkOwner));

    return true;     // the body ends "return 1"
}

// Destroys the postproduction owner's listeners, resources and private containers.
void postProductionFrameWorkOwnerDetach(void* owner);


// close, the reverse of init.

bool DX9RendModule::close()
{
    RD9_TRACE("dice::hfe::dx9rend::DX9RendModule::close", 0x1C);

    // the list at 0x10241668 leaves the registry first (0x1007B6D0), before the
    // 20-byte object and the owner are torn down.
#if BF2_PLUGIN_ABI_NATIVE
    unregisterRendererConsoleObjects();
#endif

    if (g_PostProductionFrameWorkOwner)
    {
        // Destroy retained resources and listeners before releasing owner storage.
        postProductionFrameWorkOwnerDetach(g_PostProductionFrameWorkOwner);
        ::renddx9abi::engineFree(g_PostProductionFrameWorkOwner);
        g_PostProductionFrameWorkOwner = 0;
    }

    // g_PostProductionFrameWork IS NOT cleared BY THE original. close() nulls the owner and
    // leaves the published singleton pointing at freed memory. Reproduced - it is the shipped
    // behaviour and nothing reads it after unload.

    return true;     // the body ends "return 1"
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
