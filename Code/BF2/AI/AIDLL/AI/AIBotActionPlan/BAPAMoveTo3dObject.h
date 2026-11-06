// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3DOBJECT_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3DOBJECT_H

#include "../../../../Math/Vec3.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8100 (9 words), package F-infantry.
// 3d (air) move to an object, driven by its plan entry; execute itself answers false/finished.
// operator new 0x88 (BBPChange::createPlan).
class BAPAMoveTo3dObject : public BAPAction
{
public:
    BAPAMoveTo3dObject(unsigned int, float, float, float, float, BAPCondition*, unsigned long long, int);   // 0x1D7B80
    virtual ~BAPAMoveTo3dObject();   // 0x1D7D60

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D7E30, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1D7FA0, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D7FB0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D7FC0, slot +0x14; from the base declaration
    virtual bool isFinished(Bot*);   // 0x1D7FD0, slot +0x20; [INFERRED] ELF int

    Vec3* getFirstPoint();   // 0x1D7E50; refreshes and returns this + 0x28 (was [INFERRED] int)

private:
    // [INFERRED] friendship: EntryHeliMoveToObject reads +0x34/+0x50 inline (ELF 0x2B73E0).
    friend class EntryHeliMoveToObject;
    // [INFERRED] friendship: EntryPlaneMoveToObject reads/writes +0x14..+0x20/+0x40/+0x50/+0x5C..+0x84 inline (ELF 0x2C2AD0).
    friend class EntryPlaneMoveToObject;
    /* +0x14 */ unsigned int m_object;   // information handle
    /* +0x18 */ float m_float18;         // createPlan: 0.5 [UNRESOLVED]
    /* +0x1C */ float m_maxSpeed;
    /* +0x20 */ float m_maxArriveSpeed;
    /* +0x24 */ BAPCondition* m_breakCondition;   // owned, owner = this
    /* +0x28 */ Vec3 m_point;            // getFirstPoint: the object's position or position estimate
    /* +0x34 */ Vec3 m_vec34;            // [INFERRED] previous velocity sample; EntryHeliMoveToObject
    /* +0x40 */ float m_int40[4];        // [INFERRED] first element: previous speed magnitude; others [UNRESOLVED]
    /* +0x50 */ float m_distance;        // createPlan: 25.0
    /* +0x54 */ int m_unwritten54[2];    // not written by the constructor
    /* +0x5C */ Vec3 m_up;               // (0, 1, 0)
    /* +0x68 */ Vec3 m_forward;          // (0, 0, 1)
    /* +0x74 */ Vec3 m_right;            // (1, 0, 0)
    /* +0x80 */ bool m_objectMobile;     // the object has an IPIMobile (+0x38); only set when it exists
    /* +0x84 */ float m_int84;           // [INFERRED] previous equipment height; EntryPlaneMoveToObject
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3DOBJECT_H
