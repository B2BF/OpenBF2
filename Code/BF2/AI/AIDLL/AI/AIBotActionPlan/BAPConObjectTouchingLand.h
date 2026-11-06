// Tests the inverse of the object's physical in-air state.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTTOUCHINGLAND_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTTOUCHINGLAND_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9898 (5 words), package H-deferred.
class BAPConObjectTouchingLand : public BAPConObject
{
public:
    explicit BAPConObjectTouchingLand(unsigned int);   // 0x1F02A0
    virtual ~BAPConObjectTouchingLand();   // 0x1F0300

    virtual bool evaluate(Bot*);   // 0x1F0360, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1F0490, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTTOUCHINGLAND_H
