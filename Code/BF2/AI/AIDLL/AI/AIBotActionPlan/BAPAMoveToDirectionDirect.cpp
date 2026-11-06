// Direct direction move recovered from libaidll.so.
// static init: ELF 0x1D8E30 (__tcf_0 0x1D8E90, _GLOBAL__I 0x1D8EC0): <iostream>'s Init object only.
#include <iostream>
#include "BAPAMoveToDirectionDirect.h"
#include "BAPCondition.h"
#include "BAPActionInline.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D8A30 (C1; C2 0x1D8860)
BAPAMoveToDirectionDirect::BAPAMoveToDirectionDirect(Bot*, Vec3& point, Vec3& direction,
    float maxSpeed, float maxArriveSpeed, BAPCondition* condition, BAPCondition* endCondition,
    unsigned long long controls, int patternId)
    : BAPAMoveToDirection()
{
    m_firstPoint.x = 0.0f;
    m_firstPoint.y = 0.0f;
    m_condition64 = condition;
    condition->setOwner(this);
    m_patternId = patternId;
    m_point = point;
    m_maxSpeed = maxSpeed;
    m_maxArriveSpeed = maxArriveSpeed;
    m_endCondition = endCondition;
    if (endCondition)
        endCondition->setOwner(this);
    m_state68.x = 0.0f;
    m_flag74 = true;
    m_direction = direction;
    bapinline::normalize(m_direction);
    vec3ToVec2XZ(point, m_firstPoint);
    m_controls = controls;
}

// ELF 0x1D8CA0 (D1; D2 0x1D8C00, D0 0x1D8D40)
BAPAMoveToDirectionDirect::~BAPAMoveToDirectionDirect()
{
    delete m_endCondition;
    delete m_condition64;
}

// ELF 0x1D8DE0
bool BAPAMoveToDirectionDirect::execute(Bot*, bool& success, bool& finished)
{
    success = false;
    finished = true;
    return false;
}

// ELF 0x1D8E00
Vec2* BAPAMoveToDirectionDirect::getFirstPoint()
{
    return &m_firstPoint;
}

// ELF 0x1D8E10
Vec2* BAPAMoveToDirectionDirect::getSecondPoint(bool& flag)
{
    flag = m_flag74;
    return &m_firstPoint;
}

} // namespace ai
} // namespace hfe
} // namespace dice
