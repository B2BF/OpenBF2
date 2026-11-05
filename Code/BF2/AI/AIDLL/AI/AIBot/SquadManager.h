// Owns squad coordinators and side-specific squad state.
// [INFERRED] Source placement.

#ifndef AI_AIDLL_AI_AIBOT_SQUADMANAGER_H       // [INFERRED] guard style
#define AI_AIDLL_AI_AIBOT_SQUADMANAGER_H

#include <list>
#include <map>
#include <vector>
#include "../../../../Math/Vec3.h"
#include "BotSquad.h"                  // BotSquad::OrderType

namespace dice {
namespace hfe {
namespace ai {

class SquadCoordinator;   // (`operator new(0xE0)`); held only by pointer here
class AIStrategicArea;
class SquadMember;
class IAIPlayer;
// Attested enum name in both squad-strength ELF manglings. Enumerator names are not recovered.
enum StrategicStrengthType : int;

class SquadManager
{
public:
    SquadManager();
    ~SquadManager();                                // ELF 0x1ADD70 (D1)

    // Direct AIMain scheduling call, double by value; neither image materializes a result.
    void update(double time);
    void updateArtilleryStatus(double time);
    void updateCommanderMessages();
    void updateHumanPlayers(double time);   // bounded
    void setPlayerOnValidPosition(IAIPlayer* player, bool valid, double time);

    // player id then one-based side. All two callers in
    // each image discard the result; neither body materializes one independently of its last
    // updateSquadLeader call. void is the recovered no-result contract, not decompiler EAX.
    // The relocation branch retains a named stop before removeSquadMember's first mutation.
    void addSquadMember(int playerId, int side);

    // Engine notifications use one-based side and squad ids. These bodies materialize no
    // result; the engine/courier callers discard it on both ports.
    void newSquadLeader(int side, int squadId);
    void playerJoinsSquad(int playerId, int squadId, int side);
    SquadMember* getSquadMember(int playerId, int side);
    // ELF 0x1AF480: the member occupying the object (IPIUnit::getOccupyingPlayerId(side)), or null.
    SquadMember* getSquadMember(unsigned int handle, int side);
    // ELF 0x1B0440: any squad of the side has a member with this pending request.
    bool isAnySquadMemberRespondingToRequest(unsigned int handle, int action, int side);
    // ELF 0x1B0390: 0 without a squad, 1 when `member` already has this request, else the squad's
    // SquadCoordinator::requestResponseModifier. `action` is SquadMember::RequestAction.
    float requestResponseModifier(SquadMember* member, unsigned int handle, int action, int side);
    // [CHOSEN, not recovered] declaration placement; added by ai-w1-e-fire for
    // BBPFireInfantery::createPlanInternal (coord/ai-requests.md 2026-11-01). ELF 0x1B04B0.
    void shotsFired(int playerId, int side, unsigned int target);
    void shotsFired(int playerId, int side, const Vec3& direction);
    SquadCoordinator* getSquadFromPlayer(int playerId, int side);
    SquadCoordinator* getSquadFromPlayer(unsigned int handle, int side);
    bool isInSameSquad(int firstPlayer, int secondPlayer, int side);
    bool isInAnySquad(int playerId, int side);
    int getSquadFromPlayer(int playerId, SquadCoordinator*& squad);     // ELF 0x1AF6A0: the side found
    SquadMember* getSquadLeaderClosestTo(const Vec3& position);         // ELF 0x1AFBC0
    SquadCoordinator* getSquadFromId(int squadId, int side);            // ELF 0x1AF720, unchecked
    void setSquadReceivedOrder(BotSquad::OrderType type, int playerId, bool commander);   // ELF 0x1AF860
    void repeatOrder(int side, int squadId);                            // ELF 0x1AF960
    int getSquadStrengths(int side, int areaIndex, StrategicStrengthType type);

    // Clears messages and position records by erasing each container node individually.
    void clearCommanderMessages();
    void clearPlayerValidPositions();
    void unAssignSquads(int side);               // ELF 0x1AF800
    int unAssignSquads(int areaIndex, int side); // ELF 0x1AF750
    // [FRONTIER] declared by ai-strategic for SAI::checkCommanderUseArtillery; owner ai-bot
    // (coord/ai-requests.md). getArtillerySafeDistance is recovered (ELF 0x1AEE60: +0x98).
    bool  haveCampingHumans(int side, Vec2& position);            // ELF 0x1B0610
    bool  haveHumanPlayerOnOwnSide(int side);                     // ELF 0x1AE5B0
    float getArtillerySafeDistance();                             // ELF 0x1AEE60
    float getDistanceToClosestSquad(const Vec3& position, int side);   // ELF symbol; requested from ai-bot
    SquadCoordinator* getClosestAvailableSquad(AIStrategicArea* area, const Vec3& position, int side,
                                               bool attack, int& strength);   // ELF symbol; requested from ai-bot
    bool  friendliesCloseTo(Vec3 position, float radius, int side); // ELF 0x1B07F0
    void  removeAllSquads();                                       // ELF 0x1AE000
    void  destroySquadMember(int playerId, int side);             // ELF 0x1B0010
    void  removeSquadMember(int playerId, int side);              // ELF 0x1AFF20
    bool  isInArtilleryDangerZone(Vec3 position, int side, double& sinceStart, int& artillerySide);   // ELF 0x1B0B00
    Vec3  getArtilleryTargetPos(int side);                         // ELF 0x1B0D40
    // ELF 0x1B0D80 / 0x1B0EC0: the side's remembered flee position, one for soldiers, one for
    // vehicles (the member's current vehicle's information flag 0x200000).
    bool  haveValidArtilleryFleePos(SquadMember* member, Vec3& position);
    void  setValidArtilleryFleePos(SquadMember* member, Vec3 position);
    void  incomingArtillery(Vec3 position, int side);             // ELF 0x1B09A0
    void reset();                                // ELF 0x1AE100
    void playerRemovedFromSquad(int playerId, int squadId, int side);   // ELF 0x1B0180

    static SquadManager* m_instance;

    // Queued commander message. Field and enumerator names are inferred.
    // Native dueTime offset/record size: ELF +0xC/20 bytes, PE +0x10/24 bytes.
    enum CommanderMessageType : int;
    // ELF 0x1AE970: queued once per (side index, squad index), due after `delay` s of game time.
    void enqueueCommanderMessage(int sideIndex, int squadIndex, CommanderMessageType type, double delay);
    class CommanderMessage
    {
    public:
        CommanderMessageType m_type;
        int m_squadIndex;
        int m_sideIndex;
        double m_dueTime;
    };

    // Three Vec3 values, not containers: incomingArtillery
    // and setValidArtilleryFleePos store their scalar components.
    // [INFERRED] field names. ELF offsets below; PE offsets are 0,8,16,32,40,41,44,56.
    // Only the three positions are zeroed here. The manager tail clears incomingArtillery; both doubles
    // and the two validity flags remain unwritten by retail construction.
    class SideBlock
    {
    public:
        /*  +0 */ bool incomingArtillery;    // name: AIDLL_r assert (0x1012FC60)
        /*  +4 */ double m_artilleryStartTime;
        /* +12 */ Vec3 m_artilleryPosition{};
        /* +24 */ double m_artilleryDelay;
        /* +32 */ bool m_validFlee0;
        /* +33 */ bool m_validFlee1;
        /* +36 */ Vec3 m_fleePos0{};
        /* +48 */ Vec3 m_fleePos1{};
    };

private:
    // no vtable. The constructor stores no vptr on any path, so this class has no virtuals at
    // all - checked before writing, because a missing vptr shifts every offset below by four.
    /*   0 */ std::vector<SquadCoordinator*> m_Squads[2]; // ELF stride 12 / PE stride 16  // name: AIDLL_r assert "m_Squads[side - 1]" 0x101320A0 (PE +0)
    /*  24 */ int  m_squadCursor;         // = 0; [INFERRED] name, update uses unsigned comparison
    /*  28 */ int  m_sideCursor;          // = 1; [INFERRED] name, update alternates zero/nonzero
    // Per-side state blocks, each 60 bytes in the original layout. Construction clears
    // the leading flag and the same nine words in each block.
    // Two blocks, one per side: AIDLL_r asserts "m_artillery[side - 1].incomingArtillery" (0x1012FC60)
    // and "m_artillery[theSide].incomingArtillery || m_artillery[otherSide].incomingArtillery"
    // (0x1012FD90; PE stride 0x48, flag at +0x28 + index * 0x48).
    /*  32 */ SideBlock m_artillery[2];     // +32..+91, +92..+151
    /* 152 */ float  m_float152;          // = 40.0f (bit pattern )
    /* 156 */ double m_double156;         // = 25.0
    // with a bare `operator delete` one at a time.
    /* 164 */ std::list<CommanderMessage*> m_commanderMessages;
    // Maps player ids to their first invalid-position timestamp; a valid position removes the entry.
    /* 168 */ std::map<int, double> m_playerValidPositions;
    // The original ELF map includes its empty comparator at +176 and ends at +180.
};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_AIBOT_SQUADMANAGER_H
