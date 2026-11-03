// Plane representation and transform operations. [INFERRED] Source placement.


#ifndef MATH_PLANE3_H     // [INFERRED] guard style
#define MATH_PLANE3_H

#include "Vec3.h"

namespace dice {
namespace hfe {

struct Mat4;

struct Plane3
{
    Vec3  normal;                                              // +0x00
    float d;                                                   // +0x0C

    // static, by value.
    static Plane3 transform(const Plane3& plane, const Mat4& m);
    // Projects onto the plane through the origin, scaled by normal's squared length.
    // The input may alias the output; d does not participate.
    void projectVector(const Vec3& v, Vec3& out);
    // Intersects an infinite line; parallel lines leave both outputs unchanged.
    // The range flag reports whether the intersection lies on the segment.
    bool intersectLine(const Vec3& a, const Vec3& b, Vec3& out, bool& insideSegment);
};

// the point common to three planes (Frustum::recalcCorners' corners).
Vec3 intersect(const Plane3& p1, const Plane3& p2, const Plane3& p3);

} // namespace hfe
} // namespace dice

#endif // MATH_PLANE3_H
