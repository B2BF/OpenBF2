// Pathfinding trace helpers. Evidence: libaidll.so 1.5.3153 (ia-32), see TraceUtils.h.

#include "TraceUtils.h"

#include "../Main/IAIPathfinding.h"

#include <cmath>

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x36AD90, __tcf_0 0x36ADF0, _GLOBAL__I 0x36AE20).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x36AC90. The circle point is centre + rotated * radius with
//   rotated = (d.x * cos - d.y * sin, 0, d.z * cos + d.z * sin)
// as compiled: the y component stays at its zero initialisation and the x/z terms do not form a
// rotation about Y (shipped defect, kept). The trace runs on map `map` from `from` to that point
// (IAIPathfinding +0x88: arguments 1, -1, 0.0f, 0, null; the polygon out-value is discarded).
// [PLATFORM] retail keeps the products and sums in x87 extended precision.
bool TraceUtils::tracePointOnCircle(const Vec3& direction, const Vec3& centre, const Vec3& from, int map,
                                    float angle, float radius, Vec3& point)
{
    Vec3 target;
    target.x = 0.0f;
    target.y = 0.0f;
    target.z = 0.0f;
    const float sine = std::sin(angle);
    const float cosine = std::cos(angle);
    const float rotatedX = direction.x * cosine - direction.y * sine;
    const float rotatedZ = direction.z * cosine + direction.z * sine;
    target.x = rotatedX * radius + centre.x;
    target.y = radius * target.y + centre.y;
    target.z = rotatedZ * radius + centre.z;
    unsigned int polygon;
    return IAIPathfinding::instance->trace(map, from, target, point, polygon, 1, -1, 0.0f, 0, 0);
}

} // namespace ai
} // namespace hfe
} // namespace dice
