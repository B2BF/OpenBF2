// Sphere, ray, and box intersection helpers.

#include "Intersect.h"
#include "Frustum.h"
#include "Ray3.h"

#include <cfloat>
#include <cmath>
// static init: ELF 0x4963C0, 0x496420, 0x496450 (std::__ioinit).
#include <iostream>

namespace dice {
namespace hfe {

// ELF 0x495E20; engine ELF 0x83B26D0.
bool findIntersection(const Plane3& plane, const Ray3& ray, Vec3& position)
{
    const float denominator = plane.normal.x * ray.direction.x
                            + plane.normal.y * ray.direction.y
                            + plane.normal.z * ray.direction.z;
    // The ia-32 images proceed for an unordered denominator too.
    if (std::fabs(denominator) < FLT_EPSILON)
        return false;
    const float amount = -(plane.normal.x * ray.origin.x + plane.normal.y * ray.origin.y
                         + plane.normal.z * ray.origin.z + plane.d) / denominator;
    Vec3 result;
    result.x = ray.origin.x + ray.direction.x * amount;
    result.y = ray.origin.y + ray.direction.y * amount;
    result.z = ray.origin.z + ray.direction.z * amount;
    position = result;
    return true;
}

// ELF 0x495F20; engine ELF 0x83B27C0.
bool intersect(const Frustum& frustum, const LocalBoundingBox& box)
{
    for (int i = 0; i < 6; ++i)
    {
        Vec3 point = {};
        const unsigned int mask = static_cast<unsigned int>(frustum.m_planeOutcodes[i]);
        if (mask <= 7)
        {
            point.x = (mask & 1) ? box.m_max.x : box.m_min.x;
            point.y = (mask & 2) ? box.m_max.y : box.m_min.y;
            point.z = (mask & 4) ? box.m_max.z : box.m_min.z;
        }
        const Plane3& plane = frustum.m_planes[i];
        if (point.x * plane.normal.x + point.y * plane.normal.y
          + point.z * plane.normal.z + plane.d > 0.0f)
            return false;
    }
    return true;
}

// ELF 0x496180. Per axis: below the box -> (c - min)^2, above it -> (c - max)^2, inside -> 0.
bool sphereIntersectsAABB(const Vec3& centre, float radius, const Vec3& boxMin, const Vec3& boxMax)
{
    const float* c  = &centre.x;
    const float* lo = &boxMin.x;
    const float* hi = &boxMax.x;
    float d = 0.0f;
    for (int i = 0; i < 3; ++i)
    {
        if (lo[i] > c[i])
            d += (c[i] - lo[i]) * (c[i] - lo[i]);
        else if (c[i] > hi[i])
            d += (c[i] - hi[i]) * (c[i] - hi[i]);
    }
    return radius * radius >= d;
}

namespace {

// Intersects per-axis slab intervals, rejecting disjoint near/far ranges.
inline bool slabTest(const Vec3& origin, const Vec3& dir, const float* lo, const float* hi,
                     float& tMin, float& tMax)
{
    const float* o = &origin.x;
    const float* d = &dir.x;
    float inv[3];
    for (int i = 0; i < 3; ++i)
        inv[i] = 1.0f / d[i];                       // no zero guard, as shipped (±inf slabs)

    // x
    float t0, t1;
    if (inv[0] < 0.0f) { t0 = (hi[0] - o[0]) * inv[0]; t1 = (lo[0] - o[0]) * inv[0]; }
    else               { t0 = (lo[0] - o[0]) * inv[0]; t1 = (hi[0] - o[0]) * inv[0]; }
    // y
    float ty0, ty1;
    if (inv[1] < 0.0f) { ty0 = (hi[1] - o[1]) * inv[1]; ty1 = (lo[1] - o[1]) * inv[1]; }
    else               { ty0 = (lo[1] - o[1]) * inv[1]; ty1 = (hi[1] - o[1]) * inv[1]; }
    if (t0 > ty1) return false;
    if (ty0 > t1) return false;
    if (!(ty0 <= t0)) t0 = ty0;                     // t0 = max(t0, ty0) - the ELF's exact test
    if (t1 > ty1)     t1 = ty1;                     // t1 = min(t1, ty1)
    // z
    float tz0, tz1;
    if (inv[2] < 0.0f) { tz0 = (hi[2] - o[2]) * inv[2]; tz1 = (lo[2] - o[2]) * inv[2]; }
    else               { tz0 = (lo[2] - o[2]) * inv[2]; tz1 = (hi[2] - o[2]) * inv[2]; }
    if (t0 > tz1) return false;
    if (tz0 > t1) return false;
    if (!(tz0 <= t0)) t0 = tz0;
    if (t1 > tz1)     t1 = tz1;

    tMin = t0;                                      // written before the range test, as shipped
    tMax = t1;
    if (t0 <= 1.0f)
        return t1 >= 0.0f;
    return false;
}

} // namespace

// ELF 0x496010.
bool rayIntersectsAABB(const Vec3& origin, const Vec3& dir, const Vec3& boxMin, const Vec3& boxMax,
                       float& tMin, float& tMax)
{
    return slabTest(origin, dir, &boxMin.x, &boxMax.x, tMin, tMax);
}

// ELF 0x4961F0. The box grown by `radius` on all six sides first.
bool rayRadiusIntersectsAABB(const Vec3& origin, const Vec3& dir, float radius,
                             const Vec3& boxMin, const Vec3& boxMax, float& tMin, float& tMax)
{
    float lo[3] = { boxMin.x - radius, boxMin.y - radius, boxMin.z - radius };
    float hi[3] = { boxMax.x + radius, boxMax.y + radius, boxMax.z + radius };
    return slabTest(origin, dir, lo, hi, tMin, tMax);
}

} // namespace hfe
} // namespace dice
