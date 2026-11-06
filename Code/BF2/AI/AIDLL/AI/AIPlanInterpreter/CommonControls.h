// Shared vehicle steering helpers used by CarControl and TankControl (libaidll.so CommonControls TU).

#ifndef AI_AIDLL_AI_AIPLANINTERPRETER_COMMONCONTROLS_H
#define AI_AIDLL_AI_AIPLANINTERPRETER_COMMONCONTROLS_H

#include "../../../../Math/Vec3.h"
#include "../AIBotActionPlan/BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class IPIPhysical;
class Information;

// [INFERRED] a namespace: no vtable, no constructor, and the first argument is never `this`.
namespace CommonControls
{
    void updateSpaceWithObstacles(Bot* bot, const Vec3& origin, const Vec3& direction, float& front, float& back,
                                  unsigned int ignore);   // 0x2AF110
    // 0x2AF500. Returns bool: CarControl::actionStatusDecision stores `al` (0x2AD375).
    bool checkCollisions(Bot* bot, Information* info, IPIPhysical* physical, const Vec2& forward, float& dotForward,
                         float& angleToObstacle, bool& outOfOurWay, const Vec2& velocity, const Vec2& direction,
                         float speed, bool& passing, unsigned int& obstacle, unsigned int& collided,
                         bool& collidedSeen, float& avoidAngle, float& tangentAngle, float speedSign,
                         bool& touching, AvoidSide side, bool tight);
    // 0x2AFF80. Returns the speed limit in st(0).
    float checkSlowDown(Bot* bot, Information* info, const Vec3& velocity, float speed, const Vec3& position,
                        bool& stop);
}

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIPLANINTERPRETER_COMMONCONTROLS_H
