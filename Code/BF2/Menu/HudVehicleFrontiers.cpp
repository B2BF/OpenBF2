// Vehicle HUD layout, occupant display and warning handling.
// [RETIRE-WHEN: hud-vehicle-leaf-frontiers] Complete and validate the corresponding HudVehicleFrontiers operation.
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
#include "../Platform/X87Compat.h" // [CHOSEN, not recovered] x87 conversion results, including invalid inputs.
#endif
#include "HudVehicleInformation.h"
#if !DEDICATED
#include "HudVehiclePostProcessing.h"
#include "Bf2MenuMapNode.h"                  // the zoom arm reads four private members
#include "BF2HudManager.h"                   // getPlayerVehicleType (+0x74), getMessages (+0x25C), setMode (+0x1DC)
#include "HudInformationAdapter.h"    // getFloat (+0x24) - updateLayout's menu dim
#include "HudColors.h"                       // textColors / squadChatColors
#include "../Core/StringUtil.h"              // formatString (PE 4310D0)
#include "../Core/Ids.h"                     // IID_IPlayerControlObject
#include "../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"  // getPcos / isOccupied
#include <map>
#include <string>
#include <cstring>
#include "BfOccupiedVehicleData.h"            // setOccupantIcon (7C8DB0)
#include "../Game/Objects/IObjectTemplate.h"  // getClassID / queryInterface on m_template
#include "../Core/EventManager.h"            // postEvent (ECHud 0x1F)
#include "../Game/IEventListener.h"          // ECHud
#include <cmath>                             // ceil (MSVCR71 import 87F5F4)
#include "../Game/Objects/GameLogicComp/IArmorGLComp.h"  // getHitPoints / getMaxHitPoints
#include "../Physics/IPhysicsNode.h"          // getPositionalSpeed (PE +0x5C = ELF +0x60)
#include "../Math/Mat4.h"
#include "../Game/Objects/ICameraObject.h" // getTransformation (+0x10)
#include "../Game/Objects/ObjectLayoutAccess.h" // LayoutAccess::parent - the 440060 walk
#include "../Game/Main/BF2ClientSeams.h"     // hudManager (ds:0xA10890)
#include "../Scene/Object/PlayerManager.h"   // world::playerManager
#include "../Game/Objects/IPlayer.h"
#include "HudMessages.h"                         // m_duration (+0x18)
#include "../Game/Objects/IObject.h"             // m_template (+0x14), m_vehicleHudComponent (+0x54)
#include "../Game/Objects/IVehicleHudComp.h"     // getHudInfo (+0x64)
#include "../Game/Objects/VehicleHudCompTemplate.h"  // VehicleHudInfo::displaySelectionOnEnter
#include "../Game/Objects/Game/Kit.h"          // the kit's +0x54 vehicle HUD component
#include "../Game/Objects/ISoldier.h"          // getCanFireWeapon (+0xAC)
#include "../Physics/HeightmapCluster.h"       // getSeaWaterLevel (PE +0x60 / ELF +0x64)
#include "../Game/Objects/Game/RotationalBundle.h"    // getCurrentRotation (PE +0x17C / ELF +0x184)
#include "../Game/Objects/RotationalBundleTemplate.h" // m_maxRotation (PE +0x238 / ELF +392)
#include "../Game/Objects/Game/Soldier.h"      // the Soldier -> IPlayerControlObject face (PE +0x154)
#include "../Game/Objects/GenericFireArmTemplate.h"   // the four crosshair fields (PE +0x2F0..+0x2FC)
#include "../Game/Objects/WarningHudCompTemplate.h"   // IWarningHudComp +0x38..+0x44, WarningHudInfo
#include "../Game/IGame.h"                     // world::MissileLockMessage
#include <cstdio>
#include <cstdlib>

namespace dice { namespace hfe { namespace world {
// Engine-owned heightmap cluster.
extern IBase* heightmapCluster;
// Searches siblings before descending into children for either class ID.
IObject* internalFindFirstChildOfCID(IObject* object, unsigned int firstCid, unsigned int secondCid);
} } }

namespace {
// the walk UP the parent chain for a control object - the same loop
// GameClient::enterVehicle and Player::_setVehicle spell out inline. [INFERRED] name.
// note: this anonymous namespace is at global scope, so everything here is fully qualified.
dice::hfe::world::IPlayerControlObject* findControlObject(dice::hfe::world::IObject* object)
{
    namespace world = dice::hfe::world;
    for (world::IObject* node = object; node;
         node = world::object_detail::LayoutAccess::parent(node))
    {
        if (void* const pco = node->queryInterface(world::IID_IPlayerControlObject))
            return static_cast<world::IPlayerControlObject*>(pco);
    }
    return 0;
}

// the shared float clamp, upper bound tested first, then the lower bound - the same helper
// Game/Main/BF2FrameInterpolator.cpp and Game/GameServerSettings.cpp already spell out.

// HUD cream and red colors; literals retain the original floating-point values.
const float kHudCreamR = 0.827451f;
const float kHudCreamG = 0.823529f;
const float kHudCreamB = 0.701961f;
const float kHudCreamA = 1.0f;
const float kHudRedR   = 0.949020f;
const float kHudRedG   = 0.184314f;
const float kHudRedB   = 0.184314f;
const float kHudRedA   = 1.0f;

// Clamps value to [low, high], mapping NaN to low.
inline float clampFloat(float value, float low, float high)
{
    float result = value;
    if (value >= high) result = high;
    if (!(result >= low)) return low;
    return result;
}

// The published "PostProductionFrameWork" singleton is RendDX9's owner+8 subobject, whose raw
// table is not an IBase table (slot 0 is its deleting destructor, +0x04.. are float get/set pairs),
// so the four dispatchers at the foot of this file reach it through this isolated raw-slot helper -
// the shape Menu/Ref2System.cpp's slot<> already uses for the same client/plugin boundary.
// Evidence for every slot is at the dispatchers.
typedef void (__thiscall* PostProductionSetFloat)(dice::hfe::IBase* framework, float value);
inline PostProductionSetFloat postProductionSetter(dice::hfe::IBase* framework, unsigned int byteOffset)
{
    return reinterpret_cast<PostProductionSetFloat>(
        (*reinterpret_cast<void***>(framework))[byteOffset / 4]);
}
} // namespace

namespace dice { namespace hfe { namespace hud {

// Updates the vehicle HUD layout.
void HudVehicleInformation::updateLayout()
{
    // which panel is showing -------------------------------------------------------------
    if (m_field178 == m_field184)
        m_field174 = 0;
    if (m_bottomLeftHealthAlpha == 1.0f)
        m_field174 = 1;
    if (m_bottomLeftVehicleAlpha == 1.0f)
        m_field174 = 2;

    // where the panel should slide to ----------------------------------------------------
    switch (m_field170)
    {
    case 0:
        if (m_bottomLeftVehicleAlpha != 0.0f)
        {
            m_field1a0 = 0.0f;
        }
        else
        {
            m_field188 = m_field17c;            // unconditional - see the note above
            if (m_field184 <= m_field17c)
            {
                if (m_bottomLeftHealthAlpha == 0.0f)
                    m_field188 = m_field178;
                else
                    m_field19c = 0.0f;
            }
        }
        break;

    case 1:
        if (m_field184 < m_field17c)
        {
            m_field188 = m_field17c;
        }
        else
        {
            m_field19c = 1.0f;
            if (m_bottomLeftVehicleAlpha == 0.0f)
                m_field188 = m_field17c;
            else
                m_field1a0 = 0.0f;
        }
        break;

    case 2:
        if (m_field184 < m_field17c)
        {
            m_field188 = m_field17c;
        }
        else
        {
            m_field19c = 1.0f;                  // stored before the branch below, as shipped
            if (m_bottomLeftHealthAlpha == 1.0f)
            {
                if (m_field184 < m_field180)
                    m_field188 = m_field180;
                else
                    m_field1a0 = 1.0f;
            }
        }
        break;

    default:
        break;                                  // any other mode leaves all of it alone
    }

    // the menu background dims both bottom-left gauges ------------------------------------
    float menuBackgroundAlpha = 1.0f;
    if (hudManager->getInformationAdapter() && m_field174 != 0)     // +0x244, fetched twice
    {
        menuBackgroundAlpha = hudManager->getInformationAdapter()->getFloat(
            std::string("MenuBackgroundAlpha"));                    // +0x24, ds:0x89EDC8
    }
    const float dim = 1.0f - menuBackgroundAlpha;

    float health = m_bottomLeftHealthAlpha - dim;
    if (health < 0.0f)      health = 0.0f;      // lower bound first
    else if (health > 1.0f) health = 1.0f;
    m_bottomLeftHealthFadedAlpha = health;

    float vehicle = m_bottomLeftVehicleAlpha - dim;
    if (vehicle < 0.0f)      vehicle = 0.0f;
    else if (vehicle > 1.0f) vehicle = 1.0f;
    m_bottomLeftVehicleFadedAlpha = vehicle;
}

// Refreshes the player information shown in the vehicle HUD.
void HudVehicleInformation::updatePlayerInformation(world::IObject* soldier)
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    if (!soldier) return;

    if (soldier->m_armorComponent)                                        // +0x3C
    {
        world::IArmorGLComp* const armor =
            static_cast<world::IArmorGLComp*>(soldier->m_armorComponent);

        // the injury sound ------------------------------------------------------------
        const float ratio = armor->getHitPoints() / armor->getMaxHitPoints();  // +3C / +84
        world::Kit* const kit = hudManager->getPlayerKit(player);              // +0x90
        if (kit && kit->m_vehicleHudComponent)                                 // Kit +0x54
        {
            world::IVehicleHudComp* const kitHud =
                static_cast<world::IVehicleHudComp*>(kit->m_vehicleHudComponent);
            if (ratio < m_playerHealth && ratio != 0.0f && m_field0bc <= 0.0f) {
                kitHud->playInjurySound();                                     // +0x5C
                m_field0bc = 2.0f;
            } else if ((ratio > m_playerHealth || ratio == 0.0f) && m_field0bc >= 0.0f) {
                m_field0bc = -1.0f;
            }
        }

        // publish it ------------------------------------------------------------------
        const double published =
            static_cast<double>(armor->getHitPoints()) / armor->getMaxHitPoints();
        m_playerHealth = static_cast<float>(published);                        // +0x1A8
        assignString(m_playerHealthString,                                     // +0x270
                     formatString("%i", static_cast<int>(
                         std::ceil(published * 100.0))).c_str());              // ds:89127C

        hudManager->selectFloatColor(                                          // +0x8C
            m_playerHealth, 0.3f,
            m_playerHealthColor[0], m_playerHealthColor[1],
            m_playerHealthColor[2], m_playerHealthColor[3],
            kHudCreamR, kHudCreamG, kHudCreamB, kHudCreamA,
            kHudRedR, kHudRedG, kHudRedB, kHudRedA);
        hudManager->selectFloatColor(
            m_playerStamina, 0.05f,
            m_playerSprintColor[0], m_playerSprintColor[1],
            m_playerSprintColor[2], m_playerSprintColor[3],
            kHudCreamR, kHudCreamG, kHudCreamB, kHudCreamA,
            kHudRedR, kHudRedG, kHudRedB, kHudRedA);
        hudManager->selectFloatColor(
            m_nightVisionGauge, 0.05f,
            m_nightVisionColor[0], m_nightVisionColor[1],
            m_nightVisionColor[2], m_nightVisionColor[3],
            kHudCreamR, kHudCreamG, kHudCreamB, kHudCreamA,
            kHudRedR, kHudRedG, kHudRedB, kHudRedA);
    }

    // sprint state, the stamina gauge and the panting sound -----------------------------
    world::IPlayerControlObject* const control = static_cast<world::IPlayerControlObject*>(
        soldier->queryInterface(world::IID_IPlayerControlObject));
    if (control)
    {
        world::ISoldier* const soldierFace = static_cast<world::ISoldier*>(
            control->queryInterface(world::IID_ISoldier));

        if (m_field244 != control->getIsSprinting()                            // PCO +0xF8
            || (soldierFace && m_field2ac != soldierFace->getCanFireWeapon()))  // ISoldier +0xAC
        {
            m_field244 = control->getIsSprinting();                            // asked again
            m_field2ac = soldierFace->getCanFireWeapon();                       // see note 1
            eventManager->postEvent(ECHud, 0x1F, 0, 0.0f);
        }

        // A player in any vehicle reads a full bar rather than the object's own stamina.
        m_playerStamina = m_field293 ? 1.0f : control->getSprintStamina();      // PCO +0x100
        m_playerStaminaShow = control->getIsSprinting() || m_playerStamina < 1.0f;
        if (m_playerStaminaShow)
        {
            hudManager->disableVOHelpMessage(70);                              // +0xFC
            world::Kit* const kit = hudManager->getPlayerKit(player);
            if (kit && kit->m_vehicleHudComponent)
            {
                if (m_playerStamina <= 0.0f && !player->getIsGasMaskOn())      // IPlayer +0x2BC
                    static_cast<world::IVehicleHudComp*>(kit->m_vehicleHudComponent)
                        ->playingPantingSound(true);                           // +0x54
            }
        }
    }

    // night vision, then the squad badge -----------------------------------------------
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();  // a fresh fetch
    if (local) {
        m_nightVisionGauge = local->getNormalizedNightVisionPower();            // IPlayer +0x2D4
        m_nightVisionGaugeShow = m_nightVisionGauge != 1.0f;
    }

    // see note 2 - the guard above does not extend to here.
    if (local->getIsCommander() && m_playerHealthShow                           // IPlayer +0x110
        && !hudManager->isCommanderShown())                                     // +0x1EC
    {
        m_squadInfoBarShow  = true;
        m_showCommanderIcon = true;
        m_showSquadIcon     = false;
        assignString(m_squadInfoText,
                     hudManager->getLocalizedString("HUD_HUD_SQUADINFO_COMMANDER").c_str());
        assignString(m_squadInfoIconPath, "Ingame/Kits/Icons/kit_Commander.tga");
        return;
    }

    if ((local->getIsSquadLeader() || local->getSquadId() != 0)                 // +0x138 / +0x108
        && m_playerHealthShow && !hudManager->isCommanderShown())
    {
        m_squadInfoBarShow  = true;
        m_showCommanderIcon = false;
        m_showSquadIcon     = true;
        assignString(m_squadInfoIconPath,
                     formatString("Ingame/Player/Icons/Minimap/mini_Squad_%d_small.tga",
                                  local->getSquadId()).c_str());
        if (local->getIsSquadLeader()) {
            assignString(m_squadInfoText,
                         hudManager->getLocalizedString("HUD_HUD_SQUADINFO_SQUADLEADER").c_str());
            return;
        }
        if (local->getSquadId() == 0) return;                                   // redundant
        assignString(m_squadInfoText,
                     hudManager->getLocalizedString("HUD_HUD_SQUADINFO_SQUADMEMBER").c_str());
        return;
    }

    m_squadInfoBarShow  = false;
    m_showCommanderIcon = false;
    m_showSquadIcon     = false;
    assignString(m_squadInfoText, L"");                                         // ds:8A3024
    assignString(m_squadInfoIconPath, "");                                      // ds:87FB54
}

// Updates the player's displayed orientation.
void HudVehicleInformation::updatePlayerAngles(world::IPlayer* player)
{
    if (!player->getCamera()) return;                            // PE IPlayer +17C
    const Mat4 transform = player->getCamera()->getTransformation(0.0f);   // +0x10

    m_playerElevation = transform.m[9];                          // the PE's store order
    m_playerAngle     = std::atan2(transform.m[8], transform.m[10]);
    m_playerBanking   = -std::acos(clampFloat(transform.m[1], -1.0f, 1.0f));
}

// Advances and returns the occupant-display timer.
float HudVehicleInformation::advanceOccupantTimer(float deltaTime)
{
    if (m_field020)                                  // +0x20, armed
    {
        m_field018 += deltaTime;                     // +0x18, elapsed
        if (m_field018 <= m_field01c)                // +0x1C, the window
            return m_field018;
        m_field020 = false;
        m_field01c = 0.0f;
        m_field018 = 0.0f;
        return 0.0f;
    }
    m_field020 = false;
    m_field01c = 0.0f;
    m_field018 = 0.0f;
    return m_field018;
}

// Updates the vehicle armor fraction, bar colors and icon. Requires a non-null vehicle.
// The hit-point ratio is published without checking for a zero maximum.
void HudVehicleInformation::updateVehicleArmor(world::IObject* vehicle)
{
    if (vehicle)
    {
        if (m_vehicleArmorShow && vehicle->m_armorComponent)            // +0x250 / +0x3C
        {
            world::IArmorGLComp* const armor =
                static_cast<world::IArmorGLComp*>(vehicle->m_armorComponent);
            const float hitPoints = armor->getHitPoints();              // +0x3C
            m_vehicleArmor = hitPoints / armor->getMaxHitPoints();      // +0x84, no zero check
            assignString(m_vehicleArmorString,                          // +0x26C
                         formatString("%i", static_cast<int>(
                             std::ceil(m_vehicleArmor * 100.0f))).c_str());

            hudManager->selectFloatColor(                               // +0x8C
                m_vehicleArmor, 0.3f,
                m_vehicleArmorColor[0], m_vehicleArmorColor[1],
                m_vehicleArmorColor[2], m_vehicleArmorColor[3],
                kHudCreamR, kHudCreamG, kHudCreamB, kHudCreamA,   // ds:97DDD0
                kHudRedR, kHudRedG, kHudRedB, kHudRedA);          // ds:97DDE0
        }

        if (m_vehicleStaminaShow)                                       // +0x249
        {
            hudManager->selectFloatColor(
                m_vehicleStamina, 0.05f,                                // +0x1B4
                m_vehicleSprintColor[0], m_vehicleSprintColor[1],       // +0x224
                m_vehicleSprintColor[2], m_vehicleSprintColor[3],
                kHudCreamR, kHudCreamG, kHudCreamB, kHudCreamA,
                kHudRedR, kHudRedG, kHudRedB, kHudRedA);
        }
    }

    // the shared tail, reached from every path above including the null one - see the note.
    if (vehicle->m_vehicleHudComponent)                                 // +0x54
    {
        assignString(m_vehicleIconPath,                                 // +0x27C
                     static_cast<world::IVehicleHudComp*>(
                         vehicle->m_vehicleHudComponent)->getVehicleIcon(0).c_str());   // +0x40
    }
}
// Refreshes the vehicle's HUD information.
void HudVehicleInformation::updateVehicleInformation(world::IObject* vehicle, float deltaTime)
{
    if (!vehicle)
        return;

    updateVehicleArmor(world::getRootParent(vehicle));

    world::IObject* const root = world::getRootParent(vehicle);        // 71AE20, the second call
    if (root && root->m_vehicleHudComponent)                           // +0x54
    {
        world::IVehicleHudComp* const comp =
            static_cast<world::IVehicleHudComp*>(root->m_vehicleHudComponent);
        assignString(m_vehicleName, comp->getHudInfo()->hudName.c_str());   // +0x278 / info +0x20
        m_maxClimbSpeed = comp->getHudInfo()->maxClimbSpeed;           // getHudInfo again, +0xBC
    }

    // The torque needle: smooth toward the raw input, then publish it as text and as an angle.
    const float target = (m_unknown28c > 0.0f) ? m_unknown28c : 0.0f;  // +0x28C
    m_torque   = m_torque + (target - m_field1e8) * 0.01f;             // +0x1E4 / +0x1E8
    m_field1e8 = m_torque;                                             // both get the new value
    assignString(m_torqueString,                                       // +0x288
                 formatString("%i", static_cast<int>(
                     std::ceil(m_torque * 100.0f))).c_str());          // ceil, MSVCR71 87F5F4
    m_torqueAngle = static_cast<float>(m_torque * 6.2831854820251465); // +0x1EC, 2*pi as a double

    if (vehicle->m_vehicleHudComponent)                                // the argument, not the root
        m_turretIconShow = static_cast<world::IVehicleHudComp*>(
            vehicle->m_vehicleHudComponent)->getHudInfo()->hasTurretIcon;   // +0x252 / info +0x90
    else
        m_turretIconShow = false;

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (!local || vehicle == local->getDefaultVehicle())               // IPlayer PE +0xA0
    {
        m_vehiclePassengersShow = false;                               // +0x24E
        return;
    }

    // through the manager, not through `this` - see the note above.
    hudManager->getVehicleInformation()->updateOccupantIcons(vehicle);
    m_vehiclePassengersShow =
        (hudManager->getVehicleInformation()->advanceOccupantTimer(deltaTime) > 0.0f);

    world::IObject* const sprintRoot = world::getRootParent(vehicle);  // the third call
    if (!sprintRoot)
        return;
    world::IPlayerControlObject* const pco =
        static_cast<world::IPlayerControlObject*>(
            sprintRoot->queryInterface(world::IID_IPlayerControlObject));
    if (!pco)
        return;

    if (m_field244 != pco->getIsSprinting())                           // +0x244 / ipco +0xF8
    {
        m_field244 = pco->getIsSprinting();                            // asked again
        eventManager->postEvent(ECHud, 0x1F, 0, 0.0f);
    }

    const float stamina = m_field293 ? pco->getSprintStamina()         // +0x293 / ipco +0x100
                                     : 1.0f;
    m_vehicleStamina     = stamina;                                    // +0x1B4
    m_vehicleStaminaShow = m_field244 || (stamina < 1.0f);             // +0x249
}

// update heading, bank, elevation, altitude, speed and vehicle needles.
// Altitude uses the unguarded HeightmapCluster sea level. Turret lookup uses getActiveWeapon(0).
// The last yawing rotational bundle wins; do not clear gunner angle when none qualifies.
// Preserve both gunner-angle stores and both velocity fetches. Speed truncates before *10;
// climb speed has no zero guard. Double intermediates retain the shipped rounding boundaries.
void HudVehicleInformation::updateVehicleAngles(world::IObject* vehicle)
{
    if (!vehicle)
        return;

    const Mat4& m = vehicle->getAbsoluteTransformation();               // PE +0x80 / ELF +0x84

    m_vehicleAngle = static_cast<float>(std::atan2(static_cast<double>(m.m[8]),
                                                   static_cast<double>(m.m[10])));   // +0x1C8, fpatan
    m_vehicleElevation = m.m[9];                                        // +0x1D4, a raw dword move
    m_vehicleBanking = static_cast<float>(                              // +0x1D0 = -asin(...)
        -(static_cast<double>(1.57079637f)                              // ds:0x9313C4
          - std::acos(static_cast<double>(clampFloat(m.m[1], -1.0f, 1.0f)))));
    m_angleOfAttack = static_cast<int>(std::ceil(                       // +0x1F0, reads +0x1D4 back
        static_cast<double>(m_vehicleElevation) * 100.0 + 296.0));      // ds:89127C / ds:931C48

    // ds:0xA07E28 = world::heightmapCluster, not null-tested; PE +0x60 = ELF +0x64.
    const float seaLevel =
        static_cast<world::IHeightmapCluster*>(world::heightmapCluster)->getSeaWaterLevel();
    assignString(m_altitudeString,                                      // +0x284
                 formatString("%i", static_cast<int>(std::ceil(         // ds:89E620
                     static_cast<double>(m.m[13]) - static_cast<double>(seaLevel)))).c_str());

    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();   // PlayerManager +0x30
    if (local && vehicle != local->getDefaultVehicle())                 // IPlayer PE +0xA0
    {
        // 440060 is the walk-UP-the-parents search for IID_IPlayerControlObject - the same loop
        // GameClient::enterVehicle and Player::_setVehicle spell out inline.
        world::IPlayerControlObject* const seatPco = findControlObject(vehicle);
        world::IObject* const root = world::getRootParent(vehicle);
        world::IPlayerControlObject* const rootPco = root ? findControlObject(root) : 0;
        if (rootPco && seatPco)
        {
            const std::map<int, world::IPlayerControlObject*> seats = *rootPco->getPcos();  // +0x60
            for (std::map<int, world::IPlayerControlObject*>::const_iterator it = seats.begin();
                 it != seats.end(); ++it)
            {
                world::IPlayerControlObject* const seat = it->second;
                if (!seat
                    || seat->queryInterface(world::IID_ISoldier)        // ds:91C45C - soldiers skipped
                    || seat == seatPco)                                 // and so is our own seat
                    continue;

                // The rotational-bundle walk (78CCBA..78CD43) - see seam 2 above.
                for (world::IObject* node = static_cast<world::IObject*>(
                         seat->queryInterface(world::IID_IObject));     // ds:91C16C
                     node != 0; )
                {
                    world::IObject* const bundle = world::internalFindFirstChildOfCID(
                        node->getChild(),                               // PE +0xEC
                        world::CID_RotationalBundleTemplate,            // edx, ds:91C200
                        world::CID_RotationalBundleTemplate);           // the stack word, ds:91C200
                    if (!bundle)
                        break;
                    node = bundle;                                      // 78CD07, before the test
                    const world::RotationalBundleTemplate* const bundleTemplate =
                        static_cast<const world::RotationalBundleTemplate*>(bundle->getTemplate());
                    if (bundleTemplate->m_maxRotation.x != 0.0f)        // +0x238; NaN counts as != 0
                    {
                        const float yaw =
                            static_cast<world::RotationalBundle*>(bundle)->getCurrentRotation().x
                            * 0.0174532924f;                            // PE +0x17C, ds:8F1C0C
                        m_gunnerAngle = yaw;                            // `fst` - the raw value first
                        m_gunnerAngle = clampFloat(-yaw, -3.14159274f, 3.14159274f);  // 402EE0, +-pi
                    }
                }
            }

            // PE +0x74 = getActiveWeapon(int) - see the settled note above.
            if (world::IObject* const weapon = seatPco->getActiveWeapon(0))
            {
                const Mat4& wm = weapon->getAbsoluteTransformation();   // PE +0x80
                m_vehicleTurretAngle = static_cast<float>(              // +0x1CC
                    std::atan2(static_cast<double>(wm.m[8]), static_cast<double>(wm.m[10]))
                    - static_cast<double>(m_vehicleAngle));
            }
        }                                                               // the copy dies here
    }

    if (vehicle->m_physicsNode)                                         // IObject +0x44
    {
        const Vec3& velocity =
            static_cast<world::IPhysicsNode*>(vehicle->m_physicsNode)->getPositionalSpeed();  // PE +0x5C
        const double vx = velocity.x;
        const double vy = velocity.y;
        const double vz = velocity.z;
#if (defined(__APPLE__) || defined(BF2_NATIVE_X87))
        const int speed = bf2x87::toIntTruncate(std::sqrt((vx * vx + vy * vy) + vz * vz));   // fsqrt, fistp
#else
        const int speed = static_cast<int>(std::sqrt((vx * vx + vy * vy) + vz * vz));   // fsqrt, fistp
#endif
        assignString(m_speedString,                                     // +0x280
                     formatString("%i", speed * 10).c_str());           // *10 after the truncation

        // m_physicsNode is RE-read and the velocity fetched a second time (78CE1E), as shipped.
        const Vec3& climb =
            static_cast<world::IPhysicsNode*>(vehicle->m_physicsNode)->getPositionalSpeed();
        const float needle = static_cast<float>(
            12.566370964050293 / static_cast<double>(m_maxClimbSpeed)   // fild +0x9C, fdivr 931C40
            * static_cast<double>(climb.y)
            * static_cast<double>(0.1f)                                 // ds:8A9D40
            - static_cast<double>(m_unknown1dc)
            + static_cast<double>(m_vehicleElevationSpeedAngle));
        m_vehicleElevationSpeedAngle = needle;                          // +0x1D8, fst
        m_unknown1dc = needle;                                          // +0x1DC, fstp
    }
}

// Updates icons for occupants and available vehicle positions.
void HudVehicleInformation::updateOccupantIcons(world::IObject* vehicle)
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();

    world::IObject* const root = world::getRootParent(vehicle);
    if (!root)
        return;
    world::IPlayerControlObject* const rootPco =
        static_cast<world::IPlayerControlObject*>(
            root->queryInterface(world::IID_IPlayerControlObject));
    if (!rootPco)
        return;

    const std::map<int, world::IPlayerControlObject*> seats = *rootPco->getPcos();   // +0x60

    for (int i = 0; i < 10; ++i)
        m_occupiedData->setOccupantIcon(i, 0);

    int slot = 0;                       // never compared against ten - see the note
    for (std::map<int, world::IPlayerControlObject*>::const_iterator it = seats.begin();
         it != seats.end(); ++it, ++slot)
    {
        world::IPlayerControlObject* const seat = it->second;
        int icon = 0;

        if (!seat->isOccupied())                                           // ipco +0x18
        {
            icon = 4;
        }
        else
        {
            world::IPlayer* const occupant = seat->getOccupingPlayer();    // ipco +0x10
            if (occupant && local)
            {
                if (occupant == local)
                {
                    icon = (local->getSquadId() != 0) ? 1 : 2;             // IPlayer +0x108
                }
                else if (occupant->getSquadId() != 0
                         && occupant->getSquadId() == local->getSquadId()) // asked twice
                {
                    icon = 7;
                }
                else
                {
                    icon = (occupant->getTeam() != local->getTeam()) ? 6 : 5;   // +0xE4
                }

                if (world::IObject* const ridden = occupant->getVehicle())  // IPlayer +0x44
                {
                    // the result is discarded, and m_template is read again below - both shipped.
                    ridden->getTemplate()->queryInterface(
                        world::IID_IPlayerControlObjectTemplate);
                    m_usingParachute = (ridden->getTemplate()->getClassID()
                                        == world::CID_ParachuteTemplate);  // +0xB8, ds:91C46C
                }
            }
        }

        m_occupiedData->setOccupantIcon(slot, icon);

        world::IObject* const seatObject = static_cast<world::IObject*>(
            seat->queryInterface(world::IID_IObject));
        if (seatObject && seatObject->m_vehicleHudComponent)               // +0x54
        {
            const world::VehicleHudInfo* const info =
                static_cast<world::IVehicleHudComp*>(
                    seatObject->m_vehicleHudComponent)->getHudInfo();      // +0x64
            m_occupantX[slot] = static_cast<int>(info->vehicleIconPos.x);  // +0x4C
            m_occupantY[slot] = static_cast<int>(info->vehicleIconPos.y);  // +0x74
            if (m_occupantX[slot] == 0 && m_occupantY[slot] == 0)
                m_occupiedData->setOccupantIcon(slot, 0);                  // undoes the icon
        }
    }
}

// Configures the HUD for the supplied vehicle.
void HudVehicleInformation::setupVehicle(world::IObject* vehicle)
{
    // The two ten-entry scratch arrays the seat loop fills.
    float collectedColors[10][3];
    char* collectedNames[10];
    for (int i = 0; i < 10; ++i)
    {
        collectedColors[i][0] = 0.0f;
        collectedColors[i][1] = 0.0f;
        collectedColors[i][2] = 0.0f;
        collectedNames[i]  = &s_emptyString;
        m_occupantNames[i] = &s_emptyString;   // defect 2 - the old pointer is dropped, not freed
    }

    world::IObject* const root = world::getRootParent(vehicle);
    if (!root)
        return;
    world::IPlayerControlObject* const rootPco =
        static_cast<world::IPlayerControlObject*>(
            root->queryInterface(world::IID_IPlayerControlObject));
    if (!rootPco)
        return;

    // Copies the seat map by value; requires a nonnull map pointer.
    const std::map<int, world::IPlayerControlObject*> seats = *rootPco->getPcos();   // +0x60

    int occupied = 0;      // [ebp-4]    - how many rows the roster will hold
    int label    = 1;      // [ebp-0x14] - the 1-based seat number in the text, bumped every seat
    int slot     = 0;      // the scratch index; defect 1 - never compared against ten
    for (std::map<int, world::IPlayerControlObject*>::const_iterator it = seats.begin();
         it != seats.end(); ++it, ++label, ++slot)
    {
        world::IPlayerControlObject* const seat = it->second;
        if (!seat->isOccupied())                                           // ipco +0x18
            continue;
        world::IPlayer* const occupant = seat->getOccupingPlayer();        // ipco +0x10
        if (!occupant)
            continue;

        if (collectedNames[slot] != &s_emptyString)     // defect 3 - dead as compiled
            ::operator delete(collectedNames[slot]);

        const std::string text =
            formatString("%d. %s", label, occupant->getName().c_str());    // 4310D0, IPlayer +0x20
        const std::size_t size = text.size() + 1;
        collectedNames[slot] = static_cast<char*>(::operator new(size));
        std::memcpy(collectedNames[slot], text.c_str(), size);

        world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
        if (!local)
        {
            ++occupied;          // the row still counts, it just has no tint
            continue;
        }

        // team first, squad second. tone 2 is "one of ours", 1 is everyone else.
        const int tone = (occupant->getTeam() == local->getTeam()) ? 2 : 1;   // IPlayer +0xE4
        const meme::Color* tint = &textColors[tone];                          // A1CDF8 + tone*16
        if (occupant->getSquadId() == local->getSquadId() && tone == 2        // IPlayer +0x108
            && static_cast<unsigned int>(occupant->getSquadId()) > 0u)        // asked a third time
            tint = &squadChatColors[2];

        collectedColors[slot][0] = tint->r;
        collectedColors[slot][1] = tint->g;
        collectedColors[slot][2] = tint->b;
        ++occupied;
    }

    for (int i = 0; i < 10; ++i)
        m_occupantShow[i] = false;                     // +0xEC, ten bytes cleared at 78D797

    // backwards: scratch 0..9 forwards, roster occupied-1 downwards.
    int target = occupied - 1;
    for (int i = 0; i < 10 && target >= 0; ++i)
    {
        if (collectedNames[i] == &s_emptyString)
            continue;                                  // skipped without consuming a roster row
        if (m_occupantNames[target] != &s_emptyString) // defect 3 again - dead as compiled
            ::operator delete(m_occupantNames[target]);
        m_occupantColors[target][0] = collectedColors[i][0];
        m_occupantColors[target][1] = collectedColors[i][1];
        m_occupantColors[target][2] = collectedColors[i][2];
        m_occupantNames[target]     = collectedNames[i];   // moved, not cloned
        m_occupantShow[target]      = true;
        --target;
    }
}

// Configures the HUD for the vehicle occupied by the player.
void HudVehicleInformation::setupOccupiedVehicle(world::IObject* vehicle, bool isLocalPlayer)
{
    if (!vehicle || !vehicle->getTemplate())                        // +0x14
        return;

    if (isLocalPlayer)
    {
        m_warningSources.clear();                                   // +0x258

        if (world::IObjectComponent* const comp = vehicle->m_vehicleHudComponent)   // +0x54
        {
            const world::VehicleHudInfo* const info =
                static_cast<world::IVehicleHudComp*>(comp)->getHudInfo();           // +0x64
            if (info->displaySelectionOnEnter                                       // +0x98
                && hudManager->getMessages())                                       // +0x25C
            {
                hudManager->getMessages()->m_duration = 2.0f;       // fetched again, as shipped
                hudManager->setMode(10, true);                      // +0x1DC
            }
        }
    }

    setupVehicle(vehicle);

    updateOccupantIcons(vehicle);

    m_field020 = true;                                              // +0x20
    m_field01c = 5.0f;                                              // +0x1C
    m_field018 = 0.0f;                                              // +0x18
}

// Applies a missile-lock notification to the local vehicle's warning component.
// Tracks matching lock sources and clears the warning when none remain. Missing or incompatible input is ignored.
void HudVehicleInformation::handleMissileLockEvent(world::IPlayer* local, void* data)
{
    const world::MissileLockMessage* const message =
        static_cast<const world::MissileLockMessage*>(data);
    if (!message)
        return;
    if (!local)
        return;
    if (!local->getVehicle())                                           // IPlayer +0x44
        return;
    world::IWarningHudComp* const warning = static_cast<world::IWarningHudComp*>(
        local->getVehicle()->m_warningHudComponent);                    // asked again; IObject +0x6C
    if (!warning)
        return;
    if (message->field08 == 0u)                                         // 78C06B `jbe`
        return;

    const unsigned char* const bytes = reinterpret_cast<const unsigned char*>(message);
    const unsigned char targetType = bytes[12];
    if (targetType != static_cast<unsigned char>(warning->getHudInfo()->warningType))  // byte compare
    {
        if (targetType != 3)
            return;
        if (warning->getHudInfo()->warningType != 1)                    // asked again, dword compare
            return;
    }

    if (message->field04 != -1)
    {
        warning->setWarningState(static_cast<world::WarningState>((bytes[2] != 0) + 1));   // +0x38
        m_warningIconShow = warning->getWarningState() != 0;            // +0x3C, +0x254
        if (m_warningIconShow)
        {
            const int source = static_cast<int>(message->field08);
            if (m_warningSources.find(source) == m_warningSources.end())    // 457DC0 first...
                m_warningSources.insert(source);                        // ...then 458630
            assignString(m_warningIconPath, warning->getHudIcons()->c_str());   // +0x44, +0x268
        }
    }
    else if (!m_warningSources.empty())
    {
        m_warningSources.erase(static_cast<int>(message->field08));
    }

    if (!m_warningSources.empty())
        return;
    warning->setWarningState(static_cast<world::WarningState>(0));
    m_warningIconShow = false;
}

// Handle weapon crosshair feedback for the local player's soldier. data must be a
// Non-null weapon with a GenericFireArm template. Events 79/80 select the cannot-fire
// Icon and 81 selects can-fire; update the index before testing whether to save it.
// Timers and fade fields are taken from the weapon template.
void HudVehicleInformation::handleCrossHairEvent(unsigned int id, world::IPlayer* local, void* data)
{
    world::IObject* const weapon = static_cast<world::IObject*>(data);  // not null-tested
    if (weapon->getTemplate()->getClassID() != world::CID_GenericFireArmTemplate)   // ds:91C250
        return;
    if (!weapon->getParent())                                           // +0x28
        return;
    if (weapon->getParent()->getTemplate()->getClassID() != world::CID_SoldierTemplate)   // ds:91C240
        return;
    world::IPlayerControlObject* const soldierControl =
        static_cast<world::Soldier*>(weapon->getParent());              // parent re-read; PE +0x154
    if (!soldierControl->getOccupingPlayer())                           // ipco +0x10
        return;
    if (soldierControl->getOccupingPlayer() != local)                   // asked again
        return;

    const world::GenericFireArmTemplate* const weaponTemplate =
        static_cast<const world::GenericFireArmTemplate*>(weapon->getTemplate());
    switch (id)
    {
    case 79:
        m_field0a8 = weaponTemplate->m_cannotFireGuiIndex;              // +0x2F4
        if (m_guiIndex != weaponTemplate->m_cannotFireGuiIndex)         // a fresh template read
            m_field0b0 = m_guiIndex;
        m_guiIndex = m_field0a8;
        m_field0b4 = weaponTemplate->m_cannotFireTimer;                 // +0x2F8
        return;

    case 80:
    {
        const int cannotFire = weaponTemplate->m_cannotFireGuiIndex;    // +0x2F4
        m_field0a8 = cannotFire;                                        // stored before the test
        if (m_guiIndex != m_field0ac && m_guiIndex != cannotFire)
            m_field0b0 = m_guiIndex;
        m_guiIndex = m_field0a8;
        m_field0b4 = 0.0f;
        m_field2a4 = weaponTemplate->m_crosshairFireTime;               // +0x2FC
        m_crossHairMode = 1;
        m_field2a8 = weaponTemplate->m_crosshairFireTime;               // read again
        return;
    }

    case 81:
    {
        const int canFire = weaponTemplate->m_canFireGuiIndex;          // +0x2F0
        m_field0ac = canFire;                                           // stored before the test
        if (m_guiIndex != canFire && m_guiIndex != m_field0a8)
            m_field0b0 = m_guiIndex;
        m_guiIndex = m_field0ac;
        m_field0b4 = 0.0f;
        m_field2a4 = weaponTemplate->m_crosshairFireTime;               // +0x2FC
        m_crossHairMode = 2;
        m_field2a8 = weaponTemplate->m_crosshairFireTime;               // read again
        return;
    }

    default:                                                            // the router sends only 79-81
        return;
    }
}

// Restores the vehicle's saved map zoom unless the map is expanded.
// Recomputes bounds and center before storing the restored zoom. Requires a local player;
// Vehicle types above nine pass the range check but exceed the saved-zoom table.
void dispatchHudMapApplyVehicleZoom(meme::Bf2MenuMapNode* map)
{
    if (map->isExpandedMap())                                       // +0x68E
        return;

    const int type = hudManager->getPlayerVehicleType(              // hudManager vt +0x74
        world::playerManager->getLocalHumanPlayer());               // PlayerManager +0x30
    if (type < 0 || static_cast<unsigned int>(type) >= 40u)         // `jl` / `jae 0x28`
        return;

    const int saved = map->savedZoomSlot(type);                     // [this + 0x69C + type*4]
    map->updateViewBounds();                                        // +0x120 - before the store
    map->recenterOnPlayer();                                        // +0x118
    map->m_zoomLevel = saved;                                       // +0x6D0 - latched last
}

// [PLATFORM] Win32 client/plugin setters use the published RendDX9 subobject's raw table.
// Although registered as IBase*, that subobject starts with a destructor, not IBase methods.
// Keep the one-float thiscall ABI and slots: interference 0x04, roll 0x0C, frequency 0x14, scale 0x1C.
void dispatchPostProductionInterference(IBase* framework, float value)
{
    postProductionSetter(framework, 0x04)(framework, value);
}

void dispatchPostProductionRoll(IBase* framework, float value)
{
    postProductionSetter(framework, 0x0C)(framework, value);
}

void dispatchPostProductionFrequency(IBase* framework, float value)
{
    postProductionSetter(framework, 0x14)(framework, value);
}

void dispatchPostProductionScale(IBase* framework, float value)
{
    postProductionSetter(framework, 0x1C)(framework, value);
}

} } } // namespace dice::hfe::hud

#endif
