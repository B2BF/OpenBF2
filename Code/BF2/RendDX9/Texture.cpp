// D3D texture ownership, pool-specific locking, staging surfaces, and mip accounting.

#ifdef BF2_IMAGE_RENDDX9

#include "../../RendDX9Abi/ThunkAbi.h"
#include "../../RendDX9Abi/HfeAbi.h"
#include "../../RendDX9Abi/BfMemory.h"
#include "../../RendDX9Abi/Frontier.h"
#include "RendDX9Base.h"
#include "Texture.h"

#include <new>

namespace dice {
namespace hfe {
namespace dx9rend {

extern void* g_rendererBase;   // dword_1023B970 - RendDX9Init.cpp. The ctor/dtor count through it.

// TextureManager.cpp - the four reads the constructor makes on dword_1023B974.
int                 textureManagerDefaultMipClass();   // sub_10017DF0 -> TM+0x98
int                 textureManagerManagedFlag();       // sub_10017E30 -> TM+0x20
unsigned char       textureManagerRecreateMode();      // sub_10017E50 -> TM+0x24
const std::string& textureManagerOriginLabel();       // TM+0x9C

namespace {


// Shared getRef/no-op/addRef table. Owners explicitly decrement, destroy, and free;
// this interface has no release slot.


int  RD9_THUNK tex_getRef(void* self RD9_THUNK_EDX)  { return static_cast<EngineTexture*>(self)->refCount; }
void RD9_THUNK tex_nullsub(void* RD9_THUNK_EDX)      { /* nullsub - empty in the original too */ }
void RD9_THUNK tex_addRef(void* self RD9_THUNK_EDX)  { ++static_cast<EngineTexture*>(self)->refCount; }

void* const kTextureVtable[3] =
{
    reinterpret_cast<void*>(&tex_getRef),     // +0x00
    reinterpret_cast<void*>(&tex_nullsub),    // +0x04
    reinterpret_cast<void*>(&tex_addRef)      // +0x08
};

// The four QI target GUIDs live in Texture.h (byte-read from unk_10208560/70/80/90).

IDirect3DDevice9* baseDevice()
{
    RendDX9Base* b = reinterpret_cast<RendDX9Base*>(g_rendererBase);
    return b ? b->device : 0;
}


// Adjusts the renderer's live-texture statistic.

void bumpTextureCount(int delta)
{
    if (!g_rendererBase)
        return;
    int* counter = reinterpret_cast<int*>(
        static_cast<RendDX9Base*>(g_rendererBase)->renderStats + 0x2C);
    *counter += delta;
}

} // anonymous namespace


// sub_10014AF0 - the constructor. `storage` comes from the engine allocator and is NOT zeroed; the
// shipped ctor writes exactly the fields below and leaves the created-desc block (+0x28..+0x43) and
// the bytes at +0x52/+0x53 and +0x79..+0x7B as they land. Reproduced field-for-field, in the
// shipped order where an observer could tell (the string copy and the counter bump come last).
// The placement default-initialization constructs only the name string (+0x58, empty).

EngineTexture* textureCtor(void* storage)
{
    EngineTexture* t = new (storage) EngineTexture;

    t->vptr        = const_cast<void**>(kTextureVtable);
    t->refCount    = 1;
    t->baseTexture = 0;
    t->lockCount   = 0;                                    // +0x44
    t->byte51      = 0;                                    // +0x51
    t->priority    = 0;                                    // +0x54
    t->totalBytes  = 0;                                    // +0x74
    t->usedSuffixedFile = 0;                               // +0x78

    for (int i = 0; i < 7; ++i)                            // +0x0C..+0x27, this[3]..this[9]
        t->fileInfo[i] = 0;

    // deliberately NOT written: width..face (+0x28..+0x43), pad52, pad79 - the shipped ctor
    // skips them and textureAttach qmemcpy's the desc in later.

    t->mipClassIndex      = textureManagerDefaultMipClass();
    t->pool               = textureManagerManagedFlag();
    t->recreateFromStored = textureManagerRecreateMode();

    t->name = textureManagerOriginLabel();                 // string copy from TM+0x9C

    // (*(base->vptr + 0x6C))(base) -> +0x108; sub_1000D8C0 -> ++(+0x2C).
    bumpTextureCount(+1);
    return t;
}


// sub_10014E20 - the destructor. Re-stores the vptr first (the shipped compiler's dtor prologue),
// releases the held D3D texture, unregisters it from the resource tracker, decrements the base
// stats counter and destroys the name.

void textureDtor(EngineTexture* tex)
{
    tex->vptr = const_cast<void**>(kTextureVtable);
    if (tex->baseTexture)
    {
        tex->baseTexture->Release();
        // sub_10047EE0(0, baseTexture) - the resource-tracker erase. Same deliberate omission as
        // BufferManager.cpp:274: the tracker map feeds only the GSLoadedTextureDetailedInfo debug
        // dump, which is unwritten, and registering into an unread structure would be worse.
        // [FRONTIER] dice::hfe::dx9rend - the resource tracker (sub_10047E70 / sub_10047EE0)
        tex->baseTexture = 0;
    }
    bumpTextureCount(-1);                                  // base vslot +0x6C, then sub_1000D8D0
    tex->~EngineTexture();                                 // destroys the name
}

void textureAddRef(EngineTexture* tex)
{
    ++tex->refCount;                                       // vslot +0x08, ICF_incrementDwordAt0x4
}


// sub_10014E70 - release the D3D resource without destroying the wrapper. The default-pool
// device-lost path: Meme::Texture's DeviceLostResetListener slot +0x00 tail-jumps here for a
// pool-0 texture, and onDeviceReset later re-creates through the recreate arm. Returns the D3D
// Release() count; the tracker erase fires only when that hit zero.

int textureReleaseD3DResource(EngineTexture* tex)
{
    if (!tex->baseTexture)
        return 0;
    const int remaining = static_cast<int>(tex->baseTexture->Release());
    if (!remaining)
    {
        // sub_10047EE0(0, baseTexture) - the resource-tracker erase. Same withheld frontier as
        // textureDtor; see the note there.
    }
    tex->baseTexture = 0;
    return remaining;
}

void textureReleaseRef(EngineTexture*& tex)
{
    // The owner-side pattern Font::Ported::load ships: `if (v && !--v[1]) { sub_10014E20; free }`.
    if (tex && !--tex->refCount)
    {
        textureDtor(tex);
        ::renddx9abi::engineFree(tex);
    }
    tex = 0;
}


// Replaces and AddRefs the D3D texture, copies its description, and resets lock state.
// Updates origin name, mip byte count, and resource tracking after releasing the old texture.

bool textureAttach(EngineTexture* tex, IDirect3DBaseTexture9* d3d, const unsigned int desc[7])
{
    if (tex->baseTexture)
    {
        // sub_10047EE0(0) then Release - the shipped order. Tracker: see the note in textureDtor.
        tex->baseTexture->Release();
        tex->baseTexture = 0;
        tex->lockCount   = 0;
    }

    tex->baseTexture = d3d;
    d3d->AddRef();

    ::memcpy(&tex->width, desc, 0x1C);

    tex->name = textureManagerOriginLabel();               // TM+0x9C, again

    unsigned int w = tex->width, h = tex->height, d = tex->depth;
    const int bpp = bitsPerPixel(tex->format);
    tex->totalBytes = 0;
    for (unsigned int i = 0; i < tex->mipLevels; ++i)
    {
        tex->totalBytes += static_cast<unsigned int>(h * w * d * bpp / 8);
        h >>= 1;
        w >>= 1;
        if (d > 1)
            d >>= 1;
    }

    // sub_10047E70(0, this->baseTexture, totalBytes, &name, &pool, this) - NOT reproduced, same
    // frontier as above. Nothing on the boot path reads the tracker map.
    // [FRONTIER] dice::hfe::dx9rend - the resource tracker (sub_10047E70 / sub_10048AE0)
    return true;
}


// sub_10014820 - qmemcpy(this + 0x0C, info, 0x1C). The original D3DXIMAGE_INFO, stamped at the very
// end of loadTextureFromFile - after the mip-skip has already shrunk the created desc at +0x28, so
// the two blocks deliberately differ on a skipped-mip load.

void textureSetFileInfo(EngineTexture* tex, const void* info28)
{
    ::memcpy(tex->fileInfo, info28, 0x1C);
}


// sub_10014950 - the real level surface. face == QI IDirect3DTexture9 and
// GetSurfaceLevel(mip); otherwise QI IDirect3DCubeTexture9 and GetCubeMapSurface(face, mip). The
// QI'd interface is Released before returning; the surface ref passes to the caller.

IDirect3DSurface9* textureGetLevelSurface(EngineTexture* tex, unsigned int mip, unsigned int face)
{
    IDirect3DSurface9* surf = 0;
    void* itf = 0;
    if (face == 0x7FFFFFFF)
    {
        tex->baseTexture->QueryInterface(kIID_Texture9, &itf);
        static_cast<IDirect3DTexture9*>(itf)->GetSurfaceLevel(mip, &surf);
    }
    else
    {
        tex->baseTexture->QueryInterface(kIID_CubeTexture9, &itf);
        static_cast<IDirect3DCubeTexture9*>(itf)->GetCubeMapSurface(
            static_cast<D3DCUBEMAP_FACES>(face), mip, &surf);
    }
    static_cast<IUnknown*>(itf)->Release();
    return surf;
}

namespace {


// sub_100149E0 - the staging surface for a default-pool lock. Read from the body and its prologue:
// it ignores the level surface its caller passes, reads the device from dword_1023B970+0x0C, and
// creates a fresh 1-level systemmem texture of the mip's exact w/h/format, returning its level-0
// surface (the texture itself is Released; the surface ref survives it).

IDirect3DSurface9* createStagingSurface(unsigned int w, unsigned int h, unsigned int format)
{
    IDirect3DDevice9* device = baseDevice();
    IDirect3DTexture9* staging = 0;
    device->CreateTexture(w, h, 1, 0, static_cast<D3DFORMAT>(format),
                          D3DPOOL_SYSTEMMEM, &staging, 0);
    IDirect3DSurface9* surf = 0;
    staging->GetSurfaceLevel(0, &surf);
    staging->Release();
    return surf;
}


// Texture_updateSurface, device->UpdateSurface(src, 0, dst, 0), with the shipped
// failure diagnostic at Texture.cpp:484 ("Failed to update texture. <DXGetErrorDescription9>").
// The diagnostic is rendered through the rebuild's log rather than the unread debug-stream family
// (sub_10074300/sub_10074690), the same choice every recovered diagnostic in this image has made.

void updateSurfaceFromStaging(IDirect3DSurface9* dst, IDirect3DSurface9* src)
{
    IDirect3DDevice9* device = baseDevice();
    const HRESULT hr = device->UpdateSurface(src, 0, dst, 0);
    if (FAILED(hr))
        ::renddx9abi::logf("Texture.cpp:484: Failed to update texture. hr=0x%08X",
                           static_cast<unsigned int>(hr));
}

} // anonymous namespace


// Locks a mip/face through a staging surface for default-pool textures or directly otherwise.
// Returns a lock record retained until unlock; the temporary queried interface is released.

TextureLockRecord* textureLock(EngineTexture* tex, unsigned int mip, unsigned int face)
{
    TextureLockRecord* rec =
        static_cast<TextureLockRecord*>(::renddx9abi::engineAlloc(sizeof(TextureLockRecord)));
    if (rec)
        ::memset(rec, 0, sizeof(TextureLockRecord));

    rec->width  = tex->width >> mip;
    rec->height = tex->height >> mip;
    unsigned int d = tex->depth >> mip;
    rec->depth  = (static_cast<int>(d) < 1) ? 1 : d;
    rec->bpp    = bitsPerPixel(tex->format);
    rec->mip    = mip;
    rec->face   = face;

    if (tex->pool == 0)
    {
        // default pool - can't lock the real surface; stage in systemmem.
        IDirect3DSurface9* real  = textureGetLevelSurface(tex, mip, face);
        IDirect3DSurface9* stage = createStagingSurface(rec->width, rec->height, tex->format);
        rec->staging = stage;

        D3DLOCKED_RECT lr;
        stage->LockRect(&lr, 0, 0);                        // surface vslot +0x34 (slot 13)
        real->Release();
        rec->pBits = lr.pBits;

        ++tex->lockCount;
        return rec;
    }

    void* itf = 0;
    if (face != 0x7FFFFFFF)
    {
        tex->baseTexture->QueryInterface(kIID_CubeTexture9, &itf);
        D3DLOCKED_RECT lr;
        static_cast<IDirect3DCubeTexture9*>(itf)->LockRect(
            static_cast<D3DCUBEMAP_FACES>(face), mip, &lr, 0, 0);
        static_cast<IUnknown*>(itf)->Release();
        rec->pBits = lr.pBits;
    }
    else if (tex->depth <= 1)
    {
        tex->baseTexture->QueryInterface(kIID_Texture9, &itf);
        D3DLOCKED_RECT lr;
        static_cast<IDirect3DTexture9*>(itf)->LockRect(mip, &lr, 0, 0);
        static_cast<IUnknown*>(itf)->Release();
        rec->pBits = lr.pBits;
    }
    else
    {
        tex->baseTexture->QueryInterface(kIID_VolumeTexture9, &itf);
        D3DLOCKED_BOX lb;
        static_cast<IDirect3DVolumeTexture9*>(itf)->LockBox(mip, &lb, 0, 0);
        static_cast<IUnknown*>(itf)->Release();
        rec->pBits = lb.pBits;
    }

    rec->staging = 0;
    ++tex->lockCount;
    return rec;
}


// sub_10014D30 - unlock. Managed: QI the matching interface and UnlockRect/UnlockBox. Default:
// unlock the staging surface, fetch the real level surface again, UpdateSurface staging -> real,
// release both. Then free the record and --lockCount. Always returns 1.

bool textureUnlock(EngineTexture* tex, TextureLockRecord* rec)
{
    if (tex->pool != 0)
    {
        void* itf = 0;
        if (rec->face == 0x7FFFFFFF)
        {
            if (tex->depth > 1)
            {
                tex->baseTexture->QueryInterface(kIID_VolumeTexture9, &itf);
                static_cast<IDirect3DVolumeTexture9*>(itf)->UnlockBox(rec->mip);
            }
            else
            {
                tex->baseTexture->QueryInterface(kIID_Texture9, &itf);
                static_cast<IDirect3DTexture9*>(itf)->UnlockRect(rec->mip);
            }
        }
        else
        {
            tex->baseTexture->QueryInterface(kIID_CubeTexture9, &itf);
            static_cast<IDirect3DCubeTexture9*>(itf)->UnlockRect(
                static_cast<D3DCUBEMAP_FACES>(rec->face), rec->mip);
        }
        static_cast<IUnknown*>(itf)->Release();
    }
    else
    {
        rec->staging->UnlockRect();                        // surface vslot +0x38 (slot 14)
        IDirect3DSurface9* real = textureGetLevelSurface(tex, rec->mip, rec->face);
        updateSurfaceFromStaging(real, rec->staging);      // Texture_updateSurface, 0x10014A40
        rec->staging->Release();
        real->Release();
    }

    ::renddx9abi::engineFree(rec);
    --tex->lockCount;
    return true;
}

} // namespace dx9rend
} // namespace hfe
} // namespace dice

#endif // BF2_IMAGE_RENDDX9
