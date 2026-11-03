// Three-by-three matrices; member spelling is [INFERRED].
#ifndef MATH_MAT3_H
#define MATH_MAT3_H

namespace dice {
namespace hfe {

struct Mat3
{
    float m[9];

    // Inverts an orthonormal 2D affine transform, preserving m[8].
    // Input/output aliasing retains the shipped sequential overwrite behavior.
    Mat3& setInverse(const Mat3& source);
    // Affine product; writes the final column as (0, 0, 1).
    // The destination must be distinct from the operands for an ordinary matrix product.
    Mat3& mult(const Mat3& lhs, const Mat3& rhs);
    // Full product, with sequential column writes; operands must not alias the destination.
    Mat3& fullMult(const Mat3& lhs, const Mat3& rhs);
    // Writes only the upper-left 2x2 product, preserving all other elements.
    Mat3& rotMult(const Mat3& lhs, const Mat3& rhs);
    Mat3 operator*(const Mat3& rhs) const;
};

static_assert(sizeof(Mat3) == 0x24, "Mat3 layout drift");

} // namespace hfe
} // namespace dice

#endif // MATH_MAT3_H
