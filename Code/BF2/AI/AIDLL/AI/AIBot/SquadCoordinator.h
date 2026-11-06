// Coordinates squad membership, leadership and its owned formation.
// [FRONTIER] Leader-change movement remains incomplete.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_SQUADCOORDINATOR_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SQUADCOORDINATOR_H

#include <list>
#include <vector>

#include "../../../../Math/Vec3.h"
#include "BotSquad.h"                  // BotSquad::OrderType

namespace dice {
namespace hfe {
enum KitType : int;
namespace ai {

class SquadFormation;
class SquadMember;
class IAIPlayer;
class AIStrategicArea;
class AIStrategicObject;
class Bot;
class IAIObject;
enum StrategicStrengthType : int; // Attested type name; enum labels remain unrecovered.

class SquadCoordinator
{
public:
    // attested nested type name: `std::list<SquadCoordinator::SquadMemberFire*>` insert/erase/clear
    // are instantiated. on the ELF (`shotsFired(int, const Vec3&)`, ELF 0x1A5F00, allocates 0x44),
    // four Vec3s zeroed by the allocation; the scalar members are written field by field. Roles
    // come from shotsFired, getFiringDir, removeSquadMemberFire and updateShotsFired; names are
    // [INFERRED].
    struct SquadMemberFire
    {
        /* +0x00 */ double m_time;           // expiry: game time + 10 s, restamped on each shot
        /* +0x08 */ Vec3 m_v8;               // the latest shot's direction; getFiringDir answers it
        /* +0x14 */ Vec3 m_v20;              // the direction at the last re-path (turns past dot 0.1)
        /* +0x20 */ int m_playerId;          // the shotsFired int; the search/removal key
        /* +0x24 */ SquadMember* m_member;   // getSquadMember(playerId, index), borrowed
        /* +0x28 */ Vec3 m_v40;              // shooter position at the latest shot
        /* +0x34 */ Vec3 m_position;         // shooter position at the first shot; the 5 m anchor
        /* +0x40 */ int m_i64;               // the shooting bot's firing target (Bot +0x80), else -1
    };

    SquadCoordinator(int side, int squadIndex);
    ~SquadCoordinator();                            // ELF 0x1A3270 (D1)

    void update(double time);                    // complete order
    void updateSquadPosition();                  // complete
    void updateShotsFired();                     // complete
    void updateRepairRequest(double time);       // PE inlined at ; complete
    void updateNeedOrdersMessage(double time);   // radio-enqueue seam
    void updateIncomingMessages();               // radio-enqueue seam
    bool isSquadTakingFlag();                    // complete
    void updateSquadManeuver();                  // selected maneuver seams
    int chooseSquadManeuver();                   // ELF 0x1A56A0: 0 none, 1 alternate fire, 2 static flank
    void squadManeuverAlternateFire();           // ELF 0x1A4E90
    void squadManeuverStaticFlank();             // ELF 0x1A4060
    void updateSquadPaths();                     // complete invalidation loop
    int getSide();
    int getMissionStatus();                       // integer payload, enum spelling unresolved
    bool isSquadAssignedToArea(int areaIndex);
    int getStrategicStrength(StrategicStrengthType type); // nonnull vehicle seam
    void getSquadPosition(Vec3& position);       // inferred void
    SquadMember* getSquadLeader();
    int getNumberOfSquadMembers();
    int getSquadId();
    bool squadHasPosition();
    SquadFormation* getSquadFormation();         // borrowed
    const Vec3& getSquadGotoPos();               // borrowed, return spelling inferred
    void addSquadMember(IAIPlayer* player);
    void updateSquadLeader();                   // change seam
    void unAssignSquad();                       // PE inlined at
    SquadMember* getSquadMember(int index);
    SquadMember* getSquadMember(int playerId, int& index);
    bool hasMemberWithKit(KitType type);
    // ELF 0x1A86D0: a member's pending request is (handle, action) (SquadMember::RequestAction).
    bool isAnySquadMemberRespondingToRequest(unsigned int handle, int action);
    // ELF 0x1A8730: how much `member` should answer the request (handle, action); see the .cpp.
    float requestResponseModifier(SquadMember* member, unsigned int handle, int action);
    bool isSquadAssignedToAnyArea();
    AIStrategicArea* getSquadStrategicAreaAssignment(); // borrowed
    void resetNeedOrderMessageTimer(double now);
    void reset();                                // ELF 0x1A3790
    void removeSquadMemberFire(SquadMember* member);   // ELF 0x1A64B0
    bool isSquadCloseToNonOwnedTarget();         // ELF 0x1A6EC0
    bool squadHasMission();                      // ELF 0x1A6B00: +0x78 == 2
    bool isSquadAvailable();                     // ELF 0x1A7FA0: +0x78 == 0
    void removeAll();                            // ELF 0x1A3930
    void destroySquadMember(int index);          // ELF 0x1A83C0
    bool getValidSaPos(SquadMember* member, Vec3& position);   // ELF 0x1A7BC0
    // [CHOSEN, not recovered] declaration placement; added by ai-w1-e-fire for
    // BBPFireInfantery::createPlanInternal (coord/ai-requests.md 2026-11-01). ELF 0x1A5D90.
    void shotsFired(int playerId, unsigned int target);
    bool getValidSaPos(SquadMember* member, AIStrategicArea* area, Vec3& position);   // ELF 0x1A7D30
    bool isValidPosition(SquadMember* member, const Vec3& position);   // ELF 0x1A6B60
    // The requested-mission record (+60..+95), copied out by value by getMissionRequest. Its
    // twin at +24..+59 is the accepted mission. [INFERRED] names.
    struct MissionRequest
    {
        /* +0x00 */ AIStrategicArea* m_area;
        /* +0x04 */ Vec3 m_position;
        /* +0x10 */ int m_type;            // BotSquad::OrderType, 4 = none
        /* +0x14 */ IAIObject* m_object;
        /* +0x18 */ double m_time;         // requestMission: the update time + 10 s lives at +132 instead
        /* +0x20 */ bool m_commander;      // the order came from the commander
    };
    MissionRequest getMissionRequest();                         // ELF 0x1A3A40
    void requestRepair(const Vec3& position);                   // ELF 0x1A4950
    bool acceptMission(SquadMember* member);                    // ELF 0x1A7160
    bool isPosOnWater(SquadMember* member, Vec3& position);     // ELF 0x1A4DC0
    bool getSquadMissionTargetPos(Vec3& position);              // ELF 0x1A6B20
    bool getRandomValidPosition(SquadMember* member, Vec3& position, float radius);   // ELF 0x1A79C0
    void requestMission(BotSquad::OrderType type, const Vec3& position, IAIObject* object, bool commander);   // ELF 0x1A6F30
    // ELF 0x1A69C0: the accepted order (+40 type, +44 object, +28 position within FLT_EPSILON per axis).
    bool isSameMission(BotSquad::OrderType type, const Vec3& position, IAIObject* object);
    const Vec3& getSquadLeaderGotoPos() { return m_v200; }     // ELF 0x1A7FE0: this + 0xC8
    int getSquadLeaderIndex() { return m_squadLeaderIndex; }                // ELF 0x1A5D70: +0x60
    bool isRepairRequested() { return m_b172; }                // ELF 0x1A49F0: movzbl +0xAC
    Vec3& getRepairRequestPos() { return m_v176; }             // ELF 0x1A4A00: this + 0xB0
    void setRogerThatPositionMessage(bool roger) { m_b220 = roger; }   // ELF 0x1A39F0: +0xDC
    bool getRogerThatPositionMessage() { return m_b220; }      // ELF 0x1A3A10: +0xDC
    bool isSquadRequestedToArea(int areaIndex);  // ELF 0x1A7F60
    void removeSquadMember(int index);           // ELF 0x1A82E0
    void removeSquadMember(SquadMember* member); // ELF 0x1A84F0

    // Dynamic flanking (no caller in libaidll.so), its line-of-sight and position helpers.
    void squadManeuverDynamicFlank();            // ELF 0x1A4250
    bool canSeePos(const Vec3& from, const Vec3& to);                          // ELF 0x1A4A10
    void getFlankingCoords(std::vector<Vec3>& coords);                        // ELF 0x1A4B10
    bool haveFlankingCoords() { return m_b156; }                              // ELF 0x1A4B50
    bool getRandomValidFlankPos(SquadMember* member, float radius, Vec3& position); // ELF 0x1A4B60
    int getSquadManeuver() { return m_i148; }                                 // ELF 0x1A5690
    // Fire records: shotsFired(int, const Vec3&) is SquadManager::shotsFired's per-squad half.
    void shotsFired(int playerId, const Vec3& direction);                     // ELF 0x1A5F00
    bool anyFiring();                                                         // ELF 0x1A6570
    bool isFiring(int playerId);                                              // ELF 0x1A65B0
    SquadMember* getClosestFiringMember(SquadMember* member, float& distanceSquared);
    Vec3 getFiringDir(SquadMember* member);                                   // ELF 0x1A6820
    // Repair confirmation (BBPBridgeRepair).
    void confirmRepairRequested() { m_b196 = true; }                          // ELF 0x1A49D0
    bool haveConfirmedRepairRequest() { return m_b196; }                      // ELF 0x1A49E0
    // The requested (not the accepted) mission is (type, position within FLT_EPSILON, object).
    bool isSameRequest(BotSquad::OrderType type, const Vec3& position, IAIObject* object); // ELF 0x1A6A60
    SquadMember* getBotInSquad();                // ELF 0x1A7940: the first bot member, or null
    std::list<AIStrategicObject*> getListOfAreaNavPoints();                   // ELF 0x1A7E70: a copy
    // Appends every member's current vehicle (IAIPlayer +0x54) that is not null.
    void getSquadIgnoreList(std::vector<IAIObject*>& ignore);                 // ELF 0x1A8580
    bool squadHasHumanPlayer();                  // ELF 0x1A8680: any member that is not a bot

    // layout, on the ELF, read store by store out of the constructor
    // The [UNRESOLVED] gaps hold position and claim nothing; the constructor never writes them.
    /*   +0 */ std::vector<SquadMember*> m_squadMembers;   // `getSquadMember` indexes it, unchecked
    /*  +12 */ std::list<SquadMemberFire*> m_list12;       // owned fire records; see SquadMemberFire
    /*  +16 */ int m_squadIndex;               // = the constructor's second argument
    /*  +20 */ int m_side;                     // = the first argument; `getSide` returns it
    /*  +24 */ AIStrategicArea* m_strategicAreaAssignment; // [INFERRED] name; ELF +0x18 / PE +0x28, borrowed; initially null
    /*  +28 */ Vec3 m_v28;                     // zeroed [UNRESOLVED]
    /*  +40 */ int m_i40;                      // = 4 [UNRESOLVED]
    /*  +44 */ IAIObject* m_i44;               // = 0; acceptMission: the request's object. DICE: m_mission.target
                                           // (AIDLL_r assert "m_mission.target == 0", 0x101BA880)
    /*  +48 */ double m_d48;                   // = -20.0 a "never happened" sentinel; acceptMission: the game time
    /*  +56 */ bool m_b56;                     // never written by the constructor; acceptMission: the request's commander flag
    // isSquadRequestedToArea (ELF 0x1A7F60) compares the area index at +0xF0 of this pointer.
    /*  +60 */ AIStrategicArea* m_requestedArea; // = 0 [CHOSEN] name
    /*  +64 */ Vec3 m_v64;                     // zeroed [UNRESOLVED]
    /*  +76 */ int m_i76;                      // = 4 [UNRESOLVED] - the twin of m_i40
    /*  +80 */ IAIObject* m_requestedObject;   // = 0; requestMission's object [INFERRED] name
    /*  +84 */ double m_d84;                   // never written by the constructor; getMissionRequest copies it
    /*  +92 */ bool m_b92;                     // requestMission: the order came from the commander. DICE:
                                           // m_missionRequest.isCommanderOrder (AIDLL_r assert, 0x101BA880)
    /*  +96 */ int m_squadLeaderIndex;         // leader member index, initially -1. Name: AIDLL_r assert
                                           // "m_squadLeaderIndex >= 0" in getSquadLeaderIndex (0x101B8910, PE +0x78)
    /* +100 */ SquadFormation* m_formation;    // new SquadFormation(this)
    /* +104 */ std::list<AIStrategicObject*> m_list104; // the area nav points; getListOfAreaNavPoints copies it
    /* +108 */ Vec3 m_v108;                    // zeroed twice - once before the formation, once after
    // 0 = available (isSquadAvailable, ELF 0x1A7FA0), 2 = has a mission (squadHasMission).
    /* +120 */ int m_missionStatus;            // = 0. Name: AIDLL_r assert "m_missionStatus == MSOnMission"
                                           // (getSquadMissionTargetPos 0x101B8B00 compares PE +0x98 with 2)
    /* +124 */ double m_d124;                  // = 0.0 [UNRESOLVED]
    /* +132 */ double m_d132;                  // = 0.0 [UNRESOLVED]
    /* +140 */ double m_d140;                  // = 0.0 [UNRESOLVED]
    /* +148 */ int m_i148;                     // selected maneuver payload; initially 0
    /* +152 */ int m_i152;                     // = 0 [UNRESOLVED]
    /* +156 */ bool m_b156;                    // = false [UNRESOLVED]
    /* +157 */ bool m_b157;                    // = false [UNRESOLVED]
    /* +160 */ std::vector<Vec3> m_positions;  // seeded with two zero vectors - see the header
    // Repair request state: position, a 60-second game-time deadline, and confirmation flag.
    // Expiration clears both request and confirmation.
    /* +172 */ bool m_b172;                    // = false; isRepairRequested
    /* +176 */ Vec3 m_v176;                    // zeroed; getRepairRequestPos returns its address
    /* +188 */ double m_d188;                  // request deadline; the constructor never writes it
    /* +196 */ bool m_b196;                    // = false; haveConfirmedRepairRequest
    /* +200 */ Vec3 m_v200;                    // zeroed twice, like m_v108; getSquadLeaderGotoPos
    /* +212 */ double m_needOrderMessageTimer; // = IBFTimer::getGameTime at construction
    /* +220 */ bool m_b220;                    // = false; get/setRogerThatPositionMessage
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SQUADCOORDINATOR_H
