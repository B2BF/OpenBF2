// Free vector helpers of the AI image. See AIVectorMath.h for the placement note.

#include "AIVectorMath.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x3679F0, __tcf_0 0x367A50, _GLOBAL__I 0x367A80).
#include <iostream>

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x367500.
float distancePointLineSqr(const Vec3& point, const Vec3& linePoint, const Vec3& direction)
{
    const float dx = point.x - linePoint.x;
    const float dy = point.y - linePoint.y;
    const float dz = point.z - linePoint.z;
    const float cx = direction.z * dy - direction.y * dz;
    const float cy = dz * direction.x - direction.z * dx;
    const float cz = dx * direction.y - dy * direction.x;
    return (cx * cx + cy * cy + cz * cz)
         / (direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
}

// objFireLineDist - libaidll.so (ia-32) 0x367590, 687 B (objdump listing and Ghidra, 2026-11-02).
// The segment direction is normalised by multiplying with 1/length; the projection parameter
// is the dot product of that direction with point - from. Below zero the start is nearest,
// beyond the length the end; an unordered parameter takes the projection branch, as compiled.
float objFireLineDist(const Vec3& point, const Vec3& from, const Vec3& to)
{
    float result = 0.0f;
    const float sx = to.x - from.x;
    const float sy = to.y - from.y;
    const float sz = to.z - from.z;
    const float length = std::sqrt(sx * sx + sy * sy + sz * sz);
    if (length < 0.001f)
        return result;

    const float inv = 1.0f / length;
    const float dirX = sx * inv;
    const float dirY = sy * inv;
    const float dirZ = sz * inv;
    float dx = point.x - from.x;
    float dy = point.y - from.y;
    float dz = point.z - from.z;
    const float t = dirX * dx + dirY * dy + dirZ * dz;
    if (!(t < 0.0f))
    {
        if (t > length)
        {
            dx = point.x - to.x;
            dy = point.y - to.y;
            dz = point.z - to.z;
        }
        else
        {
            dx -= dirX * t;
            dy -= dirY * t;
            dz -= dirZ * t;
        }
    }
    result = std::sqrt(dx * dx + dy * dy + dz * dz);
    return result;
}

// ELF 0x367840. Row 2 is saved, row 1 set to `up`; row 0 takes the third axis of (row 1, row 2)
// (which rewrites rows 1 and 2), row 2 is put back, and row 1 takes the third axis of
// (row 2, row 0).
void makeTransOrthonormalUp(Mat4& transformation, const Vec3& up)
{
    Vec3& right = *reinterpret_cast<Vec3*>(&transformation.m[0]);
    Vec3& upRow = *reinterpret_cast<Vec3*>(&transformation.m[4]);
    Vec3& forward = *reinterpret_cast<Vec3*>(&transformation.m[8]);
    const Vec3 savedForward = forward;
    upRow = up;
    right.makeOrthonormalBasis(upRow, forward);
    forward = savedForward;
    upRow.makeOrthonormalBasis(forward, right);
}

// ELF 0x3678D0. A zeroed matrix (the shipped Mat4 construction) set to identity; row 2 becomes
// `direction`; then row 1 takes the third axis of (row 2, row 0) and row 0 that of (row 1, row 2).
Mat4 getDofTransformation(const Vec3& direction)
{
    Mat4 transformation;
    for (int i = 0; i < 16; ++i)
        transformation.m[i] = 0.0f;
    transformation = Mat4::getIdentity();
    Vec3& right = *reinterpret_cast<Vec3*>(&transformation.m[0]);
    Vec3& up = *reinterpret_cast<Vec3*>(&transformation.m[4]);
    Vec3& forward = *reinterpret_cast<Vec3*>(&transformation.m[8]);
    forward = direction;
    up.makeOrthonormalBasis(forward, right);
    right.makeOrthonormalBasis(up, forward);
    return transformation;
}

} // namespace ai
} // namespace hfe
} // namespace dice
