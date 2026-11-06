// Turn statement base: the vehicle turn entries steer towards getDirection().
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURN_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURN_H

#include "../../../../Math/Vec3.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8D40 (9 words), package H-deferred. Size 0x34 (BAPATurnTowardsDirection's data
// starts at +0x34).
class BAPATurn : public BAPAction
{
public:
    BAPATurn(float, BAPCondition*, unsigned long long, int);   // 0x1E2140
    virtual ~BAPATurn();   // 0x1E2200

    virtual bool execute(Bot*, bool&, bool&);   // 0x1E2290, slot +0x00; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1E2360, slot +0x10; from the base declaration
    virtual bool hasDefault() { return false; }   // 0x1E2370 (weak), slot +0x14
    virtual Vec3 getDirection(bool&) = 0;   // no body, slot +0x20; [INFERRED] from BAPATurnTowardsObject: ELF sret (returned through a hidden pointer)

    // [INFERRED] names; the constructor's stores are attested. The turn entries own the reads.
    /* +0x14 */ BAPCondition* m_endCondition;   // the derived classes own and delete it
    /* +0x18 */ float m_turnSpeed;              // the ctor's float
    /* +0x1C */ float m_float1C;                // = 0 [UNRESOLVED]
    /* +0x20 */ float m_float20;                // = 0 [UNRESOLVED]
    /* +0x24 */ float m_unwritten24;            // [UNRESOLVED] not written by the constructor
    /* +0x28 */ Vec3 m_vec28;                   // = 0 [UNRESOLVED]
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPATURN_H
