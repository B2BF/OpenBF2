// [INFERRED] Source placement.

#include "BFGameEffector.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"   // IAISettings::getBotName
#include "../../../Core/ClassManager.h"
#include "../../../Core/Ids.h"                          // IID_IGameServer
#include "../../../Game/IGame.h"                        // the `game` global
#include "../../../Game/IGameServer.h"                  // createPlayer, slot +0x20
#include "../../../Scene/Object/PlayerManager.h"        // the `playerManager` global
#include "../../../Game/IGameLogic.h"                   // the `gameLogic` global
#include "../../../Game/GameLogic/ServerGameLogic.h"    // forceSpawnBot
#include "../../../Core/SettingsRepostitory.h"          // the `settings` global
#include "../../../Physics/MaterialManager.h"
#include "AIPlayer.h"
#include "IAIObject.h"

namespace dice {
namespace hfe {

extern ClassManager* classManager;
namespace world { extern IBase* materialManager; }

namespace ai {

// First wins in, exact match out.
// ELF 0x840E1E0 / 0x840E260.
IAIGameEffector::IAIGameEffector()
{
    if (!CommonSingletonServer::m_iaiGameEffector)
        classManager->registerSimpleSingleton(std::string("IAIGameEffector"), this);
}

// ELF 0x840E2E0 / 0x840E360 / 0x840E3E0.
IAIGameEffector::~IAIGameEffector()
{
    if (CommonSingletonServer::m_iaiGameEffector == this)
        classManager->unregisterSimpleSingleton(std::string("IAIGameEffector"));
}

// ELF 0x8400300 / 0x8400340.
BFGameEffector::BFGameEffector()
{
}

// ELF 0x8400380 / 0x84003E0 / 0x8400440.
BFGameEffector::~BFGameEffector()
{
}

// bf2 0x84004B0: IGame +0xA4 enterVehicle(player, the vehicle's engine object, false); the
// vehicle's object (IObjectComponent +0x20) is read before the player.
// ELF 0x84004B0.
int BFGameEffector::enterVehicle(IAIPlayer* player, IAIObject* vehicle)
{
    world::IObject* const object = vehicle->getObject();
    return game->enterVehicle(player->getPlayer(), object, false);
}

// bf2 0x8400500: IGame +0xA8 exitVehicle(player, true, true).
// ELF 0x8400500.
int BFGameEffector::exitVehicle(IAIPlayer* player)
{
    return game->exitVehicle(player->getPlayer(), true, true);
}

// Includes the overload arguments in the terminatePlayer frontier diagnostic.
// bf2 0x8400720: the player manager's player (+0x20) goes to IGameServer +0x28 destroyPlayer
// (the server is queried from `game`, which is not tested before the query's own null arm).
// [CHOSEN, not recovered] destroyPlayer is void; the ELF answers its leftover AL. 0 here.
int BFGameEffector::terminatePlayer(int playerId)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);
    IGameServer* const server =
        game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    server->destroyPlayer(player);
    return 0;
}

// bf2 0x8400780: the same for the wrapper's player.
// ELF 0x8400780.
int BFGameEffector::terminatePlayer(IAIPlayer* player)
{
    world::IPlayer* const target = player->getPlayer();
    IGameServer* const server =
        game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    server->destroyPlayer(target);
    return 0;
}

// Creates an AI player through the game server and returns its player ID.
// ELF 0x8400540.
int BFGameEffector::addBot(int& a, int& b, std::string& name)
{
    if (name.empty())
    {
        const IAISettings* const settings =
            static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);

        name.assign(settings->getBotName());
    }

    // equal weights - the engine balances, not the AI.
    b = world::playerManager->getTeamToAddPlayer(1.0f, 1.0f);

    IGameServer* const server =
        game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;

    if (server)
    {
        // asked A second time. See the block above; the binary does not reuse `server`.
        IGameServer* const target =
            game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;

        a = target->createPlayer(name, true);
        return a;
    }

    a = -1;
    return 0;
}

// bf2 0x8400620: IGameLogic +0x70 reSpawnPlayer.
// ELF 0x8400620.
int BFGameEffector::respawnBot(IAIPlayer* player)
{
    return gameLogic->reSpawnPlayer(player->getPlayer());
}

// bf2 0x8400650: only on a server (setting "GSIsServer", default false), the server game logic
// spawns the player at (position, rotation).
// ELF 0x8400650.
void BFGameEffector::forceSpawnBot(IAIPlayer* player, const Vec3& position, const Vec3& rotation)
{
    if (!settings->boolGet(std::string("GSIsServer"), false))
        return;
    std::pair<Vec3, Vec3> at(position, rotation);
    static_cast<ServerGameLogic*>(gameLogic)->spawnPlayer(player->getPlayer(), &at);
}

// bf2 0x84007D0: IGameLogic +0xE4 giveDamage on the target's engine object with no victim
// list, weapon or projectile objects, a zero position and an up (0, 1, 0) direction, -1 and the
// flags false, true, false. The int is the damage type slot.
// ELF 0x84007D0.
int BFGameEffector::giveDamage(IAIObject* target, world::IPlayer* from, int type, float amount)
{
    world::IObject* const object = target->getObject();
    Vec3 position;
    position.x = 0.0f;
    position.y = 0.0f;
    position.z = 0.0f;
    Vec3 direction;
    direction.x = 0.0f;
    direction.y = 1.0f;
    direction.z = 0.0f;
    return gameLogic->giveDamage(object, amount, from, 0, 0, type, 0, 0, position, direction, -1,
                                 false, true, false);
}

// ELF 0x8400880. The final engine effect call has no return value.
void BFGameEffector::playEffect(const Vec3& position, int a, int b)
{
    queueSoundEffect(position, a, b);
    const Vec3 up = { 0.0f, 1.0f, 0.0f };
    const world::IObjectTemplate* const effect = static_cast<const world::IObjectTemplate*>(
        static_cast<world::MaterialManager*>(world::materialManager)
            ->getEffectTemplate(static_cast<unsigned int>(a), static_cast<unsigned int>(b), 0));
    const Vec3 point = position;
    gameLogic->playEffect(point, effect, up);
}

// ELF 0x8400920. The dedicated ELF ships an empty body.
void BFGameEffector::playSoundEffects(double)
{
}

// ELF 0x8400930. The dedicated ELF ships an empty body.
void BFGameEffector::queueSoundEffect(const Vec3&, int, int)
{
}

// bf2 0x8400940: this effector's own giveDamage (+0x24) of 1e8f on the player's soldier, from
// the player, typed with its side; side, player and soldier are read in that order.
// ELF 0x8400940.
int BFGameEffector::suicide(IAIPlayer* player)
{
    const int side = player->getSide();
    world::IPlayer* const from = player->getPlayer();
    IAIObject* const soldier = player->getSoldier();
    return giveDamage(soldier, from, side, 1e8f);
}

} // namespace ai
} // namespace hfe
} // namespace dice
