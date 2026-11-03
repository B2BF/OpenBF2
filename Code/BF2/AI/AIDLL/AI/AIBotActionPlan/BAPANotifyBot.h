// Adds a plan-variable amount to a sensed object's notification word for type zero.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPANOTIFYBOT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPANOTIFYBOT_H

#include "BAPAction.h"
#include "BAPTypes.h"
#include "BAPVariable.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B8780 (8 words), package G-vehicles.
class BAPANotifyBot : public BAPAction
{
public:
    enum Type : int;   // [UNRESOLVED] enumerators
    BAPANotifyBot(BAPANotifyBot::Type, BAPVariable<int>*, BAPVariable<unsigned int>*);   // 0x1DD6D0
    virtual ~BAPANotifyBot();   // 0x1DD770

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DD800, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DD950, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DD960, slot +0x14; from the base declaration
private:
    /* +0x14 */ Type m_type;
    /* +0x18 */ BAPVariable<int>* m_amount;
    /* +0x1C */ BAPVariable<unsigned int>* m_object;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPANOTIFYBOT_H
