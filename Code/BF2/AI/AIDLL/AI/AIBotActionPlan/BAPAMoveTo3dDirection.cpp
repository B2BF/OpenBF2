// Three-dimensional direction move recovered from libaidll.so.
// static init: ELF 0x1D7900 (__tcf_0 0x1D7960, _GLOBAL__I 0x1D7990): <iostream>'s Init object only.
#include <iostream>
#include "BAPAMoveTo3dDirection.h"
#include "BAPActionInline.h"
#include "../AIBot/BotMain.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D71F0 (C1; C2 0x1D7080)
BAPAMoveTo3dDirection::BAPAMoveTo3dDirection(const Vec3& target, float maxSpeed,
    float maxArriveSpeed, const Vec3& direction, BAPCondition* condition,
    unsigned long long controls, float altitude, int patternId)
    : BAPAction(UninitializedPatternId()), m_maxSpeed(maxSpeed), m_maxArriveSpeed(maxArriveSpeed),
      m_altitude(altitude), m_vector24(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_target(target), m_direction(direction), m_forward(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_up(bapinline::vec3(0.0f, 0.0f, 0.0f)), m_right(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_endCondition(condition), m_vector70(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_vector7C(bapinline::vec3(0.0f, 0.0f, 0.0f)), m_state(0), m_initialize(false)
{
    m_patternId = patternId;
    m_controls = controls;
    if (condition)
        condition->setOwner(this);
    m_forward = bapinline::vec3(1.0f, 0.0f, 0.0f);
    m_right = bapinline::vec3(0.0f, 0.0f, 1.0f);
    m_up = bapinline::vec3(0.0f, 1.0f, 0.0f);
    m_vector24 = bapinline::vec3(0.0f, 0.0f, 0.0f);
}

// ELF 0x1D74A0 (C1; C2 0x1D7360)
BAPAMoveTo3dDirection::BAPAMoveTo3dDirection(float distance, float maxSpeed,
    float maxArriveSpeed, BAPCondition* condition, unsigned long long controls,
    float altitude, int patternId)
    : BAPAction(UninitializedPatternId()), m_maxSpeed(maxSpeed), m_maxArriveSpeed(maxArriveSpeed),
      m_altitude(altitude), m_vector24(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_target(bapinline::vec3(0.0f, 0.0f, 0.0f)), m_direction(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_forward(bapinline::vec3(0.0f, 0.0f, 0.0f)), m_up(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_right(bapinline::vec3(0.0f, 0.0f, 0.0f)), m_endCondition(condition),
      m_vector70(bapinline::vec3(0.0f, 0.0f, 0.0f)), m_vector7C(bapinline::vec3(0.0f, 0.0f, 0.0f)),
      m_state(0), m_distance(distance), m_initialize(true)
{
    m_patternId = patternId;
    m_controls = controls;
    if (condition)
        condition->setOwner(this);
    m_forward = bapinline::vec3(1.0f, 0.0f, 0.0f);
    m_right = bapinline::vec3(0.0f, 0.0f, 1.0f);
    m_up = bapinline::vec3(0.0f, 1.0f, 0.0f);
}

// ELF 0x1D7640 (D1; D2 0x1D75E0, D0 0x1D76A0)
BAPAMoveTo3dDirection::~BAPAMoveTo3dDirection()
{
    delete m_endCondition;
}

// ELF 0x1D7710
bool BAPAMoveTo3dDirection::execute(Bot* bot, bool& success, bool& finished)
{
    success = false;
    finished = true;
    if (m_initialize) {
        m_initialize = false;
        Information* self = bapinline::info(bot->getCurrentEquipment());
        const Mat4& transform = self->getTransformation();
        m_target = bapinline::vec3(m_distance * transform.m[8] + transform.m[12],
            m_distance * transform.m[9] + transform.m[13],
            m_distance * transform.m[10] + transform.m[14]);
        m_target.y = m_altitude;
        Vec3 direction = bapinline::vec3(transform.m[8], 0.0f, transform.m[10]);
        bapinline::normalize(direction);
        m_direction = direction;
    }
    return false;
}

// ELF 0x1D78E0
unsigned long long BAPAMoveTo3dDirection::getControls()
{
    return m_controls;
}

// ELF 0x1D79F0
BAPStatementType BAPAMoveTo3dDirection::getId() const
{
    return BAPSTMoveToDirection;
}

// ELF 0x1D78F0
bool BAPAMoveTo3dDirection::hasDefault()
{
    return false;
}

// ELF 0x1D79B0
bool BAPAMoveTo3dDirection::isFinished(Bot* bot)
{
    return m_endCondition && m_endCondition->evaluate(bot);
}

} // namespace ai
} // namespace hfe
} // namespace dice
