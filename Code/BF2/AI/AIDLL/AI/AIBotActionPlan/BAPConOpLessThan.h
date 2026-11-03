// Comparison condition template; only <float> is instantiated (vtable 0x4BBC38, inline in the
// image, emitted in BBPFire2d.cpp). getConditionType 0x24B7E0 returns 55.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOPLESSTHAN_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOPLESSTHAN_H

#include <ostream>

#include "BAPCondition.h"
#include "BAPExpression.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {
namespace ai {

template <class T>
class BAPConOpLessThan : public BAPCondition
{
public:
    // [CHOSEN, not recovered] Constructor spelling for the inline construction at
    // ELF 0x246A44..0x246A7E: both optional expressions null, followed by the value pointers.
    BAPConOpLessThan(BAPVariable<T>* left, BAPVariable<T>* right)
        : m_leftExpression(0), m_left(left), m_rightExpression(0), m_right(right) {}

    // ELF 0x24B790 (<float>).
    virtual bool evaluate(Bot*)
    {
        if (m_leftExpression)
            m_leftExpression->unimplementedExpressionSlot2();
        if (m_rightExpression)
            m_rightExpression->unimplementedExpressionSlot2();
        return m_left->m_value < m_right->m_value;
    }
    virtual BAPConditionType getConditionType() const { return BAPCTOpLessThan; }                            // 0x24B7E0
    // ELF 0x24B7F0 (<float>): streams the cached values without invoking the expressions.
    virtual void streamSelf(std::ostream& out) const
    {
        out << m_left->m_value << " < " << m_right->m_value;
    }
    // ELF 0x24B850 (D1; D0 0x24B8B0).
    virtual ~BAPConOpLessThan()
    {
        if (m_leftExpression)
            delete m_leftExpression;
        if (m_rightExpression)
            delete m_rightExpression;
    }

private:
    // [INFERRED] Operand names; expressions are owned, variables belong to the context.
    /* +0x08 */ BAPExpression* m_leftExpression;
    /* +0x0C */ BAPVariable<T>* m_left;
    /* +0x10 */ BAPExpression* m_rightExpression;
    /* +0x14 */ BAPVariable<T>* m_right;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOPLESSTHAN_H
