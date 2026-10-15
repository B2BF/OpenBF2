// Menu texture loading, transformed quad drawing and canvas editing.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../RendDX9/RendDX9Base.h"                // the TextureManager + atlas-cache seams
#include "../../RendDX9/Texture.h"                    // EngineTexture, textureDims, textureAddRef
#include "../../RendDX9/TextureAtlas/TextureAtlas.h"  // TextureAtlasEntry

#include <stddef.h>
#include <new>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {
namespace geom {

// IMemeTexture::CanvasPointList (GeomMisc/MenuRend/IMemeTexture.h): the by-value point list of
// slots +0x68 / +0x70. Spelled out here because that header brings the engine's IBase.
typedef std::vector<std::pair<float, float> > MemeCanvasPointList;

// From ModuleGeomMisc's getClassList (0x1007DD9D) and BF2's own call site.
extern const unsigned int CID_MemeTexture  = 81032u;
extern const unsigned int IID_IMemeTexture = 80032u;

// dword_10247C38 (ModuleGeomMisc.cpp) and the QuadDrawer's batch submit (MenuRend/QuadDrawer.cpp).
extern IBase* g_QuadDrawer;
void quadDrawerSubmit(void* drawer, void* memeTexture, void* request,
                      float u0, float v0, float u1, float v1);


// Ref-counted menu texture with cached dimensions and atlas UVs.
// The primary interface has 31 slots; the secondary interface listens for device resets.
// [UNRESOLVED] The word at +0x0C is not initialized by the constructor.

class MemeTexture
{
public:
    MemeTexture();
    void  dtor();                 // Meme_Texture_dtor, 0x100892D0

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    void  setTexture(const char* name, const char* prefix, int flags);
    bool  createTexture(unsigned int w, unsigned int h);             // +0x14
    bool  drawUV(void* request, float u0, float v0,
                 float u1, float v1);                                // +0x18
    bool  drawOther(MemeTexture* other, void* request);              // +0x1C
    void  releaseTexture();                                          // +0x5C
    void  unload();                                                  // +0x60
    void  plotOutline(float* first, float* last, unsigned char color);   // +0x68  0x10089340 (core)
    void  fillCanvas(unsigned char color);                           // +0x6C
    void  beginShape(unsigned char clearValue);                      // +0x74
    void  applyCanvas(short mode);                                   // +0x78
    void  onDeviceLost();                                            // listener +0x00  0x10088920
    void  onDeviceReset();                                           // listener +0x04  0x10088940
    // The listener thunks receive the subobject (shipped this + 4).
    static MemeTexture* fromListener(void* sub)
    {
        return reinterpret_cast<MemeTexture*>(static_cast<char*>(sub)
                                              - offsetof(MemeTexture, m_listenerVptr));
    }

    // The trivial accessor pairs, one line each - see the vtable for their shipped addresses.
    void*         engineTexture() const  { return m_texture; }                   // not a slot
    const char*   getName() const        { return m_name.c_str(); }              // +0x20
    void          setUserObject(void* p) { m_userObject = p; }                   // +0x24
    void*         getUserObject() const  { return m_userObject; }                // +0x28
    float         getUvWidthCopy() const { return m_uvWidthCopy; }               // +0x2C
    float         getUvHeightCopy() const{ return m_uvHeightCopy; }              // +0x30
    int           getPixelWidth() const  { return m_pixelWidth; }                // +0x34
    int           getPixelHeight() const { return m_pixelHeight; }               // +0x38
    int           getPageWidth() const   { return m_pageWidth; }                 // +0x3C
    int           getPageHeight() const  { return m_pageHeight; }                // +0x40
    unsigned char setByte66(unsigned char v) { m_byte66 = v; return v; }         // +0x44
    unsigned char getByte66() const      { return m_byte66; }                    // +0x48
    float         getUOffset() const     { return m_uOffset; }                   // +0x4C
    float         getVOffset() const     { return m_vOffset; }                   // +0x50
    float         getUvWidth() const     { return m_uvWidth; }                   // +0x54
    float         getUvHeight() const    { return m_uvHeight; }                  // +0x58

private:
    void*         m_vptr;         // +0x00
    void*         m_listenerVptr; // +0x04
    int           m_refCount;     // +0x08
    unsigned int  m_dword0C;      // +0x0C  [UNRESOLVED]
    std::string  m_string10;     // +0x10
    void*         m_texture;      // +0x2C
    std::string  m_name;         // +0x30  setTexture assigns this
    float         m_uvWidthCopy;  // +0x4C
    float         m_uvHeightCopy; // +0x50
    int           m_pixelWidth;   // +0x54
    int           m_pixelHeight;  // +0x58
    int           m_pageWidth;    // +0x5C
    int           m_pageHeight;   // +0x60
    unsigned char m_byte64;       // +0x64
    unsigned char m_byte65;       // +0x65  ctor: 1
    unsigned char m_byte66;       // +0x66  ctor: 0 - the +0x44/+0x48 accessor pair's byte
    unsigned char m_byte67;       // +0x67
    float         m_uOffset;      // +0x68
    float         m_vOffset;      // +0x6C
    float         m_uvWidth;      // +0x70
    float         m_uvHeight;     // +0x74
    // +0x78 - resolved by the destructor: a refcounted object handed over via slot +0x24 (stored
    // without an addRef - ownership transfers) and released through its vslot +0x08 at teardown.
    // What BF2's menu passes is host-side and unread; the store/release contract is what matters.
    void*         m_userObject;   // +0x78  ctor: 0
    void*         m_atlasCache;   // +0x7C  ctor: 0
    // +0x80/+0x84 - resolved by beginShape/applyCanvas: two pageW*pageH byte buffers.
    unsigned char* m_savedAlpha;  // +0x80  ctor: 0 - the texture's alpha, captured once
    unsigned char* m_canvas;      // +0x84  ctor: 0 - the CPU drawing canvas the plot/fill write
};



namespace {

void RD9_THUNK meme_addRef(void* s RD9_THUNK_EDX) { static_cast<MemeTexture*>(s)->addRef(); }
int  RD9_THUNK meme_getRef(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getRef(); }
int  RD9_THUNK meme_release(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->release(); }

void* RD9_THUNK meme_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<MemeTexture*>(s)->queryInterface(iid);
}

// +0x10, sub_10088EA0. the one BF2 calls.
void RD9_THUNK meme_setTexture(void* s RD9_THUNK_EDX, const char* name, const char* prefix, int flags)
{
    RD9_TRACE_AT("dice::hfe::geom::Meme::Texture::setTexture", s, 0x10);
    static_cast<MemeTexture*>(s)->setTexture(name, prefix, flags);
}


// Float-returning adapters use the x86 FPU return convention expected by callers.


bool RD9_THUNK meme_s14(void* s RD9_THUNK_EDX, unsigned int w, unsigned int h)
{
    RD9_TRACE_AT("dice::hfe::geom::Meme::Texture::createTexture", s, 0x14);
    return static_cast<MemeTexture*>(s)->createTexture(w, h);
}

bool RD9_THUNK meme_s18(void* s RD9_THUNK_EDX, void* request, float u0, float v0, float u1, float v1)
{
    return static_cast<MemeTexture*>(s)->drawUV(request, u0, v0, u1, v1);
}

bool RD9_THUNK meme_s1C(void* s RD9_THUNK_EDX, void* other, void* request)
{
    return static_cast<MemeTexture*>(s)->drawOther(static_cast<MemeTexture*>(other), request);
}

const char*   RD9_THUNK meme_s20(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getName(); }
void          RD9_THUNK meme_s24(void* s RD9_THUNK_EDX, void* p) { static_cast<MemeTexture*>(s)->setUserObject(p); }
void*         RD9_THUNK meme_s28(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getUserObject(); }
float         RD9_THUNK meme_s2C(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getUvWidthCopy(); }
float         RD9_THUNK meme_s30(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getUvHeightCopy(); }
int           RD9_THUNK meme_s34(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getPixelWidth(); }
int           RD9_THUNK meme_s38(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getPixelHeight(); }
int           RD9_THUNK meme_s3C(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getPageWidth(); }
int           RD9_THUNK meme_s40(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getPageHeight(); }
unsigned char RD9_THUNK meme_s44(void* s RD9_THUNK_EDX, unsigned char v) { return static_cast<MemeTexture*>(s)->setByte66(v); }
unsigned char RD9_THUNK meme_s48(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getByte66(); }
float         RD9_THUNK meme_s4C(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getUOffset(); }
float         RD9_THUNK meme_s50(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getVOffset(); }
float         RD9_THUNK meme_s54(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getUvWidth(); }
float         RD9_THUNK meme_s58(void* s RD9_THUNK_EDX) { return static_cast<MemeTexture*>(s)->getUvHeight(); }
void          RD9_THUNK meme_s5C(void* s RD9_THUNK_EDX) { static_cast<MemeTexture*>(s)->releaseTexture(); }
void          RD9_THUNK meme_s60(void* s RD9_THUNK_EDX) { static_cast<MemeTexture*>(s)->unload(); }

// Reports the original unsupported-operation diagnostic and returns a truthy value.
int RD9_THUNK meme_s64(void* s RD9_THUNK_EDX)
{
    RD9_TRACE_AT("dice::hfe::geom::Meme::Texture::slot_0x64", s, 0x64);
    ::renddx9abi::logf("MemeTexture.cpp:291: hmm");
    return 1;
}

// +0x68 / +0x70 take a vector<pair<float,float>> by value plus a colour byte. The callee owns the
// vector and destroys it on return, which is where the shipped body freed the point buffer.
void RD9_THUNK meme_s68(void* s RD9_THUNK_EDX, MemeCanvasPointList points, unsigned char color)
{
    float* const first = points.empty() ? 0 : &points[0].first;
    static_cast<MemeTexture*>(s)->plotOutline(first, first + 2 * points.size(), color);
}

void RD9_THUNK meme_s6C(void* s RD9_THUNK_EDX, unsigned char color)
{
    static_cast<MemeTexture*>(s)->fillCanvas(color);
}

// +0x70, 0x10089480 - pass a copy of the caller's points to its OWN vslot +0x68 (which consumes
// the copy), run the fill (vslot +0x6C); the caller's list is destroyed on return.
void RD9_THUNK meme_s70(void* s RD9_THUNK_EDX, MemeCanvasPointList points, unsigned char color)
{
    MemeTexture* self = static_cast<MemeTexture*>(s);

    // the two virtual dispatches, exactly as the shipped body makes them
    void** vtbl = *reinterpret_cast<void***>(self);
    typedef void (__thiscall* PlotFn)(void*, MemeCanvasPointList, unsigned char);
    typedef void (__thiscall* FillFn)(void*, unsigned char);
    reinterpret_cast<PlotFn>(vtbl[0x68 / 4])(self, points, color);
    reinterpret_cast<FillFn>(vtbl[0x6C / 4])(self, color);
}

void RD9_THUNK meme_s74(void* s RD9_THUNK_EDX, unsigned char clearValue)
{
    static_cast<MemeTexture*>(s)->beginShape(clearValue);
}

void RD9_THUNK meme_s78(void* s RD9_THUNK_EDX, short mode)
{
    static_cast<MemeTexture*>(s)->applyCanvas(mode);
}

void* const kMemeTextureVtable[31] =
{
    reinterpret_cast<void*>(&meme_addRef),          // +0x00
    reinterpret_cast<void*>(&meme_getRef),          // +0x04
    reinterpret_cast<void*>(&meme_release),         // +0x08  0x10089640 - dtor + free at zero
    reinterpret_cast<void*>(&meme_queryInterface),  // +0x0C  0x100894C0  read
    reinterpret_cast<void*>(&meme_setTexture),      // +0x10  0x10088EA0  read
    reinterpret_cast<void*>(&meme_s14),             // +0x14  0x10088860  createTexture (RT, A8R8G8B8)
    reinterpret_cast<void*>(&meme_s18),             // +0x18  0x100889D0  drawUV -> the QuadDrawer
    reinterpret_cast<void*>(&meme_s1C),             // +0x1C  0x10088970  drawOther -> the QuadDrawer
    reinterpret_cast<void*>(&meme_s20),             // +0x20  0x100894F0  getName (m_name c_str)
    reinterpret_cast<void*>(&meme_s24),             // +0x24  0x10089500  setUserObject (+0x78)
    reinterpret_cast<void*>(&meme_s28),             // +0x28  0x10089510  getUserObject
    reinterpret_cast<void*>(&meme_s2C),             // +0x2C  0x10089520  getUvWidthCopy (+0x4C)
    reinterpret_cast<void*>(&meme_s30),             // +0x30  0x10089530  getUvHeightCopy (+0x50)
    reinterpret_cast<void*>(&meme_s34),             // +0x34  0x10089540  getPixelWidth (+0x54)
    reinterpret_cast<void*>(&meme_s38),             // +0x38  0x101A8A30  getPixelHeight (+0x58, folded)
    reinterpret_cast<void*>(&meme_s3C),             // +0x3C  0x10089550  getPageWidth (+0x5C)
    reinterpret_cast<void*>(&meme_s40),             // +0x40  0x10177190  getPageHeight (+0x60, folded)
    reinterpret_cast<void*>(&meme_s44),             // +0x44  0x10089560  setByte66
    reinterpret_cast<void*>(&meme_s48),             // +0x48  0x10089570  getByte66
    reinterpret_cast<void*>(&meme_s4C),             // +0x4C  0x10089580  getUOffset (+0x68)
    reinterpret_cast<void*>(&meme_s50),             // +0x50  0x10089590  getVOffset (+0x6C)
    reinterpret_cast<void*>(&meme_s54),             // +0x54  0x10177260  getUvWidth (+0x70, folded)
    reinterpret_cast<void*>(&meme_s58),             // +0x58  0x10177250  getUvHeight (+0x74, folded)
    reinterpret_cast<void*>(&meme_s5C),             // +0x5C  0x10089280  releaseTexture
    reinterpret_cast<void*>(&meme_s60),             // +0x60  0x10089120  unload
    reinterpret_cast<void*>(&meme_s64),             // +0x64  0x10088A20  the "hmm" diagnostic, :291
    reinterpret_cast<void*>(&meme_s68),             // +0x68  0x10089340  plotOutline (canvas)
    reinterpret_cast<void*>(&meme_s6C),             // +0x6C  0x10088CC0  fillCanvas (scanline)
    reinterpret_cast<void*>(&meme_s70),             // +0x70  0x10089480  plotAndFill
    reinterpret_cast<void*>(&meme_s74),             // +0x74  0x10088D40  beginShape (alpha capture)
    reinterpret_cast<void*>(&meme_s78)              // +0x78  0x10088AA0  applyCanvas (7 pixel ops)
};

// Listener thunks receive the secondary subobject and adjust back to the texture owner.
void RD9_THUNK meme_onDeviceLost(void* s RD9_THUNK_EDX)
{
    MemeTexture::fromListener(s)->onDeviceLost();
}
void RD9_THUNK meme_onDeviceReset(void* s RD9_THUNK_EDX)
{
    MemeTexture::fromListener(s)->onDeviceReset();
}
void RD9_THUNK meme_listenerNullsub(void* RD9_THUNK_EDX) { /* nullsub_15 - empty in the original */ }

void* const kMemeTextureListenerVtable[3] =
{
    reinterpret_cast<void*>(&meme_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&meme_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&meme_listenerNullsub)  // +0x08
};

} // anonymous namespace

// Initializes texture state and registers the device-loss listener.
MemeTexture::MemeTexture()
    : m_vptr(const_cast<void**>(kMemeTextureVtable))
    , m_listenerVptr(const_cast<void**>(kMemeTextureListenerVtable))
    , m_refCount(1)
    , m_dword0C(0)
    , m_texture(0)
    , m_byte64(0)
    , m_byte65(1)
    , m_byte66(0)
    , m_byte67(0)
    , m_userObject(0)
    , m_atlasCache(0)
    , m_savedAlpha(0)
    , m_canvas(0)
{
#if defined(_M_IX86)
    static_assert(offsetof(MemeTexture, m_listenerVptr) == 4,
                  "both MemeTexture listener bodies are handed this + 4");
#endif
    m_uvWidthCopy = 0.0f; m_uvHeightCopy = 0.0f;
    m_pixelWidth  = 0;    m_pixelHeight  = 0;
    m_pageWidth   = 0;    m_pageHeight   = 0;
    m_uOffset     = 0.0f; m_vOffset      = 0.0f;
    m_uvWidth     = 0.0f; m_uvHeight     = 0.0f;

    // Register the device listener at priority 4; the renderer's listener registry must already exist.
    ::dice::hfe::dx9rend::registerDeviceLostResetListener(&m_listenerVptr, 4);
}

// Unregisters the listener, unloads textures, releases the user object, and frees canvas and string storage.
void MemeTexture::dtor()
{
    m_vptr         = const_cast<void**>(kMemeTextureVtable);
    m_listenerVptr = const_cast<void**>(kMemeTextureListenerVtable);

    // Before any resource goes: once off the list a device loss can no longer reach a texture
    // this destructor is about to drop. Unguarded, as shipped.
    ::dice::hfe::dx9rend::unregisterDeviceLostResetListener(&m_listenerVptr);

    unload();                                              // sub_10089120, reused by the dtor

    if (m_userObject)
    {
        // IBase::release, +0x08 - typed in both flavours (RW-266): the native Core/IBase.h and
        // the retail HfeAbi.h quartet both put it there.
        static_cast<IBase*>(m_userObject)->release();
        m_userObject = 0;                                  // the shipped dtor leaves the field; nulling is invisible here
    }
    if (m_savedAlpha)
    {
        ::renddx9abi::engineFree(m_savedAlpha);
        m_savedAlpha = 0;
    }
    if (m_canvas)
    {
        ::renddx9abi::engineFree(m_canvas);
        m_canvas = 0;
    }
    m_name.~basic_string();
    m_string10.~basic_string();
}

// Final release runs teardown before freeing storage.
int MemeTexture::release()
{
    const int remaining = --m_refCount;
    if (remaining == 0)
    {
        dtor();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// 0x100894C0, read in full: accepts iid 1 and iid 80032, nothing else.
void* MemeTexture::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IMemeTexture)
        return const_cast<MemeTexture*>(this);
    return 0;
}


// Ignores null or empty names, then tries the prefixed atlas before loading a standalone texture.
// Temporarily changes texture-manager options; flags is unused.

void MemeTexture::setTexture(const char* name, const char* prefix, int flags)
{
    (void)flags;   // present in the shipped signature, unread in the shipped body

    if (!name || !*name)
    {
        ::renddx9abi::logf("Meme::Texture::setTexture(\"\"): the name is empty, so the shipped body "
                           "does nothing - this is BF2's own call from sub_7AD030");
        return;
    }

    m_name = name;

    std::string full(prefix);          // the shipped ctor(PBD) - a null prefix would crash there too
    full += name;

    bool tryAtlas = true;               // v33

    // The shared cache. The shipped code addRefs it only when this object first stores it while
    // the global already existed; a freshly created cache is stored with its constructor's 1.
    const bool cacheExisted = ::dice::hfe::dx9rend::atlasCacheExists();
    void* cache = ::dice::hfe::dx9rend::atlasCacheEnsure();
    if (cacheExisted)
    {
        if (!m_atlasCache)
        {
            m_atlasCache = cache;
            ::dice::hfe::dx9rend::atlasCacheAddRef();
        }
    }
    else
    {
        m_atlasCache = cache;
    }

    if (!::dice::hfe::dx9rend::atlasCacheHasAtlases())
    {
        const int savedClass   = ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(1);
        const int savedManaged = ::dice::hfe::dx9rend::textureManagerSwapManaged(1);
        if (!::dice::hfe::dx9rend::atlasCacheLoadAtlases(std::string("Menu/Atlas/")))
            tryAtlas = false;
        ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(savedClass);
        ::dice::hfe::dx9rend::textureManagerSwapManaged(savedManaged);
    }

    if (tryAtlas)
    {
        std::string key;
        ::dice::hfe::dx9rend::pathStripExtension(key, full);   // sub_100753D0
        key += ".dds";                                          // sub_100019A0

        ::dice::hfe::dx9rend::TextureAtlasEntry* rec = ::dice::hfe::dx9rend::atlasCacheFind(key);
        if (rec)
        {
            ::dice::hfe::dx9rend::EngineTexture* page = rec->page;
            m_texture = page;
            ::dice::hfe::dx9rend::textureAddRef(page);          // vslot +0x08 on the page

            m_uOffset  = rec->uOffset;                          // +0x68 = rec[2]
            m_vOffset  = rec->vOffset;                          // +0x6C = rec[3]
            m_uvWidth  = rec->width;                            // +0x70 = rec[0]
            m_uvHeight = rec->height;                           // +0x74 = rec[1]

            const unsigned int* dims = ::dice::hfe::dx9rend::textureDims(page);
            m_pageWidth  = static_cast<int>(dims[0]);           // +0x5C, raw dword copy
            m_pageHeight = static_cast<int>(dims[1]);           // +0x60

            m_uvHeightCopy = m_uvHeight;                        // +0x50 = the +0x74 value
            m_pixelWidth   = static_cast<int>(static_cast<double>(m_pageWidth) * m_uvWidth);
            m_uvWidthCopy  = m_uvWidth;                         // +0x4C = the +0x70 value
            m_pixelHeight  = static_cast<int>(static_cast<double>(m_pageHeight) * m_uvHeight);

            if (rec->page)                                      // the shipped `if (v11[4]) return`
                return;
        }
    }

    // The fallback: the by-name loader, with both knobs forced to 1 for the duration.
    const int savedClass   = ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(1);
    const int savedManaged = ::dice::hfe::dx9rend::textureManagerSwapManaged(1);
    m_texture = ::dice::hfe::dx9rend::textureManagerGetTexture(full);
    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(savedClass);
    ::dice::hfe::dx9rend::textureManagerSwapManaged(savedManaged);

    const unsigned int* dims = ::dice::hfe::dx9rend::textureDims(
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture));
    m_pixelWidth  = static_cast<int>(dims[0]);                  // +0x54 and +0x5C both get W
    m_pageWidth   = static_cast<int>(dims[0]);
    m_pixelHeight = static_cast<int>(dims[1]);                  // +0x58 and +0x60 both get H
    m_pageHeight  = static_cast<int>(dims[1]);
    m_vOffset      = 0.0f;                                      // the shipped store order
    m_uOffset      = 0.0f;
    m_uvHeight     = 1.0f;
    m_uvWidth      = 1.0f;
    m_uvWidthCopy  = 1.0f;
    m_uvHeightCopy = 1.0f;

    ::renddx9abi::logf("Meme::Texture::setTexture(\"%s%s\"): loader path -> 0x%p %dx%d",
                       prefix, name, m_texture, m_pixelWidth, m_pixelHeight);
}


// Creates an argb render target configured for recreation after device reset.
// Returns whether creation succeeded; existing UV offsets and extents remain unchanged.

bool MemeTexture::createTexture(unsigned int w, unsigned int h)
{
    const int savedClass    = ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(1);
    const int savedManaged  = ::dice::hfe::dx9rend::textureManagerSwapManaged(0);
    const int savedRecreate = ::dice::hfe::dx9rend::textureManagerSwapRecreateMode(1);

    m_texture = ::dice::hfe::dx9rend::textureManagerCreateEmpty(w, h, 21, 0, 1, 0);

    ::dice::hfe::dx9rend::textureManagerSwapManaged(savedManaged);       // the shipped restore order:
    ::dice::hfe::dx9rend::textureManagerSwapRecreateMode(savedRecreate); // E20, E40, E00
    ::dice::hfe::dx9rend::textureManagerSwapDefaultMipClass(savedClass);

    if (m_texture)
    {
        const unsigned int* dims = ::dice::hfe::dx9rend::textureDims(
            static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture));
        m_pixelWidth   = static_cast<int>(dims[0]);
        m_pageWidth    = static_cast<int>(dims[0]);
        m_pixelHeight  = static_cast<int>(dims[1]);
        m_pageHeight   = static_cast<int>(dims[1]);
        m_uvWidthCopy  = 1.0f;
        m_uvHeightCopy = 1.0f;
        ::renddx9abi::logf("Meme::Texture::createTexture(%ux%u): render target -> 0x%p",
                           w, h, m_texture);
        return true;
    }
    return false;
}


// Submits transformed textured corners and UV coordinates through QuadDrawer.

bool MemeTexture::drawUV(void* request, float u0, float v0, float u1, float v1)
{
    if (!g_QuadDrawer)
    {
        ::renddx9abi::logf("Meme::Texture::drawUV: g_QuadDrawer is null (registration failed?) - "
                           "the shipped call would crash; dropping the quad");
        return true;
    }
    quadDrawerSubmit(g_QuadDrawer, this, request,
                     u0 * m_uvWidth + m_uOffset, v0 * m_uvHeight + m_vOffset,
                     u1 * m_uvWidth + m_uOffset, v1 * m_uvHeight + m_vOffset);
    return true;
}

bool MemeTexture::drawOther(MemeTexture* other, void* request)
{
    if (!g_QuadDrawer)
    {
        ::renddx9abi::logf("Meme::Texture::drawOther: g_QuadDrawer is null (registration failed?) "
                           "- the shipped call would crash; dropping the quad");
        return true;
    }
    if (other)
        quadDrawerSubmit(g_QuadDrawer, other, request,
                         m_uOffset, m_vOffset,
                         other->m_uvWidthCopy + m_uOffset,
                         other->m_uvHeightCopy + m_vOffset);
    else
        quadDrawerSubmit(g_QuadDrawer, 0, request, 0.0f, 0.0f, 0.0f, 0.0f);
    return true;
}


// Drops this texture reference and evicts surviving textures from the manager cache; destroys unreferenced textures.

void MemeTexture::releaseTexture()
{
    if (!m_texture)
        return;
    ::dice::hfe::dx9rend::EngineTexture* tex =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture);
    if (--tex->refCount)
    {
        ::dice::hfe::dx9rend::textureManagerEvictFromCache(tex);
    }
    else
    {
        ::dice::hfe::dx9rend::textureDtor(tex);
        ::renddx9abi::engineFree(tex);
    }
    m_texture = 0;
}


// Releases the texture and atlas cache. Surviving level-local textures are evicted.
// With no level loaded, the empty prefix matches every texture, so all unloads evict.

void MemeTexture::unload()
{
    if (m_texture)
    {
        ::dice::hfe::dx9rend::EngineTexture* tex =
            static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture);

        std::string level, levelDir;
        ::dice::hfe::dx9rend::readStringSetting("GSLoadLevel", level);
        ::dice::hfe::dx9rend::readStringSetting("GSLevelDirectory", levelDir);
        std::string prefix(levelDir);
        prefix += level;

        if (--tex->refCount)
        {
            if (::strstr(m_name.c_str(), prefix.c_str()))
                ::dice::hfe::dx9rend::textureManagerEvictFromCache(tex);
        }
        else
        {
            ::dice::hfe::dx9rend::textureDtor(tex);
            ::renddx9abi::engineFree(tex);
        }
        m_texture = 0;
    }

    if (m_atlasCache)
    {
        ::dice::hfe::dx9rend::atlasCacheRelease();         // vslot +0x08; nulls the global at zero
        m_atlasCache = 0;
    }
}


// Plots normalized point pairs with even-odd pixel toggles. The points belong to slot +0x68's
// by-value vector, which frees them on return (the shipped core freed the buffer here).
// The horizontal step is computed before the empty-span check, preserving division by zero.

void MemeTexture::plotOutline(float* first, float* last, unsigned char color)
{
    float* p = first;
    if (p && p + 2 != last && m_canvas)
    {
        float prevX = p[0];
        float prevY = p[1];
        const float w = static_cast<float>(m_pageWidth);
        const float h = static_cast<float>(m_pageHeight);

        for (float* cur = p + 2; cur != last; cur += 2)
        {
            const float x0f = prevX;
            const float y0f = prevY;
            prevX = cur[0];
            prevY = cur[1];

            float xTop    = w * x0f;
            float xBottom = w * prevX;
            int   yTop    = static_cast<int>(h * y0f);
            int   yBottom = static_cast<int>(h * prevY);
            if (yBottom < yTop)                            // the shipped endpoint swap
            {
                const int   ty = yTop;    yTop = yBottom;    yBottom = ty;
                const float tx = xTop;    xTop = xBottom;    xBottom = tx;
            }

            int rows = yBottom - yTop;
            const float dx = (xBottom - xTop) / static_cast<float>(yBottom - yTop);
            if (yTop < yBottom)
            {
                int rowBase = m_pageWidth * yTop;
                float x = xTop;
                do
                {
                    unsigned char* px = m_canvas + rowBase + static_cast<int>(x);
                    rowBase += m_pageWidth;
                    x += dx;
                    --rows;
                    *px = (*px != 0) ? 0 : color;
                }
                while (rows);
            }
        }
    }
}


// Fills an outlined canvas using even-odd scanlines; the final column is deliberately skipped.

void MemeTexture::fillCanvas(unsigned char color)
{
    if (m_pageHeight <= 0 || !m_canvas)
        return;

    int rowBase = 0;
    for (int rowsLeft = m_pageHeight; rowsLeft; --rowsLeft, rowBase += m_pageWidth)
    {
        int inside = 0;
        for (int col = 0; col < m_pageWidth - 1; ++col)
        {
            unsigned char* px = m_canvas + rowBase + col;
            if (*px)
            {
                if (!inside) { inside = 1; *px = color; }
                else         { inside = 0; }
            }
            else if (inside)
            {
                *px = color;
            }
        }
    }
}


// Clears the canvas at mip-zero dimensions and saves original alpha on first use.
// 16-bit formats use the upper alpha nibble; other formats use byte 3 of each 32-bit pixel.

void MemeTexture::beginShape(unsigned char clearValue)
{
    ::dice::hfe::dx9rend::EngineTexture* tex =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture);
    ::dice::hfe::dx9rend::TextureLockRecord* rec =
        ::dice::hfe::dx9rend::textureLock(tex, 0, 0x7FFFFFFF);

    const int n = static_cast<int>(rec->width * rec->height);

    if (!m_canvas)
        m_canvas = static_cast<unsigned char*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(n)));
    for (int i = 0; i < n; ++i)
        m_canvas[i] = clearValue;

    if (!m_savedAlpha)
    {
        m_savedAlpha = static_cast<unsigned char*>(::renddx9abi::engineAlloc(static_cast<unsigned int>(n)));
        if (rec->bpp == 16)
        {
            const unsigned short* px = static_cast<const unsigned short*>(rec->pBits);
            for (int i = 0; i < n; ++i)
                m_savedAlpha[i] = static_cast<unsigned char>(px[i] >> 12);
        }
        else
        {
            const unsigned char* px = static_cast<const unsigned char*>(rec->pBits);
            for (int i = 0; i < n; ++i)
                m_savedAlpha[i] = px[4 * i + 3];
        }
    }

    ::dice::hfe::dx9rend::textureUnlock(tex, rec);
}


// Combines source canvas pixels and saved alpha according to the operation flags; the last pixel is unchanged.
// [CHOSEN, not recovered] Omits the original out-of-bounds iteration.

void MemeTexture::applyCanvas(short mode)
{
    ::dice::hfe::dx9rend::EngineTexture* tex =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture);
    ::dice::hfe::dx9rend::TextureLockRecord* rec =
        ::dice::hfe::dx9rend::textureLock(tex, 0, 0x7FFFFFFF);

    unsigned char* pixels = static_cast<unsigned char*>(rec->pBits);
    const int n = static_cast<int>(rec->width * rec->height);

    const unsigned char* src = 0;
    if ((mode & 7) == 2)      src = m_canvas;
    else if ((mode & 7) == 3) src = m_savedAlpha;

    const unsigned int op = static_cast<unsigned int>(mode) & 0xF00;

    for (int k = n - 2; k >= 0; --k)                       // the shipped shifted walk, minus k = -1
    {
        unsigned int* px = reinterpret_cast<unsigned int*>(pixels) + k;
        switch (op)
        {
            case 0x100:                                    // alpha = src
                *px = (*px & 0xFFFFFF) | (static_cast<unsigned int>(src[k]) << 24);
                break;
            case 0x200:                                    // the shipped RGB multiply-mix, verbatim
            {
                const unsigned int t = *px;
                unsigned int acc = (src[k] * ((t >> 8) & 0xFFFF0000)) ^ t;
                acc &= 0xFFFFFF;
                acc ^= src[k] * ((t >> 8) & 0x00FF0000);
                *px = acc;
                break;
            }
            case 0x300:                                    // alpha &= ~src
                *px &= ~(static_cast<unsigned int>(src[k]) << 24);
                break;
            case 0x400:                                    // alpha |= src
                *px |= static_cast<unsigned int>(src[k]) << 24;
                break;
            case 0x500:                                    // alpha = 0
                pixels[4 * k + 3] = 0;
                break;
            case 0x600:                                    // alpha = 0xFF
                pixels[4 * k + 3] = 0xFF;
                break;
            case 0x700:                                    // the inverse-alpha mix, decompiler-verbatim
            {
                const unsigned int t = *px;
                const unsigned int a = (t >> 24) & 0xFF;
                *px = ((src[k] * (255 - a)) << 16)
                    ^ ((t ^ ((src[k] * (0xFFFFFFFFu - a)) << 16)) & 0xFFFFFF);
                break;
            }
            default:
                break;
        }
    }

    ::dice::hfe::dx9rend::textureUnlock(tex, rec);
}


// Releases default-pool GPU resources on device loss while retaining the texture wrapper.
// Reset recreates resources with the page dimensions; managed resources are unaffected.

void MemeTexture::onDeviceLost()
{
    ::dice::hfe::dx9rend::EngineTexture* tex =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture);
    if (tex && tex->pool == 0)
        ::dice::hfe::dx9rend::textureReleaseD3DResource(tex);
}

void MemeTexture::onDeviceReset()
{
    ::dice::hfe::dx9rend::EngineTexture* tex =
        static_cast<::dice::hfe::dx9rend::EngineTexture*>(m_texture);
    if (tex && tex->pool == 0)
        m_texture = ::dice::hfe::dx9rend::textureManagerCreateEmpty(
            static_cast<unsigned int>(m_pageWidth), static_cast<unsigned int>(m_pageHeight),
            21, 0, 1, tex);
}

// The wrapped EngineTexture (+0x2C) the QuadDrawer compares and binds by identity.
void* memeTextureEngineTexture(const MemeTexture* texture)
{
    return texture->engineTexture();
}

// RefFactory_geom_Meme_Texture, 0x1007C8B0.
IBase* __fastcall createMemeTexture(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(MemeTexture));
    if (!storage)
        return 0;

    MemeTexture* self = new (storage) MemeTexture();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
