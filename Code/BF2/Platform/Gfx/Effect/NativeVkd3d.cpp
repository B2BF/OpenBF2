// [CHOSEN, not recovered] vkd3d-shader (LGPL-2.1+) glue for native effect compilation.
#if defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))
#include "NativeVkd3d.h"
#include <vkd3d_shader.h>
#include <cstring>
#include <map>
#include <vector>

namespace bf2fx { namespace native {
namespace {
struct IncludeContext {
    const bf2fx::Request* request = nullptr;
    std::map<const char*, uint32_t> ordinal; // data pointer -> include ordinal (root 0).
    std::vector<std::string> buffers;        // NUL-terminated copies, one per include.
};
int openInclude(const char* filename, bool local, const char* parent, void* context, vkd3d_shader_code* out) {
    auto* ctx = static_cast<IncludeContext*>(context);
    uint32_t parentOrdinal = 0;
    if (parent) {
        auto it = ctx->ordinal.find(parent);
        if (it == ctx->ordinal.end()) return VKD3D_ERROR;
        parentOrdinal = it->second;
    }
    const auto& incs = ctx->request->includes;
    for (size_t i = 0; i < incs.size(); ++i) {
        const auto& inc = incs[i];
        if (inc.parent != parentOrdinal || inc.kind != (local ? 0u : 1u) || inc.name != filename) continue;
        if (!inc.found) return VKD3D_ERROR;
        const std::string& b = ctx->buffers[i];
        out->code = b.data();
        out->size = b.size();
        return VKD3D_OK;
    }
    return VKD3D_ERROR;
}
void closeInclude(const vkd3d_shader_code*, void*) {}
std::string take(char* messages) {
    std::string s = messages ? messages : "";
    vkd3d_shader_free_messages(messages);
    return s;
}
} // namespace

bool preprocessSource(const bf2fx::Request& request, std::string& out, std::string& log) {
    IncludeContext ctx;
    ctx.request = &request;
    std::string root(request.source.begin(), request.source.end());
    while (!root.empty() && root.back() == '\0') root.pop_back(); // .mfx roots carry a NUL.
    ctx.buffers.resize(request.includes.size());
    for (size_t i = 0; i < request.includes.size(); ++i) {
        const auto& b = request.includes[i].bytes;
        ctx.buffers[i].assign(b.begin(), b.end());
    }
    ctx.ordinal[root.data()] = 0;
    for (size_t i = 0; i < ctx.buffers.size(); ++i) ctx.ordinal[ctx.buffers[i].data()] = uint32_t(i + 1);
    std::vector<vkd3d_shader_macro> macros;
    for (const auto& d : request.defines) macros.push_back({d.name.c_str(), d.value.c_str()});
    vkd3d_shader_preprocess_info pp{};
    pp.type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO;
    pp.macros = macros.data();
    pp.macro_count = unsigned(macros.size());
    pp.pfn_open_include = openInclude;
    pp.pfn_close_include = closeInclude;
    pp.include_context = &ctx;
    vkd3d_shader_compile_info info{};
    info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
    info.next = &pp;
    info.source.code = root.data();
    info.source.size = root.size();
    info.source_type = VKD3D_SHADER_SOURCE_HLSL;
    info.target_type = VKD3D_SHADER_TARGET_NONE;
    info.log_level = VKD3D_SHADER_LOG_WARNING;
    info.source_name = "root.fx";
    vkd3d_shader_code result{};
    char* messages = nullptr;
    int rc = vkd3d_shader_preprocess(&info, &result, &messages);
    log = take(messages);
    if (rc < 0) return false;
    out.assign(static_cast<const char*>(result.code), result.size);
    vkd3d_shader_free_shader_code(&result);
    return true;
}

int compileShader(const std::string& source, const std::string& entry, const std::string& profile,
                  bf2fx::Bytes& out, std::string& log) {
    vkd3d_shader_compile_option options[] = {
        {VKD3D_SHADER_COMPILE_OPTION_BACKWARD_COMPATIBILITY,
         VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_MAP_SEMANTIC_NAMES |
             VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_ALLOW_HALF_GLOBALS},
    };
    vkd3d_shader_hlsl_source_info hlsl{};
    hlsl.type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO;
    hlsl.entry_point = entry.c_str();
    hlsl.profile = profile.c_str();
    vkd3d_shader_compile_info info{};
    info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
    info.next = &hlsl;
    info.source.code = source.data();
    info.source.size = source.size();
    info.source_type = VKD3D_SHADER_SOURCE_HLSL;
    info.target_type = VKD3D_SHADER_TARGET_D3D_BYTECODE;
    info.options = options;
    info.option_count = sizeof(options) / sizeof(options[0]);
    info.log_level = VKD3D_SHADER_LOG_WARNING;
    info.source_name = "effect.fx";
    vkd3d_shader_code result{};
    char* messages = nullptr;
    int rc = vkd3d_shader_compile(&info, &result, &messages);
    log = take(messages);
    if (rc < 0) return rc;
    auto* p = static_cast<const uint8_t*>(result.code);
    out.assign(p, p + result.size);
    vkd3d_shader_free_shader_code(&result);
    return 0;
}

bool disassemble(const bf2fx::Bytes& code, std::string& out) {
    vkd3d_shader_compile_info info{};
    info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO;
    info.source.code = code.data();
    info.source.size = code.size();
    info.source_type = VKD3D_SHADER_SOURCE_D3D_BYTECODE;
    info.target_type = VKD3D_SHADER_TARGET_D3D_ASM;
    info.log_level = VKD3D_SHADER_LOG_NONE;
    vkd3d_shader_code result{};
    char* messages = nullptr;
    int rc = vkd3d_shader_compile(&info, &result, &messages);
    take(messages);
    if (rc < 0) return false;
    out.assign(static_cast<const char*>(result.code), result.size);
    vkd3d_shader_free_shader_code(&result);
    return true;
}
} // namespace native
} // namespace bf2fx
#endif
