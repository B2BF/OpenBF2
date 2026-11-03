// BAPConAnd: recovered by package A-core from libaidll.so (TU without a FILE symbol, 0x1F3B10..0x1F41F0).
// Every body carries its ELF address.

#include "BAPConAnd.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F3B10 (C2; C1 0x1F3C80). Copies the list and takes ownership of its conditions.
BAPConAnd::BAPConAnd(std::list<BAPCondition*>& conditions)
{
    m_conditions = conditions;
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
        (*it)->m_owner = this;
}

// ELF 0x1F3D30 (C2; C1 0x1F3DA0).
BAPConAnd::BAPConAnd()
{
    m_conditions.clear();
}

// ELF 0x1F3E10 (D2; D1 and D0 follow). Deletes the conditions (slot +0x10); the list itself goes with
// BAPBoleanOp.
BAPConAnd::~BAPConAnd()
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// ELF 0x1F4030.
bool BAPConAnd::evaluate(Bot* bot)
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if (!(*it)->evaluate(bot))
            return false;
    }
    return true;
}

// ELF 0x1F40A0: no null check.
void BAPConAnd::addCondition(BAPCondition* condition)
{
    condition->m_owner = this;
    m_conditions.insert(m_conditions.end(), condition);
}

// ELF 0x1F4110.
BAPConditionType BAPConAnd::getConditionType() const
{
    return BAPCTAnd;
}

// ELF 0x1F4120: "(" c1 " && " c2 ... ")".
void BAPConAnd::streamSelf(std::ostream& os) const
{
    os << "(";
    std::list<BAPCondition*>::const_iterator it = m_conditions.begin();
    if (it != m_conditions.end())
    {
        os << **it;
        for (++it; it != m_conditions.end(); ++it)
            os << " && " << **it;
    }
    os << ")";
}

} // namespace ai
} // namespace hfe
} // namespace dice
