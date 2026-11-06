// Routes game events into the AI module.
// Preserve virtual declaration order. Notification callbacks forward without a scalar result.

#ifndef AI_AIDLL_AI_MAIN_AIEVENTCOURIER_H
#define AI_AIDLL_AI_MAIN_AIEVENTCOURIER_H

#include <string>
#include <vector>
#include <list>
#include <set>
#include <map>

#include "AIDllTypes.h"

namespace dice {
namespace hfe {
namespace ai {

// Registers the first IAIEventCourier in the simple-singleton registry. It has no roster
// static; destruction looks it up by name. Preserve virtual method order.
class IAIEventCourier
{
public:
    IAIEventCourier();
    virtual ~IAIEventCourier();
    virtual void eventMissileLock(IAIObject *, world::MissileLockMessage *) = 0;
    virtual void eventPlayerSpawned(int) = 0;
    virtual void eventPlayerKilled(int, int) = 0;
    virtual void eventPlayerCreated(bool) = 0;
    virtual void eventChangedKit(KitType) = 0;
    virtual void eventPlayerDisconnect(int) = 0;
    virtual void eventPlayerEnterVehicle(int, IAIObject *, IAIObject *) = 0;
    virtual void eventPlayerExitVehicle(IAIObject *, int) = 0;
    virtual void eventDrawDebug() = 0;
    virtual void eventGameStart() = 0;
    virtual void event_roundEnd() = 0;
    virtual void event_worldReset() = 0;
    virtual void eventManDown(int, IAIObject *) = 0;
    virtual void eventRevived(int, IAIObject *) = 0;
    virtual void eventRadioMessage(const AIRadioMessageSettings&, int, int) = 0;
    virtual void event_mineRemoved(unsigned int) = 0;
    virtual void event_enemySpotted(IAIObject *, unsigned char) = 0;
    virtual void event_isRepaired(int) = 0;
    virtual void event_requestNewOrders(int, int) = 0;
    virtual void event_recievedAttackCommanderOrder(int) = 0;
    virtual void event_recievedDefendCommanderOrder(int) = 0;
    virtual void event_recievedMoveCommanderOrder(int) = 0;
    virtual void event_incomingArtillery(int, Vec2) = 0;
    virtual int event_receivedArtilleryRequest(int, int, Vec3) = 0;
    virtual int event_receivedUavRequest(int, int, Vec3) = 0;
    virtual int event_receivedSupplyRequest(int, int, Vec3) = 0;
    virtual int event_receivedVehicleDropRequest(int, int, Vec3) = 0;
    virtual void event_commanderOrderAccepted(int) = 0;
    virtual void event_commanderOrderRejected(int) = 0;
    virtual void event_appliedForCommander(int) = 0;
    virtual void event_acceptedCommander(int, int) = 0;
    virtual void event_resignAsCommander(int, int) = 0;
    virtual void eventNewSquadLeader(int, int) = 0;
    virtual void event_joinedSquad(int, int, int) = 0;
    virtual void event_removedFromSquad(int, int, int) = 0;
    virtual void event_recievedAttackSquadOrder(int) = 0;
    virtual void event_recievedDefendSquadOrder(int) = 0;
    virtual void event_recievedMoveSquadOrder(int) = 0;
    virtual void event_receivedRepairSquadOrder(int) = 0;
    virtual void event_squadOrderAccepted(int, int) = 0;
    virtual void event_squadOrderRejected(int, int) = 0;
    virtual void event_botTeamKilled(int, int) = 0;
    virtual void event_freeCameraFire(bool) = 0;
    virtual void event_removeAllSquadMembers() = 0;
    virtual void event_retrievePathfindingMapId(int &, bool, const Vec3&, const Vec3&, const Mat4&) = 0;
    virtual void event_pathfindingMapIdDestroyed(int, IAIObject *) = 0;
    virtual void event_pathfindingMapIdRestored(int) = 0;
    virtual void event_pathfindingMapIdRestoreOriginalMaterials(int, IAIObject *) = 0;
    virtual void event_bridgeDestroyed(IAIObject *, int) = 0;
    virtual void event_bridgeRepaired(IAIObject *) = 0;
    virtual void event_bridgeCreated(IAIObject *) = 0;
    virtual void event_bridgeRemoved(IAIObject *) = 0;
    virtual void event_levelLoaded() = 0;
    virtual void event_botSwitchSides(int) = 0;
    virtual void event_playerSwitchSides(int, int) = 0;
};

// AIEventCourier - sizeof == 4, from BF2AI::init step 11.
class AIEventCourier : public IAIEventCourier
{
public:
    AIEventCourier();
    virtual ~AIEventCourier();

    // IAIEventCourier
    virtual void eventMissileLock(IAIObject *, world::MissileLockMessage *);
    virtual void eventPlayerSpawned(int);
    virtual void eventPlayerKilled(int, int);
    virtual void eventPlayerCreated(bool);
    virtual void eventChangedKit(KitType);
    virtual void eventPlayerDisconnect(int);
    virtual void eventPlayerEnterVehicle(int, IAIObject *, IAIObject *);
    virtual void eventPlayerExitVehicle(IAIObject *, int);
    virtual void eventDrawDebug();
    virtual void eventGameStart();
    virtual void event_roundEnd();
    virtual void event_worldReset();
    virtual void eventManDown(int, IAIObject *);
    virtual void eventRevived(int, IAIObject *);
    virtual void eventRadioMessage(const AIRadioMessageSettings&, int, int);
    virtual void event_mineRemoved(unsigned int);
    virtual void event_enemySpotted(IAIObject *, unsigned char);
    virtual void event_isRepaired(int);
    virtual void event_requestNewOrders(int, int);
    virtual void event_recievedAttackCommanderOrder(int);
    virtual void event_recievedDefendCommanderOrder(int);
    virtual void event_recievedMoveCommanderOrder(int);
    virtual void event_incomingArtillery(int, Vec2);
    virtual int event_receivedArtilleryRequest(int, int, Vec3);
    virtual int event_receivedUavRequest(int, int, Vec3);
    virtual int event_receivedSupplyRequest(int, int, Vec3);
    virtual int event_receivedVehicleDropRequest(int, int, Vec3);
    virtual void event_commanderOrderAccepted(int); // guarded IAIMain forward
    virtual void event_commanderOrderRejected(int);
    virtual void event_appliedForCommander(int);
    virtual void event_acceptedCommander(int, int);
    virtual void event_resignAsCommander(int, int);
    virtual void eventNewSquadLeader(int, int);     // side then squad id
    virtual void event_joinedSquad(int, int, int);  // player id, squad id, side
    virtual void event_removedFromSquad(int, int, int);
    virtual void event_recievedAttackSquadOrder(int);
    virtual void event_recievedDefendSquadOrder(int);
    virtual void event_recievedMoveSquadOrder(int);
    virtual void event_receivedRepairSquadOrder(int);
    virtual void event_squadOrderAccepted(int, int);
    virtual void event_squadOrderRejected(int, int);
    virtual void event_botTeamKilled(int, int);
    virtual void event_freeCameraFire(bool);
    virtual void event_removeAllSquadMembers();
    // the eight below: /
    virtual void event_retrievePathfindingMapId(int &, bool, const Vec3&, const Vec3&, const Mat4&);   // IAIPathfinding forward
    virtual void event_pathfindingMapIdDestroyed(int, IAIObject *);                  // Debug(Error) if < 0, line 508
    virtual void event_pathfindingMapIdRestored(int);                                // IAIPathfinding forward
    virtual void event_pathfindingMapIdRestoreOriginalMaterials(int, IAIObject *);   // Debug(Error) if < 0, line 532
    virtual void event_bridgeDestroyed(IAIObject *, int);    // AIStrategicObjectManager forwards
    virtual void event_bridgeRepaired(IAIObject *);          // no singleton test
    virtual void event_bridgeCreated(IAIObject *);
    virtual void event_bridgeRemoved(IAIObject *);
    virtual void event_levelLoaded();
    virtual void event_botSwitchSides(int);
    virtual void event_playerSwitchSides(int, int);

};

} // namespace ai
} // namespace hfe
} // namespace dice

#endif // AI_AIDLL_AI_MAIN_AIEVENTCOURIER_H
