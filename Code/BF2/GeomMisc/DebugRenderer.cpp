// Diagnostic geometry pooling, sorting and frustum submission.
// Source placement is attested by the PE's DebugRenderer.cpp diagnostic path.
#ifdef BF2_IMAGE_RENDDX9
#include "DebugRenderer.h"
#include "Debug.h"
#include "../Geom/GeometryInstanceData.h"
#include "../Geom/IGeometryTemplate.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/RendDX9Math.h"       // the RendDX9.dll Math copies
#include "../RendDX9/ShaderParameter.h"
#include "../RendMisc/IRenderView.h"
#include "../RendMisc/IRenderer.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif
#include <cmath>
#include <cstring>
#include <map>
#include <new>
#include <utility>
#include <vector>

namespace dice { namespace hfe { namespace dx9rend {
class Effect;
extern void* g_rendererBase;
extern void* g_shaderManager;
extern void* g_bufferManager;
void* shaderManagerGetEffect(void*, const char*);
ShaderParameter* effectGetShaderParameterBorrowedTyped(Effect*, const std::string&);
int effectRelease(Effect*);
bool effectSetTechniqueByName(void*, const std::string&);
unsigned int effectBegin(void*);
void effectBeginPass(void*, unsigned int);
void effectEnd(void*);
int bufferManagerSwapPool(int);
void* createVertexBuffer(void*, unsigned int, unsigned int, const std::string&, unsigned int);
void vertexBufferRelease(void*);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferBindStream(void*, unsigned int);
void vertexDeclarationRelease(void*);
void vertexDeclarationBind(void*);
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int);
} } }

namespace dice { namespace hfe { namespace geom {
extern IBase* g_DebugTextDrawer;
extern const unsigned int CID_DebugRenderer = 81034u;
namespace {
// PE globals 0x1024B298/0x1024B29C. Repeated construction overwrites these shared owners.
dx9rend::Effect* g_shapeEffect;
void* g_shapeDeclaration;

// All geometry instances use the named native prefix, including the explicit primary table.
GeometryInstanceData* geometryData(void* value) { return static_cast<GeometryInstanceData*>(value); }
void geometryDraw(void* value)
{
    typedef void (__thiscall* Draw)(void*, void*, void*, unsigned char);
    reinterpret_cast<Draw>(geometryData(value)->m_vptr[6])(value, 0, 0, 0);
}
IGeometryTemplate* geometryTemplate(void* value)
{
    typedef IGeometryTemplate* (__thiscall* Get)(void*);
    return reinterpret_cast<Get>(geometryData(value)->m_vptr[14])(value);
}
void geometryTransform(void* value, const Mat4& transform)
{
    typedef void (__thiscall* Set)(void*, const Mat4&);
    reinterpret_cast<Set>(geometryData(value)->m_vptr[20])(value, transform);
}
void geometryTechnique(void* value, const std::string& technique)
{
    typedef void (__thiscall* Set)(void*, const std::string&);
    reinterpret_cast<Set>(geometryData(value)->m_vptr[26])(value, technique);
}
void geometryRelease(void* value) { static_cast<IBase*>(value)->release(); }

// [INFERRED] Holder spelling. Map-node destruction calls release on its non-null value.
struct DebugTemplateReference
{
    DebugTemplateReference() : value(0) {}
    ~DebugTemplateReference() { if (value) value->release(); }
    IGeometryTemplate* value;
};
struct GeometrySortKey { float distance, scale; };
struct GeometrySortLess
{
    // PE 0x1009A780: distance ascending, then scale descending. NaNs retain retail comparisons.
    bool operator()(const GeometrySortKey& a, const GeometrySortKey& b) const
    { return a.distance == b.distance ? a.scale > b.scale : a.distance < b.distance; }
};
struct FrustumBatch
{
    FrustumBatch() {}
    ~FrustumBatch() { if (buffer) dx9rend::vertexBufferRelease(buffer); }
    void* buffer; // PE +0x38/+0x5C/+0x80
    std::string technique;
    unsigned int primitiveCount;
};
struct DebugVertex { Vec3 position; unsigned int colour; };

class DebugRenderer : public IDebugRenderer
{
public:
    DebugRenderer();
    ~DebugRenderer();
    void addRef() { ++refs; } // PE 0x1000CB20
    int getRef() { return refs; } // PE 0x101A9A10
    int release(); // PE 0x1009B110
    void* queryInterface(unsigned int iid) const; // PE 0x1009B0E0
    void flush();
    void queueGeometry(unsigned int classId, const Mat4& transform, const std::string& technique);
    void* acquireGeometry(unsigned int classId);
    void clear();
    IBase* createGraph();
    void* destroy(unsigned int flags);
    void queueFrustum(const Frustum& frustum, unsigned int colour, unsigned char projector);
private:
    IGeometryTemplate* getTemplate(unsigned int classId);
    void* createGeometry(unsigned int classId);
    void drawBatch(FrustumBatch& batch);
    int refs; // PE +0x04
    unsigned int frame; // +0x08
    unsigned int cursor; // +0x0C
    std::vector<void*> pool; // +0x10
    std::map<unsigned int, DebugTemplateReference> templates; // +0x20
    std::multimap<GeometrySortKey, void*, GeometrySortLess> queue; // +0x2C
    FrustumBatch wire, solid, projector; // +0x38/+0x5C/+0x80
};

// PE 0x10099E00. All three technique strings exist before resource creation begins.
DebugRenderer::DebugRenderer() : refs(1), frame(0), cursor(0)
{
    const D3DVERTEXELEMENT9 elements[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        D3DDECL_END()
    };
    const int oldPool = dx9rend::bufferManagerSwapPool(0);
    g_shapeEffect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "Shaders/DebugD3DXMeshShapeShader"));
    g_shapeDeclaration = dx9rend::vertexDeclarationCreate(elements);
    wire.buffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, 3600, 16, "DebugRenderer", 520);
    wire.technique = "wirefrustum"; wire.primitiveCount = 0;
    solid.buffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, 3600, 16, "DebugRenderer", 520);
    solid.technique = "solidfrustum"; solid.primitiveCount = 0;
    projector.buffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, 3600, 16, "DebugRenderer", 520);
    projector.technique = "projectorfrustum"; projector.primitiveCount = 0;
    dx9rend::bufferManagerSwapPool(oldPool);
}

// PE 0x10099C70. Batch members then release their buffers and strings in reverse order.
DebugRenderer::~DebugRenderer()
{
    clear();
    dx9rend::effectRelease(g_shapeEffect);
    dx9rend::vertexDeclarationRelease(g_shapeDeclaration);
}
int DebugRenderer::release()
{
    const int result = --refs;
    if (!result) destroy(1);
    return result;
}
void* DebugRenderer::queryInterface(unsigned int iid) const
{ return iid == 1 || iid == 80034u ? const_cast<DebugRenderer*>(this) : 0; }
// PE 0x1009B130.
void* DebugRenderer::destroy(unsigned int flags)
{
    this->~DebugRenderer();
    if (flags & 1) ::renddx9abi::engineFree(this);
    return this;
}
// PE 0x10098C50.
IBase* DebugRenderer::createGraph() { return classManager->createInstance(81038u, 80039u, 0); }

// PE 0x10099B80. The cache takes the factory reference, retaining the explicit add/release pair.
IGeometryTemplate* DebugRenderer::getTemplate(unsigned int classId)
{
    auto found = templates.find(classId);
    if (found != templates.end()) return found->second.value;
    IGeometryTemplate* value = static_cast<IGeometryTemplate*>(
        classManager->createInstance(classId, 0x17701FFFu, 0));
    if (!value)
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugRenderer.cpp", 366, "")
            << "Could not create geometryTemplate with classID " << classId;
    value->addRef();
    templates[classId].value = value;
    value->release();
    return value;
}
// PE 0x10099DB0. The resource-ready byte is restored only after createInstance returns normally.
void* DebugRenderer::createGeometry(unsigned int classId)
{
    IGeometryTemplate* value = getTemplate(classId);
    const unsigned char ready = dx9rend::resourceManagerIsReady();
    dx9rend::resourceManagerSetReady(0);
    void* geometry = value->createInstance();
    dx9rend::resourceManagerSetReady(ready);
    return geometry;
}
// PE 0x1009A000. Acquisition does not advance the cursor; queueGeometry does.
void* DebugRenderer::acquireGeometry(unsigned int classId)
{
    unsigned int currentFrame;
    // Renderer +0x6C returns its named, fixed-width statistics block; +0x18 is its frame counter.
    const unsigned char* stats = static_cast<const unsigned char*>(
        static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderStats());
    std::memcpy(&currentFrame, stats + 0x18,
                sizeof(currentFrame));
    if (frame != currentFrame) { cursor = 0; frame = currentFrame; }
    if (pool.size() <= cursor)
    {
        void* value = createGeometry(classId);
        pool.push_back(value);
        return value;
    }
    void* value = pool[cursor];
    if (geometryTemplate(value)->getClassId() != classId)
    {
        geometryRelease(value);
        value = createGeometry(classId);
        pool[cursor] = value;
    }
    return value;
}

// PE 0x10099A00. Retains the x store before the unspilled y/z distance expression.
void DebugRenderer::queueGeometry(unsigned int classId, const Mat4& transform, const std::string& technique)
{
    void* geometry = acquireGeometry(classId);
    geometryTransform(geometry, transform);
    if (technique != "") geometryTechnique(geometry, technique);
    const Mat4* camera = static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderViewDup()->getViewMatrixPtr();
    GeometrySortKey key;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Existing native x87 arithmetic policy, with the PE's single stores.
    using namespace bf2x87;
    const float x = store(sub(wide(camera->m[12]), wide(transform.m[12])));
    const Real y = sub(wide(camera->m[13]), wide(transform.m[13]));
    const Real z = sub(wide(camera->m[14]), wide(transform.m[14]));
    const Real distance = squareRoot(add(add(mul(z,z), mul(y,y)), mul(wide(x),wide(x))));
#else
    const float x = camera->m[12] - transform.m[12];
    const float y = camera->m[13] - transform.m[13];
    const float z = camera->m[14] - transform.m[14];
    const float distance = std::sqrt(z*z + y*y + x*x);
#endif
    const float xy = transform.m[0] < transform.m[5] ? transform.m[5] : transform.m[0];
    const float scale = xy < transform.m[10] ? transform.m[10] : xy;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    key.distance = bf2x87::store(distance);
#else
    key.distance = distance;
#endif
    key.scale = scale;
    queue.insert(std::make_pair(key, geometry));
    ++cursor;
}

// PE 0x10099B00. Does not clear queued pointers, cursor or frustum counts.
void DebugRenderer::clear()
{
    const unsigned int count = static_cast<unsigned int>(pool.size());
    for (unsigned int i = 0; i < count; ++i) geometryRelease(pool[i]);
    std::vector<void*>().swap(pool);
    templates.clear();
}

void DebugRenderer::drawBatch(FrustumBatch& batch)
{
    if (!batch.primitiveCount) return;
    dx9rend::vertexDeclarationBind(g_shapeDeclaration);
    dx9rend::vertexBufferBindStream(batch.buffer, 0);
    dx9rend::effectSetTechniqueByName(g_shapeEffect, batch.technique);
    const int passes = static_cast<int>(dx9rend::effectBegin(g_shapeEffect));
    for (int pass = 0; pass < passes; ++pass)
    {
        dx9rend::effectBeginPass(g_shapeEffect, static_cast<unsigned int>(pass));
        dx9rend::rendererDrawPrimitive(batch.primitiveCount, 4, 0);
    }
    dx9rend::effectEnd(g_shapeEffect);
    batch.primitiveCount = 0;
}
// PE 0x10099650. Reverse key order draws far geometry first, then the three frustum batches.
void DebugRenderer::flush()
{
    dx9rend::stateManagerPush();
    for (auto entry = queue.rbegin(); entry != queue.rend(); ++entry) geometryDraw(entry->second);
    queue.clear();
    if (wire.primitiveCount || solid.primitiveCount || projector.primitiveCount)
    {
        dx9rend::IRenderView* view = static_cast<dx9rend::IRenderer*>(dx9rend::g_rendererBase)->getRenderViewDup();
        const Mat4* inverse = view->getInverseViewMatrixPtr();
        const Mat4* projection = view->getProjectionMatrixPtr();
        Mat4 viewProjection = Mat4::getIdentity();
        viewProjection.fullMult(*inverse, *projection);
        dx9rend::ShaderParameter* parameter = dx9rend::effectGetShaderParameterBorrowedTyped(g_shapeEffect, "WorldViewProjection");
        if (parameter) parameter->setValue(&viewProjection, 0);
    }
    drawBatch(wire); drawBatch(solid); drawBatch(projector);
    dx9rend::stateManagerPop();
    if (g_DebugTextDrawer) debugTextDrawerFlush(g_DebugTextDrawer);
}

// PE 0x10098C70. Alpha selects solid/projector triangles; zero alpha selects thick wire triangles.
void DebugRenderer::queueFrustum(const Frustum& frustum, unsigned int colour, unsigned char isProjector)
{
    static const unsigned int faceColours[6] = {
        0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFF00u, 0xFFFF00FFu, 0xFF00FFFFu
    }; // PE 0x1020CBD8
    if (colour & 0xFF000000u)
    {
        FrustumBatch& batch = isProjector ? projector : solid;
        DebugVertex* output = static_cast<DebugVertex*>(dx9rend::vertexBufferLock(
            batch.buffer, 3 * batch.primitiveCount, 36, batch.primitiveCount ? 4096 : 8192));
        const unsigned int cornerOrder[] = {0,1,2,2,3,0};
        for (unsigned int face = 0; face < 6; ++face)
            for (unsigned int corner : cornerOrder)
            {
                output->position = rendmath::frustumCornerForPlane(frustum, corner, face);
                output->colour = colour & 0xFFFFFFu ? colour : (faceColours[face] | (colour & 0xFF000000u));
                ++output;
            }
        dx9rend::vertexBufferUnlock(batch.buffer);
        batch.primitiveCount += 12;
        return;
    }
    DebugVertex* output = static_cast<DebugVertex*>(dx9rend::vertexBufferLock(
        wire.buffer, 3 * wire.primitiveCount, 180, wire.primitiveCount ? 4096 : 8192));
    // Repeated corner calls are intentional. Each strip consists of two triangles, displaced
    // by (0.25,0.25,0.25); the final strip is the plane-normal marker.
    for (unsigned int face = 0; face < 6; ++face)
    {
        const unsigned int vertexColour = colour ? colour : faceColours[face];
        const unsigned int cornerOrder[24] = {0,1,1,1,0,0, 1,2,2,2,1,1,
                                               2,3,3,3,2,2, 3,0,0,0,3,3};
        for (unsigned int i = 0; i < 24; ++i)
        {
            const Vec3& corner = rendmath::frustumCornerForPlane(frustum, cornerOrder[i], face);
            if (i % 6 >= 2 && i % 6 <= 4)
            {
                output->position.x = corner.x + 0.25f;
                output->position.y = corner.y + 0.25f;
                output->position.z = corner.z + 0.25f;
            }
            else output->position = corner;
            output->colour = vertexColour; ++output;
        }
        const Vec3* c1 = &rendmath::frustumCornerForPlane(frustum, 1, face);
        const Vec3* c0 = &rendmath::frustumCornerForPlane(frustum, 0, face);
        Vec3 sum = {c0->x + c1->x, c1->y + c0->y, c1->z + c0->z};
        const Vec3* c2 = &rendmath::frustumCornerForPlane(frustum, 2, face);
        sum.x += c2->x; sum.y += c2->y; sum.z += c2->z;
        const Vec3* c3 = &rendmath::frustumCornerForPlane(frustum, 3, face);
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        // [CHOSEN, not recovered] x/y remain extended across all six marker vertices.
        using namespace bf2x87;
        const Real cx = mul(add(wide(sum.x),wide(c3->x)),wide(0.25f));
        const Real cy = mul(wide(store(add(wide(sum.y),wide(c3->y)))),wide(0.25f));
        const float cz = store(mul(wide(store(add(wide(sum.z),wide(c3->z)))),wide(0.25f)));
        auto marker = [&](bool tip, bool displaced) -> Vec3 {
            Real x = cx, y = cy, z = wide(cz);
            if (tip)
            {
                x = add(mul(wide(frustum.m_planes[face].normal.x),wide(4.0f)),cx);
                const Real py = mul(wide(frustum.m_planes[face].normal.y),wide(4.0f));
                const Real pz = mul(wide(frustum.m_planes[face].normal.z),wide(4.0f));
                if (displaced) { x = wide(store(x)); y = wide(store(add(py,cy))); }
                else y = add(wide(store(py)),cy);
                z = add(wide(store(pz)),wide(cz));
            }
            if (displaced) { x=add(x,wide(0.25f)); y=add(y,wide(0.25f)); z=add(z,wide(0.25f)); }
            return Vec3{store(x),store(y),store(z)};
        };
#else
        const Vec3 center = {(sum.x+c3->x)*0.25f, (sum.y+c3->y)*0.25f, (sum.z+c3->z)*0.25f};
        auto marker = [&](bool tip, bool displaced) -> Vec3 {
            Vec3 value = center;
            if (tip) { value.x += frustum.m_planes[face].normal.x*4; value.y += frustum.m_planes[face].normal.y*4; value.z += frustum.m_planes[face].normal.z*4; }
            if (displaced) { value.x += 0.25f; value.y += 0.25f; value.z += 0.25f; }
            return value;
        };
#endif
        const bool tip[6] = {false,true,true,true,false,false};
        const bool displaced[6] = {false,false,true,true,true,false};
        for (unsigned int i=0;i<6;++i)
        {
            output->position = marker(tip[i],displaced[i]);
            output->colour = vertexColour; ++output;
        }
    }
    dx9rend::vertexBufferUnlock(wire.buffer);
    wire.primitiveCount += 60;
}
} // namespace

// PE 0x1016E986..0x1016EA2D / 0x10174F58..0x10174FFF / 0x10173BA0..0x10173C47.
// [INFERRED] Shared helper factoring of the three identical submission sequences.
void debugRendererQueueBounds(const float* bounds)
{
    void* geometry = static_cast<IDebugRenderer*>(g_DebugRenderer)->acquireGeometry(81012u);
    geometryTechnique(geometry, "bounding");
    Mat4 transform = Mat4::getIdentity();
    transform.m[0] = bounds[3] - bounds[0];
    transform.m[5] = bounds[4] - bounds[1];
    transform.m[10] = bounds[5] - bounds[2];
    // PE 0x1016EBC0 stores the z sum before multiplying each component by one half.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Native extended expression/store policy.
    using namespace bf2x87;
    const float z = store(add(wide(bounds[5]),wide(bounds[2])));
    transform.m[12] = store(mul(add(wide(bounds[3]),wide(bounds[0])),wide(0.5f)));
    transform.m[13] = store(mul(add(wide(bounds[4]),wide(bounds[1])),wide(0.5f)));
    transform.m[14] = store(mul(wide(z),wide(0.5f)));
#else
    const float z = bounds[5] + bounds[2];
    transform.m[12] = (bounds[3] + bounds[0]) * 0.5f;
    transform.m[13] = (bounds[4] + bounds[1]) * 0.5f;
    transform.m[14] = z * 0.5f;
#endif
    static_cast<IDebugRenderer*>(g_DebugRenderer)->queueGeometry(81012u, transform, "");
}

// PE 0x1007CDB0.
IBase* __fastcall createDebugRenderer(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(DebugRenderer));
    DebugRenderer* self = storage ? new (storage) DebugRenderer() : 0;
    void* result = self->queryInterface(iid);
    if (!result) self->release();
    return static_cast<IBase*>(result);
}
} } }
#endif
