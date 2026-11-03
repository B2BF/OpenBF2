// static init: ELF 0x3998D0 (__tcf_0 0x399930, _GLOBAL__I 0x399960): <iostream>'s Init object only.
#include <iostream>

#include "AIEventCourier.h"

#include <string>

#include "../../../../Core/ClassManager.h"
#include "../../../../Core/Debug.h"                            // the map-part events' fatal Debugs
#include "CommonSingletonServer.h"
#include "IAIPathfinding.h"                                  // IAIPathfindingSetup::initMaps
#include "../AIStrategicEntity/AIStrategicObjectManager.h"   // postLoadInitAreas
#include "AIMain.h"                                            // IAIMain::spawnBots
#include "../AIBot/SquadManager.h"
#include "../AIBot/SquadMember.h"                              // squad notification forwarders
#include "../AIUtils/DebugRendering/AIRenderUtil.h"             // event_worldReset
#include "../DrawPrimitives/DPManager.h"                        // event_worldReset
#include "CommunicationHub.h"                                   // event_worldReset
#include "IBotManager.h"                                        // the bot-state forwarders
#include "AILODManager.h"                                       // eventPlayerDisconnect
#include "AIConsole.h"                                          // event_freeCameraFire
#include "../AIBot/BotMain.h"                                   // event_isRepaired
#include "../AIStrategic/SAI.h"                                 // the commander and request events
#include "../AIKnowledge/Information.h"                          // eventManDown
#include "../AIKnowledge/InformationManager.h"
#include "../AIKnowledge/IPIPhysical.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/IAIObjectPlugIn.h"   // IAIObjectUnit - eventMissileLock
#include "../../../AIInterface/BFKnowledge/BFGame.h"            // eventDrawDebug
#include "../../../AIInterface/BFKnowledge/BFTerrain.h"         // event_incomingArtillery

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// ELF 0x3E6280 / 0x3E6390.
IAIEventCourier::IAIEventCourier()
{
    if (!classManager->getSimpleSingleton("IAIEventCourier"))
        classManager->registerSimpleSingleton("IAIEventCourier", this);
}

// ELF 0x3E64A0 / 0x3E6590 / 0x3E6680.
IAIEventCourier::~IAIEventCourier()
{
    if (classManager->getSimpleSingleton("IAIEventCourier") == this)
        classManager->unregisterSimpleSingleton("IAIEventCourier");
}

// ELF 0x398570 / 0x3985B0. Stateless; construction only initializes the base.
AIEventCourier::AIEventCourier()
{
}

// ELF 0x3985F0 / 0x398630 / 0x398670. Member and base teardown only.
AIEventCourier::~AIEventCourier()
{
}

// ELF 0x3986B0: the locked object's unit plug-in (IAIObject +0x98 getUnit) gets
// IAIObjectUnit +0x78 setMissileLockStatus(1). The message pointer is not read.
void AIEventCourier::eventMissileLock(IAIObject* object, world::MissileLockMessage*)
{
    if (object)
    {
        IAIObjectUnit* const unit = object->getUnit();
        if (unit)
            unit->setMissileLockStatus(1);
    }
}

// ELF 0x3986F0: IBotManager +0x38 setBotIsAlive, unguarded.
void AIEventCourier::eventPlayerSpawned(int playerId)
{
    IBotManager::instance->setBotIsAlive(playerId);
}

// ELF 0x398780: IBotManager +0x34 setBotIsDead (unguarded), then the guarded squad member's
// squadMemberKilled. The second argument is the side the squad lookup uses.
void AIEventCourier::eventPlayerKilled(int playerId, int side)
{
    IBotManager::instance->setBotIsDead(playerId);
    if (SquadManager::m_instance)
    {
        SquadMember* const member = SquadManager::m_instance->getSquadMember(playerId, side);
        if (member)
            member->squadMemberKilled();
    }
}

// ELF 0x398740. Notifies the active AI runtime, when present, of player creation.
void AIEventCourier::eventPlayerCreated(bool isAI)
{
    if (IAIMain::m_instance)
        IAIMain::m_instance->newPlayerCreated(isAI);      // IAIMain vtable +0x20
}

// ELF 0x398730 is a bare `ret`: the kit change is not forwarded.
void AIEventCourier::eventChangedKit(KitType)
{
}

// ELF 0x3987F0: the LOD manager drops the player (when it exists). With an AI main, a leaving
// bot is deactivated (IAIMain +0x1C); for anyone else IAIMain +0x24 reactivateBot runs, which
// refills the bot population.
void AIEventCourier::eventPlayerDisconnect(int playerId)
{
    if (AILODManager::m_instance)
        AILODManager::m_instance->removePlayer(playerId);
    if (IAIMain::m_instance)
    {
        if (IBotManager::instance->getBotFromPlayerId(playerId))
            IAIMain::m_instance->deactivateBot(playerId);
        else
            IAIMain::m_instance->reactivateBot(playerId);
    }
}

// ELF 0x398880: IBotManager +0x30 setBotChangedVehicle(player, first object); the second
// object is not read.
void AIEventCourier::eventPlayerEnterVehicle(int playerId, IAIObject* vehicle, IAIObject*)
{
    IBotManager::instance->setBotChangedVehicle(playerId, vehicle);
}

// ELF 0x3988C0: IAIMain +0x60 setPlayerExitedVehicle, unguarded.
void AIEventCourier::eventPlayerExitVehicle(IAIObject* vehicle, int playerId)
{
    IAIMain::m_instance->setPlayerExitedVehicle(vehicle, playerId);
}

// ELF 0x398900: draws only while the game is playing (IAIGame +0x60 isPlaying).
void AIEventCourier::eventDrawDebug()
{
    IAIGame* const game = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame);
    if (game && game->isPlaying())
        IAIMain::m_instance->draw();
}

// ELF 0x398950. Starts bot spawning without producing a scalar result.
void AIEventCourier::eventGameStart()
{
    IAIMain::m_instance->spawnBots();       // IAIMain vtable +0x54
}

// ELF 0x398980 is a bare `ret`.
void AIEventCourier::event_roundEnd()
{
}

// ELF 0x398990: four guarded resets, in this order. AIRenderUtil and CommunicationHub are
// direct calls; the DP manager is reached through its singleton interface (+0x64 deleteAll,
// result dropped) and the AI main through IAIMain +0x4C resetLevel.
void AIEventCourier::event_worldReset()
{
    if (AIRenderUtil::m_instance)
        AIRenderUtil::m_instance->unrenderAll();
    if (CommonSingletonServer::m_iDPManager)
        static_cast<IDPManager*>(CommonSingletonServer::m_iDPManager)->deleteAll();
    if (IAIMain::m_instance)
        IAIMain::m_instance->resetLevel();
    if (CommunicationHub::m_instance)
        CommunicationHub::m_instance->reset();
}

// ELF 0x398A00: IBotManager +0x3C setBotIsManDown; then, for a sided object, the opposing
// side's information about it (IAIObject +0xB8 getInformation(side == 1 ? 2 : 1), resolved
// inline through the information pool) has its physical plug-in's health set to 0
// (IPIPhysical +0x18 setHealth, the 0.0f constant at ELF 0x49CB34).
void AIEventCourier::eventManDown(int playerId, IAIObject* object)
{
    IBotManager::instance->setBotIsManDown(playerId, object);
    const int side = object->getSide();
    if (side)
    {
        Information* const information =
            informationManager->getInformation(object->getInformation((side == 1) + 1));
        if (information)
            information->getPhysical()->setHealth(0.0f);
    }
}

// ELF 0x398AC0: IBotManager +0x40 setBotIsRevived, unguarded.
void AIEventCourier::eventRevived(int playerId, IAIObject* object)
{
    IBotManager::instance->setBotIsRevived(playerId, object);
}

// ELF 0x398B00: the hub filters and delivers the message; the fourth argument is false.
void AIEventCourier::eventRadioMessage(const AIRadioMessageSettings& settings, int a, int b)
{
    CommunicationHub::m_instance->filterAndReceiveAIRadioMessage(settings, a, b, false);
}

// ELF 0x398ED0: IBotManager +0x6C removeMineTarget, unguarded.
void AIEventCourier::event_mineRemoved(unsigned int handle)
{
    IBotManager::instance->removeMineTarget(handle);
}

// ELF 0x398F10: forwarded to the hub unchanged.
void AIEventCourier::event_enemySpotted(IAIObject* object, unsigned char value)
{
    CommunicationHub::m_instance->enemySpottedByHuman(object, value);
}

// ELF 0x398F50: a bot player's BotMain +0x1E0 setIsRepaired(true). The slot lies past the
// Bot interface's last declared one (+0x1DC); BotMain is the only Bot in the image.
void AIEventCourier::event_isRepaired(int playerId)
{
    Bot* const bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (bot)
        static_cast<BotMain*>(bot)->setIsRepaired(true);
}

// ELF 0x3991A0: guarded SquadManager::repeatOrder with the two arguments swapped.
void AIEventCourier::event_requestNewOrders(int a, int b)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->repeatOrder(b, a);   // ELF 0x3991D1
}

// ELF 0x398B90: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 0, squad, true).
void AIEventCourier::event_recievedAttackCommanderOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(BotSquad::OrderAttack, squadId, true);
}

// ELF 0x398BE0: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 1, squad, true).
void AIEventCourier::event_recievedDefendCommanderOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(BotSquad::OrderDefend, squadId, true);
}

// ELF 0x398C30: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 2, squad, true).
void AIEventCourier::event_recievedMoveCommanderOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(BotSquad::OrderMove, squadId, true);
}

// ELF 0x398C80: guarded; the 2D position gets the terrain altitude (IAITerrain +0x14
// getTerrainAltitude(x, z)) as its y, and SquadManager::incomingArtillery(Vec3, side) follows.
void AIEventCourier::event_incomingArtillery(int side, Vec2 position)
{
    if (SquadManager::m_instance)
    {
        const float height = static_cast<IAITerrain*>(CommonSingletonServer::m_iaiTerrain)
                                 ->getTerrainAltitude(position.x, position.y);
        Vec3 impact;
        impact.x = position.x;
        impact.y = height;
        impact.z = position.y;
        SquadManager::m_instance->incomingArtillery(impact, side);
    }
}

// ELF 0x398D10: the side's SAI (IAIMain +0x3C getSAI) gets SAI +0x54 playerRequestsArtillery(player, position).
// The ELF returns the SAI's bool in EAX unchanged.
int AIEventCourier::event_receivedArtilleryRequest(int side, int playerId, Vec3 position)
{
    return IAIMain::m_instance->getSAI(side)->playerRequestsArtillery(playerId, position);
}

// ELF 0x398D80: the side's SAI (IAIMain +0x3C getSAI) gets SAI +0x58 playerRequestsUav(player, position).
// The ELF returns the SAI's bool in EAX unchanged.
int AIEventCourier::event_receivedUavRequest(int side, int playerId, Vec3 position)
{
    return IAIMain::m_instance->getSAI(side)->playerRequestsUav(playerId, position);
}

// ELF 0x398DF0: the side's SAI (IAIMain +0x3C getSAI) gets SAI +0x5C playerRequestsSupply(player, position).
// The ELF returns the SAI's bool in EAX unchanged.
int AIEventCourier::event_receivedSupplyRequest(int side, int playerId, Vec3 position)
{
    return IAIMain::m_instance->getSAI(side)->playerRequestsSupply(playerId, position);
}

// ELF 0x398E60: the side's SAI (IAIMain +0x3C getSAI) gets SAI +0x60 playerRequestsVehicleDrop(player, position).
// The ELF returns the SAI's bool in EAX unchanged.
int AIEventCourier::event_receivedVehicleDropRequest(int side, int playerId, Vec3 position)
{
    return IAIMain::m_instance->getSAI(side)->playerRequestsVehicleDrop(playerId, position);
}

// ELF 0x398FA0. Guards the singleton and forwards the
// unchanged squad id through IAIMain +0x68 / +0x64. The concrete AIMain callback is empty
// in both images ; this courier still performs the virtual call.
void AIEventCourier::event_commanderOrderAccepted(int squadId)
{
    if (IAIMain::m_instance)
        IAIMain::m_instance->commanderOrderAccepted(squadId);
}

// ELF 0x398FE0: guarded IAIMain +0x6C, the same shape as event_commanderOrderAccepted.
void AIEventCourier::event_commanderOrderRejected(int squadId)
{
    if (IAIMain::m_instance)
        IAIMain::m_instance->commanderOrderRejected(squadId);
}

// ELF 0x399020: IAIMain +0x70, unguarded.
void AIEventCourier::event_appliedForCommander(int playerId)
{
    IAIMain::m_instance->appliedForCommander(playerId);
}

// ELF 0x399060: the second argument's SAI gets SAI +0x4C setPlayerIsCommander(true), unguarded.
void AIEventCourier::event_acceptedCommander(int, int side)
{
    IAIMain::m_instance->getSAI(side)->setPlayerIsCommander(true);
}

// ELF 0x3990B0: guarded; setPlayerIsCommander(false) on the second argument's SAI.
void AIEventCourier::event_resignAsCommander(int, int side)
{
    if (IAIMain::m_instance)
        IAIMain::m_instance->getSAI(side)->setPlayerIsCommander(false);
}

// ELF 0x398B50. Only the SquadManager singleton is guarded;
// the manager receives the original one-based side and squad id without validation here.
void AIEventCourier::eventNewSquadLeader(int side, int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->newSquadLeader(side, squadId);
}

// ELF 0x399100. The order is player id, squad id, side.
// Both images pass all three integers unchanged after testing the manager singleton.
void AIEventCourier::event_joinedSquad(int playerId, int squadId, int side)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->playerJoinsSquad(playerId, squadId, side);
}

// ELF 0x399150: forwarded unchanged to the squad manager when it exists.
void AIEventCourier::event_removedFromSquad(int playerId, int squadId, int side)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->playerRemovedFromSquad(playerId, squadId, side);
}

// ELF 0x3991E0: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 0, squad, false).
void AIEventCourier::event_recievedAttackSquadOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(BotSquad::OrderAttack, squadId, false);
}

// ELF 0x399230: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 1, squad, false).
void AIEventCourier::event_recievedDefendSquadOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(BotSquad::OrderDefend, squadId, false);
}

// ELF 0x399280: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 2, squad, false).
void AIEventCourier::event_recievedMoveSquadOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(BotSquad::OrderMove, squadId, false);
}

// ELF 0x3992D0: guarded SquadManager::setSquadReceivedOrder(BotSquad::OrderType 3, squad, false).
void AIEventCourier::event_receivedRepairSquadOrder(int squadId)
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->setSquadReceivedOrder(static_cast<BotSquad::OrderType>(3), squadId, false);
}

// ELF 0x399320 and 0x399330 are bare `ret`s.
void AIEventCourier::event_squadOrderAccepted(int, int)
{
}

void AIEventCourier::event_squadOrderRejected(int, int)
{
}

// ELF 0x399340: IBotManager +0x60 setBotTeamKilled, unguarded.
void AIEventCourier::event_botTeamKilled(int a, int b)
{
    IBotManager::instance->setBotTeamKilled(a, b);
}

// ELF 0x399380: IAIPathfinding +0xF0 searchPointSelected(!fire), then the console's
// freeCameraFire. Neither singleton is tested.
void AIEventCourier::event_freeCameraFire(bool fire)
{
    IAIPathfinding::instance->searchPointSelected(!fire);
    AIConsole::m_instance->freeCameraFire();
}

// ELF 0x399780: guarded SquadManager::removeAllSquads.
void AIEventCourier::event_removeAllSquadMembers()
{
    if (SquadManager::m_instance)
        SquadManager::m_instance->removeAllSquads();
}

// ELF 0x3993D0. Forwards map-part initialization unchanged, including the bool argument.
// Requires initialized owners; negative map-part IDs emit an error diagnostic.
void AIEventCourier::event_retrievePathfindingMapId(int& mapPartId, bool walkable, const Vec3& minCorner,
                                                   const Vec3& maxCorner, const Mat4& transformation)
{
    IAIPathfinding::instance->initializeDestroyableMapPart(mapPartId, walkable, minCorner, maxCorner,
                                                           transformation);   // +0xE0 / PE +0xDC
}

// ELF 0x399430.
void AIEventCourier::event_pathfindingMapIdDestroyed(int mapPartId, IAIObject* object)
{
    if (mapPartId < 0)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/Main/AIEventCourier.cpp", 508, "")
            << "Object destroyed that has unknown connection to the the pathfinding.";
    }
    else
    {
        IAIPathfinding::instance->destroyMapPart(mapPartId);                    // +0xE4 / PE +0xE0
        if (object)
            AIStrategicObjectManager::getInstance()->removeBridgeSegment(object);
    }
}

// ELF 0x399580.
void AIEventCourier::event_pathfindingMapIdRestored(int mapPartId)
{
    IAIPathfinding::instance->restoreMapPart(mapPartId);                        // +0xE8 / PE +0xE4
}

// ELF 0x3995C0.
void AIEventCourier::event_pathfindingMapIdRestoreOriginalMaterials(int mapPartId, IAIObject* object)
{
    if (mapPartId < 0)
    {
        Debug(DebugTypeError, "AI/AIDLL/AI/Main/AIEventCourier.cpp", 532, "")
            << "Object destroyed that has unknown connection to the the pathfinding.";
    }
    else
    {
        IAIPathfinding::instance->resetOriginalMaterialOfDestroyableMapPart(mapPartId);   // +0xEC / PE +0xE8
        if (object)
            AIStrategicObjectManager::getInstance()->removeBridgeSegment(object);
    }
}

// ELF 0x399710. The int lands in the segment's +4, [UNRESOLVED] (see AIStrategicObjectManager::BridgeSegment).
void AIEventCourier::event_bridgeDestroyed(IAIObject* object, int value)
{
    AIStrategicObjectManager::getInstance()->bridgeSegmentDestroyed(object, value);
}

// ELF 0x399750.
void AIEventCourier::event_bridgeRepaired(IAIObject* object)
{
    AIStrategicObjectManager::getInstance()->bridgeSegmentRepaired(object);
}

// ELF 0x3997B0.
void AIEventCourier::event_bridgeCreated(IAIObject* object)
{
    AIStrategicObjectManager::getInstance()->createBridgeSegment(object);
}

// ELF 0x3997E0.
void AIEventCourier::event_bridgeRemoved(IAIObject* object)
{
    AIStrategicObjectManager::getInstance()->removeBridgeSegment(object);
}

// ELF 0x399810. Initializes strategic areas before pathfinding maps.
void AIEventCourier::event_levelLoaded()
{
    AIStrategicObjectManager::getInstance()->postLoadInitAreas();
    IAIPathfindingSetup::instance->initMaps();

}

// ELF 0x399850: IAIMain +0x28 switchSides(int), unguarded.
void AIEventCourier::event_botSwitchSides(int playerId)
{
    IAIMain::m_instance->switchSides(playerId);
}

// ELF 0x399890. Forwards a side switch when the IAIMain singleton exists.
void AIEventCourier::event_playerSwitchSides(int side, int playerId)
{
    if (IAIMain::m_instance)
        IAIMain::m_instance->switchSides(side, playerId);      // IAIMain vtable +0x2C
}

} // namespace ai
} // namespace hfe
} // namespace dice
