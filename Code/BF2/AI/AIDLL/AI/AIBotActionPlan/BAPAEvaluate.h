// Owns an expression, invokes its pure operation and finishes successfully.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAEVALUATE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAEVALUATE_H

#include "BAPStatement.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPExpression;
class Bot;

// ELF: vtable 0x4B7B40 (8 words), package H-deferred.
class BAPAEvaluate : public BAPStatement
{
public:
    explicit BAPAEvaluate(BAPExpression*);   // 0x1D1CF0
    virtual ~BAPAEvaluate();   // 0x1D1DB0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D1E80, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D1EB0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D1EC0, slot +0x14; from the base declaration

private:
    /* +0x14 */ BAPExpression* m_expression; // owned; constructor leaves the inherited pattern id unwritten
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAEVALUATE_H
