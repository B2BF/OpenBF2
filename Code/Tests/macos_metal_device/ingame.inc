// [TEMP] [CHOSEN, not recovered] In-game draw-state checks:
// fog after shaders, ps_1_x samplers bound to cube textures, and occlusion queries with draws.
// The shaders are MojoShader-shaped MSL records offered to Create*Shader like the effect cache's.
#include "../../BF2/Platform/Gfx/Metal/EffectShaders.h"
#include "../../BF2/Platform/Gfx/Effect/EffectData.h"
#include <type_traits>

namespace ingame {
struct Vertex { float x, y, z, w, u, v, s, t; };
std::shared_ptr<bf2fx::Shader> record(bool vertex, unsigned major, unsigned minor, const std::string& msl)
{
    auto r = std::make_shared<bf2fx::Shader>();
    r->stage = vertex ? 16 : 15; r->major = major; r->minor = minor; r->msl = msl;
    const std::uint32_t tokens[2] = {(vertex ? 0xfffe0000u : 0xffff0000u) | (major << 8) | minor, 0x0000ffffu};
    r->bytecode.assign(reinterpret_cast<const std::uint8_t*>(tokens), reinterpret_cast<const std::uint8_t*>(tokens) + 8);
    if (vertex) { r->attributes.push_back({0, 0, 0, "v0"}); r->attributes.push_back({5, 0, 0, "v1"}); }
    return r;
}
template<class I> I* create(IDirect3DDevice9* d, std::shared_ptr<bf2fx::Shader> r)
{
    bf2metal::ShaderOffer offer(r);
    const DWORD* tokens = reinterpret_cast<const DWORD*>(r->bytecode.data());
    I* out = nullptr;
    if constexpr (std::is_same<I, IDirect3DVertexShader9>::value) { if (d->CreateVertexShader(tokens, &out) != D3D_OK) return nullptr; }
    else { if (d->CreatePixelShader(tokens, &out) != D3D_OK) return nullptr; }
    return out;
}
// oPos = (v0.xy, z = v0.w, 1); oFog = v0.z (when written); oT0 = v1.
std::string vertexMsl(bool fog, bool color = false)
{
    if (color) // oD0 = (2, -1, 0.5, 1): D3D9 clamps vs_1_1 colour outputs to [0,1].
        return "#include <metal_math>\nusing namespace metal;\n"
            "struct VS_Input\n{\n\tfloat4 v0 [[attribute(0)]];\n\tfloat4 v1 [[attribute(1)]];\n};\n"
            "struct VS_Output\n{\n\tfloat4 oPos [[position]];\n\tfloat4 oD0 [[user(color0)]];\n};\n\n"
            "vertex VS_Output VS (\n\tVS_Input input [[stage_in]]\n) {\n\tVS_Output output;\n"
            "\toutput.oPos = float4(input.v0.xy, input.v0.w, 1.0);\n\toutput.oD0 = float4(2.0, -1.0, 0.5, 1.0);\n"
            "\treturn output;\n}\n";
    return std::string("#include <metal_math>\nusing namespace metal;\n"
        "struct VS_Input\n{\n\tfloat4 v0 [[attribute(0)]];\n\tfloat4 v1 [[attribute(1)]];\n};\n"
        "struct VS_Output\n{\n\tfloat4 oPos [[position]];\n") + (fog ? "\tfloat4 oFog [[user(fog)]];\n" : "") +
        "\tfloat4 oT0 [[user(texcoord0)]];\n};\n\n"
        "vertex VS_Output VS (\n\tVS_Input input [[stage_in]]\n) {\n\tVS_Output output;\n"
        "\toutput.oPos = float4(input.v0.xy, input.v0.w, 1.0);\n" + (fog ? "\toutput.oFog = float4(input.v0.z);\n" : "") +
        "\toutput.oT0 = input.v1;\n\treturn output;\n}\n";
}
// ps_1_4-shaped: colour 0 = blue (no inputs).
const char* bluePixel =
    "#include <metal_common>\nusing namespace metal;\n"
    "struct PS_Output\n{\n\tfloat4 oC0 [[color(0)]];\n};\n\n"
    "fragment PS_Output PS (\n) {\n\tPS_Output output;\n\toutput.oC0 = float4(0.0, 0.0, 1.0, 1.0);\n\treturn output;\n}\n";
// ps_1_4-shaped: colour 0 = v0 * 0.5 (shows whether v0 arrived clamped).
const char* halfColorPixel =
    "#include <metal_common>\nusing namespace metal;\n"
    "struct PS_Input\n{\n\tfloat4 v0 [[user(color0)]];\n};\n"
    "struct PS_Output\n{\n\tfloat4 oC0 [[color(0)]];\n};\n\n"
    "fragment PS_Output PS (\n\tPS_Input input [[stage_in]]\n) {\n\tPS_Output output;\n"
    "\toutput.oC0 = input.v0 * 0.5;\n\treturn output;\n}\n";
// ps_1_4 `texld r0, t0` on stage 2, exactly as MojoShader writes it without a sampler map.
const char* samplePixel =
    "#include <metal_texture>\nusing namespace metal;\n"
    "struct PS_Input\n{\n\tfloat4 t0 [[user(texcoord0)]];\n};\n"
    "struct PS_Output\n{\n\tfloat4 oC0 [[color(0)]];\n};\n\n"
    "fragment PS_Output PS (\n\ttexture2d<float> s2_texture [[texture(2)]],\n\tsampler s2 [[sampler(2)]],\n"
    "\tPS_Input input [[stage_in]]\n) {\n\tPS_Output output;\n\tfloat4 r0;\n\t#define t0 input.t0\n"
    "\tr0 = s2_texture.sample(s2, t0.xy);\n\toutput.oC0 = r0;\n\t#undef t0\n\treturn output;\n}\n";
void quad(Vertex (&q)[4], float x0, float y0, float x1, float y1, float fog, float z, float s, float t, float u)
{
    const float xs[4] = {x0, x1, x0, x1}, ys[4] = {y0, y0, y1, y1};
    for (int i = 0; i < 4; ++i) q[i] = {xs[i] / 32 - 1, 1 - ys[i] / 32, fog, z, s, t, u, 1};
}
DWORD bits(float f) { DWORD v; std::memcpy(&v, &f, 4); return v; }
// D3DRENDERSTATETYPE / D3DFOGMODE values outside the production subset.
const auto FOGENABLE = D3DRENDERSTATETYPE(28), FOGCOLOR = D3DRENDERSTATETYPE(34), FOGTABLEMODE = D3DRENDERSTATETYPE(35),
    FOGSTART = D3DRENDERSTATETYPE(36), FOGEND = D3DRENDERSTATETYPE(37), FOGDENSITY = D3DRENDERSTATETYPE(38),
    FOGVERTEXMODE = D3DRENDERSTATETYPE(140), COLORWRITEENABLE = D3DRENDERSTATETYPE(168);
const DWORD FOG_NONE = 0, FOG_EXP = 1, FOG_LINEAR = 3;
} // namespace ingame

int ingameChecks(IDirect3DDevice9* d)
{
    using namespace ingame;
    IDirect3DSurface9 *target = nullptr, *other = nullptr, *oldTarget = nullptr, *depth = nullptr;
    CHECK(d->GetRenderTarget(0, &oldTarget) == D3D_OK && d->GetDepthStencilSurface(&depth) == D3D_OK);
    CHECK(d->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &target, nullptr) == D3D_OK);
    CHECK(d->CreateRenderTarget(64, 64, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &other, nullptr) == D3D_OK);
    const D3DVERTEXELEMENT9 elements[] = {{0, 0, D3DDECLTYPE(3), 0, 0, 0}, {0, 16, D3DDECLTYPE(3), 0, 5, 0}, D3DDECL_END()};
    IDirect3DVertexDeclaration9* decl = nullptr; CHECK(d->CreateVertexDeclaration(elements, &decl) == D3D_OK);
    auto* vsFog = create<IDirect3DVertexShader9>(d, record(true, 1, 1, vertexMsl(true)));
    auto* vsNoFog = create<IDirect3DVertexShader9>(d, record(true, 1, 1, vertexMsl(false)));
    auto* vsColor = create<IDirect3DVertexShader9>(d, record(true, 1, 1, vertexMsl(false, true)));
    auto* blue = create<IDirect3DPixelShader9>(d, record(false, 1, 4, bluePixel));
    auto* halfColor = create<IDirect3DPixelShader9>(d, record(false, 1, 4, halfColorPixel));
    auto sampleRecord = record(false, 1, 4, samplePixel); sampleRecord->samplers.push_back({0, 2, 1, 0, "s2"});
    auto* sample = create<IDirect3DPixelShader9>(d, sampleRecord);
    auto blue20Record = record(false, 2, 0, bluePixel);
    auto* blue20 = create<IDirect3DPixelShader9>(d, blue20Record);
    CHECK(vsFog && vsNoFog && vsColor && halfColor && blue && sample && blue20);
    CHECK(d->SetRenderTarget(0, target) == D3D_OK && d->SetVertexDeclaration(decl) == D3D_OK);
    const D3DVIEWPORT9 vp = {0, 0, 64, 64, 0, 1}; CHECK(d->SetViewport(&vp) == D3D_OK);
    CHECK(d->SetRenderState(D3DRS_ZENABLE, FALSE) == D3D_OK && d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE) == D3D_OK);
    CHECK(d->BeginScene() == D3D_OK);
    std::vector<std::uint8_t> px; Vertex q[4];
    auto draw = [&](IDirect3DVertexShader9* vs, IDirect3DPixelShader9* ps, float x0, float y0, float x1, float y1,
                    float fog = 1, float z = 0.5f, float s = 0, float t = 0, float u = 0) {
        quad(q, x0, y0, x1, y1, fog, z, s, t, u);
        return d->SetVertexShader(vs) == D3D_OK && d->SetPixelShader(ps) == D3D_OK &&
            d->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, q, sizeof(Vertex)) == D3D_OK;
    };

    // 1. Fog (D3D9 with a vertex shader): FOGTABLEMODE NONE blends with oFog, rgb only:
    //    rgb = mix(FOGCOLOR, colour, saturate(oFog)). oFog 0.25 over blue with red fog -> 191,0,64.
    CHECK(d->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 1, 0) == D3D_OK);
    CHECK(d->SetRenderState(FOGENABLE, TRUE) == D3D_OK && d->SetRenderState(FOGCOLOR, 0x80ff0000) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 0, 16, 16, 0.25f));
    CHECK(draw(vsFog, blue, 16, 0, 32, 16, 2.0f));          // oFog > 1 clamps: no fog
    CHECK(draw(vsNoFog, blue, 32, 0, 48, 16));              // oFog unwritten: factor 0, all fog colour
    CHECK(draw(vsFog, nullptr, 48, 0, 64, 16, 0.25f));      // fixed-function pixel stages: white diffuse
    CHECK(d->SetRenderState(FOGVERTEXMODE, FOG_EXP) == D3D_OK); // ignored with a vertex shader
    CHECK(draw(vsFog, blue20, 0, 16, 16, 32, 0.25f));
    CHECK(d->SetRenderState(FOGVERTEXMODE, FOG_NONE) == D3D_OK);
    // Table fog uses the pixel depth: LINEAR start 0 end 1 at z 0.5 -> f 0.5; EXP density 2 -> e^-1.
    CHECK(d->SetRenderState(FOGTABLEMODE, FOG_LINEAR) == D3D_OK && d->SetRenderState(FOGSTART, bits(0)) == D3D_OK &&
          d->SetRenderState(FOGEND, bits(1)) == D3D_OK);
    CHECK(draw(vsFog, blue, 16, 16, 32, 32, 1.0f, 0.5f));
    CHECK(d->SetRenderState(FOGTABLEMODE, FOG_EXP) == D3D_OK && d->SetRenderState(FOGDENSITY, bits(2)) == D3D_OK);
    CHECK(draw(vsFog, blue, 32, 16, 48, 32, 1.0f, 0.5f));
    CHECK(d->SetRenderState(FOGTABLEMODE, FOG_NONE) == D3D_OK);
    CHECK(d->SetRenderState(FOGENABLE, FALSE) == D3D_OK);
    CHECK(draw(vsFog, blue, 48, 16, 64, 32, 0.25f));        // fog off: blue
    CHECK(g4::readback(d, target, px));
    CHECK(g4::near(px, 8, 8, 191, 0, 64, 255) && g4::near(px, 24, 8, 0, 0, 255, 255) && g4::near(px, 40, 8, 255, 0, 0, 255));
    CHECK(g4::near(px, 56, 8, 255, 64, 64, -1) && g4::near(px, 8, 24, 191, 0, 64, 255));
    CHECK(g4::near(px, 24, 24, 128, 0, 128, 255) && g4::near(px, 40, 24, 161, 0, 94, 255) && g4::near(px, 56, 24, 0, 0, 255, 255));

    // 2. ps_1_x samplers carry no dimension: a cube bound to stage 2 is sampled as a cube with the
    //    3-component coordinate, a 2D texture as 2D, by the same shader object.
    IDirect3DCubeTexture9* cube = nullptr;
    CHECK(d->CreateCubeTexture(1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &cube, nullptr) == D3D_OK);
    const std::uint32_t faces[6] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffff00, 0xff00ffff, 0xffff00ff};
    for (unsigned f = 0; f < 6; ++f) {
        D3DLOCKED_RECT lock = {};
        CHECK(cube->LockRect(static_cast<D3DCUBEMAP_FACES>(f), 0, &lock, nullptr, 0) == D3D_OK);
        std::memcpy(lock.pBits, &faces[f], 4); CHECK(cube->UnlockRect(static_cast<D3DCUBEMAP_FACES>(f), 0) == D3D_OK);
    }
    const std::uint32_t grey = 0xff808080; IDirect3DTexture9* flat = g4::texture(d, 1, 1, &grey);
    CHECK(flat && d->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0) == D3D_OK);
    CHECK(d->SetTexture(2, cube) == D3D_OK);
    CHECK(draw(vsFog, sample, 0, 32, 16, 48, 1, 0.5f, 1, 0.1f, 0.1f));    // +X face
    CHECK(draw(vsFog, sample, 16, 32, 32, 48, 1, 0.5f, 0.1f, -1, 0.1f));  // -Y face
    CHECK(draw(vsFog, sample, 32, 32, 48, 48, 1, 0.5f, 0.1f, 0.1f, -1));  // -Z face
    CHECK(d->SetTexture(2, flat) == D3D_OK);
    CHECK(draw(vsFog, sample, 48, 32, 64, 48, 1, 0.5f, 0.5f, 0.5f, 0));
    CHECK(d->SetTexture(2, nullptr) == D3D_OK);
    CHECK(draw(vsColor, halfColor, 0, 48, 16, 64));                       // v0 = saturate(oD0)
    CHECK(g4::readback(d, target, px));
    CHECK(g4::near(px, 8, 56, 128, 0, 64, 128));
    CHECK(g4::near(px, 8, 40, 255, 0, 0, 255) && g4::near(px, 24, 40, 255, 255, 0, 255) && g4::near(px, 40, 40, 255, 0, 255, 255));
    CHECK(g4::near(px, 56, 40, 128, 128, 128, 255));

    // 3. Occlusion queries count the samples of their draws that pass depth/stencil (D3D9:
    //    Issue BEGIN/END, GetData S_FALSE until the GPU is done, FLUSH submits).
    IDirect3DQuery9 *a = nullptr, *b = nullptr, *c = nullptr;
    CHECK(d->CreateQuery(D3DQUERYTYPE_OCCLUSION, &a) == D3D_OK && d->CreateQuery(D3DQUERYTYPE_OCCLUSION, &b) == D3D_OK &&
          d->CreateQuery(D3DQUERYTYPE_OCCLUSION, &c) == D3D_OK);
    CHECK(d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1, 0) == D3D_OK);
    CHECK(d->SetRenderState(D3DRS_ZENABLE, TRUE) == D3D_OK && d->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 0, 64, 64, 1, 0.25f));                         // occluder at z 0.25, outside any query
    CHECK(a->Issue(2) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 0, 16, 16, 1, 0.1f));                          // 256 visible
    CHECK(d->SetRenderState(COLORWRITEENABLE, 0) == D3D_OK);
    CHECK(draw(vsFog, blue, 16, 0, 24, 8, 1, 0.1f));                          // +64, colour writes off
    CHECK(d->SetRenderState(COLORWRITEENABLE, 15) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 32, 32, 64, 1, 0.5f));                         // hidden behind the occluder: +0
    CHECK(a->Issue(D3DISSUE_END) == D3D_OK);
    DWORD value = 7; CHECK(a->GetData(&value, 4, 0) == S_FALSE && value == 7); // still in the open command buffer
    CHECK(b->Issue(2) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 32, 32, 64, 1, 0.5f));                         // fully occluded
    CHECK(b->Issue(D3DISSUE_END) == D3D_OK);
    // A query spanning two render passes (render-target switch) sums both.
    CHECK(c->Issue(2) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 0, 8, 8, 1, 0.1f));                            // 64
    CHECK(d->SetRenderTarget(0, other) == D3D_OK && d->SetViewport(&vp) == D3D_OK);
    CHECK(d->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1, 0) == D3D_OK);
    CHECK(draw(vsFog, blue, 0, 0, 4, 4, 1, 0.1f));                            // 16
    CHECK(c->Issue(D3DISSUE_END) == D3D_OK);
    CHECK(a->GetData(&value, 4, D3DGETDATA_FLUSH) == D3D_OK && value == 320);
    CHECK(b->GetData(&value, 4, 0) == D3D_OK && value == 0);
    CHECK(c->GetData(&value, 4, 0) == D3D_OK && value == 80);
    // Re-issuing restarts the count; END without BEGIN measures an empty interval.
    CHECK(a->Issue(2) == D3D_OK && draw(vsFog, blue, 0, 0, 2, 2, 1, 0.1f) && a->Issue(D3DISSUE_END) == D3D_OK);
    CHECK(a->GetData(&value, 4, D3DGETDATA_FLUSH) == D3D_OK && value == 4);
    CHECK(b->Issue(D3DISSUE_END) == D3D_OK && b->GetData(&value, 4, 0) == D3D_OK && value == 0);
    CHECK(d->EndScene() == D3D_OK);
    CHECK(d->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW) == D3D_OK && d->SetRenderState(FOGCOLOR, 0) == D3D_OK);

    CHECK(a->Release() == 0 && b->Release() == 0 && c->Release() == 0);
    CHECK(d->SetVertexShader(nullptr) == D3D_OK && d->SetPixelShader(nullptr) == D3D_OK && d->SetVertexDeclaration(nullptr) == D3D_OK);
    CHECK(d->SetRenderTarget(0, oldTarget) == D3D_OK && d->SetDepthStencilSurface(depth) == D3D_OK);
    CHECK(cube->Release() == 0 && flat->Release() == 0 && decl->Release() == 0);
    CHECK(vsFog->Release() == 0 && vsNoFog->Release() == 0 && vsColor->Release() == 0 && halfColor->Release() == 0 && blue->Release() == 0 && sample->Release() == 0 && blue20->Release() == 0);
    CHECK(target->Release() == 0 && other->Release() == 0);
    oldTarget->Release(); depth->Release();
    std::puts("PASS: in-game draw state (fog vertex/table/ffp, ps_1_x cube sampling, occlusion queries with draws)");
    return 0;
}
