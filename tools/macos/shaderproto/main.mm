// [CHOSEN, not recovered] Owner-requested shader pipeline experiment, outside the game.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <vkd3d_shader.h>
#include <mojoshader.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <cctype>

namespace fs = std::filesystem;
static std::string readFile(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot read " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
static void writeFile(const fs::path &path, const void *data, size_t size)
{
    std::ofstream file(path, std::ios::binary);
    file.write(static_cast<const char *>(data), size);
    if (!file) throw std::runtime_error("Cannot write " + path.string());
}
static std::string lower(std::string text)
{
    for (char &c : text) c = std::tolower(static_cast<unsigned char>(c));
    return text;
}
struct Includes { std::map<std::string, fs::path> files; };
// The extracted archive is flat. Resolve its virtual shaders/ prefix and Windows casing.
static int openInclude(const char *filename, bool, const char *, void *context,
                       vkd3d_shader_code *out)
{
    auto &files = static_cast<Includes *>(context)->files;
    std::string name = lower(filename);
    for (char &c : name) if (c == '\\') c = '/';
    if (name.compare(0, 8, "shaders/") == 0) name.erase(0, 8);
    else if (name.compare(0, 17, "mods/bf2/shaders/") == 0) name.erase(0, 17);
    else if (name.compare(0, 19, "mods/xpack/shaders/") == 0) name.erase(0, 19);
    auto it = files.find(name);
    if (it == files.end()) { std::cerr << "Missing include: " << filename << '\n'; return VKD3D_ERROR_INVALID_ARGUMENT; }
    try {
        auto text = readFile(it->second);
        char *copy = static_cast<char *>(std::malloc(text.size() + 1));
        if (!copy) return VKD3D_ERROR_OUT_OF_MEMORY;
        std::memcpy(copy, text.c_str(), text.size() + 1);
        *out = {copy, text.size()};
        return VKD3D_OK;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return VKD3D_ERROR_INVALID_ARGUMENT; }
}
static void closeInclude(const vkd3d_shader_code *code, void *) { std::free(const_cast<void *>(code->code)); }

struct Shader {
    std::string entry;
    std::vector<unsigned char> bytes;
    const MOJOSHADER_parseData *parsed;
};
static void *captureShader(const void *, const char *entry, const unsigned char *bytes,
    unsigned int length, const MOJOSHADER_swizzle *swiz, unsigned int swizCount,
    const MOJOSHADER_samplerMap *samplers, unsigned int samplerCount)
{
    // Parse with BYTECODE first, so effect parsing and Metal translation are separate gates.
    auto *shader = new Shader{entry, {bytes, bytes + length}, nullptr};
    shader->parsed = MOJOSHADER_parse(MOJOSHADER_PROFILE_BYTECODE, entry, bytes, length,
        swiz, swizCount, samplers, samplerCount, nullptr, nullptr, nullptr);
    return shader;
}
static MOJOSHADER_parseData *parseData(void *shader)
{ return const_cast<MOJOSHADER_parseData *>(static_cast<Shader *>(shader)->parsed); }
static void deleteShader(const void *, void *ptr)
{
    auto *shader = static_cast<Shader *>(ptr);
    MOJOSHADER_freeParseData(shader->parsed);
    delete shader;
}
static const char *backendError(const void *) { return "BYTECODE capture failed"; }
static const char *safe(const char *text) { return text ? text : ""; }
static void describeValue(std::ostream &out, const MOJOSHADER_effectValue &value)
{
    out << "name=" << safe(value.name) << " semantic=" << safe(value.semantic)
        << " type=" << value.type.parameter_type << " class=" << value.type.parameter_class
        << " rows=" << value.type.rows << " columns=" << value.type.columns
        << " elements=" << value.type.elements << " count=" << value.value_count;
    if (value.type.parameter_type >= MOJOSHADER_SYMTYPE_SAMPLER &&
        value.type.parameter_type <= MOJOSHADER_SYMTYPE_SAMPLERCUBE) {
        for (unsigned i = 0; i < value.value_count; ++i) {
            out << "\n      sampler-state " << value.valuesSS[i].type << ' ';
            describeValue(out, value.valuesSS[i].value);
        }
    } else if (value.values) {
        out << " raw=";
        // Numeric values and object indices occupy 32-bit words in the effect API.
        for (unsigned i = 0; i < value.value_count; ++i) out << std::hex << value.valuesI[i] << ',';
        out << std::dec;
    }
}

static int run(int argc, char **argv)
{
    bool shaderOnly = argc > 1 && std::string(argv[1]) == "--shader";
    const char *profile = "fx_2_0", *entry = nullptr;
    if (shaderOnly) {
        if (argc < 7) throw std::runtime_error("usage: shaderproto --shader PROFILE ENTRY SOURCE INCLUDE_ROOT OUTPUT_DIR [NAME=VALUE ...]");
        profile = argv[2]; entry = argv[3]; argc -= 3; argv += 3;
    }
    if (argc < 4) {
        std::cerr << "usage: shaderproto SOURCE INCLUDE_ROOT OUTPUT_DIR [NAME=VALUE ...]\n";
        return 2;
    }
    fs::path sourcePath = argv[1], includeRoot = argv[2], outputDir = argv[3];
    fs::create_directories(outputDir);
    NSMutableDictionary *result = [@{@"stage_reached": @"compile", @"compile_ok": @NO,
        @"parse_ok": @NO, @"translate_ok": @NO, @"metal_ok": @NO,
        @"shader_count": @0, @"msl_count": @0, @"library_count": @0} mutableCopy];
    result[@"mode"] = shaderOnly ? @"shader-only (bypasses effect writer/parser)" : @"effect";
    NSMutableArray *errors = [NSMutableArray array];
    auto error = [&](const std::string &message) {
        NSString *s = [[NSString alloc] initWithBytes:message.data() length:message.size() encoding:NSUTF8StringEncoding];
        [errors addObject:s ?: @"Non-UTF8 error"];
        std::cerr << message << '\n';
    };
    auto finish = [&]() {
        result[@"errors"] = errors;
        NSData *json = [NSJSONSerialization dataWithJSONObject:result options:NSJSONWritingPrettyPrinted error:nil];
        writeFile(outputDir / "result.json", json.bytes, json.length);
        std::cout.write(static_cast<const char *>(json.bytes), json.length);
        std::cout << '\n';
        return [result[@"metal_ok"] boolValue] ? 0 : 1;
    };
    Includes includes;
    for (const auto &file : fs::recursive_directory_iterator(includeRoot))
        if (file.is_regular_file()) includes.files[lower(fs::relative(file.path(), includeRoot).generic_string())] = file.path();
    std::vector<std::string> names, values;
    for (int i = 4; i < argc; ++i) {
        std::string define = argv[i];
        auto eq = define.find('=');
        names.push_back(define.substr(0, eq));
        values.push_back(eq == std::string::npos ? "1" : define.substr(eq + 1));
    }
    std::vector<vkd3d_shader_macro> macros;
    for (size_t i = 0; i < names.size(); ++i) macros.push_back({names[i].c_str(), values[i].c_str()});
    vkd3d_shader_preprocess_info preprocess{};
    preprocess.type = VKD3D_SHADER_STRUCTURE_TYPE_PREPROCESS_INFO;
    preprocess.macros = macros.data(); preprocess.macro_count = macros.size();
    preprocess.pfn_open_include = openInclude; preprocess.pfn_close_include = closeInclude;
    preprocess.include_context = &includes;
    vkd3d_shader_hlsl_source_info hlsl{};
    hlsl.type = VKD3D_SHADER_STRUCTURE_TYPE_HLSL_SOURCE_INFO;
    hlsl.next = &preprocess; hlsl.profile = profile; hlsl.entry_point = entry;
    auto source = readFile(sourcePath);
    auto sourceName = sourcePath.string();
    vkd3d_shader_compile_info info{};
    info.type = VKD3D_SHADER_STRUCTURE_TYPE_COMPILE_INFO; info.next = &hlsl;
    info.source = {source.data(), source.size()}; info.source_name = sourceName.c_str();
    info.source_type = VKD3D_SHADER_SOURCE_HLSL;
    info.target_type = shaderOnly ? VKD3D_SHADER_TARGET_D3D_BYTECODE : VKD3D_SHADER_TARGET_FX;
    info.log_level = VKD3D_SHADER_LOG_INFO;
    // A diagnostic for single shader entries only. FX runs retain default options.
    vkd3d_shader_compile_option allowHalf{VKD3D_SHADER_COMPILE_OPTION_BACKWARD_COMPATIBILITY,
        VKD3D_SHADER_COMPILE_OPTION_BACKCOMPAT_ALLOW_HALF_GLOBALS};
    if (shaderOnly) {
        info.options = &allowHalf; info.option_count = 1;
        result[@"allow_half_globals"] = @YES;
    }
    vkd3d_shader_code binary{};
    char *messages = nullptr;
    int rc = vkd3d_shader_compile(&info, &binary, &messages);
    if (messages) {
        writeFile(outputDir / "compile.log", messages, std::strlen(messages));
        if (rc < 0) error(messages);
        vkd3d_shader_free_messages(messages);
    }
    if (rc < 0) { error("vkd3d_shader_compile returned " + std::to_string(rc)); return finish(); }
    writeFile(outputDir / (shaderOnly ? "shader.bin" : "effect.fxo"), binary.code, binary.size);
    result[@"compile_ok"] = @YES; result[@"stage_reached"] = @"parse";
    id<MTLDevice> device = nil;
    unsigned shaders = 0, translated = 0, libraries = 0;
    auto translateShader = [&](const Shader &shader) {
        ++shaders;
        auto *msl = MOJOSHADER_parse(MOJOSHADER_PROFILE_METAL, shader.entry.c_str(), shader.bytes.data(),
            shader.bytes.size(), nullptr, 0, nullptr, 0, nullptr, nullptr, nullptr);
        if (msl->error_count) {
            for (int e = 0; e < msl->error_count; ++e) error(shader.entry + ": " + safe(msl->errors[e].error));
        } else if (!msl->output || !msl->output_len) error(shader.entry + ": empty MSL output");
        else {
            ++translated;
            writeFile(outputDir / (shader.entry + ".metal"), msl->output, msl->output_len);
            if (device) {
                NSString *text = [[NSString alloc] initWithBytes:msl->output length:msl->output_len encoding:NSUTF8StringEncoding];
                NSError *metalError = nil;
                id<MTLLibrary> library = [device newLibraryWithSource:text options:nil error:&metalError];
                if (library && [library newFunctionWithName:@(shader.entry.c_str())]) ++libraries;
                else error(shader.entry + ": " + (metalError ? std::string(metalError.localizedDescription.UTF8String) : "Metal entry missing"));
            }
        }
        MOJOSHADER_freeParseData(msl);
    };
    auto completeTranslation = [&]() {
        result[@"shader_count"] = @(shaders); result[@"msl_count"] = @(translated); result[@"library_count"] = @(libraries);
        if (!shaders) error("No programmable shaders: metadata/include-only or fixed-function effect; no Metal proof");
        if (shaders && translated == shaders) {
            result[@"translate_ok"] = @YES; result[@"stage_reached"] = @"metal";
            if (!device) error("MTLCreateSystemDefaultDevice returned nil; Metal compilation unavailable");
            if (libraries == shaders) result[@"metal_ok"] = @YES;
        }
    };
    if (shaderOnly) {
        auto *shader = static_cast<Shader *>(captureShader(nullptr, entry, static_cast<const unsigned char *>(binary.code),
            binary.size, nullptr, 0, nullptr, 0));
        vkd3d_shader_free_shader_code(&binary);
        if (shader->parsed->error_count) {
            for (int i = 0; i < shader->parsed->error_count; ++i) error(safe(shader->parsed->errors[i].error));
        } else {
            result[@"parse_ok"] = @YES; result[@"stage_reached"] = @"translate";
            device = MTLCreateSystemDefaultDevice(); result[@"device"] = device ? device.name : @"unavailable";
            translateShader(*shader); completeTranslation();
        }
        deleteShader(nullptr, shader);
        return finish();
    }
    MOJOSHADER_effectShaderContext context{};
    context.compileShader = captureShader; context.getParseData = parseData;
    context.deleteShader = deleteShader; context.getError = backendError;
    auto *effect = MOJOSHADER_compileEffect(static_cast<const unsigned char *>(binary.code), binary.size,
        nullptr, 0, nullptr, 0, &context);
    vkd3d_shader_free_shader_code(&binary);
    if (!effect) { error("MOJOSHADER_compileEffect returned null"); return finish(); }
    if (effect->error_count) {
        for (int i = 0; i < effect->error_count; ++i) error(safe(effect->errors[i].error));
        MOJOSHADER_deleteEffect(effect); return finish();
    }
    result[@"parse_ok"] = @YES;
    std::ofstream inventory(outputDir / "inventory.txt");
    inventory << "parameters " << effect->param_count << "\n";
    for (int i = 0; i < effect->param_count; ++i) {
        inventory << "  "; describeValue(inventory, effect->params[i].value); inventory << '\n';
    }
    inventory << "techniques " << effect->technique_count << '\n';
    unsigned passes = 0;
    for (int i = 0; i < effect->technique_count; ++i) {
        const auto &tech = effect->techniques[i];
        inventory << "  technique " << safe(tech.name) << " passes=" << tech.pass_count << '\n';
        for (unsigned p = 0; p < tech.pass_count; ++p) {
            ++passes;
            const auto &pass = tech.passes[p];
            inventory << "    pass " << safe(pass.name) << " states=" << pass.state_count << '\n';
            for (unsigned s = 0; s < pass.state_count; ++s) {
                inventory << "      state " << pass.states[s].type << ' ';
                describeValue(inventory, pass.states[s].value); inventory << '\n';
            }
        }
    }
    result[@"technique_count"] = @(effect->technique_count); result[@"pass_count"] = @(passes);
    result[@"stage_reached"] = @"translate";
    device = MTLCreateSystemDefaultDevice();
    result[@"device"] = device ? device.name : @"unavailable";
    for (int i = 0; i < effect->object_count; ++i) {
        const auto &object = effect->objects[i];
        if ((object.type != MOJOSHADER_SYMTYPE_VERTEXSHADER && object.type != MOJOSHADER_SYMTYPE_PIXELSHADER)
            || object.shader.is_preshader || !object.shader.shader) continue;
        auto *shader = static_cast<Shader *>(object.shader.shader);
        inventory << "shader object=" << i << " entry=" << shader->entry << " type=" << object.type << '\n';
        translateShader(*shader);
    }
    completeTranslation();
    MOJOSHADER_deleteEffect(effect);
    return finish();
}
int main(int argc, char **argv)
{
    @autoreleasepool {
        try { return run(argc, argv); }
        catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
    }
}
