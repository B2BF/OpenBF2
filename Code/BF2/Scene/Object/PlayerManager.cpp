// Player registry with case-insensitive name lookup, ID lookup and inactive-player parking.
// Also supplies team/squad views, score/rank scratch lists and player-class names.
// Use typed IPlayer calls so destructor-slot differences between PE and ELF remain correct.
// Source placement [INFERRED].

#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../../Platform/X87Compat.h" // [CHOSEN, not recovered]
#endif
#include "PlayerManager.h"
#include "PlayerScoreData.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "../../Core/Ids.h"     // IID_IBase, world::IID_IPlayerManager
#include "../../Core/StringUtil.h"          // class-name prefix comparison
#include "../../IO/File/File.h"
#include "../../IO/Console/IConsoleSaveable.h"
#include "../../Game/Objects/IPlayer.h"   // getIsConnected / getIsAIPlayer - getNumberOfAliveHumanPlayers
#include "../../Game/GameLogic/Player.h"  // the concrete class - updatePlayers downcasts to it
#include "../../Game/Objects/Fx/PersistentExplosionEffectManager.h"   // the singleton this ctor builds
#include "../../Game/SingletonServer.h"   // player add/rename/remove notifications
#include "../../Game/IGameLogic.h"        // removePlayer's server-only Python cleanup gate
#include "../../Game/IGame.h"
#include "../../Game/GameServer/ServerSettings.h"
#include "../../Game/Python/PythonHost.h" // removePlayer's final Python wrapper invalidation
#include "../../Platform/MsvcRand.h"  // [CHOSEN, not recovered] the client CRT's rand (MSVCR71)

namespace dice {
namespace hfe {
namespace world {

// construction and destruction

// Constructs empty registries and scratch views with one initial reference and no active player.
// The factory returns that reference without adding another.
PlayerManager::PlayerManager()
    : m_refCount(1)
    , m_activePlayer(0)
    , m_players()
    , m_playersByScore()
    , m_playersByRank()
    , m_playerNames()               // Core/Hash.h's default maxSize is 3011 (0BC3h)
    , m_playerClasses()
    , m_playersById()
    , m_inactivePlayers()
    , m_localHumanPlayer(0)
    , m_unknown4C(0)
{
    // PlayerManager owns the shared explosion-effect manager for its entire lifetime.
    explosionEffectManager = new CPersistentExplosionEffectManager();   // ds:0x8AB08E0
}

// Clears player-list and name-index nodes without deleting or releasing their borrowed players.
// Then deletes and nulls the persistent-explosion manager before member-container destruction.
PlayerManager::~PlayerManager()
{
    m_players.clear();
    m_playerNames.clear();

    delete explosionEffectManager;           // the singleton the constructor built
    explosionEffectManager = 0;
}

// the IBase quartet

// Vtable +0x00. Materialises the new count into eax; the declared return
// type stays `void` to match Core/IBase.h, which every other class in the tree follows.
void PlayerManager::addRef()
{
    ++m_refCount;
}

// Vtable +0x04.
int PlayerManager::getRef()
{
    return m_refCount;
}

// Vtable +0x08. Returns the NEW count (`mov eax, ebx` on both paths).

// `test edx, edx / jz` at 0x83035B7 is the null-`this` test the compiler inserts for `delete this`,
// not a statement. The call it guards is `[eax+0BCh]` - the deleting destructor, vtable +0xBC.
int PlayerManager::release()
{
    // ELF 83035A0 keeps EBX; PE 64F960 keeps ESI across virtual deletion.
    const int remaining = --m_refCount; // 0x83035AF  (`dec ebx`, stored back at 0x83035B2)

    if (remaining == 0)
        delete this;                    // 0x83035C0, call [eax+0BCh]

    return remaining;                  // no read from the freed object
}

// Returns IBase or IPlayerManager for the corresponding global id, otherwise null.
// Both interfaces share the same address in this inheritance layout.
void* PlayerManager::queryInterface(unsigned int iid) const
{
    if (iid == IID_IBase)
        return const_cast<PlayerManager*>(this);

    if (iid == IID_IPlayerManager)                              // 0x83035F5, cmovz
        return const_cast<PlayerManager*>(this);

    return 0;
}

// the four accessors that do not touch IPlayer's vtable

// PlayerManager::getLocalHumanPlayer - Vtable +0x30.

// BF2Engine::mainLoop calls this slot. Eleven bytes: load `this`, `mov eax,[eax+48h]`, return.
// A plain load of +0x48, no null check, no side effect - so on a dedicated server, where nothing
// ever calls setLocalHumanPlayer, it returns the constructor's 0 forever and mainLoop's null test is
// the only thing standing between that and a fault.
IPlayer* PlayerManager::getLocalHumanPlayer()
{
    return m_localHumanPlayer;
}

// The unfiltered getter exposes the live player registry without a mutation latch.
// Team filtering returns shared scratch storage, overwritten by later filtered queries.
// m_playersByScore is an [INFERRED] historical name for that scratch list.
const std::list<IPlayer*>& PlayerManager::getPlayers(int team)
{
    m_playersByScore.resize(0);
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        if ((*it)->getTeam() == team)
            m_playersByScore.insert(m_playersByScore.end(), *it);
    }
    return m_playersByScore;
}

const std::list<IPlayer*>& PlayerManager::getPlayers()
{
    return m_players;
}

// Returns team/squad members excluding the commander. Shared filtering and sorting
// reuse scratch lists; typed player calls preserve PE/ELF slot differences.
const std::list<IPlayer*>& PlayerManager::getPlayersInSquad(int team, int squad)
{
    m_playersByScore.resize(0);
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getTeam() == team
            && player->getSquadId() == squad
            && !player->getIsCommander())
        {
            m_playersByScore.insert(m_playersByScore.end(), player);
        }
    }
    return m_playersByScore;
}

// ELF 0x8300920 / PE 0x450CA0. Retail asks leader first, then team.
const std::list<IPlayer*>& PlayerManager::getTeamSquadLeaders(int team)
{
    m_playersByScore.resize(0);
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getIsSquadLeader() && player->getTeam() == team)
            m_playersByScore.insert(m_playersByScore.end(), player);
    }
    return m_playersByScore;
}

// ELF 0x8300A10 / PE 0x450120. Unlike getPlayersInSquad, this count does not ask commander state.
int PlayerManager::getNrOfPlayersInSquad(int team, int squad)
{
    int count = 0;
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getTeam() == team && player->getSquadId() == squad)
            ++count;
    }
    return count;
}

// ELF 0x8300AA0 / PE 0x450D40. The requested squad is excluded before its invitation bit is read.
const std::list<IPlayer*>& PlayerManager::getInvitedPlayers(int team, int squad)
{
    m_playersByScore.resize(0);
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getTeam() == team
            && player->getSquadId() != squad
            && player->getIsInvitedToSquad(squad))
        {
            m_playersByScore.insert(m_playersByScore.end(), player);
        }
    }
    return m_playersByScore;
}

// ELF 0x8300B90 / PE 0x44FE40. Squad zero is not an active squad; retail probes exactly 1..9.
int PlayerManager::getNrOfActiveSquads(int team)
{
    int count = 0;
    for (int squad = 1; squad <= 9; ++squad)
    {
        if (getNrOfPlayersInSquad(team, squad) >= 1)
            ++count;
    }
    return count;
}

// ELF 0x8301FD0 / PE 0x450350. Any nonzero squad id counts, including values outside 1..9.
int PlayerManager::getNrOfPlayerInSquads(int team)
{
    int count = 0;
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getTeam() == team && player->getSquadId() != 0)
            ++count;
    }
    return count;
}

// ELF 0x8300D80 / PE 0x450DF0. This is a repeated best-candidate selection, not list::sort.
// Rank compares unsigned (the constructor's -1 sentinel therefore participates as 0xFFFFFFFF),
// then total score descending, field +0x08 ascending, field +0x10 ascending and connection time
// descending. getScoreData() is intentionally called again for every comparison and saved field.
const std::list<IPlayer*>& PlayerManager::sortByRank(std::list<IPlayer*> players)
{
    m_playersByRank.resize(0);

    while (players.size() != m_playersByRank.size())
    {
        unsigned int bestRank = 0;
        int bestScore = -999999;
        int bestField08 = -999999;
        int bestField10 = 999999;
        double bestTimeConnected = 999999.0;
        IPlayer* bestPlayer = 0;

        for (std::list<IPlayer*>::iterator it = players.begin(); it != players.end(); ++it)
        {
            IPlayer* player = *it;
            bool alreadySorted = false;
            for (std::list<IPlayer*>::iterator sorted = m_playersByRank.begin();
                 sorted != m_playersByRank.end();
                 ++sorted)
            {
                if (*sorted == player)
                    alreadySorted = true;
            }

            if (alreadySorted)
                continue;

            bool choose = false;
            if (static_cast<unsigned int>(player->getScoreData()->m_rank) > bestRank)
            {
                choose = true;
            }
            else if (static_cast<unsigned int>(player->getScoreData()->m_rank) == bestRank)
            {
                if (player->getScoreData()->m_score > bestScore)
                {
                    choose = true;
                }
                else if (player->getScoreData()->m_score == bestScore)
                {
                    if (player->getScoreData()->m_field08 < bestField08)
                    {
                        choose = true;
                    }
                    else if (player->getScoreData()->m_field08 == bestField08)
                    {
                        if (player->getScoreData()->m_field10 < bestField10)
                        {
                            choose = true;
                        }
                        else if (player->getScoreData()->m_field10 == bestField10
                                 && player->getTimeConnected() > bestTimeConnected)
                        {
                            choose = true;
                        }
                    }
                }
            }

            if (choose)
            {
                bestRank = static_cast<unsigned int>(player->getScoreData()->m_rank);
                bestScore = player->getScoreData()->m_score;
                bestField08 = player->getScoreData()->m_field08;
                bestField10 = player->getScoreData()->m_field10;
                bestTimeConnected = player->getTimeConnected();
                bestPlayer = player;
            }
        }

        if (!bestPlayer)
            break;

        m_playersByRank.insert(m_playersByRank.end(), bestPlayer);
    }

    return m_playersByRank;
}

// ELF 0x8301120 / PE 0x451070. Total score and field +0x08 are descending; field +0x10 is
// ascending; connection time is descending. The sentinels and repeated virtual reads are shipped.
const std::list<IPlayer*>& PlayerManager::sortByScore(std::list<IPlayer*> players)
{
    m_playersByRank.resize(0);

    while (players.size() != m_playersByRank.size())
    {
        int bestScore = -999999;
        int bestField08 = -999999;
        int bestField10 = 999999;
        double bestTimeConnected = 999999.0;
        IPlayer* bestPlayer = 0;

        for (std::list<IPlayer*>::iterator it = players.begin(); it != players.end(); ++it)
        {
            IPlayer* player = *it;
            bool alreadySorted = false;
            for (std::list<IPlayer*>::iterator sorted = m_playersByRank.begin();
                 sorted != m_playersByRank.end();
                 ++sorted)
            {
                if (*sorted == player)
                    alreadySorted = true;
            }

            if (alreadySorted)
                continue;

            bool choose = false;
            if (player->getScoreData()->m_score > bestScore)
            {
                choose = true;
            }
            else if (player->getScoreData()->m_score == bestScore)
            {
                if (player->getScoreData()->m_field08 > bestField08)
                {
                    choose = true;
                }
                else if (player->getScoreData()->m_field08 == bestField08)
                {
                    if (player->getScoreData()->m_field10 < bestField10)
                    {
                        choose = true;
                    }
                    else if (player->getScoreData()->m_field10 == bestField10
                             && player->getTimeConnected() > bestTimeConnected)
                    {
                        choose = true;
                    }
                }
            }

            if (choose)
            {
                bestScore = player->getScoreData()->m_score;
                bestField08 = player->getScoreData()->m_field08;
                bestField10 = player->getScoreData()->m_field10;
                bestTimeConnected = player->getTimeConnected();
                bestPlayer = player;
            }
        }

        if (!bestPlayer)
            break;

        m_playersByRank.insert(m_playersByRank.end(), bestPlayer);
    }

    return m_playersByRank;
}

// The four public sorting families all copy their selected source list into the helpers' attested
// by-value argument. PE optimizer folds the two global forms to 0x4512B0/0x4512D0 and the team
// forms to 0x4513B0/0x4513E0; ELF keeps the explicit list copy at the addresses in the header.
const std::list<IPlayer*>& PlayerManager::getPlayersSortedByScore()
{
    return sortByScore(m_players);
}

const std::list<IPlayer*>& PlayerManager::getPlayersSortedByRank()
{
    return sortByRank(m_players);
}

const std::list<IPlayer*>& PlayerManager::getPlayersSortedByScore(int team)
{
    return sortByScore(getPlayers(team));
}

const std::list<IPlayer*>& PlayerManager::getPlayersSortedByRank(int team)
{
    return sortByRank(getPlayers(team));
}

// ELF 0x8300BE0/0x8300CB0; PE 0x451410/0x451440. getPlayersInSquad runs virtually first, then its
// +0x10 result is copied before the chosen sorter clears/refills +0x14.
const std::list<IPlayer*>& PlayerManager::getPlayersInSquadSortedByScore(int team, int squad)
{
    return sortByScore(getPlayersInSquad(team, squad));
}

const std::list<IPlayer*>& PlayerManager::getPlayersInSquadSortedByRank(int team, int squad)
{
    return sortByRank(getPlayersInSquad(team, squad));
}

// Vtable +0x80. const. `mov eax,[eax+8]`.
IPlayer* PlayerManager::getActivePlayer() const
{
    return m_activePlayer;
}

// Vtable +0x84. A plain store to +0x08 - no addRef, no notification, and
// no null check.
void PlayerManager::setActivePlayer(IPlayer* player)
{
    m_activePlayer = player;
}

// ds:0x8AB08E0 - Core/Globals.cpp owns it. Declared here rather than included because the class
// itself (world::CPersistentExplosionEffectManager, 4 bytes) has no header yet; Player only
// stores the pointer, so a forward declaration is the whole requirement.
class CPersistentExplosionEffectManager;
extern CPersistentExplosionEffectManager* explosionEffectManager;

// Returns the player from the active id map, or null. Does not search inactive players.
IPlayer* PlayerManager::getPlayerFromId(int id)
{
    std::map<int, IPlayer*>::const_iterator it = m_playersById.find(id);
    if (it == m_playersById.end())
        return 0;
    return it->second;
}

// Adds the player to the ordered list, id map, and name hash, then unconditionally
// makes it the active player.
void PlayerManager::addPlayer(IPlayer* player)
{
    // ELF 0x83003B4 (+0x1C) / PE 0x4508E7 (+0x30): before every registry mutation.
    SingletonServer::m_iNametagManager->addTouchItem(player);

    m_players.insert(m_players.end(), player);
    m_playersById.insert(std::make_pair(player->getId(), player));

    // The name hash, hashed once and reused - see Hash.h. The ELF NUL-terminates the key in
    // place before strcasecmp; c_str() is the same guarantee without writing through the rep.
    const std::string& name = player->getName();
    const unsigned int bucket = m_playerNames.bucketOf(name);
    if (!m_playerNames.exists(name, bucket))
        m_playerNames.insert(name, player, bucket);

    setActivePlayer(player);                                    // +0x84 - the tail call
}

// Removes borrowed player entries in nametag/name/list/ID order, without releasing the player.
// Always clears active player; clears local-human state for this player or an empty registry.
// Requires the nametag singleton. Only servers notify Python before external destruction.
void PlayerManager::removePlayer(IPlayer* player)
{
    SingletonServer::m_iNametagManager->removeItem(0, player);       // ELF +0x4C / PE +0x50
    m_playerNames.remove(player->getName());
    m_players.remove(player);

    const int id = player->getId();
    std::map<int, IPlayer*>::iterator byId = m_playersById.find(id);
    if (byId != m_playersById.end())
        m_playersById.erase(byId);

    setActivePlayer(0);

    if (m_localHumanPlayer == player)
        m_localHumanPlayer = 0;
    if (m_players.empty())
        m_localHumanPlayer = 0;

    if (gameLogic->isServer())
        python::pythonHost()->removePlayer(player);
}

// PlayerManager::removePlayerId(int) - / Vtable +0x18.
// The other disconnect arm: a connection that never acquired a Player still reserved its numeric
// id in m_playersById. A find followed by iterator erase, and no other state or ownership change.
void PlayerManager::removePlayerId(int id)
{
    std::map<int, IPlayer*>::iterator byId = m_playersById.find(id);
    if (byId != m_playersById.end())
        m_playersById.erase(byId);
}

// ELF 0x83023D0 / PE 0x450A80. Remove only the first matching list node, then the supplied ID.
// A later failure does not roll back the list/map removals. The name hash and active player stay.
bool PlayerManager::deactivatePlayer(int id)
{
    IPlayer* player = getPlayerFromId(id);
    if (!player)
        return false;

    std::list<IPlayer*>::iterator inList =
        std::find(m_players.begin(), m_players.end(), player);
    if (inList == m_players.end())
        return false;

    m_players.erase(inList);
    std::map<int, IPlayer*>::iterator byId = m_playersById.find(id);
    if (byId == m_playersById.end())
        return false;

    m_playersById.erase(byId);
    if (m_localHumanPlayer == player)
        m_localHumanPlayer = 0;
    return m_inactivePlayers.insert(std::make_pair(id, player)).second;
}

// ELF 0x8302590 / PE 0x450B40. Append, insert the player's current ID (ignoring collision),
// erase the supplied inactive key, then select a local human. There is no refcount/name-hash work.
bool PlayerManager::reactivatePlayer(int id)
{
    std::map<int, IPlayer*>::iterator inactive = m_inactivePlayers.find(id);
    if (inactive == m_inactivePlayers.end())
        return false;

    m_players.insert(m_players.end(), inactive->second);
    const int activeId = inactive->second->getId();
    m_playersById.insert(std::make_pair(activeId, inactive->second));
    m_inactivePlayers.erase(id);

    // shipped defect: both ports dereference the erased iterator again here (ELF
    // 0x8302654..0x8302691 / PE 0x450BCA..0x450BE3). Retain this invalidated-iterator read;
    // caching the player or moving erase after the callbacks would be unrequested hardening.
    if (!inactive->second->getIsRemote() && !inactive->second->getIsAIPlayer())
        m_localHumanPlayer = inactive->second;
    return true;
}

// Creates a concrete Player; classId is ignored. Duplicate names and allocation failure clear
// the active player, while an occupied explicit ID returns null without changing it.
// Negative IDs probe for a free slot but intentionally pass the original negative ID to the constructor.
IPlayer* PlayerManager::createPlayer(unsigned int classId, const std::string& name, int playerId)
{
    (void)classId;      // deliberate - see above. Not an oversight.

    if (playerId < 0)
    {
        // Probe for a free id, but do not assign it back: retail discards the candidate.
        int free = 0;
        while (getPlayerFromId(free))
            ++free;
    }
    else if (getPlayerFromId(playerId))
    {
        return 0;                                   // that id is taken
    }

    const unsigned int bucket = m_playerNames.bucketOf(name);
    if (m_playerNames.exists(name, bucket))
    {
        setActivePlayer(0);
        return 0;                                   // that name is taken - the ladder's trigger
    }

    // PlayerManager's constructor owns the explosion-effect singleton passed to each player.
    Player* player = new Player(playerId, world::explosionEffectManager);
    if (!player)
    {
        setActivePlayer(0);
        return 0;
    }

    player->setName(name);
    addPlayer(player);
    return player;
}

// ELF 0x8301D50 / PE 0x4501C0. Compare only the requested name's length, case-insensitively.
// Thus a prefix (including the empty string) selects the first match in ascending class-ID order.
// The virtual numeric overload receives that ID plus the original player name and requested ID.
IPlayer* PlayerManager::createPlayer(const std::string& className, const std::string& name, int id)
{
    for (std::map<unsigned int, std::string>::iterator it = m_playerClasses.begin();
         it != m_playerClasses.end(); ++it)
    {
        if (compareIgnoreCase(className.c_str(), it->second.c_str(),
                              static_cast<int>(className.size())) == 0)
            return createPlayer(it->first, name, id);
    }
    return 0;
}

// ELF 0x8301E10 / PE 0x450270. Both explicitly materialize bool success/failure. Skip players
// without IConsoleSaveable; stop at the first failed script. File destruction closes every path.
bool PlayerManager::saveAllPlayers(const std::string& fileName)
{
    io::File file;
    if (!file.open(fileName, static_cast<io::IFileSystem::FileMode>(0),
                    static_cast<io::IFileSystem::FileAccess>(2), 0))
        return false;

    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        io::IConsoleSaveable* saveable = static_cast<io::IConsoleSaveable*>(
            (*it)->queryInterface(io::IID_IConsoleSaveable));
        if (saveable && !saveable->makeScript(file.getStream()))
            return false;
    }
    return true;
}

// Renames a registered player unless the requested name is already taken.
// Returns false if the player is absent from the old-name index. Remove the old entry before calling
// setName: that side effect terminates its recursive rename call. Reinsert and notify nametags afterward.
// The name is passed by value and must not alias the recursive update.
bool PlayerManager::renamePlayer(IPlayer* player, std::string name)
{
    if (getPlayerFromName(name))
        return false;                               // the new name is taken

    if (!getPlayerFromName(player->getName()))
        return false;                               // not registered under its own name

    m_playerNames.remove(player->getName());        // this is what stops the recursion
    player->setName(name);                          // recurses exactly one level - see above

    const unsigned int bucket = m_playerNames.bucketOf(name);
    if (!m_playerNames.exists(name, bucket))
        m_playerNames.insert(name, player, bucket);

    // ELF 0x8302758 / PE 0x450684: after the conditional hash insertion, unguarded.
    SingletonServer::m_iNametagManager->addTouchItem(player);
    return true;
}

// Stores the player resolved from id, including null on a miss. If found, calls
// updateClientSettings after publishing the new local-human pointer.
void PlayerManager::setLocalHumanPlayer(int id)
{
    m_localHumanPlayer = getPlayerFromId(id);        // may be null; stored either way
    if (m_localHumanPlayer)
        m_localHumanPlayer->updateClientSettings();
}

// Returns a player by case-insensitive name, or null when absent.
IPlayer* PlayerManager::getPlayerFromName(const std::string& name)
{
    IPlayer* found = 0;
    if (m_playerNames.get(name, found))
        return found;
    return 0;
}

// ELF 0x8302920 / PE 0x44FF50; isUint: ELF 0x83028E0 / PE 0x44FF00.
// Names take precedence over numeric IDs. The shipped digits-only test accepts the empty string
// (atoi then selects ID 0); signs and whitespace fail. Preserve atoi's conversion semantics.
IPlayer* PlayerManager::getPlayerFromIdString(const std::string& id)
{
    IPlayer* player = getPlayerFromName(id);
    if (player)
        return player;
    // ELF 0x83028EB / PE 0x44FF03: the length comparison is signed.
    const int length = static_cast<int>(id.size());
    for (int i = 0; i < length; ++i)
        if (id[i] < '0' || id[i] > '9')
            return 0;
    return getPlayerFromId(std::atoi(id.c_str()));
}

// ELF 0x8301F70 / PE 0x450310: one team query per live list entry, without filtering.
int PlayerManager::getNrOfPlayerInTeam(int team)
{
    int count = 0;
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
        if ((*it)->getTeam() == team)
            ++count;
    }
    return count;
}

// ELF 0x8301A10 / PE 0x450800: exact signed-ID lookup, then advance and wrap.
IPlayer* PlayerManager::getNextPlayerFromId(int id)
{
    std::map<int, IPlayer*>::iterator it = m_playersById.find(id);
    if (it == m_playersById.end())
        return 0;
    ++it;
    if (it != m_playersById.end())
        return it->second;
    if (!m_playersById.empty())
        return m_playersById.begin()->second;
    return 0;
}

// ELF 0x8302040 / PE 0x4503A0. Weight/count names are [INFERRED]. Preserve repeated
// queries, pre-mode arithmetic, unguarded denominators and RNG only on the final tie.
// [PLATFORM] Explicit float casts follow each port's spills. MSVC long double is double;
// this expresses the extended intermediates without claiming every x87 rounding edge.
int PlayerManager::getTeamToAddPlayer(float team1Weight, float team2Weight)
{
    int team1Count = 0;
    int team2Count = 0;
    int team1BotCount = 0;
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it) {
        IPlayer* player = *it;
        if (player->getTeam() == 1) {
            ++team1Count;
            if (player->getIsAIPlayer())
                ++team1BotCount;
        } else if (player->getTeam() == 2) {
            ++team2Count;
            player->getIsAIPlayer(); // Retail queries this and discards the result.
        }
    }

#if defined(_WIN32)
    const int total = team1Count + team2Count;
    const long double reciprocalWeightSum = 1.0L /
        (static_cast<long double>(team1Weight) + team2Weight);
    const float target1 = static_cast<float>(team1Weight * reciprocalWeightSum);
    const float error1 = static_cast<float>(static_cast<long double>(team1Count) / total - target1);
    const float team2AsFloat = static_cast<float>(team2Count);
    const float target2 = static_cast<float>(team2Weight * reciprocalWeightSum);
    const float error2 = static_cast<float>(static_cast<long double>(team2AsFloat) / total - target2);
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
    const float team1AsFloat = static_cast<float>(team1Count);
    const float totalAsFloat = static_cast<float>(team1Count + team2Count);
    const float team2AsFloat = static_cast<float>(team2Count);
    const bf2x87::Real fraction1 = static_cast<bf2x87::Real>(team1AsFloat) / totalAsFloat;
    const float fraction2 = static_cast<float>(static_cast<bf2x87::Real>(team2AsFloat) / totalAsFloat);
    const bf2x87::Real extendedWeightSum = static_cast<bf2x87::Real>(team1Weight) + team2Weight;
    const bf2x87::Real extendedError1 = fraction1 - team1Weight / extendedWeightSum;
    const bf2x87::Real extendedError2 = fraction2 - team2Weight / extendedWeightSum;
    const float weightSum = static_cast<float>(extendedWeightSum);
    const float error1 = static_cast<float>(extendedError1);
    const float error2 = static_cast<float>(extendedError2);
#else
    const float team1AsFloat = static_cast<float>(team1Count);
    const float totalAsFloat = static_cast<float>(team1Count + team2Count);
    const float team2AsFloat = static_cast<float>(team2Count);
    const long double fraction1 = static_cast<long double>(team1AsFloat) / totalAsFloat;
    const float fraction2 = static_cast<float>(static_cast<long double>(team2AsFloat) / totalAsFloat);
    const long double extendedWeightSum = static_cast<long double>(team1Weight) + team2Weight;
    const long double extendedError1 = fraction1 - team1Weight / extendedWeightSum;
    const long double extendedError2 = fraction2 - team2Weight / extendedWeightSum;
    const float weightSum = static_cast<float>(extendedWeightSum);
    const float error1 = static_cast<float>(extendedError1);
    const float error2 = static_cast<float>(extendedError2);
#endif
#endif

    std::string gameMode;
    if (game)
        gameMode = game->getCurrentMapGameMode();
    if (compareIgnoreCase(gameMode, std::string("gpm_coop")) == 0 && serverSettings) {
        const float desiredBotRatio = serverSettings->getCoopBotRatio();
#if defined(_WIN32)
        const float team1BotsAsFloat = static_cast<float>(team1BotCount);
        const int configuredBotCount = serverSettings->getCoopBotCount();
        const long double actualBotRatio = static_cast<long double>(team1BotsAsFloat) /
            configuredBotCount * 100.0L;
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
        const int configuredBotCount = serverSettings->getCoopBotCount();
        const float team1BotsAsFloat = static_cast<float>(team1BotCount);
        const float configuredBotsAsFloat = static_cast<float>(configuredBotCount);
        const bf2x87::Real actualBotRatio = static_cast<bf2x87::Real>(team1BotsAsFloat) /
            configuredBotsAsFloat * 100.0L;
#else
        const int configuredBotCount = serverSettings->getCoopBotCount();
        const float team1BotsAsFloat = static_cast<float>(team1BotCount);
        const float configuredBotsAsFloat = static_cast<float>(configuredBotCount);
        const long double actualBotRatio = static_cast<long double>(team1BotsAsFloat) /
            configuredBotsAsFloat * 100.0L;
#endif
#endif
        // Unordered selects team 1: PE test ah,1; ELF jnb takes only the team-2 arm.
        if (!(actualBotRatio >= desiredBotRatio))
            return 1;
        return 2;
    }

    if (error1 > error2)
        return 2;
    if (error2 > error1)
        return 1;

#if defined(_WIN32)
    const int firstTotal = team2Count + (team1Count + 1);
    const int secondTotal = (team2Count + 1) + team1Count;
    const long double placement1 = std::fabs(
        (static_cast<long double>(team1Count + 1) / firstTotal - target1) -
        (static_cast<long double>(team2AsFloat) / firstTotal - target2));
    const long double placement2 = std::fabs(
        (static_cast<long double>(team1Count) / secondTotal - target1) -
        (static_cast<long double>(team2Count + 1) / secondTotal - target2));
#else
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
    // [CHOSEN, not recovered] Retain x87-width values on both Mac architectures.
    const bf2x87::Real target1 = static_cast<bf2x87::Real>(team1Weight) / weightSum;
    const bf2x87::Real target2 = static_cast<bf2x87::Real>(team2Weight) / weightSum;
    const float firstTotal = static_cast<float>(team2Count + (team1Count + 1));
    const float secondTotal = static_cast<float>((team2Count + 1) + team1Count);
    const float placement1 = static_cast<float>(bf2x87::abs(
        (static_cast<bf2x87::Real>(static_cast<float>(team1Count + 1)) / firstTotal - target1) -
        (static_cast<bf2x87::Real>(team2AsFloat) / firstTotal - target2)));
    const float placement2 = static_cast<float>(bf2x87::abs(
        (static_cast<bf2x87::Real>(team1AsFloat) / secondTotal - target1) -
        (static_cast<bf2x87::Real>(static_cast<float>(team2Count + 1)) / secondTotal - target2)));
#else
    const long double target1 = static_cast<long double>(team1Weight) / weightSum;
    const long double target2 = static_cast<long double>(team2Weight) / weightSum;
    const float firstTotal = static_cast<float>(team2Count + (team1Count + 1));
    const float secondTotal = static_cast<float>((team2Count + 1) + team1Count);
    const float placement1 = static_cast<float>(std::fabs(
        (static_cast<long double>(static_cast<float>(team1Count + 1)) / firstTotal - target1) -
        (static_cast<long double>(team2AsFloat) / firstTotal - target2)));
    const float placement2 = static_cast<float>(std::fabs(
        (static_cast<long double>(team1AsFloat) / secondTotal - target1) -
        (static_cast<long double>(static_cast<float>(team2Count + 1)) / secondTotal - target2)));
#endif
#endif
    if (placement1 > placement2)
        return 2;
    if (placement2 > placement1)
        return 1;
    return (bf2crt::rand() & 1) + 1;
}

// Counts every registered player, including bots.
int PlayerManager::getNumberOfPlayers() const
{
    return static_cast<int>(m_players.size());
}

// Calls updateClientSettings before publishing a nonnull player, then stores the pointer
// unconditionally. The callback therefore sees the old local-human player.
void PlayerManager::setLocalHumanPlayer(IPlayer* player)
{
    if (player)
        player->updateClientSettings();

    m_localHumanPlayer = player;
}

// the class factory

// Creates a one-reference PlayerManager, rejecting aggregation and releasing on IID mismatch.
// Construction also initializes the persistent-explosion manager singleton. No init/addRef follows.
// The class factory belongs to ModuleBF2; world::Module publishes only its singleton row.
IBase* PlayerManager::create(unsigned int iid, IBase* outer)
{
    if (outer != 0)
        return 0;

    PlayerManager* self = new PlayerManager();          // 0x8572317, operator new(0x50u)
    void* itf = self->queryInterface(iid);              // 0x8572332, virtual, vtable +0x0C
    if (itf == 0)
        self->release();                                // 0x8572344, virtual, vtable +0x08
    return static_cast<IBase*>(itf);
}

// Player-class names are indexed by class ID in an ordered map.

// Registers a class name with one lookup; an existing ID is silently overwritten.
void PlayerManager::registerPlayerClass(unsigned int classId, const std::string& className)
{
    std::map<unsigned int, std::string>::iterator it = m_playerClasses.lower_bound(classId);

    if (it == m_playerClasses.end() || classId < it->first)
        it = m_playerClasses.insert(it, std::make_pair(classId, std::string()));

    it->second.assign(className);
}

// getPlayerClasses -

// A forward walk of the map, appending one pair per entry. Order is the map's, so ascending by
// class id.

// it appends; it does not clear. The insert position is the caller's own end() - read fresh from
// the list header each iteration - so a non-empty `out` keeps whatever it already held and the new
// entries follow it. Calling this twice with the same list doubles its contents. Recovered as-is.
void PlayerManager::getPlayerClasses(std::list<std::pair<unsigned int, std::string> >& out) const
{
    for (std::map<unsigned int, std::string>::const_iterator it = m_playerClasses.begin();
         it != m_playerClasses.end();
         ++it)
    {
        out.insert(out.end(), std::make_pair(it->first, it->second));
    }
}

// Returns the mapped class name by reference, or a shared function-local fallback.
// Erasing a matching entry invalidates the returned reference.
const std::string& PlayerManager::getClassNameFromPlayerClassID(unsigned int classId) const
{
    static const std::string unknownPlayerClass("UnknownPlayerClass");   // 0x8AB0978, guard 0x8AB0970

    std::map<unsigned int, std::string>::const_iterator it = m_playerClasses.find(classId);

    if (it == m_playerClasses.end())
        return unknownPlayerClass;

    return it->second;
}


// Updates concrete Player objects in list order through the nonvirtual update method.
// An empty-player run does not exercise player update behavior.
void PlayerManager::updatePlayers(float deltaTime)
{
    for (std::list<IPlayer*>::iterator it = m_players.begin(); it != m_players.end(); ++it)
        static_cast<Player*>(*it)->update(deltaTime);
}

// Counts connected non-AI players, regardless of life or spawn state.
// The round-start threshold depends on this behavior despite the method name.
int PlayerManager::getNumberOfAliveHumanPlayers() const
{
    int count = 0;
    for (std::list<IPlayer*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getIsConnected() && !player->getIsAIPlayer())                    // +0x78, +0x64
            ++count;
    }
    return count;
}

// PlayerManager::getNumberOfHumanPlayers() const - Vtable +0x8C. .
// Counts every registered NON-AI player. Unlike getNumberOfAliveHumanPlayers above, it does not
// test getIsConnected - it just walks the list and counts the ones getIsAIPlayer() denies.
// GameServer::handleClientInfo's reserved-slot gate is its caller.
int PlayerManager::getNumberOfHumanPlayers() const
{
    int count = 0;
    for (std::list<IPlayer*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (!player->getIsAIPlayer())                                                // +0x64
            ++count;
    }
    return count;
}

// PlayerManager::getPlayerFromProfileId(int) - Vtable +0x28. .
// Linear scan of the player list, returning the first whose getProfileId() matches, or null.
// GameServer::handleClientInfo uses it for the duplicate-profile kick. (Distinct from getPlayerFromId,
// which is an m_playersById MAP lookup - the profile id is not keyed, so this one iterates.)
IPlayer* PlayerManager::getPlayerFromProfileId(int profileId)
{
    for (std::list<IPlayer*>::const_iterator it = m_players.begin(); it != m_players.end(); ++it)
    {
        IPlayer* player = *it;
        if (player->getProfileId() == profileId)                                     // +0x2AC
            return player;
    }
    return 0;
}

} // namespace world
} // namespace hfe
} // namespace dice
