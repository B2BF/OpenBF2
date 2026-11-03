// Path:     [INFERRED] beside the other dice::hfe math value types.
// Contents: minimal recovered layout used by world::Object's cached bounds.

// The type name and 0x3C size are symbol-attested in bf2.i64.  Both shipped ports initialise the
// six scalar endpoints/radii, the derived size/centre vectors, and the dirty byte exactly as below.
// Member spelling and access remain [INFERRED]; their offsets and semantics are cross-port proven.

#ifndef MATH_LOCALBOUNDINGBOX_H
#define MATH_LOCALBOUNDINGBOX_H

#include <cmath>
#include "Vec3.h"
#include "Mat4.h"       // isInside / testIntersectWithOBB

namespace dice {
namespace hfe {

struct Ray3;
class Frustum;
class BoundingBox;

struct LocalBoundingBox
{
    LocalBoundingBox()
    {
        m_min.x = m_min.y = m_min.z = 0.0f;
        m_max.x = m_max.y = m_max.z = 0.0f;
        m_radiusFromOrigin = 0.0f;
        m_radiusFromCentre = 0.0f;
        m_derivedDataDirty = true;
        m_size.x = m_size.y = m_size.z = 0.0f;
        m_centre.x = m_centre.y = m_centre.z = 0.0f;
    }

    // ELF 0x4983E0. Recomputes center, size and corner-derived radii, then clears the dirty flag.
    void calcRadius()
    {
        float maxOrigin = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float x = (i & 1) ? m_max.x : m_min.x;
            const float y = (i & 2) ? m_max.y : m_min.y;
            const float z = (i & 4) ? m_max.z : m_min.z;
            const float d = x * x + y * y + z * z;
            if (d > maxOrigin) maxOrigin = d;
        }
        m_radiusFromOrigin = sqrtf(maxOrigin);
        m_centre.x = (m_min.x + m_max.x) * 0.5f;
        m_centre.y = (m_min.y + m_max.y) * 0.5f;
        m_centre.z = 0.5f * (m_min.z + m_max.z);
        float maxCentre = 0.0f;
        for (int i = 0; i < 8; ++i)
        {
            const float x = ((i & 1) ? m_max.x : m_min.x) - m_centre.x;
            const float y = ((i & 2) ? m_max.y : m_min.y) - m_centre.y;
            const float z = ((i & 4) ? m_max.z : m_min.z) - m_centre.z;
            const float d = z * z + y * y + x * x;
            if (d > maxCentre) maxCentre = d;
        }
        m_radiusFromCentre = sqrtf(maxCentre);
        m_size.x = m_max.x - m_min.x;
        m_size.y = m_max.y - m_min.y;
        m_size.z = m_max.z - m_min.z;
        m_derivedDataDirty = false;
    }

    // ELF 0x497590. Tests squared point-to-box distance against squared radius, visiting x, y, then z.
    bool intersectSphere(const Vec3& centre, float radius) const
    {
        float d2 = 0.0f;
        const float* p = &centre.x;
        const float* lo = &m_min.x;
        const float* hi = &m_max.x;
        for (int i = 0; i < 3; ++i)
        {
            if (lo[i] > p[i])
                d2 += (p[i] - lo[i]) * (p[i] - lo[i]);
            else if (p[i] > hi[i])
                d2 += (p[i] - hi[i]) * (p[i] - hi[i]);
        }
        return radius * radius >= d2;
    }

    // ELF 0x495DA0. Expands max before min on each axis, then marks derived bounds dirty.
    void expand(const Vec3& p)
    {
        if (p.x > m_max.x) m_max.x = p.x;
        if (m_min.x > p.x) m_min.x = p.x;
        if (p.y > m_max.y) m_max.y = p.y;
        if (m_min.y > p.y) m_min.y = p.y;
        if (p.z > m_max.z) m_max.z = p.z;
        if (m_min.z > p.z) m_min.z = p.z;
        m_derivedDataDirty = true;              // byte +0x20
    }
    void expand(float x, float y, float z)
    {
        if (x > m_max.x) m_max.x = x;
        if (m_min.x > x) m_min.x = x;
        if (y > m_max.y) m_max.y = y;
        if (m_min.y > y) m_min.y = y;
        if (z > m_max.z) m_max.z = z;
        if (m_min.z > z) m_min.z = z;
        m_derivedDataDirty = true;
    }

    void expand(const BoundingBox& box);
    // Tests transformed corners against selected planes; both pointers must be valid even with mask zero.
    bool inside(const Frustum* frustum, const Mat4* transform, unsigned char mask) const;
    bool inside(const Frustum& frustum) const;
    // Returns the largest cached size's axis (x=0, y=1, z=2), recalculating when dirty.
    // Ties favor the earlier axis.
    int getMajorAxis() const;
    bool isInside(const Vec3& point, const Vec3& ownerPosition, const Mat4& ownerInverse) const;

    // Returns size, recalculating derived bounds when dirty.
    const Vec3& getSize()
    {
        if (m_derivedDataDirty)
            calcRadius();
        return m_size;
    }

    // ELF 0x4982E0; engine ELF 0x83B4AA0.
    // Tests the world-space sphere center against this box expanded by radius in the owner's local frame.
    bool isInside(const Vec3& centre, float radius, const Vec3& ownerPosition,
                  const Mat4& ownerInverse) const
    {
        const float dx = centre.x - ownerPosition.x;
        const float dy = centre.y - ownerPosition.y;
        const float dz = centre.z - ownerPosition.z;
        const float* const m = ownerInverse.m;
        const float lx = m[4] * dy + m[0] * dx + m[8]  * dz;
        const float ly = m[9] * dz + (m[5] * dy + m[1] * dx);
        const float lz = dz * m[10] + dy * m[6] + dx * m[2];
        if (!(lx >= m_min.x - radius)) return false;
        if (!(m_max.x + radius >= lx)) return false;
        if (!(ly >= m_min.y - radius)) return false;
        if (!(m_max.y + radius >= ly)) return false;
        if (!(lz >= m_min.z - radius)) return false;
        return radius + m_max.z >= lz;
    }

    // Separating-axis test under the supplied transforms; refreshes dirty bounds.
    // [PLATFORM] ELF applies signed grow to this box's tested centre; PE ignores grow.
    bool testIntersectWithOBB(const Mat4& thisTransform, const LocalBoundingBox& other,
                              const Mat4& otherTransform, const Vec3& grow) const;

    // Returns the nearest forward entering hit; rays starting inside do not hit.
    // Optional normal is the entry face's transformed outward axis. The first overload uses identity.
    bool intersectRay(const Ray3& ray, Vec3& position, Vec3* normal) const;
    bool intersectRay(const Ray3& ray, const Mat4& transform,
                      Vec3& position, Vec3* normal) const;

    Vec3 m_min;                 // +0x00
    Vec3 m_max;                 // +0x0C
    float m_radiusFromOrigin;   // +0x18
    float m_radiusFromCentre;   // +0x1C
    bool m_derivedDataDirty;    // +0x20
    Vec3 m_size;                // +0x24
    Vec3 m_centre;              // +0x30
};

static_assert(sizeof(LocalBoundingBox) == 0x3C, "LocalBoundingBox layout drift");

} // namespace hfe
} // namespace dice

#endif // MATH_LOCALBOUNDINGBOX_H
