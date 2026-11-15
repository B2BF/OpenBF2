// [TEMP] [CHOSEN, not recovered] Test fixture for the online services' domain
// (Code/BF2/Game/MatchMaking/OnlineProvider.h): the names formed for the retail domain and for a
// +gamespy value, and that the linked GameSpy SDK and the persistence classes look those
// names up. Built on request (bf2_online_provider_check); POSIX only.
//
// The process never asks a real resolver and never sends beyond loopback: gethostbyname and
// getaddrinfo below replace the C library's for every object linked into this executable, record
// each name asked for, and answer "not found" or 127.0.0.1. The services' ports are fixed by the
// SDK (27900, 27901, 28910, 29900, 29901, 29910, 29920); a listener that cannot bind its port is
// reported as skipped and the name checks still run. The web requests go to port 80, which an
// unprivileged process cannot bind on loopback: connect below sends them to 127.0.0.1:18080.
//
// The "silent-tcp" scenario checks the local connect timeouts (GameSpy/LOCAL_CHANGES.md) against
// loopback listeners whose backlog is full, so connects to them are never answered.
//
// Each scenario runs in its own child process, because the SDK keeps its state in statics.
#include "../BF2/Game/MatchMaking/OnlineProvider.h"
#include "../BF2/Game/Persistence/HTTPRequest.h"
#include "../BF2/Game/Persistence/StatsManager.h"
#include "../BF2/Game/Persistence/PlayerStatsManager.h"
#include "../BF2/Game/GameServer/ServerSettings.h"
#include "../BF2/Game/IGame.h"

extern "C" {
#include "../BF2/External/GameSpy/common/gsCommon.h"
#include "../BF2/External/GameSpy/common/gsAvailable.h"
#include "../BF2/External/GameSpy/qr2/qr2.h"
#include "../BF2/External/GameSpy/gcdkey/gcdkeys.h"
#include "../BF2/External/GameSpy/gstats/gstats.h"
#include "../BF2/External/GameSpy/natneg/natneg.h"
#include "../BF2/External/GameSpy/gp/gp.h"
#include "../BF2/External/GameSpy/ghttp/ghttp.h"
#include "../BF2/External/GameSpy/serverbrowsing/sb_serverbrowsing.h"
extern char* Matchup3Hostname;
extern char* SBOverrideMasterServer;
}

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

// Scaffolding for StatsManager.cpp, which reads these engine globals. Only connect() runs here.
namespace dice { namespace hfe {
ServerSettings* serverSettings = 0;
IGame* game = 0;
namespace gamespy {
extern const char* const g_gameName;
const char* const g_gameName = "battlefield2";
const char* getSecretKey() { return "hW6m9a"; }
}
} }

namespace {

using namespace dice::hfe;

// ---- the process's resolver -------------------------------------------------------------------
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
std::vector<std::string> g_lookups;      // every name asked for, in order
bool g_resolve = false;                  // false: not found; true: 127.0.0.1

void recordLookup(const char* name)
{
    char local[256] = "";
    gethostname(local, sizeof(local) - 1);
    const std::string text(name ? name : "");
    pthread_mutex_lock(&g_lock);
    g_lookups.push_back(text == local ? std::string("<this machine>") : text);
    pthread_mutex_unlock(&g_lock);
}

std::vector<std::string> takeLookups()
{
    pthread_mutex_lock(&g_lock);
    std::vector<std::string> names;
    names.swap(g_lookups);
    pthread_mutex_unlock(&g_lock);
    return names;
}

} // namespace

extern "C" struct hostent* gethostbyname(const char* name)
{
    recordLookup(name);
    if (!g_resolve)
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
    std::strncpy(canonical, name ? name : "", sizeof(canonical) - 1);
    entry.h_name = canonical;
    entry.h_aliases = noAliases;
    entry.h_addrtype = AF_INET;
    entry.h_length = 4;
    entry.h_addr_list = addresses;
    return &entry;
}

extern "C" int getaddrinfo(const char* node, const char*, const struct addrinfo*, struct addrinfo** result)
{
    recordLookup(node);
    if (!g_resolve)
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

// Web requests to 127.0.0.1:80 reach the fixture's listener on 18080; nothing else is changed.
extern "C" int connect(int fd, const struct sockaddr* address, socklen_t length)
{
    typedef int (*Connect)(int, const struct sockaddr*, socklen_t);
    static Connect real = reinterpret_cast<Connect>(dlsym(RTLD_NEXT, "connect"));
    struct sockaddr_in changed;
    if (address && address->sa_family == AF_INET && length >= sizeof(changed))
    {
        std::memcpy(&changed, address, sizeof(changed));
        if (changed.sin_port == htons(80) && changed.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
        {
            changed.sin_port = htons(18080);
            return real(fd, reinterpret_cast<struct sockaddr*>(&changed), sizeof(changed));
        }
    }
    return real(fd, address, length);
}

namespace {

// ---- checks -----------------------------------------------------------------------------------
int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  %s %s\n", ok ? "ok  " : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

void checkEqual(const std::string& actual, const std::string& expected, const std::string& what)
{
    check(actual == expected, what + " = " + actual + (actual == expected ? "" : " (expected " + expected + ")"));
}

double now()
{
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

std::string join(const std::vector<std::string>& names)
{
    std::string text;
    for (std::size_t i = 0; i < names.size(); ++i)
        text += (i ? "," : "") + names[i];
    return text;
}

// The names looked up since the last call, without this machine's own name (qr2 lists local addresses).
std::string lookedUp()
{
    std::vector<std::string> names = takeLookups();
    std::vector<std::string> kept;
    for (std::size_t i = 0; i < names.size(); ++i)
        if (names[i] != "<this machine>" && (kept.empty() || kept.back() != names[i]))
            kept.push_back(names[i]);
    return join(kept);
}

// ---- loopback listeners -----------------------------------------------------------------------
struct Listener
{
    const char* name;
    int port;
    bool udp;
    int fd;
    int events;                 // datagrams or connections seen
    std::string first;          // first bytes received
};

Listener g_listeners[] = {
    { "available+heartbeat", 27900, true,  -1, 0, "" },
    { "natneg",              27901, true,  -1, 0, "" },
    { "cdkey",               29910, true,  -1, 0, "" },
    { "serverlist",          28910, false, -1, 0, "" },
    { "gpcm",                29900, false, -1, 0, "" },
    { "gpsp",                29901, false, -1, 0, "" },
    { "gamestats",           29920, false, -1, 0, "" },
    { "web",                 18080, false, -1, 0, "" },   // stands in for port 80, see connect
};
const int kListeners = sizeof(g_listeners) / sizeof(g_listeners[0]);
volatile bool g_stopListeners = false;
bool g_answerAvailable = true;
pthread_t g_listenerThread;

Listener& listener(const char* name)
{
    for (int i = 0; i < kListeners; ++i)
        if (!std::strcmp(g_listeners[i].name, name))
            return g_listeners[i];
    std::abort();
}

void openListeners()
{
    for (int i = 0; i < kListeners; ++i)
    {
        Listener& l = g_listeners[i];
        int fd = socket(AF_INET, l.udp ? SOCK_DGRAM : SOCK_STREAM, 0);
        int on = 1;
        if (fd >= 0 && !l.udp)   // the previous scenario's connections may still be closing
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
        struct sockaddr_in address;
        std::memset(&address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<unsigned short>(l.port));
        if (fd < 0 || bind(fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0
            || (!l.udp && listen(fd, 4) != 0))
        {
            std::printf("  skip listener %s: 127.0.0.1:%d not available (%s)\n", l.name, l.port, std::strerror(errno));
            if (fd >= 0)
                close(fd);
            continue;
        }
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
        l.fd = fd;
    }
}

void* serveListeners(void*)
{
    while (!g_stopListeners)
    {
        fd_set readable;
        FD_ZERO(&readable);
        int highest = -1;
        for (int i = 0; i < kListeners; ++i)
            if (g_listeners[i].fd >= 0)
            {
                FD_SET(g_listeners[i].fd, &readable);
                if (g_listeners[i].fd > highest)
                    highest = g_listeners[i].fd;
            }
        struct timeval wait = { 0, 20000 };
        if (highest < 0 || select(highest + 1, &readable, 0, 0, &wait) <= 0)
        {
            if (highest < 0)
                usleep(20000);
            continue;
        }
        for (int i = 0; i < kListeners; ++i)
        {
            Listener& l = g_listeners[i];
            if (l.fd < 0 || !FD_ISSET(l.fd, &readable))
                continue;
            char data[2048];
            if (l.udp)
            {
                struct sockaddr_in from;
                socklen_t fromLength = sizeof(from);
                ssize_t n = recvfrom(l.fd, data, sizeof(data), 0, reinterpret_cast<struct sockaddr*>(&from), &fromLength);
                if (n <= 0)
                    continue;
                pthread_mutex_lock(&g_lock);
                if (!l.events++)
                    l.first.assign(data, static_cast<std::size_t>(n));
                pthread_mutex_unlock(&g_lock);
                // The availability request is 09 00 00 00 00 <game>; answer "all services available".
                if (l.port == 27900 && data[0] == 0x09 && g_answerAvailable)
                    sendto(l.fd, "\xFE\xFD\x09\x00\x00\x00\x00", 7, 0, reinterpret_cast<struct sockaddr*>(&from), fromLength);
            }
            else
            {
                int peer = accept(l.fd, 0, 0);
                if (peer < 0)
                    continue;
                fcntl(peer, F_SETFL, fcntl(peer, F_GETFL, 0) & ~O_NONBLOCK);   // inherited from the listener
                std::string first;
                if (l.port == 29920)
                {
                    // The statistics handshake: a challenge, the game's answer, a session key. All
                    // but the terminator is XORed with "GameSpy3D".
                    struct timeval limit = { 1, 0 };
                    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
                    const char* steps[2] = { "\\lc\\1\\challenge\\ABCDEFGHIJ\\id\\1", "\\lc\\2\\sesskey\\1234567\\proof\\0\\id\\1" };
                    for (int step = 0; step < 2; ++step)
                    {
                        std::string message(steps[step]);
                        for (std::size_t c = 0; c < message.size(); ++c)
                            message[c] ^= "GameSpy3D"[c % 9];
                        message += "\\final\\";
                        send(peer, message.data(), message.size(), 0);
                        ssize_t n = recv(peer, data, sizeof(data), 0);   // the answer, then the close
                        if (step == 0 && n > 0)
                            first.assign(data, static_cast<std::size_t>(n));
                    }
                }
                else if (l.port == 18080)
                {
                    // Read the request head, then answer an empty body.
                    struct timeval limit = { 1, 0 };
                    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
                    ssize_t n;
                    while (first.find("\r\n\r\n") == std::string::npos && (n = recv(peer, data, sizeof(data), 0)) > 0)
                        first.append(data, static_cast<std::size_t>(n));
                    const char reply[] = "HTTP/1.0 200 OK\r\nContent-Length: 1\r\n\r\nO";
                    send(peer, reply, sizeof(reply) - 1, 0);
                }
                close(peer);    // every other service: the connection itself is the evidence
                pthread_mutex_lock(&g_lock);
                if (!l.events++ || (l.port == 18080 && !first.empty()))
                    l.first += first;
                pthread_mutex_unlock(&g_lock);
            }
        }
    }
    return 0;
}

void startListeners()
{
    openListeners();
    pthread_create(&g_listenerThread, 0, &serveListeners, 0);
}

void stopListeners()
{
    g_stopListeners = true;
    pthread_join(g_listenerThread, 0);
}

// Waits up to seconds for the listener to have seen something. A listener that could not bind
// reports "skipped" and counts as passed.
void checkArrived(const char* name, const std::string& what, double seconds = 2.0)
{
    Listener& l = listener(name);
    if (l.fd < 0)
    {
        std::printf("  skip %s (no listener on 127.0.0.1:%d)\n", what.c_str(), l.port);
        return;
    }
    const double end = now() + seconds;
    int events = 0;
    while (now() < end)
    {
        pthread_mutex_lock(&g_lock);
        events = l.events;
        pthread_mutex_unlock(&g_lock);
        if (events)
            break;
        usleep(10000);
    }
    char port[16];
    std::snprintf(port, sizeof(port), "%d", l.port);
    check(events > 0, what + " arrived at 127.0.0.1:" + port);
}

std::string firstBytes(const char* name)
{
    pthread_mutex_lock(&g_lock);
    const std::string data = listener(name).first;
    pthread_mutex_unlock(&g_lock);
    return data;
}

// ---- the SDK entry points, called as the game calls them ---------------------------------------
GSIACResult availabilityCheck()
{
    GSIStartAvailableCheckA("battlefield2");          // GameSpyBackend, ProfileManager, ServerBrowser
    GSIACResult result;
    while ((result = GSIAvailableCheckThink()) == GSIACWaiting)
        msleep(5);
    return result;
}

void serverKey(int, qr2_buffer_t buffer, void*) { qr2_buffer_addA(buffer, ""); }
void playerTeamKey(int, int, qr2_buffer_t buffer, void*) { qr2_buffer_addA(buffer, ""); }
void keyList(qr2_key_type type, qr2_keybuffer_t buffer, void*) { if (type == key_server) qr2_keybuffer_add(buffer, HOSTNAME_KEY); }
int keyCount(qr2_key_type, void*) { return 0; }
void addError(qr2_error_t, gsi_char*, void*) {}

qr2_error_t startQr2(qr2_t* record)
{
    // GameSpyBackend: an internet server (ispublic). 29931 keeps clear of the loopback harness's 29911.
    return qr2_initA(record, 0, 29931, "battlefield2", "hW6m9a", 1, 0,
                     &serverKey, &playerTeamKey, &playerTeamKey, &keyList, &keyCount, &addError, 0);
}

void browserCallback(ServerBrowser, SBCallbackReason, SBServer, void*) {}

SBError startServerList(ServerBrowser* browser)
{
    // Menu/ServerBrowser.cpp: init(internet) then startUpdate(async).
    *browser = ServerBrowserNew("battlefield2", "battlefield2", "hW6m9a", 0, 40, 1, SBFalse, &browserCallback, 0);
    unsigned char fields[2] = { HOSTNAME_KEY, MAPNAME_KEY };
    return ServerBrowserUpdate(*browser, SBTrue, SBTrue, fields, 2, "");
}

void gpIgnore(GPConnection*, void*, void*) {}
void authDone(int, int, int, char*, void*) {}
void authRefresh(int, int, int, char*, void*) {}
void negotiateProgress(NegotiateState, void*) {}
void negotiateCompleted(NegotiateResult, SOCKET, struct sockaddr_in*, void*) {}

std::string gpError(GPConnection* connection)
{
    char text[GP_ERROR_STRING_LEN] = "";
    gpGetErrorStringA(connection, text);
    return text;
}

bool statsManagerConnect()
{
    // StatsManager::connect reads serverSettings->m_gameSpyPort; nothing else of the object is used.
    serverSettings = static_cast<ServerSettings*>(std::calloc(1, sizeof(ServerSettings)));
    StatsManager manager;
    return manager.connect();
}

struct WebRequest : HTTPRequest
{
    bool failed;
    std::string body;
    WebRequest() : failed(false) {}
    void onGetComplete(const std::string& text) { body = text; HTTPRequest::onGetComplete(text); }
    void onGetFailure(const std::string& text) { failed = true; HTTPRequest::onGetFailure(text); }
};

void runPlayerStats()
{
    PlayerStatsManager manager;
    manager.connect();
    manager.update();
    const double end = now() + 3.0;
    while (manager.isUpdating() && now() < end)
    {
        ghttpThink();
        msleep(5);
    }
}

// ---- scenarios --------------------------------------------------------------------------------

// The names formed, the option's validation, and the SDK variables written.
void scenarioNames()
{
    using namespace onlineProvider;
    std::puts("names: retail domain");
    checkEqual(domain(), "gamespy.com", "domain");
    check(!isOverridden(), "not overridden");
    checkEqual(availableHost(), "battlefield2.available.gamespy.com", "available");
    checkEqual(masterHost(), "battlefield2.master.gamespy.com", "master");
    checkEqual(serverListHost(), "battlefield2.ms14.gamespy.com", "server list");
    checkEqual(natNegHost(1), "battlefield2.natneg1.gamespy.com", "natneg1");
    checkEqual(natNegHost(2), "battlefield2.natneg2.gamespy.com", "natneg2");
    checkEqual(natNegHost(3), "battlefield2.natneg3.gamespy.com", "natneg3");
    checkEqual(connectionManagerHost(), "gpcm.gamespy.com", "gpcm");
    checkEqual(searchManagerHost(), "gpsp.gamespy.com", "gpsp");
    checkEqual(cdKeyHost(), "key.gamespy.com", "key");
    checkEqual(gameStatsServerName(), "gamestats.gamespy.com", "StatsServerHostname value");
    checkEqual(gameStatsHost(), "battlefield2.gamestats.gamespy.com", "gamestats");
    checkEqual(statsWebUrl(), "http://BF2Web.gamespy.com/ASP/", "stats web");
    checkEqual(playerInfoUrl(), "http://stage-net.gamespy.com/bf2/getplayerinfo.aspx?pid=", "player info");
    checkEqual(commandLineArguments(), "", "restart arguments");
    std::printf("  %s\n", describe(true).c_str());

    std::puts("names: the SDK's variables are untouched for the retail domain");
    prepareGameSpySdk();
    check(GSIACHostname[0] == 0 && qr2_hostname[0] == 0 && gcd_hostname[0] == 0, "available, master and key overrides empty");
    checkEqual(StatsServerHostname, "gamestats.gamespy.com", "SDK StatsServerHostname");
    checkEqual(GPConnectionManagerHostname, "gpcm.gamespy.com", "SDK GPConnectionManagerHostname");
    checkEqual(GPSearchManagerHostname, "gpsp.gamespy.com", "SDK GPSearchManagerHostname");
    check(!SBOverrideMasterServer && !Matchup1Hostname && !Matchup2Hostname && !Matchup3Hostname, "server list and natneg overrides null");

    std::puts("names: values +gamespy refuses");
    const std::string tooLong = std::string(36, 'a') + ".test";   // 41 characters
    const char* refused[] = { "", "1.2.3.4", "bad host", "a..b", ".example.test", "example.test.", "exa_mple.test",
                              "http://example.test", "example.test/ASP", "example.test:80", tooLong.c_str() };
    for (unsigned i = 0; i < sizeof(refused) / sizeof(refused[0]); ++i)
    {
        std::string error;
        const bool accepted = setDomain(refused[i], error);
        check(!accepted && !error.empty() && domain() == "gamespy.com" && !isOverridden(),
              std::string("refused \"") + refused[i] + "\": " + error);
    }

    std::puts("names: +gamespy gamespy.com is the retail domain");
    std::string error;
    check(setDomain("gamespy.com", error) && !isOverridden(), "accepted, not an override");

    std::puts("names: the longest domain fits the SDK's 64-byte variables");
    const std::string longest = std::string(35, 'a') + ".test";   // 40 characters
    check(setDomain(longest, error) && availableHost().size() == 63 && gameStatsHost().size() == 63, "40 characters accepted, 63-character names");

    std::puts("names: +gamespy example.test");
    check(setDomain("example.test", error) && isOverridden(), "accepted");
    checkEqual(commandLineArguments(), " +gamespy example.test", "restart arguments");
    prepareGameSpySdk();
    checkEqual(GSIACHostname, "battlefield2.available.example.test", "SDK GSIACHostname");
    checkEqual(qr2_hostname, "battlefield2.master.example.test", "SDK qr2_hostname");
    checkEqual(gcd_hostname, "key.example.test", "SDK gcd_hostname");
    checkEqual(StatsServerHostname, "gamestats.example.test", "SDK StatsServerHostname");
    checkEqual(GPConnectionManagerHostname, "gpcm.example.test", "SDK GPConnectionManagerHostname");
    checkEqual(GPSearchManagerHostname, "gpsp.example.test", "SDK GPSearchManagerHostname");
    checkEqual(SBOverrideMasterServer ? SBOverrideMasterServer : "(null)", "battlefield2.ms14.example.test", "SDK SBOverrideMasterServer");
    checkEqual(Matchup1Hostname ? Matchup1Hostname : "(null)", "battlefield2.natneg1.example.test", "SDK Matchup1Hostname");
    checkEqual(Matchup2Hostname ? Matchup2Hostname : "(null)", "battlefield2.natneg2.example.test", "SDK Matchup2Hostname");
    checkEqual(Matchup3Hostname ? Matchup3Hostname : "(null)", "battlefield2.natneg3.example.test", "SDK Matchup3Hostname");
    checkEqual(statsWebUrl(), "http://BF2Web.example.test/ASP/", "stats web");
    checkEqual(playerInfoUrl(), "http://stage-net.example.test/bf2/getplayerinfo.aspx?pid=", "player info");
    std::printf("  %s\n", describe(true).c_str());
}

// Every service, with the names the given domain forms. With resolve false no name resolves (the
// state of a domain without these hosts) and each service must give up at once; with resolve true
// every name is 127.0.0.1 and the first packet or connection of each service must arrive there.
void scenarioServices(const char* domainName, bool resolve)
{
    using namespace onlineProvider;
    const std::string d(domainName);
    std::string error;
    if (d != domain())
        check(setDomain(d, error), "+gamespy " + d);
    prepareGameSpySdk();
    g_resolve = resolve;
    if (resolve)
        startListeners();
    const double started = now();
    double mark = now();
    #define STEP_TIME(what) do { if (!resolve) { const double t = now() - mark; char s[64]; std::snprintf(s, sizeof(s), "%.2f s", t); \
                                 check(t < 1.0, std::string(what) + " gave up in " + s); } mark = now(); } while (0)

    std::puts("availability check");
    check(availabilityCheck() == GSIACAvailable, "result is available");
    checkEqual(lookedUp(), "battlefield2.available." + d, "looked up");
    if (resolve)
    {
        checkArrived("available+heartbeat", "the availability request");
        check(firstBytes("available+heartbeat").size() > 5 && firstBytes("available+heartbeat")[0] == 0x09
              && firstBytes("available+heartbeat").compare(5, 12, "battlefield2") == 0, "it names battlefield2");
    }
    STEP_TIME("availability check");

    std::puts("master: heartbeats (qr2) and CD-key validation (gcd_init_qr2)");
    qr2_t record = 0;
    const qr2_error_t qr2Result = startQr2(&record);
    checkEqual(lookedUp(), "battlefield2.master." + d, "looked up");
    if (!resolve)
        check(qr2Result == e_qrdnserror, "qr2_init reports the lookup failure");
    else
    {
        check(qr2Result == e_qrnoerror, "qr2_init succeeded");
        pthread_mutex_lock(&g_lock);
        listener("available+heartbeat").events = 0;
        listener("available+heartbeat").first.clear();
        pthread_mutex_unlock(&g_lock);
        qr2_think(record);
        checkArrived("available+heartbeat", "the heartbeat");
        check(!firstBytes("available+heartbeat").empty() && firstBytes("available+heartbeat")[0] == 0x03, "it is a heartbeat packet");
        check(gcd_init_qr2(record, 1059) == 0, "gcd_init_qr2 succeeded");
        gcd_authenticate_user(1059, 1, inet_addr("127.0.0.1"), "challeng",
                              "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef01234567",
                              &authDone, &authRefresh, 0);
        checkArrived("cdkey", "the key validation request");
        std::string request = firstBytes("cdkey");
        for (std::size_t i = 0; i < request.size(); ++i)
            request[i] ^= "gamespy"[i % 7];
        check(request.compare(0, 6, "\\auth\\") == 0, "it is an \\auth\\ request");
        gcd_disconnect_user(1059, 1);
        gcd_shutdown();
        qr2_shutdown(record);
    }
    takeLookups();
    STEP_TIME("server registration");

    std::puts("server list");
    ServerBrowser browser = 0;
    const SBError listResult = startServerList(&browser);
    checkEqual(lookedUp(), "battlefield2.ms14." + d, "looked up");
    if (!resolve)
        check(listResult == sbe_dnserror, "the update reports the lookup failure");
    else
        checkArrived("serverlist", "the list connection");
    ServerBrowserFree(browser);
    STEP_TIME("server list");

    std::puts("presence: login (gpcm) and account search (gpsp)");
    GPConnection connection;                     // ProfileManager: product 10493, namespace 12
    check(gpInitialize(&connection, 10493, 12, GP_PARTNERID_GAMESPY) == GP_NO_ERROR, "gpInitialize");
    gpDisable(&connection, GP_INFO_CACHING);     // no gp.info in the working directory
    gpSetCallback(&connection, GP_ERROR, &gpIgnore, 0);
    const GPResult loginResult = gpConnectUniqueNickA(&connection, "nobody", "nothing", GP_NO_FIREWALL,
                                                      resolve ? GP_NON_BLOCKING : GP_BLOCKING, &gpIgnore, 0);
    checkEqual(lookedUp(), "gpcm." + d, "login looked up");
    if (!resolve)
        checkEqual(gpError(&connection), "Could not resolve connection manager host name.", "login error");
    else
    {
        check(loginResult == GP_NO_ERROR, "login started");
        checkArrived("gpcm", "the login connection");
    }
    gpDestroy(&connection);
    gpInitialize(&connection, 10493, 12, GP_PARTNERID_GAMESPY);
    gpDisable(&connection, GP_INFO_CACHING);
    gpSetCallback(&connection, GP_ERROR, &gpIgnore, 0);
    const GPResult searchResult = gpGetUserNicksA(&connection, "nobody@example.invalid", "nothing",
                                                  resolve ? GP_NON_BLOCKING : GP_BLOCKING, &gpIgnore, 0);
    checkEqual(lookedUp(), "gpsp." + d, "search looked up");
    if (!resolve)
        checkEqual(gpError(&connection), "Could not resolve search manager host name.", "search error");
    else
    {
        check(searchResult == GP_NO_ERROR, "search started");
        checkArrived("gpsp", "the search connection");
    }
    gpDestroy(&connection);
    takeLookups();
    STEP_TIME("presence");

    std::puts("game statistics (StatsManager::connect)");
    const bool statsConnected = statsManagerConnect();
    checkEqual(lookedUp(), "battlefield2.gamestats." + d, "looked up");
    if (!resolve)
        check(!statsConnected, "connect() reports failure");
    else
    {
        checkArrived("gamestats", "the statistics connection");
        if (listener("gamestats").fd >= 0)
            check(statsConnected, "connect() completed the handshake with the listener");
    }
    STEP_TIME("game statistics");

    std::puts("NAT negotiation");
    const NegotiateError negotiate = NNBeginNegotiation(1234, 1, &negotiateProgress, &negotiateCompleted, 0);
    checkEqual(lookedUp(), "battlefield2.natneg1." + d + ",battlefield2.natneg2." + d + ",battlefield2.natneg3." + d, "looked up");
    if (!resolve)
        check(negotiate == ne_dnserror, "negotiation reports the lookup failure");
    else
    {
        check(negotiate == ne_noerror, "negotiation started");
        checkArrived("natneg", "the negotiation request");
        NNCancel(1234);
        NNFreeNegotiateList();
    }
    STEP_TIME("NAT negotiation");

    std::puts("stats web requests (HTTPRequest::getGameSpyUrl)");
    {
        WebRequest request;
        const std::string url = request.getGameSpyUrl() + "getbackendinfo.aspx";
        checkEqual(url, "http://BF2Web." + d + "/ASP/getbackendinfo.aspx", "url");
        request.get(url, false);
        const double end = now() + 3.0;
        while (request.isUpdating() && now() < end)
        {
            request.update();
            msleep(5);
        }
        checkEqual(lookedUp(), "BF2Web." + d, "looked up");
        if (!resolve)
            check(request.failed, "the request failed");
        else if (listener("web").fd >= 0)
        {
            checkArrived("web", "the request");
            const std::string head = firstBytes("web");
            check(head.compare(0, 29, "GET /ASP/getbackendinfo.aspx ") == 0, "request line is GET /ASP/getbackendinfo.aspx");
            check(head.find("Host: BF2Web." + d) != std::string::npos, "Host header is BF2Web." + d);
        }
        else
            check(request.failed, "the request failed (no listener)");
    }
    STEP_TIME("stats web request");

    std::puts("player info web request (PlayerStatsManager::update)");
    pthread_mutex_lock(&g_lock);
    listener("web").events = 0;
    listener("web").first.clear();
    pthread_mutex_unlock(&g_lock);
    runPlayerStats();
    checkEqual(lookedUp(), "stage-net." + d, "looked up");
    if (resolve && listener("web").fd >= 0)
    {
        checkArrived("web", "the request");
        check(firstBytes("web").compare(0, 34, "GET /bf2/getplayerinfo.aspx?pid=&g") == 0, "request line is GET /bf2/getplayerinfo.aspx?pid=...");
        check(firstBytes("web").find("Host: stage-net." + d) != std::string::npos, "Host header is stage-net." + d);
    }
    STEP_TIME("player info web request");
    #undef STEP_TIME

    if (resolve)
        stopListeners();
    std::printf("  all services: %.2f s\n", now() - started);
}

// A host that resolves but never answers: the SDK waits for two 2-second tries, once. Retail
// waits again for every later check (eight server lists at client start-up, each login, each
// server start); the local change in gsAvailable.c ends those at once.
void scenarioSilentHost()
{
    std::puts("availability check against a host that resolves and does not answer");
    g_resolve = true;
    g_answerAvailable = false;
    startListeners();
    if (listener("available+heartbeat").fd < 0)
    {
        // Something else owns the port and may answer; the timing below would mean nothing.
        std::puts("  skip (127.0.0.1:27900 is in use)");
        stopListeners();
        return;
    }
    double mark = now();
    check(availabilityCheck() == GSIACAvailable, "result is available (the SDK fails open)");
    const double first = now() - mark;
    char text[64];
    std::snprintf(text, sizeof(text), "%.2f s", first);
    check(first > 3.5 && first < 6.0, std::string("the first check waits for both tries: ") + text);
    checkEqual(lookedUp(), "battlefield2.available.gamespy.com", "looked up");
    mark = now();
    bool available = true;
    for (int i = 0; i < 8; ++i)
        available = available && availabilityCheck() == GSIACAvailable;
    const double later = now() - mark;
    std::snprintf(text, sizeof(text), "%.2f s", later);
    check(available && later < 0.5, std::string("eight more checks: ") + text);
    checkEqual(lookedUp(), "", "no further lookup");
    stopListeners();
}

// Every name resolves to this machine and nothing listens: a provider that is down. Each TCP
// service must report the refused connection (a default SIGPIPE disposition would end the process
// at the first send) and give up quickly.
void scenarioRefused()
{
    using namespace onlineProvider;
    std::string error;
    check(setDomain("example.test", error), "+gamespy example.test");
    prepareGameSpySdk();
    g_resolve = true;
    char text[64];
    double mark = now();
    #define STEP_TIME(what, limit) do { const double t = now() - mark; std::snprintf(text, sizeof(text), "%.2f s", t); \
                                        check(t < limit, std::string(what) + " gave up in " + text); mark = now(); } while (0)

    check(availabilityCheck() == GSIACAvailable, "availability check: available (the SDK fails open)");
    STEP_TIME("availability check (two 2-second tries)", 6.0);

    ServerBrowser browser = 0;
    check(startServerList(&browser) == sbe_connecterror, "server list: the update reports the refused connection");
    ServerBrowserFree(browser);
    STEP_TIME("server list", 2.0);

    GPConnection connection;
    gpInitialize(&connection, 10493, 12, GP_PARTNERID_GAMESPY);
    gpDisable(&connection, GP_INFO_CACHING);
    gpSetCallback(&connection, GP_ERROR, &gpIgnore, 0);
    check(gpConnectUniqueNickA(&connection, "nobody", "nothing", GP_NO_FIREWALL, GP_BLOCKING, &gpIgnore, 0) != GP_NO_ERROR,
          "login failed: " + gpError(&connection));
    STEP_TIME("login", 2.0);
    // The search reports its failure through the callback argument and the error text.
    gpGetUserNicksA(&connection, "nobody@example.invalid", "nothing", GP_BLOCKING, &gpIgnore, 0);
    check(!gpError(&connection).empty(), "account search failed: " + gpError(&connection));
    gpDestroy(&connection);
    STEP_TIME("account search", 2.0);

    check(!statsManagerConnect(), "game statistics: connect() reports failure");
    STEP_TIME("game statistics", 2.0);

    {
        WebRequest request;
        request.get(request.getGameSpyUrl() + "getbackendinfo.aspx", false);
        check(request.failed && !request.isUpdating(), "stats web request failed");
    }
    STEP_TIME("stats web request", 2.0);
    runPlayerStats();
    STEP_TIME("player info web request", 2.0);
    #undef STEP_TIME
    takeLookups();
}

// Every name resolves to this machine and every TCP service port has a listener that never
// accepts: its one-entry backlog is filled first, so further connects get no answer, as from an
// address that drops them. The local connect timeouts (LOCAL_CHANGES.md: 5 seconds for the server
// list, the presence managers and web requests) must end each operation; the operating system's
// own TCP timeout is 75 s on macOS.
struct SilentPort
{
    const char* name;
    int port;
    int listener;
    int filler;
};

bool openSilent(SilentPort& p)
{
    p.listener = socket(AF_INET, SOCK_STREAM, 0);
    p.filler = -1;
    int on = 1;
    setsockopt(p.listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(p.port));
    if (bind(p.listener, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0 || listen(p.listener, 1) != 0)
    {
        close(p.listener);
        p.listener = -1;
        return false;
    }
    // Fill the backlog; a second pending connect must then stay unanswered.
    p.filler = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(p.filler, F_SETFL, fcntl(p.filler, F_GETFL, 0) | O_NONBLOCK);
    connect(p.filler, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    usleep(50000);
    int probe = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(probe, F_SETFL, fcntl(probe, F_GETFL, 0) | O_NONBLOCK);
    connect(probe, reinterpret_cast<struct sockaddr*>(&address), sizeof(address));
    fd_set writable;
    FD_ZERO(&writable);
    FD_SET(probe, &writable);
    struct timeval wait = { 0, 300000 };
    const bool silent = select(probe + 1, 0, &writable, 0, &wait) == 0;
    close(probe);
    return silent;
}

void scenarioSilentTcp()
{
    using namespace onlineProvider;
    std::string error;
    check(setDomain("example.test", error), "+gamespy example.test");
    prepareGameSpySdk();
    g_resolve = true;
    SilentPort ports[] = { { "server list", 28910, -1, -1 }, { "gpcm", 29900, -1, -1 },
                           { "gpsp", 29901, -1, -1 }, { "web", 18080, -1, -1 } };
    bool ready = true;
    for (unsigned i = 0; i < sizeof(ports) / sizeof(ports[0]); ++i)
        if (!openSilent(ports[i]))
        {
            std::printf("  skip: 127.0.0.1:%d could not be made silent\n", ports[i].port);
            ready = false;
        }
    if (!ready)
    {
        for (unsigned i = 0; i < sizeof(ports) / sizeof(ports[0]); ++i)
        {
            if (ports[i].filler >= 0) close(ports[i].filler);
            if (ports[i].listener >= 0) close(ports[i].listener);
        }
        return;
    }
    // The SDK's services require a completed availability check (no answer: available after 4 s).
    check(availabilityCheck() == GSIACAvailable, "availability check: available");
    char text[64];
    double mark = now();
    #define STEP_TIME(what) do { const double t = now() - mark; std::snprintf(text, sizeof(text), "%.2f s", t); \
                                 check(t > 4.5 && t < 7.0, std::string(what) + " gave up after the 5 s limit: " + text); mark = now(); } while (0)

    ServerBrowser browser = 0;
    check(startServerList(&browser) == sbe_connecterror, "server list: the update reports a connect error");
    ServerBrowserFree(browser);
    STEP_TIME("server list");

    GPConnection connection;
    gpInitialize(&connection, 10493, 12, GP_PARTNERID_GAMESPY);
    gpDisable(&connection, GP_INFO_CACHING);
    gpSetCallback(&connection, GP_ERROR, &gpIgnore, 0);
    check(gpConnectUniqueNickA(&connection, "nobody", "nothing", GP_NO_FIREWALL, GP_BLOCKING, &gpIgnore, 0) != GP_NO_ERROR,
          "login failed: " + gpError(&connection));
    STEP_TIME("login");
    gpDestroy(&connection);
    gpInitialize(&connection, 10493, 12, GP_PARTNERID_GAMESPY);
    gpDisable(&connection, GP_INFO_CACHING);
    gpSetCallback(&connection, GP_ERROR, &gpIgnore, 0);
    gpGetUserNicksA(&connection, "nobody@example.invalid", "nothing", GP_BLOCKING, &gpIgnore, 0);
    check(!gpError(&connection).empty(), "account search failed: " + gpError(&connection));
    gpDestroy(&connection);
    STEP_TIME("account search");

    {
        WebRequest request;                       // blocking, as most persistence requests are
        request.get(request.getGameSpyUrl() + "getbackendinfo.aspx", false);
        check(request.failed && !request.isUpdating(), "stats web request failed");
    }
    STEP_TIME("stats web request (blocking)");
    {
        WebRequest request;                       // asynchronous, pumped like the menu does
        request.get(request.getGameSpyUrl() + "getleaderboard.aspx", true);
        const double end = now() + 10.0;
        while (request.isUpdating() && now() < end)
        {
            request.update();
            msleep(5);
        }
        check(request.failed, "asynchronous stats web request failed");
    }
    STEP_TIME("stats web request (asynchronous)");
    #undef STEP_TIME
    takeLookups();
    for (unsigned i = 0; i < sizeof(ports) / sizeof(ports[0]); ++i)
    {
        close(ports[i].filler);
        close(ports[i].listener);
    }
}

int runScenario(const std::string& name)
{
    if (name == "names")
        scenarioNames();
    else if (name == "retail-domain")
        scenarioServices("gamespy.com", false);
    else if (name == "other-domain")
        scenarioServices("example.test", true);
    else if (name == "other-domain-unresolved")
        scenarioServices("example.test", false);
    else if (name == "silent-host")
        scenarioSilentHost();
    else if (name == "refused")
        scenarioRefused();
    else if (name == "silent-tcp")
        scenarioSilentTcp();
    else
    {
        std::fprintf(stderr, "unknown scenario %s\n", name.c_str());
        return 2;
    }
    std::fflush(stdout);
    return g_failures ? 1 : 0;
}

} // namespace

int main(int argc, char** argv)
{
    setvbuf(stdout, 0, _IOLBF, 0);
    if (argc > 1)
        return runScenario(argv[1]);

    const char* scenarios[] = { "names", "retail-domain", "other-domain", "other-domain-unresolved", "silent-host", "refused", "silent-tcp" };
    int failed = 0;
    for (unsigned i = 0; i < sizeof(scenarios) / sizeof(scenarios[0]); ++i)
    {
        std::printf("== %s\n", scenarios[i]);
        std::fflush(stdout);
        const pid_t child = fork();
        if (child == 0)
            _exit(runScenario(scenarios[i]));
        int status = 0;
        waitpid(child, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            std::printf("== %s FAILED\n", scenarios[i]);
            ++failed;
        }
    }
    std::printf("%s\n", failed ? "ONLINE-PROVIDER FAILED" : "ONLINE-PROVIDER OK");
    return failed ? 1 : 0;
}
