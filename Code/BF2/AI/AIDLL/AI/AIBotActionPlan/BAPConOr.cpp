// BAPConOr: recovered by package A-core from libaidll.so (TU without a FILE symbol, 0x1F4D30..0x1F5410).
// Every body carries its ELF address.

#include "BAPConOr.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1F4D30 (C2; C1 0x1F4EA0). Copies the list and takes ownership of its conditions.
BAPConOr::BAPConOr(std::list<BAPCondition*>& conditions)
{
    m_conditions = conditions;
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
        (*it)->mpOwner = this;
}

// ELF 0x1F4F50 (C2; C1 0x1F4FC0).
BAPConOr::BAPConOr()
{
    m_conditions.clear();
}

// ELF 0x1F5030 (D2; D1 and D0 follow). Deletes the conditions (slot +0x10); the list itself goes with
// BAPBoleanOp.
BAPConOr::~BAPConOr()
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if (*it)
            delete *it;
    }
}

// ELF 0x1F5250.
bool BAPConOr::evaluate(Bot* bot)
{
    for (std::list<BAPCondition*>::iterator it = m_conditions.begin(); it != m_conditions.end(); ++it)
    {
        if ((*it)->evaluate(bot))
            return true;
    }
    return false;
}

// ELF 0x1F52C0: no null check.
void BAPConOr::addCondition(BAPCondition* condition)
{
    condition->mpOwner = this;
    m_conditions.insert(m_conditions.end(), condition);
}

// ELF 0x1F5330.
BAPConditionType BAPConOr::getConditionType() const
{
    return BAPCTOr;
}

// ELF 0x1F5340: "(" c1 " || " c2 ... ")".
void BAPConOr::streamSelf(std::ostream& os) const
{
    os << "(";
    std::list<BAPCondition*>::const_iterator it = m_conditions.begin();
    if (it != m_conditions.end())
    {
        os << **it;
        for (++it; it != m_conditions.end(); ++it)
            os << " || " << **it;
    }
    os << ")";
}

} // namespace ai
} // namespace hfe
} // namespace dice
