// [TEMP] [CHOSEN, not recovered] Headless check of the stats web requests and their parsers
// (Code/BF2/Game/Persistence), built on request as bf2_persistence_stats_check; POSIX only.
//
// The real request and parse code runs against a 127.0.0.1 HTTP listener that serves canned bodies
// in the tab-separated O/H/D layout, for every stats web endpoint the game requests: getplayerinfo
// (the three BFHQ queries and info=rank), getawardsinfo, getunlocksinfo, getbackendinfo, getrankinfo,
// ranknotification, getleaderboard (all 31 boards and the player row), searchforplayers, the
// selectunlock and createplayer posts, and VerifyPlayer. The host is formed as the game forms it
// with +gamespy test.openbf2 (http://BF2Web.test.openbf2/ASP/). The statistics connection
// (StatsManager, gamestats.<domain>) and the stage-net player-info request (PlayerStatsManager) are
// covered too.
//
// The bodies below are written by hand from what each parser reads (KeyParser.cpp, PlayerRankInfo.cpp
// and the callers' key names). Every value is invented test data: no backend output and no game data.
//
// No name leaves the process and nothing is sent beyond loopback: gethostbyname and getaddrinfo below
// replace the C library's for every object linked into this executable and answer 127.0.0.1 for
// *.test.openbf2 only; any other name is refused and fails the check. connect sends 127.0.0.1:80 to
// the listener's port and refuses every non-loopback peer. Production code is unchanged.
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <map>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <strings.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define private public   // ProfileManager::createStatsPlayer is private; the check calls it directly.
#include "../BF2/Game/ProfileManager.h"
#undef private
#include "../BF2/Game/MatchMaking/OnlineProvider.h"
#include "../BF2/Game/Persistence/HTTPRequest.h"
#include "../BF2/Game/Persistence/KeyParser.h"
#include "../BF2/Game/Persistence/PlayerStats.h"
#include "../BF2/Game/Persistence/PlayerAwards.h"
#include "../BF2/Game/Persistence/LeaderBoard.h"
#include "../BF2/Game/Persistence/PlayerUnlocks.h"
#include "../BF2/Game/Persistence/PlayerUnlockInfo.h"
#include "../BF2/Game/Persistence/PlayerRankInfo.h"
#include "../BF2/Game/Persistence/SearchForPlayer.h"
#include "../BF2/Game/Persistence/GSServerSideNameValidation.h"
#include "../BF2/Game/Persistence/StatsManager.h"
#include "../BF2/Game/Persistence/PlayerStatsManager.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/Game/GameLogic/ServerGameLogic.h"
#include "../BF2/Game/Python/PythonHost.h"
#include "../BF2/Game/Python/PyRef.h"
#include "../BF2/Game/IGame.h"
#include "../BF2/Game/IGameLogic.h"
#include "../BF2/Scene/Object/PlayerManager.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/System.h"
#include "../BF2/Core/StringUtil.h"

extern "C" {
#include "../BF2/External/GameSpy/common/gsCommon.h"
#include "../BF2/External/GameSpy/common/gsAvailable.h"
#include "../BF2/External/GameSpy/ghttp/ghttp.h"
}


// Engine scaffolding. StatsManager::connect reads serverSettings and the GameSpy identity;
// validateSoldierName reads the wall clock. The other globals stay null, and the functions below
// belong to paths this check never takes (Python notification, the server's unlock grant).
namespace dice { namespace hfe {
ServerSettings* serverSettings = 0;
System* g_system = 0;
ClassManager* classManager = 0;
IGame* game = 0;
IGameLogic* gameLogic = 0;
namespace world { PlayerManager* playerManager = 0; }
namespace gamespy {
extern const char* const g_gameName;
const char* const g_gameName = "battlefield2";
const char* getSecretKey() { return "hW6m9a"; }
}

[[noreturn]] void frontier(const char* name)
{
    std::fprintf(stderr, "[FRONTIER] persistence check reached %s\n", name);
    std::abort();
}
void ServerGameLogic::setUnlocks(world::IPlayer*, std::map<int, int>*) { frontier("ServerGameLogic::setUnlocks"); }
namespace python {
PyRef::~PyRef() {}
PythonHost* pythonHost() { frontier("pythonHost"); }
void PythonHost::handleEvent(ScriptEvent, PyObject*) { frontier("PythonHost::handleEvent"); }
PyRef buildArgs(const int&, world::IPlayer* const&, const std::string&) { frontier("buildArgs"); }
PyRef buildArgs(const int&, world::IPlayer* const&, const std::vector<int>&) { frontier("buildArgs"); }
PyRef buildArgs(const int&, world::IPlayer* const&, const std::vector<std::vector<int>>&) { frontier("buildArgs"); }
PyRef buildArgs(const std::string&, const std::string&, const int&, const int&, world::IPlayer* const&) { frontier("buildArgs"); }
}
} }

namespace {

const char kDomain[] = "test.openbf2";
const int kWebPort = 18127;   // stands in for port 80, see connect below
const int kGameStatsPort = 29920; // fixed by the SDK
const int kPid = 10000042;

// ---- the process's resolver -------------------------------------------------------------------
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
std::vector<std::string> g_lookups;   // every name asked for
std::vector<std::string> g_refused;   // names outside the test domain

bool inTestDomain(const char* name)
{
    if (!name)
        return false;
    const size_t length = std::strlen(name), domain = sizeof(kDomain) - 1;
    return length > domain && name[length - domain - 1] == '.'
        && strcasecmp(name + length - domain, kDomain) == 0;
}

bool recordLookup(const char* name)
{
    const bool ok = inTestDomain(name);
    pthread_mutex_lock(&g_lock);
    g_lookups.push_back(name ? name : "");
    if (!ok)
        g_refused.push_back(name ? name : "");
    pthread_mutex_unlock(&g_lock);
    return ok;
}

} // namespace

extern "C" struct hostent* gethostbyname(const char* name)
{
    if (!recordLookup(name))
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
    if (!recordLookup(node))
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

// 127.0.0.1:80 goes to the listener; any peer other than loopback is refused.
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
        changed.sin_port = htons(static_cast<unsigned short>(kWebPort));
    return real(fd, reinterpret_cast<struct sockaddr*>(&changed), sizeof(changed));
}

namespace {

using namespace dice::hfe;

// ---- checks -----------------------------------------------------------------------------------
int g_checks = 0, g_failures = 0;

void check(bool ok, const std::string& what)
{
    ++g_checks;
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

void checkEqual(const std::string& actual, const std::string& expected, const std::string& what)
{
    check(actual == expected, what + " = \"" + actual + "\"" + (actual == expected ? "" : " (expected \"" + expected + "\")"));
}

void checkEqual(long actual, long expected, const std::string& what)
{
    char text[96];
    if (actual == expected)
        std::snprintf(text, sizeof(text), " = %ld", actual);
    else
        std::snprintf(text, sizeof(text), " = %ld (expected %ld)", actual, expected);
    check(actual == expected, what + text);
}

double now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

// ---- canned bodies ----------------------------------------------------------------------------
// One table line: fields joined by TAB. The parsers split on TAB and LF only.
std::string line(std::initializer_list<const char*> fields)
{
    std::string text;
    for (std::initializer_list<const char*>::const_iterator it = fields.begin(); it != fields.end(); ++it)
        text += (it != fields.begin() ? "\t" : "") + std::string(*it);
    return text + "\n";
}
#define LINE(...) line({ __VA_ARGS__ })

// "O", the lines, then the "$ <length> $" trailer. No parser reads the length.
std::string body(const std::string& lines)
{
    return "O\n" + lines + "$\t" + formatString("%u", static_cast<unsigned>(lines.size())) + "\t$";
}

// ---- the stats web listener -------------------------------------------------------------------
struct Request
{
    std::string method, target, host, body;
};

std::map<std::string, std::string> g_bodies;   // endpoint file name (lower case) -> response body
std::vector<Request> g_requests;
int g_webFd = -1, g_statsFd = -1;
int g_statsConnections = 0;
std::string g_statsAnswer;
volatile bool g_stop = false;
pthread_t g_thread;

std::string lower(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] >= 'A' && text[i] <= 'Z')
            text[i] = static_cast<char>(text[i] - 'A' + 'a');
    return text;
}

std::string endpointOf(const std::string& target)
{
    std::string path = target.substr(0, target.find('?'));
    const std::string::size_type slash = path.rfind('/');
    return lower(slash == std::string::npos ? path : path.substr(slash + 1));
}

std::string header(const std::string& head, const std::string& name)
{
    const std::string wanted = "\r\n" + lower(name) + ":";
    const std::string text = lower(head);
    const std::string::size_type at = text.find(wanted);
    if (at == std::string::npos)
        return "";
    std::string::size_type start = at + wanted.size();
    const std::string::size_type end = head.find("\r\n", start);
    while (start < end && head[start] == ' ')
        ++start;
    return head.substr(start, end - start);
}

void serveWeb(int peer)
{
    struct timeval limit = { 2, 0 };
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    std::string data;
    char chunk[4096];
    ssize_t n;
    while (data.find("\r\n\r\n") == std::string::npos && (n = recv(peer, chunk, sizeof(chunk), 0)) > 0)
        data.append(chunk, static_cast<size_t>(n));
    const std::string::size_type headEnd = data.find("\r\n\r\n");
    if (headEnd == std::string::npos)
        return;
    const std::string head = data.substr(0, headEnd);
    const size_t length = static_cast<size_t>(std::atoi(header(head, "Content-Length").c_str()));
    std::string content = data.substr(headEnd + 4);
    while (content.size() < length && (n = recv(peer, chunk, sizeof(chunk), 0)) > 0)
        content.append(chunk, static_cast<size_t>(n));

    Request request;
    const std::string first = head.substr(0, head.find("\r\n"));
    const std::string::size_type space1 = first.find(' '), space2 = first.rfind(' ');
    request.method = first.substr(0, space1);
    request.target = first.substr(space1 + 1, space2 - space1 - 1);
    request.host = header(head, "Host");
    request.body = content;

    pthread_mutex_lock(&g_lock);
    g_requests.push_back(request);
    const std::map<std::string, std::string>::const_iterator found = g_bodies.find(endpointOf(request.target));
    const std::string reply = found != g_bodies.end() ? found->second : std::string("E\t404\n$\t0\t$");
    pthread_mutex_unlock(&g_lock);

    const std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: "
        + formatString("%u", static_cast<unsigned>(reply.size())) + "\r\nConnection: close\r\n\r\n" + reply;
    send(peer, response.data(), response.size(), 0);
}

// The statistics handshake: a challenge, the game's answer, a session key. All but the terminator
// is XORed with "GameSpy3D" (gstats).
void serveGameStats(int peer)
{
    struct timeval limit = { 1, 0 };
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    const char* steps[2] = { "\\lc\\1\\challenge\\ABCDEFGHIJ\\id\\1", "\\lc\\2\\sesskey\\1234567\\proof\\0\\id\\1" };
    char data[2048];
    for (int step = 0; step < 2; ++step)
    {
        std::string message(steps[step]);
        for (size_t c = 0; c < message.size(); ++c)
            message[c] ^= "GameSpy3D"[c % 9];
        message += "\\final\\";
        send(peer, message.data(), message.size(), 0);
        const ssize_t n = recv(peer, data, sizeof(data), 0);
        if (step == 0 && n > 0)
        {
            pthread_mutex_lock(&g_lock);
            g_statsAnswer.assign(data, static_cast<size_t>(n));
            pthread_mutex_unlock(&g_lock);
        }
    }
}

int openListener(int port)
{
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    int on = 1;
    if (fd < 0)
        return -1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (bind(fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0 || listen(fd, 8) != 0)
    {
        close(fd);
        return -1;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    return fd;
}

void* serve(void*)
{
    while (!g_stop)
    {
        fd_set readable;
        FD_ZERO(&readable);
        int highest = -1;
        const int fds[2] = { g_webFd, g_statsFd };
        for (int i = 0; i < 2; ++i)
            if (fds[i] >= 0)
            {
                FD_SET(fds[i], &readable);
                if (fds[i] > highest)
                    highest = fds[i];
            }
        struct timeval wait = { 0, 20000 };
        if (select(highest + 1, &readable, 0, 0, &wait) <= 0)
            continue;
        for (int i = 0; i < 2; ++i)
        {
            if (fds[i] < 0 || !FD_ISSET(fds[i], &readable))
                continue;
            const int peer = accept(fds[i], 0, 0);
            if (peer < 0)
                continue;
            fcntl(peer, F_SETFL, fcntl(peer, F_GETFL, 0) & ~O_NONBLOCK);
            if (fds[i] == g_webFd)
                serveWeb(peer);
            else
            {
                pthread_mutex_lock(&g_lock);
                ++g_statsConnections;
                pthread_mutex_unlock(&g_lock);
                serveGameStats(peer);
            }
            close(peer);
        }
    }
    return 0;
}

void setBody(const char* endpoint, const std::string& text)
{
    pthread_mutex_lock(&g_lock);
    g_bodies[lower(endpoint)] = text;
    pthread_mutex_unlock(&g_lock);
}

// The requests that arrived since the last call.
std::vector<Request> takeRequests()
{
    pthread_mutex_lock(&g_lock);
    std::vector<Request> requests;
    requests.swap(g_requests);
    pthread_mutex_unlock(&g_lock);
    return requests;
}

// Exactly one request arrived; its method, target and Host are as expected.
Request checkOneRequest(const std::string& method, const std::string& target, const std::string& host = "BF2Web.test.openbf2")
{
    const std::vector<Request> requests = takeRequests();
    checkEqual(static_cast<long>(requests.size()), 1, "requests that arrived");
    if (requests.empty())
        return Request();
    const Request& r = requests[0];
    checkEqual(r.method + " " + r.target, method + " " + target, "request line");
    checkEqual(r.host, host, "Host");
    return r;
}

// ---- the endpoints ----------------------------------------------------------------------------

// Menu/SwiffHost.cpp kPlayerInfoQuery: the first BFHQ query, verbatim from BF2.exe.
const char kPlayerInfoQuery[] =
    "&info=per*,cmb*,twsc,cpcp,cacp,dfcp,kila,heal,rviv,rsup,rpar,tgte,dkas,dsab,cdsc,rank,cmsc,kick,kill,deth,suic,ospm,"
    "klpm,klpr,dtpr,bksk,wdsk,bbrs,tcdr,ban,dtpm,lbtl,osaa,vrk,tsql,tsqm,tlwf,mvks,vmks,mvn*,vmr*,fkit,fmap,fveh,fwea,wtm"
    "-,wkl-,wdt-,wac-,wkd-,vtm-,vkl-,vdt-,vkd-,vkr-,atm-,awn-,alo-,abr-,ktm-,kkl-,kdt-,kkd-";

void checkPlayerInfo()
{
    std::puts("getplayerinfo: the overview query (stats mode: 4 tokens skipped, then the first H)");
    setBody("getplayerinfo.aspx", body(LINE("H", "asof") + LINE("D", "1700000000")
        + LINE("H", "pid", "nick", "scor", "jond", "wins", "loss", "rank", "kill", "deth", "Cpcp")
        + LINE("D", "10000042", "TestSoldier", "4321", "1600000000", "12", "8", "5", "345", "210", "17")));
    PlayerStats* stats = new PlayerStats();
    check(stats->getStats(kPid, kPlayerInfoQuery, false, false), "getStats (blocking) answers true");
    checkOneRequest("GET", std::string("/ASP/getplayerinfo.aspx?pid=10000042") + kPlayerInfoQuery);
    checkEqual(stats->getNumKeyMaps(), 1, "key maps");
    checkEqual(stats->getNumKeys(), 10, "keys");
    checkEqual(stats->getKeyValue(0, "nick"), "TestSoldier", "nick");
    checkEqual(stats->getKeyValue(0, "SCOR"), "4321", "SCOR (lookup ignores case)");
    checkEqual(stats->getKeyValue(0, "cpcp"), "17", "cpcp read for the header's Cpcp");
    checkEqual(stats->getKeyValue(0, "absent"), "", "a key the body lacks");
    check(!stats->isUpdating(), "nothing pending afterwards");

    std::puts("getplayerinfo: the favourites query (read by column position)");
    setBody("getplayerinfo.aspx", body(LINE("H", "asof") + LINE("D", "1700000000")
        + LINE("H", "pid", "nick", "ktm-1", "vtm-2", "wtm-3", "mtm-4")
        + LINE("D", "10000042", "TestSoldier", "3600", "1800", "900", "450")));
    check(stats->getStats(kPid, formatString("&info=ktm-,vtm-,wtm-,mtm-&kit=%d&vehicle=%d&weapon=%d&map=%d", 1, 2, 3, 4),
                          false, false), "getStats answers true");
    checkOneRequest("GET", "/ASP/getplayerinfo.aspx?pid=10000042&info=ktm-,vtm-,wtm-,mtm-&kit=1&vehicle=2&weapon=3&map=4");
    std::string key, value;
    check(stats->getKey(0, 2, key, value) && key == "ktm-1" && value == "3600", "column 2 is ktm-1 3600");
    check(stats->getKey(0, 5, key, value) && key == "mtm-4" && value == "450", "column 5 is mtm-4 450");
    check(!stats->getKey(0, 6, key, value), "no column 6");

    std::puts("getplayerinfo: the map query");
    setBody("getplayerinfo.aspx", body(LINE("H", "asof") + LINE("D", "1700000000")
        + LINE("H", "pid", "nick", "mtm-0", "mwn-0", "mls-0", "mtm-601", "mwn-601", "mls-601")
        + LINE("D", "10000042", "TestSoldier", "7200", "3", "2", "60", "0", "1")));
    check(stats->getStats(kPid, "&info=mtm-,mwn-,mls-", false, false), "getStats answers true");
    checkOneRequest("GET", "/ASP/getplayerinfo.aspx?pid=10000042&info=mtm-,mwn-,mls-");
    checkEqual(stats->getKeyValue(0, "mwn-0"), "3", "mwn-0");
    checkEqual(stats->getKeyValue(0, "mls-601"), "1", "mls-601");

    std::puts("getplayerinfo: info=rank, as the server's rank script asks for it");
    setBody("getplayerinfo.aspx", body(LINE("H", "asof") + LINE("D", "1700000000")
        + LINE("H", "pid", "nick", "rank") + LINE("D", "10000042", "TestSoldier", "5")));
    check(stats->getStats(kPid, "&info=rank", false, false), "getStats answers true");
    checkOneRequest("GET", "/ASP/getplayerinfo.aspx?pid=10000042&info=rank");
    checkEqual(stats->getKeyValue(0, "rank"), "5", "rank, with the H asof / D <time> prefix");
    // Without the prefix the parser's four skipped tokens fall into the table itself.
    setBody("getplayerinfo.aspx", body(LINE("H", "pid", "nick", "rank") + LINE("D", "10000042", "TestSoldier", "5")));
    check(stats->getStats(kPid, "&info=rank", false, false), "getStats answers true");
    takeRequests();
    checkEqual(stats->getKeyValue(0, "rank"), "", "rank, without the prefix: not found (the backend must send it)");

    std::puts("getplayerinfo: an error body");
    setBody("getplayerinfo.aspx", "E\t998\nH\tasof\terr\nD\t1700000000\tPlayer Not Found!\n$\t40\t$");
    check(stats->getStats(kPid, kPlayerInfoQuery, false, false), "getStats answers true (the transfer worked)");
    takeRequests();
    checkEqual(stats->getNumKeyMaps(), 0, "no key maps");
    stats->release();
}

void checkAwards()
{
    std::puts("getawardsinfo (stats mode: H pid asof / D, then award level when first)");
    setBody("getawardsinfo.aspx", body(LINE("H", "pid", "asof") + LINE("D", "10000042", "1700000000")
        + LINE("H", "award", "level", "when", "first")
        + LINE("D", "1000001", "2", "1650000000", "1600000000")
        + LINE("D", "2000002", "1", "1660000000", "0")
        + LINE("D", "3000003", "0", "1670000000", "0")));
    PlayerAwards* awards = new PlayerAwards();
    IPlayerAwards::AwardList list;
    check(awards->getAwards(kPid, &list, false, false), "getAwards (blocking) answers true");
    checkOneRequest("GET", "/ASP/getawardsinfo.aspx?pid=10000042");
    checkEqual(static_cast<long>(list.awards.size()), 3, "awards");
    checkEqual(list.profileId, kPid, "the list's profile id");
    if (list.awards.size() == 3)
    {
        checkEqual(list.awards[0].id, 1000001, "award 0 id");
        checkEqual(list.awards[0].level, 2, "award 0 level");
        checkEqual(list.awards[0].when, 1650000000, "award 0 when");
        checkEqual(list.awards[0].first, 1600000000, "award 0 first");
        checkEqual(list.awards[0].type, 1, "award 0 type (id below 2000001: badge)");
        checkEqual(list.awards[1].type, 0, "award 1 type (medal)");
        checkEqual(list.awards[2].type, 2, "award 2 type (id above 3000000: ribbon)");
    }
    check(list.timestamp > 0, "the list is time-stamped");
    awards->release();
}

void checkUnlocks()
{
    std::puts("getunlocksinfo (unlocks mode: 13 tokens skipped, then enlisted and officer)");
    setBody("getunlocksinfo.aspx", body(LINE("H", "pid", "nick", "asof") + LINE("D", "10000042", "TestSoldier", "1700000000")
        + LINE("H", "enlisted", "officer") + LINE("D", "2", "1")
        + LINE("H", "id", "state") + LINE("D", "11", "s") + LINE("D", "22", "n") + LINE("D", "88", "x")));
    PlayerUnlocks* unlocks = new PlayerUnlocks();
    IPlayerUnlocks::UnlockCache cache;
    cache.available = cache.selected = -1;
    check(unlocks->getUnlocks(kPid, cache), "getUnlocks(cache) (blocking) answers true");
    checkOneRequest("GET", "/ASP/getunlocksinfo.aspx?pid=10000042");
    checkEqual(cache.available, 2, "available (enlisted)");
    checkEqual(cache.selected, 1, "selected (officer)");
    checkEqual(static_cast<long>(cache.items.size()), 3, "items");
    if (cache.items.size() == 3)
    {
        check(cache.items[0].id == 11 && cache.items[0].status == 1, "11 s -> status 1");
        check(cache.items[1].id == 22 && cache.items[1].status == 0, "22 n -> status 0");
        check(cache.items[2].id == 88 && cache.items[2].status == 2, "88 x -> status 2");
    }

    std::puts("getunlocksinfo: the list mode keeps only the selected ids");
    std::vector<int> ids;
    check(unlocks->getUnlocks(kPid, false, ids), "getUnlocks(list, blocking) answers true");
    takeRequests();
    check(ids.size() == 1 && ids[0] == 11, "only 11 is listed");

    std::puts("selectunlock (an empty form post)");
    setBody("selectunlock.aspx", body(LINE("H", "response") + LINE("D", "OK")));
    check(unlocks->setUnlocks(kPid, 22), "setUnlocks answers true");
    const Request post = checkOneRequest("POST", "/ASP/selectunlock.aspx?pid=10000042&id=22");
    check(post.body.find("BF2.HTTPRequest.Post") != std::string::npos, "the body carries the BF2.HTTPRequest.Post field");
    unlocks->release();
}

void checkBackendInfo()
{
    std::puts("getbackendinfo with rnk (search mode)");
    setBody("getbackendinfo.aspx", body(LINE("H", "ver", "rnk", "now") + LINE("D", "0.1", "1", "1700000000")
        + LINE("H", "id", "kit", "name", "descr")
        + LINE("D", "11", "0", "Test unlock A", "First invented unlock")
        + LINE("D", "22", "1", "Test unlock B", "Second invented unlock")
        + LINE("D", "88", "1", "Test unlock C", "Third invented unlock")));
    PlayerUnlockInfo* info = new PlayerUnlockInfo();
    check(info->init(), "init (blocking) answers true");
    checkOneRequest("GET", "/ASP/getbackendinfo.aspx");
    check(info->getRanked(), "ranked: the header has rnk");
    check(info->getVersion() == 0.0f, "version is atoi(\"0.1\") = 0");
    std::vector<IPlayerUnlockInfo::Item> items;
    info->getItems(items);
    checkEqual(static_cast<long>(items.size()), 3, "items");
    if (items.size() == 3)
    {
        check(items[1].id == 22 && items[1].kit == 1, "item 1 is id 22, kit 1");
        checkEqual(items[1].name, "Test unlock B", "item 1 name (spaces kept)");
        checkEqual(items[1].description, "Second invented unlock", "item 1 description");
    }
    items.clear();
    info->getItemsByKit(1, items);
    checkEqual(static_cast<long>(items.size()), 2, "items for kit 1");
    IPlayerUnlockInfo::Item item;
    check(info->getItemById(88, item) && item.name == "Test unlock C", "getItemById(88)");
    info->release();

    std::puts("getbackendinfo without rnk");
    setBody("getbackendinfo.aspx", body(LINE("H", "ver", "now") + LINE("D", "0.1", "1700000000")
        + LINE("H", "id", "kit", "name", "descr") + LINE("D", "11", "0", "Test unlock A", "First invented unlock")));
    info = new PlayerUnlockInfo();
    check(info->init(), "init answers true");
    takeRequests();
    check(!info->getRanked(), "not ranked: no rnk in the header");
    items.clear();
    info->getItems(items);
    check(items.size() == 1 && items[0].id == 11, "the item table still parses");
    info->release();
}

void checkRank()
{
    std::puts("getrankinfo (its own parser: from O to the first H)");
    setBody("getrankinfo.aspx", body(LINE("H", "rank", "chng", "decr") + LINE("D", "7", "1", "0")));
    PlayerRankInfo* rank = new PlayerRankInfo();
    IPlayerRankInfo::RankInfo info = {};
    check(rank->getRankInfo(kPid, info), "getRankInfo (blocking) answers true");
    checkOneRequest("GET", "/ASP/getrankinfo.aspx?pid=10000042");
    checkEqual(info.profileId, kPid, "profile id");
    checkEqual(info.rank, 7, "rank");
    checkEqual(info.change, 1, "chng");
    checkEqual(info.decrease, 0, "decr");
    check(info.timestamp > 0, "time-stamped");

    std::puts("ranknotification");
    setBody("ranknotification.aspx", body(LINE("H", "response") + LINE("D", "OK")));
    check(rank->clearRankNotification(kPid), "clearRankNotification answers true");
    checkOneRequest("GET", "/ASP/ranknotification.aspx?pid=10000042");
    rank->release();

    std::puts("getrankinfo: an error body");
    setBody("getrankinfo.aspx", "E\t998\n$\t2\t$");
    rank = new PlayerRankInfo();
    check(!rank->getRankInfo(kPid, info), "getRankInfo answers false without a table");
    takeRequests();
    rank->release();
}

// Menu/SwiffHost.cpp fetchLeaderboard: the 31 board queries in request order.
std::vector<std::string> boardQueries()
{
    static const char* const score[5] = { "type=score&id=overall", "type=risingstar", "type=score&id=commander",
                                          "type=score&id=team", "type=score&id=combat" };
    std::vector<std::string> queries(score, score + 5);
    for (int i = 0; i < 7; ++i)
        queries.push_back(formatString("type=kit&id=%d", i));
    for (int i = 0; i < 7; ++i)
        queries.push_back(formatString("type=vehicle&id=%d", i));
    for (int i = 0; i < 12; ++i)
        queries.push_back(formatString("type=weapon&id=%d", i));
    return queries;
}

void checkLeaderboard()
{
    std::puts("getleaderboard (type 2 mode: H size asof / D, then the rows)");
    setBody("getleaderboard.aspx", body(LINE("H", "size", "asof") + LINE("D", "57", "1700000000")
        + LINE("H", "n", "pid", "nick", "killswith", "detahsby", "timeplayed", "playerrank", "countrycode")
        + LINE("D", "21", "10000021", "Player021", "300", "120", "86400", "9", "GB")
        + LINE("D", "22", "10000022", "Player022", "290", "130", "82800", "8", "DE")));
    LeaderBoard* board = new LeaderBoard();
    const std::vector<std::string> queries = boardQueries();
    int arrived = 0;
    for (size_t i = 0; i < queries.size(); ++i)
    {
        const std::string query = queries[i] + "&pos=21&before=0&after=19";
        const bool answered = board->getLeaderBoard(query, false);
        const std::vector<Request> requests = takeRequests();
        if (answered && requests.size() == 1 && requests[0].target == "/ASP/getleaderboard.aspx?" + query
            && requests[0].host == "BF2Web.test.openbf2" && board->getNumPlayers() == 57 && board->getNumKeyMaps() == 2)
            ++arrived;
        else
            std::printf("  board %u (%s) not as expected\n", static_cast<unsigned>(i), query.c_str());
    }
    checkEqual(arrived, 31, "boards requested, arrived and parsed");
    checkEqual(board->getUpdated(), 1700000000, "asof");
    KeyMap* row = board->getKeyMap(1);
    check(row && row->findKey("nick") && row->findKey("nick")->value == "Player022", "row 1 nick");
    check(row && row->findKey("detahsby") && row->findKey("detahsby")->value == "130",
          "row 1 detahsby (the client's spelling for vehicle and weapon deaths)");

    std::puts("getleaderboard: the player's own row");
    setBody("getleaderboard.aspx", body(LINE("H", "size", "asof") + LINE("D", "57", "1700000000")
        + LINE("H", "n", "pid", "nick", "killswith", "detahsby", "timeplayed", "playerrank", "countrycode")
        + LINE("D", "42", "10000042", "TestSoldier", "100", "90", "36000", "5", "SE")));
    check(board->getLeaderBoardPlayer(kPid, "type=kit&id=2", false), "getLeaderBoardPlayer answers true");
    checkOneRequest("GET", "/ASP/getleaderboard.aspx?pid=10000042&type=kit&id=2");
    row = board->getKeyMap(0);
    check(board->getNumKeyMaps() == 1 && row && row->findKey("n") && row->findKey("n")->value == "42", "one row, position 42");

    std::puts("getleaderboard: an empty board");
    setBody("getleaderboard.aspx", body(LINE("H", "size", "asof") + LINE("D", "0", "1700000000")
        + LINE("H", "n", "pid", "nick", "score")));
    check(board->getLeaderBoard("type=score&id=overall&pos=1&before=0&after=19", false), "getLeaderBoard answers true");
    takeRequests();
    check(board->getNumPlayers() == 0 && board->getNumKeyMaps() == 0, "size 0, no rows (a header alone is cleared)");
    board->release();
}

void checkSearch()
{
    std::puts("searchforplayers (stats mode; the nick encoded as retail encodes it)");
    setBody("searchforplayers.aspx", body(LINE("H", "asof") + LINE("D", "1700000000")
        + LINE("H", "n", "pid", "nick", "score")
        + LINE("D", "1", "10000042", "Test Soldier[1]", "4321")
        + LINE("D", "2", "10000043", "Test Soldier[2]", "12")));
    SearchForPlayer* search = new SearchForPlayer();
    check(search->search("Test Soldier[1]", false, false), "search (partial, blocking) answers true");
    checkOneRequest("GET", "/ASP/searchforplayers.aspx?nick=Test%20Soldier%5B1%5D&where=a&sort=a");
    std::vector<ISearchForPlayer::Player> players;
    search->getPlayers(players);
    checkEqual(static_cast<long>(players.size()), 2, "players");
    if (players.size() == 2)
    {
        checkEqual(players[0].profileId, kPid, "player 0 pid");
        checkEqual(players[0].nick, "Test Soldier[1]", "player 0 nick");
        checkEqual(players[1].score, 12, "player 1 score");
    }
    check(search->search("a:b@c", true, false), "search (exact) answers true");
    checkOneRequest("GET", "/ASP/searchforplayers.aspx?nick=a:b@c&where=x&sort=a");
    search->release();
}

// VerifyPlayer's completion looks the player up in the player manager and raises a Python event,
// neither of which exists here. The request is the production one; the body goes to the same
// parser call that onGetComplete makes.
struct Verification : GSServerSideNameValidation
{
    bool done;
    std::string response;
    Verification() : done(false) {}
    void onGetComplete(const std::string& text) override
    {
        response = text;
        done = true;
        HTTPRequest::onGetComplete(text);
        --outstandingRequests;
    }
    void onGetFailure(const std::string& text) override
    {
        done = true;
        HTTPRequest::onGetFailure(text);
        --outstandingRequests;
    }
};

void checkVerifyPlayer()
{
    std::puts("VerifyPlayer (asynchronous; validation mode)");
    // One layout that satisfies the parser's positions: token 7 pid, 8 nick, 9 spid, 14 result.
    const std::string text = body(LINE("H", "pid", "nick", "spid", "asof") + LINE("D", "10000042", "TestSoldier", "10000042", "1700000000")
        + LINE("H", "result") + LINE("D", "InvalidReportedNick"));
    setBody("VerifyPlayer.aspx", text);
    Verification* verification = new Verification();
    int dummyServer = 0;
    check(verification->validateSoldierName("Test[Nick]", "", kPid, "authtoken123", 3,
                                            reinterpret_cast<GameServer*>(&dummyServer)),
          "validateSoldierName answers true");
    checkEqual(GSServerSideNameValidation::outstandingRequests, 1, "outstanding requests while pending");
    const double end = now() + 5.0;
    while (!verification->done && now() < end)
    {
        verification->update();
        msleep(5);
    }
    check(verification->done, "the request completed");
    checkOneRequest("GET", "/ASP/VerifyPlayer.aspx?auth=authtoken123&SoldierNick=Test[Nick]&pid=10000042");
    checkEqual(verification->response, text, "the body reached the completion");
    KeyParser parser;
    std::string metadata[4];
    check(parser.parse(verification->response, KeyParser::Validation, metadata), "the validation parse succeeds");
    checkEqual(metadata[0], "10000042", "profile id (token 7)");
    checkEqual(metadata[1], "TestSoldier", "verified nick (token 8)");
    checkEqual(metadata[2], "10000042", "token 9");
    checkEqual(metadata[3], "InvalidReportedNick", "result (token 14)");
    verification->release();
}

void checkCreatePlayer()
{
    std::puts("createplayer (ProfileManager::createStatsPlayer, an empty form post)");
    setBody("createplayer.aspx", body(""));
    ProfileManager manager;
    check(manager.createStatsPlayer(kPid, "SE", "TestSoldier"), "createStatsPlayer answers true");
    const Request post = checkOneRequest("POST", "/ASP/createplayer.aspx?pid=10000042&cid=SE&nick=TestSoldier");
    check(post.body.find("BF2.HTTPRequest.Post") != std::string::npos, "the body carries the BF2.HTTPRequest.Post field");
}

void checkStageNet()
{
    std::puts("stage-net player info (PlayerStatsManager, asynchronous, comma separated)");
    PlayerStatsManager manager;
    std::string values;
    for (size_t i = 0; i < manager.getKeys().size(); ++i)
        values += formatString(i ? ",%u" : "%u", static_cast<unsigned>(i + 1));
    values += ",";
    setBody("getplayerinfo.aspx", values);
    manager.connect();
    check(manager.update(), "update answers true");
    const double end = now() + 5.0;
    while (manager.isUpdating() && now() < end)
    {
        ghttpThink();
        msleep(5);
    }
    check(!manager.isUpdating(), "the request completed");
    const std::vector<Request> requests = takeRequests();
    check(requests.size() == 1 && requests[0].target.find("/bf2/getplayerinfo.aspx?pid=&getarg=pID,pID,cID,") == 0,
          "GET /bf2/getplayerinfo.aspx?pid=&getarg=pID,pID,cID,...");
    check(requests.size() == 1 && requests[0].host == "stage-net.test.openbf2", "Host stage-net.test.openbf2");
    checkEqual(manager.getKeys()[2].numberValue, 3, "the third key (cID) holds the third value");
    checkEqual(manager.getKeys()[9].textValue, "10", "the hostname key keeps its text");
    manager.disconnect();
}

void checkGameStats()
{
    std::puts("gamestats connection (StatsManager::connect)");
    if (g_statsFd < 0)
    {
        std::printf("  skip: 127.0.0.1:%d not available\n", kGameStatsPort);
        return;
    }
    serverSettings = static_cast<ServerSettings*>(std::calloc(1, sizeof(ServerSettings)));
    // The game runs the availability check (UDP 27900) before the first statistics connection;
    // bf2_online_provider_check covers it. Here it is taken as passed.
    __GSIACResult = GSIACAvailable;
    StatsManager* manager = new StatsManager();
    check(manager->connect(), "connect answers true after the handshake");
    pthread_mutex_lock(&g_lock);
    const int connections = g_statsConnections;
    const bool answered = !g_statsAnswer.empty();
    pthread_mutex_unlock(&g_lock);
    checkEqual(connections, 1, "connections at 127.0.0.1:29920");
    check(answered, "the game answered the challenge");
    manager->release();
}

} // namespace

int main()
{
    g_system = new System();
    std::string error;
    if (!onlineProvider::setDomain(kDomain, error))
    {
        std::printf("FAIL: +gamespy %s refused: %s\n", kDomain, error.c_str());
        return 1;
    }
    onlineProvider::prepareGameSpySdk();
    std::printf("domain %s, stats web %s\n", onlineProvider::domain().c_str(), onlineProvider::statsWebUrl().c_str());

    g_webFd = openListener(kWebPort);
    if (g_webFd < 0)
    {
        std::printf("FAIL: cannot listen on 127.0.0.1:%d (%s)\n", kWebPort, std::strerror(errno));
        return 1;
    }
    g_statsFd = openListener(kGameStatsPort);
    pthread_create(&g_thread, 0, &serve, 0);

    checkPlayerInfo();
    checkAwards();
    checkUnlocks();
    checkBackendInfo();
    checkRank();
    checkLeaderboard();
    checkSearch();
    checkVerifyPlayer();
    checkCreatePlayer();
    checkStageNet();
    checkGameStats();

    g_stop = true;
    pthread_join(g_thread, 0);

    std::puts("names");
    pthread_mutex_lock(&g_lock);
    bool web = false, stage = false, stats = false;
    for (size_t i = 0; i < g_lookups.size(); ++i)
    {
        web = web || g_lookups[i] == "BF2Web.test.openbf2";
        stage = stage || g_lookups[i] == "stage-net.test.openbf2";
        stats = stats || g_lookups[i] == "battlefield2.gamestats.test.openbf2";
    }
    std::string refused;
    for (size_t i = 0; i < g_refused.size(); ++i)
        refused += (i ? "," : "") + g_refused[i];
    pthread_mutex_unlock(&g_lock);
    check(web, "BF2Web.test.openbf2 was looked up");
    check(stage, "stage-net.test.openbf2 was looked up");
    if (g_statsFd >= 0)
        check(stats, "battlefield2.gamestats.test.openbf2 was looked up");
    check(refused.empty(), "no name outside test.openbf2 was looked up" + (refused.empty() ? std::string() : ": " + refused));

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    std::puts(g_failures ? "PERSISTENCE-STATS FAIL" : "PERSISTENCE-STATS OK");
    return g_failures ? 1 : 0;
}
