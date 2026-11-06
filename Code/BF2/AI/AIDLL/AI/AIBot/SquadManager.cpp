// [INFERRED] Source placement.
// [RETIRE-WHEN: aidll-02-skeleton] Complete and validate the remaining SquadManager operations.

#include "SquadManager.h"
#include "../AIKnowledge/IPIArmament.h"
#include "../AIKnowledge/WeaponInfo.h"
#include "../AIUtils/WeaponInfoManager.h"

// <iostream>'s ios_base::Init object is this TU's static initialisation and teardown (ELF
// 0x1B0FE0, __tcf_0 0x1B1040, _GLOBAL__I 0x1B1070).
#include <iostream>
#include "../../../../Platform/X87Compat.h" // [CHOSEN, not recovered]

#include "SquadCoordinator.h"                        // the constructor builds two per squad
#include "BotMain.h"
#include "BotSquad.h"
#include "SquadMember.h"
#include "../Main/AIDllFrontier.h"
#include "../AIKnowledge/Information.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../Main/IBotManager.h"
#include "../Main/AIMain.h"
#include "../Main/CommonSingletonServer.h"
#include "../AIStrategic/SAI.h"
#include "../../../AIInterface/BFKnowledge/BFGame.h" // IAIGame::getMaxNumberOfSquads
#include "../../../AIInterface/BFKnowledge/AIPlayerFactory.h"
#include "../../../AIInterface/BFKnowledge/AIPlayer.h"
#include "../../../AIInterface/BFKnowledge/BFSquad.h"
#include "../../../AIInterface/BFKnowledge/BFCommander.h"
#include "../../../AIInterface/BFKnowledge/BFTimer.h"
#include "../../../../Core/SettingsRepostitory.h"
#include "../../../../Core/StringUtil.h"
#include "../Main/InformationManager.h"
#include "../AIKnowledge/Information.h"
#include "../Main/AIRandom.h"
#include "../Main/IAIPathfinding.h"
#include "../AIKnowledge/IPIMobile.h"
#include "../AIKnowledge/IPIUnit.h"
#include "../AIStrategicEntity/AIStrategicArea.h"
#include "../../../AIInterface/BFKnowledge/IAIObject.h"
#include "../../../AIInterface/BFKnowledge/BFObjectEnvironment.h"
#include <cmath>

namespace dice {
namespace hfe {
namespace ai {

// Both materialize the full integer sum.
// Side is one-based. Preserve the fresh vector bound and indexed receiver at every call.
int SquadManager::getSquadStrengths(int side, int areaIndex, StrategicStrengthType type)
{
    int total = 0;
    for (std::size_t index = 0; index < m_Squads[side - 1].size(); ++index)
    {
        if (m_Squads[side - 1][index]->getMissionStatus() == 2 &&
            m_Squads[side - 1][index]->isSquadAssignedToArea(areaIndex))
        {
            total += m_Squads[side - 1][index]->getStrategicStrength(type);
        }
    }
    return total;
}

SquadManager* SquadManager::m_instance = 0;

// One coordinator per call, alternating
// sides. Re-read both cursors and side-zero's size after the coordinator callback; no empty
// index or null guard exists. The size-minus-one comparison is unsigned, including underflow.
void SquadManager::update(double time)
{
    updateArtilleryStatus(time);
    updateCommanderMessages();
    m_sideCursor = m_sideCursor == 0;
    m_Squads[m_sideCursor][m_squadCursor]->update(time);
    if (m_sideCursor == 1)
    {
        if (static_cast<unsigned int>(m_squadCursor) < m_Squads[0].size() - 1)
            m_squadCursor = static_cast<int>(static_cast<unsigned int>(m_squadCursor) + 1);
        else
        {
            m_squadCursor = 0;
            updateHumanPlayers(time);
        }
    }
}

// Strict ordered expiry, then clear the flag before any
// path callback. Each expiry visits both sides, using live vector bounds/entries. Side one's
// flag, timestamp and duration are read after side zero's callbacks; simultaneous expiry
// therefore runs two complete traversals.
void SquadManager::updateArtilleryStatus(double time)
{
    SideBlock* blocks[2] = { &m_artillery[0], &m_artillery[1] };
    for (unsigned int block = 0; block < 2; ++block)
    {
        if (blocks[block]->incomingArtillery && time > m_double156 + blocks[block]->m_artilleryStartTime)
        {
            blocks[block]->incomingArtillery = false;
            for (unsigned int side = 0; side < 2; ++side)
                for (unsigned int index = 0; index < m_Squads[side].size(); ++index)
                    m_Squads[side][index]->updateSquadPaths();
        }
    }
}

// Purge messages for each human commander before reading
// game time once. Dispatch only types 0/1, but delete every expired type. The queued pointer
// is fetched again after the send callback for deletion, and the list end remains live.
void SquadManager::updateCommanderMessages()
{
    for (int side = 0; side < 2; ++side)
    {
        if (IAIMain::m_instance->getSAI(side + 1)->isPlayerCommander())
        {
            for (std::list<CommanderMessage*>::iterator i = m_commanderMessages.begin();
                 i != m_commanderMessages.end(); )
            {
                if ((*i)->m_sideIndex == side)
                {
                    delete *i;
                    m_commanderMessages.erase(i++);
                }
                else
                    ++i;
            }
        }
    }
    const double now = static_cast<IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    for (std::list<CommanderMessage*>::iterator i = m_commanderMessages.begin();
         i != m_commanderMessages.end(); )
    {
        if (now > (*i)->m_dueTime)
        {
            switch (static_cast<int>((*i)->m_type))
            {
            case 0:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)
                    ->sendAttackMessage((*i)->m_sideIndex + 1, (*i)->m_squadIndex + 1);
                break;
            case 1:
                static_cast<IBFCommander*>(CommonSingletonServer::m_iBFCommander)
                    ->sendDefendMessage((*i)->m_sideIndex + 1, (*i)->m_squadIndex + 1);
                break;
            }
            delete *i;
            m_commanderMessages.erase(i++);
        }
        else
            ++i;
    }
}

// Keep the first invalid timestamp; repeated invalid
// reports do not refresh it. A valid report erases the entry, if any. One getId call.
void SquadManager::setPlayerOnValidPosition(IAIPlayer* player, bool valid, double time)
{
    const int id = player->getId();
    std::map<int, double>::iterator i = m_playerValidPositions.find(id);
    if (valid)
    {
        if (i != m_playerValidPositions.end())
            m_playerValidPositions.erase(i);
    }
    else if (i == m_playerValidPositions.end())
        m_playerValidPositions.insert(std::make_pair(id, time));
}

// ELF 0x1AE5B0 (listing). Whether a human (not getIsAIPlayer, +0x88) of `side` (getSide, +0x6C)
// is among the factory's players (IAIPlayerFactory +0x2C, copied); null entries are skipped.
// The manager itself is not read.
bool SquadManager::haveHumanPlayerOnOwnSide(int side)
{
    const std::vector<IAIPlayer*> players =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getAllPlayers();
    for (std::vector<IAIPlayer*>::const_iterator i = players.begin(); i != players.end(); ++i)
    {
        IAIPlayer* const player = *i;
        if (player && !player->getIsAIPlayer() && player->getSide() == side)
            return true;
    }
    return false;
}

// Copy the borrowed factory
// vector before callbacks. Preserve the repeated AI-player query even when the first one
// returned true. A live, non-downed human with a vehicle next needs IAIObject::getInformation
// (ELF +0xB8 / PE +0xB4); its full interface remains unknown. Stop at that caller boundary.
void SquadManager::updateHumanPlayers(double time)
{
    const std::vector<IAIPlayer*> players =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)->getAllPlayers();
    for (std::vector<IAIPlayer*>::const_iterator i = players.begin(); i != players.end(); ++i)
    {
        IAIPlayer* player = *i;
        if (!player)
            continue;
        // ELF 0x1AE1D0: a human in an object with mobile information is on a valid position
        // unless pathfinding rejects it (IAIPathfinding +0x2C, vehicle number, equipment type).
        bool valid = true;
        IAIObject* vehicle = 0;
        if (!player->getIsAIPlayer() && player->getIsAlive() && !player->isManDown()
            && (vehicle = player->getCurrentVehicle()) != 0)
        {
            Information* const info = informationManager->getInformation(vehicle->getInformation(player->getSide()));
            if (info)
            {
                const Vec3 position = info->getPosition();
                if (IPIMobile* const mobile = info->getMobile())
                {
                    const int type = info->getUnit()->getEquipmentType();
                    if (!IAIPathfinding::instance->isValidPosition(mobile->getVehicleNumber(), type, position, false))
                        valid = false;
                }
            }
        }
        if (!player->getIsAIPlayer())
            setPlayerOnValidPosition(player, valid, time);
    }
    for (std::map<int, double>::iterator i = m_playerValidPositions.begin();
         i != m_playerValidPositions.end(); )
    {
        IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
            ->getPlayerFromId(i->first);
        if (!player || player->getIsAIPlayer() || !player->getIsAlive() || player->isManDown())
            m_playerValidPositions.erase(i++);
        else
            ++i;
    }
}

// Creates a coordinator for each squad on each side, excluding squad zero.
SquadManager::SquadManager()
    : m_Squads()
    // Default-initialize SideBlock: its three Vec3 values alone are zeroed. Value-initializing
    // the blocks would also zero timestamps/flags retail leaves untouched (full //PE reads).
    , m_commanderMessages()
    , m_playerValidPositions()
{
    const int squads =
        static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)->getMaxNumberOfSquads();

    if (squads != 1)
    {
        const unsigned int last = static_cast<unsigned int>(squads) - 1;

        for (unsigned int i = 0; i < last; ++i)
        {
            m_Squads[0].push_back(new SquadCoordinator(0, static_cast<int>(i)));
            m_Squads[1].push_back(new SquadCoordinator(1, static_cast<int>(i)));
        }
    }

    if (!m_instance)
        m_instance = this;

    m_squadCursor = 0;
    m_float152 = 40.0f;
    m_sideCursor = 1;

    m_artillery[0].incomingArtillery = false;
    m_artillery[1].incomingArtillery = false;

    m_double156 = 25.0;

    clearCommanderMessages();
    clearPlayerValidPositions();
}

// Deletes each owned commander message before erasing its list node.
// Advances the iterator before erasure.
void SquadManager::clearCommanderMessages()
{
    for (std::list<CommanderMessage*>::iterator i = m_commanderMessages.begin();
         i != m_commanderMessages.end(); )
    {
        delete *i;
        m_commanderMessages.erase(i++);
    }
}

// Erases position records individually, preserving per-node tree rebalancing.
void SquadManager::clearPlayerValidPositions()
{
    for (std::map<int, double>::iterator i = m_playerValidPositions.begin();
         i != m_playerValidPositions.end(); )
    {
        m_playerValidPositions.erase(i++);
    }
}

// complete instructions read on both ports. Prefer a nonempty
// least-populated coordinator, with later entries winning ties. Keep the repeated size reads.
// There is no side-range, empty-roster or player-null guard in either image.
void SquadManager::addSquadMember(int playerId, int side)
{
    --side;
    IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
        ->getPlayerFromId(playerId);
    SquadCoordinator* selected = 0;
    const int maximum = static_cast<IAIGame*>(CommonSingletonServer::m_iaiGame)
        ->getMaxNumberOfSquadMembers();
    int best = maximum;
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        if (coordinators[i]->getNumberOfSquadMembers() <= best &&
            coordinators[i]->getNumberOfSquadMembers() > 0)
        {
            best = coordinators[i]->getNumberOfSquadMembers();
            selected = coordinators[i];
        }
    }
    if (!selected)
        selected = coordinators[0];

    if (static_cast<unsigned int>(selected->getSquadId()) != coordinators.size() - 1 &&
        coordinators[selected->getSquadId() + 1]->getNumberOfSquadMembers() == 0)
    {
        int splitAt = 3;
        const std::string mode = settings->stringGet("GSGameMode", "gpm_cq");
        if (compareIgnoreCase(mode, std::string("gpm_coop")) == 0)
            splitAt = maximum - 1;
        best = selected->getNumberOfSquadMembers();
        if (best >= splitAt)
        {
            // ELF 0x1AF21A: the last member moves to the next (empty) squad: it is unlinked,
            // the old squad re-elects its leader, the engine squad is left (IBFSquad +0x10
            // exitSquad), the member's player joins the next coordinator - a bot through its
            // BotSquad, a human through IBFSquad +0x0C joinSquad - and the record is deleted.
            // The new player then joins that next squad too.
            SquadMember* const moved = selected->getSquadMember(best - 1);
            selected->removeSquadMember(best - 1);
            selected->updateSquadLeader();
            IBFSquad* const squads = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
            squads->exitSquad(selected->getSquadId() + 1, moved->getPlayer());
            selected = coordinators[selected->getSquadId() + 1];
            selected->addSquadMember(moved->getPlayer());
            if (moved->isBot())
                IBotManager::instance->getBotFromPlayerId(moved->getPlayerId())->getBotSquad()
                    ->joinSquad(selected->getSquadId() + 1);
            else
                squads->joinSquad(selected->getSquadId() + 1, moved->getPlayer());
            selected->updateSquadLeader();
            delete moved;
        }
    }

    selected->addSquadMember(player);
    Bot* bot = IBotManager::instance->getBotFromPlayerId(playerId);
    if (bot)
    {
        const int squad = selected->getSquadId() + 1;
        bot->getBotSquad()->joinSquad(squad);
    }
    else
    {
        static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)
            ->joinSquad(selected->getSquadId() + 1, player);
    }
    // Both shipped callers discard the bridge's Boolean; a refusal still reaches this update.
    selected->updateSquadLeader();
}

// Both identifiers are one-based. Retail has no range
// singleton or coordinator-null check here; the courier owns only the singleton guard.
void SquadManager::newSquadLeader(int side, int squadId)
{
    m_Squads[side - 1][squadId - 1]->updateSquadLeader();
}

// Search this side in coordinator order and return the first
// matching member. The coordinator's output index is intentionally not used by this caller.
SquadMember* SquadManager::getSquadMember(int playerId, int side)
{
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        int memberIndex;
        SquadMember* member = coordinators[i]->getSquadMember(playerId, memberIndex);
        if (member)
            return member;
    }
    return 0;
}

// The side is one-based and unchecked. Keep the live
// unsigned-index scan and reload the coordinator entry after a successful member query.
// The member index output is not consumed, and null coordinator entries are not guarded.
SquadCoordinator* SquadManager::getSquadFromPlayer(int playerId, int side)
{
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        int memberIndex;
        if (coordinators[i]->getSquadMember(playerId, memberIndex))
            return coordinators[i];
    }
    return 0;
}

// ELF 0x1AF6A0: the player's squad on side 1, else on side 2; returns the side searched last.
int SquadManager::getSquadFromPlayer(int playerId, SquadCoordinator*& squad)
{
    squad = getSquadFromPlayer(playerId, 1);
    if (squad)
        return 1;
    squad = getSquadFromPlayer(playerId, 2);
    return 2;
}

// ELF 0x1AF720: one-based squad id and side, no range checks.
SquadCoordinator* SquadManager::getSquadFromId(int squadId, int side)
{
    return m_Squads[side - 1][squadId - 1];
}

// ELF 0x1AF860 (256 bytes). An order received by the player's squad: the order position comes
// from the engine (IBFSquad +0x50 getCommanderOrder for a commander order, +0x54 getSquadOrder
// for the squad leader's), then a repair order (type 3) becomes a repair request and anything
// else a mission request without a target object.
void SquadManager::setSquadReceivedOrder(BotSquad::OrderType type, int playerId, bool commander)
{
    SquadCoordinator* squad;
    const int side = getSquadFromPlayer(playerId, squad);
    if (!squad)
        return;
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    IBFSquad* const engine = static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad);
    if (commander)
        engine->getCommanderOrder(side, squad->getSquadId() + 1, position, 0);
    else
        engine->getSquadOrder(side, squad->getSquadId() + 1, position, 0);
    if (type == 3)
        squad->requestRepair(position);
    else
        squad->requestMission(type, position, 0, commander);
}

// ELF 0x1AF960 (320 bytes). Without a player commander, a squad with a mission state whose request
// came from the (AI) commander gets that order again: attack (type 0) or defend, through the side's
// CommanderInterface, and a commander message (0 attack / 1 defend) due in 2 s.
void SquadManager::repeatOrder(int side, int squadId)
{
    ISAI* const sai = IAIMain::m_instance->getSAI(side);
    SquadCoordinator* const squad = getSquadFromId(squadId, side);
    if (!squad || sai->isPlayerCommander())
        return;
    if (squad->getMissionStatus() == 0)
        return;
    const SquadCoordinator::MissionRequest request = squad->getMissionRequest();
    if (!request.m_commander)
        return;
    if (request.m_type == 0)
    {
        sai->getCommanderInterface()->giveAttackOrder(request.m_position, squad);
        enqueueCommanderMessage(side - 1, squadId - 1, static_cast<CommanderMessageType>(0), 2.0f);
    }
    else
    {
        sai->getCommanderInterface()->giveDefendOrder(request.m_position, squad);
        enqueueCommanderMessage(side - 1, squadId - 1, static_cast<CommanderMessageType>(1), 2.0f);
    }
}

// ELF 0x1AE970 (276 bytes). A message for the same (side index, squad index) already queued
// keeps its place; otherwise a new one (zeroed, 20 bytes) goes to the back.
void SquadManager::enqueueCommanderMessage(int sideIndex, int squadIndex, CommanderMessageType type, double delay)
{
    for (std::list<CommanderMessage*>::iterator it = m_commanderMessages.begin(); it != m_commanderMessages.end(); ++it)
    {
        if ((*it)->m_sideIndex == sideIndex && (*it)->m_squadIndex == squadIndex)
            return;
    }
    CommanderMessage* const message = new CommanderMessage();
    message->m_dueTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime() + delay;
    message->m_type = type;
    message->m_sideIndex = sideIndex;
    message->m_squadIndex = squadIndex;
    m_commanderMessages.push_back(message);
}

// Squad zero is ignored. An existing member anywhere on the
// side also returns immediately: a bot's engine join notification must not insert it twice.
// On a miss, retain the unchecked coordinator/player lookups, add, then update the leader.
void SquadManager::playerJoinsSquad(int playerId, int squadId, int side)
{
    if (squadId == 0)
        return;
    if (getSquadMember(playerId, side))
        return;

    SquadCoordinator* coordinator = m_Squads[side - 1][squadId - 1];
    IAIPlayer* player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
        ->getPlayerFromId(playerId);
    coordinator->addSquadMember(player);
    coordinator->updateSquadLeader();
}

// ELF 0x1AF800: every coordinator of the one-based side drops its strategic-area assignment.
void SquadManager::unAssignSquads(int side)
{
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
        coordinators[i]->unAssignSquad();
}

// ELF 0x1AE100: both sides unassigned, queued commander messages and player positions cleared,
// the artillery flags of both side blocks and the update cursors reset, then every coordinator
// of side 0 and then side 1 resets.
// ELF 0x1ADD70 (D1, 467 B; recovered on the client track, docs/macos-c-singleplayer.md). Each
// side's coordinators are deleted (nonvirtual SquadCoordinator D1 + operator delete) and the
// vector emptied, then the commander messages and valid positions are cleared and the singleton
// pointer zeroed unconditionally. The members go afterwards in reverse declaration order.
SquadManager::~SquadManager()
{
    for (int side = 0; side < 2; ++side)
    {
        for (unsigned int i = 0; i < m_Squads[side].size(); ++i)
            delete m_Squads[side][i];
        m_Squads[side].erase(m_Squads[side].begin(), m_Squads[side].end());
    }
    clearCommanderMessages();
    clearPlayerValidPositions();
    m_instance = 0;
}

void SquadManager::reset()
{
    unAssignSquads(1);
    unAssignSquads(2);
    clearCommanderMessages();
    clearPlayerValidPositions();
    m_artillery[0].incomingArtillery = false;
    m_artillery[1].incomingArtillery = false;
    m_squadCursor = 0;
    m_sideCursor = 1;
    for (int side = 0; side < 2; ++side)
        for (unsigned int i = 0; i < m_Squads[side].size(); ++i)
            m_Squads[side][i]->reset();
}

// ELF 0x1B0180: squad 0 (no squad) is ignored. Otherwise the player's member record is unlinked
// from coordinator [side - 1][squadId - 1], which re-elects its leader. The record itself is not
// deleted on this path, as shipped.
void SquadManager::playerRemovedFromSquad(int playerId, int squadId, int side)
{
    if (!squadId)
        return;
    SquadMember* const member = getSquadMember(playerId, side);
    if (!member)
        return;
    SquadCoordinator* const coordinator = m_Squads[side - 1][squadId - 1];
    coordinator->removeSquadMember(member);
    coordinator->updateSquadLeader();
}

// ELF 0x1AF750: the side's squads assigned to the area are released, except those already close to
// their non-owned target; answers how many were released.
int SquadManager::unAssignSquads(int areaIndex, int side)
{
    int released = 0;
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        if (coordinators[i]->isSquadCloseToNonOwnedTarget())
            continue;
        if (coordinators[i]->isSquadAssignedToArea(areaIndex))
        {
            coordinators[i]->unAssignSquad();
            ++released;
        }
    }
    return released;
}

// ELF 0x1B0610 (466 bytes). An enemy human whose valid position (m_playerValidPositions) is
// more than 60 s old and who sits in an object with information for its side is camping; its
// position's x and z are returned.
bool SquadManager::haveCampingHumans(int side, Vec2& position)
{
    const int enemy = side == 1 ? 2 : 1;
    const double now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    IAIPlayerFactory* const factory = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
    for (std::map<int, double>::iterator it = m_playerValidPositions.begin();
         it != m_playerValidPositions.end(); ++it)
    {
        IAIPlayer* const player = factory->getPlayerFromId(it->first);
        if (!player || player->getSide() != enemy || !(it->second + 60.0 < now))
            continue;
        IAIObject* const vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* const info = informationManager->getInformation(vehicle->getInformation(player->getSide()));
        if (info)
        {
            const Vec3& p = info->getPosition();
            position.x = p.x;
            position.y = p.z;
            return true;
        }
    }
    return false;
}

// ELF 0x1AFD30. The nearest available squad with members whose strategic strength (type
// !attack) is positive; a squad requested to this very area counts only while its leader is a
// bot. strength receives twice the chosen squad's strength (0 if none).
SquadCoordinator* SquadManager::getClosestAvailableSquad(AIStrategicArea* area, const Vec3& position,
                                                         int side, bool attack, int& strength)
{
    SquadCoordinator* closest = 0;
    float best = 3.4028235e+38f;
    Vec3 squadPosition = { 0.0f, 0.0f, 0.0f };
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    strength = 0;
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        SquadCoordinator* const squad = coordinators[i];
        if (!squad->isSquadAvailable() || squad->getNumberOfSquadMembers() <= 0)
            continue;
        if (squad->isSquadRequestedToArea(area->getIndex()) && !squad->getSquadLeader()->isBot())
            continue;
        const int value = squad->getStrategicStrength(static_cast<StrategicStrengthType>(!attack)) * 2;
        if (value <= 0)
            continue;
        squad->getSquadPosition(squadPosition);
        const float dx = position.x - squadPosition.x;
        const float dy = position.y - squadPosition.y;
        const float dz = position.z - squadPosition.z;
        const float distance = dx * dx + dy * dy + dz * dz;
        if (distance < best)
        {
            strength = value;
            closest = squad;
            best = distance;
        }
    }
    return closest;
}

// ELF 0x1AFAA0: distance to the nearest squad leader with a valid position (FLT_MAX's root
// when there is none).
float SquadManager::getDistanceToClosestSquad(const Vec3& position, int side)
{
    float best = 3.4028235e+38f;
    Vec3 leader = { 0.0f, 0.0f, 0.0f };
    const std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        SquadCoordinator* const squad = coordinators[i];
        if (squad->getNumberOfSquadMembers() <= 0)
            continue;
        if (squad->getSquadLeader()->getPosition(leader))
        {
            const float dx = position.x - leader.x;
            const float dy = position.y - leader.y;
            const float dz = position.z - leader.z;
            const float distance = dx * dx + dy * dy + dz * dz;
            if (distance < best)
                best = distance;
        }
    }
    return std::sqrt(best);
}

// ELF 0x1AEE60: the float at +0x98 (+152, m_float152, 40.0f at construction).
float SquadManager::getArtillerySafeDistance()
{
    return m_float152;
}

// ELF 0x1B07F0 (427 bytes): is any player of the side within radius in an object that is a
// soldier (type flag 0x20) or not vehicle-flagged (0x10)?
bool SquadManager::friendliesCloseTo(Vec3 position, float radius, int side)
{
    std::vector<int> ids;
    bool found = false;
    static_cast<const IAIObjectEnvironment*>(CommonSingletonServer::m_iaiObjectEnvironment)->getWorldPlayerIds(ids);
    IAIPlayerFactory* const factory = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);
    for (std::vector<int>::iterator it = ids.begin(); it != ids.end(); ++it)
    {
        IAIPlayer* const player = factory->getPlayerFromId(*it);
        if (!player || player->getSide() != side)
            continue;
        IAIObject* const vehicle = player->getCurrentVehicle();
        if (!vehicle)
            continue;
        Information* const info = informationManager->getInformation(vehicle->getInformation(side));
        if (!info)
            continue;
        if (!info->isOfType(ITHelicopter) && info->isOfType(ITAir))
            continue;
        const Vec3& p = info->getPosition();
        const float dx = p.x - position.x;
        const float dy = p.y - position.y;
        const float dz = p.z - position.z;
        if (dx * dx + dy * dy + dz * dz < radius * radius)
        {
            found = true;
            break;
        }
    }
    return found;
}

// ELF 0x1B09A0 (339 bytes). Marks the side's artillery warning with the time and impact point
// and a random delay in [d/2, d) for d = m_double156; both sides' flee positions become invalid.
// The sum is x87 extended in the ELF (fildll, fmuls, fmul, faddp); bf2x87::Real keeps that.
void SquadManager::incomingArtillery(Vec3 position, int side)
{
    SideBlock& block = side - 1 == 0 ? m_artillery[0] : m_artillery[1];
    SideBlock& other = side - 1 == 0 ? m_artillery[1] : m_artillery[0];
    const double half = 0.5 * m_double156;
    block.incomingArtillery = true;
    block.m_artilleryStartTime = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    block.m_artilleryPosition = position;
    const unsigned int r = AIRandom::m_instance->next();
    block.m_validFlee0 = false;
    block.m_validFlee1 = false;
    const bf2x87::Real halfX = bf2x87::Real(half);
    const bf2x87::Real delay = halfX + (bf2x87::Real(static_cast<double>(r)) * bf2x87::Real(2.3283064e-10f)) * halfX;
    block.m_artilleryDelay = static_cast<double>(delay);
    other.m_validFlee0 = false;
    other.m_validFlee1 = false;
}

// ELF 0x1B0B00 (574 bytes). Inside the safe distance (+152) of the side's own pending strike,
// or of the other side's strike once its delay has passed: sinceStart = now - start and
// artillerySide = the striking side. Otherwise -1.0 and 0.
bool SquadManager::isInArtilleryDangerZone(Vec3 position, int side, double& sinceStart, int& artillerySide)
{
    SideBlock& own = side - 1 == 0 ? m_artillery[0] : m_artillery[1];
    SideBlock& other = side - 1 == 0 ? m_artillery[1] : m_artillery[0];
    const int otherSide = side - 1 == 0 ? 2 : 1;
    bool danger = false;
    sinceStart = -1.0;
    artillerySide = 0;
    const bf2x87::Real now = static_cast<const IBFTimer*>(CommonSingletonServer::m_iBFTimer)->getGameTime();
    const float safe = m_float152 * m_float152;
    const auto distanceSq = [&position](const SideBlock& block) {
        const float dx = block.m_artilleryPosition.x - position.x;
        const float dy = block.m_artilleryPosition.y - position.y;
        const float dz = block.m_artilleryPosition.z - position.z;
        return dx * dx + dy * dy + dz * dz;
    };
    if (own.incomingArtillery && distanceSq(own) < safe)
    {
        danger = true;
        sinceStart = static_cast<double>(now - bf2x87::Real(own.m_artilleryStartTime));
        artillerySide = side;
    }
    else if (other.incomingArtillery && distanceSq(other) < safe
             && bf2x87::Real(other.m_artilleryStartTime) + bf2x87::Real(other.m_artilleryDelay) < now)
    {
        danger = true;
        sinceStart = static_cast<double>(now - bf2x87::Real(other.m_artilleryStartTime));
        artillerySide = otherSide;
    }
    return danger;
}

// ELF 0x1AFF20: the first coordinator of the side that holds the player drops it, tells the
// engine squad (IBFSquad::exitSquad, squad id + 1) and re-elects its leader.
void SquadManager::removeSquadMember(int playerId, int side)
{
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        SquadCoordinator* const squad = coordinators[i];
        int index = 0;
        SquadMember* const member = squad->getSquadMember(playerId, index);
        if (!member)
            continue;
        squad->removeSquadMember(index);
        IAIPlayer* const player = member->getPlayer();
        static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)->exitSquad(squad->getSquadId() + 1, player);
        squad->updateSquadLeader();
        return;
    }
}

// ELF 0x1AE000: both sides' squads are emptied, then every active bot is added again.
void SquadManager::removeAllSquads()
{
    for (unsigned int i = 0; i < m_Squads[0].size(); ++i)
        m_Squads[0][i]->removeAll();
    for (unsigned int i = 0; i < m_Squads[1].size(); ++i)
        m_Squads[1][i]->removeAll();
    IBotManager* const manager = IBotManager::instance;
    if (!manager)
        return;
    const unsigned int count = manager->getBotCount();
    for (unsigned int i = 0; i < count; ++i)
    {
        Bot* const bot = manager->getBot(i);
        if (!bot->isDeactivated())
        {
            const int side = bot->getSide();
            addSquadMember(bot->getPlayerId(), side);
        }
    }
}

// ELF 0x1B0010: removeSquadMember's search, but the coordinator destroys the member record.
void SquadManager::destroySquadMember(int playerId, int side)
{
    std::vector<SquadCoordinator*>& coordinators = m_Squads[side - 1];
    for (unsigned int i = 0; i < coordinators.size(); ++i)
    {
        SquadCoordinator* const squad = coordinators[i];
        int index = 0;
        SquadMember* const member = squad->getSquadMember(playerId, index);
        if (!member)
            continue;
        IAIPlayer* const player = member->getPlayer();
        squad->destroySquadMember(index);
        static_cast<IBFSquad*>(CommonSingletonServer::m_iBFSquad)->exitSquad(squad->getSquadId() + 1, player);
        squad->updateSquadLeader();
        return;
    }
}

// ELF 0x1AF600: the object's occupying player selects the squad.
SquadCoordinator* SquadManager::getSquadFromPlayer(unsigned int handle, int side)
{
    Information* const information = informationManager->getInformation(handle);
    if (!information)
        return 0;
    IPIUnit* const unit = information->getUnit();
    if (!unit)
        return 0;
    const int playerId = unit->getOccupyingPlayerId(side);
    return playerId == -1 ? 0 : getSquadFromPlayer(playerId, side);
}

// ELF 0x1B0210: both lookups run even when the first player has no squad.
bool SquadManager::isInSameSquad(int firstPlayer, int secondPlayer, int side)
{
    SquadCoordinator* const first = getSquadFromPlayer(firstPlayer, side);
    SquadCoordinator* const second = getSquadFromPlayer(secondPlayer, side);
    if (!first || !second)
        return false;
    const int firstId = first->getSquadId();
    return firstId == second->getSquadId();
}

// ELF 0x1B02A0.
bool SquadManager::isInAnySquad(int playerId, int side)
{
    return getSquadFromPlayer(playerId, side) != 0;
}

// ELF 0x1B0510: only a type-zero active weapon publishes a firing direction.
void SquadManager::shotsFired(int playerId, int side, const Vec3& direction)
{
    IAIPlayer* const player = static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory)
        ->getPlayerFromId(playerId);
    IAIObject* const vehicle = player->getCurrentVehicle();
    InformationManager* const manager = informationManager;
    Information* const information = manager->getInformation(vehicle->getInformation(side));
    // Shipped defect: information is dereferenced without checking the pool lookup.
    IPIArmament* const armament = information->getArmament();
    if (!armament)
        return;
    WeaponInfo* const weapon = WeaponInfoManager::m_instance->getInformation(
        armament->getWeapon(armament->getActiveWeapon()));
    if (weapon && weapon->getWeaponType() == 0)
    {
        SquadCoordinator* const squad = getSquadFromPlayer(playerId, side);
        if (squad)
            squad->shotsFired(playerId, direction);
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice

namespace dice {
namespace hfe {
namespace ai {

// ELF 0x1B04B0, added by ai-w1-e-fire (createPlanInternal 0x2672BF reports every new fire plan):
// forwards to the shooter's squad, when he has one.
void SquadManager::shotsFired(int playerId, int side, unsigned int target)
{
    if (SquadCoordinator* const squad = getSquadFromPlayer(playerId, side))
        squad->shotsFired(playerId, target);
}

// ELF 0x1AFBC0: over both sides' squads (stride 12), the positioned leader of a non-empty squad
// with the smallest squared distance (strictly smaller than FLT_MAX, then than the best so far).
SquadMember* SquadManager::getSquadLeaderClosestTo(const Vec3& position)
{
    float best = 3.4028235e+38f;
    SquadMember* closest = 0;
    Vec3 leaderPosition = {};
    for (unsigned int side = 0; side < 2; ++side)
    {
        std::vector<SquadCoordinator*>& squads = m_Squads[side];
        for (unsigned int i = 0; i < squads.size(); ++i)
        {
            if (squads[i]->getNumberOfSquadMembers() <= 0)
                continue;
            SquadMember* const leader = squads[i]->getSquadLeader();
            if (!leader->getPosition(leaderPosition))
                continue;
            const float dx = position.x - leaderPosition.x;
            const float dy = position.y - leaderPosition.y;
            const float dz = position.z - leaderPosition.z;
            const float distanceSq = dx * dx + dy * dy + dz * dz;
            if (distanceSq < best)
            {
                best = distanceSq;
                closest = leader;
            }
        }
    }
    return closest;
}

// ELF 0x1AF480.
SquadMember* SquadManager::getSquadMember(unsigned int handle, int side)
{
    Information* const info = informationManager->getInformation(handle);
    if (info && info->getUnit())
    {
        const int playerId = info->getUnit()->getOccupyingPlayerId(side);
        if (playerId != -1)
            return getSquadMember(playerId, side);
    }
    return 0;
}

// ELF 0x1B0440.
bool SquadManager::isAnySquadMemberRespondingToRequest(unsigned int handle, int action, int side)
{
    const std::vector<SquadCoordinator*>& squads = m_Squads[side - 1];
    for (unsigned int i = 0; i < squads.size(); ++i)
    {
        if (squads[i]->isAnySquadMemberRespondingToRequest(handle, action))
            return true;
    }
    return false;
}

// ELF 0x1B0390.
float SquadManager::requestResponseModifier(SquadMember* member, unsigned int handle, int action, int side)
{
    float modifier = 0.0f;
    SquadCoordinator* const squad = getSquadFromPlayer(member->getPlayerId(), side);
    if (squad)
    {
        if (member->isSameRequest(handle, action))
            modifier = 1.0f;
        else
            modifier = squad->requestResponseModifier(member, handle, action);
    }
    return modifier;
}

// ELF 0x1B0D40.
Vec3 SquadManager::getArtilleryTargetPos(int side)
{
    SideBlock* const blocks[2] = { &m_artillery[0], &m_artillery[1] };
    return blocks[side - 1]->m_artilleryPosition;
}

namespace {

// The member's current vehicle's information for its side, with a unit; null otherwise.
Information* fleeInformation(SquadMember* member, int side)
{
    IAIObject* const vehicle = member->getPlayer()->getCurrentVehicle();
    if (!vehicle)
        return 0;
    Information* const info = informationManager->getInformation(vehicle->getInformation(side));
    if (!info || !info->getUnit())
        return 0;
    return info;
}

} // namespace

// ELF 0x1B0D80.
bool SquadManager::haveValidArtilleryFleePos(SquadMember* member, Vec3& position)
{
    const int side = member->getSide();
    SideBlock* const blocks[2] = { &m_artillery[0], &m_artillery[1] };
    SideBlock* const block = blocks[side - 1];
    Information* const info = fleeInformation(member, side);
    if (!info)
        return false;
    if (!info->isOfType(ITSoldier))
    {
        if (!block->m_validFlee1)
            return false;
        position = block->m_fleePos1;
    }
    else
    {
        if (!block->m_validFlee0)
            return false;
        position = block->m_fleePos0;
    }
    return true;
}

// ELF 0x1B0EC0.
void SquadManager::setValidArtilleryFleePos(SquadMember* member, Vec3 position)
{
    const int side = member->getSide();
    SideBlock* const blocks[2] = { &m_artillery[0], &m_artillery[1] };
    SideBlock* const block = blocks[side - 1];
    Information* const info = fleeInformation(member, side);
    if (!info)
        return;
    if (!info->isOfType(ITSoldier))
    {
        block->m_validFlee1 = true;
        block->m_fleePos1 = position;
    }
    else
    {
        block->m_validFlee0 = true;
        block->m_fleePos0 = position;
    }
}

} // namespace ai
} // namespace hfe
} // namespace dice
