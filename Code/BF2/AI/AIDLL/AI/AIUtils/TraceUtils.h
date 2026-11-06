// Pathfinding trace helpers of the AI image.
// Evidence: libaidll.so 1.5.3153 (ia-32), FILE symbol TraceUtils.cpp; the AIUtils directory and
// the class form of TraceUtils (the mangled name fits a class with static members as well as a
// namespace) are [INFERRED].

#ifndef AI_AIDLL_AI_AIUTILS_TRACEUTILS_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIUTILS_TRACEUTILS_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class TraceUtils
{
public:
    // Rotates `direction` by `angle` (see the body for the shipped formula), scales it by `radius`
    // around `centre` and traces the pathfinding map `map` from `from` to that point; `point`
    // receives the trace's end. Returns the trace's answer.
    static bool tracePointOnCircle(const Vec3& direction, const Vec3& centre, const Vec3& from, int map,
                                   float angle, float radius, Vec3& point);
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIUTILS_TRACEUTILS_H
