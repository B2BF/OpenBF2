// Medium-LOD vehicle movement entry. Attested TU: EntryMoveToMedium.cpp.

// static init: ELF 0x2BB1C0 (__tcf_0 0x2BB220, _GLOBAL__I 0x2BB250): <iostream>'s Init object only.
#include <iostream>

#include "EntryMoveToMedium.h"

#include "InfanteryControl.h"
#include "TankControl.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAMoveTo.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/InformationManager.h"
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

// ELF 0x2BA300. The output's y is cleared before the trigonometric calls; input may alias output.
void rotate2d(const Vec3& input, Vec3& output, float angle)
{
    output.y = 0.0f;
    const float cosine = cosf(angle);
    const float sine = sinf(angle);
    output.x = cosine * input.x - sine * input.z;
    output.z = sine * input.x + cosine * input.z;
}

} // namespace

// ELF 0x2BA3A0 (C1; C2 0x2BA380).
EntryMoveToMedium::EntryMoveToMedium()
{
}

// ELF 0x2BA3E0 (D1; D2 0x2BA3C0, D0 0x2BA400).
EntryMoveToMedium::~EntryMoveToMedium()
{
}

// ELF 0x2BB270.
int EntryMoveToMedium::getTargetId()
{
    return 7;
}

// ELF 0x2BA440. Moves a vehicle without physics, limits its heading change by the turn radius,
// and aligns the resulting transform with the terrain normal.
bool EntryMoveToMedium::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
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
        physical->disablePhysics(object->getSide());
    if (moveTo->isFinished(bot))
    {
        done = true;
        ok = true;
        return true;
    }
    done = false;
    const Vec3 facing = info->getFaceing();
    const Vec3& velocity = mobile->getSpeed();
    TankControl::resetControls(bot, bot->getInput(), true, false, false,
                               velocity.x * facing.x + velocity.y * facing.y + velocity.z * facing.z);
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
    const Mat4 previous = object->getTransformation();
    Vec2 forward2;
    forward2.x = previous.m[8];
    forward2.y = previous.m[10];
    vehiclecontrol::normalizeVec2(forward2);
    // The shipped angle calculation divides by distance even when it is zero.
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
    const float scaledSpeed = speedFactor * previous.m[5] * 0.6f;
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
