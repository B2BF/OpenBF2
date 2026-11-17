// Source path attested by the renderer's DebugCircleDrawer.cpp diagnostic.
#ifdef BF2_IMAGE_RENDDX9

#include "DebugCircleDrawer.h"
#include "../RendDX9/RendDX9Math.h"   // rendmath::normalize (sub_100777A0)
#include "Debug.h"
#include "../Math/Mat4.h"
#include "../Physics/HeightmapCluster.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/PluginEntry.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif
#include <cfloat>
#include <cmath>
#include <new>
#include <string>

namespace dice { namespace hfe {
namespace dx9rend {
extern void* g_bufferManager;
extern void* g_shaderManager;
int bufferManagerSwapPool(int);
void* createVertexBuffer(void*, unsigned int, unsigned int, const std::string&, unsigned int);
void* vertexBufferLock(void*, unsigned int, unsigned int, unsigned int);
bool vertexBufferUnlock(void*);
void vertexBufferBindStream(void*, unsigned int);
void vertexBufferRelease(void*);
void rendererDrawPrimitive(unsigned int, unsigned int, unsigned int);
}
namespace geom {
extern const unsigned int CID_DebugCircleDrawer = 81022u;
namespace {

// PE 0x1024B294. Borrowed, lazily cached, and never cleared at level teardown.
world::IHeightmapCluster* s_circleHeightmapCluster;

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Keep the PE's x87 intermediates on both native processors.
using CircleReal = bf2x87::Real;
CircleReal real(float value) { return bf2x87::wide(value); }
CircleReal real(unsigned int value) { return bf2x87::wide(value); }
float circleStore(CircleReal value) { return bf2x87::store(value); }
CircleReal circleCosine(CircleReal value) { return bf2x87::cosValue(value); }
CircleReal circleSine(CircleReal value) { return bf2x87::sinValue(value); }
#else
using CircleReal = float;
float real(float value) { return value; }
float real(unsigned int value) { return static_cast<float>(value); }
float circleStore(float value) { return value; }
float circleCosine(float value) { return std::cos(value); }
float circleSine(float value) { return std::sin(value); }
#endif

Vec3 circleCross(const Vec3& a, const Vec3& b)
{
    return {circleStore(real(a.y) * real(b.z) - real(a.z) * real(b.y)),
            circleStore(real(a.z) * real(b.x) - real(a.x) * real(b.z)),
            circleStore(real(a.x) * real(b.y) - real(a.y) * real(b.x))};
}

struct CircleVertex { Vec3 position; unsigned int colour; };
static_assert(sizeof(CircleVertex) == 16, "debug circle vertex stride");

} // namespace

class DebugCircleDrawer : public IDebugCircleDrawer
{
public:
    DebugCircleDrawer();
    ~DebugCircleDrawer();
    void addRef() { ++m_refs; } // PE 0x1000CB20
    int getRef() { return m_refs; } // PE 0x101A9A10
    int release();
    void* queryInterface(unsigned int iid) const;
    void drawCircle(const Vec3&, const Vec3&, float, unsigned int, bool, unsigned int);
    void drawArc(const Vec3&, const Vec3&, const Vec3&, float, unsigned int, bool,
                 unsigned int, float, float);
    void* destroy(unsigned int flags);

private:
    void initialize();
    void close();
    void submit(unsigned int vertexCount);
    void writeVertices(const Vec3& first, const Vec3& second, const Vec3& centre,
                       float radius, unsigned int colour, bool terrain,
                       unsigned int vertexCount, bool arc,
                       float startDegrees, float sweepDegrees);
    int m_refs; // PE +0x04
    unsigned int m_capacity; // +0x08
    unsigned int m_stride; // +0x0C
    unsigned char m_unknown10; // [UNRESOLVED] initialized to zero; neither draw reads it.
    std::string m_shaderName; // +0x14
    void* m_buffer; // +0x30
    dx9rend::Effect* m_effect; // +0x34
    dx9rend::ShaderParameter* m_worldViewProjection; // +0x38
    dx9rend::ShaderParameter* m_zbuffer; // +0x3C
};

// PE 0x10098B60.
DebugCircleDrawer::DebugCircleDrawer()
    : m_refs(1), m_capacity(4096), m_stride(0), m_unknown10(0),
      m_shaderName("DebugCircleShader"), m_buffer(0), m_effect(0),
      m_worldViewProjection(0), m_zbuffer(0)
{
    initialize();
}

// PE 0x100981F0.
void DebugCircleDrawer::initialize()
{
    m_stride = 16;
    const int oldPool = dx9rend::bufferManagerSwapPool(0);
    m_buffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, m_capacity,
                                           m_stride, "DebugCircleDrawer", 520);
    dx9rend::bufferManagerSwapPool(oldPool);
    m_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, (std::string("Shaders/") + m_shaderName).c_str()));
    m_worldViewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(
        m_effect, "WorldViewProjection");
    if (m_worldViewProjection)
        m_worldViewProjection->addRef();
    else
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugCircleDrawer.cpp",
            78, "") << "Could not load WorldViewProjection parameter from shader";
    m_zbuffer = dx9rend::effectGetShaderParameterBorrowedTyped(m_effect, "ZBUFFER");
    if (m_zbuffer) m_zbuffer->addRef();
    if (!s_circleHeightmapCluster)
        s_circleHeightmapCluster = static_cast<world::IHeightmapCluster*>(
            classManager->getSingleton(std::string("HeightmapCluster")));
}

// PE 0x100983B0. Members retain their released addresses, as shipped.
void DebugCircleDrawer::close()
{
    if (m_buffer) dx9rend::vertexBufferRelease(m_buffer);
    if (m_worldViewProjection) m_worldViewProjection->release();
    if (m_zbuffer) m_zbuffer->release();
    if (m_effect) dx9rend::effectRelease(m_effect);
}

// PE 0x10098BB0.
DebugCircleDrawer::~DebugCircleDrawer() { close(); }

// PE 0x10098BD0. Interface queries borrow the existing reference.
void* DebugCircleDrawer::queryInterface(unsigned int iid) const
{ return iid == 1u || iid == 80014u ? const_cast<DebugCircleDrawer*>(this) : 0; }

// PE 0x10098C00.
int DebugCircleDrawer::release()
{
    const int result = --m_refs;
    if (!result) destroy(1);
    return result;
}

// PE 0x10098C20.
void* DebugCircleDrawer::destroy(unsigned int flags)
{
    this->~DebugCircleDrawer();
    if (flags & 1u) ::renddx9abi::engineFree(this);
    return this;
}

// Common vertex-writing portion of drawCircle and drawArc.
void DebugCircleDrawer::writeVertices(const Vec3& first, const Vec3& second,
    const Vec3& centre, float radius, unsigned int colour, bool terrain,
    unsigned int vertexCount, bool arc, float startDegrees, float sweepDegrees)
{
    CircleVertex* vertices = static_cast<CircleVertex*>(
        dx9rend::vertexBufferLock(m_buffer, 0, vertexCount, 8192));
    if (!vertices) return;

    // shipped defect: zero/one/oversized counts are not rejected. The terrain arc
    // also ignores its start/sweep angles and writes a complete circle.
    if (!vertexCount)
    {
        dx9rend::vertexBufferUnlock(m_buffer);
        return;
    }
    const CircleReal denominator = terrain ? real(circleStore(real(vertexCount - 1u)))
                                           : real(vertexCount - 1u);
    CircleReal start = real(0.0f), sweep = real(0.0f);
    if (arc && !terrain)
    {
        sweep = real(sweepDegrees) * real(1.0f / 360.0f);
        start = real(startDegrees) * real(1.0f / 360.0f);
    }
    for (unsigned int i = 0; i < vertexCount; ++i)
    {
        const CircleReal angle = arc && !terrain
            ? ((real(i) / denominator) * sweep + start) * real(6.283184051513671875f)
            : (real(i) * real(6.283184051513671875f)) / denominator;
        const CircleReal c = circleCosine(angle) * real(radius);
        const CircleReal s = circleSine(angle) * real(radius);
        if (terrain)
        {
            const float sx = circleStore(real(second.x) * s);
            const float sz = circleStore(real(second.z) * s);
            const float cx = circleStore(real(first.x) * c);
            const CircleReal xBase = real(cx) + real(centre.x);
            const float zBase = circleStore(real(first.z) * c + real(centre.z));
            const float x = circleStore(xBase + real(sx));
            const float z = circleStore(real(zBase) + real(sz));
            vertices[i].position.y = s_circleHeightmapCluster->getHeightInWorldCoords(x, z);
            vertices[i].position.x = x;
            vertices[i].position.z = z;
        }
        else
        {
            const float sx = circleStore(real(second.x) * s);
            const float sy = circleStore(real(second.y) * s);
            const float sz = circleStore(real(second.z) * s);
            const float cx = circleStore(real(first.x) * c);
            const float cy = circleStore(real(first.y) * c);
            const float xBase = circleStore(real(cx) + real(centre.x));
            const float yBase = circleStore(real(cy) + real(centre.y));
            const CircleReal zBase = real(first.z) * c + real(centre.z);
            const CircleReal x = real(xBase) + real(sx);
            const CircleReal y = real(yBase) + real(sy);
            vertices[i].position.z = circleStore(zBase + real(sz));
            vertices[i].position.x = circleStore(x);
            vertices[i].position.y = circleStore(y);
        }
        vertices[i].colour = colour;
    }
    dx9rend::vertexBufferUnlock(m_buffer);
}

// Common drawCircle/drawArc tail, including a failed-lock draw.
void DebugCircleDrawer::submit(unsigned int vertexCount)
{
    dx9rend::effectSetTechniqueByName(m_effect, std::string("t0"));
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 worldViewProjection = Mat4::getIdentity();
    const Mat4* projection = view->getProjectionMatrixPtr();
    const Mat4* inverse = view->getInverseViewMatrixPtr();
    worldViewProjection.fullMult(*inverse, *projection);
    m_worldViewProjection->setValue(&worldViewProjection, 0);
    dx9rend::vertexBufferBindStream(m_buffer, 0);
    dx9rend::stateManagerPush();
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    dx9rend::rendererDrawPrimitive(vertexCount - 1u, 3, 0);
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();
}

// PE 0x10098410.
void DebugCircleDrawer::drawCircle(const Vec3& normal, const Vec3& centre, float radius,
    unsigned int colour, bool terrain, unsigned int vertexCount)
{
    Vec3 n = normal;
    rendmath::normalize(n);
    const Vec3 reference = {-n.y, -n.z, -n.x};
    Vec3 first = circleCross(n, reference);
    rendmath::normalize(first);
    Vec3 second = circleCross(n, first);
    rendmath::normalize(second);
    writeVertices(first, second, centre, radius, colour, terrain, vertexCount, false, 0, 0);
    submit(vertexCount);
}

// PE 0x100987B0.
void DebugCircleDrawer::drawArc(const Vec3& normal, const Vec3& reference,
    const Vec3& centre, float radius, unsigned int colour, bool terrain,
    unsigned int vertexCount, float startDegrees, float sweepDegrees)
{
    Vec3 n = normal;
    rendmath::normalize(n);
    Vec3 first = circleCross(n, reference);
    rendmath::normalize(first);
    Vec3 second = circleCross(n, first);
    rendmath::normalize(second);
    writeVertices(first, second, centre, radius, colour, terrain, vertexCount, true,
                  startDegrees, sweepDegrees);
    submit(vertexCount);
}

// PE 0x1007CD60. Allocation failure still reaches the unguarded interface query.
IBase* __fastcall createDebugCircleDrawer(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* memory = ::renddx9abi::engineAlloc(sizeof(DebugCircleDrawer));
    DebugCircleDrawer* self = memory ? new (memory) DebugCircleDrawer : 0;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result) self->release();
    return result;
}

} } }
#endif
