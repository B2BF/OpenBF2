// Box geometry used by the renderer's diagnostic overlays.
// Source placement is attested by the PE's DebugBoxMesh.cpp diagnostic path.
#ifdef BF2_IMAGE_RENDDX9

#include "Debug.h"
#include "../Geom/GeometryInstanceData.h"
#include "../Math/LocalBoundingBox.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/RendDX9Math.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendDX9/Texture.h"
#include "../RendMisc/IRenderView.h"
#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif
#include <d3dx9.h>
#include <cfloat>
#include <cmath>
#include <list>
#include <new>
#include <string>

namespace dice { namespace hfe { namespace dx9rend {
class Effect;
extern void* g_rendererBase;
extern void* g_shaderManager;
extern unsigned char g_debugBoxMeshDrawEnabled;
void* shaderManagerGetEffect(void*, const char*);
void* effectGetShaderParameter(Effect*, const std::string&);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&);
int effectRelease(Effect*);
bool effectSetTechniqueByName(void*, const std::string&);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
void vertexDeclarationRelease(void*);
void vertexDeclarationBind(void*);
void rendererDrawIndexedPrimitive(unsigned int, unsigned int, int, unsigned int,
                                  unsigned int, unsigned int);
} } }

namespace dice { namespace hfe { namespace geom {
extern const unsigned int CID_DebugBoxMesh = 81012u;

namespace {
struct DebugBoxMeshTemplate;
extern void* const kBoxTable[69];
extern void* const kBoxTemplateTable[56];

// [INFERRED] Member names; every field below has a direct constructor/accessor use.
// Native containers and pointers use the native ABI (issue #7).
struct DebugBoxMesh : GeometryInstanceData
{
    explicit DebugBoxMesh(DebugBoxMeshTemplate* owner);
    ~DebugBoxMesh();
    void initializeParameters();
    void setTransform(const Mat4& source);
    const LocalBoundingBox* bounds() const;
    float radius() const;
    void draw(unsigned char saveState);

    int refs;                       // PE +0x30
    Vec3 dimensions;                 // +0x34
    Math::Vec4 colour;               // +0x40
    float textureScale;             // +0x50
    DebugBoxMeshTemplate* owner;     // +0x54
    dx9rend::ShaderParameter* ambient; // +0x58
    dx9rend::ShaderParameter* scale; // +0x5C
    Mat4 transform;                 // +0x60
    dx9rend::EngineTexture* texture; // +0xA0
};

struct DebugBoxMeshTemplate
{
    DebugBoxMeshTemplate();
    ~DebugBoxMeshTemplate();
    void createResources();
    void load();
    DebugBoxMesh* createInstance();
    void removeInstance(DebugBoxMesh* instance);

    void* const* table;              // PE +0x00
    unsigned char loaded;            // +0x04
    unsigned int field08;            // +0x08
    int refs;                       // +0x0C
    std::list<DebugBoxMesh*> instances; // +0x10
    Vec3 boxSize;                   // +0x1C
    unsigned int field28;            // +0x28
    std::string name;               // +0x2C
    std::string path;               // +0x48
    Vec3 instanceSize;              // +0x64
    Vec3 vector70;                  // +0x70
    std::string shaderName;         // +0x7C
    ID3DXMesh* mesh;                // +0x98
    dx9rend::Effect* effect;         // +0x9C
    dx9rend::ShaderParameter* viewProjection; // +0xA0
    dx9rend::ShaderParameter* world; // +0xA4
    dx9rend::ShaderParameter* textureParameter; // +0xA8
    IDirect3DVertexBuffer9* vertices; // +0xAC
    IDirect3DIndexBuffer9* indices;  // +0xB0
    void* declaration;              // +0xB4
    unsigned int faceCount;         // +0xB8
    unsigned int vertexCount;       // +0xBC
};

// PE 0x100E13B0 / 0x100E1480 / 0x10083AB0 / 0x10083A80 / 0x10082D20 / 0x10037D40.
// Shared empty retail bodies retain each callee-cleanup arity.
void RD9_THUNK box_empty1(void* RD9_THUNK_EDX, unsigned int) {}
void RD9_THUNK box_empty2(void* RD9_THUNK_EDX, unsigned int, unsigned int) {}
void RD9_THUNK box_empty3(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) {}
void RD9_THUNK box_empty4(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int, unsigned int) {}
void RD9_THUNK box_empty6(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int,
                         unsigned int, unsigned int, unsigned int) {}
void RD9_THUNK box_empty0(void* RD9_THUNK_EDX) {}
// PE 0x100ED3B0 / 0x10088060 / 0x10083AA0 / 0x10083AC0.
int RD9_THUNK box_zero0(void* RD9_THUNK_EDX) { return 0; }
int RD9_THUNK box_zero1(void* RD9_THUNK_EDX, unsigned int) { return 0; }
int RD9_THUNK box_zero2(void* RD9_THUNK_EDX, unsigned int, unsigned int) { return 0; }
int RD9_THUNK box_zero3(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) { return 0; }
// PE 0x10083AF0 / 0x10083AD0 / 0x10186100 / 0x10082D40 / 0x10082D50 / 0x10083AE0.
bool RD9_THUNK box_false(void* RD9_THUNK_EDX) { return false; }
bool RD9_THUNK box_true(void* RD9_THUNK_EDX) { return true; }
int RD9_THUNK box_one(void* RD9_THUNK_EDX) { return 1; }
int RD9_THUNK box_minusOne(void* RD9_THUNK_EDX) { return -1; }
int RD9_THUNK box_seven(void* RD9_THUNK_EDX) { return 7; }
int RD9_THUNK box_sixteen(void* RD9_THUNK_EDX) { return 16; }
// PE 0x10082D10 / 0x10083A60 / 0x10083A70 / 0x10083A90.
float RD9_THUNK box_minusOneFloat(void* RD9_THUNK_EDX) { return -1.0f; }
float RD9_THUNK box_lodDistance(void* RD9_THUNK_EDX, unsigned int, unsigned int) { return -1.0f; }
float RD9_THUNK box_materialValue(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) { return 15.0f; }
float RD9_THUNK box_quarter(void* RD9_THUNK_EDX, unsigned int, unsigned int, unsigned int) { return 0.25f; }

// PE 0x101071D0 / 0x101071E0 / 0x101071F0 / 0x10083B60.
int RD9_THUNK box_addRef(DebugBoxMesh* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK box_getRef(DebugBoxMesh* s RD9_THUNK_EDX) { return s->refs; }
void* RD9_THUNK box_destroy(DebugBoxMesh* s RD9_THUNK_EDX, unsigned int flags)
{
    s->~DebugBoxMesh();
    if (flags & 1) ::renddx9abi::engineFree(s);
    return s;
}
int RD9_THUNK box_release(DebugBoxMesh* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs)
    {
        typedef void* (__thiscall* Destroy)(void*, unsigned int);
        reinterpret_cast<Destroy>(s->m_vptr[13])(s, 1);
    }
    return refs;
}
// PE 0x10083B20.
void* RD9_THUNK box_query(DebugBoxMesh* s RD9_THUNK_EDX, unsigned int iid)
{
    return iid == 1 || iid == 0xADCBF4FCu || iid == 0x492FE0FEu || iid == 80005u ? s : 0;
}
// PE 0x10083530 / 0x10083050 / 0x10083060 / 0x10082FB0 / 0x10082FC0.
void RD9_THUNK box_draw(DebugBoxMesh* s RD9_THUNK_EDX, void*, void*, unsigned char state) { s->draw(state); }
int RD9_THUNK box_type(void* RD9_THUNK_EDX) { return 2; }
bool RD9_THUNK box_hasGeom(void* RD9_THUNK_EDX, int index) { return index == 0; }
void* RD9_THUNK box_owner(DebugBoxMesh* s RD9_THUNK_EDX) { return s->owner; }
const Mat4* RD9_THUNK box_transform(DebugBoxMesh* s RD9_THUNK_EDX) { return &s->transform; }
// PE 0x10083300 / 0x10083410 / 0x10083470 / 0x10082D30.
const LocalBoundingBox* RD9_THUNK box_bounds(DebugBoxMesh* s RD9_THUNK_EDX) { return s->bounds(); }
float RD9_THUNK box_radius(DebugBoxMesh* s RD9_THUNK_EDX) { return s->radius(); }
void RD9_THUNK box_setTransform(DebugBoxMesh* s RD9_THUNK_EDX, const Mat4& value) { s->setTransform(value); }
void RD9_THUNK box_setName(DebugBoxMesh* s RD9_THUNK_EDX, const std::string& value) { s->m_name = value; }
// PE 0x100834E0 / 0x10083030 / 0x10082FD0 / 0x10083010 / 0x10083020 / 0x10083510 / 0x10083040.
void RD9_THUNK box_setColour(DebugBoxMesh* s RD9_THUNK_EDX, const Math::Vec4& value) { s->colour = value; }
const Math::Vec4& RD9_THUNK box_colour(DebugBoxMesh* s RD9_THUNK_EDX) { return s->colour; }
void RD9_THUNK box_setTexture(DebugBoxMesh* s RD9_THUNK_EDX, const std::string& value)
{
    if (!s->texture)
        s->texture = static_cast<dx9rend::EngineTexture*>(dx9rend::textureManagerGetTexture(value));
}
void RD9_THUNK box_setScale(DebugBoxMesh* s RD9_THUNK_EDX, float value) { s->textureScale = value; }
float RD9_THUNK box_scale(DebugBoxMesh* s RD9_THUNK_EDX) { return s->textureScale; }
void RD9_THUNK box_setDimensions(DebugBoxMesh* s RD9_THUNK_EDX, const Vec3& value) { s->dimensions = value; }
const Vec3& RD9_THUNK box_dimensions(DebugBoxMesh* s RD9_THUNK_EDX) { return s->dimensions; }

#define BOX_SLOT(f) reinterpret_cast<void*>(&f)
// PE 0x1020A810. All 69 entries, including the common geometry bodies, are callable.
void* const kBoxTable[69] = {
    BOX_SLOT(box_addRef), BOX_SLOT(box_getRef), BOX_SLOT(box_release), BOX_SLOT(box_query),
    BOX_SLOT(box_empty1), BOX_SLOT(box_minusOneFloat), BOX_SLOT(box_draw), BOX_SLOT(box_empty2),
    BOX_SLOT(box_empty6), BOX_SLOT(box_empty1), BOX_SLOT(box_zero0), BOX_SLOT(box_type),
    BOX_SLOT(box_hasGeom), BOX_SLOT(box_destroy), BOX_SLOT(box_owner), BOX_SLOT(box_zero1),
    BOX_SLOT(box_bounds), BOX_SLOT(box_radius), BOX_SLOT(box_transform), BOX_SLOT(box_empty2),
    BOX_SLOT(box_setTransform), BOX_SLOT(box_empty2), BOX_SLOT(box_empty1), BOX_SLOT(box_zero0),
    BOX_SLOT(box_empty2), BOX_SLOT(box_empty1), BOX_SLOT(box_setName), BOX_SLOT(box_empty1),
    BOX_SLOT(box_zero0), BOX_SLOT(box_one), BOX_SLOT(box_empty1), BOX_SLOT(box_zero0),
    BOX_SLOT(box_one), BOX_SLOT(box_empty1), BOX_SLOT(box_minusOne), BOX_SLOT(box_zero0),
    BOX_SLOT(box_empty3), BOX_SLOT(box_empty2), BOX_SLOT(box_empty2), BOX_SLOT(box_empty1),
    BOX_SLOT(box_zero0), BOX_SLOT(box_zero0), BOX_SLOT(box_false), BOX_SLOT(box_empty1),
    BOX_SLOT(box_zero0), BOX_SLOT(box_empty1), BOX_SLOT(box_empty1), BOX_SLOT(box_false),
    BOX_SLOT(box_empty2), BOX_SLOT(box_zero0), BOX_SLOT(box_zero1), BOX_SLOT(box_zero0),
    BOX_SLOT(box_false), BOX_SLOT(box_empty1), BOX_SLOT(box_false), BOX_SLOT(box_empty1),
    BOX_SLOT(box_empty1), BOX_SLOT(box_seven), BOX_SLOT(box_zero0), BOX_SLOT(box_zero1),
    BOX_SLOT(box_empty0), BOX_SLOT(box_empty1), BOX_SLOT(box_setColour), BOX_SLOT(box_colour),
    BOX_SLOT(box_setTexture), BOX_SLOT(box_setScale), BOX_SLOT(box_scale), BOX_SLOT(box_setDimensions),
    BOX_SLOT(box_dimensions)
};

// PE 0x10132630 / 0x10051710 / 0x10083CE0 / 0x10083CA0.
int RD9_THUNK boxTemplate_addRef(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return ++s->refs; }
int RD9_THUNK boxTemplate_getRef(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->refs; }
int RD9_THUNK boxTemplate_release(DebugBoxMeshTemplate* s RD9_THUNK_EDX)
{
    const int refs = --s->refs;
    if (!refs) { s->~DebugBoxMeshTemplate(); ::renddx9abi::engineFree(s); }
    return refs;
}
void* RD9_THUNK boxTemplate_query(DebugBoxMeshTemplate* s RD9_THUNK_EDX, unsigned int iid)
{ return iid == 1 || iid == 0x17701FFFu || iid == 80006u ? s : 0; }
// PE 0x10082D90 / 0x10082DA0 / 0x10082DB0 / 0x10082DC0 / 0x10082DD0.
unsigned int RD9_THUNK boxTemplate_class(void* RD9_THUNK_EDX) { return CID_DebugBoxMesh; }
void RD9_THUNK boxTemplate_setName(DebugBoxMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->name = v; }
const std::string& RD9_THUNK boxTemplate_name(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->name; }
void RD9_THUNK boxTemplate_setPath(DebugBoxMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->path = v; }
const std::string& RD9_THUNK boxTemplate_path(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->path; }
// PE 0x100839C0 / 0x10083CD0 / 0x10083710.
void* RD9_THUNK boxTemplate_create(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->createInstance(); }
bool RD9_THUNK boxTemplate_loaded(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->mesh != 0; }
bool RD9_THUNK boxTemplate_load(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { s->load(); return true; }
// PE 0x10083B00. The shared unit vector is copied to the output.
Vec3* RD9_THUNK boxTemplate_decompress(void* RD9_THUNK_EDX, Vec3* out, const Vec3*)
{ *out = Vec3::getIdentity(); return out; }
// PE 0x10082DE0 / 0x10082DF0 / 0x10083070 / 0x10082E00.
void RD9_THUNK boxTemplate_setShader(DebugBoxMeshTemplate* s RD9_THUNK_EDX, const std::string& v) { s->shaderName = v; }
const std::string& RD9_THUNK boxTemplate_shader(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->shaderName; }
void RD9_THUNK boxTemplate_setVector70(DebugBoxMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->vector70 = v; }
const Vec3& RD9_THUNK boxTemplate_vector70(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->vector70; }
// PE 0x10082EC0 / 0x10082ED0 / 0x10082EE0 / 0x10082EF0 / 0x10082F00 / 0x10082F10.
void RD9_THUNK boxTemplate_setX(DebugBoxMeshTemplate* s RD9_THUNK_EDX, float v) { s->boxSize.x = v; }
float RD9_THUNK boxTemplate_x(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->boxSize.x; }
void RD9_THUNK boxTemplate_setY(DebugBoxMeshTemplate* s RD9_THUNK_EDX, float v) { s->boxSize.y = v; }
float RD9_THUNK boxTemplate_y(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->boxSize.y; }
void RD9_THUNK boxTemplate_setZ(DebugBoxMeshTemplate* s RD9_THUNK_EDX, float v) { s->boxSize.z = v; }
float RD9_THUNK boxTemplate_z(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->boxSize.z; }
// PE 0x10083090 / 0x10082E10.
void RD9_THUNK boxTemplate_setSize(DebugBoxMeshTemplate* s RD9_THUNK_EDX, const Vec3& v) { s->instanceSize = v; }
const Vec3& RD9_THUNK boxTemplate_size(DebugBoxMeshTemplate* s RD9_THUNK_EDX) { return s->instanceSize; }

// PE 0x1020A928.
void* const kBoxTemplateTable[56] = {
    BOX_SLOT(boxTemplate_addRef), BOX_SLOT(boxTemplate_getRef), BOX_SLOT(boxTemplate_release), BOX_SLOT(boxTemplate_query),
    BOX_SLOT(boxTemplate_class), BOX_SLOT(boxTemplate_setName), BOX_SLOT(boxTemplate_name), BOX_SLOT(boxTemplate_setPath),
    BOX_SLOT(boxTemplate_path), BOX_SLOT(boxTemplate_create), BOX_SLOT(boxTemplate_loaded), BOX_SLOT(boxTemplate_load),
    BOX_SLOT(box_empty3), BOX_SLOT(box_lodDistance), BOX_SLOT(box_empty4), BOX_SLOT(box_materialValue),
    BOX_SLOT(box_empty4), BOX_SLOT(box_quarter), BOX_SLOT(box_empty1), BOX_SLOT(box_false),
    BOX_SLOT(box_false), BOX_SLOT(box_zero1), BOX_SLOT(box_zero3), BOX_SLOT(box_zero3),
    BOX_SLOT(box_empty1), BOX_SLOT(box_false), BOX_SLOT(box_empty1), BOX_SLOT(box_false),
    BOX_SLOT(box_empty3), BOX_SLOT(box_zero2), BOX_SLOT(box_empty1), BOX_SLOT(box_true),
    BOX_SLOT(box_empty1), BOX_SLOT(box_false), BOX_SLOT(box_empty1), BOX_SLOT(box_sixteen),
    BOX_SLOT(box_empty1), BOX_SLOT(box_false), BOX_SLOT(box_empty1), BOX_SLOT(box_false),
    BOX_SLOT(box_zero1), BOX_SLOT(box_zero1), BOX_SLOT(boxTemplate_decompress), BOX_SLOT(box_empty0),
    BOX_SLOT(boxTemplate_setShader), BOX_SLOT(boxTemplate_shader), BOX_SLOT(boxTemplate_setVector70), BOX_SLOT(boxTemplate_vector70),
    BOX_SLOT(boxTemplate_setX), BOX_SLOT(boxTemplate_x), BOX_SLOT(boxTemplate_setY), BOX_SLOT(boxTemplate_y),
    BOX_SLOT(boxTemplate_setZ), BOX_SLOT(boxTemplate_z), BOX_SLOT(boxTemplate_setSize), BOX_SLOT(boxTemplate_size)
};
#undef BOX_SLOT

// PE 0x10083910. The field at +0x28 is initialized even though no accessor reads it.
DebugBoxMeshTemplate::DebugBoxMeshTemplate()
    : table(kBoxTemplateTable), loaded(0), field08(0), refs(1), boxSize{1,1,1}, field28(0),
      instanceSize{1,1,1}, vector70{1,1,1}, shaderName("DebugD3DXMeshShapeShader"),
      mesh(0), effect(0), viewProjection(0), world(0), textureParameter(0), vertices(0),
      indices(0), declaration(0), faceCount(0), vertexCount(0)
{}

// PE 0x10083800. Instance pointers are borrowed; only the list nodes are destroyed.
DebugBoxMeshTemplate::~DebugBoxMeshTemplate()
{
    if (vertices) vertices->Release();
    if (indices) indices->Release();
    if (mesh) mesh->Release();
    if (viewProjection) viewProjection->release();
    if (world) world->release();
    if (textureParameter) textureParameter->release();
    if (effect) dx9rend::effectRelease(effect);
    if (declaration) dx9rend::vertexDeclarationRelease(declaration);
}

// PE 0x10082E20. HRESULTs and the resulting mesh are deliberately not checked.
void DebugBoxMeshTemplate::createResources()
{
    D3DXCreateBox(static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device,
                  boxSize.x, boxSize.y, boxSize.z, &mesh, 0);
    D3DVERTEXELEMENT9 elements[65];
    mesh->GetDeclaration(elements);
    declaration = dx9rend::vertexDeclarationCreate(elements);
    mesh->GetVertexBuffer(&vertices);
    mesh->GetIndexBuffer(&indices);
    vertexCount = mesh->GetNumVertices();
    faceCount = mesh->GetNumFaces();
}

// PE 0x100830B0. A repeated load overwrites its resource references, as shipped.
void DebugBoxMeshTemplate::load()
{
    createResources();
    std::string shader("Shaders/");
    shader += shaderName;
    effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(dx9rend::g_shaderManager, shader.c_str()));
    textureParameter = static_cast<dx9rend::ShaderParameter*>(dx9rend::effectGetShaderParameter(effect, "TEXLAYER0"));
    viewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "WorldViewProjection");
    if (viewProjection) viewProjection->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugBoxMesh.cpp", 188, "")
        << "Could not load WorldViewProjection parameter from shader";
    world = dx9rend::effectGetShaderParameterBorrowedTyped(effect, "World");
    if (world) world->addRef();
    else ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugBoxMesh.cpp", 196, "")
        << "Could not load WorldViewProjection parameter from shader"; // Shipped message names the other parameter.
}

// PE 0x10083720. Creating an instance does not retain its template.
DebugBoxMesh::DebugBoxMesh(DebugBoxMeshTemplate* ownerValue)
    : GeometryInstanceData(kBoxTable), refs(1), dimensions{0,0,0}, colour{0.5f,0.5f,0.5f,1},
      textureScale(50.0f), owner(ownerValue), ambient(0), scale(0), texture(0)
{
    transform = Mat4::getIdentity();
    if (owner)
    {
        typedef const Vec3& (__thiscall* GetSize)(void*);
        dimensions = reinterpret_cast<GetSize>(owner->table[55])(owner);
    }
}

// PE 0x100832A0. Shipped defect: the template's instance list is not updated here.
DebugBoxMesh::~DebugBoxMesh()
{
    if (ambient) ambient->release();
    if (scale) scale->release();
    if (texture) dx9rend::textureReleaseRef(texture);
}

// PE 0x10082F20. Repeated initialization overwrites the retained parameter pointers.
void DebugBoxMesh::initializeParameters()
{
    if (owner)
    {
        ambient = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "MATERIALAMBIENT");
        if (ambient) ambient->addRef();
        scale = dx9rend::effectGetShaderParameterBorrowedTyped(owner->effect, "TEXTURESCALE");
        if (scale) scale->addRef();
    }
}

// PE 0x100839C0. Allocation failure is still inserted and dereferenced by the tail.
DebugBoxMesh* DebugBoxMeshTemplate::createInstance()
{
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugBoxMesh));
    DebugBoxMesh* instance = storage ? new (storage) DebugBoxMesh(this) : 0;
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

// PE 0x100838C0. Erases only the first matching borrowed pointer.
void DebugBoxMeshTemplate::removeInstance(DebugBoxMesh* instance)
{
    for (auto cursor = instances.begin(); cursor != instances.end(); ++cursor)
        if (*cursor == instance) { instances.erase(cursor); return; }
}

// PE 0x10083300. All instances overwrite the same local-bounds object.
const LocalBoundingBox* DebugBoxMesh::bounds() const
{
    static LocalBoundingBox box;
    const float z = dimensions.z * 0.5f;
    box.m_derivedDataDirty = true;
    box.m_max.z = z;
    box.m_min.x = -(dimensions.x * 0.5f);
    box.m_min.y = -(dimensions.y * 0.5f);
    box.m_min.z = -z;
    box.m_max.x = dimensions.x * 0.5f;
    box.m_max.y = dimensions.y * 0.5f;
    return &box;
}

// PE 0x10083410. Ordered comparisons preserve the PE's NaN selection.
float DebugBoxMesh::radius() const
{
    const float xz = dimensions.x < dimensions.z ? dimensions.z : dimensions.x;
    const float xy = dimensions.x < dimensions.y ? dimensions.y : dimensions.x;
    return (xy < xz ? xz : xy) * 0.5f;
}

// PE 0x10083470 / comparison helper 0x10037DC0. Only diagonal entries are scaled.
void DebugBoxMesh::setTransform(const Mat4& source)
{
    transform = source;
    bool unit = true;
    const float* size = &dimensions.x;
    for (int i = 0; i < 3; ++i)
    {
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] Keep the x87 subtraction unspilled on native Apple.
        if (!bf2x87::lessEqual(bf2x87::abs(bf2x87::sub(bf2x87::wide(size[i]), bf2x87::wide(1.0f))),
                          bf2x87::wide(FLT_EPSILON))) { unit = false; break; }
#else
        if (!(std::fabs(size[i] - 1.0f) <= FLT_EPSILON)) { unit = false; break; }
#endif
    }
    if (!unit)
    {
        transform.m[0] *= dimensions.x;
        transform.m[5] *= dimensions.y;
        transform.m[10] *= dimensions.z;
    }
}

// PE 0x10083530. View and occluder arguments are unused; saveState is the low byte.
void DebugBoxMesh::draw(unsigned char saveState)
{
    if (!owner->mesh || !dx9rend::g_debugBoxMeshDrawEnabled) return;
    if (!m_name.empty()) dx9rend::effectSetTechniqueByName(owner->effect, m_name);
    else dx9rend::effectSetTechniqueByName(owner->effect, "t0");
    if (saveState) dx9rend::stateManagerPush();
    dx9rend::vertexDeclarationBind(owner->declaration);
    dx9rend::stateManagerSetStreamSource(0, owner->vertices, 0, owner->mesh->GetNumBytesPerVertex());
    dx9rend::stateManagerSetIndices(owner->indices);
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 viewProjection;
    rendmath::constructIdentity(viewProjection);                       // 0x1008360C
    const Mat4& projection = *view->getProjectionMatrixPtr();
    const Mat4& viewMatrix = *view->getInverseViewMatrixPtr();
    geom::geometryRendererMatrixProduct(viewProjection, viewMatrix, projection);   // 0x10083624
    ambient->setValue(&colour, 0);
    owner->viewProjection->setValue(&viewProjection, 0);
    owner->world->setValue(&transform, 0);
    if (scale) scale->setFloat(textureScale);
    if (texture) owner->textureParameter->setValue(texture, 0);
    const int passes = static_cast<int>(dx9rend::effectBegin(owner->effect));
    for (int pass = 0; pass < passes; ++pass)
    {
        dx9rend::effectBeginPass(owner->effect, static_cast<unsigned int>(pass));
        dx9rend::rendererDrawIndexedPrimitive(owner->faceCount, 4, 0, 0, owner->vertexCount, 0);
    }
    dx9rend::effectEnd(owner->effect);
    if (saveState) dx9rend::stateManagerPop();
}

#if defined(_M_IX86) && defined(_MSC_VER) && _MSC_VER == 1310
// The sizes include VC7.1 STL objects; current MSVC uses its own container layout.
static_assert(sizeof(DebugBoxMesh) == 0xA4, "retail debug box instance layout");
static_assert(sizeof(DebugBoxMeshTemplate) == 0xC0, "retail debug box template layout");
#endif
} // namespace

// PE 0x100838C0; exposed beside the factory for direct template-list users.
void debugBoxMeshRemoveInstance(void* owner, void* instance)
{
    static_cast<DebugBoxMeshTemplate*>(owner)->removeInstance(static_cast<DebugBoxMesh*>(instance));
}

// PE callers dispatch the DebugBoxMesh extensions at +0xF8 and +0x10C.
void debugBoxMeshSetColour(void* geometry, const Math::Vec4& colour)
{
    DebugBoxMesh* self = static_cast<DebugBoxMesh*>(geometry);
    typedef void (__thiscall* Set)(void*, const Math::Vec4&);
    reinterpret_cast<Set>(self->m_vptr[62])(self, colour);
}
void debugBoxMeshSetDimensions(void* geometry, const Vec3& dimensions)
{
    DebugBoxMesh* self = static_cast<DebugBoxMesh*>(geometry);
    typedef void (__thiscall* Set)(void*, const Vec3&);
    reinterpret_cast<Set>(self->m_vptr[67])(self, dimensions);
}

// PE 0x1007C9F0. Like the original factory, allocation failure is not guarded at query.
IBase* __fastcall createDebugBoxMesh(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugBoxMeshTemplate));
    DebugBoxMeshTemplate* instance = storage ? new (storage) DebugBoxMeshTemplate() : 0;
    typedef void* (__thiscall* Query)(void*, unsigned int);
    void* result = reinterpret_cast<Query>(instance->table[3])(instance, iid);
    if (!result) boxTemplate_release(instance RD9_THUNK_ARG);
    return static_cast<IBase*>(result);
}

} } } // namespace dice::hfe::geom
#endif
