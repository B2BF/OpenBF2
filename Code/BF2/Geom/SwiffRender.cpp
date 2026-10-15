// Flash rasterizer for menu frames, textures, masks, glyphs and credits.
// [INFERRED] source placement. ModuleGeom publishes the singleton on core event (10,1)
// and removes it on (10,2); beginFrame/endFrame bracket the menu's presentation.
// Font lookup guards remain until FontRenderer/FontSystem are available and verified
// through their draw paths. [RETIRE-WHEN: RW-80]
// Missing ServerLogoManager fails texture loading. VideoTarget owns movie texture reset handling.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendDX9/ShaderParameter.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
// RW-266: the host interfaces this file dispatches into, typed in this flavour - the
// SwiffHost, the Swiff player and the server-logo manager (see hostSlot's wrappers below).
#include "../Menu/ISwiffHost.h"
#include "../Menu/ISwiffPlayer.h"
#include "../Menu/ServerLogoManager.h"

#include <map>
#include <new>
#include <set>
#include <stddef.h>
#include <stdlib.h>     // [TEMP] atoi for the BF2_RENDDX9_SWIFFLOG stream-tracer gate
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <vector>

namespace dice {
namespace hfe {

namespace dx9rend {
// The seams this class rides on - all in the RendDX9 TUs.
extern void* g_shaderManager;                                       // dword_1023B97C
extern void* g_stateManager;                                        // dword_1023B984
extern void* g_deviceCaps;                                          // dword_1023B980
void* shaderManagerGetEffect(void*, const char*);
class Effect;
void* effectGetShaderParameter(Effect*, const std::string&);
void* effectFindTechniqueRecord(void* effect, const char* name);
void  effectSetTechniqueRecord(void* effect, void* record);
unsigned int effectBegin(void* effect);
void  effectBeginPass(void* effect, unsigned int index);
void  effectEnd(void* effect);
void* effectD3dxHandle(void* effect);
int   bufferManagerSwapPool(int value);
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  vertexBufferRelease(void* wrapper);                           // the shipped inline --ref dance
void* vertexDeclarationCreate(const void* elements);
void  vertexDeclarationBind(void* wrapper);
void  vertexDeclarationRelease(void* wrapper);                      // the shipped inline --ref dance
void  stateManagerPush();
void  stateManagerPop();
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);
void  rendererDrawPrimitiveUP(unsigned int primCount, const void* data,
                              unsigned int stride, unsigned int type);
bool  rendererBeginScene(const unsigned int* clearRgb, bool doClear);    // RendDX9Draw.cpp
bool  rendererEndScene();
bool  rendererPresent(unsigned int mode);
} // namespace dx9rend

namespace geom {

// From ModuleGeom's own registration (sub_100518A0(822323007, "dice.hfe.geom.SwiffRender")) and the
// SingletonRegInfo the core-event handler builds (-1207837439 == 0xB801DD01).
extern const unsigned int CID_SwiffRender  = 822323007u;
extern const unsigned int IID_ISwiffRender = 0xB801DD01u;

// GeomMisc/FontSystem/SwiffFont.cpp - the real font stack (RW-80). createSwiffFont resolves the
// FontSystem singleton, gets the DifFont for (name,size), and wraps it in a SwiffFontObject whose
// vtable the font slots below dispatch through. Declared at namespace-geom scope (NOT in the
// anonymous namespace below) so it links to SwiffFont.cpp's external definition.
void* createSwiffFont(const char* name, int size, const wchar_t* initialText);

// The two setters SwiffPlayer drives on the font wrapper. The shipped slots poke raw byte offsets
// (+120 box width, +136 alignment; +104 scroll index) into the wrapper, but SwiffFontObject is a
// [CHOSEN] named-member layout, not the shipped byte layout, so those pokes would land in the
// inline text buffer. These typed seams set the real named members instead. Defined in SwiffFont.cpp.
void swiffFontConfigure(void* font, float boxWidth, int align);
void swiffFontSetScrollIndex(void* font, int value);

// MenuRend/Video.cpp - the Bink movie seams the '?' fill uses. Declared at namespace-geom scope so
// they link to Video.cpp's external definitions.
void* videoCreateAndOpen(const char* path);     // createInstanceByName("dice.hfe.geom.Video") + vslot +0x18 open
void  videoDoFrame(void* video);                // vslot +0x20
void* videoFrameD3DTexture(void* video);        // vslot +0x1C getFrame -> the VideoTarget's texture

namespace {

// One-time boundary notes - same shape as BF2Render.cpp's noteOnce.
void noteOnce(unsigned bit, const char* msg)
{
    static unsigned s_noted = 0;
    if (s_noted & (1u << bit))
        return;
    s_noted |= (1u << bit);
    ::renddx9abi::logf("%s", msg);
}

// __thiscall dispatch on host-owned objects (SwiffHost, EventManager, files, fonts, videos).
template <typename Fn>
Fn hostSlot(void* obj, unsigned byteOffset)
{
    void** vtbl = *reinterpret_cast<void***>(obj);
    return reinterpret_cast<Fn>(vtbl[byteOffset / 4]);
}

typedef unsigned char (__thiscall* HostByteFn)(void*);
typedef int           (__thiscall* HostIntFn)(void*);
typedef float         (__thiscall* HostFloatFn)(void*);
typedef void          (__thiscall* HostVoidIntFn)(void*, int);
typedef void          (__thiscall* HostVoidFn)(void*);
typedef void          (__thiscall* HostRectFn)(void*, int*);


// RW-266: SwiffHost adapters use typed calls natively and fixed slots in plugin builds.
// [UNRESOLVED] role names follow ISwiffHost.

inline ui::ISwiffHost* swiffHostOf(void* host)
{
    return static_cast<ui::ISwiffHost*>(static_cast<IBase*>(host));
}
#define SWIFFHOST_CALL(typedCall, rawCall) return typedCall
inline void hostRequestRestartMenu(void* h)                                     // +0x264, 612
{ SWIFFHOST_CALL(swiffHostOf(h)->requestRestartMenu(), hostSlot<HostVoidFn>(h, 612)(h)); }
inline void hostSetFlagE80(void* h, int v)                                      // +0x274, 628
{ SWIFFHOST_CALL(swiffHostOf(h)->setFlagE80(v != 0), hostSlot<HostVoidIntFn>(h, 628)(h, v)); }
inline bool hostGetFlagE80OrNotE81(void* h)                                     // +0x278, 632
{ SWIFFHOST_CALL(swiffHostOf(h)->getFlagE80OrNotE81(), hostSlot<HostByteFn>(h, 632)(h) != 0); }
inline void hostSetFlagE81(void* h, int v)                                      // +0x27C, 636
{ SWIFFHOST_CALL(swiffHostOf(h)->setFlagE81(v != 0), hostSlot<HostVoidIntFn>(h, 636)(h, v)); }
inline bool hostGetFlagE81(void* h)                                             // +0x280, 640
{ SWIFFHOST_CALL(swiffHostOf(h)->getFlagE81(), hostSlot<HostByteFn>(h, 640)(h) != 0); }
inline float hostGetFloatF0C(void* h)                                           // +0x328, 808
{ SWIFFHOST_CALL(swiffHostOf(h)->getFloatF0C(), hostSlot<HostFloatFn>(h, 808)(h)); }
inline void hostSetFlagF10(void* h, int v)                                      // +0x330, 816
{ SWIFFHOST_CALL(swiffHostOf(h)->setFlagF10(v != 0), hostSlot<HostVoidIntFn>(h, 816)(h, v)); }
inline bool hostGetFlagF10(void* h)                                             // +0x334, 820
{ SWIFFHOST_CALL(swiffHostOf(h)->getFlagF10(), hostSlot<HostByteFn>(h, 820)(h) != 0); }
inline int hostGetIntF14(void* h)                                               // +0x338, 824
{ SWIFFHOST_CALL(swiffHostOf(h)->getIntF14(), hostSlot<HostIntFn>(h, 824)(h)); }
inline void hostSetIntF14(void* h, int v)                                       // +0x33C, 828
{ SWIFFHOST_CALL(swiffHostOf(h)->setIntF14(v), hostSlot<HostVoidIntFn>(h, 828)(h, v)); }
inline void hostSetFlagF18(void* h, int v)                                      // +0x340, 832
{ SWIFFHOST_CALL(swiffHostOf(h)->setFlagF18(v != 0), hostSlot<HostVoidIntFn>(h, 832)(h, v)); }
inline bool hostGetFlagF18(void* h)                                             // +0x344, 836
{ SWIFFHOST_CALL(swiffHostOf(h)->getFlagF18(), hostSlot<HostByteFn>(h, 836)(h) != 0); }
inline void hostGetRect(void* h, int* out)                                      // +0x348, 840
{ SWIFFHOST_CALL(swiffHostOf(h)->getRect(out), hostSlot<HostRectFn>(h, 840)(h, out)); }
#undef SWIFFHOST_CALL

// Shader-parameter operations. effectGetShaderParameter returns a 12-byte
// ShaderParameter (Effect.cpp), NOT the 76-byte FXParameterImpl. The mislabel never bit because the
// two vtables are shape-compatible - both run twelve slots with setValue/setBool/setInt at
// +0x18/+0x1C/+0x20 - and the ShaderParameter's slots are pure forwards to its owner's. So each call
// below is TWO hops: ShaderParameter::slot -> owner (an FXParameterImpl after bind's populate pass,
// or the bare process-wide ParameterOwner on the lazy-miss path, whose slots stop at a frontier).
// The recovered handle is now a real typed interface; callers do not spell its ABI slots.

void paramSetValue(void* p, const void* data, unsigned int count)
{
    if (!p) return;
    static_cast<dx9rend::ShaderParameter*>(p)->setValue(data, count);
}
void paramRelease(void* p)
{
    if (!p) return;
    static_cast<dx9rend::ShaderParameter*>(p)->release();
}
void* paramGetHandle(void* p)     // vslot +0x0C, sub_10021190: `return m_handle`
{
    if (!p) return 0;
    return static_cast<dx9rend::ShaderParameter*>(p)->getD3DXParam();
}

// The StateManager's own COM vtable, exactly as the shipped call sites reach it.
typedef int (__stdcall* SmSetRenderStateFn)(void*, unsigned int, unsigned int);
void smSetRenderState(unsigned int state, unsigned int value)
{
    void* sm = ::dice::hfe::dx9rend::g_stateManager;
    if (!sm) return;
    void** vtbl = *reinterpret_cast<void***>(sm);
    reinterpret_cast<SmSetRenderStateFn>(vtbl[28 / 4])(sm, state, value);   // COM slot 7
}

// fx->SetTexture(handle, tex) / fx->CommitChanges() on the effect's D3DX interface, as the shipped
// bodies call them.
void fxSetTexture(void* fx, void* handle, void* texture)
{
    if (!fx) return;
    static_cast<ID3DXEffect*>(fx)->SetTexture(::renddx9abi::d3dxHandle(handle),
                                              static_cast<IDirect3DBaseTexture9*>(texture));
}
void fxCommitChanges(void* fx)
{
    if (!fx) return;
    static_cast<ID3DXEffect*>(fx)->CommitChanges();
}


// Position and packed color, 16 bytes per vertex, followed by D3DDECL_END.

const unsigned short kSwiffDeclElements[3][4] =
{
    { 0, 0,  0x0002, 0x0000 },      // stream 0, offset 0,  FLOAT3,   method 0 | position, index 0
    { 0, 12, 0x0004, 0x000A },      // stream 0, offset 12, D3DCOLOR, method 0 | color,    index 0
    { 0xFF, 0, 0x0011, 0x0000 },    // D3DDECL_END {0xFF, 0, unused(17), 0, 0, 0}
};


// Five inline 84-byte styles: two fills, a line, a flush latch, and a line latch.

struct SwiffStyle
{
    unsigned int  mode;           // +0   0 off, 1 solid, 2 textured-wrap, 3 textured-clamp
    unsigned char rgba[4];        // +4   cxform-transformed color (solid arm)
    int           texId;          // +8   registry id, -1 none
    int           texW;           // +12
    int           texH;           // +16
    float         texMatrix[6];   // +20  the SWF 2x3 texture matrix {m00,m01,tx, m10,m11,ty}
    float         cxform[8];      // +44  {mulR,addR, mulG,addG, mulB,addB, mulA,addA}
    unsigned char hasAdditive;    // +76  any add component > 1.0 (the texture setter computes it)
    unsigned char pad4D[3];
    float         width;          // +80  ctor 1.0; lineStyleWidth stores the raw dword here
};
static_assert(sizeof(SwiffStyle) == 84, "five 84-byte style blocks at +128..+547");


// Texture-registry record; the growable ID-indexed array lives at SwiffRender+884.

struct SwiffTextureRecord            // ctor sub_101BEFA0, init sub_101BF000
{
    int           id;               // +0
    IUnknown*     texture;          // +4   IDirect3DBaseTexture9*, AddRef'd by init
    void*         video;            // +8   dice.hfe.geom.Video for '?' movie fills (withheld)
    std::string  name;             // +12..39
    int           refCount;         // +40  ctor 1; every successful lookup addRefs
    unsigned int  width;            // +44
    unsigned int  height;           // +48
};

// The id-indexed record vector (sub_101B9AD0 constructs it empty); a freed slot holds null.
struct SwiffTextureRegistry
{
    std::vector<SwiffTextureRecord*> slots;

    unsigned int count() const { return static_cast<unsigned int>(slots.size()); }

    // sub_101B99C0 - find by name, case-insensitive, skipping freed slots.
    SwiffTextureRecord* findByName(const std::string& name) const
    {
        for (unsigned int i = 0; i < count(); ++i)
            if (slots[i] && _stricmp(slots[i]->name.c_str(), name.c_str()) == 0)
                return slots[i];
        return 0;
    }

    // sub_101B9AB0 - by id; -1 and out-of-range yield null.
    SwiffTextureRecord* byId(int id) const
    {
        if (id == -1) return 0;
        return slots[id];            // the shipped body does not range-check either
    }

    // sub_101B9B90 - insert into the first free slot, else append (sub_101BA410 grows).
    unsigned int insert(SwiffTextureRecord* rec)
    {
        for (unsigned int i = 0; i < count(); ++i)
        {
            if (!slots[i])
            {
                slots[i] = rec;
                return i;
            }
        }
        slots.push_back(rec);
        return count() - 1;
    }
};

// sub_101BEFA0 - the record constructor.
SwiffTextureRecord* recordConstruct(void* storage)
{
    SwiffTextureRecord* rec = static_cast<SwiffTextureRecord*>(storage);
    rec->id      = 0;
    rec->texture = 0;
    rec->video   = 0;
    new (&rec->name) std::string();
    rec->refCount = 1;
    return rec;
}

// sub_101BF000 - init: id, texture (AddRef'd), name, dims. The info-pointer argument the file arm
// passes is unused in the shipped body too.
void recordInit(SwiffTextureRecord* rec, int id, IUnknown* texture, const char* name,
                unsigned int w, unsigned int h)
{
    rec->id      = id;
    rec->texture = texture;
    if (texture)
        texture->AddRef();
    rec->name    = name;
    rec->width   = w;
    rec->height  = h;
}

// sub_101BF040 / sub_101BF060 - addRef, and release-with-destroy-at-zero.
void recordAddRef(SwiffTextureRecord* rec) { ++rec->refCount; }

int recordRelease(SwiffTextureRecord* rec)
{
    const int remaining = --rec->refCount;
    if (!remaining)
    {
        if (rec->texture) { rec->texture->Release(); rec->texture = 0; }
        if (rec->video)
        {
            void** vtbl = *reinterpret_cast<void***>(rec->video);
            reinterpret_cast<HostIntFn>(vtbl[2])(rec->video);       // vslot +0x08 release
        }
        rec->name.~basic_string();
        ::renddx9abi::engineFree(rec);
        return 0;
    }
    return remaining;
}

// sub_101BEFD0 - the unconditional dispose the registry teardown uses (ignores the refcount).
void recordDispose(SwiffTextureRecord* rec)
{
    if (rec->texture) { rec->texture->Release(); rec->texture = 0; }
    if (rec->video)
    {
        void** vtbl = *reinterpret_cast<void***>(rec->video);
        reinterpret_cast<HostIntFn>(vtbl[2])(rec->video);
    }
    rec->name.~basic_string();
}

// sub_101B9AE0 + sub_101B9B60 - clear all records, then free the array.
void registryDestroy(SwiffTextureRegistry& reg)
{
    for (unsigned int i = 0; i < reg.count(); ++i)
    {
        if (reg.slots[i])
        {
            recordDispose(reg.slots[i]);
            ::renddx9abi::engineFree(reg.slots[i]);
            reg.slots[i] = 0;
        }
    }
    reg.slots.clear();
}


// Caches GSModDirectory on first use, defaulting to "No Current Mod".
std::string& modDirectory()
{
    static std::string s_dir;
    static bool         s_read = false;    // byte_1023B170 ships as 1 = "needs reading"
    if (!s_read)
    {
        ::dice::hfe::dx9rend::readStringSetting("GSModDirectory", s_dir);
        if (s_dir.c_str()[0] == '\0')
            s_dir = "No Current Mod";      // the shipped default string
        s_read = true;
    }
    return s_dir;
}

int registryGetOrLoad(SwiffTextureRegistry& reg, const std::string& name,
                      unsigned int* outW, unsigned int* outH, int skipCache)
{
    if (!skipCache)
    {
        SwiffTextureRecord* hit = reg.findByName(name);
        if (hit)
        {
            recordAddRef(hit);
            *outW = hit->width;
            *outH = hit->height;
            return hit->id;
        }
    }

    void* device = ::dice::hfe::dx9rend::renderDevice();
    if (!device)
        return -1;

    std::string path;
    const char*  raw   = name.c_str();
    const bool   empty = (raw[0] == '\0');

    if (!empty && ::strstr(raw, "http://") != 0)    // the shipped uses find() - anywhere, not prefix
    {
        // The server-logo arm: ServerLogoManager (a host-side singleton) maps the URL onto its
        // "@HOME@\LogoCache\" download. Guarded here because nothing in this image registers it -
        // the shipped build dereferences the resolution unchecked.
        void* logoMgr = 0;
        if (::dice::hfe::classManager)
            logoMgr = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("ServerLogoManager"));
        if (!logoMgr)
        {
            noteOnce(0, "[FRONTIER] SwiffRender texture loader: an http:// logo was requested and "
                        "the ServerLogoManager singleton is absent - returning -1 (the shipped "
                        "build dereferences it unchecked)");
            return -1;
        }
        // Convert registry VC7.1 keys to the host string type before calling the singleton.
        // RW-266: typed in this flavour - IServerLogoManager slot 8 (+0x20 download)
        // and slot 6 (+0x18 parseUrl), and HostString IS the host's std::string here.
        IServerLogoManager* const logos =
            static_cast<IServerLogoManager*>(static_cast<IBase*>(logoMgr));
        const renddx9abi::HostString url(raw);
        if (!logos->download(url))                                           // +0x20 download
            return -1;
        renddx9abi::HostString dir, file;
        logos->parseUrl(url, dir, file);                                     // +0x18 parseUrl
        path  = "@HOME@\\LogoCache\\";
        path += dir.c_str();
        path += file.c_str();
    }
    else if (!empty && raw[0] == '@')
    {
        path  = modDirectory();
        path += "/";
        path += raw + 1;
        ::renddx9abi::EngineFileData probe = ::renddx9abi::readEngineFile(path.c_str());
        if (!probe.data)
        {
            path  = "mods/bf2/";
            path += raw + 1;
        }
        else
        {
            ::renddx9abi::freeEngineFile(probe);
        }
    }
    else if (!empty && raw[0] == '$')
    {
        path = raw + 1;
    }
    else if (!empty && raw[0] == '#')
    {
        path  = "@ROOT@/";
        path += raw + 1;
    }
    else if (!empty && raw[0] == '?')
    {
        // Movie fills try the active mod, then base BF2. A successful Video owns playback;
        // the fill retains a fixed 256x256 authoring size regardless of movie dimensions.
        path  = modDirectory();
        path += "/Movies/";
        path += raw + 1;
        ::renddx9abi::EngineStream probe;
        if (!probe.open(path))
        {
            path  = "mods/bf2/Movies/";
            path += raw + 1;
            if (!probe.open(path))
                return -1;
        }
        void* video = videoCreateAndOpen(path.c_str());
        if (!video)
        {
            ::renddx9abi::logf("SwiffRender '?' movie fill: \"%s\" exists for the FileManager but "
                               "Video::open refused it - returning -1, no movie", path.c_str());
            return -1;                                            // ~EngineStream releases the probe
        }
        probe.close();
        void* storage = ::renddx9abi::engineAlloc(sizeof(SwiffTextureRecord));
        if (!storage)
        {
            void** vv = *reinterpret_cast<void***>(video);
            reinterpret_cast<HostIntFn>(vv[2])(video);            // vslot +0x08 release
            return -1;
        }
        SwiffTextureRecord* rec = recordConstruct(storage);
        const unsigned int  id  = reg.insert(rec);
        recordInit(rec, (int)id, 0, name.c_str(), 256u, 256u);
        rec->video = video;
        recordAddRef(rec);                                        // the caller's reference
        if (outW) *outW = 256u;
        if (outH) *outH = 256u;
        return rec->id;
    }
    else
    {
        path  = "Menu/External/FlashMenu/";
        path += raw;
    }

    ::renddx9abi::EngineFileData file = ::renddx9abi::readEngineFile(path.c_str());
    if (!file.data)
    {
        // The shipped retry: whatever the routing produced, fall back to the FlashMenu directory.
        path  = "Menu/External/FlashMenu/";
        path += raw;
        file = ::renddx9abi::readEngineFile(path.c_str());
        if (!file.data)
            return -1;
    }

    // D3DXIMAGE_INFO: Width at +0, Height at +4 (the caller reads only those two).
    unsigned int info[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    D3DXGetImageInfoFromFileInMemory(file.data, file.size,
                                     reinterpret_cast<D3DXIMAGE_INFO*>(info));

    IDirect3DTexture9* texture = 0;
    const long hr = D3DXCreateTextureFromFileInMemory(static_cast<IDirect3DDevice9*>(device),
                                                      file.data, file.size, &texture);
    ::renddx9abi::freeEngineFile(file);
    if (hr < 0 || !texture)
        return -1;

    // Report dimensions from image metadata, not the surface descriptor.
    *outW = info[0];
    *outH = info[1];

    void* storage = ::renddx9abi::engineAlloc(sizeof(SwiffTextureRecord));
    if (!storage)
    {
        static_cast<IUnknown*>(texture)->Release();
        return -1;
    }
    SwiffTextureRecord* rec = recordConstruct(storage);
    const unsigned int  id  = reg.insert(rec);
    recordInit(rec, (int)id, static_cast<IUnknown*>(texture), name.c_str(), info[0], info[1]);
    static_cast<IUnknown*>(texture)->Release();     // the record now owns the reference
    recordAddRef(rec);                              // the caller's reference, as on a cache hit
    return rec->id;
}

// sub_101BA390 - register an already-created texture (the upload slots). Empty name, zero dims.
int registryRegisterUploaded(SwiffTextureRegistry& reg, void* texture)
{
    if (!texture)
        return -1;
    void* storage = ::renddx9abi::engineAlloc(sizeof(SwiffTextureRecord));
    if (!storage)
        return -1;
    SwiffTextureRecord* rec = recordConstruct(storage);
    const unsigned int  id  = reg.insert(rec);
    recordInit(rec, (int)id, static_cast<IUnknown*>(texture), "", 0, 0);
    return rec->id;
}

// sub_101B9A60 - release by id; a record that reaches zero frees its slot.
bool registryReleaseId(SwiffTextureRegistry& reg, int id)
{
    if (id == -1)
        return false;
    SwiffTextureRecord* rec = reg.slots[id];
    if (!rec)
        return false;
    if (!recordRelease(rec))
        reg.slots[id] = 0;
    return true;
}


// Forty falling credits names, recycled from bottom to top.
// Font creation is withheld (RW-80); rows without fonts are skipped.

struct CreditsRain
{
    struct Point { void* font; float y; };
    Point         points[40];       // +0..319   (ctor zeroes only the font slots)
    unsigned int  pad320;           // +320      untouched by the ctor
    std::vector<void*> fonts;       // +324      the round-robin font list
    unsigned char inited;           // +336
    unsigned char pad[3];
};

void creditsRainPush(CreditsRain* r, void* font)
{
    r->fonts.push_back(font);
}


// One entry of the fonts vector at +956: a name and the object shutdown releases (+28 in the
// image). Nothing in this build creates one until RW-80 retires.
struct SwiffFontEntry
{
    std::string name;              // +0
    void*        object;            // +28
};


// THE object - 0x460 = 1120 bytes in the image, RefFactory alloc. The offsets beside the members
// are the shipped ones; the members from the registry on are native containers.

struct SwiffRender
{
    void**              vtPrimary;
    void**              vtDevLost;        // +4     0x1021E1B4  {onLost, onReset, onDestroy}
    void**              vtEcCore;         // +8     0x1021E1B0  {handleEvent}
    int                 refCount;         // +12
    unsigned char       deviceLost;       // +16    set by onDeviceLost, cleared by onDeviceReset
    unsigned char       wasLostSticky;    // +17    slot 65 reads, slot 66 clears
    unsigned char       pad18[2];
    float               scaleX;           // +20    ctor 1.0; setScale
    float               scaleY;           // +24
    unsigned int        movieWidth;       // +28    setMovieWidth
    int                 movieHeight;      // +32    setMovieHeight (signed - beginDisplay wraps it)
    unsigned int        bgColor;          // +36    bytes r,g,b at 36/37/38; slot 45 setter
    int                 mouseX;           // +40
    int                 mouseY;           // +44
    int                 mouseButtons;     // +48
    void*               decl;             // +52    the {position, color} declaration wrapper
    float*              vbBase;           // +56    the locked ring buffer base
    unsigned int        vbCursor;         // +60    in 16-byte vertices
    unsigned int        vbIndex;          // +64    0..2
    void*               vb[3];            // +68    three SWIFF_VBS, 10002 x 16, usage 0x208
    float               lastX;            // +80    the line strip's carried tail
    float               lastY;            // +84
    unsigned int        u88;              // +88    ctor 0, never read on recovered paths
    void*               object5C;         // +92    ctor 0; shutdown releases it if ever set
    void*               currentTexture;   // +96    the style applier's latch
    void*               effect;           // +100   "Shaders/SwiffMenu"
    void*               paramTransform;   // +104
    void*               paramDiffuse;     // +108
    void*               paramTexGenS;     // +112
    void*               paramTexGenT;     // +116
    void*               paramTexture;     // +120
    void*               paramTime;        // +124
    SwiffStyle          style[5];         // +128   fill0, fill1, line, fillLatch, lineLatch
    unsigned char       lastWasFill;      // +548
    unsigned char       pad549[3];
    float               mtxRemap[16];     // +552   movie -> viewport-subrect NDC remap
    float               mtxOrthoTwips[16];// +616   SWF twips ortho
    float               mtxOrthoPixels[16];// +680  the same ortho in pixels (x0.05)
    float               cxform[8];        // +744   setCxform's 32-byte block
    float               mtxCurrent[16];   // +776   setTransform's product; draws read the 2x3 of it
    float               solidColor[4];    // +840   the style applier's solid-diffuse latch
    int                 u856;             // +856   ctor -1, never read on recovered paths
    unsigned int        timeMs;           // +860   updateTime
    unsigned char       locked;           // +864
    unsigned char       pad865[3];
    int                 statCur;          // +868   slot 16's tracker
    int                 statPrev;         // +872
    int                 statMax;          // +876   ctor -1
    int                 statMin;          // +880   ctor 1000
    SwiffTextureRegistry registry;        // +884
    int                 u900;             // +900   untouched by every recovered body
    std::vector<std::string> strings904; // +904   dtor-destroyed; no recovered writer
    unsigned char       savedViewport[24];// +916   D3DVIEWPORT9, beginDisplay saves / endDisplay restores
    void*               lastVerts;        // +940   drawVertices' latches
    unsigned int        lastCount;        // +944
    unsigned char       stencilRef;       // +948   ctor 1; beginMask increments, wraps 255 -> 1
    unsigned char       pad949[3];
    int                 u952;             // +952   untouched
    std::vector<SwiffFontEntry*> fonts;   // +956   torn down in shutdown
    void*               released968;      // +968   shutdown releases both if set
    void*               released972;      // +972
    int                 u976;             // +976   ctor 0
    int                 u980;             // +980   ctor 0
    std::map<std::string, int> texTechniqueMap;   // +984   texture name -> technique-table index
    void*               techRecord[9];    // +996   {0, TS0, TS1, TS2, TS3, Shape, Wrap, Clamp, Line}
    void*               swiffHost;        // +1032
    void*               fontRenderer;     // +1036  null in this build (RW-80)
    std::set<void*>     fontSet;          // +1040  keyed by font pointer (slot 27's erase target)
    int                 u1052;            // +1052  untouched
    unsigned char       fontsDisabled;    // +1056  init 0; ECCore (11,3) sets it
    unsigned char       b1057;            // +1057  ctor 0
    unsigned char       pad1058[2];
    unsigned int        stats[6];         // +1060  beginFrame zeroes; draws increment
    float               trailDt;          // +1084  updateTime's second argument
    CreditsRain*        trail;            // +1088
    int                 vpW;              // +1092  beginDisplay's viewport w/h latch
    int                 vpH;              // +1096
    int                 u1100;            // +1100  untouched
    std::vector<std::string> credits;    // +1104  the credits image-name list (slots 61..64)
    int                 u1116;            // +1116  untouched
};

// The retail x86 layout. Every access is by member name, the two listener subobjects are found
// with offsetof and the object is allocated with sizeof, so x64 keeps its own offsets.
#if defined(_M_IX86)
static_assert(offsetof(SwiffRender, decl)           == 52,   "decl at +52");
static_assert(offsetof(SwiffRender, vb)             == 68,   "the VB ring at +68");
static_assert(offsetof(SwiffRender, effect)         == 100,  "the effect at +100");
static_assert(offsetof(SwiffRender, style)          == 128,  "style blocks at +128");
static_assert(offsetof(SwiffRender, lastWasFill)    == 548,  "the fill flag at +548");
static_assert(offsetof(SwiffRender, mtxRemap)       == 552,  "the remap matrix at +552");
static_assert(offsetof(SwiffRender, cxform)         == 744,  "the cxform at +744");
static_assert(offsetof(SwiffRender, mtxCurrent)     == 776,  "the current transform at +776");
static_assert(offsetof(SwiffRender, registry)       == 884,  "the registry at +884 (lea +374h)");
#endif

// The SwiffHost gate nearly every draw body opens with: vslot +632, "is the host busy/suppressed".
bool hostSuppressed(SwiffRender* s)
{
    if (!s->swiffHost)
        return false;
    return hostGetFlagE80OrNotE81(s->swiffHost);
}


// [TEMP] [CHOSEN, not recovered] BF2_RENDDX9_SWIFFLOG=N enables render-stream tracing:
// full details for the first N displays and every 100th thereafter, plus per-display summaries.
// [RETIRE-WHEN: the SwiffPlayer mispositioning is root-caused - strip this block and its call sites]

struct SwiffStreamTrace
{
    int      framesToLog;    // -1 = env not read yet; 0 = off
    int      displays;       // beginDisplay count
    bool     verbose;        // inside a fully-logged display
    unsigned nVerts, nStrips, nLines, nTransforms, nTex, nFills, nMasks, nGlyphs;
};
static SwiffStreamTrace g_sst = { -1, 0, false, 0, 0, 0, 0, 0, 0, 0, 0 };

static bool sstEnabled()
{
    if (g_sst.framesToLog < 0)
    {
        char v[16];
        const DWORD n = ::GetEnvironmentVariableA("BF2_RENDDX9_SWIFFLOG", v, sizeof(v));
        g_sst.framesToLog = (n > 0 && n < sizeof(v)) ? ::atoi(v) : 0;
        if (g_sst.framesToLog < 0)
            g_sst.framesToLog = 0;
        if (g_sst.framesToLog > 0)
            ::renddx9abi::logf("[swiff] stream tracer ON: first %d displays in full (then every 100th), "
                               "one summary line per display", g_sst.framesToLog);
    }
    return g_sst.framesToLog > 0;
}
static bool sstVerbose() { return g_sst.framesToLog > 0 && g_sst.verbose; }
#define SST(...) do { if (sstVerbose()) ::renddx9abi::logf(__VA_ARGS__); } while (0)


// sub_101531B0 - apply the cxform to four color bytes, clamped 0..255, packed little-endian
// r,g,b,a (so the dword reads 0xAABBGGRR in memory order r at +0).

unsigned int applyCxform(const float* cx, const unsigned char* rgba)
{
    unsigned int out = 0;
    for (int i = 0; i < 4; ++i)
    {
        float v = (float)rgba[i] * cx[2 * i] + cx[2 * i + 1];
        if (v >= 255.0f) v = 255.0f;
        else if (v <= 0.0f) v = 0.0f;
        out |= ((unsigned int)(unsigned char)(unsigned __int64)v) << (8 * i);
    }
    return out;
}


// sub_10153140 - advance the VB ring: unlock, next of three, cursor 0, lock discard.

float* ringAdvance(SwiffRender* s)
{
    using namespace ::dice::hfe::dx9rend;
    if (s->locked)
    {
        vertexBufferUnlock(s->vb[s->vbIndex]);
        s->locked = 0;
    }
    if (++s->vbIndex >= 3)
        s->vbIndex = 0;
    s->vbCursor = 0;
    float* base = static_cast<float*>(vertexBufferLock(s->vb[s->vbIndex], 0, 0, 0x2000));
    s->vbBase = base;
    if (base)
        s->locked = 1;
    return base;
}

// Forward declarations for the bodies that call one another.
void applyStyleState(SwiffRender* s, SwiffStyle* style);
void flushStrip(SwiffRender* s);


// sub_10153D00 - bind the technique, diffuse/texgen parameters, and texture for one style block.

void applyStyleState(SwiffRender* s, SwiffStyle* style)
{
    using namespace ::dice::hfe::dx9rend;

    switch (style->mode)
    {
    case 1: effectSetTechniqueRecord(s->effect, s->techRecord[5]); break;   // "Shape"
    case 2: effectSetTechniqueRecord(s->effect, s->techRecord[6]); break;   // "ShapeTextureWrap"
    case 3: effectSetTechniqueRecord(s->effect, s->techRecord[7]); break;   // "ShapeTextureClamp"
    default: break;
    }

    if (style->mode == 1)
    {
        // Solid: latch the diffuse floats, unbind the texture.
        s->currentTexture = 0;
        float diffuse[4];
        diffuse[0] = (float)style->rgba[0] * 0.0039215689f;
        diffuse[1] = (float)style->rgba[1] * 0.0039215689f;
        diffuse[2] = (float)style->rgba[2] * 0.0039215689f;
        diffuse[3] = (float)style->rgba[3] * 0.0039215689f;
        s->solidColor[0] = diffuse[0];
        s->solidColor[1] = diffuse[1];
        s->solidColor[2] = diffuse[2];
        s->solidColor[3] = diffuse[3];
        paramSetValue(s->paramDiffuse, diffuse, 0);
        fxSetTexture(effectD3dxHandle(s->effect), paramGetHandle(s->paramTexture), 0);
        return;
    }

    if (style->mode == 2 || style->mode == 3)
    {
        // Textured: alpha-blend states, the cxform-derived diffuse, the texgen rows, the texture.
        smSetRenderState(27, 1);     // D3DRS_ALPHABLENDENABLE
        smSetRenderState(19, 5);     // D3DRS_SRCBLEND  = srcalpha
        smSetRenderState(20, 6);     // D3DRS_DESTBLEND = invsrcalpha

        float diffuse[4];
        if (style->hasAdditive)
        {
            diffuse[0] = style->cxform[1] * 0.0039215689f;   // the ADD components / 255
            diffuse[1] = style->cxform[3] * 0.0039215689f;
            diffuse[2] = style->cxform[5] * 0.0039215689f;
            diffuse[3] = 1.0f;
        }
        else
        {
            diffuse[0] = style->cxform[0];                   // the MUL components, raw
            diffuse[1] = style->cxform[2];
            diffuse[2] = style->cxform[4];
            diffuse[3] = style->cxform[6];
        }
        paramSetValue(s->paramDiffuse, diffuse, 0);

        const float invW = 1.0f / (float)style->texW;
        const float invH = 1.0f / (float)style->texH;
        float texGenS[4], texGenT[4];
        texGenS[0] = invW * style->texMatrix[0];
        texGenS[1] = invW * style->texMatrix[1];
        texGenS[2] = 0.0f;
        texGenS[3] = invW * style->texMatrix[2];
        texGenT[0] = invH * style->texMatrix[3];
        texGenT[1] = invH * style->texMatrix[4];
        texGenT[2] = 0.0f;
        texGenT[3] = invH * style->texMatrix[5];
        paramSetValue(s->paramTexGenS, texGenS, 0);
        paramSetValue(s->paramTexGenT, texGenT, 0);

        if (style->texId != -1)
        {
            SwiffTextureRecord* rec = s->registry.byId(style->texId);

            // The per-texture technique override map: name -> index into techRecord[].
            if (rec)
            {
                const std::map<std::string, int>::const_iterator node =
                    s->texTechniqueMap.find(rec->name);
                if (node != s->texTechniqueMap.end())
                {
                    const unsigned int idx = static_cast<unsigned int>(node->second);
                    effectSetTechniqueRecord(s->effect, s->techRecord[idx]);
                }
            }

            void* d3dTexture = 0;
            if (rec)
            {
                if (rec->video)
                {
                    // The movie-fill arm: advance the Bink movie one frame (BinkWait-gated,
                    // so it only decodes when the clock says it is time) and bind the VideoTarget's
                    // D3D texture, which the frame was copied into.
                    videoDoFrame(rec->video);
                    d3dTexture = videoFrameD3DTexture(rec->video);
                }
                else
                {
                    d3dTexture = rec->texture;
                }
            }
            fxSetTexture(effectD3dxHandle(s->effect), paramGetHandle(s->paramTexture), d3dTexture);
            s->currentTexture = d3dTexture;
        }
    }
}


// sub_10154220 - flush the accumulated strip: apply the latched style, unlock, bind stream + decl,
// effect begin -> per-pass DrawPrimitive(trianglestrip, cursor-2), end, ring re-lock.

void flushStrip(SwiffRender* s)
{
    using namespace ::dice::hfe::dx9rend;
    if (!s->vbCursor)
        return;

    applyStyleState(s, &s->style[3]);          // the flush latch at +380

    vertexBufferUnlock(s->vb[s->vbIndex]);
    s->locked = 0;
    vertexBufferBindStream(s->vb[s->vbIndex], 0);
    vertexDeclarationBind(s->decl);

    const unsigned int passes = effectBegin(s->effect);

    for (unsigned int i = 0; i < passes; ++i)
    {
        effectBeginPass(s->effect, i);
        rendererDrawPrimitive(s->vbCursor - 2, 5 /* D3DPT_TRIANGLESTRIP */, 0);
        ++s->stats[1];                         // +1064
    }
    effectEnd(s->effect);

    ringAdvance(s);
    s->style[3].mode = 0;                      // the latch is consumed
}


// sub_10154A20 - the textured/solid fill collector: flush, then re-fill the ring with the
// tesselated triangle vertices and flush again under the latched fill style.

void collectFill(SwiffRender* s, const short* verts, unsigned int count)
{
    if (!s->swiffHost || hostSuppressed(s))
        return;

    flushStrip(s);
    float* base = s->vbBase;
    s->vbCursor = count;
    if (!base)
        return;

    float z = 1.0f;
    ::memcpy(&s->style[3], &s->style[0], sizeof(SwiffStyle));    // fill0 -> the flush latch

    // Convert latched color-transform floats directly to integers.
    // The additive pass uses RGB offsets and opaque alpha; the plain pass uses multipliers.
    unsigned char r, g, b, a;
    if (s->style[3].hasAdditive)
    {
        r = (unsigned char)(int)s->style[3].cxform[1];
        g = (unsigned char)(int)s->style[3].cxform[3];
        b = (unsigned char)(int)s->style[3].cxform[5];
        a = 255;
        z = 0.0f;
    }
    else
    {
        r = (unsigned char)(int)s->style[3].cxform[0];
        g = (unsigned char)(int)s->style[3].cxform[2];
        b = (unsigned char)(int)s->style[3].cxform[4];
        a = (unsigned char)(int)s->style[3].cxform[6];
    }
    const unsigned int color =
        ((unsigned int)b) | ((((((unsigned int)a << 8) | r) << 8) | g) << 8);

    float* v = base;
    for (unsigned int i = 0; i < count; ++i)
    {
        v[0] = (float)verts[2 * i]     - 0.5f;
        v[1] = (float)verts[2 * i + 1] - 0.5f;
        v[2] = z;
        *reinterpret_cast<unsigned int*>(v + 3) = color;
        v += 4;
    }

    flushStrip(s);
}


// The 67 primary slots. Shims are __fastcall(self, edx, ...) == __thiscall at the call site.


void RD9_THUNK sr_addRef(void* self RD9_THUNK_EDX)
{
    ++static_cast<SwiffRender*>(self)->refCount;
}
int RD9_THUNK sr_getRef(void* self RD9_THUNK_EDX)
{
    return static_cast<SwiffRender*>(self)->refCount;
}

void swiffRenderDestroy(SwiffRender* s);       // the dtor, below

int RD9_THUNK sr_release(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    const int remaining = --s->refCount;
    if (!remaining)
    {
        swiffRenderDestroy(s);
        ::renddx9abi::engineFree(s);
    }
    return remaining;
}

void* RD9_THUNK sr_queryInterfaceById(void* self RD9_THUNK_EDX, unsigned int iid)
{
    if (iid == 1u || iid == IID_ISwiffRender)
        return self;
    return 0;
}

int  RD9_THUNK sr_init(void* self RD9_THUNK_EDX);            // 0x10152200, below
int  RD9_THUNK sr_shutdown(void* self RD9_THUNK_EDX);        // 0x10153FE0, below

int RD9_THUNK sr_slot6(void* RD9_THUNK_EDX)
{
    return -1;
}

// upload an A8 alpha image (font glyph pages). D3DFMT_A8 == 28.
int RD9_THUNK sr_uploadAlpha(void* self RD9_THUNK_EDX, unsigned int w, unsigned int h, const void* data)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    void* device = ::dice::hfe::dx9rend::renderDevice();
    if (!device)
        return -1;
    IDirect3DTexture9* tex = 0;
    if (D3DXCreateTexture(static_cast<IDirect3DDevice9*>(device), w, h, 1, 0, D3DFMT_A8,
                          D3DPOOL_MANAGED, &tex) < 0)
        return -1;
    IDirect3DTexture9* t = tex;
    IDirect3DSurface9* surface = 0;
    t->GetSurfaceLevel(0, &surface);
    RECT rect = { 0, 0, (LONG)w, (LONG)h };
    if (D3DXLoadSurfaceFromMemory(surface, 0, 0, data, D3DFMT_A8, w, 0, &rect, 0xFFFFFFFF, 0) < 0)
    {
        if (surface) surface->Release();
        t->Release();
        return -1;
    }
    if (surface)
        surface->Release();
    const int id = registryRegisterUploaded(s->registry, t);
    t->Release();
    return id;
}

int RD9_THUNK sr_slot8(void* RD9_THUNK_EDX, int)
{
    return -1;
}

// Slot 9's descriptor: the gameswf image::image_base SwiffPlayer hands over (base/image.h),
// {data, width, height, pitch}. The interface spells it unsigned int*; the fields are read by
// name because the data pointer is pointer-sized on x64.
struct SwiffRgbaImage
{
    unsigned char* data;            // +0
    unsigned int   width;           // +4
    unsigned int   height;          // +8
    unsigned int   pitch;           // +12
};
#if defined(_M_IX86)
static_assert(sizeof(SwiffRgbaImage) == 16 && offsetof(SwiffRgbaImage, pitch) == 12,
              "the x86 descriptor is four dwords");
#endif

// upload an RGBA image: swap R<->B in place, load as A8R8G8B8 (21), register.
int RD9_THUNK sr_uploadRgba(void* self RD9_THUNK_EDX, unsigned int* desc)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    const SwiffRgbaImage* const image = reinterpret_cast<const SwiffRgbaImage*>(desc);
    void* device = ::dice::hfe::dx9rend::renderDevice();
    if (!device)
        return -1;
    IDirect3DTexture9* tex = 0;
    if (D3DXCreateTexture(static_cast<IDirect3DDevice9*>(device), image->width, image->height, 1, 0,
                          D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex) < 0)
        return -1;
    IDirect3DTexture9* t = tex;
    IDirect3DSurface9* surface = 0;
    t->GetSurfaceLevel(0, &surface);

    unsigned char* row = image->data;
    for (unsigned int y = 0; y < image->height; ++y)
    {
        unsigned char* px = row;
        for (unsigned int x = 0; x < image->width; ++x, px += 4)
        {
            const unsigned char tmp = px[0];
            px[0] = px[2];
            px[2] = tmp;
        }
        row += image->pitch;
    }

    RECT rect = { 0, 0, (LONG)image->width, (LONG)image->height };
    if (D3DXLoadSurfaceFromMemory(surface, 0, 0, image->data,
                                  D3DFMT_A8R8G8B8, image->pitch, 0, &rect, 0xFFFFFFFF, 0) < 0)
    {
        if (surface) surface->Release();
        t->Release();
        return -1;
    }
    if (surface)
        surface->Release();
    const int id = registryRegisterUploaded(s->registry, t);
    t->Release();
    return id;
}

// load by name through the routing loader. Note the raw char* argument.
int RD9_THUNK sr_loadTexture(void* self RD9_THUNK_EDX, const char* name,
                              unsigned int* outW, unsigned int* outH)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    unsigned int w = 0, h = 0;
    std::string key(name);
    const int id = registryGetOrLoad(s->registry, key, &w, &h, 0);
    *outW = w;
    *outH = h;
    if (sstEnabled())   // [TEMP] tracer
        ::renddx9abi::logf("[swiff] loadTexture(\"%s\") -> id=%d %ux%u", name ? name : "", id, w, h);
    return id;
}

int RD9_THUNK sr_releaseTexture(void* self RD9_THUNK_EDX, int id)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    return registryReleaseId(s->registry, id) ? 1 : 0;
}

// beginDisplay. Nine stack arguments; the first is unused in the shipped body too.
void RD9_THUNK sr_beginDisplay(void* self RD9_THUNK_EDX, int bgUnused, int vpX, int vpY,
                                int vpWidth, int vpHeight, float xMin, float xMax,
                                float yMin, float yMax)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.displays;
        g_sst.verbose = (g_sst.displays <= g_sst.framesToLog) || (g_sst.displays % 100 == 0);
        g_sst.nVerts = g_sst.nStrips = g_sst.nLines = g_sst.nTransforms = 0;
        g_sst.nTex = g_sst.nFills = g_sst.nMasks = g_sst.nGlyphs = 0;
        ::renddx9abi::logf("[swiff] beginDisplay #%d vp=(%d,%d,%d,%d) bounds=(%.0f,%.0f,%.0f,%.0f) "
                           "movie=%ux%d scale=(%.4f,%.4f) bg=%08X deviceLost=%d%s",
                           g_sst.displays, vpX, vpY, vpWidth, vpHeight, xMin, xMax, yMin, yMax,
                           s->movieWidth, s->movieHeight, s->scaleX, s->scaleY, (unsigned)bgUnused,
                           (int)s->deviceLost, g_sst.verbose ? " [FULL]" : "");
    }
    if (s->deviceLost)
        return;

    s->vpW = vpWidth;
    s->vpH = vpHeight;

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(renderDevice());
    stateManagerPush();

    device->GetViewport(reinterpret_cast<D3DVIEWPORT9*>(s->savedViewport));
    s->stencilRef = 1;

    const float invMovieW = 1.0f / (float)s->movieWidth;
    double movieH = (double)s->movieHeight;
    if (s->movieHeight < 0)
        movieH += 4294967300.0;                 // the shipped signed-wrap correction, verbatim
    const float invMovieH = (float)(1.0 / movieH);

    D3DVIEWPORT9 vp;
    vp.X      = (DWORD)(int)((float)vpX * s->scaleX);
    vp.Y      = (DWORD)(int)((float)vpY * s->scaleY);
    vp.Width  = (DWORD)(int)((float)vpWidth * s->scaleX);
    vp.Height = (DWORD)(int)((float)vpHeight * s->scaleY);
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    device->SetViewport(&vp);

    // M616: the SWF-twips orthographic projection over [xMin..xMax] x [yMin..yMax].
    ::memset(s->mtxOrthoTwips, 0, sizeof(s->mtxOrthoTwips));
    s->mtxOrthoTwips[10] = 1.0f;
    s->mtxOrthoTwips[15] = 1.0f;
    s->mtxOrthoTwips[0]  = 2.0f / (xMax - xMin);
    s->mtxOrthoTwips[5]  = -2.0f / (yMax - yMin);
    s->mtxOrthoTwips[12] = -((xMin + xMax) / (xMax - xMin));
    s->mtxOrthoTwips[13] = (yMin + yMax) / (yMax - yMin);

    // M680: the same bounds in pixels (twips x 0.05).
    const float pxMinX = xMin * 0.050000001f, pxMaxX = xMax * 0.050000001f;
    const float pxMinY = yMin * 0.050000001f, pxMaxY = yMax * 0.050000001f;
    ::memset(s->mtxOrthoPixels, 0, sizeof(s->mtxOrthoPixels));
    s->mtxOrthoPixels[10] = 1.0f;
    s->mtxOrthoPixels[15] = 1.0f;
    s->mtxOrthoPixels[0]  = 2.0f / (pxMaxX - pxMinX);
    s->mtxOrthoPixels[5]  = -2.0f / (pxMaxY - pxMinY);
    s->mtxOrthoPixels[12] = -((pxMinX + pxMaxX) / (pxMaxX - pxMinX));
    s->mtxOrthoPixels[13] = (pxMinY + pxMaxY) / (pxMaxY - pxMinY);

    // M552: the movie-space -> viewport-subrect NDC remap.
    ::memset(s->mtxRemap, 0, sizeof(s->mtxRemap));
    s->mtxRemap[10] = 1.0f;
    s->mtxRemap[15] = 1.0f;
    s->mtxRemap[0]  = (float)vpWidth * invMovieW;
    s->mtxRemap[5]  = (float)vpHeight * invMovieH;
    s->mtxRemap[12] = ((float)vpX + (float)vpX + (float)vpWidth) * invMovieW - 1.0f;
    s->mtxRemap[13] = 1.0f
        - ((float)vpY * invMovieH + (float)vpY * invMovieH)
        - (float)vpHeight * invMovieH;

    float combined[16];
    D3DXMatrixMultiply(reinterpret_cast<D3DXMATRIX*>(combined),
                       reinterpret_cast<const D3DXMATRIX*>(s->mtxOrthoTwips),
                       reinterpret_cast<const D3DXMATRIX*>(s->mtxRemap));
    paramSetValue(s->paramTransform, combined, 0);

    // The eleven state pairs, in the shipped order.
    smSetRenderState(19, 5);     // srcblend  = srcalpha
    smSetRenderState(20, 6);     // destblend = invsrcalpha
    smSetRenderState(27, 1);     // alphablendenable
    smSetRenderState(22, 1);     // cullmode = none
    smSetRenderState(23, 8);     // zfunc = always
    smSetRenderState(7, 0);      // zenable off
    smSetRenderState(14, 0);     // zwriteenable off
    smSetRenderState(52, 0);     // stencilenable off
    smSetRenderState(15, 0);     // alphatestenable off
    smSetRenderState(9, 2);      // fillmode = solid
    smSetRenderState(28, 0);     // fogenable off

    // Refresh every technique record - the effect may have been rebuilt since the last frame.
    s->techRecord[0] = 0;
    s->techRecord[1] = effectFindTechniqueRecord(s->effect, "TS0");
    s->techRecord[2] = effectFindTechniqueRecord(s->effect, "TS1");
    s->techRecord[3] = effectFindTechniqueRecord(s->effect, "TS2");
    s->techRecord[4] = effectFindTechniqueRecord(s->effect, "TS3");
    s->techRecord[5] = effectFindTechniqueRecord(s->effect, "Shape");
    s->techRecord[6] = effectFindTechniqueRecord(s->effect, "ShapeTextureWrap");
    s->techRecord[7] = effectFindTechniqueRecord(s->effect, "ShapeTextureClamp");
    s->techRecord[8] = effectFindTechniqueRecord(s->effect, "Line");

    ringAdvance(s);

    if (s->fontRenderer)
    {
        // sub_100A1470 - the FontRenderer's per-display begin. Withheld with the class.
        noteOnce(2, "[FRONTIER] SwiffRender::beginDisplay: the FontRenderer begin "
                    "(sub_100A1470) is withheld with the FontRenderer class");
    }
}

// endDisplay: flush, credits rain under a scissor, viewport restore, texture unbind.
void RD9_THUNK sr_endDisplay(void* self RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled())   // [TEMP] tracer
        ::renddx9abi::logf("[swiff] endDisplay #%d strips=%u verts=%u lines=%u transforms=%u fills=%u "
                           "tex=%u masks=%u glyphs=%u suppressed=%d",
                           g_sst.displays, g_sst.nStrips, g_sst.nVerts, g_sst.nLines, g_sst.nTransforms,
                           g_sst.nFills, g_sst.nTex, g_sst.nMasks, g_sst.nGlyphs,
                           (int)(!s->swiffHost || hostSuppressed(s)));
    if (s->deviceLost)
    {
        stateManagerPop();
        return;
    }
    if (!s->swiffHost || hostSuppressed(s))
    {
        stateManagerPop();
        return;
    }

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(renderDevice());
    flushStrip(s);
    // sub_100A15A0 - the FontRenderer flush. Withheld with the class (RW-80).

    if (s->trail && s->swiffHost
        && hostGetFlagF10(s->swiffHost))
    {
        // The credits window: host vslot +840 fills the rect, scaled and scissored, then the rain
        // draws inside it with D3DRS_SCISSORTESTENABLE (174).
        int hostRect[4] = { 0, 0, 0, 0 };
        hostGetRect(s->swiffHost, hostRect);
        RECT sc;
        sc.left   = (LONG)((float)hostRect[0] * s->scaleX);
        sc.top    = (LONG)((float)hostRect[1] * s->scaleY);
        sc.right  = (LONG)((float)hostRect[2] * s->scaleX);
        sc.bottom = (LONG)((float)hostRect[3] * s->scaleY);
        device->SetScissorRect(&sc);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);

        // sub_101B9070 - the rain update+draw, dt from updateTime.
        {
            CreditsRain* r = s->trail;
            if (s->swiffHost && hostGetFlagF18(s->swiffHost))
            {
                // restart requested: re-seed the forty rows from the font list
                float y = 15360.0f;
                for (int i = 0; i < 40; ++i)
                {
                    void* font = 0;
                    // sub_101B8F70 - the round-robin next font
                    if (!r->fonts.empty())
                    {
                        const int idx = hostGetIntF14(s->swiffHost);
                        const unsigned int n = (unsigned int)r->fonts.size();
                        font = r->fonts[idx];
                        unsigned int nextIdx = (unsigned int)idx + 1;
                        if (nextIdx >= n)
                            nextIdx = 0;
                        hostSetIntF14(s->swiffHost, (int)nextIdx);
                    }
                    r->points[i].font = font;
                    r->points[i].y    = y;
                    y += 400.0f;
                }
                hostSetFlagF18(s->swiffHost, 0);
                r->inited = 1;
            }
            else
            {
                float dt = s->trailDt;
                if (r->inited)
                {
                    dt = 0.0f;
                    r->inited = 0;
                }
                const float speed = hostGetFloatF0C(s->swiffHost);
                for (int i = 0; i < 40; ++i)
                {
                    if (r->points[i].font)
                    {
                        float m[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, r->points[i].y };
                        // ISwiffRender slot 30, exactly as the shipped rain calls it virtually
                        void** vtbl = s->vtPrimary;
                        typedef void (__thiscall* DrawGlyphFn)(void*, void*, const float*,
                                                               int, int);
                        reinterpret_cast<DrawGlyphFn>(vtbl[30])(s, r->points[i].font, m, -1, 0);
                    }
                    r->points[i].y -= speed * dt;
                }
                if (r->points[0].y < -400.0f)
                {
                    // scroll the window ONE row (the shipped 13 six-dword strides shift the 80
                    // dwords by two) and refill the tail from the round-robin rotation
                    for (int i = 0; i < 39; ++i)
                        r->points[i] = r->points[i + 1];
                    void* font = 0;
                    if (!r->fonts.empty())
                    {
                        const int idx = hostGetIntF14(s->swiffHost);
                        const unsigned int n = (unsigned int)r->fonts.size();
                        font = r->fonts[idx];
                        unsigned int nextIdx = (unsigned int)idx + 1;
                        if (nextIdx >= n)
                            nextIdx = 0;
                        hostSetIntF14(s->swiffHost, (int)nextIdx);
                    }
                    r->points[39].font = font;
                    r->points[39].y    = r->points[38].y + 400.0f;
                }
            }
        }

        device->SetRenderState(D3DRS_SCISSORTESTENABLE, 0);
    }
    ++s->stats[3];      // +1072

    if (s->locked)
    {
        vertexBufferUnlock(s->vb[s->vbIndex]);
        s->locked = 0;
    }
    // sub_100A1720 + sub_100A1500 - the FontRenderer end/reset pair. Withheld (RW-80).

    device->SetViewport(reinterpret_cast<D3DVIEWPORT9*>(s->savedViewport));

    if (s->effect)
    {
        fxSetTexture(effectD3dxHandle(s->effect), paramGetHandle(s->paramTexture), 0);
        fxCommitChanges(effectD3dxHandle(s->effect));
    }
    stateManagerPop();
}

// setTransform: SWF 2x3 -> 4x4, x M616 x M552, latched and pushed to transform.
void RD9_THUNK sr_setTransform(void* self RD9_THUNK_EDX, const float* m2x3)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.nTransforms;
        SST("[swiff]   setTransform [%.4f %.4f %.1f | %.4f %.4f %.1f]",
            m2x3[0], m2x3[1], m2x3[2], m2x3[3], m2x3[4], m2x3[5]);
    }
    float local[16];
    ::memset(local, 0, sizeof(local));
    local[0]  = m2x3[0];
    local[1]  = m2x3[3];
    local[4]  = m2x3[1];
    local[5]  = m2x3[4];
    local[10] = 1.0f;
    local[12] = m2x3[2];
    local[13] = m2x3[5];
    local[15] = 1.0f;

    float step[16], combined[16];
    D3DXMatrixMultiply(reinterpret_cast<D3DXMATRIX*>(step),
                       reinterpret_cast<const D3DXMATRIX*>(local),
                       reinterpret_cast<const D3DXMATRIX*>(s->mtxOrthoTwips));
    D3DXMatrixMultiply(reinterpret_cast<D3DXMATRIX*>(combined),
                       reinterpret_cast<const D3DXMATRIX*>(step),
                       reinterpret_cast<const D3DXMATRIX*>(s->mtxRemap));
    ::memcpy(s->mtxCurrent, combined, sizeof(combined));
    paramSetValue(s->paramTransform, combined, 0);
}

void RD9_THUNK sr_setCxform(void* self RD9_THUNK_EDX, const float* cx)
{
    ::memcpy(static_cast<SwiffRender*>(self)->cxform, cx, 32);
}

int RD9_THUNK sr_recordStat(void* self RD9_THUNK_EDX, int value)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    s->statPrev = s->statCur;
    s->statCur  = value;
    if (value < s->statMin) s->statMin = value;
    if (value >= s->statMax) s->statMax = value;
    return value;
}

// drawVertices: the solid arm accumulates a stitched triangle strip; the textured
// arm routes through the fill collector; an inactive fill style just flushes.
void RD9_THUNK sr_drawVertices(void* self RD9_THUNK_EDX, const short* verts, unsigned int count)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.nStrips;
        g_sst.nVerts += count;
        const int n = (int)count;
        const SwiffStyle* f = &s->style[0];
        SST("[swiff]   drawVertices n=%u mode=%d rgba=%02X%02X%02X%02X tex=%d %dx%d v0=(%d,%d) v1=(%d,%d) "
            "vN=(%d,%d) suppressed=%d",
            count, f->mode, f->rgba[0], f->rgba[1], f->rgba[2], f->rgba[3], f->texId, f->texW, f->texH,
            n > 0 ? verts[0] : 0, n > 0 ? verts[1] : 0, n > 1 ? verts[2] : 0, n > 1 ? verts[3] : 0,
            n > 0 ? verts[2 * n - 2] : 0, n > 0 ? verts[2 * n - 1] : 0,
            (int)(s->deviceLost || !s->swiffHost || hostSuppressed(s)));
    }
    if (s->deviceLost || !s->swiffHost || hostSuppressed(s))
        return;

    SwiffStyle* fill = &s->style[0];
    const bool fillActive = (fill->mode == 1) || fill->texW || fill->texH;
    if (!fillActive)
    {
        flushStrip(s);
        return;
    }

    // sub_100A15A0 - the FontRenderer flush precedes geometry. Withheld (RW-80).

    if (fill->mode == 2 || fill->mode == 3)
    {
        collectFill(s, verts, count);
        ++s->stats[0];              // +1060
        s->lastWasFill = 1;
        return;
    }

    s->lastCount = count;
    s->lastWasFill = 0;
    s->lastVerts = const_cast<short*>(verts);
    ::memcpy(&s->style[4], fill, sizeof(SwiffStyle));      // the line latch at +464

    if (count + s->vbCursor + 2 > 0x2712)
        flushStrip(s);

    ::memcpy(&s->style[3], &s->style[0], sizeof(SwiffStyle));

    const unsigned int color =
        ((unsigned int)fill->rgba[2])
        | ((((((unsigned int)fill->rgba[3] << 8) | fill->rgba[0]) << 8) | fill->rgba[1]) << 8);

    float* v = s->vbBase ? s->vbBase + 4 * s->vbCursor : 0;
    if (!v)
        return;

    double x = 0.0, y = 0.0;
    if (s->vbCursor)
    {
        // Two stitch vertices bridge the previous strip tail to the new head. The z of every
        // strip vertex is left unwritten, exactly as shipped - the SwiffMenu techniques ignore it.
        v[0] = s->lastX;
        v[1] = s->lastY;
        *reinterpret_cast<unsigned int*>(v + 3) = color;
        v += 4;
        x = (double)verts[0] - 0.5;
        y = (double)verts[1] - 0.5;
        v[0] = (float)(y * s->mtxCurrent[4] + x * s->mtxCurrent[0] + s->mtxCurrent[12]);
        v[1] = (float)(y * s->mtxCurrent[5] + x * s->mtxCurrent[1] + s->mtxCurrent[13]);
        *reinterpret_cast<unsigned int*>(v + 3) = color;
        v += 4;
        s->vbCursor += 2;
    }
    s->vbCursor += count;

    for (unsigned int i = 0; i < count; ++i)
    {
        x = (double)verts[2 * i]     - 0.5;
        y = (double)verts[2 * i + 1] - 0.5;
        v[0] = (float)(y * s->mtxCurrent[4] + x * s->mtxCurrent[0] + s->mtxCurrent[12]);
        v[1] = (float)(y * s->mtxCurrent[5] + x * s->mtxCurrent[1] + s->mtxCurrent[13]);
        *reinterpret_cast<unsigned int*>(v + 3) = color;
        v += 4;
    }
    s->lastX = (float)(y * s->mtxCurrent[4] + x * s->mtxCurrent[0] + s->mtxCurrent[12]);
    s->lastY = (float)(y * s->mtxCurrent[5] + x * s->mtxCurrent[1] + s->mtxCurrent[13]);
}

// the line-strip draw: line style + "Line" technique, transformed into the static
// scratch (unk_1025E9E0's role), drawn per pass with DrawPrimitiveUP(linestrip).
void RD9_THUNK sr_drawLineStrip(void* self RD9_THUNK_EDX, const short* verts, unsigned int count)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.nLines;
        const int n = (int)count;
        const SwiffStyle* l = &s->style[2];
        SST("[swiff]   drawLineStrip n=%u mode=%d rgba=%02X%02X%02X%02X width=%.2f v0=(%d,%d) vN=(%d,%d) "
            "suppressed=%d",
            count, l->mode, l->rgba[0], l->rgba[1], l->rgba[2], l->rgba[3], l->width,
            n > 0 ? verts[0] : 0, n > 0 ? verts[1] : 0,
            n > 0 ? verts[2 * n - 2] : 0, n > 0 ? verts[2 * n - 1] : 0,
            (int)(!s->swiffHost || hostSuppressed(s)));
    }
    if (!s->swiffHost || hostSuppressed(s))
        return;

    flushStrip(s);
    applyStyleState(s, &s->style[2]);                        // the line style at +296
    effectSetTechniqueRecord(s->effect, s->techRecord[8]);   // "Line"
    vertexDeclarationBind(s->decl);

    float diffuse[4];
    diffuse[0] = (float)s->style[2].rgba[0] * 0.0039215689f;
    diffuse[1] = (float)s->style[2].rgba[1] * 0.0039215689f;
    diffuse[2] = (float)s->style[2].rgba[2] * 0.0039215689f;
    diffuse[3] = (float)s->style[2].rgba[3] * 0.0039215689f;
    paramSetValue(s->paramDiffuse, diffuse, 0);

    // unk_1025E9E0's stand-in. [CHOSEN] the shipped static's extent is unread; ours is sized to
    // the ring convention (10002 vertices) and the count is clamped to it rather than overrun.
    static float scratch[3 * 0x2712];
    if (count > 0x2712)
        count = 0x2712;
    float* v = scratch;
    for (unsigned int i = 0; i < count; ++i)
    {
        const double x = (double)verts[2 * i]     - 0.5;
        const double y = (double)verts[2 * i + 1] - 0.5;
        v[0] = (float)(y * s->mtxCurrent[4] + x * s->mtxCurrent[0] + s->mtxCurrent[12]);
        v[1] = (float)(y * s->mtxCurrent[5] + x * s->mtxCurrent[1] + s->mtxCurrent[13]);
        v[2] = 0.0f;
        v += 3;
    }

    const unsigned int passes = effectBegin(s->effect);
    for (unsigned int i = 0; i < passes; ++i)
    {
        effectBeginPass(s->effect, i);
        rendererDrawPrimitiveUP(count - 1, scratch, 12, 3 /* D3DPT_LINESTRIP */);
        ++s->stats[4];              // +1076
    }
    effectEnd(s->effect);
}

int RD9_THUNK sr_fillStyleDisable(void* self RD9_THUNK_EDX, int index)
{
    SST("[swiff]   fillStyleDisable side=%d", index);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->style[index].mode = 0;
    return 0;
}

// solid fill: cxform-transform the color, mode 1.
void RD9_THUNK sr_fillStyleColor(void* self RD9_THUNK_EDX, int index, unsigned int rgba)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    const unsigned int transformed =
        applyCxform(s->cxform, reinterpret_cast<const unsigned char*>(&rgba));
    SwiffStyle* style = &s->style[index];
    style->mode = 1;
    *reinterpret_cast<unsigned int*>(style->rgba) = transformed;
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.nFills;
        SST("[swiff]   fillStyleColor side=%d rgba=%08X -> %08X", index, rgba, transformed);
    }
}

// 0x10152D50 / sub_10152130 - textured fill: id, dims, matrix, wrap/clamp, cxform snapshot.
void RD9_THUNK sr_setTexture(void* self RD9_THUNK_EDX, int index, int texId, int w, int h,
                              const float* matrix, int clamp)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SwiffStyle* style = &s->style[index];
    style->texId = texId;
    style->texH  = h;
    style->mode  = (clamp != 0) + 2;
    style->rgba[0] = 0xFF; style->rgba[1] = 0xFF; style->rgba[2] = 0xFF; style->rgba[3] = 0xFF;
    style->texW  = w;
    for (int i = 0; i < 6; ++i)
        style->texMatrix[i] = matrix[i];
    ::memcpy(style->cxform, s->cxform, 32);
    style->hasAdditive =
        (style->cxform[1] > 1.0f || style->cxform[3] > 1.0f
         || style->cxform[5] > 1.0f || style->cxform[7] > 1.0f) ? 1 : 0;
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.nTex;
        SST("[swiff]   setTexture side=%d id=%d %dx%d clamp=%d m=[%.5f %.5f %.1f | %.5f %.5f %.1f]",
            index, texId, w, h, clamp, matrix[0], matrix[1], matrix[2], matrix[3], matrix[4], matrix[5]);
    }
}

void RD9_THUNK sr_lineStyleDisable(void* self RD9_THUNK_EDX)
{
    SST("[swiff]   lineStyleDisable");   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->style[2].mode = 0;
}

// line color, cxform-transformed.
int RD9_THUNK sr_lineStyleColor(void* self RD9_THUNK_EDX, unsigned int rgba)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    const unsigned int transformed =
        applyCxform(s->cxform, reinterpret_cast<const unsigned char*>(&rgba));
    s->style[2].mode = 1;
    *reinterpret_cast<unsigned int*>(s->style[2].rgba) = transformed;
    SST("[swiff]   lineStyleColor rgba=%08X -> %08X", rgba, transformed);   // [TEMP] tracer
    return (int)transformed;
}

// the shipped body clamps the OLD width to >= 1.0 and then overwrites it with the
// raw new dword - the clamp is dead. Reproduced as-is.
void RD9_THUNK sr_lineStyleWidth(void* self RD9_THUNK_EDX, unsigned int widthAsDword)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (s->style[2].width < 1.0f)
        s->style[2].width = 1.0f;
    *reinterpret_cast<unsigned int*>(&s->style[2].width) = widthAsDword;
    SST("[swiff]   lineStyleWidth %.2f", s->style[2].width);   // [TEMP] tracer
}

// No-op slots retain their stack arguments so fastcall adapters perform the required
// thiscall cleanup: five words for drawBitmap, one each for slots 38 and 48.
void RD9_THUNK sr_nullsub25(void* RD9_THUNK_EDX, const void*, int, const void*, const void*, unsigned int) { }
void RD9_THUNK sr_nullsub38(void* RD9_THUNK_EDX, int) { }
void RD9_THUNK sr_nullsub48(void* RD9_THUNK_EDX, int) { }


// The font slots, 26..37. All of them ride the withheld FontRenderer/FontSystem (RW-80): createFont
// resolves the FontSystem singleton (null here) and every forward dereferences the wrapper the
// caller got from it. Null guards + a one-time note stand in for the always-present singletons.

void* RD9_THUNK sr_createFont(void* self RD9_THUNK_EDX, const char* name, int size)
{
    (void)self;
    void* font = createSwiffFont(name, size, 0);
    if (sstEnabled())   // [TEMP] tracer
        ::renddx9abi::logf("[swiff] createFont(\"%s\", %d) -> %p", name ? name : "", size, font);
    if (!font)
        noteOnce(3, "[FRONTIER] SwiffRender::createFont: createSwiffFont returned null - the "
                    "FontSystem singleton is unresolved or the .dif could not be loaded (RW-80); "
                    "this Flash text will not render");
    return font;
}

void RD9_THUNK sr_releaseFontHandle(void* self RD9_THUNK_EDX, void* font)
{
    (void)self;
    if (!font)
        return;
    noteOnce(4, "[FRONTIER] SwiffRender font slots: a non-null font reached a withheld font body "
                "(RW-80)");
}

// The forwarded argument is the text: SwiffPlayer's rcTextSet passes a const wchar_t* and the font
// wrapper's +16 setText takes one, so it stays pointer-sized on x64 (ISwiffRender.h still spells
// it int).
int RD9_THUNK sr_fontForward16(void* self RD9_THUNK_EDX, void* font, const wchar_t* text)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   font.setText font=%p arg=%p", font, text);   // [TEMP] tracer
    if (!font || s->fontsDisabled)
        return (int)(uintptr_t)font;
    void** vtbl = *reinterpret_cast<void***>(font);
    typedef int (__thiscall* Fn)(void*, const wchar_t*, int);
    return reinterpret_cast<Fn>(vtbl[16 / 4])(font, text, 0);
}

int RD9_THUNK sr_destroyFont(void* self RD9_THUNK_EDX, void* font)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (!font || s->fontsDisabled)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    reinterpret_cast<HostVoidFn>(vtbl[8 / 4])(font);   // release
    // sub_101557D0 - erase from the font list; withheld with the wrapper family (RW-80).
    return 0;
}

// draw one glyph/name object: flush, cxform the color, build the pixel-snapped 4x4,
// font vslot +20. The credits rain calls this through the vtable.
void RD9_THUNK sr_drawGlyphBitmap(void* self RD9_THUNK_EDX, void* font, const float* m2x3,
                                   unsigned int rgba, int /*unused*/)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled())   // [TEMP] tracer
    {
        ++g_sst.nGlyphs;
        SST("[swiff]   drawGlyph font=%p m=[%.4f %.4f %.1f | %.4f %.4f %.1f] rgba=%08X",
            font, m2x3[0], m2x3[1], m2x3[2], m2x3[3], m2x3[4], m2x3[5], rgba);
    }
    if (!s->swiffHost || s->fontsDisabled || hostSuppressed(s))
        return;
    if (!font)
        return;

    flushStrip(s);
    ++s->stats[5];      // +1080

    const unsigned int color =
        applyCxform(s->cxform, reinterpret_cast<const unsigned char*>(&rgba));

    float m[16];
    ::memset(m, 0, sizeof(m));
    m[0]  = m2x3[0];
    m[1]  = m2x3[3];
    m[4]  = m2x3[1];
    m[5]  = m2x3[4];
    m[10] = 1.0f;
    m[15] = 1.0f;
    m[12] = (float)(int)(m2x3[2] * 0.050000001f * s->scaleX + 0.5f);   // pixel-snapped
    m[13] = (float)(int)(m2x3[5] * 0.050000001f * s->scaleY + 0.5f);

    void** vtbl = *reinterpret_cast<void***>(font);
    typedef void (__thiscall* DrawFn)(void*, const float*, unsigned int);
    reinterpret_cast<DrawFn>(vtbl[20 / 4])(font, m, color);
}

char RD9_THUNK sr_fontGetExtent(void* self RD9_THUNK_EDX, void* font, int arg,
                                 float* outW, float* outH)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (s->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    typedef char (__thiscall* Fn)(void*, int, float*, float*);
    const char ok = reinterpret_cast<Fn>(vtbl[28 / 4])(font, arg, outW, outH);
    if (ok)
    {
        *outW /= s->scaleX;
        *outH /= s->scaleY;
    }
    return ok;
}

char RD9_THUNK sr_fontForward32(void* self RD9_THUNK_EDX, void* font, int arg,
                                 float x, float y)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (s->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    typedef char (__thiscall* Fn)(void*, int, float, float);
    return reinterpret_cast<Fn>(vtbl[32 / 4])(font, arg, x * s->scaleX, y * s->scaleY);
}

char RD9_THUNK sr_fontForward40(void* self RD9_THUNK_EDX, void* font, int a, int b)
{
    if (static_cast<SwiffRender*>(self)->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    typedef char (__thiscall* Fn)(void*, int, int);
    return reinterpret_cast<Fn>(vtbl[40 / 4])(font, a, b);
}

char RD9_THUNK sr_fontForward44(void* self RD9_THUNK_EDX, void* font,
                                 float x1, float y1, float x2, float y2)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (s->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    typedef char (__thiscall* Fn)(void*, float, float, float, float);
    return reinterpret_cast<Fn>(vtbl[44 / 4])(font, x1 * s->scaleX, y1 * s->scaleY,
                                              x2 * s->scaleX, y2 * s->scaleY);
}

int RD9_THUNK sr_fontForward48(void* self RD9_THUNK_EDX, void* font)
{
    if (static_cast<SwiffRender*>(self)->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    return reinterpret_cast<HostIntFn>(vtbl[48 / 4])(font);
}

int RD9_THUNK sr_fontSetSize(void* self RD9_THUNK_EDX, void* font, float size, int flag)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (s->fontsDisabled || !font)
        return 0;
    // sub_101530D0 writes the alignment flag to font+136 and round(size*scaleX) to font+120, then
    // leaves the offset for setText (sub_100A4F20) to compute. The flag is the paragraph alignment
    // (0 left, 1 right, 2 center). Routed through a typed seam - the raw offsets would hit the
    // wrapper's inline text buffer, which is why menu labels drew but never centered.
    swiffFontConfigure(font, (float)(int)(size * s->scaleX + 0.5f), flag);
    return (int)(uintptr_t)font;
}

int RD9_THUNK sr_fontSetField104(void* self RD9_THUNK_EDX, void* font, int value)
{
    if (static_cast<SwiffRender*>(self)->fontsDisabled || !font)
        return 0;
    // sub_10153120 stores the caret index at font+104, then sub_100A3710 recomputes the visible
    // glyph window (+108 first-visible, +112 count, +116 pixel offset); the draw emits only that
    // window, which is what cuts a text at its box's right edge. Both halves run behind this typed
    // seam (SwiffFont.cpp's setScrollIndex) rather than a raw +104 poke.
    swiffFontSetScrollIndex(font, value);
    return 0;
}

// the Flash clip-mask trio over the stencil buffer.
void RD9_THUNK sr_beginMask(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled()) { ++g_sst.nMasks; SST("[swiff]   beginMask (ref %u)", s->stencilRef); }   // [TEMP] tracer
    if (!s->swiffHost || hostSuppressed(s))
        return;
    flushStrip(s);
    ++s->stats[2];      // +1068
    if (++s->stencilRef == 0)      // the shipped 0xFF wrap: 255 rolls to 1
        s->stencilRef = 1;
    smSetRenderState(52, 1);                 // stencilenable
    smSetRenderState(56, 8);                 // stencilfunc = always
    smSetRenderState(57, s->stencilRef);     // stencilref
    smSetRenderState(55, 3);                 // stencilpass = replace
    smSetRenderState(53, 1);                 // stencilfail = keep
    smSetRenderState(54, 1);                 // stencilzfail = keep
    smSetRenderState(59, 0xFFFFFFFF);        // stencilwritemask
    smSetRenderState(58, 0xFFFFFFFF);        // stencilmask
    smSetRenderState(168, 0);                // colorwriteenable off - the mask is stencil-only
}

void RD9_THUNK sr_endMask(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   endMask (ref %u)", s->stencilRef);   // [TEMP] tracer
    if (!s->swiffHost || hostSuppressed(s))
        return;
    flushStrip(s);
    ++s->stats[2];
    smSetRenderState(168, 15);               // colorwriteenable on
    smSetRenderState(57, s->stencilRef);
    smSetRenderState(56, 3);                 // stencilfunc = equal - draw inside the mask
    smSetRenderState(55, 1);                 // keep
    smSetRenderState(53, 1);
    smSetRenderState(54, 1);
}

void RD9_THUNK sr_disableMask(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   disableMask");   // [TEMP] tracer
    if (!s->swiffHost || hostSuppressed(s))
        return;
    flushStrip(s);
    ++s->stats[2];
    smSetRenderState(52, 0);
}

void RD9_THUNK sr_setMovieWidth(void* self RD9_THUNK_EDX, unsigned int w)
{
    if (sstEnabled()) ::renddx9abi::logf("[swiff] setMovieWidth(%u)", w);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->movieWidth = w;
}
void RD9_THUNK sr_setMovieHeight(void* self RD9_THUNK_EDX, int h)
{
    if (sstEnabled()) ::renddx9abi::logf("[swiff] setMovieHeight(%d)", h);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->movieHeight = h;
}
void RD9_THUNK sr_setScale(void* self RD9_THUNK_EDX, float sx, float sy)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled()) ::renddx9abi::logf("[swiff] setScale(%.4f, %.4f)", sx, sy);   // [TEMP] tracer
    s->scaleX = sx;
    s->scaleY = sy;
}
void RD9_THUNK sr_setBackgroundColor(void* self RD9_THUNK_EDX, unsigned int rgb)
{
    SST("[swiff]   setBackgroundColor %08X", rgb);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->bgColor = rgb;
}
void RD9_THUNK sr_setMouseState(void* self RD9_THUNK_EDX, int x, int y, int buttons)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   setMouseState (%d,%d,%d)", x, y, buttons);   // [TEMP] tracer
    s->mouseX = x;
    s->mouseY = y;
    s->mouseButtons = buttons;
}

int RD9_THUNK sr_slot47(void* RD9_THUNK_EDX, int) { return -1; }
char RD9_THUNK sr_slot49(void* RD9_THUNK_EDX, intptr_t) { return 1; }             // (a no-op preload)
char RD9_THUNK sr_slot50(void* RD9_THUNK_EDX, intptr_t) { return 1; }

// beginFrame: the background clear color (when the host says the movie has one), the
// per-frame stats reset, and the renderer's beginScene. This is the call BF2 crashed reaching.
int RD9_THUNK sr_beginFrame(void* self RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);

    unsigned int rgb[3] = { 0, 0, 0 };
    const unsigned char hasBg =
        s->swiffHost ? hostGetFlagE81(s->swiffHost) : 0;
    if (sstEnabled() && g_sst.displays < g_sst.framesToLog)   // [TEMP] tracer
        ::renddx9abi::logf("[swiff] beginFrame hasBg=%d bg=%08X", (int)hasBg, s->bgColor);
    if (hasBg)
    {
        const unsigned char* c = reinterpret_cast<const unsigned char*>(&s->bgColor);
        rgb[0] = c[0];
        rgb[1] = c[1];
        rgb[2] = c[2];
    }
    for (int i = 0; i < 6; ++i)
        s->stats[i] = 0;
    const bool ok = rendererBeginScene(rgb, true);
    return ok ? 1 : 0;
}

// endFrame: endScene + Present. The menu's actual present call.
int RD9_THUNK sr_endFrame(void* self RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (!s->deviceLost)
        rendererEndScene();
    const bool presented = rendererPresent(2);
    return presented ? 1 : 0;
}

// assign a per-texture technique index (into techRecord[]) by texture name. The answer is the
// mapped value's address (the image hands back its tree node; no caller reads either).
void* RD9_THUNK sr_setTextureTechnique(void* self RD9_THUNK_EDX, const std::string* name, int index)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    int& slot = s->texTechniqueMap[*name];
    slot = index;
    return &slot;
}

int RD9_THUNK sr_updateTime(void* self RD9_THUNK_EDX, unsigned int t, float dt)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   updateTime t=%u dt=%.5f", t, dt);   // [TEMP] tracer
    s->timeMs = t;
    s->trailDt = dt;
    if (s->paramTime)
    {
        const float asFloat = (float)t;     // the shipped pushes the raw dword; time is a float
        (void)asFloat;
        paramSetValue(s->paramTime, &t, 0);
    }
    return (int)t;
}

void* RD9_THUNK sr_getTextureRegistry(void* self RD9_THUNK_EDX)
{
    return &static_cast<SwiffRender*>(self)->registry;
}

// uploadTexture: the loader with the cache skipped. Note the string-pointer argument.
int RD9_THUNK sr_uploadTexture(void* self RD9_THUNK_EDX, const std::string* name,
                                unsigned int* outW, unsigned int* outH)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    return registryGetOrLoad(s->registry, *name, outW, outH, 1);
}

// Copies the device driver version; returns zeros when caps are absent.
unsigned int* RD9_THUNK sr_getDriverVersion(void* RD9_THUNK_EDX, unsigned int* out)
{
    void* caps = ::dice::hfe::dx9rend::g_deviceCaps;
    if (caps)
    {
        const unsigned char* c = static_cast<const unsigned char*>(caps);
        out[0] = *reinterpret_cast<const unsigned int*>(c + 1360);
        out[1] = *reinterpret_cast<const unsigned int*>(c + 1364);
    }
    else
    {
        out[0] = 0;     // [CHOSEN] zeroed where the shipped body copies stack garbage
        out[1] = 0;
    }
    return out;
}

char RD9_THUNK sr_isNvidia(void* RD9_THUNK_EDX)      // adapter.VendorId == 0x10DE (4318)
{
    void* caps = ::dice::hfe::dx9rend::g_deviceCaps;
    if (!caps)
        return 0;
    const unsigned char* c = static_cast<const unsigned char*>(caps);
    return *reinterpret_cast<const unsigned int*>(c + 1368) == 4318u ? 1 : 0;
}

unsigned char g_swiffFlag1023D950 = 0;         // byte_1023D950 - a host-toggled flag pair
char RD9_THUNK sr_getFlag(void* RD9_THUNK_EDX) { return (char)g_swiffFlag1023D950; }
char RD9_THUNK sr_setFlag(void* RD9_THUNK_EDX, char v) { g_swiffFlag1023D950 = (unsigned char)v; return v; }

int RD9_THUNK sr_creditsCount(void* self RD9_THUNK_EDX);     // slot 62, forward-declared for 61

// preload images/credits/1..113.png and remember each name that resolved.
char RD9_THUNK sr_preloadCredits(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sr_creditsCount(self RD9_THUNK_ARG))
        return 1;

    for (unsigned int i = 1; i <= 0x71; ++i)
    {
        char nameBuf[64];
        ::sprintf(nameBuf, "images/credits/%d.png", i);
        std::string name(nameBuf);
        unsigned int w = 0, h = 0;
        if (registryGetOrLoad(s->registry, name, &w, &h, 0) != -1)
        {
            // sub_10011220 - push the name onto the credits vector at +1104.
            s->credits.push_back(name);
        }
    }
    return 1;
}

int RD9_THUNK sr_creditsCount(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    return (int)s->credits.size();
}

// copy the i-th credits name out; empty string when out of range.
std::string* RD9_THUNK sr_getCreditsName(void* self RD9_THUNK_EDX, std::string* out, unsigned int index)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    const int count = sr_creditsCount(self RD9_THUNK_ARG);
    if (count && index < (unsigned int)count)
        new (out) std::string(s->credits[index]);
    else
        new (out) std::string("");
    return out;
}

// byte_10267D94 - shared between the seeder and the range picker, as shipped.
bool g_swiffRandSeeded = false;

void swiffSeedRandom()      // sub_101C8240 - init()'s tail
{
    ::srand((unsigned int)::time(0));
    g_swiffRandSeeded = true;
}

// sub_101C8310 - the srand(time)-seeded uniform pick, exactly the shipped arithmetic.
int swiffRandomRange(int lo, int hi)
{
    if (!g_swiffRandSeeded)
        swiffSeedRandom();
    const float range = (float)(hi - lo);
    return lo - (int)((double)::rand() * range * -0.000030518509);
}

std::string* RD9_THUNK sr_getRandomCreditsName(void* self RD9_THUNK_EDX, std::string* out)
{
    const int count = sr_creditsCount(self RD9_THUNK_ARG);
    const int pick  = swiffRandomRange(0, count - 1);
    // slot 63, virtually - as the shipped body dispatches through its own vtable
    void** vtbl = static_cast<SwiffRender*>(self)->vtPrimary;
    typedef std::string* (__thiscall* GetNameFn)(void*, std::string*, unsigned int);
    return reinterpret_cast<GetNameFn>(vtbl[63])(self, out, (unsigned int)pick);
}

char RD9_THUNK sr_getWasDeviceLost(void* self RD9_THUNK_EDX)
{
    return (char)static_cast<SwiffRender*>(self)->wasLostSticky;
}
void RD9_THUNK sr_clearWasDeviceLost(void* self RD9_THUNK_EDX)
{
    static_cast<SwiffRender*>(self)->wasLostSticky = 0;
}


// init - 0x10152200. Singletons, the effect + six parameters, eight technique records, the vertex
// declaration, three ring VBs under a pool swap, the listener/event registrations, the rain.

int RD9_THUNK sr_init(void* self RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);

    s->fontRenderer = 0;
    s->swiffHost    = 0;
    if (::dice::hfe::classManager)
    {
        s->fontRenderer = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("FontRenderer"));
        s->swiffHost    = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    }
    if (!s->fontRenderer)
        noteOnce(5, "[FRONTIER] SwiffRender::init: the FontRenderer singleton is null (class "
                    "81047 withheld, RW-80) - every font path is disabled with a guard");

    s->effect = shaderManagerGetEffect(g_shaderManager, "Shaders/SwiffMenu");

    Effect* fx = static_cast<Effect*>(s->effect);
    s->paramTransform = effectGetShaderParameter(fx, std::string("TRANSFORM"));
    s->paramDiffuse   = effectGetShaderParameter(fx, std::string("DIFFUSE"));
    s->paramTexGenS   = effectGetShaderParameter(fx, std::string("TEXGENS"));
    s->paramTexGenT   = effectGetShaderParameter(fx, std::string("TEXGENT"));
    s->paramTexture   = effectGetShaderParameter(fx, std::string("TEXTURE"));
    s->paramTime      = effectGetShaderParameter(fx, std::string("TIME"));

    s->techRecord[0] = 0;
    s->techRecord[1] = effectFindTechniqueRecord(s->effect, "TS0");
    s->techRecord[2] = effectFindTechniqueRecord(s->effect, "TS1");
    s->techRecord[3] = effectFindTechniqueRecord(s->effect, "TS2");
    s->techRecord[4] = effectFindTechniqueRecord(s->effect, "TS3");
    s->techRecord[5] = effectFindTechniqueRecord(s->effect, "Shape");
    s->techRecord[6] = effectFindTechniqueRecord(s->effect, "ShapeTextureWrap");
    s->techRecord[7] = effectFindTechniqueRecord(s->effect, "ShapeTextureClamp");
    s->techRecord[8] = effectFindTechniqueRecord(s->effect, "Line");

    s->decl = vertexDeclarationCreate(kSwiffDeclElements);

    const int previousPool = bufferManagerSwapPool(0);
    for (int i = 0; i < 3; ++i)
        s->vb[i] = bufferManagerCreateVertexBuffer(10002, 16, 0x208, "SWIFF_VBS");
    s->vbBase = 0;
    bufferManagerSwapPool(previousPool);

    // Slot 49 with "default" - the shipped no-op preload, dispatched virtually as shipped.
    {
        void** vtbl = s->vtPrimary;
        typedef char (__thiscall* PreloadFn)(void*, const char*);
        reinterpret_cast<PreloadFn>(vtbl[49])(s, "default");
    }

    // sub_100A1240 - FontRenderer attach (its effect, decls, FONT_VBS ring). Withheld (RW-80).

    // RendDX9_registerDeviceLostResetListener(this+4, priority 4) - 0x10152565..0x10152571:
    // `push 4; lea ecx,[esi+4]; push ecx; mov ecx, dword_1023B970; call 0x10009D60`, straight after
    // the FontRenderer attach and before the ECCore (11) handler. No test of the base global.
    registerDeviceLostResetListener(&s->vtDevLost, 4);

    if (::dice::hfe::eventManager)
    {
        ::dice::hfe::eventManager->registerEventHandler(
            static_cast<EventCategory>(11), reinterpret_cast<IEventListener*>(&s->vtEcCore), 0);
    }

    s->timeMs = 0;
    s->fontsDisabled = 0;

    void* rainStorage = ::renddx9abi::engineAlloc(sizeof(CreditsRain));
    CreditsRain* rain = 0;
    if (rainStorage)
    {
        // sub_101B9200 - zero the glyph slots (the y slots stay uninitialised, as shipped), the
        // vector (constructed empty), and the inited byte.
        rain = new (rainStorage) CreditsRain;
        for (int i = 0; i < 40; ++i)
            rain->points[i].font = 0;
        rain->inited = 0;
    }
    s->trail = rain;

    // sub_101B9290 - build one font per SwiffHost credits-list entry. The list is empty at menu
    // boot; entries appear when the credits screen opens. Fonts are withheld (RW-80), so entries
    // whose font came back null are skipped where the shipped build dereferences.
    if (rain && s->swiffHost)
    {
        // ISwiffHost +0x2DC getEntriesEC8 -> vector<SwiffStringIntEntry>*. RW-266: the
        // walk below reads the retail VC7.1 layout - first at +4, last at +8, 32-byte entries
        // {string (28), int} with the int at +28. The native host hands back its own std::vector,
        // {first, last, end} at +0/+4/+8 with 28-byte entries {std::string (24), int}, which that
        // walk would have read as the wrong range at the wrong stride. So the native flavour walks
        // it typed; the per-entry body is shared, in the same order.
        std::vector<::dice::hfe::SwiffStringIntEntry>* const list = swiffHostOf(s->swiffHost)->getEntriesEC8();
        if (list)
        {
            for (std::vector<::dice::hfe::SwiffStringIntEntry>::const_iterator it = list->begin();
                 it != list->end(); ++it)
            {
                const int type = it->value;
                void** vtbl = s->vtPrimary;
                typedef void* (__thiscall* CreateFontFn)(void*, const char*, int);
                void* font = 0;
                if (type == 1)
                    font = reinterpret_cast<CreateFontFn>(vtbl[26])(s, "StandardTextBold", 13);
                else if (type == 2)
                    font = reinterpret_cast<CreateFontFn>(vtbl[26])(s, "StandardTextBold", 15);
                else
                    font = reinterpret_cast<CreateFontFn>(vtbl[26])(s, "DynamicText", 11);
                if (!font)
                    continue;       // withheld fonts (RW-80); the shipped dereferences here
                // the size/flag fields, the localized setText, the release, and the push - all in
                // the shipped order once fonts exist. Unreachable until RW-80 retires.
                creditsRainPush(rain, font);
            }
        }
    }

    if (s->swiffHost)
    {
        hostSetFlagF18(s->swiffHost, 1);
        hostSetFlagF10(s->swiffHost, 0);
    }

    swiffSeedRandom();
    return 1;
}


// shutdown - 0x10153FE0. The rain, the event handler, the paired releases, the fonts vector, the
// VB ring + declaration, the six parameters, the effect, the FontRenderer detach.

int RD9_THUNK sr_shutdown(void* self RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);

    if (s->trail)
    {
        // sub_101B8F00 - release every rain font through slot 27 (withheld font bodies no-op on
        // null), then sub_101B9240 - the vector teardown - and the free.
        CreditsRain* r = s->trail;
        for (std::vector<void*>::iterator f = r->fonts.begin(); f != r->fonts.end(); ++f)
        {
            void** vtbl = s->vtPrimary;
            typedef void (__thiscall* ReleaseFontFn)(void*, void*);
            if (*f)
                reinterpret_cast<ReleaseFontFn>(vtbl[27])(s, *f);
        }
        r->~CreditsRain();
        ::renddx9abi::engineFree(r);
        s->trail = 0;
    }

    // sub_10009D70 - unregister the device-lost listener, 0x1015401C..0x10154026: `mov ecx,
    // dword_1023B970; lea eax,[esi+4]; push eax; call 0x10009D70`, after the rain and before the
    // ECCore handler - so no lost/reset walk can reach the ring buffers released below.
    // Unguarded, as shipped.
    unregisterDeviceLostResetListener(&s->vtDevLost);

    if (::dice::hfe::eventManager)
    {
        ::dice::hfe::eventManager->unregisterEventHandler(
            static_cast<EventCategory>(11), reinterpret_cast<IEventListener*>(&s->vtEcCore));
    }

    for (int i = 0; i < 2; ++i)
    {
        void* obj = i == 0 ? s->released968 : s->released972;
        if (obj)
        {
            void** vtbl = *reinterpret_cast<void***>(obj);
            reinterpret_cast<HostVoidFn>(vtbl[8 / 4])(obj);
        }
    }
    s->released968 = 0;
    s->released972 = 0;

    // The fonts vector at +956: release each {name, object} entry. Nothing in this build can
    // populate it until RW-80 retires; the walk is the shipped one.
    for (std::vector<SwiffFontEntry*>::iterator e = s->fonts.begin(); e != s->fonts.end(); ++e)
    {
        if (*e)
        {
            SwiffFontEntry* const entry = *e;
            void* obj = entry->object;
            if (obj)
            {
                void** vtbl = *reinterpret_cast<void***>(obj);
                reinterpret_cast<HostVoidFn>(vtbl[8 / 4])(obj);
            }
            entry->~SwiffFontEntry();
            ::renddx9abi::engineFree(entry);
            *e = 0;
        }
    }
    s->fonts.clear();

    if (s->object5C)
    {
        void** vtbl = *reinterpret_cast<void***>(s->object5C);
        reinterpret_cast<HostVoidFn>(vtbl[8 / 4])(s->object5C);
    }

    if (s->locked)
    {
        vertexBufferUnlock(s->vb[s->vbIndex]);
        s->locked = 0;
    }
    for (int i = 0; i < 3; ++i)
    {
        if (s->vb[i])
            vertexBufferRelease(s->vb[i]);
        s->vb[i] = 0;
    }
    if (s->decl)
    {
        vertexDeclarationRelease(s->decl);
        s->decl = 0;
    }

    paramRelease(s->paramTransform);
    paramRelease(s->paramDiffuse);
    paramRelease(s->paramTexGenS);
    paramRelease(s->paramTexGenT);
    paramRelease(s->paramTexture);
    paramRelease(s->paramTime);
    s->paramTransform = 0; s->paramDiffuse = 0; s->paramTexGenS = 0;
    s->paramTexGenT = 0; s->paramTexture = 0; s->paramTime = 0;

    if (s->effect)
    {
        void** vtbl = *reinterpret_cast<void***>(s->effect);
        reinterpret_cast<HostVoidFn>(vtbl[8 / 4])(s->effect);   // release; the cache keeps its own
    }
    s->effect = 0;

    // sub_100A18A0 - the FontRenderer detach. Withheld with the class (RW-80).
    return 0;
}


// The device-lost listener (secondary vtable at +4) and the ECCore handler (at +8). The bodies
// receive the subobject pointer, exactly as the shipped ones do.

SwiffRender* ownerFromDevLost(void* sub)
{
    return reinterpret_cast<SwiffRender*>(
        static_cast<unsigned char*>(sub) - offsetof(SwiffRender, vtDevLost));    // +4
}
SwiffRender* ownerFromEcCore(void* sub)
{
    return reinterpret_cast<SwiffRender*>(
        static_cast<unsigned char*>(sub) - offsetof(SwiffRender, vtEcCore));     // +8
}

// Marks device loss, stops host rendering, and unlocks the ring buffer before the lower
// priority BufferManager releases it. Initialization must have supplied a valid host.
void RD9_THUNK sr_onDeviceLost(void* sub RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = ownerFromDevLost(sub);
    s->deviceLost = 1;
    s->wasLostSticky = 1;
    hostSetFlagE81(s->swiffHost, 0);                                     // +0x27C setFlagE81(false)
    if (s->locked)
    {
        vertexBufferUnlock(s->vb[s->vbIndex]);
        s->locked = 0;
    }
}

// Rebuilds cached techniques and resets menu modifier state after device reset.
// The restart flag gates host flag updates and is re-read before requesting a menu restart.
void RD9_THUNK sr_onDeviceReset(void* sub RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = ownerFromDevLost(sub);
    const bool hadEffect = s->effect != 0;
    s->vbBase = 0;
    s->deviceLost = 0;
    if (hadEffect)
    {
        s->techRecord[0] = 0;
        s->techRecord[1] = effectFindTechniqueRecord(s->effect, "TS0");
        s->techRecord[2] = effectFindTechniqueRecord(s->effect, "TS1");
        s->techRecord[3] = effectFindTechniqueRecord(s->effect, "TS2");
        s->techRecord[4] = effectFindTechniqueRecord(s->effect, "TS3");
        s->techRecord[5] = effectFindTechniqueRecord(s->effect, "Shape");
        s->techRecord[6] = effectFindTechniqueRecord(s->effect, "ShapeTextureWrap");
        s->techRecord[7] = effectFindTechniqueRecord(s->effect, "ShapeTextureClamp");
        s->techRecord[8] = effectFindTechniqueRecord(s->effect, "Line");
    }
    if (!s->fontsDisabled)
    {
        hostSetFlagE81(s->swiffHost, 1);                                 // +0x27C setFlagE81(true)
        hostSetFlagE80(s->swiffHost, 0);                                 // +0x274 setFlagE80(false)
    }
    if (s->fontsDisabled)
        hostRequestRestartMenu(s->swiffHost);                            // +0x264 requestRestartMenu
    void* player =
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffPlayer"));
    if (player)
    {
        // RW-266: typed - ISwiffPlayer slot 18 is +0x48.
        static_cast<ui::ISwiffPlayer*>(static_cast<IBase*>(player))->resetModifierState();
    }
}

void RD9_THUNK sr_onDeviceDestroy(void* RD9_THUNK_EDX) { }       // 0x10152870, SwiffRender's own `ret` (not the shared 0x10037D40)

// the ECCore handler: event (11,3) disables font rendering. The two unread arguments are
// IEventListener::handleEvent's node and data pointers.
void RD9_THUNK sr_handleECCoreEvent(void* sub RD9_THUNK_EDX, int category, int id, void*, void*)
{
    SwiffRender* s = ownerFromEcCore(sub);
    if (category == 11 && id == 3)
        s->fontsDisabled = 1;
}


// The vtables - primary 0x1021E1C0 (67), devlost 0x1021E1B4 (3), eccore 0x1021E1B0 (1).

void* const kSwiffRenderVtablePrimary[67] =
{
    reinterpret_cast<void*>(&sr_addRef),               //  0 +0x00
    reinterpret_cast<void*>(&sr_getRef),               //  1 +0x04
    reinterpret_cast<void*>(&sr_release),              //  2 +0x08
    reinterpret_cast<void*>(&sr_queryInterfaceById),   //  3 +0x0C
    reinterpret_cast<void*>(&sr_init),                 //  4 +0x10
    reinterpret_cast<void*>(&sr_shutdown),             //  5 +0x14
    reinterpret_cast<void*>(&sr_slot6),                //  6 +0x18
    reinterpret_cast<void*>(&sr_uploadAlpha),          //  7 +0x1C
    reinterpret_cast<void*>(&sr_slot8),                //  8 +0x20
    reinterpret_cast<void*>(&sr_uploadRgba),           //  9 +0x24
    reinterpret_cast<void*>(&sr_loadTexture),          // 10 +0x28
    reinterpret_cast<void*>(&sr_releaseTexture),       // 11 +0x2C
    reinterpret_cast<void*>(&sr_beginDisplay),         // 12 +0x30
    reinterpret_cast<void*>(&sr_endDisplay),           // 13 +0x34
    reinterpret_cast<void*>(&sr_setTransform),         // 14 +0x38
    reinterpret_cast<void*>(&sr_setCxform),            // 15 +0x3C
    reinterpret_cast<void*>(&sr_recordStat),           // 16 +0x40
    reinterpret_cast<void*>(&sr_drawVertices),         // 17 +0x44
    reinterpret_cast<void*>(&sr_drawLineStrip),        // 18 +0x48
    reinterpret_cast<void*>(&sr_fillStyleDisable),     // 19 +0x4C
    reinterpret_cast<void*>(&sr_fillStyleColor),       // 20 +0x50
    reinterpret_cast<void*>(&sr_setTexture),           // 21 +0x54
    reinterpret_cast<void*>(&sr_lineStyleDisable),     // 22 +0x58
    reinterpret_cast<void*>(&sr_lineStyleColor),       // 23 +0x5C
    reinterpret_cast<void*>(&sr_lineStyleWidth),       // 24 +0x60
    reinterpret_cast<void*>(&sr_nullsub25),            // 25 +0x64  retn 14h
    reinterpret_cast<void*>(&sr_createFont),           // 26 +0x68
    reinterpret_cast<void*>(&sr_releaseFontHandle),    // 27 +0x6C
    reinterpret_cast<void*>(&sr_fontForward16),        // 28 +0x70
    reinterpret_cast<void*>(&sr_destroyFont),          // 29 +0x74
    reinterpret_cast<void*>(&sr_drawGlyphBitmap),      // 30 +0x78
    reinterpret_cast<void*>(&sr_fontGetExtent),        // 31 +0x7C
    reinterpret_cast<void*>(&sr_fontForward32),        // 32 +0x80
    reinterpret_cast<void*>(&sr_fontForward40),        // 33 +0x84
    reinterpret_cast<void*>(&sr_fontForward44),        // 34 +0x88
    reinterpret_cast<void*>(&sr_fontForward48),        // 35 +0x8C
    reinterpret_cast<void*>(&sr_fontSetSize),          // 36 +0x90
    reinterpret_cast<void*>(&sr_fontSetField104),      // 37 +0x94
    reinterpret_cast<void*>(&sr_nullsub38),            // 38 +0x98  retn 4
    reinterpret_cast<void*>(&sr_beginMask),            // 39 +0x9C
    reinterpret_cast<void*>(&sr_endMask),              // 40 +0xA0
    reinterpret_cast<void*>(&sr_disableMask),          // 41 +0xA4
    reinterpret_cast<void*>(&sr_setMovieWidth),        // 42 +0xA8
    reinterpret_cast<void*>(&sr_setMovieHeight),       // 43 +0xAC
    reinterpret_cast<void*>(&sr_setScale),             // 44 +0xB0
    reinterpret_cast<void*>(&sr_setBackgroundColor),   // 45 +0xB4
    reinterpret_cast<void*>(&sr_setMouseState),        // 46 +0xB8
    reinterpret_cast<void*>(&sr_slot47),               // 47 +0xBC
    reinterpret_cast<void*>(&sr_nullsub48),            // 48 +0xC0  retn 4
    reinterpret_cast<void*>(&sr_slot49),               // 49 +0xC4
    reinterpret_cast<void*>(&sr_slot50),               // 50 +0xC8
    reinterpret_cast<void*>(&sr_beginFrame),           // 51 +0xCC  <- BF2's 0x4A2C26 call
    reinterpret_cast<void*>(&sr_endFrame),             // 52 +0xD0
    reinterpret_cast<void*>(&sr_setTextureTechnique),  // 53 +0xD4
    reinterpret_cast<void*>(&sr_updateTime),           // 54 +0xD8
    reinterpret_cast<void*>(&sr_getTextureRegistry),   // 55 +0xDC
    reinterpret_cast<void*>(&sr_uploadTexture),        // 56 +0xE0
    reinterpret_cast<void*>(&sr_getDriverVersion),     // 57 +0xE4
    reinterpret_cast<void*>(&sr_isNvidia),             // 58 +0xE8
    reinterpret_cast<void*>(&sr_getFlag),              // 59 +0xEC
    reinterpret_cast<void*>(&sr_setFlag),              // 60 +0xF0
    reinterpret_cast<void*>(&sr_preloadCredits),       // 61 +0xF4
    reinterpret_cast<void*>(&sr_creditsCount),         // 62 +0xF8
    reinterpret_cast<void*>(&sr_getCreditsName),       // 63 +0xFC
    reinterpret_cast<void*>(&sr_getRandomCreditsName), // 64 +0x100
    reinterpret_cast<void*>(&sr_getWasDeviceLost),     // 65 +0x104
    reinterpret_cast<void*>(&sr_clearWasDeviceLost),   // 66 +0x108
};

void* const kSwiffRenderVtableDevLost[3] =
{
    reinterpret_cast<void*>(&sr_onDeviceLost),         // +0x00
    reinterpret_cast<void*>(&sr_onDeviceReset),        // +0x04
    reinterpret_cast<void*>(&sr_onDeviceDestroy),      // +0x08  (its own `ret`)
};

void* const kSwiffRenderVtableEcCore[1] =
{
    reinterpret_cast<void*>(&sr_handleECCoreEvent),    // +0x00
};


// The constructor - 0x10155060. Reproduces exactly the shipped stores; fields it leaves alone stay
// whatever the allocator handed over, as shipped. The containers (the registry's vector,
// sub_101B9AD0, the two maps and the three other vectors) are constructed empty first.

SwiffRender* swiffRenderConstruct(void* storage)
{
    SwiffRender* s = new (storage) SwiffRender;
    s->vtPrimary = const_cast<void**>(kSwiffRenderVtablePrimary);
    s->vtDevLost = const_cast<void**>(kSwiffRenderVtableDevLost);
    s->vtEcCore  = const_cast<void**>(kSwiffRenderVtableEcCore);
    s->refCount  = 1;
    s->deviceLost = 0;
    s->wasLostSticky = 0;
    s->movieWidth = 0;
    s->movieHeight = 0;
    s->mouseX = 0; s->mouseY = 0; s->mouseButtons = 0;
    s->decl = 0;
    s->vbBase = 0;
    s->vbCursor = 0;
    s->vbIndex = 0;
    s->vb[0] = 0; s->vb[1] = 0; s->vb[2] = 0;
    s->lastX = 0.0f; s->lastY = 0.0f;
    s->u88 = 0;
    s->object5C = 0;
    s->currentTexture = 0;
    s->effect = 0;
    s->paramTransform = 0; s->paramDiffuse = 0; s->paramTexGenS = 0;
    s->paramTexGenT = 0; s->paramTexture = 0; s->paramTime = 0;
    s->scaleX = 1.0f;
    s->scaleY = 1.0f;

    for (int i = 0; i < 5; ++i)
    {
        SwiffStyle* style = &s->style[i];
        style->mode  = 0;
        style->texId = -1;
        style->texW  = 0;
        style->texH  = 0;
        style->hasAdditive = 0;
        style->width = 1.0f;
    }
    s->lastWasFill = 0;

    s->u856 = -1;
    s->locked = 0;
    s->statMax = -1;
    s->statMin = 1000;
    s->stencilRef = 1;
    s->released968 = 0;
    s->released972 = 0;
    s->u976 = 0;
    s->u980 = 0;
    s->techRecord[0] = 0;
    s->fontsDisabled = 0;
    s->b1057 = 0;
    s->swiffHost = 0;
    s->trail = 0;
    s->vpW = 0; s->vpH = 0;
    s->u1052 = -100;                           // *(this + 263) = -100, the one odd ctor store
    s->fontRenderer = 0;
    s->trailDt = 0.0f;
    s->timeMs = 0;                             // [CHOSEN] zeroed here; the shipped leaves it to
                                               // init(). The +904 vector, which the shipped ctor
                                               // leaves as allocator garbage and the dtor then
                                               // walks, is constructed empty instead.
    s->stats[0] = 0; s->stats[1] = 0; s->stats[2] = 0;
    s->stats[3] = 0; s->stats[4] = 0; s->stats[5] = 0;
    s->lastVerts = 0;
    s->lastCount = 0;
    return s;
}

// The destructor tail - 0x10154F60 (after shutdown): the credits vectors, the two maps, the fonts
// vector remnants, the registry - which is the members' reverse declaration order, so the closing
// destructor call releases the emptied containers in that same order.
void swiffRenderDestroy(SwiffRender* s)
{
    s->vtPrimary = const_cast<void**>(kSwiffRenderVtablePrimary);
    s->vtDevLost = const_cast<void**>(kSwiffRenderVtableDevLost);
    s->vtEcCore  = const_cast<void**>(kSwiffRenderVtableEcCore);

    sr_shutdown(s RD9_THUNK_ARG);

    // sub_100111E0 - the credits names at +1104.
    s->credits.clear();

    // The two maps, in the shipped order: the font set (+1040, sub_10155890) first, then the
    // technique map (+984, sub_10013E50).
    s->fontSet.clear();                    // empty in this build (fonts withheld, RW-80)
    s->texTechniqueMap.clear();

    s->fonts.clear();

    // The +904 string vector (no recovered writer; the dtor destroys it as shipped).
    s->strings904.clear();

    registryDestroy(s->registry);

    s->~SwiffRender();
}

} // anonymous namespace

// RefFactory_geom_SwiffRender, alloc 0x460, ctor, queryInterface against the two ids.
IBase* __fastcall createSwiffRender(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;
    void* storage = ::renddx9abi::engineAlloc(sizeof(SwiffRender));   // 0x460 in the image
    if (!storage)
        return 0;
    SwiffRender* self = swiffRenderConstruct(storage);
    void* itf = sr_queryInterfaceById(self RD9_THUNK_ARG, iid);
    if (!itf)
        sr_release(self RD9_THUNK_ARG);
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
