// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\Geom\ModuleGeom.cpp"
//           appears verbatim in RendDX9.dll at 0x1020DB18.
// Contents: recovered from RendDX9.dll (PE, symbols stripped).

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginModules.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71Map.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "XpLightManager.h"
#include "HemiMapManager/HemiMapDependencies.h"
#include "HemiMapManager/HemiMapManager.h"
#include "Terrain.h"
#include "ITerrainEditable.h"
#include "TerrainEditable.h"

namespace dice {
namespace hfe {
namespace geom {

extern IBase* g_GeometryTemplateManager;

#if BF2_PLUGIN_ABI_NATIVE
bool registerLightManagerConsoleObjects();
bool unregisterLightManagerConsoleObjects();
bool registerShadowManagerConsoleObjects();
bool unregisterShadowManagerConsoleObjects();
bool registerHemiMapManagerConsoleObjects();
bool unregisterHemiMapManagerConsoleObjects();
bool registerGeometryTemplateConsoleObjects();
bool unregisterGeometryTemplateConsoleObjects();
bool registerSkyDomeConsoleObjects();
bool unregisterSkyDomeConsoleObjects();
bool registerTerrainConsoleObjects();
bool unregisterTerrainConsoleObjects();
bool registerNametagManagerConsoleObjects();      // Geom/NametagManagerConsole.cpp - `nametags`
bool unregisterNametagManagerConsoleObjects();
bool registerUndergrowthConsoleObjects();       // Geom/UndergrowthSystemConsole.cpp - `undergrowth`
bool unregisterUndergrowthConsoleObjects();
bool registerDecalManagerConsoleObjects();      // Geom/DecalSystem/DecalManagerConsole.cpp - `DecalManager`
bool unregisterDecalManagerConsoleObjects();
bool registerOverGrowthConsoleObjects();        // Geom/Overgrowth/OverGrowthConsole.cpp - `Overgrowth`, `OvergrowthType`
bool unregisterOverGrowthConsoleObjects();
bool registerObjectGeometryConsoleObjects();   // Geom/ObjectGeometryConsole.cpp - `Object.geometry.*`
bool unregisterObjectGeometryConsoleObjects();
bool registerStaticMeshRendererConsoleObjects();   // Geom/StaticMeshRendererConsole.cpp - the atlas rows
bool unregisterStaticMeshRendererConsoleObjects();
#endif
#if BF2_PLUGIN_ABI_NATIVE && defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// Geom/LightmapGeneration/LightmapGenerationConsole.cpp - the 2006 editor's `lightmapGeneration`
// rows, editor flavour only ([CHOSEN, not recovered]: 1.50 publishes none).
bool registerLightmapGenerationConsoleObjects();
bool unregisterLightmapGenerationConsoleObjects();
#endif

// Both already in Code/BF2/Core/Ids.h from the ELF, and both confirmed here:
//   the classId is read AS data, not as an immediate - getClassList does `push ds:dword_1021AEF4`
//   at 0x100C7FE2, and that dword is 29 A3 0C 87 = 0x870CA329.
//   the iid is the one queryInterface (0x100C7ED0) tests, as the signed -1382652484.
// RoadCompiledRenderer.cpp - see the registration below.
extern const unsigned int CID_RoadCompiledRenderer;
IBase* __fastcall createRoadCompiledRenderer(unsigned int iid, IBase* outer);

// RoadCompiledTemplate.cpp - GeometryTemplate.RoadCompiled (821901), the template every stock road
// names. see the class list below.
extern const unsigned int CID_RoadCompiledTemplate;
IBase* __fastcall createRoadCompiledTemplate(unsigned int iid, IBase* outer);

// RoadEditableRenderer.cpp / RoadTemplate.cpp - the editor's road queue (821913, the
// "RoadEditableRenderer" singleton below) and dice.hfe.geom.Roads.RoadTemplate (821008).
// Both classes are registered in the class list below.
extern const unsigned int CID_RoadEditableRenderer;
IBase* __fastcall createRoadEditableRenderer(unsigned int iid, IBase* outer);
extern const unsigned int CID_RoadTemplate;
IBase* __fastcall createRoadTemplate(unsigned int iid, IBase* outer);
// Editable-road class and factory.
extern const unsigned int CID_RoadEditable;
IBase* __fastcall createRoadEditable(unsigned int iid, IBase* outer);

// DecalSystem/DecalManager.cpp - because joining A server crashed without it:
// BF2's in-game HUD does getSingleton("DecalManager") and dereferences the result unchecked
// (0x004F6FD2). The singleton entry is already in getSingletonList; this supplies its factory.
extern const unsigned int CID_DecalManager;
IBase* __fastcall createDecalManager(unsigned int iid, IBase* outer);

// The gameplay-rendering singleton batch. Each is a faithful minimal class - the
// reference spine + registration are real, the render path is a named frontier. See the per-class
// files. SkyDome and the weather/undergrowth group remain deferred.
extern const unsigned int CID_ParticleSystemManager;
IBase* __fastcall createParticleSystemManager(unsigned int iid, IBase* outer);
extern const unsigned int CID_StaticMeshRenderer;
IBase* __fastcall createStaticMeshRenderer(unsigned int iid, IBase* outer);
extern const unsigned int CID_SkinnedMeshRenderer;
IBase* __fastcall createSkinnedMeshRenderer(unsigned int iid, IBase* outer);

// Sky-dome factory; construction requires shader loading and parameter lookup.
IBase* __fastcall createSkyDome(unsigned int iid, IBase* outer);

// Factory for the undergrowth singleton and its grass resources.
IBase* __fastcall createUndergrowthSystem(unsigned int iid, IBase* outer);

// Factory for static-mesh templates and their material/technique state.
IBase* __fastcall createStaticMeshTemplate(unsigned int iid, IBase* outer);

// Factories for skinned and bundled mesh templates used by world geometry.
IBase* __fastcall createSkinnedMeshTemplate(unsigned int iid, IBase* outer);
IBase* __fastcall createBundledMeshTemplate(unsigned int iid, IBase* outer);

// Factory for mesh-particle templates, published with MeshParticlesRenderer.
IBase* __fastcall createMeshParticleMeshTemplate(unsigned int iid, IBase* outer);

// Factory for scene traversal and geometry submission. Occlusion fallback reports and
// continues with all frustum-visible candidates.
// [RETIRE-WHEN:
//     RW-objectdrawer-occluders] Replace the fallback after silhouette construction and
// occluder culling are implemented and validated.
IBase* __fastcall createObjectDrawer(unsigned int iid, IBase* outer);

// Factory for the manager coordinating nametag updates and renderer buffering.
IBase* __fastcall createNametagManager(unsigned int iid, IBase* outer);

// The core-event handler publishes the SwiffRender singleton dynamically.
extern const unsigned int CID_SwiffRender;
extern const unsigned int IID_ISwiffRender;
IBase* __fastcall createSwiffRender(unsigned int iid, IBase* outer);

// GeometryTemplateManager.cpp - now in its attested source home.  The first create/find/active and
// ownership tranche made the old in-ModuleGeom skeleton too large to keep here honestly (RW-51).
extern const unsigned int IID_IGeometryTemplateManager;
extern const unsigned int CID_GeometryTemplateManager;
IBase* __fastcall createGeometryTemplateManager(unsigned int iid, IBase* outer);

// dice.hfe.geom.ObjectDrawer, declared in PluginModules.h. The class id is read from this module's
// own getClassList (sub_100518A0(-2029213122, "dice.hfe.geom.ObjectDrawer", ...)); the interface id
// is the one Renderer::initialize pairs with it at 0x1002CB3C.

extern const unsigned int CID_ObjectDrawer  = 2265754174u;
extern const unsigned int IID_IObjectDrawer = 3087129852u;


// The sixteen singleton globals ModuleGeom registers. Each is the IBase a SingletonRegInfo points
// at, so ClassManager writes the created object straight into it.

LightManager* g_LightManager    = 0;
IBase* g_GeometryTemplateManager = 0;
IBase* g_StaticMeshRenderer     = 0;
IBase* g_SkinnedMeshRenderer    = 0;
IBase* g_ParticleSystemManager  = 0;
IHemiMapManager* g_HemiMapManager = 0;
IBase* g_DecalManager           = 0;
IBase* g_DecalShadowManager     = 0;
IBase* g_RoadCompiledRenderer   = 0;
IBase* g_RoadEditableRenderer   = 0;
IBase* g_NametagManager         = 0;
IBase* g_SkyDome                = 0;
IBase* g_UndergrowthSystem      = 0;
IBase* g_LightningManager       = 0;
IBase* g_WeatherManager         = 0;
IBase* g_RainManager            = 0;

// dword_1024B930. Terrain is registered dynamically rather than through the sixteen-entry static
// list above: the level's Terrain.con runs `terrain.create Terrain`, whose executor calls the
// body below. Null until then, and again after close() unregisters the singleton.
IBase* g_Terrain                = 0;

// Replaces Terrain with the requested registered geometry class. Unknown names publish nothing.
void registerTerrainSingleton(const vc71::string& className)
{
    const bool exists =
        classManager->getSingleton(renddx9abi::HostString("Terrain")) != 0;
    if (exists)
        classManager->unregisterSingleton(renddx9abi::HostString("Terrain"));

    vc71::string fullName("dice.hfe.geom.");
    fullName += className;
    unsigned int classId = 0;
    if (classManager->getClassID(renddx9abi::HostString(fullName.c_str()), &classId))
    {
        SingletonRegInfo info;
        info.name     = "Terrain";
        info.instance = &g_Terrain;
        info.iid      = 820004u;
        info.classId  = classId;
        info.flags    = 2;
        classManager->registerSingleton(info);
    }
}

HemiTerrainOverride beginHemiTerrainOverride()
{
    // Temporarily selects technique zero only when the terrain exposes ITerrainEditable.
    HemiTerrainOverride state = { 0, 0 };
    if (!g_Terrain)
        return state;
    ITerrainEditable* const editable =
        static_cast<ITerrainEditable*>(g_Terrain->queryInterface(kIID_ITerrainEditable));
    state.editable = editable;
    if (!editable)
        return state;
    state.previousTechnique = editable->getShaderTechnique();             // +0x8C
    editable->setShaderTechnique(0);                                      // +0x88
    return state;
}

void endHemiTerrainOverride(const HemiTerrainOverride& state)
{
    // PE 0x1012BC94..0x1012BCA1, after the state pop: the saved technique back.
    if (state.editable)
        state.editable->setShaderTechnique(state.previousTechnique);
}


// dice::hfe::geom::ModuleGeom - primary vtable 0x101E2EC4, object 12 bytes


RD9_MODULE_IBASE_BODIES(ModuleGeom)

// The SwiffRender singleton slot the core-event handler publishes into. dword_1024B920.
IBase* g_SwiffRender = 0;


// Event (10, 1) registers and immediately creates SwiffRender after SwiffPlayer loads.
// Event (10, 2) unregisters it and clears the singleton pointer.

void ModuleGeom::handleCoreModuleEvent(int category, int id, int a3, int a4)
{
    if (category != 10)
        return;

    if (id == 1)
    {
        if (!classManager)
            return;
        SingletonRegInfo info;
        info.name     = "SwiffRender";
        info.instance = &g_SwiffRender;
        info.iid      = IID_ISwiffRender;
        info.classId  = CID_SwiffRender;
        info.flags    = 2;
        classManager->registerSingleton(info);
        ::renddx9abi::logf("ModuleGeom::handleCoreModuleEvent(10,1): SwiffRender registered -> "
                           "instance 0x%08X", ::renddx9abi::addr(g_SwiffRender));
    }
    else if (id == 2)
    {
        if (classManager)
            classManager->unregisterSingleton(renddx9abi::HostString("SwiffRender"));
        g_SwiffRender = 0;
    }
}

// Appends the geometry factories available to ClassManager in registration order.
// [FRONTIER] dice::hfe::geom::ModuleGeom::getClassList: publish remaining registrations only
// when their required virtual bodies are callable and their creation/use paths are verified.
void ModuleGeom::getClassList(RD9_HOST_VECTOR_PARAMETER(ClassRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::getClassList", 0x10);

    ::renddx9abi::HostVectorWriter<ClassRegInfo> out(hostVector);

    {
        ClassRegInfo info;
        info.classId = CID_GeometryTemplateManager;
        info.name    = "dice.hfe.geom.GeometryTemplateManager";
        info.create  = RD9_CLASS_FACTORY(&createGeometryTemplateManager);
        out.push_back(info);
    }

    // Keep the recovered factories in the same relative order as PE 0x100C7F43's 32 entries.
    {
        ClassRegInfo info;
        info.classId = CID_StaticMeshRenderer;
        info.name    = "dice.hfe.geom.StaticMeshRenderer";
        info.create  = RD9_CLASS_FACTORY(&createStaticMeshRenderer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_SkinnedMeshRenderer;
        info.name    = "dice.hfe.geom.SkinnedMeshRenderer";
        info.create  = RD9_CLASS_FACTORY(&createSkinnedMeshRenderer);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = CID_ParticleSystemManager;
        info.name    = "dice.hfe.geom.ParticleSystemManager";
        info.create  = RD9_CLASS_FACTORY(&createParticleSystemManager);
        out.push_back(info);
    }

    // HemiMapManager - complete 14-slot table, including the editor/offline makeHemiMap operation.
    // Its connected stock path is the setBaseHemiMap console binding used by every stock Sky.con.
    {
        ClassRegInfo info;
        info.classId = CID_HemiMapManager;
        info.name    = "dice.hfe.geom.HemiMapManager";
        info.create  = RD9_CLASS_FACTORY(&createHemiMapManager);
        out.push_back(info);
    }

    // DecalManager - for the join path (see the forward declaration above). Its
    // getSingletonList entry (flags 2) now has a factory, so getSingleton("DecalManager") resolves.
    {
        ClassRegInfo info;
        info.classId = CID_DecalManager;
        info.name    = "dice.hfe.geom.DecalManager";
        info.create  = RD9_CLASS_FACTORY(&createDecalManager);
        out.push_back(info);
    }

    // VideoOptions::apply dereferences this singleton on every boot inside a
    // branch guarded only on g_Renderer - the shipped defect in RW-76 - so the class exists to make
    // that branch survivable. Only slot +0x18 is on that path and it is written.
    {
        ClassRegInfo info;
        info.classId = CID_RoadCompiledRenderer;
        info.name    = "dice.hfe.geom.RoadCompiledRenderer";
        info.create  = RD9_CLASS_FACTORY(&createRoadCompiledRenderer);
        out.push_back(info);
    }

    // Editable-road renderer registration.
    {
        ClassRegInfo info;
        info.classId = CID_RoadEditableRenderer;
        info.name    = "dice.hfe.geom.RoadEditableRenderer";
        info.create  = RD9_CLASS_FACTORY(&createRoadEditableRenderer);
        out.push_back(info);
    }

    // Roads.RoadTemplate - 821008 (dword_1021AEB4); shipped after the two GeometryOptimizers
    // (0x100C83BB), which the tree does not register. created only
    // through createInstance (the editor's road templates); see Geom/RoadTemplate.cpp.
    {
        ClassRegInfo info;
        info.classId = CID_RoadTemplate;
        info.name    = "dice.hfe.geom.Roads.RoadTemplate";
        info.create  = RD9_CLASS_FACTORY(&createRoadTemplate);
        out.push_back(info);
    }

    // Editable-road factory registration.
    {
        ClassRegInfo info;
        info.classId = CID_RoadEditable;
        info.name    = "dice.hfe.geom.Roads.RoadEditable";
        info.create  = RD9_CLASS_FACTORY(&createRoadEditable);
        out.push_back(info);
    }

    // NametagManager - 852419. Shipped getClassList order puts it immediately before LightManager
    // (0x100C8439, just after Roads.RoadEditable). The singleton list below already names it, so
    // ClassManager now creates it and fills g_NametagManager (IID 0xB801DCFE). RW-239.
    {
        ClassRegInfo info;
        info.classId = 852419u;
        info.name    = "dice.hfe.geom.NametagManager";
        info.create  = RD9_CLASS_FACTORY(&createNametagManager);
        out.push_back(info);
    }

    // XpLightManager.cpp: PE factory 0x100B0502, exact 0x980-byte allocation and 189-slot primary
    // interface. Its boot-live settings/empty-light surface is recovered; later light-instance and
    // GPU paths stop at their individual slots.
    {
        ClassRegInfo info;
        info.classId = CID_LightManager;
        info.name    = "dice.hfe.geom.LightManager";
        info.create  = RD9_CLASS_FACTORY(&createLightManager);
        out.push_back(info);
    }

    // Register the rasterizer instantiated by the (10, 1) core event.
    {
        ClassRegInfo info;
        info.classId = CID_SwiffRender;
        info.name    = "dice.hfe.geom.SwiffRender";
        info.create  = RD9_CLASS_FACTORY(&createSwiffRender);
        out.push_back(info);
    }

    // SkyDome - 0x870CAA3D. ; see the note beside its forward declaration above.
    {
        ClassRegInfo info;
        // The literal, not a named constant, to match how getSingletonList below spells the same
        // id: Core/Ids.h is not included in this translation unit and SkyDome.cpp defines no
        // exported id of its own.
        info.classId = 0x870CAA3Du;
        info.name    = "dice.hfe.geom.SkyDome";
        info.create  = RD9_CLASS_FACTORY(&createSkyDome);
        out.push_back(info);
    }

    // UndergrowthSystem - 822323005 (CID_UndergrowthSystem). the
    // shipped list puts it after SkyDome and LightGeometry (0x100C8574). flags-2 singleton, so
    // registerSingleton constructs it; its constructor allocates only the empty atlas owner.
    {
        ClassRegInfo info;
        info.classId = 822323005u;
        info.name    = "dice.hfe.geom.UndergrowthSystem";
        info.create  = RD9_CLASS_FACTORY(&createUndergrowthSystem);
        out.push_back(info);
    }

    // Register mesh factories in BundledMesh, SkinnedMesh, StaticMesh order.
    {
        ClassRegInfo info;
        info.classId = 0x870CA651u;
        info.name    = "dice.hfe.geom.GeometryTemplate.BundledMesh";
        info.create  = RD9_CLASS_FACTORY(&createBundledMeshTemplate);
        out.push_back(info);
    }
    // G1 draft: GeometryTemplate.MeshParticleMesh - 0x870CA655. Shipped between BundledMesh and
    // SkinnedMesh (0x100C8049..0x100C8071). Published together with the MeshParticlesRenderer's
    // retained-list draw (drawExtras, PE 0x101753E0): muzzle flashes and tracers draw through
    // IGeometry +0x18, which only queues into that renderer.
    {
        ClassRegInfo info;
        info.classId = 0x870CA655u;
        info.name    = "dice.hfe.geom.GeometryTemplate.MeshParticleMesh";
        info.create  = RD9_CLASS_FACTORY(&createMeshParticleMeshTemplate);
        out.push_back(info);
    }
    {
        ClassRegInfo info;
        info.classId = 0x870CA652u;
        info.name    = "dice.hfe.geom.GeometryTemplate.SkinnedMesh";
        info.create  = RD9_CLASS_FACTORY(&createSkinnedMeshTemplate);
        out.push_back(info);
    }

    // GeometryTemplate.StaticMesh - 0x870CA653. See the note beside its forward declaration above.
    // Kept in the shipped list's relative order: the three GeometryTemplate.* entries sit together,
    // after GeometryTemplateManager and before StaticMeshRenderer.
    {
        ClassRegInfo info;
        info.classId = 0x870CA653u;
        info.name    = "dice.hfe.geom.GeometryTemplate.StaticMesh";
        info.create  = RD9_CLASS_FACTORY(&createStaticMeshTemplate);
        out.push_back(info);
    }

    // Compiled-road template registration.
    {
        ClassRegInfo info;
        info.classId = CID_RoadCompiledTemplate;
        info.name    = "dice.hfe.geom.GeometryTemplate.RoadCompiled";
        info.create  = RD9_CLASS_FACTORY(&createRoadCompiledTemplate);
        out.push_back(info);
    }

    // ObjectDrawer - 0x870CAA3E.  ; see the note beside its forward declaration.
    // Renderer::initialize step 10 asks for it by createInstance(0x870CAA3E, 0xB801DCFC), and
    // queryInterface answers both that IID and the plain IBase 1.
    {
        ClassRegInfo info;
        info.classId = 0x870CAA3Eu;
        info.name    = "dice.hfe.geom.ObjectDrawer";
        info.create  = RD9_CLASS_FACTORY(&createObjectDrawer);
        out.push_back(info);
    }

    // Terrain is instantiated on demand by terrain.create.
    {
        ClassRegInfo info;
        info.classId = 821004u;
        info.name    = "dice.hfe.geom.Terrain";
        info.create  = RD9_CLASS_FACTORY(&createTerrain);
        out.push_back(info);
    }

    // TerrainEditable - 821005 (dword_1021AEB0), the shipped list's "dice.hfe.geom.TerrainEditable"
    // entry, factory 0x100B0586. TerrainEditable R1:
    // construction, init, update and draw are written; the editing and saving slots stop at named
    // [FRONTIER]s. Reached only through `terrain.create TerrainEditable` (registerTerrainSingleton) -
    // the editor's verb; no shipped level asks for it. Outside the editor flavour init faults exactly
    // as retail 1.50 does (see Geom/TerrainEditable.cpp).
    {
        ClassRegInfo info;
        info.classId = 821005u;
        info.name    = "dice.hfe.geom.TerrainEditable";
        info.create  = RD9_CLASS_FACTORY(&createTerrainEditable);
        out.push_back(info);
    }

    // Count follows the output container; keep the diagnostic's literal name list in sync.
    ::renddx9abi::logf("ModuleGeom::getClassList: %u classes registered (GeometryTemplateManager, "
                       "StaticMeshRenderer, SkinnedMeshRenderer, ParticleSystemManager, HemiMapManager, "
                       "DecalManager, RoadCompiledRenderer, LightManager, SwiffRender, SkyDome, UndergrowthSystem, "
                       "GeometryTemplate.BundledMesh, GeometryTemplate.MeshParticleMesh, GeometryTemplate.SkinnedMesh, GeometryTemplate.StaticMesh, ObjectDrawer, "
                       "GeometryTemplate.RoadCompiled, "
                       "RoadEditableRenderer, Roads.RoadTemplate, "
                       "Terrain, TerrainEditable, NametagManager); the rest of the shipped 32 are withheld - no factory bodies yet",
                       out.size());
}


// getSingletonList, 0x100C7974. sixteen entries, all flags == 2, in the binary's order. The iid /
// classId pairs are read from the sub_100518D0(name, instance, iid, classId, flags) call sites.

namespace {

struct GeomSingleton
{
    const char*  name;
    IBase**      instance;
    unsigned int iid;
    unsigned int classId;
};

} // anonymous namespace

void ModuleGeom::getSingletonList(RD9_HOST_VECTOR_PARAMETER(SingletonRegInfo, hostVector))
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::getSingletonList", 0x14);

    // Interface and class IDs retain their unsigned 32-bit bit patterns.
    static const GeomSingleton entries[16] =
    {
        { "LightManager",           reinterpret_cast<IBase**>(&g_LightManager),
                                                                  0xB801DCF9u,  852417u    },
        { "GeometryTemplateManager", &g_GeometryTemplateManager, 0xAD9665BCu, 0x870CA329u },
        { "StaticMeshRenderer",     &g_StaticMeshRenderer,          820013u,  821014u    },
        // iid 1 - IID_IBase - and that is what the binary passes, not a typo here.
        { "SkinnedMeshRenderer",    &g_SkinnedMeshRenderer,              1u,  821015u    },
        { "ParticleSystemManager",  &g_ParticleSystemManager,       820016u,  821017u    },
        { "HemiMapManager",         reinterpret_cast<IBase**>(&g_HemiMapManager),
                                                                       820019u,  821019u    },
        { "DecalManager",           &g_DecalManager,                820018u,  821018u    },
        { "DecalShadowManager",     &g_DecalShadowManager,          820021u,  821020u    },
        { "RoadCompiledRenderer",   &g_RoadCompiledRenderer,        820013u,  821912u    },
        { "RoadEditableRenderer",   &g_RoadEditableRenderer,        820020u,  821913u    },
        { "NametagManager",         &g_NametagManager,          0xB801DCFEu,  852419u    },
        { "SkyDome",                &g_SkyDome,                 0xB801DCFBu, 0x870CAA3Du },
        { "UndergrowthSystem",      &g_UndergrowthSystem,       0xB801DCFFu,  822323005u },
        { "LightningManager",       &g_LightningManager,        0xB801DD06u,  822323012u },
        { "WeatherManager",         &g_WeatherManager,          0xB801DD05u,  822323011u },
        { "RainManager",            &g_RainManager,             0xB801DD07u,  822323013u },
    };

    ::renddx9abi::HostVectorWriter<SingletonRegInfo> out(hostVector);

    for (int i = 0; i < 16; ++i)
    {
        SingletonRegInfo info;
        info.name     = entries[i].name;
        info.instance = entries[i].instance;
        info.iid      = entries[i].iid;
        info.classId  = entries[i].classId;
        info.flags    = 2;
        out.push_back(info);
    }
}

bool ModuleGeom::init()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::init", 0x18);

    // PE 0x100B0DF6 first passes this TU's ConsoleObject list (unk_102543C4) to
    // ConsoleObjects::registerConsoleObjects at 0x1007B570, then registers the category-10 event
    // handler below.  Native mode shares the rebuilt host's STL ABI; retail-compatible mode cannot
    // safely publish current-toolchain ConsoleObject/container layouts to the VC7.1 host.
#if BF2_PLUGIN_ABI_NATIVE
    registerLightManagerConsoleObjects();
    registerShadowManagerConsoleObjects();
    registerHemiMapManagerConsoleObjects();
    registerGeometryTemplateConsoleObjects();
    registerSkyDomeConsoleObjects();
    registerTerrainConsoleObjects();
    registerNametagManagerConsoleObjects();
    registerUndergrowthConsoleObjects();
    registerDecalManagerConsoleObjects();
    registerOverGrowthConsoleObjects();
    registerObjectGeometryConsoleObjects();
    registerStaticMeshRendererConsoleObjects();
#endif
#if BF2_PLUGIN_ABI_NATIVE && defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    registerLightmapGenerationConsoleObjects();
#endif

    // EventManager::registerEventHandler registers this module's instruction-proven one-slot
    // ECCoreEventHandler subobject (this+4) for
    // category 10, priority 0. That registration routes BF2's initMainMenuSwiff (10,1) post into
    // handleCoreModuleEvent above.
    if (eventManager)
    {
        eventManager->registerEventHandler(
            static_cast<EventCategory>(10),
            reinterpret_cast<IEventListener*>(static_cast<ECCoreEventHandler*>(this)), 0);
        ::renddx9abi::logf("ModuleGeom::init: category-10 core event handler registered "
                           "(the SwiffRender publisher)");
    }

    const char* const names[16] =
    {
        "LightManager", "GeometryTemplateManager", "StaticMeshRenderer", "SkinnedMeshRenderer",
        "ParticleSystemManager", "HemiMapManager", "DecalManager", "DecalShadowManager",
        "RoadCompiledRenderer", "RoadEditableRenderer", "NametagManager", "SkyDome",
        "UndergrowthSystem", "LightningManager", "WeatherManager", "RainManager"
    };
    IBase* const values[16] =
    {
        static_cast<IBase*>(g_LightManager), g_GeometryTemplateManager,
        g_StaticMeshRenderer, g_SkinnedMeshRenderer,
        g_ParticleSystemManager, static_cast<IBase*>(g_HemiMapManager),
        g_DecalManager, g_DecalShadowManager,
        g_RoadCompiledRenderer, g_RoadEditableRenderer, g_NametagManager, g_SkyDome,
        g_UndergrowthSystem, g_LightningManager, g_WeatherManager, g_RainManager
    };
    for (int i = 0; i < 16; ++i)
        ::renddx9abi::logf("ModuleGeom::init: %-24s %s", names[i],
                           values[i] ? "created" : "NULL (no class registered)");

    return true;
}

bool ModuleGeom::close()
{
    RD9_TRACE("dice::hfe::geom::ModuleGeom::close", 0x1C);

    // PE 0x100B0E28 unregisters the category-10 handler, removes this TU's console list through
    // ConsoleObjects::unregisterConsoleObjects at 0x1007B6D0, then unregisters Terrain if present.
    if (eventManager)
    {
        eventManager->unregisterEventHandler(
            static_cast<EventCategory>(10),
            reinterpret_cast<IEventListener*>(static_cast<ECCoreEventHandler*>(this)));
    }
#if BF2_PLUGIN_ABI_NATIVE && defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    unregisterLightmapGenerationConsoleObjects();
#endif
#if BF2_PLUGIN_ABI_NATIVE
    unregisterStaticMeshRendererConsoleObjects();
    unregisterObjectGeometryConsoleObjects();
    unregisterOverGrowthConsoleObjects();
    unregisterDecalManagerConsoleObjects();
    unregisterUndergrowthConsoleObjects();
    unregisterNametagManagerConsoleObjects();
    unregisterTerrainConsoleObjects();
    unregisterSkyDomeConsoleObjects();
    unregisterGeometryTemplateConsoleObjects();
    unregisterHemiMapManagerConsoleObjects();
    unregisterShadowManagerConsoleObjects();
    unregisterLightManagerConsoleObjects();
#endif
    if (classManager && classManager->getSingleton(renddx9abi::HostString("Terrain")))
        classManager->unregisterSingleton(renddx9abi::HostString("Terrain"));
    return true;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
