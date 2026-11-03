// Path:     [INFERRED] beside LocalBoundingBox.h and the other dice::hfe math value types.
// Contents: LocalBoundingBox's oriented-box overlap test.

#include "LocalBoundingBox.h"
#include "Ray3.h"
#include "BoundingBox.h"
#include "Frustum.h"
#include "Intersect.h"

#include <cfloat>
#include <cmath>
// static init: ELF 0x4986B0, 0x498710, 0x498740 (std::__ioinit).
#include <iostream>

namespace dice {
namespace hfe {

namespace {

float planeDistance(const Plane3& plane, const Vec3& point)
{
    return plane.normal.x * point.x + plane.normal.y * point.y
         + plane.normal.z * point.z + plane.d;
}

float dot(const Vec3& a, const Vec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float distanceBetween(const Vec3& a, const Vec3& b)
{
    const float x = a.x - b.x;
    const float y = a.y - b.y;
    const float z = a.z - b.z;
    return std::sqrt(x * x + y * y + z * z);
}

Vec3 transformPoint(const Mat4& transform, const Vec3& point)
{
    Vec3 result;
    result.x = transform.m[0] * point.x + transform.m[4] * point.y
             + transform.m[8] * point.z + transform.m[12];
    result.y = transform.m[1] * point.x + transform.m[5] * point.y
             + transform.m[9] * point.z + transform.m[13];
    result.z = transform.m[2] * point.x + transform.m[6] * point.y
             + transform.m[10] * point.z + transform.m[14];
    return result;
}

} // anonymous namespace

// ELF 0x496470; engine ELF 0x83B2CA0.
void LocalBoundingBox::expand(const BoundingBox& box)
{
    expand(box.getMin());
    expand(box.getMax());
}

// ELF 0x4964E0; engine ELF 0x83B2D00.
bool LocalBoundingBox::inside(const Frustum* frustum, const Mat4* transform, unsigned char mask) const
{
    unsigned char common = mask;
    for (int i = 0; i < 8; ++i)
    {
        const Vec3 corner = {(i & 4) ? m_max.x : m_min.x,
                             (i & 2) ? m_max.y : m_min.y,
                             (i & 1) ? m_max.z : m_min.z};
        const Vec3 point = transformPoint(*transform, corner);
        common &= static_cast<unsigned char>(frustum->getOutcode(point)) & mask;
        if (common == 0)
            return true;
    }
    return false;
}

// ELF 0x4969F0; engine ELF 0x83B31F0.
int LocalBoundingBox::getMajorAxis() const
{
    if (m_derivedDataDirty)
        const_cast<LocalBoundingBox*>(this)->calcRadius();
    if (m_size.y > m_size.x)
        return m_size.z > m_size.y ? 2 : 1;
    return m_size.z > m_size.x ? 2 : 0;
}

// ELF 0x497560; engine ELF 0x83B3CE0.
bool LocalBoundingBox::inside(const Frustum& frustum) const
{
    return intersect(frustum, *this);
}

// ELF 0x4981F0; engine ELF 0x83B49B0.
bool LocalBoundingBox::isInside(const Vec3& point, const Vec3& ownerPosition,
                               const Mat4& ownerInverse) const
{
    const float dx = point.x - ownerPosition.x;
    const float dy = point.y - ownerPosition.y;
    const float dz = point.z - ownerPosition.z;
    const float* const m = ownerInverse.m;
    const float lx = m[4] * dy + m[0] * dx + m[8] * dz;
    const float ly = m[9] * dz + (m[5] * dy + m[1] * dx);
    const float lz = dz * m[10] + dy * m[6] + dx * m[2];
    if (!(lx >= m_min.x)) return false;
    if (!(m_max.x >= lx)) return false;
    if (!(ly >= m_min.y)) return false;
    if (!(m_max.y >= ly)) return false;
    if (!(lz >= m_min.z)) return false;
    return m_max.z >= lz;
}

// ELF 0x496A70. The source overload is only an identity-transform
// forwarding wrapper in both ports.
bool LocalBoundingBox::intersectRay(const Ray3& ray, Vec3& position, Vec3* normal) const
{
    return intersectRay(ray, Mat4::getIdentity(), position, normal);
}

// ELF 0x496AC0. Tests the ray against this box under transform and writes intersection data when found.
bool LocalBoundingBox::intersectRay(const Ray3& ray, const Mat4& transform,
                                    Vec3& position, Vec3* normal) const
{
    const Vec3 worldMin = transformPoint(transform, m_min);
    const Vec3 worldMax = transformPoint(transform, m_max);

    Plane3 planes[6] = {};

    planes[0].normal.x = transform.m[8];
    planes[0].normal.y = transform.m[9];
    planes[0].normal.z = transform.m[10];
    planes[0].d = -dot(planes[0].normal, worldMax);
    planes[1].normal.x = -transform.m[8];
    planes[1].normal.y = -transform.m[9];
    planes[1].normal.z = -transform.m[10];
    planes[1].d = -dot(planes[1].normal, worldMin);

    planes[2].normal.x = transform.m[4];
    planes[2].normal.y = transform.m[5];
    planes[2].normal.z = transform.m[6];
    planes[2].d = -dot(planes[2].normal, worldMax);
    planes[3].normal.x = -transform.m[4];
    planes[3].normal.y = -transform.m[5];
    planes[3].normal.z = -transform.m[6];
    planes[3].d = -dot(planes[3].normal, worldMin);

    planes[4].normal.x = transform.m[0];
    planes[4].normal.y = transform.m[1];
    planes[4].normal.z = transform.m[2];
    planes[4].d = -dot(planes[4].normal, worldMax);
    planes[5].normal.x = -transform.m[0];
    planes[5].normal.y = -transform.m[1];
    planes[5].normal.z = -transform.m[2];
    planes[5].d = -dot(planes[5].normal, worldMin);

    Vec3 entryPoint = {};
    Vec3 farPoint = {};
    float nearestEntry = -FLT_MAX;
    float nearestExit = FLT_MAX;
    int entryPlane = -1;

    for (int i = 0; i < 6; i += 2)
    {
        Vec3 positivePoint = {};
        Vec3 negativePoint = {};
        if (!findIntersection(planes[i], ray, positivePoint))
        {
            if (planeDistance(planes[i], ray.origin) >= 0.0f)
                return false;
            if (!(planeDistance(planes[i + 1], ray.origin) < 0.0f))
                return false;
            continue;
        }

        const float positiveDistance = distanceBetween(positivePoint, ray.origin);
        if (!findIntersection(planes[i + 1], ray, negativePoint))
            continue;
        const float negativeDistance = distanceBetween(negativePoint, ray.origin);

        Vec3 positiveDelta;
        positiveDelta.x = positivePoint.x - ray.origin.x;
        positiveDelta.y = positivePoint.y - ray.origin.y;
        positiveDelta.z = positivePoint.z - ray.origin.z;
        Vec3 negativeDelta;
        negativeDelta.x = negativePoint.x - ray.origin.x;
        negativeDelta.y = negativePoint.y - ray.origin.y;
        negativeDelta.z = negativePoint.z - ray.origin.z;
        const bool positiveBehind = dot(positiveDelta, ray.direction) < 0.0f;
        const bool negativeBehind = dot(negativeDelta, ray.direction) < 0.0f;
        if (positiveBehind && negativeBehind)
            return false;

        if (dot(planes[i].normal, ray.direction) < 0.0f)
        {
            if (positiveDistance <= negativeDistance)
            {
                if (positiveDistance > nearestEntry && !positiveBehind)
                {
                    nearestEntry = positiveDistance;
                    entryPoint = positivePoint;
                    entryPlane = i;
                }
                if (negativeDistance < nearestExit && !negativeBehind)
                {
                    nearestExit = negativeDistance;
                    farPoint = negativePoint;
                }
                continue;
            }
            if (planeDistance(planes[i], ray.origin) >= 0.0f)
                return false;
        }
        else
        {
            if (positiveDistance >= negativeDistance)
            {
                if (negativeDistance > nearestEntry && !negativeBehind)
                {
                    nearestEntry = negativeDistance;
                    entryPoint = negativePoint;
                    entryPlane = i + 1;
                }
                if (positiveDistance < nearestExit && !positiveBehind)
                {
                    nearestExit = positiveDistance;
                    farPoint = positivePoint;
                }
                continue;
            }
            if (planeDistance(planes[i + 1], ray.origin) >= 0.0f)
                return false;
        }
    }

    (void)farPoint;
    if (nearestEntry <= -FLT_MAX)
        return false;
    if (nearestExit <= nearestEntry)
        return false;

    position = entryPoint;
    if (normal)
        *normal = planes[entryPlane].normal;
    return true;
}

// ELF 0x497600. Tests two oriented bounding boxes using their separating axes.
bool LocalBoundingBox::testIntersectWithOBB(const Mat4& thisTransform,
                                             const LocalBoundingBox& other,
                                             const Mat4& otherTransform,
                                             const Vec3& grow) const
{
    if (m_derivedDataDirty)
        const_cast<LocalBoundingBox*>(this)->calcRadius();

    const float a0 = m_size.x * 0.5f;
    const float a1 = m_size.y * 0.5f;
    const float a2 = m_size.z * 0.5f;

    if (other.m_derivedDataDirty)
        const_cast<LocalBoundingBox&>(other).calcRadius();

    const float b0 = other.m_size.x * 0.5f;
    const float b1 = other.m_size.y * 0.5f;
    const float b2 = other.m_size.z * 0.5f;

    float amin0 = m_min.x;
    float amin1 = m_min.y;
    float amin2 = m_min.z;
    float amax0 = m_max.x;
    float amax1 = m_max.y;
    float amax2 = m_max.z;

#if defined(_WIN32)
    (void)grow;
#else
    if (grow.x >= 0.0f) amax0 += grow.x; else amin0 += grow.x;
    if (grow.y >= 0.0f) amax1 += grow.y; else amin1 += grow.y;
    if (grow.z >= 0.0f) amax2 += grow.z; else amin2 += grow.z;
#endif

    const float ac0 = (amin0 + amax0) * 0.5f;
    const float ac1 = (amin1 + amax1) * 0.5f;
    const float ac2 = (amin2 + amax2) * 0.5f;
    const float aw0 = thisTransform.m[0] * ac0 + thisTransform.m[4] * ac1
                    + thisTransform.m[8] * ac2 + thisTransform.m[12];
    const float aw1 = thisTransform.m[1] * ac0 + thisTransform.m[5] * ac1
                    + thisTransform.m[9] * ac2 + thisTransform.m[13];
    const float aw2 = thisTransform.m[2] * ac0 + thisTransform.m[6] * ac1
                    + thisTransform.m[10] * ac2 + thisTransform.m[14];

    const float bc0 = (other.m_min.x + other.m_max.x) * 0.5f;
    const float bc1 = (other.m_min.y + other.m_max.y) * 0.5f;
    const float bc2 = (other.m_min.z + other.m_max.z) * 0.5f;
    const float bw0 = otherTransform.m[0] * bc0 + otherTransform.m[4] * bc1
                    + otherTransform.m[8] * bc2 + otherTransform.m[12];
    const float bw1 = otherTransform.m[1] * bc0 + otherTransform.m[5] * bc1
                    + otherTransform.m[9] * bc2 + otherTransform.m[13];
    const float bw2 = otherTransform.m[2] * bc0 + otherTransform.m[6] * bc1
                    + otherTransform.m[10] * bc2 + otherTransform.m[14];

    const float d0 = bw0 - aw0;
    const float d1 = bw1 - aw1;
    const float d2 = bw2 - aw2;

    const float r00 = thisTransform.m[0] * otherTransform.m[0]
                    + thisTransform.m[1] * otherTransform.m[1]
                    + thisTransform.m[2] * otherTransform.m[2];
    const float ar00 = std::fabs(r00);
    bool parallelPair = ar00 > 0.999999f;
    const float r01 = thisTransform.m[0] * otherTransform.m[4]
                    + thisTransform.m[1] * otherTransform.m[5]
                    + thisTransform.m[2] * otherTransform.m[6];
    const float ar01 = std::fabs(r01);
    if (ar01 > 0.999999f) parallelPair = true;
    const float r02 = thisTransform.m[0] * otherTransform.m[8]
                    + thisTransform.m[1] * otherTransform.m[9]
                    + thisTransform.m[2] * otherTransform.m[10];
    const float ar02 = std::fabs(r02);
    if (ar02 > 0.999999f) parallelPair = true;

    const float t0 = d0 * thisTransform.m[0] + d1 * thisTransform.m[1]
                   + d2 * thisTransform.m[2];
    if (std::fabs(t0) > a0 + b0 * ar00 + b1 * ar01 + b2 * ar02)
        return false;

    const float r10 = thisTransform.m[4] * otherTransform.m[0]
                    + thisTransform.m[5] * otherTransform.m[1]
                    + thisTransform.m[6] * otherTransform.m[2];
    const float ar10 = std::fabs(r10);
    if (ar10 > 0.999999f) parallelPair = true;
    const float r11 = thisTransform.m[4] * otherTransform.m[4]
                    + thisTransform.m[5] * otherTransform.m[5]
                    + thisTransform.m[6] * otherTransform.m[6];
    const float ar11 = std::fabs(r11);
    if (ar11 > 0.999999f) parallelPair = true;
    const float r12 = thisTransform.m[4] * otherTransform.m[8]
                    + thisTransform.m[5] * otherTransform.m[9]
                    + thisTransform.m[6] * otherTransform.m[10];
    const float ar12 = std::fabs(r12);
    if (ar12 > 0.999999f) parallelPair = true;

    const float t1 = d0 * thisTransform.m[4] + d1 * thisTransform.m[5]
                   + d2 * thisTransform.m[6];
    if (std::fabs(t1) > a1 + b0 * ar10 + b1 * ar11 + b2 * ar12)
        return false;

    const float r20 = thisTransform.m[8] * otherTransform.m[0]
                    + thisTransform.m[9] * otherTransform.m[1]
                    + thisTransform.m[10] * otherTransform.m[2];
    const float ar20 = std::fabs(r20);
    if (ar20 > 0.999999f) parallelPair = true;
    const float r21 = thisTransform.m[8] * otherTransform.m[4]
                    + thisTransform.m[9] * otherTransform.m[5]
                    + thisTransform.m[10] * otherTransform.m[6];
    const float ar21 = std::fabs(r21);
    if (ar21 > 0.999999f) parallelPair = true;
    const float r22 = thisTransform.m[8] * otherTransform.m[8]
                    + thisTransform.m[9] * otherTransform.m[9]
                    + thisTransform.m[10] * otherTransform.m[10];
    const float ar22 = std::fabs(r22);
    if (ar22 > 0.999999f) parallelPair = true;

    const float t2 = d0 * thisTransform.m[8] + d1 * thisTransform.m[9]
                   + d2 * thisTransform.m[10];
    if (std::fabs(t2) > a2 + b0 * ar20 + b1 * ar21 + b2 * ar22)
        return false;

    // The other box's three face axes.
    if (std::fabs(d0 * otherTransform.m[0] + d1 * otherTransform.m[1]
                + d2 * otherTransform.m[2]) > b0 + a0 * ar00 + a1 * ar10 + a2 * ar20)
        return false;
    if (std::fabs(d0 * otherTransform.m[4] + d1 * otherTransform.m[5]
                + d2 * otherTransform.m[6]) > b1 + a0 * ar01 + a1 * ar11 + a2 * ar21)
        return false;
    if (std::fabs(d0 * otherTransform.m[8] + d1 * otherTransform.m[9]
                + d2 * otherTransform.m[10]) > b2 + a0 * ar02 + a1 * ar12 + a2 * ar22)
        return false;

    // When one pair is (nearly) parallel, its degenerate cross product makes the remaining cross
    // axes redundant after all six face normals have passed. This retail shortcut also avoids
    // numerical separation on a near-zero axis.
    if (parallelPair)
        return true;

    if (std::fabs(t2 * r10 - t1 * r20) >
        a1 * ar20 + a2 * ar10 + b1 * ar02 + b2 * ar01)
        return false;
    if (std::fabs(t2 * r11 - t1 * r21) >
        a1 * ar21 + a2 * ar11 + b0 * ar02 + b2 * ar00)
        return false;
    if (std::fabs(t2 * r12 - t1 * r22) >
        a1 * ar22 + a2 * ar12 + b0 * ar01 + b1 * ar00)
        return false;

    if (std::fabs(t0 * r20 - t2 * r00) >
        a0 * ar20 + a2 * ar00 + b1 * ar12 + b2 * ar11)
        return false;
    if (std::fabs(t0 * r21 - t2 * r01) >
        a0 * ar21 + a2 * ar01 + b0 * ar12 + b2 * ar10)
        return false;
    if (std::fabs(t0 * r22 - t2 * r02) >
        a0 * ar22 + a2 * ar02 + b0 * ar11 + b1 * ar10)
        return false;

    if (std::fabs(t1 * r00 - t0 * r10) >
        a0 * ar10 + a1 * ar00 + b1 * ar22 + b2 * ar21)
        return false;
    if (std::fabs(t1 * r01 - t0 * r11) >
        a0 * ar11 + a1 * ar01 + b0 * ar22 + b2 * ar20)
        return false;
    if (std::fabs(t1 * r02 - t0 * r12) >
        a0 * ar12 + a1 * ar02 + b0 * ar21 + b1 * ar20)
        return false;

    return true;
}

} // namespace hfe
} // namespace dice
