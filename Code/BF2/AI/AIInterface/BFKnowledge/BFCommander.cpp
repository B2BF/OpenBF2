// [INFERRED] Source placement.

#include "BFCommander.h"

#include "IAIObject.h"
#include "AIPlayer.h"
#include "../../../IO/Network/INetworkable.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../Game/GameLogic/SquadManager/SquadOrder.h"
#include "../../../Game/Objects/Game/ArtilleryHandler.h"   // the two artillery queries
#include "../../../Game/GameEvents/ArtilleryEvent.h"        // fireArtillery
#include "../../../Game/GameEvents/UAVEvent.h"
#include "../../../Game/GameEvents/SupplyDropEvent.h"
#include "../../../Game/GameEvents/DropVehicleEvent.h"
#include "../../../Game/GameLogic/GameLogic.h"
#include "../../../Game/GameLogic/VoiceMessages.h"
#include "../../../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../../../Core/EventManager.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods

namespace dice {
namespace hfe {

extern ClassManager* classManager;
extern IBase* squadManager;                      // ds:0x8ACBFA4 - Core/Globals.cpp

namespace ai {

namespace {
// Shared spelling of the lookup/play sequence in ELF 0x840F7F0..0x8410463.
void playCommanderMessage(int team, const char* name)
{
    int messageId = 0, voiceId = 0, filterId = 0;
    bool found;
    {
        const std::string filter("");
        const std::string voice("commander");
        const std::string message(name);
        found = static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->namesToIds(
            &messageId, &voiceId, &filterId, message, voice, filter);
    }
    if (found)
        static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->playRadioMessage(
            team, messageId, voiceId, filterId);
}
}

// First wins in, exact match out.
IBFCommander::IBFCommander()
{
    if (!CommonSingletonServer::m_iBFCommander)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IBFCommander"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IBFCommander::~IBFCommander()
{
    if (CommonSingletonServer::m_iBFCommander == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IBFCommander"));
        BFSingletonServer::updateStaticMethods();
    }
}

BFCommander::BFCommander()
{
}

BFCommander::~BFCommander()
{
}

// bf2 ELF 0x840F340: ISquadManager::applyForCommander(side +0x6C, id +0x1C) (+0x58).
// [CHOSEN] The image returns whatever the void call leaves in eax; 0 here (callers discard it).
int BFCommander::applyAsCommander(IAIPlayer* player)
{
    const int id = player->getId();
    static_cast<ISquadManager*>(squadManager)->applyForCommander(player->getSide(), id);
    return 0;
}

// bf2 ELF 0x840F390: ISquadManager::resignCommander(side) (+0x68). Return as above.
int BFCommander::retireAsCommander(IAIPlayer* player)
{
    static_cast<ISquadManager*>(squadManager)->resignCommander(player->getSide());
    return 0;
}

// bf2 ELF 0x840F3C0 / 0x840F400 / 0x840F440: giveOrder with order type 5 / 4 / 6. The image
// tail-returns giveOrder's leftover eax; 0 here ([CHOSEN], CommanderInterface discards it).
int BFCommander::giveAttackOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    giveOrder(position, a, b, target, static_cast<SquadOrderType>(5));
    return 0;
}

int BFCommander::giveMoveOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    giveOrder(position, a, b, target, static_cast<SquadOrderType>(4));
    return 0;
}

int BFCommander::giveDefendOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    giveOrder(position, a, b, target, static_cast<SquadOrderType>(6));
    return 0;
}

// bf2 ELF 0x840F740: a commander order {type, target network id or 0, points = {position, 0, 0},
// flag2C false} for (team, squad) through ISquadManager::issueCommanderOrder (+0x48, last
// argument false). The target's id is its object's networkable id (IObjectComponent +0x20,
// IObject::getNetworkable slot 54, ushort at +4).
void BFCommander::giveOrder(const Vec3& position, int squadId, int team, IAIObject* target, SquadOrderType type)
{
    SquadOrder order;
    for (int i = 0; i < 3; ++i)
    {
        order.points[i].x = 0.0f;
        order.points[i].y = 0.0f;
        order.points[i].z = 0.0f;
    }
    order.flag2C = false;
    order.orderId = type;
    order.points[0] = position;
    order.targetNetId = 0;
    if (target)
    {
        io::INetworkable* const networkable = target->getObject()->getNetworkable();
        if (networkable)
            order.targetNetId = static_cast<unsigned short>(networkable->m_networkableId);
    }
    static_cast<ISquadManager*>(squadManager)->issueCommanderOrder(team, squadId, order, false);
}

// ELF 0x840F7F0. The squad argument is unused.
void BFCommander::sendAttackMessage(int team, int /*squadId*/)
{
    playCommanderMessage(team, "attack_position");
}

// ELF 0x840F980.
void BFCommander::sendDefendMessage(int team, int /*squadId*/)
{
    playCommanderMessage(team, "defend");
}

// bf2 0x840F480. Queues a request-fire ArtilleryEvent (type 1, the team in the byte at +0x14,
// GameLogic category, id 15) for the next update. A squad id other than -1 also marks that
// squad's order accepted (ISquadManager +0x24: false, order type 2, squad order type 0, false).
// [CHOSEN, not recovered] The ELF leaves the last call's EAX; no caller reads it, 0 here.
int BFCommander::fireArtillery(int team, int squadId, Vec2 position)
{
    ArtilleryEvent* const event = new ArtilleryEvent();
    event->m_target = position;
    event->m_artilleryType = ArtilleryEvent::ArtilleryTypeRequestFire;
    event->m_team = static_cast<unsigned char>(team);
    eventManager->postEventNextUpdate(ECGameLogic, 15, static_cast<void*>(event));
    if (squadId != -1)
    {
        static_cast<ISquadManager*>(squadManager)
            ->setIsAccepted(team, squadId, false, static_cast<OrderType>(2),
                            static_cast<SquadOrderType>(0), false);
    }
    return 0;
}

// ELF 0x840F530. Unlike fireArtillery, these producers do not guard squadId == -1.
void BFCommander::sendUav(int team, int squadId, Vec2 position)
{
    UAVEvent* const event = new UAVEvent(position.x, position.y, team, false);
    eventManager->postEventNextUpdate(ECGameLogic, 15, event);
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(1), false);
}

// ELF 0x840F5E0.
void BFCommander::dropSupply(int team, int squadId, Vec2 position)
{
    SupplyDropEvent* const event = new SupplyDropEvent(position.x, position.y, team);
    eventManager->postEventNextUpdate(ECGameLogic, 15, event);
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(3), false);
}

// ELF 0x840F690. The producer deliberately leaves the trailing event byte untouched.
void BFCommander::dropVehicle(int team, int squadId, Vec2 position)
{
    DropVehicleEvent* const event = new DropVehicleEvent(team, position);
    eventManager->postEventNextUpdate(ECGameLogic, 15, event);
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(2), false);
}

// ELF 0x840FB10. Acceptance state is cleared even when message lookup fails.
void BFCommander::denyArtilleryRequest(int team, int squadId)
{
    playCommanderMessage(team, "artillery_deny");
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(0), false);
}

// bf2 0x840FCE0: the team's handler (one-based, unchecked) has any artillery piece registered.
bool BFCommander::gameHasArtillery(int team)
{
    return world::artilleryManager->getHandler(static_cast<unsigned char>(team))->hasArtilleryPieces();
}

// bf2 0x840FD00: the handler's haveAnyArtillery (pieces that still have an owner object).
bool BFCommander::anyArtilleryPiecesLeft(int team)
{
    return world::artilleryManager->getHandler(static_cast<unsigned char>(team))->haveAnyArtillery();
}

// ELF 0x840FD30. The capital A is attested by the mangled name.
void BFCommander::ArtilleryUnavailable(int team, int squadId)
{
    playCommanderMessage(team, "artillery_unavailable");
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(0), false);
}

// ELF 0x840FF00.
void BFCommander::denySupplyRequest(int team, int squadId)
{
    playCommanderMessage(team, "supply_deny");
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(3), false);
}

// ELF 0x84100D0.
void BFCommander::denyVehicleDropRequest(int team, int squadId)
{
    playCommanderMessage(team, "vehicledrop_deny");
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(2), false);
}

// ELF 0x84102A0.
void BFCommander::denyUavRequest(int team, int squadId)
{
    playCommanderMessage(team, "uav_deny");
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(
        team, squadId, false, static_cast<OrderType>(2), static_cast<SquadOrderType>(1), false);
}

} // namespace ai
} // namespace hfe
} // namespace dice
