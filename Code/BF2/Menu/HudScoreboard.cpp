// HUD scoreboard display. [INFERRED] Name and source placement.
#include "HudScoreboard.h"
#if !DEDICATED
#include "BfListBoxData.h"
#include "BF2HudManager.h"                      // convertHudEventText (74DC00)
#include "HudColors.h"                          // scoreboardColors (A1CF18)
#include "../Core/StringUtil.h"                 // formatString (4310D0)
#include "../Scene/Object/PlayerScoreData.h"    // the four score fields the row reads
#include "../Game/Common/ClientVoteManager.h"    // votingEnabled / getTeamVoteOnly / m_dword04
#include "HudVoteMap.h"                          // singleClick: m_selected (+0x24)
#include "../Game/IGame.h"                       // singleClick: game->sendGameEvent (+0x8C)
#include "../Game/GameEvents/VoipPlayerMuteEvent.h"   // singleClick's mute column (541F60)
#include "../Game/GameLogic/SquadManager/ISquadManager.h"  // getSquadInfo (+0x28) / getCommander (+0x70)
#include "../Game/GameLogic/SquadManager/ISquadInfo.h"     // getSquadName (+0x00)
#include "../Game/GameLogic/ScoreSystem/ScoreManager.h"    // getTeamScore (+0x10)
#include "../External/BFMeme/System.h"
#include "../Core/EventManager.h"
#include "../Game/HudInformationLayer.h"
#include "../Game/Main/BF2ClientSeams.h"
#include "../Game/Objects/IPlayer.h"
#include "../Scene/Object/PlayerManager.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>
namespace dice { namespace hfe {
namespace world { extern IBase* scoreManager; }   // ds:9C5DA0 - Core/Globals.cpp holds the tree's
                                                 // IBase* form; setScoreManager (5498B8) writes it.
extern IBase* squadManager;                      // ds:99EF84 - GameClient.cpp already names it.
namespace hud {
namespace {
// The numeric columns' format at ds:933D54, byte for byte: 0xA7 '1' '%' 'd' 0xA7 '0'. 0xA7 is the
// section sign BF2 uses to open an inline markup run, so every number is wrapped in style 1 and
// then reset to style 0. Split across literals because "\xA71" would lex as one hex escape.
const char* const kScoreColumnFormat = "\xA7" "1%d" "\xA7" "0";
// The two name-column formats, also byte for byte: ds:932030 and ds:933DC4. Same markup wrapper.
const char* const kNameFormat = "\xA7" "1%s" "\xA7" "0";
const char* const kManageNameFormat = "\xA7" "1%s - %i" "\xA7" "0";

// Every filler cell in paintRow is this same call - the empty string at ds:87FB54, opaque white,
// value 0 - so it is written once here rather than fifteen times inline.
void appendEmpty(meme::BfListBoxData* list, int column)
{
    list->appendCell(column, std::string(), 1.0f, 1.0f, 1.0f, 1.0f, 0);
}

// The counter row a player belongs to: ten squads plus a commander slot at index 10.
// paintRow recomputes this for every one of its five counter updates, with fresh virtual calls
// each time - keeping it a function rather than a local preserves that.
int commanderOrSquad(world::IPlayer* player)
{
    return player->getIsCommander() ? 10 : player->getSquadId();   // PE +0x110 / +0x108
}
}
HudScoreboard::HudScoreboard(meme::System* system)
    : m_system(system), m_rowCount(0), m_visibleRows(21), m_squad(0), m_player(-1),
      m_team(2), m_selectedPlayer(-1), m_isSquadLeader(false), m_isCommander(false),
      m_field028(0), m_rowHeight(19.0f), m_rowOffset(2.0f), m_field034(0),
      m_currentRow(0), m_visible(false), m_field040(0), m_field044(0),
      m_toggleSquads(false), m_toggleScore(true), m_toggleManage(false), m_teamVoteOnly(true)
{
    eventManager->registerEventHandler(ECHud, this, 0);
    const int columns[9] = {5, 5, 21, 44, 212, 244, 276, 308, 340};
    for (int i = 0; i < 9; ++i) m_columns[i] = columns[i];
    resetCounters(0);
    resetCounters(1);
    resetCounters(2);
    if (m_system) {
        const char* const textures[13] = {
            "Ingame/Respawn/iconframe.tga", "Ingame/Scoreboard/Icons/checkbox_empty.tga",
            "Ingame/Kits/Icons/kit_Commander.tga", "Ingame/Scoreboard/Icons/checkbox_voipMuteGrey.tga",
            "Ingame/Scoreboard/Icons/checkbox_voipMute.tga", "Ingame/Scoreboard/Icons/checkbox_voipTalk.tga",
            "Ingame/Scoreboard/Icons/checkbox_voipInactive.tga", "Ingame/Scoreboard/Icons/vote_square.tga",
            "Ingame/Scoreboard/Icons/kickbutton_up.tga", "Ingame/Scoreboard/Icons/kickbutton_down.tga",
            "Ingame/Scoreboard/Icons/kickvote_frame.tga", "Ingame/Scoreboard/Icons/kickvote_yes.tga",
            "Ingame/Scoreboard/Icons/kickvote_no.tga"
        };
        for (int i = 0; i < 13; ++i)
            m_textures[i] = m_system->loadTexture(textures[i], true);
    } else {
        for (int i = 0; i < 13; ++i) m_textures[i] = 0;
    }
}
HudScoreboard::~HudScoreboard()
{
    eventManager->unregisterEventHandler(ECHud, this);
    // Only the native muted-player list is destroyed afterwards. Retail does
    // not delete the five list-data objects or release the texture handles.
}
void HudScoreboard::setSystem(meme::System* system) { m_system = system; }
bool HudScoreboard::registerFunctions() { return true; }
void HudScoreboard::resetCounters(int team)
{
    for (int squad = 0; squad < 11; ++squad) {
        m_counters[0][team][squad] = 0;
        m_counters[1][team][squad] = 0;
        m_counters[2][team][squad] = 0;
        m_counters[3][team][squad] = 0;
        m_counters[4][team][squad] = 0;
    }
}
bool HudScoreboard::registerData()
{
    void* storage = ::operator new(sizeof(meme::BfListBoxData));
    m_friendlyList = storage ? new(storage) meme::BfListBoxData("friendlyScoreboardList") : nullptr;
    if (!m_friendlyList) return false;
    storage = ::operator new(sizeof(meme::BfListBoxData));
    m_enemyList = storage ? new(storage) meme::BfListBoxData("enemyScoreboardList") : nullptr;
    if (!m_enemyList) return false;
    storage = ::operator new(sizeof(meme::BfListBoxData));
    m_finalList = storage ? new(storage) meme::BfListBoxData("FinalScoreboardList") : nullptr;
    if (!m_finalList) return false;
    storage = ::operator new(sizeof(meme::BfListBoxData));
    m_friendlyTotalList = storage ? new(storage) meme::BfListBoxData("friendlyTotalScoreboardList") : nullptr;
    if (!m_friendlyTotalList) return false;
    storage = ::operator new(sizeof(meme::BfListBoxData));
    m_enemyTotalList = storage ? new(storage) meme::BfListBoxData("enemyTotalScoreboardList") : nullptr;
    if (!m_enemyTotalList) return false;

    for (int i = 1; i < 9; ++i) m_friendlyList->addColumn(m_columns[i], true, false);
    m_friendlyList->addColumn(m_columns[8], true, false);
    m_friendlyList->addHiddenColumn();
    m_friendlyList->m_visibleRows = m_visibleRows;
    m_friendlyList->m_field021 = true;
    m_friendlyList->m_field023 = false;
    m_friendlyList->m_field022 = true;

    for (int i = 1; i < 9; ++i) m_enemyList->addColumn(m_columns[i], true, false);
    m_enemyList->addColumn(m_columns[8], true, false);
    m_enemyList->addHiddenColumn();
    m_enemyList->m_visibleRows = m_visibleRows;
    m_enemyList->m_field021 = true;
    m_enemyList->m_field023 = false;
    m_enemyList->m_field022 = true;

    for (int i = 1; i < 9; ++i) m_friendlyTotalList->addColumn(m_columns[i], i != 3, false);
    m_friendlyTotalList->addColumn(m_columns[8], true, false);
    m_friendlyTotalList->m_visibleRows = 1;
    m_friendlyTotalList->m_field021 = false;
    m_friendlyTotalList->m_field022 = true;

    for (int i = 1; i < 9; ++i) m_enemyTotalList->addColumn(m_columns[i], i != 3, false);
    m_enemyTotalList->addColumn(m_columns[8], true, false);
    m_enemyTotalList->m_visibleRows = 1;
    m_enemyTotalList->m_field021 = false;
    m_enemyTotalList->m_field022 = true;

    m_finalList->addColumn(148, true, false);
    m_finalList->addColumn(179, true, false);
    m_finalList->addColumn(212, true, false);
    m_finalList->addColumn(244, true, false);
    m_finalList->m_visibleRows = 20;
    m_finalList->m_field021 = false;
    m_finalList->m_field022 = false;

    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("ToggleSquads", &m_toggleSquads);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("ToggleScore", &m_toggleScore);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("ToggleManage", &m_toggleManage);
    static_cast<HudInformationLayer*>(g_hudInformationLayer)->registerBool("TeamVoteOnly", &m_teamVoteOnly);
    return true;
}
void HudScoreboard::handleEvent(EventCategory category, unsigned int id, EventNode* node, void* data)
{
    if (category == ECHud) handleHudEvent(id, node, data);
}
void HudScoreboard::handleHudEvent(unsigned int id, EventNode*, void* data)
{
    world::IPlayer* player = world::playerManager->getLocalHumanPlayer();
    if (id == 59) {
        // Same unlabelled byte/+4 signed-id payload consumed by HudCommander.
        const bool muted = *static_cast<unsigned char*>(data) != 0;
        int playerId;
        std::memcpy(&playerId, static_cast<unsigned char*>(data) + 4, sizeof(playerId));
        if (player->getId() != playerId) {
            if (muted) m_mutedPlayers.push_back(playerId);
            else if (!m_mutedPlayers.empty()) {
                auto found = std::find(m_mutedPlayers.begin(), m_mutedPlayers.end(), playerId);
                if (found != m_mutedPlayers.end()) m_mutedPlayers.erase(found);
            }
        }
    }
}
// Updates the team totals.
int HudScoreboard::getTotalScore(int team)
{
    int total = 0;
    for (int squad = 0; squad < 10; ++squad)
        if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 0)
            total += m_counters[1][team][squad];
    if (static_cast<ISquadManager*>(squadManager)->getCommander(team))
        total += m_counters[2][team][10];                    // plane 2, not plane 1
    return total;
}
int HudScoreboard::getTotalField04(int team)
{
    int total = 0;
    for (int squad = 0; squad < 10; ++squad)
        if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 0)
            total += m_counters[4][team][squad];
    if (static_cast<ISquadManager*>(squadManager)->getCommander(team))
        total += m_counters[2][team][10];                    // plane 2, not plane 4
    return total;
}
int HudScoreboard::getTotalField14(int team)
{
    int total = 0;
    for (int squad = 0; squad < 10; ++squad)
        if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 0)
            total += m_counters[2][team][squad];
    if (static_cast<ISquadManager*>(squadManager)->getCommander(team))
        total += m_counters[2][team][10];                    // the one place this index is right
    return total;
}
int HudScoreboard::getTotalField10(int team)
{
    int total = 0;
    for (int squad = 0; squad < 10; ++squad)
        if (world::playerManager->getNrOfPlayersInSquad(team, squad) > 0)
            total += m_counters[3][team][squad];
    if (static_cast<ISquadManager*>(squadManager)->getCommander(team))
        total += m_counters[2][team][10];                    // plane 2, not plane 3
    return total;
}

// Draws a total row for the selected scoreboard group.
void HudScoreboard::paintTotalRow(meme::BfListBoxData* list, int team, int squad)
{
    if (squad <= -1)
    {
        appendEmpty(list, 2);
        for (int column = 4; column <= 9; ++column) appendEmpty(list, column);
        for (int plane = 0; plane < 5; ++plane) m_counters[plane][team][10] = 0;
        return;
    }

    meme::Color c = {0.7490196228027344f, 0.7490196228027344f, 0.5607843399047852f, 1.0f};
    if (squad == m_squad && squad > 0)
        c = squadChatColors[team == m_team ? 2 : 1];

    // reinterpret_cast, not static_cast: ScoreManager has two bases, so the IBase* the tree
    // stores is an ambiguous static conversion. EndOfRoundEvent.cpp casts the same global the
    // same way.
    reinterpret_cast<world::ScoreManager*>(world::scoreManager)->getTeamScore(team); // dropped

    int scoreTotal, field04Total, field14Total, field10Total, averagePing;
    if (squad == 11)
    {
        c = {0.7960000038146973f, 0.7960000038146973f, 0.5920000076293945f, 1.0f};
        scoreTotal = getTotalScore(team);
        field04Total = getTotalField04(team);
        field14Total = getTotalField14(team);
        field10Total = getTotalField10(team);
        averagePing = world::playerManager->getNrOfPlayerInTeam(team) <= 0
            ? 0 : static_cast<int>(m_field048[6 + team])
                      / world::playerManager->getNrOfPlayerInTeam(team);
        appendEmpty(list, 0);
        appendEmpty(list, 1);
        // "#PLAYERS#" inside the localized caption is replaced with the team's live headcount.
        std::wstring caption = hudManager->getLocalizedString(
            std::string("HUD_TEXT_MENU_SCOREBOARD_NUMBER_OF_PLAYERS"));
        caption = hudManager->replaceWideTokenFromInt(caption, std::string("#PLAYERS#"),
            world::playerManager->getNrOfPlayerInTeam(team));
        list->appendPositionedWideCell0x7c8470(3, caption, -37.0f,
            static_cast<float>(m_field034), c.r, c.g, c.b, c.a, 0);
        m_field048[6 + team] = 0;
    }
    else
    {
        scoreTotal = m_counters[1][team][squad];
        field14Total = m_counters[2][team][squad];
        field10Total = m_counters[3][team][squad];
        field04Total = m_counters[4][team][squad];
        const int players = world::playerManager->getNrOfPlayersInSquad(team, squad);
        averagePing = players <= 0 ? 0 : m_counters[0][team][squad] / players;
        for (int plane = 0; plane < 5; ++plane) m_counters[plane][team][squad] = 0;
    }

    const float y = static_cast<float>(m_field034);
    appendEmpty(list, 2);
    list->appendPositionedCell0x7c8210(4, formatString(kScoreColumnFormat, scoreTotal),
        0.0f, y, c.r, c.g, c.b, c.a, 1);
    list->appendPositionedCell0x7c8210(5, formatString(kScoreColumnFormat, field04Total),
        0.0f, y, c.r, c.g, c.b, c.a, 1);
    list->appendPositionedCell0x7c8210(6, formatString(kScoreColumnFormat, field14Total),
        0.0f, y, c.r, c.g, c.b, c.a, 1);
    list->appendPositionedCell0x7c8210(7, formatString(kScoreColumnFormat, field10Total),
        0.0f, y, c.r, c.g, c.b, c.a, 1);
    appendEmpty(list, 8);
    list->appendPositionedCell0x7c8210(9, formatString(kScoreColumnFormat, averagePing),
        0.0f, y, c.r, c.g, c.b, c.a, 1);
}

// Adds an unselectable commander heading. It clears commander counters without printing a total row.
void HudScoreboard::paintCommanderHeader(meme::BfListBoxData* list, int team)
{
    meme::Color c = {0.7490196228027344f, 0.7490196228027344f, 0.5607843399047852f, 1.0f};
    if (team == m_team && m_isCommander)
        c = squadChatColors[2];       // the index is 2 unconditionally - the branch pins team==m_team

    list->appendImageCell0x7c8300(0, m_textures[0], 15.0f, 15.0f,
        static_cast<float>(m_columns[0]), m_rowOffset, c.r, c.g, c.b, c.a);
    list->appendImageCell0x7c8300(1, m_textures[2], 15.0f, 15.0f, 0.0f, m_rowOffset,
        c.r, c.g, c.b, c.a);          // kit_Commander
    list->appendPositionedWideCell0x7c8470(3,
        hudManager->getLocalizedString(std::string("HUD_HUD_MENU_COMMANDER_HEADING")),
        -21.0f, static_cast<float>(m_field034), c.r, c.g, c.b, c.a, 0);
    paintTotalRow(list, team, -1);
    list->appendCell0x7c81c0(10, formatString("%i", -1), c.r, c.g, c.b, c.a, 0);
    list->setRowHighlight0x7c4ba0(m_currentRow, 0.4862745106220245f, 0.4784313738346100f,
        0.3882353007793427f, 0.8999999761581421f, 0.0f, m_field028, m_rowHeight);
    ++m_currentRow;
}

// Draws the squad header row.
void HudScoreboard::paintSquadHeader(meme::BfListBoxData* list, int team, int squad)
{
    ISquadInfo* const info =
        static_cast<ISquadManager*>(squadManager)->getSquadInfo(team, squad);
    if (!info) return;

    meme::Color c = {0.7490196228027344f, 0.7490196228027344f, 0.5607843399047852f, 1.0f};
    if (squad == m_squad && squad > 0)
        c = squadChatColors[team == m_team ? 2 : 1];

    const float y = static_cast<float>(m_field034);
    if (squad <= 0)
    {
        appendEmpty(list, 0);
        appendEmpty(list, 1);
        list->appendPositionedWideCell0x7c8470(3,
            hudManager->getLocalizedString(std::string("HUD_TEXT_MENU_SPAWN_SQUAD_UNASSIGNED")),
            -40.0f, y, c.r, c.g, c.b, c.a, 0);
    }
    else
    {
        list->appendImageCell0x7c8300(0, m_textures[0], 15.0f, 15.0f,
            static_cast<float>(m_columns[0]), m_rowOffset, c.r, c.g, c.b, c.a);
        list->appendPositionedCell0x7c8210(1, formatString("%i", squad),
            5.0f, y, c.r, c.g, c.b, c.a, 0);
        const std::string name = getUpperCase(std::string(info->getSquadName().c_str()));
        list->appendPositionedCell0x7c8210(3, formatString(kNameFormat, name.c_str()),
            -21.0f, y, c.r, c.g, c.b, c.a, 0);
    }
    if (squad == m_squad) m_rowCount = m_currentRow;
    paintTotalRow(list, team, squad);
    list->appendCell0x7c81c0(10, formatString("%i", -1), c.r, c.g, c.b, c.a, 0);
    list->setRowHighlight0x7c4ba0(m_currentRow, 0.4862745106220245f, 0.4784313738346100f,
        0.3882353007793427f, 0.8999999761581421f, 0.0f, m_field028, m_rowHeight);
    ++m_currentRow;
}

// The three fills. 7A4820 is the flat list; 7A4780 is the commander's single row; 7A47C0 is one
// squad's band plus its members. All three thread the list through and leave `this` in place.
// fillFlatRows resets the team's counters first and then lets every row accumulate into them.
// fillSquadRows emits nothing at all for an empty squad - header included.
void HudScoreboard::fillFlatRows(meme::BfListBoxData* list,
    const std::list<world::IPlayer*>& players, int team)
{
    resetCounters(team);
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
        paintRow(list, *it, team);
}
void HudScoreboard::fillCommanderRow(meme::BfListBoxData* list, int team)
{
    paintCommanderHeader(list, team);
    paintRow(list, world::playerManager->getPlayerFromId(
        static_cast<ISquadManager*>(squadManager)->getCommander(team)), team);
}
void HudScoreboard::fillSquadRows(meme::BfListBoxData* list,
    const std::list<world::IPlayer*>& players, int team, int squad)
{
    if (players.empty()) return;
    paintSquadHeader(list, team, squad);
    for (std::list<world::IPlayer*>::const_iterator it = players.begin();
         it != players.end(); ++it)
        paintRow(list, *it, team);
}

// Rebuilds a team's scoreboard, grouping only the local team when requested.
// Squads one through nine precede unassigned players; rows use the corresponding score-sorted player lists.
void HudScoreboard::fillSideList(meme::BfListBoxData* list, int team)
{
    list->clearRows();
    const bool grouped = m_toggleSquads && m_team == team;
    m_currentRow = 0;
    if (!grouped)
    {
        fillFlatRows(list, world::playerManager->getPlayersSortedByScore(team), team);
        return;
    }
    if (static_cast<ISquadManager*>(squadManager)->getCommander(team) >= 0)
        fillCommanderRow(list, team);
    for (int squad = 1; squad < 10; ++squad)
        fillSquadRows(list, world::playerManager->getPlayersInSquadSortedByScore(team, squad),
                      team, squad);
    fillSquadRows(list, world::playerManager->getPlayersInSquadSortedByScore(team, 0), team, 0);
}

// Scrolls the friendly table to show the local player, accounting for squad headings when grouped.
void HudScoreboard::updateScrollPosition()
{
    if (m_rowCount <= m_visibleRows) { m_friendlyList->setFirstVisibleRow(0); return; }
    int first = m_rowCount - m_visibleRows + 1;
    if (m_toggleSquads)
        first += world::playerManager->getNrOfPlayersInSquad(m_team, m_squad);
    m_friendlyList->setFirstVisibleRow(first);
}

// Refreshes the scoreboard from current player and team scores.
void HudScoreboard::update(bool visible)
{
    if (visible)
    {
        world::IPlayer* const local = world::playerManager->getLocalHumanPlayer();
        if (local)
        {
            m_squad = local->getSquadId();                 // PE +0x108
            m_isSquadLeader = local->getIsSquadLeader();   // PE +0x138
            m_isCommander = local->getIsCommander();       // PE +0x110
            m_team = local->getTeam();                     // PE +0x0E4
            m_player = local->getId();                     // PE +0x050
            const int enemy = m_team == 1 ? 2 : 1;
            fillSideList(m_friendlyList, m_team);
            fillSideList(m_enemyList, enemy);
            if (m_visible != visible) updateScrollPosition();
            m_friendlyTotalList->clearRows();
            paintTotalRow(m_friendlyTotalList, m_team, 11);
            m_enemyTotalList->clearRows();
            paintTotalRow(m_enemyTotalList, enemy, 11);
        }
        if (ClientVoteManager::getInstance())
            m_teamVoteOnly = ClientVoteManager::getInstance()->getTeamVoteOnly();
    }
    m_visible = visible;
}

// Draws one player row and its score, status and selection indicators.
void HudScoreboard::paintRow(meme::BfListBoxData* list, world::IPlayer* player, int team)
{
    if (!player) return;

    const meme::TextureHandle kitIcon = hudManager->getPlayerKitIcon(player, false, false); // PE +0x64
    const meme::TextureHandle rankIcon = hudManager->getPlayerRankIcon(player);             // PE +0x68
    const bool ownSide = m_team == team;
    const int side = ownSide ? 2 : 1;

    meme::Color text;
    if (!player->getIsAlive() || hudManager->isPlayerAlive(player))            // see the note above
        text = scoreboardDeadColors[side];
    else if ((player->getSquadId() == m_squad && m_squad > 0 && ownSide)
             || (player == world::playerManager->getLocalHumanPlayer() && m_isCommander))
        text = scoreboardSquadColors[side];
    else
        text = scoreboardNameColors[side];

    // the VoIP column's icon ------------------------------------------------------------
    meme::TextureHandle voipIcon = m_textures[1];                 // checkbox_empty
    if (player->getVoipEnabled())                                 // PE +0x268
    {
        bool decided = false;
        if (!m_mutedPlayers.empty())
        {
            for (std::list<int>::const_iterator it = m_mutedPlayers.begin();
                 it != m_mutedPlayers.end(); ++it)
            {
                if (player->getId() != *it) continue;
                if (ownSide) { voipIcon = m_textures[3]; decided = true; }  // voipMuteGrey
                break;
            }
        }
        if (!decided)
        {
            if (player->getIsMuted() && ownSide) voipIcon = m_textures[4];  // voipMute
            else if (player->getIsTalking())     voipIcon = m_textures[5];  // voipTalk
            else                                 voipIcon = m_textures[6];  // voipInactive
        }
    }

    // the kick-vote column's icon -------------------------------------------------------
    meme::TextureHandle voteIcon = 0;
    bool voteTarget = false;
    if (ClientVoteManager::getInstance()->votingEnabled() && !player->getIsAIPlayer())
    {
        if (ClientVoteManager::getInstance()->m_dword04)
        {
            if (ClientVoteManager::getInstance()->m_dword04 == 2)
            {
                if (player->getParticipateInVoting())                       // PE +0x29C
                {
                    switch (static_cast<int>(player->getVotingStatus()))     // PE +0x294
                    {
                    case 0: voteIcon = m_textures[10]; break;                // kickvote_frame
                    case 1: m_selectedPlayer = player->getId();
                            voteIcon = m_textures[11]; break;                // kickvote_yes
                    case 2: voteIcon = m_textures[11]; break;
                    case 3: voteIcon = m_textures[12]; break;                // kickvote_no
                    default: break;
                    }
                    if (player->getTargetOfVote())                           // PE +0x2A4
                    {
                        voteTarget = true;
                        voteIcon = m_selectedPlayer == m_player ? m_textures[9]   // kickbutton_down
                                                                : m_textures[10];
                    }
                }
            }
            else voteIcon = 0;
        }
        else voteIcon = m_textures[8];                                       // kickbutton_up
    }

    if (m_toggleManage)
    {
        appendEmpty(list, 1);
        list->appendImageCell0x7c8300(0, rankIcon, 16.0f, 16.0f,
            static_cast<float>(m_columns[0]), m_rowOffset - 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
        appendEmpty(list, 2);
        list->appendPositionedCell0x7c8210(3,
            formatString(kManageNameFormat, player->getName().c_str(), player->getId()),
            -20.0f, static_cast<float>(m_field034), text.r, text.g, text.b, text.a, 0);
        appendEmpty(list, 5);                                     // before column 4 - shipped order
        list->appendImageCell0x7c8300(4, voipIcon, 15.0f, 15.0f, 0.0f, m_rowOffset,
            1.0f, 1.0f, 1.0f, 1.0f);
        appendEmpty(list, 6);
        appendEmpty(list, 9);
        const bool voteVisible = !ClientVoteManager::getInstance()->getTeamVoteOnly() || ownSide;
        if (voteIcon && voteVisible)
            list->appendImageCell0x7c8300(7, voteIcon, 15.0f, 15.0f, 0.0f, m_rowOffset,
                1.0f, 1.0f, 1.0f, 1.0f);
        else
            appendEmpty(list, 7);
        if (voteTarget && voteVisible)
            // The vote-square cell uses 19-pixel dimensions.
            list->appendImageCell(8, m_textures[7], 19.0f, 19.0f, -2.0f, m_rowOffset - 2.0f,
                0.4745000004768372f, 0.0f, 0.0f, 1.0f);           // vote_square, dark red
        else
            appendEmpty(list, 8);
    }
    else
    {
        list->appendImageCell0x7c8300(0, m_textures[0], 15.0f, 15.0f,          // iconframe
            static_cast<float>(m_columns[0]), m_rowOffset,
            0.5759999752044678f, 0.5730000138282776f, 0.4510000050067902f, 1.0f);
        list->appendImageCell0x7c8300(1, kitIcon, 15.0f, 15.0f, 0.0f, m_rowOffset,
            0.3843137323856354f, 0.3607843220233917f, 0.2549019753932953f, 1.0f);
        float nameX;
        if (m_toggleScore)
        {
            list->appendImageCell0x7c8300(2, rankIcon, 16.0f, 16.0f,
                static_cast<float>(m_columns[0]), m_rowOffset - 1.0f, 1.0f, 1.0f, 1.0f, 1.0f);
            nameX = 0.0f;
        }
        else
        {
            appendEmpty(list, 2);
            nameX = -20.0f;
        }
        list->appendPositionedCell0x7c8210(3, formatString(kNameFormat, player->getName().c_str()),
            nameX, static_cast<float>(m_field034), text.r, text.g, text.b, text.a, 0);

        world::PlayerScoreData* const score = player->getScoreData();         // PE +0x170
        if (!score)
        {
            for (int column = 4; column <= 9; ++column) appendEmpty(list, column);
        }
        else
        {
            const float y = static_cast<float>(m_field034);
            list->appendPositionedCell0x7c8210(4, formatString(kScoreColumnFormat, score->m_score),
                0.0f, y, text.r, text.g, text.b, text.a, 1);
            list->appendPositionedCell0x7c8210(5, formatString(kScoreColumnFormat, score->m_field04),
                0.0f, y, text.r, text.g, text.b, text.a, 1);
            list->appendPositionedCell0x7c8210(6, formatString(kScoreColumnFormat, score->m_field14),
                0.0f, y, text.r, text.g, text.b, text.a, 1);
            list->appendPositionedCell0x7c8210(7, formatString(kScoreColumnFormat, score->m_field10),
                0.0f, y, text.r, text.g, text.b, text.a, 1);
            appendEmpty(list, 8);
            list->appendPositionedCell0x7c8210(9,
                formatString(kScoreColumnFormat, static_cast<int>(player->getPing())),
                0.0f, y, text.r, text.g, text.b, text.a, 1);

            // Five accumulations, each recomputing the squad index with fresh virtual calls, and
            // in an order that does not match the columns above. m_field048[6 + team] is the only
            // one that is not per-squad.
            m_field048[6 + team] += player->getPing();
            m_counters[0][team][commanderOrSquad(player)] += static_cast<int>(player->getPing());
            m_counters[1][team][commanderOrSquad(player)] += score->m_score;
            m_counters[2][team][commanderOrSquad(player)] += score->m_field14;
            m_counters[3][team][commanderOrSquad(player)] += score->m_field10;
            m_counters[4][team][commanderOrSquad(player)] += score->m_field04;
        }
    }

    // the hidden id column, then the row background - both paths reach these -------------
    list->appendCell0x7c81c0(10, formatString("%i", player->getId()),
        text.r, text.g, text.b, text.a, 0);

    if (player->getId() == m_player)
    {
        if (!m_toggleSquads) m_rowCount = m_currentRow;
        list->setRowHighlight(m_currentRow, 0.9960784316062927f, 0.7529411911964417f,
            0.08235294371843338f, 0.3499999940395355f, 0.0f, m_field028, m_rowHeight);
        ++m_currentRow;
    }
    else
    {
        const int parity = m_currentRow >= 0 ? m_currentRow % 2 : m_currentRow % 2 + 2;
        if (parity > 0)
            list->setRowHighlight0x7c4ba0(m_currentRow, 0.7254902124404907f, 0.7098039388656616f,
                0.5803921818733215f, 0.800000011920929f, 0.0f, m_field028, m_rowHeight);
        ++m_currentRow;
    }
}

// Builds the final scoreboard for the completed round.
void HudScoreboard::updateFinalList()
{
    if (!world::scoreManager) return;
    m_finalList->clearRows();
    const std::list<world::IPlayer*>& players = world::playerManager->getPlayersSortedByScore();
    int rank = 0;
    for (std::list<world::IPlayer*>::const_iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* const player = *it;
        if (player)
        {
            const meme::Color& color = scoreboardColors[player->getTeam()];
            if (player->getScoreData())
            {
                const char* const name = player->getName().c_str();
                const float y = static_cast<float>(m_field034);
                m_finalList->appendPositionedWideCell0x7c8470(0,
                    convertHudEventText(formatString("%i - %s", ++rank, name)),
                    0.0f, y, color.r, color.g, color.b, color.a, 0);
                m_finalList->appendPositionedCell0x7c8210(1,
                    formatString(kScoreColumnFormat, player->getScoreData()->m_score),
                    0.0f, y, color.r, color.g, color.b, color.a, 0);
                m_finalList->appendPositionedCell0x7c8210(2,
                    formatString(kScoreColumnFormat, player->getScoreData()->m_field04),
                    0.0f, y, color.r, color.g, color.b, color.a, 0);
                m_finalList->appendPositionedCell0x7c8210(3,
                    formatString(kScoreColumnFormat, player->getScoreData()->m_field14),
                    0.0f, y, color.r, color.g, color.b, color.a, 0);
                m_finalList->appendPositionedCell0x7c8210(4,
                    formatString(kScoreColumnFormat, player->getScoreData()->m_field10),
                    0.0f, y, color.r, color.g, color.b, color.a, 0);
                m_finalList->appendPositionedCell0x7c8210(5,
                    formatString(kScoreColumnFormat, static_cast<int>(player->getPing())),
                    0.0f, y, color.r, color.g, color.b, color.a, 0);
            }
        }
        if (rank > 2) break;
    }
}
meme::BfListBoxData* HudScoreboard::getScoreList(int side)
{ return side == 1 ? m_enemyList : m_friendlyList; }
meme::BfListBoxData* HudScoreboard::getTotalList(int side)
{ return side == 1 ? m_enemyTotalList : m_friendlyTotalList; }
int HudScoreboard::getSelectedPlayer(int side)
{
    const std::string text = (side == 1 ? m_enemyList : m_friendlyList)->getSelectedString(10);
    return std::atoi(text.c_str());
}
void HudScoreboard::setPage(int page)
{
    m_toggleScore = page == 0;
    m_toggleSquads = page == 1;
    m_toggleManage = page == 2;
}

// Handles selection of a scoreboard row.
void HudScoreboard::singleClick(int side)
{
    if (HudVoteMap* const voteMap = hudManager->getVoteMap())                 // +0x228
    {
        if (voteMap->m_selected)                                               // +0x24
            return;
    }
    if (!m_toggleManage)                                                       // +0x366
        return;
    world::IPlayer* const local = world::playerManager->getLocalHumanPlayer(); // +0x30
    if (!local)
        return;
    const int id = getSelectedPlayer(side);
    world::IPlayer* const target = world::playerManager->getPlayerFromId(id); // +0x20
    if (!target)
        return;

    const int column = (side == 1 ? m_enemyList : m_friendlyList)->m_column;  // +0x370 / +0x368
    if (column == 4)
    {
        if (!target->getVoipEnabled())                                         // PE +0x268
            return;
        if (m_mutedPlayers.size())
        {
            for (std::list<int>::const_iterator it = m_mutedPlayers.begin();
                 it != m_mutedPlayers.end(); ++it)
            {
                if (*it == id)
                    return;
            }
        }
        VoipPlayerMuteEvent* event;
        if (target->getIsMuted())                                              // PE +0x274
        {
            target->setIsMuted(false);                                         // PE +0x284
            if (id == local->getId())
                return;
            void* const storage = ::operator new(sizeof(VoipPlayerMuteEvent));
            event = storage ? new(storage) VoipPlayerMuteEvent(false, local->getId(), id) : nullptr;
        }
        else
        {
            target->setIsMuted(true);
            if (id == local->getId())
                return;
            void* const storage = ::operator new(sizeof(VoipPlayerMuteEvent));
            event = storage ? new(storage) VoipPlayerMuteEvent(true, local->getId(), id) : nullptr;
        }
        game->sendGameEvent(static_cast<unsigned int>(id), event);             // ds:99E348 +0x8C
        return;
    }
    if (column <= 6 || column > 8)
        return;

    bool canVote = true;
    if (ClientVoteManager::getInstance()->getTeamVoteOnly())
    {
        const int targetTeam = target->getTeam();                             // PE +0xE4, target first
        canVote = local->getTeam() == targetTeam;
    }
    if (!ClientVoteManager::getInstance()->votingEnabled())
        return;
    if (!canVote)
        return;
    ClientVoteManager::getInstance()->sendKickIssue(id);                       // 599690, result dropped
}
} } }
#endif
