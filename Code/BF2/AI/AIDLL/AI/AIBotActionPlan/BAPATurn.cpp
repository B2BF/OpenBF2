// Turn statement base. Recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

#include "BAPATurn.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1E22B0, __tcf_0 0x1E2310, _GLOBAL__I 0x1E2340).
#include <iostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E2140 (C1; C2 0x1E20C0). The inline BAPStatement base clears +0x04/+0x08 and counts
// `news`; then +0x14 = cond, +0x18 = speed, +0x1C/+0x20 and +0x28..+0x30 = 0 (+0x24 is left),
// +0x08 = controls, +0x10 = tag. The condition's owner is set by the derived constructors.
BAPATurn::BAPATurn(float turnSpeed, BAPCondition* endCondition, unsigned long long controls, int tag)
{
    m_endCondition = endCondition;
    m_turnSpeed = turnSpeed;
    m_float1C = 0.0f;
    m_float20 = 0.0f;
    m_vec28.x = 0.0f;
    m_vec28.y = 0.0f;
    m_vec28.z = 0.0f;
    m_controls = controls;
    m_patternId = tag;
}

// ELF 0x1E2200 (D1; D2 0x1E21C0, D0 0x1E2240): only the inline base destructor (`destroys`).
BAPATurn::~BAPATurn()
{
}

// ELF 0x1E2290: a turn never runs by itself (the entries do the work): ok = false, failed = true.
bool BAPATurn::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1E2360 (weak)
BAPStatementType BAPATurn::getId() const
{
    return BAPSTTurn;   // 24
}

} // namespace ai
} // namespace hfe
} // namespace dice
