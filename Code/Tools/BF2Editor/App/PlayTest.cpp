// Code/Tools/BF2Editor/App/PlayTest.cpp - see PlayTest.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/PlayTest.h"

#include "App/EditSession.h"
#include "App/EditorGame.h"
#include "App/FreeCamera.h"
#include "Framework/EditorLog.h"

#include "Core/EventManager.h"
#include "Core/Ids.h"
#include "Core/System.h"
#include "Game/Common/PlayerAction.h"
#include "Game/GameServer/ServerInfo.h"
#include "Game/IGameLogic.h"
#include "Game/Objects/Game/EntryPoint.h"
#include "Game/Objects/ICameraObject.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "Game/Objects/IPlayer.h"
#include "Game/Objects/IPlayerControlObjectTemplate.h"
#include "Game/Objects/ISoldier.h"
#include "Game/Objects/IWeaponObject.h"
#include "Game/Objects/ObjectSpawnerTemplate.h"
#include "IO/Input/PlayerInput.h"
#include "Math/Mat4.h"
#include "Physics/PhysicsNodeManager.h"
#include "Physics/PhysicsUtil.h"
#include "Physics/ResponsePhysicsManager.h"
#include "RendMisc/IRenderView.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectPredicator.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "Scene/Object/PlayerManager.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

namespace dice { namespace hfe {
namespace world { extern ResponsePhysicsManager* responsePhysicsManager; }   // Core/Globals.cpp
namespace world { extern PhysicsNodeManager* physicsNodeManager; }          // Core/Globals.cpp
// The camera key's mode cycle, a free function of ServerGameLogic.cpp (ELF 0x8105A40) that no header declares.
namespace world { void toggleViewMode(ICameraObject* camera, bool forward); }
} }

namespace bf2editor
{

using namespace dice::hfe;

namespace
{

const char* const kPlayerName = "EditorPlayer";
const int         kPlayerId = 255;              // the retail editor's, and the 1.50 listen server's local player
const float       kStep = 1.0f / 30.0f;         // WorldPref::mTickTime
const float       kAboveGround = 1.5f;          // the retail editor stood the soldier 1.5 m over the ground (EE 0x10042C70)
const float       kMouseAxisScale = 0.03f;      // ControlMap's default axis scale (ControlMap.cpp), sensitivity 1
// Controls.con's mouse sensitivity for the two flying maps (AirPlayerInputControlMap, HelicopterPlayerInputControlMap).
const float       kAirMouseSensitivity = 1.7f;
const float       kHelicopterMouseSensitivity = 3.0f;
const float       kVehicleAhead = 6.0f;         // a named vehicle is made this far in front of the soldier

// The stock control maps' keys (mods/bf2/Settings/Controls.con, InfantryPlayerInputControlMap and
// defaultPlayerInputControlMap). `edge` rows fire on the step a key goes down (the maps' 10000 threshold), the others
// while it is held.
struct KeyRow
{
    int  input;
    int  key;
    bool edge;
};
const KeyRow kKeyRows[] = {
    { io::PIFire, 0, false },               // the mouse buttons, filled from PlayTestInput::buttons
    { io::PIAltFire, 0, false },
    { io::PIAction, VK_SPACE, true },
    { io::PISprint, VK_LSHIFT, false },
    { io::PIUse, 'E', false },
    { io::PIReload, 'R', true },
    { io::PIToggleCameraMode, 'C', true },
    { io::PICrouch, VK_LCONTROL, false },
    { io::PILie, 'Z', false },
    { io::PIDrop, 'G', true },
    { io::PIToggleWeapon, 'F', true },
    { io::PIFlareFire, 'X', false },
    { io::PIWeaponSelect1, '1', true }, { io::PIWeaponSelect2, '2', true }, { io::PIWeaponSelect3, '3', true },
    { io::PIWeaponSelect4, '4', true }, { io::PIWeaponSelect5, '5', true }, { io::PIWeaponSelect6, '6', true },
    { io::PIWeaponSelect7, '7', true }, { io::PIWeaponSelect8, '8', true }, { io::PIWeaponSelect9, '9', true },
    { io::PIPositionSelect1, VK_F1, true }, { io::PIPositionSelect2, VK_F2, true },
    { io::PIPositionSelect3, VK_F3, true }, { io::PIPositionSelect4, VK_F4, true },
    { io::PIPositionSelect5, VK_F5, true }, { io::PIPositionSelect6, VK_F6, true },
    { io::PIPositionSelect7, VK_F7, true }, { io::PIPositionSelect8, VK_F8, true },
    { io::PICameraMode1, VK_F9, true }, { io::PICameraMode2, VK_F10, true },
    { io::PICameraMode3, VK_F11, true }, { io::PICameraMode4, VK_F12, true },
};

// Ground below a point: the objects' collision meshes, the play-test's own soldier left out.
class GroundPredicator : public world::ObjectPredicator
{
public:
    explicit GroundPredicator(world::IObject* skip) : m_skip(skip) {}
    virtual bool predicate(world::IObject* object)
    {
        world::IObject* root = world::getRootParent(object);
        return (root ? root : object) != m_skip;
    }
    virtual unsigned int getFlags() const { return 0; }
    virtual bool performCulling(world::IObject*) { return true; }

private:
    world::IObject* m_skip;
};

Vec3 makeVec3(float x, float y, float z)
{
    Vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

void postGameStatus(int status)
{
    // The status a client learns from its server (ClientGameLogic::handleEvent, ECGameLogic id 1). At 1 (playing)
    // ClientGameLogic::update advances the world tick, and with it the world clock.
    if (eventManager)
        eventManager->postEvent(ECGameLogic, 1, &status, 0.0f);
}

} // namespace

PlayTestInput::PlayTestInput()
    : lookX(0.0f)
    , lookY(0.0f)
{
    for (int i = 0; i < 256; ++i)
        keys[i] = false;
    buttons[0] = buttons[1] = false;
}

PlayTest::PlayTest(EditSession& session, FreeCamera& camera, EditorGame& game)
    : m_session(session)
    , m_camera(camera)
    , m_game(game)
    , m_active(false)
    , m_playerId(-1)
    , m_team(1)
    , m_kit(2)
    , m_owed(0.0)
    , m_savedGameStatus(0)
    , m_lastToggle(-1000.0)
    , m_steps(0)
{
    for (int i = 0; i < 256; ++i)
        m_previousKeys[i] = false;
    m_savedViews[0] = m_savedViews[1] = 0;
}

PlayTest::~PlayTest()
{
}

world::IPlayer* PlayTest::player() const
{
    return m_playerId >= 0 && world::playerManager ? world::playerManager->getPlayerFromId(m_playerId) : 0;
}

world::IObject* PlayTest::soldier() const
{
    return m_soldier.get();
}

bool PlayTest::ensurePlayer(std::string& result)
{
    if (player())
        return true;
    if (!world::playerManager || !world::objectTemplateManager || !gameLogic)
    {
        result = "no level is open";
        return false;
    }
    // A player made after the level load takes its free camera from MultiPlayerFreeCamera (GameLogicInit.con) in its
    // constructor; without one, Player::allocateFreeCamera makes none and a soldier could never be entered.
    if (!world::objectTemplateManager->getTemplate(std::string("MultiPlayerFreeCamera")))
    {
        result = "the level has no MultiPlayerFreeCamera template (GameLogicInit.con), so a player has no camera to start in";
        return false;
    }
    world::IPlayer* made = world::playerManager->getPlayerFromName(kPlayerName);
    if (!made)
        made = world::playerManager->createPlayer(world::CID_Player, std::string(kPlayerName), kPlayerId);
    if (!made)
    {
        result = "PlayerManager made no player (id 255 or the name EditorPlayer is taken)";
        return false;
    }
    made->setIsAIPlayer(false);
    made->setIsRemote(false);
    made->setIsConnected(true);
    world::playerManager->setLocalHumanPlayer(made);
    if (!made->getFreeCamera() || made->getVehicle() != made->getFreeCamera())
    {
        result = "the player was made without its free camera";
        return false;
    }
    m_playerId = made->getId();
    logLine(LogLevel::Info, "play-test: the local player %s (id %d) is up, in its free camera", kPlayerName, m_playerId);
    return true;
}

bool PlayTest::groundBelow(const float at[3], float& height) const
{
    float terrain = 0.0f;
    const bool onTerrain = m_session.groundHeight(at[0], at[2], terrain);
    height = onTerrain ? terrain : -1.0e9f;
    if (world::objectManager)
    {
        GroundPredicator predicator(soldier());
        world::IObject* hit = 0;
        Vec3 point = makeVec3(0.0f, 0.0f, 0.0f);
        Vec3 normal = makeVec3(0.0f, 0.0f, 0.0f);
        int part = 0;
        const Vec3 from = makeVec3(at[0], at[1], at[2]);
        const Vec3 delta = makeVec3(0.0f, -1000.0f, 0.0f);
        if (world::objectManager->intersectLine(hit, point, normal, part, from, delta, &predicator, true, false, false,
                                                false, 0xFFFFFFFFu)
            && hit && point.y > height)
            height = point.y;
    }
    return height > -1.0e8f;
}

bool PlayTest::setKit(int team, int kit, std::string& result)
{
    if (team < 1 || team > 2 || kit < 0 || kit > 6)
    {
        result = "a kit is a team (1 or 2) and a kit number (0 special ops ... 6 anti-tank)";
        return false;
    }
    m_team = team;
    m_kit = kit;
    char text[96];
    std::snprintf(text, sizeof(text), "the next soldier is team %d, kit %d", team, kit);
    result = text;
    return true;
}

bool PlayTest::enterSoldier(std::string& result)
{
    if (m_active)
    {
        result = "soldier mode is on already";
        return false;
    }
    if (!ensurePlayer(result))
        return false;
    world::IPlayer* const local = player();

    // The soldier and the kit the level gives the team (gameLogic.setKit), else the 1.50 fallback body.
    const std::string skin = gameLogic->getSoldierSkin(m_team, m_kit);
    const std::string kitName = gameLogic->getKitTemplateString(m_team, m_kit);
    const world::IObjectTemplate* soldierTemplate =
        world::objectTemplateManager->getTemplate(skin.empty() ? std::string("us_light_soldier") : skin);
    if (!soldierTemplate)
    {
        result = "no soldier template " + (skin.empty() ? std::string("us_light_soldier") : skin);
        return false;
    }
    world::IObjectTemplate* kitOwner = kitName.empty() || m_noKit ? 0 : world::objectTemplateManager->getTemplate(kitName);
    void* const kitTemplate = kitOwner ? kitOwner->queryImplementation(world::CID_KitTemplate) : 0;
    if (!kitTemplate && !m_noKit)
    {
        char text[160];
        std::snprintf(text, sizeof(text), "team %d has no kit %d in this level (gameLogic.setKit)%s%s", m_team, m_kit,
                      kitName.empty() ? "" : ": ", kitName.c_str());
        result = text;
        return false;
    }

    // Where the editor camera stands, on the ground below it, facing its way.
    const float eye[3] = { m_camera.x(), m_camera.y(), m_camera.z() };
    float ground = 0.0f;
    if (!groundBelow(eye, ground))
    {
        result = "there is no ground below the camera";
        return false;
    }
    const Vec3 position = makeVec3(eye[0], ground + kAboveGround, eye[2]);
    // setRotationZXY's yaw turns +z towards -x, the editor camera's towards +x; createObjectOnAllClients negates.
    const Vec3 rotation = makeVec3(m_camera.yawDegrees(), 0.0f, 0.0f);

    m_savedGameStatus = gameLogic->getGameStatus();
    if (m_savedGameStatus != 1)
        postGameStatus(1);
    // The external and nose views are server settings a client learns from its server (ServerInfo's block, which
    // Camera::setViewMode reads); the editor is the server here, and lets the play-test use every view.
    if (serverInfo)
    {
        m_savedViews[0] = serverInfo->m_block.allowExternalViews;
        m_savedViews[1] = serverInfo->m_block.allowNoseCam;
        serverInfo->m_block.allowExternalViews = 1;
        serverInfo->m_block.allowNoseCam = 1;
    }

    world::IObject* const body = m_game.createObjectOnAllClients(soldierTemplate, position, rotation, true, 0, m_team);
    if (!body)
    {
        if (m_savedGameStatus != 1)
            postGameStatus(m_savedGameStatus);
        result = "the engine made no " + soldierTemplate->getName();
        return false;
    }
    body->setName(std::string(kPlayerName) + soldierTemplate->getName());
    m_soldier = body->getWeakPtr();
    local->setTeam(m_team, false);
    local->setKit(m_kit);
    local->setFreeCameraEnabled(false);
    if (!m_game.enterVehicle(local, body, true))
    {
        destroyMadeObjects();
        if (m_savedGameStatus != 1)
            postGameStatus(m_savedGameStatus);
        result = "the player could not enter the soldier";
        return false;
    }
    local->setIsAlive(true);

    world::IObject* const kitObject = !kitTemplate ? 0 : m_game.createObjectOnAllClients(
        static_cast<const world::IObjectTemplate*>(kitTemplate), position, rotation, true, 0, 0);
    if (kitObject)
    {
        m_kitObject = kitObject->getWeakPtr();
        gameLogic->handlePickup(kitObject, local, local->getVehicle(), true);
    }

    m_active = true;
    m_owed = 0.0;
    m_held.clear();
    m_lastToggle = -1000.0;
    char text[256];
    std::snprintf(text, sizeof(text), "soldier mode: %s with %s at %.1f/%.1f/%.1f (team %d, kit %d)",
                  soldierTemplate->getName().c_str(), kitObject ? kitName.c_str() : "no kit", position.x, position.y,
                  position.z, m_team, m_kit);
    result = text;
    logLine(LogLevel::Info, "play-test: %s", text);
    return true;
}

void PlayTest::destroyMadeObjects()
{
    std::vector<world::IObject*> made;
    for (size_t i = 0; i < m_vehicles.size(); ++i)
        made.push_back(m_vehicles[i].get());
    made.push_back(m_kitObject.get());
    made.push_back(m_soldier.get());
    m_vehicles.clear();
    m_kitObject = WeakPtr<world::IObject>();
    m_soldier = WeakPtr<world::IObject>();
    for (size_t i = 0; i < made.size(); ++i)
    {
        if (made[i] && !made[i]->isObjectDestroyed())
            made[i]->forceDestroy();
    }
    if (world::objectManager)
        world::objectManager->destroyObjects();
}

bool PlayTest::leave(std::string& result)
{
    if (!m_active)
    {
        result = "soldier mode is off";
        return true;
    }
    world::IPlayer* const local = player();
    float eye[3] = { m_camera.x(), m_camera.y(), m_camera.z() };
    float yaw = m_camera.yawDegrees();
    float pitch = m_camera.pitchDegrees();
    if (local && local->getCamera())
    {
        const Mat4 m = local->getCamera()->getTransformation(0.0f);
        eye[0] = m.m[12];
        eye[1] = m.m[13];
        eye[2] = m.m[14];
        yaw = std::atan2(m.m[8], m.m[10]) * 57.29578f;
        pitch = std::atan2(m.m[9], std::sqrt(m.m[8] * m.m[8] + m.m[10] * m.m[10])) * 57.29578f;
    }
    if (local)
    {
        // Out of every seat down to the soldier, then out of the soldier into the free camera (Player::setVehicle's
        // null path seats the player there), dead and the free camera on, as before soldier mode.
        for (int guard = 0; guard < 8 && local->getVehicle() && local->getVehicle() != local->getDefaultVehicle(); ++guard)
            m_game.exitVehicle(local, false, false);
        local->setIsAlive(false);
        if (local->getVehicle() && local->getVehicle() != local->getFreeCamera())
            m_game.exitVehicle(local, false, false);
        if (local->getVehicle() != local->getFreeCamera())
            local->setVehicle(0, -1);
        local->setDefaultVehicle(0);
        local->setFreeCameraEnabled(true);
    }
    destroyMadeObjects();
    if (gameLogic && m_savedGameStatus != 1)
        postGameStatus(m_savedGameStatus);
    if (serverInfo)
    {
        serverInfo->m_block.allowExternalViews = m_savedViews[0];
        serverInfo->m_block.allowNoseCam = m_savedViews[1];
    }
    m_active = false;
    m_noKit = false;
    m_held.clear();
    m_camera.place(eye[0], eye[1], eye[2], yaw, pitch);
    char text[160];
    std::snprintf(text, sizeof(text), "soldier mode off after %u steps; the camera is at %.1f/%.1f/%.1f", m_steps, eye[0],
                  eye[1], eye[2]);
    result = text;
    logLine(LogLevel::Info, "play-test: %s", text);
    m_steps = 0;
    return true;
}

void PlayTest::shutdown()
{
    std::string ignored;
    if (m_active)
        leave(ignored);
    world::IPlayer* const local = player();
    m_playerId = -1;
    if (!local || !world::playerManager)
        return;
    // GameServer::destroyPlayer's order, without the network and the scripts: the free camera, the "player
    // destroyed" event, the player out of the manager, deleted.
    if (world::IObject* const freeCamera = local->getFreeCamera())
    {
        freeCamera->forceDestroy();
        if (world::objectManager)
            world::objectManager->destroyObjects();
    }
    eventManager->postEvent(ECGame, 2, local, 0.0f);
    if (world::playerManager->getLocalHumanPlayer() == local)
        world::playerManager->setLocalHumanPlayer(static_cast<world::IPlayer*>(0));
    world::playerManager->removePlayer(local);
    delete local;
    logLine(LogLevel::Info, "play-test: the local player is removed");
}

bool PlayTest::drive(const std::string& templateName, std::string& result)
{
    if (!world::objectTemplateManager || !world::objectManager || !gameLogic)
    {
        result = "no level is open";
        return false;
    }
    // The vehicle: the template named, or the selected spawner's vehicle for the soldier's team (else the other's).
    std::string name = templateName;
    world::IObject* spawner = 0;
    if (name.empty())
    {
        world::IObject* const selected = EditSession::object(m_session.selection().primary());
        world::IObject* const root = selected ? world::getRootParent(selected) : 0;
        spawner = root ? root : selected;
        const world::IObjectTemplate* const spawnerTemplate = spawner ? spawner->getTemplate() : 0;
        if (!spawnerTemplate || spawnerTemplate->getClassID() != world::CID_ObjectSpawnerTemplate)
        {
            result = "select an object spawner to drive its vehicle (or EditorPlayTest.driveTemplate <template>)";
            return false;
        }
        const world::ObjectSpawnerTemplate* const spawns =
            static_cast<const world::ObjectSpawnerTemplate*>(spawnerTemplate);
        name = spawns->getObjectTemplate(static_cast<unsigned int>(m_team));
        if (name.empty())
            name = spawns->getObjectTemplate(static_cast<unsigned int>(3 - m_team));
        if (name.empty())
        {
            result = "the spawner " + spawnerTemplate->getName() + " names no vehicle for either team";
            return false;
        }
    }
    const world::IObjectTemplate* const vehicleTemplate = world::objectTemplateManager->getTemplate(name);
    if (!vehicleTemplate)
    {
        result = "no template named " + name;
        return false;
    }
    if (!vehicleTemplate->queryInterface(world::IID_IPlayerControlObjectTemplate)
        || vehicleTemplate->getClassID() == world::CID_SoldierTemplate)
    {
        result = name + " is not a vehicle (no player control object)";
        return false;
    }
    if (!m_active && !enterSoldier(result))
        return false;
    world::IPlayer* const local = player();
    world::IObject* const body = soldier();
    if (!local || !body)
    {
        result = "soldier mode has no soldier";
        return false;
    }

    // Where: the spawner's place and heading, or in front of the soldier on the ground, facing its way.
    Vec3 position, rotation;
    if (spawner)
    {
        const Mat4& m = spawner->getAbsoluteTransformation();
        float zDegrees = 0.0f, xDegrees = 0.0f, yDegrees = 0.0f;
        getRotationZXY(m, zDegrees, xDegrees, yDegrees);
        position = makeVec3(m.m[12], m.m[13] + 0.5f, m.m[14]);
        rotation = makeVec3(-yDegrees, -xDegrees, -zDegrees);
    }
    else
    {
        const Mat4& m = body->getAbsoluteTransformation();
        float at[3] = { m.m[12] + m.m[8] * kVehicleAhead, m.m[13] + 3.0f, m.m[14] + m.m[10] * kVehicleAhead };
        float ground = 0.0f;
        if (!groundBelow(at, ground))
            ground = m.m[13];
        position = makeVec3(at[0], ground + 1.0f, at[2]);
        rotation = makeVec3(std::atan2(-m.m[8], m.m[10]) * -57.29578f, 0.0f, 0.0f);
    }
    world::IObject* const vehicle = m_game.createObjectOnAllClients(vehicleTemplate, position, rotation, true, 0, m_team);
    if (!vehicle)
    {
        result = "the engine made no " + name;
        return false;
    }
    m_vehicles.push_back(vehicle->getWeakPtr());

    // Out of any seat first, then into the new vehicle's driver seat.
    if (local->getVehicle() && local->getVehicle() != local->getDefaultVehicle())
        m_game.exitVehicle(local, true, true);
    if (!m_game.enterVehicle(local, vehicle, false))
    {
        result = "the player could not enter " + name;
        return false;
    }
    m_lastToggle = WorldPref::getWorldTime();
    char text[200];
    std::snprintf(text, sizeof(text), "driving %s at %.1f/%.1f/%.1f%s%s", name.c_str(), position.x, position.y,
                  position.z, spawner ? " (the spawner " : "",
                  spawner ? (spawner->getTemplate()->getName() + ")").c_str() : "");
    result = text;
    logLine(LogLevel::Info, "play-test: %s", text);
    return true;
}

int PlayTest::controlCategory() const
{
    world::IPlayer* const local = player();
    world::IObject* const vehicle = local ? local->getVehicle() : 0;
    if (!vehicle || vehicle->getTemplate()->getClassID() == world::CID_SoldierTemplate)
        return 0;
    world::IPlayerControlObject* const pco = world::findOwningPlayerControlObject(vehicle);
    if (!pco || pco->isChildSoldierActive())
        return 0;
    const world::IPlayerControlObjectTemplate* const pcoTemplate =
        static_cast<const world::IPlayerControlObjectTemplate*>(
            vehicle->getTemplate()->queryInterface(world::IID_IPlayerControlObjectTemplate));
    if (!pcoTemplate)
        return 1;
    const int category = pcoTemplate->getControlsCategory() == 4 ? pcoTemplate->getVehicleCategory()
                                                                   : pcoTemplate->getControlsCategory();
    return category == 2 ? 2 : (category == 3 ? 3 : 1);
}

bool PlayTest::testWeapon(const std::string& templateName, std::string& result)
{
    world::IObjectTemplate* const weapon =
        world::objectTemplateManager ? world::objectTemplateManager->getTemplate(templateName) : 0;
    if (!weapon)
    {
        result = "no template named " + templateName;
        return false;
    }
    // Soldier::addItem by name does not check that the template makes an item (its queryInterface is unchecked).
    if (weapon->getClassID() != world::CID_GenericFireArmTemplate)
    {
        result = weapon->getName() + " is not a weapon (a GenericFireArm template)";
        return false;
    }
    std::string left;
    if (m_active)
        leave(left);
    m_noKit = true;
    if (!enterSoldier(result))
    {
        m_noKit = false;
        return false;
    }
    world::IObject* const body = soldier();
    world::ISoldier* const asSoldier =
        body ? static_cast<world::ISoldier*>(body->queryInterface(world::IID_ISoldier)) : 0;
    const int slot = asSoldier ? asSoldier->addItem(weapon->getName(), -1) : -1;
    if (slot <= 0)
    {
        result = "the soldier did not take " + weapon->getName() + " (Soldier::addItem answered " + std::to_string(slot) + ")";
        leave(left);
        return false;
    }
    // Drawn as a player draws it: the slot's key for two steps.
    if (slot <= 9)
    {
        Held held;
        held.input = static_cast<int>(io::PIWeaponSelect1) + slot - 1;
        held.value = 1.0f;
        held.steps = 2;
        m_held.push_back(held);
    }
    char text[200];
    std::snprintf(text, sizeof(text), "testing %s: the soldier holds it in slot %d (fire with the left button, R reloads; "
                  "a rate of fire or reload time set in the template reaches it at once)", weapon->getName().c_str(), slot);
    result = text;
    logLine(LogLevel::Info, "play-test: %s", text);
    return true;
}

bool PlayTest::hold(const std::string& name, float value, int steps, std::string& result)
{
    std::istringstream stream(name);
    io::PlayerInputMap input = io::PINone;
    stream >> input;
    if (input == io::PINone)
    {
        result = "no player input named " + name + " (PIThrottle, PIYaw, PIMouseLookX, PIUse ...)";
        return false;
    }
    if (steps < 1 || steps > 3000)
    {
        result = "an input is held for 1 to 3000 steps (30 a second)";
        return false;
    }
    Held held;
    held.input = static_cast<int>(input);
    held.value = value;
    held.steps = steps;
    m_held.push_back(held);
    char text[96];
    std::snprintf(text, sizeof(text), "%s held at %g for %d steps", name.c_str(), value, steps);
    result = text;
    return true;
}

std::string PlayTest::describe() const
{
    world::IPlayer* const local = player();
    if (!local)
        return "no play-test player";
    world::IObject* const vehicle = local->getVehicle();
    char text[320];
    if (!vehicle)
    {
        std::snprintf(text, sizeof(text), "%s (id %d): in nothing, %s", local->getName().c_str(), local->getId(),
                      m_active ? "soldier mode on" : "soldier mode off");
        return text;
    }
    const Vec3& p = vehicle->getAbsolutePosition();
    const world::ICameraObject* const camera = local->getCamera();
    std::snprintf(text, sizeof(text),
                  "%s (id %d): in %s at %.2f/%.2f/%.2f, %s, %s, camera %s, step %u, tick %d, team %d kit %d, view %d",
                  local->getName().c_str(), local->getId(), vehicle->getTemplate()->getName().c_str(), p.x, p.y, p.z,
                  local->getIsAlive() ? "alive" : "not alive", m_active ? "soldier mode on" : "soldier mode off",
                  camera ? "yes" : "none", m_steps, WorldPref::getTickCount(), local->getTeam(), local->getKit(),
                  camera ? static_cast<int>(const_cast<world::ICameraObject*>(camera)->getViewMode()) : -1);
    // The weapon in the soldier's hands: its rounds, magazines and reload.
    std::string line = text;
    world::ISoldier* const asSoldier = static_cast<world::ISoldier*>(vehicle->queryInterface(world::IID_ISoldier));
    world::IObject* const held = asSoldier ? asSoldier->getActiveWeapon(0) : 0;
    world::IWeaponObject* const weapon =
        held ? static_cast<world::IWeaponObject*>(held->queryInterface(world::IID_IWeaponObject)) : 0;
    if (weapon)
    {
        char more[200];
        std::snprintf(more, sizeof(more), ", holding %s: %d of %d rounds, %d magazines%s", held->getTemplate()->getName().c_str(),
                      weapon->getAmmoInMag(0), weapon->getMagSize(), weapon->getCurrentNrOfMags(),
                      weapon->isReloading() ? ", reloading" : "");
        line += more;
    }
    return line;
}

void PlayTest::frame(float seconds, const PlayTestInput& input)
{
    if (!m_active)
        return;
    if (!player())
    {
        logLine(LogLevel::Warning, "play-test: the player is gone; soldier mode ends");
        m_active = false;
        return;
    }
    // A long frame (a load, a breakpoint) owes at most a quarter second.
    m_owed += seconds > 0.25f ? 0.25f : (seconds > 0.0f ? seconds : 0.0f);
    bool first = true;
    while (m_owed >= kStep && m_active)
    {
        m_owed -= kStep;
        // The look is this frame's mouse movement: it goes to the first step only.
        PlayTestInput stepInput = input;
        if (!first)
            stepInput.lookX = stepInput.lookY = 0.0f;
        first = false;
        step(kStep, stepInput);
    }
}

void PlayTest::buildInput(const PlayTestInput& source, io::PlayerInput& out)
{
    for (int i = 0; i < 64; ++i)
        out.inputs[i] = 0.0f;
    out.mappingFlags = 0;
    out.dword0 = 0;
    out.dword1 = 0;
    out.flag = 0;

    // Bound inputs only: the soldier reads an input only when its mapping bit is set (Soldier.cpp mappedInput).
    out.setInputIsMapped(io::PIYaw, true);
    out.setInputIsMapped(io::PIThrottle, true);
    out.setInputIsMapped(io::PIMouseLookX, true);
    out.setInputIsMapped(io::PIMouseLookY, true);
    for (size_t i = 0; i < sizeof(kKeyRows) / sizeof(kKeyRows[0]); ++i)
        out.setInputIsMapped(static_cast<io::PlayerInputMap>(kKeyRows[i].input), true);

    const bool* const k = source.keys;
    out.setInput(io::PIYaw, (k['D'] ? 1.0f : 0.0f) - (k['A'] ? 1.0f : 0.0f));
    out.setInput(io::PIThrottle, (k['W'] ? 1.0f : 0.0f) - (k['S'] ? 1.0f : 0.0f));
    // The flying maps put the mouse on the stick (PIRoll / PIPitch) unless Left Ctrl (PIMouseLook) frees the view.
    const int category = controlCategory();
    const bool flying = category == 2 || category == 3;
    if (flying && !k[VK_LCONTROL])
    {
        const float sensitivity = category == 2 ? kAirMouseSensitivity : kHelicopterMouseSensitivity;
        out.setInputIsMapped(io::PIRoll, true);
        out.setInputIsMapped(io::PIPitch, true);
        out.setInput(io::PIRoll, source.lookX * kMouseAxisScale * sensitivity);
        out.setInput(io::PIPitch, source.lookY * kMouseAxisScale * sensitivity);
    }
    else
    {
        out.setInput(io::PIMouseLookX, source.lookX * kMouseAxisScale);
        out.setInput(io::PIMouseLookY, source.lookY * kMouseAxisScale);
    }
    if (flying)
    {
        out.setInputIsMapped(io::PIMouseLook, true);
        if (k[VK_LCONTROL])
            out.setInput(io::PIMouseLook, 1.0f);
    }
    for (size_t i = 0; i < sizeof(kKeyRows) / sizeof(kKeyRows[0]); ++i)
    {
        const KeyRow& row = kKeyRows[i];
        bool down = false;
        if (row.input == io::PIFire)
            down = source.buttons[0];
        else if (row.input == io::PIAltFire)
            down = source.buttons[1];
        else
            down = k[row.key & 0xFF];
        const bool fire = row.edge ? (down && !m_previousKeys[row.key & 0xFF]) : down;
        if (fire)
            out.setInput(static_cast<io::PlayerInputMap>(row.input), 1.0f);
    }
    for (int i = 0; i < 256; ++i)
        m_previousKeys[i] = k[i];

    // Scripted inputs (EditorPlayTest.hold) win over the keys.
    for (std::vector<Held>::iterator it = m_held.begin(); it != m_held.end();)
    {
        out.setInputIsMapped(static_cast<io::PlayerInputMap>(it->input), true);
        out.setInput(static_cast<io::PlayerInputMap>(it->input), it->value);
        if (--it->steps <= 0)
            it = m_held.erase(it);
        else
            ++it;
    }
}

// The editor's E key: ServerGameLogic::toggleEntryPoint, which the client's logic leaves to its server - the seat
// left when the player is in one, else the nearest entry point within 3 m entered, at most once a second.
void PlayTest::toggleEntry(bool usePressed)
{
    world::IPlayer* const local = player();
    if (!usePressed || !local || !local->getIsAlive())
        return;
    const double now = WorldPref::getWorldTime();
    if (now - m_lastToggle < 1.0)
        return;
    world::IObject* vehicle = local->getVehicle();
    if (!vehicle || vehicle == local->getFreeCamera())
        return;
    world::IPlayerControlObject* const pco = static_cast<world::IPlayerControlObject*>(
        vehicle->queryInterface(world::IID_IPlayerControlObject));
    if (!pco)
        return;
    const bool inVehicle = vehicle != local->getDefaultVehicle();
    const bool proceed = !inVehicle || pco->allowEnterOtherVehicle();
    if (inVehicle)
    {
        if (!pco->isAllowedToExit())
        {
            m_game.failedExitVehicle(local->getId());
            return;
        }
        if (m_game.exitVehicle(local, true, true))
        {
            m_lastToggle = now;
            logLine(LogLevel::Info, "play-test: out of %s", vehicle->getTemplate()->getName().c_str());
            return;
        }
    }
    if (!proceed)
        return;
    const Vec3& at = vehicle->getAbsolutePosition();
    world::IPlayerControlObject* const entry =
        world::BFfindEntryPoint(at, 3.0f, local->getTeam(), local, true, true, true);
    if (!entry)
        return;
    world::IObject* const target = static_cast<world::IObject*>(entry->queryInterface(world::IID_IObject));
    if (target && m_game.enterVehicle(local, target, false))
    {
        m_lastToggle = now;
        world::IObject* const root = world::getRootParent(target);
        logLine(LogLevel::Info, "play-test: into %s", (root ? root : target)->getTemplate()->getName().c_str());
    }
}

// The seat keys (F1-F8) and the camera keys (C, F9-F12): ServerGameLogic::checkPlayerTriggers' blocks 9 and 10,
// which the client's logic leaves to its server. A free seat is taken the server's way (out of the old one, into the
// new); the swap with a bot, the "seat taken" events and the weapon-held block are left out (no bots, no clients). The
// external and nose views (server settings) are allowed for the run (enterSoldier).
void PlayTest::seatAndCameraKeys(const io::PlayerInput& input)
{
    world::IPlayer* const local = player();
    world::IObject* const vehicle = local ? local->getVehicle() : 0;
    if (!vehicle)
        return;
    int seat = 0;
    for (int i = 0; i < 8 && !seat; ++i)
    {
        if (input.inputs[io::PIPositionSelect1 + i] != 0.0f)
            seat = i + 1;
    }
    if (seat)
    {
        const int requested = seat - 1;
        world::IPlayerControlObject* const rootPco = world::findOwningPlayerControlObject(world::getRootParent(vehicle));
        world::IPlayerControlObject* const currentPco = world::findOwningPlayerControlObject(vehicle);
        if (rootPco && currentPco && currentPco->getPcoId() != requested)
        {
            std::map<int, world::IPlayerControlObject*>* const pcos = rootPco->getPcos();
            const std::vector<int>* const allowed = currentPco->getPcoPosId();
            const bool permitted = !allowed || allowed->empty()
                                || std::find(allowed->begin(), allowed->end(), requested) != allowed->end();
            if (permitted && pcos)
            {
                std::map<int, world::IPlayerControlObject*>::iterator it = pcos->find(requested);
                if (it != pcos->end() && it->second && !it->second->isOccupied())
                {
                    world::IObject* const target =
                        static_cast<world::IObject*>(it->second->queryInterface(world::IID_IObject));
                    m_game.exitVehicle(local, true, true);
                    if (target && m_game.enterVehicle(local, target, false))
                        logLine(LogLevel::Info, "play-test: seat %d (%s)", seat, target->getTemplate()->getName().c_str());
                }
            }
        }
    }

    if (local->getVehicle() == local->getDefaultVehicle() || !local->getIsAlive() || local->getIsManDown())
        return;
    world::ICameraObject* const camera = local->getCamera();
    if (!camera)
        return;
    if (input.inputs[io::PIToggleCameraMode] != 0.0f && local->getVehicle() != local->getFreeCamera())
        world::toggleViewMode(camera, true);
    if (input.inputs[io::PICameraMode1] != 0.0f)
    {
        // F9 again in the first-person view is the nose camera, where the camera has one (the server's rule).
        const bool nose = camera->getViewMode() == 1 && serverInfo && serverInfo->m_block.allowNoseCam != 0;
        camera->setViewMode(static_cast<world::CameraViewMode>(nose ? 2 : 1), 0, false);
    }
    if (input.inputs[io::PICameraMode2] != 0.0f)
        camera->setViewMode(static_cast<world::CameraViewMode>(3), 0, false);
    if (input.inputs[io::PICameraMode3] != 0.0f)
        camera->setViewMode(static_cast<world::CameraViewMode>(4), 0, false);
    if (input.inputs[io::PICameraMode4] != 0.0f)
        camera->setViewMode(static_cast<world::CameraViewMode>(5), 0, false);
}

// GameClient::stepWorld's order (GameClient.cpp), without the network: no remote players are replayed, no remote
// objects predicted, no action is queued for a server.
void PlayTest::step(float dt, const PlayTestInput& source)
{
    world::IPlayer* const local = player();
    if (!local)
        return;
    ++m_steps;
    eventManager->processEvents(dt);
    m_game.updateTriggerManager(dt);
    gameLogic->update(dt);

    // sampleLocalInput
    io::PlayerInput input;
    buildInput(source, input);
    local->getAutoInput(input, false);
    if (world::IObject* const vehicle = local->getVehicle())
    {
        if (void* const pco = vehicle->queryInterface(world::IID_IPlayerControlObject))
            static_cast<world::IPlayerControlObject*>(pco)->preHandlePlayerInput(input, dt);
    }
    PlayerAction action;
    action.set(input);
    action.get(input);
    if (gameLogic->getGameStatus() != 4)
    {
        toggleEntry(input.inputs[io::PIUse] != 0.0f);
        gameLogic->checkPlayerTriggers(local, local->getVehicle(), input, false);
        seatAndCameraKeys(input);
        local->handleInput(input, dt, 0);
    }
    performHandleUpdate(dt, local->getVehicle(), 0, false);
    local->getAutoInput(input, true);

    world::objectManager->updateObjects(dt, static_cast<world::UpdateFreqencyType>(0), 3);
    performMobilePhysicsUpdate(dt, local->getVehicle(), false);
    world::physicsNodeManager->update(dt, 0, 0);
    if (local->getVehicle() && local->getVehicle()->getParent())
        performYawStabilisation(local->getVehicle());

    world::responsePhysicsManager->resetCachedCollisionObjects();
    m_game.updateWorldCollision(dt);
    world::responsePhysicsManager->resetCachedCollisionObjects();

    // stepLocalPrediction
    world::objectManager->updateObjectsInGrid();
    world::responsePhysicsManager->reset();
    performMobileCollisionUpdate(dt, local->getVehicle(), false);

    world::physicsNodeManager->postFrameUpdate();
    world::responsePhysicsManager->postFrameUpdate();
}

bool PlayTest::applyView(dx9rend::IRenderView* view, int width, int height, float farPlane, float frameTime) const
{
    world::IPlayer* const local = player();
    if (!m_active || !view || !local || !local->getCamera() || width <= 0 || height <= 0)
        return false;
    const Mat4 transform(local->getCamera()->getTransformation(frameTime));
    view->setViewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    view->setAdjustedFov(local->getFov());
    view->setAspect(static_cast<float>(height) / static_cast<float>(width));
    view->setFarPlane(farPlane);
    view->setViewMatrix(transform.m);
    return true;
}

} // namespace bf2editor
