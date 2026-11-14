// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: menu-interact-runtime]
// Native (macOS/POSIX) fixture for the c-menu-interact recoveries:
//   1. StringTokenizer (BF2.exe 0x59E1B0/0x59E1F0) on the "WxH@RHz" strings the video page sends,
//      the token split SwiffHost::parseResolution (0x489210) performs.
//   2. ProfileManager::createProfile / deleteProfile / requestNickSuggestions (0x4A6EB0 / 0x4A54B0 /
//      0x4A5BD0) against substituted GP/availability entry points: no network.
//   3. UnixFileSystem::deleteFile's client directory arm (the removal behind
//      LocalProfileManager::deleteProfile, 0x4AAF70) on a temporary @HOME@/Profiles tree.
// Build and run: Code/Tests/menu_interact_runtime.sh <out-dir>.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>

#include "../BF2/Core/StringTokenizer.h"
#include "../BF2/Game/ProfileManager.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/External/GameSpy/gp/gp.h"
#include "../BF2/External/GameSpy/common/gsAvailable.h"
#include "../BF2/External/GameSpy/common/gsPlatformUtil.h"
#include "../BF2/Game/Persistence/AntiHacks.h"
#include "../BF2/Game/Persistence/HTTPRequest.h"
#include "../BF2/IO/File/UnixFileSystem.h"
#include "SwiffHostStandIn.h"

using namespace dice::hfe;

static unsigned checks, failures;
static void check(bool pass, const char* label)
{
    ++checks;
    if (!pass) { ++failures; std::printf("FAIL %s\n", label); }
}

// ---------------------------------------------------------------------------------------------
// 1. parseResolution's tokens
static void splitResolution(const std::string& text, int* w, int* h, int* hz)
{
    // The three nextToken calls and the empty-token rule of SwiffHost::parseResolution.
    StringTokenizer tokens(text, std::string("x@Hz"));
    std::string a, b, c;
    tokens.nextToken(a); tokens.nextToken(b); tokens.nextToken(c);
    *w = a.size() ? std::atoi(a.c_str()) : 0;
    *h = b.size() ? std::atoi(b.c_str()) : 0;
    *hz = c.size() ? std::atoi(c.c_str()) : 0;
}

static void tokenizerTests()
{
    struct Row { const char* text; int w, h, hz; } rows[] = {
        { "800x600@60Hz",    800,  600, 60 },   // VideoSettings' default (VideoSettings.cpp:24)
        { "1024x768@75Hz",  1024,  768, 75 },
        { "1600x1200@85Hz", 1600, 1200, 85 },
        { "1280x1024",      1280, 1024,  0 },   // no refresh part: the third token is empty
        { "",                  0,    0,  0 },
        { "x@Hz",              0,    0,  0 },   // delimiters only
    };
    for (const Row& r : rows) {
        int w = -1, h = -1, hz = -1;
        splitResolution(r.text, &w, &h, &hz);
        char label[96];
        std::snprintf(label, sizeof(label), "parseResolution tokens of \"%s\"", r.text);
        check(w == r.w && h == r.h && hz == r.hz, label);
    }
    StringTokenizer t(std::string("a,,b"), std::string(","));
    std::string token("stale");
    check(t.nextToken(token) && token == "a", "first token");
    check(t.nextToken(token) && token == "b", "runs of delimiters are skipped");
    check(!t.nextToken(token) && token.empty(), "end of text answers false with a cleared token");
}

// ---------------------------------------------------------------------------------------------
// 2. ProfileManager against substituted GP entry points
static std::vector<std::string> calls;
static GPResult initializeResult = GP_NO_ERROR, newUserResult = GP_NO_ERROR, deleteResult = GP_NO_ERROR;
static int newUserProfileId = 0, loginProfileId = 0;
static std::string lastNick, lastUniqueNick, lastEmail, lastPassword, lastConnectNick, lastSuggestNick;
static const OnlineProfileEntry* lastDeleteParam;
static std::vector<std::string> offeredSuggestions;
static const char* sdkError = "sdk said no";
static std::string postedUrl;

static void call(const char* name) { calls.push_back(name); }
static bool called(const char* name)
{
    for (const std::string& c : calls) if (c == name) return true;
    return false;
}

extern "C" {
GPResult gpInitialize(GPConnection* connection, int, int, int)
{ call("initialize"); if (initializeResult == GP_NO_ERROR) *connection = reinterpret_cast<void*>(0x1234); return initializeResult; }
GPResult gpSetCallback(GPConnection*, GPEnum, GPCallback, void*) { call("setCallback"); return GP_NO_ERROR; }
void gpDisconnect(GPConnection*) { call("disconnect"); }
void gpDestroy(GPConnection* connection) { call("destroy"); *connection = nullptr; }
GPResult gpIsConnected(GPConnection*, GPEnum* connected) { *connected = GP_CONNECTED; return GP_NO_ERROR; }
GPResult gpProcess(GPConnection*) { return GP_NO_ERROR; }
GPResult gpConnectNewUserA(GPConnection* connection, const char* nick, const char* uniquenick, const char* email,
                           const char* password, const char* cdkey, GPEnum firewall, GPEnum blocking,
                           GPCallback callback, void* param)
{
    call("connectNewUser");
    lastNick = nick; lastUniqueNick = uniquenick; lastEmail = email; lastPassword = password;
    check(!cdkey && firewall == GP_NO_FIREWALL && blocking == GP_BLOCKING, "new profile: no cd key, no firewall, blocking");
    if (newUserResult == GP_NO_ERROR) {
        GPConnectResponseArg arg; std::memset(&arg, 0, sizeof(arg));
        arg.profile = newUserProfileId;
        callback(connection, &arg, param);
    }
    return newUserResult;
}
GPResult gpConnectA(GPConnection*, const char*, const char*, const char*, GPEnum, GPEnum, GPCallback, void*)
{ call("connect"); return GP_PARAMETER_ERROR; }
GPResult gpRegisterUniqueNickA(GPConnection*, const char*, const char*, GPEnum, GPCallback, void*)
{ call("registerUniqueNick"); return GP_PARAMETER_ERROR; }
GPResult gpConnectUniqueNickA(GPConnection* connection, const char* uniquenick, const char*, GPEnum, GPEnum,
                              GPCallback callback, void* param)
{
    call("connectUniqueNick");
    lastConnectNick = uniquenick;
    *connection = reinterpret_cast<void*>(0x5678);
    GPConnectResponseArg arg; std::memset(&arg, 0, sizeof(arg));
    arg.profile = loginProfileId;
    callback(connection, &arg, param);
    return GP_NO_ERROR;
}
GPResult gpGetInfo(GPConnection*, GPProfile, GPEnum, GPEnum, GPCallback callback, void* param)
{
    call("getInfo");
    GPGetInfoResponseArg arg; std::memset(&arg, 0, sizeof(arg));
    std::strcpy(arg.email, "login@example.invalid");
    callback(nullptr, &arg, param);
    return GP_NO_ERROR;
}
GPResult gpGetUserNicksA(GPConnection*, const char*, const char*, GPEnum, GPCallback callback, void* param)
{
    call("getUserNicks");
    GPGetUserNicksResponseArg arg; std::memset(&arg, 0, sizeof(arg));
    callback(nullptr, &arg, param);    // no nicks: the profile list stays empty
    return GP_NO_ERROR;
}
GPResult gpCheckUserA(GPConnection*, const char*, const char*, const char*, GPEnum, GPCallback, void*)
{ call("checkUser"); return GP_NO_ERROR; }
GPResult gpGetErrorStringA(GPConnection*, char errorString[GP_ERROR_STRING_LEN])
{ call("errorString"); std::strcpy(errorString, sdkError); return GP_NO_ERROR; }
GPResult gpGetLoginTicket(GPConnection*, char ticket[GP_LOGIN_TICKET_LEN]) { call("loginTicket"); ticket[0] = 0; return GP_PARAMETER_ERROR; }
GPResult gpSetInfos(GPConnection*, GPEnum, const char*) { return GP_NO_ERROR; }
GPResult gpSetInfoi(GPConnection*, GPEnum, int) { return GP_NO_ERROR; }
GPResult gpSetInfod(GPConnection*, GPEnum, int, int, int) { return GP_NO_ERROR; }
GPResult gpDeleteProfile(GPConnection*, GPCallback callback, void* param)
{
    call("deleteProfile");
    check(callback != nullptr, "delete passes a callback (the shipped no-op)");
    lastDeleteParam = static_cast<const OnlineProfileEntry*>(param);
    return deleteResult;
}
GPResult gpSuggestUniqueNickA(GPConnection* connection, const char* desirednick, GPEnum blocking,
                              GPCallback callback, void* param)
{
    call("suggest");
    lastSuggestNick = desirednick;
    check(blocking == GP_BLOCKING && *connection == reinterpret_cast<void*>(0x1234), "suggest blocks on the private connection");
    std::vector<char*> nicks;
    for (std::string& s : offeredSuggestions) nicks.push_back(&s[0]);
    GPSuggestUniqueNickResponseArg arg; std::memset(&arg, 0, sizeof(arg));
    arg.numSuggestedNicks = static_cast<int>(nicks.size());
    arg.suggestedNicks = nicks.empty() ? nullptr : &nicks[0];
    callback(connection, &arg, param);
    return GP_NO_ERROR;
}
void GSIStartAvailableCheckA(const char*) { call("startAvailable"); }
GSIACResult GSIAvailableCheckThink(void) { return GSIACAvailable; }
void msleep(gsi_time) {}
}

// The stats post and the anti-cheat stamp setCurrentProfile reaches, observed only.
namespace dice { namespace hfe {
ClassManager* classManager = nullptr;   // defined by Globals.cpp in the images
namespace macos {
// UnixFileSystem's client home; the fixture passes absolute paths and never asks for it.
const std::string& clientDataDirectory() { std::abort(); }
}
HTTPRequest::HTTPRequest() {}
HTTPRequest::~HTTPRequest() {}
void HTTPRequest::onGetComplete(const std::string&) {}
void HTTPRequest::onGetFailure(const std::string&) {}
void HTTPRequest::onGetFileComplete() {}
void HTTPRequest::onPostComplete(const std::string&) {}
void HTTPRequest::onPostFailure(const std::string&) {}
void HTTPRequest::onProgress(float) {}
std::string HTTPRequest::getGameSpyUrl() const { return "http://stats.invalid/"; }
bool HTTPRequest::post(const std::string& url, const std::string&, bool) { call("statsPost"); postedUrl = url; return true; }
AntiHacks* AntiHacks::instance() { static AntiHacks* one = static_cast<AntiHacks*>(std::malloc(64)); return one; }
void AntiHacks::addPlayerName(const std::string&) { call("antiHacksName"); }
void AntiHacks::addGSAuth(const std::string&) { call("antiHacksAuth"); }

struct ProfileManagerFixture {
    static OnlineProfile* seed(ProfileManager& p, int profiles)
    {
        p.m_currentProfile = new OnlineProfile;
        p.m_currentProfile->email = "owner@example.invalid";
        p.m_currentProfile->password = "secret";
        p.m_currentProfile->uniqueNick = "oldnick";
        p.m_currentProfile->profileId = 10;
        for (int i = 0; i < profiles; ++i) {
            OnlineProfileEntry e;
            e.profileId = 10 + i;           // entry 0 is the current profile
            e.nick = i ? "othernick" : "oldnick";
            e.uniqueNick = i ? "otherunique" : "oldnick";
            p.m_currentProfile->profiles.push_back(e);
        }
        p.m_connection = reinterpret_cast<void*>(0x1234);
        return p.m_currentProfile;
    }
    static std::vector<std::string>& suggestions(ProfileManager& p) { return p.m_nickSuggestions; }
};
} }

struct Host : SwiffHostStandIn {
    std::vector<SwiffHostMessage> posted;
    std::vector<std::string> transitions;
    int selected = -1;
    bool refuseName = false;
    bool postMessage(const SwiffHostMessage& m) override { posted.push_back(m); return true; }
    void setSelectedPlayerId(int id) override { selected = id; }
    bool validateProfileName(const std::string&, std::string& error) override
    { if (refuseName) { error = "badname"; return false; } return true; }
    void handleProfileTransition(const std::string& text) override { transitions.push_back(text); }
};
static Host host;
struct Registry : ClassManager {
    IBase* getSingleton(const std::string& name) override
    { return name == "SwiffHost" ? static_cast<IBase*>(&host) : nullptr; }
};

static void reset()
{
    calls.clear(); host.posted.clear(); host.transitions.clear(); host.selected = -1; host.refuseName = false;
    initializeResult = newUserResult = deleteResult = GP_NO_ERROR;
}

static void profileTests()
{
    Registry registry; classManager = &registry;
    {   // createProfile
        ProfileManager pm;
        reset();
        check(!pm.createProfile("newnick", "CH") && calls.empty(), "create refuses without a current profile");
        ProfileManagerFixture::seed(pm, 1);
        reset(); host.refuseName = true;
        check(!pm.createProfile("bad", "CH"), "create refuses a rejected name");
        check(host.posted.size() == 1 && host.posted[0].type == 3 && host.posted[0].code == 3 && host.posted[0].text == "badname"
              && !called("initialize"), "rejected name posts {3, error, 3} and touches no connection");

        reset(); newUserProfileId = 77;
        check(pm.createProfile("newnick", "CH"), "create succeeds");
        check(lastNick == "newnick" && lastUniqueNick == "newnick" && lastEmail == "owner@example.invalid"
              && lastPassword == "secret", "gpConnectNewUser gets nick twice and the copied account e-mail/password");
        check(called("disconnect") && called("destroy") && called("initialize") && called("setCallback"),
              "the old session ends, a new connection is initialised");
        check(!called("startAvailable"), "no availability check on this path");
        check(postedUrl.find("pid=77") != std::string::npos && postedUrl.find("cid=CH") != std::string::npos
              && postedUrl.find("nick=newnick") != std::string::npos, "stats player created for the new profile");
        OnlineProfile* now = pm.getCurrentProfile();
        check(now && now->profileId == 77 && now->uniqueNick == "newnick" && now->email == "owner@example.invalid"
              && now->password == "secret", "the new profile is current");
        check(host.selected == 77, "the host's selected player id is the new profile");

        reset(); newUserResult = GP_PARAMETER_ERROR; loginProfileId = 10;
        check(!pm.createProfile("again", "CH"), "create reports a failed gpConnectNewUser");
        check(host.posted.size() == 1 && host.posted[0].type == 1 && host.posted[0].text == sdkError,
              "the SDK error text is posted untranslated");
        check(lastConnectNick == "newnick" && pm.getCurrentProfile() && pm.getCurrentProfile()->uniqueNick == "newnick",
              "the previous unique nick logs in again");
        check(host.transitions.empty(), "a successful restore posts no transition");
    }
    {   // deleteProfile
        ProfileManager pm;
        reset();
        check(!pm.deleteProfile() && calls.empty(), "delete refuses without a current profile");
        ProfileManagerFixture::seed(pm, 1);
        reset();
        check(!pm.deleteProfile() && !called("deleteProfile"), "delete refuses the last profile");
        check(host.posted.size() == 1 && host.posted[0].type == 1
              && host.posted[0].text == "Unable to delete profile, you must have at least 1 profile",
              "the at-least-one text goes out untranslated (type 1)");
        ProfileManager pm2;
        ProfileManagerFixture::seed(pm2, 2);
        reset(); deleteResult = GP_PARAMETER_ERROR;
        check(!pm2.deleteProfile() && host.posted.size() == 1 && host.posted[0].text == sdkError && !called("disconnect"),
              "a refused delete reports the SDK error and keeps the session");
        reset(); loginProfileId = 11;
        check(pm2.deleteProfile(), "delete succeeds");
        check(lastConnectNick == "othernick", "the first other profile logs in by its nick (entry +4)");
        check(pm2.getCurrentProfile() && pm2.getCurrentProfile()->profileId == 11, "the other profile is current");
    }
    {   // requestNickSuggestions
        ProfileManager pm;
        ProfileManagerFixture::suggestions(pm).push_back("stale");
        reset(); offeredSuggestions = { "nick1", "nick2" };
        check(pm.requestNickSuggestions("nick"), "suggestions request answers true");
        std::vector<std::string> out;
        pm.getNickSuggestions(out);
        check(out.size() == 2 && out[0] == "nick1" && out[1] == "nick2" && lastSuggestNick == "nick",
              "the list is replaced by the suggestions");
        check(called("destroy"), "the private connection is destroyed");
        reset(); initializeResult = GP_PARAMETER_ERROR;
        check(!pm.requestNickSuggestions("nick") && !called("suggest"), "a failed initialise answers false");
        pm.getNickSuggestions(out);
        check(out.empty() && host.posted.size() == 1, "the list is emptied first; the error is posted");
    }
    classManager = nullptr;
}

// ---------------------------------------------------------------------------------------------
// 3. the profile directory removal on a temporary @HOME@
static void deleteDirectoryTests()
{
    char base[] = "/tmp/bf2-menu-interact-XXXXXX";
    if (!mkdtemp(base)) { check(false, "temporary home"); return; }
    const std::string home(base);
    const std::string profiles = home + "/profiles";
    const std::string dir = profiles + "/0002";
    mkdir(profiles.c_str(), 0700);
    mkdir(dir.c_str(), 0700);
    mkdir((dir + "/sub").c_str(), 0700);
    for (const char* f : { "/profile.con", "/sub/demo.con" }) {
        FILE* fp = std::fopen((dir + f).c_str(), "w"); if (fp) { std::fputs("x", fp); std::fclose(fp); }
    }
    io::UnixFileSystem fs;
    // LocalProfileManager::deleteProfile's spelling: <home>\Profiles\<name>. fixUnixPath turns the
    // backslashes round and lowercases (the temporary path is lowercase).
    check(fs.deleteFile(home + "\\Profiles\\0002"), "deleteFile removes a profile directory tree");
    struct stat st;
    check(stat(dir.c_str(), &st) != 0 && stat(profiles.c_str(), &st) == 0, "only the profile directory is gone");
    check(!fs.deleteFile(home + "\\Profiles\\0009"), "a missing directory answers false");
    rmdir(profiles.c_str());
    rmdir(home.c_str());
}

int main()
{
    tokenizerTests();
    profileTests();
    deleteDirectoryTests();
    std::printf("menu_interact_runtime: %u/%u passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
