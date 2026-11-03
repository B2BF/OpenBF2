// Frustum setup and intersection queries.

#include "Frustum.h"
#include "../Core/Debug.h"

#include <cfloat>
#include <cmath>
#include <cstring>
// static init: ELF 0x495CF0, 0x495D50, 0x495D80 (std::__ioinit).
#include <iostream>

namespace dice {
namespace hfe {

namespace {
// The normalize-in-place the ELF inlines at every plane (|n|^2 within eps of 1 -> untouched;
// degenerate -> zero).
inline void normalizePlaneNormal(Vec3& n)
{
    const float lengthSq = n.z * n.z + n.y * n.y + n.x * n.x;
    if (std::fabs(lengthSq - 1.0f) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) >= 1.1920929e-07f)
        {
            const float inv = 1.0f / std::sqrt(lengthSq);
            n.x *= inv; n.y *= inv; n.z *= inv;
        }
        else
        {
            n.x = 0.0f; n.y = 0.0f; n.z = 0.0f;
        }
    }
}
} // namespace

// ELF 0x1A0D00.
// Initializes plane normals, corners, bounds, centre, and identity transform; marks derived data dirty.
// Leaves plane distances, outcodes, radii, and projection scalars untouched.
Frustum::Frustum()
{
    for (int i = 0; i < 6; ++i)
    {
        m_planes[i].normal.x = m_planes[i].normal.y = m_planes[i].normal.z = 0.0f;
    }
    for (int i = 0; i < 8; ++i)
    {
        m_corners[i].x = m_corners[i].y = m_corners[i].z = 0.0f;
        m_prevCorners[i].x = m_prevCorners[i].y = m_prevCorners[i].z = 0.0f;
    }
    // the LocalBoundingBox: min/max/radii 0, dirty = 1, size/centre 0 - its own ctor's values
    for (int i = 0; i < 16; ++i)
        m_transformation.m[i] = 0.0f;
    m_transformation.m[15] = 1.0f;
    m_transformation.m[10] = 1.0f;
    m_transformation.m[5]  = 1.0f;
    m_transformation.m[0]  = 1.0f;
    m_center.x = m_center.y = m_center.z = 0.0f;
    m_cornersDirty = true;                                       // byte +0x1C8 = 1
    m_type = 2;                                                  // +0x1E4
}

// ELF 0x494130. Copies setup parameters and transformation, then rebuilds derived planes and corners.
Frustum& Frustum::operator=(const Frustum& other)
{
    m_fov        = other.m_fov;
    m_aspect     = other.m_aspect;
    m_dword1D4   = other.m_dword1D4;
    m_dword1D8   = other.m_dword1D8;
    m_nearDist   = other.m_nearDist;
    m_farDist    = other.m_farDist;
    m_type       = other.m_type;
    m_transformation = other.m_transformation;
    reSetup();
    return *this;
}

// ELF 0x494270.
void Frustum::setPerspective(float fov, float aspect, float nearDist, float farDist, const Mat4& m)
{
    m_fov = fov;                                                 // +0x1CC
    const float halfFov = fov * 0.5f;
    m_aspect = aspect;                                           // +0x1D0
    m_nearDist = nearDist;                                       // +0x1DC
    m_type = 0;                                                  // +0x1E4
    m_farDist = farDist;                                         // +0x1E0
    const float halfFovA = halfFov / aspect;
    const float sinA = std::sin(halfFovA), cosA = std::cos(halfFovA);
    const float sinB = std::sin(halfFov),  cosB = std::cos(halfFov);

    // planes 4 / 5: near and far, in view space
    m_planes[4].normal.x = 0.0f; m_planes[4].normal.y = 0.0f; m_planes[4].normal.z = -1.0f;
    m_planes[4].d = -(-1.0f * nearDist + 0.0f);
    m_planes[5].normal.x = 0.0f; m_planes[5].normal.y = 0.0f; m_planes[5].normal.z = 1.0f;
    m_planes[5].d = -(farDist + 0.0f);

    // planes 0..3: the four side planes, each normalized in place, d = -(n . origin) = 0
    m_planes[0].normal.x = -cosA; m_planes[0].normal.y = 0.0f;  m_planes[0].normal.z = -sinA;
    normalizePlaneNormal(m_planes[0].normal);
    m_planes[0].d = -(m_planes[0].normal.y * 0.0f + m_planes[0].normal.x * 0.0f + m_planes[0].normal.z * 0.0f);
    m_planes[1].normal.x = cosA;  m_planes[1].normal.y = 0.0f;  m_planes[1].normal.z = -sinA;
    normalizePlaneNormal(m_planes[1].normal);
    m_planes[1].d = -(m_planes[1].normal.y * 0.0f + m_planes[1].normal.x * 0.0f + m_planes[1].normal.z * 0.0f);
    m_planes[2].normal.x = 0.0f;  m_planes[2].normal.y = cosB;  m_planes[2].normal.z = -sinB;
    normalizePlaneNormal(m_planes[2].normal);
    m_planes[2].d = -(m_planes[2].normal.y * 0.0f + m_planes[2].normal.x * 0.0f + m_planes[2].normal.z * 0.0f);
    m_planes[3].normal.x = 0.0f;  m_planes[3].normal.y = -cosB; m_planes[3].normal.z = -sinB;
    normalizePlaneNormal(m_planes[3].normal);
    m_planes[3].d = -(m_planes[3].normal.y * 0.0f + m_planes[3].normal.x * 0.0f + m_planes[3].normal.z * 0.0f);

    // into world space, and the per-plane sign outcodes
    for (int i = 0; i < 6; ++i)
    {
        m_planes[i] = Plane3::transform(m_planes[i], m);
        // ELF83B11F3 / PE1007A67A extract sign bits, including negative zero
        // and signed NaNs. Floating comparisons lose that shipped behavior.
        unsigned int bits[3];
        std::memcpy(bits, &m_planes[i].normal, sizeof(bits));
        m_planeOutcodes[i] = static_cast<int>((bits[0] >> 31)
            | ((bits[1] >> 30) & 2u) | ((bits[2] >> 29) & 4u));
    }
    m_transformation = m;                                        // +0x174..+0x1B0
    // the centre: the matrix applied to (0, 0, (near + far) / 2)
    const float cy = 0.0f;
    const float cz = (nearDist + farDist) * 0.5f;
    m_center.x = m.m[4] * cy + m.m[0] * 0.0f + m.m[8] * cz + m.m[12];
    m_center.y = m.m[9] * cz + m.m[5] * cy + m.m[1] * 0.0f + m.m[13];
    m_center.z = cz * m.m[10] + cy * m.m[6] + 0.0f * m.m[2] + m.m[14];
    updateSecondarySizes();
}

// ELF 0x494A20; engine ELF 0x83B1500 / client PE6DD4C0 / renderer PE1007A7F0. Width/height and distance
// stores, six axial planes, native plane transform/sign masks, matrix and centre,
// then the same secondary-size recompute as perspective. FOV/aspect stay untouched.
void Frustum::setOrtho(float width, float height, float nearDist, float farDist, const Mat4& m)
{
    m_dword1D4 = width;
    m_nearDist = nearDist;
    m_farDist = farDist;
    m_dword1D8 = height;
    m_type = 1;
    m_planes[0].normal = {-1.0f, 0.0f, 0.0f};
    m_planes[0].d = -((-width * 0.5f) * -1.0f + 0.0f + 0.0f);
    m_planes[1].normal = {1.0f, 0.0f, 0.0f};
    m_planes[1].d = -(width * 0.5f + 0.0f + 0.0f);
    m_planes[3].normal = {0.0f, -1.0f, 0.0f};
    m_planes[3].d = -(0.0f + (-height * 0.5f) * -1.0f + 0.0f);
    m_planes[2].normal = {0.0f, 1.0f, 0.0f};
    m_planes[2].d = -(0.0f + height * 0.5f + 0.0f);
    m_planes[4].normal = {0.0f, 0.0f, -1.0f};
    m_planes[4].d = -(0.0f + nearDist * -1.0f);
    m_planes[5].normal = {0.0f, 0.0f, 1.0f};
    m_planes[5].d = -(0.0f + farDist);
    for (int i = 0; i < 6; ++i) {
        m_planes[i] = Plane3::transform(m_planes[i], m);
        unsigned int bits[3];
        std::memcpy(bits, &m_planes[i].normal, sizeof(bits));
        m_planeOutcodes[i] = static_cast<int>((bits[0] >> 31)
            | ((bits[1] >> 30) & 2u) | ((bits[2] >> 29) & 4u));
    }
    m_transformation = m;
    // Retail writes these provisional radius values before updateSecondarySizes
    // overwrites them from the corners. Keep both writes.
    // PE6DD675..714 retains the midpoint through its square, and each centre
    // transform chain through its final store. Float arithmetic can overflow
    // before halving or erase a product's low bits before its translation.
#if defined(_MSC_VER) && defined(_M_IX86)
    const float half = 0.5f;
    float* radius = &m_boundingRadius;
    float* radiusSquared = &m_boundingRadiusSq;
    float* center = &m_center.x;
    const float* matrix = m.m;
    __asm {
        fld nearDist
        fadd farDist
        fmul half
        mov ecx, radius
        fst dword ptr [ecx]
        fld st(0)
        fmul st(0), st(1)
        mov ecx, radiusSquared
        fstp dword ptr [ecx]
        mov edx, center
        mov dword ptr [edx], 0
        mov dword ptr [edx+4], 0
        fstp dword ptr [edx+8]
        mov eax, matrix
        fld dword ptr [eax+40]
        fmul dword ptr [edx+8]
        fld dword ptr [eax+24]
        fmul dword ptr [edx+4]
        faddp st(1), st(0)
        fld dword ptr [eax+8]
        fmul dword ptr [edx]
        faddp st(1), st(0)
        fadd dword ptr [eax+56]
        fld dword ptr [eax+36]
        fmul dword ptr [edx+8]
        fld dword ptr [eax+20]
        fmul dword ptr [edx+4]
        faddp st(1), st(0)
        fld dword ptr [eax+4]
        fmul dword ptr [edx]
        faddp st(1), st(0)
        fadd dword ptr [eax+52]
        fld dword ptr [eax]
        fmul dword ptr [edx]
        fld dword ptr [eax+16]
        fmul dword ptr [edx+4]
        faddp st(1), st(0)
        fld dword ptr [eax+32]
        fmul dword ptr [edx+8]
        faddp st(1), st(0)
        fadd dword ptr [eax+48]
        fstp dword ptr [edx]
        fstp dword ptr [edx+4]
        fstp dword ptr [edx+8]
    }
#else
    const long double midpoint = (static_cast<long double>(nearDist) + farDist) * 0.5L;
    m_boundingRadius = static_cast<float>(midpoint);
    m_boundingRadiusSq = static_cast<float>(midpoint * midpoint);
    m_center.x = m_center.y = 0.0f;
    m_center.z = static_cast<float>(midpoint);
    const long double x = m_center.x, y = m_center.y, z = m_center.z;
    m_center.z = static_cast<float>(m.m[10] * z + m.m[6] * y + m.m[2] * x + m.m[14]);
    m_center.y = static_cast<float>(m.m[9] * z + m.m[5] * y + m.m[1] * x + m.m[13]);
    m_center.x = static_cast<float>(m.m[0] * x + m.m[4] * y + m.m[8] * z + m.m[12]);
#endif
    updateSecondarySizes();
}

// ELF 0x494F20; engine ELF 0x83B1960.
void Frustum::expandToEncapsulate(const Frustum& other)
{
    for (int i = 0; i < 6; ++i)
    {
        Plane3& plane = m_planes[i];
        float maximum = -FLT_MAX;
        float minimum = FLT_MAX;
        for (int j = 0; j < 8; ++j)
        {
            const Vec3& corner = other.m_corners[j];
            const float projection = plane.normal.x * corner.x + plane.normal.y * corner.y
                                   + plane.normal.z * corner.z;
            maximum = maximum > projection ? maximum : projection;
            minimum = minimum > projection ? projection : minimum;
        }
        // Shipped behavior replaces the distance, without negating the chosen projection.
        plane.d = plane.d > 0.0f ? maximum : minimum;
    }
    const auto pairLength = [this](int a, int b) {
        const Plane3& p = m_planes[a];
        const Plane3& q = m_planes[b];
        const float x = p.d * p.normal.x + q.d * q.normal.x;
        const float y = p.d * p.normal.y + q.d * q.normal.y;
        const float z = p.d * p.normal.z + q.d * q.normal.z;
        return std::sqrt(x * x + y * y + z * z);
    };
    m_dword1D4 = pairLength(0, 1);
    m_dword1D8 = pairLength(2, 3);
    m_farDist = pairLength(5, 4);
    m_transformation.m[12] -= m_planes[4].d * m_planes[4].normal.x;
    m_transformation.m[13] -= m_planes[4].d * m_planes[4].normal.y;
    m_transformation.m[14] -= m_planes[4].d * m_planes[4].normal.z;
    reSetup();
}

// ELF 0x4954B0; engine ELF 0x83B1EA0 / PE6DD730: unknown type values intentionally do nothing.
void Frustum::reSetup()
{
    if (m_type == 0)
        setPerspective(m_fov, m_aspect, m_nearDist, m_farDist, m_transformation);
    else if (m_type == 1)
        setOrtho(m_dword1D4, m_dword1D8, m_nearDist, m_farDist, m_transformation);
}

// ELF 0x495630. A store followed by the existing setup path.
void Frustum::setFov(float fov)
{
    m_fov = fov;
    reSetup();
}

// ELF 0x495560. Replace, rather than compose, the entire transformation.
void Frustum::transform(const Mat4& transformation)
{
    m_transformation = transformation;
    reSetup();
}

// ELF 0x4956B0; engine ELF 0x83B2050 / PE6DD7F0: unconditional store followed by reSetup.
void Frustum::setFarDist(float distance)
{
    m_farDist = distance;
    reSetup();
}

// ELF 0x495670; engine ELF 0x83B2020.
void Frustum::setNearDist(float distance)
{
    m_nearDist = distance;
    reSetup();
}

// ELF 0x4956E0; engine ELF 0x83B2070.
const Vec3& Frustum::getCornerForPlane(unsigned int corner, FrustumPlane plane) const
{
    static const unsigned int corners[6][4] = {
        {5, 4, 0, 1}, {3, 2, 6, 7}, {0, 4, 6, 2},
        {5, 7, 3, 1}, {1, 0, 2, 3}, {7, 6, 4, 5}
    };
    if (static_cast<unsigned int>(plane) < 6 && corner < 4)
        return m_corners[corners[static_cast<unsigned int>(plane)][corner]];
    Debug(DebugTypeError, "Math/Frustum.cpp", 430, "") << "Invalid corner or plane.";
    return m_corners[1];
}

// ELF 0x498760; engine ELF 0x83B4EE0. This inline was emitted in LocalBoundingBox.cpp.
unsigned int Frustum::getOutcode(const Vec3& p) const
{
    unsigned int result = 0;
    const int order[6] = {5, 4, 1, 0, 2, 3};
    for (int i = 0; i < 6; ++i)
    {
        const int index = order[i];
        const Plane3& plane = m_planes[index];
        if (plane.normal.x * p.x + plane.normal.y * p.y + plane.normal.z * p.z + plane.d > 0.0f)
            result |= 1u << index;
    }
    return result;
}

// ELF 0x495920. Returns whether the point is inside the frustum.
bool Frustum::inside(const Vec3& p) const
{
    const Plane3& p5 = m_planes[5];
    if (p5.normal.z * p.z + p5.normal.y * p.y + p5.normal.x * p.x + p5.d > 0.0f)
        return false;
    const Plane3& p4 = m_planes[4];
    if (p4.normal.z * p.z + p4.normal.y * p.y + p4.normal.x * p.x + p4.d > 0.0f)
        return false;
    const Plane3& p1 = m_planes[1];
    if (p1.normal.z * p.z + p1.normal.y * p.y + p1.normal.x * p.x + p1.d > 0.0f)
        return false;
    const Plane3& p0 = m_planes[0];
    if (p0.normal.z * p.z + p0.normal.y * p.y + p0.normal.x * p.x + p0.d > 0.0f)
        return false;
    const Plane3& p2 = m_planes[2];
    if (p2.normal.z * p.z + p2.normal.y * p.y + p2.normal.x * p.x + p2.d > 0.0f)
        return false;
    const Plane3& p3 = m_planes[3];
    return !(p.z * p3.normal.z + p.y * p3.normal.y + p.x * p3.normal.x + p3.d > 0.0f);
}


// ELF 0x495A30. Tests far, near, right, left, top, bottom in that order; reject when n.p + d exceeds radius.
// Preserve early-return order for NaNs and floating-point exceptions.
bool Frustum::inside(const Vec3& p, float radius) const
{
    const Plane3& p5 = m_planes[5];
    if (p5.normal.z * p.z + p5.normal.y * p.y + p5.normal.x * p.x + p5.d > radius)
        return false;
    const Plane3& p4 = m_planes[4];
    if (p4.normal.z * p.z + p4.normal.y * p.y + p4.normal.x * p.x + p4.d > radius)
        return false;
    const Plane3& p1 = m_planes[1];
    if (p1.normal.z * p.z + p1.normal.y * p.y + p1.normal.x * p.x + p1.d > radius)
        return false;
    const Plane3& p0 = m_planes[0];
    if (p0.normal.z * p.z + p0.normal.y * p.y + p0.normal.x * p.x + p0.d > radius)
        return false;
    const Plane3& p2 = m_planes[2];
    if (p2.normal.z * p.z + p2.normal.y * p.y + p2.normal.x * p.x + p2.d > radius)
        return false;
    const Plane3& p3 = m_planes[3];
    return p3.normal.z * p.z + p3.normal.y * p.y + p3.normal.x * p.x + p3.d <= radius;
}


// ELF 0x495B30. Tests a sphere and, on success, writes whether it lies fully inside the frustum.
// fullyInside is unchanged on false. Its sign-bit test includes negative zero; preserve plane evaluation order.

bool Frustum::inside(const Vec3& p, float radius, bool& fullyInside) const
{
    const Plane3& pl4 = m_planes[4];
    const float d4 = pl4.normal.x * p.x + pl4.normal.y * p.y + pl4.normal.z * p.z + pl4.d;
    if (d4 > radius)
        return false;

    const Plane3& pl5 = m_planes[5];
    const float d5 = pl5.normal.x * p.x + pl5.normal.y * p.y + pl5.normal.z * p.z + pl5.d;
    if (d5 > radius)
        return false;

    const Plane3& pl0 = m_planes[0];
    const float d0 = pl0.normal.x * p.x + pl0.normal.y * p.y + pl0.normal.z * p.z + pl0.d;
    if (d0 > radius)
        return false;

    const Plane3& pl1 = m_planes[1];
    const float d1 = pl1.normal.x * p.x + pl1.normal.y * p.y + pl1.normal.z * p.z + pl1.d;
    if (d1 > radius)
        return false;

    const Plane3& pl2 = m_planes[2];
    const float d2 = pl2.normal.x * p.x + pl2.normal.y * p.y + pl2.normal.z * p.z + pl2.d;
    if (d2 > radius)
        return false;

    const Plane3& pl3 = m_planes[3];
    const float d3 = pl3.normal.x * p.x + pl3.normal.y * p.y + pl3.normal.z * p.z + pl3.d;
    if (d3 > radius)
        return false;

    // The six sums, ANDed as bit patterns; bit 31 survives only if every one of them is negative.
    const float sums[6] = { d0 + radius, d1 + radius, d4 + radius,
                            d5 + radius, d2 + radius, d3 + radius };
    unsigned int all = 0xFFFFFFFFu;
    for (int i = 0; i < 6; ++i)
    {
        unsigned int bits;
        std::memcpy(&bits, &sums[i], sizeof(bits));
        all &= bits;
    }
    fullyInside = (all >> 31) != 0;
    return true;
}

// ELF 0x494D80. The corners, the bounding radius (max |corner - centre|), the corners copied
// to the second set, the dirty flag cleared.
void Frustum::updateSecondarySizes()
{
    recalcCorners();
    m_boundingRadiusSq = 0.0f;                                   // +0x1C4
    float best = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        const float dx = m_center.x - m_corners[i].x;
        const float dy = m_center.y - m_corners[i].y;
        const float dz = m_center.z - m_corners[i].z;
        const float d = dz * dz + dy * dy + dx * dx;
        if (d > best)
        {
            m_boundingRadiusSq = d;
            best = d;
        }
    }
    m_boundingRadius = std::sqrt(best);                          // +0x1C0
    for (int i = 7; i >= 0; --i)
        m_prevCorners[i] = m_corners[i];
    m_cornersDirty = false;                                      // byte +0x1C8
}

// ELF 0x495220. The eight three-plane intersections, then the box from corners 0 and 1,
// expanded by 2..7 (LocalBoundingBox::expand - dirty set).
void Frustum::recalcCorners()
{
    m_corners[0] = intersect(m_planes[0], m_planes[2], m_planes[4]);
    m_corners[1] = intersect(m_planes[0], m_planes[3], m_planes[4]);
    m_corners[2] = intersect(m_planes[1], m_planes[2], m_planes[4]);
    m_corners[3] = intersect(m_planes[1], m_planes[3], m_planes[4]);
    m_corners[4] = intersect(m_planes[0], m_planes[2], m_planes[5]);
    m_corners[5] = intersect(m_planes[0], m_planes[3], m_planes[5]);
    m_corners[6] = intersect(m_planes[1], m_planes[2], m_planes[5]);
    m_corners[7] = intersect(m_planes[1], m_planes[3], m_planes[5]);
    m_boundingBox.m_min = m_corners[0];
    m_boundingBox.m_max = m_corners[1];
    m_boundingBox.m_derivedDataDirty = true;                    // byte +0x140 (320)
    for (int i = 2; i < 8; ++i)
        m_boundingBox.expand(m_corners[i]);
}

} // namespace hfe
} // namespace dice
