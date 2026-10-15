// Bink movie playback and its D3D frame texture. [INFERRED] source placement.
// Keep the primary vtable order and device-listener subobject at +4 compatible with callers.
// Device loss pauses playback before releasing the texture; reset recreates it before resuming.
// [CHOSEN, not recovered] private member layout, construction seams and working-directory
// Fallback in open() (RW-105). VideoTarget owns its texture and level-zero surface.

#ifdef BF2_IMAGE_RENDDX9

#include "../../../RendDX9Abi/ThunkAbi.h"
#include "../../../RendDX9Abi/HfeAbi.h"
#include "../../../RendDX9Abi/ModuleCommon.h"
#include "../../../RendDX9Abi/PluginEntry.h"
#include "../../../RendDX9Abi/PluginAbi.h"
#include "../../../RendDX9Abi/BfMemory.h"
#include "../../../RendDX9Abi/Frontier.h"
#include "../../../RendDX9Abi/Bink.h"
#include "../../RendDX9/RendDX9Base.h"
#include "../../../RendDX9Abi/D3dx9Sdk.h"

#include <windows.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <new>
#if !defined(_M_IX86)
#include <set>
#include <string>
#endif

namespace dice {
namespace hfe {
namespace dx9rend { void* renderDevice(); }     // RendDX9Base.h - the live IDirect3DDevice9
namespace geom {

// binkw32.dll exports decorated __stdcall names (_BinkOpen@8, ...). `lib /def` turns each def entry
// into a thunk symbol with one extra leading underscore (__BinkOpen@8) while the DLL import name
// stays correct, so a plain `extern "C" __stdcall BinkOpen` (symbol _BinkOpen@8) does not match the
// lib's thunk. /alternatename bridges the two. x86 only: the x64 image does not import Bink (issue #7).
#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:_BinkOpen@8=__BinkOpen@8")
#pragma comment(linker, "/alternatename:_BinkClose@4=__BinkClose@4")
#pragma comment(linker, "/alternatename:_BinkWait@4=__BinkWait@4")
#pragma comment(linker, "/alternatename:_BinkDoFrame@4=__BinkDoFrame@4")
#pragma comment(linker, "/alternatename:_BinkNextFrame@4=__BinkNextFrame@4")
#pragma comment(linker, "/alternatename:_BinkCopyToBuffer@28=__BinkCopyToBuffer@28")
#pragma comment(linker, "/alternatename:_BinkGetSummary@8=__BinkGetSummary@8")
#pragma comment(linker, "/alternatename:_BinkPause@8=__BinkPause@8")
#pragma comment(linker, "/alternatename:_BinkSetSoundSystem@8=__BinkSetSoundSystem@8")
#pragma comment(linker, "/alternatename:_BinkOpenDirectSound@4=__BinkOpenDirectSound@4")
#endif

// [INFERRED] Local class IDs are chosen; creation resolves the identified class names.
// Interface IDs match queryInterface and callers.
extern const unsigned int CID_Video        = 81048u;
extern const unsigned int IID_IVideo       = 0xB801DD04u;
extern const unsigned int CID_VideoTarget  = 81049u;
extern const unsigned int IID_IVideoTarget = 0xB801DD03u;

typedef ::renddx9abi::HostString HostString;


// VideoTarget - the D3D texture a movie's frames decode into.

const void** videoTargetVtable();
const void** videoTargetListenerVtable();

class VideoTarget
{
public:
    // Registers the target listener at priority 1. [CHOSEN] Clears the surface description before first creation.
    VideoTarget() : m_vptr(videoTargetVtable()), m_listenerVptr(videoTargetListenerVtable()),
                    m_refCount(1), m_dynamic(0), m_texture(0), m_surface(0)
    {
#if defined(_M_IX86)
        static_assert(offsetof(VideoTarget, m_listenerVptr) == 4,
                      "both VideoTarget listener bodies are handed this + 4");
#endif
        ::memset(&m_desc, 0, sizeof(m_desc));
        // No test of the base global, as shipped.
        ::dice::hfe::dx9rend::registerDeviceLostResetListener(&m_listenerVptr, 1);
    }

    bool  create(unsigned int w, unsigned int h);       // sub_1015A6D0
#if defined(_M_IX86)
    void  copyFrame(::renddx9abi::HBINK hbink);          // the lock/copy/unlock triple of doFrame
#endif
    unsigned int width() const  { return m_desc.Width; }    // vslot +0x10 sub_1015A620
    unsigned int height() const { return m_desc.Height; }   // vslot +0x14 sub_1015A630
    void* d3dTexture() const { return m_texture; }
    void  destroyTexture();

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void  dtor(char flags);                              // vslot +0x18 sub_1015A840
    void* queryInterface(unsigned int iid) const;

    void  onDeviceLost();                                // listener +0x00, 0x1015A640
    void  onDeviceReset();                               // listener +0x04, 0x1015A7D0
    // The listener bodies receive the subobject; this is the shipped `this - 4` (0x1015A7E5).
    static VideoTarget* fromListener(void* sub)
    {
        return reinterpret_cast<VideoTarget*>(static_cast<char*>(sub)
                                              - offsetof(VideoTarget, m_listenerVptr));
    }

private:
    const void** m_vptr;            // IBase vtable
    const void** m_listenerVptr;    // shipped +0x04: the DeviceLostResetListener subobject
    int          m_refCount;
    unsigned char m_dynamic;        // shipped +0x0C: default+dynamic (1) vs managed (0)
    D3DSURFACE_DESC m_desc;         // shipped +0x10: level 0's description, read by doFrame
    IDirect3DTexture9* m_texture;   // shipped +0x30
    IDirect3DSurface9* m_surface;   // shipped +0x34
};

VideoTarget* createVideoTargetSized(unsigned int w, unsigned int h)
{
    void* mem = ::renddx9abi::engineAlloc(sizeof(VideoTarget));
    if (!mem) return 0;
    VideoTarget* t = new (mem) VideoTarget();
    if (!t->create(w, h)) { t->release(); return 0; }
    return t;
}

// sub_1015A6D0, in the shipped order: release the previous surface then texture, ask the device for
// its caps, choose default+D3DUSAGE_DYNAMIC when D3DCAPS2_DYNAMICTEXTURES is advertised and managed
// otherwise (both X8R8G8B8, one level), fail on any nonzero D3DX result, take level 0's surface and
// cache its description. The shipped AddRef/Release pairs around GetSurfaceLevel are a smart-pointer
// artefact with no net effect and are not reproduced.
bool VideoTarget::create(unsigned int w, unsigned int h)
{
    destroyTexture();
    IDirect3DDevice9* dev = static_cast<IDirect3DDevice9*>(::dice::hfe::dx9rend::renderDevice());
    if (!dev)
        return false;       // [CHOSEN] the shipped body dereferences the device unconditionally;
                            // the harness host can be device-less.
    D3DCAPS9 caps;
    ::memset(&caps, 0, sizeof(caps));
    dev->GetDeviceCaps(&caps);
    m_dynamic = (caps.Caps2 & D3DCAPS2_DYNAMICTEXTURES) ? 1 : 0;

    long hr;
    if (m_dynamic)
        hr = D3DXCreateTexture(dev, w, h, 1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT,
                               &m_texture);
    else
        hr = D3DXCreateTexture(dev, w, h, 1, 0, D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &m_texture);
    if (hr != 0)
        return false;

    m_texture->GetSurfaceLevel(0, &m_surface);
    m_surface->GetDesc(&m_desc);
    return true;
}

// The middle of sub_1015A950 (Video::doFrame), verbatim: lock level 0's surface with
// D3DLOCK_DISCARD only for the dynamic flavour, BinkCopyToBuffer into it as 32-bit bgra with
// binkcopyall (the whole frame every time - a discarded dynamic surface holds nothing from the
// previous frame), destheight = the surface description's height (not the movie's), then unlock.
// The shipped body does not test the lock result; reproduced as shipped.
#if defined(_M_IX86)
void VideoTarget::copyFrame(::renddx9abi::HBINK hbink)
{
    D3DLOCKED_RECT lr;
    const unsigned long lockFlags = (m_dynamic == 1) ? D3DLOCK_DISCARD : 0;
    m_surface->LockRect(&lr, 0, lockFlags);
    ::renddx9abi::BinkCopyToBuffer(hbink, lr.pBits, lr.Pitch, m_desc.Height, 0, 0,
                                   ::renddx9abi::BINK_COPYALL | ::renddx9abi::BINK_SURFACE_32BGRA);
    m_surface->UnlockRect();
}
#endif

void VideoTarget::destroyTexture()
{
    if (m_surface) { m_surface->Release(); m_surface = 0; }
    if (m_texture) { m_texture->Release(); m_texture = 0; }
}


// Releases the video surface and texture but retains their pointer values as the reset gate.
// Requires a nonnull texture: AddRef and Release occur before the null guards.

void VideoTarget::onDeviceLost()
{
    m_texture->AddRef();
    m_texture->Release();
    if (m_surface)
        m_surface->Release();
    if (m_texture)
        m_texture->Release();
}


// Recreates resources only when the old texture pointer is nonnull, clearing both stored pointers first.
// Creation failure is ignored and leaves the texture unavailable for the next device-loss callback.

void VideoTarget::onDeviceReset()
{
    if (m_texture)
    {
        const unsigned int w = m_desc.Width;               // read before the two stores
        m_surface = 0;
        m_texture = 0;
        create(w, m_desc.Height);
    }
}

// Final release unregisters before releasing surface and texture. Failed-create cleanup uses chosen null guards.
// Destroying after device loss but before reset can release dangling fields twice, as in the original.
int VideoTarget::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        dtor(1);
    return remaining;
}

void VideoTarget::dtor(char flags)
{
    m_vptr         = videoTargetVtable();
    m_listenerVptr = videoTargetListenerVtable();
    // Unguarded, as shipped.
    ::dice::hfe::dx9rend::unregisterDeviceLostResetListener(&m_listenerVptr);
    destroyTexture();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
}

void* VideoTarget::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IVideoTarget)
        return const_cast<VideoTarget*>(this);
    return 0;
}

namespace {
void  RD9_THUNK vt_addRef(void* s RD9_THUNK_EDX)  { static_cast<VideoTarget*>(s)->addRef(); }
int   RD9_THUNK vt_getRef(void* s RD9_THUNK_EDX)  { return static_cast<VideoTarget*>(s)->getRef(); }
int   RD9_THUNK vt_release(void* s RD9_THUNK_EDX) { return static_cast<VideoTarget*>(s)->release(); }
void* RD9_THUNK vt_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{ return static_cast<VideoTarget*>(s)->queryInterface(iid); }
unsigned int RD9_THUNK vt_getWidth(void* s RD9_THUNK_EDX)  { return static_cast<VideoTarget*>(s)->width(); }
unsigned int RD9_THUNK vt_getHeight(void* s RD9_THUNK_EDX) { return static_cast<VideoTarget*>(s)->height(); }
void* RD9_THUNK vt_dtor(void* s RD9_THUNK_EDX, char flags)
{ static_cast<VideoTarget*>(s)->dtor(flags); return s; }

// VideoTarget_vftable_primary 0x1021E7D4, seven slots in the shipped order.
const void* kVideoTargetVtable[7] =
{
    reinterpret_cast<void*>(&vt_addRef),          // +0x00
    reinterpret_cast<void*>(&vt_getRef),          // +0x04
    reinterpret_cast<void*>(&vt_release),         // +0x08
    reinterpret_cast<void*>(&vt_queryInterface),  // +0x0C
    reinterpret_cast<void*>(&vt_getWidth),        // +0x10 sub_1015A620
    reinterpret_cast<void*>(&vt_getHeight),       // +0x14 sub_1015A630
    reinterpret_cast<void*>(&vt_dtor),            // +0x18 sub_1015A840
};

// Listener adapters receive the secondary subobject; unused edx preserves the x86 call convention.
void RD9_THUNK vt_onDeviceLost(void* sub RD9_THUNK_EDX)  { VideoTarget::fromListener(sub)->onDeviceLost(); }
void RD9_THUNK vt_onDeviceReset(void* sub RD9_THUNK_EDX) { VideoTarget::fromListener(sub)->onDeviceReset(); }
void RD9_THUNK vt_onDeviceDestroy(void* RD9_THUNK_EDX)   { /* 0x1015A680: ret */ }

const void* kVideoTargetListenerVtable[3] =
{
    reinterpret_cast<void*>(&vt_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&vt_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&vt_onDeviceDestroy), // +0x08
};
} // anonymous namespace
const void** videoTargetVtable() { return kVideoTargetVtable; }
const void** videoTargetListenerVtable() { return kVideoTargetListenerVtable; }


// Video - the hbink wrapper.

const void** videoVtable();
const void** videoListenerVtable();

class Video
{
public:
    // Initializes empty movie state, then registers the listener at priority 5. Requires the renderer registry.
    Video() : m_vptr(videoVtable()), m_listenerVptr(videoListenerVtable()), m_refCount(1),
              m_hbink(0), m_width(0), m_height(0), m_target(0), m_paused(false)
    {
#if defined(_M_IX86)
        static_assert(offsetof(Video, m_listenerVptr) == 4,
                      "both Video listener bodies are handed this + 4");
#endif
        ::dice::hfe::dx9rend::registerDeviceLostResetListener(&m_listenerVptr, 5);
    }

    unsigned int width() const  { return m_width; }      // vslot +0x10 sub_1015A8D0
    unsigned int height() const { return m_height; }     // vslot +0x14 sub_1015A8E0
    bool         open(const char* path);                 // vslot +0x18 sub_1015AA90
    VideoTarget* frame() const { return m_target; }      // borrowed by internal texture readers
    VideoTarget* acquireFrame() const;                  // vslot +0x1C: caller owns a reference
    void         doFrame();                              // vslot +0x20 sub_1015A950
    const HostString& path() const { return m_path; }    // vslot +0x24 sub_1015A9E0
    bool         initSound();                            // vslot +0x28 sub_1015A8F0
    void         shutdownSound() {}                      // shipped sound-shutdown slot is empty
    bool         isDone() const;                         // vslot +0x30 sub_1015A920
    void         dtor(char flags);                       // vslot +0x34 sub_1015ACD0
    void         onDeviceLost();                         // listener +0x00, 0x1015A890
    void         onDeviceReset();                        // listener +0x04, 0x1015A8B0
    // The listener bodies receive the subobject (`this + 4`; 0x1015A890 writes the paused byte as
    // [ecx+39h] = +0x3D and reads the handle as [ecx+28h] = +0x2C).
    static Video* fromListener(void* sub)
    {
        return reinterpret_cast<Video*>(static_cast<char*>(sub) - offsetof(Video, m_listenerVptr));
    }

    void  addRef() { ++m_refCount; }
    int   getRef() const { return m_refCount; }
    int   release();
    void* queryInterface(unsigned int iid) const;

private:
#if defined(_M_IX86)
    bool  openUnderRoot(const char* root, const char* path);
#endif
    void  close();

    const void** m_vptr;
    const void** m_listenerVptr;      // shipped +0x04: the DeviceLostResetListener subobject
    int          m_refCount;
    HostString   m_path;              // shipped +0x10
    ::renddx9abi::HBINK m_hbink;      // shipped +0x2C
    unsigned int m_width;             // shipped +0x30
    unsigned int m_height;            // shipped +0x34
    VideoTarget* m_target;            // shipped +0x38
    bool         m_paused;            // shipped +0x3D
};

#if defined(_M_IX86)
namespace {

// Borrows the file manager's host-ABI path list. Visit it through the per-flavor adapter, not this image's STL layout.
const ::renddx9abi::HostStringList* fileManagerPathList()
{
    if (!::dice::hfe::fileManager)
        return 0;
    // m_pathList is an inline member in both images (ELF `add eax,38h`, PE `lea eax,[ecx+5Ch]`),
    // so getPathList returns the list object itself rather than a pointer stored inside it.
    return static_cast<const ::renddx9abi::HostStringList*>(
        ::dice::hfe::fileManager->getPathList());
}

// One pass of the shipped search: root + entry + path, first BinkOpen that succeeds wins. The
// visitor is passed by value, so the result lives behind a pointer.
struct BinkOpenVisitor
{
    const char*          root;
    const char*          path;
    ::renddx9abi::HBINK* opened;

    void operator()(const HostString& entry)
    {
        if (*opened)
            return;
        char candidate[512];
        ::_snprintf_s(candidate, sizeof(candidate), _TRUNCATE, "%s%s%s", root, entry.c_str(), path);
        *opened = ::renddx9abi::BinkOpen(candidate, 0);
    }
};

} // anonymous namespace

bool Video::openUnderRoot(const char* root, const char* path)
{
    ::renddx9abi::HBINK opened = 0;
    BinkOpenVisitor visitor;
    visitor.root   = root;
    visitor.path   = path;
    visitor.opened = &opened;
    ::renddx9abi::visitHostStringList(fileManagerPathList(), visitor);
    m_hbink = opened;
    return m_hbink != 0;
}
#else
namespace {

// [CHOSEN, not recovered] The x64 image has no Bink decoder (issue #7). Each movie name is reported
// once, so a menu that asks again for the same background does not repeat the line.
void reportMovieSkipped(const char* path)
{
    static std::set<std::string> reported;
    if (!reported.insert(std::string(path)).second)
        return;
    ::renddx9abi::logf("Video::open(\"%s\"): movie skipped - the x64 build has no Bink video "
                       "decoder (issue #7); reported once per movie", path);
}

} // anonymous namespace
#endif

// Closes the previous movie, then searches executable-rooted mod paths. Missing files return false.
// Target creation failure leaves the opened movie handle for later cleanup.
// [CHOSEN, not recovered] Also searches from the current directory.
// [RETIRE-WHEN: video-open-cwd-fallback] Remove the fallback when executable and content roots use the normal search path.
// [CHOSEN, not recovered] x64 opens no movie (issue #7): it reports the skip and answers false, the
// same refusal a missing .bik produces, so every caller takes its existing no-movie path.
bool Video::open(const char* path)
{
    close();
    if (!path)
        return false;

#if defined(_M_IX86)
    char exePath[MAX_PATH];
    exePath[0] = 0;
    ::GetModuleFileNameA(0, exePath, MAX_PATH);
    char exeDir[MAX_PATH + 1];
    exeDir[0] = 0;
    {
        const char* bs = ::strrchr(exePath, '\\');
        const char* fs = ::strrchr(exePath, '/');
        const char* cut = (fs > bs) ? fs : bs;
        if (cut)
        {
            const size_t n = static_cast<size_t>(cut - exePath);
            ::memcpy(exeDir, exePath, n);
            exeDir[n]     = '\\';
            exeDir[n + 1] = 0;
        }
    }

    const char* rootUsed = "the executable directory";
    if (!openUnderRoot(exeDir, path))
    {
        char cwd[MAX_PATH + 1];
        cwd[0] = 0;
        const DWORD n = ::GetCurrentDirectoryA(MAX_PATH, cwd);
        if (n && n < MAX_PATH)
        {
            if (cwd[n - 1] != '\\' && cwd[n - 1] != '/')
            {
                cwd[n]     = '\\';
                cwd[n + 1] = 0;
            }
            rootUsed = "the working directory (RW-105 fallback)";
            openUnderRoot(cwd, path);
        }
    }
    if (!m_hbink)
    {
        ::renddx9abi::logf("Video::open(\"%s\"): BinkOpen found no .bik under the FileManager's %u "
                           "path(s) rooted at \"%s\" or the working directory - no movie",
                           path, ::renddx9abi::hostStringListSize(fileManagerPathList()), exeDir);
        return false;
    }

    ::renddx9abi::BINKSUMMARY sum;
    ::memset(&sum, 0, sizeof(sum));
    ::renddx9abi::BinkGetSummary(m_hbink, &sum);
    m_width  = sum.Width;
    m_height = sum.Height;

    m_target = createVideoTargetSized(m_width, m_height);
    if (!m_target)
    {
        ::renddx9abi::logf("Video::open(\"%s\"): the %ux%u VideoTarget could not be created",
                           path, m_width, m_height);
        return false;
    }
    m_path = path;
    ::renddx9abi::logf("Video::open(\"%s\"): playing %ux%u, %u frames (%s, %s texture)", path,
                       m_width, m_height, m_hbink->Frames, rootUsed,
                       m_target->width() == m_width && m_target->height() == m_height
                           ? "exact-size" : "rounded");
    return true;
#else
    reportMovieSkipped(path);
    return false;
#endif
}

// sub_1015A950: nothing while paused; BinkWait says whether the clock has reached the next frame;
// then decode, copy into the target, advance. Bink wraps FrameNum to 1 after the last frame, so a
// menu movie loops without any help from here.
// [CHOSEN, not recovered] x64 (issue #7): open() never succeeds, so there is no frame to decode.
void Video::doFrame()
{
#if defined(_M_IX86)
    if (m_paused || ::renddx9abi::BinkWait(m_hbink))
        return;
    ::renddx9abi::BinkDoFrame(m_hbink);
    m_target->copyFrame(m_hbink);
    ::renddx9abi::BinkNextFrame(m_hbink);
#endif
}

// sub_1015A8F0 - the one-time Bink sound-system selection. FSMoviePlayer::load calls this slot
// (+0x28) before open; SwiffRender's '?' arm does not, so a menu started with +restart (intros
// skipped) plays its background silent, as retail does. The shipped body answers true
// unconditionally and no caller reads it.
// [CHOSEN, not recovered] x64 (issue #7) has no Bink sound system to select.
bool Video::initSound()
{
#if defined(_M_IX86)
    ::renddx9abi::BinkSetSoundSystem(
        reinterpret_cast<void*>(&::renddx9abi::BinkOpenDirectSound), 0);
#endif
    return true;
}

// Returns an owned reference to the current frame target, or null before a successful open.
VideoTarget* Video::acquireFrame() const
{
    if (m_target)
        m_target->addRef();
    return m_target;
}

// sub_1015A920 - `FrameNum >= Frames`, read straight off the handle.
// [CHOSEN, not recovered] x64 (issue #7): no movie is ever open, so playback is finished.
bool Video::isDone() const
{
#if defined(_M_IX86)
    return m_hbink->FrameNum >= m_hbink->Frames;
#else
    return true;
#endif
}

// Pauses playback before the target texture is released; reset resumes it after target recreation.
// x64 holds no Bink handle (issue #7), so only the paused byte changes there.
void Video::onDeviceLost()
{
    m_paused = true;
#if defined(_M_IX86)
    if (m_hbink)
        ::renddx9abi::BinkPause(m_hbink, 1);
#endif
}

void Video::onDeviceReset()
{
    m_paused = false;
#if defined(_M_IX86)
    if (m_hbink)
        ::renddx9abi::BinkPause(m_hbink, 0);
#endif
}

// The head of sub_1015AA90 and the middle of sub_1015ACD0: release the target, close the handle.
// The shipped open() releases the target without clearing the field and re-assigns it only after a
// successful BinkOpen, so a second open() that fails would leave a released pointer for the
// destructor; no caller re-opens a Video (both creators make a fresh one per movie), and this
// [CHOSEN] version clears both fields.
void Video::close()
{
    if (m_target) { m_target->release(); m_target = 0; }
#if defined(_M_IX86)
    if (m_hbink)  { ::renddx9abi::BinkClose(m_hbink); m_hbink = 0; }
#endif
}

int Video::release()
{
    const int remaining = --m_refCount;
    if (!remaining)
        dtor(1);
    return remaining;
}

// Unregisters the listener, releases the target, closes Bink, and frees when flags bit zero is set.
// Unlike close's [CHOSEN] guard, the destructor calls BinkClose unconditionally.
void Video::dtor(char flags)
{
    m_vptr         = videoVtable();
    m_listenerVptr = videoListenerVtable();
    // Unguarded, as shipped.
    ::dice::hfe::dx9rend::unregisterDeviceLostResetListener(&m_listenerVptr);
    close();
    m_path.~HostString();
    if (flags & 1)
        ::renddx9abi::engineFree(this);
}

// Accepts the base and movie interfaces without changing the object pointer.
void* Video::queryInterface(unsigned int iid) const
{
    if (iid == RD9_IID_IBASE || iid == IID_IVideo)
        return const_cast<Video*>(this);
    return 0;
}

namespace {
void  RD9_THUNK vid_addRef(void* s RD9_THUNK_EDX)  { static_cast<Video*>(s)->addRef(); }
int   RD9_THUNK vid_getRef(void* s RD9_THUNK_EDX)  { return static_cast<Video*>(s)->getRef(); }
int   RD9_THUNK vid_release(void* s RD9_THUNK_EDX) { return static_cast<Video*>(s)->release(); }
void* RD9_THUNK vid_queryInterface(void* s RD9_THUNK_EDX, unsigned int iid)
{ return static_cast<Video*>(s)->queryInterface(iid); }
unsigned int  RD9_THUNK vid_getWidth(void* s RD9_THUNK_EDX)  { return static_cast<Video*>(s)->width(); }
unsigned int  RD9_THUNK vid_getHeight(void* s RD9_THUNK_EDX) { return static_cast<Video*>(s)->height(); }
unsigned char RD9_THUNK vid_open(void* s RD9_THUNK_EDX, const HostString* path)
{ return static_cast<Video*>(s)->open(path ? path->c_str() : 0) ? 1 : 0; }
void* RD9_THUNK vid_getFrame(void* s RD9_THUNK_EDX) { return static_cast<Video*>(s)->acquireFrame(); }
void  RD9_THUNK vid_doFrame(void* s RD9_THUNK_EDX)  { static_cast<Video*>(s)->doFrame(); }
const HostString* RD9_THUNK vid_getPath(void* s RD9_THUNK_EDX)
{ return &static_cast<Video*>(s)->path(); }
unsigned char RD9_THUNK vid_initSound(void* s RD9_THUNK_EDX)
{ return static_cast<Video*>(s)->initSound() ? 1 : 0; }
void  RD9_THUNK vid_shutdownSound(void* s RD9_THUNK_EDX) { static_cast<Video*>(s)->shutdownSound(); }
int   RD9_THUNK vid_isDone(void* s RD9_THUNK_EDX) { return static_cast<Video*>(s)->isDone() ? 1 : 0; }
void* RD9_THUNK vid_dtor(void* s RD9_THUNK_EDX, char flags)
{ static_cast<Video*>(s)->dtor(flags); return s; }

// Video_vftable_primary 0x1021E81C, fourteen slots in the shipped order.
const void* kVideoVtable[14] =
{
    reinterpret_cast<void*>(&vid_addRef),          // +0x00
    reinterpret_cast<void*>(&vid_getRef),          // +0x04
    reinterpret_cast<void*>(&vid_release),         // +0x08 Video_release
    reinterpret_cast<void*>(&vid_queryInterface),  // +0x0C Video_queryInterfaceById
    reinterpret_cast<void*>(&vid_getWidth),        // +0x10 sub_1015A8D0
    reinterpret_cast<void*>(&vid_getHeight),       // +0x14 sub_1015A8E0
    reinterpret_cast<void*>(&vid_open),            // +0x18 sub_1015AA90
    reinterpret_cast<void*>(&vid_getFrame),        // +0x1C sub_1015A930
    reinterpret_cast<void*>(&vid_doFrame),         // +0x20 sub_1015A950
    reinterpret_cast<void*>(&vid_getPath),         // +0x24 sub_1015A9E0
    reinterpret_cast<void*>(&vid_initSound),       // +0x28 sub_1015A8F0
    reinterpret_cast<void*>(&vid_shutdownSound),   // +0x2C nullsub_77
    reinterpret_cast<void*>(&vid_isDone),          // +0x30 sub_1015A920
    reinterpret_cast<void*>(&vid_dtor),            // +0x34 sub_1015ACD0
};

// The DeviceLostResetListener table, 0x1021E810 (the three dwords before the primary table).
void RD9_THUNK vid_onDeviceLost(void* sub RD9_THUNK_EDX)  { Video::fromListener(sub)->onDeviceLost(); }
void RD9_THUNK vid_onDeviceReset(void* sub RD9_THUNK_EDX) { Video::fromListener(sub)->onDeviceReset(); }
void RD9_THUNK vid_onDeviceDestroy(void* RD9_THUNK_EDX)   { /* 0x10037D40: ret, the shared nullsub */ }

const void* kVideoListenerVtable[3] =
{
    reinterpret_cast<void*>(&vid_onDeviceLost),    // +0x00
    reinterpret_cast<void*>(&vid_onDeviceReset),   // +0x04
    reinterpret_cast<void*>(&vid_onDeviceDestroy), // +0x08
};
} // anonymous namespace
const void** videoVtable() { return kVideoVtable; }
const void** videoListenerVtable() { return kVideoListenerVtable; }


// RefFactories (registered in getClassList) and the C-style seams FSMoviePlayer + SwiffRender reach.

IBase* __fastcall createVideo(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* mem = ::renddx9abi::engineAlloc(sizeof(Video));
    if (!mem) return 0;
    Video* self = new (mem) Video();
    void* itf = self->queryInterface(iid);
    if (!itf) self->release();
    return static_cast<IBase*>(itf);
}

IBase* __fastcall createVideoTarget(unsigned int iid, IBase* outer)
{
    if (outer) return 0;
    void* mem = ::renddx9abi::engineAlloc(sizeof(VideoTarget));
    if (!mem) return 0;
    VideoTarget* self = new (mem) VideoTarget();
    void* itf = self->queryInterface(iid);
    if (!itf) self->release();
    return static_cast<IBase*>(itf);
}

// Seams: the Video* comes back from createInstanceByName as an IBase* that is the Video (QI returns
// this), so a plain cast is valid.
void  videoInitSound(void* video)              { static_cast<Video*>(video)->initSound(); }
void  videoShutdownSound(void* video)          { static_cast<Video*>(video)->shutdownSound(); }
void* videoAcquireFrame(void* video)           { return static_cast<Video*>(video)->acquireFrame(); }
void* videoTargetD3DTexture(void* target)       { return static_cast<VideoTarget*>(target)->d3dTexture(); }
bool  videoOpen(void* video, const char* path) { return video && static_cast<Video*>(video)->open(path); }
void  videoDoFrame(void* video)                { if (video) static_cast<Video*>(video)->doFrame(); }
int   videoIsDone(void* video)                 { return video && static_cast<Video*>(video)->isDone() ? 1 : 0; }
void* videoFrameD3DTexture(void* video)
{
    if (!video) return 0;
    VideoTarget* t = static_cast<Video*>(video)->frame();
    return t ? t->d3dTexture() : 0;
}

// SwiffRender's '?' movie-fill: the shipped arm creates "dice.hfe.geom.Video" by name and calls its
// vslot +0x18 open(path); one seam does both. Returns the Video (an IBase* == Video*) or null; the
// record's vslot +0x08 release tears it down.
void* videoCreateAndOpen(const char* path)
{
    void* mem = ::renddx9abi::engineAlloc(sizeof(Video));
    if (!mem) return 0;
    Video* v = new (mem) Video();
    if (!v->open(path)) { v->release(); return 0; }
    return v;
}

} // namespace geom
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
