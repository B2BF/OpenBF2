// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPICCHANGEVEHICLE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPICCHANGEVEHICLE_H

#include "BAPInterruptControl.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4BA280 (8 words), operator new 0x18, package F-infantry.
// Interrupt control: hands the vehicle/seat handle to the bot (Bot::setChangeTarget, +0x1B8).
class BAPICChangeVehicle : public BAPInterruptControl
{
public:
    explicit BAPICChangeVehicle(unsigned int);   // 0x1F8490
    virtual ~BAPICChangeVehicle();   // 0x1F85F0

    virtual bool execute(Bot*, bool&, bool&);   // 0x1F84E0, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1F8520, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1F8530, slot +0x14; from the base declaration

private:
    /* +0x14 */ unsigned int m_target;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPICCHANGEVEHICLE_H
