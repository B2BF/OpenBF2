// Squad member state and coordinator association.
// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SquadMember operations.

#ifndef AI_AIDLL_AI_AIBOT_SQUADMEMBER_H      // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SQUADMEMBER_H

#include "../../../../Math/Vec3.h"

namespace dice {
namespace hfe {
namespace ai {

class IAIPlayer;
class Bot;
class SquadCoordinator;
class Information;
class WaypointList;

class SquadMember
{
public:
    SquadMember(SquadCoordinator* squad, IAIPlayer* player,
                int squadIndex, int side, int memberIndex);
    ~SquadMember();

    IAIPlayer* getPlayer();
    SquadCoordinator* getSquad();
    int getPlayerId();
    bool isBot();
    float getEstimatedArrival() { return m_estimatedArrival; }   // ELF 0x1B5550: flds +0x20
    bool isAtFormationSlot() { return m_flag30; }              // ELF 0x1B5540: movzbl +0x30
    int getSide() { return m_side + 1; }                       // ELF 0x1B5680: +0x1C, incl
    bool isTakingFlag();                                       // ELF 0x1B3810
    Bot* getBot();                           // borrowed
    int getMemberIndex();
    void setMemberIndex(int index);          // ELF 0x1B56C0: +0x04
    bool isCloseToNonOwnedTarget() const;    // ELF 0x1B1BF0
    bool isCloseToNonOwnedTarget(float, float) const; // ELF 0x1B1D10, squared horizontal thresholds
    void setIsSquadLeader(bool leader);
    bool isSquadLeader();
    bool isMoving();                         // returns m_moved
    void setIsIdle(bool idle) { m_flag49 = idle; }      // ELF 0x1B3950: +0x49
    bool getIsIdle() { return m_flag49; }               // ELF 0x1B3960
    bool isAvoidingArtillery() { return m_flag4B; }     // ELF 0x1B2E90: +0x4B
    void setHavePath(bool havePath);
    void setForcePathUpdate(bool force);
    void moveToAssistSquadMember(SquadMember* member, const Vec3& position);
    Vec3 getRandomAssistPos(SquadMember* member, const Vec3& position, const Vec3& direction);
    SquadMember* getAssistFiringSquadMember(Vec3& position);
    bool canDamageTarget(unsigned int target);
    void update(double time);                // bounded dependent calls
    void updateFormationStatus();
    void reset();                            // ELF 0x1B1930
    void removeRequest();                     // ELF 0x1B1990
    void squadMemberKilled();                 // ELF 0x1B1E60
    bool avoidArtillery(Vec3 position, double sinceStart, int side);   // ELF 0x1B2840 [FRONTIER]
    bool getPosition(Vec3& position);         // copies even if invalid
    bool isAllowedInFormation();             // nonnull vehicle [FRONTIER]
    void updateMemberPos();                  // nonnull vehicle [FRONTIER]
    bool isMobile();                         // owner query [FRONTIER]
    void moveWithFormation();                // ELF 0x1B3AC0
    void moveToSlotPos(const Vec3& position, Information* information);   // ELF 0x1B3FF0
    void moveWithoutFormation();             // ELF 0x1B3E20
    void moveToGoalPos(Information* information);   // ELF 0x1B4240
    bool isSameWaypointList(WaypointList* current, WaypointList* candidate);   // ELF 0x1B5120
    float getVehicleRadius();

    // ELF 0x1B5620: +0x2C.
    float getMaxSpeed() { return m_maxSpeed; }
    // ELF 0x1B39C0: the request (+0x34) back to action 0, handle -1.
    void clearRequestAction() { m_request->m_action = 0; m_request->m_handle = 0xFFFFFFFFu; }
    // ELF 0x1B3980: the pending request (+0x34) becomes this action (+0) and handle (+4). The image
    // takes the SquadMember::RequestAction enum; the tree keeps it as int like isSameRequest.
    void setCurrentRequestAction(int action, unsigned int handle)
    { m_request->m_action = action; m_request->m_handle = handle; }
    enum RequestAction : int;
    void getCurrentRequestAction(RequestAction& action, unsigned int& handle);
    // ELF 0x1B39E0: the pending request (+0x34) has this handle (+4) and action (+0).
    bool isSameRequest(unsigned int handle, int action)
    { return m_request->m_handle == handle && m_request->m_action == action; }
    // ELF 0x1B56B0: +0.
    int getSquadId() const { return m_squadIndex; }

    // [INFERRED] carrier name. The two fields are attested by setCurrentRequestAction
    // its first parameter is the RequestAction enum, its second unsigned int.
    struct RequestState
    {
        int m_action;
        unsigned int m_handle;
    };

    // Offsets below are ELF. PE matches through +0x20, then adds four bytes of double alignment.
    /* +0x00 */ int m_squadIndex;
    /* +0x04 */ int m_memberIndex;
    /* +0x08 */ bool m_isSquadLeader;
    /* +0x0C */ int m_playerId;
    /* +0x10 */ IAIPlayer* m_player;
    /* +0x14 */ Bot* m_bot;
    /* +0x18 */ SquadCoordinator* m_squad;
    /* +0x1C */ int m_side;                  // zero-based, unlike the engine player's side
    /* +0x20 */ float m_estimatedArrival;    // getEstimatedArrival
    /* +0x24 */ double m_time24;             // last path reconsideration time; initialized to zero
    /* +0x2C */ float m_maxSpeed;            // getMaxSpeed
    /* +0x30 */ bool m_flag30;               // moveWithFormation: within sqrt(10) of the slot [INFERRED]
    /* +0x31 */ bool m_havePath;
    /* +0x34 */ RequestState* m_request;
    /* +0x38 */ SquadMember* m_assistMember; // [INFERRED] member currently being assisted
    /* +0x3C */ Vec3 m_position;
    /* +0x48 */ bool m_positionValid;
    /* +0x49 */ bool m_flag49;               // set/getIsIdle; moveWithFormation moves an idle member to its slot
    /* +0x4A */ bool m_flag4A;               // setHavePath writes the inverse of its argument
    /* +0x4B */ bool m_flag4B;               // isAvoidingArtillery
    /* +0x4C */ float m_formationDistanceSq; // [INFERRED] name, starts at 900.0
    /* +0x50 */ bool m_moved;
    /* +0x54 */ Vec3 m_vector54;             // [UNRESOLVED]
    /* +0x60 */ Vec3 m_vector60;             // the last goal moveToGoalPos ordered [INFERRED] meaning
};

#if defined(_MSC_VER)
static_assert(sizeof(void*) != 4 || sizeof(SquadMember) == 112, "SquadMember PE x86 layout");
#endif

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SQUADMEMBER_H
