// Sensing statement with two required, owned conditions; direct execution refuses the action.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPASENSE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPASENSE_H

#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8AC0 (8 words), package H-deferred.
class BAPASense : public BAPAction
{
public:
    BAPASense(BAPCondition*, BAPCondition*, int);   // 0x1DFFE0
    virtual ~BAPASense();   // 0x1E00C0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E01B0, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1E01D0, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E01E0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1E01F0, slot +0x14; from the base declaration
private:
    // EntrySense reads these inline (ELF 0x2C3A5A / 0x2C3AA0).
    friend class EntrySense;
    /* +0x14 */ BAPCondition* m_firstCondition;
    /* +0x18 */ BAPCondition* m_secondCondition;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPASENSE_H
