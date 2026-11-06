// Matrix transforms, affine inversion, rotations, and text serialization.

// static init: ELF 0x49B9F0 (__tcf_0 0x49BA50, _GLOBAL__I 0x49BA80): <iostream>'s Init object only.
#include <iostream>

#include "Mat4.h"

#include <cfloat>
#include <istream>      // the console Mat4 text form
#include <math.h>
#include <ostream>

namespace dice {
namespace hfe {

// ELF 0x499440.
const Mat4& Mat4::getZero()
{
    static const Mat4 zero = {{ 0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 0.0f,  0.0f, 0.0f, 0.0f, 0.0f }};
    return zero;
}

// ELF 0x499300.
const Mat4& Mat4::getIdentity()
{
    static const Mat4 identity = {{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    }};
    return identity;
}

// ELF 0x498D50.
Mat4 Mat4::operator*(const Mat4& rhs) const
{
    Mat4 result;
    for (int row = 0; row != 4; ++row)
    {
        for (int column = 0; column != 4; ++column)
        {
            result.m[column * 4 + row] =
                m[row * 4]     * rhs.m[column] +
                m[row * 4 + 1] * rhs.m[4 + column] +
                m[row * 4 + 2] * rhs.m[8 + column] +
                m[row * 4 + 3] * rhs.m[12 + column];
        }
    }
    return result;
}

// Writes the full row-major product of lhs and rhs. Output must not alias either input.
// ELF 0x4996D0.
void Mat4::fullMult(const Mat4& lhs, const Mat4& rhs)
{
    for (int column = 0; column < 4; ++column)
    {
        for (int row = 0; row < 4; ++row)
        {
            m[row * 4 + column] =
                lhs.m[row * 4]     * rhs.m[column]     +
                lhs.m[row * 4 + 1] * rhs.m[4 + column] +
                lhs.m[row * 4 + 2] * rhs.m[8 + column] +
                lhs.m[row * 4 + 3] * rhs.m[12 + column];
        }
    }
}

// ELF 0x499910.
void Mat4::mult(const Mat4& lhs, const Mat4& rhs)
{
    // Unlike operator*, this is the engine's affine fast path: the final column is restored to
    // (0,0,0,1), and only the right-hand translation is added to the transformed left-hand one.
    m[0]  = lhs.m[0]  * rhs.m[0] + lhs.m[1]  * rhs.m[4] + lhs.m[2]  * rhs.m[8];
    m[4]  = lhs.m[4]  * rhs.m[0] + lhs.m[5]  * rhs.m[4] + lhs.m[6]  * rhs.m[8];
    m[8]  = lhs.m[8]  * rhs.m[0] + lhs.m[9]  * rhs.m[4] + lhs.m[10] * rhs.m[8];
    m[12] = lhs.m[12] * rhs.m[0] + lhs.m[13] * rhs.m[4] + lhs.m[14] * rhs.m[8] + rhs.m[12];

    m[1]  = lhs.m[0]  * rhs.m[1] + lhs.m[1]  * rhs.m[5] + lhs.m[2]  * rhs.m[9];
    m[5]  = lhs.m[4]  * rhs.m[1] + lhs.m[5]  * rhs.m[5] + lhs.m[6]  * rhs.m[9];
    m[9]  = lhs.m[8]  * rhs.m[1] + lhs.m[9]  * rhs.m[5] + lhs.m[10] * rhs.m[9];
    m[13] = lhs.m[12] * rhs.m[1] + lhs.m[13] * rhs.m[5] + lhs.m[14] * rhs.m[9] + rhs.m[13];

    m[2]  = lhs.m[0]  * rhs.m[2] + lhs.m[1]  * rhs.m[6] + lhs.m[2]  * rhs.m[10];
    m[6]  = lhs.m[4]  * rhs.m[2] + lhs.m[5]  * rhs.m[6] + lhs.m[6]  * rhs.m[10];
    m[10] = lhs.m[8]  * rhs.m[2] + lhs.m[9]  * rhs.m[6] + lhs.m[10] * rhs.m[10];
    m[14] = lhs.m[12] * rhs.m[2] + lhs.m[13] * rhs.m[6] + lhs.m[14] * rhs.m[10] + rhs.m[14];

    m[3] = m[7] = m[11] = 0.0f;
    m[15] = 1.0f;
}

// ELF 0x499220.
void Mat4::setInverse(const Mat4& source)
{
    // BF2's ordinary transform inverse assumes an orthonormal 3x3 rotation.  A separate
    // fullInvert helper serves general matrices in the shipped Math translation unit.
    m[0] = source.m[0];
    m[1] = source.m[4];
    m[2] = source.m[8];
    m[3] = 0.0f;
    m[4] = source.m[1];
    m[5] = source.m[5];
    m[6] = source.m[9];
    m[7] = 0.0f;
    m[8] = source.m[2];
    m[9] = source.m[6];
    m[10] = source.m[10];
    m[11] = 0.0f;
    m[12] = -(source.m[0] * source.m[12] +
              source.m[1] * source.m[13] +
              source.m[2] * source.m[14]);
    m[13] = -(source.m[4] * source.m[12] +
              source.m[5] * source.m[13] +
              source.m[6] * source.m[14]);
    m[14] = -(source.m[8] * source.m[12] +
              source.m[9] * source.m[13] +
              source.m[10] * source.m[14]);
    m[15] = 1.0f;
}

// ELF 0x4990D0; bf2 0x83B57B0. Reload factors for each sequential store, including aliases.
Mat4& Mat4::scale(const Vec3& factors)
{
    m[0] = factors.x * m[0];
    m[4] = factors.x * m[4];
    m[8] = factors.x * m[8];
    m[12] = factors.x * m[12];
    m[1] = factors.y * m[1];
    m[5] = factors.y * m[5];
    m[9] = factors.y * m[9];
    m[13] = factors.y * m[13];
    m[2] = factors.z * m[2];
    m[6] = factors.z * m[6];
    m[10] = factors.z * m[10];
    m[14] = factors.z * m[14];
    return *this;
}

// ELF 0x499150; bf2 0x83B5830.
Mat4& Mat4::rotScale(const Vec3& factors)
{
    m[0] = factors.x * m[0];
    m[4] = factors.x * m[4];
    m[8] = factors.x * m[8];
    m[1] = factors.y * m[1];
    m[5] = factors.y * m[5];
    m[9] = factors.y * m[9];
    m[2] = factors.z * m[2];
    m[6] = factors.z * m[6];
    m[10] = factors.z * m[10];
    return *this;
}

// ELF 0x4991B0; bf2 0x83B5890.
Mat4& Mat4::setTranspose(const Mat4& source)
{
    for (int row = 0; row < 4; ++row)
        for (int column = 0; column < 4; ++column)
            m[row * 4 + column] = source.m[column * 4 + row];
    return *this;
}

// ELF 0x499580; bf2 0x83B5C30.
const Mat4& Mat4::getTexScaleBias()
{
    static const Mat4 scalebias = {{
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    }};
    return scalebias;
}

// ELF 0x49AA60; bf2 0x83B6FB0.
int calcProjectionMatrix(Mat4& m, float fov, float aspectScale,
                         float nearDistance, float farDistance)
{
    const float distance = farDistance - nearDistance;
    if (distance < FLT_EPSILON)
        return -20000;

    const float halfAngle = fov * 0.5f;
    const float sine = sinf(halfAngle);
    const float cosine = cosf(halfAngle);
    const float cotangent = cosine / sine;
    const float depth = farDistance / distance;
    const float x = cotangent * aspectScale;
    if (depth == 1.0f)
    {
        m = Mat4{{x, 0.0f, 0.0f, 0.0f,
                  0.0f, cotangent, 0.0f, 0.0f,
                  0.0f, 0.0f, 1.0f, 1.0f,
                  0.0f, 0.0f, -nearDistance, 0.0f}};
    }
    else
    {
        m = Mat4{{x, 0.0f, 0.0f, 0.0f,
                  0.0f, cotangent, 0.0f, 0.0f,
                  0.0f, 0.0f, depth, 1.0f,
                  0.0f, 0.0f, -depth * nearDistance, 0.0f}};
    }
    return 0;
}

// ELF 0x49AC20; bf2 0x83B7160.
Mat4& calcRotationMatrixFromAllVectors(Mat4& m, const Vec3& forward, const Vec3& up,
                                       const Vec3& right, const Vec3& eye)
{
    m.m[0] = right.x;
    m.m[4] = right.y;
    m.m[8] = right.z;
    m.m[12] = -(eye.x * right.x + eye.y * right.y + eye.z * right.z);
    m.m[1] = up.x;
    m.m[5] = up.y;
    m.m[9] = up.z;
    m.m[13] = -(eye.x * up.x + eye.y * up.y + eye.z * up.z);
    m.m[2] = forward.x;
    m.m[6] = forward.y;
    m.m[10] = forward.z;
    const float z = -(eye.x * forward.x + eye.y * forward.y + eye.z * forward.z);
    m.m[3] = m.m[7] = m.m[11] = 0.0f;
    m.m[15] = 1.0f;
    m.m[14] = z;
    return m;
}

namespace {

const float s_degreesToRadians = 0.0174532924f; // in both shipped builds
const float s_radiansToDegrees = 57.2957764f;   // in both shipped builds

struct Rotation3
{
    float m[9];
};

Rotation3 identityRotation()
{
    Rotation3 result = {
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f
    };
    return result;
}

Rotation3 multiply(const Rotation3& lhs, const Rotation3& rhs)
{
    Rotation3 result;
    for (int row = 0; row != 3; ++row)
    {
        for (int column = 0; column != 3; ++column)
        {
            result.m[row * 3 + column] =
                lhs.m[row * 3 + 0] * rhs.m[0 * 3 + column] +
                lhs.m[row * 3 + 1] * rhs.m[1 * 3 + column] +
                lhs.m[row * 3 + 2] * rhs.m[2 * 3 + column];
        }
    }
    return result;
}

bool hasDirection(float first, float second)
{
    return fabsf(first) >= FLT_EPSILON || fabsf(second) >= FLT_EPSILON;
}

float clampUnit(float value)
{
    if (value < -1.0f)
        return -1.0f;
    if (value > 1.0f)
        return 1.0f;
    return value;
}

} // unnamed namespace

// ELF 0x499CA0.
void setRotationZXY(Mat4& matrix, float zDegrees, float xDegrees, float yDegrees)
{
    Rotation3 rotateZ = identityRotation();
    Rotation3 rotateX = identityRotation();
    Rotation3 rotateY = identityRotation();
    unsigned int present = 0;

    if (zDegrees != 0.0f)
    {
        const float radians = zDegrees * s_degreesToRadians;
        const float sine = sinf(radians);
        const float cosine = cosf(radians);
        rotateZ.m[0] = cosine;
        rotateZ.m[1] = -sine;
        rotateZ.m[3] = sine;
        rotateZ.m[4] = cosine;
        present |= 1;
    }

    if (xDegrees != 0.0f)
    {
        const float radians = xDegrees * s_degreesToRadians;
        const float sine = sinf(radians);
        const float cosine = cosf(radians);
        rotateX.m[4] = cosine;
        rotateX.m[5] = -sine;
        rotateX.m[7] = sine;
        rotateX.m[8] = cosine;
        present |= 2;
    }

    if (yDegrees != 0.0f)
    {
        const float radians = yDegrees * s_degreesToRadians;
        const float sine = sinf(radians);
        const float cosine = cosf(radians);
        rotateY.m[0] = cosine;
        rotateY.m[2] = sine;
        rotateY.m[6] = -sine;
        rotateY.m[8] = cosine;
        present |= 4;
    }

    Rotation3 rotation;
    switch (present)
    {
    case 0: rotation = identityRotation(); break;
    case 1: rotation = rotateZ; break;
    case 2: rotation = rotateX; break;
    case 3: rotation = multiply(rotateZ, rotateX); break;
    case 4: rotation = rotateY; break;
    case 5: rotation = multiply(rotateZ, rotateY); break;
    case 6: rotation = multiply(rotateX, rotateY); break;
    default: rotation = multiply(rotateZ, multiply(rotateX, rotateY)); break;
    }

    matrix.m[0] = rotation.m[0];
    matrix.m[1] = rotation.m[1];
    matrix.m[2] = rotation.m[2];
    matrix.m[4] = rotation.m[3];
    matrix.m[5] = rotation.m[4];
    matrix.m[6] = rotation.m[5];
    matrix.m[8] = rotation.m[6];
    matrix.m[9] = rotation.m[7];
    matrix.m[10] = rotation.m[8];
}

// ELF 0x49A380.
void getRotationZXY(const Mat4& matrix, float& zDegrees, float& xDegrees, float& yDegrees)
{
    const float sineX = matrix.m[9];

    if (sineX >= 1.0f)
    {
        zDegrees = hasDirection(matrix.m[2], matrix.m[0])
            ? atan2f(matrix.m[2], matrix.m[0]) * s_radiansToDegrees
            : 0.0f;
        xDegrees = 90.0f;
        yDegrees = 0.0f;
        return;
    }

    if (sineX <= -1.0f)
    {
        zDegrees = hasDirection(matrix.m[2], matrix.m[0])
            ? -atan2f(matrix.m[2], matrix.m[0]) * s_radiansToDegrees
            : 0.0f;
        xDegrees = -90.0f;
        yDegrees = 0.0f;
        return;
    }

    zDegrees = hasDirection(matrix.m[1], matrix.m[5])
        ? atan2f(-matrix.m[1], matrix.m[5]) * s_radiansToDegrees
        : 0.0f;
    xDegrees = asinf(clampUnit(sineX)) * s_radiansToDegrees;
    yDegrees = hasDirection(matrix.m[8], matrix.m[10])
        ? atan2f(-matrix.m[8], matrix.m[10]) * s_radiansToDegrees
        : 0.0f;
}


// Overwrites m with an X-axis rotation in degrees, including identity translation.

// ELF 0x499A90.
void setRotateXDeg(Mat4& m, float degrees)
{
    m = Mat4::getIdentity();

    const float radians = degrees * s_degreesToRadians;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);

    m.m[5] = cosine;
    m.m[6] = sine;
    m.m[9] = -sine;
    m.m[10] = cosine;
}


// Overwrites m with a Y-axis rotation in degrees, including identity translation.

// ELF 0x499B40.
void setRotateYDeg(Mat4& m, float degrees)
{
    m = Mat4::getIdentity();

    const float radians = degrees * s_degreesToRadians;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);

    m.m[0] = cosine;
    m.m[2] = -sine;
    m.m[8] = sine;
    m.m[10] = cosine;
}

// The Z twin of the above, same "identity first, then the four terms" shape.
// ELF 0x499BF0.
void setRotateZDeg(Mat4& m, float degrees)
{
    m = Mat4::getIdentity();

    const float radians = degrees * s_degreesToRadians;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);

    m.m[0] = cosine;
    m.m[1] = -sine;
    m.m[4] = sine;
    m.m[5] = cosine;
}

// lhs * rhs, but only the nine rotation elements are computed and stored - the
// translation row and the fourth column of `this` keep whatever they held. See the header.
// ELF 0x49B0D0.
void Mat4::rotMult(const Mat4& lhs, const Mat4& rhs)
{
    for (int row = 0; row < 3; ++row)
    {
        for (int col = 0; col < 3; ++col)
        {
            m[row * 4 + col] = lhs.m[row * 4 + 0] * rhs.m[0 * 4 + col]
                             + lhs.m[row * 4 + 1] * rhs.m[1 * 4 + col]
                             + lhs.m[row * 4 + 2] * rhs.m[2 * 4 + col];
        }
    }
}

// The change-of-basis rotation about an arbitrary line - see the header for the
// four steps and for the up-hint's degenerate case.
// ELF 0x49A650.
void setRotateAboutLine(Mat4& m, float degrees, const Vec3& axis)
{
    Vec3 up;
    up.x = 0.0f; up.y = 0.0f; up.z = 1.0f;

    Vec3 line = axis;
    if (fabsf(line.x) < 0.1f && fabsf(line.y) < 0.1f)
    {
        // The line is (near enough) vertical - (0,0,1) would be parallel to it.
        up.y = 1.0f;
        up.z = 0.0f;
    }

    Vec3 right;
    right.x = 0.0f; right.y = 0.0f; right.z = 0.0f;
    right.makeOrthonormalBasis(line, up);            // the return code is not consulted

    // The frame, as rows: [right; up; line]. The line becomes the frame's local Z.
    Mat4 basis;
    for (int i = 0; i < 16; ++i) basis.m[i] = 0.0f;
    basis.m[0] = right.x; basis.m[1] = right.y; basis.m[2]  = right.z; basis.m[3]  = 0.0f;
    basis.m[4] = up.x;    basis.m[5] = up.y;    basis.m[6]  = up.z;    basis.m[7]  = 0.0f;
    basis.m[8] = line.x;  basis.m[9] = line.y;  basis.m[10] = line.z;
    basis.m[11] = basis.m[12] = basis.m[13] = basis.m[14] = 0.0f;
    basis.m[15] = 1.0f;

    m.setInverse(basis);

    Mat4 rotation = Mat4::getIdentity();
    setRotateZDeg(rotation, degrees);

    Mat4 scratch = m;
    m.rotMult(scratch, rotation);

    scratch = m;
    m.mult(scratch, basis);
}

// The console text form - see the header block. Per row: '[' float '/' float '/' float '/' float
// ']' - the parse takes each separator as one plain char (any single character passes), the
// print writes exactly those characters with default float formatting.
std::istream& operator>>(std::istream& in, Mat4& m)
{
    for (int row = 0; row < 4; ++row)
    {
        char sep = 0;
        float f = 0.0f;
        in >> sep;                          // '['
        in >> f; in >> sep; m.m[row * 4 + 0] = f;
        in >> f; in >> sep; m.m[row * 4 + 1] = f;
        in >> f; in >> sep; m.m[row * 4 + 2] = f;
        in >> f;            m.m[row * 4 + 3] = f;
        in >> sep;                          // ']'
    }
    return in;
}

std::ostream& operator<<(std::ostream& out, const Mat4& m)
{
    for (int row = 0; row < 4; ++row)
    {
        out << '[' << m.m[row * 4 + 0] << '/' << m.m[row * 4 + 1] << '/'
            << m.m[row * 4 + 2] << '/' << m.m[row * 4 + 3] << ']';
    }
    return out;
}


// ELF 0x49AD00.
void calcLookAtMatrix(Mat4& m, const Vec3& eye, const Vec3& target, const Vec3& up)
{
    float* f = m.m;
    for (int i = 0; i < 16; ++i)
        f[i] = 0.0f;
    f[0] = 1.0f; f[5] = 1.0f; f[10] = 1.0f; f[15] = 1.0f;

    const float fy = target.y - eye.y;
    const float fz = target.z - eye.z;
    const float fx = target.x - eye.x;
    f[8] = fx; f[9] = fy; f[10] = fz;
    const float len = sqrtf(fz * fz + fy * fy + fx * fx);
    if (len >= FLT_EPSILON)
    {
        const float inv = 1.0f / len;
        f[8] = f[8] * inv;
        f[9] = f[9] * inv;
        f[10] = inv * f[10];
        const float d = f[9] * up.y + f[8] * up.x + f[10] * up.z;
        if (fabsf(d - 1.0f) >= FLT_EPSILON && fabsf(d + 1.0f) >= FLT_EPSILON)
        {
            // right = up x forward
            f[0] = up.y * f[10] - up.z * f[9];
            f[1] = up.z * f[8] - f[10] * up.x;
            f[2] = f[9] * up.x - up.y * f[8];
            float lenSq = f[1] * f[1] + f[0] * f[0] + f[2] * f[2];
            if (fabsf(lenSq - 1.0f) >= FLT_EPSILON)
            {
                if (fabsf(lenSq) >= FLT_EPSILON)
                {
                    const float k = 1.0f / sqrtf(lenSq);
                    f[0] = f[0] * k;
                    f[1] = f[1] * k;
                    f[2] = k * f[2];
                }
                else
                {
                    f[0] = 0.0f; f[1] = 0.0f; f[2] = 0.0f;
                }
            }
            // up' = forward x right
            f[4] = f[9] * f[2] - f[1] * f[10];
            f[5] = f[0] * f[10] - f[2] * f[8];
            f[6] = f[8] * f[1] - f[9] * f[0];
            lenSq = f[4] * f[4] + f[5] * f[5] + f[6] * f[6];
            if (fabsf(lenSq - 1.0f) >= FLT_EPSILON)
            {
                if (fabsf(lenSq) >= FLT_EPSILON)
                {
                    const float k = 1.0f / sqrtf(lenSq);
                    f[4] = f[4] * k;
                    f[5] = f[5] * k;
                    f[6] = k * f[6];
                }
                else
                {
                    f[4] = 0.0f; f[5] = 0.0f; f[6] = 0.0f;
                }
            }
            f[12] = eye.x; f[13] = eye.y; f[14] = eye.z;
        }
    }
    else
    {
        for (int i = 0; i < 16; ++i)
            f[i] = 0.0f;
        f[0] = 1.0f; f[5] = 1.0f; f[10] = 1.0f; f[15] = 1.0f;
    }
}

} // namespace hfe
} // namespace dice
