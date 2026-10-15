// Loads, generates and selects vegetation instances for camera cells.
// [CHOSEN, not recovered] split from OverGrowth.cpp. Record/field names are [INFERRED].
// Game instances come from level objects; procedural generation requires editorEnable.
// Preserve float evaluation order and 24-bit precision.
// [FRONTIER] RaStreamDataEditable device hooks remain unavailable.
// [RETIRE-WHEN: overgrowth-stream-device-reset] Implement the resource-tracker/device hooks
// and verify managed-stream lifecycle through device loss/reset before removing the stops.
#ifdef BF2_IMAGE_RENDDX9
#include "OverGrowth.h"
#include "OverGrowthCell.h"
#include "../MeshTemplate.h"                        // MeshBounds, meshBoundsCopy / meshBoundsExpand
#include "../Terrain.h"                             // terrainFrustumIntersectsBox (sub_101C7980)
#include "../../Math/Frustum.h"
#include "../../Math/Mat4.h"
#include "../../RendMisc/IRenderView.h"
#include "../../RendDX9/Ra/RaShader.h"              // getShader, the object block, RaVertexElementDesc
#include "../../RendDX9/Ra/RaStreamDataEditable.h"  // the cell's two vertex streams
#include "../../RendDX9/Texture.h"                  // EngineTexture / textureDtor (sub_10014E20)
#include "../../../RendDX9Abi/BfMemory.h"
#include <string>

// The host interfaces, typed in both flavours (RW-266): every slot called below sits at the retail PE
// offset in this tree's MSVC tables in both the native and the retail-ABI build (probe\probe_calls.cmd:
// getHeightInWorldCoords(float,float) +0x6C, getNormalInWorldCoords(float,float,Vec3&) +0x7C, IHeightmap
// getScale +0x54, IObjectTemplate getCullRadiusScale +0x70 and getAnchorPos +0xAC) and none passes an STL
// type. The retail-ABI flavour borrows HfeAbi's IBase, exactly as XpLightManager.cpp does.
#include "../../Game/Objects/IObjectTemplate.h"
#include "../../Physics/HeightmapCluster.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>
#include <vector>

namespace dice { namespace hfe { namespace dx9rend {
extern void*  g_OverGrowth;     // BF2Render.cpp - dword_10258F38, the level OverGrowth
extern IBase* g_ObjectDrawer;   // BF2Render.cpp - dword_1025A39C
extern void*  g_renderQueues;   // RenderQueues.cpp - dword_1023E408, the Ra shader table
} } }

namespace dice { namespace hfe { namespace geom {


// The vegetation records (OverGrowth.h / OverGrowthCell.h): instance 0x60, patch 0x98 and cell 0x2AC
// bytes in the PE. Instance transforms, spacing and generation flags feed patch coverage and cell
// batching.


namespace {

// A raw __thiscall slot on one of RendDX9's OWN objects (geometry instances, the texture's shared
// table, the ObjectDrawer's hand-built table) - the house `dispatch` from BundledMesh.cpp.
template<class R, class... A> R dispatch(void* self, unsigned int offset, A... args)
{
    typedef R(__thiscall* Fn)(void*, A...);
    return reinterpret_cast<Fn>((*static_cast<void***>(self))[offset / 4])(self, args...);
}

// [TEMP] [CHOSEN, not recovered] Known-signature boundaries always report, flush and stop
// (OverGrowth.cpp's house style); the stream's device-reset pair below is the only user.
__declspec(noreturn) void unresolvedAt(const char* qualified)
{
    std::fprintf(stderr, "[FRONTIER] %s reached\n", qualified);
    std::fflush(stderr);
    std::abort();
}

// The PE's allocations and strides: the cell sub_1005F320(0x2AC) at 0x100CD6DE, the stream
// sub_1005F320(0x3C8) at 0x100D0ECE / 0x100D0F2B, instances /0x60 (`imul 2AAAAAABh; sar edx, 4`),
// geometry pairs *0xA0, materials /0x30, material types /0x5C.

// flt_1020CF68 (0x33800000) = 2^-24 and flt_102193D8 (0x37B40000) = 360 / 2^24.
const float kRandomUnit      = 5.9604644775390625e-08f;
const float kDegreesPerCount = 2.1457672119140625e-05f;


// byte_10260610 - .bss, ONE reader (0x1015F119) and neither a writer nor an address-of anywhere in the
// image, so it is false for the whole run; byte_1023A041 / byte_1023A040 - .data, file-backed 1, one
// reader each (0x1015F2D8 / 0x1015F30D), no writer. Kept as mutable globals so each test is the
// independent load the PE performs (BF2Render.cpp's rule for byte_10238249). [INFERRED] names.

} // anonymous namespace
unsigned char g_overGrowthBuildEveryCell     = 0;   // byte_10260610
unsigned char g_overGrowthCellDistanceCull   = 1;   // byte_1023A041
unsigned char g_overGrowthCellFrustumCull    = 1;   // byte_1023A040
namespace {



// Local placement math helpers.


// sub_1000ED40 (__stdcall, retn 0Ch): `fcomi high; fcmovnb` then `fcomi low; fcmovb`. The high bound
// is applied first; a NaN comes back as `low`.
float clampFloat(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// sub_100777A0 - normalise in place unless |v|^2 is within FLT_EPSILON (flt_10208958) of 1; below
// FLT_EPSILON it zeroes and answers -20001. |v|^2 is ((x*x + y*y) + z*z).
int normalizeVec3(Vec3& v)
{
    const float lengthSquared = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSquared - 1.0f) < 1.1920928955078125e-07f)
        return 0;
    if (std::fabs(lengthSquared) < 1.1920928955078125e-07f)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const float scale = 1.0f / std::sqrt(lengthSquared);
    v.x = scale * v.x;
    v.y = scale * v.y;
    v.z = scale * v.z;
    return 0;
}

// sub_10077E30 (retn 4) - the Y rotation: identity, then m0 = cos, m2 = -sin, m8 = sin, m10 = cos of
// flt_10238738 (0x3C8EFA35, pi/180 in .data, never written) times the angle, through x87 fsincos on the
// float-rounded radians (0x10077E36..0x10077E92).
void setRotationY(Mat4& matrix, float degrees)
{
    const float radians = 0.01745329238474369f * degrees;
    float sine;
    float cosine;
#if defined(_M_IX86)
    __asm
    {
        fld radians
        fsincos
        fstp cosine
        fstp sine
    }
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates). The float radians widened,
    // then each result rounded once to float, as the two `fstp dword` stores do; the last bit can
    // differ from fsincos's 80-bit results, and fsincos's out-of-range case (|radians| >= 2^63, C2
    // set, operand left unchanged) is not reproduced.
    cosine = static_cast<float>(std::cos(static_cast<double>(radians)));
    sine   = static_cast<float>(std::sin(static_cast<double>(radians)));
#endif
    for (unsigned int i = 0; i < 16; ++i)
        matrix.m[i] = 0.0f;
    matrix.m[0] = 1.0f;
    matrix.m[5] = 1.0f;
    matrix.m[10] = 1.0f;
    matrix.m[15] = 1.0f;
    matrix.m[0] = cosine;
    matrix.m[2] = -sine;
    matrix.m[8] = sine;
    matrix.m[10] = cosine;
}

// sub_100C8820 - re-orthonormalise a basis keeping its y row: normalise rows 1 and 2, row0 = row1 x
// row2, row2 = row0 x row1 (from the new row0). Operand order as the fmul pairs read.
void orthonormalizeKeepingY(Mat4& matrix)
{
    Vec3& row0 = *reinterpret_cast<Vec3*>(&matrix.m[0]);
    Vec3& row1 = *reinterpret_cast<Vec3*>(&matrix.m[4]);
    Vec3& row2 = *reinterpret_cast<Vec3*>(&matrix.m[8]);
    normalizeVec3(row1);
    normalizeVec3(row2);
    row0.x = row2.z * row1.y - row2.y * row1.z;
    row0.y = row1.z * row2.x - row2.z * row1.x;
    row0.z = row2.y * row1.x - row1.y * row2.x;
    row2.x = row1.z * row0.y - row1.y * row0.z;
    row2.y = row1.x * row0.z - row1.z * row0.x;
    row2.z = row1.y * row0.x - row1.x * row0.y;
}

// lcgrand_step, sub_1009EF30 - RendDX9's copy of dice::hfe::Random::getRaw: two Schrage steps of the
// Law-Kelton lcgrand (24112 = 0x5E30, then 26143 = 0x661F) over the owner's +0x18. Written in unsigned
// arithmetic with the shipped `sar`s (NametagManager.cpp's randomStep, repeated TU-locally).
unsigned int randomStep(unsigned int value, unsigned int multiplier)
{
    const unsigned int low = (value & 0xFFFFu) * multiplier;
    const unsigned int high = static_cast<unsigned int>(static_cast<int>(value) >> 16) * multiplier;
    const int middle = static_cast<int>((low >> 16) + high);
    const unsigned int next = (low & 0xFFFFu) + ((static_cast<unsigned int>(middle) & 0x7FFFu) << 16)
        + static_cast<unsigned int>(middle >> 15);
    const int reduced = static_cast<int>(next - 0x7FFFFFFFu);
    return reduced < 0 ? next : static_cast<unsigned int>(reduced);
}

// Every draw in this half is `lcgrand_step(this + 0x18); or al, 80h; sar eax, 7; add eax, 1` - a count
// in [1, 2^24] (0x100C9995, 0x1015E895, 0x100CCA00).
int random24(OverGrowth* self)
{
    unsigned int& seed = self->m_randomSeed;
    seed = randomStep(randomStep(seed, 24112u), 26143u);
    return (static_cast<int>(seed | 0x80u) >> 7) + 1;
}

world::IHeightmapCluster* heightmapCluster(OverGrowth* self)
{ return self->m_cluster; }                                              // initialize: "HeightmapCluster"


// sub_101C5C50 (retn 8) - does the sphere (point, radius) reach the box? The squared distance from the
// point to the box is summed per axis ((x + y) + z; an axis inside contributes nothing) and the answer
// is `distance <= radius * radius` (test ah, 41h / jp - a NaN answers false).
bool boundsSphereIntersects(const MeshBounds& bounds, const Vec3& point, float radius)
{
    float distance = 0.0f;
    if (point.x < bounds.min.x)
    {
        const float t = point.x - bounds.min.x;
        distance = t * t;
    }
    else if (point.x > bounds.max.x)
    {
        const float t = point.x - bounds.max.x;
        distance = t * t;
    }
    if (point.y < bounds.min.y)
    {
        const float t = point.y - bounds.min.y;
        distance = distance + t * t;
    }
    else if (point.y > bounds.max.y)
    {
        const float t = point.y - bounds.max.y;
        distance = distance + t * t;
    }
    if (point.z < bounds.min.z)
    {
        const float t = point.z - bounds.min.z;
        distance = distance + t * t;
    }
    else if (point.z > bounds.max.z)
    {
        const float t = point.z - bounds.max.z;
        distance = distance + t * t;
    }
    return distance <= radius * radius;
}

// The 96-byte eight-corner box: sub_100CE120 (retn 8) zeroes the 24 floats and then calls
// sub_101C6750 (min/max to corners, in bit order x = 4, y = 2, z = 1) and sub_101C67F0 (every corner
// through the full matrix). sub_101C67F0 associates corner 0's x and y sums differently from corners
// 1-7; both orders are kept (BundledMeshDraw.cpp's worldHeight reads the same body the same way).
struct OverGrowthBoxCorners
{
    float corner[8][3];
};

void constructBoxCorners(OverGrowthBoxCorners& box, const MeshBounds& bounds, const Mat4& world)
{
    for (unsigned int i = 0; i < 8; ++i)
        for (unsigned int j = 0; j < 3; ++j)
            box.corner[i][j] = 0.0f;
    for (unsigned int i = 0; i < 8; ++i)
    {
        box.corner[i][0] = (i & 4) ? bounds.max.x : bounds.min.x;
        box.corner[i][1] = (i & 2) ? bounds.max.y : bounds.min.y;
        box.corner[i][2] = (i & 1) ? bounds.max.z : bounds.min.z;
    }
    const float* const m = world.m;
    for (unsigned int i = 0; i < 8; ++i)
    {
        float* const c = box.corner[i];
        const float x = c[0];
        const float y = c[1];
        const float z = c[2];
        float tx;
        float ty;
        float tz;
        if (i == 0)
        {
            tz = m[2] * x + z * m[10] + y * m[6] + m[14];
            ty = m[1] * x + y * m[5] + z * m[9] + m[13];
            tx = m[8] * z + m[0] * x + y * m[4] + m[12];
        }
        else
        {
            tz = z * m[10] + x * m[2] + y * m[6] + m[14];
            ty = x * m[1] + z * m[9] + y * m[5] + m[13];
            tx = m[4] * y + z * m[8] + x * m[0] + m[12];
        }
        c[0] = tx;
        c[1] = ty;
        c[2] = tz;
    }
}

// sub_101C6E10 - the component-wise minimum: `if (current > corner) current = corner` (test ah, 41h).
Vec3 boxCornersMin(const OverGrowthBoxCorners& box)
{
    Vec3 out;
    out.x = box.corner[0][0];
    out.y = box.corner[0][1];
    out.z = box.corner[0][2];
    for (unsigned int i = 1; i < 8; ++i)
    {
        if (out.x > box.corner[i][0]) out.x = box.corner[i][0];
        if (out.y > box.corner[i][1]) out.y = box.corner[i][1];
        if (out.z > box.corner[i][2]) out.z = box.corner[i][2];
    }
    return out;
}

// sub_101C6C70 - the component-wise maximum: `if (current < corner) current = corner` (test ah, 5).
Vec3 boxCornersMax(const OverGrowthBoxCorners& box)
{
    Vec3 out;
    out.x = box.corner[0][0];
    out.y = box.corner[0][1];
    out.z = box.corner[0][2];
    for (unsigned int i = 1; i < 8; ++i)
    {
        if (out.x < box.corner[i][0]) out.x = box.corner[i][0];
        if (out.y < box.corner[i][1]) out.y = box.corner[i][1];
        if (out.z < box.corner[i][2]) out.z = box.corner[i][2];
    }
    return out;
}

// sub_101C5450 (retn 4) - grow the MeshBounds by the corners' minimum, then their maximum.
void boundsExpandByCorners(MeshBounds& bounds, const OverGrowthBoxCorners& box)
{
    meshBoundsExpand(bounds, boxCornersMin(box));
    meshBoundsExpand(bounds, boxCornersMax(box));
}

// The geometry instance slots this half reads (RendDX9's own StaticMesh table; StaticMesh.h names them):
// +0x38 getTemplate, +0x40 getLocalBounds (min/max at +0 / +0xC), +0x44 getBoundingRadius.
void* geometryTemplate(void* geometry) { return dispatch<void*>(geometry, 0x38); }
const MeshBounds* geometryLocalBounds(void* geometry)
{ return dispatch<const MeshBounds*>(geometry, 0x40); }
float geometryBoundingRadius(void* geometry) { return dispatch<float>(geometry, 0x44); }


// Density and shadow maps share the stored width and height.


// sub_100C88F0 (retn 18h) - world (x, z) to shadow/density map coordinates through the primary
// heightmap's scale (+0xA8, IHeightmap::getScale called twice, as shipped). The integer part is the
// floor, re-centred by width/2 (unsigned shr); the fraction goes out separately.
// The clamp allows index == width (`cmp; ja` then `test; jge`): an index one past the row is not
// rejected here. Preserved.
void shadowCoords(OverGrowth* self, float x, float z, int* ix, int* iz, float* fx, float* fz)
{
    world::IHeightmap* const heightmap = self->m_heightmap;
    const float scaledX = x / heightmap->getScale().x;               // vslot +0x54, 0x100C88FC
    const float scaledZ = z / heightmap->getScale().z;               // vslot +0x54 again, 0x100C8911
    const float floorX = static_cast<float>(std::floor(static_cast<double>(scaledX)));
    const float floorZ = static_cast<float>(std::floor(static_cast<double>(scaledZ)));
    *fx = scaledX - floorX;
    *fz = scaledZ - floorZ;
    *ix = static_cast<int>(floorX);
    *iz = static_cast<int>(floorZ);
    *ix = static_cast<int>(static_cast<unsigned int>(*ix) + (self->m_mapWidth >> 1));
    *iz = static_cast<int>(static_cast<unsigned int>(*iz) + (self->m_mapHeight >> 1));
    if (static_cast<unsigned int>(*ix) > self->m_mapWidth || *ix < 0)
        *ix = 0;
    if (static_cast<unsigned int>(*iz) > self->m_mapHeight || *iz < 0)
        *iz = 0;
}

// sub_100C92F0 (retn 8) - the bilinear shadow-map value at (x, z): four taps (byte * 1/255,
// flt_101E2B98 = 0x3B808081; a tap outside the map is 0), blended ((s10 - s00) * fx + s00) and
// ((s11 - s01) * fx + s01), then ((b - a) * fz + a).
float sampleShadow(OverGrowth* self, float x, float z)
{
    int ix;
    int iz;
    float fx;
    float fz;
    shadowCoords(self, x, z, &ix, &iz, &fx, &fz);
    const float k = 0.003921568859368563f;
    const unsigned char* const shadow = self->m_shadowMap;
    const unsigned int width = self->m_mapWidth;
    const unsigned int height = self->m_mapHeight;
    const int ix1 = ix + 1;
    const int iz1 = iz + 1;
    float s00 = 0.0f;
    if (ix >= 0 && static_cast<unsigned int>(ix) < width && iz >= 0 && static_cast<unsigned int>(iz) < height)
        s00 = static_cast<float>(shadow[width * iz + ix]) * k;
    float s10 = 0.0f;
    if (ix1 >= 0 && static_cast<unsigned int>(ix1) < width && iz >= 0 && static_cast<unsigned int>(iz) < height)
        s10 = static_cast<float>(shadow[width * iz + ix + 1]) * k;
    float s01 = 0.0f;
    if (ix >= 0 && static_cast<unsigned int>(ix) < width && iz1 >= 0 && static_cast<unsigned int>(iz1) < height)
        s01 = static_cast<float>(shadow[width * iz1 + ix]) * k;
    float s11 = 0.0f;
    if (ix1 >= 0 && static_cast<unsigned int>(ix1) < width && iz1 >= 0 && static_cast<unsigned int>(iz1) < height)
        s11 = static_cast<float>(shadow[width * iz1 + ix + 1]) * k;
    const float a = (s10 - s00) * fx + s00;
    const float b = (s11 - s01) * fx + s01;
    return (b - a) * fz + a;
}

// sub_100C9AE0 (retn 4) - one patch's density histogram: every density texel from the patch corner's
// map cell to the far corner's (inclusive, rows then columns), then coverage[i] = count[i] / texels for
// the first sixteen values. The two ends come from shadowCoords, so a patch touching the map's far edge
// reads texel index == width (the next row's first byte, or one past the buffer on the last row) - the
// shipped clamp's off-by-one, preserved.
void computeCoverage(OverGrowth* self, OverGrowthPatch* patch)
{
    const float x0 = patch->position.x;
    const float z0 = patch->position.z;
    const float x1 = self->m_cellSize + patch->position.x;
    const float z1 = self->m_cellSize + patch->position.z;
    int ix0;
    int iz0;
    int ix1;
    int iz1;
    float fx;
    float fz;
    shadowCoords(self, x0, z0, &ix0, &iz0, &fx, &fz);
    shadowCoords(self, x1, z1, &ix1, &iz1, &fx, &fz);
    unsigned int counts[256];
    for (unsigned int i = 0; i < 256; ++i)
        counts[i] = 0;
    const unsigned char* const density = self->m_densityMap;
    for (int iz = iz0; iz <= iz1; ++iz)
        for (int ix = ix0; ix <= ix1; ++ix)
            ++counts[density[self->m_mapWidth * iz + ix]];
    const int texels = (iz1 - iz0 + 1) * (ix1 - ix0 + 1);
    const float total = static_cast<float>(static_cast<unsigned int>(texels));
    for (unsigned int i = 0; i < 16; ++i)
        patch->coverage[i] = static_cast<float>(counts[i]) / total;
}

// sub_1015EB20 (retn 4) - the material whose density value (+0x1C) is `value`, or null.
OverGrowthMaterial* findMaterial(OverGrowth* self, int value)
{
    std::vector<OverGrowthMaterial>& materials = self->m_materials;
    for (unsigned int i = 0; i < materials.size(); ++i)
    {
        OverGrowthMaterial* const material = &materials[i];
        if (static_cast<int>(material->id) == value)
            return material;
    }
    return 0;
}

// sub_100C88C0 (retn 8) - patches[z * patchCountX + x].
OverGrowthPatch* patchAt(OverGrowth* self, int x, int z)
{ return self->m_patches + (self->m_patchCountX * z + x); }

// sub_1015E880 (retn 8) - a uniformly random point of the patch's cell square, dropped onto the terrain;
// answers the density value there (vslot +0x18). Two draws: x first, then z; the cell size is re-read
// between them. Each offset is ((float)count * cellSize) * 2^-24.
unsigned int pickPosition(OverGrowth* self, const OverGrowthPatch* patch, Vec3* out)
{
    const float cellSizeX = self->m_cellSize;
    const float offsetX = static_cast<float>(random24(self)) * cellSizeX * kRandomUnit;
    const float cellSizeZ = self->m_cellSize;
    const float offsetZ = static_cast<float>(random24(self)) * cellSizeZ * kRandomUnit;
    out->x = offsetX + patch->position.x;
    out->y = patch->position.y;
    out->z = offsetZ + patch->position.z;
    out->y = heightmapCluster(self)->getHeightInWorldCoords(out->x, out->z);   // vslot +0x6C
    return self->sampleDensity(out->x, out->z);                      // vslot +0x18, 0x1015E919
}

// sub_1015E730 (retn 4) - the cell's distance to a point: the smallest of its four corners' squared
// distances ((dz*dz + dy*dy) + dx*dx), corner walked in place (x += size, z += size, x -= size - so the
// last x is (x0 + size) - size, not x0), then one fsqrt. `m = m < d ? m : d` (fcom; jnp keeps m).
float cellDistanceToPoint(const OverGrowthCell* cell, const Vec3* point)
{
    float cornerX = cell->corner.x;
    const float cornerY = cell->corner.y;
    float cornerZ = cell->corner.z;
    const float size = cell->size;
    float dx = cornerX - point->x;
    float dy = cornerY - point->y;
    float dz = cornerZ - point->z;
    float nearest = dz * dz + dy * dy + dx * dx;
    cornerX = cornerX + size;
    dx = cornerX - point->x;
    dy = cornerY - point->y;
    dz = cornerZ - point->z;
    float candidate = dz * dz + dy * dy + dx * dx;
    nearest = nearest < candidate ? nearest : candidate;
    cornerZ = cornerZ + size;
    dx = cornerX - point->x;
    dy = cornerY - point->y;
    dz = cornerZ - point->z;
    candidate = dz * dz + dy * dy + dx * dx;
    nearest = nearest < candidate ? nearest : candidate;
    dx = (cornerX - size) - point->x;
    dy = cornerY - point->y;
    dz = cornerZ - point->z;
    candidate = dz * dz + dy * dy + dx * dx;
    nearest = nearest < candidate ? nearest : candidate;
    return std::sqrt(nearest);
}

// loc_1015E580 / sub_1015E5A0 - update()'s two sort predicates (ltcg register convention in the PE:
// ecx = a, edx = b). Both compare the camera distance +0x1C. 1015E580 answers a < b (test ah, 5 / jp);
// 1015E5A0 answers a > b (test ah, 41h / jnz).
bool cellNearerFirst(const OverGrowthCell* a, const OverGrowthCell* b)
{ return a->cameraDistance < b->cameraDistance; }
bool cellFartherFirst(const OverGrowthCell* a, const OverGrowthCell* b)
{ return a->cameraDistance > b->cameraDistance; }

// sub_1015E930 - the instance's default constructor. The matrix half is the retail Mat4() inlined: all
// sixteen zeroed, then 1.0f on the diagonal. +0x54 (the shadow value) and the pad after +0x58 are NOT
// written - every shipped user stores +0x54 before reading it.
void constructInstance(OverGrowthInstance& instance)
{
    instance.geometryIndex = -1;
    for (unsigned int i = 0; i < 16; ++i)
        instance.transform.m[i] = 0.0f;
    instance.transform.m[15] = 1.0f;
    instance.transform.m[10] = 1.0f;
    instance.transform.m[5] = 1.0f;
    instance.transform.m[0] = 1.0f;
    instance.scale = 1.0f;
    instance.cullRadius = 0.0f;
    instance.radiusToSame = 0.0f;
    instance.radiusToOthers = 0.0f;
    instance.generated = 0;
    instance.lodFadeState = 1.0f;
}

// The occlusion query update() makes for every candidate cell: `mov ecx, dword_1025A39C; call
// [eax+3Ch]` with the cell's MeshBounds (0x1015F32B), through the drawer's table like the PE and
// unguarded. The slot is ObjectDrawer::isBoundsOccluded (0x100F8000, Geom/ObjectDrawer.cpp). It
// refreshes the box's derived data IN place when its dirty byte is set, hence the non-const box.
bool objectDrawerOccludes(MeshBounds& bounds)
{
    return dispatch<bool>(static_cast<void*>(dx9rend::g_ObjectDrawer), 0x3C,
                          static_cast<void*>(&bounds));
}

// The texture store rebuildCells inlines for each stream (0x100CAE5E..0x100CAEAC, 0x100CAEAC..0x100CAEFA):
// the stream's material-block texture slot (stream +0xC8, block +0xBC) takes the level OverGrowth's
// atlas texture (+0x100 of dword_10258F38 - the global, not `this`). An unchanged pointer is left alone;
// the old one is released owner-side (`if (!--ref) { sub_10014E20; free }`, the slot is not cleared
// first) and the new one is addRef'd through its shared table's +0x08. The block's Ra shader word
// (stream +0x158, raMaterialShaderSlot): rebuildCells stores "LeafOG" / "TrunkOG" there.
inline RaMaterialParameters& streamMaterial(OverGrowthCellMesh* stream)
{
    return *reinterpret_cast<RaMaterialParameters*>(stream->m_materialParameters);
}
void assignAtlasTexture(OverGrowthCellMesh* stream)
{
    dx9rend::EngineTexture* const atlas = static_cast<OverGrowth*>(dx9rend::g_OverGrowth)->m_atlasPage;
    dx9rend::EngineTexture*& slot =
        *reinterpret_cast<dx9rend::EngineTexture**>(&streamMaterial(stream).diffuseMap.value);   // block +0xBC
    dx9rend::EngineTexture* const old = slot;
    if (old == atlas)
        return;
    if (old && --old->refCount == 0)
    {
        dx9rend::textureDtor(old);
        ::renddx9abi::engineFree(old);
    }
    slot = atlas;
    if (atlas)
        dispatch<void>(static_cast<void*>(atlas), 0x08);             // the shared table's addRef
}

} // anonymous namespace


// OverGrowthCellMesh

OverGrowthCellMesh::OverGrowthCellMesh(unsigned int vertexCapacity, unsigned int indexCapacity,
                                       const std::vector<const dx9rend::Ra::RaVertexElementDesc*>& elements)
    : RaStreamDataEditable(vertexCapacity, indexCapacity, 16u, elements, dx9rend::Ra::raRendererDevice())
{
    dx9rend::Ra::raObjectParametersConstruct(m_objectParameters);    // sub_100458E0(this + 0x1A4)
    objectParameters().transparency.value = 1.0f;                    // this + 0x354
}

// 100D0C60 (retn 4) - the block (sub_1004D900), then RaStreamDataEditable's members (sub_10048F00), then
// the engine free when bit 0 is set.
void* OverGrowthCellMesh::destroy(unsigned char flags)
{
    dx9rend::Ra::raObjectParametersDestroy(m_objectParameters);
    destroyMembers();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}

// [TEMP] [CHOSEN, not recovered] RaStreamDataEditable's device-reset pair. Nothing in this tree calls
// either slot; declared only so +0x38/+0x3C land at their retail offsets.
// [RETIRE-WHEN: overgrowth-stream-device-reset] - the pair recovered into RaStreamDataEditable.cpp.
void OverGrowthCellMesh::releaseBuffers()
{
    unresolvedAt("dice::hfe::dx9rend::Ra::RaStreamDataEditable +0x30 (10048D60, release the D3D buffers)");
}
void OverGrowthCellMesh::createBuffers()
{
    unresolvedAt("dice::hfe::dx9rend::Ra::RaStreamDataEditable +0x40 (10048C70, create the D3D buffers)");
}

// 10048EF0: `mov eax, [ecx+190h]; sub eax, [ecx+18Ch]`.
unsigned int OverGrowthCellMesh::getFreeVertexCount() const { return m_vertexCapacity - m_vertexCount; }
// 10112970: `mov eax, [ecx+190h]`.   100EE9D0: `mov eax, [ecx+194h]`.
unsigned int OverGrowthCellMesh::getVertexCapacity() const { return m_vertexCapacity; }
unsigned int OverGrowthCellMesh::getIndexCapacity() const { return m_indexCapacity; }


// Initializes an unplaced cell and its leaf/trunk streams using the global level budgets.
// Vertices use SHORT4 position, D3DCOLOR normal, and SHORT2 texcoord (16 bytes total).
// Failed engine allocations leave the corresponding stream null.
OverGrowthCell::OverGrowthCell()
    : farList()
{

    visible = 0;
    built = 0;
    empty = 1;
    corner.x = 0.0f;                                                 // +0x08..+0x3C zeroed
    corner.y = 0.0f;
    corner.z = 0.0f;
    size = 0.0f;
    fadeFactor = 0.0f;
    cameraDistance = 0.0f;
    bounds.min.x = 0.0f;
    bounds.min.y = 0.0f;
    bounds.min.z = 0.0f;
    bounds.max.x = 0.0f;
    bounds.max.y = 0.0f;
    bounds.max.z = 0.0f;
    bounds.originRadius = 0.0f;
    bounds.radius = 0.0f;
    bounds.dirty = 1;                                                // +0x40
    bounds.extent.x = 0.0f;                                          // +0x44..+0x58 zeroed
    bounds.extent.y = 0.0f;
    bounds.extent.z = 0.0f;
    bounds.center.x = 0.0f;
    bounds.center.y = 0.0f;
    bounds.center.z = 0.0f;
    cellX = 0x2CEE9D7;
    cellZ = 0x2CEE9D7;


    hasNear = 0;
    dx9rend::Ra::raObjectParametersConstruct(objectParameters);
    unresolved29C = 0;
    lastPatch = 0;

    static const dx9rend::Ra::RaVertexElementDesc s_position(0, 7, 0, "Vtx_Position");
    static const dx9rend::Ra::RaVertexElementDesc s_normal(1, 4, 3, "Vtx_Normal");
    static const dx9rend::Ra::RaVertexElementDesc s_texCoord(2, 6, 5, "Vtx_TBase");

    // The stream's element descriptors (insert into the empty vector, then two push_backs).
    std::vector<const dx9rend::Ra::RaVertexElementDesc*> elements;
    elements.push_back(&s_position);
    elements.push_back(&s_normal);
    elements.push_back(&s_texCoord);

    const OverGrowth* const owner = static_cast<const OverGrowth*>(dx9rend::g_OverGrowth);
    void* const leafMemory = ::renddx9abi::engineAlloc(sizeof(OverGrowthCellMesh));
    leaf = leafMemory
        ? ::new (leafMemory) OverGrowthCellMesh(owner->m_streamVertexCounts[0],
                                                owner->m_streamIndexCounts[0], elements)
        : 0;
    void* const trunkMemory = ::renddx9abi::engineAlloc(sizeof(OverGrowthCellMesh));
    trunk = trunkMemory
        ? ::new (trunkMemory) OverGrowthCellMesh(owner->m_streamVertexCounts[1],
                                                 owner->m_streamIndexCounts[1], elements)
        : 0;
}

// +0x00: PE 0x100D1890 (the scalar deleting wrapper) over 0x100D18B0 (the body, read in full) and
// (the base destructor, which only republishes the base table - not modelled). Each stream is
// destroyed through its own +0x00 with 1 (no null store afterwards), the far list is freed, then the
// Ra object block (sub_1004D900); the cell's members are then destroyed.
void* OverGrowthCell::destroy(unsigned char flags)
{
    OverGrowthCellMesh* const leafStream = leaf;
    if (leafStream)
        leafStream->destroy(1);
    OverGrowthCellMesh* const trunkStream = trunk;
    if (trunkStream)
        trunkStream->destroy(1);
    std::vector<OverGrowthInstance*>().swap(farList);
    dx9rend::Ra::raObjectParametersDestroy(objectParameters);
    this->~OverGrowthCell();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
    return this;
}

// +0x04, appendGeometry, the LOD-vertex packer: OverGrowthCellRender.cpp.

// +0x08, leaf then trunk, each through its own +0x24 (lock); no null tests.
void OverGrowthCell::lock()
{
    leaf->lock();
    trunk->lock();
}

// +0x0C, the same pair through +0x28 (unlock).
void OverGrowthCell::unlock()
{
    leaf->unlock();
    trunk->unlock();
}

// +0x10, draw, the cell's two render-queue submissions: OverGrowthCellRender.cpp.

// +0x14, leaf +0x38 first, then trunk +0x38, summed.
unsigned int OverGrowthCell::getVertexCapacity()
{
    const unsigned int leafCapacity = leaf->getVertexCapacity();
    return leafCapacity + trunk->getVertexCapacity();
}

// +0x18, the same through +0x3C.
unsigned int OverGrowthCell::getIndexCapacity()
{
    const unsigned int leafCapacity = leaf->getIndexCapacity();
    return leafCapacity + trunk->getIndexCapacity();
}

// +0x1C, PE 0x100D1880 (16 B): `push 0; push 0; add ecx, 28Ch; call sub_100CF850` - the far list's
// resize(0) (erase, the buffer is kept).
void OverGrowthCell::clearFarList()
{
    OverGrowthInstance* const none = 0;
    farList.resize(0u, none);
}

// The allocation postLoadResources makes per grid slot (0x100CD6DE..0x100CD6FB): sub_1005F320(0x2AC), then
// the constructor when the heap answered. Published for the load half.
OverGrowthCell* createOverGrowthCell()
{
    void* const memory = ::renddx9abi::engineAlloc(sizeof(OverGrowthCell));
    return memory ? ::new (memory) OverGrowthCell() : 0;
}

namespace {


// Updates the camera-centred ring of cells, reseating changed slots and building visible cells.
// World-cell coordinates truncate toward zero; generation seeds depend on cell coordinates.

void baseUpdateCells(OverGrowth* self, IRenderView* view)
{
    if (!self->m_enabled)
        return;
    if (!self->m_initialized)
        self->initialize();                                                       // vslot +0x04
    const Mat4* const camera = view->getViewMatrixPtr();                          // vslot +0x64
    self->m_cameraPosition.y = camera->m[13];
    self->m_cameraPosition.x = camera->m[12];
    self->m_cameraPosition.z = camera->m[14];
    const float inverseCellSize = 1.0f / self->m_cellSize;
    const float viewDistance = self->m_viewDistance;
    const int cameraCellX = static_cast<int>(inverseCellSize * self->m_cameraPosition.x);
    const int cameraCellZ = static_cast<int>(inverseCellSize * self->m_cameraPosition.z);
    for (int z = 0; z < self->m_gridSize; ++z)
    {
        const float inverseViewDistance = 1.0f / viewDistance;                    // 0x1015EF51, per row
        for (int x = 0; x < self->m_gridSize; ++x)
        {
            OverGrowthCell* const cell = self->getCell(x, z);                     // vslot +0x20
            const int n = self->m_gridSize;
            const int cellX = n / 2 - (100 * n + x + cameraCellX) % n + cameraCellX;
            const int cellZ = n / 2 - (100 * n + z + cameraCellZ) % n + cameraCellZ;
            const bool moved = cellX != cell->cellX || cellZ != cell->cellZ
                               || self->m_reseatCells != 0;
            const bool built = cell->built != 0 && !moved;
            cell->built = built;
            if (!built && moved)
            {
                cell->cellX = cellX;
                cell->cellZ = cellZ;
                cell->size = self->m_cellSize;
                cell->corner.x = static_cast<float>(cellX) * self->m_cellSize;
                cell->corner.y = 0.0f;
                cell->corner.z = static_cast<float>(cellZ) * self->m_cellSize;
                const float half = self->m_cellSize * 0.5f;
                cell->corner.y = heightmapCluster(self)->getHeightInWorldCoords(
                    half + cell->corner.x, half + cell->corner.z);                // vslot +0x6C
                cell->clearFarList();                                             // cell vslot +0x1C
            }
            const float half = self->m_cellSize * 0.5f;
            const float dx = (half + cell->corner.x) - self->m_cameraPosition.x;
            const float dy = cell->corner.y - self->m_cameraPosition.y;
            const float dz = (half + cell->corner.z) - self->m_cameraPosition.z;
            cell->cameraDistance = std::sqrt(dz * dz + dy * dy + dx * dx);
            const float reach = cellDistanceToPoint(cell, &self->m_cameraPosition) * inverseViewDistance;
            const bool inRange = reach < 1.0f;
            const float fadeBand = self->m_fadeBand;
            if (fadeBand == 0.0f || !(1.0f - fadeBand < reach))
                cell->fadeFactor = 1.0f;
            else
                cell->fadeFactor = clampFloat((1.0f - reach) / fadeBand, 0.0f, 1.0f);
            bool visible = false;
            if (g_overGrowthBuildEveryCell || inRange)
            {
                if (!cell->built)
                {
                    self->m_randomSeed =
                        (static_cast<unsigned int>(cell->cellZ) * 4711u
                         + static_cast<unsigned int>(cell->cellX) * 13u + 23u) & 0x7FFFFFFFu;
                    OverGrowthInstanceVector instances;
                    self->collectCell(cell, &instances);
                    self->buildCell(cell, &instances);
                    cell->built = 1;
                }
                visible = cell->built != 0;
            }
            cell->visible = visible;
        }
    }
    self->m_reseatCells = 0;
}


// sub_100CA3A0, 176 B, read in full - the editor's patch pass (rebuildCells, when +0xF0). Row-major over
// the patch grid; each patch gets seed = (pz * 4711 + px * 13 + 23) & 0x7FFFFFFF (patch indices, from
// 0 - unlike the cells' signed world indices), its coverage (100C9AE0), then populatePatch (vslot +0x44).

void regeneratePatches(OverGrowth* self)
{
    int rowSeed = 23;
    for (int pz = 0; pz < self->m_patchCountZ; ++pz)
    {
        int seed = rowSeed;
        for (int px = 0; px < self->m_patchCountX; ++px)
        {
            OverGrowthPatch* const patch = self->m_patches + (self->m_patchCountX * pz + px);
            self->m_randomSeed = static_cast<unsigned int>(seed) & 0x7FFFFFFFu;
            computeCoverage(self, patch);
            self->populatePatch(patch);                                           // vslot +0x44
            seed += 13;
        }
        rowSeed += 4711;
    }
}

} // anonymous namespace

// OverGrowthCell.h: render reaches the base arm directly when +0xD5 is clear (0x100CA5AE).
void overGrowthBaseUpdateCells(OverGrowth* self, IRenderView* view)
{
    baseUpdateCells(self, view);
}


// Updates, culls and draws visible cells near-to-far with distance and height fade.
// Equal-distance ordering follows the host sort and may differ from the compatibility STL.
void OverGrowth::update(IRenderView* view)
{
    updateCells(view);                                                            // vslot +0x1C
    if (!m_enabled)
        return;
    if (!m_initialized)
        initialize();                                                             // vslot +0x04
    std::vector<OverGrowthCell*> fartherFirst;                                    // [ebp-0x40]
    fartherFirst.reserve(static_cast<unsigned int>(m_gridSize * m_gridSize));
    std::vector<OverGrowthCell*> nearerFirst;                                     // [ebp-0x30]
    nearerFirst.reserve(static_cast<unsigned int>(m_gridSize * m_gridSize));

    const Mat4* const camera = view->getViewMatrixPtr();                          // vslot +0x64
    const float cameraX = camera->m[12];
    const float cameraY = camera->m[13];
    const float cameraZ = camera->m[14];
    const float heightFade = clampFloat(
        std::fabs(cameraY - heightmapCluster(this)->getHeightInWorldCoords(cameraX, cameraZ))
            / m_heightFadeDistance,
        m_minimumHeightFade, 1.0f);

    for (int z = 0; z < m_gridSize; ++z)
    {
        for (int x = 0; x < m_gridSize; ++x)
        {
            OverGrowthCell* const cell = getCell(x, z);                           // vslot +0x20
            if (!cell->visible || cell->empty)
                continue;
            if (g_overGrowthCellDistanceCull)
            {
                const Mat4* const again = view->getViewMatrixPtr();               // vslot +0x64, again
                const float distance =
                    cellDistanceToPoint(cell, reinterpret_cast<const Vec3*>(&again->m[12]));
                if (view->getFarPlane() < distance)                               // vslot +0x3C
                    continue;
            }
            if (g_overGrowthCellFrustumCull)
            {
                if (!terrainFrustumIntersectsBox(view->getFrustumPtr(),           // vslot +0x74
                                                 &cell->bounds.min.x))
                    continue;
            }
            if (objectDrawerOccludes(cell->bounds))
                continue;
            cell->fadeFactor =
                clampFloat(cell->cameraDistance / m_cellFadeDistance, heightFade, 1.0f)
                * cell->fadeFactor;
            nearerFirst.push_back(cell);
        }
    }
    std::sort(fartherFirst.begin(), fartherFirst.end(), cellFartherFirst);
    std::sort(nearerFirst.begin(), nearerFirst.end(), cellNearerFirst);
    nearerFirst.insert(nearerFirst.end(), fartherFirst.begin(), fartherFirst.end());
    for (unsigned int i = 0; i < nearerFirst.size(); ++i)
    {
        OverGrowthCell* const cell = nearerFirst[i];
        cell->draw(view, &cell->corner, cell->fadeFactor);                        // cell vslot +0x10
    }
}

// +0x14, PE 0x100CAD70, 625 B, read in full. Every grid cell forgets itself - visible, built and has-near
// cleared, its far list freed (not just emptied), +0x29C and the remembered patch zeroed - and both
// streams get the Ra technique ("LeafOG" / "TrunkOG" through sub_10041F20 on dword_1023E408, skipFileCheck
// false, into the material block's +0x158) and the atlas texture. With +0xF0 (the editor's generate flag)
// the patches are regenerated (100CA3A0) and then walked once more by a loop whose body compiled to
// nothing (0x100CAF4A..0x100CAFD3, kept). +0x38 = 1 makes the next updateCells re-seat every cell.
void OverGrowth::rebuildCells()
{
    for (int z = 0; z < m_gridSize; ++z)
    {
        for (int x = 0; x < m_gridSize; ++x)
        {
            OverGrowthCell* const cell = m_cells[m_gridSize * z + x];
            cell->visible = 0;
            cell->built = 0;
            cell->hasNear = 0;
            cell->farList.clear();                                                // freed, not just emptied
            cell->farList.shrink_to_fit();
            cell->unresolved29C = 0;
            cell->lastPatch = 0;
            {
                const std::string name("LeafOG");
                dx9rend::Ra::RaShader* const shader = dx9rend::Ra::getShader(
                    static_cast<dx9rend::Ra::RaShaderTable*>(dx9rend::g_renderQueues), name, false);
                raMaterialShaderSlot(streamMaterial(cell->leaf)) = shader;       // stream +0x158
            }
            {
                const std::string name("TrunkOG");
                dx9rend::Ra::RaShader* const shader = dx9rend::Ra::getShader(
                    static_cast<dx9rend::Ra::RaShaderTable*>(dx9rend::g_renderQueues), name, false);
                raMaterialShaderSlot(streamMaterial(cell->trunk)) = shader;      // stream +0x158
            }
            assignAtlasTexture(cell->leaf);
            assignAtlasTexture(cell->trunk);
        }
    }
    if (m_editorEnable)
    {
        regeneratePatches(this);
        for (int pz = 0; pz < m_patchCountZ; ++pz)
        {
            for (int px = 0; px < m_patchCountX; ++px)
            {
                const OverGrowthInstanceVector& instances =
                    m_patches[m_patchCountX * pz + px].instances;
                for (unsigned int i = 0; i < instances.size(); ++i)
                {
                    // The shipped loop's body compiled to nothing.
                }
            }
        }
    }
    m_reseatCells = 1;
}

// +0x18, PE 0x1015E600, 163 B, read in full. The density map value under (x, z): each axis
// (world - origin(+0x64 / +0x68)) / extent(+0x5C / +0x60) * (float)(unsigned)size, truncated to int;
// outside [0, size) (signed low test, unsigned high test) answers 0, else the byte at +0x80.
unsigned int OverGrowth::sampleDensity(float x, float z)
{
    const unsigned int width = m_mapWidth;
    const int ix = static_cast<int>((x - m_originX) / m_extentX * static_cast<float>(width));
    const unsigned int height = m_mapHeight;
    const int iz = static_cast<int>((z - m_originZ) / m_extentZ * static_cast<float>(height));
    if (ix < 0 || static_cast<unsigned int>(ix) >= width
        || iz < 0 || static_cast<unsigned int>(iz) >= height)
        return 0;
    return m_densityMap[width * iz + ix];
}

// +0x1C, PE 0x100C9450, 350 B, read in full. Unloaded (empty path, size +0x9C) does nothing. Otherwise the
// base grid pass (1015EEB0), then - enabled and with patches - the near pass: the close distance
// +0xC4 = ((view zoom * +0xF4) * +0xC8) * +0x24 (zoom x quality x closeDistPercentage x view distance), and
// for every grid slot whose patch (world cell + patch grid / 2) is on the map, updateCell (vslot +0x3C)
// when the patch's bounds reach within (+0xCC + 1) * +0xC4 of the camera or the cell still has near
// instances (+0x64). `changed` is "a different patch than last time" (+0x2A0), which is then recorded.
// The slot is read straight from the array (N * z + x), not through getCell.
void OverGrowth::updateCells(IRenderView* view)
{
    if (m_path.empty())
        return;
    baseUpdateCells(this, view);
    if (!m_enabled)
        return;
    if (!m_patches)
        return;
    m_closeDistance = view->getZoom() * m_qualityScale * m_closeDistPercentage
                      * m_viewDistance;                                           // vslot +0x28
    for (int z = 0; z < m_gridSize; ++z)
    {
        for (int x = 0; x < m_gridSize; ++x)
        {
            OverGrowthCell* const cell = m_cells[m_gridSize * z + x];
            const int patchCountX = m_patchCountX;
            const int patchCountZ = m_patchCountZ;
            const int px = patchCountX / 2 + cell->cellX;
            const int pz = patchCountZ / 2 + cell->cellZ;
            if (px < 0 || pz < 0 || px >= patchCountX || pz >= patchCountZ)
                continue;
            OverGrowthPatch* const patch = m_patches + (patchCountX * pz + px);
            const float radius = (m_closeDistFade + 1.0f) * m_closeDistance;
            if (boundsSphereIntersects(patch->bounds, m_cameraPosition, radius)
                || cell->hasNear)
            {
                const bool changed = cell->lastPatch != patch;
                updateCell(cell, &patch->instances, changed);
                cell->lastPatch = patch;
            }
        }
    }
}

// +0x24, PE 0x1015EA40, 212 B, read in full (the base's slot, reused). Lock both streams (cell +0x08),
// append every instance whose geometry type has a LOD geometry (+0x1C) - cell +0x04 with (that
// geometry's +0x38 template, &transform, scale +0x44, 0, shadow +0x54) - unlock (cell +0x0C), and record
// whether the list was empty (+0x06, `setz`).
void OverGrowth::buildCell(OverGrowthCell* cell, OverGrowthInstanceVector* instanceVector)
{
    OverGrowthInstanceVector& instances = *instanceVector;
    cell->lock();                                                                 // cell vslot +0x08
    for (unsigned int i = 0; i < instances.size(); ++i)
    {
        OverGrowthInstance* const instance = &instances[i];
        const OverGrowthGeometryPair* const type =
            m_geometries.data() + static_cast<unsigned int>(instance->geometryIndex);
        void* const lodGeometry = type->lod.geometry;
        if (lodGeometry)
            cell->appendGeometry(geometryTemplate(lodGeometry), &instance->transform,
                                 instance->scale, 0, instance->shadow);
    }
    cell->unlock();                                                               // cell vslot +0x0C
    cell->empty = instances.size() == 0;
}

// +0x28, PE 0x100CBBC0, 192 B, read in full. Without patches: nothing. A cell off the patch grid gets an
// emptied list (resize(0) - the Instance() temporary 1015E930 is the by-value argument). Otherwise the
// cell takes its patch's instance list (operator=, 100D0700), bounds (sub_100AE1F0) and corner, then the
// list is walked by a loop whose body compiled to nothing (0x100CBC3E..0x100CBC62, kept).
void OverGrowth::collectCell(OverGrowthCell* cell, OverGrowthInstanceVector* instanceVector)
{
    if (!m_patches)
        return;
    OverGrowthInstanceVector& instances = *instanceVector;
    const int patchCountX = m_patchCountX;
    const int patchCountZ = m_patchCountZ;
    const int px = patchCountX / 2 + cell->cellX;
    const int pz = patchCountZ / 2 + cell->cellZ;
    if (px < 0 || pz < 0 || px >= patchCountX || pz >= patchCountZ)
    {
        OverGrowthInstance empty;
        constructInstance(empty);
        instances.resize(0u, empty);
        return;
    }
    OverGrowthPatch* const patch = patchAt(this, px, pz);
    instances = patch->instances;                                                 // operator=, 100D0700
    meshBoundsCopy(cell->bounds, patch->bounds);
    cell->corner.x = patch->position.x;
    cell->corner.y = patch->position.y;
    cell->corner.z = patch->position.z;
    for (unsigned int i = 0; i < instances.size(); ++i)
    {
        // The shipped loop's body compiled to nothing.
    }
}

// Updates the far-instance list, retaining its matching prefix and rebuilding streams when needed.
// Near instances use full meshes; distance thresholds combine template radius and type settings.
void OverGrowth::updateCell(OverGrowthCell* cell, OverGrowthInstanceVector* instanceVector, bool changed)
{
    std::vector<OverGrowthInstance*>& farList = cell->farList;
    OverGrowthInstanceVector& instances = *instanceVector;
    bool modified = false;                                                        // [esp+0x13]
    unsigned int kept = 0;                                                        // [esp+0x1C]
    for (unsigned int i = 0; i < instances.size(); ++i)
    {
        OverGrowthInstance* const instance = &instances[i];
        const float dx = m_cameraPosition.x - instance->transform.m[12];
        const float dy = m_cameraPosition.y - instance->transform.m[13];
        const float dz = m_cameraPosition.z - instance->transform.m[14];
        const OverGrowthGeometryPair* const type =
            m_geometries.data() + static_cast<unsigned int>(instance->geometryIndex);
        world::IObjectTemplate* const objectTemplate = type->full.objectTemplate;
        const float reach = objectTemplate->getCullRadiusScale() * instance->cullRadius   // +0x70
                            + type->full.distanceScale * m_closeDistance * m_viewDistanceScale;
        const bool isNear = (dx * dx + dz * dz + dy * dy) / (reach * reach) < 1.0f;
        if (!modified && !farList.empty() && kept < farList.size())
        {
            OverGrowthInstance* const listed = farList[kept];
            if (isNear)
            {
                if (listed == instance)
                {
                    OverGrowthInstance* const none = 0;
                    modified = true;
                    farList.resize(kept, none);
                }
            }
            else if (listed == instance)
            {
                ++kept;
            }
            else
            {
                OverGrowthInstance* const none = 0;
                modified = true;
                farList.resize(kept, none);
                farList.push_back(instance);
            }
            continue;
        }
        if (!isNear)
        {
            modified = true;
            farList.push_back(instance);
        }
    }

    const bool hasNear = instances.size() != farList.size();
    if (!modified)
        modified = hasNear != (cell->hasNear != 0);
    cell->hasNear = hasNear;
    const bool wasEmpty = cell->empty != 0;
    const bool nowEmpty = farList.size() == 0 || instances.size() == 0;
    cell->empty = nowEmpty;
    if (!changed && (!modified || (nowEmpty && wasEmpty)))
        return;

    // the cell's patch, re-derived; its bounds copied member-wise (the inlined 100AE1F0).
    const int patchCountX = m_patchCountX;
    const OverGrowthPatch* const patch = m_patches
        + ((m_patchCountZ / 2 + cell->cellZ) * patchCountX + patchCountX / 2 + cell->cellX);
    meshBoundsCopy(cell->bounds, patch->bounds);
    cell->lock();                                                                 // cell vslot +0x08
    for (unsigned int k = 0; k < farList.size(); ++k)
    {
        OverGrowthInstance* const listed = farList[k];
        const OverGrowthGeometryPair* const type =
            m_geometries.data() + static_cast<unsigned int>(listed->geometryIndex);
        void* const lodGeometry = type->lod.geometry;
        if (lodGeometry)
            cell->appendGeometry(geometryTemplate(lodGeometry), &listed->transform,
                                 listed->scale, 0, listed->shadow);
    }
    cell->unlock();                                                               // cell vslot +0x0C
}

// +0x40, PE 0x100C9980, 341 B, read in full. One instance's transform. Draw 1: a yaw of
// count * 360/2^24 - 180 degrees (10077E30). The terrain normal at the position (cluster +0x7C) has x and z
// scaled by the type's normal scale (+0x54); draws 2 and 3 add a tilt in [-range, range] (range = type
// +0x40) to x, then to z, each as ((count * (range - (-range))) * 2^-24 + n) + (-range) - the PE's
// association. The normalised result becomes row 1, the basis is re-orthonormalised around it (100C8820)
// and the translation row takes the position.
void OverGrowth::placeInstance(OverGrowthType* type, Vec3* position, OverGrowthInstance* instance)
{
    Mat4& transform = instance->transform;
    setRotationY(transform, static_cast<float>(random24(this)) * kDegreesPerCount - 180.0f);
    Vec3 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    normal.z = 0.0f;
    heightmapCluster(this)->getNormalInWorldCoords(position->x, position->z, normal);          // vslot +0x7C
    normal.x = normal.x * type->normalScale;
    normal.z = normal.z * type->normalScale;
    float low = -type->rotationScale;
    normal.x = static_cast<float>(random24(this)) * (type->rotationScale - low) * kRandomUnit
               + normal.x + low;
    low = -type->rotationScale;
    normal.z = static_cast<float>(random24(this)) * (type->rotationScale - low) * kRandomUnit
               + normal.z + low;
    normalizeVec3(normal);
    transform.m[4] = normal.x;                                                    // +0x14, row 1
    transform.m[5] = normal.y;
    transform.m[6] = normal.z;
    orthonormalizeKeepingY(transform);
    transform.m[12] = position->x;                                                // +0x34
    transform.m[13] = position->y;
    transform.m[14] = position->z;
}

// Generates patch instances within material-density targets and stream budgets.
// Limits attempts to five times the target and enforces spacing against generated instances.
void OverGrowth::populatePatch(OverGrowthPatch* patch)
{
    int targets[16];
    int nothingLeft[16];
    for (unsigned int i = 0; i < 16; ++i)
    {
        targets[i] = 0;                                                           // rep stosd, 0x100CC81A
        nothingLeft[i] = 0;                                                       // rep stosd, 0x100CC828
    }
    int total = 0;
    const std::vector<OverGrowthMaterial>& materials = m_materials;
    for (unsigned int i = 0; i < materials.size(); ++i)
    {
        const OverGrowthMaterial* const material = &materials[i];
        const std::vector<OverGrowthType>& types = material->types;
        float expected = 0.0f;
        for (unsigned int j = 0; j < types.size(); ++j)
            expected = expected + types[j].density * m_cellSize * m_cellSize * 0.001f;
        const int value = static_cast<int>(material->id);
        const int count = static_cast<int>(expected * patch->coverage[value]);    // __ftol2
        total += count;
        targets[value] = count;
    }

    OverGrowthInstanceVector& instances = patch->instances;
    unsigned int leafVertices = m_streamVertexCounts[0];
    unsigned int trunkVertices = m_streamVertexCounts[1];
    unsigned int leafIndices = m_streamIndexCounts[0];
    unsigned int trunkIndices = m_streamIndexCounts[1];
    for (unsigned int i = 0; i < instances.size(); ++i)
    {
        const OverGrowthInstance* const existing = &instances[i];
        if (existing->generated)
            continue;
        const OverGrowthGeometryRecord& cost =
            m_geometries.data()[static_cast<unsigned int>(existing->geometryIndex)].lod;
        leafVertices -= cost.vertexCounts[0];
        trunkVertices -= cost.vertexCounts[1];
        leafIndices -= cost.indexCounts[0];
        trunkIndices -= cost.indexCounts[1];
    }

    bool keepGoing = true;
    const unsigned int attempts = static_cast<unsigned int>(total * 5);
    for (unsigned int attempt = 0; attempt < attempts && keepGoing; ++attempt)
    {
        Vec3 position;
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = 0.0f;
        const unsigned int value = pickPosition(this, patch, &position);
        float draw = static_cast<float>(random24(this)) * kRandomUnit;
        OverGrowthMaterial* const material = findMaterial(this, static_cast<int>(value));
        if (!material || targets[material->id] == 0)
            continue;
        std::vector<OverGrowthType>& types = material->types;
        for (unsigned int j = 0; j < types.size(); ++j)
        {
            OverGrowthType* const type = &types[j];
            if (!(draw < type->probability))
            {
                draw = draw - type->probability;
                continue;
            }
            if (type->geometryIndex < 0)
            {
                // sub_1015F450(&type->+0x1C): the geometry table's find-or-add (OverGrowth.cpp). A
                // -1 (a missing template) is stored and the next type is tried with the draw left as it
                // is (0x100CCAA5..0x100CCAAA -> 0x100CCC33).
                type->geometryIndex = findOrAddGeometry(type->geometry);
                if (type->geometryIndex < 0)
                    continue;
            }
            const OverGrowthGeometryPair* const geometryType =
                m_geometries.data() + static_cast<unsigned int>(type->geometryIndex);
            if (geometryType->lod.vertexCounts[0] > leafVertices
                || geometryType->lod.indexCounts[0] > leafIndices
                || geometryType->lod.vertexCounts[1] > trunkVertices
                || geometryType->lod.indexCounts[1] > trunkIndices)
            {
                keepGoing = false;
                break;
            }
            OverGrowthInstance instance;
            constructInstance(instance);
            instance.geometryIndex = type->geometryIndex;
            instance.shadow = sampleShadow(this, position.x, position.z);
            instance.scale = 1.0f;
            void* const fullGeometry = geometryType->full.geometry;
            instance.cullRadius = geometryBoundingRadius(fullGeometry);
            instance.radiusToSame = geometryBoundingRadius(fullGeometry) * type->minRadiusToSame;
            instance.radiusToOthers = geometryBoundingRadius(fullGeometry) * type->minRadiusToOthers;
            instance.generated = 1;
            placeInstance(type, &position, &instance);                            // vslot +0x40
            {
                const OverGrowthGeometryPair* const placedType =
                    m_geometries.data() + static_cast<unsigned int>(instance.geometryIndex);
                const Vec3 anchor = placedType->full.objectTemplate->getAnchorPos();   // +0xAC
                instance.transform.m[13] = instance.transform.m[13] - anchor.y;
            }
            bool spaced = true;
            for (unsigned int k = 0; k < instances.size(); ++k)
            {
                const OverGrowthInstance* const other = &instances[k];
                if (!other->generated)
                    continue;
                const float dx = position.x - other->transform.m[12];
                const float dz = position.z - other->transform.m[14];
                const float distanceSq = dz * dz + dx * dx;
                const float spacing = other->geometryIndex == instance.geometryIndex
                    ? instance.radiusToSame + other->radiusToSame
                    : instance.radiusToOthers + other->radiusToOthers;
                if (distanceSq < spacing * spacing)
                {
                    spaced = false;
                    break;
                }
            }
            if (!spaced)
                break;
            leafIndices -= geometryType->lod.indexCounts[0];
            trunkVertices -= geometryType->lod.vertexCounts[1];
            leafVertices -= geometryType->lod.vertexCounts[0];
            trunkIndices -= geometryType->lod.indexCounts[1];
            --targets[material->id];
            instances.push_back(instance);
            OverGrowthBoxCorners corners;
            constructBoxCorners(corners, *geometryLocalBounds(fullGeometry), instance.transform);
            boundsExpandByCorners(patch->bounds, corners);
            if (std::memcmp(targets, nothingLeft, sizeof(targets)) == 0)         // repe cmpsd, 0x100CCCF3
                keepGoing = false;
            break;
        }
    }
}

} } }
#endif
