// Debug X mesh geometry. Source placement is attested by the PE diagnostic path.
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
#include <cstring>
#include <list>
#include <new>

namespace dice { namespace hfe { namespace dx9rend {
extern void* g_rendererBase;
extern void* g_shaderManager;
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*);
void* bufferManagerCreateIndexBuffer(unsigned int, unsigned int, unsigned int);
void vertexBufferRelease(void*);
void indexBufferRelease(void*);
void vertexBufferSetD3D(void*, IDirect3DVertexBuffer9*);
void indexBufferSetD3D(void*, IDirect3DIndexBuffer9*);
void vertexBufferBindStream(void*, unsigned int);
void indexBufferBind(void*);
void vertexDeclarationBind(void*);
IDirect3DVertexDeclaration9* vertexDeclarationD3D(void*);
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int);
} } }
namespace dice { namespace hfe { namespace geom {
extern const unsigned int CID_DebugXMesh = 81036u;
namespace {
using namespace debugmesh;
struct DebugXMeshTemplate;
// PE 0x10093590 / 0x10093330: signed index range and three colour floats.
struct XColourRange { int firstIndex, indexCount; Vec3 colour; };
static_assert(sizeof(XColourRange) == 20, "X colour range stride");
extern void* const kXTable[68];
extern void* const kXTemplateTable[60];

// [INFERRED] Member names; native strings, lists and pointers follow issue #7.
struct DebugXMesh : GeometryInstanceData
{
    explicit DebugXMesh(DebugXMeshTemplate*);
    ~DebugXMesh();
    void initializeParameters();
    void draw();
    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Vec3 colour;                     // +0x40
    float alpha;                    // +0x4C
    DebugXMeshTemplate* owner;       // +0x50
    dx9rend::ShaderParameter* blendAlpha; // +0x54
    dx9rend::ShaderParameter* ambient; // +0x58
    Mat4 transform;                  // +0x5C
};
struct DebugXMeshTemplate
{
    DebugXMeshTemplate();
    ~DebugXMeshTemplate();
    bool upload(unsigned int, unsigned int, unsigned int, const void*, const unsigned short*, bool, void*);
    void setRanges(const struct XColourRange*, int);
    bool adjacency(float, unsigned short*, unsigned int);
    void load();
    DebugXMesh* createInstance();
    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugXMesh*> instances; // +0x10
    float width;                    // +0x1C
    float height;                   // +0x20
    float depth;                    // +0x24
    unsigned int vertexCount;        // +0x28
    std::string name;                // +0x2C
    std::string path;                // +0x48
    Vec3 instanceSize;               // +0x64
    Vec3 colour;                     // +0x70
    std::string shaderName;          // +0x7C
    ID3DXMesh* mesh;                 // +0x98
    dx9rend::Effect* effect;          // +0x9C
    dx9rend::ShaderParameter* viewProjection; // +0xA0
    dx9rend::ShaderParameter* world;  // +0xA4
    void* vertices;                  // +0xA8
    void* indices;                   // +0xAC
    void* declaration;               // +0xB0
    struct XColourRange* ranges;     // +0xB4
    int rangeCount;                  // +0xB8
};

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x100939B0.
int RD9_THUNK xMesh_addRef(DebugXMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK xMesh_getRef(DebugXMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK xMesh_destroy(DebugXMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugXMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK xMesh_release(DebugXMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { typedef void* (__thiscall* Destroy)(void*, unsigned int); reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1); }
    return refs;
}
// PE 0x10093970.
void* RD9_THUNK xMesh_query(DebugXMesh* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80036u ? s : 0; }
// PE 0x10093130 / 0x10092A80 / 0x10092A90 / 0x10092A20 / 0x10092A30.
void RD9_THUNK xMesh_draw(DebugXMesh* s RD9_THUNK_EDX, void*, void*, unsigned char) { s->draw(); }
int RD9_THUNK xMesh_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK xMesh_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK xMesh_owner(DebugXMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK xMesh_transform(DebugXMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x10092F10. All instances overwrite one bounds object.
const LocalBoundingBox* RD9_THUNK xMesh_bounds(DebugXMesh* s RD9_THUNK_EDX)
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
// PE 0x10093020. Ordered comparisons preserve the NaN selection.
float RD9_THUNK xMesh_radius(DebugXMesh* s RD9_THUNK_EDX)
{
    const float xz = s->dimensions.x < s->dimensions.z ? s->dimensions.z : s->dimensions.x;
    const float xy = s->dimensions.x < s->dimensions.y ? s->dimensions.y : s->dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}
// PE 0x10093080.
void RD9_THUNK xMesh_setTransform(DebugXMesh* s RD9_THUNK_EDX, const Mat4& value)
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
// PE 0x100930F0 / 0x10092A40 / 0x10092A50 / 0x10092A60 / 0x10093110 / 0x10092A70.
void RD9_THUNK xMesh_setColour(DebugXMesh* s RD9_THUNK_EDX, const Vec3& value) { s->colour = value; }
const Vec3& RD9_THUNK xMesh_colour(DebugXMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK xMesh_setAlpha(DebugXMesh* s RD9_THUNK_EDX, float value) { s->alpha = value; }
float RD9_THUNK xMesh_alpha(DebugXMesh* s RD9_THUNK_EDX) { return s->alpha; }
void RD9_THUNK xMesh_setDimensions(DebugXMesh* s RD9_THUNK_EDX, const Vec3& value) { s->dimensions = value; }
const Vec3& RD9_THUNK xMesh_dimensions(DebugXMesh* s RD9_THUNK_EDX) { return s->dimensions; }

#define X_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020C448.
void* const kXTable[68] = {
    X_SLOT(xMesh_addRef), X_SLOT(xMesh_getRef), X_SLOT(xMesh_release), X_SLOT(xMesh_query),
    X_SLOT(empty1), X_SLOT(minusOneFloat), X_SLOT(xMesh_draw), X_SLOT(empty2),
    X_SLOT(empty6), X_SLOT(empty1), X_SLOT(zero0), X_SLOT(xMesh_type),
    X_SLOT(xMesh_hasGeom), X_SLOT(xMesh_destroy), X_SLOT(xMesh_owner), X_SLOT(zero1),
    X_SLOT(xMesh_bounds), X_SLOT(xMesh_radius), X_SLOT(xMesh_transform), X_SLOT(empty2),
    X_SLOT(xMesh_setTransform), X_SLOT(empty2), X_SLOT(empty1), X_SLOT(zero0),
    X_SLOT(empty2), X_SLOT(empty1), X_SLOT(setName), X_SLOT(empty1),
    X_SLOT(zero0), X_SLOT(one), X_SLOT(empty1), X_SLOT(zero0),
    X_SLOT(one), X_SLOT(empty1), X_SLOT(minusOne), X_SLOT(zero0),
    X_SLOT(empty3), X_SLOT(empty2), X_SLOT(empty2), X_SLOT(empty1),
    X_SLOT(zero0), X_SLOT(zero0), X_SLOT(false0), X_SLOT(empty1),
    X_SLOT(zero0), X_SLOT(empty1), X_SLOT(empty1), X_SLOT(false0),
    X_SLOT(empty2), X_SLOT(zero0), X_SLOT(zero1), X_SLOT(zero0),
    X_SLOT(false0), X_SLOT(empty1), X_SLOT(false0), X_SLOT(empty1),
    X_SLOT(empty1), X_SLOT(seven), X_SLOT(zero0), X_SLOT(zero1),
    X_SLOT(empty0), X_SLOT(empty1), X_SLOT(xMesh_setColour), X_SLOT(xMesh_colour),
    X_SLOT(xMesh_setAlpha), X_SLOT(xMesh_alpha), X_SLOT(xMesh_setDimensions), X_SLOT(xMesh_dimensions)
};
// PE 0x10132630 / 0x10051710 / 0x10093B40 / 0x10093B10.
int RD9_THUNK xMeshTemplate_addRef(DebugXMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK xMeshTemplate_getRef(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK xMeshTemplate_release(DebugXMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugXMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK xMeshTemplate_query(DebugXMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80037u ? s : 0; }
// PE 0x100927B0 / 0x100927C0 / 0x100927D0 / 0x100927E0 / 0x100927F0.
unsigned int RD9_THUNK xMeshTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugXMesh; }
void RD9_THUNK xMeshTemplate_setName(DebugXMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->name = value; }
const std::string& RD9_THUNK xMeshTemplate_name(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK xMeshTemplate_setPath(DebugXMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->path = value; }
const std::string& RD9_THUNK xMeshTemplate_path(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x100938E0 / 0x10093610.
void* RD9_THUNK xMeshTemplate_create(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK xMeshTemplate_load(DebugXMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x10092800 / 0x10092810 / 0x10092AA0 / 0x10092820 / 0x10092AC0 / 0x10092830.
void RD9_THUNK xMeshTemplate_setShader(DebugXMeshTemplate* s RD9_THUNK_EDX, const std::string& value) { s->shaderName = value; }
const std::string& RD9_THUNK xMeshTemplate_shader(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK xMeshTemplate_setColour(DebugXMeshTemplate* s RD9_THUNK_EDX, const Vec3& value) { s->colour = value; }
const Vec3& RD9_THUNK xMeshTemplate_colour(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK xMeshTemplate_setSize(DebugXMeshTemplate* s RD9_THUNK_EDX, const Vec3& value) { s->instanceSize = value; }
const Vec3& RD9_THUNK xMeshTemplate_size(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }
// PE 0x10092840 / 0x10092850 / 0x10092860 / 0x10092870 / 0x10092880 / 0x10092890.
void RD9_THUNK xMeshTemplate_setWidth(DebugXMeshTemplate* s RD9_THUNK_EDX, float v) { s->width = v; }
float RD9_THUNK xMeshTemplate_width(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->width; }
void RD9_THUNK xMeshTemplate_setHeight(DebugXMeshTemplate* s RD9_THUNK_EDX, float v) { s->height = v; }
float RD9_THUNK xMeshTemplate_height(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->height; }
void RD9_THUNK xMeshTemplate_setDepth(DebugXMeshTemplate* s RD9_THUNK_EDX, float v) { s->depth = v; }
float RD9_THUNK xMeshTemplate_depth(DebugXMeshTemplate* s RD9_THUNK_EDX) { return s->depth; }
// PE 0x10092AE0 / 0x10093590 / 0x100928A0 / 0x100928C0.
bool RD9_THUNK xMeshTemplate_upload(DebugXMeshTemplate* s RD9_THUNK_EDX, unsigned int vertices,
    unsigned int indices, unsigned int stride, const void* data, const unsigned short* indexData,
    bool optimize, void* declaration)
{ return s->upload(vertices, indices, stride, data, indexData, optimize, declaration); }
void RD9_THUNK xMeshTemplate_ranges(DebugXMeshTemplate* s RD9_THUNK_EDX, const struct XColourRange* data, int count)
{ s->setRanges(data, count); }
unsigned int RD9_THUNK xMeshTemplate_faces(DebugXMeshTemplate* s RD9_THUNK_EDX)
{ return s->mesh ? s->mesh->GetNumFaces() : 0; }
bool RD9_THUNK xMeshTemplate_adjacency(DebugXMeshTemplate* s RD9_THUNK_EDX, float epsilon,
    unsigned short* data, unsigned int count) { return s->adjacency(epsilon, data, count); }
// PE 0x1020C5C0.
void* const kXTemplateTable[60] = {
    X_SLOT(xMeshTemplate_addRef), X_SLOT(xMeshTemplate_getRef), X_SLOT(xMeshTemplate_release), X_SLOT(xMeshTemplate_query),
    X_SLOT(xMeshTemplate_class), X_SLOT(xMeshTemplate_setName), X_SLOT(xMeshTemplate_name), X_SLOT(xMeshTemplate_setPath),
    X_SLOT(xMeshTemplate_path), X_SLOT(xMeshTemplate_create), X_SLOT(false0), X_SLOT(xMeshTemplate_load),
    X_SLOT(empty3), X_SLOT(lodDistance), X_SLOT(empty4), X_SLOT(materialValue),
    X_SLOT(empty4), X_SLOT(quarter), X_SLOT(empty1), X_SLOT(false0),
    X_SLOT(false0), X_SLOT(zero1), X_SLOT(zero3), X_SLOT(zero3),
    X_SLOT(empty1), X_SLOT(false0), X_SLOT(empty1), X_SLOT(false0),
    X_SLOT(empty3), X_SLOT(zero2), X_SLOT(empty1), X_SLOT(true0),
    X_SLOT(empty1), X_SLOT(false0), X_SLOT(empty1), X_SLOT(sixteen),
    X_SLOT(empty1), X_SLOT(false0), X_SLOT(empty1), X_SLOT(false0),
    X_SLOT(zero1), X_SLOT(zero1), X_SLOT(decompress), X_SLOT(empty0),
    X_SLOT(xMeshTemplate_setShader), X_SLOT(xMeshTemplate_shader), X_SLOT(xMeshTemplate_setColour), X_SLOT(xMeshTemplate_colour),
    X_SLOT(xMeshTemplate_setWidth), X_SLOT(xMeshTemplate_width), X_SLOT(xMeshTemplate_setHeight), X_SLOT(xMeshTemplate_height),
    X_SLOT(xMeshTemplate_setDepth), X_SLOT(xMeshTemplate_depth), X_SLOT(xMeshTemplate_upload), X_SLOT(xMeshTemplate_ranges),
    X_SLOT(xMeshTemplate_faces), X_SLOT(xMeshTemplate_adjacency),
    X_SLOT(xMeshTemplate_setSize), X_SLOT(xMeshTemplate_size)
};
#undef X_SLOT

// PE 0x10093830.
DebugXMeshTemplate::DebugXMeshTemplate()
    : table(kXTemplateTable), loaded(0), field08(0), refs(1), width(1), height(1), depth(1), vertexCount(0),
      instanceSize{1,1,1}, colour{1,1,1}, shaderName("DebugD3DXMeshShapeShader"), mesh(0),
      effect(0), viewProjection(0), world(0), vertices(0), indices(0), declaration(0), ranges(0), rangeCount(0) {}
// PE 0x100936F0.
DebugXMeshTemplate::~DebugXMeshTemplate()
{
    if (vertices) dx9rend::vertexBufferRelease(vertices);
    if (indices) dx9rend::indexBufferRelease(indices);
    if (declaration) dx9rend::vertexDeclarationRelease(declaration);
    if (mesh) mesh->Release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (effect) dx9rend::effectRelease(effect);
    if (ranges) ::renddx9abi::engineFree(ranges);
}
// PE 0x10092AE0. Shipped failures leave the released mesh pointer in place.
bool DebugXMeshTemplate::upload(unsigned int numVertices, unsigned int numIndices,
    unsigned int stride, const void* data, const unsigned short* indexData,
    bool optimize, void* suppliedDeclaration)
{
    if (mesh) mesh->Release();
    if (vertices) dx9rend::vertexBufferRelease(vertices);
    if (indices) dx9rend::indexBufferRelease(indices);
    if (declaration) dx9rend::vertexDeclarationRelease(declaration);
    vertices = dx9rend::bufferManagerCreateVertexBuffer(numVertices, stride, 8, "DebugXMeshTemplate");
    indices = dx9rend::bufferManagerCreateIndexBuffer(numIndices, 8, 101);
    vertexCount = numVertices;
    ID3DXMesh* staging = 0;
    const D3DVERTEXELEMENT9* elements = 0;
    const D3DVERTEXELEMENT9 position[] = {
        {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0}, D3DDECL_END()
    };
    const D3DVERTEXELEMENT9 positionColour[] = {
        {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}, D3DDECL_END()
    };
    const D3DVERTEXELEMENT9 positionNormal[] = {
        {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0}, D3DDECL_END()
    };
    const D3DVERTEXELEMENT9 positionNormalColour[] = {
        {0,0,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_FLOAT3,0,D3DDECLUSAGE_NORMAL,0},
        {0,24,D3DDECLTYPE_D3DCOLOR,0,D3DDECLUSAGE_COLOR,0}, D3DDECL_END()
    };
    if (!suppliedDeclaration)
    {
        switch (stride)
        {
        case 12: elements = position; break;
        case 16: elements = positionColour; break;
        case 24: elements = positionNormal; break;
        case 28: elements = positionNormalColour; break;
        }
        declaration = dx9rend::vertexDeclarationCreate(elements);
    }
    else
    {
        // Shipped defect: both GetDeclaration outputs are null, so elements
        // stays null. The supplied wrapper is stored without retaining it.
        dx9rend::vertexDeclarationD3D(suppliedDeclaration)->GetDeclaration(0, 0);
        declaration = suppliedDeclaration;
    }
    IDirect3DDevice9* device = static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device;
    if (D3DXCreateMesh(numIndices / 3, vertexCount, 0x110, elements, device, &staging) != 0)
        return false;
    void* vertexData;
    staging->LockVertexBuffer(0x800, &vertexData);
    if (data) std::memcpy(vertexData, data, vertexCount * stride);
    staging->UnlockVertexBuffer();
    void* indicesData;
    staging->LockIndexBuffer(0x800, &indicesData);
    if (indexData) std::memcpy(indicesData, indexData, numIndices * 2u);
    staging->UnlockIndexBuffer();
    if (stride == 24) D3DXComputeNormals(staging, 0);
    if (optimize)
    {
        DWORD* adjacencyData = static_cast<DWORD*>(::renddx9abi::engineAlloc(numIndices * 4u));
        staging->GenerateAdjacency(0, adjacencyData);
        ID3DXMesh* optimized = 0;
        staging->Optimize(0x04000110, adjacencyData, 0, 0, 0, &optimized);
        staging->Release();
        staging = optimized;
        ::renddx9abi::engineFree(adjacencyData);
    }
    staging->CloneMesh(0x220, elements,
        static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device, &mesh);
    staging->Release();
    if (mesh)
    {
        IDirect3DVertexBuffer9* vertexBuffer;
        IDirect3DIndexBuffer9* indexBuffer;
        mesh->GetVertexBuffer(&vertexBuffer);
        mesh->GetIndexBuffer(&indexBuffer);
        dx9rend::vertexBufferSetD3D(vertices, vertexBuffer);
        dx9rend::indexBufferSetD3D(indices, indexBuffer);
        vertexBuffer->Release();
        indexBuffer->Release();
    }
    return true;
}
// PE 0x10093590. Allocation failure and signed counts remain unchecked.
void DebugXMeshTemplate::setRanges(const XColourRange* data, int count)
{
    if (ranges) ::renddx9abi::engineFree(ranges);
    const unsigned int bytes = static_cast<unsigned int>(count) * 20u;
    XColourRange* storage = static_cast<XColourRange*>(::renddx9abi::engineAlloc(bytes));
    ranges = 0;
    if (storage)
    {
        const unsigned int unsignedCount = static_cast<unsigned int>(count);
        if (static_cast<int>(unsignedCount - 1u) >= 0)
            for (unsigned int i = 0; i < unsignedCount; ++i) storage[i].colour = Vec3{0,0,0};
        ranges = storage;
    }
    std::memcpy(ranges, data, bytes);
    rangeCount = count;
}
// PE 0x100928C0. Shipped output truncates DWORD face indices to 16 bits.
bool DebugXMeshTemplate::adjacency(float epsilon, unsigned short* data, unsigned int count)
{
    if (!mesh) return false;
    DWORD* adjacencyData = static_cast<DWORD*>(::renddx9abi::engineAlloc(count * 4u));
    if (mesh->GenerateAdjacency(epsilon, adjacencyData) != 0)
    {
        ::renddx9abi::engineFree(adjacencyData);
        return false;
    }
    for (unsigned int i = 0; i < count; ++i) data[i] = static_cast<unsigned short>(adjacencyData[i]);
    ::renddx9abi::engineFree(adjacencyData);
    return true;
}
// PE 0x100933D0. Shipped defect: repeated loads overwrite owned resource references.
void DebugXMeshTemplate::load()
{
    {
        const std::string prefix("Shaders/");
        const std::string shader = prefix + shaderName;
        effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, shader.c_str()));
    }
    viewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "WorldViewProjection");
    if (viewProjection) viewProjection->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugXMesh.cpp", 185, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugXMesh.cpp", 193, "")
        << "Could not load WorldViewProjection parameter from shader";
}
// PE 0x10093620. The template's colour is not copied into the instance.
DebugXMesh::DebugXMesh(DebugXMeshTemplate* value)
    : GeometryInstanceData(kXTable), refs(1), dimensions{0,0,0}, colour{0.5f,0.5f,0.5f},
      alpha(1), owner(value), blendAlpha(0), ambient(0)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetSize)(void*);
        dimensions = reinterpret_cast<GetSize>(owner->table[59])(owner);
    }
}
// PE 0x10092950. Shipped defect: the borrowed list retains the stale instance.
DebugXMesh::~DebugXMesh()
{
    if (blendAlpha) blendAlpha->release();
    if (ambient) ambient->release();
}
// PE 0x10092990.
void DebugXMesh::initializeParameters()
{
    if (owner)
    {
        blendAlpha = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "BLENDALPHA");
        if (blendAlpha) blendAlpha->addRef();
        ambient = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "MATERIALAMBIENT");
        if (ambient) ambient->addRef();
    }
}
// PE 0x100938E0. Null allocations are inserted and later dereferenced, as shipped.
DebugXMesh* DebugXMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugXMesh));
    DebugXMesh* instance = storage ? new (storage) DebugXMesh(this) : 0;
    instances.push_back(instance);
    if (!loaded)
    {
        typedef bool (__thiscall* Load)(void*);
        reinterpret_cast<Load>(table[11])(this);
    }
    loaded = 1;
    instance->initializeParameters();
    return instance;
}
// PE 0x10093130. The final pass is selected but never drawn.
void DebugXMesh::draw()
{
    if (!owner->mesh) return;
    dx9rend::stateManagerPush();
    if (!m_name.empty()) dx9rend::effectSetTechniqueByName(owner->effect, m_name);
    else dx9rend::effectSetTechniqueByName(owner->effect, "t0");
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 viewProjection = Mat4::getIdentity();
    const Mat4& projection = *view->getProjectionMatrixPtr();
    const Mat4& viewMatrix = *view->getInverseViewMatrixPtr();
    viewProjection.fullMult(viewMatrix, projection);
    if (blendAlpha) blendAlpha->setValue(&alpha, 0);
    if (ambient) ambient->setValue(&colour, 0);
    owner->viewProjection->setValue(&viewProjection, 0);
    owner->world->setValue(&transform, 0);
    dx9rend::vertexDeclarationBind(owner->declaration);
    dx9rend::vertexBufferBindStream(owner->vertices, 0);
    dx9rend::indexBufferBind(owner->indices);
    const int lastPass = static_cast<int>(dx9rend::effectBegin(owner->effect)) - 1;
    for (int pass = 0; pass < (lastPass > 1 ? lastPass : 1); ++pass)
    {
        dx9rend::effectBeginPass(owner->effect, pass);
        if (!owner->rangeCount)
        {
            ID3DXMesh* mesh = owner->mesh;
            const unsigned int count = owner->vertexCount;
            const unsigned int faces = mesh->GetNumFaces();
            dx9rend::rendererDrawIndexedPrimitive(faces, 4, 0, 0, count, 0);
        }
        else for (int i = 0; i < owner->rangeCount; ++i)
        {
            const XColourRange& range = owner->ranges[i];
            ambient->setValue(&range.colour, 0);
            dx9rend::rendererDrawIndexedPrimitive(range.indexCount / 3, 4, 0, 0, owner->vertexCount, range.firstIndex);
        }
    }
    dx9rend::effectBeginPass(owner->effect, lastPass);
    dx9rend::effectEnd(owner->effect);
    dx9rend::stateManagerPop();
}
#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER == 1310
static_assert(sizeof(DebugXMesh) == 0x9C, "retail xMesh instance layout");
static_assert(sizeof(DebugXMeshTemplate) == 0xBC, "retail xMesh template layout");
#endif
} // namespace

// PE 0x100937E0. Removes the first matching borrowed pointer.
void debugXMeshRemoveInstance(void* value, void* instance)
{
    DebugXMeshTemplate* owner = static_cast<DebugXMeshTemplate*>(value);
    for (auto cursor = owner->instances.begin(); cursor != owner->instances.end(); ++cursor)
        if (*cursor == instance) { owner->instances.erase(cursor); return; }
}
// PE 0x1007CB80. Allocation failure is not guarded before virtual query.
IBase* __fastcall createDebugXMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugXMeshTemplate));
    DebugXMeshTemplate* instance = storage ? new (storage) DebugXMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result)
    {
        typedef int (__thiscall* Release)(void*);
        reinterpret_cast<Release>(instance->table[2])(instance);
    }
    return static_cast<IBase*>(result);
}
} } }
#endif
