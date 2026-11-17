// [TEMP] [CHOSEN, not recovered] Compiler/constant fixtures for the Linux port, issue #36.
// These exercise real effect libraries without starting a window, server or game session.
#include "Native.h"
#include "NativeMojo.h"
#include "NativeVkd3d.h"
#include "relative_load.h"
#include "D3DAsm.h"
#include "Runtime.h"
#include "Wire.h"
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

using namespace bf2fx;

namespace {
void check(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}

struct Observer final : Callbacks {
    unsigned shaders = 0, constantsCalls = 0, samplers = 0;
    void state(uint32_t, uint32_t, const Value&) override {}
    void shader(uint32_t, const Shader* shader) override {
        check(shader && !shader->bytecode.empty() && shader->msl.empty(), "Pass must select bytecode without MSL");
        ++shaders;
    }
    void constants(const Shader&, const Registers&) override { ++constantsCalls; }
    void sampler(uint32_t, uint32_t, const Value&, const std::shared_ptr<void>&) override { ++samplers; }
    bool validate(const Pass&, const Description&) override { return true; }
};

const char source[] = R"fx(
float4 Tint = float4(1, 1, 1, 1);
float4x4 Transform;
texture ColorTexture;
sampler2D ColorSampler = sampler_state {
    Texture = <ColorTexture>;
    MinFilter = LINEAR;
    MagFilter = LINEAR;
};
struct Vertex { float4 position : POSITION0; float2 uv : TEXCOORD0; };
Vertex vertexMain(Vertex input) {
    Vertex result;
    result.position = mul(input.position, Transform);
    result.uv = input.uv;
    return result;
}
float4 pixelMain(float2 uv : TEXCOORD0) : COLOR0 {
    return tex2D(ColorSampler, uv) * Tint;
}
technique MenuFixture {
    pass Draw {
        AlphaBlendEnable = TRUE;
        SrcBlend = SRCALPHA;
        DestBlend = INVSRCALPHA;
        VertexShader = compile vs_2_0 vertexMain();
        PixelShader = compile ps_2_0 pixelMain();
    }
}
)fx";

void run(const std::string& cache) {
    Bytes relativeCode;
    std::string relativeLog;
    check(native::compileShader(relativeLoadSource, "relativeVertex", "vs_1_1", relativeCode, relativeLog) == 0,
          "Repeated relative matrix loads compile at requested profile: " + relativeLog);
    check(relativeCode.size() >= 4 && relativeCode[0] == 1 && relativeCode[1] == 1,
          "Relative-load regression keeps vs_1_1 bytecode");
    Shader relativeShader;
    relativeShader.bytecode = relativeCode;
    relativeShader.entry = "relativeVertex";
    check(native::reflect(relativeShader, {}, relativeLog), "Relative-load reflection: " + relativeLog);
    check(sha256({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "Empty SHA-256");
    check(sha256(Bytes{'a', 'b', 'c'}) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc");
    Request request;
    auto result = capture(source, std::strlen(source), {}, 0, native::identityHash(), native::identity(), nullptr, request);
    check(bool(result), result.detail);
    Entry entry = native::compile(request);
    check(entry.kind == EffectEntry && !entry.compileResult && !entry.parseResult, "Compile fixture: " + entry.log);
    check(entry.description.shaders.size() == 2, "Vertex and pixel shader records");
    Runtime runtime(entry.description);
    check(runtime.valid(), "Runtime accepts compiled description");
    const std::array<float, 4> tint{{0.125f, 0.25f, 0.5f, 0.75f}};
    check(runtime.setVector(runtime.getParameterByName("Tint"), tint), "Set Tint");
    const float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    check(runtime.setMatrix(runtime.getParameterByName("Transform"), matrix), "Set Transform");
    bool sawTint = false, sawTransform = false, sawSampler = false;
    for (const auto& shader : entry.description.shaders) {
        check(!shader.bytecode.empty() && shader.bytecode.size() % 4 == 0 && shader.msl.empty(), "Only D3D9 bytecode");
        auto registers = runtime.mapConstants(shader);
        for (const auto& binding : shader.bindings) {
            if (binding.type.name == "Tint") {
                check(binding.set == 2 && binding.count == 1, "Float constant register binding");
                for (unsigned i = 0; i < 4; ++i)
                    check(std::abs(registers.f.at(binding.index * 4 + i) - tint[i]) < 1e-6f, "Tint mapped to D3D9 register");
                sawTint = true;
            }
            if (binding.type.name == "Transform") {
                check(binding.set == 2 && binding.count == 4, "Matrix register binding");
                for (unsigned i = 0; i < 16; ++i)
                    check(std::abs(registers.f.at(binding.index * 4 + i) - matrix[i]) < 1e-6f, "Matrix mapped to D3D9 registers");
                sawTransform = true;
            }
        }
        sawSampler = sawSampler || !shader.samplers.empty();
        Shader reflected = shader;
        std::string error;
        check(native::reflect(reflected, {}, error), error);
        check(reflected.bytecode == shader.bytecode && reflected.msl.empty(), "Reflection preserves bytecode");
    }
    check(sawTint && sawTransform && sawSampler, "CTAB and sampler reflection retained");
    Observer observer;
    check(runtime.applyPass(0, 0, observer), "Apply compiled pass");
    check(observer.shaders == 2 && observer.constantsCalls == 2 && observer.samplers == 1, "Pass dispatches both shaders, constants and sampler");
    Shader literal;
    literal.entry = "literalFixture";
    std::string error;
    check(assemble("ps_1_1\ndef c0, 0.25, 0.5, 0.75, 1.0\nmov r0, c0\n", literal.bytecode, error), error);
    check(native::reflect(literal, {}, error), error);
    auto registers = runtime.mapConstants(literal);
    check(registers.f.size() == 4 && registers.f[0] == 0.25f && registers.f[3] == 1.0f, "DEF literal constants retained");
    Shader malformed;
    malformed.bytecode = Bytes(4, 0);
    check(!native::reflect(malformed, {}, error) && !error.empty(), "Malformed bytecode refused");
    Request untagged;
    const auto tagged = native::tag(request);
    check(native::untag(tagged.data(), tagged.size(), untagged) && cacheKey(untagged) == cacheKey(request), "Native request tag roundtrip");
    result = publish(cache, request, entry);
    check(bool(result), "Publish: " + result.detail);
    Entry cached;
    result = load(cache, request, cached);
    check(bool(result) && cached.description.shaders.size() == 2, "Load bytecode cache");
    for (size_t i = 0; i < cached.description.shaders.size(); ++i)
        check(cached.description.shaders[i].bytecode == entry.description.shaders[i].bytecode && cached.description.shaders[i].msl.empty(), "Cache preserves bytecode without MSL");
    Request otherBackend = request;
    otherBackend.translator += "-different-backend";
    check(cacheKey(otherBackend) != cacheKey(request), "Backend identities cannot share cache keys");
    std::cout << "Linux effect compilation, CTAB/constants, pass application, rejection and cache checks passed\n";
}
} // namespace

int main(int argc, char** argv) {
    try {
        check(argc == 2, "usage: bf2_linux_fx_compiler SCRATCH");
        std::filesystem::create_directories(argv[1]);
        std::string pattern = std::string(argv[1]) + "/compiler-XXXXXX";
        std::vector<char> path(pattern.begin(), pattern.end());
        path.push_back(0);
        check(mkdtemp(path.data()) != nullptr, "Create isolated scratch directory");
        run(path.data());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
