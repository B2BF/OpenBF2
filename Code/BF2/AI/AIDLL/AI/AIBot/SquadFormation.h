// Maintains squad member positions, movement flags and formation speed.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_SQUADFORMATION_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SQUADFORMATION_H

#include <utility>
#include <vector>

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class SquadCoordinator;
class SquadMember;

class SquadFormation
{
public:
    // One position in the formation. , from the constructor's `operator new(0x24)`, and
    // the type name is attested by the mangled `std::vector<SquadFormation::Slot*>` instantiations.
    // The constructor writes every field; the names below are [INFERRED] from the two readers.
    class Slot
    {
    public:
        // [CHOSEN, not recovered] Source packaging for swapSlots' measured temporary:
        // only its Vec3 is initialized. Ordinary formation slots retain their constructor.
        struct SwapTemporary {};
        explicit Slot(SwapTemporary) : m_offset() {}

        Slot()
            : hasSquadMemberAssigned(false)
            , m_memberIndex(-1)
            , m_i20(-1)
            , m_b24(false)
            , m_i28(0)
            , m_referenceSlot(0)
        {
            m_offset.x = 0.0f;
            m_offset.y = 0.0f;
            m_offset.z = 0.0f;
        }

        /*  +0 */ bool hasSquadMemberAssigned;       // the flag every deep branch in this class is guarded by; name: AIDLL_r assert (0x101B4F00; +0)
        /*  +4 */ int  m_memberIndex;    // = -1; indexes the coordinator's squad-member vector
        /*  +8 */ Vec3 m_offset;         // the position the formation setters compute
        /* +20 */ int  m_i20;            // = -1; the trailing position (m_trailingPositions) this slot falls back to
        /* +24 */ bool m_b24;            // = false [UNRESOLVED]
        /* +28 */ float m_i28;           // legacy inferred name; float proved by leaveFormation's x87 loads/stores
        /* +32 */ int  m_referenceSlot;  // the slot this one is measured from [INFERRED]
    };

    // values attested, names [INFERRED]. `setFormation` switches on 0, 1, 2 and 3 and nothing
    // else; `changeFormation(Formations)` is the mangled name that says this is an enum at all.
    enum Formations
    {
        FormationLine     = 0,
        FormationWedge    = 1,
        FormationEchelonA = 2,
        FormationEchelonB = 3
    };

    explicit SquadFormation(SquadCoordinator* coordinator);
    ~SquadFormation();                              // ELF 0x1A9E10 (D1)

    void update(double time);                 // complete scheduler
    void updateFormation();                  // leader/anchor reassignment
    void updateWaitForSquadMembers(double time); // ELF 0x1AAFC0
    void updateFormationForwardVector();     // ELF 0x1AB470
    void updateTrailingPositions();          // complete
    void updateTrailingSlots();              // complete
    void selectFormation();                  // / PE inlined at
    int getSquadMemberIndexAtSlot(int slotIndex); // no occupied test
    // ELF 0x1AB6C0: the first slot is occupied (no empty-vector guard).
    bool hasSquadMembers() { return m_slots[0]->hasSquadMemberAssigned; }
    bool getSlotWorldPos(int memberIndex, Vec3& position);     // ELF 0x1AC550
    int getFirstFreeTrailingSlot(SquadMember* member);         // ELF 0x1ACA30
    bool isTrailPosAssigned(int trailIndex);                   // ELF 0x1ACB70
    bool shouldLeaveSquadLeader();           // returns m_b84
    void joinSquadLeader();                  // (inlines the movement update)
    void leaveSquadLeader();                 // ELF 0x1AB640
    float getFormationMaxSpeed();
    bool shouldSquadMoveOut() { return m_b36; }   // ELF 0x1AB6F0: +0x24
    void updateFormationMaxSpeed();
    void setFormation();
    void changeFormation(Formations formation);
    bool hasFormationMoved();
    bool haveLineOfSightBetween(Vec3& from, Vec3& to);
    void debugCheckNoFormationSlotGaps();
    void setLineFormation();
    void setWedgeFormation();                // ELF 0x1ABA10
    void setEchelonFormation();              // ELF 0x1ABBF0

    void joinFormation(int memberIndex);
    bool isInFormation(int memberIndex);      // ignores occupied flag
    void leaveFormation(int memberIndex);
    void resetSlot(int slotIndex);            // inlined
    void reassignSlots();                     // ELF 0x1AC370
    void updateClosestSlots();
    int getClosestSlotIndex(int slotIndex);    // world-position seam
    void swapSlots(int first, int second);    // three payload fields only
    void updateMovementFlag(bool force);
    void updateSlotZeroPos();                 // inlined
    void resetTrailingPositions();

    // layout, on the ELF, read store by store out of the constructor
    /*   +0 */ std::vector<Slot*> m_slots;            // one per squad member, each `new`ed
    /*  +12 */ SquadCoordinator* m_coordinator;       // the constructor's argument
    /*  +16 */ int m_formation;                       // = 0 (line); `setFormation` switches on it
    /*  +20 */ Vec3 m_direction;                      // set by `setFormation` per formation
    /*  +32 */ unsigned int m_numberOfSlots;        // = IAIGame::getMaxNumberOfSquadMembers  // name: AIDLL_r assert 0x101B5980 (PE +0x24)
    /*  +36 */ bool m_b36;                            // = false; shouldSquadMoveOut returns it
    /*  +40 */ float m_formationMaxSpeed;             // FLT_MAX, then 5.0f - see the header note
    /*  +44 */ int m_side;                            // = the coordinator's side
    // ELF +0x30 / PE +0x34: the formation's XZ forward vector, born (0, 1). Attested as one Vec2
    // by the forward-vector update passing its address to `vec3ToVec2XZ(const Vec3&, Vec2&)`;
    // getSlotWorldPos (ELF 0x1AC550) reads it as two floats.
    /*  +48 */ Vec2 m_forward;
    /*  +56 */ bool m_b56;                            // = false; hasFormationMoved returns it
    /*  +60 */ Vec3 m_v60;                            // zeroed; anchor position at the last movement stamp
    /*  +72 */ Vec3 m_v72;                            // zeroed; slot-zero world position (updateSlotZeroPos)
    /*  +84 */ bool m_b84;                            // = false; shouldLeaveSquadLeader returns it
    /*  +85 */ bool m_b85;                            // = false; set by joinSquadLeader, cleared by promotion
    /*  +88 */ int m_i88;                             // = 0; the scheduler's signed six-tick cycle counter
    /*  +92 */ double m_d92;                          // = 0.0; next movement-stamp time (now + 0.2)
    // [INFERRED] Per-slot breadcrumb targets. Anchor movement beyond six metres shifts the
    // trail and inserts its previous position; construction sizes it to the slot count.
    /* +100 */ std::vector<std::pair<bool, Vec3> > m_trailingPositions;   // (valid, crumb) per index  // name: AIDLL_r assert 0x101B7E20 (PE +0x68)
    /* +112 */ Vec3 m_trailingPosition;               // [INFERRED name]; the newest crumb, seeded from m_v72
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SQUADFORMATION_H
