// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONINSIDEBATTLEZONE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONINSIDEBATTLEZONE_H

#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B91B8 (5 words), operator new 0xC/0x7C, package G-vehicles.
// True while the bot's player is inside the combat area (IAITerrain::isInsideCombatArea). The
// constructor's distance is stored but not read.
class BAPConInsideBattleZone : public BAPCondition
{
public:
    explicit BAPConInsideBattleZone(float);   // 0x1E72D0
    virtual ~BAPConInsideBattleZone();   // 0x1E7410

    virtual bool evaluate(Bot*);   // 0x1E7300, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E7350, slot +0x04; from the base declaration

private:
    /* +0x08 */ float m_distance;   // [INFERRED] name: callers pass 150 and 200
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONINSIDEBATTLEZONE_H
