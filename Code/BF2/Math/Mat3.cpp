// FILE symbol: Mat3.cpp; Math directory placement is [INFERRED].
#include "Mat3.h"

// static init: ELF 0x498CA0 (__tcf_0 0x498D00, _GLOBAL__I 0x498D30): iostream Init only.
#include <iostream>

namespace dice {
namespace hfe {

// ELF 0x498870; bf2 0x83B4FE0. Both images retain the intermediate rotation values.
Mat3& Mat3::setInverse(const Mat3& source)
{
    const float r00 = source.m[0];
    m[0] = r00;
    const float r10 = source.m[3];
    m[1] = r10;
    const float r01 = source.m[1];
    m[3] = r01;
    const float r11 = source.m[4];
    m[4] = r11;
    m[6] = -(r00 * source.m[6] + r01 * source.m[7]);
    const float ty = -(r10 * source.m[6] + r11 * source.m[7]);
    m[5] = m[2] = 0.0f;
    m[7] = ty;
    // shipped defect: m[8] is never initialized here.
    return *this;
}

// ELF 0x4988E0; bf2 0x83B5040.
Mat3& Mat3::mult(const Mat3& lhs, const Mat3& rhs)
{
    m[0] = rhs.m[0] * lhs.m[0] + rhs.m[3] * lhs.m[1];
    m[3] = rhs.m[0] * lhs.m[3] + rhs.m[3] * lhs.m[4];
    m[6] = rhs.m[0] * lhs.m[6] + rhs.m[3] * lhs.m[7] + rhs.m[6];
    m[1] = rhs.m[1] * lhs.m[0] + rhs.m[4] * lhs.m[1];
    m[4] = rhs.m[1] * lhs.m[3] + rhs.m[4] * lhs.m[4];
    m[7] = rhs.m[1] * lhs.m[6] + rhs.m[4] * lhs.m[7] + rhs.m[7];
    m[2] = m[5] = 0.0f;
    m[8] = 1.0f;
    return *this;
}

// ELF 0x498990; bf2 0x83B50E0.
Mat3& Mat3::fullMult(const Mat3& lhs, const Mat3& rhs)
{
    for (int column = 0; column < 3; ++column)
        for (int row = 0; row < 3; ++row)
            m[row * 3 + column] = rhs.m[column] * lhs.m[row * 3]
                               + rhs.m[3 + column] * lhs.m[row * 3 + 1]
                               + rhs.m[6 + column] * lhs.m[row * 3 + 2];
    return *this;
}

// ELF 0x498A90; bf2 0x83B51E0.
Mat3& Mat3::rotMult(const Mat3& lhs, const Mat3& rhs)
{
    m[0] = rhs.m[0] * lhs.m[0] + rhs.m[3] * lhs.m[1];
    m[3] = rhs.m[0] * lhs.m[3] + rhs.m[3] * lhs.m[4];
    m[1] = rhs.m[1] * lhs.m[0] + rhs.m[4] * lhs.m[1];
    m[4] = rhs.m[1] * lhs.m[3] + rhs.m[4] * lhs.m[4];
    return *this;
}

// ELF 0x498AF0; bf2 0x83B5240. A full product returned by value.
Mat3 Mat3::operator*(const Mat3& rhs) const
{
    Mat3 result = {};
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            result.m[row * 3 + column] = rhs.m[column] * m[row * 3]
                                      + rhs.m[3 + column] * m[row * 3 + 1]
                                      + rhs.m[6 + column] * m[row * 3 + 2];
    return result;
}

} // namespace hfe
} // namespace dice
