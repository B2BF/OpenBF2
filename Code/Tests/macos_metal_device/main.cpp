// [TEMP] [CHOSEN, not recovered] G3 integration test through production subset interfaces.
// A GPU and a logged-in WindowServer session are required. No game content or sockets.
#include "../../BF2/Platform/Gfx/D3D9Subset.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
extern "C" void* metalTestWindow();
extern "C" void* metalTestView(void*);
extern "C" void metalTestResize(void*);
extern "C" void metalTestResizeTo(void*,int,int);
extern "C" void metalTestDestroy(void*);
#define CHECK(expr) do { if (!(expr)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#expr); return 1; } } while (0)
#include "groups.inc"
#include "effect.inc"
#include "review.inc"
#include "d3dx.inc"
#include "menu.inc"
#include "recovery.inc"
#include "ingame.inc"
#include "msaa.inc"
#include "closure.inc"
#include "../caps_reference/caps_fields.h"
#include "ffdevice.inc"

int main(int argc, char** argv)
{
    CHECK(cpuChecks()==0);
    CHECK(cpuFormatChecks()==0);
    if(argc==2 && !std::strcmp(argv[1],"--cpu")) return 0;
    void* window = metalTestWindow();
    CHECK(window);
    CHECK(Direct3DCreate9(0) == nullptr);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(d3d && d3d->GetAdapterCount() == 1);
    CHECK(d3d->AddRef() == 2 && d3d->Release() == 1);
    D3DADAPTER_IDENTIFIER9 identifier = {};
    CHECK(d3d->GetAdapterIdentifier(0,0,&identifier) == D3D_OK && identifier.Description[0]);
    std::printf("GPU: %s\n",identifier.Description);
    CHECK(closureAdapterChecks(d3d)==0);
    D3DDISPLAYMODE mode = {};
    CHECK(d3d->GetAdapterDisplayMode(0,&mode) == D3D_OK && mode.Width && mode.Height);
    const UINT modes = d3d->GetAdapterModeCount(0,D3DFMT_X8R8G8B8);
    CHECK(modes > 0 && d3d->EnumAdapterModes(0,D3DFMT_X8R8G8B8,modes-1,&mode) == D3D_OK);
    CHECK(d3d->EnumAdapterModes(0,D3DFMT_X8R8G8B8,modes,&mode) == D3DERR_INVALIDCALL);
    CHECK(d3d->GetAdapterModeCount(0,D3DFMT_R5G6B5) == 0);
    CHECK(d3d->CheckDeviceType(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,D3DFMT_A8R8G8B8,TRUE) == D3D_OK);
    for (const auto format : {D3DFMT_DXT1,D3DFMT_DXT3,D3DFMT_DXT5})
        CHECK(d3d->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,format) == D3D_OK);
    CHECK(d3d->CheckDeviceFormat(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,D3DFMT_R32F) == D3D_OK);
    DWORD quality = 99;
    CHECK(d3d->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,TRUE,D3DMULTISAMPLE_NONE,&quality) == D3D_OK && quality == 1);
    // MSAA (c-options): every Mac GPU Metal supports renders 4 samples; NONMASKABLE is not offered.
    CHECK(d3d->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_D24S8,TRUE,D3DMULTISAMPLE_4_SAMPLES,&quality) == D3D_OK && quality == 1);
    CHECK(d3d->CheckDeviceMultiSampleType(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,TRUE,D3DMULTISAMPLE_NONMASKABLE,&quality) == D3DERR_NOTAVAILABLE && quality == 0);
    D3DCAPS9 caps = {};
    CHECK(d3d->GetDeviceCaps(0,D3DDEVTYPE_HAL,&caps) == D3D_OK && caps.VertexShaderVersion == 0xfffe0300 && caps.PixelShaderVersion == 0xffff0300);
    D3DPRESENT_PARAMETERS pp = {};
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = metalTestView(window);
    pp.Windowed = TRUE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    IDirect3DDevice9* device = nullptr;
    CHECK(d3d->CreateDevice(0,D3DDEVTYPE_HAL,pp.hDeviceWindow,0x50,&pp,&device) == D3DERR_NOTAVAILABLE && !device);
    CHECK(d3d->CreateDevice(0,D3DDEVTYPE_HAL,pp.hDeviceWindow,0x40,&pp,&device) == D3D_OK && device);
    CHECK(pp.BackBufferWidth == 320 && pp.BackBufferHeight == 240);
    if(argc==3 && !std::strcmp(argv[1],"--ff-device")) {
        const int rc = ffDeviceRun(device, argv[2]);
        CHECK(device->Release()==0 && d3d->Release()==0); metalTestDestroy(window); return rc;
    }
    if(argc==2 && !std::strcmp(argv[1],"--caps")) {   // same lines as Code/Tests/caps_reference (real HAL)
        D3DCAPS9 caps = {};
        CHECK(device->GetDeviceCaps(&caps) == D3D_OK);
        std::printf("device macOS Metal\n");
        bf2PrintCaps(stdout, caps);
        CHECK(device->Release()==0 && d3d->Release()==0); metalTestDestroy(window); return 0;
    }
    if(argc==2 && !std::strcmp(argv[1],"--menu")) {
        CHECK(menuClearChecks(device)==0 && menuDepthExtentChecks(device)==0 && menuJPEGChecks(device)==0 && menuPresentChecks(device,window)==0);
        CHECK(device->Release()==0 && d3d->Release()==0); metalTestDestroy(window); return 0;
    }
    if (argc == 2 && !std::strcmp(argv[1],"--frontier")) {
        // Was the fixed-function vertex frontier (abort); closed in 0.3.0 piece 1. A draw with
        // neither a declaration nor an FVF is D3DERR_INVALIDCALL, inside a scene as outside.
        CHECK(device->DrawPrimitive(D3DPT_TRIANGLELIST,0,1) == D3DERR_INVALIDCALL); // outside a scene
        CHECK(device->BeginScene() == D3D_OK && device->DrawPrimitive(D3DPT_TRIANGLELIST,0,1) == D3DERR_INVALIDCALL);
        CHECK(device->EndScene() == D3D_OK && device->Release() == 0 && d3d->Release() == 0);
        metalTestDestroy(window);
        std::puts("PASS: draw without a declaration or FVF answers D3DERR_INVALIDCALL");
        return 0;
    }
    if(argc==2 && !std::strcmp(argv[1],"--state-frontier")) {
        CHECK(device->SetRenderState(static_cast<D3DRENDERSTATETYPE>(19),99)==D3D_OK);
        device->BeginScene(); device->DrawPrimitive(D3DPT_TRIANGLELIST,0,1); return 2;
    }
    if(argc==2 && !std::strcmp(argv[1],"--closure")) {
        CHECK(closureDeviceChecks(device)==0 && closureEffectChecks(device)==0 && closureLodBiasChecks(device)==0 && closureShaderArrayChecks(device)==0 && closureHigherProfileChecks(device)==0 && closureFlatShadeChecks(device)==0 && closureFixedFunctionVertexChecks(device)==0);
        CHECK(device->Release()==0 && d3d->Release()==0); metalTestDestroy(window); return 0;
    }
    if(argc==2 && !std::strcmp(argv[1],"--ingame")) {
        CHECK(ingameChecks(device)==0);
        CHECK(device->Release()==0 && d3d->Release()==0); metalTestDestroy(window); return 0;
    }
    if(argc==3 && !std::strcmp(argv[1],"--effect")) {
        CHECK(effectChecks(device,argv[2])==0);
        CHECK(device->Release()==0 && d3d->Release()==0); metalTestDestroy(window); return 0;
    }
    CHECK(stateChecks(device)==0 && declarationChecks(device)==0 && textureChecks(device)==0 && bufferChecks(device)==0 && copyChecks(device)==0 && targetQueryChecks(device)==0);
    CHECK(reviewLockChecks(device)==0 && reviewQueryChecks(device)==0 && reviewViewportChecks(device)==0 &&
        reviewStretchChecks(device)==0);
    CHECK(ingameChecks(device)==0);
    // Real-content native effect replay is opt-in through --effect FxNative-directory.
    CHECK(d3dxTextureChecks(device)==0 && d3dxMeshChecks(device)==0);
    CHECK(closureDeviceChecks(device)==0 && closureEffectChecks(device)==0 && closureLodBiasChecks(device)==0 && closureShaderArrayChecks(device)==0 && closureHigherProfileChecks(device)==0 && closureFlatShadeChecks(device)==0 && closureFixedFunctionVertexChecks(device)==0);
    CHECK(menuClearChecks(device)==0 && menuDepthExtentChecks(device)==0 && menuJPEGChecks(device)==0 && menuPresentChecks(device,window)==0);
    IDirect3DSwapChain9* chain = nullptr;
    IDirect3DSurface9 *back = nullptr, *target = nullptr, *depth = nullptr;
    CHECK(device->GetSwapChain(0,&chain) == D3D_OK);
    CHECK(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&back) == D3D_OK);
    CHECK(chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&target) == D3D_OK && target == back);
    CHECK(target->Release() == 1);
    CHECK(device->GetRenderTarget(0,&target) == D3D_OK && target == back);
    CHECK(device->GetDepthStencilSurface(&depth) == D3D_OK);
    CHECK(device->SetDepthStencilSurface(nullptr) == D3D_OK);
    IDirect3DSurface9* absentDepth = depth;
    CHECK(device->GetDepthStencilSurface(&absentDepth) == static_cast<HRESULT>(0x88760866u) && !absentDepth);
    CHECK(device->SetRenderTarget(0,back) == D3D_OK && device->SetDepthStencilSurface(depth) == D3D_OK);
    CHECK(device->Reset(&pp) == D3DERR_INVALIDCALL); // outstanding surfaces
    IDirect3DDevice9* owner = nullptr;
    CHECK(back->GetDevice(&owner) == D3D_OK && owner == device);
    CHECK(owner->Release() == 5); // device + chain + two back references + depth
    D3DSURFACE_DESC desc = {};
    CHECK(back->GetDesc(&desc) == D3D_OK && desc.Width == 320 && desc.Format == D3DFMT_X8R8G8B8);
    D3DVIEWPORT9 viewport = {};
    CHECK(device->GetViewport(&viewport) == D3D_OK && viewport.Width == 320);
    viewport.Width = 0;
    CHECK(device->SetViewport(&viewport) == D3DERR_INVALIDCALL);
    viewport.Width = 320;
    CHECK(device->SetViewport(&viewport) == D3D_OK);
    CHECK(device->EndScene() == D3DERR_INVALIDCALL);
    for (unsigned frame = 0; frame < 3; ++frame) {
        CHECK(device->TestCooperativeLevel() == D3D_OK);
        CHECK(device->BeginScene() == D3D_OK && device->BeginScene() == D3DERR_INVALIDCALL);
        CHECK(device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
            0xff203040u + frame * 0x00202020u,1.0f,frame) == D3D_OK);
        CHECK(device->Present(nullptr,nullptr,nullptr,nullptr) == D3DERR_INVALIDCALL);
        CHECK(device->EndScene() == D3D_OK);
        CHECK((frame == 2 ? chain->Present(nullptr,nullptr,nullptr,nullptr,0) :
            device->Present(nullptr,nullptr,nullptr,nullptr)) == D3D_OK);
    }
    CHECK(target->Release() == 1 && back->Release() == 0 && depth->Release() == 0);
    CHECK(commandFailureChecks(device,chain,window,pp) == 0);
    metalTestResize(window);
    CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
    // Review: Present never answers DEVICENOTRESET in D3D9; RendDX9Draw's Present loop retries
    // that unknown code forever. A resized layer is a lost device until TestCooperativeLevel/Reset.
    CHECK(device->Present(nullptr,nullptr,nullptr,nullptr) == D3DERR_DEVICELOST);
    CHECK(chain->Present(nullptr,nullptr,nullptr,nullptr,0) == D3DERR_DEVICELOST);
    CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
    pp.BackBufferWidth = pp.BackBufferHeight = 0;
    // Review: Windows writes the actual size/count/format back (Wine test_reset, which passes there).
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    CHECK(device->Reset(&pp) == D3D_OK && pp.BackBufferWidth == 400 && pp.BackBufferHeight == 300 &&
        pp.BackBufferCount == 1 && pp.BackBufferFormat == D3DFMT_X8R8G8B8);
    CHECK(device->GetViewport(&viewport) == D3D_OK && viewport.Width == 400 && viewport.Height == 300);
    CHECK(device->TestCooperativeLevel() == D3D_OK);
    // The options menu's resolution change: windowed, the host resizes
    // the window and drawable, then the renderer resets with zero dimensions; full screen, it
    // resets with the mode's explicit size. Each new back buffer clears and presents.
    struct { int w,h; BOOL windowed; } optionModes[] = { {1024,768,TRUE}, {1280,800,FALSE}, {800,600,TRUE} };
    for (const auto& m : optionModes) {
        if (m.windowed) {
            metalTestResizeTo(window,m.w,m.h);
            CHECK(device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET);
            pp.BackBufferWidth = pp.BackBufferHeight = 0;
        } else {
            pp.BackBufferWidth = UINT(m.w); pp.BackBufferHeight = UINT(m.h);
        }
        pp.Windowed = m.windowed;
        CHECK(device->Reset(&pp) == D3D_OK && pp.BackBufferWidth == UINT(m.w) && pp.BackBufferHeight == UINT(m.h));
        CHECK(device->TestCooperativeLevel() == D3D_OK);
        CHECK(device->GetViewport(&viewport) == D3D_OK && viewport.Width == UINT(m.w) && viewport.Height == UINT(m.h));
        IDirect3DSurface9* bb = nullptr; D3DSURFACE_DESC bd = {};
        CHECK(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb) == D3D_OK && bb->GetDesc(&bd) == D3D_OK &&
              bd.Width == UINT(m.w) && bd.Height == UINT(m.h));
        CHECK(bb->Release() == 0);
        CHECK(device->BeginScene() == D3D_OK && device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102030u,1.0f,0) == D3D_OK &&
              device->EndScene() == D3D_OK && device->Present(nullptr,nullptr,nullptr,nullptr) == D3D_OK);
    }
    // The options menu's anti-aliasing (c-options MSAA): a 4-sample back buffer and auto depth; the
    // clear resolves into the readable back buffer, the back buffer reports its sample count, and
    // returning to no multisampling works.
    pp.MultiSampleType = D3DMULTISAMPLE_4_SAMPLES; pp.MultiSampleQuality = 0;
    CHECK(device->Reset(&pp) == D3D_OK && pp.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES);
    {
        IDirect3DSurface9* bb = nullptr; D3DSURFACE_DESC bd = {};
        CHECK(device->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,&bb) == D3D_OK && bb->GetDesc(&bd) == D3D_OK &&
              bd.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES);
        CHECK(bb->Release() == 0);
        CHECK(device->BeginScene() == D3D_OK &&
              device->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff405060u,1.0f,0) == D3D_OK &&
              device->EndScene() == D3D_OK && device->Present(nullptr,nullptr,nullptr,nullptr) == D3D_OK);
        if (msaaSurfaceChecks(device)) return 1;
        if (msaaOverlayDepthChecks(device,pp)) return 1;
    }
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    CHECK(device->Reset(&pp) == D3D_OK && pp.MultiSampleType == D3DMULTISAMPLE_NONE);
    CHECK(device->BeginScene() == D3D_OK && device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff102030u,1.0f,0) == D3D_OK &&
          device->EndScene() == D3D_OK && device->Present(nullptr,nullptr,nullptr,nullptr) == D3D_OK);
    CHECK(chain->Release() == 0);
    CHECK(device->Release() == 0);
    CHECK(d3d->Release() == 0);
    metalTestDestroy(window);
    std::puts("PASS: three clear/present submissions, resize/reset, options resolution changes, 4x MSAA reset/clear/present, multisampled surfaces and the screenshot copy, surface/chain/device/adapter references zero");
    return 0;
}
