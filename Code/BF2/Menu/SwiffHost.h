// Host implementation for the front-end menu plugin.
#ifndef MENU_SWIFFHOST_H
#define MENU_SWIFFHOST_H
#include "ISwiffHost.h"
#include "../Game/Persistence/IPlayerStats.h"
#include <map>
#include <vector>
#if !DEDICATED
namespace dice { namespace hfe {

// Cached records exposed to the menu player.

// The leaderboard cache (+0xA4, and the values of the +0x6F4 map) is SwiffLeaderboardCache and
// the 3720-byte record of the +0x74 vector is SwiffPlayerStatsRecord, both in ISwiffHost.h: the
// native SwiffPlayer flavour reads them typed.

class SwiffHost : public ui::ISwiffHost
{
public:
    SwiffHost();
    static IBase* create(unsigned int iid, IBase* outer);        // 0x410244 (__fastcall)

    // IBase
    void  addRef() override;                                     // 0x6D0970 (the shared body)
    int   getRef() override;
    int   release() override;                                    // the 0x4A20B0 body, then the free
    void* queryInterface(unsigned int iid) const override;       // 0x4A2330: 1 and 121011

    bool init() override;
    bool loadCredits() override;
    bool recreateControlMap(const std::string& name) override;
    bool deleteControlMap(const std::string& name) override;
    void update() override;
    void onMenuActivated() override;
    bool pollEvent(MenuEvent& out) override;
    int  pushEvent(MenuEvent& event) override;
    IBase* getPlayerStats() override;
    IBase* getPlayerAwards() override;
    IBase* getPlayerUnlocks() override;
    IBase* getSearchForPlayer() override;
    IBase* getLeaderBoard() override;
    IBase* getDemoLibrary() override;
    IBase* getPlayerUnlockInfo() override;
    IBase* getServerLogoManager() override;
    IBase* getMenuTeamManager() override;
    IBase* getPlayerRankInfo() override;
    std::string getCurrentMapName() override;
    std::string getCurrentMapGameMode() override;
    int  getCurrentMapMaxPlayers() override;
    int  getCurrentMapSize() override;
    bool setPlayerRenderCapability0(intptr_t value) override;
    void* getCursor(int which) override;
    bool setPlayerRenderCapability1(intptr_t value) override;
    void setFlag28(bool value) override;
    bool getFlag28() override;
    ui::IServerBrowser* getServerList(const std::string& name) override;
    bool getPlayerName(std::string& out, const std::string& fallback) override;
    bool setPlayerName(const std::string& name) override;
    bool getCommandLineFile(std::string& out) override;
    bool setCommandLineFile(const std::string& commandLine) override;
    void setLoadingProgress(float value) override;
    float getLoadingProgress() override;
    int  getGameStatus() override;
    float getTimeUntilRestartMap() override;
    std::string getLoadingRecordString() override;
    bool getSpStartupMap(std::string& out) override;
    bool getLocalizedString(std::wstring& out, const std::string& key) override;
    bool requestEvent1() override;
    bool requestQuit() override;
    bool requestShutdownGame() override;
    bool isGameRunning(bool* out) override;
    int  getMessageId() override;
    void setMessageId(int id) override;
    std::string getFrameLabel() override;
    void setFrameLabel(const std::string& label) override;
    bool getFlag7B8() override;
    void setFlag7B8(bool value) override;
    bool getGameStarting() override;
    void setGameStarting(bool value) override;
    std::string getJoinAddressSetting() override;
    std::string getPasswordSetting() override;
    int  getPortSetting() override;
    bool requestHostServer(const SwiffSinglePlayerLevel* level) override;
    bool getSinglePlayerLevels(std::vector<SwiffSinglePlayerLevel>& out) override;
    bool getMapListCount(int* out) override;
    bool clearServerEntries() override;
    bool setServerEntries(const std::vector<SwiffServerEntry>& entries) override;
    bool getServerEntries(std::vector<SwiffServerEntry>& out) override;
    bool joinServer(const std::string& address, int port, int queryPort) override;
    bool hostServer() override;
    bool isGameSpyAvailable() override;
    void setSelectedServer(const std::string& address, int queryPort, int hostPort) override;
    void getSelectedServer(std::string& address, int* queryPort, int* hostPort) override;
    int  getIndexedInt(int index) override;
    void setIndexedInt(int index, int value) override;
    bool getServerSettingsInfo(std::string& interfaceIP, int* queryPort, int* port) override;
    IBase* getModInfo() override;
    std::string getModDirectorySetting() override;
    bool addServerEntry(const std::string& map, const std::string& gameMode, int size) override;
    bool removeServerEntry(unsigned int index) override;
    bool moveServerEntry(unsigned int index, bool up) override;
    std::string getServerListKind() override;
    void setServerListKind(const std::string& kind) override;
    int  getLeaderboardCategory() override;
    void setLeaderboardCategory(int category) override;
    int  getLeaderboardId() override;
    void setLeaderboardId(int id) override;
    int  getLeaderboardPosition() override;
    void setLeaderboardPosition(int position) override;
    void goLeaderboardPlayer(int playerId) override;
    int  getLeaderboardPlayerPosition(int playerId, int request) override;
    void setIntE98(int value) override;
    int  getIntE98() override;
    std::string getGameMode() override;
    void setGameMode(const std::string& mode) override;
    bool playNow() override;
    bool setEndOfRoundInfo(const std::string& data, float delay) override;   // Menu/SwiffHostEndOfRound.cpp
    int  resetEndOfRoundInfo() override;
    SwiffEndOfRoundInfo* getEndOfRoundInfo() override;
    std::string getTeamName(int team) override;
    std::string getJoinPassword() override;
    void setJoinPassword(const std::string& password) override;
    bool playSound(const std::string& name) override;
    void slot18C(int a) override;
    bool getModCount(int* out) override;
    bool slot194(int index, int b) override;
    bool requestActivateMod(const std::string* mod) override;
    bool requestBrowseModHomepage(const IModInfo::Mod* mod) override;
    bool getVersionString(std::string& out) override;
    bool getEndOfRoundSummary(SwiffEndOfRoundSummary& out) override;         // Menu/SwiffHostEndOfRound.cpp
    bool isGameLogicServer() override;
    bool fetchPlayerStats(int playerId) override;
    bool getPlayerStatsRecord(int playerId, SwiffPlayerStatsRecord& out) override;
    std::string getRankName(int id) override;
    int  getRankB(int id) override;
    int  getRankC(int id) override;
    int  getRankCount() override;
    bool getUnlocks(int playerId, IPlayerUnlocks::UnlockCache* out) override;
    bool fetchUnlocks(int playerId) override;
    bool getLeaderboard(SwiffLeaderboardCache* out, int request, int position) override;
    bool getLeaderboardPlayer(int playerId, SwiffLeaderboardCache* out, int request) override;
    bool fetchLeaderboard(int playerId, int request, int position) override;
    void setViewportSize(int width, int height) override;
    void getViewportSize(unsigned int* width, unsigned int* height) override;
    bool getComputerName(std::string& out) override;
    bool postMessage(const SwiffHostMessage& message) override;
    int  getMessageCount() override;
    SwiffHostMessage* getMessageByType(int index, int type) override;
    SwiffHostMessage* getMessage(int index) override;
    bool removeMessageByType(int index, int type) override;
    bool removeMessage(unsigned int index) override;
    void clearMessages() override;
    int  countMessagesByType(int type) override;
    bool popLastMessage(SwiffHostMessage& out, bool remove) override;
    bool removeLastMessage() override;
    bool setMessagesEnabled(bool enabled) override;
    bool areMessagesEnabled() override;
    bool getPendingRecord(int* outCode, std::string& outText) override;
    void setPendingRecord(int code, const std::string& text) override;
    int  getSelectedPlayerId() override;
    void setSelectedPlayerId(int id) override;
    std::string getSelectedPlayerName() override;
    bool getCachedAwards(int profileId, IPlayerAwards::AwardList* out) override;
    std::string formatDate(const time_t* time) override;
    std::string formatDuration(int seconds) override;
    std::string resolveMenuPath(const std::string& path) override;
    std::string getStorageString(const std::string& key, const std::string& fallback) override;
    void setStorageString(const std::string& key, const std::string& value) override;
    void removeStorageString(const std::string& key) override;
    void handleProfileTransition(const std::string& name) override;
    std::string resolveHostName(const std::string& name) override;
    bool isPrivateAddress(const std::string& address) override;
    bool isDottedQuad(const std::string& address) override;
    void setJoinAddress(const std::string& address) override;
    const std::string& getJoinAddress() override;
    void setIntE58(int value) override;
    int  getIntE58() override;
    void requestRestartMenu() override;
    bool addServerToHistory(const std::string& address, int queryPort,
                            const std::string& name) override;
    bool validateProfileName(const std::string& name, std::string& error) override;
    std::string getVersionStringValue() override;
    void setFlagE80(bool value) override;
    bool getFlagE80OrNotE81() override;
    void setFlagE81(bool value) override;
    bool getFlagE81() override;
    bool getRankInfo(int id, IPlayerRankInfo::RankInfo* out) override;
    bool clearRankNotification(int id) override;
    void clearLastFetchId() override;
    int  getIntE94() override;
    void setIntE94(int value) override;
    bool requestBrowseUrl(const std::string& url) override;
    void addRank(int id, int a, int b, int c, const std::string& name) override;
    void slot2A0(int a, int b, int c, int d, std::string mod) override;
    void addGameMode(const std::string& gameMode, const std::string& name) override;
    bool getFlagEA8() override;
    void setFlagEA8(bool value) override;
    bool setBusy(int busy) override;
    void clearBusy() override;
    int  getBusy() override;
    int  getPage() override;
    void setPage(int page) override;
    bool getFlagEC4() override;
    void setFlagEC4(bool value) override;
    float getRankProgress() override;
    bool checkDisc() override;
    void reportNoDisc() override;
    void addEntryEC8(const std::string& text, int value) override;
    std::vector<SwiffStringIntEntry>* getEntriesEC8() override;
    const std::string* getServerNameString() override;
    void setStringED8(const std::string& value) override;
    bool isLocalPlayerName(const std::string& playerName) override;          // Menu/SwiffHostEndOfRound.cpp
    void resetStatsCache() override;
    bool getFlagF04() override;
    void setFlagF04(bool value) override;
    bool preloadTexture(const std::string& name) override;
    void setStringE5C(const std::string& value) override;
    const std::string& getStringE5C() override;
    int  getConnectToIpPort() override;
    void setConnectToIpPort(int port) override;
    std::string getConnectToIpHost() override;
    void setConnectToIpHost(const std::string& host) override;
    int  getIntF08() override;
    void setIntF08(int value) override;
    bool getRenderFlag() override;
    void setRenderFlag(bool value) override;
    bool isRunningWithUnsupportedDrivers() override;
    float getFloatF0C() override;
    void setFloatF0C(float value) override;
    void setFlagF10(bool value) override;
    bool getFlagF10() override;
    int  getIntF14() override;
    void setIntF14(int value) override;
    void setFlagF18(bool value) override;
    bool getFlagF18() override;
    void getRect(int* out) override;
    void setRect(int x, int y, int width, int height) override;
    void setServerMod(const std::string& mod) override;
    const std::string& getServerMod() override;
    int  getIntF4C() override;
    void setIntF4C(int value) override;
    bool isPlayingDemo() override;
    std::string getModName() override;
    bool slot368(int a, int b, int c) override;
    int  getExpansionPack(const std::string& modName) override;
    std::string getLanguageSetting() override;
    void parseResolution(const std::string& text, int* width, int* height, int* hz) override;
    bool getFlagF2C() override;
    void setFlagF2C(bool value) override;
    bool getServerInfoNoVehicles() override;
    bool getServerInfoAlwaysTrue() override;
    int  getServerInfoRoundsPerMap() override;
    int  getServerInfoSpawnTime() override;
    int  getServerInfoTimeLimit() override;
    int  getServerInfoTicketRatio() override;
    int  getServerInfoTeamRatioPercent() override;
    bool getServerInfoAutoBalanceTeam() override;
    bool getServerInfoUseGlobalUnlocks() override;
    float getServerInfoCoopBotRatio() override;
    int  getServerInfoCoopBotCount() override;
    float getServerInfoCoopBotDifficulty() override;
    bool checkGameConnected() override;
    bool loadRanks() override;

private:
    // a stats key that is a time, as a date through formatDate, or "-" when zero.
    std::string statDate(const std::string& key);
    // The favourite's time from the second getplayerinfo query (0x49D450): getKey(0, keyIndex)
    // through formatDuration, or "-" for a favourite of -1.
    std::string favouriteTime(IPlayerStats* stats, int favourite, int keyIndex);
    // Reads a key from the current leaderboard row, returning zero or an empty string when absent.
    // Each call retrieves the current board again.
    int         leaderboardInt(int row, const std::string& key);
    std::string leaderboardString(int row, const std::string& key);
    double      leaderboardFloat(int row, const std::string& key);
    // formatDate's twin with the time of day: "%d-%s-%.2d %.2d:%.2d", or "-".
    std::string formatDateTime(const time_t* time);
    ~SwiffHost();                                                // 0x4A20B0, run by release()
    friend struct SwiffHostFixture;                              // [TEMP] the fixtures' member view

    int                                   m_refCount;              // +0x004  1
    std::vector<MenuEvent>                m_events;                // +0x008  the menu event queue (20-byte rows)
    IBase*                                m_serverBrowserManager;  // +0x018
    int                                   m_indexedInts[2];        // +0x01C  getIndexedInt / setIndexedInt
    float                                 m_loadingProgress;       // +0x024
    bool                                  m_flag28;                // +0x028  1
    IBase*                                m_playerStats;           // +0x02C  created lazily, released by the destructor
    IBase*                                m_playerAwards;          // +0x030
    IBase*                                m_playerUnlocks;         // +0x034
    IBase*                                m_searchForPlayer;       // +0x038
    IBase*                                m_leaderBoard;           // +0x03C
    IBase*                                m_demoLibrary;           // +0x040  singleton, not released
    IBase*                                m_modInfo;               // +0x044  singleton, not released
    IBase*                                m_playerUnlockInfo;      // +0x048  created lazily, released
    IBase*                                m_serverLogoManager;     // +0x04C  singleton, not released
    IBase*                                m_menuTeamManager;       // +0x050  singleton, zeroed by the destructor
    IBase*                                m_playerRankInfo;        // +0x054  created lazily, released
    std::string                           m_serverListKind;        // +0x058
    std::vector<SwiffPlayerStatsRecord>   m_playerStatsRecords;    // +0x074  3720-byte rows
    IPlayerUnlocks::UnlockCache           m_unlockRecord;          // +0x084  time, id, a, b, the +0x94 vector
    SwiffLeaderboardCache                 m_leaderboard;           // +0x0A4  1616 bytes, the global boards
    std::map<int, SwiffLeaderboardCache>  m_leaderboardByPlayer;   // +0x6F4  one player's boards each
    int                                   m_messageId;             // +0x700
    std::string                           m_frameLabel;            // +0x704
    std::vector<SwiffHostMessage>         m_messages;              // +0x720  the five-deep ring (36-byte rows)
    bool                                  m_messagesEnabled;       // +0x730  1
    int                                   m_pendingCode;           // +0x734
    std::string                           m_pendingText;           // +0x738
    int                                   m_selectedPlayerId;      // +0x754
    std::string                           m_selectedPlayerName;    // +0x758
    int                                   m_leaderboardCategory;   // +0x774
    int                                   m_leaderboardId;         // +0x778
    int                                   m_leaderboardPosition;   // +0x77C  1
    std::string                           m_gameMode;              // +0x780  "gpm_cq"
    std::vector<IPlayerAwards::AwardList> m_awardCache;            // +0x79C  24-byte rows
    std::map<std::string, std::string>    m_storage;               // +0x7AC  getStorageString / setStorageString
    bool                                  m_flag7B8;               // +0x7B8
    bool                                  m_gameStarting;          // +0x7B9
    SwiffEndOfRoundInfo                   m_endOfRoundInfo;        // +0x7BC
    std::string                           m_joinPassword;          // +0xE20
    std::string                           m_joinAddress;           // +0xE3C
    int                                   m_intE58;                // +0xE58
    std::string                           m_stringE5C;             // +0xE5C joined ServerInfo gameName
    int                                   m_viewportWidth;         // +0xE78  800
    int                                   m_viewportHeight;        // +0xE7C  600
    bool                                  m_flagE80;               // +0xE80
    bool                                  m_flagE81;               // +0xE81  1
    std::vector<IPlayerRankInfo::RankInfo> m_rankCache;            // +0xE84  20-byte rows, an hour each
    int                                   m_intE94;                // +0xE94  10
    int                                   m_intE98;                // +0xE98
    std::map<int, SwiffRankRecord>        m_ranks;                 // +0xE9C  ranks.con
    bool                                  m_flagEA8;               // +0xEA8
    time_t                                m_timeEAC;               // +0xEAC  stamped by the constructor
    void*                                 m_cursors[3];            // +0xEB0  hcursor arrow / busy / keybinding
    int                                   m_busy;                  // +0xEBC
    int                                   m_page;                  // +0xEC0
    bool                                  m_flagEC4;               // +0xEC4
    std::vector<SwiffStringIntEntry>      m_entriesEC8;            // +0xEC8  32-byte rows
    std::string                           m_stringED8;             // +0xED8
    std::vector<SwiffServerEntry>         m_serverEntries;         // +0xEF4  60-byte rows
    bool                                  m_flagF04;               // +0xF04
    int                                   m_intF08;                // +0xF08  1
    float                                 m_floatF0C;              // +0xF0C  500.0
    bool                                  m_flagF10;               // +0xF10
    int                                   m_intF14;                // +0xF14
    bool                                  m_flagF18;               // +0xF18
    int                                   m_rect[4];               // +0xF1C  0, 0, 1024, 768
    bool                                  m_flagF2C;               // +0xF2C  1
    std::string                           m_serverMod;             // +0xF30
    int                                   m_intF4C;                // +0xF4C rounds left (endOfRound.swf join gate)
};

} }
#endif
#endif
