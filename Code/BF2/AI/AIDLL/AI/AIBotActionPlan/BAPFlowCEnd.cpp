// BAPFlowCEnd: recovered by package A-core from libaidll.so (TU without a FILE symbol, beside
// BAPFlowCIf.cpp; 0x1E3230..0x1E3550). Every body carries its ELF address.

#include "BAPFlowCEnd.h"

#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E3230 (C2; C1 0x1E32A0).
BAPFlowCEnd::BAPFlowCEnd(int patternId, BAPFlowCEnd::EndType type, BAPCondition* condition)
    : BAPFlowControl(patternId)
    , m_condition(condition)
    , m_type(type)
{
    if (condition)
        condition->m_owner = this;
}

// ELF 0x1E3310 (D2; D1 0x1E3380, D0 0x1E33F0). The condition's deleting destructor is slot +0x10.
BAPFlowCEnd::~BAPFlowCEnd()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1E3460; the table is in the header.
bool BAPFlowCEnd::execute(Bot* bot, bool& ok, bool& finished)
{
    switch (static_cast<int>(m_type))
    {
    case 0:
        if (m_condition == 0 || m_condition->evaluate(bot))
        {
            finished = true;
            ok = false;
        }
        else
        {
            finished = false;
            ok = true;
        }
        break;
    case 1:
        if (m_condition == 0)
        {
            finished = true;
            ok = false;
        }
        else
        {
            finished = m_condition->evaluate(bot);
            ok = true;
        }
        break;
    case 2:
        if (m_condition == 0)
        {
            finished = false;
            ok = false;
        }
        else
        {
            finished = !m_condition->evaluate(bot);
            ok = true;
        }
        break;
    default:
        break;
    }
    return true;
}

// ELF 0x1E3520.
unsigned long long BAPFlowCEnd::getControls()
{
    return 0;
}

// ELF 0x1E3540.
BAPStatementType BAPFlowCEnd::getId() const
{
    return BAPSTPlanEnd;
}

// ELF 0x1E3530.
bool BAPFlowCEnd::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
