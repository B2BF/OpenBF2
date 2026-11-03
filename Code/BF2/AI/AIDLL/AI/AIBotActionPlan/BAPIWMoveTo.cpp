// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1F79A0 (__tcf_0 0x1F7A00, _GLOBAL__I 0x1F7A30): <iostream>'s Init object only.
#include <iostream>

#include "BAPIWMoveTo.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F7820 (C1; C2 0x1F77A0 region)
BAPIWMoveTo::BAPIWMoveTo(const Vec3& position, BAPStatement* statement, int tag)
    : BAPInfoWrapper(statement, tag)
{
    m_position = position;
    m_statement->m_owner = this;
}

// ELF 0x1F78F0 (D0 0x1F7940): deletes the child.
BAPIWMoveTo::~BAPIWMoveTo()
{
    if (m_statement)
        delete m_statement;
}

// ELF 0x1F7A50: "MoveTo"
int BAPIWMoveTo::getInfoType() const
{
    return 2;
}

} // namespace ai
} // namespace hfe
} // namespace dice
