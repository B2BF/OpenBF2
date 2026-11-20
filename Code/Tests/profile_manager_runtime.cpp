// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: profile-manager-runtime]
// Strict x86 owner fixture. Only the upstream GP/availability entry points and the
// SwiffHost observer are substituted. The production ProfileManager owner/factory
// are linked intact, as is the real AntiHacks the login stamps.
// No SDK networking is linked or performed by this executable.
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>
#include <type_traits>
#include <cstring>
#include "../BF2/Game/ProfileManager.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/External/GameSpy/gp/gp.h"
#include "../BF2/External/GameSpy/common/gsAvailable.h"
#include "../BF2/External/GameSpy/common/gsPlatformUtil.h"
#include "../BF2/Game/Persistence/AntiHacks.h"
#include "SwiffHostStandIn.h"

using namespace dice::hfe;
static unsigned checks, failures;
static void check(bool pass, const char* label)
{ ++checks; if (!pass) { ++failures; std::printf("FAIL %s\n", label); } }

// Allocation observers prove actual owner/record destruction without replacing
// their destructors, release, factory, or any production virtual method.
static void* watched[16];
static unsigned freed[32], freedCount;
static bool captureOwner;
static void* lastOwner;
void* operator new(std::size_t n)
{
    void* p = std::malloc(n ? n : 1);
    if (!p) throw std::bad_alloc();
    if (captureOwner && n == sizeof(ProfileManager)) {
        lastOwner = p; watched[0] = p; captureOwner = false;
    }
    return p;
}
void operator delete(void* p) noexcept
{
    if (p) for (unsigned i = 0; i != 16; ++i)
        if (watched[i] == p && freedCount != 32) {
            freed[freedCount++] = i;
            watched[i] = nullptr; // Later unrelated allocations may reuse this address.
            break;
        }
    std::free(p);
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
static void resetWatched()
{ for (unsigned i = 0; i != 16; ++i) watched[i] = nullptr; freedCount = 0; }
static unsigned countFreed(unsigned index)
{ unsigned n = 0; for (unsigned i = 0; i != freedCount; ++i) if (freed[i] == index) ++n; return n; }

static std::vector<std::string> calls;
static GPResult connectedResult = GP_NO_ERROR, processResult = GP_NO_ERROR;
static GPEnum connectedValue = GP_CONNECTED;
static GPConnection* expectedConnection;
static ProfileManager* observedOwner;

namespace dice { namespace hfe {
ClassManager* classManager = nullptr;
struct ProfileManagerFixture {
    static bool fresh(const ProfileManager& p)
    { return p.m_refCount == 1 && !p.m_currentProfile && !p.m_connection
        && p.m_nickSuggestions.empty() && !p.m_pendingTransition && p.m_pendingTransitionString.empty(); }
    static GPConnection* connection(ProfileManager& p) { return &p.m_connection; }
    static void setConnection(ProfileManager& p, void* value) { p.m_connection = value; }
    static OnlineProfile* seed(ProfileManager& p)
    {
        p.m_currentProfile = new OnlineProfile;
        p.m_currentProfile->email.assign(81, 'e');
        p.m_currentProfile->password.assign(82, 'p');
        p.m_currentProfile->uniqueNick.assign(83, 'u');
        p.m_currentProfile->profileId = 12345;
        p.m_currentProfile->profiles.resize(1);
        p.m_currentProfile->profiles[0].profileId = 6789;
        p.m_currentProfile->profiles[0].nick.assign(84, 'n');
        p.m_currentProfile->profiles[0].uniqueNick.assign(85, 'v');
        return p.m_currentProfile;
    }
    static void watchRecord(ProfileManager& p)
    {
        OnlineProfile& r = *p.m_currentProfile;
        watched[1] = &r;
        watched[2] = const_cast<char*>(r.uniqueNick.data());
        watched[3] = const_cast<char*>(r.profiles[0].uniqueNick.data());
        watched[4] = const_cast<char*>(r.profiles[0].nick.data());
        watched[5] = const_cast<char*>(r.password.data());
        watched[6] = const_cast<char*>(r.email.data());
    }
    static void suggestions(ProfileManager& p, const std::vector<std::string>& values)
    { p.m_nickSuggestions = values; }
    static const std::string& pendingText(const ProfileManager& p)
    { return p.m_pendingTransitionString; }
};
} }

extern "C" GPResult gpIsConnected(GPConnection* connection, GPEnum* connected)
{
    calls.push_back("isConnected");
    check(connection == expectedConnection, "isConnected receives owner connection address");
    *connected = connectedValue;
    return connectedResult;
}
extern "C" GPResult gpProcess(GPConnection* connection)
{
    calls.push_back("process");
    check(connection == expectedConnection, "process receives owner connection address");
    return processResult;
}
extern "C" void gpDisconnect(GPConnection* connection)
{
    calls.push_back("disconnect");
    check(connection == expectedConnection, "disconnect receives owner connection address");
    check(observedOwner && !observedOwner->getCurrentProfile(), "logout clears current record before disconnect");
}
extern "C" void gpDestroy(GPConnection* connection)
{
    calls.push_back("destroy");
    check(connection == expectedConnection, "destroy receives same owner connection address");
    *connection = nullptr;
}

// login's SDK surface
// Every entry point ProfileManager::login reaches, substituted so the fixture stays network-free.
// Each records its call and the arguments the shipped body is proved to pass.
static GSIACResult availableAnswer = GSIACAvailable;
static int         availableWaits = 0;
static GPResult    initializeResult = GP_NO_ERROR, connectResult = GP_NO_ERROR;
static GPResult    userNicksResult = GP_NO_ERROR, checkUserResult = GP_NO_ERROR;
static GPResult    loginTicketResult = GP_NO_ERROR;
static int         connectProfileId = 0, checkUserProfileId = 0;
static const char* getInfoEmail = "";
static const char* sdkErrorText = "";
static const char* loginTicketText = "";
static std::vector<std::string> offeredNicks, offeredUniqueNicks;
static std::string lastGameName, lastConnectNick, lastConnectPassword;
static std::string lastNicksEmail, lastNicksPassword, lastCheckNick;
static int         lastProductId, lastNamespaceId, lastPartnerId, lastGetInfoProfile;
static GPEnum      lastCallbackFunc;
static void*       lastCallbackParam;
static GPCallback  lastErrorCallback;

extern "C" void msleep(gsi_time msec)
{
    calls.push_back("msleep");
    check(msec == 5, "availability poll sleeps the shipped five milliseconds");
}
extern "C" void GSIStartAvailableCheckA(const char* gamename)
{
    calls.push_back("startAvailable");
    lastGameName = gamename ? gamename : "";
    availableWaits = availableWaits;
}
extern "C" GSIACResult GSIAvailableCheckThink(void)
{
    calls.push_back("think");
    if (availableWaits > 0) { --availableWaits; return GSIACWaiting; }
    return availableAnswer;
}
extern "C" GPResult gpInitialize(GPConnection* connection, int productID, int namespaceID, int partnerID)
{
    calls.push_back("initialize");
    check(connection == expectedConnection, "initialize receives owner connection address");
    lastProductId = productID; lastNamespaceId = namespaceID; lastPartnerId = partnerID;
    return initializeResult;
}
extern "C" GPResult gpSetCallback(GPConnection* connection, GPEnum func, GPCallback callback, void* param)
{
    calls.push_back("setCallback");
    check(connection == expectedConnection, "setCallback receives owner connection address");
    lastCallbackFunc = func; lastErrorCallback = callback; lastCallbackParam = param;
    return GP_NO_ERROR;
}
extern "C" GPResult gpConnectUniqueNickA(GPConnection* connection, const char* uniquenick,
                                         const char* password, GPEnum firewall, GPEnum blocking,
                                         GPCallback callback, void* param)
{
    calls.push_back("connect");
    check(connection == expectedConnection, "connect receives owner connection address");
    check(firewall == GP_NO_FIREWALL && blocking == GP_BLOCKING, "connect is firewall-free and blocking");
    lastConnectNick = uniquenick ? uniquenick : "";
    lastConnectPassword = password ? password : "";
    if (connectResult == GP_NO_ERROR && callback) {
        GPConnectResponseArg arg; std::memset(&arg, 0, sizeof(arg));
        arg.result = GP_NO_ERROR; arg.profile = connectProfileId;
        callback(connection, &arg, param);
    }
    return connectResult;
}
extern "C" GPResult gpGetInfo(GPConnection* connection, GPProfile profile, GPEnum checkCache,
                               GPEnum blocking, GPCallback callback, void* param)
{
    calls.push_back("getInfo");
    check(connection == expectedConnection, "getInfo receives owner connection address");
    check(checkCache == GP_DONT_CHECK_CACHE && blocking == GP_BLOCKING, "getInfo bypasses the cache and blocks");
    lastGetInfoProfile = profile;
    if (callback) {
        GPGetInfoResponseArg arg; std::memset(&arg, 0, sizeof(arg));
        arg.result = GP_NO_ERROR;
        std::strcpy(arg.email, getInfoEmail);
        callback(connection, &arg, param);
    }
    return GP_NO_ERROR;
}
extern "C" GPResult gpGetUserNicksA(GPConnection* connection, const char* email, const char* password,
                                    GPEnum blocking, GPCallback callback, void* param)
{
    calls.push_back("getUserNicks");
    check(connection == expectedConnection, "getUserNicks receives owner connection address");
    check(blocking == GP_BLOCKING, "getUserNicks blocks");
    lastNicksEmail = email ? email : "";
    lastNicksPassword = password ? password : "";
    if (userNicksResult == GP_NO_ERROR && callback) {
        static std::vector<char*> nicks, uniques;
        nicks.clear(); uniques.clear();
        for (std::size_t i = 0; i != offeredNicks.size(); ++i) {
            nicks.push_back(const_cast<char*>(offeredNicks[i].c_str()));
            uniques.push_back(const_cast<char*>(offeredUniqueNicks[i].c_str()));
        }
        GPGetUserNicksResponseArg arg; std::memset(&arg, 0, sizeof(arg));
        arg.result = GP_NO_ERROR;
        arg.numNicks = static_cast<int>(nicks.size());
        arg.nicks = nicks.empty() ? nullptr : &nicks[0];
        arg.uniquenicks = uniques.empty() ? nullptr : &uniques[0];
        callback(connection, &arg, param);
    }
    return userNicksResult;
}
extern "C" GPResult gpCheckUserA(GPConnection* connection, const char* nick, const char* email,
                                 const char* password, GPEnum blocking, GPCallback callback, void* param)
{
    calls.push_back("checkUser");
    check(connection == expectedConnection, "checkUser receives owner connection address");
    check(blocking == GP_BLOCKING, "checkUser blocks");
    lastCheckNick = nick ? nick : "";
    check(lastNicksEmail == (email ? email : ""), "checkUser reuses the record's e-mail");
    check(lastNicksPassword == (password ? password : ""), "checkUser reuses the record's password");
    if (checkUserResult == GP_NO_ERROR && callback) {
        GPCheckResponseArg arg; std::memset(&arg, 0, sizeof(arg));
        arg.result = GP_NO_ERROR; arg.profile = checkUserProfileId;
        callback(connection, &arg, param);
    }
    return checkUserResult;
}
extern "C" GPResult gpGetErrorStringA(GPConnection* connection, char errorString[GP_ERROR_STRING_LEN])
{
    calls.push_back("errorString");
    check(connection == expectedConnection, "errorString receives owner connection address");
    std::strcpy(errorString, sdkErrorText);
    return GP_NO_ERROR;
}
extern "C" GPResult gpGetLoginTicket(GPConnection* connection, char loginTicket[GP_LOGIN_TICKET_LEN])
{
    calls.push_back("loginTicket");
    check(connection == expectedConnection, "loginTicket receives owner connection address");
    if (loginTicketResult == GP_NO_ERROR)
        std::strncpy(loginTicket, loginTicketText, GP_LOGIN_TICKET_LEN - 1);
    return loginTicketResult;
}

// The login witness's own host and registry: the logout observer above asserts a zero selected id
// and refuses every name but SwiffHost, neither of which holds on the login path.
struct LoginHost : SwiffHostStandIn {
    std::vector<SwiffHostMessage> posted;
    int selected = -1;
    bool postMessage(const SwiffHostMessage& message) override
    { calls.push_back("post"); posted.push_back(message); return true; }
    void setSelectedPlayerId(int id) override
    { calls.push_back("selected"); selected = id; }
};
static LoginHost loginHost;
struct LoginRegistry : ClassManager {
    bool hasHost = true;
    IBase* getSingleton(const std::string& name) override
    {
        calls.push_back("lookup");
        check(name == "SwiffHost", "the login path resolves SwiffHost by the shipped name");
        return hasHost ? static_cast<IBase*>(&loginHost) : nullptr;
    }
};

struct Host : SwiffHostStandIn {
    void setSelectedPlayerId(int id) override
    {
        calls.push_back("selected0");
        check(id == 0, "logout notifies selected player zero");
        check(observedOwner && !observedOwner->getCurrentProfile(), "host sees current record cleared");
        check(!*expectedConnection, "host sees GP connection destroyed");
    }
    bool validateProfileName(const std::string& name, std::string& error) override
    { calls.push_back("validate"); check(name == "desired", "validator forwards input"); error = "observed refusal"; return false; }
};
static Host host;
struct Registry : ClassManager {
    bool hasHost = true;
    IBase* getSingleton(const std::string& name) override
    {
        calls.push_back("lookup");
        check(name == "SwiffHost", "owner resolves SwiffHost by shipped name");
        return hasHost ? static_cast<IBase*>(&host) : nullptr;
    }
};

struct VirtualStatus : ProfileManager {
    bool answer = false;
    bool isLoggedIn() override { calls.push_back("virtualStatus"); return answer; }
};

struct SelectingOwner : ProfileManager {
    bool answer = false;
    bool login(const std::string& nick, const std::string& password) override
    {
        calls.push_back("virtualLogin");
        check(nick == "selected name", "selectProfile forwards requested name");
        const std::string before = password;
        getCurrentProfile()->password = "mutated during login";
        check(password == before && password == std::string(82, 'p'),
            "selectProfile preserves password copy across mutation in virtual login");
        return answer;
    }
};

template<class Fn> static Fn slot(IProfileManager* p, unsigned byteOffset)
{ return reinterpret_cast<Fn>((*reinterpret_cast<void***>(p))[byteOffset / 4]); }
typedef bool (__thiscall* BoolSlot)(IProfileManager*);
typedef OnlineProfile* (__thiscall* RecordSlot)(IProfileManager*);
typedef bool (__thiscall* IntOutSlot)(IProfileManager*, int&);
typedef bool (__thiscall* StringOutSlot)(IProfileManager*, std::string&);
typedef bool (__thiscall* StringsOutSlot)(IProfileManager*, std::vector<std::string>&);
typedef void (__thiscall* PendingSlot)(IProfileManager*, bool, const std::string&);

int main()
{
    static_assert(sizeof(void*) == 4, "fixture requires MSVC x86");
    static_assert(!std::is_abstract<ProfileManager>::value, "complete typed owner surface");
    Registry registry;
    classManager = &registry;

    OnlineProfile record;
    check(record.email.empty() && record.password.empty() && record.uniqueNick.empty()
        && record.profiles.empty() && record.profileId == 0, "record constructor initializes strings/vector/id");

    resetWatched(); captureOwner = true;
    IBase* unsupported = ProfileManager::create(0xFFFFFFFFu, nullptr);
    check(!unsupported && lastOwner && countFreed(0) == 1, "unsupported factory IID releases actual allocation");
    resetWatched(); captureOwner = true;
    check(!ProfileManager::create(IID_IProfileManager, &host) && captureOwner,
        "aggregation rejected before allocation");
    captureOwner = false;
    for (unsigned iid : {1u, static_cast<unsigned>(IID_IProfileManager)}) {
        resetWatched(); captureOwner = true;
        IBase* base = ProfileManager::create(iid, nullptr);
        check(base && base == lastOwner && base->getRef() == 1, "factory accepts interface with one reference");
        if (!base) return 1;
        check(base->queryInterface(1) == base && base->queryInterface(IID_IProfileManager) == base
            && !base->queryInterface(0xFFFFFFFFu) && base->getRef() == 1, "queryInterface identities do not add references");
        base->addRef(); check(base->getRef() == 2 && base->release() == 1, "reference increment/decrement");
        check(base->release() == 0 && countFreed(0) == 1, "final release deletes actual owner");
    }
    resetWatched();

    {
        ProfileManager owner;
        observedOwner = &owner;
        expectedConnection = ProfileManagerFixture::connection(owner);
        IProfileManager* api = &owner;
        check(ProfileManagerFixture::fresh(owner), "fresh owner fields");
        calls.clear();
        check(!slot<BoolSlot>(api, 0x44)(api) && !slot<BoolSlot>(api, 0x48)(api)
            && calls.empty(), "fresh status/update return false without GP calls");
        check(!slot<RecordSlot>(api, 0x2C)(api), "typed current-profile slot returns null");
        int id = -234;
        std::string out = "unchanged";
        check(!slot<IntOutSlot>(api, 0x30)(api, id) && id == -234, "absent profile leaves id output unchanged");
        check(!slot<StringOutSlot>(api, 0x34)(api, out) && out == "unchanged", "absent profile leaves nick output unchanged");
        check(!slot<StringOutSlot>(api, 0x58)(api, out) && out == "unchanged", "false pending leaves output unchanged");
        const std::string embedded("a\0b", 3);
        slot<PendingSlot>(api, 0x5C)(api, true, embedded);
        check(slot<StringOutSlot>(api, 0x58)(api, out) && out == embedded, "pending supports embedded NUL");
        out.clear();
        check(owner.hasPendingTransition(out) && out == embedded, "reading pending does not clear it");
        const std::string large(4096, 'L');
        owner.setPendingTransition(true, large);
        check(owner.hasPendingTransition(out) && out == large, "pending supports long text");
        owner.setPendingTransition(false, embedded);
        out = "preserved";
        check(!owner.hasPendingTransition(out) && out == "preserved"
            && ProfileManagerFixture::pendingText(owner) == embedded, "false setter still stores full text");
        const std::vector<std::string> suggestions = {"one", embedded, large};
        ProfileManagerFixture::suggestions(owner, suggestions);
        std::vector<std::string> got = {"old"};
        check(slot<StringsOutSlot>(api, 0x50)(api, got) && got == suggestions, "typed suggestions copy native string vector");
        got[0] = "changed";
        check(owner.getNickSuggestions(got) && got == suggestions, "suggestion result does not alias owner");
        ProfileManagerFixture::suggestions(owner, {});
        check(owner.getNickSuggestions(got) && got.empty(), "empty suggestions replace prior output");

        std::string error = "before";
        calls.clear(); registry.hasHost = false;
        check(!owner.validateProfileName("desired", error) && error == "before"
            && calls == std::vector<std::string>{"lookup"}, "missing validator host preserves output");
        registry.hasHost = true; calls.clear();
        check(!owner.validateProfileName("desired", error) && error == "observed refusal"
            && calls == std::vector<std::string>({"lookup", "validate"}), "validator forwards typed host call and return");

        OnlineProfile* seeded = ProfileManagerFixture::seed(owner);
        check(slot<RecordSlot>(api, 0x2C)(api) == seeded, "profile getter returns exact current pointer");
        check(owner.getProfileId(id) && id == 12345 && owner.getUniqueNick(out)
            && out == seeded->uniqueNick, "profile scalar/string getters read actual record");
        ProfileManagerFixture::setConnection(owner, reinterpret_cast<void*>(0x1234));
        for (GPResult result : {GP_NO_ERROR, GP_NETWORK_ERROR}) {
            connectedResult = result;
            for (GPEnum state : {GP_CONNECTED, GP_NOT_CONNECTED}) {
                connectedValue = state; calls.clear();
                check(owner.isLoggedIn() == (result == GP_NO_ERROR && state != GP_NOT_CONNECTED)
                    && calls == std::vector<std::string>{"isConnected"}, "status combines GP result and connected value");
            }
        }
        connectedResult = GP_NO_ERROR; connectedValue = GP_CONNECTED;
        for (GPResult result : {GP_NO_ERROR, GP_NETWORK_ERROR}) {
            processResult = result; calls.clear();
            check(owner.update() == (result == GP_NO_ERROR)
                && calls == std::vector<std::string>({"isConnected", "process"}), "update preserves status-before-process and result");
        }
        resetWatched(); ProfileManagerFixture::watchRecord(owner);
        calls.clear(); owner.logout();
        check(calls == std::vector<std::string>({"disconnect", "destroy", "lookup", "selected0"}), "logout GP and Swiff ordering");
        check(freedCount == 6 && freed[0] == 2 && freed[1] == 3 && freed[2] == 4
            && freed[3] == 5 && freed[4] == 6 && freed[5] == 1, "logout destroys complete record in member order");
        resetWatched(); calls.clear(); owner.logout();
        check(calls == std::vector<std::string>({"lookup", "selected0"}), "repeated logout still notifies host without GP");
    }

    {
        VirtualStatus owner;
        expectedConnection = ProfileManagerFixture::connection(owner);
        processResult = GP_NO_ERROR;
        calls.clear();
        check(!owner.update() && calls == std::vector<std::string>{"virtualStatus"}, "update uses virtual status and short circuits");
        owner.answer = true; calls.clear();
        check(owner.update() && calls == std::vector<std::string>({"virtualStatus", "process"}), "update respects overridden status despite absent record");
    }
    {
        SelectingOwner owner;
        calls.clear();
        check(!owner.selectProfile("selected name") && calls.empty(), "selectProfile skips login without a current record");
        ProfileManagerFixture::seed(owner);
        calls.clear();
        check(!owner.selectProfile("selected name") && calls == std::vector<std::string>{"virtualLogin"},
            "selectProfile forwards virtual login false");
        owner.getCurrentProfile()->password.assign(82, 'p');
        owner.answer = true; calls.clear();
        check(owner.selectProfile("selected name") && calls == std::vector<std::string>{"virtualLogin"},
            "selectProfile forwards virtual login true");
    }
    // the GameSpy presence login, PE 4A6BD0
    {
        LoginRegistry registry;
        classManager = &registry;
        {
            ProfileManager owner;
            expectedConnection = ProfileManagerFixture::connection(owner);
            observedOwner = &owner;
            loginHost.posted.clear(); calls.clear();
            check(!owner.login("nick", "") && calls == std::vector<std::string>({"lookup", "post"}),
                  "an empty password fails before any SDK call");
            check(loginHost.posted.size() == 1 && loginHost.posted[0].type == 3
                  && loginHost.posted[0].code == 3 && loginHost.posted[0].text == "nopassword",
                  "the empty-password message is the nopassword localisation id");
            loginHost.posted.clear(); calls.clear();
            check(!owner.login("", "secret") && calls == std::vector<std::string>({"lookup", "post"}),
                  "an empty nick fails before any SDK call");
            check(loginHost.posted.size() == 1 && loginHost.posted[0].text == "noname",
                  "the empty-nick message is the noname localisation id");
            registry.hasHost = false;
            loginHost.posted.clear(); calls.clear();
            check(!owner.login("nick", "") && loginHost.posted.empty() && calls == std::vector<std::string>{"lookup"},
                  "an absent host swallows the refusal message");
            registry.hasHost = true;
        }
        {   // the availability check refuses: logout, the poll, then serveroffline.
            ProfileManager owner;
            expectedConnection = ProfileManagerFixture::connection(owner);
            observedOwner = &owner;
            availableAnswer = GSIACUnavailable; availableWaits = 2;
            loginHost.posted.clear(); calls.clear();
            check(!owner.login("nick", "secret"), "an unavailable backend refuses the login");
            check(calls == std::vector<std::string>({"lookup", "selected", "startAvailable", "think",
                                                     "msleep", "think", "msleep", "think", "lookup", "post"}),
                  "logout runs first, then the availability poll waits twice before answering");
            check(lastGameName == "battlefield2", "the availability check names the shipped game");
            check(loginHost.posted.size() == 1 && loginHost.posted[0].type == 3
                  && loginHost.posted[0].text == "serveroffline", "an unavailable backend posts serveroffline");
        }
        {   // gpInitialize refuses.
            ProfileManager owner;
            expectedConnection = ProfileManagerFixture::connection(owner);
            observedOwner = &owner;
            availableAnswer = GSIACAvailable; availableWaits = 0;
            initializeResult = GP_PARAMETER_ERROR;
            loginHost.posted.clear(); calls.clear();
            check(!owner.login("nick", "secret") && loginHost.posted.size() == 1
                  && loginHost.posted[0].text == "serveroffline", "a refused gpInitialize posts serveroffline");
            check(lastProductId == 10493 && lastNamespaceId == 12 && lastPartnerId == GP_PARTNERID_GAMESPY,
                  "gpInitialize carries the shipped product and namespace ids");
            initializeResult = GP_NO_ERROR;
        }
        {   // the connect refuses: the connection is disconnected (kept) and the SDK text is translated.
            ProfileManager owner;
            expectedConnection = ProfileManagerFixture::connection(owner);
            observedOwner = &owner;
            connectResult = GP_NETWORK_ERROR;
            sdkErrorText = "Could not resolve connection manager host name.";
            loginHost.posted.clear(); calls.clear();
            check(!owner.login("thenick", "thesecret"), "a refused connect fails the login");
            check(calls == std::vector<std::string>({"lookup", "selected", "startAvailable", "think",
                                                     "initialize", "setCallback", "connect", "disconnect",
                                                     "errorString", "lookup", "post"}),
                  "a refused connect disconnects, keeping the connection for its error text");
            check(lastConnectNick == "thenick" && lastConnectPassword == "thesecret",
                  "connect carries the unique nick and password in the shipped order");
            check(lastCallbackFunc == GP_ERROR && lastCallbackParam == &owner,
                  "the error callback is installed on GP_ERROR with the owner as its parameter");
            check(loginHost.posted.size() == 1 && loginHost.posted[0].type == 3
                  && loginHost.posted[0].text == "connectionfailed",
                  "the connection-manager error becomes the connectionfailed localisation id");
            sdkErrorText = "";
            loginHost.posted.clear();
            check(!owner.login("thenick", "thesecret") && loginHost.posted.size() == 1
                  && loginHost.posted[0].type == 1 && loginHost.posted[0].text == "general error",
                  "an empty SDK error becomes a verbatim general error message");
            connectResult = GP_NO_ERROR;
        }
        {   // the success path.
            ProfileManager owner;
            expectedConnection = ProfileManagerFixture::connection(owner);
            observedOwner = &owner;
            connectProfileId = 4242; checkUserProfileId = 77;
            getInfoEmail = "player@example.invalid";
            loginTicketText = "TICKET-0123456789ABCD";
            offeredNicks.clear(); offeredUniqueNicks.clear();
            offeredNicks.push_back("first"); offeredUniqueNicks.push_back("firstunique");
            offeredNicks.push_back("skipped"); offeredUniqueNicks.push_back("");
            loginHost.posted.clear(); calls.clear();
            check(owner.login("thenick", "thesecret"), "a complete exchange logs the account in");
            check(calls == std::vector<std::string>({"lookup", "selected", "startAvailable", "think",
                                                     "initialize", "setCallback", "connect", "getInfo",
                                                     "loginTicket", "getUserNicks", "checkUser",
                                                     "lookup", "selected"}),
                  "the success path runs the shipped call sequence exactly once each");
            check(lastGetInfoProfile == 4242, "getInfo asks about the profile the connect returned");
            OnlineProfile* record = owner.getCurrentProfile();
            check(record != nullptr, "a successful login leaves a current record");
            check(record->email == "player@example.invalid" && record->password == "thesecret"
                  && record->profileId == 4242 && record->uniqueNick == "thenick",
                  "the record carries the fetched e-mail, the password, the id and the nick");
            check(record->profiles.size() == 1 && record->profiles[0].nick == "first"
                  && record->profiles[0].uniqueNick == "firstunique" && record->profiles[0].profileId == 77,
                  "an empty uniquenick is skipped and the rest are resolved to profile ids");
            check(lastCheckNick == "first", "checkUser resolves by the entry's nick");
            check(loginHost.selected == 4242, "the front end is told which player id is selected");
            check(AntiHacks::instance()->m_gsAuth == "TICKET-0123456789ABCD",
                  "the GP login ticket is stamped as the anti-cheat auth token");
            check(AntiHacks::instance()->m_playerNameHash != 18193,
                  "the unique nick replaces the anti-cheat name hash default");
            loginTicketResult = GP_NETWORK_ERROR;
            AntiHacks::instance()->addGSAuth(std::string("untouched"));
            calls.clear();
            check(owner.login("thenick", "thesecret") && AntiHacks::instance()->m_gsAuth == "untouched",
                  "a ticketless session leaves the previous auth token alone");
            loginTicketResult = GP_NO_ERROR;
        }
        classManager = nullptr;
    }

    resetWatched(); calls.clear();
    {
        ProfileManager owner;
        ProfileManagerFixture::seed(owner);
        ProfileManagerFixture::watchRecord(owner);
        ProfileManagerFixture::setConnection(owner, reinterpret_cast<void*>(0x1234));
        owner.setPendingTransition(true, std::string(400, 'x'));
        ProfileManagerFixture::suggestions(owner, {std::string(400, 'y')});
    }
    check(calls.empty() && countFreed(1) == 1, "destructor deletes current record without GP or Swiff calls");
    for (unsigned i = 2; i != 7; ++i) check(countFreed(i) == 1, "record owned string is released exactly once");
    resetWatched(); classManager = nullptr;
    std::printf("ProfileManager strict owner fixture: %u/%u checks passed; no network\n", checks - failures, checks);
    return failures ? 1 : 0;
}
