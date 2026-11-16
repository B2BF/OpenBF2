// [TEMP] [CHOSEN, not recovered] CPU state-manager observer; no rendering/device backend.
// Goldens come from original D3DX9_25 SetVector -> BeginPass callbacks, flags DONOTSAVESTATE.
#include "../../BF2/Platform/Gfx/D3DX9Subset.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <stdexcept>

namespace {
[[noreturn]] void unexpected(const char* name) {
    std::fprintf(stderr, "Unexpected graphics call in CPU state fixture: %s\n", name);
    std::abort();
}
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void hr(HRESULT value, const char* message) { check(SUCCEEDED(value), message); }
struct Device final : IDirect3DDevice9 {
    ULONG refs = 1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { unexpected("QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    // Every non-IUnknown device operation must remain unused in this state-only effect.
#define BF2_D3D_METHOD(ret, name, args) ret STDMETHODCALLTYPE name args override { unexpected(#name); }
#include "state-device-methods.inc"
#undef BF2_D3D_METHOD
};
struct Observer final : ID3DXEffectStateManager {
    ULONG refs = 1;
    std::map<DWORD, DWORD> states;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void**) override { unexpected("QueryInterface"); }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { return --refs; }
    HRESULT STDMETHODCALLTYPE SetRenderState(D3DRENDERSTATETYPE state, DWORD value) override {
        states[DWORD(state)] = value; return S_OK;
    }
#define UNUSED(name, args) HRESULT STDMETHODCALLTYPE name args override { unexpected(#name); }
    UNUSED(SetTransform, (D3DTRANSFORMSTATETYPE, const D3DMATRIX*))
    UNUSED(SetMaterial, (const D3DMATERIAL9*))
    UNUSED(SetLight, (DWORD, const D3DLIGHT9*))
    UNUSED(LightEnable, (DWORD, BOOL))
    UNUSED(SetTexture, (DWORD, IDirect3DBaseTexture9*))
    UNUSED(SetTextureStageState, (DWORD, D3DTEXTURESTAGESTATETYPE, DWORD))
    UNUSED(SetSamplerState, (DWORD, D3DSAMPLERSTATETYPE, DWORD))
    UNUSED(SetNPatchMode, (FLOAT))
    UNUSED(SetFVF, (DWORD))
    UNUSED(SetVertexShader, (IDirect3DVertexShader9*))
    UNUSED(SetVertexShaderConstantF, (UINT, const FLOAT*, UINT))
    UNUSED(SetVertexShaderConstantI, (UINT, const INT*, UINT))
    UNUSED(SetVertexShaderConstantB, (UINT, const BOOL*, UINT))
    UNUSED(SetPixelShader, (IDirect3DPixelShader9*))
    UNUSED(SetPixelShaderConstantF, (UINT, const FLOAT*, UINT))
    UNUSED(SetPixelShaderConstantI, (UINT, const INT*, UINT))
    UNUSED(SetPixelShaderConstantB, (UINT, const BOOL*, UINT))
#undef UNUSED
};
const char source[] = R"fx(
float4 Color;
int Packed;
float Scalar;
technique VectorColor { pass P { TextureFactor = (Color); FogColor = (Color); } }
technique ScalarColor { pass P { TextureFactor = (Packed); FogColor = (Packed); FogStart = (Scalar); AlphaRef = (Scalar); } }
)fx";
struct EffectOwner { ID3DXEffect* value = nullptr; ~EffectOwner() { if (value) value->Release(); } };
void apply(ID3DXEffect* effect, const char* technique) {
    hr(effect->SetTechnique(effect->GetTechniqueByName(technique)), "SetTechnique");
    UINT passes = 0;
    hr(effect->Begin(&passes, 1), "Begin DONOTSAVESTATE");
    check(passes == 1, "One state-only pass");
    hr(effect->BeginPass(0), "BeginPass");
    hr(effect->EndPass(), "EndPass");
    hr(effect->End(), "End");
}
}
int main(int argc, char** argv) try {
    check(argc == 2, "scratch directory argument");
    std::filesystem::create_directories(argv[1]);
    check(!setenv("BF2_FX_NATIVE_CACHE", argv[1], 1), "Set isolated effect cache");
    Device device;
    Observer observer;
    EffectOwner effect;
    ID3DXBuffer* errors = nullptr;
    const HRESULT compiled = D3DXCreateEffect(&device, source, UINT(std::strlen(source)), nullptr,
        nullptr, 0, nullptr, &effect.value, &errors);
    if (errors) {
        std::fprintf(stderr, "%.*s\n", int(errors->GetBufferSize()), static_cast<const char*>(errors->GetBufferPointer()));
        errors->Release();
    }
    hr(compiled, "Create production native effect");
    hr(effect.value->SetStateManager(&observer), "Set observer");
    const D3DXHANDLE color = effect.value->GetParameterByName(nullptr, "Color");
    const struct { D3DXVECTOR4 color; DWORD golden; } cases[] = {
        {{0, 0, 0, .8f}, 0xcc000000}, {{.25f, .5f, .75f, 1}, 0xff4080bf},
        {{-1, 2, .5f, 0}, 0x0000ff80}, {{.1f, .2f, .3f, .4f}, 0x661a334d},
        {{NAN, INFINITY, -INFINITY, NAN}, 0x0000ff00},
        {{NAN, 0, 0, 1}, 0xff000000}, {{0, NAN, 0, 1}, 0xff000000}, {{0, 0, NAN, 1}, 0xff000000}
    };
    for (const auto& test : cases) {
        observer.states.clear();
        hr(effect.value->SetVector(color, &test.color), "SetVector");
        apply(effect.value, "VectorColor");
        check(observer.states.size() == 2, "Both packed-color callbacks");
        check(observer.states.at(60) == test.golden, "TextureFactor original D3DX golden");
        check(observer.states.at(34) == test.golden, "FogColor original D3DX golden");
    }
    // Original D3DX9_25 callbacks at byte thresholds and their float neighbors, with
    // D3D9's default 24-bit x87 precision. Raw inputs keep compiler folding out of the oracle.
    const struct { DWORD input, golden; } boundaries[] = {
        {0x3b008080, 0x01010101}, {0x3b008081, 0x01010101}, {0x3b008082, 0x01010101},
        {0x3bc0c0c0, 0x01010101}, {0x3bc0c0c1, 0x02020202}, {0x3bc0c0c2, 0x02020202},
        {0x3effffff, 0x7f7f7f7f}, {0x3f000000, 0x80808080}, {0x3f000001, 0x80808080},
        {0x3f7f7f7e, 0xfefefefe}, {0x3f7f7f7f, 0xffffffff}, {0x3f7f7f80, 0xffffffff}
    };
    for (const auto& test : boundaries) {
        float lane; std::memcpy(&lane, &test.input, sizeof lane);
        const D3DXVECTOR4 value{lane, lane, lane, lane};
        hr(effect.value->SetVector(color, &value), "SetVector boundary");
        apply(effect.value, "VectorColor");
        check(observer.states.at(60) == test.golden && observer.states.at(34) == test.golden,
              "Packed color matches original single-precision boundary golden");
    }
    observer.states.clear();
    // The existing expression evaluator uses float registers, so choose an exactly
    // representable signed integer to isolate the scalar state conversion contract.
    constexpr DWORD packed = 0xa1b2c300u;
    hr(effect.value->SetInt(effect.value->GetParameterByName(nullptr, "Packed"), INT(packed)), "SetInt color");
    const float scalar = 12.75f;
    hr(effect.value->SetFloat(effect.value->GetParameterByName(nullptr, "Scalar"), scalar), "SetFloat scalar");
    apply(effect.value, "ScalarColor");
    DWORD bits; std::memcpy(&bits, &scalar, sizeof bits);
    check(observer.states.at(60) == packed && observer.states.at(34) == packed,
          "Integer packed colors remain unchanged");
    check(observer.states.at(36) == bits, "Ordinary float state keeps float bits");
    check(observer.states.at(24) == 12, "Ordinary numeric state keeps integer conversion");
    effect.value->Release(); effect.value = nullptr;
    check(device.refs == 1 && observer.refs == 1, "Effect releases retained observers");
    std::puts("PASS: production native packed-color state callbacks match original D3DX9_25");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what()); return 1;
}
