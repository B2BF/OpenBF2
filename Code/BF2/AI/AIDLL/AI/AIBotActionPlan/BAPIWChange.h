// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWCHANGE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWCHANGE_H

#include "BAPInfoWrapper.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;

// ELF: vtable 0x4B9FE0 (9 words), package F-infantry.
// Info wrapper of type "Change" (8): books the target vehicle/seat for the player while the plan
// lives; the destructor cancels the booking and radios the pickup result (messages 11 / 8).
class BAPIWChange : public BAPInfoWrapper
{
public:
    BAPIWChange(unsigned int, BAPStatement*, int, int);   // 0x1F5A80
    virtual ~BAPIWChange();   // 0x1F5FC0

    virtual int getInfoType() const;   // 0x1F69D0, slot +0x20; from the base declaration

    unsigned int getTarget() const { return m_target; }   // [CHOSEN] inline reader: BBPChange::createPlan reads +0x18

private:
    /* +0x18 */ unsigned int m_target;   // vehicle/seat information handle
    /* +0x1C */ int m_playerId;          // Bot::getPlayerId of the plan's bot
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPIWCHANGE_H
