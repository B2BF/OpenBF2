// Revive action: crouches and presses fire once per two executions (the shock paddles).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAREVIVE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAREVIVE_H

#include "BAPStatement.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B8940 (8 words), operator new 0x18/0x2C, package F-infantry.
class BAPARevive : public BAPStatement
{
public:
    explicit BAPARevive(int);   // 0x1DE8B0
    virtual ~BAPARevive();   // 0x1DE940

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DE9D0, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DEAE0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DEAF0, slot +0x14; from the base declaration

private:
    /* +0x14 */ bool m_firePressed;   // [INFERRED] name: set by the execute that presses fire
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAREVIVE_H
