// Direct object-move point source recovered from libaidll.so.
// static init: ELF 0x1DAF30 (__tcf_0 0x1DAF90, _GLOBAL__I 0x1DAFC0): <iostream>'s Init object only.
#include <iostream>
#include "BAPAMoveToObjectDirect.h"
#include "BAPActionInline.h"
#include "../AIKnowledge/Information.h"
#include "../AIKnowledge/IPIMobile.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DACE0 (C1; C2 0x1DAC20)
BAPAMoveToObjectDirect::BAPAMoveToObjectDirect(Bot* bot, unsigned int object,
    float distance, float maxSpeed, float maxArriveSpeed, BAPCondition* endCondition,
    unsigned long long controls, int patternId)
    : BAPAMoveToObject(bot, object, distance, maxSpeed, maxArriveSpeed, endCondition,
        controls, patternId)
{
    m_isMobile = bapinline::info(object)->getMobile() != 0;
}

// ELF 0x1DADE0 (D1; D2 0x1DADA0, D0 0x1DAE20)
BAPAMoveToObjectDirect::~BAPAMoveToObjectDirect()
{
}

// ELF 0x1DAE70
Vec2& BAPAMoveToObjectDirect::getCurrentPoint()
{
    Information* object = bapinline::info(m_object);
    if (object) {
        if (m_isMobile) {
            Vec3 position = bapinline::vec3(0.0f, 0.0f, 0.0f);
            object->getMobile()->getPositionEstimate(position);
            vec3ToVec2XZ(position, m_currentPoint);
        } else {
            vec3ToVec2XZ(object->getPosition(), m_currentPoint);
        }
    }
    return m_currentPoint;
}

// ELF 0x1DB020
int BAPAMoveToObjectDirect::usesPathfinding()
{
    return 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
