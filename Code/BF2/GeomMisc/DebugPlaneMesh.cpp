// Debug plane geometry. Source placement is attested by the PE diagnostic path.
#ifdef BF2_IMAGE_RENDDX9
#include "DebugMeshCommon.h"
#include "../RendDX9/RendDX9Math.h"   // rendmath::normalize (sub_100777A0)
#include "Debug.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <cstring>
#include <list>
#include <new>
#include <vector>

namespace dice { namespace hfe { namespace dx9rend {
class Effect;
extern void* g_shaderManager;
void* shaderManagerGetEffect(void*, const char*);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&);
int effectRelease(Effect*);
bool effectSetTechniqueByName(void*, const std::string&);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
void* bufferManagerCreateVertexBuffer(unsigned int, unsigned int, unsigned int, const char*);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferBindStream(void*, unsigned int);
void vertexBufferRelease(void*);
unsigned int vertexBufferVertexCount(void*);
void* bufferManagerCreateIndexBuffer(unsigned int, unsigned int, unsigned int);
void* indexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool indexBufferUnlock(void*);
void indexBufferBind(void*);
void indexBufferRelease(void*);
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int, unsigned int, unsigned int);
} } }

namespace dice { namespace hfe { namespace geom {
extern const unsigned int CID_DebugPlaneMesh = 81014u;
namespace {
using namespace debugmesh;
struct DebugPlaneMeshTemplate;
extern void* const kPlaneTable[66];
extern void* const kPlaneTemplateTable[54];

// [INFERRED] Member names; native strings, lists and pointers use the existing issue #7 ABI.
struct DebugPlaneMesh : GeometryInstanceData
{
    explicit DebugPlaneMesh(DebugPlaneMeshTemplate*);
    void draw();
    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Vec3 colour;                    // +0x40
    DebugPlaneMeshTemplate* owner;  // +0x4C
    Mat4 transform;                 // +0x50
};
// [INFERRED] Names; unsigned loop bounds prove both tessellation components.
struct PlaneTessellation { unsigned int x, y; };
struct PlaneVertex
{
    Vec3 position; Vec3 normal; float u, v;
    void assignFromFractions(float latitudeFraction, float longitudeFraction);
};
static_assert(sizeof(PlaneVertex) == 32, "plane vertex stream stride");
struct DebugPlaneMeshTemplate
{
    DebugPlaneMeshTemplate();
    ~DebugPlaneMeshTemplate();
    void generate();
    void load();
    DebugPlaneMesh* createInstance();
    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugPlaneMesh*> instances; // +0x10
    unsigned int vertexCount;       // +0x1C
    unsigned int indexCount;        // +0x20
    std::string name;               // +0x24
    std::string path;               // +0x40
    Vec3 instanceSize;              // +0x5C
    Vec3 colour;                    // +0x68
    std::string shaderName;         // +0x74
    std::string textureName;        // +0x90
    std::string texturePath;        // +0xAC
    PlaneTessellation tessellation; // +0xC8
    void* vertices;                 // +0xD0, counted VertexBuffer wrapper
    void* indices;                  // +0xD4, counted IndexBuffer wrapper
    dx9rend::Effect* effect;         // +0xD8
    dx9rend::EngineTexture* texture; // +0xDC
    dx9rend::ShaderParameter* textureParameter; // +0xE0
    dx9rend::ShaderParameter* blendAlpha; // +0xE4
    dx9rend::ShaderParameter* viewProjection; // +0xE8
    dx9rend::ShaderParameter* world; // +0xEC
};

// Builds one vertex on a sphere of radius 0.5 from latitude and longitude fractions in [0, 1]: the
// normal is the normalized position, u takes the longitude fraction and v the latitude fraction.
// The plane generator does not use it.
// PE 0x1008E5D0. unreachable in 1.50: no code or data references.
// [INFERRED] Name and member form: the instructions take the 32-byte vertex in ecx and two float
// stack arguments (retn 8), and nothing can read eax; no caller exposes a name.
void PlaneVertex::assignFromFractions(float latitudeFraction, float longitudeFraction)
{
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Only cos(latitude) is spilled; both angles stay unspilled.
    using namespace bf2x87;
    const Real latitude = sub(mul(wide(latitudeFraction), wide(3.1415927410125732421875f)),
                              wide(1.57079637050628662109375f));
    const Real longitude = sub(mul(wide(longitudeFraction), wide(6.283185482025146484375f)),
                               wide(3.1415927410125732421875f));
    const float cosLatitude = store(cosValue(latitude));
    position.x = store(mul(cosValue(longitude), wide(cosLatitude)));
    position.y = store(-sinValue(latitude));
    position.z = store(mul(sinValue(longitude), wide(cosLatitude)));
#else
    const float latitude = latitudeFraction * 3.1415927410125732421875f - 1.57079637050628662109375f;
    const float longitude = longitudeFraction * 6.283185482025146484375f - 3.1415927410125732421875f;
    const float cosLatitude = std::cos(latitude);
    position.x = std::cos(longitude) * cosLatitude;
    position.y = -std::sin(latitude);
    position.z = std::sin(longitude) * cosLatitude;
#endif
    position.x *= 0.5f;
    position.y *= 0.5f;
    position.z *= 0.5f;
    normal = position;
    rendmath::normalize(normal);
    u = longitudeFraction;
    v = latitudeFraction;
}

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x10082D60.
int RD9_THUNK plane_addRef(DebugPlaneMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK plane_getRef(DebugPlaneMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK plane_destroy(DebugPlaneMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugPlaneMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK plane_release(DebugPlaneMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs)
    {
        typedef void* (__thiscall* Destroy)(void*, unsigned int);
        reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1);
    }
    return refs;
}
// PE 0x1008F780.
void* RD9_THUNK plane_query(DebugPlaneMesh* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80007u ? s : 0; }
// PE 0x1008E890 / 0x1008E570 / 0x1008E580 / 0x1008E530 / 0x1008E540.
void RD9_THUNK plane_draw(DebugPlaneMesh* s RD9_THUNK_EDX, void*, void*, unsigned char) { s->draw(); }
int RD9_THUNK plane_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK plane_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK plane_owner(DebugPlaneMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK plane_transform(DebugPlaneMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x1008E670. All instances overwrite one shared local-bounds object.
const LocalBoundingBox* RD9_THUNK plane_bounds(DebugPlaneMesh* s RD9_THUNK_EDX)
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
// PE 0x1008E780.
float RD9_THUNK plane_radius(DebugPlaneMesh* s RD9_THUNK_EDX)
{
    const float xz = s->dimensions.x < s->dimensions.z ? s->dimensions.z : s->dimensions.x;
    const float xy = s->dimensions.x < s->dimensions.y ? s->dimensions.y : s->dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}
// PE 0x1008E7E0. Shipped defect: only diagonal entries receive the dimension scale.
void RD9_THUNK plane_setTransform(DebugPlaneMesh* s RD9_THUNK_EDX, const Mat4& v)
{
    s->transform = v;
    const Vec3 unit{1,1,1};
    if (!equalVector(s->dimensions, unit))
    {
        s->transform.m[0] *= s->dimensions.x;
        s->transform.m[5] *= s->dimensions.y;
        s->transform.m[10] *= s->dimensions.z;
    }
}
// PE 0x1008E850 / 0x1008E550 / 0x1008E870 / 0x1008E560. Colour has only three components.
void RD9_THUNK plane_setColour(DebugPlaneMesh* s RD9_THUNK_EDX, const Vec3& v) { s->colour = v; }
const Vec3& RD9_THUNK plane_colour(DebugPlaneMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK plane_setDimensions(DebugPlaneMesh* s RD9_THUNK_EDX, const Vec3& v) { s->dimensions = v; }
const Vec3& RD9_THUNK plane_dimensions(DebugPlaneMesh* s RD9_THUNK_EDX) { return s->dimensions; }
#define PLANE_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020BA98.
void* const kPlaneTable[66] = {
    PLANE_SLOT(plane_addRef), PLANE_SLOT(plane_getRef), PLANE_SLOT(plane_release), PLANE_SLOT(plane_query),
    PLANE_SLOT(empty1), PLANE_SLOT(minusOneFloat), PLANE_SLOT(plane_draw), PLANE_SLOT(empty2),
    PLANE_SLOT(empty6), PLANE_SLOT(empty1), PLANE_SLOT(zero0), PLANE_SLOT(plane_type),
    PLANE_SLOT(plane_hasGeom), PLANE_SLOT(plane_destroy), PLANE_SLOT(plane_owner), PLANE_SLOT(zero1),
    PLANE_SLOT(plane_bounds), PLANE_SLOT(plane_radius), PLANE_SLOT(plane_transform), PLANE_SLOT(empty2),
    PLANE_SLOT(plane_setTransform), PLANE_SLOT(empty2), PLANE_SLOT(empty1), PLANE_SLOT(zero0),
    PLANE_SLOT(empty2), PLANE_SLOT(empty1), PLANE_SLOT(setName), PLANE_SLOT(empty1),
    PLANE_SLOT(zero0), PLANE_SLOT(one), PLANE_SLOT(empty1), PLANE_SLOT(zero0),
    PLANE_SLOT(one), PLANE_SLOT(empty1), PLANE_SLOT(minusOne), PLANE_SLOT(zero0),
    PLANE_SLOT(empty3), PLANE_SLOT(empty2), PLANE_SLOT(empty2), PLANE_SLOT(empty1),
    PLANE_SLOT(zero0), PLANE_SLOT(zero0), PLANE_SLOT(false0), PLANE_SLOT(empty1),
    PLANE_SLOT(zero0), PLANE_SLOT(empty1), PLANE_SLOT(empty1), PLANE_SLOT(false0),
    PLANE_SLOT(empty2), PLANE_SLOT(zero0), PLANE_SLOT(zero1), PLANE_SLOT(zero0),
    PLANE_SLOT(false0), PLANE_SLOT(empty1), PLANE_SLOT(false0), PLANE_SLOT(empty1),
    PLANE_SLOT(empty1), PLANE_SLOT(seven), PLANE_SLOT(zero0), PLANE_SLOT(zero1),
    PLANE_SLOT(empty0), PLANE_SLOT(empty1), PLANE_SLOT(plane_setColour), PLANE_SLOT(plane_colour),
    PLANE_SLOT(plane_setDimensions), PLANE_SLOT(plane_dimensions)
};
// PE 0x10132630 / 0x10051710 / 0x1008FD40 / 0x1008FA60.
int RD9_THUNK planeTemplate_addRef(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK planeTemplate_getRef(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK planeTemplate_release(DebugPlaneMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugPlaneMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK planeTemplate_query(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80008u ? s : 0; }
// PE 0x1008E440 / 0x1008E450 / 0x1008E460 / 0x1008E470 / 0x1008E4A0.
unsigned int RD9_THUNK planeTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugPlaneMesh; }
void RD9_THUNK planeTemplate_setName(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->name = v; }
const std::string& RD9_THUNK planeTemplate_name(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK planeTemplate_setPath(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const std::string& v)
{ s->path = v; s->texturePath = s->path; }
const std::string& RD9_THUNK planeTemplate_path(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x1008ED20 / 0x1008F560.
void* RD9_THUNK planeTemplate_create(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK planeTemplate_load(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x1008E4B0 / 0x1008E4C0 / 0x1008E4D0 / 0x1008E4E0.
void RD9_THUNK planeTemplate_setShader(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->shaderName = v; }
const std::string& RD9_THUNK planeTemplate_shader(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK planeTemplate_setTexture(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->textureName = v; }
const std::string& RD9_THUNK planeTemplate_texture(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->textureName; }
// PE 0x1008E590 / 0x1008E4F0 / 0x1008F2C0 / 0x1008E500 / 0x1008E5B0 / 0x1008E510.
void RD9_THUNK planeTemplate_setColour(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->colour = v; }
const Vec3& RD9_THUNK planeTemplate_colour(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK planeTemplate_setTessellation(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const PlaneTessellation& v)
{ s->tessellation = v; s->generate(); }
const PlaneTessellation& RD9_THUNK planeTemplate_tessellation(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->tessellation; }
void RD9_THUNK planeTemplate_setSize(DebugPlaneMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->instanceSize = v; }
const Vec3& RD9_THUNK planeTemplate_size(DebugPlaneMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }
// PE 0x1020BBA0.
void* const kPlaneTemplateTable[54] = {
    PLANE_SLOT(planeTemplate_addRef), PLANE_SLOT(planeTemplate_getRef), PLANE_SLOT(planeTemplate_release), PLANE_SLOT(planeTemplate_query),
    PLANE_SLOT(planeTemplate_class), PLANE_SLOT(planeTemplate_setName), PLANE_SLOT(planeTemplate_name), PLANE_SLOT(planeTemplate_setPath),
    PLANE_SLOT(planeTemplate_path), PLANE_SLOT(planeTemplate_create), PLANE_SLOT(false0), PLANE_SLOT(planeTemplate_load),
    PLANE_SLOT(empty3), PLANE_SLOT(lodDistance), PLANE_SLOT(empty4), PLANE_SLOT(materialValue),
    PLANE_SLOT(empty4), PLANE_SLOT(quarter), PLANE_SLOT(empty1), PLANE_SLOT(false0),
    PLANE_SLOT(false0), PLANE_SLOT(zero1), PLANE_SLOT(zero3), PLANE_SLOT(zero3),
    PLANE_SLOT(empty1), PLANE_SLOT(false0), PLANE_SLOT(empty1), PLANE_SLOT(false0),
    PLANE_SLOT(empty3), PLANE_SLOT(zero2), PLANE_SLOT(empty1), PLANE_SLOT(true0),
    PLANE_SLOT(empty1), PLANE_SLOT(false0), PLANE_SLOT(empty1), PLANE_SLOT(sixteen),
    PLANE_SLOT(empty1), PLANE_SLOT(false0), PLANE_SLOT(empty1), PLANE_SLOT(false0),
    PLANE_SLOT(zero1), PLANE_SLOT(zero1), PLANE_SLOT(decompress), PLANE_SLOT(empty0),
    PLANE_SLOT(planeTemplate_setShader), PLANE_SLOT(planeTemplate_shader), PLANE_SLOT(planeTemplate_setTexture), PLANE_SLOT(planeTemplate_texture),
    PLANE_SLOT(planeTemplate_setColour), PLANE_SLOT(planeTemplate_colour), PLANE_SLOT(planeTemplate_setTessellation), PLANE_SLOT(planeTemplate_tessellation),
    PLANE_SLOT(planeTemplate_setSize), PLANE_SLOT(planeTemplate_size)
};
#undef PLANE_SLOT

// PE 0x1008EC50.
DebugPlaneMeshTemplate::DebugPlaneMeshTemplate()
    : table(kPlaneTemplateTable), loaded(0), field08(0), refs(1), vertexCount(0), indexCount(0),
      instanceSize{1,1,1}, colour{1,1,1}, shaderName("DebugSphereShader"),
      textureName("redcheckers.dds"), texturePath("../../BF2Editor/Content/DebugPrimitives/Textures/"),
      tessellation{40,40}, vertices(0), indices(0), effect(0), texture(0), textureParameter(0),
      blendAlpha(0), viewProjection(0), world(0) {}
// PE 0x1008EB00. The instance pointers are borrowed and never released here.
DebugPlaneMeshTemplate::~DebugPlaneMeshTemplate()
{
    if (vertices) dx9rend::vertexBufferRelease(vertices);
    if (indices) dx9rend::indexBufferRelease(indices);
    if (texture) dx9rend::textureReleaseRef(texture);
    if (textureParameter) textureParameter->release();
    if (blendAlpha) blendAlpha->release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (effect) dx9rend::effectRelease(effect);
}
// PE 0x1008EA40. Colour stays zero and the template is borrowed.
DebugPlaneMesh::DebugPlaneMesh(DebugPlaneMeshTemplate* value)
    : GeometryInstanceData(kPlaneTable), refs(1), dimensions{0,0,0}, colour{0,0,0}, owner(value)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetVector)(void*);
        dimensions = reinterpret_cast<GetVector>(owner->table[53])(owner);
    }
}
// PE 0x1008ED20. Null allocation is inserted in the borrowed list, as shipped.
DebugPlaneMesh* DebugPlaneMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugPlaneMesh));
    DebugPlaneMesh* instance = storage ? new (storage) DebugPlaneMesh(this) : 0;
    instances.push_back(instance);
    if (!loaded && !dx9rend::resourceManagerIsReady())
    {
        typedef bool (__thiscall* Load)(void*);
        reinterpret_cast<Load>(table[11])(this);
    }
    loaded = 1;
    return instance;
}
// PE 0x1008EDB0. Tessellation is unchecked and indices truncate to sixteen bits.
void DebugPlaneMeshTemplate::generate()
{
    const unsigned int width = tessellation.x;
    const unsigned int height = tessellation.y;
    std::vector<PlaneVertex> grid;
    for (unsigned int row = 0; row < height; ++row)
    {
        if (!width) continue;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] Retain unspilled fractions until the position stores.
        using namespace bf2x87;
        const float divisor = store(wide(width - 1u));
        const Real rowFraction = div(wide(row), wide(height - 1u));
        const float v = store(rowFraction);
        const float z = store(sub(add(rowFraction, rowFraction), wide(1.0f)));
#else
        const float divisor = static_cast<float>(width - 1u);
        const float v = static_cast<float>(row) / static_cast<float>(height - 1u);
        const float z = v + v - 1.0f;
#endif
        for (unsigned int column = 0; column < width; ++column)
        {
            PlaneVertex vertex;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const Real fraction = div(wide(column), wide(divisor));
            vertex.position.x = store(sub(add(fraction, fraction), wide(1.0f)));
            vertex.u = store(fraction);
#else
            const float fraction = static_cast<float>(column) / divisor;
            vertex.position.x = fraction + fraction - 1.0f;
            vertex.u = fraction;
#endif
            vertex.position.y = 0;
            vertex.position.z = z;
            vertex.normal = Vec3{0,1,1}; // Shipped defect: the normal is neither unit nor perpendicular.
            vertex.v = v;
            grid.push_back(vertex);
        }
    }
    std::vector<unsigned short> indexData;
    for (unsigned int row = 0; row < height - 1u; ++row)
        for (unsigned int column = 0; column < width - 1u; ++column)
        {
            const unsigned int first = row * width + column;
            const unsigned int next = (row + 1u) * width + column;
            indexData.push_back(static_cast<unsigned short>(first));
            indexData.push_back(static_cast<unsigned short>(next));
            indexData.push_back(static_cast<unsigned short>(first + 1u));
            indexData.push_back(static_cast<unsigned short>(next));
            indexData.push_back(static_cast<unsigned short>(next + 1u));
            indexData.push_back(static_cast<unsigned short>(first + 1u));
        }
    const unsigned int count = static_cast<unsigned int>(grid.size());
    if (vertexCount != count)
    {
        vertexCount = count;
        if (vertices) dx9rend::vertexBufferRelease(vertices);
        vertices = dx9rend::bufferManagerCreateVertexBuffer(vertexCount, 32, 8, "DebugPlaneMeshTemplate");
    }
    void* mapped = dx9rend::vertexBufferLock(vertices, 0, count, 0);
    if (!mapped)
    {
        dx9rend::vertexBufferUnlock(vertices); // Shipped unlock even after a failed lock.
        return;
    }
    std::memcpy(mapped, grid.data(), grid.size() * sizeof(PlaneVertex));
    dx9rend::vertexBufferUnlock(vertices);
    const unsigned int indexSize = static_cast<unsigned int>(indexData.size());
    if (indexCount != indexSize)
    {
        indexCount = indexSize;
        if (indices) dx9rend::indexBufferRelease(indices);
        indices = dx9rend::bufferManagerCreateIndexBuffer(indexCount, 8, 101);
    }
    mapped = dx9rend::indexBufferLock(indices, 0, indexCount, 0);
    if (mapped)
    {
        std::memcpy(mapped, indexData.data(), indexCount * sizeof(unsigned short));
        dx9rend::indexBufferUnlock(indices);
    }
}
// PE 0x1008F2E0. Repeated loads overwrite retained texture, effect and parameter references.
void DebugPlaneMeshTemplate::load()
{
    generate();
    texture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(texturePath + textureName));
    {
        const std::string prefix("Shaders/");
        const std::string shader = prefix + shaderName;
        effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, shader.c_str()));
    }
    textureParameter = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "TEXLAYER0");
    if (textureParameter) textureParameter->addRef();
    blendAlpha = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "BLENDALPHA");
    if (blendAlpha) blendAlpha->addRef();
    viewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "WorldViewProjection");
    if (viewProjection) viewProjection->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugPlaneMesh.cpp", 219, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugPlaneMesh.cpp", 227, "")
        << "Could not load WorldViewProjection parameter from shader";
}
// PE 0x1008E890. Ignores colour, draw arguments and the pass count; only pass zero is drawn.
void DebugPlaneMesh::draw()
{
    if (!owner->vertices) return;
    if (!m_name.empty()) dx9rend::effectSetTechniqueByName(owner->effect, m_name);
    else dx9rend::effectSetTechniqueByName(owner->effect, "t0");
    owner->textureParameter->setValue(owner->texture, 0);
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 viewProjection = Mat4::getIdentity();
    const Mat4& projection = *view->getProjectionMatrixPtr();
    const Mat4& viewMatrix = *view->getInverseViewMatrixPtr();
    viewProjection.fullMult(viewMatrix, projection);
    owner->viewProjection->setValue(&viewProjection, 0);
    owner->world->setValue(&transform, 0);
    dx9rend::vertexBufferBindStream(owner->vertices, 0);
    dx9rend::indexBufferBind(owner->indices);
    dx9rend::effectBegin(owner->effect);
    dx9rend::effectBeginPass(owner->effect, 0);
    const unsigned int count = dx9rend::vertexBufferVertexCount(owner->vertices);
    dx9rend::rendererDrawIndexedPrimitive(owner->indexCount / 3u, 4, 0, 0, count, 0);
    dx9rend::effectEnd(owner->effect);
}
#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER == 1310
static_assert(sizeof(DebugPlaneMesh) == 0x90, "retail plane instance layout");
static_assert(sizeof(DebugPlaneMeshTemplate) == 0xF0, "retail plane template layout");
#endif
} // namespace

// PE 0x1008EC00. Only the first matching borrowed instance is erased.
void debugPlaneMeshRemoveInstance(void* value, void* instance)
{
    DebugPlaneMeshTemplate* owner = static_cast<DebugPlaneMeshTemplate*>(value);
    for (auto cursor = owner->instances.begin(); cursor != owner->instances.end(); ++cursor)
        if (*cursor == instance) { owner->instances.erase(cursor); return; }
}
// PE 0x1007CA40. Allocation failure is unguarded at the virtual query.
IBase* __fastcall createDebugPlaneMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugPlaneMeshTemplate));
    DebugPlaneMeshTemplate* instance = storage ? new (storage) DebugPlaneMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result) planeTemplate_release(instance RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}
} } }
#endif
