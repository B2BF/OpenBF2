// Medium-LOD vehicle movement towards an object. Attested TU: EntryMoveToObjectMedium.cpp.

// static init: ELF 0x2BDED0 (__tcf_0 0x2BDF30, _GLOBAL__I 0x2BDF60): <iostream>'s Init object only.
#include <iostream>

#include "EntryMoveToObjectMedium.h"

#include "InfanteryControl.h"
#include "TankControl.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"
#include "../../../../Math/Mat4.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

namespace {

// ELF 0x2BD0B0.
void rotate2d(const Vec3& input, Vec3& output, float angle)
{
    output.y = 0.0f;
    const float cosine = cosf(angle);
    const float sine = sinf(angle);
    output.x = cosine * input.x - sine * input.z;
    output.z = sine * input.x + cosine * input.z;
}

} // namespace

// ELF 0x2BD150 (C1; C2 0x2BD130).
EntryMoveToObjectMedium::EntryMoveToObjectMedium()
{
}

// ELF 0x2BD190 (D1; D2 0x2BD170, D0 0x2BD1B0).
EntryMoveToObjectMedium::~EntryMoveToObjectMedium()
{
}

// ELF 0x2BDF80.
int EntryMoveToObjectMedium::getTargetId()
{
    return 9;
}

// ELF 0x2BD1F0. A missing object finishes successfully. Arrival clears done, and movement
// follows the turn-limited heading until the remaining distance is shorter than a tick's step.
bool EntryMoveToObjectMedium::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
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
    IAIObjectMobile* const mobile = object->getMobile();
    IAIObjectPhysical* const physical = object->getPhysical();
    if (physical->hasPhysicsEnabled())
        physical->disablePhysics(info->getSide());
    const Vec3 facing = info->getFaceing();
    const Vec3& velocity = mobile->getSpeed();
    TankControl::resetControls(bot, bot->getInput(), true, false, false,
                               velocity.x * facing.x + velocity.y * facing.y + velocity.z * facing.z);
    Information* const target = informationManager->getInformation(moveTo->getObject());
    if (!target || moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
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
    {
        done = false;
        return true;
    }
    const float ownX = info->getPosition().x;
    const float ownZ = info->getPosition().z;
    const Vec3& ownPosition = info->getPosition();
    const Vec3& targetPosition = target->getPosition();
    (void)(targetPosition.z - ownPosition.z);
    Vec2 delta;
    delta.x = point.x - ownX;
    delta.y = point.y - ownZ;
    const float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    if (distance <= 0.01f)
    {
        done = false;
        return true;
    }
    Mat4 transformation = object->getTransformation();
    const double timeIncrease = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease();
    Vec2 forward2;
    forward2.x = transformation.m[8];
    forward2.y = transformation.m[10];
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
    const float speedFactor = forwardDot < 0.2f ? 0.2f : forwardDot;
    const float scaledSpeed = speedFactor * transformation.m[5] * 0.6f;
    const float speed = mobile->getMaxSpeed() * scaledSpeed;
    Vec3 position = object->getPosition();
    Vec3 forward;
    forward.x = delta.x / distance;
    forward.y = 0.0f;
    forward.z = delta.y / distance;
    const float dt = static_cast<float>(timeIncrease);
    const float step = dt * speed;
    const float radius = mobile->getTurnRadius();
    const float limitedRadius = radius < 1.0f ? 1.0f : radius;
    const float angularStep = step / limitedRadius;
    const float limit = angularStep < 0.003926991f ? 0.003926991f : angularStep;
    if (limit < 6.283185482f && std::fabs(angle) > limit)
    {
        Vec3 current;
        current.x = forward2.x;
        current.y = 0.0f;
        current.z = forward2.y;
        const float sign = angle > 0.0f ? 1.0f : (0.0f > angle ? -1.0f : 0.0f);
        rotate2d(current, forward, sign * limit);
    }
    float stepX = delta.x;
    float stepZ = delta.y;
    if (!(static_cast<double>(speed) * timeIncrease > static_cast<double>(distance)))
    {
        stepX = forward.x * dt * speed;
        stepZ = forward.z * dt * speed;
    }
    position.x = stepX + position.x;
    position.z = stepZ + position.z;
    const float currentHeight = info->getPosition().y;
    float height;
    Vec3 up;
    up.x = up.y = up.z = 0.0f;
    float slope;
    static_cast<IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)
        ->getHeightAndNormalForObject(object, position, height, up, slope);
    if (!(currentHeight > height) || !(currentHeight - height > slope + slope))
        position.y = mobile->getLodHeight() + height;
    else
        position.y = currentHeight - slope;
    Vec3 right = infantry_detail::crossInline(up, forward);
    vehiclecontrol::normalizeVec3(right);
    forward = infantry_detail::crossInline(right, up);
    vehiclecontrol::normalizeVec3(forward);
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
