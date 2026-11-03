// Engine implementation of the AI player interface.
// [INFERRED] Source placement.

#include "AIPlayer.h"

#include <cmath>
#include <cstdlib>

#include "../../AIDLL/AI/Main/CommonSingletonServer.h"
#include "../../AIDLL/AI/AIUtils/checkPointer.h"
#include "../../../Core/Ids.h"                        // CID_LadderContainerTemplate, CID_Parachute*
#include "../../../Game/GameLogic/Player.h"           // world::IPlayer - every forward's target
#include "../../../Game/Objects/IObject.h"            // getTemplate
#include "../../../Game/Objects/IObjectTemplate.h"    // getClassID
#include "../../../Game/Objects/ICameraObject.h"      // getCameraTransformation
#include "../../../Math/Mat4.h"                       // the lazily built identity matrix
#include "../../../Scene/Object/PlayerManager.h"      // the `playerManager` global
#include "../../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "AIPlayerFactory.h"
#include "../../../Game/IGameLogic.h"                 // gameLogic - removePlayerFromGame
#include "../AIInstance.h"                            // the AI instance test in the destructor
#include "../../AIDLL/AI/Main/AISettingsInterfaces.h"   // setBotNameUnused                          // the id-to-player mapping pair

namespace dice {
namespace hfe {
namespace ai {

// Zero-initialised, filled on the first construction, never freed.
Mat4* AIPlayer::identityMatrix = 0;

// The interface destructor has no additional cleanup.
// ELF 0x83FCB40 / 0x83FCB50.
IAIPlayer::~IAIPlayer()
{
}

// ELF 0x83FBA60 / 0x83FBC30. Wraps the supplied player and initializes view distance from its current value.
// The player is borrowed and must outlive this wrapper.
AIPlayer::AIPlayer(world::IPlayer* player)
    : m_player(player)
    , m_i8(0)
    , m_viewDistance(0.0f)
{
    m_viewDistance = m_player->getViewDistance();

    if (!identityMatrix)
    {
        Mat4* const matrix = new Mat4{{
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        }};
        identityMatrix = checkPointer(matrix);

        // Preserves the repeated identity-matrix stores, including diagonal indices 0, 5, 10, and 15.
        matrix->m[0]  = 1.0f;  matrix->m[1]  = 0.0f;  matrix->m[2]  = 0.0f;  matrix->m[3]  = 0.0f;
        matrix->m[4]  = 0.0f;  matrix->m[5]  = 1.0f;  matrix->m[6]  = 0.0f;  matrix->m[7]  = 0.0f;
        matrix->m[8]  = 0.0f;  matrix->m[9]  = 0.0f;  matrix->m[10] = 1.0f;  matrix->m[11] = 0.0f;
        matrix->m[12] = 0.0f;  matrix->m[13] = 0.0f;  matrix->m[14] = 0.0f;  matrix->m[15] = 1.0f;
    }
}

// ELF 0x83FBE00 / 0x83FBFD0. An unattached wrapper shares the same lazy identity matrix.
AIPlayer::AIPlayer()
    : m_player(0)
    , m_i8(0)
    , m_viewDistance(0.0f)
{
    if (!identityMatrix)
    {
        Mat4* const matrix = new Mat4{{
            1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f
        }};
        identityMatrix = checkPointer(matrix);
        matrix->m[0]  = 1.0f;  matrix->m[1]  = 0.0f;  matrix->m[2]  = 0.0f;  matrix->m[3]  = 0.0f;
        matrix->m[4]  = 0.0f;  matrix->m[5]  = 1.0f;  matrix->m[6]  = 0.0f;  matrix->m[7]  = 0.0f;
        matrix->m[8]  = 0.0f;  matrix->m[9]  = 0.0f;  matrix->m[10] = 1.0f;  matrix->m[11] = 0.0f;
        matrix->m[12] = 0.0f;  matrix->m[13] = 0.0f;  matrix->m[14] = 0.0f;  matrix->m[15] = 1.0f;
    }
}

// bf2 0x83FC280: while the AI instance (0x8AC37F8) exists and is not shutting down, a bot's
// name goes back to the pool (IAISettingsModifier +0x94 setBotNameUnused).
// ELF 0x83FC1A0 / 0x83FC210 / 0x83FC280.
AIPlayer::~AIPlayer()
{
    if (g_aiInstance && !aiInstance()->isShuttingDown() && m_player && getIsAIPlayer())
        static_cast<IAISettingsModifier*>(CommonSingletonServer::m_iaiSettingsModifier)
            ->setBotNameUnused(m_player->getName());
}

// Delegates to the wrapped engine player through the typed interface.

// - and it resets the one field that is genuinely this object's, not the
// player's. A recycled `AIPlayer` therefore keeps its view distance; only +8 is cleared.
// ELF 0x83FC300.
void AIPlayer::reset()
{
    m_i8 = 0;
}

// ELF 0x83FC440.
world::IPlayer* AIPlayer::getPlayer() const
{
    return m_player;
}

// it resolves the ID and then calls its own virtual slot +0x14 - a virtual
// self-call, not a direct one, so an override of the pointer form would catch this path too.
// ELF 0x83FC310.
void AIPlayer::setPlayer(int playerId)
{
    setPlayer(world::playerManager->getPlayerFromId(playerId));
}

// Replaces the factory id mapping when the player changes. Seeds view distance only
// when its current value equals zero; NaN is left unchanged.
// ELF 0x83FC350.
void AIPlayer::setPlayer(world::IPlayer* player)
{
    IAIPlayerFactory* const factory =
        static_cast<IAIPlayerFactory*>(CommonSingletonServer::m_iaiPlayerFactory);

    if (m_player)
        factory->removeIdToPlayerMapping(this);

    m_player = player;

    if (m_player)
        factory->setIdToPlayerMapping(this);

    if (m_player && m_viewDistance == 0.0f)
        m_viewDistance = m_player->getViewDistance();
}

// - IPlayer +0x24.
// ELF 0x83FC450.
const std::string& AIPlayer::getName() const
{
    return m_player->getName();
}

// - IPlayer +0x54.
// ELF 0x83FC4C0.
int AIPlayer::getId() const
{
    return m_player->getId();
}

// - IPlayer +0x50. A different number from `getId`, and the two slots are
// adjacent on both classes; do not collapse them.
// ELF 0x83FC4E0.
int AIPlayer::getInputId() const
{
    return m_player->getInputId();
}

// - IPlayer +0x6C, narrowed with `movzx`.
// ELF 0x83FC500.
bool AIPlayer::getIsAlive() const
{
    return m_player->getIsAlive();
}

// - IPlayer +0x68.
// ELF 0x83FC520.
void AIPlayer::setIsAlive(bool alive)
{
    m_player->setIsAlive(alive);
}

// - IPlayer +0x100.
// ELF 0x83FC550.
bool AIPlayer::isManDown() const
{
    return m_player->getIsManDown();
}

// Checks the current vehicle template to identify ladder and parachute states.
// ELF 0x83FC570.
bool AIPlayer::isClimbingLadder() const
{
    world::IObject* const vehicle = m_player->getVehicle();

    if (!vehicle)
        return false;

    return vehicle->getTemplate()->getClassID() == world::CID_LadderContainerTemplate;
}

// ELF 0x83FC5B0.
bool AIPlayer::isParachuting() const
{
    world::IObject* const vehicle = m_player->getVehicle();

    if (!vehicle)
        return false;

    return vehicle->getTemplate()->getClassID() == world::CID_ParachuteTemplate;
}

// - IPlayer +0x154.
// ELF 0x83FC610.
void AIPlayer::setSpawnGroup(int group)
{
    m_player->setSpawnGroup(group);
}

// forward the selected kit unchanged to
// IPlayer::setKit (ELF +0x16C / PE +0x168), with no player-null guard or validation.
// ELF 0x83FC640.
void AIPlayer::setSpawnKit(int kit)
{
    m_player->setKit(kit);
}

// - IPlayer +0x90, `getFov`. this is what corrected the declared return type:
// the body materialises nothing in eax and leaves the player's float in st0.
// ELF 0x83FC880.
float AIPlayer::getFieldOfView()
{
    return m_player->getFov();
}

// - IPlayer +0x9C, `getAIPlayerInput`. the player has A separate input block
// for AI, and this is the slot that hands it over; the name on the player side says so outright.
// ELF 0x83FC5F0.
const void* AIPlayer::getPlayerInput() const
{
    return m_player->getAIPlayerInput();
}

// ELF 0x83FC670.
int AIPlayer::getSpawnGroup() const
{
    // IPlayer virtual +0x158/+0x154, without a null guard or value conversion.
    return m_player->getSpawnGroup();
}

// ELF 0x83FC690.
int AIPlayer::getSpawnKit() const
{
    // IPlayer virtual +0x170/+0x16C, with no null guard or value conversion.
    return m_player->getKit();
}

// bf2 0x83FC6B0. Without a vehicle -1. Otherwise the first player control object on the
// vehicle's parent chain (queryInterface IID_IPlayerControlObject, then IObject +0x28) answers
// its PCO id (+0x30). Ladder, parachute, zipline and rope containers first hop to the object
// their control object occupies (+0x14) and search from there. A chain without one reads
// through a null pointer, as the ELF does.
namespace {
world::IPlayerControlObject* findControlObject(world::IObject* object)
{
    for (; object; object = object->getParent())
    {
        world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
            object->queryInterface(world::IID_IPlayerControlObject));
        if (control)
            return control;
    }
    return 0;
}
} // anonymous namespace

// ELF 0x83FC6B0.
unsigned int AIPlayer::getPlayerControlObjectId()
{
    // The ELF reads the template before its null test (a null vehicle faults there); testing
    // first keeps the C++ defined. [CHOSEN, not recovered] ordering only.
    world::IObject* const vehicle = m_player->getVehicle();
    if (!vehicle)
        return static_cast<unsigned int>(-1);
    const unsigned int classId = vehicle->getTemplate()->getClassID();
    if (classId == world::CID_LadderContainerTemplate || classId == world::CID_ParachuteTemplate ||
        classId == world::CID_ZiplineContainerTemplate ||
        classId == world::CID_GrapplingHookRopeContainerTemplate)
    {
        world::IObject* const occupied = findControlObject(vehicle)->getOccupingObject();
        return static_cast<unsigned int>(findControlObject(occupied)->getPcoId());
    }
    return static_cast<unsigned int>(findControlObject(vehicle)->getPcoId());
}

// bf2 0x83FC7C0: the default vehicle's AI component; a player without one aborts (the ELF
// calls abort directly - an inlined assertion).
// ELF 0x83FC7C0.
IAIObject* AIPlayer::getSoldier()
{
    if (!m_player->getDefaultVehicle())
        std::abort();
    return static_cast<IAIObject*>(m_player->getDefaultVehicle()->getAIComponent());
}

// bf2 0x83FC7A0: IPlayer +0xA4, then the object's cached AI component (+0x34), unchecked.
// ELF 0x83FC7A0.
IAIObject* AIPlayer::getDefaultVehicle()
{
    return static_cast<IAIObject*>(m_player->getDefaultVehicle()->getAIComponent());
}

// ELF 0x83FC800.
IAIObject* AIPlayer::getCurrentVehicle()
{
    // One vehicle lookup; no template/occupant guards.
    // Ladder, parachute, zipline and rope wrappers expose their occupying object's AI component.
    world::IObject* vehicle = m_player->getVehicle();
    if (!vehicle)
        return 0;

    const unsigned int classId = vehicle->getTemplate()->getClassID();
    if (classId == world::CID_LadderContainerTemplate ||
        classId == world::CID_ParachuteTemplate ||
        classId == world::CID_ZiplineContainerTemplate ||
        classId == world::CID_GrapplingHookRopeContainerTemplate)
    {
        world::IPlayerControlObject* control = static_cast<world::IPlayerControlObject*>(
            vehicle->queryInterface(world::IID_IPlayerControlObject));
        return static_cast<IAIObject*>(control->getOccupingObject()->getAIComponent());
    }
    return static_cast<IAIObject*>(vehicle->getAIComponent());
}

// bf2 0x83FC8B0. Without a camera (IPlayer +0x180) 1.0. Otherwise cot(fov / 2) scaled by a
// function static cot(0.6f) built on first use (0x8AC3310 flag, 0x8AC3318 value, 1.0f / tan).
// [PLATFORM] the ELF keeps the quotients in x87 extended precision.
// ELF 0x83FC8B0.
float AIPlayer::getFieldOfViewModifier()
{
    if (!m_player->getCamera())
        return 1.0f;
    static bool s_initialised = false;
    static float s_standardCotangent;
    if (!s_initialised)
    {
        const float standard = 0.6f;
        s_initialised = true;
        s_standardCotangent = 1.0f / (sinf(standard) / cosf(standard));
    }
    const float half = m_player->getFov() * 0.5f;
    return (cosf(half) / sinf(half)) * s_standardCotangent;
}

// bf2: `flds` of the 1.2f constant at 0x889AA58.
// ELF 0x83FC8A0.
float AIPlayer::getStandardFieldOfView()
{
    return 1.2f;
}

// bf2 0x83FC960: a fifth of the stored distance (0.2f at 0x8899EF0) on night maps
// (IPlayer +0x2FC), reached through this wrapper's own getPlayer (+0x0C).
// ELF 0x83FC960.
float AIPlayer::getViewDistance()
{
    if (getPlayer()->isPlayerInNightMap())
        return 0.2f * m_viewDistance;
    return m_viewDistance;
}

// bf2: a plain store to +0x0C.
// ELF 0x83FC9A0.
void AIPlayer::setViewDistance(float distance)
{
    m_viewDistance = distance;
}

// ELF 0x83FC9B0.
int AIPlayer::getSide()
{
    // unchecked IPlayer::getTeam forward.
    return m_player->getTeam();
}

// forward the side and false to
// IPlayer::setTeam (ELF +0xE4 / PE +0xE0), with no player-null guard.
// ELF 0x83FC9D0.
void AIPlayer::setSide(int side)
{
    m_player->setTeam(side, false);
}

// bf2 0x83FCA00 (seven bytes, `xor %eax,%eax`): always 0; the weapon is not read.
// ELF 0x83FCA00.
int AIPlayer::getNumShotsFired(IWeapon*)
{
    return 0;
}

// bf2 0x83FC3D0 (recovered on the client track, docs/macos-c-singleplayer.md). The singleton at
// ds:0x8AC37F8 is AIInstanceServer::m_iAI (g_aiInstance here); +0x20 is isShuttingDown. While the
// AI is not shutting down, a live player (IPlayer +0x6C) is killed through the game logic
// (ds:0x8ACBFA0, IGameLogic +0x6C, killPlayer(player, true, false)). The instance is not null-
// tested, as shipped.
// ELF 0x83FC3D0.
void AIPlayer::removePlayerFromGame()
{
    if (aiInstance()->isShuttingDown())
        return;
    if (m_player && m_player->getIsAlive())
        gameLogic->killPlayer(m_player, true, false);
}

// bf2 0x83FCA10: IPlayer +0x20.
// ELF 0x83FCA10.
void AIPlayer::setName(const std::string& name)
{
    m_player->setName(name);
}

// bf2 0x83FCA40: the first int of the player's score data (IPlayer +0x174), 0 without one.
// [UNRESOLVED] PlayerScoreData's layout; only its leading score word is read here.
// ELF 0x83FCA40.
int AIPlayer::getScore()
{
    world::PlayerScoreData* const data = m_player->getScoreData();
    return data ? *reinterpret_cast<const int*>(data) : 0;
}

// bf2 0x83FCA70: IPlayer +0x60.
// ELF 0x83FCA70.
void AIPlayer::setIsAIPlayer(bool isAI)
{
    m_player->setIsAIPlayer(isAI);
}

// forward the player's
// Boolean query through IPlayer (+0x64 / +0x60), with no null guard.
// ELF 0x83FCAA0.
bool AIPlayer::getIsAIPlayer()
{
    return m_player->getIsAIPlayer();
}

// bf2 0x83FC470: the player's camera (IPlayer +0x180 getCamera) as a world::IObject
// (queryInterface(IID_IObject), +0x0C) gives its absolute transformation (IObject +0x84); without
// a camera the shared identity matrix. The result is the Mat4's address.
// ELF 0x83FC470.
const void* AIPlayer::getCameraTransformation() const
{
    world::ICameraObject* const camera = m_player->getCamera();
    if (!camera)
        return identityMatrix;
    const world::IObject* const object =
        static_cast<const world::IObject*>(camera->queryInterface(world::IID_IObject));
    return &object->getAbsoluteTransformation();
}

} // namespace ai
} // namespace hfe
} // namespace dice
