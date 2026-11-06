// Double-precision vectors used by navigation mesh generation.
// [INFERRED] Header placement; the ELF FILE symbol attests DoubleVec3.cpp.
#ifndef MATH_DOUBLEVEC3_H
#define MATH_DOUBLEVEC3_H

namespace dice { namespace hfe {

struct DoubleVec3
{
    double x, y, z;
    int makeOrthonormalBasis(DoubleVec3& forward, DoubleVec3& up);
    static const DoubleVec3& getZero();
    static const DoubleVec3& getIdentity();
};

typedef char DoubleVec3IsTheFileRecord[sizeof(DoubleVec3) == 24 ? 1 : -1];

} }
#endif
