// [CHOSEN, not recovered] MojoShader (zlib, Ryan C. Gordon) reflection and MSL translation.
// Mirrors the shader half of tools/macos/d3dxfx/cache-tool.cpp so that native and D3DX
// bytecode produce comparable records.
#if defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))
#include "NativeMojo.h"
#include <mojoshader.h>
#include <algorithm>
#include <cstring>
#include <memory>

namespace bf2fx { namespace native {

namespace {
std::string safe(const char* s) { return s ? s : ""; }
Type type(const MOJOSHADER_symbolTypeInfo& t, const char* name = "") {
    Type v; v.name = safe(name); v.type = t.parameter_type; v.cls = t.parameter_class;
    v.rows = t.rows; v.columns = t.columns; v.elements = t.elements;
    for (unsigned i = 0; i < t.member_count; ++i) v.members.push_back(type(t.members[i].info, t.members[i].name));
    return v;
}
Binding binding(const MOJOSHADER_symbol& s) { return {type(s.info, s.name), uint32_t(s.register_set), s.register_index, s.register_count}; }
Program program(const MOJOSHADER_preshader* p) {
    Program v; if (!p) return v; v.temps = p->temp_count; v.inputs = p->register_count;
    for (unsigned i = 0; i < p->literal_count; ++i) { uint64_t w; std::memcpy(&w, p->literals + i, 8); v.literals.push_back(w); }
    for (unsigned i = 0; i < p->symbol_count; ++i) v.bindings.push_back(binding(p->symbols[i]));
    for (unsigned i = 0; i < p->instruction_count; ++i) {
        const auto& s = p->instructions[i]; Instruction inst; inst.opcode = s.opcode; inst.elements = s.element_count;
        for (unsigned j = 0; j < s.operand_count; ++j) {
            const auto& o = s.operands[j]; Operand op; op.kind = o.type; op.index = o.index;
            if (o.array_register_count) op.arrays.assign(o.array_registers, o.array_registers + o.array_register_count);
            inst.operands.push_back(std::move(op));
        }
        v.instructions.push_back(std::move(inst));
    }
    return v;
}
} // namespace

bool reflect(Shader& s, const std::vector<SamplerSlot>&, std::string& error) {
    using PD = std::unique_ptr<const MOJOSHADER_parseData, decltype(&MOJOSHADER_freeParseData)>;
    PD pd(MOJOSHADER_parse(MOJOSHADER_PROFILE_BYTECODE, s.entry.c_str(), s.bytecode.data(), unsigned(s.bytecode.size()),
                           nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr), MOJOSHADER_freeParseData);
    if (!pd || pd->error_count) { error = pd ? safe(pd->errors[0].error) : "parse failed"; return false; }
    s.major = pd->major_ver; s.minor = pd->minor_ver; s.preshader = program(pd->preshader);
    s.bindings.clear();
    for (int j = 0; j < pd->symbol_count; ++j) s.bindings.push_back(binding(pd->symbols[j]));
#if defined(__APPLE__)
    PD msl(MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL, s.entry.c_str(), s.bytecode.data(), unsigned(s.bytecode.size()),
                            nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr), MOJOSHADER_freeParseData);
    if (!msl || msl->error_count) { error = msl ? safe(msl->errors[0].error) : "translate failed"; return false; }
    if (!msl->output || msl->output_len <= 0) { error = "Empty MSL"; return false; }
    s.msl.assign(msl->output, msl->output_len);
    const auto* reflected = msl.get();
#else
    // [CHOSEN, not recovered] DXVK consumes the original bytecode. Metal translation must
    // neither change its metadata nor reject shaders on this backend (kr4uzi/OpenBF2#36).
    s.msl.clear();
    const auto* reflected = pd.get();
#endif
    s.uniforms.clear(); s.samplers.clear(); s.attributes.clear(); s.outputs.clear(); s.constants.clear();
    for (int j = 0; j < reflected->uniform_count; ++j) { const auto& u = reflected->uniforms[j]; s.uniforms.push_back({uint32_t(u.type), uint32_t(u.index), uint32_t(std::max(1, u.array_count)), uint32_t(u.constant), safe(u.name)}); }
    for (int j = 0; j < reflected->sampler_count; ++j) { const auto& u = reflected->samplers[j]; s.samplers.push_back({uint32_t(u.type), uint32_t(u.index), 1, uint32_t(u.texbem), safe(u.name)}); }
    for (int j = 0; j < reflected->attribute_count; ++j) { const auto& a = reflected->attributes[j]; s.attributes.push_back({uint32_t(a.usage), uint32_t(a.index), 0, safe(a.name)}); }
    for (int j = 0; j < reflected->output_count; ++j) { const auto& a = reflected->outputs[j]; s.outputs.push_back({uint32_t(a.usage), uint32_t(a.index), 0, safe(a.name)}); }
    for (int j = 0; j < pd->constant_count; ++j) {
        const auto& c = pd->constants[j]; State v; v.id = c.index;
        v.value.type.type = c.type == MOJOSHADER_UNIFORM_FLOAT ? Float : c.type == MOJOSHADER_UNIFORM_INT ? Int : Bool;
        uint32_t w[4]{}; std::memcpy(w, &c.value, c.type == MOJOSHADER_UNIFORM_BOOL ? 4 : 16); v.value.words.assign(w, w + 4);
        s.constants.push_back(v);
    }
    return true;
}
} // namespace native
} // namespace bf2fx
#endif
