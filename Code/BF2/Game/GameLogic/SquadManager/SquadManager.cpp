// Squad lifetime, membership, orders, and commander control. [INFERRED] source filename.

#include "SquadManager.h"
#include "../SpawnSystem/SpawnGroup.h"   // ISpawnGroup::setSquad / setTeam
#include "../SpawnSystem/SpawnManager.h"   // reAddDynamicSpawnGroups - the spawn-group re-wiring

#include <list>

#include "../../../Core/EventManager.h"               // local ECHud notifications
#include "../../../Core/Ids.h"                       // IID_IBase, IID_ISquadManager
#include "../../../AI/AIInterface/AIInstance.h"
#include "../../../AI/AIInterface/IAIEventCourier.h"
#include "../../../AI/AIInterface/SingletonServer.h"
#include "../../../AI/AIInterface/BFKnowledge/AIFrontier.h"
#include "../../../Scene/Object/PlayerManager.h"     // getPlayersSortedByScore (+0x4C) - client arm
#include "../../../Scene/Object/PlayerScoreData.h"   // join eligibility field +0x10
#include "../../Common/GameEvent.h"                  // PostRemoteEvent
#include "../../GameEvents/ChangeSquadNameEvent.h"   // sendSquadManagerState's name replay
#include "../../GameEvents/IssueSquadOrderEvent.h"   // the three order/request broadcasts
#include "../../GameEvents/SetAcceptOrderEvent.h"    // setIsAccepted's broadcast
#include "../../GameEvents/SetPrivateSquadEvent.h"   // sendSquadManagerState's privacy replay
#include "../../GameEvents/SetSquadLeaderEvent.h"    // leader-change broadcast
#include "../../IGame.h"                             // game - sendGameEventToAllConnected (+0x94)
#include "../../IGameLogic.h"                        // gameLogic - isServer, for getCommander
#include "../../Objects/ICameraObject.h"             // commander-to-soldier camera query
#include "../../Objects/IObject.h"                   // IID_IObject result / handleMessage
#include "../../Objects/IPlayer.h"                   // getIsCommander (+0x114) / getId (+0x54)
#include "../../Objects/Game/OrderMarker.h"          // client-side squad/commander order markers
#include "../../Python/PythonHost.h"                  // leader-change ScriptEvent 13
#include "../../Python/PyRef.h"
#include "Squad.h"                                   // the reset cascade reaches Squad
#include "Team.h"                                    // removeFromSquad / the commander forwarders


namespace dice {
namespace hfe {

// Creates the manager with one caller-owned reference. Rejects aggregation and
// releases the new instance when the requested interface is unsupported.
IBase* SquadManager::create(unsigned int iid, IBase* outer)
{
    if (outer)                                          // no aggregation
        return 0;

    SquadManager* manager = new SquadManager;           // operator new(0x28), 0x85312C6
    IBase* result = static_cast<IBase*>(manager->queryInterface(iid));   // vt +0x0C
    if (!result)
        manager->release();                             // vt +0x08 - NOT delete; see above
    return result;
}

// Default squad names. Entry zero is Unassigned; the constructor uses the first
// ten entries. Preserve Gamma, the spelling used by the original game.
namespace {

// [CHOSEN] Static text table replaces runtime string initialization while retaining names, including Gamma.
const char* const kSquadNames[27] =
{
    "Unassigned", "Alpha",  "Bravo",   "Charlie", "Delta",   "Echo",    "Foxtrot",
    "Gamma",      "Hotel",  "India",   "Juliet",  "Kilo",    "Lima",    "Mike",
    "November",   "Oscar",  "Papa",    "Quebec",  "Romeo",   "Sierra",  "Tango",
    "Uniform",    "Victor", "Whiskey", "Xray",    "Yankee",  "Zulu"
};

} // anonymous namespace

// Creates a named squad, inserting a team first when necessary. Team copies must
// retain the commander camera so destroying insertion temporaries cannot release it early.
int SquadManager::create(int teamId, const std::string& name)
{
    Team* team = getTeam(teamId);
    if (!team)
    {
        // the by-value insert - see the note above for what those 1,296 bytes actually are.
        m_teams.insert(std::make_pair(teamId, Team(teamId)));
        team = getTeam(teamId);                         // re-fetched, not insert's iterator
    }
    return team->createSquad(name);
}

// Creates ten named squads on each team, alternating team 1 and 2 for each name.
SquadManager::SquadManager()
    : m_refCount(1)                                     // +0x04
    , m_firstCommanderSelectDelay(15.0f)                // +0x14
    , m_commanderSelectDelay(10.0f)                     // +0x18
    , m_field1C(20.0f)                                  // +0x1C  [UNRESOLVED]
    , m_orderMarker1(0)                                 // +0x20
    , m_orderMarker2(0)                                 // +0x24
{
    // Create ten identically named squads for each team, pairing team 1 then team 2.
    for (int i = 0; i <= 9; ++i)                        // ELF: v1 = 9 .. -1
    {
        const std::string name(kSquadNames[i]);
        create(1, name);                                // team 1
        create(2, name);                                // team 2
    }
}

// Removes all teams' squads before the team map is destroyed.
SquadManager::~SquadManager()
{
    removeAllSquads();
    // The std::map teardown below is implicit - _M_erase, header reset, deallocate(72).
}

void SquadManager::removeAllSquads()
{
    for (std::map<int, Team>::iterator it = m_teams.begin(); it != m_teams.end(); ++it)
        it->second.removeAllSquads();                   // Team::removeAllSquads, 0x811AA60
}

// Recreates surviving squads' dynamic spawn groups under their previous IDs after map reload.
// Requires all ten squads on both teams. Iteration is squad-major, team 1 then 2,
// and restores squad before team on each recovered group.
extern IBase* spawnManager;

void SquadManager::reAddDynamicSpawnGroups()
{
    for (int i = 0; i <= 9; ++i)
    {
        for (int team = 1; team <= 2; ++team)
        {
            Squad* const squad = getSquad(team, i);
            if (squad->m_spawnGroupId >= 0)                                   // +0x138
            {
                const int groupId = static_cast<SpawnManager*>(spawnManager)
                                        ->createExistingDynamicSpawnGroup(squad->m_spawnGroupId);   // +0x3C
                ISpawnGroup* const group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(groupId);   // +0x28
                if (group)
                {
                    group->setSquad(squad->getSquadId());                     // ISpawnGroup +0x20
                    group->setTeam(squad->m_team->getTeamId());               // ISpawnGroup +0x18
                }
            }
        }
    }
}

// IBase reference counting and interface queries. addRef has a void signature.
void SquadManager::addRef()
{
    ++m_refCount;                                       // the ELF returns the new value - see above
}

int SquadManager::getRef()
{
    return m_refCount;
}

int SquadManager::release()
{
    const int count = m_refCount - 1;
    m_refCount = count;
    if (count == 0 && this)                             // the binary's own redundant `this` test
        delete this;                                    // vt destructor slot, 0x81200A6
    return count;
}

void* SquadManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<SquadManager*>(this);
    if (iid == IID_ISquadManager)                       // 50314
        return const_cast<SquadManager*>(this);
    return 0;
}

// Returns the team found by ID, or null when absent.
Team* SquadManager::getTeam(int team)
{
    std::map<int, Team>::iterator i = m_teams.find(team);
    return (i == m_teams.end()) ? 0 : &i->second;
}

const Team* SquadManager::getTeam(int team) const
{
    std::map<int, Team>::const_iterator i = m_teams.find(team);
    return (i == m_teams.end()) ? 0 : &i->second;
}

// The two float accessor pairs - ELF 0x811EF20 / 0x811EF30 / 0x811EF40 / 0x811EF50. Eleven and
// fourteen bytes each; the offsets are read straight from their `fld`/`fstp` instructions.
float SquadManager::getFirstCommanderSelectDelay() const { return m_firstCommanderSelectDelay; }
void  SquadManager::setFirstCommanderSelectDelay(float v){ m_firstCommanderSelectDelay = v; }
float SquadManager::getCommanderSelectDelay() const      { return m_commanderSelectDelay; }
void  SquadManager::setCommanderSelectDelay(float v)     { m_commanderSelectDelay = v; }

// Resets each team, re-reading the map end on each iteration.
// [CHOSEN] void return; no caller consumes a result.
void SquadManager::reset()
{
    for (std::map<int, Team>::iterator it = m_teams.begin(); it != m_teams.end(); ++it)
        it->second.reset();
}

// getSquad - ELF 0x811D030, 60 bytes. Two lookups, both null-guarded.
// THE squad ID narrows TO A byte on the way into Team::getSquad - the map is keyed by
// `unsigned char`, the same narrowing Team::createSquad performs on the way in. An id above 255
// would alias.
Squad* SquadManager::getSquad(int teamId, int squadId)
{
    Team* const team = getTeam(teamId);
    if (!team)
        return 0;
    return team->getSquad(squadId);                      // narrows to unsigned char
}

// remove / resetSquad - ELF 0x811C9A0 / 0x811C9E0. Both are guarded, two-step forwarders. Team's
// byte-key lookup preserves the retail squad-id narrowing.
void SquadManager::remove(int teamId, int squadId)
{
    Team* const team = getTeam(teamId);
    if (team)
        team->removeSquad(squadId);
}

void SquadManager::resetSquad(int teamId, int squadId)
{
    Squad* const squad = getSquad(teamId, squadId);
    if (squad)
        squad->reset(true);
}

// inviteToSquad - ELF 0x811CA20, 434 B / PE 0x45D200. An invite is accepted only for a player on
// the supplied team. AI players join immediately; humans receive the squad bit and (unless
// suppressed) HUD 16. Revocation only clears the bit and never emits a HUD event.
void SquadManager::inviteToSquad(int teamId, int squadId, int playerId,
                                 bool invite, bool suppressEvent)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player || player->getTeam() != teamId)
        return;

    if (invite)
    {
        if (player->getIsAIPlayer())
        {
            addToSquad(player->getTeam(), squadId, playerId);
        }
        else
        {
            player->setSquadInviteMask(player->getSquadInviteMask() | (1u << squadId));
            if (!suppressEvent)
            {
                if (playerId == 255)
                    eventManager->postEvent(ECHud, 16, &squadId, 0.0f);
                else
                    game->sendGameEventIfConnected(playerId,
                        new PostRemoteEvent(ECHud, 16, false, &squadId, 4, 0.0f));
            }
        }
    }
    else
    {
        player->setSquadInviteMask(player->getSquadInviteMask() & ~(1u << squadId));
    }
}

// The interface overload passes retail's otherwise-unused literal 64 into the four-int worker.
//  / PE 0x460950: both forward the worker's bool result (AL).
bool SquadManager::addToSquad(int teamId, int squadId, int playerId)
{
    return addToSquad(64, squadId, playerId, teamId);
}

// addToSquad(int,int,int,int) - ELF 0x811CC20, 802 B / PE 0x45FE70. This is the admission gate:
// living/man-down state, six-player cap, then private-squad invitation. Failure notifications and
// their recipients are preserved exactly; the actual membership transition lives in Squad.
bool SquadManager::addToSquad(int /*unused*/, int squadId, int playerId, int teamId)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return false;

    Team* const team = getTeam(teamId);
    if (!team)
        return false;

    Squad* const squad = team->getSquad(squadId);
    if (!squad)
        return false;

    if ((player->getScoreData()->m_field10 > 0 && !player->getIsAlive())
        || player->getIsManDown())
    {
        return false;
    }

    if (world::playerManager->getNrOfPlayersInSquad(teamId, squadId) > 5)
    {
        if (playerId == 255)
            eventManager->postEvent(ECHud, 11, &squadId, 0.0f);
        else
            game->sendGameEventIfConnected(playerId,
                new PostRemoteEvent(ECHud, 11, false, &squadId, 4, 0.0f));
        return false;
    }

    if (squad->getIsPrivate())
    {
        const unsigned int inviteMask = player->getSquadInviteMask();
        if ((inviteMask & (1u << squad->getSquadId())) == 0
            && world::playerManager->getNrOfPlayersInSquad(teamId, squadId) > 0)
        {
            if (squad->m_leaderPlayerId == 255)
                eventManager->postEvent(ECHud, 17, &playerId, 0.0f);
            else
                game->sendGameEventIfConnected(squad->m_leaderPlayerId,
                    new PostRemoteEvent(ECHud, 17, false, &playerId, 4, 0.0f));

            if (playerId == 255)
                eventManager->postEvent(ECHud, 12, &squadId, 0.0f);
            else
                game->sendGameEventIfConnected(playerId,
                    new PostRemoteEvent(ECHud, 12, false, &squadId, 4, 0.0f));
            return false;
        }
    }

    if (player->getIsCommander())
    {
        const int playerTeam = player->getTeam();
        // Shipped argument-order defect: team=-1 and playerId=the actual team. getTeam(-1)
        // therefore makes this a no-op. Both ELF and PE carry it; do not "fix" the call.
        setCommander(-1, playerTeam);
    }
    return squad->addToSquad(playerId);
}

// Replays renamed/private squads to a joining connection. For each ID, sends team
// 1 name/privacy then team 2 name/privacy. Requires all squads and game; keep the
// second privacy query on the true branch.
void SquadManager::sendSquadManagerState(int connectionId)
{
    for (int squadId = 0; squadId < 10; ++squadId)
    {
        Squad* const team1Squad = getSquad(1, squadId);
        if (team1Squad->getSquadName() != kSquadNames[squadId])
            game->sendGameEvent(connectionId, new ChangeSquadNameEvent(
                1, squadId, team1Squad->m_leaderPlayerId, team1Squad->getSquadName()));
        if (team1Squad->getIsPrivate())
            game->sendGameEvent(connectionId, new SetPrivateSquadEvent(
                1, squadId, team1Squad->getIsPrivate()));

        Squad* const team2Squad = getSquad(2, squadId);
        if (team2Squad->getSquadName() != kSquadNames[squadId])
            game->sendGameEvent(connectionId, new ChangeSquadNameEvent(
                2, squadId, team2Squad->m_leaderPlayerId, team2Squad->getSquadName()));
        if (team2Squad->getIsPrivate())
            game->sendGameEvent(connectionId, new SetPrivateSquadEvent(
                2, squadId, team2Squad->getIsPrivate()));
    }
}

//  / PE 0x45E150. The interface is the concrete Squad's offset-zero base, so
// this is a typed tail forwarder with no pointer adjustment on either shipped port.
ISquadInfo* SquadManager::getSquadInfo(int teamId, int squadId)
{
    return getSquad(teamId, squadId);
}

// Renames only empty nonzero squads. Unsuppressed requests reject duplicate active
// names. Success resets, renames, rejoins, broadcasts, then acknowledges; returns true
// after a successful store even without game.
bool SquadManager::setSquadName(int teamId, int squadId, int playerId,
                                const std::string& name, bool suppressEvent)
{
    if (squadId == 0)
        return false;

    Squad* const squad = getSquad(teamId, squadId);
    if (!squad || !squad->m_members.empty())
        return false;

    if (!suppressEvent)
    {
        for (int otherSquadId = 0; otherSquadId <= 9; ++otherSquadId)
        {
            if (otherSquadId == squadId)
                continue;

            Squad* const other = getSquad(teamId, otherSquadId);
            if (other && other->m_leaderPlayerId != -1 && other->getSquadName() == name)
            {
                if (playerId == 255)
                    eventManager->postEvent(ECHud, 15, &otherSquadId, 0.0f);
                else
                    game->sendGameEventIfConnected(playerId,
                        new PostRemoteEvent(ECHud, 15, false, &otherSquadId, 4, 0.0f));
                return false;
            }
        }
    }

    squad->reset(true);
    if (squad->setSquadName(name))
    {
        if (!suppressEvent && game)
        {
            addToSquad(teamId, squadId, playerId);
            game->sendGameEventToAllConnected(
                new ChangeSquadNameEvent(teamId, squadId, playerId, name));

            if (playerId == 255)
                eventManager->postEvent(ECHud, 14, &squadId, 0.0f);
            else
                game->sendGameEventIfConnected(playerId,
                    new PostRemoteEvent(ECHud, 14, false, &squadId, 4, 0.0f));
        }
        return true;
    }

    return false;
}

// setIsPrivate - ELF 0x811D3B0, 165 B / PE 0x45E1F0. Squad zero is never private. The state write
// precedes the optional network event, and suppressed calls never inspect the game global.
void SquadManager::setIsPrivate(int teamId, int squadId, bool isPrivate, bool suppressEvent)
{
    if (squadId == 0)
        return;

    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    squad->m_isPrivate = isPrivate;
    if (!suppressEvent && game)
        game->sendGameEventToAllConnected(new SetPrivateSquadEvent(teamId, squadId, isPrivate));
}

// setSquadLeader - ELF 0x811D0A0, 609 B / PE 0x4601A0. The wire event goes first; both Player
// pointers are captured before Squad resets its order state and installs the leader; HUD 24 is then
// sent to every member; Python event 13 receives (squadId, oldLeader, newLeader), then its PyRef
// is released before the guarded AI courier notification.
void SquadManager::setSquadLeader(int teamId, int squadId, int playerId, bool suppressEvent)
{
    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    if (!suppressEvent && game)
        game->sendGameEventToAllConnected(new SetSquadLeaderEvent(teamId, playerId, squadId));

    world::IPlayer* const oldLeader =
        world::playerManager->getPlayerFromId(squad->m_leaderPlayerId);
    world::IPlayer* const newLeader = world::playerManager->getPlayerFromId(playerId);
    squad->setSquadLeader(playerId);

    if (gameLogic->isServer())
    {
        for (std::set<int>::iterator it = squad->m_members.begin();
             it != squad->m_members.end(); ++it)
        {
            if (*it == 255)
                eventManager->postEvent(ECHud, 24, &playerId, 0.0f);
            else
                game->sendGameEventIfConnected(*it,
                    new PostRemoteEvent(ECHud, 24, false, &playerId, 4, 0.0f));
        }
    }

    {
        python::PyRef args = python::buildArgs(squadId, oldLeader, newLeader);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(13), args.get());
    }

    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())
    {
        //  / PE 0x460334: courier +0x88 / +0x84 (teamId, squadId).
        ai::BFSingletonServer::m_iaiEventCourier->eventNewSquadLeader(teamId, squadId);
    }
}

// getSquadLeader - ELF 0x811D310, 157 B / PE 0x45E160. Server state owns the authoritative id;
// a client instead scans that squad's score-sorted players for the leader flag.
int SquadManager::getSquadLeader(int teamId, int squadId)
{
    if (gameLogic->isServer())
    {
        Squad* const squad = getSquad(teamId, squadId);
        return squad ? squad->m_leaderPlayerId : -1;
    }

    const std::list<world::IPlayer*>& players =
        world::playerManager->getPlayersInSquadSortedByScore(teamId, squadId);
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
    {
        if ((*it)->getIsSquadLeader())
            return (*it)->getId();
    }
    return -1;
}

// getFirstUnusedSquad - ELF 0x811D560, 48 B / PE 0x45E320.
int SquadManager::getFirstUnusedSquad(int teamId) const
{
    const Team* const team = getTeam(teamId);
    return team ? team->getFirstUnusedSquad() : -1;
}

// issueSquadOrder - ELF 0x811D590, 622 B / PE 0x45E370, 446 B. Squad zero is excluded. After
// installing the order, an unsuppressed live game broadcasts type 0, then visits members in set
// order: AI players receive ecai 9 `(playerId, orderId)`; human leaders receive HUD 28 and other
// humans HUD 23, each carrying the squad id.
void SquadManager::issueSquadOrder(int teamId, int squadId, SquadOrder& order,
                                   bool suppressEvent)
{
    if (squadId == 0)
        return;

    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    squad->issueSquadOrder(order);
    if (suppressEvent || !game)
        return;

    game->sendGameEventToAllConnected(
        new IssueSquadOrderEvent(teamId, squadId, &order, static_cast<OrderType>(0)));

    for (std::set<int>::iterator it = squad->m_members.begin();
         it != squad->m_members.end(); ++it)
    {
        world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);
        if (!player)
            continue;

        if (player->getIsAIPlayer())
        {
            int aiPayload[2] = { *it, order.orderId };
            eventManager->postEvent(ECAI, 9, aiPayload, 0.0f);
            continue;
        }

        const unsigned int hudId =
            (player->getId() == squad->m_leaderPlayerId) ? 28u : 23u;
        if (player->getId() == 255)
        {
            eventManager->postEvent(ECHud, hudId, &squadId, 0.0f);
        }
        else
        {
            game->sendGameEventIfConnected(player->getId(),
                new PostRemoteEvent(ECHud, hudId, false, &squadId, 4, 0.0f));
        }
    }
}

// issueCommanderOrder - ELF 0x811D800. Broadcasts the order before commander and member HUD
// notifications. Preserve repeated ID lookups. The AI singleton (ds:0x8AC37F8) is loaded once
// before the member loop; while it is non-null and initialized (IAIInstance +0x1C), every member
// first gets ecai 8 `(playerId, orderId)` (eventManager +0x14) and then its HUD 26, AI or not.
void SquadManager::issueCommanderOrder(int teamId, int squadId, SquadOrder& order,
                                       bool suppressEvent)
{
    if (squadId == 0)
        return;

    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    squad->issueCommanderOrder(order);
    if (suppressEvent || !game)
        return;

    game->sendGameEventToAllConnected(
        new IssueSquadOrderEvent(teamId, squadId, &order, static_cast<OrderType>(1)));

    if (getCommander(teamId) == 255)
    {
        eventManager->postEvent(ECHud, 27, &squadId, 0.0f);
    }
    else
    {
        game->sendGameEventIfConnected(getCommander(teamId),
            new PostRemoteEvent(ECHud, 27, false, &squadId, 4, 0.0f));
    }

    IBase* const aiSingleton = ai::g_aiInstance;   // 0x811D939: read once, before the loop
    for (std::set<int>::iterator it = squad->m_members.begin();
         it != squad->m_members.end(); ++it)
    {
        world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);
        if (!player)
            continue;

        // 0x811D98E..0x811DAA3: the AI courier's commander-order notification.
        if (aiSingleton && reinterpret_cast<ai::IAIInstance*>(aiSingleton)->isInitialized())
        {
            int aiPayload[2] = { *it, order.orderId };
            eventManager->postEvent(ECAI, 8, aiPayload, 0.0f);
        }
        if (player->getId() == 255)
        {
            eventManager->postEvent(ECHud, 26, &squadId, 0.0f);
        }
        else
        {
            game->sendGameEventIfConnected(player->getId(),
                new PostRemoteEvent(ECHud, 26, false, &squadId, 4, 0.0f));
        }
    }
}

// Broadcasts requests before leader then commander notification.
// [UNRESOLVED] Bot/coop payload and routing dependencies remain unrecovered;
// this path currently implements only the AI-singleton-null branch.
void SquadManager::issueRequest(int teamId, int squadId, SquadOrder& order,
                                bool suppressEvent)
{
    if (squadId == 0)
        return;

    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    squad->issueRequest(order);
    if (suppressEvent || !game)
        return;

    game->sendGameEventToAllConnected(
        new IssueSquadOrderEvent(teamId, squadId, &order, static_cast<OrderType>(2)));

    const int leaderId = squad->m_leaderPlayerId;
    if (leaderId >= 0)
    {
        if (leaderId == 255)
            eventManager->postEvent(ECHud, 30, &order, 0.0f);
        else
            game->sendGameEventIfConnected(leaderId,
                new PostRemoteEvent(ECHud, 30, false, &order, 4, 0.0f));
    }

    const int commanderId = getCommander(teamId);
    if (commanderId >= 0)
    {
        int commanderPayload[2] = { squadId, order.orderId };
        if (commanderId == 255)
            eventManager->postEvent(ECHud, 29, commanderPayload, 0.0f);
        else
            game->sendGameEventIfConnected(commanderId,
                new PostRemoteEvent(ECHud, 29, false, commanderPayload, 8, 0.0f));
    }
}

// Updates an existing nonzero squad's order acceptance, optionally suppressing network notification.
// Commander-order AI notification runs even when accepted is false and requires its courier.
// Event identifiers narrow to bytes after the local update.
bool SquadManager::setIsAccepted(int teamId, int squadId, bool accepted,
                                 OrderType orderType, SquadOrderType squadOrderType,
                                 bool suppressEvent)
{
    if (squadId <= 0)                                    // squad 0 is "Unassigned" - see above
        return false;

    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return false;

    squad->setIsAccepted(accepted, orderType, squadOrderType);

    if (ai::g_aiInstance && ai::aiInstance()->isInitialized() && orderType == 1)
    {
        //  / PE 0x45E2C6: courier +0x74 / +0x70 (squadId).
        ai::BFSingletonServer::m_iaiEventCourier->event_commanderOrderAccepted(squadId);
    }

    // THE `game` NULL check IS inside the suppress test, not around it - a suppressed call never
    // looks at the global at all. Reproduced in that order.
    if (!suppressEvent && game)
    {
        game->sendGameEventToAllConnected(                                     // IGame +0x94
            new SetAcceptOrderEvent(teamId, squadId, accepted, orderType, squadOrderType));
    }
    return true;
}

// findNewSquadLeader - ELF 0x811EF60, 317 B / PE 0x45F100. Prefer the first non-AI member other
// than the excluded player; if none exists, restart at begin and accept the first remaining AI.
void SquadManager::findNewSquadLeader(int teamId, int squadId, int excludedPlayerId)
{
    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    for (std::set<int>::iterator it = squad->m_members.begin();
         it != squad->m_members.end(); ++it)
    {
        world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);
        if (player && !player->getIsAIPlayer() && player->getId() != excludedPlayerId)
        {
            setSquadLeader(teamId, squadId, *it, false);
            return;
        }
    }

    for (std::set<int>::iterator it = squad->m_members.begin();
         it != squad->m_members.end(); ++it)
    {
        world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);
        if (player && player->getId() != excludedPlayerId)
        {
            setSquadLeader(teamId, squadId, *it, false);
            return;
        }
    }
}

// Team's commander facade - ELF 0x811E3D0 / 0x811E410 / 0x811E710..0x811E7C0. Missing teams
// silently produce no action (-1 for pop, null for the camera).
void SquadManager::setCommander(int teamId, int playerId)
{
    Team* const team = getTeam(teamId);
    if (team)
        team->setCommander(playerId);
}

void* SquadManager::getCommanderCam(int teamId)
{
    Team* const team = getTeam(teamId);
    return team ? team->getCommanderCam() : 0;
}

void SquadManager::applyForCommander(int teamId, int playerId)
{
    Team* const team = getTeam(teamId);
    if (team)
        team->applyForCommander(playerId);
}

int SquadManager::popNextCommanderApplicant(int teamId)
{
    Team* const team = getTeam(teamId);
    return team ? team->popNextCommanderApplicant() : -1;
}

void SquadManager::acceptCommander(int teamId, int playerId)
{
    Team* const team = getTeam(teamId);
    if (team)
        team->acceptCommander(playerId);
}

void SquadManager::declineCommander(int teamId, int playerId)
{
    Team* const team = getTeam(teamId);
    if (team)
        team->declineCommander(playerId);
}

// setCommanderOverheadCamZoomMode - ELF 0x811E440, 94 B / PE 0x45D330, 61 B. The two globals are
// the only guards; any resolved player receives the requested top-down flag, even if they are not
// currently commander or in overhead mode.
void SquadManager::setCommanderOverheadCamZoomMode(int playerId, bool topDown)
{
    if (!world::playerManager || !game)
        return;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (player)
        player->setOverheadCameraTopDownView(topDown);
}

// Enables commander overhead mode after manager/player/commander/camera checks.
// Requires a PCO in the vehicle's parent chain and a camera exposing IObject.
// Sends TemplateMessage 3 before enabling the mode.
void SquadManager::setCommanderToOverheadCamMode(int playerId)
{
    if (!world::playerManager)
        return;
    if (!game)
        return;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return;
    if (!player->getIsCommander())
        return;
    if (player->getOverheadCameraEnabled())
        return;

    const int teamId = player->getTeam();
    if (!getCommanderCam(teamId))
        return;

    world::IObject* object = player->getVehicle();
    void* playerControlObject = 0;
    while (object)
    {
        playerControlObject = object->queryInterface(world::IID_IPlayerControlObject);
        if (playerControlObject)
            break;
        object = object->getParent();
    }

    if (playerControlObject)
    {
        world::ICameraObject* const camera = player->getCamera();
        if (camera)
        {
            world::IObject* const cameraObject =
                static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject));
            cameraObject->handleMessage(static_cast<world::TemplateMessage>(3), player, false);
        }
        player->setOverheadCameraEnabled(true);
    }
}

// setCommanderToSoldierMode - ELF 0x811E5C0, 165 B / PE 0x45D440. The camera query result is
// deliberately not null-checked: retail only guards the ICameraObject pointer, then assumes it
// supports IID_IObject before delivering TemplateMessage 2.
void SquadManager::setCommanderToSoldierMode(int playerId)
{
    if (!world::playerManager || !game)
        return;

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player || !player->getOverheadCameraEnabled())
        return;

    player->setOverheadCameraEnabled(false);
    world::ICameraObject* const camera = player->getCamera();
    if (camera)
    {
        world::IObject* const object =
            static_cast<world::IObject*>(camera->queryInterface(world::IID_IObject));
        object->handleMessage(static_cast<world::TemplateMessage>(2), player, false);
    }
}

// addOrderMarker / removeAllOrderMarkers / removeOrderMarker - ELF 0x811E870 / 0x811E900 /
// cross-checked at PE 0x45D4C0 / 0x45D540 / 0x45D830. Kind 0 owns marker2 and kind 1
// owns marker1. Every add first removes that same kind through the virtual slot; all other kinds
// perform only that removal and allocate nothing.
void SquadManager::addOrderMarker(int markerId, SquadOrder& order)
{
    removeOrderMarker(markerId);
    if (markerId == 0)
        m_orderMarker2 = new world::OrderMarker(order, 0);
    else if (markerId == 1)
        m_orderMarker1 = new world::OrderMarker(order, 1);
}

void SquadManager::removeAllOrderMarkers()
{
    removeOrderMarker(0);
    removeOrderMarker(1);
}

void SquadManager::removeOrderMarker(int markerId)
{
    if (markerId == 0)
    {
        if (m_orderMarker2)
        {
            delete m_orderMarker2;
            m_orderMarker2 = 0;
        }
    }
    else if (markerId == 1)
    {
        if (m_orderMarker1)
        {
            delete m_orderMarker1;
            m_orderMarker1 = 0;
        }
    }
}

// Updates markers only for the local human's squad. Commander markers additionally
// require leadership and orderId > 3. Dedicated servers have no local human.
void SquadManager::updateOrderMarkers(world::IPlayer* player)
{
    if (!player || player != world::playerManager->getLocalHumanPlayer())
        return;

    const int squadId = player->getSquadId();
    const int teamId = player->getTeam();
    Squad* const squad = getSquad(teamId, squadId);
    if (!squad)
        return;

    SquadOrder* const squadOrder = squad->getSquadOrders();
    if (squadOrder && player->getSquadId())
        addOrderMarker(0, *squadOrder);
    else
        removeOrderMarker(0);

    SquadOrder* const commanderOrder = squad->getCommanderOrders();
    if (commanderOrder && commanderOrder->orderId > 3 && player->getIsSquadLeader())
        addOrderMarker(1, *commanderOrder);
    else
        removeOrderMarker(1);
}


// Returns the team's commander id or -1. Servers consult Team state; clients scan
// the team's score-sorted players for the commander flag and require playerManager.
int SquadManager::getCommander(int team)
{
    if (gameLogic->isServer())                                     // IGameLogic vslot +0x13C
    {
        Team* const t = getTeam(team);
        if (!t)
            return -1;
        return t->getCommander();                                  // tail-call to +0x14
    }

    // the client arm: scan the scoreboard for the commander flag
    const std::list<world::IPlayer*>& players =
        world::playerManager->getPlayersSortedByScore(team);        // +0x4C
    for (std::list<world::IPlayer*>::const_iterator i = players.begin(); i != players.end(); ++i)
    {
        if ((*i)->getIsCommander())                                // +0x114
            return (*i)->getId();                                  // +0x54
    }
    return -1;
}

// Removes an existing member. Kicking first revokes their invitation through the
// virtual inviteToSquad call. Missing teams, squads, or members are ignored.
void SquadManager::removeFromSquad(int teamId, int squadId, int playerId, bool kicked)
{
    Team* const team = getTeam(teamId);
    if (!team)
        return;
    Squad* const squad = team->getSquad(squadId);               // narrows to unsigned char
    if (!squad)
        return;
    if (squad->m_members.find(playerId) == squad->m_members.end())   // +0x140
        return;
    if (kicked)
        inviteToSquad(teamId, squadId, playerId, false, false);  // virtual, this table
    squad->removeFromSquad(playerId, false, kicked);
}

// resignCommander - ELF 0x811E800, 43 B / removeCommanderApplicant - ELF 0x811E830, 58 B. Both
// are getTeam-guarded forwarders into Team; a team id the manager does not hold is ignored.
void SquadManager::resignCommander(int teamId)
{
    if (Team* const team = getTeam(teamId))
        team->resignCommander();
}

void SquadManager::removeCommanderApplicant(int teamId, int playerId)
{
    if (Team* const team = getTeam(teamId))
        team->removeCommanderApplicant(playerId);
}

} // namespace hfe
} // namespace dice
