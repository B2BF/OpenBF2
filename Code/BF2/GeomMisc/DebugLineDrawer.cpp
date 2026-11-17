// Source path attested by the renderer's DebugLineDrawer.cpp diagnostic.
#ifdef BF2_IMAGE_RENDDX9

#include "DebugLineDrawer.h"
#include "../RendDX9/RendDX9Math.h"   // rendmath::normalize (sub_100777A0)
#include "Debug.h"
#include "../Math/Mat4.h"
#include "../RendMisc/IRenderView.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <d3dx9.h>
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h"
#endif
#include <cfloat>
#include <cmath>
#include <cstring>
#include <new>
#include <string>

namespace dice { namespace hfe {
namespace dx9rend {
extern void* g_rendererBase;
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
extern const unsigned int CID_DebugLineDrawer = 81021u;
namespace {

struct LineVertex { Vec3 position; unsigned int colour; };
static_assert(sizeof(LineVertex) == 16, "debug line vertex stride");

// PE 0x1020C798, also materialized locally by drawTriangles.
const unsigned int lineColours[] = {
    0xFFCF0000u, 0xFF00CF00u, 0xFF0000CFu, 0xFF00CFCFu,
    0xFF3F0000u, 0xFF003F00u, 0xFF00003Fu, 0xFF003F3Fu
};

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] Keep the PE's extended intermediates on native processors.
using LineReal = bf2x87::Real;
LineReal real(float v) { return bf2x87::wide(v); }
float lineStore(LineReal v) { return bf2x87::store(v); }
#else
using LineReal = float;
float real(float v) { return v; }
float lineStore(float v) { return v; }
#endif

// Shared by drawRibbon and drawLineList. Width is an XZ-plane
// half-width; the x/y products stay extended while the z product is stored.
void writeRibbon(LineVertex*& out, const D3DXVECTOR3& a, const D3DXVECTOR3& b,
                 float width, unsigned int colour)
{
    const float dx = lineStore(real(b.x) - real(a.x));
    Vec3 normal = {lineStore(real(b.z) - real(a.z)), 0.0f, -dx};
    rendmath::normalize(normal);
    const LineReal nx = real(normal.x) * real(width);
    const LineReal ny = real(normal.y) * real(width);
    const float nz = lineStore(real(normal.z) * real(width));
    const auto minus = [&](const D3DXVECTOR3& p) -> Vec3 {
        return {lineStore(real(p.x) - nx), lineStore(real(p.y) - ny),
                lineStore(real(p.z) - real(nz))};
    };
    const auto plus = [&](const D3DXVECTOR3& p) -> Vec3 {
        return {lineStore(nx + real(p.x)), lineStore(ny + real(p.y)),
                lineStore(real(nz) + real(p.z))};
    };
    *out++ = {minus(a), colour};
    *out++ = {plus(a), colour};
    *out++ = {minus(b), colour};
    *out++ = {plus(a), colour};
    *out++ = {plus(b), colour};
    *out++ = {minus(b), colour};
}

// Matrix tail shared by the D3DX and buffered draws.
// The projection accessor precedes the inverse-view accessor.
Mat4 lineMatrix()
{
    dx9rend::IRenderView* view = dx9rend::rendererGetRenderViewDup();
    Mat4 result = Mat4::getIdentity();
    const Mat4* projection = view->getProjectionMatrixPtr();
    const Mat4* inverse = view->getInverseViewMatrixPtr();
    result.fullMult(*inverse, *projection);
    return result;
}

// Typed matrix carrier for the D3DX draw overloads.
D3DXMATRIX d3dxLineMatrix()
{
    const Mat4 matrix = lineMatrix();
    D3DXMATRIX result;
    static_assert(sizeof(result) == sizeof(matrix), "four-by-four float matrices");
    std::memcpy(&result, &matrix, sizeof(result));
    return result;
}

// Palette/clamp selection shared by drawTriangles and drawLineList.
unsigned int lineColour(const std::vector<unsigned int>& colours, unsigned int index)
{
    if (colours.empty()) return lineColours[index & 7u];
    if (index >= colours.size()) index = static_cast<unsigned int>(colours.size()) - 1u;
    return colours[index];
}
}

class DebugLineDrawer : public IDebugLineDrawer, public dx9rend::DeviceLostResetListener
{
public:
    DebugLineDrawer();
    ~DebugLineDrawer();
    void addRef() override { ++m_refs; } // PE 0x1001A010
    int getRef() override { return m_refs; } // PE 0x10051870
    int release() override;
    void* queryInterface(unsigned int iid) const override;
    void drawD3dxLine(const std::vector<D3DXVECTOR3>&, unsigned int, float) override;
    void drawD3dxLine(unsigned int, const D3DXVECTOR3*, unsigned int, float) override;
    void drawD3dxLine(float, float, float, float, float, float, unsigned int, float) override;
    void drawThinLine(float, float, float, float, float, float, unsigned int, unsigned int) override;
    void drawRibbon(const std::vector<D3DXVECTOR3>&, unsigned int, float, bool) override;
    void drawLine(const std::vector<Vec3>&, unsigned int, float, bool) override;
    void drawTriangle(const Vec3&, const Vec3&, const Vec3&, unsigned int, bool) override;
    void drawTriangles(const std::vector<Vec3>&, const std::vector<unsigned int>&, bool) override;
    void drawMesh(Vec3*, unsigned int*, int, unsigned int*, bool) override;
    void drawLineList(const std::vector<Vec3>&, const std::vector<unsigned int>&, float, bool) override;
    void* destroy(unsigned int flags) override;
    void drawScreenLine(float, float, float, float, unsigned int, float) override;
    void onDeviceLost() override;
    void onDeviceReset() override;
    void onDeviceDestroy() override;

private:
    void initialize();
    void close();
    void prepare(const char* technique);
    void submit(unsigned int count, unsigned int primitive);
    unsigned int reserveBytes(unsigned int vertexCount);
    int m_refs; // PE +0x08, after the two vptrs
    ID3DXLine* m_line; // +0x0C
    unsigned int m_stride; // +0x10
    unsigned int m_capacity; // +0x14
    unsigned int m_bufferBytes; // +0x18
    unsigned int m_cursor; // +0x1C; bookkeeping does not affect lock/draw offsets
    std::string m_shaderName; // +0x20
    void* m_buffer; // +0x3C
    dx9rend::Effect* m_effect; // +0x40
    dx9rend::ShaderParameter* m_worldViewProjection; // +0x44
    dx9rend::ShaderParameter* m_zbuffer; // +0x48
};

// PE 0x10094740.
DebugLineDrawer::DebugLineDrawer()
    : m_refs(1), m_line(0), m_stride(0), m_capacity(65536), m_cursor(0),
      m_shaderName("DebugCircleShader"), m_buffer(0), m_effect(0),
      m_worldViewProjection(0), m_zbuffer(0)
{
    initialize();
    dx9rend::registerDeviceLostResetListener(this, 1);
}

// PE 0x10093C30. D3DXCreateLine's result is discarded, as shipped.
void DebugLineDrawer::initialize()
{
    D3DXCreateLine(static_cast<dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->device, &m_line);
    m_stride = 16;
    const int pool = dx9rend::bufferManagerSwapPool(0);
    m_bufferBytes = m_capacity * m_stride;
    m_buffer = dx9rend::createVertexBuffer(dx9rend::g_bufferManager, m_capacity,
                                           m_stride, "DebugLineDrawer", 520);
    dx9rend::bufferManagerSwapPool(pool);
    m_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, (std::string("Shaders/") + m_shaderName).c_str()));
    m_worldViewProjection = dx9rend::effectGetShaderParameterBorrowedTyped(m_effect, "WorldViewProjection");
    if (m_worldViewProjection) m_worldViewProjection->addRef();
    else
        ::renddx9abi::Debug(DebugTypeError,
            "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\GeomMisc\\DebugLineDrawer.cpp",
            92, "") << "Could not load WorldViewProjection parameter from shader";
    m_zbuffer = dx9rend::effectGetShaderParameterBorrowedTyped(m_effect, "ZBUFFER");
    if (m_zbuffer) m_zbuffer->addRef();
}

// PE 0x10093DD0. Released member addresses are retained.
void DebugLineDrawer::close()
{
    if (m_line) m_line->Release();
    if (m_buffer) dx9rend::vertexBufferRelease(m_buffer);
    if (m_worldViewProjection) m_worldViewProjection->release();
    if (m_zbuffer) m_zbuffer->release();
    if (m_effect) dx9rend::effectRelease(m_effect);
}

// PE 0x100947B0.
DebugLineDrawer::~DebugLineDrawer()
{
    if (dx9rend::g_rendererBase) dx9rend::unregisterDeviceLostResetListener(this);
    close();
}

// PE 0x10095620. Queries borrow the existing reference.
void* DebugLineDrawer::queryInterface(unsigned int iid) const
{ return iid == 1u || iid == 80013u ? const_cast<DebugLineDrawer*>(this) : 0; }

// PE 0x10095650.
int DebugLineDrawer::release()
{
    const int result = --m_refs;
    if (!result) destroy(1);
    return result;
}

// PE 0x10095780.
void* DebugLineDrawer::destroy(unsigned int flags)
{
    this->~DebugLineDrawer();
    if (flags & 1u) ::renddx9abi::engineFree(this);
    return this;
}

// PE 0x10093BF0 / secondary table 0x1020C74C.
void DebugLineDrawer::onDeviceLost() { if (m_line) m_line->OnLostDevice(); }
// PE 0x10093C00.
void DebugLineDrawer::onDeviceReset() { if (m_line) m_line->OnResetDevice(); }
// PE 0x10093C10.
void DebugLineDrawer::onDeviceDestroy()
{ if (m_line) { m_line->Release(); m_line = 0; } }

// PE 0x10093BA0. No Begin/End is inserted around the shipped D3DX calls.
void DebugLineDrawer::drawScreenLine(float x0, float y0, float x1, float y1,
                                    unsigned int colour, float width)
{
    const D3DXVECTOR2 points[] = {{x0, y0}, {x1, y1}};
    m_line->SetWidth(width);
    m_line->Draw(points, 2, colour);
}

// PE 0x10093E40.
void DebugLineDrawer::drawD3dxLine(float x0, float y0, float z0, float x1, float y1,
                                 float z1, unsigned int colour, float width)
{
    const D3DXVECTOR3 points[] = {{x0, y0, z0}, {x1, y1, z1}};
    m_line->SetWidth(width);
    const D3DXMATRIX matrix = d3dxLineMatrix();
    m_line->DrawTransform(points, 2, &matrix, colour);
}

// PE 0x100940B0. Count precedes the pointer in this drawer's interface.
void DebugLineDrawer::drawD3dxLine(unsigned int count, const D3DXVECTOR3* points,
                                 unsigned int colour, float width)
{
    m_line->SetWidth(width);
    const D3DXMATRIX matrix = d3dxLineMatrix();
    m_line->DrawTransform(points, count, &matrix, colour);
}

// PE 0x10094D30.
void DebugLineDrawer::drawD3dxLine(const std::vector<D3DXVECTOR3>& points,
                                 unsigned int colour, float width)
{
    m_line->SetWidth(width);
    const D3DXMATRIX matrix = d3dxLineMatrix();
    // shipped defect: drops the final vertex and underflows for an empty vector.
    m_line->DrawTransform(points.data(), static_cast<unsigned int>(points.size()) - 1u, &matrix, colour);
}

// Shared preparation tail of the buffered draws.
void DebugLineDrawer::prepare(const char* technique)
{
    dx9rend::effectSetTechniqueByName(m_effect, std::string(technique));
    const Mat4 matrix = lineMatrix();
    m_worldViewProjection->setValue(&matrix, 0);
}

// Shared submission tail of the buffered draws.
void DebugLineDrawer::submit(unsigned int count, unsigned int primitive)
{
    dx9rend::vertexBufferBindStream(m_buffer, 0);
    dx9rend::stateManagerPush();
    dx9rend::effectBegin(m_effect);
    dx9rend::effectBeginPass(m_effect, 0);
    dx9rend::rendererDrawPrimitive(count, primitive, 0);
    dx9rend::effectEnd(m_effect);
    dx9rend::stateManagerPop();
}

// Cursor bookkeeping shared by the buffered triangle and ribbon draws.
unsigned int DebugLineDrawer::reserveBytes(unsigned int vertexCount)
{
    const unsigned int bytes = m_stride * vertexCount;
    if (m_cursor > m_bufferBytes - bytes) m_cursor = 0;
    return bytes;
}

// PE 0x10093F30. Drawing continues after a failed lock.
void DebugLineDrawer::drawThinLine(float x0, float y0, float z0, float x1, float y1,
                                  float z1, unsigned int colour, unsigned int depthValue)
{
    LineVertex* out = static_cast<LineVertex*>(dx9rend::vertexBufferLock(m_buffer, 0, 2, 8192));
    if (out)
    {
        out[0] = {{x0, y0, z0}, colour};
        out[1] = {{x1, y1, z1}, colour};
        dx9rend::vertexBufferUnlock(m_buffer);
    }
    prepare("t0");
    m_zbuffer->setValue(&depthValue, 0);
    submit(1, 3);
}

// PE 0x10094180.
void DebugLineDrawer::drawTriangle(const Vec3& a, const Vec3& b, const Vec3& c,
                                  unsigned int colour, bool depth)
{
    const unsigned int bytes = reserveBytes(3);
    LineVertex* out = static_cast<LineVertex*>(dx9rend::vertexBufferLock(m_buffer, 0, 3, 8192));
    if (!out) return;
    out[0] = {a, colour}; out[1] = {b, colour}; out[2] = {c, colour};
    dx9rend::vertexBufferUnlock(m_buffer);
    prepare(depth ? "t0_usezbuffer" : "t0");
    submit(1, 4);
    m_cursor += bytes;
}

// PE 0x10094340. No range checks on indices, colours or the signed count.
void DebugLineDrawer::drawMesh(Vec3* points, unsigned int* indices, int triangleCount,
                              unsigned int* colours, bool depth)
{
    if (!triangleCount) return;
    const unsigned int bytes = reserveBytes(static_cast<unsigned int>(triangleCount) * 3u);
    LineVertex* out = static_cast<LineVertex*>(dx9rend::vertexBufferLock(
        m_buffer, 0, static_cast<unsigned int>(triangleCount) * 3u, 8192));
    if (!out) return;
    for (int i = 0; i < triangleCount; ++i)
        for (int j = 0; j < 3; ++j)
            *out++ = {points[indices[i * 3 + j]], colours[i]};
    dx9rend::vertexBufferUnlock(m_buffer);
    prepare(depth ? "t0_usezbuffer" : "t0");
    submit(static_cast<unsigned int>(triangleCount), 4);
    m_cursor += bytes;
}

// PE 0x100947F0. The depth argument is ignored; t0 is always selected.
void DebugLineDrawer::drawRibbon(const std::vector<D3DXVECTOR3>& points,
                                unsigned int colour, float width, bool)
{
    if (points.empty()) return;
    std::vector<D3DXVECTOR3>::const_iterator next = points.begin() + 1;
    if (next == points.end()) next = points.begin();
    const unsigned int bytes = reserveBytes(static_cast<unsigned int>(points.size()));
    // shipped defect: the lock and cursor account for input vertices, although each
    // segment writes six vertices. Both lock and draw still start at zero.
    LineVertex* out = static_cast<LineVertex*>(dx9rend::vertexBufferLock(
        m_buffer, 0, static_cast<unsigned int>(points.size()), 8192));
    if (!out) return;
    D3DXVECTOR3 previous = points.front();
    if (points.size() == 1)
    {
        previous = D3DXVECTOR3(lineStore(real(next->x) + real(1.0f)),
                              lineStore(real(next->y) + real(1.0f)),
                              lineStore(real(next->z) + real(1.0f)));
        writeRibbon(out, previous, *next, width, colour);
        previous = *next;
    }
    for (; next != points.end(); ++next)
    {
        writeRibbon(out, previous, *next, width, colour);
        previous = *next;
    }
    dx9rend::vertexBufferUnlock(m_buffer);
    prepare("t0");
    submit(static_cast<unsigned int>(points.size()) * 2u - 2u, 4);
    m_cursor += bytes;
}

// PE 0x10094E20.
void DebugLineDrawer::drawTriangles(const std::vector<Vec3>& points,
                                   const std::vector<unsigned int>& colours, bool depth)
{
    if (points.empty() || points.size() % 3u) return;
    const unsigned int bytes = reserveBytes(static_cast<unsigned int>(points.size()));
    LineVertex* out = static_cast<LineVertex*>(dx9rend::vertexBufferLock(
        m_buffer, 0, static_cast<unsigned int>(points.size()), 8192));
    if (!out) return;
    for (unsigned int i = 0; i < points.size(); ++i)
        out[i] = {points[i], lineColour(colours, i / 3u)};
    dx9rend::vertexBufferUnlock(m_buffer);
    prepare(depth ? "t0_usezbuffer" : "t0");
    submit(static_cast<unsigned int>(points.size()) / 3u, 4);
    m_cursor += bytes;
}

// PE 0x10095110.
void DebugLineDrawer::drawLineList(const std::vector<Vec3>& points,
                                  const std::vector<unsigned int>& colours, float width, bool depth)
{
    if (points.empty() || (points.size() & 1u)) return;
    std::vector<Vec3>::const_iterator next = points.begin() + 1;
    const unsigned int bytes = reserveBytes(static_cast<unsigned int>(points.size()));
    // shipped defect: locks only the input count, then writes three times as many vertices.
    LineVertex* out = static_cast<LineVertex*>(dx9rend::vertexBufferLock(
        m_buffer, 0, static_cast<unsigned int>(points.size()), 8192));
    if (!out) return;
    Vec3 previous = points.front();
    int outputVertices = 0;
    while (next != points.end())
    {
        // shipped defect: the colour index advances by two for each line.
        const unsigned int colour = lineColour(colours, static_cast<unsigned int>(outputVertices / 3));
        writeRibbon(out, D3DXVECTOR3(previous.x, previous.y, previous.z),
                    D3DXVECTOR3(next->x, next->y, next->z), width, colour);
        outputVertices += 6;
        ++next;
        if (next != points.end()) previous = *next++;
    }
    dx9rend::vertexBufferUnlock(m_buffer);
    prepare(depth ? "t0_usezbuffer" : "t0");
    submit(static_cast<unsigned int>(points.size()), 4);
    m_cursor += bytes;
}

// PE 0x10095500. Preserve the source-level vector conversion and virtual dispatch.
void DebugLineDrawer::drawLine(const std::vector<Vec3>& points, unsigned int colour,
                              float width, bool depth)
{
    std::vector<D3DXVECTOR3> converted;
    for (unsigned int i = 0; i < points.size(); ++i)
        converted.push_back(D3DXVECTOR3(points[i].x, points[i].y, points[i].z));
    drawRibbon(converted, colour, width, depth);
}

// PE 0x1007CBD0. Allocation failure still reaches the unguarded query.
IBase* __fastcall createDebugLineDrawer(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* memory = ::renddx9abi::engineAlloc(sizeof(DebugLineDrawer));
    DebugLineDrawer* self = memory ? new (memory) DebugLineDrawer : 0;
    IBase* result = static_cast<IBase*>(self->queryInterface(iid));
    if (!result) self->release();
    return result;
}

} } }
#endif
