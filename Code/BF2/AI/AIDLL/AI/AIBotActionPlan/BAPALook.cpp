// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D2D10 (__tcf_0 0x1D2D70, _GLOBAL__I 0x1D2DA0): <iostream>'s Init object only.
#include <iostream>

#include "BAPALook.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D2BA0 (C1; C2 0x1D2B20 identical). The inline BAPStatement base clears +0x04/+0x08 and
// counts `news`; then +0x14 = cond, +0x18 = speed, +0x1C..+0x2C = 0, +0x08 = controls, +0x10 = tag.
// The condition's owner is set by the derived constructors, not here.
BAPALook::BAPALook(float lookSpeed, BAPCondition* endCondition, unsigned long long controls, int tag)
{
    m_endCondition = endCondition;
    m_lookSpeed = lookSpeed;
    m_deviation = 0.0f;
    m_lastControl = 0.0f;
    m_cameraForward.x = 0.0f;
    m_cameraForward.y = 0.0f;
    m_cameraForward.z = 0.0f;
    m_controls = controls;
    m_patternId = tag;
}

// ELF 0x1D2C60 (D1; D2 0x1D2C20, D0 0x1D2CA0): only the inline base destructor (`destroys`).
BAPALook::~BAPALook()
{
}

// ELF 0x1D2CF0: a look never runs by itself (the entries do the work): ok = false, failed = true.
bool BAPALook::execute(Bot*, bool& ok, bool& failed)
{
    ok = false;
    failed = true;
    return false;
}

// ELF 0x1D2DC0
BAPStatementType BAPALook::getId() const
{
    return BAPSTLookAt;   // 23
}

// ELF 0x1D2DD0
bool BAPALook::hasDefault()
{
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
