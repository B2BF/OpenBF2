// Flash rasterizer for menu frames, textures, masks, glyphs and credits.
// [INFERRED] source placement. ModuleGeom publishes the singleton on core event (10,1)
// and removes it on (10,2); beginFrame/endFrame bracket the menu's presentation.
// Text goes through the FontRenderer batch (GeomMisc/FontSystem/SwiffFont.cpp), flushed at the
// shipped sites. The font wrapper behind the font slots is still a chosen layout. [RETIRE-WHEN: RW-80]
// Missing ServerLogoManager fails texture loading. VideoTarget owns movie texture reset handling.

#ifdef BF2_IMAGE_RENDDX9
#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Compiler-typed native table entries.
#include "../../RendDX9Abi/NativeVtable.h"
#include "../Menu/ISwiffRender.h"
#endif

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/ModuleCommon.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "../Core/EventManager.h"
#include "../Core/SettingsRepostitory.h"
#include "../Core/StringUtil.h"
#include "../../RendDX9Abi/PluginEntry.h"
#include "../../RendDX9Abi/EngineFile.h"
#include "../RendDX9/RendDX9Base.h"
#include "../RendMisc/IRenderer.h"
#include "../Menu/ISwiffRender.h"
#include "../RendDX9/ShaderParameter.h"
#include "../../RendDX9Abi/D3dx9Sdk.h"
// RW-266: the host interfaces this file dispatches into, typed in this flavour - the
// SwiffHost, the Swiff player and the server-logo manager (see hostSlot's wrappers below).
#include "../Menu/ISwiffHost.h"
#include "../Menu/MenuViewport.h"
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
#include "../Platform/MsvcRand.h"   // [CHOSEN, not recovered] MSVCR71 rand/srand (RAND_MAX 0x7FFF)
#if !defined(_MSC_VER)
#include "../Platform/X87Compat.h"
#endif

namespace dice {
namespace hfe {

namespace dx9rend {
// The seams this class rides on - all in the RendDX9 TUs.
extern void* g_shaderManager;                                       // dword_1023B97C
extern void* g_stateManager;                                        // dword_1023B984
extern void* g_deviceCaps;                                          // dword_1023B980
namespace Ra { extern unsigned char g_anyEffectCompiled; }          // byte_1023D950
extern void* g_rendererBase;
unsigned int* deviceCapsCopyDriverVersion(unsigned int* out);
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
void swiffFontRefreshWindow(void* font);
void swiffFontDelete(void* font);               // the wrapper's destructor + engine free

// The FontRenderer operations this file calls on the +1036 singleton (SwiffFont.cpp): init
// 0x100A1240, begin 0x100A1470, flush 0x100A15A0, unlock 0x100A1720, endFrame 0x100A1500,
// teardown 0x100A18A0.
void fontRendererInit(void* fontRenderer);
void fontRendererBegin(void* fontRenderer);
void fontRendererFlush(void* fontRenderer);
void fontRendererUnlock(void* fontRenderer);
void fontRendererEndFrame(void* fontRenderer);
void fontRendererTeardown(void* fontRenderer);

// MenuRend/Video.cpp - the Bink movie seams the '?' fill uses. Declared at namespace-geom scope so
// they link to Video.cpp's external definitions.
bool  videoOpen(void* video, const char* path);
void  videoDoFrame(void* video);                // vslot +0x20
bool  videoPathEquals(void* video, const std::string& name);   // vslot +0x24 == name
void* videoFrameD3DTexture(void* video);        // vslot +0x1C getFrame -> the VideoTarget's texture
void* videoAcquireFrame(void* video);
void* videoTargetD3DTexture(void* target);

namespace {

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

// Releases a Video through its virtual slot +0x08 (the only raw-slot release site for Video objects
// that are not owned by a texture record).
static inline void videoReleaseObject(void* video)
{
    void** vv = *reinterpret_cast<void***>(video);
    reinterpret_cast<HostIntFn>(vv[2])(video);
}


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
    static_cast<dx9rend::ShaderParameter*>(p)->setValue(data, count);
}
void paramRelease(void* p)
{
    if (!p) return;
    static_cast<dx9rend::ShaderParameter*>(p)->release();
}
void* paramGetHandle(void* p)     // vslot +0x0C, sub_10021190: `return m_handle`
{
    return static_cast<dx9rend::ShaderParameter*>(p)->getD3DXParam();
}

// The StateManager's own COM vtable, exactly as the shipped call sites reach it.
typedef int (__stdcall* SmSetRenderStateFn)(void*, unsigned int, unsigned int);
void smSetRenderState(unsigned int state, unsigned int value)
{
    void* sm = ::dice::hfe::dx9rend::g_stateManager;
    void** vtbl = *reinterpret_cast<void***>(sm);
    reinterpret_cast<SmSetRenderStateFn>(vtbl[28 / 4])(sm, state, value);   // COM slot 7
}

// fx->SetTexture(handle, tex) / fx->CommitChanges() on the effect's D3DX interface, as the shipped
// bodies call them.
void fxSetTexture(void* fx, void* handle, void* texture)
{
    static_cast<ID3DXEffect*>(fx)->SetTexture(::renddx9abi::d3dxHandle(handle),
                                              static_cast<IDirect3DBaseTexture9*>(texture));
}
void fxCommitChanges(void* fx)
{
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

    // PE 0x101520E0 (this = the style, retn 4): latch a solid color without the cxform transform.
    // Unreachable in 1.50: no code or data references.
    void setSolid(unsigned int packedRgba)
    {
        mode = 1;
        ::memcpy(rgba, &packedRgba, 4);
    }

    // PE 0x10152100 (retn 4): store the width dword at +0x50. Shipped quirk: when the old width is
    // below 1.0 the body first stores 1.0f and then the argument over it; both arms end with the
    // argument (sr_lineStyleWidth carries the inlined copy). Unreachable in 1.50: no code or data references.
    void setWidth(unsigned int widthAsDword)
    {
        ::memcpy(&width, &widthAsDword, 4);
    }

    // PE 0x10152130 (retn 0x18, the only caller is sr_setTexture): id, dims, matrix, wrap/clamp, cxform
    // snapshot.
    void setTexture(int texIdArg, int w, int h, const float* matrix, int clamp, const float* currentCxform)
    {
        texId = texIdArg;
        texH  = h;
        mode  = (clamp != 0) + 2;
        rgba[0] = 0xFF; rgba[1] = 0xFF; rgba[2] = 0xFF; rgba[3] = 0xFF;
        texW  = w;
        for (int i = 0; i < 6; ++i)
            texMatrix[i] = matrix[i];
        ::memcpy(cxform, currentCxform, 32);
        hasAdditive = (cxform[1] > 1.0f || cxform[3] > 1.0f
                       || cxform[5] > 1.0f || cxform[7] > 1.0f) ? 1 : 0;
    }
};
static_assert(sizeof(SwiffStyle) == 84, "five 84-byte style blocks at +128..+547");


// Texture-registry record; the growable ID-indexed array lives at SwiffRender+884.

struct SwiffTextureRecord            // ctor sub_101BEFA0, init sub_101BF000
{
    int           id;               // +0
    IUnknown*     texture;          // +4   IDirect3DBaseTexture9*, AddRef'd by init
    void*         video;            // +8   dice.hfe.geom.Video for '?' movie fills
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

    // PE 0x101B9AB0 (also 0x101B9A40, a second copy of the same body with no caller in 1.50):
    // by id; -1 yields null, every other id must index the vector.
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
void recordInit(SwiffTextureRecord* rec, int id, IUnknown* texture, const std::string& name,
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

// sub_101BF040 - addRef.
void recordAddRef(SwiffTextureRecord* rec) { ++rec->refCount; }

// PE 0x101BF050 (thiscall, `retn 4`) - the movie-fill Video, stored at +8 after init.
void recordSetVideo(SwiffTextureRecord* rec, void* video) { rec->video = video; }

// PE 0x101BF060. Release and destroy at zero.
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

// PE 0x101B9AE0. Dispose and free every live record, then free the slot array.
void registryClear(SwiffTextureRegistry& reg)
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
    // [CHOSEN, not recovered] Keep native container storage empty after cleanup.
    // PE 0x101B9B49 writes EBX into its three storage words; when the final slot
    // was nonnull EBX is its freed record, and 0x101B9B68 can free it again.
    // Preserve the existing native omission of that invalid storage/double free.
    reg.slots.~vector();
    new (&reg.slots) std::vector<SwiffTextureRecord*>();
}

// PE 0x101B9B60. Clear all records (registryClear), then free the array a second time and zero its
// three storage words; the native omission above covers that second free.
void registryDestroy(SwiffTextureRegistry& reg)
{
    registryClear(reg);
}

// PE 0x101B94E0 (thiscall, retn 4; neither the receiver nor the stack argument is read). Asks the
// host class manager (ds:0x10244A94, vslot +0x48) to unregister the singleton named "SwiffRender".
// Its only retail references are the two exception funnels of the vector copy at PE 0x101B9520 /
// 0x101B95C0, which call it once per element; [INFERRED] name, [UNRESOLVED] why that funnel reaches it.
void swiffRenderUnregisterSingleton()
{
    ::dice::hfe::classManager->unregisterSingleton(renddx9abi::HostString("SwiffRender"));
}


// PE 0x101D67F0 initializes global 0x1026741C from "images/blank.png".
// Its only other users are the two loader concatenations and its destruction.
const std::string kSwiffFallbackName("images/blank.png");

// Caches GSModDirectory on first use, defaulting to "No Current Mod".
std::string& modDirectory()
{
    static std::string s_dir;
    static bool         s_read = false;    // byte_1023B170 ships as 1 = "needs reading"
    if (!s_read)
    {
        const std::string fallback("No Current Mod");
        const std::string key("GSModDirectory");
        s_dir = ::dice::hfe::settings->stringGet(key, fallback);
        s_read = true;
    }
    return s_dir;
}

// PE 0x101B9C30. Resolve menu paths, load through the mounted filesystem, and cache the record.
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

    modDirectory();
    void* device = ::dice::hfe::dx9rend::renderDevice();
    IDirect3DTexture9* texture = 0;
    std::string prefix("");
    std::string path("");
    const std::string::size_type http = name.find("http://");
    ::renddx9abi::EngineStream stream;

    if (name == "")
    {
        prefix = "Menu/External/FlashMenu/";
        path = prefix + kSwiffFallbackName;
    }
    else if (http != std::string::npos)
    {
        // The server-logo arm (0x101B9DBA): the host's ServerLogoManager singleton maps the URL onto
        // its "@HOME@\LogoCache\" download. Neither the class manager nor the singleton is tested.
        void* logoMgr =
            ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("ServerLogoManager"));
        // Convert registry VC7.1 keys to the host string type before calling the singleton.
        // RW-266: typed in this flavour - IServerLogoManager slot 8 (+0x20 download)
        // and slot 6 (+0x18 parseUrl), and HostString IS the host's std::string here.
        IServerLogoManager* const logos =
            static_cast<IServerLogoManager*>(static_cast<IBase*>(logoMgr));
        if (!logos->download(name))                                          // +0x20 download
            return -1;
        renddx9abi::HostString dir, file;
        logos->parseUrl(name, dir, file);                                    // +0x18 parseUrl
        path  = "@HOME@\\LogoCache\\";
        path += dir;
        path += file;
    }
    else if (name.c_str()[0] == '@')
    {
        prefix = modDirectory();
        path = prefix + std::string(name.c_str() + 1);
        // Shipped defect: a successful probe is discarded without releasing it.
        if (!::dice::hfe::fileManager->open(path, static_cast<renddx9abi::HostFileMode>(2),
                                           static_cast<renddx9abi::HostFileAccess>(0), 0))
        {
            prefix = "mods/bf2";
            path = prefix + std::string(name.c_str() + 1);
            if (!::dice::hfe::fileManager->open(path, static_cast<renddx9abi::HostFileMode>(2),
                                               static_cast<renddx9abi::HostFileAccess>(0), 0))
                return -1;
        }
    }
    else if (name.c_str()[0] == '$')
    {
        path = std::string(name.c_str() + 1);
    }
    else if (name.c_str()[0] == '#')
    {
        prefix = "@ROOT@/";
        path = prefix + std::string(name.c_str() + 1);
    }
    else if (name.c_str()[0] == '?')
    {
        // Movie fills try the active mod, then base BF2. A successful Video owns playback;
        // the fill retains a fixed 256x256 authoring size regardless of movie dimensions.
        prefix = modDirectory() + "/Movies/";
        path = prefix + std::string(name.c_str() + 1);
        ::renddx9abi::EngineStream probe;
        if (!probe.open(path, 2, 0, 0))
        {
            prefix = "mods/bf2/Movies/";
            path = prefix + std::string(name.c_str() + 1);
            if (!probe.open(path, 2, 0, 0))
                return -1;
        }
        // [CHOSEN, not recovered] Use the rebuilt ClassManager's typed overload;
        // its native virtual order differs from the PE's by-value-name slot +0x2C.
        void* video = reinterpret_cast<ClassManager*>(::dice::hfe::classManager)->createInstance(
            std::string("dice.hfe.geom.Video"), 0xB801DD04u, 0);
        if (video && !videoOpen(video, path.c_str()))
        {
            videoReleaseObject(video);
            probe.close();
            return -1;
        }
        probe.close();
        *outW = 256u;
        *outH = 256u;
        void* storage = ::renddx9abi::engineAlloc(sizeof(SwiffTextureRecord));
        SwiffTextureRecord* rec = storage ? recordConstruct(storage) : 0;
        const unsigned int  id  = reg.insert(rec);
        recordInit(rec, (int)id, 0, name, 256u, 256u);
        recordSetVideo(rec, video);
        return rec->id;
    }
    else
    {
        prefix = "Menu/External/FlashMenu/";
        path = prefix + name;
    }

    if (!stream.open(path, 2, 0, 0))
    {
        // The shipped retry: whatever the routing produced, fall back to the FlashMenu directory.
        prefix = "Menu/External/FlashMenu/";
        path = prefix + kSwiffFallbackName;
        if (!stream.open(path, 2, 0, 0))
            return -1;
    }

    const unsigned int size = static_cast<unsigned int>(stream.getSize());
    void* bytes = ::renddx9abi::engineAlloc(size);
    const unsigned int read = stream.read(bytes, size);
    stream.close();
    D3DXIMAGE_INFO info;
    D3DXGetImageInfoFromFileInMemory(bytes, read, &info);
    const HRESULT hr = D3DXCreateTextureFromFileInMemory(static_cast<IDirect3DDevice9*>(device),
                                                         bytes, read, &texture);
    if (hr != 0)
    {
        if (bytes) ::renddx9abi::engineFree(bytes);
        return -1;
    }
    if (bytes) ::renddx9abi::engineFree(bytes);
    IDirect3DSurface9* surface = 0;
    texture->GetSurfaceLevel(0, &surface);
    D3DSURFACE_DESC desc;
    surface->GetDesc(&desc);
    surface->Release();

    // Report dimensions from image metadata, not the surface descriptor.
    *outW = info.Width;
    *outH = info.Height;

    void* storage = ::renddx9abi::engineAlloc(sizeof(SwiffTextureRecord));
    SwiffTextureRecord* rec = storage ? recordConstruct(storage) : 0;
    const unsigned int height = *outH;
    const unsigned int width = *outW;
    const unsigned int  id  = reg.insert(rec);
    recordInit(rec, (int)id, static_cast<IUnknown*>(texture), name, width, height);
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
// Fonts use the existing native wrapper layout (RW-80).

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
    void*               currentVideo;     // +976   ctor 0; selectVideo stores the Video it plays [INFERRED] name
    int                 u980;             // +980   ctor 0
    std::map<std::string, int> texTechniqueMap;   // +984   texture name -> technique-table index
    void*               techRecord[9];    // +996   {0, TS0, TS1, TS2, TS3, Shape, Wrap, Clamp, Line}
    void*               swiffHost;        // +1032
    void*               fontRenderer;     // +1036  the FontRenderer singleton
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

    // PE 0x101525F0: unlock the current ring buffer when it is locked. Unreachable in 1.50: no code or
    // data references (ringAdvance and flushStrip carry the same unlock inline).
    void unlockRing()
    {
        if (locked)
        {
            ::dice::hfe::dx9rend::vertexBufferUnlock(vb[vbIndex]);
            locked = 0;
        }
    }

    // PE 0x10152DF0: make the Video named `path` the current one (+976, currentVideo). The two cached Video objects
    // are released968 / released972 (+968 / +972): a hit replays doFrame on it; a miss releases the first,
    // shifts the second down, creates a new Video, opens it and, on success, plays a frame.
    // Unreachable in 1.50: no code or data references.
    // Shipped: the open call receives the std::string reference where Video::open reads a char*, and a
    // failed open releases the new Video without clearing +972. [CHOSEN, not recovered] The native
    // build passes path.c_str() and clears +972 and a null createInstance result (the image reads a
    // string object as a path, dereferences the null and frees the object twice at shutdown); those
    // are memory-safety omissions.
    void selectVideo(const std::string& path)
    {
        for (int i = 0; i < 2; ++i)
        {
            void* const cached = i == 0 ? released968 : released972;
            if (cached && videoPathEquals(cached, path))
            {
                videoDoFrame(cached);                                           // vslot +0x20
                currentVideo = cached;
                return;
            }
        }
        if (released968)
        {
            videoReleaseObject(released968);                                    // vslot +0x08
        }
        released968 = released972;
        void* const video = reinterpret_cast<ClassManager*>(::dice::hfe::classManager)->createInstance(
            std::string("dice.hfe.geom.Video"), 0xB801DD04u, 0);
        released972 = video;
        if (!video)
            return;
        if (!videoOpen(video, path.c_str()))                                   // vslot +0x18
        {
            videoReleaseObject(video);                                          // vslot +0x08
            released972 = 0;
            return;
        }
        videoDoFrame(video);
        currentVideo = video;
    }

    // PE 0x10153320 (no this, retn 0xC): out = the 2x3 matrix {m0 m1 m2 / m3 m4 m5} applied to the point.
    // Each component is summed in x87 registers and narrowed once, at the store. Unreachable in 1.50: no
    // code or data references.
    static void transformPoint(float* out, const float* matrix, const float* point)
    {
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] Native equivalent of the unspilled FMUL/FADDP/FADD sequences.
        const float x = bf2x87::store(bf2x87::add(bf2x87::add(
            bf2x87::mul(bf2x87::wide(matrix[1]), bf2x87::wide(point[1])),
            bf2x87::mul(bf2x87::wide(matrix[0]), bf2x87::wide(point[0]))), bf2x87::wide(matrix[2])));
        const float y = bf2x87::store(bf2x87::add(bf2x87::add(
            bf2x87::mul(bf2x87::wide(matrix[4]), bf2x87::wide(point[1])),
            bf2x87::mul(bf2x87::wide(matrix[3]), bf2x87::wide(point[0]))), bf2x87::wide(matrix[5])));
#else
        const float x = matrix[1] * point[1] + matrix[0] * point[0] + matrix[2];
        const float y = matrix[4] * point[1] + matrix[3] * point[0] + matrix[5];
#endif
        out[0] = x;
        out[1] = y;
    }

    // PE 0x10153BE0: the entry of `fonts` whose name equals `name` case-insensitively, or null. Unreachable in
    // 1.50: no code or data references.
    SwiffFontEntry* findFont(const std::string& name) const
    {
        for (std::size_t i = 0; i < fonts.size(); ++i)
        {
            if (_stricmp(fonts[i]->name.c_str(), name.c_str()) == 0)
                return fonts[i];
        }
        return 0;
    }
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

// PE 0x101B8F70. The returned slot is read after updating the host's index.
void** creditsRainNext(CreditsRain* rain)
{
    void* host = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    const int index = hostGetIntF14(host);
    // [CHOSEN, not recovered] Preserve the existing empty-list omission of the
    // null dereference in PE 0x101B9037 / 0x101B91DD.
    void** result = rain->fonts.empty() ? 0 : &rain->fonts[index];
    unsigned int next = static_cast<unsigned int>(index) + 1u;
    if (next >= rain->fonts.size())
        next = 0;
    hostSetIntF14(host, static_cast<int>(next));
    return result;
}

// PE 0x101B8ED0. Looks the SwiffRender singleton up and drops the answer.
// unreachable in 1.50: no code or data references. [INFERRED] name.
void swiffRenderLookupDiscarded()
{
    ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffRender"));
}

// PE 0x101B9200. Zeroes the forty glyph font slots (the y slots stay uninitialised, as shipped), the
// font vector (constructed empty) and the inited byte; the word at +320 is untouched.
CreditsRain* creditsRainConstruct(void* storage)
{
    CreditsRain* const rain = new (storage) CreditsRain;
    for (int i = 0; i < 40; ++i)
        rain->points[i].font = 0;
    rain->inited = 0;
    return rain;
}

// PE 0x101B8F00. Releases every non-null rain font through the SwiffRender singleton's slot 27
// (+0x6C); the vector and the scroller are the caller's to destroy and free.
void creditsRainReleaseFonts(CreditsRain* r)
{
    ui::ISwiffRender* renderer = static_cast<ui::ISwiffRender*>(
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffRender")));
    for (unsigned int i = 0; i < r->fonts.size(); ++i)
    {
        if (r->fonts[i])
            renderer->releaseFontHandle(r->fonts[i]);
    }
}

// PE 0x101B8FF0. Reseeds the forty rows: font from creditsRainNext, y from 15360 in steps of 400,
// then the SwiffHost reseed flag (+0xF18) is cleared and the inited byte set.
void creditsRainReseed(CreditsRain* rain)
{
    void* resetHost = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    float y = 15360.0f;
    for (int i = 0; i < 40; ++i)
    {
        void** font = creditsRainNext(rain);
        rain->points[i].y = y;
        y += 400.0f;
        // [CHOSEN, not recovered] retail dereferences the slot unguarded (PE 0x101B9037); an empty
        // font list answers null here instead of faulting (memory-safety exception).
        rain->points[i].font = font ? *font : 0;
    }
    hostSetFlagF18(resetHost, 0);
    rain->inited = 1;
}

// PE 0x101B9070. Scroll and draw the forty credits rows.
void creditsRainUpdate(CreditsRain* rain, float dt)
{
    void* host = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffHost"));
    ui::ISwiffRender* renderer = static_cast<ui::ISwiffRender*>(
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffRender")));
    if (hostGetFlagF18(host))
    {
        creditsRainReseed(rain);                                     // PE 0x101B8FF0
        return;
    }
    if (rain->inited)
    {
        dt = 0.0f;
        rain->inited = 0;
    }
    for (int i = 0; i < 40; ++i)
    {
        const float matrix[6] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, rain->points[i].y };
        renderer->drawGlyphBitmap(rain->points[i].font, matrix, ~0u, 0);
        const float speed = hostGetFloatF0C(host);
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] The speed call's x87 result/product is not
        // narrowed before the subtraction and final row-position store.
        rain->points[i].y = bf2x87::store(bf2x87::sub(bf2x87::wide(rain->points[i].y),
            bf2x87::mul(bf2x87::wide(speed), bf2x87::wide(dt))));
#else
        rain->points[i].y -= speed * dt;
#endif
    }
    if (rain->points[0].y < -400.0f)
    {
        for (int i = 0; i < 39; ++i)
            rain->points[i] = rain->points[i + 1];
        void** font = creditsRainNext(rain);
        rain->points[39].font = font ? *font : 0;
        rain->points[39].y = rain->points[38].y + 400.0f;
    }
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


// PE 0x101531B0 - apply the cxform to four color bytes, clamped 0..255, packed little-endian
// r,g,b,a (so the dword reads 0xAABBGGRR in memory order r at +0).
unsigned int applyCxform(const float* cx, const unsigned char* rgba)
{
    unsigned int out = 0;
    for (int i = 0; i < 4; ++i)
    {
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] Retain the unspilled upper comparison, then
        // reload the single-precision spill for the lower comparison/conversion.
        const bf2x87::Real value = bf2x87::add(bf2x87::mul(
            bf2x87::wide(rgba[i]), bf2x87::wide(cx[2 * i])), bf2x87::wide(cx[2 * i + 1]));
        float v = bf2x87::store(value);
        if (bf2x87::notLess(value, bf2x87::wide(255))) v = 255.0f;
        else if (!(v > 0.0f)) v = 0.0f;
#else
        float v = (float)rgba[i] * cx[2 * i] + cx[2 * i + 1];
        if (!(v < 255.0f)) v = 255.0f;
        else if (!(v > 0.0f)) v = 0.0f;
#endif
        out |= ((unsigned int)(unsigned char)(unsigned __int64)v) << (8 * i);
    }
    return out;
}


// PE 0x10153140 - advance the VB ring: unlock, next of three, cursor 0, lock discard.
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

float transformCoordinate(double x, double y, const float* matrix, int column)
{
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Native equivalent of the unspilled FMUL/FADDP/FADD
    // sequence at PE 0x10154D34 and 0x101547DF, narrowed only at FSTP.
    return bf2x87::store(bf2x87::add(bf2x87::add(
        bf2x87::mul(bf2x87::wide(y), bf2x87::wide(matrix[4 + column])),
        bf2x87::mul(bf2x87::wide(x), bf2x87::wide(matrix[column]))),
        bf2x87::wide(matrix[12 + column])));
#else
    return (float)(y * matrix[4 + column] + x * matrix[column] + matrix[12 + column]);
#endif
}


// PE 0x10153D00 - bind the technique, diffuse/texgen parameters, and texture for one style block.
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
        void* fx = effectD3dxHandle(s->effect);
        void* handle = paramGetHandle(s->paramTexture);
        fxSetTexture(fx, handle, 0);
        return;
    }

    if (style->mode == 2 || style->mode == 3)
    {
        // Textured: alpha-blend states, the cxform-derived diffuse, the texgen rows, the texture.
        const bool additive = style->hasAdditive != 0;
        smSetRenderState(27, 1);     // D3DRS_ALPHABLENDENABLE
        smSetRenderState(19, 5);     // D3DRS_SRCBLEND  = srcalpha
        smSetRenderState(20, 6);     // D3DRS_DESTBLEND = invsrcalpha

        float diffuse[4];
        if (additive)
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

#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] FILD reads signed dimensions; reciprocals
        // remain in x87 registers until each texgen product is stored.
        const bf2x87::Real invW = bf2x87::div(bf2x87::wide(1), bf2x87::wide(style->texW));
        const bf2x87::Real invH = bf2x87::div(bf2x87::wide(1), bf2x87::wide(style->texH));
#else
        const double invW = 1.0 / style->texW;
        const double invH = 1.0 / style->texH;
#endif
        float texGenS[4], texGenT[4];
        texGenS[0] = static_cast<float>(invW * style->texMatrix[0]);
        texGenS[1] = static_cast<float>(invW * style->texMatrix[1]);
        texGenS[2] = 0.0f;
        texGenS[3] = static_cast<float>(invW * style->texMatrix[2]);
        texGenT[0] = static_cast<float>(invH * style->texMatrix[3]);
        texGenT[1] = static_cast<float>(invH * style->texMatrix[4]);
        texGenT[2] = 0.0f;
        texGenT[3] = static_cast<float>(invH * style->texMatrix[5]);
        paramSetValue(s->paramTexGenS, texGenS, 0);
        paramSetValue(s->paramTexGenT, texGenT, 0);

        if (style->texId != -1)
        {
            SwiffTextureRecord* rec = s->registry.byId(style->texId);

            // The per-texture technique override map: name -> index into techRecord[].
            const std::map<std::string, int>::const_iterator node =
                s->texTechniqueMap.find(rec->name);
            if (node != s->texTechniqueMap.end())
            {
                const unsigned int idx = static_cast<unsigned int>(node->second);
                effectSetTechniqueRecord(s->effect, s->techRecord[idx]);
            }

            void* d3dTexture = 0;
            if (rec)
            {
                if (rec->video)
                {
                    // The movie-fill arm: advance the Bink movie one frame (BinkWait-gated,
                    // so it only decodes when the clock says it is time) and bind the VideoTarget's
                    // D3D texture, which the frame was copied into.
                    void* frame = videoAcquireFrame(rec->video);
                    d3dTexture = videoTargetD3DTexture(frame);
                    videoDoFrame(rec->video);
                    static_cast<IBase*>(frame)->release();
                }
                else
                {
                    d3dTexture = rec->texture;
                }
            }
            void* fx = effectD3dxHandle(s->effect);
            void* handle = paramGetHandle(s->paramTexture);
            fxSetTexture(fx, handle, d3dTexture);
            s->currentTexture = d3dTexture;
        }
    }
}


// sub_10154220 - flush the accumulated strip: apply the latched style, unlock, bind stream + decl,
// effect begin -> per-pass DrawPrimitive(trianglestrip, cursor-2), end, ring re-lock.

// PE 0x10154220
void flushStrip(SwiffRender* s)
{
    using namespace ::dice::hfe::dx9rend;
    if (!s->vbCursor)
        return;

    applyStyleState(s, &s->style[3]);          // the flush latch at +380

    if (s->locked)                             // PE 0x10154240: the unlock is conditional on the lock flag
    {
        vertexBufferUnlock(s->vb[s->vbIndex]);
        s->locked = 0;
    }
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


// PE 0x10154A20 - the textured/solid fill collector: flush, then re-fill the ring with the
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
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] FISTP's masked-invalid result is 0x80000000;
    // native float-to-int casts would otherwise be undefined outside int range.
    const auto colorByte = [](float value) {
        return static_cast<unsigned char>(bf2x87::toIntTruncate(bf2x87::wide(value)));
    };
#else
    const auto colorByte = [](float value) { return static_cast<unsigned char>(static_cast<int>(value)); };
#endif
    if (s->style[3].hasAdditive)
    {
        r = colorByte(s->style[3].cxform[1]);
        g = colorByte(s->style[3].cxform[3]);
        b = colorByte(s->style[3].cxform[5]);
        a = 255;
        z = 0.0f;
    }
    else
    {
        r = colorByte(s->style[3].cxform[0]);
        g = colorByte(s->style[3].cxform[2]);
        b = colorByte(s->style[3].cxform[4]);
        a = colorByte(s->style[3].cxform[6]);
    }
    const unsigned int color =
        ((unsigned int)b) | ((((((unsigned int)a << 8) | r) << 8) | g) << 8);

    float* v = base;
    const int coordinateCount = static_cast<int>(count * 2u);
    for (int i = 0; i < coordinateCount; i += 2)
    {
        v[0] = (float)verts[i]     - 0.5f;
        v[1] = (float)verts[i + 1] - 0.5f;
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

// PE 0x10155C70
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

// PE 0x10155C30
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

// PE 0x101528E0. Upload an A8 alpha image (font glyph pages). D3DFMT_A8 == 28.
int RD9_THUNK sr_uploadAlpha(void* self RD9_THUNK_EDX, unsigned int w, unsigned int h, const void* data)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    void* device = ::dice::hfe::dx9rend::renderDevice();
    IDirect3DTexture9* tex = 0;
    if (D3DXCreateTexture(static_cast<IDirect3DDevice9*>(device), w, h, 1, 0, D3DFMT_A8,
                          D3DPOOL_MANAGED, &tex) != 0)
        return -1;
    IDirect3DTexture9* t = tex;
    IDirect3DSurface9* surface = 0;
    t->GetSurfaceLevel(0, &surface);
    RECT rect = { 0, 0, (LONG)w, (LONG)h };
    if (D3DXLoadSurfaceFromMemory(surface, 0, 0, data, D3DFMT_A8, w, 0, &rect, 0xFFFFFFFF, 0) != 0)
    {
        // Shipped defect: the acquired surface reference leaks on refusal.
        if (t) t->Release();
        return -1;
    }
    if (surface)
        surface->Release();
    t->GetLevelCount();                         // descriptor's discarded mip-count query
    const int id = registryRegisterUploaded(s->registry, t);
    t->Release();
    return id;
}

// 0x101529D0: `or eax,-1; ret 4`. Its one argument is SwiffPlayer's image::rgb* (0x10005B56).
#if !defined(_MSC_VER)
int RD9_THUNK sr_slot8(void* RD9_THUNK_EDX, void*)   // [CHOSEN, not recovered] native pointer width
#else
int RD9_THUNK sr_slot8(void* RD9_THUNK_EDX, int)
#endif
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

// PE 0x101529E0. Upload an RGBA image: swap R<->B in place, load as A8R8G8B8 (21), register.
int RD9_THUNK sr_uploadRgba(void* self RD9_THUNK_EDX, unsigned int* desc)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    const SwiffRgbaImage* const image = reinterpret_cast<const SwiffRgbaImage*>(desc);
    void* device = ::dice::hfe::dx9rend::renderDevice();
    IDirect3DTexture9* tex = 0;
    if (D3DXCreateTexture(static_cast<IDirect3DDevice9*>(device), image->width, image->height, 1, 0,
                          D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex) != 0)
        return -1;
    IDirect3DTexture9* t = tex;
    IDirect3DSurface9* surface = 0;
    t->GetSurfaceLevel(0, &surface);

    RECT rect = { 0, 0, (LONG)image->width, (LONG)image->height };
    unsigned char* row = image->data;
    for (int y = 0; y < static_cast<int>(image->height); ++y)
    {
        unsigned char* px = row;
        for (int x = 0; x < static_cast<int>(image->width); ++x, px += 4)
        {
            const unsigned char tmp = px[0];
            px[0] = px[2];
            px[2] = tmp;
        }
        row += image->pitch;
    }

    if (D3DXLoadSurfaceFromMemory(surface, 0, 0, image->data,
                                  D3DFMT_A8R8G8B8, image->pitch, 0, &rect, 0xFFFFFFFF, 0) != 0)
    {
        // Shipped defect: the acquired surface reference leaks on refusal.
        if (t) t->Release();
        return -1;
    }
    if (surface)
        surface->Release();
    t->GetLevelCount();                         // descriptor's discarded mip-count query
    const int id = registryRegisterUploaded(s->registry, t);
    t->Release();
    return id;
}

// PE 0x10152B30. Load by name through the routing loader. Note the raw char* argument.
int RD9_THUNK sr_loadTexture(void* self RD9_THUNK_EDX, const char* name,
                              unsigned int* outW, unsigned int* outH)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    unsigned int w = 0, h = 0;
    int id;
    {
        std::string key(name);
        id = registryGetOrLoad(s->registry, key, &w, &h, 0);
    }
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

// PE 0x101534B0. Nine stack arguments; the first is unused in the shipped body too.
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

    auto& output = *reinterpret_cast<D3DVIEWPORT9*>(s->savedViewport);
    device->GetViewport(&output);
    const auto menu = ui::MenuViewport::fit(output.Width, output.Height);
    // Clear just the unused pixels. beginFrame has already cleared the movie background.
    D3DRECT margins[2];
    DWORD count = 0;
    const LONG left = output.X, top = output.Y;
    const LONG right = left + output.Width, bottom = top + output.Height;
    if (menu.width < output.Width)
    {
        if (menu.x) margins[count++] = {left, top, LONG(left + menu.x), bottom};
        margins[count++] = {LONG(left + menu.x + menu.width), top, right, bottom};
    }
    else if (menu.height < output.Height)
    {
        if (menu.y) margins[count++] = {left, top, right, LONG(top + menu.y)};
        margins[count++] = {left, LONG(top + menu.y + menu.height), right, bottom};
    }
    if (count)
        device->Clear(count, margins, D3DCLEAR_TARGET, 0xff000000, 1.0f, 0);
    s->stencilRef = 1;

#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Preserve the retail FILD precision and explicit
    // single-precision spills on native processors.
    using namespace bf2x87;
    const float invMovieW = store(div(wide(1), wide(s->movieWidth)));
    const float invMovieH = store(div(wide(1), wide(static_cast<unsigned int>(s->movieHeight))));
#else
    const float invMovieW = 1.0f / (float)s->movieWidth;
    double movieH = (double)s->movieHeight;
    if (s->movieHeight < 0)
        movieH += 4294967296.0;                 // exact 0x4F800000 correction
    const float invMovieH = (float)(1.0 / movieH);
#endif

    D3DVIEWPORT9 vp;
#if !defined(_MSC_VER)
    vp.X      = (DWORD)toIntTruncate(mul(wide(vpX), wide(s->scaleX)));
    vp.Y      = (DWORD)toIntTruncate(mul(wide(vpY), wide(s->scaleY)));
    vp.Width  = (DWORD)toIntTruncate(mul(wide(vpWidth), wide(s->scaleX)));
    vp.Height = (DWORD)toIntTruncate(mul(wide(vpHeight), wide(s->scaleY)));
#else
    vp.X      = (DWORD)(int)((float)vpX * s->scaleX);
    vp.Y      = (DWORD)(int)((float)vpY * s->scaleY);
    vp.Width  = (DWORD)(int)((float)vpWidth * s->scaleX);
    vp.Height = (DWORD)(int)((float)vpHeight * s->scaleY);
#endif
    vp.X += output.X + menu.x;
    vp.Y += output.Y + menu.y;
    vp.MinZ   = 0.0f;
    vp.MaxZ   = 1.0f;
    device->SetViewport(&vp);

    // M616: the SWF-twips orthographic projection over [xMin..xMax] x [yMin..yMax].
    ::memset(s->mtxOrthoTwips, 0, sizeof(s->mtxOrthoTwips));
    s->mtxOrthoTwips[10] = 1.0f;
    s->mtxOrthoTwips[15] = 1.0f;
#if !defined(_MSC_VER)
    const Real width = sub(wide(xMax), wide(xMin));
    const Real height = sub(wide(yMax), wide(yMin));
    s->mtxOrthoTwips[0]  = store(div(wide(2), width));
    s->mtxOrthoTwips[5]  = store(div(wide(-2), height));
    s->mtxOrthoTwips[12] = store(-div(add(wide(xMin), wide(xMax)), width));
    s->mtxOrthoTwips[13] = store(div(add(wide(yMin), wide(yMax)), height));
#else
    s->mtxOrthoTwips[0]  = 2.0f / (xMax - xMin);
    s->mtxOrthoTwips[5]  = -2.0f / (yMax - yMin);
    s->mtxOrthoTwips[12] = -((xMin + xMax) / (xMax - xMin));
    s->mtxOrthoTwips[13] = (yMin + yMax) / (yMax - yMin);
#endif

    // M680: the same bounds in pixels (twips x 0.05).
#if !defined(_MSC_VER)
    const Real pxMinX = mul(wide(xMin), wide(0.05f));
    const Real pxMaxX = mul(wide(xMax), wide(0.05f));
    const float pxMaxY = store(mul(wide(yMax), wide(0.05f)));
    const float pxMinY = store(mul(wide(yMin), wide(0.05f)));
    const Real pxWidth = sub(pxMaxX, pxMinX);
    const float pxHeight = store(sub(wide(pxMaxY), wide(pxMinY)));
#else
    const float pxMinX = xMin * 0.050000001f, pxMaxX = xMax * 0.050000001f;
    const float pxMinY = yMin * 0.050000001f, pxMaxY = yMax * 0.050000001f;
#endif
    ::memset(s->mtxOrthoPixels, 0, sizeof(s->mtxOrthoPixels));
    s->mtxOrthoPixels[10] = 1.0f;
    s->mtxOrthoPixels[15] = 1.0f;
#if !defined(_MSC_VER)
    s->mtxOrthoPixels[0]  = store(div(wide(2), pxWidth));
    s->mtxOrthoPixels[5]  = store(div(wide(-2), wide(pxHeight)));
    s->mtxOrthoPixels[12] = store(-div(add(pxMinX, pxMaxX), pxWidth));
    s->mtxOrthoPixels[13] = store(div(add(wide(pxMinY), wide(pxMaxY)), wide(pxHeight)));
#else
    s->mtxOrthoPixels[0]  = 2.0f / (pxMaxX - pxMinX);
    s->mtxOrthoPixels[5]  = -2.0f / (pxMaxY - pxMinY);
    s->mtxOrthoPixels[12] = -((pxMinX + pxMaxX) / (pxMaxX - pxMinX));
    s->mtxOrthoPixels[13] = (pxMinY + pxMaxY) / (pxMaxY - pxMinY);
#endif

    // M552: the movie-space -> viewport-subrect NDC remap.
    ::memset(s->mtxRemap, 0, sizeof(s->mtxRemap));
    s->mtxRemap[10] = 1.0f;
    s->mtxRemap[15] = 1.0f;
#if !defined(_MSC_VER)
    const Real remapY = mul(wide(static_cast<float>(vpHeight)), wide(invMovieH));
    s->mtxRemap[0]  = store(mul(wide(static_cast<float>(vpWidth)), wide(invMovieW)));
    s->mtxRemap[5]  = store(remapY);
    s->mtxRemap[12] = store(sub(mul(add(add(wide(static_cast<float>(vpX)),
        wide(static_cast<float>(vpX))), wide(static_cast<float>(vpWidth))), wide(invMovieW)), wide(1)));
    const Real remapVpY = mul(wide(static_cast<float>(vpY)), wide(invMovieH));
    s->mtxRemap[13] = store(sub(sub(wide(1), add(remapVpY, remapVpY)), remapY));
#else
    s->mtxRemap[0]  = (float)vpWidth * invMovieW;
    s->mtxRemap[5]  = (float)vpHeight * invMovieH;
    s->mtxRemap[12] = ((float)vpX + (float)vpX + (float)vpWidth) * invMovieW - 1.0f;
    s->mtxRemap[13] = 1.0f
        - ((float)vpY * invMovieH + (float)vpY * invMovieH)
        - (float)vpHeight * invMovieH;
#endif

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

    // 0x101539E5: the FontRenderer starts a new run (sub_100A1470); the pointer is not tested.
    fontRendererBegin(s->fontRenderer);
}

// PE 0x101544F0. Flush, credits rain under a scissor, viewport restore, texture unbind.
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
        static_cast<IDirect3DDevice9*>(renderDevice())->SetViewport(
            reinterpret_cast<const D3DVIEWPORT9*>(s->savedViewport));
        stateManagerPop();
        return;
    }

    IDirect3DDevice9* device = static_cast<IDirect3DDevice9*>(renderDevice());
    flushStrip(s);
    fontRendererFlush(s->fontRenderer);                 // 0x10154538

    if (s->trail && hostGetFlagF10(s->swiffHost))
    {
        // The credits window: host vslot +840 fills the rect, scaled and scissored, then the rain
        // draws inside it with D3DRS_SCISSORTESTENABLE (174).
        int hostRect[4] = { 0, 0, 0, 0 };
        hostGetRect(s->swiffHost, hostRect);
        RECT sc;
#if !defined(_MSC_VER)
        // [CHOSEN, not recovered] FILD/FISTP retains integer input precision.
        sc.left   = bf2x87::toIntTruncate(bf2x87::mul(bf2x87::wide(hostRect[0]), bf2x87::wide(s->scaleX)));
        sc.top    = bf2x87::toIntTruncate(bf2x87::mul(bf2x87::wide(hostRect[1]), bf2x87::wide(s->scaleY)));
        sc.bottom = bf2x87::toIntTruncate(bf2x87::mul(bf2x87::wide(hostRect[3]), bf2x87::wide(s->scaleY)));
        sc.right  = bf2x87::toIntTruncate(bf2x87::mul(bf2x87::wide(hostRect[2]), bf2x87::wide(s->scaleX)));
#else
        sc.left   = (LONG)((float)hostRect[0] * s->scaleX);
        sc.top    = (LONG)((float)hostRect[1] * s->scaleY);
        sc.right  = (LONG)((float)hostRect[2] * s->scaleX);
        sc.bottom = (LONG)((float)hostRect[3] * s->scaleY);
#endif
        const auto& output = *reinterpret_cast<const D3DVIEWPORT9*>(s->savedViewport);
        const auto menu = ui::MenuViewport::fit(output.Width, output.Height);
        sc.left += output.X + menu.x;
        sc.right += output.X + menu.x;
        sc.top += output.Y + menu.y;
        sc.bottom += output.Y + menu.y;
        device->SetScissorRect(&sc);
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, 1);

        creditsRainUpdate(s->trail, s->trailDt);

        fontRendererFlush(s->fontRenderer);             // 0x10154617, the rain's names
        device->SetRenderState(D3DRS_SCISSORTESTENABLE, 0);
    }
    ++s->stats[3];      // +1072

    if (s->locked)
    {
        vertexBufferUnlock(s->vb[s->vbIndex]);
        s->locked = 0;
    }
    fontRendererUnlock(s->fontRenderer);                // 0x10154655
    fontRendererEndFrame(s->fontRenderer);              // 0x10154660

    device->SetViewport(reinterpret_cast<D3DVIEWPORT9*>(s->savedViewport));

    if (s->effect)
    {
        void* fx = effectD3dxHandle(s->effect);
        void* handle = paramGetHandle(s->paramTexture);
        fxSetTexture(fx, handle, 0);
        fxCommitChanges(effectD3dxHandle(s->effect));
    }
    stateManagerPop();
}

// PE 0x10152C10. SWF 2x3 -> 4x4, x M616 x M552, latched and pushed to transform.
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

// PE 0x10152D00
void RD9_THUNK sr_setCxform(void* self RD9_THUNK_EDX, const float* cx)
{
    ::memcpy(static_cast<SwiffRender*>(self)->cxform, cx, 32);
}

// PE 0x10153A00
int RD9_THUNK sr_recordStat(void* self RD9_THUNK_EDX, int value)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    s->statPrev = s->statCur;
    s->statCur  = value;
    if (value < s->statMin) s->statMin = value;
    if (value >= s->statMax) s->statMax = value;
    return value;
}

// PE 0x10154BF0. The solid arm accumulates a stitched triangle strip; the textured
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

    fontRendererFlush(s->fontRenderer);                 // 0x10154C4D, before geometry

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

    float countBits;
    ::memcpy(&countBits, &count, sizeof(countBits));
    double x = countBits, y = countBits;
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
        v[0] = transformCoordinate(x, y, s->mtxCurrent, 0);
        v[1] = transformCoordinate(x, y, s->mtxCurrent, 1);
        *reinterpret_cast<unsigned int*>(v + 3) = color;
        v += 4;
        s->vbCursor += 2;
    }
    s->vbCursor += count;

    const int coordinateCount = static_cast<int>(count * 2u);
    for (int i = 0; i < coordinateCount; i += 2)
    {
        x = (double)verts[i]     - 0.5;
        y = (double)verts[i + 1] - 0.5;
        v[0] = transformCoordinate(x, y, s->mtxCurrent, 0);
        v[1] = transformCoordinate(x, y, s->mtxCurrent, 1);
        *reinterpret_cast<unsigned int*>(v + 3) = color;
        v += 4;
    }
    s->lastX = transformCoordinate(x, y, s->mtxCurrent, 0);
    s->lastY = transformCoordinate(x, y, s->mtxCurrent, 1);
}

// PE 0x101546D0. Line style + "Line" technique, transformed into the static
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

    // unk_1025E9E0's stand-in. [CHOSEN, not recovered] PE 0x101547B0 has no
    // bounds check; keep the existing native omission of its possible overrun.
    // The shipped static's extent is unread; ours is sized to
    // the ring convention (10002 vertices) and the count is clamped to it rather than overrun.
    static float scratch[3 * 0x2712];
    if (count > 0x2712)
        count = 0x2712;
    const unsigned int passes = effectBegin(s->effect);
    float* v = scratch;
    for (unsigned int i = 0; i < count; ++i)
    {
        const double x = (double)verts[2 * i]     - 0.5;
        const double y = (double)verts[2 * i + 1] - 0.5;
        v[0] = transformCoordinate(x, y, s->mtxCurrent, 0);
        v[1] = transformCoordinate(x, y, s->mtxCurrent, 1);
        v[2] = 0.0f;
        v += 3;
    }

    for (unsigned int i = 0; i < passes; ++i)
    {
        effectBeginPass(s->effect, i);
        rendererDrawPrimitiveUP(count - 1, scratch, 12, 3 /* D3DPT_LINESTRIP */);
        ++s->stats[4];              // +1076
    }
    effectEnd(s->effect);
}

// PE 0x10152D20
int RD9_THUNK sr_fillStyleDisable(void* self RD9_THUNK_EDX, int index)
{
    SST("[swiff]   fillStyleDisable side=%d", index);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->style[index].mode = 0;
    return 0;
}

// PE 0x10153A50. Solid fill: cxform-transform the color, mode 1.
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

// PE 0x10152D50 - slot: the style block (indexed by the 84-byte stride) takes the texture with the
// current cxform (this + 0x2E8).
void RD9_THUNK sr_setTexture(void* self RD9_THUNK_EDX, int index, int texId, int w, int h,
                              const float* matrix, int clamp)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    s->style[index].setTexture(texId, w, h, matrix, clamp, s->cxform);
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

// PE 0x10153AA0. Line color, cxform-transformed.
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

// PE 0x10152DA0. The shipped body clamps the OLD width to >= 1.0 and then overwrites it with the
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


// The font slots, 26..37. createFont builds a wrapper through the FontSystem singleton
// (SwiffFont.cpp's createSwiffFont); the forwards dispatch through the wrapper's table. The
// wrapper's layout is [CHOSEN, not recovered] (RW-80), so the forwards keep
// their existing null guards where retail would dereference the wrapper.

// PE 0x10152EE0. Allocation failure returns null without reporting a recovery boundary.
void* RD9_THUNK sr_createFont(void* self RD9_THUNK_EDX, const char* name, int size)
{
    (void)self;
    void* font = createSwiffFont(name, size, 0);
    if (sstEnabled())   // [TEMP] tracer
        ::renddx9abi::logf("[swiff] createFont(\"%s\", %d) -> %p", name ? name : "", size, font);
    return font;
}

// PE 0x10154870 (slot 27): erase the wrapper from the +1040 set, then destroy and free it. Neither
// step tests fontsDisabled; only the delete tests the pointer.
void RD9_THUNK sr_releaseFontHandle(void* self RD9_THUNK_EDX, void* font)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    s->fontSet.erase(font);
    if (font)
        swiffFontDelete(font);
}

// The forwarded argument is the text: SwiffPlayer's rcTextSet passes a const wchar_t* and the font
// wrapper's +16 setText takes one, so it stays pointer-sized on x64 (ISwiffRender.h still spells
// it int).
// PE 0x10152F70
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

// PE 0x101541E0
int RD9_THUNK sr_destroyFont(void* self RD9_THUNK_EDX, void* font)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (!font || s->fontsDisabled)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
    reinterpret_cast<HostVoidFn>(vtbl[8 / 4])(font);   // release
    // Shipped defect: retain the released handle in the tracking set.
    s->fontSet.insert(font);                            // 0x10154212, sub_101557D0
    return 0;
}

// PE 0x101548D0. Flush, cxform the color, build the pixel-snapped 4x4,
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
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Pixel snapping truncates the unspilled x87 sum.
    m[12] = (float)bf2x87::toIntTruncate(bf2x87::add(bf2x87::mul(bf2x87::mul(
        bf2x87::wide(m2x3[2]), bf2x87::wide(0.05f)), bf2x87::wide(s->scaleX)), bf2x87::wide(0.5f)));
    m[13] = (float)bf2x87::toIntTruncate(bf2x87::add(bf2x87::mul(bf2x87::mul(
        bf2x87::wide(m2x3[5]), bf2x87::wide(0.05f)), bf2x87::wide(s->scaleY)), bf2x87::wide(0.5f)));
#else
    m[12] = (float)(int)(m2x3[2] * 0.050000001f * s->scaleX + 0.5f);   // pixel-snapped
    m[13] = (float)(int)(m2x3[5] * 0.050000001f * s->scaleY + 0.5f);
#endif

    void** vtbl = *reinterpret_cast<void***>(font);
    typedef void (__thiscall* DrawFn)(void*, const float*, unsigned int);
    reinterpret_cast<DrawFn>(vtbl[20 / 4])(font, m, color);
}

// PE 0x10152FA0
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

#if !defined(_MSC_VER)
// PE 0x10152FF0. [CHOSEN, not recovered] Natively the passed-through second argument is the
// int* out position it is (SwiffPlayer 0x10005E30 `lea edx,[ebp+0x14]`; font 0x100A48C0 stores
// through it); the MSVC spelling keeps its 32-bit carrier.
char RD9_THUNK sr_fontForward32(void* self RD9_THUNK_EDX, void* font, int* arg,
                                 float x, float y)
#else
char RD9_THUNK sr_fontForward32(void* self RD9_THUNK_EDX, void* font, int arg,
                                 float x, float y)
#endif
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (s->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
#if !defined(_MSC_VER)
    typedef char (__thiscall* Fn)(void*, int*, float, float);
#else
    typedef char (__thiscall* Fn)(void*, int, float, float);
#endif
    return reinterpret_cast<Fn>(vtbl[32 / 4])(font, arg, x * s->scaleX, y * s->scaleY);
}

#if !defined(_MSC_VER)
// PE 0x10153030 -> font +0x28 (0x10152080), which writes both outs. [CHOSEN, not recovered]
// Natively they are the int* the SwiffPlayer forwarder 0x10005E50 passes.
char RD9_THUNK sr_fontForward40(void* self RD9_THUNK_EDX, void* font, int* a, int* b)
#else
char RD9_THUNK sr_fontForward40(void* self RD9_THUNK_EDX, void* font, int a, int b)
#endif
{
    if (static_cast<SwiffRender*>(self)->fontsDisabled || !font)
        return 0;
    void** vtbl = *reinterpret_cast<void***>(font);
#if !defined(_MSC_VER)
    typedef char (__thiscall* Fn)(void*, int*, int*);
#else
    typedef char (__thiscall* Fn)(void*, int, int);
#endif
    return reinterpret_cast<Fn>(vtbl[40 / 4])(font, a, b);
}

// PE 0x10153060
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

// PE 0x101530B0
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

// PE 0x10153120
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

// PE 0x10154320. Begin the Flash clip-mask trio over the stencil buffer.
void RD9_THUNK sr_beginMask(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled()) { ++g_sst.nMasks; SST("[swiff]   beginMask (ref %u)", s->stencilRef); }   // [TEMP] tracer
    if (!s->swiffHost || hostSuppressed(s))
        return;
    flushStrip(s);
    fontRendererFlush(s->fontRenderer);             // 0x1015434E
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

// PE 0x10154400
void RD9_THUNK sr_endMask(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   endMask (ref %u)", s->stencilRef);   // [TEMP] tracer
    if (!s->swiffHost || hostSuppressed(s))
        return;
    flushStrip(s);
    fontRendererFlush(s->fontRenderer);             // 0x1015442A
    ++s->stats[2];
    smSetRenderState(168, 15);               // colorwriteenable on
    smSetRenderState(57, s->stencilRef);
    smSetRenderState(56, 3);                 // stencilfunc = equal - draw inside the mask
    smSetRenderState(55, 1);                 // keep
    smSetRenderState(53, 1);
    smSetRenderState(54, 1);
}

// PE 0x101544A0
void RD9_THUNK sr_disableMask(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    SST("[swiff]   disableMask");   // [TEMP] tracer
    if (!s->swiffHost || hostSuppressed(s))
        return;
    flushStrip(s);
    fontRendererFlush(s->fontRenderer);             // 0x101544C6
    ++s->stats[2];
    smSetRenderState(52, 0);
}

// PE 0x10152880
void RD9_THUNK sr_setMovieWidth(void* self RD9_THUNK_EDX, unsigned int w)
{
    if (sstEnabled()) ::renddx9abi::logf("[swiff] setMovieWidth(%u)", w);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->movieWidth = w;
}
// PE 0x10152890
void RD9_THUNK sr_setMovieHeight(void* self RD9_THUNK_EDX, int h)
{
    if (sstEnabled()) ::renddx9abi::logf("[swiff] setMovieHeight(%d)", h);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->movieHeight = h;
}
// PE 0x101528B0
void RD9_THUNK sr_setScale(void* self RD9_THUNK_EDX, float sx, float sy)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (sstEnabled()) ::renddx9abi::logf("[swiff] setScale(%.4f, %.4f)", sx, sy);   // [TEMP] tracer
    s->scaleX = sx;
    s->scaleY = sy;
}
// PE 0x101528A0
void RD9_THUNK sr_setBackgroundColor(void* self RD9_THUNK_EDX, unsigned int rgb)
{
    SST("[swiff]   setBackgroundColor %08X", rgb);   // [TEMP] tracer
    static_cast<SwiffRender*>(self)->bgColor = rgb;
}
// PE 0x10152BB0
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

// PE 0x10153B60. The background clear color (when the host says the movie has one), the
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
    const bool ok = static_cast<IRenderer*>(g_rendererBase)->beginScene(rgb, 1);
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

// PE 0x10154E60. Assign a per-texture technique index (into techRecord[]) by texture name. Only an
// absent name is inserted: find (0x10036410) compared with the map head [this+0x3DC] skips the
// operator[] (0x10155BC0) store when the name exists (0x10154E7F jnz 0x10154E94), so the first
// registration wins. The image answers the found tree node or the new value's address; std::map
// exposes no node, so the found arm answers its value's address (no reader is known).
void* RD9_THUNK sr_setTextureTechnique(void* self RD9_THUNK_EDX, const std::string* name, int index)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    std::map<std::string, int>::iterator it = s->texTechniqueMap.find(*name);
    if (it != s->texTechniqueMap.end())
        return &it->second;
    int& slot = s->texTechniqueMap[*name];
    slot = index;
    return &slot;
}

// PE 0x101533C0
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

// PE 0x101533F0. The loader with the cache skipped. Note the string-pointer argument.
int RD9_THUNK sr_uploadTexture(void* self RD9_THUNK_EDX, const std::string* name,
                                unsigned int* outW, unsigned int* outH)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    return registryGetOrLoad(s->registry, *name, outW, outH, 1);
}

// PE 0x10153430. Copies the device driver version; returns zeros when caps are absent.
unsigned int* RD9_THUNK sr_getDriverVersion(void* RD9_THUNK_EDX, unsigned int* out)
{
    void* caps = ::dice::hfe::dx9rend::g_deviceCaps;
    if (caps)
        ::dice::hfe::dx9rend::deviceCapsCopyDriverVersion(out);
    else
    {
        // [CHOSEN, not recovered] PE 0x1015345C copies uninitialized stack
        // words when caps are missing; preserve the existing safe zero fill.
        out[0] = 0;
        out[1] = 0;
    }
    return out;
}

char RD9_THUNK sr_isNvidia(void* RD9_THUNK_EDX)      // adapter.VendorId == 0x10DE (4318)
{
    void* caps = ::dice::hfe::dx9rend::g_deviceCaps;
    if (!caps)
        return 0;
    return ::dice::hfe::dx9rend::deviceCapsIsNvidia() ? 1 : 0;
}

// byte_1023D950 is one retail byte shared with EffectManager::compileEffect's successful-technique
// store (PE 0x100357B9); both sides use the definition in RendDX9/Ra/EffectManager.cpp.
// getter at 0x10153490, a tail inside the ledger range of PE 0x10153430
char RD9_THUNK sr_getFlag(void* RD9_THUNK_EDX) { return (char)::dice::hfe::dx9rend::Ra::g_anyEffectCompiled; }
// PE 0x101534A0
char RD9_THUNK sr_setFlag(void* RD9_THUNK_EDX, char v) { ::dice::hfe::dx9rend::Ra::g_anyEffectCompiled = (unsigned char)v; return v; }

int RD9_THUNK sr_creditsCount(void* self RD9_THUNK_EDX);     // slot 62, forward-declared for 61

// PE 0x10154EA0. Preload images/credits/1..113.png and remember each name that resolved. The
// count query goes through primary slot +0xF8 (62) like the shipped body.
char RD9_THUNK sr_preloadCredits(void* self RD9_THUNK_EDX)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (reinterpret_cast<ui::ISwiffRender*>(self)->creditsCount())
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

// PE 0x10153C70. Copy the i-th credits name out; empty string when out of range.
std::string* RD9_THUNK sr_getCreditsName(void* self RD9_THUNK_EDX, std::string* out, unsigned int index)
{
    SwiffRender* s = static_cast<SwiffRender*>(self);
    if (!s->credits.empty()
        && index < (unsigned int)reinterpret_cast<ui::ISwiffRender*>(self)->creditsCount())
        new (out) std::string(s->credits[index]);
    else
        new (out) std::string("");
    return out;
}

// byte_10267D94 - shared between the seeder and the range picker, as shipped.
bool g_swiffRandSeeded = false;

void swiffSeedRandom()      // sub_101C8240 - init()'s tail
{
    bf2crt::srand((unsigned int)::time(0));
    g_swiffRandSeeded = true;
}

// sub_101C8310 - the srand(time)-seeded uniform pick, exactly the shipped arithmetic.
int swiffRandomRange(int lo, int hi)
{
    if (!g_swiffRandSeeded)
        swiffSeedRandom();
    const float range = (float)(hi - lo);
    return lo - (int)((double)bf2crt::rand() * range * -0.000030518509);
}

// The next rand() value scaled by 0x38000100 (about 1 / 32767), seeding first as swiffRandomRange does.
// PE 0x101C8260 (the value is returned on the x87 stack). Unreachable in 1.50: no code or data
// references. [CHOSEN, not recovered] float return type: the body ends with `fild; fmul` and nothing
// reads st0.
float swiffRandomUnit()
{
    if (!g_swiffRandSeeded)
        swiffSeedRandom();
    const int value = bf2crt::rand();
    return static_cast<float>(static_cast<double>(value) * 3.0518509447574615e-05);
}

// True when probability is above 1e-05 and a fresh unit random value does not exceed it
// (`fcomp` with C0 or C3 set). PE 0x101C82A0 (retn 4). Unreachable in 1.50: no code or data
// references.
bool swiffRandomChance(float probability)
{
    if (!(probability > 1e-05f))
        return false;
    if (!g_swiffRandSeeded)
        swiffSeedRandom();
    const int value = bf2crt::rand();
    return static_cast<float>(static_cast<double>(value) * 3.0518509447574615e-05) <= probability;
}

// A float in [lo, hi]: lo + (rand * (hi - lo)) * 0x38000100. PE 0x101C8380 (retn 8, lo first).
// Unreachable in 1.50: no code or data references.
float swiffRandomFloatRange(float lo, float hi)
{
    const float range = hi - lo;
    if (!g_swiffRandSeeded)
        swiffSeedRandom();
    const int value = bf2crt::rand();
    return static_cast<float>((static_cast<double>(value) * range) * 3.0518509447574615e-05)
         + lo;
}

// PE 0x101C83D0 (ecx = out, edx = count): fills count bytes with rand() * 0x3BFF01FE (255 / 32767)
// truncated through the CRT's _ftol (0x101C45A4, a compiler/CRT helper), seeding before the first
// byte only; always answers true. Unreachable in 1.50: no code or data references.
bool swiffRandomBytes(unsigned char* out, int count)
{
    for (; count > 0; --count)
    {
        if (!g_swiffRandSeeded)
            swiffSeedRandom();
        const int value = bf2crt::rand();
        *out++ = static_cast<unsigned char>(static_cast<int>(
            static_cast<float>(static_cast<double>(value) * 0.007782219909131527)));
    }
    return true;
}

// PE 0x10153B10
std::string* RD9_THUNK sr_getRandomCreditsName(void* self RD9_THUNK_EDX, std::string* out)
{
    const int count = sr_creditsCount(self RD9_THUNK_ARG);
    const int pick  = swiffRandomRange(0, count - 1);
    // slot 63, virtually - as the shipped body dispatches through its own vtable
    void** vtbl = static_cast<SwiffRender*>(self)->vtPrimary;
#if !defined(_MSC_VER)
    // [CHOSEN, not recovered] Let Clang supply the native hidden return position.
    typedef std::string (*GetNameFn)(void*, unsigned int);
    new (out) std::string(reinterpret_cast<GetNameFn>(vtbl[63])(self, (unsigned int)pick));
    return out;
#else
    typedef std::string* (__thiscall* GetNameFn)(void*, std::string*, unsigned int);
    return reinterpret_cast<GetNameFn>(vtbl[63])(self, out, (unsigned int)pick);
#endif
}

char RD9_THUNK sr_getWasDeviceLost(void* self RD9_THUNK_EDX)
{
    return (char)static_cast<SwiffRender*>(self)->wasLostSticky;
}
void RD9_THUNK sr_clearWasDeviceLost(void* self RD9_THUNK_EDX)
{
    static_cast<SwiffRender*>(self)->wasLostSticky = 0;
}


// PE 0x101B9290 (this = the rain). Builds one font per SwiffHost credits-list entry and answers true.
// The retail body reads the list without a null test; the guard below is a [CHOSEN, not recovered]
// memory-safety omission (a null list would fault at PE 0x101B9312).
bool creditsRainBuildFonts(CreditsRain* rain)
{
    ui::ISwiffHost* host = static_cast<ui::ISwiffHost*>(
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffHost")));
    SwiffRender* renderer = static_cast<SwiffRender*>(static_cast<void*>(
        ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffRender"))));
    std::vector<::dice::hfe::SwiffStringIntEntry>* const list = host->getEntriesEC8();
    if (list)
    {
        for (unsigned int i = 0; i < list->size(); ++i)
        {
            std::wstring localized;
            const std::string key((*list)[i].text);
            const int type = (*list)[i].value;
            if (type && (*list)[i].text != "")
                host->getLocalizedString(localized, key);
            void** vtbl = renderer->vtPrimary;
            typedef void* (__thiscall* CreateFontFn)(void*, const char*, int);
            void* font = 0;
            if (type == 1)
                font = reinterpret_cast<CreateFontFn>(vtbl[26])(renderer, "StandardTextBold", 13);
            else if (type == 2)
                font = reinterpret_cast<CreateFontFn>(vtbl[26])(renderer, "StandardTextBold", 15);
            else
                font = reinterpret_cast<CreateFontFn>(vtbl[26])(renderer, "DynamicText", 11);
            if (!font)
                continue;       // existing RW-80 omission of the null-wrapper access
            swiffFontConfigure(font, 620.0f, 2);
            typedef int (__thiscall* SetTextFn)(void*, void*, const wchar_t*);
            if (type)
                reinterpret_cast<SetTextFn>(vtbl[28])(renderer, font, localized.c_str());
            else
            {
                const std::wstring text = makeWide((*list)[i].text);
                reinterpret_cast<SetTextFn>(vtbl[28])(renderer, font, text.c_str());
            }
            typedef int (__thiscall* TrackFontFn)(void*, void*);
            reinterpret_cast<TrackFontFn>(vtbl[29])(renderer, font);
            swiffFontRefreshWindow(font);
            creditsRainPush(rain, font);
            ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffRender"));
        }
    }
    return true;
}


// PE 0x10152200. Singletons, the effect + six parameters, eight technique records, the vertex
// declaration, three ring VBs under a pool swap, the listener/event registrations, the rain.
// ClassManager must be initialized; FontRenderer is used without a null test.

int RD9_THUNK sr_init(void* self RD9_THUNK_EDX)
{
    using namespace ::dice::hfe::dx9rend;
    SwiffRender* s = static_cast<SwiffRender*>(self);

    s->fontRenderer = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("FontRenderer"));
    s->swiffHost    = ::dice::hfe::classManager->getSingleton(renddx9abi::HostString("SwiffHost"));

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

    // 0x1015255A: the FontRenderer builds its effect, declarations and FONT_VBS ring (sub_100A1240).
    fontRendererInit(s->fontRenderer);

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
        rain = creditsRainConstruct(rainStorage);                    // PE 0x101B9200
    }
    s->trail = rain;

    if (rain && s->swiffHost)
        creditsRainBuildFonts(rain);                                 // PE 0x101B9290

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
        // PE 0x101B8F00. Release every rain font through the current singleton's
        // slot 27, then destroy its vector (0x101B9240) and free the scroller.
        CreditsRain* r = s->trail;
        creditsRainReleaseFonts(r);
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

    // 0x101541C4: the FontRenderer releases its resources (sub_100A18A0).
    fontRendererTeardown(s->fontRenderer);
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

// PE 0x10152640. Marks device loss and unlocks the ring buffer before the lower
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

// PE 0x10152680. Rebuilds cached techniques and resets menu modifier state after device reset.
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

// PE 0x10153410. The ECCore handler: event (11,3) disables font rendering. The two unread arguments are
// IEventListener::handleEvent's node and data pointers.
void RD9_THUNK sr_handleECCoreEvent(void* sub RD9_THUNK_EDX, int category, int id, void*, void*)
{
    SwiffRender* s = ownerFromEcCore(sub);
    if (category == 11 && id == 3)
        s->fontsDisabled = 1;
}


// The vtables - primary 0x1021E1C0 (67), devlost 0x1021E1B4 (3), eccore 0x1021E1B0 (1).

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
void* const kSwiffRenderVtablePrimary[] =
{
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, addRef, sr_addRef),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getRef, sr_getRef),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, release, sr_release),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, queryInterface, sr_queryInterfaceById),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, init, sr_init),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, shutdown, sr_shutdown),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, slot6, sr_slot6),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, uploadAlpha, sr_uploadAlpha),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, slot8, sr_slot8),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, uploadRgba, sr_uploadRgba),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, loadTexture, sr_loadTexture),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, releaseTexture, sr_releaseTexture),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, beginDisplay, sr_beginDisplay),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, endDisplay, sr_endDisplay),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setTransform, sr_setTransform),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setCxform, sr_setCxform),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, recordStat, sr_recordStat),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, drawVertices, sr_drawVertices),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, drawLineStrip, sr_drawLineStrip),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fillStyleDisable, sr_fillStyleDisable),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fillStyleColor, sr_fillStyleColor),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setTexture, sr_setTexture),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, lineStyleDisable, sr_lineStyleDisable),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, lineStyleColor, sr_lineStyleColor),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, lineStyleWidth, sr_lineStyleWidth),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, nullsub25, sr_nullsub25),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, createFont, sr_createFont),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, releaseFontHandle, sr_releaseFontHandle),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontForward16, sr_fontForward16),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, destroyFont, sr_destroyFont),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, drawGlyphBitmap, sr_drawGlyphBitmap),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontGetExtent, sr_fontGetExtent),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontForward32, sr_fontForward32),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontForward40, sr_fontForward40),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontForward44, sr_fontForward44),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontForward48, sr_fontForward48),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontSetSize, sr_fontSetSize),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, fontSetField104, sr_fontSetField104),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, nullsub38, sr_nullsub38),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, beginMask, sr_beginMask),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, endMask, sr_endMask),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, disableMask, sr_disableMask),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setMovieWidth, sr_setMovieWidth),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setMovieHeight, sr_setMovieHeight),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setScale, sr_setScale),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setBackgroundColor, sr_setBackgroundColor),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setMouseState, sr_setMouseState),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, slot47, sr_slot47),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, nullsub48, sr_nullsub48),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, slot49, sr_slot49),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, slot50, sr_slot50),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, beginFrame, sr_beginFrame),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, endFrame, sr_endFrame),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setTextureTechnique, sr_setTextureTechnique),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, updateTime, sr_updateTime),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getTextureRegistry, sr_getTextureRegistry),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, uploadTexture, sr_uploadTexture),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getDriverVersion, sr_getDriverVersion),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, isNvidia, sr_isNvidia),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getFlag, sr_getFlag),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, setFlag, sr_setFlag),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, preloadCredits, sr_preloadCredits),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, creditsCount, sr_creditsCount),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getCreditsName, sr_getCreditsName),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getRandomCreditsName, sr_getRandomCreditsName),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, getWasDeviceLost, sr_getWasDeviceLost),
    BF2_NATIVE_SLOT(::dice::hfe::ui::ISwiffRender, clearWasDeviceLost, sr_clearWasDeviceLost),
};
#else
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
#endif

void* const kSwiffRenderVtableDevLost[3] =
{
    reinterpret_cast<void*>(&sr_onDeviceLost),         // +0x00
    reinterpret_cast<void*>(&sr_onDeviceReset),        // +0x04
    reinterpret_cast<void*>(&sr_onDeviceDestroy),      // +0x08  (its own `ret`)
};

#if !defined(_MSC_VER)
// [CHOSEN, not recovered] Match the shared consumer interface under the native ABI.
void* const kSwiffRenderVtableEcCore[] =
{
    BF2_NATIVE_SLOT(::dice::hfe::IEventListener, handleEvent, sr_handleECCoreEvent),
};
#else
void* const kSwiffRenderVtableEcCore[1] =
{
    reinterpret_cast<void*>(&sr_handleECCoreEvent),    // +0x00
};
#endif


// The constructor - PE 0x10155060. Reproduces exactly the shipped stores; fields it leaves alone stay
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
    s->currentVideo = 0;
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
                                               // init(). The +904 string vector is zeroed by the
                                               // shipped ctor too (PE 0x101551A9..0x101551B5:
                                               // +0x388/+0x38C/+0x390); the native member is
                                               // constructed empty, which is the same state.
    s->stats[0] = 0; s->stats[1] = 0; s->stats[2] = 0;
    s->stats[3] = 0; s->stats[4] = 0; s->stats[5] = 0;
    s->lastVerts = 0;
    s->lastCount = 0;
    // [CHOSEN, not recovered] The stores of vpW, vpH, stats[1..5], lastVerts, lastCount and timeMs above
    // have no counterpart in PE 0x10155060 (it leaves them to the allocator); they are zeroed so that no
    // native path reads an uninitialised word (memory-safety omission).
    return s;
}

// The destructor tail - PE 0x10154F60 (after shutdown): the credits vectors, the two maps, the fonts
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
