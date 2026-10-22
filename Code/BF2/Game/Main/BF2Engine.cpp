// Path: attested by assertions and the Game_Main_BF2Engine.cpp anonymous namespace.
// Contents: recovered from the symbolized ELF. Local variable names are [INFERRED].
// Coordinates startup, the frame loop and shutdown. Preserve the explicit retail Debug line numbers.

#include "BF2Engine.h"
#include "BF2EngineSetup.h"
#include "BF2Log.h"                         // [INFERRED] path; Game/Main/BF2Log.cpp is attested
#include "BF2Console.h"                     // [INFERRED] path
#include "IFSMoviePlayer.h"                 // [INFERRED] path; the renderer plugin's movie player (checkMovie, the mainLoop movie arm)
#include "../../Menu/ISwiffHost.h"          // the typed front-end host and MenuEvent
#include "../../IO/Input/Input.h"           // io::IKeyboardDevice - the movie arm's escape poll
#include "../../IO/Input/FalconDevice.h"    // client PE978FC4, the menu's mouse-mode global
#include "../../Core/System.h"              // [INFERRED] path; Core/System.cpp attested (anon-ns)
#include "../../Core/EventManager.h"        // [INFERRED] path; Core/EventManager.cpp attested
#include "../../Core/IProfilerClient.h"     // [INFERRED] path
#include "../../Core/DebugDraw.h"           // [INFERRED] path AND type name - see the header
#include "../../Core/ClassManager.h"
#include "../../Core/SettingsRepostitory.h"
#include "../../Core/SystemInfo.h"          // ISystemInfo + systemInfo(); replaces a local shim
                                            // whose vtable slot 0 was wrong - see the note below
#include "../../Core/DebugCounterManager.h" // the per-frame update dispatched from mainLoop
#include "../IModInfo.h"                    // real IModInfo - replaces a shim whose slot 0 was wrong
#include "../SingletonServer.h"
#include "../../IO/File/FileChangeManager.h" // io::FileChangeManager + the fileChangeManager global
#include "../IGame.h"                       // [INFERRED] path
#include "../ControlSettings.h"             // IControlSettings::setMappingInput - the (10,12) handler
#include "../GlobalSettings.h"              // IGlobalSettings::getNamePrefix - the clan tag applyProfileNameSettings reads
#include "../IGameServer.h"                 // recovered secondary interface; startServer is +0x1C
#include "../IGameClient.h"
#include "../IGameLogic.h"                  // [INFERRED] path
#include "../../Core/Thread.h"              // [INFERRED] path; Thread::Priority is nested
#include "Game.h"                           // isDedicated() / setDedicated() - the image flag
#include "../../IO/SettingsManager.h"       // [INFERRED] path; the class is SettingsManagerBase
#include "../../IO/Console/Console.h"       // io::OldConsole, io::Console::ignoredString_
#include "../../IO/File/FileManager.h"      // io::fileManager
#include "../Objects/IObject.h"             // world::Object
#include "StatusMonitor.h"                  // [INFERRED] path
#include "../../Core/LoadStat.h"
#include "../GameServer/ServerSettings.h"
#include "../../Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "../../Scene/Object/ObjectManager.h"        // world::ObjectManager + the objectManager global
#include "../../Animation/BoneAnimation/RagDollTemplate.h"   // dice::anim::RagDollTemplate singleton
#include "../Python/PythonHost.h"           // python::pythonHost(); pulls <Python.h> - see the note
                                            // on BF2Engine.cpp's line in Code\Tests\syntaxcheck.cmd,
                                            // which must pass %pyinc% for this file.
#include "../../Core/StringUtil.h"                // compareIgnoreCase
#include "../../Core/Registry.h"
#include "../../IO/Localization/ExcelLexicon.h"
#include "../Utility/SimpleParser.h"
#include "../Objects/Game/SupplyDropManager.h"   // shutdown: Instance() / Release()        // [INFERRED] path - promoted to its own header
#include "../MapList.h"                     // mapList - region 1 owns its config-file default
#include "../Utility/Vars.h"                // Vars::getFloat - the frame limiter's overhead var
#include "../../IO/TickCalculator.h"
#include "Demo.h"                           // [INFERRED] header path; demo.cpp itself is attested
#include "../../Scene/Object/PlayerManager.h" // the real PlayerManager - replaces the local shim
#include "../Objects/IPlayer.h"              // per-player handleFrameUpdate loop
#include "../Objects/ICameraObject.h"       // mainLoop's held camera: getCamera / setInterpolatedMode

#include <list>
#include <string>
#include <thread>       // std::this_thread::sleep_for - the mainLoop idle throttle (POSIX usleep)
#include <chrono>       // std::chrono::milliseconds
#include <sstream>      // handleEvent case 9's ServerSettings::save target
#include <numeric>      // std::accumulate - getAverageFPS
#include <cmath>        // floor - getAverageFPS's half-up rounding
#include <utility>      // std::make_pair - the case-9 collector rows

#include "../DemoRecorder/DemoConfig.h"     // playDemo's config-from-name branch
#include "BF2.h"                            // handleEvent case 16 -> BF2::handleInGameWinMessages
#include "BF2FrameInterpolator.h"           // +0x1C client-only object; ctor real, three methods remain [FRONTIER]
#if !DEDICATED
#include "../../RendMisc/IRenderView.h"
#include "../../RendMisc/IRenderer.h"
#include "../HudInformationLayer.h"
#include "../Objects/Fx/PersistentExplosionEffectManager.h"
#endif
#include "../../Sound/BfSound.h"            // IBfSound* g_bfSound - the client's menu/movie arms
#include "../../IO/Input/Input.h"           // io::IInputManager / IInputDeviceManager - the client's input arms
#include <ctime>                            // time / difftime - sessionStartTime
#include <cstring>                          // memcpy - updateInput's sample copy
#include <cstdio>                           // std::fprintf - the [FRONTIER] stop in startGame's client branch
#include <cstdlib>                          // std::abort   - the same stop
#if !DEDICATED
#include "BF2ClientSeams.h"                 // [FRONTIER] client collaborators without recovered classes
#include "../LocalProfileManager.h"        // native client startup interface
#include "../ProfileManager.h"             // renderMenu case 14 clears the pending transition
#include "../../Menu/BF2HudManager.h"       // constructor and established virtual prefix; publication gated
#include <cstdio>
#include <cstdlib>
#include "../../Core/Ids.h"                 // the menu CIDs / IID_IMenuLogic / IID_IFSMoviePlayer
#include "BF2Window.h"                      // the game window (brings <windows.h>)
#include "../../Menu/IMenuLogic.h"          // menuLogic
#include "../../Sound/BfSound/MusicPlayer.h"
#include "../../IO/Input/PlayerInput.h"     // updateInput's per-tick copy
#include <shellapi.h>                       // ShellExecuteA - restartBF2 / activateMod / browseModHomepage
#endif

// Middleware attribution functions have global scope. Preserve their startup call order.
void openAlCreditNotice();
void oggVorbisCreditNotice();
void pcreCreditNotice();
void pmeCreditNotice();
void zlibCreditNotice();

namespace dice {
namespace hfe {

// Singleton bring-up helpers called for their side effects.
class ISettingsRepostitory { public: static void createSingleton(); };
// The engine singleton is published by its constructor, before init() runs.
extern BF2Engine* bf2Engine;

// Hang watchdog: samples the server-touch count every 30 seconds and deliberately
// crashes if it stops advancing. Skips the first comparison. Separate mutexes protect
// shutdown and the counter; the shutdown branch releases its lock after leaving the loop.
class CheckServerAliveThread : public Thread
{
public:
    explicit CheckServerAliveThread(BF2Engine*);
    virtual void run();                             // Thread vtable +0x08

private:
    /* +0x0C */ int        m_lastTouchedCount;      // ctor: 0
    /* +0x10 */ bool       m_firstCheck;            // ctor: 1 - see above
    /* +0x14 */ BF2Engine* m_engine;
};

// Dedicated images have no active menu and return false.
bool BF2Engine::getMenuActive() const
{
#if DEDICATED
    return false;                                       // : the menu object does not exist here
#else
    return menuLogic != 0;                              // BF2.exe 0x403130
#endif
}

// [CHOSEN] accessor with getMenuActive's shape: the movie player is a client-only member, and
// mainLoop tests it in four places that must fold to nothing in a dedicated build.
bool BF2Engine::isMoviePlaying() const
{
#if DEDICATED
    return false;
#else
    return moviePlayer != 0;
#endif
}

// ctor 0x812E760: chain to Thread, install this class's vptr, then the three fields.
CheckServerAliveThread::CheckServerAliveThread(BF2Engine* engine)
    : m_lastTouchedCount(0)
    , m_firstCheck(true)
    , m_engine(engine)
{
}

// [FRONTIER] CheckServerAliveThread::run remains unwritten; the recovered watchdog algorithm
// requires separate shutdown/heartbeat locks and deliberately faults when the heartbeat stops.
void CheckServerAliveThread::run()
{
}

// File-scope helpers, all attested in the ELF symbol table. See the notes further down.
void bf2ExitCallback(void*);                    // _ZN4dice3hfe15bf2ExitCallbackEPv    0x8138B50
// Sends an unthrottled watchdog heartbeat for each load increment. Does nothing
// when the engine is absent; this callback does not repaint loading progress.
void atIncLoadCallback();                       // _ZN4dice3hfe17atIncLoadCallbackEv   0x812E8D0
void atIncLoadCallback()
{
    if (bf2Engine)
        bf2Engine->markAsStillRunning();
}
extern EventManager*         eventManager;   //  = _ZN4dice3hfe12eventManagerE
extern SettingsRepostitory*  settings;       // Shared settings repository.
                                             // Profiler singleton uses the IProfilerClient interface type.
extern IProfilerClient*      profilerClient;
extern ClassManager*         classManager;   //  = _ZN4dice3hfe12classManagerE
extern System*               g_system;       //  = _ZN4dice3hfe8g_systemE. The name is
                                             //   g_system, not system: _ZN4dice3hfe6systemE does not
                                             //   exist. Its TU-mates are g_-prefixed too
                                             //   (g_startTime/g_frequency/g_oldTime, 0x8AC0D28..50)
extern ServerSettings*       serverSettings;

//  = _ZN4dice3hfe4gameE (the name is attested). IDA types this global GameServer*, but
// it is registered under IID_IGame and Demo::stop() takes IGame* (0x8141C00,
// _ZN4dice3hfe4Demo4stopEPNS0_5IGameE), so IGame* is the source-level type.
extern IGame*                game;
extern IGameLogic*           gameLogic;      // written only by setGameLogic()

// Separate EventManager instance for frame events. [UNRESOLVED] Whether other
// behavior requires this separation; retain both singleton instances.
extern EventManager*         frameEventManager;

namespace io { extern OldConsole* mainConsole; }

//  = _ZN4dice3hfe5world13objectManagerE (attested).
// objectTemplateManager is used by shutdown() through vtable +0x38, loaded at 0x8131D94.
namespace world { extern ObjectManager* objectManager; }
namespace world { extern ObjectTemplateManager* objectTemplateManager; }

// These ID declarations must retain const to match their MSVC symbol definitions.
extern const unsigned int IID_IGame;
extern const unsigned int IID_IGameServer;
extern const unsigned int IID_IGameClient;
extern const unsigned int CID_GameServer;
extern const unsigned int CID_GameClient;
extern const unsigned int CID_ServerGameLogic;
extern const unsigned int CID_ClientGameLogic;

// dice::hfe::setGameLogic(unsigned int) - a free function, NOT a BF2Engine method. Releases the
// current gameLogic and createInstance2(classId, IID_IGameLogic) into the global.
IGameLogic* setGameLogic(unsigned int classId);

// [INFERRED] DebugDraw names an empty allocation owned by engine init/shutdown.
// Shutdown frees it without clearing the global, so repeated shutdown is not supported.
extern DebugDraw* debugDraw;   // [INFERRED] type

// ds:0x8AC86EC. Declared IBase* to match Core/Globals.cpp exactly - the declared type is part of the
// MSVC symbol, and the singleton registry stores an IBase and never dereferences through it. The
// mainLoop call site casts to the real class, which is the same address.
extern IBase* debugCounterManager;

namespace world {
extern PlayerManager* playerManager;   // ds:0x8B1B928
}

// The per-frame visual allocator arena boundary. _ZN4dice3hfe6memory15nextVisualFrameEv, 0x8050B70.
//  It is five bytes in this build - `push ebp; mov ebp,esp; pop ebp; retn`, i.e. empty. The arena
// switching it implies is not present in the dedicated-server binary. Called anyway, because the
// call is what the frame does and a future build may fill it in.
namespace memory { void nextVisualFrame(); }

// [CHOSEN, not recovered] Empty visual-frame boundary pending the allocator module's
// implementation. Callers retain the once-per-frame boundary.
namespace memory { void nextVisualFrame() { } }   // placeholder - see above

namespace io { extern IBase* inputManager; extern IBase* inputDeviceManager; }   // Core/Globals.cpp defines both as IBase*

namespace {
#if !DEDICATED
// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: music-event-trace]
// Traces the music event poster only when BF2_TRACE_MUSIC is set; retire after
// the event-poster sequence is validated.
void musicTraceSite(const char* site)
{
    static int armed = -1;
    if (armed < 0) armed = std::getenv("BF2_TRACE_MUSIC") ? 1 : 0;
    if (armed) { std::fprintf(stderr, "[MUSIC] event 0 from %s\n", site); std::fflush(stderr); }
}
bool s_menuMusicHold        = true;    // byte_96E340: activateMenu skips the menu-music start once
bool s_cursorShown          = true;    // byte_96E3A3: mainLoop hides the cursor once, when a movie starts
bool s_activateMenuOnFrame1 = true;    // byte_96E3A4: mainLoop's first-frame login + activateMenu

//  / 0x403060 - the menu-pause bookkeeping. `this` in the binary is the heap int the ELF
// calls dwordD0 (savedPauseState), NOT the engine; -1 means "we did not pause". [INFERRED] names.
void restoreMenuPause(int& state)
{
    if (game)
    {
        if (state < 0)
            return;
        if (game->isPlayingDemo())                                  // vslot +0xDC (ELF +0xE0)
        {
            if (IBase* client = static_cast<IBase*>(game->queryInterface(IID_IGameClient)))
                static_cast<IGameClient*>(client)->setPause(state != 0);
        }
        else if (gameLogic)
        {
            gameLogic->setPause(state != 0);                        // +0x220
            state = -1;
            return;
        }
    }
    state = -1;
}

void updateMenuPause(int& state, bool menuUp)
{
    if (!game) { restoreMenuPause(state); return; }
    IBase* client = static_cast<IBase*>(game->queryInterface(IID_IGameClient));   // vslot +0x0C
    if (!game) { restoreMenuPause(state); return; }                 // the binary's own re-test
    if (!gameLogic || !unresolvedGlobalA33C08)
    {
        if (!client || !game->isPlayingDemo())
        {
            restoreMenuPause(state);
            return;
        }
    }
    const bool playingDemo = game->isPlayingDemo();
    bool wantPause;
    if (menuUp && (!gameLogic || gameLogic->getGameStatus() != 2))  // +0x158
        wantPause = true;
    else
        wantPause = playingDemo && client && static_cast<IGameClient*>(client)->isDemoAtEnd();   // +0x68
    if (wantPause == (state >= 0))
        return;                                                     // nothing to change
    if (!wantPause)
    {
        restoreMenuPause(state);
        return;
    }
    if (playingDemo)
    {
        state = static_cast<IGameClient*>(client)->getPause() ? 1 : 0;   // +0x64
        static_cast<IGameClient*>(client)->setPause(true);               // +0x60
    }
    else
    {
        state = gameLogic->getPause() ? 1 : 0;                      // +0x224
        gameLogic->setPause(true);                                  // +0x220
    }
}

// The player-name / clan-tag settings block that BF2.exe inlines identically into joinServer
// (0x405C10) and startMission (0x406010) - [CHOSEN] factored once. GlobalSettings' +0x28 is asked
// up to three times, as shipped.
void applyProfileNameSettings()
{
    IBase* lpm = classManager->getSingleton("LocalProfileManager");
    IBase* pm  = classManager->getSingleton("ProfileManager");
    IBase* gs  = classManager->getSingleton("GlobalSettings");
    std::string name;
    if (!static_cast<IProfileManager*>(pm)->getUniqueNick(name))
        name = static_cast<ILocalProfileManager*>(lpm)->getName();
    settings->stringSet("GSPlayerNameNoClanTag", name);                    // vptr+0x44
    IGlobalSettings* globalSettings = static_cast<IGlobalSettings*>(gs);   // three +0x28 calls, as shipped
    if (globalSettings->getNamePrefix().empty())
    {
        settings->stringSet("GSPlayerName", name);
        settings->stringSet("GSPlayerClanTag", std::string(""));
    }
    else
    {
        settings->stringSet("GSPlayerClanTag", globalSettings->getNamePrefix());
        settings->stringSet("GSPlayerName", globalSettings->getNamePrefix() + " " + name);   // [INFERRED] operand order
    }
}
#endif // !dedicated
} // namespace

// [CHOSEN, not recovered] Editor images skip the game instance mutex so BF2.exe
// can run alongside the editor. Game images retain the mutex behavior.
#if defined(_WIN32) && !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
#  define BF2_ENGINE_INSTANCE_MUTEX 1
#else
#  define BF2_ENGINE_INSTANCE_MUTEX 0
#endif
SingleInstanceGuard::SingleInstanceGuard()
    : isFirstInstance(false)
{
#if BF2_ENGINE_INSTANCE_MUTEX
    handle = CreateMutexA(0, 0, "Battlefield2_Instance_Mutex");
    isFirstInstance = GetLastError() != ERROR_ALREADY_EXISTS;      // 183
#elif defined(_WIN32) && !DEDICATED
    handle = 0;                                                     // the editor: no guard, never a second instance
    isFirstInstance = true;
#endif
}

SingleInstanceGuard::~SingleInstanceGuard()
{
#if BF2_ENGINE_INSTANCE_MUTEX
    ReleaseMutex(handle);                                           // not CloseHandle - as shipped (0x4083F0)
#endif
}
#undef BF2_ENGINE_INSTANCE_MUTEX

BF2Engine::BF2Engine(BF2* bf2Ptr)
    : bf2(bf2Ptr)                       // +0x08  -  the constructor's only argument
    , bf2EngineSetup(0)                 // +0x0C
    , window(0)                         // +0x10 - the game window; a client-only object (BF2Window.h)
    , bf2Console(0)                     // +0x14
    , bf2Render(0)                      // +0x18
    , frameInterpolator(0)              // +0x1C  BF2FrameInterpolator - created by init() on the client only
    , statusMonitor(0)                  // +0x20   null until the END of initEngine  -  this is why
                                        //        updateStatusMonitor null-checks every frame
    , frameTime(0.0f)                   // +0x24
    , framesIndex(0)                    // +0x38  `mov dword ptr [edi+38h], 0` (0x812EC68 / 0x812E9D8).
    , dword3C(0)                        // +0x3C
    , dword40(0)                        // +0x40
    , quitNextFrameFlag(false)          // +0x44
    , menuActive(true)                  // +0x50   starts true
    , byte51(false)                     // +0x51
    , byte52(true)                      // +0x52  the mainLoop gate
    , byte53(true)                      // +0x53
    , dword54(0)                        // +0x54
    , bf2Log(0)                         // +0x58
    , byte5C(false)                     // +0x5C
    , lockFPS(100)                      // +0x60   0x64
    , byte64(false)                     // +0x64
    , dword6C(0)                        // +0x6C
    , byte70(true)                      // +0x70
    , performanceLogging(0)             // +0x74
    , byte78(false)                     // +0x78
    , double7C(-1.0)                    // +0x7C   NOT zero  -  see above
    , dword84(0)                        // +0x84 / +0xA0
    , dword88(0)
    , dword8C(0)
    , dword90(0)
    , dword94(0)
    , dword98(0)
    , dword9C(0)
    , dwordA0(0)
    , dwordA4(-1)                       // +0xA4  , constructor-only sentinel
    , musicPlayer(0)                    // +0xAC   - created by the client's initEngine only
    , dwordB0(0)                        // +0xB0  . Required before startGame's first
                                        //        `delete dwordB0`; omitting this attested store
                                        //        caused intermittent invalid frees in production.
    , checkServerAliveThread(0)         // +0xC0
    , byteC4(false)                     // +0xC4
    , touchedServerCount(0)             // +0xC8
    , instanceGuard(new SingleInstanceGuard())   // +0xCC  /0x812EDC8 (new(1) there; new(8) + CreateMutexA in BF2.exe)
    , savedPauseState(new int(-1))      // +0xD0
{
    // Scalar fields named by offset retain [UNRESOLVED] meanings. Construct classManager
    // before registering settings or other singletons.
    tickCalculator = new TickCalculator(10, 30.0f);       // +0x68, operator new(0x20)
    demo           = new Demo();                          // +0xAC, operator new(0x14)
    classManager   = new ClassManager();                  // > ds:0x8AC85A0

    // Preserve middleware attribution calls during construction.
    openAlCreditNotice();
    oggVorbisCreditNotice();
    pcreCreditNotice();
    pmeCreditNotice();
    zlibCreditNotice();

    // the engine singleton, live before init() runs
    bf2Engine = this;                                     // > ds:0x8AACBD8

    // 100-sample FPS history
    frames.resize(100, 0.0f);

#if !DEDICATED
    // BF2.exe 0x408190 continues past the shared tail: the HUD-manager slot is zeroed, then the two
    // client-only pointers (+0xF0, +0xF4 - the movie list at +0xF8 is compiler-constructed in member
    // order above), then the session clock starts. None of it exists in the dedicated ctor.
    hudManager  = 0;                                         // dword_A10890
    dwordF0     = 0;
    moviePlayer = 0;
    time(&sessionStartTime);                                 // +0x12C
#endif
}

bool BF2Engine::init(const std::string& commandLine)
{
    bf2Log = new BF2Log();                              // operator new(0x20) == 32 bytes

    ISettingsRepostitory::createSingleton();
    initDefaultSettings();

    if (!parseParameters(commandLine))
        return false;

#if !DEDICATED
    // BF2.exe 0x40C190, straight after parseParameters: the single-instance gate. The guard's flag
    // is CreateMutexA("Battlefield2_Instance_Mutex") not reporting ERROR_ALREADY_EXISTS; a second
    // instance may only continue when GSAllowMultipleInstances is set. Neither dedicated image has
    // this test (their guard is a 1-byte placeholder).
    if (!instanceGuard->isFirstInstance)
    {
        if (!settings->boolGet("GSAllowMultipleInstances", false))    // vptr+0x50
            return false;
    }
#endif

    bf2EngineSetup = new BF2EngineSetup(this);          // operator new(0x10) == 16 bytes
    bf2Console     = new BF2Console();                  // operator new(8)    ==  8 bytes
    if (isDedicated())                                  // BF2.exe 0x40C190: `if (sub_440510()) lockFPS = 60` - the
        lockFPS = 60;                                   //   dedicated images keep the unconditional store

    // Clients without a debugger clear the ignore-asserts flag. Dedicated images
    // still query debugger presence even though they do not perform that store.
    if (!systemInfo()->getIsDebuggerInstalled() && !isDedicated())   // call [vptr+0x1C]
        Vars::setValue("hack-ignore-asserts", "0");

#if !DEDICATED
    if (!isDedicated())                                 // BF2.exe 0x40C190: the client's own objects, before initModules
    {
        frameInterpolator = new BF2FrameInterpolator();                       // operator new(0x2C), 0x45BF10
        hudManager = new hud::BF2HudManager();                               // operator new(864), 0x7564A0
        // PE40C2D8: publish the constructed manager before initModules. Its widgets
        // are created later by initEngine -> BF2HudManager::init. The owner permits
        // diagnostic startup with remaining dependencies that stop when reached
        // do not gate registration on unrelated later HUD behavior.
        {
            SingletonRegInfo sri = { std::string("HudManager"), reinterpret_cast<IBase**>(&hudManager), 0, 0, 4 };
            classManager->registerSingleton(sri);                             // vptr+0x44
        }
        // [CHOSEN, not recovered] VoIP construction is omitted.
        // There should be BF2VoipClient construction (PE 0x4519B0) and registration
        // of "Bf2VoipClient" here. Keep the singleton absent until voice is wanted.
        // [RETIRE-WHEN: client-voip-optional] See debt RW-94.
    }
#endif

    if (!bf2EngineSetup->initModules())                 // /0x8131457 - an instance call on
    {                                                   // the field at +0x0C: `mov eax,[edi+0Ch];
        bf2EngineSetup->shutdownModules();              // mov [esp],eax; call ...initModules`
        return false;
    }

    initSettingsRepostitory();

    // BF2.exe 0x40C190: the client registers itself as an input filter (BF2Engine is the
    // io::IInputFilter at +0x00; ds:0x9FFCCC is io::inputManager, vslot +0x20). Folds away on a server.
    if (!isDedicated())
        static_cast<io::IInputManager*>(io::inputManager)->registerInputFilter(this);

    // Pass the adjusted IEventListener base pointer to each event registration.
    eventManager->registerEventHandler((EventCategory)5,  this, 0);
    eventManager->registerEventHandler((EventCategory)10, this, 0);
    frameEventManager->registerEventHandler((EventCategory)10, this, 0);
    eventManager->registerEventHandler((EventCategory)11, this, 0);

    // ECHud == category 2. The demo recorder listens so it can capture HUD events.
    if (!demo || !eventManager || !eventManager->registerEventHandler((EventCategory)2, demo, 100))
    {
        Debug(DEBUGTYPE_6, "Game/Main/BF2Engine.cpp", 767)
            << "Failed to register Demo as event handler for ECHud.";
    }

#if !DEDICATED
    if (!isDedicated())                                 // BF2.exe 0x40C190: the window and the renderer bridge
    {
        window    = new BF2Window();                    // operator new(0x3C), 0x45D770
        bf2Render = new BF2Render(this);                // operator new(0x20), BF2Render_ctor
    }
#endif

    if (!bf2EngineSetup->initLateModules())             // /0x81315BF, instance call on +0x0C
    {
        bf2EngineSetup->shutdownModules();
        return false;
    }

    if (!initEngine())
    {
        shutdownEngine();
        return false;
    }

    // BF2.exe 0x40C190: a `+playerName` from the command line becomes the setting once the engine is
    // up. The client reads m_playerName's size word (+0x60) directly; both dedicated images fold the
    // whole statement away behind isDedicated().
    if (!isDedicated() && !m_playerName.empty())
        settings->stringSet("GSPlayerName", m_playerName);          // vptr+0x44


    io::mainConsole->setExitCallBack(bf2ExitCallback, this);

    // boolGet is _ZN4dice3hfe19SettingsRepostitory7boolGetERKSsRKb (0x83CC8B0) - it takes the default
    // BY const reference and returns a const bool&; it is not an out-parameter API. 0x813164B
    // materialises the `false` temporary, 0x8131656 is `call [ecx+50h]`, and 0x813165B does
    // `movzx eax, byte ptr [eax]` on the returned reference.
    if (settings->boolGet("GSFileChangeMonitor", false))    // vptr+0x50
        io::fileChangeManager->setEnable(true);             // +0x1C

    // BF2.exe 0x40C190: the two default control maps (io::inputManager vslots +0x2C / +0x28, the
    // names passed BY value). Folds away on a server.
    if (!isDedicated())
    {
        static_cast<io::IInputManager*>(io::inputManager)->setPlayerControlMap(std::string("defaultPlayerInputControlMap"));
        static_cast<io::IInputManager*>(io::inputManager)->setGameControlMap(std::string("defaultGameControlMap"));
    }


    io::mainConsole->loadCommandHistory("Logs/BfCommandHistory.con");   // OldConsole vptr+0xAC

    // Client sound scripts run at normal console access; dedicated images omit them.
    if (!isDedicated())
    {
        io::mainConsole->run("Settings/Sound.con", "", "", "", "", "", "", "", "", io::Console::ignoredString_);
        io::mainConsole->run("Menu/MenuSounds.con", "", "", "", "", "", "", "", "", io::Console::ignoredString_);
    }

    io::mainConsole->runFullAccess("Init.con", "", "", "", "", "", "", "", "",
                                   io::Console::ignoredString_);   // +0x2C

    // [CHOSEN, not recovered] PunkBuster initialization and its console-command hook
    // are omitted. Commands therefore run without the SDK's pre-dispatch filter.

    debugDraw = new DebugDraw;                      // operator new(1), 0x813195D-0x8131962

    if (isDedicated() && !checkServerAliveThread)       // BF2.exe 0x40C190: `if (sub_440510() && !*(this+70))`
    {
        checkServerAliveThread = new CheckServerAliveThread(this);   // operator new(0x14) == 20
        if (checkServerAliveThread)
        {
            // An instance call on the field at +0xC0, and both arguments are real:
            //  `mov eax,2` -> [esp+8], 0x81319C0 the literal "CheckServerAliveThread" ->
            // [esp+4], 0x81319C9 `mov eax,[edi+0C0h]` -> [esp]. Signature attested:
            // _ZN4dice3hfe6Thread5startEPKcNS1_8PriorityE (0x83D2EF0).
            checkServerAliveThread->start("CheckServerAliveThread", (Thread::Priority)2);
        }
        // NON-static, ONE argument; `this` comes from ds:0x8ABBC98 = _ZN4dice3hfe8loadStatE.
        // Placement is right: 0x8131995 `jnz 0x81319B7` runs start() then `jmp 0x813199F`, so the
        // registration runs on both paths inside this `if`.
        loadStat->registerAtIncLoadCallback(atIncLoadCallback);
    }

    return true;
}

// Shuts down the game and engine services.
// [INFERRED] Boolean return type; current callers discard the always-true result.
bool BF2Engine::shutdown()
{
    // 1. Stop the server-liveness watchdog. byteC4 is the flag its loop polls; it is raised under
    //    mutex1 so the thread observes it before Thread::stop() waits on the join.
    if (checkServerAliveThread)
    {
        mutex1.lock();                  // _ZN4dice3hfe5Mutex4lockEv (0x83D2DA0), direct call at
        byteC4 = true;                  //    on this+0xB8 (`lea eax,[edi+0B8h]` 0x8131E67)
        mutex1.unlock();                // : `mov byte ptr [edi+0C4h], 1`
                                        // Release the watchdog-state mutex before stopping the thread.

        checkServerAliveThread->stop();             // non-virtual, 0x8131E93. _ZN4dice3hfe6Thread4stopEv
                                                    //   (0x83D2F40) takes NO arguments; the call site
                                                    //   pushes only `this` (`mov eax,[edi+0C0h]`)
        if (checkServerAliveThread)
            delete checkServerAliveThread;          // vtable +4 - Itanium deleting destructor

        byteC4 = false;
        checkServerAliveThread = 0;
    }

    // 2. Frees init()'s one-byte allocation. No null check is emitted and the global is NOT cleared
    //    afterwards, so a second shutdown() would double-free. Kept exactly as recovered.
    operator delete(debugDraw);                     // 0x8131B35, bare - no null check

    // 3. Unhook the demo recorder from ECHud (category 2). unregisterEventHandler is EventManager
    //    vtable +0x30 and returns bool. The demo object is passed directly here; the engine's own
    //    handler is `this`, which converts to the IEventListener base subobject at +4.
    if (!demo || !eventManager || !eventManager->unregisterEventHandler((EventCategory)2, demo))
    {
        Debug(DEBUGTYPE_6, "Game/Main/BF2Engine.cpp", 882)
            << "Failed to unregister Demo as event handler for ECHud.";
    }

    // 4. Persist what init()/initEngine() loaded.
    io::mainConsole->saveCommandHistory("Logs/BfCommandHistory.con");   // OldConsole vtable +0xB0
    io::settingsManager->save("Settings/Usersettings.con", io::VariableSet(0));   // non-virtual

    // 5. Unregister the engine's own handler. note the order: this is registration order, not the
    //    reverse - byte-for-byte the same sequence init() registers, with register swapped for
    //    unregister. Reversal would be 11, frame-10, 10, 5. Reproduced as emitted.
    eventManager->unregisterEventHandler((EventCategory)5,  this);
    eventManager->unregisterEventHandler((EventCategory)10, this);
    frameEventManager->unregisterEventHandler((EventCategory)10, this);
    eventManager->unregisterEventHandler((EventCategory)11, this);

    shutdownGame(true);                 // the bool argument is 1 (0x8131d2c-0x8131d31)

    // 6. World teardown. None of this has a counterpart in init() or initEngine(): mainTerrain and
    //    the object templates are created by level loading, so shutdown() is doing double duty as
    //    an engine teardown and a world teardown.
    world::IObject* terrain =
        world::objectManager->getObjectFromNameHash("mainTerrain");     // vtable +0x24
    if (terrain)
        terrain->release();                                            // vtable +0x8

    world::objectTemplateManager->deleteAll(false);                    // vtable +0x38, argument 0

    // Instance() is called and its result discarded, purely to force the singleton into existence
    // so that Release() has something to tear down. Faithful to the binary, odd as it reads.
    world::SupplyDropManager::Instance();
    world::SupplyDropManager::Release();

    shutdownSettingsRepostitory();      // mirrors init()'s initSettingsRepostitory()
    shutdownEngine();

    return true;
}

// Registers user-visible setting names and typed defaults. All entries use variable
// set 0. [INFERRED] The registration Boolean controls persistence.
void BF2Engine::initDefaultSettings()
{
    io::SettingsManagerBase* sm = io::settingsManager;
                                                         // Seed defaults directly in the settings repository. settingsManager is used
                                                         // separately for user-variable registration; preserve the seed ordering.
    settings->boolSet("GSMenu", false);                    // vptr+0x54, 0x812F32F - the first call
                                                           //   the function emits
    settings->stringSet("GSModDirectory",   "mods/bf2");   // +0x44
    settings->stringSet("GSLevelDirectory", "Levels/");    // +0x44

    //                              name                          save  set   default
    sm->registerUserVar("GSPerfLogAtPlayerCount",       true,  io::VariableSet(0));  // 30
    sm->registerUserVar("GSUseObjectCache",             true,  io::VariableSet(0));  // bool
    sm->registerUserVar("GSInterpolationTime",          true,  io::VariableSet(0));  // 100
    sm->registerUserVar("GSExtrapolationTime",          true,  io::VariableSet(0));  // 1200
    sm->registerUserVar("GSDefaultLatencyCompensation", true,  io::VariableSet(0));  // 0.1f
    sm->registerUserVar("GSUseLatencyCompensation",     false, io::VariableSet(0));  // bool
    sm->registerUserVar("GSUseClientSidePrediction",    false, io::VariableSet(0));  // bool
    sm->registerUserVar("GSDebugBitStream",             false, io::VariableSet(0));  // bool
    sm->registerUserVar("GSPlayerName",                 true,  io::VariableSet(0));  // "DefaultPlayerName"

    // Two more unconditional seeds, emitted between the GSPlayerName registration (0x812F895) and
    // the GSFullScreen one (0x812F9F2). Both values are recovered from the immediates.
    settings->intSet("GSViewDistance", 850);               // +0x14
                                                           //   (`mov eax, 352h` at 0x812F8FE)
    settings->boolSet("GSLoadObfuscated", true);           // +0x54
                                                           //   (`mov [ebp+var_445], 1` at 0x812F97D)

    sm->registerUserVar("GSFullScreen",                 false, io::VariableSet(0));  // bool
    sm->registerUserVar("GSExtrapolateFrame",           true,  io::VariableSet(0));  // bool
    sm->registerUserVar("GSShowNetGraph",               true,  io::VariableSet(0));  // bool
    sm->registerUserVar("GSClPunkBuster",              true,  io::VariableSet(0));  // bool

    // [UNRESOLVED] The thirteen registered variables' conditional default assignments
    // are incomplete: matching the typed Exists/Set pairs to variables is unresolved,
    // including the eight Boolean default values. Registration failure must remain visible.
}

// Parses plus-prefixed command-line options and applies their settings. Option ids
// are sparse and must retain their registered values.
bool BF2Engine::parseParameters(const std::string& commandLine)
{
    // [PLATFORM] Dedicated PE 0x404AF5 keeps the server name; client PE 0x409CBD
    // constructs "aidll.dll". Only the ELF overwrites its initial Win32 spelling.
#if DEDICATED && defined(_WIN64)
    // [CHOSEN, not recovered] Native x64 keeps the AI library name empty until a
    // compatible library exists; UseAI=false skips loading and an explicit connection
    // request retains the empty-name refusal. [RETIRE-WHEN: native-dedicated-ai-library]
    std::string aiDll;
#elif DEDICATED
    std::string aiDll("AIDLL_w32ded.dll");
#else
    std::string aiDll("aidll.dll");        // BF2.exe 0x409CBD..0x409CD6
#endif
#if !defined(_WIN32)
    aiDll = "aidll.so";                      // 0x8131F2F, std::string::assign(char const*,
                                             //   uint); strlen("aidll.so") is called at 0x8131F17
                                             //   first, which is the signature of `s = "literal"`
#endif

    // Dedicated images start with both server flags enabled, including with an empty
    // command line. Client images retain isServer=true but default dedicated=false.
    bool isServer              = true;   // var_235, 0x8131EFD
    // [PLATFORM] The client seeds only dedicated false (BF2.exe 0x409CCC); isServer
    // remains true (0x409CC8). +dedicated can still select the server runtime branch.
    bool dedicated             = DEDICATED != 0; // ELF var_237=1; client var_2=0
    bool useAI                 = false;  // var_246, 0x8131F34
    bool useEffectTextureAtlas = true;   // var_245, 0x8131F71
    int  renderPath            = 3;      // var_250, 0x8131F4A -> 0x8131F54
    int  deviceType            = 1;      // var_254, 0x8131F5D -> 0x8131F62
    int  deviceBehavior        = 80;     // var_258, 0x8131F4F -> 0x8131F6B  (edi = 0x50 = 'P')
    // Both are empty-rep constructed in the prologue (0x8131ED8/0x8131EE0 and 0x8131F3B/0x8131F43);
    // joinAddress is actually the first local the ELF constructs, before aiDll.
    std::string joinAddress;             // var_28
    std::string loadLevel;               // var_48


    std::string cmd(commandLine);            // local copy handed to parseString later
    this->commandLine.assign(commandLine);   // 0x8131F91: mov eax,[ebp+var_24C];
                                             // An empty command line skips parsing but still applies settings below.
    if (cmd.length() != 0)
    {
        SimpleParser parser('+');   // [INFERRED] form: the ctor is fully inlined. No call is emitted
        parser.add( 57, "help",                   "Displays this help");
        parser.add( 58, "?",                      "Same as +help");
        parser.add(  0, "dedicated",              "Start in dedicated server mode");
        parser.add(  1, "multi",                  "Allow starting multiple BF2 instances");
        parser.add(  8, "config",                 "Sets path to the ServerSettings.con file to use");
        parser.add( 71, "rsconfig",               "Sets path to the ReservedSlots.con file to use");
        parser.add(  9, "mapList",                "Sets the path to the MapList.con file to use");
        // The literal at ELF 0x88929E0 is exactly 97 bytes and ends mid-sentence with an
        // unterminated parenthesis - dice's own truncation, kept verbatim. One xref, 0x8132416,
        // feeding the string ctor at 0x8132422 into var_C8, passed as the 3rd add() argument at
        parser.add( 67, "pbPath",                 "Set the path to use for PunkBuster on multi-instance configurations (defaults to {install_dir}/pb");
        parser.add(  2, "joinServer",             "Join a server by ip address or hostname");
        parser.add( 66, "port",                   "specifies the network port to be used");
        parser.add(  5, "password",               "Set the server password when joining a server");
        parser.add( 46, "modPath",                "Set the mod path (default mods/bf2)");
        parser.add(  6, "checkForAvailablePatch", "");
        parser.add(  7, "checkForPatch",          "");
        parser.add( 13, "ai",                     "");
        parser.add( 20, "fullscreen",             "Start game in full screen mode");
        parser.add( 14, "wx",                     "Position game window on the screen at certain x-position");
        parser.add( 15, "wy",                     "Position game window on the screen at certain y-position");
        parser.add( 16, "szx",                    "Set resolution witdth");   // dice's typo, preserved
        parser.add( 17, "szy",                    "Set resolution height");
        parser.add( 21, "noSound",                "Start game without sound");
        parser.add( 70, "restart",                "Used when restarting executable.");
        parser.add( 72, "skipDXCheck",            "Skips DirectX version check. Use with caution.");
        parser.add( 48, "noStatusMonitor",        "Start game without status monitor screen");
        parser.add( 39, "demo",                   "Sets the con-file with demo options");
        parser.add( 11, "loadLevel",              "Set the level to load");
        parser.add(  4, "playerName",             "Set the player name");
        parser.add( 61, "playerPassword",         "Set the player password");
        parser.add( 65, "playNow",                "use playnow functionality");
        parser.add(  3, "hostServer",             "");
        parser.add( 10, "lowPriority",            "Run the game with slightly lower priority");
        parser.add( 45, "gameMode",               "Sets the game mode.");
        parser.add( 44, "maxPlayers",             "Sets max players.");
        parser.add( 59, "ranked",                 "Allows gamespy snapshot sending");
        parser.add( 73, "dropDynamicSpawns",      "Don't re-add dynamic spawn groups as round (re)starts.");

        // Only registered parser IDs reach these sparse switch cases.
        std::list<std::pair<int, std::string> > keyList;        // var_C8
        if (parser.parseString(cmd, keyList))                   // tested at 0x81333BA
        {
            for (std::list<std::pair<int, std::string> >::const_iterator it = keyList.begin();
                 it != keyList.end(); ++it)
            {
                const std::string& value = it->second;          // ebx = node + 0x0C

                switch (it->first)                              // edx = node + 0x08
                {
                case 0:     // +dedicated - see the seed note at the top of this function
                    dedicated = true;
                    break;

                case 1:     // +multi
                    // atoi(value.c_str()) != 0 - `setnz` at 0x8133F16. The c_str() is inlined as
                    // libstdc++ 3's terminator write (`movzx edx, _S_terminal; mov [data+len],dl` at
                    // ), which is what c_str() IS in that STL, not extra behaviour.
                    settings->boolSet("GSAllowMultipleInstances", atoi(value.c_str()) != 0);
                    break;

                case 2:     // +joinServer - one of only two things that clear isServer
                    isServer = false;
                    joinAddress.assign(value);
                    break;

                case 3:     // +hostServer  - value-gated: only the exact string "1" turns it on
                    if (value.compare("1") == 0)
                        isServer = true;
                    break;

                case 4:     // +playerName -> this+0x48 (0x8134005: `add eax,48h`)
                    m_playerName.assign(value);
                    break;

                case 5:     // +password
                    settings->stringSet("GSPassword", value);
                    break;

                case 8:     // +config - writes the member region 1 then finds NON-empty
                    serverSettings->m_configFile.assign(value); // this = ds:8AAB7A0
                    break;

                case 9:     // +mapList - same shape, on mapList's OWN +0x00 string
                    mapList->m_configFile.assign(value);        // this = ds:8AAAA00
                    break;

                case 11:    // +loadLevel  - an empty value is dropped, not stored
                    if (value.compare("") != 0)                 // against the "" literal
                        loadLevel.assign(value);
                    break;

                case 13:    // +ai  - value-gated on "1", exactly like +hostServer
                    if (value.compare("1") == 0)
                        useAI = true;
                    break;

                case 14:    // +wx - a Var, not a setting
                    Vars::set("opt-window-x", value);
                    break;

                case 15:    // +wy
                    Vars::set("opt-window-y", value);
                    break;

                case 16:    // +szx
                    settings->intSet("GSWindowSizeX", atoi(value.c_str()));
                    break;

                case 17:    // +szy
                    settings->intSet("GSWindowSizeY", atoi(value.c_str()));
                    break;

                case 20:    // +fullscreen
                    settings->boolSet("GSFullScreen", atoi(value.c_str()) != 0);
                    break;

                case 21:    // +noSound - passes the value through, and is redundant on a dedicated
                            // server because region 1 writes opt-nosound = "1" unconditionally.
                    Vars::set("opt-nosound", value);
                    break;

                case 39:    // +demo - three effects, and the third is easy to miss
                    settings->stringSet("GSDemo", value);
                    setMenuActive(false);
                    isServer = false;
                    break;

                case 44:    // +maxPlayers - TWO settings; the GSCommandLine* twin is what lets the
                            // command line out-rank a later ServerSettings.con load. atoi runs
                            // twice (0x81354A6, 0x8135526) - not CSE'd, reproduced.
                    settings->intSet("GSMaxNrOfPlayers", atoi(value.c_str()));
                    settings->intSet("GSCommandLineMaxNrOfPlayers", atoi(value.c_str()));
                    break;

                case 45:    // +gameMode - the same two-slot pattern
                    settings->stringSet("GSGameMode", value);
                    settings->stringSet("GSCommandLineGameMode", value);
                    break;

                case 46:    // +modPath
                    settings->stringSet("GSModDirectory", value);
                    break;

                case 48:    // +noStatusMonitor - inverted: `setz` at 0x81357BF, so the setting is
                            // GSStatusMonitor = (atoi(value) == 0). "+noStatusMonitor 1" turns the
                            // monitor OFF; "+noStatusMonitor 0" turns it ON.
                    settings->boolSet("GSStatusMonitor", atoi(value.c_str()) == 0);
                    break;

                case 57:    // +help
                case 58:    // +?     - ONE case label, and it returns false (0x8135EA3 xor eax,eax
                            // > the single retn). Region 1 never runs, so BF2Engine::init fails and
                            // the process exits after printing the help - the shipped behaviour, and
                            // the answer to this function's old "[UNRESOLVED] return value" note.
                    parser.showHelp("BF2.exe");
                    return false;

                case 59:    // +ranked - A flag: the value is never looked at (var_223 = 1 at
                            // unconditionally). "+ranked 0" still enables it.
                    settings->boolSet("GSRanked", true);
                    break;

                case 61:    // +playerPassword
                    settings->stringSet("GSPlayerPassword", value);
                    break;

                case 65:    // +playNow - an INT setting, and BF2::init tests it == 1
                    settings->intSet("playNow", atoi(value.c_str()));
                    break;

                case 66:    // +port
                    settings->intSet("GSPort", atoi(value.c_str()));
                    break;

                case 67:    // +pbPath
                    settings->stringSet("GSPunkBusterPath", value);
                    break;

                case 70:    // +restart - a flag, and it does not restart anything: the launcher
                            // passes it when it re-execs, and its only in-engine effect is movies.
                    settings->boolSet("skipMovies", true);
                    break;

                case 71:    // +rsconfig
                    reservedSlots->setConfigFile(value);        // this = ds:8AAB768
                    break;

                case 72:    // +skipDXCheck - a flag
                    settings->boolSet("GSSkipDXCheck", true);
                    break;

                case 73:    // +dropDynamicSpawns - a flag
                    settings->boolSet("dropDynamicSpawns", true);
                    break;

                // 6 (+checkForAvailablePatch), 7 (+checkForPatch) and 10 (+lowPriority) are
                // registered but share the default target 0x8133400 - they parse and do nothing.
                default:
                    break;
                }
            }
        }
    }

    // Default missing config paths under GSModDirectory. Command-line maxPlayers and
    // gameMode also set their GSCommandLine counterparts to preserve precedence.
    // [UNRESOLVED] Unregistered option 26 has no identified role.
    // [PLATFORM] The AI library name is selected for Linux, Win32 dedicated or client.
    if (serverSettings->m_configFile.empty())
        serverSettings->m_configFile =
            settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2"))
            + std::string("/settings/serversettings.con");

    if (reservedSlots->getConfigFile().empty())
        reservedSlots->setConfigFile(
            settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2"))
            + std::string("/settings/reservedSlots.con"));

    if (mapList->m_configFile.empty())
        mapList->m_configFile =
            settings->stringGet(std::string("GSModDirectory"), std::string("mods/bf2"))
            + std::string("/settings/maplist.con");

    // The settings writes, in the shipped order. Slots: intSet +0x14, stringSet +0x44,
    // boolGet +0x50, boolSet +0x54 (Core/SettingsRepostitory.h).
    settings->boolSet("GSIsServer", isServer);
    // Reads back what it just wrote, through THE repository, with `true` as the default (var_236 is
    // set to 1 at 0x8133550 and its address passed as the default). Kept rather than folded away:
    // any repository that coerces or overrides the value gets to do so before the branch below.
    isServer = settings->boolGet("GSIsServer", true);
    settings->boolSet("GSDedicated", dedicated);
#if !DEDICATED
    setDedicated(dedicated);                                     // BF2.exe 0x409CA0: sub_440520(v155) - the
                                                                 // flag's only writer; no counterpart in
                                                                 // either dedicated image. See Game.h.
#endif

    // Dedicated runtime forces sound off independently of the noSound option.
    if (isDedicated())
        Vars::set("opt-nosound", "1");

    settings->intSet("GSRenderPath", renderPath);
    settings->intSet("GSDeviceType", deviceType);
    settings->intSet("GSDeviceBehavior", deviceBehavior);
    settings->boolSet("GSUseEffectTextureAtlas", useEffectTextureAtlas);

    // Server startup clears the join address and publishes the level. Client startup
    // publishes the join address without changing GSLoadLevel.
    if (isServer)
    {
        settings->stringSet("GSJoinAddress", std::string(""));   // the "" at 0x88E24BB
        settings->stringSet("GSLoadLevel", loadLevel);
    }
    else
    {
        settings->stringSet("GSJoinAddress", joinAddress);
    }

    settings->stringSet("AIDLL", aiDll);                         // the name built at the
                                                                 //   top of this function
    settings->boolSet("UseAI", useAI);

    return true;                                                 //  `mov eax,1` -> the
                                                                 //   single retn at 0x8133900. The
                                                                 //   only other value this function
                                                                 //   returns is the help path's 0.
}

// Shuts down the current game. activateMenuAfter restores the menu on clients
// and is ignored by dedicated images.
void BF2Engine::shutdownGame(bool activateMenuAfter)   // bool from BF2.exe 0x403C50: whether the
                                                       //   menu comes back afterwards (see the tail); never read on a server
{
    if (!game)                                  // /0x8138017 -> straight to the epilogue
        return;

    if (performanceLogging)                     // field 116, tested at 0x813801d
    {
        // ~PerformanceLogging is inlined: the vptr is stored (0x8138259) and then operator delete is
        // called (0x8138262) - i.e. an ordinary `delete` of a class with a trivial destructor.
        delete performanceLogging;
        performanceLogging = 0;

        // Reset transient performance state; dwordA4 retains its constructor value.
        // [UNRESOLVED] Field meanings; double7C's double type is [INFERRED].
        byte78 = false;
    }

    g_system->setSimulationRate(100);           // direct call 0x8138039; _ZN4dice3hfe6System17setSim
                                                //   ulationRateEi (0x83D2510), receiver ds:0x8AC0D20

    if (demo)                                   // field 168, tested at 0x813803e
        demo->stop(game);                       // Demo::stop(IGame*) 0x813824f

    // EventManager vtable +0x14 == postEvent(EventCategory, unsigned int, void*, float).
    // Category 5 is one of the three BF2Engine registers a handler for in init().
    // [UNRESOLVED] what event id 6 is.
    eventManager->postEvent((EventCategory)5, 6, 0, 0.0f);

    if (quitNextFrameFlag)                      // field 68, cmp byte [ebx+44h] at 0x813807a
        deactivateMenu();                       // [PLATFORM] a stub in this build

    if (game)                                   // re-tested at 0x8138090 - it cannot have changed
    {
        game->shutdown();                                   // IGame vtable +0x1C, 0x81381d0
        classManager->unregisterSingleton("Game");           // ClassManager vtable +0x48, 0x81381fc
        game = 0;
    }

    if (gameLogic)                              // tested at 0x813809e
    {
        gameLogic->release();                                // IGameLogic vtable +0x08, 0x81381bc
        gameLogic = 0;
    }

    // Level-scoped singletons registered elsewhere during level load; dropped so the next level
    // re-creates them. The array is built on the stack (var_158/154/150 at 0x81380ad-0x81380c0),
    // NUL-terminated, and walked with a pointer. Two temporary std::strings are built per iteration,
    // one for getSingleton (0x81380f7) and one for unregisterSingleton (0x8138160).
    const char* levelSingletons[] = { "Terrain", "HeightMap", 0 };
    for (const char* const* name = levelSingletons; *name; ++name)
    {
        if (classManager->getSingleton(*name))          // ClassManager vtable +0x4C, 0x8138110
            classManager->unregisterSingleton(*name);   // ClassManager vtable +0x48, 0x8138179
    }

#if !DEDICATED
    // Clear and save the client's map list, then return to the menu unless quitting or suppressed.
    settings->boolSet("MNCommandLine", false);                       // vptr+0x54
    mapList->clear(false);
    mapList->save();
    if (activateMenuAfter && !quitNextFrameFlag && !byte51)
        activateMenu(0);
#endif
}

// Creates and initializes the selected game. noAutoJoin skips the client's automatic
// connection; startFromMenu selects the client menu flow. [PLATFORM] Dedicated
// images do not use the second flag. [INFERRED] noAutoJoin parameter name.
bool BF2Engine::startGame(bool noAutoJoin, bool startFromMenu)   // 2nd bool from BF2.exe 0x404AA0:
                                                                 //   it selects the menu flow below; BF2::init passes true,
                                                                 //   playDemo/hostServer/joinServer/startMission pass false
{
    bool ok = true;                                     // var_192, set to 1 at 0x81382d3

    // [UNRESOLVED] One-byte game-present allocation; only nullness is tested.
    // The destructor frees it without accessing its contents.
    delete dwordB0;                                     //  test / 0x8138b43 delete
    dwordB0 = new char;                                 // stored at 0x813830e

#if !DEDICATED
    // BF2.exe 0x404AA0, before the shared body: the menu flow. A non-developer client asked to start
    // from the menu logs the profile in, tells the SwiffHost which page to open and returns without
    // creating a game; the game is then created from the menu's own commands (joinServer /
    // startMission / hostServer). isDedicated() jumps straight to the shared body.
    if (!isDedicated())
    {
        ui::ISwiffHost* swiffHost = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
        bool menuFlow = false;
        if (startFromMenu)
            menuFlow = !settings->boolGet("GSDeveloper", false);            // vptr+0x50
        if (!menuFlow)
        {
            swiffHost->setGameStarting(true);                           // +0xD8
        }
        else
        {
            if (settings->intGet("playNow", 0) == 1)                       // vptr+0x10
            {
                swiffHost->setPage(1);                                      // +0x2C0
                activateMenu(0);
                return true;
            }
            std::string name     = settings->stringGet("GSPlayerName", std::string());
            std::string password = settings->stringGet("GSPlayerPassword", std::string());
            if (name != "")
            {
                IBase* lpm = classManager->getSingleton("LocalProfileManager");
                if (!static_cast<ILocalProfileManager*>(lpm)->quickLogin(name, password)) // PE+0xC0
                {
                    activateMenu(0);
                    return false;
                }
            }
            bool needLogin = false;
            if (!isDedicated() && !settings->boolGet("GSDeveloper", false))
                needLogin = true;
            if (needLogin)
            {
                IBase* lpm = classManager->getSingleton("LocalProfileManager");
                if (!static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile())
                    return false;
            }
            settings->boolSet("MNCommandLine", true);                      // vptr+0x54
            if (settings->boolGet("GSIsServer", false))
            {
                // Select page 4 for Internet servers and page 5 otherwise.
                swiffHost->setPage((serverSettings->m_internet != 1) + 4);
            }
            else
            {
                std::string address = swiffHost->resolveHostName(settings->stringGet("GSJoinAddress", "127.0.0.1"));   // +0x248
                settings->stringSet("GSJoinAddress", address);
                if (swiffHost->isPrivateAddress(settings->stringGet("GSJoinAddress", "127.0.0.1")))   // +0x24C
                    swiffHost->setPage(3);
                else
                    swiffHost->setPage(2);
            }
            activateMenu(0);
            return true;
        }
    }
#endif

    if (settings->boolGet("GSIsServer", false))         // ISettingsRepostitory vtable +0x50
    {
        // The registry stores the address of the IGame pointer using its generic IBase
        // carrier. Keep the global's declared type unchanged for symbol identity.
        SingletonRegInfo sri = { std::string("Game"), reinterpret_cast<IBase**>(&game), IID_IGame, CID_GameServer, 2 };
        classManager->registerSingleton(sri);           // ClassManager vtable +0x44, 0x81383c8

        if (!game->init())                              // IGame vtable +0x18, 0x81383ff
            return false;                               // > edx=0, return 0

        // renameAndReOpenPureLog composes "Logs/" + arg1 + arg2 + ".log" (verified from the string
        // literals inside it plus io::File::open/seek). arg1 is the empty string here and arg2 is
        // the role tag. [INFERRED] the parameter roles - only the concatenation order attests them.
        bf2Log->renameAndReOpenPureLog("", "server");   // this = field 88 (bf2Log)

        IGameServer* gameServer =
            (IGameServer*)game->queryInterface(IID_IGameServer);        // +0x0C
        if (!gameServer)
            return ok;                                  //  jumps past the failure handler:
                                                        // this returns true, not false. Preserved.

        setGameLogic(CID_ServerGameLogic);              // releases the old gameLogic and
                                                        // createInstance2(cid, IID_IGameLogic)
        if (!gameLogic->init())                         // IGameLogic vtable +0x10, 0x81384d0
            return false;

        int port = serverSettings->m_serverPort;          // ServerSettings +0x20
        if (settings->intGet("GSPort", 16567) != 16567) // 16567 == 0x40B7
            port = settings->intGet("GSPort", 16567);   // queried a second time; intGet
                                                        // [UNRESOLVED] The third startServer argument's meaning; this caller passes zero.
        ok = gameServer->startServer(serverSettings->m_serverIP, port, 0);
    }
    else
    {
        // client
        // [PLATFORM] Reachable in this build (GSIsServer is not set by default) but everything it
        // drives - IGameClient, the menu - is stubbed here; only the game client really runs it.
        SingletonRegInfo sri = { std::string("Game"), reinterpret_cast<IBase**>(&game), IID_IGame, CID_GameClient, 2 };
        classManager->registerSingleton(sri);
        if (!game->init())
            return false;

        // Deliberate asymmetry with the server branch: the client sets up the game logic before
        // renaming the log, the server does it after. Confirmed from straight fall-through order
        // > 0x813874f -> 0x813875e -> 0x813876b, with no intervening jumps.
        // [UNRESOLVED] why; no reason is visible in either build.
        setGameLogic(CID_ClientGameLogic);
        if (!gameLogic->init())
            return false;

        bf2Log->renameAndReOpenPureLog("", "client");

        IGameClient* gameClient =
            (IGameClient*)game->queryInterface(IID_IGameClient);
        if (!gameClient)
            return ok;                                  // > true

        if (noAutoJoin)
            return ok;                                  // > true

        const std::string& joinAddress = settings->stringGet("GSJoinAddress", "127.0.0.1"); // +0x40
        const std::string& password    = settings->stringGet("GSPassword", "");             // +0x40
        int  port         = settings->intGet("GSPort", 16567);                              // +0x10
        bool clPunkBuster = settings->boolGet("GSClPunkBuster", false);                     // +0x50

        // Join uses a five-second timeout; only result 1 indicates success.
        const int joinResult = gameClient->joinServer(joinAddress, port, password, 5.0f, clPunkBuster);
        if (joinResult == 1)
        {
#if !DEDICATED
            static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"))->setJoinPassword(std::string(""));   // +0x184, BF2.exe 0x404AA0
#endif
            return ok;                                  // true
        }

        ok = false;
#if !DEDICATED
        if (!isDedicated())                             // BF2.exe 0x404AA0: report the failure through the SwiffHost
        {
            ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
            switch (joinResult)
            {
            case 2:  host->setMessageId(11); break;
            case 3:  host->setMessageId(12); break;
            case 4:  host->setMessageId(8);  break;
            case 5:  host->setMessageId(9);  break;
            case 6:  host->setMessageId(13); break;
            case 7:  host->setMessageId(14); break;
            case 8:
            {
                std::string mod = host->getServerMod();                      // the server's mod
                IBase* modInfo = classManager->getSingleton("ModInfo");
                if (!modInfo_vslot0x30(modInfo, mod))                          // IModInfo +0x30 - [UNRESOLVED] role
                {
                    if (compareIgnoreCase(mod, "mods/xpack") == 0)             // both images
                        host->setMessageId(26);
                    else if (compareIgnoreCase(mod, "mods/xpack2") == 0)
                        host->setMessageId(27);
                    else
                        host->setMessageId(30);
                }
                else
                    activateModAndJoinServer(mod, joinAddress);
                break;
            }
            case 9:  host->setMessageId(2);  break;
            default: host->setMessageId(15); break;
            }
        }
#endif
    }

    if (!ok)
        shutdownGame(false);                            // second argument = 0
    return ok;                                          // > movzx edx, var_192
}

// Selects server mode and the requested level in settings, then starts the game.
// The dedicated implementation does not read the second string argument.
bool BF2Engine::hostServer(const std::string& level,
                           const std::string& /* [UNRESOLVED] never read in this build */)
{
    if (level.empty())
        return false;

    // [UNRESOLVED] the bool. shutdownGame takes one (see its own reconstruction above); the
    // decompiler shows no second argument pushed at this call site, so it is either false or an
    // artefact of the calling convention. Written as false; not proven.
    shutdownGame(false);                                // tear down any running game
    deactivateMenu();                                   // static/free function

    settings->boolSet("GSIsServer", true);              // ISettingsRepostitory vtable +0x54
    settings->stringSet("GSLoadLevel", level);          // vtable +0x44

    // The call site passes both bools as false (0x8137c5a); see the startGame header block.
    const bool ok = startGame(false, false);            // noAutoJoin = false
#if !DEDICATED
    if (!ok)                                            // BF2.exe 0x406570: back to the menu on failure. No such call
        activateMenu(0);                                //   in the ELF - and the client body has NO `level.empty()` gate
                                                        //   either: [UNRESOLVED] why the ELF does
#endif
    return ok;
}

// Initializes services after module registration, in dependency order: logging, file
// system/search paths, archives, localization/settings, status monitor, mod metadata,
// renderer, console, and networking. Each failing stage stops initialization.
bool BF2Engine::initEngine()
{
    // Retain the optional profiler query even though its result is discarded.
    if (profilerClient)
        profilerClient->getEnable();            // vtable +0x28

    // BF2Log vptr+0x08 is _ZN4dice3hfe6BF2Log17initDebugCallbackEv (0x813EB00), read straight out of
    // _ZTVN4dice3hfe6BF2LogE (0x88931F0). Call site: 0x8138B93 `mov eax,[edi+58h]`, 0x8138B96
    // `mov edx,[eax]`, 0x8138B9B `call [edx+8]`. There is no BF2Log::init symbol in the binary.
    if (!bf2Log->initDebugCallback())
        return false;

    io::mainConsole->setUseRelativePaths(true);  // OldConsole vptr+0x78, 0x8138BC6

    if (!bf2EngineSetup->initFileSystem())      // /0x8138BCF, instance call on +0x0C
        return false;

    // Add base-game assets first for non-base mods so the mod's files take precedence
    // while base assets remain available as fallback.
    const std::string modDirectory = settings->stringGet("GSModDirectory", "");   // +0x40
    if (compareIgnoreCase(modDirectory, "mods/bf2"))
        io::fileManager->addPath("mods/bf2");
    io::fileManager->addPath(modDirectory);

    // initLogFiles is NON-virtual, takes one std::string and returns bool
    // (_ZN4dice3hfe6BF2Log12initLogFilesERKSs, 0x813EA50). Its argument is the same shared empty
    // literal, built into var_68 at 0x8138CD3-0x8138CDF; `this` is the field at +0x58.
    if (!bf2Log->initLogFiles(""))
        return false;

    // Archive mounting is only permitted for the duration of this one script.
    io::fileManager->permitMountArchives(true);         // FileManager vptr+0x78
    io::mainConsole->runFullAccess("ServerArchives.con", "", "", "", "", "", "", "", "",
                                   io::Console::ignoredString_);   // +0x2C
    if (!isDedicated())                                 // BF2.exe 0x408EF0: the client archives, same access
        io::mainConsole->runFullAccess("ClientArchives.con", "", "", "", "", "", "", "", "",
                                       io::Console::ignoredString_);

    io::fileManager->permitMountArchives(false);

    if (!initLocalization())
        return false;

    io::settingsManager->load("Settings/Usersettings.con");

    if (!statusMonitor)
    {
        // Same shape as the GSFileChangeMonitor query in init(): the default true is materialised as
        // a stack temporary (`mov [ebp+var_159],1` at 0x81390CD) passed by const reference, and the
        // returned reference is dereferenced at 0x81390E1 (`movzx eax,[eax]`). 0x81390FC then tests
        // the loaded value and decides the allocation.
        if (isDedicated() && settings->boolGet("GSStatusMonitor", true))   // BF2.exe 0x408EF0 nests the
                                                        //   whole test in `if (sub_440510())`; vptr+0x50, 0x81390DC
            statusMonitor = new StatusMonitor();            // operator new(4), 0x8139109
    }

    // ClassManager vptr+0x4C, 0x8139021. [UNRESOLVED] getSingleton's real return type - the binary
    // shows an untyped pointer being used as an IModInfo*, so a cast is written here. If the true
    // return type turns out to be IModInfo* the cast is redundant but harmless.
#if !DEDICATED
    if (!isDedicated())                                 // BF2.exe 0x408EF0: the profile store comes up before ModInfo
        static_cast<ILocalProfileManager*>(classManager->getSingleton("LocalProfileManager"))->init(); // PE409364: result discarded, unguarded
#endif

    IModInfo* modInfo = (IModInfo*)classManager->getSingleton("ModInfo");
    modInfo->init();                            // ModInfo vptr+0x10 = _ZN4dice3hfe7ModInfo4initEv
                                                // A failed renderer initialization skips console/client setup and proceeds to
                                                // network initialization. Preserve this behavior for renderer-free dedicated startup.
    if (bf2EngineSetup->initRenderer())
    {
        if (!isDedicated())
        {
            if (!static_cast<io::IInputManager*>(io::inputManager)->init(1087))   // vslot +0x10, device mask 1087
                return false;
            io::mainConsole->run("Settings/Controls.con", "", "", "", "", "", "", "", "", io::Console::ignoredString_);
        }
        isDedicated();                                  // [UNRESOLVED] BF2.exe 0x408EF0: a bare isDedicated() whose
                                                        //   result is discarded (kept: it is a call in the binary)
        if (!bf2EngineSetup->initConsole())             // sub_5A5E20 - `return true` in both images
            return false;
#if !DEDICATED
        // [CHOSEN, not recovered] [RETIRE-WHEN: explosion-manager-construction-order]
        // Construct the explosion manager before HUD scripts need its blinding-light variables.
        // Replace this timing workaround when the original startup request for PlayerManager
        // is identified; repeated registration uses the same file-scope variable addresses.
        if (!world::explosionEffectManager)
            world::explosionEffectManager = new world::CPersistentExplosionEffectManager();
        if (hudManager && !hudManager->init())         // vslot +0x0C; "BF2HudManager init fails" is its own message
            return false;
#endif
    }
#if !DEDICATED
    // The twelve ECMenu event names (category 10) - registered by the client only; the dedicated
    // images carry neither the strings nor the calls. The
    // twelfth goes to the frame event manager, as shipped. EventManager vptr+0x34.
    eventManager->registerEvent(static_cast<EventCategory>(10), 1,  "ECMenuMenuEvent_Enable");
    eventManager->registerEvent(static_cast<EventCategory>(10), 2,  "ECMenuMenuEvent_Disable");
    eventManager->registerEvent(static_cast<EventCategory>(10), 3,  "ECMenuMenuEvent_InitLoadingScreen");
    eventManager->registerEvent(static_cast<EventCategory>(10), 5,  "ECMenuMenuEvent_RemoveLoadingScreen");
    eventManager->registerEvent(static_cast<EventCategory>(10), 4,  "ECMenuMenuEvent_UpdateLoadingScreen");
    eventManager->registerEvent(static_cast<EventCategory>(10), 6,  "ECMenuMenuEvent_SetLoadingScreenDebug");
    eventManager->registerEvent(static_cast<EventCategory>(10), 7,  "ECMenuMenuEvent_InitGameLoadingScreen");
    eventManager->registerEvent(static_cast<EventCategory>(10), 8,  "ECMenuMenuEvent_RemoveGameLoadingScreen");
    eventManager->registerEvent(static_cast<EventCategory>(10), 9,  "ECMenuMenuEvent_EnableBetweenRounds");
    eventManager->registerEvent(static_cast<EventCategory>(10), 10, "ECMenuMenuEvent_DisableBetweenRounds");
    eventManager->registerEvent(static_cast<EventCategory>(10), 11, "ECMenuMenuEvent_ActivateMainMenu");
    frameEventManager->registerEvent(static_cast<EventCategory>(10), 12, "ECMenuMenuEvent_EnableEscapeKeyMapping");
    if (!isDedicated())
    {
        if (IBase* host = classManager->getSingleton("SwiffHost"))
            static_cast<ui::ISwiffHost*>(host)->init(); // +0x10
    }
#endif
    if (!bf2EngineSetup->initNetwork())
        return false;
    SingletonServer::init();                            // genuinely static - 0x813907E pushes nothing
#if !DEDICATED
    musicPlayer = new MusicPlayer();                    // operator new(8), 0x7E1B40 - client only; never deleted
#endif
    return true;
}

// initLocalization - ELF 0x81392B0 is return true; client PE 0x4086E0 is the
// runtime-gated registry/filesystem path below. PE 0x406D40 appends the stock
// directory after the mod directory. ExcelLexicon keeps the first definition.
// findFiles/load results are discarded; missing lexicon is the sole false return.
bool BF2Engine::initLocalization()
{
#if !DEDICATED
    if (!isDedicated())
    {
        // Keep the existing shared global's IBase declaration; ILexicon's base is +0.
        io::ILexicon* lexicon = static_cast<io::ILexicon*>(io::lexicon);
        if (!lexicon)
            return false;
        const std::string language = registry->getString(RegKeyLocalMachine, "Language", "English", "");
        settings->stringSet("GSLanguage", language);
        const std::string localizationPath = std::string("localization/") + language + "/";
        std::list<std::string> directories;
        directories.push_back(std::string("localization/") + language + "/");
        if (settings->stringGet("GSModDirectory", "") != "mods/bf2")
            directories.push_back(std::string("../../mods/bf2/localization/") + language + "/");
        for (const std::string& directory : directories)
        {
            std::list<io::FileInfo> files;
            io::fileManager->findFiles(directory, true, files);
            for (const io::FileInfo& file : files)
                if (!file.name.empty() && !file.isDirectory)
                    lexicon->load(directory + file.name);
        }
    }
#endif
    return true;
}

// Deletes the log when present. Called by module shutdown and the engine destructor.
void BF2Engine::closeLog()
{
    if (bf2Log)                 // +0x58
        delete bf2Log;          // vtable +4 - BF2Log has a virtual destructor
    bf2Log = 0;
}

// Destroys engine-owned subobjects and resets transient fields in dependency order.
// Other services shut down through BF2EngineSetup; this is not a reversal of initEngine.
// [INFERRED] Boolean result, always true on completion.
bool BF2Engine::shutdownEngine()
{
    if (dwordB0)                    // +0xB0, trivial destructor: a bare operator delete
        delete dwordB0;             // [UNRESOLVED] type - see startGame's note on offset 176
    dwordB0 = 0;

    delete performanceLogging;      // +0x74. Inlined destructor: stores &PerformanceLogging::VMT,
    performanceLogging = 0;         // then operator delete.

    // [UNRESOLVED] This reset block's field semantics. The corresponding original
    // reset also stores a -1.0 timestamp sentinel and clears the following words;
    // the omitted stores remain noted below.
    byte78 = false;
    // ... double7C = -1.0 and dword84..dwordA0 zeroed - missing from this body, nine statements

    deactivateMenu();               // [PLATFORM] a 5-byte stub in this build

    if (demo)                       // +0xA8, ~Demo (non-virtual) followed by operator delete
        delete demo;
    demo = 0;

    if (window)                     // +0x10, virtual delete (vtable +4 in the ELF's type - see BF2Window.h)
        delete window;              // BF2Window: the game window, null in a dedicated image
    window = 0;

    if (bf2Render)                  // +0x18, virtual delete (vtable +4), 0x81394b0. type attested
        delete bf2Render;           //   from Demo::beginMainLoop's mangled parameter list;
    bf2Render = 0;                  //   [INFERRED] the field name

    if (bf2Console)                 // +0x14, ~BF2Console followed by operator delete
        delete bf2Console;
    bf2Console = 0;

    if (frameInterpolator)          // +0x1C; the inline destructor frees its two vectors (0x408AB0)
        delete frameInterpolator;
    frameInterpolator = 0;

    operator delete(tickCalculator);    // +0x68, 0x8139429-0x813942F. A bare operator delete with NO
    tickCalculator = 0;                 // null test - `delete p` would emit one - so this is written
                                        // the same way as the debugDraw free in shutdown(). There is
                                        // no `test`/`jz` between the load and the call, unlike every
                                        // other teardown in this function.

    bf2EngineSetup->shutdownModules();      // +0x0C, non-virtual. This is the call that reaches
                                            // BF2Engine::closeLog() - see closeLog above.
    if (bf2EngineSetup)
        delete bf2EngineSetup;              // ~BF2EngineSetup followed by operator delete
    bf2EngineSetup = 0;

    return true;
}

// Previous frame time is translation-unit state, not a BF2Engine member.
static double s_prevWallClockTime = 0.0;                        // [INFERRED] name

// Pointer to the console variable, read each frame so changes take effect without
// restarting. [INFERRED] local name.
static float* const s_sleepOverheadTime =                       // [INFERRED] name
    Vars::getFloat("server-sleep-overhead-time", 0.005f);       // default bits 0x3BA3D70A

bool BF2Engine::mainLoop()
{
    // Runs one frame per call. A pending quit returns false before any frame work.
    if (quitNextFrameFlag)
        return false;

    // : the idle throttle. The binary does `if (!byte52) usleep(100000);` - a flat 100 ms
    // (imm 0x186A0 at 0x8137A80), NOT derived from a getter. byte52 (+0x52) is set from a handleEvent
    // payload. [PLATFORM] the recovered call is POSIX usleep(100000); reproduced with the portable
    // C++ equivalent (platform primitives are free under the fidelity rule).
    if (!byte52)
        std::this_thread::sleep_for(std::chrono::milliseconds(100));   // imm 100000 us

    // Honor client shutdown and aborted-load requests before advancing the frame.
    // Keep the repeated client-interface queries in their original order.
    if (world::playerManager && dwordB0)
        world::playerManager->getNumberOfPlayers();             // vcall +0x88, result discarded
    if (game)
    {
#if !DEDICATED
        if ((game->queryInterface(IID_IGameClient)
             && static_cast<IGameClient*>(game->queryInterface(IID_IGameClient))->getDisconnectRequested())   // +0x70
            || (game && game->getAbortLoadLevel()))             // vcall +0x11C
            shutdownGame(true);
#else
        if (game->getAbortLoadLevel())                          // vcall +0x11C; 0x8137A07
            shutdownGame(true);
#endif
    }


    // : begin this frame's demo work. Args this->bf2Render (+0x18), this->bf2Console (+0x14).
    demo->beginMainLoop(bf2Render, bf2Console);

    // demo overrides engine timing before it is computed (0x8137341 / 0x8137380 / 0x8137445)
    double preferredFrameTime =
        demo->overridePreferredFrameTime(lockFPS ? 1.0 / (double)lockFPS : 0.0);

    // wall-clock bookkeeping (0x8137356). t0 is captured here, before the optional spin, and is
    // what the frame limiter at the bottom measures the whole frame against.
    double frameStartWallClock = g_system->getWallClockTime();      //  vcall +0x1C (var_198)
    double lastWallDelta       = frameStartWallClock - s_prevWallClockTime;   //  (var_190)
    double wallNow             = frameStartWallClock;               // var_1C8
    double wallDelta           = 0.0;                                // BF2.exe var 64: the unclamped delta renderMenu receives

    // Dedicated mode busy-waits only when sleep overhead exceeds one; the default
    // 0.005 uses the sleeping limiter instead.
    if (demo->allowLockFps() && (!isDedicated() || *s_sleepOverheadTime > 1.0f))
                                                                    //   BF2.exe 0x40CA80 adds the client short-circuit
    {
        double target = frameStartWallClock + preferredFrameTime - lastWallDelta;
        while (target > wallNow)                                    //  (jbe exits)
            wallNow = g_system->getWallClockTime();                 //  vcall +0x1C
    }

    // mainLoopDeltaTime (+0x34), 0x81373F0-0x8137428. Same clamp as frameTime below, but fed by
    // the wall clock rather than the exact/simulation clock.
    {
        wallDelta = wallNow - s_prevWallClockTime;

        float d = (float)(wallNow - s_prevWallClockTime);           //  fsub qword [0x8AACBF0]
        if (d < 0.0000099999997f)                                   // flt_8892F08; min path 0x8137A00
            d = 0.0000099999997f;
        else if (d > 0.06666667f)                                   // flt_8892F0C (1/15); 0x813741C
            d = 0.06666667f;
        mainLoopDeltaTime = d;                                      //  fstp dword [this+0x34]
        s_prevWallClockTime = wallNow;                              //  fstp qword ds:0x8AACBF0
    }

    // THE exact (simulation) clock and the frames ring, 0x8137433-0x81374AD.
    float exactBefore = (float)g_system->getExactTime();            //  vcall +0x18, m32 store

    if (!demo->overrideAdvanceTime())
        g_system->advanceExactTime();                              //  (only when not overridden)

    float exactNow = (float)g_system->getExactTime();               //  vcall +0x18, m32 store

    // Store the raw duration in the ring before clamping frameTime to 1/15 second.
    {
        float dt = exactNow - exactBefore;                          // float precision
        int   n  = (int)frames.size();                              //  (end-begin)>>2
        framesIndex = (framesIndex + 1) % n;                        //  idiv, 0x8137492
        frameTime = dt;                                             //  RAW store
        frames[framesIndex] = dt;                                   //  unclamped sample
        if (frameTime < 0.0000099999997f)
            frameTime = 0.0000099999997f;
        else if (frameTime > 0.06666667f)                           //  fcmovnbe
            frameTime = 0.06666667f;                                // clamped store 0x81374AD
    }

    //  sampled once, used twice. getSimulationRate is called exactly once per frame (0x81374BA),
    // before TickCalculator::compute, and the ==0 result cached (setz 0x81374C4). That one cached
    // flag gates both the server step (0x8137527) and the world/object/ragdoll block (0x813758A).
    // Calling the getter again at either site would be a behavioural change if it is not pure.
    const bool simRateZero = (g_system->getSimulationRate() == 0);  //  vcall +0x24

    // THE fixed-timestep decision (0x81374DD). The argument is the engine's simulation clock.
    int   ticks       = tickCalculator->compute(g_system->getExactTime());
    int   renderTicks = ticks;                                      // var_170 - input/render count
    float renderDt    = tickCalculator->tickLength;                 // var_16C = calculator's +0x00
    // note: this->dword6C(+0x6C) receives tickLength's RAW bits each frame (0x81374F9, a plain mov -
    // no FPU op), but nothing in this build reads it back. As an unobserved internal write it is
    // documented rather than reproduced (the field stays [UNRESOLVED]).

    if (game)                                                       //  test / 0x8137504 jnz
    {
        // : args &frameTime(+0x24), &ticks (server step count), &renderTicks, &renderDt,
        // tickCalculator. updateTicks may rewrite ticks and renderTicks/renderDt.
        demo->updateTicks(&frameTime, &ticks, &renderTicks, &renderDt, tickCalculator);

    }

    // BF2.exe 0x40CA80 gates the input and the demo's player input on "no movie is playing" and
    // then polls the movie queue; the dedicated images have no movie player, so isMoviePlaying()
    // and checkMovie() fold to nothing there (checkMovie is a bare `ret` in the ELF, 0x8137270).
    if (!isMoviePlaying() && game)
    {
        if (!getMenuActive())
            updateInput(renderTicks, renderDt);                    //  (render counts)
    }
    if (!isMoviePlaying())
    {
        if (game && !getMenuActive())
            demo->handlePlayerInput(game->getSimulationRate());     //  vcall +0xE8
        if (!isMoviePlaying())
            checkMovie();                                           //  on the client
    }

#if !DEDICATED
    // THE menu / movie ARM - BF2.exe 0x40CA80, never in a dedicated image
    if (menuActive && !menuLogic && s_activateMenuOnFrame1)
    {
        // First frame: log the profile in from the settings, raise the main menu, draw it once.
        std::string name     = settings->stringGet("GSPlayerName", std::string());       // vptr+0x40
        std::string password = settings->stringGet("GSPlayerPassword", std::string());
        if (name != "" && password != "")
        {
            if (IBase* lpm = classManager->getSingleton("LocalProfileManager"))
                static_cast<ILocalProfileManager*>(lpm)->quickLogin(name, password); // PE+0xC0
        }
        activateMenu(0);
        renderMenu(1, 1.0f, 1.0, false);
        s_activateMenuOnFrame1 = false;
    }
    else if (moviePlayer)
    {
        if (s_cursorShown)
        {
            ShowCursor(FALSE);
            s_cursorShown = false;
        }
        byte70 = true;
        static_cast<IFSMoviePlayer*>(moviePlayer)->drawFrame();     // +0x18
        // Poll the keyboard for the skip key.
        io::IKeyboardDevice* keyboard = static_cast<io::IKeyboardDevice*>(
            static_cast<io::IInputDeviceManager*>(io::inputDeviceManager)->getInputDevice(io::IDFKeyboard));   // ds:0x9FFCB0, vslot +0x14
        keyboard->update(-1.0f);                                    // +0x24, the two calls as shipped: -1.0 then 0.1
        keyboard->update(0.1f);
        // +0x34 keyPressed(1): key 1 is DirectInput's DIK_ESCAPE [INFERRED] - the enumerator set of
        // io::InputDeviceKeys is unrecovered, the raw value is what the binary passes. Flag 1 movies
        // (EA/dice/legal in Init.con) cannot be skipped; flag 0 ones (intro/welcome) can.
        if ((keyboard->keyPressed(static_cast<io::InputDeviceKeys>(1)) && movieFlag != 1)
            || static_cast<IFSMoviePlayer*>(moviePlayer)->finished())   // +0x1C
        {
            if (moviePlayer)
                moviePlayer->release();                             // vslot +0x08
            moviePlayer = 0;
            checkMovie();
        }
    }
    else if (menuLogic)
    {
        if (byte52)
        {
            if (byte70)
            {
                if (ShowCursor(TRUE) == 1)
                    ShowCursor(FALSE);
                if (musicPlayer)
                {
                    musicTraceSite("mainLoop byte70");               // [TEMP] BF2_TRACE_MUSIC
                    musicPlayer->setState(0, 0);
                }
                byte70 = false;
            }
            renderMenu(renderTicks, renderDt, wallDelta, true);
        }
    }

    // [CHOSEN, not recovered] Client VoIP updates are omitted.
    // [RETIRE-WHEN: client-voip-optional] Restore the voice update when game and voice client are available.
    if (game)
        updateMenuPause(*savedPauseState, menuLogic != 0);          //  on the heap int
    {
        const bool hudOrMenuUp = menuLogic != 0
                              || (hudManager && hudManager->isMenuLoading())
                              || g_system->getSimulationRate() != 100;
        // Mute reason 2 tracks menu/loading HUD or a nonstandard simulation rate.
        g_bfSound->setMuteState(static_cast<MuteState>(2), hudOrMenuUp);
        // : the same global's +0xC8 = ELF +0xCC, IBfSound::getForceUpdate() const; facade
        // body 0x7DE970 returns the byte at this+324 that setForceUpdate (0x7DE960) raises.
        if ((!isDedicated() && ticks > 0 && !game) || g_bfSound->getForceUpdate())
            g_bfSound->update(ticks);                               // IBfSound: the one-int update (PE slot +0x18)
    }
    if (game && gameLogic && !isDedicated() && renderTicks > 0)
        // The HUD information-layer singleton is required on this client path.
        static_cast<HudInformationLayer*>(g_hudInformationLayer)->update(static_cast<float>(renderTicks) * renderDt);
#endif


    // Step simulation only with ticks and both game owners present; use the
    // simulation-rate sample already taken for this frame.
    if (!isMoviePlaying() && ticks > 0 && game && gameLogic)        // 0x813752E; BF2.exe adds the movie gate
    {
#if !DEDICATED
        if (!isDedicated() && demo->allowRendering())               // BF2.exe 0x40CA80: the interpolator, only when rendering
            frameInterpolator->update(ticks);
#endif
        if (!simRateZero)
        {
            g_system->restoreFPUExceptions();                       //  (vtbl+0x30) - a 1-byte ret in every image
            if (!game->update(ticks, 1.0f / 30.0f))                 // dt = 0x3D088889
                shutdownGame(true);                                //  on a failed update
        }
    }

    // Python updates require render ticks and an initialized host, independent of
    // game presence and simulation rate.
    if (renderTicks > 0)
    {
        if (python::pythonHost()->isInitialized())
            python::pythonHost()->update();
    }

    // Keep the unconditional player lookup even in dedicated images, which discard
    // its null result. playerManager must already exist.
    world::IPlayer* localPlayer = world::playerManager->getLocalHumanPlayer();   // BF2.exe keeps it for the frame
    (void)localPlayer;

    // Advance the separate frame-event manager by a duration, not a timestamp.
    // Its processEvents result is discarded.
    frameEventManager->processEvents(frameTime);

    // Advance the world only outside movies, with a game and nonzero simulation rate.
    if (!isMoviePlaying() && game && !simRateZero)                  //  / 0x813758A; BF2.exe adds the movie gate
    {
        world::objectManager->updateObjectsInGrid();                //  vcall +0xA4, no args

        // The ragdoll step. The guard IS the singleton pointer - AnimModule::init creates it lazily,
        // so before that runs this is simply skipped.
        if (anim::g_ragDollTemplate)
        {
            // Without a viewer, retain zero vectors and disable viewer-based ragdoll sorting.
            Vec3 viewerPos = { 0.0f, 0.0f, 0.0f };
            Vec3 viewerDir = { 0.0f, 0.0f, 0.0f };
            bool hasViewer = false;
#if !DEDICATED
            // BF2.exe 0x40CA80: the viewer comes from BF2Render::getRenderView when the AI global
            // says so or the game logic is a client's. The view transform is fetched twice, as shipped. The
            // dedicated PE still makes the AI global's +0x18 call with bf2Render null (result
            // unused) - [UNRESOLVED] identity of ds:0xA33C08 / ds:0x8B25B0; not called on a server here.
            const bool aiViewer = unresolvedGlobalA33C08 && unresolvedGlobalA33C08_vslot0x18();
            if (bf2Render && (aiViewer || (gameLogic && !gameLogic->isServer())))
            {
                IBase* renderView = bf2Render->getRenderView();              // vslot +0x30, 0x4531B0
                // View matrix rows 3 and 2 supply viewer position and direction.
                const float* m = static_cast<dx9rend::IRenderView*>(renderView)->getViewMatrixPtr()->m;
                viewerPos.x = m[12]; viewerPos.y = m[13]; viewerPos.z = m[14];
                m = static_cast<dx9rend::IRenderView*>(renderView)->getViewMatrixPtr()->m;
                viewerDir.x = m[8];  viewerDir.y = m[9];  viewerDir.z = m[10];
                hasViewer = true;
            }
#endif
            anim::g_ragDollTemplate->updateInstances(hasViewer, viewerPos, viewerDir, frameTime);
        }

        world::objectManager->handleFrameUpdates(frameTime);        //  vcall +0x2C

        // ..0x81378D2. Both types and slots are now recovered: this is the live
        // PlayerManager list (not a copy), and IPlayer +0x34 is handleFrameUpdate(float).
        const std::list<world::IPlayer*>& players = world::playerManager->getPlayers();
        for (std::list<world::IPlayer*>::const_iterator it = players.begin();
             it != players.end(); ++it)
        {
            (*it)->handleFrameUpdate(frameTime);
        }
    }

#if !DEDICATED
    // THE frame - BF2.exe 0x40CA80. Never in a dedicated image: isDedicated() gates it and every
    // object it touches is client-only.
    bool rendered = false;
    if (!moviePlayer && !isDedicated() && demo->allowRendering() && !menuLogic && byte52)
    {
        // Keep the player's camera in interpolated mode only for this frame.
        world::ICameraObject* heldCamera = 0;
        if (game)
        {
            if (localPlayer)
            {
                heldCamera = localPlayer->getCamera();              // +0x17C
                if (heldCamera)
                    heldCamera->setInterpolatedMode(true);          // +0x14
            }
            // : both arguments are narrowed to float before the push - getExactTime()'s
            // double through `fstp dword ptr`, and the calculator's `qword ptr [ecx+10h]` the same
            // way. 0x45C190 itself is `retn 8` and reads both with `fld`/`fsub dword ptr`.
            frameInterpolator->interpolate(static_cast<float>(g_system->getExactTime()),
                                           static_cast<float>(tickCalculator->prevTickTime));
            demo->visualUpdate(frameTime, static_cast<float>(lockFPS),
                               frameInterpolator->getInterpolationFraction(),
                               game->getSimulationRate());          // Demo::visualUpdate. The third
                                                                    //   argument moves as a plain dword with no
                                                                    //   conversion: it is already a float.
        }
        // BF2.exe 0x40D3B5: `mov ecx, [esi+18h]; mov eax, [ecx]; call [eax+1Ch]` on this->bf2Render.
        bf2Render->beforeFrame();                                   // vslot +0x1C, 0x4530A0
        rendered = true;
        // ..0x40D3D7: the demo's +0x08 recorder and the widened frame time, then +0x18.
        bf2Render->render(frameTime, demo->recorder());             // vslot +0x18, 0x453270 [FRONTIER] body
        if (game)
            frameInterpolator->postRender();
        if (heldCamera)
            heldCamera->setInterpolatedMode(false);                 // +0x14
    }
    if (!isDedicated() && !rendered)
        // ..0x40D413: `mov ecx, [esi+18h]` - this->bf2Render, NOT the held camera - and its
        // +0x14 with the widened frame time, reached only when the render arm above did not run.
        bf2Render->update(frameTime);                               // vslot +0x14, 0x4530D0
#endif

    bf2Console->updateConsole();
#if !DEDICATED
    if (hudManager)
        hudManager->executeQueuedConsoleCommands();                 // vslot +0x360, BF2.exe 0x40CA80
#endif

    // the file-change watcher pair, gated on a console setting read every frame (0x81375BE).
    //  boolGet returns a pointer (SettingsRepostitory vcall +0x50) which is then dereferenced at
    // it is not a by-value bool getter.
    if (settings->boolGet(std::string("GSFileChangeMonitor"), false))
    {
        io::fileManager->update();                                  //  vcall +0x44, ds:0x8AC8C80
        io::fileChangeManager->update();                            //  vcall +0x2C, ds:0x8AC8C84
    }

    // The registry stores this singleton as IBase; cast to its concrete interface before updating counters.
    if (debugCounterManager)
        ((DebugCounterManager*)debugCounterManager)->update();      // vtable +0x14

    updateStatusMonitor(false);

    // Limit dedicated frames to 30 Hz using time since frame entry. Each sleep-loop
    // iteration subtracts overhead again; a backward clock ends the wait.
    if (isDedicated())                                              // BF2.exe 0x40CA80: the frame limiter is server-only
    {
    double remaining = (1.0 / 30.0)                                 // dbl_8892F10
                     - (g_system->getWallClockTime() - frameStartWallClock)
                     - *s_sleepOverheadTime;                        //  fsub m32
    if (remaining > 0.0)
    {
        double sleepStart = g_system->getWallClockTime();           //  (var_1B0, fixed)
        if (remaining > 0.0)                                        // the literal re-test
        {
            do
            {
                // truncated milliseconds: remaining * 1000.0f (flt_888B168) with the FPU rounding
                // mode forced to chop (CW | 0x0C00) around the fistp at 0x8137744.
                g_system->sleep((int)(remaining * 1000.0f));        //  vcall +0x20
                double elapsed = g_system->getWallClockTime() - sleepStart;
                if (elapsed >= 0.0)
                    remaining = remaining - elapsed - *s_sleepOverheadTime;
                else
                    remaining = -1.0f;                              //  flt_888B590
            }
            while (remaining > 0.0);
        }
    }

    // Every exit from the limiter lands here (0x8137704 jbe and 0x813778C jmp both target it), so
    // the arena boundary is crossed exactly once per frame whether or not the frame slept.
    }   // isDedicated()
    else if (byte5C)                                                // BF2.exe 0x40CA80: the client yields instead
        g_system->sleep(0);


    memory::nextVisualFrame();                                      // the visual arena

    if (demo->endMainLoop(bf2Render, quitNextFrameFlag))
        quitNextFrameFlag = true;                                  // this frame requested quit

    // The watchdog heartbeat, poked once per frame near the END so a hang earlier in the frame is
    // externally detectable. CheckServerAliveThread is the producer side.
    if (isDedicated())                                              // BF2.exe 0x40CA80: `if (sub_440510() && sub_440510())` -
        markAsStillRunning();                                       // the call site's test and the inlined body's own

    // [CHOSEN, not recovered] The per-frame PunkBuster SDK pump is omitted.
    // A queued quit makes this the final frame.
    return !quitNextFrameFlag;
}

void BF2Engine::updateStatusMonitor(bool force)
{
    if (!statusMonitor)
        return;

    statusMonitor->update(force);
}

// Reads the watchdog heartbeat, incrementing it while game status is not ordinary play.
// This allows loading to satisfy the watchdog without frame progress. Unlike the frame-side
// writer, this accessor neither locks reads nor its conditional increment.
int BF2Engine::getTouchedServerCount()
{
    if (gameLogic && gameLogic->getGameStatus() != 1)
        ++touchedServerCount;                           // unlocked, see the block above
    return touchedServerCount;
}

// . The producer half, and the one the frame loop needs. Straight-line: take mutex2,
// increment, release. The unlock is a tail jump in the original (0x812E95F `jmp Mutex::unlock`)
// rather than a call - a compiler detail with no behavioural weight, written normally here.
void BF2Engine::markAsStillRunning()
{
    mutex2.lock();                                  // on &this->mutex2 (+0xBC)
    ++touchedServerCount;                           //  (+0xC8)
    mutex2.unlock();                                // tail jmp in the binary
}

// Stops and releases the watchdog before closing the log and destroying ClassManager.
// The factory registry must remain alive through watchdog shutdown; engine-owned
// watchdog state is released before member and base destruction.
BF2Engine::~BF2Engine()
{
    if (checkServerAliveThread)
    {
        mutex1.lock();
        byteC4 = true;                          // the thread's stop signal
        mutex1.unlock();

        checkServerAliveThread->stop();
        if (checkServerAliveThread)             // the binary's own re-test, reproduced
            delete checkServerAliveThread;      // vtable +0x04 = Thread's deleting dtor (the pair
                                                // sits at +0x00/+0x04 - Thread.h:96)

        byteC4 = false;
        checkServerAliveThread = 0;
    }

    closeLog();                                 // ish

    if (classManager)
    {
        delete classManager;                    // deleting dtor through vtable +0x04
        classManager = 0;
    }

    delete savedPauseState;                     // operator delete on the ctor's new(4)
    delete instanceGuard;                       // the ctor's new(1) / new(8); the client's ReleaseMutex is its
                                                //   destructor (0x4083F0), a no-op elsewhere

    // mutex2, mutex1, commandLine, m_playerName and frames destroy member-wise, reverse declaration
    // order - matching the binary's inlined sequence. The IInputFilter base dtor runs last.
}

// The main-loop control accessors - 0x8136F10 / 0x8131140-0x8131180.
// setLockFPS refuses non-positive values (`test edx,edx; jle` skips the store) - a clamp the
// header's console binding notes never mentioned. Reproduced.
void BF2Engine::quitNextFrame()  { quitNextFrameFlag = true; }

void BF2Engine::setLockFPS(int fps)
{
    if (fps > 0)                                // the guard IS the function
        lockFPS = fps;
}

int  BF2Engine::getLockFPS() const   { return lockFPS; }
void BF2Engine::setMenuActive(bool active) { menuActive = active; }

// recordDemo - 0x8131190, and the gate is GameLogic::isServer (+0x13C, named by the implementor
// vtable). Failure detection is a member peek, not a return value: Demo::recordDemo answers
// nothing, so this reads demo->demoRecorder afterwards - null means the recorder never came up -
// and reports with the recovered string. [CHOSEN] void return (no path materialises a value).
void BF2Engine::recordDemo(const std::string& name)
{
    if (gameLogic && gameLogic->isServer())
    {
        demo->recordDemo(game, name, bf2Console);

        if (!demo->demoRecorder)
            io::mainConsole->output("Failed to record demo. Maybe the demo '" + name
                                    + "' already exists.");
    }
    else if (io::mainConsole)
    {
        io::mainConsole->output("can't record demos as a client\n");
    }
}

// . Same isServer gate; hands the demo the game pointer so it can unhook.
void BF2Engine::stopRecording()
{
    if (gameLogic && gameLogic->isServer())
        demo->stop(game);
}

// Zeroes 64 input words while the console is open; otherwise leaves input intact.
// [CHOSEN] Raw cast for this input-layout seam.
bool BF2Engine::filterPlayerInput(io::PlayerInput& input)
{
#if !DEDICATED
    if (menuLogic)                                          // BF2.exe 0x403C00: the menu swallows everything
        return false;
#endif
    if (*reinterpret_cast<const bool*>(bf2Console))         // BF2Console +0x00
    {
        unsigned int* raw = reinterpret_cast<unsigned int*>(&input);
        for (unsigned int i = 0; i <= 0x3F; ++i)            // the binary's jbe bound
            raw[i] = 0;
        return false;                                       //  / bf2_w32ded 0x402B50: al = 0 after the wipe
    }
    return true;
}

// Filters frame input for macros, menus, console, and quit.
// [CHOSEN] Raw casts retain the unresolved GameInput layout.
// [PLATFORM] Console toggling resets the ring on dedicated Win32 as well as clients.
bool BF2Engine::filterGameInput(io::GameInput& input)
{
    const unsigned int* masks = reinterpret_cast<const unsigned int*>(&input);
    const float*        axes  = reinterpret_cast<const float*>(&input);
    const unsigned int  maskA = masks[0x128 / 4];
    const unsigned int  maskB = masks[0x12C / 4];

#if !DEDICATED
    if (menuLogic)                                          // : the menu takes the input
    {
        menuLogic->handleInput(input);                      // IMenuLogic +0x38
        return false;
    }
#if defined(_WIN32)
    menuKeyCount = 0;                                       // the WM_KEYDOWN ring (Game/Main/BF2.cpp) is dropped
    menuKeyRead  = menuKeyWrite;                            //   every frame the menu is down
#endif
#endif
    io::mainConsole->updateKeyMacros(input);                // ELF +32 (a 5-byte stub there) / PE +0x1C

    if ((maskA & 4) && axes[8 / 4] > 0.5f)                  // trigger +8: the menu key
    {
        time_t now;
        time(&now);
        const bool heldLongEnough = difftime(now, sessionStartTime) > 1.0;   // +0xD4 / PE +0x114 / client +0x12C
#if !DEDICATED
        bool showMenu = false;
        if (hudManager && !hudManager->isChatActive()
            && hudManager->getMode() != 13 && hudManager->getMode() != 14)
        {
            if (settings->boolGet("GSMenu", false) == true && heldLongEnough)   // vptr+0x50
                showMenu = true;
        }
        if (showMenu)
        {
            hudManager->setMenuLoading(true);
            hudManager->showWideHelp(
                hudManager->getLocalizedString("HUD_MENU_HELP_MENU_IS_LOADING"), 5.0f);
            if (hudManager->isCommanderShown())
                hudManager->setMode(15, true);
            hudManager->setMode(0, true);
            g_system->setSimulationRate(100);
            if (!menuLogic)
                eventManager->postEvent(static_cast<EventCategory>(10), 11, 0, 2.0f);   // ActivateMainMenu in 2 s
            return false;
        }
#else
        (void)heldLongEnough;                               // bf2_w32ded 0x403120 computes and drops it
#endif
    }
    if ((maskA & 2) && axes[4 / 4] > 0.5f)                  // trigger +4: the console toggle
    {
        bf2Console->setConsoleActive(!bf2Console->getConsoleActive());
        // Discard queued console characters whenever the console toggles.
        consoleCharCount = 0;
        consoleCharRead  = consoleCharWrite;
    }
    if (bf2Console->getConsoleActive())
    {
        io::mainConsole->updateGameInput(input);            // ELF +28 / PE +0x18
        return false;
    }
    if ((maskB & 1) && axes[0x80 / 4] > 0.5f)               // trigger +0x80
    {
        if (byte53 && byte64)
        {
            byte53 = false;
#if !DEDICATED
            bf2Render->toggleFullscreen();                  // vslot +0x20, BF2.exe 0x453130; the dedicated PE has the same
#endif                                                      //   call on a pointer that is always null there
            return false;
        }
    }
    else
        byte53 = true;
    if ((maskA & 0x8000000) && axes[0x6C / 4] > 0.5f)       // trigger +0x6C: quit
        quitNextFrameFlag = true;
#if !DEDICATED
    if (!hudManager)
        return false;
    if ((maskA & 0x40000) && axes[0x48 / 4] > 0.5f          // trigger +0x48: the screenshot key
        && !io::mainConsole->getConsoleActive()             // ELF +168 / PE +0xA4
        && !hudManager->isChatActive() && !hudManager->isSquadRenameActive() && !hudManager->isSquadCreateActive())
    {
        io::mainConsole->executeLine(std::string("renderer.dumpScreen\n"), io::Console::ignoredString_,
                                     false, false, false, false, false);   // ELF +12 / PE +0x08
    }
    if (!hudManager)
        return false;
    hudManager->handleGameInput(frameTime, input);                    // vslot +0x1B8 returns void
    return true;                                                    //  materializes AL=1 independently
#else
    return false;                                           // bf2_w32ded 0x403120 returns a dead register here
#endif
}

// . A dead-frame stub - reserves 0x128 bytes of stack and touches none of it. Fourth
// sighting of the pattern (initLogFiles 0x178, setDebugCallbackDisabled 0x148, joinServer 0x38):
// a client-build body compiled out of the dedicated server with its frame left behind. mainLoop
// calls this every non-menu frame; on this build that call does nothing, faithfully.
void BF2Engine::updateInput(int renderTicks, float renderDt)
{
    // BF2.exe 0x403190: `if (isDedicated()) return;` then the per-tick pump through io::inputManager
    // (ds:0x9FFCCC): update(ticks, dt); for each queued entry, take the next game sample, copy the
    // player sample (280 bytes - sizeof(io::PlayerInput) in the shipped layout), hand it to the
    // demo recorder and to the game, advance. A dedicated build keeps the empty frame.
    if (isDedicated())
        return;
    io::IInputManager* manager = static_cast<io::IInputManager*>(io::inputManager);
    manager->update(renderTicks, renderDt);                                     // vslot +0x1C
    if (renderTicks >= 1)
    {
        for (int n = manager->getNumEntries(); n > 0; --n)                      // vslot +0x48
        {
            manager->getNewGameInputSample();                                   // vslot +0x38, result unused here
            io::PlayerInput sample;
            std::memcpy(&sample, manager->getNewPlayerInputSample(), sizeof(io::PlayerInput));   // vslot +0x34; [CHOSEN]
                                                                                //   the 280-byte raw copy of the binary
            demo->addPlayerInput(sample);                                       // > Demo::addPlayerInput
            if (game)
                game->addPlayerInput(sample);                                   // IGame +0x68
            manager->advanceNextUnread();                                       // vslot +0x3C
        }
    }
}

//  (a `ret` in the ELF) / BF2.exe 0x403240: the menu-time pump - the same walk as
// updateInput, but the samples are fetched and dropped (the menu consumes input through the
// SwiffHost). No isDedicated() test in the client; the whole body is client-only.
void BF2Engine::updateMenuInput(int renderTicks, float renderDt)
{
#if !DEDICATED
    if (renderTicks >= 1)
    {
        io::IInputManager* manager = static_cast<io::IInputManager*>(io::inputManager);
        manager->update(renderTicks, renderDt);                                 // vslot +0x1C
        for (int n = manager->getNumEntries(); n > 0; --n)                      // vslot +0x48
        {
            manager->getNewGameInputSample();                                   // vslot +0x38
            manager->getNewPlayerInputSample();                                 // vslot +0x34
            manager->advanceNextUnread();                                       // vslot +0x3C
        }
    }
#else
    (void)renderTicks; (void)renderDt;
#endif
}

// . The movie player is client-only; what survived here is the settings read - and the
// original queries "skipMovies" AND discards THE answer. Reproduced: the read may have side
// effects in SettingsRepostitory (it is not provably pure), the value never mattered.
void BF2Engine::playMovie(std::string name, int flag)
{
    bool fallback = false;
    const bool skip = settings->boolGet(std::string("skipMovies"), fallback); // result ignored on a server
#if !DEDICATED
    if (!skip && !isDedicated())                            // BF2.exe 0x407A60: queue it (the name arrives by value)
    {
        MovieEntry entry;
        entry.name = name;
        entry.flag = flag;
        movies.push_back(entry);
    }
#else
    (void)skip; (void)name; (void)flag;
#endif
}

// getAverageFPS - 0x8137AE0. The frames ring (written by mainLoop every frame) averaged with
// std::accumulate from A float zero, epsilon-floored, inverted and rounded half-UP to a whole
// number - the function reports "60", not "59.94". The 1.1920929e-7 is FLT_EPSILON.
float BF2Engine::getAverageFPS() const
{
    double average = std::accumulate(frames.begin(), frames.end(), 0.0f)
                   / static_cast<double>(frames.size());
    if (average <= 0.00000011920929)
        average = 1.0;

    return static_cast<float>(floor(1.0 / average + 0.5));
}

// . Another dead-frame stub (0x38 bytes reserved, eax zeroed, nothing done) - joining
// a server is the client's build; the dedicated server's copy is compiled out.
bool BF2Engine::joinServer(const std::string& address, int /*port - taken and never read, BF2.exe 0x405C10*/)
{
#if !DEDICATED
    // BF2.exe 0x405C10. A non-developer client must be logged in; the game is torn down, the menu
    // dropped, the join settings written (the SwiffHost supplies the stored password and the profile
    // names), and startGame runs the client branch. On failure the menu comes back.
    if (isDedicated())
        return false;
    bool needLogin = false;
    if (!isDedicated() && !settings->boolGet("GSDeveloper", false))
        needLogin = true;
    if (needLogin)
    {
        IBase* lpm = classManager->getSingleton("LocalProfileManager");
        if (!static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile())
            return false;
    }
    shutdownGame(false);
    deactivateMenu();
    settings->boolSet("GSIsServer", false);                 // vptr+0x54
    settings->stringSet("GSJoinAddress", address);          // vptr+0x44
    if (!isDedicated())
    {
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
        if (host)
            host->setJoinAddress(address);                  // +0x254
        std::string password = host->getJoinPassword();     // +0x180, unguarded in the binary
        settings->stringSet("GSPassword", password);
        applyProfileNameSettings();
    }
    const bool ok = startGame(false, false);
    if (!ok)
        activateMenu(0);
    return ok;
#else
    (void)address;
    return false;                                           // : a dead frame, eax zeroed
#endif
}

// Replaces the current game with demo playback, selecting a config file or explicit
// demo filename. Skips automatic server join and returns true regardless of playback result.
bool BF2Engine::playDemo(bool fromConfigName, const std::string& name)
{
    shutdownGame(false);
    deactivateMenu();

    bool isServer = false;
    settings->boolSet(std::string("GSIsServer"), isServer);

    if (startGame(true, false))
    {
        IGameClient* gameClient = 0;
        if (game)
            gameClient = static_cast<IGameClient*>(game->queryInterface(IID_IGameClient));

        bool started;
        if (fromConfigName)
        {
            started = demo->playDemo(gameClient, name);
        }
        else
        {
            DemoConfig config;
            config.demoFile.assign(name);       // +0x08 - IDA's struct calls it filename1; the
                                                // recovered DemoConfig.h name is demoFile
            started = demo->playDemo(gameClient, config);
        }

        if (started)
        {
#if !DEDICATED
            // The dedicated ELF makes this call through a null global; that build leaves it out.
            hudManager->setPaint(false);
#endif
            return true;
        }
    }

    activateMenu(0);
    return true;                                            // constant, all paths
}

// Starts the named level using the game mode from settings; mode is unused.
// Rebuilds the level string from c_str before calling startGame(false, false).
bool BF2Engine::startMission(const std::string& map, const std::string& /*mode - never read in either image (BF2.exe 0x406010)*/)
{
#if !DEDICATED
    // BF2.exe 0x406010 in front of the shared body: a non-developer client must be logged in, and an
    // empty map name is reported to the SwiffHost instead of started.
    bool needLogin = false;
    if (!isDedicated() && !settings->boolGet("GSDeveloper", false))
        needLogin = true;
    if (needLogin)
    {
        IBase* lpm = classManager->getSingleton("LocalProfileManager");
        if (!static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile())
            return false;
    }
    if (map.empty())
    {
        static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"))->postMessage(SwiffHostMessage(1, std::string("Map not found"), 0));   // +0x1E4
        return false;
    }
#endif
    shutdownGame(false);
    deactivateMenu();

    bool isServer = true;
    settings->boolSet(std::string("GSIsServer"), isServer);
    settings->stringSet(std::string("GSLoadLevel"), std::string(map.c_str()));
#if !DEDICATED
    if (!isDedicated())
        applyProfileNameSettings();                         // the block BF2.exe inlines here and in joinServer
#endif

    const bool started = startGame(false, false);
    if (!started)
    {
        activateMenu(0);
#if !DEDICATED
        static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"))->requestRestartMenu();   // +0x264, BF2.exe 0x406010 only
#endif
    }
    return started;
}

// . Ends the game only if a demo is actually playing (IGame +0xE0, isPlayingDemo).
void BF2Engine::shutdownDemo()
{
    if (game && game->isPlayingDemo())
        shutdownGame(true);
}

// Dispatches engine events. Payload types are event-specific: core event 1 uses a string,
// 9 a vector of diagnostic key/value pairs, and 15 a bool.
// Handlers preserve discarded queries because the calls themselves may have side effects.
void BF2Engine::handleEvent(EventCategory category, unsigned int id, EventNode* /*node*/,
                            void* data)
{
    if (category == static_cast<EventCategory>(5))
    {
        switch (id)
        {
        case 1:
        {
            // BF2.exe 0x4097B0: `if (data == "GSExtrapolateFrame" && !isDedicated()) { bool b =
            // settings->boolGet("GSExtrapolateFrame", true); nullsub_2(b); }` - the consumer is an
            // empty function in the retail client ([UNRESOLVED] which). Both dedicated images keep
            // only the comparison with its result discarded, which is this statement's fold.
            if (static_cast<const std::string*>(data)->compare("GSExtrapolateFrame") == 0 && !isDedicated())
                settings->boolGet("GSExtrapolateFrame", true);      // vptr+0x50; result to nullsub_2
            break;
        }

        case 9:
        {
            typedef std::vector<std::pair<std::string, std::string> > InfoRows;
            InfoRows& rows = *static_cast<InfoRows*>(data);

            rows.push_back(std::make_pair(std::string("- BF2Engine -"), std::string("")));
            rows.push_back(std::make_pair(std::string("Command line"), commandLine));

            if (gameLogic && serverSettings && gameLogic->isServer())
            {
                std::stringstream settingsText;             // mode 0x18 = in|out in the binary
                serverSettings->save(settingsText);
                rows.push_back(std::make_pair(std::string("Server settings"),
                                              settingsText.str()));
            }
            break;
        }

        case 10:
        {
            systemInfo()->getMemoryUsage();                 // +0x14 - discarded
            unsigned int totalPhysical = 0;
            unsigned int availablePhysical = 0;
            systemInfo()->getMemoryInfo(totalPhysical, availablePhysical);   // +0x18 - discarded
            break;
        }

        case 11:
        {
            if (dword98 || dword94)                         // +0x98 tested first, as the binary does
                g_system->getWallClockTime();               // fstp st - discarded
            break;
        }

        case 15:
            byte52 = *static_cast<bool*>(data);             // the mainLoop gate byte
            break;

        case 16:
            bf2->handleInGameWinMessages();
            break;

        default:
            break;
        }
    }
    else if (category == static_cast<EventCategory>(10))
    {
        if (id == 11)
            activateMenu(0);                                    // the argument is xor'd
#if !DEDICATED
        else if (id == 12 && !isDedicated())                    // ECMenuMenuEvent_EnableEscapeKeyMapping, BF2.exe 0x4097B0
        {
            if (IBase* controlSettings = classManager->getSingleton("ControlSettings"))
                static_cast<IControlSettings*>(controlSettings)->setMappingInput(false);   // +8C 8247D0
        }
#endif
    }
#if !DEDICATED
    else if (category == static_cast<EventCategory>(11) && id == 2)   // BF2.exe 0x4097B0: a packed (x << 16 | y) payload;
    {                                                                //   the low half is read through a byte-typed pointer
        ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));       //   there - [UNRESOLVED] its width
        const unsigned int packed = *static_cast<const unsigned int*>(data);
        const int x = static_cast<int>(packed >> 16);
        const int y = static_cast<int>(packed & 0xFFFF);
        if (menuLogic && menuLogic->getClassId() == CID_MainMenuLogic)
        {
            menuLogic->onResize(x, y);                              // IMenuLogic +0x44
            host->setFlagE80(true);                                 // +0x274
        }
        host->setViewportSize(x, y);                                // +0x1D8 (0x409860 pushes both halves)
    }
#endif
}

// loadAI - 0x8139D70. The "UseAI" setting decides; boolGet hands back a pointer whose target is
// read (the repository's live-value idiom), and only a true routes into BF2EngineSetup::initAI.
// shutdownAI - 0x8139E10 - is a plain forward (a tail jump in the binary).
bool BF2Engine::loadAI()
{
    bool fallback = false;
    const bool useAI = settings->boolGet(std::string("UseAI"), fallback);   // const bool& idiom

    if (!useAI)
        return false;

    return bf2EngineSetup->initAI();
}

void BF2Engine::shutdownAI()
{
    bf2EngineSetup->shutdownAI();                           // tail jmp at 0x8139E1F
}

// When movies are enabled and none is playing, tries queued movies until one opens.
// Releases failed players and preserves the selected entry's movie flag. Dedicated
// images have no movie/UI effect.
void BF2Engine::checkMovie()
{
#if !DEDICATED
    if (settings->boolGet("skipMovies", false))             // vptr+0x50
        return;
    if (moviePlayer)
        return;
    if (movies.empty())
        return;
    for (;;)
    {
        const std::string name = movies.front().name;
        movieFlag = movies.front().flag;
        movies.pop_front();
        moviePlayer = classManager->createInstance(std::string("dice.hfe.geom.FSMoviePlayer"), 80047u, 0);   // vptr+0x38; 80047 =
                                                        //   IID_IFSMoviePlayer, which Core/Ids.h keeps in its RendDX9 section
        if (static_cast<IFSMoviePlayer*>(moviePlayer)->load(name))   // +0x10, the path by value
            break;
        moviePlayer->release();                             // vslot +0x08, unguarded in the binary
        moviePlayer = 0;
        if (movies.empty())
            return;
    }
#endif
}

//  (ret) / BF2.exe 0x40BD10. One menu frame: update, draw (with the console over it when
// open), pump the menu's input, then execute at most one queued menu command.
void BF2Engine::renderMenu(int renderTicks, float renderDt, double wallDelta, bool draw)
{
#if !DEDICATED
    if (!menuLogic)
        return;
    // PE40BD44..40BD57 addresses global978FC4 directly, not an OldConsole field.
    if (io::g_falconMouseMode != 2)
        io::g_falconMouseMode = 1;
    menuLogic->update(wallDelta);                           // +0x28
    if (draw && menuLogic->beginFrame())                    // +0x2C
    {
        menuLogic->display();                               // +0x30
        if (io::mainConsole->getConsoleActive())            // ELF +168 / PE +0xA4
            // Drawing the console requires the active renderer.
            static_cast<dx9rend::IRenderer*>(g_renderer)->slot_0x58();   // RendDX9 0x1002E2D0
        menuLogic->endFrame();                              // +0x34
    }
    updateMenuInput(renderTicks, renderDt);

    MenuEvent event;
    ui::ISwiffHost* host = menuLogic->getSwiffHost();       // +0x20
    if (host->pollEvent(event))                             // +0x28 on that object
    {
        ui::ISwiffHost* swiffHost = 0;
        switch (event.type)
        {
        case 2:  quitNextFrameFlag = true; break;
        case 3:  joinServer(event.args[0], 0); break;
        case 4:  startMission(event.args[0], event.args[1]); break;
        case 5:  parseParameters(event.args[0]); break;
        case 6:  g_bfSound->playSound(event.args[0]); break;          // IBfSound +0x2C
        case 7:  hostServer(event.args[0], event.args[1]); break;
        case 8:  deactivateMenu(); shutdownGame(true); break;
        case 9:  deactivateMenu(); break;
        case 10: browseModHomepage(event.args[0]); break;
        case 11: activateMod(event.args[0]); break;
        case 12: playDemo(false, event.args[0]); break;
        case 13:
        {
            IBase* lpm = classManager->getSingleton("LocalProfileManager");
            if (static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile())
                static_cast<ILocalProfileManager*>(lpm)->saveCurrentProfile();
            static_cast<ILocalProfileManager*>(lpm)->shutdown(); // PE+20, preserved unguarded call
            byte51 = true;
            shutdownGame(false);
            byte51 = false;
            break;
        }
        case 14:
        {
            swiffHost = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
            if (IBase* pm = classManager->getSingleton("ProfileManager"))
                // Clear any pending profile transition before rebuilding the menu.
                static_cast<IProfileManager*>(pm)->setPendingTransition(false, std::string(""));
            shutdownGame(false);
            activateMenu(0);
            swiffHost->postMessage(SwiffHostMessage(1, event.args[0], 0));   // +0x1E4, the {1, text, 0} record 0x402F90 builds
            break;
        }
        case 15:
            swiffHost = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
            eventManager->postEvent(static_cast<EventCategory>(10), 5, 0, 0.0f);   // RemoveLoadingScreen
            swiffHost->setGameStarting(false);                      // +0xD8
            break;
        case 16: restartMenu(); break;
        case 17: byte51 = false; shutdownGame(false); break;
        case 18: restartBF2(); break;
        case 19: activateModAndJoinServer(event.args[0], event.args[1]); break;
        default: break;
        }
    }
#else
    (void)renderTicks; (void)renderDt; (void)wallDelta; (void)draw;
#endif
}

//  (ret) - the ELF's stub. [UNRESOLVED] the client body: BF2.exe folds the menu's input
// filtering into filterGameInput's head (0x404650), no separate function was located.
void BF2Engine::filterMenuInput(io::GameInput&)                            {}

//  (ret) / BF2.exe 0x4032F0.
void BF2Engine::deactivateMenu()
{
#if !DEDICATED
    if (!menuLogic)
        return;
    time(&sessionStartTime);                                // +0x12C
    if (menuLogic->getClassId() == CID_MainMenuLogic && musicPlayer)
        musicPlayer->setState(1, 0);
    else if (menuLogic->getClassId() == CID_BetweenRoundsLogic && musicPlayer)
        musicPlayer->setState(3, 0);
    menuActive = false;                                     // +0x84
    if (hudManager)
        hudManager->showHelp(std::string(""), 0.0f);
    if (menuLogic)
        menuLogic->release();                               // +0x08
    menuLogic = 0;
#endif
}

//  (ret) / BF2.exe 0x4033B0. Creates the menu logic for the requested class id (0 or the
// three menu CIDs; anything else re-uses whatever is current - null after the deactivate above, as
// shipped), hands it the console, initialises it, and marks the menu active.
void BF2Engine::activateMenu(unsigned int classId)
{
#if !DEDICATED
    if (quitNextFrameFlag || isDedicated())
        return;
    settings->boolSet("GSMenu", true);                      // vptr+0x54
    IMenuLogic* logic = menuLogic;
    if (menuLogic)
    {
        deactivateMenu();
        logic = menuLogic;
    }
    unsigned int create = CID_MainMenuLogic;
    if (!classId || classId == CID_MainMenuLogic
        || (create = CID_EndOfRoundLogic,   classId == CID_EndOfRoundLogic)
        || (create = CID_BetweenRoundsLogic, classId == CID_BetweenRoundsLogic))
    {
        logic = static_cast<IMenuLogic*>(classManager->createInstance(create, IID_IMenuLogic, 0));   // vptr+0x34
        menuLogic = logic;
    }
    logic->setConsole(bf2Console);                          // +0x3C
    menuLogic->init();                                      // +0x14
    menuLogic->getSwiffHost()->onMenuActivated();           // +0x20, then +0x24 on it
    menuActive = true;                                      // +0x84
    if (gameLogic)
        gameLogic->stopPlayingVoMessages();                 // +0x230
    if ((!classId || classId == CID_MainMenuLogic) && !s_menuMusicHold)
    {
        if (musicPlayer)
        {
            musicTraceSite("activateMenu");                          // [TEMP] BF2_TRACE_MUSIC
            musicPlayer->setState(0, 0);
        }
    }
    s_menuMusicHold = false;
#else
    (void)classId;
#endif
}

//  (mov eax,1) / BF2.exe 0x4034D0.
void BF2Engine::restartMenu()
{
#if !DEDICATED
    if (isDedicated())
        return;
    if (!menuLogic)
        return;
    const unsigned int classId = menuLogic->getClassId();
    deactivateMenu();
    ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(classManager->getSingleton("SwiffHost"));
    host->setMessageId(20);
    activateMenu(classId);
    host->setFlagE80(false);
#endif
}

//  (ret). [UNRESOLVED] the client body - not located; the menu command that plays a
// sound (renderMenu case 6) calls IBfSound::playSound directly.
void BF2Engine::playSound(const std::string&)                              {}

//  (mov eax,1) / BF2.exe 0x4035E0: minimise the game window, open the URL, quit.
void BF2Engine::browseModHomepage(const std::string& url)
{
#if !DEDICATED
    ShowWindow(window->getHwnd(), SW_MINIMIZE);             // Window vslot +0x2C
    if (reinterpret_cast<INT_PTR>(ShellExecuteA(0, "open", url.c_str(), 0, 0, SW_SHOWNORMAL)) <= 32)
        return;
    quitNextFrameFlag = true;
#else
    (void)url;
#endif
}

//  (mov eax,1) / BF2.exe 0x403640: relaunch this executable with `commandLine` (+0x68) as
// its arguments and quit.
void BF2Engine::restartBF2()
{
#if !DEDICATED
    char fileName[MAX_PATH];
    GetModuleFileNameA(0, fileName, MAX_PATH);
    if (reinterpret_cast<INT_PTR>(ShellExecuteA(0, "open", fileName, commandLine.c_str(), 0, SW_SHOWNORMAL)) <= 32)
        return;
    quitNextFrameFlag = true;
#endif
}

#if !DEDICATED
// The command line both activateMod bodies build (BF2.exe 0x403E40 / 0x404010) before restartBF2.
// [CHOSEN] factored; the join arguments are the only difference between the two.
void BF2Engine::buildRestartCommandLine(const std::string& modPath, const std::string* joinAddress)
{
    commandLine  = std::string("+modPath ") + modPath;
    commandLine += " +multi 1";
    commandLine += " +restart 1";
    if (!settings->boolGet("GSFullScreen", false))
        commandLine += " +fullscreen 0";
    if (joinAddress)
    {
        commandLine += std::string(" +joinServer ") + *joinAddress;
        commandLine += std::string(" +port ") + formatString("%d", settings->intGet("GSPort", 16567));
    }
    if (IBase* lpm = classManager->getSingleton("LocalProfileManager"))
    {
        if (static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile())
        {
            std::string scratch;
            localProfileManager_vslot0x70(lpm, localProfileManager_vslot0xD0(lpm, scratch, 1));
            std::string password;
            password = static_cast<ILocalProfileManager*>(lpm)->getPassword();
            commandLine += std::string(" +playerName ") + scratch;       // [UNRESOLVED] which string each += takes:
            commandLine += std::string(" +playerPassword ") + password;   //   the decompiler drops the operands
        }
    }
}
#endif

//  (mov eax,1) / BF2.exe 0x403E40.
void BF2Engine::activateMod(const std::string& modPath)
{
#if !DEDICATED
    buildRestartCommandLine(modPath, 0);
    restartBF2();
#else
    (void)modPath;
#endif
}

//  (mov eax,1) / BF2.exe 0x404010.
void BF2Engine::activateModAndJoinServer(const std::string& modPath, const std::string& address)
{
#if !DEDICATED
    buildRestartCommandLine(modPath, &address);
    restartBF2();
#else
    (void)modPath; (void)address;
#endif
}

// bf2ExitCallback - 0x8138B50, the free function the exit path hands around (StatusMonitor's
// quit hook): a null-guarded quitNextFrame through the void* it was registered with.
void bf2ExitCallback(void* engine)
{
    if (engine)
        static_cast<BF2Engine*>(engine)->quitNextFrame();
}


#if !DEDICATED
IMenuLogic* menuLogic = 0;                  // dword_987358 - the active menu; see BF2Engine.h
hud::BF2HudManager* hudManager = 0;         // dword_A10890 - zeroed again by the client ctor (0x408190)
BF2VoipClient* bf2VoipClient = 0;           // dword_9A4DA0
IBase* g_renderer = 0;                      // BF2.exe g_Renderer - [UNRESOLVED] symbol; set by the renderer bring-up, not by this TU
IBase* unresolvedGlobalA33C08 = 0;          // ds:0xA33C08 / bf2_w32ded ds:0x8B25B0
// The client singleton slots BF2.exe 0x4301A9 registers (Core/ModuleRegistry.cpp). [CHOSEN] placement:
// their .data addresses 0x987340..0x987398 surround menuLogic (0x987358) and s_prevWallClockTime
// (0x987330), i.e. this TU's statics.
IBase* g_hudInformationLayer = 0;      IBase* g_distributedLightRenderer = 0;  IBase* g_localProfileManager = 0;
IBase* g_profileManager = 0;           IBase* g_rankManager = 0;               IBase* g_clanManager = 0;
IBase* g_newsManager = 0;              IBase* g_autoPatch = 0;                 IBase* g_persistence = 0;
IBase* g_menuTeamManager = 0;          IBase* g_globalSettings = 0;            IBase* g_videoSettings = 0;
IBase* g_audioSettings = 0;            IBase* g_controlSettings = 0;           IBase* g_generalSettings = 0;
IBase* g_hapticSettings = 0;           IBase* g_serverBrowserManager = 0;      IBase* g_swiffHost = 0;
IBase* g_demoLibrary = 0;              IBase* g_serverLogoManager = 0;
IBase* g_gameServerSettings = 0;       // ds:0x987354 - the table's first row; no registry row yet (see BF2ClientSeams.h)
#endif

} // namespace hfe
} // namespace dice
