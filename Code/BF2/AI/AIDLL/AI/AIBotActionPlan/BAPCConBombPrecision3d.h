// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCCONBOMBPRECISION3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCCONBOMBPRECISION3D_H

#include "BAPConnectedCondition.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPAAimAt3d;
class Bot;

// ELF: vtable 0x4B9510 (5 words), operator new 0x1C, package G-vehicles.
// BAPCConPrecision3d for bombs, without the angle test: the squared precision is at least 0.01;
// with waitForClosest a miss within half the precision answers at once.
class BAPCConBombPrecision3d : public BAPConnectedCondition
{
public:
    BAPCConBombPrecision3d(BAPAAimAt3d*, float, bool);   // 0x1EB110
    virtual ~BAPCConBombPrecision3d();   // 0x1EB4F0

    virtual bool evaluate(Bot*);   // 0x1EB170, slot +0x00
    virtual BAPConditionType getConditionType() const;   // 0x1EB430, slot +0x04

private:
    /* +0x08 */ float        m_precisionSq;
    /* +0x0C */ BAPAAimAt3d* m_aim;
    /* +0x10 */ float        m_bestDistanceSq;   // FLT_MAX: no approach yet
    /* +0x14 */ float        m_float14;          // [UNRESOLVED] constructed FLT_MAX, never read
    /* +0x18 */ bool         m_waitForClosest;   // [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPCCONBOMBPRECISION3D_H
