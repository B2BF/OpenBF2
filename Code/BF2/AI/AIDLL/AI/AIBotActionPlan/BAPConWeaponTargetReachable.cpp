// Weapon-target reachability recovered from libaidll.so.
// static init: ELF 0x1F3910 (__tcf_0 0x1F3970, _GLOBAL__I 0x1F39A0): <iostream>'s Init object only.
#include <iostream>
#include "BAPConWeaponTargetReachable.h"
#include "BAPActionInline.h"
#include "Aimer.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F30B0 (C1; C2 0x1F3040)
BAPConWeaponTargetReachable::BAPConWeaponTargetReachable(BAPVariable<Vec3>* position,
    BAPVariable<WeaponInfo*>* weapon, BAPVariable<unsigned int>* target)
    : m_target(target), m_position(position)
{
    m_weapon = weapon;
    m_aimer = new Aimer;
}

// ELF 0x1F3180 (D1; D2 0x1F3120, D0 0x1F31E0)
BAPConWeaponTargetReachable::~BAPConWeaponTargetReachable()
{
    delete m_aimer;
}

// ELF 0x1F3250
bool BAPConWeaponTargetReachable::evaluate(Bot* bot)
{
    static Vec3 zero = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (!m_weapon || !m_weapon->m_value)
        return false;
    WeaponInfo* weapon = m_weapon->m_value;
    Information* target = bapinline::info(m_target->m_value);
    if (!target)
        return false;
    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self)
        return false;
    IPIMobile* mobile = target->getMobile();
    Vec3 targetPosition = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (mobile)
        mobile->getPositionEstimate(targetPosition);
    else
        targetPosition = target->getPosition();
    Vec3 relative = bapinline::sub(targetPosition, self->getPosition());
    if (!bot->getSensedObjectFromHandle(m_target->m_value))
        return false;
    const bool thrown = weapon->getIsThrown();
    weapon->getExplosionRadius();
    const Mat4& origin = weapon->getProjectileOrigin();
    m_aimer->m_speed = weapon->getExitVelocity();
    m_aimer->m_drag = weapon->getDrag();
    m_aimer->m_inverseDrag = m_aimer->m_drag != 0.0f ? 1.0f / m_aimer->m_drag : 0.0f;
    const float gravity = static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity();
    m_aimer->m_gravity = weapon->getGravityModifier() * gravity;
    if (thrown) {
        Vec3 direction = bapinline::vec3(0.0f, 0.0f, 0.0f);
        const double time = bapinline::gameTime();
        relative = bapinline::sub(targetPosition, self->getPosition());
        Vec3 shooterVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
        m_aimer->setTargetData(relative, zero, shooterVelocity, &origin, time);
        Vec3 aimPoint = bapinline::vec3(0.0f, 0.0f, 0.0f);
        if (!m_aimer->getFiringDirection(time, 0.5f, true, direction, aimPoint, 0))
            return false;
        bapinline::normalize(direction);
        const float speed = weapon->getExitVelocity();
        direction.x *= speed;
        direction.y *= speed;
        direction.z *= speed;
        const double extreme = m_aimer->getVerticalExtreme();
        const Vec3& position = self->getPosition();
        const float travelTime = static_cast<float>(extreme);
        Vec3 trajectory = bapinline::vec3(direction.x * travelTime + position.x,
            direction.y * travelTime + position.y, direction.z * travelTime + position.z);
        const float acceleration = static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity();
        trajectory.y += acceleration * static_cast<float>(extreme * extreme * 0.5);
        (void)trajectory;
    }
    // shipped defect: the calculated trajectory is discarded and the output is the shooter's position.
    m_position->m_value = self->getPosition();
    return true;
}

// ELF 0x1F3900
BAPConditionType BAPConWeaponTargetReachable::getConditionType() const
{
    return BAPCTWeaponTargetReachable;
}

} // namespace ai
} // namespace hfe
} // namespace dice
