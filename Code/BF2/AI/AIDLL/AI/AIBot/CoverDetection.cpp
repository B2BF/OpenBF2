// The ELF FILE symbol attests CoverDetection.cpp; directory placement is [INFERRED].

#include <cmath>
#include <iostream>
// static init: ELF 0x1A20C0, 0x1A2120, 0x1A2150: iostream Init only.

#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../AIInterface/BFKnowledge/PlanePolygon.h"
#include "../../../../Math/BoundingBox.h"

namespace dice { namespace hfe { namespace ai {

// ELF 0x1A19E0. Shipped defect: output is read but never written.
float hasCoverObjects(Information* information, const Vec3& position, IPlanePolygon* polygon,
                      Vec3& output)
{
    const BoundingBox* const box = information->getPhysical()->getBoundingBox();
    const Vec3& objectPosition = information->getPosition();
    const Vec3 delta = {objectPosition.x - position.x, objectPosition.y - position.y,
                        objectPosition.z - position.z};
    Vec2 direction = {};
    Vec2 normal = {};
    vec3ToVec2XZ(delta, direction);
    getNormal(direction, normal);
    const float squared = normal.x * normal.x + normal.y * normal.y;
    if (!(std::fabs(squared - 1.0f) < 1.1920928955078125e-7f))
    {
        if (!(std::fabs(squared) < 1.4210854715202004e-14f))
        {
            const float inverse = 1.0f / std::sqrt(squared);
            normal.x *= inverse;
            normal.y *= inverse;
        }
        else
        {
            normal.x = 0.0f;
            normal.y = 0.0f;
        }
    }
    const Vec3 maximum = box->getMax();
    const Vec3 maximumDelta = {maximum.x - position.x, maximum.y - position.y, maximum.z - position.z};
    Vec2 maximumXZ = {};
    vec3ToVec2XZ(maximumDelta, maximumXZ);
    Vec2 high = maximumXZ;
    Vec2 low = maximumXZ;
    // [INFERRED] local packaging of the repeated inlined projection.
    const auto projection = [&](const Vec2& point)
    {
        return (point.x * normal.x + point.y * normal.y)
             / std::sqrt(point.x * point.x + point.y * point.y);
    };
    float highest = projection(maximumXZ);
    float lowest = highest;
    const Vec3 minimum = box->getMin();
    const Vec3 minimumDelta = {minimum.x - position.x, minimum.y - position.y, minimum.z - position.z};
    Vec2 minimumXZ = {};
    vec3ToVec2XZ(minimumDelta, minimumXZ);
    Vec2 corner = {maximumXZ.x, minimumXZ.y};
    float value = projection(corner);
    if (value > highest) { highest = value; high = corner; }
    else if (value < lowest) { lowest = value; low = corner; }
    corner.x = minimumXZ.x;
    value = projection(corner);
    if (value > highest) { highest = value; high = corner; }
    else if (value < lowest) { lowest = value; low = corner; }
    corner.y = maximumXZ.y;
    value = projection(corner);
    if (value > highest) { high = corner; }
    else if (value < lowest) { low = corner; }
    Vec2 origin = {};
    Vec2 unused = {};
    vec3ToVec2XZ(position, origin);
    vec3ToVec2XZ(output, unused);
    unused.x -= origin.x;
    unused.y -= origin.y;
    if (polygon->intersects(high, origin))
        return polygon->intersects(low, origin) ? 1.0f : 0.25f;
    return polygon->intersects(low, origin) ? 0.25f : 0.0f;
}

} } }
