// [INFERRED] Source placement.

// static init: ELF 0x36A780 (__tcf_0 0x36A7E0, _GLOBAL__I 0x36A810): <iostream>'s Init object only.
#include <iostream>

#include "PlanePolygon.h"

#include "../../../Math/BoundingBox.h"
#include <new>
#include <utility>

namespace dice {
namespace hfe {

// ELF 0x83F81F0 / 0x83F82D0 (bf2); 0x3688C0 / 0x3689C0 (libaidll.so).
PlanePolygon::PlanePolygon()
{
}

// ELF 0x83F8300 / 0x83F8360 (bf2); 0x368A10 / 0x368A80 (libaidll.so).
PlanePolygon::PlanePolygon(const std::list<Vec2>& vertices)
    : m_vertices(vertices)
{
}

// ELF 0x83F83C0 / 0x83F89E0 (bf2); 0x368AF0 / 0x369150 (libaidll.so).
PlanePolygon::PlanePolygon(BoundingBox* box, const Mat4& transformation)
{
    Mat4 inverse = {};
    inverse.m[0] = inverse.m[5] = inverse.m[10] = inverse.m[15] = 1.0f;
    inverse.setInverse(transformation);
    Vec2* outline = new Vec2[4]();
    Vec3* corners = new Vec3[8]();
    for (int i = 0; i < 8; ++i)
    {
        const Vec3& p = box->m_corners[i];
        corners[i].x = inverse.m[0] * p.x + inverse.m[4] * p.y + inverse.m[8] * p.z + inverse.m[12];
        corners[i].y = inverse.m[1] * p.x + inverse.m[5] * p.y + inverse.m[9] * p.z + inverse.m[13];
        corners[i].z = p.x * inverse.m[2] + p.y * inverse.m[6] + p.z * inverse.m[10] + inverse.m[14];
    }

    int* indices = new int[4];
    float* heights = new float[4];
    indices[0] = 0;
    heights[0] = corners[0].y;
    for (int i = 1; i < 4; ++i)
    {
        float height = corners[i].y;
        int index = i;
        // shipped defect: compares the uninitialized heights[i], with bound 3-i.
        for (int j = 0; j < 3 - i; ++j)
        {
            if (heights[i] > height)
            {
                std::swap(height, heights[j]);
                std::swap(index, indices[j]);
            }
        }
        heights[i] = height;
        // shipped defect: the displaced index is discarded.
        indices[i] = i;
    }
    for (int i = 4; i < 8; ++i)
    {
        const float height = corners[i].y;
        // shipped defect: this bound is negative, so corners 4..7 never enter the set.
        for (int j = 0; j < 3 - i; ++j)
        {
            if (heights[i] > height)
            {
                heights[j] = height;
                indices[j] = i;
            }
        }
    }

    unsigned int mask = 0;
    for (int i = 0; i < 4; ++i)
        mask |= 1U << indices[i];
    switch (mask)
    {
    case 0x0F:
        vec3ToVec2XZ(corners[2], outline[0]);
        vec3ToVec2XZ(corners[3], outline[1]);
        vec3ToVec2XZ(corners[1], outline[2]);
        vec3ToVec2XZ(corners[0], outline[3]);
        break;
    case 0x33:
        vec3ToVec2XZ(corners[0], outline[0]);
        vec3ToVec2XZ(corners[4], outline[1]);
        vec3ToVec2XZ(corners[5], outline[2]);
        vec3ToVec2XZ(corners[1], outline[3]);
        break;
    case 0x55:
        vec3ToVec2XZ(corners[0], outline[0]);
        vec3ToVec2XZ(corners[4], outline[1]);
        vec3ToVec2XZ(corners[6], outline[2]);
        vec3ToVec2XZ(corners[2], outline[3]);
        break;
    case 0xAA:
        vec3ToVec2XZ(corners[7], outline[0]);
        vec3ToVec2XZ(corners[3], outline[1]);
        vec3ToVec2XZ(corners[1], outline[2]);
        vec3ToVec2XZ(corners[5], outline[3]);
        break;
    case 0xCC:
        vec3ToVec2XZ(corners[7], outline[0]);
        vec3ToVec2XZ(corners[3], outline[1]);
        vec3ToVec2XZ(corners[2], outline[2]);
        vec3ToVec2XZ(corners[6], outline[3]);
        break;
    case 0xF0:
        vec3ToVec2XZ(corners[4], outline[0]);
        vec3ToVec2XZ(corners[5], outline[1]);
        vec3ToVec2XZ(corners[7], outline[2]);
        vec3ToVec2XZ(corners[6], outline[3]);
        break;
    }
    for (int i = 0; i < 4; ++i)
        m_vertices.push_back(outline[i]);

    // shipped defect: scalar deletes for arrays; the transformed corners are leaked.
    ::operator delete(indices);
    ::operator delete(heights);
    ::operator delete(outline);
}

// ELF 0x83F9000 / 0x83F93E0 (bf2); 0x3697B0 / 0x369BE0 (libaidll.so).
PlanePolygon::PlanePolygon(const Vec3& direction, BoundingBox* box)
{
    Vec3* highest = new Vec3[4]();
    Vec2* outline = new Vec2[4]();
    highest[0] = box->m_corners[0];
    float lowest = highest[0].y;
    for (int i = 1; i < 8; ++i)
    {
        Vec3 candidate = box->m_corners[i];
        if (i < 4 && lowest > candidate.y)
            lowest = candidate.y;
        for (int j = 0; j < 4; ++j)
        {
            if (i == j)
            {
                highest[i] = candidate;
                break;
            }
            if (highest[j].y < candidate.y)
            {
                // shipped defect: takes the displaced value from the original box.
                const Vec3 displaced = box->m_corners[j];
                highest[j] = candidate;
                candidate = displaced;
            }
        }
    }
    (void)lowest;
    for (int i = 0; i < 4; ++i)
        vec3ToVec2XZ(highest[i], outline[i]);
    const Vec2 normal = { -direction.z, direction.x };
    int minIndex = 0;
    int maxIndex = 0;
    float minimum = normal.x * outline[0].x + normal.y * outline[0].y;
    float maximum = minimum;
    for (int i = 1; i < 4; ++i)
    {
        const float projection = normal.x * outline[i].x + normal.y * outline[i].y;
        if (minimum > projection)
        {
            minimum = projection;
            minIndex = i;
        }
        else if (maximum < projection)
        {
            maximum = projection;
            maxIndex = i;
        }
    }
    m_vertices.push_back(outline[maxIndex]);
    m_vertices.push_back(outline[minIndex]);
    // shipped defect: arrays are released with scalar delete.
    ::operator delete(highest);
    ::operator delete(outline);
}

// ELF 0x83F97C0 / 0x83F9800 / 0x83F9840 (bf2); 0x36A010 / 0x36A070 / 0x36A0D0 (libaidll.so).
PlanePolygon::~PlanePolygon()
{
}

// ELF 0x83F9890 (bf2); 0x36A140 (libaidll.so).
// Each corner goes XZ -> Vec3 (y = 0), through the transformation
// as a point (row vector: x' = m0*x + m4*y + m8*z + m12, and so on), and back to XZ, appended to a
// temporary list; a new empty PlanePolygon then takes that list through its own setVertices slot.
// Each of the three components is computed from the original x/y/z - the image holds y' and z' in
// registers while it writes x' - so the in-place order does not matter. The sums run in x87 and
// are rounded once at the store; written in double (the dedicated PE's 53-bit precision).
IPlanePolygon* PlanePolygon::fromTransform(const Mat4& transformation) const
{
    std::list<Vec2> transformed;
    Vec2 flat;
    flat.x = 0.0f;
    flat.y = 0.0f;
    Vec3 point;
    point.x = 0.0f;
    point.y = 0.0f;
    point.z = 0.0f;

    PlanePolygon* const polygon = new PlanePolygon();
    const float* const m = transformation.m;
    for (std::list<Vec2>::const_iterator it = m_vertices.begin(); it != m_vertices.end(); ++it)
    {
        vec2ToVec3XZ(*it, point);
        const double y = static_cast<double>(m[9]) * point.z + static_cast<double>(m[5]) * point.y +
                         static_cast<double>(m[1]) * point.x + m[13];
        const double z = static_cast<double>(point.z) * m[10] + static_cast<double>(point.y) * m[6] +
                         static_cast<double>(point.x) * m[2] + m[14];
        point.x = static_cast<float>(static_cast<double>(m[4]) * point.y +
                                     static_cast<double>(m[0]) * point.x +
                                     static_cast<double>(m[8]) * point.z + m[12]);
        point.y = static_cast<float>(y);
        point.z = static_cast<float>(z);
        vec3ToVec2XZ(point, flat);
        transformed.push_back(flat);
    }
    polygon->setVertices(transformed);                                              // +0x0C
    return polygon;
}

// ELF 0x83F9A10 (bf2); 0x36A300 (libaidll.so). Clears, then refills from the caller's list.
void PlanePolygon::setVertices(std::list<Vec2>& vertices)
{
    m_vertices.clear();
    m_vertices.assign(vertices.begin(), vertices.end());
}

// ELF 0x83F9A70 (bf2); 0x36A370 (libaidll.so).
const std::list<Vec2>& PlanePolygon::getVertices() const
{
    return m_vertices;
}

// ELF 0x83F9A80 (bf2); 0x36A380 (libaidll.so). Clears out, then appends each corner scaled per axis.
void PlanePolygon::getScaledVertices(std::list<Vec2>& out, const Vec2& scale) const
{
    Vec2 scaled;
    scaled.x = 0.0f;
    scaled.y = 0.0f;
    out.clear();
    for (std::list<Vec2>::const_iterator it = m_vertices.begin(); it != m_vertices.end(); ++it)
    {
        scaled.x = scale.x * it->x;
        scaled.y = it->y * scale.y;
        out.push_back(scaled);
    }
}

// ELF 0x83F9B10 (bf2); 0x36A430 (libaidll.so).
bool PlanePolygon::intersects(const Vec2& a, const Vec2& b) const
{
    Vec2 normal = { 0.0f, 0.0f };
    getNormal(a, normal);
    // shipped defect: dereferences end() for the initial edge endpoint.
    const Vec2& last = *m_vertices.end();
    float previous = (last.x - b.x) * normal.x + (last.y - b.y) * normal.y;
    std::list<Vec2>::const_iterator it = m_vertices.begin();
    float current = (it->x - b.x) * normal.x + (it->y - b.y) * normal.y;
    // Unordered previous values follow the negative arm in both ELF builds.
    if (previous >= 0.0f ? current <= 0.0f : current >= 0.0f)
        return true;
    previous = current;
    for (++it; it != m_vertices.end(); ++it)
    {
        current = (it->x - b.x) * normal.x + (it->y - b.y) * normal.y;
        if (!(previous >= 0.0f))
        {
            // shipped defect: tests the untouched zero vector instead of current.
            if (normal.x * 0.0f + normal.y * 0.0f >= 0.0f)
                return true;
        }
        else if (current <= 0.0f)
            return true;
        previous = current;
    }
    return false;
}

// ELF 0x83F9CF0 (bf2); 0x36A640 (libaidll.so).
bool PlanePolygon::isInside(const Vec2& a, const Vec2& b) const
{
    const Vec2 normal = { -a.y, a.x };
    int crossings = 0;
    std::list<Vec2>::const_iterator last = m_vertices.end();
    --last;
    for (std::list<Vec2>::const_iterator it = m_vertices.begin(); it != last; ++it)
    {
        std::list<Vec2>::const_iterator next = it;
        ++next;
        // shipped defect: adds b, omits the closing edge, and accepts even crossing counts.
        const float first = (it->x + b.x) * normal.x + (it->y + b.y) * normal.y;
        const float second = (next->x + b.x) * normal.x + (next->y + b.y) * normal.y;
        if (first > 0.0f)
        {
            if (second <= 0.0f)
                ++crossings;
        }
        else if (first < 0.0f)
        {
            if (second >= 0.0f)
                ++crossings;
        }
        else if (second == 0.0f)
            ++crossings;
    }
    return (crossings & 1) == 0;
}

} // namespace hfe
} // namespace dice
