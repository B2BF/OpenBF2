// [INFERRED] owner/name/path from BF2HudManager+120 and scoreboard list names.
// Complete three-slot primary and one-slot event surface, BF2.exe933DD4/933DD0.
// Native storage/lifetime, not a retail STL overlay.
#ifndef MENU_HUDSCOREBOARD_H
#define MENU_HUDSCOREBOARD_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Game/IEventListener.h"
#include <cstdint>
#include <list>
namespace dice { namespace meme { class System; class BfListBoxData; } }
namespace dice { namespace hfe { namespace world { class IPlayer; } } }  // paintRow's subject
namespace dice { namespace hfe { namespace hud {
class BF2HudManager;
class IHudScoreboard {
public:
    virtual void setSystem(meme::System*) = 0;
    virtual bool registerData() = 0;
    virtual bool registerFunctions() = 0;
};
class HudScoreboard : public IHudScoreboard, public IEventListener {
public:
    explicit HudScoreboard(meme::System*);
    ~HudScoreboard(); // 7A4750, nonvirtual; list-data and texture handles not released
    void setSystem(meme::System*) override;
    bool registerData() override;
    bool registerFunctions() override;
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    meme::BfListBoxData* getScoreList(int side); // 7A2B20, side1 selects enemy
    meme::BfListBoxData* getTotalList(int side); // 7A2B40, side1 selects enemy
    int getSelectedPlayer(int side); // 7A2AC0, selected hidden-column10 -> atoi
    // Refreshes the visible scoreboard rows from the current game state.
    void update(bool visible);
    // Refills the final list with the top three players by score. Does nothing without a score manager.
    void updateFinalList();
    // Writes player's scoreboard row into list, including team styling, voice, votes and counters.
    void paintRow(meme::BfListBoxData* list, world::IPlayer* player, int team);

    // Returns the team total over populated squads plus the original commander counter cell.
    // All four totals use the same commander cell, preserving the original behavior.
    int getTotalScore(int team);      // 7A2900, sums m_counters[1]
    int getTotalField04(int team);    // 7A2970, sums m_counters[4]
    int getTotalField14(int team);    // 7A29E0, sums m_counters[2]
    int getTotalField10(int team);    // 7A2A50, sums m_counters[3]
    // The header and total rows. paintTotalRow doubles as the team-total row (squad 11) and the
    // per-squad total row, and consumes the counters it prints - it zeroes them on the way out.
    void paintTotalRow(meme::BfListBoxData* list, int team, int squad);
    void paintCommanderHeader(meme::BfListBoxData* list, int team);
    void paintSquadHeader(meme::BfListBoxData* list, int team, int squad);
    // The three fills, and the side dispatcher that chooses between them.
    void fillCommanderRow(meme::BfListBoxData* list, int team);
    void fillSquadRows(meme::BfListBoxData* list, const std::list<world::IPlayer*>& players,
                       int team, int squad);
    void fillFlatRows(meme::BfListBoxData* list, const std::list<world::IPlayer*>& players,
                      int team);
    void fillSideList(meme::BfListBoxData* list, int team);
    void updateScrollPosition();
    void setPage(int page); // 7A2820; inferred name, signed word (not bool)
    // Handles a click on the selected side of the Manage page.
    // Column four toggles VoIP mute; columns seven and eight request a kick vote.
    void singleClick(int side);
private:
    friend class BF2HudManager;
    friend struct HudScoreboardFixture;
    void handleHudEvent(unsigned int, EventNode*, void*);
    void resetCounters(int team); // 7A28B0, no bounds guard

    meme::System* m_system; // +08 borrowed
    int m_rowCount, m_visibleRows, m_squad, m_player; // +0C..18:0,21,0,-1
    int m_team, m_selectedPlayer; // +1C/+20:2,-1
    // Local player roles used to choose scoreboard grouping and styling.
    bool m_isSquadLeader, m_isCommander; // +24/+25 false
    std::uint32_t m_field028; // initialized0; interpretation unresolved
    float m_rowHeight, m_rowOffset; // +2C/+30:19,2
    int m_field034, m_currentRow; // +34/+38 zero
    bool m_visible; // +3C false
    std::uint32_t m_field040, m_field044; // zero
    std::uint32_t m_field048[9]; // +48..68 untouched scalar storage; meanings unresolved
    int m_counters[5][3][11]; // +6C..2FF, five integer totals per team/squad
    int m_columns[9]; // +300..320, initialized after event registration
    std::intptr_t m_textures[13]; // +324..354, meme texture handles; initialized after event registration
    std::list<int> m_mutedPlayers; // retail+358, signed player ids, duplicate entries allowed
    bool m_toggleSquads, m_toggleScore, m_toggleManage, m_teamVoteOnly; // +364..367
    meme::BfListBoxData* m_friendlyList; // +368, all five untouched by ctor and not deleted
    meme::BfListBoxData* m_friendlyTotalList; // +36C
    meme::BfListBoxData* m_enemyList; // +370
    meme::BfListBoxData* m_enemyTotalList; // +374
    meme::BfListBoxData* m_finalList; // +378, allocation ends37C
};
} } }
#endif
#endif
