// [CHOSEN, not recovered] Public D3D9 resource transfers for the Linux port.
// https://github.com/kr4uzi/OpenBF2/issues/36
#if defined(__linux__) && defined(BF2_GFX_DXVK) && defined(BF2_IMAGE_RENDDX9)
#include "../../Metal/D3DXHelpers.h"
#include <cstring>

namespace bf2d3dx {
namespace {
template<class T> struct Owner {
    T* value=nullptr;
    ~Owner() { if(value) value->Release(); }
    T* operator->() const { return value; }
};
HRESULT transferLocked(IDirect3DSurface9* surface,const D3DSURFACE_DESC& desc,Bytes& bytes,bool upload) {
    const unsigned pitch=rowBytes(desc.Format,desc.Width);
    const unsigned rows=(desc.Height+blockSize(desc.Format)-1)/blockSize(desc.Format);
    const std::size_t size=std::size_t(pitch)*rows;
    if(!pitch || !rows || (upload && bytes.size()!=size)) return D3DERR_INVALIDCALL;
    D3DLOCKED_RECT lock={};
    HRESULT result=surface->LockRect(&lock,nullptr,upload?0:D3DLOCK_READONLY);
    if(FAILED(result)) return result;
    if(!lock.pBits || lock.Pitch<0 || unsigned(lock.Pitch)<pitch) {
        surface->UnlockRect(); return D3DERR_INVALIDCALL;
    }
    if(!upload) bytes.resize(size);
    for(unsigned y=0;y<rows;++y) {
        auto* resource=static_cast<unsigned char*>(lock.pBits)+std::size_t(y)*lock.Pitch;
        auto* packed=bytes.data()+std::size_t(y)*pitch;
        if(upload) std::memcpy(resource,packed,pitch); else std::memcpy(packed,resource,pitch);
    }
    return surface->UnlockRect();
}
}

HRESULT surfaceBytes(IDirect3DSurface9* surface,Bytes& bytes,bool upload) {
    if(!surface) return D3DERR_INVALIDCALL;
    D3DSURFACE_DESC desc={};
    HRESULT result=surface->GetDesc(&desc);
    if(FAILED(result)) return result;
    if(!pixelBytes(desc.Format) || (upload && bytes.size()!=imageBytes(desc.Format,desc.Width,desc.Height)))
        return D3DERR_INVALIDCALL;
    result=transferLocked(surface,desc,bytes,upload);
    if(SUCCEEDED(result)) return result;
    if(desc.Pool!=D3DPOOL_DEFAULT || (upload && desc.MultiSampleType!=D3DMULTISAMPLE_NONE)) return result;
    Owner<IDirect3DDevice9> device;
    result=surface->GetDevice(&device.value);
    if(FAILED(result)) return result;
    Owner<IDirect3DSurface9> staging;
    result=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&staging.value,nullptr);
    if(FAILED(result)) return result;
    if(upload) {
        result=transferLocked(staging.value,desc,bytes,true);
        return FAILED(result)?result:device->UpdateSurface(staging.value,nullptr,surface,nullptr);
    }
    // GetRenderTargetData only accepts a non-multisampled render target. Resolve
    // multisampling, or stretch a default-pool texture, before staging its bytes.
    Owner<IDirect3DSurface9> resolved;
    IDirect3DSurface9* source=surface;
    if(desc.MultiSampleType!=D3DMULTISAMPLE_NONE || !(desc.Usage&D3DUSAGE_RENDERTARGET)) {
        result=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&resolved.value,nullptr);
        if(FAILED(result)) return result;
        result=device->StretchRect(surface,nullptr,resolved.value,nullptr,D3DTEXF_NONE);
        if(FAILED(result)) return result;
        source=resolved.value;
    }
    result=device->GetRenderTargetData(source,staging.value);
    return FAILED(result)?result:transferLocked(staging.value,desc,bytes,false);
}

HRESULT textureBytes(IDirect3DBaseTexture9* texture,unsigned face,unsigned level,Bytes& bytes,bool upload) {
    if(!texture || level>=texture->GetLevelCount()) return D3DERR_INVALIDCALL;
    Owner<IDirect3DSurface9> surface;
    HRESULT result=D3DERR_INVALIDCALL;
    switch(texture->GetType()) {
    case D3DRTYPE_TEXTURE:
        if(face) return D3DERR_INVALIDCALL;
        result=static_cast<IDirect3DTexture9*>(texture)->GetSurfaceLevel(level,&surface.value); break;
    case D3DRTYPE_CUBETEXTURE:
        if(face>=6) return D3DERR_INVALIDCALL;
        result=static_cast<IDirect3DCubeTexture9*>(texture)->GetCubeMapSurface(static_cast<D3DCUBEMAP_FACES>(face),level,&surface.value); break;
    case D3DRTYPE_VOLUMETEXTURE: {
        if(face) return D3DERR_INVALIDCALL;
        auto* volume=static_cast<IDirect3DVolumeTexture9*>(texture);
        D3DVOLUME_DESC desc={}; result=volume->GetLevelDesc(level,&desc);
        if(FAILED(result)) return result;
        const unsigned pitch=rowBytes(desc.Format,desc.Width);
        const unsigned rows=(desc.Height+blockSize(desc.Format)-1)/blockSize(desc.Format);
        const std::size_t slice=std::size_t(pitch)*rows,size=slice*desc.Depth;
        if(!pitch || !rows || !desc.Depth || (upload && bytes.size()!=size)) return D3DERR_INVALIDCALL;
        D3DLOCKED_BOX lock={}; result=volume->LockBox(level,&lock,nullptr,upload?0:D3DLOCK_READONLY);
        if(FAILED(result)) return result; // D3D9 has no public volume readback operation.
        if(!lock.pBits || lock.RowPitch<0 || unsigned(lock.RowPitch)<pitch || lock.SlicePitch<0 ||
            std::size_t(lock.SlicePitch)<std::size_t(lock.RowPitch)*rows) {
            volume->UnlockBox(level); return D3DERR_INVALIDCALL;
        }
        if(!upload) bytes.resize(size);
        for(unsigned z=0;z<desc.Depth;++z) for(unsigned y=0;y<rows;++y) {
            auto* resource=static_cast<unsigned char*>(lock.pBits)+std::size_t(z)*lock.SlicePitch+std::size_t(y)*lock.RowPitch;
            auto* packed=bytes.data()+std::size_t(z)*slice+std::size_t(y)*pitch;
            if(upload) std::memcpy(resource,packed,pitch); else std::memcpy(packed,resource,pitch);
        }
        return volume->UnlockBox(level);
    }
    default: return D3DERR_INVALIDCALL;
    }
    return FAILED(result)?result:surfaceBytes(surface.value,bytes,upload);
}
}
#endif
