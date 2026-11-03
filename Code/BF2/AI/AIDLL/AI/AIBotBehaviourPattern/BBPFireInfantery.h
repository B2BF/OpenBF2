// Infantry fire-plan generation, recovered from libaidll.so (Linux 1.5, ia-32).
// Virtual order is checked by tools/macos/ai_behaviour_vtables.py.

#ifndef AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIREINFANTERY_H
#define AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIREINFANTERY_H

#include <list>

#include "../../../../Math/Vec3.h"
#include "../../../AIInterface/BFKnowledge/WeaponTemplate.h"
#include "../AIBotActionPlan/BAPVariable.h"
#include "BBPFire.h"

namespace dice {
namespace hfe {

namespace ai {

class BAPStatement;
class Bot;
class BotScript;
class IPIControlInfo;
class IPIPhysical;
class Information;
class WeaponInfo;

// ELF: vtable 0x4BBE80 (9 words), operator new 0x34, package E-fire-pattern.
// Layout (x86 ELF): BBPFire up to +0x28, then the per-bot arrays below and one bool (ctor 0x25BFE0).
class BBPFireInfantery : public BBPFire
{
public:
    // Per-bot fire-decision record, 0x5C bytes in the image (new[] with a count cookie, ctor 0x25BC60).
    // Names are [INFERRED] from BBPFireInfantery::createPlan (0x25C8F0); offsets are the ELF's.
    class Data
    {
    public:
        // The decision createPlan picks among its seven candidates; the image stores and compares
        // the values 0..7 (`m_decision`, the history list's element). [INFERRED] numeric names:
        // createPlan's candidate order is 1, 2, 3, 4, 7, 6, 5 and it handles 1, 2, 4 and 5.
        enum FireDecision
        {
            FireDecisionNone = 0,
            FireDecision1 = 1,   // the direct weapon (checks its min/max range)
            FireDecision2 = 2,   // the indirect weapon from here (hasIndirectLineOfFire to the aim point)
            FireDecision3 = 3,
            FireDecision4 = 4,   // the indirect weapon from a firing position (getIndirectWeaponCloseToFiringPositions)
            FireDecision5 = 5,
            FireDecision6 = 6,
            FireDecision7 = 7
        };

        Data();   // 0x25BC60

        /* +0x00 */ double m_lastUpdateTime;          // createPlan: game time of the last decision pass (not set by the ctor)
        /* +0x08 */ FireDecision m_decision;          // ctor 0
        /* +0x0C */ std::list<FireDecision> m_history;   // newest first, trimmed to 8 entries
        /* +0x10 */ int m_phase;                      // ctor 0; 0 start, 1 approach, 2 back off, 3 firing
        /* +0x14 */ double m_lockStartTime;           // ctor 0.0
        /* +0x1C */ double m_lockDuration;            // ctor 10.0; indirect fire: 5..10 s random
        /* +0x24 */ Vec3 m_vec24;                     // ctor 0; [UNRESOLVED] not used by createPlan
        /* +0x30 */ Vec3 m_phaseTargetPosition;       // ctor 0; the target's position when the phase began
        /* +0x3C */ Vec3 m_firePosition;              // ctor 0
        /* +0x48 */ int m_shotsAtPhaseStart;          // IAIPlayer::getNumShotsFired when phase 3 began (not set by the ctor)
        /* +0x4C */ int m_int4C;                      // ctor -1; [UNRESOLVED] createPlan only resets it
        /* +0x50 */ int m_int50;                      // ctor -1; [UNRESOLVED] createPlan only resets it
        /* +0x54 */ double m_phaseStartTime;          // game time when phase 3 began (not set by the ctor)
    };

    // The firing parameters createPlan hands to createPlanInternal, 0x38 bytes (ctor 0x25BAA0).
    // Names are [INFERRED] from createPlan; offsets are the ELF's.
    class FiringState
    {
    public:
        FiringState();   // 0x25BAA0

        /* +0x00 */ unsigned int m_target;            // ctor -1
        /* +0x04 */ int m_weapon;                     // ctor -1; the BBPFire weapon index
        /* +0x08 */ int m_shots;                      // ctor -1; -1 = until the target dies or the magazine is empty
        /* +0x0C */ bool m_bool0C;                    // ctor false; carried over unchanged by createPlan
        /* +0x10 */ int m_int10;                      // not set by the ctor; createPlan forces 1 while fire is not allowed
        /* +0x14 */ bool m_bool14;                    // ctor false
        /* +0x15 */ bool m_backOff;                   // ctor false; set with Data::m_phase 2
        /* +0x16 */ bool m_independentAim;            // ctor false; drive and aim controls do not overlap
        /* +0x17 */ bool m_hasFiringPosition;         // ctor false
        /* +0x18 */ Vec3 m_position;                  // ctor 0
        /* +0x24 */ bool m_bool24;                    // ctor false
        /* +0x25 */ bool m_bool25;                    // ctor false
        /* +0x26 */ bool m_bool26;                    // ctor false
        /* +0x28 */ Vec3 m_firingPosition;            // ctor 0
        /* +0x34 */ float m_arrivalRadius;            // ctor 0; createPlan: own physical radius + 2
    };

    explicit BBPFireInfantery(int);   // 0x25BFE0
    virtual ~BBPFireInfantery();   // 0x25C350

    virtual BotScript* createPlan(Bot*);   // 0x25C8F0, slot +0x08; from the base declaration
    virtual void init(Bot*, unsigned int, int, bool, bool);   // 0x25C480, slot +0x18; from the base declaration
    virtual void init(Bot*, unsigned int, const Vec3&, int);   // 0x25C530, slot +0x1C; from the base declaration
    virtual int getImplementationType();   // 0x27BC00, slot +0x20; from the base declaration

    // createPlanInternal returns its result as the plan (0x2779B1, then `jmp 0x2672D0`, the common return with %eax).
    BotScript* createGuidedPlanInternalInfantry(Bot*, bool, const Vec3&);   // 0x25F740
    // createPlan returns its result as the plan (`movl %eax,%esi` after the call at 0x25D734).
    BotScript* createPlanInternal(Bot*, bool);   // 0x2650F0
    // Both fire-plan builders return the statement createPlanInternal adds to a combiner or an If.
    BAPStatement* createFirePlan(Bot*, BBPFireInfantery::FiringState*, BAPVariable<WeaponInfo*>*, BAPVariable<unsigned int>*, BAPVariable<bool>*, BAPVariable<Vec3>*, BAPVariable<Vec3>*, BAPVariable<Vec3>*, WeaponInfo*, Information*, IPIControlInfo*, Information*, IPIPhysical*, WeaponGuidingSystem);   // 0x277CF0
    BAPStatement* createFirePlanGuided(Bot*, const BBPFireInfantery::FiringState*, BAPVariable<WeaponInfo*>*, BAPVariable<unsigned int>*, BAPVariable<Vec3>*, WeaponInfo*, Information*, IPIControlInfo*, Information*, IPIPhysical*, WeaponGuidingSystem);   // 0x279B40
    void calculateAwayPosition(const Vec3&, const Vec3&, int, Vec3&, float);   // 0x27B6F0

protected:
    /* +0x28 */ FiringState* m_firingStates;   // new FiringState[maxBots]
    /* +0x2C */ Data* m_data;                   // new Data[maxBots]
    /* +0x30 */ bool m_freshPlan;               // createPlan: no plan of this pattern yet, or bot skill < 0.3 [INFERRED] name
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOTBEHAVIOURPATTERN_BBPFIREINFANTERY_H
