// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTOCCUPIED_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTOCCUPIED_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9848 (5 words), operator new 0xC, package F-infantry.
// True when the object's unit plug-in reports it occupied (IPIUnit::isOccupied).
class BAPConObjectOccupied : public BAPConObject
{
public:
    explicit BAPConObjectOccupied(unsigned int);   // 0x1EFE40
    virtual ~BAPConObjectOccupied();   // 0x1EFF00

    virtual bool evaluate(Bot*);   // 0x1EFE70, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EFF60, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTOCCUPIED_H
