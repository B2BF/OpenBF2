// [INFERRED] Source placement.
// [FRONTIER] Remaining methods stop by name until implemented.
// [TEMP] Temporary implementation boundaries remain.
// [CHOSEN, not recovered] Temporary implementation boundaries remain.
// [RETIRE-WHEN: aidll-01-adapters] Complete and validate the remaining BFSquad operations.

#include "BFSquad.h"
#include "AIPlayer.h"

#include "AIFrontier.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../../../Game/GameLogic/SquadManager/ISquadInfo.h"
#include "../../../Game/GameLogic/SquadManager/SquadOrder.h"
#include "../../../Game/GameEvents/RequestEvent.h"
#include "../../../Game/Objects/IObject.h"
#include "../../../IO/Network/NetworkManager.h"
#include "../../../IO/Network/INetworkable.h"
#include "IAIObject.h"
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods

namespace dice {
namespace hfe {

extern ClassManager* classManager;
extern IBase* squadManager;

namespace ai {

// First wins in, exact match out.
IBFSquad::IBFSquad()
{
    if (!CommonSingletonServer::m_iBFSquad)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IBFSquad"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

IBFSquad::~IBFSquad()
{
    if (CommonSingletonServer::m_iBFSquad == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IBFSquad"));
        BFSingletonServer::updateStaticMethods();
    }
}

BFSquad::BFSquad()
{
}

BFSquad::~BFSquad()
{
}

// bf2 0x84108F0: ISquadManager +0x40 getFirstUnusedSquad.
int BFSquad::getNextEmptySquad(int team) const
{
    return static_cast<ISquadManager*>(squadManager)->getFirstUnusedSquad(team);
}

// Preserve id-before-side virtual calls.
bool BFSquad::joinSquad(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    const int side = player->getSide();
    return static_cast<ISquadManager*>(squadManager)->addToSquad(side, squad, playerId);
}

// ELF 0x8410960: id, then side, then ISquadManager +0x34 removeFromSquad(side, squad, id, false).
// The ELF answers the zero-extended low byte of that void call; nothing reads it, 0 is answered.
int BFSquad::exitSquad(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    const int side = player->getSide();
    static_cast<ISquadManager*>(squadManager)->removeFromSquad(side, squad, playerId, false);
    return 0;
}

bool BFSquad::isSquadLeader(int squad, IAIPlayer* player)
{
    // [PLATFORM] ELF 0x84109C0 reads id, side, leader; PE 0x742110 reads side, leader, id.
#if defined(_MSC_VER)
    const int side = player->getSide();
    const int leader = static_cast<ISquadManager*>(squadManager)->getSquadLeader(side, squad);
    const int playerId = player->getId();
#else
    const int playerId = player->getId();
    const int side = player->getSide();
    const int leader = static_cast<ISquadManager*>(squadManager)->getSquadLeader(side, squad);
#endif
    return playerId == leader;
}

// bf2 0x8410A10: id, then side, then ISquadManager +0x38 setSquadLeader(side, squad, id, false).
// The ELF answers the low byte of that void call; nothing reads it, 0 is answered.
int BFSquad::assignAsSquadLeader(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    const int side = player->getSide();
    static_cast<ISquadManager*>(squadManager)->setSquadLeader(side, squad, playerId, false);
    return 0;
}

// bf2 0x8410A70: a player who is not the leader answers 1; the leader is replaced by -1
// (setSquadLeader(side, squad, -1, false), whose void low byte the ELF answers; 0 here).
int BFSquad::retireAsSquadLeader(int squad, IAIPlayer* player)
{
    const int playerId = player->getId();
    if (playerId != static_cast<ISquadManager*>(squadManager)->getSquadLeader(player->getSide(), squad))
        return 1;
    static_cast<ISquadManager*>(squadManager)->setSquadLeader(player->getSide(), squad, -1, false);
    return 0;
}

// bf2 0x8410B00: issueOrder with squad order type 5.
int BFSquad::issueSquadAttackOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueOrder(position, a, b, target, 5);
}

// bf2 0x8410B30: issueOrder with squad order type 4.
int BFSquad::issueSquadMoveOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueOrder(position, a, b, target, 4);
}

// bf2 0x8410B60: issueOrder with squad order type 6.
int BFSquad::issueSquadDefendOrder(const Vec3& position, int a, int b, IAIObject* target)
{
    return issueOrder(position, a, b, target, 6);
}

// 5 bytes - a bare ret. Empty, like its decline twin.
// bf2 0x8410C70 and 0x8410C80 are bare `ret`s.
void BFSquad::acceptSquadOrder(int, IAIPlayer*)
{
}

// ELF, - empty.
void BFSquad::declineSquadOrder(int, IAIPlayer*)
{
}

int BFSquad::acceptCommanderAttackOrder(int squad, IAIPlayer* player)
{
    return acceptCommanderOrder(squad, player, 5);
}

int BFSquad::acceptCommanderDefendOrder(int squad, IAIPlayer* player)
{
    return acceptCommanderOrder(squad, player, 6);
}

int BFSquad::acceptCommanderMoveOrder(int squad, IAIPlayer* player)
{
    return acceptCommanderOrder(squad, player, 4);
}

int BFSquad::declineCommanderAttackOrder(int squad, IAIPlayer* player)
{
    return declineCommanderOrder(squad, player, 5);
}

int BFSquad::declineCommanderDefendOrder(int squad, IAIPlayer* player)
{
    return declineCommanderOrder(squad, player, 6);
}

int BFSquad::declineCommanderMoveOrder(int squad, IAIPlayer* player)
{
    return declineCommanderOrder(squad, player, 4);
}

// bf2 0x8410F30: for a human only, a RequestEvent (side, squad, type 1) executed on the server
// for the player's id (GameEvent +0x28) and released.
// [CHOSEN, not recovered] The ELF leaves the last call's EAX; no caller reads it, 0 here.
int BFSquad::requestNewCommanderOrder(int squad, IAIPlayer* player)
{
    if (player->getIsAIPlayer())
        return 0;
    RequestEvent* const event =
        new RequestEvent(player->getSide(), squad, static_cast<RequestEvent::RequestTypes>(1));
    event->executeServer(player->getId());
    event->release();
    return 0;
}

// bf2 0x8410FB0: the squad's commander order (ISquadInfo +0x14) gives the position (its first
// point) and the target through the network id. The target argument is not read.
world::IObject* BFSquad::getCommanderOrder(int a, int b, Vec3& position, IAIObject*)
{
    SquadOrder* const order = static_cast<ISquadManager*>(squadManager)->getSquadInfo(a, b)->getCommanderOrders();
    position = order->points[0];
    return io::networkManager->getObject(order->targetNetId);
}

// bf2 0x8411020: the same for the squad order (ISquadInfo +0x10).
world::IObject* BFSquad::getSquadOrder(int a, int b, Vec3& position, IAIObject*)
{
    SquadOrder* const order = static_cast<ISquadManager*>(squadManager)->getSquadInfo(a, b)->getSquadOrders();
    position = order->points[0];
    return io::networkManager->getObject(order->targetNetId);
}

// bf2 0x8410BC0. The order: the type, the target's network id (0 without a target or a
// networkable), the position as the first of three points (the other two zero); the flag at
// +0x2C is left unset as in the ELF. ISquadManager +0x44 issueSquadOrder(b, a, order, false).
int BFSquad::issueOrder(const Vec3& position, int a, int b, IAIObject* target, int orderType)
{
    SquadOrder order;
    for (int i = 0; i < 3; ++i)
    {
        order.points[i].x = 0.0f;
        order.points[i].y = 0.0f;
        order.points[i].z = 0.0f;
    }
    order.orderId = orderType;
    order.points[0] = position;
    order.targetNetId = 0;
    if (target)
    {
        io::INetworkable* const networkable = target->getObject()->getNetworkable();
        if (networkable)
            order.targetNetId = networkable->m_networkableId;
    }
    static_cast<ISquadManager*>(squadManager)->issueSquadOrder(b, a, order, false);
    return 0;
}

// bf2 0x8410D20 / 0x8410E70. The order is marked accepted (or not) for the player's side
// (ISquadManager +0x24: accepted, OrderType 1, the squad order type, false); a human's
// acceptance (RequestEvent type 2) or decline (type 3) is executed for the player and released.
int BFSquad::acceptCommanderOrder(int squad, IAIPlayer* player, int orderType)
{
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(player->getSide(), squad, true, static_cast<OrderType>(1),
                      static_cast<SquadOrderType>(orderType), false);
    if (player->getIsAIPlayer())
        return 0;
    RequestEvent* const event =
        new RequestEvent(player->getSide(), squad, static_cast<RequestEvent::RequestTypes>(2));
    event->executeServer(player->getId());
    event->release();
    return 0;
}

int BFSquad::declineCommanderOrder(int squad, IAIPlayer* player, int orderType)
{
    static_cast<ISquadManager*>(squadManager)->setIsAccepted(player->getSide(), squad, false, static_cast<OrderType>(1),
                      static_cast<SquadOrderType>(orderType), false);
    if (player->getIsAIPlayer())
        return 0;
    RequestEvent* const event =
        new RequestEvent(player->getSide(), squad, static_cast<RequestEvent::RequestTypes>(3));
    event->executeServer(player->getId());
    event->release();
    return 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
