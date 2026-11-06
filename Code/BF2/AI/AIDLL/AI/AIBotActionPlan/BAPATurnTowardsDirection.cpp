// Turn statement towards a fixed direction. Recovered from libaidll.so 1.5.3153 (ia-32); ELF
// addresses on every body.

#include "BAPATurnTowardsDirection.h"

#include "BAPCondition.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1E25D0, __tcf_0 0x1E2630, _GLOBAL__I 0x1E2660).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E2410 (C1; C2 0x1E2380): the base, the direction, then the end condition's owner (not
// null-checked).
BAPATurnTowardsDirection::BAPATurnTowardsDirection(const Vec3& direction, float turnSpeed, BAPCondition* endCondition,
                                                   unsigned long long controls, int tag)
    : BAPATurn(turnSpeed, endCondition, controls, tag)
{
    m_direction = direction;
    m_endCondition->setOwner(this);
}

// ELF 0x1E24F0 (D1; D2 0x1E24A0, D0 0x1E2540): deletes the end condition.
BAPATurnTowardsDirection::~BAPATurnTowardsDirection()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1E25A0: always valid.
Vec3 BAPATurnTowardsDirection::getDirection(bool& ok)
{
    ok = true;
    return m_direction;
}

} // namespace ai
} // namespace hfe
} // namespace dice
