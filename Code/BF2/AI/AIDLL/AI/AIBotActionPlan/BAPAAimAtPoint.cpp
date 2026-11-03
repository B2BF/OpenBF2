// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D06B0 (__tcf_0 0x1D0710, _GLOBAL__I 0x1D0740): <iostream>'s Init object only.
#include <iostream>

#include "BAPAAimAtPoint.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../AIBot/BotMain.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFPhysics.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1CFE90 (C1; C2 0x1CFC50)
BAPAAimAtPoint::BAPAAimAtPoint(Bot* bot, BAPVariable<Vec3>* point, const Vec3& offset, BAPCondition* endCondition,
                               unsigned long long controls, int patternId, BAPVariable<bool>* overrideValid,
                               BAPVariable<Vec3>* overrideAim)
    : BAPAAimAt(overrideValid, overrideAim)
{
    m_point = point;
    m_lastAim = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_bot = bot;
    m_predicted = bapinline::vec3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 16; ++i)
        m_aimerTransform.m[i] = 0.0f;
    m_aimerTransform.m[15] = 1.0f;
    m_aimerTransform.m[10] = 1.0f;
    m_aimerTransform.m[5] = 1.0f;
    m_aimerTransform.m[0] = 1.0f;
    m_valid = false;
    m_offset = offset;
    m_endCondition = endCondition;
    endCondition->setOwner(this);
    m_patternId = patternId;
    m_double98 = -5000000000.0;
    m_controls = controls;
    m_int140 = 0;
    m_int148 = 0;
}

// ELF 0x1D0050 (D1; D0 0x1D00D0): deletes the end condition.
BAPAAimAtPoint::~BAPAAimAtPoint()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1D0760
int BAPAAimAtPoint::getAimAtType()
{
    return 0;
}

// ELF 0x1D0160. Like BAPAAimAtObject::getAimVec for the point variable, without the override
// variables, with a zero target velocity and the caller's origin matrix (not the copy) handed to
// setTargetData. The bot's equipment is read and dropped first, as in the image.
bool BAPAAimAtPoint::getAimVec(const Mat4& origin, const Vec3& ownSpeed, Vec3& dir, float accuracy, float drag,
                               float speed, float gravityModifier, bool highArc, Bot* bot)
{
    Vec3 rel = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_bot->getCurrentEquipment();   // answer dropped
    const Vec3& correction = correctAim(bot);
    m_aimerTransform = origin;

    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag == 0.0f ? 0.0f : 1.0f / drag;
    m_aimer.m_speed = speed;
    m_aimer.m_gravity = static_cast<float>(
        static_cast<double>(static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity()) * gravityModifier);

    const Vec3& p = m_point->m_value;
    rel.x = (p.x - m_aimerTransform.m[12]) + m_offset.x + correction.x;
    rel.y = (p.y - m_aimerTransform.m[13]) + m_offset.y + correction.y;
    rel.z = (p.z - m_aimerTransform.m[14]) + m_offset.z + correction.z;

    const double now = bapinline::gameTime();
    const Vec3 velocity = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_aimer.setTargetData(rel, velocity, ownSpeed, &origin, now);
    m_valid = m_aimer.getFiringDirection(bapinline::gameTime(), accuracy, highArc, m_lastAim, m_predicted,
                                         &m_impactTime);
    m_predicted.x = (m_aimerTransform.m[12] + m_predicted.x) - correction.x;
    m_predicted.y = (m_aimerTransform.m[13] + m_predicted.y) - correction.y;
    m_predicted.z = (m_aimerTransform.m[14] + m_predicted.z) - correction.z;
    dir = m_lastAim;
    return m_valid;
}

// ELF 0x1D04E0: always true.
bool BAPAAimAtPoint::getVectorToTarget(const Vec3& from, Vec3& out) const
{
    out = bapinline::sub(m_point->m_value, from);
    return true;
}

// ELF 0x1D0620
const Mat4* BAPAAimAtPoint::getAimerTransform() const
{
    return &m_aimerTransform;
}

// ELF 0x1D0560
bool BAPAAimAtPoint::isAimingValid() const
{
    return m_valid;
}

// ELF 0x1D0570
bool BAPAAimAtPoint::getLastAimVec(Vec3& out) const
{
    if (m_valid)
        out = m_lastAim;
    return m_valid;
}

// ELF 0x1D05B0
bool BAPAAimAtPoint::getPredictedPosition(Vec3& out) const
{
    if (m_valid)
        out = m_predicted;
    return m_valid;
}

// ELF 0x1D05F0
bool BAPAAimAtPoint::getImpactTime(double& out) const
{
    if (m_valid)
        out = m_impactTime;
    return m_valid;
}

// ELF 0x1D0630: as BAPAAimAtObject::getImpactPosition.
bool BAPAAimAtPoint::getImpactPosition(Vec3& out, double time) const
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
