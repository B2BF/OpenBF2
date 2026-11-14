// [CHOSEN, not recovered] Native effect builder for the macOS port: preprocessed D3DX effect
// source -> the s2 effect Description (EffectData.h) that the runtime and the Metal facade
// consume. Shader compilation and bytecode reflection are supplied by a backend (vkd3d-shader
// and MojoShader in the port), so this library stays free of both.
#pragma once
#include "EffectData.h"
#include "FxParser.h"
#include <string>
#include <vector>

namespace bf2fx {

struct ShaderJob {
    uint32_t stage = 0;     // VertexShader (16) or PixelShader (15), as in EffectData.h.
    std::string profile;    // vs_1_1 ... ps_3_0.
    std::string function;   // HLSL function; empty for an asm block.
    std::string entry;      // Entry point handed to the compiler.
    std::string wrapper;    // Generated entry wrapper (empty when the function is compiled directly).
    std::string asmText;    // asm { } body.
    std::string key;        // Identity: profile, function and argument text.
    int line = 0;
};

// Sampler parameter type as seen by MojoShader's sampler map: register index -> 2D/cube/volume.
struct SamplerSlot {
    std::string name;
    uint32_t type = 0; // MojoShader sampler type: 0 2D, 1 cube, 2 volume.
};

class ShaderBackend {
public:
    virtual ~ShaderBackend() = default;
    // HLSL jobs: compile the shader text (techniques blanked) plus job.wrapper at job.entry.
    // Asm jobs: assemble job.asmText. Returns false with a diagnostic.
    virtual bool compile(const ShaderJob& job, const std::string& shaderText, Bytes& bytecode, std::string& error) = 0;
    // Fills major/minor, bindings (CTAB), msl, uniforms, samplers, attributes, outputs, constants.
    virtual bool reflect(Shader& shader, const std::vector<SamplerSlot>& samplers, std::string& error) = 0;
};

// Text handed to the shader compiler for one stage: effect-only constructs blanked, profile
// register reservations kept for their stage only, functions whose names collide with
// compiler keywords renamed (see shaderJob).
std::string compilerText(const src::Effect& effect, const std::string& text, uint32_t stage);

// Shader job for a VertexShader/PixelShader value (compile expression or asm block).
bool shaderJob(const src::Effect& effect, const std::string& text, const src::Expr& value, uint32_t stage,
               ShaderJob& job, std::string& error);

struct BuildReport {
    // When set, a shader that fails to compile leaves an empty record and is listed here
    // instead of failing the build (structure comparisons, coverage measurement).
    bool tolerateShaderFailures = false;
    // Reflection-only builds (D3DXCreateEffectCompiler on `.mfx` metadata): no techniques.
    bool allowNoTechniques = false;
    // [CHOSEN, not recovered] Set when D3DX accepts the request (refusal table): a vs_1_1/vs_2_x
    // shader the native SM1/2 lowering cannot produce (E5017 not-implemented, E9015 register or
    // encoding limits, both irrelevant on Metal) is compiled internally as vs_3_0. Observables
    // keep the requested profile: the CTAB maps constants by name, stages link by semantic, the
    // record's major/minor stay at the requested version (so oD0/oD1 are still clamped as below
    // vs_3_0) and D3DX's own result for the same source is the same arithmetic.
    bool higherVertexProfile = false;
    uint32_t higherVertexProfileUses = 0;
    std::vector<std::string> shaderFailures;
    uint32_t shaders = 0, compiled = 0;
};

// Builds the full Description. Fails with a diagnostic on the first unsupported construct or
// shader compile failure (the caller treats this exactly like a refused D3DX compile).
bool build(const std::string& text, ShaderBackend& backend, Description& out, std::string& error,
           BuildReport* report = nullptr);

} // namespace bf2fx
