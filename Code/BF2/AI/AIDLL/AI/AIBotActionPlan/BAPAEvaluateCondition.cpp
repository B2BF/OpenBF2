// Package C (ai-w1-c-actions): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.

// static init: ELF 0x1D2220 (__tcf_0 0x1D2280, _GLOBAL__I 0x1D22B0): <iostream>'s Init object only.
#include <iostream>

#include "BAPAEvaluateCondition.h"

#include "BAPActionInline.h"
#include "BAPCondition.h"
#include "../Main/IBotManager.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D1F40 (C1)
BAPAEvaluateCondition::BAPAEvaluateCondition(BAPCondition* condition, int patternId)
{
    m_condition = condition;
    m_patternId = patternId;
    condition->setOwner(this);
}

// ELF 0x1D2020 (D0 0x1D2090): deletes the condition.
BAPAEvaluateCondition::~BAPAEvaluateCondition()
{
    if (m_condition)
        delete m_condition;
}

// ELF 0x1D2100
bool BAPAEvaluateCondition::execute(Bot* bot, bool& ok, bool& failed)
{
    m_result = m_condition->evaluate(bot);
    ok = true;
    failed = true;
    return true;
}

// ELF 0x1D2140: no controls.
unsigned long long BAPAEvaluateCondition::getControls()
{
    return 0;
}

// ELF 0x1D22D0
BAPStatementType BAPAEvaluateCondition::getId() const
{
    return BAPSTEvaluateCondition;   // 43
}

// ELF 0x1D2150
bool BAPAEvaluateCondition::hasDefault()
{
    return true;
}

// ELF 0x1D2160
void BAPAEvaluateCondition::streamSelf(std::ostream& out) const
{
    IBotManager::instance->tabulate(out);
    bapinline::streamType(out, getId()) << "(";
    out << (m_result ? "[true]" : "[false]");
    out << *m_condition;
    out << ")" << std::endl;
}

} // namespace ai
} // namespace hfe
} // namespace dice
