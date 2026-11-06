// Eight-corner bounds. [INFERRED] Source directory; the ELF FILE symbol attests BoundingBox.cpp.
#include "../../Math/BoundingBox.h"
#include "../../Math/LocalBoundingBox.h"
#include "../../Math/Frustum.h"

#include <cmath>
// static init: ELF 0x493F20, 0x493F80, 0x493FB0 (std::__ioinit).
#include <iostream>

namespace dice {
namespace hfe {

// ELF 0x492BF0; engine ELF 0x83AF780.
void BoundingBox::initWithLocalBoundingBox(const LocalBoundingBox& box)
{
    for (int i = 0; i < 8; ++i)
    {
        const Vec3 point = {(i & 4) ? box.m_max.x : box.m_min.x,
                            (i & 2) ? box.m_max.y : box.m_min.y,
                            (i & 1) ? box.m_max.z : box.m_min.z};
        m_corners[i] = point;
    }
}

// ELF 0x492CF0; engine ELF 0x83AF840.
void BoundingBox::transform(const Mat4& matrix)
{
    for (int i = 0; i < 8; ++i)
    {
        const float x = m_corners[i].x, y = m_corners[i].y, z = m_corners[i].z;
        const float tx = matrix.m[0] * x + matrix.m[4] * y + matrix.m[8] * z + matrix.m[12];
        const float ty = matrix.m[1] * x + matrix.m[5] * y + matrix.m[9] * z + matrix.m[13];
        const float tz = matrix.m[2] * x + matrix.m[6] * y + matrix.m[10] * z + matrix.m[14];
        m_corners[i].x = tx;
        m_corners[i].y = ty;
        m_corners[i].z = tz;
    }
}

// ELF 0x492D80; engine ELF 0x83AF8D0.
bool BoundingBox::outside(const Frustum& frustum) const
{
    return !inside(frustum);
}

// ELF 0x492DC0; engine ELF 0x83AF900.
bool BoundingBox::inside(const Frustum& frustum) const
{
    for (int i = 0; i < 6; ++i)
    {
        const Plane3& plane = frustum.m_planes[i];
        int outsideCount = 0;
        for (int j = 0; j < 8; ++j)
        {
            const Vec3& point = m_corners[j];
            if (plane.normal.x * point.x + plane.normal.y * point.y
              + plane.normal.z * point.z + plane.d > 0.0f)
                ++outsideCount;
        }
        if (outsideCount == 8)
            return false;
    }
    return true;
}

// ELF 0x492E70; engine ELF 0x83AF990.
Vec3 BoundingBox::getMax() const
{
    Vec3 result = m_corners[0];
    for (int i = 1; i < 8; ++i)
    {
        if (m_corners[i].x > result.x) result.x = m_corners[i].x;
        if (m_corners[i].y > result.y) result.y = m_corners[i].y;
        if (m_corners[i].z > result.z) result.z = m_corners[i].z;
    }
    return result;
}

// ELF 0x492EC0; engine ELF 0x83AF9E0.
Vec3 BoundingBox::getMin() const
{
    Vec3 result = m_corners[0];
    for (int i = 1; i < 8; ++i)
    {
        if (result.x > m_corners[i].x) result.x = m_corners[i].x;
        if (result.y > m_corners[i].y) result.y = m_corners[i].y;
        if (result.z > m_corners[i].z) result.z = m_corners[i].z;
    }
    return result;
}

// ELF 0x492F20; engine ELF 0x83AFA40.
Vec3 BoundingBox::getCentre() const
{
    return {(m_corners[7].x + m_corners[0].x) * 0.5f,
            (m_corners[7].y + m_corners[0].y) * 0.5f,
            (m_corners[7].z + m_corners[0].z) * 0.5f};
}

// ELF 0x492F80; engine ELF 0x83AFA90.
bool BoundingBox::intersect(const BoundingBox& other) const
{
    Vec3 a[3], b[3];
    const int cornerIndices[3] = {4, 2, 1};
    for (int i = 0; i < 3; ++i)
    {
        const Vec3& corner = m_corners[cornerIndices[i]];
        a[i] = {(corner.x - m_corners[0].x) * 0.5f,
                (corner.y - m_corners[0].y) * 0.5f,
                (corner.z - m_corners[0].z) * 0.5f};
    }
    float ah[3], bh[3];
    for (int i = 0; i < 3; ++i)
        ah[i] = std::sqrt(a[i].x * a[i].x + a[i].y * a[i].y + a[i].z * a[i].z);
    for (int i = 0; i < 3; ++i)
    {
        const float inverse = 1.0f / ah[i];
        a[i].x *= inverse; a[i].y *= inverse; a[i].z *= inverse;
    }
    for (int i = 0; i < 3; ++i)
    {
        const Vec3& corner = other.m_corners[cornerIndices[i]];
        b[i] = {(corner.x - other.m_corners[0].x) * 0.5f,
                (corner.y - other.m_corners[0].y) * 0.5f,
                (corner.z - other.m_corners[0].z) * 0.5f};
    }
    for (int i = 0; i < 3; ++i)
        bh[i] = std::sqrt(b[i].x * b[i].x + b[i].y * b[i].y + b[i].z * b[i].z);
    for (int i = 0; i < 3; ++i)
    {
        const float inverse = 1.0f / bh[i];
        b[i].x *= inverse; b[i].y *= inverse; b[i].z *= inverse;
    }
    const Vec3 otherCentre = other.getCentre();
    const Vec3 centre = getCentre();
    const Vec3 delta = {otherCentre.x - centre.x, otherCentre.y - centre.y,
                        otherCentre.z - centre.z};
    float r[3][3], absolute[3][3], t[3];
    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            r[i][j] = a[i].x * b[j].x + a[i].y * b[j].y + a[i].z * b[j].z;
            absolute[i][j] = std::fabs(r[i][j]);
        }
        t[i] = a[i].x * delta.x + a[i].y * delta.y + a[i].z * delta.z;
        if (std::fabs(t[i]) > bh[0] * absolute[i][0] + bh[1] * absolute[i][1]
                             + bh[2] * absolute[i][2] + ah[i])
            return false;
    }
    for (int j = 0; j < 3; ++j)
    {
        if (std::fabs(b[j].x * delta.x + b[j].y * delta.y + b[j].z * delta.z)
            > ah[0] * absolute[0][j] + ah[1] * absolute[1][j]
            + ah[2] * absolute[2][j] + bh[j])
            return false;
    }
    for (int i = 0; i < 3; ++i)
    {
        const int next = (i + 1) % 3, last = (i + 2) % 3;
        const int firstA = i == 0 ? 1 : 0;
        const int secondA = i == 2 ? 1 : 2;
        for (int j = 0; j < 3; ++j)
        {
            const int firstB = j == 0 ? 1 : 0;
            const int secondB = j == 2 ? 1 : 2;
            const float distance = std::fabs(r[next][j] * t[last] - t[next] * r[last][j]);
            const float radiusA = ah[firstA] * absolute[secondA][j] + ah[secondA] * absolute[firstA][j];
            const float radiusB = bh[firstB] * absolute[i][secondB] + bh[secondB] * absolute[i][firstB];
            // Shipped ia-32 comparisons accept unordered values, including on the final axis.
            if (distance > radiusA + radiusB)
                return false;
        }
    }
    return true;
}

} // namespace hfe
} // namespace dice
