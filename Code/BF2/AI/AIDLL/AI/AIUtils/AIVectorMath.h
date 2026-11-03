// Free vector helpers of the AI image (namespace dice::hfe::ai).
// [INFERRED] Source placement and file name: libaidll.so keeps these in one translation unit with
// its own static initializer (ELF 0x367500..0x367A9F: distancePointLineSqr, objFireLineDist,
// makeTransOrthonormalUp, getDofTransformation); no __FILE__ string attests the path.

#ifndef AI_AIDLL_AI_AIUTILS_AIVECTORMATH_H
#define AI_AIDLL_AI_AIUTILS_AIVECTORMATH_H

#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

// Squared distance from `point` to the infinite line through `linePoint` along `direction`
// (|(point - linePoint) x direction|^2 / |direction|^2). A zero direction divides by zero.
// No caller in libaidll.so.
float distancePointLineSqr(const Vec3& point, const Vec3& linePoint, const Vec3& direction);

// Distance from `point` to the segment from `from` to `to`: the perpendicular distance where the
// projection falls inside the segment, otherwise the distance to the nearer end. A segment
// shorter than 0.001 answers 0.
float objFireLineDist(const Vec3& point, const Vec3& from, const Vec3& to);

// Re-orthonormalises `transformation` around `up`: row 1 becomes `up`, then the rows are rebuilt
// keeping row 2's direction (the forward vector). No caller in libaidll.so.
void makeTransOrthonormalUp(Mat4& transformation, const Vec3& up);

// An orthonormal transformation whose row 2 points along `direction`, no translation. No caller
// in libaidll.so.
Mat4 getDofTransformation(const Vec3& direction);

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_AIVECTORMATH_H
