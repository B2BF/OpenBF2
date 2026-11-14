// Maintains a GameSpy server list and translates its records for the menu.
#include "ServerBrowser.h"
#if !DEDICATED
#include "ISwiffHost.h"
#include "../Core/ClassManager.h"
#include "../Core/Ids.h"
#include "../Core/SettingsRepostitory.h"
#include "../External/GameSpy/serverbrowsing/sb_serverbrowsing.h"
#include "../External/GameSpy/common/gsAvailable.h"
#include "../External/GameSpy/common/gsPlatformUtil.h"
#include "../External/GameSpy/qr2/qr2regkeys.h"
#include "../Game/IGame.h"                      // game: the current map's name / mode / size (fillLocalRecord)
#include "../Game/IGameServer.h"                // the server view's client counts and content checking
#include "../Game/IGameLogic.h"                 // gameLogic: team names, the voice server info
#include "../Game/GameServer/ServerSettings.h"  // serverSettings (PE ds:0x9A4D98)
#include "../Game/Main/BF2Engine.h"             // getAverageFPS (PE 0x4049F0)
#include "../Game/Main/BF2ClientSeams.h"        // unresolvedGlobalA33C08 (PE ds:0xA33C08)
#include "../Game/Main/Game.h"                  // isDedicated (PE 0x440510), getBF2Version (PE 0x4404F0)
#include "../Game/Objects/IPlayer.h"
#include "../Game/Utility/BuildNrUtil.h"        // getServerName, getModName, getOS
#include "../Scene/Object/PlayerManager.h"      // world::playerManager (PE ds:0xA08F60)
#include "../Scene/Object/PlayerScoreData.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <list>
#if !defined(_WIN32)
#include <cerrno>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#endif

namespace dice { namespace hfe {

extern ClassManager* classManager;
extern BF2Engine* bf2Engine;                    // Core/Globals.cpp; PE dword_986F48
namespace gamespy { const char* getSecretKey(); }   // Game/MatchMaking/Server/GameSpyBackend.cpp: "hW6m9a" (the PE's own 0x59BA80 spells the same key out byte by byte on first use)

namespace {

// ds:0x9A64C0 - the record fillRecord (0x4840A0) assembles before copying it into the caller's.
ui::ServerBrowserRecord s_record;

// the buffer is freed (not merely emptied) and the revision bumped.
void clearRecords(ui::ServerBrowserRecordList& list)
{
    std::vector<ui::ServerBrowserRecord>().swap(list.records);
    ++list.revision;
}

// the first record filled from `server`, or null.
ui::ServerBrowserRecord* findRecord(ui::ServerBrowserRecordList& list, SBServer server)
{
    for (size_t i = 0; i < list.records.size(); ++i)
    {
        if (list.records[i].server == server)
            return &list.records[i];
    }
    return 0;
}

// a record over the list's ping limit is dropped (and reported as added); one for a
// server already listed replaces that record; anything else is appended. The revision moves in
// every accepted case.
bool addRecord(ui::ServerBrowserRecordList& list, const ui::ServerBrowserRecord& record)
{
    if (list.maxPing != -1 && record.entry.ping > list.maxPing)
        return true;
    if (ui::ServerBrowserRecord* existing = findRecord(list, record.server))
        *existing = record;
    else
        list.records.push_back(record);
    ++list.revision;
    return true;
}

// the record of `server` is erased (the tail moved down) and the revision bumped.
bool removeRecord(ui::ServerBrowserRecordList& list, SBServer server)
{
    for (size_t i = 0; i < list.records.size(); ++i)
    {
        if (list.records[i].server == server)
        {
            list.records.erase(list.records.begin() + i);
            ++list.revision;
            return true;
        }
    }
    return false;
}

// the SDK's ServerBrowserCallback. The SDK's handle type is `::ServerBrowser`; the
// class's member takes it as the struct pointer it is.
void browserCallback(::ServerBrowser browser, SBCallbackReason reason, SBServer server, void* instance)
{
    ServerBrowser::dispatchCallback(browser, static_cast<int>(reason), server, instance);
}

// Returns the first address resolved for this host, or 127.0.0.1 if hostname lookup or resolution fails.
std::string localAddress()
{
    char name[260];
    if (gethostname(name, 260) != -1)
    {
        struct hostent* host = gethostbyname(name);
        if (host)
            return std::string(inet_ntoa(*reinterpret_cast<struct in_addr*>(host->h_addr_list[0])));
    }
    return std::string("127.0.0.1");
}

#if !defined(_WIN32)
// [CHOSEN, not recovered] Retail has no such message. On macOS a denied Local Network permission
// makes every LAN broadcast send fail and the LAN list stays empty without a word. The text goes
// through the menu's own message channel: SwiffHost::postMessage (BF2.exe 0x496CA0, host +0x1E4),
// polled by mainMenu.swf's checkForMessages through dice.MessageHandler.getLastType/getLastText
// (SwiffPlayer.dll 0x1004F0A0 / 0x1004F000, host +0x204). Type 1 is that page's "error" frame:
// the text as given under the title MESSAGEHANDLER_error ("GAME MESSAGE"), the kind retail uses for
// its own plain-text messages (ProfileManager::reportError). Type 2 "warning" shows its title id
// raw (no MESSAGEHANDLER_warning string ships), type 3 looks the text up as a localization id.
const char kLanDeniedText[] =
    "OpenBF2 can't search the local network for servers. Allow it in System Settings > "
    "Privacy & Security > Local Network, then refresh the list.";

// Whether an IPv4 interface other than loopback is up and can broadcast: without one an empty LAN
// list is expected and the failed send is not a permission problem.
bool hasBroadcastInterface()
{
    struct ifaddrs* list = 0;
    if (getifaddrs(&list) != 0)
        return false;
    bool found = false;
    for (struct ifaddrs* it = list; it && !found; it = it->ifa_next)
    {
        const unsigned int flags = it->ifa_flags;
        found = it->ifa_addr && it->ifa_addr->sa_family == AF_INET
             && (flags & IFF_UP) && (flags & IFF_RUNNING) && (flags & IFF_BROADCAST) && !(flags & IFF_LOOPBACK);
    }
    freeifaddrs(list);
    return found;
}

// The LAN browse's broadcast failed with `error`. macOS refuses a send that Local Network privacy
// denies with EHOSTUNREACH; a sandbox or firewall rule refuses it with EPERM. Anything else (e.g.
// ENETUNREACH / ENETDOWN with no network) raises nothing. At most one message per browse, and
// none while the same message is still the newest one queued.
void reportLanBroadcastFailure(int error)
{
    if (error != EHOSTUNREACH && error != EPERM)
        return;
    if (!hasBroadcastInterface())
        return;
    IBase* host = classManager->getSingleton(std::string("SwiffHost"));
    if (!host)
        return;
    ui::ISwiffHost* swiffHost = static_cast<ui::ISwiffHost*>(host);
    SwiffHostMessage last;
    if (swiffHost->popLastMessage(last, false) && last.type == 1 && last.text == kLanDeniedText)
        return;
    swiffHost->postMessage(SwiffHostMessage(1, std::string(kLanDeniedText), 0));
}
#endif

} // namespace

// [CHOSEN] Initializes sort arguments to zero. They remain unused until the first sort supplies a key.
ServerBrowser::ServerBrowser()
    : m_refCount(1), m_browser(0), m_internet(false), m_suppressCallbacks(false), m_sortStringCompare(0),
      m_sortAscending(false), m_updateStarted(false), m_maxPing(-1), m_keepUnansweredServers(false)
{
    m_results.records.reserve(4000);   // 0x484F00(4000) on each list: 4000 records up front
    m_pending.records.reserve(4000);
}

// the GameSpy handle is freed first, then the members (filter, sort key, the pending
// records, the results) in reverse declaration order - the implicit destruction.
ServerBrowser::~ServerBrowser()
{
    if (m_browser)
    {
        ServerBrowserFree(m_browser);
        m_browser = 0;
    }
}

IBase* ServerBrowser::create(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    ServerBrowser* list = new ServerBrowser();
    IBase* result = static_cast<IBase*>(list->queryInterface(iid));
    if (!result)
        list->release();
    return result;
}

void  ServerBrowser::addRef() { ++m_refCount; }
int   ServerBrowser::getRef() { return m_refCount; }
int   ServerBrowser::release()
{
    const int left = --m_refCount;
    if (!left)
        delete this;
    return left;
}
void* ServerBrowser::queryInterface(unsigned int iid) const
{
    if (iid == 1 || iid == IID_IServerBrowser)
        return const_cast<ServerBrowser*>(this);
    return 0;
}

// Replaces the previous browser handle and creates an internet or LAN browser.
// Blocks on the availability check and returns false if the service is unavailable.
bool ServerBrowser::init(bool internet)
{
    shutdown();
    GSIStartAvailableCheck(settings->stringGet(std::string("gameName"), std::string("battlefield2")).c_str());
    GSIACResult result;
    for (result = GSIAvailableCheckThink(); result == GSIACWaiting; result = GSIAvailableCheckThink())
        msleep(5);
    if (result != GSIACAvailable)
        return false;
    m_internet = internet;
    m_browser = ServerBrowserNew(settings->stringGet(std::string("gameName"), std::string("battlefield2")).c_str(),
                                 settings->stringGet(std::string("gameName"), std::string("battlefield2")).c_str(),
                                 gamespy::getSecretKey(), 0, 40, 1, m_internet ? SBFalse : SBTrue,
                                 &browserCallback, this);
    return m_browser != 0;
}

void ServerBrowser::shutdown()
{
    if (m_browser)
    {
        ServerBrowserFree(m_browser);
        m_browser = 0;
    }
}

// Advances an active browse, submitting at most 39 queued basic queries before processing SDK work.
// Queued queries require the SwiffHost singleton.
void ServerBrowser::update()
{
    if (!isBrowsing())
        return;
    if (m_pending.records.size() != 0)
    {
        if (!classManager->getSingleton(std::string("SwiffHost")))
            return;
        for (int i = 0; i < 39; ++i)
        {
            if (m_pending.records.size() == 0)
                break;
            ServerBrowserAuxUpdateServer(m_browser, m_pending.records.front().server, SBTrue, SBFalse);
            m_pending.records.erase(m_pending.records.begin());
        }
    }
    ServerBrowserThink(m_browser);
}

bool ServerBrowser::isBrowsing()
{
    if (!m_browser)
        return false;
    if (m_pending.records.size() != 0)
        return true;
    SBState state = ServerBrowserState(m_browser);
    return state == sb_querying || state == sb_listxfer;
}

ui::ServerBrowserRecordList* ServerBrowser::getRecords()
{
    return &m_results;
}

void ServerBrowser::reset()
{
    if (!m_browser)
        return;
    clearRecords(m_pending);
    clearRecords(m_results);
    ServerBrowserClear(m_browser);
}

// Clears existing results and starts an update using the current internet filter or LAN query ports
// 29900 through 29950. Returns false when the browser is unavailable or the SDK refuses the update.
bool ServerBrowser::startUpdate(bool async)
{
    if (!m_browser)
        return false;
    clearRecords(m_pending);
    ServerBrowserHalt(m_browser);
    ServerBrowserClear(m_browser);
    clearRecords(m_results);
    if (m_internet)
    {
        unsigned char basicFields[23] =
        {
            HOSTNAME_KEY, GAMETYPE_KEY, MAPNAME_KEY, NUMPLAYERS_KEY, MAXPLAYERS_KEY, HOSTPORT_KEY,
            GAMEVARIANT_KEY, GAMEMODE_KEY, PASSWORD_KEY, GAMEVER_KEY, HOSTPORT_KEY,
            54, 53, 52, 58, 55, 51, 72, 71, 73, 75, 77, 78
        };
        if (ServerBrowserUpdate(m_browser, async ? SBTrue : SBFalse, SBTrue, basicFields, 23, m_filter.c_str()) != sbe_noerror)
            return false;
    }
    else
    {
#if !defined(_WIN32)
        const SBError lanError = ServerBrowserLANUpdate(m_browser, async ? SBTrue : SBFalse, 29900, 29950);
        if (ServerBrowserLANSendError(m_browser) != 0)   // [CHOSEN, not recovered], see reportLanBroadcastFailure
            reportLanBroadcastFailure(ServerBrowserLANSendError(m_browser));
        if (lanError != sbe_noerror)
            return false;
#else
        if (ServerBrowserLANUpdate(m_browser, async ? SBTrue : SBFalse, 29900, 29950) != sbe_noerror)
            return false;
#endif
    }
    m_updateStarted = true;
    return true;
}

// The listed servers become the pending queue (records, revision and ping limit), the results
// are cleared; a browse in progress is halted first.
bool ServerBrowser::requeueRecords()
{
    if (!m_browser)
        return false;
    if (isBrowsing())
        ServerBrowserHalt(m_browser);
    m_pending.records  = m_results.records;
    m_pending.revision = m_results.revision;
    m_pending.maxPing  = m_results.maxPing;
    clearRecords(m_results);
    return true;
}

// The queue is dropped, a browse in progress halted, and the last sort - if there was one -
// applied again (through the virtual slot).
void ServerBrowser::stopUpdate()
{
    if (!m_browser)
        return;
    clearRecords(m_pending);
    if (isBrowsing())
        ServerBrowserHalt(m_browser);
    if (m_sortKey.size() != 0)
        sort(m_sortKey, m_sortAscending, m_sortStringCompare);
}

// +0x34 isInternet, +0x38 sort (0x4858B0), +0x3C getRecord (0x482C60)
bool ServerBrowser::isInternet()
{
    return m_internet;
}

// The results are rebuilt from the SDK's list after it is sorted on the key: every server with
// basic keys is filled from its cached keys and added (the count is re-read each round).
void ServerBrowser::sort(const std::string& key, bool ascending, int stringCompare)
{
    if (!m_browser)
        return;
    clearRecords(m_results);
    m_sortKey = key;
    m_sortStringCompare = stringCompare;
    m_sortAscending = ascending;
    ServerBrowserSort(m_browser, ascending ? SBTrue : SBFalse, m_sortKey.c_str(), stringCompare != 0 ? sbcm_stricase : sbcm_int);
    for (unsigned int i = 0; i < static_cast<unsigned int>(ServerBrowserCount(m_browser)); ++i)
    {
        SBServer server = ServerBrowserGetServer(m_browser, i);
        if (SBServerHasBasicKeys(server))
        {
            ui::ServerBrowserRecord record;
            if (fillRecord(server, &record, false, true))
                addRecord(m_results, record);
        }
    }
}

// The listed record of the server at address:port, refreshed through fillRecord; null when the
// SDK does not know the address or the record is not listed. The suppression flag is raised
// around the lookup (getServer lowers it itself) and lowered at the end.
ui::ServerBrowserRecord* ServerBrowser::getRecord(const std::string& address, int port, bool full, bool useCached)
{
    if (!m_browser)
        return 0;
    m_suppressCallbacks = true;
    SBServer server = getServer(address, port);
    if (!server)
    {
        m_suppressCallbacks = false;
        return 0;
    }
    ui::ServerBrowserRecord* record = findRecord(m_results, server);
    if (!record)
    {
        m_suppressCallbacks = false;
        return 0;
    }
    fillRecord(server, record, full, useCached);
    m_suppressCallbacks = false;
    return record;
}

// Fills out from a server query or cached SDK data. Returns false if a required query fails.
// A full record queries missing full keys even when useCached is true.
bool ServerBrowser::fillRecord(SBServer server, ui::ServerBrowserRecord* out, bool full, bool useCached)
{
    in_addr publicAddress;
    in_addr privateAddress;
    publicAddress.s_addr  = SBServerGetPublicInetAddress(server);
    privateAddress.s_addr = SBServerGetPrivateInetAddress(server);
    s_record.server = server;
    if (!full)
    {
        if (!useCached)
        {
            m_suppressCallbacks = true;
            if (ServerBrowserAuxUpdateServer(m_browser, server, SBFalse, SBFalse) != sbe_noerror)
            {
                m_suppressCallbacks = false;
                return false;
            }
            m_suppressCallbacks = true;   // left raised, as shipped
        }
    }
    else
    {
        m_suppressCallbacks = true;
        if (!useCached)
        {
            const SBError error = ServerBrowserAuxUpdateServer(m_browser, server, SBFalse, SBTrue);
            m_suppressCallbacks = false;
            if (error != sbe_noerror)
                return false;
        }
        else
        {
            if (!SBServerHasFullKeys(server) && ServerBrowserAuxUpdateServer(m_browser, server, SBFalse, SBTrue) != sbe_noerror)
            {
                m_suppressCallbacks = false;
                return false;
            }
            m_suppressCallbacks = false;
        }
    }

    ui::ServerBrowserEntry& entry = s_record.entry;
    entry.ping             = SBServerGetPing(server);
    entry.address          = inet_ntoa(publicAddress);
    entry.queryPort        = SBServerGetPublicQueryPort(server);
    entry.privateAddress   = inet_ntoa(privateAddress);
    entry.privateQueryPort = SBServerGetPrivateQueryPort(server);
    entry.hostName         = SBServerGetStringValue(server, qr2_registered_key_list[HOSTNAME_KEY], "");
    entry.hostPort         = SBServerGetIntValue(server, qr2_registered_key_list[HOSTPORT_KEY], 0);
    entry.numPlayers       = SBServerGetIntValue(server, qr2_registered_key_list[NUMPLAYERS_KEY], 0);
    entry.maxPlayers       = SBServerGetIntValue(server, qr2_registered_key_list[MAXPLAYERS_KEY], 0);
    entry.plasma           = SBServerGetIntValue(server, qr2_registered_key_list[77], 0) != 0;   // "bf2_plasma"
    entry.reservedSlots    = SBServerGetIntValue(server, qr2_registered_key_list[78], 0);        // "bf2_reservedslots"
    entry.gameMode         = SBServerGetStringValue(server, qr2_registered_key_list[GAMETYPE_KEY], "");
    entry.password         = SBServerGetIntValue(server, qr2_registered_key_list[PASSWORD_KEY], 0) != 0;
    entry.field140         = 2;
    entry.mapName          = SBServerGetStringValue(server, qr2_registered_key_list[MAPNAME_KEY], "");
    entry.gameVariant      = SBServerGetStringValue(server, qr2_registered_key_list[GAMEVARIANT_KEY], "");
    entry.gameVersion      = SBServerGetStringValue(server, qr2_registered_key_list[GAMEVER_KEY], "0.0");
    entry.dedicated        = SBServerGetIntValue(server, "bf2_dedicated", 0) != 0;
    entry.ranked           = SBServerGetIntValue(server, "bf2_ranked", 0) == 1;
    entry.antiCheat        = SBServerGetIntValue(server, "bf2_anticheat", 0) != 0;
    entry.os               = SBServerGetStringValue(server, "bf2_os", "");
    entry.autoRecord       = SBServerGetIntValue(server, "bf2_autorec", 0) == 1;
    entry.voip             = SBServerGetIntValue(server, "bf2_voip", 0) == 1;
    entry.bots             = SBServerGetIntValue(server, "bf2_bots", 0) == 1;
    entry.pure             = SBServerGetIntValue(server, "bf2_pure", 0) == 1;
    entry.mapSize          = SBServerGetIntValue(server, "bf2_mapsize", 0);
    entry.fps              = static_cast<float>(SBServerGetFloatValue(server, "bf2_fps", 0.0));

    if (full)
    {
        ui::ServerBrowserDetails& details = s_record.details;
        details.team1             = SBServerGetStringValue(server, "bf2_team1", "team_1");
        details.team2             = SBServerGetStringValue(server, "bf2_team2", "team_2");
        details.demoIndexUrl      = SBServerGetStringValue(server, "bf2_d_idx", "");
        details.demoDownloadUrl   = SBServerGetStringValue(server, "bf2_d_dl", "");
        details.friendlyFire      = SBServerGetIntValue(server, "bf2_friendlyfire", 0) != 0;
        details.tkMode            = SBServerGetStringValue(server, "bf2_tkmode", "");
        details.startDelay        = SBServerGetIntValue(server, "bf2_startdelay", 0);
        details.spawnTime         = static_cast<float>(SBServerGetFloatValue(server, "bf2_spawntime", 0.0));
        details.timeLimit         = SBServerGetIntValue(server, qr2_registered_key_list[TIMELIMIT_KEY], 0);
        details.roundTime         = SBServerGetIntValue(server, qr2_registered_key_list[ROUNDTIME_KEY], 0);
        details.sponsorText       = SBServerGetStringValue(server, "bf2_sponsortext", "");
        details.sponsorLogoUrl    = SBServerGetStringValue(server, "bf2_sponsorlogo_url", "");
        details.communityLogoUrl  = SBServerGetStringValue(server, "bf2_communitylogo_url", "");
        details.autoBalanced      = SBServerGetIntValue(server, "bf2_autobalanced", 0) == 1;
        details.teamRatio         = static_cast<float>(SBServerGetFloatValue(server, "bf2_teamratio", 0.0));
        details.ticketRatio       = SBServerGetIntValue(server, "bf2_ticketratio", 0);
        details.scoreLimit        = SBServerGetIntValue(server, "bf2_scorelimit", 0);
        details.coopBotRatio      = static_cast<float>(SBServerGetFloatValue(server, "bf2_coopbotratio", 0.0));
        details.coopBotCount      = SBServerGetIntValue(server, "bf2_coopbotcount", 0);
        details.coopBotDifficulty = static_cast<float>(SBServerGetFloatValue(server, "bf2_coopbotdiff", 0.0));
        details.noVehicles        = SBServerGetFloatValue(server, "bf2_novehicles", 0.0) != 0.0;   // compared against 0.0f
        details.globalUnlocks     = SBServerGetIntValue(server, "bf2_globalunlocks", 0) == 1;
        std::vector<ui::ServerBrowserPlayer>().swap(details.players);   // the buffer freed
        unsigned int count = static_cast<unsigned int>(entry.numPlayers);
        for (unsigned int i = 0; i < count; ++i)
        {
            ui::ServerBrowserPlayer player;
            player.name = SBServerGetPlayerStringValue(server, i, "player", "");
            if (player.name.size() == 0)
                continue;
            player.pid    = SBServerGetPlayerIntValue(server, i, "pid", 0);
            player.team   = SBServerGetPlayerStringValue(server, i, "team", "");
            player.skill  = SBServerGetPlayerIntValue(server, i, "skill", 0);
            player.deaths = SBServerGetPlayerIntValue(server, i, "deaths", 0);
            player.score  = SBServerGetPlayerIntValue(server, i, "score", 0);
            player.aiBot  = SBServerGetPlayerIntValue(server, i, "AIBot", 0);
            details.players.push_back(player);
            // the two-string case-insensitive compare helper 0x431950 on the caller's
            // record's game mode against "gpm_coop".
            if (_stricmp(out->entry.gameMode.c_str(), "gpm_coop") == 0 && player.aiBot == 1)
                ++count;
        }
    }

    out->server  = s_record.server;
    out->entry   = s_record.entry;
    out->details = s_record.details;
    return true;
}

// Fills the hosted-server record; returns false without writing if required game interfaces are missing.
// Appends player rows without clearing previous rows and leaves server and reservedSlots unchanged.
// The reported player count is zero; each row's team is stored as a single numeric character.
bool ServerBrowser::fillLocalRecord(ui::ServerBrowserRecord* out)
{
    IBase* host = classManager->getSingleton(std::string("SwiffHost"));
    if (!host || !game || !game->queryInterface(IID_IGameServer))
        return false;

    out->entry.address = localAddress();
    out->entry.queryPort = serverSettings->m_gameSpyPort;                       // +360
    out->entry.privateAddress = "";                                             // ds:0x87FB54
    out->entry.privateQueryPort = 0;
    out->entry.hostName = getServerName();
    out->entry.hostPort = serverSettings->m_serverPort;                         // +152
    out->entry.ping = 0;
    out->entry.plasma = false;

    IGameServer* gameServer = game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    if (gameServer)
    {
        const int connecting = gameServer->getNumberOfConnectingClients();     // +0x7C, called first
        out->entry.numPlayers = gameServer->getNumberOfConnectedClients() + connecting;   // +0x78
    }
    else
    {
        out->entry.numPlayers = 0;
    }
    if (world::playerManager->getLocalHumanPlayer())                            // +0x30
        ++out->entry.numPlayers;
    if (serverSettings->m_autoRecord)                                           // +365
        --out->entry.numPlayers;
    if (out->entry.numPlayers > serverSettings->getMaxPlayers())
        out->entry.numPlayers = serverSettings->getMaxPlayers();
    if (out->entry.numPlayers < 0)
        out->entry.numPlayers = 0;
    out->entry.maxPlayers = serverSettings->getMaxPlayers();

    if (game)
        out->entry.gameMode = game->getCurrentMapGameMode();                    // PE +0xF0
    else
        out->entry.gameMode = "starting up";
    const bool password = serverSettings->getPassword().size() != 0;
    out->entry.field140 = 2;
    out->entry.password = password;
    out->entry.mapName = game->getCurrentMapName();                             // PE +0xEC, unguarded
    out->entry.gameVariant = getModName();
    out->entry.gameVersion = getBF2Version();
    out->entry.dedicated = isDedicated() != 0;
    out->entry.ranked = false;
    out->entry.antiCheat = false;
    out->entry.os = getOS();
    out->entry.autoRecord = serverSettings->m_autoRecord != 0;
    out->details.demoIndexUrl = serverSettings->m_demoIndexURL;                 // +368
    out->details.demoDownloadUrl = serverSettings->m_demoDownloadURL;           // +396

    bool voip = false;
    unsigned int voipAddress = 0;
    unsigned short voipPort = 0;
    unsigned short voipClientPort = 0;
    int voipQuality = 0;
    gameLogic->getVoipServerInfo(voip, voipAddress, voipPort, voipClientPort, voipQuality);   // +0x258, unguarded
    out->entry.voip = voip;

    out->details.friendlyFire = serverSettings->m_soldierFriendlyFire >= 100;   // +296, signed
    out->details.tkMode = serverSettings->m_tkPunishEnabled == 1 ? "Punish" : "No Punish";   // +320, `cmp byte, 1`
    out->details.startDelay = serverSettings->m_startDelay;                     // +264
    out->details.spawnTime = serverSettings->getSpawnTime();
    out->details.timeLimit = serverSettings->m_timeLimit;                       // +288
    out->details.roundTime = serverSettings->m_roundsPerMap;                    // +284
    out->details.sponsorText = serverSettings->m_sponsorText;                   // +508
    out->details.sponsorLogoUrl = serverSettings->m_sponsorLogoURL;             // +536
    out->details.communityLogoUrl = serverSettings->m_communityLogoURL;         // +564
    out->details.autoBalanced = serverSettings->m_autoBalanceTeam;              // +488
    out->details.teamRatio = serverSettings->getTeamRatioPercent();
    out->details.ticketRatio = serverSettings->getTicketRatio();
    out->details.scoreLimit = serverSettings->m_scoreLimit;                     // +292
    out->details.coopBotRatio = serverSettings->getCoopBotRatio();
    out->details.coopBotCount = serverSettings->getCoopBotCount();
    out->details.coopBotDifficulty = serverSettings->getCoopBotDifficulty();
    out->details.noVehicles = serverSettings->getNoVehicles();
    out->entry.fps = bf2Engine->getAverageFPS();

    out->details.team1 = "team0";
    if (gameLogic)
        out->details.team1 = gameLogic->getTeamName(1);                         // +0x48
    out->details.team2 = "team1";
    if (gameLogic)
        out->details.team2 = gameLogic->getTeamName(2);
    out->entry.bots = unresolvedGlobalA33C08 != 0;                              // ds:0xA33C08
    IGameServer* contentServer = game ? static_cast<IGameServer*>(game->queryInterface(IID_IGameServer)) : 0;
    out->entry.pure = contentServer->useContentChecking();                      // +0xC4, unguarded
    out->entry.mapSize = game ? game->getCurrentMapSize() : 0;                  // PE +0xFC
    out->details.globalUnlocks = serverSettings->getUseGlobalUnlocks();

    std::list<world::IPlayer*> players(world::playerManager->getPlayers());     // PE +0x40, copied (0x483370)
    for (std::list<world::IPlayer*>::iterator it = players.begin(); it != players.end(); ++it)
    {
        world::IPlayer* player = *it;
        world::PlayerScoreData* score = player->getScoreData();                 // PE +0x170
        ui::ServerBrowserPlayer row;
        row.pid = player->getProfileId();                                       // PE +0x2A8
        row.name = player->getName();                                           // PE +0x20
        row.team = static_cast<char>(player->getTeam());                        // PE +0xE4, operator=(char)
        row.skill = score->m_field14;
        row.deaths = score->m_field10;
        row.score = score->m_score;
        out->details.players.push_back(row);
    }
    out->entry.numPlayers = 0;
    return true;
}

// Queries address:port synchronously, optionally through the master server.
// Requires SwiffHost and returns true when the SDK reports no error.
bool ServerBrowser::queryAddress(const std::string& address, int port, bool viaMaster)
{
    if (!m_browser)
        return false;
    ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton(std::string("SwiffHost")));
    if (!host)
        return false;
    host->isPrivateAddress(address);
    return ServerBrowserAuxUpdateIP(m_browser, address.c_str(), static_cast<unsigned short>(port),
                                    viaMaster ? SBTrue : SBFalse, SBFalse, SBTrue) == sbe_noerror;
}

bool ServerBrowser::removeAddress(const std::string& address, int port)
{
    if (!m_browser)
        return false;
    ServerBrowserRemoveIP(m_browser, address.c_str(), static_cast<unsigned short>(port));
    return true;
}

std::string& ServerBrowser::getFilter()
{
    return m_filter;
}

void ServerBrowser::setFilter(const std::string& filter)
{
    m_filter = filter;
}

SBServer ServerBrowser::getServer(const std::string& address, int port)
{
    if (!m_browser)
        return 0;
    m_suppressCallbacks = true;
    SBServer server = ServerBrowserGetServerByIP(m_browser, address.c_str(), static_cast<unsigned short>(port));
    m_suppressCallbacks = false;
    return server;
}

void ServerBrowser::setMaxPing(int maxPing)
{
    m_maxPing = maxPing;
    m_results.maxPing = maxPing;
}

void ServerBrowser::setKeepUnansweredServers(bool keep)
{
    m_keepUnansweredServers = keep;
}

// Routes SDK callbacks to this list. A query error clears the started flag.
void ServerBrowser::dispatchCallback(_ServerBrowser* browser, int reason, SBServer server, void* instance)
{
    ServerBrowser* list = static_cast<ServerBrowser*>(instance);
    if (!list)
        return;
    switch (reason)
    {
    case sbc_serveradded:
        list->onServerAdded(server);
        break;
    case sbc_serverupdated:
        list->onServerUpdated(server);
        break;
    case sbc_serverupdatefailed:
        {
            std::string error(ServerBrowserListQueryError(browser));
            (void)error;
        }
        break;
    case sbc_serverdeleted:
        list->onServerDeleted(server);
        break;
    case sbc_updatecomplete:
        list->onUpdateComplete();
        break;
    case sbc_queryerror:
        {
            std::string error(ServerBrowserListQueryError(browser));
            list->m_updateStarted = false;
            (void)error;
        }
        break;
    default:
        break;
    }
}

// a server with basic keys (or any server, for a list that keeps unanswered ones) is
// filled from its cached keys and added.
void ServerBrowser::onServerAdded(SBServer server)
{
    if (!SBServerHasBasicKeys(server) && !m_keepUnansweredServers)
        return;
    ui::ServerBrowserRecord record;
    if (fillRecord(server, &record, false, true))
        addRecord(m_results, record);
}

// the revision moves on every update; a suppressed one moves it twice and does nothing
// else. A LAN list only accepts servers whose "gamename" matches the settings' gameName. A listed
// server's record is refilled in place, an unlisted one added.
void ServerBrowser::onServerUpdated(SBServer server)
{
    ++m_results.revision;
    if (m_suppressCallbacks)
    {
        ++m_results.revision;
        return;
    }
    bool sameGame = true;
    if (!m_internet)
    {
        std::string gameName(settings->stringGet(std::string("gameName"), std::string("battlefield2")));
        sameGame = _stricmp(SBServerGetStringValue(server, qr2_registered_key_list[GAMENAME_KEY], ""), gameName.c_str()) == 0;
    }
    const bool hasBasicKeys = SBServerHasBasicKeys(server) != SBFalse;
    if ((hasBasicKeys || m_keepUnansweredServers) && sameGame)
    {
        if (ui::ServerBrowserRecord* record = findRecord(m_results, server))
        {
            fillRecord(server, record, false, true);
            ++m_results.revision;
        }
        else
        {
            ui::ServerBrowserRecord fresh;
            if (fillRecord(server, &fresh, false, true))
                addRecord(m_results, fresh);
        }
    }
}

void ServerBrowser::onServerDeleted(SBServer server)
{
    if (!m_suppressCallbacks)
        removeRecord(m_results, server);
}

// unless suppressed, the revision moves and the last sort is applied again; the
// started flag falls either way.
void ServerBrowser::onUpdateComplete()
{
    if (!m_suppressCallbacks)
    {
        ++m_results.revision;
        if (m_sortKey.size() != 0)
            sort(m_sortKey, m_sortAscending, m_sortStringCompare);
    }
    m_updateStarted = false;
}

} }
#endif // !dedicated
