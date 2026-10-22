// SwiffPlayer lifecycle, movie pipeline, renderer adapter, and ActionScript bindings.
// [CHOSEN, not recovered] Rebuilt client/plugin ABI only: https://github.com/B2BF/OpenBF2/issues/7.
// Internal fixed-layout storage and unresolved host frontiers remain in place.

#ifdef BF2_IMAGE_SWIFFPLAYER

#include "SwiffPlayer.h"

#include "../../../../RendDX9Abi/BfMemory.h"
#include "../../../../RendDX9Abi/HfeAbi.h"
#include "../../../../RendDX9Abi/Vc71String.h"   // retained fixed-layout internal strings
#include "../../../../RendDX9Abi/Vc71Vector.h"   // the +1104 queued-mouse vector

#include "../../../Menu/ISwiffHost.h" // native storage calls share the owner's std::string ABI
#include "../../../Menu/IServerBrowser.h"  // stopBrowser's list handle (+0x30 stopUpdate)
#include "../../../Game/LocalProfileManager.h"
#include "../../../Game/ProfileManager.h"
#include "../../../Game/GlobalSettings.h"   // IGlobalSettings::getNamePrefix, the +0x28 clan tag
#include "../../../Game/IModInfo.h"        // native: the typed mod/map records getListEntries("installedMaps") walks
#include "../../../Game/GeneralSettings.h"  // native: GeneralSettings::getUseBots, the retail +0xA8 slot
#include "../../../Game/VideoSettings.h"    // native: IVideoSettings, the dice.bf2.VideoSettings rows' owner
#include "../../../Game/ControlSettings.h"  // native: IControlSettings, the dice.bf2.ControlSettings rows' owner
#include "../../../Game/AudioSettings.h"    // native: IAudioSettings, the dice.bf2.AudioSettings rows' owner
#include "../../../Game/GameServerSettings.h" // native: IGameServerSettings::setTeamRatio / setNoVehicles
#include "../../../Game/HudInformationLayer.h" // native: eorsinfo publishes the joined server through +0x8C
#include "../../../Game/Persistence/PlayerUnlockInfo.h" // native: IPlayerUnlockInfo::getItems, the names selectFirstUnlock resolves
#include "../../../Game/DemoRecorder/DemoLibrary.h" // native: Multiplay's bookmark flag pair
#include "../../../Menu/IMenuTeamManager.h"    // native: the typed team -> kit -> equipment walk behind getEquipmentName & co
#include "../../../Menu/ServerLogoManager.h"   // native: the typed logo cache behind Multiplay.downloadServerLogo

#include "base/tu_file.h"   // the file_opener_callback return type
#include "base/image.h"     // image::rgb / image::rgba (create_bitmap_info_rgb/rgba read w/h)
#include "gameswf_action.h" // as_object / as_value / get_global_object - the AS-bridge graph build

#include <new>
#include <string>
#include <string.h>
#include <stdio.h>         // memset, memcpy
#include <time.h>          // time(): the stamp the host records carry
#include <stdlib.h>         // getenv (the AS-diagnostic gate)
#include <stdarg.h>         // va_list: swiffFormatString (sub_10003320) behind Logic.setDecimals
#include <map>              // unk_100F882C: the server-browser filter clauses, keyed by bit

namespace dice {
namespace hfe {

// classManager, filled by initDll (SwiffPlayerAbi/PluginEntry.cpp). The ctor reaches it for the
// Bf2VoipClient singleton, exactly as the shipped ctor reaches g_hfeClassManager. fileManager is the
// engine VFS the file opener reads SWFs through.
extern IClassManager* classManager;
extern IBase*         fileManager;

namespace ui {


// The internal object layout remains fixed while the rebuilt client and gameswf share it.
// Reject an accidental reorder or added member at compile time.

static_assert(sizeof(SwiffPlayer) == 1120, "SwiffPlayer must be 0x460 bytes (IDB dice_hfe_SwiffPlayer_BF2)");

// dword_100F2F20 in the shipped image - the ctor stores `this` here. Read by the fscommand sink to
// find the live player. File-scope, one instance, matching the shipped global.
static SwiffPlayer* g_swiffPlayerInstance = 0;

// The gameswf modifier-key latches the shipped image keeps as file-scope bytes (byte_100F8EE8/9/A/B),
// NOT as object members - notifyChar/notifyKeyState/resetModifierState touch these.
static unsigned char s_shiftHeld = 0;   // byte_100F8EE8  (vkey 16)
static unsigned char s_altHeld   = 0;   // byte_100F8EE9  (vkey 18)
static unsigned char s_ctrlHeld  = 0;   // byte_100F8EEA  (vkey 17)
static unsigned char s_capsToggle = 0;  // byte_100F8EEB  (vkey 20)

// dword_100F8EE0 - the optional text-input / IME handler the host installs. Null in the skeleton;
// notifyChar/setTextInput/getTextInput* dispatch through it and are frontiers until Stage 5.
static void* s_imeHandler = 0;


// gameswf file callbacks forward through the engine VFS stream carried as tu_file appdata.
// Preserve stream slot order and the release operation's ownership.
namespace {
inline void** handleVtbl(void* h) { return *reinterpret_cast<void***>(h); }
}
static int  swiffFileRead(void* dst, int bytes, void* appdata)          // sub_100050B0 -> handle +0x10
{ return reinterpret_cast<int(__thiscall*)(void*,void*,int)>(handleVtbl(appdata)[0x10/4])(appdata, dst, bytes); }
static int  swiffFileWrite(const void* src, int bytes, void* appdata)   // sub_100050D0 -> handle +0x14
{ return reinterpret_cast<int(__thiscall*)(void*,const void*,int)>(handleVtbl(appdata)[0x14/4])(appdata, src, bytes); }
static int  swiffFileSeek(int pos, void* appdata)                       // sub_100050F0 -> handle +0x30 abs
{
    const unsigned char r = reinterpret_cast<unsigned char(__thiscall*)(void*,int,int)>(handleVtbl(appdata)[0x30/4])(appdata, 0, pos);
    // Shipped: BF2 seek returns nonzero on success; tu_file wants 0 = no error, 4 = TU_FILE_SEEK_ERROR.
    return (r != 0) ? TU_FILE_NO_ERROR : TU_FILE_SEEK_ERROR;
}
static int  swiffFileSeekToEnd(void* appdata)                          // sub_10005110 -> getSize +0x38 then seek +0x30
{
    // PE 10005114 captures this table before getSize; 10005121..129 maps AL to 0/4.
    void** const vtbl = handleVtbl(appdata);
    const int size = reinterpret_cast<int(__thiscall*)(void*)>(vtbl[0x38/4])(appdata);
    const unsigned char r = reinterpret_cast<unsigned char(__thiscall*)(void*,int,int)>(vtbl[0x30/4])(appdata, 0, size);
    return r != 0 ? TU_FILE_NO_ERROR : TU_FILE_SEEK_ERROR;
}
static int  swiffFileTell(const void* appdata)                        // sub_10005130 -> handle +0x34
{ void* h = const_cast<void*>(appdata); return reinterpret_cast<int(__thiscall*)(void*)>(handleVtbl(h)[0x34/4])(h); }
static bool swiffFileGetEof(void* appdata)                            // sub_10005140 -> (handle +0x1C == 0)
{ return reinterpret_cast<int(__thiscall*)(void*)>(handleVtbl(appdata)[0x1C/4])(appdata) == 0; }
static int  swiffFileClose(void* appdata)                            // sub_10005150 -> release +0x08
{ reinterpret_cast<void(__thiscall*)(void*)>(handleVtbl(appdata)[0x08/4])(appdata); return TU_FILE_NO_ERROR; }

// sub_100051F0 - the file opener gameswf calls for every SWF/cache read. Opens the path through the
// engine FileManager (VFS, so it sees inside the mounted client .zip archives) and wraps the returned
// stream in a tu_file with the 7 callbacks above. gameswf owns and deletes the tu_file.
static tu_file* swiffFileOpener(const char* urlOrPath)
{
    void* handle;
    {
        renddx9abi::HostString name(urlOrPath);
        // PE 10005201..213 has no manager guard. Native IFileSystem::open also
        // occupies +0x18; its argument is the selected host string, not internal VC7.1 storage.
        typedef void* (__thiscall* FmOpenFn)(void* self, const renddx9abi::HostString& name, int mode, int access, int flags);
        void** fmv = *reinterpret_cast<void***>(fileManager);
        handle = reinterpret_cast<FmOpenFn>(fmv[0x18/4])(fileManager, name, 2, 0, 0);
    } // PE 1000521B destroys the filename before testing the handle or allocating tu_file.
    if (!handle)
        return 0;

    return new tu_file(handle, swiffFileRead, swiffFileWrite, swiffFileSeek,
                       swiffFileSeekToEnd, swiffFileTell, swiffFileGetEof, swiffFileClose);
}

static void swiffFsCommand(gameswf::movie_interface* /*movie*/, const char* /*command*/,
                           const char* /*arg*/)
{
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: swiff-native-as-host]
    ::fprintf(stderr, "[FRONTIER] dice::hfe::ui::swiffFsCommand reached (native settings/debug sink unresolved)\n");
    ::fflush(stderr);
    ::abort();
}


// AS diagnostic LOG, gated on the environment variable BF2_SWIFFPLAYER_ASLOG=1.

// gameswf reports every unresolved script call ("call_method can't find method X", "'X' is not a
// method", "can't create unregistered object X"). Routing that to our log is how the AS-bridge work
// is sized: run a movie, read which methods it actually calls, implement those. Off by default, so a
// normal build behaves exactly as retail. [TEMP] [RETIRE-WHEN: RW-88]

static bool asLogEnabled()
{
    static int s_on = -1;
    if (s_on < 0)
    {
        const char* v = ::getenv("BF2_SWIFFPLAYER_ASLOG");
        s_on = (v && v[0] >= '1' && v[0] <= '9') ? 1 : 0;   // any non-zero level; 2 adds gameswf's log_msg lines
    }
    return s_on != 0;
}


// [TEMP] [CHOSEN, not recovered] BF2_SWIFFPLAYER_STREAMLOG=N traces player inputs and displays.
// Logs the first N displays, then every 100th; disabled unless configured.
// [RETIRE-WHEN: the SwiffPlayer mispositioning is root-caused - strip this block and its call sites]

static int streamLogFrames()
{
    static int s_n = -1;
    if (s_n < 0)
    {
        const char* v = ::getenv("BF2_SWIFFPLAYER_STREAMLOG");
        s_n = v ? ::atoi(v) : 0;
        if (s_n < 0) s_n = 0;
        if (s_n > 0)
            ::renddx9abi::logf("[stream] tracer ON: first %d displays in full (then every 100th)", s_n);
    }
    return s_n;
}
static bool     streamLogEnabled() { return streamLogFrames() > 0; }
static int      s_streamDisplays = 0;     // display() count
static bool     s_streamVerbose  = false; // inside a fully-logged display
static unsigned s_streamCounts[8];        // [0]transforms [1]strips [2]verts [3]lines [4]fills [5]tex [6]masks [7]cxforms
#define SPL(...) do { if (streamLogEnabled() && s_streamVerbose) ::renddx9abi::logf(__VA_ARGS__); } while (0)
// [TEMP] RW-89: the ungated variant for the load-time text calls (create/size/set/extent), which run
// before the first display and so outside the verbose window.
#define SPLA(...) do { if (streamLogEnabled()) ::renddx9abi::logf(__VA_ARGS__); } while (0)

static void swiffAsLogCb(bool error, const char* message)
{
    if (!message) return;
    // [TEMP] BF2_SWIFFPLAYER_ASLOG=2 also forwards gameswf's informational log_msg lines (the silent
    // "set_member ... on invalid object" and "get_member" misses that the stock player drops), which
    // is how a component that quietly takes the wrong branch gets found. Errors always pass.
    if (!error)
    {
        const char* e = getenv("BF2_SWIFFPLAYER_ASLOG");
        if (!e || e[0] < '2') return;
        ::renddx9abi::logf("[AS-msg] %s", message);
        return;
    }
    // [TEMP] RW-89 log hygiene: error lines repeating (a per-frame callback failing the same way,
    // or a script loop alternating two failures) are printed the first 5 times each and counted
    // after, in a small table of the last distinct messages summarised every 1000th repeat.
    {
        enum { SLOTS = 8, PRINT_FIRST = 5 };
        static char s_msg[SLOTS][256]; static unsigned s_count[SLOTS]; static int s_next = 0;
        int hit = -1;
        for (int k = 0; k < SLOTS; ++k)
            if (s_msg[k][0] && ::strncmp(s_msg[k], message, sizeof(s_msg[k]) - 1) == 0) { hit = k; break; }
        if (hit < 0)
        {
            hit = s_next; s_next = (s_next + 1) % SLOTS;
            ::strncpy(s_msg[hit], message, sizeof(s_msg[hit]) - 1); s_msg[hit][sizeof(s_msg[hit]) - 1] = 0;
            s_count[hit] = 0;
        }
        const unsigned n = ++s_count[hit];
        if (n > PRINT_FIRST)
        {
            if (n % 1000 == 0) ::renddx9abi::logf("[AS] (repeated %u times) %.80s", n, message);
            return;
        }
    }
    ::renddx9abi::logf("[AS] %s", message);
}


// ActionScript namespace retaining its dotted path for qualified missing-member diagnostics.
// Missing lookups log and return not-found, allowing script namespace probes to continue.
// [RETIRE-WHEN: RW-88]

class SwiffAsNamespace : public gameswf::as_object
{
public:
    explicit SwiffAsNamespace(const char* dottedPath)
    {
        ::strncpy(m_path, dottedPath ? dottedPath : "?", sizeof(m_path) - 1);
        m_path[sizeof(m_path) - 1] = '\0';
    }

    template<gameswf::as_c_function_ptr Method>
    void registerMethod(const char* name)
    {
        // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: swiff-native-as-host]
        // Publish the known callback, but stop at its actual invocation until its
        // host record/string/vector exchanges have a native adapter. Probing a
        // namespace or parsing a movie does not invoke this stop.
        set_member(tu_stringi(name), gameswf::as_value(&nativeMethodFrontier<Method>));
    }

    // Use only after the callback's complete host boundary has been audited for
    // the native ABI. Logic.active has no host boundary (DLL 1004CCA0..1004CCE7).
    template<gameswf::as_c_function_ptr Method>
    void registerNativeCompatibleMethod(const char* name)
    {
        set_member(tu_stringi(name), gameswf::as_value(Method));
    }

    virtual bool get_member(const tu_stringi& name, gameswf::as_value* val)
    {
        if (gameswf::as_object::get_member(name, val))
            return true;
        // Filter routine VM internals. A lookup miss may be a namespace-existence probe;
        // only a subsequent failed method call identifies an unavailable operation.
        if (asLogEnabled() && name.c_str()[0] != '_' && name != "constructor" && name != "prototype")
            ::renddx9abi::logf("[AS-MISS] %s.%s  (not found; a call -> owe a body, a probe -> benign)",
                               m_path, name.c_str());
        return false;
    }

private:
    template<gameswf::as_c_function_ptr Method>
    static void nativeMethodFrontier(const gameswf::fn_call&)
    {
        // [TEMP] Also log through the windowed-client channel, where stderr may be unavailable.
        ::renddx9abi::logf("[FRONTIER] dice::hfe::ui::SwiffAsNamespace::nativeMethodFrontier reached: %s (native host ABI unresolved)", __FUNCSIG__);
        ::fprintf(stderr, "[FRONTIER] dice::hfe::ui::SwiffAsNamespace::nativeMethodFrontier reached: %s (native host ABI unresolved)\n", __FUNCSIG__);
        ::fflush(stderr);
        ::abort(); // deliberately independent of BF2_SWIFFPLAYER_FRONTIER=continue
    }
    char m_path[96];
};


// ActionScript bindings. Unavailable engine-backed operations carry explicit diagnostics
// and documented result behavior at their individual boundaries. [RETIRE-WHEN: RW-88]

namespace {

// [TEMP] Logs ActionScript arguments/results when BF2_SWIFFPLAYER_ASLOG=1.
// Remove this entry tracer when the remaining bridge faults are resolved.
struct SwfAsTraceScope
{
    const gameswf::fn_call& fn;
    const char*             name;
    SwfAsTraceScope(const gameswf::fn_call& f, const char* n) : fn(f), name(n)
    {
        if (!asLogEnabled()) return;
        ::renddx9abi::logf("[as] enter %s (nargs=%d)", name, fn.nargs);
        for (int i = 0; i < fn.nargs; ++i)
        {
            const char* s = fn.arg(i).to_string();
            ::renddx9abi::logf("[as]   arg%d = \"%s\"", i, s ? s : "");
        }
    }
    ~SwfAsTraceScope()
    {
        if (!asLogEnabled()) return;
        const char* s = fn.result ? fn.result->to_string() : 0;
        ::renddx9abi::logf("[as] leave %s -> \"%s\"", name, s ? s : "");
    }
};
#define SWF_AS_TRACE(name) SwfAsTraceScope _swfAsTraceScope(fn, (name))
#define SWF_AS_LEAVE(name) do { } while(0)

// Set the string result of an AS call.
inline void asReturnString(const gameswf::fn_call& fn, const char* s)
{
    fn.result->set_tu_string(tu_string(s ? s : ""));
}

// Defined further down beside the raw-vtable helpers; declared before the loading-screen callbacks.
static void*  asHost();
inline void** rcVtbl(void* p);

// Forwards the cursor-name string through the host capability slot. The renderer ignores
// its contents and shows the hardware cursor. Requires player/host; writes no AS result.
void as_cursor_setCursor(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_cursor_setCursor");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string name(fn.arg(0).to_string());
    (void) h->setPlayerRenderCapability0(static_cast<int>(reinterpret_cast<intptr_t>(&name)));   // +0x68
}

// Requests the cursor name through a preconstructed string. Returns the string, or the
// literal "undefined" on refusal. The current renderer succeeds with an empty name.
void as_cursor_getCursor(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_cursor_getCursor");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string out;
    const bool ok = h->setPlayerRenderCapability1(static_cast<int>(reinterpret_cast<intptr_t>(&out)));   // +0x70
    fn.result->set_tu_string(tu_string(ok ? out.c_str() : "undefined"));
}

// dice.bf2.Cursor.enableCursor(flag),
// The host (unguarded), then arg(0).to_bool() through the bounds-checked accessor - after the host -
// then the host's +0x74 with the Boolean (ISwiffHost slot 29, BF2.exe 0x5DF3C0, an ICF-shared byte
// store to the host's +0x28: this row is that flag's only AS writer, and +0x78 reads it back). The
// shipped push carries the Boolean in the low byte of the stack word the argument index occupied;
// the slot reads only that byte. No AS result.
void as_cursor_enableCursor(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_cursor_enableCursor");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const bool flag = fn.arg(0).to_bool();
    h->setFlag28(flag);                                                            // +0x74
}

// Returns CommandLine.cmd contents, or an empty string on failure. Reads no AS arguments.
void as_client_getCommandlineOption(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getCommandlineOption");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string out;
    const bool ok = h->getCommandLineFile(out);                                    // +0x88
    fn.result->set_tu_string(tu_string(ok ? out.c_str() : ""));
}

// Reads the player-name setting using argument zero as fallback.
// Requires player/host; on refusal leaves the AS result untouched.
void as_client_getPlayerName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getPlayerName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string out;
    const std::string fallback(fn.arg(0).to_string());
    if (h->getPlayerName(out, fallback))                                           // +0x80
        fn.result->set_tu_string(tu_string(out.c_str()));
}

// Forwards argument zero as the player name or command-line file text.
// Requires player/host; host Boolean results are discarded.
void as_client_setPlayerName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_setPlayerName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string name(fn.arg(0).to_string());
    (void) h->setPlayerName(name);                                                 // +0x84
}

void as_client_setCommandlineOption(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_setCommandlineOption");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string text(fn.arg(0).to_string());
    (void) h->setCommandLineFile(text);                                            // +0x8C
}

// the five Client rows polled by endOfRound.swf. They are all thin,
// unguarded dispatches through the player's current SwiffHost. Note that getEORType is a number
// (1 only for host status 1), and getTotalTimeUntilRestart deliberately fetches and discards the
// host before returning the shipped constant numeric zero.
void as_client_getLoadProgress(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getLoadProgress");
    const float value = g_swiffPlayerInstance->getSwiffHost()->getLoadingProgress();
    fn.result->set_double(static_cast<double>(value));
}

void as_client_getEORType(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getEORType");
    const int status = g_swiffPlayerInstance->getSwiffHost()->getGameStatus();
    fn.result->set_double(status == 1 ? 1.0 : 0.0);
}

void as_client_getTimeUntilRestart(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getTimeUntilRestart");
    const float value = g_swiffPlayerInstance->getSwiffHost()->getTimeUntilRestartMap();
    fn.result->set_double(static_cast<double>(value));
}

void as_client_getTotalTimeUntilRestart(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getTotalTimeUntilRestart");
    (void) g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_double(0.0);
}

void as_client_getCurrentLoadFile(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_getCurrentLoadFile");
    const std::string value = g_swiffPlayerInstance->getSwiffHost()->getLoadingRecordString();
    fn.result->set_tu_string(tu_string(value.c_str()));
}

// dice.bf2.Client.hasUserEscapedFromGame(),
// Byte for byte the shape of dice.bf2.Logic.isGameRunning (1004CC30): the host (unguarded), a stack
// byte zeroed, the host's +0xB8 with its address - BF2.exe 0x486C90, `*out = game != 0` - the slot's
// own Boolean dropped, and the byte published as the boolean. So the row answers "is a game running":
// that is what the shipped DLL means by the user having escaped from a game to the menu. No argument.
void as_client_hasUserEscapedFromGame(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_client_hasUserEscapedFromGame");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    bool running = false;
    (void) h->isGameRunning(&running);                                             // +0xB8
    fn.result->set_bool(running);
}

// dice.General.getHostVersion(), and promoted.

//   host = player->+0x5C                          unguarded (the old `if (h)` was invented)
//   out  = std::string()                          constructed after the host fetch
//   ok   = host->+0x1A0 getVersionString(out)     BF2.exe 0x486D60 - the build string
//   result = string (ok ? out : "Undefined Version")
//   destroy out
void as_general_getHostVersion(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_getHostVersion");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string out;
    const bool ok = h->getVersionString(out);                                      // +0x1A0
    fn.result->set_tu_string(tu_string(ok ? out.c_str() : "Undefined Version"));
}


// Profile callbacks resolve their singletons by name through ClassManager.


// discarded player getSwiffHost, local then online singleton
// lookups (each name dies before the next call), online+44 Boolean ->2, otherwise
// local+44 full current-profile pointer ->1 or0. No player/registry/singleton guards.
void as_profile_getLoginStatus(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getLoginStatus");
    (void)g_swiffPlayerInstance->getSwiffHost();

    IBase* local  = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    IBase* online = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));

    const bool loggedIn = static_cast<IProfileManager*>(online)->isLoggedIn();
    double status = 2.0;
    if (!loggedIn)
    {
        const bool current = static_cast<ILocalProfileManager*>(local)->getCurrentProfile() != nullptr;
        status = current ? 1.0 : 0.0;
    }
    fn.result->set_double(status);
}

// The original VC7.1 LocalProfileManager vector: the allocator word first, then first/last/end
// (sub_10060030 zeroes the three words after a one-word head and reads the count from them). A
// three-word layout put the callee's writes one slot off: retail runs R..AA counted 0 profiles and
// the movie took the first-time "newbie" page instead of "login".
struct Vc71Vector { int alloc; char* first; char* last; char* end; };

// One local profile record (176 bytes): six VC7.1 std::strings, destroyed in the shipped order by
// sub_1001DC10 (+140, +112, +84, +56, +28, +0), then the vector buffer goes back to the engine heap
// (sub_1001FE70 -> bf_free). The retained vc71::string storage now uses the shared native CRT.
// Fields the original list bodies read: [0] the profile id GlobalSettings compares,
// [1] profileName, [2] the offline display name, [3] the GameSpy nick (empty = offline), [5] password.
struct LocalProfileRecord { vc71::string s[6]; char tail[8]; };
static_assert(sizeof(LocalProfileRecord) == 176, "LocalProfileRecord must be 176 bytes (sub_10015040)");

static void destroyLocalProfiles(Vc71Vector& v)
{
    if (!v.first) return;
    const int n = static_cast<int>((v.last - v.first) / 176);
    for (int i = 0; i < n; ++i)
    {
        LocalProfileRecord* r = reinterpret_cast<LocalProfileRecord*>(v.first + 176 * i);
        for (int k = 5; k >= 0; --k) r->s[k].~string();
    }
    ::renddx9abi::engineFree(v.first);
    v.first = v.last = v.end = 0;
}

// unguarded registry lookup; absent singleton -> numeric 0.
// A present owner fills an empty vector with validOnly=true, return ignored.
// Its signed count is assigned before record strings and vector storage die.
void as_profile_getNumLocalProfiles(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNumLocalProfiles");
    IBase* local = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    if (!local) { fn.result->set_double(0.0); return; }

    std::vector<LocalProfile> profiles;
    (void)static_cast<ILocalProfileManager*>(local)->getLocalProfiles(profiles, true);
    const int count = static_cast<int>(profiles.size());
    fn.result->set_double(static_cast<double>(count));
}

// Forward declarations of the SwiffHost dispatch helpers used by the AS bodies below; they are
// defined further down, beside the other rc*/host* raw-vtable helpers.
inline bool hostIsGameRunning(void* h, bool* out);
inline int  hostGetActivateReason(void* h);
inline void hostSetActivateReason(void* h, int v);
inline void hostGetFrameLabel(void* h, void* outStr);
inline void hostGetModVersion(void* h, void* outStr);
inline void hostGetStorageString(void* h, void* outStr, const void* a, const void* b);
inline void hostSetStorageString(void* h, const void* a, const void* b);
inline int  hostGetCommandLine(void* h);
inline void hostLocaleGetString(void* h, void* outWstr, const void* idStr);
inline int   hostRequest(void* h, void* req);
inline void* hostGetCursor(void* h, int which);
inline bool  hostSetBusy(void* h, int busy);
inline void  hostCursorRestored(void* h);
inline bool  hostCheckDisc(void* h);
inline void  hostReportNoDisc(void* h);
inline bool  hostJoinServer(void* h, const void* address, int port, int queryPort);
inline void  hostSetSelectedServer(void* h, const void* address, int queryPort, int hostPort);
inline void  hostGetSelectedServer(void* h, void* outStr, int* queryPort, int* hostPort);
inline int   hostGetIndexedInt(void* h, int index);
inline void  hostSetIndexedInt(void* h, int index, int value);
inline void* hostGetServerLogoManager(void* h);
inline void  hostGetJoinPassword(void* h, void* outStr);
inline void  hostSetJoinPassword(void* h, const void* str);


// Logic callbacks dispatch through the player's ISwiffHost.


// Every one of these begins the same way the shipped handlers do.
static void* asHost()
{
    return g_swiffPlayerInstance ? static_cast<void*>(g_swiffPlayerInstance->getSwiffHost()) : 0;
}

// direct player host lookup, then initialize bool output
// to false. Host+B8's return is ignored; its output becomes the AS Boolean.
// No player/host guards or argument reads.
void as_logic_isGameRunning(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_isGameRunning");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    bool running = false;
    (void)h->isGameRunning(&running);
    fn.result->set_bool(running);
}

// fetch the host once, read signed message id (+BC),
// replace the numerical AS result, then reload that same host's table and clear
// its id (+C0). No player/host guards; result destruction precedes the clear.
void as_logic_getActivateReason(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getActivateReason");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int reason = h->getMessageId();
    fn.result->set_double(static_cast<double>(reason));
    h->setMessageId(0);
}

// one direct host lookup, by-value string from +C4,
// copy c_str() to an AS string, destroy that temporary, then the host string.
// No player/host guards or argument reads. Retail constructs its hidden result.
void as_logic_getFrameLabel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getFrameLabel");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string out = h->getFrameLabel();
    fn.result->set_tu_string(tu_string(out.c_str()));
}

// Reads mod version into a constructed output string and publishes it regardless of the
// host Boolean result. This output is an assigned reference, not a hidden-return buffer.
void as_logic_getModVersion(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getModVersion");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    renddx9abi::HostString out;     // the host assigns to this already constructed string
    (void)h->getVersionString(out);
    fn.result->set_tu_string(tu_string(out.c_str()));
}

// sub_1004CCA0 - dice.bf2.Logic.active(): the shipped body is the constant `true` (no host call).
// It is load-bearing: UIObject's __set____authorMode computes
//   m_inAuthorMode = nValue || !dice.bf2.Logic.active()
// so without it every component stayed in authoring mode and drew its placeholder rectangle.
void as_logic_active(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_active");
    fn.result->set_bool(true);
}

// PE1004C650..create dice String first, then unguarded player host,
// coerce arg0 into an owned key, construct wide output and call host+A8.
// Ignore its Boolean; copy out.c_str() to the wide payload, clear narrow text,
// assign the object result, then destroy wide output and key. No argc guards.
void as_locale_getString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_locale_getString");
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(nullptr, 0);
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    renddx9abi::HostString ids(fn.arg(0).to_string());
    std::wstring out;
    h->getLocalizedString(out, ids);
    gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(out.c_str()));
    fn.result->set_as_object_interface(text);
}

// Coerces argument zero to Boolean and sets the player's mode flag; no AS result is written.
// Requires the player and argument. Coercion precedes player dereference.
void as_logic_setAsync(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setAsync");
    const char flag = fn.arg(0).to_bool() ? 1 : 0;
    g_swiffPlayerInstance->setModeFlag(flag);
}

// sub_10003320 - the image's printf-into-a-std::string helper (dice's formatString), reproduced
// TU-locally as RendDX9's nametagFormatString / raFormatString are: _vsnprintf into a 4 KB global
// with a 0xFFF limit, an overflow replaced by the fixed warning, then copied out.
static char s_swiffFormatBuffer[0x1000];
static std::string swiffFormatString(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const int written = ::_vsnprintf(s_swiffFormatBuffer, 0xFFF, format, args);
    va_end(args);
    if (written != -1)
        s_swiffFormatBuffer[written] = '\0';
    else
        ::sprintf(s_swiffFormatBuffer, "%s", "WARNING: The original message was too long for formatString(). \n");
    return std::string(s_swiffFormatBuffer);
}

// sub_1004D640 - dice.bf2.Logic.setDecimals(value, decimals): a formatter despite the
// name, with no host call. Argument 0 is converted and stored as a float (fstp dword - the
// value is rounded to single precision first), argument 1 truncated to int; "%%.%df" of the count
// builds the format, then the float widened back to double is printed through it; the string
// result. endOfRound.swf's slider component labels its value with it (m_value, m_numDecimals).
void as_logic_setDecimals(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setDecimals");
    const float value = static_cast<float>(fn.arg(0).to_number());
    const int decimals = static_cast<int>(fn.arg(1).to_number());
    const std::string format = swiffFormatString("%%.%df", decimals);
    const std::string text = swiffFormatString(format.c_str(), static_cast<double>(value));
    fn.result->set_tu_string(tu_string(text.c_str()));
}

// Returns numeric 1 for serverList, serverListLAN, or favouriteServers, otherwise -1.
// Fetches the host first but performs no flush. Requires a player and argument zero.
void as_general_flushList(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_flushList");
    (void)g_swiffPlayerInstance->getSwiffHost();
    const tu_string name = fn.arg(0).to_tu_string();
    const bool known = name == "serverList" || name == "serverListLAN" || name == "favouriteServers";
    fn.result->set_double(known ? 1.0 : -1.0);
}

// sub_10014B60 / sub_10014B70 - dice.General.setFilter / setSortOrder. The whole
// shipped body: the global player's +0x5C host is fetched and discarded; no argument is read and
// the AS result is never written. endOfRound.swf's list components call both; before these rows
// the calls were gameswf "can't find method" misses with the same undefined result.
void as_general_setFilter(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_setFilter");
    (void) g_swiffPlayerInstance->getSwiffHost();
}
void as_general_setSortOrder(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_setSortOrder");
    (void) g_swiffPlayerInstance->getSwiffHost();
}

// Returns the browser-list revision for serverList, serverListLAN, or favouriteServers;
// other IDs return -1. Requires player, host, argument zero, and a valid requested list.
void as_general_getListRevision(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_getListRevision");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const tu_string id = fn.arg(0).to_tu_string();
    double revision = -1.0;
    if (id == "serverList")
        revision = h->getServerList(std::string("inet"))->getRecords()->revision;
    else if (id == "serverListLAN")
        revision = h->getServerList(std::string("lan"))->getRecords()->revision;
    else if (id == "favouriteServers")
        revision = h->getServerList(std::string("favourites"))->getRecords()->revision;
    fn.result->set_double(revision);
}

// The list result the shipped bodies assemble (a five-word builder: sub_1001C4F0 begin,
// sub_1001C5D0 row, sub_10019970 int field, sub_100198E0 string field, sub_1001CB80 wide column,
// sub_1001CAE0 narrow column, sub_10019B00 totalCount): an array whose element 0 is a header object
// {totalCount: "<n>"} and whose further elements are one array per row - element 0 the row's
// property object ({id: <index>, then the named fields as decimal strings}), the rest the columns
// (a dice String object with wide text, or a narrow string). The movie's dice.lists.List hands the
// whole array to its callback and reads r[0].totalCount.
struct ListResult
{
    ::smart_ptr<gameswf::as_object> arr, header, row, rowObj;   // smart_ptr is base/, not gameswf::
    int rows;
    ListResult() : rows(0) {}
    void begin()
    {
        arr = gameswf::create_array_object();
        header = new gameswf::as_object;
        gameswf::array_object_push(arr.get_ptr(), gameswf::as_value(header.get_ptr()));
        rows = 0;
    }
    void beginRow(int index)
    {
        rowObj = new gameswf::as_object;
        rowObj->set_member("id", gameswf::as_value(static_cast<double>(index)));
        row = gameswf::create_array_object();
        gameswf::array_object_push(row.get_ptr(), gameswf::as_value(rowObj.get_ptr()));
        gameswf::array_object_push(arr.get_ptr(), gameswf::as_value(row.get_ptr()));
        ++rows;
    }
    // sub_1001C770 - the same row, but with a string id. The country list keys its rows by the
    // two-letter code instead of the index, and that id is what the movie hands back as the
    // selected value.
    void beginRowStr(const char* id)
    {
        rowObj = new gameswf::as_object;
        rowObj->set_member("id", gameswf::as_value(tu_string(id ? id : "")));
        row = gameswf::create_array_object();
        gameswf::array_object_push(row.get_ptr(), gameswf::as_value(rowObj.get_ptr()));
        gameswf::array_object_push(arr.get_ptr(), gameswf::as_value(row.get_ptr()));
        ++rows;
    }
    void setInt(const char* key, int v)
    {
        char buf[16]; ::sprintf(buf, "%d", v);
        rowObj->set_member(key, gameswf::as_value(tu_string(buf)));
    }
    void setString(const char* key, const char* v) { rowObj->set_member(key, gameswf::as_value(tu_string(v ? v : ""))); }
    void pushWide(const wchar_t* w, int n)
    {
        gameswf::array_object_push(row.get_ptr(),
            gameswf::as_value(gameswf::create_dice_string_object(reinterpret_cast<const Uint16*>(w), n)));
    }
    void pushNarrow(const char* v) { gameswf::array_object_push(row.get_ptr(), gameswf::as_value(tu_string(v ? v : ""))); }
    // sub_1001CC50 - an int column: "%d" through the shipped 256-byte global (byte_100F78F0), then
    // the same narrow push. The players and serverExtendedInfo lists use it for their numbers.
    void pushInt(int v) { char buf[16]; ::sprintf(buf, "%d", v); pushNarrow(buf); }
    // sub_10019A40 - a named field holding a dice String object: the wide text set, the narrow side
    // empty. The award lists' "name" fields.
    void setWide(const char* key, const wchar_t* w, int n)
    {
        rowObj->set_member(key, gameswf::as_value(
            gameswf::create_dice_string_object(reinterpret_cast<const Uint16*>(w), n)));
    }
    void setTotalCount(int n)
    {
        char buf[16]; ::sprintf(buf, "%d", n);
        header->set_member("totalCount", gameswf::as_value(tu_string(buf)));
    }
};

// The localisation slot shares the owner's native wide string, as Locale.getString does.
typedef std::wstring HostWide;

// sub_10013600 - the localised wide text of a key through SwiffHost +168 (0xA8); an empty key skips
// the host and yields an empty string.
static void hostLocalise(ISwiffHost* h, const char* key, HostWide& out)
{
    if (!h || !key || !key[0]) return;
    h->getLocalizedString(out, renddx9abi::HostString(key));
}


// dice.bf2.EndOfRound - the current-map surface read by endOfRound.swf's loading and briefing
// frames. These eight bodies are the complete PE rows at 100102D0 / 100107E0..10010B9D /
// The previous-round score surface remains separate; a first load deliberately
// takes the different-map path when its previous-map string is empty.


void as_endofround_getMapPath(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapPath");
    const std::string path = g_swiffPlayerInstance->getSwiffHost()->getCurrentMapName();
    fn.result->set_tu_string(tu_string(path.c_str()));
}

void as_endofround_hasMap(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_hasMap");
    const std::string path = g_swiffPlayerInstance->getSwiffHost()->getCurrentMapName();
    fn.result->set_bool(!path.empty());
}

void as_endofround_getMapName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapName");
    const std::string path = g_swiffPlayerInstance->getSwiffHost()->getCurrentMapName();
    IModInfo* mods = static_cast<IModInfo*>(g_swiffPlayerInstance->getSwiffHost()->getModInfo());
    const IModInfo::Map* map = mods->findMapFromCurrentMod(path);
    fn.result->set_tu_string(tu_string(map ? map->name.c_str() : "Unknown Map"));
}

void as_endofround_getMapSize(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapSize");
    const int size = g_swiffPlayerInstance->getSwiffHost()->getCurrentMapSize();
    fn.result->set_double(static_cast<double>(size));
}

void as_endofround_getMapMaxPlayers(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapMaxPlayers");
    const int players = g_swiffPlayerInstance->getSwiffHost()->getCurrentMapMaxPlayers();
    fn.result->set_double(static_cast<double>(players));
}

void as_endofround_getMapDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapDescription");
    ISwiffHost* pathHost = g_swiffPlayerInstance->getSwiffHost();
    const std::string path = pathHost->getCurrentMapName();
    ISwiffHost* modHost = g_swiffPlayerInstance->getSwiffHost();
    IModInfo* mods = static_cast<IModInfo*>(modHost->getModInfo());
    const IModInfo::Map* map = mods->findMapFromCurrentMod(path);
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(0, 0);
    if (map)
    {
        const std::string& key = !map->briefingLocId.empty() ? map->briefingLocId : map->briefing;
        HostWide wide;
        g_swiffPlayerInstance->getSwiffHost()->getLocalizedString(wide, key);
        gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
    }
    fn.result->set_as_object_interface(text);
}

void as_endofround_getMapGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapGameMode");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string mode = h->getCurrentMapGameMode();
    if (::_stricmp(mode.c_str(), "sp2") == 0 || ::_stricmp(mode.c_str(), "sp3") == 0)
    {
        IModInfo* mods = static_cast<IModInfo*>(h->getModInfo());
        mode = mods->getCurrentMod()->gameModes.front();
    }
    fn.result->set_tu_string(tu_string(mode.c_str()));
}

void as_endofround_getMapGameModeDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapGameModeDescription");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string mode = h->getCurrentMapGameMode();
    if (::_stricmp(mode.c_str(), "sp1") == 0 || ::_stricmp(mode.c_str(), "sp2") == 0 ||
        ::_stricmp(mode.c_str(), "sp3") == 0)
        mode = "gpm_cq";
    const int size = h->getCurrentMapSize();
    const std::string path = h->getCurrentMapName();
    IModInfo* mods = static_cast<IModInfo*>(g_swiffPlayerInstance->getSwiffHost()->getModInfo());
    const IModInfo::Map* map = mods->findMapFromCurrentMod(path);
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(0, 0);
    if (map)
    {
        bool found = false;
        for (std::vector<IModInfo::GameMode>::const_iterator gm = map->gameModes.begin();
             gm != map->gameModes.end() && !found; ++gm)
        {
            if (::_stricmp(gm->name.c_str(), mode.c_str()) != 0) continue;
            for (std::vector<IModInfo::MapType>::const_iterator mt = gm->mapTypes.begin(); mt != gm->mapTypes.end(); ++mt)
            {
                if (mt->players != size) continue;
                const std::string& key = !mt->locId.empty() ? mt->locId : mt->description;
                HostWide wide;
                g_swiffPlayerInstance->getSwiffHost()->getLocalizedString(wide, key);
                gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
                found = true;
                break;
            }
        }
    }
    fn.result->set_as_object_interface(text);
}

void as_endofround_getMapGameModeName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMapGameModeName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string mode = h->getCurrentMapGameMode();
    const std::string name = static_cast<IModInfo*>(h->getModInfo())->getNameFromGameMode(mode);
    fn.result->set_tu_string(tu_string(name.c_str()));
}


// Previous-round bindings read the host's end-of-round record and derived summary.
// Host lookup precedes argument conversion; numeric indices truncate and are not range-checked.



// sub_1000FC60 - the other localiser: unlike sub_10013600 (hostLocalise above) it does NOT skip an
// empty key - host +0xA8 is asked whatever the key, into a fresh wide string returned by value.
// getSquadTeamName and getPreviousMapDescription use it.
static void hostLocaliseAlways(const std::string& key, HostWide& out)
{
    std::wstring text;
    (void) g_swiffPlayerInstance->getSwiffHost()->getLocalizedString(text, key);
    out = text;
}

// the one previous-round value read during frame-one routing: the record's first
// string, copied (`std::string(*host->+0x178())`), is the map the last round was played on; a fresh
// record (resetEndOfRoundInfo on every join) yields "", which sends a first load down the
// different-map path.
void as_endofround_getPreviousMapPath(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapPath");
    const std::string path = g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo()->mapName;
    fn.result->set_tu_string(tu_string(path.c_str()));
}

// sub_10010FD0 - getTeamName(team): the host's +0x17C getTeamName (gameLogic's name of team + 1,
// "" without a game logic), as a string.
void as_endofround_getTeamName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getTeamName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int team = (int) fn.arg(0).to_number();
    const std::string name = h->getTeamName(team);                                  // +0x17C
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// sub_100124D0 - getNumVictories(team): a summary on the stack, host +0x1A4 into it (its answer
// ignored), the team's rounds won as a number, the summary destroyed.
void as_endofround_getNumVictories(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getNumVictories");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int team = (int) fn.arg(0).to_number();
    SwiffEndOfRoundSummary summary;
    (void) h->getEndOfRoundSummary(summary);                                        // +0x1A4
    fn.result->set_double((double) summary.teams[team].roundsWon);                  // team unchecked
}

// sub_10011150 - getWinningTeam(): the 0-based winner (record winner - 1) only when the record's
// victory type is 3; any other victory type answers -1. number.
void as_endofround_getWinningTeam(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getWinningTeam");
    const SwiffEndOfRoundInfo* rec = g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo();
    const int victoryType = rec->victoryType;
    const int winner = rec->winner;
    if (victoryType == 3)
        fn.result->set_double((double) (winner - 1));
    else
        fn.result->set_double(-1.0);
}

// sub_10012380 - getTicketHistorySum(team): with a summary from host +0x1A4, the sum over the ticket
// history of that team's pair value (Ticket +0x60 + 32 * team); a false answer from +0x1A4 gives the
// number 0. team unchecked (pair[team]).
void as_endofround_getTicketHistorySum(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getTicketHistorySum");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int team = (int) fn.arg(0).to_number();
    SwiffEndOfRoundSummary summary;
    if (!h->getEndOfRoundSummary(summary))
    {
        fn.result->set_double(0.0);
        return;
    }
    int sum = 0;
    for (unsigned int i = 0; i < summary.ticketHistory.tickets.size(); ++i)         // size re-read per pass
        sum += summary.ticketHistory.tickets[i].pair[team].value;
    fn.result->set_double((double) sum);
}

// sub_10011E20 - getMyTeam(): the 0-based index of the team whose player rows hold the local player
// ("isme", +0x36), scanning team 0 then team 1; -1 when neither does. number.
void as_endofround_getMyTeam(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getMyTeam");
    const SwiffEndOfRoundInfo* rec = g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo();
    for (int t = 0; t < 2; ++t)
        for (unsigned int i = 0; i < rec->teams[t].players.size(); ++i)
            if (rec->teams[t].players[i].isLocalPlayer)
            {
                fn.result->set_double((double) t);
                return;
            }
    fn.result->set_double(-1.0);
}

// sub_10011090 - getPreviousTeamName(team): the record's team name (team 0 or 1) as a string.
void as_endofround_getPreviousTeamName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousTeamName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int team = (int) fn.arg(0).to_number();
    fn.result->set_tu_string(tu_string(h->getEndOfRoundInfo()->teams[team].name.c_str()));   // team unchecked
}

// sub_10010490 - getSquadTeamName(team): a dice String object (created after the host fetch, the
// argument read and the +0x178 fetch, and before the team name is read - 0x100104E8..0x1001050B)
// holding the localised title of that team's best squad - EOR_bestussquad / EOR_bestmecsquad /
// EOR_bestchinesesquad for the team names "US" / "MEC" / "CH" (exact, case-sensitive
// operator==), else "EOR_best" + name + "squad" for any other non-empty name, else nothing (an
// empty wide string). Localised through sub_1000FC60, which does not skip an empty key.
void as_endofround_getSquadTeamName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getSquadTeamName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int team = (int) fn.arg(0).to_number();
    const SwiffEndOfRoundInfo* rec = h->getEndOfRoundInfo();
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(0, 0);
    HostWide title;
    const std::string& name = rec->teams[team].name;                                   // team unchecked
    if (name == "US")
        hostLocaliseAlways(std::string("EOR_bestussquad"), title);
    else if (name == "MEC")
        hostLocaliseAlways(std::string("EOR_bestmecsquad"), title);
    else if (name == "CH")
        hostLocaliseAlways(std::string("EOR_bestchinesesquad"), title);
    else if (name.size() != 0)                                                         // cmp [ebx+0x54], 0: the size word
    {
        const std::string key = std::string("EOR_best") + name + "squad";
        hostLocaliseAlways(std::string(key.c_str()), title);
    }
    gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(title.c_str()));
    fn.result->set_as_object_interface(text);
}

// sub_10010C50 - getPreviousMapName(): the record's map name copied, the current mod's map of that
// path (IModInfo +0x24, through a second host fetch) and its display name; "Unknown Map" without one.
void as_endofround_getPreviousMapName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapName");
    const std::string path(g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo()->mapName);
    IModInfo* mods = static_cast<IModInfo*>(g_swiffPlayerInstance->getSwiffHost()->getModInfo());
    const IModInfo::Map* map = mods->findMapFromCurrentMod(path);
    fn.result->set_tu_string(tu_string(map ? map->name.c_str() : "Unknown Map"));
}

// sub_10010D40 / sub_10010DA0 - getPreviousMapSize() / getPreviousMapMaxPlayers(): the record's
// +0x3C / +0x38 as NUMBERs.
void as_endofround_getPreviousMapSize(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapSize");
    fn.result->set_double((double) g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo()->mapSize);
}

void as_endofround_getPreviousMapMaxPlayers(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapMaxPlayers");
    fn.result->set_double((double) g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo()->maxPlayers);
}

// sub_10010E00 - getPreviousMapDescription(): as getMapDescription, but for the record's map, and
// the dice String object is created after the map lookup; the key is the map's briefing locid when
// it is not "" (operator!=), else its briefing text, localised through sub_1000FC60.
void as_endofround_getPreviousMapDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapDescription");
    const std::string path(g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo()->mapName);
    IModInfo* mods = static_cast<IModInfo*>(g_swiffPlayerInstance->getSwiffHost()->getModInfo());
    const IModInfo::Map* map = mods->findMapFromCurrentMod(path);
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(0, 0);
    if (map)
    {
        HostWide wide;
        hostLocaliseAlways(map->briefingLocId != "" ? map->briefingLocId : map->briefing, wide);
        gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
    }
    fn.result->set_as_object_interface(text);
}

// sub_10011870 - getPreviousMapGameMode(): the record's game mode; "sp1", "sp2" or "sp3"
// (compareIgnoreCase - the current-map twin tests only sp2/sp3) becomes the current mod's first
// game mode (Mod +0x10C's first element). string.
void as_endofround_getPreviousMapGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapGameMode");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string mode(h->getEndOfRoundInfo()->gameMode);
    if (::_stricmp(mode.c_str(), "sp1") == 0 || ::_stricmp(mode.c_str(), "sp2") == 0 ||
        ::_stricmp(mode.c_str(), "sp3") == 0)
    {
        IModInfo* mods = static_cast<IModInfo*>(h->getModInfo());
        mode = mods->getCurrentMod()->gameModes.front();
    }
    fn.result->set_tu_string(tu_string(mode.c_str()));
}

// sub_100119A0 - getPreviousMapGameModeDescription(): as getMapGameModeDescription, for the record:
// its mode ("sp1".."sp3" -> "gpm_cq"), size and map; the first game mode of that map whose name
// matches (compareIgnoreCase) and has a map type of that size gives the localised type locid (when
// not "") or description, through the host's +0xA8 (sub_1000FC60 inlined). A matching mode with no
// matching size continues with the next mode. The dice String is created after the map lookup.
void as_endofround_getPreviousMapGameModeDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapGameModeDescription");
    const SwiffEndOfRoundInfo* rec = g_swiffPlayerInstance->getSwiffHost()->getEndOfRoundInfo();
    std::string mode(rec->gameMode);
    if (::_stricmp(mode.c_str(), "sp1") == 0 || ::_stricmp(mode.c_str(), "sp2") == 0 ||
        ::_stricmp(mode.c_str(), "sp3") == 0)
        mode = "gpm_cq";
    const int size = rec->mapSize;
    const std::string path(rec->mapName);
    IModInfo* mods = static_cast<IModInfo*>(g_swiffPlayerInstance->getSwiffHost()->getModInfo());
    const IModInfo::Map* map = mods->findMapFromCurrentMod(path);
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(0, 0);
    if (map)
    {
        bool found = false;
        for (std::vector<IModInfo::GameMode>::const_iterator gm = map->gameModes.begin();
             gm != map->gameModes.end() && !found; ++gm)
        {
            if (::_stricmp(gm->name.c_str(), mode.c_str()) != 0) continue;
            for (std::vector<IModInfo::MapType>::const_iterator mt = gm->mapTypes.begin(); mt != gm->mapTypes.end(); ++mt)
            {
                if (mt->players != size) continue;
                HostWide wide;
                hostLocaliseAlways(mt->locId != "" ? mt->locId : mt->description, wide);
                gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
                found = true;
                break;
            }
        }
    }
    fn.result->set_as_object_interface(text);
}

// sub_10010F20 - getPreviousMapGameModeName(): the record's game mode through IModInfo +0x4C
// (getNameFromGameMode), on the same host pointer. string.
void as_endofround_getPreviousMapGameModeName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getPreviousMapGameModeName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string mode(h->getEndOfRoundInfo()->gameMode);
    const std::string name = static_cast<IModInfo*>(h->getModInfo())->getNameFromGameMode(mode);
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// sub_100106D0 - getKitNameFromId(id): NO host call at all. 0..6 through the shipped jump table
// (0x100107B8) to antitank / assault / engineer / medic / specops / support / sniper; anything
// else (an unsigned compare against 6, so negatives too) the empty string. string.
void as_endofround_getKitNameFromId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getKitNameFromId");
    const int id = (int) fn.arg(0).to_number();
    const char* name = "";
    switch (id)
    {
    case 0: name = "antitank"; break;
    case 1: name = "assault";  break;
    case 2: name = "engineer"; break;
    case 3: name = "medic";    break;
    case 4: name = "specops";  break;
    case 5: name = "support";  break;
    case 6: name = "sniper";   break;
    default: break;
    }
    fn.result->set_tu_string(tu_string(name));
}

// sub_10010380 - getTeamNameOfPlayerAtPosition(position): the team name the record keeps for top
// player `position` (+0x5C), when that player has a name (operator!= ""); else "". The record is
// asked for twice. string. position unchecked.
void as_endofround_getTeamNameOfPlayerAtPosition(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_endofround_getTeamNameOfPlayerAtPosition");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int position = (int) fn.arg(0).to_number();
    std::string team("");
    if (h->getEndOfRoundInfo()->topPlayers[position].name != "")
        team = h->getEndOfRoundInfo()->topPlayers[position].team;
    fn.result->set_tu_string(tu_string(team.c_str()));
}

// The storage string the shipped prologue fetches into a file global (unk_100F6B88) before any list
// is served: SwiffHost +568("SelectedModFilter", "bf2"). The map/mission branches read it.
static tu_string s_selectedModFilter;

// bfhq localization keys shared by favorite equipment and end-of-round summaries.
static const char* const kFavouriteKitKeys[7] = {
    "BFHQ_antitank", "BFHQ_assault", "BFHQ_engineer", "BFHQ_medic", "BFHQ_specops", "BFHQ_support", "BFHQ_sniper" };
static const char* const kFavouriteVehicleKeys[7] = {
    "BFHQ_armor", "BFHQ_aviator", "BFHQ_airdefence", "BFHQ_helicopter", "BFHQ_transport", "BFHQ_artillery", "BFHQ_grounddefense" };
static const char* const kEndOfRoundAwardKeys[11] = {
    "BFHQ_eor_teamworkscore", "BFHQ_eor_skillscore", "BFHQ_eor_captures", "BFHQ_eor_assists",
    "BFHQ_eor_defends", "BFHQ_eor_killassist", "BFHQ_eor_heal", "BFHQ_eor_revive",
    "BFHQ_eor_replenish", "BFHQ_eor_repairs", "BFHQ_eor_driver" };

// Country choices: four pinned favorites, a disabled separator, then alphabetical entries.
// [CHOSEN, not recovered] Read-only literals replace load-time heap strings.
struct CountryEntry { const char* code; const char* name; };
static const CountryEntry s_countries[] =
{
    { "US", "United States" },
    { "UK", "United Kingdom" },
    { "DE", "Germany" },
    { "SE", "Sweden" },
    { "", "" },
    { "AF", "Afghanistan" },
    { "AL", "Albania" },
    { "DZ", "Algeria" },
    { "AS", "American Samoa" },
    { "AD", "Andorra" },
    { "AO", "Angola" },
    { "AI", "Anguilla" },
    { "AG", "Antigua and Barbuda" },
    { "AR", "Argentina" },
    { "AM", "Armenia" },
    { "AW", "Aruba" },
    { "AU", "Australia" },
    { "AT", "Austria" },
    { "AZ", "Azerbaijan" },
    { "BH", "Bahrain" },
    { "BD", "Bangladesh" },
    { "BB", "Barbados" },
    { "BY", "Belarus" },
    { "BE", "Belgium" },
    { "BZ", "Belize" },
    { "BJ", "Benin" },
    { "BM", "Bermuda" },
    { "BT", "Bhutan" },
    { "BO", "Bolivia" },
    { "BA", "Bosnia-Herzegovina" },
    { "BW", "Botswana" },
    { "BV", "Bouvet Island" },
    { "BR", "Brazil" },
    { "IO", "British Indian Ocean" },
    { "BN", "Brunei" },
    { "BG", "Bulgaria" },
    { "BF", "Burkina Faso" },
    { "BI", "Burundi" },
    { "KH", "Cambodia" },
    { "CM", "Cameroon" },
    { "CA", "Canada" },
    { "CV", "Cape Verde" },
    { "KY", "Cayman Islands" },
    { "CF", "Central African Republic" },
    { "TD", "Chad" },
    { "CL", "Chile" },
    { "CN", "China" },
    { "CX", "Christmas Island" },
    { "CC", "Cocos (Keeling) Islands" },
    { "CO", "Colombia" },
    { "KM", "Comoros" },
    { "CG", "Congo" },
    { "CK", "Cook Islands" },
    { "CR", "Costa Rica" },
    { "HR", "Croatia" },
    { "CU", "Cuba" },
    { "CY", "Cyprus" },
    { "CZ", "Czech Republic" },
    { "DK", "Denmark" },
    { "DJ", "Djibouti" },
    { "DM", "Dominica" },
    { "DO", "Dominican Republic" },
    { "EC", "Ecuador" },
    { "EG", "Egypt" },
    { "SV", "El Salvador" },
    { "GQ", "Equatorial Guinea" },
    { "ER", "Eritrea" },
    { "EE", "Estonia" },
    { "ET", "Ethiopia" },
    { "FK", "Falkland Islands" },
    { "FO", "Faroe Islands" },
    { "FJ", "Fiji" },
    { "FI", "Finland" },
    { "FR", "France" },
    { "GF", "French Guiana" },
    { "PF", "French Polynesia" },
    { "GA", "Gabon" },
    { "GM", "Gambia" },
    { "GE", "Georgia" },
    { "GH", "Ghana" },
    { "GI", "Gibraltar" },
    { "GR", "Greece" },
    { "GL", "Greenland" },
    { "GD", "Grenada" },
    { "GP", "Guadeloupe" },
    { "GU", "Guam" },
    { "GT", "Guatemala" },
    { "GN", "Guinea" },
    { "GW", "Guinea-Bissau" },
    { "GY", "Guyana" },
    { "HT", "Haiti" },
    { "HM", "Heard and McDonald Islands" },
    { "HN", "Honduras" },
    { "HK", "Hong Kong" },
    { "HU", "Hungary" },
    { "IS", "Iceland" },
    { "IN", "India" },
    { "ID", "Indonesia" },
    { "IR", "Iran" },
    { "IQ", "Iraq" },
    { "IE", "Ireland" },
    { "IL", "Israel" },
    { "IT", "Italy" },
    { "JM", "Jamaica" },
    { "JP", "Japan" },
    { "JO", "Jordan" },
    { "KZ", "Kazakhstan" },
    { "KE", "Kenya" },
    { "KI", "Kiribati" },
    { "KW", "Kuwait" },
    { "KG", "Kyrgyzstan" },
    { "LA", "Laos" },
    { "LV", "Latvia" },
    { "LB", "Lebanon" },
    { "LS", "Lesotho" },
    { "LR", "Liberia" },
    { "LY", "Libya" },
    { "LI", "Liechtenstein" },
    { "LT", "Lithuania" },
    { "LU", "Luxembourg" },
    { "MO", "Macau" },
    { "MK", "Macedonia" },
    { "MG", "Madagascar" },
    { "MW", "Malawi" },
    { "MY", "Malaysia" },
    { "MV", "Maldives" },
    { "ML", "Mali" },
    { "MT", "Malta" },
    { "MH", "Marshall Islands" },
    { "MQ", "Martinique" },
    { "MR", "Mauritania" },
    { "MU", "Mauritius" },
    { "YT", "Mayotte" },
    { "MX", "Mexico" },
    { "FM", "Micronesia" },
    { "MD", "Moldova" },
    { "MC", "Monaco" },
    { "MN", "Mongolia" },
    { "MS", "Montserrat" },
    { "MA", "Morocco" },
    { "MZ", "Mozambique" },
    { "MM", "Myanmar" },
    { "NA", "Namibia" },
    { "NR", "Nauru" },
    { "NP", "Nepal" },
    { "AN", "Netherlands Antilles" },
    { "NL", "Netherlands" },
    { "NC", "New Caledonia" },
    { "NZ", "New Zealand" },
    { "NI", "Nicaragua" },
    { "NE", "Niger" },
    { "NG", "Nigeria" },
    { "NU", "Niue" },
    { "NF", "Norfolk Island" },
    { "KP", "North Korea" },
    { "MP", "Northern Mariana Islands" },
    { "NO", "Norway" },
    { "OM", "Oman" },
    { "PK", "Pakistan" },
    { "PW", "Palau" },
    { "PA", "Panama" },
    { "PG", "Papua New Guinea" },
    { "PY", "Paraguay" },
    { "PE", "Peru" },
    { "PH", "Philippines" },
    { "PN", "Pitcairn Islands" },
    { "PL", "Poland" },
    { "PT", "Portugal" },
    { "PR", "Puerto Rico" },
    { "QA", "Qatar" },
    { "RE", "Reunion" },
    { "RO", "Romania" },
    { "RU", "Russia" },
    { "RW", "Rwanda" },
    { "GS", "S. Georgia" },
    { "EH", "Samoa" },
    { "SM", "San Marino" },
    { "ST", "Sao Tome and Principe" },
    { "SA", "Saudi Arabia" },
    { "SN", "Senegal" },
    { "SC", "Seychelles" },
    { "SL", "Sierra Leone" },
    { "SG", "Singapore" },
    { "SK", "Slovak Republic" },
    { "SI", "Slovenia" },
    { "SB", "Solomon Islands" },
    { "SO", "Somalia" },
    { "ZA", "South Africa" },
    { "KR", "South Korea" },
    { "ES", "Spain" },
    { "LK", "Sri Lanka" },
    { "KN", "St Kitts-Nevis" },
    { "LC", "St Lucia" },
    { "SH", "St. Helena" },
    { "PM", "St. Pierre and Miquelon" },
    { "SD", "Sudan" },
    { "SR", "Suriname" },
    { "SJ", "Svalbard and Jan Mayen Islands" },
    { "SZ", "Swaziland" },
    { "CH", "Switzerland" },
    { "SY", "Syria" },
    { "TW", "Taiwan" },
    { "TJ", "Tajikistan" },
    { "TZ", "Tanzania" },
    { "TH", "Thailand" },
    { "TP", "Timor" },
    { "TG", "Togo" },
    { "TO", "Tonga" },
    { "TT", "Trinidad & Tobago" },
    { "TN", "Tunisia" },
    { "TR", "Turkey" },
    { "TM", "Turkmenistan" },
    { "TC", "Turks & Caicos Islands" },
    { "TV", "Tuvalu" },
    { "UM", "US Minor Outlying Islands" },
    { "UG", "Uganda" },
    { "UA", "Ukraine" },
    { "AE", "United Arab Emirates" },
    { "UY", "Uruguay" },
    { "UZ", "Uzbekistan" },
    { "VU", "Vanuatu" },
    { "VA", "Holy See (Vatican City)" },
    { "VE", "Venezuela" },
    { "VN", "Vietnam" },
    { "VG", "Virgin Islands (British)" },
    { "VI", "Virgin Islands (US)" },
    { "WF", "Wallis and Futuna Islands" },
    { "YE", "Yemen" },
    { "YU", "Yugoslavia" },
    { "ZM", "Zambia" },
    { "ZR", "Zaire" },
    { "ZW", "Zimbabwe" },
};
static const int kNumCountries = (int) (sizeof(s_countries) / sizeof(s_countries[0]));   // 0xE8 = 232

// Windowed list bindings use count -1 for all remaining rows. Unsupported list branches
// report a named frontier and return undefined. [RETIRE-WHEN: RW-88]
// Browser record helpers read the host's native containers.
static int serverRecordCount(const ServerBrowserRecordList* recs)
{
    return static_cast<int>(recs->records.size());
}
// The OS column, three 64-bit names to one icon, two unix names to another, and
// everything else - "win32" included, by falling through rather than by being tested - to Windows.
static const char* serverOsIcon(const char* os)
{
    if (::strcmp(os, "freebsd-64") == 0 || ::strcmp(os, "linux-64") == 0 || ::strcmp(os, "win32-64") == 0)
        return "images/serverIcons/64bit.png";
    if (::strcmp(os, "freebsd") == 0 || ::strcmp(os, "linux") == 0)
        return "images/serverIcons/linux.png";
    return "images/serverIcons/Windows.png";
}


// Selected-server caches own deep copies keyed by address/query port. List and sponsor
// queries use separate caches; sponsors may issue a query on a cache miss. Joining clears
// the separate in-game server-info keys.

extern "C" __declspec(dllimport) void* __stdcall SetCursor(void* cursor);
typedef ServerBrowserRecord CachedRecord;
struct RecordCache
{
    CachedRecord           record;
    renddx9abi::HostString address;
    int                    port;
    RecordCache() : port(0) {}
};
static RecordCache s_selectedCache;                     // unk_100F6DE0 / unk_100F7024 / dword_100F323C
static RecordCache s_sponsorCache;                      // dword_100F8578 / unk_100F87F4 / dword_100F8570
static renddx9abi::HostString s_ingameAddressA;         // unk_100F7040   sub_100149F0's key, cleared by joinServer
static int s_ingamePortA = 0;                           // dword_100F3240
static CachedRecord s_ingameRecordA;                    // unk_100F3510   sub_100149F0's 580-byte deep-copy cache
static renddx9abi::HostString s_ingameAddressB;         // unk_100F8B14   sub_100627F0's key, cleared by joinServer
static int s_ingamePortB = 0;                           // dword_100F88CC
static CachedRecord s_ingameRecordB;                    // unk_100F88D0   sub_100627F0's 580-byte deep-copy cache
static renddx9abi::HostString s_eorHudAddress;           // unk_100F8B30   sub_100626F0's address publication
static int s_eorHudPort = 0;                            // dword_100F88C8 sub_100626F0's query-port publication
static renddx9abi::HostString s_teamNames[2];           // unk_100F87BC / unk_100F87D8: Multiplay.getTeamName's table
static bool sameAddress(const renddx9abi::HostString& a, const renddx9abi::HostString& b)
{
    return ::strcmp(a.c_str(), b.c_str()) == 0;         // std::operator!= / == on the shipped side
}

// sub_10013520 / sub_10013580 - the registry SwiffHost's busy cursor around a fetch: the singleton
// looked up by name and, when present, cursor 1 and setBusy(1) on open, cursor 0 and clearBusy on
// close. A missing singleton skips both, exactly as the shipped bodies do.
typedef ISwiffHost* BusyHost;
static BusyHost busyOpen()
{
    ISwiffHost* host = static_cast<ISwiffHost*>(classManager->getSingleton(renddx9abi::HostString("SwiffHost")));
    if (host) { SetCursor(host->getCursor(1)); host->setBusy(1); }
    return host;
}
static void busyClose(BusyHost host) { if (host) { SetCursor(host->getCursor(0)); host->clearBusy(); } }

// sub_10014910 (port in EBX): the players / server-info fetch. The current list is fetched first
// and unconditionally (+0x134, +0x7C with the returned kind pointer passed on); then a zero port
// refuses, then the "0.0.0.0" address; a cache hit (same address, same port, and `full`) copies the
// cached record out; otherwise getRecord(address, port, true, false) - a miss refuses - and the copy
// is made both ways: into the caller's record, and from it into the cache.
static bool fetchSelectedRecord(int port, const renddx9abi::HostString& address, CachedRecord& out, bool full)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list = h->getServerList(h->getServerListKind());
    if (!port)
        return false;
    if (address == "0.0.0.0")
        return false;
    if (sameAddress(s_selectedCache.address, address) && s_selectedCache.port == port && full)
    {
        out = s_selectedCache.record;
        return true;
    }
    ServerBrowserRecord* found = list->getRecord(address, port, true, false);      // +0x3C
    if (!found)
        return false;
    out = *found;
    s_selectedCache.address = address;
    s_selectedCache.port = port;
    s_selectedCache.record = out;
    return true;
}

// sub_100509A0 (port in EDI): the sponsor rows' fetch, inside the registry SwiffHost's busy cursor.
// The current list first; a zero port refuses (the cursor restored); a cache hit copies the cached
// record out; otherwise getRecord(address, port, true, true) - found: the copy both ways, member by
// member; not found: queryAddress(address, port, false) and a second getRecord, copied both ways
// only when that one answers. The address is NOT tested against "0.0.0.0" here, and a miss still
// answers true with `out` untouched - the empty record's fields.
static bool fetchSponsorRecord(int port, const renddx9abi::HostString& address, CachedRecord& out, bool full)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list = h->getServerList(h->getServerListKind());
    BusyHost busy = busyOpen();
    if (!port)
    {
        busyClose(busy);
        return false;
    }
    if (sameAddress(s_sponsorCache.address, address) && s_sponsorCache.port == port && full)
        out = s_sponsorCache.record;
    else
    {
        ServerBrowserRecord* found = list->getRecord(address, port, true, true);    // +0x3C
        if (found)
        {
            out.server = found->server;
            out.entry = found->entry;
            out.details = found->details;
            s_sponsorCache.address = address;
            s_sponsorCache.port = port;
            s_sponsorCache.record.server = out.server;
            s_sponsorCache.record.entry = out.entry;
            s_sponsorCache.record.details = out.details;
        }
        else
        {
            list->queryAddress(address, port, false);                              // +0x48
            found = list->getRecord(address, port, true, true);
            if (found)
            {
                out = *found;
                s_sponsorCache.address = address;
                s_sponsorCache.port = port;
                s_sponsorCache.record = out;
            }
        }
    }
    busyClose(busy);
    return true;
}

// the in-game record used by General.getListEntries("eorsinfo 0/1").
// This is deliberately NOT the address-only cache below: remote calls always reset the named
// serverInfo browser, issue a direct query, then ask for a full cached record. A successful answer
// is copied both to the caller and to the A globals; a refusal leaves the caller's default record
// untouched. The local-server arm asks +0x44 to fill the caller and returns without updating A.
static void fetchEorServerRecord(CachedRecord& out)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string address = h->getJoinAddress();                              // +0x258
    const int port = h->getIntE58();                                              // +0x260
    IServerBrowser* list = h->getServerList(std::string("serverInfo"));            // +0x7C

    if (h->isGameLogicServer())                                                   // +0x1A8
    {
        (void) list->fillLocalRecord(&out);                                       // +0x44, answer ignored
        return;
    }
    if (address == "0.0.0.0") return;
    if (port == 0) return;
    if (address.empty()) return;

    list->reset();                                                               // +0x24
    (void) list->queryAddress(address, port, false);                              // +0x48
    ServerBrowserRecord* found = list->getRecord(address, port, true, true);     // +0x3C
    if (!found) return;
    out = *found;
    s_ingameAddressA = address;
    s_ingamePortA = port;
    s_ingameRecordA = out;
}

// PE 100626F0 and the eorsinfo caller at 1002F767..1002F7DC. The helper first retains the
// record's address/query port in file globals. The caller then looks up HudInformationLayer and,
// only when present, invokes +0x8C setServerInfo(hostName, address, queryPort).
static void publishEorHudServerInfo(const CachedRecord& record)
{
    s_eorHudAddress = record.entry.address;
    s_eorHudPort = record.entry.queryPort;
    IBase* base = classManager->getSingleton(renddx9abi::HostString("HudInformationLayer"));
    if (!base) return;
    static_cast<HudInformationLayer*>(base)->setServerInfo(
        record.entry.hostName, record.entry.address, record.entry.queryPort);
}

// the in-game record used by dice.bf2.ServerInfo. The helper has no success return:
// its caller supplies an already-default record and a refusal leaves it untouched. Preserve the
// shipped address-only cache defect: s_ingamePortB is stored on a fill but is never consulted when
// deciding whether the cached record matches a later request.
static void fetchInGameServerRecord(CachedRecord& out)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string address = h->getJoinAddress();                              // +0x258
    const int port = h->getIntE58();                                              // +0x260
    IServerBrowser* list = h->getServerList(std::string("serverInfo"));            // +0x7C

    if (h->isGameLogicServer())                                                   // +0x1A8
    {
        (void) list->fillLocalRecord(&out);                                       // +0x44, answer ignored
        return;
    }
    if (address == "0.0.0.0") return;
    if (port == 0) return;
    if (address.empty()) return;

    if (!sameAddress(s_ingameAddressB, address))
    {
        list->reset();                                                            // +0x24
        (void) list->queryAddress(address, port, false);                           // +0x48
        ServerBrowserRecord* found = list->getRecord(address, port, true, true);  // +0x3C
        if (!found) return;
        out = *found;
        s_ingameAddressB = address;
        s_ingamePortB = port;
        s_ingameRecordB = out;
    }
    else
        out = s_ingameRecordB;
}

// the three ServerInfo rows the briefing frame flushes.
// Missing browser data does not invent a nicer presentation: the default record yields an empty
// logo and the shipped server-name spelling " (:0)". Welcome-message pipes become newlines.
void as_serverinfo_getWelcomeMessage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getWelcomeMessage");
    std::string text = *g_swiffPlayerInstance->getSwiffHost()->getServerNameString();
    for (std::string::size_type i = 0; i < text.size(); ++i)
        if (text[i] == '|') text[i] = '\n';
    fn.result->set_tu_string(tu_string(text.c_str()));
}

void as_serverinfo_getCommunityLogo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getCommunityLogo");
    CachedRecord record;
    fetchInGameServerRecord(record);
    fn.result->set_tu_string(tu_string(record.details.communityLogoUrl.c_str()));
}

// dice.bf2.ServerInfo.getSponsorLogo(): getCommunityLogo's body one
// string earlier - a default record through the in-game fetch, then its sponsor logo url (record
// +472 / details +188). A refused fetch answers the default record's "".
void as_serverinfo_getSponsorLogo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getSponsorLogo");
    CachedRecord record;
    fetchInGameServerRecord(record);
    fn.result->set_tu_string(tu_string(record.details.sponsorLogoUrl.c_str()));
}

void as_serverinfo_getServerName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serverinfo_getServerName");
    CachedRecord record;
    fetchInGameServerRecord(record);
    char port[16];
    ::sprintf(port, "%d", record.entry.hostPort);
    std::string text = record.entry.hostName;
    text += " (";
    text += record.entry.address;
    text += ":";
    text += port;
    text += ")";
    fn.result->set_tu_string(tu_string(text.c_str()));
}

// sub_10050980: the record's two team names into the table Multiplay.getTeamName reads.
static void publishTeamNames(const CachedRecord& record)
{
    s_teamNames[0] = record.details.team1;
    s_teamNames[1] = record.details.team2;
}

// Caches the selected server's full record inside the host busy-cursor bracket and
// publishes team names on success. Requires player/host; writes no AS result.
void as_general_cacheExtendedServerInfo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_cacheExtendedServerInfo");
    (void) fn;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    BusyHost busy = busyOpen();
    {
        std::string address;
        int queryPort = 0, hostPort = 0;
        h->getSelectedServer(address, &queryPort, &hostPort);            // +0x110
        CachedRecord record;
        if (fetchSelectedRecord(queryPort, address, record, true))
            publishTeamNames(record);
    }
    busyClose(busy);
}


// Resolves the VideoSettings singleton once per call.
static IBase* videoSettingsSingleton()
{
    return classManager->getSingleton(renddx9abi::HostString("VideoSettings"));
}

static IVideoSettings* videoSettings()
{
    return static_cast<IVideoSettings*>(videoSettingsSingleton());
}
typedef bool                     (IVideoSettings::*VsBoolGetter)();
typedef int                      (IVideoSettings::*VsIntGetter)();
typedef void                     (IVideoSettings::*VsIntSetter)(int);
typedef std::string              (IVideoSettings::*VsStringGetter)();
typedef std::vector<std::string> (IVideoSettings::*VsNameList)();
// Rows call typed native members; offset arguments retain the original slot reference.
#define VS_SLOT(member, offset) (&IVideoSettings::member)

// Creates a managed dice String from a localized video-settings key.
// Empty keys produce an empty narrow string; other keys use the host's wide-string result.
static gameswf::as_object_interface* videoSettingsLocalisedString(const renddx9abi::HostString& key)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    HostWide wide;
    gameswf::as_object_interface* text;
    if (key.empty())
    {
        text = gameswf::create_dice_string_object(nullptr, 0);
        gameswf::assign_dice_string_object_narrow(text, key.c_str());
    }
    else
    {
        (void) h->getLocalizedString(wide, key);                                       // +0xA8
        text = gameswf::create_dice_string_object(nullptr, 0);
        gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
    }
    return text;
}

// The eight plain int getters (0x1006B6E0 and its seven siblings, 126 B each) and the two scheme getters
// (0x1006B550 detectVideoScheme, 0x1006B5D0 getVideoOptionScheme, 126 / 129 B): one lookup, one
// slot, its int published as a number through fild - exact, every int fits a double.
static void publishVideoInt(const gameswf::fn_call& fn, VsIntGetter getter)
{
    const int value = (videoSettings()->*getter)();
    fn.result->set_double(static_cast<double>(value));
}

// canApply / needsRestart / needsReboot (129 B each): one lookup, the slot's AL through movzx,
// published as a boolean. Only AL is read, so a dirty upper byte never leaks; the shipped result
// carries the byte in the numeric slot, this port's as_value a separate Boolean - the difference
// every promoted Boolean row in this file already has.
static void publishVideoBool(const gameswf::fn_call& fn, VsBoolGetter getter)
{
    const bool value = (videoSettings()->*getter)();
    fn.result->set_bool(value);
}

// getResolution / getAntialiasing / getSelectedResolution (155 B each): one lookup, the slot's
// std::string (callee-constructed hidden buffer), its c_str() published as a string - the shipped
// temporary also carries dbl_100D79C8 (0.0) in its number slot, dead weight for a string - and the
// string destroyed after the publication.
static void publishVideoString(const gameswf::fn_call& fn, VsStringGetter getter)
{
    const std::string value = (videoSettings()->*getter)();
    fn.result->set_tu_string(tu_string(value.c_str()));
}

// The tail every quality setter and setResolution share: the owner looked up again and its +0x34
// setCustomVideoOptionScheme (PE 0x4B2160: pending scheme = 3, "Custom").
static void markCustomVideoScheme()
{
    videoSettings()->setCustomVideoOptionScheme();                                     // +0x34
}

// order [A] - terrain, geometry, lighting, effects (0x1006AC60 / 0x1006AD10 / 0x1006ADC0 /
// 0x1006AFD0, 164 B each): the owner is looked up - and its vtable read, `mov edi, [esi]` - before
// argument zero is coerced; (int) truncates (fistp under a truncating control word); then the
// custom-scheme tail. No result.
static void setVideoIntOwnerFirst(const gameswf::fn_call& fn, VsIntSetter setter)
{
    IVideoSettings* vs = videoSettings();
    const int value = static_cast<int>(fn.arg(0).to_number());
    (vs->*setter)(value);
    markCustomVideoScheme();
}

// order [B] - dynamic lighting, dynamic shadows, texture, texture filtering (0x1006AE70 /
// 0x1006AF20 / 0x1006B080 / 0x1006B260, 162 B each): argument zero is coerced and truncated first,
// then the owner is looked up; then the same tail. The two orders are the shipped bodies' own and are
// kept per row. No result.
static void setVideoIntArgumentFirst(const gameswf::fn_call& fn, VsIntSetter setter)
{
    const int value = static_cast<int>(fn.arg(0).to_number());
    (videoSettings()->*setter)(value);
    markCustomVideoScheme();
}

// Publishes the localized name for the pending quality/scheme index without bounds checks.
// Integer offset arithmetic preserves wrap behavior, including NaN-to-int mapping to name zero.
static void publishSelectedName(const gameswf::fn_call& fn, VsIntGetter getIndex, VsNameList getNames)
{
    const int index = (videoSettings()->*getIndex)();
    const std::string name = (videoSettings()->*getNames)()[index];                   // unchecked
    const std::string key = "VIDEOSETTINGS_" + name;
    fn.result->set_as_object_interface(videoSettingsLocalisedString(key));
}

// Builds paged option rows for videoSettings list IDs 0..11.
// VideoSettings must exist. Window comparisons are unsigned, so negative starts emit no rows.
// Counts include all options; resolution windows count emitted rows. Overall-quality lists omit
// Custom, and the LCD-display flag is queried without filtering the resolution list.
typedef std::string              VsName;
typedef std::vector<std::string> VsNames;
static unsigned int  vsNameCount(const VsNames& v)                 { return static_cast<unsigned int>(v.size()); }
static const VsName& vsNameAt(const VsNames& v, unsigned int i)    { return v[i]; }                // unchecked
static bool          vsNameEquals(const VsName& a, const VsName& b) { return a == b; }

// One localised column: key = "VIDEOSETTINGS_" + name (sub_10001A20, operator+(const char*, const
// string&)), its wide text through sub_10013600 - hostLocalise above, which in the shipped helper
// fetches the player's host again on every call and skips +0xA8 for an empty key - pushed as a wide
// column (sub_1001CB80); the text, then the key, destroyed.
static void pushVideoSettingsName(ListResult& r, const VsName& name)
{
    const std::string key = "VIDEOSETTINGS_" + name;
    HostWide text;
    hostLocalise(g_swiffPlayerInstance->getSwiffHost(), key.c_str(), text);
    r.pushWide(text.c_str(), static_cast<int>(text.size()));
}

// Lists quality levels within the setting's min/max range and requested row window.
// Rows above the hardware cap are disabled (value 2); total counts eligible levels.
static void listVideoQualityLevels(ListResult& r, IBase* vsBase, const VsNames& names,
                                   VsIntGetter minSlot, VsIntGetter maxSlot, int capIndex,
                                   VsIntGetter curSlot, int start, int end, unsigned int& total)
{
    IVideoSettings* vs = static_cast<IVideoSettings*>(vsBase);
    const unsigned int minimum = static_cast<unsigned int>((vs->*minSlot)());
    const unsigned int maximum = static_cast<unsigned int>((vs->*maxSlot)());
    const unsigned int cap     = static_cast<unsigned int>(vs->getOptionMaxQuality(capIndex));       // +0xE4
    const unsigned int current = static_cast<unsigned int>((vs->*curSlot)());
    for (unsigned int i = 0; i < 5; ++i)
    {
        if (i < minimum || i > maximum)
            continue;
        if (i >= static_cast<unsigned int>(start) && i < static_cast<unsigned int>(end))
        {
            r.beginRow(static_cast<int>(i));
            pushVideoSettingsName(r, vsNameAt(names, i));
            if (i == current)
                r.setInt("selected", 1);
            if (i > cap)
                r.setInt("disabled", 2);
        }
        ++total;
    }
}

// The arm itself (see the block comment above). generalSettings is the prologue's kept lookup.
static void listVideoSettings(const gameswf::fn_call& fn, ListResult& r, const char* whichToken,
                              int start, int end, IBase* generalSettings)
{
    IBase* const vsBase = videoSettingsSingleton();
    const int which = ::atoi(whichToken);
    r.begin();
    unsigned int total = 0;
    IVideoSettings* const vs = static_cast<IVideoSettings*>(vsBase);
    std::vector<std::string> names;
    names = vs->getQualityLevelNames();                                                    // +0xF0
    switch (which)
    {
    case 0:
    {
        const std::vector<std::string> modes = vs->getResolutionList();                   // +0xE8
        const std::string current = vs->getResolution();                                  // +0x80
        // +0x15C, the answer discarded (0x10027A70 overwrites AL). The volatile local keeps the
        // singleton dereference an optimiser could otherwise drop - a null GeneralSettings faults here
        // as in retail (0x10027A65..6A).
        const volatile bool lcdDisplayModesDiscarded =
            static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(generalSettings))->getLCDDisplayModes();
        (void) lcdDisplayModesDiscarded;
        const unsigned int budget = static_cast<unsigned int>(end) - static_cast<unsigned int>(start);
        unsigned int rows = 0;
        for (unsigned int i = 0; i < vsNameCount(modes); ++i, ++total)
        {
            if (i >= static_cast<unsigned int>(start) && rows < budget)
            {
                const VsName mode(vsNameAt(modes, i));
                r.beginRowStr(mode.c_str());
                r.pushNarrow(mode.c_str());
                if (vsNameEquals(mode, current))
                    r.setInt("selected", 1);
                ++rows;
            }
        }
        break;
    }
    case 1:
        total = 4;
        for (unsigned int i = 1; i < 5; ++i)
        {
            if (i >= static_cast<unsigned int>(start) && i < static_cast<unsigned int>(end))
            {
                r.beginRow(static_cast<int>(i));
                pushVideoSettingsName(r, vsNameAt(names, i));
            }
        }
        break;
    case 2:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getTerrainQualityMin, 0x98),          VS_SLOT(getTerrainQualityMax, 0x9C),          0, VS_SLOT(getTerrainQuality, 0x40),          start, end, total); break;
    case 3:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getEffectsQualityMin, 0xC0),          VS_SLOT(getEffectsQualityMax, 0xC4),          5, VS_SLOT(getEffectsQuality, 0x68),          start, end, total); break;
    case 4:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getGeometryQualityMin, 0xA0),         VS_SLOT(getGeometryQualityMax, 0xA4),         1, VS_SLOT(getGeometryQuality, 0x48),         start, end, total); break;
    case 5:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getTextureQualityMin, 0xC8),          VS_SLOT(getTextureQualityMax, 0xCC),          6, VS_SLOT(getTextureQuality, 0x70),          start, end, total); break;
    case 6:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getLightingQualityMin, 0xA8),         VS_SLOT(getLightingQualityMax, 0xAC),         2, VS_SLOT(getLightingQuality, 0x50),         start, end, total); break;
    case 7:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getDynamicShadowsQualityMin, 0xB8),   VS_SLOT(getDynamicShadowsQualityMax, 0xBC),   4, VS_SLOT(getDynamicShadowsQuality, 0x60),   start, end, total); break;
    case 8:  listVideoQualityLevels(r, vsBase, names, VS_SLOT(getDynamicLightingQualityMin, 0xB0),  VS_SLOT(getDynamicLightingQualityMax, 0xB4),  3, VS_SLOT(getDynamicLightingQuality, 0x58),  start, end, total); break;
    case 9:
    {
        const std::vector<std::string> schemes = vs->getVideoOptionSchemeNames();          // +0xF4
        const int current = vs->getVideoOptionScheme();                                     // +0xD8
        for (unsigned int i = 0; i < 3; ++i, ++total)
        {
            if (total >= static_cast<unsigned int>(start) && total < static_cast<unsigned int>(end))
            {
                r.beginRow(static_cast<int>(i));
                pushVideoSettingsName(r, vsNameAt(schemes, i));
                if (i == static_cast<unsigned int>(current))
                    r.setInt("selected", 1);
            }
        }
        break;
    }
    case 10:
    {
        const std::vector<std::string> levels = vs->getAntialiasingList();                 // +0xEC
        const std::string current = vs->getAntialiasing();                                // +0x88
        for (unsigned int i = 0; i < vsNameCount(levels); ++i, ++total)
        {
            if (i >= static_cast<unsigned int>(start) && i < static_cast<unsigned int>(end))
            {
                r.beginRowStr(vsNameAt(levels, i).c_str());
                pushVideoSettingsName(r, vsNameAt(levels, i));
                if (vsNameEquals(vsNameAt(levels, i), current))
                    r.setInt("selected", 1);
            }
        }
        break;
    }
    case 11: listVideoQualityLevels(r, vsBase, names, VS_SLOT(getTextureFilteringQualityMin, 0xD0), VS_SLOT(getTextureFilteringQualityMax, 0xD4), 7, VS_SLOT(getTextureFilteringQuality, 0x78), start, end, total); break;
    default:
        break;
    }
    r.setTotalCount(static_cast<int>(total));
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// Builds audio provider or quality rows with localized names and current selection.
// Quality entries beyond the supported count are disabled; other selectors return no rows.
static void pushAudioSettingsName(ListResult& r, const VsName& name)
{
    const std::string key = "AUDIOSETTINGS_" + name;
    HostWide text;
    hostLocalise(g_swiffPlayerInstance->getSwiffHost(), key.c_str(), text);
    r.pushWide(text.c_str(), static_cast<int>(text.size()));
}
static void listAudioSettings(const gameswf::fn_call& fn, ListResult& r, const char* whichToken, int start, int end)
{
    IBase* const asBase = classManager->getSingleton(renddx9abi::HostString("AudioSettings"));
    const int which = ::atoi(whichToken);
    r.begin();
    unsigned int total = 0;
    if (which == 1 || which == 4)
    {
        int count = 0;
        IAudioSettings* const as = static_cast<IAudioSettings*>(asBase);
        const std::vector<std::string> names = which == 1 ? as->getProviderNames(&count)       // +0xA0
                                                          : as->getSoundQualityNames(&count);  // +0xA4
        const std::string current = which == 1 ? as->getProvider() : as->getSoundQuality();     // +0x68 / +0x70
        const unsigned int rows = which == 1 ? static_cast<unsigned int>(count) : vsNameCount(names);
        for (unsigned int i = 0; i < rows; ++i, ++total)
        {
            if (i >= static_cast<unsigned int>(start) && i < static_cast<unsigned int>(end))
            {
                r.beginRowStr(vsNameAt(names, i).c_str());
                pushAudioSettingsName(r, vsNameAt(names, i));
                if (which == 4)
                {
                    if (i < static_cast<unsigned int>(count))
                        r.setInt("enabled", 1);
                    else
                        r.setInt("disabled", 2);
                }
                if (vsNameEquals(vsNameAt(names, i), current))
                    r.setInt("selected", 1);
            }
        }
    }
    r.setTotalCount(static_cast<int>(total));
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// The bfhq list branches of sub_10022610, defined after the PlayerStats helpers they
// share (the stats record image, the unlock record, the rank keys, the favourite key tables).
typedef ISwiffHost* ListHost;
static void listPlayerStats(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                            int start, int end);
static void listWeaponStats(const gameswf::fn_call& fn, ListResult& r, const tu_string& which);
static void listProfileSelection(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listTeams(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listKits(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                     const char* word2, int start, int end);
static void listEquipments(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                           const char* word2, int start, int end);
static void listEquipSlots(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                           const char* word2);
static void listUnlockEquipment(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                                const char* word2, int start, int end);
// The bfhq awards and leaderboards, defined with their rows in the phase 4 block.
static void listLatestAwards(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listMedals(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listBadges(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listRibbons(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listLeaderboardCategories(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listLeaderboard(const gameswf::fn_call& fn, ListResult& r, ListHost h);
static void listLeaderboardPlayers(const gameswf::fn_call& fn, ListResult& r, ListHost h);
static void listLeaderboardTop(const gameswf::fn_call& fn, ListResult& r, ListHost h);
static void listMapList(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listGameModes(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end, bool withAll);
static void listDemos(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);
static void listDemoBookmarks(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end);

void as_general_getListEntries(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_getListEntries");
    // The prologue is read from 10022610..10022824. It fetches the host through the global player's
    // +0x5C and dereferences it, and it dereferences the class manager, without checking either;
    // both singleton lookups happen before any argument is touched, and the GeneralSettings result
    // is kept while the LocalProfileManager one is discarded here and refetched per branch.
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    // [ebp-30h]: kept for the "videoSettings" arm, which calls its +0x15C (and discards the answer).
    IBase* const prologueGeneralSettings = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    {
        // Host +0x238 with a hidden return buffer, the getStorageString convention; the shipped
        // body assigns the answer into a file-global std::string before any list is served.
        renddx9abi::HostString ka("SelectedModFilter"), kb("bf2");
        s_selectedModFilter = h->getStorageString(ka, kb).c_str();
    }
    // Arguments zero, one and two are read through the trapping accessor (int 3 at 10022718 and
    // 10022823), so no argument-count test is added. The shipped split is
    // sscanf(id, "%s %s %s %s") into four unbounded 256-byte globals; the loop below is the same
    // whitespace split without that overflow, which is a [CHOSEN] narrowing of a shipped defect.
    const tu_string id = fn.arg(0).to_tu_string();
    tu_string tok[4];
    {
        int t = 0; const char* c = id.c_str();
        while (*c && t < 4)
        {
            while (*c == ' ') ++c;
            if (!*c) break;
            const char* b = c;
            while (*c && *c != ' ') ++c;
            tok[t++] = tu_string(b, (int)(c - b));
        }
    }
    const int start = (int) fn.arg(1).to_number();
    const int count = (int) fn.arg(2).to_number();
    const int end   = (count == -1) ? start + 99999 : start + count;

    ListResult r;
    if (tok[0] == "localProfiles")
    {
        IBase* lpm = classManager ? classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")) : 0;
        IBase* gs  = classManager ? classManager->getSingleton(renddx9abi::HostString("GlobalSettings")) : 0;
        r.begin();
        // ILocalProfileManager::getLocalProfiles is the same +0x48 slot the raw path below calls,
        // and the native owner fills this toolchain's vector of records.
        std::vector<LocalProfile> profiles;
        if (lpm)
            (void) static_cast<ILocalProfileManager*>(lpm)->getLocalProfiles(profiles, true);
        const int n = static_cast<int>(profiles.size());
        int total = 0;
        for (int i = 0; i < n; ++i, ++total)
        {
            if (i < start || i >= end) continue;
            const LocalProfile& rec = profiles[i];
            const char* recId       = rec.name.c_str();
            const char* recProfile  = rec.profileName.c_str();
            const char* recPassword = rec.password.c_str();
            const char* recOffline  = rec.nick.c_str();
            const char* recGamespy  = rec.gamespyNick.c_str();
            const bool offline = recGamespy[0] == 0;
            r.beginRow(i);
            r.setInt("gamespy", offline ? 0 : 1);
            r.setString("profileName", recProfile);
            r.setString("password", recPassword);
            HostWide word;
            hostLocalise(h, offline ? "ACCOUNT_offline" : "ACCOUNT_online", word);
            r.pushWide(word.c_str(), (int) word.size());
            r.pushNarrow(offline ? recOffline : recGamespy);
            if (gs)
            {
                // GlobalSettings +0x20, IGlobalSettings::getDefaultUser: the profile id the screens
                // preselect, returned by value into the caller's raw buffer on the retail side.
                const std::string cur = static_cast<IGlobalSettings*>(gs)->getDefaultUser();
                if (::strcmp(cur.c_str(), recId) == 0)
                    r.setInt("selected", 1);
            }
        }
        if (total == 0)
        {
            r.beginRow(0);
            r.setInt("disabled", 2);
            HostWide dash;
            hostLocalise(h, "-", dash);
            r.pushWide(dash.c_str(), (int) dash.size());
            r.pushNarrow(" ");
            total = 1;
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    // Country rows include flags and localized names. The first entry is selected and
    // entry four is disabled; total remains 232 regardless of the requested window.
    if (tok[0] == "country")
    {
        r.begin();
        for (unsigned i = (unsigned) start; i < (unsigned) end; ++i)
        {
            if (i >= (unsigned) kNumCountries) continue;
            const char* code = s_countries[i].code;
            r.beginRowStr(code);
            {
                char flag[64];
                ::strcpy(flag, "images/flags/");
                ::strcat(flag, code);
                ::strcat(flag, ".png");
                r.pushNarrow(flag);
            }
            HostWide word;
            if (code[0] == 0)
            {
                hostLocalise(h, " ", word);
            }
            else
            {
                char key[32];
                ::strcpy(key, "COUNTRY_");
                ::strcat(key, code);
                hostLocalise(h, key, word);
            }
            r.pushWide(word.c_str(), (int) word.size());
            if (i == 0)      r.setInt("selected", 1);
            else if (i == 4) r.setInt("disabled", 2);
        }
        r.setTotalCount(kNumCountries);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "tempList")
    {
        r.begin();
        r.setTotalCount(0);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "serverList")
    {
        // Emits internet-server rows within the requested window; invalid records do not consume slots.
        // totalCount is the full record count. Version/mod/map mismatches disable a row.
        // Preserve the fixed column count and row ID containing address, query port and host port.
        // mapMode intentionally uses maxPlayers. Browser integration remains part of RW-88.
        r.begin();
        int total = 0;
        int greyed = 0, emittedRows = 0;   // diagnostic only, reported under the AS log below
        int gateNoMod = 0, gateVersion = 0, gateNoMap = 0;
        vc71::string firstMod, firstMap;   // retained internal diagnostic storage
        IModInfo* modInfo = static_cast<IModInfo*>(h->getModInfo());                     // +0x120
        const int count = serverRecordCount(h->getServerList(std::string("inet"))->getRecords());
        total = count;
        if (start < count && end != start)
        {
            int emitted = 0;
            for (int index = start; index < count && emitted < end - start; ++index)
            {
                // Looked up again every iteration, exactly as the shipped loop does.
                const ServerBrowserRecordList* recs =
                    h->getServerList(std::string("inet"))->getRecords();                 // +0x7C, +0x20
                if (recs->records.empty()) continue;   // the shipped null-record test - and,
                                                       // like it, no bounds check on the index
                const ServerBrowserRecord& rec = recs->records[index];
                const ServerBrowserEntry& e = rec.entry;
                if (e.maxPlayers <= 0) continue;

                const IModInfo::Mod* mod = modInfo->getModFromName(e.gameVariant);       // +0x2C
                // Convert the server's display name to a level path before map lookup.
                // Unknown names pass through unchanged.
                const std::string mapPath = modInfo->getMapPathFromMap(mod, e.mapName);     // PE +0x40
                const IModInfo::Map* knownMap = modInfo->findMapFromMod(mod, mapPath);      // +0x28

                char id[128];
                ::_snprintf(id, sizeof(id) - 1, "%s %d %d", e.address.c_str(), e.queryPort, e.hostPort);
                id[sizeof(id) - 1] = 0;
                r.beginRowStr(id);
                r.setString("ip", e.address.c_str());
                r.setInt("port", e.queryPort);
                r.setInt("pb", e.antiCheat ? 1 : 0);

                const std::string ourVersion = h->getVersionStringValue();               // +0x270
                const bool xpack1 = h->getExpansionPack(e.gameVariant) == 1;             // +0x36C
                const bool xpack2 = h->getExpansionPack(e.gameVariant) == 2;             // +0x36C again
                if (firstMod.empty()) { firstMod = e.gameVariant.c_str(); firstMap = e.mapName.c_str(); }
                if (!mod)
                {
                    if (!xpack1 && !xpack2) { r.setInt("disabled", 1); ++greyed; ++gateNoMod; }
                }
                else if (e.gameVersion != ourVersion) { r.setInt("disabled", 1); ++greyed; ++gateVersion; }
                else if (!knownMap)                   { r.setInt("disabled", 1); ++greyed; ++gateNoMap; }
                r.setInt("xpack1", xpack1 ? 1 : 0);
                r.setInt("xpack2", xpack2 ? 1 : 0);
                r.setInt("plasma", e.plasma ? 1 : 0);
                if (!knownMap) r.setInt("mapnotinstalled", 1);
                if (knownMap)   // the shipped body asks +0x40 a second time for the field (10022F7F)
                    r.setString("mapName", modInfo->getMapPathFromMap(mod, e.mapName).c_str());
                else
                    r.setString("mapName", " ");
                r.setInt("mapMode", e.maxPlayers);
                r.setString("gameMode", e.gameMode.c_str());
                r.setInt("maxPlayers", e.maxPlayers);
                r.setInt("mapSize", e.mapSize);

                r.pushNarrow(e.fps >= 30.0f ? "images/serverIcons/Serverload_green.png"
                           : e.fps >= 20.0f ? "images/serverIcons/Serverload_orange.png"
                                            : "images/serverIcons/Serverload_red.png");
                r.pushNarrow(serverOsIcon(e.os.c_str()));
                if (mod)
                {
                    const std::string icon = "#" + mod->path + "/" + mod->icon;
                    r.pushNarrow(icon.c_str());
                }
                else r.pushNarrow("images/serverIcons/modNotInstalled.png");
                r.pushNarrow(e.ranked    ? "images/serverIcons/ranked.png" : "");
                r.pushNarrow(e.voip      ? "images/serverIcons/battlecom.png" : "");
                r.pushNarrow(e.antiCheat ? "images/serverIcons/punkbuster.png" : "");
                r.pushNarrow(e.pure      ? "images/serverIcons/PureContent.png" : "");
                r.pushNarrow(e.password  ? "images/serverIcons/passwordenabled.png" : "");
                r.pushNarrow(e.hostName.c_str());
                char cell[64];
                ::_snprintf(cell, sizeof(cell) - 1, "%d", e.ping);
                cell[sizeof(cell) - 1] = 0;
                r.pushNarrow(cell);
                ::_snprintf(cell, sizeof(cell) - 1, "%d/%d (%d)", e.numPlayers, e.maxPlayers, e.reservedSlots);
                cell[sizeof(cell) - 1] = 0;
                r.pushNarrow(cell);
                r.pushNarrow(e.mapName.c_str());
                r.pushNarrow(modInfo->getNameFromGameMode(e.gameMode).c_str());           // +0x4C
                r.setInt("password", e.password ? 1 : 0);
                ++emitted; ++emittedRows;
            }
        }
        if (asLogEnabled() && emittedRows)
        {
            ::renddx9abi::logf("[AS] serverList -> %d of %d rows built, %d greyed out",
                               emittedRows, total, greyed);
            ::renddx9abi::logf("[AS] serverList -> gates: noMod %d, version %d, noMap %d; first '%s' / '%s'",
                               gateNoMod, gateVersion, gateNoMap, firstMod.c_str(), firstMap.c_str());
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "serverListLAN")
    {
        // LAN rows accept only gpm_cq/gpm_coop, without a max-player gate or icon columns.
        // Filtered modes consume no window slot; total remains the full browser record count.
        // Preserve repeated mod-info lookups for the mode display name.
        IModInfo* modInfo = static_cast<IModInfo*>(h->getModInfo());                     // +0x120
        r.begin();
        const int count = serverRecordCount(h->getServerList(std::string("lan"))->getRecords());
        if ((unsigned int) start < (unsigned int) count && end != start)
        {
            const unsigned int window = (unsigned int) (end - start);
            unsigned int emitted = 0;
            for (unsigned int index = (unsigned int) start; index < (unsigned int) count && emitted < window; ++index)
            {
                const ServerBrowserRecordList* recs =
                    h->getServerList(std::string("lan"))->getRecords();                  // every pass
                if (recs->records.empty()) continue;
                const ServerBrowserEntry& e = recs->records[index].entry;
                const bool coop = ::_stricmp(e.gameMode.c_str(), "gpm_coop") == 0;
                const bool conquest = ::_stricmp(e.gameMode.c_str(), "gpm_cq") == 0;
                if (!conquest && !coop) continue;

                const IModInfo::Mod* mod = modInfo->getModFromName(e.gameVariant);       // +0x2C
                const std::string mapPath = modInfo->getMapPathFromMap(mod, e.mapName);     // PE +0x40
                const IModInfo::Map* knownMap = modInfo->findMapFromMod(mod, mapPath);      // +0x28
                char id[128];
                ::_snprintf(id, sizeof(id) - 1, "%s %d %d", e.address.c_str(), e.queryPort, e.hostPort);
                id[sizeof(id) - 1] = 0;
                r.beginRowStr(id);
                r.setString("ip", e.address.c_str());
                r.setInt("port", e.queryPort);
                r.setInt("pb", e.antiCheat ? 1 : 0);
                const std::string ourVersion = h->getVersionStringValue();               // +0x270
                const bool xpack1 = h->getExpansionPack(e.gameVariant) == 1;             // +0x36C
                const bool xpack2 = h->getExpansionPack(e.gameVariant) == 2;
                if (!mod)
                {
                    if (!xpack1 && !xpack2) r.setInt("disabled", 1);
                }
                else if (e.gameVersion != ourVersion) r.setInt("disabled", 1);
                else if (!knownMap)                   r.setInt("disabled", 1);
                r.setInt("xpack1", xpack1 ? 1 : 0);
                r.setInt("xpack2", xpack2 ? 1 : 0);
                r.setInt("plasma", e.plasma ? 1 : 0);
                if (!knownMap) r.setInt("mapnotinstalled", 1);
                if (knownMap)
                    r.setString("mapName", modInfo->getMapPathFromMap(mod, e.mapName).c_str());
                else
                    r.setString("mapName", " ");
                r.setInt("mapMode", e.maxPlayers);
                r.setString("gameMode", e.gameMode.c_str());
                r.setInt("maxPlayers", e.maxPlayers);
                r.setInt("mapSize", e.mapSize);

                r.pushNarrow(e.hostName.c_str());
                char cell[64];
                ::_snprintf(cell, sizeof(cell) - 1, "%d", e.ping);
                cell[sizeof(cell) - 1] = 0;
                r.pushNarrow(cell);
                ::_snprintf(cell, sizeof(cell) - 1, "%d/%d (%d)", e.numPlayers, e.maxPlayers, e.reservedSlots);
                cell[sizeof(cell) - 1] = 0;
                r.pushNarrow(cell);
                r.pushNarrow(e.mapName.c_str());
                r.pushNarrow(static_cast<IModInfo*>(h->getModInfo())->getNameFromGameMode(e.gameMode).c_str());
                r.setInt("password", e.password ? 1 : 0);
                ++emitted;
            }
        }
        r.setTotalCount(count);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "installedMaps")
    {
        // Lists current-mod maps supporting the selected mode, one row per map size.
        // With bots enabled, skip sizes lacking AI without counting them. Select the first
        // emitted row and count all eligible rows, including those outside the window.
        // [RETIRE-WHEN: RW-88]
        IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
        (void) classManager->getSingleton(renddx9abi::HostString("GameServerSettings"));
        r.begin();
        int rowIndex = 0;
        int total = 0;
        bool selectedAssigned = false;
        const std::string gameMode = h->getGameMode();                                   // +0x164
        const bool useBots = gs && static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->getUseBots();
        const std::string modDir = h->getModDirectorySetting();                          // +0x124
        IModInfo* modInfo = static_cast<IModInfo*>(h->getModInfo());                     // +0x120
        std::vector<IModInfo::Mod>* mods = modInfo ? modInfo->getModList() : 0;          // +0x14
        const IModInfo::Mod* mod = 0;
        if (mods)
            for (unsigned int i = 0; i < mods->size() && !mod; ++i)
                if (::_stricmp((*mods)[i].path.c_str(), modDir.c_str()) == 0)
                    mod = &(*mods)[i];
        if (mod)
        {
            for (unsigned int mi = 0; mi < mod->maps.size(); ++mi)
            {
                const IModInfo::Map& map = mod->maps[mi];
                for (unsigned int gi = 0; gi < map.gameModes.size(); ++gi)
                {
                    const IModInfo::GameMode& gm = map.gameModes[gi];
                    if (::_stricmp(gm.name.c_str(), gameMode.c_str()) != 0) continue;
                    for (unsigned int ti = 0; ti < gm.mapTypes.size(); ++ti)
                    {
                        const IModInfo::MapType& mt = gm.mapTypes[ti];
                        if (useBots && !mt.ai) continue;
                        if (rowIndex >= start && rowIndex < end)
                        {
                            char text[320];
                            ::_snprintf(text, sizeof(text) - 1, "%s - %d", map.name.c_str(), mt.players);
                            text[sizeof(text) - 1] = 0;
                            r.beginRow((int) mi);
                            r.pushNarrow(text);
                            r.setString("path", map.path.c_str());
                            r.setInt("mapSize", mt.players);
                            if (!selectedAssigned) { r.setInt("selected", 1); selectedAssigned = true; }
                        }
                        ++rowIndex;
                        ++total;
                    }
                }
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }

    // End-of-round lists use unchecked team/player/group indices from the requested ID.
    // Localized labels get fresh output strings; empty keys produce empty labels.

    const auto pushLocalised = [&](const char* key) {
        HostWide word;
        hostLocalise(h, key, word);
        r.pushWide(word.c_str(), (int) word.size());
    };
    if (tok[0] == "teamScore")
    {
        // Team scores: mode 0 lists non-commanders, 1 the commander, 2 the local player.
        // Single-player modes emit at most one row and require a window containing zero.
        const int team = ::atoi(tok[1].c_str());
        const int mode = ::atoi(tok[2].c_str());
        r.begin();
        int total = 0;
        const SwiffEndOfRoundInfo* rec = h->getEndOfRoundInfo();                       // +0x178
        const std::vector<SwiffEndOfRoundPlayer>* players = rec ? &rec->teams[team].players : 0;
        const auto playerAt = [&](unsigned int i) -> const SwiffEndOfRoundPlayer& { return (*players)[i]; };
        const auto playerCount = [&]() -> unsigned int { return (unsigned int) players->size(); };
        const auto teamRow = [&](unsigned int i) {
            r.beginRow((int) i);
            r.setInt("commander", (int) playerAt(i).isCommander);                       // sub_10019970, movzx of the byte
            r.setInt("isme", (int) playerAt(i).isLocalPlayer);
            r.setInt("ismysquad", (int) playerAt(i).isSquadMate);
            r.pushInt((int) i + 1);
            r.pushNarrow(playerAt(i).name.c_str());
            r.pushInt(playerAt(i).score);
            r.pushInt(playerAt(i).teamScore);
            r.pushInt(playerAt(i).kills);
            r.pushInt(playerAt(i).deaths);
        };
        if (rec)
        {
            if (mode == 0)
            {
                int rows = total;
                for (unsigned int i = 0; i < playerCount(); ++i)                          // the size re-read per pass
                {
                    if ((unsigned int) rows >= (unsigned int) start && (unsigned int) rows < (unsigned int) end
                        && !playerAt(i).isCommander)
                        teamRow(i);
                    if (!playerAt(i).isCommander)
                        ++rows;
                }
                total = rows;
            }
            else if (mode == 1 || mode == 2)
            {
                const unsigned int n = playerCount();                                     // read once here
                for (unsigned int i = 0; i < n; ++i)
                {
                    if ((unsigned int) total >= (unsigned int) start && (unsigned int) total < (unsigned int) end
                        && (mode == 1 ? playerAt(i).isCommander : playerAt(i).isLocalPlayer))
                    {
                        teamRow(i);
                        r.setTotalCount(1);                                               // the literal 1
                        fn.result->set_as_object_interface(r.arr.get_ptr());
                        return;
                    }
                }
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "ticketHistory")
    {
        // Lists round ticket history with localized map names and both teams' tickets.
        // Total counts all tickets; only row zero receives isLastOne = 0.
        r.begin();
        int total = 0;
        SwiffEndOfRoundSummary summary;
        if (h->getEndOfRoundSummary(summary))                                          // +0x1A4
        {
            const std::vector<TicketHistory::Ticket>& tickets = summary.ticketHistory.tickets;
            for (unsigned int i = 0; i < tickets.size(); ++i, ++total)
            {
                if (i < (unsigned int) start || i >= (unsigned int) end)
                    continue;
                const TicketHistory::Ticket& ticket = tickets[i];
                char id[16];
                ::sprintf(id, "%d", i);
                r.beginRowStr(id);
                if (i == 0)
                    r.setInt("isLastOne", (int) i);
                const IModInfo::Map* map = static_cast<IModInfo*>(h->getModInfo())->findMapFromCurrentMod(ticket.string0);
                r.pushNarrow(map ? map->name.c_str() : ticket.string0.c_str());
                r.pushInt(ticket.pair[0].value);
                r.pushInt(ticket.pair[1].value);
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "topPlayers")
    {
        // Mode 0 lists seven label/value summary rows for a named top player.
        // Other modes list played kits without windowing.
        r.begin();
        const int position = ::atoi(tok[1].c_str());
        const int mode = ::atoi(tok[2].c_str());
        int total = 0;
        const SwiffEndOfRoundInfo* rec = h->getEndOfRoundInfo();
        if (rec && rec->topPlayers[position].name != "")
        {
            const SwiffEndOfRoundTopPlayer& top = rec->topPlayers[position];
            if (mode == 0)
            {
                if (top.name.empty())
                {
                    r.beginRow(0); pushLocalised("EOR_playername");        r.pushNarrow("N/A");
                    r.setInt("rank", -1); r.setInt("isme", 0);
                    r.beginRow(0); pushLocalised("EOR_teamworkscore");     r.pushNarrow("N/A");
                    r.beginRow(0); pushLocalised("EOR_combatscore");       r.pushNarrow("N/A");
                    r.beginRow(0); pushLocalised("EOR_commanderscore");    r.pushNarrow("N/A");
                    r.beginRow(0); pushLocalised("EOR_mostplayedkit");     pushLocalised("");
                    r.beginRow(0); pushLocalised("EOR_mostplayedvehicle"); pushLocalised("");
                    r.beginRow(0); pushLocalised("EOR_overallaccuracy");   r.pushNarrow("N/A");
                    total = 7;
                }
                else
                {
                    r.beginRow(0); pushLocalised("EOR_playername");
                    r.pushNarrow(top.name.c_str());
                    r.setInt("rank", top.rank);
                    r.setInt("isme", h->isLocalPlayerName(top.name) ? 1 : 0);               // +0x2E8
                    r.beginRow(0); pushLocalised("EOR_teamworkscore");  r.pushInt(top.teamScore);
                    r.beginRow(0); pushLocalised("EOR_combatscore");    r.pushInt(top.skillScore);
                    r.beginRow(0); pushLocalised("EOR_commanderscore"); r.pushInt(top.cmdScore);
                    r.beginRow(0); pushLocalised("EOR_mostplayedkit");
                    pushLocalised(top.favouriteKit != -1 ? kFavouriteKitKeys[top.favouriteKit] : "");        // unchecked index
                    r.beginRow(0); pushLocalised("EOR_mostplayedvehicle");
                    pushLocalised(top.favouriteVehicle != -1 ? kFavouriteVehicleKeys[top.favouriteVehicle] : "");
                    r.beginRow(0); pushLocalised("EOR_overallaccuracy");
                    char accuracy[64];
                    ::sprintf(accuracy, "%f", (double) top.accuracy);
                    r.pushNarrow(accuracy);
                    total = 7;
                }
            }
            else
            {
                for (unsigned int i = 0; i < top.kits.size(); ++i, ++total)
                {
                    r.beginRow(0);
                    r.pushInt(top.kits[i]);
                }
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "topSquads")
    {
        // PE 1002EAA6..1002EF35 - "topSquads <team> <mode>", the team's best squad. With a squad
        // name: mode 0 one row (name, summed score); mode 1 the leader (field "rank", name, score,
        // field "isme" = host +0x2E8 on the name); mode 2 one row per other member, id = its index,
        // the same shape as the leader's, no window, totalCount = their number. Without a name: modes
        // 0..2 a single placeholder row ("N/A", ""; modes 1 and 2 also "rank" -1 and "isme" 0). Any
        // other mode, or no record: the empty list. totalCount is 1 for every single-row answer.
        r.begin();
        const int team = ::atoi(tok[1].c_str());
        const int mode = ::atoi(tok[2].c_str());
        int total = 0;
        const SwiffEndOfRoundInfo* rec = h->getEndOfRoundInfo();
        if (rec)
        {
            const SwiffEndOfRoundTeam& squadTeam = rec->teams[team];
            const auto isMe = [&](const std::string& name) { return h->isLocalPlayerName(name) ? 1 : 0; };   // +0x2E8
            const auto member = [&](unsigned int i) -> const SwiffEndOfRoundPlayer& { return squadTeam.squadMembers[i]; };
            const auto memberCountNow = [&]() -> unsigned int { return (unsigned int) squadTeam.squadMembers.size(); };
            if (!squadTeam.bestSquadName.empty())                                       // operator!=(string, "")
            {
                if (mode == 0)
                {
                    r.beginRow(0);
                    r.pushNarrow(squadTeam.bestSquadName.c_str());
                    r.pushInt(squadTeam.bestSquadScore);
                    total = 1;
                }
                else if (mode == 1)
                {
                    r.beginRow(0);
                    r.setInt("rank", squadTeam.squadLeader.rank);
                    r.pushNarrow(squadTeam.squadLeader.name.c_str());
                    r.pushInt(squadTeam.squadLeader.score);
                    r.setInt("isme", isMe(squadTeam.squadLeader.name));
                    total = 1;
                }
                else if (mode == 2)
                {
                    for (unsigned int i = 0; i < memberCountNow(); ++i, ++total)          // the size re-read per pass
                    {
                        r.beginRow((int) i);
                        r.setInt("rank", member(i).rank);
                        r.pushNarrow(member(i).name.c_str());
                        r.pushInt(member(i).score);
                        r.setInt("isme", isMe(member(i).name));
                    }
                }
            }
            else
            {
                if (mode == 0)
                {
                    r.beginRow(0);
                    r.pushNarrow("N/A");
                    r.pushNarrow("");
                    total = 1;
                }
                else if (mode == 1 || mode == 2)
                {
                    r.beginRow(0);
                    r.setInt("rank", -1);
                    r.pushNarrow("N/A");
                    r.pushNarrow("");
                    r.setInt("isme", 0);
                    total = 1;
                }
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "hscoreKits" || tok[0] == "hscoreVehicles")
    {
        // Lists kit/vehicle high scorers, using N/A for missing or nonpositive scores.
        // Vehicle type 5 (artillery) is omitted from both rows and total.
        const bool vehicles = tok[0] == "hscoreVehicles";
        const char* const* keys = vehicles ? kFavouriteVehicleKeys : kFavouriteKitKeys;
        r.begin();
        int total = 0;
        const SwiffEndOfRoundInfo* rec = h->getEndOfRoundInfo();
        if (rec)
        {
            const SwiffEndOfRoundHighScore* leaders = vehicles ? rec->vehicleLeaders : rec->kitLeaders;
            for (int k = 0; k < 7; ++k)
            {
                if (vehicles && k == 5)
                    continue;
                if (!leaders[k].name.empty())
                {
                    r.beginRow(k);
                    if (leaders[k].score > 0)
                    {
                        pushLocalised(keys[k]);
                        r.pushNarrow(leaders[k].name.c_str());
                        r.pushInt(leaders[k].score);
                        r.setInt("rank", leaders[k].rank);
                        r.setInt("isme", h->isLocalPlayerName(leaders[k].name) ? 1 : 0);
                    }
                    else
                    {
                        pushLocalised(keys[k]);
                        r.pushNarrow("N/A");
                        r.pushNarrow("");
                    }
                }
                else
                {
                    r.beginRow(k);
                    pushLocalised(keys[k]);
                    r.pushNarrow("N/A");
                    r.pushNarrow("");
                }
                ++total;
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "hscores")
    {
        // PE 1002F448..1002F721 - "hscores <group>", the category awards in four groups of the eleven
        // (group 0: teamwork; 1: skill; 2: captures / assists / defends; anything else: kill assists
        // .. driver specials). One row per award, id = its index, each counted: when the winner has a
        // name AND a score above zero (signed), the field "rank" first, then the BFHQ_eor_* label
        // (the shipped table 0x100F33D8), the name, the score and the field "isme" (host +0x2E8);
        // otherwise the label, "N/A" and "".
        r.begin();
        const int group = ::atoi(tok[1].c_str());
        unsigned int first, last;
        if (group == 0)      { first = 0; last = 1; }
        else if (group == 1) { first = 1; last = 2; }
        else if (group == 2) { first = 2; last = 5; }
        else                 { first = 5; last = 11; }
        int total = 0;
        const SwiffEndOfRoundInfo* rec = h->getEndOfRoundInfo();
        if (rec)
        {
            for (unsigned int c = first; c < last; ++c)
            {
                if (!rec->awards[c].name.empty())
                {
                    r.beginRow((int) c);
                    if (rec->awards[c].score > 0)
                    {
                        r.setInt("rank", rec->awards[c].rank);
                        pushLocalised(kEndOfRoundAwardKeys[c]);
                        r.pushNarrow(rec->awards[c].name.c_str());
                        r.pushInt(rec->awards[c].score);
                        r.setInt("isme", h->isLocalPlayerName(rec->awards[c].name) ? 1 : 0);
                    }
                    else
                    {
                        pushLocalised(kEndOfRoundAwardKeys[c]);
                        r.pushNarrow("N/A");
                        r.pushNarrow("");
                    }
                }
                else
                {
                    r.beginRow((int) c);
                    pushLocalised(kEndOfRoundAwardKeys[c]);
                    r.pushNarrow("N/A");
                    r.pushNarrow("");
                }
                ++total;
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "eorsinfo")
    {
        // Publish the selected server's identity before selecting an end-of-round info group.
        // Merge live values with browser fields; zero/false live values use the cached record.
        CachedRecord record;
        fetchEorServerRecord(record);
        publishEorHudServerInfo(record);
        r.begin();
        const int selector = ::atoi(tok[1].c_str());
        int total = count; // an out-of-range selector leaves the incoming count in the shipped local
        HostWide label;
        const char* recordMode = 0;
        bool recordFriendlyFire = false, recordAutoBalance = false;
        bool recordGlobalUnlocks = false, recordNoVehicles = false;
        int recordRounds = 0, recordSpawn = 0, recordTime = 0;
        int recordTickets = 0, recordTeamRatio = 0;
        int recordCoopRatio = 0, recordCoopCount = 0, recordCoopDifficulty = 0;
        recordMode = record.entry.gameMode.c_str();
        recordFriendlyFire = record.details.friendlyFire;
        recordRounds = record.details.roundTime;
        recordSpawn = (int) record.details.spawnTime;
        recordTime = record.details.timeLimit;
        recordTickets = record.details.ticketRatio;
        recordTeamRatio = (int) record.details.teamRatio;
        recordAutoBalance = record.details.autoBalanced;
        recordGlobalUnlocks = record.details.globalUnlocks;
        recordNoVehicles = record.details.noVehicles;
        recordCoopRatio = (int) record.details.coopBotRatio;
        recordCoopCount = record.details.coopBotCount;
        recordCoopDifficulty = (int) record.details.coopBotDifficulty;
        // Every physical row has id 0, then one localised wide label and one narrow/integer value.
        // These small writers keep that repeated shipped order visible.
        const auto boolRow = [&](const char* key, bool value) {
            r.beginRow(0); hostLocalise(h, key, label);
            r.pushWide(label.c_str(), (int) label.size()); r.pushNarrow(value ? "1" : "0");
        };
        const auto intRow = [&](const char* key, int value) {
            r.beginRow(0); hostLocalise(h, key, label);
            r.pushWide(label.c_str(), (int) label.size()); r.pushInt(value);
        };

        if (selector == 0)
        {
            const auto liveBoolRow = [&](const char* key, bool (ISwiffHost::*getter)(), bool fallback) {
                r.beginRow(0); hostLocalise(h, key, label); r.pushWide(label.c_str(), (int) label.size());
                const bool probe = (h->*getter)();
                r.pushNarrow(probe ? ((h->*getter)() ? "1" : "0") : (fallback ? "1" : "0"));
            };
            const auto liveIntRow = [&](const char* key, int (ISwiffHost::*getter)(), int fallback, int divisor) {
                r.beginRow(0); hostLocalise(h, key, label); r.pushWide(label.c_str(), (int) label.size());
                const int probe = (h->*getter)();
                r.pushInt((probe ? (h->*getter)() : fallback) / divisor);
            };
            liveBoolRow("SERVERINFO_friendlyfire", &ISwiffHost::getServerInfoAlwaysTrue, recordFriendlyFire); // +0x384
            liveIntRow("SERVERINFO_roundspermap", &ISwiffHost::getServerInfoRoundsPerMap, recordRounds, 1);    // +0x388
            liveIntRow("SERVERINFO_spawntime", &ISwiffHost::getServerInfoSpawnTime, recordSpawn, 1);           // +0x38C
            liveIntRow("SERVERINFO_timelimit", &ISwiffHost::getServerInfoTimeLimit, recordTime, 60);           // +0x390
            liveIntRow("SERVERINFO_ticketratio", &ISwiffHost::getServerInfoTicketRatio, recordTickets, 1);     // +0x394
            liveIntRow("SERVERINFO_teamratio", &ISwiffHost::getServerInfoTeamRatioPercent, recordTeamRatio, 1);// +0x398
            total = 6;
        }
        else if (selector == 1)
        {
            const auto liveBoolRow = [&](const char* key, bool (ISwiffHost::*getter)(), bool fallback) {
                r.beginRow(0); hostLocalise(h, key, label); r.pushWide(label.c_str(), (int) label.size());
                const bool probe = (h->*getter)();
                r.pushNarrow(probe ? ((h->*getter)() ? "1" : "0") : (fallback ? "1" : "0"));
            };
            liveBoolRow("SERVERINFO_autobalance", &ISwiffHost::getServerInfoAutoBalanceTeam, recordAutoBalance);     // +0x39C
            liveBoolRow("SERVERINFO_allowglobalunlocks", &ISwiffHost::getServerInfoUseGlobalUnlocks, recordGlobalUnlocks); // +0x3A0

            bool coop = ::strcmp(recordMode, "gpm_coop") == 0;
            if (!coop)
            {
                coop = h->getCurrentMapGameMode() == "gpm_coop";                  // +0x5C
            }
            if (coop)
            {
                // PE 1002FA6E..1002FB8A reads all three directly from the browser record. It does
                // NOT consult host +0x3A4/+0x3A8/+0x3AC, even when those values are nonzero.
                intRow("SERVERINFO_coopteamratio", recordCoopRatio);
                intRow("SERVERINFO_coopbotcount", recordCoopCount);
                intRow("SERVERINFO_coopbotdiff", recordCoopDifficulty);
            }
            else
            {
                liveBoolRow("SERVERINFO_novehicles", &ISwiffHost::getServerInfoNoVehicles, recordNoVehicles); // +0x380
            }
            // Shipped quirk: both arms report TWO although they physically emit three rows in
            // non-coop and five in coop (1002FB8F / 1002FC24..1002FC6B).
            total = 2;
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "players")
    {
        // Lists selected-server players for team selector 0 or 1.
        // Coop bots are excluded only inside the requested window; outside it they still
        // contribute to totalCount, so the reported total can exceed displayed rows.
        r.begin();
        int total = 0;
        std::string address;
        int port = 0, hostPort = 0;
        h->getSelectedServer(address, &port, &hostPort);                             // +0x110
        CachedRecord record;
        const int team = ::atoi(tok[1].c_str());
        if (port != 0 && fetchSelectedRecord(port, address, record, true))
        {
            const std::vector<ServerBrowserPlayer>& players = record.details.players;
            int rowNumber = start + 1;
            int candidate = 0;
            for (unsigned int i = 0; i < players.size(); ++i)
            {
                const ServerBrowserPlayer& p = players[i];
                const int playerTeam = ::atoi(p.team.c_str());
                const bool wanted = (team == 0 && playerTeam == 1) || (team == 1 && playerTeam == 2);
                if (candidate < start || candidate >= end)
                {
                    if (wanted) { ++total; ++candidate; }
                    continue;
                }
                if (!wanted)
                    continue;
                const std::string name(p.name);
                if (record.entry.gameMode == "gpm_coop" && p.aiBot == 1)
                    continue;
                r.beginRow((int) i);
                r.pushInt(rowNumber);
                r.pushNarrow(name.c_str());
                r.pushInt(p.score);
                r.pushInt(p.skill);
                r.pushInt(p.deaths);
                ++rowNumber;
                ++total; ++candidate;
            }
        }
        r.setTotalCount(total);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    if (tok[0] == "serverExtendedInfo" || tok[0] == "staticExtendedServerInfo")
    {
        // Publishes ten selected-server detail rows inside the busy-cursor bracket.
        // A missing record returns an empty list; address/port and team names share the final row.
        BusyHost busy = busyOpen();
        r.begin();
        HostWide label;
        std::string address;
        int port = 0, hostPort = 0;
        h->getSelectedServer(address, &port, &hostPort);                             // +0x110
        CachedRecord record;
        if (!fetchSelectedRecord(port, address, record, true))
            r.setTotalCount(0);
        else
        {
            const ServerBrowserDetails& d = record.details;
            r.beginRow(0); hostLocalise(h, "SERVERINFO_friendlyfire", label);        r.pushWide(label.c_str(), (int) label.size()); r.pushNarrow(d.friendlyFire ? "1" : "0");
            r.beginRow(0); hostLocalise(h, "SERVERINFO_roundspermap", label);        r.pushWide(label.c_str(), (int) label.size()); r.pushInt(d.roundTime);
            r.beginRow(0); hostLocalise(h, "SERVERINFO_spawntime", label);           r.pushWide(label.c_str(), (int) label.size()); r.pushInt((int) d.spawnTime);
            r.beginRow(0); hostLocalise(h, "SERVERINFO_timelimit", label);           r.pushWide(label.c_str(), (int) label.size()); r.pushInt(d.timeLimit / 60);
            r.beginRow(0); hostLocalise(h, "SERVERINFO_ticketratio", label);         r.pushWide(label.c_str(), (int) label.size()); r.pushInt(d.ticketRatio);
            r.beginRow(0); hostLocalise(h, "SERVERINFO_teamratio", label);           r.pushWide(label.c_str(), (int) label.size()); r.pushInt((int) d.teamRatio);
            r.beginRow(0); hostLocalise(h, "SERVERINFO_autobalance", label);         r.pushWide(label.c_str(), (int) label.size()); r.pushNarrow(d.autoBalanced ? "1" : "0");
            r.beginRow(0); hostLocalise(h, "SERVERINFO_allowglobalunlocks", label);  r.pushWide(label.c_str(), (int) label.size()); r.pushNarrow(d.globalUnlocks ? "1" : "0");
            r.beginRow(0); hostLocalise(h, "SERVERINFO_novehicles", label);          r.pushWide(label.c_str(), (int) label.size()); r.pushNarrow(d.noVehicles ? "1" : "0");
            r.beginRow(0);
            r.pushNarrow(record.entry.address.c_str());
            char portText[16]; ::sprintf(portText, "%d", record.entry.hostPort);
            r.pushNarrow(portText);
            r.setString("team1", d.team1.c_str());
            r.setString("team2", d.team2.c_str());
            r.setTotalCount(10);
        }
        fn.result->set_as_object_interface(r.arr.get_ptr());
        busyClose(busy);
        return;
    }
    if (tok[0] == "videoSettings")
    {
        // the options > video select boxes (listVideoSettings, above).
        listVideoSettings(fn, r, tok[1].c_str(), start, end, prologueGeneralSettings);
        return;
    }
    if (tok[0] == "audioSettings")
    {
        // Audio provider and quality choices.
        listAudioSettings(fn, r, tok[1].c_str(), start, end);
        return;
    }
    // The bfhq lists: the stats tables, the player selection and the unlock page's
    // team / kit / equipment lists. Each body is defined beside the PlayerStats rows it shares its
    // record and unlock helpers with; the branch order here is not observable (one id matches).
    if (tok[0] == "playerStats")      { listPlayerStats(fn, r, h, tok[1].c_str(), start, end); return; }
    if (tok[0] == "wpstats" || tok[0] == "wpunlockstats" || tok[0] == "wpuunlockstats")
                                      { listWeaponStats(fn, r, tok[0]); return; }
    if (tok[0] == "profileSelection") { listProfileSelection(fn, r, h, start, end); return; }
    if (tok[0] == "teams")            { listTeams(fn, r, h, start, end); return; }
    if (tok[0] == "kits")             { listKits(fn, r, h, tok[1].c_str(), tok[2].c_str(), start, end); return; }
    if (tok[0] == "equipments")       { listEquipments(fn, r, h, tok[1].c_str(), tok[2].c_str(), start, end); return; }
    if (tok[0] == "equipslots")       { listEquipSlots(fn, r, h, tok[1].c_str(), tok[2].c_str()); return; }
    if (tok[0] == "unlockeq")         { listUnlockEquipment(fn, r, h, tok[1].c_str(), tok[2].c_str(), start, end); return; }
    // The awards and leaderboards. lboard and lboardplayers do not
    // read the window: the row builder keeps its own twenty rows.
    if (tok[0] == "latestAwards")     { listLatestAwards(fn, r, h, start, end); return; }
    if (tok[0] == "medals")           { listMedals(fn, r, h, start, end); return; }
    if (tok[0] == "badges")           { listBadges(fn, r, h, start, end); return; }
    if (tok[0] == "ribbons")          { listRibbons(fn, r, h, start, end); return; }
    if (tok[0] == "lboardcat")        { listLeaderboardCategories(fn, r, h, start, end); return; }
    if (tok[0] == "lboard")           { listLeaderboard(fn, r, h); return; }
    if (tok[0] == "lboardplayers")    { listLeaderboardPlayers(fn, r, h); return; }
    if (tok[0] == "lctop")            { listLeaderboardTop(fn, r, h); return; }
    // phase 5: the create-server page's map list (listMapList, defined with the
    // Multiplay create-server rows), and the seven names 1.50 answers through its shared empty exit
    // loc_1003147D (begin, totalCount edi = 0, publish): cmapbg and cmapgm, the four buddy lists and
    // gamespyNicknameSuggestions. That exit is also the compare chain's own default for a name nothing
    // matches; the seven are named here because this port's default stays the RW-88 frontier until
    // every shipped branch is written.
    if (tok[0] == "mapList")          { listMapList(fn, r, h, start, end); return; }
    // Local-server game modes and browser-filter game modes with an additional All choice.
    if (tok[0] == "gmodes")           { listGameModes(fn, r, h, start, end, false); return; }
    if (tok[0] == "sfgmodes")         { listGameModes(fn, r, h, start, end, true); return; }
    if (tok[0] == "demos")            { listDemos(fn, r, h, start, end); return; }
    if (tok[0] == "demobm")           { listDemoBookmarks(fn, r, h, start, end); return; }
    if (tok[0] == "cmapbg" || tok[0] == "cmapgm" || tok[0] == "buddiesOnline" || tok[0] == "buddies"
        || tok[0] == "buddyRequests" || tok[0] == "buddyMessages" || tok[0] == "gamespyNicknameSuggestions")
    {
        r.begin();
        r.setTotalCount(0);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    // [FRONTIER] [RETIRE-WHEN: RW-88] Always reported, not only under the AS log: this row is live
    // now, so an unread branch must name itself every time it is reached rather than answer quietly.
    ::renddx9abi::logf("[FRONTIER] dice.General.getListEntries: list '%s' not written (sub_10022610 branch unread)", id.c_str());
    fn.result->set_undefined();
}

// PE1005FCF0..1005FD82, helper10013520: a guarded registry SwiffHost owns
// cursor/busy begin and end. The separate unguarded player host gets event13.
// Event strings/storage die before restoring the retained registry host.
// The callback reads no arguments and leaves the AS result untouched.
struct SwiffHostRequest
{
    int           id;
    int           alloc;
    vc71::string* first;
    vc71::string* last;
    vc71::string* end;
};
extern "C" __declspec(dllimport) void* __stdcall SetCursor(void* cursor);
void as_profile_logout(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_logout");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    {
        MenuEvent event;
        event.type = 13;
        h->pushEvent(event);
    }
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}

void as_logic_disconnectGame(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_disconnectGame");
    BusyHost cursorHost = busyOpen();
    (void)g_swiffPlayerInstance->getSwiffHost()->requestShutdownGame();
    busyClose(cursorHost);
}

// endOfRound.swf's loading-state bridge. Every row fetches the player host without a
// guard. getIsLoading preserves the shipped two calls and their order (+0xCC first, then +0x2A8),
// and readyToPlay queues the empty type-15 MenuEvent consumed by BF2Engine::renderMenu.
void as_logic_setIsLoading(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setIsLoading");
    const bool value = fn.arg(0).to_bool();
    g_swiffPlayerInstance->getSwiffHost()->setFlag7B8(value);
}

void as_logic_getIsLoading(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getIsLoading");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    bool value = h->getFlag7B8();
    if (h->getFlagEA8()) value = true;
    fn.result->set_bool(value);
}

void as_logic_isFirstTimeLoading(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_isFirstTimeLoading");
    const bool value = g_swiffPlayerInstance->getSwiffHost()->getGameStarting();
    fn.result->set_bool(value);
}

void as_logic_isAbortLoadLevel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_isAbortLoadLevel");
    const bool value = g_swiffPlayerInstance->getSwiffHost()->getFlagF04();
    fn.result->set_bool(value);
}

void as_logic_getNumRoundsLeft(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getNumRoundsLeft");
    const int value = g_swiffPlayerInstance->getSwiffHost()->getIntF4C();
    fn.result->set_double(static_cast<double>(value));
}

void as_logic_isLoadingDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_isLoadingDemo");
    const bool value = g_swiffPlayerInstance->getSwiffHost()->isPlayingDemo();
    fn.result->set_bool(value);
}

void as_logic_readyToPlay(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_readyToPlay");
    (void) fn;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    MenuEvent event;
    event.type = 15;
    h->pushEvent(event);
}

// one direct player host lookup, signed getPage (+2BC),
// then numerical AS result replacement. No player/host guards or argument reads.
void as_logic_getCommandLine(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getCommandLine");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int page = h->getPage();
    fn.result->set_double(static_cast<double>(page));
}

// Joins the command-line address using the configured game port and query port zero.
// Returns numeric 1 or 0, not Boolean; destroys the address before publishing the result.
void as_logic_joinCommandlineServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_joinCommandlineServer");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int port = h->getPortSetting();                        // +0x0E4
    const std::string address = h->getJoinAddressSetting();       // +0x0DC
    const bool ok = h->joinServer(address, port, 0);              // +0x100
    fn.result->set_double(ok ? 1.0 : 0.0);
}

// sub_1004D450 - dice.bf2.Logic.isCapsLockPressed: GetKeyState(VK_CAPITAL) & 1, a boolean. No host.
extern "C" __declspec(dllimport) short __stdcall GetKeyState(int vkey);
void as_logic_isCapsLockPressed(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_isCapsLockPressed");
    fn.result->set_bool((GetKeyState(20) & 1) != 0);
}

// Returns host GameSpy availability as Boolean. Requires player/host and blocks while
// the SDK availability check completes.
void as_multiplay_isGameSpyAvailable(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_isGameSpyAvailable");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_bool(h->isGameSpyAvailable());
}

// Publishes the GlobalSettings name prefix using the host string-return ABI.
// All owners must exist; the returned native string lives through publication.
void as_profile_getNamePrefix(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNamePrefix");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GlobalSettings"));
    // BF2Engine::applyProfileNameSettings uses the same typed singleton. Its native string
    // dies after publication, preserving the shipped destruction order.
    const std::string prefix = static_cast<IGlobalSettings*>(gs)->getNamePrefix();
    fn.result->set_tu_string(tu_string(prefix.c_str()));
}

// Publishes the host's full signed message count as an AS number. Requires player/host.
void as_messagehandler_getNumMessages(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_getNumMessages");
    // The engine owner answers from the size of its own message list (SwiffHost::getMessageCount).
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_double(static_cast<double>(h->getMessageCount()));
}

// fetch host, construct arg0 then arg1, call host +238 with hidden
// return storage, assign the returned c_str() to the AS result, then destroy result/arg1/arg0.
// No player, host, or nargs guards. The owner shares this toolchain's std::string ABI.
void as_logic_getStorageString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getStorageString");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    renddx9abi::HostString a(fn.arg(0).to_string());
    renddx9abi::HostString b(fn.arg(1).to_string());
    const std::string out = h->getStorageString(a, b);
    fn.result->set_tu_string(tu_string(out.c_str()));
}

// fetch host, construct arg0 then arg1, call host +23C, destroy
// arg1 then arg0. No guards and no AS result assignment. The string ABI split matches the getter.
void as_logic_setStorageString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setStorageString");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    renddx9abi::HostString a(fn.arg(0).to_string());
    renddx9abi::HostString b(fn.arg(1).to_string());
    h->setStorageString(a, b);
}

} // anonymous namespace

// Queues the named engine sound. Host refusal publishes numeric -1; success leaves
// the AS result untouched. Requires player/host and argument zero.
void as_sound_playSound(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_playSound");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const renddx9abi::HostString name(fn.arg(0).to_string());
    if (!h->playSound(name))
        fn.result->set_double(-1.0);
}

// Publishes the host's unsupported-driver flag as Boolean, consuming only the low byte.
// Requires player and host.
void as_videosettings_isRunningWithUnsupportedDrivers(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_isRunningWithUnsupportedDrivers");
    // The engine owner answers this from the SwiffRender singleton: an nvidia vendor id with a
    // driver build below 663051 (SwiffHost 0x488EA0, renderer slots +0xE4/+0xE8).
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_bool(h->isRunningWithUnsupportedDrivers());
}

// the misspelling is shipped and is what endOfRound.swf calls. One unguarded host
// +0x31C getRenderFlag dispatch, published as a Boolean.
void as_videosettings_isOptimzingShaders(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_isOptimzingShaders");
    const bool value = g_swiffPlayerInstance->getSwiffHost()->getRenderFlag();
    fn.result->set_bool(value);
}

// Applies pending video settings, then saves the current profile even when apply fails.
// Publishes the apply result as a Boolean. Both singleton lookups require initialized owners.
void as_videosettings_apply(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_apply");
    const bool applied = videoSettings()->apply();                                     // +0x1C
    (void) static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")))->saveCurrentProfile(); // +0x40
    fn.result->set_bool(applied);
}

// cancel(): VideoSettings +0x24 revertChanges (PE 0x4B2890: the
// pending record takes the applied one). No argument, no result.
void as_videosettings_cancel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_cancel");
    videoSettings()->revertChanges();                                                  // +0x24
}

// canApply(): boolean of +0x18 hasChanges (PE 0x4B25F0: pending
// differs from applied). No shipped movie calls it.
void as_videosettings_canApply(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_canApply");
    publishVideoBool(fn, VS_SLOT(hasChanges, 0x18));
}

// reset(): VideoSettings +0x20 setDefaults (PE 0x4B2600). No argument,
// no result.
void as_videosettings_reset(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_reset");
    videoSettings()->setDefaults();                                                    // +0x20
}

// The eight quality pairs. Getters: number of the pending int. Setters: order [A] or [B] (above),
// then the custom-scheme tail.
void as_videosettings_getTerrainQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getTerrainQuality");          publishVideoInt(fn, VS_SLOT(getTerrainQuality, 0x40)); }
void as_videosettings_setTerrainQuality(const gameswf::fn_call& fn)          // PE 1006AC60, 164 B, [A]
{ SWF_AS_TRACE("as_videosettings_setTerrainQuality");          setVideoIntOwnerFirst(fn, VS_SLOT(setTerrainQuality, 0x44)); }
void as_videosettings_getGeometryQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getGeometryQuality");         publishVideoInt(fn, VS_SLOT(getGeometryQuality, 0x48)); }
void as_videosettings_setGeometryQuality(const gameswf::fn_call& fn)         // PE 1006AD10, 164 B, [A]
{ SWF_AS_TRACE("as_videosettings_setGeometryQuality");         setVideoIntOwnerFirst(fn, VS_SLOT(setGeometryQuality, 0x4C)); }
void as_videosettings_getLightingQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getLightingQuality");         publishVideoInt(fn, VS_SLOT(getLightingQuality, 0x50)); }
void as_videosettings_setLightingQuality(const gameswf::fn_call& fn)         // PE 1006ADC0, 164 B, [A]
{ SWF_AS_TRACE("as_videosettings_setLightingQuality");         setVideoIntOwnerFirst(fn, VS_SLOT(setLightingQuality, 0x54)); }
void as_videosettings_getDynamicLightingQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getDynamicLightingQuality");  publishVideoInt(fn, VS_SLOT(getDynamicLightingQuality, 0x58)); }
void as_videosettings_setDynamicLightingQuality(const gameswf::fn_call& fn)  // PE 1006AE70, 162 B, [B]
{ SWF_AS_TRACE("as_videosettings_setDynamicLightingQuality");  setVideoIntArgumentFirst(fn, VS_SLOT(setDynamicLightingQuality, 0x5C)); }
void as_videosettings_getDynamicShadowsQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getDynamicShadowsQuality");   publishVideoInt(fn, VS_SLOT(getDynamicShadowsQuality, 0x60)); }
void as_videosettings_setDynamicShadowsQuality(const gameswf::fn_call& fn)   // PE 1006AF20, 162 B, [B]
{ SWF_AS_TRACE("as_videosettings_setDynamicShadowsQuality");   setVideoIntArgumentFirst(fn, VS_SLOT(setDynamicShadowsQuality, 0x64)); }
void as_videosettings_getEffectsQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getEffectsQuality");          publishVideoInt(fn, VS_SLOT(getEffectsQuality, 0x68)); }
void as_videosettings_setEffectsQuality(const gameswf::fn_call& fn)          // PE 1006AFD0, 164 B, [A]
{ SWF_AS_TRACE("as_videosettings_setEffectsQuality");          setVideoIntOwnerFirst(fn, VS_SLOT(setEffectsQuality, 0x6C)); }
void as_videosettings_getTextureQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getTextureQuality");          publishVideoInt(fn, VS_SLOT(getTextureQuality, 0x70)); }
void as_videosettings_setTextureQuality(const gameswf::fn_call& fn)          // PE 1006B080, 162 B, [B]
{ SWF_AS_TRACE("as_videosettings_setTextureQuality");          setVideoIntArgumentFirst(fn, VS_SLOT(setTextureQuality, 0x74)); }

// getResolution(): string of +0x80, the pending "WxH@RHz" string.
void as_videosettings_getResolution(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getResolution");
    publishVideoString(fn, VS_SLOT(getResolution, 0x80));
}

// setResolution(text): the string is built from argument zero's
// to_string (0x1008D0F0) first; then the lookup and +0x84 setResolution with a by-value copy; then
// the custom-scheme tail; the argument string is destroyed last. No result.
void as_videosettings_setResolution(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_setResolution");
    const std::string value(fn.arg(0).to_string());
    videoSettings()->setResolution(value);                                             // +0x84, by value
    markCustomVideoScheme();
}

// getAntialiasing(): string of +0x88.
void as_videosettings_getAntialiasing(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getAntialiasing");
    publishVideoString(fn, VS_SLOT(getAntialiasing, 0x88));
}

// setAntialiasing(text): setResolution's shape on +0x8C, and with
// NO custom-scheme tail - picking an antialiasing level leaves the scheme as it was (0x1006B24F goes
// straight to the argument's destructor). No result.
void as_videosettings_setAntialiasing(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_setAntialiasing");
    const std::string value(fn.arg(0).to_string());
    videoSettings()->setAntialiasing(value);                                           // +0x8C, by value
}

// setViewDistanceScale(value): argument zero coerced first and stored
// as a float (`fstp dword` - rounded to single precision before the owner sees it), then the
// lookup and +0x94. No custom-scheme tail. The page passes its slider value / 100. No result.
void as_videosettings_setViewDistanceScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_setViewDistanceScale");
    const float value = static_cast<float>(fn.arg(0).to_number());
    videoSettings()->setViewDistanceScale(value);                                      // +0x94
}

// getViewDistanceScale(): +0x90's float (`fld dword` at 0x4B1D50)
// widened to double, a number.
void as_videosettings_getViewDistanceScale(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getViewDistanceScale");
    const float value = videoSettings()->getViewDistanceScale();                       // +0x90
    fn.result->set_double(static_cast<double>(value));
}

// setVideoOptionScheme(scheme): argument zero coerced and truncated
// first, the lookup and +0xDC setVideoOptionScheme, then a second lookup and +0x30
// applyVideoOptionScheme with the constant 0 (PE 0x4B1F20 presets the renderer's options with the
// pending scheme and reads them back; it never reads its argument). No result.
void as_videosettings_setVideoOptionScheme(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_setVideoOptionScheme");
    const int scheme = static_cast<int>(fn.arg(0).to_number());
    videoSettings()->setVideoOptionScheme(scheme);                                     // +0xDC
    videoSettings()->applyVideoOptionScheme(0);                                        // +0x30
}

// getVideoOptionScheme(): number of +0xD8, the pending scheme.
void as_videosettings_getVideoOptionScheme(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getVideoOptionScheme");
    publishVideoInt(fn, VS_SLOT(getVideoOptionScheme, 0xD8));
}

// detectVideoScheme(): number of +0x2C detectVideoOptionScheme
// (PE 0x4B1DB0: the renderer options' +0x34 detected level; 1 without a renderer). Nothing is stored.
void as_videosettings_detectVideoScheme(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_detectVideoScheme");
    publishVideoInt(fn, VS_SLOT(detectVideoOptionScheme, 0x2C));
}

void as_videosettings_getTextureFilteringQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getTextureFilteringQuality"); publishVideoInt(fn, VS_SLOT(getTextureFilteringQuality, 0x78)); }
void as_videosettings_setTextureFilteringQuality(const gameswf::fn_call& fn) // PE 1006B260, 162 B, [B]
{ SWF_AS_TRACE("as_videosettings_setTextureFilteringQuality"); setVideoIntArgumentFirst(fn, VS_SLOT(setTextureFilteringQuality, 0x7C)); }

// getSelectedResolution(): instruction for instruction
// getResolution - the unlocalised string of +0x80, not a String object like its getSelected
// siblings. The display-mode select box shows it.
void as_videosettings_getSelectedResolution(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getSelectedResolution");
    publishVideoString(fn, VS_SLOT(getResolution, 0x80));
}

// getSelectedOverallQuality(): the whole shipped body is `retn 0Ch`.
// Its only reference is the ctor's registration (0x1006D541), so it is not an ICF fold of another
// body: the row exists and does nothing - no lookup, no argument, no result, so a caller keeps the
// undefined result gameswf started it with. recovered behaviour, not a stub. No shipped movie calls it.
void as_videosettings_getSelectedOverallQuality(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getSelectedOverallQuality");
    (void) fn;
}

void as_videosettings_getSelectedTerrainQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedTerrainQuality");         publishSelectedName(fn, VS_SLOT(getTerrainQuality, 0x40),          VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedEffectsQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedEffectsQuality");         publishSelectedName(fn, VS_SLOT(getEffectsQuality, 0x68),          VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedGeometryQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedGeometryQuality");        publishSelectedName(fn, VS_SLOT(getGeometryQuality, 0x48),         VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedTexturequality(const gameswf::fn_call& fn)         // PE 1006C3F0, 270 B (shipped spelling)
{ SWF_AS_TRACE("as_videosettings_getSelectedTexturequality");         publishSelectedName(fn, VS_SLOT(getTextureQuality, 0x70),          VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedLightingQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedLightingQuality");        publishSelectedName(fn, VS_SLOT(getLightingQuality, 0x50),         VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedDynamicShadowsQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedDynamicShadowsQuality");  publishSelectedName(fn, VS_SLOT(getDynamicShadowsQuality, 0x60),   VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedDynamicLightingQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedDynamicLightingQuality"); publishSelectedName(fn, VS_SLOT(getDynamicLightingQuality, 0x58),  VS_SLOT(getQualityLevelNames, 0xF0)); }
void as_videosettings_getSelectedVideoOptionScheme(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedVideoOptionScheme");      publishSelectedName(fn, VS_SLOT(getVideoOptionScheme, 0xD8),       VS_SLOT(getVideoOptionSchemeNames, 0xF4)); }

// getSelectedAntialias(): ONE lookup; aa = +0x88 getAntialiasing;
// key = "VIDEOSETTINGS_" + aa; object of the localised key (RendDX9's antialiasing names are "Off"
// and "<n>Samples", which the lexicon carries as VIDEOSETTINGS_Off / VIDEOSETTINGS_2Samples..16);
// the key dies before aa.
void as_videosettings_getSelectedAntialias(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getSelectedAntialias");
    const std::string antialiasing = videoSettings()->getAntialiasing();              // +0x88
    const std::string key = "VIDEOSETTINGS_" + antialiasing;
    fn.result->set_as_object_interface(videoSettingsLocalisedString(key));
}

void as_videosettings_getSelectedTextureFiltering(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_videosettings_getSelectedTextureFiltering");       publishSelectedName(fn, VS_SLOT(getTextureFilteringQuality, 0x78), VS_SLOT(getQualityLevelNames, 0xF0)); }

// needsRestart(): boolean of +0x38 (PE 0x4B1DE0: a game is loaded
// AND the pending record differs). The AS name and the slot do not line up with the interface's
// [INFERRED] names: AS needsRestart is +0x38 hasChangesInGame, AS needsReboot is +0x3C needsRestart.
// No shipped movie calls this row.
void as_videosettings_needsRestart(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_needsRestart");
    publishVideoBool(fn, VS_SLOT(hasChangesInGame, 0x38));
}

// needsReboot(): boolean of +0x3C (PE 0x4B1E00: an antialiasing
// change, or a change to a quality row the renderer flags). tryToApply() reads it before apply, so
// it sees the edits apply is about to commit.
void as_videosettings_needsReboot(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_needsReboot");
    publishVideoBool(fn, VS_SLOT(needsRestart, 0x3C));
}

// getLCDDisplayModes(): NOT the video settings: the row looks up
// "GeneralSettings" (0x100D7A18) by name and publishes as a boolean the byte its +0x15C slot answers
// (PE 0x4B5280, `mov al, [ecx+0F9h]`: m_lcdDisplayModes, persisted in General.con as
// "GeneralSettings.setLCDDisplayModes"). Unguarded. initFlush() hands it to the lcdModes checkbox.
void as_videosettings_getLCDDisplayModes(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_getLCDDisplayModes");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const bool value = static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->getLCDDisplayModes(); // +0x15C
    fn.result->set_bool(value);
}

// Stores argument zero as the LCD-display-mode flag; writes no AS result.
// The current display-mode list reads but discards this flag.
void as_videosettings_setLCDDisplayModes(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_videosettings_setLCDDisplayModes");
    const bool flag = fn.arg(0).to_bool();
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->setLCDDisplayModes(flag); // +0x160
}
#undef VS_SLOT

// Stores the frame label polled by the root movie. Requires player/host and argument zero;
// host lookup precedes string coercion. Writes no AS result.
void as_logic_setFrameLabel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_setFrameLabel");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setFrameLabel(std::string(fn.arg(0).to_string()));
}

// Forwards argument zero as the scroller-enabled flag to host setFlagF10.
// Requires player/host; leaves the AS result untouched.
void as_logic_enableScroller(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_enableScroller");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setFlagF10(fn.arg(0).to_bool());
}

// Stops updates on the current browser list when it exists. Requires player/host.
// Destroys the list-kind string before stopping; writes no AS result.
void as_multiplay_stopBrowser(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_stopBrowser");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list;
    {
        const std::string kind = h->getServerListKind();   // +0x134
        list = h->getServerList(kind);                     // +0x7C
    }                                                      // the shipped body destroys it here
    if (list)
        list->stopUpdate();                                // +0x30
}

// Queues a URL-browse request inside the host busy-cursor bracket.
// Requires player/host and argument zero. Discards the host result and writes no AS result.
void as_multiplay_browse(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_browse");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(classManager->getSingleton(renddx9abi::HostString("SwiffHost")));
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    {
        const std::string url(fn.arg(0).to_string());
        (void) h->requestBrowseUrl(url);
    }
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}

// Stops the VoIP test when its host and client exist. Only the absent-settings
// refusal normally publishes false; early returns leave the ActionScript result alone.
// [FRONTIER] [CHOSEN, not recovered] The unavailable client arm reports and continues
// with that refusal value. Reports retain a hit count and are rate-limited.
// [RETIRE-WHEN: RW-88] Replace the stop-test seam when the client and audio-settings
// calls are implemented and validated through the menu.
static bool s_voipTestRunning = false;
static void voipStopTestFrontier()
{
    // Reachable repeatedly once a Bf2VoipClient exists, so it is rate-limited the way this file's
    // AS-error log is: the first five, then every thousandth, always carrying the count.
    static unsigned s_hits = 0;
    ++s_hits;
    if (s_hits <= 5 || (s_hits % 1000) == 0)
    {
        ::renddx9abi::logf("[FRONTIER] dice::hfe::ui::as_sound_voipStopTest: the Bf2VoipClient arm "
                           "(sub_100BC8B0, the client's +52/+56/+20 and AudioSettings +0x54) is not "
                           "written; answering the shipped refusal, false. Hit #%u.", s_hits);
        ::fprintf(stderr, "[FRONTIER] dice::hfe::ui::as_sound_voipStopTest: the Bf2VoipClient arm is "
                          "not written; answering false (hit #%u)\n", s_hits);
        ::fflush(stderr);
    }
}
void as_sound_voipStopTest(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_sound_voipStopTest");
    if (!s_voipTestRunning)
        return;
    if (!g_swiffPlayerInstance)
        return;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->slot18C(0);
    if (!g_swiffPlayerInstance->getBf2VoipClient())          // +0x8C; absent in this tree
        return;
    if (!classManager->getSingleton(renddx9abi::HostString("AudioSettings")))
    {
        fn.result->set_bool(false);                          // the shipped refusal
        return;
    }
    voipStopTestFrontier();
    fn.result->set_bool(false);
}

// Logs into the numbered online profile after the disc check and returns Boolean success.
// On success, stores the password or an empty string according to remember, then saves.
// Requires player, host, profile manager, and three arguments; brackets the work with busy state.
void as_profile_loginOnlineAccount(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_loginOnlineAccount");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IBase* lpmBase = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    (void) classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(lpmBase);

    bool result = false;
    if (h->checkDisc())
    {
        const int         number   = (int) fn.arg(0).to_number();
        const std::string password = fn.arg(1).to_string();
        const bool        remember = fn.arg(2).to_bool();
        if (lpm->login(number, std::string(""), password))
        {
            lpm->setPassword(remember ? password : std::string(""), true);
            lpm->saveCurrentProfile();
            result = true;
        }
    }
    else
    {
        h->reportNoDisc();
    }
    fn.result->set_bool(result);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}

// Logs in by email or nickname, saves the profile on success, and returns Boolean status.
// Requires the local profile manager and three arguments; brackets the call with busy state.
static void profileLoginOnlineWith(const gameswf::fn_call& fn, bool byNick)
{
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    (void) g_swiffPlayerInstance->getSwiffHost();
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    const std::string name     = fn.arg(0).to_string();
    const std::string password = fn.arg(1).to_string();
    const bool        remember = fn.arg(2).to_bool();
    const bool result = byNick ? lpm->loginOnlineAlternate(name, password, remember)
                               : lpm->loginOnline(name, password, remember);
    if (result)
        lpm->saveCurrentProfile();
    fn.result->set_bool(result);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}
void as_profile_loginOnlineAccountWithEmail(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_loginOnlineAccountWithEmail");
    profileLoginOnlineWith(fn, false);
}
void as_profile_loginOnlineAccountWithNick(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_loginOnlineAccountWithNick");
    profileLoginOnlineWith(fn, true);
}

// Creates an offline profile from name/nickname and returns Boolean status inside a busy bracket.
// Requires player, profile manager, and both arguments.
void as_profile_createOfflineAccount(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_createOfflineAccount");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    (void) g_swiffPlayerInstance->getSwiffHost();
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    const std::string profileName(fn.arg(0).to_string());
    const std::string nick(fn.arg(1).to_string());
    fn.result->set_bool(lpm->createProfile(profileName, nick));
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}

// Logs into a numbered offline profile with empty name/password, returning Boolean status.
// Requires player, profile manager, and argument zero; brackets the call with busy state.
void as_profile_loginOfflineAccount(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_loginOfflineAccount");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    (void) g_swiffPlayerInstance->getSwiffHost();
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    const int number = (int) fn.arg(0).to_number();
    const std::string password("");
    const std::string name("");
    fn.result->set_bool(lpm->login(number, name, password));
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}

// Returns localized COUNTRY_US as a plain UTF-8 string for the initial country selection.
// Requires player and host.
void as_profile_getFirstCountry(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getFirstCountry");
    renddx9abi::HostString key("COUNTRY_US");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::wstring w;
    h->getLocalizedString(w, key);
    tu_string utf8;
    tu_string::encode_utf8_from_wchar(&utf8, reinterpret_cast<const uint16*>(w.c_str()));
    fn.result->set_tu_string(utf8);
}

// Connect-to-IP settings forward directly to the host. Port writes truncate toward zero;
// getters publish number/string values, and setters leave the AS result untouched.
void as_multiplay_getConnectToIpPort(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getConnectToIpPort");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    fn.result->set_double(static_cast<double>(h->getConnectToIpPort()));      // +0x304
}

void as_multiplay_getConnectToIpHost(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getConnectToIpHost");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string host = h->getConnectToIpHost();                          // +0x30C
    fn.result->set_tu_string(tu_string(host.c_str()));
}

void as_multiplay_setConnectToIpPort(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setConnectToIpPort");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setConnectToIpPort(static_cast<int>(fn.arg(0).to_number()));           // +0x308, truncated
}

void as_multiplay_setConnectToIpHost(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setConnectToIpHost");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setConnectToIpHost(std::string(fn.arg(0).to_string()));                // +0x310
}

// The ProfileManager singleton's +48 fills the current player's id (the PlayerStats bodies key
// their host queries on it); absent singleton -> 0, as the shipped bodies' null check leaves it.
static int hostPlayerId()
{
    IBase* pm = classManager ? classManager->getSingleton(renddx9abi::HostString("ProfileManager")) : 0;
    int pid = 0;
    if (pm) (void) static_cast<IProfileManager*>(pm)->getProfileId(pid);   // +0x30 getProfileId(int&)
    return pid;
}

// The 20-byte rank-change record SwiffHost +644(playerId, record&) fills: {time(now), 0, rank,
// promoted, demoted} as sub_10059A80/sub_10059C40 lay it out on their stacks.
struct RankChangeRecord { int now; int zero; int rank; int promoted; int demoted; };
extern "C" __int64 __cdecl _time64(__int64*);

// ISwiffHost +0x284 (getRankInfo). The native host's IPlayerRankInfo::RankInfo
// {timestamp, profileId, rank, change, decrease} is the same five ints in the same order; a record
// the host cannot fill is left untouched, so the caller's stamped record survives a false as shipped.
static bool hostRankInfo(void* h, int playerId, RankChangeRecord& r)
{
    if (!h) return false;
    IPlayerRankInfo::RankInfo info = { r.now, r.zero, r.rank, r.promoted, r.demoted };
    const bool ok = static_cast<ISwiffHost*>(h)->getRankInfo(playerId, &info);
    r.now = info.timestamp; r.zero = info.profileId; r.rank = info.rank; r.promoted = info.change; r.demoted = info.decrease;
    return ok;
}

// ISwiffHost supplies the rank's locale key ("-" when unknown) and next promotion/demotion rank.
// Copy the native string result into the retained internal key storage.
static void hostRankKey(void* h, int rankId, vc71::string& key)
{
    if (!h) return;
    key = static_cast<ISwiffHost*>(h)->getRankName(rankId).c_str();
}
static int hostNextRank(void* h, int rankId)
{
    if (!h) return 0;
    return static_cast<ISwiffHost*>(h)->getRankC(rankId);
}

// Locale.getString through SwiffHost +168 (getLocalizedString) into a wide string - defined with the
// PlayerStats favourites below, used by every localising row.
static void hostLocaliseKey(void* h, const vc71::string& key, vc71::wstring& out);

// sub_10059A80 - dice.bf2.PlayerStats.getPromotedRank(): the record's rank when the host filled it
// and promoted >= 1, else 0.
void as_playerstats_getPromotedRank(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getPromotedRank");
    void* h = asHost();
    const int pid = hostPlayerId();
    RankChangeRecord r = { (int) _time64(0), 0, 0, 0, 0 };
    double v = 0.0;
    if (hostRankInfo(h, pid, r) && r.promoted >= 1)
        v = (double) r.rank;
    fn.result->set_double(v);
}

// sub_10059C40 - dice.bf2.PlayerStats.getDemotedRank(): as above with demoted >= 1.
void as_playerstats_getDemotedRank(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getDemotedRank");
    void* h = asHost();
    const int pid = hostPlayerId();
    RankChangeRecord r = { (int) _time64(0), 0, 0, 0, 0 };
    double v = 0.0;
    if (hostRankInfo(h, pid, r) && r.demoted >= 1)
        v = (double) r.rank;
    fn.result->set_double(v);
}

// sub_10059BA0 / sub_10059D60 - dice.bf2.PlayerStats.getNextPromotionRank(rank) and
// getNextDemomotionRank(rank): both are SwiffHost +444((int) rank) -> int in the shipped image (the
// two registered bodies are byte-identical), kept as shipped.
void as_playerstats_getNextPromotionRank(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getNextPromotionRank");
    void* h = asHost();
    const int rank = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : 0;
    fn.result->set_double(h ? (double) hostNextRank(h, rank) : 0.0);
}
void as_playerstats_getNextDemomotionRank(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getNextDemomotionRank");
    void* h = asHost();
    const int rank = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : 0;
    fn.result->set_double(h ? (double) hostNextRank(h, rank) : 0.0);
}

// sub_10059FC0 - dice.bf2.PlayerStats.getRankString(rankId): SwiffHost +436 constructs the rank's
// localisation key into the caller's raw std::string buffer, Locale (+168) turns it into wide text,
// and the answer is dice's String object carrying that wide text (narrow copy empty).
void as_playerstats_getRankString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getRankString");
    void* h = asHost();
    const int rank = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : 0;
    vc71::wstring text;
    if (h)
    {
        vc71::string key;
        hostRankKey(h, rank, key);
        hostLocaliseKey(h, key, text);
    }
    fn.result->set_as_object_interface(
        gameswf::create_dice_string_object(reinterpret_cast<const Uint16*>(text.c_str()), (int) text.size()));
}

// The player stats record SwiffHost +432(playerId, record&) fills: 3720 bytes, laid out by the
// shipped ctor/dtor pair sub_10020A80 / sub_10020E80 - VC7.1 std::strings at the byte offsets below
// (14 singles, 19 pairs at 528+68k {+0,+40}, 10 at 1820+40k, 7 pairs at 2224+64k {+0,+36}, 13 pairs
// at 2676+68k {+0,+36}, 4 singles), a vector at 3688 (allocator word, first/last/end zeroed), the
// creation time at 3704, every other word zero. Only what the callers read is interpreted: the rank
// id at +160 (getCurrentRankId). The vector at 3688 holds 40-byte elements with a std::string at
// +12 (sub_1001F210 destroys each, then bf_frees the buffer).
struct PlayerStatsRecord
{
    enum { SIZE = 3720 };
    unsigned char bytes[SIZE];
    static void offsets(int* out, int& n)
    {
        static const int singles[] = { 4, 44, 72, 192, 220, 248, 276, 304, 344, 372, 400, 428, 456, 492, 3564, 3596, 3628, 3660 };
        n = 0;
        for (int i = 0; i < (int) (sizeof(singles) / sizeof(singles[0])); ++i) out[n++] = singles[i];
        for (int k = 0; k < 19; ++k) { out[n++] = 528 + 68 * k; out[n++] = 528 + 68 * k + 40; }
        for (int k = 0; k < 10; ++k) { out[n++] = 1820 + 40 * k; }
        for (int k = 0; k < 7; ++k)  { out[n++] = 2224 + 64 * k; out[n++] = 2224 + 64 * k + 36; }
        for (int k = 0; k < 13; ++k) { out[n++] = 2676 + 68 * k; out[n++] = 2676 + 68 * k + 36; }
    }
    PlayerStatsRecord()
    {
        ::memset(bytes, 0, SIZE);
        int off[128]; int n; offsets(off, n);
        for (int i = 0; i < n; ++i) new (bytes + off[i]) vc71::string("");
        *reinterpret_cast<int*>(bytes + 3704) = (int) _time64(0);
    }
    ~PlayerStatsRecord()
    {
        int off[128]; int n; offsets(off, n);
        for (int i = n - 1; i >= 0; --i) reinterpret_cast<vc71::string*>(bytes + off[i])->~string();
        char* first = *reinterpret_cast<char**>(bytes + 3692);
        char* last  = *reinterpret_cast<char**>(bytes + 3696);
        if (first)
        {
            for (char* e = first; e != last; e += 40) reinterpret_cast<vc71::string*>(e + 12)->~string();
            ::renddx9abi::engineFree(first);
        }
    }
};

// The native host's typed SwiffPlayerStatsRecord (Menu/ISwiffHost.h) laid out into the VC7.1 byte
// image the rows below read: the shipped offsets, each string assigned into the image's
// pre-constructed vc71::string. The map rows go into the image's vector at +3688 (40-byte
// {id, wins, losses, time} elements on the engine heap, which the image's destructor frees) since
// getListEntries("playerStats 16") walks them.
static void statsRecordToImage(const dice::hfe::SwiffPlayerStatsRecord& r, PlayerStatsRecord& img)
{
    unsigned char* b = img.bytes;
    struct W
    {
        static void i(unsigned char* b, int off, int v)                { *reinterpret_cast<int*>(b + off) = v; }
        static void s(unsigned char* b, int off, const std::string& v) { *reinterpret_cast<vc71::string*>(b + off) = v.c_str(); }
    };
    W::i(b, 0, r.playerId); W::s(b, 4, r.nick); W::i(b, 32, r.scor); W::i(b, 36, r.wins); W::i(b, 40, r.loss);
    W::s(b, 44, r.jond); W::s(b, 72, r.lbtl);
    W::i(b, 100, r.twsc); W::i(b, 104, r.cpcp); W::i(b, 108, r.cacp); W::i(b, 112, r.dfcp); W::i(b, 116, r.kila);
    W::i(b, 120, r.heal); W::i(b, 124, r.rviv); W::i(b, 128, r.rsup); W::i(b, 132, r.rpar); W::i(b, 136, r.tgte);
    W::i(b, 140, r.dkas_dsab); W::i(b, 156, r.cdsc); W::i(b, 160, r.rank); W::i(b, 168, r.cmsc); W::i(b, 172, r.kick);
    W::i(b, 176, r.ban); W::i(b, 180, r.kill); W::i(b, 184, r.deth); W::i(b, 188, r.suic);
    W::s(b, 192, r.ospm); W::s(b, 220, r.klpm); W::s(b, 248, r.dtpm); W::s(b, 276, r.klpr); W::s(b, 304, r.dtpr);
    W::i(b, 332, r.bksk); W::i(b, 336, r.wdsk); W::i(b, 340, r.bbrs);
    W::s(b, 344, r.tcdr); W::s(b, 372, r.tsql); W::s(b, 400, r.tsqm); W::s(b, 428, r.tlwf);
    W::s(b, 456, r.mvns); W::i(b, 484, r.mvks); W::i(b, 488, r.mvrs);
    W::s(b, 492, r.vmns); W::i(b, 520, r.vmks); W::i(b, 524, r.vmrs);
    for (int k = 0; k < 19; k++)
    {
        const int o = 528 + 68 * k;
        W::s(b, o, r.weapons[k].time); W::i(b, o + 28, r.weapons[k].kills); W::i(b, o + 32, r.weapons[k].deaths);
        W::i(b, o + 36, r.weapons[k].accuracy); W::s(b, o + 40, r.weapons[k].kd);
    }
    for (int k = 0; k < 10; k++)
    {
        const int o = 1820 + 40 * k;
        W::s(b, o, r.armies[k].time); W::i(b, o + 28, r.armies[k].wins); W::i(b, o + 32, r.armies[k].losses);
        W::i(b, o + 36, r.armies[k].bestRound);
    }
    W::i(b, 2220, r.osaa);
    for (int k = 0; k < 7; k++)
    {
        const int o = 2224 + 64 * k;
        W::s(b, o, r.kits[k].time); W::i(b, o + 28, r.kits[k].kills); W::i(b, o + 32, r.kits[k].deaths);
        W::s(b, o + 36, r.kits[k].kd);
    }
    W::i(b, 2672, r.vrk);
    for (int k = 0; k < 13; k++)
    {
        const int o = 2676 + 68 * k;
        W::s(b, o, r.vehicles[k].time); W::i(b, o + 28, r.vehicles[k].kills); W::i(b, o + 32, r.vehicles[k].deaths);
        W::s(b, o + 36, r.vehicles[k].kd); W::i(b, o + 64, r.vehicles[k].roadKills);
    }
    W::i(b, 3560, r.fkit); W::s(b, 3564, r.fkitTime); W::i(b, 3592, r.fveh); W::s(b, 3596, r.fvehTime);
    W::i(b, 3624, r.fwea); W::s(b, 3628, r.fweaTime); W::i(b, 3656, r.fmap); W::s(b, 3660, r.fmapTime);
    W::i(b, 3704, (int) r.fetchedAt); W::i(b, 3708, r.de6); W::i(b, 3712, r.de7); W::i(b, 3716, r.de8);
    const unsigned int maps = static_cast<unsigned int>(r.maps.size());
    if (maps)
    {
        char* first = static_cast<char*>(::renddx9abi::engineAlloc(40 * maps));
        if (first)
        {
            for (unsigned int k = 0; k < maps; ++k)
            {
                char* e = first + 40 * k;
                *reinterpret_cast<int*>(e)     = r.maps[k].id;
                *reinterpret_cast<int*>(e + 4) = r.maps[k].wins;
                *reinterpret_cast<int*>(e + 8) = r.maps[k].losses;
                new (e + 12) vc71::string(r.maps[k].time.c_str());
            }
            *reinterpret_cast<char**>(b + 3692) = first;                // {alloc, first, last, end}
            *reinterpret_cast<char**>(b + 3696) = first + 40 * maps;
            *reinterpret_cast<char**>(b + 3700) = first + 40 * maps;
        }
    }
}

// ISwiffHost +0x21C (getSelectedPlayerId): the stats player every PlayerStats row
// keys its record on (setStatPlayer writes it through +0x220).
static int hostSelectedPlayerId(void* h)
{
    return static_cast<ISwiffHost*>(h)->getSelectedPlayerId();
}

// Copy the typed host's stats record into the internal layout read by the rows below.
// The host leaves a record it cannot fill untouched; that case copies the constructed default.
static bool hostFillStatsRecord(void* h, int id, PlayerStatsRecord& rec)
{
    dice::hfe::SwiffPlayerStatsRecord typed;
    const bool ok = static_cast<dice::hfe::ui::ISwiffHost*>(h)->getPlayerStatsRecord(id, typed);
    statsRecordToImage(typed, rec);
    return ok;
}

// sub_10061070 - dice.bf2.Profile.getCurrentRankId(): SwiffHost +540 names the player, +432 fills
// the stats record, and the answer is the record's rank id at +160.
void as_profile_getCurrentRankId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getCurrentRankId");
    void* h = asHost();
    double v = 0.0;
    if (h)
    {
        const int id = hostSelectedPlayerId(h);
        PlayerStatsRecord* rec = new PlayerStatsRecord();
        (void) hostFillStatsRecord(h, id, *rec);
        v = (double) *reinterpret_cast<int*>(rec->bytes + 160);
        delete rec;
    }
    fn.result->set_double(v);
}

// Retained original message layout: {type, code, text}, with a VC7.1 string.
// Shipped getLastType/getLastText/getLastCode constructed it before the host filled it.
// Active native callbacks exchange the typed SwiffHostMessage instead.
struct HostMessage { int type; int code; vc71::string text; };

// The VC7.1 layout of dice::hfe::ProfileInformation (Game/ProfileManager.h): the profile id, six
// 28-byte strings and four ints, 188 bytes - what BF2.exe 4A4AD0 reads and the shipped row builds
// on its stack (sub_1005CEC0 constructs it: id 0, six empty strings; sub_1005D0D0 destroys it).
struct Vc71ProfileInformation
{
    int profileId;
    vc71::string nick, uniqueNick, email, firstName, lastName, country;
    int sex, birthYear, birthMonth, birthDay;
};

// Creates an online account and local profile from the movie's twelve account arguments.
// Requires initialized host/profile managers. A missing disc, password mismatch or failed
// account/profile creation publishes false. Country "none" becomes "US".
// On success saves profile metadata and the password only when remember is true, then returns true.
void as_profile_createOnlineAccount(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_createOnlineAccount");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(1));
        cursorHost->setBusy(1);
    }
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    bool result = false;
    if (!h->checkDisc())
    {
        h->reportNoDisc();
    }
    else
    {
        ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
            classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
        IProfileManager* pm = static_cast<IProfileManager*>(
            classManager->getSingleton(renddx9abi::HostString("ProfileManager")));
        const std::string nick       = fn.arg(0).to_string();
        const std::string email      = fn.arg(1).to_string();
        const std::string password   = fn.arg(2).to_string();
        const std::string confirm    = fn.arg(3).to_string();
        const bool        remember   = fn.arg(4).to_bool();
        const std::string firstName  = fn.arg(5).to_string();
        const std::string lastName   = fn.arg(6).to_string();
        const int         birthYear  = (int) fn.arg(7).to_number();
        const int         birthMonth = (int) fn.arg(8).to_number();
        const int         birthDay   = (int) fn.arg(9).to_number();
        std::string       country    = fn.arg(10).to_string();
        if (country == "none")
            country = "US";
        const int         sex        = (int) fn.arg(11).to_number();
        if (password != confirm)
        {
            (void) h->postMessage(SwiffHostMessage(1, std::string("password does not match"), 0));
        }
        else if (pm->createOnlineAccount(nick, email, password, country))
        {
            if (lpm->createProfile(nick, nick))
            {
                lpm->setEmail(email);
                lpm->setGamespyNick(nick);
                lpm->setPassword(remember ? password : std::string(""), false);
                lpm->saveCurrentProfile();
                ProfileInformation info;
                info.profileId  = 0;
                info.email      = email;
                info.firstName  = firstName;
                info.lastName   = lastName;
                info.country    = country;
                info.sex        = sex;
                info.birthYear  = birthYear;
                info.birthMonth = birthMonth;
                info.birthDay   = birthDay;
                (void) pm->setProfileInformation(info);
                result = true;
            }
        }
    }
    fn.result->set_bool(result);
    if (cursorHost)
    {
        SetCursor(cursorHost->getCursor(0));
        cursorHost->clearBusy();
    }
}

// Peeks the last message type without removal and publishes it as a number.
// Requires player/host; an empty ring leaves the default type zero, matching a normal message.
void as_messagehandler_getLastType(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_getLastType");
    // The engine owner shares this toolchain's typed SwiffHostMessage and std::string.
    // SwiffHost::popLastMessage copies back() into it and pops only when told to.
    SwiffHostMessage record;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) h->popLastMessage(record, false);
    fn.result->set_double((double) record.type);
}

// Peeks the last message text without removal. Requires player/host;
// an empty ring publishes an empty string.
void as_messagehandler_getLastText(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_getLastText");
    SwiffHostMessage record;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) h->popLastMessage(record, false);
    fn.result->set_tu_string(tu_string(record.text.c_str()));
}

// Peeks without removing the message and publishes its signed 32-bit code as an AS number.
// Requires the player and host; ignores the peek's Boolean return.
void as_messagehandler_getLastCode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_getLastCode");
    SwiffHostMessage record;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) h->popLastMessage(record, false);
    fn.result->set_double((double) record.code);
}

// Removes the last host message, discarding status and leaving the AS result untouched.
// Requires player/host; reads no arguments.
void as_messagehandler_removeLast(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_removeLast");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) h->removeLastMessage();
}

// dice.MessageHandler.clear(), and promoted. Seven
// instructions and no frame, the shape of removeLast: the global player's +0x5C host, then the
// host's +0x1FC clearMessages (BF2.exe 0x494210, the ring emptied and freed), then `retn 0Ch`.
// Nothing is checked, no argument is read and the AS result is never written.
void as_messagehandler_clear(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_clear");
    g_swiffPlayerInstance->getSwiffHost()->clearMessages();                       // +0x1FC
}

// Reads arg 0 before fetching the host, then changes message handling and clears the ring.
// Requires the argument; publishes no AS result.
void as_messagehandler_enableMessageHandler(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_enableMessageHandler");
    const bool enabled = fn.arg(0).to_bool();
    (void) g_swiffPlayerInstance->getSwiffHost()->setMessagesEnabled(enabled);  // +0x20C
}

// dice.MessageHandler.raiseMessage(text, type, code). The body reads all
// three arguments unconditionally and in that order, converts type/code to signed ints by
// truncation, then builds the host record as {type, code, text}. It fetches the global player's host
// without either null guard and calls ISwiffHost::postMessage (+0x1E4); the Boolean return is
// discarded and the AS result is never written. The source text outlives the record, matching the
// shipped destruction order. The host exchanges this toolchain's typed SwiffHostMessage.
void as_messagehandler_raiseMessage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_raiseMessage");
    const std::string text(fn.arg(0).to_string());
    const int type = static_cast<int>(fn.arg(1).to_number());
    const int code = static_cast<int>(fn.arg(2).to_number());

    SwiffHostMessage message;
    message.type = type;
    message.code = code;
    message.text = text;
    ISwiffHost* host = g_swiffPlayerInstance->getSwiffHost();
    (void)host->postMessage(message);
}

// dice.MessageHandler.getPunkBusterMessage(), and
// promoted. The global player's host (unguarded), then its +0x354, and the text of the string it
// answers published as a string - the VC7.1 small-buffer read `cmp [eax+18h],10h` is exactly
// vc71::string::c_str(). No argument, no check. +0x354 is the slot this tree calls getServerMod
// (BF2.exe 0x489060, `lea eax,[ecx+0F30h]`: the host's +0xF30 string, the one +0x350 assigns);
// the shipped reads that same field as the PunkBuster message. [INFERRED] which name is the source's.
void as_messagehandler_getPunkBusterMessage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_messagehandler_getPunkBusterMessage");
    const std::string& text = g_swiffPlayerInstance->getSwiffHost()->getServerMod();   // +0x354
    fn.result->set_tu_string(tu_string(text.c_str()));
}

// Returns the active online nickname when logged in, otherwise the local-profile nickname.
// Requires both profile singletons and the player; host lookup is retained for its side effect.
void as_profile_getActivePlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getActivePlayer");
    (void) asHost();
    IBase* pm  = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    std::string name;
    if (static_cast<IProfileManager*>(pm)->isLoggedIn())
    {
        (void) static_cast<IProfileManager*>(pm)->getUniqueNick(name);
    }
    else
    {
        name = static_cast<ILocalProfileManager*>(lpm)->getNick();
    }
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// The ProfileManager's +0x44 isLoggedIn - the gate every stats-family Profile row
// opens with.
static bool pmIsLoggedIn(IBase* pm)
{
    return static_cast<IProfileManager*>(pm)->isLoggedIn();
}

// The rows' not-logged-in path: the host's +0x244 (handleProfileTransition - the LocalProfileManager
// shut down, the message ring cleared and menu event 14 queued with the text, the same slot
// SwiffPlayer::update feeds) is handed the literal "connection to profile server lost".
static void hostProfileServerLost(void* h)
{
    if (!h) return;
    static_cast<ISwiffHost*>(h)->handleProfileTransition(std::string("connection to profile server lost"));
}

// sub_1005E170 - dice.bf2.Profile.getStatPlayerName(): with the ProfileManager logged in (+68),
// SwiffHost +548 answers the stats player's name (callee-constructed std::string) -> string;
// otherwise the host's +580 is handed the literal "connection to profile server lost" and the
// answer is the empty string.
void as_profile_getStatPlayerName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getStatPlayerName");
    void* h = asHost();
    IBase* pm = classManager ? classManager->getSingleton(renddx9abi::HostString("ProfileManager")) : 0;
    if (pm && pmIsLoggedIn(pm))
    {
        if (!h) { fn.result->set_tu_string(tu_string("")); return; }
        const std::string name = static_cast<ISwiffHost*>(h)->getSelectedPlayerName();   // +0x224
        fn.result->set_tu_string(tu_string(name.c_str()));
        return;
    }
    hostProfileServerLost(h);
    fn.result->set_tu_string(tu_string(""));
}

// sub_1005D290 - dice.bf2.Profile.setStatPlayer(playerId): with the ProfileManager logged in (+68)
// the id (arg0 as a number) goes to SwiffHost +544; otherwise the host's +580 gets the literal
// "connection to profile server lost". No result. The shipped body indexes arg 0 unconditionally
// (an int 3 trap on a missing argument); this one skips the call instead. [CHOSEN, not recovered]
// for that guard only.
void as_profile_setStatPlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setStatPlayer");
    void* h = asHost();
    IBase* pm = classManager ? classManager->getSingleton(renddx9abi::HostString("ProfileManager")) : 0;
    if (!h) return;
    if (pm && pmIsLoggedIn(pm))
    {
        if (fn.nargs < 1) return;
        const int id = (int) fn.arg(0).to_number();
        static_cast<ISwiffHost*>(h)->setSelectedPlayerId(id);   // +0x220
        return;
    }
    hostProfileServerLost(h);
}

// sub_1005DF70 - dice.bf2.Profile.setActivePlayer(nick): with the ProfileManager logged in (+68),
// its +36(std::string&) adopts the nick -> false answers false; true then fetches the
// LocalProfileManager's active profile record (+68, no argument - the disassembly shows the pushed
// string surviving that call for the assignment after it), assigns the nick into the record's
// string at +84 and saves (+64); the answer is true. Not logged in: the host's +580 gets the
// "connection to profile server lost" literal and the answer is false. boolean result.
void as_profile_setActivePlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setActivePlayer");
    void* h = asHost();
    IBase* pm = classManager ? classManager->getSingleton(renddx9abi::HostString("ProfileManager")) : 0;
    bool result = false;
    if (pm && pmIsLoggedIn(pm))
    {
        const std::string nick(fn.nargs >= 1 ? fn.arg(0).to_string() : "");
        if (static_cast<IProfileManager*>(pm)->selectProfile(nick))   // +0x24
        {
            IBase* lpm = classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
            if (lpm)
            {
                LocalProfile* rec = static_cast<ILocalProfileManager*>(lpm)->getCurrentProfile();   // +0x44
                if (rec) rec->gamespyNick = nick;                                                    // retail +0x54
                (void) static_cast<ILocalProfileManager*>(lpm)->saveCurrentProfile();                // +0x40
            }
            result = true;
        }
    }
    else
    {
        hostProfileServerLost(h);
    }
    fn.result->set_bool(result);
}

// sub_1005D600 - dice.bf2.Profile.getNumTimesLoggedIn(): the LocalProfileManager's +144() as a
// number; no singleton -> 0.
void as_profile_getNumTimesLoggedIn(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNumTimesLoggedIn");
    IBase* lpm = classManager ? classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")) : 0;
    int n = 0;
    if (lpm) n = static_cast<ILocalProfileManager*>(lpm)->getNumTimesLoggedIn();   // +0x90
    fn.result->set_double((double) n);
}

// sub_1005D540 - dice.bf2.Profile.getNumRanks(): SwiffHost +448() as a number.
void as_profile_getNumRanks(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNumRanks");
    void* h = asHost();
    const int n = h ? static_cast<ISwiffHost*>(h)->getRankCount() : 0;
    fn.result->set_double((double) n);
}

// Returns whether the logged-in ProfileManager's current profile list contains the supplied id.
// Refetches the current profile at each loop step. If logged out, reports
// "connection to profile server lost" through the host and returns false.
void as_profile_isMyProfile(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_isMyProfile");
    void* h = asHost();
    IBase* pm = classManager ? classManager->getSingleton(renddx9abi::HostString("ProfileManager")) : 0;
    bool result = false;
    if (pm && pmIsLoggedIn(pm))
    {
        const int id = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : 0;
        for (unsigned i = 0; ; ++i)
        {
            OnlineProfile* obj = static_cast<IProfileManager*>(pm)->getCurrentProfile();   // +0x2C, re-fetched per step as shipped
            if (i >= obj->profiles.size()) break;
            if (obj->profiles[i].profileId == id) { result = true; break; }
        }
    }
    else
    {
        hostProfileServerLost(h);
    }
    fn.result->set_bool(result);
}

// Looks up equipment case-insensitively across teams/kits and copies its localization keys.
// Copy native host records into the retained internal key storage.
struct EquipmentKeys { vc71::string name, displayKey, descriptionKey; };
static bool findEquipmentByName(const char* name, EquipmentKeys& out)
{
    if (!classManager) return false;
    IBase* tm = classManager->getSingleton(renddx9abi::HostString("MenuTeamManager"));
    if (!tm) return false;
    IMenuTeamManager* manager = static_cast<IMenuTeamManager*>(tm);
    const int teams = manager->getTeamCount();
    for (int t = 0; t < teams; ++t)
    {
        const MenuTeam* team = manager->getTeam((unsigned) t);
        if (!team) continue;
        for (std::size_t k = 0; k < team->kits.size(); ++k)
        {
            const std::vector<MenuEquipment>& equipment = team->kits[k].equipment;
            for (std::size_t e = 0; e < equipment.size(); ++e)
            {
                const MenuEquipment& rec = equipment[e];
                if (::_stricmp(rec.templateName.c_str(), name) == 0)
                {
                    out.name = rec.templateName.c_str();
                    out.displayKey = rec.displayName.c_str();
                    out.descriptionKey = rec.description.c_str();
                    return true;
                }
            }
        }
    }
    return false;
}

// sub_1005CF10 - the shared tail: Locale.getString through SwiffHost +168 (wide out-param, narrow
// key) into a dice String object (narrow part empty, as shipped - ledger #38).
static void setLocalisedStringObject(const gameswf::fn_call& fn, const vc71::string& key)
{
    vc71::wstring out;
    hostLocaliseKey(asHost(), key, out);
    fn.result->set_as_object_interface(gameswf::create_dice_string_object(reinterpret_cast<const unsigned short*>(out.c_str()), (int) out.size()));
}

// sub_1005F700 - dice.bf2.Profile.getEquipmentName(name): the equipment record's localised display
// name (the key at +56, or +28 - the name itself - when +56 is empty) as a dice String object; an
// unknown name answers an empty String object (still an object, as shipped).
void as_profile_getEquipmentName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getEquipmentName");
    vc71::string name(fn.nargs >= 1 ? fn.arg(0).to_string() : "");
    EquipmentKeys rec;
    if (!findEquipmentByName(name.c_str(), rec)) { fn.result->set_as_object_interface(gameswf::create_dice_string_object(0, 0)); return; }
    setLocalisedStringObject(fn, rec.displayKey.size() ? rec.displayKey : rec.name);
}

// sub_1005F830 - dice.bf2.Profile.getEquipmentDescription(name): the record's description key (+84)
// localised the same way; unknown name -> an empty String object.
void as_profile_getEquipmentDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getEquipmentDescription");
    vc71::string name(fn.nargs >= 1 ? fn.arg(0).to_string() : "");
    EquipmentKeys rec;
    if (!findEquipmentByName(name.c_str(), rec)) { fn.result->set_as_object_interface(gameswf::create_dice_string_object(0, 0)); return; }
    setLocalisedStringObject(fn, rec.descriptionKey);
}

// Returns the current browser-list name. Requires player/host; reads no arguments.
void as_multiplay_getBrowser(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getBrowser");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string kind = h->getServerListKind();                     // +0x134
    fn.result->set_tu_string(tu_string(kind.c_str()));
}

// Keep the original unlock record fields alongside the native host's typed items.
// The original 32-byte record held {time(now), int, count, int, a VC7.1 vector}; shipped callers
// zeroed it, stamped time(), and freed its vector buffer afterwards (sub_1006DBA0 = bf_free).
struct UnlockRecord
{
    int now; int a; int count; int b; Vc71Vector unlocks;
    std::vector<IPlayerUnlocks::Item> items;   // the typed host's {id, status} list; the legacy field stays empty
};

// Read the typed host's unlock entries.
static int unlockCount(const UnlockRecord& rec)
{
    return (int) rec.items.size();
}
static int unlockIdAt(const UnlockRecord& rec, int i)
{
    return rec.items[i].id;
}
static void releaseUnlockRecord(UnlockRecord& rec)
{
    if (rec.unlocks.first) ::renddx9abi::engineFree(rec.unlocks.first);
    rec.unlocks.first = rec.unlocks.last = rec.unlocks.end = 0;
}

// ISwiffHost +0x1C4 (getUnlocks): the ProfileManager's profile id keys the query;
// the native host fills its typed IPlayerUnlocks::UnlockCache {time, id, available, selected, items}
// on success and leaves the caller's record alone otherwise, as the retail slot does.
static void fillUnlockRecord(void* h, UnlockRecord& rec)
{
    const int id = hostPlayerId();
    rec.now = (int) ::time(0); rec.a = 0; rec.count = 0; rec.b = 0;
    rec.unlocks.alloc = 0; rec.unlocks.first = rec.unlocks.last = rec.unlocks.end = 0;
    if (!h) return;
    IPlayerUnlocks::UnlockCache cache;
    cache.field0 = rec.now; cache.field4 = 0; cache.available = 0; cache.selected = 0;
    if (static_cast<ISwiffHost*>(h)->getUnlocks(id, &cache))
    {
        rec.now = cache.field0; rec.a = cache.field4; rec.count = cache.available; rec.b = cache.selected;
        rec.items = cache.items;
    }
}

// sub_1005A8D0 - dice.bf2.PlayerStats.getNumUnlockableItems(): the record's count as a number.
void as_playerstats_getNumUnlockableItems(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getNumUnlockableItems");
    UnlockRecord rec;
    fillUnlockRecord(asHost(), rec);
    fn.result->set_double((double) rec.count);
    releaseUnlockRecord(rec);
}

// sub_1005A0F0 - dice.bf2.PlayerStats.getRankNotification(): SwiffHost +644(playerId, record&) over the
// rank-change record {now, 0, 0, promoted, demoted}; true -> 2 when demoted >= 1, else 1 when
// promoted >= 1, else 0; false -> 0. number.
void as_playerstats_getRankNotification(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getRankNotification");
    void* h = asHost();
    const int id = hostPlayerId();
    RankChangeRecord rec = { (int) ::time(0), 0, 0, 0, 0 };
    int result = 0;
    if (hostRankInfo(h, id, rec))
    {
        if (rec.demoted < 1) { if (rec.promoted >= 1) result = 1; }
        else result = 2;
    }
    fn.result->set_double((double) result);
}

// The three bfhq favourite tables the shipped DLL builds at load (sub_100D5F60 kits, sub_100D5EE0
// vehicles, sub_100D5D80 weapons - std::strings; literals here, the same text): the locale keys
// getFavouriteName resolves by the record's favourite index. The kit and vehicle tables are
// defined ahead of General.getListEntries (they label its end-of-round rows too); the weapon
// table stays here.
static const char* const kFavouriteWeaponKeys[13] = {
    "BFHQ_assaultrifles", "BFHQ_grenadelauncher", "BFHQ_carbines", "BFHQ_lightmachineguns", "BFHQ_sniperrifles",
    "BFHQ_pistols", "BFHQ_ataa", "BFHQ_submachineguns", "BFHQ_shotguns", "BFHQ_knife", "BFHQ_defibrillator",
    "BFHQ_explosives", "BFHQ_handgrenade" };

// SwiffHost +540() names the stats player; +432(id, record&) fills the 3720-byte record and answers
// whether it could (the disassembly of sub_1005B260: the record is pushed first, then the id +540
// returned - getCurrentRankId ignores the bool, the favourites branch on it). The record's favourite
// block: kit index 3560 / its time string 3564, vehicle 3592 / 3596, weapon 3624 / 3628, map 3656 / 3660.
static bool fetchPlayerStats(void* h, PlayerStatsRecord& rec)
{
    if (!h) return false;
    return hostFillStatsRecord(h, hostSelectedPlayerId(h), rec);
}

// sub_100598E0 / sub_1005CF10 - Locale.getString through SwiffHost +168 into a wide string.
static void hostLocaliseKey(void* h, const vc71::string& key, vc71::wstring& out)
{
    if (!h) return;
    std::wstring text;
    (void) static_cast<ISwiffHost*>(h)->getLocalizedString(text, std::string(key.c_str()));
    out = text.c_str();
}

// The map record SwiffHost +288() (getModInfo, the ModInfo singleton) answers from its +52
// (getMapFromGameSpyId): {std::string key at +0, std::string name at +28, ...} - getFavouriteId
// reads the key, getFavouriteName localises the name. Native reads the typed IModInfo::Map
// (path, name - the same two leading strings).
static bool hostMapStrings(void* h, int mapId, vc71::string& key, vc71::string& name)
{
    if (!h) return false;
    IBase* mm = static_cast<ISwiffHost*>(h)->getModInfo();
    const IModInfo::Map* map = mm ? static_cast<IModInfo*>(mm)->getMapFromGameSpyId(mapId) : 0;
    if (!map) return false;
    key = map->path.c_str();
    name = map->name.c_str();
    return true;
}

// sub_1005B260 - dice.bf2.PlayerStats.getFavouriteName(kind): with the stats fetched, a dice String
// object holding the localised favourite - kind 0 kit, 1 vehicle, 2 weapon (the tables above by the
// record's index; index -1 gives the single space the shipped seeds), 3 map (the map record's name,
// or "Unknown Map" localised when the manager has no record; index -1 again the space). Another
// kind leaves the object empty. Stats not fetched: the number 0. The shipped indexes its tables
// unchecked; an index past a table answers the space here. [CHOSEN, not recovered] for that guard.
void as_playerstats_getFavouriteName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getFavouriteName");
    void* h = asHost();
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    if (!fetchPlayerStats(h, *rec)) { fn.result->set_double(0.0); delete rec; return; }
    const int kind = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : -1;
    vc71::wstring text(L" ");
    bool haveText = true;
    const unsigned char* b = rec->bytes;
    switch (kind)
    {
    case 0: { const int i = *reinterpret_cast<const int*>(b + 3560); if (i != -1 && i >= 0 && i < 7)  hostLocaliseKey(h, vc71::string(kFavouriteKitKeys[i]), text); break; }
    case 1: { const int i = *reinterpret_cast<const int*>(b + 3592); if (i != -1 && i >= 0 && i < 7)  hostLocaliseKey(h, vc71::string(kFavouriteVehicleKeys[i]), text); break; }
    case 2: { const int i = *reinterpret_cast<const int*>(b + 3624); if (i != -1 && i >= 0 && i < 13) hostLocaliseKey(h, vc71::string(kFavouriteWeaponKeys[i]), text); break; }
    case 3: {
        const int i = *reinterpret_cast<const int*>(b + 3656);
        if (i != -1)
        {
            vc71::string key, mapName;
            if (hostMapStrings(h, i, key, mapName)) hostLocaliseKey(h, mapName, text);
            else                                    hostLocaliseKey(h, vc71::string("Unknown Map"), text);
        }
        break; }
    default: haveText = false; break;
    }
    delete rec;
    fn.result->set_as_object_interface(gameswf::create_dice_string_object(
        haveText ? reinterpret_cast<const unsigned short*>(text.c_str()) : 0, haveText ? (int) text.size() : 0));
}

// sub_1005B680 - dice.bf2.PlayerStats.getFavouriteId(kind): the favourite index printed with "%d"
// (kinds 0..2), the map record's key string (kind 3, empty without a record), else "" - a string.
// Stats not fetched: the number 0.
void as_playerstats_getFavouriteId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getFavouriteId");
    void* h = asHost();
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    if (!fetchPlayerStats(h, *rec)) { fn.result->set_double(0.0); delete rec; return; }
    const int kind = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : -1;
    const unsigned char* b = rec->bytes;
    char num[32]; num[0] = 0;
    vc71::string out("");
    switch (kind)
    {
    case 0: ::sprintf_s(num, "%d", *reinterpret_cast<const int*>(b + 3560)); out = num; break;
    case 1: ::sprintf_s(num, "%d", *reinterpret_cast<const int*>(b + 3592)); out = num; break;
    case 2: ::sprintf_s(num, "%d", *reinterpret_cast<const int*>(b + 3624)); out = num; break;
    case 3: { vc71::string key, mapName; if (hostMapStrings(h, *reinterpret_cast<const int*>(b + 3656), key, mapName)) out = key; break; }
    default: break;
    }
    delete rec;
    fn.result->set_tu_string(tu_string(out.c_str()));
}

// sub_1005B8A0 - dice.bf2.PlayerStats.getFavouriteTime(kind): the record's time string beside the
// favourite index (3564 / 3596 / 3628 / 3660 for kinds 0..3), else "" - a string. Not fetched: 0.
void as_playerstats_getFavouriteTime(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getFavouriteTime");
    void* h = asHost();
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    if (!fetchPlayerStats(h, *rec)) { fn.result->set_double(0.0); delete rec; return; }
    const int kind = fn.nargs >= 1 ? (int) fn.arg(0).to_number() : -1;
    static const int kTimeOffset[4] = { 3564, 3596, 3628, 3660 };
    vc71::string out("");
    if (kind >= 0 && kind < 4) out = *reinterpret_cast<const vc71::string*>(rec->bytes + kTimeOffset[kind]);
    delete rec;
    fn.result->set_tu_string(tu_string(out.c_str()));
}

// The shipped DLL's selected-unlock name (unk_100F8838, a std::string): written by selectFirstUnlock
// and selectUnlock, read by getCurrentUnlockDescription and unlock.
static vc71::string s_selectedUnlock("");


// bfhq statistics and equipment-list bindings for the selected statistics player.


// dword_100F3238 - the equipment index dice.General.setEquipmentIndex stores and "equipslots"
// compares each slot's itemIndex (+0x80) against. In .data's uninitialised tail: 0 at load.
static int s_equipmentIndex = 0;
// unk_100F88A8 / unk_100F888C - the equipment names Profile.setSelectedEquipment and
// Profile.setSelectedUnlockEquipment store; "wpstats" / "wpunlockstats" read them.
static vc71::string s_selectedEquipment("");
static vc71::string s_selectedUnlockEquipment("");

// sub_10017DC0 - dice.General.setEquipmentIndex(n): arg0 (the trapping accessor) to a number,
// stored by `fistp` under a control word forced to chop (fnstcw / or ah,0Ch / fldcw at
// 10017DDF..10017DF5) - an int truncation. No AS result.
void as_general_setEquipmentIndex(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_general_setEquipmentIndex");
    s_equipmentIndex = (int) fn.arg(0).to_number();
}

// sub_1005D1C0 / sub_1005D1F0 - dice.bf2.Profile.setSelectedEquipment(name) and
// setSelectedUnlockEquipment(name): arg0's to_string assigned to the DLL global. No host, no AS
// result. The unlock page calls the second with the clicked row's id (Sprite465 selectUnlock).
void as_profile_setSelectedEquipment(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setSelectedEquipment");
    s_selectedEquipment = fn.arg(0).to_string();
}
void as_profile_setSelectedUnlockEquipment(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_setSelectedUnlockEquipment");
    s_selectedUnlockEquipment = fn.arg(0).to_string();
}

namespace {

// One labelled row of the playerStats tables: id 0 (sub_1001C5D0(0)), the key localised through
// sub_10013600 as a dice String object (sub_1001CB80), then one value column - a record int printed
// with "%d" (sub_1001CC50) or a record string as it stands (sub_1001CAE0).
struct StatsTable
{
    ListResult& r;
    ListHost h;
    const unsigned char* b;
    int         i(int off) const { return *reinterpret_cast<const int*>(b + off); }
    const char* s(int off) const { return reinterpret_cast<const vc71::string*>(b + off)->c_str(); }
    void label(int id, const char* key)
    {
        r.beginRow(id);
        HostWide w;
        hostLocalise(h, key, w);
        r.pushWide(w.c_str(), (int) w.size());
    }
    void intRow(const char* key, int off) { label(0, key); r.pushInt(i(off)); }
    void strRow(const char* key, int off) { label(0, key); r.pushNarrow(s(off)); }
    // cases 12..15: the favourite index is the row id, "<kind>_%d" of it, then its time string.
    void favouriteRow(const char* format, int indexOff, int timeOff)
    {
        r.beginRow(i(indexOff));
        char text[64];
        ::_snprintf(text, sizeof(text) - 1, format, i(indexOff));
        text[sizeof(text) - 1] = 0;
        r.pushNarrow(text);
        r.pushNarrow(s(timeOff));
    }
};

// unk_100F3758 - the army flag names "playerStats 10" formats into images/bigflag_%s.png, built by
// the static initializer sub_100D6100 and indexed by the team id with no bounds check.
const char* const kArmyFlags[10] = {
    "US", "MEC", "CH", "SEALS", "SAS", "SPETZ", "MECSF", "CHINSURGENT", "MEINSURGENT", "EU" };

// sub_10003580 - the shipped in-place lowercase "unlockeq" keys its name map with: the five Latin-1
// capitals Å Ä Ö Ü É become å ä ö ü é explicitly, every other byte goes through tolower() - which
// under the "C" locale changes A-Z only. [INFERRED] the locale: BF2.exe sets none this DLL sees.
void shippedLowercase(const char* src, vc71::string& out)
{
    const unsigned int n = (unsigned int) ::strlen(src);
    char local[128];
    char* buf = n < sizeof(local) ? local : static_cast<char*>(::renddx9abi::engineAlloc(n + 1));
    for (unsigned int k = 0; k < n; ++k)
    {
        unsigned char c = (unsigned char) src[k];
        if      (c == 0xC5) c = 0xE5;
        else if (c == 0xC4) c = 0xE4;
        else if (c == 0xD6) c = 0xF6;
        else if (c == 0xDC) c = 0xFC;
        else if (c == 0xC9) c = 0xE9;
        else if (c >= 'A' && c <= 'Z') c = (unsigned char) (c + ('a' - 'A'));
        buf[k] = (char) c;
    }
    buf[n] = 0;
    out = buf;
    if (buf != local)
        ::renddx9abi::engineFree(buf);
}

// (The anonymous namespace stays open through listWeaponStats: the list bodies below are the ones
// forward-declared inside the file's first anonymous namespace, ahead of getListEntries.)

// Fetches player statistics and builds the requested table even when fetch reports failure,
// using initialized defaults. Reported totals preserve legacy constants, including 9 for
// the thirteen weapon rows in case 7.
static void listPlayerStats(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                            int start, int end)
{
    (void) classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    const BusyHost busy = busyOpen();
    r.begin();
    const int which = ::atoi(word1);
    const int playerId = hostSelectedPlayerId(h);
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    (void) hostFillStatsRecord(h, playerId, *rec);
    StatsTable t = { r, h, rec->bytes };
    int total = 0;
    switch (which)
    {
    case 0:                                                   // the overview
        t.intRow("BFHQ_overallscore",   32);
        t.intRow("BFHQ_teamworkscore",  100);
        t.intRow("BFHQ_combatscore",    168);
        t.intRow("BFHQ_commanderscore", 156);
        t.strRow("BFHQ_enlisted",       44);
        t.strRow("BFHQ_lastbattle",     72);
        total = 6;
        break;
    case 1:                                                   // teamwork
        t.intRow("BFHQ_teamworkscore",   100);
        t.intRow("BFHQ_capturecp",       104);
        t.intRow("BFHQ_capturecpassist", 108);
        t.intRow("BFHQ_defendcp",        112);
        t.intRow("BFHQ_killassist",      116);
        t.intRow("BFHQ_heal",            120);
        t.intRow("BFHQ_revive",          124);
        t.intRow("BFHQ_support",         128);
        t.intRow("BFHQ_repair",          132);
        t.intRow("BFHQ_driverscore",     140);
        total = 10;
        break;
    case 2:                                                   // combat
        t.intRow("BFHQ_combatscore",     168);
        t.intRow("BFHQ_kills",           180);
        t.intRow("BFHQ_deaths",          184);
        t.strRow("BFHQ_scoreperminute",  192);
        t.strRow("BFHQ_killsperminute",  220);
        t.strRow("BFHQ_deathsperminute", 248);
        t.strRow("BFHQ_killsperround",   276);
        t.strRow("BFHQ_deathsperround",  304);
        t.intRow("BFHQ_killstreak",      332);
        t.intRow("BFHQ_deathstreak",     336);
        total = 10;
        break;
    case 3:                                                   // the personal panel
        t.intRow("BFHQ_STATS_PERSONAL_globalscore",      32);
        t.intRow("BFHQ_STATS_PERSONAL_wins",             36);
        t.intRow("BFHQ_STATS_PERSONAL_loss",             40);
        t.strRow("BFHQ_PROFILES_PLAYERINFO_enlisted",    44);
        t.strRow("BFHQ_PROFILES_PLAYERINFO_lastbattle",  72);
        total = 5;
        break;
    case 4:                                                   // global scores
        t.intRow("BFHQ_globalscore",    32);
        t.intRow("BFHQ_wins",           36);
        t.intRow("BFHQ_losses",         40);
        t.intRow("BFHQ_teamworkscore",  100);
        t.intRow("BFHQ_combatscore",    168);
        t.intRow("BFHQ_commanderscore", 156);
        total = 6;
        break;
    case 7:                                                   // weapons, 68-byte entries at +528
        for (int k = 0; k < 13; ++k)
        {
            const int w = 528 + 68 * k;
            t.label(k, kFavouriteWeaponKeys[k]);
            r.pushInt(t.i(w + 28));                           // kills
            r.pushInt(t.i(w + 32));                           // deaths
            char accuracy[24];                                // sub_10003320("%d") + "%" (sub_10001AA0)
            ::_snprintf(accuracy, sizeof(accuracy) - 1, "%d%%", t.i(w + 36));
            accuracy[sizeof(accuracy) - 1] = 0;
            r.pushNarrow(accuracy);
            r.pushNarrow(t.s(w + 40));                        // kill/death ratio
            r.pushNarrow(t.s(w));                             // time
        }
        total = 9;
        break;
    case 8:                                                   // vehicles, 68-byte entries at +2676
        for (int k = 0; k < 7; ++k)
        {
            if (k == 5)                                       // `cmp esi, 8Ch` - the sixth is skipped, its id kept
                continue;
            const int v = 2676 + 68 * k;
            t.label(k, kFavouriteVehicleKeys[k]);
            r.pushInt(t.i(v + 28));                           // kills
            r.pushInt(t.i(v + 64));                           // road kills
            r.pushInt(t.i(v + 32));                           // deaths
            r.pushNarrow(t.s(v + 36));                        // kill/death ratio
            r.pushNarrow(t.s(v));                             // time
        }
        total = 6;
        break;
    case 9:                                                   // kits, 64-byte entries at +2224
        for (int k = 0; k < 7; ++k)
        {
            const int kit = 2224 + 64 * k;
            t.label(k, kFavouriteKitKeys[k]);
            r.pushInt(t.i(kit + 28));                         // kills
            r.pushInt(t.i(kit + 32));                         // deaths
            r.pushNarrow(t.s(kit + 36));                      // kill/death ratio
            r.pushNarrow(t.s(kit));                           // time
        }
        total = 7;
        break;
    case 10:                                                  // armies, per MenuTeamManager team
    {
        // The count is asked once (var_5C); per team: getTeam, the army's 40-byte entry at
        // 1820 + 40 * teamId (unchecked), the row keyed by the team id, localise("") (an empty key
        // never reaches the host), host +0x124 getModDirectorySetting asked and dropped, the flag
        // image, localise("") again, the team's name (+8) localised, then time / wins / losses.
        IMenuTeamManager* tm = static_cast<IMenuTeamManager*>(
            classManager->getSingleton(renddx9abi::HostString("MenuTeamManager")));
        const unsigned int teams = static_cast<unsigned int>(tm->getTeamCount());           // +0x2C
        for (unsigned int k = 0; k < teams; ++k)
        {
            const MenuTeam* team = tm->getTeam(k);                                          // +0x30
            const int teamId = team->teamId;
            const int army = 1820 + 40 * teamId;
            r.beginRow(teamId);
            { HostWide w; hostLocalise(h, "", w); r.pushWide(w.c_str(), (int) w.size()); }
            (void) h->getModDirectorySetting();                                             // +0x124
            char flag[64];
            ::_snprintf(flag, sizeof(flag) - 1, "images/bigflag_%s.png", kArmyFlags[teamId]);
            flag[sizeof(flag) - 1] = 0;
            r.pushNarrow(flag);
            { HostWide w; hostLocalise(h, "", w); r.pushWide(w.c_str(), (int) w.size()); }
            { HostWide w; hostLocalise(h, team->name.c_str(), w); r.pushWide(w.c_str(), (int) w.size()); }
            r.pushNarrow(t.s(army));
            r.pushInt(t.i(army + 28));
            r.pushInt(t.i(army + 32));
        }
        total = (int) teams;
        break;
    }
    case 11:                                                  // current and next rank
    {
        vc71::string key;
        t.label(0, "PROFILE_currentrank");
        hostRankKey(h, t.i(160), key);                        // +0x1B4 with the record's rank
        r.pushNarrow(key.c_str());
        t.label(0, "PROFILE_nextrank");
        hostRankKey(h, t.i(160) + 1, key);
        r.pushNarrow(key.c_str());
        total = 2;
        break;
    }
    case 12: t.favouriteRow("kit_%d",     3560, 3564); total = 1; break;
    case 13: t.favouriteRow("vehicle_%d", 3592, 3596); total = 1; break;
    case 14: t.favouriteRow("weapon_%d",  3624, 3628); total = 1; break;
    case 15: t.favouriteRow("map_%d",     3656, 3660); total = 1; break;
    case 16:                                                  // the record's map rows
    {
        // Per 40-byte row {id, wins, losses, time} of the vector at +3688 (its size re-read every
        // pass): host +0x120 getModInfo asked again each time, +0x34 getMapFromGameSpyId(id); rows
        // whose map the mod does not know are skipped uncounted. A known map inside the window
        // gives id = the map's gsid (+0x9C), its name (+0x1C), wins, losses, time - and every known
        // map counts toward totalCount whether shown or not.
        const Vc71Vector* rows = reinterpret_cast<const Vc71Vector*>(rec->bytes + 3688);
        unsigned int known = 0;
        for (unsigned int k = 0; rows->first && k < (unsigned int) ((rows->last - rows->first) / 40); ++k)
        {
            const char* row = rows->first + 40 * k;
            const int mapId = *reinterpret_cast<const int*>(row);
            IModInfo* mi = static_cast<IModInfo*>(h->getModInfo());
            const IModInfo::Map* map = mi->getMapFromGameSpyId(mapId);
            if (!map)
                continue;
            if (known >= (unsigned int) start && known < (unsigned int) end)
            {
                r.beginRow(map->gameSpyId);
                r.pushNarrow(map->name.c_str());
                r.pushInt(*reinterpret_cast<const int*>(row + 4));
                r.pushInt(*reinterpret_cast<const int*>(row + 8));
                r.pushNarrow(reinterpret_cast<const vc71::string*>(row + 12)->c_str());
            }
            ++total;
            ++known;
        }
        break;
    }
    case 17:                                                  // the summary strip
        t.intRow("BFHQ_globalscore",        32);
        t.strRow("BFHQ_enlisted",           44);
        t.strRow("BFHQ_lastbattle",         72);
        t.strRow("BFHQ_timeascommander",    344);
        t.strRow("BFHQ_timeassquadleader",  372);
        t.strRow("BFHQ_timeassquadmember",  400);
        t.strRow("BFHQ_timeaslonewolf",     428);
        total = 7;
        break;
    default:                                                  // 5, 6 and out of range: empty
        break;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
    delete rec;
    busyClose(busy);
}

// "profileSelection" (100261F1..100264B9): the builder first, then "ProfileManager" - a missing one
// answers an empty list (totalCount 0, the branch's only guard). Host +0x214 getPendingRecord(&id,
// nick), when it answers true, adds a leading row {id, nick column, "nick"} marked "selected" when
// the id is host +0x21C's, and that row is NOT counted. Then one row per entry of the current
// OnlineProfile's profile list (ProfileManager +0x2C, its vector at +0x38 of 60-byte {profileId,
// nick, uniqueNick}) inside the window, the same shape, every entry counted. The shipped asks +0x2C
// once per use: for the loop test and four times per shown row (id, column, field, selected test).
static void listProfileSelection(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    r.begin();
    IBase* pmBase = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    if (!pmBase)
    {
        r.setTotalCount(0);
        fn.result->set_as_object_interface(r.arr.get_ptr());
        return;
    }
    int total = 0;
    IProfileManager* pm = static_cast<IProfileManager*>(pmBase);
    int pendingId = 0;
    std::string pendingNick;
    if (h->getPendingRecord(&pendingId, pendingNick))                               // +0x214
    {
        r.beginRow(pendingId);
        r.pushNarrow(pendingNick.c_str());
        r.setString("nick", pendingNick.c_str());
        if (pendingId == h->getSelectedPlayerId())                                  // +0x21C
            r.setInt("selected", 1);
    }
    for (unsigned int k = 0; k < (unsigned int) pm->getCurrentProfile()->profiles.size(); ++k)
    {
        if (k >= (unsigned int) start && k < (unsigned int) end)
        {
            r.beginRow(pm->getCurrentProfile()->profiles[k].profileId);
            r.pushNarrow(pm->getCurrentProfile()->profiles[k].nick.c_str());
            r.setString("nick", pm->getCurrentProfile()->profiles[k].nick.c_str());
            if (pm->getCurrentProfile()->profiles[k].profileId == h->getSelectedPlayerId())
                r.setInt("selected", 1);
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// Walk the typed MenuTeamManager teams, kits, and equipment for unlock lists.
typedef IMenuTeamManager* TeamManager;
typedef const MenuTeam*   TeamRef;
typedef const MenuKit*    KitRef;
typedef const MenuEquipment* EquipmentRef;
TeamManager  teamManager()                        { return static_cast<IMenuTeamManager*>(classManager->getSingleton(renddx9abi::HostString("MenuTeamManager"))); }
unsigned int teamCount(TeamManager tm)            { return (unsigned int) tm->getTeamCount(); }
TeamRef      teamAt(TeamManager tm, unsigned int k) { return tm->getTeam(k); }
TeamRef      teamById(TeamManager tm, int id)     { return tm->findTeamById(id); }
int          teamId(TeamRef team)                 { return team->teamId; }
const char*  teamName(TeamRef team)               { return team->name.c_str(); }
unsigned int kitCount(TeamRef team)               { return (unsigned int) team->kits.size(); }
KitRef       kitAt(TeamRef team, unsigned int k)  { return &team->kits[k]; }
int          kitIndex(KitRef kit)                 { return (int) kit->index; }
const char*  kitTemplate(KitRef kit)              { return kit->templateName.c_str(); }
const char*  kitHud(KitRef kit)                   { return kit->hudName.c_str(); }
unsigned int equipmentCount(KitRef kit)           { return (unsigned int) kit->equipment.size(); }
EquipmentRef equipmentAt(KitRef kit, unsigned int k) { return &kit->equipment[k]; }
const char*  equipmentTemplate(EquipmentRef e)    { return e->templateName.c_str(); }
const char*  equipmentDisplay(EquipmentRef e)     { return e->displayName.c_str(); }
int          equipmentUnlockLevel(EquipmentRef e) { return e->unlockLevel; }
int          equipmentItemIndex(EquipmentRef e)   { return e->itemIndex; }
int          equipmentType(EquipmentRef e)        { return e->equipmentType; }
int          equipmentStat(EquipmentRef e, int off) { return off == 0x70 ? e->stat70 : off == 0x74 ? e->stat74 : e->stat78; }
EquipmentRef findEquipment(TeamManager tm, const char* name) { return tm->findEquipment(std::string(name)); }   // +0x28

// A localised key pushed as a dice String object (sub_10013600 + sub_1001CB80).
void pushLocalised(ListResult& r, ListHost h, const char* key)
{
    HostWide w;
    hostLocalise(h, key, w);
    r.pushWide(w.c_str(), (int) w.size());
}

// "teams" (100264EA..10026616): "MenuTeamManager" before the builder; the count asked before the
// loop and again after every pass (unsigned); a team inside the window is asked for (+0x30) and
// gives the row id (its teamId) and its localised name (+8); every team counts.
static void listTeams(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    TeamManager tm = teamManager();
    r.begin();
    int total = 0;
    for (unsigned int k = 0; k < teamCount(tm); ++k)
    {
        if (k >= (unsigned int) start && k < (unsigned int) end)
        {
            TeamRef team = teamAt(tm, k);
            r.beginRow(teamId(team));
            pushLocalised(r, h, teamName(team));
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "kits <teamId> <selected>" (10026616..100267CC): the team by ID (+0x34 findTeamById, unguarded);
// per kit inside the window: id = the kit's index, its HUD name (+0x3C) localised, "kitid" = its
// template name (+0x20), "selected" when its position equals the third word; every kit counts.
static void listKits(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                     const char* word2, int start, int end)
{
    TeamManager tm = teamManager();
    r.begin();
    const int id = ::atoi(word1);
    const int selected = ::atoi(word2);
    TeamRef team = teamById(tm, id);
    int total = 0;
    for (unsigned int k = 0; k < kitCount(team); ++k)
    {
        if (k >= (unsigned int) start && k < (unsigned int) end)
        {
            KitRef kit = kitAt(team, k);
            r.beginRow(kitIndex(kit));
            pushLocalised(r, h, kitHud(kit));
            r.setString("kitid", kitTemplate(kit));
            if ((int) k == selected)
                r.setInt("selected", 1);
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "equipments <teamIndex> <kit>" (100267CC..1002692B): the team by index this time (+0x30), its kit
// by position; per equipment record inside the window: id = its position, its display name (+0x38)
// localised; every record counts.
static void listEquipments(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                           const char* word2, int start, int end)
{
    TeamManager tm = teamManager();
    r.begin();
    const int teamIndex = ::atoi(word1);
    const int kitPosition = ::atoi(word2);
    KitRef kit = kitAt(teamAt(tm, (unsigned int) teamIndex), (unsigned int) kitPosition);
    int total = 0;
    for (unsigned int k = 0; k < equipmentCount(kit); ++k)
    {
        if (k >= (unsigned int) start && k < (unsigned int) end)
        {
            r.beginRow((int) k);
            pushLocalised(r, h, equipmentDisplay(equipmentAt(kit, k)));
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "equipslots <teamId> <kit>" (1002C70C..1002C9C0): the kit's eight weapon slots, NO window. For
// slot s = 0..7 the first record with itemIndex (+0x80) == s + 1 AND unlockLevel (+0x7C) == 0 - the
// kit's default item - becomes a row keyed by its template name (+0x1C, sub_1001C770), its display
// name localised, "slot" s, "available" 1, "weapon" = (equipmentType (+0x84) == 0), "selected" =
// (setEquipmentIndex's value == its itemIndex) - written both ways, 1 or 0.
static void listEquipSlots(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                           const char* word2)
{
    TeamManager tm = teamManager();
    r.begin();
    const int id = ::atoi(word1);
    const int kitPosition = ::atoi(word2);
    KitRef kit = kitAt(teamById(tm, id), (unsigned int) kitPosition);
    int total = 0;
    for (int slot = 0; slot < 8; ++slot)
    {
        const unsigned int count = equipmentCount(kit);
        for (unsigned int k = 0; k < count; ++k)
        {
            EquipmentRef e = equipmentAt(kit, k);
            if (equipmentItemIndex(e) != slot + 1 || equipmentUnlockLevel(e) != 0)
                continue;
            r.beginRowStr(equipmentTemplate(e));
            pushLocalised(r, h, equipmentDisplay(e));
            r.setInt("slot", slot);
            r.setInt("available", 1);
            r.setInt("weapon", equipmentType(e) == 0 ? 1 : 0);
            r.setInt("selected", s_equipmentIndex == equipmentItemIndex(e) ? 1 : 0);
            ++total;
            break;
        }
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// Lists unlockable kit equipment and marks unlock status for the selected statistics player.
// [CHOSEN, not recovered] A flat case-folded name map replaces the ordered map;
// later duplicate names overwrite earlier IDs.
static void listUnlockEquipment(const gameswf::fn_call& fn, ListResult& r, ListHost h, const char* word1,
                                const char* word2, int start, int end)
{
    r.begin();
    TeamManager tm = teamManager();
    UnlockRecord rec;
    rec.now = (int) ::time(0); rec.a = 0; rec.count = 0; rec.b = 0;
    rec.unlocks.alloc = 0; rec.unlocks.first = rec.unlocks.last = rec.unlocks.end = 0;
    const int teamIdentifier = ::atoi(word1);
    const int kitPosition = ::atoi(word2);
    struct NameId { vc71::string name; int id; };
    NameId* names = 0;
    unsigned int nameCount = 0;
    {
        IPlayerUnlocks::UnlockCache cache;
        cache.field0 = rec.now; cache.field4 = 0; cache.available = 0; cache.selected = 0;
        if (h->getUnlocks(h->getSelectedPlayerId(), &cache))                            // +0x21C, +0x1C4
        {
            rec.now = cache.field0; rec.a = cache.field4; rec.count = cache.available; rec.b = cache.selected;
            rec.items = cache.items;
        }
    }
    IPlayerUnlockInfo* info = static_cast<IPlayerUnlockInfo*>(h->getPlayerUnlockInfo()); // +0x48
    std::vector<IPlayerUnlockInfo::Item> items;
    (void) info->getItemsByKit((unsigned int) kitPosition, items);                      // +0x14
    if (!items.empty())
    {
        names = new NameId[items.size()];
        for (std::size_t k = 0; k < items.size(); ++k)
        {
            vc71::string key;
            shippedLowercase(items[k].name.c_str(), key);
            unsigned int slot = 0;
            while (slot < nameCount && ::strcmp(names[slot].name.c_str(), key.c_str()) != 0) ++slot;
            if (slot == nameCount) { names[slot].name = key.c_str(); ++nameCount; }
            names[slot].id = (int) items[k].id;
        }
    }
    KitRef kit = kitAt(teamById(tm, teamIdentifier), (unsigned int) kitPosition);
    int total = 0;
    for (unsigned int k = 0; k < equipmentCount(kit); ++k)
    {
        EquipmentRef e = equipmentAt(kit, k);
        if (!e || equipmentUnlockLevel(e) <= 0)
            continue;
        if (k >= (unsigned int) start && k < (unsigned int) end)
        {
            vc71::string key;
            shippedLowercase(equipmentTemplate(e), key);
            r.beginRowStr(key.c_str());
            pushLocalised(r, h, equipmentDisplay(e));
            r.setInt("slot", (int) k);
            r.setInt("available", 1);
            r.setInt("weapon", equipmentType(e) == 0 ? 1 : 0);
            int unlocked = 0;
            for (unsigned int n = 0; n < nameCount; ++n)
            {
                if (::strcmp(names[n].name.c_str(), key.c_str()) != 0)
                    continue;
                for (int u = 0; u < unlockCount(rec); ++u)
                {
                    const int status = rec.items[u].status;
                    if (unlockIdAt(rec, u) == names[n].id && status == 1)
                    {
                        unlocked = 1;
                        break;
                    }
                }
                break;
            }
            r.setInt("unlocked", unlocked);
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
    delete[] names;
    releaseUnlockRecord(rec);
}

// "wpstats" / "wpunlockstats" / "wpuunlockstats" (1002C9C3..1002CD40): "MenuTeamManager" asked and
// dropped, the builder, then the name - the selected equipment, or for "wpunlockstats" the selected
// unlock equipment, or for "wpuunlockstats" the selected unlock (the order of the re-tests at
// 1002CAC4 / 1002CAF5). An empty name answers the empty list; otherwise MenuTeamManager +0x28
// findEquipment(name), and a record gives three rows id 0..2: the narrow labels "Damage",
// "Accuracy", "Rate of fire" with its +0x70 / +0x74 / +0x78 ints and "available" 1 - the stat names
// MenuTeamManager::addWeapon's three arguments carry. No record: totalCount 0.
static void listWeaponStats(const gameswf::fn_call& fn, ListResult& r, const tu_string& which)
{
    (void) teamManager();                                   // asked and dropped
    r.begin();
    vc71::string name(s_selectedEquipment.c_str());
    if (which == "wpunlockstats")
        name = s_selectedUnlockEquipment.c_str();
    else if (which == "wpuunlockstats")
        name = s_selectedUnlock.c_str();
    int total = 0;
    if (!name.empty())
    {
        EquipmentRef e = findEquipment(teamManager(), name.c_str());   // sub_100135B0 looks it up again
        if (e)
        {
            static const char* const kLabels[3] = { "Damage", "Accuracy", "Rate of fire" };
            static const int kOffsets[3] = { 0x70, 0x74, 0x78 };
            for (int k = 0; k < 3; ++k)
            {
                r.beginRow(k);
                r.pushNarrow(kLabels[k]);
                r.pushInt(equipmentStat(e, kOffsets[k]));
                r.setInt("available", 1);
            }
            total = 3;
        }
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

} // anonymous namespace (opened above StatsTable)

// sub_1005AAB0 - dice.bf2.PlayerStats.selectFirstUnlock(): the unlock record (+452) in hand, with at
// least one unlock the host's +72() object lists its 64-byte {id, int, std::string name at +8,
// std::string at +36} entries through +28(vector&); the entry whose id equals the first unlock's id
// gives its name to the selected-unlock string (the disassembly: the record's +8 string is the
// operator= source). No unlocks: the name is cleared. Both vectors are freed on the engine heap
// (sub_1006DBA0 = bf_free) after their strings are destroyed. No result.
void as_playerstats_selectFirstUnlock(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_selectFirstUnlock");
    void* h = asHost();
    UnlockRecord rec;
    fillUnlockRecord(h, rec);
    const int nUnlocks = unlockCount(rec);
    if (nUnlocks != 0 && h)
    {
        const int firstId = unlockIdAt(rec, 0);
        // +0x48 getPlayerUnlockInfo, its +0x1C getItems(vector<Item>&): the typed {id, kit, name, description}.
        IBase* obj = static_cast<ISwiffHost*>(h)->getPlayerUnlockInfo();
        std::vector<IPlayerUnlockInfo::Item> items;
        if (obj) (void) static_cast<IPlayerUnlockInfo*>(obj)->getItems(items);
        for (std::size_t i = 0; i < items.size(); ++i)
        {
            if ((int) items[i].id == firstId) { s_selectedUnlock = items[i].name.c_str(); break; }
        }
    }
    else
    {
        s_selectedUnlock = "";
    }
    releaseUnlockRecord(rec);
}

// sub_10059A20 - dice.bf2.PlayerStats.selectUnlock(name): the player's host fetched and discarded,
// then the argument (indexed unconditionally, as shipped) becomes the selected-unlock name. No result.
void as_playerstats_selectUnlock(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_selectUnlock");
    (void) asHost();
    s_selectedUnlock = fn.arg(0).to_string();
}

// sub_1005A2B0 - dice.bf2.PlayerStats.unlock(unlockId): the argument (a number, indexed
// unconditionally) and the ProfileManager's profile id go to the host's PlayerUnlocks object (+0x38)
// as setUnlocks(profileId, unlockId) - its +0x18, the setunlocks.aspx post; the host's +0x28C
// (clearLastFetchId) then drops the cached unlock record's id so the next query refetches. boolean:
// the post's answer. The host is unguarded, as shipped.
void as_playerstats_unlock(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_unlock");
    void* h = asHost();
    const int unlockId = (int) fn.arg(0).to_number();
    const int pid = hostPlayerId();
    IBase* unlocks = static_cast<ISwiffHost*>(h)->getPlayerUnlocks();
    const bool result = static_cast<IPlayerUnlocks*>(unlocks)->setUnlocks(pid, unlockId);
    static_cast<ISwiffHost*>(h)->clearLastFetchId();
    fn.result->set_bool(result);
}

// sub_100599B0 - dice.bf2.PlayerStats.clearRankNotification(): the ProfileManager's profile id to
// the host's +0x288 (clearRankNotification: PlayerRankInfo clears it, the stats cache is reset and
// the cached rank row dropped). The host's Boolean is not published - the shipped body sets no
// result. The host is unguarded, as shipped.
void as_playerstats_clearRankNotification(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_clearRankNotification");
    void* h = asHost();
    const int pid = hostPlayerId();
    (void) static_cast<ISwiffHost*>(h)->clearRankNotification(pid);
}

// Singleplayer settings bindings resolve their singleton and forward argument zero.
// They leave the AS result untouched.


// sub_10032030 - dice.bf2.GeneralSettings.setBotSkill(value): GeneralSettings +0xC4 (PE 0x4B1C90,
// the float at +0xAC) with to_number(arg 0) as a float.
void as_generalsettings_setBotSkill(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setBotSkill");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const float value = (float) fn.arg(0).to_number();
    static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->setBotSkill(value);
}

// sub_10031EB0 - dice.bf2.GeneralSettings.setUseBots(flag): old = +0xA8 getUseBots; +0xAC
// setUseBots(to_bool(arg 0)); when the flag changed, the player's host (+0x5C) +0xF4 - the entry
// vector's clear, so a map list built for the other setting never survives the switch.
void as_generalsettings_setUseBots(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setUseBots");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    GeneralSettings* settings = static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs));
    const bool old = settings->getUseBots();
    const bool flag = fn.arg(0).to_bool();
    settings->setUseBots(flag);
    if (old != flag)
        g_swiffPlayerInstance->getSwiffHost()->clearServerEntries();                       // +0xF4
}

// sub_10031FC0 - dice.bf2.GeneralSettings.setMaxBotsIncludeHumans(flag): +0xBC (PE 0x4B7340, the
// byte at +0xA8) with to_bool(arg 0).
void as_generalsettings_setMaxBotsIncludeHumans(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setMaxBotsIncludeHumans");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const bool flag = fn.arg(0).to_bool();
    static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->setMaxBotsIncludeHumans(flag);
}

// End-of-round settings use the required GeneralSettings singleton.
// apply leaves the AS result untouched; auto-ready forwards to the owner's demo-aware accessors.
void as_generalsettings_apply(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_apply");
    (void) fn;
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    (void) static_cast<IGeneralSettings*>(gs)->apply();
}

void as_generalsettings_setAutoReady(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setAutoReady");
    const bool value = fn.arg(0).to_bool();
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    static_cast<IGeneralSettings*>(gs)->setAutoReady(value);                               // +0xFC
}

void as_generalsettings_getAutoReady(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_getAutoReady");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const bool value = static_cast<IGeneralSettings*>(gs)->getAutoReady();                 // +0xF8
    fn.result->set_bool(value);
}


// General settings bindings resolve the singleton before coercing argument zero.
// Requires the singleton; numeric integer values truncate toward zero.

static IBase* generalSettingsSingleton()
{
    return classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
}
typedef bool        (IGeneralSettings::*GsBoolGetter)() const;
typedef int         (IGeneralSettings::*GsIntGetter)() const;
typedef float       (IGeneralSettings::*GsFloatGetter)() const;
typedef std::string (IGeneralSettings::*GsStringGetter)() const;
typedef void        (IGeneralSettings::*GsBoolSetter)(bool);
typedef void        (IGeneralSettings::*GsIntSetter)(int);
typedef void        (IGeneralSettings::*GsFloatSetter)(float);
typedef void        (IGeneralSettings::*GsStringSetter)(std::string);
// Rows call typed native members; offset arguments retain the original slot reference.
#define GS_SLOT(member, offset) (&IGeneralSettings::member)
static IGeneralSettings* generalSettingsOf(IBase* gs) { return static_cast<IGeneralSettings*>(gs); }

// flt_100DAD24 (0x3B808081): the float nearest 1/255, which the transparency getters multiply by -
// widened, not the exact double reciprocal. flt_100DAD20 is 255.0f and flt_100D7EA0 1.0f.
static const float kGsUnitPerByte = 0.003921568859368563f;

// The Boolean getters (getMinimapRotate 10033690 and siblings, 128 B each): the lookup, the slot's AL
// through movzx, published as a boolean.
static void publishGeneralBool(const gameswf::fn_call& fn, GsBoolGetter getter)
{
    const bool value = (generalSettingsOf(generalSettingsSingleton())->*getter)();
    fn.result->set_bool(value);
}

// The int getters (getSortOrder 10033300 and siblings): the lookup, the slot's int through fild - exact.
static void publishGeneralInt(const gameswf::fn_call& fn, GsIntGetter getter)
{
    const int value = (generalSettingsOf(generalSettingsSingleton())->*getter)();
    fn.result->set_double(static_cast<double>(value));
}

// getBotSkill / getCoopBotRatio (10033040 / 100330C0): the slot's float straight from st0 into the
// number's double - exact.
static void publishGeneralFloat(const gameswf::fn_call& fn, GsFloatGetter getter)
{
    const float value = (generalSettingsOf(generalSettingsSingleton())->*getter)();
    fn.result->set_double(static_cast<double>(value));
}

// The HUD / minimap / map-icon transparency getters (10033840 / 100337B0 / 10033720, 132 B each): the
// stored alpha (0..255) back to the menu's 0..1 transparency - `fmul flt_100DAD24; fsubr
// flt_100D7EA0`: 1.0 - value * (1/255 as a float), rounded once more to double. Both float operands
// are exact in a double, so the product is exact and only the subtraction rounds, as under the
// shipped 53-bit control word.
static void publishGeneralTransparency(const gameswf::fn_call& fn, GsFloatGetter getter)
{
    const float value = (generalSettingsOf(generalSettingsSingleton())->*getter)();
    fn.result->set_double(1.0 - static_cast<double>(value) * static_cast<double>(kGsUnitPerByte));
}

// getSortKey / getServerFilter / getBFTVSaveDirectory (10033260 / 10033400 / 100339F0, 158 B each):
// the lookup, the slot's std::string (callee-constructed hidden buffer), its c_str() as a string, the
// string destroyed after the publication.
static void publishGeneralString(const gameswf::fn_call& fn, GsStringGetter getter)
{
    const std::string value = (generalSettingsOf(generalSettingsSingleton())->*getter)();
    fn.result->set_tu_string(tu_string(value.c_str()));
}

// Boolean setters preserve per-binding argument/owner lookup order and publish no result.
static void setGeneralBoolOwnerFirst(const gameswf::fn_call& fn, GsBoolSetter setter)
{
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const bool value = fn.arg(0).to_bool();
    (gs->*setter)(value);
}
static void setGeneralBoolArgumentFirst(const gameswf::fn_call& fn, GsBoolSetter setter)
{
    const bool value = fn.arg(0).to_bool();
    (generalSettingsOf(generalSettingsSingleton())->*setter)(value);
}

// Integer setters preserve per-binding argument/owner lookup order and publish no result.
static void setGeneralIntOwnerFirst(const gameswf::fn_call& fn, GsIntSetter setter)
{
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const int value = static_cast<int>(fn.arg(0).to_number());
    (gs->*setter)(value);
}
static void setGeneralIntArgumentFirst(const gameswf::fn_call& fn, GsIntSetter setter)
{
    const int value = static_cast<int>(fn.arg(0).to_number());
    (generalSettingsOf(generalSettingsSingleton())->*setter)(value);
}

// The transparency setters (setHUDTransparency 10032800, setMinimapTransparency 10032790,
// setMapIconTransparency 10032720, 105 B each): the lookup, then the menu's 0..1 transparency back
// to a stored alpha - `fmul flt_100DAD20; fsubr flt_100DAD20; fstp dword`: 255 - x * 255, each step
// rounded to double, the whole rounded once to float. No result.
static void setGeneralTransparency(const gameswf::fn_call& fn, GsFloatSetter setter)
{
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const float value = static_cast<float>(255.0 - fn.arg(0).to_number() * 255.0);
    (gs->*setter)(value);
}

// The string setters (setSortKey 100321F0, setServerFilter 10032370, setBFTVSaveDirectory 10032940,
// 122 B each): the lookup, arg 0's text into a string, a copy of it built in the argument area for
// the by-value slot (which destroys it), then the original destroyed. No result.
static void setGeneralString(const gameswf::fn_call& fn, GsStringSetter setter)
{
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const std::string text(fn.arg(0).to_string());
    (gs->*setter)(text);
}

// the page's own rows

// The tail GeneralSettings.reset and the four ControlSettings commit rows share, inlined in each
// shipped body: "LocalProfileManager" by name - null-tested - and, only when its +0x44 current
// profile exists, its +0x40 saveCurrentProfile, the Boolean dropped.
static void saveLocalProfileIfCurrent()
{
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    if (lpm && lpm->getCurrentProfile())                                              // +0x44
        (void) lpm->saveCurrentProfile();                                             // +0x40
}

// reset(): the owner's +0x20 setDefaultValues (4B6AB0 - the option values only; the
// lists, bots and awards date are setDefaults' +0x1C, not touched here), then the profile save above.
// No argument, no result.
void as_generalsettings_reset(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_reset");
    (void) fn;
    generalSettingsOf(generalSettingsSingleton())->setDefaultValues();                // +0x20
    saveLocalProfileIfCurrent();
}

// cancel(): the owner's +0x24 (4B7540: the profile's General.con re-read, nine values
// kept - Game/GeneralSettings.cpp). No argument, no result.
void as_generalsettings_cancel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_cancel");
    (void) fn;
    generalSettingsOf(generalSettingsSingleton())->cancel();                          // +0x24
}

void as_generalsettings_getSortKey(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_generalsettings_getSortKey");          publishGeneralString(fn, GS_SLOT(getSortKey, 0x28)); }
void as_generalsettings_setSortKey(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_generalsettings_setSortKey");          setGeneralString(fn, GS_SLOT(setSortKey, 0x2C)); }
void as_generalsettings_getSortOrder(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_generalsettings_getSortOrder");        publishGeneralInt(fn, GS_SLOT(getSortOrder, 0x30)); }
void as_generalsettings_setSortOrder(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_generalsettings_setSortOrder");        setGeneralIntOwnerFirst(fn, GS_SLOT(setSortOrder, 0x34)); }
void as_generalsettings_getNumRoundsPlayed(const gameswf::fn_call& fn)  { SWF_AS_TRACE("as_generalsettings_getNumRoundsPlayed");  publishGeneralInt(fn, GS_SLOT(getNumRoundsPlayed, 0x38)); }
void as_generalsettings_setNumRoundsPlayed(const gameswf::fn_call& fn)  { SWF_AS_TRACE("as_generalsettings_setNumRoundsPlayed");  setGeneralIntOwnerFirst(fn, GS_SLOT(setNumRoundsPlayed, 0x3C)); }
void as_generalsettings_getServerFilter(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_generalsettings_getServerFilter");     publishGeneralString(fn, GS_SLOT(getServerFilter, 0x40)); }
void as_generalsettings_setServerFilter(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_generalsettings_setServerFilter");     setGeneralString(fn, GS_SLOT(setServerFilter, 0x44)); }
void as_generalsettings_getHUDTransparency(const gameswf::fn_call& fn)  { SWF_AS_TRACE("as_generalsettings_getHUDTransparency");  publishGeneralTransparency(fn, GS_SLOT(getHUDTransparency, 0x48)); }
void as_generalsettings_setHUDTransparency(const gameswf::fn_call& fn)  { SWF_AS_TRACE("as_generalsettings_setHUDTransparency");  setGeneralTransparency(fn, GS_SLOT(setHUDTransparency, 0x4C)); }

// getCrosshairColor(): the owner's +0x50 record (4B7230, the four ints copied out), then
// ONE packed number built from the LOW byte of each component - `movzx` of +0x0C (alpha), +0x00 (red),
// +0x04 (green), +0x08 (blue), shifted in by eights - so 0xAARRGGBB read back through fild as a signed
// 32-bit int: an alpha of 128 or more gives a negative number (the default 255, 255, 0, 204 is

void as_generalsettings_getCrosshairColor(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_getCrosshairColor");
    int red, green, blue, alpha;
    const GeneralSettingsRgba c = generalSettingsOf(generalSettingsSingleton())->getCrosshairColor(); // +0x50
    red = c.red; green = c.green; blue = c.blue; alpha = c.alpha;
    const unsigned int packed = ((static_cast<unsigned int>(alpha) & 0xFF) << 24) |
                                ((static_cast<unsigned int>(red)   & 0xFF) << 16) |
                                ((static_cast<unsigned int>(green) & 0xFF) << 8)  |
                                 (static_cast<unsigned int>(blue)  & 0xFF);
    fn.result->set_double(static_cast<double>(static_cast<int>(packed)));
}

// setCrosshairColor(r, g, b, a): the lookup, then four arguments through the bounds-checked
// accessor in order 0, 1, 2, 3, each truncated to int, into +0x54 (4B73F0). No result.
void as_generalsettings_setCrosshairColor(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setCrosshairColor");
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const int red = static_cast<int>(fn.arg(0).to_number());
    const int green = static_cast<int>(fn.arg(1).to_number());
    const int blue = static_cast<int>(fn.arg(2).to_number());
    const int alpha = static_cast<int>(fn.arg(3).to_number());
    gs->setCrosshairColor(red, green, blue, alpha);                                    // +0x54
}

// getBuddytagColor() / getSquadtagColor(): the +0x58 / +0x60 record (three ints),
// packed from the low bytes as 0xRRGGBB (`mov dh` of red, then green and blue shifted in) - never
// negative. Shared by the two rows.
static void publishGeneralRgb(const gameswf::fn_call& fn, int offset)
{
    int red, green, blue;
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const GeneralSettingsRgb c = offset == 0x58 ? gs->getBuddytagColor() : gs->getSquadtagColor();
    red = c.red; green = c.green; blue = c.blue;
    const int packed = ((red & 0xFF) << 16) | ((green & 0xFF) << 8) | (blue & 0xFF);
    fn.result->set_double(static_cast<double>(packed));
}
void as_generalsettings_getBuddytagColor(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_getBuddytagColor"); publishGeneralRgb(fn, 0x58); }
void as_generalsettings_getSquadtagColor(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_getSquadtagColor"); publishGeneralRgb(fn, 0x60); }

// setBuddytagColor(r, g, b) / setSquadtagColor(r, g, b): the lookup, arguments
// 0, 1, 2 truncated in order, into +0x5C / +0x64 (4B7420 / 4B7440). No result.
static void setGeneralRgb(const gameswf::fn_call& fn, int offset)
{
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const int red = static_cast<int>(fn.arg(0).to_number());
    const int green = static_cast<int>(fn.arg(1).to_number());
    const int blue = static_cast<int>(fn.arg(2).to_number());
    if (offset == 0x5C) gs->setBuddytagColor(red, green, blue);                        // +0x5C
    else                gs->setSquadtagColor(red, green, blue);                        // +0x64
}
void as_generalsettings_setBuddytagColor(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_setBuddytagColor"); setGeneralRgb(fn, 0x5C); }
void as_generalsettings_setSquadtagColor(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_setSquadtagColor"); setGeneralRgb(fn, 0x64); }

void as_generalsettings_getMinimapRotate(const gameswf::fn_call& fn)       { SWF_AS_TRACE("as_generalsettings_getMinimapRotate");       publishGeneralBool(fn, GS_SLOT(getMinimapRotate, 0x68)); }
void as_generalsettings_setMinimapRotate(const gameswf::fn_call& fn)       { SWF_AS_TRACE("as_generalsettings_setMinimapRotate");       setGeneralBoolOwnerFirst(fn, GS_SLOT(setMinimapRotate, 0x6C)); }
void as_generalsettings_getMinimapTransparency(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_getMinimapTransparency"); publishGeneralTransparency(fn, GS_SLOT(getMinimapTransparency, 0x70)); }
void as_generalsettings_setMinimapTransparency(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_setMinimapTransparency"); setGeneralTransparency(fn, GS_SLOT(setMinimapTransparency, 0x74)); }
void as_generalsettings_getViewIntroMovie(const gameswf::fn_call& fn)      { SWF_AS_TRACE("as_generalsettings_getViewIntroMovie");      publishGeneralBool(fn, GS_SLOT(getViewIntroMovie, 0x78)); }
void as_generalsettings_setViewIntroMovie(const gameswf::fn_call& fn)      { SWF_AS_TRACE("as_generalsettings_setViewIntroMovie");      setGeneralBoolOwnerFirst(fn, GS_SLOT(setViewIntroMovie, 0x7C)); }
void as_generalsettings_getOutOfVoting(const gameswf::fn_call& fn)         { SWF_AS_TRACE("as_generalsettings_getOutOfVoting");         publishGeneralBool(fn, GS_SLOT(getOutOfVoting, 0x80)); }
void as_generalsettings_setOutOfVoting(const gameswf::fn_call& fn)         { SWF_AS_TRACE("as_generalsettings_setOutOfVoting");         setGeneralBoolOwnerFirst(fn, GS_SLOT(setOutOfVoting, 0x84)); }
void as_generalsettings_getBFTVSaveDirectory(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_generalsettings_getBFTVSaveDirectory");   publishGeneralString(fn, GS_SLOT(getBFTVSaveDirectory, 0x88)); }
void as_generalsettings_setBFTVSaveDirectory(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_generalsettings_setBFTVSaveDirectory");   setGeneralString(fn, GS_SLOT(setBFTVSaveDirectory, 0x8C)); }

// setPlayedVOHelp(name): the lookup, arg 0's text into a string, and +0x114 (4B76F0, the
// case-insensitive add-if-absent) with it BY reference - unlike the three by-value string setters.
void as_generalsettings_setPlayedVOHelp(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setPlayedVOHelp");
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const std::string name(fn.arg(0).to_string());
    gs->setPlayedVOHelp(name);                                                         // +0x114
}

// checkPlayedVOHelp(name): the lookup, arg 0's text into a string, and +0x118's Boolean
// (4B62C0, the case-insensitive find) published as a boolean; the string dies after the publication.
void as_generalsettings_checkPlayedVOHelp(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_checkPlayedVOHelp");
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const std::string name(fn.arg(0).to_string());
    fn.result->set_bool(gs->checkPlayedVOHelp(name));                                 // +0x118
}

void as_generalsettings_getConfirmQuit(const gameswf::fn_call& fn)         { SWF_AS_TRACE("as_generalsettings_getConfirmQuit");         publishGeneralBool(fn, GS_SLOT(getConfirmQuit, 0x90)); }
void as_generalsettings_setConfirmQuit(const gameswf::fn_call& fn)         { SWF_AS_TRACE("as_generalsettings_setConfirmQuit");         setGeneralBoolOwnerFirst(fn, GS_SLOT(setConfirmQuit, 0x94)); }
void as_generalsettings_getMapIconTransparency(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_getMapIconTransparency"); publishGeneralTransparency(fn, GS_SLOT(getMapIconAlphaTransparency, 0x98)); }
void as_generalsettings_setMapIconTransparency(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_setMapIconTransparency"); setGeneralTransparency(fn, GS_SLOT(setMapIconAlphaTransparency, 0x9C)); }
void as_generalsettings_getUseBots(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_generalsettings_getUseBots");             publishGeneralBool(fn, GS_SLOT(getUseBots, 0xA8)); }
void as_generalsettings_getMaxBots(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_generalsettings_getMaxBots");             publishGeneralInt(fn, GS_SLOT(getMaxBots, 0xB0)); }
void as_generalsettings_setMaxBots(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_generalsettings_setMaxBots");             setGeneralIntOwnerFirst(fn, GS_SLOT(setMaxBots, 0xB4)); }
void as_generalsettings_getMaxBotsIncludeHumans(const gameswf::fn_call& fn){ SWF_AS_TRACE("as_generalsettings_getMaxBotsIncludeHumans"); publishGeneralBool(fn, GS_SLOT(getMaxBotsIncludeHumans, 0xB8)); }
void as_generalsettings_getBotSkill(const gameswf::fn_call& fn)            { SWF_AS_TRACE("as_generalsettings_getBotSkill");            publishGeneralFloat(fn, GS_SLOT(getBotSkill, 0xC0)); }
void as_generalsettings_getLastAwardsCheckDate(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_getLastAwardsCheckDate"); publishGeneralInt(fn, GS_SLOT(getLastAwardsCheckDate, 0xC8)); }

// updateLastAwardsCheckDate(): "GeneralSettings" looked up and the answer discarded, the
// 32-bit CRT time taken, the singleton looked up again and +0xCC (4B1910) handed the time. No argument,
// no result.
void as_generalsettings_updateLastAwardsCheckDate(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_updateLastAwardsCheckDate");
    (void) fn;
    (void) generalSettingsSingleton();
    __time32_t now;
    ::_time32(&now);
    generalSettingsOf(generalSettingsSingleton())->setLastAwardsCheckDate(static_cast<int>(now));   // +0xCC
}

void as_generalsettings_getAllowPunkBuster(const gameswf::fn_call& fn)           { SWF_AS_TRACE("as_generalsettings_getAllowPunkBuster");           publishGeneralBool(fn, GS_SLOT(getAllowPunkBuster, 0xD0)); }
void as_generalsettings_setAllowPunkBuster(const gameswf::fn_call& fn)           { SWF_AS_TRACE("as_generalsettings_setAllowPunkBuster");           setGeneralBoolArgumentFirst(fn, GS_SLOT(setAllowPunkBuster, 0xD4)); }
void as_generalsettings_getItemSelectionReverseItems(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_getItemSelectionReverseItems"); publishGeneralBool(fn, GS_SLOT(getItemSelectionReverseItems, 0xD8)); }
void as_generalsettings_setItemSelectionReverseItems(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_generalsettings_setItemSelectionReverseItems"); setGeneralBoolArgumentFirst(fn, GS_SLOT(setItemSelectionReverseItems, 0xDC)); }
void as_generalsettings_getAutoReload(const gameswf::fn_call& fn)                { SWF_AS_TRACE("as_generalsettings_getAutoReload");                publishGeneralBool(fn, GS_SLOT(getAutoReload, 0xE0)); }
void as_generalsettings_setAutoReload(const gameswf::fn_call& fn)                { SWF_AS_TRACE("as_generalsettings_setAutoReload");                setGeneralBoolArgumentFirst(fn, GS_SLOT(setAutoReload, 0xE4)); }
void as_generalsettings_getCoopBotRatio(const gameswf::fn_call& fn)              { SWF_AS_TRACE("as_generalsettings_getCoopBotRatio");              publishGeneralFloat(fn, GS_SLOT(getCoopBotRatio, 0xE8)); }
void as_generalsettings_getCoopBotCount(const gameswf::fn_call& fn)              { SWF_AS_TRACE("as_generalsettings_getCoopBotCount");              publishGeneralInt(fn, GS_SLOT(getCoopBotCount, 0xF0)); }
void as_generalsettings_setCoopBotCount(const gameswf::fn_call& fn)              { SWF_AS_TRACE("as_generalsettings_setCoopBotCount");              setGeneralIntOwnerFirst(fn, GS_SLOT(setCoopBotCount, 0xF4)); }
void as_generalsettings_getConnectionType(const gameswf::fn_call& fn)            { SWF_AS_TRACE("as_generalsettings_getConnectionType");            publishGeneralInt(fn, GS_SLOT(getConnectionType, 0x100)); }
void as_generalsettings_setConnectionType(const gameswf::fn_call& fn)            { SWF_AS_TRACE("as_generalsettings_setConnectionType");            setGeneralIntArgumentFirst(fn, GS_SLOT(setConnectionType, 0x104)); }

// setCoopBotRatio(value): the lookup, to_number(arg 0) rounded to float (`fstp dword`),
// +0xEC (4B1930). No result.
void as_generalsettings_setCoopBotRatio(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_setCoopBotRatio");
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const float value = static_cast<float>(fn.arg(0).to_number());
    gs->setCoopBotRatio(value);                                                        // +0xEC
}

// removeServer(id): the lookup, then the row id "<address> <queryPort> <hostPort>" split by
// sub_100508D0 (sscanf "%s %d %d" into a 260-byte buffer, the address assigned, the first number
// the port); then a favourite is removed if +0x134 finds it (+0x130), otherwise a history row if
// +0x150 finds it (+0x14C). The address string and the id die in that order. No result.
// The shipped buffer takes an unbounded %s and the two numbers stay uninitialised when the id holds
// fewer than two; the bounded width and the zeroes here are the same [CHOSEN] narrowing joinServer's
// split makes (sub_100508D0's other caller).
void as_generalsettings_removeServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_generalsettings_removeServer");
    IGeneralSettings* gs = generalSettingsOf(generalSettingsSingleton());
    const std::string id(fn.arg(0).to_string());
    std::string address;
    char buf[260] = {0};
    int port = 0, second = 0;
    ::sscanf(id.c_str(), "%259s %d %d", buf, &port, &second);
    address = buf;
    if (gs->findFavouriteServer(address, port))                                        // +0x134
        (void) gs->removeFavouriteServer(address, port);                               // +0x130
    else if (gs->findServerHistory(address, port))                                     // +0x150
        (void) gs->removeServerHistory(address, port);                                 // +0x14C
}


// Control settings edit backup maps and restore the prior engine selection afterward.
// mapInput remains unregistered until input capture and its integer status contract are
// implemented; publishing the current Boolean stub would report ineffective rebindings.

static IBase* controlSettingsSingleton()
{
    return classManager->getSingleton(renddx9abi::HostString("ControlSettings"));
}
static IControlSettings* controlSettingsOf(IBase* cs) { return static_cast<IControlSettings*>(cs); }
typedef float (IControlSettings::*CsFloatGetter)();
typedef void  (IControlSettings::*CsFloatSetter)(float);
#define CS_SLOT(member, offset) (&IControlSettings::member)

// 100F3158 (stride 28): the seven backup maps the page edits, in the shipped static initializer's
// order (100D1780); setActiveControlMap's argument indexes it unchecked. The shipped table holds
// std::strings; plain literals here reach the same text and keep the DLL's static init free of heap
// work.
static const char* const kEditedControlMapNames[7] = {
    "_InfantryPlayerInputControlMap", "_LandPlayerInputControlMap", "_AirPlayerInputControlMap",
    "_HelicopterPlayerInputControlMap", "_SeaPlayerInputControlMap", "_defaultGameControlMap",
    "_defaultPlayerInputControlMap",
};
static renddx9abi::HostString s_editedControlMap;   // unk_100F3138: the map setActiveControlMap picked ("" at load)
static renddx9abi::HostString s_savedControlMap;    // unk_100F321C: the engine's selection, kept around a feel call

// sub_1000E250: the engine's active map name read through +0x3C and kept, then the edited map selected
// through +0x34 - two lookups, both unguarded, the selection's Boolean dropped.
static void selectEditedControlMap()
{
    s_savedControlMap = controlSettingsOf(controlSettingsSingleton())->getActiveControlMapName();  // +0x3C
    (void) controlSettingsOf(controlSettingsSingleton())->selectControlMap(s_editedControlMap);     // +0x34
}

// sub_1000E2E0: when a selection was kept, it is selected again (+0x34, one more lookup); the kept
// name is emptied either way.
static void restoreControlMapSelection()
{
    if (!s_savedControlMap.empty())
    {
        (void) controlSettingsOf(controlSettingsSingleton())->selectControlMap(s_savedControlMap);  // +0x34
    }
    s_savedControlMap = "";
}

// The feel getters (getMouseSensitivity 1000E6E0 and siblings): the edited map selected, the lookup,
// the slot's float into the number's double (exact), the selection restored after the publication.
static void publishControlFloat(const gameswf::fn_call& fn, CsFloatGetter getter)
{
    selectEditedControlMap();
    const float value = (controlSettingsOf(controlSettingsSingleton())->*getter)();
    fn.result->set_double(static_cast<double>(value));
    restoreControlMapSelection();
}

// Selects the edited control map, sets a float value, then restores the map; publishes no result.
// Each binding preserves its argument/owner lookup order.
static void setControlFloatOwnerFirst(const gameswf::fn_call& fn, CsFloatSetter setter)
{
    selectEditedControlMap();
    IControlSettings* cs = controlSettingsOf(controlSettingsSingleton());
    const float value = static_cast<float>(fn.arg(0).to_number());
    (cs->*setter)(value);
    restoreControlMapSelection();
}
static void setControlFloatArgumentFirst(const gameswf::fn_call& fn, CsFloatSetter setter)
{
    selectEditedControlMap();
    const float value = static_cast<float>(fn.arg(0).to_number());
    (controlSettingsOf(controlSettingsSingleton())->*setter)(value);
    restoreControlMapSelection();
}

void as_controlsettings_getMouseSensitivity(const gameswf::fn_call& fn)           { SWF_AS_TRACE("as_controlsettings_getMouseSensitivity");           publishControlFloat(fn, CS_SLOT(getMouseSensitivity, 0x40)); }
void as_controlsettings_setMouseSensitivity(const gameswf::fn_call& fn)           { SWF_AS_TRACE("as_controlsettings_setMouseSensitivity");           setControlFloatArgumentFirst(fn, CS_SLOT(setMouseSensitivity, 0x44)); }
void as_controlsettings_getMouseSensitivityMultiplier(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_controlsettings_getMouseSensitivityMultiplier"); publishControlFloat(fn, CS_SLOT(getSlot48, 0x48)); }
void as_controlsettings_setMouseSensitivityMultiplier(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_controlsettings_setMouseSensitivityMultiplier"); setControlFloatOwnerFirst(fn, CS_SLOT(setSlot4C, 0x4C)); }
void as_controlsettings_getMouseSmoothing(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_controlsettings_getMouseSmoothing");             publishControlFloat(fn, CS_SLOT(getSlot50, 0x50)); }
void as_controlsettings_setMouseSmoothing(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_controlsettings_setMouseSmoothing");             setControlFloatOwnerFirst(fn, CS_SLOT(setSlot54, 0x54)); }
void as_controlsettings_getMouseYAWFactor(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_controlsettings_getMouseYAWFactor");             publishControlFloat(fn, CS_SLOT(getYawFactor, 0x58)); }
void as_controlsettings_setMouseYAWFactor(const gameswf::fn_call& fn)             { SWF_AS_TRACE("as_controlsettings_setMouseYAWFactor");             setControlFloatOwnerFirst(fn, CS_SLOT(setYawFactor, 0x5C)); }
void as_controlsettings_getMousePITCHFactor(const gameswf::fn_call& fn)           { SWF_AS_TRACE("as_controlsettings_getMousePITCHFactor");           publishControlFloat(fn, CS_SLOT(getPitchFactor, 0x60)); }
void as_controlsettings_setMousePITCHFactor(const gameswf::fn_call& fn)           { SWF_AS_TRACE("as_controlsettings_setMousePITCHFactor");           setControlFloatOwnerFirst(fn, CS_SLOT(setPitchFactor, 0x64)); }
void as_controlsettings_getKeyboardSensitivity(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_controlsettings_getKeyboardSensitivity");        publishControlFloat(fn, CS_SLOT(getKeyboardSensitivity, 0x70)); }
void as_controlsettings_setKeyboardSensitivity(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_controlsettings_setKeyboardSensitivity");        setControlFloatOwnerFirst(fn, CS_SLOT(setKeyboardSensitivity, 0x74)); }

// getInvertMouse() / setInvertMouse(flag): the Boolean twins of the feel
// rows - +0x68 through movzx into a boolean; to_bool(arg 0) read before the lookup, into +0x6C.
void as_controlsettings_getInvertMouse(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_controlsettings_getInvertMouse");
    selectEditedControlMap();
    const bool value = controlSettingsOf(controlSettingsSingleton())->getInvertMouse();   // +0x68
    fn.result->set_bool(value);
    restoreControlMapSelection();
}
void as_controlsettings_setInvertMouse(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_controlsettings_setInvertMouse");
    selectEditedControlMap();
    const bool value = fn.arg(0).to_bool();
    controlSettingsOf(controlSettingsSingleton())->setInvertMouse(value);                 // +0x6C
    restoreControlMapSelection();
}

// setActiveControlMap(index): the player's host fetched and discarded, to_number(arg 0)
// truncated, and the edited map set to the table's entry at that index - unchecked, as shipped. No
// ControlSettings call and no result: the choice only takes effect through the bracket above.
void as_controlsettings_setActiveControlMap(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_controlsettings_setActiveControlMap");
    (void) g_swiffPlayerInstance->getSwiffHost();
    const int index = static_cast<int>(fn.arg(0).to_number());
    s_editedControlMap = kEditedControlMapNames[index];
}

// getActiveControlMap(): the lookup and +0x3C getActiveControlMapName (4B2C20: the engine'S
// active map, "" without one - not the page's edited map) published as a string.
void as_controlsettings_getActiveControlMap(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_controlsettings_getActiveControlMap");
    const std::string name = controlSettingsOf(controlSettingsSingleton())->getActiveControlMapName();   // +0x3C
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// apply() / cancel() / reset() / resetDefault(): one
// ControlSettings slot, its answer dropped, then the profile save. apply +0x18 (4B2F30: the live maps
// reset and copied from the edited backups - the commit); cancel +0x20 (4B2FC0: the backups rebuilt
// from the live maps - the edits discarded); reset and resetDefault both +0x24 (4B3230: the live maps
// reset, Settings/Controls.con re-run, the backups rebuilt) - the two rows are byte-identical. No
// argument, no result.
static void controlSettingsCommit(int offset)
{
    IControlSettings* cs = controlSettingsOf(controlSettingsSingleton());
    if (offset == 0x18)      (void) cs->restoreControlMaps();                       // +0x18
    else if (offset == 0x20) cs->backupControlMapsByName();                         // +0x20
    else                     (void) cs->setDefaults();                              // +0x24
    saveLocalProfileIfCurrent();
}
void as_controlsettings_apply(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_controlsettings_apply");        (void) fn; controlSettingsCommit(0x18); }
void as_controlsettings_cancel(const gameswf::fn_call& fn)       { SWF_AS_TRACE("as_controlsettings_cancel");       (void) fn; controlSettingsCommit(0x20); }
void as_controlsettings_reset(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_controlsettings_reset");        (void) fn; controlSettingsCommit(0x24); }
void as_controlsettings_resetDefault(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_controlsettings_resetDefault"); (void) fn; controlSettingsCommit(0x24); }

// Returns a dice String describing the requested binding in the edited control map.
// Input -1 and CONTROLLER_None produce empty text; other keys are localized.
// Restores the previous map selection after lookup.
void as_controlsettings_getMappedString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_controlsettings_getMappedString");
    const int input = static_cast<int>(fn.arg(0).to_number());
    const bool alternate = fn.arg(1).to_bool();
    const bool second = fn.arg(2).to_bool();
    renddx9abi::HostString key;
    HostWide text;
    gameswf::as_object_interface* object = gameswf::create_dice_string_object(nullptr, 0);
    if (input != -1)
    {
        selectEditedControlMap();
        const bool gameInput = ::strcmp(s_editedControlMap.c_str(), "_defaultGameControlMap") == 0;
        (void) controlSettingsOf(controlSettingsSingleton())->getBindingDescription(
            nullptr, input, alternate, second, gameInput, key, text);                  // +0x38
        if (!key.empty())
        {
            if (::_stricmp(key.c_str(), "CONTROLLER_None") == 0)
                gameswf::assign_dice_string_object_narrow(object, "");
            else
            {
                HostWide localised;
                (void) g_swiffPlayerInstance->getSwiffHost()->getLocalizedString(localised, key);   // +0xA8
                gameswf::assign_dice_string_object_wide(object, reinterpret_cast<const Uint16*>(localised.c_str()));
            }
        }
        else
            gameswf::assign_dice_string_object_wide(object, reinterpret_cast<const Uint16*>(text.c_str()));
        restoreControlMapSelection();
    }
    fn.result->set_as_object_interface(object);
}


// Audio setters change pending settings; apply promotes them. Requires the audio singleton.
// Legacy voice/detail placeholder getters return "Remove Me" and their setters do nothing.

static IBase* audioSettingsSingleton()
{
    return classManager->getSingleton(renddx9abi::HostString("AudioSettings"));
}
static IAudioSettings* audioSettingsOf(IBase* as) { return static_cast<IAudioSettings*>(as); }
typedef bool  (IAudioSettings::*AsBoolGetter)() const;
typedef bool  (IAudioSettings::*AsStateGetter)();
typedef float (IAudioSettings::*AsFloatGetter)() const;
typedef void  (IAudioSettings::*AsBoolSetter)(bool);
typedef void  (IAudioSettings::*AsFloatSetter)(float);
#define AS_SLOT(member, offset) (&IAudioSettings::member)

// The Boolean rows: the lookup, the slot's AL, a boolean. The pending-record getters are const, the
// state queries (canApply +0x1C, needsRestart +0x28, hasEax +0x2C, hasZenithBoard +0x30) are not.
static void publishAudioBool(const gameswf::fn_call& fn, AsBoolGetter getter)
{
    const bool value = (audioSettingsOf(audioSettingsSingleton())->*getter)();
    fn.result->set_bool(value);
}
static void publishAudioState(const gameswf::fn_call& fn, AsStateGetter getter)
{
    const bool value = (audioSettingsOf(audioSettingsSingleton())->*getter)();
    fn.result->set_bool(value);
}
// The float getters: the slot's float into the number's double, exact.
static void publishAudioFloat(const gameswf::fn_call& fn, AsFloatGetter getter)
{
    const float value = (audioSettingsOf(audioSettingsSingleton())->*getter)();
    fn.result->set_double(static_cast<double>(value));
}
// ownerFirst controls whether settings lookup precedes conversion of arg 0.
static void setAudioBool(const gameswf::fn_call& fn, AsBoolSetter setter, bool ownerFirst)
{
    if (ownerFirst)
    {
        IAudioSettings* as = audioSettingsOf(audioSettingsSingleton());
        const bool value = fn.arg(0).to_bool();
        (as->*setter)(value);
        return;
    }
    const bool value = fn.arg(0).to_bool();
    (audioSettingsOf(audioSettingsSingleton())->*setter)(value);
}
// Converts arg 0 to float; ownerFirst controls its order relative to settings lookup.
static void setAudioFloat(const gameswf::fn_call& fn, AsFloatSetter setter, bool ownerFirst)
{
    if (ownerFirst)
    {
        IAudioSettings* as = audioSettingsOf(audioSettingsSingleton());
        const float value = static_cast<float>(fn.arg(0).to_number());
        (as->*setter)(value);
        return;
    }
    const float value = static_cast<float>(fn.arg(0).to_number());
    (audioSettingsOf(audioSettingsSingleton())->*setter)(value);
}

void as_audiosettings_getVoipEnabled(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_getVoipEnabled");          publishAudioBool(fn, AS_SLOT(getVoipEnabled, 0x38)); }
void as_audiosettings_setVoipEnabled(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_setVoipEnabled");          setAudioBool(fn, AS_SLOT(setVoipEnabled, 0x3C), true); }
void as_audiosettings_getVoipPlaybackVolume(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_audiosettings_getVoipPlaybackVolume");   publishAudioFloat(fn, AS_SLOT(getVoipPlaybackVolume, 0x40)); }
void as_audiosettings_setVoipPlaybackVolume(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_audiosettings_setVoipPlaybackVolume");   setAudioFloat(fn, AS_SLOT(setVoipPlaybackVolume, 0x44), true); }
void as_audiosettings_getVoipCaptureVolume(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_getVoipCaptureVolume");    publishAudioFloat(fn, AS_SLOT(getVoipCaptureVolume, 0x48)); }
void as_audiosettings_setVoipCaptureVolume(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_setVoipCaptureVolume");    setAudioFloat(fn, AS_SLOT(setVoipCaptureVolume, 0x4C), true); }
void as_audiosettings_getVoipCaptureThreshold(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_audiosettings_getVoipCaptureThreshold"); publishAudioFloat(fn, AS_SLOT(getVoipCaptureThreshold, 0x50)); }
void as_audiosettings_setVoipCaptureThreshold(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_audiosettings_setVoipCaptureThreshold"); setAudioFloat(fn, AS_SLOT(setVoipCaptureThreshold, 0x54), true); }
void as_audiosettings_getVoipBoostEnabled(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_audiosettings_getVoipBoostEnabled");     publishAudioBool(fn, AS_SLOT(getVoipBoostEnabled, 0x58)); }
void as_audiosettings_setVoipBoostEnabled(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_audiosettings_setVoipBoostEnabled");     setAudioBool(fn, AS_SLOT(setVoipBoostEnabled, 0x5C), true); }
void as_audiosettings_getVoipUsePushToTalk(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_getVoipUsePushToTalk");    publishAudioBool(fn, AS_SLOT(getVoipUsePushToTalk, 0x60)); }
void as_audiosettings_setVoipUsePushToTalk(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_setVoipUsePushToTalk");    setAudioBool(fn, AS_SLOT(setVoipUsePushToTalk, 0x64), true); }
void as_audiosettings_getEffectsVolume(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_audiosettings_getEffectsVolume");        publishAudioFloat(fn, AS_SLOT(getEffectsVolume, 0x78)); }
void as_audiosettings_setEffectsVolume(const gameswf::fn_call& fn)        { SWF_AS_TRACE("as_audiosettings_setEffectsVolume");        setAudioFloat(fn, AS_SLOT(setEffectsVolume, 0x7C), false); }
void as_audiosettings_getMusicVolume(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_getMusicVolume");          publishAudioFloat(fn, AS_SLOT(getMusicVolume, 0x80)); }
void as_audiosettings_setMusicVolume(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_setMusicVolume");          setAudioFloat(fn, AS_SLOT(setMusicVolume, 0x84), false); }
void as_audiosettings_getVoiceOverVolume(const gameswf::fn_call& fn)      { SWF_AS_TRACE("as_audiosettings_getVoiceOverVolume");      publishAudioFloat(fn, AS_SLOT(getHelpVoiceVolume, 0x88)); }
void as_audiosettings_setVoiceOverVolume(const gameswf::fn_call& fn)      { SWF_AS_TRACE("as_audiosettings_setVoiceOverVolume");      setAudioFloat(fn, AS_SLOT(setHelpVoiceVolume, 0x8C), false); }
void as_audiosettings_getEnglishOnlyVoices(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_getEnglishOnlyVoices");    publishAudioBool(fn, AS_SLOT(getEnglishOnlyVoices, 0x90)); }
void as_audiosettings_setEnglishOnlyVoices(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_setEnglishOnlyVoices");    setAudioBool(fn, AS_SLOT(setEnglishOnlyVoices, 0x94), false); }
void as_audiosettings_getEnableEAX(const gameswf::fn_call& fn)            { SWF_AS_TRACE("as_audiosettings_getEnableEAX");            publishAudioBool(fn, AS_SLOT(getEnableEAX, 0x98)); }
void as_audiosettings_setEnableEAX(const gameswf::fn_call& fn)            { SWF_AS_TRACE("as_audiosettings_setEnableEAX");            setAudioBool(fn, AS_SLOT(setEnableEAX, 0x9C), false); }
void as_audiosettings_canApply(const gameswf::fn_call& fn)                { SWF_AS_TRACE("as_audiosettings_canApply");                publishAudioState(fn, AS_SLOT(hasChanges, 0x1C)); }
void as_audiosettings_needsRestart(const gameswf::fn_call& fn)            { SWF_AS_TRACE("as_audiosettings_needsRestart");            publishAudioState(fn, AS_SLOT(needsRestart, 0x28)); }
void as_audiosettings_hasEax(const gameswf::fn_call& fn)                  { SWF_AS_TRACE("as_audiosettings_hasEax");                  publishAudioState(fn, AS_SLOT(hasEax, 0x2C)); }
void as_audiosettings_hasZenithBoard(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_hasZenithBoard");          publishAudioState(fn, AS_SLOT(hasZenithBoard, 0x30)); }

// isInCriticalState(): a boolean of +0x34's status == 1 (the value a failed apply leaves).
void as_audiosettings_isInCriticalState(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_isInCriticalState");
    const int status = audioSettingsOf(audioSettingsSingleton())->getStatus();          // +0x34
    fn.result->set_bool(status == 1);
}

// getProvider() / getSoundQuality(): the pending name (+0x68 / +0x70, a
// callee-constructed string) published as a string, destroyed after.
static void publishAudioString(const gameswf::fn_call& fn, int offset)
{
    IAudioSettings* as = audioSettingsOf(audioSettingsSingleton());
    const std::string value = offset == 0x68 ? as->getProvider() : as->getSoundQuality();
    fn.result->set_tu_string(tu_string(value.c_str()));
}
void as_audiosettings_getProvider(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_audiosettings_getProvider");     publishAudioString(fn, 0x68); }
void as_audiosettings_getSoundQuality(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_audiosettings_getSoundQuality"); publishAudioString(fn, 0x70); }

// Converts arg 0 before settings lookup, then passes a copy of the provider name by value.
void as_audiosettings_setProvider(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_setProvider");
    const std::string text(fn.arg(0).to_string());
    audioSettingsOf(audioSettingsSingleton())->setProvider(text);                     // +0x6C
}

// Fetches settings before constructing the by-value sound-quality argument.
void as_audiosettings_setSoundQuality(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_setSoundQuality");
    IAudioSettings* as = audioSettingsOf(audioSettingsSingleton());
    as->setSoundQuality(std::string(fn.arg(0).to_string()));                          // +0x74
}

// getSelectedProvider() / getSelectedSoundQuality(): the pending name
// (+0x68 / +0x70), the key "AUDIOSETTINGS_" + it, and the dice String sub_10009A70 builds from the
// key - instruction for instruction the VideoSettings helper (sub_1006B310, a second copy in the
// shipped DLL): the player's host first, then an empty key's narrow String or the key's +0xA8
// localisation in a wide one. Result object; the key, then the name, destroyed.
static void publishSelectedAudioName(const gameswf::fn_call& fn, int offset)
{
    IAudioSettings* as = audioSettingsOf(audioSettingsSingleton());
    const std::string value = offset == 0x68 ? as->getProvider() : as->getSoundQuality();
    const renddx9abi::HostString key = std::string("AUDIOSETTINGS_") + value;
    fn.result->set_as_object_interface(videoSettingsLocalisedString(key));
}
void as_audiosettings_getSelectedProvider(const gameswf::fn_call& fn)     { SWF_AS_TRACE("as_audiosettings_getSelectedProvider");     publishSelectedAudioName(fn, 0x68); }
void as_audiosettings_getSelectedSoundQuality(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_audiosettings_getSelectedSoundQuality"); publishSelectedAudioName(fn, 0x70); }

// The placeholders (the string "Remove Me"; 10008FF0 /
// empty) and getSelectedLeaderboardCategoryId (the string "Scores", through a
// std::string built and destroyed around the publication).
void as_audiosettings_getNumberOfVoices(const gameswf::fn_call& fn)       { SWF_AS_TRACE("as_audiosettings_getNumberOfVoices");       fn.result->set_tu_string(tu_string("Remove Me")); }
void as_audiosettings_getSoundDetail(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_getSoundDetail");          fn.result->set_tu_string(tu_string("Remove Me")); }
void as_audiosettings_getSelectedNumVoices(const gameswf::fn_call& fn)    { SWF_AS_TRACE("as_audiosettings_getSelectedNumVoices");    fn.result->set_tu_string(tu_string("Remove Me")); }
void as_audiosettings_getSelectedSoundDetail(const gameswf::fn_call& fn)  { SWF_AS_TRACE("as_audiosettings_getSelectedSoundDetail");  fn.result->set_tu_string(tu_string("Remove Me")); }
void as_audiosettings_setNumberOfVoices(const gameswf::fn_call& fn)       { SWF_AS_TRACE("as_audiosettings_setNumberOfVoices");       (void) fn; }
void as_audiosettings_setSoundDetail(const gameswf::fn_call& fn)          { SWF_AS_TRACE("as_audiosettings_setSoundDetail");          (void) fn; }
void as_audiosettings_getSelectedLeaderboardCategoryId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_getSelectedLeaderboardCategoryId");
    const std::string scores("Scores");
    fn.result->set_tu_string(tu_string(scores.c_str()));
}

// cancel(): +0x20 revertChanges (the pending record takes the current one). No result.
void as_audiosettings_cancel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_cancel");
    (void) fn;
    audioSettingsOf(audioSettingsSingleton())->revertChanges();                       // +0x20
}

// apply() and reset(), one tail: without pending changes (+0x1C, through
// a lookup of its own) the answer is the boolean true and nothing is applied; otherwise another lookup
// and +0x18 apply(true) - errors reported - then the "LocalProfileManager" singleton, unguarded this
// time: its +0x40 saveCurrentProfile, and only when the apply succeeded its +0x9C saveVoiceSettings;
// the apply's answer is published. reset first calls +0x24 setDefaults (its own lookup).
static void applyAudioSettings(const gameswf::fn_call& fn)
{
    if (!audioSettingsOf(audioSettingsSingleton())->hasChanges())                     // +0x1C
    {
        fn.result->set_bool(true);
        return;
    }
    const bool applied = audioSettingsOf(audioSettingsSingleton())->apply(true);       // +0x18
    ILocalProfileManager* lpm = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    (void) lpm->saveCurrentProfile();                                                 // +0x40
    if (applied)
        (void) lpm->saveVoiceSettings();                                              // +0x9C
    fn.result->set_bool(applied);
}
void as_audiosettings_apply(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_apply");
    applyAudioSettings(fn);
}
void as_audiosettings_reset(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_audiosettings_reset");
    audioSettingsOf(audioSettingsSingleton())->setDefaults();                         // +0x24
    applyAudioSettings(fn);
}

// The "audioSettings" arm of dice.General.getListEntries is defined beside the "videoSettings" one
// (listAudioSettings), because the list dispatcher that calls it comes earlier in this file.

// AS_ServerSettings_setTeamRatio (0x10063B50) - dice.bf2.ServerSettings.setTeamRatio(value): the
// GameServerSettings singleton's +0x17C setTeamRatio with to_number(arg 0) as a float.
void as_serversettings_setTeamRatio(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setTeamRatio");
    IBase* gss = classManager->getSingleton(renddx9abi::HostString("GameServerSettings"));
    const float value = (float) fn.arg(0).to_number();
    static_cast<IGameServerSettings*>(gss)->setTeamRatio(value);
}

// AS_ServerSettings_setNoVehicles (0x100640C0) - dice.bf2.ServerSettings.setNoVehicles(flag): the
// GameServerSettings singleton's +0x1B4 setNoVehicles with to_bool(arg 0).
void as_serversettings_setNoVehicles(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setNoVehicles");
    IBase* gss = classManager->getSingleton(renddx9abi::HostString("GameServerSettings"));
    const bool flag = fn.arg(0).to_bool();
    static_cast<IGameServerSettings*>(gss)->setNoVehicles(flag);
}


// Server-setting bindings publish typed getter results and forward setters without an AS result.
// Most require the singleton. Preserve duplicate getter registrations, absent punish/vote
// setters, and getFriendlyFire reading the punish flag.

static IBase* gameServerSettingsSingleton()
{
    return classManager->getSingleton(renddx9abi::HostString("GameServerSettings"));
}

typedef std::string (IGameServerSettings::*GssStringGetter)();
typedef void        (IGameServerSettings::*GssStringSetter)(std::string);
typedef bool        (IGameServerSettings::*GssBoolGetter)();
typedef void        (IGameServerSettings::*GssBoolSetter)(bool);
typedef int         (IGameServerSettings::*GssIntGetter)();
typedef void        (IGameServerSettings::*GssIntSetter)(int);
typedef float       (IGameServerSettings::*GssFloatGetter)();
typedef void        (IGameServerSettings::*GssFloatSetter)(float);
// Rows call typed native members; offset arguments retain the original slot reference.
#define GSS_SLOT(member, offset) (&IGameServerSettings::member)
static IGameServerSettings* gameServerSettings()
{
    return static_cast<IGameServerSettings*>(gameServerSettingsSingleton());
}

static void publishServerString(const gameswf::fn_call& fn, GssStringGetter getter)
{
    const std::string value = (gameServerSettings()->*getter)();
    fn.result->set_tu_string(tu_string(value.c_str()));
}

static void setServerString(const gameswf::fn_call& fn, GssStringSetter setter)
{
    IGameServerSettings* gss = gameServerSettings();
    (gss->*setter)(std::string(fn.arg(0).to_string()));
}

static void publishServerBool(const gameswf::fn_call& fn, GssBoolGetter getter)
{
    const bool value = (gameServerSettings()->*getter)();
    fn.result->set_bool(value);
}

static void setServerBool(const gameswf::fn_call& fn, GssBoolSetter setter)
{
    IGameServerSettings* gss = gameServerSettings();
    const bool value = fn.arg(0).to_bool();
    (gss->*setter)(value);
}

static void publishServerInt(const gameswf::fn_call& fn, GssIntGetter getter)
{
    const int value = (gameServerSettings()->*getter)();
    fn.result->set_double(static_cast<double>(value));
}

static void setServerInt(const gameswf::fn_call& fn, GssIntSetter setter)
{
    IGameServerSettings* gss = gameServerSettings();
    const int value = static_cast<int>(fn.arg(0).to_number());
    (gss->*setter)(value);
}

static void publishServerFloat(const gameswf::fn_call& fn, GssFloatGetter getter)
{
    const float value = (gameServerSettings()->*getter)();
    fn.result->set_double(static_cast<double>(value));
}

static void setServerFloat(const gameswf::fn_call& fn, GssFloatSetter setter)
{
    IGameServerSettings* gss = gameServerSettings();
    const float value = static_cast<float>(fn.arg(0).to_number());
    (gss->*setter)(value);
}

// AS_ServerSettings_apply (0x10063640): the owner's +0x18 apply, its answer dropped, then the
// "LocalProfileManager" singleton looked up and dropped. No AS result.
void as_serversettings_apply(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_apply");
    (void) fn;
    (void) gameServerSettings()->apply();                                                   // +0x18
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
}

// AS_ServerSettings_reset (0x100636B0): the owner's +0x1C reset. No AS result.
void as_serversettings_reset(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_reset");
    (void) fn;
    gameServerSettings()->reset();                                                          // +0x1C
}

// AS_ServerSettings_setMaxPlayers (0x10063D20): the global player's +0x5C getSwiffHost first - its
// answer dropped, the player unguarded - then the owner, (int) argument zero into +0x44
// setMaxPlayers, and +0x18 apply with its answer dropped. No AS result.
void as_serversettings_setMaxPlayers(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setMaxPlayers");
    (void) g_swiffPlayerInstance->getSwiffHost();
    IGameServerSettings* gss = gameServerSettings();
    const int value = static_cast<int>(fn.arg(0).to_number());
    gss->setMaxPlayers(value);                                                              // +0x44
    (void) gss->apply();                                                                    // +0x18
}

// AS_ServerSettings_setGameMode (0x10063E10): the owner, argument zero as a std::string BY value into
// +0x4C setGameMode, then +0x18 apply with its answer dropped. No AS result.
void as_serversettings_setGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setGameMode");
    IGameServerSettings* gss = gameServerSettings();
    gss->setGameMode(std::string(fn.arg(0).to_string()));                                  // +0x4C
    (void) gss->apply();                                                                    // +0x18
}

// AS_ServerSettings_setCoopBotRatio (0x10063920): argument zero as a float first; then
// "GeneralSettings" and "GameServerSettings" looked up in that order, each NULL-tested; the
// server's +0x19C setCoopBotRatio, then GeneralSettings +0xEC setCoopBotRatio, the same float. No
// AS result.
void as_serversettings_setCoopBotRatio(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setCoopBotRatio");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    IBase* gss = gameServerSettingsSingleton();
    if (gss)
        static_cast<IGameServerSettings*>(gss)->setCoopBotRatio(value);                     // +0x19C
    if (gs)
        static_cast<IGeneralSettings*>(gs)->setCoopBotRatio(value);                         // +0xEC
}

// AS_ServerSettings_setCoopBotCount (0x100639D0): argument zero as a float first; the two lookups;
// GeneralSettings, when present, gets (int) of that float (__ftol2) in +0xB4 setMaxBots and then
// +0xF4 setCoopBotCount; the server, when present, the same truncation in +0x1A4 setCoopBotCount.
// No AS result.
void as_serversettings_setCoopBotCount(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setCoopBotCount");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    IBase* gss = gameServerSettingsSingleton();
    if (gs)
    {
        const int count = static_cast<int>(value);
        static_cast<IGeneralSettings*>(gs)->setMaxBots(count);                              // +0xB4
        static_cast<IGeneralSettings*>(gs)->setCoopBotCount(count);                         // +0xF4
    }
    if (gss)
        static_cast<IGameServerSettings*>(gss)->setCoopBotCount(static_cast<int>(value));   // +0x1A4
}

// AS_ServerSettings_setCoopBotDiff (0x10063AA0): argument zero as a float first; the two lookups;
// GeneralSettings, when present, +0xC4 setBotSkill(value * 0.01f) (flt_100DE540, a float multiply);
// the server, when present, +0x1AC setCoopBotDifficulty(value). No AS result.
void as_serversettings_setCoopBotDiff(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setCoopBotDiff");
    const float value = static_cast<float>(fn.arg(0).to_number());
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    IBase* gss = gameServerSettingsSingleton();
    if (gs)
        static_cast<IGeneralSettings*>(gs)->setBotSkill(value * 0.01f);                     // +0xC4
    if (gss)
        static_cast<IGameServerSettings*>(gss)->setCoopBotDifficulty(value);                // +0x1AC
}

// AS_ServerSettings_setFriendlyFire (0x100636F0): to_bool(argument zero) first; the percentage is
// 100 when set, else 0 (neg / sbb / and 64h); then five fresh lookups: +0x94 setSoldierFF, +0x9C
// setVehicleFF, +0xA4 setSoldierSplashFF, +0xAC setVehicleSplashFF with the percentage, and +0xB4
// setPunishTeamKills with the flag itself. No AS result.
void as_serversettings_setFriendlyFire(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_serversettings_setFriendlyFire");
    const bool flag = fn.arg(0).to_bool();
    const int percent = flag ? 100 : 0;
    gameServerSettings()->setSoldierFF(percent);                                            // +0x94
    gameServerSettings()->setVehicleFF(percent);                                            // +0x9C
    gameServerSettings()->setSoldierSplashFF(percent);                                      // +0xA4
    gameServerSettings()->setVehicleSplashFF(percent);                                      // +0xAC
    gameServerSettings()->setPunishTeamKills(flag);                                         // +0xB4
}

// The 84 one-call rows (the shape table above; each row's handler and slot at its comment).
// AS_ServerSettings_getServerName (0x100650C0): +0x20 getServerName
void as_serversettings_getServerName(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getServerName"); publishServerString(fn, GSS_SLOT(getServerName, 0x20)); }
// AS_ServerSettings_setServerName (0x10063BC0): +0x24 setServerName
void as_serversettings_setServerName(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setServerName"); setServerString(fn, GSS_SLOT(setServerName, 0x24)); }
// AS_ServerSettings_getPassword (0x10065160): +0x28 getPassword
void as_serversettings_getPassword(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getPassword"); publishServerString(fn, GSS_SLOT(getPassword, 0x28)); }
// AS_ServerSettings_setPassword (0x10063C30): +0x2C setPassword
void as_serversettings_setPassword(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setPassword"); setServerString(fn, GSS_SLOT(setPassword, 0x2C)); }
// AS_ServerSettings_getInternet (0x10065300): +0x30 getInternet
void as_serversettings_getInternet(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getInternet"); publishServerBool(fn, GSS_SLOT(getInternet, 0x30)); }
// AS_ServerSettings_setInternet (0x10063DB0): +0x34 setInternet
void as_serversettings_setInternet(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setInternet"); setServerBool(fn, GSS_SLOT(setInternet, 0x34)); }
// AS_ServerSettings_getBandwidthChoke (0x10065200): +0x38 getBandwidthChoke
void as_serversettings_getBandwidthChoke(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getBandwidthChoke"); publishServerInt(fn, GSS_SLOT(getBandwidthChoke, 0x38)); }
// AS_ServerSettings_setBandwidthChoke (0x10063CA0): +0x3C setBandwidthChoke
void as_serversettings_setBandwidthChoke(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setBandwidthChoke"); setServerInt(fn, GSS_SLOT(setBandwidthChoke, 0x3C)); }
// AS_ServerSettings_getMaxPlayers (0x10065280): +0x40 getMaxPlayers
void as_serversettings_getMaxPlayers(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getMaxPlayers"); publishServerInt(fn, GSS_SLOT(getMaxPlayers, 0x40)); }
// AS_ServerSettings_getGameMode (0x10065390): +0x48 getGameMode
void as_serversettings_getGameMode(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getGameMode"); publishServerString(fn, GSS_SLOT(getGameMode, 0x48)); }
// AS_ServerSettings_getStartDelay (0x10065430): +0x50 getStartDelay
void as_serversettings_getStartDelay(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getStartDelay"); publishServerInt(fn, GSS_SLOT(getStartDelay, 0x50)); }
// AS_ServerSettings_setStartDelay (0x10063E80): +0x54 setStartDelay
void as_serversettings_setStartDelay(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setStartDelay"); setServerInt(fn, GSS_SLOT(setStartDelay, 0x54)); }
// AS_ServerSettings_getEndDelay (0x100654B0): +0x58 getEndDelay
void as_serversettings_getEndDelay(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getEndDelay"); publishServerInt(fn, GSS_SLOT(getEndDelay, 0x58)); }
// AS_ServerSettings_setEndDelay (0x10063F00): +0x5C setEndDelay
void as_serversettings_setEndDelay(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setEndDelay"); setServerInt(fn, GSS_SLOT(setEndDelay, 0x5C)); }
// AS_ServerSettings_getSpawnTime (0x10065530): +0x60 getSpawnTime
void as_serversettings_getSpawnTime(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getSpawnTime"); publishServerFloat(fn, GSS_SLOT(getSpawnTime, 0x60)); }
// AS_ServerSettings_setSpawnTime (0x10063F80): +0x64 setSpawnTime
void as_serversettings_setSpawnTime(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setSpawnTime"); setServerFloat(fn, GSS_SLOT(setSpawnTime, 0x64)); }
// AS_ServerSettings_getManDownTime (0x100655B0): +0x68 getManDownTime
void as_serversettings_getManDownTime(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getManDownTime"); publishServerFloat(fn, GSS_SLOT(getManDownTime, 0x68)); }
// AS_ServerSettings_setManDownTime (0x10063FE0): +0x6C setManDownTime
void as_serversettings_setManDownTime(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setManDownTime"); setServerFloat(fn, GSS_SLOT(setManDownTime, 0x6C)); }
// AS_ServerSettings_getTicketRatio (0x10065630): +0x70 getTicketRatio
void as_serversettings_getTicketRatio(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getTicketRatio"); publishServerInt(fn, GSS_SLOT(getTicketRatio, 0x70)); }
// AS_ServerSettings_setTicketRatio (0x10064040): +0x74 setTicketRatio
void as_serversettings_setTicketRatio(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setTicketRatio"); setServerInt(fn, GSS_SLOT(setTicketRatio, 0x74)); }
// AS_ServerSettings_getCoopBotRatio (0x10064F30): +0x198 getCoopBotRatio
void as_serversettings_getCoopBotRatio(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getCoopBotRatio"); publishServerFloat(fn, GSS_SLOT(getCoopBotRatio, 0x198)); }
// AS_ServerSettings_getCoopBotCount (0x10064FB0): +0x1A0 getCoopBotCount
void as_serversettings_getCoopBotCount(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getCoopBotCount"); publishServerInt(fn, GSS_SLOT(getCoopBotCount, 0x1A0)); }
// AS_ServerSettings_getCoopBotDiff (0x10065040): +0x1A8 getCoopBotDifficulty
void as_serversettings_getCoopBotDiff(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getCoopBotDiff"); publishServerFloat(fn, GSS_SLOT(getCoopBotDifficulty, 0x1A8)); }
// AS_ServerSettings_getRoundsPerMap (0x10065740): +0x78 getRoundsPerMap
void as_serversettings_getRoundsPerMap(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getRoundsPerMap"); publishServerInt(fn, GSS_SLOT(getRoundsPerMap, 0x78)); }
// AS_ServerSettings_setRoundsPerMap (0x10064130): +0x7C setRoundsPerMap
void as_serversettings_setRoundsPerMap(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setRoundsPerMap"); setServerInt(fn, GSS_SLOT(setRoundsPerMap, 0x7C)); }
// AS_ServerSettings_getTimeLimit (0x100657C0): +0x80 getTimeLimit
void as_serversettings_getTimeLimit(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getTimeLimit"); publishServerInt(fn, GSS_SLOT(getTimeLimit, 0x80)); }
// AS_ServerSettings_setTimeLimit (0x100641B0): +0x84 setTimeLimit
void as_serversettings_setTimeLimit(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setTimeLimit"); setServerInt(fn, GSS_SLOT(setTimeLimit, 0x84)); }
// AS_ServerSettings_getScoreLimit (0x10065850): +0x88 getScoreLimit
void as_serversettings_getScoreLimit(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getScoreLimit"); publishServerInt(fn, GSS_SLOT(getScoreLimit, 0x88)); }
// AS_ServerSettings_setScoreLimit (0x10064230): +0x8C setScoreLimit
void as_serversettings_setScoreLimit(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setScoreLimit"); setServerInt(fn, GSS_SLOT(setScoreLimit, 0x8C)); }
// AS_ServerSettings_getSoldierFF (0x100658E0): +0x90 getSoldierFF
void as_serversettings_getSoldierFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getSoldierFF"); publishServerInt(fn, GSS_SLOT(getSoldierFF, 0x90)); }
// AS_ServerSettings_setSoldierFF (0x100642B0): +0x94 setSoldierFF
void as_serversettings_setSoldierFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setSoldierFF"); setServerInt(fn, GSS_SLOT(setSoldierFF, 0x94)); }
// AS_ServerSettings_getVehicleFF (0x10065970): +0x98 getVehicleFF
void as_serversettings_getVehicleFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVehicleFF"); publishServerInt(fn, GSS_SLOT(getVehicleFF, 0x98)); }
// AS_ServerSettings_setVehicleFF (0x10064330): +0x9C setVehicleFF
void as_serversettings_setVehicleFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVehicleFF"); setServerInt(fn, GSS_SLOT(setVehicleFF, 0x9C)); }
// AS_ServerSettings_getSoldierSplashFF (0x10065A00): +0xA0 getSoldierSplashFF
void as_serversettings_getSoldierSplashFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getSoldierSplashFF"); publishServerInt(fn, GSS_SLOT(getSoldierSplashFF, 0xA0)); }
// AS_ServerSettings_setSoldierSplashFF (0x100643B0): +0xA4 setSoldierSplashFF
void as_serversettings_setSoldierSplashFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setSoldierSplashFF"); setServerInt(fn, GSS_SLOT(setSoldierSplashFF, 0xA4)); }
// AS_ServerSettings_getVehicleSplashFF (0x10065A90): +0xA8 getVehicleSplashFF
void as_serversettings_getVehicleSplashFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVehicleSplashFF"); publishServerInt(fn, GSS_SLOT(getVehicleSplashFF, 0xA8)); }
// AS_ServerSettings_setVehicleSplashFF (0x10064430): +0xAC setVehicleSplashFF
void as_serversettings_setVehicleSplashFF(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVehicleSplashFF"); setServerInt(fn, GSS_SLOT(setVehicleSplashFF, 0xAC)); }
// AS_ServerSettings_getPunishTeamKills (0x10065B20): +0xB0 getPunishTeamKills
void as_serversettings_getPunishTeamKills(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getPunishTeamKills"); publishServerBool(fn, GSS_SLOT(getPunishTeamKills, 0xB0)); }
// AS_ServerSettings_getVoteTime (0x10065BB0): +0xB8 getVoteTime
void as_serversettings_getVoteTime(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoteTime"); publishServerInt(fn, GSS_SLOT(getVoteTime, 0xB8)); }
// AS_ServerSettings_getMinPlayersForVoting (0x10065C40): +0xC0 getMinPlayersForVoting
void as_serversettings_getMinPlayersForVoting(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getMinPlayersForVoting"); publishServerInt(fn, GSS_SLOT(getMinPlayersForVoting, 0xC0)); }
// AS_ServerSettings_setMinPlayersForVoting (0x100644B0): +0xC4 setMinPlayersForVoting
void as_serversettings_setMinPlayersForVoting(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setMinPlayersForVoting"); setServerInt(fn, GSS_SLOT(setMinPlayersForVoting, 0xC4)); }
// AS_ServerSettings_getVoipEnabled (0x10065CD0): +0xD0 getVoipEnabled
void as_serversettings_getVoipEnabled(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoipEnabled"); publishServerBool(fn, GSS_SLOT(getVoipEnabled, 0xD0)); }
// AS_ServerSettings_setVoipEnabled (0x10064530): +0xD4 setVoipEnabled
void as_serversettings_setVoipEnabled(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVoipEnabled"); setServerBool(fn, GSS_SLOT(setVoipEnabled, 0xD4)); }
// AS_ServerSettings_getVoipQuality (0x10065D60): +0xD8 getVoipQuality
void as_serversettings_getVoipQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoipQuality"); publishServerInt(fn, GSS_SLOT(getVoipQuality, 0xD8)); }
// AS_ServerSettings_setVoipQuality (0x10064590): +0xDC setVoipQuality
void as_serversettings_setVoipQuality(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVoipQuality"); setServerInt(fn, GSS_SLOT(setVoipQuality, 0xDC)); }
// AS_ServerSettings_getVoipServerRemote (0x10065DF0): +0xE0 getVoipServerRemote
void as_serversettings_getVoipServerRemote(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoipServerRemote"); publishServerBool(fn, GSS_SLOT(getVoipServerRemote, 0xE0)); }
// AS_ServerSettings_setVoipServerRemote (0x10064610): +0xE4 setVoipServerRemote
void as_serversettings_setVoipServerRemote(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVoipServerRemote"); setServerBool(fn, GSS_SLOT(setVoipServerRemote, 0xE4)); }
// AS_ServerSettings_getVoipServerRemoteIP (0x10065E80): +0xE8 getVoipServerRemoteIP
void as_serversettings_getVoipServerRemoteIP(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoipServerRemoteIP"); publishServerString(fn, GSS_SLOT(getVoipServerRemoteIP, 0xE8)); }
// AS_ServerSettings_setVoipServerRemoteIP (0x10064670): +0xEC setVoipServerRemoteIP
void as_serversettings_setVoipServerRemoteIP(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVoipServerRemoteIP"); setServerString(fn, GSS_SLOT(setVoipServerRemoteIP, 0xEC)); }
// AS_ServerSettings_getVoipServerPort (0x10065F20): +0xF0 getVoipServerPort
void as_serversettings_getVoipServerPort(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoipServerPort"); publishServerInt(fn, GSS_SLOT(getVoipServerPort, 0xF0)); }
// AS_ServerSettings_setVoipServerPort (0x100646E0): +0xF4 setVoipServerPort
void as_serversettings_setVoipServerPort(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVoipServerPort"); setServerInt(fn, GSS_SLOT(setVoipServerPort, 0xF4)); }
// AS_ServerSettings_getVoipBFClientPort (0x10065FB0): +0xF8 getVoipBFClientPort
void as_serversettings_getVoipBFClientPort(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getVoipBFClientPort"); publishServerInt(fn, GSS_SLOT(getVoipBFClientPort, 0xF8)); }
// AS_ServerSettings_setVoipBFClientPort (0x10064760): +0xFC setVoipBFClientPort
void as_serversettings_setVoipBFClientPort(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setVoipBFClientPort"); setServerInt(fn, GSS_SLOT(setVoipBFClientPort, 0xFC)); }
// AS_ServerSettings_getGameSpyPort (0x10066040): +0x110 getGameSpyPort
void as_serversettings_getGameSpyPort(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getGameSpyPort"); publishServerInt(fn, GSS_SLOT(getGameSpyPort, 0x110)); }
// AS_ServerSettings_setGameSpyPort (0x100647E0): +0x114 setGameSpyPort
void as_serversettings_setGameSpyPort(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setGameSpyPort"); setServerInt(fn, GSS_SLOT(setGameSpyPort, 0x114)); }
// AS_ServerSettings_getAllowNATNegotation (0x100660D0): +0x118 getAllowNATNegotiation
void as_serversettings_getAllowNATNegotation(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getAllowNATNegotation"); publishServerBool(fn, GSS_SLOT(getAllowNATNegotiation, 0x118)); }
// AS_ServerSettings_setAllowNATNegotation (0x10064860): +0x11C setAllowNATNegotiation
void as_serversettings_setAllowNATNegotation(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setAllowNATNegotation"); setServerBool(fn, GSS_SLOT(setAllowNATNegotiation, 0x11C)); }
// AS_ServerSettings_getInterfaceIP (0x10066160): +0x120 getInterfaceIP
void as_serversettings_getInterfaceIP(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getInterfaceIP"); publishServerString(fn, GSS_SLOT(getInterfaceIP, 0x120)); }
// AS_ServerSettings_setInterfaceIP (0x100648C0): +0x124 setInterfaceIP
void as_serversettings_setInterfaceIP(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setInterfaceIP"); setServerString(fn, GSS_SLOT(setInterfaceIP, 0x124)); }
// AS_ServerSettings_getAutoRecord (0x10066200): +0x128 getAutoRecord
void as_serversettings_getAutoRecord(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getAutoRecord"); publishServerBool(fn, GSS_SLOT(getAutoRecord, 0x128)); }
// AS_ServerSettings_setAutoRecord (0x10064930): +0x12C setAutoRecord
void as_serversettings_setAutoRecord(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setAutoRecord"); setServerBool(fn, GSS_SLOT(setAutoRecord, 0x12C)); }
// AS_ServerSettings_getDemoIndexURL (0x10066290): +0x130 getDemoIndexURL
void as_serversettings_getDemoIndexURL(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getDemoIndexURL"); publishServerString(fn, GSS_SLOT(getDemoIndexURL, 0x130)); }
// AS_ServerSettings_setDemoIndexURL (0x10064990): +0x134 setDemoIndexURL
void as_serversettings_setDemoIndexURL(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setDemoIndexURL"); setServerString(fn, GSS_SLOT(setDemoIndexURL, 0x134)); }
// AS_ServerSettings_getDemoDownloadURL (0x10066330): +0x138 getDemoDownloadURL
void as_serversettings_getDemoDownloadURL(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getDemoDownloadURL"); publishServerString(fn, GSS_SLOT(getDemoDownloadURL, 0x138)); }
// AS_ServerSettings_setDemoDownloadURL (0x10064A00): +0x13C setDemoDownloadURL
void as_serversettings_setDemoDownloadURL(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setDemoDownloadURL"); setServerString(fn, GSS_SLOT(setDemoDownloadURL, 0x13C)); }
// AS_ServerSettings_getDemoHook (0x100663D0): +0x140 getDemoHook
void as_serversettings_getDemoHook(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getDemoHook"); publishServerString(fn, GSS_SLOT(getDemoHook, 0x140)); }
// AS_ServerSettings_setDemoHook (0x10064A70): +0x144 setDemoHook
void as_serversettings_setDemoHook(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setDemoHook"); setServerString(fn, GSS_SLOT(setDemoHook, 0x144)); }
// AS_ServerSettings_getAdminScript (0x10066470): +0x148 getAdminScript
void as_serversettings_getAdminScript(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getAdminScript"); publishServerString(fn, GSS_SLOT(getAdminScript, 0x148)); }
// AS_ServerSettings_setAdminScript (0x10064AE0): +0x14C setAdminScript
void as_serversettings_setAdminScript(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setAdminScript"); setServerString(fn, GSS_SLOT(setAdminScript, 0x14C)); }
// AS_ServerSettings_getTimeBeforeRestartMap (0x10066510): +0x150 getTimeBeforeRestartMap
void as_serversettings_getTimeBeforeRestartMap(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getTimeBeforeRestartMap"); publishServerFloat(fn, GSS_SLOT(getTimeBeforeRestartMap, 0x150)); }
// AS_ServerSettings_setTimeBeforeRestartMap (0x10064B50): +0x154 setTimeBeforeRestartMap
void as_serversettings_setTimeBeforeRestartMap(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setTimeBeforeRestartMap"); setServerFloat(fn, GSS_SLOT(setTimeBeforeRestartMap, 0x154)); }
// AS_ServerSettings_getSponsorText (0x10066590): +0x158 getSponsorText
void as_serversettings_getSponsorText(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getSponsorText"); publishServerString(fn, GSS_SLOT(getSponsorText, 0x158)); }
// AS_ServerSettings_setSponsorText (0x10064BC0): +0x15C setSponsorText
void as_serversettings_setSponsorText(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setSponsorText"); setServerString(fn, GSS_SLOT(setSponsorText, 0x15C)); }
// AS_ServerSettings_getSponsorLogoURL (0x10066630): +0x160 getSponsorLogoURL
void as_serversettings_getSponsorLogoURL(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getSponsorLogoURL"); publishServerString(fn, GSS_SLOT(getSponsorLogoURL, 0x160)); }
// AS_ServerSettings_setSponsorLogoURL (0x10064C30): +0x164 setSponsorLogoURL
void as_serversettings_setSponsorLogoURL(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setSponsorLogoURL"); setServerString(fn, GSS_SLOT(setSponsorLogoURL, 0x164)); }
// AS_ServerSettings_getSvPunkBuster (0x100666D0): +0x180 getSvPunkBuster
void as_serversettings_getSvPunkBuster(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getSvPunkBuster"); publishServerBool(fn, GSS_SLOT(getSvPunkBuster, 0x180)); }
// AS_ServerSettings_setSvPunkBuster (0x10064CA0): +0x184 setSvPunkBuster
void as_serversettings_setSvPunkBuster(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setSvPunkBuster"); setServerBool(fn, GSS_SLOT(setSvPunkBuster, 0x184)); }
// AS_ServerSettings_getAutoBalanceTeam (0x10064D90): +0x168 getAutoBalanceTeam
void as_serversettings_getAutoBalanceTeam(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getAutoBalanceTeam"); publishServerBool(fn, GSS_SLOT(getAutoBalanceTeam, 0x168)); }
// AS_ServerSettings_setAutoBalanceTeam (0x10063830): +0x16C setAutoBalanceTeam
void as_serversettings_setAutoBalanceTeam(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setAutoBalanceTeam"); setServerBool(fn, GSS_SLOT(setAutoBalanceTeam, 0x16C)); }
// AS_ServerSettings_getAutoRecordRounds (0x10064E20): +0x170 getAutoRecordRounds
void as_serversettings_getAutoRecordRounds(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getAutoRecordRounds"); publishServerInt(fn, GSS_SLOT(getAutoRecordRounds, 0x170)); }
// AS_ServerSettings_setAutoRecordRounds (0x100638A0): +0x174 setAutoRecordRounds
void as_serversettings_setAutoRecordRounds(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_setAutoRecordRounds"); setServerInt(fn, GSS_SLOT(setAutoRecordRounds, 0x174)); }
// AS_ServerSettings_getTeamRatio (0x10064EB0): +0x178 getTeamRatio
void as_serversettings_getTeamRatio(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getTeamRatio"); publishServerFloat(fn, GSS_SLOT(getTeamRatio, 0x178)); }
// AS_ServerSettings_getFriendlyFire (0x10064D00): +0xB0 getPunishTeamKills
void as_serversettings_getFriendlyFire(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getFriendlyFire"); publishServerBool(fn, GSS_SLOT(getPunishTeamKills, 0xB0)); }
// AS_ServerSettings_getNoVehicles (0x100656B0): +0x1B0 getNoVehicles
void as_serversettings_getNoVehicles(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_serversettings_getNoVehicles"); publishServerBool(fn, GSS_SLOT(getNoVehicles, 0x1B0)); }

#undef GSS_SLOT


// Clears the host map list inside a busy-cursor bracket. Requires player/host;
// writes no AS result.
void as_multiplay_clearMapList(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_clearMapList");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost) { SetCursor(cursorHost->getCursor(1)); cursorHost->setBusy(1); }
    g_swiffPlayerInstance->getSwiffHost()->clearServerEntries();                           // +0xF4
    if (cursorHost) { SetCursor(cursorHost->getCursor(0)); cursorHost->clearBusy(); }
}

// sub_10051C10 - dice.bf2.Multiplay.addMapToMapList(path, mode, size): the busy scope; the
// player's host; "LocalProfileManager" fetched and dropped; path = arg 0. Nothing happens for an
// empty path. Otherwise mode = arg 1 and size = (unsigned) (int) to_number(arg 2); with
// GeneralSettings +0xA8 getUseBots set the mode is replaced by the size's singleplayer layer -
// "sp1" up to 16 players, "sp2" up to 32, "sp3" above - unless the mode equals "gpm_coop"
// (sub_10003870 = _stricmp), which is kept; then host +0x128 addServerEntry(path, mode, size).
// No AS result.
void as_multiplay_addMapToMapList(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_addMapToMapList");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost) { SetCursor(cursorHost->getCursor(1)); cursorHost->setBusy(1); }
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const std::string path(fn.arg(0).to_string());
    if (!path.empty())
    {
        const std::string mode(fn.arg(1).to_string());
        const unsigned int size = (unsigned int) (int) fn.arg(2).to_number();
        IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
        if (static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->getUseBots())
        {
            std::string layer;
            if (size <= 32) layer = (size <= 16) ? "sp1" : "sp2";
            else            layer = "sp3";
            if (::_stricmp(mode.c_str(), "gpm_coop") == 0) layer = "gpm_coop";
            h->addServerEntry(path, layer, (int) size);                                    // +0x128
        }
        else
        {
            h->addServerEntry(path, mode, (int) size);                                     // +0x128
        }
    }
    if (cursorHost) { SetCursor(cursorHost->getCursor(0)); cursorHost->clearBusy(); }
}


// Starts a local server inside a busy bracket after disc and nonzero-round checks.
// Clears in-game info caches and selects loopback settings. Returns hostServer's Boolean;
// an already-running game leaves the AS result untouched.
void as_multiplay_createServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_createServer");
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IGameServerSettings* gss = static_cast<IGameServerSettings*>(gameServerSettingsSingleton());
    BusyHost busy = busyOpen();
    s_ingameAddressA = ""; s_ingamePortA = 0;
    s_ingameAddressB = ""; s_ingamePortB = 0;
    bool running = false;
    (void) h->isGameRunning(&running);                                                      // +0xB8
    if (running)
    {
        busyClose(busy);
        return;
    }
    if (!h->checkDisc())                                                                    // +0x2D0
    {
        h->reportNoDisc();                                                                  // +0x2D4
        fn.result->set_bool(false);
    }
    else if (gss->getRoundsPerMap() == 0)                                                   // +0x78
    {
        (void) h->postMessage(SwiffHostMessage(1, std::string("Number of rounds must be greater than 0"), 0));   // +0x1E4
        fn.result->set_bool(false);
    }
    else
    {
        std::string address;
        int queryPort = 0, port = 0;                                // [CHOSEN] zeroed; the shipped locals are not, and are unread on refusal
        if (!h->getServerSettingsInfo(address, &queryPort, &port))                         // +0x11C
            fn.result->set_bool(false);
        else
        {
            address = "127.0.0.1";
            h->setSelectedServer(address, queryPort, port);                                // +0x10C
            fn.result->set_bool(h->hostServer());                                          // +0x104
        }
    }
    busyClose(busy);
}

// sub_10050E70 (14 B) - dice.bf2.Multiplay.setCurrentMap(index): the global player's +0x5C
// getSwiffHost with its answer dropped, and nothing else - the argument is never read. No AS result.
void as_multiplay_setCurrentMap(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setCurrentMap");
    (void) fn;
    (void) g_swiffPlayerInstance->getSwiffHost();
}

// nullsub_4 (0x10050E80, 3 B) - dice.bf2.Multiplay.setMapList(list): the shipped body is a bare
// `retn 0Ch`. It is registered and does nothing, and so does this.
void as_multiplay_setMapList(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setMapList");
    (void) fn;
}

// sub_10051DF0 (170 B) - dice.bf2.Multiplay.removeMapfromMapList(index): busy open; host =
// player->+0x5C; "LocalProfileManager" looked up and dropped; (int) argument zero into host +0x12C
// removeServerEntry, its answer dropped; busy close. No AS result.
void as_multiplay_removeMapfromMapList(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_removeMapfromMapList");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const int index = static_cast<int>(fn.arg(0).to_number());
    (void) h->removeServerEntry(static_cast<unsigned int>(index));                          // +0x12C
    busyClose(busy);
}

// sub_10054CB0 (236 B) - dice.bf2.Multiplay.moveMapInList(index, up): host = player->+0x5C;
// "LocalProfileManager" looked up and dropped; index = (int) argument zero, then up = to_bool
// (argument one); host +0x130 moveServerEntry(index, up), its answer dropped - the AS result is
// boolean true (dbl_100D7E10, 1.0) whatever the move did.
void as_multiplay_moveMapInList(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_moveMapInList");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    const int index = static_cast<int>(fn.arg(0).to_number());
    const bool up = fn.arg(1).to_bool();
    (void) h->moveServerEntry(static_cast<unsigned int>(index), up);                        // +0x130
    fn.result->set_bool(true);
}

// sub_10054E20 / sub_10054EA0 (121 B each) - getCurrentMapName / getCurrentMapGameMode(): the global
// player's +0x5C host, its +0x58 / +0x5C string (hidden return buffer) published as a string and
// destroyed after the publication. Without a game the host answers "$noCurrentGame" (0x4875F0).
static void publishHostString(const gameswf::fn_call& fn, int slot)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string value = slot == 0x58 ? h->getCurrentMapName() : h->getCurrentMapGameMode();
    fn.result->set_tu_string(tu_string(value.c_str()));
}
void as_multiplay_getCurrentMapName(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_multiplay_getCurrentMapName");     publishHostString(fn, 0x58); }
void as_multiplay_getCurrentMapGameMode(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_multiplay_getCurrentMapGameMode"); publishHostString(fn, 0x5C); }

// sub_10054F20 / sub_10054F80 (95 B each) - getCurrentMapMaxPlayers / getCurrentMapSize(): the host's
// +0x60 / +0x64 int as a number (fild). The result's type and empty string are set before the call,
// which changes nothing observable.
static void publishHostInt(const gameswf::fn_call& fn, int slot)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int value = slot == 0x60 ? h->getCurrentMapMaxPlayers() : h->getCurrentMapSize();
    fn.result->set_double(static_cast<double>(value));
}
void as_multiplay_getCurrentMapMaxPlayers(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_multiplay_getCurrentMapMaxPlayers"); publishHostInt(fn, 0x60); }
void as_multiplay_getCurrentMapSize(const gameswf::fn_call& fn)
{ SWF_AS_TRACE("as_multiplay_getCurrentMapSize");       publishHostInt(fn, 0x64); }

// sub_1004C9A0 (204 B) - dice.bf2.Logic.startCommandlineServer(): what the command-line server's page
// calls (startGame's menu flow opens page 4 or 5 for a +hostServer launch). busy open; host =
// player->+0x5C; address = ""; host +0x11C getServerSettingsInfo(address, &queryPort, &port): false
// -> busy close and nothing else; true -> address = "127.0.0.1", +0x10C setSelectedServer(address,
// queryPort, port) and +0x104 hostServer() with its answer dropped; busy close. No AS result.
void as_logic_startCommandlineServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_startCommandlineServer");
    (void) fn;
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string address;
    int queryPort = 0, port = 0;                                    // [CHOSEN] zeroed, as in createServer
    if (h->getServerSettingsInfo(address, &queryPort, &port))                               // +0x11C
    {
        address = "127.0.0.1";
        h->setSelectedServer(address, queryPort, port);                                     // +0x10C
        (void) h->hostServer();                                                             // +0x104
    }
    busyClose(busy);
}

// Lists copied server map entries with localized display name, mode, path, and size.
// Counts every entry while emitting only the requested unsigned index window.
namespace {

static void listMapList(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    int total = 0;
    IModInfo* modInfo = static_cast<IModInfo*>(h->getModInfo());                           // +0x120
    (void) modInfo->getModList();                                                          // +0x14
    r.begin();
    (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
    std::vector<SwiffServerEntry> entries;
    if (h->getServerEntries(entries))                                                      // +0xFC
    {
        for (unsigned int i = 0; i < entries.size(); ++i, ++total)
        {
            if (i < static_cast<unsigned int>(start) || i >= static_cast<unsigned int>(end))
                continue;
            r.beginRow(static_cast<int>(i));
            const std::string name = modInfo->getMapNameFromMap(entries[i].map);           // PE +0x3C
            const std::string path = modInfo->getMapPathFromMap(entries[i].map);           // PE +0x44
            char size[32];
            ::_snprintf(size, sizeof(size) - 1, " - %d", entries[i].size);
            size[sizeof(size) - 1] = 0;
            r.pushNarrow((name + size).c_str());
            r.pushNarrow(entries[i].gameMode.c_str());
            r.setString("path", path.c_str());
            r.setInt("mapSize", entries[i].size);
        }
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// Lists named current-mod game modes within the window, selecting the first emitted row.
// Counts emitted names only; withAll appends/counts an empty-ID "All" choice.
static void listGameModes(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end, bool withAll)
{
    r.begin();
    unsigned int emitted = 0;
    const unsigned int from = static_cast<unsigned int>(start);
    const unsigned int to   = static_cast<unsigned int>(end);
    for (unsigned int i = 0;
         i < static_cast<IModInfo*>(h->getModInfo())->getCurrentMod()->gameModes.size();   // +0x120, +0x1C
         ++i)
    {
        if (i < from || i >= to)
            continue;
        const std::string id(static_cast<IModInfo*>(h->getModInfo())->getCurrentMod()->gameModes[i]);
        const std::string name = static_cast<IModInfo*>(h->getModInfo())->getNameFromGameMode(id);   // +0x4C
        if (!name.empty())
        {
            r.beginRowStr(id.c_str());
            r.pushNarrow(name.c_str());
            if (emitted == 0)
                r.setInt("selected", 1);
            ++emitted;
        }
    }
    if (withAll)
    {
        if (emitted >= from && emitted < to)
        {
            r.beginRowStr("");
            r.pushNarrow("All");
        }
        ++emitted;
    }
    r.setTotalCount(static_cast<int>(emitted));
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// The shipped test also rewrites the caller's string; both callers pass a copy.
static void underscoresToSpaces(std::string& s)
{
    for (std::string::size_type i = 0; i < s.size(); ++i)
        if (s[i] == '_')
            s[i] = ' ';
}
static bool modListsMap(const IModInfo::Mod* mod, std::string& name)
{
    underscoresToSpaces(name);
    for (unsigned int i = 0; i < mod->maps.size(); ++i)
    {
        std::string display(mod->maps[i].name);
        std::string path(mod->maps[i].path);
        underscoresToSpaces(display);
        underscoresToSpaces(path);
        if (::_stricmp(display.c_str(), name.c_str()) == 0)
            return true;
        if (::_stricmp(path.c_str(), name.c_str()) == 0)
            return true;
    }
    return false;
}

// As shipped, a row outside the window is counted whatever its map, and neither the library nor
// the mod is null-tested.
static void listDemos(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    r.begin();
    int total = 0;
    const unsigned int count = static_cast<IDemoLibrary*>(h->getDemoLibrary())->getNumEntries();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (i < static_cast<unsigned int>(start) || i >= static_cast<unsigned int>(end))
        {
            ++total;
            continue;
        }
        const DemoLibraryEntry* entry = static_cast<IDemoLibrary*>(h->getDemoLibrary())->getEntry(i);
        const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();
        std::string map(entry->mapName);
        if (!modListsMap(mod, map))
            continue;
        r.beginRow(static_cast<int>(i));
        r.pushNarrow(entry->dateTime.c_str());
        r.pushNarrow(entry->name.c_str());
        const std::string mapName = static_cast<IModInfo*>(h->getModInfo())->getMapNameFromMap(entry->mapName);
        r.pushNarrow(mapName.c_str());
        if (entry->version != 3153)
            r.setInt("disabled", 1);
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// Unlike listDemos, the mod check runs on the map's display name. The singleton is not
// null-tested, as shipped.
static void listDemoBookmarks(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    ILocalProfileManager* profiles = static_cast<ILocalProfileManager*>(
        classManager->getSingleton(renddx9abi::HostString("LocalProfileManager")));
    r.begin();
    int total = 0;
    const unsigned int count = profiles->getNumDemoBookmarks();
    for (unsigned int i = 0; i < count; ++i)
    {
        if (i < static_cast<unsigned int>(start) || i >= static_cast<unsigned int>(end))
        {
            ++total;
            continue;
        }
        const LocalDemoBookmark* bookmark = profiles->getDemoBookmark(i);
        const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();
        std::string shown = static_cast<IModInfo*>(h->getModInfo())->getMapNameFromMap(bookmark->field1c);
        if (!modListsMap(mod, shown))
            continue;
        r.beginRow(static_cast<int>(i));
        r.pushNarrow(bookmark->field54.c_str());
        r.pushNarrow(bookmark->field00.c_str());
        const std::string mapName = static_cast<IModInfo*>(h->getModInfo())->getMapNameFromMap(bookmark->field1c);
        r.pushNarrow(mapName.c_str());
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}
} // anonymous namespace (the mapList, game-mode and BattleRecorder lists)

// sub_10055C40 - dice.bf2.Multiplay.getMapNameFromId(id): the player's host; id = arg 0; the
// current mod (host +0x120 getModInfo -> +0x1C getCurrentMod, NOT null-tested, as shipped) and a
// walk of its maps (+0xFC, stride 164): the first whose path (+0) equals the id - std::string
// operator==, case-sensitive - answers its display name (+0x1C) as a string; none answers "".
void as_multiplay_getMapNameFromId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getMapNameFromId");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string id(fn.arg(0).to_string());
    const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();
    for (unsigned int i = 0; i < mod->maps.size(); ++i)
    {
        if (mod->maps[i].path == id)
        {
            fn.result->set_tu_string(tu_string(mod->maps[i].name.c_str()));
            return;
        }
    }
    fn.result->set_tu_string(tu_string(""));
}

// sub_10055DD0 - dice.bf2.Multiplay.getMapDescriptionFromId(id): the same walk; the match's
// briefing loc-id (+0x54) - or, when that is empty, its briefing text (+0x38) - goes through the
// player's host +0xA8 getLocalizedString, and the wide answer is published as a dice String
// object (the Locale.getString shape: a new String, its wide payload assigned, its narrow text
// cleared). No match answers the plain string "".
void as_multiplay_getMapDescriptionFromId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getMapDescriptionFromId");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string id(fn.arg(0).to_string());
    const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();
    for (unsigned int i = 0; i < mod->maps.size(); ++i)
    {
        if (mod->maps[i].path == id)
        {
            std::string key(mod->maps[i].briefingLocId);
            if (key == "")
                key = mod->maps[i].briefing;
            std::wstring out;
            g_swiffPlayerInstance->getSwiffHost()->getLocalizedString(out, key);            // +0xA8
            gameswf::as_object_interface* text = gameswf::create_dice_string_object(nullptr, 0);
            gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(out.c_str()));
            fn.result->set_as_object_interface(text);
            return;
        }
    }
    fn.result->set_tu_string(tu_string(""));
}

// sub_10053710 - dice.bf2.Multiplay.getGameModeFromId(mode): the player's host; mode = arg 0. A
// mode equal (sub_10003870 = _stricmp) to "sp1", "sp2" or "sp3" is "Singleplayer"; otherwise the
// host's +0x164 getGameMode equal to "gpm_coop" is "Co-op" and anything else "Conquest". string.
void as_multiplay_getGameModeFromId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getGameModeFromId");
    const char* answer = "Conquest";
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string mode(fn.arg(0).to_string());
    if (::_stricmp(mode.c_str(), "sp1") == 0 || ::_stricmp(mode.c_str(), "sp2") == 0
        || ::_stricmp(mode.c_str(), "sp3") == 0)
        answer = "Singleplayer";
    else if (::_stricmp(h->getGameMode().c_str(), "gpm_coop") == 0)                         // +0x164
        answer = "Co-op";
    fn.result->set_tu_string(tu_string(answer));
}

// sub_10051700 - dice.bf2.Multiplay.setFilterGameMode(mode), the player's
// host (+0x5C, unguarded), argument zero through the trapping accessor into a std::string, host
// +0x168 setGameMode(mode); then modPath = host +0x120 getModInfo() -> +0x20 getCurrentModPath(),
// and when that equals "mods/bf2" or "mods/xpack" (sub_10003870) and the mode equals "gpm_coop" or
// "gpm_cq", host +0xF4 - the clear of the +0xEF4 entry vector the page's addMapToMapList fills. No
// AS result. The singleplayer page calls it with "gpm_cq" before it asks for installedMaps.
void as_multiplay_setFilterGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setFilterGameMode");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string mode(fn.arg(0).to_string());
    h->setGameMode(mode);                                                                // +0x168
    IModInfo* modInfo = static_cast<IModInfo*>(h->getModInfo());                         // +0x120
    const std::string modPath = modInfo->getCurrentModPath();                            // +0x20
    if (::_stricmp(modPath.c_str(), "mods/bf2") == 0 || ::_stricmp(modPath.c_str(), "mods/xpack") == 0)
        if (::_stricmp(mode.c_str(), "gpm_coop") == 0 || ::_stricmp(mode.c_str(), "gpm_cq") == 0)
            h->clearServerEntries();                                                     // +0xF4
}

// Publishes the host's game-mode filter string, used for newly added map-list entries.
// Takes no arguments; requires the player and host.
void as_multiplay_getFilterGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getFilterGameMode");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string mode = h->getGameMode();                                            // +0x164
    fn.result->set_tu_string(tu_string(mode.c_str()));
}


// Stores the browser-list name without fetching its contents. Requires player/host and
// argument zero; writes no AS result. [RETIRE-WHEN: RW-88]
void as_multiplay_setBrowser(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setBrowser");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string kind(fn.arg(0).to_string());
    h->setServerListKind(kind);                                          // +0x138
}

// Starts updating the current browser list with refresh enabled. Requires player, host,
// and list; ignores update status and leaves the AS result untouched.
void as_multiplay_startBrowse(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_startBrowse");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list;
    {
        const std::string kind = h->getServerListKind();       // +0x134
        list = h->getServerList(kind);                         // +0x7C
        (void) list->startUpdate(true);                        // +0x28, the Boolean discarded
    }                                                          // the shipped body destroys it here
}

// Returns the full current browser record count as a number, including filtered-out rows.
// Requires player, host, and browser list.
void as_multiplay_getNumServers(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getNumServers");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    int count;
    {
        const std::string kind = h->getServerListKind();       // +0x134
        IServerBrowser* list = h->getServerList(kind);         // +0x7C
        count = serverRecordCount(list->getRecords());         // +0x20, /580
    }                                                          // the shipped body destroys it here
    fn.result->set_double(count);
}


// Joins row ID "address queryPort hostPort" after checking game/disc state; returns Boolean.
// Clears in-game server-info keys inside a busy bracket. [CHOSEN] Address parsing uses
// a bounded width instead of the original unbounded conversion.
void as_multiplay_joinServer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_joinServer");
    BusyHost busy = busyOpen();
    s_ingameAddressA = ""; s_ingamePortA = 0;
    s_ingameAddressB = ""; s_ingamePortB = 0;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    bool running = false;
    (void) h->isGameRunning(&running);                                         // +0xB8
    if (running)
        fn.result->set_bool(false);
    else if (h->checkDisc())                                                   // +0x2D0
    {
        (void) classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
        (void) classManager->getSingleton(renddx9abi::HostString("LocalProfileManager"));
        const std::string id(fn.arg(0).to_string());
        std::string address;
        char buf[260] = {0};
        int first = 0, second = 0;
        ::sscanf(id.c_str(), "%259s %d %d", buf, &first, &second);
        address = buf;
        const bool ok = h->joinServer(address, second, first);                 // +0x100
        fn.result->set_bool(ok);
    }
    else
    {
        h->reportNoDisc();                                                     // +0x2D4
        fn.result->set_bool(false);
    }
    busyClose(busy);
}

// dice.bf2.Multiplay.setCurrentServerId(id), read in full. The browser's
// row selection: the id "<address> <queryPort> <hostPort>" split (sscanf "%s %d %d") into host
// +0x10C setSelectedServer(address, queryPort, hostPort), then both indexed ints (+0x114 read,
// +0x118 written; index 0, then 1) raised by one. No AS result. The same 260-byte %s as joinServer,
// bounded here the same way.
void as_multiplay_setCurrentServerId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setCurrentServerId");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string id(fn.arg(0).to_string());
    std::string address;
    char buf[260] = {0};
    int queryPort = 0, hostPort = 0;
    ::sscanf(id.c_str(), "%259s %d %d", buf, &queryPort, &hostPort);
    address = buf;
    h->setSelectedServer(address, queryPort, hostPort);                        // +0x10C
    h->setIndexedInt(0, h->getIndexedInt(0) + 1);                              // +0x114 / +0x118
    h->setIndexedInt(1, h->getIndexedInt(1) + 1);
}

// dice.bf2.Multiplay.clearServerId(), read in full: host +0x10C
// setSelectedServer("0.0.0.0", 0, 0) - the address the record fetches refuse. No argument, no result.
void as_multiplay_clearServerId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_clearServerId");
    (void) fn;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setSelectedServer(std::string("0.0.0.0"), 0, 0);
}

// Returns selected-server sponsor text or logo URL. Failed fetches produce an empty string.
// Requires player/host; cached records own their string data.
static void sponsorField(const gameswf::fn_call& fn, bool logoUrl)
{
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::string address;
    int port = 0, hostPort = 0;
    h->getSelectedServer(address, &port, &hostPort);                           // +0x110
    CachedRecord record;
    (void) fetchSponsorRecord(port, address, record, true);
    const std::string& field = logoUrl ? record.details.sponsorLogoUrl : record.details.sponsorText;
    fn.result->set_tu_string(tu_string(field.c_str()));
}
void as_multiplay_getSponsorText(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getSponsorText");
    sponsorField(fn, false);
}
void as_multiplay_getSponsorLogoUrl(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getSponsorLogoUrl");
    sponsorField(fn, true);
}

// Downloads argument zero through the server-logo manager and returns Boolean status.
// Requires player, host, and logo manager; brackets the operation with busy state.
void as_multiplay_downloadServerLogo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_downloadServerLogo");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    BusyHost busy = busyOpen();
    {
        const std::string url(fn.arg(0).to_string());
        IServerLogoManager* manager = static_cast<IServerLogoManager*>(h->getServerLogoManager());   // +0x4C
        fn.result->set_bool(manager->download(url));                           // +0x20
    }
    busyClose(busy);
}

// dice.bf2.Multiplay.getTeamName(index), read in full. The tab labels of
// players ON server: a string copy of the DLL-global team-name table (unk_100F87BC, two 28-byte
// std::strings) at arg(0) as a number. No host, and no bounds test: the shipped body indexes the
// 28-byte stride wherever the movie points it. The table is written only by
// General.cacheExtendedServerInfo.
void as_multiplay_getTeamName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getTeamName");
    const int index = (int) fn.arg(0).to_number();
    const renddx9abi::HostString name(s_teamNames[index]);
    fn.result->set_tu_string(tu_string(name.c_str()));
}

// dice.bf2.Multiplay.isServerOnline(id), read in full. The host is fetched
// and discarded, the id split with sscanf("%s %d %d") into locals nobody reads, and the answer is
// the Boolean false, always: a shipped stub, so the movie's "server online" test never passes.
void as_multiplay_isServerOnline(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_isServerOnline");
    (void) g_swiffPlayerInstance->getSwiffHost();
    char buf[260] = {0};
    int first = 0, second = 0;
    ::sscanf(fn.arg(0).to_string(), "%259s %d %d", buf, &first, &second);
    fn.result->set_bool(false);
}

// dice.bf2.Multiplay.setJoinPassword(text) and
// getJoinPassword(), read in full: host +0x184 setJoinPassword(arg(0) as a string), no result; and
// host +0x180 getJoinPassword() (a hidden-return string) as the string result. The movie asks for the
// password in a popup when a selected server has one, stores it here, and BF2Engine::joinServer
// reads it back into the GSPassword setting.
void as_multiplay_setJoinPassword(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setJoinPassword");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setJoinPassword(std::string(fn.arg(0).to_string()));                    // +0x184
}
void as_multiplay_getJoinPassword(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getJoinPassword");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string password = h->getJoinPassword();                         // +0x180
    fn.result->set_tu_string(tu_string(password.c_str()));
}

// Returns whether the current list is browsing, or false if no list exists.
// Requires player/host and destroys the kind string before querying list status.
void as_multiplay_isBrowsing(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_isBrowsing");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IServerBrowser* list;
    {
        const std::string kind = h->getServerListKind();       // +0x134
        list = h->getServerList(kind);                         // +0x7C
    }                                                          // the shipped body destroys it here
    fn.result->set_bool(list ? list->isBrowsing() : false);    // +0x1C
}

// Returns the language-setting name as a plain string. Requires player/host; reads no arguments.
void as_locale_getLanguage(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_locale_getLanguage");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string language = h->getLanguageSetting();                // +0x370
    fn.result->set_tu_string(tu_string(language.c_str()));
}

// the end-of-round bookmark indicator. The player host's +0x44 returns
// DemoLibrary (and refreshes it as part of that host body); its +0x40/+0x44 Boolean pair carries
// the flag. Both owners are unguarded, and the setter leaves the AS result untouched.
void as_multiplay_getHasNewBookmark(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getHasNewBookmark");
    IDemoLibrary* demos = static_cast<IDemoLibrary*>(g_swiffPlayerInstance->getSwiffHost()->getDemoLibrary());
    const bool value = demos->getFlagA4();
    fn.result->set_bool(value);
}

void as_multiplay_setHasNewBookmark(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setHasNewBookmark");
    const bool value = fn.arg(0).to_bool();
    IDemoLibrary* demos = static_cast<IDemoLibrary*>(g_swiffPlayerInstance->getSwiffHost()->getDemoLibrary());
    demos->setFlagA4(value);
}

// Menu event 12 becomes BF2Engine::playDemo(false, path).
void as_multiplay_playDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_playDemo");
    BusyHost busy = busyOpen();
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = (int) fn.arg(0).to_number();
    const DemoLibraryEntry* entry =
        static_cast<IDemoLibrary*>(h->getDemoLibrary())->getEntry(static_cast<unsigned int>(index));
    if (entry)
    {
        MenuEvent event;
        event.type = 12;
        event.args.push_back(entry->path);
        h->pushEvent(event);
    }
    busyClose(busy);
}

void as_multiplay_deleteDemo(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_deleteDemo");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const int index = (int) fn.arg(0).to_number();
    (void) static_cast<IDemoLibrary*>(h->getDemoLibrary())->removeEntry(static_cast<unsigned int>(index));
}


// dice.bf2.Mod - the namespace was published with no methods, so every one of its rows answered
// undefined. getCurrentModName is the one the server browser needs: it builds the
// `gamevariant = '<mod>'` filter clause, and an undefined answer turned that into the literal
// "gamevariant = 'undefined'".


// Returns the current mod's lowercase directory leaf as a narrow dice String object.
// Host lookup precedes object allocation; requires player/host.
void as_mod_getCurrentModName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_mod_getCurrentModName");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(nullptr, 0);
    const std::string name = h->getModName();                            // +0x364
    gameswf::assign_dice_string_object_narrow(text, name.c_str());
    fn.result->set_as_object_interface(text);
}

// sub_1005A7E0 - dice.bf2.PlayerStats.getCurrentUnlockDescription(): the selected-unlock name
// (selectFirstUnlock's string) looked up through the same MenuTeamManager walk as the equipment
// methods (sub_1005A680 is byte-for-byte sub_1005F5A0); the record's description key at +84
// localised into a dice String object; no record -> an empty String object.
void as_playerstats_getCurrentUnlockDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getCurrentUnlockDescription");
    vc71::string name(s_selectedUnlock.c_str());
    EquipmentKeys rec;
    if (!findEquipmentByName(name.c_str(), rec)) { fn.result->set_as_object_interface(gameswf::create_dice_string_object(0, 0)); return; }
    setLocalisedStringObject(fn, rec.descriptionKey);
}

// sub_10060F40 - dice.bf2.Profile.getCurrentRankString(): the stats record (+540 -> +432) gives the
// rank id at +160; SwiffHost +436(key&, rankId) names the rank's locale key (a callee-constructed
// std::string), localised through +168 into a dice String object.
void as_profile_getCurrentRankString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getCurrentRankString");
    void* h = asHost();
    if (!h) { fn.result->set_as_object_interface(gameswf::create_dice_string_object(0, 0)); return; }
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    (void) fetchPlayerStats(h, *rec);
    const int rankId = *reinterpret_cast<const int*>(rec->bytes + 160);
    delete rec;
    vc71::string key;
    hostRankKey(h, rankId, key);
    vc71::wstring text;
    hostLocaliseKey(h, key, text);
    fn.result->set_as_object_interface(gameswf::create_dice_string_object(reinterpret_cast<const unsigned short*>(text.c_str()), (int) text.size()));
}

// dice.bf2.Logic.beginWaitState(flag), and promoted.
// The global player's host (unguarded), then arg(0).to_number() (the bounds-checked accessor, an
// int 3 on a missing argument) converted by the x87 with the control word forced to truncate
// (fnstcw / or ah,0Ch / fistp), then the host's +0x2B0 setBusy(flag) (BF2.exe 0x488AB0: the busy word
// at +0xEBC - the menu bridge queues a mouse press only while +0x2B8 answers 0 - then UpdateWindow).
// Its Boolean is discarded; no AS result. The shipped player's own busy scope (sub_10013520) sets
// the same flag around its long calls. The old `if (!h)` and `nargs >= 1` were invented.
void as_logic_beginWaitState(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_beginWaitState");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    (void) h->setBusy(static_cast<int>(fn.arg(0).to_number()));                   // +0x2B0
}

// dice.bf2.Logic.endWaitState(), and promoted. No
// frame: the global player's host, then its +0x2B4 clearBusy (BF2.exe 0x488AE0), then `retn 0Ch`.
// No check, no argument, no result.
void as_logic_endWaitState(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_endWaitState");
    g_swiffPlayerInstance->getSwiffHost()->clearBusy();                           // +0x2B4
}

// Returns the player's mode flag as Boolean without a host lookup.
// [PLATFORM] This gameswf representation normalizes nonzero bytes to true.
void as_logic_getAsync(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getAsync");
    fn.result->set_bool(g_swiffPlayerInstance->getModeFlag() != 0);          // +0x7C
}

// Returns the team ID found by ID, rather than by index.
// Requires player, host, team manager, and an existing team; unknown IDs are not guarded.
void as_logic_getTeamsConId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_logic_getTeamsConId");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    IMenuTeamManager* tm = static_cast<IMenuTeamManager*>(h->getMenuTeamManager());   // +0x50
    const int id = (int) fn.arg(0).to_number();
    const MenuTeam* team = tm->findTeamById(id);                                      // +0x34
    fn.result->set_double((double) team->teamId);
}

// unk_100F882C - the DLL-global std::map<int, std::string> of server-browser filter clauses,
// keyed by the same bit mask the checkboxes use. setToggleFilter below writes it; its reader is
// Multiplay.updateFilter (PE 10056020 -> 10055700). The shipped map
// is constructed and destroyed by the DLL's own static initialiser pair at 100D6290 / 100D6A21.
// The retained vc71::string values are internal storage; this map never crosses the DLL boundary.
static std::map<int, vc71::string> s_filterValues;

// unk_100F8810 - the DLL-global free-text filter clause, written by Multiplay.setFilter
// (PE 100519C0, still unregistered here) and read - and rewritten in place - by the builder below.
static vc71::string s_textFilter;

// Publishes whether any bit in the supplied filter mask is enabled.
// Requires GeneralSettings and one numeric argument.
// [RETIRE-WHEN: RW-88] Retire after the remaining browser bindings and end-to-end checks are complete.
void as_multiplay_getToggleFilter(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getToggleFilter");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const int mask = (int) fn.arg(0).to_number();
    fn.result->set_bool(static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->getToggleFilter(mask));
}
// Sets a browser filter bit and its query clause from (mask, on, clause); publishes no result.
// Requires GeneralSettings and all three arguments. Empty/"%%" clauses clear the bit
// regardless of on and remove its stored clause; updateFilter applies the assembled query.
// [RETIRE-WHEN: RW-88] Retire with the remaining browser bindings and end-to-end filter checks.
void as_multiplay_setToggleFilter(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setToggleFilter");
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    const int   mask   = (int) fn.arg(0).to_number();
    const bool  on     = fn.arg(1).to_bool();
    vc71::string clause(fn.arg(2).to_string());
    const bool  blank  = clause.c_str()[0] == 0 || ::strcmp(clause.c_str(), "%%") == 0;
    static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->setToggleFilter(mask, blank ? false : on);
    if (blank)
    {
        // The shipped body looks the key up first and only erases when it is present; erase(key)
        // alone would behave the same, but the find is the observable call the original makes.
        if (s_filterValues.find(mask) != s_filterValues.end())
            s_filterValues.erase(mask);
    }
    else
    {
        s_filterValues[mask] = clause;
    }
}
// Builds the browser filter in key order. Clauses are ANDed except map-size choices,
// which form one OR group; the free-text filter follows the generated clauses.
static bool filterBitOn(IBase* gs, int mask)
{
    return static_cast<GeneralSettings*>(static_cast<IGeneralSettings*>(gs))->getToggleFilter(mask);
}
static bool isMapSizeFilter(int mask)
{
    return mask == 2048 || mask == 4096 || mask == 8192;
}
static void buildFilterString(vc71::string& out)
{
    IBase* gs = classManager->getSingleton(renddx9abi::HostString("GeneralSettings"));
    vc71::string filter;

    typedef std::map<int, vc71::string>::const_iterator Entry;
    unsigned int mapSizeArmed = 0;
    for (Entry it = s_filterValues.begin(); it != s_filterValues.end(); ++it)
        if (filterBitOn(gs, it->first) && it->second.c_str()[0] != 0 && isMapSizeFilter(it->first))
            ++mapSizeArmed;

    unsigned int mapSizeEmitted = 0;
    bool inMapSizeGroup = false;
    for (Entry it = s_filterValues.begin(); it != s_filterValues.end(); ++it)
    {
        const int mask = it->first;
        const vc71::string& clause = it->second;
        if (!filterBitOn(gs, mask) || clause.c_str()[0] == 0)
            continue;
        if (filter.c_str()[0] == 0)
        {
            if (isMapSizeFilter(mask))
            {
                inMapSizeGroup = true;
                if (mapSizeArmed > 1)
                    filter += "(";
                filter += clause;
                ++mapSizeEmitted;
            }
            else
            {
                filter += clause;
            }
        }
        else if (isMapSizeFilter(mask))
        {
            if (inMapSizeGroup)
                filter += " or ";
            else
                filter += mapSizeArmed > 1 ? " and (" : " and ";
            filter += clause;
            if (++mapSizeEmitted == mapSizeArmed && mapSizeArmed > 1)
                filter += ")";
            inMapSizeGroup = true;
        }
        else
        {
            filter += " and ";
            filter += clause;
        }
    }

    // The free-text filter Multiplay.setFilter stores. When there is nothing else in the query the
    // leading conjunction the movie leaves on it is stripped IN place - updateFilter rewrites what
    // setFilter stored, which is a real side effect and not a copy.
    if (s_textFilter.c_str()[0] != 0)
    {
        if (filter.c_str()[0] == 0)
        {
            // The shipped body strips with substr(4, size - 4) and substr(5, size - 5), which
            // throw std::out_of_range for a text filter of exactly "and" (respectively " and") -
            // the one length where the offset runs past the end. Reproducing the throw would mean
            // reproducing an exception this DLL has no handler for, and an unsigned underflow here
            // would be a wild copy rather than a throw, so the length tests stop one character
            // short of that boundary. Every other input is byte-for-byte the shipped result.
            if (s_textFilter.size() > 4 && ::strncmp(s_textFilter.c_str(), "and", 3) == 0)
                s_textFilter = vc71::string(s_textFilter.c_str() + 4, s_textFilter.size() - 4);
            if (s_textFilter.size() > 5 && ::strncmp(s_textFilter.c_str(), " and", 4) == 0)
                s_textFilter = vc71::string(s_textFilter.c_str() + 5, s_textFilter.size() - 5);
            filter = s_textFilter;
        }
        else
        {
            filter += s_textFilter;
        }
    }
    out = filter;
}

// Applies the generated filter to the current browser list inside a busy bracket.
// Requires player, host, and list; leaves the AS result untouched. [RETIRE-WHEN: RW-88]
void as_multiplay_updateFilter(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_updateFilter");
    IBase* cursorBase = classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ISwiffHost* cursorHost = static_cast<ISwiffHost*>(cursorBase);
    if (cursorHost) { SetCursor(cursorHost->getCursor(1)); cursorHost->setBusy(1); }
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    vc71::string filter;                                   // declared first, so it dies last
    buildFilterString(filter);
    const std::string kind = h->getServerListKind();       // +0x134
    IServerBrowser* list = h->getServerList(kind);         // +0x7C
    // Convert the builder's internal carrier to the host's native string, preserving embedded NULs.
    if (asLogEnabled()) ::renddx9abi::logf("[AS] updateFilter -> %s", filter.c_str());
    list->setFilter(std::string(filter.c_str(), filter.size()));                      // +0x54
    if (cursorHost) { SetCursor(cursorHost->getCursor(0)); cursorHost->clearBusy(); }
}


// bfhq award and leaderboard callbacks backed by SwiffHost caches.


// the award lists the host hands over
typedef IPlayerAwards::AwardList HostAwardList;

// One award as the rows read it; the shipped helpers copy the whole record out (sub_10014CA0).
struct AwardView
{
    int         type, id, level;
    long        when, first;
    std::string name, description;
    AwardView() : type(0), id(0), level(0), when(0), first(0) {}
};
static int awardCount(const HostAwardList& list)
{
    return (int) list.awards.size();
}
static AwardView awardAt(const HostAwardList& list, int i)
{
    AwardView v;
    const IPlayerAwards::Award& a = list.awards[i];
    v.name = a.name;
    v.description = a.description;
    v.type = a.type; v.id = a.id; v.level = a.level; v.when = a.when; v.first = a.first;
    return v;
}

// Host +0x21C (the stats player), then +0x228 getCachedAwards for it - how every award row and list
// opens. No caller reads the Boolean.
static void hostCachedAwards(void* h, HostAwardList& out)
{
    const int pid = hostSelectedPlayerId(h);                                               // +0x21C
    (void) static_cast<ISwiffHost*>(h)->getCachedAwards(pid, &out);                       // +0x228
}
// Host +0x34 (the lazy PlayerAwards object), then its +0x20 sortListDate(list, flag), whose answer
// comes back by value. The shipped `true` sorts newest first (the comparator PlayerAwards.cpp calls
// AscendingDateSort compares with >), `false` oldest first.
static void hostSortAwardsByDate(void* h, const HostAwardList& in, bool newestFirst, HostAwardList& out)
{
    IPlayerAwards* awards = static_cast<IPlayerAwards*>(static_cast<ISwiffHost*>(h)->getPlayerAwards());
    out = awards->sortListDate(in, newestFirst);
}
// Host +0x34, then PlayerAwards +0x24 getAllAwards: the catalogue awards.con registered, in its map
// order (PlayerAwards keys a badge by level first, so a badge's lowest level comes first).
static void hostAllAwards(void* h, HostAwardList& out)
{
    static_cast<IPlayerAwards*>(static_cast<ISwiffHost*>(h)->getPlayerAwards())->getAllAwards(out);
}
// Host +0x34, then PlayerAwards +0x2C matchModAward(id, mod): the mod - the SelectedModFilter string
// the list prologue fetched - goes BY value, as shipped (the callee destroys it).
static bool hostMatchModAward(void* h, int id, const char* mod)
{
    return static_cast<IPlayerAwards*>(static_cast<ISwiffHost*>(h)->getPlayerAwards())->matchModAward(id, std::string(mod));
}
// Host +0x22C formatDate(&time) ("%d-%s-%.2d", or "-") and +0x230 formatDuration(seconds). The
// shipped callers pass a 32-bit time; the native host takes a time_t, widened into a local here.
static std::string hostFormatDate(void* h, long when)
{
    const time_t t = when;
    return static_cast<ISwiffHost*>(h)->formatDate(&t);
}
static std::string hostFormatDuration(void* h, int seconds)
{
    return static_cast<ISwiffHost*>(h)->formatDuration(seconds);
}

// the selected award
// unk_100F8854: the award the Profile getters describe, as a key - "<id>" for a medal or a ribbon,
// "<id>_<level>" for a badge. selectAward stores its argument here and selectLatestAward the newest
// award's key; the getters parse it back. "" (no selection) parses to id 0, which sub_10060160 reads
// as "the OLDEST award".
static std::string s_selectedAward;

// sub_1005CFA0 - a key back into (id, level): "" is (0, 0); anything else is sscanf'd "%d_%d" into
// two zeroed ints, so "<id>" leaves the level 0.
static void parseAwardKey(const std::string& key, int* id, int* level)
{
    if (key == "")
    {
        *id = 0;
        *level = 0;
        return;
    }
    int parsedId = 0, parsedLevel = 0;
    ::sscanf(key.c_str(), "%d_%d", &parsedId, &parsedLevel);
    *id = parsedId;
    *level = parsedLevel;
}

// sub_10060160 - the award (id, level) names, from the stats player's cached list (host +0x21C and
// +0x228, both asked first in either arm). id 0: the list sorted oldest first (PlayerAwards +0x20
// with false) and its first award, when there is one. Otherwise the first cached award with that id
// that is a medal or a ribbon (type 0 or 2, any level), or a badge at exactly that level.
static bool findAward(int id, int level, AwardView& out)
{
    void* h = g_swiffPlayerInstance->getSwiffHost();
    HostAwardList cached;
    hostCachedAwards(h, cached);
    if (!id)
    {
        HostAwardList sorted;
        hostSortAwardsByDate(h, cached, false, sorted);
        if (!awardCount(sorted))
            return false;
        out = awardAt(sorted, 0);
        return true;
    }
    for (int i = 0; i < awardCount(cached); ++i)
    {
        const AwardView award = awardAt(cached, i);
        if (award.id != id)
            continue;
        if (award.type == 0 || award.type == 2 || award.level == level)
        {
            out = award;
            return true;
        }
    }
    return false;
}

// Profile.hasAward(): whether the stats player has any award at all.
void as_profile_hasAward(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_hasAward");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    HostAwardList cached;
    hostCachedAwards(h, cached);
    fn.result->set_bool(awardCount(cached) != 0);
}

// Profile.selectAward(key): the key becomes the selection first, whatever follows;
// the answer is whether the stats player has an award of exactly that id AND level - "<id>" parses to
// level 0, so it matches only an award whose level is 0.
void as_profile_selectAward(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_selectAward");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    const std::string key(fn.arg(0).to_string());
    s_selectedAward = key;
    HostAwardList cached;
    hostCachedAwards(h, cached);
    int id, level;
    parseAwardKey(s_selectedAward, &id, &level);
    bool found = false;
    for (int i = 0; i < awardCount(cached) && !found; ++i)
    {
        const AwardView award = awardAt(cached, i);
        found = award.id == id && award.level == level;
    }
    fn.result->set_bool(found);
}

// Profile.selectLatestAward(): the selection cleared, then the stats player's awards
// sorted newest first (PlayerAwards +0x20 with true); the first medal, badge or ribbon becomes the
// selection ("<id>", "<id>_<level>", "<id>") and the answer is true. None: "" and false.
void as_profile_selectLatestAward(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_selectLatestAward");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    s_selectedAward = "";
    HostAwardList cached;
    hostCachedAwards(h, cached);
    HostAwardList latest;
    hostSortAwardsByDate(h, cached, true, latest);
    for (int i = 0; i < awardCount(latest); ++i)
    {
        const AwardView award = awardAt(latest, i);
        if (award.type == 0 || award.type == 2)
        {
            s_selectedAward = swiffFormatString("%d", award.id);
            fn.result->set_bool(true);
            return;
        }
        if (award.type == 1)
        {
            s_selectedAward = swiffFormatString("%d_%d", award.id, award.level);
            fn.result->set_bool(true);
            return;
        }
    }
    s_selectedAward = "";
    fn.result->set_bool(false);
}

// Profile.hasThisAward(key): its OWN argument parsed (the selection is untouched) and
// looked up as sub_10060160 does - so "" asks whether the player has any award.
void as_profile_hasThisAward(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_hasThisAward");
    const std::string key(fn.arg(0).to_string());
    int id, level;
    parseAwardKey(key, &id, &level);
    AwardView award;
    fn.result->set_bool(findAward(id, level, award));
}

// Profile.getAwardName() / getAwardDescription(): the host fetched and
// dropped, a dice String object built, then the selected award's locale key (+8 name / +36
// description) localised through host +0xA8 (sub_1005CF10: asked even for an empty key) into its
// wide side, the narrow side emptied. No award: the wide side cleared and the narrow side " " for the
// name but "" for the description. Both preserved.
static void awardText(const gameswf::fn_call& fn, bool description)
{
    (void) g_swiffPlayerInstance->getSwiffHost();
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(nullptr, 0);
    int id, level;
    parseAwardKey(s_selectedAward, &id, &level);
    AwardView award;
    if (findAward(id, level, award))
    {
        vc71::wstring wide;
        hostLocaliseKey(static_cast<void*>(g_swiffPlayerInstance->getSwiffHost()),
                        vc71::string((description ? award.description : award.name).c_str()), wide);
        gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
    }
    else
    {
        gameswf::assign_dice_string_object_narrow(text, description ? "" : " ");
    }
    fn.result->set_as_object_interface(text);
}
void as_profile_getAwardName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getAwardName");
    awardText(fn, false);
}
void as_profile_getAwardDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getAwardDescription");
    awardText(fn, true);
}

// Profile.getAwardLevel(): the selected award's level as a number, 0 without one.
void as_profile_getAwardLevel(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getAwardLevel");
    int id, level;
    parseAwardKey(s_selectedAward, &id, &level);
    AwardView award;
    fn.result->set_double(findAward(id, level, award) ? (double) award.level : 0.0);
}

// Profile.getAwardLatestDate() / getAwardFirstDate(): the host fetched
// first, then the selected award's "when" (+0x48) or "first" (+0x4C) through host +0x22C formatDate
// as a string; "" without an award.
static void awardDate(const gameswf::fn_call& fn, bool first)
{
    void* h = g_swiffPlayerInstance->getSwiffHost();
    int id, level;
    parseAwardKey(s_selectedAward, &id, &level);
    AwardView award;
    if (findAward(id, level, award))
        fn.result->set_tu_string(tu_string(hostFormatDate(h, first ? award.first : award.when).c_str()));
    else
        fn.result->set_tu_string(tu_string(""));
}
void as_profile_getAwardLatestDate(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getAwardLatestDate");
    awardDate(fn, false);
}
void as_profile_getAwardFirstDate(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getAwardFirstDate");
    awardDate(fn, true);
}

// The list bodies below are declared with the other list prototypes inside the anonymous
// namespace, so they are defined in it too (reopened here, as the StatsTable block reopens it).
namespace {

// the award lists (sub_10022610)
// "latestAwards" (1002D12C): the stats player's awards newest first. Every award is counted; one
// inside the window gets a row only when its name key is non-empty (an award the catalogue does not
// know). Row: id "<id>_<level>" for a badge, "<id>" otherwise; "type"; the level under the field name
// " " [sic - a single space, the shipped literal at 0x100D996C, so the movie's "level" is undefined
// here]; "date" (formatDate of "when"); "name" (the localised name, a dice String).
static void listLatestAwards(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    r.begin();
    HostAwardList cached;
    hostCachedAwards(h, cached);
    HostAwardList latest;
    hostSortAwardsByDate(h, cached, true, latest);
    int total = 0;
    for (int i = 0; i < awardCount(latest); ++i)
    {
        const AwardView award = awardAt(latest, i);
        if ((unsigned int) i >= (unsigned int) start && (unsigned int) i < (unsigned int) end && !award.name.empty())
        {
            char id[32];
            if (award.type == 1) ::sprintf(id, "%d_%d", award.id, award.level);
            else                 ::sprintf(id, "%d", award.id);
            r.beginRowStr(id);
            r.setInt("type", award.type);
            r.setInt(" ", award.level);
            r.setString("date", hostFormatDate(h, award.when).c_str());
            HostWide wide;
            hostLocalise(h, award.name.c_str(), wide);
            r.setWide("name", wide.c_str(), (int) wide.size());
        }
        total = i + 1;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "medals" (1002D3C6): the catalogue's medals (getAllAwards) in its order, each one the selected mod
// knows (matchModAward - asked of every medal, and only those it accepts are counted; the window is
// tested on that count). Row: id "<id>", "type", "name" (the catalogue name localised), "level" - the
// level of the stats player's first cached award with that id, 0 when the player has none.
static void listMedals(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    r.begin();
    HostAwardList cached;
    hostCachedAwards(h, cached);
    HostAwardList all;
    hostAllAwards(h, all);
    int total = 0;
    for (int d = 0; d < awardCount(all); ++d)
    {
        const AwardView def = awardAt(all, d);
        if (def.type != 0)
            continue;
        if (!hostMatchModAward(h, def.id, s_selectedModFilter.c_str()))
            continue;
        if ((unsigned int) total >= (unsigned int) start && (unsigned int) total < (unsigned int) end)
        {
            char id[32];
            ::sprintf(id, "%d", def.id);
            r.beginRowStr(id);
            r.setInt("type", def.type);
            HostWide wide;
            hostLocalise(h, def.name.c_str(), wide);
            r.setWide("name", wide.c_str(), (int) wide.size());
            int level = 0;
            for (int c = 0; c < awardCount(cached); ++c)
            {
                const AwardView own = awardAt(cached, c);
                if (own.id == def.id)
                {
                    level = own.level;
                    break;
                }
            }
            r.setInt("level", level);
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "badges" (1002D65F): one row per badge ID - the catalogue's first definition of an id (its lowest
// level) that the selected mod knows; a definition the mod refuses leaves the id open for the next
// one. That definition is kept in a map, and inside the window every cached award of that id that
// is of a higher level replaces it. A player with the badge: id "<id>_<level>", "level", "type" (the
// definition's), "name" (the kept award's name localised). Without: id "<id>_0", "level" 0, "type",
// and "name" from an empty key. Every accepted id is counted.
static void listBadges(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    r.begin();
    HostAwardList cached;
    hostCachedAwards(h, cached);
    HostAwardList all;
    hostAllAwards(h, all);
    std::map<int, AwardView> best;
    int total = 0;
    for (int d = 0; d < awardCount(all); ++d)
    {
        const AwardView def = awardAt(all, d);
        if (def.type != 1)
            continue;
        if (best.find(def.id) != best.end())
            continue;
        if (!hostMatchModAward(h, def.id, s_selectedModFilter.c_str()))
            continue;
        best[def.id] = def;
        if ((unsigned int) total >= (unsigned int) start && (unsigned int) total < (unsigned int) end)
        {
            bool found = false;
            for (int c = 0; c < awardCount(cached); ++c)
            {
                const AwardView own = awardAt(cached, c);
                if (own.id == def.id)
                {
                    if (own.level > best[def.id].level)
                        best[def.id] = own;
                    found = true;
                }
            }
            char id[32];
            HostWide wide;
            if (found)
            {
                const AwardView& kept = best[def.id];
                ::sprintf(id, "%d_%d", def.id, kept.level);
                r.beginRowStr(id);
                r.setInt("level", kept.level);
                r.setInt("type", def.type);
                hostLocalise(h, kept.name.c_str(), wide);
            }
            else
            {
                ::sprintf(id, "%d_%d", def.id, 0);
                r.beginRowStr(id);
                r.setInt("level", 0);
                r.setInt("type", def.type);
                hostLocalise(h, "", wide);
            }
            r.setWide("name", wide.c_str(), (int) wide.size());
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "ribbons" (1002DA92): the stats player's awards oldest first; every ribbon is counted, and one
// inside the window gets a row only when the selected mod knows it (matchModAward is asked of the
// windowed ribbons only). Row: id "<id>", "type", "level", "date" (formatDate of "when"), "name".
static void listRibbons(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    r.begin();
    HostAwardList cached;
    hostCachedAwards(h, cached);
    HostAwardList sorted;
    hostSortAwardsByDate(h, cached, false, sorted);
    int total = 0;
    for (int i = 0; i < awardCount(sorted); ++i)
    {
        const AwardView award = awardAt(sorted, i);
        if (award.type != 2)
            continue;
        if ((unsigned int) total >= (unsigned int) start && (unsigned int) total < (unsigned int) end
            && hostMatchModAward(h, award.id, s_selectedModFilter.c_str()))
        {
            char id[32];
            ::sprintf(id, "%d", award.id);
            r.beginRowStr(id);
            r.setInt("type", award.type);
            r.setInt("level", award.level);
            r.setString("date", hostFormatDate(h, award.when).c_str());
            HostWide wide;
            hostLocalise(h, award.name.c_str(), wide);
            r.setWide("name", wide.c_str(), (int) wide.size());
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

} // anonymous namespace (the award lists)

// the leaderboard cache the host copies out
// SwiffLeaderboardCache (ISwiffHost.h) describes the 31 groups, the four row shapes and the request
// numbers; leaderboardGroupOf below is its request -> group table.
typedef SwiffLeaderboardCache HostLeaderboard;

// A row as the builder reads it, whatever its shape.
struct LeaderboardRowView
{
    int         n, pid, playerRank, totalTime;
    int         values[4];
    std::string nick, countryCode, weeklyScore;
    LeaderboardRowView() : n(0), pid(0), playerRank(0), totalTime(0) { values[0] = values[1] = values[2] = values[3] = 0; }
};

// Request -> group: requests 3 and 4 swap, vehicles (12..18) are groups 24..30, weapons (19..30)
// groups 12..23.
static int leaderboardGroupOf(int request)
{
    if (request == 3) return 4;
    if (request == 4) return 3;
    if (request >= 12 && request <= 18) return request + 12;
    if (request >= 19 && request <= 30) return request - 7;
    return request;
}

// The typed cache's group g (0..30), visited with its own row type.
template <class Visitor>
static void visitCacheGroup(const SwiffLeaderboardCache& c, int g, Visitor visit)
{
    if (g == 0) visit(c.overall);
    else if (g == 1) visit(c.risingStar);
    else if (g == 2) visit(c.commander);
    else if (g == 3) visit(c.combat);
    else if (g == 4) visit(c.teamwork);
    else if (g >= 5 && g <= 11) visit(c.kits[g - 5]);
    else if (g >= 12 && g <= 23) visit(c.weapons[g - 12]);
    else if (g >= 24 && g <= 30) visit(c.vehicles[g - 24]);
}
static void toView(const SwiffLeaderboardScoreRow& row, LeaderboardRowView& v)
{
    v.values[0] = row.values[0]; v.values[1] = row.values[1];
}
static void toView(const SwiffLeaderboardRisingStarRow& row, LeaderboardRowView& v)
{
    v.weeklyScore = row.weeklyScore; v.totalTime = row.totalTime;
}
static void toView(const SwiffLeaderboardRow& row, LeaderboardRowView& v)
{
    v.values[0] = row.values[0]; v.values[1] = row.values[1]; v.values[2] = row.values[2];
}
static void toView(const SwiffLeaderboardWeaponRow& row, LeaderboardRowView& v)
{
    v.values[0] = row.values[0]; v.values[1] = row.values[1]; v.values[2] = row.values[2]; v.values[3] = row.values[3];
}

static int leaderboardRowCount(const HostLeaderboard& c, int g)
{
    int count = 0;
    visitCacheGroup(c, g, [&](const auto& group) { count = (int) group.rows.size(); });
    return count;
}
static LeaderboardRowView leaderboardRowAt(const HostLeaderboard& c, int g, int i)
{
    LeaderboardRowView v;
    visitCacheGroup(c, g, [&](const auto& group) {
        const auto& row = group.rows[i];
        v.n = row.n; v.pid = row.pid; v.nick = row.nick; v.playerRank = row.playerRank; v.countryCode = row.countryCode;
        toView(row, v);
    });
    return v;
}
static int leaderboardNumPlayers(const HostLeaderboard& c, int g)
{
    int n = 0;
    visitCacheGroup(c, g, [&](const auto& group) { n = group.numPlayers; });
    return n;
}
static std::string leaderboardUpdated(const HostLeaderboard& c, int g)
{
    std::string s;
    visitCacheGroup(c, g, [&](const auto& group) { s = group.updated; });
    return s;
}

// The leaderboard host slots, typed on the native side.
static int hostLeaderboardCategory(void* h)
{
    return static_cast<ISwiffHost*>(h)->getLeaderboardCategory();                          // +0x13C
}
static int hostLeaderboardId(void* h)
{
    return static_cast<ISwiffHost*>(h)->getLeaderboardId();                                // +0x144
}
static void hostSetLeaderboardId(void* h, int id)
{
    static_cast<ISwiffHost*>(h)->setLeaderboardId(id);                                     // +0x148
}
static int hostLeaderboardPosition(void* h)
{
    return static_cast<ISwiffHost*>(h)->getLeaderboardPosition();                          // +0x14C
}
static void hostSetLeaderboardPosition(void* h, int position)
{
    static_cast<ISwiffHost*>(h)->setLeaderboardPosition(position);                         // +0x150
}
static void hostGoLeaderboardPlayer(void* h, int playerId)
{
    static_cast<ISwiffHost*>(h)->goLeaderboardPlayer(playerId);                            // +0x154
}
static bool hostGetLeaderboard(void* h, HostLeaderboard& out, int request, int position)
{
    return static_cast<ISwiffHost*>(h)->getLeaderboard(&out, request, position);           // +0x1CC
}
static bool hostGetLeaderboardPlayer(void* h, int playerId, HostLeaderboard& out, int request)
{
    return static_cast<ISwiffHost*>(h)->getLeaderboardPlayer(playerId, &out, request);     // +0x1D0
}
static bool hostFetchLeaderboard(void* h, int playerId, int request, int position)
{
    return static_cast<ISwiffHost*>(h)->fetchLeaderboard(playerId, request, position);     // +0x1D4
}

// The category tables the shipped DLL builds at load (sub_100D60A0 for the scores, 0x100F7060; the
// kits, vehicles and weapons are the bfhq favourite tables above: 0x100F3248, 0x100F6BA8,
// 0x100F6C70). Multiplay.getLeaderboardName / Description index them by the selected board with no
// bound test, as shipped.
static const char* const kLeaderboardScoreKeys[5] = {
    "BFHQ_lb_overall", "BFHQ_lb_risingstar", "BFHQ_lb_commander", "BFHQ_lb_teamwork", "BFHQ_lb_combatc" };
static const char* leaderboardKey(int category, int board)
{
    switch (category)
    {
    case 0: return kLeaderboardScoreKeys[board];
    case 1: return kFavouriteKitKeys[board];
    case 2: return kFavouriteVehicleKeys[board];
    case 3: return kFavouriteWeaponKeys[board];
    default: return 0;
    }
}

// Multiplay.getLeaderboardCategoryId() / getLeaderboardId(): host +0x13C /
// +0x144 as a number.
void as_multiplay_getLeaderboardCategoryId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getLeaderboardCategoryId");
    fn.result->set_double((double) hostLeaderboardCategory(g_swiffPlayerInstance->getSwiffHost()));
}
void as_multiplay_getLeaderboardId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getLeaderboardId");
    fn.result->set_double((double) hostLeaderboardId(g_swiffPlayerInstance->getSwiffHost()));
}
// Multiplay.setLeaderboardId(id): the host first, then argument 0 truncated, to
// host +0x148. No result.
void as_multiplay_setLeaderboardId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setLeaderboardId");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    hostSetLeaderboardId(h, static_cast<int>(fn.arg(0).to_number()));
}

// Multiplay.getLeaderboardName() / getLeaderboardDescription(), inside the
// busy scope: the selected category's table entry for the selected board (plus "_desc" for the
// description; categories 0..3, anything else the empty key), localised through a second host fetch
// (+0xA8, asked even for the empty key) into a new dice String object - wide side the text, narrow
// side empty.
static void leaderboardText(const gameswf::fn_call& fn, bool description)
{
    BusyHost busy = busyOpen();
    void* h = g_swiffPlayerInstance->getSwiffHost();
    const int category = hostLeaderboardCategory(h);                                       // +0x13C
    const int board = hostLeaderboardId(h);                                                // +0x144
    std::string key;
    const char* entry = (category >= 0 && category <= 3) ? leaderboardKey(category, board) : 0;
    if (entry)
        key = description ? std::string(entry) + "_desc" : std::string(entry);
    vc71::wstring wide;
    hostLocaliseKey(static_cast<void*>(g_swiffPlayerInstance->getSwiffHost()), vc71::string(key.c_str()), wide);
    gameswf::as_object_interface* text = gameswf::create_dice_string_object(nullptr, 0);
    gameswf::assign_dice_string_object_wide(text, reinterpret_cast<const Uint16*>(wide.c_str()));
    fn.result->set_as_object_interface(text);
    busyClose(busy);
}
void as_multiplay_getLeaderboardName(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getLeaderboardName");
    leaderboardText(fn, false);
}
void as_multiplay_getLeaderboardDescription(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getLeaderboardDescription");
    leaderboardText(fn, true);
}

// Leaderboard navigation publishes no result. First selects position 1; next/previous step by 20.
// Previous clamps to 1; last only fetches the host and leaves the position unchanged.
void as_multiplay_goFirstLeaderboard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_goFirstLeaderboard");
    BusyHost busy = busyOpen();
    hostSetLeaderboardPosition(g_swiffPlayerInstance->getSwiffHost(), 1);
    busyClose(busy);
}
void as_multiplay_goLastLeaderboard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_goLastLeaderboard");
    BusyHost busy = busyOpen();
    (void) g_swiffPlayerInstance->getSwiffHost();
    busyClose(busy);
}
void as_multiplay_goNextLeaderboard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_goNextLeaderboard");
    BusyHost busy = busyOpen();
    void* h = g_swiffPlayerInstance->getSwiffHost();
    hostSetLeaderboardPosition(h, hostLeaderboardPosition(h) + 20);
    busyClose(busy);
}
void as_multiplay_goPreviousLeaderboard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_goPreviousLeaderboard");
    BusyHost busy = busyOpen();
    void* h = g_swiffPlayerInstance->getSwiffHost();
    if (hostLeaderboardPosition(h) - 20 >= 1)
        hostSetLeaderboardPosition(h, hostLeaderboardPosition(h) - 20);
    else
        hostSetLeaderboardPosition(h, 1);
    busyClose(busy);
}
// Multiplay.goLeaderboardPlayer(id): inside the busy scope, the host, then argument 0
// truncated to host +0x154. No result.
void as_multiplay_goLeaderboardPlayer(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_goLeaderboardPlayer");
    BusyHost busy = busyOpen();
    void* h = g_swiffPlayerInstance->getSwiffHost();
    hostGoLeaderboardPlayer(h, static_cast<int>(fn.arg(0).to_number()));
    busyClose(busy);
}

// PlayerStats.getLeaderBoard(id, request): both arguments truncated, a fresh cache,
// then host +0x1D0 (that player's boards) for a non-zero id or +0x1CC (the global boards) at
// position 0 - which differs from any cached position (at least 1), so the global form always
// refetches. The host's answer as a Boolean; the cache is thrown away.
void as_playerstats_getLeaderBoard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getLeaderBoard");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    const int id = static_cast<int>(fn.arg(0).to_number());
    const int request = static_cast<int>(fn.arg(1).to_number());
    HostLeaderboard cache;
    const bool ok = id ? hostGetLeaderboardPlayer(h, id, cache, request) : hostGetLeaderboard(h, cache, request, 0);
    fn.result->set_bool(ok);
}
// PlayerStats.refreshLeaderBoard(id, request): host +0x1D4(id, request, 0), a Boolean.
void as_playerstats_refreshLeaderBoard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_refreshLeaderBoard");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    const int id = static_cast<int>(fn.arg(0).to_number());
    const int request = static_cast<int>(fn.arg(1).to_number());
    fn.result->set_bool(hostFetchLeaderboard(h, id, request, 0));
}

// Legacy leaderboard requests by category/board.
// [CHOSEN] Out-of-range requests follow contiguous table entries while available, then
// suppress fetching; out-of-range result groups yield zero count or empty date.
static const int kLeaderboardRequestTable[26] = {
    5, 6, 7, 8, 9, 10, 11,  12, 13, 14, 15, 16, 17, 18,  19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30 };
static bool selectedLeaderboardGroup(void* h, HostLeaderboard& cache, int* group)
{
    const int category = hostLeaderboardCategory(h);                                       // +0x13C
    const int board = hostLeaderboardId(h);                                                // +0x144
    const int position = hostLeaderboardPosition(h);                                       // +0x14C
    int request = -1;
    switch (category)
    {
    case 0:
        if ((unsigned int) board > 4)
            return false;
        request = board;
        *group = leaderboardGroupOf(board);
        break;
    case 1: *group = 5 + board;  request = (board >= 0 && board < 26) ? kLeaderboardRequestTable[board] : -1; break;
    case 2: *group = 24 + board; request = (board >= 0 && board < 19) ? kLeaderboardRequestTable[7 + board] : -1; break;
    case 3: *group = 12 + board; request = (board >= 0 && board < 12) ? kLeaderboardRequestTable[14 + board] : -1; break;
    default:
        return false;
    }
    (void) hostGetLeaderboard(h, cache, request, position);                                // +0x1CC
    return *group >= 0 && *group <= 30;
}
void as_playerstats_getNumPlayersInLeaderboard(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getNumPlayersInLeaderboard");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    HostLeaderboard cache;
    int group = 0;
    const int players = selectedLeaderboardGroup(h, cache, &group) ? leaderboardNumPlayers(cache, group) : 0;
    fn.result->set_double((double) players);
}
void as_playerstats_getLeaderboardDate(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getLeaderboardDate");
    void* h = g_swiffPlayerInstance->getSwiffHost();
    HostLeaderboard cache;
    int group = 0;
    const std::string date = selectedLeaderboardGroup(h, cache, &group) ? leaderboardUpdated(cache, group) : std::string();
    fn.result->set_tu_string(tu_string(date.c_str()));
}

// The list bodies below are declared with the other list prototypes inside the anonymous
// namespace, so they are defined in it too (reopened here, as the StatsTable block reopens it).
namespace {

// Builds up to twenty leaderboard rows but counts the full selected group.
// Marks the local/statistics player and adds category-specific values. Unknown categories,
// including category 4, emit no rows. Requires the local profile.
static int buildLeaderboardRows(const HostLeaderboard& cache, ListResult& r)
{
    void* h = g_swiffPlayerInstance->getSwiffHost();
    IBase* pm = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    const int myId = static_cast<IProfileManager*>(pm)->getCurrentProfile()->profileId;             // +0x2C, +0x48
    int count = 0;
    const int category = hostLeaderboardCategory(h);                                       // +0x13C
    const int board = hostLeaderboardId(h);                                                // +0x144
    // The family decides the group and the trailing columns: 0 score int, 1 weekly text, 2 kills +
    // time, 3 kills + time + accuracy; -1 writes nothing.
    int group = -1, family = -1;
    if (category == 0)
    {
        switch (board)
        {
        case 0: group = 0; family = 0; break;
        case 1: group = 1; family = 1; break;
        case 2: group = 2; family = 0; break;
        case 3: group = 4; family = 0; break;
        case 4: group = 3; family = 0; break;
        default: break;
        }
    }
    else if (category == 1) { group = 5 + board;  family = 2; }
    else if (category == 2) { group = 24 + board; family = 2; }
    else if (category == 3) { group = 12 + board; family = 3; }
    // [CHOSEN] a board past the cache's 31 groups (retail reads beyond the cache) writes nothing.
    if (family >= 0 && group >= 0 && group <= 30)
    {
        const int rows = leaderboardRowCount(cache, group);
        for (int i = 0; i < rows; ++i, ++count)
        {
            if (i >= 20)
                continue;
            const LeaderboardRowView row = leaderboardRowAt(cache, group, i);
            r.beginRow(row.pid);
            if (row.pid == myId)
            {
                r.setInt("selected", 1);
                r.setInt("isme", 1);
            }
            else if (row.pid == hostSelectedPlayerId(h))                                  // +0x21C
            {
                r.setInt("selected", 1);
                r.setInt("isme", 0);
            }
            r.pushInt(row.n);
            r.pushNarrow(("images/flags/" + row.countryCode + ".png").c_str());
            r.pushNarrow(swiffFormatString("images/ranks/front/rank_%d.png", row.playerRank).c_str());
            r.pushNarrow(row.nick.c_str());
            if (family == 0)
                r.pushInt(row.values[0]);
            else if (family == 1)
                r.pushNarrow(row.weeklyScore.c_str());
            else
            {
                r.pushInt(row.values[0]);
                r.pushInt(row.values[1]);
                r.pushNarrow(hostFormatDuration(h, row.values[2]).c_str());               // +0x230
                if (family == 3)
                    r.pushInt(row.values[3]);
            }
        }
    }
    r.setTotalCount(count);
    return count;
}

// "lboardcat" (1002B8D8): the boards of the selected category (+0x13C). Scores: the five score
// keys; kits: the seven kit keys; vehicles: the vehicle keys 0..6 without 5 (neither written nor
// counted); weapons: the first nine weapon keys; each a localised wide column in a row keyed by its
// index, row 0 "selected". Rising star (4): one row, id 0, the narrow literal "Rising Star",
// "selected", and a total of 1 whatever the window. Anything else: empty.
static void listLeaderboardCategories(const gameswf::fn_call& fn, ListResult& r, ListHost h, int start, int end)
{
    const int category = hostLeaderboardCategory(h);                                       // +0x13C
    r.begin();
    int total = 0;
    const char* const* keys = 0;
    int boards = 0;
    switch (category)
    {
    case 0: keys = kLeaderboardScoreKeys; boards = 5; break;
    case 1: keys = kFavouriteKitKeys;     boards = 7; break;
    case 2: keys = kFavouriteVehicleKeys; boards = 7; break;
    case 3: keys = kFavouriteWeaponKeys;  boards = 9; break;
    case 4:
        r.beginRow(0);
        r.pushNarrow("Rising Star");
        r.setInt("selected", 1);
        total = 1;
        break;
    default: break;
    }
    for (int i = 0; i < boards; ++i)
    {
        if (category == 2 && i == 5)
            continue;
        if ((unsigned int) i >= (unsigned int) start && (unsigned int) i < (unsigned int) end)
        {
            r.beginRow(i);
            HostWide wide;
            hostLocalise(h, keys[i], wide);
            r.pushWide(wide.c_str(), (int) ::wcslen(wide.c_str()));
            if (i == 0)
                r.setInt("selected", 1);
        }
        ++total;
    }
    r.setTotalCount(total);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

// "lboard" (1002BDED): the list begun, then the category, board and position (+0x13C, +0x144,
// +0x14C), then the busy scope. A -1 board or category writes nothing - not even a totalCount.
// Scores boards 0..4 (requests 0..4) and rising star (request 1) fetch through host +0x1CC at the
// position and build; kits, vehicles and weapons build even for an unknown board (an empty cache,
// nothing fetched) - and the vehicle arm has no board 5 (request 17), so that board always builds
// empty. A score board outside 0..4 writes nothing.
static void listLeaderboard(const gameswf::fn_call& fn, ListResult& r, ListHost h)
{
    r.begin();
    const int category = hostLeaderboardCategory(h);                                       // +0x13C
    const int board = hostLeaderboardId(h);                                                // +0x144
    const int position = hostLeaderboardPosition(h);                                       // +0x14C
    BusyHost busy = busyOpen();
    if (board != -1 && category != -1)
    {
        int request = -1;
        bool build = false;
        switch (category)
        {
        case 0: if ((unsigned int) board <= 4) { request = board; build = true; } break;
        case 1: build = true; if ((unsigned int) board <= 6) request = 5 + board; break;
        case 2: build = true; if ((unsigned int) board <= 6 && board != 5) request = 12 + board; break;
        case 3: build = true; if ((unsigned int) board <= 11) request = 19 + board; break;
        case 4: build = true; request = 1; break;
        default: break;
        }
        if (build)
        {
            HostLeaderboard cache;
            if (request != -1)
                (void) hostGetLeaderboard(h, cache, request, position);                    // +0x1CC
            (void) buildLeaderboardRows(cache, r);
        }
    }
    fn.result->set_as_object_interface(r.arr.get_ptr());
    busyClose(busy);
}

// Builds leaderboard rows for the profile and selected player, then overwrites totalCount with zero.
// [CHOSEN] Missing profile IDs start at -1 instead of using an uninitialized stack value.
static void listLeaderboardPlayers(const gameswf::fn_call& fn, ListResult& r, ListHost h)
{
    BusyHost busy = busyOpen();
    IBase* pm = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    const int category = hostLeaderboardCategory(h);                                       // +0x13C
    const int board = hostLeaderboardId(h);                                                // +0x144
    r.begin();
    int players[2] = { -1, -1 };
    int id = -1;
    std::string otherNick;
    (void) static_cast<IProfileManager*>(pm)->getProfileId(id);                           // +0x30
    players[0] = id;
    if (h->getPendingRecord(&id, otherNick))                                               // +0x214
        players[1] = id;
    int total = 0;
    if (category != -1 && board != -1)
    {
        for (int k = 0; k < 2; ++k)
        {
            const int player = players[k];
            if (player == -1)
                continue;
            int request = -1;
            bool build = false;
            switch (category)
            {
            case 0: if ((unsigned int) board <= 4) { request = board; build = true; } break;
            case 1: build = true; if ((unsigned int) board <= 6) request = 5 + board; break;
            case 2: build = true; if ((unsigned int) board <= 6) request = 12 + board; break;
            case 3: build = true; if ((unsigned int) board <= 11) request = 19 + board; break;
            case 4: build = true; request = 1; break;
            default: break;
            }
            if (!build)
                continue;
            HostLeaderboard cache;
            if (request != -1)
                (void) hostGetLeaderboardPlayer(h, player, cache, request);                 // +0x1D0
            total += buildLeaderboardRows(cache, r);
        }
    }
    r.setTotalCount(total);
    r.setTotalCount(0);
    fn.result->set_as_object_interface(r.arr.get_ptr());
    busyClose(busy);
}

// "lctop" (100307EA): the four category tabs, rows 0..3 with no window test - the localised
// BFHQ_leaderboardscores, _leaderboardkits, _leaderboardvehicles and _leaderboardinfantryequipment -
// and a totalCount of 4.
static void listLeaderboardTop(const gameswf::fn_call& fn, ListResult& r, ListHost h)
{
    static const char* const kTabs[4] = {
        "BFHQ_leaderboardscores", "BFHQ_leaderboardkits", "BFHQ_leaderboardvehicles", "BFHQ_leaderboardinfantryequipment" };
    r.begin();
    for (int i = 0; i < 4; ++i)
    {
        r.beginRow(i);
        HostWide wide;
        hostLocalise(h, kTabs[i], wide);
        r.pushWide(wide.c_str(), (int) ::wcslen(wide.c_str()));
    }
    r.setTotalCount(4);
    fn.result->set_as_object_interface(r.arr.get_ptr());
}

} // anonymous namespace (the leaderboard lists)

// dice.bf2.Multiplay.setLeaderboardCategoryId(id), and
// promoted. The global player's host (unguarded), then arg(0).to_number() (bounds-checked accessor)
// truncated by the x87 exactly as in beginWaitState, then the host's +0x140(id) - the slot this tree
// calls setLeaderboardCategory (BF2.exe 0x487D00, one store to +0x774: the selected leaderboard category, named
// by this, its only AS writer). No result. The old `if (h)` and `nargs >= 1` were invented.
void as_multiplay_setLeaderboardCategoryId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_setLeaderboardCategoryId");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    h->setLeaderboardCategory(static_cast<int>(fn.arg(0).to_number()));            // +0x140
}

// Returns the first current-mod game mode's display name, or empty for no modes.
// Requires player/host/mod; preserve repeated mod-info and current-mod calls.
void as_multiplay_getSelectedFilterGameMode(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getSelectedFilterGameMode");
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    const IModInfo::Mod* mod = static_cast<IModInfo*>(h->getModInfo())->getCurrentMod();
    // The shipped test is `first != 0 && (last - first)/28 != 0`; on the typed vector that is empty().
    if (!mod->gameModes.empty())
    {
        IModInfo* second = static_cast<IModInfo*>(h->getModInfo());
        IModInfo* third  = static_cast<IModInfo*>(h->getModInfo());
        const IModInfo::Mod* again = second->getCurrentMod();
        const std::string name = third->getNameFromGameMode(again->gameModes[0]);   // +0x4C
        fn.result->set_tu_string(tu_string(name.c_str()));
    }
    else
    {
        fn.result->set_tu_string(tu_string(""));
    }
}

// Returns the localized Scores category as a dice String, regardless of selection.
// Reads no arguments and preserves both host fetches.
void as_multiplay_getSelectedLeaderboardCategoryId(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_multiplay_getSelectedLeaderboardCategoryId");
    (void) g_swiffPlayerInstance->getSwiffHost();
    const vc71::string key("BFHQ_leaderboardscores");
    vc71::wstring text;
    ISwiffHost* h = g_swiffPlayerInstance->getSwiffHost();
    std::wstring wide;
    (void) h->getLocalizedString(wide, std::string(key.c_str()));                 // +0xA8
    text = wide.c_str();
    // The String's wide side is assigned from the copy's c_str() (10052647..1005264E), so it ends
    // at the first NUL whatever length the host wrote.
    fn.result->set_as_object_interface(gameswf::create_dice_string_object(
        reinterpret_cast<const unsigned short*>(text.c_str()), (int) ::wcslen(text.c_str())));
}

// sub_1005D7A0 - dice.bf2.Profile.getRankProgress(): SwiffHost +716() as a double - a number.
void as_profile_getRankProgress(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getRankProgress");
    void* h = asHost();
    fn.result->set_double(h ? (double) static_cast<ISwiffHost*>(h)->getRankProgress() : 0.0);
}

// sub_10061110 - dice.bf2.Profile.getNextRankString(): the stats record's rank id (+160) through
// SwiffHost +444 (the next promotion rank) and +436 (its locale key), localised into a dice String
// object.
void as_profile_getNextRankString(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_profile_getNextRankString");
    void* h = asHost();
    if (!h) { fn.result->set_as_object_interface(gameswf::create_dice_string_object(0, 0)); return; }
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    (void) fetchPlayerStats(h, *rec);
    const int rankId = *reinterpret_cast<const int*>(rec->bytes + 160);
    delete rec;
    const int next = hostNextRank(h, rankId);
    vc71::string key;
    hostRankKey(h, next, key);
    vc71::wstring text;
    hostLocaliseKey(h, key, text);
    fn.result->set_as_object_interface(gameswf::create_dice_string_object(reinterpret_cast<const unsigned short*>(text.c_str()), (int) text.size()));
}

// sub_1005BA20 - dice.bf2.PlayerStats.getKitAccuracy(): the stats record's int at +2220 as a number
// once the record is fetched, else 0.
void as_playerstats_getKitAccuracy(const gameswf::fn_call& fn)
{
    SWF_AS_TRACE("as_playerstats_getKitAccuracy");
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    const bool ok = fetchPlayerStats(asHost(), *rec);
    const double v = ok ? (double) *reinterpret_cast<const int*>(rec->bytes + 2220) : 0.0;
    delete rec;
    fn.result->set_double(v);
}

// sub_1005AC60 / sub_1005ADB0 - dice.bf2.PlayerStats.getOpponentName() / getVictimName(): the stats
// record's std::string at +492 / +456; empty or a single space becomes "-", and so does a record that
// could not be fetched. string.
static void statsNameAt(const gameswf::fn_call& fn, int offset)
{
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    vc71::string out("-");
    if (fetchPlayerStats(asHost(), *rec))
    {
        const vc71::string* name = reinterpret_cast<const vc71::string*>(rec->bytes + offset);
        if (name->size() != 0 && ::strcmp(name->c_str(), " ") != 0) out = *name;
    }
    delete rec;
    fn.result->set_tu_string(tu_string(out.c_str()));
}
void as_playerstats_getOpponentName(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_playerstats_getOpponentName"); statsNameAt(fn, 492); }
void as_playerstats_getVictimName(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_playerstats_getVictimName");   statsNameAt(fn, 456); }

// sub_1005AF00 / sub_1005AFF0 - getOpponentRankId() / getVictimRankId(): the int beside the name
// (+524 / +488) as a number; a single-space name or an unfetched record answers -1.
static void statsRankIdAt(const gameswf::fn_call& fn, int nameOffset, int idOffset)
{
    PlayerStatsRecord* rec = new PlayerStatsRecord();
    double v = -1.0;
    if (fetchPlayerStats(asHost(), *rec))
    {
        const vc71::string* name = reinterpret_cast<const vc71::string*>(rec->bytes + nameOffset);
        if (::strcmp(name->c_str(), " ") != 0) v = (double) *reinterpret_cast<const int*>(rec->bytes + idOffset);
    }
    delete rec;
    fn.result->set_double(v);
}
void as_playerstats_getOpponentRankId(const gameswf::fn_call& fn) { SWF_AS_TRACE("as_playerstats_getOpponentRankId"); statsRankIdAt(fn, 492, 524); }
void as_playerstats_getVictimRankId(const gameswf::fn_call& fn)   { SWF_AS_TRACE("as_playerstats_getVictimRankId");   statsRankIdAt(fn, 456, 488); }

static void registerAsMethods(SwiffAsNamespace* general, SwiffAsNamespace** ns,
                              const char* const* names, int count)
{
    if (general)
    {
        general->registerNativeCompatibleMethod<as_general_getHostVersion>("getHostVersion");
        general->registerNativeCompatibleMethod<as_general_flushList>("flushList"); // discarded host fetch
        general->registerNativeCompatibleMethod<as_general_getListRevision>("getListRevision");
        general->registerNativeCompatibleMethod<as_general_getListEntries>("getListEntries");
        general->registerNativeCompatibleMethod<as_general_setFilter>("setFilter");
        general->registerNativeCompatibleMethod<as_general_setSortOrder>("setSortOrder");
        general->registerNativeCompatibleMethod<as_general_cacheExtendedServerInfo>("cacheExtendedServerInfo");
        general->registerNativeCompatibleMethod<as_general_setEquipmentIndex>("setEquipmentIndex");
    }

    for (int i = 0; i < count; ++i)
    {
        const char* n = names[i];
        if (::strcmp(n, "Cursor") == 0)
        {
            // The shipped registrar's order (1000FA90): getCursor, setCursor, enableCursor.
            ns[i]->registerNativeCompatibleMethod<as_cursor_getCursor>("getCursor");
            ns[i]->registerNativeCompatibleMethod<as_cursor_setCursor>("setCursor");
            ns[i]->registerNativeCompatibleMethod<as_cursor_enableCursor>("enableCursor");
        }
        else if (::strcmp(n, "PlayerStats") == 0)
        {
            // Statistics bindings use typed native host interfaces.
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getRankString>("getRankString");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getRankNotification>("getRankNotification");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getNumUnlockableItems>("getNumUnlockableItems");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getFavouriteName>("getFavouriteName");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getFavouriteId>("getFavouriteId");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getFavouriteTime>("getFavouriteTime");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_selectFirstUnlock>("selectFirstUnlock");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getCurrentUnlockDescription>("getCurrentUnlockDescription");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getKitAccuracy>("getKitAccuracy");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getOpponentName>("getOpponentName");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getVictimName>("getVictimName");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getOpponentRankId>("getOpponentRankId");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getVictimRankId>("getVictimRankId");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getPromotedRank>("getPromotedRank");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getDemotedRank>("getDemotedRank");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getNextPromotionRank>("getNextPromotionRank");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getNextDemomotionRank>("getNextDemomotionRank");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_selectUnlock>("selectUnlock");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_unlock>("unlock");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_clearRankNotification>("clearRankNotification");
            // The leaderboard rows: the host's leaderboard cache, +0x1CC / +0x1D0 / +0x1D4.
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getLeaderBoard>("getLeaderBoard");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_refreshLeaderBoard>("refreshLeaderBoard");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getNumPlayersInLeaderboard>("getNumPlayersInLeaderboard");
            ns[i]->registerNativeCompatibleMethod<as_playerstats_getLeaderboardDate>("getLeaderboardDate");
        }
        else if (::strcmp(n, "GeneralSettings") == 0)
        {
            // All sixty rows of the shipped registrar 10033B50, in its order.
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_apply>("apply");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_reset>("reset");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_cancel>("cancel");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setSortKey>("setSortKey");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getSortKey>("getSortKey");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setSortOrder>("setSortOrder");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getSortOrder>("getSortOrder");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setNumRoundsPlayed>("setNumRoundsPlayed");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getNumRoundsPlayed>("getNumRoundsPlayed");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setServerFilter>("setServerFilter");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getServerFilter>("getServerFilter");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setHUDTransparency>("setHUDTransparency");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getHUDTransparency>("getHUDTransparency");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setCrosshairColor>("setCrosshairColor");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getCrosshairColor>("getCrosshairColor");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setBuddytagColor>("setBuddytagColor");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getBuddytagColor>("getBuddytagColor");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setSquadtagColor>("setSquadtagColor");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getSquadtagColor>("getSquadtagColor");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setMinimapRotate>("setMinimapRotate");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getMinimapRotate>("getMinimapRotate");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setMinimapTransparency>("setMinimapTransparency");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getMinimapTransparency>("getMinimapTransparency");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setViewIntroMovie>("setViewIntroMovie");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getViewIntroMovie>("getViewIntroMovie");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setOutOfVoting>("setOutOfVoting");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getOutOfVoting>("getOutOfVoting");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setBFTVSaveDirectory>("setBFTVSaveDirectory");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getBFTVSaveDirectory>("getBFTVSaveDirectory");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setPlayedVOHelp>("setPlayedVOHelp");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_checkPlayedVOHelp>("checkPlayedVOHelp");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setConfirmQuit>("setConfirmQuit");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getConfirmQuit>("getConfirmQuit");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setMapIconTransparency>("setMapIconTransparency");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getMapIconTransparency>("getMapIconTransparency");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setUseBots>("setUseBots");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getUseBots>("getUseBots");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setMaxBots>("setMaxBots");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getMaxBots>("getMaxBots");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setMaxBotsIncludeHumans>("setMaxBotsIncludeHumans");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getMaxBotsIncludeHumans>("getMaxBotsIncludeHumans");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setBotSkill>("setBotSkill");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getBotSkill>("getBotSkill");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setCoopBotRatio>("setCoopBotRatio");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getCoopBotRatio>("getCoopBotRatio");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setCoopBotCount>("setCoopBotCount");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getCoopBotCount>("getCoopBotCount");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getLastAwardsCheckDate>("getLastAwardsCheckDate");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_updateLastAwardsCheckDate>("updateLastAwardsCheckDate");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getAllowPunkBuster>("getAllowPunkBuster");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setAllowPunkBuster>("setAllowPunkBuster");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getItemSelectionReverseItems>("getItemSelectionReverseItems");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setItemSelectionReverseItems>("setItemSelectionReverseItems");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setAutoReload>("setAutoReload");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getAutoReload>("getAutoReload");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setAutoReady>("setAutoReady");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getAutoReady>("getAutoReady");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_setConnectionType>("setConnectionType");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_getConnectionType>("getConnectionType");
            ns[i]->registerNativeCompatibleMethod<as_generalsettings_removeServer>("removeServer");
        }
        else if (::strcmp(n, "AudioSettings") == 0)
        {
            // All forty-three rows of the shipped registrar 1000BC00, in its order.
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoipEnabled>("getVoipEnabled");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoipEnabled>("setVoipEnabled");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoipPlaybackVolume>("getVoipPlaybackVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoipPlaybackVolume>("setVoipPlaybackVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoipCaptureVolume>("getVoipCaptureVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoipCaptureVolume>("setVoipCaptureVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoipCaptureThreshold>("getVoipCaptureThreshold");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoipCaptureThreshold>("setVoipCaptureThreshold");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoipBoostEnabled>("getVoipBoostEnabled");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoipBoostEnabled>("setVoipBoostEnabled");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoipUsePushToTalk>("getVoipUsePushToTalk");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoipUsePushToTalk>("setVoipUsePushToTalk");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getProvider>("getProvider");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setProvider>("setProvider");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSoundQuality>("getSoundQuality");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setSoundQuality>("setSoundQuality");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getEffectsVolume>("getEffectsVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setEffectsVolume>("setEffectsVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getMusicVolume>("getMusicVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setMusicVolume>("setMusicVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getVoiceOverVolume>("getVoiceOverVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setVoiceOverVolume>("setVoiceOverVolume");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getNumberOfVoices>("getNumberOfVoices");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setNumberOfVoices>("setNumberOfVoices");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSoundDetail>("getSoundDetail");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setSoundDetail>("setSoundDetail");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_apply>("apply");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_canApply>("canApply");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_cancel>("cancel");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_hasEax>("hasEax");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_hasZenithBoard>("hasZenithBoard");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_reset>("reset");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_needsRestart>("needsRestart");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSelectedNumVoices>("getSelectedNumVoices");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSelectedProvider>("getSelectedProvider");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSelectedSoundDetail>("getSelectedSoundDetail");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSelectedSoundQuality>("getSelectedSoundQuality");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getSelectedLeaderboardCategoryId>("getSelectedLeaderboardCategoryId");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_isInCriticalState>("isInCriticalState");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getEnglishOnlyVoices>("getEnglishOnlyVoices");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setEnglishOnlyVoices>("setEnglishOnlyVoices");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_getEnableEAX>("getEnableEAX");
            ns[i]->registerNativeCompatibleMethod<as_audiosettings_setEnableEAX>("setEnableEAX");
        }
        else if (::strcmp(n, "ControlSettings") == 0)
        {
            // Twenty-one of the shipped registrar 1000F090's twenty-two rows, in its order.
            // mapInput (1000EDA0) is withheld on purpose: see the section comment above its siblings.
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setActiveControlMap>("setActiveControlMap");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getMappedString>("getMappedString");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_cancel>("cancel");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_apply>("apply");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_reset>("reset");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_resetDefault>("resetDefault");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getMouseSensitivity>("getMouseSensitivity");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setMouseSensitivity>("setMouseSensitivity");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getMouseSensitivityMultiplier>("getMouseSensitivityMultiplier");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setMouseSensitivityMultiplier>("setMouseSensitivityMultiplier");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getMouseSmoothing>("getMouseSmoothing");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setMouseSmoothing>("setMouseSmoothing");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getMouseYAWFactor>("getMouseYAWFactor");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setMouseYAWFactor>("setMouseYAWFactor");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getMousePITCHFactor>("getMousePITCHFactor");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setMousePITCHFactor>("setMousePITCHFactor");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getInvertMouse>("getInvertMouse");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setInvertMouse>("setInvertMouse");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getKeyboardSensitivity>("getKeyboardSensitivity");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_setKeyboardSensitivity>("setKeyboardSensitivity");
            ns[i]->registerNativeCompatibleMethod<as_controlsettings_getActiveControlMap>("getActiveControlMap");
        }
        else if (::strcmp(n, "ServerInfo") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getWelcomeMessage>("getWelcomeMessage");
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getCommunityLogo>("getCommunityLogo");
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getSponsorLogo>("getSponsorLogo");
            ns[i]->registerNativeCompatibleMethod<as_serverinfo_getServerName>("getServerName");
        }
        else if (::strcmp(n, "ServerSettings") == 0)
        {
            // All 98 registrations of ctor sub_10066760, in its order: 94 names,
            // four of them registered twice as shipped.
            ns[i]->registerNativeCompatibleMethod<as_serversettings_apply>("apply");                 // AS_ServerSettings_apply 0x10063640
            ns[i]->registerNativeCompatibleMethod<as_serversettings_reset>("reset");                 // AS_ServerSettings_reset 0x100636B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getServerName>("getServerName"); // AS_ServerSettings_getServerName 0x100650C0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setServerName>("setServerName"); // AS_ServerSettings_setServerName 0x10063BC0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getPassword>("getPassword");     // AS_ServerSettings_getPassword 0x10065160
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setPassword>("setPassword");     // AS_ServerSettings_setPassword 0x10063C30
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getInternet>("getInternet");     // AS_ServerSettings_getInternet 0x10065300
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setInternet>("setInternet");     // AS_ServerSettings_setInternet 0x10063DB0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getBandwidthChoke>("getBandwidthChoke"); // AS_ServerSettings_getBandwidthChoke 0x10065200
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setBandwidthChoke>("setBandwidthChoke"); // AS_ServerSettings_setBandwidthChoke 0x10063CA0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getMaxPlayers>("getMaxPlayers"); // AS_ServerSettings_getMaxPlayers 0x10065280
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setMaxPlayers>("setMaxPlayers"); // AS_ServerSettings_setMaxPlayers 0x10063D20
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getInternet>("getInternet");     // AS_ServerSettings_getInternet registered a second time, as shipped
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setInternet>("setInternet");     // AS_ServerSettings_setInternet registered a second time, as shipped
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getGameMode>("getGameMode");     // AS_ServerSettings_getGameMode 0x10065390
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setGameMode>("setGameMode");     // AS_ServerSettings_setGameMode 0x10063E10
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getStartDelay>("getStartDelay"); // AS_ServerSettings_getStartDelay 0x10065430
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setStartDelay>("setStartDelay"); // AS_ServerSettings_setStartDelay 0x10063E80
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getEndDelay>("getEndDelay");     // AS_ServerSettings_getEndDelay 0x100654B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setEndDelay>("setEndDelay");     // AS_ServerSettings_setEndDelay 0x10063F00
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getSpawnTime>("getSpawnTime");   // AS_ServerSettings_getSpawnTime 0x10065530
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setSpawnTime>("setSpawnTime");   // AS_ServerSettings_setSpawnTime 0x10063F80
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getManDownTime>("getManDownTime"); // AS_ServerSettings_getManDownTime 0x100655B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setManDownTime>("setManDownTime"); // AS_ServerSettings_setManDownTime 0x10063FE0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getTicketRatio>("getTicketRatio"); // AS_ServerSettings_getTicketRatio 0x10065630
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setTicketRatio>("setTicketRatio"); // AS_ServerSettings_setTicketRatio 0x10064040
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getCoopBotRatio>("getCoopBotRatio"); // AS_ServerSettings_getCoopBotRatio 0x10064F30
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setCoopBotRatio>("setCoopBotRatio"); // AS_ServerSettings_setCoopBotRatio 0x10063920
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getCoopBotCount>("getCoopBotCount"); // AS_ServerSettings_getCoopBotCount 0x10064FB0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setCoopBotCount>("setCoopBotCount"); // AS_ServerSettings_setCoopBotCount 0x100639D0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getCoopBotDiff>("getCoopBotDiff"); // AS_ServerSettings_getCoopBotDiff 0x10065040
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setCoopBotDiff>("setCoopBotDiff"); // AS_ServerSettings_setCoopBotDiff 0x10063AA0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getRoundsPerMap>("getRoundsPerMap"); // AS_ServerSettings_getRoundsPerMap 0x10065740
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setRoundsPerMap>("setRoundsPerMap"); // AS_ServerSettings_setRoundsPerMap 0x10064130
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getTimeLimit>("getTimeLimit");   // AS_ServerSettings_getTimeLimit 0x100657C0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setTimeLimit>("setTimeLimit");   // AS_ServerSettings_setTimeLimit 0x100641B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getScoreLimit>("getScoreLimit"); // AS_ServerSettings_getScoreLimit 0x10065850
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setScoreLimit>("setScoreLimit"); // AS_ServerSettings_setScoreLimit 0x10064230
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getSoldierFF>("getSoldierFF");   // AS_ServerSettings_getSoldierFF 0x100658E0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setSoldierFF>("setSoldierFF");   // AS_ServerSettings_setSoldierFF 0x100642B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVehicleFF>("getVehicleFF");   // AS_ServerSettings_getVehicleFF 0x10065970
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVehicleFF>("setVehicleFF");   // AS_ServerSettings_setVehicleFF 0x10064330
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getSoldierSplashFF>("getSoldierSplashFF"); // AS_ServerSettings_getSoldierSplashFF 0x10065A00
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setSoldierSplashFF>("setSoldierSplashFF"); // AS_ServerSettings_setSoldierSplashFF 0x100643B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVehicleSplashFF>("getVehicleSplashFF"); // AS_ServerSettings_getVehicleSplashFF 0x10065A90
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVehicleSplashFF>("setVehicleSplashFF"); // AS_ServerSettings_setVehicleSplashFF 0x10064430
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getPunishTeamKills>("getPunishTeamKills"); // AS_ServerSettings_getPunishTeamKills 0x10065B20
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getPunishTeamKills>("getPunishTeamKills"); // AS_ServerSettings_getPunishTeamKills registered a second time, as shipped
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoteTime>("getVoteTime");     // AS_ServerSettings_getVoteTime 0x10065BB0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoteTime>("getVoteTime");     // AS_ServerSettings_getVoteTime registered a second time, as shipped
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getMinPlayersForVoting>("getMinPlayersForVoting"); // AS_ServerSettings_getMinPlayersForVoting 0x10065C40
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setMinPlayersForVoting>("setMinPlayersForVoting"); // AS_ServerSettings_setMinPlayersForVoting 0x100644B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoipEnabled>("getVoipEnabled"); // AS_ServerSettings_getVoipEnabled 0x10065CD0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVoipEnabled>("setVoipEnabled"); // AS_ServerSettings_setVoipEnabled 0x10064530
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoipQuality>("getVoipQuality"); // AS_ServerSettings_getVoipQuality 0x10065D60
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVoipQuality>("setVoipQuality"); // AS_ServerSettings_setVoipQuality 0x10064590
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoipServerRemote>("getVoipServerRemote"); // AS_ServerSettings_getVoipServerRemote 0x10065DF0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVoipServerRemote>("setVoipServerRemote"); // AS_ServerSettings_setVoipServerRemote 0x10064610
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoipServerRemoteIP>("getVoipServerRemoteIP"); // AS_ServerSettings_getVoipServerRemoteIP 0x10065E80
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVoipServerRemoteIP>("setVoipServerRemoteIP"); // AS_ServerSettings_setVoipServerRemoteIP 0x10064670
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoipServerPort>("getVoipServerPort"); // AS_ServerSettings_getVoipServerPort 0x10065F20
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVoipServerPort>("setVoipServerPort"); // AS_ServerSettings_setVoipServerPort 0x100646E0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getVoipBFClientPort>("getVoipBFClientPort"); // AS_ServerSettings_getVoipBFClientPort 0x10065FB0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setVoipBFClientPort>("setVoipBFClientPort"); // AS_ServerSettings_setVoipBFClientPort 0x10064760
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getGameSpyPort>("getGameSpyPort"); // AS_ServerSettings_getGameSpyPort 0x10066040
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setGameSpyPort>("setGameSpyPort"); // AS_ServerSettings_setGameSpyPort 0x100647E0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getAllowNATNegotation>("getAllowNATNegotation"); // AS_ServerSettings_getAllowNATNegotation 0x100660D0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setAllowNATNegotation>("setAllowNATNegotation"); // AS_ServerSettings_setAllowNATNegotation 0x10064860
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getInterfaceIP>("getInterfaceIP"); // AS_ServerSettings_getInterfaceIP 0x10066160
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setInterfaceIP>("setInterfaceIP"); // AS_ServerSettings_setInterfaceIP 0x100648C0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getAutoRecord>("getAutoRecord"); // AS_ServerSettings_getAutoRecord 0x10066200
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setAutoRecord>("setAutoRecord"); // AS_ServerSettings_setAutoRecord 0x10064930
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getDemoIndexURL>("getDemoIndexURL"); // AS_ServerSettings_getDemoIndexURL 0x10066290
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setDemoIndexURL>("setDemoIndexURL"); // AS_ServerSettings_setDemoIndexURL 0x10064990
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getDemoDownloadURL>("getDemoDownloadURL"); // AS_ServerSettings_getDemoDownloadURL 0x10066330
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setDemoDownloadURL>("setDemoDownloadURL"); // AS_ServerSettings_setDemoDownloadURL 0x10064A00
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getDemoHook>("getDemoHook");     // AS_ServerSettings_getDemoHook 0x100663D0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setDemoHook>("setDemoHook");     // AS_ServerSettings_setDemoHook 0x10064A70
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getAdminScript>("getAdminScript"); // AS_ServerSettings_getAdminScript 0x10066470
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setAdminScript>("setAdminScript"); // AS_ServerSettings_setAdminScript 0x10064AE0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getTimeBeforeRestartMap>("getTimeBeforeRestartMap"); // AS_ServerSettings_getTimeBeforeRestartMap 0x10066510
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setTimeBeforeRestartMap>("setTimeBeforeRestartMap"); // AS_ServerSettings_setTimeBeforeRestartMap 0x10064B50
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getSponsorText>("getSponsorText"); // AS_ServerSettings_getSponsorText 0x10066590
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setSponsorText>("setSponsorText"); // AS_ServerSettings_setSponsorText 0x10064BC0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getSponsorLogoURL>("getSponsorLogoURL"); // AS_ServerSettings_getSponsorLogoURL 0x10066630
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setSponsorLogoURL>("setSponsorLogoURL"); // AS_ServerSettings_setSponsorLogoURL 0x10064C30
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getSvPunkBuster>("getSvPunkBuster"); // AS_ServerSettings_getSvPunkBuster 0x100666D0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setSvPunkBuster>("setSvPunkBuster"); // AS_ServerSettings_setSvPunkBuster 0x10064CA0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getAutoBalanceTeam>("getAutoBalanceTeam"); // AS_ServerSettings_getAutoBalanceTeam 0x10064D90
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setAutoBalanceTeam>("setAutoBalanceTeam"); // AS_ServerSettings_setAutoBalanceTeam 0x10063830
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getAutoRecordRounds>("getAutoRecordRounds"); // AS_ServerSettings_getAutoRecordRounds 0x10064E20
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setAutoRecordRounds>("setAutoRecordRounds"); // AS_ServerSettings_setAutoRecordRounds 0x100638A0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getTeamRatio>("getTeamRatio");   // AS_ServerSettings_getTeamRatio 0x10064EB0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setTeamRatio>("setTeamRatio");   // AS_ServerSettings_setTeamRatio 0x10063B50
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getFriendlyFire>("getFriendlyFire"); // AS_ServerSettings_getFriendlyFire 0x10064D00
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setFriendlyFire>("setFriendlyFire"); // AS_ServerSettings_setFriendlyFire 0x100636F0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_getNoVehicles>("getNoVehicles"); // AS_ServerSettings_getNoVehicles 0x100656B0
            ns[i]->registerNativeCompatibleMethod<as_serversettings_setNoVehicles>("setNoVehicles"); // AS_ServerSettings_setNoVehicles 0x100640C0
        }
        else if (::strcmp(n, "Sound") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_sound_playSound>("playSound");
            ns[i]->registerNativeCompatibleMethod<as_sound_voipStopTest>("voipStopTest");
        }
        else if (::strcmp(n, "VideoSettings") == 0)
        {
            // All 47 rows of ctor sub_1006CA60, in its order. Every body's host boundary
            // is typed on the native side: IVideoSettings, ILocalProfileManager::saveCurrentProfile,
            // the GeneralSettings LCD bridge and ISwiffHost::getLocalizedString.
            ns[i]->registerNativeCompatibleMethod<as_videosettings_apply>("apply");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_cancel>("cancel");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_canApply>("canApply");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_reset>("reset");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getTerrainQuality>("getTerrainQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setTerrainQuality>("setTerrainQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getGeometryQuality>("getGeometryQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setGeometryQuality>("setGeometryQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getLightingQuality>("getLightingQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setLightingQuality>("setLightingQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getDynamicLightingQuality>("getDynamicLightingQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setDynamicLightingQuality>("setDynamicLightingQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getDynamicShadowsQuality>("getDynamicShadowsQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setDynamicShadowsQuality>("setDynamicShadowsQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getEffectsQuality>("getEffectsQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setEffectsQuality>("setEffectsQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getTextureQuality>("getTextureQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setTextureQuality>("setTextureQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getResolution>("getResolution");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setResolution>("setResolution");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getAntialiasing>("getAntialiasing");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setAntialiasing>("setAntialiasing");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setViewDistanceScale>("setViewDistanceScale");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getViewDistanceScale>("getViewDistanceScale");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setVideoOptionScheme>("setVideoOptionScheme");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getVideoOptionScheme>("getVideoOptionScheme");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_detectVideoScheme>("detectVideoScheme");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getTextureFilteringQuality>("getTextureFilteringQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setTextureFilteringQuality>("setTextureFilteringQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedResolution>("getSelectedResolution");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedOverallQuality>("getSelectedOverallQuality");   // sub_1006AAB0, `retn 0Ch`
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedTerrainQuality>("getSelectedTerrainQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedEffectsQuality>("getSelectedEffectsQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedGeometryQuality>("getSelectedGeometryQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedTexturequality>("getSelectedTexturequality");   // sub_1006C3F0, shipped spelling
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedLightingQuality>("getSelectedLightingQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedDynamicShadowsQuality>("getSelectedDynamicShadowsQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedDynamicLightingQuality>("getSelectedDynamicLightingQuality");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedVideoOptionScheme>("getSelectedVideoOptionScheme");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedAntialias>("getSelectedAntialias");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getSelectedTextureFiltering>("getSelectedTextureFiltering");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_needsRestart>("needsRestart");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_needsReboot>("needsReboot");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_isRunningWithUnsupportedDrivers>("isRunningWithUnsupportedDrivers");
            ns[i]->registerNativeCompatibleMethod<as_videosettings_isOptimzingShaders>("isOptimzingShaders");                         // sub_1006BDD0, shipped spelling
            ns[i]->registerNativeCompatibleMethod<as_videosettings_getLCDDisplayModes>("getLCDDisplayModes");                 // sub_1006BCD0 ("GeneralSettings" +0x15C)
            ns[i]->registerNativeCompatibleMethod<as_videosettings_setLCDDisplayModes>("setLCDDisplayModes");                 // sub_1006BC60 ("GeneralSettings" +0x160)
        }
        else if (::strcmp(n, "Client") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_client_getCommandlineOption>("getCommandlineOption");
            ns[i]->registerNativeCompatibleMethod<as_client_setCommandlineOption>("setCommandlineOption");
            ns[i]->registerNativeCompatibleMethod<as_client_getPlayerName>("getPlayerName");
            ns[i]->registerNativeCompatibleMethod<as_client_setPlayerName>("setPlayerName");
            ns[i]->registerNativeCompatibleMethod<as_client_hasUserEscapedFromGame>("hasUserEscapedFromGame");
            ns[i]->registerNativeCompatibleMethod<as_client_getLoadProgress>("getLoadProgress");
            ns[i]->registerNativeCompatibleMethod<as_client_getEORType>("getEORType");
            ns[i]->registerNativeCompatibleMethod<as_client_getTimeUntilRestart>("getTimeUntilRestart");
            ns[i]->registerNativeCompatibleMethod<as_client_getTotalTimeUntilRestart>("getTotalTimeUntilRestart");
            ns[i]->registerNativeCompatibleMethod<as_client_getCurrentLoadFile>("getCurrentLoadFile");
        }
        else if (::strcmp(n, "Profile") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_profile_getLoginStatus>("getLoginStatus");
            ns[i]->registerNativeCompatibleMethod<as_profile_getNumLocalProfiles>("getNumLocalProfiles");
            ns[i]->registerNativeCompatibleMethod<as_profile_getNamePrefix>("getNamePrefix");
            ns[i]->registerNativeCompatibleMethod<as_profile_logout>("logout");
            ns[i]->registerNativeCompatibleMethod<as_profile_loginOnlineAccount>("loginOnlineAccount");
            ns[i]->registerNativeCompatibleMethod<as_profile_loginOnlineAccountWithEmail>("loginOnlineAccountWithEmail");
            ns[i]->registerNativeCompatibleMethod<as_profile_loginOnlineAccountWithNick>("loginOnlineAccountWithNick");
            ns[i]->registerNativeCompatibleMethod<as_profile_createOfflineAccount>("createOfflineAccount");
            ns[i]->registerNativeCompatibleMethod<as_profile_createOnlineAccount>("createOnlineAccount");
            ns[i]->registerNativeCompatibleMethod<as_profile_loginOfflineAccount>("loginOfflineAccount");
            ns[i]->registerNativeCompatibleMethod<as_profile_getFirstCountry>("getFirstCountry");
            ns[i]->registerNativeCompatibleMethod<as_profile_getCurrentRankId>("getCurrentRankId");
            ns[i]->registerNativeCompatibleMethod<as_profile_getActivePlayer>("getActivePlayer");
            ns[i]->registerNativeCompatibleMethod<as_profile_getStatPlayerName>("getStatPlayerName");
            ns[i]->registerNativeCompatibleMethod<as_profile_setStatPlayer>("setStatPlayer");
            ns[i]->registerNativeCompatibleMethod<as_profile_setActivePlayer>("setActivePlayer");
            ns[i]->registerNativeCompatibleMethod<as_profile_getNumTimesLoggedIn>("getNumTimesLoggedIn");
            ns[i]->registerNativeCompatibleMethod<as_profile_getNumRanks>("getNumRanks");
            ns[i]->registerNativeCompatibleMethod<as_profile_isMyProfile>("isMyProfile");
            ns[i]->registerNativeCompatibleMethod<as_profile_getEquipmentName>("getEquipmentName");
            ns[i]->registerNativeCompatibleMethod<as_profile_getEquipmentDescription>("getEquipmentDescription");
            ns[i]->registerNativeCompatibleMethod<as_profile_setSelectedEquipment>("setSelectedEquipment");
            ns[i]->registerNativeCompatibleMethod<as_profile_setSelectedUnlockEquipment>("setSelectedUnlockEquipment");
            ns[i]->registerNativeCompatibleMethod<as_profile_getCurrentRankString>("getCurrentRankString");
            ns[i]->registerNativeCompatibleMethod<as_profile_getRankProgress>("getRankProgress");
            ns[i]->registerNativeCompatibleMethod<as_profile_getNextRankString>("getNextRankString");
            // The award rows: the host's awards cache (+0x228) and PlayerAwards +0x20/+0x24/+0x2C.
            ns[i]->registerNativeCompatibleMethod<as_profile_selectAward>("selectAward");
            ns[i]->registerNativeCompatibleMethod<as_profile_selectLatestAward>("selectLatestAward");
            ns[i]->registerNativeCompatibleMethod<as_profile_getAwardName>("getAwardName");
            ns[i]->registerNativeCompatibleMethod<as_profile_getAwardLevel>("getAwardLevel");
            ns[i]->registerNativeCompatibleMethod<as_profile_getAwardLatestDate>("getAwardLatestDate");
            ns[i]->registerNativeCompatibleMethod<as_profile_getAwardFirstDate>("getAwardFirstDate");
            ns[i]->registerNativeCompatibleMethod<as_profile_getAwardDescription>("getAwardDescription");
            ns[i]->registerNativeCompatibleMethod<as_profile_hasAward>("hasAward");
            ns[i]->registerNativeCompatibleMethod<as_profile_hasThisAward>("hasThisAward");
        }
        else if (::strcmp(n, "Multiplay") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_multiplay_isGameSpyAvailable>("isGameSpyAvailable");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_stopBrowser>("stopBrowser");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_browse>("browse");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getBrowser>("getBrowser");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setBrowser>("setBrowser");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setFilterGameMode>("setFilterGameMode");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getFilterGameMode>("getFilterGameMode");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_clearMapList>("clearMapList");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_addMapToMapList>("addMapToMapList");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getMapNameFromId>("getMapNameFromId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getMapDescriptionFromId>("getMapDescriptionFromId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getGameModeFromId>("getGameModeFromId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_startBrowse>("startBrowse");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getNumServers>("getNumServers");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_isBrowsing>("isBrowsing");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getToggleFilter>("getToggleFilter");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setToggleFilter>("setToggleFilter");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_updateFilter>("updateFilter");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setLeaderboardCategoryId>("setLeaderboardCategoryId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getLeaderboardCategoryId>("getLeaderboardCategoryId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setLeaderboardId>("setLeaderboardId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getLeaderboardId>("getLeaderboardId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getLeaderboardDescription>("getLeaderboardDescription");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getLeaderboardName>("getLeaderboardName");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_goFirstLeaderboard>("goFirstLeaderboard");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_goLastLeaderboard>("goLastLeaderboard");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_goNextLeaderboard>("goNextLeaderboard");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_goPreviousLeaderboard>("goPreviousLeaderboard");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_goLeaderboardPlayer>("goLeaderboardPlayer");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getSelectedFilterGameMode>("getSelectedFilterGameMode");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getSelectedLeaderboardCategoryId>("getSelectedLeaderboardCategoryId");
            // connect TO IP: the popup's two fields and its connect button.
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getConnectToIpPort>("getConnectToIpPort");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getConnectToIpHost>("getConnectToIpHost");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setConnectToIpPort>("setConnectToIpPort");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setConnectToIpHost>("setConnectToIpHost");
            // The join page: the selected row, its sponsor, its teams, the button.
            ns[i]->registerNativeCompatibleMethod<as_multiplay_joinServer>("joinServer");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setCurrentServerId>("setCurrentServerId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_clearServerId>("clearServerId");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getSponsorText>("getSponsorText");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getSponsorLogoUrl>("getSponsorLogoUrl");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_downloadServerLogo>("downloadServerLogo");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getTeamName>("getTeamName");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_isServerOnline>("isServerOnline");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setJoinPassword>("setJoinPassword");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getJoinPassword>("getJoinPassword");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getHasNewBookmark>("getHasNewBookmark");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setHasNewBookmark>("setHasNewBookmark");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_playDemo>("playDemo");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_deleteDemo>("deleteDemo");
            // Hosting a server: the create-server page's rows.
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setCurrentMap>("setCurrentMap");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_setMapList>("setMapList");                           // nullsub_4 0x10050E80
            ns[i]->registerNativeCompatibleMethod<as_multiplay_createServer>("createServer");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_removeMapfromMapList>("removeMapfromMapList");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_moveMapInList>("moveMapInList");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getCurrentMapName>("getCurrentMapName");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getCurrentMapGameMode>("getCurrentMapGameMode");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getCurrentMapMaxPlayers>("getCurrentMapMaxPlayers");
            ns[i]->registerNativeCompatibleMethod<as_multiplay_getCurrentMapSize>("getCurrentMapSize");
        }
        else if (::strcmp(n, "Logic") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_logic_disconnectGame>("disconnectGame");
            ns[i]->registerNativeCompatibleMethod<as_logic_isGameRunning>("isGameRunning");
            ns[i]->registerNativeCompatibleMethod<as_logic_getActivateReason>("getActivateReason");
            ns[i]->registerNativeCompatibleMethod<as_logic_getFrameLabel>("getFrameLabel");
            ns[i]->registerNativeCompatibleMethod<as_logic_getModVersion>("getModVersion");
            ns[i]->registerNativeCompatibleMethod<as_logic_setDecimals>("setDecimals"); // no host call
            ns[i]->registerNativeCompatibleMethod<as_logic_getCommandLine>("getCommandLine");
            ns[i]->registerNativeCompatibleMethod<as_logic_joinCommandlineServer>("joinCommandlineServer"); // the +joinServer launch
            ns[i]->registerNativeCompatibleMethod<as_logic_getStorageString>("getStorageString");
            ns[i]->registerNativeCompatibleMethod<as_logic_setStorageString>("setStorageString");
            ns[i]->registerNativeCompatibleMethod<as_logic_isCapsLockPressed>("isCapsLockPressed"); // GetKeyState only, no host calls
            ns[i]->registerNativeCompatibleMethod<as_logic_active>("active"); // no host calls
            ns[i]->registerNativeCompatibleMethod<as_logic_setAsync>("setAsync"); // player slot, no host
            ns[i]->registerNativeCompatibleMethod<as_logic_getAsync>("getAsync");
            ns[i]->registerNativeCompatibleMethod<as_logic_getTeamsConId>("getTeamsConId"); // sub_1004DA00, registered at 1004E283
            ns[i]->registerNativeCompatibleMethod<as_logic_beginWaitState>("beginWaitState");
            ns[i]->registerNativeCompatibleMethod<as_logic_endWaitState>("endWaitState");
            ns[i]->registerNativeCompatibleMethod<as_logic_setFrameLabel>("setFrameLabel");
            ns[i]->registerNativeCompatibleMethod<as_logic_enableScroller>("enableScroller");
            ns[i]->registerNativeCompatibleMethod<as_logic_getIsLoading>("getIsLoading");
            ns[i]->registerNativeCompatibleMethod<as_logic_setIsLoading>("setIsLoading");
            ns[i]->registerNativeCompatibleMethod<as_logic_readyToPlay>("readyToPlay");
            ns[i]->registerNativeCompatibleMethod<as_logic_isFirstTimeLoading>("isFirstTimeLoading");
            ns[i]->registerNativeCompatibleMethod<as_logic_isAbortLoadLevel>("isAbortLoadLevel");
            ns[i]->registerNativeCompatibleMethod<as_logic_getNumRoundsLeft>("getNumRoundsLeft");
            ns[i]->registerNativeCompatibleMethod<as_logic_isLoadingDemo>("isLoadingDemo");
            ns[i]->registerNativeCompatibleMethod<as_logic_startCommandlineServer>("startCommandlineServer");
        }
        else if (::strcmp(n, "EndOfRound") == 0)
        {
            // Register previous-round bindings in interface order.
            ns[i]->registerNativeCompatibleMethod<as_endofround_getTeamName>("getTeamName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getNumVictories>("getNumVictories");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getWinningTeam>("getWinningTeam");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getTicketHistorySum>("getTicketHistorySum");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMyTeam>("getMyTeam");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousTeamName>("getPreviousTeamName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getSquadTeamName>("getSquadTeamName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapPath>("getPreviousMapPath");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapName>("getPreviousMapName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapSize>("getPreviousMapSize");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapMaxPlayers>("getPreviousMapMaxPlayers");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapDescription>("getPreviousMapDescription");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapGameMode>("getPreviousMapGameMode");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapGameModeDescription>("getPreviousMapGameModeDescription");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getPreviousMapGameModeName>("getPreviousMapGameModeName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapPath>("getMapPath");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapName>("getMapName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapSize>("getMapSize");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapMaxPlayers>("getMapMaxPlayers");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapDescription>("getMapDescription");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapGameMode>("getMapGameMode");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapGameModeDescription>("getMapGameModeDescription");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getMapGameModeName>("getMapGameModeName");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getKitNameFromId>("getKitNameFromId");
            ns[i]->registerNativeCompatibleMethod<as_endofround_getTeamNameOfPlayerAtPosition>("getTeamNameOfPlayerAtPosition");
            ns[i]->registerNativeCompatibleMethod<as_endofround_hasMap>("hasMap");
        }
        else if (::strcmp(n, "Mod") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_mod_getCurrentModName>("getCurrentModName");
        }
        else if (::strcmp(n, "Locale") == 0)
        {
            ns[i]->registerNativeCompatibleMethod<as_locale_getString>("getString");
            ns[i]->registerNativeCompatibleMethod<as_locale_getLanguage>("getLanguage");
        }
    }
}

// The AS namespace graph. stage-5a: named namespace objects on stock gameswf's as_object (the
// get_global_object accessor forces action_init and hands back _global). Methods land by demand.
static SwiffAsNamespace* addNs(gameswf::as_object* parent, const char* name, const char* dottedPath)
{
    SwiffAsNamespace* ns = new SwiffAsNamespace(dottedPath);
    parent->set_member(tu_stringi(name),
                       gameswf::as_value(static_cast<gameswf::as_object_interface*>(ns)));
    return ns;
}
static void buildAsGraph()
{
    gameswf::as_object* g = gameswf::get_global_object();
    if (!g)
    {
        ::renddx9abi::logf("[SwiffPlayer] buildAsGraph: get_global_object() returned null");
        return;
    }
    SwiffAsNamespace* dice = addNs(g, "dice", "dice");
    SwiffAsNamespace* general = addNs(dice, "General", "dice.General");
    SwiffAsNamespace* messageHandler = addNs(dice, "MessageHandler", "dice.MessageHandler");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_getNumMessages>("getNumMessages");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_getLastType>("getLastType");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_getLastText>("getLastText");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_getLastCode>("getLastCode");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_removeLast>("removeLast");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_clear>("clear");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_enableMessageHandler>("enableMessageHandler");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_raiseMessage>("raiseMessage");
    messageHandler->registerNativeCompatibleMethod<as_messagehandler_getPunkBusterMessage>("getPunkBusterMessage");
    SwiffAsNamespace* bf2 = addNs(dice, "bf2", "dice.bf2");

    static const char* const kNamespaces[] =
    {
        "Locale", "Logic", "Render", "Singleplay", "Multiplay", "Clans", "Profile", "Options",
        "Client", "Sound", "Cursor", "Mod", "Player", "EndOfRound", "ControlSettings",
        "HapticSettings", "VideoSettings", "AudioSettings", "ServerSettings", "ServerInfo",
        "GeneralSettings", "PlayerStats", "FavouriteServers", "ServerHistory", "Shader"
    };
    const int nsCount = (int)(sizeof(kNamespaces) / sizeof(kNamespaces[0]));
    SwiffAsNamespace* built[32];
    for (int i = 0; i < nsCount; ++i)
    {
        char dotted[96];
        ::strcpy(dotted, "dice.bf2.");
        ::strncat(dotted, kNamespaces[i], sizeof(dotted) - 1 - ::strlen(dotted));
        built[i] = addNs(bf2, kNamespaces[i], dotted);
    }
    registerAsMethods(general, built, kNamespaces, nsCount);
    ::renddx9abi::logf("[SwiffPlayer] buildAsGraph: _global.dice + dice.bf2.<%d namespaces> built", nsCount);
}


// Adapts gameswf render callbacks to SwiffRender's fixed interface offsets.
// The 2006 handler differs from the 2004 interface, so mapping follows each operation's
// meaning. set_antialiased has no renderer counterpart and remains a no-op hint.

namespace {

inline void** rcVtbl(void* rc) { return *reinterpret_cast<void***>(rc); }

// bitmap creators (return an int texture handle)
inline int rcCreateEmpty(void* rc)                                            // +0x18 slot 6
{ return reinterpret_cast<int(__thiscall*)(void*)>(rcVtbl(rc)[0x18/4])(rc); }
inline int rcUploadAlpha(void* rc, unsigned w, unsigned h, const void* data)  // +0x1C slot 7
{ return reinterpret_cast<int(__thiscall*)(void*,unsigned,unsigned,const void*)>(rcVtbl(rc)[0x1C/4])(rc, w, h, data); }
inline int rcCreateRgb(void* rc, void* im)                                    // +0x20 slot 8
{ return reinterpret_cast<int(__thiscall*)(void*,void*)>(rcVtbl(rc)[0x20/4])(rc, im); }
inline int rcUploadRgba(void* rc, void* im)                                   // +0x24 slot 9
{ return reinterpret_cast<int(__thiscall*)(void*,void*)>(rcVtbl(rc)[0x24/4])(rc, im); }
inline int rcLoadTexture(void* rc, const char* name, int* w, int* h)          // +0x28 slot 10
{ return reinterpret_cast<int(__thiscall*)(void*,const char*,int*,int*)>(rcVtbl(rc)[0x28/4])(rc, name, w, h); }
inline void rcReleaseTexture(void* rc, int id)                                // +0x2C slot 11
{ reinterpret_cast<void(__thiscall*)(void*,int)>(rcVtbl(rc)[0x2C/4])(rc, id); }

// display bracket + transforms
inline void rcBeginDisplay(void* rc, int unused, int vx0, int vy0, int vw, int vh,
                           float x0, float x1, float y0, float y1)             // +0x30 slot 12
{ reinterpret_cast<void(__thiscall*)(void*,int,int,int,int,int,float,float,float,float)>(rcVtbl(rc)[0x30/4])(rc, unused, vx0, vy0, vw, vh, x0, x1, y0, y1); }
inline void rcEndDisplay(void* rc)                                            // +0x34 slot 13
{ reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(rc)[0x34/4])(rc); }
inline void rcSetTransform(void* rc, const float* m2x3)                       // +0x38 slot 14
{ reinterpret_cast<void(__thiscall*)(void*,const float*)>(rcVtbl(rc)[0x38/4])(rc, m2x3); }
inline void rcSetCxform(void* rc, const float* cx)                            // +0x3C slot 15
{ reinterpret_cast<void(__thiscall*)(void*,const float*)>(rcVtbl(rc)[0x3C/4])(rc, cx); }

// geometry + styles
inline void rcDrawVertices(void* rc, const void* verts, int count)            // +0x44 slot 17
{ reinterpret_cast<void(__thiscall*)(void*,const void*,int)>(rcVtbl(rc)[0x44/4])(rc, verts, count); }
inline void rcDrawLineStrip(void* rc, const void* verts, int count)           // +0x48 slot 18
{ reinterpret_cast<void(__thiscall*)(void*,const void*,int)>(rcVtbl(rc)[0x48/4])(rc, verts, count); }
inline void rcFillStyleDisable(void* rc, int side)                            // +0x4C slot 19
{ reinterpret_cast<void(__thiscall*)(void*,int)>(rcVtbl(rc)[0x4C/4])(rc, side); }
inline void rcFillStyleColor(void* rc, int side, unsigned rgba)               // +0x50 slot 20
{ reinterpret_cast<void(__thiscall*)(void*,int,unsigned)>(rcVtbl(rc)[0x50/4])(rc, side, rgba); }
inline void rcSetTexture(void* rc, int side, int texId, int w, int h,
                         const float* m, int wrapMode)                        // +0x54 slot 21
{ reinterpret_cast<void(__thiscall*)(void*,int,int,int,int,const float*,int)>(rcVtbl(rc)[0x54/4])(rc, side, texId, w, h, m, wrapMode); }
inline void rcLineStyleDisable(void* rc)                                      // +0x58 slot 22
{ reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(rc)[0x58/4])(rc); }
inline void rcLineStyleColor(void* rc, unsigned rgba)                         // +0x5C slot 23
{ reinterpret_cast<void(__thiscall*)(void*,unsigned)>(rcVtbl(rc)[0x5C/4])(rc, rgba); }
inline void rcLineStyleWidth(void* rc, unsigned widthAsDword)                 // +0x60 slot 24
{ reinterpret_cast<void(__thiscall*)(void*,unsigned)>(rcVtbl(rc)[0x60/4])(rc, widthAsDword); }
inline void rcDrawBitmapNull(void* rc, const void* m, int texId,
                             const void* coords, const void* uv, unsigned rgba) // +0x64 slot 25 (nullsub in SwiffRender)
{ reinterpret_cast<void(__thiscall*)(void*,const void*,int,const void*,const void*,unsigned)>(rcVtbl(rc)[0x64/4])(rc, m, texId, coords, uv, rgba); }

// masks
inline void rcBeginMask(void* rc)   { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(rc)[0x9C/4])(rc); }  // +0x9C slot 39
inline void rcEndMask(void* rc)     { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(rc)[0xA0/4])(rc); }  // +0xA0 slot 40
inline void rcDisableMask(void* rc) { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(rc)[0xA4/4])(rc); }  // +0xA4 slot 41
// [DICE-MOD SwiffPlayer #36] the text ("font wrapper") slots 26-37, the targets of the shipped
// render_handler forwarders 0x10005DC0..0x10005EC0.
inline void* rcTextCreate(void* rc, const char* name, int size)             { return reinterpret_cast<void*(__thiscall*)(void*, const char*, int)>(rcVtbl(rc)[0x68/4])(rc, name, size); }                    // +0x68 slot 26 createFont
inline void  rcTextRelease(void* rc, void* line)                            { reinterpret_cast<void(__thiscall*)(void*, void*)>(rcVtbl(rc)[0x6C/4])(rc, line); }                                              // +0x6C slot 27
inline void  rcTextSet(void* rc, void* line, const wchar_t* text)           { reinterpret_cast<void(__thiscall*)(void*, void*, const wchar_t*)>(rcVtbl(rc)[0x70/4])(rc, line, text); }                        // +0x70 slot 28 setText
inline void  rcTextDetach(void* rc, void* line)                             { reinterpret_cast<void(__thiscall*)(void*, void*)>(rcVtbl(rc)[0x74/4])(rc, line); }                                              // +0x74 slot 29
inline void  rcTextDraw(void* rc, void* line, const void* m, unsigned c, int f) { reinterpret_cast<void(__thiscall*)(void*, void*, const void*, unsigned, int)>(rcVtbl(rc)[0x78/4])(rc, line, m, c, f); }   // +0x78 slot 30 drawText
inline char  rcTextExtent(void* rc, void* line, int n, float* w, float* h)  { return reinterpret_cast<char(__thiscall*)(void*, void*, int, float*, float*)>(rcVtbl(rc)[0x7C/4])(rc, line, n, w, h); }      // +0x7C slot 31
inline char  rcTextHit(void* rc, void* line, int* pos, int x, int y)        { return reinterpret_cast<char(__thiscall*)(void*, void*, int*, int, int)>(rcVtbl(rc)[0x80/4])(rc, line, pos, x, y); }          // +0x80 slot 32
inline int   rcTextSelection(void* rc, void* line, int* a, int* b)          { return reinterpret_cast<int(__thiscall*)(void*, void*, int*, int*)>(rcVtbl(rc)[0x84/4])(rc, line, a, b); }                   // +0x84 slot 33 - AL answers "a selection exists" (sub_1009A910 -> sub_10088D60)
inline char  rcTextSetSelection(void* rc, void* line, int a, int b, int c, int d) { return reinterpret_cast<char(__thiscall*)(void*, void*, int, int, int, int)>(rcVtbl(rc)[0x88/4])(rc, line, a, b, c, d); } // +0x88 slot 34
inline void  rcTextBlur(void* rc, void* line)                               { reinterpret_cast<void(__thiscall*)(void*, void*)>(rcVtbl(rc)[0x8C/4])(rc, line); }                                              // +0x8C slot 35
inline void  rcTextSetSize(void* rc, void* line, float size, int flag)      { reinterpret_cast<void(__thiscall*)(void*, void*, float, int)>(rcVtbl(rc)[0x90/4])(rc, line, size, flag); }                    // +0x90 slot 36
inline void  rcTextCaret(void* rc, void* line, int pos)                     { reinterpret_cast<void(__thiscall*)(void*, void*, int)>(rcVtbl(rc)[0x94/4])(rc, line, pos); }                                    // +0x94 slot 37

// SwiffPlayer-driven slots
inline void rcSetMovieWidth(void* rc, unsigned w)   { reinterpret_cast<void(__thiscall*)(void*,unsigned)>(rcVtbl(rc)[0xA8/4])(rc, w); }  // +0xA8 slot 42
inline void rcSetMovieHeight(void* rc, int h)       { reinterpret_cast<void(__thiscall*)(void*,int)>(rcVtbl(rc)[0xAC/4])(rc, h); }       // +0xAC slot 43
inline void rcSetScale(void* rc, float sx, float sy){ reinterpret_cast<void(__thiscall*)(void*,float,float)>(rcVtbl(rc)[0xB0/4])(rc, sx, sy); } // +0xB0 slot 44
inline void rcSetBackgroundColor(void* rc, unsigned rgb) { reinterpret_cast<void(__thiscall*)(void*,unsigned)>(rcVtbl(rc)[0xB4/4])(rc, rgb); }  // +0xB4 slot 45
inline void rcSetMouseState(void* rc, int x, int y, int b) { reinterpret_cast<void(__thiscall*)(void*,int,int,int)>(rcVtbl(rc)[0xB8/4])(rc, x, y, b); } // +0xB8 slot 46
inline char rcCapability0(void* rc, int a)          { return reinterpret_cast<char(__thiscall*)(void*,int)>(rcVtbl(rc)[0xC4/4])(rc, a); } // +0xC4 slot 49
inline char rcCapability1(void* rc, int a)          { return reinterpret_cast<char(__thiscall*)(void*,int)>(rcVtbl(rc)[0xC8/4])(rc, a); } // +0xC8 slot 50
inline void rcSetPairState(void* rc, int a, int b)  { reinterpret_cast<void(__thiscall*)(void*,int,int)>(rcVtbl(rc)[0xD4/4])(rc, a, b); } // +0xD4 slot 53
inline void rcUpdateTime(void* rc, unsigned t, float dt) { reinterpret_cast<void(__thiscall*)(void*,unsigned,float)>(rcVtbl(rc)[0xD8/4])(rc, t, dt); } // +0xD8 slot 54

// Movie objects belong to this gameswf build and require its typed API; compatibility
// offsets from the older movie interface do not apply.
// Host callbacks below retain the separate ISwiffHost ABI.
inline void hostPreLoad10(void* h) { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(h)[0x10/4])(h); }   // +0x10
inline void hostPreLoad50(void* h) { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(h)[0x50/4])(h); }   // +0x50
inline void hostUpdate(void* h)    { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(h)[0x20/4])(h); }   // +0x20
inline void hostGetViewportSize(void* h, unsigned* w, unsigned* hh) { reinterpret_cast<void(__thiscall*)(void*,unsigned*,unsigned*)>(rcVtbl(h)[0x1DC/4])(h, w, hh); } // +0x1DC
// the SwiffHost slots the dice.bf2.Logic AS methods dispatch to (all read from the shipped
// handlers registered by the Logic ctor sub_1004DEF0; each cites its handler EA)
// In the shipped string-return calls, MSVC passed a 28-byte VC7.1 return buffer as the first
// stack argument for construction by the callee. Native bindings above use typed host calls.
inline bool hostIsGameRunning(void* h, bool* out)                                             // +184  sub_1004CC30
{ return reinterpret_cast<bool(__thiscall*)(void*,bool*)>(rcVtbl(h)[184/4])(h, out); }
inline int  hostGetActivateReason(void* h)                                                    // +188  sub_1004CCF0
{ return reinterpret_cast<int(__thiscall*)(void*)>(rcVtbl(h)[188/4])(h); }
inline void hostSetActivateReason(void* h, int v)                                             // +192  sub_1004CCF0 tail
{ reinterpret_cast<void(__thiscall*)(void*,int)>(rcVtbl(h)[192/4])(h, v); }
inline void hostGetFrameLabel(void* h, void* outStr)                                          // +196  sub_1004CD70
{ reinterpret_cast<void(__thiscall*)(void*,void*)>(rcVtbl(h)[196/4])(h, outStr); }
inline void hostGetModVersion(void* h, void* outStr)                                          // +416  sub_1004D520
{ reinterpret_cast<void(__thiscall*)(void*,void*)>(rcVtbl(h)[416/4])(h, outStr); }
inline void hostGetStorageString(void* h, void* outStr, const void* a, const void* b)         // +568  sub_1004DB60
{ reinterpret_cast<void(__thiscall*)(void*,void*,const void*,const void*)>(rcVtbl(h)[568/4])(h, outStr, a, b); }
inline void hostSetStorageString(void* h, const void* a, const void* b)                       // +572  sub_1004D0E0
{ reinterpret_cast<void(__thiscall*)(void*,const void*,const void*)>(rcVtbl(h)[572/4])(h, a, b); }
inline int  hostGetCommandLine(void* h)                                                       // +700  sub_1004D4B0
{ return reinterpret_cast<int(__thiscall*)(void*)>(rcVtbl(h)[700/4])(h); }

inline void hostHandleProfileTransition(void* h, void* stdStringName) { reinterpret_cast<void(__thiscall*)(void*,void*)>(rcVtbl(h)[0x244/4])(h, stdStringName); } // +0x244
inline void hostLocaleGetString(void* h, void* outWstr, const void* idStr)                       // +168  sub_1004C650
{ (void)reinterpret_cast<bool(__thiscall*)(void*, void*, const void*)>(rcVtbl(h)[168/4])(h, outWstr, idStr); }
inline int   hostRequest(void* h, void* req)   { return reinterpret_cast<int(__thiscall*)(void*, void*)>(rcVtbl(h)[44/4])(h, req); }        // +44   sub_1005FCF0 (request {id, strings})
inline void* hostGetCursor(void* h, int which) { return reinterpret_cast<void*(__thiscall*)(void*, int)>(rcVtbl(h)[108/4])(h, which); }    // +108  sub_1005FCF0 / sub_10013520
inline bool  hostSetBusy(void* h, int busy) { return reinterpret_cast<bool(__thiscall*)(void*, int)>(rcVtbl(h)[0x2B0/4])(h, busy); }
inline void  hostCursorRestored(void* h)       { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(h)[692/4])(h); }                        // +692  sub_1005FCF0
// the join page's slots: joinServer, the selected row, the sponsor rows
inline bool  hostCheckDisc(void* h)            { return reinterpret_cast<unsigned char(__thiscall*)(void*)>(rcVtbl(h)[0x2D0/4])(h) != 0; }   // +0x2D0  sub_100543B0
inline void  hostReportNoDisc(void* h)         { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(h)[0x2D4/4])(h); }                        // +0x2D4  sub_100543B0
inline bool  hostJoinServer(void* h, const void* address, int port, int queryPort)                                                         // +0x100  sub_100543B0
{ return reinterpret_cast<unsigned char(__thiscall*)(void*, const void*, int, int)>(rcVtbl(h)[0x100/4])(h, address, port, queryPort) != 0; }
inline void  hostSetSelectedServer(void* h, const void* address, int queryPort, int hostPort)                                              // +0x10C  sub_10051B20 / sub_10050EE0
{ reinterpret_cast<void(__thiscall*)(void*, const void*, int, int)>(rcVtbl(h)[0x10C/4])(h, address, queryPort, hostPort); }
inline void  hostGetSelectedServer(void* h, void* outStr, int* queryPort, int* hostPort)                                                   // +0x110  assigns into the caller's constructed string
{ reinterpret_cast<void(__thiscall*)(void*, void*, int*, int*)>(rcVtbl(h)[0x110/4])(h, outStr, queryPort, hostPort); }
inline int   hostGetIndexedInt(void* h, int index)          { return reinterpret_cast<int(__thiscall*)(void*, int)>(rcVtbl(h)[0x114/4])(h, index); }        // +0x114  sub_10051B20
inline void  hostSetIndexedInt(void* h, int index, int value) { reinterpret_cast<void(__thiscall*)(void*, int, int)>(rcVtbl(h)[0x118/4])(h, index, value); }  // +0x118
inline void* hostGetServerLogoManager(void* h)              { return reinterpret_cast<void*(__thiscall*)(void*)>(rcVtbl(h)[0x4C/4])(h); }                    // +0x4C   sub_10052BB0
inline void  hostGetJoinPassword(void* h, void* outStr)     { reinterpret_cast<void(__thiscall*)(void*, void*)>(rcVtbl(h)[0x180/4])(h, outStr); }             // +0x180  hidden-return string, sub_10052A00
inline void  hostSetJoinPassword(void* h, const void* str)  { reinterpret_cast<void(__thiscall*)(void*, const void*)>(rcVtbl(h)[0x184/4])(h, str); }          // +0x184  sub_10051650

// ProfileManager (BF2.exe singleton; null in the harness)
inline unsigned char pmHasPendingTransition(void* pm, void* stdStringOut) { return reinterpret_cast<unsigned char(__thiscall*)(void*,void*)>(rcVtbl(pm)[88/4])(pm, stdStringOut); } // +0x58
inline void pmClear(void* pm) { reinterpret_cast<void(__thiscall*)(void*)>(rcVtbl(pm)[72/4])(pm); }   // +0x48

// A queued mouse sample - 12 bytes, the element of the +1104 vc71-shaped vector.
struct MouseState { int x; int y; int buttons; };

// Reinterpret a float's bits as a dword (updateTime/line_style_width pass floats through dword slots).
inline unsigned floatBits(float f) { unsigned d; ::memcpy(&d, &f, 4); return d; }

} // anonymous namespace


// SwiffPlayer_ctor 0x10006EF0

// [TEMP] SwiffCrashLog.cpp - the env-gated crash logger (BF2_SWIFFPLAYER_CRASHLOG). [RETIRE-WHEN: RW-89]
} } }   // close dice::hfe::ui for the global declaration
void swiffCrashLogInstall();
namespace dice { namespace hfe { namespace ui {

SwiffPlayer::SwiffPlayer()
{
    ::swiffCrashLogInstall();          // [TEMP] no-op unless BF2_SWIFFPLAYER_CRASHLOG is set

    // The two vtables (this->vtable at +0 and render_handler.vtable at +4) are set by C++ object
    // construction - the ISwiffPlayer base ctor and the SwiffRenderHandler member ctor - which is
    // exactly the base-then-derived vtable init the shipped ctor does by hand at 0x10006EF0.

    ref_count            = 1;
    deferred_mouse_dirty = 1;        // the shipped ctor sets this to 1
    legacy_flag_44c      = 1;
    render_context       = 0;
    movie_definition     = 0;
    movie_instance       = 0;
    bf2_voip_client      = 0;
    movie_width          = 0;
    movie_height         = 0;
    mouse_x = mouse_y = mouse_buttons = 0;
    left_shift_down = right_shift_down = left_alt_down = right_alt_down = 0;
    left_ctrl_down = right_ctrl_down = unknown_36 = 0;
    voip_update_flag     = 0;
    deferred_mouse_x = deferred_mouse_y = 0;
    mode_flag            = 0;
    pad_45[0] = pad_45[1] = pad_45[2] = 0;
    swiff_host           = 0;
    elapsed_time         = 0.0f;
    pad_44d[0] = pad_44d[1] = pad_44d[2] = 0;
    queued_mouse_aux = queued_mouse_begin = queued_mouse_end = queued_mouse_capacity = 0;

    g_swiffPlayerInstance = this;

    // The shipped ctor's sub_1006F100(0) sets a gameswf file-scope byte to 0 [UNRESOLVED: which
    // gameswf setter]. That byte is zero-initialised anyway, so omitting the call is a no-op
    // equivalent; resolve and reinstate in Stage 3 if the movie path proves it matters.

    // The three gameswf registrations (file / fscommand / render), in the shipped
    // ctor's order at 10006F79..10006F8F. NO sound handler is registered -
    // the shipped ctor registers exactly these three.
    gameswf::register_file_opener_callback(swiffFileOpener);
    gameswf::register_fscommand_callback(swiffFsCommand);
    gameswf::set_render_handler(&render_handler);

    // Off unless BF2_SWIFFPLAYER_ASLOG=1 - see asLogEnabled(). Retail registers no log callback.
    if (asLogEnabled())
        gameswf::register_log_callback(swiffAsLogCb);

    initKeyTranslation();

    // getSingleton("Bf2VoipClient") - the BF2.exe VoIP client (null in the harness). classManager
    // vslot +0x48.
    bf2_voip_client = static_cast<void*>(
        classManager->getSingleton(renddx9abi::HostString("Bf2VoipClient")));
}


// Releases movie state, gameswf globals, and queued mouse storage.

SwiffPlayer::~SwiffPlayer()
{
    // Shipped dtor also: sub_10071710 gameswf-global check, drops movie_definition/movie_instance
    // via sub_1006F2A0, sub_10077700 gameswf AS/GC teardown, and a LocalProfileManager voip-volume
    // save. All are movie/voip-state teardown; with both null here they are no-ops. [Stage 3]

    voip_update_flag = 0;
    if (queued_mouse_begin)
        ::renddx9abi::engineFree(queued_mouse_begin);
    queued_mouse_begin = queued_mouse_end = queued_mouse_capacity = 0;

    if (g_swiffPlayerInstance == this)
        g_swiffPlayerInstance = 0;
}


// IBase quartet (slots 0-3).

void SwiffPlayer::addRef()   { ++ref_count; }
int  SwiffPlayer::getRef()   { return ref_count; }

// Release one reference; at zero, destroy the object once and return its storage through
// engineFree to the shared native CRT. This keeps the existing explicit teardown path.
int SwiffPlayer::release()
{
    const int remaining = --ref_count;
    if (remaining == 0)
    {
        this->~SwiffPlayer();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// (__fastcall in the shipped image; a virtual here, dispatched the same way). Answers
// iid 1 (IBase) and 870001 (ISwiffPlayer), returning the RAW this with no adjustment - correct
// because ISwiffPlayer is the sole base at +0.
void* SwiffPlayer::queryInterface(unsigned int iid) const
{
    if (iid == 1u /* IID_IBase */ || iid == IID_ISwiffPlayer)
        return const_cast<SwiffPlayer*>(this);
    return 0;
}


// SwiffPlayer_initKeyTranslation the scancode->vkey table, extracted verbatim.

void SwiffPlayer::initKeyTranslation()
{
    ::memset(key_translation, 0, sizeof(key_translation));
    key_translation[42]  = 16;  key_translation[54]  = 16;   // shift
    key_translation[29]  = 17;  key_translation[157] = 17;   // ctrl
    key_translation[56]  = 18;  key_translation[184] = 18;   // alt
    key_translation[211] = 46;                                // delete
    key_translation[14]  = 8;                                 // backspace
    key_translation[15]  = 9;                                 // tab
    key_translation[28]  = 13;                                // enter
    key_translation[203] = 37;  key_translation[205] = 39;    // left / right
    key_translation[200] = 38;  key_translation[208] = 40;    // up / down
    key_translation[199] = 36;  key_translation[207] = 35;    // home / end
    key_translation[11]  = 48;
    key_translation[2]   = 49;  key_translation[3]   = 50;  key_translation[4] = 51;
    key_translation[5]   = 52;  key_translation[6]   = 53;  key_translation[7] = 54;
    key_translation[8]   = 55;  key_translation[9]   = 56;  key_translation[10] = 57;
}


// The pure member accessors - real and complete. No render_context / movie / host dispatch, so no
// frontier.

bool  SwiffPlayer::isMovieLoaded()                 { return movie_instance != 0; }
void  SwiffPlayer::setRender(ISwiffRender* r)      { render_context = r; }
ISwiffRender* SwiffPlayer::getRender()             { return render_context; }
void  SwiffPlayer::setSwiffHost(ISwiffHost* h)     { swiff_host = h; }
ISwiffHost* SwiffPlayer::getSwiffHost()            { return swiff_host; }
int   SwiffPlayer::getMovieWidth()                 { return movie_width; }
int   SwiffPlayer::getMovieHeight()                { return movie_height; }
void* SwiffPlayer::getBf2VoipClient()              { return bf2_voip_client; }
void  SwiffPlayer::setVoipUpdateFlag(char v)       { voip_update_flag = (unsigned char)v; }
void  SwiffPlayer::legacyModeOff()                 { legacy_flag_44c = 0; }
void  SwiffPlayer::legacyModeOn()                  { legacy_flag_44c = 1; }
char  SwiffPlayer::getModeFlag()                   { return (char)mode_flag; }
void  SwiffPlayer::setModeFlag(char v)             { mode_flag = (unsigned char)v; }
void  SwiffPlayer::reserved14(int)                 { /* nullsub_20 0x100057F0 */ }
void  SwiffPlayer::reserved15(int)                 { /* nullsub_21 0x10005800 */ }

// store the deferred mouse position (as a qword at +60) and set the dirty flag.
void SwiffPlayer::setMousePositionDeferred(int x, int y)
{
    if (deferred_mouse_x != x || deferred_mouse_y != y)
    {
        deferred_mouse_x     = x;
        deferred_mouse_y     = y;
        deferred_mouse_dirty = 1;
    }
}

// clear the three modifier latches.
void SwiffPlayer::resetModifierState()
{
    s_ctrlHeld = 0;
    s_altHeld  = 0;
    s_shiftHeld = 0;
}


// notifyKey the modifier edge-detect state machine, then gameswf::notify_key_event.
// Real: it touches only object members (the L/R modifier bytes) and the linked gameswf function.

void SwiffPlayer::notifyKey(int code, int down)
{
    const unsigned char d = (unsigned char)down;
    switch (code)
    {
    case 2:   gameswf::notify_key_event((gameswf::key::code)49, down != 0); break;
    case 3:   gameswf::notify_key_event((gameswf::key::code)50, down != 0); break;
    case 4:   gameswf::notify_key_event((gameswf::key::code)51, down != 0); break;
    case 5:   gameswf::notify_key_event((gameswf::key::code)52, down != 0); break;
    case 6:   gameswf::notify_key_event((gameswf::key::code)53, down != 0); break;
    case 7:   gameswf::notify_key_event((gameswf::key::code)54, down != 0); break;
    case 8:   gameswf::notify_key_event((gameswf::key::code)55, down != 0); break;
    case 9:   gameswf::notify_key_event((gameswf::key::code)56, down != 0); break;
    case 10:  gameswf::notify_key_event((gameswf::key::code)57, down != 0); break;
    case 11:  gameswf::notify_key_event((gameswf::key::code)48, down != 0); break;
    case 28:  gameswf::notify_key_event((gameswf::key::code)13, down != 0); break;
    // Modifiers: fire the gameswf event only on the first of the L/R pair to change.
    case 29:  { const bool other = right_ctrl_down != 0; left_ctrl_down = d;
                if (!other) gameswf::notify_key_event((gameswf::key::code)17, down != 0); } break;
    case 157: { const bool other = left_ctrl_down != 0; right_ctrl_down = d;
                if (!other) gameswf::notify_key_event((gameswf::key::code)17, down != 0); } break;
    case 42:  { const bool other = right_shift_down != 0; left_shift_down = d;
                if (!other) gameswf::notify_key_event((gameswf::key::code)16, down != 0); } break;
    case 54:  { const bool other = left_shift_down != 0; right_shift_down = d;
                if (!other) gameswf::notify_key_event((gameswf::key::code)16, down != 0); } break;
    case 56:  { const bool other = right_alt_down != 0; left_alt_down = d;
                if (!other) gameswf::notify_key_event((gameswf::key::code)18, down != 0); } break;
    case 184: { const bool other = left_alt_down != 0; right_alt_down = d;
                if (!other) gameswf::notify_key_event((gameswf::key::code)18, down != 0); } break;
    default:
        gameswf::notify_key_event((gameswf::key::code)key_translation[code & 0xFF], down != 0);
        break;
    }
}


// The frontiers - every slot that dispatches by raw byte offset into render_context, movie_instance
// or swiff_host, or that drives the full movie/AS pipeline. Real signatures (frame balanced), named
// stops. Land in Stage 3 (movie/render/input) and Stage 5 (text). Slot = the shipped vtable offset.

// render_context->setScale, guarded on render_context (as the shipped body is).
void SwiffPlayer::setScale(float xScale, float yScale)
{
    if (streamLogEnabled())   // [TEMP] stream tracer
        ::renddx9abi::logf("[stream] setScale(%.4f, %.4f) rc=%p", xScale, yScale, render_context);
    if (render_context)
        rcSetScale(render_context, xScale, yScale);
}

// loadMovie. Stage 3: parse + instance + push size, real. The bitmap-character walk
// (external images -> loadTexture) and the 27-namespace AS graph read dice-modified gameswf internals
// by raw offset and are deferred to Stage 5; the movie still parses, advances and renders its vector
// shapes without them.
bool SwiffPlayer::loadMovie(const char* movieName)
{
    ISwiffHost* host = getSwiffHost();
    if (host) { hostPreLoad10(host); hostPreLoad50(host); }   // getSwiffHost()->+0x10, +0x50

    char path[512];
    ::strcpy(path, "Menu/External/FlashMenu/");
    if (movieName)
        ::strncat(path, movieName, sizeof(path) - 1 - ::strlen(path));

    int   version = 0, frameCount = 0, tagCount = 0;
    float fps = 0.0f;
    gameswf::get_movie_info(path, &version, &movie_width, &movie_height, &fps, &frameCount, &tagCount);
    if (streamLogEnabled())   // [TEMP] stream tracer
        ::renddx9abi::logf("[stream] loadMovie '%s': version=%d size=%dx%d fps=%.2f frames=%d tags=%d",
                           path, version, movie_width, movie_height, fps, frameCount, tagCount);
    if (version == 0)
    {
        ::renddx9abi::logf("[SwiffPlayer::loadMovie] SwiffPlayer.cpp:277 - get_movie_info failed: '%s'", path);
        return false;
    }

    // Push the movie size into the renderer (shipped: no null guard - render_context is set first).
    rcSetMovieWidth(render_context, static_cast<unsigned>(movie_width));
    rcSetMovieHeight(render_context, movie_height);

    // DO_NOT_LOAD_BITMAPS - the documented host-texture policy, and the reason the shipped DLL
    // links no zlib/jpeglib: menu bitmaps are never decoded, only named and resolved externally.
    gameswf::set_default_create_bitmaps(gameswf::DO_NOT_LOAD_BITMAPS);
    gameswf::movie_definition* def = gameswf::create_library_movie(path);
    movie_definition = def;
    if (!def)
    {
        ::renddx9abi::logf("[SwiffPlayer::loadMovie] SwiffPlayer.cpp:288 - can't create movie from '%s'", path);
        return false;
    }
    gameswf::movie_interface* inst = def->create_instance();
    movie_instance = inst;
    if (!inst)
    {
        ::renddx9abi::logf("[SwiffPlayer::loadMovie] SwiffPlayer.cpp:295 - can't create movie instance");
        return false;
    }

    // Resolves every bitmap through the renderer using its copied export name, or ""
    // for unnamed characters. Copy names from the export hash to avoid dangling c_str pointers.
    {
        tu_string              names[256];
        gameswf::bitmap_info*  infos[256];
        const int n = def->get_exported_bitmaps(256, names, infos);
        const int total = def->get_bitmap_info_count();
        int resolved = 0, unnamed = 0;
        for (int i = 0; i < total; ++i)
        {
            gameswf::bitmap_info* bi = def->get_bitmap_info(i);
            if (!bi) continue;
            const char* name = "";
            for (int k = 0; k < n; ++k)
                if (infos[k] == bi) { name = names[k].c_str(); break; }
            int tw = 0, th = 0;
            const int handle = render_context ? rcLoadTexture(render_context, name, &tw, &th) : -1;
            if (handle >= 0)
            {
                // The handle IS the texture; the bitmap_info carries nothing else (no pixels).
                bi->m_texture_id      = static_cast<unsigned>(handle);
                bi->m_original_width  = tw;
                bi->m_original_height = th;
                if (name[0]) ++resolved; else ++unnamed;
            }
            else if (name[0])
            {
                ::renddx9abi::logf("[SwiffPlayer::loadMovie] loadTexture(\"%s\") failed", name);
            }
        }
        ::renddx9abi::logf("[SwiffPlayer::loadMovie] bitmaps: %d characters, %d named exports, "
                           "%d named resolved via loadTexture, %d unnamed given handles",
                           total, n, resolved, unnamed);
    }

    // Publish the ActionScript namespace graph before the movie's first advance.
    // Registration adapts the engine bindings to this gameswf version's fn_call ABI.
    buildAsGraph();

    ::renddx9abi::logf("[SwiffPlayer::loadMovie] returning true");
    return true;
}

// update. The order is load-bearing: profile-transition early-out, host update, updateTime,
// mouse flush, viewport, advance, [bg color - drifted, see below], VoIP tick.
void SwiffPlayer::update(float deltaTime)
{
    { static int s_n = 0; ++s_n; if (s_n <= 3 || (s_n % 100) == 0) ::renddx9abi::logf("[frame] %s call#%d", "update", s_n); }
    // [TEMP] stream tracer: the player-side inputs, first N updates + every 100th.
    static int s_updates = 0;
    ++s_updates;
    const bool traceUpdate = streamLogEnabled()
        && (s_updates <= streamLogFrames() || (s_updates % 100) == 0);
    if (traceUpdate)
        ::renddx9abi::logf("[stream] update #%d dt=%.5f elapsed=%.3f movie=%p host=%p rc=%p size=%dx%d",
                           s_updates, deltaTime, elapsed_time, movie_instance, swiff_host, render_context,
                           movie_width, movie_height);
    if (!movie_instance)
        return;

    // the returned manager is guarded, but the registry
    // and the transition's host are not. The transition string outlives update().
    IBase* pm = classManager->getSingleton(renddx9abi::HostString("ProfileManager"));
    if (pm)
    {
        renddx9abi::HostString transitionName;
        const bool pending = static_cast<IProfileManager*>(pm)->hasPendingTransition(transitionName);
        if (pending)
        {
            swiff_host->handleProfileTransition(transitionName);
            if (traceUpdate)   // [TEMP] stream tracer
                ::renddx9abi::logf("[stream]   update #%d: ProfileManager transition pending -> early return", s_updates);
            return;
        }
        (void)static_cast<IProfileManager*>(pm)->update();
    }

    if (swiff_host)
        hostUpdate(swiff_host);                                     // SwiffHost->update() +0x20
    // [TEMP] RW-89 host input-block tracer (BF2_SWIFFPLAYER_ASLOG): SwiffHost +696 is the flag the menu
    // bridge consults before queuing a mouse press (sub_4A2D60/sub_4A2DD0 in BF2.exe); interactive run
    // drive10 lost every click after the multiplayer tab while mouse moves kept arriving. Logged on
    // change only, with the display count, so the AS lines around the flip name what set it.
    if (swiff_host && asLogEnabled())
    {
        static int s_busy = -1;
        const int busy = reinterpret_cast<int(__thiscall*)(void*)>(rcVtbl(swiff_host)[696/4])(swiff_host);
        if (busy != s_busy)
        {
            ::renddx9abi::logf("[host] input-block flag %d -> %d (display #%d)", s_busy, busy, s_streamDisplays);
            s_busy = busy;
        }
    }

    elapsed_time += deltaTime;
    if (render_context)
        rcUpdateTime(render_context, floatBits(elapsed_time), deltaTime);   // +0xD8

    // Mouse flush: the queued path if there is queued input or the deferred sample is already spent;
    // otherwise apply the deferred position once and mark it spent.
    const bool haveQueued = queued_mouse_begin &&
        ((static_cast<char*>(queued_mouse_end) - static_cast<char*>(queued_mouse_begin)) / (int)sizeof(MouseState)) > 0;
    if (haveQueued || !deferred_mouse_dirty)
    {
        processQueuedMouseState();     // virtual, exactly as the shipped update dispatches it
    }
    else
    {
        notifyMouse(deferred_mouse_x, deferred_mouse_y, mouse_buttons);
        deferred_mouse_dirty = 0;
    }

    gameswf::movie_interface* mi = static_cast<gameswf::movie_interface*>(movie_instance);
    mi->set_display_viewport(0, 0, movie_width, movie_height);      // typed (2006 4-arg)
    mi->advance(deltaTime);
    if (traceUpdate)   // [TEMP] stream tracer
        ::renddx9abi::logf("[stream]   viewport=(0,0,%d,%d) after advance: frame=%d/%d play=%d mouse=(%d,%d,%d)",
                           movie_width, movie_height, mi->get_current_frame(),
                           movie_definition ? static_cast<gameswf::movie_definition*>(movie_definition)->get_frame_count() : -1,
                           (int)mi->get_play_state(), mouse_x, mouse_y, mouse_buttons);

    // Propagate the movie background color to the renderer each frame.
    // [DICE-MOD] The movie interface exposes the background getter used by this bridge.
    if (render_context)
    {
        const gameswf::rgba bg = mi->get_background_color();
        unsigned bgDword = 0;
        ::memcpy(&bgDword, &bg, 4);
        rcSetBackgroundColor(render_context, bgDword);
    }

    if (voip_update_flag && bf2_voip_client)
    {
        // VoIP tick (Stage 6). The shipped guard: if !voip->isBusy() then voip->tick().
    }
}

// display: the live movie draws itself (typed).
void SwiffPlayer::display()
{
    { static int s_n = 0; ++s_n; if (s_n <= 3 || (s_n % 100) == 0) ::renddx9abi::logf("[frame] %s call#%d", "display", s_n); }
    // [TEMP] stream tracer: the first N displays in full, then every 100th; one summary line each.
    if (streamLogEnabled())
    {
        ++s_streamDisplays;
        s_streamVerbose = (s_streamDisplays <= streamLogFrames()) || (s_streamDisplays % 100) == 0;
        ::memset(s_streamCounts, 0, sizeof(s_streamCounts));
        ::renddx9abi::logf("[stream] display #%d%s", s_streamDisplays, s_streamVerbose ? " [FULL]" : "");
    }
    if (movie_instance)
        static_cast<gameswf::movie_interface*>(movie_instance)->display();
    if (streamLogEnabled())
        ::renddx9abi::logf("[stream] display #%d done: transforms=%u strips=%u verts=%u lines=%u fills=%u "
                           "tex=%u masks=%u cxforms=%u",
                           s_streamDisplays, s_streamCounts[0], s_streamCounts[1], s_streamCounts[2],
                           s_streamCounts[3], s_streamCounts[4], s_streamCounts[5], s_streamCounts[6],
                           s_streamCounts[7]);
}

// push a mouse sample onto the +1104 vc71-shaped vector (grows on the engine heap).
void SwiffPlayer::queueMouseState(int x, int y, int buttons)
{
    // [TEMP] RW-89 queue tracer (BF2_SWIFFPLAYER_ASLOG): every button change the engine queues and the
    // queue depth then, plus a depth line every 2000th sample - drive12 showed the host's input-block
    // flag flat at 0 while no press reached the movie after the multiplayer tab; this says whether the
    // engine still queues presses (starvation in our drain) or stopped calling (engine-side routing).
    if (asLogEnabled())
    {
        static int s_lastButtons = 0; static unsigned s_samples = 0;
        const int depth = queued_mouse_begin ? (int)((static_cast<char*>(queued_mouse_end) - static_cast<char*>(queued_mouse_begin)) / (int)sizeof(MouseState)) : 0;
        ++s_samples;
        if (buttons != s_lastButtons || (s_samples % 2000) == 0)
            ::renddx9abi::logf("[mouse] queued buttons %d -> %d at (%d,%d) depth=%d sample#%u", s_lastButtons, buttons, x, y, depth, s_samples);
        s_lastButtons = buttons;
    }
    MouseState ms; ms.x = x; ms.y = y; ms.buttons = buttons;
    vc71::vectorRef<MouseState> q(&queued_mouse_aux);
    q.push_back(ms);
}

// text input (Stage 5): these dispatch through the host IME handler + write into a host std::string.
static void swiffKeyDownLatch(int vkey);   // defined below
// SwiffPlayer_notifyChar a digit goes down the key path (gameswf key event + the latch,
// so the field's keymap types it); any other character goes to the focused field's slot 1.
void  SwiffPlayer::notifyChar(int character)
{
    const char c = (char) character;
    if (c >= 48 && c < 58)
    {
        gameswf::notify_key_event((gameswf::key::code) c, true);
        swiffKeyDownLatch(c);
        return;
    }
    if (gameswf::text_input* field = gameswf::get_focused_text_input())
        field->on_char(character);
}
// sub_10004FF0 - the key-down latch. Sets the modifier bytes the shipped image keeps at file scope,
// toggles caps on vkey 20, then forwards the vkey to the optional text-input/IME handler.
static void swiffKeyDownLatch(int vkey)
{
    if      (vkey == 16) s_shiftHeld  = 1;
    else if (vkey == 18) s_altHeld    = 1;
    else if (vkey == 17) s_ctrlHeld   = 1;
    else if (vkey == 20) s_capsToggle = (s_capsToggle == 0);   // caps is a toggle, not a latch
    // The "IME handler" of the shipped latch IS the focused text field's +200 interface
    // (dword_100F8EE0): its slot 0 takes the virtual key. [DICE-MOD #37]
    gameswf::set_text_input_modifiers(s_shiftHeld != 0, s_altHeld != 0, s_ctrlHeld != 0, s_capsToggle != 0);
    if (gameswf::text_input* field = gameswf::get_focused_text_input())
        field->on_key(vkey);
    else if (s_imeHandler)
        reinterpret_cast<void(__thiscall*)(void*,int)>(rcVtbl(s_imeHandler)[0])(s_imeHandler, vkey);
}

// notifyKey first (the virtual, so an override would be honoured), then latch on press /
// release the three modifier bytes on release. Note the release arm handles only 16/18/17 - caps is
// deliberately not cleared, because it is a toggle.

// The shipped body indexes key_translation[code] with NO bounds check. The table is 255 ints and
// `code` is a raw scancode from BF2, so a stray value reads off the end. We guard; retail does not.
void  SwiffPlayer::notifyKeyState(int code, int down)
{
    notifyKey(code, down);
    if (code < 0 || code >= 255)
        return;
    const int vkey = key_translation[code];
    if ((unsigned char)down)
    {
        swiffKeyDownLatch(vkey);
    }
    else
    {
        if      (vkey == 16) s_shiftHeld = 0;
        else if (vkey == 18) s_altHeld   = 0;
        else if (vkey == 17) s_ctrlHeld  = 0;
        gameswf::set_text_input_modifiers(s_shiftHeld != 0, s_altHeld != 0, s_ctrlHeld != 0, s_capsToggle != 0);
    }
}
// 0x10006DB0 setTextInput: the host hands over clipboard text -> `src`, then the focused field's
// paste (slot 2). 0x10006E30 / 0x10006E90 getTextInput0/1: the focused field's copy (slot 3) / cut
// (slot 4), then `src` is assigned into the host's PRE-constructed std::string. [DICE-MOD #37]
void  SwiffPlayer::setTextInput(const char* text)
{
    gameswf::text_input_clipboard() = text ? text : "";
    if (gameswf::text_input* field = gameswf::get_focused_text_input())
        field->paste();
}
void  SwiffPlayer::getTextInput0(void* outStdString)
{
    if (gameswf::text_input* field = gameswf::get_focused_text_input())
        field->copy();
    if (outStdString)
        *reinterpret_cast<renddx9abi::HostString*>(outStdString) = gameswf::text_input_clipboard().c_str();
}
void  SwiffPlayer::getTextInput1(void* outStdString)
{
    if (gameswf::text_input* field = gameswf::get_focused_text_input())
        field->cut();
    if (outStdString)
        *reinterpret_cast<renddx9abi::HostString*>(outStdString) = gameswf::text_input_clipboard().c_str();
}

// render capability queries, guarded.
char SwiffPlayer::renderCapability0(int a) { return render_context ? rcCapability0(render_context, a) : 0; }
char SwiffPlayer::renderCapability1(int a) { return render_context ? rcCapability1(render_context, a) : 0; }

// render pair-state, guarded.
void SwiffPlayer::renderSetPairState(int a, int b)
{
    if (render_context)
        rcSetPairState(render_context, a, b);
}

// notify_mouse. Scales the raw (x,y) from the SwiffHost viewport into movie space,
// clamps, feeds the movie and the renderer.
void SwiffPlayer::notifyMouse(int x, int y, int buttons)
{
    unsigned vpW = 1, vpH = 1;
    if (swiff_host)
        hostGetViewportSize(swiff_host, &vpW, &vpH);               // +0x1DC, two out-params
    if (vpW == 0) vpW = 1;
    if (vpH == 0) vpH = 1;

    const double sx = static_cast<double>(static_cast<unsigned>(getMovieWidth()))  / static_cast<double>(vpW);
    const double sy = static_cast<double>(static_cast<unsigned>(getMovieHeight())) / static_cast<double>(vpH);
    int mx = static_cast<int>(static_cast<double>(x) * sx);
    int my = static_cast<int>(sy * static_cast<double>(y));
    if (mx < 0) mx = 0;
    if (mx > movie_width)  mx = movie_width;
    if (my < 0) my = 0;
    if (my > movie_height) my = movie_height;
    mouse_x = mx;
    mouse_y = my;
    // [TEMP] RW-89 mouse tracer (BF2_SWIFFPLAYER_ASLOG): every button transition the host delivers,
    // raw and in movie space - interactive run drive6 had an OK click that reached no handler and
    // the logs could not say whether the press ever arrived. [RETIRE-WHEN: RW-89]
    if (asLogEnabled() && buttons != mouse_buttons)
        ::renddx9abi::logf("[mouse] buttons %d -> %d at raw=(%d,%d) movie=(%d,%d)", mouse_buttons, buttons, x, y, mx, my);
    {   // [TEMP] stream tracer: the host viewport the mouse scale is derived from (first 3 + full frames)
        static int s_n = 0;
        if (streamLogEnabled() && (++s_n <= 3 || s_streamVerbose))
            ::renddx9abi::logf("[stream] notifyMouse raw=(%d,%d,%d) hostViewport=%ux%u movie=%dx%d -> (%d,%d)",
                               x, y, buttons, vpW, vpH, movie_width, movie_height, mx, my);
    }

    if (movie_instance)
        static_cast<gameswf::movie_interface*>(movie_instance)->notify_mouse_state(mx, my, buttons);  // typed
    if (render_context)
        rcSetMouseState(render_context, mx, my, buttons);          // +0xB8
    mouse_buttons = buttons;
}

// pop the front queued sample and deliver it. No-op when the queue is empty.
void SwiffPlayer::processQueuedMouseState()
{
    if (!queued_mouse_begin)
        return;
    MouseState* begin = static_cast<MouseState*>(queued_mouse_begin);
    MouseState* end   = static_cast<MouseState*>(queued_mouse_end);
    const int count = static_cast<int>(end - begin);
    if (count <= 0)
        return;

    const MouseState front = begin[0];
    // erase-front: shift the rest down one and retract end (the shipped sub_10007370).
    for (int i = 1; i < count; ++i)
        begin[i - 1] = begin[i];
    queued_mouse_end = static_cast<char*>(queued_mouse_end) - sizeof(MouseState);

    notifyMouse(front.x, front.y, front.buttons);
}


// Forwards gameswf rendering callbacks to the matching SwiffRender operations.
// setRender and loadMovie must supply render_context before any callback; no null guards.

namespace {
inline void* rhRc(SwiffRenderHandler* self)
{ return *reinterpret_cast<void**>(reinterpret_cast<char*>(self) + 8); }
inline unsigned rgbaDword(const gameswf::rgba& c) { unsigned d; ::memcpy(&d, &c, 4); return d; }
} // anonymous namespace

gameswf::bitmap_info* SwiffRenderHandler::create_bitmap_info_empty()
{
    gameswf::bitmap_info* bi = new gameswf::bitmap_info();
    bi->m_texture_id = static_cast<unsigned>(rcCreateEmpty(rhRc(this)));
    return bi;
}
gameswf::bitmap_info* SwiffRenderHandler::create_bitmap_info_alpha(int w, int h, unsigned char* data)
{
    gameswf::bitmap_info* bi = new gameswf::bitmap_info();
    bi->m_texture_id     = static_cast<unsigned>(rcUploadAlpha(rhRc(this), (unsigned)w, (unsigned)h, data));
    bi->m_original_width  = w;
    bi->m_original_height = h;
    return bi;
}
gameswf::bitmap_info* SwiffRenderHandler::create_bitmap_info_rgb(image::rgb* im)
{
    gameswf::bitmap_info* bi = new gameswf::bitmap_info();
    bi->m_texture_id = static_cast<unsigned>(rcCreateRgb(rhRc(this), im));
    if (im) { bi->m_original_width = im->m_width; bi->m_original_height = im->m_height; }
    return bi;
}
gameswf::bitmap_info* SwiffRenderHandler::create_bitmap_info_rgba(image::rgba* im)
{
    gameswf::bitmap_info* bi = new gameswf::bitmap_info();
    bi->m_texture_id = static_cast<unsigned>(rcUploadRgba(rhRc(this), im));
    if (im) { bi->m_original_width = im->m_width; bi->m_original_height = im->m_height; }
    return bi;
}
// The shipped slot-5 forwarder: create_bitmap_info_file(path, &w, &h) -> ISwiffRender::loadTexture
// (+0x28). The handle IS the texture; a failed resolve (-1) is "no image", which the loadMovie
// image loader treats as nothing to place.
gameswf::bitmap_info* SwiffRenderHandler::create_bitmap_info_file(const char* path)
{
    int w = 0, h = 0;
    const int handle = rcLoadTexture(rhRc(this), path ? path : "", &w, &h);
    if (handle < 0)
    {
        ::renddx9abi::logf("[SwiffPlayer] create_bitmap_info_file(\"%s\") - loadTexture failed", path ? path : "");
        return 0;
    }
    gameswf::bitmap_info* bi = new gameswf::bitmap_info();
    bi->m_texture_id      = static_cast<unsigned>(handle);
    bi->m_original_width  = w;
    bi->m_original_height = h;
    return bi;
}
void SwiffRenderHandler::delete_bitmap_info(gameswf::bitmap_info* bi)
{
    // Release the render texture; gameswf's ref-counting frees the bitmap_info object itself.
    if (bi)
        rcReleaseTexture(rhRc(this), static_cast<int>(bi->m_texture_id));
}
void SwiffRenderHandler::begin_display(gameswf::rgba background_color,
                                       int vx0, int vy0, int vw, int vh,
                                       float x0, float x1, float y0, float y1)
{
    // The shipped forwarder tail-calls, so the rgba lands in SwiffRender's "unused" first slot (the
    // menu background is driven by beginFrame/SwiffHost, not here). Pass its dword to match.
    SPL("[stream]  begin_display vp=(%d,%d,%d,%d) bounds=(%.0f,%.0f,%.0f,%.0f) bg=%08X",
        vx0, vy0, vw, vh, x0, x1, y0, y1, rgbaDword(background_color));   // [TEMP] stream tracer
    rcBeginDisplay(rhRc(this), static_cast<int>(rgbaDword(background_color)), vx0, vy0, vw, vh, x0, x1, y0, y1);
}
void SwiffRenderHandler::end_display()
{ SPL("[stream]  end_display"); rcEndDisplay(rhRc(this)); }
void SwiffRenderHandler::set_matrix(const gameswf::matrix& m)
{
    const float* mf = reinterpret_cast<const float*>(&m);
    if (streamLogEnabled())   // [TEMP] stream tracer
    {
        ++s_streamCounts[0];
        SPL("[stream]   set_matrix [%.4f %.4f %.1f | %.4f %.4f %.1f]", mf[0], mf[1], mf[2], mf[3], mf[4], mf[5]);
    }
    rcSetTransform(rhRc(this), mf);
}
void SwiffRenderHandler::set_cxform(const gameswf::cxform& cx)
{
    const float* cf = reinterpret_cast<const float*>(&cx);
    if (streamLogEnabled())   // [TEMP] stream tracer
    {
        ++s_streamCounts[7];
        SPL("[stream]   set_cxform [%.3f %.1f %.3f %.1f %.3f %.1f %.3f %.1f]",
            cf[0], cf[1], cf[2], cf[3], cf[4], cf[5], cf[6], cf[7]);
    }
    rcSetCxform(rhRc(this), cf);
}
void SwiffRenderHandler::draw_mesh_strip(const void* coords, int vertex_count)
{
    if (streamLogEnabled())   // [TEMP] stream tracer
    {
        ++s_streamCounts[1];
        s_streamCounts[2] += (unsigned)vertex_count;
        const short* v = static_cast<const short*>(coords);
        const int n = vertex_count;
        SPL("[stream]   draw_mesh_strip n=%d v0=(%d,%d) v1=(%d,%d) vN=(%d,%d)", n,
            n > 0 ? v[0] : 0, n > 0 ? v[1] : 0, n > 1 ? v[2] : 0, n > 1 ? v[3] : 0,
            n > 0 ? v[2 * n - 2] : 0, n > 0 ? v[2 * n - 1] : 0);
    }
    rcDrawVertices(rhRc(this), coords, vertex_count);
}
void SwiffRenderHandler::draw_line_strip(const void* coords, int vertex_count)
{
    if (streamLogEnabled())   // [TEMP] stream tracer
    {
        ++s_streamCounts[3];
        const short* v = static_cast<const short*>(coords);
        const int n = vertex_count;
        SPL("[stream]   draw_line_strip n=%d v0=(%d,%d) vN=(%d,%d)", n,
            n > 0 ? v[0] : 0, n > 0 ? v[1] : 0, n > 0 ? v[2 * n - 2] : 0, n > 0 ? v[2 * n - 1] : 0);
    }
    rcDrawLineStrip(rhRc(this), coords, vertex_count);
}
void SwiffRenderHandler::fill_style_disable(int fill_side)
{ SPL("[stream]   fill_style_disable side=%d", fill_side); rcFillStyleDisable(rhRc(this), fill_side); }
void SwiffRenderHandler::fill_style_color(int fill_side, gameswf::rgba color)
{
    if (streamLogEnabled())   // [TEMP] stream tracer
    {
        ++s_streamCounts[4];
        SPL("[stream]   fill_style_color side=%d rgba=%08X", fill_side, rgbaDword(color));
    }
    rcFillStyleColor(rhRc(this), fill_side, rgbaDword(color));
}
void SwiffRenderHandler::fill_style_bitmap(int fill_side, const gameswf::bitmap_info* bi,
                                           const gameswf::matrix& m, bitmap_wrap_mode wm)
{
    const int texId = bi ? static_cast<int>(bi->m_texture_id) : 0;
    const int w     = bi ? bi->m_original_width  : 0;
    const int h     = bi ? bi->m_original_height : 0;
    const float* mf = reinterpret_cast<const float*>(&m);
    if (streamLogEnabled())   // [TEMP] stream tracer
    {
        ++s_streamCounts[5];
        SPL("[stream]   fill_style_bitmap side=%d id=%d %dx%d wrap=%d m=[%.5f %.5f %.1f | %.5f %.5f %.1f]",
            fill_side, texId, w, h, (int)wm, mf[0], mf[1], mf[2], mf[3], mf[4], mf[5]);
    }
    rcSetTexture(rhRc(this), fill_side, texId, w, h, mf, static_cast<int>(wm));
}
void SwiffRenderHandler::line_style_disable()
{ SPL("[stream]   line_style_disable"); rcLineStyleDisable(rhRc(this)); }
void SwiffRenderHandler::line_style_color(gameswf::rgba color)
{ SPL("[stream]   line_style_color rgba=%08X", rgbaDword(color)); rcLineStyleColor(rhRc(this), rgbaDword(color)); }
void SwiffRenderHandler::line_style_width(float width)
{ SPL("[stream]   line_style_width %.2f", width); rcLineStyleWidth(rhRc(this), floatBits(width)); }
void SwiffRenderHandler::draw_bitmap(const gameswf::matrix& m, const gameswf::bitmap_info* bi,
                                     const gameswf::rect& coords, const gameswf::rect& uv_coords,
                                     gameswf::rgba color)
{
    // Shipped forwarder targets SwiffRender +0x64, which is a nullsub - the menu uses the font-wrapper
    // path (Stage 5) for textured glyphs, not draw_bitmap. Forwarded for fidelity; lands on the no-op.
    const int texId = bi ? static_cast<int>(bi->m_texture_id) : 0;
    SPL("[stream]   draw_bitmap id=%d (nullsub)", texId);   // [TEMP] stream tracer
    rcDrawBitmapNull(rhRc(this), &m, texId, &coords, &uv_coords, rgbaDword(color));
}
void SwiffRenderHandler::set_antialiased(bool)
{
    // [INFERRED no-op] SwiffRender has no antialiased slot; the 2006 render_handler carries this where
    // the 2004 base did not. Antialiasing is a render hint the menu can ignore. See GAMESWF_DIVERGENCES.md.
}
void SwiffRenderHandler::begin_submit_mask()
{
    if (streamLogEnabled()) { ++s_streamCounts[6]; SPL("[stream]   begin_submit_mask"); }   // [TEMP] stream tracer
    rcBeginMask(rhRc(this));
}
void SwiffRenderHandler::end_submit_mask()
{ SPL("[stream]   end_submit_mask"); rcEndMask(rhRc(this)); }
void SwiffRenderHandler::disable_mask()
{ SPL("[stream]   disable_mask"); rcDisableMask(rhRc(this)); }

// [DICE-MOD SwiffPlayer #36] The twelve text slots. Each shipped forwarder is `mov ecx,[ecx+8]; mov
// eax,[ecx]; jmp [eax+off]` except two that convert an out-parameter (0x10005E20 hit test: the
// renderer writes the position into a stack copy of the last argument, the forwarder stores it through
// the pointer; 0x10005E50 selection: two locals, two stores).
void* SwiffRenderHandler::text_create(const char* fontName, int heightPx)
{
    void* line = rcTextCreate(rhRc(this), fontName ? fontName : "", heightPx);
    SPLA("[stream]   text_create(\"%s\", %d) -> %p", fontName ? fontName : "", heightPx, line);
    return line;
}
void SwiffRenderHandler::text_release(void* line)   { SPL("[stream]   text_release(%p)", line); rcTextRelease(rhRc(this), line); }
void SwiffRenderHandler::text_set(void* line, const wchar_t* text)
{
    if (streamLogEnabled())      // [TEMP] RW-89: logged outside the verbose window too - the fields are
    {                            // filled during loadMovie, before the first display

        char narrow[96]; int n = 0;
        for (const wchar_t* p = text; p && *p && n < 95; ++p) narrow[n++] = (*p < 128) ? (char) *p : '?';
        narrow[n] = 0;
        SPLA("[stream]   text_set(%p, \"%s\")", line, narrow);
    }
    rcTextSet(rhRc(this), line, text);
}
void SwiffRenderHandler::text_detach(void* line)    { SPL("[stream]   text_detach(%p)", line); rcTextDetach(rhRc(this), line); }
void SwiffRenderHandler::text_draw(void* line, const gameswf::matrix* m, unsigned color, int flags)
{
    if (streamLogEnabled()) ++s_streamCounts[0];
    SPL("[stream]   text_draw(%p, [%.4f %.4f %.1f | %.4f %.4f %.1f], %08X, %d)", line,
        m->m_[0][0], m->m_[0][1], m->m_[0][2], m->m_[1][0], m->m_[1][1], m->m_[1][2], color, flags);
    rcTextDraw(rhRc(this), line, m, color, flags);
}
bool SwiffRenderHandler::text_extent(void* line, int count, float* w, float* h)
{
    const bool ok = rcTextExtent(rhRc(this), line, count, w, h) != 0;
    SPLA("[stream]   text_extent(%p, %d) -> %d w=%.1f h=%.1f", line, count, ok ? 1 : 0, w ? *w : -1.f, h ? *h : -1.f);   // [TEMP] RW-89
    return ok;
}
bool SwiffRenderHandler::text_hit(void* line, int* outPos, int x, int y)
{
    int pos = y;                                         // the shipped forwarder's stack copy of the last argument
    const bool hit = rcTextHit(rhRc(this), line, &pos, x, y) != 0;
    if (outPos) *outPos = pos;
    return hit;
}
bool SwiffRenderHandler::text_selection(void* line, int* a, int* b)
{
    int va = (int) (intptr_t) line, vb = 0;             // the forwarder reuses its first argument slot for the first out value
    const bool has = rcTextSelection(rhRc(this), line, &va, &vb) != 0;
    if (a) *a = va;
    if (b) *b = vb;
    // The render context's AL is the answer the field's delete_selection (sub_10088D60) tests
    // before it deletes [start, end): with "no selection" the outs are meaningless. [DICE-MOD #46]
    return has;
}
bool SwiffRenderHandler::text_set_selection(void* line, int a, int b, int c, int d)
{ return rcTextSetSelection(rhRc(this), line, a, b, c, d) != 0; }
void SwiffRenderHandler::text_blur(void* line)      { rcTextBlur(rhRc(this), line); }
void SwiffRenderHandler::text_set_size(void* line, float size, int flag)
{
    SPLA("[stream]   text_set_size(%p, %.1f, %d)", line, size, flag);   // [TEMP] RW-89
    rcTextSetSize(rhRc(this), line, size, flag);
}
void SwiffRenderHandler::text_caret(void* line, int pos) { rcTextCaret(rhRc(this), line, pos); }


// RefFactory_ui_SwiffPlayer 0x10007F50. __fastcall (the /Gr convention BF2's createInstance uses).

IBase* __fastcall RefFactory_ui_SwiffPlayer(unsigned int iid, IBase* parent)
{
    if (parent)
        return 0;                                     // aggregation not supported

    void* mem = ::renddx9abi::engineAlloc(sizeof(SwiffPlayer));   // sub_1006DB60(1120) = bf_malloc
    if (!mem)
        return 0;   // the shipped factory derefs a failed alloc; we refuse instead (see BfMemory.cpp)

    SwiffPlayer* p = new (mem) SwiffPlayer();
    void* result = p->queryInterface(iid);            // raw this for iid 1 / 870001, else null
    if (!result)
        p->release();
    return reinterpret_cast<IBase*>(result);
}

} // namespace ui
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_SWIFFPLAYER
