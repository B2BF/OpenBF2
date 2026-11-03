// EntryPlaneAimAt (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2C0B00 (__tcf_0 0x2C0B60, _GLOBAL__I 0x2C0B90): <iostream>'s Init object only.
#include <iostream>

#include "EntryPlaneAimAt.h"

#include "PlaneControl.h"
#include "VehicleControlInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBotActionPlan/BAPAAimAtObject3d.h"
#include "../AIBotActionPlan/BAPCondition.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2BFBD0 (C1; C2 0x2BFBB0).
EntryPlaneAimAt::EntryPlaneAimAt()
{
}

// ELF 0x2BFC10 (D1; D2 0x2BFBF0, D0 0x2BFC30).
EntryPlaneAimAt::~EntryPlaneAimAt()
{
}

// ELF 0x2C0BB0.
int EntryPlaneAimAt::getTargetId()
{
    return 6;
}

// ELF 0x2BFC70.
bool EntryPlaneAimAt::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAAimAt3d* const aim = static_cast<BAPAAimAt3d*>(script);
    ok = true;
    done = false;
    if (aim->m_condition->evaluate(bot))
    {
        done = true;
        return true;
    }
    Information* const info = informationManager->getInformation(bot->getCurrentEquipment());
    if (!info)
    {
        done = true;
        ok = false;
        aim->m_floats1AC[0] = 3.1415927f;
        return true;
    }
    IPIArmament* const armament = info->getArmament();
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    IPIMobile* const mobile = info->getMobile();
    if (aim->getAimAtType() != 1)
    {
        done = true;
        ok = false;
        aim->m_floats1AC[0] = 3.1415927f;
        return true;
    }
    Information* const target = informationManager->getInformation(static_cast<BAPAAimAtObject3d*>(aim)->m_target);
    if (!target)
    {
        done = true;
        ok = false;
        aim->m_floats1AC[0] = 3.1415927f;
        return true;
    }
    Vec3 direction = {0.0f, 0.0f, 0.0f};
    Vec3 firingDirection = {0.0f, 0.0f, 0.0f};
    const Mat4& origin = weapon->getProjectileOrigin();
    const Mat4& targetTransform = target->getTransformation();
    float throttle = 1.0f;
    float distance;
    bool useForward = false;
    if ((target->getObjectTypeFlags() & 0x10) != 0)
    {
        const bool indirect = weapon->getIndirect();
        const float gravity = weapon->getGravityModifier();
        const float speed = weapon->getExitVelocity();
        const float drag = weapon->getDrag();
        bool solved = aim->getAimVec(origin, mobile->getSpeed(), direction, 0.5f, drag, speed, gravity, indirect);
        if (solved)
            firingDirection = direction;
        else
        {
            if (!aim->getVectorToTarget(info->getPosition(), direction))
            {
                ok = false;
                done = true;
                return true;
            }
            const float length = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (length > 1.0f)
            {
                solved = true;
                const float inverse = 1.0f / length;
                direction.x *= inverse;
                direction.y *= inverse;
                direction.z *= inverse;
            }
        }
        distance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        useForward = !solved;
    }
    else
    {
        const bool indirect = weapon->getIndirect();
        const float gravity = weapon->getGravityModifier();
        const float speed = weapon->getExitVelocity();
        const float drag = weapon->getDrag();
        const bool solved = aim->getAimVec(origin, mobile->getSpeed(), direction, 0.75f, drag, speed, gravity, false);
        throttle = 0.5f;
        if (solved)
        {
            firingDirection = direction;
            if (indirect)
            {
                const float x = origin.m[12] - targetTransform.m[12];
                const float y = origin.m[13] - targetTransform.m[13];
                const float z = origin.m[14] - targetTransform.m[14];
                distance = std::sqrt(x * x + y * y + z * z);
                if (distance < 60.0f || distance > 300.0f)
                {
                    direction.y = 0.0f;
                    if (std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z) > 1e-05f)
                    {
                        const float inverse = 1.0f / std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
                        direction.x *= inverse;
                        direction.y *= inverse;
                        direction.z *= inverse;
                    }
                }
            }
            else
                distance = std::sqrt(direction.x * direction.x + direction.z * direction.z);
        }
        else
        {
            if (!aim->getVectorToTarget(info->getPosition(), direction))
            {
                ok = false;
                done = true;
                return true;
            }
            direction.y = 20.0f + direction.y;
            distance = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            if (distance > 1.0f)
            {
                const float inverse = 1.0f / distance;
                direction.x *= inverse;
                direction.y *= inverse;
                direction.z *= inverse;
            }
            else
                useForward = true;
        }
    }
    if (useForward || -0.7f > direction.y)
    {
        direction = info->getFaceing();
        direction.y = 0.0f;
        distance = 1.0f;
        vehiclecontrol::normalizeVec3(direction);
    }
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    const Vec3& speed = mobile->getSpeed();
    const float inverse = 1.0f / dt;
    const Vec3 acceleration = {(speed.x - aim->m_vecA8.x) * inverse,
                               (speed.y - aim->m_vecA8.y) * inverse,
                               (speed.z - aim->m_vecA8.z) * inverse};
    (void)acceleration;
    aim->m_vecA8 = speed;
    Mat4 inverseOrigin = {{1.0f, 0.0f, 0.0f, 0.0f,
                          0.0f, 1.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 1.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, 1.0f}};
    inverseOrigin.setInverse(origin);
    const Vec3 localDirection = {
        direction.x * inverseOrigin.m[0] + direction.y * inverseOrigin.m[4] + direction.z * inverseOrigin.m[8],
        direction.x * inverseOrigin.m[1] + direction.y * inverseOrigin.m[5] + direction.z * inverseOrigin.m[9],
        direction.x * inverseOrigin.m[2] + direction.y * inverseOrigin.m[6] + direction.z * inverseOrigin.m[10]};
    const Mat4& transform = info->getTransformation();
    const Vec3 steeringDirection = {
        localDirection.x * transform.m[0] + localDirection.y * transform.m[4] + localDirection.z * transform.m[8],
        localDirection.x * transform.m[1] + localDirection.y * transform.m[5] + localDirection.z * transform.m[9],
        localDirection.x * transform.m[2] + localDirection.y * transform.m[6] + localDirection.z * transform.m[10]};
    const auto clamp = [](float value) {
        return -1.0f > value ? -1.0f : (value > 1.0f ? 1.0f : value);
    };
    if (aim->isAimVecValid())
    {
        const Mat4& weaponUp = weapon->getAbsoluteTransformation();
        float dot = firingDirection.x * weaponUp.m[4] + firingDirection.y * weaponUp.m[5] + firingDirection.z * weaponUp.m[6];
        const float pitch = std::acos(clamp(clamp(dot))) - 1.5707964f;
        const Mat4& weaponRight = weapon->getAbsoluteTransformation();
        dot = firingDirection.x * weaponRight.m[0] + firingDirection.y * weaponRight.m[1] + firingDirection.z * weaponRight.m[2];
        float yaw = std::acos(clamp(clamp(dot))) - 1.5707964f;
        const Mat4& weaponForward = weapon->getAbsoluteTransformation();
        dot = firingDirection.x * weaponForward.m[8] + firingDirection.y * weaponForward.m[9] + firingDirection.z * weaponForward.m[10];
        if (dot < 0.0f)
            yaw = (yaw < 0.0f ? -3.1415927f : 3.1415927f) - yaw;
        aim->m_floats1AC[1] = pitch;
        aim->m_floats1AC[0] = yaw;
    }
    else
    {
        aim->m_floats1AC[1] = 3.1415927f;
        aim->m_floats1AC[0] = 3.1415927f;
    }
    aim->m_vec24 = info->getFaceing();
    const Mat4& right = info->getTransformation();
    aim->m_vec30 = {right.m[0], right.m[1], right.m[2]};
    const Mat4& up = info->getTransformation();
    aim->m_vec3C = {up.m[4], up.m[5], up.m[6]};
    aim->m_floats1AC[3] = info->getPosition().y;
    ok = PlaneControl::aimAtDirection(bot, info, steeringDirection, distance, throttle, aim->m_float1A4,
                                     aim->m_vec24, aim->m_vec30, aim->m_vec3C, bot->getInput());
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
