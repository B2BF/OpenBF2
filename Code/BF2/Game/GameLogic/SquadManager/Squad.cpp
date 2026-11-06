// dice::hfe::Squad - construction, order state, membership, leader/spawn-point maintenance and
// teardown. See Squad.h for the measured layout.
// path: [INFERRED] filename in the attested SquadManager/ directory.

#include "Squad.h"

#include "Team.h"
#include "ISquadManager.h"                           // reset goes out through the global and back in
#include "../SpawnSystem/SpawnGroup.h"
#include "../SpawnSystem/SpawnManager.h"
#include "../../IGame.h"
#include "../../Objects/IObject.h"
#include "../../Objects/IPlayer.h"
#include "../../../Core/Ids.h"
#include "../../../AI/AIInterface/AIInstance.h"
#include "../../../AI/AIInterface/IAIEventCourier.h"
#include "../../../AI/AIInterface/SingletonServer.h"
#include "../../../AI/AIInterface/BFKnowledge/AIFrontier.h"
#include "../../../Core/EventManager.h"              // eventManager->postEvent - removeFromSquad
#include "../../Common/GameEvent.h"                  // PostRemoteEvent - the ECHud 20/21 sends
#include "../../IGameLogic.h"                        // gameLogic->isServer
#include "../../Python/PythonHost.h"                 // buildArgs<IPlayer*,int,int> - event 11
#include "../../Python/PyRef.h"
#include "../../../Scene/Object/PlayerManager.h"     // getPlayerFromId (+0x20)
#include "../../Objects/SpawnPoint.h"                // getSpawnPoint / ISpawnPoint - addSquadLeaderSpawnPoint
#include "../../Objects/IObjectTemplate.h"           // createObject (+0x54)
#include "../../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"   // getTemplate by name
#include <set>
#include <string>

#include <vector>

namespace dice {
namespace hfe {

namespace world {
void findSpawnPoints(IObject* root, std::vector<IObject*>& out);
}

// The tree's form of the global - Core/Globals.cpp declares it IBase*, and every consumer downcasts
// (SpawnGroupEvents.cpp does the same).
extern IBase* spawnManager;                          // ds:0x8AD3A00
extern IBase* squadManager;                          // ds:0x8ACBFA4 - Core/Globals.cpp
extern IGame* game;                                  // ds:0x8AC9C40 - Core/Globals.cpp

// Creates a squad and reserves its dynamic leader spawn group. Requires a nonnull Team.
// The group initially has squad ID -1; Team::createSquad assigns the final ID afterward.
// Order presence flags gate records whose leading and trailing fields remain uninitialized.
Squad::Squad(Team* team)
    : m_squadId(0)                                      // +0x008
    , m_leaderPlayerId(-1)                              // +0x00C  "no leader"
    , m_isPrivate(false)                                // +0x010
    , m_hasSquadOrders(false)                           // +0x011
    , m_hasCommanderOrders(false)                       // +0x012
    , m_spawnGroupId(-1)                                // +0x138
    , m_team(team)                                      // +0x13C
{
    // The six order blocks: only the three Vec3s at bytes +8..+43 are cleared. orderId,
    // targetNetId, both padding regions and flag2C remain untouched, as they do in both ports.
    for (int p = 0; p < 3; ++p)
    {
        m_squadOrders.points[p].x = 0.0f;
        m_squadOrders.points[p].y = 0.0f;
        m_squadOrders.points[p].z = 0.0f;
    }
    for (int r = 0; r < 4; ++r)
    {
        for (int p = 0; p < 3; ++p)
        {
            m_requests[r].points[p].x = 0.0f;
            m_requests[r].points[p].y = 0.0f;
            m_requests[r].points[p].z = 0.0f;
        }
    }
    for (int p = 0; p < 3; ++p)
    {
        m_commanderOrders.points[p].x = 0.0f;
        m_commanderOrders.points[p].y = 0.0f;
        m_commanderOrders.points[p].z = 0.0f;
    }

    for (int i = 0; i < 4; ++i)
        m_hasRequest[i] = false;                        // +0x013 .. +0x016

    // the dynamic spawn group - one per squad, at construction.
    SpawnManager* const spawn = static_cast<SpawnManager*>(spawnManager);
    m_spawnGroupId = spawn->createDynamicSpawnGroup();               // vt +0x38
    if (m_spawnGroupId >= 0)
    {
        ISpawnGroup* const group = spawn->getSpawnGroup(m_spawnGroupId);   // vt +0x28
        if (group)
        {
            group->setTeam(m_team->getTeamId());        // vt +0x18
            group->setSquad(-1);                        // vt +0x20  -1, not m_squadId - see above
            group->setActive(true);                     // vt +0x28
        }
    }
}

// Destroys owned containers but leaves the spawn group registered. m_team is a
// non-owning back-pointer and is not destroyed.
Squad::~Squad()
{
    // m_members and m_squadName destroy themselves. m_spawnGroupId is deliberately abandoned.
}

// Order accessors return null when no order exists, distinct from a zero-valued order.
const std::string& Squad::getSquadName() { return m_squadName; }     // : `add eax, 4`
bool  Squad::getIsPrivate()              { return m_isPrivate; }     // : byte [eax+0x10]
int   Squad::getSquadId()                { return m_squadId; }       // : [eax+8]
int   Squad::getSpawnGroupId()           { return m_spawnGroupId; }  // : [eax+0x138]

SquadOrder* Squad::getSquadOrders()
{
    return m_hasSquadOrders ? &m_squadOrders : 0;       // : test +0x11, then `add eax, 18h`
}

SquadOrder* Squad::getCommanderOrders()
{
    return m_hasCommanderOrders ? &m_commanderOrders : 0;   // test +0x12, then `add eax, 108h`
}

// IT clamps rather than rejects. A negative type becomes 0; anything >= 4 becomes 3. So an
// out-of-range SquadOrderType silently reads request 0 or request 3 instead of failing.
SquadOrder* Squad::getRequest(SquadOrderType type)
{
    int index = 0;
    if ((int)type >= 0)
    {
        index = (int)type;
        if (index >= 4)
            index = 3;                                  // clamp, not reject
    }
    return m_hasRequest[index] ? &m_requests[index] : 0;    // the +0x13 gate, then +index*0x30+0x48
}

// getIsAccepted - ELF 0x8119810, 95 B / PE 0x45D020, 74 B. Unlike getRequest, the request arm
// rejects an out-of-range index rather than clamping it. An absent order always answers false;
// flag2C is consulted only after the matching validity flag succeeds.
bool Squad::getIsAccepted(SquadOrderType squadOrderType, OrderType orderType)
{
    const int order = static_cast<int>(orderType);
    const int request = static_cast<int>(squadOrderType);

    if (order == 1 && m_hasCommanderOrders)
        return m_commanderOrders.flag2C;
    if (order == 0 && m_hasSquadOrders)
        return m_squadOrders.flag2C;
    if (order == 2 && static_cast<unsigned int>(request) < 4u && m_hasRequest[request])
        return m_requests[request].flag2C;
    return false;
}

// Copies order state and marks it valid before refreshing eligible local markers.
// Commander markers require local leadership; request orderId is an unchecked index.
void Squad::issueSquadOrder(SquadOrder& order)
{
    m_hasSquadOrders = true;
    m_squadOrders = order;

    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (player && player->getSquadId() == getSquadId()
        && player->getTeam() == m_team->getTeamId())
    {
        static_cast<ISquadManager*>(squadManager)->updateOrderMarkers(player);
    }
}

void Squad::issueCommanderOrder(SquadOrder& order)
{
    m_hasCommanderOrders = true;
    m_commanderOrders = order;

    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (player && player->getSquadId() == getSquadId()
        && player->getTeam() == m_team->getTeamId()
        && player->getIsSquadLeader())
    {
        static_cast<ISquadManager*>(squadManager)->updateOrderMarkers(player);
    }
}

void Squad::issueRequest(SquadOrder& order)
{
    // No range check in either shipped port: malformed local state can index both arrays out of
    // bounds. Network deserialization narrows the id, but this method itself does not defend it.
    m_hasRequest[order.orderId] = true;
    m_requests[order.orderId] = order;
}

// removeSquadLeaderSpawnPoint - ELF 0x811A4A0, 245 B / PE 0x460650, 186 B. A valid soldier root
// loses every spawn point in its subtree. Retail repeats getDefaultVehicle at all four sites; the
// getter can clean a stale weak block, so those calls are deliberately not coalesced.
void Squad::removeSquadLeaderSpawnPoint(world::IPlayer* player)
{
    if (!player)
        return;
    if (!player->getDefaultVehicle())
        return;
    if (!player->getDefaultVehicle()->queryInterface(world::IID_ISoldier))
        return;

    std::vector<world::IObject*> spawnPoints;
    world::findSpawnPoints(player->getDefaultVehicle(), spawnPoints);

    for (int i = 0; i < static_cast<int>(spawnPoints.size()); ++i)
    {
        world::IObject* const spawnPoint = spawnPoints[i];
        player->getDefaultVehicle()->removeChild(spawnPoint);
        game->destroyObject(spawnPoint);
    }
}

// setSquadName - ELF 0x811A7F0, 36 bytes. A string assign and an unconditional `return 1`.
// THE bool IS always true (`mov eax, 1` at 0x811A80D) - it cannot fail, and no caller could learn
// anything from it. Reproduced as the constant it is.
bool Squad::setSquadName(const std::string& name)
{
    m_squadName = name;                                 // string::assign on this+4
    return true;                                        // always
}

// Clears order validity before transferring leadership, preserving members and
// privacy. [UNRESOLVED] The two-argument overload's leading literal 1.
bool Squad::setSquadLeader(int playerId)
{
    reset(false);
    return setSquadLeader(1, playerId);                 // the 1 is [UNRESOLVED]
}

// Changes acceptance only for an existing order. Declining also clears its presence flag.
// Request indices outside 0..3 are ignored, unlike getRequest's clamping behavior.
void Squad::setIsAccepted(bool accepted, OrderType orderType, SquadOrderType squadOrderType)
{
    const int type = static_cast<int>(orderType);
    const int slot = static_cast<int>(squadOrderType);

    if (type == 1)                                          // commander orders
    {
        if (m_hasCommanderOrders)
        {
            m_commanderOrders.flag2C       = accepted;      // 0x811.98DB - SquadOrder +44
            m_hasCommanderOrders           = accepted;      // false deletes the order
        }
    }
    else if (type == 0)                                     // squad orders
    {
        if (m_hasSquadOrders)
        {
            m_squadOrders.flag2C       = accepted;
            m_hasSquadOrders           = accepted;
        }
    }
    else if (type == 2)                                     // one of the four requests
    {
        if (slot >= 0 && slot <= 3 && m_hasRequest[slot])   // signed, inclusive
        {
            m_requests[slot].flag2C       = accepted;       // 48*slot stride
            m_hasRequest[slot]            = accepted;
        }
    }
}

// Clears order state through the global manager with notifications suppressed.
// Requires the owning team to be registered. A full reset also clears members, leader and privacy;
// round resets preserve those fields. [CHOSEN] The return type is void.
void Squad::reset(bool full)
{
    // SIX separate calls, NOT A loop - 589 bytes of straight-line code, and each one re-fetches
    // getSquadId() through the vtable and getTeamId() through the back-pointer. Written out rather
    // than folded into a table so the pair order (note 3 before 2) stays visible as transcription.
    ISquadManager* const manager = static_cast<ISquadManager*>(squadManager);   // ds:0x8ACBFA4

    #define BF2_SQUAD_RESET_ACCEPT(order, sot)                                                    manager->setIsAccepted(m_team->getTeamId(), getSquadId(), false,       /* vt +0x24 */                                 static_cast<OrderType>(order),                                                                 static_cast<SquadOrderType>(sot), true)         /* ★ no event */

    BF2_SQUAD_RESET_ACCEPT(0, 5);      // squad orders     - squadOrderType unread by this arm
    BF2_SQUAD_RESET_ACCEPT(1, 5);      // commander orders - squadOrderType unread by this arm
    BF2_SQUAD_RESET_ACCEPT(2, 0);      // request 0
    BF2_SQUAD_RESET_ACCEPT(2, 1);      // request 1
    BF2_SQUAD_RESET_ACCEPT(2, 3);      // request 3   three before TWO - the binary's order
    BF2_SQUAD_RESET_ACCEPT(2, 2);      // request 2

    #undef BF2_SQUAD_RESET_ACCEPT

    if (full)                                               // Team::reset passes false
    {
        m_leaderPlayerId = -1;
        m_isPrivate      = false;
        m_members.clear();
    }
}

// Moves membership before broadcasting HUD/Python notifications; an empty squad
// installs the new member as leader. AI notification precedes a fresh squad lookup.
// The human joining an AI-led squad path requires a resolvable leader.
bool Squad::addToSquad(int playerId)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (!player)
        return false;

    const int oldSquadId = player->getSquadId();
    const int oldTeamId = player->getTeam();
    ISquadManager* const manager = static_cast<ISquadManager*>(squadManager);
    manager->removeFromSquad(oldTeamId, oldSquadId, playerId, false);

    const int countTeamId = player->getTeam();
    if (world::playerManager->getNrOfPlayersInSquad(countTeamId, m_squadId) == 0)
    {
        const int newLeaderId = player->getId();
        const int leaderTeamId = player->getTeam();
        manager->setSquadLeader(leaderTeamId, m_squadId, newLeaderId, false);
    }

    player->setSquadId(m_squadId);
    player->setSquadInviteMask(player->getSquadInviteMask() & ~(1u << m_squadId));
    m_members.insert(playerId);

    if (gameLogic->isServer())
    {
        for (std::set<int>::iterator it = m_members.begin(); it != m_members.end(); ++it)
        {
            if (*it == 255)
            {
                eventManager->postEvent(ECHud, 22, &playerId, 0.0f);
            }
            else
            {
                game->sendGameEventIfConnected(*it,
                    new PostRemoteEvent(ECHud, 22, false, &playerId, 4, 0.0f));
            }
        }

        python::PyRef args = python::buildArgs(player, oldSquadId, m_squadId);
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(11), args.get());
    }

    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())
    {
        // [PLATFORM] PE caches the courier before getTeam (0x45F4C4 / 0x45F4D2);
        // ELF loads it after getTeam (0x8119B58 / 0x8119B5E).
#if defined(_MSC_VER)
        ai::IAIEventCourier* const courier = ai::BFSingletonServer::m_iaiEventCourier;
#endif
        const int teamId = player->getTeam();
#if !defined(_MSC_VER)
        ai::IAIEventCourier* const courier = ai::BFSingletonServer::m_iaiEventCourier;
#endif
        courier->event_joinedSquad(playerId, m_squadId, teamId);                // ELF +0x8C / PE +0x88

        Squad* const squad = m_team->getSquad(m_squadId);
        if (squad && !player->getIsAIPlayer())
        {
            world::IPlayer* const leader =
                world::playerManager->getPlayerFromId(squad->m_leaderPlayerId);
            if (leader->getIsAIPlayer())                                      // leader unchecked
            {
                // [PLATFORM] PE captures the squad id before this repeated getter;
                // ELF reads it after (0x45F542 / 0x45F550; 0x8119BDA / 0x8119BE9).
#if defined(_MSC_VER)
                const int countSquadId = m_squadId;
#endif
                const int countTeamId = player->getTeam();
#if !defined(_MSC_VER)
                const int countSquadId = m_squadId;
#endif
                if (world::playerManager->getNrOfPlayersInSquad(countTeamId, countSquadId) > 1)
                {
                    game->sendGameEventIfConnected(playerId,
                        new PostRemoteEvent(ECHud, 36, true, &playerId, 4, 0.0f));
                }
            }
        }
    }
    return true;
}

// Moves a departing player to the unassigned squad, notifies remaining members and reelects a leader.
// Captures the squad ID before a possible empty-squad reset. Leader reelection requires a live player.
// [INFERRED] skipScriptEvent suppresses Python notification; kicked chooses the HUD message.
// The AI courier notification remains a named stop before the empty-squad reset.
bool Squad::removeFromSquad(int playerId, bool skipScriptEvent, bool kicked)
{
    const int squadId = m_squadId;                                              // captured first

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);   // +0x20
    if (player && player->getSquadId())                                         // +0x10C
    {
        player->setSquadId(0);                                                  // +0x108
        if (player->getIsSquadLeader())                                         // +0x13C
            removeSquadLeaderSpawnPoint(player);                                // virtual, +0x24
        player->setIsSquadLeader(false);                                        // +0x138

        // back into the team's "Unassigned" pool - squad 0's member set, reached through the
        // interface and its offset-zero concrete class.
        ISquadInfo* const unassigned =
            static_cast<ISquadManager*>(squadManager)->getSquadInfo(player->getTeam(), 0);   // +0x28
        static_cast<Squad*>(unassigned)->m_members.insert(playerId);            // +0x140
    }

    if (gameLogic->isServer())                                                  // +0x13C
    {
        const unsigned int hudId = kicked ? 20u : 21u;
        for (std::set<int>::iterator it = m_members.begin(); it != m_members.end(); ++it)
        {
            if (*it == 255)
                eventManager->postEvent(ECHud, hudId, &playerId, 0.0f);         // +0x14
            else
                game->sendGameEventIfConnected(*it,                              // IGame +0x98
                    new PostRemoteEvent(ECHud, hudId, false, &playerId, 4, 0.0f));
        }
    }

    m_members.erase(playerId);

    if (playerId == m_leaderPlayerId)                                           // +0x0C
    {
        if (!m_members.empty())                                                 // +0x144
        {
            static_cast<ISquadManager*>(squadManager)->setSquadLeader(          // +0x38
                player->getTeam(), m_squadId, *m_members.begin(), false);       // player unchecked
        }
        else
        {
            m_leaderPlayerId = -1;
        }
    }

    if (!skipScriptEvent)
    {
        const int zero = 0;
        python::PyRef args = python::buildArgs(player, squadId, zero);          // <IPlayer*,int,int>
        python::pythonHost()->handleEvent(static_cast<ScriptEvent>(11), args.get());
    }

    if (ai::g_aiInstance && ai::aiInstance()->isInitialized())
    {
        //  / PE 0x45F844 reads the team before the courier call with
        // (playerId, current m_squadId, teamId), ELF +0x90 / PE +0x8C.
        const int teamId = player->getTeam();                                  // player unchecked
        ai::BFSingletonServer::m_iaiEventCourier->event_removedFromSquad(playerId, m_squadId, teamId);
    }

    if (m_members.empty())                                                      // +0x144
        reset(true);
    return true;
}

// Transfers leadership flags and server-side spawn points. Stores playerId even
// when that player is absent. [UNRESOLVED] The first parameter is unused; retain
// it in the signature. Spawn-point updates remain virtual calls.
bool Squad::setSquadLeader(int /*unused - [UNRESOLVED]*/, int playerId)
{
    if (m_leaderPlayerId != -1)
    {
        world::IPlayer* const old = world::playerManager->getPlayerFromId(m_leaderPlayerId);
        if (old)
        {
            old->setIsSquadLeader(false);                                       // +0x138
            if (old->getDefaultVehicle() && gameLogic->isServer())              // +0xA4 / +0x13C
                removeSquadLeaderSpawnPoint(old);                               // virtual +0x24
        }
    }

    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    if (player)
    {
        player->setIsSquadLeader(true);                                         // +0x138
        player->setSquadId(m_squadId);                                          // +0x108
    }
    m_leaderPlayerId = playerId;                                                // +0x0C

    if (player && player->getDefaultVehicle() && gameLogic->isServer())
        addSquadLeaderSpawnPoint(player);                                       // virtual +0x20
    return true;
}

// Attaches or reuses the leader soldier's first spawn point and allows spawned
// squadmates to enter the leader's vehicle. Requires a default vehicle when player
// is nonnull; absent group, soldier interface, or spawn template leaves state unchanged.
void Squad::addSquadLeaderSpawnPoint(world::IPlayer* player)
{
    if (!player)
        return;
    if (m_spawnGroupId < 0)                                                     // +0x138
        return;
    if (!player->getDefaultVehicle()->queryInterface(world::IID_ISoldier))     // unchecked body
        return;

    const bool inVehicle = player->getDefaultVehicle() != player->getVehicle();   // +0xA4 / +0x48

    std::vector<world::IObject*> spawnPoints;
    world::findSpawnPoints(player->getDefaultVehicle(), spawnPoints);

    ISpawnPoint* point;
    if (!spawnPoints.empty())
    {
        point = getSpawnPoint(spawnPoints[0]);
    }
    else
    {
        world::IObjectTemplate* const templ =
            world::objectTemplateManager->getTemplate(std::string("SquadLeaderSpawnPoint"));   // +0x24
        if (!templ)
            return;
        world::IObject* const object = templ->createObject();                  // +0x54
        player->getDefaultVehicle()->addChild(object);                          // +0xF4
        ISpawnPoint* const created = getSpawnPoint(object);
        static_cast<SpawnManager*>(spawnManager)->addSpawnPoint(created, m_spawnGroupId);   // +0x18
        point = getSpawnPoint(object);                                          // asked again
    }
    point->setOverrideEnterOnSpawn(inVehicle);                                  // +0x3C
}

} // namespace hfe
} // namespace dice
