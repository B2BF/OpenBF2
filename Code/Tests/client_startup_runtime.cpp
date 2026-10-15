// [TEMP] [CHOSEN, not recovered]
// Tests client parsing, crash-directory setup, movie queues, and Flash bridges with recording collaborators.
// MenuTeamManager is real; script requests are recorded without interpretation. No listener or server connection is started.
#include "../BF2/Game/Main/BF2Engine.h"
#include "../BF2/Game/Main/BF2EngineSetup.h"
#include "../BF2/Game/Main/Game.h"
#include "../BF2/Game/Main/IFSMoviePlayer.h"
#include "../BF2/Core/System.h"
#include "../BF2/Core/SettingsRepostitory.h"
#include "../BF2/Core/ClassManager.h"
#include "../BF2/Core/ConsoleObject.h"
#include "../BF2/Core/ConsoleObjectBase.h"
#include "../BF2/Core/ConsoleObjects.h"
#include "../BF2/Core/Ids.h"
#include "../BF2/Core/EventManager.h"
#include "../BF2/Core/StringUtil.h"
#include "../BF2/Menu/IMenuLogic.h"
#include "../BF2/Menu/ISwiffHost.h"
#include "../BF2/Menu/ISwiffPlayer.h"
#include "../BF2/Menu/ISwiffRender.h"
#include "../BF2/Menu/MenuTeamManager.h"
#include "../BF2/Menu/SwiffHost.h"
#include "../BF2/Menu/MainMenuLogic.h"
#include "../BF2/Menu/BetweenRoundsLogic.h"
#include "../BF2/Menu/IServerBrowser.h"
#include "../BF2/Menu/IServerBrowserManager.h"
#include "../BF2/Menu/ServerBrowser.h"
#include "../BF2/Menu/ServerBrowserManager.h"
#include "../BF2/External/GameSpy/serverbrowsing/sb_serverbrowsing.h"
#include "../BF2/External/GameSpy/serverbrowsing/sb_internal.h"   // [TEMP] a synthetic SBServer for the record fill
#include "../BF2/Game/LocalProfileManager.h"
#include "../BF2/Game/ProfileManager.h"
#include "../BF2/Game/GlobalSettings.h"
#include "../BF2/Game/ControlSettings.h"
#include "../BF2/Game/ModInfo.h"
#include "../BF2/Game/Utility/BuildNrUtil.h"
#include "../BF2/Game/Main/BF2ClientSeams.h"
#include "../BF2/IO/Input/Input.h"
#include "../BF2/IO/Input/ControlMapManager.h"
#include "../BF2/IO/Input/GameInput.h"
#include "../BF2/IO/Input/FalconDevice.h"
#include "../BF2/IO/Console/Console.h"
#include "../BF2/IO/File/FileManager.h"
#include <windows.h>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <list>
#include <string>
#include <vector>

namespace dice { namespace hfe {
const wchar_t* getCrashDumpDirectory();
// Same recovered static interface seam as BF2Engine.cpp; no shared header exists yet.
class ISettingsRepostitory { public: static void createSingleton(); };
extern ClassManager* classManager;                  // Core/Globals.cpp
extern BF2Engine*    bf2Engine;                     // Core/Globals.cpp; the engine constructor publishes itself
std::list<ConsoleObject*>& gameConsoleObjects();    // Game/Main/GameConsole.cpp
void registerSwiffHostConsoleObjects();             // Menu/SwiffHostConsole.cpp
void unregisterSwiffHostConsoleObjects();
namespace io { extern IBase* controlMapManager; extern IBase* inputDeviceManager; }   // Core/Globals.cpp
// The menu key ring (Game/Main/BF2.cpp) MainMenuLogic::handleInput drains.
extern char menuKeyBuffer[256];
extern int  menuKeyCount;
extern int  menuKeyRead;
extern int  menuKeyWrite;
// [TEMP] BF2Engine::init creates the engine's BF2EngineSetup; the fixture never runs init, so it
// seats one itself for the menu bridge's initSwiff / shutdownSwiff.
struct BF2EngineFixture
{
    static void createSetup(BF2Engine& engine) { if (!engine.bf2EngineSetup) engine.bf2EngineSetup = new BF2EngineSetup(&engine); }
};
// [TEMP] reads the ServerBrowser's private fields.
struct ServerBrowserFixture
{
    static _ServerBrowser* handle(ui::IServerBrowser* list) { return static_cast<ServerBrowser*>(list)->m_browser; }
    static bool keepsUnanswered(ui::IServerBrowser* list) { return static_cast<ServerBrowser*>(list)->m_keepUnansweredServers; }
    static bool suppressed(ui::IServerBrowser* list) { return static_cast<ServerBrowser*>(list)->m_suppressCallbacks; }
    static bool updateStarted(ui::IServerBrowser* list) { return static_cast<ServerBrowser*>(list)->m_updateStarted; }
    static int  maxPing(ui::IServerBrowser* list) { return static_cast<ServerBrowser*>(list)->m_maxPing; }
    static const std::string& sortKey(ui::IServerBrowser* list) { return static_cast<ServerBrowser*>(list)->m_sortKey; }
};
} }

namespace {
using namespace dice::hfe;

// [TEMP] the plugin movie player, observed: every load is recorded, the first `g_rejectLoads`
// of them refused, frames counted, and the last release destroys the object as the plugin's does.
unsigned g_playerCreates = 0, g_playerReleases = 0, g_rejectLoads = 0, g_frames = 0;
std::vector<std::string> g_loads;

struct MoviePlayerObserver : IFSMoviePlayer
{
    int refs;
    MoviePlayerObserver() : refs(1) {}
    virtual void  addRef() { ++refs; }
    virtual int   getRef() { return refs; }
    virtual int   release() { const int left = --refs; if (!left) { ++g_playerReleases; delete this; } return left; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual bool  load(std::string path)
    {
        g_loads.push_back(path);
        if (g_rejectLoads) { --g_rejectLoads; return false; }
        return true;
    }
    virtual void  releaseAll() {}
    virtual void  drawFrame() { ++g_frames; }
    virtual bool  finished() { return false; }
};

IBase* createMoviePlayer(unsigned int, IBase*) { ++g_playerCreates; return new MoviePlayerObserver(); }

// [TEMP] a class server publishing only the movie player, under the name checkMovie asks for.
struct MovieClassServer : IClassServer
{
    virtual void  addRef() {}
    virtual int   getRef() { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual void  getClassList(std::vector<ClassRegInfo>& out)
    {
        ClassRegInfo info;
        info.classId = geom::CID_FSMoviePlayer;
        info.name    = "dice.hfe.geom.FSMoviePlayer";
        info.create  = &createMoviePlayer;
        out.push_back(info);
    }
    virtual void  getSingletonList(std::vector<SingletonRegInfo>&) {}
};

// the menu's collaborators, observed
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: client-menu-update-witness]
// Stop inside renderMenu's first virtual call, after observing its real global write.
// No init means the inherited destructor's deactivate posts no event. It still
// activates the mouse, so run this witness after the bridge's lifecycle counters.
struct MenuUpdateReached {};
struct TrapMenuLogic : MainMenuLogic
{
    int seenMode = 0;
    double seenDelta = 0;
    unsigned updates = 0;
    void update(double delta) override
    {
        seenMode = io::g_falconMouseMode;
        seenDelta = delta;
        ++updates;
        throw MenuUpdateReached();
    }
};
struct MenuFrameGlobalsRestore
{
    IMenuLogic* previousLogic = menuLogic;
    int previousMode = io::g_falconMouseMode;
    ~MenuFrameGlobalsRestore()
    {
        menuLogic = previousLogic;
        io::g_falconMouseMode = previousMode;
    }
};

struct MouseSample { int x, y, buttons; };
// [TEMP] the SwiffPlayer plugin object: every call the menu bridge makes is recorded.
struct FakeSwiffPlayer : ui::ISwiffPlayer
{
    int refs = 1;
    ui::ISwiffHost* host = 0; ui::ISwiffRender* render = 0; std::string movie, textInput;
    float scaleX = 0, scaleY = 0, lastDt = 0; unsigned updates = 0, displays = 0, loads = 0;
    unsigned legacyOffs = 0, legacyOns = 0;
    std::vector<MouseSample> mouse; std::vector<std::pair<int, int> > keyStates; std::vector<int> chars;
    void  addRef() override { ++refs; }
    int   getRef() override { return refs; }
    int   release() override { return --refs; }
    void* queryInterface(unsigned int) const override { return const_cast<FakeSwiffPlayer*>(this); }
    void  setScale(float x, float y) override { scaleX = x; scaleY = y; }
    bool  loadMovie(const char* name) override { movie = name; ++loads; return true; }
    void  update(float dt) override { ++updates; lastDt = dt; }
    void  display() override { ++displays; }
    bool  isMovieLoaded() override { return !movie.empty(); }
    void  setRender(ui::ISwiffRender* r) override { render = r; }
    ui::ISwiffRender* getRender() override { return render; }
    void  setMousePositionDeferred(int, int) override {}
    void  queueMouseState(int x, int y, int b) override { MouseSample m = { x, y, b }; mouse.push_back(m); }
    void  notifyKey(int, int) override {}
    void  reserved14(int) override {}
    void  reserved15(int) override {}
    void  notifyChar(int c) override { chars.push_back(c); }
    void  notifyKeyState(int code, int down) override { keyStates.push_back(std::make_pair(code, down)); }
    void  resetModifierState() override {}
    void  setTextInput(const char* text) override { textInput = text; }
    void  getTextInput0(std::string& out) override { out = "copied"; }
    void  getTextInput1(std::string& out) override { out = "cut"; }
    void  setSwiffHost(ui::ISwiffHost* h) override { host = h; }
    ui::ISwiffHost* getSwiffHost() override { return host; }
    char  renderCapability0(intptr_t) override { return 1; }
    char  renderCapability1(intptr_t) override { return 1; }
    int   getMovieWidth() override { return 800; }
    int   getMovieHeight() override { return 600; }
    void  renderSetPairState(int, int) override {}
    void  legacyModeOff() override { ++legacyOffs; }
    void  legacyModeOn() override { ++legacyOns; }
    char  getModeFlag() override { return 0; }
    void  setModeFlag(char) override {}
    void  notifyMouse(int, int, int) override {}
    void  processQueuedMouseState() override {}
    void* getBf2VoipClient() override { return 0; }
    void  setVoipUpdateFlag(char) override {}
};
// [TEMP] the SwiffRender plugin object: init / beginFrame / endFrame counted, the flag kept.
struct FakeSwiffRender : ui::ISwiffRender
{
    int refs = 1; unsigned inits = 0, beginFrames = 0, endFrames = 0; char flag = 0;
    void  addRef() override { ++refs; }
    int   getRef() override { return refs; }
    int   release() override { return --refs; }
    void* queryInterface(unsigned int) const override { return const_cast<FakeSwiffRender*>(this); }
    int   init() override { ++inits; return 1; }
    int   shutdown() override { return 1; }
    int   slot6() override { return 0; }
    int   uploadAlpha(unsigned int, unsigned int, const void*) override { return 0; }
    int   slot8(int) override { return 0; }
    int   uploadRgba(unsigned int*) override { return 0; }
    int   loadTexture(const char*, unsigned int*, unsigned int*) override { return 0; }
    int   releaseTexture(int) override { return 0; }
    void  beginDisplay(int, int, int, int, int, float, float, float, float) override {}
    void  endDisplay() override {}
    void  setTransform(const float*) override {}
    void  setCxform(const float*) override {}
    int   recordStat(int) override { return 0; }
    void  drawVertices(const short*, unsigned int) override {}
    void  drawLineStrip(const short*, unsigned int) override {}
    int   fillStyleDisable(int) override { return 0; }
    void  fillStyleColor(int, unsigned int) override {}
    void  setTexture(int, int, int, int, const float*, int) override {}
    void  lineStyleDisable() override {}
    int   lineStyleColor(unsigned int) override { return 0; }
    void  lineStyleWidth(unsigned int) override {}
    void  nullsub25(const void*, int, const void*, const void*, unsigned int) override {}
    void* createFont(const char*, int) override { return 0; }
    void  releaseFontHandle(void*) override {}
    int   fontForward16(void*, int) override { return 0; }
    int   destroyFont(void*) override { return 0; }
    void  drawGlyphBitmap(void*, const float*, unsigned int, int) override {}
    char  fontGetExtent(void*, int, float*, float*) override { return 0; }
    char  fontForward32(void*, int, float, float) override { return 0; }
    char  fontForward40(void*, int, int) override { return 0; }
    char  fontForward44(void*, float, float, float, float) override { return 0; }
    int   fontForward48(void*) override { return 0; }
    int   fontSetSize(void*, float, int) override { return 0; }
    int   fontSetField104(void*, int) override { return 0; }
    void  nullsub38(int) override {}
    void  beginMask() override {}
    void  endMask() override {}
    void  disableMask() override {}
    void  setMovieWidth(unsigned int) override {}
    void  setMovieHeight(int) override {}
    void  setScale(float, float) override {}
    void  setBackgroundColor(unsigned int) override {}
    void  setMouseState(int, int, int) override {}
    int   slot47(int) override { return -1; }
    void  nullsub48(int) override {}
    char  slot49(intptr_t) override { return 1; }
    char  slot50(intptr_t) override { return 1; }
    int   beginFrame() override { ++beginFrames; return 1; }
    int   endFrame() override { ++endFrames; return 1; }
    void* setTextureTechnique(const std::string&, int) override { return 0; }
    int   updateTime(unsigned int, float) override { return 0; }
    void* getTextureRegistry() override { return 0; }
    int   uploadTexture(const std::string&, unsigned int*, unsigned int*) override { return -1; }
    unsigned int* getDriverVersion(unsigned int* out) override { *out = 0; return out; }
    char  isNvidia() override { return 0; }
    char  getFlag() override { return flag; }
    char  setFlag(char v) override { flag = v; return v; }
    char  preloadCredits() override { return 1; }
    int   creditsCount() override { return 0; }
    std::string getCreditsName(unsigned int) override { return std::string(); }
    std::string getRandomCreditsName() override { return std::string(); }
    char  getWasDeviceLost() override { return 0; }
    void  clearWasDeviceLost() override {}
};
// [TEMP] the input side: a mouse whose activation is counted and a keyboard with scripted states.
struct FakeMouse : io::IInputDevice
{
    int refs = 1; unsigned activations = 0, deactivations = 0;
    void  addRef() override { ++refs; }
    int   getRef() override { return refs; }
    int   release() override { return --refs; }
    void* queryInterface(unsigned int) const override { return const_cast<FakeMouse*>(this); }
    io::InputDeviceTypes getType() const override { return io::IDTMouse; }
    bool  setCooperativeMode(io::InputDeviceMode) override { return true; }
    io::InputDeviceMode getCooperativeMode() const override { return io::IDM_NONEXCLUSIVE; }
    bool  activate() override { ++activations; return true; }
    bool  deActivate() override { ++deactivations; return true; }
    void  update(float) override {}
    bool  isActive() const override { return true; }
    bool  getNextInput(io::InputDeviceAxes&, io::InputDeviceButtons&, io::InputDeviceKeys&, bool&) override { return false; }
};
struct FakeKeyboard : io::IKeyboardDevice
{
    int refs = 1; std::vector<int> pressed, released;
    void  addRef() override { ++refs; }
    int   getRef() override { return refs; }
    int   release() override { return --refs; }
    void* queryInterface(unsigned int) const override { return const_cast<FakeKeyboard*>(this); }
    io::InputDeviceTypes getType() const override { return io::IDTKeyboard; }
    bool  setCooperativeMode(io::InputDeviceMode) override { return true; }
    io::InputDeviceMode getCooperativeMode() const override { return io::IDM_NONEXCLUSIVE; }
    bool  activate() override { return true; }
    bool  deActivate() override { return true; }
    void  update(float) override {}
    bool  isActive() const override { return true; }
    bool  getNextInput(io::InputDeviceAxes&, io::InputDeviceButtons&, io::InputDeviceKeys&, bool&) override { return false; }
    bool  has(const std::vector<int>& v, int k) const { for (std::size_t i = 0; i < v.size(); ++i) if (v[i] == k) return true; return false; }
    bool  keyDown(io::InputDeviceKeys k) const override { return has(pressed, k); }
    bool  keyPressed(io::InputDeviceKeys k) const override { return has(pressed, k); }
    bool  keyReleased(io::InputDeviceKeys k) const override { return has(released, k); }
    std::wstring getKeyName(io::InputDeviceKeys) const override { return std::wstring(); }
};
struct FakeInputDeviceManager : io::IInputDeviceManager
{
    int refs = 1; FakeMouse mouse; FakeKeyboard keyboard; unsigned updates = 0; float lastDt = 0;
    void  addRef() override { ++refs; }
    int   getRef() override { return refs; }
    int   release() override { return --refs; }
    void* queryInterface(unsigned int) const override { return const_cast<FakeInputDeviceManager*>(this); }
    bool  initDevices(unsigned int*) override { return true; }
    io::IInputDevice* getInputDevice(io::InputDeviceFlags flags) const override
    {
        if (flags == io::IDFMouse) return const_cast<FakeMouse*>(&mouse);
        if (flags == io::IDFKeyboard) return const_cast<FakeKeyboard*>(&keyboard);
        return 0;
    }
    unsigned int getInputDevices() const override { return 3; }
    bool  activateDevices(unsigned int) override { return true; }
    bool  deActivateDevices(unsigned int) override { return true; }
    void  update(float dt) override { ++updates; lastDt = dt; }
    unsigned int getActiveInputDevices() const override { return 3; }
    void* getAllDevices() override { return 0; }
};
// [TEMP] the console: the menu's .con loads are recorded instead of executed.
struct FixtureConsole : io::OldConsole
{
    std::vector<std::string> files;
    bool run(const std::string& file, const std::string&, const std::string&, const std::string&, const std::string&,
             const std::string&, const std::string&, const std::string&, const std::string&, std::string&) override
    { files.push_back(file); return true; }
    unsigned count(const char* name) const { unsigned n = 0; for (std::size_t i = 0; i < files.size(); ++i) if (files[i] == name) ++n; return n; }
};
// [TEMP] the file system: nothing exists, writes are recorded, fileExistsEx2 echoes its argument.
struct FixtureFileManager : io::FileManager
{
    std::vector<std::string> writes;
    bool fileExists(const std::string&) override { return false; }
    std::string fileExistsEx(const std::string&, bool) override { return std::string(); }
    std::string fileExistsEx2(const std::string& path) override { return path; }
    bool getFileInfo(const std::string&, io::FileInfo&) override { return false; }
    bool readFile(const std::string&, void*, unsigned int, unsigned int& read) override { read = 0; return false; }
    bool writeFile(const std::string& name, const void*, unsigned int, bool) override { writes.push_back(name); return true; }
    int  findFiles(const std::string&, bool, std::list<io::FileInfo>& out) override { out.clear(); return 0; }
    io::IStream* open(const std::string&, io::FileMode, io::FileAccess, unsigned int) override { return 0; }
};
struct FixtureEvents : EventManager
{
    std::vector<std::pair<int, int> > posted;
    unsigned int postEvent(EventCategory category, unsigned int id, void* data, float delay) override
    { posted.push_back(std::make_pair(static_cast<int>(category), static_cast<int>(id))); return EventManager::postEvent(category, id, data, delay); }
};
FakeSwiffPlayer g_swiffPlayer; FakeSwiffRender g_swiffRender;
IBase* g_swiffPlayerSlot = 0; IBase* g_swiffRenderSlot = 0;
IBase* g_fixtureModInfoSlot = 0; // [TEMP] singleton seat for the real ModInfo factory.
IBase* createFakeSwiffPlayer(unsigned int, IBase*) { g_swiffPlayer.addRef(); return &g_swiffPlayer; }
IBase* createFakeSwiffRender(unsigned int, IBase*) { g_swiffRender.addRef(); return &g_swiffRender; }
// [TEMP] a class server with the real front-end rows (SwiffHost, MainMenuLogic, BetweenRoundsLogic,
// LocalProfileManager, GlobalSettings, MenuTeamManager - the same factories ModuleGame registers) and the two
// plugin fakes the menu bridge pulls.
struct MenuClassServer : IClassServer
{
    virtual void  addRef() {}
    virtual int   getRef() { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    virtual void  getClassList(std::vector<ClassRegInfo>& out)
    {
        ClassRegInfo row;
        row.classId = CID_SwiffHost;           row.name = "dice.hfe.SwiffHost";           row.create = &SwiffHost::create;           out.push_back(row);
        row.classId = CID_MainMenuLogic;       row.name = "dice.hfe.MainMenuLogic";       row.create = &MainMenuLogic::create;       out.push_back(row);
        row.classId = CID_BetweenRoundsLogic;  row.name = "dice.hfe.BetweenRoundsLogic";  row.create = &BetweenRoundsLogic::create;  out.push_back(row);
        row.classId = CID_ServerBrowser;       row.name = "dice.hfe.ServerBrowser";       row.create = &ServerBrowser::create;       out.push_back(row);
        row.classId = CID_ServerBrowserManager; row.name = "dice.hfe.ServerBrowserManager"; row.create = &ServerBrowserManager::create; out.push_back(row);
        row.classId = CID_LocalProfileManager; row.name = "dice.hfe.LocalProfileManager"; row.create = &LocalProfileManager::create; out.push_back(row);
        row.classId = CID_ProfileManager;      row.name = "dice.hfe.ProfileManager";      row.create = &ProfileManager::create;      out.push_back(row);
        row.classId = CID_GlobalSettings;      row.name = "dice.hfe.GlobalSettings";      row.create = &GlobalSettings::create;      out.push_back(row);
        row.classId = CID_ModInfo;             row.name = "dice.hfe.ModInfo";             row.create = &ModInfo::create;             out.push_back(row);
        row.classId = 870002u;                 row.name = "fixture.SwiffPlayer";          row.create = &createFakeSwiffPlayer;       out.push_back(row);
        row.classId = geom::CID_SwiffRender;   row.name = "fixture.SwiffRender";          row.create = &createFakeSwiffRender;       out.push_back(row);
        row.classId = CID_MenuTeamManager;     row.name = "dice.hfe.MenuTeamManager";     row.create = &MenuTeamManager::create;     out.push_back(row);
    }
    virtual void  getSingletonList(std::vector<SingletonRegInfo>& out)
    {
        SingletonRegInfo row;
        row.name = "SwiffHost";           row.instance = &g_swiffHost;           row.iid = IID_ISwiffHost;           row.classId = CID_SwiffHost;           row.flags = 2; out.push_back(row);
        row.name = "ServerBrowserManager"; row.instance = &g_serverBrowserManager; row.iid = IID_IServerBrowserManager; row.classId = CID_ServerBrowserManager; row.flags = 2; out.push_back(row);
        row.name = "LocalProfileManager"; row.instance = &g_localProfileManager; row.iid = IID_ILocalProfileManager; row.classId = CID_LocalProfileManager; row.flags = 2; out.push_back(row);
        row.name = "ProfileManager";      row.instance = &g_profileManager;      row.iid = IID_IProfileManager;      row.classId = CID_ProfileManager;      row.flags = 2; out.push_back(row);
        row.name = "GlobalSettings";      row.instance = &g_globalSettings;      row.iid = IID_IGlobalSettings;      row.classId = CID_GlobalSettings;      row.flags = 2; out.push_back(row);
        row.name = "ModInfo";             row.instance = &g_fixtureModInfoSlot;  row.iid = IID_IModInfo;             row.classId = CID_ModInfo;             row.flags = 2; out.push_back(row);
        row.name = "SwiffPlayer";         row.instance = &g_swiffPlayerSlot;     row.iid = 870001u;                  row.classId = 870002u;                 row.flags = 2; out.push_back(row);
        row.name = "SwiffRender";         row.instance = &g_swiffRenderSlot;     row.iid = geom::IID_ISwiffRender;   row.classId = geom::CID_SwiffRender;   row.flags = 2; out.push_back(row);
        row.name = "MenuTeamManager";     row.instance = &g_menuTeamManager;     row.iid = IID_IMenuTeamManager;     row.classId = CID_MenuTeamManager;     row.flags = 2; out.push_back(row);
    }
};

} // namespace

int main()
{
    using namespace dice::hfe;
    // These pre-init objects have no valid full-engine shutdown path. This bounded fixture
    // leaves their reclamation to process exit; it makes no teardown claim.
    g_system = new System();
    BF2Engine* engine = new BF2Engine(0);
    ISettingsRepostitory::createSingleton();
    unsigned checks = 0, failures = 0;
    auto check = [&](bool condition, const char* name) {
        ++checks;
        if (!condition) { ++failures; std::printf("FAIL: %s\n", name); }
    };
    check(engine->parseParameters(""), "empty command line accepted");
    check(!isDedicated(), "client runtime defaults to non-dedicated");
    check(!settings->boolGet("GSDedicated", true), "client repository defaults to non-dedicated");
    check(settings->boolGet("GSIsServer", false), "retail client keeps GSIsServer true");
    check(engine->parseParameters("+dedicated 1"), "dedicated option accepted");
    check(isDedicated() && settings->boolGet("GSDedicated", false), "explicit dedicated mode still works");
    check(engine->parseParameters("+multi 1 +noSound 1 +fullscreen 0 +modPath mods/bf2"), "diagnostic launch arguments accepted");
    check(!isDedicated() && !settings->boolGet("GSDedicated", true), "next client parse clears dedicated mode");
    check(settings->boolGet("GSAllowMultipleInstances", false), "multi-instance argument applied");
    check(!settings->boolGet("GSFullScreen", true), "windowed argument applied");
    check(engine->parseParameters("+joinServer 127.0.0.1"), "join argument parsed without making a connection");
    check(!isDedicated() && !settings->boolGet("GSIsServer", true), "join selects client settings");
    const wchar_t* path = getCrashDumpDirectory();
    const std::wstring directory(path);
    check(directory.size() >= 5 && directory.substr(directory.size() - 5) == L"\\dmp\\", "retail crash-directory suffix");
    check(path == getCrashDumpDirectory(), "cached crash-directory pointer remains stable");
    // GetFileAttributesW requires the directory name without its trailing separator.
    DWORD attributes = GetFileAttributesW(directory.substr(0, directory.size() - 1).c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
        std::printf("Crash-directory attributes failed: %lu\n", GetLastError());
    check(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY), "crash directory exists");

    // the intro-movie queue: Init.con's bf2Engine.playMovie rows -> checkMovie -> the plugin player
    classManager = new ClassManager();
    static MovieClassServer movieServer;
    classManager->registerServer(&movieServer);
    ConsoleObjectBaseImpl* playMovie = 0;
    unsigned engineBindings = 0;
    std::list<ConsoleObject*>& bindings = gameConsoleObjects();
    for (std::list<ConsoleObject*>::iterator it = bindings.begin(); it != bindings.end(); ++it)
    {
        if (std::strcmp((*it)->getObjectName(), "bf2Engine")) continue;
        ++engineBindings;
        if (!std::strcmp((*it)->getFunctionName(), "playMovie")) playMovie = static_cast<ConsoleObjectBaseImpl*>(*it);
    }
    check(playMovie != 0 && engineBindings == 1, "bf2Engine.playMovie is the game TU's one bf2Engine binding");
    if (playMovie)
    {
        check(!std::strcmp(playMovie->getReturnType(), "void") && playMovie->getMinNoArgs() == 2 && playMovie->getMaxNoArgs() == 2
              && !std::strcmp(playMovie->getArgType(0), "std::string") && !std::strcmp(playMovie->getArgType(1), "int"),
              "playMovie: void, exactly two arguments, a string then an int");
        check(playMovie->getAccess() == 2 && playMovie->getCategory() == 0, "playMovie carries the bf2Engine defaults (access 2, category 0)");
        BF2Engine* const published = bf2Engine;
        bf2Engine = 0;
        check(!playMovie->isActive(), "playMovie is inactive without an engine");
        bf2Engine = published;
        check(bf2Engine == engine && playMovie->isActive(), "the engine constructor published bf2Engine, so playMovie is active");
        check(!engine->isMoviePlaying(), "no movie plays before checkMovie");
        std::string args[2];
        args[0] = "Movies\\EA.bik";
        args[1] = "1";
        playMovie->execute(args, 1);
        engine->checkMovie();
        check(g_playerCreates == 0, "a one-argument playMovie queues nothing");
        settings->boolSet("skipMovies", true);
        engine->playMovie("Movies\\dice.bik", 1);
        engine->checkMovie();
        check(g_playerCreates == 0, "skipMovies drops both the queued movie and the check");
        settings->boolSet("skipMovies", false);
        playMovie->execute(args, 2);                       // bf2Engine.playMovie Movies\EA.bik 1
        engine->playMovie("Movies\\intro.bik", 0);
        g_rejectLoads = 1;
        engine->checkMovie();
        check(g_playerCreates == 2 && g_playerReleases == 1 && g_loads.size() == 2
              && g_loads[0] == "Movies\\EA.bik" && g_loads[1] == "Movies\\intro.bik",
              "a refused load releases the player and the next queued movie opens");
        check(engine->isMoviePlaying() && g_frames == 0, "the player of the opened movie stays alive; no frame drawn outside mainLoop");
        engine->checkMovie();
        check(g_playerCreates == 2, "checkMovie with a running player creates nothing");
    }

    // Front-end host and Flash menu bridge.
    {
        static MenuClassServer menuServer;
        classManager->registerServer(&menuServer);
        BF2EngineFixture::createSetup(*engine);
        static FixtureEvents events; eventManager = &events;
        static FixtureConsole console; io::mainConsole = &console;
        static FixtureFileManager fileSystem; io::fileManager = &fileSystem;
        static FakeInputDeviceManager devices; io::inputDeviceManager = &devices;
        io::controlMapManager = new io::ControlMapManager();
        g_controlSettings = ControlSettings::create(IID_IControlSettings, 0);
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
        check(host != 0 && g_swiffHost == host && host->queryInterface(IID_ISwiffHost) == host && host->queryInterface(1) == host
              && !host->queryInterface(2), "the SwiffHost singleton comes from its row and answers iids 1 and 121011");
        // [TEMP] Real online owner, factory and idle frame path; no account/login fixture.
        IProfileManager* online = static_cast<IProfileManager*>(classManager->getSingleton("ProfileManager"));
        check(online && online == g_profileManager && online->queryInterface(IID_IProfileManager) == online,
              "online profile singleton resolves through its actual factory and interface");
        check(!online->isLoggedIn() && !online->update(),
              "fresh online owner is disconnected and its idle update returns false");
        std::string transition = "unchanged";
        check(!online->hasPendingTransition(transition) && transition == "unchanged",
              "absent transition leaves the output untouched");
        const std::string transitionText(8192, 't');
        online->setPendingTransition(true, transitionText);
        check(online->hasPendingTransition(transition) && transition == transitionText,
              "pending transition copies the owner's long string");
        transition.clear();
        check(online->hasPendingTransition(transition) && transition == transitionText,
              "reading a transition does not consume its flag");
        online->setPendingTransition(false, "retained");
        transition = "sentinel";
        check(!online->hasPendingTransition(transition) && transition == "sentinel",
              "disabled transition preserves the caller's output");
        online->logout();
        check(!online->isLoggedIn() && host->getSelectedPlayerId() == 0 && host->getSelectedPlayerName().empty(),
              "idle logout reaches the real host's zero-id reset without a login");
        // [TEMP] Exercise both real host wrappers and the existing formatter, preserving
        // the owner's build stamp and the assignment-versus-value-return contracts.
        const char* buildText = BuildNrUtil::getBuildNrString();
        check(std::strcmp(buildText, "1.5.3153-802.0-LC") == 0,
              "version formatter retains the existing owner stamp");
        std::string version(8192, 'x');
        check(host->getVersionString(version) && version == buildText,
              "host version assigns into a populated output and returns true");
        std::string versionValue = host->getVersionStringValue();
        check(versionValue == buildText, "value-returning host version uses the same formatter");
        version.assign(8192, 'y'); versionValue.assign(8192, 'z');
        check(BuildNrUtil::getBuildNrString() == buildText
              && std::strcmp(buildText, "1.5.3153-802.0-LC") == 0,
              "both host strings own their bytes and leave the cached formatter intact");
        version.clear();
        check(host->getVersionString(version) && version == "1.5.3153-802.0-LC",
              "host version also assigns into an empty constructed output");
        check(host->getVersionStringValue() == version,
              "repeated value return remains valid after earlier copies change");
        unsigned int width = 0, height = 0; host->getViewportSize(&width, &height);
        int rect[4]; host->getRect(rect);
        check(width == 800 && height == 600 && rect[0] == 0 && rect[2] == 1024 && rect[3] == 768 && host->getGameMode() == "gpm_cq"
              && host->getFlagE81() && host->getFlag28() && host->getFloatF0C() == 500.0f && host->getIntE94() == 10 && host->getLeaderboardPosition() == 1
              && host->getIntF08() == 1 && host->getFlagF2C() && host->areMessagesEnabled() && host->getPage() == 0 && host->getBusy() == 0,
              "the constructor's defaults (0x4A27D0)");
        MenuEvent event;
        check(!host->pollEvent(event), "an empty queue polls nothing");
        MenuEvent a; a.type = 3; a.args.push_back("1.2.3.4");
        MenuEvent b; b.type = 3; b.args.push_back("5.6.7.8");
        host->pushEvent(a); host->pushEvent(a); host->pushEvent(b);
        check(host->pollEvent(event) && event.type == 3 && event.args.size() == 1 && event.args[0] == "5.6.7.8"
              && host->pollEvent(event) && event.args[0] == "1.2.3.4" && !host->pollEvent(event),
              "an event identical to the last queued one is dropped; polling hands out the last first");
        host->playSound("menuClick"); host->requestBrowseUrl("http://x"); host->requestRestartMenu(); host->requestQuit(); host->requestShutdownGame();
        check(host->pollEvent(event) && event.type == 17 && host->pollEvent(event) && event.type == 2 && host->pollEvent(event) && event.type == 16
              && host->pollEvent(event) && event.type == 10 && event.args[0] == "http://x" && host->pollEvent(event) && event.type == 6 && event.args[0] == "menuClick",
              "playSound, browse, restart, quit and shutdown queue events 6, 10, 16, 2 and 17");
        std::string modName("bf2");
        check(host->requestActivateMod(&modName) && host->pollEvent(event) && event.type == 11 && event.args[0] == "bf2"
              && !host->requestActivateMod(0) && host->getMessageCount() == 1 && host->getMessage(0)->type == 1 && host->getMessage(0)->code == 0
              && host->getMessage(0)->text == "no mod specified", "activateMod queues event 11; a null mod posts the type-1 message");
        host->clearMessages();
        for (int i = 0; i < 6; ++i) host->postMessage(SwiffHostMessage(3, std::string("m") + static_cast<char>('0' + i), i));
        check(host->getMessageCount() == 5 && host->getMessage(0)->text == "m1" && host->getMessage(4)->text == "m5" && host->getMessage(4)->code == 5,
              "the ring keeps five messages, the oldest shifted out");
        host->postMessage(SwiffHostMessage(1, "The password does not match the email address.", 0));
        check(host->getMessageCount() == 5 && host->getMessage(4)->type == 1 && host->getMessage(4)->text.find("An account name already exists") == 0,
              "the password mismatch text is rewritten");
        check(host->countMessagesByType(3) == 4 && host->getMessageByType(1, 3)->text == "m3" && host->getMessageByType(9, 3)->text.empty()
              && host->removeMessageByType(0, 3) && host->getMessage(0)->text == "m3" && !host->removeMessageByType(5, 3) && !host->removeMessage(9),
              "type-indexed lookup and removal, with the static empty record for a miss");
        SwiffHostMessage last;
        check(host->popLastMessage(last, false) && last.type == 1 && host->getMessageCount() == 4 && host->popLastMessage(last, true)
              && host->getMessageCount() == 3 && host->removeLastMessage() && host->getMessageCount() == 2, "pop with and without removal");
        check(!host->setMessagesEnabled(false) && host->getMessageCount() == 0 && !host->postMessage(SwiffHostMessage(3, "x", 3))
              && host->getMessageCount() == 0 && host->setMessagesEnabled(true), "disabling messages empties the ring and refuses posts");
        host->setStorageString("k", "v");
        check(host->getStorageString("k", "d") == "v" && host->getStorageString("z", "d") == "d", "storage strings answer by key with a fallback");
        host->removeStorageString("k");
        check(host->getStorageString("k", "d") == "d", "a removed storage string falls back");
        std::string error;
        check(!host->validateProfileName("ab", error) && error == "nameshort"
              && !host->validateProfileName(std::string(21, 'a'), error) && error == "namelong"
              && !host->validateProfileName(" abc", error) && error == "invalidnamespace"
              && !host->validateProfileName("a@bc", error) && error == "invalidnamethree"
              && !host->validateProfileName("abc\"d", error) && error == "invalidnamecharacter"
              && !host->validateProfileName("abc\\d", error) && error == "invalidnamecharacter"
              && host->validateProfileName("abc@d", error), "the profile-name rules (0x4886C0)");
        check(host->isDottedQuad("192.168.1.1") && !host->isDottedQuad("192.168.1") && !host->isDottedQuad("256.1.1.1")
              && !host->isDottedQuad("a.b.c.d") && !host->isDottedQuad(""), "dotted quads");
        check(host->isPrivateAddress("10.0.0.1") && host->isPrivateAddress("172.16.5.5") && host->isPrivateAddress("172.31.0.1")
              && host->isPrivateAddress("192.168.0.9") && !host->isPrivateAddress("8.8.8.8") && !host->isPrivateAddress("172.32.0.1"),
              "RFC 1918 addresses");
        check(host->formatDuration(3725) == "1:02:05" && host->formatDuration(59) == "0:00:59", "durations");
        time_t zero = 0;
        check(host->formatDate(&zero) == "-", "a zero time formats as -");
        {
            struct tm when = {}; when.tm_year = 105; when.tm_mon = 5; when.tm_mday = 7; when.tm_hour = 12;
            time_t stamp = mktime(&when);
            check(host->formatDate(&stamp) == "2005-06-07", "year, two-digit month name and day");
        }
        check(host->resolveMenuPath("$/mods/bf2/x.png") == "..\\..\\..\\..\\..\\mods/bf2/x.png"
              && host->resolveMenuPath("logo.png") == "..\\..\\..\\..\\..\\Menu\\Extermal\\FlashMenu\\logo.png",
              "menu paths with and without the root prefix");
        check(host->getExpansionPack("xpack") == 1 && host->getExpansionPack("XPACK2") == 2 && host->getExpansionPack("bf2") == 0, "expansion packs");
        std::vector<SwiffSinglePlayerLevel> levels;
        check(host->getSinglePlayerLevels(levels) && levels.size() == 8 && levels[0].name == "US vs MEC - Wetlands Level 1" && levels[0].index == 1
              && levels[4].map == "the_dam" && levels[7].map == "divided_city_day" && levels[7].mode == "mp" && levels[3].mode == "mp",
              "the eight single-player rows");
        host->addServerEntry("a", "b", 1); host->addServerEntry("c", "d", 2); host->addServerEntry("e", "f", 3);
        check(!host->moveServerEntry(0, true) && !host->moveServerEntry(2, false) && !host->moveServerEntry(9, true) && host->moveServerEntry(1, true),
              "moves off the ends are refused, a move up swaps with the previous row");
        std::vector<SwiffServerEntry> entries;
        check(host->getServerEntries(entries) && entries.size() == 3 && entries[0].map == "c" && entries[1].map == "a" && entries[1].size == 1
              && !host->removeServerEntry(5) && host->removeServerEntry(0) && host->clearServerEntries() && host->getServerEntries(entries) && entries.empty(),
              "entries copy out, remove by index and clear");
        host->addRank(3, 1, 2, 3, "Sergeant");
        check(console.count("menu/external/flashmenu/ranks.con") == 1 && host->getRankName(3) == "Sergeant" && host->getRankB(3) == 2 && host->getRankC(3) == 3
              && host->getRankName(4) == "-" && host->getRankB(4) == 0 && host->getRankCount() == 1 && console.count("menu/external/flashmenu/ranks.con") == 1,
              "ranks.con runs once and the rank table answers");
        settings->stringSet("GSJoinAddress", "1.2.3.4");
        check(host->getJoinAddressSetting() == "1.2.3.4" && host->getPortSetting() == 16567 && host->getConnectToIpPort() == 16567
              && host->getLanguageSetting() == "English" && host->getModDirectorySetting() == "mods/bf2", "the settings-backed getters and their defaults");
        host->setConnectToIpPort(1234); host->setConnectToIpHost("h");
        check(host->getConnectToIpPort() == 1234 && host->getConnectToIpHost() == "h", "the connect-to-ip setters");
        host->setJoinAddress("addr"); host->setJoinPassword("pw"); host->setPage(4); host->setMessageId(20); host->setServerMod("mods/xpack");
        check(host->getJoinAddress() == "addr" && host->getJoinPassword() == "pw" && host->getPage() == 4 && host->getMessageId() == 20
              && host->getServerMod() == "mods/xpack" && host->getExpansionPack(host->getServerMod()) == 0, "the join/page/message/mod fields");
        host->setFloatF0C(9000.0f); const float clampedHigh = host->getFloatF0C(); host->setFloatF0C(1.0f);
        check(clampedHigh == 4000.0f && host->getFloatF0C() == 500.0f, "the +0xF0C float clamps to 500..4000");
        host->setLeaderboardPosition(-5);
        check(host->getLeaderboardPosition() == 1, "the +0x77C int clamps to at least 1");
        check(host->getCurrentMapName() == "$noCurrentGame" && host->getGameStatus() == 1 && host->getTimeUntilRestartMap() == 0.0f
              && !host->isGameLogicServer() && !host->isPlayingDemo() && host->checkDisc() && host->getTeamName(1).empty() && !host->getRenderFlag(),
              "without a game, game logic or renderer the getters answer their idle values");
        bool running = true;
        check(!host->isGameRunning(&running) && !running, "isGameRunning writes false through the out-parameter");
        check(!host->checkGameConnected() && host->getMessageCount() == 1
              && host->getMessage(0)->text == "unable to fetch server info since game is not connected", "checkGameConnected posts its message");
        host->reportNoDisc();
        check(host->getMessageCount() == 2 && host->getMessage(1)->text == "unable to find BF2 cd/dvd in tray" && host->getMessage(1)->type == 1, "reportNoDisc");
        host->clearMessages();
        IMenuTeamManager* menuTeams = static_cast<IMenuTeamManager*>(host->getMenuTeamManager());
        check(menuTeams != 0 && menuTeams == g_menuTeamManager && menuTeams->queryInterface(IID_IMenuTeamManager) == menuTeams
              && menuTeams->getTeamCount() == 0 && host->getMenuTeamManager() == menuTeams
              && console.count("menu/external/flashmenu/menuTeams.con") == 1
              && console.count("menu/external/flashmenu/weaponStats.con") == 1
              && console.count("menu/external/flashmenu/gameModes.con") == 1,
              "the real MenuTeamManager singleton is fetched and requests all three scripts once");
        // [TEMP] The script recorder above deliberately does not parse .con files.
        // Replay gameModes.con's three stock argument pairs through the real console
        // binding, real SwiffHost slot and real ModInfo factory/table as a separate check.
        registerSwiffHostConsoleObjects();
        ConsoleObject* addMode = mainConsoleObjects->getConsoleObject("swiffHost", "addGameMode");
        IModInfo* modeInfo = static_cast<IModInfo*>(host->getModInfo());
        check(addMode && modeInfo && modeInfo == g_fixtureModInfoSlot,
              "game mode console binding and real ModInfo singleton are available");
        const char* modeKeys[] = { "gpm_cq", "sp1", "gpm_coop" };
        const char* modeNames[] = { "Conquest", "Singleplayer", "Co-op" };
        for (unsigned i = 0; i < 3 && addMode && modeInfo; ++i) {
            std::string modeArgs[] = { modeKeys[i], modeNames[i] };
            check(addMode->execute(modeArgs, 2).empty() && modeInfo->getNameFromGameMode(modeKeys[i]) == modeNames[i],
                  "stock game mode pair reaches the real SwiffHost and ModInfo table");
        }
        unregisterSwiffHostConsoleObjects();
        host->loadCredits(); host->loadCredits();
        check(console.count("menu/external/flashmenu/credits.con") == 1, "credits.con runs once");
        check(!host->preloadTexture("x") && !host->isRunningWithUnsupportedDrivers(), "the renderer queries against the fixture renderer");
        host->setRenderFlag(true);
        check(host->getRenderFlag() && g_swiffRender.flag == 1, "the render flag goes to SwiffRender +0xF0");

        // MainMenuLogic through its row
        IMenuLogic* logic = static_cast<IMenuLogic*>(classManager->createInstance(CID_MainMenuLogic, IID_IMenuLogic, 0));
        check(logic != 0 && logic->getClassId() == CID_MainMenuLogic && logic->queryInterface(IID_IMenuLogic) == logic && logic->queryInterface(1) == logic
              && !logic->queryInterface(3) && !logic->getSwiffPlayer() && !logic->getSwiffHost() && !logic->getSwiffRender() && !logic->getConsole(),
              "MainMenuLogic is created through its row with empty collaborators");
        logic->setConsole(0);
        fileSystem.writes.clear(); console.files.clear(); events.posted.clear();
        check(logic->init(), "init succeeds against the fixture's player/renderer and real team manager");
        check(logic->getSwiffPlayer() == &g_swiffPlayer && logic->getSwiffHost() == host && logic->getSwiffRender() == &g_swiffRender
              && g_swiffPlayer.host == host && g_swiffPlayer.render == &g_swiffRender && g_swiffPlayer.movie == "mainMenu.swf" && g_swiffPlayer.loads == 1
              && g_swiffRender.inits == 1 && menuTeams == g_menuTeamManager
              && console.count("menu/external/flashmenu/menuTeams.con") == 0
              && console.count("menu/external/flashmenu/weaponStats.con") == 0
              && console.count("menu/external/flashmenu/gameModes.con") == 0
              && g_swiffPlayer.scaleX == 1.0f && g_swiffPlayer.scaleY == 1.0f
              && devices.mouse.deactivations == 1 && events.posted.size() == 1 && events.posted[0].first == 10 && events.posted[0].second == 1
              && console.count("menu/external/flashmenu/precache.con") == 1 && fileSystem.writes.size() == 1 && fileSystem.writes[0] == "@HOME@/Profiles/Global.con"
              && !host->getFlagEC4() && !host->getFlagF04() && host->getFlagE81(),
              "init: Global.con written by the profile manager, SwiffPlayer.dll load ignored, (10, 1) posted, the singletons wired, the movie loaded, "
              "the scale 1:1, the mouse released, precache.con once, the host flags reset");
        check(logic->beginFrame() && g_swiffRender.beginFrames == 1, "beginFrame asks the renderer and the loaded movie");
        logic->display(); logic->endFrame(); logic->update(0.5);
        check(g_swiffPlayer.displays == 1 && g_swiffRender.endFrames == 1 && g_swiffPlayer.updates == 1 && g_swiffPlayer.lastDt == 0.5f,
              "display, endFrame and update reach the player and the renderer");
        logic->onMouseMove(10, 20); logic->onMouseButtonDown(); logic->onMouseButtonUp(); logic->onMouseDoubleClick();
        check(g_swiffPlayer.mouse.size() == 4 && g_swiffPlayer.mouse[0].x == 10 && g_swiffPlayer.mouse[0].buttons == 0 && g_swiffPlayer.mouse[1].buttons == 1
              && g_swiffPlayer.mouse[2].buttons == 0 && g_swiffPlayer.mouse[3].buttons == 2 && g_swiffPlayer.mouse[3].y == 20,
              "mouse moves and presses reach the player with the codes 0, 1, 0, 2");
        logic->onResize(1600, 1200);
        check(g_swiffPlayer.scaleX == 2.0f && g_swiffPlayer.scaleY == 2.0f && host->getFlagEC4(), "resize rescales the player and raises the host's +0x2C8 flag");
        io::GameInput gameInput;
        menuKeyBuffer[menuKeyWrite++] = 'a'; ++menuKeyCount;
        g_swiffPlayer.chars.clear(); g_swiffPlayer.keyStates.clear(); devices.keyboard.released.push_back(30);
        check(logic->handleInput(gameInput) && g_swiffPlayer.chars.size() == 1 && g_swiffPlayer.chars[0] == 'a'
              && g_swiffPlayer.keyStates.size() == 1 && g_swiffPlayer.keyStates[0].first == 30 && g_swiffPlayer.keyStates[0].second == 0,
              "a printable key is a character; the release sweep reports the keyboard's released key");
        menuKeyBuffer[menuKeyWrite++] = 8; ++menuKeyCount; g_swiffPlayer.keyStates.clear(); devices.keyboard.released.clear();
        check(logic->handleInput(gameInput) && g_swiffPlayer.keyStates.size() == 1 && g_swiffPlayer.keyStates[0].first == 14 && g_swiffPlayer.keyStates[0].second == 1,
              "backspace becomes DIK_BACK pressed");
        g_swiffPlayer.keyStates.clear(); devices.keyboard.pressed.push_back(200);
        check(logic->handleInput(gameInput) && g_swiffPlayer.keyStates.size() == 1 && g_swiffPlayer.keyStates[0].first == 200 && g_swiffPlayer.keyStates[0].second == 1,
              "with no character queued the navigation keys are polled");
        devices.keyboard.pressed.clear();
        logic->deactivate();
        check(devices.mouse.activations == 1 && events.posted.size() == 2 && events.posted[1].first == 10 && events.posted[1].second == 2,
              "deactivate re-activates the mouse and posts (10, 2)");
        check(logic->release() == 0 && devices.mouse.activations == 2 && events.posted.size() == 2, "the final release runs the destructor's deactivate without a second (10, 2)");

        // the engine's own menu entry points
        g_swiffPlayer.loads = 0; events.posted.clear();
        engine->activateMenu(0);
        check(settings->boolGet("GSMenu", false) && g_swiffPlayer.loads == 1 && events.posted.size() == 1 && events.posted[0].second == 1,
              "activateMenu creates and initialises the menu logic through the class manager");
        const unsigned int packed = (1024u << 16) | 768u;
        engine->handleEvent(static_cast<EventCategory>(11), 2, 0, const_cast<unsigned int*>(&packed));
        host->getViewportSize(&width, &height);
        check(width == 1024 && height == 768 && host->getFlagE80OrNotE81() && g_swiffPlayer.scaleX == 1024.0f / 800.0f, "handleEvent(11, 2) resizes the menu and the host's viewport");
        engine->restartMenu();
        check(host->getMessageId() == 20 && !host->getFlagE80OrNotE81() && g_swiffPlayer.loads == 2 && events.posted.size() == 3, "restartMenu re-creates the menu logic around message 20");
        engine->deactivateMenu();
        check(events.posted.size() == 4 && events.posted[3].second == 2 && devices.mouse.activations == 4, "deactivateMenu releases the menu logic");

        // BetweenRoundsLogic uses the same host/player/render singletons, but its update also
        // advances input and its frame path deliberately does not wait for the SWF-loaded flag.
        IMenuLogic* between = static_cast<IMenuLogic*>(classManager->createInstance(CID_BetweenRoundsLogic, IID_IMenuLogic, 0));
        check(between != 0 && between->getClassId() == CID_BetweenRoundsLogic
              && between->queryInterface(IID_IMenuLogic) == between && between->queryInterface(1) == between
              && !between->queryInterface(3) && !between->getSwiffPlayer() && !between->getSwiffHost()
              && !between->getSwiffRender() && !between->getConsole(),
              "BetweenRoundsLogic is created through its client row with empty collaborators");
        const unsigned loadsBefore = g_swiffPlayer.loads;
        const unsigned renderInitsBefore = g_swiffRender.inits;
        const unsigned mouseDeactivationsBefore = devices.mouse.deactivations;
        const unsigned mouseActivationsBefore = devices.mouse.activations;
        const unsigned playerUpdatesBefore = g_swiffPlayer.updates;
        const unsigned inputUpdatesBefore = devices.updates;
        const unsigned displaysBefore = g_swiffPlayer.displays;
        const unsigned beginFramesBefore = g_swiffRender.beginFrames;
        const unsigned endFramesBefore = g_swiffRender.endFrames;
        const unsigned legacyOffsBefore = g_swiffPlayer.legacyOffs;
        const unsigned legacyOnsBefore = g_swiffPlayer.legacyOns;
        events.posted.clear();
        check(between->init(), "BetweenRoundsLogic init succeeds against the front-end fixture");
        check(between->getSwiffPlayer() == &g_swiffPlayer && between->getSwiffHost() == host
              && between->getSwiffRender() == &g_swiffRender && !between->getConsole()
              && g_swiffPlayer.host == host && g_swiffPlayer.render == &g_swiffRender
              && g_swiffPlayer.movie == "endOfRound.swf" && g_swiffPlayer.loads == loadsBefore + 1
              && g_swiffRender.inits == renderInitsBefore + 1
              && g_swiffPlayer.scaleX == 1024.0f / 800.0f && g_swiffPlayer.scaleY == 768.0f / 600.0f
              && devices.mouse.deactivations == mouseDeactivationsBefore + 1
              && events.posted.size() == 1 && events.posted[0].first == 10 && events.posted[0].second == 1
              && !host->getFlagEC4() && host->getFlag28() && host->getFlagE81() && !host->getFlagEA8(),
              "BetweenRoundsLogic init posts enable, wires the collaborators, loads endOfRound.swf, scales to the viewport and resets loading flags");
        between->update(0.75);
        check(g_swiffPlayer.updates == playerUpdatesBefore + 1 && g_swiffPlayer.lastDt == 0.75f
              && devices.updates == inputUpdatesBefore + 1 && devices.lastDt == 0.75f,
              "BetweenRoundsLogic update advances both the movie and the input manager");
        g_swiffPlayer.movie.clear();
        check(!g_swiffPlayer.isMovieLoaded() && between->beginFrame()
              && g_swiffRender.beginFrames == beginFramesBefore + 1,
              "BetweenRoundsLogic begins rendering before the loading movie reports loaded");
        between->display();
        between->endFrame();
        check(g_swiffPlayer.displays == displaysBefore + 1 && g_swiffRender.endFrames == endFramesBefore + 1,
              "BetweenRoundsLogic display and endFrame reach the player and renderer");
        g_swiffPlayer.mouse.clear();
        g_swiffPlayer.keyStates.clear();
        g_swiffPlayer.chars.clear();
        devices.keyboard.pressed.clear();
        devices.keyboard.released.clear();
        between->onMouseMove(37, 49);
        between->onMouseButtonDown();
        check(between->handleInput(gameInput) && g_swiffPlayer.mouse.size() == 1
              && g_swiffPlayer.mouse[0].x == 37 && g_swiffPlayer.mouse[0].y == 49 && g_swiffPlayer.mouse[0].buttons == 1
              && g_swiffPlayer.legacyOffs == legacyOffsBefore + 1,
              "BetweenRoundsLogic flushes its cached pressed mouse state and lowers the player flag");
        between->onMouseButtonUp();
        check(between->handleInput(gameInput) && g_swiffPlayer.mouse.size() == 2 && g_swiffPlayer.mouse[1].buttons == 0,
              "BetweenRoundsLogic flushes the released mouse state");
        between->onMouseDoubleClick();
        check(g_swiffPlayer.legacyOns == legacyOnsBefore + 1,
              "BetweenRoundsLogic double-click raises the player flag");
        between->deactivate();
        check(devices.mouse.activations == mouseActivationsBefore + 1 && events.posted.size() == 2
              && events.posted[1].first == 10 && events.posted[1].second == 2 && !host->getRenderFlag(),
              "BetweenRoundsLogic deactivate re-activates the mouse, lowers rendering and posts disable");
        check(between->release() == 0 && devices.mouse.activations == mouseActivationsBefore + 2
              && events.posted.size() == 2,
              "BetweenRoundsLogic final release repeats teardown without posting a second disable");
    }
    // Server-browser manager and list integration.
    {
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
        ui::IServerBrowserManager* manager = static_cast<ui::IServerBrowserManager*>(classManager->getSingleton("ServerBrowserManager"));
        check(manager != 0 && g_serverBrowserManager == manager && manager->queryInterface(IID_IServerBrowserManager) == manager
              && manager->queryInterface(1) == manager && !manager->queryInterface(2) && manager->getListCount() == 0 && !manager->isAnyBrowsing(),
              "the ServerBrowserManager singleton comes from its row, answers iids 1 and 121063 and starts empty");
        const DWORD started = GetTickCount();
        check(host->init() && host->init(), "SwiffHost::init creates the eight lists once (each ran the GameSpy availability check)");
        std::printf("  (the eight lists took %lu ms: the availability check of battlefield2.available.gamespy.com per list)\n",
                    static_cast<unsigned long>(GetTickCount() - started));
        check(manager->getListCount() == 8 && !manager->createList(std::string("inet"), true) && manager->getListCount() == 8,
              "eight named lists; creating a name again refuses");
        ui::IServerBrowser* inet = manager->getList(std::string("inet"));
        ui::IServerBrowser* lan = manager->getList(std::string("lan"));
        ui::IServerBrowser* favourites = manager->getList(std::string("favourites"));
        ui::IServerBrowser* history = manager->getList(std::string("history"));
        check(inet && lan && favourites && history && !manager->getList(std::string("nosuch")) && host->getServerList(std::string("playNow")) != 0
              && host->getServerList(std::string("serverInfo")) != 0 && !host->getServerList(std::string("x")),
              "the lists by name through the manager and the host");
        check(inet->isInternet() && !lan->isInternet() && !manager->getList(std::string("joinServerLan"))->isInternet()
              && manager->getList(std::string("joinServerInet"))->isInternet() && favourites->isInternet(), "internet and LAN lists as init named them");
        check(ServerBrowserFixture::keepsUnanswered(favourites) && ServerBrowserFixture::keepsUnanswered(history) && !ServerBrowserFixture::keepsUnanswered(inet),
              "favourites and history keep servers without basic keys; the others do not");
        check(inet->queryInterface(IID_IServerBrowser) == inet && inet->queryInterface(1) == inet && !inet->queryInterface(2) && inet->getRef() == 1,
              "a list answers iids 1 and 121034 and is held once by the manager");
        const bool internetHandle = ServerBrowserFixture::handle(inet) != 0;
        check(ServerBrowserFixture::handle(lan) != 0, "a LAN list has its GameSpy handle whatever the availability answer");
        std::printf("  (the internet lists %s a GameSpy handle: the availability check answered %s)\n",
                    internetHandle ? "hold" : "lack", internetHandle ? "available" : "not available");
        ui::ServerBrowserRecordList* records = lan->getRecords();
        check(records != 0 && records->records.empty() && records->revision == 0 && records->maxPing == -1 && lan->getFilter().empty()
              && ServerBrowserFixture::sortKey(lan).empty() && !ServerBrowserFixture::updateStarted(lan),
              "a fresh list: no records, revision 0, no ping limit, no filter, no sort");
        check(!inet->isBrowsing() && !lan->isBrowsing() && !manager->isAnyBrowsing(), "nothing browses before an update starts");
        host->update(); manager->update();
        check(!manager->isAnyBrowsing() && records->revision == 0, "the per-frame update with nothing browsing changes nothing");
        lan->setMaxPing(150); lan->setFilter(std::string("gametype='gpm_cq'"));
        check(records->maxPing == 150 && ServerBrowserFixture::maxPing(lan) == 150 && lan->getFilter() == "gametype='gpm_cq'",
              "the ping limit reaches the results list and the class; the filter is kept");
        lan->setMaxPing(-1);

        // a synthetic server in the LAN list's SDK list (the SDK's own allocator and key table)
        _ServerBrowser* handle = ServerBrowserFixture::handle(lan);
        SBServer server = SBAllocServer(&handle->list, inet_addr("10.1.2.3"), htons(29900));
        SBServerAddKeyValue(server, "hostname", "Fixture server");
        SBServerAddIntKeyValue(server, "hostport", 16000);
        SBServerAddIntKeyValue(server, "numplayers", 2);
        SBServerAddIntKeyValue(server, "maxplayers", 32);
        SBServerAddKeyValue(server, "gametype", "gpm_coop");
        SBServerAddKeyValue(server, "mapname", "strike_at_karkand");
        SBServerAddKeyValue(server, "gamevariant", "bf2");
        SBServerAddKeyValue(server, "gamever", "1.5.3153-802.0");
        SBServerAddIntKeyValue(server, "bf2_ranked", 1);
        SBServerAddIntKeyValue(server, "bf2_dedicated", 1);
        SBServerAddIntKeyValue(server, "bf2_plasma", 1);
        SBServerAddIntKeyValue(server, "bf2_reservedslots", 4);
        SBServerAddKeyValue(server, "bf2_fps", "35.5");
        SBServerAddKeyValue(server, "bf2_team1", "US");
        SBServerAddKeyValue(server, "bf2_novehicles", "1.0");
        SBServerAddIntKeyValue(server, "bf2_globalunlocks", 1);
        SBServerAddKeyValue(server, "player_0", "alice"); SBServerAddIntKeyValue(server, "pid_0", 7); SBServerAddKeyValue(server, "team_0", "1");
        SBServerAddIntKeyValue(server, "score_0", 10); SBServerAddIntKeyValue(server, "AIBot_0", 0);
        SBServerAddKeyValue(server, "player_1", "bot1"); SBServerAddIntKeyValue(server, "AIBot_1", 1);
        SBServerAddKeyValue(server, "player_2", "carol"); SBServerAddIntKeyValue(server, "deaths_2", 3);
        SBServerSetState(server, STATE_BASICKEYS | STATE_FULLKEYS);
        SBServerListAppendServer(&handle->list, server);
        const int revisionAfterAppend = records->revision;
        std::printf("  (the SDK's append left the results at revision %d with %u record(s); the unqueried server's ping reads %d)\n",
                    revisionAfterAppend, static_cast<unsigned>(records->records.size()), SBServerGetPing(server));
        ui::ServerBrowserRecord record;
        check(lan->fillRecord(server, &record, true, true) && record.server == server && record.entry.address == "10.1.2.3" && record.entry.queryPort == 29900
              && record.entry.privateAddress == "0.0.0.0" && record.entry.hostName == "Fixture server" && record.entry.hostPort == 16000
              && record.entry.numPlayers == 2 && record.entry.maxPlayers == 32 && record.entry.gameMode == "gpm_coop" && record.entry.mapName == "strike_at_karkand"
              && record.entry.gameVariant == "bf2" && record.entry.gameVersion == "1.5.3153-802.0" && record.entry.ranked && record.entry.dedicated
              && !record.entry.antiCheat && record.entry.plasma && record.entry.reservedSlots == 4 && record.entry.fps == 35.5f && record.entry.field140 == 2
              && !record.entry.password && record.entry.os.empty() && record.details.team1 == "US" && record.details.team2 == "team_2"
              && record.details.noVehicles && record.details.globalUnlocks && !record.details.friendlyFire && record.details.players.size() == 2
              && record.details.players[0].name == "alice" && record.details.players[0].pid == 7 && record.details.players[0].team == "1"
              && record.details.players[0].score == 10 && record.details.players[1].name == "bot1" && record.details.players[1].aiBot == 1,
              "fillRecord reads the standard, bf2_ and player keys of a server with full keys, without a query");
        check(lan->fillRecord(server, &record, true, true) && record.details.players.size() == 3 && record.details.players[2].name == "carol"
              && record.details.players[2].deaths == 3, "the coop rule reads the caller's record's game mode: the bot extends the loop the second time");
        ui::ServerBrowserRecord basic;
        check(lan->fillRecord(server, &basic, false, true) && basic.entry.hostName == "Fixture server" && basic.details.team1 == "US"
              && basic.details.players.size() == 3 && ServerBrowserFixture::suppressed(lan) == false,
              "a basic fill copies the static record's details from the last full fill (as shipped)");
        lan->sort(std::string("hostname"), true, 1);
        check(records->records.size() == 1 && records->records[0].server == server && records->revision == revisionAfterAppend + 2
              && ServerBrowserFixture::sortKey(lan) == "hostname", "sort rebuilds the results from the SDK's list: one record, the revision moved twice (clear, add)");
        const int revisionAfterSort = records->revision;
        check(lan->getServer(std::string("10.1.2.3"), 29900) == server && !lan->getServer(std::string("10.1.2.4"), 29900) && !ServerBrowserFixture::suppressed(lan),
              "getServer by address and port");
        ui::ServerBrowserRecord* listed = lan->getRecord(std::string("10.1.2.3"), 29900, true, true);
        check(listed != 0 && listed == &records->records[0] && listed->entry.hostPort == 16000 && listed->details.players.size() == 3 && !ServerBrowserFixture::suppressed(lan),
              "getRecord refreshes the listed record in place (the coop rule now sees the record's own game mode)");
        check(!lan->getRecord(std::string("10.1.2.4"), 29900, true, true) && !ServerBrowserFixture::suppressed(lan), "an unknown address has no record");
        check(lan->removeAddress(std::string("10.1.2.3"), 29900) && records->records.empty() && records->revision == revisionAfterSort + 1
              && !lan->getServer(std::string("10.1.2.3"), 29900), "removeAddress drops the SDK's server and, through the deleted callback, its record");
        check(lan->requeueRecords() && records->revision == revisionAfterSort + 2 && lan->getRecords()->records.empty(), "requeueRecords moves the (empty) results to the queue");
        lan->reset();
        check(records->revision == revisionAfterSort + 3 && !lan->isBrowsing(), "reset clears both lists and the SDK");
        lan->stopUpdate();
        check(records->revision == revisionAfterSort + 4 && records->records.empty(), "stopUpdate with a sort key sorts again over the now empty SDK list");
        favourites->reset(); history->reset();
        check(true, "the profile loader's favourites and history reset runs (a no-op for a list without a handle)");
        manager->clearLists();
        check(manager->getListCount() == 0 && !host->getServerList(std::string("inet")) && !manager->isAnyBrowsing(), "clearLists empties the manager (the lists are freed without their destructors, as shipped)");
        {
            // [TEMP] Observe only the production engine's entry and first virtual call;
            // rendering, input pumping and queued menu commands are outside this check.
            TrapMenuLogic trap;
            MenuFrameGlobalsRestore restore;
            const int modes[] = { -1, 0, 1, 2, 7 };
            for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
                menuLogic = 0;
                io::g_falconMouseMode = modes[i];
                engine->renderMenu(3, 0.25f, 0.375, false);
                check(io::g_falconMouseMode == modes[i], "renderMenu without a menu preserves Falcon mode");
                menuLogic = &trap;
                bool reachedUpdate = false;
                try { engine->renderMenu(3, 0.25f, 0.375, (i & 1) != 0); }
                catch (const MenuUpdateReached&) { reachedUpdate = true; }
                check(reachedUpdate && trap.updates == i + 1 && trap.seenDelta == 0.375
                      && trap.seenMode == (modes[i] == 2 ? 2 : 1)
                      && io::g_falconMouseMode == trap.seenMode,
                      "renderMenu preserves mode 2, otherwise writes 1 before update, forwarding wall delta");
            }
        }
    }
    std::printf("Client startup: %u/%u checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
