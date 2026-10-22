// [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: RW-83]
// SwiffPlayer DLL harness covering registration, factory creation, references, movie loading, frames, and unload.
// Compares strings with MSVCP71 and records render calls through observers.
// endOfRound.swf selects extra settings, loading, map/mode, and server-browser fixtures.
// Shared typedefs cannot independently validate calling conventions; captured streams support regression comparison, not retail pixel parity.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <new>          // placement new: the host slots construct the returned std::string

#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/Vc71Vector.h"
#include "../../RendDX9Abi/Vc71String.h"
#include "../../RendDX9Abi/BfMemory.h"

using namespace dice::hfe;

static const char* kGameDir  = "D:\\B2BF Launcher\\Battlefield2";
static const char* kMenuDir  = "D:\\B2BF Launcher\\Battlefield2\\mods\\bf2\\menu";
static int g_failures = 0;

static void check(bool ok, const char* what)
{
    printf("  [%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++g_failures;
}


// The ISwiffRender call stream. Every fake render slot appends one line; test 5 diffs two runs.

static char  g_stream[128 * 1024];
static int   g_streamLen  = 0;
static bool  g_streamOn    = false;
static int   g_nextHandle  = 1000;
static int   g_callCounts[8];   // [0]beginDisplay [1]endDisplay [2]drawVertices [3]updateTime
                                //  [4]setMouseState [5]advanceSeen(unused) [6]setBg [7]masks

static void streamf(const char* fmt, ...)
{
    if (!g_streamOn) return;
    char line[256];
    va_list a; va_start(a, fmt);
    _vsnprintf_s(line, sizeof(line), _TRUNCATE, fmt, a);
    va_end(a);
    int n = (int)strlen(line);
    if (g_streamLen + n + 1 < (int)sizeof(g_stream))
    {
        memcpy(g_stream + g_streamLen, line, n);
        g_streamLen += n;
        g_stream[g_streamLen++] = '\n';
    }
}

// Renderer observer with all 67 virtual slots; used slots record calls and unused slots abort.
// __fastcall thunks accept self/edx before the exact arguments expected by __thiscall callers.
static int __fastcall fr_addRef(void*, void*)                { return 1; }
static int __fastcall fr_getRef(void*, void*)                { return 1; }
static int __fastcall fr_release(void*, void*)               { return 1; }
static void* __fastcall fr_queryInterface(void* s, void*, unsigned) { return s; }
static int __fastcall fr_init(void*, void*)                  { streamf("init"); return 1; }
static int __fastcall fr_shutdown(void*, void*)              { return 1; }

static int __fastcall fr_createEmpty(void*, void*)                                      { int h = g_nextHandle++; streamf("createBitmapEmpty -> h%d", h); return h; }
static int __fastcall fr_uploadAlpha(void*, void*, unsigned w, unsigned h, const void*) { int t = g_nextHandle++; streamf("uploadAlpha(w=%u h=%u) -> h%d", w, h, t); return t; }
static int __fastcall fr_createRgb(void*, void*, void*)                                 { int h = g_nextHandle++; streamf("createBitmapRgb -> h%d", h); return h; }
static int __fastcall fr_uploadRgba(void*, void*, void*)                                { int h = g_nextHandle++; streamf("uploadRgba -> h%d", h); return h; }
static int __fastcall fr_loadTexture(void*, void*, const char* n, int* w, int* h)       { int t = g_nextHandle++; if (w) *w = 0; if (h) *h = 0; streamf("loadTexture(\"%s\") -> h%d", n ? n : "", t); return t; }
static void __fastcall fr_releaseTexture(void*, void*, int id)                          { streamf("releaseTexture(h%d)", id); }
static void __fastcall fr_beginDisplay(void*, void*, int bg, int vx, int vy, int vw, int vh, float x0, float x1, float y0, float y1)
{ ++g_callCounts[0]; streamf("beginDisplay(vp=%d,%d,%d,%d bounds=%.0f,%.0f,%.0f,%.0f)", vx, vy, vw, vh, x0, x1, y0, y1); (void)bg; }
static void __fastcall fr_endDisplay(void*, void*)           { ++g_callCounts[1]; streamf("endDisplay"); }
static void __fastcall fr_setTransform(void*, void*, const float* m) { streamf("setTransform(%.3f,%.3f,%.3f, %.3f,%.3f,%.3f)", m[0], m[1], m[2], m[3], m[4], m[5]); }
static void __fastcall fr_setCxform(void*, void*, const float*)      { streamf("setCxform"); }
static void __fastcall fr_recordStat(void*, void*)          { streamf("recordStat(UNEXPECTED)"); }
static void __fastcall fr_drawVertices(void*, void*, const void*, int n)  { ++g_callCounts[2]; streamf("drawVertices(n=%d)", n); }
static void __fastcall fr_drawLineStrip(void*, void*, const void*, int n) { streamf("drawLineStrip(n=%d)", n); }
static void __fastcall fr_fillStyleDisable(void*, void*, int s) { streamf("fillStyleDisable(side=%d)", s); }
static void __fastcall fr_fillStyleColor(void*, void*, int s, unsigned rgba) { streamf("fillStyleColor(side=%d rgba=%08X)", s, rgba); }
static void __fastcall fr_setTexture(void*, void*, int s, int t, int w, int h, const float*, int wm) { streamf("setTexture(side=%d h%d %dx%d wrap=%d)", s, t, w, h, wm); }
static void __fastcall fr_lineStyleDisable(void*, void*)    { streamf("lineStyleDisable"); }
static void __fastcall fr_lineStyleColor(void*, void*, unsigned rgba) { streamf("lineStyleColor(rgba=%08X)", rgba); }
static void __fastcall fr_lineStyleWidth(void*, void*, unsigned w) { streamf("lineStyleWidth(bits=%08X)", w); }
static void __fastcall fr_drawBitmapNull(void*, void*, const void*, int t, const void*, const void*, unsigned) { streamf("drawBitmap(h%d) [nullsub]", t); }
static void __fastcall fr_beginMask(void*, void*)          { ++g_callCounts[7]; streamf("beginMask"); }
static void __fastcall fr_endMask(void*, void*)            { streamf("endMask"); }
static void __fastcall fr_disableMask(void*, void*)        { streamf("disableMask"); }
static void __fastcall fr_setMovieWidth(void*, void*, unsigned w)  { streamf("setMovieWidth(%u)", w); }
static void __fastcall fr_setMovieHeight(void*, void*, int h)      { streamf("setMovieHeight(%d)", h); }
static void __fastcall fr_setScale(void*, void*, float x, float y) { streamf("setScale(%.3f,%.3f)", x, y); }
static void __fastcall fr_setBackgroundColor(void*, void*, unsigned rgb) { ++g_callCounts[6]; streamf("setBackgroundColor(%08X)", rgb); }
static void __fastcall fr_setMouseState(void*, void*, int x, int y, int b) { ++g_callCounts[4]; streamf("setMouseState(%d,%d,%d)", x, y, b); }
static char __fastcall fr_capability0(void*, void*, int a) { streamf("capability0(%d)", a); return 0; }
static char __fastcall fr_capability1(void*, void*, int a) { streamf("capability1(%d)", a); return 0; }
static void __fastcall fr_setPairState(void*, void*, int a, int b) { streamf("setPairState(%d,%d)", a, b); }
static void __fastcall fr_updateTime(void*, void*, unsigned t, float dt) { ++g_callCounts[3]; streamf("updateTime(t=%08X dt=%.4f)", t, dt); }
static int  __fastcall fr_unexpected(void*, void*)         { streamf("*** UNEXPECTED SwiffRender slot called"); ++g_failures; return -1; }
// The text ("font wrapper") slots 26-37 the dice text field drives. A fake line is a counter handle;
// the extent answers 8 px per character so word wrap and auto-size take real branches.
static int   g_textLines = 0;
static void* __fastcall fr_textCreate(void*, void*, const char* name, int size) { ++g_textLines; streamf("textCreate(\"%s\", %d) -> L%d", name ? name : "", size, g_textLines); return (void*)(intptr_t)g_textLines; }
static void  __fastcall fr_textRelease(void*, void*, void* l)               { streamf("textRelease(L%d)", (int)(intptr_t)l); }
static void  __fastcall fr_textSet(void*, void*, void* l, const wchar_t* t) { int n = 0; while (t && t[n]) ++n; streamf("textSet(L%d, %d chars)", (int)(intptr_t)l, n); }
static void  __fastcall fr_textDetach(void*, void*, void* l)                { streamf("textDetach(L%d)", (int)(intptr_t)l); }
static void  __fastcall fr_textDraw(void*, void*, void* l, const float* m, unsigned c, int f) { streamf("textDraw(L%d, [%.2f %.2f %.0f | %.2f %.2f %.0f], %08X, %d)", (int)(intptr_t)l, m[0], m[1], m[2], m[3], m[4], m[5], c, f); }
static char  __fastcall fr_textExtent(void*, void*, void* l, int n, float* w, float* h) { if (w) *w = 8.0f * n; if (h) *h = 12.0f; streamf("textExtent(L%d, %d)", (int)(intptr_t)l, n); return 1; }
static char  __fastcall fr_textHit(void*, void*, void* l, int* pos, int x, int y) { (void)l; (void)x; (void)y; if (pos) *pos = 0; return 0; }
static void  __fastcall fr_textSelection(void*, void*, void* l, int* a, int* b) { (void)l; if (a) *a = 0; if (b) *b = 0; }
static char  __fastcall fr_textSetSelection(void*, void*, void* l, int, int, int, int) { (void)l; return 0; }
static void  __fastcall fr_textBlur(void*, void*, void* l)                  { (void)l; }
static void  __fastcall fr_textSetSize(void*, void*, void* l, float s, int f) { streamf("textSetSize(L%d, %.1f, %d)", (int)(intptr_t)l, s, f); }
static void  __fastcall fr_textCaret(void*, void*, void* l, int p)          { (void)l; (void)p; }

static void* g_frVtbl[67];
static void  buildFakeRenderVtbl()
{
    for (int i = 0; i < 67; ++i) g_frVtbl[i] = (void*)&fr_unexpected;
    g_frVtbl[0]  = (void*)&fr_addRef;          g_frVtbl[1]  = (void*)&fr_getRef;
    g_frVtbl[2]  = (void*)&fr_release;         g_frVtbl[3]  = (void*)&fr_queryInterface;
    g_frVtbl[4]  = (void*)&fr_init;            g_frVtbl[5]  = (void*)&fr_shutdown;
    g_frVtbl[6]  = (void*)&fr_createEmpty;     g_frVtbl[7]  = (void*)&fr_uploadAlpha;   // +0x18 +0x1C
    g_frVtbl[8]  = (void*)&fr_createRgb;       g_frVtbl[9]  = (void*)&fr_uploadRgba;    // +0x20 +0x24
    g_frVtbl[10] = (void*)&fr_loadTexture;     g_frVtbl[11] = (void*)&fr_releaseTexture;// +0x28 +0x2C
    g_frVtbl[12] = (void*)&fr_beginDisplay;    g_frVtbl[13] = (void*)&fr_endDisplay;    // +0x30 +0x34
    g_frVtbl[14] = (void*)&fr_setTransform;    g_frVtbl[15] = (void*)&fr_setCxform;     // +0x38 +0x3C
    g_frVtbl[16] = (void*)&fr_recordStat;                                              // +0x40
    g_frVtbl[17] = (void*)&fr_drawVertices;    g_frVtbl[18] = (void*)&fr_drawLineStrip; // +0x44 +0x48
    g_frVtbl[19] = (void*)&fr_fillStyleDisable;g_frVtbl[20] = (void*)&fr_fillStyleColor;// +0x4C +0x50
    g_frVtbl[21] = (void*)&fr_setTexture;                                              // +0x54
    g_frVtbl[22] = (void*)&fr_lineStyleDisable;g_frVtbl[23] = (void*)&fr_lineStyleColor;// +0x58 +0x5C
    g_frVtbl[24] = (void*)&fr_lineStyleWidth;  g_frVtbl[25] = (void*)&fr_drawBitmapNull;// +0x60 +0x64
    g_frVtbl[39] = (void*)&fr_beginMask;       g_frVtbl[40] = (void*)&fr_endMask;       // +0x9C +0xA0
    g_frVtbl[41] = (void*)&fr_disableMask;                                             // +0xA4
    g_frVtbl[26] = (void*)&fr_textCreate;      g_frVtbl[27] = (void*)&fr_textRelease;   // +0x68 +0x6C
    g_frVtbl[28] = (void*)&fr_textSet;         g_frVtbl[29] = (void*)&fr_textDetach;    // +0x70 +0x74
    g_frVtbl[30] = (void*)&fr_textDraw;        g_frVtbl[31] = (void*)&fr_textExtent;    // +0x78 +0x7C
    g_frVtbl[32] = (void*)&fr_textHit;         g_frVtbl[33] = (void*)&fr_textSelection; // +0x80 +0x84
    g_frVtbl[34] = (void*)&fr_textSetSelection;g_frVtbl[35] = (void*)&fr_textBlur;      // +0x88 +0x8C
    g_frVtbl[36] = (void*)&fr_textSetSize;     g_frVtbl[37] = (void*)&fr_textCaret;     // +0x90 +0x94
    g_frVtbl[42] = (void*)&fr_setMovieWidth;   g_frVtbl[43] = (void*)&fr_setMovieHeight;// +0xA8 +0xAC
    g_frVtbl[44] = (void*)&fr_setScale;        g_frVtbl[45] = (void*)&fr_setBackgroundColor; // +0xB0 +0xB4
    g_frVtbl[46] = (void*)&fr_setMouseState;                                           // +0xB8
    g_frVtbl[49] = (void*)&fr_capability0;     g_frVtbl[50] = (void*)&fr_capability1;   // +0xC4 +0xC8
    g_frVtbl[53] = (void*)&fr_setPairState;    g_frVtbl[54] = (void*)&fr_updateTime;    // +0xD4 +0xD8
}
struct FakeRender { void* vtbl; };


// FakeSwiffHost - the consumed slots only (+0x10, +0x20, +0x50, +0x1DC getViewportSize, +0x244, +0x2B0).
// getViewportSize hands back a fixed 1024x768; the rest log. Raw vtable, sized to the largest offset.

static void __fastcall fh_generic(void*, void*)            { /* other host slots - unused here */ }
static void __fastcall fh_preLoad10(void*, void*)          { streamf("host.preLoad(+0x10)"); }
static void __fastcall fh_update(void*, void*)             { streamf("host.update(+0x20)"); }
static void __fastcall fh_preLoad50(void*, void*)          { streamf("host.preLoad(+0x50)"); }
static void __fastcall fh_getViewportSize(void*, void*, unsigned* w, unsigned* h) { if (w) *w = 1024; if (h) *h = 768; }
static void __fastcall fh_setEnabled(void*, void*, int)    { }

// [TEMP]
// Host callbacks keep exact callee-cleaned argument lists to preserve the x86 stack.
// An in-memory store makes string get/set round trips observable.
struct KV { char k[128]; char v[256]; };
static KV   g_kv[64];
static int  g_kvCount = 0;

static void __fastcall fh_isGameRunning(void*, void*, unsigned char* out) { if (out) *out = 0; }
static int  __fastcall fh_getActivateReason(void*, void*)                { return 0; }
static void __fastcall fh_setActivateReason(void*, void*, int)           { }
// [TEMP] BF2_SWIFFPLAYER_FRAMELABEL=<label> makes the fake host answer getFrameLabel with a real
// frame label (the retail host's answer navigates the root movie; an empty label leaves it on
// frame 0), so the login-frame paths - registered components, loadImg, the drawing API - run in
// the harness under its SEH handler. Empty by default, as before.
static void __fastcall fh_getFrameLabel(void*, void*, void* out)
{
    const char* label = getenv("BF2_SWIFFPLAYER_FRAMELABEL");
    new (out) vc71::string(label ? label : "");
}
// Slot +416 assigns an already constructed string; +196 constructs into raw storage.
// Keeping that distinction catches callers that pass the wrong output storage.
static void __fastcall fh_getModVersion(void*, void*, vc71::string* out)  { if (out) *out = "1.50"; }
// +168 Locale.getString(std::wstring& out, const std::string& id): pre-constructed, the host assigns.
// The fake answers with the id itself in brackets, widened, so a localised label is recognisable in
// a log and a long id exercises the heap case of the wide string.
static int   __fastcall fh_request(void*, void*, void* req)            { (void)req; return 0; }   // leaves the vector empty
static void* __fastcall fh_getCursor(void*, void*, int)                { return 0; }
static void  __fastcall fh_cursorRestored(void*, void*)                { }
static void __fastcall fh_localeGetString(void*, void*, vc71::wstring* out, const vc71::string* id)
{
    if (!out) return;
    wchar_t w[256]; unsigned n = 0;
    w[n++] = L'[';
    for (const char* p = id ? id->c_str() : ""; *p && n < 254; ++p) w[n++] = (wchar_t)(unsigned char)*p;
    w[n++] = L']'; w[n] = 0;
    *out = w;
}
static int  __fastcall fh_getCommandLine(void*, void*)                   { return 0; }

// Returns the stored string for a key, or the supplied default when absent.
static void __fastcall fh_getStorageString(void*, void*, void* out,
                                           const vc71::string* a, const vc71::string* b)
{
    const char* key = a ? a->c_str() : "";
    for (int i = 0; i < g_kvCount; ++i)
        if (strcmp(g_kv[i].k, key) == 0) { new (out) vc71::string(g_kv[i].v); return; }
    new (out) vc71::string(b ? b->c_str() : "");
}
static void __fastcall fh_setStorageString(void*, void*, const vc71::string* a, const vc71::string* b)
{
    if (!a || !b) return;
    // The shipped setter takes (arg0, arg1); the harness stores it verbatim so a later
    // getStorageString with the same first key can observe it.
    for (int i = 0; i < g_kvCount; ++i)
        if (strcmp(g_kv[i].k, a->c_str()) == 0)
        { strncpy_s(g_kv[i].v, b->c_str(), _TRUNCATE); return; }
    if (g_kvCount < 64)
    {
        strncpy_s(g_kv[g_kvCount].k, a->c_str(), _TRUNCATE);
        strncpy_s(g_kv[g_kvCount].v, b->c_str(), _TRUNCATE);
        ++g_kvCount;
    }
}

// scaffolding: the smallest retail-layout map graph needed by stock endOfRound.swf's first-load
// briefing. These are [TEMP] [CHOSEN, not recovered] records: their offsets and element strides are
// the recovered VC7.1 ABI consumed by the replacement DLL, while their Kubra Dam text is deterministic
// fixture data. They never enter a production image. [RETIRE-WHEN: RW-83]
struct EorMapTypeFixture
{
    int players;                         // +0x00
    char pad04[28];                      // +0x04
    vc71::string locId;                  // +0x20
    vc71::string description;            // +0x3C
    char tail58[4];                      // +0x58
};
struct EorGameModeFixture
{
    vc71::string name;                   // +0x00
    vc71::vector_layout mapTypes;        // +0x1C; first/last at +0x20/+0x24
};
struct EorMapFixture
{
    vc71::string path;                   // +0x00
    vc71::string name;                   // +0x1C
    vc71::string briefing;               // +0x38
    vc71::string briefingLocId;          // +0x54
    char pad70[28];                      // +0x70
    vc71::vector_layout gameModes;       // +0x8C; first/last at +0x90/+0x94
};
static_assert(sizeof(EorMapTypeFixture) == 92, "retail map-type stride must be 92");
static_assert(sizeof(EorGameModeFixture) == 44, "retail game-mode stride must be 44");
static_assert(offsetof(EorMapFixture, name) == 28, "retail map name offset");
static_assert(offsetof(EorMapFixture, briefing) == 56, "retail map briefing offset");
static_assert(offsetof(EorMapFixture, briefingLocId) == 84, "retail map briefing loc-id offset");
static_assert(offsetof(EorMapFixture, gameModes) == 0x8C, "retail map game-mode vector offset");

struct RawFixtureObject { void* vtbl; };
static RawFixtureObject g_eorModInfo;
static RawFixtureObject g_eorServerList;
static void* g_eorModInfoVtbl[24];
static void* g_eorServerListVtbl[24];
static EorMapFixture* g_eorMap = 0;
static bool g_eorIsLoading = true;
static int g_eorAutoReadyReads = 0;
static int g_eorLoadProgressReads = 0;
static int g_eorCurrentMapReads = 0;
static int g_eorMapFinds = 0;
static int g_eorLocalServerFills = 0;
// eorsinfo's nine live ServerInfo slots, in vtable order +0x380..+0x3A0. The mixed zero/nonzero
// fixture values below make the harness observe both shipped paths: zero is read once before the
// browser-record fallback, while a nonzero value is read a second time for publication.
static int g_eorServerInfoReads[9] = { 0 };
static int g_eorHudPublications = 0;
static char g_eorHudName[64] = { 0 };
static char g_eorHudAddress[64] = { 0 };
static int g_eorHudPort = 0;

static bool eorFixtureScenario()
{
    const char* movie = getenv("BF2_SWIFFPLAYER_MOVIE");
    if (!movie) return false;
    const char* base = movie;
    for (const char* p = movie; *p; ++p) if (*p == '/' || *p == '\\') base = p + 1;
    return _stricmp(base, "endOfRound.swf") == 0;
}

static void initEorFixtureData()
{
    static EorMapTypeFixture mapType;
    static EorGameModeFixture gameMode;
    static EorMapFixture map;
    static bool initialized = false;
    if (!initialized)
    {
        mapType.players = 64;
        mapType.locId = "HUD_MAPTYPE_GPM_CQ_64";
        mapType.description = "64-player Conquest";

        gameMode.name = "gpm_cq";
        gameMode.mapTypes.alloc = 0;
        gameMode.mapTypes.first = reinterpret_cast<char*>(&mapType);
        gameMode.mapTypes.last = gameMode.mapTypes.first + sizeof(mapType);
        gameMode.mapTypes.end = gameMode.mapTypes.last;

        map.path = "kubra_dam";
        map.name = "Kubra Dam";
        map.briefing = "Capture and hold control points.";
        map.briefingLocId = "HUD_MAPDESC_KUBRA_DAM";
        map.gameModes.alloc = 0;
        map.gameModes.first = reinterpret_cast<char*>(&gameMode);
        map.gameModes.last = map.gameModes.first + sizeof(gameMode);
        map.gameModes.end = map.gameModes.last;
        initialized = true;
    }
    g_eorMap = &map;
    g_eorIsLoading = true;
    g_eorAutoReadyReads = g_eorLoadProgressReads = g_eorCurrentMapReads = 0;
    g_eorMapFinds = g_eorLocalServerFills = 0;
    memset(g_eorServerInfoReads, 0, sizeof(g_eorServerInfoReads));
    g_eorHudPublications = 0;
    g_eorHudName[0] = g_eorHudAddress[0] = 0;
    g_eorHudPort = 0;
}

static void* __fastcall eorMod_findMap(void*, void*, const vc71::string* path)
{
    ++g_eorMapFinds;
    return path && g_eorMap && _stricmp(path->c_str(), g_eorMap->path.c_str()) == 0 ? g_eorMap : 0;
}
static void* __fastcall eorMod_getGameModeName(void*, void*, void* out, const vc71::string* mode)
{
    const char* name = mode && _stricmp(mode->c_str(), "gpm_cq") == 0 ? "Conquest" : "Unknown Mode";
    return new (out) vc71::string(name);
}

// Exact retail SwiffHost call shapes used by Client, Logic, EndOfRound, Mod, VideoSettings and
// ServerInfo on the first-load route. Fixed values are fixture policy, not recovered engine state.
static void* __fastcall fh_getCurrentMapName(void*, void*, void* out)
{ ++g_eorCurrentMapReads; return new (out) vc71::string("kubra_dam"); }
static void* __fastcall fh_getCurrentMapGameMode(void*, void*, void* out)
{ return new (out) vc71::string("gpm_cq"); }
static int __fastcall fh_getCurrentMapMaxPlayers(void*, void*) { return 64; }
static int __fastcall fh_getCurrentMapSize(void*, void*) { return 64; }
static float __fastcall fh_getLoadingProgress(void*, void*) { ++g_eorLoadProgressReads; return 1.0f; }
static int __fastcall fh_getGameStatus(void*, void*) { return 0; }
static float __fastcall fh_getTimeUntilRestart(void*, void*) { return 0.0f; }
static void* __fastcall fh_getCurrentLoadFile(void*, void*, void* out)
{ return new (out) vc71::string("Objects_client.zip"); }
static unsigned char __fastcall fh_getIsLoading(void*, void*) { return g_eorIsLoading ? 1 : 0; }
static void __fastcall fh_setIsLoading(void*, void*, char value) { g_eorIsLoading = value != 0; }
static unsigned char __fastcall fh_getGameStarting(void*, void*) { return 1; }
static void* __fastcall fh_getModInfo(void*, void*) { return &g_eorModInfo; }
static const vc71::string* __fastcall fh_getPreviousServerRecord(void*, void*)
{ static vc71::string previousMap(""); return &previousMap; }
static unsigned char __fastcall fh_isGameLogicServer(void*, void*) { return 1; }
static void* __fastcall fh_getServerList(void*, void*, const vc71::string*) { return &g_eorServerList; }
static const vc71::string* __fastcall fh_getJoinAddress(void*, void*)
{ static vc71::string address("0.0.0.0"); return &address; }
static int __fastcall fh_getJoinQueryPort(void*, void*) { return 0; }
static unsigned char __fastcall fh_getForcedLoading(void*, void*) { return 0; }
static const vc71::string* __fastcall fh_getWelcomeMessage(void*, void*)
{ static vc71::string welcome("Welcome to the BF2Rebuilt fixture|Have fun!"); return &welcome; }
static unsigned char __fastcall fh_isAbortLoadLevel(void*, void*) { return 0; }
static unsigned char __fastcall fh_isOptimizingShaders(void*, void*) { return 0; }
static int __fastcall fh_getRoundsLeft(void*, void*) { return 0; }
static unsigned char __fastcall fh_isLoadingDemo(void*, void*) { return 0; }
static void* __fastcall fh_getModName(void*, void*, void* out)
{ return new (out) vc71::string("bf2"); }
static unsigned char __fastcall eorServerList_fillLocalRecord(void*, void*, void* out)
{
    // The caller constructed its 580-byte RetailServerBrowserRecord before dispatch. Assign only
    // the briefing fields we want to exercise; its own destructor remains responsible for them.
    char* record = static_cast<char*>(out);
    ++g_eorLocalServerFills;
    *reinterpret_cast<vc71::string*>(record + 4) = "127.0.0.1";             // address
    *reinterpret_cast<int*>(record + 32) = 29900;                            // queryPort
    *reinterpret_cast<vc71::string*>(record + 68) = "BF2Rebuilt fixture";   // hostName
    *reinterpret_cast<vc71::string*>(record + 112) = "gpm_cq";             // gameMode
    *reinterpret_cast<int*>(record + 276) = 16677;                           // hostPort; never bind
    record[396] = 1;                                                         // friendlyFire fallback
    *reinterpret_cast<float*>(record + 432) = 15.7f;                         // spawnTime fallback
    *reinterpret_cast<int*>(record + 436) = 1200;                            // timeLimit fallback
    *reinterpret_cast<int*>(record + 440) = 3;                               // roundsPerMap fallback
    *reinterpret_cast<vc71::string*>(record + 500) = "";                    // communityLogoUrl
    record[528] = 1;                                                         // autoBalanced fallback
    *reinterpret_cast<float*>(record + 532) = 1.5f;                          // teamRatio fallback
    *reinterpret_cast<int*>(record + 536) = 100;                             // ticketRatio fallback
    record[544] = 0;                                                         // globalUnlocks fallback
    record[576] = 1;                                                         // noVehicles fallback
    return 1;
}

static unsigned char __fastcall fh_getServerInfoNoVehicles(void*, void*)
{ ++g_eorServerInfoReads[0]; return 1; }
static unsigned char __fastcall fh_getServerInfoFriendlyFire(void*, void*)
{ ++g_eorServerInfoReads[1]; return 1; }
static int __fastcall fh_getServerInfoRoundsPerMap(void*, void*)
{ ++g_eorServerInfoReads[2]; return 0; }
static int __fastcall fh_getServerInfoSpawnTime(void*, void*)
{ ++g_eorServerInfoReads[3]; return 18; }
static int __fastcall fh_getServerInfoTimeLimit(void*, void*)
{ ++g_eorServerInfoReads[4]; return 0; }
static int __fastcall fh_getServerInfoTicketRatio(void*, void*)
{ ++g_eorServerInfoReads[5]; return 125; }
static int __fastcall fh_getServerInfoTeamRatio(void*, void*)
{ ++g_eorServerInfoReads[6]; return 0; }
static unsigned char __fastcall fh_getServerInfoAutoBalance(void*, void*)
{ ++g_eorServerInfoReads[7]; return 1; }
static unsigned char __fastcall fh_getServerInfoGlobalUnlocks(void*, void*)
{ ++g_eorServerInfoReads[8]; return 0; }

// scaffolding: exact retail +0x8C by-value VC7.1 string call used by eorsinfo to publish the
// fetched server identity. This object is exposed only in the endOfRound fixture.
struct FakeEorHudInformationLayer { void* vtbl; };
static void* g_eorHudVtbl[40];
static void __fastcall eorHud_setServerInfo(FakeEorHudInformationLayer*, void*,
                                            vc71::string name, vc71::string address, int port)
{
    ++g_eorHudPublications;
    strncpy_s(g_eorHudName, name.c_str(), _TRUNCATE);
    strncpy_s(g_eorHudAddress, address.c_str(), _TRUNCATE);
    g_eorHudPort = port;
}
static void buildFakeEorHudVtbl()
{
    for (int i = 0; i < (int)(sizeof(g_eorHudVtbl) / sizeof(g_eorHudVtbl[0])); ++i)
        g_eorHudVtbl[i] = (void*)&fh_generic;
    g_eorHudVtbl[0x8C/4] = (void*)&eorHud_setServerInfo;
}

// scaffolding: raw GeneralSettings owner for the one stock first-load read. The shipped AS callback
// calls +0xF8 unguarded, so the harness must publish an object with that exact byte slot. The paired
// apply/set slots are modelled too in case the movie toggles auto-ready. [TEMP] [CHOSEN, not recovered]
// [RETIRE-WHEN: RW-83]
struct FakeGeneralSettings { void* vtbl; };
static void* g_generalSettingsVtbl[64];
static bool g_fixtureAutoReady = false;
static void __fastcall gs_generic(void*, void*) { }
static unsigned char __fastcall gs_apply(void*, void*) { return 1; }
static unsigned char __fastcall gs_getAutoReady(void*, void*)
{ ++g_eorAutoReadyReads; return g_fixtureAutoReady ? 1 : 0; }
static void __fastcall gs_setAutoReady(void*, void*, char value) { g_fixtureAutoReady = value != 0; }
static void buildFakeGeneralSettingsVtbl()
{
    for (int i = 0; i < 64; ++i) g_generalSettingsVtbl[i] = (void*)&gs_generic;
    g_generalSettingsVtbl[0x18/4] = (void*)&gs_apply;
    g_generalSettingsVtbl[0xF8/4] = (void*)&gs_getAutoReady;
    g_generalSettingsVtbl[0xFC/4] = (void*)&gs_setAutoReady;
    g_fixtureAutoReady = false;
}

// scaffolding: the dice.MessageHandler slots. BF2_SWIFFPLAYER_HOST_MESSAGE=1 makes the host hold ONE
// type-1 (error) message - the "disconnected by another login" line retail drive6 showed - until the
// movie's OK button calls removeLast; off, the host holds none (deterministic 0 instead of whatever
// fh_generic left in eax). The record is {type, code, std::string text} as the SwiffPlayer reads it.
struct HostMessageRecord { int type; int code; vc71::string text; };
static bool hostMessageScenario() { const char* e = getenv("BF2_SWIFFPLAYER_HOST_MESSAGE"); return e && e[0] == '1'; }
static int  g_msgCount   = -1;
static int  g_msgRemoved = 0;
static int  __fastcall fh_getNumMessages(void*, void*)
{ if (g_msgCount < 0) g_msgCount = hostMessageScenario() ? 1 : 0; return g_msgCount; }
static void __fastcall fh_getLastMessage(void*, void*, HostMessageRecord* m, int)
{ if (!m) return; m->type = 1; m->code = 0; m->text = "This profile has been disconnected by another login."; streamf("host.getLastMessage -> type 1"); }
static void __fastcall fh_removeLast(void*, void*)      { if (g_msgCount > 0) --g_msgCount; ++g_msgRemoved; streamf("host.removeLast (%d left)", g_msgCount); }
static void __fastcall fh_clearMessages(void*, void*)   { g_msgCount = 0; streamf("host.clearMessages"); }
static void __fastcall fh_enableMessageHandler(void*, void*, int on) { streamf("host.enableMessageHandler(%d)", on); }

// ISwiffHost has 238 retail slots through +0x3B4. Keep the complete table so the ServerInfo block
// at +0x380..+0x3A0 is an in-bounds ABI surface in the endOfRound fixture.
static void* g_fhVtbl[238];
static void  buildFakeHostVtbl()
{
    for (int i = 0; i < (int)(sizeof(g_fhVtbl) / sizeof(g_fhVtbl[0])); ++i)
        g_fhVtbl[i] = (void*)&fh_generic;
    g_fhVtbl[0x10/4] = (void*)&fh_preLoad10;        // +0x10
    g_fhVtbl[0x20/4] = (void*)&fh_update;           // +0x20
    g_fhVtbl[0x50/4] = (void*)&fh_preLoad50;        // +0x50
    g_fhVtbl[0x1DC/4] = (void*)&fh_getViewportSize; // +0x1DC
    g_fhVtbl[0x2B0/4] = (void*)&fh_setEnabled;      // +0x2B0
    g_fhVtbl[184/4] = (void*)&fh_isGameRunning;     // +184  dice.bf2.Logic.isGameRunning
    g_fhVtbl[188/4] = (void*)&fh_getActivateReason; // +188  getActivateReason
    g_fhVtbl[192/4] = (void*)&fh_setActivateReason; // +192  (the consume-on-read clear)
    g_fhVtbl[196/4] = (void*)&fh_getFrameLabel;     // +196  getFrameLabel
    g_fhVtbl[416/4] = (void*)&fh_getModVersion;     // +416  getModVersion
    g_fhVtbl[168/4] = (void*)&fh_localeGetString;   // +168  Locale.getString (wide out-param)
    g_fhVtbl[44/4]  = (void*)&fh_request;           // +44   request {id, vector<string>} (Profile.logout = 13)
    g_fhVtbl[108/4] = (void*)&fh_getCursor;         // +108  getCursor(which) -> hcursor
    g_fhVtbl[692/4] = (void*)&fh_cursorRestored;    // +692  after the cursor reset
    g_fhVtbl[568/4] = (void*)&fh_getStorageString;  // +568  getStorageString
    g_fhVtbl[572/4] = (void*)&fh_setStorageString;  // +572  setStorageString
    g_fhVtbl[700/4] = (void*)&fh_getCommandLine;    // +700  getCommandLine
    g_fhVtbl[488/4] = (void*)&fh_getNumMessages;    // +488  MessageHandler.getNumMessages
    g_fhVtbl[516/4] = (void*)&fh_getLastMessage;    // +516  the last record (getLastType/Text/Code)
    g_fhVtbl[520/4] = (void*)&fh_removeLast;        // +520  removeLast
    g_fhVtbl[508/4] = (void*)&fh_clearMessages;     // +508  clear
    g_fhVtbl[524/4] = (void*)&fh_enableMessageHandler; // +524 enableMessageHandler(bool)

    // Keep the historical menu.swf/mainMenu.swf fake untouched. The richer owner graph is selected
    // only when BF2_SWIFFPLAYER_MOVIE names stock endOfRound.swf. [TEMP] fixture routing.
    if (eorFixtureScenario())
    {
        for (int i = 0; i < 24; ++i)
        {
            g_eorModInfoVtbl[i] = (void*)&fh_generic;
            g_eorServerListVtbl[i] = (void*)&fh_generic;
        }
        g_eorModInfo.vtbl = g_eorModInfoVtbl;
        g_eorServerList.vtbl = g_eorServerListVtbl;
        g_eorModInfoVtbl[0x24/4] = (void*)&eorMod_findMap;
        g_eorModInfoVtbl[0x4C/4] = (void*)&eorMod_getGameModeName;
        g_eorServerListVtbl[0x44/4] = (void*)&eorServerList_fillLocalRecord;
        g_fhVtbl[0x58/4] = (void*)&fh_getCurrentMapName;
        g_fhVtbl[0x5C/4] = (void*)&fh_getCurrentMapGameMode;
        g_fhVtbl[0x60/4] = (void*)&fh_getCurrentMapMaxPlayers;
        g_fhVtbl[0x64/4] = (void*)&fh_getCurrentMapSize;
        g_fhVtbl[0x7C/4] = (void*)&fh_getServerList;
        g_fhVtbl[0x94/4] = (void*)&fh_getLoadingProgress;
        g_fhVtbl[0x98/4] = (void*)&fh_getGameStatus;
        g_fhVtbl[0x9C/4] = (void*)&fh_getTimeUntilRestart;
        g_fhVtbl[0xA0/4] = (void*)&fh_getCurrentLoadFile;
        g_fhVtbl[0xCC/4] = (void*)&fh_getIsLoading;
        g_fhVtbl[0xD0/4] = (void*)&fh_setIsLoading;
        g_fhVtbl[0xD4/4] = (void*)&fh_getGameStarting;
        g_fhVtbl[0x120/4] = (void*)&fh_getModInfo;
        g_fhVtbl[0x178/4] = (void*)&fh_getPreviousServerRecord;
        g_fhVtbl[0x1A8/4] = (void*)&fh_isGameLogicServer;
        g_fhVtbl[0x258/4] = (void*)&fh_getJoinAddress;
        g_fhVtbl[0x260/4] = (void*)&fh_getJoinQueryPort;
        g_fhVtbl[0x2A8/4] = (void*)&fh_getForcedLoading;
        g_fhVtbl[0x2E0/4] = (void*)&fh_getWelcomeMessage;
        g_fhVtbl[0x2F0/4] = (void*)&fh_isAbortLoadLevel;
        g_fhVtbl[0x31C/4] = (void*)&fh_isOptimizingShaders;
        g_fhVtbl[0x358/4] = (void*)&fh_getRoundsLeft;
        g_fhVtbl[0x360/4] = (void*)&fh_isLoadingDemo;
        g_fhVtbl[0x364/4] = (void*)&fh_getModName;
        g_fhVtbl[0x380/4] = (void*)&fh_getServerInfoNoVehicles;
        g_fhVtbl[0x384/4] = (void*)&fh_getServerInfoFriendlyFire;
        g_fhVtbl[0x388/4] = (void*)&fh_getServerInfoRoundsPerMap;
        g_fhVtbl[0x38C/4] = (void*)&fh_getServerInfoSpawnTime;
        g_fhVtbl[0x390/4] = (void*)&fh_getServerInfoTimeLimit;
        g_fhVtbl[0x394/4] = (void*)&fh_getServerInfoTicketRatio;
        g_fhVtbl[0x398/4] = (void*)&fh_getServerInfoTeamRatio;
        g_fhVtbl[0x39C/4] = (void*)&fh_getServerInfoAutoBalance;
        g_fhVtbl[0x3A0/4] = (void*)&fh_getServerInfoGlobalUnlocks;
    }
}
struct FakeHost { void* vtbl; };


// FileHandle - a BF2 IStream stand-in serving a SWF from an in-memory buffer. Raw vtable at the exact
// offsets the SwiffPlayer tu_file callbacks use: release +0x08, read +0x10, write +0x14, eof +0x1C,
// seek +0x30, tell +0x34, getSize +0x38.

struct FileHandle { void* vtbl; unsigned char* data; int size; int pos; };

static int  __fastcall fhandle_release(FileHandle* s, void*) { if (s->data) free(s->data); s->data = 0; free(s); return 0; }
static int  __fastcall fhandle_read(FileHandle* s, void*, void* dst, int bytes)
{ int n = bytes; if (s->pos + n > s->size) n = s->size - s->pos; if (n < 0) n = 0; if (n) memcpy(dst, s->data + s->pos, n); s->pos += n; return n; }
static int  __fastcall fhandle_write(FileHandle*, void*, const void*, int) { return 0; }
static int  __fastcall fhandle_eofCheck(FileHandle* s, void*) { return (s->pos >= s->size) ? 1 : 0; }  // callback wants (==0) => not-eof
static int  __fastcall fhandle_seek(FileHandle* s, void*, int origin, int off) { (void)origin; if (off < 0) off = 0; if (off > s->size) off = s->size; s->pos = off; return 1; } // nonzero = success
static int  __fastcall fhandle_tell(FileHandle* s, void*) { return s->pos; }
static int  __fastcall fhandle_getSize(FileHandle* s, void*) { return s->size; }
static void __fastcall fhandle_generic(void*, void*) { }

static void* g_fhandleVtbl[16];
static void  buildFileHandleVtbl()
{
    for (int i = 0; i < 16; ++i) g_fhandleVtbl[i] = (void*)&fhandle_generic;
    g_fhandleVtbl[0x08/4] = (void*)&fhandle_release;
    g_fhandleVtbl[0x10/4] = (void*)&fhandle_read;
    g_fhandleVtbl[0x14/4] = (void*)&fhandle_write;
    g_fhandleVtbl[0x1C/4] = (void*)&fhandle_eofCheck;
    g_fhandleVtbl[0x30/4] = (void*)&fhandle_seek;
    g_fhandleVtbl[0x34/4] = (void*)&fhandle_tell;
    g_fhandleVtbl[0x38/4] = (void*)&fhandle_getSize;
}


// FakeFileManager - IBase + the open slot at +0x18. Maps "Menu/External/FlashMenu/<name>" to the loose
// SWF in the game's menu dir and serves it whole from memory.

class FakeFileManager : public IBase
{
public:
    virtual void  addRef()  {}
    virtual int   getRef()  { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<FakeFileManager*>(this); }
    virtual void  slot_0x10() { }
    virtual void  slot_0x14() { }
    // +0x18 open(const std::string& name, mode, access, flags) -> IStream* (our FileHandle)
    virtual void* open(const vc71::string& name, int /*mode*/, int /*access*/, unsigned /*flags*/)
    {
        const char* p = name.c_str();
        const char* base = p;
        for (const char* c = p; *c; ++c) if (*c == '/' || *c == '\\') base = c + 1;
        char full[MAX_PATH];
        // [TEMP] BF2_SWIFFPLAYER_MENUDIR serves the movie from another folder - a hand-assembled
        // bytecode movie in an agent-owned directory, so a VM fixture never writes into the game dir.
        const char* menuDir = getenv("BF2_SWIFFPLAYER_MENUDIR");
        sprintf_s(full, "%s\\%s", (menuDir && menuDir[0]) ? menuDir : kMenuDir, base);
        FILE* f = 0; fopen_s(&f, full, "rb");
        if (!f) { printf("      FileManager.open(\"%s\") -> NOT FOUND (%s)\n", p, full); return 0; }
        fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
        FileHandle* h = (FileHandle*)malloc(sizeof(FileHandle));
        h->vtbl = g_fhandleVtbl; h->size = (int)sz; h->pos = 0;
        h->data = (unsigned char*)malloc(sz > 0 ? sz : 1);
        size_t got = fread(h->data, 1, sz, f); fclose(f);
        h->size = (int)got;
        printf("      FileManager.open(\"%s\") -> %ld bytes from %s\n", p, sz, full);
        return h;
    }
};


// Minimal IBase for the host-owned singletons.

class HostObject : public IBase
{
public:
    explicit HostObject(const char* name) : m_name(name), m_refCount(1) {}
    virtual void  addRef()  { ++m_refCount; }
    virtual int   getRef()  { return m_refCount; }
    virtual int   release() { return --m_refCount; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostObject*>(this); }
    const char* name() const { return m_name; }
private:
    const char* m_name;
    int         m_refCount;
};


// EventManager stand-in - captures the module's ECCore handler so the harness can post (10,1)/(10,2).

typedef void (__thiscall* CoreEventFn)(void* self, int category, int id, int a3, int a4);
class HostEventManager : public IBase
{
public:
    HostEventManager() : m_handler(0) {}
    virtual void  addRef()  {}
    virtual int   getRef()  { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostEventManager*>(this); }
    virtual void slot_0x10() { unexpected(0x10); }
    virtual void slot_0x14() { unexpected(0x14); }
    virtual void slot_0x18() { unexpected(0x18); }
    virtual void slot_0x1C() { unexpected(0x1C); }
    virtual void slot_0x20() { unexpected(0x20); }
    virtual void slot_0x24() { unexpected(0x24); }
    virtual void slot_0x28() { unexpected(0x28); }
    virtual int  registerHandler(int c, void* h, int p) { printf("      eventManager.registerHandler(cat=%d, handler=%p, pri=%d)\n", c, h, p); m_handler = h; return 1; }
    virtual int  unregisterHandler(int c, void* h) { printf("      eventManager.unregisterHandler(cat=%d, handler=%p)\n", c, h); if (m_handler == h) m_handler = 0; return 1; }
    bool fire(int c, int id) { if (!m_handler) return false; void** v = *reinterpret_cast<void***>(m_handler); reinterpret_cast<CoreEventFn>(v[0])(m_handler, c, id, 0, 0); return true; }
    bool hasHandler() const { return m_handler != 0; }
private:
    static void unexpected(unsigned s) { printf("  [FAIL] unmodelled EventManager slot +0x%02X\n", s); ++g_failures; }
    void* m_handler;
};


// The fake ClassManager.

class HostClassManager : public IClassManager
{
public:
    HostClassManager() : m_classCount(0), m_singletonCount(0), m_serverCount(0) {}
    virtual void  addRef() {}
    virtual int   getRef() { return 1; }
    virtual int   release() { return 1; }
    virtual void* queryInterface(unsigned int) const { return const_cast<HostClassManager*>(this); }
    virtual void  reserved_vectorDeletingDestructor() { fatal("~ClassManager"); }
    virtual void registerServer(IClassServer* server)
    {
        if (m_serverCount < 8) m_servers[m_serverCount++] = server;
        vc71::vector_layout classes = { 0, 0, 0, 0 }, singles = { 0, 0, 0, 0 };
        server->addRef();
        server->getClassList(&classes);
        server->getSingletonList(&singles);
        for (ClassRegInfo* c = (ClassRegInfo*)classes.first; c != (ClassRegInfo*)classes.last; ++c)
        {
            if (m_classCount < 64) { m_classIds[m_classCount] = c->classId; m_classCreate[m_classCount] = c->create;
                strncpy_s(m_classNames[m_classCount], sizeof(m_classNames[0]), c->name.c_str(), _TRUNCATE); ++m_classCount; }
            printf("      class    cid=%-10u %s\n", c->classId, c->name.c_str());
        }
        for (SingletonRegInfo* s = (SingletonRegInfo*)singles.first; s != (SingletonRegInfo*)singles.last; ++s)
            registerSingleton(*s);
        destroyVector<ClassRegInfo>(classes);
        destroyVector<SingletonRegInfo>(singles);
    }
    virtual void unregisterServer(IClassServer* server)
    {
        for (int i = 0; i < m_serverCount; ++i) if (m_servers[i] == server) { m_servers[i] = 0; server->release(); return; }
        fatal("unregisterServer: never registered");
    }
    virtual void  reserved_setDefaultClass()  { fatal("setDefaultClass"); }
    virtual void  reserved_getDefaultClass()  { fatal("getDefaultClass"); }
    virtual void  reserved_getClassName()     { fatal("getClassName"); }
    virtual bool  getClassID(const vc71::string& name, unsigned int* out)
    { for (int i = 0; i < m_classCount; ++i) if (!strcmp(m_classNames[i], name.c_str())) { if (out) *out = m_classIds[i]; return true; } return false; }
    virtual void  reserved_createInstance_cid() { fatal("createInstance(cid)"); }
    virtual IBase* createInstance(unsigned int cid, unsigned int iid, IBase* outer)
    { for (int i = 0; i < m_classCount; ++i) if (m_classIds[i] == cid) return m_classCreate[i](iid, outer); return 0; }
    virtual void  reserved_createInstance_name() { fatal("createInstance(name)"); }
    virtual void  reserved_setSingletonClass()   { fatal("setSingletonClass"); }
    virtual void  reserved_getSingletonClass()   { fatal("getSingletonClass"); }
    virtual void registerSingleton(const SingletonRegInfo& info)
    {
        IBase* created = 0;
        if (info.flags & 2) { created = createInstance(info.classId, info.iid, 0); if (info.instance) *info.instance = created; }
        if (m_singletonCount < 64) { strncpy_s(m_singletonNames[m_singletonCount], sizeof(m_singletonNames[0]), info.name.c_str(), _TRUNCATE);
            m_singletonSlots[m_singletonCount] = info.instance; ++m_singletonCount; }
        printf("      singleton %-14s iid=%-8u cid=%-8u flags=%d -> %s\n", info.name.c_str(), info.iid, info.classId, info.flags,
               (info.flags & 2) ? (created ? "created" : "NULL") : "published");
    }
    virtual void unregisterSingleton(const vc71::string& name)
    { for (int i = 0; i < m_singletonCount; ++i) if (!strcmp(m_singletonNames[i], name.c_str())) { m_singletonNames[i][0] = '\0'; return; } }
    virtual IBase* getSingleton(const vc71::string& name)
    { for (int i = 0; i < m_singletonCount; ++i) if (!strcmp(m_singletonNames[i], name.c_str())) return m_singletonSlots[i] ? *m_singletonSlots[i] : 0; return 0; }
    virtual void  reserved_getSingletonAs()               { fatal("getSingletonAs"); }
    virtual void  registerSimpleSingleton(const renddx9abi::HostString&, void*) { fatal("registerSimpleSingleton"); }
    virtual void  reserved_unregisterSimpleSingleton()    { fatal("unregisterSimpleSingleton"); }
    virtual void  reserved_getSimpleSingleton()           { fatal("getSimpleSingleton"); }
    virtual void  reserved_getPrintableSingletonList()    { fatal("getPrintableSingletonList"); }
    virtual void  reserved_getAllClassIncludingSubstring(){ fatal("getAllClassIncludingSubstring"); }
    virtual void  reserved_initSingletons()               { fatal("initSingletons"); }
    void publish(const char* name, IBase** slot)
    { strncpy_s(m_singletonNames[m_singletonCount], sizeof(m_singletonNames[0]), name, _TRUNCATE); m_singletonSlots[m_singletonCount] = slot; ++m_singletonCount; }
    int serverCount() const { int n = 0; for (int i = 0; i < m_serverCount; ++i) if (m_servers[i]) ++n; return n; }
private:
    static void fatal(const char* s) { printf("  [FAIL] unimplemented IClassManager slot: %s\n", s); ++g_failures; }
    template <typename T> static void destroyVector(vc71::vector_layout& v)
    { for (T* p = (T*)v.first; p != (T*)v.last; ++p) p->~T(); if (v.first) ::operator delete(v.first); v.first = v.last = v.end = 0; }
    IClassServer* m_servers[8]; int m_serverCount;
    unsigned int m_classIds[64]; char m_classNames[64][96];
    IBase* (__fastcall *m_classCreate[64])(unsigned int, IBase*); int m_classCount;
    char m_singletonNames[64][64]; IBase** m_singletonSlots[64]; int m_singletonCount;
};


// test 0 - the string ABI vs the real MSVCP71.

typedef void (__thiscall* P71Ctor)(void*, const char*);
typedef void (__thiscall* P71Dtor)(void*);
static void testStringAbi(HMODULE p71)
{
    P71Ctor ctor = (P71Ctor)GetProcAddress(p71, "??0?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@PBD@Z");
    P71Dtor dtor = (P71Dtor)GetProcAddress(p71, "??1?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@QAE@XZ");
    if (!ctor || !dtor) { check(false, "MSVCP71 basic_string ctor/dtor resolved"); return; }
    { unsigned char t[28]; memset(t, 0xCD, 28); ctor(t, "SwiffPlayer");
      vc71::string o("SwiffPlayer"); const unsigned char* ob = (const unsigned char*)&o;
      check(memcmp(t + 4, ob + 4, 12) == 0, "short \"SwiffPlayer\": chars+terminator match MSVCP71");
      check(memcmp(t + 20, ob + 20, 8) == 0, "short: _Mysize and _Myres match MSVCP71"); dtor(t); }
    { unsigned char t[28]; memset(t, 0xCD, 28); ctor(t, "ObjectTemplateManager");
      vc71::string o("ObjectTemplateManager"); const unsigned char* ob = (const unsigned char*)&o;
      unsigned tr = *(unsigned*)(t + 24), orr = *(unsigned*)(ob + 24);
      check(*(unsigned*)(t + 20) == 21 && *(unsigned*)(ob + 20) == 21, "long: _Mysize == 21 both sides");
      check(tr == orr && tr >= 16, "long: _Myres matches MSVCP71 rounding, heap"); dtor(t); }
}


// Raw dispatch into the created SwiffPlayer, the way BF2.exe does (by vtable byte offset).

static void  pl_setRender(void* p, void* r)    { void** v = *(void***)p; ((void(__thiscall*)(void*,void*))v[0x24/4])(p, r); }
static void  pl_setSwiffHost(void* p, void* h) { void** v = *(void***)p; ((void(__thiscall*)(void*,void*))v[0x58/4])(p, h); }
static bool  pl_loadMovie(void* p, const char* n) { void** v = *(void***)p; return ((bool(__thiscall*)(void*,const char*))v[0x14/4])(p, n); }
static void  pl_update(void* p, float dt)      { void** v = *(void***)p; ((void(__thiscall*)(void*,float))v[0x18/4])(p, dt); }
static void  pl_display(void* p)               { void** v = *(void***)p; ((void(__thiscall*)(void*))v[0x1C/4])(p); }
static bool  pl_isMovieLoaded(void* p)         { void** v = *(void***)p; return ((bool(__thiscall*)(void*))v[0x20/4])(p); }
static void  pl_queueMouse(void* p, int x, int y, int b) { void** v = *(void***)p; ((void(__thiscall*)(void*,int,int,int))v[0x30/4])(p, x, y, b); }   // slot 12 queueMouseState

static unsigned g_sehCode = 0; static void* g_sehAddr = 0;
static void* g_sehFrames[16]; static int g_sehFrameCount = 0;
static int recordSeh(EXCEPTION_POINTERS* ep)
{
    g_sehCode = ep->ExceptionRecord->ExceptionCode;
    g_sehAddr = ep->ExceptionRecord->ExceptionAddress;
    // Walk the EBP frame chain from the crash context (the DLL is /Zi with frame pointers).
    g_sehFrameCount = 0;
    void** frame = (void**)ep->ContextRecord->Ebp;
    for (int i = 0; i < 16 && frame; ++i)
    {
        if (IsBadReadPtr(frame, 8)) break;
        void* ret = frame[1];                     // saved return address
        g_sehFrames[g_sehFrameCount++] = ret;
        void** next = (void**)frame[0];           // saved EBP
        if (next <= frame) break;                 // chain must climb
        frame = next;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

// The frame loop, extracted so __try has no C++ unwinding objects around it. Returns false on a crash.
static bool runFrames(void* player, int frames, float dt)
{
    for (int f = 0; f < frames; ++f)
    {
        printf("      frame %d: update...", f); streamf("--- frame %d ---", f);
        __try { pl_update(player, dt); printf(" display..."); pl_display(player); printf(" ok\n"); }
        __except (recordSeh(GetExceptionInformation()))
        {
            HMODULE m = 0;
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               (LPCSTR)g_sehAddr, &m);
            printf(" CRASH code=0x%08X at %p  RVA=0x%X\n", g_sehCode, g_sehAddr,
                   m ? (unsigned)((char*)g_sehAddr - (char*)m) : 0);
            printf("      call stack (SwiffPlayer.dll RVAs, map them in SwiffPlayer.map):\n");
            for (int i = 0; i < g_sehFrameCount; ++i)
            {
                HMODULE fm = 0;
                GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   (LPCSTR)g_sehFrames[i], &fm);
                if (fm == m) printf("        #%d  RVA=0x%X\n", i, (unsigned)((char*)g_sehFrames[i] - (char*)m));
                else         printf("        #%d  %p (other module)\n", i, g_sehFrames[i]);
            }
            return false;
        }
    }
    return true;
}

// scaffolding: the same loop paced at dt per frame (Sleep), for scenarios that need the movie's
// setInterval timers - gameswf's Timer::expired reads the wall clock.
static bool runFramesPaced(void* player, int frames, float dt)
{
    for (int f = 0; f < frames; ++f)
    {
        if (!runFrames(player, 1, dt)) return false;
        Sleep((DWORD)(dt * 1000.0f));
    }
    return true;
}


int main()
{
    setvbuf(stdout, 0, _IONBF, 0);   // unbuffered - so a crash mid-run still shows how far we got
    printf("=== SwiffPlayer replacement host harness (Stage 3) ===\n\n");
    buildFakeRenderVtbl(); buildFakeHostVtbl(); buildFakeGeneralSettingsVtbl(); buildFakeEorHudVtbl(); buildFileHandleVtbl();
    if (eorFixtureScenario()) initEorFixtureData();

    SetDllDirectoryA(kGameDir);
    char path[MAX_PATH];
    sprintf_s(path, "%s\\msvcr71.dll", kGameDir); HMODULE r71 = LoadLibraryA(path);
    sprintf_s(path, "%s\\msvcp71.dll", kGameDir); HMODULE p71 = LoadLibraryA(path);
    printf("[setup] msvcr71=%p msvcp71=%p\n", (void*)r71, (void*)p71);
    check(r71 != 0, "msvcr71.dll loaded"); check(p71 != 0, "msvcp71.dll loaded");

    printf("\n[TEST 0] std::string ABI vs the real MSVCP71\n");
    if (p71) testStringAbi(p71);

    printf("\n[TEST 1] initDll\n");
    static HostEventManager eventManagerObject;
    static FakeFileManager  fileManagerObject;
    static FakeGeneralSettings generalSettingsObject;
    static FakeEorHudInformationLayer eorHudObject;
    generalSettingsObject.vtbl = g_generalSettingsVtbl;
    eorHudObject.vtbl = g_eorHudVtbl;
    static HostObject objects[] = {
        HostObject("SettingsRepostitory"), HostObject("MainConsoleObjects"), HostObject("MainConsole"),
        HostObject("FileChangeManager"), HostObject("BfGlobal_callback"), HostObject("BfGlobal_callbackData"),
        HostObject("DebugOn"), HostObject("DebugModules"), HostObject("Profiler"), HostObject("System"),
        HostObject("FrameEventManager"), HostObject("ObjectManager"), HostObject("ObjectTemplateManager"),
        HostObject("PlayerManager"),
    };
    static IBase* slots[64];
    HostClassManager cm; int s = 0;
    for (int i = 0; i < (int)(sizeof(objects)/sizeof(objects[0])); ++i, ++s) { slots[s] = &objects[i]; cm.publish(objects[i].name(), &slots[s]); }
    slots[s] = &fileManagerObject;  cm.publish("FileManager",  &slots[s]); ++s;
    slots[s] = &eventManagerObject; cm.publish("EventManager", &slots[s]); ++s;
    if (eorFixtureScenario())
    {
        slots[s] = reinterpret_cast<IBase*>(&generalSettingsObject);
        cm.publish("GeneralSettings", &slots[s]); ++s;
        slots[s] = reinterpret_cast<IBase*>(&eorHudObject);
        cm.publish("HudInformationLayer", &slots[s]); ++s;
    }

    HMODULE dll = LoadLibraryA("SwiffPlayer.dll");
    if (!dll) { printf("  [FAIL] LoadLibraryA(\"SwiffPlayer.dll\") GLE=%lu\n", GetLastError()); return 1; }
    check(true, "LoadLibraryA(\"SwiffPlayer.dll\")");
    typedef bool (__cdecl* InitFn)(IClassManager*); typedef void (__cdecl* DeinitFn)();
    InitFn initDll = (InitFn)GetProcAddress(dll, "initDll"); DeinitFn deinitDll = (DeinitFn)GetProcAddress(dll, "deinitDll");
    check(initDll && deinitDll, "GetProcAddress initDll/deinitDll");
    if (!initDll || !deinitDll) return 1;

    printf("    registration walk:\n");
    check(initDll(&cm), "initDll returned TRUE");
    check(cm.serverCount() == 1, "the one module registered");
    check(eventManagerObject.hasHandler(), "module registered its ECCore handler (cat 10)");

    printf("\n[TEST 2] event (10,1) -> factory -> \"SwiffPlayer\" singleton\n");
    check(eventManagerObject.fire(10, 1), "posted (10,1)");
    IBase* player = cm.getSingleton(vc71::string("SwiffPlayer"));
    check(player != 0, "SwiffPlayer singleton created");
    if (!player) return 1;

    printf("\n[TEST 3] QueryInterface / refcount\n");
    check(player->queryInterface(1u) == player, "QI(IID_IBase 1)");
    check(player->queryInterface(870001u) == player, "QI(IID_ISwiffPlayer 870001)");
    check(player->queryInterface(999u) == 0, "QI(bogus) -> null");

    // test 4: wire the fakes + load a real SWF
    // BF2_SWIFFPLAYER_MOVIE overrides which movie is driven, so the AS-demand sweep can be run
    // against mainMenu.swf (the real front end, 2 MB) as well as the small menu.swf.
    const char* movieName = getenv("BF2_SWIFFPLAYER_MOVIE");
    if (!movieName || !movieName[0]) movieName = "menu.swf";
    printf("\n[TEST 4] setRender + setSwiffHost + loadMovie(\"%s\")\n", movieName);
    static FakeRender fakeRender; fakeRender.vtbl = g_frVtbl;
    static FakeHost   fakeHost;   fakeHost.vtbl   = g_fhVtbl;
    pl_setRender(player, &fakeRender);
    pl_setSwiffHost(player, &fakeHost);
    check(!pl_isMovieLoaded(player), "isMovieLoaded() false before load");
    const bool loaded = pl_loadMovie(player, movieName);
    check(loaded, "loadMovie(...) returned true (real gameswf parse)");
    check(pl_isMovieLoaded(player), "isMovieLoaded() true after load");

    // Runs time/input updates, advances ActionScript, then displays into the recording renderer.
    printf("\n[TEST 5] frame pipeline -> full ISwiffRender render call stream\n");
    int frames = 3; const float dt = 1.0f / 30.0f;
    // [TEMP] BF2_SWIFFPLAYER_FRAMES=<N> drives more frames, so the stream tracer can show the settled
    // layout (after the intro tweens) for the retail-vs-harness diff.
    if (const char* nf = getenv("BF2_SWIFFPLAYER_FRAMES")) { const int v = atoi(nf); if (v > 0) frames = v; }
    memset(g_callCounts, 0, sizeof(g_callCounts));
    g_streamOn = true; g_streamLen = 0;
    const bool ranClean = runFrames(player, frames, dt);
    g_streamOn = false;
    const int len1 = g_streamLen;
    static char firstRun[sizeof(g_stream)];
    memcpy(firstRun, g_stream, len1 < (int)sizeof(firstRun) ? len1 : (int)sizeof(firstRun));

    printf("      render call stream (%d bytes):\n", len1);
    { const char* p = g_stream; int left = len1; int shown = 0; while (left > 0 && shown < 40) { const char* nl = (const char*)memchr(p, '\n', left); int n = nl ? (int)(nl - p) : left; printf("        %.*s\n", n, p); ++shown; if (!nl) break; p = nl + 1; left -= (n + 1); } if (shown >= 40) printf("        ... (truncated)\n"); }

    check(ranClean, "the frame loop ran without crashing - advance() completed, the movie rendered");
    check(g_callCounts[3] >= frames, "updateTime dispatched once per frame (+0xD8)");
    check(g_callCounts[4] >= 1, "setMouseState dispatched (+0xB8) - deferred-mouse path + host getViewportSize");
    check(g_callCounts[0] >= 1 && g_callCounts[1] >= 1, "beginDisplay/endDisplay bracket the frame - the movie DREW");
    check(g_callCounts[2] >= 1, "drawVertices fired - the movie drew vector geometry (the menu background)");
    if (eorFixtureScenario())
    {
        check(g_eorAutoReadyReads >= 1, "endOfRound read GeneralSettings.getAutoReady (+0xF8)");
        check(g_eorLoadProgressReads >= 1, "endOfRound polled Client.getLoadProgress host +0x94");
        check(g_eorCurrentMapReads >= 1, "endOfRound queried the current map (hasMap/map briefing)");
        check(g_eorMapFinds >= 1, "endOfRound resolved the retail-layout ModInfo map record");
        check(g_eorLocalServerFills >= 1, "endOfRound fetched the local server-info record");
        const int selector0Calls = g_eorServerInfoReads[2]; // roundsPerMap returns zero: one call/list build
        check(selector0Calls >= 1,
              "endOfRound executed eorsinfo selector 0 (rounds-per-map live probe reached)");
        check(g_eorServerInfoReads[1] == selector0Calls * 2 &&
              g_eorServerInfoReads[3] == selector0Calls * 2 &&
              g_eorServerInfoReads[5] == selector0Calls * 2 &&
              g_eorServerInfoReads[4] == selector0Calls &&
              g_eorServerInfoReads[6] == selector0Calls,
              "eorsinfo 0 repeats nonzero live getters and calls zero fallback getters once");
        const int selector1Calls = g_eorServerInfoReads[8]; // globalUnlocks returns zero: one call/list build
        check(selector1Calls >= 1,
              "endOfRound executed eorsinfo selector 1 (global-unlocks live probe reached)");
        check(g_eorServerInfoReads[7] == selector1Calls * 2 &&
              g_eorServerInfoReads[0] == selector1Calls * 2,
              "eorsinfo 1 repeats nonzero auto-balance/no-vehicles and uses the non-coop arm");
        check(g_eorHudPublications >= 2 &&
              strcmp(g_eorHudName, "BF2Rebuilt fixture") == 0 &&
              strcmp(g_eorHudAddress, "127.0.0.1") == 0 && g_eorHudPort == 29900,
              "both eorsinfo selectors publish host name, address and query port to HudInformationLayer");
    }
    (void)firstRun; (void)len1;
    // Reloading retains ActionScript globals, so determinism comparisons require separate processes.
    // The host-message scenario queues an OK-button click and expects removeLast; BF2_SWIFFPLAYER_CLICK selects movie coordinates.
    if (hostMessageScenario())
    {
        int mx = 640, my = 407;
        if (const char* cl = getenv("BF2_SWIFFPLAYER_CLICK")) sscanf_s(cl, "%d,%d", &mx, &my);
        printf("\n[TEST 5b] host message -> dialog -> OK click at (%d,%d)\n", mx, my);
        // gameswf's interval timers read the wall clock (Timer::expired, tu_timer ticks), and the
        // movie's UpdateManager ticks its callbacks from setInterval: paced frames, or the message
        // poll never fires. 90 frames at 30/s is three seconds, the poll's period is well inside.
        g_streamOn = true;
        bool ok = runFramesPaced(player, 90, dt);
        pl_queueMouse(player, mx, my, 0); if (ok) ok = runFramesPaced(player, 4, dt);
        pl_queueMouse(player, mx, my, 1); if (ok) ok = runFramesPaced(player, 4, dt);
        pl_queueMouse(player, mx, my, 0); if (ok) ok = runFramesPaced(player, 12, dt);
        g_streamOn = false;
        check(ok, "the message scenario ran without crashing");
        check(g_msgRemoved >= 1, "the OK release reached dice.MessageHandler.removeLast (the dialog's onRelease)");
    }

    // test 6: unpublish + unload (advance() completes cleanly now, so teardown is safe)
    printf("\n[TEST 6] event (10,2) unpublishes; deinitDll + FreeLibrary\n");
    eventManagerObject.fire(10, 2);
    check(cm.getSingleton(vc71::string("SwiffPlayer")) == 0, "(10,2) cleared the SwiffPlayer singleton");
    deinitDll();
    check(cm.serverCount() == 0, "deinitDll unregistered the module");
    check(FreeLibrary(dll) != 0, "FreeLibrary");

    printf("\n=== %s: %d failure(s) ===\n", g_failures ? "FAILED" : "PASSED", g_failures);
    return g_failures ? 1 : 0;
}
