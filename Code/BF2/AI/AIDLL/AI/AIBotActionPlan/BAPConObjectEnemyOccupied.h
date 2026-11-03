// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTENEMYOCCUPIED_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTENEMYOCCUPIED_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9670 (5 words), operator new 0xC, package F-infantry.
// True when vehicleOccupied(object, bot side) and the object's side differs from the bot's.
class BAPConObjectEnemyOccupied : public BAPConObject
{
public:
    explicit BAPConObjectEnemyOccupied(unsigned int);   // 0x1EDFF0
    virtual ~BAPConObjectEnemyOccupied();   // 0x1EE0F0

    virtual bool evaluate(Bot*);   // 0x1EE020, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EE150, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTENEMYOCCUPIED_H
