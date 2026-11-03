// [INFERRED] Source placement.
// [TEMP] Temporary implementation boundaries remain.
// [CHOSEN, not recovered] Temporary implementation boundaries remain.
// [RETIRE-WHEN: aidll-01-adapters] Complete and validate the remaining BFGame operations.

#include "BFGame.h"

#include "../../../Core/Debug.h"        // the no-settings report

#include "AIFrontier.h"
#include "../../../Game/Objects/ISpawnPoint.h"   // getSpawnPointPositions
#include "../../../Game/Objects/IObject.h"

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"    // IAISettings, the menu arm's override
#include "../../../Game/GeneralSettings.h"                 // IGeneralSettings, the menu arm
#include "../../../Game/LocalProfileManager.h"             // ILocalProfileManager, the menu arm
#include "../../../Core/StringUtil.h"                      // compareIgnoreCase (0x431950)
#include "../../../Core/ClassManager.h"
#include "../../../Game/GameServer/ServerSettings.h"
#include "../../../Game/IGame.h"       // the `game` global and queryInterface (+0x0C)
#include "../../../Game/IGameLogic.h"  // the `gameLogic` global - the fog and view-distance reads
#include "../../../Core/Ids.h"         // IID_IGameServer
#include "../../../Core/SettingsRepostitory.h"   // the `settings` global - getLevelPath's two keys
#include "../../../Game/Objects/IObject.h"                  // getAbsolutePosition, getTemplate
#include "../../../Game/Objects/ControlPointTemplate.h"     // getRadius, getUnableToChangeTeam
#include "../../../Game/Objects/Game/ControlPoint.h"        // getTeam, getTakeOverCounter
#include "../../../Game/GameLogic/ScoreSystem/ScoreManager.h" // scoreManager, TeamScore::m_tickets
#include "../../../Game/GameLogic/SpawnSystem/SpawnManager.h"
#include "../../../Game/GameLogic/SpawnSystem/SpawnGroup.h"

#include "../../../Game/Objects/Projectiles/IProjectileObject.h"
#include "../../../Game/Objects/Projectiles/GenericProjectile.h"   // getTeam, IProjectileDetonationComp
#include "../../../Game/Objects/ObjectLayoutAccess.h"              // IObject +0x34, the AI object
#include "../../../Game/Objects/IPlayer.h"                         // getCloseMines, getId
#include "../../../IO/Network/INetworkable.h"                      // m_networkableId
#include "../../../Math/Vec3.h"                                    // vec3ToVec2XZ, insideCentreBox
#include "AIPlayer.h"
#include "IAIObject.h"

#include <algorithm>                                        // std::find over the ignore list
#include <map>                                              // the control-point map's own type

namespace dice {
namespace hfe {

extern ClassManager* classManager;
extern IBase* spawnManager; // Core/Globals.cpp; SpawnManager's primary IBase subobject.
namespace world { extern IBase* scoreManager; } // Core/Globals.cpp's shared storage

namespace ai {

// First wins in, exact match out.
IAIGame::IAIGame()
{
    if (!CommonSingletonServer::m_iaiGame)
        classManager->registerSimpleSingleton(std::string("IAIGame"), this);
}

IAIGame::~IAIGame()
{
    if (CommonSingletonServer::m_iaiGame == this)
        classManager->unregisterSimpleSingleton(std::string("IAIGame"));
}

// (!) it starts precaching true. The constructor's only store is `[this+4] = 1`.
BFGame::BFGame()
    : m_isPrecaching(true)
{
}

BFGame::~BFGame()
{
}

// - a tail jump to ServerSettings::getMaxPlayers
// on the serverSettings global (ds:).
int BFGame::getMaxNumberOfPlayers() const
{
    return serverSettings->getMaxPlayers();
}

// Copies game settings into the AI configuration, including bot population and team balance.
// BF2.exe 0x815C50 (530 B, RET 0xC). With a "GeneralSettings" singleton (the client hosting a game
// from the menu) the values come from the profile's GeneralSettings; otherwise from serverSettings.
// The menu arm, in order:
//   1. "LocalProfileManager" (string 0x87FD98) must exist and have a current profile (+0x44);
//      otherwise return false (0x815DB4) and leave a/b/c alone.
//   2. settings +0x40 stringGet("GSGameMode", "gpm_cq") (0x880688 / 0x89B4D0); when it equals
//      "sp1" (0x89B200, compareIgnoreCase 0x431950), the bot count is forced: 16, or
//      IAISettings (CommonSingletonServer, 0xA3867C) slot 0 getMaxNBots when IAISettings exists
//      and its slot +0xB8 overrideMenuSettings() answers true; then GeneralSettings +0xB4
//      setMaxBots(count).
//   3. a = +0xB0 getMaxBots, b = +0xB8 getMaxBotsIncludeHumans, c = +0xC0 getBotSkill (stored
//      through the x87 stack as a float); return true.
int BFGame::getAISettingsFromGame(int& a, bool& b, float& c) const
{
    IBase* general = classManager->getSingleton(std::string("GeneralSettings"));
#if DEDICATED
    // The dedicated images declare no GeneralSettings (Game/GeneralSettings.h is client-only), and
    // no dedicated module registers the singleton, so this arm cannot be reached there.
    if (general)
    {
        BF2_AI_FRONTIER("dice::hfe::ai::BFGame::getAISettingsFromGame"
                        " - the MENU arm in a dedicated image (no GeneralSettings interface)");
    }
#else
    if (general)
    {
        IGeneralSettings* generalSettings = static_cast<IGeneralSettings*>(general);
        ILocalProfileManager* profiles = static_cast<ILocalProfileManager*>(
            classManager->getSingleton(std::string("LocalProfileManager")));
        if (!profiles || !profiles->getCurrentProfile())                            // +0x44
            return 0;
        const std::string gameMode(settings->stringGet(std::string("GSGameMode"),    // +0x40
                                                       std::string("gpm_cq")));
        if (compareIgnoreCase(gameMode, std::string("sp1")) == 0)
        {
            int count = 16;
            const IAISettings* aiSettings =
                static_cast<const IAISettings*>(CommonSingletonServer::m_iaiSettings);
            if (aiSettings && aiSettings->overrideMenuSettings())                    // +0xB8
                count = aiSettings->getMaxNBots();                                    // +0x00
            generalSettings->setMaxBots(count);                                       // +0xB4
        }
        a = generalSettings->getMaxBots();                                            // +0xB0
        b = generalSettings->getMaxBotsIncludeHumans();                               // +0xB8
        c = generalSettings->getBotSkill();                                           // +0xC0
        return 1;
    }
#endif

    // no settings and no menu is A reported failure, not A silent zero - and the caller honours
    // the false by leaving the script's values alone.
    if (!serverSettings)
    {
        Debug(DebugTypeError, "AI/AIInterface/BFKnowledge/BFGame.cpp", 104)
            << "GeneralSettings are not available, no settings importade from menu.";
        return 0;
    }

    a = serverSettings->getCoopBotCount();
    b = false;
    // the scale is 0.0099999998f in the binary - the nearest float to 1/100, not the double 0.01.
    // Difficulty is authored 0..100 and the AI wants 0..1.
    c = serverSettings->getCoopBotDifficulty() * 0.0099999998f;
    return 1;
}

// Returns whether the current game exposes the server interface; false when no game exists.
bool BFGame::isServer() const
{
    if (!game)
        return false;

    return game->queryInterface(IID_IGameServer) != 0;
}

// getTeamScore is the typed +0x10
// dispatch on both ports; the returned TeamScore's +0x84 is read without a guard.
int BFGame::getTickets(int team) const
{
    return static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(team)->m_tickets;
}

// bf2 0x83FF9B0 (ELF listing, Ghidra pseudo-code as cross-check). Walks the player's close mines
// (IPlayer +0x32C) for at most 100 entries, counting skipped ones. A mine counts when its
// projectile team (IProjectileObject +0x64) differs from the bot's side (IAIPlayer +0x6C) and its
// object id is not in `ignore`; the nearest by squared distance wins. For each new best, in this
// order: id, position (IObject +0x6C), normal = the transformation's third row (+0x84, bytes
// 0x20..0x28), found = true, then found = false when the detonation component's
// getRadiusTriggerType (+0x78) is 2. Neither the projectile face nor the component is null-tested.
// The return value is whether any mine was taken (a byte; the caller tests %al).
int BFGame::getClosestHostileMine(IAIPlayer* player, Vec3 from, bool& found, Vec3& position, Vec3& normal, unsigned int& id, std::vector<unsigned int>& ignore) const
{
    bool any = false;
    world::IPlayer* const owner = player->getPlayer();                                    // +0x0C
    const int side = player->getSide();                                                   // +0x6C
    std::vector<world::IObject*> mines;
    owner->getCloseMines(mines);                                                          // +0x32C
    int count = 0;
    Vec3 minePosition;
    minePosition.x = 0.0f;
    minePosition.y = 0.0f;
    minePosition.z = 0.0f;
    float best = 3.4028235e38f;
    if (!mines.empty())
    {
        for (std::vector<world::IObject*>::iterator it = mines.begin(); it != mines.end(); ++it)
        {
            if (++count > 100)
                break;
            world::IObject* const mine = *it;
            // GenericProjectile is IProjectileObject's only implementor; the typed downcast is the
            // retail +0x64 subtable dispatch (thunk _ZThn316_ ... getTeam at vtable +0x37C).
            world::GenericProjectile* const projectile = static_cast<world::GenericProjectile*>(
                static_cast<world::IProjectileObject*>(
                    mine->queryInterface(world::IID_IProjectileObject)));                 // +0x0C
            if (projectile->getTeam() == side)                                            // +0x64
                continue;
            const unsigned int mineId = mine->getObjectId();                              // +0x10
            if (std::find(ignore.begin(), ignore.end(), mineId) != ignore.end())
                continue;
            minePosition = mine->getAbsolutePosition();                                   // +0x6C
            const Vec3 at = from;
            const float dx = at.x - minePosition.x;
            const float dy = at.y - minePosition.y;
            const float dz = at.z - minePosition.z;
            const float distance = dx * dx + dy * dy + dz * dz;
            if (distance < best)
            {
                any = true;
                id = mineId;
                best = distance;
                position = minePosition;
                const float* const m = mine->getAbsoluteTransformation().m;              // +0x84
                normal.x = m[8];
                normal.y = m[9];
                normal.z = m[10];
                world::IProjectileDetonationComp* const detonation =
                    static_cast<world::IProjectileDetonationComp*>(
                        mine->queryComponent(world::IID_IProjectileDetonationComp,
                                             world::IID_IProjectileDetonationComp));      // +0x5C
                found = true;
                if (detonation->getRadiusTriggerType() == 2)                              // +0x78
                    found = false;
            }
        }
    }
    return any;
}

// Only gameLogic is guarded.
// The map is captured once and traversed in key order; every point's live team
// is queried once. PE uses the adjusted IControlPoint base (+0x1C), whereas ELF
// uses ControlPoint's primary +0x1A0; the native typed call handles that difference.
int BFGame::getNumberOfControlPoints(int team) const
{
    if (!gameLogic)
        return 0;

    typedef std::map<unsigned int, world::IObject*> ControlPointMap;
    ControlPointMap* const points = gameLogic->getControlPointMap();
    int count = 0;
    for (ControlPointMap::iterator it = points->begin(); it != points->end(); ++it)
    {
        if (static_cast<world::ControlPoint*>(it->second)->getTeam() == team)
            ++count;
    }
    return count;
}

// Control-point queries borrow the game logic's map.
// The game logic and requested control-point ID must be valid.

// Map lookup, then vtable slot +0x1A0 on the object's primary table - which
// `SpawnPoint::getActive` already recorded as `ControlPoint::getTeam`, with the warning that
// `IControlPoint`'s own +0x1C is a different slot. The typed call below sidesteps that entirely.
int BFGame::getOwnerOfControlPoint(unsigned int index) const
{
    typedef std::map<unsigned int, world::IObject*> ControlPointMap;

    ControlPointMap* const points = gameLogic->getControlPointMap();

    // no `end` check. See the block above.
    const ControlPointMap::iterator found = points->find(index);

    return static_cast<world::ControlPoint*>(found->second)->getTeam();
}

// The result is the
// control point's float capture progress, forwarded in ST0 without conversion.
// ELF calls the primary +0x1AC slot; PE adjusts to IControlPoint and calls +0x5C.
// Like getOwnerOfControlPoint, neither the game/map nor find's result is guarded.
float BFGame::getControlPointTakeOverCounter(unsigned int index) const
{
    typedef std::map<unsigned int, world::IObject*> ControlPointMap;

    ControlPointMap* const points = gameLogic->getControlPointMap();
    const ControlPointMap::iterator found = points->find(index);

    return static_cast<world::ControlPoint*>(found->second)->getTakeOverCounter();
}

// Returns whether the control point can change teams and writes that value to active.
// Writes the live position and template radius. The control-point ID must exist.
bool BFGame::getControlPointInfo(unsigned int index, Vec3& position, float& radius, bool& active) const
{
    typedef std::map<unsigned int, world::IObject*> ControlPointMap;

    ControlPointMap* const points = gameLogic->getControlPointMap();

    // no `end` check. See the block above.
    const ControlPointMap::iterator found = points->find(index);

    world::IObject* const object = found->second;

    position = object->getAbsolutePosition();

    const world::ControlPointTemplate* const controlPointTemplate =
        static_cast<const world::ControlPointTemplate*>(object->getTemplate());

    radius = controlPointTemplate->getRadius();

    active = !controlPointTemplate->getUnableToChangeTeam();

    return active;
}

// bf2 0x83FF350. The id of the first control point (key order) whose XZ position lies in the box
// insideCentreBox(position, a, b) (bf2 0x83F7A20, same code as the AI image's) describes; -1 when
// there is none or no game logic. The Vec2 scratch is zeroed once, before the walk.
int BFGame::findControlPoint(const Vec2& a, const Vec2& b) const
{
    Vec2 position;
    position.x = 0.0f;
    position.y = 0.0f;
    if (!gameLogic)
        return -1;
    typedef std::map<unsigned int, world::IObject*> ControlPointMap;
    ControlPointMap* const points = gameLogic->getControlPointMap();                      // +0x174
    for (ControlPointMap::iterator it = points->begin(); it != points->end(); ++it)
    {
        vec3ToVec2XZ(it->second->getAbsolutePosition(), position);                        // +0x6C
        if (insideCentreBox(position, a, b))
            return static_cast<int>(it->first);
    }
    return -1;
}

// The returned list is
// the manager's borrowed scratch storage, not an owned copy. No manager null guard.
const std::list<int>& BFGame::getSpawnPointList(int team) const
{
    return static_cast<SpawnManager*>(spawnManager)->getGroupsForTeam(team);
}

// Both calls dispatch
// virtually; the group and returned position are borrowed, with no null guard.
const Vec3& BFGame::getSpawnPointPosition(int index) const
{
    return static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(index)->getPos();
}

// retain the returned
// group's identity for its virtual query. No manager/group guard; preserve the
// full 32-bit result, including the caller's -1 sentinel.
int BFGame::getControlPointId(int index) const
{
    return static_cast<int>(static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(index)->getControlPointId());
}

// bf2 0x83FF800. Every group's spawn points (copied vector, ISpawnGroup +0x40) that are neither
// AI-enter-on-spawn (+0x24), enter-on-spawn (+0x20) nor human-only (+0x50) add their spawn
// position (+0x34, checkAI true, from a zero Vec3). The team is not read.
// [CHOSEN, not recovered] The ELF leaves the last call's EAX; no caller reads it, 0 here.
int BFGame::getSpawnPointPositions(std::vector<Vec3>& out, unsigned char) const
{
    const std::map<int, ISpawnGroup*>& groups =
        static_cast<SpawnManager*>(spawnManager)->getSpawnGroups();
    for (std::map<int, ISpawnGroup*>::const_iterator group = groups.begin();
         group != groups.end(); ++group)
    {
        const std::vector<ISpawnPoint*> points = group->second->getSpawnPoints();
        for (std::vector<ISpawnPoint*>::const_iterator it = points.begin(); it != points.end(); ++it)
        {
            ISpawnPoint* const point = *it;
            if (point->getAIEnterOnSpawn() || point->getEnterOnSpawn() || point->getOnlyForHuman())
                continue;
            Vec3 position;
            position.x = 0.0f;
            position.y = 0.0f;
            position.z = 0.0f;
            point->getSpawnPosition(position, true);
            out.push_back(position);
        }
    }
    return 0;
}

// bf2 0x83FFCB0: every control point's absolute position (IObject +0x6C), from the game logic's
// control-point map (IGameLogic +0x174); nothing without a game logic. The team is not read.
void BFGame::getControlPointPositions(std::vector<Vec3>& out, unsigned char) const
{
    if (!gameLogic)
        return;
    std::map<unsigned int, world::IObject*>* const points = gameLogic->getControlPointMap();
    for (std::map<unsigned int, world::IObject*>::iterator it = points->begin(); it != points->end(); ++it)
        out.push_back(it->second->getAbsolutePosition());
}

// Capture team before
// getActive(true), but compare it afterwards. A matching team's human-only query
// still runs when the captured active flag is false.
bool BFGame::isEligibleSpawnpoint(int team, int groupId) const
{
    if (groupId == 0)
        return false;

    ISpawnGroup* const group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(groupId);
    if (!group)
        return false;

    const int groupTeam = group->getTeam();
    const bool active = group->getActive(true);
    if (groupTeam != team)
        return false;
    if (group->getOnlyForHuman())
        return false;
    return active;
}

// bf2 0x83FF530. Group 0 or an unknown group is false; otherwise the group's AI spawn point
// (ISpawnGroup +0x44, true) answers enter-on-spawn (+0x20) or AI-enter-on-spawn (+0x24). The
// returned point is not null-tested.
bool BFGame::isEnterOnSpawnSpawnpoint(int index) const
{
    if (index == 0)
        return false;
    ISpawnGroup* const group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(index);  // +0x28
    if (!group)
        return false;
    ISpawnPoint* const point = group->getSpawnPoint(true);                                 // +0x44
    return point->getEnterOnSpawn() || point->getAIEnterOnSpawn();                          // +0x20 / +0x24
}

// bf2 0x83FF5B0. 0 for group 0 or an unknown group; otherwise the AI spawn point's object to
// enter (ISpawnPoint +0x28, (true, side)) and that object's AI object (IObject +0x34): -1 without
// one, else its getInformation(side) (IAIObject +0xB8). Neither the point nor the object is
// null-tested.
int BFGame::getEnterOnSpawnSpawnpointObject(int a, int b) const
{
    if (a == 0)
        return 0;
    ISpawnGroup* const group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(a);  // +0x28
    if (!group)
        return 0;
    world::IObject* const object = group->getSpawnPoint(true)->getObjectToEnter(true, b);   // +0x44, +0x28
    IAIObject* const aiObject =
        static_cast<IAIObject*>(world::object_detail::LayoutAccess::aiObject(object));     // +0x34
    if (!aiObject)
        return -1;
    return static_cast<int>(aiObject->getInformation(b));                                  // +0xB8
}

// Returns the level directory joined with the configured level name.
std::string BFGame::getLevelPath() const
{
    const std::string& level =
        settings->stringGet(std::string("GSLoadLevel"), std::string(""));
    const std::string& directory =
        settings->stringGet(std::string("GSLevelDirectory"), std::string(""));

    return directory + level;
}

// - `mov eax, 0Ah`. A constant: ten squads.
int BFGame::getMaxNumberOfSquads() const
{
    return 10;
}

// - `mov eax, 6`. A constant: six per squad.
int BFGame::getMaxNumberOfSquadMembers() const
{
    return 6;
}

// Computes the recommended AI view distance from the fog settings.
float BFGame::getRecomendedViewDistance() const
{
    const Vec4& fog = gameLogic->getFogStartEndAndBase();

    float t = 0.0f;
    for (;;)
    {
        const float linear  = fog.z * t;
        const float density = (fog.w > linear) ? linear : (t * t * t + fog.w);

        if (density >= 0.8f)
            break;

        t += 0.01f;
        if (t > 1.0f)
            break;
    }

    const float fogged = t * (fog.y - fog.x) + fog.x;

    float distance = gameLogic->getMaximumLevelViewDistance();
    if (distance > fogged)
        distance = fogged;
    if (distance > 300.0f)
        distance = 300.0f;

    return distance;
}

// Returns whether the game is in the playing state.
bool BFGame::isPlaying() const
{
    return gameLogic->getGameStatus() == GameStatus_Playing;
}

void BFGame::setIsPrecaching(bool precaching)
{
    m_isPrecaching = precaching;
}

bool BFGame::getIsPrecaching() const
{
    return m_isPrecaching;
}

// bf2 0x83FFE30. Without a game nothing happens. Otherwise the spotted object's networkable id
// (IObject +0xD8, the u16 at +0x04; 0 without one) goes out as a spotted message with a zero Vec2
// and vehicle type 26 (IGame +0xD0), and the game's spotted text for that id (+0xCC) is sent as a
// team radio message from the player (+0x58, (TCTeam, text, player id, 0, false, false)).
// [UNRESOLVED] the name of VehicleType 26.
void BFGame::enemySpottedByAI(world::IPlayer* player, world::IObject* object) const
{
    if (!game)
        return;
    unsigned short networkableId = 0;
    if (io::INetworkable* const networkable = object->getNetworkable())                  // +0xD8
        networkableId = networkable->m_networkableId;
    Vec2 zero;
    zero.x = 0.0f;
    zero.y = 0.0f;
    game->sendSpottedMessage(player, networkableId, zero, static_cast<world::VehicleType>(26));  // +0xD0
    const unsigned int playerId = static_cast<unsigned int>(player->getId());            // +0x54
    const std::string message = game->getSpottedMessage(networkableId);                  // +0xCC
    game->radioMessageFromPlayer(TCTeam, message, playerId, 0, false, false);            // +0x58
}

} // namespace ai
} // namespace hfe
} // namespace dice

#if 0   // Disabled subsystem declarations.

#endif
