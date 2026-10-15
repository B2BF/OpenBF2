// Weapon status and ammunition display.
#include "HudAmmo.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "HudBlinker.h"
#include "HudInformationAdapter.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#include "../Core/Ids.h"
#include "../Core/System.h"
#include "../Core/WeakPtr.h"
#include "../Game/Objects/IObject.h"
#include "../Game/Objects/IObjectTemplate.h"
#include "../Game/Objects/IWeaponObject.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/Objects/ISoldier.h"
#include "../Game/Objects/Game/Kit.h"
#include "../Game/Objects/Game/KitTemplate.h"
#include "../Game/Objects/Game/Soldier.h"
#include "../Game/Objects/GameLogicComp/IAbilityGLComp.h"
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"
#include "../Game/Objects/IAbilityHudComp.h"
#include "../Game/Objects/IVehicleHudComp.h"
#include "../Game/Objects/IWeaponHudComp.h"           // +38/+3C/+44/+4C/+50
#include "../Game/Objects/WeaponHudCompTemplate.h"    // WeaponHudInfo / WeaponHudIcons
#include "../Game/Objects/Game/TargetObject.h"        // getTargetActive, PE 5E9630
#include "../Game/Objects/Projectiles/GenericProjectileTemplate.h" // IProjectileObjectTemplate
#include "HudVehicleInformation.h"                    // +24 getGuiIndex, +244 sprint
#include "../Game/IGame.h"
#include "../Game/IGameLogic.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/ObjectManager.h"
#include "../Scene/Object/ObjectPredicator.h"
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../Game/HudInformationLayer.h"
#include "Ref2System.h"                       // getRenderView, 7AD390
#include "../RendMisc/IRenderView.h"          // +54 getOrthoWidth, +5C getOrthoHeight
#include "../Game/Main/BF2ClientSeams.h"
#include "../External/BFMeme/Data.h"
#include "../External/BFMeme/System.h"
#include "../External/BFMeme/Pointer.h"
#include <windows.h>
#include <cmath>
#include <cstring>
#include <cwchar>                                     // wcscmp / wcslen - the wide updateString
#include <new>
#include <set>
#include <vector>
namespace dice { namespace hfe { extern IGameLogic* gameLogic; } }
namespace dice { namespace hfe { namespace world {
// Returns lifetime remaining for the GenericProjectile behind an IProjectileObject interface pointer.
float projectileTimeToLive(void* projectile);
} } }
namespace dice { namespace hfe { namespace hud {
char HudAmmo::s_emptyString;
wchar_t HudAmmo::s_emptyWideString;
HudAmmo::HudAmmo(meme::System* system) // 7A5B10, complete1428-byte body
{
    m_system = system;
    m_field010 = m_field014 = 1.0f;
    m_field024 = m_field028 = 165.0f;
    m_field018 = false;
    m_field01c = 503.0f;
    m_field020 = 337.0f;
    m_field02c = 503.0f;
    m_field090 = 3.0f;
    m_field094 = 10.0f;
    m_field11c = m_field120 = 300;
    m_field098 = -1;
    m_field09c = 0.0f;
    m_field0a0 = 32;
    m_field0a4 = false;
    m_field0ac = m_field0ad = m_field0ae = m_field0af = true;
    m_field0b0 = false;
    m_field0b4 = m_field0b8 = 0.0f;
    m_field0bc = m_field0bd = m_field0be = m_field0bf = m_field0c0 = m_field0c1 = false;
    m_field0c4 = m_field0c8 = 0.0f;
    m_field0cc = m_field0cd = m_field0ce = m_field0cf = false;
    m_field0d0 = 0.0f;
    m_field0e4 = 0;
    m_field0e8 = m_field0e9 = m_field0ea = m_field0eb = m_field0ec = m_field0ed = m_field0ee = false;
    m_field0f0 = m_field0f4 = 0.0f;
    m_field0f8 = 8.0f;
    m_field0fc = 0.0f;
    m_field100 = 10;
    m_field104 = 1000;
    m_field108 = m_field10c = m_field110 = m_field114 = m_field118 = 0;
    m_field124 = m_field128 = 400;
    m_field12c = m_field12d = m_field12e = m_field12f = false;
    m_field130 = m_field131 = m_field132 = m_field133 = m_field134 = m_field135 = false;
    m_field136 = m_field137 = m_field138 = m_field139 = m_field13a = false;
    m_field13b = m_field13c = m_field13d = m_field13e = m_field13f = false;
    m_field140 = 0.0f;
    m_field144 = m_field148 = m_field14c = m_field150 = m_field154 = m_field158 = &s_emptyString;
    m_field15c = 0.0f;
    m_field160 = m_field164 = m_field168 = m_field16c = m_field170 = m_field174 = &s_emptyString;
    m_field178 = 0.0f;
    m_field17c = m_field180 = &s_emptyString;
    m_field18c = false;
    m_field190 = &s_emptyWideString;
    m_field194 = m_field198 = m_field19c = m_field1a0 = m_field1a4 = &s_emptyString;
    m_field1a8 = -1.0f;
    m_field1ac = 0.0f;
    m_field1b0 = m_field1b1 = false;
    m_field1b4 = &s_emptyString;
    m_field1b8 = 0.0f;
    m_field1bc = m_field1bd = m_field1be = m_field1bf = m_field1c0 = m_field1c1 = m_field1c2 = false;
    // +A8 and the flares fields+184/+188, including all padding, remain untouched.
    // Colors and m_blinkManager are deliberately still unwritten at subscription.
    eventManager->registerEventHandler(ECGame, this, 0);
    eventManager->registerEventHandler(ECHud, this, 0);
    void* storage = ::operator new(sizeof(HudBlinkManager));
    m_blinkManager = storage ? new(storage) HudBlinkManager : nullptr;
    storage = ::operator new(sizeof(HudBlinker));
    HudBlinker* primaryAmmo = storage ? new(storage) HudBlinker(&m_field0b4, &m_field12e, .3f, 0.0f, .7f, .15f, true) : nullptr;
    storage = ::operator new(sizeof(HudBlinker));
    HudBlinker* primaryClip = storage ? new(storage) HudBlinker(&m_field140, &m_field135, 1.5f, 0.0f, .3f, .15f, true) : nullptr;
    storage = ::operator new(sizeof(HudHighBlinker));
    HudBlinker* primaryHeat = storage ? new(storage) HudHighBlinker(&m_field0f4, &m_field12f, .7f, 1.0f, .3f, .15f, true) : nullptr;
    storage = ::operator new(sizeof(HudBlinker));
    HudBlinker* television = storage ? new(storage) HudBlinker(nullptr, &m_field12c, 0.0f, 0.0f, .7f, .7f, false) : nullptr;
    storage = ::operator new(sizeof(HudBlinker));
    HudBlinker* secondaryAmmo = storage ? new(storage) HudBlinker(&m_field0c4, &m_field137, .3f, 0.0f, .7f, .15f, true) : nullptr;
    storage = ::operator new(sizeof(HudBlinker));
    HudBlinker* secondaryClip = storage ? new(storage) HudBlinker(&m_field15c, &m_field13f, .7f, 0.0f, .3f, .15f, true) : nullptr;
    storage = ::operator new(sizeof(HudHighBlinker));
    HudBlinker* secondaryHeat = storage ? new(storage) HudHighBlinker(&m_field0f0, &m_field138, .7f, 1.0f, .3f, .15f, true) : nullptr;
    m_blinkManager->add(primaryAmmo);
    m_blinkManager->add(primaryHeat);
    m_blinkManager->add(secondaryAmmo);
    m_blinkManager->add(primaryClip);
    m_blinkManager->add(secondaryClip);
    m_blinkManager->add(secondaryHeat);
    m_blinkManager->add(television);
    m_field0d4 = m_field0d8 = m_field0dc = m_field0e0 = 1.0f;
    m_field030 = m_field034 = m_field038 = m_field03c = 1.0f;
    m_field040 = m_field044 = m_field048 = m_field04c = 1.0f;
    m_field050 = m_field054 = m_field058 = m_field05c = 1.0f;
    m_field060 = m_field064 = m_field068 = m_field06c = 1.0f;
    m_field070 = m_field074 = m_field078 = m_field07c = 1.0f;
    m_field080 = m_field084 = m_field088 = m_field08c = 1.0f;
}

void HudAmmo::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    if (category == ECGame) handleGameEvent(id, node, data);
    else if (category == ECHud) handleHudEvent(id, node, data);
}
void HudAmmo::handleGameEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (id != 8 || !local) return;
    world::IPlayer* player = world::playerManager->getPlayerFromId(*static_cast<int*>(data));
    if (!player) return;
    const int localId = local->getId();
    if (player->getId() != localId) return;
    if (!local->getFreeCameraEnabled() && hudManager->getPaint())
        hide(!local->getIsCommander());
    stopAbilitySounds(hudManager->getPlayerKit(local));
}
void HudAmmo::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    switch (id) {
    case 2:
        setHudMode(hudManager->getMode());
        break;
    case 44:
        if (data && local && local->getVehicle() && local->getVehicle()->m_warningHudComponent) {
            const auto* message = static_cast<const world::MissileLockMessage*>(data);
            // The existing16-byte transport carrier has an opaque first word.
            // PE reads its second byte, then first byte; both must be nonzero.
            if ((message->field00 & 0xff00u) && (message->field00 & 0xffu))
                m_field098 = message->field04;
        }
        break;
    case 57:
    case 58: {
        world::IPlayer* player = world::playerManager->getPlayerFromId(*static_cast<int*>(data));
        if (player && local && player == local)
            hudManager->getInformationAdapter()->setBool("CloseToMine", id == 57); // helper7893D0
        break;
    }
    default: break;
    }
}
void HudAmmo::setHudMode(int mode)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    switch (mode) {
    case 0:
        if (local && local->getIsAlive()) {
            m_field0e9 = true;
            updatePointer(m_field0a4);
        }
        break;
    case 15:
        m_field02c = m_field01c;
        m_field018 = false;
        m_field13a = false;
        // fall through
    case 17: case 18: case 22: case 23: case 24: case 26:
    case 1: case 3: case 4: case 5: case 7: case 8: case 11:
        m_field0e9 = false;
        break;
    case 12: case 16: case 19: case 20: case 21: case 27:
        break;
    default:
        m_field0e9 = true;
        break;
    }
}
// Refreshes the voice-chat indicators.
void HudAmmo::updateVoipIcons()
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (!local) return;                       // nothing is written at all
    if (local->getVoipEnabled()) {
        if (local->getIsMuted()) {
            m_field1bc = true;                // VoiceOverIconShow
            m_field1c0 = true;                // VoiceOverMuted
            return;
        }
        m_field1c0 = false;
        if (local->getIsTalking()) {
            m_field1bc = true;
            if (bf2VoipClient && bf2VoipClient->isBusy()) {          // +E4
                m_field1bd = false;           // VoiceOverTalkIconShow
                m_field1be = true;            // VoiceOverDistIconShow
                return;
            }
            if (local->getIsCommander() || local->getIsSquadLeader()) {
                if (bf2VoipClient->getUsingLeaderChannel()) {        // +A0, unguarded
                    m_field1be = false;
                    m_field1bd = false;
                    m_field1bf = true;        // VoiceOverCommanderIconShow
                    return;
                }
            }
            m_field1bf = false;
            m_field1be = false;
            m_field1bd = true;
            return;
        }
    }
    m_field1bc = false;
    m_field1bf = false;
    m_field1c0 = false;
    m_field1be = false;
    m_field1bd = false;
}

void HudAmmo::updatePointer(bool enabled)
{
    if (hudManager->isPointerEnabled() == enabled) return;
    if (enabled) {
        const meme::TextureHandle texture = m_system->loadTexture(m_field1a0, true);
        const int mode = hudManager->getMode();
        hudManager->enablePointer(texture, mode);
        auto* pointer = static_cast<meme::Pointer*>(m_system->getPointerObject());
        meme::positionPointer(m_system, pointer, m_field090, m_field094);
    } else {
        hudManager->disablePointer();
    }
}
void HudAmmo::stopAbilitySounds(world::IObject* object)
{
    if (object && object->m_abilityHudComponent)
        static_cast<world::IAbilityHudComp*>(object->m_abilityHudComponent)->forceStopAllSound();
}
void HudAmmo::updateCrosshair(world::IObject* object)
{
    if (!m_field0e9) { m_field0e8 = false; return; }
    if (!object || !object->m_vehicleHudComponent) return;
    m_field0e8 = static_cast<world::IVehicleHudComp*>(object->m_vehicleHudComponent)->getHudInfo()->showCrossHair;
    if (!m_field0e8) return;
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (local) m_field0fc = local->getCrossHairFadeValue();
    m_field0eb = m_field0fc != 0.0f; // fucom parity: unordered is also shown.
}
// Republishes HUD mode when the tracked values differ, including unordered floating-point values.
void HudAmmo::syncHudMode()
{
    if (m_field02c != m_field020) {
        m_field02c = m_field020;
        setHudMode(hudManager->getMode());
    }
}

// Updates the flare indicators across the three weapon channels.
void HudAmmo::updateFlares(world::IObject* object)
{
    m_field184 = false;
    if (!object) return;
    auto* control = static_cast<world::IPlayerControlObject*>(
        object->queryInterface(world::IID_IPlayerControlObject));
    for (int channel = 0; channel < 3; ++channel) {
        world::IObject* const active = control->getActiveWeapon(channel); // PE +74
        if (!active) continue;
        auto* weapon = static_cast<world::IWeaponObject*>(
            active->queryInterface(world::IID_IWeaponObject));
        if (!weapon) continue;
        m_field184 = weapon->getFireInput() == io::PIFlareFire;
        if (m_field184) {
            m_field188 = 1.0f - weapon->getTotalReloadTimeLeftInPercent();
            if (!weapon->isReadyToUseFire() && m_field188 == 1.0f)
                m_field188 = 0.0f;
        }
        if (m_field184)
            m_field184 = weapon->isReloading() || !weapon->isReadyToUseFire();
    }
}

// Updates HUD state for the active weapon.
void HudAmmo::updateWeapon(world::IObject* vehicle)
{
    const bool pointerWasEnabled = m_field0a4;   // captured before anything can move it
    updateFlares(vehicle);

    if (updatePrimaryWeapon(vehicle)) {
        syncHudMode();                           // the inlined copy at 7A9A3A
        updateSecondaryWeapon(vehicle);
    } else {
        world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
        world::IObjectComponent* const vehicleHud = vehicle->m_vehicleHudComponent; // +0x54
        if (!local || !vehicleHud
            || !static_cast<world::IVehicleHudComp*>(vehicleHud)->getHudInfo()->useChildHud) {
            m_field0c4 = 0.0f;                   // the PE's exact store order
            m_field0b4 = 0.0f;
            m_field136 = false;
            m_field12d = false;
            m_field12e = false;
            m_field135 = false;
            m_field137 = false;
            m_field13f = false;
            m_field0e8 = false;
            m_field0eb = false;
            m_field0ee = false;
        } else if (updatePrimaryWeapon(local->getDefaultVehicle())) {
            syncHudMode();                       // 7A9090, the out-of-line copy
            updateSecondaryWeapon(vehicle);
        }
    }

    float menuBackgroundAlpha = 1.0f;
    if (hudManager->getInformationAdapter())
        menuBackgroundAlpha =
            hudManager->getInformationAdapter()->getFloat(std::string("MenuBackgroundAlpha"));
    if (m_field010 > 0.0f)
        m_field014 = menuBackgroundAlpha - 1.0f + m_field010;
    else
        m_field014 = 0.0f;

    if (m_field0a4 != pointerWasEnabled)
        updatePointer(m_field0a4);
}

// Returns the fire-rate icon path and updates its visibility flag. Negative rates select icon zero.
std::string HudAmmo::getFireRateIconPath(int fireRate)
{
    const std::string prefix("Ingame/Weapons/Icons/Hud/firerate_");
    if (fireRate > -1) {
        m_field18c = true;
        return formatString("%s%i.tga", prefix.c_str(), fireRate);
    }
    m_field18c = false;
    return std::string("Ingame/Weapons/Icons/Hud/firerate_0");
}

// Positions the weapon crosshairs.
void HudAmmo::updateCrosshairPositions(world::IWeaponObject* weapon)
{
    if (!weapon) return;
    auto* view = static_cast<dx9rend::IRenderView*>(
        static_cast<menu::Ref2System*>(m_system)->getRenderView());
    const int deviation  = static_cast<int>(std::ceil(
        static_cast<double>(weapon->getDeviation()) * m_field100));
    const int halfWidth  = static_cast<int>(std::ceil(
        static_cast<double>(view->getOrthoWidth()) * 0.5));
    const int halfHeight = static_cast<int>(std::ceil(
        static_cast<double>(view->getOrthoHeight()) * 0.5));
    const int margin  = static_cast<int>(m_field0f8);
    const int minimum = m_field108;   // WeaponHudInfo::minVisualDeviation
    const int maximum = m_field104;   // WeaponHudInfo::maxVisualDeviation
    if (minimum + deviation > maximum) {
        m_field10c = halfHeight - margin - maximum;        // the PE's store order: 10C,110,118,114
        m_field110 = maximum + halfHeight + 1;
        m_field118 = maximum + halfWidth;
        m_field114 = halfWidth - margin - maximum - 1;
    } else {
        m_field10c = 300 - margin - minimum - deviation;   // 10C,110,114,118
        m_field110 = minimum + deviation + 301;
        m_field114 = 399 - margin - minimum - deviation;
        m_field118 = minimum + deviation + 400;
    }
}

// Updates primary magazine pips and count text. Zero magazines shows a digit but no pips.
void HudAmmo::updatePrimaryClips(int clips)
{
    m_field131 = clips > 0;
    m_field133 = clips > 2;
    m_field132 = clips > 1;
    m_field134 = clips >= 0;
    if (clips >= 0)
        updateString(&m_field148, formatString("%i", clips).c_str());
}
void HudAmmo::updateSecondaryClips(int clips)
{
    m_field13b = clips > 0;
    m_field13d = clips > 2;
    m_field13c = clips > 1;
    m_field13e = clips >= 0;
    if (clips >= 0)
        updateString(&m_field164, formatString("%i", clips).c_str());
}

namespace {
// ds:97DDD0..DC and ds:97DDE0..EC - the only two colours every ammo/clip/heat selector picks
// between. Each call site pushes them as four separate .rdata dwords, so they are four floats in
// the source too, not a struct. 211/210/179 and 242/47/47 out of 255.
const float kAmmoNormalR = 0.827451f;
const float kAmmoNormalG = 0.823529f;
const float kAmmoNormalB = 0.701961f;
const float kAmmoNormalA = 1.0f;
const float kAmmoLowR    = 0.949020f;
const float kAmmoLowG    = 0.184314f;
const float kAmmoLowB    = 0.184314f;
const float kAmmoLowA    = 1.0f;

// Returns the distance between the range finder's endpoints.
float rangeFinderDistance(const Vec3& from, const Vec3& to)
{
    const float x = to.x - from.x;
    const float y = to.y - from.y;
    const float z = to.z - from.z;
    return std::sqrt(z * z + x * x + y * y);
}
}

// Updates the bomb-fuel display.
void HudAmmo::updateBombFuel(world::IWeaponObject* weapon)
{
    if (!weapon) return;

    world::IObject* const projectile = weapon->getProjectile();                    // PE +15C
    if (projectile) {
        void* const projectileFace = projectile->queryInterface(world::IID_IProjectileObject);
        if (projectileFace) {
            world::IObjectTemplate* const projectileTemplate = weapon->getProjectileTemplate(); // +190
            void* const templateFace = projectileTemplate
                ? projectileTemplate->queryInterface(world::IID_IProjectileObjectTemplate)
                : 0;
            if (templateFace) {
                // The divisor is read before the projectile call, into the reused argument slot.
                const float timeToLive = static_cast<world::IProjectileObjectTemplate*>(
                    templateFace)->getTimeToLive().value;                          // +10, then +4
                m_field0d0 = world::projectileTimeToLive(projectileFace)           // +0xB4
                           / timeToLive;                                           // fdiv, fstp +D0
            } else {
                m_field0d0 = 1.0f;
            }
        }
    }

    world::IObject* const target = weapon->getTargetObject();                      // PE +120
    if (target && static_cast<world::TargetObject*>(target)->getTargetActive()) {
        world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
        if (local) {
            const Mat4& view = local->getVehicle()->getAbsoluteTransformation();   // PE +44, +80
            const Mat4& targetTransform = target->getAbsoluteTransformation();
            // Computes the heading from the horizontal direction.
            const float heading = std::atan2(view.m[8], view.m[10]);               // fpatan #1
            const float bearing = std::atan2(targetTransform.m[14] - view.m[14],
                                             targetTransform.m[12] - view.m[12]);  // fpatan #2
            m_field178 = heading + (bearing - 1.5707964f);                         // ds:933F94
            m_field0d4 = 1.0f; m_field0d8 = 0.0f; m_field0dc = 0.0f; m_field0e0 = 1.0f;
        } else {
            m_field178 = 0.0f;
            m_field0d4 = 0.0f; m_field0d8 = 1.0f; m_field0dc = 0.0f; m_field0e0 = 1.0f;
        }
    } else {
        m_field178 = 0.0f;
        m_field0d4 = 0.0f; m_field0d8 = 1.0f; m_field0dc = 0.0f; m_field0e0 = 1.0f;
    }
}

// Updates range and coordinates for weapons with a range finder; otherwise leaves them unchanged.
// Display coordinates are world X, negative world Z, and world Y, truncated to integers.
void HudAmmo::updateRangeFinder(world::IObject* weaponObject, bool firing)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // PE +30

    world::IWeaponObject* const weapon = weaponObject
        ? static_cast<world::IWeaponObject*>(
              weaponObject->queryInterface(world::IID_IWeaponObject))
        : 0;

    if (!local) return;
    if (!local->getVehicle()) return;                                            // PE +44
    world::IObject* const vehicle = local->getVehicle();                         // asked again

    world::IObjectComponent* const hudCompRaw = weaponObject->m_weaponHudComponent; // untested
    if (!hudCompRaw) return;
    if (!static_cast<world::IWeaponHudComp*>(hudCompRaw)->getHudInfo()->hasRangeFinder)
        return;                                                                  // info +0xB8

    // weapon is dereferenced with no test (see the header).
    world::IObject* const target = weapon->getTargetObject();                    // +0x120
    const Mat4& vehicleTransform = vehicle->getAbsoluteTransformation();         // +0x80
    Vec3 vehiclePosition;                                                        // [ebp-20h]
    vehiclePosition.x = vehicleTransform.m[12];
    vehiclePosition.y = vehicleTransform.m[13];
    vehiclePosition.z = vehicleTransform.m[14];
    Vec3 targetPosition = vehiclePosition;                                       // [ebp-14h]

    if (target) {
        if (firing) {

            const Mat4& targetTransform = target->getAbsoluteTransformation();   // +0x80
            targetPosition.x = targetTransform.m[12];
            targetPosition.y = targetTransform.m[13];
            targetPosition.z = targetTransform.m[14];
            const float distance = rangeFinderDistance(targetPosition, vehiclePosition);
            updateString(&m_field190, hudManager->replaceWideTokenFromInt(                // +0x44
                hudManager->getLocalizedString(std::string("HUD_HUD_RANGE_TO_TARGET_TEXT")), // +0x38
                std::string("#RANGE#"), static_cast<int>(distance)).c_str());
            updateString(&m_field194,
                formatString("X: %i", static_cast<int>(targetPosition.x)).c_str());
            updateString(&m_field198,
                formatString("Y: %i", -static_cast<int>(targetPosition.z)).c_str());
            updateString(&m_field19c,
                formatString("Z: %i", static_cast<int>(targetPosition.y)).c_str());
        } else {

            updateString(&m_field190, hudManager->replaceWideTokenFromString(             // +0x40
                hudManager->getLocalizedString(std::string("HUD_HUD_RANGE_TO_TARGET_TEXT")), // +0x38
                std::string("#RANGE#"), std::string("- - -")).c_str());
            updateString(&m_field194, formatString("X: - - - ").c_str());   // trailing space
            updateString(&m_field198, formatString("Y: - - -").c_str());
            updateString(&m_field19c, formatString("Z: - - -").c_str());
        }
        return;
    }

    // The spotting ray's hit object; [ebp+8], the dead weaponObject slot, in retail.
    world::IObject* hit = 0;
    if (hudManager->getMode() != 4 && hudManager->getMode() != 5                 // +0x1E0 x5
        && hudManager->getMode() != 3 && hudManager->getMode() != 7
        && hudManager->getMode() != 8
        && hudManager->findSpottedTarget(hit, targetPosition, 2000.0f, true)) {  // +0x14C

        if (hit) {
            const Mat4& hitTransform = hit->getAbsoluteTransformation();         // +0x80
            targetPosition.x = hitTransform.m[12];
            targetPosition.y = hitTransform.m[13];
            targetPosition.z = hitTransform.m[14];
        }
        const float distance = rangeFinderDistance(targetPosition, vehiclePosition);
        updateString(&m_field190,
            formatString(L"%i", static_cast<int>(distance)).c_str());
        updateString(&m_field194,
            formatString("%i", static_cast<int>(targetPosition.x)).c_str());     // ds:89E620
        updateString(&m_field198,
            formatString("%i", -static_cast<int>(targetPosition.z)).c_str());
        updateString(&m_field19c,
            formatString("%i", static_cast<int>(targetPosition.y)).c_str());
    } else {
        // The varargs below are pushed by the shipped calls and ignored by the formats.
        updateString(&m_field190, formatString(L"- - -", 0).c_str());
        updateString(&m_field194,
            formatString("- - -", static_cast<int>(targetPosition.x)).c_str());
        updateString(&m_field198,
            formatString("- - -", -static_cast<int>(targetPosition.z)).c_str());
        updateString(&m_field19c,
            formatString("- - -", static_cast<int>(targetPosition.y)).c_str());
    }
}

// Updates the primary-weapon display. Returns true when a primary weapon is found.
bool HudAmmo::updatePrimaryWeapon(world::IObject* object)
{
    using namespace world;

    // the PE inlines findControlParent's loop (the shared 440060 body) here.
    IPlayerControlObject* const control = findControlParent(object);
    if (!control) return false;

    IObject* const weaponObject = control->getActiveWeapon(0);          // PE +74
    if (!weaponObject) return false;

    IObjectComponent* const hudCompRaw = weaponObject->m_weaponHudComponent;   // +0x5C
    IWeaponHudComp* const hudComp = static_cast<IWeaponHudComp*>(hudCompRaw);
    IWeaponObject* const weapon =
        static_cast<IWeaponObject*>(weaponObject->queryInterface(IID_IWeaponObject));

    bool hasMag = false;
    if (hudCompRaw) {
        if (!weapon) { m_field12d = true; return true; }

        if (!hudComp->getHudInfo()->showInfo) {                         // info +0xB4
            world::IPlayer* const local = playerManager->getLocalHumanPlayer();
            if (local) {
                m_field0a4 = false;
                hide(!local->getIsCommander());                          // PE IPlayer +110
            } else {
                hide(true);
            }
        } else {
            if (!hudComp->getHudInfo()->hudName.empty())
                updateString(&m_field17c, hudComp->getHudInfo()->hudName.c_str());
            {
                // Asked, measured, thrown away - then asked again for the text itself.
                const bool hasCrossHair = !hudComp->getCrossHairIcon().empty();
                if (hasCrossHair)
                    updateString(&m_field1a0, hudComp->getCrossHairIcon().c_str());
            }
            if (!hudComp->getHudIcons()->ammoBarFrontIcon.empty())       // icons +0xA8
                updateString(&m_field154, hudComp->getHudIcons()->ammoBarFrontIcon.c_str());
            if (!hudComp->getHudIcons()->ammoBarBackIcon.empty())        // icons +0xC4
                updateString(&m_field158, hudComp->getHudIcons()->ammoBarBackIcon.c_str());

            if (hudManager->getVehicleInformation()) {
                m_field0a4 = hudComp->getHudInfo()->enableMouse          // info +0xB5
                          && hudManager->getVehicleInformation()->getGuiIndex() != 0;
            } else {
                m_field0a4 = false;
            }
            if (m_field0a4) {
                if (hudComp->getHudInfo()->switchGuiOnCamera             // info +0xB9
                    && hudManager->getVehicleInformation()) {
                    const bool sprinting = hudManager->getVehicleInformation()->m_field244;
                    if (hudComp->getHudInfo()->altGuiIndex != hudComp->getGuiIndex(sprinting))
                        m_field0a4 = false;
                }
                if (m_field0a4)
                    m_field0a0 = hudComp->getHudInfo()->crosshairIconSize;
            }

            hasMag = hudComp->getHudInfo()->hasMag && weapon->getMagSize() >= 0;
            m_field0ac = hudComp->getHudInfo()->showAmmo;
            m_field0ad = hudComp->getHudInfo()->showClips;

            int fireRate = -1;
            if (hudComp->getHudInfo()->hasFireRate)
                fireRate = weapon->getFireRate();
            updateString(&m_field1a4, getFireRateIconPath(fireRate).c_str());

            if (hasMag) {
                m_field130 = false;
                if (!hudComp->getHudIcons()->clipIcon.empty())           // icons +0xE0
                    updateString(&m_field14c, hudComp->getHudIcons()->clipIcon.c_str());
            } else {
                m_field130 = true;
                if (!hudComp->getHudIcons()->ammoIcon.empty())           // icons +0xFC
                    updateString(&m_field150, hudComp->getHudIcons()->ammoIcon.c_str());
            }
        }
        // published on both arms above, including the hidden one.
        m_field100 = hudComp->getHudInfo()->deviationFactor;
        m_field104 = hudComp->getHudInfo()->maxVisualDeviation;
        m_field108 = hudComp->getHudInfo()->minVisualDeviation;
    }

    if (!weapon) { m_field12d = true; return true; }
    updateCrosshairPositions(weapon);
    if (hudCompRaw && !hudComp->getHudInfo()->showInfo) return false;
    m_field12d = true;

    if (m_field0ac && !(m_field0b8 < 1.0f && m_field0b8 > 0.0f)) {
        if (weapon->getMagSize() < 0) {
            hudManager->selectFloatColor(1.0f, 0.3f,
                m_field030, m_field034, m_field038, m_field03c,
                kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
            // ds:87FB54 is a lone NUL: formatString("") with no varargs, i.e. clear the digits.
            updateString(&m_field144, formatString("").c_str());
            m_field0b4 = 1.0f;
            m_field12e = false;
            m_field0bd = true;
            m_field0c1 = false;
        } else {
            m_field0bd = false;
            m_field0b4 = static_cast<float>(weapon->getMag0Size())
                       / static_cast<float>(weapon->getMagSize());
            updateString(&m_field144, formatString("%i", weapon->getMag0Size()).c_str());
            if (weapon->getMagSize() > 1) {
                hudManager->selectFloatColor(m_field0b4, 0.3f,
                    m_field030, m_field034, m_field038, m_field03c,
                    kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                    kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
                m_field0c1 = false;
            } else {
                // both colour groups are the normal one here, so the selector cannot change
                // anything - a single-round weapon never turns its ammo digits red.
                hudManager->selectFloatColor(m_field0b4, 0.1f,
                    m_field030, m_field034, m_field038, m_field03c,
                    kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                    kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA);
                m_field0c1 = true;
            }
        }
        m_field0be = weapon->getMaxNrOfMags() <= -1 && !m_field0bd;
        hudManager->selectBoolColor(weapon->getHasHeat(), true,
            m_field070, m_field074, m_field078, m_field07c,
            kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
            kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
    }


    m_field0ea = weapon->getZoom() != 0;
    m_field0b8 = 1.0f - weapon->getTotalReloadTimeLeftInPercent();
    m_field0bc = m_field0b8 < 1.0f;
    m_field0f4 = weapon->getHeat();
    m_field0ed = m_field0f4 > 0.0f;
    m_field0bf = m_field0ed && weapon->getShowHudIcon();

    if (weapon->getHasHeat() && !m_field0b0) {
        IObjectComponent* const soundOwner = weaponObject->m_weaponHudComponent;
        if (soundOwner) {
            static_cast<IWeaponHudComp*>(soundOwner)->playOverheatSound();   // +0x44
            m_field0b0 = true;
        }
    }
    if (!weapon->getHasHeat()) m_field0b0 = false;

    if (weapon->isFiring()) {
        if (hudCompRaw && hudManager->getVehicleInformation())
            m_field0e4 = hudComp->getGuiIndex(hudManager->getVehicleInformation()->m_field244);
        updateRangeFinder(weaponObject, true);
    } else {
        updateRangeFinder(weaponObject, false);
        m_field0e4 = 0;
    }

    if (m_field0ad && hasMag) {
        updatePrimaryClips(weapon->getCurrentNrOfMags() - 1);
        m_field140 = static_cast<float>(weapon->getCurrentNrOfMags() - 1); // asked again
        hudManager->selectFloatColor(m_field140 / weapon->getMaxNrOfMags(), 0.3f,
            m_field040, m_field044, m_field048, m_field04c,
            kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
            kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
    } else {
        // The selector runs on the stale +140 before it is reset, against an unreachable -10.
        hudManager->selectFloatColor(m_field140 / weapon->getMaxNrOfMags(), -10.0f,
            m_field040, m_field044, m_field048, m_field04c,
            kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
            kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
        m_field135 = false;
        m_field140 = -1.0f;
        m_field131 = false;
        m_field132 = false;
        m_field133 = false;
        m_field134 = false;
    }

    updateBombFuel(weapon);
    // The weapon interface is asked for its OWN interface id again.
    void* const face = weapon->queryInterface(IID_IWeaponObject);
    if (face)
        m_field1c1 = static_cast<IWeaponObject*>(face)->hasMissileConnection();  // +0x168
    return true;
}

// Updates secondary-weapon HUD fields and bottom-right visibility from channel one.
void HudAmmo::updateSecondaryWeapon(world::IObject* object)
{
    using namespace world;

    bool showing = false;                                                  // the +13A latch
    bool hasMag = false;

    IPlayerControlObject* const control = findControlParent(object);
    if (!control) return;

    IObject* const weaponObject = control->getActiveWeapon(1);             // channel 1
    if (weaponObject) {
        IObjectComponent* const hudCompRaw = weaponObject->m_weaponHudComponent;
        IWeaponHudComp* const hudComp = static_cast<IWeaponHudComp*>(hudCompRaw);
        IWeaponObject* const weapon =
            static_cast<IWeaponObject*>(weaponObject->queryInterface(IID_IWeaponObject));

        if (hudCompRaw) {
            if (!weapon) { m_field136 = false; goto latch; }

            if (!hudComp->getHudInfo()->showInfo) {
                // an early return, not a fall-through: the secondary bails out here, where
                // the primary carries on to publish the deviation triple.
                syncHudMode();
                if (m_field13a) { m_field13a = false; m_field018 = false; }
                return;
            }
            if (!hudComp->getHudInfo()->hudName.empty())
                updateString(&m_field180, hudComp->getHudInfo()->hudName.c_str());
            {
                const bool hasCrossHair = !hudComp->getCrossHairIcon().empty();
                if (hasCrossHair)
                    updateString(&m_field1a0, hudComp->getCrossHairIcon().c_str());
            }
            if (!hudComp->getHudIcons()->ammoBarFrontIcon.empty())
                updateString(&m_field170, hudComp->getHudIcons()->ammoBarFrontIcon.c_str());
            if (!hudComp->getHudIcons()->ammoBarBackIcon.empty())
                updateString(&m_field174, hudComp->getHudIcons()->ammoBarBackIcon.c_str());

            // only when the primary left the pointer off.
            if (!m_field0a4) {
                if (hudManager->getVehicleInformation()) {
                    m_field0a4 = hudComp->getHudInfo()->enableMouse
                              && hudManager->getVehicleInformation()->getGuiIndex() != 0;
                }
                if (m_field0a4) {
                    if (hudComp->getHudInfo()->switchGuiOnCamera
                        && hudManager->getVehicleInformation()) {
                        // not the sprint flag - the secondary passes "is there a vehicle
                        // information object at all", which is true wherever this is reached.
                        const bool argument = hudManager->getVehicleInformation() != 0;
                        if (hudComp->getHudInfo()->altGuiIndex != hudComp->getGuiIndex(argument))
                            m_field0a4 = false;
                    }
                    if (m_field0a4)
                        m_field0a0 = hudComp->getHudInfo()->crosshairIconSize;
                }
            }

            hasMag = hudComp->getHudInfo()->hasMag && weapon->getMagSize() >= 0;
            m_field0ae = hudComp->getHudInfo()->showAmmo;
            m_field0af = hudComp->getHudInfo()->showClips;

            if (hasMag) {
                m_field139 = false;
                if (!hudComp->getHudIcons()->clipIcon.empty())
                    updateString(&m_field168, hudComp->getHudIcons()->clipIcon.c_str());
            } else {
                m_field139 = true;
                if (!hudComp->getHudIcons()->ammoIcon.empty())
                    updateString(&m_field16c, hudComp->getHudIcons()->ammoIcon.c_str());
            }
        }


        if (!weapon) { m_field136 = false; goto latch; }
        showing = true;
        m_field136 = m_field010 > 0.0f;

        // one reload test, where the primary has two.
        if (m_field0ae && !(m_field0c8 < 1.0f)) {
            if (weapon->getMagSize() < 0) {
                hudManager->selectFloatColor(1.0f, 0.3f,
                    m_field050, m_field054, m_field058, m_field05c,
                    kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                    kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
                updateString(&m_field160, formatString("").c_str());
                m_field0c4 = 1.0f;
                m_field137 = false;
                m_field0cd = true;
                m_field0cf = false;
            } else {
                m_field0cd = false;
                // format first, then divide - the primary does it the other way round.
                updateString(&m_field160, formatString("%i", weapon->getMag0Size()).c_str());
                m_field0c4 = static_cast<float>(weapon->getMag0Size())
                           / static_cast<float>(weapon->getMagSize());
                if (weapon->getMagSize() > 1) {
                    hudManager->selectFloatColor(m_field0c4, 0.3f,
                        m_field050, m_field054, m_field058, m_field05c,
                        kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                        kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
                    m_field0cf = false;
                } else {
                    hudManager->selectFloatColor(m_field0c4, 0.1f,
                        m_field050, m_field054, m_field058, m_field05c,
                        kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                        kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA);
                    m_field0cf = true;
                }
            }
            m_field0ce = weapon->getMaxNrOfMags() <= -1 && !m_field0cd;
            hudManager->selectBoolColor(weapon->getHasHeat(), true,
                m_field080, m_field084, m_field088, m_field08c,
                kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
        }

        // heat before reload here; the primary publishes reload first.
        m_field0f0 = weapon->getHeat();
        m_field0ec = m_field0f0 > 0.0f;
        m_field0c0 = m_field0ec && weapon->getShowHudIcon();
        m_field0c8 = 1.0f - weapon->getTotalReloadTimeLeftInPercent();
        m_field0cc = m_field0c8 < 1.0f;

        if (m_field0af && hasMag) {
            updateSecondaryClips(weapon->getCurrentNrOfMags() - 1);
            m_field15c = static_cast<float>(weapon->getCurrentNrOfMags() - 1);
            // the RAW count against 1.5 - the primary normalises by getMaxNrOfMags().
            hudManager->selectFloatColor(m_field15c, 1.5f,
                m_field060, m_field064, m_field068, m_field06c,
                kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
        } else {
            hudManager->selectFloatColor(m_field15c, -10.0f,
                m_field060, m_field064, m_field068, m_field06c,
                kAmmoNormalR, kAmmoNormalG, kAmmoNormalB, kAmmoNormalA,
                kAmmoLowR, kAmmoLowG, kAmmoLowB, kAmmoLowA);
            m_field13f = false;
            m_field15c = -1.0f;
            m_field13b = false;
            m_field13c = false;
            m_field13d = false;
            m_field13e = false;
        }
    }

latch:
    // The latch drives the BottomRightDirection byte the fade reads.
    if (m_field13a != showing) {
        m_field13a = showing;
        m_field018 = showing;
    }
}

void HudAmmo::update(float delta)
{
    world::IPlayer* local = world::playerManager->getLocalHumanPlayer();
    if (local && local->getVehicle() && local->getDefaultVehicle() && local->getIsAlive()
        && !hudManager->isCommanderShown()) {
        updateWeapon(local->getVehicle());
        world::IObject* defaultVehicle = local->getDefaultVehicle();
        if (local->getVehicle() == defaultVehicle) {
            auto* soldier = static_cast<world::ISoldier*>(local->getDefaultVehicle()->queryInterface(world::IID_ISoldier));
            if (soldier) updateCrosshair(soldier->getKit());
            m_field1c2 = false;
        } else {
            updateCrosshair(local->getVehicle());
            m_field1c2 = true;
        }
        m_blinkManager->update(delta);
        updateAbility(delta);
        updateVoipIcons();
        return;
    }
    stopAbilitySounds(hudManager->getPlayerKit(local));
    hide(hudManager->isCommanderShown());
    m_field0c4 = m_field0b4 = 0.0f;
    m_field12d = m_field12e = m_field135 = m_field136 = m_field137 = m_field13f = false;
    m_field0e8 = m_field0eb = m_field0ee = false;
    m_field0e4 = 0;
    m_field1b0 = false;
    m_field1bc = m_field1bd = m_field1be = m_field1bf = m_field1c0 = false;
}
bool HudAmmo::canChangeWeapon(world::IObject* object) // 7A5400; ECX unused
{
    world::IPlayerControlObject* control = nullptr;
    while (object) {
        control = static_cast<world::IPlayerControlObject*>(object->queryInterface(world::IID_IPlayerControlObject));
        if (control) break;
        object = object->getParent();
    }
    if (!control) return false;
    // PE+74 is getActiveWeapon, confirmed by5F67D0; ELF's overload order differs.
    world::IObject* weapon = control->getActiveWeapon(0);
    if (!weapon) return true;
    auto* weaponInterface = static_cast<world::IWeaponObject*>(weapon->queryInterface(world::IID_IWeaponObject));
    return weaponInterface->isUnselectable(); // PE5E02F0, no null guard after the query
}
world::IPlayerControlObject* HudAmmo::findControlParent(world::IObject* object)
{
    while (object) {
        auto* control = static_cast<world::IPlayerControlObject*>(object->queryInterface(world::IID_IPlayerControlObject));
        if (control) return control;
        object = object->m_parent;
    }
    return nullptr;
}
world::IWeaponObject* HudAmmo::findAbilityWeapon(world::IPlayer* player)
{
    auto* control = findControlParent(player->getVehicle());
    if (!control) return nullptr;
    world::IObject* weapon = control->getActiveWeapon(0);
    if (!weapon) {
        if (!control->isChildSoldierActive()) return nullptr;
        world::IObject* defaultVehicle = player->getDefaultVehicle();
        if (!defaultVehicle) return nullptr;
        control = findControlParent(defaultVehicle);
        if (!control) return nullptr;
        weapon = control->getActiveWeapon(0);
        if (!weapon) return nullptr;
    }
    return static_cast<world::IWeaponObject*>(weapon->queryInterface(world::IID_IWeaponObject));
}
bool HudAmmo::isReplenishingWeapon(world::IWeaponObject* weapon)
{
    if (!weapon) return false;
    auto* object = static_cast<world::IObject*>(weapon->queryInterface(world::IID_IObject));
    if (!object || !object->m_template) return false;
    auto* component = const_cast<world::IObjectTemplate*>(object->m_template)->queryComponent(world::IID_IWeaponAmmoCompTemplate);
    return component && component->getClassID() == world::CID_ReplenishingAmmoCompTemplate;
}
namespace {
// getGLInfo returns the address of AbilityGLCompTemplate's existing first
// Boolean. Unsigned-byte access inspects its native object representation;
// it neither invents a payload object's lifetime nor reads any STL offsets.
bool abilityFlag(world::IAbilityGLComp* ability, unsigned index)
{
    const world::AbilityGLInfo* const info = ability->getGLInfo();
    switch (index) {
    case 0: return info->hasHealingAbility;
    case 1: return info->hasRepairingAbility;
    default: return info->hasAmmoAbility;
    }
}
float armorAbilityRatio(float hitPoints, float maximum, float previous, bool blend)
{
    const float half = 0.5f;
#if defined(_M_IX86)
    float result;
    __asm {
        fld hitPoints
        fdiv maximum
        cmp blend, 0
        je armor_no_blend
        fadd previous
        fmul half
    armor_no_blend:
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    double ratio = static_cast<double>(hitPoints) / maximum;
    if (blend) ratio = (ratio + previous) * half;
    return static_cast<float>(ratio);
#endif
}
float ammoAbilityRatio(int status, float previous, bool blend)
{
    const float scale = 0.06666667014360428f; // PE934BB0, bits3D888889
    const float half = 0.5f;
#if defined(_M_IX86)
    float result;
    __asm {
        fild status
        fmul scale
        cmp blend, 0
        je ammo_no_blend
        fadd previous
        fmul half
    ammo_no_blend:
        fstp result
    }
    return result;
#else
    // [PLATFORM] x64: SSE2 arithmetic (x87 keeps 80-bit intermediates).
    double ratio = static_cast<double>(status) * scale;
    if (blend) ratio = (ratio + previous) * half;
    return static_cast<float>(ratio);
#endif
}
const char repairAbilityIcon[] = "Ingame/Player/Bars/abilitybar_smallRepairIcon.tga";
const char medicAbilityIcon[] = "Ingame/Player/Bars/abilitybar_smallMedicIcon.tga";
const char ammoAbilityIcon[] = "Ingame/Player/Bars/abilitybar_smallAmmoIcon.tga";
}
float HudAmmo::getRepairAbility(world::IPlayer* player, world::IAbilityGLComp* ability)
{
    using namespace world;
    float result = 1.0f;
    auto* soldier = static_cast<ISoldier*>(player->getDefaultVehicle()->queryInterface(IID_ISoldier));
    const auto* kitTemplate = static_cast<const KitTemplate*>(soldier->getKit()->m_template);
    const int initialMaterial = kitTemplate->m_abilityInVehicleMaterial;
    AbilityPredicator predicate(0x400, nullptr, initialMaterial, false);
    std::set<IObject*> repairTargets;
    std::vector<IObject*> objects;
    IObject* vehicle = player->getVehicle();
    const float radius = kitTemplate->m_abilityInVehicleRadius;
    const Vec3& position = vehicle->getAbsolutePosition();
    objectManager->getObjectsFromGrid(position, radius, objects, &predicate);
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        IObject* target = *it;
        if (!target || target->queryInterface(IID_IProjectileObject) ||
            soldier->getKitType() != 4 || !(target->m_flags & 0x01000000u)) continue;
        auto* control = static_cast<IPlayerControlObject*>(target->queryInterface(IID_IPlayerControlObject));
        if (control) {
            const int targetTeam = control->getTeam();
            if (targetTeam != player->getTeam() && control->getTeam() != 0) continue;
        }
        auto* armor = static_cast<IArmorGLComp*>(target->m_armorComponent);
        if (!armor || soldier->getKitType() != 4) continue;
        const int material = kitTemplate->m_abilityInVehicleMaterial;
        const float strength = kitTemplate->m_abilityInVehicleStrength;
        const int targetMaterial = armor->getDefaultMaterial();
        if (gameLogic->heal(target, strength, player, material, targetMaterial, nullptr, true, false))
            repairTargets.insert(target);
    }
    // ISoldier's preceding sibling is the actual IPlayerControlObject face.
    // Use the compiler-adjusted concrete conversion, not retail -8/-15C.
    const auto* weapons = static_cast<const std::vector<IObject*>*>(static_cast<Soldier*>(soldier)->getWeapons(0));
    bool foundWeapon = false;
    bool processed = false;
    for (std::size_t i = 0; i < weapons->size(); ++i) {
        IObject* object = (*weapons)[i];
        if (!object) continue;
        auto* weapon = static_cast<IWeaponObject*>(object->queryInterface(IID_IWeaponObject));
        if (!isReplenishingWeapon(weapon)) continue;
        foundWeapon = true;
        unsigned index = 0;
        for (auto it = repairTargets.begin(); it != repairTargets.end(); ++it, ++index) {
            IObject* target = *it;
            if (!target || !abilityFlag(ability, 1)) continue;
            auto* armor = static_cast<IArmorGLComp*>(target->m_armorComponent);
            if (!armor) continue;
            processed = true;
            const float hitPoints = armor->getHitPoints();
            const float maximum = armor->getMaxHitPoints();
            result = armorAbilityRatio(hitPoints, maximum, result, index != 0);
        }
        break;
    }
    if (!foundWeapon || repairTargets.empty() || (!processed && result == 1.0f))
        result = m_field1ac;
    return result;
}
void HudAmmo::updateAbility(float) // 7A9E70; argument consumed, never read
{
    using namespace world;
    m_field1b0 = false;
    m_field0ee = false;
    m_field1b1 = false;
    world::IPlayer* player = playerManager->getLocalHumanPlayer();
    IObject* kit = player ? hudManager->getPlayerKit(player) : nullptr;
    auto* ability = kit ? static_cast<IAbilityGLComp*>(kit->m_abilityGLComponent) : nullptr;
    IWeaponObject* weapon = nullptr;
    if (player) {
        if (player->getVehicle() && m_field1c2 && findControlParent(player->getVehicle())) {
            bool giving = false;
            if (player->getIsGivingAbility()) {
                const unsigned expires = player->getHealRepairedAtTick() + 15u;
                giving = expires > static_cast<unsigned>(WorldPref::getTickCount());
            }
            if (giving) {
                if (player->getIsRepairing()) {
                    m_field1ac = getRepairAbility(player, ability);
                    m_field1b4 = repairAbilityIcon;
                } else {
                    m_field1b4 = medicAbilityIcon;
                    m_field1ac = 1.0f;
                }
                m_field1b0 = true;
            } else if (player->getIsGivingReplenish()) {
                const unsigned expires = player->getGaveAmmoAtTick() + 15u;
                if (expires > static_cast<unsigned>(WorldPref::getTickCount())) {
                    m_field1b4 = ammoAbilityIcon;
                    m_field1ac = 1.0f;
                    m_field1b0 = true;
                }
            }
            IObject* currentKit = hudManager->getPlayerKit(player);
            if (!currentKit) return;
            auto* sound = static_cast<IAbilityHudComp*>(currentKit->m_abilityHudComponent);
            if (!sound) return;
            sound->setRepairingActive(m_field1b0 && player->getIsGivingAbility() && player->getIsRepairing());
            sound->setHealingActive(m_field1b0 && player->getIsGivingAbility() && !player->getIsRepairing());
            sound->setAmmoActive(m_field1b0 && player->getIsGivingReplenish() && !player->getIsGivingAbility());
            return;
        }
        weapon = findAbilityWeapon(player);
    }
    if (!ability || !weapon) {
        if (!weapon) stopAbilitySounds(kit);
        return;
    }
    if (!isReplenishingWeapon(weapon)) {
        stopAbilitySounds(kit);
        return;
    }
    // Full ELF825B0A0 returns &ReplenishingAmmoComp::m_abilityObjects; its
    // native vector<WeakPtr<IObject>> lifetime and element type already exist.
    const auto* targets = static_cast<const std::vector<WeakPtr<IObject>>*>(weapon->getAbilityObjectList());
    const bool working = weapon->getAbilityWorking();
    float value = 0.0f;
    if (targets) {
        for (std::size_t i = 0; i < targets->size(); ++i) {
            IObject* target = (*targets)[i].get();
            if (!target) continue;
            if (abilityFlag(ability, 2)) {
                if (!target->queryInterface(IID_IPlayerControlObject)) continue;
                auto* control = static_cast<IPlayerControlObject*>(target->queryInterface(IID_IPlayerControlObject));
                const int status = control->getAmmoStatus();
                m_field1b0 = true;
                value = ammoAbilityRatio(status, value, i != 0);
                continue;
            }
            bool swimming = false;
            if (player) {
                auto* soldier = static_cast<ISoldier*>(player->getDefaultVehicle()->queryInterface(IID_ISoldier));
                swimming = static_cast<Soldier*>(soldier)->getIsSwimming(); // PE5AB330, native cast; no guard
            }
            auto* armor = static_cast<IArmorGLComp*>(target->m_armorComponent);
            if (!armor || swimming) continue;
            const float hitPoints = armor->getHitPoints();
            const float maximum = armor->getMaxHitPoints();
            value = armorAbilityRatio(hitPoints, maximum, value, i != 0);
            m_field1b0 = true;
            m_field0ee = true;
        }
    }
    if (player && (!player->getIsAlive() || player->getIsManDown())) {
        stopAbilitySounds(kit);
    } else if (abilityFlag(ability, 2)) {
        const bool shown = m_field1b0;
        if (shown) { m_field1b4 = ammoAbilityIcon; m_field1ac = value; }
        auto* sound = static_cast<IAbilityHudComp*>(kit->m_abilityHudComponent);
        if (sound) sound->setAmmoActive(working && shown);
    } else if (abilityFlag(ability, 1)) {
        const bool shown = m_field1b0;
        if (shown) { m_field1b4 = repairAbilityIcon; m_field1ac = value; }
        auto* sound = static_cast<IAbilityHudComp*>(kit->m_abilityHudComponent);
        if (sound) sound->setRepairingActive(working && shown);
    } else if (abilityFlag(ability, 0)) {
        const bool shown = m_field1b0;
        if (shown) { m_field1b4 = medicAbilityIcon; m_field1ac = value; }
        auto* sound = static_cast<IAbilityHudComp*>(kit->m_abilityHudComponent);
        if (sound) sound->setHealingActive(working && shown);
    }
    m_field1b1 = true;
    const float amount = weapon->getAbility();
    m_field1b8 = amount;
    m_field0b4 = amount;
    if (player && (!player->getIsAlive() || player->getIsManDown())) m_field1b0 = false;
}
namespace {
HudInformationLayer* information()
{ return static_cast<HudInformationLayer*>(g_hudInformationLayer); }
// 789400 (FloatRefData), 7858C0 (BoolRefData): the two identical helper bodies
// differ only in their concrete allocation/constructor. Names are [INFERRED].
template<class Data, class Value>
bool registerReference(meme::System* system, Value* value,
                       const std::string& name, const std::string& container)
{
    void* allocation = ::operator new(sizeof(Data));
    Data* data = allocation ? new(allocation) Data(value, name.c_str()) : nullptr;
    if (data) {
        system->registerData(data, container.c_str());
        return true; // The registration result is discarded.
    }
    const std::string caption = formatString("Failed to register: %s in %s", name.c_str(), container.c_str());
    MessageBoxA(0, "Error RegisterData", caption.c_str(), 0); // Retail text/caption order.
    return false;
}
}
void HudAmmo::setSystem(meme::System* system) { m_system = system; }
bool HudAmmo::registerFunctions() { return true; } // 5A5E20 MOV AL,1;RET
void HudAmmo::unknown0C() {} // 83D270 RET
void HudAmmo::unknown10() {} // 83D270 RET
void HudAmmo::unknown14() {} // 83D270 RET

HudAmmo::~HudAmmo()
{
    eventManager->unregisterEventHandler(ECGame, this);
    eventManager->unregisterEventHandler(ECHud, this);
    if (m_blinkManager) delete m_blinkManager;
    if (m_field17c != &s_emptyString) delete[] m_field17c;
    if (m_field180 != &s_emptyString) delete[] m_field180;
    if (m_field144 != &s_emptyString) delete[] m_field144;
    if (m_field150 != &s_emptyString) delete[] m_field150;
    if (m_field154 != &s_emptyString) delete[] m_field154;
    if (m_field158 != &s_emptyString) delete[] m_field158;
    if (m_field1a4 != &s_emptyString) delete[] m_field1a4;
    if (m_field14c != &s_emptyString) delete[] m_field14c;
    if (m_field148 != &s_emptyString) delete[] m_field148;
    if (m_field160 != &s_emptyString) delete[] m_field160;
    if (m_field16c != &s_emptyString) delete[] m_field16c;
    if (m_field170 != &s_emptyString) delete[] m_field170;
    if (m_field174 != &s_emptyString) delete[] m_field174;
    if (m_field168 != &s_emptyString) delete[] m_field168;
    if (m_field164 != &s_emptyString) delete[] m_field164;
    if (m_field1a0 != &s_emptyString) delete[] m_field1a0;
    if (m_field190 != &s_emptyWideString) delete[] m_field190;
    if (m_field194 != &s_emptyString) delete[] m_field194;
    if (m_field198 != &s_emptyString) delete[] m_field198;
    if (m_field19c != &s_emptyString) delete[] m_field19c;
    // Retail does not release the AbilityIconPath string at +1B4.
}
void HudAmmo::hide(bool updatePointer)
{
    m_field02c = m_field01c;
    m_field018 = false;
    m_field13a = false;
    if (updatePointer) {
        const bool pointer = m_field0a4;
        m_field0e9 = false;
        if (pointer && !hudManager->unknown1E8() && hudManager->isPointerEnabled())
            hudManager->disablePointer();
    }
}
void HudAmmo::updateString(const char** destination, const char* source)
{
    // The compare precedes the old-pointer guard; a null old value is unsafe.
    if (!source || std::strcmp(source, *destination) == 0) return;
    const char* old = *destination;
    if (old && old != &s_emptyString) {
        ::operator delete(const_cast<char*>(old));
        *destination = nullptr;
    }
    const std::size_t bytes = std::strlen(source) + 1;
    char* replacement = static_cast<char*>(::operator new(bytes));
    std::memcpy(replacement, source, bytes);
    *destination = replacement;
}
// Publishes a wide-text HUD value.
void HudAmmo::updateString(const wchar_t** destination, const wchar_t* source)
{
    if (!source || std::wcscmp(source, *destination) == 0) return;
    const wchar_t* old = *destination;
    if (old && old != &s_emptyWideString) {
        ::operator delete(const_cast<wchar_t*>(old));
        *destination = nullptr;
    }
    const std::size_t bytes = (std::wcslen(source) + 1) * sizeof(wchar_t);
    wchar_t* replacement = static_cast<wchar_t*>(::operator new(bytes));
    std::memcpy(replacement, source, bytes);
    *destination = replacement;
}
bool HudAmmo::registerData() // 7A62C0; all return values ignored
{
    {
        const std::string container("BottomRight/Alpha"), name("BottomRight_alpha");
        registerReference<meme::FloatRefData>(m_system, &m_field010, name, container);
    }
    {
        const std::string container("BottomRight"), name("BottomRight_oldXPos");
        registerReference<meme::FloatRefData>(m_system, &m_field028, name, container);
    }
    {
        const std::string container("BottomRight"), name("BottomRight_newXPos");
        registerReference<meme::FloatRefData>(m_system, &m_field02c, name, container);
    }
    {
        const std::string container("BottomRight"), name("BottomRight_direction");
        registerReference<meme::BoolRefData>(m_system, &m_field018, name, container);
    }
    information()->registerBool("BottomRightDirection", &m_field018);
    information()->registerFloat("BottomRightAlpha", &m_field010);
    information()->registerFloat("BottomRightFadedAlpha", &m_field014);
    information()->registerInt("MissileTargetLockId", &m_field098);
    information()->registerFloat("TargetDirection", &m_field178);
    information()->registerFloat("PrimaryAmmo", &m_field0b4);
    information()->registerFloat("SecondaryAmmo", &m_field0c4);
    information()->registerFloat("HitIndicatorIconAlpha", &m_field0fc);
    information()->registerFloat("BombFuel", &m_field0d0);
    information()->registerInt("FiringIndex", &m_field0e4);
    information()->registerInt("CrosshairUpPos", &m_field10c);
    information()->registerInt("CrosshairDownPos", &m_field110);
    information()->registerInt("CrosshairLeftPos", &m_field114);
    information()->registerInt("CrosshairRightPos", &m_field118);
    information()->registerBool("TVBlink", &m_field12c);
    information()->registerBool("SecondaryHeatShow", &m_field0ec);
    information()->registerFloat("SecondaryHeatValue", &m_field0f0);
    information()->registerBool("PrimaryHeatShow", &m_field0ed);
    information()->registerFloat("PrimaryHeatValue", &m_field0f4);
    information()->registerBool("PrimaryGrenadeLoadShow", &m_field0bf);
    information()->registerBool("SecondaryGrenadeLoadShow", &m_field0c0);
    information()->registerFloat("SecondaryReloadTime", &m_field0c8);
    information()->registerBool("SecondaryReloadTimeShow", &m_field0cc);
    information()->registerBool("SecondaryEternalShow", &m_field0cd);
    information()->registerBool("SecondaryEternalMagsShow", &m_field0ce);
    information()->registerBool("PrimaryEternalShow", &m_field0bd);
    information()->registerBool("PrimaryEternalMagsShow", &m_field0be);
    information()->registerBool("PrimaryReloadTimeShow", &m_field0bc);
    information()->registerFloat("PrimaryReloadTime", &m_field0b8);
    information()->registerBool("PrimarySingleFireShow", &m_field0c1);
    information()->registerBool("SecondarySingleFireShow", &m_field0cf);
    information()->registerBool("PrimaryAmmoBarShow", &m_field0ac);
    information()->registerBool("PrimaryClipsShow", &m_field0ad);
    information()->registerBool("PrimaryAmmoShow", &m_field12d);
    information()->registerBool("PrimaryAmmoBlink", &m_field12e);
    information()->registerBool("PrimaryHeatBlink", &m_field12f);
    information()->registerBool("PrimaryAmmoIconShow", &m_field130);
    information()->registerString("PrimaryAmmoIconPathString", &m_field150);
    information()->registerString("PrimaryAmmoString", &m_field144);
    information()->registerBool("PrimaryClipStringShow", &m_field134);
    information()->registerBool("PrimaryClipBlink", &m_field135);
    information()->registerString("PrimaryClipPathString", &m_field14c);
    information()->registerString("PrimaryClipString", &m_field148);
    information()->registerFloat("PrimaryClips", &m_field140);
    information()->registerBool("PrimaryClip1Show", &m_field131);
    information()->registerBool("PrimaryClip2Show", &m_field132);
    information()->registerBool("PrimaryClip3Show", &m_field133);
    information()->registerString("PrimaryAmmoBarFrontPathString", &m_field154);
    information()->registerString("PrimaryAmmoBarBackPathString", &m_field158);
    information()->registerBool("SecondaryAmmoBarShow", &m_field0ae);
    information()->registerBool("SecondaryClipsShow", &m_field0af);
    information()->registerBool("SecondaryAmmoShow", &m_field136);
    information()->registerBool("SecondaryAmmoBlink", &m_field137);
    information()->registerBool("SecondaryHeatBlink", &m_field138);
    information()->registerBool("SecondaryAmmoIconShow", &m_field139);
    information()->registerString("SecondaryAmmoIconPathString", &m_field16c);
    information()->registerString("SecondaryAmmoString", &m_field160);
    information()->registerBool("SecondaryClipStringShow", &m_field13e);
    information()->registerBool("SecondaryClipBlink", &m_field13f);
    information()->registerString("SecondaryClipPathString", &m_field168);
    information()->registerString("SecondaryClipString", &m_field164);
    information()->registerFloat("SecondaryClips", &m_field15c);
    information()->registerBool("SecondaryClip1Show", &m_field13b);
    information()->registerBool("SecondaryClip2Show", &m_field13c);
    information()->registerBool("SecondaryClip3Show", &m_field13d);
    information()->registerString("SecondaryAmmoBarFrontPathString", &m_field170);
    information()->registerString("SecondaryAmmoBarBackPathString", &m_field174);
    information()->registerString("PrimaryWeaponNameString", &m_field17c);
    information()->registerString("SecondaryWeaponNameString", &m_field180);
    information()->registerFloat("LaserTargetRed", &m_field0d4);
    information()->registerFloat("LaserTargetGreen", &m_field0d8);
    information()->registerFloat("LaserTargetBlue", &m_field0dc);
    information()->registerFloat("LaserTargetAlpha", &m_field0e0);
    information()->registerFloat("PrimaryAmmoColorRed", &m_field030);
    information()->registerFloat("PrimaryAmmoColorGreen", &m_field034);
    information()->registerFloat("PrimaryAmmoColorBlue", &m_field038);
    information()->registerFloat("PrimaryAmmoColorAlpha", &m_field03c);
    information()->registerFloat("PrimaryClipColorRed", &m_field040);
    information()->registerFloat("PrimaryClipColorGreen", &m_field044);
    information()->registerFloat("PrimaryClipColorBlue", &m_field048);
    information()->registerFloat("PrimaryClipColorAlpha", &m_field04c);
    information()->registerFloat("SecondaryAmmoColorRed", &m_field050);
    information()->registerFloat("SecondaryAmmoColorGreen", &m_field054);
    information()->registerFloat("SecondaryAmmoColorBlue", &m_field058);
    information()->registerFloat("SecondaryAmmoColorAlpha", &m_field05c);
    information()->registerFloat("SecondaryClipColorRed", &m_field060);
    information()->registerFloat("SecondaryClipColorGreen", &m_field064);
    information()->registerFloat("SecondaryClipColorBlue", &m_field068);
    information()->registerFloat("SecondaryClipColorAlpha", &m_field06c);
    information()->registerFloat("PrimaryHeatColorRed", &m_field070);
    information()->registerFloat("PrimaryHeatColorGreen", &m_field074);
    information()->registerFloat("PrimaryHeatColorBlue", &m_field078);
    information()->registerFloat("PrimaryHeatColorAlpha", &m_field07c);
    information()->registerFloat("SecondaryHeatColorRed", &m_field080);
    information()->registerFloat("SecondaryHeatColorGreen", &m_field084);
    information()->registerFloat("SecondaryHeatColorBlue", &m_field088);
    information()->registerFloat("SecondaryHeatColorAlpha", &m_field08c);
    information()->registerFloat("FlaresReloadTime", &m_field188);
    information()->registerBool("FlaresReloadTimeShow", &m_field184);
    information()->registerBool("CrosshairIconShow", &m_field0e8);
    information()->registerBool("CrosshairActive", &m_field0e9);
    information()->registerBool("HitIndicatorIconShow", &m_field0eb);
    information()->registerBool("WrenchRepairingShow", &m_field0ee);
    information()->registerWideString("TargetDistanceString", &m_field190);
    information()->registerString("TargetCoordXString", &m_field194);
    information()->registerString("TargetCoordYString", &m_field198);
    information()->registerString("TargetCoordZString", &m_field19c);
    information()->registerString("CrosshairIconPathString", &m_field1a0);
    information()->registerBool("FireRateIconShow", &m_field18c);
    information()->registerString("FireRateIconPathString", &m_field1a4);
    information()->registerString("AbilityIconPath", &m_field1b4);
    information()->registerBool("AbilityBarShow", &m_field1b0);
    information()->registerFloat("AbilityBar", &m_field1ac);
    information()->registerFloat("AbilityValue", &m_field1b8);
    information()->registerBool("AbilityValueShow", &m_field1b1);
    information()->registerBool("VoiceOverIconShow", &m_field1bc);
    information()->registerBool("VoiceOverMuted", &m_field1c0);
    information()->registerBool("VoiceOverTalkIconShow", &m_field1bd);
    information()->registerBool("VoiceOverDistIconShow", &m_field1be);
    information()->registerBool("VoiceOverCommanderIconShow", &m_field1bf);
    information()->registerBool("HasMissileConnection", &m_field1c1);
    {
        const std::string path = formatString("Ingame/GeneralIcons/empty.tga");
        updateString(&m_field158, path.c_str());
    }
    {
        const std::string path = formatString("Ingame/GeneralIcons/empty.tga");
        updateString(&m_field154, path.c_str());
    }
    return true;
}
} } }
#endif
