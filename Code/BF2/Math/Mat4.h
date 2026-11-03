// Four-by-four matrix and transform operations. [INFERRED] Source placement.

#ifndef MATH_MAT4_H     // [INFERRED] guard style
#define MATH_MAT4_H

#include <iosfwd>

#include "Vec3.h"       // setRotateAboutLine's axis

namespace dice {
namespace hfe {


// This declaration is an aggregate: default construction leaves its values uninitialized.
// Callers needing identity must initialize it explicitly.

struct Mat4
{
    float m[16];

    static const Mat4& getIdentity();
    static const Mat4& getZero();       // dice::hfe::Mat4::getZero - ArmorGLComp::setUseDestroyedLodRecursive
                                        // hands it to the geometry to hide a part
    static const Mat4& getTexScaleBias();
    Mat4 operator*(const Mat4& rhs) const;
    void mult(const Mat4& lhs, const Mat4& rhs);

    // Writes all sixteen elements of lhs * rhs. Use the specialized products only for their narrower contracts.
    void fullMult(const Mat4& lhs, const Mat4& rhs);

    void setInverse(const Mat4& source);

    // Scales x/y/z components, including translation for scale and excluding it for rotScale.
    // Both return this matrix and preserve the fourth column.
    Mat4& scale(const Vec3& factors);
    Mat4& rotScale(const Vec3& factors);
    // Sequential transpose; source must be distinct for a correct transpose.
    Mat4& setTranspose(const Mat4& source);

    // Writes only the 3x3 rotation product, preserving translation and the fourth column.
    void rotMult(const Mat4& lhs, const Mat4& rhs);
};


// Parses and prints four bracketed rows of four slash-separated floats in memory order.
// [CHOSEN] Stream-operator packaging. Malformed first-use input can differ from the original identity-initialized parser.

std::istream& operator>>(std::istream& in, Mat4& m);
std::ostream& operator<<(std::ostream& out, const Mat4& m);

// Gets Euler angles in Z, X, Y degree order. Script-facing rotation reverses and negates this order.
void getRotationZXY(const Mat4& m, float& zDegrees, float& xDegrees, float& yDegrees);
void setRotationZXY(Mat4& m, float zDegrees, float xDegrees, float yDegrees);

// Overwrites the complete matrix, including translation, with the requested rotation.
void setRotateXDeg(Mat4& m, float degrees);

// Overwrites the whole matrix with a Y rotation; m[2] is -sin and m[8] is sin.
void setRotateYDeg(Mat4& m, float degrees);

// Same shape as setRotateXDeg - identity first, then the four terms - so it
// too overwrites the whole matrix. m[0] = cos, m[1] = -sin, m[4] = sin, m[5] = cos.
void setRotateZDeg(Mat4& m, float degrees);


// Overwrites m with a rotation in degrees around axis through the origin.
// The axis must be nondegenerate; basis-construction failure is not handled.

void setRotateAboutLine(Mat4& m, float degrees, const Vec3& axis);


// Builds a viewing transform from eye, target and up.

void calcLookAtMatrix(Mat4& m, const Vec3& eye, const Vec3& target, const Vec3& up);

// Perspective projection in radians. Returns -20000 without writing m when far-near < FLT_EPSILON.
// Otherwise returns zero; aspectScale multiplies the horizontal cotangent.
int calcProjectionMatrix(Mat4& m, float fov, float aspectScale, float nearDistance, float farDistance);

// Writes the basis columns in right/up/forward order and translation as negative eye dot each axis.
// Returns the destination matrix.
Mat4& calcRotationMatrixFromAllVectors(Mat4& m, const Vec3& forward, const Vec3& up,
                                       const Vec3& right, const Vec3& eye);

// General 4x4 inverse. Supports input/output aliasing; no singular-determinant guard.
// [INFERRED] Void return.
void fullInvert(const Mat4& source, Mat4& destination);

} // namespace hfe
} // namespace dice

#endif // MATH_MAT4_H
