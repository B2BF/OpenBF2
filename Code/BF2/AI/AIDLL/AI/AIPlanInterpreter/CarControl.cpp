// Car steering for the plan interpreter (libaidll.so CarControl TU). Every body is recovered.
// static init: the <iostream> guard object only (ELF 0x2AF060, __tcf_0 0x2AF0C0, _GLOBAL__I 0x2AF0F0).

#include "CarControl.h"

#include "CommonControls.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"

#include <cmath>
#include <iostream>
#include <string>

#include "../../../../IO/Input/PlayerInput.h"

namespace dice {
namespace hfe {
namespace ai {

using namespace vehiclecontrol;

// The TU's anonymous tweak_* values (.data 0x4BCB04..0x4BCB38: terrainSampleInterval 1,
// terrainLookAhead 20, maxSpeed 20, minSpeed 2, stopAndTurn_maxSpeed 7,
// stopAndTurn_angularDeviationThreshold 30, speedScale 2, speedCompensationFallOff 30,
// speedCompensationScale 1, maxSpeedCompensation 10, rotationalSpeedScale 1,
// rotationalSpeedCompensationFallOff 30, rotationalSpeedCompensationScale 1,
// maxRotationalSpeedCompensation 10) have no reader in the image and are not defined here.

// ELF 0x2AAF40 (listing; Ghidra cross-check). Turns and throttles a car from the outputs of
// actionStatusDecision: `mode` (0 = turn on the spot towards `angle`, -1 = the same reversed by pi,
// else drive with `mode` as the speed sign), `angle` (the signed angle to the goal, negated while
// driving with mode != reference and faster than 1 m/s), `reference` and `limit` (the largest
// target speed). Physics is enabled for both sides when a speed is asked for. Driving: turn input
// clamp(3 * angle, -1, 1); target speed = speed * mode, reduced by up to 80 % for a sideways or
// forward slope (the larger |rotational speed| x/z above 0.15), capped at `limit`; throttle =
// clamp(min(1, (target - |v| * reference) / 3) + target / 15, -1, 1), and below 1.5 m/s a non-zero
// throttle becomes its sign (1 or -1, as 0 > throttle). Turning on the spot: the turn input (set only
// while mapped, through the inlined setInput) is min(1, |3 * (|angle| [- pi])|) against the angle's
// sign; the throttle input gets `mode`. Returns 1.
int CarControl::controlTowardsDirection(Information* info, IPIControlInfo* controlInfo, IPIMobile* mobile,
                                        IPIPhysical*, io::PlayerInput* input, float speed, float mode, float angle,
                                        float reference, float limit)
{
    const Mat4& transformation = info->getTransformation();
    Vec2 forward;
    forward.x = forward.y = 0.0f;
    vec3ToVec2XZ(*reinterpret_cast<const Vec3*>(&transformation.m[8]), forward);
    Vec2 right;   // read, never used
    right.x = right.y = 0.0f;
    vec3ToVec2XZ(*reinterpret_cast<const Vec3*>(&transformation.m[0]), right);
    Vec2 position;   // read, never used
    position.x = position.y = 0.0f;
    vec3ToVec2XZ(*reinterpret_cast<const Vec3*>(&transformation.m[12]), position);
    mobile->getMaxSpeed();   // 0x2AAFB8, result discarded
    Vec2 velocity;
    velocity.x = velocity.y = 0.0f;
    vec3ToVec2XZ(mobile->getSpeed(), velocity);
    const float velocityLength = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    Vec2 heading;   // computed, never used
    if (velocityLength == 0.0f)
    {
        heading = forward;
    }
    else
    {
        const float inverse = 1.0f / velocityLength;
        heading.x = velocity.x * inverse;
        heading.y = inverse * velocity.y;
    }
    (void)heading;
    if (mode != 0.0f && reference != mode && velocityLength > 1.0f)
        angle = -angle;
    if (speed != 0.0f)
    {
        // 0x2AB577
        IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
        object->enablePhysics(1);
        object->enablePhysics(2);
    }
    float sign = 0.0f;
    if (angle <= 0.0f)
    {
        if (angle < 0.0f)
            sign = -1.0f;
    }
    else
    {
        sign = 1.0f;
    }

    float throttle;
    if (mode != 0.0f)
    {
        // 0x2AB2E8
        const float scaledAngle = angle * 3.0f;
        float turn = -1.0f;
        if (-1.0f <= scaledAngle)
            turn = (1.0f < scaledAngle) ? 1.0f : scaledAngle;
        input->setInput(controlInfo->getDriveTurnControl(), turn);
        speed = speed * mode;
        const Vec3& rotation = mobile->getRotationalSpeed();
        float tilt = std::fabs(rotation.z);
        if (std::fabs(rotation.z) <= std::fabs(rotation.x))
            tilt = std::fabs(rotation.x);
        if (tilt > 0.15f)
        {
            float scale = (tilt - 0.15f) / 0.7f;
            if (scale > 1.0f)
                scale = 1.0f;
            speed = (1.0f - scale * 0.8f) * speed;
        }
        if (std::fabs(speed) > limit)
        {
            float speedSign = 1.0f;
            if (speed <= 0.0f)
                speedSign = (speed < 0.0f) ? -1.0f : 0.0f;
            speed = speedSign * limit;
        }
        float response = (speed - velocityLength * reference) / 3.0f;
        if (response > 1.0f)
            response = 1.0f;
        response = response + speed / 15.0f;
        throttle = -1.0f;
        if (response >= -1.0f)
            throttle = (response <= 1.0f) ? response : 1.0f;
        if (throttle != 0.0f && !(1.5f <= std::fabs(velocityLength)))
        {
            float throttleSign = 1.0f;
            if (throttle <= 0.0f)
                throttleSign = (0.0f <= throttle) ? 0.0f : -1.0f;
            throttle = throttleSign;
        }
    }
    else
    {
        // 0x2AB0C7
        float turnAngle = std::fabs(angle);
        if (mode == -1.0f)
            turnAngle = turnAngle - 3.1415927f;
        float turn = std::fabs(turnAngle * 3.0f);
        if (turn > 1.0f)
            turn = 1.0f;
        input->setInput(controlInfo->getDriveTurnControl(), turn * -sign);
        throttle = mode;
    }
    input->setInput(controlInfo->getDriveThrottleControl(), throttle);
    return 1;
}

// ELF 0x2AB5F0 (listing). Releases the selected input groups in the order move, look, trigger.
// Moving: the strafe/turn inputs 0, 2 and 1 are released, and the throttle (input 3) brakes against
// `forwardSpeed`: the speed is clamped to +-maxThrottle (a function static, 0.7), negated, and
// within 0.05 of zero the throttle is released; otherwise it is the sign times at least 0.4.
// Look releases inputs 4 and 5; trigger releases 8, 31, 14..30, 38, 37, 10 and 32. Returns 1.
int CarControl::resetControls(Bot*, io::PlayerInput* input, bool move, bool look, bool trigger, float forwardSpeed)
{
    static float maxThrottle = 0.7f;
    if (move)
    {
        input->setInput(static_cast<io::PlayerInputMap>(0), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(2), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(1), 0.0f);
        float clamped;
        if (-maxThrottle > forwardSpeed)
            clamped = -maxThrottle;
        else
            clamped = (forwardSpeed > maxThrottle) ? maxThrottle : forwardSpeed;
        const float brake = -clamped;
        float throttle = 0.0f;
        if (std::fabs(brake) > 0.05f)
        {
            const float sign = (brake > 0.0f) ? 1.0f : ((0.0f > brake) ? -1.0f : 0.0f);
            const float magnitude = std::fabs(brake);
            throttle = sign * ((magnitude > 0.4f) ? magnitude : 0.4f);
        }
        input->setInput(static_cast<io::PlayerInputMap>(3), throttle);
    }
    if (look)
    {
        input->setInput(static_cast<io::PlayerInputMap>(4), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(5), 0.0f);
    }
    if (trigger)
    {
        input->setInput(static_cast<io::PlayerInputMap>(8), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(31), 0.0f);
        for (int i = 14; i <= 30; ++i)
            input->setInput(static_cast<io::PlayerInputMap>(i), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(38), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(37), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(10), 0.0f);
        input->setInput(static_cast<io::PlayerInputMap>(32), 0.0f);
    }
    return 1;
}

// ELF 0x2AB970 (listing block by block; Ghidra cross-check). The car's move-to state machine.
// Measures the free space around the vehicle with eight IAIPathfinding traces from its last valid
// position (forward, the two front diagonals of the bounding box and the same backwards, each
// diagonal/backward trace shortened by the obstacles the navigation holds, then right and left),
// asks CommonControls for the obstacle ahead and the vehicle to slow down for, picks an avoid
// side for a collided vehicle and then advances the bot's move-to status (0 drive, 2/3 and 9/11
// turn forwards/backwards, 4/5 and 13/14 pass an obstacle, 6 avoid, 8 reverse, 12 back off).
// Outputs: `mode` (1 forward, -1 reverse, 666 = arrived at the collided object), `angle` (the
// signed steering angle, -2 = the slow-down stop), `speedSign` (the sign of the forward speed)
// and `speedLimit`. Returns 0 without a mobile or physical part, else 1.
// Shipped behaviour kept: the forward-space trace reads 0 when the trace hit is the zero vector,
// the space-with-obstacles front output is an uninitialised local that is never read, the
// collided object's information is dereferenced without a null test, and statuses 2/3 re-test the
// turn direction they are already in.
int CarControl::actionStatusDecision(Bot* bot, Information* info, const Vec2& direction, float speed,
                                     BAPAMoveTo* moveTo, float& mode, float& angle, float& speedSign,
                                     float& speedLimit)
{
    static float spaceTweak = 1.3f;   // .data 0x4BCB40

    IPIMobile* const mobile = info->getMobile();
    IPIPhysical* const physical = info->getPhysical();
    if (mobile == 0 || physical == 0)
        return 0;
    BotMain* const botMain = static_cast<BotMain*>(bot);   // slots 0x238.. are declared on BotMain only
    const float radius = physical->getRadius();
    Vec2 desired = direction;
    const Vec3& position3 = info->getPosition();
    Vec2 position;
    position.x = position.y = 0.0f;
    Vec2 forward;
    forward.x = forward.y = 0.0f;
    Vec2 normal;
    normal.x = normal.y = 0.0f;
    vec3ToVec2XZ(position3, position);
    vec3ToVec2XZ(info->getFaceing(), forward);
    normalizeVec2(forward);
    getNormal(forward, normal);
    Vec2 goal;
    goal.x = goal.y = 0.0f;
    vec3ToVec2XZ(moveTo->getCoordinate(), goal);
    float goalDistance = moveTo->getTargetDistance();
    if (goalDistance == 0.0f)
    {
        Vec2 toGoal;
        toGoal.x = position.x - goal.x;
        toGoal.y = position.y - goal.y;
        goalDistance = std::sqrt(toGoal.x * toGoal.x + toGoal.y * toGoal.y);
    }
    const float nearDistance = radius * 10.0f;
    const float dot = desired.x * forward.x + desired.y * forward.y;
    angle = getSignedDiffAngleToDirWithNormal(forward, normal, desired);
    const bool angleNegative = 0.0f > angle;
    const float speedMagnitude = mobile->getSpeedMagnitude();
    const Vec3& velocity = mobile->getSpeed();
    const Vec3 facing = info->getFaceing();
    if (velocity.x * facing.x + velocity.y * facing.y + velocity.z * facing.z >= 0.0f)
        speedSign = 1.0f;
    else
        speedSign = -1.0f;
    bool slowDownStop;
    const float slowDown = CommonControls::checkSlowDown(bot, info, velocity, speedMagnitude * speedSign, position3,
                                                         slowDownStop);
    Vec3 forward3;
    forward3.x = forward3.y = forward3.z = 0.0f;
    vec2ToVec3XZ(forward, forward3);
    mode = 1.0f;

    // 0x2ABCE4: the eight space traces.
    IAIPathfinding* const pathfinding = IAIPathfinding::instance;
    mobile->getVehicleNumber();   // result discarded
    BotNavigation* const navigation = bot->getBotNavigation();
    const float traceLength = radius + 10.0f;
    Vec3 hit;
    hit.x = hit.y = hit.z = 0.0f;
    const IPIMobile::ValidPos& lastValid = mobile->getLastValidPosition(pathfinding->getTraceMapIndex());
    const int equipmentType = info->getUnit()->getEquipmentType();
    Vec3 right;
    right.x = normal.x;
    right.y = 0.0f;
    right.z = normal.y;
    int status = botMain->getMoveToStatus();
    const bool reversing = status > 7;
    float back = 0.0f;
    float front = 0.0f;
    LocalBoundingBox* const box = const_cast<LocalBoundingBox*>(physical->getLocalBoundingBox());
    const Vec3& size = box->getSize();
    const float width = size.x;
    const float length = size.z;
    const Vec3 origin = lastValid.position;
    const Vec3 ahead = forward3;
    Vec3 frontLeft;
    frontLeft.x = length * forward3.x - right.x * width;
    frontLeft.y = length * forward3.y - right.y * width;
    frontLeft.z = length * forward3.z - right.z * width;
    normalizeVec3(frontLeft);
    Vec3 frontRight;
    frontRight.x = length * ahead.x + right.x * width;
    frontRight.y = ahead.y * length + right.y * width;
    frontRight.z = length * ahead.z + right.z * width;
    normalizeVec3(frontRight);
    unsigned int polygon;
    unsigned char traceStatus;
    float frontUnused;   // -0x2CC: updateSpaceWithObstacles' front output, never initialised or read

    Vec3 target = alongFrom(origin, ahead, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, 0);
    if (!isZeroVec3(hit))
        front = distanceVec3(hit, origin);
    const float firstFront = front;
    target = alongFrom(origin, frontLeft, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, &traceStatus);
    if (traceStatus != 4 && !isZeroVec3(hit))
    {
        const float d = distanceVec3(hit, origin);
        front = (d > firstFront) ? firstFront : d;
    }
    target = alongFrom(origin, frontRight, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, &traceStatus);
    if (traceStatus != 4 && !isZeroVec3(hit))
    {
        const float d = distanceVec3(hit, origin);
        front = (d > front) ? front : d;
    }
    target = backFrom(origin, ahead, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, 0);
    if (!isZeroVec3(hit))
        back = distanceVec3(hit, origin);
    CommonControls::updateSpaceWithObstacles(bot, origin, ahead, frontUnused, back, 0xFFFFFFFFu);
    target = backFrom(origin, frontLeft, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, &traceStatus);
    if (traceStatus != 4 && !isZeroVec3(hit))
    {
        const float d = distanceVec3(hit, origin);
        back = (d > back) ? back : d;
    }
    CommonControls::updateSpaceWithObstacles(bot, origin, frontLeft, frontUnused, back, 0xFFFFFFFFu);
    target = backFrom(origin, frontRight, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, &traceStatus);
    if (traceStatus != 4 && !isZeroVec3(hit))
    {
        const float d = distanceVec3(hit, origin);
        back = (d > back) ? back : d;
    }
    CommonControls::updateSpaceWithObstacles(bot, origin, frontRight, frontUnused, back, 0xFFFFFFFFu);
    target = alongFrom(origin, right, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, &traceStatus);
    float rightSpace = traceLength;
    if (traceStatus != 4)
        rightSpace = isZeroVec3(hit) ? 0.0f : distanceVec3(hit, origin);
    target = backFrom(origin, right, traceLength);
    pathfinding->trace(navigation, true, target, hit, polygon, equipmentType, -1, 0.0f, 6, &traceStatus);
    float leftSpace = traceLength;
    if (traceStatus != 4)
        leftSpace = isZeroVec3(hit) ? 0.0f : distanceVec3(hit, origin);

    // 0x2AD236: the obstacle ahead.
    const float spaceRadius = radius * spaceTweak;
    unsigned int obstacle = 0xFFFFFFFFu;
    unsigned int collided = botMain->getCollidedObject();
    int avoidSide = botMain->getAvoidSide();
    Vec2 desiredVelocity;
    desiredVelocity.x = speed * direction.x;
    desiredVelocity.y = speed * direction.y;
    float dotForward;
    float angleToObstacle;
    bool outOfOurWay;
    bool passing;
    bool collidedSeen;
    float avoidAngle;
    float tangentAngle;
    bool touching;
    bool collision = CommonControls::checkCollisions(bot, info, physical, forward, dotForward, angleToObstacle,
                                                     outOfOurWay, desiredVelocity, desired, speed, passing, obstacle,
                                                     collided, collidedSeen, avoidAngle, tangentAngle, speedSign,
                                                     touching, static_cast<AvoidSide>(avoidSide), true);
    if (collided != 0xFFFFFFFFu && !collidedSeen)
        collision = false;
    if (collision)
    {
        Information* const obstacleInfo = lookupInformation(obstacle);
        const float limitSqr = botMain->getIgnoreObstacles() ? 2.5f * 2.5f : 1.0f;
        if (obstacle == collided && obstacle != 0xFFFFFFFFu)
        {
            // 0x2AEB27: drop the collision once the collided object has moved away.
            const Vec3& last = botMain->getCollidedObjectPosition();
            const Vec3& now = obstacleInfo->getPosition();
            Vec3 moved;
            moved.x = now.x - last.x;
            moved.y = now.y - last.y;
            moved.z = now.z - last.z;
            if (moved.x * moved.x + moved.y * moved.y + moved.z * moved.z > limitSqr)
                collision = false;
        }
        else
        {
            botMain->setCollidedObjectPosition(obstacleInfo->getPosition());
        }
    }
    if (!collision)
    {
        botMain->setCollidedObject(0xFFFFFFFFu);
        obstacle = 0xFFFFFFFFu;
        collided = 0xFFFFFFFFu;
    }
    bool tooClose = false;
    if (collision)
    {
        botMain->setCollidedObject(obstacle);
        collided = obstacle;
        if (avoidSide == 0)
        {
            // 0x2AD494: choose the side with more room around the collided vehicle.
            Information* const other = lookupInformation(collided);
            IPIMobile* const otherMobile = other->getMobile();
            IPIPhysical* const otherPhysical = other->getPhysical();
            if (otherMobile == 0)
            {
                avoidSide = 1;
            }
            else
            {
                const float clearance = radius + otherPhysical->getSmallestRadius() * 0.5f;
                const float reach = clearance + 0.5f;
                const int vehicleNumber = mobile->getVehicleNumber();
                const IPIMobile::ValidPos& otherValid = otherMobile->getLastValidPosition(vehicleNumber);
                if (otherValid.polygon == 0xFFFFFFFFu || otherValid.island == 0xFFFFFFFFu)
                {
                    avoidSide = 1;
                }
                else
                {
                    Vec3 hitRight;
                    hitRight.x = hitRight.y = hitRight.z = 0.0f;
                    Vec3 hitLeft;
                    hitLeft.x = hitLeft.y = hitLeft.z = 0.0f;
                    unsigned int sidePolygon;
                    Vec3 sideTarget = alongFrom(otherValid.position, rightAxis(info->getTransformation()), reach);
                    pathfinding->trace(otherValid.polygon, otherValid.island, vehicleNumber, otherValid.position,
                                       sideTarget, hitRight, sidePolygon, equipmentType, -1, 0.0f, 0, 0);
                    float roomRight = isZeroVec3(hitRight) ? 0.0f : distanceVec3(hitRight, otherValid.position);
                    sideTarget = backFrom(otherValid.position, rightAxis(info->getTransformation()), reach);
                    pathfinding->trace(otherValid.polygon, otherValid.island, vehicleNumber, otherValid.position,
                                       sideTarget, hitLeft, sidePolygon, equipmentType, -1, 0.0f, 0, 0);
                    float roomLeft = isZeroVec3(hitLeft) ? 0.0f : distanceVec3(hitLeft, otherValid.position);
                    bool firstSide = roomRight < roomLeft;   // before the obstacles shorten them
                    CommonControls::updateSpaceWithObstacles(bot, otherValid.position,
                                                             rightAxis(info->getTransformation()), roomRight,
                                                             roomLeft, collided);
                    const float smaller = (roomLeft < roomRight) ? roomLeft : roomRight;
                    const float larger = (roomRight > roomLeft) ? roomRight : roomLeft;
                    tooClose = clearance > larger;
                    if (smaller > clearance)
                    {
                        // 0x2AEA3E: room on both sides - the side the other vehicle lies on across the desired line.
                        Vec2 sideNormal;
                        sideNormal.x = sideNormal.y = 0.0f;
                        getNormal(desired, sideNormal);
                        Vec2 toOther;
                        toOther.x = otherValid.position.x - position.x;
                        toOther.y = otherValid.position.z - position.y;
                        firstSide = toOther.x * sideNormal.x + toOther.y * sideNormal.y > 0.0f;
                    }
                    else if (!(clearance * 0.7f > larger))
                    {
                        firstSide = roomRight < roomLeft;
                    }
                    avoidSide = 2;
                    if (firstSide)
                        avoidSide = 1;
                }
            }
            if (botMain->setAvoidSide(avoidSide) && !botMain->getIgnoreObstacles())
            {
                status = 0;
                botMain->setMoveToStatus(0);
                botMain->setIgnoreObstacles(true);
            }
        }
    }
    else
    {
        botMain->setAvoidSide(0);
    }

    // 0x2ADA40: arrived next to the collided object's goal?
    bool hasCollided = collided != 0xFFFFFFFFu;
    bot->setUnresolvedCollisionObject(collided);
    if (hasCollided)
    {
        Information* const other = lookupInformation(collided);
        if (other == 0)
        {
            hasCollided = false;
            collided = 0xFFFFFFFFu;
        }
        else
        {
            Vec2 otherPosition;
            otherPosition.x = otherPosition.y = 0.0f;
            vec3ToVec2XZ(*reinterpret_cast<const Vec3*>(&other->getTransformation().m[12]), otherPosition);
            const float otherRadius = other->getPhysical()->getRadius();
            Vec2 goalOffset;
            goalOffset.x = goal.x - otherPosition.x;
            goalOffset.y = goal.y - otherPosition.y;
            if (otherRadius * otherRadius > goalOffset.x * goalOffset.x + goalOffset.y * goalOffset.y)
            {
                Vec2 toOther;
                toOther.x = otherPosition.x - position.x;
                toOther.y = otherPosition.y - position.y;
                const float distanceSqr = toOther.x * toOther.x + toOther.y * toOther.y;
                const float reach = other->getPhysical()->getRadius() + radius + 3.0f;
                if (reach * reach > distanceSqr)
                {
                    mode = 666.0f;
                    angle = 0.0f;
                    return 1;
                }
            }
        }
    }
    else
    {
        avoidSide = 0;
        botMain->setAvoidSide(0);
    }

    // 0x2ADB2D: room flags and the speed limit.
    bool wideSpace = false;
    const bool obstacleAngleNegative = 0.0f > angleToObstacle;
    if (leftSpace + rightSpace > radius * 2.7f && front + back > radius * 2.7f)
        wideSpace = true;
    bool veryWideSpace = false;
    if (leftSpace + rightSpace > radius * 3.0f && front + back > radius * 3.0f)
        veryWideSpace = true;
    const float backRatio = (back - spaceRadius) / (traceLength - spaceRadius);
    const float frontRatio = (front - spaceRadius) / (traceLength - spaceRadius);
    bool stuck = false;
    if (front == 0.0f && back == 0.0f)
    {
        stuck = true;
        status = !(dot > 0.0f) ? 8 : 0;
    }
    const float maxSpeed = mobile->getMaxSpeed();
    const float minSpeed = 2.0f;
    if (!reversing)
    {
        if (0.95f > frontRatio)
            speedLimit = 5.0f - (1.0f - frontRatio) * (5.0f - minSpeed);
        else
            speedLimit = maxSpeed - (1.0f - frontRatio) * (maxSpeed - minSpeed);
    }
    else
    {
        speedLimit = 5.0f - (1.0f - backRatio) * (5.0f - minSpeed);
    }
    if (tooClose)
        speedLimit = (7.0f > speedLimit) ? speedLimit : 7.0f;
    if (!hasCollided)
    {
        botMain->setIgnoreObstacles(false);
        botMain->setAvoidSide(0);
    }
    bool ignoring = botMain->getIgnoreObstacles();
    if (ignoring)
    {
        if (hasCollided)
        {
            // 0x2ADD1E: keep ignoring while we push the collided object, unless it has a human driver.
            Information* const other = lookupInformation(collided);
            IAIObjectPhysical* const otherObject = other->getAIObject()->getPhysical();
            otherObject->enablePhysics(1);
            otherObject->enablePhysics(2);
            if (botMain->getSlowDownToTarget() == collided)
                ignoring = true;
            if (other->getAIObject() != 0 && IBotManager::instance->getBot(other->getAIObject()) == 0
                && other->getUnit() != 0 && other->getUnit()->isOccupied())
                ignoring = false;
        }
        if (ignoring && (static_cast<unsigned int>(status - 4) <= 2u || status == 13 || status == 14))
        {
            status = 0;
            botMain->setMoveToStatus(0);
        }
    }

    // 0x2ADE10
    switch (status)
    {
    case 0:
        if (stuck)
        {
            mode = 1.0f;
            speedLimit = maxSpeed * 0.5f;
            break;
        }
        if (!(0.0f > dot) && !(spaceRadius > front))
        {
            if (collision && !ignoring)
                botMain->setMoveToStatus(6);
            else
                mode = 1.0f;
            break;
        }
        if (nearDistance >= goalDistance && -0.6f > dot && back > spaceRadius)
        {
            botMain->setMoveToStatus(8);
            break;
        }
        if (0.0f > dot && !wideSpace)
        {
            botMain->setMoveToStatus(12);
            break;
        }
        botMain->setMoveToStatus(angleNegative ? 9 : 11);
        break;
    case 2:
        if (angleNegative)
        {
            botMain->setMoveToStatus(dot > 0.0f ? 3 : 9);
            break;
        }
        if (dot > 0.9f)
        {
            botMain->setMoveToStatus(0);
            break;
        }
        if (spaceRadius > front || (dotForward > 0.5f && hasCollided && !outOfOurWay && !ignoring))
        {
            botMain->setMoveToStatus(11);
            break;
        }
        if (angleNegative && wideSpace && 0.0f > dot && back > spaceRadius)
        {
            botMain->setMoveToStatus(9);
            break;
        }
        mode = 1.0f;
        angle = std::fabs(angle);
        break;
    case 3:
        if (!angleNegative)
        {
            botMain->setMoveToStatus(dot > 0.0f ? 2 : 11);
            break;
        }
        if (dot > 0.9f)
        {
            botMain->setMoveToStatus(0);
            break;
        }
        if (spaceRadius > front || (dotForward > 0.5f && hasCollided && !outOfOurWay && !ignoring))
        {
            botMain->setMoveToStatus(9);
            break;
        }
        if (!angleNegative && wideSpace && 0.0f > dot && back > spaceRadius)
        {
            botMain->setMoveToStatus(11);
            break;
        }
        mode = 1.0f;
        angle = -std::fabs(angle);
        break;
    case 4:
        if (!hasCollided || (outOfOurWay && !obstacleAngleNegative))
            botMain->setMoveToStatus(6);
        else if (spaceRadius > front || touching)
            botMain->setMoveToStatus(13);
        else
        {
            mode = 1.0f;
            angle = -2.0f;
        }
        break;
    case 5:
        if (!hasCollided || (outOfOurWay && obstacleAngleNegative))
            botMain->setMoveToStatus(6);
        else if (spaceRadius > front || touching)
            botMain->setMoveToStatus(14);
        else
        {
            mode = 1.0f;
            angle = 2.0f;
        }
        break;
    case 6:
        if (!hasCollided || (passing && 0.3f > dotForward))
        {
            botMain->setMoveToStatus(0.0f > angle ? 3 : 2);
            break;
        }
        if (touching && !outOfOurWay && dotForward > 0.0f)
        {
            botMain->setMoveToStatus(avoidSide == 2 ? 14 : 13);
            break;
        }
        if (front >= spaceRadius)
        {
            angle = avoidAngle;
            break;
        }
        if (avoidSide == 2)
        {
            if (botMain->setAvoidSide(1) && !botMain->getIgnoreObstacles())
            {
                botMain->setMoveToStatus(0);
                botMain->setIgnoreObstacles(true);
            }
            else
            {
                botMain->setMoveToStatus(13);
            }
        }
        else
        {
            if (botMain->setAvoidSide(2) && !botMain->getIgnoreObstacles())
            {
                botMain->setMoveToStatus(0);
                botMain->setIgnoreObstacles(true);
            }
            else
            {
                botMain->setMoveToStatus(14);
            }
        }
        break;
    case 8:
        if (stuck)
        {
            mode = -1.0f;
            angle = -angle;
            reverseAngle(angle);
            break;
        }
        if (dot > -0.5f || goalDistance > nearDistance + 5.0f || spaceRadius > back)
        {
            botMain->setMoveToStatus(0);
            mode = 1.0f;
            break;
        }
        if (-0.6f > dotForward && hasCollided)
        {
            botMain->setMoveToStatus(obstacleAngleNegative ? 3 : 2);
            break;
        }
        mode = -1.0f;
        reverseAngle(angle);
        break;
    case 9:
        if (!angleNegative)
        {
            botMain->setMoveToStatus(dot > 0.0f ? 2 : 11);
            break;
        }
        if (dot > 0.9f && front > spaceRadius)
            botMain->setMoveToStatus(0);
        else if (spaceRadius > back)
            botMain->setMoveToStatus(3);
        else
        {
            mode = -1.0f;
            angle = std::fabs(angle);
        }
        break;
    case 11:
        if (angleNegative)
        {
            botMain->setMoveToStatus(dot > 0.0f ? 3 : 9);
            break;
        }
        if (dot > 0.9f && front > spaceRadius)
            botMain->setMoveToStatus(0);
        else if (spaceRadius > back)
            botMain->setMoveToStatus(2);
        else
        {
            mode = -1.0f;
            angle = -std::fabs(angle);
        }
        break;
    case 12:
        if (dot > 0.0f)
            botMain->setMoveToStatus(0);
        else if (nearDistance >= goalDistance && -0.6f > dot && back > spaceRadius)
            botMain->setMoveToStatus(8);
        else if (veryWideSpace)
            botMain->setMoveToStatus(angleNegative ? 9 : 11);
        else
        {
            mode = -1.0f;
            reverseAngle(angle);
        }
        break;
    case 13:
        if (!hasCollided || (outOfOurWay && !obstacleAngleNegative))
            botMain->setMoveToStatus(6);
        else if (spaceRadius > back)
            botMain->setMoveToStatus(4);
        mode = -1.0f;
        angle = 2.0f;
        break;
    case 14:
        if (!hasCollided || (outOfOurWay && obstacleAngleNegative))
            botMain->setMoveToStatus(6);
        else if (spaceRadius > back)
            botMain->setMoveToStatus(5);
        mode = -1.0f;
        angle = -2.0f;
        break;
    default:
        break;
    }
    // 0x2ADE60
    if (speedLimit > slowDown)
        speedLimit = slowDown;
    if (slowDownStop)
        angle = -2.0f;
    return 1;
}

} // namespace ai
} // namespace hfe
} // namespace dice
