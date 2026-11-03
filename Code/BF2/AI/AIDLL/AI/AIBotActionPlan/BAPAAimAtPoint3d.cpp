// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.

// static init: ELF 0x1D0D50 (__tcf_0 0x1D0DB0, _GLOBAL__I 0x1D0DE0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAAimAtPoint3d.h"

#include "../../../AIInterface/BFKnowledge/BFPhysics.h"
#include "BAPActionInline.h"
#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D0870 (C1; C2 0x1D0770). The condition's owner is set without a null test, as compiled.
BAPAAimAtPoint3d::BAPAAimAtPoint3d(Vec3& point, Vec3& offset, BAPCondition* condition, unsigned long long controls,
                                   int patternId, float float1A4, float float1A8)
{
    m_point = bapinline::vec3(offset.x + point.x, offset.y + point.y, offset.z + point.z);
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

// ELF 0x1D09F0 (D1 0x1D0970, D0 0x1D0A70)
BAPAAimAtPoint3d::~BAPAAimAtPoint3d()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1D0E00
int BAPAAimAtPoint3d::getAimAtType()
{
    return 0;
}

// ELF 0x1D0D00
bool BAPAAimAtPoint3d::getVectorToTarget(const Vec3& from, Vec3& vector) const
{
    vector = bapinline::sub(m_point, from);
    return true;
}

// ELF 0x1D0AF0. A resting target: the relative position is the point (plus the offset once more)
// from the transformation's position; unlike the object aim, the transformation is not stored, so
// the predicted position is offset by the previous aim's weapon position.
bool BAPAAimAtPoint3d::getAimVec(const Mat4& transform, const Vec3& shooterVelocity, Vec3& direction,
                                 float accuracy, float drag, float speed, float gravityScale, bool highArc)
{
    Vec3 relative = bapinline::vec3(0.0f, 0.0f, 0.0f);
    const Vec3 targetVelocity = bapinline::vec3(0.0f, 0.0f, 0.0f);   // stored, never read
    (void)targetVelocity;
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    m_aimer.m_drag = drag;
    m_aimer.m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer.m_speed = speed;
    m_aimer.m_gravity = static_cast<IAIPhysics*>(CommonSingletonServer::m_iaiPhysics)->getGravity() * gravityScale;

    const Vec3 position = bapinline::vec3(transform.m[12], transform.m[13], transform.m[14]);
    const Vec3 toPoint = bapinline::sub(m_point, position);
    relative = bapinline::add(toPoint, m_offset);
    const Vec3 resting = bapinline::vec3(0.0f, 0.0f, 0.0f);
    m_aimer.setTargetData(relative, resting, shooterVelocity, &transform, timer->getGameTime());
    m_aimVecValid = m_aimer.getFiringDirection(timer->getGameTime(), accuracy, highArc, m_lastAimVec,
                                               m_predictedPosition, &m_impactTime);
    direction = m_lastAimVec;
    m_predictedPosition.x = m_aimerTransform.m[12] + m_predictedPosition.x;
    m_predictedPosition.y = m_aimerTransform.m[13] + m_predictedPosition.y;
    m_predictedPosition.z = m_aimerTransform.m[14] + m_predictedPosition.z;
    return m_aimVecValid;
}

} // namespace ai
} // namespace hfe
} // namespace dice
