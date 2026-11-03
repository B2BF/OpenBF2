// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1CF280 (__tcf_0 0x1CF2E0, _GLOBAL__I 0x1CF310): <iostream>'s Init object only.
#include <iostream>

#include "BAPAAimAtObject.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../AIBot/SensedObject.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/Information.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1CE650 (C1; C2 0x1CE3B0). The isPrimary/getPrimaryObject pairs on the bot's equipment and
// on the target are evaluated and their answers dropped, as in the image (neither information is
// null-checked there).
BAPAAimAtObject::BAPAAimAtObject(Bot* bot, unsigned int target, const Vec3& offset, BAPCondition* endCondition,
                                 unsigned long long controls, int patternId, bool useCenter,
                                 BAPVariable<bool>* overrideValid, BAPVariable<Vec3>* overrideAim)
    : BAPAAimAt(overrideValid, overrideAim)
{
    m_bot = bot;
    m_target = target;
    m_lastAim = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_predicted = bapinline::vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 16; ++i)
        m_aimerTransform.m[i] = 0.0f;
    m_aimerTransform.m[15] = 1.0f;
    m_aimerTransform.m[10] = 1.0f;
    m_aimerTransform.m[5] = 1.0f;
    m_aimerTransform.m[0] = 1.0f;
    m_valid = false;
    m_useCenter = useCenter;

    Information* self = bapinline::info(bot->getCurrentEquipment());
    if (!self->isPrimary())
        self->getPrimaryObject(bot->getSide());
    Information* info = bapinline::info(target);
    if (!info->isPrimary())
        info->getPrimaryObject(bot->getSide());

    m_offset = offset;
    m_endCondition = endCondition;
    endCondition->setOwner(this);
    m_patternId = patternId;
    m_int140 = 0;
    m_controls = controls;
    m_int148 = 0;
    m_double98 = -5000000000.0;
}

// ELF 0x1CE8F0 (D1; D0 0x1CE970): deletes the end condition (virtual destructor, slot +0x10).
BAPAAimAtObject::~BAPAAimAtObject()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1CF330
int BAPAAimAtObject::getAimAtType()
{
    return 1;
}

// ELF 0x1CEA00. With both override variables set their values win. Otherwise the target point is
// the sensed hit point (Bot slot +0x94 getSensedObjectFromHandle, +0x04, through the target's
// transformation) or, when the target is not sensed and m_useCenter is set, its position; not
// sensed without m_useCenter, or no target information: false. The Aimer gets the drag, speed and
// scaled gravity, the target relative to the weapon origin (plus the offset and the bot's fire
// correction) and the target's velocity (its own mobile, else its primary object's).
bool BAPAAimAtObject::getAimVec(const Mat4& origin, const Vec3& ownSpeed, Vec3& dir, float accuracy, float drag,
                                float speed, float gravityModifier, bool highArc, Bot* bot)
{
    if (m_overrideValid && m_overrideAim) {
        dir = m_overrideAim->m_value;
        m_lastAim = dir;
        m_valid = m_overrideValid->m_value;
        return m_overrideValid->m_value;
    }

    Vec3 rel = bapinline::vec3(0.0f, 0.0f, 0.0f);
    const Vec3& correction = correctAim(bot);
    Information* target = bapinline::info(m_target);
    if (!target)
        return false;
    m_bot->getCurrentEquipment();   // answer dropped (0x1CEB5x)
    m_aimerTransform = origin;

    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = speed;
    m_aimer.m_gravity = static_cast<float>(
        static_cast<double>(static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity()) * gravityModifier);

    Vec3 point;
    SensedObject* sensed = m_bot->getSensedObjectFromHandle(m_target);
    if (!sensed) {
        if (!m_useCenter)
            return false;
        point = target->getPosition();
    } else {
        const Mat4& t = target->getAIObject()->getTransformation();
        const Vec3& l = sensed->m_seenPosition;
        point.x = t.m[0] * l.x + t.m[4] * l.y + t.m[8] * l.z + t.m[12];     // 0x1CECE8..0x1CED09
        point.y = t.m[1] * l.x + t.m[5] * l.y + t.m[9] * l.z + t.m[13];
        point.z = l.x * t.m[2] + l.y * t.m[6] + l.z * t.m[10] + t.m[14];
    }

    const float ox = m_aimerTransform.m[12];
    const float oy = m_aimerTransform.m[13];
    const float oz = m_aimerTransform.m[14];
    rel.x = (point.x - ox) + m_offset.x + correction.x;
    rel.y = (point.y - oy) + m_offset.y + correction.y;
    rel.z = (point.z - oz) + m_offset.z + correction.z;

    Vec3 velocity;
    IPIMobile* mobile = target->getMobile();
    if (!mobile && !target->isPrimary()) {
        Information* primary = bapinline::info(target->getPrimaryObject(m_bot->getSide()));
        if (primary)
            mobile = primary->getMobile();
    }
    if (mobile)
        velocity = mobile->getSpeed();
    else
        velocity = bapinline::vec3(0.0f, 0.0f, 0.0f);

    m_aimer.setTargetData(rel, velocity, ownSpeed, &m_aimerTransform, bapinline::gameTime());
    m_valid = m_aimer.getFiringDirection(bapinline::gameTime(), accuracy, highArc, m_lastAim, m_predicted,
                                         &m_impactTime);
    // Back to world space; the correction comes off again.
    m_predicted.x = (m_aimerTransform.m[12] + m_predicted.x) - correction.x;
    m_predicted.y = (m_aimerTransform.m[13] + m_predicted.y) - correction.y;
    m_predicted.z = (m_aimerTransform.m[14] + m_predicted.z) - correction.z;
    dir = m_lastAim;
    return m_valid;
}

// ELF 0x1CF060: target position - from; false (out untouched) without the target information.
bool BAPAAimAtObject::getVectorToTarget(const Vec3& from, Vec3& out) const
{
    Information* target = bapinline::info(m_target);
    if (target)
        out = bapinline::sub(target->getPosition(), from);
    return target != 0;
}

// ELF 0x1CF1F0
const Mat4* BAPAAimAtObject::getAimerTransform() const
{
    return &m_aimerTransform;
}

// ELF 0x1CF130
bool BAPAAimAtObject::isAimingValid() const
{
    return m_valid;
}

// ELF 0x1CF140
bool BAPAAimAtObject::getLastAimVec(Vec3& out) const
{
    if (m_valid)
        out = m_lastAim;
    return m_valid;
}

// ELF 0x1CF180
bool BAPAAimAtObject::getPredictedPosition(Vec3& out) const
{
    if (m_valid)
        out = m_predicted;
    return m_valid;
}

// ELF 0x1CF1C0
bool BAPAAimAtObject::getImpactTime(double& out) const
{
    if (m_valid)
        out = m_impactTime;
    return m_valid;
}

// ELF 0x1CF200: the Aimer's impact position along the transform's row 2 (+0x194), made absolute.
bool BAPAAimAtObject::getImpactPosition(Vec3& out, double time) const
{
    Vec3 forward = bapinline::vec3(m_aimerTransform.m[8], m_aimerTransform.m[9], m_aimerTransform.m[10]);
    const bool result = m_aimer.getImpactPosition(out, forward, static_cast<float>(time));
    out.x = m_aimerTransform.m[12] + out.x;
    out.y = m_aimerTransform.m[13] + out.y;
    out.z = m_aimerTransform.m[14] + out.z;
    return result;
}

} // namespace ai
} // namespace hfe
} // namespace dice
