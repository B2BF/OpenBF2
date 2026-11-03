// Eight-corner bounding volume. Header placement and member spelling are [INFERRED].
#ifndef MATH_BOUNDINGBOX_H
#define MATH_BOUNDINGBOX_H

#include "Vec3.h"

namespace dice {
namespace hfe {

struct LocalBoundingBox;
struct Mat4;
class Frustum;

class BoundingBox
{
public:
    // Writes corners in binary x/y/z order, with z changing fastest.
    void initWithLocalBoundingBox(const LocalBoundingBox& box);
    void transform(const Mat4& matrix);
    // Plane overlap tests; inside does not require full containment.
    bool outside(const Frustum& frustum) const;
    bool inside(const Frustum& frustum) const;
    Vec3 getMax() const;
    Vec3 getMin() const;
    // Midpoint of corners zero and seven.
    Vec3 getCentre() const;
    // Separating-axis test for orthogonal boxes; degenerate axes are not guarded.
    bool intersect(const BoundingBox& other) const;

    Vec3 m_corners[8]; // +0x00, stride 0x0C
};

static_assert(sizeof(BoundingBox) == 0x60, "BoundingBox layout drift");

} // namespace hfe
} // namespace dice

#endif // MATH_BOUNDINGBOX_H
