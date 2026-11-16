// [TEMP] [CHOSEN, not recovered] D3DX effect -> DXVK -> pixel-readback check, issue #36.
#include "../../BF2/Platform/Gfx/D3DX9Subset.h"
#include "Native.h"
#include "relative_load.h"
#include <SDL3/SDL.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {
constexpr auto alphaBlendEnable = static_cast<D3DRENDERSTATETYPE>(27); // Public D3DRS_ALPHABLENDENABLE.
void check(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
void hr(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        char code[16]; std::snprintf(code, sizeof(code), "0x%08x", unsigned(result));
        throw std::runtime_error(std::string(operation) + ": " + code);
    }
}
template<class T> struct Owner {
    T* value = nullptr;
    ~Owner() { if (value) value->Release(); }
    T* operator->() const { return value; }
};
const char effectSource[] = R"fx(
float4 Tint;
texture ColorTexture;
sampler2D ColorSampler = sampler_state { Texture = <ColorTexture>; MinFilter = POINT; MagFilter = POINT; };
struct Vertex { float4 position : POSITION0; float2 uv : TEXCOORD0; };
Vertex vertexMain(Vertex input) { return input; }
float4 pixelMain(float2 uv : TEXCOORD0) : COLOR0 { return tex2D(ColorSampler, uv) * Tint; }
technique Main {
    pass Draw {
        AlphaBlendEnable = FALSE;
        VertexShader = compile vs_2_0 vertexMain();
        PixelShader = compile ps_2_0 pixelMain();
    }
}
)fx";

void validate(ID3DXEffect* effect, const char* phase) {
    const D3DXHANDLE main = effect->GetTechniqueByName("Main");
    check(main != nullptr, std::string(phase) + ": Main technique exists");
    hr(effect->ValidateTechnique(main), (std::string(phase) + ": ValidateTechnique").c_str());
    D3DXHANDLE selected = nullptr;
    hr(effect->FindNextValidTechnique(nullptr, &selected), (std::string(phase) + ": FindNextValidTechnique").c_str());
    check(selected == main, std::string(phase) + ": bytecode technique selected");
    check(effect->FindNextValidTechnique(main, &selected) == S_FALSE && !selected,
          std::string(phase) + ": technique enumeration ends");
}

void relativeLoads(IDirect3DDevice9* device) {
    Owner<ID3DXEffect> effect;
    Owner<ID3DXBuffer> errors;
    const HRESULT compiled = D3DXCreateEffect(device, relativeLoadSource, UINT(std::strlen(relativeLoadSource)),
        nullptr, nullptr, 0, nullptr, &effect.value, &errors.value);
    if (errors.value) std::fprintf(stderr, "%.*s\n", int(errors->GetBufferSize()), static_cast<const char*>(errors->GetBufferPointer()));
    hr(compiled, "Relative-load D3DXCreateEffect");
    validate(effect.value, "Relative-load effect");
    hr(effect->SetTechnique(effect->GetTechniqueByName("Main")), "Relative-load SetTechnique");
    D3DXMATRIX matrices[4]{};
    for (unsigned index = 0; index < 4; ++index)
        for (unsigned row = 0; row < 4; ++row)
            for (unsigned column = 0; column < 3; ++column)
                matrices[index].m[row][column] = 0.1f * float(1 + 3 * index + 2 * row + column);
    hr(effect->SetMatrixArray(effect->GetParameterByName(nullptr, "Matrices"), matrices, 4), "Relative-load matrices");
    const D3DVERTEXELEMENT9 elements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_BLENDINDICES, 0},
        {0, 24, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 0},
        {0, 36, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 1},
        {0, 48, D3DDECLTYPE_FLOAT3, 0, D3DDECLUSAGE_TEXCOORD, 2}, D3DDECL_END()};
    Owner<IDirect3DVertexDeclaration9> declaration;
    hr(device->CreateVertexDeclaration(elements, &declaration.value), "Relative-load declaration");
    hr(device->SetVertexDeclaration(declaration.value), "Relative-load SetVertexDeclaration");
    const float basis[3][3] = {{1, .5f, 0}, {.25f, 1, 0}, {0, .25f, 1}};
    float vertices[4][15]{};
    for (unsigned vertex = 0; vertex < 4; ++vertex) {
        vertices[vertex][0] = vertex & 1 ? 1.f : -1.f;
        vertices[vertex][1] = vertex & 2 ? -1.f : 1.f;
        vertices[vertex][3] = 1;
        std::memcpy(vertices[vertex] + 6, basis, sizeof(basis));
    }
    const unsigned pairs[][2] = {{1, 3}, {2, 0}};
    for (const auto& pair : pairs) {
        for (auto& vertex : vertices) { vertex[4] = float(pair[0]); vertex[5] = float(pair[1]); }
        hr(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0), "Relative-load Clear");
        hr(device->BeginScene(), "Relative-load BeginScene");
        UINT passes = 0;
        hr(effect->Begin(&passes, 0), "Relative-load Begin");
        check(passes == 1, "Relative-load one pass");
        hr(effect->BeginPass(0), "Relative-load BeginPass");
        hr(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0])), "Relative-load Draw");
        hr(effect->EndPass(), "Relative-load EndPass");
        hr(effect->End(), "Relative-load End");
        hr(device->EndScene(), "Relative-load EndScene");
        Owner<IDirect3DSurface9> back, copy;
        hr(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back.value), "Relative-load GetBackBuffer");
        hr(device->CreateOffscreenPlainSurface(64, 64, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &copy.value, nullptr), "Relative-load readback surface");
        hr(device->GetRenderTargetData(back.value, copy.value), "Relative-load readback");
        D3DLOCKED_RECT locked{};
        hr(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY), "Relative-load LockRect");
        std::array<unsigned char, 3> pixel{};
        std::memcpy(pixel.data(), static_cast<unsigned char*>(locked.pBits) + 32 * locked.Pitch + 32 * 4, 3);
        hr(copy->UnlockRect(), "Relative-load UnlockRect");
        for (unsigned component = 0; component < 3; ++component) {
            float expected = 0;
            for (unsigned index = 0; index < 2; ++index)
                for (unsigned row = 0; row < 3; ++row)
                    for (unsigned column = 0; column < 3; ++column)
                        expected += basis[component][row] * matrices[pair[index]].m[row][column] * (index ? .025f : .05f);
            check(std::abs(int(pixel[2 - component]) - int(std::lround(expected * 255))) <= 1,
                  "Relative-load pixel agrees with independent CPU matrix multiplication");
        }
        std::printf("Relative-load matrices %u/%u pixel BGR: %u %u %u\n", pair[0], pair[1], pixel[0], pixel[1], pixel[2]);
    }
}

void run(SDL_Window* window) {
    Owner<IDirect3D9> d3d;
    d3d.value = Direct3DCreate9(D3D_SDK_VERSION);
    check(d3d.value != nullptr, "Direct3DCreate9");
    D3DPRESENT_PARAMETERS pp{};
    pp.BackBufferWidth = pp.BackBufferHeight = 64;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = window;
    pp.Windowed = TRUE;
    Owner<IDirect3DDevice9> device;
    hr(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, 0x40, &pp, &device.value), "CreateDevice");
    Owner<ID3DXEffect> effect;
    Owner<ID3DXBuffer> errors;
    const HRESULT compiled = D3DXCreateEffect(device.value, effectSource, UINT(std::strlen(effectSource)),
        nullptr, nullptr, 0, nullptr, &effect.value, &errors.value);
    if (errors.value) std::fprintf(stderr, "%.*s\n", int(errors->GetBufferSize()), static_cast<const char*>(errors->GetBufferPointer()));
    hr(compiled, "D3DXCreateEffect");
    validate(effect.value, "Compiled effect");
    Owner<ID3DXEffect> cached;
    hr(D3DXCreateEffect(device.value, effectSource, UINT(std::strlen(effectSource)),
        nullptr, nullptr, 0, nullptr, &cached.value, nullptr), "D3DXCreateEffect cached");
    validate(cached.value, "Cached effect");
    // Native .cfx envelopes retain source. A tag from Metal must be retargeted before lookup.
    bf2fx::Request foreign;
    check(bool(bf2fx::capture(effectSource, std::strlen(effectSource), {}, 0, std::string(64, 'a'),
        "metal-fixture", nullptr, foreign)), "Capture foreign request");
    const auto tagged = bf2fx::native::tag(foreign);
    Owner<ID3DXEffect> imported;
    hr(D3DXCreateEffect(device.value, tagged.data(), UINT(tagged.size()), nullptr, nullptr, 0, nullptr,
        &imported.value, nullptr), "D3DXCreateEffect foreign tag");
    check(!std::filesystem::exists(std::filesystem::path(std::getenv("BF2_FX_NATIVE_CACHE")) / "entries" /
        bf2fx::cacheKey(foreign)), "Foreign backend identity must not be published");
    validate(imported.value, "Imported effect");
    std::swap(effect.value, imported.value);
    hr(effect->SetTechnique(effect->GetTechniqueByName("Main")), "SetTechnique");
    Owner<IDirect3DTexture9> texture;
    hr(device->CreateTexture(1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture.value, nullptr), "CreateTexture");
    D3DLOCKED_RECT locked{};
    hr(texture->LockRect(0, &locked, nullptr, 0), "Texture LockRect");
    const DWORD white = 0xffffffff;
    std::memcpy(locked.pBits, &white, sizeof(white));
    hr(texture->UnlockRect(0), "Texture UnlockRect");
    hr(effect->SetTexture(effect->GetParameterByName(nullptr, "ColorTexture"), texture.value), "SetTexture");
    const D3DVERTEXELEMENT9 elements[] = {
        {0, 0, D3DDECLTYPE_FLOAT4, 0, D3DDECLUSAGE_POSITION, 0},
        {0, 16, D3DDECLTYPE_FLOAT2, 0, D3DDECLUSAGE_TEXCOORD, 0}, D3DDECL_END()};
    Owner<IDirect3DVertexDeclaration9> declaration;
    hr(device->CreateVertexDeclaration(elements, &declaration.value), "CreateVertexDeclaration");
    hr(device->SetVertexDeclaration(declaration.value), "SetVertexDeclaration");
    hr(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "Cull state");
    hr(device->SetRenderState(D3DRS_ZENABLE, FALSE), "Depth state");
    hr(device->SetRenderState(alphaBlendEnable, TRUE), "Initial blend state");
    const float vertices[][6] = {{-1, 1, 0, 1, 0, 0}, {1, 1, 0, 1, 1, 0}, {-1, -1, 0, 1, 0, 1}, {1, -1, 0, 1, 1, 1}};
    const D3DXVECTOR4 colors[] = {{0.25f, 0.5f, 0.75f, 1}, {0.75f, 0.125f, 0.25f, 1}};
    for (const auto& color : colors) {
        hr(effect->SetVector(effect->GetParameterByName(nullptr, "Tint"), &color), "SetVector");
        hr(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0), "Clear");
        hr(device->BeginScene(), "BeginScene");
        UINT passes = 0;
        hr(effect->Begin(&passes, 0), "Effect Begin");
        check(passes == 1, "One effect pass");
        hr(effect->BeginPass(0), "Effect BeginPass");
        hr(device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(vertices[0])), "DrawPrimitiveUP");
        hr(effect->EndPass(), "Effect EndPass");
        hr(effect->End(), "Effect End");
        hr(device->EndScene(), "EndScene");
        DWORD blend = 0;
        hr(device->GetRenderState(alphaBlendEnable, &blend), "Read restored state");
        check(blend == TRUE, "Effect restores caller blend state");
        Owner<IDirect3DSurface9> back, copy;
        hr(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &back.value), "GetBackBuffer");
        hr(device->CreateOffscreenPlainSurface(64, 64, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &copy.value, nullptr), "Readback surface");
        hr(device->GetRenderTargetData(back.value, copy.value), "Readback");
        hr(copy->LockRect(&locked, nullptr, D3DLOCK_READONLY), "Readback LockRect");
        std::array<unsigned char, 3> pixel{};
        std::memcpy(pixel.data(), static_cast<unsigned char*>(locked.pBits) + 32 * locked.Pitch + 32 * 4, 3);
        hr(copy->UnlockRect(), "Readback UnlockRect");
        const float expected[] = {color.z, color.y, color.x};
        for (unsigned channel = 0; channel < 3; ++channel)
            check(std::abs(int(pixel[channel]) - int(std::lround(expected[channel] * 255))) <= 1, "Rendered effect pixel matches texture times Tint");
        std::printf("Effect pixel BGR: %u %u %u; state restored\n", pixel[0], pixel[1], pixel[2]);
    }
    relativeLoads(device.value);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: %s PRIVATE_CACHE_DIRECTORY\n", argv[0]); return 2; }
    setenv("BF2_FX_NATIVE_CACHE", argv[1], 1);
    setenv("DXVK_WSI_DRIVER", "SDL3", 1);
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    SDL_Window* window = SDL_CreateWindow("OpenBF2 effect device check", 64, 64, SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN);
    int result = 0;
    try { check(window != nullptr, SDL_GetError()); run(window); }
    catch (const std::exception& error) { std::fprintf(stderr, "FAIL: %s\n", error.what()); result = 1; }
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
