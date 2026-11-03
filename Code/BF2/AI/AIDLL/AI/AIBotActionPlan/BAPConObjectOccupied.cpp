// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1EFF70 (__tcf_0 0x1EFFD0, _GLOBAL__I 0x1F0000): <iostream>'s Init object only.
#include <iostream>

#include "BAPConObjectOccupied.h"

#include "BAPActionInline.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIKnowledge/Information.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1EFE40 (C1; C2 0x1EFE10): +4 = 0, +8 = the handle.
BAPConObjectOccupied::BAPConObjectOccupied(unsigned int object)
{
    setOwner(0);
    m_object = object;
}

// ELF 0x1EFF00 (D0 0x1EFF20): only the base vtable store.
BAPConObjectOccupied::~BAPConObjectOccupied()
{
}

// ELF 0x1EFE70: false without the information or its unit plug-in (+0x40).
bool BAPConObjectOccupied::evaluate(Bot*)
{
    Information* info = bapinline::info(m_object);
    if (info && info->getUnit())
        return info->getUnit()->isOccupied();
    return false;
}

// ELF 0x1EFF60
BAPConditionType BAPConObjectOccupied::getConditionType() const
{
    return BAPCTObjectOccupied;   // 29
}

} // namespace ai
} // namespace hfe
} // namespace dice
