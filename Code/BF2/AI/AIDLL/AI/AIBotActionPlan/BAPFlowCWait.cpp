// BAPFlowCWait: recovered by package A-core from libaidll.so (TU without a FILE symbol, beside
// BAPFlowCIf.cpp; 0x1E3F80..0x1E4200). Every body carries its ELF address.

#include "BAPFlowCWait.h"

#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E3F80 (C2; C1 0x1E3FF0). The condition's owner is set unconditionally.
BAPFlowCWait::BAPFlowCWait(BAPCondition* condition, int patternId)
    : BAPFlowControl(patternId)
    , m_condition(condition)
{
    condition->m_owner = this;
}

// ELF 0x1E4060 (D2; D1 0x1E40C0, D0 0x1E4120).
BAPFlowCWait::~BAPFlowCWait()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1E4190.
bool BAPFlowCWait::execute(Bot* bot, bool& ok, bool& finished)
{
    finished = m_condition->evaluate(bot);
    ok = true;
    return true;
}

// ELF 0x1E41D0.
unsigned long long BAPFlowCWait::getControls()
{
    return 0;
}

// ELF 0x1E41F0.
BAPStatementType BAPFlowCWait::getId() const
{
    return BAPSTWait;
}

// ELF 0x1E41E0.
bool BAPFlowCWait::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
