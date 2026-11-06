// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTBEHIND_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTBEHIND_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B95F8 (5 words), operator new 0x14, package F-infantry.
// True when the bot's equipment is behind the object: the XZ direction object->bot dotted with the
// object's forward axis (transformation row 2) is at most the threshold. True when the object is gone.
class BAPConObjectBehind : public BAPConObject
{
public:
    BAPConObjectBehind(unsigned int, float, bool);   // 0x1ECB20
    virtual ~BAPConObjectBehind();   // 0x1ECB80

    virtual bool evaluate(Bot*);   // 0x1ECBE0, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1ECFB0, slot +0x04; from the base declaration
private:
    /* +0x0C */ float m_threshold;     // compared with the dot product (createPlan passes -0.95 / -0.8)
    /* +0x10 */ bool m_useEstimate;    // use the object's IPIMobile position estimate when it has one
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTBEHIND_H
