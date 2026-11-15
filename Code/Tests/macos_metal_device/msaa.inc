// [TEMP] [CHOSEN, not recovered] Multisampled surfaces (rend-msaa), run on a
// device whose back buffer is 4x: creation and refusals, clear + StretchRect resolve, the D3D9
// GetRenderTargetData refusal, a mismatched render target/depth pair, StretchRect into the
// multisampled back buffer, and the screenshot copy (SCRATCH surface, D3DXLoadSurfaceFromSurface,
// PNG and JPEG files; RendDX9 PE 0x10005200).
#include "../../BF2/Platform/Gfx/D3DX9Subset.h"
#include <string>
#include <unistd.h>
static unsigned msaaPixel(IDirect3DDevice9* d, IDirect3DSurface9* target, unsigned w, unsigned h) {
    IDirect3DSurface9* sys = nullptr; D3DLOCKED_RECT r = {}; unsigned v = 0;
    if (d->CreateOffscreenPlainSurface(w,h,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr) != D3D_OK) return 0;
    if (d->GetRenderTargetData(target,sys) == D3D_OK && sys->LockRect(&r,nullptr,D3DLOCK_READONLY) == D3D_OK) {
        std::memcpy(&v,r.pBits,4); sys->UnlockRect();
    }
    sys->Release(); return v;
}
static int msaaSurfaceChecks(IDirect3DDevice9* d) {
    IDirect3DSurface9 *ms = nullptr, *msDepth = nullptr, *single = nullptr, *refused = nullptr, *back = nullptr, *depth = nullptr;
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONMASKABLE,0,FALSE,&refused,nullptr) == D3DERR_NOTAVAILABLE && !refused);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_4_SAMPLES,1,FALSE,&refused,nullptr) == D3DERR_NOTAVAILABLE && !refused);
    CHECK(d->CreateDepthStencilSurface(8,8,D3DFMT_D24S8,D3DMULTISAMPLE_NONMASKABLE,0,FALSE,&refused,nullptr) == D3DERR_NOTAVAILABLE);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_4_SAMPLES,0,FALSE,&ms,nullptr) == D3D_OK);
    CHECK(d->CreateDepthStencilSurface(8,8,D3DFMT_D24S8,D3DMULTISAMPLE_4_SAMPLES,0,FALSE,&msDepth,nullptr) == D3D_OK);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&single,nullptr) == D3D_OK);
    D3DSURFACE_DESC desc = {};
    CHECK(ms->GetDesc(&desc) == D3D_OK && desc.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES && desc.Usage == D3DUSAGE_RENDERTARGET);
    CHECK(msDepth->GetDesc(&desc) == D3D_OK && desc.MultiSampleType == D3DMULTISAMPLE_4_SAMPLES);
    D3DLOCKED_RECT locked = {};
    CHECK(ms->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3DERR_INVALIDCALL);
    CHECK(d->GetRenderTarget(0,&back) == D3D_OK && d->GetDepthStencilSurface(&depth) == D3D_OK);
    // A matched multisampled pair: clear, then StretchRect resolves into the single-sample target.
    CHECK(d->SetRenderTarget(0,ms) == D3D_OK && d->SetDepthStencilSurface(msDepth) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,0xff336699u,1.0f,0) == D3D_OK &&
          d->EndScene() == D3D_OK);
    const RECT half = {0,0,4,8};
    CHECK(d->Clear(1,reinterpret_cast<const D3DRECT*>(&half),D3DCLEAR_TARGET,0xff112233u,1.0f,0) == D3D_OK); // a partial clear of samples
    CHECK(d->StretchRect(ms,nullptr,single,nullptr,D3DTEXF_NONE) == D3D_OK);
    IDirect3DSurface9* sys = nullptr;
    CHECK(d->CreateOffscreenPlainSurface(8,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,nullptr) == D3D_OK);
    CHECK(d->GetRenderTargetData(ms,sys) == D3DERR_INVALIDCALL);       // D3D9: the render target is multisampled
    CHECK(d->GetRenderTargetData(back,sys) == D3DERR_INVALIDCALL);     // the 4x back buffer too
    CHECK(d->GetRenderTargetData(single,sys) == D3D_OK && sys->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3D_OK);
    const unsigned* row = static_cast<const unsigned*>(locked.pBits);
    CHECK(row[0] == 0xff112233u && row[7] == 0xff336699u);
    CHECK(sys->UnlockRect() == D3D_OK && sys->Release() == 0);
    // A mismatched pair (single-sample target, 4x depth) clears both buffers.
    CHECK(d->SetRenderTarget(0,single) == D3D_OK);
    CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xff00ff00u,0.5f,0) == D3D_OK);
    CHECK(msaaPixel(d,single,8,8) == 0xff00ff00u);
    // StretchRect into a multisampled surface stops by name; into the 4x back buffer it lands in
    // every sample, so a later clear-free pass keeps it and the present shows it.
    CHECK(d->SetRenderTarget(0,back) == D3D_OK && d->SetDepthStencilSurface(depth) == D3D_OK);
    D3DSURFACE_DESC bd = {}; CHECK(back->GetDesc(&bd) == D3D_OK);
    IDirect3DSurface9* fill = nullptr;
    CHECK(d->CreateRenderTarget(bd.Width,bd.Height,bd.Format,D3DMULTISAMPLE_NONE,0,FALSE,&fill,nullptr) == D3D_OK);
    CHECK(d->SetRenderTarget(0,fill) == D3D_OK && d->SetDepthStencilSurface(nullptr) == D3D_OK &&
          d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff804020u,1.0f,0) == D3D_OK);
    CHECK(d->SetRenderTarget(0,back) == D3D_OK && d->SetDepthStencilSurface(depth) == D3D_OK);
    CHECK(d->StretchRect(fill,nullptr,back,nullptr,D3DTEXF_NONE) == D3D_OK);
    CHECK(fill->Release() == 0);
    // A partial colour clear loads the samples and resolves them: the copy must be in the samples.
    const RECT corner = {0,0,1,1};
    CHECK(d->Clear(1,reinterpret_cast<const D3DRECT*>(&corner),D3DCLEAR_TARGET,0xff010203u,1.0f,0) == D3D_OK);
    // Terrain::drawMain's copy under multisampling: the X8R8G8B8 back buffer into an A8R8G8B8 target.
    if (bd.Format == D3DFMT_X8R8G8B8) {
        IDirect3DSurface9* argb = nullptr;
        CHECK(d->CreateRenderTarget(bd.Width,bd.Height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&argb,nullptr) == D3D_OK);
        CHECK(d->StretchRect(back,nullptr,argb,nullptr,D3DTEXF_NONE) == D3D_OK);
        CHECK(msaaPixel(d,argb,bd.Width,bd.Height) == 0xff010203u);
        CHECK(argb->Release() == 0);
    }
    // The screenshot copy: SCRATCH surface, back buffer load, PNG and JPEG files.
    IDirect3DSurface9 *capture = nullptr, *chainBack = nullptr; IDirect3DSwapChain9* chain = nullptr;
    CHECK(d->CreateOffscreenPlainSurface(bd.Width,bd.Height,D3DFMT_A8R8G8B8,D3DPOOL_SCRATCH,&capture,nullptr) == D3D_OK);
    CHECK(capture->GetDesc(&desc) == D3D_OK && desc.Pool == D3DPOOL_SCRATCH);
    CHECK(d->StretchRect(back,nullptr,capture,nullptr,D3DTEXF_NONE) == D3DERR_INVALIDCALL); // SCRATCH: no device copies
    CHECK(d->GetSwapChain(0,&chain) == D3D_OK && chain->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&chainBack) == D3D_OK);
    CHECK(D3DXLoadSurfaceFromSurface(capture,nullptr,nullptr,chainBack,nullptr,nullptr,D3DX_FILTER_NONE,0) == D3D_OK);
    CHECK(capture->LockRect(&locked,nullptr,D3DLOCK_READONLY) == D3D_OK);
    const unsigned* pixels = static_cast<const unsigned*>(locked.pBits);
    CHECK(pixels[0] == 0xff010203u && pixels[bd.Width*(bd.Height-1)+bd.Width-1] == 0xff804020u);
    CHECK(capture->UnlockRect() == D3D_OK);
    char temp[] = "/private/tmp/openbf2-msaa-XXXXXX"; CHECK(mkdtemp(temp));
    const std::string png = std::string(temp)+"\\screen000.png", jpg = std::string(temp)+"/screen000.jpg";
    CHECK(D3DXSaveSurfaceToFileA(png.c_str(),D3DXIFF_PNG,capture,nullptr,nullptr) == D3D_OK); // backslash separator
    CHECK(D3DXSaveSurfaceToFileA(jpg.c_str(),D3DXIFF_JPG,capture,nullptr,nullptr) == D3D_OK);
    D3DXIMAGE_INFO info = {};
    const std::string pngPath = std::string(temp)+"/screen000.png";
    CHECK(D3DXGetImageInfoFromFileA(pngPath.c_str(),&info) == D3D_OK && info.ImageFileFormat == D3DXIFF_PNG &&
          info.Width == bd.Width && info.Height == bd.Height);
    CHECK(D3DXGetImageInfoFromFileA(jpg.c_str(),&info) == D3D_OK && info.ImageFileFormat == D3DXIFF_JPG && info.Width == bd.Width);
    unlink(pngPath.c_str()); unlink(jpg.c_str()); rmdir(temp);
    CHECK(capture->Release() == 0);
    chainBack->Release(); chain->Release();
    single->Release(); msDepth->Release(); ms->Release(); back->Release(); depth->Release();
    std::puts("PASS: 4x render target/depth surfaces, refusals, StretchRect resolve, mismatched clear, back-buffer copy, screenshot PNG/JPEG");
    return 0;
}
// The overlay bracket of Renderer::renderFrame under multisampling (RendDX9 PE 0x100306D2 and
// 0x10030AE7): GetDepthStencilSurface, SetDepthStencilSurface(0), the overlay passes, then the
// saved surface back. A reference left on the implicit depth-stencil surface keeps the device
// referenced (the final release at shutdown fails, RendDX9Init.cpp:738) and refuses Reset; the
// bracket must leave the device count where it was.
static int msaaOverlayDepthChecks(IDirect3DDevice9* d, D3DPRESENT_PARAMETERS& pp) {
    const ULONG before = (d->AddRef(), d->Release());
    IDirect3DSurface9* saved = nullptr;
    for (unsigned frame = 0; frame < 3; ++frame) {
        saved = nullptr;
        CHECK(d->BeginScene() == D3D_OK && d->GetDepthStencilSurface(&saved) == D3D_OK && saved);
        CHECK(d->SetDepthStencilSurface(nullptr) == D3D_OK);
        CHECK(d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff203040u,1.0f,0) == D3D_OK);
        CHECK(d->SetDepthStencilSurface(saved) == D3D_OK);
        CHECK(saved->Release() == 0);
        CHECK(d->EndScene() == D3D_OK && d->Present(nullptr,nullptr,nullptr,nullptr) == D3D_OK);
    }
    CHECK((d->AddRef(), d->Release()) == before);
    // One frame without the release: the device stays referenced and Reset is refused.
    CHECK(d->GetDepthStencilSurface(&saved) == D3D_OK && (d->AddRef(), d->Release()) == before + 1);
    CHECK(d->Reset(&pp) == D3DERR_INVALIDCALL);
    CHECK(saved->Release() == 0 && (d->AddRef(), d->Release()) == before);
    CHECK(d->Reset(&pp) == D3D_OK);
    std::puts("PASS: multisampled overlay depth-stencil bracket leaves the device count unchanged; a leaked depth reference refuses Reset");
    return 0;
}
