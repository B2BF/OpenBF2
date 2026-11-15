// [CHOSEN, not recovered] Owner-requested Metal seam.
// Native C++ objects implement the public subset; no retail DLL/vtable is bridged.
#if defined(__APPLE__) && defined(BF2_IMAGE_RENDDX9)
#include "../D3D9Subset.h"
#include "../../macOS/CocoaHost.h"
#include "MetalBridge.h"
#include "DrawBridge.h"
#include "FFVertex.h"
#include "EffectShaders.h"
#include "../Effect/EffectData.h"
#include "D3DXHelpers.h"
#include "MenuBackend.h"
#include "RefTrace.h"
#include "../Unsupported.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <vector>
#include <memory>
#include <map>

#include "RefTrace.inc" // [TEMP] [RETIRE-WHEN: rend-deinit-reftrace]

namespace {
using bf2metal::frontier;
using bf2metal::mainThread;

bool colorFormat(D3DFORMAT f) { return f == D3DFMT_X8R8G8B8 || f == D3DFMT_A8R8G8B8; }
bool adapterType(UINT a, D3DDEVTYPE t) { return a == 0 && t == D3DDEVTYPE_HAL; }
void caps(D3DCAPS9& c)
{
    c = {};
    c.DeviceType = D3DDEVTYPE_HAL;
    c.PresentationIntervals = D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_IMMEDIATE;
    c.DevCaps = 0x90000; // HWTRANSFORMANDLIGHT | HWRASTERIZATION; no pure/software device.
    // SM3 target contract. These bits describe the eventual state/shader backend;
    // G3b records state/resources; shader creation and every draw still stop explicitly.
    c.PrimitiveMiscCaps = 0x00000bf2; // masks, culling, clipping, color writes, blend operations
    c.RasterCaps = 0x07422090; // depth test/bias, vertex fog, LOD, anisotropy, perspective, scissor
    c.ZCmpCaps = c.AlphaCmpCaps = 0xff;
    c.SrcBlendCaps = c.DestBlendCaps = 0x23ff; // zero/one, color/alpha inverses, blend factor
    c.TextureCaps = 0x0001ec45; // alpha, perspective, repeat, projection, 2D/cube/volume mip chains
    c.TextureFilterCaps = 0x03030700; // point/linear/mip, anisotropic minification
    c.CubeTextureFilterCaps = c.VolumeTextureFilterCaps = 0x03030300;
    c.TextureAddressCaps = c.VolumeTextureAddressCaps = 0x3f;
    c.MaxTextureWidth = c.MaxTextureHeight = 8192;
    c.MaxVolumeExtent = 256;
    c.MaxTextureRepeat = c.MaxTextureAspectRatio = 8192;
    c.MaxAnisotropy = 16; // State mapping ceiling; GPU sampler objects are constructed by G4.
    c.MaxVertexW = 1.0e10f;
    c.GuardBandLeft = c.GuardBandTop = -8192;
    c.GuardBandRight = c.GuardBandBottom = 8192;
    c.StencilCaps = 0x1ff; // all eight operations, two-sided stencil
    c.FVFCaps = 0x100008; // per-vertex point size, 8 texture coordinate sets
    // Fixed-function vertex processing (FixedFunctionDraw.inc, 0.3.0 pieces 1-2): texgen, material
    // sources, directional and positional lights, local viewer, sphere map; 8 lights; 4 blend
    // matrices, indexed with 256 matrices.
    c.VertexProcessingCaps = 0x13b;
    c.MaxActiveLights = 8;
    c.MaxVertexBlendMatrices = 4;
    c.MaxVertexBlendMatrixIndex = 255;
    c.MaxUserClipPlanes = 6;
    c.MaxPointSize = 1;
    c.MaxPrimitiveCount = 1048575;
    c.MaxVertexIndex = 1048575;
    c.MaxStreams = 16;
    c.MaxStreamStride = 255;
    c.VertexShaderVersion = 0xfffe0300;
    c.PixelShaderVersion = 0xffff0300;
    c.MaxVertexShaderConst = 256;
    c.PixelShader1xMaxValue = 1;
    c.DevCaps2 = 0x51; // stream offsets, StretchRect from textures, shared declaration offsets
    c.NumberOfAdaptersInGroup = 1;
    c.NumSimultaneousRTs = 1;
    c.DeclTypes = 0x30f; // UBYTE4/N, SHORT2N/4N, FLOAT16_2/4
    c.MaxSimultaneousTextures = 16;
    c.VS20Caps = {1, 24, 32, 4}; // predication
    c.PS20Caps = {0x1f, 24, 32, 4, 512}; // SM3 swizzle/gradient/predication/read limits
    c.MaxVShaderInstructionsExecuted = 65535;
    c.MaxPShaderInstructionsExecuted = 65536;
    c.VertexTextureFilterCaps = 0x01000100; // point min/mag
    c.MaxVertexShader30InstructionSlots = c.MaxPixelShader30InstructionSlots = 512;
}

class Device;
#include "Resources.inc"
#include "StateCache.inc"
#include "Shaders.inc"
class SwapChain final : public IDirect3DSwapChain9 {
public:
    Device* device;
    ULONG refs = 0;
    explicit SwapChain(Device* d) : device(d) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        static const GUID iidSwapChain={0x794950f2,0xadfc,0x458a,{0x90,0x5e,0x10,0xa1,0x0b,0x0b,0x50,0x3b}};
        if(!sameIID(id,iidUnknown) && !sameIID(id,iidSwapChain)) return E_NOINTERFACE;
        *out=static_cast<IDirect3DSwapChain9*>(this); AddRef(); return D3D_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE Present(const RECT*,const RECT*,HWND,const RGNDATA*,DWORD) override;
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT,D3DBACKBUFFER_TYPE,IDirect3DSurface9**) override;
    // [CHOSEN, not recovered] Metal exposes no front buffer to read; no BF2 caller (E4).
    HRESULT STDMETHODCALLTYPE GetFrontBufferData(IDirect3DSurface9*) override {
        bf2gfx::unsupported("IDirect3DSwapChain9::GetFrontBufferData (no front buffer on Metal)"); return D3DERR_INVALIDCALL;
    }
};

class Direct3D final : public IDirect3D9 {
public:
    ULONG refs = 1;
    bf2metal::Adapter* native;
    std::vector<D3DDISPLAYMODE> modes;
    explicit Direct3D(bf2metal::Adapter* a) : native(a) {
        // Main-display modes are metadata only: neither enumeration nor CreateDevice switches them.
        for (const auto& m : hostModes())
            if (m.width > 0 && m.height > 0 && m.width <= 8192 && m.height <= 8192)
                modes.push_back({UINT(m.width),UINT(m.height),UINT(m.refreshHz),D3DFMT_X8R8G8B8});
    }
    // [TEMP] BF2_TEST_DISPLAY_MODES="WxH@Hz,..." replaces the host's mode list so a display without
    // the requested mode (the M2's built-in panel has no 800x600) can be reproduced on another Mac.
    static std::vector<dice::hfe::macos::HostDisplayModeInfo> hostModes() {
        const char* test = std::getenv("BF2_TEST_DISPLAY_MODES");
        if (!test || !*test) return dice::hfe::macos::CocoaHost::instance().displayModes();
        std::vector<dice::hfe::macos::HostDisplayModeInfo> out;
        for (const char* p = test; *p;) {
            int w = 0, h = 0, hz = 0, used = 0;
            if (std::sscanf(p, "%dx%d@%d%n", &w, &h, &hz, &used) != 3 || used <= 0) break;
            out.push_back({w, h, hz, 32});
            p += used;
            if (*p == ',') ++p;
        }
        return out;
    }
    ~Direct3D() { bf2metal::destroyAdapter(native); }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** out) override {
        if(!out) return D3DERR_INVALIDCALL; *out=nullptr;
        static const GUID iidDirect3D={0x81bdcbca,0x64d4,0x426d,{0xae,0x8d,0xad,0x01,0x47,0xf4,0x27,0x5c}};
        if(!sameIID(id,iidUnknown) && !sameIID(id,iidDirect3D)) return E_NOINTERFACE;
        *out=static_cast<IDirect3D9*>(this); AddRef(); return D3D_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { mainThread("IDirect3D9::AddRef"); return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override {
        mainThread("IDirect3D9::Release"); const ULONG n = --refs; if (!n) delete this; return n;
    }
    UINT STDMETHODCALLTYPE GetAdapterCount() override { mainThread("IDirect3D9::GetAdapterCount"); return 1; }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT a, DWORD flags, D3DADAPTER_IDENTIFIER9* out) override {
        mainThread("IDirect3D9::GetAdapterIdentifier");
        if (a || !out || (flags & ~2u)) return D3DERR_INVALIDCALL;
        *out = {};
        std::snprintf(out->Driver, sizeof(out->Driver), "OpenBF2 Metal G3");
        std::snprintf(out->Description, sizeof(out->Description), "%s", bf2metal::adapterName(native));
        std::snprintf(out->DeviceName, sizeof(out->DeviceName), "Main display");
        // [CHOSEN, not recovered] Owner decision 2026-11-12: the real GPU vendor and device IDs
        // (MetalBridge adapterPciIds): Intel 0x8086, AMD 0x1002 with their PCI device IDs, Apple
        // silicon 0x106B/0. RendDX9 branches on them (classifyGpu, the ATI/NVIDIA shader defines,
        // the GPU whitelist). No WHQL status or subsystem ID is fabricated.
        unsigned vendor = 0, device = 0;
        bf2metal::adapterPciIds(native, vendor, device);
        // [TEMP] Test hook (not a user setting): BF2_TEST_ADAPTER_IDENTITY="VVVV:DDDD" (hex)
        // replaces the reported IDs, so the AMD code paths run on any Mac (as BF2_TEST_DISPLAY_MODES
        // does for the mode list).
        if (const char* test = std::getenv("BF2_TEST_ADAPTER_IDENTITY")) {
            unsigned v = 0, d = 0;
            if (std::sscanf(test, "%x:%x", &v, &d) == 2) { vendor = v & 0xffff; device = d & 0xffff; }
        }
        out->VendorId = vendor; out->DeviceId = device;
        return D3D_OK;
    }
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT a, D3DFORMAT format) override {
        mainThread("IDirect3D9::GetAdapterModeCount");
        return !a && format == D3DFMT_X8R8G8B8 ? UINT(modes.size()) : 0;
    }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT a,D3DFORMAT format,UINT index,D3DDISPLAYMODE* out) override {
        mainThread("IDirect3D9::EnumAdapterModes");
        if (!out || index >= GetAdapterModeCount(a,format)) return D3DERR_INVALIDCALL;
        *out = modes[index]; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT a,D3DDISPLAYMODE* out) override {
        mainThread("IDirect3D9::GetAdapterDisplayMode");
        if (a || !out) return D3DERR_INVALIDCALL;
        const auto m = dice::hfe::macos::CocoaHost::instance().desktopMode();
        if (m.width <= 0 || m.height <= 0) return D3DERR_NOTAVAILABLE;
        *out = {UINT(m.width),UINT(m.height),UINT(m.refreshHz),D3DFMT_X8R8G8B8}; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT a,D3DDEVTYPE type,D3DFORMAT display,D3DFORMAT back,BOOL) override {
        mainThread("IDirect3D9::CheckDeviceType");
        return adapterType(a,type) && colorFormat(display) && colorFormat(back) ? D3D_OK : D3DERR_NOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT a,D3DDEVTYPE type,D3DFORMAT display,DWORD usage,
                                                D3DRESOURCETYPE resource,D3DFORMAT format) override {
        mainThread("IDirect3D9::CheckDeviceFormat");
        if (!adapterType(a,type) || !colorFormat(display)) return D3DERR_NOTAVAILABLE;
        if(!bf2metal::formatSupported(format) || (usage&~(D3DUSAGE_DYNAMIC|3u))) return D3DERR_NOTAVAILABLE;
        const bool depth=format==D3DFMT_D24S8 || format==D3DFMT_D16;
        if(bool(usage&D3DUSAGE_DEPTHSTENCIL)!=depth || ((usage&3)==3)) return D3DERR_NOTAVAILABLE;
        if(resource!=D3DRTYPE_SURFACE && resource!=D3DRTYPE_TEXTURE && resource!=D3DRTYPE_CUBETEXTURE && resource!=D3DRTYPE_VOLUMETEXTURE)
            return D3DERR_NOTAVAILABLE;
        if(resource==D3DRTYPE_VOLUMETEXTURE && ((usage&3) || bf2metal::formatBlock(format)>1)) return D3DERR_NOTAVAILABLE;
        if((usage&3) && !bf2metal::formatRenderable(format)) return D3DERR_NOTAVAILABLE;
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT a,D3DDEVTYPE type,D3DFORMAT format,BOOL,
                                                         D3DMULTISAMPLE_TYPE samples,DWORD* quality) override {
        mainThread("IDirect3D9::CheckDeviceMultiSampleType");
        if (quality) *quality = 0;
        // [CHOSEN, not recovered] Plain sample counts the GPU can render (Metal
        // supportsTextureSampleCount), one quality level each; NONMASKABLE is not offered
        // (retail's AA list skips it anyway).
        if (!adapterType(a,type) || !bf2metal::formatRenderable(format) ||
            (samples != D3DMULTISAMPLE_NONE && !bf2metal::supportsSamples(native,unsigned(samples))))
            return D3DERR_NOTAVAILABLE;
        if (quality) *quality = 1;
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT a,D3DDEVTYPE t,D3DFORMAT display,D3DFORMAT back,D3DFORMAT depth) override {
        mainThread("IDirect3D9::CheckDepthStencilMatch");
        return adapterType(a,t) && colorFormat(display) && bf2metal::formatRenderable(back) &&
            back!=D3DFMT_D24S8 && back!=D3DFMT_D16 && (depth==D3DFMT_D24S8 || depth==D3DFMT_D16)
            ? D3D_OK : D3DERR_NOTAVAILABLE;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT a,D3DDEVTYPE type,D3DCAPS9* out) override {
        mainThread("IDirect3D9::GetDeviceCaps");
        if (!out || !adapterType(a,type)) return D3DERR_INVALIDCALL;
        caps(*out); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT,D3DDEVTYPE,HWND,DWORD,D3DPRESENT_PARAMETERS*,IDirect3DDevice9**) override;
    // [CHOSEN, not recovered] No software rasterizer can be plugged in (never called, E2).
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void*) override {
        bf2gfx::unsupported("IDirect3D9::RegisterSoftwareDevice"); return D3DERR_NOTAVAILABLE;
    }
    // The conversions StretchRect performs (DeviceResources.inc copySurface): same format, and
    // X8R8G8B8 <-> A8R8G8B8.
    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(UINT a,D3DDEVTYPE t,D3DFORMAT source,D3DFORMAT target) override {
        mainThread("IDirect3D9::CheckDeviceFormatConversion");
        if(!adapterType(a,t)) return D3DERR_INVALIDCALL;
        if(!bf2metal::formatSupported(source) || !bf2metal::formatRenderable(target)) return D3DERR_NOTAVAILABLE;
        return source==target || (colorFormat(source) && colorFormat(target)) ? D3D_OK : D3DERR_NOTAVAILABLE;
    }
    // [CHOSEN, not recovered] There is no Win32 monitor handle; adapter 0 answers a fixed
    // non-null token (never called, E2), any other adapter null as D3D9 does.
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT a) override {
        static int token; return a ? nullptr : static_cast<HMONITOR>(&token);
    }
};

class Device final : public IDirect3DDevice9 {
public:
    ULONG refs = 1;
    Direct3D* parent;
    bf2metal::Frame* native;
    D3DPRESENT_PARAMETERS pp = {};
    D3DDEVICE_CREATION_PARAMETERS creation = {};
    D3DVIEWPORT9 viewport = {};
    Surface back{this,native,D3DUSAGE_RENDERTARGET,true}, depth{this,native,D3DUSAGE_DEPTHSTENCIL,true};
    SwapChain chain{this};
    bool scene = false, depthBound = false;
    // [CHOSEN, not recovered] The D3D9 exclusive-mode focus-loss contract. On Windows the D3D9
    // runtime loses a full-screen device (Windowed == FALSE) when its application is deactivated
    // (Alt-Tab, WM_ACTIVATEAPP FALSE): Present answers D3DERR_DEVICELOST, TestCooperativeLevel
    // answers D3DERR_DEVICELOST while the application stays inactive and D3DERR_DEVICENOTRESET once
    // it is active again, and only a successful Reset ends the loss. Retail RendDX9 depends on that
    // reset: QuadDrawer (the HUD and menu 2D batcher) suspends itself on CEActivateApp(false)
    // (PE 0x1008B0B0) and only its onDeviceReset listener (PE 0x1008AF60) resumes it. A Metal layer
    // is never lost by deactivation, so without this latch the HUD stays suspended after a
    // full-screen tab-out. focusLosses is the host's deactivation count the default targets were
    // built under; focusLost latches once the count moves while the device is full-screen.
    unsigned int focusLosses = 0;
    bool focusLost = false;
    StateCache state;
    Binding<IDirect3DSurface9> colorTarget, depthTarget;
    Device(Direct3D* p, bf2metal::Frame* f, const D3DPRESENT_PARAMETERS& params,
           HWND focus, DWORD flags) : parent(p), native(f), pp(params) {
        parent->AddRef();
        creation = {0,D3DDEVTYPE_HAL,focus,flags};
        focusLosses = dice::hfe::macos::CocoaHost::instance().deactivationCount();
        describe();
    }
    // True while the device is lost to an application deactivation (see focusLosses).
    bool lostToFocus() {
        if (!focusLost && !pp.Windowed &&
            dice::hfe::macos::CocoaHost::instance().deactivationCount() != focusLosses)
            focusLost = true;
        return focusLost;
    }
    static bool applicationActive() { return dice::hfe::macos::CocoaHost::instance().isAppActive(); }
    ~Device() { releaseFvfDeclarations(); bf2metal::destroyFrame(native); parent->Release(); }
    void describe() {
        bf2metal::dimensions(native, pp.BackBufferWidth, pp.BackBufferHeight);
        back.desc = {pp.BackBufferFormat,D3DRTYPE_SURFACE,D3DUSAGE_RENDERTARGET,D3DPOOL_DEFAULT,
                     pp.MultiSampleType,0,pp.BackBufferWidth,pp.BackBufferHeight};
        depth.desc = {D3DFMT_D24S8,D3DRTYPE_SURFACE,D3DUSAGE_DEPTHSTENCIL,D3DPOOL_DEFAULT,
                      pp.MultiSampleType,0,pp.BackBufferWidth,pp.BackBufferHeight};
        viewport = {0,0,pp.BackBufferWidth,pp.BackBufferHeight,0,1};
        depthBound = pp.EnableAutoDepthStencil != 0;
        back.lockable=false; depth.lockable=false;
        auto bindDefault = [&](Surface& s,bool z) {
            auto image=std::make_shared<Image>(); image->native=bf2metal::defaultTarget(native,z); image->borrowed=true;
            s.plane=std::make_shared<Plane>(Plane{native,image,pp.BackBufferWidth,pp.BackBufferHeight,1,0,0,
                s.desc.Format,s.desc.Usage,D3DPOOL_DEFAULT,false,0,{}});
        };
        bindDefault(back,false); bindDefault(depth,true);
        bf2metal::defaultColorFormat(native,pp.BackBufferFormat);
        colorTarget.set(&back); depthTarget.set(depthBound?&depth:nullptr);
        state.reset(pp.EnableAutoDepthStencil!=0,pp.BackBufferWidth,pp.BackBufferHeight);
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        mainThread("IDirect3DDevice9::AddRef"); bf2metal::reftrace::deviceRef(1); return ++refs;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        mainThread("IDirect3DDevice9::Release"); bf2metal::reftrace::deviceRef(-1);
        const ULONG n = --refs; if (!n) delete this; return n;
    }
    HRESULT STDMETHODCALLTYPE GetDirect3D(IDirect3D9** out) override {
        mainThread("IDirect3DDevice9::GetDirect3D");
        if (!out) return D3DERR_INVALIDCALL; *out = parent; parent->AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(D3DCAPS9* out) override { return parent->GetDeviceCaps(0,D3DDEVTYPE_HAL,out); }
    HRESULT STDMETHODCALLTYPE GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* out) override {
        mainThread("IDirect3DDevice9::GetCreationParameters");
        if (!out) return D3DERR_INVALIDCALL; *out = creation; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDisplayMode(UINT chainIndex,D3DDISPLAYMODE* out) override {
        if (chainIndex) return D3DERR_INVALIDCALL; return parent->GetAdapterDisplayMode(0,out);
    }
    UINT STDMETHODCALLTYPE GetNumberOfSwapChains() override { mainThread("IDirect3DDevice9::GetNumberOfSwapChains"); return 1; }
    HRESULT STDMETHODCALLTYPE GetSwapChain(UINT index, IDirect3DSwapChain9** out) override {
        mainThread("IDirect3DDevice9::GetSwapChain");
        if (!out) return D3DERR_INVALIDCALL; *out = nullptr;
        if (index) return D3DERR_INVALIDCALL; *out = &chain; chain.AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetBackBuffer(UINT chainIndex,UINT index,D3DBACKBUFFER_TYPE type,IDirect3DSurface9** out) override {
        mainThread("IDirect3DDevice9::GetBackBuffer");
        if (!out) return D3DERR_INVALIDCALL; *out = nullptr;
        if (chainIndex || index || type != D3DBACKBUFFER_TYPE_MONO) return D3DERR_INVALIDCALL;
        *out = &back; back.AddRef(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetRenderTarget(DWORD index,IDirect3DSurface9** out) override {
        if(index) return D3DERR_INVALIDCALL; return colorTarget.get(out);
    }
    HRESULT STDMETHODCALLTYPE GetDepthStencilSurface(IDirect3DSurface9** out) override {
        mainThread("IDirect3DDevice9::GetDepthStencilSurface");
        if (!out) return D3DERR_INVALIDCALL; *out = nullptr;
        if (!depthTarget.ptr) return static_cast<HRESULT>(0x88760866u); // D3DERR_NOTFOUND
        return depthTarget.get(out);
    }
    HRESULT STDMETHODCALLTYPE SetRenderTarget(DWORD index,IDirect3DSurface9* target) override {
        mainThread("IDirect3DDevice9::SetRenderTarget");
        auto* s=dynamic_cast<Surface*>(target);
        if(index || !s || s->owner!=this || !(s->desc.Usage&D3DUSAGE_RENDERTARGET) || s->plane->locked) return D3DERR_INVALIDCALL;
        colorTarget.set(s);
        state.scissor={0,0,LONG(s->desc.Width),LONG(s->desc.Height)};
        viewport = {0,0,s->desc.Width,s->desc.Height,0,1}; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetDepthStencilSurface(IDirect3DSurface9* target) override {
        mainThread("IDirect3DDevice9::SetDepthStencilSurface");
        auto* s=dynamic_cast<Surface*>(target);
        if(target && (!s || s->owner!=this || !(s->desc.Usage&D3DUSAGE_DEPTHSTENCIL) ||
            (s==&depth && !pp.EnableAutoDepthStencil))) return D3DERR_INVALIDCALL;
        depthTarget.set(target); depthBound = target != nullptr; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetViewport(const D3DVIEWPORT9* v) override {
        mainThread("IDirect3DDevice9::SetViewport");
        // Windows accepts viewports past the target edges and MinZ > MaxZ (Wine visual
        // test_viewport); refusing would leave the previous viewport silently in effect.
        // Rasterization still clips to the target: a draw must clamp what it gives Metal.
        if (!v || !v->Width || !v->Height ||
            !std::isfinite(v->MinZ) || !std::isfinite(v->MaxZ) || v->MinZ < 0 || v->MaxZ > 1)
            return D3DERR_INVALIDCALL;
        viewport = *v; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetViewport(D3DVIEWPORT9* v) override {
        mainThread("IDirect3DDevice9::GetViewport");
        if (!v) return D3DERR_INVALIDCALL; *v = viewport; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE BeginScene() override {
        mainThread("IDirect3DDevice9::BeginScene");
        if (scene) return D3DERR_INVALIDCALL; scene = true; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE EndScene() override {
        mainThread("IDirect3DDevice9::EndScene");
        if (!scene) return D3DERR_INVALIDCALL; scene = false; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Clear(DWORD count,const D3DRECT* rects,DWORD flags,D3DCOLOR color,float z,DWORD stencil) override {
        mainThread("IDirect3DDevice9::Clear");
        if (!flags || (flags & ~7u) || ((flags & 6u) && !depthBound) ||
            ((flags & D3DCLEAR_ZBUFFER) && (!std::isfinite(z) || z < 0 || z > 1)) ||
            (count && !rects)) return D3DERR_INVALIDCALL;
        auto* c=static_cast<Surface*>(colorTarget.ptr); auto* d=static_cast<Surface*>(depthTarget.ptr);
        if(c->plane->locked || (d && d->plane->locked)) return D3DERR_INVALIDCALL;
        if ((flags&6) && (d->desc.Width<c->desc.Width || d->desc.Height<c->desc.Height)) return D3DERR_INVALIDCALL;
        if ((flags&4) && d->desc.Format!=D3DFMT_D24S8) return D3DERR_INVALIDCALL;
        const auto regions=bf2metal::clearRectangles(viewport,c->desc.Width,c->desc.Height,count,rects,
            state.raw.render[174]?&state.scissor:nullptr);
        if(regions.empty()) return D3D_OK;
        // [CHOSEN, not recovered] D3DRS_SRGBWRITEENABLE: no retail call and no shipped effect
        // pass sets it. The clear colour is written as given.
        if((flags&1) && state.raw.render[194]) bf2gfx::unsupported("IDirect3DDevice9::Clear with D3DRS_SRGBWRITEENABLE (colour written without sRGB conversion)");
        // XRGB targets always store opaque alpha.
        if (c->desc.Format == D3DFMT_X8R8G8B8 || c->desc.Format == D3DFMT_X8B8G8R8) color |= 0xff000000u;
        return bf2metal::clearTargets(native,c->plane->image->native,c->plane->level,c->plane->slice,
            d?d->plane->image->native:nullptr,d?d->plane->level:0,d?d->plane->slice:0,flags,color,z,stencil,
            regions.data(),regions.size())
            ? D3D_OK : D3DERR_DEVICELOST;
    }
    HRESULT STDMETHODCALLTYPE Present(const RECT* source,const RECT* dest,HWND window,const RGNDATA* dirty) override {
        return present(source,dest,window,dirty,0);
    }
    HRESULT present(const RECT* source,const RECT* dest,HWND window,const RGNDATA* dirty,DWORD flags) {
        mainThread("IDirect3DDevice9::Present");
        if (scene) return D3DERR_INVALIDCALL;
        flushDrawTrace();
        bf2metal::createFinishedEffectShaders();
        const auto hr=bf2metal::presentParameters(pp.SwapEffect,source,dest,dirty,flags);
        if(FAILED(hr)) return hr;
        // Present never answers DEVICENOTRESET in D3D9 (TestCooperativeLevel does), and the
        // renderer's Present loop retries unknown codes forever. A resized layer is a lost device;
        // the caller's TestCooperativeLevel then reports DEVICENOTRESET and drives the Reset.
        if (bf2metal::resized(native) || lostToFocus()) return D3DERR_DEVICELOST;
        switch(bf2metal::present(native,window,(flags&1)!=0)) {
        case bf2metal::PresentResult::ok: return D3D_OK;
        case bf2metal::PresentResult::busy: return static_cast<HRESULT>(0x8876021cu);
        case bf2metal::PresentResult::invalidWindow: return D3DERR_INVALIDCALL;
        default: return D3DERR_DEVICELOST;
        }
    }
    HRESULT STDMETHODCALLTYPE TestCooperativeLevel() override {
        // [CHOSEN, not recovered] GPU errors use the same recovery loop as resize.
        if (!bf2metal::renderable(native)) return D3DERR_DEVICELOST;
        if (lostToFocus()) return applicationActive() ? D3DERR_DEVICENOTRESET : D3DERR_DEVICELOST;
        return !bf2metal::healthy(native) || bf2metal::resized(native) ? D3DERR_DEVICENOTRESET : D3D_OK;
    }
    static HRESULT validate(const D3DPRESENT_PARAMETERS& p) {
        if (!colorFormat(p.BackBufferFormat) || p.BackBufferCount > 1 || p.MultiSampleType == D3DMULTISAMPLE_NONMASKABLE ||
            unsigned(p.MultiSampleType) > 16u || p.MultiSampleQuality || p.SwapEffect != D3DSWAPEFFECT_DISCARD || (p.Flags & ~2u) ||
            (p.EnableAutoDepthStencil && p.AutoDepthStencilFormat != D3DFMT_D24S8) ||
            (p.PresentationInterval != 0 && p.PresentationInterval != D3DPRESENT_INTERVAL_ONE &&
             p.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE)) return D3DERR_NOTAVAILABLE;
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE Reset(D3DPRESENT_PARAMETERS* params) override {
        mainThread("IDirect3DDevice9::Reset");
        if (!params || scene || activeQuery || back.refs || depth.refs ||
            (params->hDeviceWindow && params->hDeviceWindow != pp.hDeviceWindow)) return D3DERR_INVALIDCALL;
        // [CHOSEN, not recovered] [RETIRE-WHEN: macos-reset-default-pool-resources]
        // Reset permits live explicit D3DPOOL_DEFAULT resources until the renderer's
        // rebuild listeners release/recreate them. Enforce Windows INVALIDCALL only
        // after those listeners are wired and validated through loss/resize recovery.
        D3DPRESENT_PARAMETERS p = *params;
        if (p.BackBufferFormat == D3DFMT_UNKNOWN && p.Windowed) p.BackBufferFormat = D3DFMT_X8R8G8B8;
        const HRESULT hr = validate(p); if (FAILED(hr)) return hr;
        // A device lost to deactivation cannot be reset until the application is active again.
        if (lostToFocus() && !applicationActive()) return D3DERR_DEVICELOST;
        if (!bf2metal::reset(native,p.BackBufferWidth,p.BackBufferHeight,
                p.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE,p.EnableAutoDepthStencil != 0,
                p.MultiSampleType > 1 ? unsigned(p.MultiSampleType) : 1u))
            return D3DERR_DEVICELOST;
        const HWND window = pp.hDeviceWindow;
        pp = p; pp.hDeviceWindow = window; pp.BackBufferCount = 1;
        focusLost = false;
        focusLosses = dice::hfe::macos::CocoaHost::instance().deactivationCount();
        describe();
        // Windows writes the actual size, count and format back, as CreateDevice does
        // (Wine's test_reset records this); the caller's window field is left as passed.
        const HWND callerWindow = params->hDeviceWindow;
        *params = pp; params->hDeviceWindow = callerWindow;
        return D3D_OK;
    }
#include "DeviceState.inc"
#include "DeviceResources.inc"
#include "DeviceFallbacks.inc"
#include "FixedFunctionDraw.inc"
#include "DeviceDraw.inc"
};

} // namespace
// [TEMP] [RETIRE-WHEN: rend-deinit-reftrace] RefTrace.h.
void bf2metalRefTraceReport(void* device) {
    if (!bf2metal::reftrace::enabled() || !device) return;
    auto* d = static_cast<Device*>(static_cast<IDirect3DDevice9*>(device));
    bf2metal::reftrace::report(d, d->refs);
}
namespace {
ULONG SwapChain::AddRef() { mainThread("IDirect3DSwapChain9::AddRef"); device->AddRef(); return ++refs; }
ULONG SwapChain::Release() {
    mainThread("IDirect3DSwapChain9::Release");
    const ULONG n = --refs; device->Release(); return n;
}
HRESULT SwapChain::Present(const RECT* source,const RECT* dest,HWND window,const RGNDATA* dirty,DWORD flags) {
    return device->present(source,dest,window,dirty,flags);
}
HRESULT SwapChain::GetBackBuffer(UINT index,D3DBACKBUFFER_TYPE type,IDirect3DSurface9** out) {
    return device->GetBackBuffer(0,index,type,out);
}
HRESULT Direct3D::CreateDevice(UINT a,D3DDEVTYPE type,HWND focus,DWORD flags,
                             D3DPRESENT_PARAMETERS* params,IDirect3DDevice9** out) {
    mainThread("IDirect3D9::CreateDevice");
    if (!out) return D3DERR_INVALIDCALL; *out = nullptr;
    if (!params) return D3DERR_INVALIDCALL;
    // Honor the engine's existing retry without PUREDEVICE. Native rendering stays on main.
    if (!adapterType(a,type) || (flags & ~0x42u) || !(flags & 0x40u)) return D3DERR_NOTAVAILABLE;
    D3DPRESENT_PARAMETERS p = *params;
    if (p.BackBufferFormat == D3DFMT_UNKNOWN && p.Windowed) p.BackBufferFormat = D3DFMT_X8R8G8B8;
    const HRESULT hr = Device::validate(p); if (FAILED(hr)) return hr;
    if (!p.hDeviceWindow) p.hDeviceWindow = focus;
    if (!p.hDeviceWindow) return D3DERR_INVALIDCALL;
    bf2metal::Frame* frame = bf2metal::createFrame(native,p.hDeviceWindow,p.BackBufferWidth,p.BackBufferHeight,
        p.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE,p.EnableAutoDepthStencil != 0,
        p.MultiSampleType > 1 ? unsigned(p.MultiSampleType) : 1u);
    if (!frame) return D3DERR_NOTAVAILABLE;
    p.BackBufferCount = 1;
    Device* device = new (std::nothrow) Device(this,frame,p,focus,flags);
    if (!device) { bf2metal::destroyFrame(frame); return E_OUTOFMEMORY; }
    *out = device; *params = device->pp; return D3D_OK;
}
}
namespace bf2d3dx {
static HRESULT planeBytes(const std::shared_ptr<Plane>& plane,Bytes& bytes,bool upload) {
    bf2metal::mainThread("bf2d3dx::resourceBytes");
    if(!plane || plane->locked || !pixelBytes(plane->format)) return D3DERR_INVALIDCALL;
    if(upload) {
        if(bytes.size()!=std::size_t(plane->slicePitch())*plane->d) return D3DERR_INVALIDCALL;
        plane->bytes=bytes; return plane->write()?D3D_OK:D3DERR_DEVICELOST;
    }
    if(!plane->read()) return D3DERR_DEVICELOST; bytes=plane->bytes; return D3D_OK;
}
HRESULT surfaceBytes(IDirect3DSurface9* p,Bytes& bytes,bool upload) {
    auto* s=dynamic_cast<Surface*>(p); return s?planeBytes(s->plane,bytes,upload):D3DERR_INVALIDCALL;
}
HRESULT textureBytes(IDirect3DBaseTexture9* p,unsigned face,unsigned level,Bytes& bytes,bool upload) {
    std::shared_ptr<Plane> plane;
    if(auto* t=dynamic_cast<Texture2D*>(p)) { if(face || level>=t->levels) return D3DERR_INVALIDCALL; plane=t->planes[level]; }
    else if(auto* t=dynamic_cast<CubeTexture*>(p)) { if(face>=6 || level>=t->levels) return D3DERR_INVALIDCALL; plane=t->planes[face*t->levels+level]; }
    else if(auto* t=dynamic_cast<VolumeTexture*>(p)) { if(face || level>=t->levels) return D3DERR_INVALIDCALL; plane=t->planes[level]; }
    return planeBytes(plane,bytes,upload);
}
}
namespace bf2metal {
#if defined(BF2_METAL_DEVICE_TESTING)
void injectCommandBufferFailure(void* p) {
    auto* device=static_cast<Device*>(static_cast<IDirect3DDevice9*>(p));
    armCommandBufferFailure(device->native);
}
#endif
void defaultState(RawState& raw,bool depth,unsigned width,unsigned height) {
    StateCache cache; cache.reset(depth,width,height); raw=cache.raw;
}
bool inspectState(void* p,RawState& raw,StateMapping& mapping,const char** refusal) {
    if(!p || !refusal) return false;
    auto* device=static_cast<Device*>(static_cast<IDirect3DDevice9*>(p));
    raw=device->state.raw; *refusal=device->state.map(mapping); return !*refusal;
}
bool inspectDeclaration(void* p,unsigned i,VertexElement& element,unsigned& format) {
    if(!p) return false;
    auto* d=static_cast<Declaration*>(static_cast<IDirect3DVertexDeclaration9*>(p));
    if(i>=d->mapped.size()) return false;
    element=d->mapped[i]; format=layoutFormat(d->native,i); return true;
}
bool inspectSurface(void* p,void* bytes,std::size_t size) {
    if(!p || !bytes) return false;
    auto* s=dynamic_cast<Surface*>(static_cast<IDirect3DSurface9*>(p));
    if(!s || size!=std::size_t(s->plane->slicePitch())*s->plane->d || !s->plane->read()) return false;
    std::memcpy(bytes,s->plane->bytes.data(),size); return true;
}
}
extern "C" IDirect3D9* WINAPI Direct3DCreate9(UINT version)
{
    mainThread("Direct3DCreate9");
    if (version != D3D_SDK_VERSION) return nullptr;
    bf2metal::Adapter* adapter = bf2metal::createAdapter();
    if (!adapter) return nullptr;
    if (!bf2metal::supportsBC(adapter)) {
        bf2metal::destroyAdapter(adapter);
        std::fprintf(stderr, "Direct3DCreate9: Metal BC1/BC2/BC3 texture compression is required\n");
        std::fflush(stderr);
        return nullptr;
    }
    Direct3D* d3d = nullptr;
    try { d3d = new (std::nothrow) Direct3D(adapter); }
    catch (const std::bad_alloc&) { bf2metal::destroyAdapter(adapter); return nullptr; }
    if (!d3d) bf2metal::destroyAdapter(adapter);
    return d3d;
}
#endif
