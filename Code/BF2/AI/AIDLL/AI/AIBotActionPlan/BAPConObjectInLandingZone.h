// Tests whether the object is inside the bot's assigned landing zone.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINLANDINGZONE_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINLANDINGZONE_H

#include "BAPConObject.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9710 (5 words), package H-deferred.
class BAPConObjectInLandingZone : public BAPConObject
{
public:
    explicit BAPConObjectInLandingZone(unsigned int);   // 0x1EEAD0
    virtual ~BAPConObjectInLandingZone();   // 0x1EEB30

    virtual bool evaluate(Bot*);   // 0x1EEB90, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1EECD0, slot +0x04; from the base declaration
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONOBJECTINLANDINGZONE_H
