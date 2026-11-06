// Turn statement towards an object.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURNTOWARDSOBJECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURNTOWARDSOBJECT_H

#include "../../../../Math/Vec3.h"
#include "BAPATurn.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8DC0 (9 words), package H-deferred. Size 0x3C (`operator new(0x3c)` in
// BBPFire2dDriver::createPlan).
class BAPATurnTowardsObject : public BAPATurn
{
public:
    BAPATurnTowardsObject(Bot*, unsigned int, float, BAPCondition*, unsigned long long, int);   // 0x1E27C0
    virtual ~BAPATurnTowardsObject();   // 0x1E2950

    virtual Vec3 getDirection(bool&);   // 0x1E2A00, slot +0x20; from the base declaration

    /* +0x34 */ Bot* m_bot;
    /* +0x38 */ unsigned int m_target;   // information handle
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURNTOWARDSOBJECT_H
