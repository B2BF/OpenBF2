// Debug torus geometry. Source placement is attested by the PE diagnostic path.
#ifdef BF2_IMAGE_RENDDX9
#include "DebugMeshCommon.h"
#include "Debug.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <d3dx9.h>
#if defined(__APPLE__) || defined(BF2_LINUX_CLIENT)
#include "../Platform/Gfx/Metal/D3DXMesh.h"
#endif
#include <list>
#include <new>

namespace dice { namespace hfe { namespace dx9rend {
extern void* g_rendererBase;
extern void* g_shaderManager;
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int);
} } }
namespace dice { namespace hfe { namespace geom {
extern const unsigned int CID_DebugTorusMesh = 81020u;
namespace {
using namespace debugmesh;
struct DebugTorusMeshTemplate;
extern void* const kTorusTable[68];
extern void* const kTorusTemplateTable[58];

// [INFERRED] Member names; native strings, lists and pointers follow issue #7.
struct DebugTorusMesh : GeometryInstanceData
{
    explicit DebugTorusMesh(DebugTorusMeshTemplate*);
    ~DebugTorusMesh();
    void initializeParameters();
    void draw();
    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Vec3 colour;                     // +0x40
    DebugTorusMeshTemplate* owner; // +0x4C
    float alpha;                     // +0x50
    dx9rend::ShaderParameter* blendAlpha; // +0x54
    dx9rend::ShaderParameter* ambient; // +0x58
    Mat4 transform;                  // +0x5C
};
struct DebugTorusMeshTemplate
{
    DebugTorusMeshTemplate();
    ~DebugTorusMeshTemplate();
    void createResources();
    void load();
    DebugTorusMesh* createInstance();
    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugTorusMesh*> instances; // +0x10
    float innerRadius;              // +0x1C
    float outerRadius;              // +0x20
    unsigned int sides;             // +0x24
    unsigned int rings;             // +0x28
    unsigned int field2C;           // +0x2C
    std::string name;               // +0x30
    std::string path;               // +0x4C
    Vec3 instanceSize;              // +0x68
    Vec3 colour;                    // +0x74
    std::string shaderName;         // +0x80
    ID3DXMesh* mesh;                // +0x9C
    dx9rend::Effect* effect;         // +0xA0
    dx9rend::ShaderParameter* viewProjection; // +0xA4
    dx9rend::ShaderParameter* world; // +0xA8
    IDirect3DVertexBuffer9* vertices; // +0xAC
    IDirect3DIndexBuffer9* indices;  // +0xB0
    unsigned int faceCount;         // +0xB4
    unsigned int vertexCount;       // +0xB8
};

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x100925F0.
int RD9_THUNK torus_addRef(DebugTorusMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK torus_getRef(DebugTorusMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK torus_destroy(DebugTorusMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugTorusMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK torus_release(DebugTorusMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { typedef void* (__thiscall* Destroy)(void*, unsigned int); reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1); }
    return refs;
}
// PE 0x100925B0.
void* RD9_THUNK torus_query(DebugTorusMesh* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80011u ? s : 0; }
// PE 0x100920D0 / 0x10091C90 / 0x10091CA0 / 0x10091C30 / 0x10091C40.
void RD9_THUNK torus_draw(DebugTorusMesh* s RD9_THUNK_EDX, void*, void*, unsigned char) { s->draw(); }
int RD9_THUNK torus_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK torus_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK torus_owner(DebugTorusMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK torus_transform(DebugTorusMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x10091EB0. All instances overwrite one bounds object.
const LocalBoundingBox* RD9_THUNK torus_bounds(DebugTorusMesh* s RD9_THUNK_EDX)
{
    static LocalBoundingBox box;
    const float z = s->dimensions.z * 0.5f;
    box.m_derivedDataDirty = true;
    box.m_max.z = z;
    box.m_min.x = -(s->dimensions.x * 0.5f);
    box.m_min.y = -(s->dimensions.y * 0.5f);
    box.m_min.z = -z;
    box.m_max.x = s->dimensions.x * 0.5f;
    box.m_max.y = s->dimensions.y * 0.5f;
    return &box;
}
// PE 0x10091FC0. Ordered comparisons preserve the NaN selection.
float RD9_THUNK torus_radius(DebugTorusMesh* s RD9_THUNK_EDX)
{
    const float xz = s->dimensions.x < s->dimensions.z ? s->dimensions.z : s->dimensions.x;
    const float xy = s->dimensions.x < s->dimensions.y ? s->dimensions.y : s->dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}
// PE 0x10092020.
void RD9_THUNK torus_setTransform(DebugTorusMesh* s RD9_THUNK_EDX, const Mat4& value)
{
    s->transform = value;
    const Vec3 unit{1,1,1};
    if (!equalVector(s->dimensions, unit))
    {
        s->transform.m[0] *= s->dimensions.x;
        s->transform.m[5] *= s->dimensions.y;
        s->transform.m[10] *= s->dimensions.z;
    }
}
// PE 0x10092090 / 0x10091C50 / 0x10091C60 / 0x10091C70 / 0x100920B0 / 0x10091C80.
void RD9_THUNK torus_setColour(DebugTorusMesh* s RD9_THUNK_EDX, const Vec3& value) { s->colour = value; }
const Vec3& RD9_THUNK torus_colour(DebugTorusMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK torus_setAlpha(DebugTorusMesh* s RD9_THUNK_EDX, float value) { s->alpha = value; }
float RD9_THUNK torus_alpha(DebugTorusMesh* s RD9_THUNK_EDX) { return s->alpha; }
void RD9_THUNK torus_setDimensions(DebugTorusMesh* s RD9_THUNK_EDX, const Vec3& value) { s->dimensions = value; }
const Vec3& RD9_THUNK torus_dimensions(DebugTorusMesh* s RD9_THUNK_EDX) { return s->dimensions; }

#define TORUS_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020C1E0.
void* const kTorusTable[68] = {
    TORUS_SLOT(torus_addRef), TORUS_SLOT(torus_getRef), TORUS_SLOT(torus_release), TORUS_SLOT(torus_query),
    TORUS_SLOT(empty1), TORUS_SLOT(minusOneFloat), TORUS_SLOT(torus_draw), TORUS_SLOT(empty2),
    TORUS_SLOT(empty6), TORUS_SLOT(empty1), TORUS_SLOT(zero0), TORUS_SLOT(torus_type),
    TORUS_SLOT(torus_hasGeom), TORUS_SLOT(torus_destroy), TORUS_SLOT(torus_owner), TORUS_SLOT(zero1),
    TORUS_SLOT(torus_bounds), TORUS_SLOT(torus_radius), TORUS_SLOT(torus_transform), TORUS_SLOT(empty2),
    TORUS_SLOT(torus_setTransform), TORUS_SLOT(empty2), TORUS_SLOT(empty1), TORUS_SLOT(zero0),
    TORUS_SLOT(empty2), TORUS_SLOT(empty1), TORUS_SLOT(setName), TORUS_SLOT(empty1),
    TORUS_SLOT(zero0), TORUS_SLOT(one), TORUS_SLOT(empty1), TORUS_SLOT(zero0),
    TORUS_SLOT(one), TORUS_SLOT(empty1), TORUS_SLOT(minusOne), TORUS_SLOT(zero0),
    TORUS_SLOT(empty3), TORUS_SLOT(empty2), TORUS_SLOT(empty2), TORUS_SLOT(empty1),
    TORUS_SLOT(zero0), TORUS_SLOT(zero0), TORUS_SLOT(false0), TORUS_SLOT(empty1),
    TORUS_SLOT(zero0), TORUS_SLOT(empty1), TORUS_SLOT(empty1), TORUS_SLOT(false0),
    TORUS_SLOT(empty2), TORUS_SLOT(zero0), TORUS_SLOT(zero1), TORUS_SLOT(zero0),
    TORUS_SLOT(false0), TORUS_SLOT(empty1), TORUS_SLOT(false0), TORUS_SLOT(empty1),
    TORUS_SLOT(empty1), TORUS_SLOT(seven), TORUS_SLOT(zero0), TORUS_SLOT(zero1),
    TORUS_SLOT(empty0), TORUS_SLOT(empty1), TORUS_SLOT(torus_setColour), TORUS_SLOT(torus_colour),
    TORUS_SLOT(torus_setAlpha), TORUS_SLOT(torus_alpha), TORUS_SLOT(torus_setDimensions), TORUS_SLOT(torus_dimensions)
};
// PE 0x10132630 / 0x10051710 / 0x10092780 / 0x10092750.
int RD9_THUNK torusTemplate_addRef(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK torusTemplate_getRef(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK torusTemplate_release(DebugTorusMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugTorusMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK torusTemplate_query(DebugTorusMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80012u ? s : 0; }
// PE 0x100919C0 / 0x100919D0 / 0x100919E0 / 0x100919F0 / 0x10091A00.
unsigned int RD9_THUNK torusTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugTorusMesh; }
void RD9_THUNK torusTemplate_setName(DebugTorusMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->name = value; }
const std::string& RD9_THUNK torusTemplate_name(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK torusTemplate_setPath(DebugTorusMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->path = value; }
const std::string& RD9_THUNK torusTemplate_path(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x10092510 / 0x10092290.
void* RD9_THUNK torusTemplate_create(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK torusTemplate_load(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x10091A10 / 0x10091A20 / 0x10091CB0 / 0x10091A30 / 0x10091CD0 / 0x10091A40.
void RD9_THUNK torusTemplate_setShader(DebugTorusMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->shaderName = value; }
const std::string& RD9_THUNK torusTemplate_shader(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK torusTemplate_setColour(DebugTorusMeshTemplate* s RD9_THUNK_EDX, const Vec3& value) { s->colour = value; }
const Vec3& RD9_THUNK torusTemplate_colour(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK torusTemplate_setSize(DebugTorusMeshTemplate* s RD9_THUNK_EDX, const Vec3& value) { s->instanceSize = value; }
const Vec3& RD9_THUNK torusTemplate_size(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }
// PE 0x10091AE0 / 0x10091AF0.
void RD9_THUNK torusTemplate_setInnerRadius(DebugTorusMeshTemplate* s RD9_THUNK_EDX, float value) { s->innerRadius = value; }
float RD9_THUNK torusTemplate_innerRadius(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->innerRadius; }
// PE 0x10091B00 / 0x10091B10.
void RD9_THUNK torusTemplate_setOuterRadius(DebugTorusMeshTemplate* s RD9_THUNK_EDX, float value) { s->outerRadius = value; }
float RD9_THUNK torusTemplate_outerRadius(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->outerRadius; }
// PE 0x10091B20 / 0x10091B30.
void RD9_THUNK torusTemplate_setSides(DebugTorusMeshTemplate* s RD9_THUNK_EDX, unsigned int value) { s->sides = value; }
unsigned int RD9_THUNK torusTemplate_sides(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->sides; }
// PE 0x10091B40 / 0x10091B50.
void RD9_THUNK torusTemplate_setRings(DebugTorusMeshTemplate* s RD9_THUNK_EDX, unsigned int value) { s->rings = value; }
unsigned int RD9_THUNK torusTemplate_rings(DebugTorusMeshTemplate* s RD9_THUNK_EDX) { return s->rings; }
// PE 0x1020C340.
void* const kTorusTemplateTable[58] = {
    TORUS_SLOT(torusTemplate_addRef), TORUS_SLOT(torusTemplate_getRef), TORUS_SLOT(torusTemplate_release), TORUS_SLOT(torusTemplate_query),
    TORUS_SLOT(torusTemplate_class), TORUS_SLOT(torusTemplate_setName), TORUS_SLOT(torusTemplate_name), TORUS_SLOT(torusTemplate_setPath),
    TORUS_SLOT(torusTemplate_path), TORUS_SLOT(torusTemplate_create), TORUS_SLOT(false0), TORUS_SLOT(torusTemplate_load),
    TORUS_SLOT(empty3), TORUS_SLOT(lodDistance), TORUS_SLOT(empty4), TORUS_SLOT(materialValue),
    TORUS_SLOT(empty4), TORUS_SLOT(quarter), TORUS_SLOT(empty1), TORUS_SLOT(false0),
    TORUS_SLOT(false0), TORUS_SLOT(zero1), TORUS_SLOT(zero3), TORUS_SLOT(zero3),
    TORUS_SLOT(empty1), TORUS_SLOT(false0), TORUS_SLOT(empty1), TORUS_SLOT(false0),
    TORUS_SLOT(empty3), TORUS_SLOT(zero2), TORUS_SLOT(empty1), TORUS_SLOT(true0),
    TORUS_SLOT(empty1), TORUS_SLOT(false0), TORUS_SLOT(empty1), TORUS_SLOT(sixteen),
    TORUS_SLOT(empty1), TORUS_SLOT(false0), TORUS_SLOT(empty1), TORUS_SLOT(false0),
    TORUS_SLOT(zero1), TORUS_SLOT(zero1), TORUS_SLOT(decompress), TORUS_SLOT(empty0),
    TORUS_SLOT(torusTemplate_setShader), TORUS_SLOT(torusTemplate_shader), TORUS_SLOT(torusTemplate_setColour), TORUS_SLOT(torusTemplate_colour),
    TORUS_SLOT(torusTemplate_setInnerRadius), TORUS_SLOT(torusTemplate_innerRadius), TORUS_SLOT(torusTemplate_setOuterRadius), TORUS_SLOT(torusTemplate_outerRadius),
    TORUS_SLOT(torusTemplate_setSides), TORUS_SLOT(torusTemplate_sides), TORUS_SLOT(torusTemplate_setRings), TORUS_SLOT(torusTemplate_rings),
    TORUS_SLOT(torusTemplate_setSize), TORUS_SLOT(torusTemplate_size)
};
#undef TORUS_SLOT

// PE 0x10092450.
DebugTorusMeshTemplate::DebugTorusMeshTemplate()
    : table(kTorusTemplateTable), loaded(0), field08(0), refs(1), innerRadius(0.5f), outerRadius(1.5f), sides(40), rings(40), field2C(0),
      instanceSize{1,1,1}, colour{1,1,1}, shaderName("DebugD3DXMeshShapeShader"), mesh(0),
      effect(0), viewProjection(0), world(0), vertices(0), indices(0), faceCount(0), vertexCount(0) {}
// PE 0x10092370.
DebugTorusMeshTemplate::~DebugTorusMeshTemplate()
{
    if (vertices) vertices->Release();
    if (indices) indices->Release();
    if (mesh) mesh->Release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (effect) dx9rend::effectRelease(effect);
}
// PE 0x10091A50. No HRESULT is tested; the declaration is fetched and discarded.
void DebugTorusMeshTemplate::createResources()
{
    D3DXCreateTorus(static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device,
                      innerRadius, outerRadius, sides, rings, &mesh, 0);
    D3DVERTEXELEMENT9 elements[65];
    mesh->GetDeclaration(elements);
    mesh->GetVertexBuffer(&vertices);
    mesh->GetIndexBuffer(&indices);
    vertexCount = mesh->GetNumVertices();
    faceCount = mesh->GetNumFaces();
}
// PE 0x10091CF0. Shipped defect: repeated loads overwrite owned resource references.
void DebugTorusMeshTemplate::load()
{
    createResources();
    {
        const std::string prefix("Shaders/");
        const std::string shader = prefix + shaderName;
        effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, shader.c_str()));
    }
    viewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "WorldViewProjection");
    if (viewProjection) viewProjection->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugTorusMesh.cpp", 181, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugTorusMesh.cpp", 189, "")
        << "Could not load WorldViewProjection parameter from shader";
}
// PE 0x100922A0. The template's colour is not copied into the instance.
DebugTorusMesh::DebugTorusMesh(DebugTorusMeshTemplate* value)
    : GeometryInstanceData(kTorusTable), refs(1), dimensions{0,0,0}, colour{0.5f,0.5f,0.5f},
      owner(value), alpha(1), blendAlpha(0), ambient(0)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetSize)(void*);
        dimensions = reinterpret_cast<GetSize>(owner->table[57])(owner);
    }
}
// PE 0x10091B60. Shipped defect: the borrowed list retains the stale instance.
DebugTorusMesh::~DebugTorusMesh()
{
    if (blendAlpha) blendAlpha->release();
    if (ambient) ambient->release();
}
// PE 0x10091BA0.
void DebugTorusMesh::initializeParameters()
{
    if (owner)
    {
        blendAlpha = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "BLENDALPHA");
        if (blendAlpha) blendAlpha->addRef();
        ambient = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "MATERIALAMBIENT");
        if (ambient) ambient->addRef();
    }
}
// PE 0x10092510. Null allocations are inserted and later dereferenced, as shipped.
DebugTorusMesh* DebugTorusMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTorusMesh));
    DebugTorusMesh* instance = storage ? new (storage) DebugTorusMesh(this) : 0;
    instances.push_back(instance);
    if (!loaded && !dx9rend::resourceManagerIsReady())
    {
        typedef bool (__thiscall* Load)(void*);
        reinterpret_cast<Load>(table[11])(this);
    }
    loaded = 1;
    instance->initializeParameters();
    return instance;
}
// PE 0x100920D0. Draws pass zero without binding a declaration or testing pass count.
void DebugTorusMesh::draw()
{
    if (!owner->mesh) return;
    if (!m_name.empty()) dx9rend::effectSetTechniqueByName(owner->effect, m_name);
    else dx9rend::effectSetTechniqueByName(owner->effect, "t0");
    dx9rend::stateManagerSetStreamSource(0, owner->vertices, 0, 24);
    dx9rend::stateManagerSetIndices(owner->indices);
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 viewProjection = Mat4::getIdentity();
    const Mat4& projection = *view->getProjectionMatrixPtr();
    const Mat4& viewMatrix = *view->getInverseViewMatrixPtr();
    viewProjection.fullMult(viewMatrix, projection);
    blendAlpha->setValue(&alpha, 0);
    ambient->setValue(&colour, 0);
    owner->viewProjection->setValue(&viewProjection, 0);
    owner->world->setValue(&transform, 0);
    dx9rend::effectBegin(owner->effect);
    dx9rend::effectBeginPass(owner->effect, 0);
    dx9rend::rendererDrawIndexedPrimitive(owner->faceCount, 4, 0, 0, owner->vertexCount, 0);
    dx9rend::effectEnd(owner->effect);
}
#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER == 1310
static_assert(sizeof(DebugTorusMesh) == 0x9C, "retail torus instance layout");
static_assert(sizeof(DebugTorusMeshTemplate) == 0xBC, "retail torus template layout");
#endif
} // namespace

// PE 0x10092400. Removes the first matching borrowed pointer.
void debugTorusMeshRemoveInstance(void* value, void* instance)
{
    DebugTorusMeshTemplate* owner = static_cast<DebugTorusMeshTemplate*>(value);
    for (auto cursor = owner->instances.begin(); cursor != owner->instances.end(); ++cursor)
        if (*cursor == instance) { owner->instances.erase(cursor); return; }
}
// PE 0x1007CB30. Allocation failure is not guarded before virtual query.
IBase* __fastcall createDebugTorusMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugTorusMeshTemplate));
    DebugTorusMeshTemplate* instance = storage ? new (storage) DebugTorusMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result) torusTemplate_release(instance RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}
} } }
#endif
