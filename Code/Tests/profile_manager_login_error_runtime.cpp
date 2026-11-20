// [TEMP] [CHOSEN, not recovered] Test fixture for the text of the online-account error popups
// (ProfileManager::reportError). The production ProfileManager runs against the linked GameSpy SDK and
// loopback stand-ins for the availability check (27900), the connection manager (gpcm, 29900) and the
// search manager (gpsp, 29901). Built on request (bf2_profile_manager_login_error_check); POSIX only.
//
// gethostbyname and getaddrinfo below replace the C library's for every object linked into this
// executable and answer 127.0.0.1, so no name is asked of a resolver and nothing leaves loopback.
//
// loginAccount by e-mail finds the account's nick (gpsp nicks + check succeed), then the gpcm login
// refuses the password, as a wrong password does. The popup must carry the server's errmsg verbatim
// (a type-1 message).
#include "../BF2/Game/ProfileManager.h"
#include "../BF2/Game/LocalProfileManager.h"
#include "../BF2/Core/ClassManager.h"
#include "SwiffHostStandIn.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace dice::hfe;

namespace dice { namespace hfe {
ClassManager* classManager = nullptr;
} }

namespace {

unsigned g_checks, g_failures;
void check(bool pass, const char* label)
{
    ++g_checks;
    if (!pass)
    {
        ++g_failures;
        std::printf("FAIL %s\n", label);
    }
}

const char kRefusal[] = "The password provided is incorrect.";

// ---- the menu side: the popup channel and the local profile owner -------------------------------
struct PopupHost : SwiffHostStandIn {
    std::vector<SwiffHostMessage> posted;
    bool postMessage(const SwiffHostMessage& message) override { posted.push_back(message); return true; }
    void setSelectedPlayerId(int) override {}   // logout's notification
};
PopupHost g_host;

// The GP_ERROR callback asks the local owner for its current profile; none is current while an
// account is retrieved. Every other slot aborts.
struct ProfileOwner : ILocalProfileManager {
    void addRef() override { std::abort(); }
    int getRef() override { std::abort(); }
    int release() override { std::abort(); }
    void* queryInterface(unsigned int) const override { std::abort(); }
    bool hasDefaultProfile() override { std::abort(); }
    bool createDefaultProfile() override { std::abort(); }
    bool init() override { std::abort(); }
    bool login(int,const std::string&,const std::string&) override { std::abort(); }
    void shutdown() override { std::abort(); }
    bool createProfile(const std::string&,const std::string&) override { std::abort(); }
    bool deleteProfile(int) override { std::abort(); }
    bool createOnlineAccount(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    bool hasCurrentProfile(int) override { std::abort(); }
    bool slot34(int, int) override { std::abort(); }
    bool hasProfile(const std::string&) override { std::abort(); }
    bool loadCurrentProfile() override { std::abort(); }
    bool saveCurrentProfile() override { std::abort(); }
    LocalProfile* getCurrentProfile() override { return nullptr; }
    bool getLocalProfiles(std::vector<LocalProfile>&,bool) override { std::abort(); }
    std::string getProfilesPath() override { std::abort(); }
    bool findProfileDirectory(const std::string&,std::string&) override { std::abort(); }
    bool loadProfileFile(const std::string&) override { std::abort(); }
    bool applySettings(bool) override { std::abort(); }
    bool loginOnline(const std::string&,const std::string&,bool) override { std::abort(); }
    bool loginOnlineAlternate(const std::string&,const std::string&,bool) override { std::abort(); }
    void setName(const std::string&) override { std::abort(); }
    void setGamespyNick(const std::string&) override { std::abort(); }
    void setEmail(const std::string&) override { std::abort(); }
    void setPassword(const std::string&,bool) override { std::abort(); }
    void setNick(const std::string&) override { std::abort(); }
    void setNumTimesLoggedIn(int) override { std::abort(); }
    std::string getName() override { std::abort(); }
    std::string getGamespyNick() override { std::abort(); }
    std::string getEmail() override { std::abort(); }
    std::string getPassword() override { std::abort(); }
    std::string getNick() override { std::abort(); }
    int getNumTimesLoggedIn() override { std::abort(); }
    bool hasDefaultVoiceSettings() override { std::abort(); }
    bool loadVoiceSettings() override { std::abort(); }
    bool saveVoiceSettings() override { std::abort(); }
    bool readVoiceSettings(float&,float&,float&,bool&) override { std::abort(); }
    void addDemoBookmark(const std::string&,const std::string&,const std::string&,const std::string&) override { std::abort(); }
    unsigned int getNumDemoBookmarks() override { std::abort(); }
    LocalDemoBookmark* getDemoBookmark(unsigned int) override { std::abort(); }
    bool removeDemoBookmark(unsigned int) override { std::abort(); }
    float getTotalPlayedTime() override { std::abort(); }
    void setTotalPlayedTime(float) override { std::abort(); }
    bool validateProfileName(const std::string&,std::string&) override { std::abort(); }
    bool quickLogin(const std::string&,const std::string&) override { std::abort(); }
    bool saveGlobalSettings() override { std::abort(); }
    bool validateProfileFiles(const std::string&) override { std::abort(); }
    int getLoginProfileIndex() override { std::abort(); }
    std::string getLoginPassword() override { std::abort(); }
};
ProfileOwner g_owner;

struct Registry : ClassManager {
    IBase* getSingleton(const std::string& name) override
    {
        if (name == "SwiffHost")
            return &g_host;
        if (name == "LocalProfileManager")
            return &g_owner;
        check(false, "the profile manager looks up only SwiffHost and LocalProfileManager");
        return nullptr;
    }
};

// ---- the services ------------------------------------------------------------------------------
volatile bool g_stop = false;
int g_available = -1, g_gpcm = -1, g_gpsp = -1;
std::vector<std::string> g_requests;   // first key of every gpcm/gpsp request, in order
pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

int openListener(int port, bool udp)
{
    int fd = socket(AF_INET, udp ? SOCK_DGRAM : SOCK_STREAM, 0);
    int on = 1;
    if (fd >= 0 && !udp)
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(static_cast<unsigned short>(port));
    if (fd < 0 || bind(fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0
        || (!udp && listen(fd, 4) != 0))
    {
        std::printf("127.0.0.1:%d not available (%s)\n", port, std::strerror(errno));
        std::exit(2);
    }
    return fd;
}

// Reads one request up to its \final\ terminator.
std::string readRequest(int peer)
{
    std::string text;
    char data[2048];
    while (text.find("\\final\\") == std::string::npos)
    {
        ssize_t n = recv(peer, data, sizeof(data), 0);
        if (n <= 0)
            break;
        text.append(data, static_cast<std::size_t>(n));
    }
    pthread_mutex_lock(&g_lock);
    const std::size_t end = text.find('\\', 1);
    g_requests.push_back(text.substr(1, end == std::string::npos ? 0 : end - 1));
    pthread_mutex_unlock(&g_lock);
    return text;
}

void sendText(int peer, const std::string& text) { send(peer, text.data(), text.size(), 0); }

std::string refusal()
{
    return std::string("\\error\\\\err\\260\\fatal\\\\errmsg\\") + kRefusal + "\\id\\1\\final\\";
}

void serveConnection(int listener, bool gpcm)
{
    int peer = accept(listener, 0, 0);
    if (peer < 0)
        return;
    fcntl(peer, F_SETFL, fcntl(peer, F_GETFL, 0) & ~O_NONBLOCK);
    struct timeval limit = { 5, 0 };
    setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &limit, sizeof(limit));
    if (gpcm)
    {
        // The login handshake: the server's challenge, the client's login, the server's refusal.
        sendText(peer, "\\lc\\1\\challenge\\ABCDEFGHIJ\\id\\1\\final\\");
        readRequest(peer);
        sendText(peer, refusal());
    }
    else
    {
        const std::string request = readRequest(peer);
        if (request.compare(0, 7, "\\nicks\\") == 0)
            sendText(peer, "\\nr\\1\\nick\\thenick\\uniquenick\\thenick\\ndone\\\\final\\");
        else if (request.compare(0, 7, "\\check\\") == 0)
            sendText(peer, "\\cur\\0\\pid\\4242\\final\\");
    }
    char drain[256];
    while (recv(peer, drain, sizeof(drain), 0) > 0) {}
    close(peer);
}

void* serve(void*)
{
    while (!g_stop)
    {
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(g_available, &readable);
        FD_SET(g_gpcm, &readable);
        FD_SET(g_gpsp, &readable);
        int highest = g_available > g_gpcm ? g_available : g_gpcm;
        if (g_gpsp > highest)
            highest = g_gpsp;
        struct timeval wait = { 0, 20000 };
        if (select(highest + 1, &readable, 0, 0, &wait) <= 0)
            continue;
        if (FD_ISSET(g_available, &readable))
        {
            // The availability request is 09 00 00 00 00 <game>; answer "all services available".
            char data[256];
            struct sockaddr_in from;
            socklen_t fromLength = sizeof(from);
            ssize_t n = recvfrom(g_available, data, sizeof(data), 0, reinterpret_cast<struct sockaddr*>(&from), &fromLength);
            if (n > 0 && data[0] == 0x09)
                sendto(g_available, "\xFE\xFD\x09\x00\x00\x00\x00", 7, 0, reinterpret_cast<struct sockaddr*>(&from), fromLength);
        }
        if (FD_ISSET(g_gpcm, &readable))
            serveConnection(g_gpcm, true);
        if (FD_ISSET(g_gpsp, &readable))
            serveConnection(g_gpsp, false);
    }
    return 0;
}

std::string requests()
{
    pthread_mutex_lock(&g_lock);
    std::string text;
    for (std::size_t i = 0; i < g_requests.size(); ++i)
        text += (i ? " " : "") + g_requests[i];
    g_requests.clear();
    pthread_mutex_unlock(&g_lock);
    return text;
}

void runScenario(const char* name, const char* expectedRequests)
{
    g_host.posted.clear();
    requests();
    bool result;
    {
        ProfileManager manager;
        result = manager.loginAccount("player@example.invalid", "wrongpassword-test", "");
    }
    const std::string seen = requests();
    std::printf("%s: loginAccount=%d, requests [%s], %u message(s)\n", name, result ? 1 : 0, seen.c_str(),
                static_cast<unsigned>(g_host.posted.size()));
    for (std::size_t i = 0; i < g_host.posted.size(); ++i)
        std::printf("  message type %d code %d text \"%s\" (%u bytes)\n", g_host.posted[i].type, g_host.posted[i].code,
                    g_host.posted[i].text.c_str(), static_cast<unsigned>(g_host.posted[i].text.size()));
    check(!result, "a refused password fails the retrieval");
    check(seen == expectedRequests, "the retrieval reaches the expected services");
    check(g_host.posted.size() == 1, "one popup message is posted");
    check(g_host.posted.size() == 1 && g_host.posted[0].type == 1 && g_host.posted[0].code == 3,
          "the server's text is posted as a plain (type 1) message");
    check(g_host.posted.size() == 1 && g_host.posted[0].text == kRefusal,
          "the popup carries the server's errmsg verbatim");
}

} // namespace

extern "C" struct hostent* gethostbyname(const char* name)
{
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

extern "C" int getaddrinfo(const char*, const char*, const struct addrinfo*, struct addrinfo** result)
{
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

int main()
{
    Registry registry;
    classManager = &registry;
    g_available = openListener(27900, true);
    g_gpcm = openListener(29900, false);
    g_gpsp = openListener(29901, false);
    fcntl(g_gpcm, F_SETFL, fcntl(g_gpcm, F_GETFL, 0) | O_NONBLOCK);
    fcntl(g_gpsp, F_SETFL, fcntl(g_gpsp, F_GETFL, 0) | O_NONBLOCK);
    pthread_t thread;
    pthread_create(&thread, 0, &serve, 0);

    runScenario("login-refused", "nicks check login");

    g_stop = true;
    pthread_join(thread, 0);
    std::printf("%u checks, %u failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
