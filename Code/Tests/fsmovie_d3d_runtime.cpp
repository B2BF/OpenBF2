// [TEMP] [CHOSEN, not recovered] Hidden-window movie draw integration.
// Actual FSMoviePlayer, Video/Bink, Effect/FXParameter, ShaderManager cache,
// BufferManager, StateManager and renderer frame/draw wrappers run unchanged.
// Explicit device/base setup, cached stock shader injection and the host's
// class/path services replace engine startup. Manager/cache teardown frontiers
// remain outside scope; those objects deliberately live until process exit.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include "../BF2/RendDX9/ShaderManager.cpp"
#include "../BF2/Game/Main/IFSMoviePlayer.h"
namespace dice { namespace hfe { namespace geom {
IBase* __fastcall createFSMoviePlayer(unsigned, IBase*);
IBase* __fastcall createVideo(unsigned, IBase*);
void* videoFrameD3DTexture(void*);
} namespace dx9rend {
bool createStateManager(void*);
bool createBufferManager(IDirect3DDevice9*);
bool stateManagerIsFullyRestored();
} } }
extern "C" long __stdcall D3DXCreateEffect(IDirect3DDevice9*,const void*,unsigned,
    const void*,void*,unsigned,void*,void**,void**);
#pragma comment(linker, "/alternatename:_D3DXCreateEffect@36=_D3DXCreateEffect")
using namespace dice::hfe;
using namespace dice::hfe::dx9rend;
namespace {
int checks=0,failures=0;
void* lastVideo=nullptr;
bool check(bool ok,const char* label) {
    ++checks;if(!ok)++failures;std::printf("%s %s\n",ok?"PASS":"FAIL",label);std::fflush(stdout);return ok;
}
[[noreturn]] void cold() { std::fputs("unexpected movie GPU fixture host call\n",stderr);std::abort(); }
class Paths final : public io::IFileManager {
public:
    std::list<std::string> paths={"mods/bf2/"};
    void* getPathList() const override { return const_cast<std::list<std::string>*>(&paths); }
    std::string stripModPath(const std::string& path) override { return path; }
    void addRef() override { cold(); }
    int getRef() override { cold(); }
    int release() override { cold(); }
    void* queryInterface(unsigned) const override { cold(); }
    void init() override { cold(); }
    bool isLastFileOpenedFromArchive() const override { cold(); }
    io::IStream* open(const std::string&,FileMode,FileAccess,unsigned) override { cold(); }
    bool fileExists(const std::string&) override { cold(); }
    bool deleteFile(const std::string&) override { cold(); }
    bool moveFile(const std::string&,const std::string&) override { cold(); }
    bool copyFile(const std::string&,const std::string&,bool) override { cold(); }
    bool readFile(const std::string&,void*,unsigned,unsigned&) override { cold(); }
    bool writeFile(const std::string&,const void*,unsigned,bool) override { cold(); }
    int findFiles(const std::string&,bool,std::list<io::FileInfo>&) override { cold(); }
    bool getFileInfo(const std::string&,io::FileInfo&) override { cold(); }
    bool compareFileTime(const std::string&,const std::string&) override { cold(); }
    bool getFullFileName(std::string&) override { cold(); }
    void update() override { cold(); }
    const std::string& getHomeSubstitution() const override { cold(); }
    const std::string& getRootSubstitution() const override { cold(); }
    void setBaseSystem(io::IFileSystem*) override { cold(); }
    io::IFileSystem* getBaseSystem() override { cold(); }
    io::IStream* openwin32(const std::string&,FileMode,FileAccess,unsigned) override { cold(); }
    void addArchiveHandler(io::IArchiveHandler*) override { cold(); }
    void removeArchiveHandler(io::IArchiveHandler*) override { cold(); }
    bool addPathOverload(const std::string&,const std::string&) override { cold(); }
    bool removePathOverload(const std::string&,const std::string&) override { cold(); }
    bool addMergeDirectory(const std::string&,const std::string&) override { cold(); }
    bool removeMergeDirectory(const std::string&) override { cold(); }
    io::IArchive* openArchive(const std::string&,int,bool) override { cold(); }
    void permitMountArchives(bool) override { cold(); }
    io::IArchive* mountArchive(const std::string&,const std::string&,bool) override { cold(); }
    bool unmountArchive(const std::string&) override { cold(); }
    void unmountAllArchives() override { cold(); }
    void getAllArchives(std::vector<io::IArchive*>&) override { cold(); }
    bool hasMountedArchives() const override { cold(); }
    void addPath(const std::string&) override { cold(); }
    void removePath(const std::string&) override { cold(); }
    void removeAllPaths() override { cold(); }
    void getWritePath(const std::string&,std::string&) override { cold(); }
    int getFileResolveMode() const override { cold(); }
    void setFileResolveMode(int) override { cold(); }
    std::string fileExistsEx(const std::string&,bool) override { cold(); }
    std::string fileExistsEx2(const std::string&) override { cold(); }
    void setModPaths(std::vector<std::string>) override { cold(); }
};
void* factorySlots[32];struct Factory { void** table; } factory={factorySlots};
IBase* __fastcall makeVideo(void* self,void*,std::string name,unsigned iid,IBase* outer) {
    if(!check(self==&factory && name=="dice.hfe.geom.Video" && iid==0xb801dd04u && !outer,
              "native by-name host call forwards to actual Video factory"))cold();
    IBase* video=geom::createVideo(iid,outer);lastVideo=video;return video;
}
std::vector<char> read(const char* path) {
    FILE* f=std::fopen(path,"rb");if(!f)return {};
    std::fseek(f,0,SEEK_END);const long size=std::ftell(f);std::rewind(f);
    std::vector<char> bytes(size);std::fread(bytes.data(),1,bytes.size(),f);std::fclose(f);return bytes;
}
bool readback(IDirect3DDevice9* device,const char* path) {
    IDirect3DSurface9 *surface=nullptr,*copy=nullptr;
    if(FAILED(device->GetRenderTarget(0,&surface)))return false;
    D3DSURFACE_DESC desc{};surface->GetDesc(&desc);
    HRESULT hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr);
    if(SUCCEEDED(hr))hr=device->GetRenderTargetData(surface,copy);
    surface->Release();if(FAILED(hr)){if(copy)copy->Release();return false;}
    D3DLOCKED_RECT pixels{};if(FAILED(copy->LockRect(&pixels,nullptr,D3DLOCK_READONLY))){copy->Release();return false;}
    auto texture=static_cast<IDirect3DTexture9*>(geom::videoFrameD3DTexture(lastVideo));
    D3DSURFACE_DESC texdesc{};texture->GetLevelDesc(0,&texdesc);
    D3DLOCKED_RECT texels{};
    if(FAILED(texture->LockRect(0,&texels,nullptr,D3DLOCK_READONLY))){copy->UnlockRect();copy->Release();return false;}
    // Independent CPU bilinear sampling at the D3D9 integer pixel centers. The
    // movie's unadjusted clip-space quad maps those centers to (x/W,y/H).
    const auto texel=[&](int x,int y,unsigned shift) {
        x=(x+texdesc.Width)%texdesc.Width;y=(y+texdesc.Height)%texdesc.Height;
        const unsigned value=reinterpret_cast<const unsigned*>(static_cast<const char*>(texels.pBits)+y*texels.Pitch)[x];
        return static_cast<float>((value>>shift)&255u);
    };
    unsigned mismatch=0,samples=0,nonClear=0,nonBlack=0;
    for(unsigned y=2;y+2<desc.Height;y+=3)for(unsigned x=2;x+2<desc.Width;x+=3) {
        const unsigned actual=reinterpret_cast<const unsigned*>(static_cast<const char*>(pixels.pBits)+y*pixels.Pitch)[x]&0xffffffu;
        nonClear+=actual!=0xff0000u;nonBlack+=actual!=0;
        const float sx=static_cast<float>(x)*texdesc.Width/desc.Width-0.5f;
        const float sy=static_cast<float>(y)*texdesc.Height/desc.Height-0.5f;
        const int x0=static_cast<int>(std::floor(sx)),y0=static_cast<int>(std::floor(sy));
        const float fx=sx-x0,fy=sy-y0;
        for(unsigned shift=0;shift<24;shift+=8) {
            const float upper=texel(x0,y0,shift)*(1-fx)+texel(x0+1,y0,shift)*fx;
            const float lower=texel(x0,y0+1,shift)*(1-fx)+texel(x0+1,y0+1,shift)*fx;
            const int expected=static_cast<int>(upper*(1-fy)+lower*fy+0.5f);
            mismatch+=std::abs(static_cast<int>((actual>>shift)&255u)-expected)>3;
            ++samples;
        }
    }
    std::printf("GPU comparison: %u channel samples, %u mismatches, %u non-clear, %u non-black\n",samples,mismatch,nonClear,nonBlack);
    check(mismatch==0,"actual movie draw matches decoded texture sampled through stock shader");
    check(nonClear>100 && nonBlack>10,"readback contains movie detail rather than a clear or empty frame");
    texture->UnlockRect(0);
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
    file.bfType=0x4d42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
    info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);
    info.biPlanes=1;info.biBitCount=32;info.biSizeImage=desc.Width*desc.Height*4;
    FILE* out=std::fopen(path,"wb");if(out){std::fwrite(&file,sizeof(file),1,out);std::fwrite(&info,sizeof(info),1,out);
        for(unsigned y=0;y<desc.Height;++y)std::fwrite(static_cast<const char*>(pixels.pBits)+y*pixels.Pitch,desc.Width*4,1,out);
        std::fclose(out);}
    copy->UnlockRect();copy->Release();return out!=nullptr;
}
}
// Host interface key functions are not part of the renderer image. The concrete
// fixture overrides them; the base entries are cold and must never execute.
namespace dice { namespace hfe { namespace io {
void IFileSystem::init() { cold(); }
bool IFileSystem::isLastFileOpenedFromArchive() const { cold(); }
} } }
int main(int argc,char** argv) {
    if(argc!=4)return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);renddx9abi::logOpen();
    const auto source=read(argv[2]);if(!check(!source.empty(),"read unmodified stock LoadingScreen shader"))return 1;
    if(!check(SetCurrentDirectoryA(argv[1])!=FALSE,"select stock content directory without modifying it"))return 1;
    Paths paths;fileManager=&paths;
    for(void*& slot:factorySlots)slot=reinterpret_cast<void*>(&cold);
    factorySlots[0x2c/4]=reinterpret_cast<void*>(&makeVideo);classManager=reinterpret_cast<IClassManager*>(&factory);
    HWND window=CreateWindowExA(0,"STATIC","BF2 movie GPU component",WS_POPUP,0,0,128,128,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);
    if(!check(window && d3d,"create hidden window and actual D3D9 provider"))return 1;
    D3DADAPTER_IDENTIFIER9 adapter{};d3d->GetAdapterIdentifier(0,0,&adapter);std::printf("adapter=%s driver=%s\n",adapter.Description,adapter.Driver);
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=128;pp.BackBufferHeight=128;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_COPY;pp.hDeviceWindow=window;pp.Windowed=TRUE;
    pp.EnableAutoDepthStencil=TRUE;pp.AutoDepthStencilFormat=D3DFMT_D24S8;pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9* device=nullptr;
    if(!check(SUCCEEDED(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device)),"create actual HAL device and COPY backbuffer"))return 1;
    RendDX9Base base{};base.device=device;base.d3d9=d3d;base.framePresented=1;base.pp=pp;
    g_rendererBase=&base;g_Renderer=reinterpret_cast<IBase*>(&base);
    // Test-owned sentinel for the actual listener registration/unregistration.
    struct Node { Node* next;Node* prev;int priority;DeviceLostResetListener* listener; } head{};
    head.next=head.prev=&head;*reinterpret_cast<Node**>(base.deviceLostList+4)=&head;
    IDirect3DSwapChain9* chain=nullptr;device->GetSwapChain(0,&chain);
    *reinterpret_cast<IDirect3DSwapChain9***>(reinterpret_cast<char*>(&base)+228)=&chain;
    if(!check(createStateManager(device) && createBufferManager(device) && createShaderManager(&base,device),"construct actual state, buffer and shader managers"))return 1;
    void* fx=nullptr;void* errors=nullptr;
    const HRESULT compiled=D3DXCreateEffect(device,source.data(),static_cast<unsigned>(source.size()),nullptr,nullptr,0,nullptr,&fx,&errors);
    if(errors){typedef const char* (__stdcall* Text)(void*);std::printf("D3DX: %s\n",renddx9abi::d3dxSlot<Text>(errors,0x0c)(errors));renddx9abi::d3dxSlot<renddx9abi::D3dxReleaseFn>(errors,8)(errors);}
    if(!check(SUCCEEDED(compiled) && fx,"compile stock LoadingScreen through real D3DX"))return 1;
    Effect* effect=createEffectObject();if(!check(effectBind(effect,fx),"actual effect bind registers parameters and installs real StateManager"))return 1;
    renddx9abi::d3dxSlot<renddx9abi::D3dxReleaseFn>(fx,8)(fx);
    auto manager=static_cast<ShaderManager*>(g_shaderManager);
    auto node=vc71::mapInsert(manager->effectCache,0x30,"mods/xpack/shaders/loadingscreen");vc71::mapNodeValueRef(node)=effect;
    IFSMoviePlayer* movie=reinterpret_cast<IFSMoviePlayer*>(geom::createFSMoviePlayer(80047u,nullptr));
    if(!check(movie && movie->load("Movies/legal.bik"),"actual movie loads stock Bink, cached effect, declaration and vertex buffer"))return 1;
    // The real codec's timing gate may defer the first decode; repeated actual
    // draws are bounded and allow it to advance without rewriting codec state.
    for(unsigned frame=0;frame<20;++frame){movie->drawFrame();Sleep(20);}
    check(base.framePresented && !base.inScene,"actual renderer ended and presented its movie frame");
    check(stateManagerIsFullyRestored(),"movie's actual StateManager push/pop is balanced");
    check(readback(device,argv[3]),"save GPU movie readback BMP");
    movie->release();
    // Do not release the cache's reference: Effect final teardown is a named
    // frontier. Managers and their cache intentionally remain until exit.
    std::printf("FSMovie GPU: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
