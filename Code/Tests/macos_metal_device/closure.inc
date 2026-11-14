// [CHOSEN, not recovered] rend-close: D3D9/D3DX semantics that replaced former stops.
// Each check states the D3D9 reference behaviour.
#include "../../BF2/Platform/Gfx/D3DX9Subset.h"
#include <cmath>

static const GUID closureIidUnknown = {0,0,0,{0xc0,0,0,0,0,0,0,0x46}};
static const GUID closureIidOther = {0x12345678,0x1234,0x1234,{1,2,3,4,5,6,7,8}};

static int closureDeviceChecks(IDirect3DDevice9* d)
{
    // QueryInterface: IUnknown and the own IID succeed, any other IID is E_NOINTERFACE.
    void* out = nullptr;
    CHECK(d->QueryInterface(closureIidUnknown,&out) == D3D_OK && out == d); d->Release();
    CHECK(d->QueryInterface(closureIidOther,&out) == E_NOINTERFACE && !out);
    IDirect3D9* d3d = nullptr;
    CHECK(d->GetDirect3D(&d3d) == D3D_OK);
    CHECK(d3d->QueryInterface(closureIidUnknown,&out) == D3D_OK && out == d3d); d3d->Release();
    CHECK(d3d->QueryInterface(closureIidOther,&out) == E_NOINTERFACE);
    CHECK(d3d->CheckDeviceFormatConversion(0,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,D3DFMT_A8R8G8B8) == D3D_OK);
    CHECK(d3d->CheckDeviceFormatConversion(0,D3DDEVTYPE_HAL,D3DFMT_DXT1,D3DFMT_X8R8G8B8) == D3DERR_NOTAVAILABLE);
    CHECK(d3d->GetAdapterMonitor(0) != nullptr && d3d->GetAdapterMonitor(1) == nullptr);
    CHECK(d3d->RegisterSoftwareDevice(nullptr) == D3DERR_NOTAVAILABLE);
    d3d->Release();
    IDirect3DSwapChain9* chain = nullptr;
    CHECK(d->GetSwapChain(0,&chain) == D3D_OK);
    CHECK(chain->QueryInterface(closureIidUnknown,&out) == D3D_OK && out == chain); chain->Release();
    CHECK(chain->QueryInterface(closureIidOther,&out) == E_NOINTERFACE);
    chain->Release();

    // Transforms: identity until set; MultiplyTransform stores m x current; invalid types refused.
    D3DMATRIX m = {}, r = {};
    CHECK(d->GetTransform(D3DTS_WORLD,&r) == D3D_OK && r._11 == 1 && r._22 == 1 && r._33 == 1 && r._44 == 1 && r._12 == 0);
    for (int i = 0; i < 16; ++i) (&m._11)[i] = float(i + 1);
    CHECK(d->SetTransform(D3DTS_VIEW,&m) == D3D_OK && d->GetTransform(D3DTS_VIEW,&r) == D3D_OK && r._43 == 15.0f);
    D3DMATRIX scale = {}; scale._11 = 2; scale._22 = 2; scale._33 = 2; scale._44 = 2;
    CHECK(d->MultiplyTransform(D3DTS_VIEW,&scale) == D3D_OK && d->GetTransform(D3DTS_VIEW,&r) == D3D_OK && r._11 == 2.0f && r._44 == 32.0f);
    CHECK(d->SetTransform(static_cast<D3DTRANSFORMSTATETYPE>(1),&m) == D3DERR_INVALIDCALL);
    D3DMATERIAL9 mat = {}, matOut = {}; mat.Power = 7;
    CHECK(d->SetMaterial(&mat) == D3D_OK && d->GetMaterial(&matOut) == D3D_OK && matOut.Power == 7);
    BOOL enabled = TRUE;
    CHECK(d->GetLightEnable(3,&enabled) == D3DERR_INVALIDCALL);  // never set
    CHECK(d->LightEnable(3,TRUE) == D3D_OK && d->GetLightEnable(3,&enabled) == D3D_OK && enabled);
    unsigned char light[104] = {};
    CHECK(d->GetLight(3,reinterpret_cast<D3DLIGHT9*>(light)) == D3D_OK);
    DWORD type = 0; float diffuse = 0, dirZ = 0; std::memcpy(&type,light,4); std::memcpy(&diffuse,light+4,4); std::memcpy(&dirZ,light+72,4);
    CHECK(type == 3 && diffuse == 1.0f && dirZ == 1.0f); // D3D9 default light: directional, white, +Z
    D3DCLIPSTATUS9 clip = {1,2}, clipOut = {};
    CHECK(d->SetClipStatus(&clip) == D3D_OK && d->GetClipStatus(&clipOut) == D3D_OK && clipOut.ClipIntersection == 2);
    DWORD passes = 0;
    CHECK(d->ValidateDevice(&passes) == D3D_OK && passes == 1);
    PALETTEENTRY pal[256] = {}, palOut[256] = {}; pal[5].peGreen = 9; UINT current = 0;
    CHECK(d->SetCurrentTexturePalette(4) == D3DERR_INVALIDCALL);
    CHECK(d->SetPaletteEntries(4,pal) == D3D_OK && d->GetPaletteEntries(4,palOut) == D3D_OK && palOut[5].peGreen == 9);
    CHECK(d->SetCurrentTexturePalette(4) == D3D_OK && d->GetCurrentTexturePalette(&current) == D3D_OK && current == 4);
    CHECK(d->SetSoftwareVertexProcessing(TRUE) == D3DERR_INVALIDCALL && !d->GetSoftwareVertexProcessing()); // hardware device
    CHECK(d->SetNPatchMode(3.0f) == D3D_OK && d->GetNPatchMode() == 3.0f);
    DWORD fvf = 0;
    CHECK(d->SetFVF(0x42) == D3D_OK && d->GetFVF(&fvf) == D3D_OK && fvf == 0x42 && d->SetFVF(0) == D3D_OK);
    D3DRASTER_STATUS raster = {TRUE,5};
    CHECK(d->GetRasterStatus(0,&raster) == D3D_OK && raster.ScanLine == 0);
    D3DGAMMARAMP ramp = {};
    d->GetGammaRamp(0,&ramp);
    CHECK(ramp.red[0] == 0 && ramp.red[255] == 0xffff && ramp.blue[128] == 128 * 257);
    CHECK(d->ShowCursor(TRUE) == FALSE && d->ShowCursor(FALSE) == TRUE);
    CHECK(d->DrawRectPatch(1,nullptr,nullptr) == D3DERR_INVALIDCALL && d->DeletePatch(1) == D3DERR_INVALIDCALL); // no patch caps
    CHECK(d->EvictManagedResources() == D3D_OK && d->GetAvailableTextureMem() >= (64u << 20));

    // Resource creation rules: shared handles (D3D9Ex only), invalid pools, absent formats, volume
    // render targets, index formats, lockable multisampled targets, quality without samples.
    IDirect3DTexture9* t = nullptr; HANDLE shared = nullptr;
    CHECK(d->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&t,&shared) == D3DERR_INVALIDCALL && !t);
    CHECK(d->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,static_cast<D3DPOOL>(7),&t,nullptr) == D3DERR_INVALIDCALL);
    CHECK(d->CreateTexture(4,4,1,0,D3DFMT_P8,D3DPOOL_MANAGED,&t,nullptr) == D3DERR_INVALIDCALL);
    IDirect3DVolumeTexture9* v = nullptr;
    CHECK(d->CreateVolumeTexture(4,4,4,1,D3DUSAGE_RENDERTARGET,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&v,nullptr) == D3DERR_INVALIDCALL);
    IDirect3DIndexBuffer9* ib = nullptr;
    CHECK(d->CreateIndexBuffer(64,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&ib,nullptr) == D3DERR_INVALIDCALL);
    IDirect3DSurface9* s = nullptr;
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_4_SAMPLES,0,TRUE,&s,nullptr) == D3DERR_INVALIDCALL);
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,1,FALSE,&s,nullptr) == D3DERR_INVALIDCALL);

    // Private data: bytes, MOREDATA with the needed size, NOTFOUND, IUnknown references.
    CHECK(d->CreateTexture(4,4,2,0,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&t,nullptr) == D3D_OK);
    const GUID key = {1,2,3,{4,5,6,7,8,9,10,11}};
    const unsigned payload = 0xabcdef01u; unsigned got = 0; DWORD size = 2;
    CHECK(t->GetPrivateData(key,&got,&size) == D3DERR_NOTFOUND);
    CHECK(t->SetPrivateData(key,&payload,4,0) == D3D_OK);
    CHECK(t->GetPrivateData(key,&got,&size) == D3DERR_MOREDATA && size == 4);
    CHECK(t->GetPrivateData(key,&got,&size) == D3D_OK && got == payload);
    IDirect3DSurface9* level = nullptr;
    CHECK(t->GetSurfaceLevel(0,&level) == D3D_OK);
    level->AddRef(); const ULONG before = level->Release();
    CHECK(t->SetPrivateData(key,&level,sizeof(level),1) == D3D_OK); // D3DSPD_IUNKNOWN references
    level->AddRef(); CHECK(level->Release() == before + 1);
    CHECK(t->FreePrivateData(key) == D3D_OK && t->FreePrivateData(key) == D3DERR_NOTFOUND);
    level->AddRef(); CHECK(level->Release() == before);
    level->Release();
    // Without D3DUSAGE_AUTOGENMIPMAP: the filter cannot be set and reads NONE.
    CHECK(t->SetAutoGenFilterType(D3DTEXF_LINEAR) == D3DERR_INVALIDCALL && t->GetAutoGenFilterType() == D3DTEXF_NONE);
    t->GenerateMipSubLevels();

    // UpdateTexture copies every level of a SYSTEMMEM texture into a DEFAULT one.
    for (UINT l = 0; l < 2; ++l) {
        D3DLOCKED_RECT lock = {};
        CHECK(t->LockRect(l,&lock,nullptr,0) == D3D_OK);
        for (UINT y = 0; y < (l ? 2u : 4u); ++y)
            for (UINT x = 0; x < (l ? 2u : 4u); ++x)
                reinterpret_cast<unsigned*>(static_cast<unsigned char*>(lock.pBits) + y * lock.Pitch)[x] = 0xff000000u | (l << 16) | (y << 8) | x;
        CHECK(t->UnlockRect(l) == D3D_OK);
    }
    IDirect3DTexture9* gpu = nullptr;
    CHECK(d->CreateTexture(4,4,2,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&gpu,nullptr) == D3D_OK);
    CHECK(d->UpdateTexture(t,gpu) == D3D_OK);
    for (UINT l = 0; l < 2; ++l) {
        D3DLOCKED_RECT lock = {};
        CHECK(gpu->LockRect(l,&lock,nullptr,D3DLOCK_READONLY) == D3D_OK);
        const unsigned px = reinterpret_cast<unsigned*>(static_cast<unsigned char*>(lock.pBits) + lock.Pitch)[1];
        CHECK(px == (0xff000000u | (l << 16) | (1u << 8) | 1u));
        CHECK(gpu->UnlockRect(l) == D3D_OK);
    }
    CHECK(gpu->Release() == 0 && t->Release() == 0);

    // ColorFill writes the rectangle of a DEFAULT render target and nothing else.
    IDirect3DSurface9 *target = nullptr, *cpu = nullptr;
    CHECK(d->CreateRenderTarget(8,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr) == D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(8,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr) == D3D_OK);
    const RECT whole = {0,0,8,8}, part = {2,2,4,4};
    CHECK(d->ColorFill(target,&whole,0xff000000u) == D3D_OK && d->ColorFill(target,&part,0x80ff4020u) == D3D_OK);
    CHECK(d->ColorFill(cpu,&part,0) == D3DERR_INVALIDCALL); // D3D9: DEFAULT pool only
    D3DLOCKED_RECT lock = {};
    CHECK(d->GetRenderTargetData(target,cpu) == D3D_OK && cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY) == D3D_OK);
    const unsigned* row2 = reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(lock.pBits) + 2 * lock.Pitch);
    const unsigned* row5 = reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(lock.pBits) + 5 * lock.Pitch);
    CHECK(row2[3] == 0x80ff4020u && row2[4] == 0xff000000u && row5[3] == 0xff000000u);
    CHECK(cpu->UnlockRect() == D3D_OK && cpu->Release() == 0 && target->Release() == 0);
    std::puts("PASS: closure device semantics (QueryInterface, format conversion, fixed-function state storage, resource rules, private data, autogen, UpdateTexture, ColorFill)");
    return 0;
}

static const char kClosureEffect[] =
    "float4 Tint <float4 Range = {1, 2, 3, 4}; float2x2 Basis = {5, 6, 7, 8};> = {0.25, 0.5, 0.75, 1};\n"
    "float4 vs(float4 p : POSITION) : POSITION { return p; }\n"
    "float4 ps() : COLOR { return Tint; }\n"
    "technique T { pass P { CullMode = CW; VertexShader = compile vs_2_0 vs(); PixelShader = compile ps_2_0 ps(); } }\n";

static int closureEffectChecks(IDirect3DDevice9* d)
{
    ID3DXEffect* fx = nullptr; ID3DXBuffer* errors = nullptr;
    const HRESULT hr = D3DXCreateEffect(d,kClosureEffect,UINT(sizeof(kClosureEffect) - 1),nullptr,nullptr,0,nullptr,&fx,&errors);
    if (FAILED(hr) && errors) std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));
    CHECK(SUCCEEDED(hr) && fx);
    D3DXEFFECT_DESC desc = {};
    CHECK(fx->GetDesc(&desc) == D3D_OK && desc.Parameters == 1 && desc.Techniques == 1 && desc.Functions == 0);
    ID3DXEffectPool* pool = reinterpret_cast<ID3DXEffectPool*>(1);
    CHECK(fx->GetPool(&pool) == D3D_OK && pool == nullptr); // created without a pool
    void* out = nullptr;
    CHECK(fx->QueryInterface(closureIidUnknown,&out) == D3D_OK && out == fx); fx->Release();
    CHECK(fx->QueryInterface(closureIidOther,&out) == E_NOINTERFACE && !out);
    const D3DXHANDLE tint = fx->GetParameterByName(nullptr,"Tint");
    const D3DXHANDLE range = fx->GetAnnotationByName(tint,"Range");
    const D3DXHANDLE basis = fx->GetAnnotationByName(tint,"Basis");
    CHECK(tint && range && basis);
    D3DXVECTOR4 vec[1] = {};
    CHECK(fx->GetVectorArray(range,vec,1) == D3D_OK && vec[0].x == 1 && vec[0].w == 4);
    D3DXMATRIX mat = {};
    CHECK(fx->GetMatrix(basis,&mat) == D3D_OK && mat._11 == 5 && mat._12 == 6 && mat._21 == 7 && mat._22 == 8 && mat._33 == 0);
    CHECK(fx->GetMatrixTranspose(basis,&mat) == D3D_OK && mat._12 == 7 && mat._21 == 6);
    const D3DXVECTOR4 one = {1,1,1,1};
    CHECK(fx->SetVector(range,&one) == D3DERR_INVALIDCALL); // annotations are read-only
    CHECK(fx->SetString(tint,"x") == D3DERR_INVALIDCALL && fx->GetFunction(0) == nullptr);
    // Begin without DONOTSAVESTATE saves what the technique sets and End restores it, also with
    // D3DXFX_DONOTSAVESHADERSTATE (shaders and constants are then not restored).
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(22),3) == D3D_OK); // CULLMODE = CCW
    UINT passes = 0; DWORD cull = 0;
    CHECK(fx->Begin(&passes,2) == D3D_OK && passes == 1 && fx->BeginPass(0) == D3D_OK);
    CHECK(d->GetRenderState(static_cast<D3DRENDERSTATETYPE>(22),&cull) == D3D_OK && cull == 2);
    IDirect3DVertexShader9* vs = nullptr;
    CHECK(d->GetVertexShader(&vs) == D3D_OK && vs);
    CHECK(fx->EndPass() == D3D_OK && fx->End() == D3D_OK);
    CHECK(d->GetRenderState(static_cast<D3DRENDERSTATETYPE>(22),&cull) == D3D_OK && cull == 3);
    IDirect3DVertexShader9* after = nullptr;
    CHECK(d->GetVertexShader(&after) == D3D_OK && after == vs); // shader state not restored
    vs->Release(); after->Release();
    CHECK(d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    CHECK(fx->Release() == 0);
    std::puts("PASS: closure effect semantics (GetDesc, GetPool without a pool, QueryInterface, annotation vector/matrix reads, read-only annotations, partial state saving)");
    return 0;
}

// D3DSAMP_MIPMAPLODBIAS (xpack RaShaderWaterBase.fx sets -10). Reference: the bias adds to the
// computed LOD, clamped to the mip chain. A 4-level texture (red, green, blue, white per level)
// drawn 1:1 (LOD 0) and at quarter size (LOD 2), through a pixel shader and through the
// fixed-function stage, point-filtered.
static const char kClosureBiasEffect[] =
    "texture Tex; sampler S = sampler_state { Texture = (Tex); MinFilter = POINT; MagFilter = POINT; MipFilter = POINT; AddressU = CLAMP; AddressV = CLAMP; };\n"
    "struct V { float4 p : POSITION; float2 t : TEXCOORD0; };\n"
    "V vs(V v) { return v; }\n"
    "float4 ps(float2 t : TEXCOORD0) : COLOR { return tex2D(S, t); }\n"
    "technique Shader { pass P { CullMode = NONE; ZEnable = FALSE; VertexShader = compile vs_2_0 vs(); PixelShader = compile ps_2_0 ps(); } }\n"
    "technique Fixed { pass P { CullMode = NONE; ZEnable = FALSE; VertexShader = compile vs_2_0 vs(); PixelShader = NULL; Sampler[0] = (S);\n"
    "  ColorOp[0] = SELECTARG1; ColorArg1[0] = TEXTURE; AlphaOp[0] = SELECTARG1; AlphaArg1[0] = TEXTURE; ColorOp[1] = DISABLE; AlphaOp[1] = DISABLE; } }\n";

static int closureLodBiasChecks(IDirect3DDevice9* d)
{
    ID3DXEffect* fx = nullptr; ID3DXBuffer* errors = nullptr;
    const HRESULT hr = D3DXCreateEffect(d,kClosureBiasEffect,UINT(sizeof(kClosureBiasEffect) - 1),nullptr,nullptr,0,nullptr,&fx,&errors);
    if (FAILED(hr) && errors) std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));
    CHECK(SUCCEEDED(hr) && fx);
    IDirect3DTexture9* tex = nullptr;
    CHECK(d->CreateTexture(8,8,4,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,nullptr) == D3D_OK);
    const unsigned colors[4] = {0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu};
    for (UINT l = 0; l < 4; ++l) {
        D3DLOCKED_RECT lock = {}; const UINT n = 8u >> l;
        CHECK(tex->LockRect(l,&lock,nullptr,0) == D3D_OK);
        for (UINT y = 0; y < n; ++y) for (UINT x = 0; x < n; ++x)
            reinterpret_cast<unsigned*>(static_cast<unsigned char*>(lock.pBits) + y * lock.Pitch)[x] = colors[l];
        CHECK(tex->UnlockRect(l) == D3D_OK);
    }
    CHECK(fx->SetTexture(fx->GetParameterByName(nullptr,"Tex"),tex) == D3D_OK);
    IDirect3DSurface9 *old = nullptr, *target = nullptr, *cpu = nullptr;
    CHECK(d->GetRenderTarget(0,&old) == D3D_OK);
    CHECK(d->CreateRenderTarget(16,16,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr) == D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(16,16,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr) == D3D_OK);
    const D3DVERTEXELEMENT9 elements[] = {{0,0,D3DDECLTYPE(3),0,0,0},{0,16,D3DDECLTYPE(1),0,5,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* decl = nullptr;
    CHECK(d->CreateVertexDeclaration(elements,&decl) == D3D_OK);
    struct Vtx { float x,y,z,w,u,v; };
    // A quad over viewport pixels [x0,x0+size) of the 16x16 target, uv 0..1.
    auto quad = [&](float x0,float y0,float size) -> bool {
        const D3DVIEWPORT9 vp = {DWORD(x0),DWORD(y0),DWORD(size),DWORD(size),0,1};
        const Vtx q[4] = {{-1,1,0.5f,1,0,0},{1,1,0.5f,1,1,0},{-1,-1,0.5f,1,0,1},{1,-1,0.5f,1,1,1}};
        return d->SetViewport(&vp) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(Vtx)) == D3D_OK;
    };
    auto bias = [&](float b) { DWORD w; std::memcpy(&w,&b,4); return d->SetSamplerState(0,D3DSAMPLERSTATETYPE(8),w) == D3D_OK; };
    struct Case { const char* technique; float bias, x, y, size; unsigned expect; };
    // 8x8 texels on 8x8 pixels: LOD 0; on 2x2 pixels: LOD 2.
    const Case cases[] = {
        {"Shader",0,0,0,8,colors[0]}, {"Shader",1,8,0,8,colors[1]}, {"Shader",0,0,8,2,colors[2]}, {"Shader",-10,4,8,2,colors[0]},
        {"Fixed",0,8,8,2,colors[2]}, {"Fixed",-10,12,8,2,colors[0]}, {"Fixed",2,8,12,4,colors[3]}, // LOD 1 + 2 = 3
    };
    CHECK(d->SetRenderTarget(0,target) == D3D_OK && d->SetVertexDeclaration(decl) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000u,1,0) == D3D_OK);
    for (const auto& c : cases) {
        UINT passes = 0;
        CHECK(fx->SetTechnique(fx->GetTechniqueByName(c.technique)) == D3D_OK && fx->Begin(&passes,7) == D3D_OK && fx->BeginPass(0) == D3D_OK);
        CHECK(bias(c.bias) && quad(c.x,c.y,c.size));
        CHECK(fx->EndPass() == D3D_OK && fx->End() == D3D_OK);
    }
    CHECK(bias(0) && d->EndScene() == D3D_OK);
    D3DLOCKED_RECT lock = {};
    CHECK(d->GetRenderTargetData(target,cpu) == D3D_OK && cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY) == D3D_OK);
    int failures = 0;
    for (const auto& c : cases) {
        const unsigned cx = unsigned(c.x + c.size / 2 - 1), cy = unsigned(c.y + c.size / 2 - 1);
        const unsigned px = reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(lock.pBits) + cy * lock.Pitch)[cx];
        if (px != c.expect) { std::fprintf(stderr,"FAIL lod bias %s bias %g size %g: 0x%08x, expected 0x%08x\n",c.technique,c.bias,c.size,px,c.expect); ++failures; }
    }
    CHECK(cpu->UnlockRect() == D3D_OK);
    CHECK(d->SetRenderTarget(0,old) == D3D_OK && d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    old->Release(); decl->Release(); cpu->Release(); target->Release(); tex->Release(); fx->Release();
    CHECK(failures == 0);
    std::puts("PASS: closure D3DSAMP_MIPMAPLODBIAS (pixel shader and fixed-function stage, positive, negative, clamped)");
    return 0;
}

// Owner decision 2026-11-12: the adapter reports the real GPU vendor (Intel 0x8086, AMD 0x1002,
// Apple silicon 0x106B) and device; BF2_TEST_ADAPTER_IDENTITY (test hook) replaces them.
static int closureAdapterChecks(IDirect3D9* d3d)
{
    D3DADAPTER_IDENTIFIER9 id = {};
    unsetenv("BF2_TEST_ADAPTER_IDENTITY");
    CHECK(d3d->GetAdapterIdentifier(0,0,&id) == D3D_OK);
    std::printf("adapter: VendorId 0x%04x DeviceId 0x%04x \"%s\"\n",unsigned(id.VendorId),unsigned(id.DeviceId),id.Description);
    CHECK(id.VendorId == 0x8086 || id.VendorId == 0x1002 || id.VendorId == 0x106B);
    CHECK(id.VendorId == 0x106B ? id.DeviceId == 0 : id.DeviceId != 0);
    setenv("BF2_TEST_ADAPTER_IDENTITY","1002:67ef",1);
    CHECK(d3d->GetAdapterIdentifier(0,0,&id) == D3D_OK && id.VendorId == 0x1002 && id.DeviceId == 0x67ef);
    unsetenv("BF2_TEST_ADAPTER_IDENTITY");
    std::puts("PASS: closure adapter identity (real PCI vendor/device, test-hook override)");
    return 0;
}

// D3DX accepts an unsized shader-object array initialized by compile expressions (xpack
// RaShaderSTM.fx vsStaticMesh_Array); a pass selects an element by parameter.
static const char kClosureShaderArrayEffect[] =
    "int Sel = 0;\n"
    "float4 vs(float4 p : POSITION, uniform bool flip) : POSITION { return flip ? -p : p; }\n"
    "VertexShader vsArr[] = { compile vs_2_0 vs(false), compile vs_2_0 vs(true) };\n"
    "float4 ps() : COLOR { return 1; }\n"
    "technique T { pass P { VertexShader = (vsArr[Sel]); PixelShader = compile ps_2_0 ps(); } }\n";

static int closureShaderArrayChecks(IDirect3DDevice9* d)
{
    ID3DXEffect* fx = nullptr; ID3DXBuffer* errors = nullptr;
    const HRESULT hr = D3DXCreateEffect(d,kClosureShaderArrayEffect,UINT(sizeof(kClosureShaderArrayEffect) - 1),nullptr,nullptr,0,nullptr,&fx,&errors);
    if (FAILED(hr) && errors) std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));
    CHECK(SUCCEEDED(hr) && fx);
    D3DXPARAMETER_DESC pd = {};
    CHECK(fx->GetParameterDesc(fx->GetParameterByName(nullptr,"vsArr"),&pd) == D3D_OK && pd.Elements == 2);
    IDirect3DVertexShader9* chosen[2] = {};
    for (int sel = 0; sel < 2; ++sel) {
        UINT passes = 0;
        CHECK(fx->SetInt(fx->GetParameterByName(nullptr,"Sel"),sel) == D3D_OK);
        CHECK(fx->Begin(&passes,7) == D3D_OK && fx->BeginPass(0) == D3D_OK);
        CHECK(d->GetVertexShader(&chosen[sel]) == D3D_OK && chosen[sel]);
        CHECK(fx->EndPass() == D3D_OK && fx->End() == D3D_OK);
    }
    CHECK(chosen[0] != chosen[1]);
    chosen[0]->Release(); chosen[1]->Release();
    CHECK(d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    CHECK(fx->Release() == 0);
    std::puts("PASS: closure unsized shader-object array (compile-expression initializer, selector)");
    return 0;
}

// [CHOSEN, not recovered] SM1/2 jump lowering (vkd3d 0014): a loop with a uniform exit and an
// in-loop break, unrolled to the array bound, compiles natively as vs_1_1 (it used to fail with
// E5017 and take the internal vs_3_0, BuildReport::higherVertexProfile, now only a fallback; test
// hook BF2_FX_TEST_HIGHER_VERTEX_PROFILE=0 turns it off here). The vs_1_1 and vs_3_0 techniques
// give the same texcoords; vs_1_1 keeps its semantics: oD0 is clamped to [0, 1].
static const char kClosureHigherProfileEffect[] =
    "int Count = 2;\n"
    "float4x4 M[6];\n"
    "struct VO { float4 p : POSITION; float4 c : COLOR0; float4 t[6] : TEXCOORD0; };\n"
    "VO vs(float4 p : POSITION) {\n"
    "    VO o; o.p = p; o.c = float4(2, 0.25, 0, 1);\n"
    "    for (int k = 0; k < 6; ++k) o.t[k] = 0;\n"
    "    for (int q = 0; q < Count; ++q) { if (p.x > 2.0) break; o.t[q] = mul(float4(1, 1, 1, 1), M[q]); }\n"
    "    return o;\n"
    "}\n"
    "float4 ps(float4 c : COLOR0, float4 t0 : TEXCOORD0, float4 t1 : TEXCOORD1) : COLOR {\n"
    "    return float4(c.r - 0.5, c.g, t0.x + t1.y, 1);\n"
    "}\n"
    "technique Low { pass P { CullMode = NONE; ZEnable = FALSE; VertexShader = compile vs_1_1 vs(); PixelShader = compile ps_2_0 ps(); } }\n"
    "technique High { pass P { CullMode = NONE; ZEnable = FALSE; VertexShader = compile vs_3_0 vs(); PixelShader = compile ps_2_0 ps(); } }\n";

static int closureHigherProfileChecks(IDirect3DDevice9* d)
{
    setenv("BF2_FX_TEST_HIGHER_VERTEX_PROFILE","0",1);
    ID3DXEffect* fx = nullptr; ID3DXBuffer* errors = nullptr;
    const HRESULT hr = D3DXCreateEffect(d,kClosureHigherProfileEffect,UINT(sizeof(kClosureHigherProfileEffect) - 1),nullptr,nullptr,0,nullptr,&fx,&errors);
    unsetenv("BF2_FX_TEST_HIGHER_VERTEX_PROFILE");
    if (FAILED(hr) && errors) std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));
    CHECK(SUCCEEDED(hr) && fx);
    D3DXMATRIX m[6] = {};
    m[0]._11 = 0.25f; m[1]._12 = 0.25f;                 // t0.x = 0.25, t1.y = 0.25 -> blue 0.5
    CHECK(fx->SetMatrixArray(fx->GetParameterByName(nullptr,"M"),m,6) == D3D_OK);
    IDirect3DSurface9 *old = nullptr, *target = nullptr, *cpu = nullptr;
    CHECK(d->GetRenderTarget(0,&old) == D3D_OK);
    CHECK(d->CreateRenderTarget(8,4,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr) == D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(8,4,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr) == D3D_OK);
    const D3DVERTEXELEMENT9 elements[] = {{0,0,D3DDECLTYPE(3),0,0,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* decl = nullptr;
    CHECK(d->CreateVertexDeclaration(elements,&decl) == D3D_OK);
    CHECK(d->SetRenderTarget(0,target) == D3D_OK && d->SetVertexDeclaration(decl) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0) == D3D_OK);
    const char* techniques[2] = {"Low","High"};
    for (int i = 0; i < 2; ++i) {
        const D3DVIEWPORT9 vp = {DWORD(i * 4),0,4,4,0,1};
        const float q[4][4] = {{-1,1,0.5f,1},{1,1,0.5f,1},{-1,-1,0.5f,1},{1,-1,0.5f,1}};
        UINT passes = 0;
        CHECK(fx->SetTechnique(fx->GetTechniqueByName(techniques[i])) == D3D_OK && fx->Begin(&passes,7) == D3D_OK && fx->BeginPass(0) == D3D_OK);
        // The bound vertex shader is native bytecode of the technique's profile (vs_1_1 / vs_3_0).
        IDirect3DVertexShader9* vs = nullptr; UINT size = 0;
        CHECK(d->GetVertexShader(&vs) == D3D_OK && vs && vs->GetFunction(nullptr,&size) == D3D_OK && size >= 4);
        std::vector<DWORD> code(size / 4);
        CHECK(vs->GetFunction(code.data(),&size) == D3D_OK);
        CHECK(code[0] == (i == 0 ? 0xfffe0101u : 0xfffe0300u));
        vs->Release();
        CHECK(d->SetViewport(&vp) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,16) == D3D_OK);
        CHECK(fx->EndPass() == D3D_OK && fx->End() == D3D_OK);
    }
    CHECK(d->EndScene() == D3D_OK);
    D3DLOCKED_RECT lock = {};
    CHECK(d->GetRenderTargetData(target,cpu) == D3D_OK && cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY) == D3D_OK);
    const unsigned* row = reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(lock.pBits) + lock.Pitch);
    const unsigned low = row[1], high = row[5];
    CHECK(cpu->UnlockRect() == D3D_OK);
    std::printf("higher profile: vs_1_1 path 0x%08x, vs_3_0 path 0x%08x\n",low,high);
    // Blue (t0.x + t1.y = 0.5) and green (0.25) match; red differs only by vs_1_1's oD0 clamp:
    // vs_1_1 clamps 2 to 1 -> 0.5; vs_3_0 keeps 2 -> 1.5 -> saturated 1.
    auto near = [](unsigned v, unsigned want) { return (v > want ? v - want : want - v) <= 2u; };
    CHECK(near(low & 0xff,0x80) && near(high & 0xff,0x80));               // blue
    CHECK(near((low >> 8) & 0xff,0x40) && near((high >> 8) & 0xff,0x40)); // green
    CHECK(near((low >> 16) & 0xff,0x80) && ((high >> 16) & 0xff) == 0xff); // red
    CHECK(d->SetRenderTarget(0,old) == D3D_OK && d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    old->Release(); decl->Release(); cpu->Release(); target->Release(); fx->Release();
    std::puts("PASS: closure SM1/2 jump lowering (native vs_1_1 for a break loop, same texcoords as vs_3_0, vs_1_1 colour clamp kept)");
    return 0;
}

// D3DRS_SHADEMODE = D3DSHADE_FLAT (the geom debug drawers, IDebugLineDrawer::drawD3dxLine /
// IDebugCircleDrawer::drawCircle): the colour input is the first vertex's over the whole
// triangle; GOURAUD interpolates. Both the pixel-shader and the fixed-function stage paths.
static const char kClosureFlatEffect[] =
    "struct V { float4 p : POSITION; float4 c : COLOR0; };\n"
    "V vs(V v) { return v; }\n"
    "float4 ps(float4 c : COLOR0) : COLOR { return c; }\n"
    "technique Shader { pass P { CullMode = NONE; ZEnable = FALSE; VertexShader = compile vs_2_0 vs(); PixelShader = compile ps_2_0 ps(); } }\n"
    "technique Fixed { pass P { CullMode = NONE; ZEnable = FALSE; VertexShader = compile vs_2_0 vs(); PixelShader = NULL;\n"
    "  ColorOp[0] = SELECTARG1; ColorArg1[0] = DIFFUSE; AlphaOp[0] = SELECTARG1; AlphaArg1[0] = DIFFUSE; ColorOp[1] = DISABLE; AlphaOp[1] = DISABLE; } }\n";

static int closureFlatShadeChecks(IDirect3DDevice9* d)
{
    ID3DXEffect* fx = nullptr; ID3DXBuffer* errors = nullptr;
    const HRESULT hr = D3DXCreateEffect(d,kClosureFlatEffect,UINT(sizeof(kClosureFlatEffect) - 1),nullptr,nullptr,0,nullptr,&fx,&errors);
    if (FAILED(hr) && errors) std::fprintf(stderr,"%s\n",static_cast<const char*>(errors->GetBufferPointer()));
    CHECK(SUCCEEDED(hr) && fx);
    IDirect3DSurface9 *old = nullptr, *target = nullptr, *cpu = nullptr;
    CHECK(d->GetRenderTarget(0,&old) == D3D_OK);
    CHECK(d->CreateRenderTarget(32,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr) == D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(32,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr) == D3D_OK);
    const D3DVERTEXELEMENT9 elements[] = {{0,0,D3DDECLTYPE(3),0,0,0},{0,16,D3DDECLTYPE(4),0,10,0},D3DDECL_END()};
    IDirect3DVertexDeclaration9* decl = nullptr;
    CHECK(d->CreateVertexDeclaration(elements,&decl) == D3D_OK);
    struct Vtx { float x,y,z,w; DWORD c; };
    // One quad per case: first vertex red, the others green.
    const Vtx q[4] = {{-1,1,0.5f,1,0xffff0000u},{1,1,0.5f,1,0xff00ff00u},{-1,-1,0.5f,1,0xff00ff00u},{1,-1,0.5f,1,0xff00ff00u}};
    CHECK(d->SetRenderTarget(0,target) == D3D_OK && d->SetVertexDeclaration(decl) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0) == D3D_OK);
    struct Case { const char* technique; DWORD shade; };
    const Case cases[4] = {{"Shader",1},{"Shader",2},{"Fixed",1},{"Fixed",2}};
    for (int i = 0; i < 4; ++i) {
        const D3DVIEWPORT9 vp = {DWORD(i * 8),0,8,8,0,1};
        UINT passes = 0;
        CHECK(fx->SetTechnique(fx->GetTechniqueByName(cases[i].technique)) == D3D_OK && fx->Begin(&passes,7) == D3D_OK && fx->BeginPass(0) == D3D_OK);
        CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(9),cases[i].shade) == D3D_OK); // D3DRS_SHADEMODE
        CHECK(d->SetViewport(&vp) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(Vtx)) == D3D_OK);
        CHECK(fx->EndPass() == D3D_OK && fx->End() == D3D_OK);
    }
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(9),2) == D3D_OK && d->EndScene() == D3D_OK);
    D3DLOCKED_RECT lock = {};
    CHECK(d->GetRenderTargetData(target,cpu) == D3D_OK && cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY) == D3D_OK);
    int failures = 0;
    for (int i = 0; i < 4; ++i) {
        // The first triangle (vertices 0,1,2) near vertex 1 (top right): flat = red (provoking
        // vertex 0), gouraud = mostly green.
        const unsigned px = reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(lock.pBits) + 1 * lock.Pitch)[i * 8 + 5];
        const unsigned red = (px >> 16) & 0xff, green = (px >> 8) & 0xff;
        const bool ok = cases[i].shade == 1 ? (red == 0xff && green == 0) : (green > red);
        if (!ok) { std::fprintf(stderr,"FAIL shade mode %lu (%s): 0x%08x\n",(unsigned long)cases[i].shade,cases[i].technique,px); ++failures; }
    }
    CHECK(cpu->UnlockRect() == D3D_OK);
    CHECK(d->SetRenderTarget(0,old) == D3D_OK && d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    old->Release(); decl->Release(); cpu->Release(); target->Release(); fx->Release();
    CHECK(failures == 0);
    std::puts("PASS: closure D3DRS_SHADEMODE flat (first-vertex colour) and gouraud, pixel shader and fixed-function stage");
    return 0;
}

// 0.3.0 piece 1: draws without a vertex shader (fixed-function vertex processing, FVF declarations).
// D3D9 reference: XYZRHW positions are screen space; XYZ goes through world, view and projection;
// vertex fog (FOGTABLEMODE NONE) blends toward FOGCOLOR by the per-vertex factor; D3DTS_TEXTURE0
// transforms (u, v, 1, 0) with D3DTTFF_COUNT2. The per-vertex results are checked against the real
// runtime in Code/Tests/ff_reference (ff_check, ff_metal_check); this checks the draw path itself.
static int closureFixedFunctionVertexChecks(IDirect3DDevice9* d)
{
    IDirect3DSurface9 *old = nullptr, *target = nullptr, *cpu = nullptr;
    CHECK(d->GetRenderTarget(0,&old) == D3D_OK);
    CHECK(d->CreateRenderTarget(40,8,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&target,nullptr) == D3D_OK);
    CHECK(d->CreateOffscreenPlainSurface(40,8,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,nullptr) == D3D_OK);
    IDirect3DTexture9* tex = nullptr;
    CHECK(d->CreateTexture(2,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,nullptr) == D3D_OK);
    { D3DLOCKED_RECT lock = {}; CHECK(tex->LockRect(0,&lock,nullptr,0) == D3D_OK);
      unsigned* p = static_cast<unsigned*>(lock.pBits); p[0] = 0xffff0000u; p[1] = 0xff00ff00u; CHECK(tex->UnlockRect(0) == D3D_OK); }
    CHECK(d->SetRenderTarget(0,target) == D3D_OK && d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK);
    CHECK(d->SetRenderState(D3DRS_ZENABLE,0) == D3D_OK && d->SetRenderState(D3DRS_CULLMODE,1) == D3D_OK);
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(137),FALSE) == D3D_OK); // D3DRS_LIGHTING
    CHECK(d->BeginScene() == D3D_OK && d->Clear(0,nullptr,D3DCLEAR_TARGET,0,1,0) == D3D_OK);
    D3DMATRIX ident = {}; ident._11 = ident._22 = ident._33 = ident._44 = 1;
    CHECK(d->SetTransform(D3DTS_VIEW,&ident) == D3D_OK && d->SetTransform(D3DTS_PROJECTION,&ident) == D3D_OK);
    // case 0: XYZRHW | DIFFUSE, screen-space quad over the top half of cell 0
    { const D3DVIEWPORT9 vp = {0,0,8,8,0,1}; CHECK(d->SetViewport(&vp) == D3D_OK);
      struct V { float x,y,z,rhw; DWORD c; };
      const V q[4] = {{0,0,0.5f,1,0xffff0000u},{8,0,0.5f,1,0xffff0000u},{0,4,0.5f,1,0xffff0000u},{8,4,0.5f,1,0xffff0000u}};
      CHECK(d->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(V)) == D3D_OK); }
    // case 1: XYZ | DIFFUSE, world translates a [-0.5, 0.5] quad right by 0.5 (cell 1)
    struct P { float x,y,z; DWORD c; };
    const P quad[4] = {{-0.5f,1,0.75f,0xff00ff00u},{0.5f,1,0.75f,0xff00ff00u},{-0.5f,-1,0.75f,0xff00ff00u},{0.5f,-1,0.75f,0xff00ff00u}};
    D3DMATRIX world = ident; world._41 = 0.5f;
    { const D3DVIEWPORT9 vp = {8,0,8,8,0,1}; CHECK(d->SetViewport(&vp) == D3D_OK);
      CHECK(d->SetTransform(D3DTS_WORLD,&world) == D3D_OK);
      CHECK(d->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(P)) == D3D_OK); }
    // case 2: the same with linear vertex fog 0..1 toward blue: camera z 0.75 -> factor 0.25 (cell 2)
    { const D3DVIEWPORT9 vp = {16,0,8,8,0,1}; CHECK(d->SetViewport(&vp) == D3D_OK);
      const float start = 0, end = 1; DWORD s, e; std::memcpy(&s,&start,4); std::memcpy(&e,&end,4);
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(28),TRUE) == D3D_OK && d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(34),0xff0000ffu) == D3D_OK);
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(140),3) == D3D_OK); // D3DRS_FOGVERTEXMODE LINEAR
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(36),s) == D3D_OK && d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(37),e) == D3D_OK);
      CHECK(d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,quad,sizeof(P)) == D3D_OK);
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(28),FALSE) == D3D_OK && d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(140),0) == D3D_OK); }
    // case 3: XYZ | TEX1 full-cell quad, u 0..1 over a red|green texture shifted by +0.5 (cell 3):
    // left half samples u 0.5..1 (green), right half wraps to 0..0.5 (red)
    { const D3DVIEWPORT9 vp = {24,0,8,8,0,1}; CHECK(d->SetViewport(&vp) == D3D_OK);
      struct T { float x,y,z,u,v; };
      const T q[4] = {{-1,1,0.5f,0,0},{1,1,0.5f,1,0},{-1,-1,0.5f,0,1},{1,-1,0.5f,1,1}};
      D3DMATRIX shift = ident; shift._31 = 0.5f;   // (u, v, 1, 0) * M: the third row translates 2D coordinates
      CHECK(d->SetTransform(D3DTS_WORLD,&ident) == D3D_OK && d->SetTransform(D3DTS_TEXTURE0,&shift) == D3D_OK);
      CHECK(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,2) == D3D_OK && d->SetTexture(0,tex) == D3D_OK);
      // stage 0 as the D3D9 defaults (earlier checks' effects leave other values): MODULATE TEXTURE, DIFFUSE
      const DWORD stage[5][2] = {{1,4},{2,2},{3,0},{4,2},{5,2}}; // COLOROP, COLORARG1, COLORARG2, ALPHAOP, ALPHAARG1
      for (const auto& st : stage) CHECK(d->SetTextureStageState(0,static_cast<D3DTEXTURESTAGESTATETYPE>(st[0]),st[1]) == D3D_OK);
      CHECK(d->SetSamplerState(0,D3DSAMP_ADDRESSU,1) == D3D_OK && d->SetSamplerState(0,D3DSAMP_ADDRESSV,1) == D3D_OK); // WRAP
      for (DWORD f : {5u,6u,7u}) CHECK(d->SetSamplerState(0,static_cast<D3DSAMPLERSTATETYPE>(f),f == 7 ? 0 : 1) == D3D_OK); // MAG/MIN POINT, MIP NONE
      CHECK(d->SetFVF(D3DFVF_XYZ | D3DFVF_TEX1) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(T)) == D3D_OK);
      CHECK(d->SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,0) == D3D_OK && d->SetTexture(0,nullptr) == D3D_OK); }
    // case 4: XYZ | NORMAL lit by one directional light along +z (towards the camera-facing normal):
    // emissive 0 + material ambient (1,0,0) * D3DRS_AMBIENT 0x40 + material diffuse (0,1,0) * 1 (cell 4)
    { const D3DVIEWPORT9 vp = {32,0,8,8,0,1}; CHECK(d->SetViewport(&vp) == D3D_OK);
      struct N { float x,y,z,nx,ny,nz; };
      const N q[4] = {{-1,1,0.5f,0,0,-1},{1,1,0.5f,0,0,-1},{-1,-1,0.5f,0,0,-1},{1,-1,0.5f,0,0,-1}};
      D3DMATERIAL9 m = {}; m.Diffuse.g = 1; m.Diffuse.a = 1; m.Ambient.r = 1;
      unsigned char l[104] = {};   // D3DLIGHT9: Type 3 (directional), Diffuse 1,1,1, Direction 0,0,1
      const DWORD directional = 3; const float one = 1.0f; std::memcpy(l,&directional,4);
      for (int i = 0; i < 3; ++i) std::memcpy(l + 4 + 4 * i,&one,4);
      std::memcpy(l + 72,&one,4);
      CHECK(d->SetMaterial(&m) == D3D_OK && d->SetLight(0,reinterpret_cast<const D3DLIGHT9*>(l)) == D3D_OK && d->LightEnable(0,TRUE) == D3D_OK);
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(137),TRUE) == D3D_OK);   // D3DRS_LIGHTING
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(139),0xff404040u) == D3D_OK); // D3DRS_AMBIENT
      CHECK(d->SetFVF(D3DFVF_XYZ | D3DFVF_NORMAL) == D3D_OK && d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,q,sizeof(N)) == D3D_OK);
      CHECK(d->LightEnable(0,FALSE) == D3D_OK && d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(139),0) == D3D_OK);
      CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(137),FALSE) == D3D_OK); }
    CHECK(d->EndScene() == D3D_OK);
    D3DLOCKED_RECT lock = {};
    CHECK(d->GetRenderTargetData(target,cpu) == D3D_OK && cpu->LockRect(&lock,nullptr,D3DLOCK_READONLY) == D3D_OK);
    auto at = [&](int x,int y) { return reinterpret_cast<const unsigned*>(static_cast<unsigned char*>(lock.pBits) + y * lock.Pitch)[x]; };
    auto near = [](unsigned px,unsigned want) {
        for (int s = 0; s < 24; s += 8) if (std::abs(int((px >> s) & 255) - int((want >> s) & 255)) > 3) return false;
        return true;
    };
    struct Probe { int x, y; unsigned want; const char* what; };
    const Probe probes[] = {
        {2,1,0xffff0000u,"XYZRHW quad inside"}, {2,6,0xff000000u,"XYZRHW quad outside"},
        {9,4,0xff000000u,"world-translated quad, left of it"}, {14,4,0xff00ff00u,"world-translated quad"},
        {22,4,0xff0040bfu,"vertex fog 0.25 toward blue"},
        {25,4,0xff00ff00u,"texture transform, left half"}, {30,4,0xffff0000u,"texture transform, right half"},
        {36,4,0xff40ff00u,"directional light, ambient"},
    };
    int failures = 0;
    for (const Probe& p : probes)
        if (!near(at(p.x,p.y),p.want)) { std::fprintf(stderr,"FAIL fixed-function vertex: %s at (%d,%d): 0x%08x, want 0x%08x\n",p.what,p.x,p.y,at(p.x,p.y),p.want); ++failures; }
    CHECK(cpu->UnlockRect() == D3D_OK);
    DWORD fvf = 1;
    CHECK(d->GetFVF(&fvf) == D3D_OK && fvf == (D3DFVF_XYZ | D3DFVF_NORMAL));
    CHECK(d->SetFVF(0x1) == D3DERR_INVALIDCALL);   // D3DFVF_RESERVED0
    CHECK(d->SetRenderState(static_cast<D3DRENDERSTATETYPE>(137),TRUE) == D3D_OK && d->SetRenderTarget(0,old) == D3D_OK);
    CHECK(d->SetTransform(D3DTS_TEXTURE0,&ident) == D3D_OK && d->SetTransform(D3DTS_WORLD,&ident) == D3D_OK);
    old->Release(); cpu->Release(); target->Release(); tex->Release();
    CHECK(failures == 0);
    std::puts("PASS: closure fixed-function vertex draws: XYZRHW, world/view/projection, vertex fog, texture transform, directional light with ambient, FVF declarations");
    return 0;
}
