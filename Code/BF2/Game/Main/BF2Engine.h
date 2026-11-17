
#ifndef BF2ENGINE_H
#define BF2ENGINE_H

#include <string>
#include <vector>

#include "../../IO/IInputFilter.h"
#include "../IEventListener.h"
#include "../../Core/Mutex.h"
#include <list>
#include <ctime>

namespace bf2editor { class EngineAccess; }

namespace dice {
namespace hfe {

class BF2;
class BF2Console;
class BF2EngineSetup;
class BF2Log;
class BF2Render;
class BF2Window;
class IBase;
class BF2FrameInterpolator;
class MusicPlayer;
// Client single-instance guard. The destructor retains ReleaseMutex on an
// unacquired handle rather than CloseHandle. [INFERRED] names.
struct SingleInstanceGuard;
class IMenuLogic;
class Demo;
class EventNode;
class Mutex;
class PerformanceLogging;
class StatusMonitor;
class Thread;
class TickCalculator;
enum  EventCategory : int;

namespace io { class GameInput; class PlayerInput; }
namespace world { unsigned int getCurrentFrame(); }

// Engine lifecycle, frame dispatch, and image-specific front-end state.
// Preserve IInputFilter then IEventListener inheritance and member order.
// [INFERRED] Header path and descriptive member names.
class BF2Engine : public io::IInputFilter,
                  public IEventListener
{
public:
    explicit BF2Engine(BF2* bf2);
    virtual ~BF2Engine();

    // Initializes services and parses the command line before optional game startup.
    bool init(const std::string& commandLine);
    // Shuts down game/services. [INFERRED] Boolean return; callers discard it.
    bool shutdown();
    bool initEngine();
    bool shutdownEngine();
    void initDefaultSettings();
    bool parseParameters(const std::string& cmd);
    bool initLocalization();
    void closeLog();

    // Runs one frame and returns whether the caller should continue.
    bool mainLoop();
    void quitNextFrame();
    float getAverageFPS() const;
    void  setLockFPS(int fps);
    int   getLockFPS() const;

    // [INFERRED] noAutoJoin name. The second Boolean is retained but unused.
    bool startGame(bool noAutoJoin, bool);
    // The Boolean restores the client menu after shutdown; dedicated images ignore it.
    void shutdownGame(bool);
    bool startMission(const std::string& map, const std::string& mode);
    bool hostServer(const std::string&, const std::string&);
    bool joinServer(const std::string& address, int port);

    // Returns true. fromConfigName selects a config filename rather than a fresh DemoConfig.
    bool playDemo(bool fromConfigName, const std::string& name);
    // Reports failure through the console; inspect the demo recorder for the outcome.
    void recordDemo(const std::string& name);
    void stopRecording();
    void shutdownDemo();

    // Image-specific input-filter overrides; preserve both virtual slots.
    virtual bool filterGameInput(io::GameInput&);
    virtual bool filterPlayerInput(io::PlayerInput&);
    void updateInput(int, float);
    void updateMenuInput(int, float);

    virtual void handleEvent(EventCategory, unsigned int, EventNode*, void*);

    bool loadAI();
    void shutdownAI();

    int  getTouchedServerCount();
    void markAsStillRunning();
    void updateStatusMonitor(bool);

    void setMenuActive(bool);
    // Dedicated images always return false.
    bool getMenuActive() const;
    bool isMoviePlaying() const; // [CHOSEN] Dedicated images return false.

    // [INFERRED] Accessor for a direct field read in the original.
    BF2Window* getWindow() const { return window; }
    // [CHOSEN] Accessors expose fields used directly by client console bindings.
    bool getAllowToggleFullscreen() const { return byte64; }
    void setAllowToggleFullscreen(bool allow) { byte64 = allow; }
    BF2Render* getBF2Render() const { return bf2Render; }
    void activateMenu(unsigned int);
    void deactivateMenu();
    void restartMenu();
    void renderMenu(int, float, double, bool);
    void filterMenuInput(io::GameInput&);
    void playMovie(std::string, int);
    void checkMovie();
    void playSound(const std::string&);
    void browseModHomepage(const std::string&);
    void restartBF2();
    void activateMod(const std::string&);
    void activateModAndJoinServer(const std::string&, const std::string&);
#if !DEDICATED
    void buildRestartCommandLine(const std::string& modPath, const std::string* joinAddress); // [CHOSEN] Shared implementation for activateMod variants.
#endif

    // [INFERRED] Friendship expresses direct field access; original source visibility is unknown.
    friend class BF2EngineSetup;
    friend class GameServer;
    friend class MainMenuLogic;
    friend class BetweenRoundsLogic;
    friend class SwiffHost;
    friend class AutoRecorder;
    friend class BF2FrameInterpolator;
    // [TEMP] Client startup fixture installs setup without running init.
    friend struct BF2EngineFixture;
    friend class BF2Render;
    friend class GameClient;
    friend unsigned int world::getCurrentFrame();
    // [CHOSEN, not recovered] Editor bootstrap installs the shared engine state without init.
    friend class ::bf2editor::EngineAccess;

// Offsets describe the original x86 ELF layout, not native compiler layout.
// [PLATFORM] STL and client-only members shift PE offsets. Preserve reset ordering.
private:

    /*   8 */ BF2*                bf2; // Non-owning application facade.
    /*  12 */ BF2EngineSetup*     bf2EngineSetup; // Owned module/service setup.
    /*  16 */ BF2Window*          window; // Client game window; null in dedicated images.
    /*  20 */ BF2Console*         bf2Console; // Owned console used by demo playback.
    /*  24 */ BF2Render*          bf2Render; // Owned client renderer.
    /*  28 */ BF2FrameInterpolator* frameInterpolator; // Client-only; deleted during engine shutdown.
    /*  32 */ StatusMonitor*      statusMonitor; // Owned server status display.
    /*  36 */ float               frameTime; // [INFERRED] Clamped frame duration; frames stores the raw sample.
    /*  40 */ std::vector<float>  frames; // [INFERRED] Frame-duration ring for FPS measurements.
    /*  52 */ float           mainLoopDeltaTime; // [INFERRED] Wall-clock delta clamped to 1e-5..0.0666667 seconds.
    /*  56 */ int                 framesIndex; // [INFERRED] Current ring index.
    /*  60 */ int                 dword3C; // [UNRESOLVED] Initialized to zero.
    /*  64 */ int                 dword40; // [UNRESOLVED] Initialized to zero.
    /*  68 */ bool            quitNextFrameFlag; // [INFERRED] Stops mainLoop before the next frame.
    /*  72 */ std::string         m_playerName; // Command-line player name.
    /*  76 */ std::string         commandLine; // Original command line.
    /*  80 */ bool                menuActive; // Initialized true.
    /*  81 */ bool                byte51; // [UNRESOLVED] Initialized false.
    /*  82 */ bool                byte52; // [UNRESOLVED] Idle-throttle gate; initialized true, updated by event 15.
    /*  83 */ bool                byte53; // [UNRESOLVED] Input-filter state; initialized true.
    /*  84 */ unsigned int        dword54; // [UNRESOLVED] Initialized zero; incremented by each server simulation step.
    /*  88 */ BF2Log*             bf2Log; // Owned log; closeLog deletes and clears it.
    /*  92 */ bool                byte5C; // [UNRESOLVED] Initialized false.
    /*  96 */ int                 lockFPS; // Constructor default 100; init sets 60.
    /* 100 */ bool                byte64; // Alt+Enter permission exposed to console bindings.
    /* 104 */ TickCalculator*     tickCalculator; // Owned fixed-step calculator.
    /* 108 */ int                 dword6C; // [UNRESOLVED] Integer state copied from TickCalculator.
    /* 112 */ bool                byte70; // [UNRESOLVED] Initialized true.
    /* 116 */ PerformanceLogging* performanceLogging; // Performance log state used during game shutdown.
    /* 120 */ bool                byte78; // [UNRESOLVED] Cleared with the following reset block.
    /* 124 */ double              double7C; // [UNRESOLVED] Meaning; [INFERRED] double sentinel initialized to -1.
    /* 132 */ int                 dword84; // [UNRESOLVED] Reset to zero.
    /* 136 */ int                 dword88; // [UNRESOLVED] Reset to zero.
    /* 140 */ int                 dword8C; // [UNRESOLVED] Reset to zero.
    /* 144 */ int                 dword90; // [UNRESOLVED] Reset to zero.
    /* 148 */ unsigned int        dword94; // [UNRESOLVED] Unsigned event-11 state, initially zero.
    /* 152 */ unsigned int        dword98; // [UNRESOLVED] Unsigned event-11 state, initially zero.
    /* 156 */ int                 dword9C; // [UNRESOLVED] Reset to zero.
    /* 160 */ int                 dwordA0; // [UNRESOLVED] Last field in the zero-reset block.
    /* 164 */ int                 dwordA4; // [UNRESOLVED] Constructor sets -1; shutdown does not reset it.

    /* 168 */ Demo*               demo; // Owned demo controller.
    /* 172 */ MusicPlayer*        musicPlayer; // Client-only allocation; the client shutdown arm deletes it.
#if !DEDICATED
    struct MovieEntry // Client-only movie queue payload.
    {
        std::string name;
        int         flag;
    };
    /* PE +0xF0  */ int                    dwordF0; // [UNRESOLVED] Initialized zero.
    /* PE +0xF4  */ IBase*                 moviePlayer; // Released when playback finishes or is skipped.
    /* PE +0xF8  */ std::list<MovieEntry>  movies; // Pending intro movies.
    /* PE +0x104 */ int                    movieFlag; // 1 prevents skipping. Uninitialized until checkMovie assigns it.
#endif
    /* 176 */ void*               dwordB0; // [UNRESOLVED] One-byte allocation used only as a game-present flag.
    /* 180 */ int                 dwordB4; // [UNRESOLVED] Original code does not access this member.
    /* 184 */ Mutex               mutex1; // [INFERRED] Guards byteC4; embedded by value.
    /* 188 */ Mutex               mutex2; // [INFERRED] Guards touchedServerCount; embedded by value.
    /* 192 */ Thread*        checkServerAliveThread; // Stopped and deleted by the destructor.
    /* 196 */ bool                byteC4; // [UNRESOLVED] Shutdown flag guarded by mutex1.
    /* 200 */ unsigned int        touchedServerCount; // Watchdog progress counter guarded by mutex2.
    /* 204 */ SingleInstanceGuard* instanceGuard; // Owned client instance guard; dedicated images never read its flag.
    /* 208 */ int*                savedPauseState; // [INFERRED] Owned menu pause state; -1 means not paused by the menu.
    /* 212 */ time_t              sessionStartTime; // [INFERRED] Menu-key timing origin; retail time_t is 32-bit.
};
// Client single-instance guard. The destructor retains ReleaseMutex on an
// unacquired handle rather than CloseHandle. [INFERRED] names.
struct SingleInstanceGuard
{
#if defined(_WIN32) && !DEDICATED
    /* +0x00 */ void* handle; // Win32 handle without including windows.h.
#elif (defined(__APPLE__) || defined(BF2_LINUX_CLIENT)) && !DEDICATED
    // [CHOSEN, not recovered] Owned flock descriptor; closed on destruction or exec.
    int lockFd;
#endif
    /* +0x04 (client) / +0x00 */ bool isFirstInstance;
    SingleInstanceGuard();
    ~SingleInstanceGuard();
};

#if !DEDICATED
// Active client menu, created by activateMenu and released by deactivateMenu.
extern IMenuLogic* menuLogic;
#endif

} // namespace hfe
} // namespace dice

#endif
