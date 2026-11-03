// Enables or disables physics on the bot's primary equipment when its current state differs.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAPHYSICSENABLE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAPHYSICSENABLE_H

#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B8800 (8 words), package H-deferred.
class BAPAPhysicsEnable : public BAPAction
{
public:
    explicit BAPAPhysicsEnable(bool);   // 0x1DDD00
    virtual ~BAPAPhysicsEnable();   // 0x1DDD90

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DDE20, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DDFD0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DDFE0, slot +0x14; from the base declaration
private:
    /* +0x14 */ bool m_enable;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAPHYSICSENABLE_H
