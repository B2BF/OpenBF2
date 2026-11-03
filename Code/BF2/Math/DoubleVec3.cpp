// ELF FILE symbol DoubleVec3.cpp. [INFERRED] Math directory placement.
#include "DoubleVec3.h"
#include <cmath>
// ELF static init 0x2F22E0, __tcf_0 0x2F2340, _GLOBAL__I 0x2F2370: iostream initialization.
#include <iostream>

namespace dice { namespace hfe {
namespace {

// [INFERRED] Helper names for the inlined operations in ELF 0x2F1D80.
bool normalizeDoubleBasis(DoubleVec3& vector)
{
    const double squared = vector.x * vector.x + vector.y * vector.y + vector.z * vector.z;
    const double epsilon = 1.1920928955078125e-7f;
    if (std::fabs(squared - 1.0) < epsilon)
        return true;
    if (std::fabs(squared) < epsilon)
        return false;
    const double inverse = 1.0 / std::sqrt(squared);
    vector.x *= inverse;
    vector.y *= inverse;
    vector.z *= inverse;
    return true;
}

void zeroDoubleBasis(DoubleVec3& vector)
{
    vector.x = 0;
    vector.y = 0;
    vector.z = 0;
}

void crossDoubleBasis(DoubleVec3& output, const DoubleVec3& a, const DoubleVec3& b)
{
    output.x = a.y * b.z - a.z * b.y;
    output.y = b.x * a.z - b.z * a.x;
    output.z = b.y * a.x - b.x * a.y;
}

void snapDoubleBasis(DoubleVec3& vector)
{
    const double epsilon = 1.1920928955078125e-7f;
    if (std::fabs(vector.x) < epsilon) vector.x = 0;
    if (std::fabs(vector.y) < epsilon) vector.y = 0;
    if (std::fabs(vector.z) < epsilon) vector.z = 0;
}

} // namespace

// ELF 0x2F1D80. The up-vector reconstruction alone continues after a degenerate result.
int DoubleVec3::makeOrthonormalBasis(DoubleVec3& forward, DoubleVec3& up)
{
    if (!normalizeDoubleBasis(forward)) { zeroDoubleBasis(forward); return -20003; }
    if (!normalizeDoubleBasis(up)) { zeroDoubleBasis(up); return -20003; }
    crossDoubleBasis(*this, forward, up);
    if (!normalizeDoubleBasis(*this)) { zeroDoubleBasis(*this); return -20003; }
    crossDoubleBasis(up, *this, forward);
    if (!normalizeDoubleBasis(up)) zeroDoubleBasis(up);
    crossDoubleBasis(forward, up, *this);
    if (!normalizeDoubleBasis(forward)) { zeroDoubleBasis(forward); return -20003; }
    snapDoubleBasis(*this);
    snapDoubleBasis(forward);
    snapDoubleBasis(up);
    return 0;
}

// ELF 0x2F2260.
const DoubleVec3& DoubleVec3::getZero()
{
    static DoubleVec3 DoubleVec3zero = {0, 0, 0};
    return DoubleVec3zero;
}

// ELF 0x2F22A0.
const DoubleVec3& DoubleVec3::getIdentity()
{
    static DoubleVec3 DoubleVec3identity = {1, 1, 1};
    return DoubleVec3identity;
}

} }
