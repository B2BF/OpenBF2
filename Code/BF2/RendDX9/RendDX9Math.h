// RendDX9.dll's own copies of the engine Math/Frustum helpers.
//
// [CHOSEN, not recovered] placement and names. RendDX9.dll links its own instantiations of the
// Math library's inline and out-of-line helpers (PE 0x10077910..0x1007C6C0 and 0x100A2F00), and
// their accumulation orders differ from the Linux server's (Frustum::inside(Vec3, float): RendDX9
// PE 0x10079A66..0x10079A7B sums z, y, x then +d; ELF 0x083B23A2..0x083B23B2 sums x, y, z). The
// shared Code/BF2/Math sources follow the ELF and stay untouched; RendDX9-image callers use these
// copies wherever the PE calls the RendDX9 copy. Each body follows its listing: operand order, the
// points where the x87 value is stored to a float, and the element write order (which decides the
// result when an output aliases an input).
//
// The MSVC x86 build evaluates in plain float. The Apple arms carry the PE's x87 values between
// its float stores through bf2x87::Real.

#ifndef RENDDX9_RENDDX9MATH_H
#define RENDDX9_RENDDX9MATH_H

#include "../Math/Vec3.h"
#include "../Math/Mat3.h"
#include "../Math/Mat4.h"
#include "../Math/Plane3.h"
#include "../Math/Frustum.h"
#include "../Math/LocalBoundingBox.h"
#include "../GeomMisc/Debug.h"
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Math/PortableX87.h"     // bf2x87::Real, trig
#endif

#include <cmath>
#include <cstring>

namespace dice {
namespace hfe {

namespace geom {
// StaticMesh.cpp - the PE 0x10077A30 product (output = left * right), sequential element writes.
void geometryRendererMatrixProduct(Mat4& output, const Mat4& left, const Mat4& right);
}

namespace rendmath {

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] x87 extended values between the PE's float stores.
typedef bf2x87::Real Real;
inline Real squareRootOf(Real value) { return bf2x87::squareRoot(value); }
inline Real absOf(Real value) { return bf2x87::abs(value); }
inline void sineCosine(float angle, float& sine, float& cosine)
{
    bf2x87::trig(angle, sine, cosine);
}
#else
typedef float Real;
inline Real squareRootOf(Real value) { return std::sqrt(value); }
inline Real absOf(Real value) { return std::fabs(value); }
inline void sineCosine(float angle, float& sine, float& cosine)
{
    sine = std::sin(angle);
    cosine = std::cos(angle);
}
#endif

// A load of a float into the evaluation type, and a store back.
inline Real ld(float value) { return static_cast<Real>(value); }
inline float st(Real value) { return static_cast<float>(value); }

// float 0x3C8EFA35 (.data 0x10238738), the degrees-to-radians factor the rotation copies load.
const float kDegreesToRadians = 0.0174533f;
// float 0x34000000 (.rdata 0x10208958), FLT_EPSILON.
const float kEpsilon = 1.1920929e-07f;

// PE 0x10077760: out = cross(a, b); `out` is the thiscall receiver and is written x, y, z.
inline void cross(Vec3& out, const Vec3& a, const Vec3& b)
{
    out.x = st(ld(b.z) * ld(a.y) - ld(b.y) * ld(a.z));
    out.y = st(ld(b.x) * ld(a.z) - ld(a.x) * ld(b.z));
    out.z = st(ld(a.x) * ld(b.y) - ld(b.x) * ld(a.y));
}

// PE 0x100277A0 [INFERRED] Vec3 squared length (receiver in ecx): (x*x + y*y) + z*z, returned in
// st0 without a float store. psmType0Update calls the copy for its wind-speed term (0x10180B34)
// and spells the same sum inline in this source.
inline Real lengthSquared(const Vec3& v)
{
    return (ld(v.x) * ld(v.x) + ld(v.y) * ld(v.y)) + ld(v.z) * ld(v.z);
}

// PE 0x100777A0: in-place normalise. |v|^2 = (x*x + y*y) + z*z is stored as a float; within
// FLT_EPSILON of 1 the vector stays, below FLT_EPSILON it is zeroed and -20001 is answered,
// otherwise each component is scaled by 1 / sqrt(|v|^2). An unordered length takes the scale.
inline int normalize(Vec3& v)
{
    const float lengthSquared = st((ld(v.x) * ld(v.x) + ld(v.y) * ld(v.y)) + ld(v.z) * ld(v.z));
    if (absOf(ld(lengthSquared) - ld(1.0f)) < ld(kEpsilon))
        return 0;
    if (std::fabs(lengthSquared) < kEpsilon)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        return -20001;
    }
    const Real inverse = ld(1.0f) / squareRootOf(ld(lengthSquared));
    v.x = st(inverse * ld(v.x));
    v.y = st(inverse * ld(v.y));
    v.z = st(inverse * ld(v.z));
    return 0;
}

// PE 0x10077830 [INFERRED] scale a matrix in place by a Vec3 (thiscall on the matrix): m0, m4, m8
// and m12 by x, then m1, m5, m9, m13 by y, then m2, m6, m10, m14 by z, each product stored at
// once. The translation row is scaled too; the w column is untouched.
inline void scaleColumns(Mat4& m, const Vec3& s)
{
    m.m[0]  = st(ld(s.x) * ld(m.m[0]));
    m.m[4]  = st(ld(m.m[4]) * ld(s.x));
    m.m[8]  = st(ld(s.x) * ld(m.m[8]));
    m.m[12] = st(ld(m.m[12]) * ld(s.x));
    m.m[1]  = st(ld(s.y) * ld(m.m[1]));
    m.m[5]  = st(ld(s.y) * ld(m.m[5]));
    m.m[9]  = st(ld(s.y) * ld(m.m[9]));
    m.m[13] = st(ld(s.y) * ld(m.m[13]));
    m.m[2]  = st(ld(s.z) * ld(m.m[2]));
    m.m[6]  = st(ld(m.m[6]) * ld(s.z));
    m.m[10] = st(ld(m.m[10]) * ld(s.z));
    m.m[14] = st(ld(m.m[14]) * ld(s.z));
}

// PE 0x10077910 [INFERRED] Mat4::setTranspose copy: out = transpose(in), written in element
// order 0..15 straight from the source (no temporary), returning out.
inline Mat4& setTranspose(Mat4& out, const Mat4& in)
{
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            out.m[row * 4 + column] = in.m[column * 4 + row];
    return out;
}

// PE 0x10077980 [INFERRED] Mat4::setInverse copy (rigid transform): the transposed rotation,
// then translation -(t . column) from the rotation just written, then the w column (0, 0, 0, 1).
inline Mat4& setInverse(Mat4& out, const Mat4& in)
{
    out.m[0] = in.m[0];  out.m[1] = in.m[4];  out.m[2] = in.m[8];
    out.m[4] = in.m[1];  out.m[5] = in.m[5];  out.m[6] = in.m[9];
    out.m[8] = in.m[2];  out.m[9] = in.m[6];  out.m[10] = in.m[10];
    out.m[12] = st(-((ld(in.m[12]) * ld(out.m[0]) + ld(out.m[4]) * ld(in.m[13]))
                    + ld(in.m[14]) * ld(out.m[8])));
    out.m[13] = st(-((ld(in.m[14]) * ld(out.m[9]) + ld(out.m[5]) * ld(in.m[13]))
                    + ld(in.m[12]) * ld(out.m[1])));
    out.m[14] = st(-((ld(in.m[14]) * ld(out.m[10]) + ld(in.m[13]) * ld(out.m[6]))
                    + ld(in.m[12]) * ld(out.m[2])));
    out.m[11] = 0.0f;
    out.m[7] = 0.0f;
    out.m[3] = 0.0f;
    out.m[15] = 1.0f;
    return out;
}

// PE 0x10077C50 [INFERRED] Mat4::mult copy (affine product): the 3x3 rotation of lhs * rhs plus
// rhs's translation, w column (0, 0, 0, 1). Elements are written 0, 4, 8, 12, 1, 5, 9, 13, 2, 6,
// 10, 14, each from the inputs as they stand at that point.
inline Mat4& mult(Mat4& out, const Mat4& lhs, const Mat4& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    out.m[0] = st((ld(l[0]) * ld(r[0]) + ld(l[1]) * ld(r[4])) + ld(l[2]) * ld(r[8]));
    out.m[4] = st((ld(l[6]) * ld(r[8]) + ld(l[5]) * ld(r[4])) + ld(l[4]) * ld(r[0]));
    out.m[8] = st((ld(l[10]) * ld(r[8]) + ld(l[9]) * ld(r[4])) + ld(l[8]) * ld(r[0]));
    out.m[12] = st(((ld(l[14]) * ld(r[8]) + ld(l[13]) * ld(r[4])) + ld(l[12]) * ld(r[0]))
                   + ld(r[12]));
    out.m[1] = st((ld(l[1]) * ld(r[5]) + ld(r[1]) * ld(l[0])) + ld(r[9]) * ld(l[2]));
    out.m[5] = st((ld(l[6]) * ld(r[9]) + ld(l[5]) * ld(r[5])) + ld(l[4]) * ld(r[1]));
    out.m[9] = st((ld(l[10]) * ld(r[9]) + ld(l[9]) * ld(r[5])) + ld(l[8]) * ld(r[1]));
    out.m[13] = st(((ld(l[14]) * ld(r[9]) + ld(l[13]) * ld(r[5])) + ld(l[12]) * ld(r[1]))
                   + ld(r[13]));
    out.m[2] = st((ld(l[0]) * ld(r[2]) + ld(r[10]) * ld(l[2])) + ld(r[6]) * ld(l[1]));
    out.m[6] = st((ld(l[4]) * ld(r[2]) + ld(r[10]) * ld(l[6])) + ld(r[6]) * ld(l[5]));
    out.m[10] = st((ld(l[8]) * ld(r[2]) + ld(r[10]) * ld(l[10])) + ld(r[6]) * ld(l[9]));
    out.m[14] = st(((ld(l[12]) * ld(r[2]) + ld(r[10]) * ld(l[14])) + ld(r[6]) * ld(l[13]))
                   + ld(r[14]));
    out.m[3] = 0.0f;
    out.m[7] = 0.0f;
    out.m[11] = 0.0f;
    out.m[15] = 1.0f;
    return out;
}

// PE 0x10077EC0 [INFERRED] setRotateZDeg copy: identity, then the z rotation from the float
// radians (degrees * 0.0174533f): m[0] = cos, m[1] = -sin, m[4] = sin, m[5] = cos.
inline Mat4& setRotateZDeg(Mat4& m, float degrees)
{
    const float radians = st(ld(kDegreesToRadians) * ld(degrees));
    for (int i = 0; i < 16; ++i)
        m.m[i] = 0.0f;
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    float sine, cosine;
    sineCosine(radians, sine, cosine);
    m.m[0] = cosine;
    m.m[1] = -sine;
    m.m[4] = sine;
    m.m[5] = cosine;
    return m;
}

// PE 0x100780D0 [INFERRED] the rigid inverse of a view matrix given by its rows. The PE passes the
// destination in ecx, the z row in edx and the y row, x row and position on the stack; out gets the
// rows as columns and -(row . position) as its translation, written column by column.
inline Mat4& setInverseFromRows(Mat4& out, const Vec3& rowX, const Vec3& rowY, const Vec3& rowZ,
                                const Vec3& position)
{
    out.m[0] = rowX.x;  out.m[4] = rowX.y;  out.m[8] = rowX.z;
    out.m[12] = st(-((ld(rowX.y) * ld(position.y) + ld(rowX.x) * ld(position.x))
                    + ld(position.z) * ld(rowX.z)));
    out.m[1] = rowY.x;  out.m[5] = rowY.y;  out.m[9] = rowY.z;
    out.m[13] = st(-((ld(rowY.x) * ld(position.x) + ld(rowY.z) * ld(position.z))
                    + ld(position.y) * ld(rowY.y)));
    out.m[2] = rowZ.x;  out.m[6] = rowZ.y;  out.m[10] = rowZ.z;
    out.m[14] = st(-((ld(rowZ.y) * ld(position.y) + ld(rowZ.x) * ld(position.x))
                    + ld(position.z) * ld(rowZ.z)));
    out.m[3] = 0.0f;
    out.m[7] = 0.0f;
    out.m[11] = 0.0f;
    out.m[15] = 1.0f;
    return out;
}

// PE 0x10078180 [INFERRED] Mat4::rotMult copy: only the 3x3 rotation of lhs * rhs, written
// 0, 4, 8, 1, 5, 9, 2, 6, 10; element 0 sums x, y, z and element 4 z, y, x (decision 1).
inline Mat4& rotMult(Mat4& out, const Mat4& lhs, const Mat4& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    out.m[0] = st((ld(l[0]) * ld(r[0]) + ld(l[1]) * ld(r[4])) + ld(l[2]) * ld(r[8]));
    out.m[4] = st((ld(l[6]) * ld(r[8]) + ld(l[5]) * ld(r[4])) + ld(l[4]) * ld(r[0]));
    out.m[8] = st((ld(l[10]) * ld(r[8]) + ld(l[9]) * ld(r[4])) + ld(l[8]) * ld(r[0]));
    out.m[1] = st((ld(l[1]) * ld(r[5]) + ld(r[1]) * ld(l[0])) + ld(r[9]) * ld(l[2]));
    out.m[5] = st((ld(l[6]) * ld(r[9]) + ld(l[5]) * ld(r[5])) + ld(l[4]) * ld(r[1]));
    out.m[9] = st((ld(l[10]) * ld(r[9]) + ld(l[9]) * ld(r[5])) + ld(l[8]) * ld(r[1]));
    out.m[2] = st((ld(l[0]) * ld(r[2]) + ld(l[1]) * ld(r[6])) + ld(l[2]) * ld(r[10]));
    out.m[6] = st((ld(l[6]) * ld(r[10]) + ld(l[5]) * ld(r[6])) + ld(l[4]) * ld(r[2]));
    out.m[10] = st((ld(l[10]) * ld(r[10]) + ld(l[9]) * ld(r[6])) + ld(l[8]) * ld(r[2]));
    return out;
}

// PE 0x1007C3D0 [INFERRED] Mat3 from three rows: zeroed, then rows 0, 1, 2.
inline Mat3 mat3FromRows(const Vec3& row0, const Vec3& row1, const Vec3& row2)
{
    Mat3 out;
    for (int i = 0; i < 9; ++i)
        out.m[i] = 0.0f;
    out.m[0] = row0.x; out.m[1] = row0.y; out.m[2] = row0.z;
    out.m[3] = row1.x; out.m[4] = row1.y; out.m[5] = row1.z;
    out.m[6] = row2.x; out.m[7] = row2.y; out.m[8] = row2.z;
    return out;
}

// PE 0x1007C2C0 [INFERRED] Mat3 product lhs * rhs: every element stored as a float, rows 2, 1, 0
// computed in that order, the result built through PE 0x1007C3D0.
inline Mat3 mat3Product(const Mat3& lhs, const Mat3& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    Vec3 row2, row1, row0;
    row2.x = st((ld(l[7]) * ld(r[3]) + ld(l[8]) * ld(r[6])) + ld(l[6]) * ld(r[0]));
    row2.y = st((ld(l[7]) * ld(r[4]) + ld(l[6]) * ld(r[1])) + ld(r[7]) * ld(l[8]));
    row2.z = st((ld(l[7]) * ld(r[5]) + ld(l[8]) * ld(r[8])) + ld(l[6]) * ld(r[2]));
    row1.x = st((ld(l[5]) * ld(r[6]) + ld(l[4]) * ld(r[3])) + ld(l[3]) * ld(r[0]));
    row1.y = st((ld(r[7]) * ld(l[5]) + ld(r[4]) * ld(l[4])) + ld(r[1]) * ld(l[3]));
    row1.z = st((ld(l[5]) * ld(r[8]) + ld(l[4]) * ld(r[5])) + ld(l[3]) * ld(r[2]));
    row0.x = st((ld(r[0]) * ld(l[0]) + ld(l[2]) * ld(r[6])) + ld(l[1]) * ld(r[3]));
    row0.y = st((ld(r[7]) * ld(l[2]) + ld(r[4]) * ld(l[1])) + ld(r[1]) * ld(l[0]));
    row0.z = st((ld(r[2]) * ld(l[0]) + ld(l[2]) * ld(r[8])) + ld(l[1]) * ld(r[5]));
    return mat3FromRows(row0, row1, row2);
}

// PE 0x10078730 [INFERRED] the Euler rotation copy (degrees, z then x then y, as
// setRotationZXY): a z rotation from the first angle, an x rotation from the second and a y
// rotation from the third, each built only when its angle compares unequal to 0 (an unordered
// angle builds it), combined through PE 0x1007C2C0 by which of them were built, and copied into
// the 3x3 rotation of m. The translation and w column are never written; with all three angles
// zero the rotation becomes the identity.
inline Mat4& setRotationZXY(Mat4& m, float zDegrees, float xDegrees, float yDegrees)
{
    Mat3 rotZ = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    Mat3 rotX = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    Mat3 rotY = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    unsigned int built = 0;
    float sine, cosine;
    if (zDegrees != 0.0f)
    {
        built = 1;
        sineCosine(st(ld(kDegreesToRadians) * ld(zDegrees)), sine, cosine);
        rotZ.m[0] = cosine; rotZ.m[1] = -sine; rotZ.m[3] = sine; rotZ.m[4] = cosine;
    }
    if (xDegrees != 0.0f)
    {
        built |= 2;
        sineCosine(st(ld(kDegreesToRadians) * ld(xDegrees)), sine, cosine);
        rotX.m[4] = cosine; rotX.m[5] = -sine; rotX.m[7] = sine; rotX.m[8] = cosine;
    }
    if (yDegrees != 0.0f)
    {
        built |= 4;
        sineCosine(st(ld(kDegreesToRadians) * ld(yDegrees)), sine, cosine);
        rotY.m[0] = cosine; rotY.m[6] = -sine; rotY.m[2] = sine; rotY.m[8] = cosine;
    }
    Mat3 rotation = {{1, 0, 0, 0, 1, 0, 0, 0, 1}};
    switch (built)
    {
    case 0:
        m.m[0] = 1.0f; m.m[1] = 0.0f; m.m[2] = 0.0f;
        m.m[4] = 0.0f; m.m[5] = 1.0f; m.m[6] = 0.0f;
        m.m[8] = 0.0f; m.m[9] = 0.0f; m.m[10] = 1.0f;
        return m;
    case 1: rotation = rotZ; break;                                   // PE 0x10079420 copies
    case 2: rotation = rotX; break;
    case 3: rotation = mat3Product(rotZ, rotX); break;
    case 4: rotation = rotY; break;
    case 5: rotation = mat3Product(rotZ, rotY); break;
    case 6: rotation = mat3Product(rotX, rotY); break;
    default: rotation = mat3Product(rotZ, mat3Product(rotX, rotY)); break;
    }
    m.m[0] = rotation.m[0]; m.m[1] = rotation.m[1]; m.m[2] = rotation.m[2];
    m.m[4] = rotation.m[3]; m.m[5] = rotation.m[4]; m.m[6] = rotation.m[5];
    m.m[8] = rotation.m[6]; m.m[9] = rotation.m[7]; m.m[10] = rotation.m[8];
    return m;
}

// PE 0x10078DE0 [INFERRED] calcLookAtMatrix copy (eye in edx, target and up on the stack):
// identity; row 2 = target - eye, normalised by 1 / sqrt((x*x + y*y) + z*z). A length below
// FLT_EPSILON re-stores the identity. When row 2 is within FLT_EPSILON of +up or -up (dot summed
// z, y, x) the body returns with rows 0 and 1 and the translation still the identity's (shipped
// quirk). Otherwise row 0 = cross(up, row 2), row 1 = cross(row 2, row 0), each normalised
// through PE 0x100777A0, and the translation is the eye.
inline Mat4& calcLookAtMatrix(Mat4& m, const Vec3& eye, const Vec3& target, const Vec3& up)
{
    for (int i = 0; i < 16; ++i)
        m.m[i] = 0.0f;
    m.m[0] = m.m[5] = m.m[10] = m.m[15] = 1.0f;
    Vec3& row0 = *reinterpret_cast<Vec3*>(&m.m[0]);
    Vec3& row1 = *reinterpret_cast<Vec3*>(&m.m[4]);
    Vec3& row2 = *reinterpret_cast<Vec3*>(&m.m[8]);
    row2.x = st(ld(target.x) - ld(eye.x));
    row2.y = st(ld(target.y) - ld(eye.y));
    row2.z = st(ld(target.z) - ld(eye.z));
    const Real length = squareRootOf((ld(row2.x) * ld(row2.x) + ld(row2.y) * ld(row2.y))
                                     + ld(row2.z) * ld(row2.z));
    if (length < ld(kEpsilon))
    {
        m.m[1] = m.m[2] = m.m[3] = 0.0f;
        m.m[0] = 1.0f;
        m.m[5] = 1.0f;
        m.m[4] = m.m[6] = m.m[7] = 0.0f;
        m.m[10] = 1.0f;
        m.m[8] = m.m[9] = m.m[11] = 0.0f;
        m.m[15] = 1.0f;
        m.m[13] = m.m[14] = m.m[12] = 0.0f;
        return m;
    }
    const Real inverse = ld(1.0f) / length;
    row2.x = st(inverse * ld(row2.x));
    row2.y = st(inverse * ld(row2.y));
    row2.z = st(inverse * ld(row2.z));
    if (absOf(((ld(row2.z) * ld(up.z) + ld(row2.y) * ld(up.y)) + ld(row2.x) * ld(up.x))
              - ld(1.0f)) < ld(kEpsilon))
        return m;
    if (absOf(((ld(row2.z) * ld(up.z) + ld(row2.y) * ld(up.y)) + ld(row2.x) * ld(up.x))
              + ld(1.0f)) < ld(kEpsilon))
        return m;
    cross(row0, up, row2);
    normalize(row0);
    cross(row1, row2, row0);
    normalize(row1);
    m.m[12] = eye.x;
    m.m[13] = eye.y;
    m.m[14] = eye.z;
    return m;
}

// PE 0x10079460 [INFERRED] the Mat4 four-row setter copy (thiscall on the matrix): row 0, 1, 2
// and 3 copied in order, four floats each.
inline void setRows(float* m, const float* row0, const float* row1, const float* row2,
                    const float* row3)
{
    for (int i = 0; i < 4; ++i)
    {
        m[i]      = row0[i];
        m[4 + i]  = row1[i];
        m[8 + i]  = row2[i];
        m[12 + i] = row3[i];
    }
}

// PE 0x100795B0 [INFERRED] Mat4(row0, row1, row2, row3): zeroed, then the four rows through
// setRows (PE 0x10079460).
inline Mat4 mat4FromRows(const Vec4& row0, const Vec4& row1, const Vec4& row2, const Vec4& row3)
{
    Mat4 out;
    for (int i = 0; i < 16; ++i)
        out.m[i] = 0.0f;
    const Vec4* const rows[4] = { &row0, &row1, &row2, &row3 };
    for (int r = 0; r < 4; ++r)
    {
        out.m[r * 4 + 0] = rows[r]->x;
        out.m[r * 4 + 1] = rows[r]->y;
        out.m[r * 4 + 2] = rows[r]->z;
        out.m[r * 4 + 3] = rows[r]->w;
    }
    return out;
}

// PE 0x10079610 [INFERRED] Mat4 transformPoint copy (thiscall on the matrix, then in and out):
// z = ((m2*x + m10*z) + m6*y) + m14 and y = ((m1*x + m9*z) + m5*y) + m13 are formed before
// x = ((m8*z + m4*y) + x*m0) + m12 is stored, so out may alias in.
inline void transformPoint(const Mat4& m, const Vec3& in, Vec3& out)
{
    const Real z = ((ld(m.m[2]) * ld(in.x) + ld(m.m[10]) * ld(in.z)) + ld(m.m[6]) * ld(in.y))
                   + ld(m.m[14]);
    const Real y = ((ld(m.m[1]) * ld(in.x) + ld(m.m[9]) * ld(in.z)) + ld(m.m[5]) * ld(in.y))
                   + ld(m.m[13]);
    const Real x = ((ld(m.m[8]) * ld(in.z) + ld(m.m[4]) * ld(in.y)) + ld(in.x) * ld(m.m[0]))
                   + ld(m.m[12]);
    out.x = st(x);
    out.y = st(y);
    out.z = st(z);
}

// PE 0x10079670 [INFERRED] Plane3 signed distance, (n.z*p.z + n.y*p.y) + n.x*p.x + d, left on the
// x87 stack (callers compare it unstored).
inline Real planeDistance(const Plane3& plane, const Vec3& p)
{
    return ((ld(p.z) * ld(plane.normal.z) + ld(p.y) * ld(plane.normal.y))
            + ld(p.x) * ld(plane.normal.x)) + ld(plane.d);
}

// PE 0x100796A0 [INFERRED] LocalBoundingBox::expand copy: per axis x, y, z the maximum moves on an
// ordered p > max and the minimum on an ordered p < min; the dirty byte (+0x20) is set.
inline void expand(LocalBoundingBox& box, const Vec3& p)
{
    if (p.x > box.m_max.x) box.m_max.x = p.x;
    if (p.x < box.m_min.x) box.m_min.x = p.x;
    if (p.y > box.m_max.y) box.m_max.y = p.y;
    if (p.y < box.m_min.y) box.m_min.y = p.y;
    if (p.z > box.m_max.z) box.m_max.z = p.z;
    if (p.z < box.m_min.z) box.m_min.z = p.z;
    box.m_derivedDataDirty = true;
}

// PE 0x100797C0 [INFERRED] Frustum::getCornerForPlane copy (jump table 0x10079950). A plane above
// 5 or a corner above 3 emits Debug type 6 "Invalid corner or plane." (Math/Frustum.cpp:430),
// which the host treats as fatal, then answers corner 1.
inline const Vec3& frustumCornerForPlane(const Frustum& frustum, unsigned int corner,
                                         unsigned int plane)
{
    static const unsigned int corners[6][4] = {
        {5, 4, 0, 1}, {3, 2, 6, 7}, {0, 4, 6, 2},
        {5, 7, 3, 1}, {1, 0, 2, 3}, {7, 6, 4, 5}
    };
    if (plane <= 5 && corner <= 3)
        return frustum.m_corners[corners[plane][corner]];
    ::renddx9abi::Debug(DebugTypeError,
        "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Math\\Frustum.cpp", 430, "")
        << "Invalid corner or plane.";
    return frustum.m_corners[1];
}

// One plane's sum for the two sphere tests: ((n.z*p.z + n.y*p.y) + n.x*p.x) + d.
inline Real frustumPlaneSum(const Plane3& plane, const Vec3& p)
{
    return ((ld(plane.normal.z) * ld(p.z) + ld(plane.normal.y) * ld(p.y))
            + ld(plane.normal.x) * ld(p.x)) + ld(plane.d);
}

// PE 0x10079A60 [INFERRED] Frustum::inside(p, radius) copy: planes 5, 4, 1, 0, 2, then 3 through
// planeDistance (PE 0x10079670); a plane rejects only on an ordered sum > radius.
inline bool frustumInside(const Frustum& frustum, const Vec3& p, float radius)
{
    static const int order[5] = { 5, 4, 1, 0, 2 };
    for (int i = 0; i < 5; ++i)
        if (frustumPlaneSum(frustum.m_planes[order[i]], p) > ld(radius))
            return false;
    return !(planeDistance(frustum.m_planes[3], p) > ld(radius));
}

// PE 0x10079B40 [INFERRED] Frustum::inside(p, radius, fullyInside) copy: planes 4, 5, 0, 1, 2,
// then 3 through PE 0x10079670, each rejecting on an ordered sum > radius. Every sum but plane 3's
// is stored as a float; on success fullyInside is the AND of the six (sum + radius) sign bits,
// each sum + radius stored as a float. fullyInside is untouched on false.
inline bool frustumInside(const Frustum& frustum, const Vec3& p, float radius, bool& fullyInside)
{
    static const int order[5] = { 4, 5, 0, 1, 2 };
    float sums[5];
    for (int i = 0; i < 5; ++i)
    {
        const Real sum = frustumPlaneSum(frustum.m_planes[order[i]], p);
        sums[i] = st(sum);
        if (sum > ld(radius))
            return false;
    }
    const Real last = planeDistance(frustum.m_planes[3], p);
    if (last > ld(radius))
        return false;
    // stored in the PE's order: planes 0, 1, 4, 5, 2, then 3
    const float shifted[6] = {
        st(ld(sums[2]) + ld(radius)), st(ld(sums[3]) + ld(radius)),
        st(ld(sums[0]) + ld(radius)), st(ld(sums[1]) + ld(radius)),
        st(ld(sums[4]) + ld(radius)), st(last + ld(radius))
    };
    unsigned int all = 0xFFFFFFFFu;
    for (int i = 0; i < 6; ++i)
    {
        unsigned int bits;
        std::memcpy(&bits, &shifted[i], sizeof(bits));
        all &= bits;
    }
    fullyInside = (all & 0x80000000u) == 0x80000000u;
    return true;
}

// PE 0x10079C90 [INFERRED] the three-plane intersection copy (p1 in edx, p2 and p3 on the stack):
// (-d1 (n2 x n3) + -d2 (n3 x n1) + -d3 (n1 x n2)) / (n1 . (n2 x n3)), with the PE's float stores
// (the denominator keeps (n2 x n3).x unstored).
inline Vec3 intersectPlanes(const Plane3& p1, const Plane3& p2, const Plane3& p3)
{
    const Vec3& a = p1.normal;
    const Vec3& b = p2.normal;
    const Vec3& c = p3.normal;
    const Real bcX = ld(c.z) * ld(b.y) - ld(c.y) * ld(b.z);
    const Real bcY = ld(c.x) * ld(b.z) - ld(c.z) * ld(b.x);
    const float bcYStored = st(bcY);
    const float bcZStored = st(ld(c.y) * ld(b.x) - ld(c.x) * ld(b.y));
    const Real abX = ld(a.y) * ld(b.z) - ld(a.z) * ld(b.y);
    const Real abY = ld(b.x) * ld(a.z) - ld(a.x) * ld(b.z);
    const float abZStored = st(ld(a.x) * ld(b.y) - ld(a.y) * ld(b.x));
    const float negD3 = st(-ld(p3.d));
    const float abXScaled = st(abX * ld(negD3));
    const Real abYScaled = abY * ld(negD3);
    const float abZScaled = st(ld(abZStored) * ld(negD3));
    const Real caX = ld(c.y) * ld(a.z) - ld(a.y) * ld(c.z);
    const Real caY = ld(a.x) * ld(c.z) - ld(c.x) * ld(a.z);
    const float caZStored = st(ld(a.y) * ld(c.x) - ld(a.x) * ld(c.y));
    const float negD2 = st(-ld(p2.d));
    const float caXScaled = st(caX * ld(negD2));
    const Real caYScaled = caY * ld(negD2);
    const Real caZScaled = ld(caZStored) * ld(negD2);
    const float bcYAgain = st(bcY);
    const float negD1 = st(-ld(p1.d));
    const float bcXScaled = st(bcX * ld(negD1));
    const Real bcYScaled = ld(bcYAgain) * ld(negD1);
    const float bcZScaled = st(ld(bcZStored) * ld(negD1));
    const float sumX = st(ld(bcXScaled) + ld(caXScaled));
    const float sumY = st(bcYScaled + caYScaled);
    const float sumZ = st(ld(bcZScaled) + caZScaled);
    const float numeratorX = st(ld(sumX) + ld(abXScaled));
    const float numeratorY = st(ld(sumY) + abYScaled);
    const float numeratorZ = st(ld(sumZ) + ld(abZScaled));
    const Real inverse = ld(1.0f)
        / ((bcX * ld(a.x) + ld(bcZStored) * ld(a.z)) + ld(bcYStored) * ld(a.y));
    Vec3 out;
    out.x = st(ld(numeratorX) * inverse);
    out.y = st(ld(numeratorY) * inverse);
    out.z = st(ld(numeratorZ) * inverse);
    return out;
}

// PE 0x10079E50 [INFERRED] Frustum::recalcCorners copy: the eight corners through PE 0x10079C90,
// the box from corners 0 and 1 with its dirty byte set, then expanded by corners 2..7 through
// expand (PE 0x100796A0).
inline void frustumRecalcCorners(Frustum& frustum)
{
    const Plane3* const p = frustum.m_planes;
    frustum.m_corners[0] = intersectPlanes(p[0], p[2], p[4]);
    frustum.m_corners[1] = intersectPlanes(p[0], p[3], p[4]);
    frustum.m_corners[2] = intersectPlanes(p[1], p[2], p[4]);
    frustum.m_corners[3] = intersectPlanes(p[1], p[3], p[4]);
    frustum.m_corners[4] = intersectPlanes(p[0], p[2], p[5]);
    frustum.m_corners[5] = intersectPlanes(p[0], p[3], p[5]);
    frustum.m_corners[6] = intersectPlanes(p[1], p[2], p[5]);
    frustum.m_corners[7] = intersectPlanes(p[1], p[3], p[5]);
    frustum.m_boundingBox.m_min = frustum.m_corners[0];
    frustum.m_boundingBox.m_max = frustum.m_corners[1];
    frustum.m_boundingBox.m_derivedDataDirty = true;
    for (int i = 2; i < 8; ++i)
        expand(frustum.m_boundingBox, frustum.m_corners[i]);
}

// PE 0x10079FE0 [INFERRED] Frustum::updateSecondarySizes copy: the corners (PE 0x10079E50), the
// squared radius 0 then raised by every corner's (dz*dz + dy*dy) + dx*dx (d = centre - corner, dx
// stored) on an ordered greater compare (corner 0 against the constant 0), the radius its square
// root, the corners copied to the second set 0..7, the dirty byte cleared.
inline void frustumUpdateSecondarySizes(Frustum& frustum)
{
    frustumRecalcCorners(frustum);
    frustum.m_boundingRadiusSq = 0.0f;
    for (int i = 0; i < 8; ++i)
    {
        const Vec3& corner = frustum.m_corners[i];
        const float dx = st(ld(frustum.m_center.x) - ld(corner.x));
        const Real dy = ld(frustum.m_center.y) - ld(corner.y);
        const Real dz = ld(frustum.m_center.z) - ld(corner.z);
        const Real distance = (dz * dz + dy * dy) + ld(dx) * ld(dx);
        const float best = i == 0 ? 0.0f : frustum.m_boundingRadiusSq;
        if (distance > ld(best))
            frustum.m_boundingRadiusSq = st(distance);
    }
    frustum.m_boundingRadius = st(squareRootOf(ld(frustum.m_boundingRadiusSq)));
    for (int i = 0; i < 8; ++i)
        frustum.m_prevCorners[i] = frustum.m_corners[i];
    frustum.m_cornersDirty = false;
}

// PE 0x1007C530 [INFERRED] Plane3::transform copy into out (callers pass the source plane as out):
// the point -d n and the rotated normal, then d = -(t . n') summed z, y, x with t the transformed
// point. Every read of the source precedes the first store.
inline void planeTransform(Plane3& out, const Plane3& plane, const Mat4& m)
{
    const float* const t = m.m;
    const float d = plane.d;
    const float pxStored = st(-ld(plane.normal.x) * ld(d));
    const Real py = -ld(plane.normal.y) * ld(d);
    const Real pz = ld(st(-ld(plane.normal.z))) * ld(d);
    const Real nz = (ld(plane.normal.z) * ld(t[10]) + ld(t[6]) * ld(plane.normal.y))
                    + ld(t[2]) * ld(plane.normal.x);
    const Real ny = (ld(plane.normal.z) * ld(t[9]) + ld(plane.normal.y) * ld(t[5]))
                    + ld(t[1]) * ld(plane.normal.x);
    const Real nx = (ld(plane.normal.x) * ld(t[0]) + ld(t[8]) * ld(plane.normal.z))
                    + ld(plane.normal.y) * ld(t[4]);
    const float nzStored = st(nz);
    const float nyStored = st(ny);
    out.normal.x = st(nx);
    out.normal.y = nyStored;
    out.normal.z = nzStored;
    const float tx = st(((py * ld(t[4]) + ld(pxStored) * ld(t[0])) + pz * ld(t[8])) + ld(t[12]));
    const float ty = st(((pz * ld(t[9]) + ld(pxStored) * ld(t[1])) + py * ld(t[5])) + ld(t[13]));
    const Real tz = ((pz * ld(t[10]) + py * ld(t[6])) + ld(pxStored) * ld(t[2])) + ld(t[14]);
    out.d = st(-((tz * ld(nzStored) + ld(ty) * ld(nyStored)) + ld(tx) * ld(out.normal.x)));
}

// PE 0x1007A460 [INFERRED] Frustum::setPerspective copy: the setup scalars and type 0; the near and
// far planes (d = -(near * -1) and -far, no +0 term); the four side planes from the half angles
// (fov/2 / aspect for planes 0/1, fov/2 for planes 2/3), each normalised through PE 0x100777A0
// with d = -((z*0 + y*0) + x*0); every plane through PE 0x1007C530 in place with its sign-bit
// outcode; the transformation; the centre (0, 0, (near + far) / 2) through the matrix; then the
// secondary sizes (PE 0x10079FE0).
inline void frustumSetPerspective(Frustum& frustum, float fov, float aspect, float nearDist,
                                  float farDist, const Mat4& m)
{
    const Real halfFov = ld(fov) * ld(0.5f);
    const float halfFovStored = st(halfFov);
    const float halfFovAspect = st(halfFov / ld(aspect));
    frustum.m_fov = fov;
    frustum.m_aspect = aspect;
    frustum.m_nearDist = nearDist;
    frustum.m_farDist = farDist;
    frustum.m_type = 0;
    float sinA, cosA, sinB, cosB;
    sineCosine(halfFovAspect, sinA, cosA);
    sineCosine(halfFovStored, sinB, cosB);
    Plane3* const planes = frustum.m_planes;
    planes[4].normal.x = 0.0f;
    planes[4].normal.y = 0.0f;
    planes[4].normal.z = -1.0f;
    planes[4].d = st(-(ld(nearDist) * ld(-1.0f)));
    planes[5].normal.x = 0.0f;
    planes[5].normal.y = 0.0f;
    planes[5].normal.z = 1.0f;
    planes[5].d = st(-ld(farDist));
    const Vec3 sides[4] = {
        { -cosA, 0.0f, -sinA }, { cosA, 0.0f, -sinA },
        { 0.0f, cosB, -sinB },  { 0.0f, -cosB, -sinB }
    };
    for (int i = 0; i < 4; ++i)
    {
        Vec3 n = sides[i];
        normalize(n);
        planes[i].normal = n;
        planes[i].d = st(-((ld(n.z) * ld(0.0f) + ld(n.y) * ld(0.0f)) + ld(n.x) * ld(0.0f)));
    }
    for (int i = 0; i < 6; ++i)
    {
        planeTransform(planes[i], planes[i], m);
        unsigned int bits[3];
        std::memcpy(bits, &planes[i].normal, sizeof(bits));
        frustum.m_planeOutcodes[i] = static_cast<int>((bits[0] >> 31)
            | ((bits[1] >> 30) & 2u) | ((bits[2] >> 29) & 4u));
    }
    frustum.m_transformation = m;
    frustum.m_center.x = 0.0f;
    frustum.m_center.y = 0.0f;
    frustum.m_center.z = st((ld(nearDist) + ld(farDist)) * ld(0.5f));
    const float* const t = m.m;
    const float cx = frustum.m_center.x;
    const float cy = frustum.m_center.y;
    const float cz = frustum.m_center.z;
    const Real z = ((ld(t[6]) * ld(cy) + ld(t[10]) * ld(cz)) + ld(cx) * ld(t[2])) + ld(t[14]);
    const Real y = ((ld(t[5]) * ld(cy) + ld(t[9]) * ld(cz)) + ld(cx) * ld(t[1])) + ld(t[13]);
    const Real x = ((ld(cy) * ld(t[4]) + ld(cx) * ld(t[0])) + ld(cz) * ld(t[8])) + ld(t[12]);
    frustum.m_center.x = st(x);
    frustum.m_center.y = st(y);
    frustum.m_center.z = st(z);
    frustumUpdateSecondarySizes(frustum);
}

// PE 0x1007A7F0 [INFERRED] Frustum::setOrtho copy: width, near, far, height and type 1; the six
// axial planes (d = -((w * -0.5) * -1), -(w * 0.5), and the same for the height, -(near * -1),
// -far, no +0 terms); every plane through PE 0x1007C530 in place with its sign-bit outcode; the
// transformation; the provisional radius (near + far) / 2 and its square (both from the unstored
// midpoint), the centre (0, 0, midpoint) through the matrix (each chain from the stored centre,
// summed as listed), then PE 0x10079FE0.
inline void frustumSetOrtho(Frustum& frustum, float width, float height, float nearDist,
                            float farDist, const Mat4& m)
{
    frustum.m_dword1D4 = width;
    frustum.m_nearDist = nearDist;
    frustum.m_farDist = farDist;
    frustum.m_dword1D8 = height;
    frustum.m_type = 1;
    Plane3* const planes = frustum.m_planes;
    planes[0].normal.x = -1.0f;
    planes[0].normal.y = 0.0f;
    planes[0].normal.z = 0.0f;
    planes[0].d = st(-((ld(width) * ld(-0.5f)) * ld(-1.0f)));
    planes[1].normal.x = 1.0f;
    planes[1].normal.y = 0.0f;
    planes[1].normal.z = 0.0f;
    planes[1].d = st(-(ld(width) * ld(0.5f)));
    planes[3].normal.x = 0.0f;
    planes[3].normal.y = -1.0f;
    planes[3].normal.z = 0.0f;
    planes[3].d = st(-((ld(height) * ld(-0.5f)) * ld(-1.0f)));
    planes[2].normal.x = 0.0f;
    planes[2].normal.y = 1.0f;
    planes[2].normal.z = 0.0f;
    planes[2].d = st(-(ld(height) * ld(0.5f)));
    planes[4].normal.x = 0.0f;
    planes[4].normal.y = 0.0f;
    planes[4].normal.z = -1.0f;
    planes[4].d = st(-(ld(nearDist) * ld(-1.0f)));
    planes[5].normal.x = 0.0f;
    planes[5].normal.y = 0.0f;
    planes[5].normal.z = 1.0f;
    planes[5].d = st(-ld(farDist));
    for (int i = 0; i < 6; ++i)
    {
        planeTransform(planes[i], planes[i], m);
        unsigned int bits[3];
        std::memcpy(bits, &planes[i].normal, sizeof(bits));
        frustum.m_planeOutcodes[i] = static_cast<int>((bits[0] >> 31)
            | ((bits[1] >> 30) & 2u) | ((bits[2] >> 29) & 4u));
    }
    frustum.m_transformation = m;
    const Real midpoint = (ld(nearDist) + ld(farDist)) * ld(0.5f);
    frustum.m_boundingRadius = st(midpoint);
    frustum.m_boundingRadiusSq = st(midpoint * midpoint);
    frustum.m_center.x = 0.0f;
    frustum.m_center.y = 0.0f;
    frustum.m_center.z = st(midpoint);
    const float* const t = m.m;
    const float cx = frustum.m_center.x;
    const float cy = frustum.m_center.y;
    const float cz = frustum.m_center.z;
    const Real z = ((ld(t[10]) * ld(cz) + ld(t[6]) * ld(cy)) + ld(t[2]) * ld(cx)) + ld(t[14]);
    const Real y = ((ld(t[9]) * ld(cz) + ld(t[5]) * ld(cy)) + ld(t[1]) * ld(cx)) + ld(t[13]);
    const Real x = ((ld(t[0]) * ld(cx) + ld(t[4]) * ld(cy)) + ld(t[8]) * ld(cz)) + ld(t[12]);
    frustum.m_center.x = st(x);
    frustum.m_center.y = st(y);
    frustum.m_center.z = st(z);
    frustumUpdateSecondarySizes(frustum);
}

// PE 0x1007AA60 [INFERRED] Frustum::reSetup copy: type 0 rebuilds through PE 0x1007A460 (fov,
// aspect, near, far), type 1 through PE 0x1007A7F0 (width, height, near, far), both with the
// frustum's own transformation; any other type does nothing.
inline void frustumReSetup(Frustum& frustum)
{
    if (frustum.m_type == 0)
        frustumSetPerspective(frustum, frustum.m_fov, frustum.m_aspect, frustum.m_nearDist,
                              frustum.m_farDist, frustum.m_transformation);
    else if (frustum.m_type == 1)
        frustumSetOrtho(frustum, frustum.m_dword1D4, frustum.m_dword1D8, frustum.m_nearDist,
                        frustum.m_farDist, frustum.m_transformation);
}

// PE 0x1007AAD0 [INFERRED] Frustum::transform copy: the matrix replaces the transformation
// (PE 0x10030E80), then PE 0x1007AA60.
inline void frustumTransform(Frustum& frustum, const Mat4& m)
{
    frustum.m_transformation = m;
    frustumReSetup(frustum);
}

// PE 0x1007AB00 [INFERRED] Frustum::setFov copy: the store, then PE 0x1007AA60. Unreachable in
// 1.50: no code or data references.
inline void frustumSetFov(Frustum& frustum, float fov)
{
    frustum.m_fov = fov;
    frustumReSetup(frustum);
}

// PE 0x1007AB20 [INFERRED] Frustum::setNearDist copy: the store, then PE 0x1007AA60. Unreachable
// in 1.50: no code or data references.
inline void frustumSetNearDist(Frustum& frustum, float nearDist)
{
    frustum.m_nearDist = nearDist;
    frustumReSetup(frustum);
}

// PE 0x1007AB40 [INFERRED] Frustum::setFarDist copy: the store, then PE 0x1007AA60.
inline void frustumSetFarDist(Frustum& frustum, float farDist)
{
    frustum.m_farDist = farDist;
    frustumReSetup(frustum);
}

// PE 0x1007AB60 [INFERRED] Frustum::operator= copy: fov, aspect, width, height, near, far and
// type in that order, the transformation (PE 0x10030E80), then PE 0x1007AA60 rebuilds the rest.
inline Frustum& frustumAssign(Frustum& frustum, const Frustum& other)
{
    frustum.m_fov = other.m_fov;
    frustum.m_aspect = other.m_aspect;
    frustum.m_dword1D4 = other.m_dword1D4;
    frustum.m_dword1D8 = other.m_dword1D8;
    frustum.m_nearDist = other.m_nearDist;
    frustum.m_farDist = other.m_farDist;
    frustum.m_type = other.m_type;
    frustum.m_transformation = other.m_transformation;
    frustumReSetup(frustum);
    return frustum;
}

// The length of p.d * p.n + q.d * q.n as the expandToEncapsulate copy evaluates it: q's x product kept unstored,
// every other product stored, the sums unstored, sqrt((z*z + y*y) + x*x).
inline float frustumPlanePairLength(const Plane3& p, const Plane3& q)
{
    const Real qx = ld(q.d) * ld(q.normal.x);
    const float qy = st(ld(q.d) * ld(q.normal.y));
    const float qz = st(ld(q.d) * ld(q.normal.z));
    const float px = st(ld(p.d) * ld(p.normal.x));
    const float py = st(ld(p.d) * ld(p.normal.y));
    const float pz = st(ld(p.d) * ld(p.normal.z));
    const Real x = qx + ld(px);
    const Real y = ld(py) + ld(qy);
    const Real z = ld(pz) + ld(qz);
    return st(squareRootOf((z * z + y * y) + x * x));
}

// PE 0x1007ABE0 [INFERRED] Frustum::expandToEncapsulate copy: per plane the other frustum's eight
// corners projected ((c.z*n.z + c.y*n.y) + n.x*c.x); the maximum (stored) takes every projection
// that is not below it, the minimum every one below it, both compared unstored; d becomes the
// maximum when d > 0, else the minimum (no negation, as shipped). Then width, height and far from
// the plane pairs (1, 0), (3, 2), (4, 5), the translation moved by -d4 * n4, and PE 0x1007AA60
// inline. Unreachable in 1.50: no code or data references.
inline void frustumExpandToEncapsulate(Frustum& frustum, const Frustum& other)
{
    for (int i = 0; i < 6; ++i)
    {
        Plane3& plane = frustum.m_planes[i];
        float maximum = -3.40282347e+38f;
        float minimum = 3.40282347e+38f;
        for (int j = 0; j < 8; ++j)
        {
            const Vec3& corner = other.m_corners[j];
            const Real projection = (ld(corner.z) * ld(plane.normal.z)
                                     + ld(corner.y) * ld(plane.normal.y))
                                    + ld(plane.normal.x) * ld(corner.x);
            const float stored = st(projection);
            if (!(projection < ld(maximum)))
                maximum = stored;
            if (projection < ld(minimum))
                minimum = stored;
        }
        plane.d = plane.d > 0.0f ? maximum : minimum;
    }
    const Plane3* const p = frustum.m_planes;
    frustum.m_dword1D4 = frustumPlanePairLength(p[0], p[1]);
    frustum.m_dword1D8 = frustumPlanePairLength(p[2], p[3]);
    frustum.m_farDist = frustumPlanePairLength(p[5], p[4]);
    float* const t = frustum.m_transformation.m;
    const Real nearX = ld(p[4].d) * ld(p[4].normal.x);
    const float nearY = st(ld(p[4].d) * ld(p[4].normal.y));
    const float nearZ = st(ld(p[4].d) * ld(p[4].normal.z));
    t[12] = st(ld(t[12]) - nearX);
    t[13] = st(ld(t[13]) - ld(nearY));
    t[14] = st(ld(t[14]) - ld(nearZ));
    frustumReSetup(frustum);
}

// PE 0x1007AFE0 [INFERRED] Vec3 difference a - b (b is the thiscall receiver), written x, y, z.
inline Vec3 subtract(const Vec3& a, const Vec3& b)
{
    Vec3 out;
    out.x = st(ld(a.x) - ld(b.x));
    out.y = st(ld(a.y) - ld(b.y));
    out.z = st(ld(a.z) - ld(b.z));
    return out;
}

// PE 0x1007B0D0 [INFERRED] zero each component whose magnitude is strictly below epsilon (an
// unordered component stays).
inline void snapToZero(Vec3& v, float epsilon)
{
    if (std::fabs(v.x) < epsilon) v.x = 0.0f;
    if (std::fabs(v.y) < epsilon) v.y = 0.0f;
    if (std::fabs(v.z) < epsilon) v.z = 0.0f;
}

// PE 0x1007B110 [INFERRED] Vec3::makeOrthonormalBasis copy, receiver `third`: forward and up
// normalised, third = forward x up normalised, up = third x forward (its normalise result
// ignored), forward = up x third normalised; any failed checked normalise answers -20003 at once.
// Then third, forward and up snap components below FLT_EPSILON to zero, and 0 is answered.
inline int makeOrthonormalBasis(Vec3& third, Vec3& forward, Vec3& up)
{
    if (normalize(forward))
        return -20003;
    if (normalize(up))
        return -20003;
    third.x = st(ld(up.z) * ld(forward.y) - ld(up.y) * ld(forward.z));
    third.y = st(ld(forward.z) * ld(up.x) - ld(up.z) * ld(forward.x));
    third.z = st(ld(up.y) * ld(forward.x) - ld(forward.y) * ld(up.x));
    if (normalize(third))
        return -20003;
    cross(up, third, forward);
    normalize(up);
    cross(forward, up, third);
    if (normalize(forward))
        return -20003;
    snapToZero(third, kEpsilon);
    snapToZero(forward, kEpsilon);
    snapToZero(up, kEpsilon);
    return 0;
}

// PE 0x1007B1E0 [INFERRED] the function-local zero vector (guard bit 1 of dword_10247BF8).
inline const Vec3& zeroVector()
{
    static const Vec3 zero = { 0.0f, 0.0f, 0.0f };
    return zero;
}

// PE 0x1007B220 [INFERRED] the function-local unit vector (1, 1, 1) (guard bit 1 of
// dword_10247C08): the templates' default compression scale.
inline const Vec3& unitScale()
{
    static const Vec3 one = { 1.0f, 1.0f, 1.0f };
    return one;
}

// PE 0x1007C440 [INFERRED] Mat4 transformVector copy: transformPoint without the translation row;
// z and y are formed before x is stored, so out may alias in.
inline void transformVector(const Mat4& m, const Vec3& in, Vec3& out)
{
    const Real z = (ld(m.m[2]) * ld(in.x) + ld(m.m[10]) * ld(in.z)) + ld(m.m[6]) * ld(in.y);
    const Real y = (ld(m.m[1]) * ld(in.x) + ld(m.m[9]) * ld(in.z)) + ld(m.m[5]) * ld(in.y);
    const Real x = (ld(m.m[8]) * ld(in.z) + ld(m.m[4]) * ld(in.y)) + ld(in.x) * ld(m.m[0]);
    out.x = st(x);
    out.y = st(y);
    out.z = st(z);
}

// PE 0x1007C4C0 [INFERRED] v = v * m in place: the source copied first, then
// x = (z*m6 + y*m3) + x*m0, y = (z*m7 + x*m1) + y*m4, z = (z*m8 + x*m2) + y*m5.
inline Vec3& multiplyInPlace(Vec3& v, const Mat3& m)
{
    const Vec3 source = v;
    const Real z = (ld(source.z) * ld(m.m[8]) + ld(source.x) * ld(m.m[2]))
                   + ld(source.y) * ld(m.m[5]);
    const Real y = (ld(source.z) * ld(m.m[7]) + ld(source.x) * ld(m.m[1]))
                   + ld(source.y) * ld(m.m[4]);
    const Real x = (ld(source.z) * ld(m.m[6]) + ld(source.y) * ld(m.m[3]))
                   + ld(source.x) * ld(m.m[0]);
    v.x = st(x);
    v.y = st(y);
    v.z = st(z);
    return v;
}

// PE 0x1007C620 [INFERRED] Plane3::projectVector copy: v through the matrix
// [yy+zz, -xy, -xz; -xy, xx+zz, -yz; -xz, -yz, xx+yy] of the plane normal (each off-diagonal
// product times -1.0f, every element stored as a float) via PE 0x1007C4C0, into out.
inline void projectVector(const Plane3& plane, const Vec3& v, Vec3& out)
{
    const Vec3& n = plane.normal;
    const Real xx = ld(n.x) * ld(n.x);
    const Real yy = ld(n.y) * ld(n.y);
    const Real zz = ld(n.z) * ld(n.z);
    Mat3 matrix;
    matrix.m[0] = st(zz + yy);
    matrix.m[4] = st(zz + xx);
    matrix.m[8] = st(yy + xx);
    matrix.m[1] = matrix.m[3] = st(ld(n.y) * ld(n.x) * ld(-1.0f));
    matrix.m[2] = matrix.m[6] = st(ld(n.z) * ld(n.x) * ld(-1.0f));
    matrix.m[5] = matrix.m[7] = st(ld(n.z) * ld(n.y) * ld(-1.0f));
    Vec3 projected = v;
    multiplyInPlace(projected, matrix);
    out = projected;
}

// PE 0x1007C6C0 [INFERRED] Plane3::intersectLine copy: the segment a..b against the plane. A
// denominator ((b-a).x n.x + (b-a).z n.z) + (b-a).y n.y comparing equal to 0 answers false and
// writes nothing; otherwise t = ((p-a) . n, same order) / denominator with p = -d n stored,
// insideSegment = (0 <= t <= 1, ordered), out = a + (b-a) t with the PE's float stores, true.
inline bool intersectLine(const Plane3& plane, const Vec3& a, const Vec3& b, Vec3& out,
                          bool& insideSegment)
{
    const Vec3& n = plane.normal;
    const Real negD = -ld(plane.d);
    const float px = st(negD * ld(n.x));
    const float py = st(negD * ld(n.y));
    const float pz = st(negD * ld(n.z));
    const Real denominator = ((ld(b.x) - ld(a.x)) * ld(n.x) + (ld(b.z) - ld(a.z)) * ld(n.z))
                             + (ld(b.y) - ld(a.y)) * ld(n.y);
    if (denominator == ld(0.0f))
        return false;
    const Real numerator = ((ld(px) - ld(a.x)) * ld(n.x) + (ld(pz) - ld(a.z)) * ld(n.z))
                           + (ld(py) - ld(a.y)) * ld(n.y);
    const Real t = numerator / denominator;
    insideSegment = !(t < ld(0.0f)) && t <= ld(1.0f);
    const float dz = st(ld(b.z) - ld(a.z));
    const float scaledX = st((ld(b.x) - ld(a.x)) * t);
    const Real scaledY = (ld(b.y) - ld(a.y)) * t;
    const float scaledZ = st(ld(dz) * t);
    out.x = st(ld(scaledX) + ld(a.x));
    out.y = st(scaledY + ld(a.y));
    out.z = st(ld(scaledZ) + ld(a.z));
    return true;
}

// PE 0x100A2F00 [INFERRED] Mat4 *=: a copy of m (PE 0x10030DD0), then m = copy * rhs through the
// PE 0x10077A30 product.
inline Mat4& multiplyAssign(Mat4& m, const Mat4& rhs)
{
    const Mat4 copy = m;
    geom::geometryRendererMatrixProduct(m, copy, rhs);
    return m;
}

// ---------------------------------------------------------------------------------------------
// The remaining Mat4/Mat3/Frustum copies of the image. Names follow the symbolized Linux server's
// Math functions with the same shape (getRotationZXY, setRotateAboutLine, calcProjectionMatrix,
// Mat4::operator*, Frustum::inside, Mat3::setInverse/mult/fullMult/rotMult) and are [INFERRED].
// ---------------------------------------------------------------------------------------------

// float 0x42652EE0 (.data 0x10238748, read only by PE 0x10077F50), the radians-to-degrees factor.
const float kRadiansToDegrees = 57.2957764f;

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
// [CHOSEN, not recovered] fpatan: atan2(y, x) on the x87 values.
inline Real arcTangent(Real y, Real x) { return bf2x87::atanValue(y, x); }
#else
inline Real arcTangent(Real y, Real x) { return std::atan2(y, x); }
#endif
// MSVCR71 _CIasin on a float operand, answered at double precision.
inline Real arcSine(float value)
{
    return static_cast<Real>(std::asin(static_cast<double>(value)));
}

// PE 0x10077F50 [INFERRED] getRotationZXY copy (matrix in ecx, z in edx, x and y on the stack,
// retn 8): m9 not below 1 (or unordered) answers z = atan2(m2, m0) in degrees, x = 90, y = 0; m9 at
// or below -1 answers z = -(atan2(m2, m0) in degrees), x = -90, y = 0. Otherwise
// z = atan2(-m1, m5), x = asin(m9 clamped to [-1, 1] through PE 0x1000ED40) and y = atan2(-m8, m10),
// each in degrees. Every atan2 term whose two operands both have a magnitude below FLT_EPSILON is
// 0 instead. The outputs are written z, x, y, each matrix element read where the listing reads it.
// Unreachable in 1.50: no code or data reference.
inline void getRotationZXY(const Mat4& m, float& zDegrees, float& xDegrees, float& yDegrees)
{
    if (!(m.m[9] < 1.0f))
    {
        if (std::fabs(m.m[2]) < kEpsilon && std::fabs(m.m[0]) < kEpsilon)
            zDegrees = 0.0f;
        else
            zDegrees = st(arcTangent(ld(m.m[2]), ld(m.m[0])) * ld(kRadiansToDegrees));
        xDegrees = 90.0f;
        yDegrees = 0.0f;
        return;
    }
    if (!(m.m[9] > -1.0f))
    {
        if (std::fabs(m.m[2]) < kEpsilon && std::fabs(m.m[0]) < kEpsilon)
            zDegrees = 0.0f;
        else
            zDegrees = st(-(arcTangent(ld(m.m[2]), ld(m.m[0])) * ld(kRadiansToDegrees)));
        xDegrees = -90.0f;
        yDegrees = 0.0f;
        return;
    }
    if (std::fabs(m.m[1]) < kEpsilon && std::fabs(m.m[5]) < kEpsilon)
        zDegrees = 0.0f;
    else
        zDegrees = st(arcTangent(-ld(m.m[1]), ld(m.m[5])) * ld(kRadiansToDegrees));
    // The clamp of PE 0x1000ED40: the high bound first; an unordered value ends at the low bound.
    float sine = m.m[9];
    if (sine >= 1.0f)
        sine = 1.0f;
    if (!(sine >= -1.0f))
        sine = -1.0f;
    xDegrees = st(arcSine(sine) * ld(kRadiansToDegrees));
    if (std::fabs(m.m[8]) < kEpsilon && std::fabs(m.m[10]) < kEpsilon)
        yDegrees = 0.0f;
    else
        yDegrees = st(arcTangent(-ld(m.m[8]), ld(m.m[10])) * ld(kRadiansToDegrees));
}

// PE 0x10078B40 [INFERRED] setRotateAboutLine copy (matrix in ecx, axis in edx, degrees on the
// stack, retn 4): the up hint is (0, 0, 1), or (0, 1, 0) when the axis copy's |x| and |y| are both
// below 0.1f; third = (0, 0, 0) and PE 0x1007B110 builds the basis (third, axis, up), its result
// ignored. The basis rows are third, up, axis and (0, 0, 0, 1); m becomes inverse(basis) through
// the copy at PE 0x10077980, is rotated about its local z through PE 0x10079500 (an identity,
// PE 0x10077EC0, a copy of m and PE 0x10078180), then m = copy(m) * basis through PE 0x10077C50.
// Unreachable in 1.50: no code or data reference.
inline void setRotateAboutLine(Mat4& m, float degrees, const Vec3& axis)
{
    Vec3 line = axis;
    Vec3 up;
    up.x = 0.0f;
    up.y = 0.0f;
    up.z = 1.0f;
    if (std::fabs(line.x) < 0.1f && std::fabs(line.y) < 0.1f)
    {
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
    }
    Vec3 third;
    third.x = 0.0f;
    third.y = 0.0f;
    third.z = 0.0f;
    rendmath::makeOrthonormalBasis(third, line, up);
    Mat4 basis;
    basis.m[0] = third.x;  basis.m[1] = third.y;  basis.m[2] = third.z;   basis.m[3] = 0.0f;
    basis.m[4] = up.x;     basis.m[5] = up.y;     basis.m[6] = up.z;      basis.m[7] = 0.0f;
    basis.m[8] = line.x;   basis.m[9] = line.y;   basis.m[10] = line.z;   basis.m[11] = 0.0f;
    basis.m[12] = 0.0f;    basis.m[13] = 0.0f;    basis.m[14] = 0.0f;     basis.m[15] = 1.0f;
    rendmath::setInverse(m, basis);
    {
        // The body of PE 0x10079500 (psmType1RotateZ in ParticleInstanceType1Emission.inl).
        Mat4 rotation;
        for (int i = 0; i < 16; ++i)
            rotation.m[i] = 0.0f;
        rotation.m[0] = rotation.m[5] = rotation.m[10] = rotation.m[15] = 1.0f;
        rendmath::setRotateZDeg(rotation, degrees);
        const Mat4 copy = m;
        rendmath::rotMult(m, copy, rotation);
    }
    const Mat4 copy = m;
    rendmath::mult(m, copy, basis);
}

// PE 0x10078C80 [INFERRED] calcProjectionMatrix copy (matrix in ecx, untouched until the row
// setter; fov, aspect scale, near and far on the stack, retn 0x10). far - near (unstored) below
// FLT_EPSILON answers -20000 and writes nothing. Otherwise fsincos of the stored fov * 0.5f,
// depth = far / (far - near stored), the cotangent cos / sin kept unstored for x = aspect * cot.
// depth comparing equal to 1 sets the rows (x,0,0,0), (0,cot,0,0), (0,0,1,1), (0,0,-near,0); any
// other depth (unordered too) (x,0,0,0), (0,cot,0,0), (0,0,depth,1), (0,0,-(depth*near),0); both
// through PE 0x10079460, answering 0. Unreachable in 1.50: no code or data reference.
inline int calcProjectionMatrix(Mat4& m, float fov, float aspectScale, float nearDistance,
                                float farDistance)
{
    const Real distance = ld(farDistance) - ld(nearDistance);
    const float distanceStored = st(distance);
    if (distance < ld(kEpsilon))
        return -20000;
    const float halfAngle = st(ld(fov) * ld(0.5f));
    float sine, cosine;
    sineCosine(halfAngle, sine, cosine);
    const float depth = st(ld(farDistance) / ld(distanceStored));
    const Real cotangent = ld(cosine) / ld(sine);
    const float x = st(ld(aspectScale) * cotangent);
    const float cotangentStored = st(cotangent);
    if (depth == 1.0f)
    {
        const float row0[4] = { x, 0.0f, 0.0f, 0.0f };
        const float row1[4] = { 0.0f, cotangentStored, 0.0f, 0.0f };
        const float row2[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
        const float row3[4] = { 0.0f, 0.0f, -nearDistance, 0.0f };
        setRows(m.m, row0, row1, row2, row3);
        return 0;
    }
    const float row0[4] = { x, 0.0f, 0.0f, 0.0f };
    const float row1[4] = { 0.0f, cotangentStored, 0.0f, 0.0f };
    const float row2[4] = { 0.0f, 0.0f, depth, 1.0f };
    const float row3[4] = { 0.0f, 0.0f, st(-(ld(depth) * ld(nearDistance))), 0.0f };
    setRows(m.m, row0, row1, row2, row3);
    return 0;
}

// PE 0x10078F70 [INFERRED] Mat4::operator* copy (lhs in ecx, hidden result and rhs on the stack,
// retn 8). As the Linux server's operator* it stores the transposed product: result row r
// is (lhs * rhs) column r, i.e. result[r][c] = lhs row c . rhs column r. The rows are formed 3, 2,
// 1, 0 with the listing's summation orders and stored as floats; every operand is read before the
// result is zeroed (inline) and set through PE 0x10079460.
// Unreachable in 1.50: no code or data reference.
inline Mat4 mat4Product(const Mat4& lhs, const Mat4& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    float row3[4], row2[4], row1[4], row0[4];
    row3[0] = st(((ld(r[11]) * ld(l[2]) + ld(r[7]) * ld(l[1])) + ld(l[3]) * ld(r[15]))
                 + ld(r[3]) * ld(l[0]));
    row3[1] = st(((ld(r[11]) * ld(l[6]) + ld(r[3]) * ld(l[4])) + ld(l[7]) * ld(r[15]))
                 + ld(r[7]) * ld(l[5]));
    row3[2] = st(((ld(r[7]) * ld(l[9]) + ld(l[11]) * ld(r[15])) + ld(r[11]) * ld(l[10]))
                 + ld(l[8]) * ld(r[3]));
    row3[3] = st(((ld(r[11]) * ld(l[14]) + ld(r[3]) * ld(l[12])) + ld(r[7]) * ld(l[13]))
                 + ld(r[15]) * ld(l[15]));
    row2[0] = st(((ld(r[10]) * ld(l[2]) + ld(r[2]) * ld(l[0])) + ld(l[1]) * ld(r[6]))
                 + ld(r[14]) * ld(l[3]));
    row2[1] = st(((ld(r[14]) * ld(l[7]) + ld(r[10]) * ld(l[6])) + ld(l[5]) * ld(r[6]))
                 + ld(r[2]) * ld(l[4]));
    row2[2] = st(((ld(l[9]) * ld(r[6]) + ld(r[14]) * ld(l[11])) + ld(r[10]) * ld(l[10]))
                 + ld(l[8]) * ld(r[2]));
    row2[3] = st(((ld(r[14]) * ld(l[15]) + ld(r[10]) * ld(l[14])) + ld(l[13]) * ld(r[6]))
                 + ld(l[12]) * ld(r[2]));
    row1[0] = st(((ld(r[9]) * ld(l[2]) + ld(r[1]) * ld(l[0])) + ld(l[1]) * ld(r[5]))
                 + ld(r[13]) * ld(l[3]));
    row1[1] = st(((ld(r[13]) * ld(l[7]) + ld(r[9]) * ld(l[6])) + ld(l[5]) * ld(r[5]))
                 + ld(r[1]) * ld(l[4]));
    row1[2] = st(((ld(l[9]) * ld(r[5]) + ld(r[13]) * ld(l[11])) + ld(r[9]) * ld(l[10]))
                 + ld(l[8]) * ld(r[1]));
    row1[3] = st(((ld(r[13]) * ld(l[15]) + ld(r[9]) * ld(l[14])) + ld(l[13]) * ld(r[5]))
                 + ld(l[12]) * ld(r[1]));
    row0[0] = st(((ld(r[8]) * ld(l[2]) + ld(r[0]) * ld(l[0])) + ld(l[1]) * ld(r[4]))
                 + ld(r[12]) * ld(l[3]));
    row0[1] = st(((ld(r[12]) * ld(l[7]) + ld(r[8]) * ld(l[6])) + ld(l[5]) * ld(r[4]))
                 + ld(r[0]) * ld(l[4]));
    row0[2] = st(((ld(l[9]) * ld(r[4]) + ld(r[12]) * ld(l[11])) + ld(r[8]) * ld(l[10]))
                 + ld(l[8]) * ld(r[0]));
    row0[3] = st(((ld(r[12]) * ld(l[15]) + ld(r[8]) * ld(l[14])) + ld(l[13]) * ld(r[4]))
                 + ld(l[12]) * ld(r[0]));
    Mat4 result;
    for (int i = 0; i < 16; ++i)
        result.m[i] = 0.0f;
    setRows(result.m, row0, row1, row2, row3);
    return result;
}

// PE 0x10079970 [INFERRED] Frustum::inside(p) copy (thiscall, retn 4): planes 5, 4, 1, 0, 2 summed
// ((n.z*p.z + n.y*p.y) + n.x*p.x) + d, then plane 3 through PE 0x10079670; a plane rejects only on
// an ordered sum > 0 (float 0 at .rdata 0x101DDE64). The shared Math::Frustum::inside(const Vec3&)
// in Frustum.cpp spells the same planes, orders and compares in float; this copy carries the x87
// sums. Called at 0x101305F9 (NametagManager::update), 0x10161E2A and 0x10161EA1
// (fitFrustumToNodeCorners), 0x101BB3AB and 0x101BB3E7 (the shadow projection frustum tests).
inline bool frustumInside(const Frustum& frustum, const Vec3& p)
{
    static const int order[5] = { 5, 4, 1, 0, 2 };
    for (int i = 0; i < 5; ++i)
        if (frustumPlaneSum(frustum.m_planes[order[i]], p) > ld(0.0f))
            return false;
    return !(planeDistance(frustum.m_planes[3], p) > ld(0.0f));
}

// PE 0x1007C0A0 [INFERRED] Mat3::setInverse copy (thiscall, retn 4) of an orthonormal 2D affine
// transform, written in place in sequence: m0 = s0, m1 = s3, m3 = s1, m4 = s4, then
// m6 = -(s7*m3 + s6*m0) and m7 = -(s6*m1 + s7*m4) from the elements just written, then m5 = m2 = 0.
// m8 is never written (shipped). With out aliasing in, each read sees the earlier stores (m3 gets
// the new m1). Returning out is [INFERRED]: both images leave `this` in eax, no caller exists.
// Unreachable in 1.50: no code or data reference.
inline Mat3& mat3SetInverse(Mat3& out, const Mat3& in)
{
    out.m[0] = in.m[0];
    out.m[1] = in.m[3];
    out.m[3] = in.m[1];
    out.m[4] = in.m[4];
    out.m[6] = st(-(ld(in.m[7]) * ld(out.m[3]) + ld(in.m[6]) * ld(out.m[0])));
    out.m[7] = st(-(ld(in.m[6]) * ld(out.m[1]) + ld(in.m[7]) * ld(out.m[4])));
    out.m[5] = 0.0f;
    out.m[2] = 0.0f;
    return out;
}

// PE 0x1007C0F0 [INFERRED] Mat3::mult copy (thiscall, lhs and rhs on the stack, retn 8): the 2D
// affine product, elements 0, 3, 6, 1, 4, 7 stored in that order from the operands as they stand
// (6 and 7 add rhs's translation r6, r7), then m2 = m5 = 0 and m8 = 1. Returning out as
// mat3SetInverse. Unreachable in 1.50: no code or data reference.
inline Mat3& mat3Mult(Mat3& out, const Mat3& lhs, const Mat3& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    out.m[0] = st(ld(l[0]) * ld(r[0]) + ld(r[3]) * ld(l[1]));
    out.m[3] = st(ld(l[4]) * ld(r[3]) + ld(l[3]) * ld(r[0]));
    out.m[6] = st((ld(l[7]) * ld(r[3]) + ld(l[6]) * ld(r[0])) + ld(r[6]));
    out.m[1] = st(ld(l[0]) * ld(r[1]) + ld(l[1]) * ld(r[4]));
    out.m[4] = st(ld(l[4]) * ld(r[4]) + ld(l[3]) * ld(r[1]));
    out.m[7] = st((ld(l[7]) * ld(r[4]) + ld(l[6]) * ld(r[1])) + ld(r[7]));
    out.m[2] = 0.0f;
    out.m[5] = 0.0f;
    out.m[8] = 1.0f;
    return out;
}

// PE 0x1007C180 [INFERRED] Mat3::fullMult copy (thiscall, lhs and rhs on the stack, retn 8): the
// full 3x3 product stored column by column (0, 3, 6, 1, 4, 7, 2, 5, 8), each element from the
// operands as they stand at that store. Returning out as mat3SetInverse.
// Unreachable in 1.50: no code or data reference.
inline Mat3& mat3FullMult(Mat3& out, const Mat3& lhs, const Mat3& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    out.m[0] = st((ld(l[0]) * ld(r[0]) + ld(l[1]) * ld(r[3])) + ld(r[6]) * ld(l[2]));
    out.m[3] = st((ld(l[4]) * ld(r[3]) + ld(l[3]) * ld(r[0])) + ld(l[5]) * ld(r[6]));
    out.m[6] = st((ld(l[7]) * ld(r[3]) + ld(l[6]) * ld(r[0])) + ld(l[8]) * ld(r[6]));
    out.m[1] = st((ld(l[1]) * ld(r[4]) + ld(r[7]) * ld(l[2])) + ld(r[1]) * ld(l[0]));
    out.m[4] = st((ld(l[5]) * ld(r[7]) + ld(l[4]) * ld(r[4])) + ld(l[3]) * ld(r[1]));
    out.m[7] = st((ld(l[8]) * ld(r[7]) + ld(l[7]) * ld(r[4])) + ld(l[6]) * ld(r[1]));
    out.m[2] = st((ld(l[0]) * ld(r[2]) + ld(l[1]) * ld(r[5])) + ld(r[8]) * ld(l[2]));
    out.m[5] = st((ld(l[5]) * ld(r[8]) + ld(l[4]) * ld(r[5])) + ld(l[3]) * ld(r[2]));
    out.m[8] = st((ld(l[8]) * ld(r[8]) + ld(l[7]) * ld(r[5])) + ld(l[6]) * ld(r[2]));
    return out;
}

// PE 0x1007C270 [INFERRED] Mat3::rotMult copy (thiscall, lhs and rhs on the stack, retn 8): only
// the upper-left 2x2 product, elements 0, 3, 1, 4 in that order; every other element keeps its
// value. Returning out as mat3SetInverse. Unreachable in 1.50: no code or data reference.
inline Mat3& mat3RotMult(Mat3& out, const Mat3& lhs, const Mat3& rhs)
{
    const float* const l = lhs.m;
    const float* const r = rhs.m;
    out.m[0] = st(ld(l[0]) * ld(r[0]) + ld(r[3]) * ld(l[1]));
    out.m[3] = st(ld(l[4]) * ld(r[3]) + ld(l[3]) * ld(r[0]));
    out.m[1] = st(ld(l[0]) * ld(r[1]) + ld(r[4]) * ld(l[1]));
    out.m[4] = st(ld(l[4]) * ld(r[4]) + ld(l[3]) * ld(r[1]));
    return out;
}

// PE 0x10030D80 [INFERRED] Mat4::Mat4() copy (thiscall on the receiver, answers it): the image's
// Mat4 default constructor builds the identity, all sixteen elements zeroed in index order, then
// m15, m10, m5 and m0 set to 1. Also the element constructor that the image's `eh vector
// constructor iterator` receives for Mat4 arrays (pushed at 0x1010454E, 0x1010469A, 0x101047DE,
// 0x10146E00). The shared Math/Mat4.h is an aggregate (the Linux ELF emits no Mat4 default
// constructor), so RendDX9 callers construct through this copy where the PE calls it.
inline Mat4& constructIdentity(Mat4& m)
{
    for (int i = 0; i < 16; ++i)
        m.m[i] = 0.0f;
    m.m[15] = 1.0f;
    m.m[10] = 1.0f;
    m.m[5] = 1.0f;
    m.m[0] = 1.0f;
    return m;
}

// constructIdentity for an initialiser (a function-local static's guarded construction).
inline Mat4 constructedIdentity()
{
    Mat4 m;
    constructIdentity(m);
    return m;
}

// PE 0x10027810 [INFERRED] Mat4::makeIdentity copy (thiscall on the receiver, answers it): every
// element written once in index order (m1..m3 0, m0 1, m4 0, m5 1, ...). The mesh particles'
// directional stretch (0x10175B3E) is its only caller.
inline Mat4& makeIdentity(Mat4& m)
{
    for (int i = 0; i < 16; ++i)
        m.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    return m;
}

// PE 0x100793F0 [INFERRED] Vec3::operator-(const Vec3&) const copy (thiscall, hidden result then
// rhs, retn 8): receiver - rhs for z, y, x, all three differences formed before the x, y, z
// stores. Callers 0x100A9032 / 0x100AB69D (the lightmap traces' camera-relative position).
inline Vec3 minus(const Vec3& self, const Vec3& rhs)
{
    const float z = st(ld(self.z) - ld(rhs.z));
    const float y = st(ld(self.y) - ld(rhs.y));
    const float x = st(ld(self.x) - ld(rhs.x));
    Vec3 out;
    out.x = x;
    out.y = y;
    out.z = z;
    return out;
}

} // namespace rendmath
} // namespace hfe
} // namespace dice

#endif // RENDDX9_RENDDX9MATH_H
