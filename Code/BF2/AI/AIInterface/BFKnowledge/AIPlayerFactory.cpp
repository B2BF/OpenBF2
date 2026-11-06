// [INFERRED] Source placement.

#include "AIPlayerFactory.h"
#include <algorithm>

#include "AIPlayer.h"                                 // the concrete wrapper this creates
#include "IAIObject.h"                                // isOccupiedObject
#include "IAIObjectPlugIn.h"                          // IAIObjectUnit::isOccupied
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../../Game/GameLogic/Player.h"           // world::IPlayer::getId

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../../Core/ClassManager.h"
#include "../../../Scene/Object/PlayerManager.h"   // the player lookups
#include "../SingletonServer.h"   // BFSingletonServer::updateStaticMethods

namespace dice {
namespace hfe {

extern ClassManager* classManager;

namespace ai {

// bf2 0x842B700. The primary object's unit is tested first (null unit: false); the secondary
// objects' units are not null-checked (`call *0x48(%eax)` straight after getUnit), as in the image.
bool isOccupiedObject(IAIObject* object, IAIObject* exclude)
{
    IAIObject* const primary = object->getPrimaryObject();          // +0xFC
    IAIObjectUnit* const unit = primary->getUnit();                 // +0x98
    if (!unit)
        return false;
    if (exclude != primary && unit->isOccupied())                   // IAIObjectUnit +0x48
        return true;
    int iterator;
    for (IAIObject* secondary = primary->getFirstSecondaryObject(iterator);   // +0x11C
         secondary;
         secondary = primary->getSecondaryObject(iterator))                   // +0x114
    {
        IAIObjectUnit* const secondaryUnit = secondary->getUnit();
        if (exclude != secondary && secondaryUnit->isOccupied())
            return true;
    }
    return false;
}

// First wins in, exact match out.
// ELF 0x842DDA0 / 0x842DE30.
IAIPlayerFactory::IAIPlayerFactory()
{
    if (!CommonSingletonServer::m_iaiPlayerFactory)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->registerSimpleSingleton(std::string("IAIPlayerFactory"), this);
        BFSingletonServer::updateStaticMethods();
    }
}

// ELF 0x842DEC0 / 0x842DF40 / 0x842DFC0.
IAIPlayerFactory::~IAIPlayerFactory()
{
    if (CommonSingletonServer::m_iaiPlayerFactory == this)
    {
        // The roster refresh. Seven interface pairs do this and nine do not - see BFTerrain.cpp.
        classManager->unregisterSimpleSingleton(std::string("IAIPlayerFactory"));
        BFSingletonServer::updateStaticMethods();
    }
}

// The ELF's _M_fill_insert / erase pair on an empty vector is `resize(256, 0)` - libstdc++ 3.x
// expands resize into exactly that branch. 256 is the player-id space, so the vector is an
// id-indexed table from the first instruction.
// ELF 0x842B7B0 / 0x842B850.
AIPlayerFactory::AIPlayerFactory()
{
    m_players.resize(256, 0);
}

// ELF 0x842B8F0 / 0x842B970 / 0x842B9F0.
AIPlayerFactory::~AIPlayerFactory()
{
}

// ELF 0x842BA80. Creates an unattached wrapper without inserting it into the id table.
IAIPlayer* AIPlayerFactory::create()
{
    return checkPointer(new AIPlayer());
}

// bf2 0x842BBA0: the player manager's player by id, wrapped through this factory's own
// create(IPlayer*) (+0x14); null for an unknown id.
// ELF 0x842BBA0.
IAIPlayer* AIPlayerFactory::create(int id)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(id);
    return player ? create(player) : 0;
}

// bf2 0x842BBE0: the same by name (+0x1C getPlayerFromName).
// ELF 0x842BBE0.
IAIPlayer* AIPlayerFactory::create(const std::string& name)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromName(name);
    return player ? create(player) : 0;
}

// Wraps a player and stores it at its player id. The caller must size m_players first;
// this function neither grows the vector nor checks the index.
// ELF 0x842BC20.
IAIPlayer* AIPlayerFactory::create(world::IPlayer* player)
{
    AIPlayer* const created = checkPointer(new AIPlayer(player));

    // No bounds check. See the block above.
    m_players[player->getId()] = created;

    return created;
}

// bf2 0x842BDC0: the table slot is cleared by id while the wrapper has a player, else by
// searching for the pointer; the wrapper is then deleted (null tolerated by the test before the
// deleting destructor).
// ELF 0x842BDC0.
void AIPlayerFactory::destroy(IAIPlayer* player)
{
    if (player->getPlayer())
    {
        m_players[player->getId()] = 0;
    }
    else
    {
        const std::vector<IAIPlayer*>::iterator it = std::find(m_players.begin(), m_players.end(), player);
        if (it != m_players.end())
            *it = 0;
    }
    if (player)
        delete player;
}

// bf2: the named player's id (IPlayer +0x54), or -1.
// ELF 0x842BE80.
int AIPlayerFactory::getPlayerIdFromName(const std::string& name)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromName(name);
    return player ? player->getId() : -1;
}

// IAIPlayerFactory's ELF slot +0x20
// (PE +0x1C) is the lookup used by BotManager::addUnspawnedBot. Both bodies reject
// negative ids and otherwise index the existing table without an upper-bound check.
// The vector's storage begins at ELF +4 / VC7.1 PE +8; use the source container.
IAIPlayer* AIPlayerFactory::getPlayerFromId(int id)
{
    if (id < 0)
        return 0;

    return m_players[id];
}

// bf2: unchecked store at the wrapper's id (IAIPlayer +0x1C).
// ELF 0x842BD60.
void AIPlayerFactory::setIdToPlayerMapping(IAIPlayer* player)
{
    m_players[player->getId()] = player;
}

// ELF 0x842BD90.
void AIPlayerFactory::removeIdToPlayerMapping(IAIPlayer* player)
{
    m_players[player->getId()] = 0;
}

// - a bare member load, so it hands back the vector itself.
// ELF 0x842BE70.
const std::vector<IAIPlayer*>& AIPlayerFactory::getAllPlayers()
{
    return m_players;
}

// bf2: the local human (+0x30 getLocalHumanPlayer) looked up by id, null without one.
// ELF 0x842BEC0.
IAIPlayer* AIPlayerFactory::getPlayerFromLocalHumanPlayer()
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    return player ? m_players[player->getId()] : 0;
}

} // namespace ai
} // namespace hfe
} // namespace dice
