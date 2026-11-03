// Turn statement towards a fixed direction.
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURNTOWARDSDIRECTION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURNTOWARDSDIRECTION_H

#include "../../../../Math/Vec3.h"
#include "BAPATurn.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;

// ELF: vtable 0x4B8D80 (9 words), package H-deferred.
class BAPATurnTowardsDirection : public BAPATurn
{
public:
    BAPATurnTowardsDirection(const Vec3&, float, BAPCondition*, unsigned long long, int);   // 0x1E2410
    virtual ~BAPATurnTowardsDirection();   // 0x1E24F0

    virtual Vec3 getDirection(bool&);   // 0x1E25A0, slot +0x20; from the base declaration

    /* +0x34 */ Vec3 m_direction;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURNTOWARDSDIRECTION_H
