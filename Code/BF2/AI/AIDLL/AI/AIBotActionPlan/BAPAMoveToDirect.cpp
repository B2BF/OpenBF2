// Direct move statement (no pathfinding). Recovered from libaidll.so 1.5.3153 (ia-32); ELF
// addresses on every body.

#include "BAPAMoveToDirect.h"

#include "BAPCondition.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1D8330, __tcf_0 0x1D8390, _GLOBAL__I 0x1D83C0).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D8110 (C1; C2 0x1D8010): the base, +0xB4 = (0, 0), the arguments, the end condition's
// owner when there is one, +0x60 = 0, +0x5C = true, the goal in XZ, then the controls.
BAPAMoveToDirect::BAPAMoveToDirect(Bot* bot, const Vec3& coordinate, float maxSpeed, float maxArriveSpeed,
                                   BAPCondition* endCondition, unsigned long long controls, int patternId)
    : BAPAMoveTo(bot)
{
    m_goal2d.x = 0.0f;
    m_goal2d.y = 0.0f;
    m_bot = bot;
    m_patternId = patternId;
    m_coordinate = coordinate;
    m_maxSpeed = maxSpeed;
    m_maxArriveSpeed = maxArriveSpeed;
    m_endCondition = endCondition;
    if (endCondition)
        endCondition->setOwner(this);
    m_previousSpeed = 0.0f;
    m_pathValid = true;
    vec3ToVec2XZ(coordinate, m_goal2d);
    m_controls = controls;
}

// ELF 0x1D8250 (D1; D2 0x1D8210, D0 0x1D8290, thunks 0x1D83E0 / 0x1D8400): base only.
BAPAMoveToDirect::~BAPAMoveToDirect()
{
}

// ELF 0x1D82E0: runs only through the move entries.
bool BAPAMoveToDirect::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1D8300: always the goal; neither flag is raised.
int BAPAMoveToDirect::update(Vec2& point, bool& reached, bool& done, float)
{
    point = m_goal2d;
    reached = false;
    done = false;
    return 1;
}

} // namespace ai
} // namespace hfe
} // namespace dice
