// [TEMP] [CHOSEN, not recovered]
// Tests SwiffHost leaderboard/awards caching through real HTTP and persistence code, built on
// request as bf2_swiff_leaderboard_check (POSIX). Starts the local stats fixture
// (stats_fixture/bf2_stats_fixture.py) on 127.0.0.1:18124 and stops it before returning.
// The stats host is formed as the game forms it with +gamespy test.openbf2. gethostbyname and
// getaddrinfo below answer 127.0.0.1 for *.test.openbf2 and refuse every other name; connect
// sends 127.0.0.1:80 to the fixture's port. No name leaves the process.
// Injected cache objects avoid unrelated singleton/script paths; the collaborators the checks do
// not reach are scaffolding stubs that end the process if called.
#include <arpa/inet.h>
#include <dlfcn.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <spawn.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include "../BF2/Core/Ids.h"
#include "../BF2/Menu/SwiffHost.h"
#include "../BF2/Game/MatchMaking/OnlineProvider.h"
#include "../BF2/Game/Persistence/LeaderBoard.h"
#include "../BF2/Game/Persistence/PlayerAwards.h"

#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/LoadStat.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/SystemInfo.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Game/GeneralSettings.h"
#include "../BF2/Game/MapList.h"
#include "../BF2/Game/GameServer/ServerInfo.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/Game/Main/BF2Engine.h"
#include "../BF2/Game/Python/PythonHost.h"
#include "../BF2/Game/Python/PyRef.h"
#include "../BF2/Game/Utility/BuildNrUtil.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/File/FileManager.h"
#include "../BF2/IO/Localization/ExcelLexicon.h"
#include "../BF2/Platform/macOS/CocoaHost.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Sound/BfSound.h"

extern char** environ;

// Engine scaffolding: SwiffHost.cpp refers to these, the leaderboard and awards paths do not reach
// them. The globals stay null; the functions end the process if called.
namespace dice { namespace hfe {
ServerInfo* serverInfo = 0;
ClassManager* classManager = 0;
ServerSettings* serverSettings = 0;
IGame* game = 0;
IGameLogic* gameLogic = 0;
MapList* mapList = 0;
LoadStat* loadStat = 0;
SettingsRepostitory* settings = 0;
BF2Engine* bf2Engine = 0;
IBfSound* g_bfSound = 0;
namespace io {
FileManager* fileManager = 0;
OldConsole* mainConsole = 0;
IBase* controlMapManager = 0;
IBase* lexicon = 0;
std::string Console::ignoredString_;
}
namespace world { PlayerManager* playerManager = 0; }

[[noreturn]] void frontier(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] leaderboard check reached %s\n", name);
    std::abort();
}
ISystemInfo* systemInfo() { frontier("systemInfo"); }
const char* BuildNrUtil::getBuildNrString() { frontier("BuildNrUtil::getBuildNrString"); }
void GeneralSettings::addServerHistory(const std::string&, int, const std::string&, int) { frontier("GeneralSettings::addServerHistory"); }
void* macos::CocoaHost::loadCursorFromFile(const char*) { frontier("CocoaHost::loadCursorFromFile"); }
bool MapList::appendEntry(const std::string&, const std::string&, int, bool) { frontier("MapList::appendEntry"); }
void MapList::setCurrentMap(int, bool) { frontier("MapList::setCurrentMap"); }
bool MapList::clear(bool) { frontier("MapList::clear"); }
int MapList::getMapCount() const { frontier("MapList::getMapCount"); }
int MapList::getCurrentMap() const { frontier("MapList::getCurrentMap"); }
bool MapList::save() const { frontier("MapList::save"); }
const MapList::Entry* MapList::getEntry(int) const { frontier("MapList::getEntry"); }
int ServerSettings::getMaxPlayers() const { frontier("ServerSettings::getMaxPlayers"); }
bool SwiffHost::isLocalPlayerName(const std::string&) { frontier("SwiffHost::isLocalPlayerName"); }
bool SwiffHost::setEndOfRoundInfo(const std::string&, float) { frontier("SwiffHost::setEndOfRoundInfo"); }
bool SwiffHost::getEndOfRoundSummary(SwiffEndOfRoundSummary&) { frontier("SwiffHost::getEndOfRoundSummary"); }
namespace python {
PyRef::~PyRef() {}
PythonHost* pythonHost() { frontier("pythonHost"); }
void PythonHost::handleEvent(ScriptEvent, PyObject*) { frontier("PythonHost::handleEvent"); }
PyRef buildArgs(const int&, world::IPlayer* const&, const std::string&) { frontier("buildArgs"); }
PyRef buildArgs(const int&, world::IPlayer* const&, const std::vector<std::vector<int>>&) { frontier("buildArgs"); }
}
} }

namespace dice { namespace hfe {
struct SwiffHostFixture
{
    static void inject(SwiffHost& host, IBase* board, IBase* awards)
    {
        host.m_leaderBoard = board;
        host.m_playerAwards = awards;
    }
    static const SwiffLeaderboardCache& global(const SwiffHost& host) { return host.m_leaderboard; }
    static size_t players(const SwiffHost& host) { return host.m_leaderboardByPlayer.size(); }
    static size_t awardLists(const SwiffHost& host) { return host.m_awardCache.size(); }
};
} }

namespace {
const int kPort = 18124;   // the fixture's port; never 16567
const char kDomain[] = "test.openbf2";
int refusedNames = 0;

bool inTestDomain(const char* name)
{
    const size_t domain = sizeof(kDomain) - 1;
    const size_t length = name ? std::strlen(name) : 0;
    const bool ok = length > domain && name[length - domain - 1] == '.' && strcasecmp(name + length - domain, kDomain) == 0;
    if (!ok)
    {
        ++refusedNames;
        std::printf("FAIL: a name outside %s was looked up: %s\n", kDomain, name ? name : "(null)");
    }
    return ok;
}
} // namespace

extern "C" struct hostent* gethostbyname(const char* name)
{
    if (!inTestDomain(name))
    {
        h_errno = HOST_NOT_FOUND;
        return 0;
    }
    static struct in_addr address;
    static char* addresses[2];
    static char* noAliases[1];
    static char canonical[256];
    static struct hostent entry;
    address.s_addr = htonl(INADDR_LOOPBACK);
    addresses[0] = reinterpret_cast<char*>(&address);
    addresses[1] = 0;
    noAliases[0] = 0;
    std::strncpy(canonical, name, sizeof(canonical) - 1);
    entry.h_name = canonical;
    entry.h_aliases = noAliases;
    entry.h_addrtype = AF_INET;
    entry.h_length = 4;
    entry.h_addr_list = addresses;
    return &entry;
}

extern "C" int getaddrinfo(const char* node, const char*, const struct addrinfo*, struct addrinfo** result)
{
    if (!inTestDomain(node))
        return EAI_NONAME;
    struct addrinfo* info = static_cast<struct addrinfo*>(std::calloc(1, sizeof(struct addrinfo)));
    struct sockaddr_in* address = static_cast<struct sockaddr_in*>(std::calloc(1, sizeof(struct sockaddr_in)));
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    info->ai_family = AF_INET;
    info->ai_socktype = SOCK_STREAM;
    info->ai_addrlen = sizeof(struct sockaddr_in);
    info->ai_addr = reinterpret_cast<struct sockaddr*>(address);
    *result = info;
    return 0;
}

extern "C" void freeaddrinfo(struct addrinfo* info)
{
    if (info)
    {
        std::free(info->ai_addr);
        std::free(info);
    }
}

// 127.0.0.1:80 goes to the fixture; any peer other than loopback is refused.
extern "C" int connect(int fd, const struct sockaddr* address, socklen_t length)
{
    typedef int (*Connect)(int, const struct sockaddr*, socklen_t);
    static Connect real = reinterpret_cast<Connect>(dlsym(RTLD_NEXT, "connect"));
    struct sockaddr_in changed;
    if (!address || address->sa_family != AF_INET || length < sizeof(changed))
    {
        errno = ECONNREFUSED;
        return -1;
    }
    std::memcpy(&changed, address, sizeof(changed));
    if (changed.sin_addr.s_addr != htonl(INADDR_LOOPBACK))
    {
        errno = ECONNREFUSED;
        return -1;
    }
    if (changed.sin_port == htons(80))
        changed.sin_port = htons(static_cast<unsigned short>(kPort));
    return real(fd, reinterpret_cast<struct sockaddr*>(&changed), sizeof(changed));
}

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* what)
{
    ++checks;
    if (!ok)
    {
        ++failures;
        std::printf("FAIL: %s\n", what);
    }
}

// The stats fixture, as a child process on its own port.
pid_t g_server = 0;
bool startServer(const char* python, const char* fixtureDir)
{
    const std::string script = std::string(fixtureDir) + "/bf2_stats_fixture.py";
    char port[16];
    std::snprintf(port, sizeof port, "%d", kPort);
    const char* argv[] = { python, "-I", "-u", script.c_str(), "--port", port, "--lb-size", "57",
                           "--lb-position", "42", "--lb-pid", "1", 0 };
    if (posix_spawn(&g_server, python, 0, 0, const_cast<char* const*>(argv), environ) != 0)
    {
        g_server = 0;
        return false;
    }
    // Wait for the listener (up to ten seconds).
    typedef int (*Connect)(int, const struct sockaddr*, socklen_t);
    static Connect real = reinterpret_cast<Connect>(dlsym(RTLD_NEXT, "connect"));
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const int s = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in a = {};
        a.sin_family = AF_INET;
        a.sin_port = htons(static_cast<unsigned short>(kPort));
        a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        const bool up = real(s, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0;
        close(s);
        if (up)
            return true;
        usleep(100000);
    }
    return false;
}
void stopServer()
{
    if (g_server > 0)
    {
        kill(g_server, SIGTERM);
        int status = 0;
        waitpid(g_server, &status, 0);
        g_server = 0;
    }
}
} // namespace

int main(int argc, char** argv)
{
    using namespace dice::hfe;
    if (argc != 3)
    {
        std::printf("usage: swiff_leaderboard_runtime <python3> <stats fixture directory>\n");
        return 2;
    }
    std::string error;
    if (!onlineProvider::setDomain(kDomain, error))
    {
        std::printf("FAIL: +gamespy %s refused: %s\n", kDomain, error.c_str());
        return 1;
    }
    if (!startServer(argv[1], argv[2]))
    {
        std::printf("FAIL: the stats fixture did not come up on 127.0.0.1:%d\n", kPort);
        stopServer();
        return 1;
    }

    LeaderBoard* board = new LeaderBoard();
    PlayerAwards* awards = new PlayerAwards();
    SwiffHost* host = static_cast<SwiffHost*>(SwiffHost::create(IID_ISwiffHost, nullptr));
    check(host != nullptr, "the SwiffHost factory constructs the host");
    if (!host)
    {
        stopServer();
        return 1;
    }
    SwiffHostFixture::inject(*host, static_cast<IBase*>(static_cast<ILeaderBoard*>(board)),
                             static_cast<IBase*>(static_cast<IPlayerAwards*>(awards)));
    const SwiffLeaderboardCache& global = SwiffHostFixture::global(*host);

    // 1. A global kit board (request 7 = kit 2, group 7) from position 41: rows 41..57 of 57.
    check(host->fetchLeaderboard(0, 7, 41), "fetchLeaderboard(0, 7, 41) answers true");
    check(global.kits[2].numPlayers == 57, "the board's size is getNumPlayers (the answer's \"size\")");
    check(global.kits[2].updated.size() == 16 && global.kits[2].updated[4] == '-' && global.kits[2].updated[13] == ':',
          "the board's date is 0x487DC0's \"%d-%s-%.2d %.2d:%.2d\"");
    check(global.kits[2].rows.size() == 17 && global.position == 41, "seventeen rows from 41, and the position is stored");
    if (!global.kits[2].rows.empty())
    {
        const SwiffLeaderboardRow& r = global.kits[2].rows[0];
        check(r.n == 41 && r.pid == 40000041 && r.nick == "Player041", "row 0 is n 41, its pid and nick");
        check(r.values[0] == 59123 && r.values[1] == 59123 && r.values[2] == 309406,
              "a kit row reads killswith, deathsby and timeplayed");
        check(r.playerRank == 19 && r.countryCode == "GB", "and playerrank / countrycode");
    }

    // 2. getLeaderboard at the same position does not refetch; another position does.
    SwiffLeaderboardCache copy;
    check(host->getLeaderboard(&copy, 7, 41) && copy.kits[2].rows.size() == 17 && copy.position == 41,
          "getLeaderboard(7, 41) copies the cached board out");
    check(host->getLeaderboard(&copy, 7, 21) && copy.kits[2].rows.size() == 20 && copy.kits[2].rows[0].n == 21 && copy.position == 21,
          "another position refetches: twenty rows from 21");

    // 3. The rising star (request 1): the weekly score is kept as "%.2f" plus "%".
    check(host->fetchLeaderboard(0, 1, 1) && global.risingStar.rows.size() == 20, "the rising-star board: twenty rows");
    if (!global.risingStar.rows.empty())
        check(global.risingStar.rows[0].weeklyScore == "99.13%" && global.risingStar.rows[0].date.size() == 10,
              "weeklyscore 99.1300 is kept as \"99.13%\", the date as its wire text");

    // 4. A player board: an uncached player is refused by the fetcher, created by the reader.
    check(!host->fetchLeaderboard(77, 21, 0), "fetchLeaderboard refuses a player it has no cache for");
    SwiffLeaderboardCache mine;
    check(host->getLeaderboardPlayer(77, &mine, 21), "getLeaderboardPlayer(77, 21) answers true");
    check(SwiffHostFixture::players(*host) == 1, "the player's cache now exists");
    check(mine.weapons[2].rows.size() == 1 && mine.weapons[2].rows[0].pid == 77 && mine.weapons[2].rows[0].n == 42,
          "weapon 2 (request 21, group 14) holds the player's own row at 42");
    if (!mine.weapons[2].rows.empty())
    {
        const SwiffLeaderboardWeaponRow& w = mine.weapons[2].rows[0];
        check(w.values[0] == 58126 && w.values[1] == 0 && w.values[2] == 308172 && w.values[3] == 18,
              "a weapon row: killswith, \"detahsby\" [sic] reads 0 for the served deathsby, timeused, accuracy");
    }
    check(host->getLeaderboardPlayerPosition(77, 21) == 42, "getLeaderboardPlayerPosition answers row 0's n");
    check(host->getLeaderboardPlayerPosition(77, 5) == 0 && host->getLeaderboardPlayerPosition(78, 21) == 0 &&
          host->getLeaderboardPlayerPosition(0, 21) == 0,
          "an empty board, an uncached player and player 0 answer 0");

    // 5. goLeaderboardPlayer: weapon category 3, board 2 = request 21; the page starts nine above.
    host->setLeaderboardCategory(3);
    host->setLeaderboardId(2);
    host->goLeaderboardPlayer(77);
    check(host->getLeaderboardPosition() == 33, "goLeaderboardPlayer sets the position to 42 - 9");
    check(global.position == 33 && global.weapons[2].rows.size() == 20 && global.weapons[2].rows[0].n == 33,
          "and fetched the global weapon board from 33");
    host->setLeaderboardPosition(-4);
    check(host->getLeaderboardPosition() == 1, "setLeaderboardPosition clamps to 1");

    // 6. An out-of-range request fetches nothing but still stores the position.
    check(host->fetchLeaderboard(0, 31, 9) && global.position == 9, "request 31: true, position stored, no fetch");

    // 7. The awards cache: one HTTP fetch per profile, then the cached list.
    IPlayerAwards::AwardList list;
    check(host->getCachedAwards(77, &list), "getCachedAwards(77) answers true");
    check(list.profileId == 77 && list.awards.size() == 9, "nine awards for profile 77");
    if (list.awards.size() == 9)
    {
        check(list.awards[0].id == 2021403 && list.awards[0].type == 0, "a medal (2000001..3000000) is type 0");
        check(list.awards[3].id == 1031406 && list.awards[3].level == 3 && list.awards[3].type == 1, "a badge keeps its level, type 1");
        check(list.awards[6].id == 3191305 && list.awards[6].type == 2, "a ribbon (above 3000000) is type 2");
        check(list.awards[0].when > 1700000000 && list.awards[0].first > 1700000000, "when / first are the served times");
    }
    check(list.timestamp > 1700000000, "the list is stamped when its answer arrives");
    IPlayerAwards::AwardList again;
    check(host->getCachedAwards(77, &again) && again.awards.size() == 9 && SwiffHostFixture::awardLists(*host) == 1,
          "a second ask is served from the one cached list");

    // 8. resetStatsCache: the rows, the player map and the awards go; the position stays.
    host->resetStatsCache();
    check(global.kits[2].rows.empty() && global.weapons[2].rows.empty() && global.risingStar.rows.empty(),
          "resetStatsCache empties every global board's rows");
    check(global.kits[2].numPlayers == 57 && global.position == 9, "but keeps their counts and the position");
    check(SwiffHostFixture::players(*host) == 0 && SwiffHostFixture::awardLists(*host) == 0,
          "and drops the player caches and the awards cache");

    host->release();
    stopServer();
    check(refusedNames == 0, "no name outside test.openbf2 was looked up");
    std::printf("swiff_leaderboard_runtime: %d/%d checks passed; %d failures\n", checks - failures, checks, failures);
    return failures ? 1 : 0;
}
