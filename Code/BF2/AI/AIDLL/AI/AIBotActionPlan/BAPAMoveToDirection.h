// Direction-move state shared by the direct and finding variants; derived classes own its conditions.

#ifndef AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTION_H
#define AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTION_H

#include "../../../../Math/Vec3.h"
#include "BAPAComponentSoldierPose.h"
#include "BAPAction.h"
#include "BAPTypes.h"

namespace dice {
namespace hfe {

namespace ai {

class Bot;
class BAPCondition;

// ELF: vtable 0x4B8200 (32 words).
class BAPAMoveToDirection : public BAPAction, public BAPAComponentSoldierPose
{
public:
    BAPAMoveToDirection();   // 0x1D8520
    virtual ~BAPAMoveToDirection();   // 0x1D8740

    virtual unsigned long long getControls();   // 0x1D86B0, slot +0x0C; from the base declaration
    virtual BAPStatementType getId() const;   // 0x1D86C0, slot +0x10; from the base declaration
    virtual bool hasDefault();   // 0x1D86D0, slot +0x14; from the base declaration
    virtual int update(Vec2&, bool&);   // 0x1D86E0, slot +0x20; [INFERRED] ELF int
    virtual bool isFinished(Bot*);   // 0x1D8700, slot +0x24; [INFERRED] ELF int
    // [CHOSEN, not recovered] Names and integer types under AGENTS.md's unimplemented-slot exception.
    // Pure in base/Direct/Finding: ia-32 vtables 0x4B8200/0x4B82C0/0x4B8360,
    // amd-64 0x614D80/0x614EE0/0x615000, primary slots 10/11 (+0x28/+0x2C; +0x50/+0x58).
    // EntryBoatMoveToDirection calls 0x2B2DFA / amd-64 0x2FF5CA pass only this;
    // dec eax compares the full 32-bit result with 1, so a byte/bool return is insufficient.
    virtual int unimplementedDirectionSlot10() = 0;
    // Calls 0x2B2E5C (also reached from 0x2B30D7) / amd-64 0x2FF78D/0x2FF7A9:
    // stack this,int 1|2 / rdi=this,esi=1|2; no caller consumes the result.
    virtual void unimplementedDirectionSlot11(int) = 0;
    // [INFERRED] names; constructors and interpreter readers attest the member sequence.
    /* +0x40 */ Vec3 m_point;
    /* +0x4C */ float m_maxSpeed;
    /* +0x50 */ float m_maxArriveSpeed;
    /* +0x54 */ Vec3 m_direction;
    /* +0x60 */ BAPCondition* m_endCondition;
    /* +0x64 */ BAPCondition* m_condition64;
    /* +0x68 */ Vec3 m_state68; // [INFERRED] previous signed speed, then XZ facing; EntryTankMoveToDirection (ELF 0x2C4800)
    /* +0x74 */ bool m_flag74;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTACTIONPLAN_BAPAMOVETODIRECTION_H
