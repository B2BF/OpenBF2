// [CHOSEN, not recovered] header-only helpers for the vehicle controls (CarControl, TankControl).
// libaidll.so has no symbol for any of them: each is the inline expansion the actionStatusDecision
// bodies spell out in place (ELF 0x2AB970 / 0x2D1390), written once here. No build-list entry.

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_VEHICLECONTROLINLINE_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_VEHICLECONTROLINLINE_H

#include "../Main/InformationManager.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {
namespace vehiclecontrol {

inline Information* lookupInformation(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

// The engine's inline Vec2 normalise: kept within FLT_EPSILON of unit length, zeroed below
// FLT_EPSILON^2, else scaled by 1/sqrt (0x2ABA6F).
inline void normalizeVec2(Vec2& v)
{
    const float lengthSqr = v.x * v.x + v.y * v.y;
    if (std::fabs(lengthSqr - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lengthSqr) < 1.4210855e-14f)
    {
        v.x = 0.0f;
        v.y = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSqr);
    v.x = v.x * inverse;
    v.y = inverse * v.y;
}

// The Vec3 variant: zeroed below FLT_EPSILON (0x2ABEDC).
inline void normalizeVec3(Vec3& v)
{
    const float lengthSqr = v.x * v.x + v.y * v.y + v.z * v.z;
    if (std::fabs(lengthSqr - 1.0f) < 1.1920929e-07f)
        return;
    if (std::fabs(lengthSqr) < 1.1920929e-07f)
    {
        v.x = v.y = v.z = 0.0f;
        return;
    }
    const float inverse = 1.0f / std::sqrt(lengthSqr);
    v.x = v.x * inverse;
    v.y = v.y * inverse;
    v.z = inverse * v.z;
}

// The zero test the bodies apply to trace hits: |c - 0| <= FLT_EPSILON for x, y and z (NaN is
// not zero).
inline bool isZeroVec3(const Vec3& v)
{
    const float epsilon = 1.1920929e-07f;
    return std::fabs(v.x - 0.0f) <= epsilon && std::fabs(v.y - 0.0f) <= epsilon && std::fabs(v.z - 0.0f) <= epsilon;
}

// |a - b|
inline float distanceVec3(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// origin + direction * length
inline Vec3 alongFrom(const Vec3& origin, const Vec3& direction, float length)
{
    Vec3 v;
    v.x = length * direction.x + origin.x;
    v.y = length * direction.y + origin.y;
    v.z = length * direction.z + origin.z;
    return v;
}

// origin - direction * length
inline Vec3 backFrom(const Vec3& origin, const Vec3& direction, float length)
{
    Vec3 v;
    v.x = origin.x - length * direction.x;
    v.y = origin.y - length * direction.y;
    v.z = origin.z - length * direction.z;
    return v;
}

// The first row of a transformation, passed where the bodies hand the Mat4 itself to a
// `const Vec3&` parameter.
inline const Vec3& rightAxis(const Mat4& transformation)
{
    return *reinterpret_cast<const Vec3*>(&transformation.m[0]);
}

// Folds a steering angle beyond +-pi/2 back to the reverse direction: sign(a) * (pi - |a|), with
// sign 0 for 0 and NaN (0x2AE3E1).
inline void reverseAngle(float& angle)
{
    const float magnitude = std::fabs(angle);
    if (!(magnitude > 1.5707964f))
        return;
    float sign = 1.0f;
    if (!(angle > 0.0f))
        sign = (0.0f > angle) ? -1.0f : 0.0f;
    angle = sign * (3.1415927f - magnitude);
}

} // namespace vehiclecontrol
} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_VEHICLECONTROLINLINE_H
