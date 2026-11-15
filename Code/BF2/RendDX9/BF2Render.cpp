// Path: attested by RendDX9.dll __FILE__ at 0x101E00A0 (initialize, line 437).
// Renderer implementation for RendDX9.dll, separate from Game/Main/BF2Render.cpp.
// Recovered from the PE; unread bodies remain named [FRONTIER] stops.
// [CHOSEN, not recovered] BF2_IMAGE_RENDDX9 isolates this image from recursive builds.
// [RETIRE-WHEN: RW-50] Remove the guard once build.cmd has a RendDX9 image manifest.

#ifdef BF2_IMAGE_RENDDX9
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../RendDX9Abi/NativeVtable.h"
#include "../Game/IEventListener.h"
#endif
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../RendDX9Abi/NativeVtable.h"
#include "RendDX9Base.h"
#endif

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/PluginModules.h"
#if !defined(_MSC_VER) || !defined(_M_IX86)
#include "../Math/PortableX87.h"
#endif
#include "../../RendDX9Abi/PluginEntry.h"    // dice::hfe::classManager, for hWndBf2TempSlt
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Game/Main/IVideoOptions.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"         // formatString - the frame-time overlay text
#include "../GeomMisc/Debug.h"
// The ECCore CEPreLoad hook hands two of the host's own records to ModInfo::parseMapDesc. In the
// native flavour the host IS this toolchain, so the recovered declarations are the right types; the
// retail flavour cannot use them (VC7.1 string/vector layouts) and mirrors them instead - see
// selectShaderArchive below.
#include "../Game/IModInfo.h"
#include "../IO/Console/Console.h"
#include "../Geom/XpLightManager.h"
#include "../Geom/HemiMapManager/HemiMapManager.h"
#include "../Geom/HemiMapManager/HemiMapDependencies.h"
#include "../Geom/Overgrowth/OverGrowth.h"
#include "../Geom/UndergrowthSystem.h"
#include "../Geom/WeatherManager.h"          // the typed storm calls (update, render, reset)
#include "../Geom/Rain/RainManager.h"         // the typed rain calls (update, render, reset)
#include "../Geom/ParticleSystem/ParticleSystemManager.h"
#include "../Geom/Terrain.h"
#include "../Geom/SkyDome.h"                 // the draw pass's RenderPathListener call
#include "../Geom/ObjectDrawer.h"
#include "../Geom/StaticMeshRenderer.h"          // the frame's culled-material reset
#include "../Geom/DecalSystem/DecalManager.h"    // typed update / render-path draw
#include "../Geom/RoadEditableRenderer.h"   // IRoadEditableRenderer - the typed drawAll below
#include "../RendMisc/IRenderer.h"
#include "../RendMisc/IRenderView.h"
#include "../Math/Frustum.h"
#include "RendDX9Math.h"                    // the RendDX9.dll Math copies
#include "../RendMisc/EnvMapSystem/EnvMapManager.h"
#include "../Scene/Object/ObjectManager.h"
// HM1: Renderer::setLods walks the geometry-template registry and the three mesh
// templates' live-instance sets; its object arm reads each object's geometry (IObject +0x38).
#include "../Geom/GeometryTemplateManager.h"
#include "../Geom/StaticMeshTemplate.h"
#include "../Geom/SkinnedMeshTemplate.h"
#include "../Geom/BundledMeshTemplate.h"
#include "../Geom/MeshTemplate.h"            // GeometryTemplate +0x08, the streaming touch
#include "../Geom/GeometryInstanceData.h"    // geometry +0x20 / +0x24, the streaming touch
#include "../Game/Objects/ObjectLayoutAccess.h"
#include "../Game/Objects/IObjectTemplate.h"  // the performance-analysis row names
#include "RendDX9Base.h"
#include "Texture.h"
#include "QueueProfiler.h"
#include "ResourceTracker.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "PostProductionFrameWork.h"
#include "../GeomMisc/FSQuadDrawer.h"        // the top-down capture's copy and the overlay draw
// RW-266: the host classes the frame loop dispatches into, typed - see hostCalls below.
// System, WindManager and PlayerManager carry no STL across the calls made on them, so both flavours
// include them; the Menu class, ISwiffHost and ModInfo are native-only (their calls carry the host's
// own STL types, which only the native flavour shares).
#include "../Core/System.h"
#include "../Scene/Object/WindManager.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Game/ModInfo.h"
#include "../Menu/BF2HudManager.h"
#include "../Menu/ISwiffHost.h"
#include "MovieRenderer.h"                  // base +0x3F4, the demo movie recorder
#include "../GeomMisc/DebugRenderer.h"    // geom::g_DebugRenderer, flushed by renderFrame
#include "../AI/AIDLL/AI/Main/BF2AI.h"    // ai::IAI - the "AI" singleton's isInitialized

#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <new>
#include <math.h>
#include <stddef.h>

namespace dice {
namespace hfe {
// GeomMisc - the frame-time overlay's collaborators: dword_10247C50 (the DebugTextWriter
// singleton) and the Font::Ported colour getter (font slot +0x34).
namespace geom {
extern IBase* g_DebugTextWriter;
unsigned fontPortedGetColour(IBase* font);
int fontPortedGetHeight(IBase* font);
void debugTextWriterSetColour(IBase* writer, unsigned colour);                      // slot +0x20
void debugTextWriterQueueText(IBase* writer, int x, int y, const std::string& text,
                              int alignment);                                      // slot +0x40
// GeomMisc/MenuRend/MemeTexture.cpp - a Meme::Texture's wrapped EngineTexture (+0x2C), the target
// of the top-down capture slot +0x60 replays.
class MemeTexture;
void* memeTextureEngineTexture(const MemeTexture* texture);
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
double systemExactTime()               { return g_system->getExactTime(); }            // +0x14
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
    static_cast<hud::BF2HudManager*>(hudManager)->setPaint(enabled != 0);
}

void hudUpdate(void* hudManager, float timeF, void* player, float x, float y, float z) // +0x28
{
    Vec3 look;
    look.x = x; look.y = y; look.z = z;
    static_cast<hud::BF2HudManager*>(hudManager)->update(
        timeF, static_cast<world::IPlayer*>(player), look);
}

void hudPaint(void* hudManager)                                                         // +0x1C
{
    static_cast<hud::BF2HudManager*>(hudManager)->paint();
}

// The streaming scan's frustum query, into the Renderer's result vector (retail: the VC7.1 vector
// at dwords 469..472 of the Renderer): the host's ObjectManager::getObjectsFromGridNoCullConst
// (BF2.exe 0x00727F20, recovered in Scene/Object/ObjectManager.cpp).
void objectManagerQueryFrustum(void* frustum488, std::vector<world::IObject*>& result)  // +0x5C
{
    // reinterpret_cast, as BundledMeshHeight.cpp reaches the same singleton: the published IBase*
    // is the object's start, which is what the raw dispatch assumed too.
    reinterpret_cast<world::ObjectManager*>(objectManager)->getObjectsFromGridNoCullConst(
        *static_cast<const Frustum*>(frustum488), result, 0, false, 0.0f, 0.0f);
}

} // namespace hostCalls

// Game/Objects/IControlPoint.h - the performance analysis keeps the interface pointer only. The
// interface id is Core/Ids.h's world::IID_IControlPoint (dword 0xC4D1 at .rdata 0x10208D24);
// that header also defines this file's IID_IRenderer pair, so the value is repeated here.
namespace world {
class IControlPoint;
const unsigned int kIID_IControlPoint = 50385u;
}

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
void skinnedMeshRendererSetEnabled(unsigned char enabled);
extern IBase* g_RoadCompiledRenderer;
extern IBase* g_RoadEditableRenderer;
extern IBase* g_GeometryTemplateManager;
extern IBase* g_Terrain;
// RoadCompiledRenderer.cpp - what roadQueuePass reads off the road renderer singleton: the fade pair
// (+0xB0/+0xB4) and the two depth-bias words (+0x90/+0x94) as their bit patterns.
const float* roadCompiledRendererFadeOut();
unsigned int roadCompiledRendererDepthBiasBits();
unsigned int roadCompiledRendererSlopeScaleDepthBiasBits();
float roadCompiledRendererViewDistanceScale();     // +0x8C, unguarded
void roadCompiledRendererSetViewDistanceScale(float value);
void roadCompiledRendererSetDepthBias(float value);
// The singleton's RenderPathListener subobject (retail +0x24), whose +0x0C renderFrame calls.
void* roadCompiledRendererListener();
}

namespace dx9rend {

// TextureManager.cpp / ResourceManager.cpp - the two calls Renderer::initialize step 16 makes.
void textureManagerSweep();
void resourceManagerSetReady(unsigned char value);

// ShaderManager.cpp - what vtable slot +0xC4 tail-calls into.
extern void* g_shaderManager;
void updateShaderDefines(void* shaderManager, void* base);

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
// RendererConsole.cpp - the `renderer` console object's list (the static at 0x10241668).
bool registerRendererConsoleObjects();
bool unregisterRendererConsoleObjects();
// RenderQueues.cpp - the mesh sort buckets and the withheld world-infrastructure globals.
void renderQueueFlush(unsigned int slotByteOffset, int flushArg);
extern void* g_renderQueues;
// RenderQueues.cpp - the queue-profiler registry walks behind onlyDrawPlacedObjects.
void queueProfilersSelectAll(unsigned char selected);
void queueProfilersSelectByPrefix(const std::string& prefix, unsigned char selected);
void queueProfilersSetEnabledOnSelected(unsigned char enabled);

namespace Ra {
// Ra/RaShader.h declares both of these; they are forward-declared here rather than pulling
// that header in, exactly as this file forward-declares the rest of its cross-TU seams.
struct RaShaderTable;
struct RaShader;     // as Geom/BundledMeshTemplate.h declares it
int  raLoadAllShaders(RaShaderTable* table, unsigned char flags);
RaShader* getShader(RaShaderTable* table, const std::string& name, bool skipFileCheck);
void effectManagerResetEffects();
}
extern void* g_renderEntryPool;
extern void* g_raShaderConstants;
// [CHOSEN, not recovered] The 3D water clock (B2BF/OpenBF2 issue 69): GlobalTime (+0x94) kept in
// double precision. Every GlobalTime accumulation in this file adds the same frame time to it, so
// Terrain::updateWaterConstants runs the 3D water at the retail rate without the float's rounding.
double g_waterClock = 0.0;
extern void* g_renderTargetSystem;
// QueryManager.cpp - the GPU-throttle fences.
void queryIssueEnd(void* query);
void queryWaitDone(void* query);
void queryReleaseRef(void* query);
// PostProductionFrameWork.cpp - the owner block's size and published interface subobject (548 and
// owner + 8 on x86), and sub_1004F1C0, the is-active predicate on the published owner.
unsigned int postProductionFrameWorkOwnerSize();
void* postProductionFrameWorkInterface(void* owner);
bool  postProductionFrameWorkIsActive();

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

// One control point's row of the performance analysis, a 0x34-byte VC7.1 vector element at
// Renderer +0x850. runPerformanceAnalysis pushes one row per object that answers IID_IControlPoint;
// updatePerformanceAnalysis samples the total queue profiler into the last row and reports it. In
// each triple, index 0 is the frame rate (profiler +0x1B8), index 1 the draw calls (counter 0),
// and index 2 the profiler word at +0x25C, which the report scales by 2^-20. The report header
// labels them FR, DC and SM. [INFERRED] type and member names.
struct PerformanceAnalysisRecord
{
    world::IObject*       object;        // +0x00  the control point; its template names the row
    world::IControlPoint* controlPoint;  // +0x04  the same object's IID_IControlPoint interface;
                                         //        written, never read
    float minimum[3];                    // +0x08  seeded 1.0e8
    float maximum[3];                    // +0x14  seeded 0
    float average[3];                    // +0x20  seeded 0; every sample adds value / sample count
    int   sample;                        // +0x2C  the sample index last taken
    float elapsed;                       // +0x30  accumulated frame time
};

class Renderer : public IRenderer
{
public:
    Renderer();
    ~Renderer();

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
    // [INFERRED] Source names; these are non-virtual overlay helpers.
    void drawStatisticsOverlay();                             // PE 0x1002E440
    // Unreachable in 1.50: no code or data references to the two setters below.
    void setStatisticsOverlayMode(int mode);                  // PE 0x1002A6A0
    void setUnreadWord6D8(int value);                         // PE 0x1002A6E0
    void drawStateStatisticsOverlay();                        // PE 0x1002EAE0
    void drawProfilerRow(int y, QueueProfiler* profile, bool heading); // PE 0x100290A0
    void drawTextColumn(int x, int y, const std::vector<std::string>& lines, int spacing); // PE 0x1002C590
    // [INFERRED] name and parameter names; drawObject is handed to ObjectDrawer +0x20 only when
    // the flags select world pass A without pass B.
    virtual bool renderScene(double time, const RenderFlags* flags,
                             void* drawObject);                 // +0x2C  0x1002BC50
    virtual bool slot_0x30();                                   // +0x30  0x1002F770
    virtual bool slot_0x34(void* drawObject, const RenderFlags* flags);   // +0x34  0x1002D040
    virtual void slot_0x38();                                   // +0x38  0x1002A4A0
    virtual void* slot_0x3C(const DisplayModeFilter* filter);   // 0x10007420   build+return the display-mode list (retn 4)
    virtual bool slot_0x40(unsigned int index);                 // +0x40  0x10005C70
    virtual bool setRenderView(IBase* view);                    // +0x44
    virtual IRenderView* getRenderView();                       // +0x48
    virtual bool setRenderViewDup(IBase* view);                  // +0x4C
    virtual IRenderView* getRenderViewDup();                    // +0x50
                                                                //        (ICF-folded twin of +0x48)
    virtual unsigned int getFrameOrdinal();                      // +0x54
    // Draws the open in-game console.
    virtual void slot_0x58();
    virtual bool slot_0x5C(const std::string& name, void* bitmap, bool reupload,
                           bool alphaToGrey, unsigned int* width,
                           unsigned int* height);              // +0x5C  0x10005010
    virtual void slot_0x60(unsigned int size, void* target, float x, float y, float z,
                           float orthoExtent, bool drawObjects); // +0x60  0x1002C820
    virtual void slot_0x64(const DemoConfig* config);           // +0x64  0x10004FC0
    virtual void slot_0x68();                                   // +0x68  0x100051D0
    virtual void* getRenderStats();                             // 0x101432E0  return this + 0x108
    virtual bool slot_0x70();                                   // +0x70  0x10032700
    virtual IVideoOptions* getVideoOptions();                    // +0x74  0x10005000   return this+1016
    virtual void slot_0x78(float value);                        // +0x78  0x100326E0
    virtual float slot_0x7C();                                  // +0x7C  0x100326F0
    virtual void slot_0x80(float value);                        // +0x80  0x100285A0
    virtual float slot_0x84();                                  // +0x84  0x100285C0
    virtual void slot_0x88(float value);                        // +0x88  0x100285E0
    virtual float slot_0x8C();                                  // +0x8C  0x10028600
    virtual void slot_0x90(bool enabled);                       // +0x90  0x10028620
    virtual bool slot_0x94();                                   // +0x94  0x10028640
    virtual void slot_0x98(bool value);                         // +0x98  0x10028660
    virtual bool slot_0x9C();                                   // +0x9C  0x10028680
    // [INFERRED] spelling; exact binary64 carrier and measured-FPS role are recovered.
    virtual double getFrameRate();                              // +0xA0
    virtual void setRenderAheadLimit(unsigned int limit);       // +0xA4
    virtual unsigned int getRenderAheadLimit();                 // +0xA8
    virtual bool isExpansionPack();                             // +0xAC  0x10032740   read
    virtual unsigned int slot_0xB0();                           // +0xB0
    virtual void* destroy(unsigned int flags);                 // +0xB4  0x10032C80
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
    virtual bool getDesktopMode(unsigned int& width, unsigned int& height,
                                unsigned int& refreshHz);       // +0xDC [CHOSEN] port addition

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

    // The texture name at +0x6E0 that rendererReloadOverlayTexture loads into +0x8D8.
    const std::string& overlayTextureName() const { return m_overlayTextureName; }
    void setNightMode(unsigned int mode);
    void setFakeHDRBlendingTimeLight(double value);
    void setFakeHDRBlendingTimeDark(double value);
    void setFakeHDRBlendingFactor(float value);

    // Members whose retail copies have no caller in 1.50 (xpLightBlendTick carries
    // sampleShadowUnderView inline). The debug image's console commands resetFpsHistogram,
    // printFpsHistogram, onlyDrawPlacedObjects and runPerformanceAnalysis reach their copies, its
    // overlays call drawText, and its update step opens a "updatePerformanceAnalysis" profiler
    // scope. [INFERRED] names.
    void drawHUD();                                             // 0x10028090
    void resetFpsHistogram();                                   // 0x100280B0
    std::string getFpsHistogram();                              // 0x100280F0
    void drawText(int x, int y, const std::string& text);       // 0x10028250
    void onlyDrawPlacedObjects(bool enabled);                   // 0x10028460
    float sampleShadowUnderView(IRenderView* view);             // 0x10028780
    Vec4 scaleByViewDistance(const Vec4& value);                // 0x1002A710
    void updatePerformanceAnalysis(float deltaTime);            // 0x1002DB80
    void runPerformanceAnalysis(int sampleCount, float interval); // 0x1002E120

    // The per-pass render switch byte at retail `retailOffset` (+0x69C..+0x6AB, +0x6B0..+0x6B7);
    // a null result names no switch. Recovered bodies spell these as decimal byte indices.
    unsigned char* renderSwitch(unsigned int retailOffset);
    unsigned char& sw(unsigned int retailOffset);

    // [CHOSEN, not recovered] The tail field that sits at `retailOffset` in the retail x86 layout,
    // when it is a mapped field of `bytes` bytes and the given kind (issue #7); null otherwise.
    enum TailKind { TailByte, TailInt, TailFloat, TailDouble };
    void* tailField(unsigned int retailOffset, TailKind kind, unsigned int bytes);

private:
    // Step 6 factors the two identical debug-text-writer calls; see its definition.
    void setDebugTextWriterFont(const char* singletonName, const char* fontPath);
    void initializeFontAndConsoleOverlay();   // BF2Render_initializeFontAndConsoleOverlay 0x1002BA40

    // The frame-loop helpers, all
    void hudManagerFramePrep(float timeF);
    void geometryStreamTick();                // sub_1002DF20 - the streaming scan (budget 25.0)
    void roadQueuePass();                     // sub_1002A560 - terrain-gated road queue flush
    void roadScenePass();                     // sub_10028330 - renderScene's road queue flush
    bool xpLightBlendTick();                  // sub_1002B8E0 - renderFrame's xpack light step

public:
    // The data members are public inside this translation unit: the hand-built listener thunks
    // and the owner-TU seams below reach them by name. [CHOSEN, not recovered] (issue #7): every
    // field the recovered bodies read or write is a named member. The x86 object keeps the retail
    // layout (the offsets below, asserted after the class); the x64 object is native, which is
    // why no code may reach these fields by raw byte offset - other TUs use the RendDX9Base.h
    // accessors (rendererTail*, rendererRenderSwitch, ...), which map a retail offset to a member.
    // Unread retail ranges stay opaque byte arrays and are zeroed with everything else.

    // The base occupies +0x000..+0x537 and is accessed through RendDX9Base's overlay; constructD3DHalf
    // constructs its std containers in place and destructD3DHalf destroys them. The primary vptr is
    // already declared, so storage starts at +0x004. The native RendDX9Base fits the retail 0x538
    // bytes on x86; a larger one (x64) grows the storage instead of overflowing it.
    char         m_baseStorage[(sizeof(RendDX9Base) > 0x538 ? sizeof(RendDX9Base) : 0x538)
                               - sizeof(void*)];   // +0x004 .. +0x537
    void*        m_evHandlerVptr;        // +0x538  ECCoreEventHandler       - kECCoreEventHandlerVtable
    void*        m_deviceLostVptr;       // +0x53C  DeviceLostResetListener  - kRendererDeviceLostVtable
    void*        m_renderPathVptr;       // +0x540  RenderPathListener       - kRendererRenderPathVtable
    // The render-path listener's own word (listener + 4): renderer_renderPathActivated stores the
    // path, renderer_getCurrentRenderPath returns it. The ctor does not write it.
    int          m_currentRenderPath;    // +0x544
    int          m_refCount;             // +0x548

    // A 4x4 matrix at dword indices 339..354. The constructor lays an identity into it twice (once
    // in the run of zeros, once at the very end), and initialize both writes its translation row
    // and hands the whole thing to RenderView::setViewMatrix - which is what identifies it.
    float        m_viewMatrix[16];       // +0x54C .. +0x58B
    float        m_f58C;                 // +0x58C  (355) ctor: 1.0; divides the path-target size
    float        m_f590;                 // +0x590  (356) ctor: 0x3D27EF9E; both views' near plane
    float        m_near;                 // +0x594  (357) ctor: 0.01; the second view's near plane
    float        m_far;                  // +0x598  (358) ctor: 15.0
    float        m_f59C;                 // +0x59C  (359) ctor: 6.0
    unsigned char m_clearDepthBeforeFirstPerson;   // +0x5A0  (byte 1440) ctor: 1
    unsigned char m_postProductionNeedsActive;     // +0x5A1  (byte 1441) ctor: 1
    unsigned char m_pad5A2[2];
    // The GPU-throttle fence ring: five D3DQUERYTYPE_EVENT queries (dwords 361..365), the ring
    // size in use (366, slot +0xA4/+0xA8's renderAheadLimit) and the cursor (367).
    void*        m_frameFences[5];       // +0x5A4 .. +0x5B7
    unsigned int m_renderAheadLimit;     // +0x5B8  (366) ctor 2, max 5
    unsigned int m_fenceCursor;          // +0x5BC  (367)
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
    // The 48-byte QPC frame timer (sub_10077710); the mesh fades read its total time at +0x20.
    void*        m_frameTimer;           // +0x5DC  (375)
    unsigned char m_tail5E0[0x5E8 - 0x5E0];        // +0x5E0 [UNRESOLVED] no Renderer-relative access
    // The frame's time argument, stored by update and by renderFrame (double 189).
    double       m_frameTime;            // +0x5E8
    unsigned char m_tail5F0[0x5F8 - 0x5F0];        // +0x5F0 [UNRESOLVED] no Renderer-relative access
    // slot +0x38's clock mark: the System +0x14 time of its previous HUD-only frame.
    double       m_hudFrameTime;         // +0x5F8
    double       m_frameRate;            // +0x600  measured FPS
    double       m_fpsAccumulatedTime;   // +0x608  (double 193)
    unsigned int m_fpsAccumulatedFrames; // +0x610  (388)
    unsigned char m_tail614[0x618 - 0x614];        // +0x614 [UNRESOLVED] no Renderer-relative access
    // sub_1002E440 (the +0x6D4 overlay) stores each frame's time (+0x5E8, as float) at
    // +0x618 + 4 * index and steps the index modulo 32 (`and ecx, 8000001Fh`); the constructor
    // zeroes the index (0x1002D63C).
    float        m_frameTimeHistory[32]; // +0x618 .. +0x697
    int          m_frameTimeHistoryIndex;// +0x698
    // The per-pass render switches (bytes 1692..1707 and 1712..1719); 0x6AC between them is a
    // dword. See renderSwitch.
    unsigned char m_renderSwitches69C[0x6AC - 0x69C];  // +0x69C .. +0x6AB
    unsigned int m_frameTimeOverlay;     // +0x6AC  (427) gates sub_10029C50; the hemi bake clears it
    unsigned char m_renderSwitches6B0[0x6B8 - 0x6B0];  // +0x6B0 .. +0x6B7
    // 0x1002A780 (the night-vision environment snapshot): zero means no snapshot is held; a
    // non-zero request is stored here, and a zero one restores the snapshot and clears it.
    // 0x100286B0 returns it. [INFERRED] name.
    int          m_nightVisionStage;     // +0x6B8
    unsigned char m_fullSizeRenderTargets;         // +0x6BC
    unsigned char m_pad6BD[3];
    // The fog override triple the fog block swaps into the host ObjectManager (floats 432..434,
    // ctor 4, 5, 100; VideoOptions::apply rewrites them; the hemi bake treats +0x6C0 as a dword).
    float        m_fogOverride[3];       // +0x6C0 .. +0x6CB
    unsigned int m_raPath;               // +0x6CC  (435) the RAPATH shader define / quality index
    unsigned char m_byte6D0;             // +0x6D0  [UNRESOLVED] read by the static-mesh draw
    unsigned char m_pad6D1[3];
    // Statistics overlay mode, 0..4 (setter 0x1002A6A0 clamps it and mirrors "non-zero" into
    // byte_1023E7F0); gates sub_1002E440.
    unsigned int m_dword6D4;             // +0x6D4  (437)
    // [UNRESOLVED] role: the only writers are the constructor's zero and 0x1002A6E0, an
    // unreferenced setter that clamps its argument to 0..2; no instruction reads it.
    int          m_dword6D8;             // +0x6D8
    unsigned char m_byte6DC;             // +0x6DC  (1756) ctor 1; makeTopWorldScreen's particles flag
    unsigned char m_byte6DD;             // +0x6DD  (1757) ctor 0; gates the overlay draw sub_10028FD0
    unsigned char m_pad6DE[2];
    // The two std::string members sit in retail 28-byte VC7.1 string slots, padded back up to
    // their slot so the x86 tail keeps its retail offsets.
    std::string m_overlayTextureName;                     // +0x6E0 (dword 440) [INFERRED] name
    char         m_overlayTextureNamePad[sizeof(std::string) < 0x1C
                                         ? 0x1C - sizeof(std::string) : 1];
    // The overlay draw's scale and offset (sub_10028FD0: x = +0x700, y = -+0x704).
    float        m_f6FC[3];              // +0x6FC .. +0x707 (447..449) ctor 0.125, 0.75, 0.75
    unsigned char m_meshShaderLodEnabled;// +0x708  the mesh draws' shader-LOD gate
    unsigned char m_pad709[3];
    // Three four-entry distance tables (451..462, ctor [0] = 10.0): the static-mesh draw reads
    // +0x70C + 4 * bucket, the bundled-mesh LOD +0x72C + 4 * i.
    float        m_lodDistances70C[4];   // +0x70C
    float        m_lodDistances71C[4];   // +0x71C
    float        m_lodDistances72C[4];   // +0x72C
    float        m_f73C;                 // +0x73C  (463) ctor 9999; PostProductionFrameWork +0x54
    float        m_f740;                 // +0x740  (464) ctor 9999; PostProductionFrameWork +0x58
    float        m_streamBudget;         // +0x744  (465) ctor 25; the geometry-streaming budget
    unsigned int m_dword748;             // +0x748  (466) ctor 10
    unsigned int m_dword74C;             // +0x74C  (467) the streaming scan zeroes it
    unsigned int m_dword750;             // +0x750  (468) the streaming scan zeroes it
    unsigned int m_streamObjectsAllocator;  // +0x754 the retail vector's allocator word (unused)
    // The streaming scan's frustum-query result (retail dwords 470..472).
    std::vector<world::IObject*> m_streamObjects;          // +0x758
    unsigned char m_expansionPack;       // +0x764  isExpansionPack / setExpansionPack
    unsigned char m_pad765[3];
    unsigned int m_dword768;             // +0x768  (474) ctor 2; slot +0xB0 returns it
    IDirect3DSurface9* m_backBuffer;     // +0x76C  the device back buffer reference
    IDirect3DSurface9* m_depthStencil;   // +0x770  the depth-stencil reference
    EngineTexture* m_hemiScratchTexture; // +0x774  accumulation/Hemi scratch target
    // The render-path targets: +0x778..+0x7C8 (+0x788 full size, +0x7A0 a quarter-height strip,
    // +0x7A4..+0x7C8 quarter size), then +0x7CC/+0x7D0/+0x7D4.
    EngineTexture* m_pathTargets[(0x7D8 - 0x778) / 4];      // +0x778 .. +0x7D7
    // The forced static/skinned/bundled mesh LODs (502..504, ctor -1).
    int          m_forcedLod[3];         // +0x7D8 / +0x7DC / +0x7E0
    int          m_lodSkipBase;          // +0x7E4  the mesh templates' LOD-skip base
    unsigned int m_dword7E8;             // +0x7E8  (506) ctor 100
    int          m_lodSkipBias;          // +0x7EC  the mesh templates' LOD-skip bias
    // Slot +0x60's stored argument set (+0x7F4..+0x80C) and the byte it arms (+0x7F0), which
    // beginFrame tests before replaying the set through slot +0x60(0, ...).
    unsigned char m_frameHookEnabled;    // +0x7F0
    unsigned char m_pad7F1[3];
    unsigned int m_frameHookA;           // +0x7F4  the capture size
    void*        m_frameHookB;           // +0x7F8  the capture target ([INFERRED] a Meme::Texture)
    float        m_frameHookVec[3];      // +0x7FC .. +0x807  the capture position
    float        m_frameHookC;           // +0x808  the orthographic extent
    unsigned char m_frameHookD;          // +0x80C  the objects switch (+0x69D) during the capture
    // renderScene's colour-correction latch: set while the saved PostProduction state vector
    // (+0x810, PostProductionFrameWork +0x28/+0x24) is the live one. [INFERRED] names.
    unsigned char m_colourCorrectionLive;// +0x80D
    unsigned char m_pad80E[2];
    float        m_savedColourCorrection[4];   // +0x810 .. +0x81F
    float        m_fpsBucketScale;       // +0x820  (520) ctor 10
    unsigned int m_fpsHistogram[10];     // +0x824 .. +0x84B (521..530)
    float        m_lodScale84C;          // +0x84C  (531) ctor 1; every mesh LOD distance scale
    // The performance analysis: a VC7.1 vector of 0x34-byte control-point rows (+0x850
    // allocator, +0x854 begin, +0x858 end, +0x85C capacity), which runPerformanceAnalysis rebuilds
    // and updatePerformanceAnalysis consumes from the back. The destructor frees it at 0x1002DB05.
    unsigned int m_performanceRecordsAllocator;  // +0x850 the retail vector's allocator word (unused)
    std::vector<PerformanceAnalysisRecord> m_performanceRecords;   // +0x854
    unsigned int m_dword860;             // +0x860  (536) ctor 8; the analysis' samples per row
    float        m_f864;                 // +0x864  (537) ctor 2; the analysis' seconds per sample
    // The performance analysis report: the header, then one line per finished row; written to the
    // console when the last row finishes.
    std::string m_string868;                              // +0x868 (dword 538)
    char         m_string868Pad[sizeof(std::string) < 0x1C ? 0x1C - sizeof(std::string) : 1];
    void*        m_lazy884;              // +0x884  (545) ctor: 0; initialize lazily allocates it
    // The ECCore performance-test pair (ids 10/11, the handler's +0x350/+0x354/+0x358).
    unsigned char m_performanceTestActive;  // +0x888
    unsigned char m_pad889[3];
    unsigned int m_performanceTest88C;   // +0x88C
    float        m_performanceTest890;   // +0x890  renderFrame's `fadd`/`fstp dword` accumulator
    // Graph slots must match frame rendering: framerate, render time, then draw calls.
    void*        m_debugGraphs[3];       // +0x894 / +0x898 / +0x89C
    // +0x8A0 / +0x8A8, dbl[276] and dbl[277]. The frame bracket: slot_0x20 stamps the start, and
    // renderFrame's tail subtracts it from the same clock to get the frame's wall time, which is
    // then the sample fed to the Rendering-time graph.
    double       m_frameStartTime;       // +0x8A0
    double       m_frameWallTime;        // +0x8A8
    // Per-family LOD distance scales (556..558, ctor 1): static, bundled, skinned mesh.
    float        m_lodScale8B0;          // +0x8B0
    float        m_lodScale8B4;          // +0x8B4
    float        m_lodScale8B8;          // +0x8B8
    // The expansion-pack light blend sub_1002B8E0 steps each frame: the blended shadow level,
    // the rise and fall durations (set by 0x100286F0 / 0x10028720 and the level setup at
    // 0x1002B6E6), and the clock mark of the previous step. [INFERRED] names.
    float        m_xpLightLevel;         // +0x8BC
    double       m_xpLightRiseTime;      // +0x8C0
    double       m_xpLightFallTime;      // +0x8C8
    double       m_xpLightMarkTime;      // +0x8D0
    EngineTexture* m_overlayTexture;     // +0x8D8  rendererReloadOverlayTexture's texture
    unsigned char m_tail8DC[0x8E0 - 0x8DC];        // +0x8DC [UNRESOLVED] no Renderer-relative access
};

// The retail x86 layout, member by member. The x64 Renderer is native: nothing may depend on
// these offsets there (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(Renderer) == 0x8E0,
              "RefFactory_dx9rend_Renderer allocates 0x8E0 - see 0x10051940");
static_assert(offsetof(Renderer, m_evHandlerVptr)        == 0x538, "+0x538");
static_assert(offsetof(Renderer, m_deviceLostVptr)       == 0x53C, "+0x53C");
static_assert(offsetof(Renderer, m_renderPathVptr)       == 0x540, "+0x540");
static_assert(offsetof(Renderer, m_currentRenderPath)    == 0x544, "+0x544");
static_assert(offsetof(Renderer, m_refCount)             == 0x548, "+0x548");
static_assert(offsetof(Renderer, m_viewMatrix)           == 0x54C, "+0x54C");
static_assert(offsetof(Renderer, m_f58C)                 == 0x58C, "+0x58C");
static_assert(offsetof(Renderer, m_f59C)                 == 0x59C, "+0x59C");
static_assert(offsetof(Renderer, m_clearDepthBeforeFirstPerson) == 0x5A0, "+0x5A0");
static_assert(offsetof(Renderer, m_frameFences)          == 0x5A4, "+0x5A4");
static_assert(offsetof(Renderer, m_renderAheadLimit)     == 0x5B8, "+0x5B8");
static_assert(offsetof(Renderer, m_fenceCursor)          == 0x5BC, "+0x5BC");
static_assert(offsetof(Renderer, m_objectDrawer)         == 0x5C0, "+0x5C0");
static_assert(offsetof(Renderer, m_consoleOverlay)       == 0x5C8, "+0x5C8");
static_assert(offsetof(Renderer, m_hudManager)           == 0x5D0, "+0x5D0");
static_assert(offsetof(Renderer, m_frameTimer)           == 0x5DC, "+0x5DC");
static_assert(offsetof(Renderer, m_frameTime)            == 0x5E8, "+0x5E8");
static_assert(offsetof(Renderer, m_frameRate)            == 0x600, "+0x600");
static_assert(offsetof(Renderer, m_fpsAccumulatedTime)   == 0x608, "+0x608");
static_assert(offsetof(Renderer, m_fpsAccumulatedFrames) == 0x610, "+0x610");
static_assert(offsetof(Renderer, m_frameTimeHistory)     == 0x618, "+0x618");
static_assert(offsetof(Renderer, m_frameTimeHistoryIndex) == 0x698, "+0x698");
static_assert(offsetof(Renderer, m_renderSwitches69C)    == 0x69C, "+0x69C");
static_assert(offsetof(Renderer, m_nightVisionStage)     == 0x6B8, "+0x6B8");
static_assert(offsetof(Renderer, m_dword6D8)             == 0x6D8, "+0x6D8");
static_assert(offsetof(Renderer, m_frameTimeOverlay)     == 0x6AC, "+0x6AC");
static_assert(offsetof(Renderer, m_renderSwitches6B0)    == 0x6B0, "+0x6B0");
static_assert(offsetof(Renderer, m_fullSizeRenderTargets) == 0x6BC, "+0x6BC");
static_assert(offsetof(Renderer, m_fogOverride)          == 0x6C0, "+0x6C0");
static_assert(offsetof(Renderer, m_raPath)               == 0x6CC, "+0x6CC");
static_assert(offsetof(Renderer, m_byte6D0)              == 0x6D0, "+0x6D0");
static_assert(offsetof(Renderer, m_dword6D4)             == 0x6D4, "+0x6D4");
static_assert(offsetof(Renderer, m_byte6DC)              == 0x6DC, "+0x6DC");
static_assert(offsetof(Renderer, m_overlayTextureName)   == 0x6E0, "+0x6E0");
static_assert(offsetof(Renderer, m_f6FC)                 == 0x6FC, "+0x6FC");
static_assert(offsetof(Renderer, m_meshShaderLodEnabled) == 0x708, "+0x708");
static_assert(offsetof(Renderer, m_lodDistances70C)      == 0x70C, "+0x70C");
static_assert(offsetof(Renderer, m_lodDistances71C)      == 0x71C, "+0x71C");
static_assert(offsetof(Renderer, m_lodDistances72C)      == 0x72C, "+0x72C");
static_assert(offsetof(Renderer, m_f73C)                 == 0x73C, "+0x73C");
static_assert(offsetof(Renderer, m_streamBudget)         == 0x744, "+0x744");
static_assert(offsetof(Renderer, m_dword748)             == 0x748, "+0x748");
static_assert(offsetof(Renderer, m_streamObjectsAllocator) == 0x754, "+0x754");
static_assert(offsetof(Renderer, m_streamObjects)        == 0x758, "+0x758");
static_assert(offsetof(Renderer, m_expansionPack)        == 0x764, "+0x764");
static_assert(offsetof(Renderer, m_dword768)             == 0x768, "+0x768");
static_assert(offsetof(Renderer, m_backBuffer)           == 0x76C, "+0x76C");
static_assert(offsetof(Renderer, m_depthStencil)         == 0x770, "+0x770");
static_assert(offsetof(Renderer, m_hemiScratchTexture)   == 0x774, "+0x774");
static_assert(offsetof(Renderer, m_pathTargets)          == 0x778, "+0x778");
static_assert(offsetof(Renderer, m_forcedLod)            == 0x7D8, "+0x7D8");
static_assert(offsetof(Renderer, m_lodSkipBase)          == 0x7E4, "+0x7E4");
static_assert(offsetof(Renderer, m_dword7E8)             == 0x7E8, "+0x7E8");
static_assert(offsetof(Renderer, m_lodSkipBias)          == 0x7EC, "+0x7EC");
static_assert(offsetof(Renderer, m_frameHookEnabled)     == 0x7F0, "+0x7F0");
static_assert(offsetof(Renderer, m_frameHookA)           == 0x7F4, "+0x7F4");
static_assert(offsetof(Renderer, m_frameHookVec)         == 0x7FC, "+0x7FC");
static_assert(offsetof(Renderer, m_frameHookC)           == 0x808, "+0x808");
static_assert(offsetof(Renderer, m_frameHookD)           == 0x80C, "+0x80C");
static_assert(offsetof(Renderer, m_colourCorrectionLive) == 0x80D, "+0x80D");
static_assert(offsetof(Renderer, m_savedColourCorrection) == 0x810, "+0x810");
static_assert(offsetof(Renderer, m_hudFrameTime)         == 0x5F8, "+0x5F8");
static_assert(offsetof(Renderer, m_fpsBucketScale)       == 0x820, "+0x820");
static_assert(offsetof(Renderer, m_fpsHistogram)         == 0x824, "+0x824");
static_assert(offsetof(Renderer, m_lodScale84C)          == 0x84C, "+0x84C");
static_assert(offsetof(Renderer, m_performanceRecordsAllocator) == 0x850, "+0x850");
static_assert(offsetof(Renderer, m_performanceRecords)   == 0x854, "+0x854");
static_assert(sizeof(PerformanceAnalysisRecord)         == 0x34, "0x34-byte rows (imul 0x4EC4EC4F, sar 4)");
static_assert(offsetof(Renderer, m_dword860)             == 0x860, "+0x860");
static_assert(offsetof(Renderer, m_f864)                 == 0x864, "+0x864");
static_assert(offsetof(Renderer, m_string868)            == 0x868, "+0x868");
static_assert(offsetof(Renderer, m_lazy884)              == 0x884, "+0x884");
static_assert(offsetof(Renderer, m_performanceTestActive) == 0x888, "+0x888");
static_assert(offsetof(Renderer, m_performanceTest890)   == 0x890, "+0x890");
static_assert(offsetof(Renderer, m_debugGraphs)          == 0x894, "+0x894");
static_assert(offsetof(Renderer, m_frameStartTime)       == 0x8A0, "+0x8A0");
static_assert(offsetof(Renderer, m_frameWallTime)        == 0x8A8, "+0x8A8");
static_assert(offsetof(Renderer, m_lodScale8B0)          == 0x8B0, "+0x8B0");
static_assert(offsetof(Renderer, m_lodScale8B8)          == 0x8B8, "+0x8B8");
static_assert(offsetof(Renderer, m_xpLightLevel)         == 0x8BC, "+0x8BC");
static_assert(offsetof(Renderer, m_xpLightRiseTime)      == 0x8C0, "+0x8C0");
static_assert(offsetof(Renderer, m_xpLightMarkTime)      == 0x8D0, "+0x8D0");
static_assert(offsetof(Renderer, m_overlayTexture)       == 0x8D8, "+0x8D8");
#endif


// The globals the registration records point at. Both are `IBase` slots in a SingletonRegInfo, so
// ClassManager writes through them - which is exactly how the renderer image learns its own
// Renderer pointer without ever calling getSingleton for it.

IBase* g_Renderer                     = 0;   // ctor writes it, registration republishes
IBase* g_PostProductionFrameWork      = 0;   // published by DX9RendModule::init, flags == 4
IBase* g_PostProductionFrameWorkOwner = 0;   // the owner block; the singleton is its interface subobject

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

// The three listener subobjects back to their Renderer: retail `lea ecx, [esi-538h]` and the
// +0x53C / +0x540 twins. offsetof keeps the x86 adjustment and is the native one on x64.
inline Renderer* rendererFromEventHandler(void* subobject)
{
    return reinterpret_cast<Renderer*>(static_cast<unsigned char*>(subobject) -
                                       offsetof(Renderer, m_evHandlerVptr));
}

inline Renderer* rendererFromDeviceLostListener(void* subobject)
{
    return reinterpret_cast<Renderer*>(static_cast<unsigned char*>(subobject) -
                                       offsetof(Renderer, m_deviceLostVptr));
}

inline Renderer* rendererFromRenderPathListener(void* subobject)
{
    return reinterpret_cast<Renderer*>(static_cast<unsigned char*>(subobject) -
                                       offsetof(Renderer, m_renderPathVptr));
}


// PE 0x1002F440.
// MSVC will not let a free function be __thiscall, so the ECX/EDX pair is spelled out: __fastcall
// with an unused second parameter is byte-for-byte the same calling convention for a slot like
// this one, whose arguments are all stack-passed after `this`.
void RD9_THUNK renderer_handleECCoreEvent(void* self RD9_THUNK_EDX,
                                           int category, unsigned int id,
                                           void* node, void* userData)
{
    (void)node;                                     // arg_8 - the EventNode; unread by this body

    if (category != 5)                              // cmp [ebp+arg_0], 5 / jnz - ECCore only
        return;

    // `lea ecx, [esi-538h]` - the handler's own `this` is the Renderer's +0x538 subobject.
    Renderer* const renderer = rendererFromEventHandler(self);

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
            reinterpret_cast<SlotFn>(vtbl[0x44 / 4])(geom::g_NametagManager);
        }

        // 1002F469 ->same optional guard, now backed by the real reflection owner.
        if (Rend::g_EnvMapManager)
            static_cast<Rend::EnvMapManager*>(Rend::g_EnvMapManager)->onPostLoad();
        return;
    }

    // id 9, CECollectSystemInfo (PE 0x1002F5A6..0x1002F70F): the payload is the host's
    // std::vector<std::pair<std::string, std::string> >; push a "- BF2Render -" header row, then one
    // row per video-quality setting naming its level (getQuality, Renderer +0x74 / VideoOptions
    // +0x04). sub_10030C00 / sub_10032DB0 / sub_100198A0 are the pair constructor, the vector's
    // push_back and the pair destructor. Host and renderer share one STL in every flavour
    // (RendDX9Abi/PluginAbi.h, issue #7), so the rows are the host's own type.
    case 9:
    {
        typedef std::vector<std::pair<std::string, std::string> > InfoRows;
        InfoRows& rows = *static_cast<InfoRows*>(userData);
        {
            const std::string empty("");
            const std::string header("- BF2Render -");
            rows.push_back(std::make_pair(header, empty));
        }
        static const char* const kSettingNames[8] =
        {
            "Terrain", "Geometry", "Lighting", "DynamicLighting",
            "DynamicShadows", "Effects", "Texture", "TextureFiltering"
        };
        static const char* const kLevelNames[5] = { "Off", "Low", "Medium", "High", "NotSet" };
        const std::string unused;
        IVideoOptions* const options = renderer->getVideoOptions();                    // +0x74
        for (unsigned int i = 0; i < 8; ++i)
        {
            const int level = options->getQuality(static_cast<int>(i));                // +0x04
            const std::string levelName(kLevelNames[level]);
            const std::string settingName(kSettingNames[i]);
            rows.push_back(std::make_pair(settingName, levelName));
        }
        return;
    }

    // id 10, CEStartPerformanceTest / id 11, CEStopPerformanceTest
    // Pure field writes on the subobject (esi+0x350/+0x354/+0x358 == Renderer +0x888/+0x88C/
    // +0x890), in the shipped store order.
    case 10:
        renderer->m_performanceTest88C = 0;
        renderer->m_performanceTest890 = 0.0f;      // `mov [esi+358h], edi` - all-zero bits
        renderer->m_performanceTestActive = 1;
        return;

    case 11:
        renderer->m_performanceTestActive = 0;
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
            reinterpret_cast<SlotFn>((*reinterpret_cast<void***>(envMap))[0x38 / 4])(
                envMap);
        }

        IBase* const hemiMap = classManager->getSingleton(renddx9abi::HostString("HemiMapManager"));
        if (hemiMap)
        {
            // +0x10 is IHemiMapManager::clear(), which IS written - Geom/HemiMapManager.
            typedef void (__thiscall* SlotFn)(void*);
            reinterpret_cast<SlotFn>((*reinterpret_cast<void***>(hemiMap))[0x10 / 4])(
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
        reinterpret_cast<RendDX9Base*>(renderer)->appActive =
            *static_cast<const unsigned char*>(userData);
        return;

    default:
        // The shipped switch has no default arm; every other id falls straight out.
        return;
    }
}

// The one-slot table itself, now with its body behind it.
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
void* const kECCoreEventHandlerVtable[] =
{
    BF2_NATIVE_SLOT(::dice::hfe::IEventListener, handleEvent, renderer_handleECCoreEvent),
};
#else
void* const kECCoreEventHandlerVtable[1] =
{
    reinterpret_cast<void*>(&renderer_handleECCoreEvent)
};
#endif

// The render-path target at retail `retailOffset` (+0x778..+0x7D4).
inline EngineTexture*& pathTarget(Renderer* renderer, unsigned int retailOffset)
{
    return renderer->m_pathTargets[(retailOffset - 0x778u) / 4u];
}

void releaseRendererPathResources(Renderer* renderer)
{
    IDirect3DSurface9*& backBuffer = renderer->m_backBuffer;      // +0x76C
    IDirect3DSurface9*& depth      = renderer->m_depthStencil;    // +0x770
    if (backBuffer) backBuffer->Release();
    backBuffer = 0;
    if (depth) depth->Release();
    depth = 0;

    renderer->releaseHemiScratchTexture();
    for (unsigned int offset = 0x778; offset <= 0x7C8; offset += 4)
        dx9rend::textureReleaseRef(pathTarget(renderer, offset));
    dx9rend::textureReleaseRef(pathTarget(renderer, 0x7CC));
    dx9rend::textureReleaseRef(pathTarget(renderer, 0x7D0));
    dx9rend::textureReleaseRef(pathTarget(renderer, 0x7D4));
}

// A viewport extent scaled by the reciprocal of the hemi scale, truncated to an integer. The retail body
// keeps 1.0f / scale in an x87 register and multiplies the extent by it before the truncating store.
inline int scaledViewportExtent(float extent, float scale)
{
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Preserve the unspilled extended-precision reciprocal.
    const bf2x87::Real inverse = bf2x87::div(bf2x87::wide(1.0f), bf2x87::wide(scale));
    return bf2x87::toIntTruncate(bf2x87::mul(inverse, bf2x87::wide(extent)));
#else
    return static_cast<int>(extent * (1.0f / scale));
#endif
}

// PE 0x100278B0 (listing read). These are Renderer-owned path targets, created after LightManager's own
// shadow resources. No failure checks were present in retail; null wrappers remain null and are handled
// by the same downstream target-selection fallbacks. The hemi scratch texture is the viewport divided by
// the hemi scale; the full-size arm then reads the viewport a second time and truncates it to integers
// (quarter sizes are signed divisions by four); the other arm uses fixed 512x512, 512x128 and 128x128.
void createRendererPathResources(Renderer* renderer)
{
    const int oldManaged = dx9rend::textureManagerSwapManaged(0);
    dx9rend::IRenderView* const view = static_cast<dx9rend::IRenderView*>(
        reinterpret_cast<RendDX9Base*>(renderer)->renderView);
    float* const viewport = view->getViewport();
    const float scale = renderer->m_f58C;                                  // +0x58C
    const int scaledWidth = scaledViewportExtent(viewport[2], scale);
    const int scaledHeight = scaledViewportExtent(viewport[3], scale);
    renderer->setHemiScratchTexture(
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(scaledWidth), static_cast<unsigned int>(scaledHeight),
            D3DFMT_A8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0)));

    const bool fullSize = dx9rend::deviceCapsSupportsFullSizeRenderTargets();
    renderer->m_fullSizeRenderTargets = fullSize ? 1u : 0u;              // +0x6BC
    int width = 512;
    int height = 512;
    int halfHeight = 128;
    int quarterWidth = 128;
    int quarterHeight = 128;
    if (fullSize)
    {
        float* const fullViewport = view->getViewport();                   // second read, as shipped
        width = static_cast<int>(fullViewport[2]);
        height = static_cast<int>(fullViewport[3]);
        halfHeight = height / 4;
        quarterWidth = width / 4;
        quarterHeight = height / 4;
    }

    pathTarget(renderer, 0x788) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(width), static_cast<unsigned int>(height),
            D3DFMT_A8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0));
    pathTarget(renderer, 0x7A0) =
        static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(width), static_cast<unsigned int>(halfHeight),
            D3DFMT_A8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0));
    for (unsigned int offset = 0x7A4; offset <= 0x7C8; offset += 4)
    {
        pathTarget(renderer, offset) =
            static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerCreateEmpty(
                static_cast<unsigned int>(quarterWidth), static_cast<unsigned int>(quarterHeight),
                D3DFMT_A8R8G8B8, 1, D3DUSAGE_RENDERTARGET, 0));
    }
    dx9rend::textureManagerSwapManaged(oldManaged);
}

// PE 0x10027FD0
void RD9_THUNK renderer_renderPathActivated(void* listener RD9_THUNK_EDX, int path)
{
    Renderer* const self = rendererFromRenderPathListener(listener);
    self->m_currentRenderPath = path;                                      // +0x544

    RendDX9Base* const renderer = reinterpret_cast<RendDX9Base*>(self);
    renderer->device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &self->m_backBuffer);
    renderer->device->GetDepthStencilSurface(&self->m_depthStencil);
    dx9rend::stateManagerSetRenderTarget(0, self->m_backBuffer);
    dx9rend::stateManagerSetDepthStencilSurface(self->m_depthStencil);

    if (geom::g_LightManager)
        geom::createShadowResources(geom::g_LightManager);
    createRendererPathResources(self);
}

// PE 0x100289D0
void RD9_THUNK renderer_renderPathDeactivated(void* listener RD9_THUNK_EDX)
{
    releaseRendererPathResources(rendererFromRenderPathListener(listener));
}

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Notification has a discarded path argument on native targets.
void renderer_renderPathNull(void*, int) {}
#else
void RD9_THUNK renderer_renderPathNull(void* RD9_THUNK_EDX) {}
#endif
void RD9_THUNK renderer_renderPathNullWithPath(void* RD9_THUNK_EDX, int) {}
int RD9_THUNK renderer_getCurrentRenderPath(void* listener RD9_THUNK_EDX)
{
    return rendererFromRenderPathListener(listener)->m_currentRenderPath;   // listener + 4
}
// PE 0x10032710, listener +0x14 (`retn 4`): the name, constructed into the caller's slot.
std::string* RD9_THUNK renderer_getRenderPathName(void* RD9_THUNK_EDX, std::string* out)
{
    new (out) std::string("BF2Renderer");
    return out;
}

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
void* const kRendererRenderPathVtable[] =
{
    BF2_NATIVE_SLOT(::dice::hfe::dx9rend::RenderPathListener, onRenderPathActivated, renderer_renderPathActivated),
    BF2_NATIVE_SLOT(::dice::hfe::dx9rend::RenderPathListener, onRenderPathDeactivated, renderer_renderPathDeactivated),
    BF2_NATIVE_SLOT(::dice::hfe::dx9rend::RenderPathListener, onRenderPathChanged, renderer_renderPathNullWithPath),
    BF2_NATIVE_SLOT(::dice::hfe::dx9rend::RenderPathListener, onRenderPathNotification, renderer_renderPathNull),
    BF2_NATIVE_SLOT(::dice::hfe::dx9rend::RenderPathListener, getCurrentRenderPath, renderer_getCurrentRenderPath),
    BF2_NATIVE_SLOT(::dice::hfe::dx9rend::RenderPathListener, getRenderPathListenerName, renderer_getRenderPathName),
};
#else
void* const kRendererRenderPathVtable[6] =
{
    reinterpret_cast<void*>(&renderer_renderPathActivated),
    reinterpret_cast<void*>(&renderer_renderPathDeactivated),
    reinterpret_cast<void*>(&renderer_renderPathNullWithPath),
    reinterpret_cast<void*>(&renderer_renderPathNull),
    reinterpret_cast<void*>(&renderer_getCurrentRenderPath),
    reinterpret_cast<void*>(&renderer_getRenderPathName)
};
#endif


// Replaces the owned overlay texture from its stored name; an empty name leaves it alone.
// Device reset and lazy frame loading share this operation.
// PE 0x10028F60
void rendererReloadOverlayTexture(Renderer* renderer)
{
    const std::string& name = renderer->overlayTextureName();
    if (name.size() == 0)                                   // cmp [edi+6F4h], 0 / jz
        return;
    dx9rend::textureReleaseRef(renderer->m_overlayTexture);              // +0x8D8
    dx9rend::EngineTexture* texture;
    {
        const std::string textureName(name);
        texture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(textureName));
    }
    renderer->m_overlayTexture = texture;
}

// PE 0x10028FD0, run by renderFrame while +0x6DD is set: draws the overlay texture (+0x8D8) over
// the current render target with FSQuadDrawer pass 45, scaled by +0x6FC and placed at
// (+0x700, -+0x704) through the pass's custom matrix. A missing texture is loaded from its stored
// name first; when that fails too the name is cleared, so the load is not retried every frame.
void rendererDrawOverlayTexture(Renderer* renderer)
{
    if (!renderer->m_overlayTexture)                                         // +0x8D8
    {
        if (renderer->overlayTextureName().size() > 0)                       // cmp [esi+6F4h], 0
            rendererReloadOverlayTexture(renderer);                          // sub_10028F60
        if (!renderer->m_overlayTexture)
        {
            renderer->m_overlayTextureName.clear();                          // +0x6E0
            return;
        }
    }
    const float scale = renderer->m_f6FC[0];                                  // +0x6FC
    float matrix[16];
    matrix[0]  = scale; matrix[1]  = 0.0f;  matrix[2]  = 0.0f; matrix[3]  = 0.0f;
    matrix[4]  = 0.0f;  matrix[5]  = scale; matrix[6]  = 0.0f; matrix[7]  = 0.0f;
    matrix[8]  = 0.0f;  matrix[9]  = 0.0f;  matrix[10] = 1.0f; matrix[11] = 0.0f;
    matrix[12] = renderer->m_f6FC[1];                                         // +0x700
    matrix[13] = -renderer->m_f6FC[2];                                        // -+0x704 (fchs)
    matrix[14] = 0.0f;  matrix[15] = 1.0f;
    geom::fsQuadDrawerSourcePass(45u, renderer->m_overlayTexture, false, true, 0u,
                                 matrix);                                     // sub_10085CF0
}

// Releases overlay texture, then render-path resources on device loss.
// StateManager's higher-priority listener has already released its surface references.
// PE 0x10028870
void RD9_THUNK renderer_onDeviceLost(void* listener RD9_THUNK_EDX)
{
    Renderer* const renderer = rendererFromDeviceLostListener(listener);
    dx9rend::textureReleaseRef(renderer->m_overlayTexture);              // +0x8D8
    releaseRendererPathResources(renderer);
}

// Reacquires back/depth surfaces, rebuilds path resources, and reloads the overlay.
// LightManager restores its own shadow resources; D3D acquisition results are unchecked.
// PE 0x1002B9D0
void RD9_THUNK renderer_onDeviceReset(void* listener RD9_THUNK_EDX)
{
    Renderer* const self = rendererFromDeviceLostListener(listener);
    RendDX9Base* const renderer = reinterpret_cast<RendDX9Base*>(self);

    renderer->device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &self->m_backBuffer);
    renderer->device->GetDepthStencilSurface(&self->m_depthStencil);
    dx9rend::stateManagerSetRenderTarget(0, self->m_backBuffer);
    dx9rend::stateManagerSetDepthStencilSurface(self->m_depthStencil);
    createRendererPathResources(self);
    rendererReloadOverlayTexture(self);
}

// 0x10027FC0 - `ret`.
void RD9_THUNK renderer_onDeviceDestroy(void* RD9_THUNK_EDX) {}

void* const kRendererDeviceLostVtable[3] =
{
    reinterpret_cast<void*>(&renderer_onDeviceLost),      // +0x00
    reinterpret_cast<void*>(&renderer_onDeviceReset),     // +0x04
    reinterpret_cast<void*>(&renderer_onDeviceDestroy)    // +0x08
};

} // anonymous namespace


// Initializes renderer state. Zero-filled opaque storage differs from allocator leftovers
// for fields the original constructor did not write.

namespace {

// Zeroes the plain members laid out from `first` up to (not including) `end`.
inline void zeroMemberRange(void* first, const void* end)
{
    ::memset(first, 0, static_cast<size_t>(static_cast<const unsigned char*>(end) -
                                           static_cast<unsigned char*>(first)));
}

void frameTimerConstruct(unsigned char* t);   // defined with the frame timer below (PE 0x10077710)
} // anonymous namespace

// PE 0x1002D4C0. Initializes renderer state; the stores are annotated with their retail addresses below.
Renderer::Renderer()
    : m_evHandlerVptr(0)
    , m_deviceLostVptr(0)
    , m_renderPathVptr(0)
    , m_currentRenderPath(0)
    , m_refCount(1)          // mov dword ptr [esi+548h], 1
{
    for (unsigned int i = 0; i < sizeof(m_baseStorage); ++i) m_baseStorage[i] = 0;
    // Every plain tail member is zeroed here, before the base half runs - in five ranges around
    // the four std containers, which their own constructors have already built empty. This
    // includes m_objectDrawer, which must be zero: initialize step 10 releases a non-null one
    // before replacing it.
    zeroMemberRange(&m_viewMatrix, &m_overlayTextureName);              // +0x54C .. +0x6DF
    zeroMemberRange(&m_overlayTextureNamePad, &m_streamObjects);        // +0x6FC .. +0x757
    zeroMemberRange(&m_expansionPack, &m_performanceRecords);           // +0x764 .. +0x853
    zeroMemberRange(&m_dword860, &m_string868);                         // +0x860 .. +0x867
    zeroMemberRange(&m_string868Pad, this + 1);                         // +0x884 .. +0x8DF

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
    m_f590 = 0.041f;         // 0x3D27EF9E, the store at 0x1002D58F
    m_near = 0.0099999998f;
    m_far  = 15.0f;
    m_f59C = 6.0f;
    m_lazy884 = 0;

    // Initialize per-frame subsystem enable flags and state. Byte/dword/float indices are the
    // retail ones (byte 1440 == +0x5A0, dword 375 == +0x5DC, float 432 == +0x6C0).
    {
        m_clearDepthBeforeFirstPerson = 1;   // byte 1440: the z-only clear before the 1st-person passes
        m_postProductionNeedsActive = 1;     // byte 1441: post-production requires its active() query
        m_renderAheadLimit = 2;      // the GPU-throttle fence ring uses TWO of the five fences
        m_fenceCursor = 0;           // dword 367, the ring cursor

        // the frame timer at +0x5DC - 48 bytes, QueryPerformanceCounter-backed (sub_10077710):
        // {pad, lastCounter int64 +8, accum int64 +16, dt double +24, total double +32,
        // 1/frequency double +40}
        {
            unsigned char* timer = static_cast<unsigned char*>(::renddx9abi::engineAlloc(48));
            if (timer)
                frameTimerConstruct(timer);                      // sub_10077710 (0x1002D609)
            m_frameTimer = timer;    // dword 375
        }

        sw(1693) = 1;                // fog block
        sw(1694) = 1;
        sw(1695) = 1;                // particles
        sw(1696) = 1;                // decals
        sw(1697) = 1;
        sw(1698) = 1;
        sw(1699) = 1;                // skinned meshes
        sw(1700) = 1;
        sw(1701) = 1;
        sw(1702) = 1;                // undergrowth
        sw(1703) = 1;
        sw(1704) = 1;                // nametags
        sw(1705) = 1;
        sw(1706) = 0;
        sw(1707) = 1;
        sw(1712) = 0;                // the debug graphs stay hidden
        sw(1713) = 1;                // the HudManager post-frame call
        sw(1714) = 1;                // sky
        sw(1715) = 1;
        sw(1716) = 1;                // the console query
        sw(1717) = 1;                // post-production
        m_byte6DC = 1;               // byte 1756
        m_byte6DD = 0;               // byte 1757

        m_fogOverride[0] = 4.0f;     // floats 432..434: the fog override triple the fog block swaps in
        m_fogOverride[1] = 5.0f;
        m_fogOverride[2] = 100.0f;
        m_f6FC[0] = 0.125f;          // floats 447..449 (0x3E000000 at 0x1002D74C, then 0.75 x2)
        m_f6FC[1] = 0.75f;
        m_f6FC[2] = 0.75f;
        m_lodDistances70C[0] = 10.0f;    // float 451
        m_lodDistances71C[0] = 10.0f;    // float 455
        m_lodDistances72C[0] = 10.0f;    // float 459
        m_f73C = 9999.0f;            // float 463
        m_f740 = 9999.0f;            // float 464
        m_streamBudget = 25.0f;      // float 465: the geometry-streaming budget - the scan runs every frame
        m_dword748 = 10;             // dword 466
        m_dword768 = 2;              // dword 474
        m_forcedLod[0] = -1;         // dwords 502..504: the forced static/skinned/bundled mesh LODs
        m_forcedLod[1] = -1;
        m_forcedLod[2] = -1;
        m_dword7E8 = 100;            // dword 506
        m_fpsBucketScale = 10.0f;    // float 520: the FPS bucket scale
        m_lodScale84C = 1.0f;        // float 531
        m_dword860 = 8;              // dword 536
        m_f864 = 2.0f;               // float 537
        m_lodScale8B0 = 1.0f;        // floats 556..558
        m_lodScale8B4 = 1.0f;
        m_lodScale8B8 = 1.0f;
        // The two string members at dword 440 and 538 are constructed empty; nothing recovered
        // writes them yet.
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

    // The tail matches PE 0x1002D4C5..0x1002DA25: every immediate and register store was checked
    // against the members above (the rest are the zeros and 0.0 doubles the ranges clear), and both
    // strings are default-constructed (MSVCP71 import 0x101DB1F4). The frame timer at +0x5DC is
    // this rebuild's copy of sub_10077710 on a bf_malloc(0x30) block.
    // The base half, RendDX9_base_ctor (0x1000B940), is constructD3DHalf in RendDX9Init.cpp; its
    // unwritten remainder is marked there, not here.
}

void Renderer::addRef()
{
    ++m_refCount;
}

int Renderer::getRef()
{
    return m_refCount;
}

// PE 0x10032780: at zero, the deleting destructor through the vtable (+0xB4) with 1.
int Renderer::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
        destroy(1);
    return remaining;
}

// PE 0x1002DA60, the destructor body: free the frame timer and clear it, release the three
// debug graphs, shut down, free the render view (no destructor runs, as shipped), then the
// members. The base destructor follows in destroy().
Renderer::~Renderer()
{
    if (m_frameTimer)                                                             // +0x5DC
    {
        ::renddx9abi::engineFree(m_frameTimer);
        m_frameTimer = 0;
    }
    for (unsigned int i = 0; i < 3; ++i)                                          // +0x894..+0x89C
    {
        if (m_debugGraphs[i])
            static_cast<IBase*>(m_debugGraphs[i])->release();
    }
    shutdown();                                                                   // 0x100287B0
    ::operator delete(reinterpret_cast<RendDX9Base*>(this)->renderView);          // +0xF8

    // The raw members' destruction: +0x884 is freed, and the ObjectDrawer (+0x5C0) released, at
    // their member positions. [CHOSEN, not recovered] The native string and vector members
    // (+0x868, +0x854, +0x758, +0x6E0) are destroyed after this body instead of between the
    // two; they own only memory. The +0x850 performance-analysis vector frees its rows after the
    // +0x868 string, as at 0x1002DB05.
    if (m_lazy884)
        ::renddx9abi::engineFree(m_lazy884);
    if (m_objectDrawer)
        static_cast<IBase*>(m_objectDrawer)->release();
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
    // The constructor always builds it; the original dereferences it unguarded.
    Rend::RenderView* view = static_cast<Rend::RenderView*>(b->renderView);
    Rend::renderViewSetViewMatrix(view, m_viewMatrix);
    ::renddx9abi::logf("Renderer::initialize: steps 1-3 done - view matrix (0,100,0) pushed to the "
                       "RenderView at 0x%08X", ::renddx9abi::addr(view));

    // Aspect is height/width (0x1002C913..0x1002C93B); an empty display mode is not tested.
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

    // step 7: three singletons, cached and not dereferenced here. update, the transparent pass
    // and handleLevelPreLoad call the weather and rain managers through these pointers.
    m_hudManager     = classManager ? classManager->getSingleton(renddx9abi::HostString("HudManager"))     : 0;
    m_weatherManager = classManager ? classManager->getSingleton(renddx9abi::HostString("WeatherManager")) : 0;
    m_rainManager    = classManager ? classManager->getSingleton(renddx9abi::HostString("RainManager"))    : 0;
    // HudManager is BF2.exe's own singleton; WeatherManager and RainManager are ModuleGeom's.
    ::renddx9abi::logf("Renderer::initialize: step 7 done - HudManager=0x%08X (BF2's own) "
                       "WeatherManager=0x%08X RainManager=0x%08X (ModuleGeom's)",
                       ::renddx9abi::addr(m_hudManager),
                       ::renddx9abi::addr(m_weatherManager),
                       ::renddx9abi::addr(m_rainManager));

    // Create a separate secondary view with far range m_far - m_near and FOV 0.82.
    IBase* view2 = classManager
                 ? classManager->createInstance(Rend::CID_RenderView, Rend::IID_IRenderView, 0)
                 : 0;
    b->renderView2 = view2;     // used unguarded below, as shipped

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
    // The singleton is used unguarded, as shipped.
    {
        IBase*  gtmBase = geom::g_GeometryTemplateManager;
        void**  vtbl    = *reinterpret_cast<void***>(gtmBase);
        typedef bool (__thiscall* RegisterFn)(void*, const std::string&, const std::string&);
        typedef void (__thiscall* VoidFn)(void*);

        if (!readBoolSetting("GSOptimizeGeomOnDemand", false))
        {
            RegisterFn registerOptimizer = reinterpret_cast<RegisterFn>(vtbl[0x50 / 4]);
            registerOptimizer(gtmBase, std::string("BundledMeshOptimizer"),
                                       std::string("BundledMeshOptimizer"));
            registerOptimizer(gtmBase, std::string("StaticMeshOptimizer"),
                                       std::string("StaticMeshOptimizer"));
            reinterpret_cast<VoidFn>(vtbl[0x54 / 4])(gtmBase);   // runOptimizers
        }

        reinterpret_cast<VoidFn>(vtbl[0x58 / 4])(gtmBase);       // clearOptimizers
        ::renddx9abi::logf("Renderer::initialize: step 11 done - GeometryTemplateManager "
                           "0x%08X swept", ::renddx9abi::addr(gtmBase));
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
#if defined(_M_IX86)
        void** slot = reinterpret_cast<void**>(reinterpret_cast<char*>(this) + 0x5A4);
#else
        // [CHOSEN, not recovered] c-loopback: the retail offset is m_frameFences only on x86 (the
        // static_asserts above). On LP64 +0x5A4 lies inside the base's embedded VideoOptions and
        // the five pointers overwrote aaDescriptors' capacity word: the first profile login then
        // crashed in buildAAList's push_back.
        void** slot = m_frameFences;
#endif
        for (int i = 0; i < 5; ++i)
            slot[i] = queryManagerCreateQuery(D3DQUERYTYPE_EVENT);

        ::renddx9abi::logf("Renderer::initialize: step 13 done - five D3DQUERYTYPE_EVENT fences at "
                           "+0x5A4: %08X %08X %08X %08X %08X",
                           ::renddx9abi::addr(slot[0]), ::renddx9abi::addr(slot[1]),
                           ::renddx9abi::addr(slot[2]), ::renddx9abi::addr(slot[3]),
                           ::renddx9abi::addr(slot[4]));
    }

    // PE 0x1002CCE8..0x1002CCFD: while base flag +0x102 is set ([UNRESOLVED] role), load every
    // Ra shader file up front - sub_10042150 on the render-queue table with flags 1. The base
    // global is read without a null test.
    if (static_cast<const RendDX9Base*>(g_rendererBase)->flag102)
        Ra::raLoadAllShaders(static_cast<Ra::RaShaderTable*>(g_renderQueues), 1);

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
            m_debugGraphs[i] = g;      // dereferenced unguarded, as shipped

            void** v = *reinterpret_cast<void***>(g);
            typedef void (__thiscall* IntFn)(void*, int);
            typedef void (__thiscall* StrFn)(void*, const std::string&);
            typedef void (__thiscall* PtrFn)(void*, const float*);
            typedef void (__thiscall* Col3Fn)(void*, unsigned, unsigned, unsigned);
            typedef void (__thiscall* Flt2Fn)(void*, float, float);
            typedef void (__thiscall* FltFn)(void*, float);

            // +0x3C first, and it is the one that matters: it allocates the ring buffer every later
            // call writes into. See DebugLineGraphD3d.cpp - it is a named frontier.
            reinterpret_cast<IntFn>(v[0x3C / 4])(g, 250);

            reinterpret_cast<StrFn>(v[0x18 / 4])(g, std::string(spec.name));
            reinterpret_cast<PtrFn>(v[0x10 / 4])(g, spec.rect);
            reinterpret_cast<Col3Fn>(v[0x44 / 4])(g, spec.colours[0], spec.colours[1],
                                                  spec.colours[2]);
            reinterpret_cast<Flt2Fn>(v[0x4C / 4])(g, spec.rangeLo, spec.rangeHi);
            reinterpret_cast<IntFn>(v[0x50 / 4])(g, 1);
            reinterpret_cast<Flt2Fn>(v[0x1C / 4])(g, 0.0f, spec.arg1C);
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
    // Both instances are dereferenced unguarded, as shipped (0x1002BA7D / 0x1002BB38).
    m_consoleFont = classManager ? classManager->createInstance(81007u, 80001u, 0) : 0;

    {
        // font->vslot+0x10(path). The bool decides only which message is logged.
        void** vtbl = *reinterpret_cast<void***>(m_consoleFont);
        typedef bool (__thiscall* LoadFn)(void*, const std::string&);
        std::string path("Fonts/BF1942.font");
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

    {
        void** vtbl = *reinterpret_cast<void***>(m_consoleOverlay);
        typedef void (__thiscall* SetTextureFn)(void*, const std::string&);
        typedef void (__thiscall* SetFloatFn)(void*, float);

        std::string texture("Menu/Console/white");
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
    // Dispatched unguarded, as shipped (0x1002C9FF / 0x1002CA47).
    IBase* writer = classManager ? classManager->getSingleton(renddx9abi::HostString(singletonName)) : 0;

    void** vtbl = *reinterpret_cast<void***>(writer);
    typedef bool (__thiscall* SetFontFn)(void*, const std::string&);

    std::string path(fontPath);
    const bool ok = reinterpret_cast<SetFontFn>(vtbl[0x10 / 4])(writer, path);

    ::renddx9abi::logf("Renderer::initialize: step 6 - %s.setFont(\"%s\") -> %s "
                       "(the original discards this bool)",
                       singletonName, fontPath, ok ? "true" : "false");
}

// The host supplies display-mode inputs: the depth-format vector must be nonempty;
// the multisample vector may be empty. Do not infer these inputs from the D3D device.

// PE 0x10005720, +0x44: the complete body stores its one pointer argument at +0xF8,
// materializes true in AL, then returns with ret 4. There is no addRef/release or null guard.
// [INFERRED] source name and IBase* spelling: BF2.exe Ref2System's paint brackets pass their
// RenderView object through this slot. The existing +0xF8 overlay remains RW-56's named debt.
bool Renderer::setRenderView(IBase* view)
{
    reinterpret_cast<RendDX9Base*>(this)->renderView = view;
    return true;
}

// PE 0x10005740, +0x4C: byte-for-byte twin of +0x44 in the retail image.  HemiMapManager invokes
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
    return multiSampleListActive(base);
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

// The RendDX9.dll copy of the QPC timer (48 bytes: pad, lastCount int64 +8, accum int64 +16,
// deltaTime double +24, time double +32, secondsPerCount double +40). Unlike the Core/Timer.cpp
// recovery (32-bit truncation before scaling), this image keeps every count 64-bit (fild qword).
// [INFERRED] free-function names over the raw block.

// The PerformanceCounter constructor's frequency (dword_10247C30 / dword_10247C34).
LONGLONG g_frameTimerFrequency = 0;

// PE 0x1007C040: the PerformanceCounter constructor at the timer's +0 (the pad word, never
// written): QueryPerformanceFrequency into the static; answers the object.
// [INFERRED] free-function name over the raw block, like the other frame-timer pieces.
unsigned char* frameTimerCounterConstruct(unsigned char* t)
{
    LARGE_INTEGER frequency;
    ::QueryPerformanceFrequency(&frequency);
    g_frameTimerFrequency = frequency.QuadPart;
    return t;
}

// PE 0x1007C070: PerformanceCounter::getFrequency, the static in edx:eax.
LONGLONG frameTimerFrequency()
{
    return g_frameTimerFrequency;
}

// PE 0x1007C080: getCount, QueryPerformanceCounter.
LONGLONG frameTimerCount()
{
    LARGE_INTEGER now;
    ::QueryPerformanceCounter(&now);
    return now.QuadPart;
}

// Re-origins the timer at now minus seconds, then samples once (the update is inlined).
// PE 0x100776B0. The origin is __ftol2 of seconds / secondsPerCount, stored as 64 bits.
void frameTimerReset(unsigned char* t, double seconds)
{
    *reinterpret_cast<LONGLONG*>(t + 8) = frameTimerCount();
    *reinterpret_cast<LONGLONG*>(t + 16) =
        static_cast<LONGLONG>(seconds / *reinterpret_cast<double*>(t + 40));
    const LONGLONG count = frameTimerCount();
    const LONGLONG delta = count - *reinterpret_cast<LONGLONG*>(t + 8);
    *reinterpret_cast<LONGLONG*>(t + 16) += delta;
    *reinterpret_cast<LONGLONG*>(t + 8) = count;
    const double secondsPerCount = *reinterpret_cast<double*>(t + 40);
    *reinterpret_cast<double*>(t + 24) = static_cast<double>(delta) * secondsPerCount;
    *reinterpret_cast<double*>(t + 32) =
        static_cast<double>(*reinterpret_cast<LONGLONG*>(t + 16)) * secondsPerCount;
}

// Constructs the timer in place: frequency, secondsPerCount = 1.0 / frequency, reset(0.0).
// PE 0x10077710. The pad word at +0 is never written.
void frameTimerConstruct(unsigned char* t)
{
    frameTimerCounterConstruct(t);
    *reinterpret_cast<double*>(t + 40) = 1.0 / static_cast<double>(frameTimerFrequency());
    frameTimerReset(t, 0.0);
}

// PE 0x100776A0: getTime, the double at +0x20.
double frameTimerTime(const void* timer)
{
    return *reinterpret_cast<const double*>(static_cast<const unsigned char*>(timer) + 0x20);
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

// sub_1004F1C0 - is post-production active. Reads the owner object's flags (+0x200, +0x24, +0x164,
// +0x27, +0x220, +0xFA, then the float +0xE0) through its owner TU, which knows the layout; a
// null owner answers false. The rebuilt owner is zeroed, so every term is false - the at-rest
// state of a real one.
bool postProductionActive()
{
    return postProductionFrameWorkIsActive();
}

// sub_100F7AD0 - is the camera under water: view matrix Y against the constants block's
// WaterHeight. With the block withheld the answer is "above water", the menu truth.
bool isCameraUnderWater()
{
    if (!g_raShaderConstants)
        return false;
    float* m = viewGetMatrixPtr(rendererGetRenderView());
    return m[13] < *reinterpret_cast<float*>(
        raShaderConstantsAt(444));
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
        raShaderConstantsAt(444));
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


// PE 0x1002BB80, +0x24: Renderer::update(time), the per-frame tick BF2 calls before renderFrame.
// The shipped body updates the world-side singletons unconditionally because a shipped ModuleGeom
// always creates them; here every withheld one is guarded with a one-time note. WindManager is
// resolved by name each call - BF2's own singleton, absent until a level registers it.
void Renderer::update(double time)
{
    ++reinterpret_cast<RendDX9Base*>(this)->frameOrdinal;      // retail dword 65
    m_frameTime = time;                                         // +0x5E8 (self + 1512)
    const float timeF = static_cast<float>(time);

    {
        void* wind = classManager ? classManager->getSingleton(renddx9abi::HostString("WindManager")) : 0;
        if (wind)
            hostCalls::windUpdate(wind, timeF);                                        // +0x10
    }

    if (geom::g_SkyDome)
    {
        typedef void (__thiscall* Fn)(void*, float);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(geom::g_SkyDome))[4])(
            geom::g_SkyDome, timeF);
    }
    else
        noteOnce(1u, "SkyDome::update");

    if (sw(1702) && geom::g_UndergrowthSystem)
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
        static_cast<geom::IDecalManager*>(geom::g_DecalManager)->update(timeF);    // +0x30
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

// PE 0x10028EE0
void Renderer::hudManagerFramePrep(float timeF)
{
    void* hud = m_hudManager;
    if (!hud)
        return;

    hostCalls::hudSetPaint(hud, sw(1713));                                           // +0x20

    void* player = hostCalls::localHumanPlayer();                                       // +0x30

    float* m = viewGetMatrixPtr(getRenderView());
    hostCalls::hudUpdate(m_hudManager, timeF, player, m[8], m[9], m[10]);               // +0x28
}


// Updates road constants and flushes the road queue with depth bias.
// Requires a road renderer and terrain. Adds frame time again after the main accumulation.

void Renderer::roadQueuePass()
{
    if (!geom::g_RoadCompiledRenderer || !geom::g_Terrain)
        return;

    IRenderView* const view = rendererGetRenderViewDup();                                   // +0x50
    // The Ra constants block by retail offset (raShaderConstantsAt maps it on x64, issue #7).
    *reinterpret_cast<float*>(raShaderConstantsAt(0x94)) += static_cast<float>(m_frameTime);
    g_waterClock += static_cast<float>(m_frameTime);

    {
        const float* const m = view->getViewMatrixPtr()->m;                                  // +0x64
        float* const matrix = reinterpret_cast<float*>(raShaderConstantsAt(0x1C));
        for (int i = 0; i < 16; ++i)
            matrix[i] = m[i];
        float* const camera = reinterpret_cast<float*>(raShaderConstantsAt(0x60));
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
    float* const sunColor = reinterpret_cast<float*>(raShaderConstantsAt(0x11C));
    sunColor[0] = sun.x;
    sunColor[1] = sun.y;
    sunColor[2] = sun.z;

    const float* const fade = geom::roadCompiledRendererFadeOut();
    float* const roadFadeOut = reinterpret_cast<float*>(raShaderConstantsAt(0x12C));
    roadFadeOut[0] = fade[0];
    roadFadeOut[1] = fade[1];

    postProductionUpdateFog(underwaterDepth());

    stateManagerPush();
    stateManagerSetRenderState(195u, geom::roadCompiledRendererDepthBiasBits());            // D3DRS_DEPTHBIAS
    stateManagerSetRenderState(175u, geom::roadCompiledRendererSlopeScaleDepthBiasBits());  // slopescaledepthbias
    renderQueueFlush(0x24, 1);                                                               // RSMTRoad
    stateManagerPop();
}


// sub_1002B8E0 - the expansion-pack light blend. False (nothing moved) while either duration is
// zero or the camera's simple-shadow sample (LightManager +0x2C4 at the view's x, z) equals the
// current level; otherwise the level moves toward the sample by the elapsed fraction of the rise
// (sample above) or fall duration, clamped at the whole step, and the mark advances. The tests are
// x87 `fucompp`/`jnp`: an unordered comparison does not count as equal.
bool Renderer::xpLightBlendTick()
{
    if (m_xpLightRiseTime == 0.0)                                                 // +0x8C0
        return false;
    if (m_xpLightFallTime == 0.0)                                                 // +0x8C8
        return false;

    const float sample = sampleShadowUnderView(getRenderView());                  // +0x48, inline
    const float delta = sample - m_xpLightLevel;                                  // +0x8BC
    if (delta == 0.0f)
        return false;

    const double duration = (delta > 0.0f) ? m_xpLightRiseTime : m_xpLightFallTime;
    const double now = frameTimerTime(m_frameTimer);                              // sub_100776A0
    double step = (now - m_xpLightMarkTime) / duration;                           // +0x8D0
    if (step > 1.0)
        step = 1.0;
    m_xpLightLevel = static_cast<float>(step * delta + m_xpLightLevel);
    m_xpLightMarkTime = now;
    return true;
}


// sub_1002DF20 - the geometry-streaming scan, and it runs at the menu: the budget at float 465 is
// the constructor's 25.0. Build a forward-expanded frustum from the render view's, hand it to the
// host ObjectManager's frustum query, and touch the mesh templates of whatever comes back. With
// an empty world the result vector comes back empty and the touch loop never runs; the touch arms
// belong to the unbuilt mesh front and are a named frontier.

namespace { void streamTouchGeometry(Renderer* renderer); }

void Renderer::geometryStreamTick()
{
    if (m_streamBudget <= 0.00000011920929f)
        return;

    m_dword74C = 0;
    m_dword750 = 0;
    m_streamObjects.clear();                 // resize the result vector to zero (first == last)

    void* view = getRenderViewDup();
    Frustum* const vf = static_cast<Frustum*>(viewGetFrustumPtr(view));
    const float* vfMatrix = vf->m_transformation.m;                         // frustum +0x174

    float m[16];
    for (int i = 0; i < 16; ++i)
        m[i] = vfMatrix[i];

    // slide the frustum origin half a budget along the look row, and stretch near/far by the
    // budget - the shipped -budget * row * 0.5 fold.
    const float budget = m_streamBudget;
    m[12] += m[8]  * -budget * 0.5f;
    m[13] += m[9]  * -budget * 0.5f;
    m[14] += m[10] * -budget * 0.5f;

    const float scanFar  = budget * 1.5f + vf->m_farDist;                  // frustum +0x1E0
    const float scanNear = budget * 0.5f;

    // The pointer-free Frustum is 0x1E8 bytes on both architectures (RenderView.cpp asserts it).
    union { unsigned char bytes[0x1E8]; float align; } scanStorage;
    unsigned char* const scan = scanStorage.bytes;
    ::memset(scan, 0, sizeof(scanStorage.bytes));
    Rend::renderViewBuildFrustum(scan, vf->m_fov, vf->m_aspect,             // +0x1CC / +0x1D0
                                 scanNear, scanFar, m);

    // host ObjectManager vslot +0x5C: query(frustum, &resultVector, 0, 0, 0, 0) into this
    // object's result vector (retail dwords 469..472) - see hostCalls (RW-266).
    hostCalls::objectManagerQueryFrustum(scan, m_streamObjects);                       // +0x5C

    streamTouchGeometry(this);                     // 0x1002E043..0x1002E0F1

    // PE 0x1002E0F7..0x1002E10A.
    if (geom::g_Terrain)
        static_cast<geom::Terrain*>(geom::g_Terrain)->geometryStreamTick(
            reinterpret_cast<const Frustum*>(scan));
}


void rendererDrawFrameTimeOverlay(Renderer* renderer);   // sub_10029C50, defined after renderFrame
void rendererDrawCullingOverlay(Renderer* renderer);     // sub_10029BB0, defined after renderFrame
namespace { void raConstantsSetSunColor(const Vec3& colour); }   // sub_100387E0, defined below

// PE 0x1002F7E0: +0x28, Renderer::renderFrame. The full shipped sequence with every flag-gated arm
// carried; several singleton dereferences the shipped body makes unguarded are guarded here (one-time
// notes). While a panorama movie is recorded (base +0x3F4) the body runs once per slice.
bool Renderer::renderFrame(double time, float deltaTime)
{
    float          timeF = static_cast<float>(time);   // zeroed with `time` by the panorama arm

    // BF2Render.cpp:991 - the state-manager sanity check.
    if (!stateManagerIsFullyRestored())
    {
        ::renddx9abi::logf("BF2Render.cpp:991 State manager: render states not completely "
                           "restored at beginning of rendering of the frame");
        stateManagerForceRestore();
    }

    if (geom::g_StaticMeshRenderer)
        static_cast<geom::StaticMeshRenderer*>(
            static_cast<void*>(geom::g_StaticMeshRenderer))->m_culledMaterials = 0;   // retail +0x5C

    // host System vslot +0x28: frame begin - storeAndTurnOffFPUExceptions (typed, RW-266).
    hostCalls::systemFrameBegin();

    // The view setup: near planes, the second view's far, its matrix from the first view, the
    // camera into the (withheld) constant block, the frame timer.
    viewSetNearPlane(getRenderView(),    m_f590);
    viewSetNearPlane(getRenderViewDup(), m_f590);
    viewSetNearPlane(getRenderView2(),   m_near);
    viewSetFarPlane(getRenderView2(),    m_far - m_near);
    viewSetViewMatrix(getRenderView2(),  viewGetMatrixPtr(getRenderViewDup()));
    if (g_raShaderConstants)
    {
        // sub_10037EE0: view matrix into block+28, camera position into block+96 (WorldSpaceCamPos)
        float* dst = reinterpret_cast<float*>(raShaderConstantsAt(28));
        float* src = viewGetMatrixPtr(getRenderViewDup());
        for (int i = 0; i < 16; ++i)
            dst[i] = src[i];
        float* cam = reinterpret_cast<float*>(raShaderConstantsAt(96));
        cam[0] = src[12]; cam[1] = src[13]; cam[2] = src[14];
    }
    frameTimerTick(m_frameTimer);

    const double presentGate = time;

    if (isMultisampleActive())
        stateManagerSetRenderStatePersistent(161, 1);

    // PE 0x1002F977..0x1002FA4D: while a demo movie is recorded (base +0x3F4), a panorama
    // renders one pass per slice, and the camera is saved here and put back after the passes.
    MovieRenderer* const movie = static_cast<RendDX9Base*>(g_rendererBase)->movieRenderer;
    int iterations = 1;
    if (movie && movie->isPanorama())                                             // sub_100032F0
        iterations = movie->sliceCount();                                         // sub_10003300
    Mat4 savedCamera = Mat4::getIdentity();
    if (movie)
        savedCamera = *rendererGetRenderView()->getViewMatrixPtr();               // +0x48, +0x64

    for (int i = 0; i < iterations; ++i)
    {
        // PE 0x1002FA60..0x1002FBC5: a panorama slice. The saved camera is levelled (a forward
        // axis with no horizontal extent becomes +X), re-orthonormalised with Y up, turned by
        // i * 360 / iterations degrees about Y (rotation part only; the camera position is kept)
        // and set on both views. Slices after the first render with time 0.
        if (movie && movie->isPanorama())
        {
            Mat4 slice = savedCamera;                                             // 0x10030E80
            Vec3& right = *reinterpret_cast<Vec3*>(&slice.m[0]);
            Vec3& up = *reinterpret_cast<Vec3*>(&slice.m[4]);
            Vec3& forward = *reinterpret_cast<Vec3*>(&slice.m[8]);
            forward.y = 0.0f;
            if (forward.x * forward.x + forward.z * forward.z <= 0.001f)
            {
                forward.x = 1.0f;
                forward.y = 0.0f;
                forward.z = 0.0f;
            }
            up.x = 0.0f;
            up.y = 1.0f;
            up.z = 0.0f;
            rendmath::makeOrthonormalBasis(up, forward, right);                   // 0x1007B110

            Mat4 turn = Mat4::getIdentity();                                      // 0x100791E0
            setRotateYDeg(turn, static_cast<float>(static_cast<double>(i) /
                                                   static_cast<double>(iterations) * 360.0));
            Mat4 view = Mat4::getIdentity();
            rendmath::rotMult(view, slice, turn);                                 // 0x10078180
            view.m[12] = savedCamera.m[12];
            view.m[13] = savedCamera.m[13];
            view.m[14] = savedCamera.m[14];
            rendererGetRenderView()->setViewMatrix(view.m);                       // +0x48, +0x60
            rendererGetRenderViewDup()->setViewMatrix(view.m);                    // +0x50, +0x60
            if (i > 0)
            {
                time = 0.0;
                timeF = 0.0f;
            }
        }
        m_frameTime = time;

        // PE 0x1002FBD2..0x1002FBED: the xpack light arm - step the blend, and when it moved,
        // sub_100F2740 on the LightManager.
        if (isExpansionPack() && xpLightBlendTick())
        {
            geom::g_LightManager->updateExpansionLighting();
        }

        // PE 0x1002FBF2: the post-production mode selector picks this frame's preset
        // (sub_1004DF30). Retail does not null-test dword_1023F3D8.
        postProductionModeSelectorUpdate(g_renderTargetSystem);

        {
            void* wind = classManager ? classManager->getSingleton(renddx9abi::HostString("WindManager"))
                                      : 0;
            if (wind)
                hostCalls::windUpdate(wind, timeF);                                    // +0x10
        }

        // The fog block: save the host ObjectManager's fog triple, swap in the renderer's
        // override, run the ObjectDrawer, restore. The drawer is withheld; the host fog churn is
        // the shipped behaviour and runs.
        if (sw(1693))
        {
            void* om = objectManager;
            void*** omv = reinterpret_cast<void***>(om);
            typedef float (__thiscall* GetF)(void*);
            typedef void  (__thiscall* SetF)(void*, float);
            const float fogA = reinterpret_cast<GetF>((*omv)[49])(om);   // +196
            const float fogB = reinterpret_cast<GetF>((*omv)[51])(om);   // +204
            const float fogC = reinterpret_cast<GetF>((*omv)[47])(om);   // +188
            reinterpret_cast<SetF>((*omv)[46])(om, m_fogOverride[2]);             // +184
            reinterpret_cast<SetF>((*omv)[48])(om, m_fogOverride[0]);             // +192
            reinterpret_cast<SetF>((*omv)[50])(om, m_fogOverride[1]);             // +200
            if (m_objectDrawer)
            {
                void*** dv = reinterpret_cast<void***>(m_objectDrawer);
                typedef void (__thiscall* SetViewFn)(void*, void*);
                typedef void (__thiscall* TimeFn)(void*, float);
                reinterpret_cast<SetViewFn>((*dv)[10])(m_objectDrawer, getRenderView());
                reinterpret_cast<TimeFn>((*dv)[5])(m_objectDrawer, timeF);
                reinterpret_cast<TimeFn>((*dv)[4])(m_objectDrawer, timeF);
            }
            else
                noteOnce(16u, "ObjectDrawer (fog block)");
            reinterpret_cast<SetF>((*omv)[48])(om, fogA);
            reinterpret_cast<SetF>((*omv)[50])(om, fogB);
            reinterpret_cast<SetF>((*omv)[46])(om, fogC);
        }

        if (sw(1714))
        {
            if (geom::g_SkyDome)
            {
                typedef void (__thiscall* Fn)(void*, float);
                reinterpret_cast<Fn>((*reinterpret_cast<void***>(geom::g_SkyDome))[4])(
                    geom::g_SkyDome, timeF);
            }
            else
                noteOnce(32u, "SkyDome (frame update)");
        }

        if (m_weatherManager && sw(1695))
            geom::asWeatherManager(m_weatherManager)->update(time);                      // +0x18
        if (m_rainManager && sw(1695))
            geom::asRainManager(m_rainManager)->update(time);                            // +0x18
        if (sw(1702) && geom::g_UndergrowthSystem)
            static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->update(timeF);   // +0x24
        if (geom::g_LightManager)
        {
            geom::g_LightManager->update(time);
        }
        if (sw(1695) && geom::g_ParticleSystemManager)
        {
            geom::asParticleSystemManager(geom::g_ParticleSystemManager)->update(timeF);
        }
        if (geom::g_DecalManager)
            static_cast<geom::IDecalManager*>(geom::g_DecalManager)->update(timeF); // +0x30

        // 0x1002FDED `mov eax, [ebp+arg_8]; push eax` - the second argument is forwarded verbatim
        // and lands on BF2HudManager::update as its float dt. It IS a float: BF2.exe pushes its
        // BF2Engine +0x38, the clamped mainLoop delta (see RendMisc/IRenderer.h). The old carrier
        // here bit-cast an int "frame index", which reached the HUD as a denormal ~0 and froze
        // every HUD easing - most visibly the spawn map, which never grew to full size.
        hudManagerFramePrep(deltaTime);

        // PE 0x1002FDF8..0x1002FE03: the environment-map selection step, the same call
        // Renderer::update makes (dword_10267A34 tested, then sub_101C0870 with the float time).
        if (Rend::g_EnvMapManager)
            static_cast<Rend::EnvMapManager*>(Rend::g_EnvMapManager)->update(timeF);

        if (sw(1717) && g_PostProductionFrameWorkOwner
            && (!m_postProductionNeedsActive || postProductionActive()))
        {
            postProductionPrepare();
        }

        if (beginScene(0, 0))
        {
            // PE 0x1002FE52 passes XpLightManager.cpp's coordinator at 0x10259B20, not the
            // LightManager singleton.  The old guard/frontier conflated those two objects.
            geom::prepareLightFrame(time);
            // PE 0x1002FE5F: the mode selector's first-frame step (sub_1004DF10).
            postProductionModeSelectorFrameStart(g_renderTargetSystem);

            if (geom::g_LightManager)
            {
                geom::g_LightManager->renderShadowMaps();
            }
            else
                noteOnce(64u, "LightManager (shadow-map pass + light constants)");

            // sub_100057E0(7, 0, 1.0f, 0, 0, 0) - the frame clear: RT flush + Clear ALL, black.
            flushRenderTargetsSeam();
            {
                IDirect3DDevice9* dev = static_cast<RendDX9Base*>(g_rendererBase)->device;
                dev->Clear(0, 0, 7, 0, 1.0f, 0);
            }

            if (sw(1695) && geom::g_ParticleSystemManager)
            {
                geom::particleSystemManagerBeginFrame(
                    geom::asParticleSystemManager(geom::g_ParticleSystemManager));
            }

            // sub_100383D0 - GlobalTime accumulate on the withheld constants block.
            if (g_raShaderConstants)
            {
                *reinterpret_cast<float*>(
                    raShaderConstantsAt(148)) +=
                    static_cast<float>(m_frameTime);
                g_waterClock += static_cast<float>(m_frameTime);
            }

            // The constants block is still unpublished in this
            // reconstruction, so retain its existing guard while using the exact LightManager
            // getters and destinations once it is present.
            if (g_raShaderConstants && geom::g_LightManager)
            {
                geom::LightManager* const lights = geom::g_LightManager;
                const Vec3& staticSky = lights->getStaticSkyColor();
                *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x1F8)) = staticSky;

                const Vec3& treeSky = lights->getTreeSkyColor();
                Vec3& treeSkyConstant = *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x154));
                treeSkyConstant.x = treeSky.x * 0.5f;
                treeSkyConstant.y = treeSky.y * 0.5f;
                treeSkyConstant.z = treeSky.z * 0.5f;

                *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x214)) =
                    lights->getStaticSpecularColor();
                *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x230)) =
                    lights->getSinglePointColor();
                *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x24C)) =
                    lights->getDynamicPointColor();
                *reinterpret_cast<unsigned int*>(raShaderConstantsAt(0x25C)) =
                    isExpansionPack() ? 1u : 0u;
                *reinterpret_cast<float*>(raShaderConstantsAt(0x264)) =
                    lights->getOccludedSkyNightIntensity();
            }

            // PE 0x1002FFED..0x10030006: with a terrain, its sun colour (+0x78, returned into the
            // argument slot) goes to the constants block's sun colour (sub_100387E0). The shipped
            // body tests only the terrain; the block guard follows the stores above.
            if (g_raShaderConstants && geom::g_Terrain)
            {
                Vec3 sun;
                static_cast<geom::Terrain*>(geom::g_Terrain)->getSunColorRaw(&sun);   // +0x78
                raConstantsSetSunColor(sun);                                          // sub_100387E0
            }

            // the road fade pair into the constants block (sub_10038810), and the underwater fog
            // pair (sub_100F7B00 + sub_1004FDA0) - all constants-block writers, all guarded.
            if (g_raShaderConstants && geom::g_RoadCompiledRenderer)
            {
                const float* const fade = geom::roadCompiledRendererFadeOut();   // retail +0xB0
                raShaderConstantsSetRoadFade(fade[0], fade[1]);                  // sub_10038810
            }

            // the underwater fog pair, the camera's
            // depth below the water plane into PostProductionFrameWork::updateFog, which is what
            // writes the Ra FogRange/FogColor constants and D3DRS_FOGCOLOR every frame. The
            // shipped call is unguarded (ds:0x1023F7C0 straight into ecx); the guard follows this
            // function's convention for its singletons. [CHOSEN guard]
            if (g_raShaderConstants && g_PostProductionFrameWorkOwner)
                postProductionUpdateFog(underwaterDepth());

            if (sw(1693) && m_objectDrawer)
            {
                typedef void (__thiscall* TimeFn)(void*, float);
                reinterpret_cast<TimeFn>((*reinterpret_cast<void***>(m_objectDrawer))[6])(
                    m_objectDrawer, timeF);
            }

            renderQueueFlush(0x30, 1);                       // RSMTZOnly

            // Terrain is available after the level's terrain.create command.
            IBase* terrain = geom::g_Terrain;
            if (sw(1701) && terrain)
            {
                static_cast<geom::Terrain*>(terrain)->renderWorldPassBegin();   // +0xB8
                terrain = geom::g_Terrain;
            }

            // The world pass publishes the renderer-owned accumulation
            // target and its lazily initialized projection vectors, then the current base Hemi map
            // and its four constants. The shipped guard is only byte +0x69D; the old source also
            // tested dword 477 and thereby hid a real live path. Both texture records retain their
            // retail ownership semantics inside RenderQueues.cpp.
            if (sw(1693))
            {
                // PE 0x100300A2..0x100300DD: the base texture of g_Renderer +0x774 into the
                // AccumulationMap record. PE 0x100300DD..0x100301C5: the half texel of the same
                // texture (+0x774 read again, sub_100846A0, call 0x100300F4), then the projection
                // pair, two function-local statics behind bits 0 and 1 of the guard word
                // dword_1023D948 (bit 2 is the PostProduction profiler below): the scale
                // (0.5, -0.5, 1, 1) and the offset (0.5 + half texel x, 0.5 + half texel y, 0, 0),
                // fixed by the first world pass even if the target is later recreated at another
                // size. Both are copied into the constants block on every world pass. The scale
                // has a guard bit in retail because its type has a constructor; here it is a
                // constant aggregate, and nothing can observe the difference.
                raShaderConstantsSetAccumulationMap(
                    static_cast<Renderer*>(g_Renderer)->getHemiScratchTexture());
                const Math::Vec2 halfTexel = geom::textureHalfTexelSize(
                    static_cast<Renderer*>(g_Renderer)->getHemiScratchTexture());
                static const Vec4 accumulationScale = { 0.5f, -0.5f, 1.0f, 1.0f };
                static const Vec4 accumulationOffset =
                    { halfTexel.x + 0.5f, halfTexel.y + 0.5f, 0.0f, 0.0f };
                raShaderConstantsSetAccumulationProjection(&accumulationScale.x,
                                                           &accumulationOffset.x);

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
                if (sw(1703) && g_OverGrowth)
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
                if (sw(1702) && geom::g_UndergrowthSystem)
                    static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->render(timeF);

                if (shouldToggleTransparencyAA())
                    stateManagerSetTransparencyAA(false);

                terrain = geom::g_Terrain;
            }

            // PE 0x100303A7..0x100303BC mirrors the pre-detail Terrain arm at +0xBC.
            if (sw(1701) && terrain)
                static_cast<geom::Terrain*>(terrain)->renderWorldPassEnd();     // +0xBC

            if (sw(1700))
                roadQueuePass();
            if (sw(1700))
            {
                if (geom::g_RoadCompiledRenderer)
                {
                    unsigned char* roadListener = static_cast<unsigned char*>(
                        geom::roadCompiledRendererListener());                   // retail +0x24
                    typedef void (__thiscall* Fn)(void*, int);
                    reinterpret_cast<Fn>((*reinterpret_cast<void***>(roadListener))[3])(
                        roadListener, 0);
                }
                if (geom::g_RoadEditableRenderer)
                {
                    // IRoadEditableRenderer +0x18 drawAll, the slot is
                    // the same in every flavour - IBase's four, then add / remove / drawAll.
                    static_cast<geom::IRoadEditableRenderer*>(geom::g_RoadEditableRenderer)->drawAll();
                }
            }

            if (sw(1714))
            {
                if (geom::g_SkyDome)
                {
                    // The SkyDome's RenderPathListener subobject (retail +0x04), slot +0x0C.
                    RenderPathListener* const skyListener =
                        static_cast<geom::SkyDome*>(geom::g_SkyDome);
                    skyListener->onRenderPathNotification(0);
                    if (sw(1715))
                    {
                        typedef void (__thiscall* SkyFn)(void*, int);
                        reinterpret_cast<SkyFn>(
                            (*reinterpret_cast<void***>(geom::g_SkyDome))[42])(
                            geom::g_SkyDome, 0);
                    }
                }
                else
                    noteOnce(128u, "SkyDome (draw pass)");
            }

            renderQueueFlush(0x3C, 1);                       // RSMTFogPass

            if (sw(1693) && isCameraUnderWater())
            {
                renderQueueFlush(0x08, -1);                  // RSMTTransparency
                renderQueueFlush(0x48, -1);                  // RSMTAux0Transparency
                renderQueueFlush(0x18, -1);                  // RSMTDeltaTransparency
                renderQueueFlush(0x58, -1);                  // RSMTAux1Transparency
            }
            if (sw(1696) && geom::g_DecalManager)
            {
                // The manager's RenderPathListener subobject (retail +0x04), slot +0x0C.
                RenderPathListener* const decalListener = static_cast<geom::DecalManager*>(
                    static_cast<geom::IDecalManager*>(geom::g_DecalManager));
                decalListener->onRenderPathNotification(0);
            }
            // PE 0x100304A5..0x100304C4: under water the additive queue is flushed here, before the
            // particles and the water; above water it waits for the transparency queues below.
            if (sw(1693) && isCameraUnderWater())
                renderQueueFlush(0x2C, 1);                   // RSMTAdditive (under water)
            if (sw(1695) && geom::g_ParticleSystemManager)
            {
                geom::asParticleSystemManager(geom::g_ParticleSystemManager)->onPathDraw(false, true);
            }
            renderQueueFlush(0x28, -1);                      // RSMTWater
            if (sw(1693))
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
            if (sw(1695) && geom::g_ParticleSystemManager)
            {
                geom::asParticleSystemManager(geom::g_ParticleSystemManager)->onPathDraw(false, false);
            }
            renderQueueFlush(0x40, 1);                       // RSMTFogTransparentPass
            if (m_weatherManager && sw(1695))
                geom::asWeatherManager(m_weatherManager)->render();                      // +0x1C
            if (m_rainManager && sw(1695))
                geom::asRainManager(m_rainManager)->render();                            // +0x1C
            if (sw(1714) && sw(1715) && geom::g_SkyDome)
            {
                typedef void (__thiscall* SkyFn)(void*, int);
                reinterpret_cast<SkyFn>((*reinterpret_cast<void***>(geom::g_SkyDome))[42])(
                    geom::g_SkyDome, 1);
            }
            if (sw(1717) && g_PostProductionFrameWorkOwner
                && (!m_postProductionNeedsActive || postProductionActive()))
            {
                postProductionPass();
            }

            if (m_clearDepthBeforeFirstPerson)
            {
                // sub_100057E0(2, 0, 1.0f, 0, 0, 0) - the z-only clear before the first-person
                // passes.
                flushRenderTargetsSeam();
                IDirect3DDevice9* dev = static_cast<RendDX9Base*>(g_rendererBase)->device;
                dev->Clear(0, 0, 2, 0, 1.0f, 0);
            }

            renderQueueFlush(0x0C, m_clearDepthBeforeFirstPerson);              // RSMT1Person
            renderQueueFlush(0x4C, -1);                      // RSMTAux0Person
            renderQueueFlush(0x1C, m_clearDepthBeforeFirstPerson);              // RSMTDelta1Person
            renderQueueFlush(0x5C, -1);                      // RSMTAux1Person
            // PE 0x10030658..0x10030666: the first-person particles follow Aux1Person directly;
            // Aux1PersonTransparency is flushed once, last (0x100306BE).
            if (sw(1695) && m_clearDepthBeforeFirstPerson && geom::g_ParticleSystemManager)
            {
                geom::asParticleSystemManager(geom::g_ParticleSystemManager)->onPathDraw(true, false);
            }
            renderQueueFlush(0x10, m_clearDepthBeforeFirstPerson);              // RSMT1PersonTransparency
            renderQueueFlush(0x50, -1);                      // RSMTAux0PersonTransparency
            renderQueueFlush(0x20, m_clearDepthBeforeFirstPerson);              // RSMTDelta1PersonTransparency
            renderQueueFlush(0x60, -1);                      // RSMTAux1PersonTransparency

            // PE 0x100306D2..0x1003070C: with a multisampled back buffer the overlay passes run
            // without multisampling and without a depth-stencil surface; the surface is put back
            // after the console draw (0x10030AE7..0x10030AFA). The shipped body never releases the
            // reference GetDepthStencilSurface adds; it is dropped after the restore below.
            IDirect3DSurface9* savedDepthStencil = 0;
            if (isMultisampleActive())                                             // sub_1000A6B0
            {
                stateManagerSetRenderStatePersistent(161, 0);    // D3DRS_MULTISAMPLEANTIALIAS
                IDirect3DDevice9* const dev = static_cast<RendDX9Base*>(g_rendererBase)->device;
                dev->GetDepthStencilSurface(&savedDepthStencil);                   // +0xA0
                dev->SetDepthStencilSurface(0);                                    // +0x9C
            }

            // PE 0x10030712..0x100307A0: the profiler is initialized on first use.
            if (sw(1717) && g_PostProductionFrameWorkOwner
                && (!m_postProductionNeedsActive || postProductionActive()))
            {
                static QueueProfiler profile(std::string("PostProduction"));
                profile.begin();
                postProductionFinish();
                QueueProfiler::end();
            }

            if (sw(1704))
            {
                void* nametags = classManager
                    ? classManager->getSingleton(renddx9abi::HostString("NametagManager")) : 0;
                if (nametags)
                {
                    typedef void (__thiscall* Fn)(void*);
                    reinterpret_cast<Fn>((*static_cast<void***>(nametags))[6])(nametags);
                }
            }

            // PE 0x100307EA..0x10030806: sky pass 2 follows the nametags. The shipped call does
            // not null-test the SkyDome global; this function guards it like its other sky calls.
            if (sw(1714) && sw(1715) && geom::g_SkyDome)
            {
                typedef void (__thiscall* SkyFn)(void*, int);
                reinterpret_cast<SkyFn>((*reinterpret_cast<void***>(geom::g_SkyDome))[42])(
                    geom::g_SkyDome, 2);
            }

            if (sw(1713) && m_hudManager)
                hostCalls::hudPaint(m_hudManager);                                     // +0x1C

            if (m_byte6DD)                                                             // +0x6DD
                rendererDrawOverlayTexture(this);                                      // sub_10028FD0
            if (m_performanceTestActive)                                               // +0x888
            {
                // PE 0x1003083D..0x10030867: unsigned counter load, one final float store.
                ++m_performanceTest88C;
#if defined(_MSC_VER) && defined(_M_IX86)
                m_performanceTest890 += g_totalQueueProfiler->counter(0);
#elif defined(_MSC_VER)
                // [CHOSEN, not recovered] Retain the extended sum before the float store.
                m_performanceTest890 = static_cast<float>(
                    static_cast<double>(g_totalQueueProfiler->counter(0)) + m_performanceTest890);
#else
                // [CHOSEN, not recovered] Preserve the x87 intermediate on native processors.
                m_performanceTest890 = bf2x87::store(bf2x87::add(
                    bf2x87::wide(g_totalQueueProfiler->counter(0)),
                    bf2x87::wide(m_performanceTest890)));
#endif
            }

            // The three debug graphs: render-time (ms), FPS, draw calls. PE 0x100308B3
            // samples the total profiler's unsigned counter 0, then stores it as a float.
            {
                typedef void (__thiscall* SampleFn)(void*, float);
                typedef void (__thiscall* FlagFn)(void*, unsigned char);
                void* gRender = m_debugGraphs[1];
                void* gFps    = m_debugGraphs[0];
                void* gDraw   = m_debugGraphs[2];
                if (gRender)
                    reinterpret_cast<SampleFn>((*static_cast<void***>(gRender))[16])(
                        gRender, static_cast<float>(m_frameWallTime * 1000.0));
                if (gFps)
                    reinterpret_cast<SampleFn>((*static_cast<void***>(gFps))[16])(
                        gFps, static_cast<float>(1.0 / m_frameTime));
                if (gDraw)
                    reinterpret_cast<SampleFn>((*static_cast<void***>(gDraw))[16])(
                        gDraw, static_cast<float>(g_totalQueueProfiler->counter(0)));

                // the ECRenderer / ECCore frame events - the posts BF2's own menu system draws
                // from. Delay-zero, so BF2's handlers run inside these calls.
                ::dice::hfe::eventManager->postEvent(
                    ECPhysics, 0, static_cast<void*>(0), 0.0f);
                ::dice::hfe::eventManager->postEvent(
                    ECCore, 5, static_cast<void*>(0), 0.0f);
                ::dice::hfe::eventManager->postEvent(
                    static_cast<EventCategory>(4), 6000, static_cast<void*>(0), 0.0f);

                // PE 0x1003091B..0x10030990: with the DebugRenderer singleton (dword_10247C3C)
                // published, an initialized "AI" singleton gets the AI debug-draw event
                // (category 3, id 4), then the DebugRenderer flushes its queue - all with the
                // base statistics off.
                const unsigned char statsWere = statsSwapEnabled(0);
                if (geom::g_DebugRenderer)
                {
                    ai::IAI* const aiSingleton = static_cast<ai::IAI*>(
                        classManager->getSimpleSingleton(renddx9abi::HostString("AI")));   // +0x58
                    if (aiSingleton && aiSingleton->isInitialized())                       // PE +0x18
                        ::dice::hfe::eventManager->postEvent(static_cast<EventCategory>(3), 4,
                                                             static_cast<void*>(0), 0.0f);
                    static_cast<geom::IDebugRenderer*>(geom::g_DebugRenderer)->flush();   // +0x10
                }
                statsSwapEnabled(statsWere);

                if (m_dword6D4)
                    drawStatisticsOverlay();

                // The FPS average: accumulate frame time and publish frames/seconds once the
                // total is ordered and strictly greater than the binary64 0.33 at 0x101E09B0.
                // Equal, less and unordered all skip publication (`fcom`; test C3|C0).
                m_fpsAccumulatedTime += m_frameTime;
                ++m_fpsAccumulatedFrames;
                if (m_fpsAccumulatedTime > 0.33)
                {
                    m_frameRate = static_cast<double>(m_fpsAccumulatedFrames) / m_fpsAccumulatedTime;
                    m_fpsAccumulatedFrames = 0;
                    m_fpsAccumulatedTime = 0.0;
                }
                if (m_frameRate == 0.0)
                    m_frameRate = 1.0 / m_frameTime;
                for (int bucket = 9; bucket >= 0; --bucket)
                {
                    if (static_cast<double>(bucket) * m_fpsBucketScale < m_frameRate)
                    {
                        ++m_fpsHistogram[bucket];
                        break;
                    }
                }

                // Separate debug flags gate the frame-time and culling text overlays.
                if (m_frameTimeOverlay)                                   // +0x6AC
                    rendererDrawFrameTimeOverlay(this);                   // sub_10029C50
                // Native object vectors shift the fields following them; use the flag member.
                if (g_ObjectDrawer
                    && reinterpret_cast<const geom::ObjectDrawer*>(g_ObjectDrawer)->isCullingDebugEnabled())
                    rendererDrawCullingOverlay(this);                     // sub_10029BB0

                if (gRender)
                    reinterpret_cast<FlagFn>((*static_cast<void***>(gRender))[10])(
                        gRender, sw(1712));
                if (gFps)
                    reinterpret_cast<FlagFn>((*static_cast<void***>(gFps))[10])(
                        gFps, sw(1712));
                if (gDraw)
                    reinterpret_cast<FlagFn>((*static_cast<void***>(gDraw))[10])(
                        gDraw, sw(1712));
            }

            // PE 0x10030AAF..0x10030ABA: the device/state-manager counter overlay.
            if (sw(1706))
                drawStateStatisticsOverlay();

            // Query console activation through its typed interface before drawing the overlay.
            {
                io::OldConsole* const con = reinterpret_cast<io::OldConsole*>(
                    ::dice::hfe::mainConsole);
                const bool consoleOpen = con->getConsoleActive();
                if (consoleOpen && sw(1716))
                    slot_0x58();
            }

            if (isMultisampleActive())                                             // 0x10030AE7
                static_cast<RendDX9Base*>(g_rendererBase)->device->SetDepthStencilSurface(
                    savedDepthStencil);
            // [CHOSEN, not recovered] Not in the shipped body: one leaked reference per frame
            // drawn with multisampling kept the depth-stencil surface, and with it the device,
            // referenced, so the final device release at shutdown (rendererReleaseD3DDevice,
            // RendDX9Init.cpp:738) failed after any level had been drawn with antialiasing.
            if (savedDepthStencil)
                savedDepthStencil->Release();

            if (sw(1695) && geom::g_ParticleSystemManager)
            {
                geom::particleSystemManagerEndFrame(
                    geom::asParticleSystemManager(geom::g_ParticleSystemManager));
            }

            if (g_renderEntryPool)
                *static_cast<unsigned int*>(g_renderEntryPool) = 0;

            endScene();

            // PE 0x10030B29..0x10030B30: the movie frame is captured only after a scene that
            // began; a failed beginScene jumps past it to the present test (0x1002FE46).
            if (movie)
                movie->captureFrame();                                            // sub_10003510
        }

        if (presentGate != 0.0)
            present(2);
    }

    if (movie)
        rendererGetRenderView()->setViewMatrix(savedCamera.m);                    // +0x48, +0x60

    // The GPU-throttle fence ring: issue this frame's fence, block on the one from
    // ring-size frames ago.
    {
        const unsigned int count = m_renderAheadLimit;
        if (count)
        {
            queryIssueEnd(m_frameFences[m_fenceCursor % count]);
            const unsigned int cursor = ++m_fenceCursor;
            queryWaitDone(m_frameFences[(cursor - count) % count]);
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

// renderer.drawFps (console object 0x1005CEB0, accessor 0x1005D000): the +0x6AC overlay mode.
int rendererGetDrawFps()
{
    return static_cast<int>(static_cast<Renderer*>(static_cast<void*>(g_Renderer))->m_frameTimeOverlay);
}

void rendererSetDrawFps(int value)
{
    static_cast<Renderer*>(static_cast<void*>(g_Renderer))->m_frameTimeOverlay =
        static_cast<unsigned int>(value);
}

// [CHOSEN, not recovered] Native arithmetic retains x87 intermediates until the
// listing's float/double/integer stores. MSVC keeps the ordinary source expressions.
#if defined(_MSC_VER)
using ProfilerReal = long double;
#define RD9_PROF_WIDE(v) static_cast<ProfilerReal>(v)
#define RD9_PROF_INT(v) static_cast<int>(v)
#define RD9_PROF_DOUBLE(v) static_cast<double>(v)
#define RD9_PROF_GT(a, b) ((a) > (b))
#else
using ProfilerReal = bf2x87::Real;
#define RD9_PROF_WIDE(v) bf2x87::wide(v)
#define RD9_PROF_INT(v) bf2x87::toIntTruncate(v)
#define RD9_PROF_DOUBLE(v) bf2x87::storeDouble(v)
#define RD9_PROF_GT(a, b) bf2x87::greater((a), (b))
#endif

namespace {

// PE 0x10046E60 / 0x10046EB0; internal expression adapters for their x87 results.
ProfilerReal profilerCounterRatio(QueueProfiler* profile, unsigned int index)
{
    const unsigned int draws = profile->counter(0);
    return draws ? RD9_PROF_WIDE(profile->counter(index)) / RD9_PROF_WIDE(draws)
                 : RD9_PROF_WIDE(0.0f);
}

// PE 0x10046EB0.
ProfilerReal profilerCounterRate(QueueProfiler* profile, unsigned int index)
{
    return RD9_PROF_WIDE(profile->counter(index)) * RD9_PROF_WIDE(profile->framesPerSecond);
}

void profilerText(int x, int y, const std::string& text)
{
    geom::debugTextWriterQueueText(geom::g_DebugTextWriter, x, y, text, 0);
}

// [CHOSEN, not recovered] The base statistics block contains only fixed-width scalars.
// Reach it through the native base member, retaining each retail counter's position in it.
int rendererDeviceStatistic(Renderer* renderer, unsigned int offset)
{
    int value;
    ::memcpy(&value, reinterpret_cast<RendDX9Base*>(renderer)->renderStats + offset - 0x108, 4);
    return value;
}

} // anonymous namespace

// PE 0x100290A0. Queues a heading or one stored profiler row; no pointer guards.
void Renderer::drawProfilerRow(int y, QueueProfiler* profile, bool heading)
{
    const unsigned int draws = profile->counter(0);
    if (m_dword6D4 == 1)
    {
        if (heading)
        {
            static const int x[] = {10,45,90,130,180,220,260,290,330,360,390,415,440,465,490,515,540,565};
            static const char* const labels[] = {
                "draw", "prim", "prim/c", "vert", "vert/c", "us", "us/c", "us(%)",
                "tex", "utex", "vb", "uvb", "ib", "uib", "vsv", "psv", "id", "name"
            };
            for (unsigned int i = 0; i != 18; ++i) profilerText(x[i], y, labels[i]);
            return;
        }
        if (profile->flag1C5) profilerText(4, y, "*");
        profilerText(10, y, formatString("%-3d", static_cast<int>(draws)));
        profilerText(45, y, formatString("%-6d", static_cast<int>(profile->counter(1))));
        profilerText(90, y, formatString("%-4d", RD9_PROF_INT(profilerCounterRatio(profile, 1))));
        profilerText(130, y, formatString("%-6d", static_cast<int>(profile->counter(2))));
        profilerText(180, y, formatString("%-4d", RD9_PROF_INT(profilerCounterRatio(profile, 2))));
        profilerText(220, y, formatString("%-4d",
            RD9_PROF_INT(RD9_PROF_WIDE(profile->time168) * RD9_PROF_WIDE(1000.0f))));
        // The listing uses signed FIDIV here, unlike the unsigned counter-ratio getter.
        profilerText(260, y, formatString("%-4d", draws ? RD9_PROF_INT(
            RD9_PROF_WIDE(profile->time168) * RD9_PROF_WIDE(1000.0f)
            / RD9_PROF_WIDE(static_cast<int>(draws))) : 0));
        profilerText(290, y, formatString("%-3.1f%%", RD9_PROF_DOUBLE(
            RD9_PROF_WIDE(profile->time168) / RD9_PROF_WIDE(g_totalQueueProfiler->time168)
            * RD9_PROF_WIDE(100.0f))));
        static const int resourceX[] = {330,360,390,415,440,465};
        for (unsigned int i = 0; i != 6; ++i)
            profilerText(resourceX[i], y, formatString("%-2d",
                static_cast<int>(profile->resources[i / 2].untouched0C[i % 2])));

        DWORD vertexVersion = 0, pixelVersion = 0;
        if (draws && profile->name != "Total")
        {
            DWORD code[8192];
            UINT bytes = 0x2000;
            if (profile->vertexShader && SUCCEEDED(profile->vertexShader->GetFunction(code, &bytes)))
                vertexVersion = D3DXGetShaderVersion(code);
            bytes = 0x2000;
            if (profile->pixelShader && SUCCEEDED(profile->pixelShader->GetFunction(code, &bytes)))
                pixelVersion = D3DXGetShaderVersion(code);
        }
        profilerText(490, y, formatString("%d.%d", (vertexVersion >> 8) & 255, vertexVersion & 255));
        profilerText(515, y, formatString("%d.%d", (pixelVersion >> 8) & 255, pixelVersion & 255));
        profilerText(540, y, formatString(profile->flag1C4 ? "%-2d" : "[%-2d]",
                                          static_cast<int>(profile->id)));
        profilerText(565, y, std::string(profile->name.c_str()));
        return;
    }

    int x = 10;
    if (heading)
    {
        profilerText(x, y, "draw");
        // shipped defect: the general table skips primitives (counter 1).
        for (unsigned int i = 2; i != 16; ++i)
        {
            x += 40;
            profilerText(x, y, profile->counterName(i));
        }
        profilerText(x + 40, y, "id");
        profilerText(x + 70, y, "name");
        return;
    }
    if (profile->flag1C5) profilerText(4, y, "*");
    profilerText(x, y, formatString("%-3d", static_cast<int>(profile->counter(0))));
    for (unsigned int i = 2; i != 16; ++i)
    {
        if (m_dword6D4 == 2)
        {
            x += 40;
            profilerText(x, y, formatString("%-4d", static_cast<int>(profile->counter(i))));
        }
        else if (m_dword6D4 == 3)
        {
            x += 40;
            profilerText(x, y, formatString("%-1.2f", RD9_PROF_DOUBLE(profilerCounterRatio(profile, i))));
        }
        else if (m_dword6D4 == 4)
        {
            x += 40;
            profilerText(x, y, formatString("%-1.1f", RD9_PROF_DOUBLE(profilerCounterRate(profile, i))));
        }
    }
    profilerText(x + 40, y, formatString(profile->flag1C4 ? "%-2d" : "[%-2d]",
                                         static_cast<int>(profile->id)));
    profilerText(x + 70, y, profile->name);
}

// PE 0x1002E440.
void Renderer::drawStatisticsOverlay()
{
    const unsigned int colour = geom::fontPortedGetColour(static_cast<IBase*>(m_consoleFont));
    const unsigned int textColour = reinterpret_cast<RendDX9Base*>(this)->debugTextColour;
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, textColour);
    profilerText(200, 40, formatString("numDrawsPerSecond:        %d",
        RD9_PROF_INT(profilerCounterRate(g_totalQueueProfiler, 0))));
    profilerText(200, 50, formatString("numOfPrimitivesPerSecond: %dk",
        RD9_PROF_INT(profilerCounterRate(g_totalQueueProfiler, 1)) / 1024));
    profilerText(200, 60, formatString("numOfVerticesPerSecond:   %dk",
        RD9_PROF_INT(profilerCounterRate(g_totalQueueProfiler, 2)) / 1024));
    m_frameTimeHistory[m_frameTimeHistoryIndex] = static_cast<float>(m_frameTime);
    m_frameTimeHistoryIndex = (m_frameTimeHistoryIndex + 1) % 32;
    profilerText(10, 40, formatString("fps: %f", static_cast<double>(g_totalQueueProfiler->framesPerSecond)));
    profilerText(10, 50, formatString("frame tot time: %.2f ms", RD9_PROF_DOUBLE(
        RD9_PROF_WIDE(1000.0f) / RD9_PROF_WIDE(g_totalQueueProfiler->framesPerSecond))));
    profilerText(10, 60, formatString("managers tot time: %.2f ms (%2.1f%%)",
        static_cast<double>(g_totalQueueProfiler->time168), RD9_PROF_DOUBLE(
            RD9_PROF_WIDE(g_totalQueueProfiler->framesPerSecond)
            * RD9_PROF_WIDE(g_totalQueueProfiler->time168) * RD9_PROF_WIDE(0.1f))));
    static const char* const memoryLabels[] = {
        "textureMem: %1.1f Mb", "vbMem: %1.1f Mb", "ibMem: %1.1f Mb", "totMem: %1.1f Mb"
    };
    for (unsigned int i = 0; i != 4; ++i)
        profilerText(400, 40 + 10 * i, formatString(memoryLabels[i], RD9_PROF_DOUBLE(
            RD9_PROF_WIDE(i == 3 ? resourceTrackerTotalBytes() : resourceTrackerBytes(i))
            * RD9_PROF_WIDE(0.00000095367431640625f))));
    static const char* const sceneLabels[] = {
        "textureSceneMem: %1.1f Mb", "vbSceneMem: %1.1f Mb", "ibSceneMem: %1.1f Mb", "totSceneMem: %1.1f Mb"
    };
    for (unsigned int i = 0; i != 4; ++i)
        profilerText(400, 90 + 10 * i, formatString(sceneLabels[i], RD9_PROF_DOUBLE(
            RD9_PROF_WIDE(i == 3 ? g_totalQueueProfiler->untouched25C
                                : g_totalQueueProfiler->resources[i].untouched0C[2])
            * RD9_PROF_WIDE(0.00000095367431640625f))));
    static const char* const modes[] = {
        "Misc stats", "Stats per frame", "Stats per draw call", "Stats per second"
    };
    if (m_dword6D4 >= 1 && m_dword6D4 <= 4)
        profilerText(10, 140, modes[m_dword6D4 - 1]);
    drawProfilerRow(150, g_totalQueueProfiler, true);
    {
        std::vector<QueueProfiler*> rows(g_activeQueueProfilers->begin() + 2, g_activeQueueProfilers->end());
        // PE 0x10032230: enabled first, then case-insensitive ascending name.
        std::sort(rows.begin(), rows.end(), [](QueueProfiler* a, QueueProfiler* b) {
            return a->flag1C4 != b->flag1C4 ? a->flag1C4 > b->flag1C4
                : ::_stricmp(a->name.c_str(), b->name.c_str()) < 0;
        });
        rows.insert(rows.begin(), (*g_activeQueueProfilers)[0]);
        rows.push_back((*g_activeQueueProfilers)[1]);
        unsigned int row = 15;
        int y = 150;
        for (std::vector<QueueProfiler*>::iterator it = rows.begin(); it != rows.end(); ++it)
        {
            // FCOMP/test AH,41 includes unordered values as well as <= 2.
            if (!RD9_PROF_GT(RD9_PROF_WIDE((*it)->milliseconds1C0), RD9_PROF_WIDE(2.0f)))
            {
                geom::debugTextWriterSetColour(geom::g_DebugTextWriter,
                                               row & 1 ? textColour : textColour ^ 0x404040);
                ++row;
                y += 10;
                drawProfilerRow(y, *it, false);
            }
        }
    }
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, colour);
}

#undef RD9_PROF_WIDE
#undef RD9_PROF_INT
#undef RD9_PROF_DOUBLE
#undef RD9_PROF_GT

// PE 0x1002C590. Font height is read once per column, including an empty column.
void Renderer::drawTextColumn(int x, int y, const std::vector<std::string>& lines, int spacing)
{
    const int step = geom::fontPortedGetHeight(static_cast<IBase*>(m_consoleFont)) + spacing + 1;
    for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end(); ++it)
    {
        profilerText(x, y, *it);
        y += step;
    }
}

// PE 0x1002EAE0.
void Renderer::drawStateStatisticsOverlay()
{
    static std::vector<std::string> headings;
    if (headings.empty())
    {
        static const char* const labels[] = {
            "Area:", "SetVB", "SetIB", "SetVDecl", "SetTex", "SetRT", "SetDSS",
            "SetRS", "SetTSS", "SetSS", "SetVS", "SetVSConsts", "SetPS", "SetPSConsts"
        };
        for (unsigned int i = 0; i != 14; ++i) headings.push_back(labels[i]);
    }
    const auto stat = [this](unsigned int offset) { return rendererDeviceStatistic(this, offset); };
    const auto column = [&stat](std::vector<std::string>& lines, unsigned int first) {
        lines.push_back(formatString("%i", stat(first)));
        lines.push_back(formatString("%i", stat(first + 4)));
        lines.push_back(formatString("%i", stat(first + 8)));
        lines.push_back(formatString("%i (%i was NULL)", stat(first + 12), stat(first + 16)));
        lines.push_back(formatString("%i (%i was NULL)", stat(first + 20), stat(first + 24)));
        for (unsigned int offset = 28; offset != 48; offset += 4)
            lines.push_back(formatString("%i", stat(first + offset)));
        lines.push_back(formatString("%i (%i registers)", stat(first + 48), stat(first + 52)));
        lines.push_back(formatString("%i", stat(first + 56)));
        lines.push_back(formatString("%i (%i registers)", stat(first + 60), stat(first + 64)));
    };
    std::vector<std::string> states;
    states.push_back("->StateMgr:");
    column(states, 0x240);
    std::vector<std::string> device;
    device.push_back("->D3DDevice:");
    column(device, 0x1E0);
    const int indexed = stat(0x228);
    const int total = stat(0x20C) + indexed + stat(0x1FC) + stat(0x230) + stat(0x21C)
        + stat(0x200) + stat(0x1EC) + stat(0x238) + stat(0x210) + stat(0x204)
        + stat(0x1E4) + stat(0x1E0) + stat(0x218) + stat(0x208) + stat(0x1F4) + stat(0x1E8);
    std::vector<std::string> summary;
    summary.push_back(formatString("DIPs:  %i (%i primitives)", indexed, stat(0x22C)));
    summary.push_back(formatString("DPs:   %i (%i primitives)", stat(0x230), stat(0x234)));
    summary.push_back(formatString("DPUPs: %i (%i primitives)", stat(0x238), stat(0x23C)));
    summary.push_back("");
    summary.push_back(formatString("Total prims:      %i", stat(0x224)));
    summary.push_back(formatString("Total d3d calls:  %i", total));
    const unsigned char enabled = statsSwapEnabled(0);
    const unsigned int colour = geom::fontPortedGetColour(static_cast<IBase*>(m_consoleFont));
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter,
                                  reinterpret_cast<RendDX9Base*>(this)->debugTextColour);
    drawTextColumn(6, 18, headings, 0);
    drawTextColumn(106, 18, states, 0);
    drawTextColumn(216, 18, device, 0);
    drawTextColumn(6, 194, summary, 0);
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, colour);
    statsSwapEnabled(enabled);
}

// sub_10029C50 - the frame-time overlay. With the base statistics switched off, the
// DebugTextWriter takes the base debug-text colour (+0x1C); mode 1 queues "fps / ms" at (6, 6),
// which DebugTextWriterManager draws before EndScene. The writer then gets the console font's
// colour back. Neither global is null-tested.
void rendererDrawFrameTimeOverlay(Renderer* renderer)
{
    const unsigned char statsWere = statsSwapEnabled(0);                       // base +0x108
    IBase* const font = static_cast<IBase*>(renderer->m_consoleFont);          // +0x5CC
    const unsigned int fontColour = geom::fontPortedGetColour(font);
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter,
                                   reinterpret_cast<RendDX9Base*>(renderer)->debugTextColour);
    if (renderer->m_frameTimeOverlay == 1)                                     // +0x6AC
    {
        const double fps = renderer->m_frameRate;                              // +0x600
        const std::string text = formatString("%.1f / %.1f", fps, 1000.0 / fps);
        geom::debugTextWriterQueueText(geom::g_DebugTextWriter, 6, 6, text, 0);
    }
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, fontColour);
    statsSwapEnabled(statsWere);
}

// sub_10029BB0 - the culling overlay, run by renderFrame while the ObjectDrawer's debug byte (+0x70)
// is set: with the base statistics switched off and the writer in the base debug-text colour, it
// queues "Culled: %i / %i / %i / %i" with the ObjectDrawer's +0x74, +0x78, +0x80 and +0x84 counters
// at (6, 48), then restores the console font's colour.
void rendererDrawCullingOverlay(Renderer* renderer)
{
    const unsigned char statsWere = statsSwapEnabled(0);                       // base +0x108
    IBase* const font = static_cast<IBase*>(renderer->m_consoleFont);          // +0x5CC
    const unsigned int fontColour = geom::fontPortedGetColour(font);           // +0x34
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter,
                                   reinterpret_cast<RendDX9Base*>(renderer)->debugTextColour);
    const geom::ObjectDrawer* const drawer =
        reinterpret_cast<const geom::ObjectDrawer*>(g_ObjectDrawer);           // dword_1025A39C
    const std::string text = formatString("Culled: %i / %i / %i / %i",
                                          drawer->m_culledCount,
                                          static_cast<int>(drawer->m_dword78),
                                          static_cast<int>(drawer->m_dword80),
                                          static_cast<int>(drawer->m_dword84));
    geom::debugTextWriterQueueText(geom::g_DebugTextWriter, 6, 0x30, text, 0);  // +0x40
    geom::debugTextWriterSetColour(geom::g_DebugTextWriter, fontColour);
    statsSwapEnabled(statsWere);
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
    typedef void (__thiscall* SetViewFn)(void*, void*);
    reinterpret_cast<SetViewFn>((*static_cast<void***>(drawer))[0x28 / 4])(
        drawer, view);
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
    Renderer* const raw = static_cast<Renderer*>(g_rendererBase);
    if (raw->sw(1693))
        objectDrawerSetRenderView(raw->m_objectDrawer, view);           // +0x5C0 (1472)
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
    Renderer* const raw = static_cast<Renderer*>(g_rendererBase);
    IDirect3DSurface9*& fallback = static_cast<RendDX9Base*>(g_rendererBase)->tempBackBuffer;
    fallback = texture ? textureGetLevelSurface(texture, 0, 0x7FFFFFFFu) : 0;

    // sub_10005920(0, 0, 0, 0x7fffffff): with a null texture argument the renderer takes +0xF4,
    // AddRefs it around StateManager +0x60, then releases that temporary reference.
    IDirect3DSurface9* selected = fallback;
    if (!selected)
        selected = raw->m_backBuffer;                                   // +0x76C
    if (selected)
        selected->AddRef();
    stateManagerSetRenderTarget(0, selected);
    if (selected)
        selected->Release();
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

// PE 0x10009F00.
int rendererGetHemiRenderMode()
{
    return static_cast<RendDX9Base*>(g_rendererBase)->renderMode;
}

// PE 0x10009EB0: signed 0..5 range test; modes 2 and 3 set render state 8 to 2, the others to 3, then store the mode.
void rendererSetHemiRenderMode(int mode)
{
    if (static_cast<unsigned int>(mode) <= 5u)
    {
        stateManagerSetRenderState(8u, (mode == 2 || mode == 3) ? 2u : 3u);
        static_cast<RendDX9Base*>(g_rendererBase)->renderMode = mode;
    }
}

void rendererSetHemiRoadPass(bool enabled)
{
    static_cast<Renderer*>(g_rendererBase)->sw(1700) = enabled ? 1u : 0u;
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
    return (*static_cast<void***>(geometry))[offset / 4];
}

inline geom::IGeometryTemplate* lodGeometryTemplate(void* geometry)                    // +0x38
{
    typedef void* (__thiscall* GetTemplateFn)(void*);
    return static_cast<geom::IGeometryTemplate*>(
        reinterpret_cast<GetTemplateFn>(lodGeometrySlot(geometry, 0x38))(geometry));
}

// The clamp all four arms inline.
void forceGeometryLod(void* geometry, int lod)
{
    typedef int (__thiscall* GetLodCountFn)(void*);
    typedef void (__thiscall* SetLodFn)(void*, int);
    int selected =
        reinterpret_cast<GetLodCountFn>(lodGeometrySlot(geometry, 0x80))(geometry) - 1;  // +0x80
    if (selected >= lod)
        selected = lod;
    reinterpret_cast<SetLodFn>(lodGeometrySlot(geometry, 0x78))(geometry, selected);     // +0x78
}

// One template's live-instance set, in key order (the set iterator's increment: inlined for
// StaticMesh and SkinnedMesh, sub_1009F6E0 for BundledMesh). The set's key IS the instance pointer.
template <typename InstanceSet>
void forceInstanceSetLod(const InstanceSet& instances, int lod)
{
    for (typename InstanceSet::const_iterator it = instances.begin(); it != instances.end(); ++it)
        forceGeometryLod(*it, lod);
}

// ObjectManager +0x44, getObjectsFromGrid(centre, radius, out, predicator) - the query setLods makes
// (0x1002D3A5). Split as hostCalls::objectManagerQueryFrustum is: the recovered declaration in the
// native flavour, whose result is this toolchain's std::vector; the retail slot in the compatibility
// flavour, handed the VC7.1 vector object - its unwritten allocator word first - the retail host fills.
void queryObjectsInRadius(const Vec3& centre, float radius, std::vector<world::IObject*>& out)
{
    hemiObjectManager()->getObjectsFromGrid(centre, radius, out, 0);
}

// The streaming scan's touch walk (sub_1002DF20, 0x1002E043..0x1002E0F1). For every object the
// frustum query returned that has a geometry (object +0x38) whose template (geometry +0x38) is a
// static, skinned or bundled mesh template: when both of the geometry's frame stamps (+0x20 drawn,
// +0x24 visible) are more than +0x748 frames old, the geometry is stamped visible (+0xF0) and
// counted (+0x74C); when the template's own stamp (+0x08) is that old too, the template preloads
// its device resources (+0xAC, which re-stamps it) and is counted (+0x750). Unsigned comparisons.
void streamTouchGeometry(Renderer* renderer)
{
    const unsigned int ordinal = reinterpret_cast<RendDX9Base*>(renderer)->frameOrdinal;   // +0x104
    for (std::vector<world::IObject*>::const_iterator it = renderer->m_streamObjects.begin();
         it != renderer->m_streamObjects.end(); ++it)
    {
        void* const geometry = world::object_detail::LayoutAccess::geometry(*it);           // +0x38
        if (!geometry)
            continue;
        geom::IGeometryTemplate* const geometryTemplate = lodGeometryTemplate(geometry);    // +0x38
        const unsigned int classId = geometryTemplate->getClassId();                         // +0x10
        if (classId != kStaticMeshTemplateClassId && classId != kSkinnedMeshTemplateClassId &&
            classId != kBundledMeshTemplateClassId)
            continue;

        const geom::GeometryInstanceData* const data =
            static_cast<const geom::GeometryInstanceData*>(geometry);
        if (data->m_drawFrame + renderer->m_dword748 >= ordinal)                            // +0x20
            continue;
        if (data->m_visibleFrame + renderer->m_dword748 >= ordinal)                         // +0x24
            continue;

        typedef void (__thiscall* StampFn)(void*);
        reinterpret_cast<StampFn>(lodGeometrySlot(geometry, 0xF0))(geometry);                // +0xF0
        ++renderer->m_dword74C;

        geom::GeometryTemplate* const stamped = static_cast<geom::GeometryTemplate*>(geometryTemplate);
        if (static_cast<unsigned int>(stamped->m_dword08) + renderer->m_dword748 >= ordinal) // +0x08
            continue;
        geometryTemplate->releaseDeviceResources();                                          // +0xAC
        ++renderer->m_dword750;
    }
}

} // anonymous namespace

// PE 0x1002D140
void rendererSetLods(int lod, unsigned int classId)
{
    geom::GeometryTemplateRegistry* const templates =
        static_cast<geom::IGeometryTemplateManager*>(
            static_cast<void*>(geom::g_GeometryTemplateManager))->getTemplateRegistry();   // +0x60
    for (geom::GeometryTemplateRegistry::const_iterator node = templates->begin();
         node != templates->end(); ++node)
    {
        if (node->second->getClassId() != classId)                                          // +0x10
            continue;
        geom::IGeometryTemplate* const geometryTemplate = node->second;                     // +0x28
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
            ::renddx9abi::Debug(DebugTypeError,
                "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\RendDX9\\BF2Render.cpp", 3315, "")
                << "unknown CID for setLods";
    }

    Vec3 origin;
    origin.x = 0.0f;
    origin.y = 0.0f;
    origin.z = 0.0f;
    std::vector<world::IObject*> objects;
    queryObjectsInRadius(origin, 10000.0f, objects);
    for (std::vector<world::IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        void* const geometry = world::object_detail::LayoutAccess::geometry(*it);           // +0x38
        if (geometry && lodGeometryTemplate(geometry)->getClassId() == classId)
            forceGeometryLod(geometry, lod);
    }
}

// (29 B each, __thiscall on the renderer), the
// forced lod stored at Renderer +0x7D8 / +0x7DC / +0x7E0 (dwords 502 / 503 / 504), then setLods for
// the class. The StaticMesh one is the ground-hemi bake's (0x1012B1B3 restore, 0x1012B4B1 apply); the
// other two are the `renderer.forceSkinnedMeshLod` / `forceBundledMeshLod` SET arms (0x1005945B /
// 0x100595FB), whose GET arms read the stored dword back.
// PE 0x1002D460
void rendererSetStaticMeshLod(int lod)
{
    static_cast<Renderer*>(g_rendererBase)->m_forcedLod[0] = lod;          // +0x7D8 (2008)
    rendererSetLods(lod, kStaticMeshTemplateClassId);
}

// PE 0x1002D480
void rendererSetSkinnedMeshLod(int lod)
{
    static_cast<Renderer*>(g_rendererBase)->m_forcedLod[1] = lod;          // +0x7DC (2012)
    rendererSetLods(lod, kSkinnedMeshTemplateClassId);
}

// PE 0x1002D4A0
void rendererSetBundledMeshLod(int lod)
{
    static_cast<Renderer*>(g_rendererBase)->m_forcedLod[2] = lod;          // +0x7E0 (2016)
    rendererSetLods(lod, kBundledMeshTemplateClassId);
}

int rendererGetSkinnedMeshLod()
{
    return static_cast<const Renderer*>(g_rendererBase)->m_forcedLod[1];
}

int rendererGetBundledMeshLod()
{
    return static_cast<const Renderer*>(g_rendererBase)->m_forcedLod[2];
}

// renderer.globalStaticMeshLodDistanceScale / globalBundleMeshLodDistanceScale /
// globalSkinnedMeshLodDistanceScale (ctors 0x1005A4B0 / 0x1005A650 / 0x1005A7F0, arms 0x1005A600 /
// 0x1005A7A0 / 0x1005A940): the renderer floats +0x8B0 / +0x8B4 / +0x8B8, read and stored as is.
float rendererGetGlobalLodDistanceScale(int which)
{
    const Renderer* const r = static_cast<const Renderer*>(static_cast<const void*>(g_Renderer));
    return which == 0 ? r->m_lodScale8B0 : (which == 1 ? r->m_lodScale8B4 : r->m_lodScale8B8);
}

void rendererSetGlobalLodDistanceScale(int which, float value)
{
    Renderer* const r = static_cast<Renderer*>(static_cast<void*>(g_Renderer));
    if (which == 0)
        r->m_lodScale8B0 = value;
    else if (which == 1)
        r->m_lodScale8B4 = value;
    else
        r->m_lodScale8B8 = value;
}

// renderer.drawHud (ctor 0x1005CB70, arms 0x1005CCC0): the byte the HudManager paint call and
// hudManagerFramePrep's hudSetPaint read; the SET arm stores the parsed bool unchanged.
bool rendererGetDrawHud()
{
    return static_cast<Renderer*>(g_rendererBase)->sw(1713) != 0;
}

void rendererSetDrawHud(bool on)
{
    static_cast<Renderer*>(g_rendererBase)->sw(1713) = on;
}

// The renderer.draw* switch properties (RendererConsole.cpp): each executor reads or stores one
// renderer byte as is (for example drawTerrain 0x10054ED0 on +0x6A5, drawConsole 0x1005CE60 on
// +0x6B4). draw1pMeshes (0x100542D0) is the +0x5A0 byte.
bool rendererGetRenderSwitch(unsigned int retailOffset)
{
    Renderer* const r = static_cast<Renderer*>(g_rendererBase);
    return (retailOffset == 0x5A0u ? r->m_clearDepthBeforeFirstPerson : r->sw(retailOffset)) != 0;
}

void rendererSetRenderSwitch(unsigned int retailOffset, bool on)
{
    Renderer* const r = static_cast<Renderer*>(g_rendererBase);
    if (retailOffset == 0x5A0u)
        r->m_clearDepthBeforeFirstPerson = on;
    else
        r->sw(retailOffset) = on;
}

// The switch bytes, by retail offset: +0x69C..+0x6AB and +0x6B0..+0x6B7.
unsigned char* Renderer::renderSwitch(unsigned int retailOffset)
{
    if (retailOffset >= 0x69Cu && retailOffset < 0x6ACu)
        return &m_renderSwitches69C[retailOffset - 0x69Cu];
    if (retailOffset >= 0x6B0u && retailOffset < 0x6B8u)
        return &m_renderSwitches6B0[retailOffset - 0x6B0u];
    return 0;
}

unsigned char& Renderer::sw(unsigned int retailOffset)
{
    unsigned char* const field = renderSwitch(retailOffset);
    if (!field)
    {
        RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer::sw (a retail byte offset that names no "
                        "render switch)", this, retailOffset);
        std::abort();
    }
    return *field;
}

namespace {

// [CHOSEN, not recovered] One row per mapped plain field of the Renderer's tail (issue #7): its
// retail x86 offset and size, its value kind and element size, and its native offset. An array
// row covers every element (`retail + i * element`). Pointer, container and listener members are
// not plain fields and are reached through their own typed accessors.
struct TailFieldRow
{
    unsigned short       retailOffset;
    unsigned short       retailBytes;
    Renderer::TailKind   kind;
    unsigned char        elementBytes;
    size_t               nativeOffset;
};

#define RD9_TAIL_ROW(member, retail, kind, element) \
    { retail, static_cast<unsigned short>(sizeof(static_cast<Renderer*>(0)->member)), \
      Renderer::kind, element, offsetof(Renderer, member) }

const TailFieldRow kTailFields[] =
{
    RD9_TAIL_ROW(m_currentRenderPath,       0x544, TailInt,    4),
    RD9_TAIL_ROW(m_viewMatrix,              0x54C, TailFloat,  4),
    RD9_TAIL_ROW(m_f58C,                    0x58C, TailFloat,  4),
    RD9_TAIL_ROW(m_f590,                    0x590, TailFloat,  4),
    RD9_TAIL_ROW(m_near,                    0x594, TailFloat,  4),
    RD9_TAIL_ROW(m_far,                     0x598, TailFloat,  4),
    RD9_TAIL_ROW(m_f59C,                    0x59C, TailFloat,  4),
    RD9_TAIL_ROW(m_clearDepthBeforeFirstPerson, 0x5A0, TailByte, 1),
    RD9_TAIL_ROW(m_postProductionNeedsActive,   0x5A1, TailByte, 1),
    RD9_TAIL_ROW(m_renderAheadLimit,        0x5B8, TailInt,    4),
    RD9_TAIL_ROW(m_fenceCursor,             0x5BC, TailInt,    4),
    RD9_TAIL_ROW(m_consoleOverlayReady,     0x5C4, TailByte,   1),
    RD9_TAIL_ROW(m_frameTime,               0x5E8, TailDouble, 8),
    RD9_TAIL_ROW(m_frameRate,               0x600, TailDouble, 8),
    RD9_TAIL_ROW(m_fpsAccumulatedTime,      0x608, TailDouble, 8),
    RD9_TAIL_ROW(m_fpsAccumulatedFrames,    0x610, TailInt,    4),
    RD9_TAIL_ROW(m_renderSwitches69C,       0x69C, TailByte,   1),
    RD9_TAIL_ROW(m_frameTimeOverlay,        0x6AC, TailInt,    4),
    RD9_TAIL_ROW(m_renderSwitches6B0,       0x6B0, TailByte,   1),
    RD9_TAIL_ROW(m_fullSizeRenderTargets,   0x6BC, TailByte,   1),
    RD9_TAIL_ROW(m_fogOverride,             0x6C0, TailFloat,  4),
    RD9_TAIL_ROW(m_raPath,                  0x6CC, TailInt,    4),
    RD9_TAIL_ROW(m_byte6D0,                 0x6D0, TailByte,   1),
    RD9_TAIL_ROW(m_dword6D4,                0x6D4, TailInt,    4),
    RD9_TAIL_ROW(m_byte6DC,                 0x6DC, TailByte,   1),
    RD9_TAIL_ROW(m_byte6DD,                 0x6DD, TailByte,   1),
    RD9_TAIL_ROW(m_f6FC,                    0x6FC, TailFloat,  4),
    RD9_TAIL_ROW(m_meshShaderLodEnabled,    0x708, TailByte,   1),
    RD9_TAIL_ROW(m_lodDistances70C,         0x70C, TailFloat,  4),
    RD9_TAIL_ROW(m_lodDistances71C,         0x71C, TailFloat,  4),
    RD9_TAIL_ROW(m_lodDistances72C,         0x72C, TailFloat,  4),
    RD9_TAIL_ROW(m_f73C,                    0x73C, TailFloat,  4),
    RD9_TAIL_ROW(m_f740,                    0x740, TailFloat,  4),
    RD9_TAIL_ROW(m_streamBudget,            0x744, TailFloat,  4),
    RD9_TAIL_ROW(m_dword748,                0x748, TailInt,    4),
    RD9_TAIL_ROW(m_dword74C,                0x74C, TailInt,    4),
    RD9_TAIL_ROW(m_dword750,                0x750, TailInt,    4),
    RD9_TAIL_ROW(m_expansionPack,           0x764, TailByte,   1),
    RD9_TAIL_ROW(m_dword768,                0x768, TailInt,    4),
    RD9_TAIL_ROW(m_forcedLod,               0x7D8, TailInt,    4),
    RD9_TAIL_ROW(m_lodSkipBase,             0x7E4, TailInt,    4),
    RD9_TAIL_ROW(m_dword7E8,                0x7E8, TailInt,    4),
    RD9_TAIL_ROW(m_lodSkipBias,             0x7EC, TailInt,    4),
    RD9_TAIL_ROW(m_frameHookEnabled,        0x7F0, TailByte,   1),
    RD9_TAIL_ROW(m_fpsBucketScale,          0x820, TailFloat,  4),
    RD9_TAIL_ROW(m_fpsHistogram,            0x824, TailInt,    4),
    RD9_TAIL_ROW(m_lodScale84C,             0x84C, TailFloat,  4),
    RD9_TAIL_ROW(m_dword860,                0x860, TailInt,    4),
    RD9_TAIL_ROW(m_f864,                    0x864, TailFloat,  4),
    RD9_TAIL_ROW(m_performanceTestActive,   0x888, TailByte,   1),
    RD9_TAIL_ROW(m_performanceTest88C,      0x88C, TailInt,    4),
    RD9_TAIL_ROW(m_performanceTest890,      0x890, TailFloat,  4),
    RD9_TAIL_ROW(m_frameStartTime,          0x8A0, TailDouble, 8),
    RD9_TAIL_ROW(m_frameWallTime,           0x8A8, TailDouble, 8),
    RD9_TAIL_ROW(m_lodScale8B0,             0x8B0, TailFloat,  4),
    RD9_TAIL_ROW(m_lodScale8B4,             0x8B4, TailFloat,  4),
    RD9_TAIL_ROW(m_lodScale8B8,             0x8B8, TailFloat,  4),
};

#undef RD9_TAIL_ROW

Renderer* publishedRenderer()
{
    return static_cast<Renderer*>(static_cast<void*>(g_Renderer));
}

// The accessors' common stop: the offset or the value kind names no mapped field.
void* requireTailField(unsigned int retailOffset, Renderer::TailKind kind, unsigned int bytes,
                       const char* accessor)
{
    Renderer* const renderer = publishedRenderer();
    void* const field = renderer->tailField(retailOffset, kind, bytes);
    if (!field)
    {
        ::renddx9abi::logf("[FRONTIER] dice::hfe::dx9rend::%s(0x%X): no mapped Renderer tail field "
                           "of that type at this retail offset", accessor, retailOffset);
        RD9_FRONTIER_AT("dice::hfe::dx9rend::Renderer tail accessor (an unmapped retail offset)",
                        renderer, retailOffset);
        std::abort();
    }
    return field;
}

} // anonymous namespace

void* Renderer::tailField(unsigned int retailOffset, TailKind kind, unsigned int bytes)
{
    // [CHOSEN, not recovered] Native member access for fields already attested in the PE.
    // The base constructor writes the occlusion ages at 0x1000BA72 / 0x1000BA78;
    // xpLightBlendTick (0x1002B8E0) reads and writes the blend and its three times.
    if (kind == TailInt && bytes == 4)
    {
        RendDX9Base* const base = reinterpret_cast<RendDX9Base*>(this);
        if (retailOffset == 0x3EC) return &base->occlusionTerrainMaxFrames;
        if (retailOffset == 0x3F0) return &base->occlusionObjectsMaxFrames;
    }
    if (kind == TailFloat && bytes == 4 && retailOffset == 0x8BC)
        return &m_xpLightLevel;
    if (kind == TailDouble && bytes == 8)
    {
        if (retailOffset == 0x8C0) return &m_xpLightRiseTime;
        if (retailOffset == 0x8C8) return &m_xpLightFallTime;
        if (retailOffset == 0x8D0) return &m_xpLightMarkTime;
    }
    for (unsigned int i = 0; i != sizeof(kTailFields) / sizeof(kTailFields[0]); ++i)
    {
        const TailFieldRow& row = kTailFields[i];
        const unsigned int first = row.retailOffset;
        const unsigned int end = first + row.retailBytes;
        if (retailOffset < first || retailOffset >= end)
            continue;
        const unsigned int within = retailOffset - first;
        if (row.kind != kind || row.elementBytes != bytes || (within % bytes) != 0)
            return 0;
        return reinterpret_cast<unsigned char*>(this) + row.nativeOffset + within;
    }
    return 0;
}

// [CHOSEN, not recovered] Owner-TU seams over the Renderer's unrecovered tail (issue #7): other
// subsystems name these fields through BF2Render.cpp, which alone knows where they sit. Each maps
// the retail x86 offset onto the named member (the identical address on x86). Callers keep their
// own preconditions; none of these tests the Renderer singleton.
unsigned char& rendererRenderSwitch(unsigned int retailOffset)
{
    return publishedRenderer()->sw(retailOffset);
}

unsigned char& rendererTailByte(unsigned int retailOffset)
{
    return *static_cast<unsigned char*>(
        requireTailField(retailOffset, Renderer::TailByte, 1, "rendererTailByte"));
}

int& rendererTailInt(unsigned int retailOffset)
{
    return *static_cast<int*>(
        requireTailField(retailOffset, Renderer::TailInt, 4, "rendererTailInt"));
}

float& rendererTailFloat(unsigned int retailOffset)
{
    return *static_cast<float*>(
        requireTailField(retailOffset, Renderer::TailFloat, 4, "rendererTailFloat"));
}

double& rendererTailDouble(unsigned int retailOffset)
{
    return *static_cast<double*>(
        requireTailField(retailOffset, Renderer::TailDouble, 8, "rendererTailDouble"));
}

unsigned int rendererRaPath()
{
    return publishedRenderer()->m_raPath;                                    // +0x6CC
}

void* rendererFrameTimer()
{
    return publishedRenderer()->m_frameTimer;                                // +0x5DC
}

IDirect3DSurface9* rendererBackBufferSurface()
{
    return publishedRenderer()->m_backBuffer;                                // +0x76C
}

IDirect3DSurface9* rendererDepthStencilSurface()
{
    return publishedRenderer()->m_depthStencil;                              // +0x770
}

EngineTexture* rendererPathTarget(unsigned int retailOffset)
{
    if (retailOffset < 0x778u || retailOffset >= 0x7D8u || (retailOffset & 3u) != 0)
    {
        RD9_FRONTIER_AT("dice::hfe::dx9rend::rendererPathTarget (not a render-path target "
                        "offset)", publishedRenderer(), retailOffset);
        std::abort();
    }
    return pathTarget(publishedRenderer(), retailOffset);
}

RenderPathListener* rendererRenderPathListener()
{
    // The +0x540 subobject; its table is the hand-built kRendererRenderPathVtable, laid out as
    // RenderPathListener's six slots.
    return reinterpret_cast<RenderPathListener*>(&publishedRenderer()->m_renderPathVptr);
}

// PE 0x1012B320 (the capture counterpart of rendererRestoreHemiState; every field offset and the
// two raw word copies compared with the listing).
void rendererCaptureHemiState(HemiRenderState& state)
{
    Renderer* const renderer = static_cast<Renderer*>(g_rendererBase);
    state.bytes[40] = renderer->sw(1695);
    postProductionGetHemiState(reinterpret_cast<float*>(state.bytes + 24));
    // +0x6C0 (1728) and +0x7D8 (2008), copied as their raw 32-bit words.
    ::memcpy(state.bytes + 20, &renderer->m_fogOverride[0], 4);
    ::memcpy(state.bytes + 16, &renderer->m_forcedLod[0], 4);
    state.bytes[0] = renderer->sw(1717);
    state.bytes[1] = renderer->sw(1693);
    state.bytes[2] = renderer->sw(1697);
    state.bytes[3] = renderer->sw(1698);
    state.bytes[4] = renderer->sw(1714);
    state.bytes[5] = renderer->sw(1700);
    state.bytes[6] = geom::roadCompiledRendererGetHemiDirectFlag() == 0 ? 1u : 0u;
    state.bytes[7] = renderer->sw(1701);
    state.bytes[8] = renderer->sw(1704);
    state.bytes[9] = renderer->sw(1702);
    state.bytes[10] = renderer->sw(1713);
    state.bytes[11] = renderer->m_frameTimeOverlay != 0 ? 1u : 0u;          // +0x6AC (1708)
    state.bytes[15] = renderer->sw(1716);
    // bytes 12..14 and 41..43 are intentionally left untouched by the retail helper.
}

// PE 0x1012B460: the neutral post-production values (2000, 2000, 0, 0) and the fixed hemi bake switches.
void rendererApplyHemiState()
{
    Renderer* const renderer = static_cast<Renderer*>(g_rendererBase);
    renderer->sw(1695) = 0;
    const float postState[4] = { 2000.0f, 2000.0f, 0.0f, 0.0f };
    postProductionSetHemiState(postState);
    // +0x6C0 (1728) receives the raw word 0x447A0000 (the float 1000.0).
    const float fogValue = 1000.0f;
    ::memcpy(&renderer->m_fogOverride[0], &fogValue, 4);
    rendererSetStaticMeshLod(0);
    renderer->sw(1717) = 0;
    renderer->sw(1693) = 1;
    renderer->sw(1697) = 0;
    geom::bundledMeshSetHemiEnabled(0); // unguarded in retail
    renderer->sw(1698) = 1;
    geom::staticMeshRendererSetHemiEnabled(1); // unguarded in retail
    renderer->sw(1714) = 0;
    renderer->sw(1700) = 1;
    geom::roadCompiledRendererSetHemiEnabled(1); // unguarded in retail
    renderer->sw(1702) = 0;
    renderer->sw(1703) = 0;
    geom::roadCompiledRendererSetHemiDirectFlag(1); // guarded in its owner seam
    renderer->sw(1701) = 1;
    renderer->sw(1704) = 0;
    renderer->sw(1705) = 1;
    renderer->sw(1713) = 0;
    renderer->m_frameTimeOverlay = 0;                                       // +0x6AC (1708)
    renderer->sw(1716) = 0;
}

// PE 0x1012B170: puts back the switches rendererCaptureHemiState saved (the counterpart of PE 0x1012B320).
void rendererRestoreHemiState(const HemiRenderState& state)
{
    Renderer* const renderer = static_cast<Renderer*>(g_rendererBase);
    renderer->sw(1695) = state.bytes[40];
    postProductionSetHemiState(reinterpret_cast<const float*>(state.bytes + 24));
    ::memcpy(&renderer->m_fogOverride[0], state.bytes + 20, 4);              // +0x6C0 (1728)
    rendererSetStaticMeshLod(*reinterpret_cast<const int*>(state.bytes + 16));
    renderer->sw(1717) = state.bytes[0];
    renderer->sw(1693) = state.bytes[1];
    renderer->sw(1697) = state.bytes[2];
    geom::bundledMeshSetHemiEnabled(state.bytes[2]); // unguarded in retail
    renderer->sw(1698) = state.bytes[3];
    geom::staticMeshRendererSetHemiEnabled(state.bytes[3]); // unguarded in retail
    renderer->sw(1714) = state.bytes[4];
    renderer->sw(1700) = state.bytes[5];
    geom::roadCompiledRendererSetHemiEnabled(state.bytes[5]); // unguarded in retail
    geom::roadCompiledRendererSetHemiDirectFlag(state.bytes[6] == 0 ? 1u : 0u);
    renderer->sw(1701) = state.bytes[7];
    renderer->sw(1704) = state.bytes[8];
    renderer->sw(1705) = state.bytes[9];
    renderer->sw(1702) = state.bytes[9];
    renderer->sw(1703) = state.bytes[9];
    renderer->sw(1713) = state.bytes[10];
    renderer->m_frameTimeOverlay = state.bytes[11];                         // +0x6AC (1708)
    renderer->sw(1716) = state.bytes[15];
}


// Top-down, orthographic, and cube-face captures used by editor commands.


extern void* g_deviceCaps;   // RendDX9Init.cpp, dword_1023B980 - a D3DCAPS9 at +0

// byte_10238870, file-backed 1: DebugBoxMesh's draw gate (its draw, 0x10083530, runs only while it is
// set). The cube-face render clears it around each face and restores it. [CHOSEN placement]: the
// owner, GeomMisc\DebugBoxMesh.cpp (attested at 0x1020A7B8), is not in this tree - the definition
// moves there with it, and nothing else in this image reads it yet (RW-273).
// [INFERRED] name.
unsigned char g_debugBoxMeshDrawEnabled = 1;

// sub_10077E30 / sub_10077DA0 (both `retn 4`, ecx = the matrix): the whole matrix to identity, then a Y
// (m0 = cos, m2 = -sin, m8 = sin, m10 = cos) or X (m5 = cos, m6 = sin, m9 = -sin, m10 = cos) rotation
// of flt_10238738 (pi/180) times the angle - x87 fsincos on the float-rounded radians. The Y one is
// published through RendDX9Base.h (Geom's overgrowth placement calls it as well); the X one stays
// local. [CHOSEN] placement: the Mat4 rotation helpers have no shared home yet (RW-273).
void setRotationYDegrees(Mat4& matrix, float degrees)
{
#if defined(_WIN32)
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
#if defined(_M_IX86)
    __asm
    {
        fld radians
        fsincos
        fstp cosine
        fstp sine
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates); fsincos is the double
    // cos/sin of the float radians, each stored as a float (issue #7).
    cosine = static_cast<float>(::cos(static_cast<double>(radians)));
    sine = static_cast<float>(::sin(static_cast<double>(radians)));
#endif
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
#else
// [CHOSEN, not recovered] Preserve the S2.1 POSIX numeric arm (docs/macos-port.md).
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
    // [CHOSEN, not recovered] Preserve x87 rounded angle and float sin/cos stores.
    bf2x87::trig(radians, sine, cosine);
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
#endif
}

namespace {

// PE 0x10077DA0
void setRotationXDegrees(Mat4& matrix, float degrees)
{
#if defined(_WIN32)
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
#if defined(_M_IX86)
    __asm
    {
        fld radians
        fsincos
        fstp cosine
        fstp sine
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates) (issue #7).
    cosine = static_cast<float>(::cos(static_cast<double>(radians)));
    sine = static_cast<float>(::sin(static_cast<double>(radians)));
#endif
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
#else
// [CHOSEN, not recovered] Preserve the S2.1 POSIX numeric arm (docs/macos-port.md).
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
    // [CHOSEN, not recovered] Preserve x87 rounded angle and float sin/cos stores.
    bf2x87::trig(radians, sine, cosine);
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
#endif
}

} // anonymous namespace


// PE 0x10029DA0.
// Renders a square orthographic view into target using its height as the side length.
// Consumes a caller reference to target and restores views and depth state afterward.
// Requires valid surfaces and render subsystems. The prior depth-surface reference is retained.
void rendererRenderOrthoTopDown(IDirect3DSurface9* target, IDirect3DSurface9* depthStencil,
                                Vec3 position, float orthoExtent, RenderFlags flags)
{
    Renderer* const renderer = static_cast<Renderer*>(g_Renderer);
    Renderer* const raw = renderer;
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

    if (raw->sw(1693))
        objectDrawerSetRenderView(raw->m_objectDrawer, view);           // +0x5C0 (1472)
    renderer->setRenderView(view);                                                       // +0x44
    renderer->setRenderViewDup(view);                                                    // +0x4C

    IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(renderer)->device;
    IDirect3DSurface9* previousDepthStencil = 0;
    device->GetDepthStencilSurface(&previousDepthStencil);                               // +0xA0
    device->SetDepthStencilSurface(depthStencil);                                        // +0x9C
    reinterpret_cast<RendDX9Base*>(renderer)->tempBackBuffer = target;
    rendererPostSelectBackBuffer();

    EngineTexture* const savedScratch = renderer->getHemiScratchTexture();              // +0x774
    const int savedManaged = textureManagerSwapManaged(0);
    renderer->setHemiScratchTexture(static_cast<EngineTexture*>(
        textureManagerCreateEmpty(side, side, D3DFMT_A8R8G8B8, 1u, 1u, 0)));
    textureManagerSwapManaged(savedManaged);

    if (reinterpret_cast<RendDX9Base*>(renderer)->flag102)
    {
        renderer->renderScene(0.0, &flags, 0);       // +0x2C; 0x10029F77 pushes a zero dword
        stateManagerSetRenderState(195u, geom::roadCompiledRendererDepthBiasBits());   // depthbias
        stateManagerSetRenderState(175u, geom::roadCompiledRendererSlopeScaleDepthBiasBits());
        // `mov edx, g_Renderer; cmp byte ptr [edx+6A4h], 0` - the same object, reached as the global.
        if (static_cast<Renderer*>(static_cast<void*>(g_Renderer))->sw(1700) &&
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
    IDirect3DSurface9*& tempBackBuffer = reinterpret_cast<RendDX9Base*>(renderer)->tempBackBuffer;
    if (tempBackBuffer)
        tempBackBuffer->Release();
    tempBackBuffer = 0;
    rendererPostSelectBackBuffer();
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
    // PE 0x10001160 (inlined width getter).
    if (resolution > caps->MaxTextureWidth)
    {
        const unsigned int droppedClamp = caps->MaxTextureWidth;     // asked again, never stored
        (void)droppedClamp;
    }
    (void)resolution;                                                // never read again (shipped)

    textureManagerSwapManaged(1);                                    // the old value dropped

    Renderer* const self = static_cast<Renderer*>(static_cast<void*>(g_Renderer));
    IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(self)->device;
    IDirect3DSurface9* target = 0;
    IDirect3DSurface9* depthStencil = 0;
    device->CreateRenderTarget(2048u, 2048u, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0u, FALSE,
                               &target, 0);                                              // +0x70
    device->CreateDepthStencilSurface(2048u, 2048u, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0u, FALSE,
                                      &depthStencil, 0);                                 // +0x74

    RenderFlags flags;
    flags.useRenderView    = 0;
    flags.field01          = self->sw(1701);
    flags.world            = self->sw(1693);
    flags.worldAlt         = 1;
    flags.field04          = self->sw(1697);
    flags.staticMeshes     = self->sw(1698);
    flags.skinnedMeshes    = self->sw(1699);
    flags.decals           = self->sw(1696);
    flags.particles        = self->m_byte6DC;
    flags.field09          = 0;
    flags.field0a          = 0;
    flags.skyDome          = self->sw(1714);
    flags.undergrowth      = self->sw(1702);
    flags.field0d          = self->sw(1703);
    flags.postProduction   = 0;
    flags.colourCorrection = 0;
    flags.hud              = self->sw(1713);
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
    D3DXSaveSurfaceToFileA(path.c_str(), D3DXIFF_DDS, target, 0, 0);                    // IAT 0x101DB4C8
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
    Renderer* const raw = renderer;
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

    const unsigned char savedObjects    = raw->sw(1693);
    const unsigned char savedHud        = raw->sw(1713);
    const unsigned char saved1705       = raw->sw(1705);
    const unsigned char savedBundled    = raw->sw(1697);
    const unsigned char savedDebugBoxes = g_debugBoxMeshDrawEnabled;
    const unsigned char saved1702       = raw->sw(1702);
    const unsigned char saved1703       = raw->sw(1703);
    const unsigned char saved1717       = raw->sw(1717);
    raw->sw(1713) = 0;
    raw->sw(1705) = 0;
    raw->sw(1701) = 1;
    raw->sw(1697) = 0;
    geom::bundledMeshSetHemiEnabled(0);                            // dword_102597F4 +0x10, unguarded
    g_debugBoxMeshDrawEnabled = 0;
    raw->sw(1702) = 0;
    raw->sw(1703) = 0;
    raw->sw(1717) = 0;
    if (raw->sw(1693))
        objectDrawerSetRenderView(raw->m_objectDrawer, view);           // +0x5C0 (1472)
    renderer->setRenderView(view);                                                       // +0x44
    renderer->setRenderViewDup(view);                                                    // +0x4C

    IDirect3DSurface9*& tempBackBuffer = reinterpret_cast<RendDX9Base*>(renderer)->tempBackBuffer;
    tempBackBuffer = textureGetLevelSurface(cube, 0u, static_cast<unsigned int>(face));
    rendererPostSelectBackBuffer();
    renderer->renderFrame(0.0, 0.0f);                                                    // +0x28
    if (tempBackBuffer)
        tempBackBuffer->Release();
    tempBackBuffer = 0;
    rendererPostSelectBackBuffer();
    renderer->setRenderView(savedPrimary);
    renderer->setRenderViewDup(savedSecondary);

    raw->sw(1697) = savedBundled;
    geom::bundledMeshSetHemiEnabled(savedBundled);
    raw->sw(1693) = savedObjects;
    raw->sw(1713) = savedHud;
    raw->sw(1705) = saved1705;
    g_debugBoxMeshDrawEnabled = savedDebugBoxes;
    raw->sw(1702) = saved1702;
    raw->sw(1703) = saved1703;
    raw->sw(1717) = saved1717;
    return false;
}

namespace {

// PE 0x1002A120 (`retn 1Ch`), the capture slot +0x60 replays: render a square orthographic top-down
// frame of side `size` (clamped to the device's maximum texture height, then width) at `position`
// into the render-path target +0x7D0, then copy it with FSQuadDrawer pass 16 into the target's
// texture. `target` is [INFERRED] a Meme::Texture: the body reads its +0x2C texture, which is the
// Meme::Texture's wrapped render target (MemeTexture::createTexture). No caller is known; the body
// logs the developer note below on every entry. Without a target texture or a RenderView it returns
// with +0x7F0 still armed, so beginFrame replays it again next frame, as shipped.
void rendererRenderTopDownToTexture(Renderer* renderer, unsigned int size, void* target,
                                    float x, float y, float z, float orthoExtent,
                                    bool drawObjects)
{
    Renderer* const raw = renderer;
    const D3DCAPS9* const caps = static_cast<const D3DCAPS9*>(g_deviceCaps);
    if (size > caps->MaxTextureHeight)                               // sub_10001170, +0x5C
        size = caps->MaxTextureHeight;
    if (size > caps->MaxTextureWidth)                                // sub_10001160, +0x58
        size = caps->MaxTextureWidth;

    if (!target)
        return;
    EngineTexture* const targetTexture = static_cast<EngineTexture*>(
        geom::memeTextureEngineTexture(static_cast<const geom::MemeTexture*>(target)));   // +0x2C
    if (!targetTexture)
        return;

    // sub_10074300(6, __FILE__, 3666, "") << ...; sub_10074690 - dice's debug stream, kept as one
    // log line as every recovered diagnostic in this image is.
    ::renddx9abi::logf("BF2Render.cpp:3666 tl: Is this function used?? I've optimized away "
                       "something it required. Tell me about this!");

    IRenderView* const view = static_cast<IRenderView*>(static_cast<void*>(
        classManager->createInstance(Rend::CID_RenderView, Rend::IID_IRenderView, 0)));   // +0x30
    if (!view)
        return;

    IRenderView* const savedPrimary = renderer->getRenderView();                        // +0x48
    IRenderView* const savedSecondary = renderer->getRenderViewDup();                   // +0x50

    Mat4 viewMatrix = Mat4::getIdentity();                                   // sub_100791E0 + copy
    const float side = static_cast<float>(size);                             // fild, +2^32 if signed
    viewMatrix.m[0] = 1.0f;  viewMatrix.m[1] = 0.0f;  viewMatrix.m[2] = 0.0f;
    viewMatrix.m[8] = 0.0f;  viewMatrix.m[9] = -1.0f; viewMatrix.m[10] = 0.0f;
    viewMatrix.m[4] = 0.0f;  viewMatrix.m[5] = 0.0f;  viewMatrix.m[6] = 1.0f;
    viewMatrix.m[12] = x;
    viewMatrix.m[13] = y;
    viewMatrix.m[14] = z;
    view->setViewport(0.0f, 0.0f, side, side);                                          // +0x10
    view->setProjectionMode(1u);                                                         // +0x48
    view->setNearPlane(0.01f);                                                           // +0x30
    view->setFarPlane(1000.0f);                                                          // +0x38
    view->setOrthoWidth(orthoExtent);                                                    // +0x50
    view->setOrthoHeight(orthoExtent);                                                   // +0x58
    view->setViewMatrix(viewMatrix.m);                                                   // +0x60

    // All five switches are read before any is written.
    const unsigned char savedObjects = raw->sw(1693);                                    // +0x69D
    const unsigned char savedHud     = raw->sw(1713);                                    // +0x6B1
    const unsigned char saved1705    = raw->sw(1705);                                    // +0x6A9
    const unsigned char savedBundled = raw->sw(1697);                                    // +0x6A1
    const unsigned char saved1717    = raw->sw(1717);                                    // +0x6B5
    raw->sw(1693) = drawObjects ? 1 : 0;
    raw->sw(1713) = 0;
    raw->sw(1705) = 0;
    raw->sw(1701) = 1;                                               // left set, as shipped
    raw->sw(1697) = 0;
    geom::bundledMeshSetHemiEnabled(0);                            // dword_102597F4 +0x10, unguarded
    raw->sw(1717) = 0;
    if (raw->sw(1693))
        objectDrawerSetRenderView(raw->m_objectDrawer, view);           // +0x5C0 (1472)
    renderer->setRenderView(view);                                                       // +0x44
    renderer->setRenderViewDup(view);                                                    // +0x4C

    IDirect3DSurface9*& tempBackBuffer = reinterpret_cast<RendDX9Base*>(renderer)->tempBackBuffer;
    tempBackBuffer = textureGetLevelSurface(pathTarget(renderer, 0x7D0), 0u, 0x7FFFFFFFu);
    rendererPostSelectBackBuffer();                                  // sub_10005920(0, 0, 0, max)

    EngineTexture* const savedScratch = renderer->getHemiScratchTexture();              // +0x774
    renderer->setHemiScratchTexture(pathTarget(renderer, 0x7CC));
    renderer->renderFrame(0.0, 0.0f);                                                    // +0x28

    IDirect3DDevice9* const device = reinterpret_cast<RendDX9Base*>(renderer)->device;
    device->BeginScene();                                                                // +0xA4
    geom::fsQuadDrawerTexturePass(16u, pathTarget(renderer, 0x7D0), targetTexture, true, 0u);
    device->EndScene();                                                                  // +0xA8

    if (tempBackBuffer)
        tempBackBuffer->Release();
    tempBackBuffer = 0;
    rendererPostSelectBackBuffer();
    renderer->setRenderView(savedPrimary);                                               // +0x44
    renderer->setRenderViewDup(savedSecondary);                                          // +0x4C
    view->release();                                                                     // +0x08

    renderer->setHemiScratchTexture(savedScratch);
    raw->sw(1697) = savedBundled;
    geom::bundledMeshSetHemiEnabled(savedBundled);
    raw->sw(1693) = savedObjects;
    raw->sw(1713) = savedHud;
    raw->sw(1705) = saved1705;
    raw->sw(1717) = saved1717;
    raw->m_frameHookEnabled = 0;                                                         // +0x7F0
}

} // anonymous namespace

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

// byte_1023E7F0, .bss: the statistics overlay's "active" byte. Written by the mode setter, the
// performance-test reset and its rebuild; nothing in the image reads it. [INFERRED] name.
unsigned char g_statisticsOverlayActive = 0;

// PE 0x1002A6A0. Clamps the mode to 0..4, stores it at +0x6D4 and mirrors "non-zero" into the
// overlay's active byte.
void Renderer::setStatisticsOverlayMode(int mode)
{
    if (mode < 0)
        mode = 0;
    else if (mode > 4)
        mode = 4;
    m_dword6D4 = static_cast<unsigned int>(mode);
    g_statisticsOverlayActive = m_dword6D4 != 0;
}

// PE 0x1002A6E0. Clamps the value to 0..2 and stores it at +0x6D8, which no instruction reads.
void Renderer::setUnreadWord6D8(int value)
{
    if (value < 0)
        value = 0;
    else if (value > 2)
        value = 2;
    m_dword6D8 = value;
}

// PE 0x1002A710. Returns `value` with y and w multiplied by the video options' view-distance
// scale, which is fetched afresh for each through the published renderer (+0x74, then +0x28);
// x and z are copied. `this` is not read.
// unreachable in 1.50: no code or data references
Vec4 Renderer::scaleByViewDistance(const Vec4& value)
{
    Vec4 scaled = value;
    scaled.y = rendererGetVideoOptions()->getViewDistanceScale() * scaled.y;
    scaled.w = rendererGetVideoOptions()->getViewDistanceScale() * scaled.w;
    return scaled;
}

// PE 0x1002DB80. One frame of the performance analysis, run on the last control-point row: the
// sample index is the row's elapsed time over the +0x864 interval, truncated. On a new index the
// total queue profiler's frame rate, draw calls (counter 0) and +0x25C word update the row's
// minimum, maximum and running average (value / +0x860 sample count); the frame time is then
// added. While the index is below the sample count the camera sits 2 units above the control
// point, turned index * 350 / count degrees about y, in both render views. Once it reaches the
// count, the row's report line is appended to +0x868 and the row popped; after the last row the
// statistics overlay byte follows +0x6D4 again and the console prints the report.
// unreachable in 1.50: no code or data references
void Renderer::updatePerformanceAnalysis(float deltaTime)
{
    if (m_performanceRecords.empty())
        return;
    PerformanceAnalysisRecord& record = m_performanceRecords.back();
    const int sample = static_cast<int>(static_cast<double>(record.elapsed) / m_f864);   // chop
    const int sampleCount = static_cast<int>(m_dword860);
    if (record.sample != sample)
    {
        // Each minimum keeps the row value only while it is below the sample (an unordered sample
        // is taken); each maximum takes the sample only while the row value is below it.
        double value = g_totalQueueProfiler->framesPerSecond;                    // +0x1B8
        if (!(record.minimum[0] < value))
            record.minimum[0] = static_cast<float>(value);
        value = g_totalQueueProfiler->framesPerSecond;
        if (record.maximum[0] < value)
            record.maximum[0] = static_cast<float>(value);
        record.average[0] = static_cast<float>(
            static_cast<double>(g_totalQueueProfiler->framesPerSecond) / sampleCount +
            record.average[0]);

        value = static_cast<double>(g_totalQueueProfiler->counter(0));
        if (!(record.minimum[1] < value))
            record.minimum[1] = static_cast<float>(value);
        value = static_cast<double>(g_totalQueueProfiler->counter(0));
        if (record.maximum[1] < value)
            record.maximum[1] = static_cast<float>(value);
        record.average[1] = static_cast<float>(
            static_cast<double>(g_totalQueueProfiler->counter(0)) / sampleCount +
            record.average[1]);

        value = static_cast<double>(g_totalQueueProfiler->untouched25C);         // +0x25C
        if (!(record.minimum[2] < value))
            record.minimum[2] = static_cast<float>(value);
        value = static_cast<double>(g_totalQueueProfiler->untouched25C);
        if (record.maximum[2] < value)
            record.maximum[2] = static_cast<float>(value);
        record.average[2] = static_cast<float>(
            static_cast<double>(g_totalQueueProfiler->untouched25C) / sampleCount +
            record.average[2]);
    }
    record.sample = sample;
    record.elapsed = deltaTime + record.elapsed;

    if (sample >= sampleCount)
    {
        const double megabyte = 1.0 / 1048576.0;                                 // float 0x35800000
        const std::string& name = record.object->getTemplate()->getName();      // +0x14, +0x28
        m_string868 += formatString(
            "%-40s %5.1f %5.1f %5.1f   %5.1f %5.1f %5.1f   %5.1f %5.1f %5.1f\n", name.c_str(),
            static_cast<double>(record.average[0]), static_cast<double>(record.minimum[0]),
            static_cast<double>(record.maximum[0]), static_cast<double>(record.average[1]),
            static_cast<double>(record.minimum[1]), static_cast<double>(record.maximum[1]),
            static_cast<double>(record.average[2]) * megabyte,
            static_cast<double>(record.minimum[2]) * megabyte,
            static_cast<double>(record.maximum[2]) * megabyte);
        m_performanceRecords.pop_back();
        if (m_performanceRecords.empty())
        {
            g_statisticsOverlayActive = m_dword6D4 != 0;
            reinterpret_cast<io::Console*>(::dice::hfe::mainConsole)->output(m_string868);  // +0x10
        }
        return;
    }

    Mat4 view = Mat4::getIdentity();                                             // 0x100791E0
    setRotateYDeg(view, static_cast<float>(static_cast<double>(sample) * 350.0f / sampleCount));
    const Vec3& position = record.object->getAbsolutePosition();                 // +0x68
    view.m[12] = position.x;
    view.m[13] = position.y + 2.0f;
    view.m[14] = position.z;
    rendererGetRenderView()->setViewMatrix(view.m);                              // +0x48, +0x60
    rendererGetRenderViewDup()->setViewMatrix(view.m);                           // +0x50, +0x60
}

// PE 0x1002E120. Starts a performance analysis: the row vector is freed, the sample count clamped
// to 1..32 (+0x860) and the interval to 0.1..20 seconds (+0x864, the shared clamp: the high bound
// first, an unordered value landing on the low bound), and the statistics overlay byte set. Every
// object of the ObjectManager's map that answers IID_IControlPoint gets a row (minimums 1.0e8,
// everything else 0), and +0x868 restarts with the report header.
// unreachable in 1.50: no code or data references
void Renderer::runPerformanceAnalysis(int sampleCount, float interval)
{
    // The VC7.1 clear(): the storage is freed and the three words zeroed.
    std::vector<PerformanceAnalysisRecord>().swap(m_performanceRecords);
    if (sampleCount < 1)
        sampleCount = 1;
    else if (sampleCount > 32)
        sampleCount = 32;
    m_dword860 = static_cast<unsigned int>(sampleCount);
    float clamped = interval;
    if (clamped >= 20.0f)
        clamped = 20.0f;
    if (!(clamped >= 0.1f))
        clamped = 0.1f;
    m_f864 = clamped;
    g_statisticsOverlayActive = 1;

    std::map<unsigned int, world::IObject*>& objects =
        reinterpret_cast<world::ObjectManager*>(::dice::hfe::objectManager)->getAllObjectsFromGrid();
    for (std::map<unsigned int, world::IObject*>::iterator it = objects.begin();
         it != objects.end(); ++it)
    {
        if (!it->second->queryInterface(world::kIID_IControlPoint))
            continue;
        PerformanceAnalysisRecord record;
        record.object = it->second;
        record.controlPoint =
            static_cast<world::IControlPoint*>(it->second->queryInterface(world::kIID_IControlPoint));
        for (int i = 0; i < 3; ++i)
        {
            record.minimum[i] = 1.0e8f;
            record.maximum[i] = 0.0f;
            record.average[i] = 0.0f;
        }
        record.elapsed = 0.0f;
        record.sample = 0;
        m_performanceRecords.push_back(record);                                  // 0x100327C0
    }
    m_string868 = formatString("%-40s %-5s %5s %5s   %5s %5s %5s   %5s %5s %5s   \n",
                               "controlPoint", "avgFR", "minFR", "maxFR", "avgDC", "minDC",
                               "maxDC", "avgSM", "minSM", "maxSM");
}

// PE 0x1002A0E0: stamps the frame clock and, while slot +0x60 has armed +0x7F0, replays the argument set slot
// +0x60 stored. Requires the system singleton.
void Renderer::beginFrame()
{
    m_frameStartTime = hostCalls::systemWallClockTime();                              // +0x18

    if (m_frameHookEnabled != 0)                          // +0x7F0
        slot_0x60(0, 0, 0.0f, 0.0f, 0.0f, 0.0f, false);   // seven zero dwords, through the vtable
}

namespace {

// byte_1023D6A8, .bss: renderScene's two post-production arms skip while it is set. Nothing in
// the image writes it. [INFERRED] name.
unsigned char g_renderSceneSkipPostProduction = 0;

// The render-target selector (index, texture, level, face): the same retail function as
// geom::lightManagerSelectRenderTarget (XpLightManager.cpp, which carries the citation and the one
// body); this TU's callers keep their short name.
bool selectRenderTarget(RendDX9Base* base, unsigned int index, EngineTexture* texture,
                        unsigned int level, unsigned int face)
{
    return geom::lightManagerSelectRenderTarget(base, index, texture, level, face);
}

// sub_10037EE0 on the Ra constants block: the view matrix into +0x1C, its translation row into
// the camera position at +0x60.
void raConstantsSetViewMatrix(const float* m)
{
    float* const matrix = reinterpret_cast<float*>(raShaderConstantsAt(0x1C));
    for (int i = 0; i < 16; ++i)
        matrix[i] = m[i];
    float* const camera = reinterpret_cast<float*>(raShaderConstantsAt(0x60));
    camera[0] = matrix[12];
    camera[1] = matrix[13];
    camera[2] = matrix[14];
}

// sub_100383D0 - GlobalTime (+0x94) accumulates the frame time.
void raConstantsAddGlobalTime(float dt)
{
    *reinterpret_cast<float*>(raShaderConstantsAt(0x94)) += dt;
    g_waterClock += dt;
}

// sub_100387E0 - the three dwords of the sun colour at +0x11C.
void raConstantsSetSunColor(const Vec3& colour)
{
    float* const sun = reinterpret_cast<float*>(raShaderConstantsAt(0x11C));
    sun[0] = colour.x;
    sun[1] = colour.y;
    sun[2] = colour.z;
}

// The base hemi map's texture and its four constants, as renderScene publishes them twice
// (HemiMapManager +0x18 / +0x1C; the manager is not null-tested).
void publishBaseHemiMap()
{
    geom::HemiMap* const hemiMap = geom::g_HemiMapManager->getBaseHemiMap();
    if (!hemiMap)
        return;
    raShaderConstantsSetHemiMapTexture(hemiMap->texture);           // sub_10031180, block +0x134
    const geom::HemiMapInfo& info = geom::g_HemiMapManager->getHemiMapInfo(hemiMap);
    raShaderConstantsSetHemiMapInfo(&info.positionX);               // block +0x140
}

// The ObjectDrawer's primary slots +0x10 / +0x14 are its draw / updateVisible members
// (Geom/ObjectDrawer.cpp's table forwards to them).
geom::ObjectDrawer* objectDrawer(void* drawer)
{
    return static_cast<geom::ObjectDrawer*>(drawer);
}

// StaticMeshRenderer primary +0x18 stores its enable byte (+0x04).
void staticMeshRendererSetEnabled(unsigned char enabled)
{
    static_cast<geom::StaticMeshRenderer*>(static_cast<void*>(geom::g_StaticMeshRenderer))
        ->setEnabled(enabled);
}

// The SkinnedMeshRenderer's RenderPathListener subobject is its second word; slot +0x0C.
void skinnedMeshRendererPathNotification()
{
    RenderPathListener* const listener = reinterpret_cast<RenderPathListener*>(
        reinterpret_cast<void**>(geom::g_SkinnedMeshRenderer) + 1);
    listener->onRenderPathNotification(0);
}

// SkyDome primary +0xA8 (drawFlare) and +0x10 (update); `sky` is the singleton as read at the
// call, untested.
void skyDomeDrawPass(IBase* sky, int pass)
{
    static_cast<geom::SkyDome*>(sky)->drawFlare(pass);
}

void skyDomeUpdate(IBase* sky, float time)
{
    static_cast<geom::SkyDome*>(sky)->update(time);
}

// sub_100D89C0, the Terrain's +0x35E setter: the store happens only while the value differs and
// +0x35F is clear. [CHOSEN, not recovered] placement: a Terrain member in the PE; kept here until
// Geom/Terrain publishes it.
void terrainSetLowQualityB(geom::Terrain* terrain, unsigned char value)
{
    if (terrain->m_lowQualityB != value && !terrain->m_lowQuality)
        terrain->m_lowQualityB = value;
}

} // anonymous namespace


// The Renderer seams PostProductionFrameWork.cpp's passes use (PostProductionFrameWork.h). All of
// them require the published Renderer.

bool rendererPostMultisampleActive()
{
    return isMultisampleActive();                                        // sub_1000A6B0
}

// +0x788, the full-size render-path target the scene is drawn into while post-production is active.
EngineTexture* rendererPostSceneTexture()
{
    return pathTarget(publishedRenderer(), 0x788);
}

// The render-path target `index` slots past +0x778; the tinnitus history uses 17..20 (+0x7BC..).
EngineTexture* rendererPostHistoryTexture(unsigned int index)
{
    return pathTarget(publishedRenderer(), 0x778u + 4u * index);
}

// PE 0x100286E0 - the Renderer's time: its frame timer (+0x5DC) handed to frameTimerTime
// (0x100776A0, the total seconds at +0x20) as a tail call. Post-production and SkinnedMesh read it.
double rendererPostTime()
{
    return frameTimerTime(publishedRenderer()->m_frameTimer);
}

// PE 0x10028090. Paints the HUD when a HUD manager is attached (+0x5D0): its +0x1C paint, as a
// tail jump. Unreachable in 1.50: no code or data references. The shipped renderFrame
// (0x1003080C), renderScene (0x1002C534) and slot +0x38 (0x1002A52B) inline the same test and call
// instead. [INFERRED] name: the 2006 RendDX9_r copy (0x10078C30) opens a "drawHUD" profiler scope
// around the call and is called from the renderScene and renderFrame counterparts there.
void Renderer::drawHUD()
{
    if (m_hudManager)
        hostCalls::hudPaint(m_hudManager);                                        // +0x1C
}

// PE 0x100280B0. Clears the ten FPS histogram buckets that renderFrame fills.
// unreachable in 1.50: no code or data references
void Renderer::resetFpsHistogram()
{
    for (int i = 0; i < 10; ++i)
        m_fpsHistogram[i] = 0;
}

// PE 0x100280F0. Returns the FPS histogram as text, one line per bucket: the bucket's frame-rate
// range (index times the +0x820 bucket width, truncated), its share of all counted frames and the
// running share, both in percent. The running total is stored as a float after every bucket; the
// percentages are passed as doubles. An empty histogram divides by zero, as shipped.
// unreachable in 1.50: no code or data references
std::string Renderer::getFpsHistogram()
{
    unsigned int total = 0;
    for (int i = 9; i >= 0; --i)
        total += m_fpsHistogram[i];
    std::string text;
    const float totalFrames = static_cast<float>(total);
    float runningFrames = 0.0f;
    for (int i = 0; i < 10; ++i)
    {
        const double frames = static_cast<double>(m_fpsHistogram[i]);       // unsigned load
        runningFrames = static_cast<float>(runningFrames + frames);
        text += formatString("[%2d-%2d]\t%1.1f%%, (%1.1f%%)\n",
                             static_cast<int>(static_cast<double>(i) * m_fpsBucketScale),
                             static_cast<int>(static_cast<double>(i + 1) * m_fpsBucketScale),
                             frames * 100.0f / totalFrames,
                             static_cast<double>(runningFrames) * 100.0f / totalFrames);
    }
    return text;
}

// PE 0x10028250. Queues text through the DebugTextWriter singleton, alignment 0. The debug image
// first asserts that the renderer has a font ("drawText: No font loaded!"); the retail body has no
// test and does not read `this`. The overlays spell the same call inline (profilerText above).
// unreachable in 1.50: no code or data references
void Renderer::drawText(int x, int y, const std::string& text)
{
    geom::debugTextWriterQueueText(geom::g_DebugTextWriter, x, y, text, 0);   // +0x40
}

// PE 0x10028460. Restricts the queue-profiler enable flag to the placed-object queues: every
// profiler is selected, and with `enabled` all of them are disabled, deselected, and the STM, BM,
// Leaf, SM, RSM, PostP and RSMTTrans prefixes reselected (LeafOG explicitly deselected). The
// selected profilers are then enabled, so `enabled == false` enables every profiler.
// unreachable in 1.50: no code or data references
void Renderer::onlyDrawPlacedObjects(bool enabled)
{
    queueProfilersSelectAll(1);
    if (enabled)
    {
        queueProfilersSetEnabledOnSelected(0);
        queueProfilersSelectAll(0);
        queueProfilersSelectByPrefix(std::string("STM"), 1);
        queueProfilersSelectByPrefix(std::string("BM"), 1);
        queueProfilersSelectByPrefix(std::string("Leaf"), 1);
        queueProfilersSelectByPrefix(std::string("SM"), 1);
        queueProfilersSelectByPrefix(std::string("RSM"), 1);
        queueProfilersSelectByPrefix(std::string("PostP"), 1);
        queueProfilersSelectByPrefix(std::string("LeafOG"), 0);
        queueProfilersSelectByPrefix(std::string("RSMTTrans"), 1);
    }
    queueProfilersSetEnabledOnSelected(1);
}

// PE 0x100286F0
void Renderer::setFakeHDRBlendingTimeLight(double value)
{
    const unsigned char* const timer = static_cast<const unsigned char*>(m_frameTimer);
    m_xpLightRiseTime = value;
    m_xpLightMarkTime = *reinterpret_cast<const double*>(timer + 0x20);
}

// PE 0x10028720
void Renderer::setFakeHDRBlendingTimeDark(double value)
{
    const unsigned char* const timer = static_cast<const unsigned char*>(m_frameTimer);
    m_xpLightFallTime = value;
    m_xpLightMarkTime = *reinterpret_cast<const double*>(timer + 0x20);
}

// PE 0x10028750
void Renderer::setFakeHDRBlendingFactor(float value)
{
    const unsigned char* const timer = static_cast<const unsigned char*>(m_frameTimer);
    m_xpLightLevel = value;
    m_xpLightMarkTime = *reinterpret_cast<const double*>(timer + 0x20);
}

// PE 0x10028780. Samples the LightManager's simple shadow map under the view's matrix
// translation (x = +0x30, z = +0x38) and returns the sample. `this` is not read. xpLightBlendTick
// carries this body inline (0x1002B91B..0x1002B93A) and calls it here.
// unreachable in 1.50: no code or data references
float Renderer::sampleShadowUnderView(IRenderView* view)
{
    const float* const m = view->getViewMatrixPtr()->m;                            // +0x64
    return geom::g_LightManager->sampleSimpleShadowMap(m[12], m[14]);              // +0x2C4
}

// [CHOSEN, not recovered] Console bridges to the renderer's recovered member setters.
void rendererSetFakeHDRBlendingTimeLight(double value)
{
    publishedRenderer()->setFakeHDRBlendingTimeLight(value);
}

void rendererSetFakeHDRBlendingTimeDark(double value)
{
    publishedRenderer()->setFakeHDRBlendingTimeDark(value);
}

void rendererSetFakeHDRBlendingFactor(float value)
{
    publishedRenderer()->setFakeHDRBlendingFactor(value);
}

// PE 0x1002A780
void Renderer::setNightMode(unsigned int mode)
{
    static Vec3 sunSpecColor = {}, sunColor = {};
    static Vec3 sunColorHigh = {}, sunColorLow = {};
    static Vec3 sunSpecColorHigh = {}, sunSpecColorLow = {};
    static Vec3 staticSunColor = {}, terrainSunColor = {}, terrainGIColor = {};
    static Vec3 skyColor = {}, staticSkyColor = {}, ambientColor = {}, terrainAmbientColor = {};
    static Vec3 fogColor = {};
    static Vec4 fogState = {}, illuminationRange = {};
    static Vec3 dynamicPointColor = {}, singlePointColor = {}, treeAmbientColor = {};
    static Vec3 treeSunColor = {}, treeSkyColor = {}, effectSunColor = {};
    static Vec3 singlePointHigh = {}, singlePointLow = {};
    static Vec3 dynamicPointHigh = {}, dynamicPointLow = {};
    static Vec3 skyColorHigh = {}, skyColorLow = {};
    static Vec3 staticSkyHigh = {}, staticSkyLow = {};
    static Vec3 terrainGIHigh = {}, terrainGILow = {};
    static Vec3 waterFogColorB = {}, waterFogColorA = {};
    static Vec4 waterFogRangeB = {}, waterFogRangeA = {};
    static Vec3 waterColor = {};
    static Vec4 waterSpecularColor = {};
    static unsigned char drawObjects = 0;
    static double riseTime = 0.0, fallTime = 0.0;
    static float waterSpecularPower = 0.0f, highPassGate = 0.0f;

    if (m_nightVisionStage == 0)
    {
        sunSpecColor = geom::g_LightManager->getSunSpecColor();
        sunColor = geom::g_LightManager->getSunColor();
        sunColorHigh = geom::g_LightManager->getSunColorHigh();
        sunColorLow = geom::g_LightManager->getSunColorLow();
        sunSpecColorHigh = geom::g_LightManager->getSunSpecColorHigh();
        sunSpecColorLow = geom::g_LightManager->getSunSpecColorLow();
        staticSunColor = geom::g_LightManager->getStaticSunColor();
        Vec3 temporary;
        terrainSunColor = *geom::terrain()->getSunColor(&temporary);
        terrainGIColor = *geom::terrain()->getGIColorTimes2(&temporary);
        skyColor = geom::g_LightManager->getSkyColor();
        staticSkyColor = geom::g_LightManager->getStaticSkyColor();
        ambientColor = geom::g_LightManager->getAmbientColor();
        postProductionGetFogColor255(&fogColor.x);
        postProductionGetFogState(&fogState.x);
        postProductionGetAmbientIlluminationRange(&illuminationRange.x);
        dynamicPointColor = geom::g_LightManager->getDynamicPointColor();
        singlePointColor = geom::g_LightManager->getSinglePointColor();
        terrainAmbientColor = *geom::terrain()->getAmbientColor(&temporary);
        treeAmbientColor = geom::g_LightManager->getTreeAmbientColor();
        treeSunColor = geom::g_LightManager->getTreeSunColor();
        treeSkyColor = geom::g_LightManager->getTreeSkyColor();
        effectSunColor = geom::g_LightManager->getEffectSunColor();
        drawObjects = sw(0x69C);
        riseTime = m_xpLightRiseTime;
        fallTime = m_xpLightFallTime;
        singlePointHigh = geom::g_LightManager->getSinglePointColorHigh();
        singlePointLow = geom::g_LightManager->getSinglePointColorLow();
        dynamicPointHigh = geom::g_LightManager->getDynamicPointColorHigh();
        dynamicPointLow = geom::g_LightManager->getDynamicPointColorLow();
        skyColorHigh = geom::g_LightManager->getSkyColorHigh();
        skyColorLow = geom::g_LightManager->getSkyColorLow();
        staticSkyHigh = geom::g_LightManager->getStaticSkyColorHigh();
        staticSkyLow = geom::g_LightManager->getStaticSkyColorLow();
        terrainGIHigh = geom::terrain()->m_giColorHigh;
        terrainGILow = geom::terrain()->m_giColorLow;
        postProductionGetWaterFogColor255(&waterFogColorB.x);
        postProductionGetWaterFogColorA255(&waterFogColorA.x);
        postProductionGetWaterFogStartEndAndBase(&waterFogRangeB.x);
        postProductionGetWaterFogRangeA(&waterFogRangeA.x);
        waterColor = geom::terrain()->getWaterColor();
        Vec4 temporary4;
        waterSpecularColor = *geom::terrain()->getWaterSpecularColor(&temporary4);
        waterSpecularPower = geom::terrain()->getWaterSpecularPower();
        highPassGate = geom::fsQuadDrawerGetSettings().highPassGate;
    }
    if (mode != 0)
    {
        m_nightVisionStage = static_cast<int>(mode);
        return;
    }
    geom::g_LightManager->setSunSpecColor(sunSpecColor);
    geom::g_LightManager->setSunColor(sunColor);
    geom::g_LightManager->setSunColorHigh(sunColorHigh);
    geom::g_LightManager->setSunColorLow(sunColorLow);
    geom::g_LightManager->setSunSpecColorHigh(sunSpecColorHigh);
    geom::g_LightManager->setSunSpecColorLow(sunSpecColorLow);
    geom::g_LightManager->setStaticSunColor(staticSunColor);
    geom::terrain()->setSunColor(terrainSunColor.x, terrainSunColor.y, terrainSunColor.z);
    geom::terrain()->setGIColor(terrainGIColor.x, terrainGIColor.y, terrainGIColor.z);
    geom::g_LightManager->setSkyColor(skyColor);
    geom::g_LightManager->setStaticSkyColor(staticSkyColor);
    geom::g_LightManager->setAmbientColor(ambientColor);
    postProductionSetFogColor255(&fogColor.x);
    postProductionSetFogState(&fogState.x);
    postProductionSetAmbientIlluminationRange(&illuminationRange.x);
    geom::g_LightManager->setDynamicPointColor(dynamicPointColor);
    geom::g_LightManager->setSinglePointColor(singlePointColor);
    geom::terrain()->setAmbientColor(terrainAmbientColor.x, terrainAmbientColor.y, terrainAmbientColor.z);
    geom::g_LightManager->setTreeAmbientColor(treeAmbientColor);
    geom::g_LightManager->setTreeSunColor(treeSunColor);
    geom::g_LightManager->setTreeSkyColor(treeSkyColor);
    geom::g_LightManager->setEffectSunColor(effectSunColor);
    m_xpLightRiseTime = riseTime;
    sw(0x69C) = drawObjects;
    const unsigned char* timer = static_cast<const unsigned char*>(m_frameTimer);
    m_xpLightMarkTime = *reinterpret_cast<const double*>(timer + 0x20);
    m_xpLightFallTime = fallTime;
    timer = static_cast<const unsigned char*>(m_frameTimer);
    m_xpLightMarkTime = *reinterpret_cast<const double*>(timer + 0x20);
    geom::g_LightManager->setSinglePointColorHigh(singlePointHigh);
    geom::g_LightManager->setSinglePointColorLow(singlePointLow);
    geom::g_LightManager->setDynamicPointColorHigh(dynamicPointHigh);
    geom::g_LightManager->setDynamicPointColorLow(dynamicPointLow);
    geom::g_LightManager->setSkyColorHigh(skyColorHigh);
    geom::g_LightManager->setSkyColorLow(skyColorLow);
    geom::g_LightManager->setStaticSkyColorHigh(staticSkyHigh);
    geom::g_LightManager->setStaticSkyColorLow(staticSkyLow);
    geom::terrain()->m_giColorHigh = terrainGIHigh;
    geom::terrain()->m_giColorLow = terrainGILow;
    postProductionSetWaterFogColorB255(&waterFogColorB.x);
    postProductionSetWaterFogColorA255(&waterFogColorA.x);
    postProductionSetWaterFogRangeB(&waterFogRangeB.x);
    postProductionSetWaterFogRangeA(&waterFogRangeA.x);
    geom::terrain()->setWaterColor(waterColor);
    geom::terrain()->setWaterSpecularColor(waterSpecularColor.x, waterSpecularColor.y,
                                          waterSpecularColor.z, waterSpecularColor.w);
    geom::terrain()->setWaterSpecularPower(waterSpecularPower);
    geom::FSQuadDrawerSettings settings = geom::fsQuadDrawerGetSettings();
    settings.highPassGate = highPassGate;
    geom::fsQuadDrawerSetSettings(settings);
    m_nightVisionStage = 0;
}

// PE 0x100286B0
unsigned int rendererGetNightMode()
{
    return static_cast<unsigned int>(publishedRenderer()->m_nightVisionStage);
}

// [CHOSEN, not recovered] Native owner bridge for PE 0x1002A780.
void rendererSetNightMode(unsigned int mode)
{
    publishedRenderer()->setNightMode(mode);
}

// PE 0x1005D330 reads and assigns the name without reloading its texture.
const std::string& rendererGetWatermarkFilename()
{
    return publishedRenderer()->m_overlayTextureName;
}

// PE 0x1005D330, assignment arm.
void rendererSetWatermarkFilename(const std::string& name)
{
    const std::string copy(name);
    publishedRenderer()->m_overlayTextureName = copy;
}

// The selector with (0, 0, 0, 0x7FFFFFFF): render target 0 back on the base's temporary back
// buffer (+0xF4) or the device back buffer (+0x76C).
void rendererPostSelectBackBuffer()
{
    selectRenderTarget(static_cast<RendDX9Base*>(g_rendererBase), 0, 0, 0, 0x7FFFFFFFu);
}

// sub_10028E80 - unless `texture` is already the one retained at +0x7D4: release that one, retain
// `texture` (texture slot +0x08) and select it as render target 0 (sub_10005920); a null texture
// selects the back buffer.
void rendererPostSetTarget(EngineTexture* texture)
{
    EngineTexture*& retained = pathTarget(publishedRenderer(), 0x7D4);
    if (retained == texture)
        return;
    textureReleaseRef(retained);
    retained = texture;
    if (texture)
        textureAddRef(texture);
    selectRenderTarget(static_cast<RendDX9Base*>(g_rendererBase), 0, texture, 0, 0x7FFFFFFFu);
}

// sub_10028070 - select the retained target (+0x7D4) as render target 0 again (a null one selects
// the back buffer).
void rendererPostReselectTarget()
{
    selectRenderTarget(static_cast<RendDX9Base*>(g_rendererBase), 0,
                       pathTarget(publishedRenderer(), 0x7D4), 0, 0x7FFFFFFFu);
}

// sub_1004F260 - resolve the multisampled back buffer (+0x76C) into the scene target (+0x788,
// level 0) with StretchRect, D3DTEXF_NONE. Neither surface is null-tested.
void rendererPostResolveScene()
{
    Renderer* const renderer = publishedRenderer();
    IDirect3DSurface9* const destination =
        textureGetLevelSurface(pathTarget(renderer, 0x788), 0, 0x7FFFFFFFu);
    static_cast<RendDX9Base*>(g_rendererBase)->device->StretchRect(
        renderer->m_backBuffer, 0, destination, 0, D3DTEXF_NONE);
    destination->Release();
}

// sub_10028330 - renderScene's road pass. Runs only when the Ra table has a "Road" shader and
// both the road renderer and the Terrain exist: GlobalTime, the duplicate view's matrix and the
// Terrain sun colour into the constants, the fog update, then the RSMTRoad queue under the road
// renderer's two depth-bias states. Unlike roadQueuePass it writes no road fade constants.
void Renderer::roadScenePass()
{
    Ra::RaShader* const shader = Ra::getShader(static_cast<Ra::RaShaderTable*>(g_renderQueues),
                                               std::string("Road"), false);       // sub_10041F20
    if (!shader || !geom::g_RoadCompiledRenderer || !geom::g_Terrain)
        return;

    IRenderView* const view = rendererGetRenderViewDup();                         // base +0x50
    raConstantsAddGlobalTime(static_cast<float>(m_frameTime));                    // sub_100383D0
    raConstantsSetViewMatrix(view->getViewMatrixPtr()->m);                        // +0x64, sub_10037EE0
    Vec3 sun;
    static_cast<geom::Terrain*>(geom::g_Terrain)->getSunColorRaw(&sun);           // +0x78
    raConstantsSetSunColor(sun);                                                  // sub_100387E0

    postProductionUpdateFog(underwaterDepth());                                   // sub_1004FDA0

    stateManagerPush();
    stateManagerSetRenderState(195u, geom::roadCompiledRendererDepthBiasBits());            // D3DRS_DEPTHBIAS
    stateManagerSetRenderState(175u, geom::roadCompiledRendererSlopeScaleDepthBiasBits());  // slopescaledepthbias
    renderQueueFlush(0x24, 1);                                                    // RSMTRoad
    stateManagerPop();
}


// PE 0x1002BC50, slot +0x2C: render the world into a frame the caller closes with slot +0x30, as
// `flags` selects (beginScene opens it; its answer is not tested). `drawObject` reaches
// ObjectDrawer +0x20 when pass A runs without pass B. Every exit materializes AL=1.
// Unwritten callees keep a named stop at their call: sub_10029C50 (the +0x6AC overlay) and
// ClassManager +0x58 ("AI", aiDebug only).
bool Renderer::renderScene(double time, const RenderFlags* flags, void* drawObject)
{
    viewSetNearPlane(getRenderView(),    m_f590);                                 // +0x48 -> +0x30
    viewSetNearPlane(getRenderViewDup(), m_f590);                                 // +0x50 -> +0x30
    viewSetNearPlane(getRenderView2(),   m_near);                                 // +0xD4 -> +0x30
    viewSetFarPlane(getRenderView2(),    m_far - m_near);                         // +0x38
    {
        IRenderView* const source = getRenderViewDup();
        IRenderView* const target = getRenderView2();
        viewSetViewMatrix(target, viewGetMatrixPtr(source));                      // +0x64 -> +0x60
    }
    raConstantsSetViewMatrix(viewGetMatrixPtr(getRenderViewDup()));               // sub_10037EE0

    if (m_hemiScratchTexture)                                                     // +0x774
    {
        // The accumulation target is cleared to white: sub_10005920 on the base, sub_100057E0(1,
        // 0xFFFFFF, 0.0f, 0, 0, 0), then target 0 back to the back buffer.
        selectRenderTarget(static_cast<RendDX9Base*>(g_rendererBase), 0,
                           static_cast<Renderer*>(g_Renderer)->m_hemiScratchTexture, 0,
                           0x7FFFFFFFu);
        stateManagerClear(D3DCLEAR_TARGET, 0x00FFFFFFu, 0.0f, 0);
        selectRenderTarget(reinterpret_cast<RendDX9Base*>(this), 0, 0, 0, 0x7FFFFFFFu);
    }

    {
        IBase* const wind = classManager->getSingleton(renddx9abi::HostString("WindManager"));
        if (wind)
            hostCalls::windUpdate(wind, static_cast<float>(time));                // +0x10
    }

    m_frameTime = time;                                                           // +0x5E8
    const float timeF = static_cast<float>(time);

    if (flags->world)
    {
        publishBaseHemiMap();
        objectDrawerSetRenderView(m_objectDrawer, getRenderView());               // +0x28
        objectDrawer(m_objectDrawer)->updateVisible(timeF);                       // +0x14
        objectDrawer(m_objectDrawer)->draw(timeF);                                // +0x10
    }

    if (flags->undergrowth && geom::g_UndergrowthSystem)
        static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->update(timeF);   // +0x24

    if (geom::g_LightManager)
        geom::g_LightManager->update(time);                                       // +0x10

    if (geom::g_SkyDome && flags->skyDome)
        skyDomeUpdate(geom::g_SkyDome, timeF);                                    // +0x10

    IBase* const particles =
        classManager->getSingleton(renddx9abi::HostString("ParticleSystemManager"));
    if (particles)
    {
        if (flags->particles)
            geom::asParticleSystemManager(particles)->update(timeF);              // +0x28
        else
            geom::asParticleSystemManager(particles)->reserved_clearInternalVector();   // +0x40
    }

    {
        IBase* const decals = classManager->getSingleton(renddx9abi::HostString("DecalManager"));
        if (decals)
            static_cast<geom::IDecalManager*>(decals)->update(timeF);             // +0x30
    }

    if (flags->hud)
        hudManagerFramePrep(timeF);                                               // sub_10028EE0

    if (m_frameTimeOverlay)                                                       // +0x6AC
        rendererDrawFrameTimeOverlay(this);                                       // 0x1002BF27

    if (Rend::g_EnvMapManager)                                                    // dword_10267A34
        static_cast<Rend::EnvMapManager*>(Rend::g_EnvMapManager)->update(timeF);  // sub_101C0870

    // The owner's byte +0x24 - the flag postProductionFrameWorkIsActive tests - takes the
    // post-production switch whenever an owner is published.
    const unsigned char postProduction = flags->postProduction;
    if (g_PostProductionFrameWorkOwner)
        postProductionSetGlowEnabled(postProduction);                             // owner +0x24

    if (m_fullSizeRenderTargets && postProduction && g_PostProductionFrameWorkOwner)
    {
        postProductionPrepare();                                                  // sub_1004F210
    }
    else
    {
        EngineTexture*& target = pathTarget(this, 0x7D4);
        if (target)
        {
            textureReleaseRef(target);                                            // nulls +0x7D4
            selectRenderTarget(static_cast<RendDX9Base*>(g_rendererBase), 0, 0, 0, 0x7FFFFFFFu);
        }
    }

    // flags +0x14 is the clear colour beginScene receives.
    beginScene(flags->useRenderView ? static_cast<const unsigned int*>(flags->renderView) : 0,
               1);                                                                // +0xC8

    if (particles && flags->particles)
    {
        geom::particleSystemManagerBeginFrame(                                    // sub_100D2E80
            geom::asParticleSystemManager(geom::g_ParticleSystemManager));
    }
    m_byte6DC = flags->particles;                                                 // +0x6DC

    geom::g_LightManager->renderShadowMaps();                                     // +0x19C, untested

    if (flags->field0d && g_OverGrowth)
        static_cast<geom::OverGrowth*>(g_OverGrowth)->render(getRenderView(), timeF);   // +0x34

    const bool worldPass = flags->world || flags->worldAlt;
    if (worldPass)
    {
        objectDrawerSetRenderView(m_objectDrawer, getRenderView());               // +0x28
        if (flags->worldAlt)
        {
            objectDrawer(m_objectDrawer)->slot_0x1C();
        }
        else
        {
            if (drawObject)
            {
                objectDrawer(m_objectDrawer)->slot_0x20(drawObject);
            }
            objectDrawer(m_objectDrawer)->slot_0x18(timeF);
        }
    }

    if (flags->undergrowth && geom::g_UndergrowthSystem)
        static_cast<geom::UndergrowthSystem*>(geom::g_UndergrowthSystem)->render(timeF);   // +0x20

    if (flags->field01 && geom::g_Terrain)
    {
        static_cast<geom::Terrain*>(geom::g_Terrain)->m_flag35A = flags->field12;  // +0x35A
        static_cast<geom::Terrain*>(geom::g_Terrain)->slot11_noop();               // +0x2C
    }

    roadScenePass();                                                              // sub_10028330

    if (worldPass)
    {
        objectDrawerSetRenderView(m_objectDrawer, getRenderView());               // +0x28
        if (geom::g_StaticMeshRenderer)
            staticMeshRendererSetEnabled(flags->staticMeshes);                    // +0x18
        if (geom::g_SkinnedMeshRenderer && sw(1699))                              // +0x6A3
        {
            geom::skinnedMeshRendererSetEnabled(flags->skinnedMeshes);            // PE 0x1002C125, +0x10
            skinnedMeshRendererPathNotification();                                // listener +0x0C
        }
    }

    raConstantsAddGlobalTime(static_cast<float>(m_frameTime));                    // sub_100383D0
    {
        Vec3 black;
        black.x = 0.0f; black.y = 0.0f; black.z = 0.0f;
        raConstantsSetSunColor(black);                                            // sub_100387E0
    }

    // The light colours, from the LightManager (not null-tested), as renderFrame writes them.
    *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x1F8)) =
        geom::g_LightManager->getStaticSkyColor();                                // +0x1E8
    {
        const Vec3& treeSky = geom::g_LightManager->getTreeSkyColor();            // +0x1D0
        Vec3& treeSkyConstant = *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x154));
        treeSkyConstant.x = treeSky.x * 0.5f;
        treeSkyConstant.y = treeSky.y * 0.5f;
        treeSkyConstant.z = treeSky.z * 0.5f;
    }
    *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x214)) =
        geom::g_LightManager->getStaticSpecularColor();                           // +0x1E0
    *reinterpret_cast<Vec3*>(raShaderConstantsAt(0x230)) =
        geom::g_LightManager->getSinglePointColor();                              // +0x1F0

    // The colour-correction latch on the PostProductionFrameWork state vector (+0x28 get,
    // +0x24 set): without colour correction the current vector is saved once and a neutral one
    // set every call; with it, the saved vector is put back once.
    if (!flags->colourCorrection)
    {
        if (m_colourCorrectionLive)                                               // +0x80D
        {
            float current[4];
            postProductionGetHemiState(current);
            for (int i = 0; i < 4; ++i)
                m_savedColourCorrection[i] = current[i];                          // +0x810
            m_colourCorrectionLive = 0;
        }
        const float neutral[4] = { 0.0f, 10000.0f, 1.0f, 0.5f };
        postProductionSetHemiState(neutral);
    }
    else if (!m_colourCorrectionLive)
    {
        postProductionSetHemiState(m_savedColourCorrection);
        m_colourCorrectionLive = 1;
    }

    postProductionUpdateFog(underwaterDepth());                                   // sub_1004FDA0

    publishBaseHemiMap();

    renderQueuesFlushWorldPass();                                                 // sub_1003AD80

    if (geom::g_SkyDome && flags->skyDome)
    {
        RenderPathListener* const skyListener = static_cast<geom::SkyDome*>(geom::g_SkyDome);
        skyListener->onRenderPathNotification(0);                                 // +0x04 / +0x0C
        if (sw(1715))                                                             // +0x6B3
            skyDomeDrawPass(geom::g_SkyDome, 0);
    }

    if (!g_renderSceneSkipPostProduction && flags->postProduction
        && g_PostProductionFrameWorkOwner)
    {
        postProductionSceneGlowPass();                                            // sub_1004EF80
    }

    if (geom::g_StaticMeshRenderer)
        staticMeshRendererSetEnabled((flags->staticMeshes && worldPass) ? 1u : 0u);

    renderQueueFlush(0x28, -1);                                                   // RSMTWater
    renderQueueFlush(0x08, -1);                                                   // RSMTTransparency
    renderQueueFlush(0x18, -1);                                                   // RSMTDeltaTransparency

    if (geom::g_DecalManager && flags->decals)
    {
        RenderPathListener* const decalListener = static_cast<geom::DecalManager*>(
            static_cast<geom::IDecalManager*>(geom::g_DecalManager));
        decalListener->onRenderPathNotification(0);                               // +0x04 / +0x0C
    }

    if (worldPass)
        renderQueueFlush(0x2C, 1);                                                // RSMTAdditive

    if (particles && flags->particles)
        geom::asParticleSystemManager(particles)->onPathDraw(false, false);       // +0x2C

    if (sw(1714) && sw(1715))                                                     // +0x6B2, +0x6B3
        skyDomeDrawPass(geom::g_SkyDome, 1);

    renderQueueFlush(0x0C, 1);                                                    // RSMT1Person
    renderQueueFlush(0x1C, 1);                                                    // RSMTDelta1Person

    if (particles && flags->particles)
        geom::asParticleSystemManager(particles)->onPathDraw(true, false);

    renderQueueFlush(0x10, 1);                                                    // RSMT1PersonTransparency
    renderQueueFlush(0x20, 1);                                                    // RSMTDelta1PersonTransparency

    if (sw(1714) && sw(1715))
        skyDomeDrawPass(geom::g_SkyDome, 2);

    if (!g_renderSceneSkipPostProduction && postProduction && g_PostProductionFrameWorkOwner)
    {
        postProductionFinish();                                                   // sub_100512C0
    }

    // PE 0x1002C4E4..0x1002C531: with AI debugging on, an initialized "AI" singleton gets the
    // AI debug-draw event (category 3, id 4). Neither classManager nor eventManager is
    // null-tested.
    if (flags->aiDebug)
    {
        ai::IAI* const aiSingleton = static_cast<ai::IAI*>(
            classManager->getSimpleSingleton(renddx9abi::HostString("AI")));      // +0x58
        if (aiSingleton && aiSingleton->isInitialized())                          // PE +0x18
            ::dice::hfe::eventManager->postEvent(static_cast<EventCategory>(3), 4,
                                                 static_cast<void*>(0), 0.0f);    // +0x10
    }

    if (flags->hud && m_hudManager)
        hostCalls::hudPaint(m_hudManager);                                        // +0x1C

    *static_cast<unsigned int*>(g_renderEntryPool) = 0;                           // dword_1023DE08
    return true;
}

// PE 0x1002F770, slot +0x30: close the frame renderScene opened - the two debug overlays, the
// particle end-of-frame, endScene, the base +0x3F4 object's pass, then present(2). AL=1.
bool Renderer::slot_0x30()
{
    if (m_dword6D4)                                                               // +0x6D4
        drawStatisticsOverlay();
    if (sw(1706))                                                                 // +0x6AA
        drawStateStatisticsOverlay();
    if (m_byte6DC)                                                                // +0x6DC
    {
        geom::particleSystemManagerEndFrame(                                      // sub_100D2EA0
            geom::asParticleSystemManager(geom::g_ParticleSystemManager));
    }
    endScene();                                                                   // +0xCC
    if (MovieRenderer* const movie = static_cast<RendDX9Base*>(g_rendererBase)->movieRenderer)
        movie->captureFrame();                                                    // sub_10003510
    present(2);                                                                   // +0xD0
    return true;
}

// PE 0x1002D040, slot +0x34: draw the ObjectDrawer's object into swap chain 1, present it, and
// return to swap chain 0. The Renderer's back-buffer reference (+0x76C) follows the chain when it
// is held. `flags` +0x14 is the clear colour. AL=1.
bool Renderer::slot_0x34(void* drawObject, const RenderFlags* flags)
{
    RendDX9Base* const base = reinterpret_cast<RendDX9Base*>(this);
    slot_0x40(1);                                                                 // +0x40
    beginScene(static_cast<const unsigned int*>(flags->renderView), 1);           // +0xC8
    if (m_backBuffer)                                                             // +0x76C
    {
        m_backBuffer->Release();
        base->swapChains[1]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &m_backBuffer);
        selectRenderTarget(base, 0, 0, 0, 0x7FFFFFFFu);                           // sub_10005920
    }

    objectDrawerSetRenderView(m_objectDrawer, getRenderView());                   // +0x28
    if (drawObject)
    {
        objectDrawer(m_objectDrawer)->slot_0x20(drawObject);
    }
    if (geom::g_SkinnedMeshRenderer)
        skinnedMeshRendererPathNotification();                                    // listener +0x0C

    endScene();                                                                   // +0xCC
    present(2);                                                                   // +0xD0
    slot_0x40(0);
    if (m_backBuffer)
    {
        m_backBuffer->Release();
        base->device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &m_backBuffer);   // +0x48
        selectRenderTarget(base, 0, 0, 0, 0x7FFFFFFFu);
    }
    return true;
}

// PE 0x1002A4A0, slot +0x38: a frame that draws only the HUD. The HUD update receives the time
// since the previous such frame (System +0x14 clock) and the render view's look row; the mark at
// +0x5F8 moves on whether or not the scene opened.
void Renderer::slot_0x38()
{
    const double now = hostCalls::systemExactTime();                              // +0x14
    const double elapsed = now - m_hudFrameTime;                                  // +0x5F8
    if (m_hudManager)                                                             // +0x5D0
    {
        void* const player = hostCalls::localHumanPlayer();                       // +0x30
        const float* const m = viewGetMatrixPtr(getRenderView());                 // +0x48 -> +0x64
        hostCalls::hudUpdate(m_hudManager, static_cast<float>(elapsed), player,
                             m[8], m[9], m[10]);                                  // +0x28
    }
    if (beginScene(0, 1))                                                         // +0xC8
    {
        if (m_hudManager)
            hostCalls::hudPaint(m_hudManager);                                    // +0x1C
        endScene();                                                               // +0xCC
    }
    m_hudFrameTime = now;
}

// The display-mode enumeration (Renderer slot +0x3C, 0x10007420, and its helpers). Builds the
// Renderer's Mode40 list from the D3D adapter modes, cross-checked against the Win32 display list and
// annotated with each mode's supported depth-stencil formats. VideoOptions::buildResolutionList
// (sub_10010710) reads this list to fill the menu's resolution strings.

int bitsPerPixel(unsigned int d3dFormat);   // RendDX9Types.cpp - D3DFORMAT -> bpp

namespace {

#if !defined(__APPLE__)
// PE 0x1000CEE0: narrows an adapter display mode into the 40-byte mode record (width, height and
// refresh as words, the bit depth through bitsPerPixel of the format); the member vectors
// default-construct.
Mode40 modeFromAdapterMode(const D3DDISPLAYMODE& mode)
{
    Mode40 rec;
    rec.width   = static_cast<unsigned short>(mode.Width);
    rec.height  = static_cast<unsigned short>(mode.Height);
    rec.refresh = static_cast<unsigned short>(mode.RefreshRate);
    rec.bpp     = static_cast<unsigned char>(bitsPerPixel(mode.Format));
    return rec;
}
#endif

#if defined(__APPLE__)
// [CHOSEN, not recovered] G3 main-screen mode metadata. The native backend never
// switches display modes; records describe render sizes, not exclusive modes.
// [RETIRE-WHEN: macos-display-mode-policy] Validate this G3 policy on Intel and M2;
// settle the plan's fractional-resolution/Retina policy before retiring the debt.
void injectRequestedMode(RendDX9Base* b);
void buildModeListImpl(RendDX9Base* b)
{
    if (!b->modeList.empty() || !b->d3d9) return;
    const UINT count = b->d3d9->GetAdapterModeCount(b->adapter, D3DFMT_X8R8G8B8);
    for (UINT i = 0; i < count; ++i)
    {
        D3DDISPLAYMODE mode = {};
        if (FAILED(b->d3d9->EnumAdapterModes(b->adapter, D3DFMT_X8R8G8B8, i, &mode)) ||
            mode.Width > 65535 || mode.Height > 65535 || mode.RefreshRate > 65535)
            continue;
        Mode40 rec;
        rec.width = static_cast<unsigned short>(mode.Width);
        rec.height = static_cast<unsigned short>(mode.Height);
        rec.refresh = static_cast<unsigned short>(mode.RefreshRate);
        rec.bpp = 32;
        if (SUCCEEDED(b->d3d9->CheckDeviceFormat(b->adapter, static_cast<D3DDEVTYPE>(b->deviceType),
            mode.Format, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8)))
            rec.depthFormats.push_back(D3DFMT_D24S8);
        rec.multiSample.push_back({D3DMULTISAMPLE_NONE, 0});
        b->modeList.push_back(rec);
    }
    injectRequestedMode(b);
}

// [CHOSEN, not recovered] [RETIRE-WHEN: macos-display-mode-policy] The requested window size
// (GSWindowSizeX/Y, i.e. +szx/+szy, copied in by copyDisplayModeFrom) joins the list when the
// display lists no mode of that size. A Windows driver lists the classic sizes on every panel, so
// retail's own requests ("800x600", VideoSettings' "800x600@60Hz") always match; the M2's built-in
// panel has no 800x600, and setDynamicOption's miss then stops the game. This backend never
// switches display modes and presents at the panel's rate, so the size is added at the desktop
// rate and at the 60 Hz the game's defaults name.
void injectRequestedMode(RendDX9Base* b)
{
    const unsigned int w = b->modeWidth, h = b->modeHeight;
    if (w < 1 || h < 1 || w > 8192 || h > 8192)
        return;
    for (const Mode40& m : b->modeList)
        if (m.width == w && m.height == h)
            return;
    D3DDISPLAYMODE desktop = {};
    std::vector<unsigned short> rates(1, 60);
    if (SUCCEEDED(b->d3d9->GetAdapterDisplayMode(b->adapter, &desktop)) &&
        desktop.RefreshRate > 0 && desktop.RefreshRate != 60 && desktop.RefreshRate <= 65535)
        rates.push_back(static_cast<unsigned short>(desktop.RefreshRate));
    for (unsigned short hz : rates)
    {
        Mode40 rec;
        rec.width = static_cast<unsigned short>(w);
        rec.height = static_cast<unsigned short>(h);
        rec.refresh = hz;
        rec.bpp = 32;
        if (SUCCEEDED(b->d3d9->CheckDeviceFormat(b->adapter, static_cast<D3DDEVTYPE>(b->deviceType),
            D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL, D3DRTYPE_SURFACE, D3DFMT_D24S8)))
            rec.depthFormats.push_back(D3DFMT_D24S8);
        rec.multiSample.push_back({D3DMULTISAMPLE_NONE, 0});
        b->modeList.push_back(rec);
        ::renddx9abi::logf("buildModeList: requested %ux%u not listed by the display; added @%uHz",
                           w, h, static_cast<unsigned int>(hz));
    }
}
#else
// PE 0x1000CD20. Every depth other than 32 selects R5G6B5.
D3DFORMAT formatFromBpp(unsigned int bpp)
{
    return static_cast<D3DFORMAT>(2u * (bpp != 32u) + 21u);
}

// sub_10006990 - enumerate the adapter's modes for R5G6B5 (16-bit) then X8R8G8B8 (32-bit), keeping
// (when minRes) only >= 640x480 @ 60Hz. Appends each D3DDISPLAYMODE to `out`.
void enumAdapterModes(RendDX9Base* b, bool minRes, std::vector<D3DDISPLAYMODE>& out)
{
    const D3DFORMAT passes[2] = { D3DFMT_R5G6B5, D3DFMT_X8R8G8B8 };
    for (int p = 0; p < 2; ++p)
    {
        const UINT count = b->d3d9->GetAdapterModeCount(b->adapter, passes[p]);
        for (UINT i = 0; i < count; ++i)
        {
            D3DDISPLAYMODE m;
            if (FAILED(b->d3d9->EnumAdapterModes(b->adapter, passes[p], i, &m)))
                continue;
            if (!minRes || (m.Width >= 640 && m.Height >= 480 && m.RefreshRate >= 60))
                out.push_back(m);
        }
    }
}

// sub_10006B40 - the Win32 display list, used only to cross-check the D3D modes. Keeps 16/32bpp modes
// with no special flags, matching the min-resolution and (optional) 4:3 gates.
void enumDevModes(bool aspect43, bool minRes, std::vector<DEVMODEA>& out)
{
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
        out.push_back(dm);
    }
}

// sub_10006C60 - fill each mode's depth-stencil format list by probing D3DFMT_D32 (71) and
// D3DFMT_D24S8 (75) with CheckDeviceFormat; the mode's own bpp gives the adapter format.
void fillDepthFormats(RendDX9Base* b)
{
    const D3DFORMAT probes[2] = { D3DFMT_D32, D3DFMT_D24S8 };
    for (std::vector<Mode40>::iterator rec = b->modeList.begin(); rec != b->modeList.end(); ++rec)
    {
        const D3DFORMAT adapterFmt = formatFromBpp(rec->bpp);
        for (int k = 0; k < 2; ++k)
        {
            if (SUCCEEDED(b->d3d9->CheckDeviceFormat(b->adapter, static_cast<D3DDEVTYPE>(b->deviceType),
                                                     adapterFmt, D3DUSAGE_DEPTHSTENCIL,
                                                     D3DRTYPE_SURFACE, probes[k])))
                rec->depthFormats.push_back(static_cast<unsigned int>(probes[k]));
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
    if (!b->modeList.empty())
        return;      // already built
    if (!b->d3d9)
        return;

    std::vector<DEVMODEA>       devModes;
    std::vector<D3DDISPLAYMODE> adapterModes;

    enumDevModes(false /*aspect43*/, true /*minRes*/, devModes);
    enumAdapterModes(b, true /*minRes*/, adapterModes);
    const bool haveDevModes = !devModes.empty();

    for (std::vector<D3DDISPLAYMODE>::const_iterator m = adapterModes.begin();
         m != adapterModes.end(); ++m)
    {
        Mode40 rec = modeFromAdapterMode(*m);

        bool keep = !haveDevModes;   // no Win32 list -> keep every enumerated mode (the shipped v29 gate)
        if (haveDevModes)
        {
            for (std::vector<DEVMODEA>::const_iterator dm = devModes.begin(); dm != devModes.end();
                 ++dm)
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
            b->modeList.push_back(rec);
    }

    fillDepthFormats(b);
}
#endif

} // anonymous namespace

// the mode-list builder (Renderer vtable slot +0x3C, `retn 4`: one filter argument,
// IRenderer.h). Returns the base's std::vector<Mode40> (retail `this+0x90`), building the list
// whenever it is null or empty (0x10007439..0x1000746E) - in practice once, by
// VideoOptions::initialize. `this` is the base object, as for slot +0x74. [CHOSEN] this rebuild
// keeps its fixed (false, true, true) policy and ignores the filter bytes; see IRenderer.h for what
// each byte means to the shipped body.
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
// PE 0x10005C70, slot +0x40: render target 0 becomes swap chain `index`'s back buffer, and
// `index` the current chain (+0xF0). False, with the RendDX9Draw.cpp:219 log line, when the back
// buffer cannot be fetched. [CHOSEN, not recovered] placement: the attested __FILE__ is
// RendDX9Draw.cpp; the body stays with the Renderer's other slots in this TU.
bool Renderer::slot_0x40(unsigned int index)
{
    RendDX9Base* const base = reinterpret_cast<RendDX9Base*>(this);
    IDirect3DSurface9* surface = 0;
    if (FAILED(base->swapChains[index]->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &surface)))
    {
        ::renddx9abi::logf("RendDX9Draw.cpp:219 Failed to get swap chain back buffer. Chain no: %d",
                           static_cast<int>(index));                              // severity 6
        return false;
    }
    stateManagerSetRenderTarget(0, surface);                                      // StateManager +0x60
    surface->Release();
    base->currentSwapChain = index;                                               // +0xF0
    return true;
}
// the one-instruction frame ordinal getter. Renderer::update increments the same
// dword at +0x104, and the directional LightManager forwards it to IGeometry +0xAC.
unsigned int Renderer::getFrameOrdinal()
{
    return reinterpret_cast<RendDX9Base*>(this)->frameOrdinal;
}


} // namespace dx9rend - reopened below: the four console-object seams are geom's

namespace geom {
// FontSystem/FontSystem.cpp and OverlayTexture.cpp - typed virtual forwards onto the two console
// objects initializeFontAndConsoleOverlay created (held here as IBase*, as createInstance returned).
int  fontPortedGetHeight(IBase* font);                                            // Font::Ported +0x1C
void fontPortedDrawText(IBase* font, float x, float y, const std::string& text);  // Font::Ported +0x18
void overlayTextureSetSize(IBase* overlay, const Vec2& size);                      // OverlayTexture +0x18
void overlayTextureDraw(IBase* overlay);                                           // OverlayTexture +0x10
} // namespace geom

namespace dx9rend {


// PE 0x1002E2D0: draw the last 20 console lines, including the current prompt, over a full-width background.
// Re-read font height per line and restore statistics before destroying the line vector.
// Compat strings use the VC7.1 CRT; their vector storage uses the engine heap.
// [CHOSEN, not recovered] native lines are copied to std::string at the font boundary.
void Renderer::slot_0x58()
{
    const unsigned char statsWere = statsSwapEnabled(0);

    std::vector<std::string> lines;
    reinterpret_cast<io::Console*>(::dice::hfe::mainConsole)->getLines(20, lines);
    const unsigned int lineCount = static_cast<unsigned int>(lines.size());

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
    for (std::vector<std::string>::const_iterator it = lines.begin(); it != lines.end();
         ++it, ++line)
    {
        const std::string text = ::renddx9abi::fromHostString(*it);
        const int height = geom::fontPortedGetHeight(font);                    // re-read per line
        geom::fontPortedDrawText(font, 2.0f, static_cast<float>((height + 1) * line + 2), text);
    }

    statsSwapEnabled(statsWere);

}   // native: ~vector<std::string>

// PE 0x10005010, slot +0x5C (`retn 18h`, AL = 1 on every exit): copy the named texture's top
// level into the GDI bitmap `bitmap` (an HBITMAP): a scratch A8R8G8B8 surface of the bitmap's size
// receives the level through D3DXLoadSurfaceFromSurface (filter 0x80004), and SetBitmapBits copies
// bmWidthBytes * bmHeight bytes from it - or, with `alphaToGrey`, from a buffer whose blue, green
// and red bytes of each pixel are that pixel's alpha (the fourth byte is left unwritten). `width`
// and `height`, when non-null, receive the texture's size. `reupload` first re-uploads the texture
// through the TextureManager. Texture-manager "managed" mode is forced on for the load. No caller
// is known. [INFERRED] parameter names.
bool Renderer::slot_0x5C(const std::string& name, void* bitmap, bool reupload, bool alphaToGrey,
                         unsigned int* width, unsigned int* height)
{
#if defined(_WIN32)
    const int savedManaged = textureManagerSwapManaged(1);                       // sub_10017E20
    BITMAP info;
    ::GetObjectA(static_cast<HBITMAP>(bitmap), sizeof(BITMAP), &info);           // 0x18 bytes
    IDirect3DSurface9* scratch = 0;
    reinterpret_cast<RendDX9Base*>(this)->device->CreateOffscreenPlainSurface(
        static_cast<UINT>(info.bmWidth), static_cast<UINT>(info.bmHeight), D3DFMT_A8R8G8B8,
        D3DPOOL_SCRATCH, &scratch, 0);                                            // +0x90
    if (reupload)
        textureManagerReupload(name, 0);                    // sub_10018C50, result discarded
    EngineTexture* const texture =
        static_cast<EngineTexture*>(textureManagerGetTexture(name));             // sub_10019260
    IDirect3DTexture9* texture2D = 0;
    texture->baseTexture->QueryInterface(kIID_Texture9,                         // 0x10208580
                                         reinterpret_cast<void**>(&texture2D));   // sub_10014930
    IDirect3DSurface9* level = 0;
    texture2D->GetSurfaceLevel(0, &level);                                        // +0x48
    texture2D->Release();
    const unsigned int* const dims = textureDims(texture);                        // sub_10014940
    if (width)
        *width = dims[0];
    if (height)
        *height = dims[1];
    D3DXLoadSurfaceFromSurface(scratch, 0, 0, level, 0, 0, 0x80004u, 0);
    level->Release();
    if (--texture->refCount == 0)
    {
        textureDtor(texture);                                                     // sub_10014E20
        ::renddx9abi::engineFree(texture);
    }

    D3DLOCKED_RECT locked;
    scratch->LockRect(&locked, 0, D3DLOCK_READONLY);                              // +0x34
    const int bytes = info.bmWidthBytes * info.bmHeight;
    if (alphaToGrey)
    {
        unsigned char* const grey = static_cast<unsigned char*>(
            ::renddx9abi::engineAlloc(static_cast<unsigned int>(bytes)));         // sub_1005F340
        if (bytes > 0)
        {
            const unsigned char* source = static_cast<const unsigned char*>(locked.pBits) + 3;
            unsigned char* out = grey;
            for (unsigned int n = ((static_cast<unsigned int>(bytes) - 1u) >> 2) + 1u; n; --n)
            {
                const unsigned char alpha = *source;
                out[2] = alpha;
                out[1] = alpha;
                out[0] = alpha;
                source += 4;
                out += 4;
            }
        }
        ::SetBitmapBits(static_cast<HBITMAP>(bitmap), static_cast<DWORD>(bytes), grey);
        ::renddx9abi::engineFree(grey);                                           // sub_1005F370
    }
    else
    {
        ::SetBitmapBits(static_cast<HBITMAP>(bitmap), static_cast<DWORD>(bytes), locked.pBits);
    }
    scratch->UnlockRect();                                                        // +0x38
    scratch->Release();
    textureManagerSwapManaged(savedManaged);
    return true;
#else
    // [CHOSEN, not recovered] PE 0x10005010 requires a Windows GDI bitmap target.
    // This platform has no such target; refuse without writing output dimensions.
    (void)name; (void)bitmap; (void)reupload; (void)alphaToGrey; (void)width; (void)height;
    std::fprintf(stderr, "[CHOSEN, not recovered] dice::hfe::dx9rend::Renderer::slot_0x5C: "
                         "GDI bitmap capture unavailable on this platform\n");
    std::fflush(stderr);
    return false;
#endif
}
// PE 0x1002C820, slot +0x60: a non-zero first argument stores the seven-argument set
// (+0x7F4..+0x80C) and arms +0x7F0 for beginFrame; a zero one replays the stored set through
// sub_1002A120, the top-down capture (the zero set beginFrame passes is otherwise unused).
void Renderer::slot_0x60(unsigned int size, void* target, float x, float y, float z,
                         float orthoExtent, bool drawObjects)
{
    if (size == 0)
    {
        rendererRenderTopDownToTexture(this, m_frameHookA, m_frameHookB, m_frameHookVec[0],
                                       m_frameHookVec[1], m_frameHookVec[2], m_frameHookC,
                                       m_frameHookD != 0);                       // sub_1002A120
        return;
    }
    m_frameHookA = size;                                                          // +0x7F4
    m_frameHookB = target;                                                        // +0x7F8
    m_frameHookVec[0] = x;                                                        // +0x7FC
    m_frameHookVec[1] = y;                                                        // +0x800
    m_frameHookVec[2] = z;                                                        // +0x804
    m_frameHookC = orthoExtent;                                                   // +0x808
    m_frameHookD = drawObjects ? 1u : 0u;                                         // +0x80C
    m_frameHookEnabled = 1;                                                       // +0x7F0
}

// PE 0x10004FC0, slot +0x64: drop the current movie recorder (slot +0x68), then build a new one
// (operator new(0x44), sub_10003390) from the demo's render settings. A failed allocation leaves
// the slot null.
void Renderer::slot_0x64(const DemoConfig* config)
{
    slot_0x68();                                                                  // +0x68
    RendDX9Base* const base = reinterpret_cast<RendDX9Base*>(this);
    void* const storage = ::renddx9abi::engineAlloc(sizeof(MovieRenderer));
    base->movieRenderer = storage ? new (storage) MovieRenderer(base, config) : 0;  // +0x3F4
}

// PE 0x100051D0, slot +0x68: destroy and free the movie recorder (sub_100031B0), then clear the
// slot. renderer.stopRenderMovie runs it, and the base destructor calls it directly (0x1000BC2F).
void destroyMovieRenderer(RendDX9Base* base)
{
    if (MovieRenderer* const movie = base->movieRenderer)                         // +0x3F4
    {
        movie->~MovieRenderer();
        ::renddx9abi::engineFree(movie);
        base->movieRenderer = 0;
    }
}

void Renderer::slot_0x68()
{
    destroyMovieRenderer(reinterpret_cast<RendDX9Base*>(this));
}
// 0x101432E0, slot +0x6C.  One instruction - `lea eax, [ecx+108h]` - the
// address of the draw/resource statistics block embedded at +0x108.  Texture.cpp already documents
// that block (its +0x1C counts vertex buffers, its +0x2C textures); RenderQueues.cpp bumps its
// per-category draw counters through this slot when an entry is added to a queue.
void* Renderer::getRenderStats()
{
    return reinterpret_cast<RendDX9Base*>(this)->renderStats;
}
// PE 0x10032700, slot +0x70: `mov al, [ecx+7F0h]` - whether slot +0x60 has armed the hook.
bool Renderer::slot_0x70()
{
    return m_frameHookEnabled != 0;
}
// one instruction: `return this + 1016`. The embedded VideoOptions getter, identical to
// the base's own +0x74. VideoOptions is constructed inside this object at +0x3F8 (1016) - the same
// sub-object VideoOptions::apply reaches at `this + 0x3F8` - with its 15-slot vtable installed, so the
// pointer handed back is live. A menu screen fetches it here to read the current resolution/quality.
IVideoOptions* Renderer::getVideoOptions()
{
    return reinterpret_cast<RendDX9Base*>(this)->embeddedVideoOptions();
}
// PE 0x100326E0 / 0x100326F0, slots +0x78 / +0x7C: the third fog-override float (+0x6C8, the
// one the fog block hands ObjectManager +184; ctor 100).
void Renderer::slot_0x78(float value)
{
    m_fogOverride[2] = value;
}

float Renderer::slot_0x7C()
{
    return m_fogOverride[2];
}

// PE 0x100285A0..0x10028600, slots +0x80..+0x8C: the RoadCompiledRenderer's view-distance
// scale (+0x8C) and depth bias (+0x90). The setters store only while the road renderer exists; the
// getters answer 0.0f (0x101DDE64) without one.
void Renderer::slot_0x80(float value)
{
    if (geom::g_RoadCompiledRenderer)
        geom::roadCompiledRendererSetViewDistanceScale(value);
}

float Renderer::slot_0x84()
{
    if (!geom::g_RoadCompiledRenderer)
        return 0.0f;
    return geom::roadCompiledRendererViewDistanceScale();
}

void Renderer::slot_0x88(float value)
{
    if (geom::g_RoadCompiledRenderer)
        geom::roadCompiledRendererSetDepthBias(value);
}

float Renderer::slot_0x8C()
{
    if (!geom::g_RoadCompiledRenderer)
        return 0.0f;
    const unsigned int bits = geom::roadCompiledRendererDepthBiasBits();
    float value;
    ::memcpy(&value, &bits, sizeof(value));
    return value;
}

// PE 0x10028620 / 0x10028640, slots +0x90 / +0x94: tail calls into the LightManager's
// setDynamicShadowsEnabled (+0xEC) / getDynamicShadowsEnabled (+0xF0); false without one.
void Renderer::slot_0x90(bool enabled)
{
    if (geom::g_LightManager)
        geom::g_LightManager->setDynamicShadowsEnabled(enabled);
}

bool Renderer::slot_0x94()
{
    if (geom::g_LightManager)
        return geom::g_LightManager->getDynamicShadowsEnabled();
    return false;
}

// PE 0x10028660 / 0x10028680, slots +0x98 / +0x9C: the Terrain's +0x35E byte, set through
// sub_100D89C0 and read directly; false without a Terrain.
void Renderer::slot_0x98(bool value)
{
    if (geom::g_Terrain)
        terrainSetLowQualityB(static_cast<geom::Terrain*>(geom::g_Terrain), value ? 1u : 0u);
}

bool Renderer::slot_0x9C()
{
    if (geom::g_Terrain)
        return static_cast<geom::Terrain*>(geom::g_Terrain)->m_lowQualityB != 0;
    return false;
}

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


// PE 0x10032740, slot +0xAC. Selects the base-game or expansion shader path.
bool Renderer::isExpansionPack()
{
    return m_expansionPack != 0;
}

// Prefixes paths without "mods/" using the current expansion mode and shared scratch string.
// [INFERRED] Helper name; file placement follows the rdebug neighbour mapping.
// PE 0x10029D00.
void rendererPatchModPath(std::string& path)
{
    static std::string prefixed;
    if (!::strstr(path.c_str(), "mods/"))
    {
        prefixed = !g_Renderer || static_cast<IRenderer*>(g_Renderer)->isExpansionPack()
            ? "mods/xpack/" : "mods/bf2/";
        prefixed += path;
        path = prefixed;
    }
}


// PE 0x10028310. Sets expansion mode and returns whether it changed.

bool Renderer::setExpansionPack(unsigned char value)
{
    if (m_expansionPack == value)
        return false;
    m_expansionPack = value;
    return true;
}


// Returns the constructor-initialized word at +0x768 (normally 2).
// [UNRESOLVED] Its meaning; the unsigned carrier is [CHOSEN].

unsigned int Renderer::slot_0xB0()
{
    return m_dword768;
}
// PE 0x10032C80, slot +0xB4: the destructor 0x1002DA60 (the Renderer's body and members, then
// the base destructor 0x1000BC20), and the free when bit 0 is set.
void* Renderer::destroy(unsigned int flags)
{
    this->~Renderer();
    destructD3DHalf(this);
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}

// flush cached texture references while keeping the device alive.
// Clear effect bindings, optionally reset effects, then run StateManager lost/reset.
// Retail tests the flag twice; EffectManager's reset listener is empty. Receivers are
// unguarded globals. CEUnloadLevel passes false before the outgoing texture sweep.
// PE 0x10009B40 (listing read).
void Renderer::slot_0xB8(bool includeEffectManager)
{
    shaderManagerClearEffectTextureBindings(g_shaderManager);

    DeviceLostResetListener* const stateListener =
        stateManagerDeviceLostListener();                                     // dword_1023B984 + 4

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

// PE 0x100055B0.
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


typedef ::dice::hfe::IModInfo::Map HostLevelMap;
typedef ::dice::hfe::IModInfo::Mod HostModRecord;


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
    std::string modDirectory;
    readStringSettingDefault("GSModDirectory", "", modDirectory);

    if (levelName)
    {
        HostLevelMap  map;              // IModInfo::Map::Map(), RendDX9.dll COMDAT copy PE 0x10043C70
        map.path = levelName;
        HostModRecord mod;

        IBase* const swiffHost = classManager
            ? classManager->getSingleton(renddx9abi::HostString("SwiffHost"))
            : 0;

        // +0x120 getModInfo, then +0x50 parseMapDesc(Mod*, const string& levelsPath, Map*).
        // Typed in the native flavour (RW-266): ISwiffHost slot 72 and ModInfo slot 20, whose Mod /
        // Map / std::string ARE the host's here; the retail-ABI flavour keeps both retail slots.
        ModInfo* const modInfo = static_cast<ModInfo*>(static_cast<IModInfo*>(
            static_cast<ui::ISwiffHost*>(swiffHost)->getModInfo()));

        // var_70 = modDir; var_70 += "/levels/"; var_1C = var_70 - the PE copies it too.
        renddx9abi::HostString levelsPath(modDirectory.c_str());
        levelsPath += "/levels/";

        modInfo->parseMapDesc(&mod, levelsPath, &map);                                 // +0x50

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
        geom::asWeatherManager(m_weatherManager)->reset();                              // +0x14
    if (m_rainManager)
        geom::asRainManager(m_rainManager)->reset();                                    // +0x14

    IBase* const gtm = geom::g_GeometryTemplateManager;
    static_cast<geom::IGeometryTemplateManager*>(gtm)->clear();

    if (selectShaderArchive(levelName) || g_shaderSystemReloadPending)
    {
        // `mov ecx, dword_1023B97C` with nothing pushed: the callee re-reads the base object from
        // dword_1023B970, which is g_rendererBase - the same Renderer this method is running on.
        updateShaderDefines(g_shaderManager, g_rendererBase);

        shaderManagerReloadAllEffects(g_shaderManager);                  // sub_10023540
        // Reset is repeated inside the load pass. Keep both calls; loading registers
        // metashaders required before geometry technique masks are built.
        Ra::effectManagerResetEffects();
        Ra::raLoadAllShaders(static_cast<Ra::RaShaderTable*>(g_renderQueues), 3);

        g_shaderSystemReloadPending = 0;
    }

    reinterpret_cast<VoidFn>(
        (*reinterpret_cast<void***>(gtm))[0x48 / 4])(gtm);       // the technique re-arm
}
// PE 0x100287B0, slot +0xD8; the destructor (0x1002DA60) runs it too. Unregisters the ECCore
// handler, releases the console font and overlay, drops the five frame fences (the pointers are
// not cleared), leaves both listener lists, then shuts the D3D subsystems down.
// +0xDC [CHOSEN, not recovered] Port addition (IRenderer.h): the adapter's desktop mode as the
// renderer's own GetAdapterDisplayMode calls see it (createD3DDeviceAndSwapChains, PE 0x1000A9C0).
bool Renderer::getDesktopMode(unsigned int& width, unsigned int& height, unsigned int& refreshHz)
{
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(this);
    D3DDISPLAYMODE mode = {};
    if (!b->d3d9 || FAILED(b->d3d9->GetAdapterDisplayMode(b->adapter, &mode)) || !mode.Width || !mode.Height)
        return false;
    width = mode.Width;
    height = mode.Height;
    refreshHz = mode.RefreshRate;
    return true;
}

void Renderer::shutdown()
{
    ::dice::hfe::eventManager->unregisterEventHandler(
        ECCore, reinterpret_cast<IEventListener*>(&m_evHandlerVptr));             // +0x30
    if (m_consoleFont)                                                            // +0x5CC
        static_cast<IBase*>(m_consoleFont)->release();
    if (m_consoleOverlay)                                                         // +0x5C8
        static_cast<IBase*>(m_consoleOverlay)->release();
    for (unsigned int i = 0; i < 5; ++i)                                          // +0x5A4
    {
        if (m_frameFences[i])
            queryReleaseRef(m_frameFences[i]);                                    // sub_10003FA0
    }
    unregisterRenderPathListener(static_cast<void*>(&m_renderPathVptr));          // +0x540 from +0x6C
    unregisterDeviceLostResetListener(static_cast<void*>(&m_deviceLostVptr));     // +0x53C from +0x5C
    RendDX9_shutdownD3DSubsystems(this);                                          // result discarded
}

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


// getClassList, PE 0x10051BB0. ONE class, and the whole body is four statements.
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

// Defined in PostProductionFrameWork.cpp: the 0x1004FFA0 constructor. Takes the pre-allocated
// owner block (548 bytes on x86), installs the three interface vtables and the scalar defaults,
// acquires the PostProduction effect, its parameter handles and the helper textures, and registers
// the ECGameLogic and device-lost listeners. Returns the owner.
void* postProductionFrameWorkOwnerCtor(void* mem);


// init, 0x100519B0.

bool DX9RendModule::init()
{
    RD9_TRACE("dice::hfe::dx9rend::DX9RendModule::init", 0x18);

    // 548 bytes, and the singleton published is the interior pointer owner + 8 (a subobject in a
    // three-interface object). The constructor at PostProductionFrameWorkOwner_ctor -
    // is recovered in PostProductionFrameWork.cpp; it installs the three vtables so BF2's virtual
    // calls through the singleton (the per-frame gas-mask blend among them) dispatch into recovered
    // bodies rather than faulting on a null vptr. The owner TU supplies the size and the interface
    // subobject: 548 and owner + 8 on x86, the native layout on x64 (issue #7).
    void* owner = ::renddx9abi::engineAlloc(postProductionFrameWorkOwnerSize());
    if (owner)
    {
        postProductionFrameWorkOwnerCtor(owner);
        g_PostProductionFrameWorkOwner = reinterpret_cast<IBase*>(owner);
        g_PostProductionFrameWork      =
            reinterpret_cast<IBase*>(postProductionFrameWorkInterface(owner));
    }
    else
    {
        g_PostProductionFrameWorkOwner = 0;
        g_PostProductionFrameWork      = 0;
    }

    // PE 0x100519EF..0x10051A27: the 20-byte post-production mode selector (0x1004E100).
    g_renderTargetSystem = postProductionModeSelectorCreate();

    // Register bindings only for the matching native string/container ABI.
    registerRendererConsoleObjects();

    ::renddx9abi::logf("DX9RendModule::init: PostProductionFrameWork published at 0x%08X "
                       "(owner 0x%08X + 8)",
                       ::renddx9abi::addr(g_PostProductionFrameWork),
                       ::renddx9abi::addr(g_PostProductionFrameWorkOwner));

    return true;     // the body ends "return 1"
}

// PostProductionFrameWork.cpp - the owner's destructor body (0x10050830): detach both listeners,
// release the textures, parameter handles and effect, and end the string and deque members.
void postProductionFrameWorkOwnerDetach(void* owner);


// close, the reverse of init.

bool DX9RendModule::close()
{
    RD9_TRACE("dice::hfe::dx9rend::DX9RendModule::close", 0x1C);

    // the list at 0x10241668 leaves the registry first (0x1007B6D0), before the
    // 20-byte object and the owner are torn down.
    unregisterRendererConsoleObjects();

    // PE 0x10051A50..0x10051A6B: the mode selector (0x1004DEE0, then its storage).
    if (g_renderTargetSystem)
    {
        postProductionModeSelectorDestroy(g_renderTargetSystem);
        g_renderTargetSystem = 0;
    }

    if (g_PostProductionFrameWorkOwner)
    {
        // The owner's destructor body, then its storage.
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
