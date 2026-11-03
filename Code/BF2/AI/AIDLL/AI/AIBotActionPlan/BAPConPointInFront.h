// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONPOINTINFRONT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONPOINTINFRONT_H

#include "../../../../Math/Vec3.h"
#include "BAPCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;

// ELF: vtable 0x4B9390 (5 words), operator new 0x1C, package G-vehicles.
// BAPConObjectInFront for a fixed point. getConditionType answers ObjectInFront (23), as shipped.
class BAPConPointInFront : public BAPCondition
{
public:
    BAPConPointInFront(const Vec3&, float, float);   // 0x1E9520
    virtual ~BAPConPointInFront();   // 0x1E95B0

    virtual bool evaluate(Bot*);   // 0x1E9610, slot +0x00; from the base declaration
    virtual BAPConditionType getConditionType() const;   // 0x1E9820, slot +0x04; from the base declaration

private:
    /* +0x08 */ float m_distance;   // [INFERRED] names
    /* +0x0C */ float m_minDot;
    /* +0x10 */ Vec3  m_point;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCONPOINTINFRONT_H
