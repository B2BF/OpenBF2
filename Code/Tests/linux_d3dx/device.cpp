// [TEMP] [CHOSEN, not recovered] DXVK resource and D3DX helper contracts, issue #36.
#include "../../BF2/Platform/Gfx/Metal/D3DXHelpers.h"
#include "../../BF2/Platform/Gfx/Metal/D3DXMesh.h"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

// Standalone helper fixture: physical file paths, without an engine host.
namespace dice { namespace hfe { namespace io { class IFileManager; } io::IFileManager* fileManager=nullptr; } }
namespace {
using bf2d3dx::Bytes;
template<class T> struct Owner {
    T* value=nullptr;
    ~Owner() { if(value) value->Release(); }
    T* operator->() const { return value; }
};
void check(bool ok,const char* operation) { if(!ok) throw std::runtime_error(operation); }
void hr(HRESULT result,const char* operation) {
    if(FAILED(result)) { char code[16]; std::snprintf(code,sizeof(code),"0x%08x",unsigned(result));
        throw std::runtime_error(std::string(operation)+": "+code); }
}
Bytes pattern(unsigned width,unsigned height,unsigned depth=1) {
    Bytes bytes(std::size_t(width)*height*depth*4);
    for(std::size_t i=0;i<bytes.size()/4;++i) {
        bytes[i*4]=i*17; bytes[i*4+1]=i*5; bytes[i*4+2]=i*3; bytes[i*4+3]=255;
    }
    return bytes;
}
void textureRoundtrip(IDirect3DBaseTexture9* texture,unsigned face,unsigned level,unsigned width,unsigned height,unsigned depth=1) {
    auto source=pattern(width,height,depth); Bytes copy;
    hr(bf2d3dx::textureBytes(texture,face,level,source,true),"Upload texture");
    hr(bf2d3dx::textureBytes(texture,face,level,copy,false),"Read texture");
    check(source==copy,"Texture bytes survive roundtrip");
}
void WINAPI fillColor(D3DXVECTOR4* out,const D3DXVECTOR2*,const D3DXVECTOR2*,void*) {
    *out=D3DXVECTOR4(0.25f,0.5f,0.75f,1.0f);
}
ULONG deviceReferences(IDirect3DDevice9* device) {
    device->AddRef();
    return device->Release();
}
void overlayDepthLifetime(SDL_Window* window) {
    // [TEMP] Public D3D9 lifetime regression for issue #35 / PR #37. The negative
    // control keeps the old missing-release behavior, then releases its test-owned
    // references so this diagnostic does not leave a device alive.
    for(bool omitRelease:{false,true}) {
        Owner<IDirect3D9> d3d; d3d.value=Direct3DCreate9(D3D_SDK_VERSION);
        check(d3d.value!=nullptr,"Create overlay lifetime D3D9");
        D3DPRESENT_PARAMETERS pp={}; pp.BackBufferWidth=pp.BackBufferHeight=64;
        pp.BackBufferFormat=D3DFMT_A8R8G8B8; pp.BackBufferCount=1;
        pp.MultiSampleType=D3DMULTISAMPLE_8_SAMPLES;
        pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D24S8;
        pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow=window; pp.Windowed=TRUE;
        Owner<IDirect3DDevice9> device;
        hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,0x40,&pp,&device.value),"Create multisampled overlay device");
        const ULONG before=deviceReferences(device.value);
        struct SavedDepths {
            std::vector<IDirect3DSurface9*> values;
            ~SavedDepths() { for(auto* value:values) value->Release(); }
        } retained;
        for(unsigned frame=0;frame<3;++frame) {
            Owner<IDirect3DSurface9> saved;
            hr(device->BeginScene(),"Begin overlay frame");
            hr(device->GetDepthStencilSurface(&saved.value),"Save overlay depth surface");
            check(saved.value!=nullptr,"Implicit depth surface exists");
            D3DSURFACE_DESC desc={}; hr(saved->GetDesc(&desc),"Describe overlay depth surface");
            check(desc.MultiSampleType==D3DMULTISAMPLE_8_SAMPLES,"Overlay depth is eight-sample");
            hr(device->SetDepthStencilSurface(nullptr),"Unbind overlay depth surface");
            hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040u,1,0),"Draw overlay without depth");
            hr(device->SetDepthStencilSurface(saved.value),"Restore overlay depth surface");
            if(omitRelease) {
                retained.values.push_back(saved.value); saved.value=nullptr;
            } else {
                saved.value->Release(); saved.value=nullptr;
                check(deviceReferences(device.value)==before,"Balanced overlay restores device reference count");
            }
            hr(device->EndScene(),"End overlay frame");
        }
        const ULONG after=deviceReferences(device.value);
        check(omitRelease?after>before:after==before,"Overlay omission retains the device; balanced path does not");
        IDirect3DDevice9* finalDevice=device.value; device.value=nullptr;
        const ULONG finalReferences=finalDevice->Release();
        check(omitRelease?finalReferences>0:finalReferences==0,"Final device Release detects omitted depth reference");
        std::printf("PASS: overlay depth %s: device references %u -> %u, final Release=%u\n",
            omitRelease?"omission control":"balanced bracket",unsigned(before),unsigned(after),unsigned(finalReferences));
    }
}
void run(SDL_Window* window,const char* output) {
    Owner<IDirect3D9> d3d; d3d.value=Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d.value!=nullptr,"Create D3D9");
    D3DPRESENT_PARAMETERS pp={}; pp.BackBufferWidth=pp.BackBufferHeight=64;
    pp.BackBufferFormat=D3DFMT_X8R8G8B8; pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_DISCARD; pp.hDeviceWindow=window; pp.Windowed=TRUE;
    Owner<IDirect3DDevice9> device;
    hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,0x40,&pp,&device.value),"Create device");
    for(D3DPOOL pool:{D3DPOOL_MANAGED,D3DPOOL_DEFAULT}) {
        Owner<IDirect3DTexture9> texture;
        hr(device->CreateTexture(8,4,3,0,D3DFMT_A8R8G8B8,pool,&texture.value,nullptr),"Create texture");
        textureRoundtrip(texture.value,0,0,8,4);
        textureRoundtrip(texture.value,0,2,2,1);
    }
    Owner<IDirect3DCubeTexture9> cube;
    hr(device->CreateCubeTexture(4,2,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&cube.value,nullptr),"Create cube");
    for(unsigned face=0;face<6;++face) textureRoundtrip(cube.value,face,1,2,2);
    Owner<IDirect3DVolumeTexture9> volume;
    hr(device->CreateVolumeTexture(4,4,4,2,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&volume.value,nullptr),"Create volume");
    textureRoundtrip(volume.value,0,0,4,4,4); textureRoundtrip(volume.value,0,1,2,2,2);
    Owner<IDirect3DTexture9> compressed;
    hr(device->CreateTexture(8,8,3,0,D3DFMT_DXT1,D3DPOOL_MANAGED,&compressed.value,nullptr),"Create BC1");
    Bytes blocks(32,0x55),blockCopy;
    hr(bf2d3dx::textureBytes(compressed.value,0,0,blocks,true),"Upload BC1");
    hr(bf2d3dx::textureBytes(compressed.value,0,0,blockCopy,false),"Read BC1");
    check(blocks==blockCopy,"BC1 blocks survive roundtrip");
    blocks.resize(8);
    hr(bf2d3dx::textureBytes(compressed.value,0,2,blocks,true),"Upload small BC1 mip");
    hr(bf2d3dx::textureBytes(compressed.value,0,2,blockCopy,false),"Read small BC1 mip");
    check(blocks==blockCopy,"Small BC1 mip survives roundtrip");
    Bytes invalid;
    check(FAILED(bf2d3dx::textureBytes(cube.value,6,0,invalid,false)),"Reject invalid face");
    check(FAILED(bf2d3dx::textureBytes(volume.value,0,2,invalid,false)),"Reject invalid level");
    check(FAILED(bf2d3dx::textureBytes(cube.value,0,0,invalid,true)),"Reject short upload");

    // Public D3DX entry points must discriminate forwarded COM textures by GetType,
    // including generated mip levels; the forwarding interfaces have separate C++ RTTI.
    Owner<IDirect3DTexture9> filled;
    hr(device->CreateTexture(8,8,0,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&filled.value,nullptr),"Create fill texture");
    hr(D3DXFillTexture(filled.value,fillColor,nullptr),"Fill forwarded texture");
    Bytes filtered;
    hr(bf2d3dx::textureBytes(filled.value,0,3,filtered,false),"Read filled final mip");
    check(filtered==Bytes({191,128,64,255}),"Fill reaches final mip with requested color");
    for(unsigned face=0;face<6;++face) {
        Bytes constant(4*4*4,static_cast<unsigned char>(32+face));
        hr(bf2d3dx::textureBytes(cube.value,face,0,constant,true),"Seed cube face");
    }
    hr(D3DXFilterTexture(cube.value,nullptr,0,D3DX_FILTER_BOX),"Filter forwarded cube");
    for(unsigned face=0;face<6;++face) {
        hr(bf2d3dx::textureBytes(cube.value,face,1,filtered,false),"Read filtered cube face");
        check(filtered==Bytes(2*2*4,static_cast<unsigned char>(32+face)),"Cube filter preserves each face");
    }
    Bytes volumeColor(4*4*4*4,93);
    hr(bf2d3dx::textureBytes(volume.value,0,0,volumeColor,true),"Seed volume");
    hr(D3DXFilterTexture(volume.value,nullptr,0,D3DX_FILTER_BOX),"Filter forwarded volume");
    hr(bf2d3dx::textureBytes(volume.value,0,1,filtered,false),"Read filtered volume");
    check(filtered==Bytes(2*2*2*4,93),"Volume filter preserves color");
    bf2d3dx::Image dds;
    dds.info={8,8,1,1,D3DFMT_DXT5,D3DRTYPE_TEXTURE,D3DXIFF_DDS};
    dds.data.push_back(Bytes(64,0));
    for(unsigned block=0;block<4;++block) {
        auto* p=dds.data[0].data()+block*16;
        p[0]=p[1]=255; p[9]=p[11]=0xf8; // opaque red BC3 block.
    }
    Bytes encoded;
    check(bf2d3dx::serialize(dds,D3DXIFF_DDS,encoded),"Encode single-level DDS fixture");
    Owner<IDirect3DTexture9> generated;
    hr(D3DXCreateTextureFromFileInMemory(device.value,encoded.data(),encoded.size(),&generated.value),"Load DDS with default mip generation");
    check(generated->GetLevelCount()==4,"Default DDS loader creates full mip chain");
    hr(bf2d3dx::textureBytes(generated.value,0,3,filtered,false),"Read generated BC3 final mip");
    unsigned char pixel[4]={};
    check(bf2d3dx::convert(filtered.data(),D3DFMT_DXT5,1,1,16,pixel,D3DFMT_A8R8G8B8,1,1,4,D3DX_FILTER_NONE),
          "Decode generated BC3 final mip");
    check(pixel[0]==0 && pixel[1]==0 && pixel[2]==255 && pixel[3]==255,"Generated BC3 mip retains red");

    Owner<IDirect3DSurface9> target;
    hr(device->CreateRenderTarget(8,4,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target.value,nullptr),"Create target");
    auto source=pattern(8,4); Bytes copy;
    hr(bf2d3dx::surfaceBytes(target.value,source,true),"Upload render target");
    hr(bf2d3dx::surfaceBytes(target.value,copy,false),"Read render target");
    check(copy==source,"Render target staging roundtrip");
    hr(D3DXSaveSurfaceToFileA(output,D3DXIFF_PNG,target.value,nullptr,nullptr),"Save surface PNG");
    Owner<IDirect3DTexture9> loaded;
    hr(D3DXCreateTextureFromFileExA(device.value,output,8,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,
        D3DX_FILTER_NONE,D3DX_FILTER_NONE,0,nullptr,nullptr,&loaded.value),"Reload surface PNG");
    hr(bf2d3dx::textureBytes(loaded.value,0,0,copy,false),"Read loaded PNG");
    check(copy==source,"Screenshot PNG reload preserves bytes");

    Owner<IDirect3DSurface9> loadedSurface;
    hr(loaded->GetSurfaceLevel(0,&loadedSurface.value),"Get render-to-surface output");
    Owner<ID3DXRenderToSurface> renderer;
    hr(D3DXCreateRenderToSurface(device.value,8,4,D3DFMT_A8R8G8B8,FALSE,D3DFMT_UNKNOWN,&renderer.value),"Create render-to-surface");
    hr(renderer->BeginScene(loadedSurface.value,nullptr),"Begin render-to-surface");
    hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0),"Clear render-to-surface");
    hr(renderer->EndScene(D3DX_FILTER_NONE),"End render-to-surface");
    hr(bf2d3dx::textureBytes(loaded.value,0,0,copy,false),"Read render-to-surface result");
    for(std::size_t i=0;i<copy.size();i+=4)
        check(copy[i]==0x56 && copy[i+1]==0x34 && copy[i+2]==0x12 && copy[i+3]==0xff,"Render-to-surface updates managed texture");
    const std::string jpegPath=std::string(output)+".jpg";
    hr(D3DXSaveSurfaceToFileA(jpegPath.c_str(),D3DXIFF_JPG,loadedSurface.value,nullptr,nullptr),"Save JPEG");
    Owner<IDirect3DTexture9> jpeg;
    hr(D3DXCreateTextureFromFileExA(device.value,jpegPath.c_str(),8,4,1,0,D3DFMT_X8R8G8B8,D3DPOOL_MANAGED,
        D3DX_FILTER_NONE,D3DX_FILTER_NONE,0,nullptr,nullptr,&jpeg.value),"Reload JPEG");
    hr(bf2d3dx::textureBytes(jpeg.value,0,0,copy,false),"Read JPEG");
    for(std::size_t i=0;i<copy.size();i+=4)
        check(std::abs(int(copy[i])-0x56)<=3 && std::abs(int(copy[i+1])-0x34)<=3 &&
            std::abs(int(copy[i+2])-0x12)<=3 && copy[i+3]==0xff,"JPEG encoded color and orientation");

    DWORD quality=0;
    if(SUCCEEDED(d3d->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_A8R8G8B8,TRUE,D3DMULTISAMPLE_2_SAMPLES,&quality))) {
        Owner<IDirect3DSurface9> multisampled,saved;
        D3DVIEWPORT9 viewport={}; hr(device->GetViewport(&viewport),"Save viewport");
        hr(device->GetRenderTarget(0,&saved.value),"Save render target");
        hr(device->CreateRenderTarget(8,4,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_2_SAMPLES,0,FALSE,&multisampled.value,nullptr),"Create multisampled target");
        hr(device->SetRenderTarget(0,multisampled.value),"Set multisampled target");
        hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff123456,1,0),"Clear multisampled target");
        hr(bf2d3dx::surfaceBytes(multisampled.value,copy,false),"Resolve multisampled target for readback");
        for(std::size_t i=0;i<copy.size();i+=4)
            check(copy[i]==0x56 && copy[i+1]==0x34 && copy[i+2]==0x12 && copy[i+3]==0xff,"Multisample readback color");
        hr(device->SetRenderTarget(0,saved.value),"Restore render target");
        hr(device->SetViewport(&viewport),"Restore viewport");
        std::puts("PASS: multisample surface readback");
    } else std::puts("SKIP: GPU lacks 2x multisample render targets");

    Owner<ID3DXMesh> mesh; Owner<ID3DXBuffer> adjacency;
    hr(D3DXCreateBox(device.value,2,4,6,&mesh.value,&adjacency.value),"Create D3DX box");
    check(mesh->GetNumFaces()==12 && mesh->GetNumVertices()==24,"Box topology");
    hr(D3DXComputeNormals(mesh.value,static_cast<const DWORD*>(adjacency->GetBufferPointer())),"Compute normals");
    void* vertices=nullptr; hr(mesh->LockVertexBuffer(D3DLOCK_READONLY,&vertices),"Lock mesh vertices");
    const auto* typed=static_cast<const bf2d3dx::MeshVertex*>(vertices);
    for(unsigned i=0;i<mesh->GetNumVertices();++i) {
        const auto& n=typed[i].normal;
        check(std::abs(n.x*n.x+n.y*n.y+n.z*n.z-1)<0.0001f,"Unit mesh normal");
    }
    hr(mesh->UnlockVertexBuffer(),"Unlock mesh vertices");

    Owner<ID3DXLine> line; hr(D3DXCreateLine(device.value,&line.value),"Create D3DX line");
    hr(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0),"Clear backbuffer");
    hr(device->BeginScene(),"Begin scene"); hr(line->SetWidth(4),"Line width");
    hr(line->Begin(),"Begin line"); const D3DXVECTOR2 points[]={{8,32},{56,32}};
    hr(line->Draw(points,2,0xffff0000),"Draw red line"); hr(line->End(),"End line");
    hr(device->EndScene(),"End scene");
    Owner<IDirect3DSurface9> back;
    hr(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back.value),"Get backbuffer");
    hr(bf2d3dx::surfaceBytes(back.value,copy,false),"Read drawn line");
    const std::size_t center=(32*64+32)*4;
    check(copy[center+2]>240 && copy[center]<10 && copy[center+1]<10,"Line reaches red center pixel");
    std::puts("PASS: 2D/default/cube/volume/BC1 transfers, render-target staging, PNG screenshot reload, render-to-surface, mesh normals, line GPU pixels");
}
}
int main(int argc,char** argv) {
    if(argc!=2) { std::fprintf(stderr,"usage: %s OUTPUT.png\n",argv[0]); return 2; }
    setenv("DXVK_WSI_DRIVER","SDL3",1);
    if(!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr,"SDL: %s\n",SDL_GetError()); return 1; }
    SDL_Window* window=SDL_CreateWindow("OpenBF2 D3DX helper check",64,64,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);
    int result=0;
    try { check(window!=nullptr,SDL_GetError()); run(window,argv[1]); overlayDepthLifetime(window); }
    catch(const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); result=1; }
    SDL_DestroyWindow(window); SDL_Quit(); return result;
}
