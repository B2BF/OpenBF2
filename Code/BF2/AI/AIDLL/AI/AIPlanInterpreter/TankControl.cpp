// Tank steering for the plan interpreter (libaidll.so TankControl TU). Every body is recovered.
// static init: the <iostream> guard object only (ELF 0x2D4AF0, __tcf_0 0x2D4B50, _GLOBAL__I 0x2D4B80).

#include "TankControl.h"

#include "CommonControls.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/BotNavigation.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IAIPathfinding.h"
#include "../Main/IBotManager.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../IO/Input/PlayerInput.h"
#include "../../../../Math/Mat4.h"
#include <cmath>
#include "../../../../Math/Vec3.h"

#include <iostream>
#include <string>

namespace dice {
namespace hfe {
namespace ai {

using namespace vehiclecontrol;

namespace {

// The TU's tweak values (.data 0x4BDD84..0x4BDDB8). Only the last three are read (by
// turnTowardsDirection); tweak_maxSpeed 20, tweak_minSpeed 2, tweak_stopAndTurn_maxSpeed 2,
// tweak_speedScale 2, tweak_speedCompensationFallOff 30, tweak_speedCompensationScale 1,
// tweak_maxSpeedCompensation 10, tweak_rotationalSpeedScale 1,
// tweak_rotationalSpeedCompensationFallOff 30, tweak_rotationalSpeedCompensationScale 1 and
// tweak_maxRotationalSpeedCompensation 10 have no reader in the image.
float tweak_velocityLimitModifier = 0.3f;   // .data 0x4BDDB0
float tweak_lowThrottle = 0.5f;             // .data 0x4BDDB4
float tweak_highThrottle = 1.0f;            // .data 0x4BDDB8

// min(1, |signedSpeed|): the pass statuses' steering scale (0x2D3C39).
inline float passTurn(float signedSpeed)
{
    float magnitude = std::fabs(signedSpeed);
    if (magnitude > 1.0f)
        magnitude = 1.0f;
    return magnitude;
}

} // namespace

// ELF 0x2D07B0 (Ghidra; listing for the statics and the inlined setInput). The car version with a
// speed-scaled turn: angle' = min(1, |v| / maxSpeed)^2 * angle, and 0 below minVelocity (the
// statics, 2 and 1). Driving: turn input clamp(3 * angle', -1, 1); target speed = speed * mode,
// capped at `limit` (no slope reduction); throttle = clamp(target / 15 + sign(r) * r^2, -1, 1) with
// r = min(1, (target - |v| * reference) / 3), and below 1.5 m/s a non-zero throttle becomes its sign.
// Turning on the spot: the turn input (only while mapped) is sign(angle') * min(1, |3 * (|angle'|
// [- pi])|); the throttle input gets `mode`. `direction` and the physical part are not read.
// Returns 1.
int TankControl::controlTowardsDirection(Information* info, IPIControlInfo* controlInfo, IPIMobile* mobile,
                                         IPIPhysical*, io::PlayerInput* input, const Vec2&, float speed, float mode,
                                         float angle, float reference, float limit)
{
    static float minVelocity = 1.0f;   // .data 0x4BDDBC
    static float maxSpeed = 2.0f;      // .data 0x4BDDC0

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
    float speedRatio = std::fabs(velocityLength) / maxSpeed;
    if (std::fabs(velocityLength) < minVelocity)
        angle = 0.0f;
    if (speedRatio > 1.0f)
        speedRatio = 1.0f;
    const float scaledAngle = speedRatio * speedRatio * angle;
    float sign = 1.0f;
    if (scaledAngle <= 0.0f)
        sign = (0.0f <= scaledAngle) ? 0.0f : -1.0f;
    if (speed != 0.0f)
    {
        IAIObjectPhysical* const object = info->getAIObject()->getPhysical();
        object->enablePhysics(1);
        object->enablePhysics(2);
    }

    float throttle;
    if (mode != 0.0f)
    {
        const float turnAngle = scaledAngle * 3.0f;
        float turn = -1.0f;
        if (-1.0f <= turnAngle)
            turn = (1.0f < turnAngle) ? 1.0f : turnAngle;
        input->setInput(controlInfo->getDriveTurnControl(), turn);
        speed = speed * mode;
        if (std::fabs(speed) > limit)
        {
            float speedSign = 0.0f;
            if (speed <= 0.0f)
            {
                if (speed < 0.0f)
                    speedSign = -1.0f;
            }
            else
            {
                speedSign = 1.0f;
            }
            speed = speedSign * limit;
        }
        float response = (speed - velocityLength * reference) / 3.0f;
        if (response > 1.0f)
            response = 1.0f;
        float responseSign = 1.0f;
        if (response <= 0.0f)
            responseSign = (0.0f <= response) ? 0.0f : -1.0f;
        const float combined = speed / 15.0f + responseSign * response * response;
        throttle = -1.0f;
        if (-1.0f <= combined)
            throttle = (combined <= 1.0f) ? combined : 1.0f;
        if (throttle != 0.0f && !(1.5f <= std::fabs(velocityLength)))
        {
            float throttleSign = 1.0f;
            if (throttle <= 0.0f)
                throttleSign = (throttle < 0.0f) ? -1.0f : 0.0f;
            throttle = throttleSign;
        }
    }
    else
    {
        float turnAngle = std::fabs(scaledAngle);
        if (mode == -1.0f)
            turnAngle = turnAngle - 3.1415927f;
        float turn = std::fabs(turnAngle * 3.0f);
        if (turn > 1.0f)
            turn = 1.0f;
        input->setInput(controlInfo->getDriveTurnControl(), sign * turn);
        throttle = mode;
    }
    input->setInput(controlInfo->getDriveThrottleControl(), throttle);
    return 1;
}

// ELF 0x2D0E30 (listing; Ghidra cross-check). Turns a tank on the spot towards `angle`:
// turn input -sign(angle) * (forward ? 1 : -1); the rotational speed about y is driven to
// min(1, (|angle| + 0.05) / maxOffsetAngle) times that and rotationTweak in steps of 0.05; the
// throttle is (forward ? 1 : -1) times tweak_lowThrottle while the ground speed is above
// min(1, angle^2 * tweak_velocityLimitModifier), else tweak_highThrottle. `unused` and `done` are
// not read; the normalised ground velocity is computed and dropped.
void TankControl::turnTowardsDirection(float angle, float, bool forward, Information* info, IPIMobile* mobile,
                                       IPIControlInfo* controlInfo, io::PlayerInput* input, bool*)
{
    static float rotationTweak = 2.0f;    // .data 0x4BDDC4
    static float maxOffsetAngle = 0.6f;   // .data 0x4BDDC8

    Vec2 velocity;
    velocity.x = velocity.y = 0.0f;
    vec3ToVec2XZ(mobile->getSpeed(), velocity);
    const float velocityLength = std::sqrt(velocity.x * velocity.x + velocity.y * velocity.y);
    const float inverse = 1.0f / velocityLength;
    Vec2 heading;   // computed, never used
    heading.x = velocity.x * inverse;
    heading.y = velocity.y * inverse;
    (void)heading;
    const float direction = forward ? 1.0f : -1.0f;
    float sign = 1.0f;
    if (!(angle > 0.0f))
        sign = (0.0f > angle) ? -1.0f : 0.0f;
    float velocityLimit = angle * angle * tweak_velocityLimitModifier;
    if (velocityLimit > 1.0f)
        velocityLimit = 1.0f;
    const float turn = -sign * direction;
    input->setInput(controlInfo->getDriveTurnControl(), turn);
    float offset = (std::fabs(angle) + 0.05f) / maxOffsetAngle;
    if (offset > 1.0f)
        offset = 1.0f;
    info->getAIObject()->striveForRotationalSpeed(1, offset * turn * rotationTweak, 0.05f);
    if (velocityLength > velocityLimit)
        input->setInput(controlInfo->getDriveThrottleControl(), direction * tweak_lowThrottle);
    else
        input->setInput(controlInfo->getDriveThrottleControl(), direction * tweak_highThrottle);
}

// ELF 0x2D1010 (Ghidra). The car's reset with the tank's own maxThrottle static (also 0.7):
// moving releases inputs 0, 2 and 1 and brakes the throttle (input 3) against `forwardSpeed`
// (clamped to +-maxThrottle, negated; released within 0.05 of zero, else the sign times at least
// 0.4). Look releases 4 and 5; trigger releases 8, 31, 14..30, 38, 37, 10 and 32. Returns 1.
int TankControl::resetControls(Bot*, io::PlayerInput* input, bool move, bool look, bool trigger, float forwardSpeed)
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

// ELF 0x2D1390 (listing, diffed against CarControl::actionStatusDecision 0x2AB970 instruction by
// instruction; Ghidra cross-check). The tank's move-to state machine, the car's with these changes:
// the near-goal distance is 5 radii (car 10), checkCollisions looks ahead 0.5 s (car 0.25 s), the
// speed limit is maxSpeed - (1 - ratio) * maxSpeed / 2 (no 5 m/s band), the pass statuses 4/5/13/14
// steer +-0.2 * min(1, |signed speed|) (car +-2) and also leave when touching an obstacle more than
// 0.8 ahead, status 11 returns to 0 above 0.9 without the front-space test, and status 12 turns
// towards the side with more room. Outputs and shipped quirks as in the car version.
int TankControl::actionStatusDecision(Bot* bot, Information* info, const Vec2& direction, float speed,
                                     BAPAMoveTo* moveTo, float& mode, float& angle, float& speedSign,
                                     float& speedLimit)
{
    static float spaceTweak = 1.3f;   // .data 0x4BDDD0

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
    const float nearDistance = radius * 5.0f;
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
    const float signedSpeed = speedMagnitude * speedSign;
    const float slowDown = CommonControls::checkSlowDown(bot, info, velocity, signedSpeed, position3, slowDownStop);
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
                                                     touching, static_cast<AvoidSide>(avoidSide), false);
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
    const float frontRatio = (front - spaceRadius) / (traceLength - spaceRadius);
    const float backRatio = (back - spaceRadius) / (traceLength - spaceRadius);
    bool stuck = false;
    if (front == 0.0f && back == 0.0f)
    {
        stuck = true;
        status = !(dot > 0.0f) ? 8 : 0;
    }
    const float maxSpeed = mobile->getMaxSpeed();
    const float minSpeed = maxSpeed * 0.5f;
    if (!reversing)
        speedLimit = maxSpeed - (1.0f - frontRatio) * (maxSpeed - minSpeed);
    else
        speedLimit = maxSpeed - (1.0f - backRatio) * (maxSpeed - minSpeed);
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
        else if (spaceRadius > front || (touching && dotForward > 0.8f))
            botMain->setMoveToStatus(13);
        else
        {
            mode = 1.0f;
            angle = passTurn(signedSpeed) * -0.2f;
        }
        break;
    case 5:
        if (!hasCollided || (outOfOurWay && obstacleAngleNegative))
            botMain->setMoveToStatus(6);
        else if (spaceRadius > front || (touching && dotForward > 0.8f))
            botMain->setMoveToStatus(14);
        else
        {
            mode = 1.0f;
            angle = passTurn(signedSpeed) * 0.2f;
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
        if (dot > 0.9f)
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
            botMain->setMoveToStatus(rightSpace > leftSpace ? 9 : 11);
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
        angle = passTurn(signedSpeed) * 0.2f;
        break;
    case 14:
        if (!hasCollided || (outOfOurWay && obstacleAngleNegative))
            botMain->setMoveToStatus(6);
        else if (spaceRadius > back)
            botMain->setMoveToStatus(5);
        mode = -1.0f;
        angle = passTurn(signedSpeed) * -0.2f;
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
