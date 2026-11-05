// Ballistic arc helper. [INFERRED] Source placement.

// static init: ELF 0x2EA8B0 (__tcf_0 0x2EA910, _GLOBAL__I 0x2EA940): <iostream>'s Init object only.
#include <iostream>

#include "AimerHelper.h"

#include "checkPointer.h"
#include "../AIBotActionPlan/Aimer.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2EA2D0 (C1; C2 0x2EA160).
AimerHelper::AimerHelper() : m_aimer(checkPointer(new Aimer()))
{
}

// ELF 0x2EA4A0 (D1; D2 0x2EA440, D0 0x2EA500).
AimerHelper::~AimerHelper()
{
    if (m_aimer)
        delete m_aimer;
}

// ELF 0x2EA570 (amd-64 0x335D50).
bool AimerHelper::hasIndirectLineOfFire(float speed, const float& drag, const float& gravity,
    const Vec3& relativePosition, const Vec3& origin, Vec3& predictedTarget, Vec3& velocity, Vec3& apex)
{
    m_aimer->m_speed = speed;
    m_aimer->m_drag = drag;
    m_aimer->m_inverseDrag = drag != 0.0f ? 1.0f / drag : 0.0f;
    m_aimer->m_gravity = gravity;
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    static Vec3 zero = {0.0f, 0.0f, 0.0f};
    const Vec3 shooterVelocity = {0.0f, 0.0f, 0.0f};
    m_aimer->setTargetData(relativePosition, zero, shooterVelocity, 0, now);
    if (!m_aimer->getFiringDirection(now, 0.5f, true, velocity, predictedTarget, 0))
        return false;
    const float lengthSqr = velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
    if (!(std::fabs(lengthSqr - 1.0f) < 1.1920929e-07f))
    {
        if (std::fabs(lengthSqr) < 1.1920929e-07f)
            velocity.x = velocity.y = velocity.z = 0.0f;
        else
        {
            const float inverse = 1.0f / std::sqrt(lengthSqr);
            velocity.x *= inverse;
            velocity.y *= inverse;
            velocity.z *= inverse;
        }
    }
    velocity.x *= speed;
    velocity.y *= speed;
    velocity.z *= speed;
    const double extreme = m_aimer->getVerticalExtreme();
    const float time = static_cast<float>(extreme);
    const float halfTimeSquared = static_cast<float>(extreme * extreme * 0.5);
    const Vec3 displacement = {time * velocity.x, time * velocity.y, time * velocity.z};
    const Vec3 position = {displacement.x + origin.x, displacement.y + origin.y, displacement.z + origin.z};
    // Preserve the zero acceleration terms as well: zero times an infinite time is NaN.
    const Vec3 fall = {halfTimeSquared * 0.0f, halfTimeSquared * gravity, halfTimeSquared * 0.0f};
    const Vec3 result = {position.x + fall.x, position.y + fall.y, position.z + fall.z};
    apex = result;
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
