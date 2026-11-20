// [TEMP] [CHOSEN, not recovered] Test fixture for the redirects of the online service names
// (Code/BF2/Game/MatchMaking/OnlineProvider.h): the retail names, +gamespyRedirect with an address
// or a domain, the +gamespyRedirect<label> options, and that the linked GameSpy SDK and the
// persistence classes look the redirected names up while URLs and Host headers stay retail.
// Built on request (bf2_online_provider_check); POSIX only.
//
// The process never asks a real resolver and never sends beyond loopback: gethostbyname and
// getaddrinfo below replace the C library's for every object linked into this executable, record
// each name asked for, and answer "not found" or 127.0.0.1; names under .target.test answer
// g_targetAddress (127.0.0.1 unless a scenario changes it, without sending anything). The
// services' ports are fixed by the SDK (27900, 27901, 28910, 29900, 29901, 29910, 29920); a
// listener that cannot bind its port is reported as skipped and the name checks still run. The
// web requests go to port 80, which an unprivileged process cannot bind on loopback: connect below
// sends them to 127.0.0.1:18080.
//
// "names" checks the retail state and the parser options (+gamespy is gone); "redirect-*" checks
// +gamespyRedirect with an address and with a domain (each name resolved at every lookup with its
// retail domain replaced); "label-*" checks every +gamespyRedirect<label> option with addresses
// and host-name targets, its precedence over +gamespyRedirect, and every service reaching its
// label's target; the "*-web" scenarios check every stats web request keeping its retail Host
// header.
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
#include "../BF2/Game/Utility/SimpleParser.h"

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
#include <list>
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
in_addr_t g_targetAddress = htonl(INADDR_LOOPBACK);   // the answer for names under .target.test

// Redirect targets of the scenarios: names ending in ".target.test" always answer g_targetAddress.
bool isTargetName(const char* name)
{
    static const char suffix[] = ".target.test";
    const std::size_t n = name ? std::strlen(name) : 0;
    return n > sizeof(suffix) - 1 && std::strcmp(name + n - (sizeof(suffix) - 1), suffix) == 0;
}

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
    // A dotted address is answered without a lookup, as the C library does (the redirect hands
    // the SDK its target in this form); every other name follows g_resolve.
    const in_addr_t numeric = name ? inet_addr(name) : INADDR_NONE;
    if (!g_resolve && numeric == INADDR_NONE && !isTargetName(name))
    {
        h_errno = HOST_NOT_FOUND;
        return 0;
    }
    static struct in_addr address;
    static char* addresses[2];
    static char* noAliases[1];
    static char canonical[256];
    static struct hostent entry;
    address.s_addr = numeric != INADDR_NONE ? numeric : isTargetName(name) ? g_targetAddress : htonl(INADDR_LOOPBACK);
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
    const in_addr_t numeric = node ? inet_addr(node) : INADDR_NONE;
    if (!g_resolve && numeric == INADDR_NONE && !isTargetName(node))
        return EAI_NONAME;
    struct addrinfo* info = static_cast<struct addrinfo*>(std::calloc(1, sizeof(struct addrinfo)));
    struct sockaddr_in* address = static_cast<struct sockaddr_in*>(std::calloc(1, sizeof(struct sockaddr_in)));
    address->sin_family = AF_INET;
    address->sin_addr.s_addr = numeric != INADDR_NONE ? numeric : isTargetName(node) ? g_targetAddress : htonl(INADDR_LOOPBACK);
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

// The retail names every lookup starts from.
const char* const kRetailNames[] = {
    "battlefield2.available.gamespy.com", "battlefield2.ms14.gamespy.com", "battlefield2.master.gamespy.com",
    "gpcm.gamespy.com", "gpsp.gamespy.com", "battlefield2.gamestats.gamespy.com", "BF2Web.gamespy.com",
    "stage-net.gamespy.com", "eapusher.dice.se", "battlefield2.natneg1.gamespy.com",
    "battlefield2.natneg2.gamespy.com", "battlefield2.natneg3.gamespy.com",
};
const int kRetailNameCount = sizeof(kRetailNames) / sizeof(kRetailNames[0]);

// The label options, in id order, as BF2Engine::parseParameters registers them.
const char* const kExpectedLabels[] = {
    "available", "ms14", "master", "gpcm", "gpsp", "gamestats", "BF2Web", "stage-net", "eapusher",
    "natneg1", "natneg2", "natneg3",
};

// Registers the online options exactly as BF2Engine::parseParameters does (+gamespy is gone).
void addOnlineOptions(SimpleParser& parser)
{
    parser.add(77, "gamespyRedirect", "");
    for (int i = 0; i < onlineProvider::labelCount(); ++i)
        parser.add(79 + i, onlineProvider::labelOption(i), onlineProvider::labelHelp(i));
}

// No option: the retail names everywhere, the label table, and the parser options.
void scenarioNames()
{
    using namespace onlineProvider;
    std::puts("names: no option, nothing installed");
    check(!hasRedirect(), "no redirect");
    checkEqual(commandLineArguments(), "", "restart arguments");
    checkEqual(describe(), "online services: retail names, resolved unchanged", "start-up line");
    const char* retailName = "gpcm.gamespy.com";
    check(gsiRedirectHostname(retailName) == retailName, "gsiRedirectHostname returns the name itself");
    prepareGameSpySdk();
    check(gsiRedirectHostname(retailName) == retailName, "still the name itself after prepareGameSpySdk");
    for (int i = 0; i < kRetailNameCount; ++i)
        checkEqual(redirectLookup(kRetailNames[i]), "", std::string(kRetailNames[i]) + " unchanged");

    std::puts("names: the SDK's host-name variables stay retail");
    check(GSIACHostname[0] == 0 && qr2_hostname[0] == 0 && gcd_hostname[0] == 0, "available, master and key overrides empty");
    checkEqual(StatsServerHostname, "gamestats.gamespy.com", "SDK StatsServerHostname");
    checkEqual(GPConnectionManagerHostname, "gpcm.gamespy.com", "SDK GPConnectionManagerHostname");
    checkEqual(GPSearchManagerHostname, "gpsp.gamespy.com", "SDK GPSearchManagerHostname");
    check(!SBOverrideMasterServer && !Matchup1Hostname && !Matchup2Hostname && !Matchup3Hostname, "server list and natneg overrides null");
    {
        HTTPRequest request;
        checkEqual(request.getGameSpyUrl(), "http://BF2Web.gamespy.com/ASP/", "HTTPRequest::getGameSpyUrl");
    }

    std::puts("names: the label options");
    checkEqual(std::to_string(labelCount()), "12", "label count");
    for (int i = 0; i < labelCount() && i < 12; ++i)
    {
        checkEqual(label(i), kExpectedLabels[i], "label " + std::to_string(i));
        checkEqual(labelOption(i), std::string("gamespyRedirect") + kExpectedLabels[i], "option " + std::to_string(79 + i));
    }
    for (int i = 0; i < kRetailNameCount; ++i)
        checkEqual(std::to_string(labelOf(kRetailNames[i])), std::to_string(i), std::string(kRetailNames[i]) + ": label index");
    checkEqual(std::to_string(labelOf("bf2web.GAMESPY.com")), "6", "bf2web.GAMESPY.com: case ignored");
    checkEqual(std::to_string(labelOf("gamestats.gamespy.com")), "5", "gamestats.gamespy.com (StatsServerHostname as set)");
    checkEqual(std::to_string(labelOf("master.gamespy.com")), "2", "master.gamespy.com (qr2's unused MASTER_ADDR)");
    const char* noLabel[] = { "key.gamespy.com", "peerchat.gamespy.com", "motd.gamespy.com", "gamespy.com",
                              "battlefield2.ms13.gamespy.com", "battlefield2.natneg4.gamespy.com", "gpcm.example.test",
                              "gpcm.notgamespy.com", "eapusher.gamespy.com", "gpcm.dice.se", "eapusher.example.test",
                              "BF2Web.gamespy.com.example.test", "logos.example.test" };
    for (unsigned i = 0; i < sizeof(noLabel) / sizeof(noLabel[0]); ++i)
        checkEqual(std::to_string(labelOf(noLabel[i])), "-1", std::string(noLabel[i]) + ": no label option");

    std::puts("names: +gamespy is no option any more; the parser skips it");
    SimpleParser parser('+');
    addOnlineOptions(parser);
    std::list<std::pair<int, std::string> > keys;
    parser.parseString("+gamespy example.test +gamespyRedirectgpcm 10.0.0.4 +gamespyRedirectstage-net 10.0.0.8"
                       " +gamespyRedirect 10.9.9.9 +gamespyRedirectbf2web 10.0.0.7", keys);
    std::string parsed;
    for (std::list<std::pair<int, std::string> >::const_iterator k = keys.begin(); k != keys.end(); ++k)
        parsed += (parsed.empty() ? "" : ",") + std::to_string(k->first) + "=" + k->second;
    checkEqual(parsed, "82=10.0.0.4,86=10.0.0.8,77=10.9.9.9",
               "only the redirect options parse (+gamespy and the misspelt +gamespyRedirectbf2web are skipped)");
    check(!hasRedirect(), "parsing alone changes nothing");
}

// What each service looks up (consecutive repeats count once, as lookedUp() reports them), and the
// names in the web requests' URLs and Host headers.
struct ExpectedLookups
{
    std::string available, master, serverList, gpcm, gpsp, gameStats, natNeg, statsWeb, playerInfo;
    std::string statsWebName, playerInfoName;
};

// The names retail forms, with domain d in place of gamespy.com, each looked up as it is.
ExpectedLookups formedLookups(const std::string& d)
{
    ExpectedLookups e;
    e.available = "battlefield2.available." + d;
    e.master = "battlefield2.master." + d;
    e.serverList = "battlefield2.ms14." + d;
    e.gpcm = "gpcm." + d;
    e.gpsp = "gpsp." + d;
    e.gameStats = "battlefield2.gamestats." + d;
    e.natNeg = "battlefield2.natneg1." + d + ",battlefield2.natneg2." + d + ",battlefield2.natneg3." + d;
    e.statsWeb = e.statsWebName = "BF2Web." + d;
    e.playerInfo = e.playerInfoName = "stage-net." + d;
    return e;
}

// Every service, with the retail names. With resolve false no name resolves (the state today)
// and each service must give up at once; with resolve true the first packet or connection of each
// service must arrive at 127.0.0.1. Without expected every name resolves there; with expected (a
// redirect) the mock answers only addresses and names under .target.test, so only what the
// redirect hands the SDK reaches the listeners.
void scenarioServices(bool resolve, const ExpectedLookups* expected = 0)
{
    using namespace onlineProvider;
    const ExpectedLookups e = expected ? *expected : formedLookups("gamespy.com");
    prepareGameSpySdk();
    g_resolve = resolve && !expected;
    if (resolve)
        startListeners();
    const double started = now();
    double mark = now();
    #define STEP_TIME(what) do { if (!resolve) { const double t = now() - mark; char s[64]; std::snprintf(s, sizeof(s), "%.2f s", t); \
                                 check(t < 1.0, std::string(what) + " gave up in " + s); } mark = now(); } while (0)

    std::puts("availability check");
    check(availabilityCheck() == GSIACAvailable, "result is available");
    checkEqual(lookedUp(), e.available, "looked up");
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
    checkEqual(lookedUp(), e.master, "looked up");
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
    checkEqual(lookedUp(), e.serverList, "looked up");
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
    checkEqual(lookedUp(), e.gpcm, "login looked up");
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
    checkEqual(lookedUp(), e.gpsp, "search looked up");
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
    checkEqual(lookedUp(), e.gameStats, "looked up");
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
    checkEqual(lookedUp(), e.natNeg, "looked up");
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
        checkEqual(url, "http://" + e.statsWebName + "/ASP/getbackendinfo.aspx", "url");
        request.get(url, false);
        const double end = now() + 3.0;
        while (request.isUpdating() && now() < end)
        {
            request.update();
            msleep(5);
        }
        checkEqual(lookedUp(), e.statsWeb, "looked up");
        if (!resolve)
            check(request.failed, "the request failed");
        else if (listener("web").fd >= 0)
        {
            checkArrived("web", "the request");
            const std::string head = firstBytes("web");
            check(head.compare(0, 29, "GET /ASP/getbackendinfo.aspx ") == 0, "request line is GET /ASP/getbackendinfo.aspx");
            check(head.find("Host: " + e.statsWebName + "\r\n") != std::string::npos, "Host header is " + e.statsWebName);
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
    checkEqual(lookedUp(), e.playerInfo, "looked up");
    if (resolve && listener("web").fd >= 0)
    {
        checkArrived("web", "the request");
        check(firstBytes("web").compare(0, 34, "GET /bf2/getplayerinfo.aspx?pid=&g") == 0, "request line is GET /bf2/getplayerinfo.aspx?pid=...");
        check(firstBytes("web").find("Host: " + e.playerInfoName + "\r\n") != std::string::npos, "Host header is " + e.playerInfoName);
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

// ---- +gamespyRedirect ----------------------------------------------------------------------------

// Runs body with stderr written to a temporary file and returns what was written.
template <typename Body>
std::string captureStderr(Body body)
{
    char path[] = "/tmp/online_provider_stderrXXXXXX";
    const int file = mkstemp(path);
    std::fflush(stderr);
    const int saved = dup(2);
    dup2(file, 2);
    body();
    std::fflush(stderr);
    dup2(saved, 2);
    close(saved);
    std::string text;
    char data[4096];
    lseek(file, 0, SEEK_SET);
    for (ssize_t n; (n = read(file, data, sizeof(data))) > 0;)
        text.append(data, static_cast<std::size_t>(n));
    close(file);
    unlink(path);
    return text;
}

int countOf(const std::string& text, const std::string& part)
{
    int n = 0;
    for (std::string::size_type at = text.find(part); at != std::string::npos; at = text.find(part, at + 1))
        ++n;
    return n;
}

std::string redirected(const char* name)
{
    const char* lookup = gsiRedirectHostname(name);
    return lookup ? lookup : "(fails)";
}

// The values every redirect option refuses.
const char* const kRefusedValues[] = { "", "bad host", "1.2.3", "1.2.3.256", "a..b", "host.test.", "\"host.test\"",
                                       "http://host.test", "host.test:80", "host.test/x", "+gamespyRedirect",
                                       "gpcm.gamespy.com=127.0.0.1", "host_name.test" };
const unsigned kRefusedCount = sizeof(kRefusedValues) / sizeof(kRefusedValues[0]);

std::string addressOf(const char* name)
{
    struct hostent* host = gsiGetHostByName(name);
    if (!host)
        return "(fails)";
    char text[INET_ADDRSTRLEN] = "";
    inet_ntop(AF_INET, host->h_addr_list[0], text, sizeof(text));
    return text;
}

// +gamespyRedirect with an address: the values refused, the names matched, the restart arguments,
// the start-up line, the SDK seam with and without the redirect installed, and the log.
void scenarioRedirectNames()
{
    using namespace onlineProvider;
    std::string error;

    std::puts("redirect: values +gamespyRedirect refuses");
    const std::string tooLong = std::string(226, 'a') + ".test";     // 231 characters: no room for battlefield2.available.
    for (unsigned i = 0; i < kRefusedCount; ++i)
    {
        error.clear();
        check(!setRedirect(kRefusedValues[i], error) && !error.empty() && !hasRedirect(),
              std::string("refused \"") + kRefusedValues[i] + "\": " + error);
    }
    error.clear();
    check(!setRedirect(tooLong, error) && !error.empty(), "refused a 231-character domain: " + error.substr(error.size() > 20 ? error.size() - 20 : 0));
    check(setRedirect(std::string(225, 'a') + ".test", error), "a 230-character domain is accepted");

    std::puts("redirect: +gamespyRedirect 10.1.2.3");
    check(setRedirect("10.1.2.3", error), "+gamespyRedirect 10.1.2.3");
    check(hasRedirect(), "redirect in effect");
    for (int i = 0; i < kRetailNameCount; ++i)
    {
        std::string option;
        const std::string lookup = redirectLookup(kRetailNames[i], &option);
        checkEqual(lookup + " " + option, "10.1.2.3 gamespyRedirect", kRetailNames[i]);
    }
    checkEqual(redirectLookup("key.gamespy.com"), "10.1.2.3", "key.gamespy.com (no label, contains gamespy)");
    checkEqual(redirectLookup("BF2Web.GameSpy.com"), "10.1.2.3", "BF2Web.GameSpy.com (case ignored)");
    checkEqual(redirectLookup("www.dice.se"), "10.1.2.3", "www.dice.se (under dice.se)");
    checkEqual(redirectLookup("gamespy.example.test"), "10.1.2.3", "gamespy.example.test (contains gamespy)");
    checkEqual(redirectLookup("dice.se.example.test"), "", "dice.se.example.test (forwarded: not under dice.se)");
    checkEqual(redirectLookup("logos.example.test"), "", "logos.example.test (forwarded)");
    checkEqual(commandLineArguments(), " +gamespyRedirect 10.1.2.3", "restart arguments");
    const std::string line = describe();
    std::printf("  %s\n", line.c_str());
    checkEqual(line, "online services: retail names; other names containing gamespy or under dice.se resolve to 10.1.2.3 (+gamespyRedirect)",
               "the start-up line");
    check(gsiRedirectHostname("gpcm.gamespy.com") != std::string("10.1.2.3"), "the SDK seam is unchanged before prepareGameSpySdk");

    std::puts("redirect: the SDK seam after prepareGameSpySdk, logging each name once");
    std::string results[3];
    const std::string log = captureStderr([&]() {
        prepareGameSpySdk();
        for (int i = 0; i < 3; ++i)
        {
            results[0] = redirected("battlefield2.ms14.gamespy.com");
            results[1] = redirected("logos.example.test");
        }
        results[2] = redirected("eapusher.dice.se");
    });
    checkEqual(results[0], "10.1.2.3", "battlefield2.ms14.gamespy.com looks up");
    checkEqual(results[1], "logos.example.test", "logos.example.test looks up");
    checkEqual(results[2], "10.1.2.3", "eapusher.dice.se looks up");
    std::printf("  log:\n%s", log.c_str());
    check(countOf(log, "redirecting battlefield2.ms14.gamespy.com to 10.1.2.3 (+gamespyRedirect)\n") == 1, "redirected name logged once");
    check(countOf(log, "forwarding logos.example.test\n") == 1, "forwarded name logged once");
    check(countOf(log, "redirecting eapusher.dice.se to 10.1.2.3 (+gamespyRedirect)\n") == 1, "dice.se name logged");
    check(GSIACHostname[0] == 0 && !SBOverrideMasterServer, "the SDK's host-name variables stay untouched");
    checkEqual(lookedUp(), "", "no resolver lookup for an address");
    checkEqual(addressOf("gpcm.gamespy.com"), "10.1.2.3", "gsiGetHostByName(gpcm.gamespy.com)");
    checkEqual(lookedUp(), "10.1.2.3", "the SDK looked up the address");
}

// +gamespyRedirect with a domain: each name is resolved, at every lookup, with its retail domain
// replaced. The mock answers *.target.test with g_targetAddress.
void scenarioRedirectDomain()
{
    using namespace onlineProvider;
    std::string error;
    check(setRedirect("provider.target.test", error), "+gamespyRedirect provider.target.test");

    std::puts("domain: the names looked up instead");
    const char* expected[] = {
        "battlefield2.available.provider.target.test", "battlefield2.ms14.provider.target.test",
        "battlefield2.master.provider.target.test", "gpcm.provider.target.test", "gpsp.provider.target.test",
        "battlefield2.gamestats.provider.target.test", "BF2Web.provider.target.test", "stage-net.provider.target.test",
        "eapusher.provider.target.test", "battlefield2.natneg1.provider.target.test",
        "battlefield2.natneg2.provider.target.test", "battlefield2.natneg3.provider.target.test",
    };
    for (int i = 0; i < kRetailNameCount; ++i)
    {
        std::string option;
        const std::string lookup = redirectLookup(kRetailNames[i], &option);
        checkEqual(lookup + " " + option, std::string(expected[i]) + " gamespyRedirect", kRetailNames[i]);
    }
    checkEqual(redirectLookup("key.gamespy.com"), "key.provider.target.test", "key.gamespy.com");
    checkEqual(redirectLookup("BF2Web.GameSpy.COM"), "BF2Web.provider.target.test", "BF2Web.GameSpy.COM (the label keeps its case)");
    checkEqual(redirectLookup("gamespy.com"), "provider.target.test", "gamespy.com itself");
    checkEqual(redirectLookup("gamespy.example.test"), "", "gamespy.example.test (no retail domain to replace: unchanged)");
    checkEqual(redirectLookup("logos.example.test"), "", "logos.example.test (unchanged)");
    checkEqual(commandLineArguments(), " +gamespyRedirect provider.target.test", "restart arguments");
    const std::string line = describe();
    std::printf("  %s\n", line.c_str());
    check(line.find("resolve with their retail domain replaced by provider.target.test (+gamespyRedirect)") != std::string::npos,
          "the start-up line names the domain");

    std::puts("domain: resolved at every lookup, never at start-up");
    const std::string log = captureStderr([&]() { prepareGameSpySdk(); });
    checkEqual(lookedUp(), "", "prepareGameSpySdk resolved nothing");
    g_targetAddress = inet_addr("10.1.1.1");
    checkEqual(addressOf("gpcm.gamespy.com"), "10.1.1.1", "gpcm.gamespy.com resolves to the first answer");
    g_targetAddress = inet_addr("10.2.2.2");
    checkEqual(addressOf("gpcm.gamespy.com"), "10.2.2.2", "gpcm.gamespy.com resolves to the changed answer");
    checkEqual(join(takeLookups()), "gpcm.provider.target.test,gpcm.provider.target.test",
               "gpcm.provider.target.test was looked up each time; gpcm.gamespy.com never");
    g_targetAddress = htonl(INADDR_LOOPBACK);

    std::puts("domain: a proxied name that does not resolve fails the lookup");
    check(setRedirect("nowhere.invalid", error), "+gamespyRedirect nowhere.invalid (replaces the value)");
    checkEqual(addressOf("gpsp.gamespy.com"), "(fails)", "gsiGetHostByName(gpsp.gamespy.com) fails");
    checkEqual(join(takeLookups()), "gpsp.nowhere.invalid", "only gpsp.nowhere.invalid was looked up");
}

// Every service with +gamespyRedirect 127.0.0.1 and the retail names: the mock resolves no name,
// so every first packet or connection that arrives came through the redirect, and the web
// requests keep the retail names in their Host headers.
void scenarioRedirectServices()
{
    std::string error;
    check(onlineProvider::setRedirect("127.0.0.1", error), "+gamespyRedirect 127.0.0.1");
    ExpectedLookups e = formedLookups("gamespy.com");
    e.available = e.master = e.serverList = e.gpcm = e.gpsp = e.gameStats = e.natNeg = e.statsWeb = e.playerInfo = "127.0.0.1";
    scenarioServices(true, &e);
}

// Every service with +gamespyRedirect provider.target.test: each looks up its own name under that
// domain, reaches loopback, and the web requests keep the retail Host headers.
void scenarioRedirectDomainServices()
{
    std::string error;
    check(onlineProvider::setRedirect("provider.target.test", error), "+gamespyRedirect provider.target.test");
    ExpectedLookups e = formedLookups("provider.target.test");
    e.statsWebName = "BF2Web.gamespy.com";
    e.playerInfoName = "stage-net.gamespy.com";
    const std::string log = captureStderr([&]() { scenarioServices(true, &e); });
    std::printf("  log:\n%s", log.c_str());
    check(countOf(log, "redirecting gpcm.gamespy.com to gpcm.provider.target.test (+gamespyRedirect)\n") == 1, "gpcm logged");
    check(countOf(log, "redirecting BF2Web.gamespy.com to BF2Web.provider.target.test (+gamespyRedirect)\n") == 1, "BF2Web logged");
}

// The request line and Host header of every stats web request the game makes (each persistence
// class builds its URL from getGameSpyUrl() and sends it with HTTPRequest::get or post), of the
// player info request, and of a forwarded request, with +gamespyRedirect 127.0.0.1.
struct WebEndpoint
{
    const char* path;      // after getGameSpyUrl()
    bool post;
    const char* caller;
};

const WebEndpoint kWebEndpoints[] = {
    { "getbackendinfo.aspx",                         false, "PlayerUnlockInfo" },
    { "getplayerinfo.aspx?pid=1&info=rank",          false, "PlayerStats" },
    { "getawardsinfo.aspx?pid=1",                    false, "PlayerAwards" },
    { "getunlocksinfo.aspx?pid=1",                   false, "PlayerUnlocks" },
    { "selectunlock.aspx?pid=1&id=11",               true,  "PlayerUnlocks::selectUnlock" },
    { "getrankinfo.aspx?pid=1",                      false, "PlayerRankInfo" },
    { "ranknotification.aspx?pid=1",                 false, "PlayerRankInfo::clearNotification" },
    { "getleaderboard.aspx?type=score&id=overall",   false, "LeaderBoard" },
    { "searchforplayers.aspx?nick=nobody",           false, "SearchForPlayer" },
    { "VerifyPlayer.aspx?auth=x&SoldierNick=nobody", false, "GSServerSideNameValidation" },
    { "createplayer.aspx?pid=1&cid=US&nick=nobody",  true,  "ProfileManager::createStatsPlayer" },
};

struct PostRequest : HTTPRequest
{
    bool done;
    PostRequest() : done(false) {}
    void onPostComplete(const std::string& text) { done = true; HTTPRequest::onPostComplete(text); }
    void onPostFailure(const std::string& text) { done = true; HTTPRequest::onPostFailure(text); }
};

void clearWeb()
{
    pthread_mutex_lock(&g_lock);
    listener("web").events = 0;
    listener("web").first.clear();
    pthread_mutex_unlock(&g_lock);
}

void checkHead(const std::string& head, const std::string& method, const std::string& path, const std::string& host,
               const std::string& what)
{
    const std::string line = method + " " + path + " HTTP/1.1\r\n";
    check(head.compare(0, line.size(), line) == 0, what + ": request line " + method + " " + path);
    check(countOf(head, "\r\nHost: ") == 1 && head.find("\r\nHost: " + host + "\r\n") != std::string::npos,
          what + ": one Host header, " + host);
}

// labels: +gamespyRedirectBF2Web, +gamespyRedirectstage-net and +gamespyRedirecteapusher 127.0.0.1,
// with +gamespyRedirect at a domain whose names never resolve, so a request that reaches the
// listener went through the label options.
void scenarioRedirectWeb(bool labels = false)
{
    using namespace onlineProvider;
    std::string error;
    if (!labels)
        check(setRedirect("127.0.0.1", error), "+gamespyRedirect 127.0.0.1");
    else
    {
        check(setRedirect("fallback.invalid", error), "+gamespyRedirect fallback.invalid (its names never resolve)");
        for (int l = 0; l < labelCount(); ++l)
            if (label(l) == std::string("BF2Web") || label(l) == std::string("stage-net") || label(l) == std::string("eapusher"))
                check(setLabelRedirect(l, "127.0.0.1", error), "+" + labelOption(l) + " 127.0.0.1");
    }
    prepareGameSpySdk();
    g_resolve = false;              // only the redirect's address can answer
    startListeners();
    if (listener("web").fd < 0)
    {
        std::puts("  skip (127.0.0.1:18080 is in use)");
        stopListeners();
        return;
    }

    std::puts("stats web requests: connected to 127.0.0.1, Host BF2Web.gamespy.com");
    for (unsigned i = 0; i < sizeof(kWebEndpoints) / sizeof(kWebEndpoints[0]); ++i)
    {
        const WebEndpoint& endpoint = kWebEndpoints[i];
        clearWeb();
        takeLookups();
        std::string url;
        if (endpoint.post)
        {
            PostRequest request;
            url = request.getGameSpyUrl() + endpoint.path;
            request.post(url, "", false);
            const double end = now() + 3.0;
            while (!request.done && now() < end)
            {
                request.update();
                msleep(5);
            }
        }
        else
        {
            WebRequest request;
            url = request.getGameSpyUrl() + endpoint.path;
            request.get(url, false);
            const double end = now() + 3.0;
            while (request.isUpdating() && now() < end)
            {
                request.update();
                msleep(5);
            }
        }
        const std::string what = endpoint.caller;
        checkEqual(url.substr(0, 30), "http://BF2Web.gamespy.com/ASP/", what + ": URL");
        checkEqual(lookedUp(), "127.0.0.1", what + ": looked up");
        checkArrived("web", what + ": the request");
        checkHead(firstBytes("web"), endpoint.post ? "POST" : "GET", std::string("/ASP/") + endpoint.path,
                  "BF2Web.gamespy.com", what);
    }

    std::puts("player info request: Host stage-net.gamespy.com");
    clearWeb();
    takeLookups();
    runPlayerStats();
    checkEqual(lookedUp(), "127.0.0.1", "PlayerStatsManager: looked up");
    checkArrived("web", "PlayerStatsManager: the request");
    check(firstBytes("web").compare(0, 32, "GET /bf2/getplayerinfo.aspx?pid=") == 0, "PlayerStatsManager: request line");
    check(countOf(firstBytes("web"), "\r\nHost: ") == 1 && firstBytes("web").find("\r\nHost: stage-net.gamespy.com\r\n") != std::string::npos,
          "PlayerStatsManager: one Host header, stage-net.gamespy.com");

    {
        std::puts("menu image request: connected to 127.0.0.1, Host eapusher.dice.se");
        clearWeb();
        takeLookups();
        {
            WebRequest request;
            request.getFile("http://eapusher.dice.se/bf2/news.png", "/tmp/online_provider_unused.png", false);
            const double end = now() + 3.0;
            while (request.isUpdating() && now() < end)
            {
                request.update();
                msleep(5);
            }
        }
        unlink("/tmp/online_provider_unused.png");
        checkEqual(lookedUp(), "127.0.0.1", "eapusher.dice.se: looked up");
        checkArrived("web", "eapusher.dice.se: the request");
        checkHead(firstBytes("web"), "GET", "/bf2/news.png", "eapusher.dice.se", "eapusher.dice.se");
    }

    std::puts("a request to another host is forwarded (HTTPRequest::getFile, as server logos)");
    clearWeb();
    takeLookups();
    {
        WebRequest request;
        request.getFile("http://logos.example.test/logo.dds", "/tmp/online_provider_unused.dds", false);
        const double end = now() + 3.0;
        while (request.isUpdating() && now() < end)
        {
            request.update();
            msleep(5);
        }
        check(request.failed, "it failed (the name does not resolve)");
    }
    checkEqual(lookedUp(), "logos.example.test", "looked up its own name");
    stopListeners();
}

// ---- +gamespyRedirect<label> ---------------------------------------------------------------------

std::string labelAddress(int i)
{
    return "10.0.0." + std::to_string(i + 1);
}

// Every label option with an address, refused values, precedence over +gamespyRedirect, the restart
// arguments, the start-up line and the log.
void scenarioLabelNames()
{
    using namespace onlineProvider;
    std::string error;

    std::puts("labels: values every label option refuses");
    for (int l = 0; l < labelCount(); ++l)
    {
        unsigned refused = 0;
        std::string first;
        for (unsigned i = 0; i < kRefusedCount; ++i)
        {
            error.clear();
            if (!setLabelRedirect(l, kRefusedValues[i], error) && !error.empty())
                ++refused;
            if (i == 0)
                first = error;
        }
        check(refused == kRefusedCount && !hasRedirect(),
              "+" + labelOption(l) + " refuses all " + std::to_string(refused) + " (empty: " + first + ")");
    }
    check(!setLabelRedirect(labelCount(), "10.0.0.1", error) && !setLabelRedirect(-1, "10.0.0.1", error), "no label option out of range");
    checkEqual(commandLineArguments(), "", "restart arguments after refused values");

    std::puts("labels: each option alone, without +gamespyRedirect");
    for (int l = 0; l < labelCount(); ++l)
        check(setLabelRedirect(l, labelAddress(l), error), "+" + labelOption(l) + " " + labelAddress(l));
    for (int i = 0; i < kRetailNameCount; ++i)
    {
        std::string option;
        const std::string lookup = redirectLookup(kRetailNames[i], &option);
        checkEqual(lookup + " " + option, labelAddress(i) + " " + labelOption(i), kRetailNames[i]);
    }
    checkEqual(redirectLookup("GPCM.GameSpy.com"), "10.0.0.4", "GPCM.GameSpy.com (case ignored)");
    checkEqual(redirectLookup("key.gamespy.com"), "", "key.gamespy.com unchanged (no label option, no +gamespyRedirect)");
    checkEqual(redirectLookup("peerchat.gamespy.com"), "", "peerchat.gamespy.com unchanged");
    checkEqual(redirectLookup("logos.example.test"), "", "logos.example.test unchanged");
    checkEqual(commandLineArguments(),
               " +gamespyRedirectavailable 10.0.0.1 +gamespyRedirectms14 10.0.0.2 +gamespyRedirectmaster 10.0.0.3"
               " +gamespyRedirectgpcm 10.0.0.4 +gamespyRedirectgpsp 10.0.0.5 +gamespyRedirectgamestats 10.0.0.6"
               " +gamespyRedirectBF2Web 10.0.0.7 +gamespyRedirectstage-net 10.0.0.8 +gamespyRedirecteapusher 10.0.0.9"
               " +gamespyRedirectnatneg1 10.0.0.10 +gamespyRedirectnatneg2 10.0.0.11 +gamespyRedirectnatneg3 10.0.0.12",
               "restart arguments");
    const std::string line = describe();
    std::printf("  %s\n", line.c_str());
    for (int l = 0; l < labelCount(); ++l)
        check(line.find(std::string("; ") + label(l) + " names resolve as " + labelAddress(l) + " (+" + labelOption(l) + ")") != std::string::npos,
              "the start-up line names +" + labelOption(l));

}

// Some label options with +gamespyRedirect: precedence, the restart arguments, the start-up line,
// the log, per-lookup resolution of a host-name target and a target that does not resolve.
void scenarioLabelFallback()
{
    using namespace onlineProvider;
    std::string error;
    std::puts("labels: some options with +gamespyRedirect 10.9.9.9");
    check(setRedirect("10.9.9.9", error), "+gamespyRedirect 10.9.9.9");
    check(setLabelRedirect(3, "10.0.0.4", error), "+gamespyRedirectgpcm 10.0.0.4");
    check(setLabelRedirect(6, "stats.target.test", error), "+gamespyRedirectBF2Web stats.target.test");
    struct Case { const char* name; const char* lookup; const char* option; };
    const Case cases[] = {
        { "gpcm.gamespy.com", "10.0.0.4", "gamespyRedirectgpcm" },
        { "BF2Web.gamespy.com", "stats.target.test", "gamespyRedirectBF2Web" },
        { "stage-net.gamespy.com", "10.9.9.9", "gamespyRedirect" },
        { "gpsp.gamespy.com", "10.9.9.9", "gamespyRedirect" },
        { "eapusher.dice.se", "10.9.9.9", "gamespyRedirect" },
        { "key.gamespy.com", "10.9.9.9", "gamespyRedirect" },
        { "logos.example.test", "", "" },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i)
    {
        std::string option;
        const std::string lookup = redirectLookup(cases[i].name, &option);
        checkEqual(lookup + "|" + option, std::string(cases[i].lookup) + "|" + cases[i].option, cases[i].name);
    }
    checkEqual(commandLineArguments(), " +gamespyRedirect 10.9.9.9 +gamespyRedirectgpcm 10.0.0.4 +gamespyRedirectBF2Web stats.target.test",
               "restart arguments");
    const std::string line = describe();
    std::printf("  %s\n", line.c_str());
    checkEqual(line, "online services: retail names; gpcm names resolve as 10.0.0.4 (+gamespyRedirectgpcm); BF2Web names resolve as"
                     " stats.target.test (+gamespyRedirectBF2Web); other names containing gamespy or under dice.se resolve to"
                     " 10.9.9.9 (+gamespyRedirect)", "the start-up line");

    std::puts("labels: the SDK seam");
    std::string results[3];
    const std::string log = captureStderr([&]() {
        prepareGameSpySdk();
        for (int i = 0; i < 2; ++i)
        {
            results[0] = redirected("gpcm.gamespy.com");
            results[1] = redirected("BF2Web.gamespy.com");
        }
        results[2] = redirected("gpsp.gamespy.com");
    });
    checkEqual(results[0], "10.0.0.4", "gpcm.gamespy.com looks up");
    checkEqual(results[1], "stats.target.test", "BF2Web.gamespy.com looks up");
    checkEqual(results[2], "10.9.9.9", "gpsp.gamespy.com looks up");
    std::printf("  log:\n%s", log.c_str());
    check(countOf(log, "redirecting gpcm.gamespy.com to 10.0.0.4 (+gamespyRedirectgpcm)\n") == 1, "label option logged once");
    check(countOf(log, "redirecting BF2Web.gamespy.com to stats.target.test (+gamespyRedirectBF2Web)\n") == 1, "host-name target logged once");
    check(countOf(log, "redirecting gpsp.gamespy.com to 10.9.9.9 (+gamespyRedirect)\n") == 1, "fallback logged");
    checkEqual(lookedUp(), "", "the seam itself resolves nothing");

    std::puts("labels: a host-name target is resolved at every lookup");
    g_targetAddress = inet_addr("10.3.3.3");
    checkEqual(addressOf("BF2Web.gamespy.com"), "10.3.3.3", "BF2Web.gamespy.com resolves to the first answer");
    g_targetAddress = inet_addr("10.4.4.4");
    checkEqual(addressOf("BF2Web.gamespy.com"), "10.4.4.4", "BF2Web.gamespy.com resolves to the changed answer");
    checkEqual(join(takeLookups()), "stats.target.test,stats.target.test", "stats.target.test looked up each time");
    g_targetAddress = htonl(INADDR_LOOPBACK);

    std::puts("labels: a host-name target that does not resolve fails the lookup");
    check(setLabelRedirect(4, "missing.invalid", error), "+gamespyRedirectgpsp missing.invalid");
    checkEqual(addressOf("gpsp.gamespy.com"), "(fails)", "gsiGetHostByName(gpsp.gamespy.com) fails");
    checkEqual(join(takeLookups()), "missing.invalid", "neither gpsp.gamespy.com nor the fallback was tried");
}

// Every label option with its own host-name target and +gamespyRedirect at a domain whose names
// never resolve: every packet or connection that arrives went to its label's target, which the
// SDK looked up itself.
void scenarioLabelServices()
{
    using namespace onlineProvider;
    std::string error;
    check(setRedirect("fallback.invalid", error), "+gamespyRedirect fallback.invalid (its names never resolve)");
    for (int l = 0; l < labelCount(); ++l)
        check(setLabelRedirect(l, std::string(label(l)) + ".target.test", error),
              "+" + labelOption(l) + " " + label(l) + ".target.test");
    ExpectedLookups e = formedLookups("gamespy.com");
    e.available  = "available.target.test";
    e.master     = "master.target.test";
    e.serverList = "ms14.target.test";
    e.gpcm       = "gpcm.target.test";
    e.gpsp       = "gpsp.target.test";
    e.gameStats  = "gamestats.target.test";
    e.natNeg     = "natneg1.target.test,natneg2.target.test,natneg3.target.test";
    e.statsWeb   = "BF2Web.target.test";
    e.playerInfo = "stage-net.target.test";
    const std::string log = captureStderr([&]() { scenarioServices(true, &e); });

    std::puts("the name not looked up by the services above");
    checkEqual(redirected("eapusher.dice.se"), "eapusher.target.test", "eapusher.dice.se looks up");
    checkEqual(addressOf("eapusher.dice.se"), "127.0.0.1", "and resolves");
    checkEqual(lookedUp(), "eapusher.target.test", "eapusher.target.test was looked up");

    std::printf("  log:\n%s", log.c_str());
    for (int i = 0; i < kRetailNameCount; ++i)
    {
        if (i == 8)
            continue;   // eapusher.dice.se: no service above
        const std::string text = std::string("redirecting ") + kRetailNames[i] + " to " + label(i) + ".target.test (+" + labelOption(i) + ")\n";
        check(countOf(log, text) == 1, "logged once: " + text.substr(0, text.size() - 1));
    }
    check(log.find("fallback.invalid") == std::string::npos, "no name went to +gamespyRedirect");
}

int runScenario(const std::string& name)
{
    if (name == "names")
        scenarioNames();
    else if (name == "retail-unresolved")
        scenarioServices(false);
    else if (name == "retail-resolved")
        scenarioServices(true);
    else if (name == "redirect-names")
        scenarioRedirectNames();
    else if (name == "redirect-domain")
        scenarioRedirectDomain();
    else if (name == "redirect-services")
        scenarioRedirectServices();
    else if (name == "redirect-domain-services")
        scenarioRedirectDomainServices();
    else if (name == "redirect-web")
        scenarioRedirectWeb();
    else if (name == "label-names")
        scenarioLabelNames();
    else if (name == "label-fallback")
        scenarioLabelFallback();
    else if (name == "label-services")
        scenarioLabelServices();
    else if (name == "label-web")
        scenarioRedirectWeb(true);
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

    const char* scenarios[] = { "names", "retail-unresolved", "retail-resolved", "redirect-names", "redirect-domain",
                                "redirect-services", "redirect-domain-services", "redirect-web", "label-names",
                                "label-fallback", "label-services", "label-web", "silent-host", "refused", "silent-tcp" };
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
