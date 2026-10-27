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
    // [TEMP] A file-loader fixture, not a reconstructed template. Only the
    // scalar loaded flag and two nested vectors used by selection are modeled.
    alignas(dice::hfe::geom::SkinnedMeshTemplate) unsigned char storage[
        sizeof(dice::hfe::geom::SkinnedMeshTemplate)];
    vc71::vector_layout geoms[4]; // two pairs: lod vector + distance vector
    vc71::vector_layout companions[2];
    alignas(4) unsigned char lods[2][2][132];
    vc71::vector_layout materials[2][2];

    TemplateFixture() { std::memset(this, 0, sizeof(*this)); }
    dice::hfe::geom::SkinnedMeshTemplate* get()
    { return reinterpret_cast<dice::hfe::geom::SkinnedMeshTemplate*>(storage); }

    void load()
    {
        for (unsigned int geom = 0; geom != 2; ++geom)
        {
            geoms[geom * 2].first = reinterpret_cast<char*>(&lods[geom][0][0]);
            geoms[geom * 2].last = geoms[geom * 2].end = geoms[geom * 2].first + 264;
            companions[geom].first = reinterpret_cast<char*>(&materials[geom][0]);
            companions[geom].last = companions[geom].end = companions[geom].first + 32;
            for (unsigned int lod = 0; lod != 2; ++lod)
            {
                // A finalized file bounds record. No geometry bounds calculation
                // is replaced: the production getter reads the fixture radius.
                dice::hfe::geom::MeshBounds* b =
                    reinterpret_cast<dice::hfe::geom::MeshBounds*>(lods[geom][lod]);
                b->radius = 6.25f + geom * 10.0f + lod;
            }
        }
        get()->m_vector0DC.first = reinterpret_cast<char*>(geoms);
        get()->m_vector0DC.last = get()->m_vector0DC.end = get()->m_vector0DC.first + 64;
        get()->m_vector0CC.first = reinterpret_cast<char*>(companions);
        get()->m_vector0CC.last = get()->m_vector0CC.end = get()->m_vector0CC.first + 32;
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
void* crtMalloc(size_t size) { return std::malloc(size); }
void crtFree(void* value) { std::free(value); }
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
int rendererGetShaderPath(const void*) { unexpected("rendererGetShaderPath"); }
unsigned int rendererFrameOrdinal() { unexpected("rendererFrameOrdinal"); }
bool deviceCapsIsNvidia() { unexpected("deviceCapsIsNvidia"); }
void* shaderManagerGetEffect(void*, const char*) { unexpected("shaderManagerGetEffect"); }
ShaderParameter* effectGetShaderParameterTyped(Effect*, const vc71::string&)
{ unexpected("effectGetShaderParameterTyped"); }
bool effectSetTechniqueByName(void*, const vc71::string&) { unexpected("effectSetTechniqueByName"); }
IRenderView* rendererGetRenderViewDup() { unexpected("rendererGetRenderViewDup"); }
RenderQueueEntry* renderEntryPoolAcquire() { unexpected("renderEntryPoolAcquire"); }
void renderQueueEnqueue(RenderQueueEntry*, bool, bool, bool) { unexpected("renderQueueEnqueue"); }
void vertexBufferBindStream(void*, unsigned int) { unexpected("vertexBufferBindStream"); }
void indexBufferBind(void*) { unexpected("indexBufferBind"); }
void vertexDeclarationBind(void*) { unexpected("vertexDeclarationBind"); }
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int)
{ unexpected("rendererDrawIndexedPrimitive"); }
namespace Ra {
RaShader* getShader(RaShaderTable*, const vc71::string&, bool) { unexpected("getShader"); }
// Record this unrelated embedded block's lifecycle only. No Ra field is read by
// the exercised methods, so constructing GPU-backed parameter holders is omitted.
void raObjectParametersConstruct(void*) { ++parameterConstructions; parametersReady = true; }
void raObjectParametersDestroy(void*) { ++parameterDestructions; }
}
}
namespace geom {
LightManager* g_LightManager = 0;
IBase* g_SkinnedMeshRenderer = 0;
unsigned char g_skinnedMeshTechniquesPending = 0;
void meshBoundsFinalize(MeshBounds&) { unexpected("meshBoundsFinalize (fixture supplies clean bounds)"); }
void instanceSetErase(vc71::set_layout& set, void*)
{
    // This fixture never registers an instance. Model only erase from its empty set.
    if (set.size) unexpected("instanceSetErase (nonempty fixture set)");
    ++instanceErases;
}
void* staticMeshRootParent(void*) { unexpected("staticMeshRootParent"); }
bool meshBuildTechniqueMasks(const vc71::string&, const vc71::string&, const char* const*,
    std::map<unsigned int, dx9rend::Ra::RaShader*>&, vc71::vector_layout&)
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
        check(mesh.m_lodRecord == t.lods[0][0], "load refresh selects geom 0 lod 0 bounds");
        check(mesh.m_lodMaterials == &t.materials[0][0], "load refresh selects geom 0 lod 0 materials");
        check(mesh.getBoundingRadius() == 6.25f, "load refresh exposes positive flag-like bounds");
        mesh.m_geomIndex = 1;
        mesh.m_lodIndex = 1;
        mesh.refreshSelection();
        check(mesh.m_lodRecord == t.lods[1][1], "refresh preserves current geom and lod for bounds");
        check(mesh.m_lodMaterials == &t.materials[1][1], "refresh preserves current geom and lod for materials");
        check(mesh.getBoundingRadius() == 17.25f, "refresh reads the current lod radius");
        for (unsigned int i = 0; i != 2; ++i)
        {
            InstanceFixture later(t);
            check(later.mesh->m_lodRecord == t.lods[0][0], "already-loaded constructor immediately selects bounds");
            check(later.mesh->m_lodMaterials == &t.materials[0][0], "already-loaded constructor immediately selects materials");
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
