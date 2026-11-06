// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D3C90 (__tcf_0 0x1D3CF0, _GLOBAL__I 0x1D3D20): <iostream>'s Init object only.
#include <iostream>

#include "BAPALookInDir.h"

#include "../AIBotActionPlan/BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D3B00 (C1; C2 0x1D3A70 identical): direction copied to +0x30, then the end condition's
// owner (+0x04) is pointed at this statement.
BAPALookInDir::BAPALookInDir(const Vec3& direction, float lookSpeed, BAPCondition* endCondition,
                             unsigned long long controls, int tag)
    : BAPALook(lookSpeed, endCondition, controls, tag)
{
    m_direction = direction;
    endCondition->setOwner(this);
}

// ELF 0x1D3BE0 (D1; D2 0x1D3B90, D0 0x1D3C30): deletes the end condition (virtual D0, slot +0x10).
BAPALookInDir::~BAPALookInDir()
{
    if (m_endCondition)
        delete m_endCondition;
}

// ELF 0x1D3D40: always valid, the stored direction.
Vec3 BAPALookInDir::getLookDirection(bool& ok)
{
    ok = true;
    return m_direction;
}

} // namespace ai
} // namespace hfe
} // namespace dice
