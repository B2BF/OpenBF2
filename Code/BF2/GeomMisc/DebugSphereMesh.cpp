// Debug sphere geometry. Source placement is attested by the PE diagnostic path.
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
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int);
} } }

namespace dice { namespace hfe { namespace geom {
// Existing shared pack helper, PE 0x1008DD80 (StaticMesh.cpp).
const float* staticMeshPackVec4(const float[3], float);
extern const unsigned int CID_DebugSphereMesh = 81010u;
namespace {
using namespace debugmesh;
struct DebugSphereMeshTemplate;
extern void* const kSphereTable[66];
extern void* const kSphereTemplateTable[54];

// [INFERRED] Member names; native containers and pointers use the existing issue #7 ABI.
struct DebugSphereMesh : GeometryInstanceData
{
    explicit DebugSphereMesh(DebugSphereMeshTemplate*);
    void draw();
    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Math::Vec4 colour;               // +0x40
    DebugSphereMeshTemplate* owner; // +0x50
    Mat4 transform;                 // +0x54
};

// [INFERRED] Name; the generator's unsigned comparisons prove both component types.
struct SphereTessellation { unsigned int x, y; };
struct SphereVertex
{
    Vec3 position; Vec3 normal; float u, v;
    void assignFromFractions(float latitudeFraction, float longitudeFraction);
};
static_assert(sizeof(SphereVertex) == 32, "sphere vertex stream stride");
struct DebugSphereMeshTemplate
{
    DebugSphereMeshTemplate();
    ~DebugSphereMeshTemplate();
    void generate();
    void load();
    DebugSphereMesh* createInstance();
    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugSphereMesh*> instances; // +0x10
    unsigned int vertexCount;       // +0x1C
    std::string name;               // +0x20
    std::string path;               // +0x3C
    Vec3 instanceSize;              // +0x58
    Vec3 colour;                    // +0x64
    std::string shaderName;         // +0x70
    std::string textureName;        // +0x8C
    std::string texturePath;        // +0xA8
    SphereTessellation tessellation; // +0xC4
    void* vertices;                 // +0xCC, counted VertexBuffer wrapper
    dx9rend::Effect* effect;         // +0xD0
    dx9rend::EngineTexture* texture; // +0xD4
    dx9rend::ShaderParameter* textureParameter; // +0xD8
    dx9rend::ShaderParameter* blendAlpha; // +0xDC
    dx9rend::ShaderParameter* viewProjection; // +0xE0
    dx9rend::ShaderParameter* world; // +0xE4
    dx9rend::ShaderParameter* ambient; // +0xE8
    dx9rend::ShaderParameter* diffuse; // +0xEC
};

// Builds one unit-sphere vertex from latitude and longitude fractions in [0, 1]: the normal is the
// normalized position, u takes the longitude fraction and v the latitude fraction. generate()
// computes the same vertex inline.
// PE 0x1008CC50. unreachable in 1.50: no code or data references.
// [INFERRED] Name and member form: the instructions take the 32-byte vertex in ecx and two float
// stack arguments (retn 8), and nothing can read eax; no caller exposes a name.
void SphereVertex::assignFromFractions(float latitudeFraction, float longitudeFraction)
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
    normal = position;
    rendmath::normalize(normal);
    u = longitudeFraction;
    v = latitudeFraction;
}

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x10082D60.
int RD9_THUNK sphere_addRef(DebugSphereMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK sphere_getRef(DebugSphereMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK sphere_destroy(DebugSphereMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugSphereMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK sphere_release(DebugSphereMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs)
    {
        typedef void* (__thiscall* Destroy)(void*, unsigned int);
        reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1);
    }
    return refs;
}
// PE 0x1008E070.
void* RD9_THUNK sphere_query(DebugSphereMesh* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80003u ? s : 0; }
// PE 0x1008CEF0 / 0x1008CBF0 / 0x1008CC00 / 0x1008CBB0 / 0x1008CBC0.
void RD9_THUNK sphere_draw(DebugSphereMesh* s RD9_THUNK_EDX, void*, void*, unsigned char) { s->draw(); }
int RD9_THUNK sphere_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK sphere_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK sphere_owner(DebugSphereMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK sphere_transform(DebugSphereMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x1008CCC0. All instances overwrite one shared local-bounds object.
const LocalBoundingBox* RD9_THUNK sphere_bounds(DebugSphereMesh* s RD9_THUNK_EDX)
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
// PE 0x1008CDD0.
float RD9_THUNK sphere_radius(DebugSphereMesh* s RD9_THUNK_EDX)
{
    const float xz = s->dimensions.x < s->dimensions.z ? s->dimensions.z : s->dimensions.x;
    const float xy = s->dimensions.x < s->dimensions.y ? s->dimensions.y : s->dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}
// PE 0x1008CE30. Shipped defect: only diagonal entries receive the dimension scale.
void RD9_THUNK sphere_setTransform(DebugSphereMesh* s RD9_THUNK_EDX, const Mat4& v)
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
// PE 0x1008CEA0 / 0x1008CBD0 / 0x1008CED0 / 0x1008CBE0.
void RD9_THUNK sphere_setColour(DebugSphereMesh* s RD9_THUNK_EDX, const Math::Vec4& v) { s->colour = v; }
const Math::Vec4& RD9_THUNK sphere_colour(DebugSphereMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK sphere_setDimensions(DebugSphereMesh* s RD9_THUNK_EDX, const Vec3& v) { s->dimensions = v; }
const Vec3& RD9_THUNK sphere_dimensions(DebugSphereMesh* s RD9_THUNK_EDX) { return s->dimensions; }
#define SPHERE_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020B768.
void* const kSphereTable[66] = {
    SPHERE_SLOT(sphere_addRef), SPHERE_SLOT(sphere_getRef), SPHERE_SLOT(sphere_release), SPHERE_SLOT(sphere_query),
    SPHERE_SLOT(empty1), SPHERE_SLOT(minusOneFloat), SPHERE_SLOT(sphere_draw), SPHERE_SLOT(empty2),
    SPHERE_SLOT(empty6), SPHERE_SLOT(empty1), SPHERE_SLOT(zero0), SPHERE_SLOT(sphere_type),
    SPHERE_SLOT(sphere_hasGeom), SPHERE_SLOT(sphere_destroy), SPHERE_SLOT(sphere_owner), SPHERE_SLOT(zero1),
    SPHERE_SLOT(sphere_bounds), SPHERE_SLOT(sphere_radius), SPHERE_SLOT(sphere_transform), SPHERE_SLOT(empty2),
    SPHERE_SLOT(sphere_setTransform), SPHERE_SLOT(empty2), SPHERE_SLOT(empty1), SPHERE_SLOT(zero0),
    SPHERE_SLOT(empty2), SPHERE_SLOT(empty1), SPHERE_SLOT(setName), SPHERE_SLOT(empty1),
    SPHERE_SLOT(zero0), SPHERE_SLOT(one), SPHERE_SLOT(empty1), SPHERE_SLOT(zero0),
    SPHERE_SLOT(one), SPHERE_SLOT(empty1), SPHERE_SLOT(minusOne), SPHERE_SLOT(zero0),
    SPHERE_SLOT(empty3), SPHERE_SLOT(empty2), SPHERE_SLOT(empty2), SPHERE_SLOT(empty1),
    SPHERE_SLOT(zero0), SPHERE_SLOT(zero0), SPHERE_SLOT(false0), SPHERE_SLOT(empty1),
    SPHERE_SLOT(zero0), SPHERE_SLOT(empty1), SPHERE_SLOT(empty1), SPHERE_SLOT(false0),
    SPHERE_SLOT(empty2), SPHERE_SLOT(zero0), SPHERE_SLOT(zero1), SPHERE_SLOT(zero0),
    SPHERE_SLOT(false0), SPHERE_SLOT(empty1), SPHERE_SLOT(false0), SPHERE_SLOT(empty1),
    SPHERE_SLOT(empty1), SPHERE_SLOT(seven), SPHERE_SLOT(zero0), SPHERE_SLOT(zero1),
    SPHERE_SLOT(empty0), SPHERE_SLOT(empty1), SPHERE_SLOT(sphere_setColour), SPHERE_SLOT(sphere_colour),
    SPHERE_SLOT(sphere_setDimensions), SPHERE_SLOT(sphere_dimensions)
};
// PE 0x10132630 / 0x10051710 / 0x1008E410 / 0x1008E130.
int RD9_THUNK sphereTemplate_addRef(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK sphereTemplate_getRef(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK sphereTemplate_release(DebugSphereMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugSphereMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK sphereTemplate_query(DebugSphereMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80004u ? s : 0; }
// PE 0x1008CAE0 / 0x1008CAF0 / 0x1008CB00 / 0x1008CB10 / 0x1008CB20.
unsigned int RD9_THUNK sphereTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugSphereMesh; }
void RD9_THUNK sphereTemplate_setName(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->name = v; }
const std::string& RD9_THUNK sphereTemplate_name(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK sphereTemplate_setPath(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->path = v; }
const std::string& RD9_THUNK sphereTemplate_path(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x1008D420 / 0x1008DD70.
void* RD9_THUNK sphereTemplate_create(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK sphereTemplate_load(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x1008CB30 / 0x1008CB40 / 0x1008CB50 / 0x1008CB60.
void RD9_THUNK sphereTemplate_setShader(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->shaderName = v; }
const std::string& RD9_THUNK sphereTemplate_shader(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK sphereTemplate_setTexture(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->textureName = v; }
const std::string& RD9_THUNK sphereTemplate_texture(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->textureName; }
// PE 0x1008CC10 / 0x1008CB70 / 0x1008D990 / 0x1008CB80 / 0x1008CC30 / 0x1008CB90.
void RD9_THUNK sphereTemplate_setColour(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->colour = v; }
const Vec3& RD9_THUNK sphereTemplate_colour(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK sphereTemplate_setTessellation(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const SphereTessellation& v)
{ s->tessellation = v; s->generate(); }
const SphereTessellation& RD9_THUNK sphereTemplate_tessellation(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->tessellation; }
void RD9_THUNK sphereTemplate_setSize(DebugSphereMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->instanceSize = v; }
const Vec3& RD9_THUNK sphereTemplate_size(DebugSphereMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }
// PE 0x1020B870.
void* const kSphereTemplateTable[54] = {
    SPHERE_SLOT(sphereTemplate_addRef), SPHERE_SLOT(sphereTemplate_getRef), SPHERE_SLOT(sphereTemplate_release), SPHERE_SLOT(sphereTemplate_query),
    SPHERE_SLOT(sphereTemplate_class), SPHERE_SLOT(sphereTemplate_setName), SPHERE_SLOT(sphereTemplate_name), SPHERE_SLOT(sphereTemplate_setPath),
    SPHERE_SLOT(sphereTemplate_path), SPHERE_SLOT(sphereTemplate_create), SPHERE_SLOT(false0), SPHERE_SLOT(sphereTemplate_load),
    SPHERE_SLOT(empty3), SPHERE_SLOT(lodDistance), SPHERE_SLOT(empty4), SPHERE_SLOT(materialValue),
    SPHERE_SLOT(empty4), SPHERE_SLOT(quarter), SPHERE_SLOT(empty1), SPHERE_SLOT(false0),
    SPHERE_SLOT(false0), SPHERE_SLOT(zero1), SPHERE_SLOT(zero3), SPHERE_SLOT(zero3),
    SPHERE_SLOT(empty1), SPHERE_SLOT(false0), SPHERE_SLOT(empty1), SPHERE_SLOT(false0),
    SPHERE_SLOT(empty3), SPHERE_SLOT(zero2), SPHERE_SLOT(empty1), SPHERE_SLOT(true0),
    SPHERE_SLOT(empty1), SPHERE_SLOT(false0), SPHERE_SLOT(empty1), SPHERE_SLOT(sixteen),
    SPHERE_SLOT(empty1), SPHERE_SLOT(false0), SPHERE_SLOT(empty1), SPHERE_SLOT(false0),
    SPHERE_SLOT(zero1), SPHERE_SLOT(zero1), SPHERE_SLOT(decompress), SPHERE_SLOT(empty0),
    SPHERE_SLOT(sphereTemplate_setShader), SPHERE_SLOT(sphereTemplate_shader), SPHERE_SLOT(sphereTemplate_setTexture), SPHERE_SLOT(sphereTemplate_texture),
    SPHERE_SLOT(sphereTemplate_setColour), SPHERE_SLOT(sphereTemplate_colour), SPHERE_SLOT(sphereTemplate_setTessellation), SPHERE_SLOT(sphereTemplate_tessellation),
    SPHERE_SLOT(sphereTemplate_setSize), SPHERE_SLOT(sphereTemplate_size)
};
#undef SPHERE_SLOT

// PE 0x1008D340.
DebugSphereMeshTemplate::DebugSphereMeshTemplate()
    : table(kSphereTemplateTable), loaded(0), field08(0), refs(1), vertexCount(0),
      instanceSize{1,1,1}, colour{1,1,1}, shaderName("DebugSphereShader"),
      textureName("redcheckers.dds"), texturePath("../../BF2Editor/Content/DebugPrimitives/Textures/"),
      tessellation{40,40}, vertices(0), effect(0), texture(0), textureParameter(0), blendAlpha(0),
      viewProjection(0), world(0), ambient(0), diffuse(0) {}
// PE 0x1008D1F0. The instance pointers are borrowed and never released here.
DebugSphereMeshTemplate::~DebugSphereMeshTemplate()
{
    if (vertices) dx9rend::vertexBufferRelease(vertices);
    if (texture) dx9rend::textureReleaseRef(texture);
    if (textureParameter) textureParameter->release();
    if (blendAlpha) blendAlpha->release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (ambient) ambient->release();
    if (diffuse) diffuse->release();
    if (effect) dx9rend::effectRelease(effect);
}
// PE 0x1008D0E0. The owner is borrowed; there is no removal call on destruction.
DebugSphereMesh::DebugSphereMesh(DebugSphereMeshTemplate* value)
    : GeometryInstanceData(kSphereTable), refs(1), dimensions{0,0,0}, colour{0,0,0,0}, owner(value)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetVector)(void*);
        dimensions = reinterpret_cast<GetVector>(owner->table[53])(owner);
        const Vec3& rgb = reinterpret_cast<GetVector>(owner->table[49])(owner);
        std::memcpy(&colour, staticMeshPackVec4(&rgb.x, 1.0f), sizeof(colour));
    }
    else { dimensions = Vec3{1,1,1}; colour = Math::Vec4{1,1,1,1}; }
}
// PE 0x1008D420. Null allocation is inserted in the borrowed list, as shipped.
DebugSphereMesh* DebugSphereMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugSphereMesh));
    DebugSphereMesh* instance = storage ? new (storage) DebugSphereMesh(this) : 0;
    instances.push_back(instance);
    if (!loaded && !dx9rend::resourceManagerIsReady())
    {
        typedef bool (__thiscall* Load)(void*);
        reinterpret_cast<Load>(table[11])(this);
    }
    loaded = 1;
    return instance;
}

// PE 0x1008D4B0. Unsigned underflow, division by zero and unchecked grid reads are shipped.
void DebugSphereMeshTemplate::generate()
{
    const unsigned int width = tessellation.x;
    const unsigned int height = tessellation.y;
    std::vector<SphereVertex> grid;
    for (unsigned int row = 0; row < height; ++row)
    {
        if (!width) continue;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] Keep the original single stores and unspilled latitude.
        using namespace bf2x87;
        const float u = store(div(wide(row), wide(height - 1u)));
        const float divisor = store(wide(width - 1u));
        const Real longitude = sub(mul(wide(u), wide(6.283185482025146484375f)), wide(3.1415927410125732421875f));
        const float cosLongitude = store(cosValue(longitude));
        const float sinLongitude = store(sinValue(longitude));
#else
        const float u = static_cast<float>(row) / static_cast<float>(height - 1u);
        const float divisor = static_cast<float>(width - 1u);
        const float longitude = u * 6.283185482025146484375f - 3.1415927410125732421875f;
        const float cosLongitude = std::cos(longitude);
        const float sinLongitude = std::sin(longitude);
#endif
        for (unsigned int column = 0; column < width; ++column)
        {
            SphereVertex vertex;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
            const Real fraction = div(wide(column), wide(divisor));
            const float v = store(fraction);
            const Real latitude = sub(mul(fraction, wide(3.1415927410125732421875f)), wide(1.57079637050628662109375f));
            const Real cosLatitude = cosValue(latitude);
            vertex.position.x = store(mul(wide(cosLongitude), cosLatitude));
            vertex.position.y = store(-sinValue(latitude));
            vertex.position.z = store(mul(wide(sinLongitude), cosLatitude));
#else
            const float v = static_cast<float>(column) / divisor;
            const float latitude = v * 3.1415927410125732421875f - 1.57079637050628662109375f;
            const float cosLatitude = std::cos(latitude);
            vertex.position.x = cosLongitude * cosLatitude;
            vertex.position.y = -std::sin(latitude);
            vertex.position.z = sinLongitude * cosLatitude;
#endif
            vertex.normal = vertex.position;
            rendmath::normalize(vertex.normal);
            vertex.u = u;
            vertex.v = v;
            grid.push_back(vertex);
        }
    }
    std::vector<SphereVertex> triangles;
    for (unsigned int row = 0; row < height - 1u; ++row)
        for (unsigned int column = 0; column < width - 1u; ++column)
        {
            const unsigned int first = row * width + column;
            const unsigned int next = (row + 1u) * width + column;
            triangles.push_back(grid[first]);
            triangles.push_back(grid[next]);
            triangles.push_back(grid[first + 1u]);
            triangles.push_back(grid[next]);
            triangles.push_back(grid[next + 1u]);
            triangles.push_back(grid[first + 1u]);
        }
    const unsigned int count = static_cast<unsigned int>(triangles.size());
    if (vertexCount != count)
    {
        vertexCount = count;
        if (vertices) dx9rend::vertexBufferRelease(vertices);
        vertices = dx9rend::bufferManagerCreateVertexBuffer(vertexCount, 32, 8, "DebugSphereMeshTemplate");
    }
    void* mapped = dx9rend::vertexBufferLock(vertices, 0, count, 0);
    if (!mapped) return;
    std::memcpy(mapped, triangles.data(), triangles.size() * sizeof(SphereVertex));
    dx9rend::vertexBufferUnlock(vertices);
}

// PE 0x1008D9B0. Repeated loads overwrite retained texture, effect and parameter references.
void DebugSphereMeshTemplate::load()
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
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugSphereMesh.cpp", 226, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugSphereMesh.cpp", 234, "")
        << "Could not load WorldViewProjection parameter from shader";
    ambient = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "MATERIALAMBIENT");
    if (ambient) ambient->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugSphereMesh.cpp", 242, "")
        << "Could not load MatAmbient parameter from shader";
    diffuse = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "MATERIALDIFFUSE");
    if (diffuse) diffuse->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugSphereMesh.cpp", 250, "")
        << "Could not load MatDiffuse parameter from shader";
}
// PE 0x1008CEF0. View, occluder and save-state arguments are unused.
void DebugSphereMesh::draw()
{
    if (!owner->vertices) return;
    if (!m_name.empty()) dx9rend::effectSetTechniqueByName(owner->effect, m_name);
    else dx9rend::effectSetTechniqueByName(owner->effect, "t0");
    if (m_name == "editor") dx9rend::stateManagerClear(2, 0, 1.0f, 0);
    owner->textureParameter->setValue(owner->texture, 0);
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 viewProjection = Mat4::getIdentity();
    const Mat4& projection = *view->getProjectionMatrixPtr();
    const Mat4& viewMatrix = *view->getInverseViewMatrixPtr();
    viewProjection.fullMult(viewMatrix, projection);
    owner->viewProjection->setValue(&viewProjection, 0);
    owner->world->setValue(&transform, 0);
    owner->diffuse->setValue(&colour, 0);
    dx9rend::vertexBufferBindStream(owner->vertices, 0);
    const int passes = static_cast<int>(dx9rend::effectBegin(owner->effect));
    for (int pass = 0; pass < passes; ++pass)
    {
        dx9rend::effectBeginPass(owner->effect, static_cast<unsigned int>(pass));
        dx9rend::rendererDrawPrimitive(owner->vertexCount / 3u, 4, 0);
    }
    dx9rend::effectEnd(owner->effect);
}
#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER == 1310
static_assert(sizeof(DebugSphereMesh) == 0x94, "retail sphere instance layout");
static_assert(sizeof(DebugSphereMeshTemplate) == 0xF0, "retail sphere template layout");
#endif
} // namespace

// PE 0x1008D2F0. Only the first matching borrowed instance is erased.
void debugSphereMeshRemoveInstance(void* value, void* instance)
{
    DebugSphereMeshTemplate* owner = static_cast<DebugSphereMeshTemplate*>(value);
    for (auto cursor = owner->instances.begin(); cursor != owner->instances.end(); ++cursor)
        if (*cursor == instance) { owner->instances.erase(cursor); return; }
}
// PE 0x1007C9A0. Allocation failure is unguarded at the virtual query.
IBase* __fastcall createDebugSphereMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugSphereMeshTemplate));
    DebugSphereMeshTemplate* instance = storage ? new (storage) DebugSphereMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result) sphereTemplate_release(instance RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}
} } }
#endif
