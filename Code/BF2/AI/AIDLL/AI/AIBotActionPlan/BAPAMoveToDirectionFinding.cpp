// Finding direction move recovered from libaidll.so.
// static init: ELF 0x1D95F0 (__tcf_0 0x1D9650, _GLOBAL__I 0x1D9680): <iostream>'s Init object only.
#include <iostream>
#include "BAPAMoveToDirectionFinding.h"
#include "BAPCondition.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D9170 (C1; C2 0x1D8F20)
BAPAMoveToDirectionFinding::BAPAMoveToDirectionFinding(Bot* bot, Vec3& point, Vec3& direction,
    float maxSpeed, float maxArriveSpeed, BAPCondition* condition, BAPCondition* endCondition,
    unsigned long long controls, int patternId)
    : BAPAMoveToDirection()
{
    m_firstPoint.x = m_firstPoint.y = 0.0f;
    m_secondPoint.x = m_secondPoint.y = 0.0f;
    Vec2 position = {0.0f, 0.0f};
    Information* self = bapinline::info(bot->getCurrentEquipment());
    m_condition64 = condition;
    condition->setOwner(this);
    m_patternId = patternId;
    m_point = point;
    m_maxSpeed = maxSpeed;
    m_maxArriveSpeed = maxArriveSpeed;
    m_endCondition = endCondition;
    if (endCondition)
        endCondition->setOwner(this);
    m_flag74 = true;
    m_state68.x = 0.0f;
    m_direction = direction;
    bapinline::normalize(m_direction);
    self->getMobile()->getVehicleNumber();
    // Shipped calls discard both results; neither stored path point is updated.
    vec3ToVec2XZ(self->getPosition(), position);
    m_controls = controls;
}

// ELF 0x1D9460 (D1; D2 0x1D93C0, D0 0x1D9500)
BAPAMoveToDirectionFinding::~BAPAMoveToDirectionFinding()
{
    delete m_endCondition;
    delete m_condition64;
}

// ELF 0x1D95A0
bool BAPAMoveToDirectionFinding::execute(Bot*, bool& success, bool& finished)
{
    success = false;
    finished = true;
    return false;
}

// ELF 0x1D95C0
Vec2* BAPAMoveToDirectionFinding::getFirstPoint()
{
    return &m_firstPoint;
}

// ELF 0x1D95D0
Vec2* BAPAMoveToDirectionFinding::getSecondPoint(bool& flag)
{
    flag = m_flag74;
    return &m_secondPoint;
}

} // namespace ai
} // namespace hfe
} // namespace dice
