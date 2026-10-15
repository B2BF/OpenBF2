// HUD squad controls.
#include "HudSquad.h"
#if !DEDICATED
#include "BF2HudManager.h"
#include "BfListBoxData.h"
#include "HudKitSelection.h"
#include "HudColors.h"                              // Row palettes.
#include "../Game/Common/ClientVoteManager.h"       // voteIssueAllowed - the commander heading's mutiny
#include "../External/BFMeme/System.h"
#include "../Core/EventManager.h"
#include "../Core/StringUtil.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Objects/IPlayer.h"
#include "../Game/IGameLogic.h"                 // getTeamName (+0x48)
#include "../Game/Objects/Game/Kit.h"           // getPlayerKit's return type
#include "../Game/Objects/IVehicleHudComp.h"    // getHudInfo (+0x64)
#include "../Game/Objects/VehicleHudCompTemplate.h" // VehicleHudInfo::hudName (+0x20)
#include <string>
#include "../Game/GameLogic/SquadManager/ISquadManager.h"
#include "../Game/GameLogic/SpawnSystem/SpawnManager.h"
#include "../Game/GameLogic/SpawnSystem/SpawnGroup.h"
#include "../Scene/Object/PlayerManager.h"
#include "../Sound/BfSound.h"
#include <algorithm>
#include <cstdio>     // [FRONTIER] diagnostics.
#include <cstdlib>
#include <cstring>
#include <new>
namespace dice { namespace hfe {
extern IBase* squadManager;
extern IBase* spawnManager;
namespace hud {
namespace {
// PE8673E0 constructs this TU's 27 strings at A1E380; destructor87C0C0.
// BF2HudManager has a separate identically initialized array at A212A8.
const std::string defaultSquadNames[27] = {
    "Unassigned", "Alpha", "Bravo", "Charlie", "Delta", "Echo", "Foxtrot",
    "Gamma", "Hotel", "India", "Juliet", "Kilo", "Lima", "Mike", "November",
    "Oscar", "Papa", "Quebec", "Romeo", "Sierra", "Tango", "Uniform", "Victor",
    "Whiskey", "Xray", "Yankee", "Zulu"
};
// Event59's caller-owned payload:791AAA/791AAF read byte0 and signed dword4.
struct SquadEvent59 { bool enabled; int playerId; };

// Colors used by squad-row builders. Preserve literal precision.
const meme::Color kColor97DE20 = {0.7490196228027344f, 0.7490196228027344f,
                                  0.5607843399047852f, 1.0f};   // tan: default row text
const meme::Color kColor97DE30 = {1.0f, 0.7919999957084656f,
                                  0.0430000014603138f, 1.0f};   // gold: the button captions
const meme::Color kColor97DD50 = {1.0f, 1.0f, 1.0f, 1.0f};      // white: bars, arrows, checkboxes
const meme::Color kColor97DE10 = {0.4862745106220245f, 0.4784313738346100f,
                                  0.3882353007793427f, 1.0f};   // the band of every heading/squad row
const meme::Color kColor97E2B0 = {0.5759999752044678f, 0.5730000138282776f,
                                  0.45100000500679016f, 1.0f};  // olive: frames, public lock, greyed
const meme::Color kColor97E2A0 = {0.3843137323856354f, 0.3607843220233917f,
                                  0.2549019753932953f, 1.0f};   // the vehicle and kit icons
const meme::Color kColor97DE40 = {1.0f, 1.0f, 0.0f, 1.0f};      // yellow: a talking speaker
const meme::Color kColor97DE00 = {0.7254902124404907f, 0.7098039388656616f,
                                  0.5803921818733215f, 1.0f};   // every other player/invite row
// ds:932030 / ds:932024: the squad row's name and member count, each closed by "\xA7" "0".
const char* const kSquadNameFormat = "\xA7" "1%s" "\xA7" "0";
const char* const kSquadCountFormat = "\xA7" "1%i/%i" "\xA7" "0";
// ds:93207C, byte for byte A7 31 25 73 00: the player/invite name without the closing "\xA7" "0".
const char* const kPlayerNameFormat = "\xA7" "1%s";
// ds:92FF88, the pooled literal HudVoip's painter pushes too.
const char* const kSquadIconFormat = "Ingame/Player/Icons/Minimap/mini_Squad_%d_small.tga";
}
char HudSquad::s_emptyString;
wchar_t HudSquad::s_emptyWideString;
void HudSquad::joinSquad(int squad)
{
    auto* player = world::playerManager->getLocalHumanPlayer();
    if (!player || player->getIsCommander()) return;
    if (!player->getSquadId()) {
        if (squad <= -1) return;
        const int team = player->getTeam();
        if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 0 || squad == 0) {
            hudManager->disableVOHelpMessage(12);
            const int playerId = player->getId();
            const int repeatedTeam = player->getTeam();
            static_cast<ISquadManager*>(squadManager)->addToSquad(repeatedTeam, squad, playerId);
        }
        return;
    }
    ISquadInfo* current = static_cast<ISquadManager*>(squadManager)->getSquadInfo(m_team, m_squadId);
    ISquadInfo* target = static_cast<ISquadManager*>(squadManager)->getSquadInfo(m_team, squad);
    if (!current || !target) return;
    const char* targetName = target->getSquadName().c_str();
    const char* currentName = current->getSquadName().c_str();
    const std::string copiedTarget(targetName);
    const std::string copiedCurrent(currentName);
    std::wstring message = hudManager->getLocalizedString("HUD_HUD_SQUAD_MUST_LEAVE_TO_JOIN");
    message = hudManager->replaceWideTokenFromString(message, "#SQUADNAME1#", copiedCurrent);
    message = hudManager->replaceWideTokenFromString(message, "#SQUADNAME2#", copiedTarget);
    hudManager->addServerChat(message, 0);
}
void HudSquad::finishSquadCreate()
{
    auto* player = world::playerManager->getLocalHumanPlayer();
    if (!m_createVisible && player) {
        const int team = player->getTeam();
        const int index = hudManager->findEmptySquad(team);
        const int repeatedTeam = player->getTeam();
        const std::string name = getAvailableSquadName(repeatedTeam, index);
        hudManager->setSquadEditText(name);
    }
    hudManager->getSquadEditText(); // The returned string is destroyed, then submission follows.
    hudManager->submitSquadCreate();
    setCreateMode(false);
}
int HudSquad::toggleSelectedSquad(int squad)
{
    m_field08c = squad == m_field08c ? -1 : squad;
    m_expanded.reset();
    if (m_field08c > -1) m_expanded.set(std::max(0, std::min(9, m_field08c)));
    return m_field08c;
}
void HudSquad::selectSquad(int squad)
{
    m_field08c = squad;
    if (squad > 0 && squad < 10
        && world::playerManager->getNrOfPlayersInSquad(m_team, squad) > 0) {
        selectPlayer(-1);
        return;
    }
    m_field08c = -1;
}
void HudSquad::togglePlayerSelection(int playerId)
{
    m_field090 = playerId == m_field090 ? -1 : playerId;
}
void HudSquad::toggleSquadSelection(int squad)
{
    m_field08c = squad == m_field08c ? -1 : squad;
}
void HudSquad::setSquadExpanded(int squad, bool expanded)
{
    m_expanded.set(std::max(0, std::min(9, squad)), expanded);
}
void HudSquad::selectPlayer(int playerId)
{
    m_field090 = playerId;
    auto* player = world::playerManager->getPlayerFromId(playerId);
    if (player) {
        m_field08c = -1;
        const int squad = player->getSquadId();
        m_expanded.set(std::max(0, std::min(9, squad)));
    } else m_field090 = -1;
}
HudSquad::HudSquad(meme::System* system)
    : m_system(system), m_title(&s_emptyString), m_unassignedText(&s_emptyWideString),
      m_squadCountText(&s_emptyWideString), m_rowCount(0), m_localPlayerRow(0),
      m_playerId(-1), m_visibleRows(21), m_sortByScore(false), m_squadId(0), m_team(2),
      m_isSquadLeader(false), m_isCommander(false), m_showCommander(true),
      m_field09f(true), m_visible(false), m_inviteVisible(false), m_isInSquad(false),
      m_field0a8(0), m_field0ac(19), m_field0b0(2), m_field0b4(0),
      m_createVisible(false), m_renameVisible(false), m_createLocked(false),
      m_createTag(&s_emptyString), m_newName(&s_emptyString)
{
    eventManager->registerEventHandler(ECHud, this, 0);
    m_columns[0] = 5; m_columns[1] = 5;
    m_columns[9] = 216; m_columns[8] = 216;
    m_columns[2] = 22; m_columns[3] = 45; m_columns[4] = 85;
    m_columns[6] = 116; m_columns[7] = 133; m_columns[5] = 196;
    m_columns[10] = 7; m_columns[11] = 181;
    for (int i = 0; i < 10; ++i) m_expanded.reset(i);
    m_selectedPlayers.clear();
    m_excludedPlayers.clear();
    m_invitePlayers.clear();
    for (std::intptr_t& icon : m_squadIcons) icon = 0;
    if (m_system) {
        m_textures[12] = m_system->loadTexture("Ingame/Kits/Icons/kit_Commander.tga", true);
        m_textures[13] = m_system->loadTexture("Ingame/Kits/Icons/kit_Squad.tga", true);
        m_textures[0] = m_system->loadTexture("Ingame/Respawn/button_upp.tga", true);
        m_textures[1] = m_system->loadTexture("Ingame/Respawn/button_down.tga", true);
        m_textures[2] = m_system->loadTexture("Ingame/GeneralIcons/empty.tga", true);
        m_textures[3] = m_system->loadTexture("Ingame/Scoreboard/Icons/checkbox_empty.tga", true);
        m_textures[4] = m_system->loadTexture("Ingame/Scoreboard/Icons/checkbox_cross.tga", true);
        m_textures[5] = m_system->loadTexture("Ingame/Scoreboard/Icons/checkbox_ask.tga", true);
        m_textures[6] = m_system->loadTexture("Ingame/Respawn/button_longlight7.tga", true);
        m_textures[7] = m_system->loadTexture("Ingame/Respawn/iconframe.tga", true);
        m_textures[8] = m_system->loadTexture("Ingame/Respawn/lockicon.tga", true);
        m_textures[9] = m_system->loadTexture("Ingame/Scoreboard/Icons/checkbox_voipMuteGrey.tga", true);
        m_textures[10] = m_system->loadTexture("Ingame/Respawn/icon_speaker.tga", true);
        m_textures[11] = m_system->loadTexture("Ingame/Respawn/icon_speakerMute.tga", true);
    } else {
        m_textures[13] = 0; m_textures[12] = 0;
        for (int i = 0; i < 12; ++i) m_textures[i] = 0;
    }
}
HudSquad::~HudSquad()
{
    eventManager->unregisterEventHandler(ECHud, this);
    if (m_title != &s_emptyString) delete[] m_title;
    if (m_unassignedText != &s_emptyWideString) delete[] m_unassignedText;
    if (m_squadCountText != &s_emptyWideString) delete[] m_squadCountText;
    if (m_newName != &s_emptyString) delete[] m_newName;
    if (m_createTag != &s_emptyString) delete[] m_createTag;
    // Lists destruct in reverse order; textures and list-data owners are not released.
}
void HudSquad::setSystem(meme::System* system) { m_system = system; }
bool HudSquad::registerFunctions() { return true; } // MOV AL,1; RET
bool HudSquad::registerData()
{
    void* allocation = ::operator new(sizeof(meme::BfListBoxData));
    m_memberList = allocation ? new(allocation) meme::BfListBoxData("SquadMemberList") : nullptr;
    if (!m_memberList) return false;
    allocation = ::operator new(sizeof(meme::BfListBoxData));
    m_inviteList = allocation ? new(allocation) meme::BfListBoxData("SquadInviteList") : nullptr;
    if (!m_inviteList) return false;
    m_memberList->addColumn(m_columns[1], false, false);
    m_memberList->addColumn(m_columns[2], true, false);
    m_memberList->addColumn(m_columns[2], true, false);
    m_memberList->addIconColumn(m_columns[3], 166, false);
    m_memberList->addIconColumn(m_columns[3], 69, false);
    m_memberList->addIconColumn(m_columns[4], 30, false);
    m_memberList->addColumn(m_columns[6], true, false);
    m_memberList->addColumn(m_columns[6], true, false);
    m_memberList->addColumn(m_columns[7], true, false);
    m_memberList->addIconColumn(m_columns[7], 80, false);
    m_memberList->addColumn(m_columns[9], true, false);
    m_memberList->addColumn(m_columns[9], true, false);
    m_memberList->addHiddenColumn();
    m_memberList->addHiddenColumn();
    m_memberList->m_visibleRows = m_visibleRows;
    m_memberList->m_field021 = true;
    m_memberList->m_field023 = false;
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerString("TeamTopLabelString", &m_title);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerWideString("NrOfUnassigned", &m_unassignedText);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerWideString("NrOfSquads", &m_squadCountText);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SquadMemberListShow", &m_visible);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SquadRenameShow", &m_renameVisible);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SquadCreateShow", &m_createVisible);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("SquadCreateLockedShow", &m_createLocked);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerString("SquadCreateTagString", &m_createTag);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerString("NewSquadName", &m_newName);
    m_inviteList->addColumn(m_columns[11], false, false);
    m_inviteList->addHiddenColumn();
    m_inviteList->m_visibleRows = m_visibleRows;
    m_inviteList->m_field021 = true;
    m_inviteList->m_field023 = false;
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("InviteListShow", &m_inviteVisible);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("IsInSquad", &m_isInSquad);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("IsSquadLeader", &m_isSquadLeader);
    return true;
}
void HudSquad::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    if (category == ECHud) handleHudEvent(id, node, data);
}
void HudSquad::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (id != 59) return;
    const SquadEvent59* event = static_cast<const SquadEvent59*>(data);
    const bool enabled = event->enabled;
    const int playerId = event->playerId;
    if (player->getId() == playerId) return; // No local-player null guard in retail.
    if (enabled) m_eventPlayers.push_back(playerId);
    else if (!m_eventPlayers.empty()) {
        const auto found = std::find(m_eventPlayers.begin(), m_eventPlayers.end(), playerId);
        if (found != m_eventPlayers.end()) m_eventPlayers.erase(found); // First duplicate only.
    }
}
void HudSquad::setVisible(bool visible)
{
    if (visible) {
        world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
        if (player) {
            m_squadId = player->getSquadId();
            m_isSquadLeader = player->getIsSquadLeader();
            m_isCommander = player->getIsCommander();
            m_team = player->getTeam();
            m_playerId = player->getId();
            m_isInSquad = m_squadId > 0;
            if (visible != m_visible) {
                const int squad = m_squadId < 0 ? 0 : (m_squadId > 9 ? 9 : m_squadId);
                m_expanded.set(squad);
                scrollToSquad();
            }
            refreshTitle();
            refreshCounts(m_team);
            refreshRows(m_team);
            refreshCreateTag(false);
        }
    }
    m_visible = visible;
}
// Updates the squad menu title for the current team and selection.
void HudSquad::refreshTitle()
{
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
    if (!local)
        return;

    std::wstring title = hudManager->getLocalizedString(
        std::string("HUD_TEXT_MENU_SPAWN_SQUAD_SOLDIER"));          // +0x38

    if (local->getIsCommander())                                     // +0x114
    {
        title = hudManager->getLocalizedString(
            std::string("HUD_TEXT_MENU_SPAWN_SQUAD_COMMANDER"));
    }
    else if (local->getIsSquadLeader())                              // +0x13C
    {
        title = hudManager->getLocalizedString(
            std::string("HUD_TEXT_MENU_SPAWN_SQUAD_SQUADLEADER"));
    }
    else if (local->getDefaultVehicle())                             // +0xA4
    {
        world::Kit* const kit = hudManager->getPlayerKit(local);     // +0x90
        if (kit && kit->getVehicleHudComponent())                    // +0x54
        {
            // no null check on the info block - retail dereferences it straight away.
            world::VehicleHudInfo* const info =
                static_cast<world::IVehicleHudComp*>(kit->getVehicleHudComponent())
                    ->getHudInfo();                                  // +0x64
            title = hudManager->getLocalizedString(info->hudName);   // +0x20
        }
    }

    // The existing format passes a wide title to narrow %s; preserve the behavior until an intentional fix.
    const std::string text = formatString("%s - %s",
                                          gameLogic->getTeamName(local->getTeam()).c_str(),
                                          title.c_str());
    updateString(&m_title, text.c_str());
}

// Squad-screen row creation and refresh.

// 78E2C0 (102 B). Scroll the member list so the local player's row is in view: when that row is
// past the visible window, the first visible row becomes (local row - visible rows) plus the number
// of players in the local squad, so the rows after it show too. A player with no squad (0), or a
// row already in view, scrolls to the top.
// m_localPlayerRow is whatever paintPlayerRow last stored - stale when the local row was not
// painted (see its declaration). As shipped.
void HudSquad::scrollToSquad()
{
    if (m_localPlayerRow <= m_visibleRows) {                                   // +7C / +84
        m_memberList->setFirstVisibleRow(0);
        return;
    }
    const int overflow = m_localPlayerRow - m_visibleRows;
    if (!m_squadId) {                                                          // +94
        m_memberList->setFirstVisibleRow(0);
        return;
    }
    m_memberList->setFirstVisibleRow(
        overflow + world::playerManager->getNrOfPlayersInSquad(m_team, m_squadId));  // PE +0x9C
}

// [CHOSEN, not recovered] - see the declaration. Byte for byte the inlined sequence in 78E330.
void HudSquad::updateWideString(const wchar_t** destination, const wchar_t* source)
{
    if (source && std::wcscmp(source, *destination)) {
        if (*destination && *destination != &s_emptyWideString) {
            ::operator delete(const_cast<wchar_t*>(*destination));
            *destination = nullptr;
        }
        const std::size_t count = std::wcslen(source) + 1;
        wchar_t* const clone = static_cast<wchar_t*>(::operator new(count * sizeof(wchar_t)));
        std::memcpy(clone, source, count * sizeof(wchar_t));
        *destination = clone;
    }
}

// 78E330 (496 B). The two count lines of the squad screen, localised with their tokens.
// "#TOTAL#" in the squads line is the constant 9 - the nine squad slots, not a count.
// The squads line goes through an extra wstring built from c_str() (78E4AF), which would drop
// anything past an embedded NUL; the unassigned line does not. Both kept.
void HudSquad::refreshCounts(int team)
{
    const int total = world::playerManager->getNrOfPlayerInTeam(team);         // PE +0x94
    const std::wstring unassigned = hudManager->replaceWideTokenFromInt(       // +0x44
        hudManager->replaceWideTokenFromInt(                                   // +0x44
            hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_SQUAD_NUMBER_OF_UNASSIGNED"),  // +0x38
            "#UNASSIGNED#", total - world::playerManager->getNrOfPlayerInSquads(team)),     // +0x98
        "#TOTAL#", total);
    updateWideString(&m_unassignedText, unassigned.c_str());                  // +0C
    const std::wstring squads(hudManager->replaceWideTokenFromInt(
        hudManager->replaceWideTokenFromInt(
            hudManager->getLocalizedString("HUD_TEXT_MENU_SPAWN_SQUAD_NUMBER_OF_SQUADS"),
            "#ACTIVE#", world::playerManager->getNrOfActiveSquads(team)),     // +0xA4
        "#TOTAL#", 9).c_str());
    updateWideString(&m_squadCountText, squads.c_str());                      // +10
}

// Refreshes the squad list rows.
void HudSquad::refreshRows(int team)
{
    m_memberList->clearRows();
    m_rowCount = 0;
    if (m_showCommander)                                                        // +9E
        fillCommanderRows(team);
    if (m_squadId <= 0 && !m_isCommander)                                       // +94 / +9D
        paintCreateRow(team);
    for (int squad = 1; squad < 10; ++squad)
        fillSquadRows(m_sortByScore                                             // +88
                ? world::playerManager->getPlayersInSquadSortedByScore(team, squad)   // PE +0x58
                : world::playerManager->getPlayersInSquadSortedByRank(team, squad),   // PE +0x5C
            team, squad);
    if (!m_inviteVisible)                                                       // +A1
        return;
    m_inviteList->clearRows();
    m_rowCount = 0;
    rebuildInvitePlayers(m_selectedPlayers);
    // The by-value keys are built right to left: column 0 is the label, column 1 the action.
    paintHeadingRow(std::string("HUD_SQUADTAB_SQUAD"), std::string("HUD_SQUADTAB_KICK"));
    if (m_sortByScore) {
        fillInviteRows(world::playerManager->getPlayersInSquadSortedByScore(team, m_squadId),
                       team, -1);
        fillInviteRows(m_invitePlayers, team, -1);                              // +F4
        paintHeadingRow(std::string("HUD_SQUADTAB_TEAM"), std::string("HUD_SQUADTAB_INVITE"));
        fillInviteRows(world::playerManager->getPlayersSortedByScore(team), team, m_squadId);  // PE +0x44
    } else {
        fillInviteRows(world::playerManager->getPlayersInSquadSortedByRank(team, m_squadId),
                       team, -1);
        fillInviteRows(m_invitePlayers, team, -1);
        paintHeadingRow(std::string("HUD_SQUADTAB_TEAM"), std::string("HUD_SQUADTAB_INVITE"));
        fillInviteRows(world::playerManager->getPlayersSortedByRank(team), team, m_squadId);   // PE +0x4C
    }
}

// 78E520 (369 B). A heading row on the invite list: two localised captions and a "-1" marker in
// the hidden column, then the band. It shares the row counter m_rowCount with the member list;
// refreshRows resets it per list.
void HudSquad::paintHeadingRow(std::string labelKey, std::string actionKey)
{
    m_inviteList->appendPositionedWideCell0x7c8470(0, hudManager->getLocalizedString(labelKey),
        static_cast<float>(m_columns[10]), m_field0b4,                         // fild +E0, raw +B4
        kColor97DE20.r, kColor97DE20.g, kColor97DE20.b, kColor97DE20.a, 0);
    m_inviteList->appendPositionedWideCell0x7c8470(1, hudManager->getLocalizedString(actionKey),
        0.0f, m_field0b4, kColor97DE20.r, kColor97DE20.g, kColor97DE20.b, kColor97DE20.a, 2);
    m_inviteList->appendCell(2, std::string("-1"), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_inviteList->setRowHighlight0x7c4ba0(m_rowCount,
        kColor97DE10.r, kColor97DE10.g, kColor97DE10.b, kColor97DE10.a,
        0.0f, m_field0a8, m_field0ac);
    ++m_rowCount;                                                               // +78
}

// 78E6A0 (1,534 B). The commander band - row type 0. Its button caption and the click handler
// 791CE0 run the same decision, so they agree: resign for the commander, apply while the team has
// none, mutiny while a vote may be issued, and no button at all otherwise.
// getCommander(team) is asked up to three times, never cached. The apply lookup is always made
// first and then copy-assigned over. The "no button" test is a real operator== against L"". The
// team is the argument here, where 791CE0 uses m_team (refreshRows passes m_team, so they agree).
// Nothing is null-tested, ClientVoteManager::getInstance()'s answer included.
void HudSquad::paintCommanderHeading(int team)
{
    meme::Color color = kColor97DE20;
    std::wstring text = hudManager->getLocalizedString(
        std::string("HUD_TEXT_MENU_SPAWN_SQUAD_APPLY"));
    meme::TextureHandle arrow = m_textures[0];                                  // +14 button_upp
    if (!m_field09f)                                                            // +9F, expanded
        arrow = m_textures[1];                                                  // +18 button_down
    if (m_isCommander) {                                                        // +9D
        text = hudManager->getLocalizedString(std::string("HUD_TEXT_COMMANDER_RESIGN"));
        color = squadChatColors[2];
    } else if (static_cast<ISquadManager*>(squadManager)->getCommander(team) >= 0) {
        if (ClientVoteManager::getInstance()->voteIssueAllowed())
            text = hudManager->getLocalizedString(
                std::string("HUD_TEXT_MENU_SPAWN_SQUAD_MUTINITY"));             // the retail spelling
        else
            text = L"";                                                         // pooled L"" 8A3024
    } else {
        arrow = m_textures[2];                                                  // +1C empty: no commander
    }
    if (m_field090 >= 0                                                         // +90, the selection
            && static_cast<ISquadManager*>(squadManager)->getCommander(team) == m_field090)
        color = mapWhiteColors[2];                                              // A1CEB8, wins
    m_memberList->appendImageCell0x7c8300(0, m_textures[7], 15.0f, 15.0f,     // +30 iconframe
        static_cast<float>(m_columns[0]), m_field0b0, color.r, color.g, color.b, color.a);
    m_memberList->appendImageCell0x7c8300(1, m_textures[12], 15.0f, 15.0f,    // +44 kit_Commander
        0.0f, m_field0b0, color.r, color.g, color.b, color.a);
    m_memberList->appendCell(2, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(3, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(4, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendPositionedWideCell0x7c8470(5,
        hudManager->getLocalizedString(std::string("HUD_HUD_MENU_COMMANDER_HEADING")),
        -21.0f, m_field0b4, color.r, color.g, color.b, color.a, 0);
    m_memberList->appendCell(6, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(7, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(8, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    if (text == L"") {
        m_memberList->appendCell(9, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_memberList->appendCell(10, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    } else {
        m_memberList->appendImageCell0x7c8300(9, m_textures[6], 80.0f, 17.0f, // +2C button bar
            0.0f, m_field0b0 - 1.0f,
            kColor97DD50.r, kColor97DD50.g, kColor97DD50.b, kColor97DD50.a);
        m_memberList->appendPositionedWideCell0x7c8470(10, text, 0.0f, m_field0b4,
            kColor97DE30.r, kColor97DE30.g, kColor97DE30.b, kColor97DE30.a, 1);
    }
    m_memberList->appendCell(11, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendImageCell0x7c8300(12, arrow, 17.0f, 17.0f, 0.0f, m_field0b0 - 1.0f,
        kColor97DD50.r, kColor97DD50.g, kColor97DD50.b, kColor97DD50.a);
    m_memberList->appendCell(13,                                                // the commander's id
        formatString("%i", static_cast<ISquadManager*>(squadManager)->getCommander(team)),
        1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(14, formatString("%i", 0), 1.0f, 1.0f, 1.0f, 1.0f, 0);    // row type 0
    m_memberList->setRowHighlight0x7c4ba0(m_rowCount,
        kColor97DE10.r, kColor97DE10.g, kColor97DE10.b, kColor97DE10.a,
        0.0f, m_field0a8, m_field0ac);
    ++m_rowCount;
}

// 78ECA0 (1,079 B). The create-squad row - row type 3; 791CE0 turns a click on its column 10 into
// hudManager +0x28C (submitSquadCreate). The team argument is never read (`retn 4` only pops it).
void HudSquad::paintCreateRow(int)
{
    const std::wstring text = hudManager->getLocalizedString(std::string("HUD_SQUADTAB_CREATE"));
    m_memberList->appendImageCell0x7c8300(0, m_textures[7], 15.0f, 15.0f,     // +30 iconframe
        static_cast<float>(m_columns[0]), m_field0b0,
        kColor97DE20.r, kColor97DE20.g, kColor97DE20.b, kColor97DE20.a);
    m_memberList->appendImageCell0x7c8300(1, m_textures[13], 15.0f, 15.0f,    // +48 kit_Squad
        0.0f, m_field0b0, kColor97DE20.r, kColor97DE20.g, kColor97DE20.b, kColor97DE20.a);
    m_memberList->appendCell(2, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(3, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(4, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendPositionedWideCell0x7c8470(5,
        hudManager->getLocalizedString(std::string("HUD_SQUADTAB_SQUAD")),
        -21.0f, m_field0b4, kColor97DE20.r, kColor97DE20.g, kColor97DE20.b, kColor97DE20.a, 0);
    m_memberList->appendCell(6, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(7, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(8, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendImageCell0x7c8300(9, m_textures[6], 80.0f, 17.0f,     // +2C button bar
        0.0f, m_field0b0 - 1.0f,
        kColor97DD50.r, kColor97DD50.g, kColor97DD50.b, kColor97DD50.a);
    m_memberList->appendPositionedWideCell0x7c8470(10, text, 0.0f, m_field0b4,
        kColor97DE30.r, kColor97DE30.g, kColor97DE30.b, kColor97DE30.a, 1);
    m_memberList->appendCell(11, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(12, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);  // no arrow
    m_memberList->appendCell(13, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);  // no id
    m_memberList->appendCell(14, formatString("%i", 3), 1.0f, 1.0f, 1.0f, 1.0f, 0);   // row type 3
    m_memberList->setRowHighlight0x7c4ba0(m_rowCount,
        kColor97DE10.r, kColor97DE10.g, kColor97DE10.b, kColor97DE10.a,
        0.0f, m_field0a8, m_field0ac);
    ++m_rowCount;
}

// Appends a squad row when squad information exists, using memberCount and a six-player capacity.
// Unassigned non-commanders receive voice help on each row refresh.
void HudSquad::paintSquadRow(int team, int squad, int memberCount)
{
    ISquadInfo* const info =
        static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squad);
    if (!info)
        return;
    meme::Color c;
    if (squad == m_field08c)                                                    // +8C, the selection
        c = mapWhiteColors[2];
    else if (squad == m_squadId)                                                // +94
        c = squadChatColors[2];
    else
        c = kColor97DE20;
    meme::Color lockColor = kColor97E2B0;
    if (info->getIsPrivate())                                                   // ISquadInfo +04
        lockColor = kColor97DE20;
    std::wstring buttonText = hudManager->getLocalizedString(
        std::string("HUD_TEXT_MENU_SPAWN_SQUAD_JOIN"));
    meme::Color buttonColor = kColor97DE30;
    if ((m_squadId != squad && m_squadId > 0) || m_isCommander)
        buttonColor = kColor97E2B0;                                             // greyed
    else if (m_squadId == 0)
        hudManager->enableVOHelpMessage(12);                                    // +F8 78FDB2
    else if (m_squadId == squad)
        buttonText = hudManager->getLocalizedString(
            std::string("HUD_TEXT_MENU_SPAWN_SQUAD_LEAVE"));                   // 78FDDF, copy-assigned
    meme::TextureHandle arrow = m_textures[0];                                  // +14 button_upp
    const int bit = squad < 0 ? 0 : (squad > 9 ? 9 : squad);
    if (!m_expanded.test(static_cast<std::size_t>(bit)))                       // _Xran 78FE1F unreachable
        arrow = m_textures[1];                                                  // +18 button_down
    m_memberList->appendImageCell0x7c8300(0, m_textures[7], 15.0f, 15.0f,     // iconframe
        static_cast<float>(m_columns[0]), m_field0b0, c.r, c.g, c.b, c.a);
    m_memberList->appendPositionedCell0x7c8210(1, formatString("%i", squad),
        5.0f, m_field0b4, c.r, c.g, c.b, c.a, 0);
    m_memberList->appendCell(2, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(3, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(4, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendPositionedCell0x7c8210(5,
        formatString(kSquadNameFormat,
            getUpperCase(std::string(info->getSquadName().c_str())).c_str()),
        -21.0f, m_field0b4, c.r, c.g, c.b, c.a, 0);
    m_memberList->appendPositionedCell0x7c8210(6,
        formatString(kSquadCountFormat, memberCount, 6),                       // capacity literal
        0.0f, m_field0b4, c.r, c.g, c.b, c.a, 2);
    m_memberList->appendImageCell0x7c8300(7, m_textures[7], 15.0f, 15.0f, 0.0f, m_field0b0,
        kColor97E2B0.r, kColor97E2B0.g, kColor97E2B0.b, kColor97E2B0.a);
    m_memberList->appendImageCell0x7c8300(8, m_textures[8], 15.0f, 15.0f, 0.0f, m_field0b0,
        lockColor.r, lockColor.g, lockColor.b, lockColor.a);                    // 7900C7 lockicon
    m_memberList->appendImageCell0x7c8300(9, m_textures[6], 80.0f, 17.0f, 0.0f, m_field0b0 - 1.0f,
        kColor97DD50.r, kColor97DD50.g, kColor97DD50.b, kColor97DD50.a);
    m_memberList->appendPositionedWideCell0x7c8470(10, buttonText, 0.0f, m_field0b4,
        buttonColor.r, buttonColor.g, buttonColor.b, buttonColor.a, 1);
    m_memberList->appendCell(11, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendImageCell0x7c8300(12, arrow, 17.0f, 17.0f, 0.0f, m_field0b0 - 1.0f,
        kColor97DD50.r, kColor97DD50.g, kColor97DD50.b, kColor97DD50.a);
    m_memberList->appendCell(13, formatString("%i", squad), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(14, formatString("%i", 2), 1.0f, 1.0f, 1.0f, 1.0f, 0);   // row type 2
    m_memberList->setRowHighlight0x7c4ba0(m_rowCount,
        kColor97DE10.r, kColor97DE10.g, kColor97DE10.b, kColor97DE10.a,
        0.0f, m_field0a8, m_field0ac);
    ++m_rowCount;
}

// 7902C0 (1,998 B). One player row - row type 4. A null player paints nothing at all.
// The team argument is never read. getPlayerRankIcon is called and its answer discarded. The
// greyed test is `!getIsAlive() || isPlayerAlive(player)`, not negated - HudScoreboard::paintRow's
// shape. The local-mute scan asks getId() once per node. Every palette is read at the fixed [2].
void HudSquad::paintPlayerRow(world::IPlayer* player, int)
{
    if (!player)
        return;
    const meme::TextureHandle kitIcon = hudManager->getPlayerKitIcon(player, false, false); // +64
    hudManager->getPlayerRankIcon(player);                                      // +68, discarded
    meme::TextureHandle vehicleIcon = hudManager->getPlayerVehicleIcon(player, true); // +60
    if (!vehicleIcon)
        vehicleIcon = m_textures[2];                                            // empty.tga
    meme::TextureHandle leaderIcon;
    if (player->getIsSquadLeader()) {                                           // PE +0x138
        const int squad = player->getSquadId();                                 // read once
        if (!m_squadIcons[squad])                                               // unbounded
            m_squadIcons[squad] = m_system->loadTexture(
                formatString(kSquadIconFormat, squad).c_str(), true);           // +8C, retried if 0
        leaderIcon = m_squadIcons[squad];
    } else {
        leaderIcon = m_textures[2];
    }
    meme::TextureHandle voipIcon = m_textures[10];                              // icon_speaker
    meme::Color voipColor = kColor97DE40;
    bool voipShown = false;
    if (player->getVoipEnabled()) {                                             // PE +0x268
        voipShown = true;
        bool mutedLocally = false;                                              // event-59 ids
        if (!m_eventPlayers.empty())
            for (std::list<int>::const_iterator it = m_eventPlayers.begin();
                 it != m_eventPlayers.end(); ++it)
                if (*it == player->getId()) {                                   // per node
                    mutedLocally = true;
                    break;
                }
        if (mutedLocally) {
            voipIcon = m_textures[9];                                           // voipMuteGrey
            voipColor = kColor97DD50;
        } else if (player->getIsMuted()) {                                      // PE +0x274
            voipIcon = m_textures[11];                                          // speakerMute
            voipColor = kColor97DD50;
        } else if (player->getIsTalking()) {                                    // PE +0x278
            voipColor = kColor97DE40;                                           // the same yellow
        } else {
            voipColor = kColor97E2B0;
        }
    }
    meme::Color text;
    if (!player->getIsAlive() || hudManager->isPlayerAlive(player))            // not negated
        text = scoreboardDeadColors[2];
    else if (player->getId() == m_field090)                                     // the selection
        text = mapWhiteColors[2];
    else if ((player->getSquadId() == m_squadId && m_squadId > 0)               // signed here
             || (player == world::playerManager->getLocalHumanPlayer() && m_isCommander))
        text = scoreboardSquadColors[2];
    else
        text = scoreboardNameColors[2];
    const meme::Color leaderTint = squadChatColors[2];                          // snapshot, 790571
    m_memberList->appendImageCell0x7c8300(0, m_textures[7], 15.0f, 15.0f,
        static_cast<float>(m_columns[0]), m_field0b0,
        kColor97E2B0.r, kColor97E2B0.g, kColor97E2B0.b, kColor97E2B0.a);
    m_memberList->appendImageCell0x7c8300(1, vehicleIcon, 15.0f, 15.0f, 0.0f, m_field0b0,
        kColor97E2A0.r, kColor97E2A0.g, kColor97E2A0.b, kColor97E2A0.a);
    m_memberList->appendImageCell0x7c8300(2, m_textures[7], 15.0f, 15.0f, 0.0f, m_field0b0,
        kColor97E2B0.r, kColor97E2B0.g, kColor97E2B0.b, kColor97E2B0.a);
    m_memberList->appendImageCell0x7c8300(3, kitIcon, 15.0f, 15.0f, 0.0f, m_field0b0,
        kColor97E2A0.r, kColor97E2A0.g, kColor97E2A0.b, kColor97E2A0.a);
    m_memberList->appendPositionedCell0x7c8210(4,
        formatString(kPlayerNameFormat, player->getName().c_str()),            // PE +0x20
        -4.0f, m_field0b4, text.r, text.g, text.b, text.a, 0);
    m_memberList->appendCell(5, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(6, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(7, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(8, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendImageCell0x7c8300(9, leaderIcon, 16.0f, 16.0f,
        static_cast<float>(m_columns[9] - m_columns[7]) - 18.0f, m_field0b0 - 1.0f,   // int SUB, fild
        leaderTint.r, leaderTint.g, leaderTint.b, leaderTint.a);
    m_memberList->appendCell(10, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendImageCell0x7c8300(11, m_textures[7], 15.0f, 15.0f, 0.0f, m_field0b0,
        kColor97E2B0.r, kColor97E2B0.g, kColor97E2B0.b, kColor97E2B0.a);
    if (voipShown)
        m_memberList->appendImageCell0x7c8300(12, voipIcon, 15.0f, 15.0f, 0.0f, m_field0b0,
            voipColor.r, voipColor.g, voipColor.b, voipColor.a);
    else
        m_memberList->appendCell(12, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(13, formatString("%i", player->getId()), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    m_memberList->appendCell(14, formatString("%i", 4), 1.0f, 1.0f, 1.0f, 1.0f, 0);   // row type 4
    if (player->getId() == m_playerId) {                                        // the local player
        m_localPlayerRow = m_rowCount;                                          // +7C, 7909FE
        m_memberList->setRowHighlight(m_rowCount,                               // 7C4AF0 itself
            0.9960784316062927f, 0.7529411911964417f, 0.08235294371843338f, 0.3499999940395355f,
            0.0f, m_field0a8, m_field0ac);
        ++m_rowCount;
        return;
    }
    const int parity = m_rowCount >= 0 ? m_rowCount % 2 : m_rowCount % 2 + 2;  // floored
    if (parity > 0)
        m_memberList->setRowHighlight0x7c4ba0(m_rowCount,
            kColor97DE00.r, kColor97DE00.g, kColor97DE00.b, kColor97DE00.a,
            0.0f, m_field0a8, m_field0ac);
    ++m_rowCount;
}

// 790A90 (1,116 B). One invite-list row. A null player still paints "", "", "-1" - that is how
// rebuildInvitePlayers' padding shows.
// The team argument is never read. The "other squad" test is evaluated twice with fresh
// getSquadId() calls, and getId() is asked once per list scan. getIsInvitedToSquad runs even for
// an excluded player that is not selected, before the excluded veto. The second squad test is
// unsigned (`test eax,eax; jbe`), i.e. != 0 - reproduced with a cast.
void HudSquad::paintInviteRow(world::IPlayer* player, int, int excludedSquad)
{
    if (player) {
        meme::TextureHandle checkIcon = m_textures[3];                          // checkbox_empty
        if (excludedSquad < 0 && player->getSquadId() != m_squadId) {
            checkIcon = m_textures[2];                                          // no box
        } else {
            bool selected = false;
            const int id = player->getId();
            if (!m_selectedPlayers.empty())
                for (std::list<int>::const_iterator it = m_selectedPlayers.begin();
                     it != m_selectedPlayers.end(); ++it)
                    if (id == *it) {
                        selected = true;
                        break;
                    }
            bool excluded = false;
            const int repeatedId = player->getId();                             // 790B0B, again
            if (!m_excludedPlayers.empty())
                for (std::list<int>::const_iterator it = m_excludedPlayers.begin();
                     it != m_excludedPlayers.end(); ++it)
                    if (repeatedId == *it) {
                        excluded = true;
                        break;
                    }
            if (excluded && player->getSquadId() == m_squadId)
                checkIcon = m_textures[4];                                      // checkbox_cross
            else if ((selected || player->getIsInvitedToSquad(m_squadId)) && !excluded)   // PE +0x1F4
                checkIcon = m_textures[5];                                      // checkbox_ask
        }
        meme::Color text;
        if (excludedSquad < 0 && player->getSquadId() != m_squadId) {           // 790B63, re-tested
            text = squadInviteColors[2];                                        // A1C798, whole ...
            text.a = 0.5f;                                                      // ... then alpha 0.5
        } else if (!player->getIsAlive() || hudManager->isPlayerAlive(player)) {
            text = scoreboardDeadColors[2];
        } else if (player->getId() == m_field090) {
            text = mapWhiteColors[2];
        } else if (player->getSquadId() == m_squadId
                   && static_cast<unsigned int>(player->getSquadId()) > 0u) {   // 790C26 `jbe`
            text = scoreboardSquadColors[2];
        } else {
            text = scoreboardNameColors[2];
        }
        m_inviteList->appendPositionedCell0x7c8210(0,
            formatString(kPlayerNameFormat, player->getName().c_str()),
            static_cast<float>(m_columns[10]), m_field0b4,                     // fild +E0, raw +B4
            text.r, text.g, text.b, text.a, 0);
        m_inviteList->appendImageCell0x7c8300(1, checkIcon, 15.0f, 15.0f, 0.0f, m_field0b0,
            kColor97DD50.r, kColor97DD50.g, kColor97DD50.b, kColor97DD50.a);
        m_inviteList->appendCell(2, formatString("%i", player->getId()),
            1.0f, 1.0f, 1.0f, 1.0f, 0);
    } else {
        m_inviteList->appendCell(0, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_inviteList->appendCell(1, std::string(""), 1.0f, 1.0f, 1.0f, 1.0f, 0);
        m_inviteList->appendCell(2, std::string("-1"), 1.0f, 1.0f, 1.0f, 1.0f, 0);
    }
    const int parity = m_rowCount >= 0 ? m_rowCount % 2 : m_rowCount % 2 + 2;  // floored
    if (parity > 0)
        m_inviteList->setRowHighlight0x7c4ba0(m_rowCount,
            kColor97DE00.r, kColor97DE00.g, kColor97DE00.b, kColor97DE00.a,
            0.0f, m_field0a8, m_field0ac);
    ++m_rowCount;
}

// 791030 (83 B). The commander block: the band, and - while expanded - the commander's own player
// row. Selecting the commander expands it.
void HudSquad::fillCommanderRows(int team)
{
    const int commander = static_cast<ISquadManager*>(squadManager)->getCommander(team);  // +0x70
    if (m_field090 == commander)                                                // +90
        m_field09f = true;                                                      // +9F
    paintCommanderHeading(team);
    if (m_field09f)
        paintPlayerRow(world::playerManager->getPlayerFromId(commander), team);
}

// 791090 (124 B). One invite row per player - a null entry still gets its row - skipping (for a
// real player only) commanders, the local player and, when excludedSquad >= 0, that squad.
void HudSquad::fillInviteRows(const std::list<world::IPlayer*>& players, int team,
                              int excludedSquad)
{
    if (players.size() == 0)
        return;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it) {
        world::IPlayer* const player = *it;
        if (player) {
            if (player->getIsCommander())                                       // PE +0x110
                continue;
            if (player->getId() == m_playerId)                                  // +80
                continue;
            if (excludedSquad >= 0 && player->getSquadId() == excludedSquad)
                continue;
        }
        paintInviteRow(player, team, excludedSquad);
    }
}

// 791110 (204 B). One squad: its row, and while expanded its leader (from the squad manager, not
// the list) then every member not flagged leader. The member walk has no null test. `members`
// is PlayerManager's scratch list; nothing in the walk refills it.
void HudSquad::fillSquadRows(const std::list<world::IPlayer*>& members, int team,
                             int squad)
{
    const std::size_t count = members.size();
    if (count == 0)
        return;
    paintSquadRow(team, squad, static_cast<int>(count));
    const int bit = squad < 0 ? 0 : (squad > 9 ? 9 : squad);
    if (!m_expanded.test(static_cast<std::size_t>(bit)))                       // +A4
        return;
    paintPlayerRow(world::playerManager->getPlayerFromId(
        static_cast<ISquadManager*>(squadManager)->getSquadLeader(team, squad)), team);   // +0x3C
    for (std::list<world::IPlayer*>::const_iterator it = members.begin(); it != members.end(); ++it)
        if (!(*it)->getIsSquadLeader())                                         // PE +0x138
            paintPlayerRow(*it, team);
}

// 791740 (404 B). m_invitePlayers <- the selected ids that resolve to a player and are not
// excluded, then padded with null entries.
// The pad count is excluded - selected - (players in the local squad) + 6, all read from the
// members; it goes negative (no padding) easily. As shipped.
void HudSquad::rebuildInvitePlayers(const std::list<int>& selected)
{
    m_invitePlayers.clear();                                                    // +F4
    if (selected.size() != 0) {
        for (std::list<int>::const_iterator it = selected.begin(); it != selected.end(); ++it) {
            world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);   // +0x20
            if (!player)
                continue;
            if (m_excludedPlayers.size() != 0
                    && std::find(m_excludedPlayers.begin(), m_excludedPlayers.end(), *it)
                           != m_excludedPlayers.end())
                continue;
            m_invitePlayers.push_back(player);
        }
    }
    const int selectedCount = static_cast<int>(m_selectedPlayers.size());      // +108
    const int excludedCount = static_cast<int>(m_excludedPlayers.size());      // +114
    const int padding = excludedCount - selectedCount
        - world::playerManager->getNrOfPlayersInSquad(m_team, m_squadId) + 6;  // PE +0x9C
    for (int i = 0; i < padding; ++i)
        m_invitePlayers.push_back(nullptr);
}

void HudSquad::updateString(const char** destination, const char* source)
{
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
void HudSquad::refreshCreateTag(bool resetLocked)
{
    {
        const std::string tag = formatString("%i", hudManager->findEmptySquad(m_team));
        updateString(&m_createTag, tag.c_str());
    }
    if (resetLocked) m_createLocked = false;
}
std::string HudSquad::getAvailableSquadName(int team, unsigned int index)
{
    for (int squad = 0; squad < 10; ++squad) {
        if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 0) {
            ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squad);
            if (info && defaultSquadNames[index] == info->getSquadName()) {
                // Retail compares with756 (the VC7.1 array byte size), not27.
                // Preserve this missing element-bound check; no native sizeof substitution.
                if (index < 0x2f4u) return getAvailableSquadName(team, index + 1);
                break;
            }
        }
    }
    return std::string(defaultSquadNames[index]);
}
bool HudSquad::setCreateMode(bool enabled)
{
    if ((m_squadId == 0 && !m_isCommander) || !enabled) {
        int squad = hudManager->findEmptySquad(m_team);
        if (m_squadId > 0 && world::playerManager->getNrOfPlayersInSquad(m_team, m_squadId) == 1)
            squad = m_squadId;
        if (squad > 0) {
            hudManager->prepareSquadCreate(enabled);
            m_createVisible = enabled;
            if (enabled) {
                hudManager->setFlagE6(enabled);
                const std::string name = getAvailableSquadName(m_team, squad);
                hudManager->setSquadEditText(name);
                hudManager->setFlagE7(true);
                refreshCreateTag(true);
            } else m_renameVisible = false;
            return true;
        }
        m_createVisible = false;
        const std::wstring text = hudManager->getLocalizedString("HUD_HUD_SQUAD_MAX_REACHED");
        hudManager->addServerChat(text, 0);
        g_bfSound->playSound(std::string("warning"));
        return false;
    }
    if (m_isCommander) {
        const std::wstring text = hudManager->getLocalizedString("HUD_HUD_SQUAD_COMMANDER_MUST_RESIGN");
        hudManager->addServerChat(text, 0);
        g_bfSound->playSound(std::string("warning"));
        return false;
    }
    ISquadInfo* info = static_cast<ISquadManager*>(squadManager)->getSquadInfo(m_team, m_squadId);
    if (info) {
        const std::string name(info->getSquadName().c_str());
        const std::wstring text = hudManager->getLocalizedString("HUD_HUD_SQUAD_MUST_LEAVE_TO_CREATE");
        const std::wstring message = hudManager->replaceWideTokenFromString(text, "#SQUADNAME#", name);
        hudManager->addServerChat(message, 0);
    }
    return false;
}
void HudSquad::leaveAndReinvite()
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (m_squadId <= 0 || !player) return;
    const int firstId = player->getId();
    static_cast<ISquadManager*>(squadManager)->removeFromSquad(m_team, m_squadId, firstId, false);
    const int secondId = player->getId();
    static_cast<ISquadManager*>(squadManager)->inviteToSquad(m_team, m_squadId, secondId, false, false);
    const int groupId = player->getSpawnGroup();
    ISpawnGroup* group = static_cast<SpawnManager*>(spawnManager)->getSpawnGroup(groupId);
    if (group && group->getSquad() > 0) hudManager->getKitSelection()->setSpawnGroup(0);
}

// SquadMenu command handlers for list clicks and creation/invitation controls.

// doubleClickNew (row 189) and doubleClickInvite (row 197). both are the image'S shared
// one-byte `ret`. With both bodies folded to it, the two generated console classes became identical
// and were folded too: row 197's constructor (76DD85) stores row 189's table 92DD08, so both rows run
// executor 76D927. A double click on either list does nothing beyond that executor's getSquad().
void HudSquad::doubleClickNew() {}      // 83D270, actual shared RET-only body
void HudSquad::doubleClickInvite() {}   // 83D270, the same folded body

// 78F820 / 78F8B0 (137 B each). The member list's current row, decoded: hidden column 14 is the row
// type, and only a squad row (2) / a player row (4) answers its column 13; anything else is -1.
// With no current row getSelectedString (7C48D0) answers "", which atoi reads as 0 - the
//   commander band's type. The first string is destroyed before the type test.
int HudSquad::getSelectedRowSquad()
{
    if (std::atoi(m_memberList->getSelectedString(14).c_str()) != 2)            // +124, atoi
        return -1;
    return std::atoi(m_memberList->getSelectedString(13).c_str());
}
int HudSquad::getSelectedRowPlayer()
{
    if (std::atoi(m_memberList->getSelectedString(14).c_str()) != 4)
        return -1;
    return std::atoi(m_memberList->getSelectedString(13).c_str());
}

// 78F940 (100 B). A squad row's lock cell (column 8): only the leader of that squad may flip it.
// The squad is decoded from the list again here, not taken from singleClickNew's copy.
// setIsPrivate's trailing false is pushed before getIsPrivate is even asked; m_team is read twice.
void HudSquad::toggleSquadPrivate()
{
    const int squad = getSelectedRowSquad();
    if (squad <= 0 || !m_isSquadLeader || m_squadId != squad)                   // +9C / +94
        return;
    ISquadInfo* const info =
        static_cast<ISquadManager*>(squadManager)->getSquadInfo(m_team, squad); // +0x28
    if (!info)
        return;
    static_cast<ISquadManager*>(squadManager)->setIsPrivate(m_team, squad,
        !info->getIsPrivate(), false);                                          // +0x20, ISquadInfo +04
}

// 790EF0 (86 B). A squad row's arrow cell (column 12): flip that squad's expansion bit. The test uses
// the index clamped to 0..9 (bitset _Xran 78F590 unreachable); the RAW squad goes to
// setSquadExpanded, which clamps again.
void HudSquad::toggleSquadExpanded(int squad)
{
    const int bit = squad < 0 ? 0 : (squad > 9 ? 9 : squad);
    setSquadExpanded(squad, !m_expanded.test(static_cast<std::size_t>(bit)));  // +A4, 78FA60
}

// Handles a click in the squad list.
void HudSquad::singleClickNew()
{
    const int squad = getSelectedRowSquad();                                    // 78F820 -> ebx
    const int playerId = getSelectedRowPlayer();                                // 78F8B0 -> [ebp-4]
    const int rowType = std::atoi(m_memberList->getSelectedString(14).c_str()); // 7C48D0, atoi
    switch (rowType) {
    case 0:
        switch (m_memberList->m_column) {                                       // ListBoxData +0x0C
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 11:
            togglePlayerSelection(
                static_cast<ISquadManager*>(squadManager)->getCommander(m_team));   // +0x70
            return;
        case 10:
            if (!m_isCommander                                                   // +9D
                    && static_cast<ISquadManager*>(squadManager)->getCommander(m_team) < 0)
                hudManager->commanderApply();                                    // +2B0
            else if (m_isCommander)                                              // re-read
                hudManager->commanderResign();                                   // +2B4
            else if (ClientVoteManager::getInstance()->voteIssueAllowed())
                hudManager->commanderMutiny();                                   // +2B8
            return;
        case 12:
            m_field09f = !m_field09f;                                            // +9F
            return;
        default:                                                                 // 7, 8, 9
            return;
        }
    case 2:
        switch (m_memberList->m_column) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 11:
            toggleSquadSelection(squad);
            return;
        case 8:
            toggleSquadPrivate();
            return;
        case 10:
            if (hudManager->getMode() == 26 && m_isSquadLeader)                  // +1E0 / +9C
                hudManager->setMode(17, true);                                   // +1DC
            if (squad == m_squadId)                                              // +94
                leaveAndReinvite();
            else if (!m_isCommander)                                             // +9D
                joinSquad(squad);
            return;
        case 12:
            toggleSquadExpanded(squad);
            return;
        default:                                                                 // 7, 9
            return;
        }
    case 3:
        if (m_memberList->m_column == 10)
            hudManager->submitSquadCreate();                                     // +28C
        return;
    case 4:
        togglePlayerSelection(playerId);
        return;
    default:                                                                    // 1, and the rest
        return;
    }
}

// 78F220 (17 B). The create panel's lock button (SquadCreateLockedShow): flip +EA. BF2HudManager's
// HUD event 14 (75A793) reads it back when the new squad is confirmed and passes it to setIsPrivate;
// refreshCreateTag(true) resets it.
void HudSquad::squadCreateLockToggle()
{
    m_createLocked = !m_createLocked;                                           // +EA
}

// 78F190 (26 B). The create panel's name field: while the create panel is up (+E8), hand the
// keyboard to the squad-name edit box (BF2HudManager 7B11E0). Explicit AL 1 / 0; the console row
// drops it.
bool HudSquad::setSquadNameSelect()
{
    if (!m_createVisible)                                                       // +E8
        return false;
    hudManager->selectSquadCreateName();                                        // 7B11E0, non-virtual
    return true;
}

// 7918E0 (96 B). The invite list's open (1) / close (0) buttons - squad leaders only. Opening drops
// every pending change, re-seeds the pending invitations from the players already invited and
// switches the HUD to mode 26; closing goes back to the squad screen, mode 17.
// It never writes m_inviteVisible: HudInformationAdapter::setHudMode (786260) sets InviteListShow,
//   which is bound to +A1, from the mode. A non-leader's close is ignored as well.
void HudSquad::setShowInviteList(bool show)
{
    if (!m_isSquadLeader)                                                       // +9C
        return;
    if (show) {
        m_excludedPlayers.clear();                                              // +10C, 460320
        m_selectedPlayers.clear();
        collectInvitedPlayers();
        hudManager->setMode(26, true);                                          // +1DC
    } else {
        hudManager->setMode(17, true);
    }
}

// 791690 (169 B). The pending invitations start as the squad's existing invitees: PlayerManager
// +0xA0 getInvitedPlayers - the team's players invited to the squad and not in it (w32ded twin
// 450D40 read to confirm the slot). getLocalHumanPlayer() is asked and its answer discarded.
// A null entry is skipped. The list is PlayerManager's scratch list; nothing here refills it.
void HudSquad::collectInvitedPlayers()
{
    const std::list<world::IPlayer*>& invited =
        world::playerManager->getInvitedPlayers(m_team, m_squadId);            // +0xA0
    world::playerManager->getLocalHumanPlayer();                                // +0x30, discarded
    if (invited.size() == 0)
        return;
    for (std::list<world::IPlayer*>::const_iterator it = invited.begin(); it != invited.end(); ++it)
        if (*it)
            m_selectedPlayers.push_back((*it)->getId());                        // PE +0x50
}

// 78FB50 / 78FBC0 (102 B each). Is the id pending in the kick / the invitation list? With `erase` a
// found id is also removed. Every caller passes true.
bool HudSquad::findExcludedPlayer(int playerId, bool erase)
{
    if (m_excludedPlayers.size() != 0) {                                        // +114
        const std::list<int>::iterator found =
            std::find(m_excludedPlayers.begin(), m_excludedPlayers.end(), playerId);
        if (found != m_excludedPlayers.end()) {
            if (erase)
                m_excludedPlayers.erase(found);
            return true;
        }
    }
    return false;
}
bool HudSquad::findSelectedPlayer(int playerId, bool erase)
{
    if (m_selectedPlayers.size() != 0) {                                        // +108
        const std::list<int>::iterator found =
            std::find(m_selectedPlayers.begin(), m_selectedPlayers.end(), playerId);
        if (found != m_selectedPlayers.end()) {
            if (erase)
                m_selectedPlayers.erase(found);
            return true;
        }
    }
    return false;
}

// Toggles a player in the pending invitation list.
bool HudSquad::toggleInvitePlayer(int playerId)
{
    world::IPlayer* const player = world::playerManager->getPlayerFromId(playerId);   // +0x20
    if (!player)
        return true;
    const int squad = m_squadId;                                                // +94 -> [ebp-4]
    if (player->getSquadId() != squad && !player->getIsInvitedToSquad(squad)) { // +0x108 / +0x1F4
        if (findSelectedPlayer(playerId, true))                                 // withdrawn
            return true;
        const int pending = static_cast<int>(m_selectedPlayers.size());        // +108
        const int kicks = static_cast<int>(m_excludedPlayers.size());          // +114
        const int room = 6 - world::playerManager->getNrOfPlayersInSquad(m_team, m_squadId);  // +0x9C
        if (pending - kicks >= room)                                            // signed `jge`
            return false;
        m_selectedPlayers.push_back(playerId);
        return true;
    }
    if (findExcludedPlayer(playerId, true)) {                                   // restored
        const int pending = static_cast<int>(m_selectedPlayers.size());
        const int kicks = static_cast<int>(m_excludedPlayers.size());
        if (world::playerManager->getNrOfPlayersInSquad(m_team, m_squadId) + (pending - kicks) > 6
                && m_selectedPlayers.size() != 0)                               // re-read after
            m_selectedPlayers.pop_back();
        return true;
    }
    m_excludedPlayers.push_back(playerId);                                      // inlined 5BE700
    return true;
}

// 791F50 (126 B). The invite list's click ("1 SquadMenu.singleClickInvite"): only the checkbox column
// (1) acts, on the id in hidden column 2. A full squad answers with the "warning" sound (ds:926A74)
// through g_bfSound (ds:A23DC0) +0x2C, the typed playSound(const std::string&).
// The heading and padding rows carry "-1": no player, toggleInvitePlayer answers true, nothing.
// The selected string is destroyed before toggleInvitePlayer runs.
void HudSquad::singleClickInvite()
{
    if (m_inviteList->m_column != 1)                                            // +128 -> +0x0C
        return;
    const int playerId = std::atoi(m_inviteList->getSelectedString(2).c_str()); // 7C48D0, atoi
    if (!toggleInvitePlayer(playerId))
        g_bfSound->playSound(std::string("warning"));                           // +0x2C
}

// Applies pending removals before sending invitations.
void HudSquad::applyInviteList()
{
    if (m_excludedPlayers.size() != 0) {                                        // +114
        for (std::list<int>::const_iterator it = m_excludedPlayers.begin();
             it != m_excludedPlayers.end(); ++it) {
            world::IPlayer* const player = world::playerManager->getPlayerFromId(*it);   // +0x20
            if (!player)
                continue;
            const int squad = m_squadId;                                        // +94 -> ebx
            if (player->getSquadId() == squad)                                  // +0x108
                static_cast<ISquadManager*>(squadManager)->removeFromSquad(m_team, squad, *it,
                                                                           true);   // +0x34
            else
                static_cast<ISquadManager*>(squadManager)->inviteToSquad(m_team, squad, *it,
                                                                         false, false);  // +0x2C
        }
    }
    if (m_selectedPlayers.size() != 0) {                                        // +108
        for (std::list<int>::const_iterator it = m_selectedPlayers.begin();
             it != m_selectedPlayers.end(); ++it) {
            if (m_excludedPlayers.size() != 0                                   // re-read per id
                    && std::find(m_excludedPlayers.begin(), m_excludedPlayers.end(), *it)
                           != m_excludedPlayers.end())
                continue;
            static_cast<ISquadManager*>(squadManager)->inviteToSquad(m_team, m_squadId, *it,
                                                                     true, false);   // +0x2C
        }
    }
    if (m_isSquadLeader)                                                        // +9C
        hudManager->setMode(17, true);                                          // +1DC
}
} } }
#endif
