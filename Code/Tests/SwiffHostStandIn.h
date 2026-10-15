// [TEMP] [CHOSEN, not recovered] A fixture-only stand-in for dice::hfe::ui::ISwiffHost: every slot
// aborts, so a test that reaches a slot it did not expect fails loudly. Fixtures derive from it and
// override the slots they observe (postMessage for the error reporters, the queue and viewport
// for the menu bridge). Not a production object; never linked into an image.
#ifndef BF2_STUBS_SWIFFHOSTSTANDIN_H
#define BF2_STUBS_SWIFFHOSTSTANDIN_H
#include "../BF2/Menu/ISwiffHost.h"
#include <cstdlib>

struct SwiffHostStandIn : dice::hfe::ui::ISwiffHost
{
    typedef dice::hfe::IBase IBase;
    typedef dice::hfe::MenuEvent MenuEvent;
    typedef dice::hfe::SwiffHostMessage SwiffHostMessage;
    int refs = 1;
    void  addRef() override { ++refs; }
    int   getRef() override { return refs; }
    int   release() override { return --refs; }
    void* queryInterface(unsigned int iid) const override { return (iid == 1 || iid == 121011u) ? const_cast<SwiffHostStandIn*>(this) : nullptr; }
    bool init() override { std::abort(); }
    bool loadCredits() override { std::abort(); }
    bool recreateControlMap(const std::string&) override { std::abort(); }
    bool deleteControlMap(const std::string&) override { std::abort(); }
    void update() override { std::abort(); }
    void onMenuActivated() override { std::abort(); }
    bool pollEvent(MenuEvent&) override { std::abort(); }
    int  pushEvent(MenuEvent&) override { std::abort(); }
    IBase* getPlayerStats() override { std::abort(); }
    IBase* getPlayerAwards() override { std::abort(); }
    IBase* getPlayerUnlocks() override { std::abort(); }
    IBase* getSearchForPlayer() override { std::abort(); }
    IBase* getLeaderBoard() override { std::abort(); }
    IBase* getDemoLibrary() override { std::abort(); }
    IBase* getPlayerUnlockInfo() override { std::abort(); }
    IBase* getServerLogoManager() override { std::abort(); }
    IBase* getMenuTeamManager() override { std::abort(); }
    IBase* getPlayerRankInfo() override { std::abort(); }
    std::string getCurrentMapName() override { std::abort(); }
    std::string getCurrentMapGameMode() override { std::abort(); }
    int  getCurrentMapMaxPlayers() override { std::abort(); }
    int  getCurrentMapSize() override { std::abort(); }
    bool setPlayerRenderCapability0(intptr_t) override { std::abort(); }
    void* getCursor(int) override { std::abort(); }
    bool setPlayerRenderCapability1(intptr_t) override { std::abort(); }
    void setFlag28(bool) override { std::abort(); }
    bool getFlag28() override { std::abort(); }
    dice::hfe::ui::IServerBrowser* getServerList(const std::string&) override { std::abort(); }
    bool getPlayerName(std::string&, const std::string&) override { std::abort(); }
    bool setPlayerName(const std::string&) override { std::abort(); }
    bool getCommandLineFile(std::string&) override { std::abort(); }
    bool setCommandLineFile(const std::string&) override { std::abort(); }
    void setLoadingProgress(float) override { std::abort(); }
    float getLoadingProgress() override { std::abort(); }
    int  getGameStatus() override { std::abort(); }
    float getTimeUntilRestartMap() override { std::abort(); }
    std::string getLoadingRecordString() override { std::abort(); }
    bool getSpStartupMap(std::string&) override { std::abort(); }
    bool getLocalizedString(std::wstring&, const std::string&) override { std::abort(); }
    bool requestEvent1() override { std::abort(); }
    bool requestQuit() override { std::abort(); }
    bool requestShutdownGame() override { std::abort(); }
    bool isGameRunning(bool*) override { std::abort(); }
    int  getMessageId() override { std::abort(); }
    void setMessageId(int) override { std::abort(); }
    std::string getFrameLabel() override { std::abort(); }
    void setFrameLabel(const std::string&) override { std::abort(); }
    bool getFlag7B8() override { std::abort(); }
    void setFlag7B8(bool) override { std::abort(); }
    bool getGameStarting() override { std::abort(); }
    void setGameStarting(bool) override { std::abort(); }
    std::string getJoinAddressSetting() override { std::abort(); }
    std::string getPasswordSetting() override { std::abort(); }
    int  getPortSetting() override { std::abort(); }
    bool requestHostServer(const dice::hfe::SwiffSinglePlayerLevel*) override { std::abort(); }
    bool getSinglePlayerLevels(std::vector<dice::hfe::SwiffSinglePlayerLevel>&) override { std::abort(); }
    bool getMapListCount(int*) override { std::abort(); }
    bool clearServerEntries() override { std::abort(); }
    bool setServerEntries(const std::vector<dice::hfe::SwiffServerEntry>&) override { std::abort(); }
    bool getServerEntries(std::vector<dice::hfe::SwiffServerEntry>&) override { std::abort(); }
    bool joinServer(const std::string&, int, int) override { std::abort(); }
    bool hostServer() override { std::abort(); }
    bool isGameSpyAvailable() override { std::abort(); }
    void setSelectedServer(const std::string&, int, int) override { std::abort(); }
    void getSelectedServer(std::string&, int*, int*) override { std::abort(); }
    int  getIndexedInt(int) override { std::abort(); }
    void setIndexedInt(int, int) override { std::abort(); }
    bool getServerSettingsInfo(std::string&, int*, int*) override { std::abort(); }
    IBase* getModInfo() override { std::abort(); }
    std::string getModDirectorySetting() override { std::abort(); }
    bool addServerEntry(const std::string&, const std::string&, int) override { std::abort(); }
    bool removeServerEntry(unsigned int) override { std::abort(); }
    bool moveServerEntry(unsigned int, bool) override { std::abort(); }
    std::string getServerListKind() override { std::abort(); }
    void setServerListKind(const std::string&) override { std::abort(); }
    int  getLeaderboardCategory() override { std::abort(); }
    void setLeaderboardCategory(int) override { std::abort(); }
    int  getLeaderboardId() override { std::abort(); }
    void setLeaderboardId(int) override { std::abort(); }
    int  getLeaderboardPosition() override { std::abort(); }
    void setLeaderboardPosition(int) override { std::abort(); }
    void goLeaderboardPlayer(int) override { std::abort(); }
    int  getLeaderboardPlayerPosition(int, int) override { std::abort(); }
    void setIntE98(int) override { std::abort(); }
    int  getIntE98() override { std::abort(); }
    std::string getGameMode() override { std::abort(); }
    void setGameMode(const std::string&) override { std::abort(); }
    bool playNow() override { std::abort(); }
    bool setEndOfRoundInfo(const std::string&, float) override { std::abort(); }
    int  resetEndOfRoundInfo() override { std::abort(); }
    dice::hfe::SwiffEndOfRoundInfo* getEndOfRoundInfo() override { std::abort(); }
    std::string getTeamName(int) override { std::abort(); }
    std::string getJoinPassword() override { std::abort(); }
    void setJoinPassword(const std::string&) override { std::abort(); }
    bool playSound(const std::string&) override { std::abort(); }
    void slot18C(int) override { std::abort(); }
    bool getModCount(int*) override { std::abort(); }
    bool slot194(int, int) override { std::abort(); }
    bool requestActivateMod(const std::string*) override { std::abort(); }
    bool requestBrowseModHomepage(const dice::hfe::IModInfo::Mod*) override { std::abort(); }
    bool getVersionString(std::string&) override { std::abort(); }
    bool getEndOfRoundSummary(dice::hfe::SwiffEndOfRoundSummary&) override { std::abort(); }
    bool isGameLogicServer() override { std::abort(); }
    bool fetchPlayerStats(int) override { std::abort(); }
    bool getPlayerStatsRecord(int, dice::hfe::SwiffPlayerStatsRecord&) override { std::abort(); }
    std::string getRankName(int) override { std::abort(); }
    int  getRankB(int) override { std::abort(); }
    int  getRankC(int) override { std::abort(); }
    int  getRankCount() override { std::abort(); }
    bool getUnlocks(int, dice::hfe::IPlayerUnlocks::UnlockCache*) override { std::abort(); }
    bool fetchUnlocks(int) override { std::abort(); }
    bool getLeaderboard(dice::hfe::SwiffLeaderboardCache*, int, int) override { std::abort(); }
    bool getLeaderboardPlayer(int, dice::hfe::SwiffLeaderboardCache*, int) override { std::abort(); }
    bool fetchLeaderboard(int, int, int) override { std::abort(); }
    void setViewportSize(int, int) override { std::abort(); }
    void getViewportSize(unsigned int*, unsigned int*) override { std::abort(); }
    bool getComputerName(std::string&) override { std::abort(); }
    bool postMessage(const SwiffHostMessage&) override { std::abort(); }
    int  getMessageCount() override { std::abort(); }
    SwiffHostMessage* getMessageByType(int, int) override { std::abort(); }
    SwiffHostMessage* getMessage(int) override { std::abort(); }
    bool removeMessageByType(int, int) override { std::abort(); }
    bool removeMessage(unsigned int) override { std::abort(); }
    void clearMessages() override { std::abort(); }
    int  countMessagesByType(int) override { std::abort(); }
    bool popLastMessage(SwiffHostMessage&, bool) override { std::abort(); }
    bool removeLastMessage() override { std::abort(); }
    bool setMessagesEnabled(bool) override { std::abort(); }
    bool areMessagesEnabled() override { std::abort(); }
    bool getPendingRecord(int*, std::string&) override { std::abort(); }
    void setPendingRecord(int, const std::string&) override { std::abort(); }
    int  getSelectedPlayerId() override { std::abort(); }
    void setSelectedPlayerId(int) override { std::abort(); }
    std::string getSelectedPlayerName() override { std::abort(); }
    bool getCachedAwards(int, dice::hfe::IPlayerAwards::AwardList*) override { std::abort(); }
    std::string formatDate(const time_t*) override { std::abort(); }
    std::string formatDuration(int) override { std::abort(); }
    std::string resolveMenuPath(const std::string&) override { std::abort(); }
    std::string getStorageString(const std::string&, const std::string&) override { std::abort(); }
    void setStorageString(const std::string&, const std::string&) override { std::abort(); }
    void removeStorageString(const std::string&) override { std::abort(); }
    void handleProfileTransition(const std::string&) override { std::abort(); }
    std::string resolveHostName(const std::string&) override { std::abort(); }
    bool isPrivateAddress(const std::string&) override { std::abort(); }
    bool isDottedQuad(const std::string&) override { std::abort(); }
    void setJoinAddress(const std::string&) override { std::abort(); }
    const std::string& getJoinAddress() override { std::abort(); }
    void setIntE58(int) override { std::abort(); }
    int  getIntE58() override { std::abort(); }
    void requestRestartMenu() override { std::abort(); }
    bool addServerToHistory(const std::string&, int, const std::string&) override { std::abort(); }
    bool validateProfileName(const std::string&, std::string&) override { std::abort(); }
    std::string getVersionStringValue() override { std::abort(); }
    void setFlagE80(bool) override { std::abort(); }
    bool getFlagE80OrNotE81() override { std::abort(); }
    void setFlagE81(bool) override { std::abort(); }
    bool getFlagE81() override { std::abort(); }
    bool getRankInfo(int, dice::hfe::IPlayerRankInfo::RankInfo*) override { std::abort(); }
    bool clearRankNotification(int) override { std::abort(); }
    void clearLastFetchId() override { std::abort(); }
    int  getIntE94() override { std::abort(); }
    void setIntE94(int) override { std::abort(); }
    bool requestBrowseUrl(const std::string&) override { std::abort(); }
    void addRank(int, int, int, int, const std::string&) override { std::abort(); }
    void slot2A0(int, int, int, int, std::string) override { std::abort(); }
    void addGameMode(const std::string&, const std::string&) override { std::abort(); }
    bool getFlagEA8() override { std::abort(); }
    void setFlagEA8(bool) override { std::abort(); }
    bool setBusy(int) override { std::abort(); }
    void clearBusy() override { std::abort(); }
    int  getBusy() override { std::abort(); }
    int  getPage() override { std::abort(); }
    void setPage(int) override { std::abort(); }
    bool getFlagEC4() override { std::abort(); }
    void setFlagEC4(bool) override { std::abort(); }
    float getRankProgress() override { std::abort(); }
    bool checkDisc() override { std::abort(); }
    void reportNoDisc() override { std::abort(); }
    void addEntryEC8(const std::string&, int) override { std::abort(); }
    std::vector<dice::hfe::SwiffStringIntEntry>* getEntriesEC8() override { std::abort(); }
    const std::string* getServerNameString() override { std::abort(); }
    void setStringED8(const std::string&) override { std::abort(); }
    bool isLocalPlayerName(const std::string&) override { std::abort(); }
    void resetStatsCache() override { std::abort(); }
    bool getFlagF04() override { std::abort(); }
    void setFlagF04(bool) override { std::abort(); }
    bool preloadTexture(const std::string&) override { std::abort(); }
    void setStringE5C(const std::string&) override { std::abort(); }
    const std::string& getStringE5C() override { std::abort(); }
    int  getConnectToIpPort() override { std::abort(); }
    void setConnectToIpPort(int) override { std::abort(); }
    std::string getConnectToIpHost() override { std::abort(); }
    void setConnectToIpHost(const std::string&) override { std::abort(); }
    int  getIntF08() override { std::abort(); }
    void setIntF08(int) override { std::abort(); }
    bool getRenderFlag() override { std::abort(); }
    void setRenderFlag(bool) override { std::abort(); }
    bool isRunningWithUnsupportedDrivers() override { std::abort(); }
    float getFloatF0C() override { std::abort(); }
    void setFloatF0C(float) override { std::abort(); }
    void setFlagF10(bool) override { std::abort(); }
    bool getFlagF10() override { std::abort(); }
    int  getIntF14() override { std::abort(); }
    void setIntF14(int) override { std::abort(); }
    void setFlagF18(bool) override { std::abort(); }
    bool getFlagF18() override { std::abort(); }
    void getRect(int*) override { std::abort(); }
    void setRect(int, int, int, int) override { std::abort(); }
    void setServerMod(const std::string&) override { std::abort(); }
    const std::string& getServerMod() override { std::abort(); }
    int  getIntF4C() override { std::abort(); }
    void setIntF4C(int) override { std::abort(); }
    bool isPlayingDemo() override { std::abort(); }
    std::string getModName() override { std::abort(); }
    bool slot368(int, int, int) override { std::abort(); }
    int  getExpansionPack(const std::string&) override { std::abort(); }
    std::string getLanguageSetting() override { std::abort(); }
    void parseResolution(const std::string&, int*, int*, int*) override { std::abort(); }
    bool getFlagF2C() override { std::abort(); }
    void setFlagF2C(bool) override { std::abort(); }
    bool getServerInfoNoVehicles() override { std::abort(); }
    bool getServerInfoAlwaysTrue() override { std::abort(); }
    int  getServerInfoRoundsPerMap() override { std::abort(); }
    int  getServerInfoSpawnTime() override { std::abort(); }
    int  getServerInfoTimeLimit() override { std::abort(); }
    int  getServerInfoTicketRatio() override { std::abort(); }
    int  getServerInfoTeamRatioPercent() override { std::abort(); }
    bool getServerInfoAutoBalanceTeam() override { std::abort(); }
    bool getServerInfoUseGlobalUnlocks() override { std::abort(); }
    float getServerInfoCoopBotRatio() override { std::abort(); }
    int  getServerInfoCoopBotCount() override { std::abort(); }
    float getServerInfoCoopBotDifficulty() override { std::abort(); }
    bool checkGameConnected() override { std::abort(); }
    bool loadRanks() override { std::abort(); }
};
#endif
