
// Geometry, placement and visibility for the eight surrounding terrain patches.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/PluginAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/HostFileAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include <string>
#include "../../RendDX9Abi/D3dx9Sdk.h"
#include "QuadPatch.h"
#include "Terrain.h"
#include "../Math/Frustum.h"
#include "../Math/Mat4.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../RendMisc/IRenderView.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace dice {
namespace hfe {

namespace dx9rend {
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);                                          // --ref, sub_10014510, free
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  rendererDrawIndexedPrimitive(unsigned int primCount, unsigned int type, int baseVertex,
                                   unsigned int minIndex, unsigned int numVertices,
                                   unsigned int startIndex);
} // namespace dx9rend

namespace geom {

namespace {

// sub_10074AB0 with one integer argument - see QuadPatch.cpp.
void formatString(std::string& out, const char* format, int a)
{
    char buffer[0x1000];
    const int written = std::snprintf(buffer, 0xFFF, format, a);
    if (written < 0)
        std::snprintf(buffer, sizeof(buffer), "%s",
                      "WARNING: The original message was too long for formatString(). \n");
    else
        buffer[written] = 0;
    out = buffer;
}

// sub_10079460 - a 4x4 from four rows, copied in order.
void matrixFromRows(float* m, const float* row0, const float* row1, const float* row2, const float* row3)
{
    for (int i = 0; i < 4; ++i)
    {
        m[i]      = row0[i];
        m[4 + i]  = row1[i];
        m[8 + i]  = row2[i];
        m[12 + i] = row3[i];
    }
}

} // anonymous namespace

SurroundingPatch::SurroundingPatch(Terrain* owner)
{
    terrain = owner;
    xzBytes = 0;
    lowDetailmap = 0;
    colormap = 0;
    xzVertexBuffer = 0;
    heightVertexBuffer = 0;
    originX = 0;
    originZ = 0;
    center.x = 0.0f; center.y = 0.0f; center.z = 0.0f;
    originXf = 0.0f; zero5C = 0.0f; originZf = 0.0f; zero64 = 0.0f;
    fileMaxY = 0.0f;
    fileMinY = 0.0f;
    boundsCenter.x = 0.0f; boundsCenter.y = 0.0f; boundsCenter.z = 0.0f;
    boundsRadius = 0.0f;
    slot = 0;
    for (int i = 0; i < 16; ++i) placement[i] = 0.0f;
    placement[15] = 1.0f; placement[10] = 1.0f; placement[5] = 1.0f; placement[0] = 1.0f;
    bounds.min.x = 0.0f; bounds.min.y = 0.0f; bounds.min.z = 0.0f;
    bounds.max.x = 0.0f; bounds.max.y = 0.0f; bounds.max.z = 0.0f;
    bounds.cornerLength = 0.0f; bounds.radius = 0.0f;
    bounds.dirty = 1;
    bounds.extent.x = 0.0f; bounds.extent.y = 0.0f; bounds.extent.z = 0.0f;
    bounds.centre.x = 0.0f; bounds.centre.y = 0.0f; bounds.centre.z = 0.0f;
    for (int i = 0; i < 24; ++i) corners[i] = 0.0f;
}

// Releases colormap, low-detail map, then both vertex buffers without clearing their pointers.
// The load path already freed xzBytes; do not free the dangling pointer again.
SurroundingPatch::~SurroundingPatch()
{
    terrainReleaseTexture(colormap);                                              // +0x18
    terrainReleaseTexture(lowDetailmap);                                          // +0x14
    if (xzVertexBuffer)     dx9rend::vertexBufferRelease(xzVertexBuffer);         // +0x1C
    if (heightVertexBuffer) dx9rend::vertexBufferRelease(heightVertexBuffer);     // +0x20
}

// four file fields, the placement, then the packed xz bytes and the height vectors
// into two vertex buffers, and the two per-slot textures.
void SurroundingPatch::load(int slotIndex, TerrainRawReader& reader)
{
    slot = slotIndex;
    reader.readU32(&fileMaxY);
    reader.readU32(&fileMinY);
    reader.readVec3(&boundsCenter);
    reader.readU32(&boundsRadius);
    setupPlacement();

    Terrain* const t = terrain;
    // +0xFC + 4 * isCorner: the side grid's vertex count for a side slot, the corner grid's for a
    // corner slot.
    const unsigned int vertexCount = isCorner ? t->m_cornerVertexCount : t->m_sideVertexCount;
    const unsigned int xzBytesSize = 4u * vertexCount;
    xzBytes = ::renddx9abi::engineAlloc(xzBytesSize);
    reader.read(xzBytes, xzBytesSize);

    xzVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(vertexCount, 4u, 8u, "SurroundingPatch");
    ::memcpy(dx9rend::vertexBufferLock(xzVertexBuffer, 0, 0, 0), xzBytes, xzBytesSize);   // unguarded, as shipped
    dx9rend::vertexBufferUnlock(xzVertexBuffer);

    heightVertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(
        vertexCount, t->m_halfFloatSurrounding ? 8u : 12u, 8u, "SurroundingPatch");
    unsigned char* dst = static_cast<unsigned char*>(dx9rend::vertexBufferLock(heightVertexBuffer, 0, 0, 0));
    const unsigned int heightBytes = 12u * vertexCount;
    void* const storage = ::renddx9abi::engineAlloc(heightBytes);
    float* heights = 0;
    if (storage)
    {
        heights = static_cast<float*>(storage);
        for (unsigned int i = 0; i < vertexCount; ++i)
        {
            heights[3 * i] = 0.0f; heights[3 * i + 1] = 0.0f; heights[3 * i + 2] = 0.0f;
        }
    }
    reader.read(heights, heightBytes);
    if (t->m_halfFloatSurrounding)
    {
        // Four halves per vertex: x, y, z and a zero.
        const float* src = heights;
        for (unsigned int i = 0; i < vertexCount; ++i, src += 3)
        {
            float x = src[0];
            float y = src[1];
            float z = src[2];
            float w = 0.0f;
            D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(dst),     &x, 1);
            D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(dst + 2), &y, 1);
            D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(dst + 4), &z, 1);
            D3DXFloat32To16Array(reinterpret_cast<D3DXFLOAT16*>(dst + 6), &w, 1);
            dst += 8;
        }
    }
    else
    {
        // Three floats per vertex, copied (the shipped loop is unrolled by four).
        const float* src = heights;
        float* out = reinterpret_cast<float*>(dst);
        for (unsigned int i = 0; i < vertexCount; ++i, src += 3, out += 3)
        {
            out[0] = src[0];
            out[1] = src[1];
            out[2] = src[2];
        }
    }
    ::renddx9abi::engineFree(heights);
    dx9rend::vertexBufferUnlock(heightVertexBuffer);

    std::string suffix;
    formatString(suffix, "_s%i", slot);
    {
        std::string name(t->m_colormapBaseName.c_str());
        name += suffix;
        colormap = dx9rend::textureManagerGetTexture(name);
    }
    {
        std::string name(t->m_lowDetailmapBaseName.c_str());
        name += suffix;
        lowDetailmap = dx9rend::textureManagerGetTexture(name);
    }
}

// the placement of slot 0..7: the origin in world units, the 4x4 the draw applies
// (two scaled axis rows, the z row, the translation row), the corner flag, the centre and bounds.
// `h` is the ring span, `s` the secondary scale; integer division truncates as the shipped idiv.
void SurroundingPatch::setupPlacement()
{
    Terrain* const t = terrain;
    secondaryScaleX = static_cast<int>(t->m_secondaryWorldScale.x);
    span = static_cast<int>(t->m_primaryWorldScale.x) * (t->m_heightmapSizeX - 1);
    spanPlusOne = span + 1;
    sideCount = span / secondaryScaleX + 1;
    sideCountSquared = sideCount * sideCount;

    const int   h  = span;
    const float sf = static_cast<float>(secondaryScaleX);
    float row0[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float row1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const float row2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
    float row3[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    switch (slot)
    {
    case 0:
        originX = -h - h / 2;
        originZ = h - h / 2;
        row0[1] = sf;  row1[0] = -sf;
        row3[0] = static_cast<float>(originX + h); row3[1] = static_cast<float>(originZ);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 1;
        break;
    case 1:
        originX = -(h / 2);
        originZ = h - h / 2;
        row0[0] = sf;  row1[1] = sf;
        row3[0] = static_cast<float>(originX); row3[1] = static_cast<float>(originZ);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 0;
        break;
    case 2:
        originX = h - h / 2;
        originZ = h - h / 2;
        row0[0] = sf;  row1[1] = sf;
        row3[0] = static_cast<float>(originX); row3[1] = static_cast<float>(originZ);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 1;
        break;
    case 3:
        originX = h - h / 2;
        originZ = -(h / 2);
        row0[1] = -sf; row1[0] = sf;
        row3[0] = static_cast<float>(originX); row3[1] = static_cast<float>(originZ + h);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 0;
        break;
    case 4:
        originX = h - h / 2;
        originZ = -h - h / 2;
        row0[1] = -sf; row1[0] = sf;
        row3[0] = static_cast<float>(originX); row3[1] = static_cast<float>(originZ + h);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 1;
        break;
    case 5:
        originX = -(h / 2);
        originZ = -h - h / 2;
        row0[0] = -sf; row1[1] = -sf;
        row3[0] = static_cast<float>(originX + h); row3[1] = static_cast<float>(originZ + h);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 0;
        break;
    case 6:
        originX = -h - h / 2;
        originZ = -h - h / 2;
        row0[0] = -sf; row1[1] = -sf;
        row3[0] = static_cast<float>(originX + h); row3[1] = static_cast<float>(originZ + h);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 1;
        break;
    case 7:
        originX = -h - h / 2;
        originZ = -(h / 2);
        row0[1] = sf;  row1[0] = -sf;
        row3[0] = static_cast<float>(originX + h); row3[1] = static_cast<float>(originZ);
        matrixFromRows(placement, row0, row1, row2, row3);
        isCorner = 0;
        break;
    default:
        // sub_10074300(6, __FILE__, 156, "") << the message - dice's debug stream, kept as one line.
        ::renddx9abi::logf("[SurroundingPatch.cpp:156] TBM: unknown surrounding terrain patch type!");
        break;
    }

    // The origin as (x, 0, z, 0), the centre half a span in from it (lea ecx, [esi+58h]).
    zero5C = 0.0f;
    zero64 = 0.0f;
    originXf = static_cast<float>(originX);
    originZf = static_cast<float>(originZ);
    const Vec3* const origin = terrainOriginScratch(*reinterpret_cast<const Vec3*>(&originXf));
    center.z = origin->z;
    center.y = origin->y;
    center.x = origin->x;
    center.x = static_cast<float>(static_cast<double>(span) * 0.5 + center.x);
    center.z = static_cast<float>(static_cast<double>(span) * 0.5 + center.z);

    endX = originX + span;
    endZ = originZ + span;
    const float x0 = static_cast<float>(originX);
    const float z0 = static_cast<float>(originZ);
    const float x1 = static_cast<float>(endX);
    const float z1 = static_cast<float>(endZ);
    const float dx = x1 - x0;
    const float dy = fileMaxY - fileMinY;
    const float dz = z1 - z0;
    boundsCenter.z = static_cast<float>(dz * 0.5 + z0);
    boundsCenter.x = static_cast<float>(dx * 0.5 + x0);
    boundsCenter.y = static_cast<float>(dy * 0.5 + fileMinY);
    boundsRadius = static_cast<float>(
        std::sqrt(static_cast<double>(dz) * dz + static_cast<double>(dy) * dy + static_cast<double>(dx) * dx) * 0.5);
    bounds.min.z = z0;
    bounds.min.x = x0;
    bounds.dirty = 1;
    bounds.min.y = fileMinY;
    bounds.max.x = x1;
    bounds.max.y = fileMaxY;
    bounds.max.z = z1;
    bounds.dirty = 1;
    aabbCorners(corners, &bounds.min.x);                                           // 0x1018EF77 on +0xCC
}

namespace {

// sub_1018E570 - do the xz segments P (a1,a2)-(a3,a4) and Q (a5,a6)-(a7,a8) cross? The shipped
// answer keeps its two degenerate arms: a zero denominator is "no", and a parameter that is
// exactly 0 or exactly the denominator is "yes" without the range test. Doubles, as computed.
bool segmentsCross(float a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8)
{
    const double v8 = (static_cast<double>(a8) - a6) * a1;
    const float v16 = a4 - a2;
    const double v15 = (static_cast<double>(a2) - a4) * a5 + (static_cast<double>(a6) - a8) * a3
                     + static_cast<double>(v16) * a7 + v8;
    if (v15 == 0.0)
        return false;
    const double v9 = v8 + (static_cast<double>(a6) - a2) * a7 + (static_cast<double>(a2) - a8) * a5;
    if (v9 != 0.0 && v9 != v15)
    {
        const double v14 = 1.0 / v15;
        const double v10 = v9 * v14;
        const double v11 = -((static_cast<double>(a6) - a4) * a1 + (static_cast<double>(a2) - a6) * a3
                             + static_cast<double>(v16) * a5);
        if (v11 != 0.0 && v11 != v15)
        {
            const double v12 = v11 * v14;
            return v12 >= 0.0 && v12 <= 1.0 && v10 >= 0.0 && v10 <= 1.0;
        }
    }
    return true;
}

// sub_1018E6A0 - does the xz segment cross one of the six frustum edges (corners 0-4, 2-6, 1-5,
// 3-7, 5-7, 4-6 of Frustum::m_corners)?
bool crossesFrustumEdge(float x0, float z0, float x1, float z1, const Frustum* frustum)
{
    const float* const f = reinterpret_cast<const float*>(frustum);
    return segmentsCross(x0, z0, x1, z1, f[24], f[26], f[36], f[38])
        || segmentsCross(x0, z0, x1, z1, f[30], f[32], f[42], f[44])
        || segmentsCross(x0, z0, x1, z1, f[27], f[29], f[39], f[41])
        || segmentsCross(x0, z0, x1, z1, f[33], f[35], f[45], f[47])
        || segmentsCross(x0, z0, x1, z1, f[39], f[41], f[45], f[47])
        || segmentsCross(x0, z0, x1, z1, f[36], f[38], f[42], f[44]);
}

} // anonymous namespace

// visible when the camera's xz lies inside the patch; invisible when the patch lies
// beyond the far plane of the camera on any side; else the slot's two or three outer edges are
// tested against the frustum's edges (both operands of each `|` are evaluated, as shipped).
bool SurroundingPatch::isVisible(int)
{
    dx9rend::IRenderView* const view = dx9rend::rendererGetRenderView();                    // base +0x48
    const Frustum* const frustum = view->getFrustumPtr();                          // +0x74
    const float* const m = view->getViewMatrixPtr()->m;                            // +0x64
    const float camX = m[12];
    const float camZ = m[14];
    if (static_cast<double>(originX) <= camX && static_cast<double>(endX) >= camX
        && static_cast<double>(originZ) <= camZ && static_cast<double>(endZ) >= camZ)
        return true;
    const float farPlane = view->getFarPlane();                                         // +0x3C
    const float x0 = static_cast<float>(originX);
    if (x0 - farPlane > camX) return false;
    const float x1 = static_cast<float>(endX);
    if (x1 + farPlane < camX) return false;
    const float z0 = static_cast<float>(originZ);
    if (z0 - farPlane > camZ) return false;
    const float z1 = static_cast<float>(endZ);
    if (z1 + farPlane < camZ) return false;
    unsigned char hit = 0;
    switch (slot)
    {
    case 0:
        hit = crossesFrustumEdge(x1, z0, x1, z1, frustum) ? 1 : 0;
        return ((crossesFrustumEdge(x0, z0, x1, z0, frustum) ? 1 : 0) | hit) != 0;
    case 1:
        hit = crossesFrustumEdge(x1, z0, x1, z1, frustum) ? 1 : 0;
        hit = crossesFrustumEdge(x0, z0, x1, z0, frustum) | hit;
        return ((crossesFrustumEdge(x0, z0, x0, z1, frustum) ? 1 : 0) | hit) != 0;
    case 2:
        hit = crossesFrustumEdge(x0, z0, x1, z0, frustum) ? 1 : 0;
        return ((crossesFrustumEdge(x0, z0, x0, z1, frustum) ? 1 : 0) | hit) != 0;
    case 3:
        hit = crossesFrustumEdge(x0, z0, x0, z1, frustum) ? 1 : 0;
        hit = crossesFrustumEdge(x0, z0, x1, z0, frustum) | hit;
        return ((crossesFrustumEdge(x0, z1, x1, z1, frustum) ? 1 : 0) | hit) != 0;
    case 4:
        hit = crossesFrustumEdge(x0, z0, x0, z1, frustum) ? 1 : 0;
        return ((crossesFrustumEdge(x0, z1, x1, z1, frustum) ? 1 : 0) | hit) != 0;
    case 5:
        hit = crossesFrustumEdge(x0, z0, x0, z1, frustum) ? 1 : 0;
        hit = crossesFrustumEdge(x0, z1, x1, z1, frustum) | hit;
        return ((crossesFrustumEdge(x1, z0, x1, z1, frustum) ? 1 : 0) | hit) != 0;
    case 6:
        hit = crossesFrustumEdge(x0, z1, x1, z1, frustum) ? 1 : 0;
        return ((crossesFrustumEdge(x1, z0, x1, z1, frustum) ? 1 : 0) | hit) != 0;
    case 7:
        hit = crossesFrustumEdge(x0, z0, x1, z0, frustum) ? 1 : 0;
        hit = crossesFrustumEdge(x0, z1, x1, z1, frustum) | hit;
        return ((crossesFrustumEdge(x1, z0, x1, z1, frustum) ? 1 : 0) | hit) != 0;
    default:
        return false;
    }
}

// when visible: TEXLAYER0 = the colormap, TEXLAYER5 = the low-detail map, sttransxz =
// the placement, the xz stream on 1 and the height stream on 3, pass 0, one indexed draw of the
// corner or side grid.
void SurroundingPatch::draw(int)
{
    if (!isVisible(0))
        return;
    Terrain* const t = terrain;
    static_cast<dx9rend::ShaderParameter*>(t->m_hTexLayer[0])->setValue(colormap, 0);
    static_cast<dx9rend::ShaderParameter*>(t->m_hTexLayer[5])->setValue(lowDetailmap, 0);
    static_cast<dx9rend::ShaderParameter*>(t->m_hStTransXZ)->setValue(placement, 0);
    dx9rend::vertexBufferBindStream(xzVertexBuffer, 1);
    dx9rend::vertexBufferBindStream(heightVertexBuffer, 3);
    const unsigned int vertexCount = isCorner ? t->m_cornerVertexCount : t->m_sideVertexCount;   // +0xFC + 4 * isCorner
    const unsigned int primCount = (isCorner ? t->m_cornerIndexCount : t->m_sideIndexCount) / 3;  // +0xF4 + 4 * isCorner
    dx9rend::effectBeginPass(t->m_effect, 0u);
    dx9rend::rendererDrawIndexedPrimitive(primCount, 4u, 0, 0u, vertexCount, 0u);   // sub_10005820(prims, verts, 4, 0, 0, 0)
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
