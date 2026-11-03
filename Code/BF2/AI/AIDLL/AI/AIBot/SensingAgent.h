// Owns and executes a bot's sensing plan.

#ifndef AI_AIDLL_AI_AIBOT_SENSINGAGENT_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SENSINGAGENT_H

#include <utility>
#include <vector>

#include "../../../../Math/Vec3.h"     // Vec2 and Vec3 live here, and both are typedefs

namespace dice {
namespace hfe {
namespace ai {

class BAPContext;
// The plan is a `BotScript*` (the patterns' createPlan returns one; deletePlan deletes it through
// the virtual destructor).
class BotMain;
class BotSenses;
class Information;
class ISensingAgentPattern;
class BotScript;

class SensingAgent
{
    // [INFERRED] friendship: AIConsole::sensingAgentDebug ELF 0x396930 stores +0x86 at 0x396970.
    friend class AIConsole;
public:
    // The second parameter is in the ABI and is never read - see the header block.
    SensingAgent(BotMain* owner, unsigned char behaviourCount);
    ~SensingAgent();                            // [FRONTIER]

    // A focus candidate: one per sensed object (evaluateSensedObjects 0x1BC5B0) or incoming shot
    // (evaluateFireObjects 0x1BC7E0); 16 bytes, the vector stride (`>> 4`). [INFERRED] member names.
    struct FocusInformation
    {
        float weight;                           // +0
        Vec3  position;                         // +4
    };
    // The chosen focus quadrant: update keeps it on the stack (0x14 bytes used); computeTargetQuadrant
    // fills it, createPlan / checkForPlanInvalidation / calculateCurrentPeriod read it. [INFERRED] names.
    struct QuadrantInformation
    {
        int   quadrant;                         // +0, -1 = none
        float weight;                           // +4
        Vec3  position;                         // +8, the mean position of the quadrant's candidates
    };
    // getQuadrantWeights' per-quadrant accumulator: weight, (position sum, candidate count).
    typedef std::vector<std::pair<float, std::pair<Vec3, unsigned int> > > QuadrantWeights;

    void reset();
    // Called by `BotMain::reset` when a bot loses its active plan - losing a plan releases the
    // bot's aim override.
    void setOverride(bool);
    void deletePlan();
    void clearPlanTargetInformation();
    void update();                              // ELF 0x1BBD30
    void executePlan();                         // ELF 0x1BF230
    void disallowOverride(bool b);              // ELF 0x1BE0A0
    void updateQuadTimeUrgencies(Vec2& direction, bool turnedAway);   // ELF 0x1BC1A0
    void calcTargetQuadFromObjects(QuadrantInformation* info, const Vec2& direction);   // ELF 0x1BC410
    void evaluateSensedObjects();               // ELF 0x1BC5B0
    void evaluateFireObjects();                 // ELF 0x1BC7E0
    void getQuadrantWeights(QuadrantWeights& weights);                                  // ELF 0x1BCA90
    void computeTargetQuadrant(const QuadrantWeights& weights, QuadrantInformation* info);   // ELF 0x1BCD50
    void checkForPlanInvalidation(const QuadrantInformation& info, bool force);         // ELF 0x1BCFF0
    bool targetFeasible(int quadrant) const;    // ELF 0x1BD170
    bool isForward(int quadrant, const Vec2& direction) const;   // ELF 0x1BD250
    bool isBehind(int quadrant, const Vec2& direction) const;    // ELF 0x1BD340
    bool withinCamView(const Vec2& direction) const;             // ELF 0x1BD430
    void clearInformation();                    // ELF 0x1BE020
    void calculateCurrentPeriod(const QuadrantInformation& info, Information* equipment,
                                const Vec2& direction);          // ELF 0x1BE250
    void createPlan(ISensingAgentPattern* pattern, const QuadrantInformation& info);   // ELF 0x1BE850
    void createDirPlan(const Vec3& direction, ISensingAgentPattern* pattern);         // ELF 0x1BF120
    void createNullPlan();                      // ELF 0x1BF180
    // ELF 0x1BE110: the quadrants one unit ahead of the camera that are in focus are marked seen
    // (weight 0); seeing the plan's focus quadrant (+24) sets +44.
    void setQuadSeen();
    // ELF 0x1BD860: the two quadrants (0..3 diagonal sectors, 4..7 axis sectors) of a position
    // around the camera, in the XZ plane; false when the position is on the camera.
    bool computeQuadrantsFromPosition(const Vec3& position, int* quadrants) const;
    // ELF 0x1BDA60: the camera's XZ forward is within ~10 degrees (dot > 0.9848) of the quadrant.
    bool quadInFocus(int quadrant) const;
    // ELF 0x1BDBC0: the quadrant's unit XZ direction; anything else is reported.
    void getQuadPosition(int quadrant, Vec2& position) const;
    BotScript* getPlan() const { return m_plan; }   // BotMain::planExecution reads +136 inline
    // BotMain's sensing forwarders read +104 inline (ELF 0x18a1d0 `mov 0xc0(%eax),%eax; mov 0x68(%eax)`).
    BotSenses* getSenses() const { return m_senses; }
    // ELF 0x1BDF70: empty.
    void insertBehaviourInformation(float, const Vec3&, unsigned char) {}
    // ELF 0x1BDF80: empty.
    void insertPlanTargetInformation(float, unsigned int, bool) {}
    // ELF 0x1BDF90: a plan direction - +64 the direction, +60 the bot's current equipment,
    // +76 the weight, and the sensed-new flag (+120) set to `flag`.
    void insertPlanDirectionInformation(float weight, const Vec3& direction, bool flag);
    void drawSensingAgentDebugStuff();
    // BotSenses::event_seenObject (indirect sighting) and event_heardObject (new record) store
    // `movb $1, 0x78(agent)` inline. [INFERRED] name.
    void setSensedNewObject() { m_flag120 = true; }
    // EntryInfanteryMoveTo / MoveToObject (0x2B7CF7 `fstpl 0x7c(%eax)`) store 0.0 here inline, so the
    // next update ends a pending override at once. [INFERRED] name.
    void setOverrideTime(double time) { m_double124 = time; }

    // [INFERRED] inline accessors for the fields the BAP actions read and write in place
    // (ai-w1-c-actions request; ELF 0x1D3F00, 0x1D5660).
    const Vec2& getMoveDirection() const { return m_moveDirection; }
    void setMoveDirection(const Vec2& direction) { m_moveDirection = direction; }
    const Vec3& getSprintFocus() const { return m_sprintFocus; }
    void setSprintFocus(const Vec3& point) { m_sprintFocus = point; }
    unsigned int getSprintFocusTarget() const { return m_sprintFocusTarget; }
    void setSprintFocusTarget(unsigned int handle) { m_sprintFocusTarget = handle; }

private:
    // [INFERRED] friendship: the patterns' createPlan reads and clears +0x88 (the plan) and clears
    // +0x90 (the BAP context) inline (e.g. SensingAgentNullPattern 0x1C1A93 / 0x1C1AD1).
    friend class SensingAgentTurretPattern;
    friend class SensingAgentMouseFreeLookPattern;
    friend class SensingAgentSoldierPattern;
    friend class SensingAgentNullPattern;
    // the candidates of the last update; clearInformation zeroes their weights. Constructed empty.
    /*   0 */ std::vector<FocusInformation> m_sensedFocus;   // evaluateSensedObjects
    /*  12 */ std::vector<FocusInformation> m_fireFocus;     // evaluateFireObjects
    // neither is A gap - the constructor skips them and `reset` writes both. A field the
    // constructor misses is not necessarily unused; check the reset before calling it dead.
    // the focus quadrant of the current quadrant plan (createPlan 0x1BE850). [INFERRED] names.
    /*  24 */ int m_int24;                 // reset: -1; the focus quadrant
    /*  28 */ float m_focusWeight;         // reset: 0
    /*  32 */ Vec3 m_focusPosition;        // ctor: 0
    /*  44 */ bool m_flag44;               // reset: false; the focus quadrant was looked at
    // the eight-quadrant weight ring - the constructor's only container, and the only thing it
    // sizes. Eight zeroed floats; see the header block.
    /*  48 */ std::vector<float> m_quadrantWeights;
    // [INFERRED] Plan-target handle, cleared to zero and passed to BotMain when checking
    // plan invalidation; the neighboring words are also cleared.
    /*  60 */ unsigned int m_planTarget60;  // clearPlanTargetInformation: 0
    // insertPlanDirectionInformation's direction and weight; createPlan compares the weight with
    // the focus quadrant's and normalises the direction.
    /*  64 */ Vec3 m_planDirection;        // ctor: 0
    /*  76 */ float m_planWeight;          // clearPlanTargetInformation: 0
    // BAPAMergeSoldierInputs::execute (ELF 0x1D3F00) reads +0x50/+0x54 as floats (a Vec2),
    // +0x58..+0x60 as a Vec3 and compares +0x64 with -1 (a handle); BAPAMoveTo::setIsSprinting
    // (0x1D5660) writes +0x58 / +0x64. [INFERRED] names. The constructor zeroes +80..+96;
    // reset zeroes +80 / +84; +100 is never written by either.
    /*  80 */ Vec2 m_moveDirection;
    /*  88 */ Vec3 m_sprintFocus;
    /* 100 */ unsigned int m_sprintFocusTarget;
    /* 104 */ BotSenses* m_senses;         // new(0x94) - the mechanism this agent steers
    /* 108 */ BotMain*   m_bot;            // the ctor argument
    // the whole tail is `reset`'s, not the constructor's - see the note at +24.
    /* 112 */ double m_double112;          // reset: the game time at reset
    /* 120 */ bool m_flag120;              // reset: false; update re-plans at once while set
    /* 121 */ bool m_flag121;              // reset: false; evaluateFireObjects: enemy fire booked
    /* 122 */ bool m_flag122;              // reset: false; disallowOverride: false; the override
    /* 123 */ char m_pad123;
    /* 124 */ double m_double124;          // reset: 0.0
    /* 132 */ bool m_flag132;              // reset: false; disallowOverride(b) stores b
    /* 133 */ bool m_flag133;              // reset: false; createPlan: the focus is ahead of the move
    /* 134 */ bool m_flag134;              // ctor: false
    /* 135 */ char m_pad135;
    // the plan itself - one owned statement, guarded everywhere it is touched.
    /* 136 */ BotScript* m_plan;           // ctor: 0
    // Set true by `reset` and again by `deletePlan`, and nothing recovered so far reads it.
    // Meaning [UNRESOLVED]; the name stays neutral rather than guess at `m_planInvalid`.
    // executePlan: set when the plan failed or did not finish (failed || !ok).
    /* 140 */ bool m_flag140;              // reset: true, deletePlan: true
    /* 141 */ char m_pad141[3];
    /* 144 */ BAPContext* m_bapContext;    // new(4) - shared vocabulary with BotMain's own
    /* 148 */ bool m_flag148;              // reset: true; createPlan: a quadrant (not target) plan
    /* 149 */ char m_pad149[3];
    // the time between two re-plans: reset a random float in [0.5, 1.0), calculateCurrentPeriod
    // a random float in a range that depends on what the bot does.
    /* 152 */ float m_currentPeriod;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SENSINGAGENT_H
