// Tracks vertex/index buffers and cached vertex declarations across device loss/reset.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdlib>
#include <stddef.h>
#include <string.h>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace dice {
namespace hfe {
namespace dx9rend {

extern IBase* g_Renderer;   // BF2Render.cpp - the resource counters live off it
int bitsPerPixel(unsigned int d3dFormat); // RendDX9Types.cpp, sub_1000CD30


// Device-listener owner for tracked buffers. The pool word is a D3DPOOL, not a refcount;
// the manager has no IBase interface.


namespace {

// The two trees whose element types are unrecovered (see BufferManager). Retail nodes are 0x34 and
// 0x30 bytes, carrying 36 and 32 bytes of payload; nothing recovered traverses them, so the
// key/value split is [UNRESOLVED] and only their construction is reproduced.
struct BmUnresolvedValue32
{
    unsigned int words[8];
};

struct BmUnresolvedValue28
{
    unsigned int words[7];
};

struct VertexDeclaration;

// The vertex-declaration cache key: the element list without its D3DDECL_END terminator.
typedef std::vector<D3DVERTEXELEMENT9> DeclarationKey;

// A key as a lookup sees it, before (or without) copying it into a DeclarationKey.
struct DeclarationKeyView
{
    const D3DVERTEXELEMENT9* elements;
    unsigned int             count;
};

inline DeclarationKeyView viewOf(const DeclarationKey& key)
{
    const DeclarationKeyView view = { key.data(), static_cast<unsigned int>(key.size()) };
    return view;
}

// The element count first, then the element bytes.
int compareDeclarationKey(const DeclarationKeyView& a, const DeclarationKeyView& b)
{
    if (a.count != b.count)
        return a.count > b.count ? 1 : -1;
    const int compared = ::memcmp(a.elements, b.elements, a.count * sizeof(D3DVERTEXELEMENT9));
    return compared > 0 ? 1 : (compared < 0 ? -1 : 0);
}

// sub_1004A5F0 is std::greater<vector<...> >: a greater key sorts first (descends left).
struct DeclarationKeyGreater
{
    typedef void is_transparent;

    bool operator()(const DeclarationKey& a, const DeclarationKey& b) const
    {
        return compareDeclarationKey(viewOf(a), viewOf(b)) > 0;
    }
    bool operator()(const DeclarationKeyView& a, const DeclarationKey& b) const
    {
        return compareDeclarationKey(a, viewOf(b)) > 0;
    }
    bool operator()(const DeclarationKey& a, const DeclarationKeyView& b) const
    {
        return compareDeclarationKey(viewOf(a), b) > 0;
    }
};

typedef std::map<DeclarationKey, VertexDeclaration*, DeclarationKeyGreater> DeclarationCache;

} // anonymous namespace

class BufferManager : public DeviceLostResetListener
{
public:
    explicit BufferManager(IDirect3DDevice9* device);

    IDirect3DDevice9* device() const { return m_device; }
    unsigned int      pool()   const { return m_pool; }
    // sub_100113B0 - the swap-setter the QuadDrawer (0) and FSMoviePlayer (1) bracket creates with.
    int               swapPool(int v) { const int old = static_cast<int>(m_pool); m_pool = static_cast<unsigned int>(v); return old; }

    // The complete three-slot listener table at 0x101DE2B4.
    virtual void onDeviceLost();
    virtual void onDeviceReset();
    virtual void onDeviceDestroy();

    // Initialization intentionally succeeds without additional work.
    bool init() { return true; }

    // The four buffer-tracking sets, by index: 0 managed vertex buffers, 1 default-pool vertex
    // buffers, 2 managed index buffers, 3 default-pool index buffers.
    std::set<void*>&  bufferSet(unsigned int index) { return m_bufferSets[index]; }
    DeclarationCache& declarations() { return m_declarations; }

private:
    IDirect3DDevice9* m_device;   // +0x04
    // Default D3D allocation pool; also selects the buffer tracking set.
    unsigned int      m_pool;     // +0x08  D3DPOOL_MANAGED (1) - see the defect note above
    // Retail std::set<Buffer*> instances with 0x14-byte pointer-key nodes, ordered by address.
    std::set<void*>   m_bufferSets[4];   // +0x0C / +0x18 / +0x24 / +0x30
    DeclarationCache  m_declarations;    // +0x3C  0x24-byte nodes
    // [UNRESOLVED] element types: two trees the constructor builds (0x34- and 0x30-byte nodes) that
    // nothing recovered traverses.
    std::map<unsigned int, BmUnresolvedValue32> m_unresolvedTree34;   // +0x48
    std::map<unsigned int, BmUnresolvedValue28> m_unresolvedTree30;   // +0x54
};


// complete.

BufferManager::BufferManager(IDirect3DDevice9* device)
    : m_device(0)
    , m_pool(1)          // D3DPOOL_MANAGED
{
    // The four sets, the declaration cache and the two unresolved trees are constructed as members,
    // in the constructor's own order.

    // Retain and store the device after constructing the lists.
    if (device)
        device->AddRef();
    m_device = device;
}

// dword_1023B978 in the shipped image. initializeD3DSubsystems publishes it here and
// geom::Font::Ported's constructor reads it directly.
void* g_bufferManager = 0;

// RendDX9Init.cpp:280 / :285 - allocate, construct, init, then register as a device-lost listener.
bool createBufferManager(IDirect3DDevice9* device)
{
    BufferManager* bm = new BufferManager(device);
    g_bufferManager = bm;

    if (!bm->init())
    {
        ::renddx9abi::logf("RendDX9Init.cpp:285 Failed to init Buffermanager.");
        return false;
    }

    // RendDX9Init.cpp:287, immediately after init: priority 3.
    registerDeviceLostResetListener(static_cast<DeviceLostResetListener*>(bm), 3);
    return true;
}


// Ref-counted vertex-buffer wrapper with cached descriptor and stride.
// Shares the three-slot reference table with Query.

namespace {

struct D3dVertexBufferDesc      // D3DVERTEXBUFFER_DESC, 24 bytes
{
    unsigned int format, type, usage, pool, size, fvf;
};

struct VertexBuffer
{
    void*                    vptr;      // +0x00
    int                      refCount;  // +0x04
    IDirect3DVertexBuffer9*  buffer;    // +0x08
    unsigned int             dword0C;   // +0x0C
    D3dVertexBufferDesc      desc;      // +0x10 .. +0x27
    unsigned int             stride;    // +0x28
};

// Retail x86 evidence. createVertexBuffer allocates sizeof(VertexBuffer); other TUs reach the fields
// through the accessors below, so the x64 wrapper is native (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(VertexBuffer) == 0x2C, "createVertexBuffer allocates 0x2C - see 0x100120A0");
#endif
static_assert(sizeof(D3dVertexBufferDesc) == sizeof(D3DVERTEXBUFFER_DESC),
              "GetDesc writes the D3D descriptor over the wrapper's copy");

// Both buffer wrappers share this +4 reference-count getter (IndexBuffer keeps refCount at the
// same offset; see the assert after its definition).
int   RD9_THUNK vb_getRef(void* self RD9_THUNK_EDX)
{ return static_cast<VertexBuffer*>(self)->refCount; }
void  RD9_THUNK vb_nullsub(void* RD9_THUNK_EDX) { }
void  RD9_THUNK vb_addRef(void* s RD9_THUNK_EDX) { ++static_cast<VertexBuffer*>(s)->refCount; }

void* const kVertexBufferVtable[3] =
{
    reinterpret_cast<void*>(&vb_getRef),    // +0x00
    reinterpret_cast<void*>(&vb_nullsub),   // +0x04  0x100E13B0  nullsub
    reinterpret_cast<void*>(&vb_addRef)     // +0x08  createVertexBuffer calls this
};

// The renderer's resource counters. sub_1000D840/860/870 are one instruction each - ++ and -- on
// two dwords of the sub-object Renderer vslot +0x6C returns (`return this + 264`).
void bumpRendererCounter(unsigned int dwordIndex, int delta)
{
    if (!g_Renderer)
        return;
    unsigned char* sub = reinterpret_cast<RendDX9Base*>(g_Renderer)->renderStats;
    *reinterpret_cast<int*>(sub + dwordIndex * 4) += delta;
}

std::set<void*>& pointerSet(BufferManager* manager, unsigned int listIndex)
{
    return manager->bufferSet(listIndex);
}

void retainInPointerSet(BufferManager* manager, unsigned int listIndex, void* object,
                        int& referenceCount)
{
    // PE sub_10013600 performs std::set::insert and the factory then takes the manager reference.
    if (!pointerSet(manager, listIndex).insert(object).second)
    {
        ::renddx9abi::logf("BufferManager tracking-set insertion failed for 0x%08X",
                           ::renddx9abi::addr(object));
        std::abort();
    }
    ++referenceCount;
}

// The old buffer is released first, then the new one AddRef'd, stored, and described.
void vertexBufferSetBuffer(VertexBuffer* self, IDirect3DVertexBuffer9* vb)
{
    if (self->buffer)
    {
        self->buffer->Release();
        self->buffer = 0;
        bumpRendererCounter(8, -1);
    }
    if (!vb)
        return;

    vb->AddRef();
    self->buffer = vb;

    // Store the descriptor in the wrapper itself.
    vb->GetDesc(reinterpret_cast<D3DVERTEXBUFFER_DESC*>(&self->desc));

    bumpRendererCounter(8, +1);

    // NOT reproduced: sub_10047E70, the resource tracker. The original registers
    // (desc.Size, name, vb, &desc) into a statistics registry that only an unwritten debug view
    // reads. It is bookkeeping, not rendering - the same class of omission as OverlayTexture's four
    // acquisitions - so it is logged once rather than aborting a functional path.
    // [FRONTIER] dice::hfe::dx9rend - the resource tracker (sub_10047E70 / sub_10048AE0)
}

} // anonymous namespace


// Creates a counted vertex-buffer wrapper using the manager's current pool.
// Out-of-memory returns null; other D3D failures log and still produce a wrapper with no buffer.
// The name is borrowed here rather than passed by value as in the compatibility signature.
// [INFERRED] dynamic/managed failure behavior follows the selected pool and usage flags.

void* createVertexBuffer(void* bufferManager, unsigned int count, unsigned int stride,
                         const std::string& name, unsigned int usage)
{
    RD9_TRACE_AT("dice::hfe::dx9rend::BufferManager::createVertexBuffer", bufferManager, 0x00);

    BufferManager* bm = static_cast<BufferManager*>(bufferManager);
    if (!bm || !bm->device())
        return 0;

    IDirect3DVertexBuffer9* vb = 0;
    const long hr = bm->device()->CreateVertexBuffer(count * stride, usage, 0,
                                                     static_cast<D3DPOOL>(bm->pool()), &vb, 0);
    if (hr < 0)
    {
        if (hr == static_cast<long>(0x8876017C) || hr == static_cast<long>(0x8007000E))
            return 0;   // out of video memory / out of memory - the silent arms

        ::renddx9abi::logf("BufferManager.cpp:70 Failed to create vertex buffer (\"%s\", %u x %u, "
                           "usage 0x%X, hr 0x%08X)", name.c_str(), count, stride, usage,
                           static_cast<unsigned>(hr));
        // and the original carries on regardless - see the note above.
    }

    void* storage = ::renddx9abi::engineAlloc(sizeof(VertexBuffer));
    if (!storage)
        return 0;

    VertexBuffer* self = static_cast<VertexBuffer*>(storage);
    self->vptr     = const_cast<void**>(kVertexBufferVtable);
    self->refCount = 1;
    self->buffer   = 0;
    self->dword0C  = 0;
    self->stride   = 0;
    for (int i = 0; i < 6; ++i)
        reinterpret_cast<unsigned int*>(&self->desc)[i] = 0;
    bumpRendererCounter(7, +1);              // sub_1000D840, the ctor's tail

    vertexBufferSetBuffer(self, vb);
    self->stride = stride;

    if (vb)
        vb->Release();                       // the wrapper holds the only reference now

    // Track default-pool buffers separately because device loss requires recreating them.
    retainInPointerSet(bm, bm->pool() ? 0u : 1u, self, self->refCount);

    ::renddx9abi::logf("createVertexBuffer(\"%s\"): %u x %u = %u bytes, usage 0x%X, pool %u -> "
                       "wrapper 0x%08X buffer 0x%08X", name.c_str(), count, stride, count * stride,
                       usage, bm->pool(), ::renddx9abi::addr(self), ::renddx9abi::addr(vb));
    return self;
}


// Sets the default buffer pool and returns its previous value.
// Dynamic buffers require the caller to select D3DPOOL_DEFAULT during creation.
int bufferManagerSwapPool(int value)
{
    BufferManager* bm = static_cast<BufferManager*>(g_bufferManager);
    if (!bm)
        return 0;
    return bm->swapPool(value);
}

// The by-global convenience the QuadDrawer's ctor tail uses.
void* bufferManagerCreateVertexBuffer(unsigned int count, unsigned int stride,
                                      unsigned int usage, const char* name)
{
    return createVertexBuffer(g_bufferManager, count, stride, std::string(name), usage);
}

// Locks in vertex units and returns null on failure. Dynamic discard locks pass byte size zero.
// Successful locks increment the wrapper's lock count.
void* vertexBufferLock(void* wrapper, unsigned int offsetVerts, unsigned int countVerts,
                       unsigned int flags)
{
    VertexBuffer* self = static_cast<VertexBuffer*>(wrapper);
    const unsigned int stride  = self->stride;
    unsigned int byteOffset    = offsetVerts * stride;
    unsigned int byteSize      = countVerts * stride;
    if ((self->desc.usage & 0x200) != 0 && (flags & 0x2000) != 0)
        byteSize = 0;

    void* mapped = 0;
    if (!self->buffer ||
        self->buffer->Lock(byteOffset, byteSize, &mapped, flags) < 0)
        return 0;
    ++self->dword0C;                                       // +0x0C - the lock count (resolved here)
    return mapped;
}

// sub_10014690 - unlock: only with an outstanding lock, and only counting down on success.
bool vertexBufferUnlock(void* wrapper)
{
    VertexBuffer* self = static_cast<VertexBuffer*>(wrapper);
    if (!self->dword0C || !self->buffer || self->buffer->Unlock() < 0)
        return false;
    --self->dword0C;
    return true;
}

// sub_100146C0 - bind this buffer to a device stream through THE statemanager
// (vslot +0x58: stream, D3D buffer, offset 0, the wrapper's stride). Nothing here touches the
// device; the binding is issued by the next draw's pre-draw apply.
void vertexBufferBindStream(void* wrapper, unsigned int stream)
{
    VertexBuffer* self = static_cast<VertexBuffer*>(wrapper);
    stateManagerSetStreamSource(stream, self->buffer, 0, self->stride);
}

// sub_10014700 / sub_10014710 - read in full: the stride
// (`mov eax, [ecx+28h]`) and the vertex count, the created size over the stride
// (`mov eax, [ecx+20h]; xor edx, edx; div dword [ecx+28h]` - unsigned). Their shipped users are the
// TerrainEditable raw writer (count * stride bytes of a locked buffer),
// sub_1008E890 and three call sites IDA holds no function for (0x101073BC / 0x1010740C /
// 0x1010745C); nothing in the rebuilt game path calls either yet.
unsigned int vertexBufferStride(void* wrapper)
{
    return static_cast<VertexBuffer*>(wrapper)->stride;
}

unsigned int vertexBufferVertexCount(void* wrapper)
{
    const VertexBuffer* const self = static_cast<const VertexBuffer*>(wrapper);
    return self->desc.size / self->stride;
}

// [CHOSEN, not recovered] (issue #7) typed reads of the wrapper fields other TUs used to take by
// retail offset: the D3D buffer (+0x08) and the created byte size (desc.Size, +0x20).
IDirect3DVertexBuffer9* vertexBufferD3D(void* wrapper)
{
    return static_cast<VertexBuffer*>(wrapper)->buffer;
}

unsigned int vertexBufferByteSize(void* wrapper)
{
    return static_cast<VertexBuffer*>(wrapper)->desc.size;
}


// Adds an owner reference to the shared vertex-buffer wrapper.
void vertexBufferAddRef(void* wrapper) { vb_addRef(wrapper RD9_THUNK_ARG); }

// The shipped call sites release a VertexBuffer wrapper inline: `if (!--wrapper[1])
// { sub_10014510(wrapper); engineFree(wrapper); }`. sub_10014510, read in full: restore the base
// vptr, unregister the D3D buffer from the resource tracker (sub_10047EE0(1, vb) - the tracker is
// the frontier noted at vertexBufferSetBuffer), Release it, and decrement the renderer's wrapper
// counter (dword 7). This seam carries the whole dance so callers stay one line.
void vertexBufferRelease(void* wrapper)
{
    VertexBuffer* self = static_cast<VertexBuffer*>(wrapper);
    if (--self->refCount)
        return;
    if (self->buffer)
    {
        // sub_10047EE0(1, buffer) - the resource tracker's unregister; the tracker itself is the
        // standing [FRONTIER] above, so there is nothing registered to remove.
        self->buffer->Release();
        self->buffer = 0;
    }
    bumpRendererCounter(7, -1);
    ::renddx9abi::engineFree(self);
}


// dice::hfe::dx9rend::IndexBuffer - 36 bytes (0x24), recovered for FSQuadDrawer.

// PE 0x10012130 allocates this exact size after IDirect3DDevice9::CreateIndexBuffer.  Its wrapper
// has the same three-slot {getRef,nullsub,addRef} table as VertexBuffer, followed by the D3D buffer,
// a lock count and the 20-byte D3DINDEXBUFFER_DESC.  The create path gives the BufferManager's
// tracking set one counted reference, just as createVertexBuffer does.

namespace {

struct IndexBuffer
{
    void*                   vptr;       // +0x00
    int                     refCount;   // +0x04
    IDirect3DIndexBuffer9*  buffer;     // +0x08
    unsigned int            lockCount;  // +0x0C
    D3DINDEXBUFFER_DESC     desc;       // +0x10 .. +0x23
};

// Retail x86 evidence; bufferManagerCreateIndexBuffer allocates sizeof(IndexBuffer) (issue #7).
#if defined(_M_IX86)
static_assert(sizeof(IndexBuffer) == 0x24,
              "BufferManager::createIndexBuffer allocates 0x24 at PE 0x100121B0");
#endif
static_assert(offsetof(IndexBuffer, refCount) == offsetof(VertexBuffer, refCount) &&
              offsetof(IndexBuffer, buffer) == offsetof(VertexBuffer, buffer),
              "the shared {getRef, nullsub, addRef} table reads both wrappers alike");

void indexBufferSetBuffer(IndexBuffer* self, IDirect3DIndexBuffer9* ib)
{
    // sub_10014340: release the old resource first, retain/describe the new resource, and keep the
    // wrapper's outstanding-lock count reset.  The omitted resource-tracker rows are the same
    // already named BufferManager frontier as the VertexBuffer wrapper above.
    if (self->buffer)
    {
        self->buffer->Release();
        self->buffer = 0;
        bumpRendererCounter(10, -1);
    }
    self->lockCount = 0;
    if (!ib)
        return;

    ib->AddRef();
    self->buffer = ib;
    ib->GetDesc(&self->desc);
    bumpRendererCounter(10, +1);
}

} // anonymous namespace

// BufferManager_createIndexBuffer, PE 0x10012130.  `count` is an index count, so the byte size is
// count * bitsPerPixel(format) / 8.  OOM returns null silently; every other failing HRESULT emits
// the shipped line-191 error and continues into the original null-resource fault path.
void* bufferManagerCreateIndexBuffer(unsigned int count, unsigned int usage,
                                     unsigned int format)
{
    BufferManager* bm = static_cast<BufferManager*>(g_bufferManager);
    IDirect3DIndexBuffer9* ib = 0;
    const unsigned int bytes = count * static_cast<unsigned int>(bitsPerPixel(format)) / 8u;
    const long hr = bm->device()->CreateIndexBuffer(
        bytes, usage, static_cast<D3DFORMAT>(format), static_cast<D3DPOOL>(bm->pool()), &ib, 0);
    if (hr < 0)
    {
        if (hr == static_cast<long>(0x8876017C) || hr == static_cast<long>(0x8007000E))
            return 0;
        ::renddx9abi::logf("BufferManager.cpp:191 Failed to create index buffer");
    }

    IndexBuffer* self = static_cast<IndexBuffer*>(
        ::renddx9abi::engineAlloc(sizeof(IndexBuffer)));
    if (!self)
        return 0;
    self->vptr      = const_cast<void**>(kVertexBufferVtable);
    self->refCount  = 1;
    self->buffer    = 0;
    self->lockCount = 0;
    ::memset(&self->desc, 0, sizeof(self->desc));
    bumpRendererCounter(9, +1);

    indexBufferSetBuffer(self, ib);
    // The PE unconditionally releases the device result after attaching it.  Preserve the missing
    // guard: a non-OOM CreateIndexBuffer failure reaches the same null dereference retail does.
    ib->Release();

    retainInPointerSet(bm, bm->pool() ? 2u : 3u, self, self->refCount);
    return self;
}

// sub_10014420 / sub_10014480: offsets and counts are in index units and are converted with the
// wrapper's format.  Like retail, HRESULTs are ignored and the lock count advances regardless.
void* indexBufferLock(void* wrapper, unsigned int offsetIndices, unsigned int countIndices,
                      unsigned int flags)
{
    IndexBuffer* self = static_cast<IndexBuffer*>(wrapper);
    const unsigned int bytesPerIndex =
        static_cast<unsigned int>(bitsPerPixel(self->desc.Format)) / 8u;
    void* mapped = 0;
    self->buffer->Lock(offsetIndices * bytesPerIndex, countIndices * bytesPerIndex,
                       &mapped, flags);
    ++self->lockCount;
    return mapped;
}

bool indexBufferUnlock(void* wrapper)
{
    IndexBuffer* self = static_cast<IndexBuffer*>(wrapper);
    self->buffer->Unlock();
    --self->lockCount;
    return true;
}

// sub_100144A0: bind the wrapped D3D object through StateManager slot +0x54.
void indexBufferBind(void* wrapper)
{
    IndexBuffer* self = static_cast<IndexBuffer*>(wrapper);
    stateManagerSetIndices(self->buffer);
}

// [CHOSEN, not recovered] (issue #7) the wrapped D3D index buffer (+0x08), for other TUs.
IDirect3DIndexBuffer9* indexBufferD3D(void* wrapper)
{
    return static_cast<IndexBuffer*>(wrapper)->buffer;
}

// The inline owner-side decrement around sub_10014300.
void indexBufferRelease(void* wrapper)
{
    IndexBuffer* self = static_cast<IndexBuffer*>(wrapper);
    if (--self->refCount)
        return;
    if (self->buffer)
    {
        self->buffer->Release();
        self->buffer = 0;
        bumpRendererCounter(10, -1);
    }
    bumpRendererCounter(9, -1);
    ::renddx9abi::engineFree(self);
}


// Ref-counted vertex declaration cached by its element vector, excluding D3DDECL_END.
// Destruction releases the D3D object and decrements the renderer's declaration count.

namespace {

struct VertexDeclaration
{
    void**                          vptr;       // +0x00
    int                             refCount;   // +0x04
    IDirect3DVertexDeclaration9*    decl;       // +0x08
};

void RD9_THUNK vd_getRef(void* s RD9_THUNK_EDX) { (void)s; }
void RD9_THUNK vd_nullsub(void* RD9_THUNK_EDX) { }
void RD9_THUNK vd_addRef(void* s RD9_THUNK_EDX) { ++static_cast<VertexDeclaration*>(s)->refCount; }

void* const kVertexDeclVtable[3] =
{
    reinterpret_cast<void*>(&vd_getRef),
    reinterpret_cast<void*>(&vd_nullsub),
    reinterpret_cast<void*>(&vd_addRef)
};

unsigned int declarationElementCount(const D3DVERTEXELEMENT9* elements)
{
    unsigned int count = 0;
    do
    {
        ++count;
    }
    while (elements[count].Stream != 0xFFu);
    return count;
}

} // anonymous namespace

void* vertexDeclarationCreate(const void* elements)
{
    BufferManager* bm = static_cast<BufferManager*>(g_bufferManager);
    const D3DVERTEXELEMENT9* const declaration =
        static_cast<const D3DVERTEXELEMENT9*>(elements);
    const unsigned int elementCount = declarationElementCount(declaration);
    const DeclarationKeyView key = { declaration, elementCount };
    DeclarationCache& cache = bm->declarations();
    DeclarationCache::iterator cached = cache.find(key);
    if (cached != cache.end())
    {
        ++cached->second->refCount;
        return cached->second;
    }

    IDirect3DVertexDeclaration9* decl = 0;
    bm->device()->CreateVertexDeclaration(declaration, &decl);

    VertexDeclaration* self = static_cast<VertexDeclaration*>(
        ::renddx9abi::engineAlloc(sizeof(VertexDeclaration)));
    self->vptr     = const_cast<void**>(kVertexDeclVtable);
    self->refCount = 1;
    self->decl     = decl;
    decl->AddRef();
    bumpRendererCounter(12, +1);
    decl->Release();                           // discard CreateVertexDeclaration's reference

    cache.insert(std::make_pair(DeclarationKey(declaration, declaration + elementCount), self));
    ++self->refCount;                          // declaration-cache ownership
    return self;
}

// sub_100147C0 - one instruction of dispatch: StateManager dice slot +0x5C with the D3D object.
void vertexDeclarationBind(void* wrapper)
{
    VertexDeclaration* self = static_cast<VertexDeclaration*>(wrapper);
    stateManagerSetVertexDeclaration(self->decl);
}

// [CHOSEN, not recovered] (issue #7) the wrapped D3D declaration (+0x08), for other TUs.
IDirect3DVertexDeclaration9* vertexDeclarationD3D(void* wrapper)
{
    return static_cast<VertexDeclaration*>(wrapper)->decl;
}

// The inline release dance around sub_10014790, same shape as the vertex buffer's.
void vertexDeclarationRelease(void* wrapper)
{
    VertexDeclaration* self = static_cast<VertexDeclaration*>(wrapper);
    if (--self->refCount)
        return;
    if (self->decl)
    {
        self->decl->Release();
        self->decl = 0;
    }
    bumpRendererCounter(12, -1);              // sub_10014790's tail
    ::renddx9abi::engineFree(self);
}


// BufferManager's three DeviceLostResetListener slots, PE 0x100113C0 / 0x100114A0 / 0x10011580.
// Only default-pool buffers are discarded/recreated.  Device teardown drops the manager-owned
// references in the shipped order and deliberately leaves the tree nodes in place.

void BufferManager::onDeviceLost()
{
    std::set<void*>& vertexBuffers = pointerSet(this, 1); // +0x18, default pool
    for (std::set<void*>::const_iterator node = vertexBuffers.begin();
         node != vertexBuffers.end(); ++node)
    {
        VertexBuffer* const buffer = static_cast<VertexBuffer*>(*node);
        if (buffer->buffer)
            vertexBufferSetBuffer(buffer, 0);
    }

    std::set<void*>& indexBuffers = pointerSet(this, 3);  // +0x30, default pool
    for (std::set<void*>::const_iterator node = indexBuffers.begin();
         node != indexBuffers.end(); ++node)
    {
        IndexBuffer* const buffer = static_cast<IndexBuffer*>(*node);
        if (buffer->buffer)
            indexBufferSetBuffer(buffer, 0);
    }
}

void BufferManager::onDeviceReset()
{
    std::set<void*>& vertexBuffers = pointerSet(this, 1);
    for (std::set<void*>::const_iterator node = vertexBuffers.begin();
         node != vertexBuffers.end(); ++node)
    {
        VertexBuffer* const buffer = static_cast<VertexBuffer*>(*node);
        if (!buffer->buffer)
        {
            // sub_10014720 creates directly into +0x08.  It does not route through setBuffer and
            // therefore does not touch the renderer's live-resource counter on reset.
            m_device->CreateVertexBuffer(buffer->desc.size, buffer->desc.usage, 0,
                                         static_cast<D3DPOOL>(buffer->desc.pool),
                                         &buffer->buffer, 0);
        }
    }

    std::set<void*>& indexBuffers = pointerSet(this, 3);
    for (std::set<void*>::const_iterator node = indexBuffers.begin();
         node != indexBuffers.end(); ++node)
    {
        IndexBuffer* const buffer = static_cast<IndexBuffer*>(*node);
        if (!buffer->buffer)
        {
            m_device->CreateIndexBuffer(buffer->desc.Size, buffer->desc.Usage,
                                        buffer->desc.Format, buffer->desc.Pool,
                                        &buffer->buffer, 0);
        }
    }
}

void BufferManager::onDeviceDestroy()
{
    // PE order: default VB, managed VB, default IB, managed IB, declaration cache.
    static const unsigned int vertexLists[2] = { 1u, 0u };
    for (unsigned int listIndex = 0; listIndex < 2; ++listIndex)
    {
        std::set<void*>& buffers = pointerSet(this, vertexLists[listIndex]);
        for (std::set<void*>::const_iterator node = buffers.begin(); node != buffers.end();
             ++node)
            vertexBufferRelease(*node);
    }

    static const unsigned int indexLists[2] = { 3u, 2u };
    for (unsigned int listIndex = 0; listIndex < 2; ++listIndex)
    {
        std::set<void*>& buffers = pointerSet(this, indexLists[listIndex]);
        for (std::set<void*>::const_iterator node = buffers.begin(); node != buffers.end();
             ++node)
            indexBufferRelease(*node);
    }

    for (DeclarationCache::const_iterator node = m_declarations.begin();
         node != m_declarations.end(); ++node)
        vertexDeclarationRelease(node->second);
}

// PE 0x10011B00, with removals 0x100119A0/0x10011A50. Release before erasing,
// then restart from begin: a resource destructor can change other entries in the same set.
void bufferManagerCollectUnused()
{
    BufferManager* const manager = static_cast<BufferManager*>(g_bufferManager);
    const unsigned int order[] = { 1u, 0u, 3u, 2u };
    for (unsigned int i = 0; i < 4; ++i)
    {
        std::set<void*>& buffers = pointerSet(manager, order[i]);
        std::set<void*>::iterator node = buffers.begin();
        while (node != buffers.end())
        {
            void* const buffer = *node;
            typedef int (__thiscall *GetRefFn)(void*);
            const int refs = reinterpret_cast<GetRefFn>(
                (*reinterpret_cast<void***>(buffer))[0])(buffer);
            if (refs == 1)
            {
                // The removal helpers search default before managed, even when collection
                // arrived here from the managed set (PE 100119A0 / 10011A50).
                const unsigned int firstList = i < 2 ? 1u : 3u;
                for (unsigned int candidate = 0; candidate < 2; ++candidate)
                {
                    std::set<void*>& owned = pointerSet(manager, firstList - candidate);
                    std::set<void*>::iterator found = owned.find(buffer);
                    if (found == owned.end())
                        continue;
                    if (i < 2)
                        vertexBufferRelease(buffer);
                    else
                        indexBufferRelease(buffer);
                    owned.erase(found);
                    break;
                }
                node = buffers.begin();
            }
            else
                ++node;
        }
    }
}

// The owner-side decrement NameTagRenderer::removeTexture inlines (0x101B285E..0x101B2879):
//     `--wrapper[1]; if (!wrapper[1]) { sub_10014510(wrapper); engineFree(wrapper); }`
// with the remaining count returned, because that caller then removes a surviving buffer from the
// BufferManager's tracking set. The destroy half is vertexBufferRelease's, unchanged. (RW-239)
int vertexBufferReleaseReference(void* wrapper)
{
    VertexBuffer* const self = static_cast<VertexBuffer*>(wrapper);
    const int remaining = self->refCount - 1;
    vertexBufferRelease(wrapper);
    return remaining;
}

namespace {

// sub_10013170 - std::find(first, last, value) over a pointer set: a linear in-order walk comparing
// each node's key for equality (not the tree's lower_bound find). Custom ltcg convention in the
// image (ECX = &result, EDX = &value, stack first/last). Same answer as set::find for unique keys;
// written as the operation the image performs.
std::set<void*>::iterator findLinear(std::set<void*>& set, void* value)
{
    std::set<void*>::iterator node = set.begin();
    while (node != set.end() && *node != value)
        ++node;
    return node;
}

} // anonymous namespace

// Removes and releases a vertex buffer from the default set, then the managed set if absent.
// Requires an initialized global buffer manager.
void bufferManagerRemoveVertexBuffer(void* wrapper)
{
    BufferManager* const manager = static_cast<BufferManager*>(g_bufferManager);
    void* const value = wrapper;

    std::set<void*>& defaultPool = pointerSet(manager, 1);      // +0x18, D3DPOOL_DEFAULT buffers
    std::set<void*>::iterator node = findLinear(defaultPool, value);
    if (node != defaultPool.end())
    {
        vertexBufferRelease(*node);
        defaultPool.erase(node);
        return;
    }

    std::set<void*>& managedPool = pointerSet(manager, 0);      // +0x0C, D3DPOOL_MANAGED buffers
    node = findLinear(managedPool, value);
    if (node != managedPool.end())
    {
        vertexBufferRelease(*node);
        managedPool.erase(node);
    }
}

// Removes and releases an index buffer from the default set, then the managed set if absent.
// Requires an initialized global buffer manager.
void bufferManagerRemoveIndexBuffer(void* wrapper)
{
    BufferManager* const manager = static_cast<BufferManager*>(g_bufferManager);
    void* const value = wrapper;

    std::set<void*>& defaultPool = pointerSet(manager, 3);      // +0x30, D3DPOOL_DEFAULT index buffers
    std::set<void*>::iterator node = findLinear(defaultPool, value);
    if (node != defaultPool.end())
    {
        indexBufferRelease(*node);
        defaultPool.erase(node);
        return;
    }

    std::set<void*>& managedPool = pointerSet(manager, 2);      // +0x24, D3DPOOL_MANAGED index buffers
    node = findLinear(managedPool, value);
    if (node != managedPool.end())
    {
        indexBufferRelease(*node);
        managedPool.erase(node);
    }
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
