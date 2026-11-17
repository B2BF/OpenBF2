// [CHOSEN, not recovered] D3DX texture operations over G3b resources.
#if (defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))) && defined(BF2_IMAGE_RENDDX9)
#include "D3DXHelpers.h"
#if defined(__APPLE__)
#include "RefTrace.h" // [TEMP] [RETIRE-WHEN: rend-deinit-reftrace]
#endif
#include "../../../IO/File/IFileSystem.h"
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <limits>
namespace dice { namespace hfe { extern io::IFileManager* fileManager; } }
namespace bf2d3dx {
static constexpr HRESULT invalidData=static_cast<HRESULT>(0x88760b59u);
static bool readFile(const char* name,Bytes& bytes) {
    if(!name) return false;
    // Use mounted engine archives and search paths whenever the host is present.
    auto* manager=dice::hfe::fileManager;
    if(manager) {
        auto* s=manager->open(std::string(name),dice::hfe::io::FILEMODE_READ,static_cast<dice::hfe::io::FileAccess>(0),0);
        if(!s) return false;
        const int size=s->getAvailableBytes();
        bool ok=size>0; if(ok) { bytes.resize(size); ok=s->read(bytes.data(),size)==unsigned(size); }
        s->release(); return ok;
    }
    // Standalone tools have no engine host. Paths there denote physical files.
    FILE* f=std::fopen(name,"rb"); if(!f) return false;
    bool ok=std::fseek(f,0,SEEK_END)==0; long size=ok?std::ftell(f):-1;
    ok=size>0 && std::size_t(size)<=std::numeric_limits<UINT>::max() && std::fseek(f,0,SEEK_SET)==0;
    if(ok) { bytes.resize(size); ok=std::fread(bytes.data(),1,bytes.size(),f)==bytes.size(); }
    std::fclose(f); return ok;
}
static bool writeFile(const char* name,const Bytes& bytes) {
    if(!name) return false;
    // Save APIs receive getWritePath-resolved names at the engine call sites. [CHOSEN, not
    // recovered] As in the Win32 file APIs, a backslash separates path components (the
    // screenshot capture, PE 0x10004F44, turns its separators into backslashes).
    std::string path(name); std::replace(path.begin(),path.end(),'\\','/');
    FILE* f=std::fopen(path.c_str(),"wb"); if(!f) return false;
    const bool ok=std::fwrite(bytes.data(),1,bytes.size(),f)==bytes.size(); const int close=std::fclose(f); return ok && !close;
}
static bool rectangle(const RECT* r,unsigned w,unsigned h,RECT& out) {
    out=r?*r:RECT{0,0,LONG(w),LONG(h)};
    return out.left>=0 && out.top>=0 && out.left<out.right && out.top<out.bottom && unsigned(out.right)<=w && unsigned(out.bottom)<=h;
}
static HRESULT blit(const void* source,D3DFORMAT sf,unsigned sw,unsigned sh,unsigned sp,const RECT& sr,
                    Bytes& dest,D3DFORMAT df,unsigned dw,unsigned dh,const RECT& dr,DWORD filter,D3DCOLOR key) {
    if(!pixelBytes(sf) || !pixelBytes(df)) return D3DERR_INVALIDCALL;
    if(filter==D3DX_DEFAULT) filter=D3DX_FILTER_TRIANGLE;
    if(filter&~0x6f000fu) stop("bf2d3dx::blit filter flags");
    if((filter&15)<1 || (filter&15)>5) return D3DERR_INVALIDCALL;
    const unsigned rw=sr.right-sr.left,rh=sr.bottom-sr.top,ow=dr.right-dr.left,oh=dr.bottom-dr.top;
    const unsigned sb=blockSize(sf),db=blockSize(df),dp=rowBytes(df,dw);
    const auto* src=static_cast<const unsigned char*>(source);
    // Preserve exact compressed blocks when both rectangles fit block boundaries.
    if(sf==df && rw==ow && rh==oh && !key && !(filter&(D3DX_FILTER_SRGB_IN|D3DX_FILTER_SRGB_OUT)) &&
        sr.left%sb==0 && sr.top%sb==0 && dr.left%db==0 && dr.top%db==0 &&
        (rw%sb==0 || (unsigned(sr.right)==sw && unsigned(dr.right)==dw)) &&
        (rh%sb==0 || (unsigned(sr.bottom)==sh && unsigned(dr.bottom)==dh))) {
        const unsigned row=rowBytes(sf,rw),rows=(rh+sb-1)/sb;
        for(unsigned y=0;y<rows;++y) std::memcpy(dest.data()+std::size_t(dr.top/db+y)*dp+(dr.left/db)*pixelBytes(df),src+std::size_t(sr.top/sb+y)*sp+(sr.left/sb)*pixelBytes(sf),row);
        return D3D_OK;
    }
    Bytes unpacked;
    D3DFORMAT input=sf; unsigned pitch=sp;
    if(sb>1) {
        unpacked.resize(imageBytes(D3DFMT_A32B32G32R32F,sw,sh));
        if(!convert(source,sf,sw,sh,sp,unpacked.data(),D3DFMT_A32B32G32R32F,sw,sh,sw*16,D3DX_FILTER_NONE)) return invalidData;
        src=unpacked.data(); pitch=sw*16; input=D3DFMT_A32B32G32R32F;
    }
    src+=std::size_t(sr.top)*pitch+sr.left*pixelBytes(input);
    // A partial BC destination requires recompression of touched blocks only.
    // Keep untouched blocks byte-for-byte and preserve their decoded neighbors.
    if(db>1) {
        const unsigned left=(dr.left/4)*4,top=(dr.top/4)*4,right=std::min(dw,(unsigned(dr.right)+3)&~3u),bottom=std::min(dh,(unsigned(dr.bottom)+3)&~3u);
        const unsigned w=right-left,h=bottom-top;
        Bytes rgba(imageBytes(D3DFMT_A32B32G32R32F,w,h));
        if(!convert(dest.data()+std::size_t(top/4)*dp+(left/4)*pixelBytes(df),df,w,h,dp,rgba.data(),D3DFMT_A32B32G32R32F,w,h,w*16,D3DX_FILTER_NONE)) return invalidData;
        if(!convert(src,input,rw,rh,pitch,rgba.data()+std::size_t(dr.top-top)*w*16+(dr.left-left)*16,D3DFMT_A32B32G32R32F,ow,oh,w*16,filter,key)) return invalidData;
        return convert(rgba.data(),D3DFMT_A32B32G32R32F,w,h,w*16,dest.data()+std::size_t(top/4)*dp+(left/4)*pixelBytes(df),df,w,h,dp,D3DX_FILTER_NONE)?D3D_OK:invalidData;
    }
    return convert(src,input,rw,rh,pitch,dest.data()+std::size_t(dr.top)*dp+dr.left*pixelBytes(df),df,ow,oh,dp,filter,key)?D3D_OK:invalidData;
}
static HRESULT describe(IDirect3DBaseTexture9* t,unsigned level,D3DXIMAGE_INFO& i) {
    if(!t || level>=t->GetLevelCount()) return D3DERR_INVALIDCALL;
    i={}; i.Depth=1; i.MipLevels=t->GetLevelCount(); i.ResourceType=t->GetType(); i.ImageFileFormat=D3DXIFF_DDS;
    // Resource type is the public D3D9 discriminator. Forwarded COM objects implement
    // namespaced subset interfaces, so their C++ RTTI is not the engine's interface RTTI.
    if(i.ResourceType==D3DRTYPE_VOLUMETEXTURE) { auto* v=static_cast<IDirect3DVolumeTexture9*>(t); D3DVOLUME_DESC d; HRESULT hr=v->GetLevelDesc(level,&d); if(FAILED(hr)) return hr; i.Width=d.Width; i.Height=d.Height; i.Depth=d.Depth; i.Format=d.Format; }
    else { D3DSURFACE_DESC d; HRESULT hr;
        if(i.ResourceType==D3DRTYPE_TEXTURE) hr=static_cast<IDirect3DTexture9*>(t)->GetLevelDesc(level,&d);
        else if(i.ResourceType==D3DRTYPE_CUBETEXTURE) hr=static_cast<IDirect3DCubeTexture9*>(t)->GetLevelDesc(level,&d); else return D3DERR_INVALIDCALL;
        if(FAILED(hr)) return hr; i.Width=d.Width; i.Height=d.Height; i.Format=d.Format;
    }
    return D3D_OK;
}
static HRESULT loadTexture(IDirect3DDevice9* device,const Image& im,UINT w,UINT h,UINT levels,DWORD usage,D3DFORMAT format,D3DPOOL pool,DWORD filter,DWORD mipFilter,D3DCOLOR key,IDirect3DTexture9** out) {
    if(!device || !out) return D3DERR_INVALIDCALL; *out=nullptr;
    // [TEMP] [CHOSEN, not recovered] [RETIRE-WHEN: macos-dds-mip-skip]
    // Implement skip-bit dimension/level selection and golden-test stored mip
    // selection before accepting this option; inventory callers use no skip.
    if(mipFilter!=D3DX_DEFAULT && (mipFilter&0xfc000000u)) stop("D3DXCreateTextureFromFileExA DDS mip skip");
    if(im.info.ResourceType!=D3DRTYPE_TEXTURE) return D3DERR_INVALIDCALL;
    // DDS cube/volume payloads remain available through parse and the G3b
    // factories; the public D3DXCreateTexture APIs create 2D textures only.
    auto powerOfTwo=[](unsigned n) { unsigned v=1; while(v<n && v<0x80000000u) v<<=1; return v; };
    if(w==D3DX_DEFAULT || w==0) w=powerOfTwo(im.info.Width);
    else if(w==D3DX_DEFAULT_NONPOW2 || w==D3DX_FROM_FILE) w=im.info.Width;
    if(h==D3DX_DEFAULT || h==0) h=powerOfTwo(im.info.Height);
    else if(h==D3DX_DEFAULT_NONPOW2 || h==D3DX_FROM_FILE) h=im.info.Height;
    if(levels==D3DX_FROM_FILE) levels=im.info.MipLevels;
    if(format==D3DFMT_UNKNOWN) {
        format=im.info.Format;
        // [CHOSEN, not recovered] JPEG source formats carry no alpha. D3DX's
        // default-format choice needs an alpha channel when a color key is requested.
        if(im.info.ImageFileFormat==D3DXIFF_JPG && key)
            format=format==D3DFMT_L8?D3DFMT_A8L8:D3DFMT_A8R8G8B8;
    }
    else if(unsigned(format)==D3DX_FROM_FILE) { format=im.sourceFormat; if(!pixelBytes(format)) return D3DERR_NOTAVAILABLE; }
    if(levels==D3DX_DEFAULT) levels=0;
    HRESULT hr=D3DXCreateTexture(device,w,h,levels,usage,format,pool,out); if(FAILED(hr)) return hr;
    auto* texture=*out;
    for(unsigned l=0;l<texture->GetLevelCount();++l) {
        D3DSURFACE_DESC d; hr=texture->GetLevelDesc(l,&d); if(FAILED(hr)) break;
        if(l<im.info.MipLevels) {
            Bytes bytes(imageBytes(format,d.Width,d.Height)); const unsigned sw=std::max(1u,im.info.Width>>l),sh=std::max(1u,im.info.Height>>l);
            if(!convert(im.data[l].data(),im.info.Format,sw,sh,rowBytes(im.info.Format,sw),bytes.data(),format,d.Width,d.Height,rowBytes(format,d.Width),filter,key)) { hr=invalidData; break; }
            hr=textureBytes(texture,0,l,bytes,true);
        } else { hr=D3DXFilterTexture(texture,nullptr,l-1,mipFilter); break; }
        if(FAILED(hr)) break;
    }
    if(FAILED(hr)) { texture->Release(); *out=nullptr; } return hr;
}
class RenderToSurface final : public ID3DXRenderToSurface {
    ULONG refs=1;
    IDirect3DDevice9* device;
    D3DXRTS_DESC desc;
    IDirect3DSurface9 *depth=nullptr,*target=nullptr,*oldTarget=nullptr,*oldDepth=nullptr,*output=nullptr;
    D3DVIEWPORT9 oldViewport={}; bool active=false;
    void restore() {
        device->SetDepthStencilSurface(nullptr); device->SetRenderTarget(0,oldTarget); device->SetDepthStencilSurface(oldDepth); device->SetViewport(&oldViewport);
        if(oldTarget) oldTarget->Release(); if(oldDepth) oldDepth->Release(); if(output) output->Release();
        oldTarget=oldDepth=output=nullptr; active=false;
    }
public:
    RenderToSurface(IDirect3DDevice9* d,const D3DXRTS_DESC& v):device(d),desc(v) {
#if defined(__APPLE__)
        { bf2metal::reftrace::Owned o; device->AddRef(); } bf2metal::reftrace::born(this,"ID3DXRenderToSurface",[](const void*) { return 1ul; });
#else
        device->AddRef();
#endif
    }
    ~RenderToSurface() { if(active) { device->EndScene(); restore(); } if(depth) depth->Release(); if(target) target->Release();
#if defined(__APPLE__)
        bf2metal::reftrace::died(this); bf2metal::reftrace::Owned o;
#endif
        device->Release(); }
    HRESULT initialize() {
        HRESULT hr=device->CreateRenderTarget(desc.Width,desc.Height,desc.Format,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr);
        if(SUCCEEDED(hr) && desc.DepthStencil) hr=device->CreateDepthStencilSurface(desc.Width,desc.Height,desc.DepthStencilFormat,D3DMULTISAMPLE_NONE,0,TRUE,&depth,nullptr); return hr;
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        const GUID unknown={0,0,0,{0xc0,0,0,0,0,0,0,0x46}},own={0x6985f346,0x2c3d,0x43b3,{0xbe,0x8b,0xda,0xae,0x8a,0x03,0xd8,0x94}};
        if(std::memcmp(&id,&unknown,16) && std::memcmp(&id,&own,16)) return E_NOINTERFACE; *out=this; AddRef(); return D3D_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { auto n=--refs; if(!n) delete this; return n; }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override { if(!out) return D3DERR_INVALIDCALL; *out=device; device->AddRef(); return D3D_OK; }
    HRESULT STDMETHODCALLTYPE GetDesc(D3DXRTS_DESC* out) override { if(!out) return D3DERR_INVALIDCALL; *out=desc; return D3D_OK; }
    HRESULT STDMETHODCALLTYPE BeginScene(IDirect3DSurface9* surface,const D3DVIEWPORT9* viewport) override {
        if(active || !surface) return D3DERR_INVALIDCALL;
        IDirect3DDevice9* owner=nullptr; HRESULT ownerResult=surface->GetDevice(&owner);
        if(FAILED(ownerResult)) return ownerResult; const bool same=owner==device; owner->Release(); if(!same) return D3DERR_INVALIDCALL;
        D3DSURFACE_DESC d; if(FAILED(surface->GetDesc(&d)) || d.Width!=desc.Width || d.Height!=desc.Height || d.Format!=desc.Format) return D3DERR_INVALIDCALL;
        D3DVIEWPORT9 v=viewport?*viewport:D3DVIEWPORT9{0,0,desc.Width,desc.Height,0,1};
        if(!v.Width || !v.Height || v.X>desc.Width || v.Y>desc.Height || v.Width>desc.Width-v.X || v.Height>desc.Height-v.Y || v.MinZ<0 || v.MaxZ>1 || v.MinZ>v.MaxZ) return D3DERR_INVALIDCALL;
        device->GetViewport(&oldViewport); device->GetRenderTarget(0,&oldTarget); device->GetDepthStencilSurface(&oldDepth);
        output=surface; output->AddRef();
        HRESULT hr=device->SetDepthStencilSurface(nullptr);
        if(SUCCEEDED(hr)) hr=device->SetRenderTarget(0,(d.Usage&D3DUSAGE_RENDERTARGET)?surface:target);
        if(SUCCEEDED(hr)) hr=device->SetDepthStencilSurface(depth);
        if(SUCCEEDED(hr)) hr=device->SetViewport(&v);
        if(SUCCEEDED(hr)) hr=device->BeginScene();
        if(FAILED(hr)) { restore(); return hr; } active=true; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE EndScene(DWORD filter) override {
        if(!active) return D3DERR_INVALIDCALL;
        HRESULT hr=device->EndScene(); D3DSURFACE_DESC d; output->GetDesc(&d);
        if(SUCCEEDED(hr) && !(d.Usage&D3DUSAGE_RENDERTARGET)) hr=D3DXLoadSurfaceFromSurface(output,nullptr,nullptr,target,nullptr,nullptr,filter,0);
        restore(); return hr;
    }
};
}
using namespace bf2d3dx;
HRESULT WINAPI D3DXCreateTexture(IDirect3DDevice9* d,UINT w,UINT h,UINT n,DWORD usage,D3DFORMAT f,D3DPOOL pool,IDirect3DTexture9** out) {
    if(!d || !out) return D3DERR_INVALIDCALL; *out=nullptr;
    if(w==D3DX_DEFAULT) w=256; if(h==D3DX_DEFAULT) h=256; if(n==D3DX_DEFAULT) n=0; if(f==D3DFMT_UNKNOWN) f=D3DFMT_A8R8G8B8;
    return d->CreateTexture(w,h,n,usage,f,pool,out,nullptr);
}
HRESULT WINAPI D3DXGetImageInfoFromFileA(LPCSTR path,D3DXIMAGE_INFO* out) {
    if(!out || !path) return D3DERR_INVALIDCALL; Bytes data;
    return readFile(path,data)?D3DXGetImageInfoFromFileInMemory(data.data(),UINT(data.size()),out):invalidData;
}
HRESULT WINAPI D3DXCreateTextureFromFileInMemory(IDirect3DDevice9* d,LPCVOID p,UINT n,IDirect3DTexture9** out) {
    if(!out) return D3DERR_INVALIDCALL; *out=nullptr; Image im;
    if(!parse(p,n,im)) return invalidData;
    return loadTexture(d,im,D3DX_DEFAULT,D3DX_DEFAULT,0,0,D3DFMT_UNKNOWN,D3DPOOL_MANAGED,D3DX_DEFAULT,D3DX_DEFAULT,0,out);
}
HRESULT WINAPI D3DXCreateTextureFromFileExA(IDirect3DDevice9* d,LPCSTR path,UINT w,UINT h,UINT n,DWORD usage,D3DFORMAT f,D3DPOOL pool,DWORD filter,DWORD mipFilter,D3DCOLOR key,D3DXIMAGE_INFO* info,PALETTEENTRY* palette,IDirect3DTexture9** out) {
    if(!out) return D3DERR_INVALIDCALL; *out=nullptr; if(palette) stop("D3DXCreateTextureFromFileExA output palette");
    Bytes data; Image im; if(!readFile(path,data) || !parse(data.data(),data.size(),im)) return invalidData;
    if(info && !metadata(data.data(),data.size(),*info)) return invalidData;
    return loadTexture(d,im,w,h,n,usage,f,pool,filter,mipFilter,key,out);
}
HRESULT WINAPI D3DXLoadSurfaceFromSurface(IDirect3DSurface9* dst,const PALETTEENTRY* dp,const RECT* destRect,IDirect3DSurface9* src,const PALETTEENTRY* sp,const RECT* sourceRect,DWORD filter,D3DCOLOR key) {
    if(!dst || !src) return D3DERR_INVALIDCALL;
    if(dp || sp) stop("D3DXLoadSurfaceFromSurface palette");
    D3DSURFACE_DESC a,b; HRESULT hr=src->GetDesc(&a); if(FAILED(hr)) return hr; hr=dst->GetDesc(&b); if(FAILED(hr)) return hr;
    RECT sr,dr; if(!rectangle(sourceRect,a.Width,a.Height,sr) || !rectangle(destRect,b.Width,b.Height,dr)) return D3DERR_INVALIDCALL;
    Bytes from,to; hr=surfaceBytes(src,from,false); if(FAILED(hr)) return hr; hr=surfaceBytes(dst,to,false); if(FAILED(hr)) return hr;
    hr=blit(from.data(),a.Format,a.Width,a.Height,rowBytes(a.Format,a.Width),sr,to,b.Format,b.Width,b.Height,dr,filter,key);
    return FAILED(hr)?hr:surfaceBytes(dst,to,true);
}
HRESULT WINAPI D3DXLoadSurfaceFromMemory(IDirect3DSurface9* dst,const PALETTEENTRY* dp,const RECT* destRect,LPCVOID src,D3DFORMAT sf,UINT pitch,const PALETTEENTRY* sp,const RECT* sourceRect,DWORD filter,D3DCOLOR key) {
    if(!dst || !src || !sourceRect) return D3DERR_INVALIDCALL;
    if(dp || sp) stop("D3DXLoadSurfaceFromMemory palette");
    D3DSURFACE_DESC d; HRESULT hr=dst->GetDesc(&d); if(FAILED(hr)) return hr;
    RECT dr,sr; if(!rectangle(destRect,d.Width,d.Height,dr) || sourceRect->right<=0 || sourceRect->bottom<=0 ||
        !rectangle(sourceRect,sourceRect->right,sourceRect->bottom,sr) || !pixelBytes(sf) || pitch<rowBytes(sf,sr.right)) return D3DERR_INVALIDCALL;
    Bytes to; hr=surfaceBytes(dst,to,false); if(FAILED(hr)) return hr;
    hr=blit(src,sf,sr.right,sr.bottom,pitch,sr,to,d.Format,d.Width,d.Height,dr,filter,key);
    return FAILED(hr)?hr:surfaceBytes(dst,to,true);
}
HRESULT WINAPI D3DXFilterTexture(IDirect3DBaseTexture9* t,const PALETTEENTRY* palette,UINT level,DWORD filter) {
    if(!t) return D3DERR_INVALIDCALL; if(palette) stop("D3DXFilterTexture palette");
    if(level==D3DX_DEFAULT) level=0; if(level>=t->GetLevelCount()) return D3DERR_INVALIDCALL;
    if(filter==D3DX_DEFAULT) filter=D3DX_FILTER_BOX;
    const unsigned faces=t->GetType()==D3DRTYPE_CUBETEXTURE?6:1;
    for(unsigned face=0;face<faces;++face) for(unsigned l=level+1;l<t->GetLevelCount();++l) {
        D3DXIMAGE_INFO a,b; HRESULT hr=describe(t,l-1,a); if(FAILED(hr)) return hr; hr=describe(t,l,b); if(FAILED(hr)) return hr;
        Bytes from,to(imageBytes(b.Format,b.Width,b.Height,b.Depth)); hr=textureBytes(t,face,l-1,from,false); if(FAILED(hr)) return hr;
        // Filter every source slice in XY first, then apply the same filter in Z.
        // Float intermediates retain unclamped HDR and channel precision.
        std::vector<Bytes> slices(a.Depth,Bytes(imageBytes(D3DFMT_A32B32G32R32F,b.Width,b.Height)));
        for(unsigned z=0;z<a.Depth;++z) if(!convert(from.data()+imageBytes(a.Format,a.Width,a.Height)*z,a.Format,a.Width,a.Height,rowBytes(a.Format,a.Width),slices[z].data(),D3DFMT_A32B32G32R32F,b.Width,b.Height,b.Width*16,filter&~D3DX_FILTER_SRGB_OUT)) return invalidData;
        Bytes column(std::size_t(a.Depth)*16),filtered(std::size_t(b.Depth)*16),rgba(imageBytes(D3DFMT_A32B32G32R32F,b.Width,b.Height,b.Depth));
        for(unsigned y=0;y<b.Height;++y) for(unsigned x=0;x<b.Width;++x) {
            for(unsigned z=0;z<a.Depth;++z) std::memcpy(column.data()+z*16,slices[z].data()+(std::size_t(y)*b.Width+x)*16,16);
            DWORD zfilter=filter&~(D3DX_FILTER_SRGB_IN|D3DX_FILTER_SRGB_OUT|D3DX_FILTER_MIRROR_U|D3DX_FILTER_MIRROR_V);
            if(filter&D3DX_FILTER_MIRROR_W) zfilter|=D3DX_FILTER_MIRROR_U;
            if(!convert(column.data(),D3DFMT_A32B32G32R32F,a.Depth,1,a.Depth*16,filtered.data(),D3DFMT_A32B32G32R32F,b.Depth,1,b.Depth*16,zfilter)) return invalidData;
            for(unsigned z=0;z<b.Depth;++z) std::memcpy(rgba.data()+((std::size_t(z)*b.Height+y)*b.Width+x)*16,filtered.data()+z*16,16);
        }
        for(unsigned z=0;z<b.Depth;++z) if(!convert(rgba.data()+imageBytes(D3DFMT_A32B32G32R32F,b.Width,b.Height)*z,D3DFMT_A32B32G32R32F,b.Width,b.Height,b.Width*16,to.data()+imageBytes(b.Format,b.Width,b.Height)*z,b.Format,b.Width,b.Height,rowBytes(b.Format,b.Width),D3DX_FILTER_NONE|(filter&D3DX_FILTER_SRGB_OUT))) return invalidData;
        hr=textureBytes(t,face,l,to,true); if(FAILED(hr)) return hr;
    }
    return D3D_OK;
}
HRESULT WINAPI D3DXFillTexture(IDirect3DTexture9* t,LPD3DXFILL2D function,void* data) {
    if(!t || !function) return D3DERR_INVALIDCALL;
    for(unsigned l=0;l<t->GetLevelCount();++l) {
        D3DXIMAGE_INFO i; HRESULT hr=describe(t,l,i); if(FAILED(hr)) return hr;
        Bytes rgba(imageBytes(D3DFMT_A32B32G32R32F,i.Width,i.Height)),packed(imageBytes(i.Format,i.Width,i.Height));
        const D3DXVECTOR2 size(1.f/i.Width,1.f/i.Height);
        for(unsigned y=0;y<i.Height;++y) for(unsigned x=0;x<i.Width;++x) {
            D3DXVECTOR4 color; const D3DXVECTOR2 uv((x+.5f)/i.Width,(y+.5f)/i.Height); function(&color,&uv,&size,data);
            std::memcpy(rgba.data()+(std::size_t(y)*i.Width+x)*16,&color,16);
        }
        if(!convert(rgba.data(),D3DFMT_A32B32G32R32F,i.Width,i.Height,i.Width*16,packed.data(),i.Format,i.Width,i.Height,rowBytes(i.Format,i.Width),D3DX_FILTER_NONE)) return invalidData;
        hr=textureBytes(t,0,l,packed,true); if(FAILED(hr)) return hr;
    }
    return D3D_OK;
}
HRESULT WINAPI D3DXSaveTextureToFileA(LPCSTR path,D3DXIMAGE_FILEFORMAT format,IDirect3DBaseTexture9* t,const PALETTEENTRY* palette) {
    if(!path || !t) return D3DERR_INVALIDCALL; if(palette) stop("D3DXSaveTextureToFileA palette");
    if(format!=D3DXIFF_DDS && format!=D3DXIFF_TGA) stop("D3DXSaveTextureToFileA output format");
    Image im; HRESULT hr=describe(t,0,im.info); if(FAILED(hr)) return hr;
    const unsigned faces=t->GetType()==D3DRTYPE_CUBETEXTURE?6:1;
    for(unsigned f=0;f<faces;++f) for(unsigned l=0;l<t->GetLevelCount();++l) { Bytes b; hr=textureBytes(t,f,l,b,false); if(FAILED(hr)) return hr; im.data.push_back(std::move(b)); }
    Bytes bytes; return serialize(im,format,bytes) && writeFile(path,bytes)?D3D_OK:invalidData;
}
HRESULT WINAPI D3DXSaveSurfaceToFileA(LPCSTR path,D3DXIMAGE_FILEFORMAT format,IDirect3DSurface9* s,const PALETTEENTRY* palette,const RECT* rect) {
    if(!path || !s) return D3DERR_INVALIDCALL; if(palette) stop("D3DXSaveSurfaceToFileA palette");
    if(format!=D3DXIFF_DDS && format!=D3DXIFF_TGA && format!=D3DXIFF_PNG && format!=D3DXIFF_JPG) stop("D3DXSaveSurfaceToFileA output format");
    D3DSURFACE_DESC d; HRESULT hr=s->GetDesc(&d); if(FAILED(hr)) return hr; RECT r; if(!rectangle(rect,d.Width,d.Height,r)) return D3DERR_INVALIDCALL;
    Bytes source; hr=surfaceBytes(s,source,false); if(FAILED(hr)) return hr;
    Image im; im.info={UINT(r.right-r.left),UINT(r.bottom-r.top),1,1,d.Format,D3DRTYPE_TEXTURE,format};
    Bytes bytes(imageBytes(d.Format,im.info.Width,im.info.Height)); RECT dr={0,0,LONG(im.info.Width),LONG(im.info.Height)};
    hr=blit(source.data(),d.Format,d.Width,d.Height,rowBytes(d.Format,d.Width),r,bytes,d.Format,im.info.Width,im.info.Height,dr,D3DX_FILTER_NONE,0);
    if(FAILED(hr)) return hr; im.data.push_back(std::move(bytes)); return serialize(im,format,bytes) && writeFile(path,bytes)?D3D_OK:invalidData;
}
HRESULT WINAPI D3DXCreateRenderToSurface(IDirect3DDevice9* d,UINT w,UINT h,D3DFORMAT f,BOOL z,D3DFORMAT zf,ID3DXRenderToSurface** out) {
    if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
    if(!d || !w || !h || !pixelBytes(f) || blockSize(f)>1) return D3DERR_INVALIDCALL;
    auto* r=new RenderToSurface(d,D3DXRTS_DESC{w,h,f,z,zf}); HRESULT hr=r->initialize();
    if(FAILED(hr)) r->Release(); else *out=r; return hr;
}
#endif
