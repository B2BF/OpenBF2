// A reported shot. Evidence: libaidll.so 1.5.3153 (ia-32), see FireObject.h.

#include "FireObject.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x2D8710, __tcf_0 0x2D8770, _GLOBAL__I 0x2D87A0).
#include <iostream>

#include <cmath>

#include "../AIUtils/checkPointer.h"
#include "../Main/AISettingsInterfaces.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x2D8090 (C1; C2 0x2D8280 is identical). Field stores first, then the strength count from
// IAISettings +0x34 (getNBattleStrengths), the checked array and the copy loop.
FireObject::FireObject(Vec3 origin, Vec3 target, float* strengths, bool direct, double time, float decay)
{
    m_direct = direct;
    m_origin = origin;
    m_target = target;
    m_time = time;
    m_decay = decay;
    IAISettings* const settings = static_cast<IAISettings*>(CommonSingletonServer::m_iaiSettings);
    const int count = settings->getNBattleStrengths();
    m_strengths = checkPointer(new float[count]);
    for (int i = 0; i < count; ++i)
        m_strengths[i] = strengths[i];
}

// ELF 0x2D8470: `if (+4) operator delete[](+4)`.
FireObject::~FireObject()
{
    if (m_strengths)
        delete[] m_strengths;
}

// ELF 0x2D84D0. The shot's XZ direction: getVec2NormalFromVec3XZ(target - origin), then the inline
// Vec2 normalisation (left alone within FLT_EPSILON of unit length, zeroed below FLT_EPSILON^2,
// else scaled by 1 / sqrt). The offset of `position` from the origin, projected to XZ, dotted with
// that normal is the distance from the line.
bool FireObject::isWithinRadius(const Vec3& position, float radius)
{
    Vec3 direction;
    direction.x = m_target.x - m_origin.x;
    direction.y = m_target.y - m_origin.y;
    direction.z = m_target.z - m_origin.z;
    Vec2 normal;
    normal.x = 0.0f;
    normal.y = 0.0f;
    getVec2NormalFromVec3XZ(direction, normal);
    const float lengthSq = normal.x * normal.x + normal.y * normal.y;
    if (std::fabs(1.0f - lengthSq) >= 1.1920929e-07f)
    {
        if (std::fabs(lengthSq) >= 1.4210855e-14f)
        {
            const float inverse = 1.0f / std::sqrt(lengthSq);
            normal.x = normal.x * inverse;
            normal.y = inverse * normal.y;
        }
        else
        {
            normal.x = 0.0f;
            normal.y = 0.0f;
        }
    }
    Vec3 offset;
    offset.x = position.x - m_origin.x;
    offset.y = position.y - m_origin.y;
    offset.z = position.z - m_origin.z;
    Vec2 offsetXZ;
    offsetXZ.x = 0.0f;
    offsetXZ.y = 0.0f;
    vec3ToVec2XZ(offset, offsetXZ);
    float distance = offsetXZ.x * normal.x + offsetXZ.y * normal.y;
    if (distance < 0.0f)
        distance = -distance;
    return distance <= radius;
}

// ELF 0x2D86B0: `flds (+4)[type]`; IBFTimer +8 (getGameTime) - +0x20, rounded to float, times
// +0x28, plus 1.0; divide.
float FireObject::getBattleStrength(int type)
{
    const float* const strengths = m_strengths;
    const IBFTimer* const timer = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer);
    const float age = static_cast<float>(timer->getGameTime() - m_time);
    return strengths[type] / (age * m_decay + 1.0f);
}

} // namespace ai
} // namespace hfe
} // namespace dice
