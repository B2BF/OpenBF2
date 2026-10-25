// Full-screen movie playback through Video. [INFERRED] Source placement.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/D3dxEffect.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../RendDX9/ShaderParameter.h"

#include <new>
#include <cstring>

namespace dice {
namespace hfe {
namespace dx9rend {
// ShaderManager.cpp / Effect.cpp - the same seams DebugLineGraphD3d.cpp declares.
extern void* g_shaderManager;                       // dword_1023B97C
void* shaderManagerGetEffect(void*, const char*);   // sub_10022EF0
class Effect;
void* effectGetShaderParameter(Effect*, const vc71::string&);   // sub_1001E420
void* effectD3dxHandle(void* effect);
int   bufferManagerSwapPool(int value);
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name);
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags);
bool  vertexBufferUnlock(void* wrapper);
void  vertexBufferRelease(void* wrapper);
void  vertexBufferBindStream(void* wrapper, unsigned int stream);
void  vertexDeclarationBind(void* wrapper);
bool  rendererBeginScene(const unsigned int* clearRgb, bool doClear);
bool  rendererEndScene();
bool  rendererPresent(unsigned int mode);
void  rendererDrawPrimitive(unsigned int primCount, unsigned int type,
                            unsigned int startVertex);
}
namespace geom {

extern const unsigned int IID_IVideo;
void  videoInitSound(void* video);
void  videoShutdownSound(void* video);
bool  videoOpen(void* video, const char* path);
void* videoAcquireFrame(void* video);
void* videoTargetD3DTexture(void* target);
void  videoDoFrame(void* video);
int   videoIsDone(void* video);

// From getClassList's own registration at 0x1007DF47/0x1020AF2C, and queryInterface (0x100A2410).
extern const unsigned int CID_FSMoviePlayer  = 81046u;
extern const unsigned int IID_IFSMoviePlayer = 80047u;


// Owns video playback, a frame-target reference, quad and shader resources.
// Device listener callbacks are empty; Video and BufferManager handle device loss.

class FSMoviePlayer
{
public:
    FSMoviePlayer();

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

    bool  load(const vc71::string& path);   // +0x10
    void  releaseAll();                     // +0x14
    void  drawFrame();                      // +0x18
    bool  finished() const { return videoIsDone(m_video) != 0; }
    void  dtor(char flags);                 // +0x20

private:
    void*         m_vptr;         // +0x00
    void*         m_listenerVptr; // +0x04
    int           m_refCount;     // +0x08
    void*         m_video;        // +0x0C
    void*         m_videoTarget;  // +0x10, owns a reference separate from Video's
    vc71::string  m_path;         // +0x14
    void*         m_vertexBuffer; // +0x30
    dx9rend::Effect* m_effect;    // +0x34
    dx9rend::ShaderParameter* m_paramTransform; // +0x38
    dx9rend::ShaderParameter* m_paramTexture;   // +0x3C
    void*         m_vertexDeclaration; // +0x40
};

static_assert(sizeof(FSMoviePlayer) == 0x44,
              "RefFactory_geom_FSMoviePlayer allocates 0x44 - see 0x1007D030");

namespace {

void __fastcall fsm_addRef(void* s, void*) { static_cast<FSMoviePlayer*>(s)->addRef(); }
int  __fastcall fsm_getRef(void* s, void*) { return static_cast<FSMoviePlayer*>(s)->getRef(); }
int  __fastcall fsm_release(void* s, void*) { return static_cast<FSMoviePlayer*>(s)->release(); }

void* __fastcall fsm_queryInterface(void* s, void*, unsigned int iid)
{
    return static_cast<FSMoviePlayer*>(s)->queryInterface(iid);
}

// +0x10 - the shipped signature takes the path by value (retn 0x1C); the copy lives inside.
#if BF2_PLUGIN_ABI_NATIVE
// [CHOSEN, not recovered] Converts the host's by-value string once at the native movie boundary.
// [RETIRE-WHEN: fsmovie-native-string-adapter] Replace the internal path with native storage while preserving the host's by-value load contract.
bool __fastcall fsm_load(void* s, void*, ::renddx9abi::HostString path)
{
    const vc71::string internalPath(path.data(), static_cast<unsigned int>(path.size()));
    return static_cast<FSMoviePlayer*>(s)->load(internalPath);
}
#else
bool __fastcall fsm_load(void* s, void*, const char* pathRaw0, int, int, int, int, int, int)
{
    // the by-value string's seven stack dwords: the first is the union/alloc dword, the buffer
    // or pointer follows. Reconstituting through the layout rather than guessing an overload:
    const vc71::string* path = reinterpret_cast<const vc71::string*>(&pathRaw0);
    const bool ok = static_cast<FSMoviePlayer*>(s)->load(*path);
    path->~string();                                       // callee-destroyed, as shipped
    return ok;
}
#endif

void __fastcall fsm_releaseAll(void* s, void*) { static_cast<FSMoviePlayer*>(s)->releaseAll(); }

void __fastcall fsm_drawFrame(void* s, void*)
{
    static_cast<FSMoviePlayer*>(s)->drawFrame();
}

// Calls the video's playback query. Requires a successfully loaded video; no null guard is provided.
int __fastcall fsm_videoPassthrough(void* s, void*)
{
    return static_cast<FSMoviePlayer*>(s)->finished() ? 1 : 0;
}

void __fastcall fsm_dtor(void* s, void*, char flags) { static_cast<FSMoviePlayer*>(s)->dtor(flags); }

void* const kFSMoviePlayerVtable[9] =
{
    reinterpret_cast<void*>(&fsm_addRef),           // +0x00  0x1001A010 (folded at0x8 pair)
    reinterpret_cast<void*>(&fsm_getRef),           // +0x04
    reinterpret_cast<void*>(&fsm_release),          // +0x08
    reinterpret_cast<void*>(&fsm_queryInterface),   // +0x0C
    reinterpret_cast<void*>(&fsm_load),             // +0x10
    reinterpret_cast<void*>(&fsm_releaseAll),       // +0x14
    reinterpret_cast<void*>(&fsm_drawFrame),        // +0x18  0x100A2300
    reinterpret_cast<void*>(&fsm_videoPassthrough), // +0x1C
    reinterpret_cast<void*>(&fsm_dtor)              // +0x20
};

void __fastcall fsm_listenerNoop(void*, void*) { /* all three shipped slots are no-ops */ }

void* const kFSMoviePlayerListenerVtable[3] =
{
    reinterpret_cast<void*>(&fsm_listenerNoop),     // +0x00
    reinterpret_cast<void*>(&fsm_listenerNoop),     // +0x04
    reinterpret_cast<void*>(&fsm_listenerNoop)      // +0x08  nullsub_39
};

} // anonymous namespace

// geom_FSMoviePlayer_ctor, 0x100A2290 - read in full. The listener vptr is double-written as
// usual; only the survivor is reproduced.
FSMoviePlayer::FSMoviePlayer()
    : m_vptr(const_cast<void**>(kFSMoviePlayerVtable))
    , m_listenerVptr(const_cast<void**>(kFSMoviePlayerListenerVtable))
    , m_refCount(1)
    , m_video(0)
    , m_videoTarget(0)
    , m_vertexBuffer(0)
    , m_effect(0)
    , m_paramTransform(0)
    , m_paramTexture(0)
    , m_vertexDeclaration(0)
{
}

// FSMoviePlayer_release, 0x100A2440 - `--ref; if zero vslot+0x20(this, 1)`, i.e. the virtual
// scalar-deleting destructor with the free bit set.
int FSMoviePlayer::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        dtor(1);
    return remaining;
}

// FSMoviePlayer_queryInterfaceById, 0x100A2410: iid 1 and iid 80047, nothing else.
void* FSMoviePlayer::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IFSMoviePlayer)
        return const_cast<FSMoviePlayer*>(this);
    return 0;
}


// Loads a movie with sound and a full-screen textured quad. Call on a fresh player.
// Failed loads retain any acquired resources for the caller's final release.

bool FSMoviePlayer::load(const vc71::string& path)
{
    m_effect = static_cast<dx9rend::Effect*>(dx9rend::shaderManagerGetEffect(
        dx9rend::g_shaderManager, "mods/xpack/Shaders/LoadingScreen"));
    if (!m_effect)
        return false;

    m_paramTransform = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("TRANSFORM"));
    if (!m_paramTransform)
        return false;

    m_paramTexture = dx9rend::effectGetShaderParameterTyped(m_effect, vc71::string("TEXTURE"));
    if (!m_paramTexture)
        return false;

    m_path = path;

    // The native host exposes the by-name overload through its real ClassManager declaration.
    // HfeAbi's abbreviated interface leaves that overload reserved.
    m_video = reinterpret_cast<ClassManager*>(classManager)->createInstance(
        ::renddx9abi::HostString("dice.hfe.geom.Video"), IID_IVideo, 0);
    if (m_video)
    {
        videoInitSound(m_video);
        if (!videoOpen(m_video, path.c_str()))
        {
            videoShutdownSound(m_video);
            return false;
        }
    }
    m_videoTarget = videoAcquireFrame(m_video);

    static const D3DVERTEXELEMENT9 declaration[] = {
        {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        D3DDECL_END()
    };
    m_vertexDeclaration = dx9rend::vertexDeclarationCreate(declaration);
    const int previousPool = dx9rend::bufferManagerSwapPool(1);
    m_vertexBuffer = dx9rend::bufferManagerCreateVertexBuffer(4, 24, 8, "VB");
    dx9rend::bufferManagerSwapPool(previousPool);
    void* destination = dx9rend::vertexBufferLock(m_vertexBuffer, 0, 0, 0);
    if (!destination)
        return false;

    struct Vertex { float x, y, z; unsigned int colour; float u, v; };
    static_assert(sizeof(Vertex) == 24, "movie quad vertex stride");
    static const Vertex vertices[] = {
        {-1.0f, -1.0f, 0.0f, 0xffffffffu, 0.0f, 1.0f},
        { 1.0f, -1.0f, 0.0f, 0xffffffffu, 1.0f, 1.0f},
        { 1.0f,  1.0f, 0.0f, 0xffffffffu, 1.0f, 0.0f},
        {-1.0f,  1.0f, 0.0f, 0xffffffffu, 0.0f, 0.0f}
    };
    std::memcpy(destination, vertices, sizeof(vertices));
    dx9rend::vertexBufferUnlock(m_vertexBuffer);
    return true;
}

// Releases the frame target before Video, then the quad resources. This is terminal cleanup;
// the retail body leaves member pointers unchanged and must not be called twice.
void FSMoviePlayer::releaseAll()
{
    if (m_paramTransform) m_paramTransform->release();
    if (m_paramTexture) m_paramTexture->release();
    if (m_effect) dx9rend::effectRelease(m_effect);
    if (m_videoTarget) static_cast<IBase*>(m_videoTarget)->release();
    if (m_video) static_cast<IBase*>(m_video)->release();
    if (m_vertexDeclaration) dx9rend::vertexDeclarationRelease(m_vertexDeclaration);
    if (m_vertexBuffer) dx9rend::vertexBufferRelease(m_vertexBuffer);
}

// Decodes only when Video's clock is ready, renders the current frame, and presents.
// The effect texture is unbound after drawing so it does not keep the movie alive.
void FSMoviePlayer::drawFrame()
{
    const unsigned int clearRgb[] = {255, 0, 0};
    if (dx9rend::rendererBeginScene(clearRgb, true))
    {
        dx9rend::stateManagerPush();
        static const float identity[] = {
            1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1
        };
        m_paramTransform->setValue(identity, 0);
        videoDoFrame(m_video);

        void* fx = dx9rend::effectD3dxHandle(m_effect);
        ::renddx9abi::d3dxSlot< ::renddx9abi::D3dxSetTextureFn>(fx, 0xD0)(
            fx, m_paramTexture->getD3DXParam(), videoTargetD3DTexture(m_videoTarget));
        dx9rend::vertexBufferBindStream(m_vertexBuffer, 0);
        dx9rend::vertexDeclarationBind(m_vertexDeclaration);
        dx9rend::effectBegin(m_effect);
        dx9rend::effectBeginPass(m_effect, 0);
        dx9rend::rendererDrawPrimitive(2, D3DPT_TRIANGLEFAN, 0);
        dx9rend::effectEnd(m_effect);
        dx9rend::stateManagerPop();

        fx = dx9rend::effectD3dxHandle(m_effect);
        ::renddx9abi::d3dxSlot< ::renddx9abi::D3dxSetTextureFn>(fx, 0xD0)(
            fx, m_paramTexture->getD3DXParam(), 0);
        dx9rend::rendererEndScene();
    }
    dx9rend::rendererPresent(2);
}

// +0x20, 0x100A2460 - restore both vptrs, releaseAll, destroy the path, free when flags & 1.
void FSMoviePlayer::dtor(char flags)
{
    m_vptr         = const_cast<void**>(kFSMoviePlayerVtable);
    m_listenerVptr = const_cast<void**>(kFSMoviePlayerListenerVtable);
    releaseAll();
    m_path.~string();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
}

// RefFactory_geom_FSMoviePlayer, 0x1007D030. Same shape and __fastcall convention as the others.
IBase* __fastcall createFSMoviePlayer(unsigned int iid, IBase* outer)
{
    if (outer)
        return 0;

    void* storage = ::renddx9abi::engineAlloc(sizeof(FSMoviePlayer));
    if (!storage)
        return 0;

    FSMoviePlayer* self = new (storage) FSMoviePlayer();

    void* itf = self->queryInterface(iid);
    if (!itf)
        self->release();
    return static_cast<IBase*>(itf);
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
