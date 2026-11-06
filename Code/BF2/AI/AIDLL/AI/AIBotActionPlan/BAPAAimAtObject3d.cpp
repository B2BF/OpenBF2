// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1CFC90 (__tcf_0 0x1CFCF0, _GLOBAL__I 0x1CFD20): <iostream>'s Init object only.
#include <iostream>

#include "BAPAAimAtObject3d.h"

#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "BAPActionInline.h"
#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1CF410 (C1; C2 0x1CF340). The condition's owner is set without a null test, as compiled.
BAPAAimAtObject3d::BAPAAimAtObject3d(Bot* bot, unsigned int target, const Vec3& offset, BAPCondition* condition,
                                     unsigned long long controls, int patternId, float float1A4, float float1A8)
{
    m_bot = bot;
    m_target = target;
    m_float1A4 = float1A4;
    m_float1A8 = float1A8;
    m_offset = offset;
    m_condition = condition;
    condition->setOwner(this);
    m_patternId = patternId;
    m_int1BC = 0;
    m_controls = controls;
    m_int1C4 = 0;
    m_double19C = static_cast<double>(-5000000000.0f);
}

// ELF 0x1CF560 (D1 0x1CF4E0, D0 0x1CF5E0)
BAPAAimAtObject3d::~BAPAAimAtObject3d()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1CFD40
int BAPAAimAtObject3d::getAimAtType()
{
    return 1;
}

// ELF 0x1CFBC0
bool BAPAAimAtObject3d::getVectorToTarget(const Vec3& from, Vec3& vector) const
{
    Information* const target = bapinline::info(m_target);
    if (target)
        vector = bapinline::sub(target->getPosition(), from);
    return target != 0;
}

// ELF 0x1CF660. Refuses a slow projectile (speed at most 0.1) from a nearly resting shooter. The
// target point is the bot's sensed point on the object, in the object's current transformation,
// or else the object's position; its velocity is the object's IPIMobile speed, or that of its
// primary object for the bot's side, or zero.
bool BAPAAimAtObject3d::getAimVec(const Mat4& transform, const Vec3& shooterVelocity, Vec3& direction,
                                  float accuracy, float drag, float speed, float gravityScale, bool highArc)
{
    if (speed <= 0.1f && bapinline::lengthSqr(shooterVelocity) <= 0.01f)
        return false;
    Bot* const bot = m_bot;
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    Information* const target = bapinline::info(m_target);
    if (!target)
        return false;

    m_aimerTransform = transform;
    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = speed;
    m_aimer.m_gravity = static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity() * gravityScale;

    Vec3 targetPosition = bapinline::vec3(0.0f, 0.0f, 0.0f);
    if (SensedObject* const sensed = bot->getSensedObjectFromHandle(m_target))
    {
        const Mat4& t = target->getAIObject()->getTransformation();
        const Vec3& p = sensed->m_seenPosition;
        targetPosition.x = t.m[0] * p.x + t.m[4] * p.y + t.m[8] * p.z + t.m[12];
        targetPosition.y = t.m[1] * p.x + t.m[5] * p.y + t.m[9] * p.z + t.m[13];
        targetPosition.z = t.m[2] * p.x + t.m[6] * p.y + t.m[10] * p.z + t.m[14];
    }
    else
    {
        targetPosition = target->getPosition();
    }
    const Vec3 relative = bapinline::sub(targetPosition, *reinterpret_cast<const Vec3*>(&m_aimerTransform.m[12]));

    Vec3 targetVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
    IPIMobile* mobile = target->getMobile();
    if (!mobile && !target->isPrimary())
    {
        if (Information* const primary = bapinline::info(target->getPrimaryObject(m_bot->getSide())))
            mobile = primary->getMobile();
    }
    if (mobile)
        targetVelocity = mobile->getSpeed();

    m_aimer.setTargetData(relative, targetVelocity, shooterVelocity, &transform, timer->getGameTime());
    m_aimVecValid = m_aimer.getFiringDirection(timer->getGameTime(), accuracy, highArc, direction,
                                               m_predictedPosition, &m_impactTime);
    m_lastAimVec = direction;
    m_predictedPosition.x = m_aimerTransform.m[12] + m_predictedPosition.x;
    m_predictedPosition.y = m_aimerTransform.m[13] + m_predictedPosition.y;
    m_predictedPosition.z = m_aimerTransform.m[14] + m_predictedPosition.z;
    return m_aimVecValid;
}

} // namespace ai
} // namespace hfe
} // namespace dice
