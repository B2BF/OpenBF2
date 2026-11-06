// Owns a child statement and overrides its finished result after interpreter execution.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPWRAPPERFINISHED_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPWRAPPERFINISHED_H

#include <iosfwd>
#include "BAPStatement.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;

// ELF: vtable 0x4BA3C0 (8 words), package H-deferred.
class BAPWrapperFinished : public BAPStatement
{
public:
    BAPWrapperFinished(BAPStatement*, bool);   // 0x1F9F00
    virtual ~BAPWrapperFinished();   // 0x1F9FE0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1FA0C0, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1FA130, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1FA140, slot +0x14; from the base declaration
    virtual void streamSelf(std::ostream&) const;   // 0x1FA150, slot +0x1C; from the base declaration
private:
    /* +0x14 */ BAPStatement* m_statement;
    /* +0x18 */ bool m_finished;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPWRAPPERFINISHED_H
