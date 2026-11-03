// Base of the 3d aim actions (aircraft and helicopter weapons): the plan statement the vehicle aim
// entries (EntryHeliAimAt, ...) execute. It keeps the aimer's ballistic state for the precision
// conditions (BAPCConPrecision3d, BAPCConBombPrecision3d).
// Recovered from libaidll.so (Linux 1.5.3153, ia-32, symbols); every body carries its ELF address.
// Virtual functions are in ELF vtable slot order (tools/macos/ai_behaviour_vtables.py checks them).

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMAT3D_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMAT3D_H

#include "../../../../Math/Mat4.h"
#include "../../../../Math/Vec3.h"
#include "../AIBot/Aimer.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPCondition;
class Bot;

// ELF: vtable 0x4B7800 (19 words), package G-vehicles. Object size 0x1D4 (the subclasses add from
// +0x1D4). Member names are [INFERRED]; members only the entries use are [UNRESOLVED] by meaning
// and carry their constructed values.
class BAPAAimAt3d : public BAPAction
{
public:
    BAPAAimAt3d();   // 0x1CDE70
    virtual ~BAPAAimAt3d();   // 0x1CE360 (weak)

    virtual bool execute(Bot*, bool&, bool&);   // 0x1CDC70, slot +0x00
    virtual BAPStatementType getId() const;   // 0x1CE280, slot +0x10 (weak): AimAt
    virtual bool hasDefault();   // 0x1CE290, slot +0x14 (weak): false
    virtual int getAimAtType() = 0;   // no body, slot +0x20: 1 object, 0 point
    // Target position minus `from`; false when the target has no information.
    virtual bool getVectorToTarget(const Vec3& from, Vec3& vector) const = 0;   // no body, slot +0x24
    // Feeds the aimer with the weapon transform and velocities and solves the firing direction
    // (Aimer::getFiringDirection); returns, and stores, whether it found one.
    virtual bool getAimVec(const Mat4& transform, const Vec3& shooterVelocity, Vec3& direction, float accuracy,
                           float drag, float speed, float gravityScale, bool highArc) = 0;   // no body, slot +0x28
    // The return types of the slots below follow their bodies (this + 0xCC, the +0x114 flag, the
    // Aimer results); the skeleton's int/void were wrong.
    virtual const Mat4& getAimerTransform() const;   // 0x1CE2A0, slot +0x2C (weak); [INFERRED] reference
    virtual bool isAimVecValid() const;   // 0x1CE2B0, slot +0x30 (weak)
    virtual bool getLastAimVec(Vec3&);   // 0x1CE2C0, slot +0x34 (weak)
    virtual bool getPredictedPosition(Vec3&) const;   // 0x1CE300, slot +0x38 (weak)
    virtual bool getImpactTime(double&) const;   // 0x1CE340, slot +0x3C (weak)
    virtual bool getImpactPosition(Vec3&, double) const;   // 0x1CE050, slot +0x40
    virtual bool getNearestImpactPosition(Vec3&, double&) const;   // 0x1CE0D0, slot +0x44
    virtual void getPredictedPosition(double, Vec3&) const;   // 0x1CE150, slot +0x48

protected:
    // [INFERRED] friendship: EntryHeliAimAt reads/writes +0x14/+0x24..+0x3C/+0xA8/+0x1A4..+0x1D0 inline (ELF 0x2B5420).
    friend class EntryHeliAimAt;
    // [INFERRED] friendship: EntryPlaneAimAt reads/writes +0x14/+0x24..+0x3C/+0xA8/+0x1A4..+0x1B8 inline (ELF 0x2BFC70).
    friend class EntryPlaneAimAt;
    /* +0x014 */ BAPCondition* m_condition;      // the subclass constructors' condition; deleted by them
    /* +0x018 */ Vec3   m_offset;                // aim offset (subclass constructors)
    /* +0x024 */ Vec3   m_vec24;                 // [INFERRED] equipment facing; EntryHeliAimAt
    /* +0x030 */ Vec3   m_vec30;                 // [INFERRED] equipment right axis; EntryHeliAimAt
    /* +0x03C */ Vec3   m_vec3C;                 // [INFERRED] equipment up axis; EntryHeliAimAt
    /* +0x048 */ Vec3   m_vecs48[4];             // [UNRESOLVED] constructed zero
    /* +0x078 */ Vec3   m_vecs78[4];             // [UNRESOLVED] constructed zero
    /* +0x0A8 */ Vec3   m_vecA8;                 // [INFERRED] previous velocity sample; EntryHeliAimAt
    /* +0x0B4 */ Vec3   m_lastAimVec;            // getAimVec's direction
    /* +0x0C0 */ Vec3   m_predictedPosition;     // getFiringDirection's target, in world space
    /* +0x0CC */ Mat4   m_aimerTransform;        // the weapon transform of the last getAimVec; identity
    /* +0x10C */ double m_impactTime;            // getFiringDirection's impact time (not constructed)
    /* +0x114 */ bool   m_aimVecValid;           // getAimVec's result
    /* +0x118 */ Aimer  m_aimer;
    /* +0x184 */ char   m_unknown184[0x18];      // [UNRESOLVED] not touched by the AimAt3d family
    /* +0x19C */ double m_double19C;             // [UNRESOLVED] subclass constructors: -5e9
    /* +0x1A4 */ float  m_float1A4;              // [UNRESOLVED] the subclass constructors' first float
    /* +0x1A8 */ float  m_float1A8;              // [UNRESOLVED] their second float
    /* +0x1AC */ float  m_floats1AC[4];          // [INFERRED] yaw error, pitch error, unresolved, equipment height; EntryHeliAimAt
    /* +0x1BC */ int    m_int1BC;                // [UNRESOLVED] subclass constructors: 0
    /* +0x1C0 */ int    m_unknown1C0;            // [UNRESOLVED] not constructed
    /* +0x1C4 */ int    m_int1C4;                // [UNRESOLVED] constructed 0 (again by the subclasses)
    /* +0x1C8 */ Vec3   m_vec1C8;                // [INFERRED] previous steering direction; EntryHeliAimAt
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAAIMAT3D_H
