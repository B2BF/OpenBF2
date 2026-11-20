// [INFERRED] Client tutorial implementation and placement. BF2.exe is the reference;
// the symbolized dedicated ELF has no corresponding tutorial owner.
#include <cmath>
#include <cstddef>
#include <cstring>
#include <vector>

#include "PlayerVOHelp.h"

#if !DEDICATED
#include "GameLogic.h"
#include "VoiceMessages.h"
#include "../Main/Game.h"
#include "../Main/BF2ClientSeams.h"
#include "../GeneralSettings.h"
#include "../LocalProfileManager.h"
#include "../Objects/IObject.h"
#include "../Objects/IPlayer.h"
#include "../Objects/ISoldier.h"
#include "../Objects/ICameraObject.h"
#include "../Objects/IWeaponObject.h"
#include "../Objects/HelpHudCompTemplate.h"
#include "../Objects/IWeaponHudComp.h"
#include "../Objects/WeaponHudCompTemplate.h"
#include "../Objects/Game/Kit.h"
#include "../Objects/IVehicleHudComp.h"
#include "../Objects/ObjectLayoutAccess.h"
#include "../Objects/Game/EntryPoint.h"
#include "../Objects/GameLogicComp/IArmorGLComp.h"
#include "../../Scene/Object/ObjectTypes/Game/IPlayerControlObject.h"
#include "../Objects/ObjectTemplate.h"
#include "../../Core/Random.h"
#include "../../Core/SettingsRepostitory.h"
#include "../../Core/Ids.h"
#include "../../Core/System.h"
#include "../../Core/StringUtil.h"
#include "../../Sound/BfSound.h"
#include "../../Sound/NullSound.h"
#include "../../Scene/Object/PlayerManager.h"
#include "../../Scene/Object/ObjectManager.h"
#include "../../Scene/Object/ObjectPredicator.h"
#include "../../AI/AIInterface/SingletonServer.h"
#include "../../AI/AIInterface/IAIPlayerCourier.h"
#include "../../Menu/BF2HudManager.h"
#include "../../Menu/HudAmmo.h"
#include "../../Menu/HudVehicleInformation.h"

namespace dice { namespace hfe {

namespace {
// [CHOSEN, not recovered] Owner-requested +tutorialMessages 0/1 outranks the profile for this session.
// Both tutorial publication paths share the override; General.con keeps the profile value.
bool showTutorialMessages()
{
    if (settings->boolExists("GSCommandLineShowTutorialMessages"))
        return settings->boolGet("GSCommandLineShowTutorialMessages", true);
    return static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(g_generalSettings))
        ->getShowTutorialMessages();
}

// BF2.exe 0x43FFA0/0x440030; preserve the shared Rand draw and signed shift.
float helpRandom(float scale, float offset)
{
    const int value = (static_cast<int>(Rand.getRaw() | 0x80u) >> 7) + 1;
    return static_cast<float>(static_cast<long double>(value) * scale + offset);
}
}

// BF2.exe 0x5EDF21, repeated in cases 54/55/69/76/77. No component query.
world::IHelpHudComp* PlayerVOHelp::helpComponent(world::IObject* object)
{
    return static_cast<world::IHelpHudComp*>(world::object_detail::LayoutAccess::helpHudComponent(object));
}
// BF2.exe 0x5EDF2D.
bool PlayerVOHelp::hasAbilityHud(world::IObject* object)
{
    return world::object_detail::LayoutAccess::abilityHudComponent(object) != 0;
}
// BF2.exe 0x5EDF55/0x5EE308/0x5EE54F/0x5EFF4C: retain getHudInfo dispatch.
// Its borrowed result is the embedded info block; use the owning type's offset
// so the adjacent scalar reads follow this compiler's STL and pointer layout.
const world::HelpHudCompTemplate* PlayerVOHelp::helpTemplate(world::IHelpHudComp* help)
{
    return reinterpret_cast<const world::HelpHudCompTemplate*>(
        reinterpret_cast<const char*>(help->getHudInfo()) -
        offsetof(world::HelpHudCompTemplate, m_helpHudInfo));
}
// BF2.exe 0x5EDF58/0x5EFF4F/0x5EE30B/0x5EE552: info +188/+189/+18A/+18B.
bool PlayerVOHelp::helpActive(world::IHelpHudComp* help, int kind)
{
    const world::HelpHudCompTemplate* componentTemplate = helpTemplate(help);
    switch (kind)
    {
    case 0: return componentTemplate->m_isActive;
    case 1: return componentTemplate->m_altIsActive;
    case 2: return componentTemplate->m_lowArmorIsActive;
    default: return componentTemplate->m_lowAmmoIsActive;
    }
}
// BF2.exe 0x5EF98D/0x5EF990: getHudInfo, then signed integer at info +190.
int PlayerVOHelp::helpDistance(world::IHelpHudComp* help)
{
    return helpTemplate(help)->m_closeToDistance;
}
// BF2.exe 0x5EE7AB/0x5EE7D0: HudAmmo +B4, preserving both caller reads.
float PlayerVOHelp::helpPrimaryAmmo(hud::HudAmmo* ammo)
{
    return ammo->m_field0b4;
}

namespace {
// BF2.exe 0x5DEC70. [INFERRED] helper name. This search ignores team and entry
// radius; prefer the nearest empty seat, retaining the nearest AI-occupied fallback.
world::IPlayerControlObject* findNearbyControl(const Vec3& origin, world::IPlayer* player, float radius)
{
    if (player && player->getIsAIPlayer())
    {
        radius = 15.0f;
        static_cast<ai::IAIPlayerCourier*>(ai::BFSingletonServer::m_iaiPlayerCourier)
            ->getRequestedVehicle(player->getId());
    }
    std::vector<world::IObject*>& scratch = world::objectManager->getObjectTempVector();
    std::vector<world::IObject*>().swap(scratch);
    float nearestEmpty = 100000.0f, nearestAI = 100000.0f;
    world::IPlayerControlObject* empty = 0;
    world::IPlayerControlObject* aiOccupied = 0;
    world::ObjectFlagPredicator predicator(0x200);
    std::vector<world::IObject*> candidates;
    world::objectManager->getObjectsFromGrid(origin, radius, scratch, &predicator);
    for (world::IObject* object : scratch)
        candidates.push_back(object);
    world::objectManager->getNoCollisionObjectsFromGrid(origin, radius, scratch, &predicator);
    for (world::IObject* object : scratch)
        candidates.push_back(object);
    for (world::IObject* object : candidates)
    {
        const Vec3& position = object->getAbsolutePosition();
        const float x = position.x - origin.x;
        const float y = position.y - origin.y;
        const float z = position.z - origin.z;
        const float distance = std::sqrt(z * z + (y * y + x * x));
        if (distance > radius || distance > nearestEmpty)
            continue;
        world::IArmorGLComp* armor = 0;
        for (world::IObject* parent = object; parent; parent = parent->getParent())
            if ((armor = static_cast<world::IArmorGLComp*>(parent->getArmorComponent())))
                break;
        if (armor && armor->isDestroyed())
            continue;
        world::IObject* root = world::getRootParent(object ?
            static_cast<world::IObject*>(object->queryInterface(world::IID_IObject)) : 0);
        world::IPlayerControlObject* pco = world::findOwningPlayerControlObject(root);
        if (!pco)
            continue;
        auto* seats = pco->getPcos();
        for (const auto& entry : *seats)
        {
            world::IPlayerControlObject* seat = entry.second;
            if (!seat->isOccupied())
            {
                nearestEmpty = distance;
                empty = seat;
                break;
            }
            if (seat->getOccupingPlayer()->getIsAIPlayer() && distance < nearestAI)
            {
                nearestAI = distance;
                aiOccupied = seat;
            }
        }
    }
    return empty ? empty : aiOccupied;
}
}

// BF2.exe 0x5ED220.
PlayerVOHelp::PlayerVOHelp()
    : messageActive(false), messageDuration(10.0f), messageElapsed(0.0f),
      enabled(false), onFoot(false)
{
    for (int i = 0; i < 82; ++i)
        timers[i].interval = timers[i].remaining = 0.03333333507180214f;
    for (bool& value : played)
        value = false;
    for (bool& value : disabled)
        value = false;
    for (int& value : minimumExperience)
        value = 0;
    nextMessageDelay = helpRandom(0.00000178813934326171875f, 10.0f);

    labels[0] = "HUD_HELP_PLAYER_CONTROLS_moveMouseToLook";
    labels[1] = "HUD_HELP_PLAYER_CONTROLS_moveForwardsBackwards";
    labels[2] = "HUD_HELP_PLAYER_CONTROLS_fireLeftMouse";
    labels[3] = "HUD_HELP_WEPAON_HANDHELD_GENERAL_reloadWeapon";
    labels[4] = "HUD_HELP_PLAYER_CONTROLS_zoomAltFire";
    labels[5] = "HUD_HELP_PLAYER_CONTROLS_jump";
    minimumExperience[5] = 7200;
    labels[6] = "HUD_HELP_PLAYER_CONTROLS_crouch";
    minimumExperience[6] = 18000;
    labels[7] = "HUD_HELP_PLAYER_CONTROLS_lieDown";
    disabled[7] = true;
    labels[8] = "HUD_HELP_PLAYER_CONTROLS_strafeLeftRight";
    minimumExperience[8] = 7200;
    labels[9] = "HUD_HELP_SQUAD_squadJoined";
    labels[10] = "HUD_HELP_SPAWNSCREEN_SQUADTAB_createSquad";
    minimumExperience[10] = 36000;
    labels[11] = "HUD_HELP_SQUAD_squadCreated";
    disabled[11] = true;
    labels[12] = "HUD_HELP_SPAWNSCREEN_SQUADTAB_joinSquad";
    disabled[12] = true;
    labels[13] = "HUD_HELP_PLAYER_CONTROLS_parachute";
    labels[14] = "HUD_HELP_MENU_CONTROLS_callMedicWhenManDown";
    labels[15] = "HUD_HELP_MENU_CONTROLS_COMMOROSE_slcommoRoseAvailable";
    labels[16] = "HUD_HELP_COMMANDER_placeArtillery";
    disabled[16] = true;
    labels[17] = "HUD_HELP_COMMANDER_placeSupplies";
    disabled[17] = true;
    labels[18] = "HUD_HELP_COMMANDER_placeUAV";
    disabled[18] = true;
    labels[19] = "HUD_HELP_COMMANDER_artilleryFired";
    disabled[19] = true;
    labels[20] = "HUD_HELP_COMMANDER_UAVplaced";
    disabled[20] = true;
    labels[21] = "HUD_HELP_SQUAD_callForTarget";
    disabled[21] = true;
    labels[22] = "HUD_HELP_COMMANDER_commanderApply";
    disabled[22] = true;
    labels[23] = "HUD_HELP_SPAWNSCREEN_SQUADTAB_applyCommander";
    minimumExperience[23] = 72000;
    labels[24] = "HUD_HELP_COMMANDER_SQUADLIST_squadList";
    disabled[24] = true;
    labels[25] = "HUD_HELP_COMMANDER_SQUADLIST_clickSquadList";
    labels[26] = "HUD_HELP_COMMANDER_MINIMAP_description";
    labels[27] = "HUD_HELP_COMMANDER_MAP_QUICKMENU";
    labels[28] = "HUD_HELP_COMMANDER_MAP_ZOOM_inOut";
    labels[29] = "HUD_HELP_COMMANDER_SATELLITESCAN_description";
    labels[30] = "HUD_HELP_COMMANDER_UAV_description";
    labels[31] = "HUD_HELP_COMMANDER_ARTILLERY_description";
    labels[32] = "HUD_HELP_COMMANDER_SUPPLIES_description";
    labels[34] = "HUD_HELP_SQUAD_LEADER_VOIP_commanderChannel";
    disabled[34] = true;
    labels[35] = "HUD_HELP_COMMANDER_VOIP_allSquadLeadersChannel";
    disabled[35] = true;
    labels[36] = "HUD_HELP_COMMANDER_COMMUNICATION_VOIPTOGGLE";
    disabled[36] = true;
    labels[37] = "HUD_HELP_COMMANDER_MAP_REQUESTS_received";
    disabled[37] = true;
    labels[38] = "HUD_HELP_COMMANDER_SQUADLIST_orderIcon";
    disabled[38] = true;
    labels[39] = "HUD_HELP_COMMANDER_MAP_SATELLITEVIEW_moveKeys";
    disabled[39] = true;
    labels[40] = "HUD_HELP_COMMANDER_SQUADLIST_commoMenu";
    labels[41] = "HUD_HELP_COMMANDER_MAP_FILTERS";
    labels[42] = "HUD_HELP_SQUAD_LEADER_mainMapRightMouseMenu";
    labels[43] = "";
    labels[45] = "HUD_HELP_KIT_MEDIC_NAMETAG_indicatorBar";
    disabled[45] = true;
    labels[46] = "HUD_HELP_KIT_SUPPORT_NAMETAG_indicatorBar";
    disabled[46] = true;
    labels[47] = "HUD_HELP_KIT_ENGINEER_NAMETAG_indicatorBar";
    disabled[47] = true;
    labels[48] = "HUD_HELP_GAMEPLAY_captureFlag";
    disabled[48] = true;
    labels[49] = "HUD_HELP_GAMEPLAY_exitToCapture";
    disabled[49] = true;
    labels[50] = "HUD_HELP_MENU_CONTROLS_COMMOROSE_specificSpotted";
    disabled[50] = true;
    labels[51] = "HUD_HELP_COMMANDER_uavDamaged";
    disabled[51] = true;
    labels[52] = "HUD_HELP_COMMANDER_radarDamaged";
    disabled[52] = true;
    labels[53] = "HUD_HELP_COMMANDER_artilleryDamaged";
    disabled[53] = true;
    labels[54] = "";
    labels[55] = "";
    labels[56] = "HUD_HELP_SPAWNSCREEN_spawnOnSL";
    disabled[56] = true;
    labels[57] = "HUD_HELP_MENU_CONTROLS_zoomMap";
    labels[58] = "HUD_HELP_SQUAD_recievedTarget";
    disabled[58] = true;
    labels[59] = "HUD_HELP_SPAWNSCREEN_KITTAB_pickArmyAndKit";
    labels[60] = "HUD_HELP_SPAWNSCREEN_KITTAB_pickSpawnPoint";
    labels[61] = "HUD_HELP_SPAWNSCREEN_KITTAB_useUnlock";
    disabled[61] = true;
    labels[62] = "HUD_HELP_SPAWNSCREEN_KITTAB_trySquads";
    minimumExperience[62] = 21600;
    labels[63] = "HUD_HELP_GAMEPLAY_climbLadder";
    labels[64] = "HUD_HELP_PLAYER_CONTROLS_radioMessages";
    minimumExperience[64] = 7200;
    labels[65] = "HUD_HELP_MENU_COMMOROSE_slCommoRoseFirstUse";
    disabled[65] = true;
    minimumExperience[66] = 7200;
    disabled[66] = true;
    labels[67] = "HUD_HELP_VEHICLE_GENERAL_CONTROLS_enterVehicle";
    labels[68] = "HUD_HELP_VEHICLE_CONTROLS_exitVehicle";
    timers[69].interval = 1.0f;
    labels[69] = "";
    labels[70] = "HUD_HELP_PLAYER_CONTROLS_sprint";
    disabled[70] = true;
    labels[71] = "HUD_HELP_PLAYER_CONTROLS_lieDown";
    minimumExperience[71] = 18000;
    labels[72] = "HUD_HELP_MENU_CONTROLS_changeWeapon";
    labels[73] = "HUD_HELP_MENU_CONTROLS_largeMap";
    minimumExperience[73] = 7200;
    labels[74] = "HUD_HELP_MENU_CONTROLS_scoreBoard";
    minimumExperience[74] = 18000;
    labels[75] = "HUD_HELP_MENU_CONTROLS_spawnScreen";
    labels[76] = "";
    labels[77] = "";
    labels[78] = "HUD_HELP_PLAYER_CONTROLS_GasMask";
    labels[79] = "HUD_HELP_PLAYER_CONTROLS_NightVision";
    labels[80] = "HUD_HELP_WEAPON_HANDHELD_ZLINE_CONTROLS_useZipLine";
    labels[81] = "HUD_HELP_WEAPON_HANDHELD_GHOOK_CONTROLS_useGrapplingHookUp";

}

// BF2.exe 0x5ED0E0.
void PlayerVOHelp::enable(bool value) { enabled = value; }
// BF2.exe 0x5ED120. No index guard is present in retail.
void PlayerVOHelp::disableMessage(int id, bool value) { disabled[id] = value; }
// BF2.exe 0x5ED140.
bool PlayerVOHelp::messagePlayed(int id) const { return played[id]; }

// BF2.exe 0x5ED0F0; the argument is a by-value string.
void PlayerVOHelp::savePlayed(std::string key)
{
    if (!isDedicated())
        static_cast<IGeneralSettings*>(g_generalSettings)->setPlayedVOHelp(key);
}

// BF2.exe 0x5ED1A0; the dedicated arm returns false.
bool PlayerVOHelp::wasPlayed(std::string key)
{
    if (!isDedicated())
        return static_cast<IGeneralSettings*>(g_generalSettings)->checkPlayedVOHelp(key);
    return false;
}

// BF2.exe 0x5EDB20. Empty dynamic-message slots are never marked played.
void PlayerVOHelp::markPlayed(int id, bool save)
{
    if (id != 82 && !labels[id].empty())
    {
        played[id] = true;
        if (save)
            savePlayed(labels[id]);
    }
}

// BF2.exe 0x5EDB80. The profile stores total played seconds; retain repeated mode checks.
void PlayerVOHelp::loadPlayed()
{
    if (!isDedicated())
        for (int id = 0; id < 82; ++id)
        {
            if ((!labels[id].empty() &&
                 static_cast<IGeneralSettings*>(g_generalSettings)->checkPlayedVOHelp(labels[id])) ||
                isDedicated() || !(minimumExperience[id] <=
                    static_cast<ILocalProfileManager*>(g_localProfileManager)->getTotalPlayedTime()))
                markPlayed(id, false);
        }
}

// BF2.exe 0x5ED150. Reset on expiry; subtract dt only on the non-expired arm.
bool PlayerVOHelp::tickMessage(int id, float dt)
{
    if (timers[id].remaining <= 0.0f)
    {
        timers[id].remaining = timers[id].interval;
        return true;
    }
    timers[id].remaining -= dt;
    return false;
}

// BF2.exe 0x5F1460.
void PlayerVOHelp::update(float dt)
{
    if (!enabled || (gameLogic && gameLogic->getGameStatus() != 1))
        return;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return;
    world::IObject* vehicle = player->getVehicle();
    onFoot = vehicle && vehicle->getTemplate()->getClassID() == 38035;
    messageElapsed += dt;
    nextMessageDelay -= dt;
    if (messageActive)
    {
        if (!(messageElapsed > messageDuration))
            return;
        messageActive = false;
        messageElapsed = 0.0f;
        nextMessageDelay = helpRandom(30.0f * 0.000000059604644775390625f, 10.0f);
    }
    for (int id = 0; id < 82; ++id)
        if (!disabled[id] && !played[id] && tickMessage(id, dt) && tryMessage(id, dt))
        {
            messageActive = true;
            messageElapsed = 0.0f;
            nextMessageDelay = 0.0f;
            markPlayed(id, true);
            return;
        }
}

// BF2.exe 0x4F8110. Client definition of the existing VoiceMessages entry point;
// [INFERRED] placement here keeps this recovery within the lent PlayerVOHelp boundary.
int VoiceMessages::playHelpMessage(const std::string& key)
{
    const auto message = m_helpMessages.find(getLowerCase(key));
    if (message == m_helpMessages.end())
        return -1;
    const int id = message->second;
    ISoundTemplate* soundTemplate = m_helpSound->getTemplate();
    if (g_bfSound->getVolumeForGroup(soundTemplate->getVolumeGroup()) > 0.05f)
    {
        m_helpSound->stop();
        m_helpSound->playSample(id, 1);
    }
    return id;
}

// BF2.exe 0x5ED1E0, with GameLogic::getVoiceMessages at client 0x4BDBB0.
void PlayerVOHelp::playVoice(std::string key)
{
    if (world::playerManager->getLocalHumanPlayer())
        static_cast<GameLogic*>(gameLogic)->getVoiceMessages()->playHelpMessage(key);
}

// BF2.exe 0x5EDC10. The decompiler's incorrect no-return annotation on delete
// hides the candidate scan; instructions retain both grid queries and the class test.
bool PlayerVOHelp::nearObject(const Vec3& origin, float radius, unsigned int classId)
{
    std::vector<world::IObject*>& scratch = world::objectManager->getObjectTempVector();
    std::vector<world::IObject*>().swap(scratch);
    world::ObjectFlagPredicator predicator(0x200);
    std::vector<world::IObject*> candidates;
    world::objectManager->getObjectsFromGrid(origin, radius, scratch, &predicator);
    for (world::IObject* object : scratch)
        candidates.push_back(object);
    world::objectManager->getNoCollisionObjectsFromGrid(origin, radius, scratch, &predicator);
    for (world::IObject* object : scratch)
        candidates.push_back(object);
    for (world::IObject* object : candidates)
        if (object->getTemplate()->getClassID() == classId)
        {
            const Vec3& position = object->getAbsolutePosition();
            const float x = position.x - origin.x;
            const float y = position.y - origin.y;
            const float z = position.z - origin.z;
            if (!(std::sqrt(z * z + y * y + x * x) > radius))
                return true;
        }
    return false;
}

// BF2.exe 0x5EDE00. Keep message IDs and the ordering within each branch.
bool PlayerVOHelp::tryMessage(int id, float /*dt*/)
{
    const float random = helpRandom(0.0000059604644775390625f, 0.0f);
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (!player)
        return false;
    // Repeated calls below are intentional: retail does not cache the HUD mode.
    const auto show = [this](int message) {
        // [CHOSEN, not recovered] Owner's visual-only tutorial preference.
        // Keep voice, timers and played-history updates on their recovered paths.
        if (!showTutorialMessages())
            return;
        hudManager->showWideHelp(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString(labels[message])), 9.0f);
    };
    const auto showAndVoice = [&show, this](int message) {
        show(message);
        playVoice(labels[message]);
        return true;
    };
    const auto showKey = [](const std::string& key) {
        // [CHOSEN, not recovered] Also covers component and proximity tutorial text.
        if (!showTutorialMessages())
            return;
        hudManager->showWideHelp(hudManager->replaceWideInputKeys(
            hudManager->getLocalizedString(key)), 9.0f);
    };
    // Shared retail tails retain each virtual getHudInfo call and the C-string
    // comparison: embedded NULs are not compared like std::string values.
    const auto showComponent = [&showKey](world::IHelpHudComp* help,
        std::string world::HelpHudInfo::*text, std::string world::HelpHudInfo::*sound) {
        if (!(help->getHudInfo()->*text).empty() && !wasPlayed(help->getHudInfo()->*text))
        {
            showKey(help->getHudInfo()->*text);
            const char* soundKey = (help->getHudInfo()->*sound).c_str();
            const char* textKey = (help->getHudInfo()->*text).c_str();
            if (std::strcmp(textKey, soundKey) != 0)
                savePlayed(help->getHudInfo()->*text);
        }
        if (!(help->getHudInfo()->*sound).empty() && !wasPlayed(help->getHudInfo()->*sound))
        {
            playVoice(help->getHudInfo()->*sound);
            savePlayed(help->getHudInfo()->*sound);
        }
    };
    switch (id)
    {
    case 0:
        if (hudManager->getMode() == 0 && onFoot)
        {
            show(0);
            return true;
        }
        break;
    case 1:
        if (!(WorldPref::getWorldTime() > 3.0))
            return false;
        if (hudManager->getMode() != 0 || !onFoot)
            return false;
        show(1);
        playVoice("move_help");
        return true;
    case 2:
        if (hudManager->getMode() == 0 && onFoot && player->getIsAlive())
            return showAndVoice(2);
        break;
    case 3:
        if (hudManager->getMode() == 0 && onFoot && player->getIsAlive())
        {
            if (helpPrimaryAmmo(hudManager->getAmmo()) <= 0.3f &&
                helpPrimaryAmmo(hudManager->getAmmo()) > 0.0f)
                return showAndVoice(3);
        }
        break;
    case 4:
        if (hudManager->getMode() == 0 && onFoot && player->getIsAlive())
        {
            show(4);
            playVoice("weapon_zoom");
            return true;
        }
        break;
    case 5:
    case 6:
    case 7:
    case 8:
    case 71:
    case 75:
        if (nextMessageDelay < 0.0f && random < ((id == 8 || id == 71) ? 20.0f : 10.0f))
            if (hudManager->getMode() == 0 && onFoot)
            {
                show(id);
                return true;
            }
        break;
    case 9:
        if (player->getSquadId() != 0 && !player->getIsSquadLeader())
        {
            show(9);
            return true;
        }
        break;
    case 10:
    case 12:
        if (hudManager->getMode() == 18 && player->getSquadId() == 0)
            return showAndVoice(id);
        break;
    case 11:
        if (player->getIsSquadLeader())
        {
            show(11);
            return true;
        }
        break;
    case 13:
        if (onFoot && hudManager)
        {
            world::ISoldier* soldier = hudManager->getPlayerSoldier(player);
            if (soldier && soldier->getIsSkyDiving())
            {
                playVoice("parachute_help");
                show(13);
                return true;
            }
        }
        break;
    case 14:
        {
            world::IObject* vehicle = player->getVehicle();
            if (vehicle && vehicle->getTemplate()->getClassID() == 38035)
            {
                world::IArmorGLComp* armor = 0;
                for (world::IObject* object = vehicle; object; object = object->getParent())
                    if ((armor = static_cast<world::IArmorGLComp*>(object->getArmorComponent())))
                        break;
                if (armor && armor->getStatus() == 1)
                {
                    playVoice("man_down_help");
                    show(14);
                    return true;
                }
            }
        }
        break;
    case 15:
        if (player->getIsSquadLeader() && hudManager->getMode() == 0)
            return showAndVoice(15);
        break;
    case 16:
        if (hudManager->getMode() != 22)
            return false;
        show(16);
        playVoice("place_artillery");
        return true;
    case 17:
        show(17);
        playVoice("place_supplies");
        return true;
    case 18:
        show(18);
        playVoice("place_uav");
        return true;
    case 19:
    case 20:
        if (hudManager->isCommanderShown())
        {
            show(id);
            playVoice(id == 19 ? "artillery_fired" : "uav_placed");
            return true;
        }
        break;
    case 21:
        show(21);
        return true;
    case 22:
        if (!player->getIsCommander())
            return showAndVoice(22);
        break;
    case 23:
        if (hudManager->getMode() != 18 && hudManager->getMode() != 17)
            return false;
        return showAndVoice(23);
    case 24:
    case 26:
    case 27:
    case 28:
    case 29:
    case 30:
    case 31:
    case 32:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
    case 51:
    case 52:
    case 53:
        if (hudManager->isCommanderShown())
            return showAndVoice(id);
        break;
    case 25:
    case 40:
        if (hudManager->isCommanderShown() &&
            world::playerManager->getNrOfActiveSquads(player->getTeam()) > 0)
            return showAndVoice(id);
        break;
    case 34:
    case 45:
    case 46:
    case 47:
    case 48:
    case 49:
    case 50:
    case 56:
        return showAndVoice(id);
    case 41:
        if (hudManager->getMode() == 15)
            return showAndVoice(41);
        break;
    case 42:
        if (hudManager->getMode() == 17)
            return showAndVoice(42);
        break;
    case 43:
        {
            if (!player->getVehicle())
                return false;
            world::IObject* defaultVehicle = player->getDefaultVehicle();
            if (player->getVehicle() == defaultVehicle)
                return false;
            world::Kit* kit = hudManager->getPlayerKit(player);
            if (!kit)
                return false;
            auto* pco = static_cast<world::IPlayerControlObject*>(
                player->getVehicle()->queryInterface(world::IID_IPlayerControlObject));
            world::IHelpHudComp* help = helpComponent(kit);
            if (!pco || !hasAbilityHud(kit) || !pco->getAutoUseAbility() || !help)
                return false;
            if (!helpActive(help, 0))
                return false;
            showComponent(help, &world::HelpHudInfo::helpStringKey,
                                &world::HelpHudInfo::helpSoundKey);
            help->setIsActive(false);
            return true;
        }
    case 54:
        if (player->getVehicle())
        {
            world::IHelpHudComp* help = helpComponent(player->getVehicle());
            if (help)
            {
                auto* armor = static_cast<world::IArmorGLComp*>(player->getVehicle()->getArmorComponent());
                if (armor)
                {
                    const float hitPoints = armor->getHitPoints();
                    const long double maximum = armor->getMaxHitPoints();
                    if (hitPoints / maximum < 0.3f && helpActive(help, 2))
                    {
                        showComponent(help, &world::HelpHudInfo::lowArmorHelpStringKey,
                                            &world::HelpHudInfo::lowArmorHelpSoundKey);
                        help->setLowArmorIsActive(false);
                        return true;
                    }
                }
            }
        }
        break;
    case 55:
        if (player->getVehicle())
        {
            world::IPlayerControlObject* pco = world::findOwningPlayerControlObject(player->getVehicle());
            if (pco)
                for (int channel = 0; channel < 2; ++channel)
                {
                    world::IObject* object = pco->getActiveWeapon(channel);
                    if (!object)
                        continue;
                    auto* weapon = static_cast<world::IWeaponObject*>(object->queryInterface(world::IID_IWeaponObject));
                    if (!weapon)
                        continue;
                    world::IHelpHudComp* help = helpComponent(object);
                    if (!help)
                        continue;
                    const float firstMagazine = static_cast<float>(weapon->getMag0Size());
                    const int magazine = weapon->getMagSize();
                    if (static_cast<long double>(firstMagazine) / magazine < 0.3f && helpActive(help, 3))
                    {
                        showComponent(help, &world::HelpHudInfo::lowAmmoHelpStringKey,
                                            &world::HelpHudInfo::lowAmmoHelpSoundKey);
                        help->setLowAmmoIsActive(false);
                        return true;
                    }
                }
        }
        break;
    case 69:
        if (onFoot)
        {
            world::IObject* vehicle = player->getVehicle();
            if (vehicle && vehicle->queryInterface(world::IID_IPlayerControlObject))
            {
                world::IPlayerControlObject* near = findNearbyControl(
                    player->getVehicle()->getAbsolutePosition(), player, 30.0f);
                if (!near)
                    return false;
                auto* object = static_cast<world::IObject*>(near->queryInterface(world::IID_IObject));
                if (!object)
                    return false;
                world::IHelpHudComp* help = helpComponent(object);
                if (!help)
                    return false;
                int otherTeam = near->getTeam();
                int referenceTeam = player->getTeam();
                if (!help->getIsCloseToActive(referenceTeam, otherTeam))
                    return false;
                world::IObject* current = player->getVehicle();
                const Vec3& target = object->getAbsolutePosition();
                const Vec3& source = current->getAbsolutePosition();
                const float x = source.x - target.x;
                const float y = source.y - target.y;
                const float z = source.z - target.z;
                const float distance = std::sqrt((z * z + y * y) + x * x);
                if (!(static_cast<long double>(helpDistance(help)) > distance))
                    return false;
                // Retail evaluates the other team's getter before the local team's.
                const auto closeText = [&]() -> const std::string& {
                    const int other = near->getTeam();
                    const int reference = player->getTeam();
                    return help->getCloseToHelpStringKey(reference, other);
                };
                const auto closeSound = [&]() -> const std::string& {
                    const int other = near->getTeam();
                    const int reference = player->getTeam();
                    return help->getCloseToHelpSoundKey(reference, other);
                };
                if (!closeText().empty() && !wasPlayed(closeText()))
                {
                    showKey(closeText());
                    const char* soundKey = closeSound().c_str();
                    const char* textKey = closeText().c_str();
                    if (std::strcmp(textKey, soundKey) != 0)
                        savePlayed(closeText());
                }
                if (!closeSound().empty() && !wasPlayed(closeSound()))
                {
                    playVoice(closeSound());
                    savePlayed(closeSound());
                }
                otherTeam = near->getTeam();
                referenceTeam = player->getTeam();
                help->setCloseToIsActive(referenceTeam, otherTeam, false);
                return true;
            }
        }
        break;
    case 76:
        if (!player->getVehicle() || hudManager->getMode() != 0 || !player->getIsAlive())
            return false;
        {
            world::IPlayerControlObject* pco = world::findOwningPlayerControlObject(player->getVehicle());
            if (!pco)
                return false;
            world::IObject* weapon = pco->getActiveWeapon(0);
            if (!weapon)
                return false;
            world::IHelpHudComp* help = helpComponent(weapon);
            if (!help)
                return false;
            if (!helpActive(help, 0) && !helpActive(help, 1))
                return false;
            auto* weaponHud = static_cast<world::IWeaponHudComp*>(world::object_detail::LayoutAccess::weaponHudComponent(weapon));
            if (!weaponHud || !hudManager->getVehicleInformation())
                return false;
            if (helpActive(help, 0))
            {
                hud::HudVehicleInformation* vehicleHud = hudManager->getVehicleInformation();
                const int guiIndex = weaponHud->getHudInfo()->guiIndex;
                if (vehicleHud->getGuiIndex() == guiIndex)
                {
                    showComponent(help, &world::HelpHudInfo::helpStringKey,
                                        &world::HelpHudInfo::helpSoundKey);
                    help->setIsActive(false);
                    return true;
                }
            }
            if (helpActive(help, 1))
            {
                hud::HudVehicleInformation* vehicleHud = hudManager->getVehicleInformation();
                const int guiIndex = weaponHud->getHudInfo()->altGuiIndex;
                if (vehicleHud->getGuiIndex() == guiIndex)
                {
                    showComponent(help, &world::HelpHudInfo::altHelpStringKey,
                                        &world::HelpHudInfo::altHelpSoundKey);
                    help->setAltIsActive(false);
                    return true;
                }
            }
        }
        break;
    case 77:
        if (!player->getVehicle() || !player->getIsAlive())
            return false;
        {
            world::IHelpHudComp* help = helpComponent(player->getVehicle());
            if (help && helpActive(help, 0))
            {
                showComponent(help, &world::HelpHudInfo::helpStringKey,
                                    &world::HelpHudInfo::helpSoundKey);
                help->setIsActive(false);
                return true;
            }
        }
        break;
    case 57:
        if (player->getIsAlive())
            if (hudManager->getMode() == 2 || hudManager->getMode() == 18 ||
                hudManager->getMode() == 17 || hudManager->getMode() == 15)
            {
                show(57);
                return true;
            }
        break;
    case 58:
        show(58);
        playVoice("place_artillery");
        return true;
    case 59:
    case 60:
    case 61:
    case 62:
        if (hudManager->getMode() == 1)
            return showAndVoice(id);
        break;
    case 63:
        if (onFoot && player->getVehicle()->queryInterface(world::IID_IPlayerControlObject) &&
            nearObject(player->getVehicle()->getAbsolutePosition(), 3.0f, 40000))
        {
            playVoice("ladder_help");
            show(63);
            return true;
        }
        break;
    case 64:
        if (hudManager->getMode() == 0)
        {
            show(64);
            playVoice("comm_rose");
            return true;
        }
        break;
    case 65:
    case 66:
        if (player->getIsSquadLeader())
            return showAndVoice(id);
        break;
    case 67:
        if (onFoot)
        {
            world::IObject* vehicle = player->getVehicle();
            if (vehicle && vehicle->queryInterface(world::IID_IPlayerControlObject))
            {
                world::IObject* origin = player->getVehicle();
                const int team = player->getTeam();
                if (world::BFfindEntryPoint(origin->getAbsolutePosition(),
                    3.0f, team, player, true, true, true))
                {
                    auto* vehicleHud = static_cast<world::IVehicleHudComp*>(vehicle->getVehicleHudComponent());
                    if (vehicle->getTemplate() && vehicle->getTemplate()->getClassID() == 40000)
                        return false;
                    if (vehicleHud && !vehicleHud->getHudInfo()->standardHelpEnabled)
                        return false;
                    playVoice("enter_Vehicle_help");
                    show(67);
                    return true;
                }
            }
        }
        break;
    case 68:
        {
            world::IObject* defaultVehicle = player->getDefaultVehicle();
            if (player->getVehicle() != defaultVehicle && player->getIsAlive())
            {
                world::IObject* vehicle = player->getVehicle();
                if (vehicle)
                {
                    auto* vehicleHud = static_cast<world::IVehicleHudComp*>(vehicle->getVehicleHudComponent());
                    if (vehicle->getTemplate() && vehicle->getTemplate()->getClassID() == 40006)
                        return false;
                    if (vehicleHud && !vehicleHud->getHudInfo()->standardHelpEnabled)
                        return false;
                }
                playVoice("exit_Vehicle_help");
                show(68);
                return true;
            }
        }
        break;
    case 70:
        if (onFoot)
        {
            playVoice("HUD_HELP_PLAYER_CONTROLS_sprint");
            show(70);
            return true;
        }
        break;
    case 72:
        if (WorldPref::getWorldTime() > 20.0)
            if (hudManager->getMode() == 0 && onFoot && player->getIsAlive())
            {
                show(72);
                playVoice("weapon_change");
                return true;
            }
        break;
    case 73:
        if (WorldPref::getWorldTime() > 60.0)
        {
            show(73);
            playVoice("open_map");
            return true;
        }
        break;
    case 74:
        if (hudManager->getMode() == 0 && onFoot)
        {
            show(74);
            return true;
        }
        break;
    case 78:
        if (hudManager->getMode() == 0 && onFoot && player->getIsNearGasCloud())
            return showAndVoice(78);
        break;
    case 79:
        if (hudManager->getMode() == 0 && onFoot && player->isPlayerInNightMap())
            return showAndVoice(79);
        break;
    case 80:
    case 81:
        if (hudManager->getMode() == 0 && onFoot && player->getDefaultVehicle())
            if (nearObject(player->getDefaultVehicle()->getAbsolutePosition(), 3.0f,
                           id == 80 ? 40009 : 40013))
                return showAndVoice(id);
        break;
    }
    return false;
}

} }
#endif
