// Sensing statement with two required, owned conditions; direct execution refuses the action.

#include "BAPASense.h"

#include "BAPCondition.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1DFFE0 (C1; C2 0x1DFF70)
BAPASense::BAPASense(BAPCondition* first, BAPCondition* second, int tag)
    : BAPAction(UninitializedPatternId()), m_firstCondition(first), m_secondCondition(second)
{
    m_firstCondition->setOwner(this);
    m_secondCondition->setOwner(this);
    m_patternId = tag;
}

// ELF 0x1E00C0 (D1; D2 0x1E0050, D0 0x1E0130)
BAPASense::~BAPASense()
{
    delete m_firstCondition;
    delete m_secondCondition;
}

// ELF 0x1E01B0
bool BAPASense::execute(Bot*, bool& success, bool& finished)
{
    success = false;
    finished = true;
    return false;
}

// ELF 0x1E01D0
unsigned long long BAPASense::getControls()
{
    return 0;
}

// ELF 0x1E01E0
BAPStatementType BAPASense::getId() const
{
    return BAPSTSense;
}

// ELF 0x1E01F0
bool BAPASense::hasDefault()
{
    return false;
}

} // namespace ai
} // namespace hfe
} // namespace dice
