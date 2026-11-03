// BAPConSharedCondition: recovered by package A-core from libaidll.so (TU without a FILE symbol,
// 0x1E9D80..0x1EA280). Every body carries its ELF address.

#include "BAPConSharedCondition.h"

#include "../AIUtils/checkPointer.h"

#include <ostream>

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1E9D80 (C2; C1 0x1E9DF0). The cell is unchecked `operator new(8)`; no null check on the
// condition.
BAPConSharedCondition::BAPConSharedCondition(BAPCondition* condition)
{
    m_shared = new SharedValue;
    m_shared->value = false;
    m_shared->references = 1;
    m_condition = condition;
    condition->m_owner = this;
}

// ELF 0x1E9E60 (C2; C1 0x1E9EA0): shares the cell, owns no condition.
BAPConSharedCondition::BAPConSharedCondition(const BAPConSharedCondition& other)
    : BAPCondition()
    , m_shared(other.m_shared)
    , m_condition(0)
{
    ++m_shared->references;
}

// ELF 0x1E9EE0 (D2; D1 0x1E9F50, D0 0x1E9FC0).
BAPConSharedCondition::~BAPConSharedCondition()
{
    if (m_condition)
        delete m_condition;
    if (--m_shared->references == 0)
        delete m_shared;
}

// ELF 0x1EA040.
BAPConSharedCondition* BAPConSharedCondition::getNewSharedCondition() const
{
    return checkPointer(new BAPConSharedCondition(*this));
}

// ELF 0x1EA190: only the original evaluates; every holder returns the cell.
bool BAPConSharedCondition::evaluate(Bot* bot)
{
    if (m_condition)
        m_shared->value = m_condition->evaluate(bot);
    return m_shared->value;
}

// ELF 0x1EA1D0.
BAPConditionType BAPConSharedCondition::getConditionType() const
{
    return BAPCTSharedCondition;
}

// ELF 0x1EA1E0: "[" condition "][" cell address "]"; a copy prints "[][" address "]".
void BAPConSharedCondition::streamSelf(std::ostream& os) const
{
    if (m_condition)
        os << "[" << *m_condition << "][" << static_cast<const void*>(m_shared) << "]";
    else
        os << "[" << "][" << static_cast<const void*>(m_shared) << "]";
}

} // namespace ai
} // namespace hfe
} // namespace dice
