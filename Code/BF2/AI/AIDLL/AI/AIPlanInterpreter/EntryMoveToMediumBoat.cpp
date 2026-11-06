// Medium-LOD boat movement entry. Attested TU: EntryMoveToMediumBoat.cpp.

// static init: ELF 0x2BC280 (__tcf_0 0x2BC2E0, _GLOBAL__I 0x2BC310): <iostream>'s Init object only.
#include <iostream>

#include "EntryMoveToMediumBoat.h"

#include "BoatControl.h"
#include "InfanteryControl.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/CommonSingletonServer.h"
#include "../Main/IAIPathfinding.h"
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/LocalBoundingBox.h"
#include "../../../../Math/Mat4.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF 0x2BB280.
void rotate2d(const Vec3& input, Vec3& output, float angle)
{
    output.y = 0.0f;
    const float cosine = cosf(angle);
    const float sine = sinf(angle);
    output.x = cosine * input.x - sine * input.z;
    output.z = sine * input.x + cosine * input.z;
}

} // namespace

// ELF 0x2BB320 (C1; C2 0x2BB300).
EntryMoveToMediumBoat::EntryMoveToMediumBoat()
{
}

// ELF 0x2BB360 (D1; D2 0x2BB340, D0 0x2BB380).
EntryMoveToMediumBoat::~EntryMoveToMediumBoat()
{
}

// ELF 0x2BC330.
int EntryMoveToMediumBoat::getTargetId()
{
    return 7;
}

// ELF 0x2BB3C0. Moves the boat without physics, retaining a running statement when both the
// current and proposed positions fail the pathfinding check. Grounded boats follow the terrain;
// floating boats use the water level and a world-up axis.
bool EntryMoveToMediumBoat::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAMoveTo* const moveTo = static_cast<BAPAMoveTo*>(script);
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        return true;
    }
    IAIObject* const object = info->getAIObject();
    IAIObjectPhysical* const physical = object->getPhysical();
    if (physical->hasPhysicsEnabled())
        physical->disablePhysics(object->getSide());
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    bool resetDone;
    BoatControl::resetControls(bot, true, false, false, resetDone);
    Vec3 position = object->getPosition();
    Vec2 position2;
    vec3ToVec2XZ(position, position2);
    Vec2 point;
    point.x = point.y = 0.0f;
    bool arrived;
    ok = static_cast<unsigned char>(moveTo->update(point, done, arrived, 0.0f)) != 0;
    if (!ok)
    {
        done = true;
        return true;
    }
    if (done || arrived)
        return true;
    Vec2 delta;
    delta.x = point.x - position2.x;
    delta.y = point.y - position2.y;
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance <= 0.01f)
    {
        done = true;
        return true;
    }
    object->releaseObjectFromSpawner();
    IAIObjectMobile* const mobile = object->getMobile();
    const Mat4 previous = object->getTransformation();
    Vec2 forward2;
    forward2.x = previous.m[8];
    forward2.y = previous.m[10];
    vehiclecontrol::normalizeVec2(forward2);
    const float inverse = 1.0f / distance;
    const float targetX = inverse * delta.x;
    const float targetZ = inverse * delta.y;
    float rightDot = targetX * forward2.y - targetZ * forward2.x;
    const float forwardDot = targetX * forward2.x + targetZ * forward2.y;
    if (rightDot < -1.0f)
        rightDot = -1.0f;
    else if (rightDot > 1.0f)
        rightDot = 1.0f;
    float angle = acosf(rightDot) - 1.570796371f;
    if (forwardDot < 0.0f)
    {
        const float sign = angle > 0.0f ? 1.0f : (0.0f > angle ? -1.0f : 0.0f);
        angle = sign * 3.141592741f - angle;
    }
    IAITerrain* const terrain = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain);
    const float speedFactor = forwardDot < 0.2f ? 0.2f : forwardDot;
    const float scaledSpeed = speedFactor * previous.m[5] * 0.9f;
    const float speed = mobile->getMaxSpeed() * scaledSpeed;
    const double timeIncrease = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();
    const float dt = static_cast<float>(timeIncrease);
    const float step = dt * speed;
    const float radius = mobile->getTurnRadius();
    const float limitedRadius = radius < 1.0f ? 1.0f : radius;
    const float angularStep = step / limitedRadius;
    const float limit = angularStep < 0.003926991f ? 0.003926991f : angularStep;
    Vec3 forward;
    if (limit < 6.283185482f && std::fabs(angle) > limit)
    {
        Vec3 current;
        current.x = forward2.x;
        current.y = 0.0f;
        current.z = forward2.y;
        const float sign = angle > 0.0f ? 1.0f : (0.0f > angle ? -1.0f : 0.0f);
        rotate2d(current, forward, sign * limit);
    }
    else
    {
        forward.x = delta.x;
        forward.y = 0.0f;
        forward.z = delta.y;
    }
    if (!(static_cast<double>(speed) * timeIncrease > static_cast<double>(distance)))
    {
        if (distance > 1e-05f)
        {
            const float invDistance = 1.0f / distance;
            delta.x = delta.x * invDistance;
            delta.y = invDistance * delta.y;
        }
        position.x = delta.x * dt * speed + position.x;
        position.z = delta.y * dt * speed + position.z;
    }
    else if (!(distance > 0.5f))
    {
        position.x = previous.m[8] * 0.5f + position.x;
        position.z = 0.5f * previous.m[10] + position.z;
    }
    else
    {
        position.x = delta.x + position.x;
        position.z = delta.y + position.z;
    }
    vehiclecontrol::normalizeVec3(forward);
    const int equipmentType = info->getUnit()->getEquipmentType();
    if (static_cast<unsigned char>(moveTo->usesPathfinding()))
    {
        const Vec3& currentPosition = info->getPosition();
        const int vehicle = mobile->getVehicleNumber();
        if (!IAIPathfinding::instance->isValidPosition(vehicle, equipmentType, currentPosition, true))
        {
            const int candidateVehicle = mobile->getVehicleNumber();
            if (!IAIPathfinding::instance->isValidPosition(candidateVehicle, equipmentType, position, true))
                return true;
        }
    }
    Vec3 up;
    up.x = up.y = up.z = 0.0f;
    unsigned char material;
    const float height = terrain->getHeightAndNormal(position.x, position.z, up, material);
    const float water = terrain->getWaterLevel(position.x, position.z);
    const float lodHeight = mobile->getLodHeight();
    Vec3 right;
    if (water - lodHeight > height)
    {
        position.y = mobile->getLodHeight() + water;
        up.x = 0.0f;
        up.y = 1.0f;
        up.z = 0.0f;
        right = infantry_detail::crossInline(up, forward);
    }
    else
    {
        LocalBoundingBox* const bounds = const_cast<LocalBoundingBox*>(physical->getLocalBoundingBox());
        position.y = 0.5f * bounds->getSize().y + height;
        right = infantry_detail::crossInline(up, forward);
        vehiclecontrol::normalizeVec3(right);
        forward = infantry_detail::crossInline(right, up);
        vehiclecontrol::normalizeVec3(forward);
    }
    Mat4 transformation = object->getTransformation();
    transformation.m[0] = right.x;
    transformation.m[1] = right.y;
    transformation.m[2] = right.z;
    transformation.m[4] = up.x;
    transformation.m[5] = up.y;
    transformation.m[6] = up.z;
    transformation.m[8] = forward.x;
    transformation.m[9] = forward.y;
    transformation.m[10] = forward.z;
    transformation.m[12] = position.x;
    transformation.m[13] = position.y;
    transformation.m[14] = position.z;
    object->setTransformation(transformation, bot->getSide());
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
