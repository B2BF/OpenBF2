// [TEMP] [CHOSEN, not recovered] Real D3D9 HAL / D3DX stock-shader component.
// Links the actual native renderer objects. No fake COM, file/settings, shader,
// or vertex-declaration providers. Explicit fixture setup supplies a hidden
// window/device and unmodified extracted shader bytes to real D3DXCreateEffect.
// This does not initialize Renderer/StateManager, construct QuadDrawer, or run HUD.
#include <map>
#include <string>
#include <vector>
#include <cstdio>
#define private public
#include "../BF2/RendDX9/Effect.cpp"
#undef private

using namespace dice::hfe::dx9rend;
namespace dice { namespace hfe { namespace dx9rend {
bool createBufferManager(IDirect3DDevice9*);
} } }
namespace {
struct ShaderDefine { const char* name; const char* value; };
#include "../BF2/RendDX9/ShaderManagerDefines.inc"
int failures=0,checks=0;
bool check(bool ok,const char* label)
{ ++checks;std::printf("%s %s\n",ok?"PASS":"FAIL",label);std::fflush(stdout);if(!ok)++failures;return ok; }
std::vector<char> read(const char* path)
{
    std::FILE* f=std::fopen(path,"rb");if(!f)return {};
    std::fseek(f,0,SEEK_END);long bytes=std::ftell(f);std::rewind(f);
    std::vector<char> data(bytes);std::fread(data.data(),1,data.size(),f);std::fclose(f);return data;
}
D3DXHANDLE parameter(ID3DXEffect* fx,const char* name)
{
    return fx->GetParameterByName(nullptr,name);
}
bool saveFrame(IDirect3DDevice9* device,const char* path)
{
    IDirect3DSurface9* target=nullptr;IDirect3DSurface9* copy=nullptr;
    if(FAILED(device->GetRenderTarget(0,&target))) return false;
    D3DSURFACE_DESC desc;target->GetDesc(&desc);
    HRESULT hr=device->CreateOffscreenPlainSurface(desc.Width,desc.Height,desc.Format,D3DPOOL_SYSTEMMEM,&copy,nullptr);
    if(SUCCEEDED(hr)) hr=device->GetRenderTargetData(target,copy);
    target->Release();if(FAILED(hr)){if(copy)copy->Release();return false;}
    D3DLOCKED_RECT locked;hr=copy->LockRect(&locked,nullptr,D3DLOCK_READONLY);
    if(FAILED(hr)){copy->Release();return false;}
    const auto pixel=[&](unsigned x,unsigned y){return reinterpret_cast<const unsigned*>(
        static_cast<const char*>(locked.pBits)+y*locked.Pitch)[x]&0xffffffu;};
    std::printf("readback center=%06X corner=%06X\n",pixel(32,32),pixel(1,1));
    check(pixel(32,32)==0xff0000u && pixel(1,1)==0,"actual stock notex pass produces red interior and black border");
    BITMAPFILEHEADER file{};BITMAPINFOHEADER info{};
    file.bfType=0x4D42;file.bfOffBits=sizeof(file)+sizeof(info);file.bfSize=file.bfOffBits+desc.Width*desc.Height*4;
    info.biSize=sizeof(info);info.biWidth=desc.Width;info.biHeight=-static_cast<LONG>(desc.Height);
    info.biPlanes=1;info.biBitCount=32;info.biCompression=BI_RGB;info.biSizeImage=desc.Width*desc.Height*4;
    std::FILE* output=std::fopen(path,"wb");
    if(output)
    {
        std::fwrite(&file,sizeof(file),1,output);std::fwrite(&info,sizeof(info),1,output);
        for(unsigned y=0;y<desc.Height;++y)std::fwrite(static_cast<const char*>(locked.pBits)+y*locked.Pitch,desc.Width*4,1,output);
        std::fclose(output);
    }
    copy->UnlockRect();copy->Release();return output!=nullptr;
}
}
int main(int argc,char** argv)
{
    if(argc!=3)return 2;
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    renddx9abi::logOpen();
    const auto source=read(argv[1]);if(!check(!source.empty(),"read stock PortedMenuShader bytes"))return 1;
    HWND window=CreateWindowExA(0,"STATIC","BF2 renderer component",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    if(!check(window!=nullptr,"create hidden 64x64 device window"))return 1;
    IDirect3D9* d3d=Direct3DCreate9(D3D_SDK_VERSION);
    if(!check(d3d!=nullptr,"load actual Windows Direct3D9"))return 1;
    D3DADAPTER_IDENTIFIER9 adapter{};d3d->GetAdapterIdentifier(0,0,&adapter);
    std::printf("adapter: %s / %s\n",adapter.Description,adapter.Driver);
    D3DCAPS9 caps{};const HRESULT capsResult=d3d->GetDeviceCaps(0,D3DDEVTYPE_HAL,&caps);
    D3DDISPLAYMODE display{};d3d->GetAdapterDisplayMode(0,&display);
    char modulePath[MAX_PATH]{};GetModuleFileNameA(GetModuleHandleA("d3d9.dll"),modulePath,MAX_PATH);
    std::printf("d3d9=%s; caps=%08X DevCaps=%08X VS=%08X PS=%08X display=%ux%u format=%u\n",
        modulePath,static_cast<unsigned>(capsResult),caps.DevCaps,caps.VertexShaderVersion,caps.PixelShaderVersion,
        display.Width,display.Height,display.Format);
    D3DPRESENT_PARAMETERS pp{};pp.BackBufferWidth=64;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
    pp.BackBufferCount=1;pp.SwapEffect=D3DSWAPEFFECT_DISCARD;pp.hDeviceWindow=window;pp.Windowed=TRUE;
    IDirect3DDevice9* device=nullptr;
    HRESULT hr=d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,D3DCREATE_HARDWARE_VERTEXPROCESSING,&pp,&device);
    std::printf("CreateDevice HRESULT=%08X\n",static_cast<unsigned>(hr));
    if(!check(SUCCEEDED(hr)&&device,"create actual HAL device with hardware vertex processing"))return 1;
    std::vector<D3DXMACRO> defines;for(auto item:kShaderDefines)defines.push_back({item.name,item.value});defines.push_back({nullptr,nullptr});
    ID3DXEffect* fx=nullptr;ID3DXBuffer* errors=nullptr;
    hr=D3DXCreateEffect(device,source.data(),static_cast<unsigned>(source.size()),defines.data(),nullptr,0,nullptr,&fx,&errors);
    if(errors)
    {
        auto text=static_cast<const char*>(errors->GetBufferPointer());
        std::printf("D3DX compiler: %s\n",text);
        errors->Release();
    }
    if(!check(SUCCEEDED(hr)&&fx,"compile unmodified stock shader with recovered static D3D defines"))return 1;
    if(!check(createBufferManager(device),"construct actual BufferManager for real declaration creation"))return 1;
    // Explicit component injection of a REAL D3DX object. Effect::bind's parameter
    // registry and full Renderer initialization are outside this diagnostic scope.
    Effect* effect=new Effect;effect->m_d3dxEffect=fx;
    check(effect->validateTechniques()==2,"actual reflection/validation/state-link postpass");
    auto menu=static_cast<EffectTechnique*>(effectFindTechniqueRecord(effect,"Menu"));
    auto quad=static_cast<EffectTechnique*>(effectFindTechniqueRecord(effect,"QuadWithTexture"));
    auto cache=static_cast<EffectTechnique*>(effectFindTechniqueRecord(effect,"QuadCache"));
    if(!check(menu && quad && cache && menu->states,"stock named records and Menu_States link exist"))return 1;
    check(menu->passes==1 && quad->passes==3 && cache->passes==1 && menu->states->restore==1,"actual D3DX pass counts and Restore metadata");
    if(!check(quad->declaration!=nullptr,"actual BufferManager created D3D vertex declaration"))return 1;
    // [TEMP] Inspection of the non-STL BufferManager wrapper whose real +8 member
    // is the IDirect3DVertexDeclaration9. This does not replace a production call.
    auto decl=*reinterpret_cast<IDirect3DVertexDeclaration9**>(static_cast<char*>(quad->declaration)+8);
    D3DVERTEXELEMENT9 elements[MAXD3DDECLLENGTH+1];UINT count=MAXD3DDECLLENGTH+1;
    hr=decl->GetDeclaration(elements,&count);
    check(SUCCEEDED(hr)&&count==5&&elements[1].Offset==12&&elements[2].Offset==28&&elements[3].Offset==36,
          "driver returns the stock declaration created by recovered annotation reader");
    const float identity[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    for(const char* name:{"mWorld","mView","mProj"})
        fx->SetMatrix(parameter(fx,name),reinterpret_cast<const D3DXMATRIX*>(identity));
    fx->SetInt(parameter(fx,"dwZEnable"),0);
    fx->SetBool(parameter(fx,"bZWriteEnable"),0);
    struct Vertex { float x,y,z,r,g,b,a,u,v,u2,v2; };
    const Vertex vertices[]={
        {-0.75f,-0.75f,0,1,0,0,1,0,1,0,1}, {-0.75f,0.75f,0,1,0,0,1,0,0,0,0},
        {0.75f,-0.75f,0,1,0,0,1,1,1,1,1}, {0.75f,0.75f,0,1,0,0,1,1,0,1,0}};
    check(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0)),"clear actual render target");
    check(SUCCEEDED(device->BeginScene()),"begin actual D3D scene");
    effectSetTechniqueByName(effect,vc71::string("QuadWithTexture"));
    // Direct device setup makes the component boundary explicit: StateManager
    // and its deferred draw application are NOT initialized or bypassed silently.
    check(SUCCEEDED(device->SetVertexDeclaration(decl)),"bind actual recovered declaration directly for component draw");
    unsigned passes=0;
    check(SUCCEEDED(fx->Begin(&passes,0))&&passes==3,"actual D3DX begin validates three-pass technique");
    check(SUCCEEDED(fx->BeginPass(0)),"actual stock notex pass applies shaders/states");
    device->SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
    hr=device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex));
    check(SUCCEEDED(hr),"actual DrawPrimitiveUP executes stock shaders");
    fx->EndPass();
    fx->End();
    check(SUCCEEDED(device->EndScene()),"end actual D3D scene");
    check(saveFrame(device,argv[2]),"save actual GPU readback BMP");
    // Existing renderer final release / BufferManager teardown remain unwritten.
    // Component-owned Effect/BufferManager therefore explicitly live to exit.
    device->Release();d3d->Release();DestroyWindow(window);renddx9abi::logClose();
    std::printf("effect_stock_d3d_runtime: %d checks, %d failures\n",checks,failures);
    return failures?1:0;
}
