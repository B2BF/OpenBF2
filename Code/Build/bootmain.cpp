// Scaffolding, not recovered source: diagnostic engine entry point with optional probes
// and a configurable frame bound. Production startup and probe fixtures are separate modes.
// The staged output lives under Code\Build\bin; see Code\Build\bin\readme.md.
#include <winsock2.h>        // Winsock must precede windows.h.
#include <windows.h>
#include <dbghelp.h>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>           // serverScreenLogPath - strrchr / strncpy
#include <csignal>           // the SIGABRT footprint - an abort() is invisible to the AV handler
#include <string>

#pragma comment(lib, "dbghelp.lib")

static void logf(const char* fmt, ...);   // defined below - the BF2_LOG mirror

// Rebuilds the engine's single command-line string from argv. The C runtime strips the shell's
// quotes, so an argument containing a space must be quoted again or it splits when rejoined; an
// absolute path such as "D:\Battlefield 2\mods\...\maplist.con" would otherwise lose everything
// after its first space.
static void appendCommandLineArg(std::string& out, const char* arg, bool first)
{
    if (!first)
        out += ' ';
    const std::string value(arg ? arg : "");
    if (value.empty() || value.find_first_of(" \t") != std::string::npos)
    {
        out += '"';
        out += value;
        out += '"';
    }
    else
    {
        out += value;
    }
}

// Resolves fault addresses through DbgHelp. Keep image-relative RVAs in diagnostics:
// /force-resolved missing functions may point at RVA zero without a symbol or source line.
static void describe(SIZE_T addr, SIZE_T base)
{
    char symbuf[sizeof(SYMBOL_INFO) + 512];
    SYMBOL_INFO* sym = (SYMBOL_INFO*)symbuf;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen   = 500;

    DWORD64 disp = 0;
    const bool haveSym = SymFromAddr(GetCurrentProcess(), addr, &disp, sym) != FALSE;

    IMAGEHLP_LINE64 line;
    line.SizeOfStruct = sizeof(line);
    DWORD lineDisp = 0;
    const bool haveLine = SymGetLineFromAddr64(GetCurrentProcess(), addr, &lineDisp, &line) != FALSE;

    if (haveLine)
        logf("            %s:%lu", line.FileName, line.LineNumber);
    else if (haveSym)
        logf("            %s + 0x%IX", sym->Name, (SIZE_T)disp);
    else
        logf("            (no symbol - RVA 0x%08IX)", addr - base);

    logf("\n");
}

#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Main/BF2.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Core/System.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/IO/File/FileManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/IO/Console/Console.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Core/ConsoleObjects.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Core/ClassManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Core/Ids.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/IO/File/File.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameServer/ServerSettings.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameServer/ServerInfo.h"          // the tick-services footprint
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameServer/ServerVoteManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameServer/BanList.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/AbilityGLCompTemplate.h"   // [TEMP] the radio/ability read-back probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/RadioGLCompTemplate.h"    // [TEMP] the radio/ability read-back probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/WindAffectorTemplate.h"   // [TEMP] WindAffector production read-back
#include "D:/LC/BF2Rebuilt/Code/BF2/Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Scene/Object/ObjectTemplateUtil.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/IObjectTemplate.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Base/SimpleObjectTemplate.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/PlayerControlObjectTemplate.h" // [TEMP] vehicle/exit binding probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/ObjectLayoutAccess.h" // [TEMP] ATS runtime node probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Projectiles/GenericProjectile.h" // [TEMP] ATS runtime node probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Projectiles/GenericProjectileTemplate.h" // [TEMP] ATS binding probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Physics/IPhysicsNode.h" // [TEMP] ATS runtime node probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameServer/GameServer.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/GameLogic.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/ServerGameLogic.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Game/SoldierTemplate.h"   // e10b probe   // the probe derives from it
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Game/PlayerControlObject.h" // [TEMP] stamina binding/math probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Animation/BoneAnimation/AnimationSystem.h"   // e10c probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Animation/BoneAnimation/CameraSpring.h"     // e10d probe
#include "D:/LC/BF2Rebuilt/Code/BF2/Scene/Object/PlayerManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Core/SettingsRepostitory.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/MapList.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/IModInfo.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/IO/Network/NetworkManager.h"              // e10's post-load counts
#include "D:/LC/BF2Rebuilt/Code/BF2/IO/Network/INetworkable.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/IGameServer.h"      // BF2_AI_PLAYER
#include "D:/LC/BF2Rebuilt/Code/BF2/IO/Network/ProtocolLayer/NetServer.h"  // BF2_NET_TX_TEST
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/IPlayer.h"    // BF2_AI_PLAYER
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/SpawnSystem/SpawnManager.h"   // BF2_AI_PLAYER
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/SpawnSystem/SpawnGroup.h"     // BF2_AI_PLAYER
#include "D:/LC/BF2Rebuilt/Code/BF2/AI/AIInterface/AIInstance.h"                 // BF2_AI_PLAYER
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/ScoreSystem/ScoreManager.h"   // ticket gate
namespace dice { namespace hfe { namespace world { extern IBase* scoreManager; } } }   // ds:0x8AD1740
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/SpawnSystem/SpawnManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/GameLogic/SpawnSystem/SpawnGroup.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Scene/Object/ObjectManager.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/IObject.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Game/ControlPoint.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/IControlPoint.h"           // the round footprint at the fault
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/ObjectLayoutAccess.h"      // createdOnLoad - the spawned-roots footprint
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/Base/BundleTemplate.h"       // the template-side footprint (getNoTemplates)
#include "D:/LC/BF2Rebuilt/Code/BF2/Game/Objects/ControlPointTemplate.h"
#include "D:/LC/BF2Rebuilt/Code/BF2/Physics/PhysicsNodeManager.h"            // e10's mobile-node count
#include "D:/LC/BF2Rebuilt/Code/BF2/Physics/IPhysicsNode.h"

// CPython entry points used by the fixture. Declared here because this harness does not
// include Python headers; bootrun.cmd links the embedded interpreter.
extern "C" void Py_Initialize(void);
extern "C" int  PyRun_SimpleString(const char*);

// Harness-local singleton declarations for the optional e10 fixtures.
namespace dice { namespace hfe {
extern IBase* squadManager;     // Core/Globals.cpp; e10 pre-seats it (see the probe)
extern IBase* spawnManager;     // Core/Globals.cpp; e10's post-load spawn-group count
namespace world { extern PhysicsNodeManager* physicsNodeManager; }   // ds:0x8B27844; e10's mobile-node count
} }

namespace {

using namespace dice::hfe;      // the stub signatures below quote the interface headers verbatim,
                                // which spell their types dice::hfe-relative

class ProbeGameServer : public dice::hfe::GameServer
{
public:
    ProbeGameServer()
    {
        // The recovered retail constructors deliberately leave these values for init/startServer
        // (and Game leaves currentMapSize unset). The deep e10 probe bypasses that startup slice,
        // so only these otherwise-indeterminate fields receive fixture-owned defaults here.
        m_currentInputIndex = 0;
        m_isPunkBusterEnabled = false;
        m_currentMapSize = 0;
    }
    // Fixture-only interface overrides; inherited methods continue to exercise engine bodies.
    virtual bool say(const std::string&) { return true; } // [TEMP] ProbeGame interface fixture only.
    // These empty overrides suppress ghost diagnostics, raw UDP sends, and event handling in the fixture.
    virtual void debugShowActiveGhosts(int) {}
    virtual void sendUDPPacket(const char*, unsigned short, void*, int) {}
    virtual void handleEvent(EventCategory, unsigned int, EventNode*, void*) {}
};

class ProbeGameLogic : public dice::hfe::ServerGameLogic       // Harness fixture derived from ServerGameLogic.
{
public:
    ProbeGameLogic()
    {
        // Base construction requires the server and supporting singletons to be published first.
    }
    // Fixture-only reference-count and interface-query behavior.
    virtual void addRef() {}
    virtual int getRef() { return 0; }
    virtual int release() { return 0; }
    virtual void* queryInterface(unsigned int) const { return 0; }
    // close is recovered on ServerGameLogic (bool); inherit its teardown instead of shadowing it.
    virtual void restartDemoPlayback() {}
    virtual unsigned int getStartPreStoredNetID() const { return 0; }
    virtual unsigned int getEndPreStoredNetID() const { return 0; }
    // Fixture returns an empty vehicle name by reference; keep the interface signature.
    virtual const std::string& getTeamDropVehicle(int team) const
    { static const std::string s; return s; }
    virtual void setNumDropVehiclesForTeam(int team, int n) {}
    virtual int getNumDropVehiclesForTeam(int team) { return 0; }
    virtual void setDropVehicleDeviationRadius(int, int) {}
    virtual int getDropVehicleDeviationRadius() { return 0; }
    virtual int getDropVehicleDeviationMinRadius() { return 0; }
    virtual void suicide() {}
    virtual bool killPlayer(world::IPlayer*, bool, bool) { return false; }      // Fixture returns false.
    virtual bool reSpawnPlayer(world::IPlayer*) { return false; }               // Fixture returns false.
    virtual bool selectSpawnGroup(world::IPlayer*, int) { return false; }       // Fixture returns false.
    virtual bool selectTeam(world::IPlayer*, int) { return false; }             // Fixture returns false.
    virtual bool selectKitAndUnlockLevel(world::IPlayer*, int, int) { return false; }   // Fixture returns false.
    virtual bool isLevelLoadedXP() { return false; }
    virtual void setDefaultTimeToNextAIWave(float) {}
    virtual float getDefaultTimeToNextAIWave() { return 0; }
    virtual void setMaximumLevelViewDistance(float) {}
    virtual float getMaximumLevelViewDistance() const { return 0; }
    virtual void setExtraUAVVehicleFlightHeight(float) {}
    virtual float getExtraUAVVehicleFlightHeight() const { return 0; }
    virtual void setFogStartEndAndBase(const Vec4&) {}
    virtual const Vec4& getFogStartEndAndBase() const { static Vec4 s; return s; }
    virtual void refreshTriggerables() {}
    virtual bool giveDamage(world::IObject*, float, world::IPlayer*, std::list<WeakPtr<world::IPlayer> >*, world::IObject*, int, world::IObject*, world::IObject*, Vec3, Vec3, int, bool, bool, bool) { return false; }   // Fixture returns false.
    virtual void handleExplosion(Vec3, float, int, float, world::IPlayer*, world::IArmorGLComp*, int, world::IObject*, world::IObject*, float, world::IObject*, world::IObject*, float, bool, bool) {}
    virtual void handlePickup(world::IObject*, world::IPlayer*, world::IObject*, bool) {}
    virtual void handleDrop(world::IObject*, world::IPlayer*, world::IObject*, bool) {}
    virtual bool handleCollisionForProjectile(world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, world::CollisionFaceInfo*, world::CollisionFaceInfo*) { return false; }
    virtual bool handleCollisionForReplenisher(bool, world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&, int, int) { return false; }
    virtual bool handleCollisionForResurrecter(world::IObject*, world::IObject*, float, float, const Vec3&, const Vec3&, const Vec3&, int, int) { return false; }
    virtual void handleCollision(world::IObject*, world::IObject*, const Vec3&, const Vec3&, const Vec3&, int, int, world::CollisionFaceInfo*, world::CollisionFaceInfo*) {}
    virtual bool heal(world::IObject*, float, world::IPlayer*, int, int, float*, bool, bool) { return false; }
    virtual bool replenishAmmo(world::IObject*, float, world::IPlayer*, int, int, float*, bool) { return false; }
    virtual void doImpact(int, int, world::MaterialCollisionType, const Vec3&, const Vec3&, const Vec3&, world::IObject*, world::IObject*, world::IProjectileObject*, world::CollisionFaceInfo*, Vec3, bool) {}
    virtual void playCollisionEffect(Vec3, const world::IObjectTemplate*, Vec3, Vec3) {}
    virtual void triggerSpecialGameEvent(SpecialGameEventType, void*) {}
    // remoteCommand is recovered on GameLogic (bool); keep its event dispatch visible here.
    virtual float getTimeUntilRestartMap() { return 0; }
    // Fixture returns no scheduled state-change delay.
    virtual float getTimeToNextStateChange() { return 0; }
    // Control-point registration is inherited; do not shadow it with fixture overrides.
    virtual void addFlag(world::IObject*) {}
    virtual void* getFlagMap() const { return 0; }
    virtual void removeFlag(world::IObject*) {}
    // [FRONTIER] This fixture suppresses strategic-object registration and removal.
    // Remove these overrides when the probe is ready to exercise those engine operations.
    virtual void addStrategicObject(world::IObject*) {}
    virtual void removeStrategicObject(world::IObject*) {}
    virtual world::IObject* findBestTargetObject(const Vec3&, const Vec3&, world::TargetType, float, float, world::IObject*, world::IObject*, float, const world::IObject*, bool, bool, bool) { return 0; }
    virtual void toggleLocalFreeCamera() {}
    virtual void togglePause() {}
    virtual void setPause(bool) {}
    virtual bool getPause() const { return false; }
    // Preserve inherited precache state so SpawnPoint follows its normal gate.
    virtual bool getEnableVoHelp() { return false; }
    virtual void stopPlayingVoMessages() {}
    virtual void setSupplyDropHeight(float) {}
    virtual float getSupplyDropHeight() { return 0; }
    virtual void setVehicleDropHeight(float) {}
    virtual float getVehicleDropHeight() { return 0; }
    virtual void setSupplyDropNumSecsToLive(float) {}
    virtual float getSupplyDropNumSecsToLive() { return 0; }
    virtual void setFeslEnvironment(int) {}
    virtual int getFeslEnvironment() { return 0; }
    virtual void startupVoipClient() {}
    virtual bool getOutputPlayerStats() { return false; }
    virtual bool getAirMouseInverted() { return false; }
    virtual void setAirMouseSensitivity(float) {}
    virtual float getAirMouseSensitivity() { return 0; }
    virtual void becomeSquadLeader(bool) {}
    // TicketHistory storage is also real on ServerGameLogic; do not replace it in the probe.
    virtual void setHitIndicationThreshold(float) {}
    virtual float getHitIndicationThreshold() { return 0; }
    virtual float getUAVActiveTime() const { return 0; }
    virtual float getSatelliteScanActiveTime() const { return 0; }
    virtual float getUAVReloadTime() const { return 0; }
    virtual float getUAVRadius() const { return 0; }
    virtual float getSatelliteScanReloadTime() const { return 0; }
    virtual float getSatelliteScanVisibleTime() const { return 0; }
    virtual world::IObject* getGroundDecalObject() { return 0; }
    virtual void AddInvalidDropVehicleObject(std::string, bool) {}
    virtual std::list<std::pair<std::string, bool> >* GetInvalidVehicleDropObjectList() { return 0; }
    virtual void setDebugMissiles(int) {}
    virtual int getDebugMissiles() { return 0; }
    virtual void setDebugReplenish(int) {}
    virtual int getDebugReplenish() { return 0; }
    virtual void setTeamDropVehicle(int, const std::string&) {}
    virtual void ClearDropVehicles() {}
    virtual void setOutputPlayerStats(bool) {}
};

} // namespace


// Raw minizip probe hooks - the library is C, so bare extern "C" declarations reach the vendored
// unzip.c without dragging its headers (and their include dirs) into this harness's compile line.
extern "C" {
    typedef void* probe_unzFile;
    probe_unzFile unzOpen(const char* path);
    int unzClose(probe_unzFile file);
}
namespace dice { namespace hfe { namespace io {
    std::list<ConsoleObject*>& fileManagerConsoleObjects();
} } }

namespace dice { namespace hfe { extern System* g_system; } }
namespace dice { namespace hfe { extern ClassManager* classManager; } }   // Core/Globals.cpp

static const int kFrames = 5;

// BF2_LOG mirrors harness diagnostics to a file while preserving console output.
// The status screen needs a real console; redirecting stdout to a pipe disables its backend.
static FILE* g_log = 0;

// BF2_SERVER_SCREEN gives the status monitor exclusive use of the console while CRT
// stdout goes to a log. Unset enables it for a real console; 0 disables it and 1 forces it.
// Open a fresh conout$ handle before freopen, then restore STD_OUTPUT_HANDLE: freopen
// closes the original stdout handle and can also redirect the process standard handle.
// Fault diagnostics are echoed to the console; stderr, including [FRONTIER] reports, stays visible.
static HANDLE g_consoleOut   = INVALID_HANDLE_VALUE;   // the real console, held across the redirect
static bool   g_serverScreen = false;
static bool   g_screenTaken  = false;                  // the fault path has wiped the screen already

static bool haveRealConsole()
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    const HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    return h != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(h, &info) != FALSE;
}

// Returns BF2_LOG when set, otherwise bf2_server.log beside the executable.
// Keep the default log out of the reference game directory, which may be the working directory.
static void serverScreenLogPath(char* out, size_t cap)
{
    out[0] = '\0';
    if (GetEnvironmentVariableA("BF2_LOG", out, (DWORD)cap) > 0 && out[0])
        return;

    char exe[MAX_PATH] = {0};
    if (GetModuleFileNameA(NULL, exe, MAX_PATH) == 0)
    {
        strncpy(out, "bf2_server.log", cap - 1);
        out[cap - 1] = '\0';
        return;
    }
    char* slash = strrchr(exe, '\\');
    if (slash) slash[1] = '\0'; else exe[0] = '\0';
    _snprintf(out, cap - 1, "%sbf2_server.log", exe);
    out[cap - 1] = '\0';
}

static void serverScreenArm()
{
    char mode[16] = {0};
    const bool explicitMode = GetEnvironmentVariableA("BF2_SERVER_SCREEN", mode, sizeof(mode)) > 0;
    const bool want = explicitMode ? (atoi(mode) != 0) : haveRealConsole();
    if (!want)
        return;

    // not GetStdHandle - see the block above. This handle has to survive freopen closing fd 1.
    g_consoleOut = CreateFileA("CONOUT$", GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
    if (g_consoleOut == INVALID_HANDLE_VALUE)
        return;                                 // no console to hand over - leave stdout alone

    char path[512];
    serverScreenLogPath(path, sizeof(path));
    if (!freopen(path, "w", stdout))
    {
        CloseHandle(g_consoleOut);
        g_consoleOut = INVALID_HANDLE_VALUE;    // no log to hide the chatter in - leave it on screen
        return;
    }
    SetStdHandle(STD_OUTPUT_HANDLE, g_consoleOut);      // see the block above - load-bearing
    setvbuf(stdout, 0, _IOLBF, 4096);
    g_serverScreen = true;

    // [CHOSEN, not recovered] - a harness touch, not engine behaviour. The shipped server's own
    // title (if it sets one) has not been read out of the PE.
    SetConsoleTitleA("Battlefield 2 Dedicated Server");
}

// Wipe the status screen and give the cursor back, so a fault footprint is written onto a clean
// terminal instead of interleaving with the half-painted screen underneath it. Idempotent.
static void serverScreenTakeBack()
{
    if (!g_serverScreen || g_screenTaken || g_consoleOut == INVALID_HANDLE_VALUE)
        return;
    g_screenTaken = true;

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(g_consoleOut, &info))
        return;

    const COORD home = { 0, info.srWindow.Top };
    DWORD written = 0;
    FillConsoleOutputCharacterA(g_consoleOut, ' ',
                                (DWORD)info.dwSize.X * (DWORD)(info.srWindow.Bottom - info.srWindow.Top + 1),
                                home, &written);
    SetConsoleCursorPosition(g_consoleOut, home);

    CONSOLE_CURSOR_INFO cursor;
    if (GetConsoleCursorInfo(g_consoleOut, &cursor))
    {
        cursor.bVisible = TRUE;                 // the monitor's destructor never runs on this path
        SetConsoleCursorInfo(g_consoleOut, &cursor);
    }
}

// Hand a clean terminal to the screen before the frame loop starts, so its first paint is not
// preceded by whatever scrollback the launch left behind.
static void serverScreenClear()
{
    if (!g_serverScreen || g_consoleOut == INVALID_HANDLE_VALUE)
        return;

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(g_consoleOut, &info))
        return;

    const COORD home = { 0, 0 };
    DWORD written = 0;
    FillConsoleOutputCharacterA(g_consoleOut, ' ',
                                (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y, home, &written);
    FillConsoleOutputAttribute(g_consoleOut, info.wAttributes,
                               (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y, home, &written);
    SetConsoleCursorPosition(g_consoleOut, home);

    // Park the window at the top of the buffer: the monitor paints at srWindow.Top, and leaving the
    // window scrolled down would put the screen in the middle of a page of blanks.
    SMALL_RECT top = info.srWindow;
    const SHORT h = (SHORT)(top.Bottom - top.Top);
    top.Top = 0;
    top.Bottom = h;
    SetConsoleWindowInfo(g_consoleOut, TRUE, &top);
}

static void logOpen()
{
    // In server-screen mode stdout is the log (serverScreenArm freopen'd it there), so opening a
    // second writer on the same path would interleave two buffered streams into one file.
    if (g_serverScreen)
        return;

    char buf[512] = {0};
    if (GetEnvironmentVariableA("BF2_LOG", buf, sizeof(buf)) > 0 && buf[0])
        g_log = fopen(buf, "w");
}

static void logf(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    if (g_log)
    {
        va_start(ap, fmt);
        vfprintf(g_log, fmt, ap);
        va_end(ap);
        fflush(g_log);
    }
    // Echo fault diagnostics to the console because stdout is redirected in server-screen mode.
    if (g_serverScreen && g_consoleOut != INVALID_HANDLE_VALUE)
    {
        char buf[4096];
        va_start(ap, fmt);
        const int n = _vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
        va_end(ap);
        buf[sizeof(buf) - 1] = '\0';
        DWORD echoed = 0;
        WriteConsoleA(g_consoleOut, buf,
                      (DWORD)(n < 0 ? strlen(buf) : (size_t)n), &echoed, 0);
    }
}

static LONG CALLBACK onFault(EXCEPTION_POINTERS* ep)
{
    const EXCEPTION_RECORD* r = ep->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;

    serverScreenTakeBack();             // the footprint goes on a clean terminal, not over the screen
    SIZE_T base = (SIZE_T)GetModuleHandleA(NULL);
    logf("\n*** ACCESS VIOLATION ***\n");
    logf("    fault at 0x%08IX  (RVA 0x%08IX)\n", (SIZE_T)r->ExceptionAddress,
           (SIZE_T)r->ExceptionAddress - base);
    describe((SIZE_T)r->ExceptionAddress, base);
    if ((SIZE_T)r->ExceptionAddress == base)
        logf("    ^ RVA 0 = a /FORCE-resolved UNWRITTEN function was called\n");
    // ExceptionInformation[0] distinguishes read (0), write (1), and execute (8) faults.
    // An RVA-zero call is an execute fault.
    if (r->NumberParameters >= 2)
    {
        const ULONG_PTR kind = r->ExceptionInformation[0];
        logf("    %s 0x%08IX\n",
               kind == 8 ? "executing" : (kind ? "writing" : "reading"),
               (SIZE_T)r->ExceptionInformation[1]);
    }

    // [TEMP] Dump registers and attribute stack addresses to modules for null-call diagnosis.
    {
        const CONTEXT* c = ep->ContextRecord;
        logf("    registers: eip=%08X esp=%08X ebp=%08X eax=%08X ebx=%08X ecx=%08X edx=%08X esi=%08X edi=%08X\n",
             c->Eip, c->Esp, c->Ebp, c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi);
        const SIZE_T* raw = (const SIZE_T*)c->Esp;
        for (int i = 0; i < 24; ++i)
        {
            SIZE_T v = raw[i];
            HMODULE hm = 0;
            char name[260] = "";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)v, &hm) && hm)
            {
                GetModuleFileNameA(hm, name, sizeof(name));
                const char* slash = strrchr(name, '\\');
                logf("        [esp+%3d] %08X  %s+0x%X\n", i * 4, v, slash ? slash + 1 : name, (unsigned)(v - (SIZE_T)hm));
            }
            else
                logf("        [esp+%3d] %08X\n", i * 4, v);
        }
    }
    logf("    return addresses on the stack (image-relative):\n");
    SIZE_T* sp = (SIZE_T*)ep->ContextRecord->Esp;
    SIZE_T  end = base + 0x400000;   // Diagnostic scan bound.
    int shown = 0;
    for (int i = 0; i < 512 && shown < 10; ++i)
    {
        SIZE_T v = sp[i];
        if (v > base + 0x1000 && v < end)
        {
            logf("        [esp+%4d] RVA 0x%08IX\n", i * 4, v - base);
            describe(v, base);
            ++shown;
        }
    }
    // Report registered-object counts at the fault.
    if (dice::hfe::world::objectManager)
        logf("    world at the fault: ObjectManager registered = %u\n",
               (unsigned)dice::hfe::world::objectManager->getAllRegisteredObjects().size());
    // Report game status and round state at the fault.
    if (dice::hfe::gameLogic && dice::hfe::world::objectManager)
    {
        using namespace dice::hfe;
        const std::map<unsigned int, world::IObject*>& objs = world::objectManager->getAllRegisteredObjects();
        unsigned pcos = 0, cps = 0, cpTeamed = 0;
        for (std::map<unsigned int, world::IObject*>::const_iterator it = objs.begin(); it != objs.end(); ++it)
        {
            const world::IObjectTemplate* t = it->second->getTemplate();
            if (!t) continue;
            if (t->getClassID() == world::CID_PlayerControlObjectTemplate) ++pcos;
            if (t->getClassID() == world::CID_ControlPointTemplate)
            {
                ++cps;
                world::IControlPoint* cp = static_cast<world::IControlPoint*>(it->second->queryInterface(world::IID_IControlPoint));
                if (cp && cp->getTeam() != 0) ++cpTeamed;
            }
        }
        logf("    round at the fault: gameStatus = %d, PCO-templated objects = %u, control points = %u (team != 0: %u)\n",
               gameLogic->getGameStatus(), pcos, cps, cpTeamed);
        // The objects created after the load (setCreatedOnLoadFlag is real now, so the roots the
        // spawners made are exactly the !createdOnLoad roots): their template names, first dozen.
        unsigned spawned = 0;
        std::string names;
        for (std::map<unsigned int, world::IObject*>::const_iterator it = objs.begin(); it != objs.end(); ++it)
        {
            world::IObject* o = it->second;
            if ((o->getFlags() & 0x01000000u) && !world::object_detail::LayoutAccess::createdOnLoad(o))
            {
                ++spawned;
                if (spawned <= 12 && o->getTemplate())
                    names += (names.empty() ? "" : ", ") + o->getTemplate()->getName();
            }
        }
        logf("    round at the fault: roots created after the load = %u [%s%s]\n", spawned, names.c_str(),
               spawned > 12 ? ", ..." : "");
        // Report vehicle component, physics-node, and response state at the fault.
        std::string trees;
        unsigned shown = 0;
        for (std::map<unsigned int, world::IObject*>::const_iterator it = objs.begin(); it != objs.end() && shown < 6; ++it)
        {
            world::IObject* o = it->second;
            const world::IObjectTemplate* t = o->getTemplate();
            if (!t || t->getClassID() != world::CID_PlayerControlObjectTemplate || o->getParent()) continue;
            std::vector<world::IObject*> subtree;
            world::getChilds(o, subtree, 0, 0);
            char buf[160];
            sprintf(buf, "%s%s: %u nodes (", trees.empty() ? "" : ", ", t->getName().c_str(), (unsigned)subtree.size());
            trees += buf;
            for (size_t n = 0; n < subtree.size() && n < 12; ++n)     // the node templates, in getChilds order
            {
                const world::IObjectTemplate* nt = subtree[n]->getTemplate();
                sprintf(buf, "%s%s/%u", n ? " " : "", nt ? nt->getName().c_str() : "?", nt ? nt->getClassID() : 0u);
                trees += buf;
            }
            trees += ")";
            ++shown;
        }
        logf("    round at the fault: vehicle subtrees [%s]\n", trees.c_str());
        // ...and the template side of the same question: the tank template's child list, and
        // whether its .con-created sub-part templates exist at all.
        if (world::objectTemplateManager)
        {
            const char* probeNames[] = { "tnk_type98", "TNK_Type98_Turret", "TNK_Type98_Engine", "TNK_Type98_Rwheel_01", "S_tnk_type98_Ambient", "tnk_type98_pilot_EntryPoint" };
            std::string tinfo;
            for (unsigned k = 0; k < 6; ++k)
            {
                world::IObjectTemplate* t = world::objectTemplateManager->getTemplate(std::string(probeNames[k]));
                char buf[200];
                if (!t) sprintf(buf, "%s%s: MISSING", tinfo.empty() ? "" : ", ", probeNames[k]);
                else
                {
                    world::BundleTemplate* b = static_cast<world::BundleTemplate*>(t->queryImplementation(world::CID_BundleTemplate));
                    sprintf(buf, "%s%s: cid %u, children %d", tinfo.empty() ? "" : ", ", probeNames[k], t->getClassID(), b ? b->getNoTemplates() : -1);
                }
                tinfo += buf;
            }
            logf("    round at the fault: templates [%s]\n", tinfo.c_str());
        }
    }
    fflush(stdout);
    ExitProcess(7);
}

// [TEMP] Report SIGABRT before MSVC abort fast-fails, which may bypass the exception handler.
static void onAbort(int)
{
    serverScreenTakeBack();             // as onFault - see serverScreenArm's block
    SIZE_T base = (SIZE_T)GetModuleHandleA(NULL);
    logf("\n*** ABORT (SIGABRT) ***\n");
    void* frames[64];
    const USHORT n = CaptureStackBackTrace(0, 64, frames, 0);
    for (USHORT i = 0; i < n; ++i)
    {
        logf("        [%2u] RVA 0x%08IX\n", (unsigned)i, (SIZE_T)frames[i] - base);
        describe((SIZE_T)frames[i], base);
    }
    fflush(stdout);
    ExitProcess(8);
}

#pragma init_seg(compiler)
struct ArmFirst
{
    ArmFirst()
    {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS);
        SymInitialize(GetCurrentProcess(), 0, TRUE);
        AddVectoredExceptionHandler(1, onFault);
        signal(SIGABRT, onAbort);
        // Arm screen mode before opening the log or printing: it redirects stdout and restores
        // the console handle that the status monitor will capture.
        serverScreenArm();
        logOpen();
        printf("[boot] fault handler armed (before engine static init)\n");
        if (g_serverScreen)
        {
            char path[512];
            serverScreenLogPath(path, sizeof(path));
            printf("[boot] BF2_SERVER_SCREEN - stdout redirected here (%s); the terminal belongs to\n"
                   "       StatusMonitor. BF2_SERVER_SCREEN=0 restores the old behaviour.\n", path);
        }
        fflush(stdout);
    }
};
static ArmFirst g_armFirst;

// Synthetic-player probe uses queryInterface and retains the adjusted IGameServer pointer.
// Never cast that secondary interface directly to GameServer.
namespace dice { namespace hfe { extern IBase* spawnManager; } }   // ds:0x8AD3A00

static void spawnSyntheticPlayer(int team, int kit)
{
    using namespace dice::hfe;

    if (!game)
    {
        printf("[boot] BF2_AI_PLAYER: no `game` - nothing to spawn onto\n"); fflush(stdout);
        return;
    }

    IGameServer* sv = (IGameServer*)game->queryInterface(IID_IGameServer);
    if (!sv)
    {
        printf("[boot] BF2_AI_PLAYER: queryInterface(IID_IGameServer) = 0\n"); fflush(stdout);
        return;
    }

    // Report the AI singleton and active spawn groups before attempting creation.
    {
        printf("[boot] BF2_AI_PLAYER: ai::g_aiInstance = %p%s\n", (void*)ai::g_aiInstance,
               ai::g_aiInstance ? "" : "   <- NULL: createPlayer will refuse with \"AI not enabled\"");
        SpawnManager* spawn = static_cast<SpawnManager*>(spawnManager);
        if (!spawn)
            printf("[boot] BF2_AI_PLAYER: spawnManager = 0\n");
        else
        {
            const std::list<int>& g = spawn->getGroupsForTeam(team);
            printf("[boot] BF2_AI_PLAYER: getGroupsForTeam(%d) = %d group(s):", team, (int)g.size());
            for (std::list<int>::const_iterator i = g.begin(); i != g.end(); ++i)
            {
                ISpawnGroup* sg = spawn->getSpawnGroup(*i);
                printf(" [%d %s]", *i, !sg ? "no-group" : (sg->getActive(true) ? "ACTIVE" : "inactive"));
            }
            printf("\n");
        }
        fflush(stdout);
    }

    const int before = world::playerManager ? world::playerManager->getNumberOfPlayers() : -1;

    // BF2_PLAYER_KIND selects AI creation or the local-human path (default: AI).
    // AI creation requires an active spawn group and AI singleton. The human fixture uses
    // player id 255 and exercises local listen-server behavior; it does not test remote joining.
    char kindBuf[16] = {0};
    GetEnvironmentVariableA("BF2_PLAYER_KIND", kindBuf, sizeof(kindBuf));
    const bool human = (kindBuf[0] == 'h' || kindBuf[0] == 'H');

    const int id = human ? sv->createPlayer(std::string("BotOne"), false)
                         : sv->createAndSpawnAIPlayer(std::string("BotOne"), team, kit);
    printf("[boot] BF2_AI_PLAYER: kind = %s\n", human ? "human (createPlayer, isAI=false)"
                                                       : "ai (createAndSpawnAIPlayer)");

    const int after = world::playerManager ? world::playerManager->getNumberOfPlayers() : -1;
    printf("[boot] BF2_AI_PLAYER: -> id %d (playerManager count %d -> %d)\n",
           id, before, after);

    if (id >= 0 && world::playerManager)
    {
        world::IPlayer* p = world::playerManager->getPlayerFromId(id);
        if (p)
        {
            printf("[boot] BF2_AI_PLAYER: player %d name \"%s\" team %d kit %d spawnGroup %d "
                   "isAI %d netId %u\n", p->getId(), p->getName().c_str(), p->getTeam(),
                   p->getKit(), p->getSpawnGroup(), (int)p->getIsAIPlayer(),
                   p->getNetworkable() ? (unsigned)p->getNetworkable()->m_networkableId : 0u);
            // Report the state that controls entry into checkPlayerTriggers.
            printf("[boot] BF2_AI_PLAYER: isAlive %d  vehicle %p  freeCamera %p  (equal %d)  "
                   "overheadCam %d  isRemote %d\n",
                   (int)p->getIsAlive(), (void*)p->getVehicle(), (void*)p->getFreeCamera(),
                   (int)(p->getVehicle() == p->getFreeCamera()),
                   (int)p->getOverheadCameraEnabled(), (int)p->getIsRemote());

            // Fixture shortcut: set team before spawn group because setTeam clears the group.
            // This bypasses the player-facing team and spawn selection flow.
            {
                char tBuf[16] = {0};
                const int wantTeam = GetEnvironmentVariableA("BF2_PLAYER_TEAM", tBuf, sizeof(tBuf)) > 0
                                         ? atoi(tBuf) : 1;

                p->setTeam(wantTeam, false);        // first - it clears the spawn group
                printf("[boot] BF2_AI_PLAYER: setTeam(%d) -> team %d, spawnGroup %d (cleared by setTeam)\n",
                       wantTeam, p->getTeam(), p->getSpawnGroup());
                fflush(stdout);

                // Pick the first spawn group for this team that is active for a human and actually
                // has a spawn point to offer. Both tests are the ones spawnPlayer itself applies.
                int chosen = 0;
                SpawnManager* spawn2 = static_cast<SpawnManager*>(spawnManager);
                if (spawn2)
                {
                    const std::list<int>& gs = spawn2->getGroupsForTeam(wantTeam);
                    for (std::list<int>::const_iterator i = gs.begin(); i != gs.end(); ++i)
                    {
                        ISpawnGroup* sg = spawn2->getSpawnGroup(*i);
                        if (!sg || !sg->getActive(false)) continue;
                        if (sg->getTeam() != wantTeam)   continue;
                        if (!sg->getSpawnPoint(false))   continue;
                        chosen = *i;
                        break;
                    }
                }

                if (!chosen)
                    printf("[boot] BF2_AI_PLAYER: no ACTIVE human spawn group for team %d - "
                           "spawnPlayer will not be reached\n", wantTeam);
                else
                {
                    p->setSpawnGroup(chosen);       // second - see the order note
                    printf("[boot] BF2_AI_PLAYER: setSpawnGroup(%d) -> team %d, spawnGroup %d, kit %d\n",
                           chosen, p->getTeam(), p->getSpawnGroup(), p->getKit());

                    // Fixture supplies team tickets normally seeded by round reset; spawning refuses at zero tickets.
                    if (dice::hfe::world::scoreManager)
                    {
                        reinterpret_cast<dice::hfe::world::ScoreManager*>(dice::hfe::world::scoreManager)
                            ->getTeamScore(wantTeam)->setTickets(50);
                        printf("[boot] BF2_AI_PLAYER: seeded team %d with 50 tickets "
                               "(stands in for ServerGameLogic::reset)\n", wantTeam);
                    }
                }
                fflush(stdout);
            }
        }
        else
            printf("[boot] BF2_AI_PLAYER: getPlayerFromId(%d) = 0 - created but not registered\n", id);
    }
    fflush(stdout);
}

int main(int argc, const char** argv)
{
    printf("[boot] static initialisation SURVIVED\n"); fflush(stdout);

    dice::hfe::g_system = new dice::hfe::System();
    printf("[boot] System constructed\n"); fflush(stdout);

    std::string commandLine;
    for (int i = 1; i < argc; ++i)
        appendCommandLineArg(commandLine, argv[i], i == 1);

    dice::hfe::BF2* bf2 = new dice::hfe::BF2();
    printf("[boot] BF2 constructed\n"); fflush(stdout);

    printf("[boot] calling BF2::init(\"%s\") ...\n", commandLine.c_str()); fflush(stdout);
    if (!bf2->init(commandLine))
    {
        printf("[boot] BF2::init returned FALSE (argument parsing failed)\n");
        return 42;
    }
    printf("[boot] BF2::init returned TRUE\n"); fflush(stdout);
    // [TEMP] Read-only configuration evidence after the real production startup.
    if (GetEnvironmentVariableA("BF2_LISTENER_PROBE", 0, 0) != 0)
    {
        printf("[listener] serverIP=%s gamePort=%d interfaceIP=%s queryPort=%d UseAI=%d AIDLL=%s\n",
               serverSettings->m_serverIP.c_str(), serverSettings->m_serverPort,
               serverSettings->m_interfaceIP.c_str(), serverSettings->m_gameSpyPort,
               (int)settings->boolGet("UseAI", false),
               settings->stringGet("AIDLL", std::string("")).c_str());
        fflush(stdout);
    }

    // [TEMP] BF2_NET_TX_TEST exercises bufferPacket and send on a live loopback connection
    // without running the main frame loop. It checks socket transmission independently of gameplay ticks.
    {
        char tb[8] = {0};
        if (GetEnvironmentVariableA("BF2_NET_TX_TEST", tb, sizeof(tb)) > 0 && tb[0] != '0')
        {
            using namespace dice::hfe;
            printf("[tx-test] === TRANSMIT-PATH TEST (bypasses the GameServer send chain) ===\n");
            fflush(stdout);
            io::NetServer* ns = 0;
            IGameServer* sv = game ? (IGameServer*)game->queryInterface(IID_IGameServer) : 0;
            if (sv) ns = sv->getNetServer();
            if (!ns)
            {
                printf("[tx-test] no NetServer (game=%p sv=%p)\n", (void*)game, (void*)sv);
                fflush(stdout);
                return 50;
            }
            // Allow five seconds for the peer handshake, then send on connection zero.
            // bufferPacket does nothing until that connection exists.
            const int connId = 0;                                  // the first assigned id (see the accept)
            printf("[tx-test] NetServer %p bound; giving the peer 5s to connect+ack...\n",
                   (void*)ns); fflush(stdout);
            Sleep(5000);
            // A recognisable payload. The transmit path frames it as a type-15 data packet:
            // [4b type=15][8b id][6+6+32 extended header][16b length][payload].
            unsigned char payload[] = "TXTRANCHE-transmit-path-probe-0123456789";
            const unsigned int n = (unsigned int)sizeof(payload);
            printf("[tx-test] sending %u-byte payload to connection %d, 8x over 4s...\n", n, connId);
            fflush(stdout);
            for (int k = 0; k < 8; ++k)
            {
                ns->bufferPacket(connId, payload, n, n);           // Direct transmit probe.
                ns->send(connId);                                  // Direct transmit probe.
                printf("[tx-test] k=%d: bufferPacket + send(conn %d)\n", k, connId);
                fflush(stdout);
                Sleep(500);
            }
            printf("[tx-test] === done; holding 2s for the peer to finish capturing ===\n");
            fflush(stdout);
            Sleep(2000);
            return 0;
        }
    }

    // BF2_HARNESS_PROBES enables content and console fixtures. They rerun settings, create
    // templates, and publish ProbeGameServer/ProbeGameLogic over the engine singletons.
    // Leave this unset when measuring production startup and ticks.
    const bool harnessProbes = GetEnvironmentVariableA("BF2_HARNESS_PROBES", 0, 0) != 0;
    if (!harnessProbes)
    {
        printf("[boot] harness probe section skipped (BF2_HARNESS_PROBES=1 enables it)\n");
        fflush(stdout);
    }
    if (harnessProbes)
    {
        using namespace dice::hfe;

        char cwd[MAX_PATH] = {0};
        GetCurrentDirectoryA(MAX_PATH, cwd);
        printf("[probe] cwd                    = %s\n", cwd);
        printf("[probe] io::fileManager        = %p\n", (void*)io::fileManager);
        printf("[probe] io::mainConsole        = %p\n", (void*)io::mainConsole);
        fflush(stdout);

        // Check whether initEngine mounted the archives before replaying any mount commands.
        if (io::fileManager)
        {
            printf("[probe] MOUNTED AT INIT? fileExists(<in Objects_server.zip>) = %d\n",
                   (int)io::fileManager->fileExists(
                       "Objects/staticobjects/_asia/village/village_architecture/"
                       "ch_gas_station/ch_gas_station.con"));
            fflush(stdout);
        }

        if (io::fileManager)
        {
            printf("[probe] fileExists(ServerArchives.con) = %d\n",
                   (int)io::fileManager->fileExists("ServerArchives.con"));
            fflush(stdout);
        }

        if (io::mainConsole)
        {
            std::string result;

            // Verify mounting by checking a path that exists only inside Objects_server.zip.
            if (io::fileManager)
            {
                const char* inArchive =
                    "Objects/staticobjects/_asia/village/village_architecture/"
                    "ch_gas_station/ch_gas_station.con";
                printf("[probe] fileExists(<inside Objects_server.zip>) = %d\n",
                       (int)io::fileManager->fileExists(inArchive));
                fflush(stdout);
            }

            // Script success does not include individual command results. Reopen the mount-permission
            // window and test console dispatch, direct mounting, file lookup, and raw ZIP access separately.
            {
                probe_unzFile z = unzOpen("mods/bf2/Objects_server.zip");
                printf("[probe] 0. raw unzOpen(mods/bf2/Objects_server.zip) = %p\n", z);
                if (z) unzClose(z);

                dice::hfe::IBase* handlerBase = dice::hfe::classManager->createInstance(
                    io::CID_ZipArchiveHandler, io::IID_IArchiveHandler, 0);
                printf("[probe] 0. classManager create(ZipArchiveHandler)   = %p\n",
                       (void*)handlerBase);

                if (handlerBase)
                {
                    io::IArchiveHandler* handler =
                        static_cast<io::IArchiveHandler*>((void*)handlerBase);
                    io::IArchive* arc =
                        handler->open("mods/bf2/Objects_server.zip", 0, true);
                    printf("[probe] 0. handler->open(mods/bf2/Objects_server.zip) = %p\n",
                           (void*)arc);
                    if (arc)
                    {
                        printf("[probe] 0. arc->fileExists(<inner, no mount prefix>) = %d\n",
                               (int)arc->fileExists(
                                   "staticobjects/_asia/village/village_architecture/"
                                   "ch_gas_station/ch_gas_station.con"));
                        arc->release();
                    }
                    handler->release();
                }
                fflush(stdout);
            }

            if (io::fileManager)
            {
                io::fileManager->permitMountArchives(true);

                std::string lineResult;
                io::mainConsole->executeLine("fileManager.mountArchive Objects_server.zip Objects",
                                             lineResult, false, true, false, false, false);
                printf("[probe] a. executeLine(mount line) result = \"%s\"\n", lineResult.c_str());
                printf("[probe] a. after console line: fileExists(<inside zip>) = %d\n",
                       (int)io::fileManager->fileExists(
                           "Objects/staticobjects/_asia/village/village_architecture/"
                           "ch_gas_station/ch_gas_station.con"));

                io::IArchive* arc =
                    io::fileManager->mountArchive("Objects_server.zip", "Objects", true);
                printf("[probe] b. direct mountArchive(\"Objects_server.zip\", \"Objects\") = %p\n",
                       (void*)arc);
                printf("[probe] b. (0 here is ambiguous if a. mounted - gate 3 rejects doubles)\n");

                const std::string gate2 =
                    io::fileManager->fileExistsEx("Objects_server.zip", true);
                printf("[probe] c. gate2 fileExistsEx(\"Objects_server.zip\", skip=true) = \"%s\"\n",
                       gate2.c_str());

                io::fileManager->permitMountArchives(false);
                fflush(stdout);

                // Read a packed file through io::File to exercise archive lookup, inflate, and stream reads.
                {
                    io::File packed;
                    const bool opened = packed.open(
                        "Objects/staticobjects/_asia/village/village_architecture/"
                        "ch_gas_station/ch_gas_station.con",
                        io::IFileSystem::FILEMODE_READ,
                        static_cast<io::IFileSystem::FileAccess>(0),
                        2);                             // flags = 2, as OldConsole::run passes
                    if (opened)
                    {
                        char head[97] = {0};
                        const unsigned int got = packed.read(head, 96);
                        for (unsigned int i = 0; i < got; ++i)
                            if (head[i] == '\r' || head[i] == '\n') head[i] = ' ';
                        printf("[probe] d. File::open(<packed .con>) OK - read %u bytes:\n", got);
                        printf("[probe] d.   \"%.72s\"\n", head);
                        packed.close();
                    }
                    else
                    {
                        printf("[probe] d. File::open(<packed .con>) FAILED\n");
                    }
                    fflush(stdout);
                }

                // Round-trip typed console arguments and property results, including optional mount arguments.
                {
                    std::string r;
                    io::mainConsole->executeLine("vars.lockFps 30",
                                                 r, false, true, false, false, false);
                    printf("[probe] e1. vars.lockFps 30    reply = \"%s\"\n", r.c_str());
                    r.clear();
                    io::mainConsole->executeLine("vars.lockFps",
                                                 r, false, true, false, false, false);
                    printf("[probe] e2. vars.lockFps       reply = \"%s\"   (expect 30)\n",
                           r.c_str());
                    r.clear();
                    io::fileManager->permitMountArchives(true);
                    io::mainConsole->executeLine(
                        "fileManager.mountArchive Objects_server.zip Objects 1",
                        r, false, true, false, false, false);
                    io::fileManager->permitMountArchives(false);
                    printf("[probe] e3. mount x y 1        reply = \"%s\"   (argc-3 arm reached)\n",
                           r.c_str());
                    // Check server-setting writes, reads, and the maxPlayers upper clamp.
                    r.clear();
                    io::mainConsole->executeLine("sv.maxPlayers 24",
                                                 r, false, true, false, false, false);
                    printf("[probe] e4. sv.maxPlayers 24   reply = \"%s\"   (\"\" = accepted)\n",
                           r.c_str());
                    r.clear();
                    io::mainConsole->executeLine("sv.maxPlayers",
                                                 r, false, true, false, false, false);
                    printf("[probe] e5. sv.maxPlayers      reply = \"%s\"   (expect 24)\n",
                           r.c_str());
                    r.clear();
                    io::mainConsole->executeLine("sv.maxPlayers 100",
                                                 r, false, true, false, false, false);
                    io::mainConsole->executeLine("sv.maxPlayers",
                                                 r, false, true, false, false, false);
                    printf("[probe] e6. set 100, get back  reply = \"%s\"   (expect 64 - the "
                           "clamp)\n", r.c_str());
                    fflush(stdout);

                    // Load the configured server settings through the console and inspect the resulting values.
                    {
                        printf("[probe] e7. configFile = \"%s\"\n",
                               dice::hfe::serverSettings->m_configFile.c_str());
                        // Reset to a value the FILE does not contain, so the transition proves the
                        // file's own `sv.maxPlayers 64` line executed through the console.
                        r.clear();
                        io::mainConsole->executeLine("sv.maxPlayers 24",
                                                     r, false, true, false, false, false);
                        printf("[probe] e7. pre-load reset:  maxPlayers = %d\n",
                               dice::hfe::serverSettings->m_maxPlayers);
                        const bool loaded = dice::hfe::serverSettings->load();
                        printf("[probe] e7. serverSettings->load() = %d\n", (int)loaded);
                        printf("[probe] e7. after load: maxPlayers = %d (expect the FILE's 64), "
                               "serverName = \"%s\" (no binding yet)\n",
                               dice::hfe::serverSettings->m_maxPlayers,
                               dice::hfe::serverSettings->m_serverName.c_str());
                        fflush(stdout);
                    }

                    // Create a SimpleObject template through console dispatch and verify the active template.
                    {
                        using dice::hfe::world::objectTemplateManager;
                        std::string r;
                        io::mainConsole->executeLine(
                            "ObjectTemplate.create SimpleObject probe_crate",
                            r, false, true, false, false, false);
                        printf("[probe] e8. create SimpleObject probe_crate reply = \"%s\" "
                               "(the template's address - non-0 = built)\n", r.c_str());
                        dice::hfe::world::IObjectTemplate* byName =
                            objectTemplateManager->getTemplate(std::string("probe_crate"));
                        dice::hfe::world::IObjectTemplate* active =
                            objectTemplateManager->getActiveObjectTemplate();
                        printf("[probe] e8. getTemplate(\"probe_crate\") = %p, active = %p, "
                               "same = %d\n", (void*)byName, (void*)active,
                               (int)(byName && byName == active));
                        dice::hfe::world::IObjectTemplate* narrowed =
                            dice::hfe::world::getActiveTemplate(
                                dice::hfe::world::CID_SimpleObjectTemplate);
                        printf("[probe] e8. getActiveTemplate(CID_SimpleObjectTemplate) = %p "
                               "(the property-binding bridge)\n", (void*)narrowed);
                        r.clear();
                        io::mainConsole->executeLine(
                            "ObjectTemplate.create NoSuchClass probe_typo",
                            r, false, true, false, false, false);
                        printf("[probe] e8. create NoSuchClass reply = \"%s\" (expect 0 - typo "
                               "clears the cursor), active now = %p\n", r.c_str(),
                               (void*)objectTemplateManager->getActiveObjectTemplate());
                        fflush(stdout);
                    }

                    // [TEMP] [CHOSEN, not recovered] BF2_ATS_BINDING_PROBE checks projectile console bindings
                    // and the active-template cursor, then exits before the broader e9/e10 fixtures.
                    if (GetEnvironmentVariableA("BF2_ATS_BINDING_PROBE", 0, 0) != 0)
                    {
                        using namespace dice::hfe::world;
                        std::string reply;
                        io::mainConsole->executeLine(
                            "ObjectTemplate.create GenericProjectile ats_binding_probe",
                            reply, false, true, false, false, false);

                        struct PropertyExpectation
                        {
                            const char* name;
                            const char* value;
                            double expected;
                        };
                        static const PropertyExpectation kProperties[] = {
                            { "drag",              "2",   2.0 },
                            { "mass",              "25", 25.0 },
                            { "gravityModifier",   "0.1", 0.1 },
                            { "hasMobilePhysics", "1",   1.0 },
                            { "hasCollisionPhysics", "1", 1.0 },
                            { "material",          "52", 52.0 },
                            { "collisionGroups",   "8",   8.0 },
                            { "startDelay",        "0.2", 0.2 },
                            { "acceleration",      "175", 175.0 },
                            { "maxSpeed",          "45", 45.0 },
                            { "timeToLive",        "10", 10.0 },
                            { "damage",            "1030", 1030.0 },
                            { "minDamage",         "1",   1.0 },
                            { "hasOnTimeEffect",   "1",   1.0 },
                            { "damageOwner",       "1",   1.0 },
                        };

                        bool passed = getActiveTemplate(CID_GenericProjectileTemplate) != 0;

                        // Reproduce the stock TOW include order exactly: the .con selects Mesh by
                        // numeric alias first, then the .tweak overrides it with symbolic Point.
                        // The old int binding silently retained 3 when parsing "Point".
                        io::mainConsole->executeLine("ObjectTemplate.physicsType 3", reply,
                                                     false, true, false, false, false);
                        reply.clear();
                        io::mainConsole->executeLine("ObjectTemplate.physicsType", reply,
                                                     false, true, false, false, false);
                        const bool numericPhysicsType = reply == "Mesh";
                        passed = passed && numericPhysicsType;
                        printf("[probe] ats. physicsType 3 -> %-17s %s\n", reply.c_str(),
                               numericPhysicsType ? "PASS" : "FAIL");

                        io::mainConsole->executeLine("ObjectTemplate.physicsType Point", reply,
                                                     false, true, false, false, false);
                        reply.clear();
                        io::mainConsole->executeLine("ObjectTemplate.physicsType", reply,
                                                     false, true, false, false, false);
                        const bool symbolicPhysicsType = reply == "Point";
                        passed = passed && symbolicPhysicsType;
                        printf("[probe] ats. physicsType Point -> %-13s %s\n", reply.c_str(),
                               symbolicPhysicsType ? "PASS" : "FAIL");

                        for (unsigned i = 0;
                             i < sizeof(kProperties) / sizeof(kProperties[0]); ++i)
                        {
                            std::string setLine = "ObjectTemplate.";
                            setLine += kProperties[i].name;
                            setLine += ' ';
                            setLine += kProperties[i].value;
                            reply.clear();
                            io::mainConsole->executeLine(setLine, reply,
                                                         false, true, false, false, false);

                            std::string getLine = "ObjectTemplate.";
                            getLine += kProperties[i].name;
                            reply.clear();
                            io::mainConsole->executeLine(getLine, reply,
                                                         false, true, false, false, false);
                            char* end = 0;
                            const double value = std::strtod(reply.c_str(), &end);
                            const double delta = value - kProperties[i].expected;
                            const bool distributionProperty =
                                std::strcmp(kProperties[i].name, "timeToLive") == 0;
                            const bool propertyPassed = distributionProperty
                                ? reply == "CRD_NONE/10/0/0"
                                : end != reply.c_str()
                                    && delta > -0.0001 && delta < 0.0001;
                            passed = passed && propertyPassed;
                            printf("[probe] ats. %-17s reply = \"%s\" %s\n",
                                   kProperties[i].name, reply.c_str(),
                                   propertyPassed ? "PASS" : "FAIL");
                        }

                        GenericProjectileTemplate* projectile =
                            static_cast<GenericProjectileTemplate*>(
                                getActiveTemplate(CID_GenericProjectileTemplate));
                        // Stock TOW owns a SeekClosestTargetComp. Its hasUpdate() result is what
                        // makes GenericProjectile::activate request the root 0x800 update flag.
                        // Keep that component in this probe so the activation path, rather than a
                        // direct handleUpdate call alone, guards the free recursive-flags helper.
                        reply.clear();
                        io::mainConsole->executeLine(
                            "ObjectTemplate.createComponent SeekClosestTargetComp", reply,
                            false, true, false, false, false);
                        const bool seekComponent = projectile
                            && projectile->getComponentFromClassID(
                                   CID_SeekClosestTargetCompTemplate) != 0;
                        passed = passed && seekComponent;
                        printf("[probe] ats. SeekClosest update component %s\n",
                               seekComponent ? "PASS" : "FAIL");

                        const bool directFields = projectile
                            && projectile->m_physicsType == 1
                            && projectile->m_field1D4 > 0.1999f
                            && projectile->m_field1D4 < 0.2001f
                            && projectile->m_field1EC == 175.0f
                            && projectile->m_field1F0 == 45.0f
                            && projectile->m_timeToLive.distribution == CRD_NONE
                            && projectile->m_timeToLive.value == 10.0f
                            && projectile->m_timeToLive.variation == 0.0f
                            && !projectile->m_timeToLive.relative
                            && projectile->m_projectileDamage == 1030.0f
                            && projectile->m_minDamage == 1.0f
                            && projectile->m_hasOnTimeEffect == 1
                            && projectile->m_byte1F8 == 1;
                        passed = passed && directFields;
                        printf("[probe] ats. direct motor fields %s\n",
                               directFields ? "PASS" : "FAIL");

                        IObject* const created = projectile ? projectile->createObject() : 0;
                        GenericProjectile* const liveProjectile =
                            static_cast<GenericProjectile*>(created);
                        // Retail initializes the object/component graph before activation.
                        // Without this, m_seekComp remains null and the probe would never ask
                        // activate() to set 0x800 even though the template owns SeekClosest.
                        if (liveProjectile)
                            liveProjectile->init();
                        IPhysicsNode* const node = created
                            ? static_cast<IPhysicsNode*>(
                                  object_detail::LayoutAccess::physicsNode(created))
                            : 0;
                        const bool pointNode = node && node->getIsPoint()
                            && node->getDrag() > 1.9999f && node->getDrag() < 2.0001f
                            && node->getMass() > 24.9999f && node->getMass() < 25.0001f
                            && node->getGravityModifier() > 0.0999f
                            && node->getGravityModifier() < 0.1001f;
                        passed = passed && pointNode;
                        printf("[probe] ats. runtime Point node %s\n",
                               pointNode ? "PASS" : "FAIL");

                        bool rootUpdateFlag = false;
                        bool firstMotorTick = false;
                        if (liveProjectile && node)
                        {
                            liveProjectile->setAbsoluteTransformation(Mat4::getIdentity());
                            liveProjectile->activate();
                            rootUpdateFlag = (liveProjectile->getFlags() & 0x800u) != 0;
                            const Vec3 launchVelocity = { 0.0f, 0.0f, 20.0f };
                            node->setPositionalSpeed(launchVelocity);
                            node->setIsAwake();
                            const float dt = 0.033333335f;
                            liveProjectile->handleUpdate(dt, 0);
                            node->updatePhysics(dt);

                            const Vec3& velocity = node->getPositionalSpeed();
                            const Vec3& position = liveProjectile->getAbsolutePosition();
                            firstMotorTick = velocity.y > -0.0002f && velocity.y < 0.0002f
                                && velocity.z > 25.8314f && velocity.z < 25.8319f
                                && position.y > -0.0002f && position.y < 0.0002f
                                && position.z > 0.7636f && position.z < 0.7641f;
                            printf("[probe] ats. first Point motor tick v=(%.6f,%.6f,%.6f) "
                                   "p=(%.6f,%.6f,%.6f) %s\n",
                                   velocity.x, velocity.y, velocity.z,
                                   position.x, position.y, position.z,
                                   firstMotorTick ? "PASS" : "FAIL");
                        }
                        passed = passed && rootUpdateFlag && firstMotorTick;
                        printf("[probe] ats. activated root update flag %s\n",
                               rootUpdateFlag ? "PASS" : "FAIL");

                        printf("[probe] ATS BINDING PROBE: %s\n", passed ? "PASS" : "FAIL");
                        fflush(stdout);
                        return passed ? 0 : 43;
                    }

                    // Scaffolding: BF2_STAMINA_BINDING_PROBE creates a local SoldierTemplate through console
                    // bindings, checks stock stamina properties, and exits before the e9/e10 fixtures.
                    if (GetEnvironmentVariableA("BF2_STAMINA_BINDING_PROBE", 0, 0) != 0)
                    {
                        using namespace dice::hfe;
                        using namespace dice::hfe::world;

                        const auto execute = [](const char* line) -> std::string {
                            std::string reply;
                            io::mainConsole->executeLine(
                                line, reply, false, true, false, false, false);
                            return reply;
                        };
                        const auto nearValue = [](double value, double expected) -> bool {
                            const double delta = value - expected;
                            return delta > -0.0001 && delta < 0.0001;
                        };
                        const auto scalarReplyIs = [&nearValue](const std::string& reply,
                                                                double expected) -> bool {
                            char* end = 0;
                            const double value = std::strtod(reply.c_str(), &end);
                            while (end && (*end == ' ' || *end == '\t' ||
                                           *end == '\r' || *end == '\n'))
                                ++end;
                            return end != reply.c_str() && end && *end == '\0'
                                && nearValue(value, expected);
                        };

                        const std::string createReply = execute(
                            "ObjectTemplate.create Soldier stamina_binding_probe");
                        SoldierTemplate* const soldierTemplate =
                            static_cast<SoldierTemplate*>(getActiveTemplate(CID_SoldierTemplate));
                        bool passed = soldierTemplate != 0;
                        printf("[probe] stamina. create reply=\"%s\" active=%d\n",
                               createReply.c_str(), (int)(soldierTemplate != 0));

                        struct ScalarPropertyProbe
                        {
                            const char* setLine;
                            const char* getLine;
                            double expected;
                        };
                        // Exact stock light-soldier spelling and values for the four gameplay
                        // properties, followed by the other two numeric direct-field bindings in
                        // the same retail block.
                        static const ScalarPropertyProbe kProperties[] = {
                            { "ObjectTemplate.SprintRecoverTime 17",
                              "ObjectTemplate.SprintRecoverTime", 17.0 },
                            { "ObjectTemplate.SprintDissipationTime 10",
                              "ObjectTemplate.SprintDissipationTime", 10.0 },
                            { "ObjectTemplate.SprintLimit 0.05",
                              "ObjectTemplate.SprintLimit", 0.05 },
                            { "ObjectTemplate.SprintLossAtJump 0.15",
                              "ObjectTemplate.SprintLossAtJump", 0.15 },
                            { "ObjectTemplate.gasMaskSprintFactor 1.25",
                              "ObjectTemplate.gasMaskSprintFactor", 1.25 },
                            { "ObjectTemplate.ragdollLandedDelay 3.5",
                              "ObjectTemplate.ragdollLandedDelay", 3.5 },
                        };

                        for (unsigned i = 0;
                             i < sizeof(kProperties) / sizeof(kProperties[0]); ++i)
                        {
                            execute(kProperties[i].setLine);
                            const std::string reply = execute(kProperties[i].getLine);
                            const bool propertyPassed =
                                scalarReplyIs(reply, kProperties[i].expected);
                            passed = passed && propertyPassed;
                            printf("[probe] stamina. %-36s reply=\"%s\" %s\n",
                                   kProperties[i].getLine, reply.c_str(),
                                   propertyPassed ? "PASS" : "FAIL");
                        }

                        const bool consumerConstants = soldierTemplate
                            && nearValue(soldierTemplate->getSprintRecoverTime(), 17.0)
                            && nearValue(soldierTemplate->getSprintDissipationTime(), 10.0)
                            && nearValue(soldierTemplate->getSprintLimit(), 0.05);
                        passed = passed && consumerConstants;

                        SprintState sprint;
                        if (soldierTemplate)
                        {
                            sprint.setConstants(
                                soldierTemplate->getSprintDissipationTime(),
                                soldierTemplate->getSprintRecoverTime(),
                                soldierTemplate->getSprintLimit());
                        }
                        sprint.setStamina(1.0f);
                        sprint.handleTMSprint(false);
                        const bool started = sprint.handleUpdate(false, 0.0f, 0.0f) == 42;
                        sprint.handleTMSprint(false);
                        sprint.handleUpdate(false, 0.0f, 1.0f);
                        const double expectedAfterDrain = serverSettings->getNoVehicles()
                            ? 0.95 : 0.90;
                        const double afterDrain = sprint.m_stamina;
                        const bool drainRate = nearValue(afterDrain, expectedAfterDrain);

                        sprint.reset();
                        sprint.setStamina(0.0f);
                        sprint.handleUpdate(false, 0.0f, 1.0f);
                        const double afterRecover = sprint.m_stamina;
                        const bool recoverRate = nearValue(afterRecover, 1.0 / 17.0);

                        sprint.reset();
                        sprint.setStamina(0.049f);
                        sprint.handleTMSprint(false);
                        const bool belowLimit =
                            sprint.handleUpdate(false, 0.0f, 0.0f) == 71
                            && !sprint.m_isSprinting;
                        sprint.setStamina(0.05f);
                        sprint.handleTMSprint(false);
                        const bool atLimit =
                            sprint.handleUpdate(false, 0.0f, 0.0f) == 42
                            && sprint.m_isSprinting;

                        passed = passed && started && drainRate && recoverRate
                            && belowLimit && atLimit;
                        printf("[probe] stamina. direct consumer constants %s\n",
                               consumerConstants ? "PASS" : "FAIL");
                        printf("[probe] stamina. SprintState start=%s drain=%.6f/%s "
                               "recover=%.6f/%s threshold=%s\n",
                               started ? "PASS" : "FAIL", afterDrain,
                               drainRate ? "PASS" : "FAIL", afterRecover,
                               recoverRate ? "PASS" : "FAIL",
                               (belowLimit && atLimit) ? "PASS" : "FAIL");
                        printf("[probe] STAMINA BINDING PROBE: %s\n",
                               passed ? "PASS" : "FAIL");
                        fflush(stdout);
                        return passed ? 0 : 45;
                    }

                    // [TEMP] [CHOSEN, not recovered] BF2_VEHICLE_EXIT_BINDING_PROBE checks stock propulsion
                    // and dismount values through console bindings, then exits before the e9/e10 fixtures.
                    if (GetEnvironmentVariableA("BF2_VEHICLE_EXIT_BINDING_PROBE", 0, 0) != 0)
                    {
                        using namespace dice::hfe::world;

                        const auto execute = [](const char* line) -> std::string {
                            std::string reply;
                            io::mainConsole->executeLine(
                                line, reply, false, true, false, false, false);
                            return reply;
                        };
                        const auto nearValue = [](double value, double expected) -> bool {
                            const double delta = value - expected;
                            return delta > -0.0001 && delta < 0.0001;
                        };
                        const auto scalarReplyIs = [&nearValue](const std::string& reply,
                                                                double expected) -> bool {
                            char* end = 0;
                            const double value = std::strtod(reply.c_str(), &end);
                            while (end && (*end == ' ' || *end == '\t' ||
                                           *end == '\r' || *end == '\n'))
                                ++end;
                            return end != reply.c_str() && end && *end == '\0'
                                && nearValue(value, expected);
                        };
                        const auto vecReplyIs = [&nearValue](const std::string& reply,
                                                             double expectedX,
                                                             double expectedY,
                                                             double expectedZ) -> bool {
                            float x = 0.0f;
                            float y = 0.0f;
                            float z = 0.0f;
                            char trailing = 0;
                            const int fields = std::sscanf(
                                reply.c_str(), "%f/%f/%f%c", &x, &y, &z, &trailing);
                            return fields == 3
                                && nearValue(x, expectedX)
                                && nearValue(y, expectedY)
                                && nearValue(z, expectedZ);
                        };

                        bool passed = true;

                        execute("ObjectTemplate.create Spring vehicle_binding_probe_spring");
                        const bool springActive = getActiveTemplate(CID_SpringTemplate) != 0;
                        execute("ObjectTemplate.grip 8");
                        const std::string grip8Reply = execute("ObjectTemplate.grip");
                        const bool grip8 = scalarReplyIs(grip8Reply, 8.0);
                        execute("ObjectTemplate.grip 128");
                        const std::string grip128Reply = execute("ObjectTemplate.grip");
                        const bool grip128 = scalarReplyIs(grip128Reply, 128.0);
                        passed = passed && springActive && grip8 && grip128;
                        printf("[probe] vehicle. Spring active=%d grip8=\"%s\" %s "
                               "grip128=\"%s\" %s\n",
                               (int)springActive,
                               grip8Reply.c_str(), grip8 ? "PASS" : "FAIL",
                               grip128Reply.c_str(), grip128 ? "PASS" : "FAIL");

                        execute("ObjectTemplate.create Engine vehicle_binding_probe_engine");
                        const bool engineActive = getActiveTemplate(CID_EngineTemplate) != 0;
                        struct ScalarPropertyProbe
                        {
                            const char* setLine;
                            const char* getLine;
                            double expected;
                        };
                        static const ScalarPropertyProbe kEngineProperties[] = {
                            { "ObjectTemplate.setGearUp 0.95", "ObjectTemplate.gearUp", 0.95 },
                            { "ObjectTemplate.setGearDown 0.45", "ObjectTemplate.gearDown", 0.45 },
                            { "ObjectTemplate.setGearChangeTime 0.1",
                              "ObjectTemplate.gearChangeTime", 0.1 },
                            { "ObjectTemplate.trackTurnAcceleration 30",
                              "ObjectTemplate.trackTurnAcceleration", 30.0 },
                            { "ObjectTemplate.trackTurnSpeed 0.85",
                              "ObjectTemplate.trackTurnSpeed", 0.85 },
                        };
                        passed = passed && engineActive;
                        for (unsigned int i = 0;
                             i < sizeof(kEngineProperties) / sizeof(kEngineProperties[0]); ++i)
                        {
                            execute(kEngineProperties[i].setLine);
                            const std::string reply = execute(kEngineProperties[i].getLine);
                            const bool propertyPassed =
                                scalarReplyIs(reply, kEngineProperties[i].expected);
                            passed = passed && propertyPassed;
                            printf("[probe] vehicle. %-31s reply=\"%s\" %s\n",
                                   kEngineProperties[i].getLine, reply.c_str(),
                                   propertyPassed ? "PASS" : "FAIL");
                        }
                        execute("ObjectTemplate.setGearChangeTime 0.01");
                        const std::string clampLowReply =
                            execute("ObjectTemplate.gearChangeTime");
                        const bool clampLow = scalarReplyIs(clampLowReply, 0.1);
                        execute("ObjectTemplate.setGearChangeTime 101");
                        const std::string clampHighReply =
                            execute("ObjectTemplate.gearChangeTime");
                        const bool clampHigh = scalarReplyIs(clampHighReply, 100.0);
                        passed = passed && clampLow && clampHigh;
                        printf("[probe] vehicle. gearChangeTime clamps low=\"%s\" %s "
                               "high=\"%s\" %s\n",
                               clampLowReply.c_str(), clampLow ? "PASS" : "FAIL",
                               clampHighReply.c_str(), clampHigh ? "PASS" : "FAIL");

                        execute("ObjectTemplate.create PlayerControlObject "
                                "vehicle_binding_probe_pco");
                        PlayerControlObjectTemplate* const pco =
                            static_cast<PlayerControlObjectTemplate*>(
                                getActiveTemplate(CID_PlayerControlObjectTemplate));
                        execute("ObjectTemplate.setSoldierExitLocation "
                                "-3.5/0.1/0 0/0/0");
                        const std::string primaryExitReply =
                            execute("ObjectTemplate.soldierExitPosition");
                        const std::string exitRotationReply =
                            execute("ObjectTemplate.soldierExitRotation");
                        bool primaryExit = false;
                        bool exitRotation = false;
                        if (pco)
                        {
                            const Vec3 position = pco->getSoldierExitPosition();
                            const Vec3 rotation = pco->getSoldierExitRotation();
                            primaryExit = nearValue(position.x, -3.5)
                                && nearValue(position.y, 0.1)
                                && nearValue(position.z, 0.0);
                            exitRotation = nearValue(rotation.x, 0.0)
                                && nearValue(rotation.y, 0.0)
                                && nearValue(rotation.z, 0.0);
                        }
                        primaryExit = primaryExit
                            && vecReplyIs(primaryExitReply, -3.5, 0.1, 0.0);
                        exitRotation = exitRotation
                            && vecReplyIs(exitRotationReply, 0.0, 0.0, 0.0);
                        execute("ObjectTemplate.altSoldierExitPosition 0/0.1/-4.5");
                        const std::string altExitReply =
                            execute("ObjectTemplate.altSoldierExitPosition");
                        const bool altExit = vecReplyIs(altExitReply, 0.0, 0.1, -4.5);
                        execute("ObjectTemplate.hasRestrictedExit 1");
                        const std::string restrictedReply =
                            execute("ObjectTemplate.hasRestrictedExit");
                        const bool restricted = scalarReplyIs(restrictedReply, 1.0);
                        execute("ObjectTemplate.exitSpeedMod 0.5");
                        const std::string speedModReply = execute("ObjectTemplate.exitSpeedMod");
                        const bool speedMod = scalarReplyIs(speedModReply, 0.5);
                        passed = passed && pco && primaryExit && exitRotation
                            && altExit && restricted && speedMod;
                        printf("[probe] exit. PCO=%p primary=\"%s\" %s rotation=\"%s\" %s "
                               "alt=\"%s\" %s restricted=\"%s\" %s speedMod=\"%s\" %s\n",
                               (void*)pco,
                               primaryExitReply.c_str(),
                               primaryExit ? "PASS" : "FAIL",
                               exitRotationReply.c_str(),
                               exitRotation ? "PASS" : "FAIL",
                               altExitReply.c_str(), altExit ? "PASS" : "FAIL",
                               restrictedReply.c_str(), restricted ? "PASS" : "FAIL",
                               speedModReply.c_str(), speedMod ? "PASS" : "FAIL");
                        printf("[probe] VEHICLE/EXIT BINDING PROBE: %s\n",
                               passed ? "PASS" : "FAIL");
                        fflush(stdout);
                        return passed ? 0 : 44;
                    }

                    // Replay the gas-station template properties and read back the resulting template.
                    {
                        // This fixture omits the collisionMesh property, so it does not exercise collision loading.
                        static const char* const kBlock[] = {
                            "ObjectTemplate.create Bundle ch_gas_station",
                            "ObjectTemplate.saveInSeparateFile 1",
                            "ObjectTemplate.creator GOD-1:god",
                            "ObjectTemplate.mapMaterial 0 plastic 0",
                            "ObjectTemplate.mapMaterial 4 concrete 0",
                            "ObjectTemplate.hasCollisionPhysics 1",
                            "ObjectTemplate.physicsType 3",
                            "ObjectTemplate.physicsType 9",
                            "ObjectTemplate.geometry ch_gas_station",
                            "ObjectTemplate.anchor -5.1060/-3.2435/0",
                        };
                        std::string r9;
                        for (unsigned i = 0; i < sizeof(kBlock) / sizeof(kBlock[0]); ++i)
                        {
                            r9.clear();
                            io::mainConsole->executeLine(kBlock[i], r9,
                                                         false, true, false, false, false);
                            if (!r9.empty())
                                printf("[probe] e9.   \"%s\" -> \"%s\"\n", kBlock[i], r9.c_str());
                        }
                        using namespace dice::hfe::world;
                        SimpleObjectTemplate* t = static_cast<SimpleObjectTemplate*>(
                            getActiveTemplate(CID_SimpleObjectTemplate));
                        if (t)
                        {
                            printf("[probe] e9. built Bundle template: geometry=\"%s\" "
                                   "collisionMesh=\"%s\" (empty - line skipped, see note)\n",
                                   t->getGeometry().c_str(), t->getCollisionMesh().c_str());
                            printf("[probe] e9. physicsType=%d (expect 3 - the 9 was guarded "
                                   "off), hasCollisionPhysics=%d, saveInSeparateFile flag=%d\n",
                                   t->m_physicsType, (int)((t->m_physicsFlags & 4) != 0),
                                   (int)((t->getFlags() & 1) != 0));
                            r9.clear();
                            io::mainConsole->executeLine("ObjectTemplate.geometry", r9,
                                                         false, true, false, false, false);
                            printf("[probe] e9. console GET geometry reply = \"%s\"\n",
                                   r9.c_str());
                            r9.clear();
                            io::mainConsole->executeLine("ObjectTemplate.anchor", r9,
                                                         false, true, false, false, false);
                            printf("[probe] e9. console GET anchor reply = \"%s\" (expect "
                                   "-5.106/-3.2435/0 - the Vec3 round-trip)\n", r9.c_str());
                            printf("[probe] e9. anchor fields = %g/%g/%g, materials mapped = %u\n",
                                   t->getAnchor().x, t->getAnchor().y, t->getAnchor().z,
                                   (unsigned)t->getMaterialVector()->size());
                        }
                        else
                        {
                            printf("[probe] e9. NO ACTIVE SimpleObject-family template - "
                                   "the block did not build\n");
                        }
                        fflush(stdout);
                    }

                    // Drive GameServer::loadPath using local server and logic fixtures against installed content.
                    {
                        using namespace dice::hfe;
                        if (!world::playerManager)
                            world::playerManager = new world::PlayerManager();
                        Py_Initialize();
                        // Use the fixture gamemode with empty init/deinit; this bypasses the stock Python host
                        // module dependencies. A failing gamemode import terminates the process.
                        PyRun_SimpleString(
                            "import sys\n"
                            "sys.path.insert(0, 'D:/LC/BF2Rebuilt/Code/Build/pyfixture')\n");
                        if (!mapList)
                            mapList = MapList::instance();
                        // Publish GameServer before constructing ServerGameLogic: reset broadcasts through game
                        // and accesses the network and score managers.
                        static ProbeGameServer probeServer;
                        game      = &probeServer;
                        static ProbeGameLogic  probeLogic;
                        gameLogic = &probeLogic;

                        // BF2_LEVEL selects the drive target; default is dalian_plant.
                        char levelBuf[128] = "dalian_plant";
                        GetEnvironmentVariableA("BF2_LEVEL", levelBuf, sizeof(levelBuf));
                        const std::string driveLevel(levelBuf);
                        // Pre-append diagnostic: which checkIfValidEntry gate answers.
                        {
                            IModInfo* mi = (IModInfo*)classManager->getSingleton("ModInfo");
                            printf("[probe] e10. ModInfo singleton = %p\n", (void*)mi);
                            {
                                std::list<io::FileInfo> probeDirs;
                                io::fileManager->findFiles(std::string("@ROOT@/mods/"),
                                                           false, probeDirs);
                                printf("[probe] e10. findFiles(@ROOT@/mods/) entries = %u\n",
                                       (unsigned)probeDirs.size());
                                for (std::list<io::FileInfo>::iterator pit = probeDirs.begin();
                                     pit != probeDirs.end(); ++pit)
                                    printf("[probe] e10.   entry: %s dir=%d\n",
                                           pit->name.c_str(), (int)pit->isDirectory);
                            }
                            if (mi)
                            {
                                printf("[probe] e10. modInfo->init() = %d (false when ALREADY "
                                       "initialised or when the scan yields no mods)\n",
                                       (int)mi->init());
                                std::vector<IModInfo::Mod>* mods = mi->getModList();
                                printf("[probe] e10. getModList = %p, count = %u\n",
                                       (void*)mods, mods ? (unsigned)mods->size() : 0u);
                                if (mods)
                                    for (unsigned mi2 = 0; mi2 < mods->size() && mi2 < 4; ++mi2)
                                        printf("[probe] e10.   mod[%u] = %s\n", mi2,
                                               (*mods)[mi2].path.c_str());
                                const IModInfo::Mod* cur = mi->getCurrentMod();
                                printf("[probe] e10. getCurrentMod = %p (%s)\n", (const void*)cur,
                                       cur ? cur->path.c_str() : "-");
                                const IModInfo::Map* mp = mi->findMapFromCurrentMod(driveLevel);
                                printf("[probe] e10. findMapFromCurrentMod(%s) = %p\n", driveLevel.c_str(),
                                       (const void*)mp);
                                if (mp)
                                {
                                    printf("[probe] e10. map gameModes: %u [",
                                           (unsigned)mp->gameModes.size());
                                    for (unsigned gi = 0; gi < mp->gameModes.size(); ++gi)
                                        printf(" %s", mp->gameModes[gi].name.c_str());
                                    printf(" ]\n");
                                }
                            }
                        }

                        // Try normal map-list validation first with the fixture's modifiable override.
                        const bool appended =
                            mapList->appendEntry(driveLevel, "gpm_cq", 16, true);
                        mapList->setCurrentMap(0, true);
                        printf("[probe] e10. appendEntry(%s, gpm_cq, 16) = %d, "
                               "count = %d, current = %d\n", driveLevel.c_str(), (int)appended,
                               mapList->getMapCount(), mapList->getCurrentMap());

                        settings->stringSet("GSLevelDirectory", std::string("levels/"));

                        // Pre-seat a no-op squad-manager fixture so loadPath takes its existing-manager branch.
                        struct ProbeSquadManager
                        {
                            virtual void s00() {} virtual void s04() {} virtual void s08() {}
                            virtual void s0C() {} virtual void s10() {}
                            virtual void reAddDynamicSpawnGroups() {}   // +0x14
                        };
                        static ProbeSquadManager psm;
                        squadManager = (IBase*)&psm;    // scaffolding cast; only +0x14 dispatches

                        // [CHOSEN] Seed the rotation directly only when content exists, bypassing normal validation
                        // for this fixture. loadPath requires a nonempty rotation.
                        bool lp = false;
                        const bool haveContent = io::fileManager->fileExists(
                            std::string("mods/bf2/Levels/") + driveLevel + "/server.zip");
                        printf("[probe] e10. content present (%s server.zip) = %d\n", driveLevel.c_str(),
                               (int)haveContent);
                        if (!appended && haveContent)
                        {
                            MapList::Entry seed;
                            seed.name = driveLevel;
                            seed.gameMode = "gpm_cq";
                            seed.mapSize = 16;
                            mapList->m_entries.push_back(seed);
                            mapList->m_currentMap = 0;
                            printf("[probe] e10. rotation SEEDED directly (validation stubbed - "
                                   "the parseModDesc frontier); count = %d, current = %d\n",
                                   mapList->getMapCount(), mapList->getCurrentMap());
                        }
                        // BF2_DRIVE_LOADPATH opts into the load drive; without it, report the preceding probes only.
                        const bool driveOptIn = GetEnvironmentVariableA("BF2_DRIVE_LOADPATH",
                                                                        0, 0) != 0;
                        if (mapList->getMapCount() > 0 && driveOptIn)
                            lp = game->loadPath(driveLevel);
                        else if (mapList->getMapCount() > 0)
                            printf("[probe] e10. rotation ready; loadPath drive is opt-in "
                                   "(BF2_DRIVE_LOADPATH=1) - current reach: the Objects sweep's "
                                   "first unwritten template-class factory\n");
                        else
                            printf("[probe] e10. no content - loadPath not driven\n");

                        printf("[probe] e10. game->loadPath(%s) = %d\n", driveLevel.c_str(), (int)lp);
                        // fingerprint: 1 = LoadLevelFingerprint found and parsed the level's
                        // archive.md5, 0 = no file.
                        printf("[probe] e10. loadState = %d (expect 2), fingerprint = %d "
                               "(1 = archive.md5 found), abort = %d\n",
                               probeServer.m_loadState, (int)probeServer.m_levelFingerprint,
                               (int)probeServer.getAbortLoadLevel());
                        printf("[probe] e10. fingerprint entries = %u\n",
                               (unsigned)probeServer.m_levelFingerprintStrings.size());
                        const MapInfo probeMapInfo = probeServer.getMapInfo();
                        printf("[probe] e10. MapInfo: name = \"%s\", mode = \"%s\", "
                               "levelPath = \"%s\", maxPlayers = %d, ordinal = %d\n",
                               probeMapInfo.getMapName().c_str(),
                               probeMapInfo.getGameMode().c_str(),
                               probeMapInfo.getLevelPath().c_str(),
                               probeMapInfo.getMaxNumberOfPlayers(),
                               probeMapInfo.getChallengeOrdinal());
                        // Check for the free-camera template loaded by GameLogicInit.con during CEPreLoad.
                        printf("[probe] e10. MultiPlayerFreeCamera template = %p "
                               "(non-null = GameLogicInit.con ran)\n",
                               (void*)(world::objectTemplateManager
                                   ? world::objectTemplateManager->getTemplate(
                                         std::string("MultiPlayerFreeCamera")) : 0));
                        // Report soldier templates and skeletons loaded by the Objects sweep.
                        {
                            static const char* const kSoldiers[] = {
                                "us_light_soldier", "us_heavy_soldier",
                                "ch_light_soldier", "eu_soldier" };
                            for (int si = 0; si < 4; ++si)
                            {
                                dice::hfe::world::IObjectTemplate* st =
                                    world::objectTemplateManager
                                        ? world::objectTemplateManager->getTemplate(
                                              std::string(kSoldiers[si])) : 0;
                                if (!st)
                                {
                                    printf("[probe] e10b. soldier template \"%s\" = NULL "
                                           "(the Objects/ sweep never created it)\n", kSoldiers[si]);
                                    continue;
                                }
                                dice::hfe::world::SoldierTemplate* sot =
                                    static_cast<dice::hfe::world::SoldierTemplate*>(
                                        st->queryImplementation(dice::hfe::world::CID_SoldierTemplate));
                                void* as1 = sot ? sot->getAnimationSystem1P() : 0;
                                void* as3 = sot ? sot->getAnimationSystem3P() : 0;
                                printf("[probe] e10b. soldier template \"%s\" = %p  soldierImpl = %p  "
                                       "skeleton1P = %p  skeleton3P = %p  animSys1P = %p  animSys3P = %p\n",
                                       kSoldiers[si], (void*)st, (void*)sot,
                                       sot ? (void*)sot->getSkeleton1P() : 0,
                                       sot ? (void*)sot->getSkeleton3P() : 0, as1, as3);

                                // Check animation-template registration; naming a template does not itself read its file.
                                if (dice::anim::animationSystemTemplateManager)
                                {
                                    // Inspect camera-spring values loaded from the template include.
                                    {
                                        dice::anim::AnimationSystemTemplate* ast1 =
                                            static_cast<dice::anim::AnimationSystemTemplate*>(as1);
                                        dice::anim::CameraSpring* spr = ast1 ? ast1->getCameraSpring() : 0;
                                        if (spr)
                                            printf("[probe] e10d. cameraSpring = %p  amplitude=%.3f/%.3f/%.3f  "
                                                   "stiffness=%.1f/%.1f/%.1f  move=%.1f/%.1f/%.1f  "
                                                   "zoomDamping=%.1f/%.1f/%.1f  look=%.1f/%.1f  fire=%.1f/%.1f/%.1f\n",
                                                   (void*)spr, spr->amplitude.x, spr->amplitude.y, spr->amplitude.z,
                                                   spr->stiffness.x, spr->stiffness.y, spr->stiffness.z,
                                                   spr->move.x, spr->move.y, spr->move.z,
                                                   spr->zoomDamping.x, spr->zoomDamping.y, spr->zoomDamping.z,
                                                   spr->look.x, spr->look.y,
                                                   spr->fire.x, spr->fire.y, spr->fire.z);
                                        else
                                            printf("[probe] e10d. cameraSpring = NULL\n");
                                    }
                                    printf("[probe] e10c. animSysMgr = %p  1P path = \"%s\"  "
                                           "3P path = \"%s\"  active = %p  playback = %.3f\n",
                                           (void*)dice::anim::animationSystemTemplateManager,
                                           dice::anim::animationSystemTemplateManager->getPath(
                                               static_cast<dice::anim::AnimationSystemTemplate*>(as1)).c_str(),
                                           dice::anim::animationSystemTemplateManager->getPath(
                                               static_cast<dice::anim::AnimationSystemTemplate*>(as3)).c_str(),
                                           (void*)dice::anim::animationSystemTemplateManager->getActive(),
                                           dice::anim::animationSystemTemplateManager->getPlaybackModifier());
                                }
                                else
                                {
                                    printf("[probe] e10c. animSysMgr = NULL - AnimModule::init never ran\n");
                                }
                            }
                        }

                        printf("[probe] e10. GLGameLevelPath = \"%s\" (load()'s own write), "
                               "GSMaxNrOfPlayers = %d, GSGameMode = \"%s\"\n",
                               settings->stringGet("GLGameLevelPath", std::string("")).c_str(),
                               settings->intGet("GSMaxNrOfPlayers", 0),
                               settings->stringGet("GSGameMode", std::string("")).c_str());

                        // Report replication and spawn registries after loading.
                        if (io::networkManager)
                        {
                            printf("[probe] e10. NetworkManager: objects = %u, descriptors = %u, "
                                   "static ghosts = %u, scopable roots = %u, highest used id = %u\n",
                                   (unsigned)io::networkManager->getObjectMap().size(),
                                   (unsigned)io::networkManager->getDescriptorMap().size(),
                                   (unsigned)io::networkManager->getStaticDescriptors().size(),
                                   (unsigned)io::networkManager->getScopableRootDescriptors().size(),
                                   (unsigned)io::networkManager->getHighestUsedNetID());
                            // Every descriptor: id, control-object id, property count, min bits, the
                            // first networkable's base priority (< 0 = static ghost).
                            std::map<unsigned short, io::NetworkableDescriptor*>& descs =
                                io::networkManager->getDescriptorMap();
                            std::string line;
                            for (std::map<unsigned short, io::NetworkableDescriptor*>::iterator it = descs.begin();
                                 it != descs.end(); ++it)
                            {
                                io::NetworkableDescriptor* d = it->second;
                                float prio = 0.0f;
                                if (!d->properties.empty() && !d->properties[0].networkables.empty())
                                    prio = d->properties[0].networkables[0]->m_basePriority;
                                char buf[96];
                                sprintf(buf, "%s%u:ctl%d/p%u/bits%d/prio%g", line.empty() ? "" : " ",
                                        (unsigned)it->first, d->controlObjId, (unsigned)d->properties.size(),
                                        d->minimumBitSize, prio);
                                line += buf;
                            }
                            printf("[probe] e10. descriptors: [%s]\n", line.c_str());
                            // Report control-point template properties and each live object's team.
                            std::map<unsigned short, world::IObject*>& objs = io::networkManager->getObjectMap();
                            std::string cps;
                            for (std::map<unsigned short, world::IObject*>::iterator it = objs.begin();
                                 it != objs.end(); ++it)
                            {
                                const world::IObjectTemplate* t = it->second->getTemplate();
                                if (!t || t->getClassID() != world::CID_ControlPointTemplate) continue;
                                world::ControlPoint* cp = static_cast<world::ControlPoint*>(it->second);
                                const world::ControlPointTemplate* ct = static_cast<const world::ControlPointTemplate*>(t);
                                char buf[128];
                                sprintf(buf, "%s%u:%s cpid%d team%d/start%d r%g get%g lose%g area%u/%u",
                                        cps.empty() ? "" : " ", (unsigned)it->first, t->getName().c_str(),
                                        cp->getControlerId(), cp->getTeam(),
                                        const_cast<world::ControlPointTemplate*>(ct)->getStartTeam(),
                                        const_cast<world::ControlPointTemplate*>(ct)->getRadius(),
                                        const_cast<world::ControlPointTemplate*>(ct)->getTimeToGetControl(),
                                        const_cast<world::ControlPointTemplate*>(ct)->getTimeToLoseControl(),
                                        const_cast<world::ControlPointTemplate*>(ct)->getAreaValueTeam1(),
                                        const_cast<world::ControlPointTemplate*>(ct)->getAreaValueTeam2());
                                cps += buf;
                            }
                            printf("[probe] e10. control points: [%s]\n", cps.c_str());
                        }
                        if (spawnManager)
                        {
                            const std::map<int, ISpawnGroup*>& groups =
                                static_cast<SpawnManager*>(spawnManager)->getSpawnGroups();
                            unsigned points = 0;
                            std::string ids;
                            for (std::map<int, ISpawnGroup*>::const_iterator it = groups.begin();
                                 it != groups.end(); ++it)
                            {
                                points += (unsigned)it->second->getSpawnPoints().size();
                                char buf[48];
                                sprintf(buf, "%s%d:cp%u/t%d/%up", ids.empty() ? "" : " ", it->first,
                                        it->second->getControlPointId(), it->second->getTeam(),
                                        (unsigned)it->second->getSpawnPoints().size());
                                ids += buf;
                            }
                            // [FRONTIER] Avoid querying SpawnPoint::getActive here; this probe only counts entries.
                            printf("[probe] e10. SpawnManager: groups = %u, spawn points in groups = "
                                   "%u  [%s]\n", (unsigned)groups.size(), points, ids.c_str());
                        }
                        // The object registry the networkable sweep walks: how many registered
                        // objects, how many the grid filter passes, how many carry a networkable.
                        if (world::objectManager)
                        {
                            const std::map<unsigned int, world::IObject*>& reg =
                                world::objectManager->getAllRegisteredObjects();
                            unsigned withNet = 0;
                            for (std::map<unsigned int, world::IObject*>::const_iterator it = reg.begin();
                                 it != reg.end(); ++it)
                                if (it->second->getNetworkable()) ++withNet;
                            printf("[probe] e10. ObjectManager: registered = %u, in grid = %u, with a "
                                   "networkable = %u\n", (unsigned)reg.size(),
                                   (unsigned)world::objectManager->getAllObjectsFromGrid().size(), withNet);
                        }
                        // Summarize mobile physics nodes by point/rigid type, platform flag, and sleep state.
                        if (world::physicsNodeManager)
                        {
                            std::list<world::IPhysicsNode*>& nodes =
                                world::physicsNodeManager->getPhysicsNodeList();
                            unsigned point = 0, rigid = 0, platform = 0, asleep = 0, withObj = 0;
                            for (std::list<world::IPhysicsNode*>::iterator it = nodes.begin();
                                 it != nodes.end(); ++it)
                            {
                                if ((*it)->getIsPoint()) ++point; else ++rigid;
                                if ((*it)->getIsPlatformPhysics()) ++platform;
                                if ((*it)->getIsSleeping()) ++asleep;
                                if ((*it)->getObject()) ++withObj;
                            }
                            printf("[probe] e10. PhysicsNodeManager: mobile nodes = %u (point %u, rigid %u, "
                                   "platform %u, asleep %u, with object %u)\n",
                                   (unsigned)nodes.size(), point, rigid, platform, asleep, withObj);
                        }
                        fflush(stdout);
                    }
                }
            }
        }
    }

    // BF2_AI_PLAYER=N creates one synthetic player after frame N; unset disables the probe.
    // BF2_AI_TEAM and BF2_AI_KIT override the defaults. This exercises in-process creation,
    // not the network join protocol.
    int aiAtFrame = 0, aiTeam = 1, aiKit = 0;
    {
        char buf[16] = {0};
        if (GetEnvironmentVariableA("BF2_AI_PLAYER", buf, sizeof(buf)) > 0) aiAtFrame = atoi(buf);
        buf[0] = 0;
        if (GetEnvironmentVariableA("BF2_AI_TEAM", buf, sizeof(buf)) > 0)   aiTeam = atoi(buf);
        buf[0] = 0;
        if (GetEnvironmentVariableA("BF2_AI_KIT", buf, sizeof(buf)) > 0)    aiKit = atoi(buf);
    }

    // BF2_FRAMES sets the frame limit; zero runs until the engine stops. Unset uses kFrames
    // for diagnostic runs and an unbounded loop in server-screen mode.
    int frames = g_serverScreen ? 0 : kFrames;
    {
        char buf[16] = {0};
        if (GetEnvironmentVariableA("BF2_FRAMES", buf, sizeof(buf)) > 0)
            frames = atoi(buf);
    }

    // BF2_QUIET suppresses per-frame lines while retaining diagnostics and summaries.
    // Defaults to true in server-screen mode; an explicit value overrides the default.
    bool quiet = g_serverScreen;
    {
        char buf[16] = {0};
        if (GetEnvironmentVariableA("BF2_QUIET", buf, sizeof(buf)) > 0)
            quiet = (atoi(buf) != 0);
    }
    if (quiet)
    {
        printf("[boot] BF2_QUIET - per-frame lines suppressed; the status screen owns the console\n");
        fflush(stdout);
    }
    if (frames <= 0)
    {
        printf("[boot] BF2_FRAMES=0 - running unbounded (`while (bf2->run()) ;`)\n"); fflush(stdout);
        serverScreenClear();
        if (quiet && !g_serverScreen)
        {
            // Place the cursor at the window origin before the first status-screen paint.
            HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO info;
            if (out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(out, &info))
            {
                SMALL_RECT top = info.srWindow;
                const SHORT h = (SHORT)(top.Bottom - top.Top);
                top.Top = 0; top.Bottom = h;
                SetConsoleWindowInfo(out, TRUE, &top);
            }
        }
        while (bf2->run())
            ;
        printf("[boot] run() returned false (engine asked to stop)\n");
        fflush(stdout);
        serverScreenTakeBack();         // see the note at the other return - the screen is never deleted
        return 0;
    }
    // [TEMP] BF2_PROBE_LOGIC logs logic identity, refcount, server flag, and game status before
    // and during the frame loop.
    const bool probeLogic = GetEnvironmentVariableA("BF2_PROBE_LOGIC", 0, 0) != 0;
    if (probeLogic && gameLogic)
    {
        printf("[probe] logic pre-loop: %p ref=%d isServer=%d status=%d game=%p\n", (void*)gameLogic,
               gameLogic->getRef(), (int)gameLogic->isServer(), gameLogic->getGameStatus(), (void*)game);
        fflush(stdout);
    }
    // [TEMP] BF2_VEHICLE_PROBE=N reports root vehicle positions every N frames for motion checks.
    int vehicleProbeEvery = 0;
    {
        char vbuf[32];
        if (GetEnvironmentVariableA("BF2_VEHICLE_PROBE", vbuf, sizeof(vbuf)) > 0)
            vehicleProbeEvery = atoi(vbuf);
    }
    // [TEMP] BF2_RELOAD_AT=N requests a round transition at frame N, then keeps ticking
    // the engine through unload/reload. Status is reported every 150 frames.
    int reloadAt = 0;
    {
        char rbuf[32];
        if (GetEnvironmentVariableA("BF2_RELOAD_AT", rbuf, sizeof(rbuf)) > 0)
            reloadAt = atoi(rbuf);
    }
    // [TEMP] BF2_RADIO_PROBE reads loaded radio and ability component properties through typed accessors.
    if (GetEnvironmentVariableA("BF2_RADIO_PROBE", 0, 0) != 0 && world::objectTemplateManager)
    {
        using dice::hfe::world::IObjectTemplate;
        using dice::hfe::world::IObjectTemplateComponent;
        IObjectTemplate* rt = world::objectTemplateManager->getTemplate(std::string("mobileradar_ch_dest"));
        if (rt)
        {
            IObjectTemplateComponent* rc = rt->queryComponent(world::IID_IRadioGLCompTemplate);
            if (rc)
            {
                world::RadioGLCompTemplate* radio = static_cast<world::RadioGLCompTemplate*>(rc);
                printf("[radio probe] mobileradar_ch_dest comp=%p destroyed='%s' repaired='%s' fail='%s'\n",
                       (void*)rc, radio->getDestroyedMessage().c_str(),
                       radio->getRepairedMessage().c_str(), radio->getFailMessage().c_str());
            }
            else
                printf("[radio probe] mobileradar_ch_dest HAS NO radio component\n");
        }
        else
            printf("[radio probe] no template mobileradar_ch_dest\n");
        static const char* const abilityNames[] = { "CH_Medic", "US_Medic", "CH_Support", "US_Support", "CH_Engineer" };   // the KIT templates carry createComponent Ability (Kits/*/*.con)
        bool foundAbility = false;
        for (int k = 0; k < 5 && !foundAbility; ++k)
        {
            IObjectTemplate* at = world::objectTemplateManager->getTemplate(std::string(abilityNames[k]));
            if (!at)
                continue;
            IObjectTemplateComponent* ac = at->queryComponent(world::IID_IAbilityGLCompTemplate);
            if (!ac)
                continue;
            world::AbilityGLCompTemplate* ability = static_cast<world::AbilityGLCompTemplate*>(ac);
            printf("[ability probe] %s comp=%p heal=%d repair=%d ammo=%d radar=%d\n",
                   abilityNames[k], (void*)ac, (int)ability->getHasHealingAbility(),
                   (int)ability->getHasRepairingAbility(), (int)ability->getHasAmmoAbility(),
                   ability->getRadarRadius());
            foundAbility = true;
        }
        if (!foundAbility)
            printf("[ability probe] no probed kit-item template carries an Ability component\n");
        fflush(stdout);
    }

    // [TEMP] BF2_WIND_PROBE checks loaded aircraft WindAffector values and fails on mismatches.
    if (GetEnvironmentVariableA("BF2_WIND_PROBE", 0, 0) != 0)
    {
        struct WindProbeRow
        {
            const char* name;
            float speed;
            float falloff;
            float radialFalloff;
        };
        static const WindProbeRow rows[] = {
            { "ahe_z10_Rotor",              20.0f, 30.0f, 20.0f },
            { "AIR_F35B_HelicopterEngine",  10.0f, 30.0f, 10.0f },
            { "chthe_z8_RotorHead1",        20.0f, 30.0f, 20.0f }
        };

        bool allWindRowsMatch = world::objectTemplateManager != 0;
        for (unsigned int i = 0; i < sizeof(rows) / sizeof(rows[0]); ++i)
        {
            world::IObjectTemplate* const objectTemplate = world::objectTemplateManager
                ? world::objectTemplateManager->getTemplate(std::string(rows[i].name)) : 0;
            world::IObjectTemplateComponent* const component = objectTemplate
                ? objectTemplate->queryComponent(world::IID_IWindAffectorTemplate) : 0;
            world::WindAffectorTemplate* const wind =
                static_cast<world::WindAffectorTemplate*>(component);
            if (!wind)
            {
                printf("[wind probe] %-29s template=%p HAS NO WindAffector component\n",
                       rows[i].name, (void*)objectTemplate);
                allWindRowsMatch = false;
                continue;
            }

            const Math::Vec3& direction = wind->getWindDirection();
            const bool matches = wind->getWindType() == "RadialDir"
                              && wind->getWindIsDynamic()
                              && direction.x == 0.0f && direction.y == 0.0f
                              && direction.z == -1.0f
                              && wind->getWindSpeed() == rows[i].speed
                              && wind->getWindBlowTime() == -1.0f
                              && wind->getWindFalloff() == rows[i].falloff
                              && wind->getWindRadialFalloff() == rows[i].radialFalloff;
            printf("[wind probe] %-29s comp=%p type=%s dynamic=%d dir=%g/%g/%g "
                   "speed=%g blow=%g falloff=%g radial=%g  %s\n",
                   rows[i].name, (void*)component, wind->getWindType().c_str(),
                   (int)wind->getWindIsDynamic(), direction.x, direction.y, direction.z,
                   wind->getWindSpeed(), wind->getWindBlowTime(), wind->getWindFalloff(),
                   wind->getWindRadialFalloff(), matches ? "MATCH" : "MISMATCH");
            allWindRowsMatch = allWindRowsMatch && matches;
        }
        fflush(stdout);
        if (!allWindRowsMatch)
            return 64;
    }
    serverScreenClear();        // the bounded loop paints the screen too - give it a clean terminal
    for (int f = 0; f < frames; ++f)
    {
        if (reloadAt > 0 && gameLogic && (f % 150 == 0 || f == reloadAt))
        {
            unsigned objs = dice::hfe::world::objectManager
                ? (unsigned)dice::hfe::world::objectManager->getAllRegisteredObjects().size() : 0u;
            printf("[reload] frame %5d  gameStatus=%d  currentMap=%d  objects=%u  isLevelLoaded=%d\n",
                   f, gameLogic->getGameStatus(),
                   dice::hfe::mapList ? dice::hfe::mapList->getCurrentMap() : -1, objs,
                   (int)gameLogic->isLevelLoaded());
            fflush(stdout);
        }
        if (reloadAt > 0 && f == reloadAt && gameLogic)
        {
            // Pin the current map before requesting the transition so the probe reloads that map.
            const int current = dice::hfe::mapList ? dice::hfe::mapList->getCurrentMap() : 0;
            printf("[reload] frame %d: setNextLevel(%d) + runNextLevel()  (the admin pair - vt +0x30 / +0x2C)\n",
                   f, current);
            fflush(stdout);
            gameLogic->setNextLevel(current);
            gameLogic->runNextLevel();
            printf("[reload] runNextLevel returned; gameStatus=%d\n", gameLogic->getGameStatus());
            fflush(stdout);
        }
        if (!quiet) { printf("[boot] --- frame %d: BF2::run() -> BF2Engine::mainLoop() ---\n", f); fflush(stdout); }
        if (vehicleProbeEvery > 0 && f % vehicleProbeEvery == 0 && dice::hfe::world::objectManager)
        {
            const std::map<unsigned int, dice::hfe::world::IObject*>& vreg =
                dice::hfe::world::objectManager->getAllRegisteredObjects();
            for (std::map<unsigned int, dice::hfe::world::IObject*>::const_iterator it = vreg.begin();
                 it != vreg.end(); ++it)
            {
                dice::hfe::world::IObject* o = it->second;
                if (!o || !o->getTemplate() || o->getTemplate()->getClassID() != 50370u)
                    continue;
                if (dice::hfe::world::getRootParent(o) != o)
                    continue;
                const dice::hfe::Vec3& vp = o->getAbsolutePosition();
                printf("[vehicle] frame %5d  %-20s id=%-6u pos=(%8.2f, %8.2f, %8.2f)\n", f,
                       o->getTemplate()->getName().c_str(), it->first, vp.x, vp.y, vp.z);
            }
            fflush(stdout);
        }
        if (probeLogic && gameLogic)
        {
            printf("[probe] logic frame %d: %p ref=%d isServer=%d status=%d\n", f, (void*)gameLogic,
                   gameLogic->getRef(), (int)gameLogic->isServer(), gameLogic->getGameStatus());
            fflush(stdout);
        }
        if (!bf2->run())
        {
            printf("[boot] run() returned false at frame %d (engine asked to stop)\n", f);
            break;
        }
        if (aiAtFrame > 0 && f == aiAtFrame)
            spawnSyntheticPlayer(aiTeam, aiKit);
    }
    // Report vote-manager, ban-list, and server-info state after ticking.
    {
        using dice::hfe::ServerVoteManager;
        using dice::hfe::BanList;
        ServerVoteManager* votes = ServerVoteManager::getInstance();
        BanList* bans = BanList::getInstance();
        printf("[boot] tick services: ServerVoteManager map/kick1/kick2/mutiny1/mutiny2 inProgress = %d/%d/%d/%d/%d, "
               "mapChangePending = %d; BanList = %p (keys %u, addresses %u); "
               "ServerInfo dirty = %d, data block = %u bytes\n",
               (int)votes->m_mapVote.m_inProgress, (int)votes->m_kickVotes[1].m_inProgress,
               (int)votes->m_kickVotes[2].m_inProgress, (int)votes->m_mutinyVotes[1].m_inProgress,
               (int)votes->m_mutinyVotes[2].m_inProgress, (int)votes->m_mapChangePending,
               (void*)bans, bans ? (unsigned)bans->m_bannedKeys.size() : 0u,
               bans ? (unsigned)bans->m_bannedAddresses.size() : 0u,
               (int)dice::hfe::serverInfo->isDirty(), (unsigned)dice::hfe::serverInfo->m_dataBlock.size());
        // Drive the wire writer once (getDataBlock regenerates): the byte count is the measurement.
        const void* blockData = 0;
        unsigned int blockSize = 0;
        dice::hfe::serverInfo->getDataBlock(&blockData, &blockSize, true);
        printf("[boot] tick services: ServerInfo::generateDataBlock(full) -> %u bytes, gameName = \"%s\", welcome[0] = \"%s\"\n",
               blockSize, dice::hfe::serverInfo->m_block.gameName.c_str(),
               dice::hfe::serverInfo->m_block.welcomeMessages.empty() ? "" : dice::hfe::serverInfo->m_block.welcomeMessages[0].c_str());
        // Report registered objects after the frame loop, including objects created by spawners.
        if (dice::hfe::world::objectManager)
        {
            const std::map<unsigned int, dice::hfe::world::IObject*>& reg =
                dice::hfe::world::objectManager->getAllRegisteredObjects();
            printf("[boot] tick services: ObjectManager registered after the frames = %u\n",
                   (unsigned)reg.size());

            // Group live objects by template to identify growth between runs with different frame counts.
            std::map<std::string, unsigned int> byTemplate;
            for (std::map<unsigned int, dice::hfe::world::IObject*>::const_iterator it = reg.begin();
                 it != reg.end(); ++it)
            {
                const char* name = "<null object>";
                if (it->second && it->second->getTemplate())
                    name = it->second->getTemplate()->getName().c_str();
                else if (it->second)
                    name = "<no template>";
                ++byTemplate[name];
            }
            // Print the twelve most common templates, largest first.
            for (int rank = 0; rank < 12; ++rank)
            {
                std::map<std::string, unsigned int>::iterator best = byTemplate.end();
                for (std::map<std::string, unsigned int>::iterator it = byTemplate.begin();
                     it != byTemplate.end(); ++it)
                    if (best == byTemplate.end() || it->second > best->second)
                        best = it;
                if (best == byTemplate.end() || best->second == 0)
                    break;
                printf("[boot] leak probe: %6u  %s\n", best->second, best->first.c_str());
                byTemplate.erase(best);
            }
        }
        fflush(stdout);

        // Dump reserved network ids and the first dynamic range to identify occupied slots.
        if (dice::hfe::io::networkManager)
        {
            printf("[boot] netid probe: reserved-range descriptors after the frames:\n");
            unsigned int reservedHits = 0, dynamicHits = 0;
            for (unsigned int id = 1; id <= 800; ++id)
            {
                dice::hfe::io::NetworkableDescriptor* const d =
                    dice::hfe::io::networkManager->getDescriptor(static_cast<unsigned short>(id));
                if (!d)
                    continue;
                if (id >= 642) { ++dynamicHits; continue; }
                ++reservedHits;
                dice::hfe::world::IObject* const obj =
                    dice::hfe::io::networkManager->getObject(static_cast<unsigned short>(id));
                const char* what = "<bare networkable>";
                if (obj && obj->getTemplate())
                    what = obj->getTemplate()->getName().c_str();
                else if (obj)
                    what = "<object, no template>";
                printf("[boot] netid probe:   id %4u  %s\n", id, what);
            }
            printf("[boot] netid probe: %u reserved-range ids occupied, %u dynamic (642..800 window)\n",
                   reservedHits, dynamicHits);
            // The dynamic window in detail - what each registered descriptor belongs to.
            for (unsigned int id = 642; id <= 800; ++id)
            {
                if (!dice::hfe::io::networkManager->getDescriptor(static_cast<unsigned short>(id)))
                    continue;
                dice::hfe::world::IObject* const obj =
                    dice::hfe::io::networkManager->getObject(static_cast<unsigned short>(id));
                const char* what = "<bare networkable>";
                if (obj && obj->getTemplate())
                    what = obj->getTemplate()->getName().c_str();
                printf("[boot] netid probe:   dyn %4u  %s\n", id, what);
            }
            // And the attach census: how many registry objects carry a networkable at all, and how
            // many of those still have id 0 (attached but never registered - the sweep's leftovers).
            if (dice::hfe::world::objectManager)
            {
                const std::map<unsigned int, dice::hfe::world::IObject*>& reg2 =
                    dice::hfe::world::objectManager->getAllRegisteredObjects();
                unsigned int withNw = 0, unregistered = 0;
                for (std::map<unsigned int, dice::hfe::world::IObject*>::const_iterator it = reg2.begin();
                     it != reg2.end(); ++it)
                {
                    if (!it->second) continue;
                    dice::hfe::io::INetworkable* const nw = it->second->getNetworkable();
                    if (!nw) continue;
                    ++withNw;
                    if (nw->m_networkableId == 0)
                        ++unregistered;
                }
                printf("[boot] netid probe: registry objects with a networkable = %u, of those with id 0 (never registered) = %u\n",
                       withNw, unregistered);
            }

            // Dump template ids and per-type counts to compare registration order across runs.
            if (dice::hfe::world::objectTemplateManager)
            {
                unsigned int templateCount = 0;
                for (unsigned int tid = 1; tid <= 8000; ++tid)
                {
                    dice::hfe::world::IObjectTemplate* const t =
                        dice::hfe::world::objectTemplateManager->getTemplate(tid);
                    if (!t)
                        continue;
                    ++templateCount;
                    // the defining .con file (createTemplate's m_workingFolders record) - the
                    // column that exposes a template born from a file retail's server never runs.
                    const std::string& conFile =
                        dice::hfe::world::objectTemplateManager->getWorkingFolderAndFile(t->getName());
                    logf("[boot] TPL %4u cid=%u %s | %s\n", tid, t->getClassID(),
                         t->getName().c_str(), conFile.c_str());
                }
                logf("[boot] TPL total registered (normal id space, 1..8000): %u\n", templateCount);
            }

            // Report the prestored-id bracket, kit map, and resolved templates to diagnose missing warmup objects.
            if (dice::hfe::gameLogic)
            {
                printf("[boot] warmup probe: preStored bracket = [%u .. %u)  (0xFFFFFFFF = never ran)\n",
                       dice::hfe::gameLogic->getStartPreStoredNetID(),
                       dice::hfe::gameLogic->getEndPreStoredNetID());
                typedef std::map<int, std::string> KitMap;
                const KitMap* const kits =
                    static_cast<const KitMap*>(dice::hfe::gameLogic->getKits());   // +0x150, opaque
                if (kits)
                {
                    printf("[boot] warmup probe: m_kits holds %u entries\n",
                           static_cast<unsigned int>(kits->size()));
                    for (KitMap::const_iterator it = kits->begin(); it != kits->end(); ++it)
                    {
                        const dice::hfe::world::IObjectTemplate* t = 0;
                        if (dice::hfe::world::objectTemplateManager)
                            t = dice::hfe::world::objectTemplateManager->getTemplate(it->second);
                        printf("[boot] warmup probe:   kit key 0x%04X -> %-28s template %s\n",
                               static_cast<unsigned int>(it->first), it->second.c_str(),
                               t ? "OK" : "MISSING");
                    }
                }
            }
            fflush(stdout);
        }
    }

    // Report final player state; a live player outside its free camera confirms an actual spawned body.
    if (dice::hfe::world::playerManager)
    {
        dice::hfe::world::IPlayer* fp = dice::hfe::world::playerManager->getPlayerFromId(255);
        if (fp)
        {
            printf("[boot] player after the frames: id %d team %d spawnGroup %d kit %d  "
                   "isAlive %d  vehicle %p  freeCamera %p  (spawned %d)\n",
                   fp->getId(), fp->getTeam(), fp->getSpawnGroup(), fp->getKit(),
                   (int)fp->getIsAlive(), (void*)fp->getVehicle(), (void*)fp->getFreeCamera(),
                   (int)(fp->getIsAlive() && fp->getVehicle() != fp->getFreeCamera()));
        }
        fflush(stdout);
    }

    // [TEMP] BF2_SHUTDOWN deletes BF2 after the frame loop and probes immediate listen-port
    // rebind. [PLATFORM] This follows Win32 shutdown; the ELF entry point returns without deletion.
    // Rebind proves the socket was released, not that another server can start successfully.
    if (GetEnvironmentVariableA("BF2_SHUTDOWN", 0, 0) != 0)
    {
        int port = 16567;
        {
            char pbuf[16] = {0};
            if (GetEnvironmentVariableA("BF2_PORT", pbuf, sizeof(pbuf)) > 0)
                port = atoi(pbuf);
        }
        printf("[boot] BF2_SHUTDOWN - delete bf2: ~BF2 -> BF2::shutdown -> BF2Engine::shutdown -> "
               "shutdownGame -> GameServer::shutdown -> NetServer::shutDown\n");
        fflush(stdout);
        const DWORD t0 = GetTickCount();
        delete bf2;
        bf2 = 0;
        printf("[boot] shutdown: delete bf2 returned after %lu ms\n", (unsigned long)(GetTickCount() - t0));
        fflush(stdout);

        WSADATA wsa;
        WSAStartup(MAKEWORD(2, 2), &wsa);                    // refcounted; the server never cleans up
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family      = AF_INET;
        addr.sin_port        = htons((unsigned short)port);
        addr.sin_addr.s_addr = INADDR_ANY;
        const int r = bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        const int err = (r != 0) ? WSAGetLastError() : 0;
        printf("[boot] rebind probe: bind(0.0.0.0:%d) -> %d (WSA %d)  %s\n", port, r, err,
               r == 0 ? "PORT FREE - the listener released it" : "PORT STILL HELD");
        fflush(stdout);
        closesocket(s);

        // WinMain's last act: the System goes after the game (PE 0x401890, vtable +0 with flag 1).
        delete dice::hfe::g_system;
        dice::hfe::g_system = 0;
        printf("[boot] shutdown: System deleted\n");
        fflush(stdout);
    }

    printf("[boot] COMPLETED %d frames without faulting\n", frames);
    fflush(stdout);
    // Restore the terminal explicitly because engine shutdown does not delete the status monitor.
    serverScreenTakeBack();
    return 0;
}
