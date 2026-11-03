// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINFRONT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINFRONT_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B96E8 (5 words), operator new 0x14, package F-infantry.
// True when the direction from a point `distance` ahead of the bot's vehicle to the object deviates
// from the vehicle's forward axis by a cosine above `minDot`.
class BAPConObjectInFront : public BAPConObject
{
public:
    BAPConObjectInFront(unsigned int, float, float);   // 0x1EE6D0
    virtual ~BAPConObjectInFront();   // 0x1EE730

    virtual bool evaluate(Bot*);   // 0x1EE790, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EE9D0, slot +0x04; from the base declaration

private:
    /* +0x0C */ float m_distance;   // [INFERRED] names
    /* +0x10 */ float m_minDot;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINFRONT_H
