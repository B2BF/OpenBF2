// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining BotSquad operations.

#include "BotSquad.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1A1930, __tcf_0 0x1A1990, _GLOBAL__I 0x1A19C0).
#include <iostream>
#include "BotMain.h"

#include "../Main/AIDllFrontier.h"
#include "../Main/CommonSingletonServer.h"
#include "../../../AIInterface/BFKnowledge/BFSquad.h"
#include "../AIKnowledge/Information.h"
#include "SquadCoordinator.h"
#include "SquadManager.h"

namespace dice {
namespace hfe {
namespace ai {

// Three stores, and the interesting one is the third: a fresh squad handle starts at `OrderNone`
// not at zero. Zero is `OrderAttack`, so a memset-initialised BotSquad would believe it had been
// told to attack. The constructor is what keeps that from being true.
BotSquad::BotSquad(BotMain* owner)
    : m_bot(owner)
    , m_squadId(0)
    , m_order(OrderNone)
{
}

// five bytes each - bare `ret`s, and `reset` doing nothing is a finding rather than a gap.
// A recycled bot keeps its squad id and its last order. See BotSquad.h.
BotSquad::~BotSquad()
{
}

void BotSquad::reset()
{
}

// The four order predicates. One `cmp` each, and between them they are how the enum was read.
bool BotSquad::hasAttackOrder()    { return m_order == OrderAttack; }
bool BotSquad::hasDefendOrder()    { return m_order == OrderDefend; }
bool BotSquad::hasMoveOrderOrder() { return m_order == OrderMove;   }   // dice's name
bool BotSquad::hasNoOrder()        { return m_order == OrderNone;   }

// Forwards squad operations to the engine squad adapter using the bot's player.
// [RETIRE-WHEN: aidll-02-skeleton] Remove when the corresponding implementation is complete and validated.

// Both test and return the adapter's AL.
bool BotSquad::joinSquad(int squad)
{
#if defined(_MSC_VER)
    // [PLATFORM] PE captures the adapter before getPlayer; ELF loads it afterward.
    IBFSquad* const adapter = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
    IAIPlayer* const player = m_bot->getPlayer();
#else
    IAIPlayer* const player = m_bot->getPlayer();
    IBFSquad* const adapter = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
#endif
    const bool joined = adapter->joinSquad(squad, player);
    if (joined)
        m_squadId = squad;
    return joined;
}

namespace {

IBFSquad* squadAdapter()
{
    return static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
}

IAIObject* objectOf(Information* info)
{
    return info ? info->getAIObject() : 0;     // Information +0x20
}

} // namespace

// ELF 0x1A11B0: joins the side's next empty squad (the id is not stored; the engine's join event
// sets it).
bool BotSquad::createSquad()
{
    IAIPlayer* const player = m_bot->getPlayer();
    const int squad = squadAdapter()->getNextEmptySquad(m_bot->getSide());
    return squadAdapter()->joinSquad(squad, player);
}

// ELF 0x1A1230.
void BotSquad::exitSquad()
{
    IAIPlayer* const player = m_bot->getPlayer();
    squadAdapter()->exitSquad(m_squadId, player);
}

// ELF 0x1A1280.
bool BotSquad::isSquadLeader() const
{
    IAIPlayer* const player = m_bot->getPlayer();
    return squadAdapter()->isSquadLeader(m_squadId, player);
}

// ELF 0x1A12D0: the argument is not read.
void BotSquad::applyAsSquadLeader(int)
{
    IAIPlayer* const player = m_bot->getPlayer();
    squadAdapter()->assignAsSquadLeader(m_squadId, player);
}

// ELF 0x1A1320: the argument is not read.
void BotSquad::retireAsSquadLeader(int)
{
    IAIPlayer* const player = m_bot->getPlayer();
    squadAdapter()->retireAsSquadLeader(m_squadId, player);
}

// ELF 0x1A13F0 / 0x1A1460 / 0x1A14D0: (position, squad, side, the target's object or null).
void BotSquad::issueSquadAttackOrder(const Vec3& position, Information* info)
{
    IAIObject* const object = objectOf(info);
    squadAdapter()->issueSquadAttackOrder(position, m_squadId, m_bot->getSide(), object);
}

void BotSquad::issueSquadMoveOrder(const Vec3& position, Information* info)
{
    IAIObject* const object = objectOf(info);
    squadAdapter()->issueSquadMoveOrder(position, m_squadId, m_bot->getSide(), object);
}

void BotSquad::issueSquadDefendOrder(const Vec3& position, Information* info)
{
    IAIObject* const object = objectOf(info);
    squadAdapter()->issueSquadDefendOrder(position, m_squadId, m_bot->getSide(), object);
}

namespace {

// ELF 0x1A1540 / 0x1A1640 / 0x1A1740: the squad order's position (IBFSquad +0x54 getSquadOrder),
// the order type stored, and the squad's mission requested unless it is the same already.
void receiveSquadOrder(BotMain* bot, int squadId, BotSquad::OrderType& order, BotSquad::OrderType type)
{
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    squadAdapter()->getSquadOrder(bot->getSide(), squadId, position, 0);
    order = type;
    SquadCoordinator* const squad = SquadManager::m_instance->getSquadFromId(squadId, bot->getSide());
    if (!squad->isSameMission(order, position, 0))
        squad->requestMission(order, position, 0, false);
}

} // namespace

void BotSquad::recieveAttackSquadOrder()
{
    receiveSquadOrder(m_bot, m_squadId, m_order, OrderAttack);
}

void BotSquad::recieveDefendSquadOrder()
{
    receiveSquadOrder(m_bot, m_squadId, m_order, OrderDefend);
}

void BotSquad::recieveMoveSquadOrder()
{
    receiveSquadOrder(m_bot, m_squadId, m_order, OrderMove);
}

// ELF 0x1A18E0 / 0x1A18F0 / 0x1A1900: empty.
void BotSquad::recieveAttackCommanderOrder() {}
void BotSquad::recieveDefendCommanderOrder() {}
void BotSquad::recieveMoveCommanderOrder() {}

// ELF 0x1A1910 / 0x1A1920: empty (a 0x148-byte frame and no code).
void BotSquad::acceptCommanderOrder() {}
void BotSquad::declineCommanderOrder() {}

// ELF 0x1A1840 / 0x1A1890.
void BotSquad::acceptSquadOrder()
{
    IAIPlayer* const player = m_bot->getPlayer();
    squadAdapter()->acceptSquadOrder(m_squadId, player);
}

void BotSquad::declineSquadOrder()
{
    IAIPlayer* const player = m_bot->getPlayer();
    squadAdapter()->declineSquadOrder(m_squadId, player);
}

} // namespace ai
} // namespace hfe
} // namespace dice
