// HUD vehicle information. [INFERRED] Class and source placement.
#include "HudVehicleInformation.h"
#if !DEDICATED
#include "HudBlinker.h"
#include "BF2HudManager.h"
#include "HudInformationAdapter.h"
#include "BfOccupiedVehicleData.h"
#include "Bf2MenuMapNode.h"
#include "HudVehiclePostProcessing.h"
#include "../Core/ClassManager.h"
#include "../Core/Ids.h"
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/Objects/IPostProcessComp.h"
#include "../Game/Objects/IVehicleHudComp.h"
#include "../Game/Objects/IWeaponHudComp.h"
#include "../Game/Objects/WarningHudCompTemplate.h"
#include "../Game/Objects/WeaponHudCompTemplate.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/System.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#include "../Core/System.h"                // WorldPref::getTickCount, PE 4C4420
#include "../Game/HudInformationLayer.h"
#include "../Game/IGameLogic.h"
#include "../Game/IGame.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Objects/Game/Kit.h"
#include "../Game/Objects/IAbilityHudComp.h"
#include "../Game/Objects/ICameraObject.h"
#include "../Game/Objects/IObject.h"          // getRootParent, the component fields
#include "../Game/Objects/IObjectTemplate.h"  // getClassID, for the ladder-container test
#include "../Game/Objects/IPlayer.h"
#include "../Scene/Object/PlayerManager.h"
#include <cwchar>                           // wcscmp / wcslen - 78A740
#include <new>
#include <cstring>
#include <cwchar>
#include <windows.h>
#if !defined(_M_IX86)
#include <cmath>
#endif

namespace dice { namespace hfe { extern ClassManager* classManager; extern IBase* squadManager; } }
namespace dice { namespace hfe { namespace hud {
char HudVehicleInformation::s_emptyString;
wchar_t HudVehicleInformation::s_emptyWideString;

void HudVehicleInformation::assignString(char*& destination, const char* source)
{
    if (source && std::strcmp(source, destination)) {
        if (destination && destination != &s_emptyString) {
            ::operator delete(destination);
            destination = nullptr;
        }
        const std::size_t size = std::strlen(source) + 1;
        destination = static_cast<char*>(::operator new(size));
        std::memcpy(destination, source, size);
    }
}

// Replaces a wide string when its contents differ, preserving the shared empty-string sentinel.
// Requires a nonnull destination because comparison precedes its null check.
void HudVehicleInformation::assignString(wchar_t*& destination, const wchar_t* source)
{
    if (source && std::wcscmp(source, destination)) {
        if (destination && destination != &s_emptyWideString) {
            ::operator delete(destination);
            destination = nullptr;
        }
        const std::size_t count = std::wcslen(source) + 1;
        destination = static_cast<wchar_t*>(::operator new(count * sizeof(wchar_t)));
        std::memcpy(destination, source, count * sizeof(wchar_t));
    }
}

void HudVehicleInformation::refreshVehicle()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player && player->getVehicle()) {
        world::IObject* defaultVehicle = player->getDefaultVehicle();
        if (player->getVehicle() != defaultVehicle) setupVehicle(player->getVehicle());
    }
}

bool HudVehicleInformation::canSelectWeapon(bool checkVehicle)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return false;
    world::IObject* vehicle = player->getVehicle();
    if (!vehicle || (game && game->isPlayingDemo())) return false;
    if (vehicle->m_vehicleHudComponent && checkVehicle
        && static_cast<world::IVehicleHudComp*>(vehicle->m_vehicleHudComponent)->getHudInfo()->useSelectionIcons)
        return true;
    world::IPlayerControlObject* control = nullptr;
    while (vehicle) {
        control = static_cast<world::IPlayerControlObject*>(vehicle->queryInterface(world::IID_IPlayerControlObject));
        if (control) break;
        vehicle = vehicle->getParent();
    }
    if (!control) return false;
    for (int iteration = 0; iteration < 2; ++iteration) {
        // Retail repeats channel zero twice; the loop counter is not the argument.
        world::IObject* weapon = control->getActiveWeapon(0);
        if (weapon && weapon->m_weaponHudComponent
            && static_cast<world::IWeaponHudComp*>(weapon->m_weaponHudComponent)
                   ->getHudInfo()->displaySelectOnActivation)
            return true;
    }
    return false;
}

void HudVehicleInformation::handleEvent(EventCategory category, unsigned int id,
                                       EventNode* node, void* data)
{
    if (category == ECGameLogic) {
        world::playerManager->getLocalHumanPlayer(); // Discarded read occurs for every id.
        if (id == 5) {
            refreshVehicle();
            m_field020 = true;
            m_field01c = 5.0f;
            m_field018 = 0.0f;
        }
    } else if (category == ECGame) handleGameEvent(id, node, data);
    else if (category == ECHud) handleHudEvent(id, node, data);
}

// Vehicle HUD event handlers.

// the local-player lookup is unconditional and precedes the switch, so it runs for all eighty ids
// the jump table covers (2..81) including the seventy that do nothing. Ids 57/58 and 69 dereference
// `data` with no null test, and 69 dispatches on the local player with no null test. Both missing
// guards are retail and are preserved.
void HudVehicleInformation::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    switch (id) {
    case 2:                                                 // ECHudHEHudStateChanged
        applyHudState(hudManager->getMode());
        return;
    case 3:                                                 // ECHudHEHudDisable
        reset(false);
        return;
    case 31:                                                // ECHudHEUpdateHud
        updateGuiIndex();
        return;
    case 44:                                                // ECHudHEMissileLock
        // The server's PostRemoteEvent(ECHud, 44) from sendMissileMessageToOccupyingPlayers.
        handleMissileLockEvent(local, data);
        return;
    case 57:                                                // ECHudHEIsCloseToMine
    case 58: {                                              // ECHudHENotCloseToMine
        // Player::updateIsCloseToMine and emptyMineMap post the player's getId().
        world::IPlayer* const source =
            world::playerManager->getPlayerFromId(*static_cast<int*>(data));
        if (!source || !local || source != local) return;
        // Retail spells the two arms separately, so exactly one adapter query happens either way.
        if (id == 57) hudManager->getInformationAdapter()->setCloseToMine(true);
        else          hudManager->getInformationAdapter()->setCloseToMine(false);
        return;
    }
    case 69: {                                              // Soldier::dropKit, local player only
        const int droppedBy = *static_cast<int*>(data);
        if (droppedBy == -1) return;
        if (droppedBy != local->getId()) return;             // no null test on `local` in retail
        setCloseToMedic(false);
        setCloseToRepair(false);
        setCloseToAmmo(false);
        m_field294 = true;
        return;
    }
    // Zipline launcher feedback events for whether firing is available.
    case 79:
        // Case 79 opens with the same crosshair-alpha restore that updateGuiIndex ends with, before
        // the weapon/soldier tests (78C260..78C29F).
        if (m_crossHairMode == 1 || m_crossHairMode == 2) {
            hudManager->getInformationAdapter()->setFloat("CrossHairColorAlpha", 1.0f);
            m_crossHairMode = 0;
        }
        handleCrossHairEvent(id, local, data);
        return;
    case 80:
    case 81:
        handleCrossHairEvent(id, local, data);
        return;
    default:                                                // ids 4-30, 32-43, 45-56, 59-68, 70-78
        return;
    }
}

// The same unconditional local-player lookup, again before the switch. The jump table covers ids
// 6..11; 7 and 10 fall straight through to the exit. The payload is the posted IPlayer* for 6, 9 and
// 11, and the kill payload's first int for 8.
void HudVehicleInformation::handleGameEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    switch (id) {
    case 6: {                                               // ServerGameLogic::spawnPlayer
        world::IPlayer* const player = static_cast<world::IPlayer*>(data);
        if (!local || !player) return;
        const int localId = local->getId();                  // the local id is read first
        if (player->getId() != localId) return;
        updateHudMode();
        updateGuiIndex();
        return;
    }
    case 8: {                                               // GameLogic::killPlayer
        if (!local) return;
        if (local->getFreeCameraEnabled()) return;
        world::IPlayer* const victim =
            world::playerManager->getPlayerFromId(*static_cast<int*>(data));
        if (!victim) return;
        if (!hudManager->getPaint()) return;
        const int localId = local->getId();
        if (victim->getId() != localId) return;
        reset(false);
        hudManager->disablePointer();
        eventManager->postEvent(ECHud, 2u, static_cast<void*>(0), 0.0f);
        // Retail queries the vehicle three separate times here.
        if (!local->getVehicle()) return;
        if (!local->getVehicle()->m_warningHudComponent) return;
        static_cast<world::IWarningHudComp*>(local->getVehicle()->m_warningHudComponent)
            ->setWarningState(static_cast<world::WarningState>(0));
        m_warningIconShow = false;
        return;
    }
    case 9: {                                               // GameServer::enterVehicle
        world::IPlayer* const player = static_cast<world::IPlayer*>(data);
        if (!local || !player) return;
        if (player == local && hudManager->getMap())          // getMap is queried twice
            dispatchHudMapApplyVehicleZoom(hudManager->getMap());
        world::IObject* const defaultVehicle = player->getDefaultVehicle();
        if (player->getVehicle() == defaultVehicle) return;
        const int localId = local->getId();
        if (player->getId() == localId) {
            updateGuiIndex();
            world::IObject* const vehicle = player->getVehicle();
            if (vehicle && vehicle->m_vehicleHudComponent
                && static_cast<world::IVehicleHudComp*>(vehicle->m_vehicleHudComponent)
                       ->getHudInfo()->showInfo) {
                m_field170 = 2;
                hudManager->disableVOHelpMessage(0x43);
            }
        }
        // Both roots are taken through the lazy IObject parent cache, local side first.
        world::IObject* const localRoot = world::getRootParent(local->getVehicle());
        if (world::getRootParent(player->getVehicle()) != localRoot) return;
        const int againLocalId = local->getId();
        const bool sameAsLocal = player->getId() == againLocalId;
        setupOccupiedVehicle(player->getVehicle(), sameAsLocal);
        return;
    }
    case 11: {                                              // GameServer::exitVehicle
        world::IPlayer* const player = static_cast<world::IPlayer*>(data);
        if (!local || !player) return;
        const int localId = local->getId();
        if (player->getId() != localId) return;
        if (player->getDefaultVehicle() != player->getVehicle()) return;
        updateHudMode();
        clear();
        m_warningIconShow = false;
        hudManager->disableVOHelpMessage(0x44);
        if (!hudManager->getMap()) return;                   // getMap is queried twice again
        dispatchHudMapApplyVehicleZoom(hudManager->getMap());
        return;
    }
    default:                                                // ids 7 and 10, and everything outside
        return;
    }
}

// applyHudState - 78BB30, 175 B. What ECHud 2 (HudStateChanged) actually does.
// The local player is queried before the mode is examined, and the commander predicate is
// evaluated before the local-player test even though nothing consumes it until the very last line.
// Both orders are retail. isPlayerAlive is BF2HudManager +0x34C, whose body reads the soldier
// component's +0x5C: a true answer skips the refresh, which is why its polarity looks inverted here.
void HudVehicleInformation::applyHudState(int mode)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (mode == 15) {                                        // the commander board keeps its state
        reset(true);
        return;
    }
    const bool commanderMode = hudManager->isCommanderShown()
                            && hudManager->isCommanderMode(hudManager->getMode());
    if (!local) return;
    if (hudManager->isPlayerAlive(local)) return;
    if (!local->getIsAlive()) return;
    if (m_field170 != 0) return;
    if (commanderMode) return;
    updateHudMode();
}

// updateHudMode - 78B870, 206 B. Recomputes the vehicle-info mode, then tail-calls updateGuiIndex.
// The commander arm is the only exit that skips updateGuiIndex; every other path reaches it,
// including the one with no local player. The vehicle-hud component and its showInfo flag are read
// twice, once before the default-vehicle test and once after the ladder test. That repetition is in
// the binary.
void HudVehicleInformation::updateHudMode()
{
    if (hudManager->isCommanderShown() && hudManager->isCommanderMode(hudManager->getMode()))
        return;
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (local) {
        m_field170 = 1;
        m_killedState = false;
        m_field294 = false;
        world::IObject* const vehicle = local->getVehicle();
        if (vehicle && vehicle->m_vehicleHudComponent
            && static_cast<world::IVehicleHudComp*>(vehicle->m_vehicleHudComponent)
                   ->getHudInfo()->showInfo
            && vehicle != local->getDefaultVehicle()
            && vehicle->getTemplate()->getClassID() != world::CID_LadderContainerTemplate
            && vehicle->m_vehicleHudComponent
            && static_cast<world::IVehicleHudComp*>(vehicle->m_vehicleHudComponent)
                   ->getHudInfo()->showInfo)
            m_field170 = 2;
    }
    updateGuiIndex();
}

// Selects the vehicle GUI index. Some exit paths clear it while others retain its previous value.
void HudVehicleInformation::updateGuiIndex()
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    bool zeroOnExit = true;

    if (m_field170 != 0 && local && local->getIsAlive() && local->getVehicle()) {
        world::IObject* selected = local->getVehicle();
        if (local->getVehicle() == local->getDefaultVehicle()) {
            // On foot the kit carries the HUD component; a missing kit falls back to the vehicle.
            if (world::IObject* const kit = hudManager->getPlayerKit(local)) selected = kit;
        }
        if (selected) {
            world::IPlayerControlObject* control = nullptr;
            // Complete 440060: first matching interface up the parent chain.
            for (world::IObject* walk = selected; walk; walk = walk->getParent()) {
                control = static_cast<world::IPlayerControlObject*>(
                    walk->queryInterface(world::IID_IPlayerControlObject));
                if (control) break;
            }
            if (!control) {
                zeroOnExit = false;
            } else if (selected->m_vehicleHudComponent) {
                // [INFERRED] Ducking suppresses the vehicle index on this path.
                if (!control->getIsDucking()) {
                    zeroOnExit = false;
                    m_guiIndex = static_cast<world::IVehicleHudComp*>(selected->m_vehicleHudComponent)
                                     ->getGuiIndex(m_field244 || !m_field2ac);
                }
            } else {
                zeroOnExit = false;
                world::IObject* const weapon = control->getActiveWeapon(0);
                if (weapon && !(weapon->getFlags() & 1)) {
                    world::IObject* const again = control->getActiveWeapon(0);   // queried twice
                    if (again->m_weaponHudComponent)
                        m_guiIndex = static_cast<world::IWeaponHudComp*>(again->m_weaponHudComponent)
                                         ->getGuiIndex(m_field244 || !m_field2ac);
                }
            }
        }
    }
    if (zeroOnExit) m_guiIndex = 0;
    m_field0a4 = m_guiIndex;
    if (m_crossHairMode == 1 || m_crossHairMode == 2) {
        hudManager->getInformationAdapter()->setFloat("CrossHairColorAlpha", 1.0f);
        m_crossHairMode = 0;
    }
}

// Updates the local player's post-processing state.
bool HudVehicleInformation::updatePostProcessing()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player) return false;
    world::IObject* object = hudManager->getPlayerKit(player);
    world::IObject* defaultVehicle = player->getDefaultVehicle();
    if (player->getVehicle() != defaultVehicle) object = player->getVehicle();
    if (player->getOverheadCameraEnabled() && hudManager->getMap()
        && hudManager->getMap()->m_overheadZoomActive) {
        const int team = player->getTeam();
        object = static_cast<world::IObject*>(static_cast<ISquadManager*>(squadManager)->getCommanderCam(team));
    }
    if (!object || !object->m_vehicleHudComponent) return false;
    auto* component = static_cast<world::IVehicleHudComp*>(object->m_vehicleHudComponent);
    world::IPostProcessComp* post = component->getEnablePostProcessing(m_guiIndex);
    if (!post) return false;
    IBase* framework = classManager->getSingleton("PostProductionFrameWork");
    if (framework) {
        dispatchPostProductionFrequency(framework, post->getInfo()->tvDistortionFreq);
        dispatchPostProductionRoll(framework, post->getInfo()->tvDistortionRoll);
        dispatchPostProductionScale(framework, post->getInfo()->tvDistortionScale);
        dispatchPostProductionInterference(framework, post->getInfo()->tvInterference);
    }
    return true; // Retail succeeds once a component exists, even without the singleton.
}

void HudVehicleInformation::update(float dt)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    updateLayout();
    if (player) {
        if (player->getIsAlive() && !hudManager->isPlayerAlive(player)) {
            m_blinkManager->update(dt);
            const bool vehicleVisible = m_bottomLeftVehicleAlpha > 0.0f;
            m_playerHealthShow = true;
            m_vehicleIconShow = vehicleVisible;
            m_vehicleArmorShow = vehicleVisible;
            if (!m_field294) updateProximity(dt);
            if (player->getDefaultVehicle()) {
                if (m_field0bc >= 0.0f) m_field0bc -= dt;
                updatePlayerInformation(player->getDefaultVehicle());
                updatePlayerAngles(player);
                world::Kit* kit = hudManager->getPlayerKit(player);
                if (kit && kit->m_vehicleHudComponent)
                    assignString(m_localKitName,
                        static_cast<world::IVehicleHudComp*>(kit->m_vehicleHudComponent)->getHudInfo()->hudName.c_str());
            }
            world::IObject* defaultVehicle = player->getDefaultVehicle();
            if (player->getVehicle() != defaultVehicle) {
                updateVehicleInformation(player->getVehicle(), dt);
                updateVehicleAngles(player->getVehicle());
            } else m_turretIconShow = false;
            world::ICameraObject* camera = player->getCamera();
            if (camera) {
                const int mode = camera->getViewMode();
                if (mode != 0 && mode != 1 && mode != 2) { // Negative values also enter.
                    world::IObject* vehicle = player->getVehicle();
                    if (vehicle) {
                        world::IPlayerControlObject* control = nullptr;
                        // Complete440060: first matching interface up the parent chain.
                        while (vehicle) {
                            control = static_cast<world::IPlayerControlObject*>(vehicle->queryInterface(world::IID_IPlayerControlObject));
                            if (control) break;
                            vehicle = vehicle->getParent();
                        }
                        if (control) for (int index = 0; index < 3; ++index) {
                            world::IObject* weapon = control->getActiveWeapon(index);
                            if (weapon && weapon->m_weaponHudComponent)
                                static_cast<world::IWeaponHudComp*>(weapon->m_weaponHudComponent)->setLockState(static_cast<world::WarningState>(0));
                        }
                    }
                }
            }
        } else {
            m_squadInfoBarShow = false;
            m_showCommanderIcon = false;
            m_showSquadIcon = false;
            m_usingParachute = false;
            m_playerHealthShow = false;
            m_playerStaminaShow = false;
            if (std::wcscmp(L"", m_squadInfoText)) {
                if (m_squadInfoText && m_squadInfoText != &s_emptyWideString) {
                    ::operator delete(m_squadInfoText);
                    m_squadInfoText = nullptr;
                }
                m_squadInfoText = static_cast<wchar_t*>(::operator new(sizeof(wchar_t)));
                *m_squadInfoText = L'\0'; // Complete737F20 for the known empty source.
            }
            assignString(m_squadInfoIconPath, "");
            m_field0bc = -1.0f;
        }
    }
    if (m_field0b4 > 0.0f) {
        const double remaining = static_cast<double>(m_field0b4) - dt;
        m_field0b4 = static_cast<float>(remaining);
        // Compare the unrounded subtraction after its independent float store.
        if (remaining <= 0.0 && m_field0a8 == m_guiIndex) m_guiIndex = m_field0b0;
    }
    if (m_crossHairMode == 1 || m_crossHairMode == 2) {
        if (m_field2a8 == 0.0f) m_field2a8 = 1.0f;
        HudInformationAdapter* adapter = hudManager->getInformationAdapter();
        const float alpha = m_field2a4 / m_field2a8;
        adapter->setFloat("CrossHairColorAlpha", alpha);
        const double remaining = static_cast<double>(m_field2a4) - dt;
        m_field2a4 = static_cast<float>(remaining);
        if (remaining <= 0.0) {
            hudManager->getInformationAdapter()->setFloat("CrossHairColorAlpha", 1.0f);
            m_guiIndex = m_field0b0;
            m_crossHairMode = 0;
        }
    }
}

HudVehicleInformation::HudVehicleInformation()
    : m_system(nullptr), m_field018(0.0f), m_field020(false), m_field021(false),
      m_field024(1.0f), m_connectionMessage(), m_maxClimbSpeed(10), m_guiIndex(0),
      m_field0a4(0), m_field0a8(0), m_field0ac(0), m_field0b0(0), m_field0b4(0.0f),
      m_usingParachute(false), m_field0bc(-1.0f), m_field170(0), m_field174(0),
      m_field178(-295.0f), m_field17c(-137.0f), m_field180(54.0f),
      m_field184(-295.0f), m_field188(-295.0f), m_bottomLeftHealthAlpha(0.0f),
      m_bottomLeftVehicleAlpha(0.0f), m_field19c(0.0f), m_field1a0(0.0f),
      m_playerHealth(0.0f), m_playerStamina(0.0f), m_nightVisionGauge(0.0f),
      m_vehicleStamina(0.0f), m_vehicleArmor(0.0f), m_playerAngle(0.0f),
      m_playerBanking(0.0f), m_playerElevation(0.0f), m_vehicleAngle(0.0f),
      m_vehicleTurretAngle(0.0f), m_vehicleBanking(0.0f), m_vehicleElevation(0.0f),
      m_vehicleElevationSpeedAngle(0.0f), m_unknown1dc(0.0f), m_gunnerAngle(0.0f),
      m_torque(0.0f), m_field1e8(0.0f), m_torqueAngle(0.0f), m_angleOfAttack(0),
      m_field244(false), m_playerStaminaShow(false), m_playerStaminaBlink(false),
      m_nightVisionGaugeShow(false), m_nightVisionGaugeBlink(false),
      m_vehicleStaminaShow(false), m_vehicleStaminaBlink(false),
      m_playerHealthShow(false), m_playerHealthBlink(false),
      m_vehiclePassengersShow(false), m_vehicleIconShow(false),
      m_vehicleArmorShow(false), m_vehicleArmorBlink(false), m_turretIconShow(false),
      m_killedState(false), m_warningIconShow(false), m_warningSources(),
      m_referenceCrossShow(false), m_warningIconPath(&s_emptyString),
      m_vehicleArmorString(&s_emptyString), m_playerHealthString(&s_emptyString),
      m_localKitName(&s_emptyString), m_vehicleName(&s_emptyString),
      m_vehicleIconPath(&s_emptyString), m_speedString(&s_emptyString),
      m_altitudeString(&s_emptyString), m_torqueString(&s_emptyString),
      m_closeToMedic(false), m_closeToRepair(false), m_closeToAmmo(false),
      m_field293(false), m_field294(false), m_squadInfoBarShow(false),
      m_showCommanderIcon(false), m_showSquadIcon(false), m_squadInfoText(&s_emptyWideString),
      m_squadInfoIconPath(&s_emptyString), m_crossHairMode(0), m_field2a4(0.0f),
      m_field2a8(0.0f), m_field2ac(true)
{
    // Registration precedes blink allocation and occupant/color initialization.
    // Keep the enclosing HUD publication guard while other owner slots are open.
    eventManager->registerEventHandler(ECGame, this, 0);
    eventManager->registerEventHandler(ECGameLogic, this, 0);
    eventManager->registerEventHandler(ECHud, this, 0);
    void* allocation = ::operator new(sizeof(HudBlinkManager));
    m_blinkManager = allocation ? new(allocation) HudBlinkManager : nullptr;
    m_warningSources.clear();
    for (int i = 0; i != 10; ++i) {
        m_occupantNames[i] = &s_emptyString;
        m_occupantShow[i] = false;
        for (float& component : m_occupantColors[i]) component = 0.0f;
    }
    for (float& component : m_playerHealthColor) component = 1.0f;
    for (float& component : m_vehicleArmorColor) component = 1.0f;
}

namespace {
HudInformationLayer* vehicleInformation()
{ return static_cast<HudInformationLayer*>(g_hudInformationLayer); }
bool registerVehicleFloat(meme::System* system, float* value,
                          const std::string& name, const std::string& container)
{
    void* allocation = ::operator new(sizeof(meme::FloatRefData));
    auto* data = allocation ? new(allocation) meme::FloatRefData(value, name.c_str()) : nullptr;
    if (data) {
        system->registerData(data, container.c_str());
        return true;
    }
    const std::string caption = formatString("Failed to register: %s in %s", name.c_str(), container.c_str());
    MessageBoxA(nullptr, "Error RegisterData", caption.c_str(), 0); // 785860, retail text/caption
    return false;
}
HudBlinker* makeVehicleBlinker(float* source, bool* output)
{
    void* allocation = ::operator new(sizeof(HudBlinker));
    return allocation ? new(allocation) HudBlinker(source, output, 0.3f, 0.0f, 0.7f, 0.15f, true) : nullptr;
}
}
bool HudVehicleInformation::init() // 789480; every registration result is discarded
{
    void* allocation = ::operator new(sizeof(meme::BfOccupiedVehicleData));
    m_occupiedData = allocation ? new(allocation) meme::BfOccupiedVehicleData("OccupiedData") : nullptr;
    if (!m_occupiedData) return false;
#define VEHICLE_REF(containerText, nameText, member) { \
    const std::string container(containerText), name(nameText); \
    registerVehicleFloat(m_system, &member, name, container); }
    VEHICLE_REF("BottomLeft/Alpha", "BottomLeft_alpha1", m_bottomLeftHealthAlpha);
    VEHICLE_REF("BottomLeft/Alpha", "BottomLeft_alpha2", m_bottomLeftVehicleAlpha);
    VEHICLE_REF("BottomLeft/Alpha", "BottomLeft_nextAlpha1", m_field19c);
    VEHICLE_REF("BottomLeft/Alpha", "BottomLeft_nextAlpha2", m_field1a0);
    VEHICLE_REF("BottomLeft", "BottomLeft_nextXPos", m_field188);
    VEHICLE_REF("BottomLeft", "BottomLeft_XPos", m_field184);
#undef VEHICLE_REF
#define VEHICLE_BIND(kind, key, value) { const std::string name(key); vehicleInformation()->register##kind(name, value); }
    VEHICLE_BIND(Float, "BottomLeftHealthAlpha", &m_bottomLeftHealthAlpha);
    VEHICLE_BIND(Float, "BottomLeftVehicleAlpha", &m_bottomLeftVehicleAlpha);
    VEHICLE_BIND(Float, "BottomLeftHealthFadedAlpha", &m_bottomLeftHealthFadedAlpha);
    VEHICLE_BIND(Float, "BottomLeftVehicleFadedAlpha", &m_bottomLeftVehicleFadedAlpha);
    VEHICLE_BIND(Bool, "UsingParachute", &m_usingParachute);
    VEHICLE_BIND(Float, "PlayerStamina", &m_playerStamina);
    VEHICLE_BIND(Float, "NightVisionGaugeValue", &m_nightVisionGauge);
    VEHICLE_BIND(Float, "VehicleStamina", &m_vehicleStamina);
    VEHICLE_BIND(Float, "PlayerHealth", &m_playerHealth);
    VEHICLE_BIND(Float, "VehicleArmor", &m_vehicleArmor);
    VEHICLE_BIND(String, "PlayerHealthString", const_cast<const char**>(&m_playerHealthString));
    VEHICLE_BIND(String, "VehicleArmorString", const_cast<const char**>(&m_vehicleArmorString));
    VEHICLE_BIND(Float, "VehicleAngle", &m_vehicleAngle);
    VEHICLE_BIND(Float, "VehicleTurretAngle", &m_vehicleTurretAngle);
    VEHICLE_BIND(Float, "VehicleBanking", &m_vehicleBanking);
    VEHICLE_BIND(Float, "VehicleElevation", &m_vehicleElevation);
    VEHICLE_BIND(Float, "VehicleElevationSpeedAngle", &m_vehicleElevationSpeedAngle);
    VEHICLE_BIND(Float, "PlayerAngle", &m_playerAngle);
    VEHICLE_BIND(Float, "PlayerBanking", &m_playerBanking);
    VEHICLE_BIND(Float, "PlayerElevation", &m_playerElevation);
    VEHICLE_BIND(String, "AltitudeString", const_cast<const char**>(&m_altitudeString));
    VEHICLE_BIND(String, "SpeedString", const_cast<const char**>(&m_speedString));
    VEHICLE_BIND(String, "TorqueString", const_cast<const char**>(&m_torqueString));
    VEHICLE_BIND(Int, "AngleOfAttack", &m_angleOfAttack);
    VEHICLE_BIND(Float, "Torque", &m_torque);
    VEHICLE_BIND(Float, "TorqueAngle", &m_torqueAngle);
    VEHICLE_BIND(Bool, "VehiclePassengersShow", &m_vehiclePassengersShow);
    VEHICLE_BIND(Bool, "VehicleIconShow", &m_vehicleIconShow);
    VEHICLE_BIND(String, "VehicleIconPathString", const_cast<const char**>(&m_vehicleIconPath));
    VEHICLE_BIND(Float, "GunnerAngle", &m_gunnerAngle);
    VEHICLE_BIND(Bool, "WarningIconShow", &m_warningIconShow);
    VEHICLE_BIND(String, "WarningIconPath", const_cast<const char**>(&m_warningIconPath));
    VEHICLE_BIND(Bool, "PlayerStaminaShow", &m_playerStaminaShow);
    VEHICLE_BIND(Bool, "VehicleStaminaShow", &m_vehicleStaminaShow);
    VEHICLE_BIND(Bool, "PlayerHealthShow", &m_playerHealthShow);
    VEHICLE_BIND(Bool, "PlayerStaminaBlink", &m_playerStaminaBlink);
    VEHICLE_BIND(Bool, "NightVisionGaugeBlink", &m_nightVisionGaugeBlink);
    VEHICLE_BIND(Bool, "VehicleStaminaBlink", &m_vehicleStaminaBlink);
    VEHICLE_BIND(Bool, "PlayerHealthBlink", &m_playerHealthBlink);
    VEHICLE_BIND(Bool, "VehicleArmorShow", &m_vehicleArmorShow);
    VEHICLE_BIND(Bool, "VehicleArmorBlink", &m_vehicleArmorBlink);
    VEHICLE_BIND(Bool, "TurretIconShow", &m_turretIconShow);
    VEHICLE_BIND(Bool, "NightVisionGaugeShow", &m_nightVisionGaugeShow);
    VEHICLE_BIND(Bool, "ReferenceCrossShow", &m_referenceCrossShow);
    VEHICLE_BIND(String, "LocalKitNameString", const_cast<const char**>(&m_localKitName));
    VEHICLE_BIND(String, "VehicleNameString", const_cast<const char**>(&m_vehicleName));
    VEHICLE_BIND(Float, "PlayerHealthColorRed", &m_playerHealthColor[0]);
    VEHICLE_BIND(Float, "PlayerHealthColorGreen", &m_playerHealthColor[1]);
    VEHICLE_BIND(Float, "PlayerHealthColorBlue", &m_playerHealthColor[2]);
    VEHICLE_BIND(Float, "PlayerHealthColorAlpha", &m_playerHealthColor[3]);
    VEHICLE_BIND(Float, "PlayerSprintColorRed", &m_playerSprintColor[0]);
    VEHICLE_BIND(Float, "PlayerSprintColorGreen", &m_playerSprintColor[1]);
    VEHICLE_BIND(Float, "PlayerSprintColorBlue", &m_playerSprintColor[2]);
    VEHICLE_BIND(Float, "PlayerSprintColorAlpha", &m_playerSprintColor[3]);
    VEHICLE_BIND(Float, "NightVisionColorRed", &m_nightVisionColor[0]);
    VEHICLE_BIND(Float, "NightVisionColorGreen", &m_nightVisionColor[1]);
    VEHICLE_BIND(Float, "NightVisionColorBlue", &m_nightVisionColor[2]);
    VEHICLE_BIND(Float, "NightVisionColorAlpha", &m_nightVisionColor[3]);
    VEHICLE_BIND(Float, "VehicleSprintColorRed", &m_vehicleSprintColor[0]);
    VEHICLE_BIND(Float, "VehicleSprintColorGreen", &m_vehicleSprintColor[1]);
    VEHICLE_BIND(Float, "VehicleSprintColorBlue", &m_vehicleSprintColor[2]);
    VEHICLE_BIND(Float, "VehicleSprintColorAlpha", &m_vehicleSprintColor[3]);
    VEHICLE_BIND(Float, "VehicleArmorColorRed", &m_vehicleArmorColor[0]);
    VEHICLE_BIND(Float, "VehicleArmorColorGreen", &m_vehicleArmorColor[1]);
    VEHICLE_BIND(Float, "VehicleArmorColorBlue", &m_vehicleArmorColor[2]);
    VEHICLE_BIND(Float, "VehicleArmorColorAlpha", &m_vehicleArmorColor[3]);
    VEHICLE_BIND(Bool, "CloseToMedic", &m_closeToMedic);
    VEHICLE_BIND(Bool, "CloseToRepair", &m_closeToRepair);
    VEHICLE_BIND(Bool, "CloseToAmmo", &m_closeToAmmo);
    VEHICLE_BIND(Bool, "SquadInfoBarShow", &m_squadInfoBarShow);
    VEHICLE_BIND(Bool, "ShowCommanderIcon", &m_showCommanderIcon);
    VEHICLE_BIND(Bool, "ShowSquadIcon", &m_showSquadIcon);
    VEHICLE_BIND(WideString, "SquadInfoText", const_cast<const wchar_t**>(&m_squadInfoText));
    VEHICLE_BIND(String, "SquadInfoIconPath", const_cast<const char**>(&m_squadInfoIconPath));
    VEHICLE_BIND(Bool, "KilledState", &m_killedState);
    VEHICLE_BIND(Int, "GuiIndex", &m_guiIndex);
    for (int i = 0; i != 10; ++i) {
        VEHICLE_BIND(Bool, formatString("OccupyingPlayerName%iShow", i + 1), &m_occupantShow[i]);
        VEHICLE_BIND(String, formatString("OccupyingPlayerName%iString", i + 1), const_cast<const char**>(&m_occupantNames[i]));
        VEHICLE_BIND(Int, formatString("OccupyingPlayerPos%iX", i + 1), &m_occupantX[i]);
        VEHICLE_BIND(Int, formatString("OccupyingPlayerPos%iY", i + 1), &m_occupantY[i]);
        VEHICLE_BIND(Float, formatString("OccupyingPlayer%iColorRed", i + 1), &m_occupantColors[i][0]);
        VEHICLE_BIND(Float, formatString("OccupyingPlayer%iColorGreen", i + 1), &m_occupantColors[i][1]);
        VEHICLE_BIND(Float, formatString("OccupyingPlayer%iColorBlue", i + 1), &m_occupantColors[i][2]);
    }
#undef VEHICLE_BIND
    // All five allocations precede the first vector insertion; nulls also enter.
    HudBlinker* health = makeVehicleBlinker(&m_playerHealth, &m_playerHealthBlink);
    HudBlinker* armor = makeVehicleBlinker(&m_vehicleArmor, &m_vehicleArmorBlink);
    HudBlinker* sprint = makeVehicleBlinker(&m_playerStamina, &m_playerStaminaBlink);
    HudBlinker* nightVision = makeVehicleBlinker(&m_nightVisionGauge, &m_nightVisionGaugeBlink);
    HudBlinker* vehicleSprint = makeVehicleBlinker(&m_vehicleStamina, &m_vehicleStaminaBlink);
    m_blinkManager->add(health);
    m_blinkManager->add(armor);
    m_blinkManager->add(sprint);
    m_blinkManager->add(nightVision);
    m_blinkManager->add(vehicleSprint);
    return true;
}

void HudVehicleInformation::setSystem(meme::System* system) { m_system = system; }
bool HudVehicleInformation::refresh() { return true; } // 5A5E20 MOV AL,1;RET
void HudVehicleInformation::unknown0C() {} // 83D270 RET
void HudVehicleInformation::unknown10() {} // 83D270 RET
void HudVehicleInformation::unknown14() {} // 83D270 RET
void HudVehicleInformation::unknown18() {} // 83D270 RET
int HudVehicleInformation::getGuiIndex() { return m_guiIndex; }

void HudVehicleInformation::setMouseLook(bool active)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player || !player->getCamera() || !player->getCamera()->getToggleMouseLook())
        return;
    if (active) {
        if (m_guiIndex) {
            m_field0a4 = m_guiIndex;
            m_guiIndex = 0;
        }
    } else if (!m_guiIndex) {
        // Preserve the unrounded x87 subtraction and unordered rejection.
        const float tolerance = 0.01f;
        const float first = m_playerAngle, second = m_vehicleAngle;
        bool aligned;
#if defined(_M_IX86)
        __asm {
            fld first
            fsub second
            fabs
            fcomp tolerance
            fnstsw ax
            test ah, 41h
            setnp aligned
        }
#else
        // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
        // setnp after test ah,41h: an ordered difference at or below the tolerance.
        aligned = std::fabs(static_cast<double>(first) - second) <= tolerance;
#endif
        if (aligned) m_guiIndex = m_field0a4;
    }
}

// Updates proximity indicators for nearby medics, mechanics and ammunition sources.
void HudVehicleInformation::updateProximity(float)
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;

    bool changed = false;
    if (player->getDefaultVehicle() != player->getVehicle()) {
        if (!m_field293) changed = true;
        m_field293 = true;
    } else {
        if (m_field293) changed = true;
        m_field293 = false;
    }

    setCloseToMedic(!player->getIsGivingAbility() && !player->getIsRepairing()
                    && player->getHealRepairedAtTick() + 15u
                           > static_cast<unsigned>(WorldPref::getTickCount()));

    setCloseToRepair(m_field293 && !player->getIsGivingAbility() && player->getIsRepairing()
                     && player->getHealRepairedAtTick() + 15u
                            > static_cast<unsigned>(WorldPref::getTickCount()));

    setCloseToAmmo(!changed && !player->getIsGivingReplenish()
                   && player->getGaveAmmoAtTick() + 15u
                          > static_cast<unsigned>(WorldPref::getTickCount()));
}

void HudVehicleInformation::setCloseToMedic(bool close)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player) {
        world::Kit* kit = hudManager->getPlayerKit(player);
        if (kit) {
            auto* ability = static_cast<world::IAbilityHudComp*>(kit->m_abilityHudComponent);
            if (ability && (m_closeToMedic != close || (!close && ability->isPlayingHealSound()))) {
                ability->setGetHealActive(close);
                m_closeToMedic = close;
            }
        }
    }
}

void HudVehicleInformation::setCloseToRepair(bool close)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player) {
        world::Kit* kit = hudManager->getPlayerKit(player);
        if (kit) {
            auto* ability = static_cast<world::IAbilityHudComp*>(kit->m_abilityHudComponent);
            if (ability && (m_closeToRepair != close || (!close && ability->isPlayingRepairSound()))) {
                ability->setGetRepairActive(close);
                m_closeToRepair = close;
            }
        }
    }
}

void HudVehicleInformation::setCloseToAmmo(bool close)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (player) {
        world::Kit* kit = hudManager->getPlayerKit(player);
        if (kit) {
            auto* ability = static_cast<world::IAbilityHudComp*>(kit->m_abilityHudComponent);
            if (ability && (m_closeToAmmo != close || (!close && ability->isPlayingAmmoSound()))) {
                ability->setGetAmmoActive(close);
                m_closeToAmmo = close;
            }
        }
    }
}

void HudVehicleInformation::reset(bool keepKilledState)
{
    m_field170 = 0;
    m_field294 = false;
    if (!keepKilledState) {
        m_killedState = gameLogic->getGameStatus() != 4;
        m_playerHealth = 0.0f;
        m_vehicleArmor = 0.0f;
    }
    m_guiIndex = 0;
    m_field0a4 = 0;
    m_vehiclePassengersShow = false;
    m_turretIconShow = false;
    m_usingParachute = false;
    // Retail repeats the player/kit query for each ability, and stores each
    // cached flag only after its callback. The setters preserve both details.
    setCloseToMedic(false);
    setCloseToRepair(false);
    setCloseToAmmo(false);
    hudManager->getInformationAdapter()->setBool("CloseToMine", false);
}

void HudVehicleInformation::clear()
{
    m_field020 = false;
    m_warningSources.clear();
    m_usingParachute = false;
    m_field020 = false;
    m_field01c = 0.0f;
    m_field018 = 0.0f;
}

HudVehicleInformation::~HudVehicleInformation()
{
    for (int i = 0; i != 10; ++i) {
        m_occupantX[i] = 0;
        m_occupantY[i] = 0;
        if (m_occupantNames[i] != &s_emptyString) {
            ::operator delete(m_occupantNames[i]); // 737EE0, scalar deallocation
            m_occupantNames[i] = nullptr;
        }
    }
    if (m_blinkManager) delete m_blinkManager;
    if (m_localKitName != &s_emptyString) delete[] m_localKitName;
    if (m_vehicleName != &s_emptyString) delete[] m_vehicleName;
    if (m_vehicleIconPath != &s_emptyString) delete[] m_vehicleIconPath;
    if (m_vehicleArmorString != &s_emptyString) delete[] m_vehicleArmorString;
    if (m_playerHealthString != &s_emptyString) delete[] m_playerHealthString;
    if (m_speedString != &s_emptyString) delete[] m_speedString;
    if (m_altitudeString != &s_emptyString) delete[] m_altitudeString;
    if (m_torqueString != &s_emptyString) delete[] m_torqueString;
    if (m_warningIconPath != &s_emptyString) delete[] m_warningIconPath;
    if (m_squadInfoText != &s_emptyWideString) delete[] m_squadInfoText;
    if (m_squadInfoIconPath != &s_emptyString) delete[] m_squadInfoIconPath;
    eventManager->unregisterEventHandler(ECGame, this);
    eventManager->unregisterEventHandler(ECGameLogic, this);
    eventManager->unregisterEventHandler(ECHud, this);
    // The native set and string destruct after the explicit owner cleanup.
}

} } }
#endif
