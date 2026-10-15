// Class/path/member names [INFERRED] from HUD+138, constructor791260.
// The complete five-slot table932084 starts with IEventListener::handleEvent;
// destruction790F80 is nonvirtual. Native STL replaces the retail12C allocation.
// No factory or HUD publication is added here.
#ifndef MENU_HUDSQUAD_H
#define MENU_HUDSQUAD_H
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include "../Game/IEventListener.h"
#include <bitset>
#include <cstdint>
#include <list>
#include <string>
namespace dice { namespace meme { class System; class BfListBoxData; } }
namespace dice { namespace hfe { namespace world { class IPlayer; }
namespace hud {
class BF2HudManager;
class HudSquad : public IEventListener {
public:
    explicit HudSquad(meme::System*);
    ~HudSquad();                                        // 790F80, nonvirtual
    void handleEvent(EventCategory, unsigned int, EventNode*, void*) override;
    virtual void setSystem(meme::System*);
    virtual bool registerData();
    virtual bool registerFunctions();                   // +0C 5A5E20, actual true default
    virtual void setVisible(bool);
    bool setCreateMode(bool);
    void leaveAndReinvite();
    std::string getAvailableSquadName(int team, unsigned int index);
    void refreshCreateTag(bool resetLocked);
    // Nonvirtual action names/void returns inferred from discard-only callers.
    void joinSquad(int squad);
    void finishSquadCreate();
    int toggleSelectedSquad(int squad);                 // 7911E0, EAX consumed
    void selectSquad(int squad);
    void setSquadExpanded(int squad, bool expanded);
    void selectPlayer(int playerId);
    // [INFERRED] Toggles the selected ID, clearing it to -1 when already selected. Expansion state is unchanged.
    void togglePlayerSelection(int playerId);
    void toggleSquadSelection(int squad);               // 78F270 -> +8C
    // Inlined field views used by the map color selector (PE 7788B0).
    int getSelectedSquadForMap() const { return m_field08c; }
    int getSelectedPlayerForMap() const { return m_field090; }
    // SquadMenu console targets. [INFERRED] Method names from registered command names.
    void doubleClickNew();                              // 83D270, shipped shared RET-only body
    void doubleClickInvite();                           // 83D270, the same RET (see the .cpp)
    void singleClickNew();                              // 791CE0, the member list's click
    void squadCreateLockToggle();                       // 78F220 -> +EA
    bool setSquadNameSelect();                          // 78F190 -> hudManager 7B11E0
    void setShowInviteList(bool show);
    void applyInviteList();
    void singleClickInvite();                           // 791F50, the invite list's click
private:
    friend class BF2HudManager;
    friend class HudSquadFixture;
    void handleHudEvent(unsigned int, EventNode*, void*);
    void updateString(const char**, const char*);
    // [CHOSEN, not recovered] Shared wide-string update helper. The existing comparison occurs before the destination null test.
    void updateWideString(const wchar_t** destination, const wchar_t* source);
    void scrollToSquad();                               // 78E2C0 (E2)
    void refreshTitle();
    void refreshCounts(int team);                       // 78E330 (E2)
    void refreshRows(int team);                         // 791B20 (E2)
    // [INFERRED] Squad-row builders. Hidden column 14 identifies row kind: commander zero, squad two, create three, player four.
    void paintHeadingRow(std::string labelKey, std::string actionKey);   // 78E520, invite list
    void paintCommanderHeading(int team);                // 78E6A0, row type 0
    void paintCreateRow(int team);                       // 78ECA0, row type 3 (team never read)
    void paintSquadRow(int team, int squad, int memberCount);            // 78FC30, row type 2
    void paintPlayerRow(world::IPlayer* player, int team);               // 7902C0, row type 4
    void paintInviteRow(world::IPlayer* player, int team, int excludedSquad); // 790A90, invite list
    void fillCommanderRows(int team);
    void fillInviteRows(const std::list<world::IPlayer*>& players, int team,
                        int excludedSquad);
    void fillSquadRows(const std::list<world::IPlayer*>& members, int team, int squad);
    void rebuildInvitePlayers(const std::list<int>& selected);
    // [INFERRED] Reads the selected row's kind from column 14 and its ID from column 13.
    int getSelectedRowSquad();                          // 78F820, type 2 -> column 13, else -1
    int getSelectedRowPlayer();                         // 78F8B0, type 4 -> column 13, else -1
    void toggleSquadPrivate();                          // 78F940, the lock cell (column 8)
    void toggleSquadExpanded(int squad);                // 790EF0, the arrow cell (column 12)
    void collectInvitedPlayers();                       // 791690, seeds m_selectedPlayers
    bool toggleInvitePlayer(int playerId);              // 791940, false = no room in the squad
    bool findExcludedPlayer(int playerId, bool erase);  // 78FB50, on m_excludedPlayers
    bool findSelectedPlayer(int playerId, bool erase);  // 78FBC0, on m_selectedPlayers
    static char s_emptyString;                          // A1E368, zero-filled data
    static wchar_t s_emptyWideString;                   // A1E36C, zero-filled data
    meme::System* m_system;                             // +04 borrowed
    const char* m_title;                                // +08 owned unless sentinel
    const wchar_t* m_unassignedText;                     // +0C owned unless sentinel
    const wchar_t* m_squadCountText;                     // +10 owned unless sentinel
    std::intptr_t m_textures[14];                       // +14..48, meme texture handles
    // Lazy per-squad icon cache. Squad indices are unchecked; failed loads retry on the next paint.
    std::intptr_t m_squadIcons[10];                     // +4C..70
    std::uint32_t m_field074;                           // untouched by constructor
    // Current row count and last painted local-player row. The latter can remain stale when its squad is collapsed.
    int m_rowCount, m_localPlayerRow;                    // +78/+7C zero
    int m_playerId, m_visibleRows;                       // +80=-1, +84=21
    bool m_sortByScore;                                 // +88=false; padding untouched
    int m_field08c, m_field090;                          // selected squad/player; untouched by ctor
    int m_squadId, m_team;                              // +94=0/+98=2
    bool m_isSquadLeader, m_isCommander;                 // +9C/+9D false
    bool m_showCommander, m_field09f;                    // +9E/+9F true
    bool m_visible, m_inviteVisible, m_isInSquad;         // +A0..A2 false
    std::bitset<16> m_expanded;                          // +A4; ctor resets indices0..9
    // Row-highlight geometry begins at +A8 with a float initialized to zero.
    float m_field0a8;
    float m_field0ac, m_field0b0, m_field0b4;             // +AC=19,+B0=2,+B4=0
    int m_columns[12];                                  // +B8..E4 initialized after registration
    bool m_createVisible, m_renameVisible, m_createLocked; // +E8..EA false
    const char* m_createTag;                            // +EC owned unless sentinel
    const char* m_newName;                              // +F0 owned unless sentinel
    std::list<world::IPlayer*> m_invitePlayers;           // +F4, borrowed;791740
    std::list<int> m_selectedPlayers;                    // +100 player ids;791690
    std::list<int> m_excludedPlayers;                    // +10C player ids;791940
    std::list<int> m_eventPlayers;                       // +118 player ids;791A90
    meme::BfListBoxData* m_memberList;                   // +124 untouched by ctor, not deleted
    meme::BfListBoxData* m_inviteList;                    // +128 untouched by ctor, not deleted
};
} } }
#endif
#endif
