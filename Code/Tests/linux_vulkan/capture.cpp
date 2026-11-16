// [TEMP] [CHOSEN, not recovered] Issue #36 display capture contract on a real device.
#include "../../BF2/Platform/Linux/FrameCapture.h"
#include "../../BF2/Platform/InputScript.h"
#include "../../BF2/Platform/Gfx/Metal/D3DXHelpers.h"
#include <SDL3/SDL.h>
#include <png.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

// Standalone image helpers use physical test paths, without an engine host.
namespace dice { namespace hfe { namespace io { class IFileManager; } io::IFileManager* fileManager=nullptr; } }
namespace {
namespace fs=std::filesystem;
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
struct Raster {
    png_image image={};
    std::vector<unsigned char> rgba;
    explicit Raster(const fs::path& path) {
        image.version=PNG_IMAGE_VERSION;
        check(png_image_begin_read_from_file(&image,path.c_str())!=0,"Read PNG header");
        image.format=PNG_FORMAT_RGBA;
        rgba.resize(PNG_IMAGE_SIZE(image));
        check(png_image_finish_read(&image,nullptr,rgba.data(),0,nullptr)!=0,"Decode PNG pixels");
    }
    ~Raster() { png_image_free(&image); }
};
void run(SDL_Window* window,const fs::path& output,D3DMULTISAMPLE_TYPE samples) {
    constexpr unsigned width=64,height=48;
    Owner<IDirect3D9> d3d; d3d.value=Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d.value!=nullptr,"Create D3D9");
    D3DPRESENT_PARAMETERS pp={}; pp.BackBufferWidth=width; pp.BackBufferHeight=height;
    pp.BackBufferFormat=D3DFMT_A8R8G8B8; pp.BackBufferCount=1;
    pp.MultiSampleType=samples; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=window; pp.Windowed=TRUE;
    Owner<IDirect3DDevice9> device;
    hr(d3d->CreateDevice(0,D3DDEVTYPE_HAL,window,0x40,&pp,&device.value),"Create capture device");
    const D3DCOLOR colors[]={0x001f7fbfu,0x403fc181u,0x80c1175fu,0xff7f51e3u};
    const D3DRECT rectangles[]={{0,0,32,24},{32,0,64,24},{0,24,32,48},{32,24,64,48}};
    for(unsigned i=0;i<4;++i)
        hr(device->Clear(1,&rectangles[i],D3DCLEAR_TARGET,colors[i],1,0),"Draw colored alpha regions");
    Owner<IDirect3DSwapChain9> chain;
    hr(device->GetSwapChain(0,&chain.value),"Get capture swap chain");
    Owner<IDirect3DSurface9> back;
    hr(chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&back.value),"Get reference backbuffer");
    D3DSURFACE_DESC description={}; hr(back->GetDesc(&description),"Get backbuffer description");
    check(description.MultiSampleType==samples,"Requested sample count is active");
    bf2d3dx::Bytes reference;
    hr(bf2d3dx::surfaceBytes(back.value,reference,false),"Resolve reference RGB and alpha");
    check(reference.size()==width*height*4,"Reference image dimensions");
    unsigned nonopaque=0;
    for(std::size_t i=3;i<reference.size();i+=4) nonopaque+=reference[i]!=255;
    check(nonopaque==width*height*3/4,"Reference contains visible RGB with nonopaque alpha");

    fs::create_directories(output);
    check(setenv("BF2_CAPTURE_DIR",output.c_str(),1)==0,"Set capture directory");
    dice::hfe::inputscript::inputScript().requestCapture("alpha");
    bf2LinuxCaptureBeforePresent(chain.value);
    fs::path captured;
    for(const auto& file:fs::directory_iterator(output))
        if(file.path().extension()==".png" && file.path().filename().string().find("present-")==0) {
            check(captured.empty(),"Exactly one queued display capture"); captured=file.path();
        }
    check(!captured.empty(),"Display capture was written");
    Raster display(captured);
    check(display.image.width==width && display.image.height==height,"Display PNG dimensions");
    std::ifstream stream(captured,std::ios::binary); unsigned char header[26]={};
    stream.read(reinterpret_cast<char*>(header),sizeof header);
    check(stream.gcount()==sizeof header && header[25]==PNG_COLOR_TYPE_RGB,"Display PNG has no alpha channel");
    const auto direct=output/"surface-alpha.png";
    hr(D3DXSaveSurfaceToFileA(direct.c_str(),D3DXIFF_PNG,back.value,nullptr,nullptr),"Direct D3DX export");
    Raster asset(direct);
    check(asset.image.width==width && asset.image.height==height,"Direct PNG dimensions");
    for(std::size_t i=0;i<reference.size();i+=4) {
        for(unsigned channel=0;channel<3;++channel) {
            check(display.rgba[i+channel]==reference[i+2-channel],"Display RGB equals resolved backbuffer");
            check(asset.rgba[i+channel]==reference[i+2-channel],"Direct D3DX RGB is preserved");
        }
        check(display.rgba[i+3]==255,"Display output is opaque");
        check(asset.rgba[i+3]==reference[i+3],"General D3DX export preserves alpha");
    }
    std::string unused;
    check(!dice::hfe::inputscript::inputScript().takeCaptureRequest(unused),"Capture request is consumed once");
    std::printf("PASS: %u-sample display PNG preserves RGB and is opaque; direct export preserves %u nonopaque pixels\n",
        samples==D3DMULTISAMPLE_NONE?1u:unsigned(samples),nonopaque);
}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"usage: bf2_linux_capture OUTPUT_PARENT");
        const auto output=fs::absolute(argv[1]); fs::create_directories(output);
        std::string pattern=(output/"capture-XXXXXX").string();
        std::vector<char> scratch(pattern.begin(),pattern.end()); scratch.push_back(0);
        check(mkdtemp(scratch.data())!=nullptr,"Create isolated capture directory");
        const fs::path captureRoot=scratch.data();
        setenv("DXVK_WSI_DRIVER","SDL3",1);
        check(SDL_Init(SDL_INIT_VIDEO),SDL_GetError());
        SDL_Window* window=SDL_CreateWindow("Capture regression",64,48,SDL_WINDOW_VULKAN|SDL_WINDOW_HIDDEN);
        check(window!=nullptr,SDL_GetError());
        run(window,captureRoot/"single",D3DMULTISAMPLE_NONE);
        run(window,captureRoot/"msaa8",D3DMULTISAMPLE_8_SAMPLES);
        SDL_DestroyWindow(window); SDL_Quit();
        std::printf("Artifacts: %s\n",captureRoot.c_str()); return 0;
    } catch(const std::exception& error) {
        std::fprintf(stderr,"FAIL: %s\n",error.what()); SDL_Quit(); return 1;
    }
}
