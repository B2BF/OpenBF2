// Path:     attested - "C:\dice\Projects\BF2Branches\Patch_1_50\Code\BF2\RendDX9\ShaderManager.cpp",
//           quoted with lines 950 and 980, both "Undefined texture filter quality."
// Contents: recovered from RendDX9.dll. The constructor, init and updateShaderDefines are written;
//           the effect cache and everything that compiles a shader are not.

// See Code/BF2/RendDX9/BF2Render.cpp for the BF2_IMAGE_RENDDX9 guard and why it is here.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/BfMemory.h"
#include <string>
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"
#include "../RendMisc/IRenderer.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"

#include "../IO/File/FileManager.h"

#include <map>
#include <new>
#include <set>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_deviceCaps;
extern void* g_rendererBase;   // RendDX9Init.cpp, dword_1023B970 - the two A2M-mask reads below
namespace Ra {
extern void* g_effectManager;
unsigned char effectManagerFlag40();   // Ra/EffectManager.cpp - its +0x40 byte
}

namespace {

// [CHOSEN, not recovered] ABI adapter for the two recovered FileManager calls at
// PE 10022F17 and 10022653. The retail table uses +90 and a VC7.1 string return.
// Native IFileSystem has an inherited destructor before FileManager's additions;
// its +90 is hasMountedArchives, so native calls must use the real declaration.
// [RETIRE-WHEN: RW-49] Keep the retail branch and its string carrier unchanged.
renddx9abi::HostString stripShaderModPath(const char* path)
{
    renddx9abi::HostString in(path);
    return static_cast<io::FileManager*>(::dice::hfe::fileManager)->stripModPath(in);
}

struct CapsView { D3DCAPS9 caps; D3DADAPTER_IDENTIFIER9 adapter; int gpuFamily; };

const CapsView* capsOf() { return static_cast<const CapsView*>(g_deviceCaps); }

bool isNvidia()          { const CapsView* c = capsOf(); return c && c->adapter.VendorId == 4318u; }
bool isAti()             { const CapsView* c = capsOf(); return c && c->adapter.VendorId == 4098u; }
unsigned int vsMajor()   { const CapsView* c = capsOf(); return c ? ((c->caps.VertexShaderVersion >> 8) & 0xFFu) : 0u; }
unsigned int psMajor()   { const CapsView* c = capsOf(); return c ? ((c->caps.PixelShaderVersion  >> 8) & 0xFFu) : 0u; }
int  gpuFamily()         { const CapsView* c = capsOf(); return c ? c->gpuFamily : -1; }
unsigned int maxAniso()  { const CapsView* c = capsOf(); return c ? c->caps.MaxAnisotropy : 0u; }
bool minAnisoOk()        { const CapsView* c = capsOf(); return c && (c->caps.TextureFilterCaps & 0x400u) != 0u; }
bool magAnisoOk()        { const CapsView* c = capsOf(); return c && (c->caps.TextureFilterCaps & 0x4000000u) != 0u; }

// PE sub_1000A6B0 on dword_1023B970, is multisampling active: the forced-on byte
// +0x514 wins, then the forced-off +0x515, else the first entry of the multisample list (+0xC0 /
// +0xC4, 8-byte descriptors) has a type above zero. The same file-local copy BF2Render.cpp,
// TerrainDraw.cpp and FSQuadDrawer.cpp carry. Both +0x514 / +0x515 and the list are written by
// VideoOptions' resolution change (sub_1000F8D0 and its probe sub_1000A700), so this is no longer
// constant false.
bool isMultisampleActive()
{
    const RendDX9Base* const base = static_cast<const RendDX9Base*>(g_rendererBase);
    if (base->forceMultisample)
        return true;
    if (base->disableMultisample)
        return false;
    return multiSampleListActive(base);
}

// The transparency-AA code the "%X" formatter reads - the dword at 0x1023D090 / 0x1023D094.
unsigned int transparencyAACode(const unsigned char (&code)[4])
{
    unsigned int value;
    ::memcpy(&value, code, sizeof(value));
    return value;
}

struct ShaderDefine { const char* name; const char* value; };

#include "ShaderManagerDefines.inc"

} // anonymous namespace


// Shader cache, macro state and include/device-listener interfaces.
// Preserve the three interface subobjects and retained device reference.

class Effect;

struct ShaderManager
{
    // Every scalar starts at zero; the containers start empty.
    ShaderManager()
        : vptr(0), vptr2(0), vptrDeviceLost(0), refCount(0), device(0)
        , flag78(0), flag79(0), flag7A(0), flag7B(0), flag7C(0), flag7D(0), flag7E(0), flag7F(0)
        , flag80(0), flags84(0), currentEffect98(0), statesEffect9C(0), rawEffectA0(0)
        , disableStateManagerA4(0)
    {
        pad81[0] = pad81[1] = pad81[2] = 0;
        padA5[0] = padA5[1] = padA5[2] = 0;
    }

    void*         vptr;
    void*         vptr2;
    void*         vptrDeviceLost;
    int           refCount;                     // +0x0C
    void*         device;                       // +0x10
    // The lowered, mod-stripped effect name -> its Effect wrapper (strcmp order).
    std::map<std::string, Effect*> effectCache;   // +0x14  node 0x30
    // The other five containers are constructed with it, as 0x10024A40 does (sub_10024D40 for the
    // four 0x30-node maps, sub_10024E70 for the 0x14-node one, sub_1001BA10 for the 0x48-node one),
    // because the device-lost listener walks +0x48 unguarded. Only +0x14 and +0x48 have a recovered
    // reader; the names below are positional and the element types of the unread three are
    // [INFERRED] from their node sizes.
    std::map<std::string, void*> cache20;      // +0x20  node 0x30 - getEffect's second map (unwritten)
    std::string  name;                         // +0x2C
    // +0x48: a map<string, T*> (node 0x30) whose T carries its OWN map of ID3DXEffect* at +0x9C
    // (node 0x1C: value +0x10, _Isnil +0x19). The listener's second walk is the only recovered
    // reader; nothing recovered inserts, so it stays empty here. [UNRESOLVED] what T is (the
    // functions at 0x1001A5A9..0x1001A9D6 read a +0xA0 head - the states-effect code, 0x1001A0A0).
    std::map<std::string, void*> map48;        // +0x48  node 0x30
    std::map<std::string, void*> map54;        // +0x54  node 0x30
    std::set<unsigned int>        map60;        // +0x60  node 0x14 - one dword per node
    std::map<std::string, std::string> map6C; // +0x6C  node 0x48 - two strings per node
    unsigned char flag78;                       // +0x78  isNvidia, written at the very end
    unsigned char flag79;
    unsigned char flag7A;                       // +0x7A  -> flags |= 0x20
    unsigned char flag7B;                       // +0x7B  -> if zero, byte at +0x85 |= 1
    unsigned char flag7C;                       // +0x7C  -> flags |= 4
    unsigned char flag7D;
    unsigned char flag7E;                       // ctor: 1
    unsigned char flag7F;                       // +0x7F  -> debugterrain
    unsigned char flag80;                       // ctor: 1
    unsigned char pad81[3];
    unsigned int  flags84;                      // +0x84
    // The flat define list: name, value, name, value, ...
    std::vector<std::string> defines;          // +0x88
    // Current wrapper, states effect, and raw effect used for pre-draw CommitChanges.
    // A nonzero disable flag bypasses the shared StateManager.
    void*         currentEffect98;
    void*         statesEffect9C;
    void*         rawEffectA0;
    unsigned char disableStateManagerA4;
    unsigned char padA5[3];
};

void* g_shaderManager = 0;   // dword_1023B97C

extern IBase* g_Renderer;    // BF2Render.cpp - renderDevice() reaches the device through it

// Effect.cpp
class Effect;
Effect* createEffectObject();
bool effectBind(Effect* self, void* d3dxEffect);
void* effectD3dxHandle(void* effect);   // wrapper+0x08, the ID3DXEffect* (the listener walks)
const std::string& effectName(void* effect);   // wrapper+0x0C, the effect's name
void deobfuscateShader(unsigned char* buf, unsigned int size);   // the include Open uses it too

namespace {

// Appends one string to the flat define vector; callers alternate names and values.
void pushDefine(ShaderManager* sm, const char* text)
{
    sm->defines.push_back(std::string(text));
}

// Destroys the define strings; leaves the effect cache intact.
void clearDefines(ShaderManager* sm)
{
    sm->defines.clear();
}

} // anonymous namespace


// ShaderManager_updateShaderDefines, 0x10023730.

// Clears the list, walks the 120-entry table pushing name then value - nine names take a computed
// value instead - then appends the A2M pair, the two shader-model forcing flags, the filter and mip
// block from VideoOptions, and the terrain pair.

void updateShaderDefines(void* shaderManager, void* base)
{
    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    RendDX9Base*   b  = static_cast<RendDX9Base*>(base);
    const VideoOptions* vo = &b->videoOptions;                     // base + 1016

    clearDefines(sm);

    char buf[32];

    for (int i = 0; i < 120; ++i)
    {
        const char* name  = kShaderDefines[i].name;
        const char* value = kShaderDefines[i].value;

        pushDefine(sm, name);

        // The nine special cases, in the body's own strcmp order.
        if (::strcmp(name, "RAPATH") == 0)
        {
            // itoa(g_Renderer[1740], 10). With no Renderer the original starts from the literal
            // "0" it copies into the buffer before the test.
            pushDefine(sm, "0");
        }
        else if (::strcmp(name, "USEPARTIALPRECISIONTYPES") == 0)
        {
            pushDefine(sm, sm->flag7A ? "1" : "0");
        }
        else if (::strcmp(name, "PS2_EXT") == 0)
        {
            // This define expands to a shader profile name.
            pushDefine(sm, sm->flag78 ? "ps_2_a" : "ps_2_0");
        }
        else if (::strcmp(name, "NVIDIA") == 0)
        {
            pushDefine(sm, isNvidia() ? "1" : "0");
        }
        else if (::strcmp(name, "ATI") == 0)
        {
            pushDefine(sm, isAti() ? "1" : "0");
        }
        else if (::strcmp(name, "SM30") == 0)
        {
            pushDefine(sm, (psMajor() < 3 || vsMajor() < 3) ? "0" : "1");
        }
        else if (::strcmp(name, "IS_NV4X") == 0)
        {
            pushDefine(sm, gpuFamily() == 6 ? "1" : "0");
        }
        else if (::strcmp(name, "HASALPHA2MASK") == 0)
        {
            // 0x10023AB0..0x10023AF7, read: sub_1000A6B0(dword_1023B970) and
            // then the byte at dword_1023B970 + 0x516 (both through the global, not the `base`
            // argument) -> "1", else "0". The inputs are no longer constant: VideoOptions'
            // resolution change puts the selected AA descriptor into the multisample list and its
            // two probes set +0x514 / +0x515 / +0x516. (Was a hard-coded "0" while nothing wrote
            // them.)
            const bool alpha2Mask =
                isMultisampleActive() &&
                static_cast<const RendDX9Base*>(g_rendererBase)->hasAlphaToCoverage != 0;
            pushDefine(sm, alpha2Mask ? "1" : "0");
        }
        else if (::strcmp(name, "Alpha2Mask") == 0)
        {
            // Select the vendor-specific render state used to enable alpha-to-coverage.
            pushDefine(sm, isNvidia() ? "AdaptiveTess_Y" : value);
        }
        else
        {
            pushDefine(sm, value);
        }
    }

    // Format both vendor transparency-AA codes on each update before adding the defines.
    char a2mEnable[32];
    char a2mDisable[32];
    ::_snprintf_s(a2mEnable, sizeof(a2mEnable), _TRUNCATE, "%X",
                  transparencyAACode(g_transparencyAAOn));
    ::_snprintf_s(a2mDisable, sizeof(a2mDisable), _TRUNCATE, "%X",
                  transparencyAACode(g_transparencyAAOff));
    pushDefine(sm, "A2MEnable");
    pushDefine(sm, a2mEnable);
    pushDefine(sm, "A2MDisable");
    pushDefine(sm, a2mDisable);

    // _FORCE_1_4_SHADERS_ indexes a two-entry table of "0" / "1" at off_101E19A8. The guard reads
    //     if (psMajor < 2 || effectManager[64] != 0) -> 1 else 0
    const bool force14 =
        (psMajor() < 2) || (Ra::g_effectManager != 0 && Ra::effectManagerFlag40() != 0);
    pushDefine(sm, "_FORCE_1_4_SHADERS_");
    pushDefine(sm, force14 ? "1" : "0");
    pushDefine(sm, "_FORCE_1_3_SHADERS_");
    pushDefine(sm, "0");

    // the texture filter block, from VideoOptions::getQuality(7)
    const int filterQuality = vo->quality[7];
    const int aniso         = static_cast<int>(maxAniso());

    if (filterQuality == 1 || filterQuality == 2)
    {
        // Both arms leave all four diff filters linear; they differ only in which mip filter goes
        // point, which the mip block below picks up.
        pushDefine(sm, "FILTER_TRN_DIFF_MIN");  pushDefine(sm, "LINEAR");
        pushDefine(sm, "FILTER_TRN_DIFF_MAG");  pushDefine(sm, "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MIN"); pushDefine(sm, "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MAG"); pushDefine(sm, "LINEAR");
    }
    else if (filterQuality == 3)
    {
        // THE TWO clamps ARE different AND that IS NOT A slip: terrain caps at 2, road at 4.
        const int trnAniso  = aniso >= 2 ? 2 : aniso;
        const int roadAniso = aniso >= 4 ? 4 : aniso;

        pushDefine(sm, "FILTER_TRN_DIFF_MIN");
        pushDefine(sm, minAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_TRN_DIFF_MAG");
        pushDefine(sm, magAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_TRN_DIFF_MAX_ANISOTROPY");
        ::_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%i", trnAniso);
        pushDefine(sm, buf);

        pushDefine(sm, "FILTER_ROAD_DIFF_MIN");
        pushDefine(sm, minAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MAG");
        pushDefine(sm, magAnisoOk() ? "ANISOTROPIC" : "LINEAR");
        pushDefine(sm, "FILTER_ROAD_DIFF_MAX_ANISOTROPY");
        ::_snprintf_s(buf, sizeof(buf), _TRUNCATE, "%i", roadAniso);
        pushDefine(sm, buf);
    }
    else
    {
        ::renddx9abi::logf("ShaderManager.cpp:950 Undefined texture filter quality. (got %d)",
                           filterQuality);
    }

    // The three mip filters. All linear unless quality 1 or 2 forced one to point - quality 1
    // affects both the terrain and the bundled-mesh filter, quality 2 only the latter.
    const char* trnMip  = (filterQuality == 1) ? "POINT" : "LINEAR";
    const char* roadMip = (filterQuality == 1 || filterQuality == 2) ? "POINT" : "LINEAR";
    pushDefine(sm, "FILTER_TRN_MIP");      pushDefine(sm, trnMip);
    pushDefine(sm, "FILTER_ROAD_MIP");     pushDefine(sm, roadMip);
    pushDefine(sm, "FILTER_PARTICLE_MIP"); pushDefine(sm, roadMip);

    // terrain quality, from getQuality(0) - 1
    // 0x10023730 computes `result = getQuality(0) - 1`
    // and then tests `result - 1`: quality 1 (result 0) defines nothing, quality 2 (result 1) is
    // midterrain, quality 3 (result 2) is highterrain, anything else is the :980 diagnostic. The
    // earlier transcription made quality 2 high and quality 3 "undefined", which compiled the LOW
    // include for a High-terrain profile and left the terrain effect without `Hi_Terrain` (run gr10).
    const int terrain = vo->quality[0] - 1;
    if (terrain == 1)
    {
        pushDefine(sm, "HIGHTERRAIN"); pushDefine(sm, "0");
        pushDefine(sm, "MIDTERRAIN");  pushDefine(sm, "1");
    }
    else if (terrain == 2)
    {
        pushDefine(sm, "HIGHTERRAIN"); pushDefine(sm, "1");
        pushDefine(sm, "MIDTERRAIN");  pushDefine(sm, "0");
    }
    else if (terrain != 0)
    {
        // Preserve the diagnostic's original texture/terrain wording mismatch.
        ::renddx9abi::logf("ShaderManager.cpp:980 Undefined texture filter quality. (terrain %d)",
                           terrain);
    }

    if (sm->flag7F)
    {
        pushDefine(sm, "DEBUGTERRAIN");
        pushDefine(sm, "1");
    }

    const unsigned int count = static_cast<unsigned int>(sm->defines.size());
    ::renddx9abi::logf("ShaderManager::updateShaderDefines: %u entries (%u pairs) - NVIDIA=%d "
                       "ATI=%d SM30=%d IS_NV4X=%d PS2_EXT=%s filterQuality=%d terrain=%d",
                       count, count / 2,
                       isNvidia() ? 1 : 0, isAti() ? 1 : 0,
                       (psMajor() >= 3 && vsMajor() >= 3) ? 1 : 0,
                       gpuFamily() == 6 ? 1 : 0,
                       sm->flag78 ? "ps_2_a" : "ps_2_0",
                       filterQuality, terrain);
}


// ID3DXInclude occupies the ShaderManager face at +0x04; it has Open/Close only.
// Open resolves the mod path, reads into engine-owned storage, and optionally deobfuscates.
// A short read fails. Close frees the returned buffer.

namespace {

long __stdcall smIncludeOpen(void* /*self*/, int /*includeType*/, const char* fileName,
                             const void* /*parentData*/, const void** outData,
                             unsigned int* outBytes)
{
    if (!fileName || !outData || !outBytes)
        return (long)-2005529767;

    // FileManager +0x90 - stripModPath, string returned by value through the hidden slot.
    std::string path;
    if (::dice::hfe::fileManager)
    {
        const renddx9abi::HostString out = stripShaderModPath(fileName);
        renddx9abi::assignFromHostString(path, out);
    }
    else
    {
        path = fileName;
    }

    // sub_10029D00 - already under "mods/" stays; anything else gets the active mod's prefix.
    if (!::strstr(path.c_str(), "mods/"))
    {
        bool xpack = true;              // the shipped default when g_Renderer is null
        if (g_Renderer)
        {
            xpack = static_cast<IRenderer*>(g_Renderer)->isExpansionPack();
        }
        std::string prefixed(xpack ? "mods/xpack/" : "mods/bf2/");
        prefixed += path.c_str();
        path = prefixed;
    }

    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(path.c_str());
    if (!file.ok())
    {
        ::renddx9abi::logf("ShaderManager include Open(\"%s\"): NOT FOUND through the FileManager",
                           path.c_str());
        return (long)-2005530522;
    }

    // The buffer crosses the Open->parse->Close boundary, so it moves to a bare engine allocation
    // that Close alone frees - the same 0x1005F340/0x1005F370 pairing as shipped.
    unsigned char* buf = static_cast<unsigned char*>(::renddx9abi::engineAlloc(file.size));
    if (!buf)
    {
        ::renddx9abi::freeEngineFile(file);
        return (long)-2147024882;       // E_OUTOFMEMORY
    }
    ::memcpy(buf, file.data, file.size);
    const unsigned int size = file.size;
    ::renddx9abi::freeEngineFile(file);

    if (readBoolSetting("GSLoadObfuscated", true))
        deobfuscateShader(buf, size);

    *outData  = buf;
    *outBytes = size;
    return 0;
}

long __stdcall smIncludeClose(void* /*self*/, const void* data)
{
    if (!data)
        return (long)-2005529767;
    ::renddx9abi::engineFree(const_cast<void*>(data));
    return 0;
}

void* const kShaderManagerIncludeVtable[2] =
{
    reinterpret_cast<void*>(&smIncludeOpen),    // +0x00
    reinterpret_cast<void*>(&smIncludeClose),   // +0x04
};


// Device notifications visit cached effects and nested generated effects without null guards.
// Priority 5 releases device state first and restores it last.

// ID3DXEffect::OnLostDevice or ID3DXEffect::OnResetDevice.
typedef HRESULT (STDMETHODCALLTYPE ID3DXEffect::*EffectDeviceNotifyFn)();

inline ShaderManager* smFromListener(void* listener)
{
    // The DeviceLostResetListener face is the +0x08 subobject.
    return reinterpret_cast<ShaderManager*>(
        static_cast<unsigned char*>(listener) - offsetof(ShaderManager, vptrDeviceLost));
}

// The value map inside each +0x48 object: {_Left, _Parent, _Right, key, ID3DXEffect*, _Color,
// _Isnil} = 0x1C bytes. The successor is the VC7.1 one the bodies inline (testing +0x19).
// x86 only: the +0x48 objects are unrecovered and these are their retail x86 offsets (issue #7).
#if defined(_M_IX86)
struct EffectSubNode
{
    EffectSubNode* left;      // +0x00
    EffectSubNode* parent;    // +0x04
    EffectSubNode* right;     // +0x08
    unsigned int   key;       // +0x0C
    ID3DXEffect*   effect;    // +0x10
    unsigned char  pad14[4];  // +0x14
    unsigned char  color;     // +0x18
    unsigned char  isNil;     // +0x19
    unsigned char  pad1A[2];
};

static_assert(sizeof(EffectSubNode) == 0x1C, "the +0x9C map's node is 0x1C bytes");

EffectSubNode* effectSubNext(EffectSubNode* node)
{
    if (!node->right->isNil)
    {
        node = node->right;
        while (!node->left->isNil)
            node = node->left;
        return node;
    }
    EffectSubNode* parent = node->parent;
    while (!parent->isNil && node == parent->right)
    {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}
#endif

// The +0x48 value's map at +0x9C: head at +0xA0. `add ebx, 9Ch` then `cmp esi, [ebx+4]`.
void forEachSubEffect(void* object, EffectDeviceNotifyFn notify)
{
#if defined(_M_IX86)
    EffectSubNode* const head = *reinterpret_cast<EffectSubNode**>(
        static_cast<unsigned char*>(object) + 0xA0);
    for (EffectSubNode* node = head->left; node != head; node = effectSubNext(node))
        (node->effect->*notify)();
#else
    // [CHOSEN, not recovered] (issue #7): nothing recovered inserts into +0x48, and its objects'
    // layout is known only as retail x86 offsets, which do not describe a native object. x64 stops
    // here instead of walking a guessed layout.
    (void)notify;
    RD9_FRONTIER_AT("dice::hfe::dx9rend::ShaderManager - the +0x48 object's effect-map walk "
                    "(unrecovered object, retail x86 offsets only)", object, 0x00);
#endif
}

void forEachShaderManagerEffect(ShaderManager* sm, EffectDeviceNotifyFn notify)
{
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
    {
        ID3DXEffect* const d3dx = static_cast<ID3DXEffect*>(
            effectD3dxHandle(node->second));                                    // wrapper+0x08
        (d3dx->*notify)();
    }
    for (std::map<std::string, void*>::iterator node = sm->map48.begin();
         node != sm->map48.end(); ++node)
        forEachSubEffect(node->second, notify);
}

void RD9_THUNK sm_onDeviceLost(void* listener RD9_THUNK_EDX)
{
    forEachShaderManagerEffect(smFromListener(listener), &ID3DXEffect::OnLostDevice);
}

void RD9_THUNK sm_onDeviceReset(void* listener RD9_THUNK_EDX)
{
    forEachShaderManagerEffect(smFromListener(listener), &ID3DXEffect::OnResetDevice);
}

// shutdown only (the destroy broadcast, 0x10002600). The mapped objects' IBase
// release (`mov ecx, [node+28h]; mov eax, [ecx]; call [eax+8]`) over both maps; the nodes are left
// in place for the destructor that follows.
void RD9_THUNK sm_onDeviceDestroy(void* listener RD9_THUNK_EDX)
{
    ShaderManager* const sm = smFromListener(listener);
    typedef int (__thiscall* ReleaseFn)(void*);
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
    {
        void* const object = node->second;
        reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(object))[2])(object);
    }
    for (std::map<std::string, void*>::iterator node = sm->map48.begin();
         node != sm->map48.end(); ++node)
    {
        void* const object = node->second;
        reinterpret_cast<ReleaseFn>((*reinterpret_cast<void***>(object))[2])(object);
    }
}

void* const kShaderManagerListenerVtable[3] =
{
    reinterpret_cast<void*>(&sm_onDeviceLost),      // +0x00
    reinterpret_cast<void*>(&sm_onDeviceReset),     // +0x04
    reinterpret_cast<void*>(&sm_onDeviceDestroy),   // +0x08
};

} // anonymous namespace


// sub_10024A40 - the constructor. RendDX9Init.cpp:293.

bool createShaderManager(void* base, IDirect3DDevice9* device)
{
    void* const storage = ::renddx9abi::engineAlloc(sizeof(ShaderManager));
    if (!storage)
        return false;

    // The six containers, in the constructor's own order (0x10024A66..0x10024B50): sub_10024D40 for
    // the 0x30-node maps, sub_10024E70 for the 0x14-node one, sub_1001BA10 for the 0x48-node one,
    // with the std::string at +0x2C default-constructed between the second and third (0x10024AB9).
    // The member declaration order is that order. The effect cache used to be built by clearDefines,
    // on every define refresh - see the correction there.
    ShaderManager* sm = new (storage) ShaderManager;

    // Installs include and device-reset interfaces.
    // [FRONTIER] The primary table remains null until release/destruction is implemented.
    sm->vptr2 = const_cast<void**>(kShaderManagerIncludeVtable);
    sm->vptrDeviceLost = const_cast<void**>(kShaderManagerListenerVtable);

    sm->refCount = 1;

    sm->flag7E  = 1;
    sm->flag80  = 1;
    sm->flags84 = 0;

    device->AddRef();
    sm->device = device;

    // The three flag-to-bit transfers, in the body's order. All three sources are zero on a fresh
    // object, so none fires - reproduced anyway because they are what the constructor does.
    if (sm->flag7A)  sm->flags84 |= 0x20u;
    if (!sm->flag7B) reinterpret_cast<unsigned char*>(&sm->flags84)[1] |= 1u;
    if (sm->flag7C)  sm->flags84 |= 4u;

    sm->flag78 = isNvidia() ? 1 : 0;

    g_shaderManager = sm;

    updateShaderDefines(sm, base);

    ::renddx9abi::logf("ShaderManager: built at 0x%08X, device AddRef'd, isNvidia=%d",
                       ::renddx9abi::addr(sm), sm->flag78);
    return true;
}

// the expression initializeD3DSubsystems registers at 0x1000B78D..0x1000B7A2:
// `dword_1023B97C ? dword_1023B97C + 8 : 0`, the DeviceLostResetListener subobject. The null arm
// is shipped (it registers a null listener).
void* shaderManagerDeviceLostListener()
{
    return g_shaderManager ? &static_cast<ShaderManager*>(g_shaderManager)->vptrDeviceLost : 0;
}

// sub_10024A10 - init. On anything but nvidia it does nothing and returns 1. Reproduced whole.
bool shaderManagerInit(void* base)
{
    if (!g_shaderManager)
        return true;

    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);

    if (g_deviceCaps && isNvidia())
    {
        sm->flags84 |= 0x20u;
        sm->flag7A   = 1;
        sm->flag78   = 1;
        updateShaderDefines(sm, base);
    }
    return true;
}


// Returns a borrowed view of the macro strings used by the effect compiler.
ShaderManagerDefines shaderManagerDefines(void* shaderManager)
{
    ShaderManagerDefines out;
    out.first = 0;
    out.count = 0;
    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    if (sm && !sm->defines.empty())
    {
        out.first = &sm->defines[0];
        out.count = static_cast<unsigned int>(sm->defines.size());
    }
    return out;
}

const char* ShaderManagerDefines::at(unsigned int i) const
{
    return first ? first[i].c_str() : "";
}

// Returns the embedded ID3DXInclude interface that resolves files through mounted archives.
void* shaderManagerInclude(void* shaderManager)
{
    return shaderManager ? &static_cast<ShaderManager*>(shaderManager)->vptr2 : 0;   // +4
}

// +0xA4: a nonzero byte keeps freshly bound effects off the shared StateManager (Effect.cpp's
// bind and begin read it).
unsigned char shaderManagerStateManagerDisabled(void* shaderManager)
{
    return static_cast<ShaderManager*>(shaderManager)->disableStateManagerA4;
}

// +132 == +0x84, the D3DXSHADER_* flag word handed to D3DXCreateEffect.
unsigned long shaderManagerEffectFlags(void* shaderManager)
{
    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    return sm ? sm->flags84 : 0u;
}

// The device the compiler passes as pDevice: g_Renderer's base object at +0x0C.
void* renderDevice()
{
    if (!g_Renderer)
        return 0;
    return reinterpret_cast<RendDX9Base*>(g_Renderer)->device;
}

// Effect-cache path helpers. Keys are lowercased with the mod prefix stripped; loads restore
// that prefix and append .fx. Returned effects carry a caller reference.
// [FRONTIER] ShaderManager::getEffect: the secondary cache and failed-compile retry/event arm
// remain separate from the current primary-cache behavior.
namespace {

void toLowerAscii(char* text)
{
    for (char* c = text; *c; ++c)
        if (*c >= 'A' && *c <= 'Z')
            *c = static_cast<char>(*c - 'A' + 'a');
}

} // anonymous namespace

// sub_1001F8F0's caller-facing shape: a find in the effect cache with the same key normalisation
// getEffect uses, and NO compile on a miss. The QuadDrawer's frame bracket (its +0x14/+0x18)
// probes for "Menu" through this; on the recovered paths nothing ever caches that name, so the
// miss arm is the live one.
void* shaderManagerFindCachedEffect(const char* rawPath)
{
    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);
    if (!sm || !rawPath)
        return 0;

    char key[512];
    ::_snprintf_s(key, sizeof(key), _TRUNCATE, "%s", rawPath);
    toLowerAscii(key);

    std::map<std::string, Effect*>& cache = sm->effectCache;      // +0x14
    const std::map<std::string, Effect*>::iterator hit = cache.find(key);
    return hit != cache.end() ? hit->second : 0;
}

void* shaderManagerGetEffect(void* shaderManager, const char* rawPath)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::ShaderManager::getEffect", shaderManager, 0x00);

    ShaderManager* sm = static_cast<ShaderManager*>(shaderManager);
    if (!sm || !rawPath)
        return 0;

    // The cache key: the mod prefix stripped off, then lowercased.
    char key[512];
    if (::dice::hfe::fileManager)
    {
        const renddx9abi::HostString out = stripShaderModPath(rawPath);
        ::_snprintf_s(key, sizeof(key), _TRUNCATE, "%s", out.c_str());
    }
    else
    {
        ::_snprintf_s(key, sizeof(key), _TRUNCATE, "%s", rawPath);
    }
    toLowerAscii(key);

    std::map<std::string, Effect*>& cache = sm->effectCache;      // +0x14

    const std::map<std::string, Effect*>::iterator hit = cache.find(key);
    if (hit != cache.end())
    {
        void* cached = hit->second;
        if (cached)
        {
            void** v = *reinterpret_cast<void***>(cached);
            typedef void (__thiscall* AddRefFn)(void*);
            reinterpret_cast<AddRefFn>(v[0x00 / 4])(cached);
            ::renddx9abi::logf("ShaderManager::getEffect(\"%s\"): cache HIT -> 0x%08X", key,
                               ::renddx9abi::addr(cached));
            return cached;
        }
    }

    // A miss. The file name puts the mod prefix back and appends off_10238184[0].
    char file[512];
    ::_snprintf_s(file, sizeof(file), _TRUNCATE, "%s.fx", rawPath);

    Effect* effect = createEffectObject();
    if (!effect)
        return 0;

    void* d3dx = compileEffectFromFile(file);
    if (d3dx)
    {
        if (!effectBind(effect, d3dx))
            ::renddx9abi::logf("ShaderManager::getEffect(\"%s\"): compiled, but bind reported a "
                               "failure", file);
    }
    else
    {
        ::renddx9abi::logf("ShaderManager::getEffect(\"%s\"): the compile failed - the Effect is "
                           "cached UNBOUND, which is what the original does when its retry arm "
                           "also fails", file);
    }

    // AND A retail RUN found IT. The shipped miss path calls the effect's
    // vtbl+0x00 (addRef) before returning - `(v12)(v12)` at so the effect leaves
    // getEffect with refcount 2: ONE owned by the cache, ONE by the caller. Without it the first
    // caller to release (FSMoviePlayer's movie-failure cleanup) dropped the count to zero and hit
    // the unwritten ~Effect. With it that release goes 2 -> 1 and the destructor stays honestly
    // unreachable on every recovered path (later movie attempts are cache hits: addRef/release
    // around a stable 1).
    {
        void** v = *reinterpret_cast<void***>(effect);
        typedef void (__thiscall* AddRefFn)(void*);
        reinterpret_cast<AddRefFn>(v[0x00 / 4])(effect);
    }

    cache[key] = effect;

    ::renddx9abi::logf("ShaderManager::getEffect(\"%s\"): cached as \"%s\" -> effect 0x%08X "
                       "(d3dx 0x%08X), %u entries", file, key, ::renddx9abi::addr(effect),
                       ::renddx9abi::addr(d3dx), static_cast<unsigned int>(cache.size()));
    return effect;
}


// Clears texture bindings on every cached effect without changing the cache.
// Requires a live manager and valid cached effect pointers.

void shaderManagerClearEffectTextureBindings(void* shaderManager)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(shaderManager);
    for (std::map<std::string, Effect*>::iterator node = sm->effectCache.begin();
         node != sm->effectCache.end(); ++node)
    {
        effectClearTextureParameters(node->second);
    }
}


// setCurrentEffect - sub_100227A0, the call Effect::begin makes with the effect and Effect::end
// makes with null. Replacing a still-current effect with another logs the shipped
// ShaderManager.cpp:110 nesting complaint (severity 6) and takes the new one anyway; the effect's
// name string is the wrapper's +0x0C member.

void shaderManagerSetCurrentEffect(void* effect)
{
    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);
    if (!sm)
        return;
    if (sm->currentEffect98 && effect)
    {
        const std::string& prevName = effectName(sm->currentEffect98);
        ::renddx9abi::logf("ShaderManager.cpp:110 Previous effect: %s not ended.",
                           prevName.c_str());
    }
    sm->currentEffect98 = effect;
}


// The pre-draw CommitChanges walk - the first three calls of sub_10026880, in the shipped order:
// the raw ID3DXEffect at +0xA0, the current Effect wrapper at +0x98 (through its ID3DXEffect at
// wrapper+0x08), then the states-effect arm at +0x9C.
// Nothing on the recovered paths writes +0x9C or +0xA0, so their arms stay guarded; the +0x9C
// commit goes through sub_1001A0A0, which is unread - reaching it is a named frontier, not a
// fabricated call.

void shaderManagerCommitCurrentEffects()
{
    ShaderManager* sm = static_cast<ShaderManager*>(g_shaderManager);
    if (!sm)
        return;

    if (sm->rawEffectA0)
    {
        ID3DXEffect* fx = static_cast<ID3DXEffect*>(sm->rawEffectA0);
        fx->CommitChanges();
    }
    if (sm->currentEffect98)
    {
        ID3DXEffect* fx = static_cast<ID3DXEffect*>(
            effectD3dxHandle(sm->currentEffect98));                       // wrapper+0x08
        if (fx)
            fx->CommitChanges();
    }
    if (sm->statesEffect9C)
    {
        RD9_FRONTIER_AT("dice::hfe::dx9rend::ShaderManager - the states-effect commit arm "
                        "(sub_1001A0A0 is unread; nothing recovered writes +0x9C)", sm, 0x00);
    }
}

// sub_10022420, the active RA effect, kept at +0xA0 (`rawEffectA0` above): what
// RaShader::begin installs and RaShader::end clears, and the first of the three effects the
// pre-draw apply (sub_10026880) commits before every device draw - which is how a Ra shader's
// per-entry parameter sets reach the device.  Installing over a live one logs the attested
// ShaderManager.cpp:119 line and proceeds.  No null test on the manager,
// as shipped.
void shaderManagerSetActiveRaEffect(void* effect)
{
    ShaderManager* const sm = static_cast<ShaderManager*>(g_shaderManager);
    if (sm->rawEffectA0 && effect)
        ::renddx9abi::logf("ShaderManager.cpp:119 Previous effect not ended in RaShader.");
    sm->rawEffectA0 = effect;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
