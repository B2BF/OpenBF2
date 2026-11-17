// [TEMP] [CHOSEN, not recovered] Native equivalence guards and bounded reuse; issue #36.
#include "Native.h"
#include "NativeEquivalence.h"
#include "Wire.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace bf2fx;
namespace {
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
const std::string good = "float4 main() : COLOR0 { return float4(.2,.3,.4,1); }\n";
const std::string sideEffect =
    "static float4 state = float4(0,0,0,1);\n"
    "float initialize() { state = float4(1,0,0,1); return 0; }\n"
    "static float unreferenced = initialize();\n"
    "float4 main() : COLOR0 { return state; }\n";
ShaderJob job() { ShaderJob j; j.stage = PixelShader; j.profile = "ps_2_0"; j.entry = j.function = "main"; return j; }
Bytes full(const std::string& source) {
    Bytes code; std::string error;
    check(native::compileJob(job(), source, code, error), "Full baseline compile");
    return code;
}
std::string row(const std::string& source, const Bytes& code) {
    return native::equivalenceInputKey(job(), source) + " " + sha256(code) + "\n";
}
std::string header(const std::string& fingerprint) { return "BF2FX-native-equivalence 1\nfingerprint " + fingerprint + "\n"; }
void request(const std::filesystem::path& path, const std::string& shader) {
    Request r;
    const auto source = shader + "technique T { pass P { PixelShader = compile ps_2_0 main(); } }\n";
    r.source.assign(source.begin(), source.end());
    writeFile(path.string(), envelope(r, "request"));
}
}

int main(int argc, char** argv) {
    try {
        check(argc == 2, "Scratch directory argument");
        const std::string fingerprint = native::compilerFingerprint();
        const std::string a = good + "float unused() { return 1; }\n";
        const std::string b = good + "float unused() { return 2; }\n";
        const auto code = full(a);
        check(code == full(b), "Independent full sources agree");
        const auto manifest = header(fingerprint) + row(a, code) + row(b, code);
        unsigned calls = 0;
        const auto compile = [&](const ShaderJob& j, const std::string& source, Bytes& out, std::string& error) {
            ++calls; return native::compileJob(j, source, out, error);
        };
        native::EquivalenceCache cache(manifest, fingerprint);
        Bytes output;
        check(cache.tryCompile(job(), a, compile, output) && output == code, "Audited first compile");
        check(cache.tryCompile(job(), b, compile, output) && output == code && calls == 1, "Equivalent second source reuses bytecode");
        check(cache.stats().hits == 1 && cache.stats().entries == 1, "Reuse statistics");
        check(!cache.tryCompile(job(), a + "\n", compile, output) && calls == 1, "Changed full text is unlisted");
        auto changed = job(); changed.profile = "ps_3_0";
        check(!cache.tryCompile(changed, a, compile, output), "Profile belongs to exact input");
        changed = job(); changed.entry = "unused";
        check(!cache.tryCompile(changed, a, compile, output), "Entry belongs to exact input");
        native::EquivalenceCache stale(manifest, std::string(64, '0'));
        check(!stale.tryCompile(job(), a, compile, output) && !stale.auditedInputs(), "Stale compiler fingerprint falls back");
        native::EquivalenceCache malformed(manifest + "bad row\n", fingerprint);
        check(!malformed.auditedInputs(), "Malformed manifest is disabled atomically");
        native::EquivalenceCache absent("", fingerprint);
        check(!absent.tryCompile(job(), a, compile, output), "Absent manifest falls back");

        const auto sideCode = full(sideEffect);
        std::string trimmed; check(native::trimSource(sideEffect, "main", trimmed), "Trim side-effect example");
        check(sideCode != full(trimmed), "Static initializer counterexample remains observable");
        check(!cache.tryCompile(job(), sideEffect, compile, output), "Unlisted initializer uses full source");
        native::EquivalenceCache wrongOutput(header(fingerprint) + row(sideEffect, sideCode), fingerprint);
        const Bytes sentinel{1, 2, 3}; output = sentinel;
        check(!wrongOutput.tryCompile(job(), sideEffect, compile, output) && output == sentinel,
              "Wrong optimized digest leaves output untouched for full fallback");
        std::string error;
        check(native::compileJob(job(), sideEffect, output, error) && output == sideCode, "Initializer full fallback output");
        const std::string invalid = "float4 bad(float4 x) { return x.nope; }\n" + good;
        check(!cache.tryCompile(job(), invalid, compile, output), "Unlisted unused-function error is not trimmed");
        check(!native::compileJob(job(), invalid, output, error) && error.find("Invalid swizzle") != std::string::npos,
              "Full-source diagnostic is preserved");

        native::EquivalenceCache retry(manifest, fingerprint);
        unsigned attempts = 0;
        auto transient = [&](const ShaderJob& j, const std::string& source, Bytes& out, std::string& message) {
            if (++attempts == 1) { message = "temporary allocation failure"; return false; }
            return native::compileJob(j, source, out, message);
        };
        check(!retry.tryCompile(job(), a, transient, output), "Transient failure requests full fallback");
        check(retry.tryCompile(job(), a, transient, output) && attempts == 2, "Transient failure is not retained");
        native::EquivalenceCache throwing(manifest, fingerprint);
        attempts = 0; output = sentinel;
        auto allocationFailure = [&](const ShaderJob& j, const std::string& source, Bytes& out, std::string& message) {
            if (++attempts == 1) throw std::bad_alloc();
            return native::compileJob(j, source, out, message);
        };
        check(!throwing.tryCompile(job(), a, allocationFailure, output) && output == sentinel,
              "Thrown allocation failure leaves full fallback available");
        check(throwing.tryCompile(job(), a, allocationFailure, output) && attempts == 2,
              "Thrown allocation failure is retried");

        const std::string other = "float4 main() : COLOR0 { return float4(.7,.1,.6,1); }\n";
        const auto otherCode = full(other);
        native::EquivalenceCache bounded(manifest + row(other, otherCode), fingerprint, 1024 * 1024, 1);
        calls = 0;
        check(bounded.tryCompile(job(), a, compile, output), "Bound first entry");
        check(bounded.tryCompile(job(), other, compile, output), "Bound replacement entry");
        check(bounded.tryCompile(job(), a, compile, output) && calls == 3, "Evicted result recompiles");
        check(bounded.stats().entries == 1 && bounded.stats().peakBytes <= 1024 * 1024, "Entry and byte bounds");
        native::EquivalenceCache noStorage(manifest, fingerprint, code.size() - 1, 8);
        check(noStorage.tryCompile(job(), a, compile, output) && noStorage.stats().bytes == 0, "Oversized success is not retained");

        const std::filesystem::path scratch(argv[1]);
        std::filesystem::create_directories(scratch);
        request(scratch / "positive.request", good);
        request(scratch / "initializer.request", sideEffect);
        request(scratch / "invalid.request", invalid);
        const auto positive = unenvelope<Request>(readFile((scratch / "positive.request").string()), "request");
        check(native::known(positive) == native::Known::Unknown, "Native evidence never changes D3DX classification");
        const auto built = native::compile(positive);
        check(built.kind == EffectEntry && !built.description.shaders.empty()
              && built.description.shaders.front().bytecode == code, "Production backend preserves full bytecode");
        for (const char* name : {"positive", "initializer", "invalid"}) {
            const auto path = std::filesystem::absolute(scratch / (std::string(name) + ".request")).string() + "\n";
            writeFile((scratch / (std::string(name) + ".list")).string(), Bytes(path.begin(), path.end()));
        }
        std::cout << "Native equivalence matching, full fallbacks, transient retry and memory bounds passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
