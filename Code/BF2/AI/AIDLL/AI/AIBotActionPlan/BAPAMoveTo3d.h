// Package F (ai-w1-f-urgency): recovered from libaidll.so 1.5.3153 (ia-32); ELF addresses on every body.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3D_H

#include "../../../../Math/Vec3.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B8080 (9 words), package F-infantry.
// 3d (air) move to a point, driven by its plan entry; execute itself answers false/finished.
// operator new 0x7C (BBPChange::createPlan).
class BAPAMoveTo3d : public BAPAction
{
public:
    BAPAMoveTo3d(Bot*, const Vec3&, float, float, BAPCondition*, unsigned long long, float, int);   // 0x1D6B90
    virtual ~BAPAMoveTo3d();   // 0x1D6E30

    virtual bool execute(Bot*, bool&, bool&);   // 0x1D6F00, slot +0x00; from the base declaration
    virtual unsigned long long getControls();   // 0x1D6F20, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D7030, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D6F30, slot +0x14; from the base declaration
    virtual bool isFinished(Bot*);   // 0x1D7040, slot +0x20; [INFERRED] ELF int

    float getMaxSpeed();   // 0x1D6F40; x87 load of +0x20
    float getMaxArriveSpeed();   // 0x1D6F50; x87 load of +0x24
    BAPCondition* getBreakCondition();   // 0x1D6F60; returns +0x28 (was [INFERRED] int)
    Vec3* getFirstPoint();   // 0x1D6F70; returns this + 0x2C (was [INFERRED] int)

private:
    // [INFERRED] friendship: EntryHeliMoveTo reads +0x3C/+0x48 inline (ELF 0x2B69B0).
    friend class EntryHeliMoveTo;
    // [INFERRED] friendship: EntryPlaneMoveTo reads/writes +0x48/+0x4C/+0x58..+0x78 inline (ELF 0x2C0C80).
    friend class EntryPlaneMoveTo;
    /* +0x14 */ Vec3 m_vec14;            // = 0 [UNRESOLVED]
    /* +0x20 */ float m_maxSpeed;
    /* +0x24 */ float m_maxArriveSpeed;
    /* +0x28 */ BAPCondition* m_breakCondition;   // owned (deleted by the destructor), owner = this
    /* +0x2C */ Vec3 m_point;            // the goal
    /* +0x38 */ int m_unwritten38;       // not written by the constructor
    /* +0x3C */ Vec3 m_vec3C;            // [INFERRED] previous velocity sample; EntryHeliMoveTo
    /* +0x48 */ float m_distance;        // the constructor's 7th argument (createPlan: 25.0)
    /* +0x4C */ float m_int4C;           // [INFERRED] previous equipment height; EntryPlaneMoveTo
    /* +0x50 */ int m_int50;             // = 0 [UNRESOLVED]
    /* +0x54 */ float m_altitude;        // current equipment's getAltitude(its IPIMobile::getSpeed())
    /* +0x58 */ Vec3 m_forward;          // current equipment's transformation row 2
    /* +0x64 */ Vec3 m_up;               // row 1
    /* +0x70 */ Vec3 m_right;            // row 0
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETO3D_H
