// Draws nearby vegetation instances and gathers shadow casters.
// Distant vegetation is rendered through batched cell streams.
#ifdef BF2_IMAGE_RENDDX9
#include "OverGrowth.h"
#include "OverGrowthCell.h"                  // overGrowthBaseUpdateCells (1015EEB0)
#include "../GeometryInstanceData.h"          // 100C88A0's +0x28 / +0x2C and the +0xF4 opacity slot
#include "../MeshTemplate.h"                  // IGeometryTemplate::createInstance (+0x24), MeshBounds
#include "../StaticMesh.h"                    // StaticMesh::m_float25C - the instance's shadow value
#include "../StaticMeshTemplate.h"            // the template bounds the shadow walk reads at +0xFC
#include "../Terrain.h"                       // terrainFrustumIntersectsBox - sub_101C7980
#include "../XpLightManager.h"                // g_LightManager's type
#include "../../Math/Frustum.h"
#include "../../Math/LocalBoundingBox.h"
#include "../../Math/Mat4.h"
#include "../../Math/Plane3.h"
#include "../../Math/Vec3.h"
#include "../../RendMisc/IRenderView.h"
#include "../../RendDX9/RendDX9Base.h"        // RendDX9Base (+0x514..+0x516), rendererFrameOrdinal
#include <string>

// The ONE host (BF2.exe) call of the draw half: IObjectTemplate::getCullRadiusScale on the object
// template a draw type keeps at +0x80. Included the way XpLightManager.cpp includes its host
// interfaces: the retail-ABI flavour takes HfeAbi's IBase / ClassManager carriers.
#include "../../Game/Objects/IObjectTemplate.h"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dice { namespace hfe {

namespace dx9rend {
extern void* g_rendererBase;       // dword_1023B970 - RendDX9Init.cpp, the Renderer (RendDX9Base)
extern IBase* g_ObjectDrawer;      // dword_1025A39C - BF2Render.cpp, published by Renderer::initialize
}

namespace geom {

extern LightManager* g_LightManager;   // dword_1024B914 - ModuleGeom.cpp

namespace {


// The vegetation draw records (OverGrowth.h): each draw type (geometry pair) holds the LOD and main
// geometry pools; geometries borrow instance transforms. Instance draw-type indices are unchecked.
// Patch bounds are finalized before culling.


// byte_102390C0 - file-backed 1 in .data with exactly ONE reference in the image, the read at
// 0x100CA70E, and no writer: the +0x58 skip below can never fire in retail. Kept mutable so the load
// happens at each instance, as shipped (the g_transparencyAARenderBracketsEnabled pattern).
// [INFERRED] name.
unsigned char g_overGrowthDrawObjectInstances = 1;

inline const Vec3& translationOf(const Mat4& transform)
{
    return *reinterpret_cast<const Vec3*>(&transform.m[12]);                // +0x30
}


// The geometry instance slots the draw calls. As in ObjectDrawer.cpp and XpLightManager.cpp the
// instance interface is not modelled as a C++ class in this tree, so each slot is reached by offset
// through the instance's retail-ordered table; StaticMesh, BundledMesh and SkinnedMesh install a body
// with the same arity at every one of these offsets (StaticMesh.cpp kStaticMeshVtable). These are the
// renderer's OWN objects (created by the draw type's IGeometryTemplate), laid out identically in both
// flavours - RW-266's "renderer reading its own structures".

inline void* geometrySlot(void* geometry, unsigned int offset)
{
    return (*reinterpret_cast<void***>(geometry))[offset / 4];
}

// +0x18 draw(renderView, occluder, flag), `retn 0Ch` (StaticMesh 0x100FF510). The OverGrowth draw
// always passes (view, 0, 1): 100CA974..100CA981 and 100CA9EC..100CA9F9.
inline void geometryDraw(void* geometry, IRenderView* view)
{
    typedef void (__thiscall* DrawFn)(void*, void*, void*, int);
    reinterpret_cast<DrawFn>(geometrySlot(geometry, 0x18))(geometry, view, 0, 1);
}

// +0x24 - the shadow record this geometry receives from (StaticMesh +0x34C), one argument.
inline void geometryAttachShadowRecord(void* geometry, void* record)
{
    typedef void (__thiscall* AttachFn)(void*, void*);
    reinterpret_cast<AttachFn>(geometrySlot(geometry, 0x24))(geometry, record);
}

// +0x40 getLocalBounds - the selected lod's box (StaticMesh lod record + 4).
inline const LocalBoundingBox& geometryLocalBounds(void* geometry)
{
    typedef const void* (__thiscall* BoundsFn)(void*);
    return *static_cast<const LocalBoundingBox*>(
        reinterpret_cast<BoundsFn>(geometrySlot(geometry, 0x40))(geometry));
}

// +0x44 getBoundingRadius, float in ST0 (StaticMesh it finalises the lod bounds first,
// so the call is NOT side-effect free and every shipped repetition is kept).
inline float geometryBoundingRadius(void* geometry)
{
    typedef float (__thiscall* RadiusFn)(void*);
    return reinterpret_cast<RadiusFn>(geometrySlot(geometry, 0x44))(geometry);
}

// +0x48 getGeometryTransformation (StaticMesh the stored pointer or the shared identity).
inline const Mat4* geometryTransformation(void* geometry)
{
    typedef const Mat4* (__thiscall* TransformFn)(void*);
    return reinterpret_cast<TransformFn>(geometrySlot(geometry, 0x48))(geometry);
}

// +0x50 setGeometryTransformation - stores the pointer (StaticMesh 0x100FF2B0), one argument.
inline void geometrySetTransformation(void* geometry, const Mat4* transform)
{
    typedef void (__thiscall* SetTransformFn)(void*, const Mat4*);
    reinterpret_cast<SetTransformFn>(geometrySlot(geometry, 0x50))(geometry, transform);
}

// +0xAC - the shadow-caster frame stamp (StaticMesh +0x350), one argument: renderer +0x54.
inline void geometryStampShadowFrame(void* geometry, unsigned int frameOrdinal)
{
    typedef void (__thiscall* StampFn)(void*, unsigned int);
    reinterpret_cast<StampFn>(geometrySlot(geometry, 0xAC))(geometry, frameOrdinal);
}


// PE 0x100C88A0 (29 B), read in full: `fld arg; mov eax, arg; fmul [ecx+28h]; mov edx, [ecx];
// push ecx; mov [ecx+2Ch], eax; fstp [esp]; call [edx+0F4h]; retn 4` - a thiscall on the geometry.
// The distance fade goes to +0x2C and the geometry's own opacity slot (+0xF4, StaticMesh's setFade:
// the Ra transparency) gets it times the base opacity +0x28. The product is formed before the store,
// the store precedes the call. [INFERRED] name; it sits among OverGrowth's COMDATs (0x100C8xxx).

void geometrySetDistanceFade(void* geometry, float fade)
{
    GeometryInstanceData* const data = static_cast<GeometryInstanceData*>(geometry);
    const float opacity = fade * data->m_float28;
    data->m_float2C = fade;
    data->setOpacity(opacity);                                              // +0xF4
}


// Scalar helpers. Each is its own out-of-line body in the PE (callee-cleanup, float in ST0).


// PE 0x1000ED40 (34 B), read in full: `fcomi high; fcmovnb` then `fcomi low; fcmovb` - the high bound first; an
// unordered value sets CF and comes back as `low`. The tree's twins (NametagManagerUpdate.inl's
// nametagClampFloat) are file-local, so this TU carries its own.
float clampFloat(float value, float low, float high)
{
    if (value >= high)
        value = high;
    if (!(value >= low))
        value = low;
    return value;
}

// PE 0x10030C60 (30 B), read in full: `fld a; fcomp b; test ah,5; jp -> b` - a when a < b (ordered),
// else b.
float minFloat(float a, float b)
{
    return a < b ? a : b;
}

// PE 0x10030C80 (30 B), read in full: the same compare, the other answer - b when a < b (ordered),
// else a.
float maxFloat(float a, float b)
{
    return a < b ? b : a;
}

// PE 0x1007B080 (75 B), read in full, register arguments (ecx = a, edx = b; ltcg): d = b - a, and the sum is
// (dz*dz + dx*dx) + dy*dy - the shipped association (1007B0B1..1007B0C1).
float distanceSquared(const Vec3& a, const Vec3& b)
{
    const float dx = b.x - a.x;
    const float dy = b.y - a.y;
    const float dz = b.z - a.z;
    return dz * dz + dx * dx + dy * dy;
}

// PE 0x1000A6B0 (77 B), read in full, on dword_1023B970 - is multisampling active: the forced byte
// +0x514 wins, then the disabled byte +0x515, else the first 8-byte entry of the mode vector +0xC0
// (`sar 3`) is > 0
// (`setnle`). BF2Render.cpp and TerrainDraw.cpp keep the same body file-local.
bool isMultisampleActive()
{
    const dx9rend::RendDX9Base* const base =
        static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    return dx9rend::multiSampleListActive(base);
}

// The render's alpha-to-coverage doubling, 100CA929..100CA950 and the predicate is
// asked, then dword_1023B970 is re-read for the capability byte +0x516, at each of the two sites.
float alphaToCoverageAdjusted(float alpha)
{
    if (isMultisampleActive()
        && static_cast<const dx9rend::RendDX9Base*>(dx9rend::g_rendererBase)->hasAlphaToCoverage)
        alpha = alpha + alpha;                                              // `fadd st, st`, NOT re-clamped
    return alpha;
}


// Tests sphere containment and writes fullyInside only on success.
// [PLATFORM] Uses RendDX9's z/y/x floating-point association, preserving unordered comparisons.
bool frustumContainsSphere(const Frustum& frustum, const Vec3& p, float radius, bool& fullyInside)
{
    const Plane3& p4 = frustum.m_planes[4];
    const float d4 = p4.normal.z * p.z + p4.normal.y * p.y + p4.normal.x * p.x + p4.d;
    if (d4 > radius)
        return false;
    const Plane3& p5 = frustum.m_planes[5];
    const float d5 = p5.normal.z * p.z + p5.normal.y * p.y + p5.normal.x * p.x + p5.d;
    if (d5 > radius)
        return false;
    const Plane3& p0 = frustum.m_planes[0];
    const float d0 = p0.normal.z * p.z + p0.normal.y * p.y + p.x * p0.normal.x + p0.d;
    if (d0 > radius)
        return false;
    const Plane3& p1 = frustum.m_planes[1];
    const float d1 = p1.normal.z * p.z + p1.normal.y * p.y + p.x * p1.normal.x + p1.d;
    if (d1 > radius)
        return false;
    const Plane3& p2 = frustum.m_planes[2];
    const float d2 = p2.normal.z * p.z + p2.normal.y * p.y + p.x * p2.normal.x + p2.d;
    if (d2 > radius)
        return false;
    const Plane3& p3 = frustum.m_planes[3];
    const float d3 = p.z * p3.normal.z + p.y * p3.normal.y + p.x * p3.normal.x + p3.d;
    if (d3 > radius)
        return false;

    const float sums[6] = { d0 + radius, d1 + radius, d4 + radius, d5 + radius, d2 + radius,
                            d3 + radius };
    unsigned int all = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < 6; ++i)
    {
        unsigned int bits;
        std::memcpy(&bits, &sums[i], sizeof(bits));
        all &= bits;
    }
    fullyInside = (all & 0x80000000u) == 0x80000000u;
    return true;
}

// PE 0x101C5350 (254 B), read in full - Frustum::getOutcode(point), RendDX9's copy (the tree's is a
// declared [FRONTIER] in Frustum.h). Bit i is plane i with the point strictly in front of it
// (distance > 0, ordered: `test ah,41h; jnz`), evaluated in the order 5, 4, 1, 0, 2, 3 (plane 5 assigns
// 0x20; the rest OR in), every distance (n.z*p.z + n.y*p.y) + n.x*p.x + d.
unsigned char frustumOutcode(const Frustum& frustum, const Vec3& p)
{
    unsigned char code = 0;
    const Plane3& p5 = frustum.m_planes[5];
    if (p5.normal.z * p.z + p5.normal.y * p.y + p5.normal.x * p.x + p5.d > 0.0f)
        code = 0x20;
    const Plane3& p4 = frustum.m_planes[4];
    if (p4.normal.z * p.z + p4.normal.y * p.y + p4.normal.x * p.x + p4.d > 0.0f)
        code |= 0x10;
    const Plane3& p1 = frustum.m_planes[1];
    if (p1.normal.z * p.z + p1.normal.y * p.y + p1.normal.x * p.x + p1.d > 0.0f)
        code |= 0x02;
    const Plane3& p0 = frustum.m_planes[0];
    if (p0.normal.z * p.z + p0.normal.y * p.y + p.x * p0.normal.x + p0.d > 0.0f)
        code |= 0x01;
    const Plane3& p2 = frustum.m_planes[2];
    if (p2.normal.z * p.z + p2.normal.y * p.y + p2.normal.x * p.x + p2.d > 0.0f)
        code |= 0x04;
    const Plane3& p3 = frustum.m_planes[3];
    if (p3.normal.z * p.z + p3.normal.y * p.y + p.x * p3.normal.x + p3.d > 0.0f)
        code |= 0x08;
    return code;
}

// PE 0x10079610 (92 B), read in full - Mat4 transformPoint (SkinnedMesh.cpp keeps the same body
// file-local as mat4TransformPoint). z and y are formed before x is stored because out may alias in.
void transformPoint(const Mat4& m, const float in[3], float out[3])
{
    const float z = m.m[2] * in[0] + m.m[10] * in[2] + m.m[6] * in[1] + m.m[14];
    const float y = m.m[1] * in[0] + m.m[9] * in[2] + m.m[5] * in[1] + m.m[13];
    out[0] = m.m[8] * in[2] + m.m[4] * in[1] + in[0] * m.m[0] + m.m[12];
    out[1] = y;
    out[2] = z;
}

// PE 0x101C54B0 (518 B), read in full - a local box against a frustum under a transform, only the
// planes in `mask` (thiscall on the box, `retn 0Ch`: frustum, transform, mask). True unless all eight
// transformed corners share an outside plane of the mask: the running AND of the corners' masked
// outcodes returns true the moment it reaches zero. Corner 0 (min, min, min) is transformed inline as
// (z*m[8+c] + y*m[4+c]) + x*m[c] + m[12+c] (101C54BA..101C5512); corners 1..7 go through 0x10079610,
// in the order x bit 2, y bit 1, z bit 0 - the associations differ, so both are kept.
bool boxIntersectsFrustum(const LocalBoundingBox& box, const Frustum& frustum, const Mat4& transform,
                          unsigned char mask)
{
    const float* const m = transform.m;
    const float x = box.m_min.x;
    const float y = box.m_min.y;
    const float z = box.m_min.z;
    Vec3 corner;
    corner.x = z * m[8] + y * m[4] + x * m[0] + m[12];
    corner.y = z * m[9] + y * m[5] + x * m[1] + m[13];
    corner.z = z * m[10] + y * m[6] + x * m[2] + m[14];
    unsigned char shared = static_cast<unsigned char>(frustumOutcode(frustum, corner) & mask);
    if (!shared)
        return true;
    for (unsigned int i = 1; i < 8; ++i)
    {
        const float local[3] = {
            (i & 4u) ? box.m_max.x : box.m_min.x,
            (i & 2u) ? box.m_max.y : box.m_min.y,
            (i & 1u) ? box.m_max.z : box.m_min.z
        };
        transformPoint(transform, local, &corner.x);
        shared = static_cast<unsigned char>(shared & (frustumOutcode(frustum, corner) & mask));
        if (!shared)
            return true;
    }
    return false;
}


// The eight-corner object box - PE 0x100CE120 (122 B: zero the 24 floats, then 0x101C6750 setBounds
// and 0x101C67F0 transform) and 0x100AE110 (94 B: the zeroing alone). All four bodies read here in
// full; this is XpLightManager.cpp's ShadowBoxCorners (file-local there), re-checked against the PE:
// corner i takes x from max when bit 2, y when bit 1, z when bit 0; the transform forms all three rows
// from the untouched corner, and corner 0's y and x rows associate differently from corners 1..7
// (101C67F6..101C683D against 101C6845..101C688C).

struct BoxCorners
{
    Vec3 corner[8];

    BoxCorners()
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
    }

    BoxCorners(const LocalBoundingBox& bounds, const Mat4& world)
    {
        for (unsigned int i = 0; i < 8; ++i)
            corner[i].x = corner[i].y = corner[i].z = 0.0f;
        setBounds(bounds);
        transform(world);
    }

    void setBounds(const LocalBoundingBox& bounds)
    {
        for (unsigned int i = 0; i < 8; ++i)
        {
            corner[i].x = (i & 4u) ? bounds.m_max.x : bounds.m_min.x;
            corner[i].y = (i & 2u) ? bounds.m_max.y : bounds.m_min.y;
            corner[i].z = (i & 1u) ? bounds.m_max.z : bounds.m_min.z;
        }
    }

    void transform(const Mat4& world)
    {
        const float* const m = world.m;
        for (unsigned int i = 0; i < 8; ++i)
        {
            const float x = corner[i].x;
            const float y = corner[i].y;
            const float z = corner[i].z;
            const float tz = z * m[10] + x * m[2] + y * m[6] + m[14];
            const float ty = i == 0 ? x * m[1] + y * m[5] + z * m[9] + m[13]
                                    : x * m[1] + z * m[9] + y * m[5] + m[13];
            const float tx = i == 0 ? z * m[8] + x * m[0] + y * m[4] + m[12]
                                    : y * m[4] + z * m[8] + x * m[0] + m[12];
            corner[i].x = tx;
            corner[i].y = ty;
            corner[i].z = tz;
        }
    }
};

#if defined(_M_IX86)
static_assert(sizeof(BoxCorners) == 0x60, "the eight-corner box is 96 bytes (0x100CE120 / 0x100AE110)");
static_assert(sizeof(LocalBoundingBox) == 0x3C, "the patch bounds run +0x5C..+0x97");
static_assert(sizeof(Frustum) == 0x1E8, "a shadow record's frustum sits at +0x10 of 0x498 bytes");
static_assert(sizeof(Mat4) == 0x40, "an instance's transform runs +0x04..+0x43");
#endif
// The patch and template bounds are read as LocalBoundingBox: every architecture.
static_assert(sizeof(MeshBounds) == sizeof(LocalBoundingBox), "MeshBounds is LocalBoundingBox");

// PE 0x101C6A90 (433 B), read in full - the box against a frustum's six planes (thiscall on the box,
// `retn 4`): false as soon as one plane has ALL eight corners strictly in front of it (distance > 0,
// ordered; the eight are always all counted). Corner 0's distance is (z*n.z + x*n.x) + y*n.y + d,
// corners 1..7's (y*n.y + z*n.z) + x*n.x + d (101C6AA0..101C6AB4 against 101C6ACD..101C6AE2).
// StaticMesh.cpp's StaticMeshBoundsCorners::outside sums x, y, z for every corner; not reused.
bool boxCornersInsideFrustum(const BoxCorners& box, const Frustum& frustum)
{
    for (unsigned int plane = 0; plane < 6; ++plane)
    {
        const Plane3& p = frustum.m_planes[plane];
        int outside = 0;
        {
            const Vec3& c = box.corner[0];
            if (c.z * p.normal.z + c.x * p.normal.x + c.y * p.normal.y + p.d > 0.0f)
                outside += 1;
        }
        for (unsigned int i = 1; i < 8; ++i)
        {
            const Vec3& c = box.corner[i];
            if (c.y * p.normal.y + c.z * p.normal.z + c.x * p.normal.x + p.d > 0.0f)
                outside += 1;
        }
        if (outside == 8)
            return false;
    }
    return true;
}


// Corner distance used while extending the caster near plane; preserves z/y/x association.

float shadowCornerDistance(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dz * dz + dy * dy + dx * dx);
}

void fitShadowFrustumNearPlane(Frustum& frustum, const Vec3* corners)
{
    Plane3& plane = frustum.m_planes[4];                                    // +0x40
    const Vec3 normal = plane.normal;
    float maximum = -FLT_MAX;
    float maximumPlusD = -FLT_MAX;
    int best = -1;
    for (int i = 0; i < 8; ++i)
    {
        const Vec3& corner = corners[i];
        const float dot = corner.z * normal.z + corner.y * normal.y + normal.x * corner.x;
        if (dot > maximum)
        {
            best = i;
            maximumPlusD = dot + plane.d;
            maximum = dot;
        }
    }
    if (maximumPlusD >= 0.0f)
    {
        const Vec3& corner = corners[best];
        plane.d = -(normal.y * corner.y + normal.z * corner.z + normal.x * corner.x);
    }

    frustum.recalcCorners();
    const Vec3* const corner = frustum.m_corners;
    frustum.m_farDist = shadowCornerDistance(corner[4], corner[0]) - frustum.m_nearDist;   // +0x1E0
    Mat4 transformation = frustum.m_transformation;
    transformation.m[12] = (corner[0].x + corner[3].x) * 0.5f;
    transformation.m[13] = (corner[3].y + corner[0].y) * 0.5f;
    transformation.m[14] = (corner[3].z + corner[0].z) * 0.5f;
    frustum.transform(transformation);
}


// Moves the frustum toward the light, extending its far distance and transforming its planes.

void shiftShadowFrustumTowardLight(Frustum& frustum, float distance)
{
    Mat4 transformation = frustum.m_transformation;
    const Vec3 normal = frustum.m_planes[4].normal;
    const float x = normal.x * distance;
    const float y = normal.y * distance;
    const float z = normal.z * distance;
    transformation.m[12] = transformation.m[12] + x;
    transformation.m[13] = transformation.m[13] + y;
    transformation.m[14] = transformation.m[14] + z;
    frustum.m_farDist = frustum.m_farDist + distance;
    frustum.transform(transformation);
}

void lightManagerAttachShadowReceiver(LightManager* manager, void* geometry)
{
    Frustum shadowFrustum(lightManagerShadowFrustum(manager));              // +0x278, [ebp-0x274]
    shiftShadowFrustumTowardLight(shadowFrustum, lightManagerShadowPullbackDistance(manager));   // +0x16C
    {
        const Mat4* const transform = geometryTransformation(geometry);    // +0x48
        const float radius = geometryBoundingRadius(geometry);             // +0x44
        if (!shadowFrustum.inside(translationOf(*transform), radius))
            return;
    }

    std::vector<LightShadowRecord>& records = lightManagerShadowRecords(manager);   // +0x79C
    for (LightShadowRecord* record = records.data();
         record != records.data() + records.size(); ++record)
    {
        Frustum recordFrustum(record->frustum);                             // [ebp-0x45C]
        shiftShadowFrustumTowardLight(recordFrustum, lightManagerShadowPullbackDistance(manager));
        const Mat4* const transform = geometryTransformation(geometry);    // +0x48, again
        const float radius = geometryBoundingRadius(geometry);             // +0x44, again
        if (recordFrustum.inside(translationOf(*transform), radius))
        {
            geometryAttachShadowRecord(geometry, record);                  // +0x24
            geometryStampShadowFrame(geometry, dx9rend::rendererFrameOrdinal());   // +0x54, then +0xAC
            return;
        }
    }
}


// PE 0x1015E9E0 (83 B), read in full - a member of the unpublished base (thiscall on the OverGrowth):
// every draw type's two pools start the frame with nothing handed out - +0x38 (the "_lod" pool) and
// +0x88 (the main pool's +0x38) zeroed, type by type, the type vector re-read for each store.
// Its one caller is render (0x100CA5E4). [INFERRED] name.
void resetGeometryPools(OverGrowth* self)
{
    for (unsigned int index = 0; index < self->m_geometries.size(); ++index)
    {
        self->m_geometries[index].lod.poolUsed = 0;
        self->m_geometries[index].full.poolUsed = 0;
    }
}

// PE 0x1015EE40 (98 B), read in full - thiscall on a pool, `retn 4`: hand out the pool's next geometry
// for this instance. When every geometry the pool owns is already out (`cmp [esi+38h], eax; jnz` - an
// equality test against the vector's size, `sar 2`) the pool's template creates one more (+0x24,
// IGeometryTemplate::createInstance - no null test on the result) and it is pushed (0x1009ACE0, the
// push_back); then the geometry at +0x38 is taken, +0x38 advances, and the geometry is handed
// the address of the instance's transform (+0x50 with instance + 4). Callers: render (twice) and the
// shadow walk. [INFERRED] name.
void* acquireGeometry(OverGrowthGeometryRecord* pool, OverGrowthInstance* instance)
{
    std::vector<void*>& geometries = pool->pool;
    const unsigned int owned = static_cast<unsigned int>(geometries.size());
    if (pool->poolUsed == owned)
    {
        void* created = pool->geometryTemplate->createInstance();              // +0x24
        geometries.push_back(created);
    }
    const unsigned int used = pool->poolUsed;
    pool->poolUsed = used + 1;
    void* const geometry = geometries[used];
    geometrySetTransformation(geometry, &instance->transform);              // +0x50
    return geometry;
}

// StaticMesh +0x25C - the vegetation shadow value the draw hands each geometry (the instance's +0x54,
// stored as its bits). The pools' geometries are StaticMesh instances.
inline void geometrySetShadowValue(void* geometry, const OverGrowthInstance* instance)
{
    static_cast<StaticMesh*>(static_cast<GeometryInstanceData*>(geometry))->m_float25C = instance->shadow;
}


// ObjectDrawer primary slot +0x38 (PE 0x100F8220), called through the drawer's table exactly as the
// PE does (`mov ecx, dword_1025A39C; mov edx, [ecx]; push edi; call [edx+38h]` at 0x100CA7E5..0x100CA7EE,
// AL tested). Its body is ObjectDrawer::isGeometryOccluded (Geom/ObjectDrawer.cpp, the od_slot38
// thunk). Unguarded, as shipped.

inline bool objectDrawerOccludes(IBase* drawer, void* geometry)
{
    typedef bool (__thiscall* OccludedFn)(void*, void*);
    void** const table = *reinterpret_cast<void***>(drawer);
    return reinterpret_cast<OccludedFn>(table[0x38 / 4])(drawer, geometry);
}

// The host call. IObjectTemplate::getCullRadiusScale - MSVC slot 28 = +0x70 in this tree's table
// (probe_hostvtables.cpp.IObjectTemplate.layout.txt) = retail PE +0x70 (100CA721..100CA729): BF2.exe's
// object-template tables (0x8C3388, 0x8C34C8, 0x8C5A50, 0x8E5E48, ... - 200+ of them) all share the
// getter 0x536EFA `fld dword ptr [ecx+58h]; retn` at +0x70 beside its setter 0x536EED at +0x6C; the
// ELF's +0x74 (IObjectTemplate.h) is the same slot after the one-slot destructor shift. No STL crosses,
// so it is typed in both flavours (RW-266).
inline float objectTemplateCullRadiusScale(const void* objectTemplate)
{
    return static_cast<const world::IObjectTemplate*>(objectTemplate)->getCullRadiusScale();
}

} // namespace

// Updates visible vegetation cells, culls instances and draws their main/LOD cross-fade.
// view and the initialized geometry/light services must be valid; time is the frame delta.
// Main-pool geometry is acquired before culling. LOD geometry draws first and is not
// registered as a directional-shadow receiver.
void OverGrowth::render(IRenderView* view, float time)
{
    if (m_path.empty())
        return;
    if (m_updateBeforeRender)
        OverGrowth::update(view);                                           // 100CA5A2, direct
    else
        overGrowthBaseUpdateCells(this, view);                              // 100CA5AE, direct
    if (!m_enabled)
        return;
    if (!m_patches)
        return;
    if (!m_drawNear)
        return;

    (void)view->getViewMatrixPtr();                                         // 100CA5DF +0x64, discarded
    resetGeometryPools(this);
    (void)view->getFrustumPtr();                                            // 100CA5ED +0x74, discarded

    for (int row = 0; row < m_gridSize; ++row)
    {
        for (int column = 0; column < m_gridSize; ++column)
        {
            const OverGrowthCell* const cell = m_cells[column + row * m_gridSize];
            const int patchesX = m_patchCountX;
            const int patchesZ = m_patchCountZ;
            const int x = patchesX / 2 + cell->cellX;
            const int z = patchesZ / 2 + cell->cellZ;
            if (x < 0 || z < 0 || x >= patchesX || z >= patchesZ)
                continue;
            OverGrowthPatch* const patch = m_patches + (x + z * patchesX);
            const float reach = (m_closeDistFade + 1.0f) * m_closeDistance;
            if (!reinterpret_cast<const LocalBoundingBox&>(patch->bounds).intersectSphere(
                    m_cameraPosition, reach))
                continue;

            for (unsigned int index = 0; index < patch->instances.size(); ++index)
            {
                OverGrowthInstance* const instance = &patch->instances[index];
                OverGrowthGeometryPair* const type = m_geometries.data() + instance->geometryIndex;
                if (!g_overGrowthDrawObjectInstances && instance->generated)
                    continue;

                const float drawRadius =
                    objectTemplateCullRadiusScale(type->full.objectTemplate) * instance->cullRadius
                    + m_viewDistanceScale * m_closeDistance * type->full.distanceScale;
                const float ratio = distanceSquared(translationOf(instance->transform), m_cameraPosition)
                                  / (drawRadius * drawRadius);
                if (!(ratio < 1.0f))                                        // `test ah,5; jp`
                    continue;

                void* const geometry = acquireGeometry(&type->full, instance);
                {
                    const Mat4* const transform = geometryTransformation(geometry);  // +0x48
                    const float radius = geometryBoundingRadius(geometry);           // +0x44
                    const Frustum* const frustum = view->getFrustumPtr();            // +0x74
                    // [CHOSEN] initialised: 0x10079B40 writes it on every true answer and it is read
                    // only after one, so the PE's unset byte is never observed.
                    bool fullyInside = false;
                    if (!frustumContainsSphere(*frustum, translationOf(*transform), radius, fullyInside))
                        continue;
                    if (!fullyInside)
                    {
                        const Mat4* const boxTransform = geometryTransformation(geometry);
                        const Frustum* const boxFrustum = view->getFrustumPtr();
                        const LocalBoundingBox& bounds = geometryLocalBounds(geometry);
                        if (!boxIntersectsFrustum(bounds, *boxFrustum, *boxTransform, 5))
                            continue;
                    }
                }
                if (objectDrawerOccludes(dx9rend::g_ObjectDrawer, geometry))
                    continue;
                lightManagerAttachShadowReceiver(g_LightManager, geometry);

                float fade = clampFloat((1.0f - std::sqrt(ratio)) / m_closeDistFade
                                        * std::sqrt(type->full.distanceScale), 0.0f, 1.0f);
                if (m_lodTimer > 0.0f)                                       // 100CA83B `test ah,41h`
                {
                    if (fade < 1.0f)                                         // 100CA852 `test ah,5; jp`
                    {
                        const float rate = 1.0f / m_lodTimer;
                        float& state = instance->lodFadeState;
                        if (fade > 0.5f)                                    // 100CA876 `test ah,41h`
                            state = maxFloat(0.0f, minFloat(state - rate * time, 2.0f - (fade + fade)));
                        else
                            state = minFloat(1.0f, maxFloat(rate * time + state, 1.0f - (fade + fade)));
                    }
                    else
                    {
                        instance->lodFadeState = 0.0f;
                    }
                    fade = 1.0f - instance->lodFadeState;
                }

                if (type->lod.geometry && fade < 1.0f)
                {
                    const float alpha = alphaToCoverageAdjusted(
                        clampFloat((1.0f - fade) + (1.0f - fade), 0.0f, 1.0f));
                    void* const lodGeometry = acquireGeometry(&type->lod, instance);
                    geometrySetDistanceFade(lodGeometry, alpha);
                    geometrySetShadowValue(lodGeometry, instance);
                    geometryDraw(lodGeometry, view);                                   // +0x18
                }
                if (fade > 0.0f)                                             // 100CA984 `test ah,41h`
                {
                    const float alpha = alphaToCoverageAdjusted(
                        clampFloat(fade + fade, 0.0f, 1.0f));
                    geometrySetDistanceFade(geometry, alpha);
                    geometrySetShadowValue(geometry, instance);
                    geometryDraw(geometry, view);                                      // +0x18
                }
            }
        }
    }
}


// Appends vegetation intersecting the directional shadow volumes and expands caster bounds.
// Uses LOD geometry when available. Pool cursors are shared with drawing and reset only by
// render, so this pass must run before the frame's vegetation draw resets those cursors.

void OverGrowth::collectShadowGeometry(const std::vector<LightShadowRecord>* records,
                                       const Frustum* lightFrustum, Frustum* casterFrustumPointer,
                                       std::vector<void*>* geometries)
{
    if (!m_enabled)
        return;
    if (!m_patches)
        return;
    if (!m_drawNear)
        return;
    const std::vector<LightShadowRecord>& shadowRecords = *records;
    if (shadowRecords.size() == 0)
        return;
    if (m_gridSize <= 0)
        return;

    Frustum& casterFrustum = *casterFrustumPointer;
    std::vector<void*>& output = *geometries;

    for (int row = 0; row < m_gridSize; ++row)
    {
        for (int column = 0; column < m_gridSize; ++column)
        {
            const OverGrowthCell* const cell = m_cells[column + row * m_gridSize];
            const int patchesX = m_patchCountX;
            const int patchesZ = m_patchCountZ;
            const int x = patchesX / 2 + cell->cellX;
            const int z = patchesZ / 2 + cell->cellZ;
            if (x < 0 || z < 0 || x >= patchesX || z >= patchesZ)
                continue;
            OverGrowthPatch* const patch = m_patches + (x + z * patchesX);
            if (!terrainFrustumIntersectsBox(lightFrustum, &patch->bounds.min.x))
                continue;

            for (unsigned int index = 0; index < patch->instances.size(); ++index)
            {
                OverGrowthInstance* const instance = &patch->instances[index];
                OverGrowthGeometryPair* const type = m_geometries.data() + instance->geometryIndex;
                const StaticMeshTemplate* const lodTemplate =
                    static_cast<const StaticMeshTemplate*>(type->lod.geometryTemplate);
                const BoxCorners box(reinterpret_cast<const LocalBoundingBox&>(lodTemplate->m_bounds),
                                     instance->transform);

                bool held = false;
                for (const LightShadowRecord* record = shadowRecords.data();
                     record != shadowRecords.data() + shadowRecords.size(); ++record)
                {
                    if (boxCornersInsideFrustum(box, record->frustum))
                    {
                        held = true;
                        break;
                    }
                }
                if (!held)
                    continue;

                OverGrowthGeometryRecord* const pool = type->lod.geometry ? &type->lod : &type->full;
                void* geometry = acquireGeometry(pool, instance);
                output.push_back(geometry);                                 // 0x1009A340
                fitShadowFrustumNearPlane(casterFrustum, box.corner);
            }
        }
    }
}

} } }
#endif