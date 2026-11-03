// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1FADB0 (__tcf_0 0x1FAE10, _GLOBAL__I 0x1FAE40): <iostream>'s Init object only.
#include <iostream>

#include "BAPWrapperLookAt.h"

#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/IPIControlInfo.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIKnowledge/WeaponInfoManager.h"
#include "../Main/AIDllFrontier.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1FA390 (C1; C2 0x1FA2D0): look at an object.
BAPWrapperLookAt::BAPWrapperLookAt(unsigned int target, BAPVariable<Vec3>* lookPoint, bool continuous, bool lead,
                                   BAPVariable<Vec3>* lookDirection, BAPVariable<bool>* valid)
{
    m_target = target;
    m_lookPoint = lookPoint;
    m_lookDirection = lookDirection;
    m_valid = valid;
    m_point.x = m_point.y = m_point.z = 0.0f;
    m_continuous = continuous;
    m_lead = lead;
}

// ELF 0x1FA4B0 (C1; C2 0x1FA410): look at a fixed point.
BAPWrapperLookAt::BAPWrapperLookAt(const Vec3& point, BAPVariable<Vec3>* lookPoint, bool continuous,
                                   BAPVariable<Vec3>* lookDirection, BAPVariable<bool>* valid)
{
    m_target = 0xFFFFFFFFu;
    m_lookPoint = lookPoint;
    m_lookDirection = lookDirection;
    m_valid = valid;
    m_point = point;
    m_lead = false;
    m_continuous = continuous;
}

// ELF 0x1FAE60 (D1; D0 0x1FAEA0): base only.
BAPWrapperLookAt::~BAPWrapperLookAt()
{
}

// ELF 0x1FA550. Fixed point: the point, ok, done unless continuous (the direction variable is not
// written). Object: unknown information: ok = false, done. Otherwise ok, done unless continuous,
// valid = true, and when the bot senses it: the sensed hit point in world space (or, with m_lead,
// that point led by the target's velocity over the projectile's flight time from the camera,
// raised by 5% of the distance for soldiers) into m_lookPoint and the normalised direction from
// the bot into m_lookDirection. The bot's information is not null-checked.
bool BAPWrapperLookAt::execute(Bot* bot, bool& ok, bool& done)
{
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (m_target == 0xFFFFFFFFu) {
        ok = true;
        done = !m_continuous;
        m_lookPoint->m_value = m_point;
        return true;
    }
    Information* target = bapinline::info(m_target);
    if (!target) {
        ok = false;
        done = true;
        return true;
    }
    ok = true;
    done = !m_continuous;
    if (m_valid)
        m_valid->m_value = true;
    SensedObject* sensed = bot->getSensedObjectFromHandle(m_target);
    if (!sensed)
        return true;

    const Mat4& t = target->getTransformation();
    const Vec3& l = sensed->m_seenPosition;
    const Vec3 point = bapinline::vec3(t.m[8] * l.z + t.m[0] * l.x + t.m[4] * l.y + t.m[12],
                                       t.m[1] * l.x + t.m[5] * l.y + t.m[9] * l.z + t.m[13],
                                       l.y * t.m[6] + l.x * t.m[2] + l.z * t.m[10] + t.m[14]);
    Vec3 dir;
    if (!m_lead) {
        m_lookPoint->m_value = point;
        if (!m_lookDirection)
            return true;
        dir = bapinline::sub(point, self->getPosition());
        bapinline::normalize(dir);
    } else {
        IPIMobile* targetMobile = target->getMobile();
        IPIArmament* armament = self->getArmament();
        WeaponInfo* weapon = WeaponInfoManager::m_instance->getInformation(armament->getWeapon(armament->getActiveWeapon()));
        const Mat4& origin = weapon->getProjectileOrigin();
        Vec3 projSpeed = bapinline::vec3(0.0f, 0.0f, 0.0f);
        const bool haveSpeed = armament->getProjectileSpeed(projSpeed);
        Vec3 projPos = bapinline::vec3(0.0f, 0.0f, 0.0f);
        armament->getProjectilePosition(projPos);
        const float speedLen = std::sqrt(projSpeed.z * projSpeed.z + projSpeed.y * projSpeed.y + projSpeed.x * projSpeed.x);
        const Vec3 from = (speedLen == 0.0f || !haveSpeed) ? bapinline::vec3(origin.m[12], origin.m[13], origin.m[14])
                                                           : projPos;
        Vec3 rel = bapinline::sub(point, from);
        const Mat4* camera = &self->getControlInfo()->getCameraTransformation();
        Vec3 targetVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
        if (targetMobile)
            targetVelocity = targetMobile->getSpeed();
        if (std::sqrt(projSpeed.z * projSpeed.z + projSpeed.y * projSpeed.y + projSpeed.x * projSpeed.x) == 0.0f) {
            projSpeed.x = camera->m[8] * 20.0f;
            projSpeed.y = camera->m[9] * 20.0f;
            projSpeed.z = camera->m[10] * 20.0f;
        }
        const float closing = std::sqrt((projSpeed.z - targetVelocity.z) * (projSpeed.z - targetVelocity.z)
                                        + (projSpeed.x - targetVelocity.x) * (projSpeed.x - targetVelocity.x)
                                        + (projSpeed.y - targetVelocity.y) * (projSpeed.y - targetVelocity.y));
        const float distance = std::sqrt(rel.z * rel.z + rel.y * rel.y + rel.x * rel.x);
        const float flight = distance / closing;
        if (flight < 2.5f) {
            rel.x = targetVelocity.x * flight + rel.x;
            rel.z = targetVelocity.z * flight + rel.z;
            rel.y = rel.y + 0.0f;
        }
        camera = &self->getControlInfo()->getCameraTransformation();
        if (self->getObjectTypeFlags() & 0x200000u)
            rel.y = distance * 0.05f + rel.y;
        m_lookPoint->m_value = bapinline::vec3(rel.x + camera->m[12], rel.y + camera->m[13], rel.z + camera->m[14]);
        if (!m_lookDirection)
            return true;
        dir = bapinline::sub(m_lookPoint->m_value, self->getPosition());
        bapinline::normalize(dir);
    }
    m_lookDirection->m_value = dir;
    return true;
}

// ELF 0x1FAD90
BAPStatementType BAPWrapperLookAt::getId() const
{
    return BAPSTLookAtWrapper;   // 19
}

// ELF 0x1FADA0
bool BAPWrapperLookAt::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
