// Connects the front-end menu plugin to engine services.
// [RETIRE-WHEN: swiff-host-bodies] Complete and validate the corresponding SwiffHost operation.
#include "../Core/BuildConfig.h"
#if !DEDICATED
#include <winsock2.h>                       // before windows.h: gethostbyname / inet_addr / inet_ntoa / ntohl
#include <windows.h>
#include "SwiffHost.h"
#include "IMenuTeamManager.h"
#include "ISwiffPlayer.h"
#include "ISwiffRender.h"
#include "../Core/ClassManager.h"
#include "../Core/LoadStat.h"                 // PE dword_9A60A8: the live loading record
#include "../Core/SmartItf.h"               // classManager
#include "../Core/Ids.h"
#include "../Core/SettingsRepostitory.h"
#include "../Core/StringUtil.h"             // formatString, compareIgnoreCase, setLowerCase
#include "../Core/SystemInfo.h"
#include "../Game/IGame.h"
#include "../Game/IGameLogic.h"
#include "../Game/IModInfo.h"
#include "../Game/LocalProfileManager.h"
#include "../Game/GeneralSettings.h"               // addServerToHistory -> addServerHistory (+0x138)
#include "../Game/GameServerSettings.h"     // joinServer applies the settings before a join
#include "../Game/MapList.h"                // joinServer clears and saves the map list; getMapListCount
#include "ServerLogoManager.h"              // the typed logo cache behind getServerLogoManager
#include "../Game/GameServer/ServerInfo.h"
#include "../Game/GameServer/ServerSettings.h" // getServerNameString's local-server welcome message
#include "../Game/DemoRecorder/DemoLibrary.h"
#include "../Game/Persistence/LeaderBoard.h"       // fetchLeaderboard's key maps (non-virtual)
#include "../Game/Persistence/PlayerAwards.h"      // slot2A0 (swiffHost.addAward) -> IPlayerAwards +0x28
#include "../Game/Persistence/PlayerUnlocks.h"
#include "../Game/Persistence/PlayerUnlockInfo.h"
#include "../Game/Persistence/PlayerRankInfo.h"
#include "../Game/Persistence/IPlayerStats.h"   // the stats object behind fetchPlayerStats
#include "../Game/Main/BF2Engine.h"
#include "../Game/Main/BF2Window.h"
#include "../Game/Main/BF2ClientSeams.h"    // the ServerLogoManager / 0x9A4B10 seams
#include "../Game/Utility/BuildNrUtil.h"   // PE 4322E0 / ELF 8149CF0; shared cached build stamp
#include "IServerBrowser.h"                 // the named GameSpy lists
#include "IServerBrowserManager.h"
#include "../IO/File/FileManager.h"
#include "../IO/Console/Console.h"
#include "../IO/Input/Input.h"
#include "../IO/Localization/ExcelLexicon.h"
#include "../External/GameSpy/qr2/qr2regkeys.h"
#include "../External/GameSpy/common/gsAvailable.h"      // isGameSpyAvailable (+0x108)
#include "../External/GameSpy/common/gsPlatformUtil.h"   // msleep
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace dice { namespace hfe {

extern BF2Engine* bf2Engine;                // Core/Globals.cpp; PE dword_986F48
namespace io { extern IBase* controlMapManager; }   // Core/Globals.cpp; PE 0x9FFCB4

namespace {

// The file-scope statics of the SwiffHost translation unit and its neighbours, by PE address.
bool s_serverListsCreated = false;          // byte_9A6A20  init
bool s_creditsLoaded      = false;          // byte_9A6A21  loadCredits
bool s_ranksLoaded        = false;          // byte_9A6A22  loadRanks
bool s_cursorsLoaded      = false;          // byte_9A6B2C  getCursor
bool s_queryKeysRegistered = false;         // byte_9EBE88  registerBf2QueryKeys (its own TU, 0x5A0A80)
std::string s_selectedServerAddress;        // unk_9A6A04   setSelectedServer / getSelectedServer - the browser's selected row
int  s_selectedServerQueryPort = 0;         // dword_9A6708
int  s_selectedServerHostPort = 0;          // dword_9A670C
char s_resolvedAddress[256];                // unk_9A6A28   resolveHostName's answer
unsigned long s_lastInetAddr = 0;           // dword_9A6B28 isPrivateAddress's inet_addr result
#define SWIFF_HOST_FRONTIER(method, evidence)                                                        \
    do {                                                                                             \
        std::fprintf(stderr, "[FRONTIER] dice::hfe::SwiffHost::" method " reached (" evidence ")\n"); \
        std::fflush(stderr);                                                                         \
        std::abort();                                                                                \
    } while (0)

// Registers the BF2-specific GameSpy query keys once.
void registerBf2QueryKeys()
{
    if (s_queryKeysRegistered)
        return;
    qr2_register_key(51, "bf2_dedicated");
    qr2_register_key(52, "bf2_ranked");
    qr2_register_key(53, "bf2_anticheat");
    qr2_register_key(54, "bf2_os");
    qr2_register_key(55, "bf2_autorec");
    qr2_register_key(56, "bf2_d_idx");
    qr2_register_key(57, "bf2_d_dl");
    qr2_register_key(58, "bf2_voip");
    qr2_register_key(59, "bf2_autobalanced");
    qr2_register_key(60, "bf2_friendlyfire");
    qr2_register_key(61, "bf2_tkmode");
    qr2_register_key(62, "bf2_startdelay");
    qr2_register_key(63, "bf2_spawntime");
    qr2_register_key(64, "bf2_sponsortext");
    qr2_register_key(65, "bf2_sponsorlogo_url");
    qr2_register_key(66, "bf2_scorelimit");
    qr2_register_key(67, "bf2_ticketratio");
    qr2_register_key(68, "bf2_teamratio");
    qr2_register_key(79, "bf2_coopbotratio");
    qr2_register_key(80, "bf2_coopbotcount");
    qr2_register_key(81, "bf2_coopbotdiff");
    qr2_register_key(82, "bf2_novehicles");
    qr2_register_key(69, "bf2_team1");
    qr2_register_key(70, "bf2_team2");
    qr2_register_key(71, "bf2_bots");
    qr2_register_key(72, "bf2_pure");
    qr2_register_key(73, "bf2_mapsize");
    qr2_register_key(74, "bf2_globalunlocks");
    qr2_register_key(75, "bf2_fps");
    qr2_register_key(76, "bf2_communitylogo_url");
    qr2_register_key(77, "bf2_plasma");
    qr2_register_key(78, "bf2_reservedslots");
    s_queryKeysRegistered = true;
}

// the current mod's name - GSModDirectory's leaf after the last '/', lowercased
// (0x4312E0 is the Swedish-aware lower-casing this tree spells setLowerCase [INFERRED]).
std::string currentModName()
{
    std::string name = settings->stringGet(std::string("GSModDirectory"), std::string("(unknown)"));
    const std::string::size_type slash = name.find('/');
    if (slash != std::string::npos)
    {
        name.erase(0, slash + 1);
        setLowerCase(name);
    }
    return name;
}

// The console's file runner with the eight empty arguments every menu .con load passes
// (off_978F14 +0x24 with io::Console::ignoredString_ as the output sink).
void runMenuScript(const char* file)
{
    io::mainConsole->run(std::string(file), std::string(""), std::string(""), std::string(""),
                         std::string(""), std::string(""), std::string(""), std::string(""),
                         std::string(""), io::Console::ignoredString_);
}

// RFC 1918 test on a network-order address; a null pointer counts as private.
int isPrivateNetworkAddress(const unsigned long* address)
{
    if (!address)
        return 1;
    const unsigned long host = ntohl(*address);
    const unsigned long b0 = host >> 24;
    const unsigned long b1 = (host >> 16) & 0xFF;
    if (b0 == 10)
        return 1;
    if (b0 == 172)
    {
        if (b1 >= 0x10 && b1 <= 0x1F)
            return 1;
    }
    else if (b0 == 192 && b1 == 168)
    {
        return 1;
    }
    return 0;
}

} // namespace

// Construction (0x4A27D0), destruction (0x4A20B0), the factory (0x410244) and the quartet
SwiffHost::SwiffHost()
    : m_refCount(1), m_serverBrowserManager(0), m_loadingProgress(0.0f), m_flag28(true),
      m_playerStats(0), m_playerAwards(0), m_playerUnlocks(0), m_searchForPlayer(0), m_leaderBoard(0),
      m_demoLibrary(0), m_modInfo(0), m_playerUnlockInfo(0), m_serverLogoManager(0), m_menuTeamManager(0),
      m_playerRankInfo(0), m_messageId(0), m_messagesEnabled(true), m_pendingCode(0), m_selectedPlayerId(0),
      m_leaderboardCategory(0), m_leaderboardId(0), m_leaderboardPosition(1), m_flag7B8(false), m_gameStarting(false), m_intE58(0),
      m_viewportWidth(800), m_viewportHeight(600), m_flagE80(false), m_flagE81(true), m_intE94(10),
      m_intE98(0), m_flagEA8(false), m_timeEAC(0), m_busy(0), m_page(0), m_flagEC4(false), m_flagF04(false),
      m_intF08(1), m_floatF0C(500.0f), m_flagF10(false), m_intF14(0), m_flagF18(false), m_flagF2C(true),
      m_intF4C(0)
{
    // Initialize cache timestamps before registering query keys and resetting menu state.
    {
        time_t now;
        time(&now);
        m_unlockRecord.field0 = static_cast<int>(now);
    }
    m_unlockRecord.field4 = 0;
    m_unlockRecord.available = 0;
    m_unlockRecord.selected = 0;
    m_indexedInts[0] = 0;
    m_indexedInts[1] = 0;
    time(&m_timeEAC);
    registerBf2QueryKeys();
    m_cursors[0] = m_cursors[1] = m_cursors[2] = 0;
    m_gameMode = "gpm_cq";
    m_rect[0] = 0;
    m_rect[1] = 0;
    m_rect[2] = 1024;
    m_rect[3] = 768;
}

// the seven lazily created persistence objects are released (the singletons at +0x18,
// +0x40, +0x44 and +0x4C are not), the MenuTeamManager pointer is cleared, then the members go.
SwiffHost::~SwiffHost()
{
    if (m_playerStats)      m_playerStats->release();
    if (m_playerAwards)     m_playerAwards->release();
    if (m_playerUnlocks)    m_playerUnlocks->release();
    if (m_searchForPlayer)  m_searchForPlayer->release();
    if (m_leaderBoard)      m_leaderBoard->release();
    if (m_playerUnlockInfo) m_playerUnlockInfo->release();
    if (m_playerRankInfo)   m_playerRankInfo->release();
    m_menuTeamManager = 0;
}

IBase* SwiffHost::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    SwiffHost* host = new SwiffHost();
    IBase* result = static_cast<IBase*>(host->queryInterface(iid));
    if (!result)
        host->release();
    return result;
}

void  SwiffHost::addRef() { ++m_refCount; }
int   SwiffHost::getRef() { return m_refCount; }
int   SwiffHost::release()
{
    const int left = --m_refCount;
    if (!left)
        delete this;
    return left;
}
void* SwiffHost::queryInterface(unsigned int iid) const
{
    if (iid == 1 || iid == IID_ISwiffHost)
        return const_cast<SwiffHost*>(this);
    return 0;
}

// Creates the eight browser lists once. Favourites and history retain unanswered servers.
// Requires the browser manager and the named lists it creates.
bool SwiffHost::init()
{
    if (!s_serverListsCreated)
    {
        if (!m_serverBrowserManager)
        {
            m_serverBrowserManager = classManager->getSingleton(std::string("ServerBrowserManager"));
            if (!m_serverBrowserManager)
                return false;
        }
        ui::IServerBrowserManager* manager = static_cast<ui::IServerBrowserManager*>(m_serverBrowserManager);
        manager->createList(std::string("inet"), true);
        manager->createList(std::string("lan"), false);
        manager->createList(std::string("serverInfo"), true);
        manager->createList(std::string("playNow"), true);
        manager->createList(std::string("joinServerLan"), false);
        manager->createList(std::string("joinServerInet"), true);
        manager->createList(std::string("history"), true);
        manager->createList(std::string("favourites"), true);
        getServerList(std::string("favourites"))->setKeepUnansweredServers(true);
        getServerList(std::string("history"))->setKeepUnansweredServers(true);
        s_serverListsCreated = true;
    }
    return true;
}

// menu/external/flashmenu/credits.con, once.
bool SwiffHost::loadCredits()
{
    if (!s_creditsLoaded)
    {
        runMenuScript("menu/external/flashmenu/credits.con");
        s_creditsLoaded = true;
    }
    return true;
}

// Deletes the named control map and creates a replacement through ControlMapManager.
bool SwiffHost::recreateControlMap(const std::string& name)
{
    deleteControlMap(name);
    static_cast<io::IControlMapManager*>(io::controlMapManager)->create(name);
    return true;
}
bool SwiffHost::deleteControlMap(const std::string& name)
{
    static_cast<io::IControlMapManager*>(io::controlMapManager)->deleteControlMap(name);
    return true;
}

// the per-frame update SwiffPlayer::update makes - the +0x24 hook, the demo library,
// the server browser.
void SwiffHost::update()
{
    onMenuActivated();
    if (m_demoLibrary)
        static_cast<IDemoLibrary*>(m_demoLibrary)->update();
    if (m_serverBrowserManager)
        static_cast<ui::IServerBrowserManager*>(m_serverBrowserManager)->update();
}

void SwiffHost::onMenuActivated() {}                        // nullsub_3 - the shipped body is empty

// the last queued event is handed out and popped.
bool SwiffHost::pollEvent(MenuEvent& out)
{
    if (m_events.empty())
        return false;
    const MenuEvent& last = m_events.back();
    out.type = last.type;
    out.args = last.args;
    m_events.pop_back();
    return true;
}

// queued unless identical (type and arguments) to the last queued event.
// [UNRESOLVED] the value the queued path returns (0x49CB70's, an insert cursor); 0 here.
int SwiffHost::pushEvent(MenuEvent& event)
{
    if (m_events.empty() || m_events.back().type != event.type || !(m_events.back().args == event.args))
    {
        m_events.push_back(event);
        return 0;
    }
    return 1;
}

// +0x030..+0x054  the persistence objects (created by name, IID in hand) and the singletons
IBase* SwiffHost::getPlayerStats()
{
    if (!m_playerStats)
        m_playerStats = classManager->createInstance(std::string("dice.hfe.PlayerStats"), IID_IPlayerStats, 0);
    return m_playerStats;
}
IBase* SwiffHost::getPlayerAwards()
{
    if (!m_playerAwards)
    {
        m_playerAwards = classManager->createInstance(std::string("dice.hfe.PlayerAwards"), IID_IPlayerAwards, 0);
        runMenuScript("menu/external/flashmenu/awards.con");
    }
    return m_playerAwards;
}
IBase* SwiffHost::getPlayerUnlocks()
{
    if (!m_playerUnlocks)
        m_playerUnlocks = classManager->createInstance(std::string("dice.hfe.PlayerUnlocks"), IID_IPlayerUnlocks, 0);
    return m_playerUnlocks;
}
IBase* SwiffHost::getSearchForPlayer()
{
    if (!m_searchForPlayer)
        m_searchForPlayer = classManager->createInstance(std::string("dice.hfe.SearchForPlayer"), IID_ISearchForPlayer, 0);
    return m_searchForPlayer;
}
IBase* SwiffHost::getLeaderBoard()
{
    if (!m_leaderBoard)
        m_leaderBoard = classManager->createInstance(std::string("dice.hfe.LeaderBoard"), IID_ILeaderBoard, 0);
    return m_leaderBoard;
}
IBase* SwiffHost::getDemoLibrary()
{
    if (!m_demoLibrary)
    {
        m_demoLibrary = classManager->getSingleton(std::string("DemoLibrary"));
        static_cast<IDemoLibrary*>(m_demoLibrary)->refresh();   // unguarded in the binary
    }
    return m_demoLibrary;
}
IBase* SwiffHost::getPlayerUnlockInfo()
{
    if (!m_playerUnlockInfo)
    {
        m_playerUnlockInfo = classManager->createInstance(std::string("dice.hfe.PlayerUnlockInfo"), IID_IPlayerUnlockInfo, 0);
        static_cast<IPlayerUnlockInfo*>(m_playerUnlockInfo)->init();   // unguarded in the binary
    }
    return m_playerUnlockInfo;
}
IBase* SwiffHost::getServerLogoManager()
{
    if (!m_serverLogoManager)
    {
        m_serverLogoManager = classManager->getSingleton(std::string("ServerLogoManager"));
        if (!m_serverLogoManager)
            return 0;
        static_cast<IServerLogoManager*>(m_serverLogoManager)->init();   // +0x10, the constant-true stub
    }
    return m_serverLogoManager;
}
IBase* SwiffHost::getMenuTeamManager()
{
    if (!m_menuTeamManager)
    {
        m_menuTeamManager = classManager->getSingleton(std::string("MenuTeamManager"));
        if (m_menuTeamManager)
            static_cast<IMenuTeamManager*>(m_menuTeamManager)->init();
    }
    return m_menuTeamManager;
}
IBase* SwiffHost::getPlayerRankInfo()
{
    if (!m_playerRankInfo)
        m_playerRankInfo = classManager->createInstance(std::string("dice.hfe.PlayerRankInfo"), IID_IPlayerRankInfo, 0);
    return m_playerRankInfo;
}

// +0x058..+0x0B8  the running game, the cursors, names and the command-line file
std::string SwiffHost::getCurrentMapName()
{
    if (game)
        return std::string(game->getCurrentMapName());
    return std::string("$noCurrentGame");
}
std::string SwiffHost::getCurrentMapGameMode()
{
    if (game)
        return std::string(game->getCurrentMapGameMode());
    return std::string("$noCurrentGame");
}
int SwiffHost::getCurrentMapMaxPlayers() { return game ? game->getCurrentMapMaxPlayers() : 0; }
int SwiffHost::getCurrentMapSize()       { return game ? game->getCurrentMapSize() : 0; }

// SwiffPlayer +0x60, then the window stops hiding the cursor (+0x3B).
bool SwiffHost::setPlayerRenderCapability0(intptr_t value)
{
    IBase* player = classManager->getSingleton(std::string("SwiffPlayer"));
    if (!player || !static_cast<ui::ISwiffPlayer*>(player)->renderCapability0(value))
        return false;
    bf2Engine->window->m_hideCursor = false;
    return true;
}

// 0x488960 (326 B): the three .ani cursors, loaded once from their mod-relative files.
void* SwiffHost::getCursor(int which)
{
    if (s_cursorsLoaded)
        return m_cursors[which];
    std::string arrow = io::fileManager->fileExistsEx(std::string("menu/external/flashmenu/cursor/cursor_arrow.ani"), true);
    m_cursors[0] = LoadCursorFromFileA(arrow.c_str());
    std::string busy = io::fileManager->fileExistsEx(std::string("menu/external/flashmenu/cursor/cursor_busy.ani"), true);
    m_cursors[1] = LoadCursorFromFileA(busy.c_str());
    std::string keybinding = io::fileManager->fileExistsEx(std::string("menu/external/flashmenu/cursor/cursor_keybinding.ani"), true);
    m_cursors[2] = LoadImageA(GetModuleHandleA(0), keybinding.c_str(), IMAGE_CURSOR, 0, 0, LR_LOADFROMFILE | LR_SHARED);
    s_cursorsLoaded = true;
    return m_cursors[which];
}

bool SwiffHost::setPlayerRenderCapability1(intptr_t value)
{
    IBase* player = classManager->getSingleton(std::string("SwiffPlayer"));
    return player && static_cast<ui::ISwiffPlayer*>(player)->renderCapability1(value) != 0;
}
void SwiffHost::setFlag28(bool value) { m_flag28 = value; }
bool SwiffHost::getFlag28()           { return m_flag28; }

// ServerBrowserManager +0x14 by name (the manager fetched on demand, unguarded after).
ui::IServerBrowser* SwiffHost::getServerList(const std::string& name)
{
    if (!m_serverBrowserManager)
        m_serverBrowserManager = classManager->getSingleton(std::string("ServerBrowserManager"));
    return static_cast<ui::IServerBrowserManager*>(m_serverBrowserManager)->getList(name);
}

bool SwiffHost::getPlayerName(std::string& out, const std::string& fallback)
{
    out = settings->stringGet(std::string("GSPlayerName"), fallback);
    return true;
}
bool SwiffHost::setPlayerName(const std::string& name)
{
    settings->stringSet(std::string("GSPlayerName"), name);
    return true;
}

// 0x48A110 (328 B): CommandLine.cmd's whole text; a missing file posts a type-1 message.
bool SwiffHost::getCommandLineFile(std::string& out)
{
    out = "";
    io::FileInfo info;
    const bool missing = !io::fileManager->getFileInfo(std::string("CommandLine.cmd"), info);
    if (missing)
    {
        postMessage(SwiffHostMessage(1, std::string("unable to open commandline file"), 0));
        return false;
    }
    if (info.size)
    {
        char* buffer = new char[info.size + 1];
        std::memset(buffer, 0, info.size + 1);
        unsigned int bytesRead = 0;
        io::fileManager->readFile(std::string("CommandLine.cmd"), buffer, info.size, bytesRead);
        out = buffer;
        delete[] buffer;
    }
    return true;
}

// 0x493B90 (211 B): event 5 (parseParameters) with the text, then the text appended to the file.
bool SwiffHost::setCommandLineFile(const std::string& commandLine)
{
    MenuEvent event;
    event.type = 5;
    event.args.push_back(commandLine);
    pushEvent(event);
    io::fileManager->writeFile(std::string("CommandLine.cmd"), commandLine.c_str(),
                               static_cast<unsigned int>(std::strlen(commandLine.c_str())), true);
    return true;
}

void  SwiffHost::setLoadingProgress(float value) { m_loadingProgress = value; }
float SwiffHost::getLoadingProgress()
{
    // Returns cached progress when no load record exists; otherwise converts its percentage to 0..1.
    // Keep the separate percentage conversions to preserve floating-point rounding.
    if (!loadStat)
        return m_loadingProgress;

    float percent;
    if (loadStat->m_unknown0B)
    {
        percent = 100.0f;
    }
    else if (loadStat->m_unknown24 != 0)
    {
        if (static_cast<unsigned int>(loadStat->m_loadCounter)
            >= static_cast<unsigned int>(loadStat->m_unknown24))
        {
            loadStat->m_loadCounter = loadStat->m_unknown24;
        }
        const float fraction = static_cast<float>(static_cast<unsigned int>(loadStat->m_loadCounter))
                             / static_cast<float>(static_cast<unsigned int>(loadStat->m_unknown24));
        percent = (loadStat->m_unknown38
                 + fraction * (loadStat->m_unknown3C - loadStat->m_unknown38)) * 100.0f;
    }
    else
    {
        percent = 100.0f * loadStat->m_unknown38;
    }
    return percent * 0.01f;
}
int SwiffHost::getGameStatus()
{
    if (gameLogic && gameLogic->isLevelLoaded())
        return gameLogic->getGameStatus();
    return 1;
}
float SwiffHost::getTimeUntilRestartMap()
{
    if (!gameLogic)
        return 0.0f;
    return gameLogic->getTimeUntilRestartMap();
}
std::string SwiffHost::getLoadingRecordString()
{
    // copy the loading record's +0x0C current-file string, or an empty literal when
    // the singleton is absent. LoadStat::setCurrentFile is the sole writer during file loading.
    return loadStat ? loadStat->m_string0C : std::string("");
}

// the current mod's <sp_startup_map>, "gulf_of_oman" (and false) when it has none.
bool SwiffHost::getSpStartupMap(std::string& out)
{
    const IModInfo::Mod* mod = static_cast<IModInfo*>(getModInfo())->getCurrentMod();
    if (mod)
        out = mod->spStartupMap;
    if (!out.empty())
        return true;
    out = "gulf_of_oman";
    return false;
}
bool SwiffHost::getLocalizedString(std::wstring& out, const std::string& key)
{
    out = static_cast<io::ILexicon*>(io::lexicon)->getString(key);
    return true;
}
bool SwiffHost::requestEvent1()        { MenuEvent e; e.type = 1;  pushEvent(e); return true; }
bool SwiffHost::requestQuit()          { MenuEvent e; e.type = 2;  pushEvent(e); return true; }
bool SwiffHost::requestShutdownGame()  { MenuEvent e; e.type = 17; pushEvent(e); return true; }
bool SwiffHost::isGameRunning(bool* out)
{
    *out = game != 0;
    return game != 0;
}

// +0x0BC..+0x0E4  message id, frame label, flags and the join settings
int  SwiffHost::getMessageId()                       { return m_messageId; }
void SwiffHost::setMessageId(int id)                 { m_messageId = id; }
std::string SwiffHost::getFrameLabel()               { return m_frameLabel; }
void SwiffHost::setFrameLabel(const std::string& s)  { m_frameLabel = s; }
bool SwiffHost::getFlag7B8()                         { return m_flag7B8; }
void SwiffHost::setFlag7B8(bool value)               { m_flag7B8 = value; }
bool SwiffHost::getGameStarting()                    { return m_gameStarting; }
void SwiffHost::setGameStarting(bool value)          { m_gameStarting = value; }
std::string SwiffHost::getJoinAddressSetting()       { return settings->stringGet(std::string("GSJoinAddress"), std::string("")); }
std::string SwiffHost::getPasswordSetting()          { return settings->stringGet(std::string("GSPassword"), std::string("")); }
int SwiffHost::getPortSetting()
{
    const int fallback = 16567;
    return settings->intGet(std::string("GSPort"), fallback);
}

// Queues a host-server request for level's map and game mode and returns true. Requires a nonnull level.
bool SwiffHost::requestHostServer(const SwiffSinglePlayerLevel* level)
{
    MenuEvent event;
    event.type = 7;
    event.args.push_back(level->map);
    event.args.push_back(level->mode);
    pushEvent(event);
    return true;
}

// 0x496270 (931 B): the eight single-player rows, verbatim.
bool SwiffHost::getSinglePlayerLevels(std::vector<SwiffSinglePlayerLevel>& out)
{
    static const struct { int index; const char* name; const char* map; const char* mode; } rows[8] = {
        { 1, "US vs MEC - Wetlands Level 1",      "Wetlands",         "sp1" },
        { 2, "US vs MEC - Wetlands Level 2",      "Wetlands",         "sp2" },
        { 3, "US vs MEC - Wetlands Level 3",      "Wetlands",         "sp3" },
        { 4, "US vs MEC - Wetlands Level 4",      "Wetlands",         "mp"  },
        { 5, "MEC vs US - The Dam Level 1",       "the_dam",          "sp1" },
        { 6, "MEC vs US - Divided City Level 2",  "divided_city_day", "sp2" },
        { 7, "MEC vs US - Divided City Level 3",  "divided_city_day", "sp3" },
        { 8, "MEC vs US - Divided City Level 4",  "divided_city_day", "mp"  },
    };
    for (int i = 0; i < 8; ++i)
    {
        SwiffSinglePlayerLevel level;
        level.name  = rows[i].name;
        level.map   = rows[i].map;
        level.mode  = rows[i].mode;
        level.index = rows[i].index;
        out.push_back(level);
    }
    return true;
}
// Writes the map-list entry count to out and returns true.
bool SwiffHost::getMapListCount(int* out)
{
    *out = mapList->getMapCount();
    return true;
}

// +0x0F4..+0x0FC, +0x128..+0x130  the +0xEF4 server-entry vector
bool SwiffHost::clearServerEntries()
{
    std::vector<SwiffServerEntry>().swap(m_serverEntries);
    return true;
}
bool SwiffHost::setServerEntries(const std::vector<SwiffServerEntry>& entries)
{
    m_serverEntries = entries;
    return true;
}
bool SwiffHost::getServerEntries(std::vector<SwiffServerEntry>& out)
{
    out = m_serverEntries;
    return true;
}
bool SwiffHost::addServerEntry(const std::string& map, const std::string& gameMode, int size)
{
    SwiffServerEntry entry;
    entry.map = map;
    entry.gameMode = gameMode;
    entry.size = size;
    m_serverEntries.push_back(entry);
    return true;
}
bool SwiffHost::removeServerEntry(unsigned int index)
{
    if (index >= m_serverEntries.size())
        return false;
    m_serverEntries.erase(m_serverEntries.begin() + index);
    return true;
}
// 0x495FF0 (639 B): two working copies, the neighbour swap, then the vector refilled row by row.
bool SwiffHost::moveServerEntry(unsigned int index, bool up)
{
    if (m_serverEntries.empty())
        return false;
    if (index >= m_serverEntries.size())
        return false;
    if (m_serverEntries.size() <= 1)
        return false;
    std::vector<SwiffServerEntry> a(m_serverEntries);
    std::vector<SwiffServerEntry> b(m_serverEntries);
    if (!up)
    {
        if (index == m_serverEntries.size() - 1)
            return false;
        a[index]     = b[index + 1];
        a[index + 1] = b[index];
    }
    else
    {
        if (index == 0)
            return false;
        a[index]     = b[index - 1];
        a[index - 1] = b[index];
    }
    std::vector<SwiffServerEntry>().swap(m_serverEntries);
    for (std::size_t i = 0; i < a.size(); ++i)
        m_serverEntries.push_back(a[i]);
    return true;
}

// Resolves address and queues a server join. port is the game port; queryPort is the browser query port.
// A zero game port is queried or taken from settings. Returns false for a pending join, invalid address
// or missing query result. Success means the join was queued, not that a connection was established.
bool SwiffHost::joinServer(const std::string& address, int port, int queryPort)
{
    if (m_flagEC4)
        return false;
    resetEndOfRoundInfo();
    setPage(0);
    IBase* profiles = classManager->getSingleton(std::string("LocalProfileManager"));
    mapList->clear(true);                                          // 0x448FC0 on ds:0x9A4B10 - the MapList
    mapList->save();
    static_cast<ILocalProfileManager*>(profiles)->saveCurrentProfile();
    const std::string resolved = resolveHostName(address);
    if (resolved == "")
    {
        SwiffHostMessage message(3, std::string("invalidip"), 3);
        postMessage(message);
        return false;
    }
    IBase* serverSettings = classManager->getSingleton(std::string("GameServerSettings"));
    static_cast<IGameServerSettings*>(serverSettings)->apply();
    if (!port)
    {
        if (queryPort)
        {
            ui::IServerBrowser* list = getServerList(std::string("joinServerLan"));
            if (!list || !isPrivateAddress(resolved))
            {
                list = getServerList(std::string("joinServerInet"));
                if (!list)
                    return false;
            }
            list->reset();
            list->queryAddress(address, queryPort, false);
            list->update();
            if (!list->getRecords()->records.size())                // 0x4828D0, the null-tested count
                return false;
            if (list->getRecords()->records.size())                 // asked a second time, as shipped
            {
                ui::ServerBrowserRecord* record = list->getRecord(address, queryPort, true, true);
                if (!record)
                    return false;
                port = record->entry.hostPort;
            }
        }
        else
        {
            queryPort = 16567;                                       // the fallback lives in the query-port slot
            port = settings->intGet(std::string("GSPort"), queryPort);
        }
    }
    settings->intSet(std::string("GSPort"), port);
    MenuEvent event;
    event.type = 3;
    event.args.push_back(resolved);
    pushEvent(event);
    return true;
}
// Saves profile settings and queues hosting of the selected map.
// Returns false when the ordinary map list is empty or invalid; true means the host event was queued.
bool SwiffHost::hostServer()
{
    if (m_flagEC4 == 1)
        return false;
    resetEndOfRoundInfo();
    setPage(0);
    IBase* profiles = classManager->getSingleton(std::string("LocalProfileManager"));
    static_cast<ILocalProfileManager*>(profiles)->saveCurrentProfile();
    IBase* gameServerSettings = classManager->getSingleton(std::string("GameServerSettings"));
    static_cast<IGameServerSettings*>(gameServerSettings)->apply();
    IBase* generalSettings = classManager->getSingleton(std::string("GeneralSettings"));
    const bool commandLineDefault = false;
    if (settings->boolGet(std::string("MNCommandLine"), commandLineDefault))
    {
        const std::string level = settings->stringGet(std::string("GSLoadLevel"), std::string(""));
        std::string gameMode("gpm_cq");
        const unsigned int size = static_cast<unsigned int>(serverSettings->getMaxPlayers());
        const bool useAIDefault = false;
        if (settings->boolGet(std::string("UseAI"), useAIDefault) == 1)
        {
            if (size > 32)
                gameMode = "sp3";
            else if (size > 16)
                gameMode = "sp2";
            else
                gameMode = "sp1";
            if (compareIgnoreCase(getGameMode(), std::string("gpm_coop")) == 0)     // +0x164
                gameMode = "gpm_coop";
        }
        mapList->appendEntry(level, gameMode, size, true);
        mapList->save();
        mapList->setCurrentMap(0, true);
        settings->boolErase(std::string("MNCommandLine"));                        // +0x5C
    }
    else
    {
        mapList->clear(true);
        for (std::size_t i = 0; i < m_serverEntries.size(); ++i)
        {
            const std::string map(m_serverEntries[i].map);
            const std::string gameMode(m_serverEntries[i].gameMode);
            mapList->appendEntry(map, gameMode, m_serverEntries[i].size, true);
        }
        mapList->save();
        mapList->setCurrentMap(0, true);
        settings->stringErase(std::string("GSLoadLevel"));                        // +0x4C
        settings->stringErase(std::string("GSGameMode"));
        if (generalSettings)
        {
            const bool coop = compareIgnoreCase(getGameMode(), std::string("gpm_coop")) == 0;
            if (static_cast<IGeneralSettings*>(generalSettings)->getUseBots() || coop)   // +0xA8
            {
                const bool useAI = true;
                settings->boolSet(std::string("UseAI"), useAI);                   // +0x54
            }
            else
            {
                settings->boolErase(std::string("UseAI"));
            }
        }
        if (!mapList->getMapCount())
        {
            SwiffHostMessage message(3, std::string("nomapsselected"), 3);
            postMessage(message);
            return false;
        }
        mapList->setCurrentMap(0, true);
        const int current = mapList->getCurrentMap();
        if (current < 0 || current >= mapList->getMapCount())
        {
            SwiffHostMessage message(1, std::string("currentMap is not set"), 0);
            postMessage(message);
            return false;
        }
    }
    const std::string& gameMode = mapList->getEntry(mapList->getCurrentMap())->gameMode;
    const std::string& name = mapList->getEntry(mapList->getCurrentMap())->name;
    MenuEvent event;
    event.type = 4;
    event.args.push_back(name);
    event.args.push_back(gameMode);
    pushEvent(event);
    return true;
}
// Performs the blocking GameSpy availability check.
bool SwiffHost::isGameSpyAvailable()
{
    GSIStartAvailableCheck("battlefield2");
    GSIACResult result;
    for (result = GSIAvailableCheckThink(); result == GSIACWaiting; result = GSIAvailableCheckThink())
        msleep(5);
    return result == GSIACAvailable;
}
// the browser's selected row - Multiplay.setCurrentServerId splits the row id
// "<address> <queryPort> <hostPort>" into these three statics, clearServerId writes "0.0.0.0", 0, 0,
// and the sponsor rows, cacheExtendedServerInfo and the players / serverExtendedInfo lists read them
// back to fetch that server's record.
void SwiffHost::setSelectedServer(const std::string& address, int queryPort, int hostPort)
{
    s_selectedServerAddress = address;
    s_selectedServerQueryPort = queryPort;
    s_selectedServerHostPort = hostPort;
}
void SwiffHost::getSelectedServer(std::string& address, int* queryPort, int* hostPort)
{
    address = s_selectedServerAddress;
    *queryPort = s_selectedServerQueryPort;
    *hostPort = s_selectedServerHostPort;
}
int  SwiffHost::getIndexedInt(int index)            { return m_indexedInts[index]; }
void SwiffHost::setIndexedInt(int index, int value) { m_indexedInts[index] = value; }
// Returns the configured server address, query port and game port.
bool SwiffHost::getServerSettingsInfo(std::string& interfaceIP, int* queryPort, int* port)
{
    *queryPort = serverSettings->m_gameSpyPort;
    interfaceIP = serverSettings->m_interfaceIP;
    *port = serverSettings->m_serverPort;
    return true;
}
IBase* SwiffHost::getModInfo()
{
    if (!m_modInfo)
        m_modInfo = classManager->getSingleton(std::string("ModInfo"));
    return m_modInfo;
}
std::string SwiffHost::getModDirectorySetting()
{
    return settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2"));
}
std::string SwiffHost::getServerListKind()                    { return m_serverListKind; }
void SwiffHost::setServerListKind(const std::string& kind)    { m_serverListKind = kind; }
int  SwiffHost::getLeaderboardCategory()                      { return m_leaderboardCategory; }
void SwiffHost::setLeaderboardCategory(int category)          { m_leaderboardCategory = category; }
int  SwiffHost::getLeaderboardId()                            { return m_leaderboardId; }
void SwiffHost::setLeaderboardId(int id)                      { m_leaderboardId = id; }
int  SwiffHost::getLeaderboardPosition()                      { return m_leaderboardPosition; }
void SwiffHost::setLeaderboardPosition(int position)          { m_leaderboardPosition = position >= 1 ? position : 1; }

// Selects a player for the leaderboard view.
void SwiffHost::goLeaderboardPlayer(int playerId)
{
    if (!playerId)
        return;
    const int category = getLeaderboardCategory();                                // +0x13C
    const int board = getLeaderboardId();                                          // +0x144
    (void) getLeaderboardPosition();                                               // +0x14C
    if (category == -1 || board == -1 || playerId == -1)
        return;
    int request;
    switch (category)
    {
    case 0:  if ((unsigned int) board > 4)  return; request = board;      break;
    case 1:  if ((unsigned int) board > 6)  return; request = 5 + board;  break;
    case 2:  if ((unsigned int) board > 6)  return; request = 12 + board; break;
    case 3:  if ((unsigned int) board > 11) return; request = 19 + board; break;
    case 4:  request = 1; break;
    default: return;
    }
    SwiffLeaderboardCache cache;
    getLeaderboardPlayer(playerId, &cache, request);                               // +0x1D0
    const int position = getLeaderboardPlayerPosition(playerId, request) - 9;      // +0x158
    setLeaderboardPosition(position);                                              // +0x150
    fetchLeaderboard(0, request, position);                                        // +0x1D4
}

namespace {
// Row 0's "n" of a cached board, or 0 for an empty one (0x48CC00's size test, then *first).
template <class Row>
int firstRowPosition(const std::vector<Row>& rows)
{
    return rows.empty() ? 0 : rows[0].n;
}
}

// Returns the first cached row's position for a player, without fetching.
// Returns zero for no player, an uncached or empty board, or a request outside 0..30.
int SwiffHost::getLeaderboardPlayerPosition(int playerId, int request)
{
    if (!playerId)
        return 0;
    std::map<int, SwiffLeaderboardCache>::const_iterator found = m_leaderboardByPlayer.find(playerId);
    if (found == m_leaderboardByPlayer.end())
        return 0;
    const SwiffLeaderboardCache& cache = found->second;
    switch (request)
    {
    case 0:  return firstRowPosition(cache.overall.rows);
    case 1:  return firstRowPosition(cache.risingStar.rows);
    case 2:  return firstRowPosition(cache.commander.rows);
    case 3:  return firstRowPosition(cache.teamwork.rows);
    case 4:  return firstRowPosition(cache.combat.rows);
    default:
        if (request >= 5 && request <= 11)
            return firstRowPosition(cache.kits[request - 5].rows);
        if (request >= 12 && request <= 18)
            return firstRowPosition(cache.vehicles[request - 12].rows);
        if (request >= 19 && request <= 30)
            return firstRowPosition(cache.weapons[request - 19].rows);
        return 0;
    }
}
void SwiffHost::setIntE98(int value)                          { m_intE98 = value; }
int  SwiffHost::getIntE98()                                   { return m_intE98; }
std::string SwiffHost::getGameMode()                          { return m_gameMode; }
void SwiffHost::setGameMode(const std::string& mode)          { m_gameMode = mode; }
bool SwiffHost::playNow()
{
    SWIFF_HOST_FRONTIER("playNow", "PE 0x497E90, 718 B: the playNow ServerBrowser list and its 580-byte rows are unrecovered");
}
// Clears previous round results. [CHOSEN, not recovered] Returns zero; existing callers ignore the result.
int SwiffHost::resetEndOfRoundInfo()
{
    SwiffEndOfRoundInfo fresh;
    m_endOfRoundInfo = fresh;
    return 0;
}
// 0x4881E0: `lea eax, [ecx+0x7BC]`.
SwiffEndOfRoundInfo* SwiffHost::getEndOfRoundInfo()           { return &m_endOfRoundInfo; }
std::string SwiffHost::getTeamName(int team)
{
    if (gameLogic)
        return std::string(gameLogic->getTeamName(team + 1));
    return std::string("");
}
std::string SwiffHost::getJoinPassword()                      { return m_joinPassword; }
void SwiffHost::setJoinPassword(const std::string& password)  { m_joinPassword = password; }
// event 6 - BF2Engine::renderMenu plays it through IBfSound.
bool SwiffHost::playSound(const std::string& name)
{
    MenuEvent event;
    event.type = 6;
    event.args.push_back(name);
    pushEvent(event);
    return true;
}
void SwiffHost::slot18C(int)
{
    SWIFF_HOST_FRONTIER("slot18C", "PE 0x488920: ds:0xA23DC0 +0xB4(1, a) - the object is unidentified");
}
bool SwiffHost::getModCount(int* out)
{
    std::vector<IModInfo::Mod>* mods = static_cast<IModInfo*>(getModInfo())->getModList();
    *out = static_cast<int>(mods->size());
    return true;
}
bool SwiffHost::slot194(int, int)
{
    SWIFF_HOST_FRONTIER("slot194", "PE 0x4A2B00: 0x481490 on the indexed Mod is unrecovered");
}
// event 11 (activateMod), or the "no mod specified" type-1 message for a null name.
bool SwiffHost::requestActivateMod(const std::string* mod)
{
    if (mod)
    {
        MenuEvent event;
        event.type = 11;
        event.args.push_back(*mod);
        pushEvent(event);
        return true;
    }
    postMessage(SwiffHostMessage(1, std::string("no mod specified"), 0));
    return false;
}
// event 10 (browseModHomepage) with the mod's url (PE +112 of the Mod record).
bool SwiffHost::requestBrowseModHomepage(const IModInfo::Mod* mod)
{
    if (mod)
    {
        MenuEvent event;
        event.type = 10;
        event.args.push_back(mod->url);
        pushEvent(event);
        return true;
    }
    postMessage(SwiffHostMessage(1, std::string("no mod specified"), 0));
    return false;
}
// Writes the protocol build number to out and returns true.
bool SwiffHost::getVersionString(std::string& out)
{
    out = BuildNrUtil::getBuildNrString();
    return true;
}
// +0x1A4 getEndOfRoundSummary (0x499380) is in Menu/SwiffHostEndOfRound.cpp - it copies the
// end-of-round record's team rows, not the stats cache.
bool SwiffHost::isGameLogicServer()                           { return gameLogic && gameLogic->isServer(); }
namespace {
// The readers of PE 0x49D450: a key through IPlayerStats::getKeyValue(0, key) - +0x24 on the host's
// stats object - as an integer through atoi, or as the wire text itself.
int statInt(IPlayerStats* stats, const std::string& key)
{
    return atoi(stats->getKeyValue(0, key).c_str());
}
std::string statText(IPlayerStats* stats, const std::string& key)
{
    return stats->getKeyValue(0, key);
}
// The first getplayerinfo.aspx query of 0x49D450, verbatim.
const char kPlayerInfoQuery[] =
    "&info=per*,cmb*,twsc,cpcp,cacp,dfcp,kila,heal,rviv,rsup,rpar,tgte,dkas,dsab,cdsc,rank,cmsc,kick,kill,deth,suic,ospm,"
    "klpm,klpr,dtpr,bksk,wdsk,bbrs,tcdr,ban,dtpm,lbtl,osaa,vrk,tsql,tsqm,tlwf,mvks,vmks,mvn*,vmr*,fkit,fmap,fveh,fwea,wtm"
    "-,wkl-,wdt-,wac-,wkd-,vtm-,vkl-,vdt-,vkd-,vkr-,atm-,awn-,alo-,abr-,ktm-,kkl-,kdt-,kkd-";
// The 31 map ids the third query is read for, in the shipped order.
const char* const kStatsMapIds[31] = {
    "0", "1", "2", "3", "4", "5", "6", "10", "11", "12", "100", "101", "102", "103", "105", "110",
    "200", "201", "202", "300", "301", "302", "303", "304", "305", "306", "307", "601", "602", "603", "604" };
}

// 0x4877A0 (in full): the key's value through the stats object, atol, and when it is not zero the
// date it names through formatDate (+0x22C); "-" otherwise.
std::string SwiffHost::statDate(const std::string& key)
{
    IPlayerStats* stats = static_cast<IPlayerStats*>(getPlayerStats());
    const long value = atol(stats->getKeyValue(0, key).c_str());
    if (value)
    {
        time_t stamp = value;
        return formatDate(&stamp);
    }
    return std::string("-");
}
std::string SwiffHost::favouriteTime(IPlayerStats* stats, int favourite, int keyIndex)
{
    if (favourite == -1)
        return std::string("-");
    std::string key, value;
    (void) stats->getKey(0, keyIndex, key, value);
    return formatDuration(atoi(value.c_str()));
}

// Fetches player statistics into the host cache.
bool SwiffHost::fetchPlayerStats(int playerId)
{
    if (!playerId)
        return true;
    int cached = -1;
    for (std::size_t i = 0; i < m_playerStatsRecords.size(); i++)
    {
        if (m_playerStatsRecords[i].playerId == playerId)
        {
            cached = (int) i;
            break;
        }
    }
    SwiffPlayerStatsRecord rec;
    rec.playerId = playerId;
    time(&rec.fetchedAt);
    IPlayerStats* stats = static_cast<IPlayerStats*>(getPlayerStats());
    if (!stats->getStats(playerId, std::string(kPlayerInfoQuery), false, false))
        return false;

    rec.tsql = formatDuration(statInt(stats, "tsql"));
    rec.tsqm = formatDuration(statInt(stats, "tsqm"));
    rec.tlwf = formatDuration(statInt(stats, "tlwf"));
    rec.nick = statText(stats, "nick");
    rec.scor = statInt(stats, "scor");
    rec.wins = statInt(stats, "wins");
    rec.loss = statInt(stats, "loss");
    rec.jond = statDate(std::string("jond"));
    rec.lbtl = statDate(std::string("lbtl"));
    rec.twsc = statInt(stats, "twsc");
    rec.cpcp = statInt(stats, "Cpcp");
    rec.cacp = statInt(stats, "Cacp");
    rec.dfcp = statInt(stats, "Dfcp");
    rec.kila = statInt(stats, "kila");
    rec.heal = statInt(stats, "heal");
    rec.rviv = statInt(stats, "rviv");
    rec.rsup = statInt(stats, "rsup");
    rec.rpar = statInt(stats, "rpar");
    rec.tgte = statInt(stats, "tgte");
    {
        const int dkas = statInt(stats, "dkas");
        const int dsab = statInt(stats, "dsab");
        rec.dkas_dsab = dkas + dsab;
    }
    rec.cdsc = statInt(stats, "cdsc");
    rec.rank = statInt(stats, "rank");
    rec.cmsc = statInt(stats, "cmsc");
    rec.kick = statInt(stats, "kick");
    rec.ban  = statInt(stats, "ban");
    rec.kill = statInt(stats, "kill");
    rec.deth = statInt(stats, "deth");
    rec.suic = statInt(stats, "suic");
    rec.ospm = statText(stats, "ospm");
    rec.klpm = statText(stats, "klpm");
    rec.dtpm = statText(stats, "dtpm");
    rec.klpr = statText(stats, "klpr");
    rec.dtpr = statText(stats, "dtpr");
    rec.bksk = statInt(stats, "bksk");
    rec.wdsk = statInt(stats, "wdsk");
    rec.bbrs = statInt(stats, "bbrs");
    rec.tcdr = formatDuration(statInt(stats, "tcdr"));
    rec.osaa = statInt(stats, "osaa");
    rec.vrk  = statInt(stats, "vrk");
    rec.mvks = statInt(stats, "mvks");
    rec.vmks = statInt(stats, "vmks");
    rec.de6  = statInt(stats, "de-6");
    rec.de7  = statInt(stats, "de-7");
    rec.de8  = statInt(stats, "de-8");
    rec.mvns = statText(stats, "mvns");
    rec.mvrs = statInt(stats, "mvrs");
    rec.vmns = statText(stats, "vmns");
    rec.vmrs = statInt(stats, "vmrs");
    rec.fkit = statInt(stats, "fkit");
    rec.fmap = statInt(stats, "fmap");
    rec.fveh = statInt(stats, "fveh");
    rec.fwea = statInt(stats, "fwea");
    for (int k = 0; k < 19; k++)
    {
        SwiffPlayerStatsWeapon& w = rec.weapons[k];
        w.time     = formatDuration(statInt(stats, formatString("wtm-%d", k)));
        w.kills    = statInt(stats, formatString("wkl-%d", k));
        w.deaths   = statInt(stats, formatString("wdt-%d", k));
        w.accuracy = statInt(stats, formatString("wac-%d", k));
        w.kd       = statText(stats, formatString("wkd-%d", k));
    }
    for (int k = 0; k < 13; k++)
    {
        SwiffPlayerStatsVehicle& v = rec.vehicles[k];
        v.time      = formatDuration(statInt(stats, formatString("vtm-%d", k)));
        v.kills     = statInt(stats, formatString("vkl-%d", k));
        v.deaths    = statInt(stats, formatString("vdt-%d", k));
        v.kd        = statText(stats, formatString("vkd-%d", k));
        v.roadKills = statInt(stats, formatString("vkr-%d", k));
    }
    for (int k = 0; k < 10; k++)
    {
        SwiffPlayerStatsArmy& a = rec.armies[k];
        a.time      = formatDuration(statInt(stats, formatString("atm-%d", k)));
        a.wins      = statInt(stats, formatString("awn-%d", k));
        a.losses    = statInt(stats, formatString("alo-%d", k));
        a.bestRound = statInt(stats, formatString("abr-%d", k));
    }
    for (int k = 0; k < 7; k++)
    {
        SwiffPlayerStatsKit& kit = rec.kits[k];
        kit.time   = formatDuration(statInt(stats, formatString("ktm-%d", k)));
        kit.kills  = statInt(stats, formatString("kkl-%d", k));
        kit.deaths = statInt(stats, formatString("kdt-%d", k));
        kit.kd     = statText(stats, formatString("kkd-%d", k));
    }

    std::string query = formatString("&info=ktm-,vtm-,wtm-,mtm-&kit=%d&vehicle=%d&weapon=%d&map=%d",
                                     rec.fkit == -1 ? 0 : rec.fkit, rec.fveh == -1 ? 0 : rec.fveh,
                                     rec.fwea == -1 ? 0 : rec.fwea, rec.fmap == -1 ? 0 : rec.fmap);
    if (!stats->getStats(playerId, query, false, false))
        return false;
    rec.fkitTime = favouriteTime(stats, rec.fkit, 2);
    rec.fvehTime = favouriteTime(stats, rec.fveh, 3);
    rec.fweaTime = favouriteTime(stats, rec.fwea, 4);
    rec.fmapTime = favouriteTime(stats, rec.fmap, 5);

    rec.maps.clear();
    query = "&info=mtm-,mwn-,mls-";
    if (!stats->getStats(playerId, query, false, false))
        return false;
    if (stats->getNumKeyMaps())
    {
        for (int m = 0; m < 31; m++)
        {
            const char* id = kStatsMapIds[m];
            SwiffPlayerStatsMapRow row;
            row.id = atoi(id);
            const long seconds = atol(stats->getKeyValue(0, formatString("mtm-%s", id)).c_str());
            row.time   = seconds ? formatDuration((int) seconds) : std::string("-");
            row.wins   = statInt(stats, formatString("mwn-%s", id));
            row.losses = statInt(stats, formatString("mls-%s", id));
            rec.maps.push_back(row);
        }
    }
    if (cached == -1)
        m_playerStatsRecords.push_back(rec);
    else
        m_playerStatsRecords[cached] = rec;
    return true;
}

// Copies the player's stats to out, refreshing entries older than an hour.
// Returns false if no usable record is available. A zero playerId returns true without writing out.
bool SwiffHost::getPlayerStatsRecord(int playerId, SwiffPlayerStatsRecord& out)
{
    if (!playerId)
        return true;
    for (std::size_t i = 0; i < m_playerStatsRecords.size(); i++)
    {
        if (m_playerStatsRecords[i].playerId != playerId)
            continue;
        time_t now = 0;
        time(&now);
        const bool fresh = m_playerStatsRecords[i].playerId == playerId
                        && difftime(now, m_playerStatsRecords[i].fetchedAt) < 3600.0;
        if (fresh || fetchPlayerStats(playerId))
        {
            out = m_playerStatsRecords[i];
            return true;
        }
        return false;
    }
    (void) fetchPlayerStats(playerId);
    for (std::size_t i = 0; i < m_playerStatsRecords.size(); i++)
    {
        if (m_playerStatsRecords[i].playerId == playerId)
        {
            out = m_playerStatsRecords[i];
            return true;
        }
    }
    return false;
}

// +0x1B4..+0x1C0, +0x29C, +0x3B4  the rank table (ranks.con)
std::string SwiffHost::getRankName(int id)
{
    loadRanks();
    std::map<int, SwiffRankRecord>::iterator it = m_ranks.find(id);
    if (it == m_ranks.end())
        return std::string("-");
    return it->second.name;
}
int SwiffHost::getRankB(int id)
{
    loadRanks();
    std::map<int, SwiffRankRecord>::iterator it = m_ranks.find(id);
    return it == m_ranks.end() ? 0 : it->second.b;
}
int SwiffHost::getRankC(int id)
{
    loadRanks();
    std::map<int, SwiffRankRecord>::iterator it = m_ranks.find(id);
    return it == m_ranks.end() ? 0 : it->second.c;
}
int SwiffHost::getRankCount()                                  { return static_cast<int>(m_ranks.size()); }
void SwiffHost::addRank(int id, int a, int b, int c, const std::string& name)
{
    loadRanks();
    SwiffRankRecord& record = m_ranks[id];
    record.a = a;
    record.b = b;
    record.c = c;
    record.name = name;
}
bool SwiffHost::loadRanks()
{
    if (!s_ranksLoaded)
    {
        runMenuScript("menu/external/flashmenu/ranks.con");
        s_ranksLoaded = true;
    }
    return true;
}

// +0x1C4..+0x1D4  the unlock record and the stats fetches
// the cached record when it is this player's and under an hour old, else a fetch.
bool SwiffHost::getUnlocks(int playerId, IPlayerUnlocks::UnlockCache* out)
{
    time_t now;
    time(&now);
    if ((m_unlockRecord.field4 == playerId && difftime(now, static_cast<time_t>(m_unlockRecord.field0)) < 3600.0)
        || fetchUnlocks(playerId))
    {
        out->field0    = m_unlockRecord.field0;
        out->field4    = m_unlockRecord.field4;
        out->available = m_unlockRecord.available;
        out->selected  = m_unlockRecord.selected;
        out->items     = m_unlockRecord.items;
        return true;
    }
    return false;
}
// PlayerUnlocks fills the record; on success the id and the time are stamped.
bool SwiffHost::fetchUnlocks(int playerId)
{
    IBase* unlocks = getPlayerUnlocks();
    const bool result = static_cast<IPlayerUnlocks*>(unlocks)->getUnlocks(playerId, m_unlockRecord);
    if (result)
    {
        m_unlockRecord.field4 = playerId;
        time_t now;
        time(&now);
        m_unlockRecord.field0 = static_cast<int>(now);
        return true;
    }
    return result;
}
// Visits the cache group selected by the public leaderboard request number.
namespace {
template <class Visitor>
void visitLeaderboardGroup(SwiffLeaderboardCache& cache, int request, Visitor visit)
{
    switch (request)
    {
    case 0: visit(cache.overall);    return;
    case 1: visit(cache.risingStar); return;
    case 2: visit(cache.commander);  return;
    case 3: visit(cache.teamwork);   return;          // request 3 is group 4
    case 4: visit(cache.combat);     return;          // request 4 is group 3
    default:
        if (request >= 5 && request <= 11)
            visit(cache.kits[request - 5]);
        else if (request >= 12 && request <= 18)
            visit(cache.vehicles[request - 12]);      // groups 24..30
        else if (request >= 19 && request <= 30)
            visit(cache.weapons[request - 19]);       // groups 12..23
        return;                                       // anything else: no group
    }
}
} // namespace

// Refreshes the requested group when empty, cached for another position, or more than an hour past
// cache construction. Copies the entire cache to out and returns true, including for unknown requests.
bool SwiffHost::getLeaderboard(SwiffLeaderboardCache* out, int request, int position)
{
    time_t now;
    time(&now);
    visitLeaderboardGroup(m_leaderboard, request, [&](const auto& group) {
        if (group.rows.empty() || difftime(now, group.createdAt) >= 3600.0 || position != m_leaderboard.position)
            fetchLeaderboard(0, request, position);                                // +0x1D4
    });
    *out = m_leaderboard;
    return true;
}

// Creates or refreshes the player's requested board and copies the entire player cache to out.
// Refreshes empty boards, a different player's first row, or caches over an hour past construction.
bool SwiffHost::getLeaderboardPlayer(int playerId, SwiffLeaderboardCache* out, int request)
{
    time_t now;
    time(&now);
    SwiffLeaderboardCache* cache;
    std::map<int, SwiffLeaderboardCache>::iterator found = m_leaderboardByPlayer.find(playerId);
    if (found == m_leaderboardByPlayer.end())
    {
        SwiffLeaderboardCache fresh;
        m_leaderboardByPlayer[playerId] = fresh;
        cache = &m_leaderboardByPlayer.find(playerId)->second;
    }
    else
    {
        cache = &found->second;
    }
    if (!cache)
        return false;
    visitLeaderboardGroup(*cache, request, [&](const auto& group) {
        if (group.rows.empty() || group.rows[0].pid != playerId || difftime(now, group.createdAt) >= 3600.0)
            fetchLeaderboard(playerId, request, 0);                                // +0x1D4
    });
    *out = *cache;
    return true;
}

// fetchLeaderboard's key readers. Each asks the lazy getter again
// (+0x40, a table call) and looks the key up twice: once to test, once to read.
int SwiffHost::leaderboardInt(int row, const std::string& key)
{
    LeaderBoard* board = static_cast<LeaderBoard*>(static_cast<ILeaderBoard*>(getLeaderBoard()));
    if (!board->getKeyMap(row)->findKey(key))
        return 0;
    return atoi(board->getKeyMap(row)->findKey(key)->value.c_str());
}
std::string SwiffHost::leaderboardString(int row, const std::string& key)
{
    LeaderBoard* board = static_cast<LeaderBoard*>(static_cast<ILeaderBoard*>(getLeaderBoard()));
    if (board->getKeyMap(row)->findKey(key))
        return std::string(board->getKeyMap(row)->findKey(key)->value);
    return std::string("");
}
double SwiffHost::leaderboardFloat(int row, const std::string& key)
{
    LeaderBoard* board = static_cast<LeaderBoard*>(static_cast<ILeaderBoard*>(getLeaderBoard()));
    if (!board->getKeyMap(row)->findKey(key))
        return 0.0;
    return atof(board->getKeyMap(row)->findKey(key)->value.c_str());
}

// Fetches a leaderboard group into the host cache.
bool SwiffHost::fetchLeaderboard(int playerId, int request, int position)
{
    ILeaderBoard* const board = static_cast<ILeaderBoard*>(getLeaderBoard());      // +0x40
    SwiffLeaderboardCache* playerCache = 0;
    if (playerId)
    {
        if (m_leaderboardByPlayer.find(playerId) == m_leaderboardByPlayer.end())
            return false;
        playerCache = &m_leaderboardByPlayer[playerId];
    }
    SwiffLeaderboardCache& cache = playerCache ? *playerCache : m_leaderboard;

    if (request >= 0 && request <= 30)
    {
        static const char* const kScoreQueries[5] = {
            "type=score&id=overall", "type=risingstar", "type=score&id=commander", "type=score&id=team",
            "type=score&id=combat" };
        std::string query;
        if (request <= 4)
            query = kScoreQueries[request];
        else if (request <= 11)
            query = formatString("type=kit&id=%d", request - 5);
        else if (request <= 18)
            query = formatString("type=vehicle&id=%d", request - 12);
        else
            query = formatString("type=weapon&id=%d", request - 19);
        const bool sent = playerId
            ? board->getLeaderBoardPlayer(playerId, query, false)                       // +0x14
            : board->getLeaderBoard(query + formatString("&pos=%d&before=0&after=19", position), false);   // +0x10
        if (!sent)
            return false;

        // getNumKeyMaps is LeaderBoard's own (non-virtual, 0x47B490), asked again on every pass.
        LeaderBoard* const keys = static_cast<LeaderBoard*>(board);
        // The group header, the same three steps for every family.
        const auto refill = [&](auto& group) {
            group.rows.clear();
            group.numPlayers = board->getNumPlayers();                             // +0x1C
            time_t updated = board->getUpdated();                                   // +0x18
            group.updated = formatDateTime(&updated);
        };
        if (request == 0 || request == 2 || request == 3)
        {
            // 76-byte rows: overall ("score", "totaltime"), commander ("coscore", "cotime"),
            // teamwork ("teamscore", "totaltime").
            SwiffLeaderboardGroup<SwiffLeaderboardScoreRow>& group =
                request == 0 ? cache.overall : request == 2 ? cache.commander : cache.teamwork;
            const char* const first  = request == 0 ? "score" : request == 2 ? "coscore" : "teamscore";
            const char* const second = request == 2 ? "cotime" : "totaltime";
            refill(group);
            for (unsigned int i = 0; i < (unsigned int) keys->getNumKeyMaps(); ++i)
            {
                SwiffLeaderboardScoreRow row;
                row.n           = leaderboardInt(i, std::string("n"));
                row.pid         = leaderboardInt(i, std::string("pid"));
                row.nick        = leaderboardString(i, std::string("nick"));
                row.values[0]   = leaderboardInt(i, std::string(first));
                row.values[1]   = leaderboardInt(i, std::string(second));
                row.playerRank  = leaderboardInt(i, std::string("playerrank"));
                row.countryCode = leaderboardString(i, std::string("countrycode"));
                group.rows.push_back(row);
            }
        }
        else if (request == 1)
        {
            SwiffLeaderboardGroup<SwiffLeaderboardRisingStarRow>& group = cache.risingStar;
            refill(group);
            for (unsigned int i = 0; i < (unsigned int) keys->getNumKeyMaps(); ++i)
            {
                SwiffLeaderboardRisingStarRow row;
                row.n           = leaderboardInt(i, std::string("n"));
                row.pid         = leaderboardInt(i, std::string("pid"));
                row.nick        = leaderboardString(i, std::string("nick"));
                row.weeklyScore = formatString("%.2f", leaderboardFloat(i, std::string("weeklyscore"))) + "%";
                row.totalTime   = leaderboardInt(i, std::string("totaltime"));
                row.date        = leaderboardString(i, std::string("date"));
                row.playerRank  = leaderboardInt(i, std::string("playerrank"));
                row.countryCode = leaderboardString(i, std::string("countrycode"));
                group.rows.push_back(row);
            }
        }
        else if (request <= 18)
        {
            // 80-byte rows: combat (request 4: "score", "totalkills", "totaltime"), kits
            // ("killswith", "deathsby", "timeplayed"), vehicles ("killswith", "detahsby" [sic],
            // "timeused").
            SwiffLeaderboardGroup<SwiffLeaderboardRow>& group =
                request == 4 ? cache.combat : request <= 11 ? cache.kits[request - 5] : cache.vehicles[request - 12];
            const char* const k0 = request == 4 ? "score"      : "killswith";
            const char* const k1 = request == 4 ? "totalkills" : request <= 11 ? "deathsby"   : "detahsby";
            const char* const k2 = request == 4 ? "totaltime"  : request <= 11 ? "timeplayed" : "timeused";
            refill(group);
            for (unsigned int i = 0; i < (unsigned int) keys->getNumKeyMaps(); ++i)
            {
                SwiffLeaderboardRow row;
                row.n           = leaderboardInt(i, std::string("n"));
                row.pid         = leaderboardInt(i, std::string("pid"));
                row.nick        = leaderboardString(i, std::string("nick"));
                row.values[0]   = leaderboardInt(i, std::string(k0));
                row.values[1]   = leaderboardInt(i, std::string(k1));
                row.values[2]   = leaderboardInt(i, std::string(k2));
                row.playerRank  = leaderboardInt(i, std::string("playerrank"));
                row.countryCode = leaderboardString(i, std::string("countrycode"));
                group.rows.push_back(row);
            }
        }
        else
        {
            // 84-byte weapon rows: "killswith", "detahsby" [sic], "timeused", "accuracy".
            SwiffLeaderboardGroup<SwiffLeaderboardWeaponRow>& group = cache.weapons[request - 19];
            refill(group);
            for (unsigned int i = 0; i < (unsigned int) keys->getNumKeyMaps(); ++i)
            {
                SwiffLeaderboardWeaponRow row;
                row.n           = leaderboardInt(i, std::string("n"));
                row.pid         = leaderboardInt(i, std::string("pid"));
                row.nick        = leaderboardString(i, std::string("nick"));
                row.values[0]   = leaderboardInt(i, std::string("killswith"));
                row.values[1]   = leaderboardInt(i, std::string("detahsby"));
                row.values[2]   = leaderboardInt(i, std::string("timeused"));
                row.values[3]   = leaderboardInt(i, std::string("accuracy"));
                row.playerRank  = leaderboardInt(i, std::string("playerrank"));
                row.countryCode = leaderboardString(i, std::string("countrycode"));
                group.rows.push_back(row);
            }
        }
    }
    cache.position = position;
    return true;
}
void SwiffHost::setViewportSize(int width, int height)
{
    m_viewportWidth = width;
    m_viewportHeight = height;
}
void SwiffHost::getViewportSize(unsigned int* width, unsigned int* height)
{
    *width = static_cast<unsigned int>(m_viewportWidth);
    *height = static_cast<unsigned int>(m_viewportHeight);
}
// [CHOSEN] Copies the system-info singleton's computer name to out and returns true.
bool SwiffHost::getComputerName(std::string& out)
{
    out = systemInfo()->getComputerName();
    return true;
}

// Adds a message to the five-entry queue, discarding the oldest entry when full.
// Returns false while messages are disabled.
bool SwiffHost::postMessage(const SwiffHostMessage& message)
{
    const bool enabled = areMessagesEnabled();
    if (!enabled)
        return enabled;
    SwiffHostMessage record(message.type, message.text, message.code);
    if (record.text == "The password does not match the email address.")
        record.text = "An account name already exists for that email address/password. Please try again with a different email address or password.";
    if (m_messages.size() >= 5)
    {
        for (std::size_t i = 1; i < m_messages.size(); ++i)
        {
            m_messages[i - 1].type = m_messages[i].type;
            m_messages[i - 1].code = m_messages[i].code;
            m_messages[i - 1].text = m_messages[i].text;
        }
        m_messages[4].type = record.type;
        m_messages[4].code = record.code;
        m_messages[4].text = record.text;
    }
    else
    {
        m_messages.push_back(record);
    }
    return enabled;
}
int SwiffHost::getMessageCount()                              { return static_cast<int>(m_messages.size()); }
// the index-th message of a type, else a function-static empty record.
SwiffHostMessage* SwiffHost::getMessageByType(int index, int type)
{
    int seen = 0;
    for (std::size_t i = 0; i < m_messages.size(); ++i)
    {
        if (m_messages[i].type != type)
            continue;
        if (seen == index)
            return &m_messages[i];
        ++seen;
    }
    static SwiffHostMessage empty;
    return &empty;
}
SwiffHostMessage* SwiffHost::getMessage(int index)            { return m_messages.data() + index; }   // unchecked, as shipped
bool SwiffHost::removeMessageByType(int index, int type)
{
    int seen = 0;
    for (std::size_t i = 0; i < m_messages.size(); ++i)
    {
        if (m_messages[i].type != type)
            continue;
        if (seen == index)
        {
            m_messages.erase(m_messages.begin() + i);
            return true;
        }
        ++seen;
    }
    return false;
}
bool SwiffHost::removeMessage(unsigned int index)
{
    if (index >= m_messages.size())
        return false;
    m_messages.erase(m_messages.begin() + index);
    return true;
}
void SwiffHost::clearMessages()                               { std::vector<SwiffHostMessage>().swap(m_messages); }
int SwiffHost::countMessagesByType(int type)
{
    int count = 0;
    for (std::size_t i = 0; i < m_messages.size(); ++i)
        if (m_messages[i].type == type)
            ++count;
    return count;
}
bool SwiffHost::popLastMessage(SwiffHostMessage& out, bool remove)
{
    if (m_messages.empty())
        return false;
    out.type = m_messages.back().type;
    out.code = m_messages.back().code;
    out.text = m_messages.back().text;
    if (remove)
        m_messages.pop_back();
    return true;
}
bool SwiffHost::removeLastMessage()
{
    if (m_messages.empty())
        return false;
    m_messages.pop_back();
    return true;
}
bool SwiffHost::setMessagesEnabled(bool enabled)
{
    m_messagesEnabled = enabled;
    clearMessages();
    return m_messagesEnabled;
}
bool SwiffHost::areMessagesEnabled()                          { return m_messagesEnabled; }
bool SwiffHost::getPendingRecord(int* outCode, std::string& outText)
{
    if (!m_pendingCode)
        return false;
    *outCode = m_pendingCode;
    outText = m_pendingText;
    return true;
}
void SwiffHost::setPendingRecord(int code, const std::string& text)
{
    m_pendingCode = code;
    m_pendingText = text;
}
int  SwiffHost::getSelectedPlayerId()                         { return m_selectedPlayerId; }
void SwiffHost::setSelectedPlayerId(int id)
{
    // PE494410..494486: the id is stored before either arm. Logout's zero-id
    // path only clears the name; a nonzero id then constructs the stats record.
    m_selectedPlayerId = id;
    if (!id) {
        m_selectedPlayerName = "";
        return;
    }
    // a record on the stack (0x492EC0), the +0x1B0 cache asked for the id - its answer
    // unread - and the record's nick becomes the selected name, empty when nothing came back.
    SwiffPlayerStatsRecord record;
    (void) getPlayerStatsRecord(id, record);
    m_selectedPlayerName = record.nick;
}
std::string SwiffHost::getSelectedPlayerName()                { return m_selectedPlayerName; }
// Copies cached awards to out, refreshing entries older than one hour.
// Returns false for a failed uncached fetch; a failed refresh still returns true with the emptied cached list.
bool SwiffHost::getCachedAwards(int profileId, IPlayerAwards::AwardList* out)
{
    time_t now;
    time(&now);
    for (unsigned int i = 0; i < m_awardCache.size(); ++i)
    {
        if (m_awardCache[i].profileId == profileId)
        {
            if (difftime(now, static_cast<time_t>(m_awardCache[i].timestamp)) >= 3600.0)
                static_cast<IPlayerAwards*>(getPlayerAwards())->getAwards(profileId, &m_awardCache[i], false, false);
            out->timestamp = m_awardCache[i].timestamp;
            out->profileId = m_awardCache[i].profileId;
            out->awards    = m_awardCache[i].awards;
            return true;
        }
    }
    // The shipped local leaves the time and the id indeterminate (getAwards writes the id, and the
    // time only when the answer arrives); zeroed here, [CHOSEN].
    IPlayerAwards::AwardList fresh{};
    if (static_cast<IPlayerAwards*>(getPlayerAwards())->getAwards(profileId, &fresh, false, false))
    {
        m_awardCache.push_back(fresh);
        out->timestamp = fresh.timestamp;
        out->profileId = fresh.profileId;
        out->awards    = fresh.awards;
        return true;
    }
    return false;
}

// +0x22C..+0x250  formatters, the storage strings and the network helpers
// 0x487F70 (410 B): "%d-%s-%.2d" of the year, a two-digit month name and the day; "-" for no time.
std::string SwiffHost::formatDate(const time_t* time)
{
    std::string result;
    const struct tm* local = localtime(time);
    if (local && *time)
    {
        static const char* const months[12] = { "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12" };
        result = formatString("%d-%s-%.2d", local->tm_year + 1900, months[local->tm_mon], local->tm_mday);
    }
    else
    {
        result = "-";
    }
    return result;
}
// 0x487DC0 (a non-virtual twin of formatDate, fetchLeaderboard's board dates): the year, month
// name and day as formatDate, then "%.2d:%.2d" of the hour and minute; "-" for no time.
std::string SwiffHost::formatDateTime(const time_t* time)
{
    std::string result;
    const struct tm* local = localtime(time);
    if (local && *time)
    {
        static const char* const months[12] = { "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12" };
        result = formatString("%d-%s-%.2d %.2d:%.2d", local->tm_year + 1900, months[local->tm_mon], local->tm_mday,
                              local->tm_hour, local->tm_min);
    }
    else
    {
        result = "-";
    }
    return result;
}
// 0x4888A0: "%d:%.2d:%.2d" of hours, minutes and seconds.
std::string SwiffHost::formatDuration(int seconds)
{
    const int hours = seconds / 60 / 60;
    const int rest = seconds - hours * 3600;
    return formatString("%d:%.2d:%.2d", hours, rest / 60, rest % 60);
}
// 0x48A2F0 (344 B): a "$/" prefix selects the game root, otherwise the menu directory - spelled
// "Extermal" in the binary; the file manager resolves the rest.
std::string SwiffHost::resolveMenuPath(const std::string& path)
{
    static const std::string dollarSlash("$/");
    std::string relative(path);
    bool root = false;
    if (relative.find(dollarSlash, 0) == 0)
    {
        relative.erase(0, 2);
        root = true;
    }
    std::string resolved = io::fileManager->fileExistsEx2(relative);
    std::string result;
    std::string base(root ? "..\\..\\..\\..\\..\\" : "..\\..\\..\\..\\..\\Menu\\Extermal\\FlashMenu\\");
    base += resolved;
    result = base;
    return result;
}
std::string SwiffHost::getStorageString(const std::string& key, const std::string& fallback)
{
    std::map<std::string, std::string>::iterator it = m_storage.find(key);
    if (it == m_storage.end())
        return fallback;
    return it->second;
}
void SwiffHost::setStorageString(const std::string& key, const std::string& value)   { m_storage[key] = value; }
void SwiffHost::removeStorageString(const std::string& key)                          { m_storage.erase(key); }
// the profile manager shuts down, the messages go, event 14 carries the name.
void SwiffHost::handleProfileTransition(const std::string& name)
{
    IBase* manager = classManager->getSingleton(std::string("LocalProfileManager"));
    if (manager)
    {
        static_cast<ILocalProfileManager*>(manager)->shutdown();
        clearMessages();
        MenuEvent event;
        event.type = 14;
        event.args.push_back(name);
        pushEvent(event);
    }
}
// 0x488440 (188 B): Winsock 1.1, gethostbyname, the first address as a dotted quad in a static buffer.
std::string SwiffHost::resolveHostName(const std::string& name)
{
    WSADATA data;
    if (WSAStartup(0x101, &data))
        return std::string("");
    hostent* host = gethostbyname(name.c_str());
    if (!host)
    {
        WSACleanup();
        return std::string("");
    }
    in_addr address;
    std::memcpy(&address, host->h_addr_list[0], host->h_length);
    std::memset(s_resolvedAddress, 0, sizeof(s_resolvedAddress));
    const char* text = inet_ntoa(address);
    char* out = s_resolvedAddress;
    char c;
    do
    {
        c = *text;
        *out++ = *text++;
    }
    while (c);
    WSACleanup();
    return std::string(s_resolvedAddress);
}
// inet_addr, or a host lookup when the text is not numeric; the RFC 1918 test on it.
bool SwiffHost::isPrivateAddress(const std::string& address)
{
    const char* text = address.c_str();
    s_lastInetAddr = inet_addr(text);
    if (s_lastInetAddr != INADDR_NONE)
        return isPrivateNetworkAddress(&s_lastInetAddr) != 0;
    hostent* host = gethostbyname(text);
    if (host)
        return isPrivateNetworkAddress(reinterpret_cast<const unsigned long*>(host->h_addr_list[0])) != 0;
    return isPrivateNetworkAddress(0) != 0;
}
// 0x488380 (190 B): digits and exactly three dots, each octet under 256.
bool SwiffHost::isDottedQuad(const std::string& address)
{
    const std::size_t length = address.size();
    if (!length)
        return false;
    unsigned int dots = 0;
    for (std::size_t i = 0; i < length; ++i)
    {
        const char c = address[i];
        if (c == '.')
            ++dots;
        else if (c < '0' || c > '9')
            return false;
    }
    if (dots != 3)
        return false;
    unsigned int octets[4];
    if (sscanf(address.c_str(), "%d.%d.%d.%d", &octets[0], &octets[1], &octets[2], &octets[3]) != 4)
        return false;
    for (int k = 0; k < 4; ++k)
        if (octets[k] >= 0x100)
            return false;
    return true;
}
void SwiffHost::setJoinAddress(const std::string& address)    { m_joinAddress = address; }
const std::string& SwiffHost::getJoinAddress()                { return m_joinAddress; }
void SwiffHost::setIntE58(int value)                          { m_intE58 = value; }
int  SwiffHost::getIntE58()                                   { return m_intE58; }
void SwiffHost::requestRestartMenu()                          { MenuEvent e; e.type = 16; pushEvent(e); }
// Adds the server to connection history.
bool SwiffHost::addServerToHistory(const std::string& address, int queryPort, const std::string& name)
{
    if (isPrivateAddress(address))                                            // +0x24C
        return true;

    ILocalProfileManager* const profiles = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(std::string("LocalProfileManager")));
    GeneralSettings* const general = static_cast<GeneralSettings*>(
        classManager->getSingleton(std::string("GeneralSettings")));
    if (!profiles)
        return false;                                                         // unspecified in retail - see above
    if (!profiles->getCurrentProfile())                                       // +0x44
        return false;
    if (!general)
        return true;                                                          // the profile pointer, as shipped
    general->addServerHistory(address, queryPort, name, -1);                  // +0x138
    return profiles->saveCurrentProfile();                                    // +0x40
}
// 0x4886C0 (359 B): 3..20 characters, no leading space, nothing below '"' (signed), no DEL, no
// backslash, no @ + # in the first three, no quotes.
bool SwiffHost::validateProfileName(const std::string& name, std::string& error)
{
    const std::size_t length = name.size();
    if (length < 3)
    {
        error = "nameshort";
        return false;
    }
    if (length > 0x14)
    {
        error = "namelong";
        return false;
    }
    const char* text = name.c_str();
    if (text[0] == ' ')
    {
        error = "invalidnamespace";
        return false;
    }
    for (std::size_t i = 0; i < std::strlen(text); ++i)
    {
        const char c = text[i];
        if (c < 34 || c == 127 || c == '\\')
        {
            error = "invalidnamecharacter";
            return false;
        }
        if (i < 3 && (c == '@' || c == '+' || c == '#'))
        {
            error = "invalidnamethree";
            return false;
        }
        if (c == '"' || c == '\'')
        {
            error = "invalidnamecharacter";
            return false;
        }
    }
    return true;
}
std::string SwiffHost::getVersionStringValue()
{
    // the same formatter, constructing the caller-owned return string.
    return std::string(BuildNrUtil::getBuildNrString());
}
void SwiffHost::setFlagE80(bool value)                        { m_flagE80 = value; }
bool SwiffHost::getFlagE80OrNotE81()                          { return m_flagE80 || !m_flagE81; }
void SwiffHost::setFlagE81(bool value)                        { m_flagE81 = value; }
bool SwiffHost::getFlagE81()                                  { return m_flagE81; }

// +0x284..+0x298  the rank cache, +0x2A0..+0x2C8  logos, busy, page
// 0x494550 (294 B): an hour-old cached row is answered, a stale one dropped, then PlayerRankInfo.
bool SwiffHost::getRankInfo(int id, IPlayerRankInfo::RankInfo* out)
{
    if (!id)
        return false;
    IBase* info = getPlayerRankInfo();
    if (!info)
        return false;
    time_t now;
    time(&now);
    for (std::size_t i = 0; i < m_rankCache.size(); ++i)
    {
        if (m_rankCache[i].profileId != id)
            continue;
        if (difftime(now, static_cast<time_t>(m_rankCache[i].timestamp)) < 3600.0)
        {
            *out = m_rankCache[i];
            return true;
        }
        m_rankCache.erase(m_rankCache.begin() + i);
        break;
    }
    const bool result = static_cast<IPlayerRankInfo*>(info)->getRankInfo(id, *out);
    if (result)
    {
        m_rankCache.push_back(*out);
        return true;
    }
    return result;
}
// 0x491940 (176 B): PlayerRankInfo clears it, the stats cache is reset, the cached row dropped.
bool SwiffHost::clearRankNotification(int id)
{
    if (!id)
        return false;
    IBase* info = getPlayerRankInfo();
    if (!info || !static_cast<IPlayerRankInfo*>(info)->clearRankNotification(id))
        return false;
    resetStatsCache();
    for (std::size_t i = 0; i < m_rankCache.size(); ++i)
    {
        if (m_rankCache[i].profileId == id)
        {
            m_rankCache.erase(m_rankCache.begin() + i);
            return true;
        }
    }
    return true;
}
void SwiffHost::clearLastFetchId()                            { m_unlockRecord.field4 = 0; }
int  SwiffHost::getIntE94()                                   { return m_intE94; }
void SwiffHost::setIntE94(int value)                          { m_intE94 = value; }
bool SwiffHost::requestBrowseUrl(const std::string& url)
{
    MenuEvent event;
    event.type = 10;
    event.args.push_back(url);
    pushEvent(event);
    return true;
}
// Retrieves the player's awards.
void SwiffHost::slot2A0(int a, int b, int c, int d, std::string mod)
{
    IBase* awards = getPlayerAwards();
    if (awards)
    {
        if (!mod.size())
            mod = "bf2";
        static_cast<IPlayerAwards*>(awards)->addAward(a, b, c, d, mod);
    }
}
void SwiffHost::addGameMode(const std::string& gameMode, const std::string& name)
{
    // Requires ModInfo and forwards both strings unchanged.
    static_cast<IModInfo*>(getModInfo())->addNameForGameMode(gameMode, name);
}
bool SwiffHost::getFlagEA8()                                  { return m_flagEA8; }
void SwiffHost::setFlagEA8(bool value)                        { m_flagEA8 = value; }
// the busy flag, then UpdateWindow on the game window.
bool SwiffHost::setBusy(int busy)
{
    m_busy = busy;
    return UpdateWindow(bf2Engine->window->getHwnd()) != 0;
}
void SwiffHost::clearBusy()                                   { m_busy = 0; }
int  SwiffHost::getBusy()                                     { return m_busy; }
int  SwiffHost::getPage()                                     { return m_page; }
void SwiffHost::setPage(int page)                             { m_page = page; }
bool SwiffHost::getFlagEC4()                                  { return m_flagEC4; }
void SwiffHost::setFlagEC4(bool value)                        { m_flagEC4 = value; }
// Returns rank progress clamped to 0..1, or 1.0 when stats are unavailable.
// An unknown rank inserts an empty threshold row; the top rank uses a further 1000 points.
float SwiffHost::getRankProgress()
{
    SwiffPlayerStatsRecord record;
    if (!getPlayerStatsRecord(m_selectedPlayerId, record))
        return 1.0f;
    const int rank = record.rank;
    const int threshold = m_ranks[rank].a;
    const int next = getRankC(rank);
    int nextThreshold = m_ranks[next].a;
    if (rank == next)
        nextThreshold = threshold + 1000;
    float ratio = static_cast<float>(record.scor - threshold) / static_cast<float>(nextThreshold - threshold);
    if (ratio < 0.0f)
        return 0.0f;
    if (ratio > 1.0f)
        ratio = 1.0f;
    return ratio;
}
bool SwiffHost::checkDisc()                                   { return true; }
void SwiffHost::reportNoDisc()
{
    postMessage(SwiffHostMessage(1, std::string("unable to find BF2 cd/dvd in tray"), 0));
}
void SwiffHost::addEntryEC8(const std::string& text, int value)
{
    SwiffStringIntEntry entry;
    entry.text = text;
    entry.value = value;
    m_entriesEC8.push_back(entry);
}
std::vector<SwiffStringIntEntry>* SwiffHost::getEntriesEC8()  { return &m_entriesEC8; }
// Returns a borrowed pointer to the local server's welcome message or the cached remote message.
const std::string* SwiffHost::getServerNameString()
{
    if (isGameLogicServer())
        return &serverSettings->m_welcomeMessage;
    return &m_stringED8;
}
void SwiffHost::setStringED8(const std::string& value)        { m_stringED8 = value; }
// Releases stats rows, unlock state, player boards, awards and ranks.
// Retains global leaderboard metadata, including timestamps and positions.
void SwiffHost::resetStatsCache()
{
    std::vector<SwiffPlayerStatsRecord>().swap(m_playerStatsRecords);
    m_unlockRecord.field4 = 0;
    std::vector<SwiffLeaderboardScoreRow>().swap(m_leaderboard.overall.rows);
    std::vector<SwiffLeaderboardRisingStarRow>().swap(m_leaderboard.risingStar.rows);
    std::vector<SwiffLeaderboardScoreRow>().swap(m_leaderboard.commander.rows);
    std::vector<SwiffLeaderboardScoreRow>().swap(m_leaderboard.teamwork.rows);
    std::vector<SwiffLeaderboardRow>().swap(m_leaderboard.combat.rows);
    for (int k = 0; k < 7; ++k)
        std::vector<SwiffLeaderboardRow>().swap(m_leaderboard.kits[k].rows);
    for (int v = 0; v < 7; ++v)
        std::vector<SwiffLeaderboardRow>().swap(m_leaderboard.vehicles[v].rows);
    for (int w = 0; w < 12; ++w)
        std::vector<SwiffLeaderboardWeaponRow>().swap(m_leaderboard.weapons[w].rows);
    m_leaderboardByPlayer.clear();
    std::vector<IPlayerAwards::AwardList>().swap(m_awardCache);
    std::vector<IPlayerRankInfo::RankInfo>().swap(m_rankCache);
}
bool SwiffHost::getFlagF04()                                  { return m_flagF04; }
void SwiffHost::setFlagF04(bool value)                        { m_flagF04 = value; }
// 0x488C20 (112 B): true with a running game; otherwise SwiffRender uploads it, -1 is failure.
bool SwiffHost::preloadTexture(const std::string& name)
{
    if (game)
        return true;
    IBase* render = classManager->getSingleton(std::string("SwiffRender"));
    unsigned int size[2];
    return render && static_cast<ui::ISwiffRender*>(render)->uploadTexture(name, &size[0], &size[1]) != -1;
}
// GameClient block 0 stores ServerInfo::gameName here through +0x2FC.
void SwiffHost::setStringE5C(const std::string& value)        { m_stringE5C = value; }
const std::string& SwiffHost::getStringE5C()                  { return m_stringE5C; }
int SwiffHost::getConnectToIpPort()
{
    const int fallback = 16567;
    return settings->intGet(std::string("GSConnectToIpPort"), fallback);
}
void SwiffHost::setConnectToIpPort(int port)                  { settings->intSet(std::string("GSConnectToIpPort"), port); }
std::string SwiffHost::getConnectToIpHost()                   { return settings->stringGet(std::string("GSConnectToIpHost"), std::string("")); }
void SwiffHost::setConnectToIpHost(const std::string& host)   { settings->stringSet(std::string("GSConnectToIpHost"), host); }
int  SwiffHost::getIntF08()                                   { return m_intF08; }
void SwiffHost::setIntF08(int value)                          { m_intF08 = value; }
bool SwiffHost::getRenderFlag()
{
    IBase* render = classManager->getSingleton(std::string("SwiffRender"));
    if (render)
        return static_cast<ui::ISwiffRender*>(render)->getFlag() != 0;
    return false;
}
void SwiffHost::setRenderFlag(bool value)
{
    IBase* render = classManager->getSingleton(std::string("SwiffRender"));
    if (render)
        static_cast<ui::ISwiffRender*>(render)->setFlag(value ? 1 : 0);
}
// an nvidia adapter whose driver build is below 663051.
bool SwiffHost::isRunningWithUnsupportedDrivers()
{
    IBase* render = classManager->getSingleton(std::string("SwiffRender"));
    unsigned int version = 0;
    return render && static_cast<ui::ISwiffRender*>(render)->isNvidia()
        && *static_cast<ui::ISwiffRender*>(render)->getDriverVersion(&version) < 663051u;
}
float SwiffHost::getFloatF0C()                                { return m_floatF0C; }
void SwiffHost::setFloatF0C(float value)
{
    if (value >= 500.0f)
        m_floatF0C = value <= 4000.0f ? value : 4000.0f;
    else
        m_floatF0C = 500.0f;
}
void SwiffHost::setFlagF10(bool value)
{
    m_flagF10 = value;
    m_intF14 = 0;
    m_flagF18 = true;
}
bool SwiffHost::getFlagF10()                                  { return m_flagF10; }
int  SwiffHost::getIntF14()                                   { return m_intF14; }
void SwiffHost::setIntF14(int value)                          { m_intF14 = value; }
void SwiffHost::setFlagF18(bool value)                        { m_flagF18 = value; }
bool SwiffHost::getFlagF18()                                  { return m_flagF18; }
void SwiffHost::getRect(int* out)
{
    out[0] = m_rect[0];
    out[1] = m_rect[1];
    out[2] = m_rect[2];
    out[3] = m_rect[3];
}
void SwiffHost::setRect(int x, int y, int width, int height)
{
    m_rect[0] = x;
    m_rect[1] = y;
    m_rect[2] = width;
    m_rect[3] = height;
}
void SwiffHost::setServerMod(const std::string& mod)          { m_serverMod = mod; }
const std::string& SwiffHost::getServerMod()                  { return m_serverMod; }
// BF2.exe GameLogic::load seeds 999; EndOfRoundEvent replaces it with the transmitted rounds-left.
int  SwiffHost::getIntF4C()                                   { return m_intF4C; }
void SwiffHost::setIntF4C(int value)                          { m_intF4C = value; }
bool SwiffHost::isPlayingDemo()                               { return game && game->isPlayingDemo(); }
std::string SwiffHost::getModName()                           { return currentModName(); }
bool SwiffHost::slot368(int, int, int)                        { return true; }
// 0x4890D0 (190 B): 1 for xpack, 2 for xpack2, else 0; an empty name means the current mod.
int SwiffHost::getExpansionPack(const std::string& modName)
{
    std::string mod(modName);
    if (mod.empty())
        mod = currentModName();
    if (compareIgnoreCase(mod, std::string("xpack")) == 0)
        return 1;
    if (compareIgnoreCase(mod, std::string("xpack2")) == 0)
        return 2;
    return 0;
}
std::string SwiffHost::getLanguageSetting()
{
    std::string language = settings->stringGet(std::string("GSLanguage"), std::string("English"));
    return language;
}
void SwiffHost::parseResolution(const std::string&, int*, int*, int*)
{
    SWIFF_HOST_FRONTIER("parseResolution", "PE 0x489210: the \"x@Hz\" tokenizer 0x59E1B0/0x59E1F0 is unrecovered");
}
bool SwiffHost::getFlagF2C()                                  { return m_flagF2C; }
void SwiffHost::setFlagF2C(bool value)                        { m_flagF2C = value; }

// +0x380..+0x3AC  the ServerInfo block (PE +88..+124 of the singleton = block +0x38..+0x5C)
bool  SwiffHost::getServerInfoNoVehicles()         { return serverInfo->m_block.noVehicles != 0; }
bool  SwiffHost::getServerInfoAlwaysTrue()         { return serverInfo->m_block.alwaysTrue != 0; }
int   SwiffHost::getServerInfoRoundsPerMap()       { return serverInfo->m_block.roundsPerMap; }
int   SwiffHost::getServerInfoSpawnTime()          { return serverInfo->m_block.spawnTime; }
int   SwiffHost::getServerInfoTimeLimit()          { return serverInfo->m_block.timeLimit; }
int   SwiffHost::getServerInfoTicketRatio()        { return serverInfo->m_block.ticketRatio; }
int   SwiffHost::getServerInfoTeamRatioPercent()   { return serverInfo->m_block.teamRatioPercent; }
bool  SwiffHost::getServerInfoAutoBalanceTeam()    { return serverInfo->m_block.autoBalanceTeam != 0; }
bool  SwiffHost::getServerInfoUseGlobalUnlocks()   { return serverInfo->m_block.useGlobalUnlocks != 0; }
float SwiffHost::getServerInfoCoopBotRatio()       { return static_cast<float>(serverInfo->m_block.coopBotRatio); }
int   SwiffHost::getServerInfoCoopBotCount()       { return serverInfo->m_block.coopBotCount; }
float SwiffHost::getServerInfoCoopBotDifficulty()  { return static_cast<float>(serverInfo->m_block.coopBotDifficulty); }

// a connected game (queryInterface 120002), else a type-1 message.
bool SwiffHost::checkGameConnected()
{
    if (game && game->queryInterface(120002u))
        return true;
    postMessage(SwiffHostMessage(1, std::string("unable to fetch server info since game is not connected"), 0));
    return false;
}

} }
#endif
