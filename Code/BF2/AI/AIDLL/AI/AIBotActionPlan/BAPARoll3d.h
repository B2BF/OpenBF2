// Roll state consumed by the plane interpreter; owns its required break condition.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAROLL3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAROLL3D_H

#include "BAPAction.h"
#include "../../../../Math/Vec3.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8980 (8 words), package H-deferred.
class BAPARoll3d : public BAPAction
{
public:
    BAPARoll3d(Bot*, int, float, float, BAPCondition*, unsigned long long, float, int);   // 0x1DEEB0
    virtual ~BAPARoll3d();   // 0x1DF210

    virtual bool execute(Bot*, bool&, bool&);   // 0x1DF2F0, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1DF310, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1DF400, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1DF320, slot +0x14; from the base declaration

    float getMaxSpeed();   // 0x1DF330; [INFERRED] ELF float
    BAPCondition* getBreakCondition();   // 0x1DF340
    // [INFERRED] names. The plane entry reads and updates these fields inline.
    /* +0x14 */ float m_maxSpeed;
    /* +0x18 */ float m_roll;
    /* +0x1C */ Vec3 m_vector1C;
    /* +0x28 */ float m_float28;
    /* +0x2C */ float m_float2C; // shipped defect: constructor leaves this and +0x30 uninitialized
    /* +0x30 */ float m_previousY;
    /* +0x34 */ int m_direction;
    /* +0x38 */ float m_initialAltitude;
    /* +0x3C */ Vec3 m_forward;
    /* +0x48 */ Vec3 m_up;
    /* +0x54 */ Vec3 m_right;
    /* +0x60 */ Vec3 m_flatFacing;
    /* +0x6C */ Vec3 m_vector6C;
    /* +0x78 */ Vec3 m_vector78;
    /* +0x84 */ BAPCondition* m_breakCondition;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAROLL3D_H
