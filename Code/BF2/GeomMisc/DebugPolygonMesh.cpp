// Debug polygon geometry. Source placement is attested by the PE diagnostic path.
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
extern const unsigned int CID_DebugPolygonMesh = 81018u;
namespace {
using namespace debugmesh;
struct DebugPolygonMeshTemplate;
extern void* const kPolygonTable[68];
extern void* const kPolygonTemplateTable[54];

// [INFERRED] Member names; native strings, lists and pointers follow issue #7.
struct DebugPolygonMesh : GeometryInstanceData
{
    explicit DebugPolygonMesh(DebugPolygonMeshTemplate*);
    ~DebugPolygonMesh();
    void initializeParameters();
    void draw();
    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Vec3 colour;                     // +0x40
    DebugPolygonMeshTemplate* owner; // +0x4C
    float alpha;                     // +0x50
    dx9rend::ShaderParameter* blendAlpha; // +0x54
    dx9rend::ShaderParameter* ambient; // +0x58
    Mat4 transform;                  // +0x5C
};
struct DebugPolygonMeshTemplate
{
    DebugPolygonMeshTemplate();
    ~DebugPolygonMeshTemplate();
    void createResources();
    void load();
    DebugPolygonMesh* createInstance();
    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugPolygonMesh*> instances; // +0x10
    float length;                   // +0x1C
    unsigned int sides;             // +0x20
    unsigned int field24;           // +0x24
    std::string name;               // +0x28
    std::string path;               // +0x44
    Vec3 instanceSize;              // +0x60
    Vec3 colour;                    // +0x6C
    std::string shaderName;         // +0x78
    ID3DXMesh* mesh;                // +0x94
    dx9rend::Effect* effect;         // +0x98
    dx9rend::ShaderParameter* viewProjection; // +0x9C
    dx9rend::ShaderParameter* world; // +0xA0
    IDirect3DVertexBuffer9* vertices; // +0xA4
    IDirect3DIndexBuffer9* indices;  // +0xA8
    unsigned int faceCount;         // +0xAC
    unsigned int vertexCount;       // +0xB0
};

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x10091800.
int RD9_THUNK polygon_addRef(DebugPolygonMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK polygon_getRef(DebugPolygonMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK polygon_destroy(DebugPolygonMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugPolygonMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK polygon_release(DebugPolygonMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { typedef void* (__thiscall* Destroy)(void*, unsigned int); reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1); }
    return refs;
}
// PE 0x100917C0.
void* RD9_THUNK polygon_query(DebugPolygonMesh* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80009u ? s : 0; }
// PE 0x100912F0 / 0x10090EB0 / 0x10090EC0 / 0x10090E50 / 0x10090E60.
void RD9_THUNK polygon_draw(DebugPolygonMesh* s RD9_THUNK_EDX, void*, void*, unsigned char) { s->draw(); }
int RD9_THUNK polygon_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK polygon_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK polygon_owner(DebugPolygonMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK polygon_transform(DebugPolygonMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x100910D0. All instances overwrite one bounds object.
const LocalBoundingBox* RD9_THUNK polygon_bounds(DebugPolygonMesh* s RD9_THUNK_EDX)
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
// PE 0x100911E0. Ordered comparisons preserve the NaN selection.
float RD9_THUNK polygon_radius(DebugPolygonMesh* s RD9_THUNK_EDX)
{
    const float xz = s->dimensions.x < s->dimensions.z ? s->dimensions.z : s->dimensions.x;
    const float xy = s->dimensions.x < s->dimensions.y ? s->dimensions.y : s->dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}
// PE 0x10091240.
void RD9_THUNK polygon_setTransform(DebugPolygonMesh* s RD9_THUNK_EDX, const Mat4& value)
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
// PE 0x100912B0 / 0x10090E70 / 0x10090E80 / 0x10090E90 / 0x100912D0 / 0x10090EA0.
void RD9_THUNK polygon_setColour(DebugPolygonMesh* s RD9_THUNK_EDX, const Vec3& value) { s->colour = value; }
const Vec3& RD9_THUNK polygon_colour(DebugPolygonMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK polygon_setAlpha(DebugPolygonMesh* s RD9_THUNK_EDX, float value) { s->alpha = value; }
float RD9_THUNK polygon_alpha(DebugPolygonMesh* s RD9_THUNK_EDX) { return s->alpha; }
void RD9_THUNK polygon_setDimensions(DebugPolygonMesh* s RD9_THUNK_EDX, const Vec3& value) { s->dimensions = value; }
const Vec3& RD9_THUNK polygon_dimensions(DebugPolygonMesh* s RD9_THUNK_EDX) { return s->dimensions; }

#define POLY_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020BF88.
void* const kPolygonTable[68] = {
    POLY_SLOT(polygon_addRef), POLY_SLOT(polygon_getRef), POLY_SLOT(polygon_release), POLY_SLOT(polygon_query),
    POLY_SLOT(empty1), POLY_SLOT(minusOneFloat), POLY_SLOT(polygon_draw), POLY_SLOT(empty2),
    POLY_SLOT(empty6), POLY_SLOT(empty1), POLY_SLOT(zero0), POLY_SLOT(polygon_type),
    POLY_SLOT(polygon_hasGeom), POLY_SLOT(polygon_destroy), POLY_SLOT(polygon_owner), POLY_SLOT(zero1),
    POLY_SLOT(polygon_bounds), POLY_SLOT(polygon_radius), POLY_SLOT(polygon_transform), POLY_SLOT(empty2),
    POLY_SLOT(polygon_setTransform), POLY_SLOT(empty2), POLY_SLOT(empty1), POLY_SLOT(zero0),
    POLY_SLOT(empty2), POLY_SLOT(empty1), POLY_SLOT(setName), POLY_SLOT(empty1),
    POLY_SLOT(zero0), POLY_SLOT(one), POLY_SLOT(empty1), POLY_SLOT(zero0),
    POLY_SLOT(one), POLY_SLOT(empty1), POLY_SLOT(minusOne), POLY_SLOT(zero0),
    POLY_SLOT(empty3), POLY_SLOT(empty2), POLY_SLOT(empty2), POLY_SLOT(empty1),
    POLY_SLOT(zero0), POLY_SLOT(zero0), POLY_SLOT(false0), POLY_SLOT(empty1),
    POLY_SLOT(zero0), POLY_SLOT(empty1), POLY_SLOT(empty1), POLY_SLOT(false0),
    POLY_SLOT(empty2), POLY_SLOT(zero0), POLY_SLOT(zero1), POLY_SLOT(zero0),
    POLY_SLOT(false0), POLY_SLOT(empty1), POLY_SLOT(false0), POLY_SLOT(empty1),
    POLY_SLOT(empty1), POLY_SLOT(seven), POLY_SLOT(zero0), POLY_SLOT(zero1),
    POLY_SLOT(empty0), POLY_SLOT(empty1), POLY_SLOT(polygon_setColour), POLY_SLOT(polygon_colour),
    POLY_SLOT(polygon_setAlpha), POLY_SLOT(polygon_alpha), POLY_SLOT(polygon_setDimensions), POLY_SLOT(polygon_dimensions)
};
// PE 0x10132630 / 0x10051710 / 0x10091990 / 0x10091960.
int RD9_THUNK polygonTemplate_addRef(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK polygonTemplate_getRef(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK polygonTemplate_release(DebugPolygonMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugPolygonMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK polygonTemplate_query(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80010u ? s : 0; }
// PE 0x10090C30 / 0x10090C40 / 0x10090C50 / 0x10090C60 / 0x10090C70.
unsigned int RD9_THUNK polygonTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugPolygonMesh; }
void RD9_THUNK polygonTemplate_setName(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->name = value; }
const std::string& RD9_THUNK polygonTemplate_name(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK polygonTemplate_setPath(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->path = value; }
const std::string& RD9_THUNK polygonTemplate_path(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x10091720 / 0x100914B0.
void* RD9_THUNK polygonTemplate_create(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK polygonTemplate_load(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x10090C80 / 0x10090C90 / 0x10090ED0 / 0x10090CA0 / 0x10090EF0 / 0x10090CB0.
void RD9_THUNK polygonTemplate_setShader(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->shaderName = value; }
const std::string& RD9_THUNK polygonTemplate_shader(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK polygonTemplate_setColour(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, const Vec3& value) { s->colour = value; }
const Vec3& RD9_THUNK polygonTemplate_colour(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK polygonTemplate_setSize(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, const Vec3& value) { s->instanceSize = value; }
const Vec3& RD9_THUNK polygonTemplate_size(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }
// PE 0x10090D40 / 0x10090D50 / 0x10090D60 / 0x10090D70.
void RD9_THUNK polygonTemplate_setLength(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, float value) { s->length = value; }
float RD9_THUNK polygonTemplate_length(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->length; }
void RD9_THUNK polygonTemplate_setSides(DebugPolygonMeshTemplate* s RD9_THUNK_EDX, unsigned int value) { s->sides = value; }
unsigned int RD9_THUNK polygonTemplate_sides(DebugPolygonMeshTemplate* s RD9_THUNK_EDX) { return s->sides; }
// PE 0x1020C0E8.
void* const kPolygonTemplateTable[54] = {
    POLY_SLOT(polygonTemplate_addRef), POLY_SLOT(polygonTemplate_getRef), POLY_SLOT(polygonTemplate_release), POLY_SLOT(polygonTemplate_query),
    POLY_SLOT(polygonTemplate_class), POLY_SLOT(polygonTemplate_setName), POLY_SLOT(polygonTemplate_name), POLY_SLOT(polygonTemplate_setPath),
    POLY_SLOT(polygonTemplate_path), POLY_SLOT(polygonTemplate_create), POLY_SLOT(false0), POLY_SLOT(polygonTemplate_load),
    POLY_SLOT(empty3), POLY_SLOT(lodDistance), POLY_SLOT(empty4), POLY_SLOT(materialValue),
    POLY_SLOT(empty4), POLY_SLOT(quarter), POLY_SLOT(empty1), POLY_SLOT(false0),
    POLY_SLOT(false0), POLY_SLOT(zero1), POLY_SLOT(zero3), POLY_SLOT(zero3),
    POLY_SLOT(empty1), POLY_SLOT(false0), POLY_SLOT(empty1), POLY_SLOT(false0),
    POLY_SLOT(empty3), POLY_SLOT(zero2), POLY_SLOT(empty1), POLY_SLOT(true0),
    POLY_SLOT(empty1), POLY_SLOT(false0), POLY_SLOT(empty1), POLY_SLOT(sixteen),
    POLY_SLOT(empty1), POLY_SLOT(false0), POLY_SLOT(empty1), POLY_SLOT(false0),
    POLY_SLOT(zero1), POLY_SLOT(zero1), POLY_SLOT(decompress), POLY_SLOT(empty0),
    POLY_SLOT(polygonTemplate_setShader), POLY_SLOT(polygonTemplate_shader), POLY_SLOT(polygonTemplate_setColour), POLY_SLOT(polygonTemplate_colour),
    POLY_SLOT(polygonTemplate_setLength), POLY_SLOT(polygonTemplate_length), POLY_SLOT(polygonTemplate_setSides), POLY_SLOT(polygonTemplate_sides),
    POLY_SLOT(polygonTemplate_setSize), POLY_SLOT(polygonTemplate_size)
};
#undef POLY_SLOT

// PE 0x10091670.
DebugPolygonMeshTemplate::DebugPolygonMeshTemplate()
    : table(kPolygonTemplateTable), loaded(0), field08(0), refs(1), length(1), sides(3), field24(0),
      instanceSize{1,1,1}, colour{1,1,1}, shaderName("DebugD3DXMeshShapeShader"), mesh(0),
      effect(0), viewProjection(0), world(0), vertices(0), indices(0), faceCount(0), vertexCount(0) {}
// PE 0x10091590.
DebugPolygonMeshTemplate::~DebugPolygonMeshTemplate()
{
    if (vertices) vertices->Release();
    if (indices) indices->Release();
    if (mesh) mesh->Release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (effect) dx9rend::effectRelease(effect);
}
// PE 0x10090CC0. No HRESULT is tested; the declaration is fetched and discarded.
void DebugPolygonMeshTemplate::createResources()
{
    D3DXCreatePolygon(static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device,
                      length, sides, &mesh, 0);
    D3DVERTEXELEMENT9 elements[65];
    mesh->GetDeclaration(elements);
    mesh->GetVertexBuffer(&vertices);
    mesh->GetIndexBuffer(&indices);
    vertexCount = mesh->GetNumVertices();
    faceCount = mesh->GetNumFaces();
}
// PE 0x10090F10. Shipped defect: repeated loads overwrite owned resource references.
void DebugPolygonMeshTemplate::load()
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
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugPolygonMesh.cpp", 179, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugPolygonMesh.cpp", 187, "")
        << "Could not load WorldViewProjection parameter from shader";
}
// PE 0x100914C0. The template's colour is not copied into the instance.
DebugPolygonMesh::DebugPolygonMesh(DebugPolygonMeshTemplate* value)
    : GeometryInstanceData(kPolygonTable), refs(1), dimensions{0,0,0}, colour{0.5f,0.5f,0.5f},
      owner(value), alpha(1), blendAlpha(0), ambient(0)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetSize)(void*);
        dimensions = reinterpret_cast<GetSize>(owner->table[53])(owner);
    }
}
// PE 0x10090D80. Shipped defect: the borrowed list retains the stale instance.
DebugPolygonMesh::~DebugPolygonMesh()
{
    if (blendAlpha) blendAlpha->release();
    if (ambient) ambient->release();
}
// PE 0x10090DC0.
void DebugPolygonMesh::initializeParameters()
{
    if (owner)
    {
        blendAlpha = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "BLENDALPHA");
        if (blendAlpha) blendAlpha->addRef();
        ambient = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "MATERIALAMBIENT");
        if (ambient) ambient->addRef();
    }
}
// PE 0x10091720. Null allocations are inserted and later dereferenced, as shipped.
DebugPolygonMesh* DebugPolygonMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugPolygonMesh));
    DebugPolygonMesh* instance = storage ? new (storage) DebugPolygonMesh(this) : 0;
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
// PE 0x100912F0. Draws pass zero without binding a declaration or testing pass count.
void DebugPolygonMesh::draw()
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
static_assert(sizeof(DebugPolygonMesh) == 0x9C, "retail polygon instance layout");
static_assert(sizeof(DebugPolygonMeshTemplate) == 0xB4, "retail polygon template layout");
#endif
} // namespace

// PE 0x10091620. Removes the first matching borrowed pointer.
void debugPolygonMeshRemoveInstance(void* value, void* instance)
{
    DebugPolygonMeshTemplate* owner = static_cast<DebugPolygonMeshTemplate*>(value);
    for (auto cursor = owner->instances.begin(); cursor != owner->instances.end(); ++cursor)
        if (*cursor == instance) { owner->instances.erase(cursor); return; }
}
// PE 0x1007CAE0. Allocation failure is not guarded before virtual query.
IBase* __fastcall createDebugPolygonMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugPolygonMeshTemplate));
    DebugPolygonMeshTemplate* instance = storage ? new (storage) DebugPolygonMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result) polygonTemplate_release(instance RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}
} } }
#endif
