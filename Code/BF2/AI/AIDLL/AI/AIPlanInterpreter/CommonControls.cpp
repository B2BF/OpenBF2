// Shared vehicle steering helpers for CarControl and TankControl (libaidll.so CommonControls TU).
// static init: the <iostream> guard object only (ELF 0x2B0910, __tcf_0 0x2B0970, _GLOBAL__I 0x2B09A0).
// Bodies recovered from the ELF listings (objdump with GOT/rodata resolved; Ghidra cross-check).
// Comparisons are written as compiled, so NaN takes the branch the x87/SSE code takes.
// [PLATFORM] the image keeps intermediate sums in x87 extended precision; the native build rounds
// each float operation.

#include "CommonControls.h"

#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBot/CommunicationAgent.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <iostream>
#include <vector>

namespace dice {
namespace hfe {
namespace ai {

namespace {

inline Information* lookupInformation(unsigned int handle)
{
    return informationManager->getInformation(handle);
}

} // namespace

// ELF 0x2AF110. Shortens the free space ahead (`front`) and behind (`back`) of `origin` along the
// unit direction `direction` by the obstacles the bot's navigation has collected: each other
// non-soldier object with a physical part (except `ignore` and the bot's own vehicle) whose
// smallest-radius sphere crosses the line within the current space limits the space to the
// sphere's near intersection.
void CommonControls::updateSpaceWithObstacles(Bot* bot, const Vec3& origin, const Vec3& direction, float& front,
                                              float& back, unsigned int ignore)
{
    Information* const self = lookupInformation(bot->getCurrentEquipment());
    BotNavigation* const navigation = bot->getBotNavigation();
    const std::vector<unsigned int>& obstacles = navigation->getObstacles();
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        const unsigned int handle = obstacles[i];
        if (handle == ignore)
            continue;
        Information* const obstacle = lookupInformation(handle);
        if (self == obstacle || obstacle == 0)
            continue;
        IPIPhysical* const physical = obstacle->getPhysical();
        if (physical == 0 || obstacle->isOfType(ITSoldier))
            continue;
        const float radius = physical->getSmallestRadius();
        const Vec3& position = obstacle->getPosition();
        Vec3 offset;
        offset.x = position.x - origin.x;
        offset.y = position.y - origin.y;
        offset.z = position.z - origin.z;
        const float along = direction.x * offset.x + direction.y * offset.y + direction.z * offset.z;
        if (along > 0.0f)
        {
            if (!(along > radius + front))
            {
                Vec3 perpendicular;
                perpendicular.x = offset.x - direction.x * along;
                perpendicular.y = offset.y - direction.y * along;
                perpendicular.z = offset.z - direction.z * along;
                const float distance = std::sqrt(perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y
                                                 + perpendicular.z * perpendicular.z);
                if (!(distance > radius))
                {
                    const float half = std::sqrt(radius * radius - distance * distance);
                    const float space = along - half;
                    if (!(space > front))
                        front = space;
                }
            }
        }
        else if (!(-along > radius + back))
        {
            Vec3 perpendicular;
            perpendicular.x = offset.x - direction.x * along;
            perpendicular.y = offset.y - direction.y * along;
            perpendicular.z = offset.z - direction.z * along;
            const float distance = std::sqrt(perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y
                                             + perpendicular.z * perpendicular.z);
            if (!(distance > radius))
            {
                const float half = std::sqrt(radius * radius - distance * distance);
                const float space = -along - half;
                if (!(space > back))
                    back = space;
            }
        }
    }
}

// ELF 0x2AF500. Picks the obstacle the vehicle is about to hit. `forward` and `direction` are the
// unit heading and the desired direction (XZ), `velocity` the desired 2D velocity, `speed` its
// length and `speedSign` the sign of the forward speed (nothing is looked at while reversing).
// A candidate is a non-soldier obstacle within the look-ahead reach, ahead of the heading and
// within the combined radius of the heading line; the one closest to the line wins (low-angle
// ones only when they are the bot's collided object or there is none). With no candidate the
// collided object (`collided`, which also sets `collidedSeen` when it is still an obstacle) is
// kept. For the winner the body writes `dotForward` (its direction against the heading),
// `angleToObstacle`, `outOfOurWay` / `passing` (whether the heading / desired line clear its
// avoid circle), `touching`, the tangent angle `avoidAngle` (`tangentAngle` the direction to it),
// or, inside the avoid circle, the angle to the side's perpendicular (`tangentAngle` -666), and
// `obstacle`. Returns whether an obstacle was chosen; when the desired direction already leads
// away from it both handles are cleared and false is returned.
bool CommonControls::checkCollisions(Bot* bot, Information* info, IPIPhysical* physical, const Vec2& forward,
                                     float& dotForward, float& angleToObstacle, bool& outOfOurWay,
                                     const Vec2& velocity, const Vec2& direction, float speed, bool& passing,
                                     unsigned int& obstacle, unsigned int& collided, bool& collidedSeen,
                                     float& avoidAngle, float& tangentAngle, float speedSign, bool& touching,
                                     AvoidSide side, bool tight)
{
    static float radiusTweak = 0.9f;           // .data 0x4BCB84, not read by the body
    static float obstacleRadiusTweak = 0.8f;   // .data 0x4BCB88
    static float outOfOurWayTweak = 0.7f;      // .data 0x4BCB8C
    (void)radiusTweak;

    IPIMobile* const mobile = info->getMobile();
    const float radius = physical->getRadius();
    BotNavigation* const navigation = bot->getBotNavigation();
    const std::vector<unsigned int>& obstacles = navigation->getObstacles();
    float bestDistanceSqr = 99999.0f;
    dotForward = 0.0f;
    collidedSeen = false;
    const float lookAheadTime = 1.0f;
    unsigned int best = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        const unsigned int handle = obstacles[i];
        if (collided != 0xFFFFFFFFu && collided == handle)
            collidedSeen = true;
        if (0.0f > speedSign)
            continue;
        Information* const other = lookupInformation(handle);
        if (info == other || other == 0)
            continue;
        IPIPhysical* const otherPhysical = other->getPhysical();
        if (otherPhysical == 0 || other->isOfType(ITSoldier))
            continue;
        const float combinedRadius = otherPhysical->getRadius() + radius;
        const Vec3& ownPosition = info->getPosition();
        const Vec3& otherPosition = other->getPosition();
        Vec3 offset;
        offset.x = otherPosition.x - ownPosition.x;
        offset.y = otherPosition.y - ownPosition.y;
        offset.z = otherPosition.z - ownPosition.z;
        Vec2 offset2;
        offset2.x = offset.x;
        offset2.y = offset.z;
        const float distanceSqr = offset.x * offset.x + offset.y * offset.y + offset.z * offset.z;
        const Vec3& ownSpeed = mobile->getSpeed();
        const Vec3 facing = info->getFaceing();
        const float minimumReach = (radius > 5.0f) ? radius : 5.0f;
        const float reach = (facing.x * ownSpeed.x + facing.y * ownSpeed.y + facing.z * ownSpeed.z) * lookAheadTime
                            + minimumReach + combinedRadius;
        if (distanceSqr > reach * reach)
            continue;
        if (distanceSqr > speed * speed)
        {
            Vec2 predicted;
            predicted.x = offset2.x - velocity.x;
            predicted.y = offset2.y - velocity.y;
            if (predicted.x * predicted.x + predicted.y * predicted.y > combinedRadius * combinedRadius)
                continue;
        }
        const float along = forward.x * offset2.x + forward.y * offset2.y;
        if (0.0f > along)
            continue;
        const float length = std::sqrt(offset2.x * offset2.x + offset2.y * offset2.y);
        const float cosine = along / length;
        Vec2 perpendicular;
        perpendicular.x = offset2.x - forward.x * along;
        perpendicular.y = offset2.y - forward.y * along;
        const float perpendicularSqr = perpendicular.x * perpendicular.x + perpendicular.y * perpendicular.y;
        if (perpendicularSqr > combinedRadius * combinedRadius)
            continue;
        if (0.5f > cosine && collided != handle && collided != 0xFFFFFFFFu)
            continue;
        if (bestDistanceSqr > perpendicularSqr)
        {
            bestDistanceSqr = perpendicularSqr;
            best = handle;
        }
    }
    if (best == 0xFFFFFFFFu)
    {
        if (collidedSeen)
        {
            const unsigned int handle = collided;
            best = lookupInformation(handle) ? handle : best;
        }
        if (best == 0xFFFFFFFFu)
            return false;
    }
    Information* const target = lookupInformation(best);
    if (target == 0)
        return best != 0xFFFFFFFFu;
    IPIPhysical* const targetPhysical = target->getPhysical();
    IPIMobile* const targetMobile = target->getMobile();
    if (targetMobile)
        targetMobile->getSpeedMagnitude();   // result discarded
    const float avoidRadius = targetPhysical->getRadius() * obstacleRadiusTweak + radius;
    const float avoidRadiusSqr = avoidRadius * avoidRadius;
    Vec2 ownPosition;
    ownPosition.x = info->getPosition().x;
    ownPosition.y = info->getPosition().z;
    Vec2 targetPosition;
    targetPosition.x = target->getPosition().x;
    targetPosition.y = target->getPosition().z;
    Vec2 toTarget;
    toTarget.x = targetPosition.x - ownPosition.x;
    toTarget.y = targetPosition.y - ownPosition.y;
    const float distance = std::sqrt(toTarget.x * toTarget.x + toTarget.y * toTarget.y);
    if (1e-05f > distance)
        return false;
    const float inverse = 1.0f / distance;
    toTarget.x = toTarget.x * inverse;
    toTarget.y = inverse * toTarget.y;
    dotForward = toTarget.x * forward.x + toTarget.y * forward.y;
    touching = targetPhysical->getRadius() + radius > distance;
    Vec2 ahead = ownPosition;
    const float aheadTime = tight ? 0.25f : 0.5f;
    ahead.x = aheadTime * mobile->getSpeed().x + ahead.x;
    ahead.y = aheadTime * mobile->getSpeed().z + ahead.y;
    Vec2 toTargetAhead;
    toTargetAhead.x = targetPosition.x - ahead.x;
    toTargetAhead.y = targetPosition.y - ahead.y;
    const float aheadDistance = std::sqrt(toTargetAhead.x * toTargetAhead.x + toTargetAhead.y * toTargetAhead.y);
    const float aheadInverse = 1.0f / aheadDistance;
    toTargetAhead.x = toTargetAhead.x * aheadInverse;
    toTargetAhead.y = aheadInverse * toTargetAhead.y;
    if (avoidRadius > aheadDistance)
    {
        // 0x2AFBAD: inside the avoid circle - steer along the side's perpendicular.
        Vec2 sideDirection;
        sideDirection.x = 0.0f;
        sideDirection.y = 0.0f;
        if (side == static_cast<AvoidSide>(2))
        {
            sideDirection.x = toTargetAhead.y;
            sideDirection.y = -toTargetAhead.x;
        }
        else
        {
            sideDirection.x = -toTargetAhead.y;
            sideDirection.y = toTargetAhead.x;
        }
        avoidAngle = getSignedDiffAngleToDir(forward, sideDirection);
        tangentAngle = -666.0f;
    }
    else
    {
        // 0x2AFE7D: the tangent past the avoid circle.
        const float ratio = avoidRadius / aheadDistance;
        float clamped = ratio;
        if (-1.0f > ratio)
            clamped = -1.0f;
        else if (ratio > 1.0f)
            clamped = 1.0f;
        avoidAngle = std::asin(clamped);
        const float angle = getSignedDiffAngleToDir(forward, toTargetAhead);
        tangentAngle = angle;
        if (side == static_cast<AvoidSide>(2))
            avoidAngle = angle + avoidAngle;
        else
            avoidAngle = angle - avoidAngle;
    }
    // 0x2AFC06: do the heading line and the desired line clear the target?
    Vec2 headingPoint;
    headingPoint.x = forward.x * dotForward * distance + ownPosition.x;
    headingPoint.y = dotForward * forward.y * distance + ownPosition.y;
    const float headingX = headingPoint.x - targetPosition.x;
    const float headingY = headingPoint.y - targetPosition.y;
    outOfOurWay = headingX * headingX + headingY * headingY > avoidRadiusSqr * outOfOurWayTweak;
    const float alongDesired = toTarget.x * direction.x + toTarget.y * direction.y;
    Vec2 desiredPoint;
    desiredPoint.x = direction.x * alongDesired * distance + ownPosition.x;
    desiredPoint.y = direction.y * alongDesired * distance + ownPosition.y;
    const float desiredX = desiredPoint.x - targetPosition.x;
    const float desiredY = desiredPoint.y - targetPosition.y;
    bool clear = false;
    if (desiredX * desiredX + desiredY * desiredY > avoidRadiusSqr)
        clear = true;
    else if (-0.1f > alongDesired && speedSign > 0.0f)
        clear = true;
    passing = clear;
    if (0.0f > toTarget.x * direction.x + toTarget.y * direction.y
        && forward.x * direction.x + forward.y * direction.y > 0.0f)
    {
        // 0x2AFD88: the desired direction already leads away from it.
        collided = 0xFFFFFFFFu;
        passing = true;
        obstacle = 0xFFFFFFFFu;
        outOfOurWay = true;
        return false;
    }
    const float headingDot = forward.x * direction.x + forward.y * direction.y;
    if (std::fabs(headingDot) > 0.85f)
        angleToObstacle = getSignedDiffAngleToDir(direction, toTarget);
    else
        angleToObstacle = getSignedDiffAngleToDir(forward, toTarget);
    obstacle = best;
    return best != 0xFFFFFFFFu;
}

// ELF 0x2AFF80. The speed limit for following the slowest vehicle ahead. At or below 2 m/s
// (`speed` is the signed forward speed) nothing is checked and 9999 is returned. Otherwise every
// obstacle whose sphere lies on the projected path within 15 m is rated by its speed along our
// heading minus 1 (at least 0); vehicles beyond 8 m (maxDist) or crossing our path count only
// when the relative speed is above 10 m/s, which also rates -20 and sets `stop` while the bot has
// no move-to status. The lowest rating is returned and its handle stored as the bot's slow-down
// target; a same-side object within 5 m (or a soldier ahead) is honked at when we carry a driver.
float CommonControls::checkSlowDown(Bot* bot, Information* info, const Vec3& velocity, float speed,
                                    const Vec3& position, bool& stop)
{
    static float maxDist = 8.0f;   // .data 0x4BCB90

    stop = false;
    if (2.0f >= speed)
        return 9999.0f;
    BotNavigation* const navigation = bot->getBotNavigation();
    const std::vector<unsigned int>& obstacles = navigation->getObstacles();
    const Vec3 facing = info->getFaceing();
    const Vec3 facingAgain = info->getFaceing();
    const float facingDot = facingAgain.x * velocity.x + facingAgain.y * velocity.y + facingAgain.z * velocity.z;
    float sign = 1.0f;
    if (!(facingDot > 0.0f))
        sign = (0.0f > facingDot) ? -1.0f : 0.0f;
    Vec3 heading;
    heading.x = facing.x * sign;
    heading.y = facing.y * sign;
    heading.z = sign * facing.z;
    float lowest = 9999.0f;
    unsigned int lowestHandle = 0xFFFFFFFFu;
    Information* honkTarget = 0;
    IPIPhysical* const ownPhysical = info->getPhysical();
    IPIMobile* const ownMobile = info->getMobile();
    const float zero = 0.0f;
    for (unsigned int i = 0; i < obstacles.size(); ++i)
    {
        Information* const other = lookupInformation(obstacles[i]);
        if (other == 0 || other == info)
            continue;
        IPIPhysical* const otherPhysical = other->getPhysical();
        if (otherPhysical == 0)
            continue;
        if (other->isOfType(ITSoldier) && 0.0f >= otherPhysical->getHealth())
            continue;
        IPIMobile* const otherMobile = other->getMobile();
        if (otherMobile == 0)
            continue;
        Information* const changeTarget = lookupInformation(bot->getChangeTarget());
        if (changeTarget && !changeTarget->isPrimary()
            && changeTarget->getPrimaryObject(bot->getSide()) == obstacles[i])
            continue;
        // 0x2B01AB: the centre of the other's sphere.
        Vec3 offset;
        offset.x = offset.y = offset.z = zero;
        const Mat4& transformation = other->getTransformation();
        const Vec3 local = otherPhysical->getSphereLocalOffset();
        const float* const m = transformation.m;
        offset.x = m[0] * local.x + m[4] * local.y + m[8] * local.z;
        offset.y = m[1] * local.x + m[5] * local.y + m[9] * local.z;
        offset.z = m[2] * local.x + m[6] * local.y + m[10] * local.z;
        const Vec3& otherPosition = other->getPosition();
        Vec3 centre;
        centre.x = offset.x + otherPosition.x;
        centre.y = offset.y + otherPosition.y;
        centre.z = offset.z + otherPosition.z;
        Vec3 toCentre;
        toCentre.x = centre.x - position.x;
        toCentre.y = centre.y - position.y;
        toCentre.z = centre.z - position.z;
        float distance = std::sqrt(toCentre.x * toCentre.x + toCentre.y * toCentre.y + toCentre.z * toCentre.z);
        const float otherRadius = otherPhysical->getSmallestRadius();
        const float radii = otherRadius + ownPhysical->getSmallestRadius();
        if (distance > 1e-05f)
        {
            const float inverse = 1.0f / distance;
            toCentre.x = toCentre.x * inverse;
            toCentre.y = toCentre.y * inverse;
            toCentre.z = inverse * toCentre.z;
        }
        distance = distance - radii;
        if (distance > 15.0f)
            continue;
        const float otherSpeed = otherMobile->getSpeedMagnitude();
        const float along = heading.x * toCentre.x + heading.y * toCentre.y + heading.z * toCentre.z;
        const float reach = distance + radii;
        Vec3 projected;
        projected.x = heading.x * along * reach + position.x;
        projected.y = heading.y * along * reach + position.y;
        projected.z = heading.z * along * reach + position.z;
        Vec3 miss;
        miss.x = centre.x - projected.x;
        miss.y = centre.y - projected.y;
        miss.z = centre.z - projected.z;
        if (miss.x * miss.x + miss.y * miss.y + miss.z * miss.z > radii * radii)
            continue;
        if (other->isOfType(ITSoldier))
        {
            // 0x2B049C: a soldier ahead is only a horn candidate.
            if (along > 0.0f && info->getSide() == other->getSide())
                honkTarget = other;
            continue;
        }
        if (0.75f > along)
        {
            if (!(2.0f > distance))
                continue;
            if (!(along > 0.0f))
                continue;
            if (!(otherSpeed > 2.0f))
                continue;
        }
        // 0x2B054C
        Vec3 otherVelocity = otherMobile->getSpeed();
        const bool moving = otherSpeed > 0.5f;
        float rating = heading.x * otherVelocity.x + heading.y * otherVelocity.y + heading.z * otherVelocity.z - 1.0f;
        if (!(rating > zero))
            rating = zero;
        if (moving)
        {
            const float inverse = 1.0f / otherSpeed;
            otherVelocity.x = otherVelocity.x * inverse;
            otherVelocity.y = otherVelocity.y * inverse;
            otherVelocity.z = otherVelocity.z * inverse;
        }
        const Vec3& otherSpeedVector = otherMobile->getSpeed();
        const Vec3& ownSpeedVector = ownMobile->getSpeed();
        Vec3 relative;
        relative.x = otherSpeedVector.x - ownSpeedVector.x;
        relative.y = otherSpeedVector.y - ownSpeedVector.y;
        relative.z = otherSpeedVector.z - ownSpeedVector.z;
        const bool fastApproach = relative.x * relative.x + relative.y * relative.y + relative.z * relative.z > 100.0f;
        if (fastApproach && static_cast<BotMain*>(bot)->getMoveToStatus() == 0)
        {
            rating = -20.0f;
            stop = true;
        }
        if (distance > maxDist && !fastApproach)
            continue;
        const float otherDot = otherVelocity.x * heading.x + otherVelocity.y * heading.y + otherVelocity.z * heading.z;
        if (moving && 0.6f > otherDot && otherDot > -0.75f && !fastApproach)
            continue;
        if (static_cast<BotMain*>(bot)->getIgnoreObstacles()
            && static_cast<BotMain*>(bot)->getCollidedObject() == obstacles[i] && !fastApproach)
            continue;
        if (info->getSide() == other->getSide() && 5.0f > distance)
            honkTarget = other;
        if (lowest > rating)
        {
            lowest = rating;
            lowestHandle = obstacles[i];
        }
    }
    if (honkTarget && info->getUnit()->isOccupied())
        bot->getCommunicationAgent()->honkHorn(honkTarget);
    static_cast<BotMain*>(bot)->setSlowDownToTarget(lowestHandle);
    return lowest;
}

} // namespace ai
} // namespace hfe
} // namespace dice
