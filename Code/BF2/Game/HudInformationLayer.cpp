// Path: attested (assert), BF2.exe 0x89E910.
// [INFERRED] member/helper names. Bodies read from the client PE; no HUD ELF body.
#include "HudInformationLayer.h"
#if !DEDICATED
#include "../Core/Debug.h"
#include "../Core/ClassManager.h"
#include "../Core/SmartItf.h"
#include "../Core/Ids.h"
#include "../Menu/BF2HudManager.h"
#include "../Menu/Bf2MenuMapNode.h"
#include "../Menu/HudAmmo.h"
#include "../Menu/HudVehicleInformation.h"
#include "../Menu/HudKitSelection.h"
#include "../Menu/HudVoteMap.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Scene/Object/PlayerScoreData.h"
#include "../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "GeneralSettings.h"
#include "IGame.h"
#include "IGameClient.h"
#include "IGameLogic.h"
#include "Main/BF2ClientSeams.h"
#include "Objects/IPlayer.h"
#include "Objects/IObject.h"
#include "Objects/IObjectTemplate.h"
#include "Objects/ICameraObject.h"
#include "Objects/IItemTemplate.h"
#include "Objects/IControlPoint.h"
#include "Objects/Game/KitTemplate.h"
#include "Objects/VehicleHudCompTemplate.h"
#include "Objects/WeaponHudCompTemplate.h"
#include "GameLogic/CombatAreaManager.h"
#include "GameLogic/ScoreSystem/ScoreManager.h"
#include "GameLogic/ScoreSystem/TeamScore.h"
#include "GameServer/ServerInfo.h"
#include "Common/ClientVoteManager.h"
#include <cstdio>
#include <cstdlib>
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#include "../Sound/BfSound.h"           // g_bfSound - the ticket sounds
#include "../Menu/HudBlinker.h"
#include "../Menu/HudColors.h"
#include "GameServer/ServerSettings.h"
#include <cstring>
#include <cwchar>
#include <cstddef>
#include <cmath>
#include <new>
namespace dice { namespace hfe {
namespace world { extern IBase* scoreManager; }
namespace {
//  / 0x9A60B4. Identity matters independently of string contents.
const char emptyString[] = "";
const wchar_t emptyWideString[] = L"";
}
// PE40FECC: reject aggregation, construct, query the requested interface, and
// release on mismatch. The retail body has no post-allocation null guard.
IBase* HudInformationLayer::create(unsigned int iid, IBase* outer)
{
    if (outer) return nullptr;
    void* allocation = ::operator new(sizeof(HudInformationLayer));
    HudInformationLayer* layer = allocation ? new (allocation) HudInformationLayer : nullptr;
    IBase* result = static_cast<IBase*>(layer->queryInterface(iid));
    if (!result) layer->release();
    return result;
}

// PE4644A0..464610, complete instructions read. Preserve all32 scalar copies
// and their family order; the records are shared mutable palette storage.
void HudInformationLayer::refreshColors()
{
    m_state.FriendlyTeamTopRed = hud::teamTopColors[2].r;
    m_state.FriendlyTeamTopGreen = hud::teamTopColors[2].g;
    m_state.FriendlyTeamTopBlue = hud::teamTopColors[2].b;
    m_state.FriendlyTeamTopAlpha = hud::teamTopColors[2].a;
    m_state.FriendlyTicketRed = hud::ticketColors[2].r;
    m_state.FriendlyTicketGreen = hud::ticketColors[2].g;
    m_state.FriendlyTicketBlue = hud::ticketColors[2].b;
    m_state.FriendlyTicketAlpha = hud::ticketColors[2].a;
    m_state.EnemyTeamTopRed = hud::teamTopColors[1].r;
    m_state.EnemyTeamTopGreen = hud::teamTopColors[1].g;
    m_state.EnemyTeamTopBlue = hud::teamTopColors[1].b;
    m_state.EnemyTeamTopAlpha = hud::teamTopColors[1].a;
    m_state.EnemyTicketRed = hud::ticketColors[1].r;
    m_state.EnemyTicketGreen = hud::ticketColors[1].g;
    m_state.EnemyTicketBlue = hud::ticketColors[1].b;
    m_state.EnemyTicketAlpha = hud::ticketColors[1].a;
    m_state.EnemyTextRed = hud::textColors[1].r;
    m_state.EnemyTextGreen = hud::textColors[1].g;
    m_state.EnemyTextBlue = hud::textColors[1].b;
    m_state.EnemyTextAlpha = hud::textColors[1].a;
    m_state.FriendlyTextRed = hud::textColors[2].r;
    m_state.FriendlyTextGreen = hud::textColors[2].g;
    m_state.FriendlyTextBlue = hud::textColors[2].b;
    m_state.FriendlyTextAlpha = hud::textColors[2].a;
    m_state.EnemyBrightRed = hud::brightColors[1].r;
    m_state.EnemyBrightGreen = hud::brightColors[1].g;
    m_state.EnemyBrightBlue = hud::brightColors[1].b;
    m_state.EnemyBrightAlpha = hud::brightColors[1].a;
    m_state.FriendlyBrightRed = hud::brightColors[2].r;
    m_state.FriendlyBrightGreen = hud::brightColors[2].g;
    m_state.FriendlyBrightBlue = hud::brightColors[2].b;
    m_state.FriendlyBrightAlpha = hud::brightColors[2].a;
}

// ..463BCA; complete instructions [INFERRED] name.
void HudInformationLayer::activateHud()
{
    // Preserve the stores and the game lookup preceding the optional HUD guard.
    const bool alternateBorder = !m_state.MapMinSize;
    m_state.MapShow = true;
    m_state.MapBorderShow = true;
    m_state.MapBorderAlternateShow = alternateBorder;
    m_state.PlayerScoreShow = true;
    m_state.BarTopRightShow = true;
    m_state.BarTopLeftShow = true;
    m_state.PlayerKitIconShow = true;
    m_state.PlayerRankIconShow = true;
    m_state.ObjectiveIconShow = true;
    m_state.ShowIngameHud = true;
    // PE +2D7 is the last byte of the preserved overlapping weapon flags.
    m_state.weaponFlags[31] = game->getCurrentMapGameMode() == "sp1";
    if (!hudManager) return;
    if (hudManager->getVehicleInformation())
        hudManager->getVehicleInformation()->unknown0C();
    if (hudManager->getAmmo())
        hudManager->getAmmo()->unknown0C();
    for (int i = 0; i < 10; ++i) {
        const std::string icon = formatString("Ingame/Scoreboard/Optionbar/option_active0.tga");
        updateString(&m_state.optionIcons[i], icon.c_str());
    }
    const std::string& mapName = game->getCurrentMapName();
    const std::wstring level = hudManager->getLocalizedString(
        formatString("HUD_LEVELNAME_%s", mapName.c_str()));
    updateWideString(&m_state.ActiveLevel, level.c_str());
}

// ..463D2F. Deliberately no hudManager null guard.
void HudInformationLayer::deactivateHud()
{
    m_state.ShowIngameHud = false;
    if (hudManager->getVehicleInformation())
        hudManager->getVehicleInformation()->unknown10();
    if (hudManager->getAmmo())
        hudManager->getAmmo()->unknown10();
    for (int i = 0; i < 10; ++i) {
        const std::string icon = formatString("Ingame/Scoreboard/Optionbar/option_active0.tga");
        updateString(&m_state.optionIcons[i], icon.c_str());
    }
}

// ..464352. The sentinel tests compare address identity, not emptiness;
// the GeneralSettings lookup occurs even when an unset string will return early.
void HudInformationLayer::setServerFavorite(bool favourite)
{
    GeneralSettings* generalSettings;
    {
        const std::string singletonName("GeneralSettings");
        generalSettings = static_cast<GeneralSettings*>(classManager->getSingleton(singletonName));
    }
    if (m_state.ServerNameString == emptyString) return;
    if (m_state.ServerIPString == emptyString) return;
    if (favourite) {
        const std::string name(m_state.ServerNameString);
        const std::string address(m_state.ServerIPString);
        const int queryPort = serverSettings->m_gameSpyPort; // PE +168, ELF +A8
        generalSettings->addFavouriteServer(address, queryPort, name);
        return;
    }
    const std::string address(m_state.ServerIPString);
    const int queryPort = serverSettings->m_gameSpyPort;
    //  dispatches GeneralSettings +130(address const&, queryPort).
    generalSettings->removeFavouriteServer(address, queryPort);
}


namespace {
// . The floating comparisons deliberately retain the unordered arm.
// Names are inferred; all instructions were read. The return is discarded by 464AA0.
int normalizeHudDirection(Math::Vec3& value)
{
    const float lengthSquared = static_cast<float>(
        static_cast<double>(value.x) * value.x +
        static_cast<double>(value.y) * value.y +
        static_cast<double>(value.z) * value.z);
    const double epsilon = 1.1920928955078125e-7;
    if (!(std::fabs(static_cast<double>(lengthSquared) - 1.0) < epsilon)) {
        if (std::fabs(static_cast<double>(lengthSquared)) < epsilon) {
            value.x = value.y = value.z = 0.0f;
            return -20001;
        }
        const double inverseLength = 1.0 / std::sqrt(static_cast<double>(lengthSquared));
        value.x = static_cast<float>(inverseLength * value.x);
        value.y = static_cast<float>(inverseLength * value.y);
        value.z = static_cast<float>(inverseLength * value.z);
    }
    return 0;
}

// . The X difference is stored to float; Y and Z remain on x87.
double hudControlPointDistance(const Math::Vec3& a, const Math::Vec3& b)
{
    const float x = static_cast<float>(static_cast<double>(b.x) - a.x);
    const double y = static_cast<double>(b.y) - a.y;
    const double z = static_cast<double>(b.z) - a.z;
    return std::sqrt(z * z + static_cast<double>(x) * x + y * y);
}
}

// shared by update and ECHud52. The vehicle and armor checks are
// present; the subsequent camera pointer is deliberately unchecked.
void HudInformationLayer::updateHitDirection(bool show, float delta)
{
    m_state.HitDirectionShow = show;
    if (show) {
        world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            world::IObject* const vehicle = player->getVehicle();
            if (vehicle && vehicle->getArmorComponent()) {
                const Mat4& transformation = player->getCamera()->getTransformation(0.0f);
                Math::Vec3 forward = {transformation.m[8], 0.0f, transformation.m[10]};
                normalizeHudDirection(forward);
                const float angle = static_cast<float>(std::fmod(
                    std::atan2(static_cast<double>(forward.x), static_cast<double>(forward.z)),
                    6.283185482025146484375));
                m_state.HitDirectionAngle = static_cast<float>(
                    static_cast<double>(angle) - player->getHitFromDirection());
            }
        }
    }
    if (!(m_state.field19c <= 0.0f)) {
        if (m_state.HitDirectionShow)
            m_state.field19c = static_cast<float>(static_cast<double>(m_state.field19c) - delta);
    } else {
        m_state.field19c = m_state.field1a4;
        m_state.HitDirectionShow = false;
    }
}

// . Native map type also attested by GameLogic::getControlPointMap
// ELF80D5B10/PE464F30; never reinterpret a retail STL node.
void HudInformationLayer::findClosestControlPoint()
{
    m_state.weakTarget = WeakPtr<world::IObject>();
    float closestDistance = 1000.0f;
    if (!gameLogic) return;
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    std::map<unsigned int, world::IObject*>* const points = gameLogic->getControlPointMap();
    for (auto it = points->begin(); it != points->end(); ++it) {
        world::IObject* const point = it->second;
        if (point && point->queryInterface(world::IID_IControlPoint) &&
            point->getTemplate()->queryInterface(world::IID_IControlPointTemplate) &&
            player->getVehicle()) {
            world::IObject* const vehicle = player->getVehicle();
            const Math::Vec3 pointPosition = point->getAbsolutePosition();
            const Math::Vec3 vehiclePosition = vehicle->getAbsolutePosition();
            if (isWithinRadius(vehiclePosition, pointPosition, closestDistance)) {
                m_state.weakTarget = point->getWeakPtr();
                world::IObject* const updatedVehicle = player->getVehicle();
                const Math::Vec3& updatedPointPosition = point->getAbsolutePosition();
                const Math::Vec3& updatedVehiclePosition = updatedVehicle->getAbsolutePosition();
                closestDistance = static_cast<float>(hudControlPointDistance(
                    updatedVehiclePosition, updatedPointPosition));
            }
        }
    }
}

// . Allied means numeric team2 here, even if the local player is1.
void HudInformationLayer::setCloseToControlPointTeam(int team)
{
    m_state.CloseToAlliedCP = team == 2;
    m_state.CloseToNeutralCP = team == 0;
    m_state.CloseToAxisCP = team == 1;
    if (team >= 0) {
        findClosestControlPoint();
        if (m_state.weakTarget.get()) {
            m_state.CPInterfaceEnabled = true;
            hudManager->enableVOHelpMessage(48);
            return;
        }
    }
    m_state.CPInterfaceEnabled = false;
}

// . Events2..51,53..69 and every out-of-range ID do nothing.
void HudInformationLayer::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    switch (id) {
    case 1:
        if (hudManager->getMap())
            setCloseToControlPointTeam(hudManager->getMap()->getCloseToControlPointTeam());
        break;
    case 52:
        m_state.field19c = m_state.field1a4;
        updateHitDirection(true, 0.0f);
        break;
    case 71: {
        const int team = *static_cast<int*>(data); // no payload-null guard in retail
        if (team < 1 || team > 2 || team != m_state.PlayerTeam) break;
        const int state = static_cast<world::IScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->m_ticketState;
        // PE464640 adds10 for every negative input, including negative multiples.
        const int level = state >= 0 ? state % 10 : 10 + state % 10;
        if (level)
            game->localGameInfoMessage(static_cast<TargetChannel>(14),
                formatString("AUTO_RULES_ticketlevel_%i", level));
        break;
    }
    case 70: {
        const int team = *static_cast<int*>(data);
        if (team != m_state.PlayerTeam) break;
        const bool bleeding = static_cast<world::IScoreManager*>(world::scoreManager)
            ->getTeamScore(team)->m_ticketBleed;
        game->localGameInfoMessage(static_cast<TargetChannel>(14),
            std::string(bleeding ? "AUTO_RULES_ticketbleedstart" : "AUTO_RULES_ticketbleedend"));
        break;
    }
    }
}


// PE464C60; name inferred. No receiver state is read by the original helper.
world::WeaponHudCompTemplate* HudInformationLayer::getKitWeaponHud(
    world::KitTemplate* kit, int level)
{
    if (!kit) return 0;
    std::vector<world::IObjectTemplate*>* const items = kit->getUnlockItems(level);
    for (auto it = items->begin(); it != items->end(); ++it) {
        world::IItemTemplate* const item = static_cast<world::IItemTemplate*>(
            (*it)->queryInterface(world::IID_IItemTemplate)); // *it unchecked here
        if (item) {
            const int defaultWeapon = kit->m_defaultWeaponIndex;
            if (item->getItemIndex() == defaultWeapon) {
                world::IObjectTemplate* const objectTemplate = *it;
                if (objectTemplate && objectTemplate->getClassID() == world::CID_GenericFireArmTemplate) {
                    return static_cast<world::WeaponHudCompTemplate*>(
                        objectTemplate->queryComponent(world::IID_IWeaponHudCompTemplate));
                }
                return 0;
            }
        }
    }
    return 0;
}

// PE468510, complete instruction list. All STL objects below are native.
void HudInformationLayer::updateKits(int team)
{
    world::IPlayer* const player = world::playerManager->getLocalHumanPlayer();
    if (!player) return;
    if (!(m_state.KitsShow && gameLogic)) {
        for (int i = 0; i < 7; ++i)
            m_state.kitShow[i] = m_state.KitsShow;
        return;
    }
    for (int kitIndex = 0; kitIndex < 7; ++kitIndex) {
        m_state.kitShow[kitIndex] = false;
        world::IObjectTemplate* const base = gameLogic->getKitTemplate(team, kitIndex);
        if (!base) continue;
        world::VehicleHudCompTemplate* const kitHud = static_cast<world::VehicleHudCompTemplate*>(
            base->queryComponent(world::IID_IVehicleHudCompTemplate));
        if (!kitHud) continue;
        world::KitTemplate* const kit = static_cast<world::KitTemplate*>(base);
        bool unlockPrimaryFound = false;
        const int chosenLevel = player->getChosenUnlockLevel(kitIndex, team);
        player->isUnlocked(kitIndex, team, chosenLevel + 1); // result discarded by retail
        int abilityCount = 0;
        std::map<int, std::string> itemIcons;
        std::map<std::string, int> iconItems;
        itemIcons.clear();
        iconItems.clear();
        updateWideString(&m_state.kitNames[kitIndex],
            hudManager->getLocalizedString(kitHud->getHudSid()).c_str());
        updateString(&m_state.kitIcons[kitIndex],
            static_cast<std::string*>(kitHud->getVehicleIcon(0))->c_str());
        for (int i = 0; i < kitHud->getAbilityIconNumber() && abilityCount < 5; ++i) {
            const char** const destination = &m_state.kitAbilityIcons[kitIndex * 5 + abilityCount];
            updateString(destination, static_cast<std::string*>(kitHud->getAbilityIcon(i))->c_str());
            if (std::strcmp("", *destination)) {
                iconItems.insert(std::make_pair(std::string(*destination), 0));
                m_state.kitAbilityShow[kitIndex * 5 + abilityCount] = true;
                ++abilityCount;
            }
        }
        std::vector<world::IObjectTemplate*>* const unlocks = kit->getUnlockItems(chosenLevel);
        for (auto it = unlocks->begin(); it != unlocks->end(); ++it) {
            world::IItemTemplate* const item = static_cast<world::IItemTemplate*>(
                (*it)->queryInterface(world::IID_IItemTemplate));
            world::IObjectTemplate* const itemTemplate = static_cast<world::IObjectTemplate*>(
                item->queryInterface(world::IID_IObjectTemplate)); // item unchecked in retail
            if (!itemTemplate || itemTemplate->getClassID() != world::CID_GenericFireArmTemplate)
                continue;
            world::WeaponHudCompTemplate* const weaponHud = static_cast<world::WeaponHudCompTemplate*>(
                itemTemplate->queryComponent(world::IID_IWeaponHudCompTemplate));
            if (!weaponHud) continue;
            const int defaultWeapon = kit->m_defaultWeaponIndex;
            if (item->getItemIndex() == defaultWeapon) {
                updateString(&m_state.kitWeapon[kitIndex], weaponHud->getWeaponIcon().c_str());
                unlockPrimaryFound = true;
            }
            if (abilityCount < 5 &&
                iconItems.find(weaponHud->getSpecialAbilityIcon()) == iconItems.end() &&
                itemIcons.find(item->getItemIndex()) == itemIcons.end()) {
                const char** const destination = &m_state.kitAbilityIcons[kitIndex * 5 + abilityCount];
                updateString(destination, weaponHud->getSpecialAbilityIcon().c_str());
                if (std::strcmp("", *destination)) {
                    const int itemIndex = item->getItemIndex();
                    itemIcons.insert(std::make_pair(itemIndex, std::string(*destination)));
                    const int repeatedItemIndex = item->getItemIndex();
                    iconItems.insert(std::make_pair(std::string(*destination), repeatedItemIndex));
                    m_state.kitAbilityShow[kitIndex * 5 + abilityCount] = true;
                    ++abilityCount;
                }
            }
        }
        for (auto it = kit->m_templateInfos.begin(); it != kit->m_templateInfos.end(); ++it) {
            if (abilityCount >= 5) break;
            world::IObjectTemplate* const itemTemplate = world::objectTemplateManager->getTemplate(
                (*it)->m_templateName); // result unchecked in retail
            world::IItemTemplate* const item = static_cast<world::IItemTemplate*>(
                itemTemplate->queryInterface(world::IID_IItemTemplate));
            if (!item) continue;
            if (itemIcons.find(item->getItemIndex()) != itemIcons.end() ||
                itemTemplate->getClassID() != world::CID_GenericFireArmTemplate)
                continue;
            world::WeaponHudCompTemplate* const weaponHud = static_cast<world::WeaponHudCompTemplate*>(
                itemTemplate->queryComponent(world::IID_IWeaponHudCompTemplate));
            if (!weaponHud) continue;
            const int defaultWeapon = kit->m_defaultWeaponIndex;
            if (item->getItemIndex() == defaultWeapon && !unlockPrimaryFound)
                updateString(&m_state.kitWeapon[kitIndex], weaponHud->getWeaponIcon().c_str());
            if (iconItems.find(weaponHud->getSpecialAbilityIcon()) == iconItems.end()) {
                const char** const destination = &m_state.kitAbilityIcons[kitIndex * 5 + abilityCount];
                updateString(destination, weaponHud->getSpecialAbilityIcon().c_str());
                if (std::strcmp("", *destination)) {
                    const int itemIndex = item->getItemIndex();
                    itemIcons.insert(std::make_pair(itemIndex, std::string(*destination)));
                    const int repeatedItemIndex = item->getItemIndex();
                    iconItems.insert(std::make_pair(std::string(*destination), repeatedItemIndex));
                    m_state.kitAbilityShow[kitIndex * 5 + abilityCount] = true;
                    ++abilityCount;
                }
            }
        }
        // Retail's do/while writes even when abilityCount==5: it clears the next
        // kit's first flag; for kit6 that byte aliases kitUnlockArrowShow[0].
        // Access the containing object representation so the preserved write
        // does not introduce an out-of-bounds C++ bool-array operation.
        do {
            reinterpret_cast<unsigned char*>(&m_state)[offsetof(State, kitAbilityShow) +
                kitIndex * 5 + abilityCount] = 0;
            ++abilityCount;
        } while (abilityCount < 5);
        updateString(&m_state.kitAltWeapon[kitIndex],
            formatString("%s", "Ingame/GeneralIcons/empty.tga").c_str());
        m_state.kitSprint[kitIndex] = static_cast<float>(
            1.0 - static_cast<double>(kit->m_sprintStaminaDissipationFactor));
        const bool unlockShow = m_state.KitsShow && kit->getHighestUnlockLevel() > 0;
        m_state.kitUnlockShow[kitIndex] = unlockShow;
        if (unlockShow) {
            const int alternateLevel = chosenLevel == 0 ? kit->getHighestUnlockLevel() : 0;
            world::WeaponHudCompTemplate* const alternate = getKitWeaponHud(kit, alternateLevel);
            if (alternate)
                updateString(&m_state.kitAltWeapon[kitIndex], alternate->getAltWeaponIcon().c_str());
        }
        const bool showArrow = m_state.KitsShow && m_state.kitUnlockShow[kitIndex] &&
            player->isUnlocked(kitIndex, team, 1);
        m_state.kitUnlockArrowShow[kitIndex] = showArrow;
        m_state.kitShow[kitIndex] = m_state.KitsShow;
        if (showArrow)
            hudManager->enableVOHelpMessage(61);
    }
}


// Complete client PE instructions read. Names [INFERRED].
void HudInformationLayer::updateControlPointCounts() // PE465520
{
    int friendly = 0, enemy = 0, total = 0;
    if (gameLogic) {
        const std::map<unsigned int, world::IObject*>& points = *gameLogic->getControlPointMap();
        for (auto it = points.begin(); it != points.end(); ++it) {
            if (it->second) {
                auto* point = static_cast<world::IControlPoint*>(it->second->queryInterface(world::IID_IControlPoint));
                if (point) {
                    if (point->getTeam() == m_state.PlayerTeam) ++friendly;
                    else if (point->getTeam()) ++enemy;
                    ++total;
                }
            }
        }
    }
    if (!hudManager || hudManager->getMaximumNrOfCPs() <= 0) {
        m_state.FriendlyCPs = 0;
        m_state.EnemyCPs = 0;
        m_state.FriendlyNeutralCPs = 0;
        m_state.EnemyNeutralCPs = 0;
        m_state.FriendlyEnemyCPs = 0;
        m_state.EnemyFriendlyCPs = 0;
        return;
    }
    const int neutral = total - enemy - friendly;
    updateString(&m_state.FriendlyCPString, formatString("%i", friendly).c_str());
    updateString(&m_state.EnemyCPString, formatString("%i", enemy).c_str());
    updateString(&m_state.NeutralCPString, formatString("%i", neutral).c_str());
    const float neutralHalf = static_cast<float>(static_cast<double>(neutral) * 0.5);
    int friendlyNeutral = static_cast<int>(std::floor(static_cast<double>(neutral) * 0.5));
    int enemyNeutral = static_cast<int>(std::ceil(neutralHalf));
    const int friendlyCapacity = static_cast<int>(std::floor(
        static_cast<double>(hudManager->getMaximumNrOfCPs()) * 0.5));
    const int enemyCapacity = static_cast<int>(std::ceil(
        static_cast<double>(hudManager->getMaximumNrOfCPs()) * 0.5));
    const float totalHalf = static_cast<float>(static_cast<double>(total) * 0.5);
    const int friendlyExtra = friendly - static_cast<int>(std::floor(static_cast<double>(total) * 0.5));
    const int enemyExtra = enemy - static_cast<int>(std::ceil(totalHalf));
    if (friendlyExtra >= 0 && friendlyNeutral > 0) {
        enemyNeutral += friendlyNeutral;
        friendlyNeutral = 0;
    }
    if (enemyExtra >= 0 && enemyNeutral > 0) {
        friendlyNeutral += enemyNeutral;
        enemyNeutral = 0;
    }
    m_state.FriendlyEnemyCPs = friendlyExtra > 0
        ? static_cast<float>(static_cast<double>(friendlyExtra) / enemyCapacity) : 0.0f;
    m_state.EnemyFriendlyCPs = enemyExtra > 0
        ? static_cast<float>(static_cast<double>(enemyExtra) / friendlyCapacity) : 0.0f;
    const double friendlyNeutralShare = static_cast<double>(friendlyNeutral) / friendlyCapacity
        + m_state.EnemyFriendlyCPs;
    m_state.FriendlyNeutralCPs = static_cast<float>(friendlyNeutralShare);
    // PE465862 spills the denominator and465871 spills this intermediate.
    const float enemyDenominator = static_cast<float>(enemyCapacity);
    const float enemyNeutralShare = static_cast<float>(
        static_cast<double>(enemyNeutral) / enemyDenominator + m_state.FriendlyEnemyCPs);
    m_state.EnemyNeutralCPs = enemyNeutralShare;
    m_state.FriendlyCPs = static_cast<float>(static_cast<double>(friendly) / friendlyCapacity
        + friendlyNeutralShare - m_state.FriendlyEnemyCPs);
    m_state.EnemyCPs = static_cast<float>(static_cast<double>(enemy) / enemyDenominator
        + enemyNeutralShare - m_state.EnemyFriendlyCPs);
}

void HudInformationLayer::updateLevelTime() // PE464870
{
    if (!gameLogic) return;
    m_state.TimeInfoShow = gameLogic->getTimeToNextStateChange() > 0.0f;
    if (!m_state.TimeInfoShow) return;
    const int seconds = static_cast<int>(gameLogic->getTimeToNextStateChange());
    int remainder = seconds % 3600;
    if (seconds < 0) remainder += 3600;
    const int hours = (seconds - remainder) / 3600;
    const int afterHours = seconds - hours * 3600;
    remainder = afterHours % 60;
    if (afterHours < 0) remainder += 60;
    const int minutes = (afterHours - remainder) / 60;
    const int afterMinutes = seconds - 60 * (minutes + 60 * hours);
    std::string hoursString(""), secondsString(""), minutesString("");
    if (hours > 0) hoursString = formatString("%d:", hours);
    if (hours > 0 || minutes > 0) minutesString = formatString("%02d:", minutes);
    else minutesString = formatString("");
    secondsString = formatString("%02d", afterMinutes);
    updateString(&m_state.LevelTime,
        formatString("%s%s%s", hoursString.c_str(), minutesString.c_str(), secondsString.c_str()).c_str());
}

void HudInformationLayer::updateTickets() // PE4646B0
{
    updateString(&m_state.FriendlyTicketsString, formatString("%d",
        static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(m_state.PlayerTeam)->m_tickets).c_str());
    updateString(&m_state.EnemyTicketsString, formatString("%d",
        static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore((m_state.PlayerTeam != 2) + 1)->m_tickets).c_str());
    m_state.field88 = static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(m_state.PlayerTeam)->m_ticketBleed;
    m_state.field90 = static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore((m_state.PlayerTeam != 2) + 1)->m_ticketBleed;
    const int state = static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(m_state.PlayerTeam)->m_ticketState;
    int remainder = state % 10;
    if (state < 0) remainder += 10; // Negative exact multiples also receive +10.
    const int level = (state - remainder) / 10;
    // Plays or stops the low-ticket warning through the sound facade.
    if (level == 1 && !m_state.field98) {
        const std::string sound("lowOnTickets");
        g_bfSound->playSound(sound);                       // PE464816: g_bfSound +2C
        m_state.field98 = true;  // retail's post-call store
    } else if (level == 0 && m_state.field98) {
        const std::string sound("lowOnTickets");
        g_bfSound->stopSound(sound);                       // PE464851: g_bfSound +34
        m_state.field98 = false; // retail's post-call store
    }
}

void HudInformationLayer::updateKitUnlockBlink(float dt) // PE463D30
{
    if (m_state.field2b0) return;
    for (int i = 0; i < 7; ++i) {
        if (!m_state.kitUnlockArrowShow[i]) continue;
        if ((m_state.kitUnlockAlpha[i] >= 1.0f && m_state.field2b4 % 2 != 1)
            || (m_state.kitUnlockAlpha[i] <= 0.0f && m_state.field2b4 % 2 == 1))
            ++m_state.field2b4;
        if (m_state.field2b4 % 2 == 1)
            m_state.kitUnlockAlpha[i] = static_cast<float>(static_cast<double>(m_state.kitUnlockAlpha[i]) - dt);
        else
            m_state.kitUnlockAlpha[i] = static_cast<float>(static_cast<double>(dt) + m_state.kitUnlockAlpha[i]);
        if (m_state.field2b4 >= 8) {
            m_state.kitUnlockAlpha[i] = 0;
            m_state.field2b0 = true;
        }
    }
}

void HudInformationLayer::updateControlPointProgress() // PE464CF0
{
    world::IObject* object = m_state.weakTarget.get();
    world::IControlPoint* point = object
        ? static_cast<world::IControlPoint*>(object->queryInterface(world::IID_IControlPoint)) : nullptr;
    if (!point) {
        m_state.TimePreCPIsNeutral = 0;
        m_state.TimePreCPIsTaken = 0;
        m_state.CPInterfaceEnabled = false;
        return;
    }
    object->getTemplate()->queryInterface(world::IID_IControlPointTemplate); // Retail discards result.
    if (point->getCapturingState() != m_state.PlayerTeam && point->getCapturingState())
        m_state.TimePreCPIsNeutral = point->getTakeOverCounter();
    else
        m_state.TimePreCPIsNeutral = 0;
    if (point->getCapturingState() == m_state.PlayerTeam && !point->getTeam())
        m_state.TimePreCPIsTaken = point->getTakeOverCounter();
    else if (point->getCapturingState() == m_state.PlayerTeam)
        m_state.TimePreCPIsTaken = 1;
    else
        m_state.TimePreCPIsTaken = 0;
}

void HudInformationLayer::refreshServerFavorite() // PE464360
{
    auto* settings = static_cast<GeneralSettings*>(classManager->getSingleton("GeneralSettings"));
    const std::string address(m_state.ServerIPString);
    const int queryPort = serverSettings->m_gameSpyPort;
    m_state.ServerIsFavourite = settings->isFavouriteServer(address, queryPort);   // : +128
}


// PE4668D0..467725; complete instructions read.
void HudInformationLayer::update(float dt)
{
    if (!gameLogic) return;
    if (hudManager->getVehicleInformation()) {
        m_state.MapFullSizeAndNotSpawnShow = m_state.MapFullSize && !m_state.SpawnShow
            && !hudManager->getVehicleInformation()->m_vehicleIconShow;
    }
    m_state.MapFullSizeAndSpawnShow = m_state.MapFullSize && m_state.SpawnShow;
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (hudManager->getMap()) {
        m_state.MapFullSize = hudManager->getMap()->m_field68c;
        m_state.MapMinSize = hudManager->getMap()->m_field68d;
        m_state.MapBorderAlternateShow = !m_state.MapMinSize;
    }
    if (hudManager->getMode() == 11) {
        const int remaining = static_cast<int>(std::ceil(gameLogic->getTimeUntilRestartMap()));
        updateString(&m_state.RoundRestartString, formatString("%is to round restart", remaining).c_str());
    }
    m_blinkers->update(dt);
    if (player) {
        refreshColors();
        m_state.PlayerTeam = hudManager->getLocalPlayerTeam();
        updateControlPointCounts();
        updateLevelTime();
        updateTickets();
        updateKitUnlockBlink(dt);
        m_state.SuicideButtonShow = m_state.MapFullSizeAndSpawnShow && player->getIsAlive()
            && !hudManager->isPlayerAlive(player);
        if (hudManager->getMode() == 9) {
            updateString(&m_state.FriendlyTeamWinsString, formatString("%d",
                static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore(m_state.PlayerTeam)->m_roundsWon).c_str());
            updateString(&m_state.EnemyTeamWinsString, formatString("%d",
                static_cast<world::IScoreManager*>(world::scoreManager)->getTeamScore((m_state.PlayerTeam != 2) + 1)->m_roundsWon).c_str());
        }
        world::PlayerScoreData* score = player->getScoreData();
        if (score) updateString(&m_state.LocalScore, formatString("%i", score->m_score).c_str());
        if (m_state.LevelsListShow) {
            updateString(&m_state.TotalNoOfPlayers, formatString("%d", world::playerManager->getNumberOfPlayers()).c_str());
            if (ClientVoteManager::getInstance()->m_dword04 == 1) {
                updateString(&m_state.YesVotes, formatString("%d", ClientVoteManager::getInstance()->getNumYesVotesCasted()).c_str());
                updateString(&m_state.NoVotes, formatString("%d", ClientVoteManager::getInstance()->getNumNoVotesCasted()).c_str());
            } else {
                updateString(&m_state.YesVotes, "-");
                updateString(&m_state.NoVotes, "-");
            }
        } else {
            hud::HudVoteMap* voteMap = hudManager->getVoteMap();
            if (voteMap) voteMap->m_selected = false; // PE785300(0): only write+24; RET4.
        }
        if (m_state.CPInterfaceEnabled) updateControlPointProgress();
        switch (static_cast<int>(gameLogic->getBeginRoundState())) {
        case 1: {
            int connected = 0;
            const auto& players = world::playerManager->getPlayers();
            for (auto it = players.begin(); it != players.end(); ++it) {
                if (*it && (*it)->getIsConnected()) ++connected;
            }
            int needed = static_cast<int>(serverInfo->m_block.numPlayersNeededToStart) - connected;
            if (needed < 0) needed = 0;
            else if (needed > 1000) needed = 1000;
            const std::string count = formatString("%d", needed);
            hudManager->showConnectionMessage(hudManager->replaceWideTokenFromString(
                hudManager->getLocalizedString("HUD_STARTOFROUND_NRPLAYERSNEEDED"), "#NROFPLAYERS#", count));
            break;
        }
        case 2:
            hudManager->showConnectionMessage(hudManager->replaceWideTokenFromString(
                hudManager->getLocalizedString("HUD_COMMANDERAPPLY_TIMETOSTART"), "#TIME#", m_state.LevelTime));
            break;
        case 3: {
            if (gameLogic->getGameStatus() == 1) hudManager->clearConnectionMessage();
            const double time = player->getIsAlive() ? player->getManDownTimeLeft() : player->getTimeToSpawn();
            int remaining = static_cast<int>(std::ceil(time));
            if (remaining < 0) remaining = 0;
            else if (remaining > 120) remaining = 120;
            const std::wstring label = hudManager->getLocalizedString("HUD_TEXT_MENU_TIME_TO_RESPAWN");
            updateWideString(&m_state.TimeToSpawnString, formatString(L"%s %2d", label.c_str(), remaining).c_str());
            break;
        }
        default:
            hudManager->clearConnectionMessage();
            break;
        }
        if (player->getIsAlive()) {
            m_state.SquadOptionsShow = player->getIsSquadLeader() & m_state.ScoreboardShow;
            updateString(&m_state.ScoreString, formatString("%d", player->getScoreData()->m_score).c_str());
            CombatAreaManager* combat = CombatAreaManager::getInstance();
            if (combat && player->getTimeOutSideWorld() > 0.0f && combat->m_useCombatAreas) {
                float permitted = combat->getTimeAllowedOutside(player->getVehicle());
                const double difference = static_cast<double>(permitted) - player->getTimeOutSideWorld();
                float remaining;
                // PE46707C takes the recompute arm for unordered as well as >=0.
                if (difference < 0.0) remaining = 0;
                else {
                    permitted = combat->getTimeAllowedOutside(player->getVehicle());
                    remaining = static_cast<float>(static_cast<double>(permitted) - player->getTimeOutSideWorld());
                }
                const std::string time = formatString("%d", static_cast<int>(std::ceil(remaining)));
                updateWideString(&m_state.SpawnInfoString, hudManager->replaceWideTokenFromString(
                    hudManager->getLocalizedString("HUD_CENTERINFOBOX_timeuntildamage"), "#TIME#", time).c_str());
                if (gameLogic->getGameStatus() == 1)
                    hudManager->showConnectionMessage(hudManager->getLocalizedString("AUTO_MOODGP_leavingcombatarea"));
                if (!m_state.SpawnInfoShow && !player->getIsAIPlayer()) {
                    const std::string message("leaving_the_combat_area");
                    game->localRadioMessage(static_cast<TargetChannel>(2), message);
                }
                m_state.SpawnInfoShow = hudManager->getMode() != 12 && hudManager->getMode() != 11;
            } else m_state.SpawnInfoShow = false;
        } else {
            if (player->getSpawnGroup() < 1) {
                if (hudManager->getKitSelection()
                    && (hudManager->getMode() == 1 || hudManager->getMode() == 18
                        || hudManager->getMode() == 13 || hudManager->getMode() == 17)
                    ) {
                    hud::HudKitSelection* selection = hudManager->getKitSelection();
                    const bool selected = selection->m_selectedSpawnGroup[selection->m_selectedTeam] > 0;
                    updateWideString(&m_state.SpawnInfoString,
                        hudManager->getLocalizedString(selected ? "HUD_CENTERINFOBOX_presstospawn"
                            : "HUD_CENTERINFOBOX_selectspawnpoint").c_str());
                } else {
                    updateWideString(&m_state.SpawnInfoString,
                        hudManager->getLocalizedString("HUD_CENTERINFOBOX_selectspawnpoint").c_str());
                }
                m_state.SpawnInfoShow = hudManager->getMode() != 12 && hudManager->getMode() != 11;
            } else {
                hud::HudKitSelection* selection = hudManager->getKitSelection();
                const int selected = selection->m_selectedSpawnGroup[selection->m_selectedTeam];
                if (selected != player->getSpawnGroup()) {
                    updateWideString(&m_state.SpawnInfoString,
                        hudManager->getLocalizedString("HUD_CENTERINFOBOX_invalidspawnpoint").c_str());
                } else if (player->getTimeToSpawn() > 0.0f) {
                    const float remaining = player->getTimeToSpawn();
                    const std::string time = formatString("%i", static_cast<int>(std::ceil(remaining)));
                    updateWideString(&m_state.SpawnInfoString, hudManager->replaceWideTokenFromString(
                        hudManager->getLocalizedString("HUD_CENTERINFOBOX_timetospawn"), "#TIME#", time).c_str());
                    m_state.SpawnInfoShow = hudManager->getMode() != 12 && hudManager->getMode() != 11;
                } else {
                    updateWideString(&m_state.SpawnInfoString,
                        hudManager->getLocalizedString("HUD_CENTERINFOBOX_instantspawn").c_str());
                }
            }
            m_state.CPInterfaceEnabled = false;
        }
        updateHitDirection(m_state.HitDirectionShow, dt);
    }
    refreshServerFavorite();
    if (!game) return;
    IGameClient* const client = static_cast<IGameClient*>(game->queryInterface(120002));
    if (client && game->isPlayingDemo()) {
        if (!m_state.DemoShow) m_state.DemoShow = true;
        m_state.MapMinSize = false;
        m_state.SpawnInfoShow = false;
        updateWideString(&m_state.TimeToSpawnString, L"");
        updateWideString(&m_state.DemoPausePlayString, hudManager->getLocalizedString(game->getSimulationRate()
            ? "HUD_HUD_DEMO_PAUSE" : "HUD_HUD_DEMO_PLAY").c_str());
        // The camera button names the other camera.
        if (client->getIsDemoFreeFlying()) {
            m_state.DemoPlayerNameShow = false;
            updateWideString(&m_state.DemoCameraString,
                hudManager->getLocalizedString("HUD_HUD_DEMO_PLAYER_CAM").c_str());
        } else {
            m_state.DemoPlayerNameShow = true;
            updateWideString(&m_state.DemoCameraString,
                hudManager->getLocalizedString("HUD_HUD_DEMO_FREE_CAM").c_str());
        }
        // The id is asked again for the lookup, as shipped.
        const char* name = "";
        if (client->getCurrentDemoPlayerId() != -1) {
            if (world::IPlayer* const followed =
                    world::playerManager->getPlayerFromId(client->getCurrentDemoPlayerId()))
                name = followed->getName().c_str();
        }
        updateString(&m_state.DemoPlayerNameString, name);
        return;
    }
    m_state.DemoPlayerNameShow = false;
    m_state.DemoShow = false;
    updateString(&m_state.DemoPlayerNameString, "");
}


HudInformationLayer::HudInformationLayer() : m_refCount(1), m_nextId(0)
{
    static_assert(sizeof(State) == 0x518, "Non-STL x86 suffix changed");
    m_state.field88 = false;
    m_state.FriendlyTicketBleed = 0;
    m_state.field90 = false;
    m_state.EnemyTicketBleed = 0;
    m_state.DeathMessage = emptyWideString;
    m_state.DisconnectMessage = emptyWideString;
    m_state.CPInterfaceEnabled = false;
    m_state.TimePreCPIsNeutral = 0;
    m_state.TimePreCPIsTaken = 0;
    m_state.FriendlyCPs = 0;
    m_state.EnemyCPs = 0;
    m_state.EnemyNeutralCPs = 0;
    m_state.FriendlyNeutralCPs = 0;
    m_state.EnemyFriendlyCPs = 0;
    m_state.FriendlyEnemyCPs = 0;
    m_state.FriendlyCPString = emptyString;
    m_state.EnemyCPString = emptyString;
    m_state.NeutralCPString = emptyString;
    m_state.FriendlyVictoryAlpha = 1.0000000f;
    m_state.EnemyVictoryAlpha = 1.0000000f;
    m_state.MapZoom = 1;
    m_state.ShowIngameHud = true;
    m_state.field19c = 0;
    m_state.HitDirectionAngle = 0;
    m_state.field1a4 = 0.10000000f;
    m_state.HitDirectionShow = false;
    m_state.TimeInfoShow = false;
    m_state.SetupShow = false;
    m_state.SayAllShow = false;
    m_state.SaySquadShow = false;
    m_state.SayTeamShow = false;
    m_state.ChatListShow = true;
    m_state.CloseToFlag = false;
    m_state.CloseToAlliedCP = false;
    m_state.CloseToAxisCP = false;
    m_state.CloseToNeutralCP = false;
    m_state.MedicInRadius = false;
    m_state.RepairInRadius = false;
    m_state.AmmoInRadius = false;
    m_state.CloseToMine = false;
    m_state.ShowRankUpIcon = false;
    m_state.ShowMedalIcon = false;
    m_state.PlayerRankIconShow = false;
    m_state.PlayerKitIconShow = false;
    m_state.RankUpIconPathString = emptyString;
    m_state.MedalIconPathString = emptyString;
    m_state.IngameHelpShow = false;
    m_state.IngameHelpString = emptyWideString;
    m_state.VictoryShow = false;
    m_state.VictoryRankShow = false;
    m_state.MapShow = false;
    m_state.MapBorderShow = false;
    m_state.MapBorderAlternateShow = false;
    m_state.MapStatic = false;
    m_state.SpawnShow = false;
    m_state.SpawnInfoShow = false;
    m_state.ScoreboardShow = false;
    m_state.VoipListShow = false;
    m_state.LevelsListShow = false;
    m_state.MapFullSize = false;
    m_state.MapMinSize = false;
    m_state.MapFullSizeAndNotSpawnShow = false;
    m_state.MapFullSizeAndSpawnShow = false;
    m_state.SuicideButtonShow = false;
    m_state.ObjectiveIconShow = false;
    m_state.SquadOptionsShow = false;
    m_state.JoinOptionShow = false;
    m_state.KickOptionShow = false;
    m_state.LeaveOptionShow = false;
    m_state.InviteOptionShow = false;
    m_state.MapMenuShow = false;
    m_state.SquadLeaderMenuShow = false;
    m_state.CommanderMenuShow = false;
    m_state.ChoiceMenuShow = false;
    m_state.CommanderShow = false;
    m_state.SquadInterfaceShow = false;
    m_state.RadioInterfaceShow = false;
    m_state.RadioVehicleInterfaceShow = false;
    m_state.SquadLeaderInterfaceShow = false;
    m_state.CommanderInterfaceShow = false;
    m_state.SpottedInterfaceShow = false;
    m_state.DemoShow = false;
    m_state.DemoCameraInterfaceShow = false;
    m_state.DemoPlayerNameShow = false;
    m_state.DemoRecInterfaceShow = false;
    m_state.DemoPlayerNameString = emptyString;
    m_state.DemoPausePlayString = emptyWideString;
    m_state.DemoCameraString = emptyWideString;
    m_state.MembersShow = false;
    m_state.KitsShow = false;
    m_state.EnemyTicketsShow = true;
    m_state.FriendlyTicketsShow = true;
    m_state.BarTopLeftShow = false;
    m_state.BarTopRightShow = false;
    m_state.PlayerScoreShow = false;
    m_state.Team1Selected = false;
    m_state.Team2Selected = false;
    m_state.PlayerTeam = 0;
    m_state.field2b0 = false;
    m_state.field2b4 = 0;
    m_state.weaponFlags[30] = false;
    m_state.weaponFlags[31] = false;
    m_state.LocalScore = emptyString;
    m_state.EnemyTicketsString = emptyString;
    m_state.FriendlyTicketsString = emptyString;
    m_state.ScoreString = emptyString;
    m_state.TimeToSpawnString = emptyWideString;
    m_state.SpawnInfoString = emptyWideString;
    m_state.LevelTime = emptyString;
    m_state.ActiveLevel = emptyWideString;
    m_state.FriendlyFlagIconPathString = emptyString;
    m_state.EnemyFlagIconPathString = emptyString;
    m_state.Team1FlagIconPathString = emptyString;
    m_state.Team2FlagIconPathString = emptyString;
    m_state.FriendlyTeamNameString = emptyString;
    m_state.EnemyTeamNameString = emptyString;
    m_state.Team1NameString = emptyWideString;
    m_state.Team2NameString = emptyWideString;
    m_state.RoundRestartString = emptyString;
    m_state.FriendlyTeamWinsString = emptyString;
    m_state.EnemyTeamWinsString = emptyString;
    m_state.ServerNameString = emptyString;
    m_state.ServerIPString = emptyString;
    m_state.ServerPortString = emptyString;
    m_state.field588 = -1;
    m_state.ServerIsFavourite = false;
    m_state.TotalNoOfPlayers = emptyString;
    m_state.YesVotes = emptyString;
    m_state.NoVotes = emptyString;
    m_state.ShowNightVisionGauge = false;
    m_state.field98 = false;
    m_state.WinHeadlineString = emptyWideString;
    m_state.MenuBackgroundRed = 0;
    m_state.MenuBackgroundGreen = 0;
    m_state.MenuBackgroundBlue = 0;
    m_state.MenuBackgroundAlpha = 0.69999999f;
    m_state.CrossHairColorRed = 1.0000000f;
    m_state.CrossHairColorGreen = 1.0000000f;
    m_state.CrossHairColorBlue = 0;
    m_state.CrossHairColorAlpha = 1.0000000f;
    m_state.MenuMapAlpha = 0.80000001f;
    m_state.MenuMapIconAlpha = 0.89999998f;
    for (int i = 0; i < 7; ++i) {
        m_state.kitSelected[i] = false;
        m_state.kitNames[i] = emptyWideString;
        m_state.kitIcons[i] = emptyString;
        m_state.kitWeapon[i] = emptyString;
        m_state.kitAltWeapon[i] = emptyString;
        m_state.kitUnlockArrowShow[i] = false;
        m_state.kitUnlockShow[i] = false;
        m_state.kitSprint[i] = 0;
        m_state.kitShow[i] = false;
        m_state.kitUnlockAlpha[i] = 0;
        for (int j = 0; j < 5; ++j) {
            m_state.kitAbilityShow[i * 5 + j] = false;
            m_state.kitAbilityIcons[i * 5 + j] = emptyString;
        }
        registerWideString(formatString("KitName%iString", i), &m_state.kitNames[i]);
        registerString(formatString("KitIcon%iPath", i), &m_state.kitIcons[i]);
        registerString(formatString("KitWeaponIcon%iPath", i), &m_state.kitWeapon[i]);
        registerString(formatString("KitAltWeaponIcon%iPath", i), &m_state.kitAltWeapon[i]);
        registerBool(formatString("KitUnlockArrow%iShow", i), &m_state.kitUnlockArrowShow[i]);
        registerBool(formatString("KitUnlock%iShow", i), &m_state.kitUnlockShow[i]);
        registerFloat(formatString("Kit%iSprintAbility", i), &m_state.kitSprint[i]);
        registerFloat(formatString("Kit%iUnlockBlinkAlpha", i), &m_state.kitUnlockAlpha[i]);
        registerBool(formatString("Kit%iShow", i), &m_state.kitShow[i]);
        registerBool(formatString("PlayerKitIcon%iSelectShow", i), &m_state.kitSelected[i]);
        for (int j = 0; j < 5; ++j) {
            registerBool(formatString("Kit%iAbilityIcon%iShow", i, j), &m_state.kitAbilityShow[i * 5 + j]);
            registerString(formatString("Kit%iAbilityIcon%iPathString", i, j), &m_state.kitAbilityIcons[i * 5 + j]);
        }
    }
    for (int i = 0; i < 12; ++i) {
        //  really writes twelve pointers beginning at +0x20C.
        // The last two overwrite eight bytes of adjacent HUD flags. Copy their
        // representation without inventing a larger source array or correcting
        // the shipped overrun. The destructor frees only the ten real pointers.
        const char* initial = emptyString;
        std::memcpy(reinterpret_cast<unsigned char*>(&m_state) + offsetof(State, optionIcons) + i * sizeof(initial),
                    &initial, sizeof(initial));
        m_state.radioNames[i] = emptyWideString;
        m_state.radioVehicleNames[i] = emptyWideString;
        m_state.squadNames[i] = emptyWideString;
        m_state.squadLeaderNames[i] = emptyWideString;
        m_state.commanderNames[i] = emptyWideString;
        m_state.weaponIcons[i] = emptyString;
        // ..0x469662: twelve iterations, ten-byte spacing. The
        // overlapping tail includes ServerInfoSelected and SinglePlayerActive.
        m_state.weaponFlags[20 + i] = false;
        m_state.weaponFlags[10 + i] = false;
        m_state.weaponFlags[i] = false;
        registerWideString(formatString("Squad%iString", i + 1), &m_state.squadNames[i]);
        registerWideString(formatString("SquadLeader%iString", i + 1), &m_state.squadLeaderNames[i]);
        registerWideString(formatString("Radio%iString", i + 1), &m_state.radioNames[i]);
        registerWideString(formatString("RadioVehicle%iString", i + 1), &m_state.radioVehicleNames[i]);
        registerWideString(formatString("Commander%iString", i + 1), &m_state.commanderNames[i]);
        registerString(formatString("PlayerWeaponIcon%iPathString", i + 1), &m_state.weaponIcons[i]);
        registerBool(formatString("PlayerWeaponIcon%iSelectShow", i + 1), &m_state.weaponFlags[20 + i]);
        registerBool(formatString("PlayerWeaponIcon%iActiveShow", i + 1), &m_state.weaponFlags[10 + i]);
        registerBool(formatString("PlayerWeaponIcon%iShow", i + 1), &m_state.weaponFlags[i]);
    }
    registerBool("ShowIngameHud", &m_state.ShowIngameHud);
    m_state.OrderMenuPosX = 50;
    m_state.OrderMenuPosY = 50;
    registerInt("OrderMenuPosX", &m_state.OrderMenuPosX);
    registerInt("OrderMenuPosY", &m_state.OrderMenuPosY);
    registerString("CreateSquadActiveIconPath", &m_state.optionIcons[0]);
    registerString("JoinSquadActiveIconPath", &m_state.optionIcons[1]);
    registerString("LeaveSquadActiveIconPath", &m_state.optionIcons[2]);
    registerString("KickMemberActiveIconPath", &m_state.optionIcons[3]);
    registerString("InvitePlayerActiveIconPath", &m_state.optionIcons[4]);
    registerString("MapVoteActiveIconPath", &m_state.optionIcons[6]);
    registerString("RematchVoteActiveIconPath", &m_state.optionIcons[8]);
    registerString("MutinyVoteActiveIconPath", &m_state.optionIcons[9]);
    registerString("KickVoteActiveIconPath", &m_state.optionIcons[7]);
    registerBool("TimeInfoShow", &m_state.TimeInfoShow);
    registerBool("SayAllShow", &m_state.SayAllShow);
    registerBool("SayTeamShow", &m_state.SayTeamShow);
    registerBool("SaySquadShow", &m_state.SaySquadShow);
    registerBool("ChatListShow", &m_state.ChatListShow);
    registerBool("EnemyTicketsShow", &m_state.EnemyTicketsShow);
    registerBool("FriendlyTicketsShow", &m_state.FriendlyTicketsShow);
    registerBool("PlayerScoreShow", &m_state.PlayerScoreShow);
    registerInt("MapZoom", &m_state.MapZoom);
    registerBool("CloseToFlag", &m_state.CloseToFlag);
    registerBool("CloseToNeutralCP", &m_state.CloseToNeutralCP);
    registerBool("CloseToAlliedCP", &m_state.CloseToAlliedCP);
    registerBool("CloseToAxisCP", &m_state.CloseToAxisCP);
    registerBool("CPInterfaceEnabled", &m_state.CPInterfaceEnabled);
    registerFloat("TimePreCPIsTaken", &m_state.TimePreCPIsTaken);
    registerFloat("TimePreCPIsNeutral", &m_state.TimePreCPIsNeutral);
    registerFloat("FriendlyCPs", &m_state.FriendlyCPs);
    registerFloat("EnemyCPs", &m_state.EnemyCPs);
    registerFloat("FriendlyNeutralCPs", &m_state.FriendlyNeutralCPs);
    registerFloat("EnemyNeutralCPs", &m_state.EnemyNeutralCPs);
    registerFloat("EnemyFriendlyCPs", &m_state.EnemyFriendlyCPs);
    registerFloat("FriendlyEnemyCPs", &m_state.FriendlyEnemyCPs);
    registerString("FriendlyCPString", &m_state.FriendlyCPString);
    registerString("EnemyCPString", &m_state.EnemyCPString);
    registerString("NeutralCPString", &m_state.NeutralCPString);
    registerBool("MedicInRadius", &m_state.MedicInRadius);
    registerBool("RepairInRadius", &m_state.RepairInRadius);
    registerBool("AmmoInRadius", &m_state.AmmoInRadius);
    registerBool("CloseToMine", &m_state.CloseToMine);
    registerBool("ShowRankUpIcon", &m_state.ShowRankUpIcon);
    registerString("RankUpIconPathString", &m_state.RankUpIconPathString);
    registerBool("ShowMedalIcon", &m_state.ShowMedalIcon);
    registerString("MedalIconPathString", &m_state.MedalIconPathString);
    registerBool("IngameHelpShow", &m_state.IngameHelpShow);
    registerWideString("IngameHelpString", &m_state.IngameHelpString);
    registerBool("HitDirectionShow", &m_state.HitDirectionShow);
    registerFloat("HitDirectionAngle", &m_state.HitDirectionAngle);
    registerBool("SinglePlayerActive", &m_state.weaponFlags[31]);
    registerBool("VictoryShow", &m_state.VictoryShow);
    registerBool("VictoryRankShow", &m_state.VictoryRankShow);
    registerBool("MapShow", &m_state.MapShow);
    registerBool("MapFullSize", &m_state.MapFullSize);
    registerBool("MapMinSize", &m_state.MapMinSize);
    registerBool("MapFullSizeAndSpawnShow", &m_state.MapFullSizeAndSpawnShow);
    registerBool("MapFullSizeAndNotSpawnShow", &m_state.MapFullSizeAndNotSpawnShow);
    registerBool("MapStatic", &m_state.MapStatic);
    registerBool("MapBorderShow", &m_state.MapBorderShow);
    registerBool("MapBorderAlternateShow", &m_state.MapBorderAlternateShow);
    registerBool("MapMenuShow", &m_state.MapMenuShow);
    registerBool("SuicideButtonShow", &m_state.SuicideButtonShow);
    registerBool("SquadLeaderMenuShow", &m_state.SquadLeaderMenuShow);
    registerBool("CommanderMenuShow", &m_state.CommanderMenuShow);
    registerBool("ChoiceMenuShow", &m_state.ChoiceMenuShow);
    registerBool("CommanderShow", &m_state.CommanderShow);
    registerBool("SpawnShow", &m_state.SpawnShow);
    registerBool("SpawnInfoShow", &m_state.SpawnInfoShow);
    registerBool("SquadInterfaceShow", &m_state.SquadInterfaceShow);
    registerBool("RadioInterfaceShow", &m_state.RadioInterfaceShow);
    registerBool("RadioVehicleInterfaceShow", &m_state.RadioVehicleInterfaceShow);
    registerBool("SquadLeaderInterfaceShow", &m_state.SquadLeaderInterfaceShow);
    registerBool("CommanderInterfaceShow", &m_state.CommanderInterfaceShow);
    registerBool("SpottedInterfaceShow", &m_state.SpottedInterfaceShow);
    registerBool("ScoreboardShow", &m_state.ScoreboardShow);
    registerBool("VoipListShow", &m_state.VoipListShow);
    registerBool("LevelsListShow", &m_state.LevelsListShow);
    registerBool("DemoShow", &m_state.DemoShow);
    registerBool("DemoRecInterfaceShow", &m_state.DemoRecInterfaceShow);
    registerBool("DemoCameraInterfaceShow", &m_state.DemoCameraInterfaceShow);
    registerBool("DemoPlayerNameShow", &m_state.DemoPlayerNameShow);
    registerString("DemoPlayerNameString", &m_state.DemoPlayerNameString);
    registerWideString("DemoPausePlayString", &m_state.DemoPausePlayString);
    registerWideString("DemoCameraString", &m_state.DemoCameraString);
    registerBool("SquadOptionsShow", &m_state.SquadOptionsShow);
    registerBool("JoinOptionShow", &m_state.JoinOptionShow);
    registerBool("LeaveOptionShow", &m_state.LeaveOptionShow);
    registerBool("KickOptionShow", &m_state.KickOptionShow);
    registerBool("InviteOptionShow", &m_state.InviteOptionShow);
    registerBool("MembersShow", &m_state.MembersShow);
    registerBool("KitsShow", &m_state.KitsShow);
    registerBool("PlayerKitIconShow", &m_state.PlayerKitIconShow);
    registerBool("PlayerRankIconShow", &m_state.PlayerRankIconShow);
    registerBool("BarTopLeftShow", &m_state.BarTopLeftShow);
    registerBool("BarTopRightShow", &m_state.BarTopRightShow);
    registerBool("ObjectiveIconShow", &m_state.ObjectiveIconShow);
    registerString("EnemyTicketsString", &m_state.EnemyTicketsString);
    registerString("FriendlyTicketsString", &m_state.FriendlyTicketsString);
    registerString("ScoreString", &m_state.ScoreString);
    registerWideString("TimeToSpawnString", &m_state.TimeToSpawnString);
    registerWideString("SpawnInfoString", &m_state.SpawnInfoString);
    registerWideString("DeathMessage", &m_state.DeathMessage);
    registerWideString("DisconnectMessage", &m_state.DisconnectMessage);
    registerString("LevelTime", &m_state.LevelTime);
    registerWideString("ActiveLevel", &m_state.ActiveLevel);
    registerBool("Team1Selected", &m_state.Team1Selected);
    registerBool("Team2Selected", &m_state.Team2Selected);
    registerInt("PlayerTeam", &m_state.PlayerTeam);
    registerString("FriendlyFlagIconPathString", &m_state.FriendlyFlagIconPathString);
    registerString("EnemyFlagIconPathString", &m_state.EnemyFlagIconPathString);
    registerString("Team1FlagIconPathString", &m_state.Team1FlagIconPathString);
    registerString("Team2FlagIconPathString", &m_state.Team2FlagIconPathString);
    registerString("FriendlyTeamNameString", &m_state.FriendlyTeamNameString);
    registerString("EnemyTeamNameString", &m_state.EnemyTeamNameString);
    registerWideString("Team1NameString", &m_state.Team1NameString);
    registerWideString("Team2NameString", &m_state.Team2NameString);
    registerString("RoundRestartString", &m_state.RoundRestartString);
    registerString("FriendlyTeamWinsString", &m_state.FriendlyTeamWinsString);
    registerString("EnemyTeamWinsString", &m_state.EnemyTeamWinsString);
    registerWideString("WinHeadlineString", &m_state.WinHeadlineString);
    registerFloat("EnemyVictoryAlpha", &m_state.EnemyVictoryAlpha);
    registerFloat("FriendlyVictoryAlpha", &m_state.FriendlyVictoryAlpha);
    registerBool("SetupShow", &m_state.SetupShow);
    registerFloat("MenuBackgroundRed", &m_state.MenuBackgroundRed);
    registerFloat("MenuBackgroundGreen", &m_state.MenuBackgroundGreen);
    registerFloat("MenuBackgroundBlue", &m_state.MenuBackgroundBlue);
    registerFloat("MenuBackgroundAlpha", &m_state.MenuBackgroundAlpha);
    registerFloat("CrossHairColorRed", &m_state.CrossHairColorRed);
    registerFloat("CrossHairColorGreen", &m_state.CrossHairColorGreen);
    registerFloat("CrossHairColorBlue", &m_state.CrossHairColorBlue);
    registerFloat("CrossHairColorAlpha", &m_state.CrossHairColorAlpha);
    registerFloat("MenuMapAlpha", &m_state.MenuMapAlpha);
    registerFloat("MenuMapIconAlpha", &m_state.MenuMapIconAlpha);
    registerFloat("FriendlyTextRed", &m_state.FriendlyTextRed);
    registerFloat("FriendlyTextGreen", &m_state.FriendlyTextGreen);
    registerFloat("FriendlyTextBlue", &m_state.FriendlyTextBlue);
    registerFloat("FriendlyTextAlpha", &m_state.FriendlyTextAlpha);
    registerFloat("EnemyTextRed", &m_state.EnemyTextRed);
    registerFloat("EnemyTextGreen", &m_state.EnemyTextGreen);
    registerFloat("EnemyTextBlue", &m_state.EnemyTextBlue);
    registerFloat("EnemyTextAlpha", &m_state.EnemyTextAlpha);
    registerFloat("FriendlyBrightRed", &m_state.FriendlyBrightRed);
    registerFloat("FriendlyBrightGreen", &m_state.FriendlyBrightGreen);
    registerFloat("FriendlyBrightBlue", &m_state.FriendlyBrightBlue);
    registerFloat("FriendlyBrightAlpha", &m_state.FriendlyBrightAlpha);
    registerFloat("EnemyBrightRed", &m_state.EnemyBrightRed);
    registerFloat("EnemyBrightGreen", &m_state.EnemyBrightGreen);
    registerFloat("EnemyBrightBlue", &m_state.EnemyBrightBlue);
    registerFloat("EnemyBrightAlpha", &m_state.EnemyBrightAlpha);
    registerFloat("FriendlyTeamTopRed", &m_state.FriendlyTeamTopRed);
    registerFloat("FriendlyTeamTopGreen", &m_state.FriendlyTeamTopGreen);
    registerFloat("FriendlyTeamTopBlue", &m_state.FriendlyTeamTopBlue);
    registerFloat("FriendlyTeamTopAlpha", &m_state.FriendlyTeamTopAlpha);
    registerFloat("EnemyTeamTopRed", &m_state.EnemyTeamTopRed);
    registerFloat("EnemyTeamTopGreen", &m_state.EnemyTeamTopGreen);
    registerFloat("EnemyTeamTopBlue", &m_state.EnemyTeamTopBlue);
    registerFloat("EnemyTeamTopAlpha", &m_state.EnemyTeamTopAlpha);
    registerFloat("FriendlyTicketRed", &m_state.FriendlyTicketRed);
    registerFloat("FriendlyTicketGreen", &m_state.FriendlyTicketGreen);
    registerFloat("FriendlyTicketBlue", &m_state.FriendlyTicketBlue);
    registerFloat("FriendlyTicketAlpha", &m_state.FriendlyTicketAlpha);
    registerFloat("EnemyTicketRed", &m_state.EnemyTicketRed);
    registerFloat("EnemyTicketGreen", &m_state.EnemyTicketGreen);
    registerFloat("EnemyTicketBlue", &m_state.EnemyTicketBlue);
    registerFloat("EnemyTicketAlpha", &m_state.EnemyTicketAlpha);
    registerString("TotalNoOfPlayers", &m_state.TotalNoOfPlayers);
    registerString("YesVotes", &m_state.YesVotes);
    registerString("NoVotes", &m_state.NoVotes);
    registerString("ServerNameString", &m_state.ServerNameString);
    registerString("ServerIPString", &m_state.ServerIPString);
    registerString("ServerPortString", &m_state.ServerPortString);
    registerBool("ServerIsFavourite", &m_state.ServerIsFavourite);
    registerString("LocalScore", &m_state.LocalScore);
    registerBool("ShowNightVisionGauge", &m_state.ShowNightVisionGauge);
    registerFloat("FriendlyTicketBleed", &m_state.FriendlyTicketBleed);
    registerFloat("EnemyTicketBleed", &m_state.EnemyTicketBleed);
    registerBool("ServerInfoSelected", &m_state.weaponFlags[30]);
    m_blinkers = new hud::HudBlinkManager;
    hud::HudFloatBlinker* friendly = new hud::HudFloatBlinker(&m_state.field88,
        &m_state.FriendlyTicketBleed, true, 1.0f, true);
    hud::HudFloatBlinker* enemy = new hud::HudFloatBlinker(&m_state.field90,
        &m_state.EnemyTicketBleed, true, 1.0f, true);
    m_blinkers->add(friendly);
    m_blinkers->add(enemy);
    eventManager->registerEventHandler(ECHud, this, 0);
    eventManager->registerEventHandler(ECGame, this, 0);
}

void HudInformationLayer::addRef() { ++m_refCount; }
int HudInformationLayer::getRef() { return m_refCount; }
int HudInformationLayer::release()
{
    const int remaining = --m_refCount;
    if (!remaining) delete this;
    return remaining;
}
void* HudInformationLayer::queryInterface(unsigned int iid) const
{
    return iid == 1 || iid == 120008 ? const_cast<HudInformationLayer*>(this) : 0;
}
template<class T>
int HudInformationLayer::registerValue(Storage<T>& storage, const std::string& name,
                                       T* value, int line)
{
    if (m_names.find(name) != m_names.end())
        Debug(DebugTypeError, "c:\\dice\\projects\\bf2branches\\patch_1_50\\code\\bf2\\game\\HudInformationLayer.h", line)
            << "\tRe registration of already registred identifier " << name;
    // A filtered-out diagnostic continues: allocate another id, leave the old
    // name-to-id insertion intact, and retain the new pointer under its new id.
    const int id = m_nextId++;
    storage.current = storage.values.find(id);
    if (storage.current == storage.values.end())
        storage.values.insert(std::make_pair(id, value));
    else
        storage.current->second = value;
    m_names.insert(std::make_pair(name, id));
    return id;
}
template<class T> T* HudInformationLayer::findValue(Storage<T>& storage, int id)
{
    storage.current = storage.values.find(id);
    if (storage.current == storage.values.end()) return 0;
    // Preserve both source-level finds and the cached iterator writes.
    T* result = 0;
    storage.current = storage.values.find(id);
    if (storage.current != storage.values.end()) result = storage.current->second;
    return result;
}
int HudInformationLayer::registerBool(const std::string& name, bool* value)
{ return registerValue(m_bools, name, value, 107); }
bool* HudInformationLayer::getBoolById(int id)
{ return findValue(m_bools, id); }
bool* HudInformationLayer::getBoolByName(const std::string& name)
{
    const auto it = m_names.find(name);
    return it == m_names.end() ? 0 : getBoolById(it->second);
}

int HudInformationLayer::registerInt(const std::string& name, int* value)
{ return registerValue(m_ints, name, value, 108); }
int* HudInformationLayer::getIntById(int id)
{ return findValue(m_ints, id); }
int* HudInformationLayer::getIntByName(const std::string& name)
{
    const auto it = m_names.find(name);
    return it == m_names.end() ? 0 : getIntById(it->second);
}

int HudInformationLayer::registerFloat(const std::string& name, float* value)
{ return registerValue(m_floats, name, value, 109); }
float* HudInformationLayer::getFloatById(int id)
{ return findValue(m_floats, id); }
float* HudInformationLayer::getFloatByName(const std::string& name)
{
    const auto it = m_names.find(name);
    return it == m_names.end() ? 0 : getFloatById(it->second);
}

int HudInformationLayer::registerString(const std::string& name, const char** value)
{ return registerValue(m_strings, name, value, 110); }
const char** HudInformationLayer::getStringById(int id)
{ return findValue(m_strings, id); }
const char** HudInformationLayer::getStringByName(const std::string& name)
{
    const auto it = m_names.find(name);
    return it == m_names.end() ? 0 : getStringById(it->second);
}

int HudInformationLayer::registerWideString(const std::string& name, const wchar_t** value)
{ return registerValue(m_wideStrings, name, value, 111); }
const wchar_t** HudInformationLayer::getWideStringById(int id)
{ return findValue(m_wideStrings, id); }
const wchar_t** HudInformationLayer::getWideStringByName(const std::string& name)
{
    const auto it = m_names.find(name);
    return it == m_names.end() ? 0 : getWideStringById(it->second);
}

void HudInformationLayer::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    if (category == ECHud) handleHudEvent(id, node, data);
}
// These are recovered false bodies, not success/failure scaffolding. Only their
// source names are unresolved; the PE contains no parameter cleanup at either slot.
bool HudInformationLayer::unknown58() { return false; }
bool HudInformationLayer::unknown5C() { return false; }
void HudInformationLayer::setDeathMessage(std::wstring message)
{ updateWideString(&m_state.DeathMessage, message.c_str()); }
void HudInformationLayer::setDisconnectMessage(std::wstring message)
{ updateWideString(&m_state.DisconnectMessage, message.c_str()); }
bool HudInformationLayer::isWithinRadius(Math::Vec3 a, Math::Vec3 b, float radius)
{
    // Explicit Vec3 value copies at 0x4659B7..0x4659F0 settle argument grouping.
    const double x = static_cast<double>(b.x) - a.x;
    const double y = static_cast<double>(b.y) - a.y;
    const double z = static_cast<double>(b.z) - a.z;
    return std::sqrt(z * z + x * x + y * y) <= radius;
}
bool HudInformationLayer::registerFunction(const std::string& name, Function function)
{
    if (m_functions.find(name) == m_functions.end()) {
        m_functions.insert(std::make_pair(name, function));
        return true;
    }
    Debug(DebugTypeError, "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\HudInformationLayer.cpp", 1654)
        << "Re registration of already registred identifier " << name;
    return false;
}
HudInformationLayer::Function HudInformationLayer::getFunction(const std::string& name)
{
    const auto it = m_functions.find(name);
    return it == m_functions.end() ? 0 : it->second;
}
bool HudInformationLayer::registerStringFunction(const std::string& name, StringFunction function)
{
    if (m_stringFunctions.find(name) == m_stringFunctions.end()) {
        m_stringFunctions.insert(std::make_pair(name, function));
        return true;
    }
    Debug(DebugTypeError, "C:\\dice\\Projects\\BF2Branches\\Patch_1_50\\Code\\BF2\\Game\\HudInformationLayer.cpp", 1684)
        << "Re registration of already registred identifier " << name;
    return false;
}
HudInformationLayer::StringFunction HudInformationLayer::getStringFunction(const std::string& name)
{
    const auto it = m_stringFunctions.find(name);
    return it == m_stringFunctions.end() ? 0 : it->second;
}
void HudInformationLayer::setServerInfo(std::string name, std::string address, int port)
{
    // ..0x463C56 loads the settings port before the first string update.
    // The passed port is never read. Native member access replaces PE +0x98.
    (void)port;
    const int currentPort = serverSettings->m_serverPort;
    updateString(&m_state.ServerNameString, name.c_str());
    updateString(&m_state.ServerIPString, address.c_str());
    const std::string text = formatString("%d", currentPort);
    updateString(&m_state.ServerPortString, text.c_str());
    m_state.field588 = currentPort;
}
void HudInformationLayer::updateString(const char** destination, const char* value)
{
    // The missing destination/*destination guards and comparison-before-release
    // order are retail. An equal value keeps its existing allocation.
    if (value && std::strcmp(value, *destination)) {
        if (*destination && *destination != emptyString) {
            ::operator delete(const_cast<char*>(*destination));
            *destination = 0;
        }
        const std::size_t size = std::strlen(value) + 1;
        char* copy = static_cast<char*>(::operator new(size));
        std::memcpy(copy, value, size);
        *destination = copy;
    }
}
void HudInformationLayer::updateWideString(const wchar_t** destination, const wchar_t* value)
{
    if (value && std::wcscmp(value, *destination)) {
        if (*destination && *destination != emptyWideString) {
            ::operator delete(const_cast<wchar_t*>(*destination));
            *destination = 0;
        }
        const std::size_t size = (std::wcslen(value) + 1) * sizeof(wchar_t);
        wchar_t* copy = static_cast<wchar_t*>(::operator new(size));
        std::memcpy(copy, value, size);
        *destination = copy;
    }
}
HudInformationLayer::~HudInformationLayer()
{
    delete m_blinkers;
    eventManager->unregisterEventHandler(ECHud, this);
    eventManager->unregisterEventHandler(ECGame, this);
    if (m_state.LocalScore != emptyString) delete[] m_state.LocalScore;
    if (m_state.EnemyTicketsString != emptyString) delete[] m_state.EnemyTicketsString;
    if (m_state.FriendlyTicketsString != emptyString) delete[] m_state.FriendlyTicketsString;
    if (m_state.ScoreString != emptyString) delete[] m_state.ScoreString;
    if (m_state.TimeToSpawnString != emptyWideString) delete[] m_state.TimeToSpawnString;
    if (m_state.SpawnInfoString != emptyWideString) delete[] m_state.SpawnInfoString;
    if (m_state.LevelTime != emptyString) delete[] m_state.LevelTime;
    if (m_state.FriendlyFlagIconPathString != emptyString) delete[] m_state.FriendlyFlagIconPathString;
    if (m_state.EnemyFlagIconPathString != emptyString) delete[] m_state.EnemyFlagIconPathString;
    if (m_state.FriendlyTeamNameString != emptyString) delete[] m_state.FriendlyTeamNameString;
    if (m_state.EnemyTeamNameString != emptyString) delete[] m_state.EnemyTeamNameString;
    if (m_state.Team1FlagIconPathString != emptyString) delete[] m_state.Team1FlagIconPathString;
    if (m_state.Team2FlagIconPathString != emptyString) delete[] m_state.Team2FlagIconPathString;
    if (m_state.Team1NameString != emptyWideString) delete[] m_state.Team1NameString;
    if (m_state.Team2NameString != emptyWideString) delete[] m_state.Team2NameString;
    if (m_state.FriendlyTeamWinsString != emptyString) delete[] m_state.FriendlyTeamWinsString;
    if (m_state.EnemyTeamWinsString != emptyString) delete[] m_state.EnemyTeamWinsString;
    if (m_state.DeathMessage != emptyWideString) delete[] m_state.DeathMessage;
    if (m_state.DisconnectMessage != emptyWideString) delete[] m_state.DisconnectMessage;
    for (int i = 0; i < 7; ++i) {
        if (m_state.kitWeapon[i] != emptyString) delete[] m_state.kitWeapon[i];
        if (m_state.kitAltWeapon[i] != emptyString) delete[] m_state.kitAltWeapon[i];
        if (m_state.kitNames[i] != emptyWideString) delete[] m_state.kitNames[i];
        if (m_state.kitIcons[i] != emptyString) delete[] m_state.kitIcons[i];
        for (int j = 0; j < 5; ++j)
            if (m_state.kitAbilityIcons[i * 5 + j] != emptyString)
                delete[] m_state.kitAbilityIcons[i * 5 + j];
    }
    for (int i = 0; i < 12; ++i) {
        if (m_state.weaponIcons[i] != emptyString) delete[] m_state.weaponIcons[i];
        if (m_state.radioNames[i] != emptyWideString) delete[] m_state.radioNames[i];
        if (m_state.radioVehicleNames[i] != emptyWideString) delete[] m_state.radioVehicleNames[i];
        if (m_state.squadLeaderNames[i] != emptyWideString) delete[] m_state.squadLeaderNames[i];
        if (m_state.commanderNames[i] != emptyWideString) delete[] m_state.commanderNames[i];
        if (m_state.squadNames[i] != emptyWideString) delete[] m_state.squadNames[i];
    }
    for (int i = 0; i < 10; ++i)
        if (m_state.optionIcons[i] != emptyString) delete[] m_state.optionIcons[i];
    if (m_state.IngameHelpString != emptyWideString) delete[] m_state.IngameHelpString;
    if (m_state.RankUpIconPathString != emptyString) delete[] m_state.RankUpIconPathString;
    if (m_state.MedalIconPathString != emptyString) delete[] m_state.MedalIconPathString;
    if (m_state.WinHeadlineString != emptyWideString) delete[] m_state.WinHeadlineString;
    if (m_state.ActiveLevel != emptyWideString) delete[] m_state.ActiveLevel;
    if (m_state.FriendlyCPString != emptyString) delete[] m_state.FriendlyCPString;
    if (m_state.EnemyCPString != emptyString) delete[] m_state.EnemyCPString;
    if (m_state.NeutralCPString != emptyString) delete[] m_state.NeutralCPString;
    if (m_state.TotalNoOfPlayers != emptyString) delete[] m_state.TotalNoOfPlayers;
    if (m_state.YesVotes != emptyString) delete[] m_state.YesVotes;
    if (m_state.NoVotes != emptyString) delete[] m_state.NoVotes;
    if (m_state.ServerNameString != emptyString) delete[] m_state.ServerNameString;
    if (m_state.ServerIPString != emptyString) delete[] m_state.ServerIPString;
    if (m_state.DemoPlayerNameString != emptyString) delete[] m_state.DemoPlayerNameString;
    if (m_state.DemoPausePlayString != emptyWideString) delete[] m_state.DemoPausePlayString;
    if (m_state.DemoCameraString != emptyWideString) delete[] m_state.DemoCameraString;
    // PE omits +0x53C RoundRestartString and +0x584 ServerPortString.
    // Preserve those omissions. WeakPtr and native maps now destroy in member order.
}
} }
#endif
