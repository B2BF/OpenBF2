// BAPConNot: recovered by package A-core from libaidll.so (TU without a FILE symbol,
// 0x1F4580..0x1F4D30). Every body carries its ELF address.

#include "BAPConNot.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F4580 (C2; C1 0x1F46E0). No null check.
BAPConNot::BAPConNot(BAPCondition* condition)
{
    condition->m_owner = this;
    m_conditions.insert(m_conditions.end(), condition);
}

// ELF 0x1F4780 (C2; C1 0x1F4850). Only the list's front is taken (owner set, appended).
BAPConNot::BAPConNot(std::list<BAPCondition*>& conditions)
{
    m_conditions.clear();
    conditions.front()->m_owner = this;
    m_conditions.insert(m_conditions.end(), conditions.front());
}

// ELF 0x1F4920 (C2; C1 0x1F4990).
BAPConNot::BAPConNot()
{
    m_conditions.clear();
}

// ELF 0x1F4A00 (D2; D1 0x1F4AB0, D0 0x1F4B60). Deletes the conditions (slot +0x10).
BAPConNot::~BAPConNot()
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// ELF 0x1F4C20: front() without an empty check (retail).
bool BAPConNot::evaluate(Bot* bot)
{
    return !m_conditions.front()->evaluate(bot);
}

// ELF 0x1F4C50: ignored once a condition is present.
void BAPConNot::addCondition(BAPCondition* condition)
{
    if (!m_conditions.empty())
        return;
    condition->m_owner = this;
    m_conditions.insert(m_conditions.end(), condition);
}

// ELF 0x1F4CC0.
BAPConditionType BAPConNot::getConditionType() const
{
    return BAPCTNot;
}

// ELF 0x1F4CD0.
void BAPConNot::streamSelf(std::ostream& os) const
{
    os << "NOT(" << *m_conditions.front() << ")";
}

} // namespace ai
} // namespace hfe
} // namespace dice
