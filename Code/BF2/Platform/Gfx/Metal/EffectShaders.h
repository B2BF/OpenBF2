// [CHOSEN, not recovered] Owner-requested macOS port.
// Handoff of offline-translated shader records from the D3DX effect facade (Effect.cpp)
// to the Metal device's CreateVertexShader/CreatePixelShader. No D3D or Objective-C types.
#ifndef BF2_METAL_EFFECTSHADERS_H
#define BF2_METAL_EFFECTSHADERS_H
#include <cstddef>
#include <cstdint>
#include <memory>
namespace bf2fx { struct Shader; }
namespace bf2metal {
// D3DX creates its shader objects through the public device interface. While an offer is alive,
// the next Create*Shader call on this thread whose bytecode equals the record's bytecode adopts
// the record's MSL and reflection. Bytecode alone does not identify MSL: MojoShader's output also
// depends on the effect's sampler types, so a process-wide bytecode registry is not used.
class ShaderOffer {
public:
    explicit ShaderOffer(std::shared_ptr<const bf2fx::Shader> record);
    ~ShaderOffer();
    ShaderOffer(const ShaderOffer&) = delete;
    ShaderOffer& operator=(const ShaderOffer&) = delete;
    // Returns the offered record when its bytecode equals the caller's token stream.
    static std::shared_ptr<const bf2fx::Shader> take(const std::uint32_t* tokens, std::size_t bytes);
};
// Run-time translation of bytecode that no effect offered (the renderer's D3DXAssembleShader
// probes). Reflects and translates the token stream with MojoShader when the client is built with
// native effect compilation; returns null without it or when translation fails (the reason is
// written to stderr), and the device then keeps its named stop.
std::shared_ptr<const bf2fx::Shader> translateRuntimeShader(const std::uint32_t* tokens, std::size_t bytes,
                                                            bool vertex);
// Length in bytes of a D3D9 shader token stream through its end token, or 0 when malformed.
std::size_t shaderLength(const std::uint32_t* tokens);
// [CHOSEN, not recovered] Creates the device shader objects of natively compiled effects whose
// background build has finished but which no pass has used yet, a few per call (the device calls
// it at every Present). Without it the shaders of an effect that is first drawn during play
// (particles, trails) would only exist, and start to compile for Metal, at that draw.
void createFinishedEffectShaders();
// [CHOSEN, not recovered] Debug aid, off by default: BF2_METAL_DRAW_TRACE=1 counts draws per
// effect technique and outcome and prints the table every 600 presents. The effect facade names
// the technique of its active pass; draws outside an effect pass count under "(no effect)".
bool drawTraceEnabled();
void setDrawTraceLabel(const char* label);
const char* drawTraceLabel();
}
#endif
