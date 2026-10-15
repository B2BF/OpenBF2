// Batches menu quads, clips geometry and flushes on rendering-state changes.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../Core/EventManager.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"
#include "../../RendMisc/IRenderView.h"

#include <stddef.h>
#include <string.h>
#include <new>

namespace dice {
namespace hfe {
namespace dx9rend {
extern void* g_shaderManager;                       // dword_1023B97C
void* shaderManagerGetEffect(void*, const char*);   // sub_10022EF0
void  effectBeginTechniqueStates(void*, const std::string&);
void  effectEndTechniqueStates(void*, const std::string&);
class Effect;
void* effectGetShaderParameter(Effect*, const std::string&);   // sub_1001E420
int   bufferManagerSwapPool(int value);             // sub_100113B0 - BufferManager+0x08
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
// BufferManager.cpp - the shipped inline owner-side release: `if (!--wrapper[1]) { sub_10014510;
// engineFree }`, one seam. The destructor below makes it at 0x1008B23C..0x1008B259.
void  vertexBufferRelease(void* wrapper);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);          // sub_10014630
bool  vertexBufferUnlock(void* wrapper);             // sub_10014690

// Render-state and vertex-buffer operations used by the draw core.
void  stateManagerPush();                            // sub_100261A0 on dword_1023B984
void  stateManagerPop();                             // sub_100261B0
void  vertexBufferBindStream(void* wrapper, unsigned int stream);   // sub_100146C0
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);              // sub_10005870
IRenderView* rendererGetRenderView();                // Renderer::getRenderView (0x1000CAF0)
bool  effectSetTechniqueByName(void* effect, const std::string& name);
unsigned int effectBegin(void* effect);              // sub_1001D780
void  effectBeginPass(void* effect, unsigned int index);   // sub_1001DBB0
void  effectEnd(void* effect);                       // sub_1001D7E0
}
namespace geom {

class MemeTexture;   // MenuRend/MemeTexture.cpp - the virtual dims/uv getters the batcher calls
void* memeTextureEngineTexture(const MemeTexture* texture);   // its +0x2C EngineTexture

extern const unsigned int CID_QuadDrawer  = 81031u;  // dword_1020AEF0
extern const unsigned int IID_IQuadDrawer = 80031u;  // queryInterface, 0x1008CA80


// The draw request - the 0xBC-byte block BF2's menu hands to every submit (read from the fields
// sub_1008B480 touches). Corners are 8 dwords each - the same shape the clip scratch uses.

struct QuadCorner
{
    float x, y, z;          // +0x00  screen-space; submit subtracts the half-pixel 0.5 from x/y
    float r, g, b, a;       // +0x0C  colour as four floats
    float pad;              // +0x1C
};

struct QuadDrawRequest
{
    float*        clipRect;     // +0x00  {x, y, w, h} or null = no clipping
    unsigned char blendByte;    // +0x04  cached-state key
    unsigned char pad05[3];
    unsigned int  stateDw08;    // +0x08  cached-state key (ctor default 7)
    unsigned int  stateDw0C;    // +0x0C  (default 8)
    unsigned int  stateDw10;    // +0x10  (default -1)
    unsigned int  stateDw14;    // +0x14  (default -1)
    unsigned char stateByte18;  // +0x18  cached-state key
    unsigned char pad19[3];
    MemeTexture*  texture2;     // +0x1C  the second texture layer, or null
    unsigned char pad20[0x40 - 0x20];
    QuadCorner    corner[4];    // +0x40 / +0x60 / +0x80 / +0xA0
};

#if defined(_M_IX86)
static_assert(sizeof(QuadDrawRequest) == 0xC0, "the corners end at +0xC0; submit reads none beyond");
#else
// [PLATFORM] x64: the client's native dice::meme::Quad (External/BFMeme/Quad.h) - a pointer clip,
// an intptr_t texture2 and eight dwords before the corners.
static_assert(offsetof(QuadDrawRequest, texture2) == 0x20, "x64 Quad texture2 at +0x20");
static_assert(offsetof(QuadDrawRequest, corner) == 0x48, "x64 Quad corners at +0x48");
static_assert(sizeof(QuadDrawRequest) == 0xC8, "x64 Quad is 0xC8 bytes");
#endif

// One batch vertex - 44 bytes, 11 dwords, read from the immediate-mode append:
struct QuadVertex
{
    float x, y, z;          // +0x00
    float r, g, b, a;       // +0x0C
    float u1, v1;           // +0x1C  the submitting texture's uv
    float u2, v2;           // +0x24  the second layer's uv
};

static_assert(sizeof(QuadVertex) == 44, "the batch strides by 44 - see the ctor's zeroing loop");

// Batches menu quads by texture and blend/depth state into a dynamic vertex buffer.
// Preserve primary, device-listener and event-listener order and the fixed vertex capacity.
// Device loss/core events can suspend drawing; begin/end capture constrain flush timing.
class QuadDrawer
{
public:
    QuadDrawer();

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    void  flush();                                  // +0x10
    void  beginFrame();                             // +0x14
    void  resetState();                             // +0x18
    void  endFrame();                               // +0x1C
    void  submit(MemeTexture* tex, QuadDrawRequest* req,
                 float u0, float v0, float u1, float v1);   // sub_1008B480
    void  handleECCoreEvent(int category, unsigned int id, unsigned char* payload);
    void  onDeviceLost()  { m_suspended = 1; }
    void  onDeviceReset() { m_basePos = 0; m_suspended = 0; }
    void  dtor();

    // The listener thunks receive a subobject: the device listener (shipped this + 4) or the ECCore
    // listener (shipped this + 8).
    static QuadDrawer* fromDeviceListener(void* sub)
    {
        return reinterpret_cast<QuadDrawer*>(static_cast<char*>(sub)
                                             - offsetof(QuadDrawer, m_deviceLostVptr));
    }
    static QuadDrawer* fromECCoreListener(void* sub)
    {
        return reinterpret_cast<QuadDrawer*>(static_cast<char*>(sub)
                                             - offsetof(QuadDrawer, m_ecCoreVptr));
    }

private:
    void  cacheState(MemeTexture* tex, QuadDrawRequest* req);
    void  uploadStateParams();                      // sub_1008B290

    void*          m_vptr;
    void*          m_deviceLostVptr;
    void*          m_ecCoreVptr;
    int            m_refCount;
    unsigned char  m_registered;
    unsigned char  m_suspended;
    unsigned char  m_pad12[2];
    unsigned int   m_passIndex;
    void*          m_effect;
    void*          m_params[14];
    unsigned char  m_captureMode;
    unsigned char  m_pad55[3];
    unsigned int   m_basePos;
    unsigned char  m_enabled;
    unsigned char  m_pad5D[3];
    unsigned int   m_vertexCount;
    QuadVertex     m_vertices[24576];
    MemeTexture*   m_cachedTexture;
    MemeTexture*   m_cachedTexture2;
    unsigned char  m_cachedBlend;
    unsigned char  m_pad6D[3];
    unsigned int   m_cachedDw08;
    unsigned int   m_cachedDw0C;
    unsigned int   m_cachedDw10;
    unsigned int   m_cachedDw14;
    unsigned char  m_cachedByte18;
    unsigned char  m_pad81[3];
    float          m_halfTexelU1;
    float          m_halfTexelU2;
    float          m_halfTexelV1;
    float          m_halfTexelV2;
    void*          m_vertexBuffer;
};

#if defined(_M_IX86)
static_assert(sizeof(QuadDrawer) == 0x108098,
              "RefFactory_geom_Meme_QuadDrawer allocates 0x108098 - see 0x1007C950");
#endif

namespace {

void RD9_THUNK qd_addRef(void* s RD9_THUNK_EDX) { static_cast<QuadDrawer*>(s)->addRef(); }
int  RD9_THUNK qd_getRef(void* s RD9_THUNK_EDX) { return static_cast<QuadDrawer*>(s)->getRef(); }
int  RD9_THUNK qd_release(void* s RD9_THUNK_EDX) { return static_cast<QuadDrawer*>(s)->release(); }

void* RD9_THUNK qd_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{
    return static_cast<QuadDrawer*>(s)->queryInterface(iid);
}

void RD9_THUNK qd_flush(void* s RD9_THUNK_EDX)      { static_cast<QuadDrawer*>(s)->flush(); }
void RD9_THUNK qd_beginFrame(void* s RD9_THUNK_EDX) { static_cast<QuadDrawer*>(s)->beginFrame(); }
void RD9_THUNK qd_resetState(void* s RD9_THUNK_EDX) { static_cast<QuadDrawer*>(s)->resetState(); }
void RD9_THUNK qd_endFrame(void* s RD9_THUNK_EDX)   { static_cast<QuadDrawer*>(s)->endFrame(); }

void* const kQuadDrawerVtable[8] =
{
    reinterpret_cast<void*>(&qd_addRef),          // +0x00  0x10132630  ICF_incrementDwordAt0xC
    reinterpret_cast<void*>(&qd_getRef),          // +0x04  0x10051710  ICF_getDwordAt0xC
    reinterpret_cast<void*>(&qd_release),         // +0x08
    reinterpret_cast<void*>(&qd_queryInterface),  // +0x0C
    reinterpret_cast<void*>(&qd_flush),           // +0x10
    reinterpret_cast<void*>(&qd_beginFrame),      // +0x14
    reinterpret_cast<void*>(&qd_resetState),      // +0x18
    reinterpret_cast<void*>(&qd_endFrame)         // +0x1C
};

// Suspends drawing on device loss and resets the vertex cursor before resuming.
// BufferManager recreates GPU resources at priority 3, before this listener at priority 4.
void RD9_THUNK qd_onDeviceLost(void* s RD9_THUNK_EDX)
{
    QuadDrawer::fromDeviceListener(s)->onDeviceLost();
}
void RD9_THUNK qd_onDeviceReset(void* s RD9_THUNK_EDX)
{
    QuadDrawer::fromDeviceListener(s)->onDeviceReset();
}
void RD9_THUNK qd_listenerNullsub(void* RD9_THUNK_EDX) { /* 0x1008AF70: ret */ }

void* const kQuadDrawerListenerVtable[3] =
{
    reinterpret_cast<void*>(&qd_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&qd_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&qd_listenerNullsub)  // +0x08
};

// Forwards the event from its listener subobject; userData is the fourth event argument.
void RD9_THUNK qd_handleECCoreEvent(void* s RD9_THUNK_EDX, int category, unsigned int id,
                                     void* /*node*/, unsigned char* payload)
{
    QuadDrawer::fromECCoreListener(s)->handleECCoreEvent(category, id, payload);
}

// The core-event interface has one virtual slot; the adjacent table belongs to the device listener.
void* const kQuadDrawerECCoreVtable[1] =
{
    reinterpret_cast<void*>(&qd_handleECCoreEvent),  // +0x00
};

// Shared clipping scratch: paired position/color and UV buffers plus triangle-fan output.
// Initialization keeps each position/color vertex's unused eighth word at 1.0.
unsigned int g_clipInitFlags = 0;                  // dword_1024AAA0
float        g_clipUvA[16];                        // dword_1024AA20
float        g_clipUvB[16];                        // dword_1024AA60 (the 8-pair buffer)
float        g_clipScratchB[8 * 8];                // unk_1024A820
float        g_clipScratchA[8 * 8];                // unk_1024A920
float        g_clipFanOut[8 * 11];                 // dword_1024A6C0

// sub_101C4D30 - lerp the 7 meaningful dwords of a scratch vertex (pos3 + colour4).
void clipLerpVertex(float* out, const float* a, const float* b, float t)
{
    for (int i = 0; i < 7; ++i)
        out[i] = (b[i] - a[i]) * t + a[i];
}

} // anonymous namespace

// dword_10247C38 - ModuleGeomMisc's singleton instance slot. Now non-null: the class registers.
extern IBase* g_QuadDrawer;


// Creates shader parameters and a dynamic default-pool vertex buffer, then registers device and core listeners.

QuadDrawer::QuadDrawer()
    : m_vptr(const_cast<void**>(kQuadDrawerVtable))
    , m_deviceLostVptr(const_cast<void**>(kQuadDrawerListenerVtable))
    , m_ecCoreVptr(const_cast<void**>(kQuadDrawerECCoreVtable))
    , m_refCount(1)
    , m_registered(0)
    , m_suspended(0)
    , m_captureMode(0)
    , m_basePos(0)
    , m_enabled(1)
    , m_vertexCount(0)
    , m_cachedTexture(0)
    , m_cachedTexture2(0)
    , m_cachedBlend(1)
    , m_cachedDw08(7)
    , m_cachedDw0C(8)
    , m_cachedDw10(0xFFFFFFFF)
    , m_cachedDw14(0xFFFFFFFF)
    , m_cachedByte18(0)
    , m_halfTexelU1(0.0f)
    , m_halfTexelU2(0.0f)
    , m_halfTexelV1(0.0f)
    , m_halfTexelV2(0.0f)
    , m_vertexBuffer(0)
{
#if defined(_M_IX86)
    static_assert(offsetof(QuadDrawer, m_deviceLostVptr) == 4
                      && offsetof(QuadDrawer, m_ecCoreVptr) == 8,
                  "the listener thunks are handed this + 4 and this + 8");
#endif
    m_pad12[0] = 0; m_pad12[1] = 0;
    m_pad55[0] = 0; m_pad55[1] = 0; m_pad55[2] = 0;
    m_pad5D[0] = 0; m_pad5D[1] = 0; m_pad5D[2] = 0;
    m_pad6D[0] = 0; m_pad6D[1] = 0; m_pad6D[2] = 0;
    m_pad81[0] = 0; m_pad81[1] = 0; m_pad81[2] = 0;
    m_passIndex = 0;

    ::memset(m_vertices, 0, sizeof(m_vertices));   // the shipped 24576-iteration zeroing loop

    // The tail, sub_1008AC10
    m_effect = ::dice::hfe::dx9rend::shaderManagerGetEffect(
        ::dice::hfe::dx9rend::g_shaderManager, "mods/xpack/shaders/PortedMenuShader");

    static const char* const kParamNames[14] =
    {
        "matWORLD", "matVIEW", "matPROJ", "ALPHABLEND", "SRCBLEND", "DESTBLEND",
        "ALPHATEST", "ALPHAFUNC", "ALPHAREF", "ZMODE", "ZFUNC", "ZWRITEENABLE",
        "TEXLAYER0", "TEXLAYER1"
    };
    for (int i = 0; i < 14; ++i)
        m_params[i] = m_effect
            ? ::dice::hfe::dx9rend::effectGetShaderParameter(
                  static_cast<::dice::hfe::dx9rend::Effect*>(m_effect), std::string(kParamNames[i]))
            : 0;

    const int savedPool = ::dice::hfe::dx9rend::bufferManagerSwapPool(0);   // default
    m_vertexBuffer = ::dice::hfe::dx9rend::bufferManagerCreateVertexBuffer(
        24576, 44, 0x208 /* DYNAMIC | WRITEONLY */, "QuadDrawer");
    ::dice::hfe::dx9rend::bufferManagerSwapPool(savedPool);

    // RendDX9_registerDeviceLostResetListener(this + 4, 4) - the shipped order: after the pool
    // restore, before the ECCore handler. No test of the base global (dword_1023B970), as
    // shipped; ModuleGeomMisc's singletons are created after the Renderer's constructor built it.
    ::dice::hfe::dx9rend::registerDeviceLostResetListener(&m_deviceLostVptr, 4);

    if (::dice::hfe::eventManager)
    {
        ::dice::hfe::eventManager->registerEventHandler(
            ECCore, reinterpret_cast<IEventListener*>(&m_ecCoreVptr), 0);
    }
    m_registered = 1;

    ::renddx9abi::logf("QuadDrawer: built at 0x%p - effect=0x%p, 14 params, dynamic VB "
                       "wrapper=0x%p (pool swapped DEFAULT for usage 0x208), ECCore handler "
                       "registered LIVE at priority 0",
                       static_cast<const void*>(this), m_effect, m_vertexBuffer);
}

// Meme_QuadDrawer_release (0x1008CAB0): --refcount(+0x0C); at zero, Meme_QuadDrawer_dtor + free.
int QuadDrawer::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
    {
        dtor();
        ::renddx9abi::engineFree(this);
    }
    return remaining;
}

// Releases shader parameters, effect and vertex buffer before unregistering core and device listeners.
void QuadDrawer::dtor()
{
    m_vptr           = const_cast<void**>(kQuadDrawerVtable);
    m_deviceLostVptr = const_cast<void**>(kQuadDrawerListenerVtable);
    m_ecCoreVptr     = const_cast<void**>(kQuadDrawerECCoreVtable);

    for (int i = 0; i < 14; ++i)
    {
        if (m_params[i])
        {
            static_cast<::dice::hfe::dx9rend::ShaderParameter*>(m_params[i])->release();
            m_params[i] = 0;
        }
    }
    if (m_effect)
    {
        ::dice::hfe::dx9rend::effectRelease(
            static_cast<::dice::hfe::dx9rend::Effect*>(m_effect));
        m_effect = 0;
    }
    if (m_vertexBuffer)
    {
        // `--wrapper->refCount; if zero { sub_10014510; free }` - the BufferManager's tracking set
        // holds the other reference, so on a live manager this only drops the QuadDrawer's.
        // The shipped release is unguarded; the guard stays [CHOSEN] for the harness host, whose
        // device-less createVertexBuffer answers null (the same reason as the parameter guards).
        ::dice::hfe::dx9rend::vertexBufferRelease(m_vertexBuffer);
        m_vertexBuffer = 0;
    }
    if (::dice::hfe::eventManager)
    {
        ::dice::hfe::eventManager->unregisterEventHandler(
            ECCore, reinterpret_cast<IEventListener*>(&m_ecCoreVptr));
    }

    // RendDX9_unregisterDeviceLostResetListener(this + 4). Unguarded, as shipped.
    ::dice::hfe::dx9rend::unregisterDeviceLostResetListener(&m_deviceLostVptr);

    m_registered = 0;                                      // mov byte [esi+10h], 0
}

// Meme_QuadDrawer_queryInterfaceById (0x1008CA80): iid 1 and iid 80031.
void* QuadDrawer::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IQuadDrawer)
        return const_cast<QuadDrawer*>(this);
    return 0;
}

// Updates draw enable from the application-active byte and suspends on deactivation.
// Only device reset clears suspension, so reactivation requires the renderer's reset sequence.
void QuadDrawer::handleECCoreEvent(int category, unsigned int id, unsigned char* payload)
{
    if (category == 5 && id == 15)
    {
        const unsigned char active = *payload;
        m_enabled = active;
        if (!active)
            m_suspended = 1;
    }
}

// +14/+1C (1008AED0/1008AF10): the lookup belongs to the member Effect's
// technique map, not ShaderManager's effect cache. No null-effect guard exists.
void QuadDrawer::beginFrame()
{
    ::dice::hfe::dx9rend::effectBeginTechniqueStates(m_effect, std::string("Menu"));
    m_basePos = 0;
}

void QuadDrawer::endFrame()
{
    ::dice::hfe::dx9rend::effectEndTechniqueStates(m_effect, std::string("Menu"));
}

// 1008B410, previously omitted from the primary table. Deliberately does not
// reset blend, cachedDw14, U2 or V2; the final tail call uploads this mixed state.
void QuadDrawer::resetState()
{
    m_cachedTexture = nullptr;
    m_cachedTexture2 = nullptr;
    m_vertexCount = 0;
    m_cachedDw08 = 7;
    m_cachedDw0C = 8;
    m_cachedDw10 = 0xFFFFFFFFu;
    m_cachedByte18 = 0;
    m_halfTexelU1 = 0.0f;
    m_halfTexelV1 = 0.0f;
    uploadStateParams();
}

namespace {

// The submit's state-cache refresh, factored exactly as the shipped body repeats it twice: cache
// the request's key fields and recompute the four half-texel offsets from the textures' page dims
// (virtual +0x3C/+0x40 on each Meme texture).
void cacheHalfTexels(QuadDrawer* self, MemeTexture* tex, MemeTexture* tex2,
                     float* u1, float* v1, float* u2, float* v2)
{
    typedef int (__thiscall* GetDimFn)(void*);
    if (tex)
    {
        void** vtbl = *reinterpret_cast<void***>(tex);
        const int w = reinterpret_cast<GetDimFn>(vtbl[0x3C / 4])(tex);
        const int h = reinterpret_cast<GetDimFn>(vtbl[0x40 / 4])(tex);
        *u1 = 0.5f / static_cast<float>(w);
        *v1 = 0.5f / static_cast<float>(h);
        if (tex2)
        {
            void** vtbl2 = *reinterpret_cast<void***>(tex2);
            const int w2 = reinterpret_cast<GetDimFn>(vtbl2[0x3C / 4])(tex2);
            const int h2 = reinterpret_cast<GetDimFn>(vtbl2[0x40 / 4])(tex2);
            *u2 = 0.5f / static_cast<float>(w2);
            *v2 = 0.5f / static_cast<float>(h2);
        }
        else
        {
            *u2 = 0.0f;
            *v2 = 0.0f;
        }
    }
    else
    {
        *u1 = 0.0f;
        *v1 = 0.0f;
    }
    (void)self;
}

} // anonymous namespace

void QuadDrawer::cacheState(MemeTexture* tex, QuadDrawRequest* req)
{
    m_cachedTexture  = tex;
    m_cachedTexture2 = static_cast<MemeTexture*>(req->texture2);
    m_cachedBlend    = req->blendByte;
    m_cachedDw08     = req->stateDw08;
    m_cachedDw0C     = req->stateDw0C;
    m_cachedDw10     = req->stateDw10;
    m_cachedDw14     = req->stateDw14;
    m_cachedByte18   = req->stateByte18;
    cacheHalfTexels(this, tex, m_cachedTexture2,
                    &m_halfTexelU1, &m_halfTexelV1, &m_halfTexelU2, &m_halfTexelV2);
}


// Submits a quad, subtracting half a pixel from the request's corner coordinates in place.
// Flushes on state changes or capacity limits unless capturing, then clips and triangulates the geometry.

void QuadDrawer::submit(MemeTexture* tex, QuadDrawRequest* req,
                        float u0, float v0, float u1, float v1)
{
    for (int c = 0; c < 4; ++c)
    {
        req->corner[c].x -= 0.5f;
        req->corner[c].y -= 0.5f;
    }

    if (m_captureMode)
    {
        cacheState(tex, req);
    }
    else
    {
        // the state-change comparison, texture identity by the wrapped EngineTexture at +0x2C
        bool texChanged;
        if (m_cachedTexture)
            texChanged = !tex
                || memeTextureEngineTexture(tex) != memeTextureEngineTexture(m_cachedTexture);
        else
            texChanged = tex != 0;

        bool tex2Changed;
        MemeTexture* rt2 = static_cast<MemeTexture*>(req->texture2);
        if (rt2 && m_cachedTexture2)
            tex2Changed = memeTextureEngineTexture(rt2) != memeTextureEngineTexture(m_cachedTexture2);
        else
            tex2Changed = (rt2 != 0) != (m_cachedTexture2 != 0);

        if (texChanged || tex2Changed
            || req->blendByte   != m_cachedBlend
            || req->stateDw08   != m_cachedDw08
            || req->stateDw0C   != m_cachedDw0C
            || req->stateDw10   != m_cachedDw10
            || req->stateByte18 != m_cachedByte18
            || req->stateDw14   != m_cachedDw14
            || m_vertexCount + 6 >= 0x6000)
        {
            flush();                                       // vslot +0x10
            cacheState(tex, req);
        }
    }

    if (!req->clipRect)
    {
        // Appends triangles (c3,c0,c1) and (c1,c2,c3). Secondary UVs remain unchanged, including stale reused values.
        QuadVertex* v = &m_vertices[m_vertexCount];
        m_vertexCount += 6;

        const QuadCorner& c0 = req->corner[0];
        const QuadCorner& c1 = req->corner[1];
        const QuadCorner& c2 = req->corner[2];
        const QuadCorner& c3 = req->corner[3];

        v[0].x = c3.x; v[0].y = c3.y; v[0].z = c3.z;
        v[0].r = c3.r; v[0].g = c3.g; v[0].b = c3.b; v[0].a = c3.a;
        v[0].u1 = u0; v[0].v1 = v1;

        v[1].x = c0.x; v[1].y = c0.y; v[1].z = c0.z;
        v[1].r = c0.r; v[1].g = c0.g; v[1].b = c0.b; v[1].a = c0.a;
        v[1].u1 = u0; v[1].v1 = v0;

        v[2].x = c1.x; v[2].y = c1.y; v[2].z = c1.z;
        v[2].r = c1.r; v[2].g = c1.g; v[2].b = c1.b; v[2].a = c1.a;
        v[2].u1 = u1; v[2].v1 = v0;

        v[3].x = v[2].x; v[3].y = v[2].y; v[3].z = v[2].z;
        v[3].r = v[2].r; v[3].g = v[2].g; v[3].b = v[2].b; v[3].a = v[2].a;
        v[3].u1 = v[2].u1; v[3].v1 = v[2].v1;

        v[4].x = c2.x; v[4].y = c2.y; v[4].z = c2.z;
        v[4].r = c2.r; v[4].g = c2.g; v[4].b = c2.b; v[4].a = c2.a;
        v[4].u1 = u1; v[4].v1 = v1;

        v[5].x = v[0].x; v[5].y = v[0].y; v[5].z = v[0].z;
        v[5].r = v[0].r; v[5].g = v[0].g; v[5].b = v[0].b; v[5].a = v[0].a;
        v[5].u1 = v[0].u1; v[5].v1 = v[0].v1;
        return;
    }

    // The clipped path
    const float clipX0 = req->clipRect[0];
    const float clipY0 = req->clipRect[1];
    const float clipX1 = req->clipRect[0] + req->clipRect[2];
    const float clipY1 = req->clipRect[1] + req->clipRect[3];

    // the once-per-boot scratch init the shipped flags gate; ours zero-initialise statically, the
    // colour pads are set to 1.0 here to match bits 2/3
    if (!(g_clipInitFlags & 4))
    {
        g_clipInitFlags |= 4 | 8 | 1 | 2 | 16;
        for (int i = 0; i < 8; ++i)
        {
            g_clipScratchA[8 * i + 7] = 1.0f;
            g_clipScratchB[8 * i + 7] = 1.0f;
        }
    }

    // seed polygon: the four corners + the caller uv at the four combinations
    float (*src)[8]  = reinterpret_cast<float(*)[8]>(g_clipScratchA);
    float (*dst)[8]  = reinterpret_cast<float(*)[8]>(g_clipScratchB);
    float (*uvSrc)[2] = reinterpret_cast<float(*)[2]>(g_clipUvA);
    float (*uvDst)[2] = reinterpret_cast<float(*)[2]>(g_clipUvB);

    // The seed polygon is the request's OWN corner array (already the 8-dword scratch shape), in
    // order 0..3, with the uv quadruple the shipped code writes into dword_1024AA20:
    // (u0,v0) (u1,v0) (u1,v1) (u0,v1).
    const float cornerUv[4][2] =
        { { u0, v0 }, { u1, v0 }, { u1, v1 }, { u0, v1 } };
    for (int i = 0; i < 4; ++i)
    {
        const QuadCorner& c = req->corner[i];
        src[i][0] = c.x; src[i][1] = c.y; src[i][2] = c.z;
        src[i][3] = c.r; src[i][4] = c.g; src[i][5] = c.b; src[i][6] = c.a;
        uvSrc[i][0] = cornerUv[i][0];
        uvSrc[i][1] = cornerUv[i][1];
    }
    int count = 4;

    for (int pass = 0; pass < 4 && count > 0; ++pass)
    {
        const int axis    = (pass == 0 || pass == 2) ? 0 : 1;
        const bool isMin  = pass < 2;
        const float bound = (pass == 0) ? clipX0 : (pass == 1) ? clipY0
                          : (pass == 2) ? clipX1 : clipY1;

        int outCount = 0;
        for (int i = 0; i < count; ++i)
        {
            float* cur  = src[i];
            float* prev = src[(i + count - 1) % count];
            float* uvC  = uvSrc[i];
            float* uvP  = uvSrc[(i + count - 1) % count];

            const bool curIn  = isMin ? (cur[axis]  >= bound) : (cur[axis]  <= bound);
            const bool prevIn = isMin ? (prev[axis] >= bound) : (prev[axis] <= bound);

            if (curIn != prevIn)
            {
                const float t = (bound - prev[axis]) / (cur[axis] - prev[axis]);
                clipLerpVertex(dst[outCount], prev, cur, t);           // sub_101C4D30
                uvDst[outCount][0] = (uvC[0] - uvP[0]) * t + uvP[0];
                uvDst[outCount][1] = (uvC[1] - uvP[1]) * t + uvP[1];
                ++outCount;
            }
            if (curIn)
            {
                ::memcpy(dst[outCount], cur, 8 * sizeof(float));
                uvDst[outCount][0] = uvC[0];
                uvDst[outCount][1] = uvC[1];
                ++outCount;
            }
        }
        count = outCount;
        float (*ts)[8] = src;   src = dst;   dst = ts;
        float (*tu)[2] = uvSrc; uvSrc = uvDst; uvDst = tu;
    }

    if (count < 3)
        return;

    // assemble the surviving polygon into the 11-dword fan buffer
    float (*fan)[11] = reinterpret_cast<float(*)[11]>(g_clipFanOut);
    for (int i = 0; i < count; ++i)
    {
        ::memcpy(fan[i], src[i], 7 * sizeof(float));       // pos3 + colour4
        fan[i][7] = uvSrc[i][0];
        fan[i][8] = uvSrc[i][1];
        fan[i][9]  = 0.0f;
        fan[i][10] = 0.0f;
    }

    // the second-layer uv: project each vertex into the clip rect through texture2's own virtual
    // uv getters (+0x4C uOffset, +0x50 vOffset, +0x54 uvWidth, +0x58 uvHeight)
    if (req->texture2)
    {
        MemeTexture* t2 = static_cast<MemeTexture*>(req->texture2);
        void** vtbl = *reinterpret_cast<void***>(t2);
        typedef float (__thiscall* GetF)(void*);
        const float uOff = reinterpret_cast<GetF>(vtbl[0x4C / 4])(t2);
        const float vOff = reinterpret_cast<GetF>(vtbl[0x50 / 4])(t2);
        const float uW   = reinterpret_cast<GetF>(vtbl[0x54 / 4])(t2);
        const float uH   = reinterpret_cast<GetF>(vtbl[0x58 / 4])(t2);
        for (int i = 0; i < count; ++i)
        {
            fan[i][9]  = uOff + uW * ((fan[i][0] - req->clipRect[0]) / req->clipRect[2]);
            fan[i][10] = vOff + uH * ((fan[i][1] - req->clipRect[1]) / req->clipRect[3]);
        }
    }

    // fan emission: (0, i+1, i+2) triangles into the batch
    for (int i = 0; i + 2 < count; ++i)
    {
        QuadVertex* v = &m_vertices[m_vertexCount];
        m_vertexCount += 3;
        const float* tri[3] = { fan[0], fan[i + 1], fan[i + 2] };
        for (int k = 0; k < 3; ++k)
        {
            v[k].x = tri[k][0]; v[k].y = tri[k][1]; v[k].z = tri[k][2];
            v[k].r = tri[k][3]; v[k].g = tri[k][4]; v[k].b = tri[k][5]; v[k].a = tri[k][6];
            v[k].u1 = tri[k][7]; v[k].v1 = tri[k][8];
            v[k].u2 = tri[k][9]; v[k].v2 = tri[k][10];
        }
    }
}

namespace {

// sub_1008AF80 - the engine blend enum -> D3DBLEND. The default arm is one (2).
unsigned int mapBlendEnum(unsigned int v)
{
    switch (v)
    {
    case 1:  return 1;    // zero
    case 3:  return 3;    // srccolor
    case 4:  return 4;    // invsrccolor
    case 5:  return 9;    // destcolor
    case 6:  return 10;   // invdestcolor
    case 7:  return 5;    // srcalpha
    case 8:  return 6;    // invsrcalpha
    case 9:  return 7;    // destalpha
    case 10: return 8;    // invdestalpha
    case 11: return 11;   // srcalphasat
    default: return 2;    // one
    }
}

// sub_1008B030 - the engine compare enum -> D3DCMP. 5 and 6 are swapped (engine notequal=5,
// greater=6); the default arm is always (8).
unsigned int mapCmpEnum(unsigned int v)
{
    switch (v)
    {
    case 1:  return 1;
    case 2:  return 2;
    case 3:  return 3;
    case 4:  return 4;
    case 5:  return 6;    // notequal
    case 6:  return 5;    // greater
    case 7:  return 7;
    default: return 8;    // always
    }
}

// The three ShaderParameter operations the upload uses. The handle and its 12-slot owner contract
// are recovered types; call their methods normally rather than spelling their ABI positions here.
void paramSetValue(void* handle, const void* data, unsigned int count)
{
    static_cast<dx9rend::ShaderParameter*>(handle)->setValue(data, count);
}
void paramSetBool(void* handle, unsigned char value)
{
    static_cast<dx9rend::ShaderParameter*>(handle)->setBool(value);
}
void paramSetInt(void* handle, int value)
{
    static_cast<dx9rend::ShaderParameter*>(handle)->setInt(value);
}

} // anonymous namespace


// Uploads texture, blend, depth and shader state. A value of -1 preserves the alpha-test setting.
// Depth -1 selects an always-pass test and disables depth writes without changing zmode.

void QuadDrawer::uploadStateParams()
{
    void* engineTex = 0;
    if (m_cachedTexture)
        engineTex = memeTextureEngineTexture(m_cachedTexture);          // +0x2C

    if (m_cachedTexture && engineTex)
    {
        paramSetValue(m_params[12], engineTex, 0);                      // TEXLAYER0
        if (m_cachedTexture2)
        {
            paramSetValue(m_params[13], memeTextureEngineTexture(m_cachedTexture2), 0);   // TEXLAYER1
            m_passIndex = 2;
        }
        else
        {
            m_passIndex = 1;
        }
    }
    else
    {
        paramSetValue(m_params[12], 0, 0);
        m_passIndex = 0;
    }

    paramSetBool(m_params[3], m_cachedBlend);                           // alphablend
    if (m_cachedBlend)
    {
        paramSetInt(m_params[4], static_cast<int>(mapBlendEnum(m_cachedDw08)));   // srcblend
        paramSetInt(m_params[5], static_cast<int>(mapBlendEnum(m_cachedDw0C)));   // destblend
    }

    if (m_cachedDw10 != 0xFFFFFFFFu)
    {
        paramSetBool(m_params[6], 1);                                   // alphatest
        paramSetInt(m_params[7], static_cast<int>(mapCmpEnum(m_cachedDw10)));     // alphafunc
        paramSetInt(m_params[8], static_cast<int>(m_cachedByte18));     // alpharef
    }

    if (m_cachedDw14 == 0xFFFFFFFFu)
    {
        paramSetInt(m_params[10], 8);                                   // zfunc = always
        paramSetBool(m_params[11], 0);                                  // zwriteenable off
    }
    else
    {
        paramSetBool(m_params[9], 1);                                   // zmode
        paramSetInt(m_params[10], static_cast<int>(mapCmpEnum(m_cachedDw14)));    // zfunc
        paramSetBool(m_params[11], 1);                                  // zwriteenable
    }

    // sub_100791E0's shared identity - built once behind a flag in the original; a local static
    // carries the same value with the same one-time shape.
    static const float kIdentity[16] =
    {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };

    dx9rend::IRenderView* const view = ::dice::hfe::dx9rend::rendererGetRenderView();
    float* const viewMatrix = view->getViewMatrixPtr()->m;
    paramSetValue(m_params[0], viewMatrix, 0);                          // matWORLD
    paramSetValue(m_params[1], kIdentity, 0);                           // matVIEW
    float* const projMatrix = view->getProjectionMatrixPtr()->m;
    paramSetValue(m_params[2], projMatrix, 0);                          // matPROJ
}


// Uploads and draws queued vertices, then resets the batch. Disabled or suspended drawing is skipped.
// Uses discard or no-overwrite locking according to the vertex-buffer cursor.

void QuadDrawer::flush()
{
    if (m_suspended || !m_enabled)
    {
        m_vertexCount = 0;
        return;
    }
    if (m_captureMode)
        ::renddx9abi::logf("QuadDrawer.cpp:521: try to flush between begin/end capture");

    const unsigned int count = m_vertexCount;
    if (!count)
        return;

    if (count >= 0x6000)
    {
        // QuadDrawer.cpp:562, severity 1 - the "Dynamic VertexBuffer overrun detected" arm.
        ::renddx9abi::logf("QuadDrawer.cpp:562: %u Dynamic VertexBuffer overrun detected, "
                           "dropping the batch", count);
        m_vertexCount = 0;
        return;
    }

    if (count + m_basePos >= 0x6000)
        m_basePos = 0;

    void* dst = m_basePos
        ? ::dice::hfe::dx9rend::vertexBufferLock(m_vertexBuffer, m_basePos, count,
                                                 0x1000 /* D3DLOCK_NOOVERWRITE */)
        : ::dice::hfe::dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0,
                                                 0x2000 /* D3DLOCK_DISCARD */);
    if (dst)
        ::memcpy(dst, m_vertices, 44u * count);
    ::dice::hfe::dx9rend::vertexBufferUnlock(m_vertexBuffer);

    ::dice::hfe::dx9rend::stateManagerPush();

    ::dice::hfe::dx9rend::effectSetTechniqueByName(m_effect, std::string("QuadWithTexture"));
    uploadStateParams();
    ::dice::hfe::dx9rend::effectBegin(m_effect);
    ::dice::hfe::dx9rend::effectBeginPass(m_effect, m_passIndex);

    ::dice::hfe::dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
    ::dice::hfe::dx9rend::rendererDrawPrimitive(count / 3u, 4u /* D3DPT_TRIANGLELIST */,
                                                m_basePos);

    m_basePos += count;
    m_vertexCount = 0;

    ::dice::hfe::dx9rend::effectEnd(m_effect);
    ::dice::hfe::dx9rend::stateManagerPop();
}


// The seam the two Meme::Texture draw slots call - the shipped call is a direct `call sub_1008B480`
// with ecx = dword_10247C38.

void quadDrawerSubmit(void* drawer, void* memeTexture, void* request,
                      float u0, float v0, float u1, float v1)
{
    static_cast<QuadDrawer*>(drawer)->submit(
        static_cast<MemeTexture*>(memeTexture),
        static_cast<QuadDrawRequest*>(request), u0, v0, u1, v1);
}

// RefFactory_geom_Meme_QuadDrawer, 0x1007C950.
IBase* __fastcall createQuadDrawer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(QuadDrawer));
    if (!storage)
        return 0;

    QuadDrawer* self = new (storage) QuadDrawer();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
