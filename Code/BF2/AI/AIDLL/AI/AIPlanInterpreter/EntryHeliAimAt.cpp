// EntryHeliAimAt (split out of BAPEntries.cpp by S0, bodies unchanged).

// static init: ELF 0x2B6400 (__tcf_0 0x2B6460, _GLOBAL__I 0x2B6490): <iostream>'s Init object only.
#include <iostream>

#include "EntryHeliAimAt.h"

#include "HeliControl.h"
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

// ELF 0x2B5380 (C1; C2 0x2B5360).
EntryHeliAimAt::EntryHeliAimAt()
{
}

// ELF 0x2B53C0 (D1; D2 0x2B53A0, D0 0x2B53E0).
EntryHeliAimAt::~EntryHeliAimAt()
{
}

// ELF 0x2B64B0.
int EntryHeliAimAt::getTargetId()
{
    return 6;
}

// ELF 0x2B5420.
bool EntryHeliAimAt::execute(BotScript* script, Bot* bot, bool& ok, bool& done)
{
    BAPAAimAt3d* const aim = static_cast<BAPAAimAt3d*>(script);
    ok = true;
    done = false;
    if (aim->m_condition->evaluate(bot))
        done = true;
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
    Information* target = 0;
    if (aim->getAimAtType() == 1)
    {
        target = informationManager->getInformation(static_cast<BAPAAimAtObject3d*>(aim)->m_target);
        if (!target)
        {
            done = true;
            ok = false;
            aim->m_floats1AC[0] = 3.1415927f;
            return true;
        }
    }
    Vec3 direction = {0.0f, 0.0f, 1.0f};
    Vec3 firingDirection = {0.0f, 0.0f, 0.0f};
    const Mat4& origin = weapon->getProjectileOrigin();
    const Vec3& velocity = mobile->getSpeed();
    const Vec3& position = info->getPosition();
    // shipped defect: point-aim statements leave target null, yet it is read here.
    const Vec3& targetPosition = target->getPosition();
    const float x = position.x - targetPosition.x;
    const float y = position.y - targetPosition.y;
    const float z = position.z - targetPosition.z;
    const float distance = std::sqrt(x * x + y * y + z * z);
    const bool airborne = (target->getObjectTypeFlags() & 0x10) != 0;
    bool solved;
    if (airborne)
    {
        const bool indirect = weapon->getIndirect();
        const float gravity = weapon->getGravityModifier();
        const float speed = weapon->getExitVelocity();
        const float drag = weapon->getDrag();
        solved = aim->getAimVec(origin, mobile->getSpeed(), direction, 0.5f, drag, speed, gravity, indirect);
        if (solved)
        {
            firingDirection = direction;
            vehiclecontrol::normalizeVec3(direction);
        }
    }
    else
    {
        const bool indirect = weapon->getIndirect();
        const float gravity = weapon->getGravityModifier();
        const float speed = weapon->getExitVelocity();
        const float drag = weapon->getDrag();
        if (indirect)
        {
            const Vec3 stationary = {0.0f, 0.0f, 0.0f};
            solved = aim->getAimVec(origin, stationary, direction, 0.75f, drag, speed, gravity, false);
        }
        else
            solved = aim->getAimVec(origin, velocity, direction, 0.5f, drag, speed, gravity, false);
        if (solved)
        {
            firingDirection = direction;
            if (indirect)
                direction.y = 0.0f;
            vehiclecontrol::normalizeVec3(direction);
        }
    }
    if (!solved)
    {
        const bool found = aim->getVectorToTarget(info->getPosition(), direction);
        if (!found && !airborne)
        {
            ok = false;
            done = true;
        }
        vehiclecontrol::normalizeVec3(direction);
        if (!found)
        {
            if (airborne)
            {
                ok = false;
                done = true;
            }
            return true;
        }
    }
    const float dt = static_cast<float>(static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getTimeIncrease());
    const Vec3& speed = mobile->getSpeed();
    const float inverse = 1.0f / dt;
    const Vec3 acceleration = {(speed.x - aim->m_vecA8.x) * inverse,
                               (speed.y - aim->m_vecA8.y) * inverse,
                               (speed.z - aim->m_vecA8.z) * inverse};
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
    // [INFERRED] inline clamp expansion; unordered input passes through these two tests.
    const auto clamp = [](float value, float low, float high) {
        return low > value ? low : (value > high ? high : value);
    };
    if (aim->isAimVecValid())
    {
        const Mat4& weaponUp = weapon->getAbsoluteTransformation();
        float dot = firingDirection.x * weaponUp.m[4] + firingDirection.y * weaponUp.m[5] + firingDirection.z * weaponUp.m[6];
        const float pitch = std::acos(clamp(clamp(dot, -1.0f, 1.0f), -1.0f, 1.0f)) - 1.5707964f;
        const Mat4& weaponRight = weapon->getAbsoluteTransformation();
        dot = firingDirection.x * weaponRight.m[0] + firingDirection.y * weaponRight.m[1] + firingDirection.z * weaponRight.m[2];
        float yaw = std::acos(clamp(clamp(dot, -1.0f, 1.0f), -1.0f, 1.0f)) - 1.5707964f;
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
    const float difference = direction.y - aim->m_vec1C8.y;
    // 0x2B5C45..0x2B5C59: ja then fcmovnbe, so a NaN difference passes through to asin.
    const float asinInput = clamp(difference, -1.0f, 1.0f);
    const float pitchRate = clamp(std::asin(asinInput) / dt, -0.1f, 0.1f);
    aim->m_vec1C8 = direction;
    ok = HeliControl::aimAtDirection(bot, steeringDirection, distance, aim->m_float1A4, acceleration, pitchRate) != 0;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
