// Engine host interface used by the front-end menu plugin. Preserve virtual declaration order.
#ifndef MENU_ISWIFFHOST_H
#define MENU_ISWIFFHOST_H
#include "../Core/IBase.h"
#include <stdint.h>
#include "../Game/IModInfo.h"
#include "../Game/Persistence/PlayerAwards.h"      // getCachedAwards (+0x228): IPlayerAwards::AwardList
#include "../Game/Persistence/PlayerRankInfo.h"
#include "../Game/Persistence/PlayerUnlocks.h"
#include "../Game/GameLogic/TicketHistory.h"        // SwiffEndOfRoundSummary::ticketHistory (+0x1A4)
#include <ctime>
#include <string>
#include <vector>
#if !DEDICATED
namespace dice { namespace hfe {

// The menu event record the host queues and BF2Engine::renderMenu (0x40BD10) drains. Layout from
// its clear routine (0x408510): an int, then a std::vector<std::string> read as args[0]/args[1].
struct MenuEvent
{
    int                       type;
    std::vector<std::string>  args;
    MenuEvent() : type(0) {}
};

// Player statistics returned by the profile service and cached by the menu host.
// STL-containing records require explicit conversion when crossing the retail plugin ABI.
struct SwiffPlayerStatsWeapon
{
    std::string time;        // +0   formatDuration(wtm-N)
    int         kills;       // +28  wkl-N
    int         deaths;      // +32  wdt-N
    int         accuracy;    // +36  wac-N
    std::string kd;          // +40  wkd-N
    SwiffPlayerStatsWeapon() : kills(0), deaths(0), accuracy(0) {}
};
struct SwiffPlayerStatsArmy
{
    std::string time;        // +0   formatDuration(atm-N)
    int         wins;        // +28  awn-N
    int         losses;      // +32  alo-N
    int         bestRound;   // +36  abr-N
    SwiffPlayerStatsArmy() : wins(0), losses(0), bestRound(0) {}
};
struct SwiffPlayerStatsKit
{
    std::string time;        // +0   formatDuration(ktm-N)
    int         kills;       // +28  kkl-N
    int         deaths;      // +32  kdt-N
    std::string kd;          // +36  kkd-N
    SwiffPlayerStatsKit() : kills(0), deaths(0) {}
};
struct SwiffPlayerStatsVehicle
{
    std::string time;        // +0   formatDuration(vtm-N)
    int         kills;       // +28  vkl-N
    int         deaths;      // +32  vdt-N
    std::string kd;          // +36  vkd-N
    int         roadKills;   // +64  vkr-N
    SwiffPlayerStatsVehicle() : kills(0), deaths(0), roadKills(0) {}
};
struct SwiffPlayerStatsMapRow   // the 40-byte row of the record's vector
{
    int         id;          // +0   the map id
    int         wins;        // +4   mwn-<id>
    int         losses;      // +8   mls-<id>
    std::string time;        // +12  formatDuration(mtm-<id>), "-" when zero
    SwiffPlayerStatsMapRow() : id(0), wins(0), losses(0) {}
};
struct SwiffPlayerStatsRecord
{
    int         playerId;                     // +0
    std::string nick;                         // +4
    int         scor, wins, loss;             // +32 +36 +40
    std::string jond, lbtl;                   // +44 +72   dates (0x4877A0: formatDate, or "-")
    int         twsc, cpcp, cacp, dfcp, kila, heal, rviv, rsup, rpar, tgte;   // +100..+136
    int         dkas_dsab;                    // +140  dkas + dsab
    int         cdsc, rank;                   // +156 +160
    int         cmsc, kick, ban, kill, deth, suic;   // +168..+188
    std::string ospm, klpm, dtpm, klpr, dtpr; // +192..+304  ratios, the wire text
    int         bksk, wdsk, bbrs;             // +332..+340
    std::string tcdr, tsql, tsqm, tlwf;       // +344..+428  durations
    std::string mvns;                         // +456
    int         mvks, mvrs;                   // +484 +488
    std::string vmns;                         // +492
    int         vmks, vmrs;                   // +520 +524
    SwiffPlayerStatsWeapon  weapons[19];      // +528,  68 bytes each
    SwiffPlayerStatsArmy    armies[10];       // +1820, 40 bytes each
    int         osaa;                         // +2220
    SwiffPlayerStatsKit     kits[7];          // +2224, 64 bytes each
    int         vrk;                          // +2672
    SwiffPlayerStatsVehicle vehicles[13];     // +2676, 68 bytes each
    int         fkit; std::string fkitTime;   // +3560 +3564
    int         fveh; std::string fvehTime;   // +3592 +3596
    int         fwea; std::string fweaTime;   // +3624 +3628
    int         fmap; std::string fmapTime;   // +3656 +3660
    std::vector<SwiffPlayerStatsMapRow> maps; // +3688
    time_t      fetchedAt;                    // +3704
    int         de6, de7, de8;                // +3708..+3716
    SwiffPlayerStatsRecord()
        : playerId(0), scor(0), wins(0), loss(0),
          twsc(0), cpcp(0), cacp(0), dfcp(0), kila(0), heal(0), rviv(0), rsup(0), rpar(0), tgte(0),
          dkas_dsab(0), cdsc(0), rank(0), cmsc(0), kick(0), ban(0), kill(0), deth(0), suic(0),
          bksk(0), wdsk(0), bbrs(0), mvks(0), mvrs(0), vmks(0), vmrs(0), osaa(0), vrk(0),
          fkit(0), fveh(0), fwea(0), fmap(0), fetchedAt(0), de6(0), de7(0), de8(0)
    {
        time(&fetchedAt);
    }
};

// Leaderboard cache shared by global and per-player board views.
// Request order differs from storage order for teamwork/combat and vehicle/weapon groups.
// Timestamps are set only at construction, so every read refetches after the first hour.
// The service key spelling "detahsby" is preserved; absent values become zero.
// [CHOSEN] The last-fetch position starts at zero instead of remaining uninitialized.
struct SwiffLeaderboardScoreRow
{
    int         n;                        // +0   "n"
    int         pid;                      // +4   "pid"
    std::string nick;                     // +8   "nick"
    int         values[2];                // +36  see above
    int         playerRank;               // +44  "playerrank"
    std::string countryCode;              // +48  "countrycode"
};
struct SwiffLeaderboardRisingStarRow     // 128 B (constructor 0x486830, destructor 0x487A60)
{
    int         n;                        // +0   "n"
    int         pid;                      // +4   "pid"
    std::string nick;                     // +8   "nick"
    std::string weeklyScore;              // +36  "weeklyscore" read as a float, "%.2f" then "%"
    int         totalTime;                // +64  "totaltime"
    std::string date;                     // +68  "date", the wire text
    int         playerRank;               // +96  "playerrank"
    std::string countryCode;              // +100 "countrycode"
};
struct SwiffLeaderboardRow               // 80 B: combat, kits, vehicles
{
    int         n;                        // +0
    int         pid;                      // +4
    std::string nick;                     // +8
    int         values[3];                // +36  see above
    int         playerRank;               // +48
    std::string countryCode;              // +52
};
struct SwiffLeaderboardWeaponRow
{
    int         n;                        // +0
    int         pid;                      // +4
    std::string nick;                     // +8
    int         values[4];                // +36  see above
    int         playerRank;               // +52
    std::string countryCode;              // +56
};
template <class Row>
struct SwiffLeaderboardGroup
{
    int              numPlayers;          // +0   LeaderBoard::getNumPlayers, the board's size
    std::string      updated;             // +4   LeaderBoard::getUpdated through 0x487DC0 ("%d-%s-%.2d %.2d:%.2d", or "-")
    time_t           createdAt;           // +32  stamped by the constructor only (see above)
    std::vector<Row> rows;                // +36
    SwiffLeaderboardGroup() : numPlayers(0), createdAt(0) { time(&createdAt); }
};
struct SwiffLeaderboardCache
{
    SwiffLeaderboardGroup<SwiffLeaderboardScoreRow>      overall;       // group 0     request 0
    SwiffLeaderboardGroup<SwiffLeaderboardRisingStarRow> risingStar;    // group 1     request 1
    SwiffLeaderboardGroup<SwiffLeaderboardScoreRow>      commander;     // group 2     request 2
    SwiffLeaderboardGroup<SwiffLeaderboardRow>           combat;        // group 3     request 4
    SwiffLeaderboardGroup<SwiffLeaderboardScoreRow>      teamwork;      // group 4     request 3
    SwiffLeaderboardGroup<SwiffLeaderboardRow>           kits[7];       // 5..11       requests 5..11
    SwiffLeaderboardGroup<SwiffLeaderboardWeaponRow>     weapons[12];   // 12..23      requests 19..30
    SwiffLeaderboardGroup<SwiffLeaderboardRow>           vehicles[7];   // 24..30      requests 12..18
    int                                                  position;      // +1612
    SwiffLeaderboardCache() : position(0) {}
};

struct SwiffHostMessage
{
    int         type;
    int         code;
    std::string text;
    SwiffHostMessage() : type(0), code(0) {}
    SwiffHostMessage(int aType, const std::string& aText, int aCode) : type(aType), code(aCode), text(aText) {}
};

// One row of getSinglePlayerLevels (0x496270): index, then name / map / mode (88 B in the PE).
struct SwiffSinglePlayerLevel
{
    int         index;
    std::string name;
    std::string map;
    std::string mode;
    SwiffSinglePlayerLevel() : index(0) {}
};

// Create-server map-list entry containing level, game mode and player size. [INFERRED] Member names.
struct SwiffServerEntry
{
    std::string map;        // +0   the level directory ("dalian_plant")
    std::string gameMode;   // +28  "gpm_cq", or an "sp1".."sp3" layer
    int         size;       // +56  the map size (16 / 32 / 64)
    SwiffServerEntry() : size(0) {}
};

// The 32-byte entry of the +0xEC8 vector (addEntryEC8 0x4976C0): a string and an int.
// [UNRESOLVED] roles.
struct SwiffStringIntEntry
{
    std::string text;
    int         value;
    SwiffStringIntEntry() : value(0) {}
};

// The rank record of the +0xE9C map (node 0x3C bytes: three ints and a name), filled by addRank
// (0x494030) from menu/external/flashmenu/ranks.con and read by getRankName/getRankB/getRankC.
struct SwiffRankRecord
{
    int         a;
    int         b;
    int         c;
    std::string name;
    SwiffRankRecord() : a(0), b(0), c(0) {}
};

// Player row shared with the end-of-round menu. Convert STL fields explicitly when crossing compiler ABIs.
struct SwiffEndOfRoundPlayer
{
    std::string name;            // +0x00  IPlayer::getName
    int         kills;           // +0x1C  PlayerScoreData +0x14
    int         deaths;          // +0x20  PlayerScoreData +0x10
    int         score;           // +0x24  PlayerScoreData +0x00
    int         teamScore;       // +0x28  PlayerScoreData +0x04 ("rplScore")
    int         rank;            // +0x2C  PlayerScoreData +0x20, raw; the squad rows use the -1 -> 0 lookup
    int         cmdScore;        // +0x30  PlayerScoreData +0x0C
    bool        isSquadMate;     // +0x34  same non-zero squad and same team as the local player
    bool        isCommander;     // +0x35  IPlayer::getIsCommander (PE +0x110)
    bool        isLocalPlayer;   // +0x36  the player is PlayerManager::getLocalHumanPlayer
    SwiffEndOfRoundPlayer()
        : kills(0), deaths(0), score(0), teamScore(0), rank(0), cmdScore(0),
          isSquadMate(false), isCommander(false), isLocalPlayer(false) {}
};

// 0x98 bytes, two at +0x40 (team 1, team 2).
struct SwiffEndOfRoundTeam
{
    std::string                        name;            // +0x00  gameLogic->getTeamName(team) ("US", "MEC", "CH", ...)
    std::vector<SwiffEndOfRoundPlayer> players;         // +0x1C  every team member, PlayerManager's score order
    int                                bestSquadScore;  // +0x2C  the summed score of the best squad
    std::string                        bestSquadName;   // +0x30  ISquadInfo::getSquadName of that squad
    SwiffEndOfRoundPlayer              squadLeader;     // +0x4C  its leader: name, rank (+0x78), score (+0x70)
    std::vector<SwiffEndOfRoundPlayer> squadMembers;    // +0x84  its other members: name, rank, score
    int                                roundsWon;       // +0x94  TeamScore +0x88 (m_roundsWon) - unwritten by 0x497980
    SwiffEndOfRoundTeam() : bestSquadScore(0), roundsWon(0) {}
};

// Top-player award row. Its score field represents skill score rather than the team row's total score.
struct SwiffEndOfRoundTopPlayer
{
    std::string      name;               // +0x00  np<i>
    int              kills;              // +0x1C  unwritten (0)
    int              deaths;             // +0x20  unwritten (0)
    int              skillScore;         // +0x24  ssp<i>  - published as EOR_combatscore
    int              teamScore;          // +0x28  tsp<i>  - published as EOR_teamworkscore
    int              rank;               // +0x2C  the -1 -> 0 lookup of np<i> (0x488160)
    int              cmdScore;           // +0x30  csp<i>  - published as EOR_commanderscore
    bool             isSquadMate;        // +0x34  unwritten (false)
    bool             isCommander;        // +0x35  unwritten (false)
    bool             isLocalPlayer;      // +0x36  isLocalPlayerName(np<i>) (+0x2E8)
    int              favouriteKit;       // +0x38  -1 (ctor), else the kit with the longest tk<k>p<i>
    int              favouriteVehicle;   // +0x3C  -1 (ctor), else the vehicle with the longest tv<v>p<i>
    std::vector<int> kits;               // +0x40  every kit index with a non-zero tk<k>p<i>
    float            accuracy;           // +0x50  bhp / bfp * 100 (0 when bfp is 0) - EOR_overallaccuracy
    int              bulletsFired;       // +0x54  bfp<i>
    int              bulletsHit;         // +0x58  bhp<i>
    std::string      team;               // +0x5C  the team name, when a team row carries the same name
    SwiffEndOfRoundTopPlayer()
        : kills(0), deaths(0), skillScore(0), teamScore(0), rank(0), cmdScore(0),
          isSquadMate(false), isCommander(false), isLocalPlayer(false),
          favouriteKit(-1), favouriteVehicle(-1), accuracy(0.0f), bulletsFired(0), bulletsHit(0) {}
};

// 0x24 bytes: a kit's (seven at +0x2D8, sk<k>/skn<k>) or vehicle's (seven at +0x3D4, sv<v>/svn<v>)
// best player, published by the "hscoreKits" / "hscoreVehicles" lists.
struct SwiffEndOfRoundHighScore
{
    std::string name;            // +0x00  skn<k> / svn<v> (assigned through c_str())
    int         rank;            // +0x1C  the -1 -> 0 lookup of the name
    int         score;           // +0x20  sk<k> / sv<v>
    SwiffEndOfRoundHighScore() : rank(0), score(0) {}
};

// One category award published to the end-of-round menu.
struct SwiffEndOfRoundAward
{
    int         score;           // +0x00  ts / ss / cpc / ...
    std::string name;            // +0x04  tsn / ssn / cpcn / ...
    int         rank;            // +0x20  the -1 -> 0 lookup of the name
    SwiffEndOfRoundAward() : score(0), rank(0) {}
};

struct SwiffEndOfRoundInfo                        // 0x664 = 1636 bytes
{
    std::string              mapName;             // +0x000  getCurrentMapName (EndOfRound.getPreviousMapPath)
    std::string              gameMode;            // +0x01C  getCurrentMapGameMode
    int                      maxPlayers;          // +0x038  getCurrentMapMaxPlayers - unwritten by 0x497980
    int                      mapSize;             // +0x03C  getCurrentMapSize       - unwritten by 0x497980
    SwiffEndOfRoundTeam      teams[2];            // +0x040
    SwiffEndOfRoundTopPlayer topPlayers[3];       // +0x170
    SwiffEndOfRoundHighScore kitLeaders[7];       // +0x2D8
    SwiffEndOfRoundHighScore vehicleLeaders[7];   // +0x3D4
    SwiffEndOfRoundAward     awards[11];          // +0x4D0
    int                      winner;              // +0x65C  (unsigned char) scoreManager->getWinner() - unwritten by 0x497980
    int                      victoryType;         // +0x660  (unsigned char) scoreManager->getVictoryType() - unwritten by 0x497980
    // The ints the constructor leaves unwritten are zeroed here because C++ must give them a value;
    // the assignment copies them exactly as 0x498EA0 does.
    SwiffEndOfRoundInfo() : maxPlayers(0), mapSize(0), winner(0), victoryType(0) {}
};

// End-of-round summary player row. [UNRESOLVED] Roles of the always-empty second string and zero trailing integer.
struct SwiffEndOfRoundSummaryPlayer
{
    std::string name;            // +0x00
    std::string text1C;          // +0x1C  always "" [UNRESOLVED]
    int         score;           // +0x38
    int         teamScore;       // +0x3C
    int         kills;           // +0x40
    int         deaths;          // +0x44
    int         field48;         // +0x48  always 0 [UNRESOLVED]
    SwiffEndOfRoundSummaryPlayer() : score(0), teamScore(0), kills(0), deaths(0), field48(0) {}
};
struct SwiffEndOfRoundSummaryTeam                 // 0x30 bytes
{
    std::string                               name;       // +0x00
    std::vector<SwiffEndOfRoundSummaryPlayer> players;    // +0x1C
    int                                       roundsWon;  // +0x2C  uninitialised by 0x10012290
    SwiffEndOfRoundSummaryTeam() : roundsWon(0) {}
};
struct SwiffEndOfRoundSummary                     // 0x74 bytes
{
    SwiffEndOfRoundSummaryTeam teams[2];          // +0x00
    int                        winner;            // +0x60  uninitialised by 0x10012290
    TicketHistory              ticketHistory;     // +0x64  gameLogic->getTicketHistory(), 132-byte Tickets
    SwiffEndOfRoundSummary() : winner(0) {}
};

namespace ui {

struct IServerBrowser;   // Menu/IServerBrowser.h - the named GameSpy list getServerList answers

struct ISwiffHost : public IBase
{
    // initialisation and the per-frame calls ------------------------------------------------
    virtual bool init() = 0;                                                        //   4 +0x010  0x4868E0  the ServerBrowser lists, once
    virtual bool loadCredits() = 0;                                                 //   5 +0x014  0x486AC0  credits.con, once
    virtual bool recreateControlMap(const std::string& name) = 0;                   // +0x018
    virtual bool deleteControlMap(const std::string& name) = 0;                     // +0x01C
    virtual void update() = 0;                                                      //   8 +0x020  0x486C60  SwiffPlayer::update
    virtual void onMenuActivated() = 0;                                             //   9 +0x024  nullsub_3  BF2Engine::activateMenu
    virtual bool pollEvent(MenuEvent& out) = 0;                                     //  10 +0x028  0x49CAF0  renderMenu
    virtual int  pushEvent(MenuEvent& event) = 0;                                   //  11 +0x02C  0x49D210  the as "request" (dedupes the last)
    // the lazily created persistence objects and the singletons --------------------------------
    virtual IBase* getPlayerStats() = 0;                                            //  12 +0x030  0x4872C0  dice.hfe.PlayerStats
    virtual IBase* getPlayerAwards() = 0;                                           //  13 +0x034  0x487300  dice.hfe.PlayerAwards + awards.con
    virtual IBase* getPlayerUnlocks() = 0;                                          // +0x038
    virtual IBase* getSearchForPlayer() = 0;                                        // +0x03C
    virtual IBase* getLeaderBoard() = 0;                                            // +0x040
    virtual IBase* getDemoLibrary() = 0;                                            //  17 +0x044  0x4871E0  singleton + refresh
    virtual IBase* getPlayerUnlockInfo() = 0;                                       //  18 +0x048  0x487270  + init
    virtual IBase* getServerLogoManager() = 0;                                      //  19 +0x04C  0x487540  singleton + its +0x10
    virtual IBase* getMenuTeamManager() = 0;                                        //  20 +0x050  0x4875A0  singleton + init
    virtual IBase* getPlayerRankInfo() = 0;                                         // +0x054
    // the running game --------------------------------------------------------------------------
    virtual std::string getCurrentMapName() = 0;                                    //  22 +0x058  0x4875F0  "$noCurrentGame" without a game
    virtual std::string getCurrentMapGameMode() = 0;                                // +0x05C
    virtual int  getCurrentMapMaxPlayers() = 0;                                     // +0x060
    virtual int  getCurrentMapSize() = 0;                                           // +0x064
// [CHOSEN, not recovered] x64 (https://github.com/B2BF/OpenBF2/issues/7): the render-capability
// chain (host +0x68/+0x70 -> player +0x60/+0x64 -> render slots 49/50) carries the address of a
// SwiffPlayer string, so it is intptr_t - int on x86, where signatures and mangling are unchanged.
    virtual bool setPlayerRenderCapability0(intptr_t value) = 0;                         //  26 +0x068  0x486E10  SwiffPlayer +0x60, then window +0x3B = 0
    virtual void* getCursor(int which) = 0;                                         //  27 +0x06C  0x488960  hcursor: 0 arrow, 1 busy, 2 keybinding
    virtual bool setPlayerRenderCapability1(intptr_t value) = 0;                         //  28 +0x070  0x486E80  SwiffPlayer +0x64
    virtual void setFlag28(bool value) = 0;                                         //  29 +0x074  0x5DF3C0 (ICF)  [UNRESOLVED] role
    virtual bool getFlag28() = 0;                                                   //  30 +0x078  0x659F70 (ICF)
    virtual IServerBrowser* getServerList(const std::string& name) = 0;             //  31 +0x07C  0x486D10  ServerBrowserManager +0x14
    virtual bool getPlayerName(std::string& out, const std::string& fallback) = 0;  //  32 +0x080  0x486D80  GSPlayerName
    virtual bool setPlayerName(const std::string& name) = 0;                        // +0x084
    virtual bool getCommandLineFile(std::string& out) = 0;                          //  34 +0x088  0x48A110  CommandLine.cmd
    virtual bool setCommandLineFile(const std::string& commandLine) = 0;            //  35 +0x08C  0x493B90  event 5 + the file
    virtual void setLoadingProgress(float value) = 0;                               //  36 +0x090  0x4B7200 (ICF)
    virtual float getLoadingProgress() = 0;                                         // +0x094
    virtual int  getGameStatus() = 0;                                               //  38 +0x098  0x487010  1 without a loaded level
    virtual float getTimeUntilRestartMap() = 0;                                     // +0x09C
    virtual std::string getLoadingRecordString() = 0;                               //  40 +0x0A0  0x487060  LoadStat::m_string0C
    virtual bool getSpStartupMap(std::string& out) = 0;                             //  41 +0x0A4  0x4870A0  Mod::spStartupMap, "gulf_of_oman"
    virtual bool getLocalizedString(std::wstring& out, const std::string& key) = 0; //  42 +0x0A8  0x486CD0  the lexicon
    virtual bool requestEvent1() = 0;                                               //  43 +0x0AC  0x4931B0  event 1 (renderMenu ignores it)
    virtual bool requestQuit() = 0;                                                 //  44 +0x0B0  0x491800  event 2
    virtual bool requestShutdownGame() = 0;                                         //  45 +0x0B4  0x491860  event 17
    virtual bool isGameRunning(bool* out) = 0;                                      //  46 +0x0B8  0x486C90  dice.bf2.Logic.isGameRunning
    virtual int  getMessageId() = 0;                                                // +0x0BC / +0x700
    virtual void setMessageId(int id) = 0;                                          //  48 +0x0C0  0x487A50  startGame's join errors, restartMenu (20)
    virtual std::string getFrameLabel() = 0;                                        //  49 +0x0C4  0x487C40  dice.bf2.Logic.getFrameLabel
    virtual void setFrameLabel(const std::string& label) = 0;                       // +0x0C8
    virtual bool getFlag7B8() = 0;                                                  //  51 +0x0CC  0x488120  [UNRESOLVED] role
    virtual void setFlag7B8(bool value) = 0;                                        // +0x0D0
    virtual bool getGameStarting() = 0;                                             //  53 +0x0D4  0x488140  [INFERRED] role: startGame true, loading screen off false
    virtual void setGameStarting(bool value) = 0;                                   // +0x0D8
    virtual std::string getJoinAddressSetting() = 0;                                //  55 +0x0DC  0x488270  GSJoinAddress
    virtual std::string getPasswordSetting() = 0;                                   //  56 +0x0E0  0x488320  GSPassword
    virtual int  getPortSetting() = 0;                                              //  57 +0x0E4  0x4882D0  GSPort, 16567
    virtual bool requestHostServer(const SwiffSinglePlayerLevel* level) = 0;        //  58 +0x0E8  0x493B00  event 7 (map, mode); Singleplay.chooseMission's row ()
    virtual bool getSinglePlayerLevels(std::vector<SwiffSinglePlayerLevel>& out) = 0;   //  59 +0x0EC  0x496270  the eight Wetlands/Dam/Divided City rows
    virtual bool getMapListCount(int* out) = 0;                                     // Writes the map-list entry count.
    virtual bool clearServerEntries() = 0;                                          //  61 +0x0F4  0x493270  the +0xEF4 vector
    virtual bool setServerEntries(const std::vector<SwiffServerEntry>& entries) = 0;   // +0x0F8
    virtual bool getServerEntries(std::vector<SwiffServerEntry>& out) = 0;          // +0x0FC
    virtual bool joinServer(const std::string& address, int port, int queryPort) = 0;   //  64 +0x100  0x4932C0  the join server button: port is the game port, 0 = look it up by querying queryPort
    virtual bool hostServer() = 0;                                                  // +0x104
    virtual bool isGameSpyAvailable() = 0;                                          //  66 +0x108  0x486F30  the blocking GameSpy availability poll
    virtual void setSelectedServer(const std::string& address, int queryPort, int hostPort) = 0;   //  67 +0x10C  0x486F60  the browser's selected row (Multiplay.setCurrentServerId); file statics 0x9A6A04/0x9A6708/0x9A670C
    virtual void getSelectedServer(std::string& address, int* queryPort, int* hostPort) = 0;       // +0x110
    virtual int  getIndexedInt(int index) = 0;                                      //  69 +0x114  0x486FC0  +0x1C[index]
    virtual void setIndexedInt(int index, int value) = 0;                           // +0x118
    virtual bool getServerSettingsInfo(std::string& interfaceIP, int* queryPort, int* port) = 0;   //  71 +0x11C  0x487180  ServerSettings PE +332/+360/+152 ()
    virtual IBase* getModInfo() = 0;                                                //  72 +0x120  0x4876B0  singleton
    virtual std::string getModDirectorySetting() = 0;                               //  73 +0x124  0x487710  GSModDirectory, "mods/bf2"
    virtual bool addServerEntry(const std::string& map, const std::string& gameMode, int size) = 0;   // +0x128
    virtual bool removeServerEntry(unsigned int index) = 0;                         // +0x12C
    virtual bool moveServerEntry(unsigned int index, bool up) = 0;                  // +0x130
    virtual std::string getServerListKind() = 0;                                    // +0x134 / +0x58
    virtual void setServerListKind(const std::string& kind) = 0;                    // +0x138
    // Leaderboard selection: category, board within that category and global starting position. [INFERRED] Names.
    virtual int  getLeaderboardCategory() = 0;                                      // +0x13C / +0x774
    virtual void setLeaderboardCategory(int category) = 0;                          // +0x140
    virtual int  getLeaderboardId() = 0;                                            // +0x144 / +0x778
    virtual void setLeaderboardId(int id) = 0;                                      // +0x148
    virtual int  getLeaderboardPosition() = 0;                                      // +0x14C / +0x77C
    virtual void setLeaderboardPosition(int position) = 0;                          //  84 +0x150  0x487D40  clamped to >= 1
    virtual void goLeaderboardPlayer(int playerId) = 0;                             //  85 +0x154  0x496DF0  the player's page of the selected board
    virtual int  getLeaderboardPlayerPosition(int playerId, int request) = 0;       //  86 +0x158  0x48CC00  the cached row 0's "n", or 0
    virtual void setIntE98(int value) = 0;                                          //  87 +0x15C  0x487D70  [UNRESOLVED] role
    virtual int  getIntE98() = 0;                                                   // +0x160
    virtual std::string getGameMode() = 0;                                          //  89 +0x164  0x487D90  +0x780, "gpm_cq"
    virtual void setGameMode(const std::string& mode) = 0;                          // +0x168
    virtual bool playNow() = 0;                                                     // +0x16C
    // Parses round results, adds live teams and schedules the between-rounds menu after delay. Returns true.
    virtual bool setEndOfRoundInfo(const std::string& data, float delay) = 0;       // +0x170
    virtual int  resetEndOfRoundInfo() = 0;                                         // +0x174
    virtual SwiffEndOfRoundInfo* getEndOfRoundInfo() = 0;                           // +0x178 / +0x7BC
    virtual std::string getTeamName(int team) = 0;                                  //  95 +0x17C  0x4881F0  gameLogic team + 1
    virtual std::string getJoinPassword() = 0;                                      //  96 +0x180  0x488240  +0xE20  joinServer
    virtual void setJoinPassword(const std::string& password) = 0;                  //  97 +0x184  0x488260  startGame: "" after a join
    virtual bool playSound(const std::string& name) = 0;                            //  98 +0x188  0x493C70  event 6 (dice.bf2.Sound.playSound)
    virtual void slot18C(int a) = 0;                                                //  99 +0x18C  0x488920  ds:0xA23DC0 +0xB4 [UNRESOLVED]
    virtual bool getModCount(int* out) = 0;                                         // 100 +0x190  0x48A260  ModInfo list size
    virtual bool slot194(int index, int b) = 0;                                     // 101 +0x194  0x4A2B00  0x481490 on the indexed Mod [UNRESOLVED]
    virtual bool requestActivateMod(const std::string* mod) = 0;                    // 102 +0x198  0x493CE0  event 11; null -> "no mod specified"
    virtual bool requestBrowseModHomepage(const IModInfo::Mod* mod) = 0;            // 103 +0x19C  0x493E00  event 10 with the mod's url
    virtual bool getVersionString(std::string& out) = 0;                            // 104 +0x1A0  0x486D60  "1.5.3153-802.<hash>"
    virtual bool getEndOfRoundSummary(SwiffEndOfRoundSummary& out) = 0;             // 105 +0x1A4  0x499380  [INFERRED] name: team rows, rounds won, winner, ticket history
    virtual bool isGameLogicServer() = 0;                                           // 106 +0x1A8  0x4871C0  gameLogic->isServer
    virtual bool fetchPlayerStats(int playerId) = 0;                                // 107 +0x1AC  0x49D450  three getplayerinfo.aspx queries into a SwiffPlayerStatsRecord, cached
    virtual bool getPlayerStatsRecord(int playerId, SwiffPlayerStatsRecord& out) = 0;   // 108 +0x1B0  0x496AB0  the hour-long record cache in front of it
    virtual std::string getRankName(int id) = 0;                                    // 109 +0x1B4  0x48CB20  "-" when unknown
    virtual int  getRankB(int id) = 0;                                              // +0x1B8
    virtual int  getRankC(int id) = 0;                                              // +0x1BC
    virtual int  getRankCount() = 0;                                                // +0x1C0
    virtual bool getUnlocks(int playerId, IPlayerUnlocks::UnlockCache* out) = 0;    // +0x1C4
    virtual bool fetchUnlocks(int playerId) = 0;                                    // 114 +0x1C8  0x4879F0  PlayerUnlocks PE +0x10
    // Returns cached global or player leaderboards, fetching the requested group when stale.
    // request uses SwiffLeaderboardCache group order. [INFERRED] Method names.
    virtual bool getLeaderboard(SwiffLeaderboardCache* out, int request, int position) = 0;        // +0x1CC
    virtual bool getLeaderboardPlayer(int playerId, SwiffLeaderboardCache* out, int request) = 0;  // +0x1D0
    virtual bool fetchLeaderboard(int playerId, int request, int position) = 0;                    // +0x1D4
    virtual void setViewportSize(int width, int height) = 0;                        // +0x1D8 / +0xE78 / +0xE7C
    virtual void getViewportSize(unsigned int* width, unsigned int* height) = 0;    // 119 +0x1DC  0x487C20  SwiffPlayer::notifyMouse, MainMenuLogic::init
    virtual bool getComputerName(std::string& out) = 0;                             // +0x1E0
    virtual bool postMessage(const SwiffHostMessage& message) = 0;                  // 121 +0x1E4  0x496CA0  the five-deep message ring
    virtual int  getMessageCount() = 0;                                             // +0x1E8
    virtual SwiffHostMessage* getMessageByType(int index, int type) = 0;            // 123 +0x1EC  0x48BFE0  a static empty record when missing
    virtual SwiffHostMessage* getMessage(int index) = 0;                            // 124 +0x1F0  0x48BF60  unchecked
    virtual bool removeMessageByType(int index, int type) = 0;                      // +0x1F4
    virtual bool removeMessage(unsigned int index) = 0;                             // +0x1F8
    virtual void clearMessages() = 0;                                               // 127 +0x1FC  0x494210  the message ring emptied and freed
    virtual int  countMessagesByType(int type) = 0;                                 // +0x200
    virtual bool popLastMessage(SwiffHostMessage& out, bool remove) = 0;            // +0x204
    virtual bool removeLastMessage() = 0;                                           // +0x208
    virtual bool setMessagesEnabled(bool enabled) = 0;                              // 131 +0x20C  0x494490  also clears the +0x720 vector
    virtual bool areMessagesEnabled() = 0;                                          // +0x210
    virtual bool getPendingRecord(int* outCode, std::string& outText) = 0;          // 133 +0x214  0x487C70  +0x734/+0x738 [UNRESOLVED] role: a (player id, nick) the lboardplayers list shows beside the profile's own and profileSelection lists first
    virtual void setPendingRecord(int code, const std::string& text) = 0;           // +0x218
    virtual int  getSelectedPlayerId() = 0;                                         // +0x21C / +0x754
    virtual void setSelectedPlayerId(int id) = 0;                                   // 136 +0x220  0x494410  fills +0x758 through getPlayerStatsRecord
    virtual std::string getSelectedPlayerName() = 0;                                // +0x224 / +0x758
    virtual bool getCachedAwards(int profileId, IPlayerAwards::AwardList* out) = 0; // Awards cache, valid for one hour per player.
    virtual std::string formatDate(const time_t* time) = 0;                         // 139 +0x22C  0x487F70  "%d-%s-%.2d" year, month, day
    virtual std::string formatDuration(int seconds) = 0;                            // 140 +0x230  0x4888A0  "%d:%.2d:%.2d"
    virtual std::string resolveMenuPath(const std::string& path) = 0;               // 141 +0x234  0x48A2F0  "$/" and the Extermal [sic] prefix
    virtual std::string getStorageString(const std::string& key, const std::string& fallback) = 0;   // 142 +0x238  0x48D100  dice.bf2.Logic.getStorageString
    virtual void setStorageString(const std::string& key, const std::string& value) = 0;         // +0x23C
    virtual void removeStorageString(const std::string& key) = 0;                   // +0x240
    virtual void handleProfileTransition(const std::string& name) = 0;              // 145 +0x244  0x4944C0  SwiffPlayer::update, event 14
    virtual std::string resolveHostName(const std::string& name) = 0;               // 146 +0x248  0x488440  gethostbyname -> dotted quad
    virtual bool isPrivateAddress(const std::string& address) = 0;                  // +0x24C
    virtual bool isDottedQuad(const std::string& address) = 0;                      // +0x250
    virtual void setJoinAddress(const std::string& address) = 0;                    // +0x254 / +0xE3C
    virtual const std::string& getJoinAddress() = 0;                                // +0x258
    virtual void setIntE58(int value) = 0;                                          // 151 +0x25C  0x4885F0  [UNRESOLVED] role
    virtual int  getIntE58() = 0;                                                   // +0x260
    virtual void requestRestartMenu() = 0;                                          // 153 +0x264  0x4918F0  event 16
    // Adds the joined server to history unless address is private.
    virtual bool addServerToHistory(const std::string& address, int queryPort,
                                    const std::string& name) = 0;                   // +0x268
    virtual bool validateProfileName(const std::string& name, std::string& error) = 0;   // 155 +0x26C  0x4886C0  LocalProfileManager::validateProfileName
    virtual std::string getVersionStringValue() = 0;                                // +0x270
    virtual void setFlagE80(bool value) = 0;                                        // 157 +0x274  0x488850  handleEvent(11,2) true, restartMenu false
    virtual bool getFlagE80OrNotE81() = 0;                                          // +0x278 / +0xE80 / +0xE81
    virtual void setFlagE81(bool value) = 0;                                        // 159 +0x27C  0x4A2360  MainMenuLogic::init (disable-swiff)
    virtual bool getFlagE81() = 0;                                                  // +0x280
    virtual bool getRankInfo(int id, IPlayerRankInfo::RankInfo* out) = 0;           // 161 +0x284  0x494550  the hour-long rank cache
    virtual bool clearRankNotification(int id) = 0;                                 // +0x288
    virtual void clearLastFetchId() = 0;                                            // +0x28C / +0x88
    virtual int  getIntE94() = 0;                                                   // 164 +0x290  0x488880  [UNRESOLVED] role, 10
    virtual void setIntE94(int value) = 0;                                          // +0x294
    virtual bool requestBrowseUrl(const std::string& url) = 0;                      // 166 +0x298  0x493D90  event 10
    virtual void addRank(int id, int a, int b, int c, const std::string& name) = 0; // 167 +0x29C  0x494030  ranks.con, console swiffHost.addRank (executor 0x4112B2)
    virtual void slot2A0(int a, int b, int c, int d, std::string mod) = 0;          // 168 +0x2A0  0x487980  console swiffHost.addAward (awards.con): getPlayerAwards() +0x28 addAward
    virtual void addGameMode(const std::string& gameMode, const std::string& name) = 0; // 169 +0x2A4 0x4876F0, console executor 0x4117AF
    virtual bool getFlagEA8() = 0;                                                  // 170 +0x2A8  0x488940  [UNRESOLVED] role
    virtual void setFlagEA8(bool value) = 0;                                        // +0x2AC
    virtual bool setBusy(int busy) = 0;                                             // 172 +0x2B0  0x488AB0  + UpdateWindow (Logic.beginWaitState)
    virtual void clearBusy() = 0;                                                   // 173 +0x2B4  0x488AE0  (Logic.endWaitState)
    virtual int  getBusy() = 0;                                                     // 174 +0x2B8  0x488AF0  gates the menu's clicks
    virtual int  getPage() = 0;                                                     // +0x2BC / +0xEC0
    virtual void setPage(int page) = 0;                                             // 176 +0x2C0  0x488B10  startGame 1..5
    virtual bool getFlagEC4() = 0;                                                  // 177 +0x2C4  0x488B20  [INFERRED] a join in progress
    virtual void setFlagEC4(bool value) = 0;                                        // +0x2C8
    virtual float getRankProgress() = 0;                                            // 179 +0x2CC  0x4940A0  (score - rank threshold) / (next threshold - rank threshold), 0..1
    virtual bool checkDisc() = 0;                                                   // 180 +0x2D0  0x488B40 -> 0x5A5E20  constant true
    virtual void reportNoDisc() = 0;                                                // 181 +0x2D4  0x488B50  "unable to find BF2 cd/dvd in tray"
    virtual void addEntryEC8(const std::string& text, int value) = 0;               // 182 +0x2D8  0x4976C0  console swiffHost.addCredit (credits.con)
    virtual std::vector<SwiffStringIntEntry>* getEntriesEC8() = 0;                  // +0x2DC
    virtual const std::string* getServerNameString() = 0;                           // 184 +0x2E0  0x488BC0  local ServerSettings::m_welcomeMessage or remote +0xED8
    virtual void setStringED8(const std::string& value) = 0;                        // +0x2E4
    virtual bool isLocalPlayerName(const std::string& playerName) = 0;              // 186 +0x2E8  0x4881A0  [INFERRED] name: getLocalHumanPlayer()->getName() == playerName
    virtual void resetStatsCache() = 0;                                             // 187 +0x2EC  0x4A2510  MainMenuLogic::init
    virtual bool getFlagF04() = 0;                                                  // 188 +0x2F0  0x488C00  [UNRESOLVED] role
    virtual void setFlagF04(bool value) = 0;                                        // 189 +0x2F4  0x488C10  MainMenuLogic::init false
    virtual bool preloadTexture(const std::string& name) = 0;                       // 190 +0x2F8  0x488C20  SwiffRender +0xE0 != -1
    virtual void setStringE5C(const std::string& value) = 0;                        // 191 +0x2FC  0x488C90  joined ServerInfo gameName
    virtual const std::string& getStringE5C() = 0;                                  // 192 +0x300  0x488CA0  joined server name
    virtual int  getConnectToIpPort() = 0;                                          // 193 +0x304  0x488CB0  GSConnectToIpPort, 16567
    virtual void setConnectToIpPort(int port) = 0;                                  // +0x308
    virtual std::string getConnectToIpHost() = 0;                                   // 195 +0x30C  0x488D40  GSConnectToIpHost
    virtual void setConnectToIpHost(const std::string& host) = 0;                   // +0x310
    virtual int  getIntF08() = 0;                                                   // 197 +0x314  0x488DE0  the credits scroller's fade speed, 1
    virtual void setIntF08(int value) = 0;                                          // 198 +0x318  0x488DF0  console swiffHost.setScrollFadeSpeed
    virtual bool getRenderFlag() = 0;                                               // 199 +0x31C  0x488E00  SwiffRender +0xEC
    virtual void setRenderFlag(bool value) = 0;                                     // 200 +0x320  0x488E50  SwiffRender +0xF0
    virtual bool isRunningWithUnsupportedDrivers() = 0;                             // 201 +0x324  0x488EA0  nvidia && driver < 663051
    virtual float getFloatF0C() = 0;                                                // 202 +0x328  0x488F10  the credits scroll speed, 500.0
    virtual void setFloatF0C(float value) = 0;                                      // 203 +0x32C  0x488F20  clamped 500..4000; console swiffHost.setScrollSpeed
    virtual void setFlagF10(bool value) = 0;                                        // 204 +0x330  0x488F70  also +0xF14 = 0, +0xF18 = 1
    virtual bool getFlagF10() = 0;                                                  // +0x334
    virtual int  getIntF14() = 0;                                                   // +0x338
    virtual void setIntF14(int value) = 0;                                          // +0x33C
    virtual void setFlagF18(bool value) = 0;                                        // +0x340
    virtual bool getFlagF18() = 0;                                                  // +0x344
    virtual void getRect(int* out) = 0;                                             // +0x348
    virtual void setRect(int x, int y, int width, int height) = 0;                  // 211 +0x34C  0x489020  console swiffHost.setScrollerRect
    virtual void setServerMod(const std::string& mod) = 0;                          // +0x350 / +0xF30
    virtual const std::string& getServerMod() = 0;                                  // 213 +0x354  0x489060  startGame's mod mismatch check
    virtual int  getIntF4C() = 0;                                                   // 214 +0x358  0x489070  rounds left (EOR join gate)
    virtual void setIntF4C(int value) = 0;                                          // 215 +0x35C  0x489080  GameLogic sentinel / EndOfRoundEvent
    virtual bool isPlayingDemo() = 0;                                               // +0x360
    virtual std::string getModName() = 0;                                           // 217 +0x364  0x4890B0  GSModDirectory's leaf, lowercased
    virtual bool slot368(int a, int b, int c) = 0;                                  // 218 +0x368  0x626200  constant true
    virtual int  getExpansionPack(const std::string& modName) = 0;                  // 219 +0x36C  0x4890D0  1 xpack, 2 xpack2
    virtual std::string getLanguageSetting() = 0;                                   // 220 +0x370  0x489190  GSLanguage, "English"
    virtual void parseResolution(const std::string& text, int* width, int* height, int* hz) = 0;   // 221 +0x374  0x489210  "x@Hz" tokens
    virtual bool getFlagF2C() = 0;                                                  // 222 +0x378  0x489340  [UNRESOLVED] role, 1
    virtual void setFlagF2C(bool value) = 0;                                        // +0x37C
    // the ServerInfo block (PE +88..+124 of the ServerInfo singleton) --------------------------
    virtual bool getServerInfoNoVehicles() = 0;                                     // +0x380
    virtual bool getServerInfoAlwaysTrue() = 0;                                     // +0x384
    virtual int  getServerInfoRoundsPerMap() = 0;                                   // +0x388
    virtual int  getServerInfoSpawnTime() = 0;                                      // +0x38C
    virtual int  getServerInfoTimeLimit() = 0;                                      // +0x390
    virtual int  getServerInfoTicketRatio() = 0;                                    // +0x394
    virtual int  getServerInfoTeamRatioPercent() = 0;                               // +0x398
    virtual bool getServerInfoAutoBalanceTeam() = 0;                                // +0x39C
    virtual bool getServerInfoUseGlobalUnlocks() = 0;                               // +0x3A0
    virtual float getServerInfoCoopBotRatio() = 0;                                  // 233 +0x3A4  0x489360  read as float [UNRESOLVED] width
    virtual int  getServerInfoCoopBotCount() = 0;                                   // +0x3A8
    virtual float getServerInfoCoopBotDifficulty() = 0;                             // 235 +0x3AC  0x489380  read as float [UNRESOLVED] width
    virtual bool checkGameConnected() = 0;                                          // 236 +0x3B0  0x4870F0  game->queryInterface(120002)
    virtual bool loadRanks() = 0;                                                   // 237 +0x3B4  0x487820  ranks.con, once
};

} // namespace ui
} }
#endif
#endif
