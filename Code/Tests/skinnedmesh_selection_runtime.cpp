// [TEMP] [CHOSEN, not recovered] Focused x86 fixture for the actual SkinnedMesh
// constructor, selection refresh and bounding-radius query. No game or DLL is loaded.
// Regression evidence: RendDX9 0x101A78B1..0x101A78F7 selects current geom/lod
// when the template is already loaded, before allocating the bone palettes.
// File loading, rendering and engine ownership are outside this fixture.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#ifndef BF2_SKINNEDMESH_SOURCE
#define BF2_SKINNEDMESH_SOURCE "../BF2/Geom/SkinnedMesh.cpp"
#endif
#include BF2_SKINNEDMESH_SOURCE

namespace {
int checks = 0;
int failures = 0;
unsigned int allocations = 0;
unsigned int frees = 0;
unsigned int parameterConstructions = 0;
unsigned int parameterDestructions = 0;
unsigned int instanceErases = 0;
dice::hfe::geom::SkinnedMesh* constructing = 0;
bool allocationOrderCorrect = true;
bool parametersReady = false;
void check(bool value, const char* label)
{
    ++checks;
    if (!value) { ++failures; std::fprintf(stderr, "FAIL: %s\n", label); }
}

__declspec(noreturn) void unexpected(const char* name)
{
    std::fprintf(stderr, "FAIL: unexpected fixture dependency: %s\n", name);
    std::fflush(stderr);
    std::abort();
}

struct TemplateFixture
{
    // [TEMP] A file-loader fixture, not a reconstructed template. Only the scalar loaded flag,
    // the two nested tables selection reads and the instance set the destructor visits are
    // constructed; the rest of the storage stays zero.
    typedef dice::hfe::geom::SkinnedMeshTemplate Template;
    typedef std::set<dice::hfe::geom::SkinnedMesh*> InstanceSet;
    alignas(Template) unsigned char storage[sizeof(Template)];

    TemplateFixture()
    {
        std::memset(storage, 0, sizeof(storage));
        new (&get()->m_materials) Template::MaterialTable();
        new (&get()->m_geoms) Template::GeomTable();
        new (&get()->m_instances) InstanceSet();
    }
    ~TemplateFixture()
    {
        destroy(get()->m_instances);
        destroy(get()->m_geoms);
        destroy(get()->m_materials);
    }
    template<class T> static void destroy(T& member) { member.~T(); }
    Template* get() { return reinterpret_cast<Template*>(storage); }
    dice::hfe::geom::SkinnedMeshLod* lod(unsigned int geom, unsigned int lod)
    { return &get()->m_geoms[geom].lods[lod]; }
    Template::LodMaterials* materials(unsigned int geom, unsigned int lod)
    { return &get()->m_materials[geom][lod]; }

    void load()
    {
        // Two geoms of two lods each, plus each lod's (empty) material list.
        get()->m_geoms.resize(2);
        get()->m_materials.resize(2);
        for (unsigned int geom = 0; geom != 2; ++geom)
        {
            get()->m_geoms[geom].lods.resize(2);
            get()->m_materials[geom].resize(2);
            for (unsigned int lod = 0; lod != 2; ++lod)
            {
                // A finalized file bounds record. No geometry bounds calculation
                // is replaced: the production getter reads the fixture radius.
                this->lod(geom, lod)->bounds.radius = 6.25f + geom * 10.0f + lod;
            }
        }
        get()->m_loaded = 1;
    }
};

struct InstanceFixture
{
    alignas(dice::hfe::geom::SkinnedMesh) unsigned char storage[sizeof(dice::hfe::geom::SkinnedMesh)];
    dice::hfe::geom::SkinnedMesh* mesh;
    explicit InstanceFixture(TemplateFixture& t)
    {
        // Deliberate nonzero fill also makes checking ctor writes meaningful.
        std::memset(storage, 0xA5, sizeof(storage));
        constructing = reinterpret_cast<dice::hfe::geom::SkinnedMesh*>(storage);
        parametersReady = false;
        allocationOrderCorrect = true;
        unsigned int before = allocations;
        mesh = dice::hfe::geom::skinnedMeshConstruct(storage, t.get());
        constructing = 0;
        check(allocations - before == 104, "constructor allocates the shipped 104 bone matrices");
        check(allocationOrderCorrect, "selection and Ra initialization precede bone allocations");
    }
    ~InstanceFixture() { mesh->~SkinnedMesh(); }
};
}

// [TEMP] Narrow external providers. The instance's real constructor, destructor,
// full vtable, refreshSelection and radius getter remain linked unchanged. Heap
// calls use this process's CRT in BOTH ABI modes, never an original engine heap.
namespace renddx9abi {
void* engineAlloc(unsigned int size)
{
    ++allocations;
    if (constructing)
    {
        if (!parametersReady) allocationOrderCorrect = false;
        if (constructing->m_template->m_loaded &&
            (!constructing->m_lodRecord || !constructing->m_lodMaterials))
            allocationOrderCorrect = false;
    }
    return std::malloc(size);
}
void engineFree(void* value) { ++frees; std::free(value); }
void logf(const char* format, ...)
{
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::fputc('\n', stderr);
}
void frontier(const char* name, const void*, unsigned int) { unexpected(name); }
}

namespace dice { namespace hfe {
IBase* playerManager = 0;
void fullInvert(const Mat4&, Mat4&) { unexpected("fullInvert"); }
namespace dx9rend {
void* g_rendererBase = 0;
void* g_renderQueues = 0;
void* g_shaderManager = 0;
IBase* g_Renderer = 0;
unsigned int rendererFrameOrdinal() { unexpected("rendererFrameOrdinal"); }
bool deviceCapsIsNvidia() { unexpected("deviceCapsIsNvidia"); }
void* shaderManagerGetEffect(void*, const char*) { unexpected("shaderManagerGetEffect"); }
ShaderParameter* effectGetShaderParameterTyped(Effect*, const std::string&)
{ unexpected("effectGetShaderParameterTyped"); }
bool effectSetTechniqueByName(void*, const std::string&) { unexpected("effectSetTechniqueByName"); }
IRenderView* rendererGetRenderViewDup() { unexpected("rendererGetRenderViewDup"); }
RenderQueueEntry* renderEntryPoolAcquire() { unexpected("renderEntryPoolAcquire"); }
void renderQueueEnqueue(RenderQueueEntry*, bool, bool, bool) { unexpected("renderQueueEnqueue"); }
void vertexBufferBindStream(void*, unsigned int) { unexpected("vertexBufferBindStream"); }
void indexBufferBind(void*) { unexpected("indexBufferBind"); }
void vertexDeclarationBind(void*) { unexpected("vertexDeclarationBind"); }
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int)
{ unexpected("rendererDrawIndexedPrimitive"); }
namespace Ra {
RaShader* getShader(RaShaderTable*, const std::string&, bool) { unexpected("getShader"); }
// Record this unrelated embedded block's lifecycle only. No Ra field is read by the exercised
// methods, so its holders are not initialized; the userData vector member is still constructed
// and destroyed by these bodies.
RaObjectParameters::RaObjectParameters() { ++parameterConstructions; parametersReady = true; }
RaObjectParameters::~RaObjectParameters() { ++parameterDestructions; }
}
}
namespace geom {
LightManager* g_LightManager = 0;
IBase* g_SkinnedMeshRenderer = 0;
unsigned char g_skinnedMeshTechniquesPending = 0;
void meshBoundsFinalize(MeshBounds&) { unexpected("meshBoundsFinalize (fixture supplies clean bounds)"); }
void instanceSetErase(std::set<SkinnedMesh*>& set, SkinnedMesh*)
{
    // This fixture never registers an instance. Model only erase from its empty set.
    if (!set.empty()) unexpected("instanceSetErase (nonempty fixture set)");
    ++instanceErases;
}
void* staticMeshRootParent(void*) { unexpected("staticMeshRootParent"); }
// Draw-path and material-teardown dependencies outside the selection boundary. The fixture's
// material lists stay empty, so no SkinnedMeshMaterial is ever destroyed.
int skinnedMeshRendererMode() { unexpected("skinnedMeshRendererMode"); }
void skinnedMeshRendererSubmitA(SkinnedMesh*) { unexpected("skinnedMeshRendererSubmitA"); }
void skinnedMeshRendererSubmitB(SkinnedMesh*) { unexpected("skinnedMeshRendererSubmitB"); }
double lightManagerShadowFadeDuration(const LightManager*) { unexpected("lightManagerShadowFadeDuration"); }
SkinnedMeshMaterial::~SkinnedMeshMaterial() { unexpected("SkinnedMeshMaterial::~SkinnedMeshMaterial"); }
bool meshBuildTechniqueMasks(const std::string&, const std::string&, const char* const*,
    std::map<unsigned int, dx9rend::Ra::RaShader*>&, std::vector<unsigned int>&)
{ unexpected("meshBuildTechniqueMasks"); }
}
} }

int main()
{
    using namespace dice::hfe::geom;
    static_assert(sizeof(void*) == 4, "SkinnedMesh fixtures require x86");
    {
        TemplateFixture t;
        InstanceFixture first(t);
        SkinnedMesh& mesh = *first.mesh;
        check(mesh.m_lodRecord == 0, "unloaded constructor leaves bounds unselected");
        check(mesh.m_lodMaterials == 0, "unloaded constructor leaves materials unselected");
        check(mesh.getBoundingRadius() == 0.0f, "unloaded unselected radius is shipped zero");
        t.load();
        mesh.refreshSelection();
        check(mesh.m_lodRecord == t.lod(0, 0), "load refresh selects geom 0 lod 0 bounds");
        check(mesh.m_lodMaterials == t.materials(0, 0), "load refresh selects geom 0 lod 0 materials");
        check(mesh.getBoundingRadius() == 6.25f, "load refresh exposes positive flag-like bounds");
        mesh.m_geomIndex = 1;
        mesh.m_lodIndex = 1;
        mesh.refreshSelection();
        check(mesh.m_lodRecord == t.lod(1, 1), "refresh preserves current geom and lod for bounds");
        check(mesh.m_lodMaterials == t.materials(1, 1), "refresh preserves current geom and lod for materials");
        check(mesh.getBoundingRadius() == 17.25f, "refresh reads the current lod radius");
        for (unsigned int i = 0; i != 2; ++i)
        {
            InstanceFixture later(t);
            check(later.mesh->m_lodRecord == t.lod(0, 0), "already-loaded constructor immediately selects bounds");
            check(later.mesh->m_lodMaterials == t.materials(0, 0), "already-loaded constructor immediately selects materials");
            check(later.mesh->m_geomIndex == 0 && later.mesh->m_lodIndex == 0,
                  "each later instance starts with its own geom 0 lod 0 selection");
            check(later.mesh->getBoundingRadius() == 6.25f,
                  "later instance returns positive radius before a draw or refresh sweep");
        }
    }
    check(allocations == frees, "real destructor frees every allocated bone matrix");
    check(parameterConstructions == 3 && parameterDestructions == 3, "Ra lifetime fixture records all three instances");
    check(instanceErases == 3, "real destructor visits the template instance set");
    std::printf("SkinnedMesh selection: %d/%d checks passed (ABI native=%d; actual SkinnedMesh.cpp)\n",
        checks-failures, checks, BF2_PLUGIN_ABI_NATIVE);
    return failures ? 1 : 0;
}
#include "renderer_accessor_fixture_support.h"   // RendDX9Base.h Renderer accessors (x86 identity)
