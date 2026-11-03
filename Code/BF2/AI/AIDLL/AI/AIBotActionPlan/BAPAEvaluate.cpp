// Expression statement; neither shipped image constructs an expression object.

#include "BAPAEvaluate.h"

#include "BAPExpression.h"

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1D1CF0 (C1; C2 0x1D1C90).
BAPAEvaluate::BAPAEvaluate(BAPExpression* expression)
    : BAPStatement(UninitializedPatternId{}), m_expression(expression)
{
}

// ELF 0x1D1DB0 (D1; D2 0x1D1D50, D0 0x1D1E10).
BAPAEvaluate::~BAPAEvaluate()
{
    if (m_expression)
        delete m_expression;
}

// ELF 0x1D1E80
bool BAPAEvaluate::execute(Bot*, bool& ok, bool& done)
{
    m_expression->unimplementedExpressionSlot2();
    ok = true;
    done = true;
    return true;
}

// ELF 0x1D1EB0
BAPStatementType BAPAEvaluate::getId() const
{
    return BAPSTEvaluate;
}

// ELF 0x1D1EC0
bool BAPAEvaluate::hasDefault()
{
    return true;
}

} // namespace ai
} // namespace hfe
} // namespace dice
