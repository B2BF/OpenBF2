// Base interface and shared state for strategic objects. Preserve virtual declaration order.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECT_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECT_H

#include <list>
#include <set>
#include <string>

// Vec2 and Vec3 cannot be forward-declared in this tree: both are typedefs to
// `dice::hfe::Math::Vec2/Vec3`, so a local `struct Vec3;` is a redefinition with a different basic
// type (C2371). The header has to come in.
#include "../../../../Math/Vec3.h"
#include "../AIStrategic/StatusMap.h"

namespace dice {
namespace hfe {
namespace ai {

class EnemyStatistics;
class ISAI;
class Strategy;

class AIStrategicObject
{
public:
    // Strategic object classifications used by console scripts. Unknown values have no printed name.
    enum StrategicObjectType
    {
        Flank          = 1,
        Centre         = 2,
        Base           = 3,
        Close          = 4,
        Remote         = 5,
        AirSpawner     = 6,
        LandSpawner    = 7,
        NavalSpawner   = 8,
        SoldierSpawner = 9,
        StrongPoint    = 10,
        ChokePoint     = 11,
        Bridge         = 12,
        AirField       = 13,
        SupplyPoint    = 14,
        Route          = 15,
        ControlPoint   = 16,
        North          = 17,
        West           = 18,
        South          = 19,
        East           = 20,
        // 21 has no name in the writer
        Front          = 22,
        Safe           = 23,
        Enemy          = 24,
        // 25 has no name in the writer
        NeutralType    = 26     // spelled "Neutral" by the writer; renamed here only because
                                // `StatusMap::Status::Neutral` would collide at this scope
    };

    AIStrategicObject(float radius, float f148);

    // the vtable, , thirty-three slots
    virtual ~AIStrategicObject();                                      // slots 0/1,

    virtual int getType() const;                                       // slot 2,
    virtual const Vec2& getPosition(int side) const;                   // slot 3,
    virtual void setPosition(int side, const Vec2& position);          // slot 4,

    // Slots 5..17, 21..25, 28, 31 and 32 are `__cxa_pure_virtual` in this class's table. Every name
    // and parameter list below is taken from the matching slot of `AIStrategicArea`'s vtable; the
    // return types are [INFERRED] from whether the override leaves its result in eax or in st0.
    virtual Vec3 getOrderPos(int a, int b) const = 0;                  // slot 5
    virtual float updateTemperature(const Strategy* strategy, int side) = 0;              // slot 6; SAI discards st0
    virtual void update(int side, ISAI* sai, bool& changed) = 0;                          // slot 7; paired Area body has no return
    virtual int  getChangedAttackDefence(int side) = 0;                                   // slot 8
    virtual float getControlPointTakeOverCounter() = 0;                                  // slot 9; ST0 float chain
    virtual bool isEnemyTakingOverBase(int side) = 0;                                     // slot 10
    virtual float calculateDefenseNeed(int side) = 0;                                      // slot 11
    virtual float calculateAttackValue(int side) = 0;                                      // slot 12
    virtual float calculateAttackCost(int side) = 0;                                       // slot 13
    virtual bool isNeighbour(AIStrategicObject* object) const = 0;                        // slot 14
    virtual bool hasNeighbourWithStatus(StatusMap::Status status, int side) const = 0;    // slot 15
    virtual bool isNeighbouringObject(AIStrategicObject* object) const = 0;               // slot 16
    virtual bool hasNeighbouringObjectWithStatus(StatusMap::Status status,
                                                 int side) const = 0;                     // slot 17

    virtual int  getObjectTypeFlags() const;                           // slot 18,
    virtual bool hasObjectTypeFlag(StrategicObjectType type) const;    // slot 19,
    virtual bool hasObjectTypeFlags(int flags) const;                  // slot 20,

    virtual int  getTacticalStatus(int side) const = 0;                                   // slot 21
    virtual bool isAreaTakeable() = 0;                                                    // slot 22
    virtual bool collectSquadResources(int side, ISAI* sai) = 0;                          // slot 23
    virtual int  collectAdditionalSquadResources(int amount, int side, ISAI* sai) = 0;    // slot 24
    virtual bool releaseSurplus(int side) = 0;                                            // slot 25

    virtual float gravity(const Vec3& position, int side);             // slot 26,
    virtual float gravity(const Vec2& position, int side);             // slot 27,

    virtual bool retainBot(int side, int a, ISAI* sai) = 0;                               // slot 28

    virtual float getXExtent(int side) const;                          // slot 29,
    virtual float getZExtent(int side) const;                          // slot 30,

    virtual void setAsDirty(int side) = 0;                                                // slot 31
    // void: AIStrategicArea::orderBot (ELF 0x33C340) passes through its arms' eax, which neither
    // arm sets.
    virtual void orderBot(int side, const std::list<AIStrategicObject*>& objects,
                          int a, ISAI* sai) = 0;                                          // slot 32

    int getNumberOfAssignedUnits(int side) const;
    // ELF 0x341ED0 / 0x341F40: the per-side statuses for owner 1 or 2 (any other value marks
    // only the second pair Neutral); getSide answers 1, 2 or 0 from the second pair.
    void setSide(int side);
    int getSide();
    void setPriority(int priority, int side);                 // ELF 0x341F70
    int getPriority(int side) const;                          // ELF 0x341F90
    // ELF 0x342110: hands an assigned unit back through the side's SAI; false when unassigned.
    bool releaseAssignedUnit(int unit, int side);
    // ELF 0x3421C0: whether the unit under the iterator drives a known unit (its current
    // equipment's information has a unit part).
    bool bookBot(bool attacker, int side, std::set<int>::iterator& it);
    int getNumberOfAvailableBots(bool attackers, int side) const;    // ELF 0x342330
    int getNumberOfAvailableAttackers(int side) const;               // ELF 0x342360
    int getNumberOfAvailableDefenders(int side) const;               // ELF 0x342380
    int getNumberOfPresentUnits(int side) const;                     // ELF 0x3423C0
    // ELF 0x3425F0: m_statistics[side + 1], the second (size-2) pair. Reference spelling
    // [INFERRED], as getBattleStatistics.
    EnemyStatistics& getStrategicStatistics(int side) const;
    // ELF 0x342820 / 0x3428B0: empty the side's available set; answer the count it held
    // (the image reloads the pre-clear size into eax). [INFERRED] int.
    int unregisterAllAvailableAttackers(int side);
    int unregisterAllAvailableDefenders(int side);
    // ELF 0x342D40 / 0x342D90 / 0x342DD0: set a vehicle-group bit of +0xF4, which a -1 ("all")
    // mask first resets to 0; the equipment and name forms map through IAISettings.
    void addAllowedUnit(int equipment);
    void addAllowedVehicleGroup(int group);
    void addAllowedVehicleGroup(const std::string& group);
    int getAllowedVehicleGroups() const;                      // ELF 0x342E20
    bool isAllowedUnit(int equipment) const;                  // ELF 0x342E30
    void addExpelledUnit(int equipment);                      // ELF 0x342EC0
    int getExpelledVehicleTypes() const;                      // ELF 0x342EE0
    // ELF 0x342F10: erases the group from m_set256; false when absent.
    bool removeGrouping(int group);
    bool isAttack(int side) const;
    void setAttack(bool attack, int side);                     // ELF 0x341FB0
    // ELF 0x342940: assigned squad strength / resource need (integer division), 1 without need.
    float getAssignedPartOfWantedStrength(int side) const;
    // Borrowed mutable statistics at m_statistics[side - 1]; reference spelling is [INFERRED].
    EnemyStatistics& getBattleStatistics(int side) const;
    int releaseAssignedResources(int side);
    // ELF 0x3422A0..0x342300: the per-side available-unit sets (m_set20 attackers, m_set44
    // defenders), one-based side. Reference spelling [INFERRED].
    std::set<int>& getAvailableBots(bool attackers, int side);
    std::set<int>& getAvailableAttackers(int side);
    std::set<int>& getAvailableDefenders(int side);
    // ELF 0x3426E0 / 0x342780: erase the unit under the iterator and advance it; false at end().
    bool unregisterAvailableAttacker(std::set<int>::iterator& it, int side);
    bool unregisterAvailableDefender(std::set<int>::iterator& it, int side);
    bool registerAvailableAttacker(int unit, int side);        // ELF 0x341FF0; always true
    bool registerAvailableDefender(int unit, int side);        // ELF 0x342040; always true
    bool unregisterAvailableAttacker(int unit, int side);      // ELF 0x342600
    bool unregisterAvailableDefender(int unit, int side);      // ELF 0x342670
    // ELF 0x342310 / 0x3423E0..0x342550: assigned (m_set68) and present (m_set92) units.
    std::set<int>& getAssignedUnits(int side);
    std::set<int>& getPresentUnits(int side);
    bool isPresentUnit(int unit, int side) const;
    std::pair<std::set<int>::iterator, bool> setAsPresentUnit(int unit, int side);
    void removeAsPresentUnit(std::set<int>::iterator& it, int side);
    // ELF 0x3424C0: find in the present set, erase when found; answers the find result.
    std::set<int>::iterator removeAsPresentUnit(int unit, int side);
    // ELF 0x342E80 / 0x342EF0: bit tests of the vehicle-group mask (+0xF4) and the expelled
    // equipment mask (+0xF8).
    bool isAllowedVehicleGroup(int group) const;
    bool isExpelledUnit(int equipment) const;
    // ELF 0x342EA0: `mov 0xf4(%eax),%eax; and %edx,%eax; setne` - any bit of a group mask.
    bool allowsSomeVehicleGroups(int groupMask) const;
    // ELF 0x342B80: a uniform point in the side's extent box (slots 29/30) scaled by `scale`,
    // around the side's position, at height m_f148.
    void randomizePos(int side, float scale, Vec3& position);
    // ELF 0x342990: position += (sin a, 0, cos a) * scale * (0.5 + draw * (spread - 0.5)), a a
    // uniform angle.
    void randomizeControlpointPos(float scale, float spread, Vec3& position);

    // [INFERRED] accessor names/forms. SAI::updateStates and the area's neighbour scans inline
    // these loads: status is at +0x70 + 4*side and index at +0xF0 in both shipped x86 ports.
    StatusMap::Status getStatus(int side) const { return m_status[side - 1]; }
    int getIndex() const { return m_index; }
    void resetStatus();                          // ELF 0x342FC0

    // [CHOSEN, not recovered] Public fields allow manager access; the original may have
    // used friendship instead. Offsets document ELF layout and do not constrain modern STL sizes.
    /*   +4 */ EnemyStatistics* m_statistics[4];   // new EnemyStatistics(6), (6), (2), (2) in order
    // Per-side strategic-object sets. [UNRESOLVED] Exact roles of these four pairs remain
    // unidentified; at least two pairs represent neighboring areas and objects.
    /*  +20 */ std::set<int> m_set20[2];
    /*  +44 */ std::set<int> m_set44[2];
    /*  +68 */ std::set<int> m_set68[2]; // assigned units, one-based side accessor
    /*  +92 */ std::set<int> m_set92[2];

    // All four set to 2. `StatusMap::Status(2)` is `Neutral`, which is the only reason this member
    // carries that name - [INFERRED], on the strength of one coincidence.
    /* +116 */ StatusMap::Status m_status[4];

    // one-based: `getPosition(side)` is `m_position[side - 1]`. See the header note.
    /* +132 */ Vec2 m_position[2];

    /* +148 */ float m_f148;        // the constructor's second argument; -666.0f from every caller
    // Console basicTemperature is element zero; update adds it to both temperature pairs.
    // Keep the shared array spelling used by side-radius readers in other subsystems.
    enum { kBasicTemperature = 0 };
    /* +152 */ float m_size[3];     // [kBasicTemperature] baseline temperature; [1..2] side radii
    /* +164 */ float m_f164[4];     // all four set to the radius argument [UNRESOLVED]
    // Paired Area::update reads/writes these as floats, not the formerly guessed ints.
    // [INFERRED] names; one-based side pairs, all zeroed by the base constructor.
    /* +180 */ float m_needCurveInput[2];
    /* +188 */ float m_friendlyRatio[2];
    /* +196 */ float m_friendlyBattleStrength[2];
    /* +204 */ float m_enemyBattleStrength[2];
    /* +212 */ int m_unresolved212[2]; // zeroed; the per-side priority (setPriority / getPriority)
    /* +220 */ int m_resourceNeed[2];
    /* +228 */ int m_assignedSquadStrength[2];
    /* +236 */ bool  m_attack[2];   // both true; isAttack(side) reads [side - 1]
    /* +240 */ int   m_index;       // not initialised here - addStrategicObject assigns it
    /* +244 */ int   m_i244;        // -1; allowed vehicle-group bit mask (isAllowedVehicleGroup)
    /* +248 */ int   m_i248;        // 0; expelled equipment bit mask (isExpelledUnit)
    /* +252 */ int   m_category;    // 1 until createFromControlPoint or create sets it
    /* +256 */ std::set<int> m_set256;
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AISTRATEGICENTITY_AISTRATEGICOBJECT_H
