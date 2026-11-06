// Tests whether the object's speed is strictly below a fixed threshold.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTSPEED_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTSPEED_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9870 (5 words), package H-deferred.
class BAPConObjectSpeed : public BAPConObject
{
public:
    BAPConObjectSpeed(unsigned int, float);   // 0x1F0060
    virtual ~BAPConObjectSpeed();   // 0x1F00C0

    virtual bool evaluate(Bot*);   // 0x1F0120, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F01A0, slot +0x04; from the base declaration
private:
    /* +0x0C */ float m_speed;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTSPEED_H
