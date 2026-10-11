// Ra effect compilation, shader definitions, cache and device-reset handling.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/PluginEntry.h"   // dice::hfe::fileManager - the +0x38 getFileInfo probe
#include "../../../RendDX9Abi/Vc71Map.h"       // the 0x30-byte VC7.1 map the +0x54 MetaFx cache uses
#include "../../../RendDX9Abi/D3dx9Sdk.h"
#include "../RendDX9Base.h"
#include "../../RendMisc/IRenderer.h"   // Renderer +0xBC, typed (see below)
#include "../../Game/Main/IVideoOptions.h"
// FOR sub_10033740 only, and it is a read, not a policy change. The block at the
// top of this file explains that the two translation units deliberately meet at the linker; that is
// still true for metaFxConstruct/metaFxParse, which stay externs. sub_10033740 is different: it
// walks the MetaFx cache and drives ONE MetaFx through its public surface (getGeneratedNames() and
// generateShader()), and there is no way to do that through an extern without defining a new body
// in Ra/RaShaderPermutator.cpp - a file this sitting may not edit. Nothing below indexes a MetaFx
// by offset and no private member is touched.
#include "RaShaderPermutator.h"
#include "RaShader.h"          // RaShaderTable + findOrCreateShader, for the load pass

#include <stdio.h>
#include <string.h>
#include <new>
#include <string>
#include <list>
#include <regex>

namespace dice {
namespace hfe {

extern IBase* fileChangeManager; // PluginEntry.cpp, PE 0x10244AC8

// PluginEntry.cpp, PE 0x10244AB8. initDll fetches it but does NOT require it, so it can legally be
// null here; the shipped compileEffect dereferences it with no guard. See the note on the progress
// bracket in compileEffect for what this build does instead.
extern IBase* loadStat;

namespace dx9rend {

// From RendDX9Init.cpp - the DeviceCaps this file interrogates, and the base object.
extern void* g_deviceCaps;
extern void* g_rendererBase;

// BF2Render.cpp, dword_1023D098 - the Renderer singleton (`g_Renderer = this` in its ctor). The
// MetaFx loader guards the path fixup on it and the fixup dispatches its xpack test through it.
// Distinct from g_rendererBase (dword_1023B970); do not substitute one for the other.
extern IBase* g_Renderer;

// RenderQueues.cpp, dword_1023E408 - the 184-byte render-queue table. getMetaFx reads exactly ONE
// byte of it, the +0xB4 flag its constructor sets to 1, and hands it to the loader.
extern void* g_renderQueues;

// StateManager.cpp, dword_1023B984. getEffect hands it to every effect it compiles through
// ID3DXEffect::SetStateManager (+0x11C) - the same attested slot Effect::bind uses.
extern void* g_stateManager;

// Shared in-place shader deobfuscation.
void deobfuscateShader(unsigned char* buf, unsigned int size);

namespace Ra {


// MetaFx construction and parsing are implemented in RaShaderPermutator.cpp.
// Its constructor takes the path stem, extension and mode flag.

class MetaFx;

// The size of the object `metaFxConstruct` builds in `storage`. It is NOT the shipped literal
// 0xC0. Retail measures 0xC0 with a 16-byte std::vector and a 12-byte std::map; this toolchain's
// are 12 and 8, so the same members measure less here. Asking the owning translation unit keeps the
// allocation and the object in agreement on whatever toolchain builds this DLL, which a hard-coded
// 0xC0 would not. See the seam at the bottom of Ra/RaShaderPermutator.cpp.
unsigned int metaFxObjectSize();

// PE 0x1003E110. `storage` is the engine allocation sized by the call above; returns it.
MetaFx* metaFxConstruct(void* storage,
                        const vc71::string& stem,
                        const vc71::string& extension,
                        bool flag);

// PE 0x1003EEF0 - `do { sub_1003EAB0(this); } while (!sub_1003E3B0(this));`, the permutation parse
// loop. __thiscall, no arguments.
void metaFxParse(MetaFx* self);


// Reads device capabilities from the D3DCAPS9 record at offset zero.

namespace {

struct CapsView
{
    D3DCAPS9               caps;
    D3DADAPTER_IDENTIFIER9 adapter;
};

const CapsView* capsOf()      { return static_cast<const CapsView*>(g_deviceCaps); }

bool         isNvidia()       { const CapsView* c = capsOf(); return c && c->adapter.VendorId == 4318u; }
bool         isAti()          { const CapsView* c = capsOf(); return c && c->adapter.VendorId == 4098u; }
unsigned int vsMajor()        { const CapsView* c = capsOf(); return c ? ((c->caps.VertexShaderVersion >> 8) & 0xFF) : 0u; }
unsigned int psMajor()        { const CapsView* c = capsOf(); return c ? ((c->caps.PixelShaderVersion  >> 8) & 0xFF) : 0u; }
// sub_100010E0 - `*(BYTE*)(this + 204)`, byte 0 of D3DCAPS9.PixelShaderVersion: the minor number.
// The compile search wants the pair as one 0xMMmm word, which is how 0x100354EF..0x10035504 builds
// it (`major << 8 | minor`).
unsigned int psMinor()        { const CapsView* c = capsOf(); return c ? (c->caps.PixelShaderVersion & 0xFF) : 0u; }
// sub_10001450 - `*(DWORD*)(this + 264) & 1`. Byte 264 of D3DCAPS9 is PS20Caps.Caps, and bit 0 is
// the documented D3DPS20CAPS_ARBITRARYSWIZZLE.
bool ps20ArbitrarySwizzle()   { const CapsView* c = capsOf(); return c && (c->caps.PS20Caps.Caps & 1u) != 0; }
unsigned int maxAnisotropy()  { const CapsView* c = capsOf(); return c ? c->caps.MaxAnisotropy : 0u; }
bool         minAniso()       { const CapsView* c = capsOf(); return c && (c->caps.TextureFilterCaps & 0x400u) != 0; }
bool         magAniso()       { const CapsView* c = capsOf(); return c && (c->caps.TextureFilterCaps & 0x4000000u) != 0; }

// The list sentinels, identical in shape to BufferManager.cpp's - see the node-size rule there
// (size == flag offset + 3, confirmed against two read allocators).
//     sub_10036120  flag 45  -> 48   [INFERRED] by the rule
//     sub_10036250  flag 69  -> 72   [INFERRED] by the rule
//     sub_100362F0  flag 41  -> 44   [INFERRED] by the rule
unsigned int* allocSentinel(unsigned int size, unsigned int flagOffset)
{
    unsigned char* node = static_cast<unsigned char*>(::renddx9abi::engineAlloc(size));
    if (!node)
        return 0;
    for (unsigned int i = 0; i < size; ++i)
        node[i] = 0;
    node[flagOffset - 1] = 1;
    node[flagOffset]     = 1;
    unsigned int* w = reinterpret_cast<unsigned int*>(node);
    const unsigned int self = reinterpret_cast<unsigned int>(node);
    w[0] = self; w[1] = self; w[2] = self;
    return w;
}

struct RaList { void* alloc; unsigned int* head; unsigned int count; };

void initList(RaList& l, unsigned int nodeSize, unsigned int flagOffset)
{
    l.alloc = 0;
    l.head  = allocSentinel(nodeSize, flagOffset);
    l.count = 0;
}

// Reads the shared vendor-probe transparency-AA code as a 32-bit value.
inline unsigned int transparencyAACode(const unsigned char (&code)[4])
{
    unsigned int value;
    ::memcpy(&value, code, sizeof(value));
    return value;
}


// Include resolver and file-change listener owned by EffectManager.
// Tracks watched names case-sensitively and keeps the current shader's fully expanded text.

struct EffectSubObject
{
    void*        vptr;        // +0x00  &off_101E139C - the ID3DXInclude table, installed below
    void*        vptr2;       // +0x04  off_101E1388
    int          refCount;    // +0x08  1
    RaList       list;        // +0x0C  44-byte nodes - the registered-watch name set
    vc71::string text;        // +0x18  "Generated.fx"
};

static_assert(sizeof(EffectSubObject) == 0x34, "sub_10035A00 allocates 0x34");

// Defined at the bottom of this file, beside the two bodies it points at; declared
// here because the constructor installs it and the constructor is above them.
void* raIncludeVtable();

// off_101E143C, the DeviceLostResetListener at EffectManager+0x04. Defined
// after resetEffects, which its lost slot tail-jumps to.
void* effectManagerListenerVtable();

} // anonymous namespace


// dice::hfe::dx9rend::Ra::EffectManager - 168 bytes (0xA8), ctor sub_10035A60 (0x10035A60).
// Every offset below is one the constructor writes.

class EffectManager
{
public:
    explicit EffectManager(IDirect3DDevice9* device, bool disableShaderCache);

    void rebuildShaderDefines();   // sub_10033240, EffectManager.cpp:438
    void rebuildDebugDefines();
    void setShowChannel(unsigned int channel);

    // The MetaFx cache at +0x54. See the block after the class bodies for the evidence.
    MetaFx* getMetaFx(const vc71::string& name);
    // THE name IS taken BY value, and that is read from the epilogue (`retn 20h` = a 28-byte
    // std::string plus the flag dword), not guessed. The callee destroys it.
    MetaFx* loadMetaFx(vc71::string name, bool flag);

    const vc71::string& shaderDefines() const { return m_shaderDefines; }

    // The single byte ShaderManager_updateShaderDefines reads.
    bool flag40() const { return m_flag40 != 0; }

    // the in-memory shader-file table at +0x9C. The permutator writes
    // each generated .fx into it under a lowercased name and drops them again afterwards, which is
    // how a generated permutation reaches the D3DX include handler without ever touching disk.
    bool addMemoryFile(const vc71::string& name, const vc71::string& contents);
    void removeMemoryFile(const vc71::string& name);


    // sub_10035900, `retn 4`. The one call the RaShader build makes.
    void* getEffect(const vc71::string& name);

    // sub_10034F60, `retn 0Ch`. The compile, including the .cfx disk cache.
    void* compileEffect(const vc71::string& name, int unused, void** effectOut);

    // Compiles source into an ID3DXBuffer containing effect bytecode.
    ID3DXBuffer* compileEffectSource(const char* source, unsigned int sourceLen,
                                     const D3DXMACRO* defines, unsigned long* flags,
                                     ID3DXBuffer** errors);

    // sub_10033DB0, `retn 8`. Reads one effect file whole into an engine-heap, NUL-terminated
    // buffer. Falls back to the in-memory table and then to generating the permutation.
    unsigned int readEffectFile(const vc71::string& name, unsigned char** bufferOut);

    // sub_10033AA0, `retn 4`. The in-memory table lookup; null on a miss.
    const vc71::string* findMemoryFile(const vc71::string& name);

    // sub_100336C0, `retn 0`. Re-parse every cached MetaFx while the memory table is empty.
    void refillMemoryFiles();

    // sub_10033740, `retn 4`. Generate the one permutation whose clean name matches.
    bool generateMissingShader(const vc71::string& name);

    // sub_10033170, `retn 8`. Write a compiled blob to the .cfx cache, index-XORed.
    bool writeEffectCache(ID3DXBuffer* blob, const vc71::string& path);

    // The include handler needs these three; it runs as a free __stdcall function on m_sub.
    const vc71::string& debugDefines() const { return m_debugDefines; }
    EffectSubObject*    includeHandler() const { return m_sub; }

    // Resets cached effects without changing the selected debug channel.
    void resetEffects();

private:

    // m_list2C reinterpreted as the VC7.1 std::map header it is. THE comparator IS `_stricmp`,
    // not the case-sensitive ordering the +0x54 cache uses: sub_10036390 (find) and sub_100372D0
    // (operator[]) both push their two c_str()s straight into MSVCR71's `_stricmp`. Vc71Map.h's
    // mapFindInsensitive/mapInsertInsensitive are the twins that match, and its plain mapFind is
    // the wrong one here.
    vc71::map_layout& effectCache();

    // m_list54 reinterpreted as the VC7.1 std::map header it is. See the MetaFx-cache block below
    // for why the shared 0x30-node helpers are the right ones for this map and not for the others.
    vc71::map_layout& metaFxCache();

    void*            m_vptr;          // +0x00  off_101E1448
    void*            m_vptr2;         // +0x04  off_101E143C - DeviceLostResetListener
    int              m_refCount;      // +0x08  1
    // Most recently requested effect and its lowercase name, updated on hits and misses.
    vc71::string     m_name;          // +0x0C
    void*            m_lastEffect;    // +0x28
    RaList           m_list2C;        // +0x2C  48-byte nodes - THE effect cache
    IDirect3DDevice9* m_device;       // +0x38
    // +0x3C is D3DXCreateEffect's pPool. The constructor zeroes it and nothing in the image ever
    // writes it, so every Ra effect is created with a NULL effect pool - which is legal and means
    // no cross-effect parameter sharing.
    void*            m_effectPool;    // +0x3C
    unsigned char    m_flag40;        // +0x40  <- the byte ShaderManager reads
    unsigned char    m_flag41;        // +0x41
    unsigned char    m_pad42[2];
    EffectSubObject* m_sub;           // +0x44

    // Eight debug toggles, in the order rebuildDebugDefines emits them.
    unsigned char    m_disableDiffuseMap;   // +0x48
    unsigned char    m_disableBumpMap;      // +0x49
    unsigned char    m_disableSpecular;     // +0x4A
    unsigned char    m_disableSpecularMap;  // +0x4B
    unsigned char    m_disableLightmap;     // +0x4C
    unsigned char    m_enableWireframe;     // +0x4D
    unsigned char    m_disableVsPs;         // +0x4E
    unsigned char    m_disableVsPs20;       // +0x4F

    unsigned int     m_showChannel;   // +0x50
    RaList           m_list54;        // +0x54  48-byte nodes
    unsigned char    m_byte60;        // +0x60  GSDisableShaderCache
    unsigned char    m_byte61;        // +0x61
    unsigned char    m_byte62;        // +0x62  ctor: 1
    unsigned char    m_pad63;
    vc71::string     m_debugDefines;  // +0x64
    vc71::string     m_shaderDefines; // +0x80
    RaList           m_list9C;        // +0x9C  72-byte nodes
};

static_assert(sizeof(EffectManager) == 0xA8,
              "initializeD3DSubsystems allocates 168 for it");

EffectManager::EffectManager(IDirect3DDevice9* device, bool disableShaderCache)
    : m_vptr(0)
    , m_vptr2(0)
    , m_refCount(1)
    , m_lastEffect(0)
    , m_device(device)
    , m_effectPool(0)
    , m_flag40(0)
    , m_flag41(0)
    , m_sub(0)
    , m_disableDiffuseMap(0)
    , m_disableBumpMap(0)
    , m_disableSpecular(0)
    , m_disableSpecularMap(0)
    , m_disableLightmap(0)
    , m_enableWireframe(0)
    , m_disableVsPs(0)
    , m_disableVsPs20(0)
    , m_showChannel(0)
    , m_byte60(0)
    , m_byte61(0)
    , m_byte62(1)
    , m_pad63(0)
{
    m_pad42[0] = 0;
    m_pad42[1] = 0;

    initList(m_list2C, 48, 45);
    initList(m_list54, 48, 45);
    initList(m_list9C, 72, 69);

    m_sub = static_cast<EffectSubObject*>(::renddx9abi::engineAlloc(sizeof(EffectSubObject)));
    if (m_sub)
    {
        // D3DX calls this include-handler table while compiling shader #include directives.
        m_sub->vptr     = raIncludeVtable();

        // off_101E1388, the second table, is still null. All six of its bodies are read (see the
        // block above EffectSubObject) but the only thing that dispatches on it is the
        // FileChangeManager watch, whose registration is withheld under RW-78 exactly as
        // Ra/RaShaderPermutator.cpp's and TextureManager's are. Installing a table nothing calls
        // would claim a registration that is not made.
        // [FRONTIER] dice::hfe::dx9rend::Ra::EffectManager - the include handler's IBase table
        m_sub->vptr2    = 0;
        m_sub->refCount = 1;
        initList(m_sub->list, 44, 41);
        new (&m_sub->text) vc71::string();
    }

    // The setting the constructor reads. The original stores it into the byte at +0x60.
    m_byte60 = disableShaderCache ? 1 : 0;

    // Installs the device-reset listener, which forwards resetEffects to its owner.
    // [FRONTIER] The primary interface remains null until its destructor is implemented;
    // file-change registration and shutdown deletion must remain deferred.
    m_vptr2 = effectManagerListenerVtable();

    rebuildShaderDefines();
    rebuildDebugDefines();
}


// complete - eight bools and a channel enum, appended to the string at
// +0x64. Self-contained; no caps, no device, no settings.

void EffectManager::rebuildDebugDefines()
{
    m_debugDefines = "";

    if (m_disableDiffuseMap)  m_debugDefines += "#define DISABLE_DIFFUSEMAP\n";
    if (m_disableBumpMap)     m_debugDefines += "#define DISABLE_BUMPMAP\n";
    if (m_disableSpecular)    m_debugDefines += "#define DISABLE_SPECULAR\n";
    if (m_disableSpecularMap) m_debugDefines += "#define DISABLE_SPECULARMAP\n";
    if (m_disableLightmap)    m_debugDefines += "#define DISABLE_LIGHTMAP\n";
    if (m_enableWireframe)    m_debugDefines += "#define ENABLE_WIREFRAME\n";
    if (m_disableVsPs)        m_debugDefines += "#define DISABLE_VSPS\n";
    if (m_disableVsPs20)      m_debugDefines += "#define DISABLE_VSPS20\n";

    if (m_showChannel)
    {
        m_debugDefines += "#define SHOW_CHANNEL\n";
        // SIX separate `if`s IN THE original, NOT A switch, and the last one returns while the
        // others fall through. Functionally identical because the values are distinct; reproduced
        // as a chain rather than tidied into a switch.
        if (m_showChannel == 1) m_debugDefines += "#define DIFFUSE_CHANNEL\n";
        if (m_showChannel == 2) m_debugDefines += "#define NORMAL_CHANNEL\n";
        if (m_showChannel == 3) m_debugDefines += "#define SPECULAR_CHANNEL\n";
        if (m_showChannel == 4) m_debugDefines += "#define ALPHA_CHANNEL\n";
        if (m_showChannel == 5) m_debugDefines += "#define ENVIRONMENT_CHANNEL\n";
        if (m_showChannel == 6) m_debugDefines += "#define SHADOW_CHANNEL\n";
    }
}


// EffectManager.cpp:438. complete except the two A2M values.

void EffectManager::rebuildShaderDefines()
{
    RendDX9Base* base = reinterpret_cast<RendDX9Base*>(g_rendererBase);
    char line[128];

    m_shaderDefines = "";

    m_shaderDefines += isNvidia() ? "#define NVIDIA 1\n" : "#define NVIDIA 0\n";
    m_shaderDefines += isAti()    ? "#define ATI 1\n"    : "#define ATI 0\n";

    // A shader-model level requires support from both vertex and pixel shaders.
    m_shaderDefines += (psMajor() < 3 || vsMajor() < 3) ? "#define SM30 0\n" : "#define SM30 1\n";
    m_shaderDefines += (psMajor() < 2 || vsMajor() < 2) ? "#define SM20 0\n" : "#define SM20 1\n";

    // base[435] - byte 1740, the same field updateShaderDefines prints as rapath.
    const unsigned int raPath =
        base ? *reinterpret_cast<const unsigned int*>(reinterpret_cast<const unsigned char*>(base) + 1740) : 0u;
    ::sprintf_s(line, "#define RAPATH %i \n", raPath);
    m_shaderDefines += line;

    // base[1302] gates it alongside the multisample check - see hasAlpha2Mask in ShaderManager.cpp.
    const unsigned char* rawBase = reinterpret_cast<const unsigned char*>(base);
    bool a2m = false;
    if (base)
    {
        const bool multisampleOk =
            base->multiSampleFirst && ((base->multiSampleLast - base->multiSampleFirst) >= 2)
                ? (*base->multiSampleFirst > 0) : false;
        a2m = (rawBase[1300] || (!rawBase[1301] && multisampleOk)) && rawBase[1302];
    }
    ::sprintf_s(line, "#define HASALPHA2MASK %i \n", a2m ? 1 : 0);
    m_shaderDefines += line;

    // "AdaptiveTess_Y" is nvidia's documented alpha-to-coverage trigger - a magic FOURCC written
    // into a render state. On anything else the define falls back to PointSize, which is the same
    // trick played through a different state.
    m_shaderDefines += isNvidia() ? "#define Alpha2Mask AdaptiveTess_Y \n"
                                  : "#define Alpha2Mask PointSize \n";

    // the shared transparency-AA codes, see transparencyAACode above.
    ::sprintf_s(line, "#define A2MEnable 0x%X \n", transparencyAACode(g_transparencyAAOn));
    m_shaderDefines += line;
    ::sprintf_s(line, "#define A2MDisable 0x%X \n", transparencyAACode(g_transparencyAAOff));
    m_shaderDefines += line;

    // base->vslot+0x74 is `return this + 1016` - the VideoOptions inside the base object - and
    // VideoOptions' vslot+0x04 is `return this[index + 10]`. Index 7 is the texture filter quality.
    unsigned int filterQuality = 0;
    if (base)
    {
        const unsigned int* videoOptions =
            reinterpret_cast<const unsigned int*>(rawBase + 1016);
        filterQuality = videoOptions[7 + 10];
    }

    vc71::string stmNormMip("#define FILTER_STM_NORM_MIP LINEAR \n");
    vc71::string bmNormMip ("#define FILTER_BM_NORM_MIP LINEAR \n");

    switch (filterQuality)
    {
    case 1:
        stmNormMip = "#define FILTER_STM_NORM_MIP POINT \n";
        // falls into case 2 - the original jumps into the middle of it
    case 2:
        bmNormMip = "#define FILTER_BM_NORM_MIP POINT \n";
        m_shaderDefines += "#define FILTER_STM_DIFF_MIN LINEAR \n";
        m_shaderDefines += "#define FILTER_STM_DIFF_MAG LINEAR \n";
        m_shaderDefines += "#define FILTER_BM_DIFF_MIN LINEAR \n";
        m_shaderDefines += "#define FILTER_BM_DIFF_MAG LINEAR \n";
        break;

    case 3:
    {
        // Bundled and static meshes use anisotropy limits of four and two respectively.
        unsigned int anisoBm  = maxAnisotropy(); if (anisoBm  >= 4) anisoBm  = 4;
        unsigned int anisoStm = maxAnisotropy(); if (anisoStm >= 2) anisoStm = 2;

        const char* minF = minAniso() ? "ANISOTROPIC" : "LINEAR";
        const char* magF = magAniso() ? "ANISOTROPIC" : "LINEAR";

        ::sprintf_s(line, "#define FILTER_STM_DIFF_MIN %s \n", minF);          m_shaderDefines += line;
        ::sprintf_s(line, "#define FILTER_STM_DIFF_MAG %s \n", magF);          m_shaderDefines += line;
        ::sprintf_s(line, "#define FILTER_STM_DIFF_MAX_ANISOTROPY %i\n", anisoStm); m_shaderDefines += line;
        ::sprintf_s(line, "#define FILTER_BM_DIFF_MIN %s \n", minF);           m_shaderDefines += line;
        ::sprintf_s(line, "#define FILTER_BM_DIFF_MAG %s \n", magF);           m_shaderDefines += line;
        ::sprintf_s(line, "#define FILTER_BM_DIFF_MAX_ANISOTROPY %i\n", anisoBm);   m_shaderDefines += line;
        break;
    }

    default:
        // EffectManager.cpp:438. AND IT does NOT return - the two NORM_MIP lines below are still
        // appended, with their linear defaults. Same shape as BF2Render.cpp:761's "aborting."
        ::renddx9abi::logf("EffectManager.cpp:438 Undefined texture filter quality. (got %u; "
                           "VideoOptions is not initialised on any written path, so this is "
                           "expected here rather than a bad setting)", filterQuality);
        break;
    }

    m_shaderDefines += stmNormMip;
    m_shaderDefines += bmNormMip;
}

namespace {

// The retail PE orders the FileChangeManager overloads as +0x14 remove(listener) and +0x18
// remove(listener,name).  The latter is the instruction-proven call in EffectManager::resetEffects.
class IFileChangeManagerPe : public IBase
{
public:
    virtual void slot10() = 0; // addListener; not called here
    virtual bool removeListener(IBase* listener) = 0; // +0x14
    virtual bool removeListener(IBase* listener,
                                const renddx9abi::HostString& name) = 0; // +0x18
};

struct EffectNode48
{
    EffectNode48* left;
    EffectNode48* parent;
    EffectNode48* right;
    unsigned char key[28];
    IBase* effect;
    unsigned char color;
    unsigned char isNil;
    unsigned char pad[2];
};

struct EffectNode72
{
    EffectNode72* left;
    EffectNode72* parent;
    EffectNode72* right;
    unsigned char key[28];
    unsigned char value[28];
    unsigned char color;
    unsigned char isNil;
    unsigned char pad[2];
};

static_assert(sizeof(EffectNode48) == 48, "EffectManager cache node layout drift");
static_assert(sizeof(EffectNode72) == 72, "EffectManager secondary cache node layout drift");

EffectNode48* nextEffectNode(EffectNode48* node)
{
    if (!node->right->isNil)
    {
        node = node->right;
        while (!node->left->isNil)
            node = node->left;
        return node;
    }
    EffectNode48* parent = node->parent;
    while (!parent->isNil && node == parent->right)
    {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

void destroyEffectNodes(EffectNode48* node)
{
    // sub_100369E0: recurse right, destroy/free the current node, continue down its left arm.
    while (!node->isNil)
    {
        destroyEffectNodes(node->right);
        EffectNode48* const left = node->left;
        reinterpret_cast<vc71::string*>(node->key)->~string();
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

void destroyEffectNodes72(EffectNode72* node)
{
    // sub_10036A80 destroys the mapped string first, then the key string, in this order.
    while (!node->isNil)
    {
        destroyEffectNodes72(node->right);
        EffectNode72* const left = node->left;
        reinterpret_cast<vc71::string*>(node->value)->~string();
        reinterpret_cast<vc71::string*>(node->key)->~string();
        ::renddx9abi::engineFree(node);
        node = left;
    }
}

} // anonymous namespace

// The reset is deliberately broad: shader/debug define strings, every material
// queue's compiled effect, every cached Effect wrapper/watch, and the secondary string cache.
void EffectManager::resetEffects()
{
    rebuildDebugDefines();
    rebuildShaderDefines();
    renderQueuesResetEffects();

    EffectNode48* const head = reinterpret_cast<EffectNode48*>(m_list2C.head);
    for (EffectNode48* node = head->left; node != head; node = nextEffectNode(node))
    {
        if (node->effect)
        {
            static_cast<ID3DXEffect*>(static_cast<void*>(node->effect))->Release();
            node->effect = 0;
        }

        const vc71::string& key = *reinterpret_cast<const vc71::string*>(node->key);
        const renddx9abi::HostString hostKey = renddx9abi::makeHostString(key);
        static_cast<IFileChangeManagerPe*>(
            static_cast<void*>(::dice::hfe::fileChangeManager))->removeListener(
                reinterpret_cast<IBase*>(this), hostKey);
    }

    destroyEffectNodes(head->parent);
    head->parent = head;
    head->left = head;
    head->right = head;
    m_list2C.count = 0;

    EffectNode72* const secondaryHead = reinterpret_cast<EffectNode72*>(m_list9C.head);
    destroyEffectNodes72(secondaryHead->parent);
    secondaryHead->parent = secondaryHead;
    secondaryHead->left = secondaryHead;
    secondaryHead->right = secondaryHead;
    m_list9C.count = 0;
}

namespace {


// Device loss rebuilds defines and drops compiled effects/watches; reset does nothing.
// Subsequent draws compile effects again, using valid disk caches when available.

void __fastcall em_onDeviceLost(void* listener, void*)
{
    reinterpret_cast<EffectManager*>(static_cast<unsigned char*>(listener) - 4)->resetEffects();
}

void __fastcall em_listenerNullsub(void*, void*)
{
    // 0x10037D40 - `ret`, empty in the original too.
}

void* const kEffectManagerListenerVtable[3] =
{
    reinterpret_cast<void*>(&em_onDeviceLost),      // +0x00
    reinterpret_cast<void*>(&em_listenerNullsub),   // +0x04
    reinterpret_cast<void*>(&em_listenerNullsub),   // +0x08
};

void* effectManagerListenerVtable()
{
    return const_cast<void**>(kEffectManagerListenerVtable);
}

} // anonymous namespace

void EffectManager::setShowChannel(unsigned int channel)
{
    if (m_showChannel != channel)
    {
        m_showChannel = channel;
        resetEffects();
    }
}


// MetaFx cache nodes have a 48-byte compatibility layout, distinct from the other
// effect-manager maps. Do not share their node allocation or nil-byte accessors.
// The compatibility tree preserves key order but currently omits red-black balancing.


namespace {

static_assert(sizeof(RaList) == sizeof(vc71::map_layout),
              "the +0x54 cache header is the shipped 12-byte VC7.1 std::map {comp, head, size}");


// Compatibility FileInfo needs a live VC7.1 string and the full 64-byte output record,
// even though this path only consumes getFileInfo's Boolean result.

#if !BF2_PLUGIN_ABI_NATIVE
struct PeFileInfo
{
    vc71::string  name;          // +0x00
    unsigned char isDirectory;   // +0x1C
    unsigned char pad1[3];
    unsigned int  size;          // +0x20
    unsigned int  pad2;          // +0x24  MSVC 8-aligns the filetime block that follows
    unsigned int  times[6];      // +0x28  creation / last access / last write, low dword first
};

static_assert(sizeof(PeFileInfo) == 64, "the PE io::FileInfo the shipped frame reserves");
#endif

bool fileManagerGetFileInfo(const vc71::string& path)
{
    // [CHOSEN, not recovered] Permit loading before the host publishes FileManager.
    if (!::dice::hfe::fileManager)
        return false;

    const renddx9abi::HostString hostPath = renddx9abi::makeHostString(path);

#if BF2_PLUGIN_ABI_NATIVE
    ::dice::hfe::io::FileInfo info;
    return ::dice::hfe::fileManager->getFileInfo(hostPath, info);
#else
    PeFileInfo info;
    void** const vtbl = *reinterpret_cast<void***>(::dice::hfe::fileManager);
    typedef unsigned char (__thiscall* GetFileInfoFn)(void*, const renddx9abi::HostString&,
                                                      PeFileInfo&);
    return reinterpret_cast<GetFileInfoFn>(vtbl[0x38 / 4])(
               ::dice::hfe::fileManager, hostPath, info) != 0;
#endif
}


// Prefixes unqualified shader paths; a null renderer selects xpack.
// [CHOSEN, not recovered] Scratch strings are local rather than shared static storage.

void applyModPrefix(vc71::string& path)
{
    if (::strstr(path.c_str(), "mods/"))
        return;

    bool xpack = true;                 // the shipped default when g_Renderer is null
    if (g_Renderer)
    {
        xpack = static_cast<IRenderer*>(g_Renderer)->isExpansionPack();
    }

    vc71::string prefixed(xpack ? "mods/xpack/" : "mods/bf2/");
    prefixed += path;
    path = prefixed;
}

} // anonymous namespace

vc71::map_layout& EffectManager::metaFxCache()
{
    // RaList and vc71::map_layout are the same twelve bytes in the same order. The cast keeps the
    // shipped member declaration - which is what fixes every later offset in this 168-byte class -
    // while letting the audited shared helpers do the walking.
    return *reinterpret_cast<vc71::map_layout*>(&m_list54);
}

// Accesses the string-keyed effect cache using its compatibility map layout.
vc71::map_layout& EffectManager::effectCache()
{
    return *reinterpret_cast<vc71::map_layout*>(&m_list2C);
}


// Returns a cached MetaFx by lower-case key, loading with the original name on a miss.

MetaFx* EffectManager::getMetaFx(const vc71::string& name)
{
    // sub_10074E80 is `std::string toLower(const std::string&)` - a copy, then the in-place
    // lowercase at sub_10074D10 that this tree already carries as pathToLower.
    vc71::string key(name);
    pathToLower(key);

    // sub_10036410 is std::map::find: a lower_bound (sub_101C0AD0) and then "is the probe less
    // than what we landed on", which selects between the node and end(). Its caller compares the
    // result against *(this + 0x58) - the head - so a miss is end(); mapFind answers the same
    // question with a null.
    const vc71::map_node* const node = vc71::mapFind(metaFxCache(), key.c_str());
    if (node)
        return static_cast<MetaFx*>(vc71::mapNodeValue(node));

    // Requires the render-queue table published by initializeD3DSubsystems.
    const unsigned char flag = *(static_cast<const unsigned char*>(g_renderQueues) + 0xB4);

    return loadMetaFx(name, flag != 0);
}


// Constructs and caches a MetaFx; returns null if it is already cached.
// Use getMetaFx to retrieve existing entries. Diagnostics use the resolved path.

MetaFx* EffectManager::loadMetaFx(vc71::string name, bool flag)
{
    {
        // The re-check, on the same lowered key and the same map as getMetaFx.
        vc71::string key(name);
        pathToLower(key);
        if (vc71::mapFind(metaFxCache(), key.c_str()))
            return 0;                       // null, NOT the cached MetaFx - see above
    }

    // THE fixup IS guarded ON g_Renderer BY THE caller even though sub_10029D00 handles a null
    // g_Renderer itself. Both tests are shipped; both are kept.
    vc71::string path(name);
    if (g_Renderer)
        applyModPrefix(path);

    if (!fileManagerGetFileInfo(path))      // FileManager vslot +0x38
    {
        ::renddx9abi::logf("EffectManager.cpp:483: Can't find metaShader file: %s", path.c_str());
        return 0;
    }

    MetaFx* metaFx = 0;

    // sub_1005F320(0xC0) in the image; see metaFxObjectSize's note on why the literal is not used.
    void* const storage = ::renddx9abi::engineAlloc(metaFxObjectSize());
    if (storage)
    {
        // Derive extension before stem from the resolved path.
        vc71::string extension;
        pathGetExtension(extension, path);                   // sub_10075180 -> sub_10075580
        vc71::string stem;
        pathStripExtension(stem, path);                      // sub_100753D0 -> sub_10075A50

        // AND THE boolean IS inverted ON THE WAY IN. 0x10033C4E is `cmp [ebp+arg_1C], 0` -
        // a byte compare - followed by `setz al`, so the constructor receives the negation of the
        // byte getMetaFx read out of the render-queue table.
        metaFx = metaFxConstruct(storage, stem, extension, !flag);
    }
    // NO free() ON THE failure ARM, because there is no failure arm: a null allocation skips the
    // constructor entirely (0x10033C4C) and the two strings are never built, which is what the
    // `ebx = 3` / `ebx = var_4` destructor-flag bookkeeping at 0x10033C5B and 0x10033C7E encodes.

    if (metaFx)
    {
        // sub_10074E80 a second time - the first lowered key was destroyed with the re-check.
        vc71::string key(name);
        pathToLower(key);

        // sub_100367F0 - std::map::insert(value_type(key, metaFx)). The pair is assembled on the
        // stack at 0x10033CB5 (the string) and 0x10033CC1 (the pointer 28 bytes later), copied
        // once more at 0x10033CD4, and handed over by reference. The key cannot already be present
        // here: the re-check above returned on a hit.
        vc71::map_node* const node = vc71::mapInsert(metaFxCache(), 0x30, key.c_str());
        if (node)
            vc71::mapNodeValueRef(node) = metaFx;

        metaFxParse(metaFx);
    }

    return metaFx;
}

// dword_1023D954 in the shipped image.
void* g_effectManager = 0;

bool createEffectManager(IDirect3DDevice9* device, bool disableShaderCache)
{
    EffectManager* em = new EffectManager(device, disableShaderCache);
    g_effectManager = em;

    ::renddx9abi::logf("EffectManager: built at 0x%08X, %u bytes of shader defines",
                       ::renddx9abi::addr(em), em->shaderDefines().size());
    return true;
}

// the expression initializeD3DSubsystems registers at 0x1000B3DD..0x1000B3FB:
// `dword_1023D954 ? dword_1023D954 + 4 : 0`. The null arm is shipped - it registers a null listener.
void* effectManagerDeviceLostListener()
{
    return g_effectManager ? static_cast<unsigned char*>(g_effectManager) + 4 : 0;
}

} // namespace Ra

void effectManagerSetShowChannel(unsigned int channel)
{
    // PE sub_10034430 dereferences the singleton without a guard.
    static_cast<Ra::EffectManager*>(Ra::g_effectManager)->setShowChannel(channel);
}

// sub_10033240 for VideoOptions' resolution change (sub_1000F8D0, its last call:
// 0x1000FB54 `mov ecx, dword_1023D954` / 0x1000FB5A `call sub_10033240`). Unguarded, as that call
// site is. It runs right after the two probes, so the A2M defines pick up what they just found.
void effectManagerRebuildShaderDefines()
{
    static_cast<Ra::EffectManager*>(Ra::g_effectManager)->rebuildShaderDefines();
}

// The seam StaticMeshStateManager::initTechniques (Code/BF2/Geom/StaticMeshTemplate.cpp, the
// RW-124 frontier) needs. Its declaration belongs beside effectManagerSetShowChannel in
// RendDX9Base.h and is deliberately NOT added there this sitting, because another agent owns that
// header right now; whoever unblocks the Geom side adds the one line.

// unguarded ON purpose. Both shipped call sites - 0x10115D08 and load
// dword_1023D954 straight into ecx and call, with no null test.
Ra::MetaFx* effectManagerGetMetaFx(const vc71::string& name)
{
    return static_cast<Ra::EffectManager*>(Ra::g_effectManager)->getMetaFx(name);
}


// Returns the compiled ID3DXEffect for path, or null. Requires the effect-manager singleton.

void* effectManagerGetEffect(const vc71::string& path)
{
    return static_cast<Ra::EffectManager*>(Ra::g_effectManager)->getEffect(path);
}


// In-memory generated shader files use 72-byte string/string nodes, not cache pointer nodes.
// Names compare case-insensitively and are also lowercased. The compatibility tree keeps
// search order but omits red-black rebalancing.

namespace Ra {
namespace {

inline EffectNode72*  asNode72(unsigned int* p) { return reinterpret_cast<EffectNode72*>(p); }
inline vc71::string&  key72(EffectNode72* n)    { return *reinterpret_cast<vc71::string*>(n->key); }
inline vc71::string&  value72(EffectNode72* n)  { return *reinterpret_cast<vc71::string*>(n->value); }

// sub_10035E40 - lower_bound. Descends remembering the last node that is NOT less than the key.
EffectNode72* lowerBound72(RaList& m, const vc71::string& key)
{
    EffectNode72* const head = asNode72(m.head);
    EffectNode72* node = head->parent;
    EffectNode72* best = head;
    while (!node->isNil)
    {
        if (::_stricmp(key72(node).c_str(), key.c_str()) >= 0) { best = node; node = node->left; }
        else                                                    { node = node->right; }
    }
    return best;
}

// sub_10035FD0 - upper_bound. The same descent with the comparison the other way round.
EffectNode72* upperBound72(RaList& m, const vc71::string& key)
{
    EffectNode72* const head = asNode72(m.head);
    EffectNode72* node = head->parent;
    EffectNode72* best = head;
    while (!node->isNil)
    {
        if (::_stricmp(key.c_str(), key72(node).c_str()) >= 0) { node = node->right; }
        else                                                    { best = node; node = node->left; }
    }
    return best;
}

// In-order successor, the walk sub_10037090 inlines to advance across the erase range.
EffectNode72* next72(EffectNode72* node)
{
    if (node->isNil)
        return node;
    if (!node->right->isNil)
    {
        node = node->right;
        while (!node->left->isNil)
            node = node->left;
        return node;
    }
    EffectNode72* parent = node->parent;
    while (!parent->isNil && node == parent->right)
    {
        node = parent;
        parent = parent->parent;
    }
    return parent;
}

void destroyNode72(EffectNode72* node)
{
    key72(node).~string();
    value72(node).~string();
    ::renddx9abi::engineFree(node);
}

// Unlink one node, then free it. Plain BST removal - see the RW-74 note above.
void erase72(RaList& m, EffectNode72* z)
{
    EffectNode72* const head = asNode72(m.head);
    if (z == head || z->isNil)
        return;

    EffectNode72* splice = z;
    if (!z->left->isNil && !z->right->isNil)
        splice = next72(z);

    EffectNode72* child = splice->left->isNil ? splice->right : splice->left;
    if (!child->isNil)
        child->parent = splice->parent;

    if (splice == head->parent)            head->parent = child;   // the root
    else if (splice == splice->parent->left)  splice->parent->left  = child;
    else                                      splice->parent->right = child;

    if (splice != z)
    {
        key72(z)   = key72(splice);
        value72(z) = value72(splice);
    }

    // The head's leftmost/rightmost cursors, which the shipped clear path also rebuilds.
    EffectNode72* scan = head->parent;
    if (scan->isNil) { head->left = head; head->right = head; }
    else
    {
        while (!scan->left->isNil)  scan = scan->left;
        head->left = scan;
        scan = head->parent;
        while (!scan->right->isNil) scan = scan->right;
        head->right = scan;
    }

    destroyNode72(splice);
    if (m.count)
        --m.count;
}

} // anonymous namespace


// Inserts a lowercase name without replacing duplicates; always returns true.

bool EffectManager::addMemoryFile(const vc71::string& name, const vc71::string& contents)
{
    vc71::string lowered(name);
    pathToLower(lowered);

    EffectNode72* const head  = asNode72(m_list9C.head);
    EffectNode72*       node  = head->parent;
    EffectNode72*       parent = head;
    bool                goLeft = true;

    while (!node->isNil)
    {
        parent = node;
        goLeft = ::_stricmp(lowered.c_str(), key72(node).c_str()) < 0;
        node   = goLeft ? node->left : node->right;
    }

    // std::map::insert semantics: an existing equivalent key wins and nothing is stored.
    EffectNode72* const existing = lowerBound72(m_list9C, lowered);
    if (existing != head && ::_stricmp(key72(existing).c_str(), lowered.c_str()) == 0)
        return true;

    EffectNode72* const fresh =
        static_cast<EffectNode72*>(::renddx9abi::engineAlloc(sizeof(EffectNode72)));
    if (!fresh)
        return true;                                  // shipped: the result is a constant true

    fresh->left   = head;
    fresh->right  = head;
    fresh->parent = parent;
    fresh->color  = 0;
    fresh->isNil  = 0;
    fresh->pad[0] = 0;
    fresh->pad[1] = 0;
    new (fresh->key)   vc71::string(lowered);
    new (fresh->value) vc71::string(contents);

    if (parent == head)      { head->parent = fresh; head->left = fresh; head->right = fresh; }
    else if (goLeft)         { parent->left  = fresh; if (parent == head->left)  head->left  = fresh; }
    else                     { parent->right = fresh; if (parent == head->right) head->right = fresh; }

    ++m_list9C.count;
    return true;
}


// 0x10034460, 79 B, `retn 4`. `erase(lower_bound(k), upper_bound(k))` - and note the shipped order:
// upper_bound is computed first (0x10034481) and lower_bound second (0x1003448B), which is
// equal_range evaluated backwards. Preserved, because both descents read the same tree and the
// order is observable if either ever faulted.

void EffectManager::removeMemoryFile(const vc71::string& name)
{
    vc71::string lowered(name);
    pathToLower(lowered);                          // sub_10074E80's lowering half

    EffectNode72* const last  = upperBound72(m_list9C, lowered);   // sub_10035FD0, first
    EffectNode72*       first = lowerBound72(m_list9C, lowered);   // sub_10035E40, second

    while (first != last && !first->isNil)
    {
        EffectNode72* const doomed = first;
        first = next72(first);
        erase72(m_list9C, doomed);
    }
}


// Ra effects use virtual include files, a per-adapter compiled cache and a shader-model
// search. Their include resolver also maintains the fully expanded source text.


namespace {


// FileManager vslot +0x38 again, but this time the record is read: the size and the last-write
// filetime, which is what the .cfx freshness test compares. The 64-byte PE record is the one the
// block above fileManagerGetFileInfo describes; the native flavour has the typed io::FileInfo, and
// the two are NOT the same layout, which is why this is written once, here, with the split.

struct RaFileInfo
{
    unsigned int size;
    unsigned int writeTimeLow;
    unsigned int writeTimeHigh;
};

bool raGetFileInfo(const vc71::string& path, RaFileInfo& out)
{
    out.size = 0;
    out.writeTimeLow = 0;
    out.writeTimeHigh = 0;

    // [CHOSEN, not recovered] Permit loading before the host publishes FileManager.
    if (!::dice::hfe::fileManager)
        return false;

    const renddx9abi::HostString hostPath = renddx9abi::makeHostString(path);

#if BF2_PLUGIN_ABI_NATIVE
    ::dice::hfe::io::FileInfo info;
    if (!::dice::hfe::fileManager->getFileInfo(hostPath, info))
        return false;
    out.size          = info.size;
    out.writeTimeLow  = info.lastWriteTimeLow;
    out.writeTimeHigh = info.lastWriteTimeHigh;
    return true;
#else
    PeFileInfo info;
    void** const vtbl = *reinterpret_cast<void***>(::dice::hfe::fileManager);
    typedef unsigned char (__thiscall* GetFileInfoFn)(void*, const renddx9abi::HostString&,
                                                      PeFileInfo&);
    if (!reinterpret_cast<GetFileInfoFn>(vtbl[0x38 / 4])(
            ::dice::hfe::fileManager, hostPath, info))
        return false;
    out.size          = info.size;
    out.writeTimeLow  = info.times[4];   // record+0x38, what 0x100351A5 compares
    out.writeTimeHigh = info.times[5];   // record+0x3C
    return true;
#endif
}

// FileManager vslot +0x18. The read pair is (2, 0) and the write pair is (0, 2), both read off
// their call sites: 0x1003528C pushes 0,0,2,path and 0x1003317F pushes 0,2,0,path.
renddx9abi::HostStream* raOpen(const vc71::string& path, int mode, int access)
{
    if (!::dice::hfe::fileManager)
        return 0;
    const renddx9abi::HostString hostPath = renddx9abi::makeHostString(path);
    return ::dice::hfe::fileManager->open(hostPath,
                                          static_cast<renddx9abi::HostFileMode>(mode),
                                          static_cast<renddx9abi::HostFileAccess>(access), 0);
}

// Returns the basename without its final extension, accepting either path separator.
void raCacheStem(vc71::string& out, const vc71::string& in)
{
    std::string t(in.c_str(), in.size());
    const std::string::size_type back = t.rfind('\\');
    const std::string::size_type fwd  = t.rfind('/');
    std::string::size_type cut = back;
    if (fwd != std::string::npos && (cut == std::string::npos || fwd > cut))
        cut = fwd;
    if (cut != std::string::npos)
        t = t.substr(cut + 1);
    const std::string::size_type dot = t.rfind('.');
    if (dot != std::string::npos)
        t = t.substr(0, dot);
    out = t.c_str();
}

// sub_1003C440 - drop every '_' (95) and '|' (124). File-static in the PE, so each caller's
// translation unit carries its own copy; Ra/RaShaderPermutator.cpp has the other one, and this file
// already follows the same precedent for sub_10029D00.
void raCleanPermutationName(vc71::string& out, const vc71::string& in)
{
    std::string t;
    const char* p = in.c_str();
    for (; *p; ++p)
        if (*p != '_' && *p != '|')
            t += *p;
    out = t.c_str();
}

// The 44-byte node the include handler's watch set uses: links, a 28-byte key, _Color at +40 and
// _Isnil at +41 - the shape sub_100362F0 allocates and sub_100379A0 / sub_10037A50 walk. Its
// comparator is the ordinary case-sensitive std::string `<` (std::operator< on char), NOT the
// `_stricmp` the two caches above use.
struct EffectNode44
{
    EffectNode44* left;
    EffectNode44* parent;
    EffectNode44* right;
    unsigned char key[28];
    unsigned char color;
    unsigned char isNil;
    unsigned char pad[2];
};

static_assert(sizeof(EffectNode44) == 44, "sub_100362F0 allocates 44");

inline vc71::string& key44(EffectNode44* n) { return *reinterpret_cast<vc71::string*>(n->key); }

// sub_100379A0 - find. lower_bound, then "is the probe less than what we landed on".
bool raWatchSetContains(RaList& m, const vc71::string& key)
{
    EffectNode44* const head = reinterpret_cast<EffectNode44*>(m.head);
    EffectNode44* node = head->parent;
    while (node && !node->isNil)
    {
        const int cmp = ::strcmp(key.c_str(), key44(node).c_str());
        if (cmp == 0)
            return true;
        node = cmp < 0 ? node->left : node->right;
    }
    return false;
}

// sub_10037A50 - insert. Same RW-74 divergence as every other insert in this tree: a correct BST
// with the shipped ordering, without sub_10037B10's rebalance.
void raWatchSetInsert(RaList& m, const vc71::string& key)
{
    EffectNode44* const head = reinterpret_cast<EffectNode44*>(m.head);
    EffectNode44* parent = head;
    EffectNode44* node   = head->parent;
    bool goLeft = true;

    while (node && !node->isNil)
    {
        parent = node;
        goLeft = ::strcmp(key.c_str(), key44(node).c_str()) < 0;
        node   = goLeft ? node->left : node->right;
    }

    EffectNode44* const fresh =
        static_cast<EffectNode44*>(::renddx9abi::engineAlloc(sizeof(EffectNode44)));
    if (!fresh)
        return;

    fresh->left   = head;
    fresh->right  = head;
    fresh->parent = parent;
    fresh->color  = 0;
    fresh->isNil  = 0;
    fresh->pad[0] = 0;
    fresh->pad[1] = 0;
    new (fresh->key) vc71::string(key);

    if (parent == head)  { head->parent = fresh; head->left = fresh; head->right = fresh; }
    else if (goLeft)     { parent->left  = fresh; if (parent == head->left)  head->left  = fresh; }
    else                 { parent->right = fresh; if (parent == head->right) head->right = fresh; }

    ++m.count;
}

// sub_10036470 - the 72-byte map's find. The same `_stricmp` lower_bound shape as sub_10036390,
// on the node geometry this file already models.
EffectNode72* find72(RaList& m, const vc71::string& key)
{
    EffectNode72* const head = asNode72(m.head);
    EffectNode72* node = head->parent;
    while (node && !node->isNil)
    {
        const int cmp = ::_stricmp(key.c_str(), key72(node).c_str());
        if (cmp == 0)
            return node;
        node = cmp < 0 ? node->left : node->right;
    }
    return 0;
}

// Generates the first permutation whose cleaned name matches wanted; returns false on a miss.
bool raGenerateShaderByCleanName(MetaFx* fx, const vc71::string& wanted)
{
    const MetaFxStringArray& names = fx->getGeneratedNames();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        vc71::string clean;
        raCleanPermutationName(clean, names[i]);
        if (::strcmp(clean.c_str(), wanted.c_str()) == 0)
        {
            fx->generateShader(names[i]);
            return true;
        }
    }
    return false;
}

} // anonymous namespace


// Regenerates cached in-memory shaders only when the memory store is empty.
// The mode test assumes every MetaFx was constructed by loadMetaFx with the queue flag;
// a second producer requires reading each object's own mode instead.

void EffectManager::refillMemoryFiles()
{
    if (m_list9C.count != 0)
        return;

    const bool writeGeneratedFiles =
        (*(static_cast<const unsigned char*>(g_renderQueues) + 0xB4)) == 0;

    EffectNode48* const head = reinterpret_cast<EffectNode48*>(m_list54.head);
    for (EffectNode48* node = head->left; node != head; node = nextEffectNode(node))
    {
        MetaFx* const fx = static_cast<MetaFx*>(static_cast<void*>(node->effect));
        if (!fx)
            continue;
        if (!writeGeneratedFiles)
            metaFxParse(fx);
    }
}


// sub_10033AA0, 0x10033AA0, 101 B, `retn 4`. The in-memory shader-file lookup.

//     refillMemoryFiles();
//     it = m_memoryFiles.find(toLower(name));
//     return it == end() ? 0 : &it->second;

// Refreshes memory files before looking up the lowercase name.

const vc71::string* EffectManager::findMemoryFile(const vc71::string& name)
{
    refillMemoryFiles();

    vc71::string key(name);
    pathToLower(key);

    EffectNode72* const node = find72(m_list9C, key);
    if (!node)
        return 0;
    return &value72(node);                                    // node + 40
}


// Generates a missing shader from the first matching MetaFx name.
// The suffix starts at prefix length even when the substring match was not at offset zero.

bool EffectManager::generateMissingShader(const vc71::string& name)
{
    vc71::string stripped;
    pathStripExtension(stripped, name);
    vc71::string wanted(stripped);
    pathToLower(wanted);                                      // the sub_10074E80 half

    EffectNode48* const head = reinterpret_cast<EffectNode48*>(m_list54.head);
    for (EffectNode48* node = head->left; node != head; node = nextEffectNode(node))
    {
        vc71::string prefix;
        pathStripExtension(prefix, *reinterpret_cast<const vc71::string*>(node->key));

        const std::string haystack(wanted.c_str(), wanted.size());
        if (haystack.find(prefix.c_str(), 0, prefix.size()) == std::string::npos)
            continue;

        const vc71::string tail(wanted.c_str() + prefix.size());
        MetaFx* const fx = static_cast<MetaFx*>(static_cast<void*>(node->effect));
        raGenerateShaderByCleanName(fx, tail);
        return true;
    }
    return false;
}


// Reads a NUL-terminated effect source into engine-allocated bufferOut and returns its size.
// Tries disk, generated memory files, then on-demand generation. Returns zero on failure;
// buffers allocated before a read/open failure are retained.

unsigned int EffectManager::readEffectFile(const vc71::string& name, unsigned char** bufferOut)
{
    vc71::string path(name);
    if (g_Renderer)
        applyModPrefix(path);

    RaFileInfo info;
    if (raGetFileInfo(path, info))                            // FileManager +0x38
    {
        *bufferOut = static_cast<unsigned char*>(::renddx9abi::engineAlloc(info.size + 1));

        renddx9abi::HostStream* const file = raOpen(path, 2, 0);   // FileManager +0x18
        if (!file)
        {
            ::renddx9abi::logf("EffectManager.cpp:884: Can't open effect file: %s", path.c_str());
            return 0;
        }

        // THE `*bufferOut ?` test IS ours. sub_1005F340's answer is used unchecked at
        // a failed allocation there hands the host FileManager a null destination.
        const unsigned int read = *bufferOut ? file->read(*bufferOut, info.size) : 0;
        file->release();

        if (read != info.size)
        {
            ::renddx9abi::logf("EffectManager.cpp:892: Can't read whole file: %s", path.c_str());
            return 0;
        }

        (*bufferOut)[read] = 0;
        return read;
    }

    const vc71::string* const stored = findMemoryFile(path);
    if (stored)
    {
        *bufferOut = static_cast<unsigned char*>(
            ::renddx9abi::engineAlloc(stored->size() + 1));
        if (*bufferOut)
            ::strcpy(reinterpret_cast<char*>(*bufferOut), stored->c_str());
        return stored->size();
    }

    if (generateMissingShader(name))
        return readEffectFile(name, bufferOut);               // the shipped recursion

    ::renddx9abi::logf("EffectManager.cpp:871: Can't find effect file: %s Loading Filename:%s",
                       name.c_str(), path.c_str());
    return 0;
}


// Writes a binary compiled-effect cache with each byte XORed by its index.
// Returns false if the output cannot be opened; individual writes are unchecked.

bool EffectManager::writeEffectCache(ID3DXBuffer* blob, const vc71::string& path)
{
    renddx9abi::HostStream* const file = raOpen(path, 0, 2);  // FileManager +0x18, the write pair
    if (!file)
        return false;

    const unsigned int size = blob->GetBufferSize();

    char* const copy = static_cast<char*>(::renddx9abi::engineAlloc(size));
    if (!copy)
    {
        // NOT shipped. sub_1005F340's answer is used unchecked at 0x100331AC; this build refuses
        // rather than memcpy through a null, and says so, because a failed cache write costs one
        // recompile next run and nothing else.
        ::renddx9abi::logf("EffectManager::writeEffectCache: %u-byte engine allocation failed for "
                           "\"%s\"; the cache file is not written", size, path.c_str());
        file->release();
        return false;
    }

    const char* const src = static_cast<const char*>(blob->GetBufferPointer());
    ::memcpy(copy, src, size);

    for (unsigned int i = 0; i < size; ++i)
        copy[i] = static_cast<char>(copy[i] ^ static_cast<char>(i));

    file->write(copy, size);                                  // stream +0x14
    file->release();                                          // stream +0x08
    ::renddx9abi::engineFree(copy);
    return true;
}

// The two seams Ra/RaShaderPermutator.cpp calls. The shipped bodies are methods on the
// EffectManager; the permutator reaches them through the singleton, exactly as its own call sites
// do (`mov ecx, dword_1023D954`). Unguarded, as those call sites are.
bool effectManagerAddMemoryFile(const vc71::string& name, const vc71::string& contents)
{
    return static_cast<EffectManager*>(g_effectManager)->addMemoryFile(name, contents);
}

// sub_10034210 - published for Renderer::handleLevelPreLoad, which calls it directly one
// statement before the load pass (which calls it again; both are shipped).
void effectManagerResetEffects()
{
    static_cast<EffectManager*>(g_effectManager)->resetEffects();
}

void effectManagerRemoveMemoryFile(const vc71::string& name)
{
    static_cast<EffectManager*>(g_effectManager)->removeMemoryFile(name);
}

namespace {


// Default compile flags disable preshaders; debug, skipped optimization and partial precision
// remain off unless their switches change.

unsigned char g_noPreshader      = 1;   // byte_102382A0 - ONE, not zero
unsigned char g_shaderDebug      = 0;   // byte_1023DD9B
unsigned char g_skipOptimization = 0;   // byte_1023DD9A
unsigned char g_partialPrecision = 0;   // byte_1023DD99
unsigned char g_forceOneFour     = 0;   // byte_1023DD98

// byte_1023D950 - set to 1 the first time a compile produces a usable technique. Read back by
// sub_10153490/sub_101534A0, which are outside this subsystem.
unsigned char g_anyEffectCompiled = 0;

// The macro tables, transcribed from 0x101E1268..0x101E1308. Each row is {name, value}.
const char* const kPsVersion[5][2] =
{
    { "PSVERSION", "13" }, { "PSVERSION", "14" }, { "PSVERSION", "21" },
    { "PSVERSION", "22" }, { "PSVERSION", "20" }
};
const char* const kPsModel[5][2] =
{
    { "PSMODEL", "ps_1_3" }, { "PSMODEL", "ps_1_4" }, { "PSMODEL", "ps_2_a" },
    { "PSMODEL", "ps_2_b" }, { "PSMODEL", "ps_2_0" }
};
const char* const kVsModel[2][2] =
{
    { "VSMODEL", "vs_1_1" }, { "VSMODEL", "vs_2_0" }
};
const char* const kForce14[2][2] =
{
    { "_FORCE_1_4_SHADERS_", "0" }, { "_FORCE_1_4_SHADERS_", "1" }
};
// dword_101E12F8 - the minimum (major<<8 | minor) pixel-shader version each psmodel row needs.
const unsigned int kMinPsVersion[5] = { 0x103u, 0x104u, 0x201u, 0x202u, 0x200u };

template <typename Interface>
inline void raRelease(Interface*& p)
{
    // sub_10035DC0 - `if (p) { p->Release(); p = 0; }`, the 23-byte helper every arm here uses.
    if (p)
    {
        p->Release();
        p = 0;
    }
}

// Pumps the renderer's inactive/device-lost wait and reports whether it reset the device.
// g_Renderer must be non-null. A true result invalidates shader compilation state.
bool rendererDeviceWasReset()
{
    // `mov ecx, dword_1023D098; mov eax, [ecx]; call [eax+0BCh]; test al, al; jne skip` - the
    // caller already tested g_Renderer; the dispatch is typed through the shared interface, whose
    // +0xBC is Renderer::waitWhileAppInactive (0x100055B0). true means "no wait, no reset".
    return !static_cast<IRenderer*>(g_Renderer)->waitWhileAppInactive();
}


// Lazily loads D3DXCreateEffectCompiler from d3dx9_24.dll.
// This local copy retains its module reference. Consolidate it with the permutator's
// helper when that helper has a shared declaration and external linkage.

typedef HRESULT (WINAPI* RaD3dxCreateEffectCompilerFn)(LPCSTR, UINT, CONST D3DXMACRO*,
                                                       LPD3DXINCLUDE, DWORD,
                                                       LPD3DXEFFECTCOMPILER*, LPD3DXBUFFER*);

HMODULE                      s_raD3dxModule = 0;              // dword_1023E108
RaD3dxCreateEffectCompilerFn s_raCreateEffectCompiler = 0;    // dword_1023E10C

long raCreateEffectCompiler(const char* srcData, unsigned int srcDataLen,
                            const D3DXMACRO* defines, LPD3DXINCLUDE include, unsigned long flags,
                            ID3DXEffectCompiler** compilerOut, ID3DXBuffer** parseErrorsOut)
{
    if (!s_raD3dxModule)
    {
        s_raD3dxModule = ::LoadLibraryA("d3dx9_24.dll");
        if (s_raD3dxModule)
        {
            s_raCreateEffectCompiler = reinterpret_cast<RaD3dxCreateEffectCompilerFn>(
                ::GetProcAddress(s_raD3dxModule, "D3DXCreateEffectCompiler"));
        }
        if (!s_raD3dxModule || !s_raCreateEffectCompiler)
        {
            // The diagnostic retains its original DLL-name typo.
            ::renddx9abi::logf("RaShaderPermutator.cpp:574: Failed to load D3DX9d_24.dll, we need "
                               "this to avoid the April 2005 DxSDK bug(recursive call on parsing "
                               "some .fx files (skinnedmesh.fx)");
            return static_cast<long>(0x80004005);             // E_FAIL, as the shipped body returns
        }
    }

    return s_raCreateEffectCompiler(srcData, srcDataLen, defines, include, flags,
                                    compilerOut, parseErrorsOut);
}


// StringFromGUID2's output, spelled out. The shipped body calls ole32's StringFromGUID2 into 39
// wide characters and then narrows them a byte at a time. This implementation formats the same
// canonical braced upper-case form directly: 38 characters and a NUL, without an ole32 call.
// Byte-for-byte identical output; [CHOSEN, not recovered] as a mechanism.

void raFormatGuid(char out[40], const GUID& guid)
{
    ::sprintf_s(out, 40, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                static_cast<unsigned long>(guid.Data1),
                static_cast<unsigned int>(guid.Data2), static_cast<unsigned int>(guid.Data3),
                guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

} // anonymous namespace


// Compiles source with this manager's include resolver and supplied macro definitions.
// Returns a compiled-effect buffer, or null with compiler diagnostics in errors.
// The optional second pass fixes projected-texture shaders for pixel shader 1.4.

ID3DXBuffer* EffectManager::compileEffectSource(const char* source, unsigned int sourceLen,
                                                const D3DXMACRO* defines, unsigned long* flags,
                                                ID3DXBuffer** errors)
{
    {
        // The shipped expression, with the four copy-constructed temporaries MSVC emitted for it
        // collapsed: `text = std::string("#line 2 \"Generated.fx\"\r\n") + source`.
        vc71::string generated("#line 2 \"Generated.fx\"\r\n");
        generated += source;
        m_sub->text = generated;
    }

    ID3DXBuffer* blob = 0;

    for (int pass = 1; ; ++pass)
    {
        ID3DXEffectCompiler* compiler = 0;

        // sub_1003C610 - the lazily-bound D3DXCreateEffectCompiler in d3dx9_24.dll, with `pInclude`
        // set to this object's +0x44 sub-object, whose +0x00 is the off_101E139C include table.
        const long created = raCreateEffectCompiler(
            source, sourceLen, defines, reinterpret_cast<LPD3DXINCLUDE>(m_sub), *flags, &compiler,
            errors);
        if (created != 0)
        {
            raRelease(compiler);
            return 0;
        }

        raRelease(*errors);

        blob = 0;
        const long hr = compiler->CompileEffect(*flags, &blob, errors);
        raRelease(compiler);

        if (hr != 0)
        {
            if (blob)
                blob->Release();
            return 0;
        }

        if (pass > 1)
            break;
        if (!blob)
            return 0;


        // [FRONTIER] Projected-texture rewriting for ps_1_4 remains unavailable.
        // The flag is never enabled, and the compatibility test reads macro entry 3
        // rather than psversion at entry 5. Preserve both conditions.

        if (!m_byte61
            || ::strcmp(defines[3].Name, "PSVERSION") != 0
            || ::strcmp(defines[3].Definition, "14") != 0)
        {
            break;
        }

        RD9_FRONTIER("dice::hfe::dx9rend::Ra::EffectManager::compileEffectSource "
                     "(the ps_1_4 projected-texture fixup, 0x100345FF; PME/D3DXDisassembleEffect. "
                     "This is unreachable in the shipped image - +0x61 is never set and the macro "
                     "index the two strcmps use is entry 3, not 5)", 0x61);
    }

    // LABEL_70 - clear the expanded copy and hand the blob back.
    m_sub->text = "";
    return blob;
}


// Loads or compiles an effect and replaces the reference in effectOut.
// Cache identity includes the adapter, build number and filter quality; stale caches
// are recompiled across supported shader models. unused is retained for ABI compatibility.

void* EffectManager::compileEffect(const vc71::string& name, int unused, void** effectOut)
{
    (void)unused;   // arg_4: passed by getEffect as 0, never read by the shipped body

    // [PLATFORM] Compatibility builds publish LoadStat progress during shader compilation.
    // [FRONTIER] Native builds omit this loading-bar update until LoadStat exposes typed
    // accessors; its layout differs and must not be read using PE offsets.
#if !BF2_PLUGIN_ABI_NATIVE
    // [CHOSEN, not recovered] Allow LoadStat to be absent during early DLL initialization.
    unsigned char* const rawLoadStat = static_cast<unsigned char*>(
        static_cast<void*>(::dice::hfe::loadStat));
    if (rawLoadStat)
        rawLoadStat[8] = 1;
#endif

    unsigned long flags = 0;
    if (g_noPreshader)      flags  = 0x100u;
    if (g_shaderDebug)      flags |= 0x001u;
    if (g_skipOptimization) flags |= 0x004u;
    if (g_partialPrecision) flags |= 0x020u;

    // sub_10001260 on dword_1023B980 - sixteen bytes at DeviceCaps + 1384, which is
    // D3DADAPTER_IDENTIFIER9::DeviceIdentifier (the adapter block starts at +304 and the GUID is
    // 1080 bytes into it; the same arithmetic puts VendorId at +1368, which is the dword the
    // isNvidia/isAti pair above already reads). The shipped body then StringFromGUID2s it into 39
    // wide characters and copies them one byte at a time - a deliberate narrowing, not a
    // conversion. See raFormatGuid for why the OLE call itself is not made here.
    char guidText[40];
    guidText[0] = '\0';
    {
        const CapsView* const caps = capsOf();
        if (caps)
            raFormatGuid(guidText, caps->adapter.DeviceIdentifier);
    }

    // g_rendererBase->vslot+0x74 -> VideoOptions; VideoOptions->vslot+0x04(7) -> filter quality.
    unsigned int filterQuality = 0;
    if (g_rendererBase)
    {
        IVideoOptions* const videoOptions =
            static_cast<IRenderer*>(g_rendererBase)->getVideoOptions();
        if (videoOptions)
        {
            filterQuality = static_cast<unsigned int>(videoOptions->getQuality(7));
        }
    }

    vc71::string modPath("mods/bf2");
    if (g_Renderer && static_cast<IRenderer*>(g_Renderer)->isExpansionPack())
        modPath = "mods/xpack";

    vc71::string cachePath("@HOME@/");
    cachePath += modPath;
    cachePath += "/cache/";
    {
        char suffix[64];
        ::sprintf_s(suffix, "_%d_%d", 0xC51, filterQuality);   // sub_10074AB0, "_%d_%d", 3153, q
        cachePath += guidText;
        cachePath += suffix;
    }
    cachePath += "/";
    {
        vc71::string stem;
        raCacheStem(stem, name);
        cachePath += stem;
    }
    cachePath += ".cfx";

    // The freshness test. this+0x78 is the _Mysize of the string at +0x64 - m_debugDefines - so
    // the cache is used only when NO debug define is active, which is what makes the cached blob
    // safe to reuse at all.
    bool useCache = false;
    RaFileInfo cacheInfo;
    cacheInfo.size = 0;
    cacheInfo.writeTimeLow = 0;
    cacheInfo.writeTimeHigh = 0;

    if (!m_byte60 && m_debugDefines.size() == 0)
    {
        if (raGetFileInfo(cachePath, cacheInfo))
        {
            useCache = true;
            RaFileInfo sourceInfo;
            if (raGetFileInfo(name, sourceInfo))
            {
                // Equal timestamps require recompilation.
                const bool newer =
                    (cacheInfo.writeTimeHigh > sourceInfo.writeTimeHigh)
                    || (cacheInfo.writeTimeHigh == sourceInfo.writeTimeHigh
                        && cacheInfo.writeTimeLow > sourceInfo.writeTimeLow);
                useCache = newer;
            }
            // Keep using the cache if the source file is absent.
        }
    }

    unsigned char* source = 0;
    const unsigned int sourceLen =
        readEffectFile(useCache ? cachePath : name, &source);
    if (sourceLen == 0)
    {
        // This early return leaves LoadStat's busy flag set and skips the progress event.
        *effectOut = 0;
        ::renddx9abi::engineFree(source);
        return 0;
    }

    ID3DXEffect* effect    = 0;
    ID3DXBuffer* errors    = 0;
    D3DXHANDLE   technique = 0;

    if (useCache)
    {
        // Read the cached effect again into D3DX-owned storage.
        // Local failure guards report a bad cache and leave a null effect for build failure.
        ID3DXBuffer* buffer = 0;
        if (D3DXCreateBuffer(cacheInfo.size, &buffer) != 0)
            ::renddx9abi::logf("EffectManager.cpp:1000 (D3DXCreateBuffer(%u) failed for \"%s\")",
                               cacheInfo.size, cachePath.c_str());

        renddx9abi::HostStream* const file = raOpen(cachePath, 2, 0);
        if (file && buffer)
        {
            void* const dst = buffer->GetBufferPointer();
            const unsigned int read = file->read(dst, cacheInfo.size);
            if (read != cacheInfo.size)
                ::renddx9abi::logf("EffectManager.cpp:1004 (read %u of %u from \"%s\")",
                                   read, cacheInfo.size, cachePath.c_str());
        }
        if (file)
            file->release();

        if (buffer)
        {
            const unsigned int size = buffer->GetBufferSize();
            char* const bytes = static_cast<char*>(buffer->GetBufferPointer());
            for (unsigned int i = 0; i < size; ++i)
                bytes[i] = static_cast<char>(bytes[i] ^ static_cast<char>(i));

            const unsigned long createFlags = flags | (m_byte62 ? 0x800u : 0u);
            const long hr = D3DXCreateEffect(
                m_device,
                buffer->GetBufferPointer(),
                buffer->GetBufferSize(),
                0, 0, createFlags, static_cast<LPD3DXEFFECTPOOL>(m_effectPool), &effect, &errors);
            if (hr != 0)
                ::renddx9abi::logf("EffectManager.cpp:1016 Failed to create effect: hr=0x%08X (%s)",
                                   static_cast<unsigned>(hr), cachePath.c_str());
        }

        if (effect)
        {
            technique = effect->GetTechniqueByName("defaultTechnique");
            if (!technique)
                effect->FindNextValidTechnique(0, &technique);
            effect->OnLostDevice();

            raRelease(buffer);

            // AND then IT does IT again. When the +0xBC query says a device reset happened the
            // body runs OnResetDevice, re-finds the technique and calls OnLostDevice a second time.
            // Both OnLostDevice calls are shipped and both are kept.
            if (g_Renderer && rendererDeviceWasReset())
            {
                effect->OnResetDevice();
                technique = effect->GetTechniqueByName("defaultTechnique");
                if (!technique)
                    effect->FindNextValidTechnique(0, &technique);
                effect->OnLostDevice();
            }
        }
        else
        {
            raRelease(buffer);
        }
    }
    else
    {
        // GSLoadObfuscated, read per call in the shipped body with its default byte seeded to 1.
        if (readBoolSetting("GSLoadObfuscated", true))
            deobfuscateShader(source, sourceLen);

        const unsigned int psVersion = (psMajor() << 8) | psMinor();

        // GSQuickShaderPermutation starts the pixel-model search at ps_2_a instead of ps_1_3.
        const bool quick = readBoolSetting("GSQuickShaderPermutation", false);

        // THE TWO skips ARE PER-vendor AND they ARE NOT symmetric.
        //   ps_2_a (index 2) is skipped on an ATI part that lacks PS20Caps.Caps bit 0
        //                    (D3DPS20CAPS_ARBITRARYSWIZZLE, sub_10001450 = caps[66] & 1)
        //   ps_2_b (index 3) is skipped on any nvidia part
        const bool skipPs2a = isAti() && !ps20ArbitrarySwizzle();
        const bool skipPs2b = isNvidia();

        // _FORCE_1_4_SHADERS_. It is NOT the same expression the permutator's own macro builder
        // uses (sub_1003CA30 omits the debug byte); both are shipped, in different bodies.
        const unsigned int forceIndex =
            (psVersion < 0x200u || g_forceOneFour || m_flag40) ? 1u : 0u;

        const int firstPs = quick ? 3 : 0;

        for (int vsIndex = 0; vsIndex < 2; )
        {
            int psIndex = firstPs;
            for (; psIndex < 5; ++psIndex)
            {
                if (skipPs2a && psIndex == 2) continue;
                if (skipPs2b && psIndex == 3) continue;
                if (psVersion < kMinPsVersion[psIndex]) continue;

                // A device reset restarts both profile indices and immediately compiles index zero.
                if (g_Renderer && rendererDeviceWasReset())
                {
                    vsIndex = 0;
                    psIndex = 0;
                }

                D3DXMACRO defines[7];
                defines[0].Name = kVsModel[vsIndex][0];  defines[0].Definition = kVsModel[vsIndex][1];
                defines[1].Name = kPsModel[psIndex][0];  defines[1].Definition = kPsModel[psIndex][1];
                defines[2].Name = kForce14[forceIndex][0];
                defines[2].Definition = kForce14[forceIndex][1];
                defines[3].Name = "_FORCE_1_3_SHADERS_"; defines[3].Definition = "0";
                defines[4].Name = "LOWPSMODEL";          defines[4].Definition = "ps_1_4";
                defines[5].Name = kPsVersion[psIndex][0];
                defines[5].Definition = kPsVersion[psIndex][1];
                defines[6].Name = 0;                     defines[6].Definition = 0;

                raRelease(errors);

                unsigned long passFlags = flags;      // the shipped body passes a copy by pointer
                ID3DXBuffer* const blob = compileEffectSource(
                    reinterpret_cast<const char*>(source), sourceLen, defines, &passFlags, &errors);
                if (!blob)
                    continue;

                const unsigned long createFlags = flags | (m_byte62 ? 0x800u : 0u);
                raRelease(errors);

                const long hr = D3DXCreateEffect(
                    m_device,
                    blob->GetBufferPointer(),
                    blob->GetBufferSize(),
                    0, 0, createFlags, static_cast<LPD3DXEFFECTPOOL>(m_effectPool), &effect,
                    &errors);

                // NOT reproduced, AND nothing IS lost. 0x1003573D and 0x10035743 call
                // DXGetErrorStringA (sub_1005F380, 48,672 B) and DXGetErrorDescription9A
                // (9,617 B) on `hr` and discard both results - the DXTRACE_ERR macro with its
                // output compiled out. Both are DXErr9 library code, not dice's.
                (void)hr;

                if (!effect)
                    continue;

                effect->FindNextValidTechnique(0, &technique);
                effect->OnLostDevice();

                if (technique)
                {
                    g_anyEffectCompiled = 1;                  // byte_1023D950
                    writeEffectCache(blob, cachePath);
                    blob->Release();
                    goto compiled;
                }

                blob->Release();
                raRelease(effect);
            }
            ++vsIndex;
        }
    }

compiled:
    if (effect)
    {
        // Release the caller's previous effect before replacing it.
        if (*effectOut)
            static_cast<ID3DXEffect*>(*effectOut)->Release();
        *effectOut = effect;


        // File-change registration stays disabled until the callback argument types are known.
        // Disk edits therefore do not reload effects during a running session.
        // [RETIRE-WHEN: RW-78]

    }

    raRelease(errors);

#if !BF2_PLUGIN_ABI_NATIVE
    if (rawLoadStat)
    {
        rawLoadStat[8] = 0;
        if (*reinterpret_cast<const unsigned int*>(rawLoadStat) != 0)
        {
            float percent;
            if (rawLoadStat[0x0B])
            {
                percent = 100.0f;
            }
            else
            {
                unsigned int* const current = reinterpret_cast<unsigned int*>(rawLoadStat + 0x50);
                unsigned int* const total   = reinterpret_cast<unsigned int*>(rawLoadStat + 0x54);
                const float* const  lo      = reinterpret_cast<const float*>(rawLoadStat + 0x68);
                const float* const  hi      = reinterpret_cast<const float*>(rawLoadStat + 0x6C);
                if (*total)
                {
                    if (*current >= *total)
                        *current = *total;
                    percent = (static_cast<float>(*current) / static_cast<float>(*total))
                              * (*hi - *lo) + *lo;
                }
                else
                {
                    percent = *lo;
                }
                percent *= 100.0f;
            }

            if (::dice::hfe::eventManager)
            {
                // Only percent is supplied by the original callback contract.
                // [CHOSEN, not recovered] Zero the remaining fields rather than expose stack data.
                struct { float percent; float pad1; const char* pad2; } payload = { percent, 0.0f, 0 };

                // EventManager vslot +0x10. ON THE PE that IS THE four-argument
                // postEvent(EventCategory, unsigned id, void* payload, float); on the ELF the same
                // overload sits at +0x14 and +0x10 is postEvent(cat, EventNode*, float).
                // Code/BF2/Core/EventManager.h records the swap, and TextureManager.cpp's twin
                // carries the same warning. This branch is PE-only by construction, so the raw slot
                // is the correct dispatch and it avoids pulling the engine EventManager header into
                // a retail-ABI translation unit.
                void** const emVtbl =
                    *reinterpret_cast<void***>(static_cast<void*>(::dice::hfe::eventManager));
                typedef unsigned int (__thiscall* RaPostEventFn)(void*, int, unsigned int,
                                                                 void*, float);
                reinterpret_cast<RaPostEventFn>(emVtbl[0x10 / 4])(
                    static_cast<void*>(::dice::hfe::eventManager), 10, 4u, &payload, 0.0f);
            }
        }
    }
#endif

    ::renddx9abi::engineFree(source);                          // sub_1005F360 on Str1
    return effect;
}


// Returns the cached effect or compiles it using the lower-case name.
// Updates the last-name/effect cache even when compilation returns null.

void* EffectManager::getEffect(const vc71::string& name)
{
    vc71::string key(name);
    pathToLower(key);

    void* effect = 0;

    // sub_10036390 - std::map::find with the `_stricmp` comparator. Its caller compares the
    // returned iterator against *(this + 0x30), the head, so a miss is end(); mapFindInsensitive
    // answers the same question with a null.
    vc71::map_node* node = vc71::mapFindInsensitive(effectCache(), key);
    if (node)
    {
        effect = vc71::mapNodeValue(node);                     // node + 40
    }
    else
    {
        compileEffect(key, 0, &effect);
        if (effect)
        {
            // sub_100372D0 - operator[], which inserts on a miss and returns &node->second.
            node = vc71::mapInsertInsensitive(effectCache(), 0x30, key);
            if (node)
                vc71::mapNodeValueRef(node) = effect;

            // Requires the published StateManager.
            static_cast<ID3DXEffect*>(effect)->SetStateManager(
                static_cast<LPD3DXEFFECTSTATEMANAGER>(g_stateManager));
        }
    }

    m_lastEffect = effect;                                     // +0x28
    m_name       = key;                                        // +0x0C
    return effect;
}


// THE Ra ID3DXInclude handler - off_101E139C, the sub-object's primary table.


namespace {


// PME("^#include\s+\"<name>\".*", "mg") driven one line at a time. Ra/RaShaderPermutator.cpp
// records why PME itself is not here: it is a third-party pcre wrapper (?match@PME@@, ??apme@@,
// ?getMarkers@PME@@) that this tree does not carry, so the pattern is transcribed verbatim and the
// cursor is written out. `matchEnd` is where PME::getMarkers(0) would put the match's end - the
// byte before the newline, because `.` does not match '\n' but does match the '\r' of a CRLF.

bool raFindIncludeLine(const std::string& text, const std::string& pattern,
                       std::string::size_type& matchBegin, std::string::size_type& matchEnd)
{
    std::regex re;
    try
    {
        re.assign(pattern);
    }
    catch (const std::regex_error&)
    {
        // The shipped pattern embeds the requested file name unescaped, so a name carrying regex
        // punctuation is a pattern PME would accept and std::regex may not. Refusing here leaves
        // the expanded copy un-spliced, which only the (unreachable) ps_1_4 fixup would have read.
        ::renddx9abi::logf("Ra include: the transcribed #include pattern did not compile for "
                           "\"%s\"; the expanded copy is left alone", pattern.c_str());
        return false;
    }

    std::string::size_type from = 0;
    for (;;)
    {
        std::string::size_type rawEnd = text.find('\n', from);
        if (rawEnd == std::string::npos)
            rawEnd = text.size();
        std::string::size_type lineEnd = rawEnd;
        while (lineEnd > from && text[lineEnd - 1] == '\r')
            --lineEnd;

        if (std::regex_match(text.begin() + static_cast<std::ptrdiff_t>(from),
                             text.begin() + static_cast<std::ptrdiff_t>(lineEnd), re))
        {
            matchBegin = from;
            matchEnd   = rawEnd;
            return true;
        }

        if (rawEnd >= text.size())
            return false;
        from = rawEnd + 1;
    }
}


// Supplies virtual RaDefines.fx or reads and watches an include, then splices its text
// into the expanded source. Always returns S_OK, including file failures; on open failure
// outBytes remains untouched and the allocated buffer is not released.

long __stdcall raIncludeOpen(void* self, int /*includeType*/, const char* fileName,
                             const void* /*parentData*/, const void** outData,
                             unsigned int* outBytes)
{
    EffectSubObject* const sub = static_cast<EffectSubObject*>(self);
    EffectManager* const  owner = static_cast<EffectManager*>(g_effectManager);

    // [CHOSEN, not recovered] Reject missing callback arguments or an unpublished owner.
    if (!fileName || !outData || !outBytes || !sub || !owner)
    {
        if (outData)  *outData  = 0;
        if (outBytes) *outBytes = 0;
        return 0;                                              // the shipped constant
    }

    if (::_stricmp(fileName, "shaders/RaDefines.fx") == 0
        || ::_stricmp(fileName, "mods/xpack/shaders/RaDefines.fx") == 0)
    {
        vc71::string defines(owner->shaderDefines());
        defines += owner->debugDefines();

        *outBytes = defines.size();
        char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(defines.size() + 1));
        *outData = buffer;
        if (buffer)
            ::strcpy(buffer, defines.c_str());
    }
    else
    {
        vc71::string path(fileName);
        if (g_Renderer)
            applyModPrefix(path);

        // Read the setting on every call before opening the file.
        const bool obfuscated = readBoolSetting("GSLoadObfuscated", true);

        RaFileInfo info;
        if (!raGetFileInfo(path, info))
        {
            // AND IT carries ON. 0x1003751D logs EffectManager.cpp:107 and then allocates
            // `info.size + 1` out of a record the probe never filled.
            ::renddx9abi::logf("EffectManager.cpp:107 Can't load effect file: %s", path.c_str());
        }

        char* const buffer = static_cast<char*>(::renddx9abi::engineAlloc(info.size + 1));
        *outData = buffer;

        renddx9abi::HostStream* const file = raOpen(path, 2, 0);
        if (!file)
        {
            ::renddx9abi::logf("EffectManager.cpp:114 Can't load effect file: %s", path.c_str());
            return 0;
        }

        *outBytes = buffer ? file->read(buffer, info.size) : 0;
        file->release();

        if (*outBytes != info.size)
        {
            ::renddx9abi::logf("EffectManager.cpp:122 Can't load whole file: %s", path.c_str());
            return 0;
        }

        buffer[info.size] = 0;
        if (obfuscated)
            deobfuscateShader(reinterpret_cast<unsigned char*>(buffer), info.size);

        // The watch set. The SET maintenance is shipped and is kept; the registration behind it is
        // withheld under RW-78, exactly as compileEffect's is - see the block there.
        if (!raWatchSetContains(sub->list, path))
            raWatchSetInsert(sub->list, path);
    }

    // The splice. `"^#include\\s+\"" + fileName + "\".*"`, PME flags "mg", applied to the expanded
    // copy until no match remains.
    {
        std::string pattern("^#include\\s+\"");
        pattern += fileName;
        pattern += "\".*";

        for (;;)
        {
            std::string text(sub->text.c_str(), sub->text.size());
            std::string::size_type begin = 0;
            std::string::size_type end   = 0;
            if (!raFindIncludeLine(text, pattern, begin, end))
                break;

            const unsigned int bytes = *outBytes;
            const char* const  data  = static_cast<const char*>(*outData);

            std::string spliced;
            spliced.reserve(text.size() + bytes);
            spliced.append(text, 0, begin);
            if (data && bytes)
                spliced.append(data, bytes);
            spliced.append(text, end, text.size() - end);

            // std::string::operator=(char const*) at so an embedded NUL truncates
            // here in the original too.
            sub->text = spliced.c_str();
        }
    }

    return 0;
}

// sub_10037450, 0x10037450, 21 B - ID3DXInclude::Close, slot +0x04. `bf_free(pData); return 0;`
// with no null test of its own.
long __stdcall raIncludeClose(void* /*self*/, const void* data)
{
    ::renddx9abi::engineFree(const_cast<void*>(data));
    return 0;
}

void* const kRaIncludeVtable[2] =
{
    reinterpret_cast<void*>(&raIncludeOpen),     // +0x00
    reinterpret_cast<void*>(&raIncludeClose)     // +0x04
};

void* raIncludeVtable()
{
    return const_cast<void**>(kRaIncludeVtable);
}

} // anonymous namespace


// Shader-directory loading helpers.
// Flag bit 0 loads bare shaders/*.mfx; bit 1 scans mod-prefixed rashader*.fx;
// bit 2 additionally builds those shaders immediately. STM/SM/BM master shaders use the
// meta-effect path and are excluded from the ordinary scan.
// [PLATFORM] compatibility builds retain a reporting seam because the host file-list ABI
// is unavailable; native builds perform the scan.
namespace {

// sub_10075180 + the "mfx" literal at 0x101E1C00, compared with sub_100750C0(3).
bool raPathExtensionIs(const char* path, const char* extension)
{
    const char* const dot = ::strrchr(path, '.');
    if (!dot)
        return false;
    return ::_stricmp(dot + 1, extension) == 0;
}

// sub_10041AC0 - see the block above.  The argument is already lowercased.
bool raIsLoadableShaderFx(const char* lowered)
{
    bool candidate = false;
    if (::strstr(lowered, "rashader") != 0 && ::strstr(lowered, ".fx") != 0)
        candidate = (::strstr(lowered, "common") == 0);

    return candidate
        && ::_stricmp(lowered, "rashaderSTM.fx") != 0
        && ::_stricmp(lowered, "rashaderSM.fx")  != 0
        && ::_stricmp(lowered, "rashaderBM.fx")  != 0;
}

} // anonymous namespace

int raLoadAllShaders(RaShaderTable* table, unsigned char flags)
{
    static_cast<EffectManager*>(g_effectManager)->resetEffects();

    {
        const vc71::string defaultName("default");
        table->defaultShader = findOrCreateShader(table, defaultName, false);   // +0x84
    }

    int created = 0;

#if BF2_PLUGIN_ABI_NATIVE
    std::list< ::dice::hfe::io::FileInfo> files;

    if (::dice::hfe::fileManager)
    {
        const ::renddx9abi::HostString bare =
            ::renddx9abi::makeHostString(vc71::string("shaders/"));
        ::dice::hfe::fileManager->findFiles(bare, false, files);         // vslot +0x34
    }

    if ((flags & 1u) != 0)
    {
        for (std::list< ::dice::hfe::io::FileInfo>::const_iterator it = files.begin();
             it != files.end(); ++it)
        {
            if (!raPathExtensionIs(it->name.c_str(), "mfx"))
                continue;

            vc71::string full("shaders/");
            full += it->name.c_str();

            // The flag is the table's own +0xB4 - the same byte refillMemoryFiles reads one
            // indirection later to decide whether a MetaFx writes its generated files out.
            static_cast<EffectManager*>(g_effectManager)->loadMetaFx(full, table->byteB4 != 0);
        }
    }

    static_cast<EffectManager*>(g_effectManager)->refillMemoryFiles();

    vc71::string modRoot("shaders/");
    if (g_Renderer)
        applyModPrefix(modRoot);

    files.clear();                                                       // sub_10043B60(0, ...)
    if (::dice::hfe::fileManager)
    {
        const ::renddx9abi::HostString prefixed = ::renddx9abi::makeHostString(modRoot);
        ::dice::hfe::fileManager->findFiles(prefixed, false, files);
    }

    if ((flags & 2u) != 0)
    {
        for (std::list< ::dice::hfe::io::FileInfo>::const_iterator it = files.begin();
             it != files.end(); ++it)
        {
            // sub_10074E80 is `copy then sub_10074D10`, and sub_10074D10 is the pathToLower this
            // file already uses twice above; written out rather than reaching into RaShader.cpp's
            // file-local copy of the pair.
            vc71::string lowered(it->name.c_str());
            pathToLower(lowered);
            if (!raIsLoadableShaderFx(lowered.c_str()))
                continue;

            vc71::string stem;
            raCacheStem(stem, lowered);
            if (stem.size() < 8u)
                continue;                                                 // see the note above

            const vc71::string name(stem.c_str() + 8);                    // substr(8) - "rashader"
            RaShader* const shader = findOrCreateShader(table, name, false);

            if ((flags & 4u) != 0 && shader)
                shader->slot04();

            ++created;
        }
    }

    ::renddx9abi::logf("Ra: shaders/* load pass - %u file(s) scanned, %d Ra shader(s) created "
                       "(flags 0x%02X)",
                       static_cast<unsigned int>(files.size()), created,
                       static_cast<unsigned int>(flags));
#else
    (void)flags;
    ::renddx9abi::logf("[FRONTIER] dice::hfe::dx9rend::Ra - the shaders/* load pass (sub_10042150) "
                       "is SKIPPED in the retail-ABI flavour: its directory scan needs FileManager "
                       "vslot +0x34, whose VC7.1 std::list<io::FileInfo> out-parameter this build "
                       "does not model. The native flavour runs it. No value is fabricated; the "
                       "default shader above IS created, as it is on both paths.");
#endif

    return created;
}


} // namespace Ra


} // namespace dx9rend

} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
