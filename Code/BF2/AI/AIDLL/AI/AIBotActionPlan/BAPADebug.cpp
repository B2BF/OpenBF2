// A completed debug marker whose integer value appears in plan dumps.

// static init: ELF 0x1FB8F0 (__tcf_0 0x1FB950, _GLOBAL__I 0x1FB980): <iostream>'s Init object only.
#include <iostream>

#include "BAPADebug.h"

#include "../Main/IBotManager.h"
#include "../Main/AIStreamOperators.h"
#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1FB6F0 (C1; C2 0x1FB6A0)
BAPADebug::BAPADebug(int value) : BAPAction(UninitializedPatternId()), m_value(value)
{
}

// ELF 0x1FB780 (D1; D2 0x1FB740, D0 0x1FB7C0)
BAPADebug::~BAPADebug()
{
}

// ELF 0x1FB810
bool BAPADebug::execute(Bot*, bool& success, bool& finished)
{
    success = true;
    finished = true;
    return true;
}

// ELF 0x1FB9A0
unsigned long long BAPADebug::getControls()
{
    return m_controls;
}

// ELF 0x1FB9B0
BAPStatementType BAPADebug::getId() const
{
    return BAPSTDebug;
}

// ELF 0x1FB9C0
bool BAPADebug::hasDefault()
{
    return false;
}

// ELF 0x1FB830
void BAPADebug::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    out << getId() << "(" << m_value << ")" << std::endl;
}

} // namespace ai
} // namespace hfe
} // namespace dice
