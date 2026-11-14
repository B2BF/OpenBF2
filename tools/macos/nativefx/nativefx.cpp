// [CHOSEN, not recovered] Native effect harness for the macOS port (no Wine, no D3DX):
// vkd3d-shader preprocess + compile, the source-level effect parser and MojoShader reflection.
//
//   nativefx measure LIST.tsv REQUESTS_DIR OUT.tsv      per-shader compile classification
//   nativefx build REQUEST OUT.entry [DUMP.txt]          native Description (entry envelope)
//   nativefx dump ENTRY [OUT.txt]                        canonical description text
//   nativefx compare LIST.tsv REQUESTS_DIR ENTRIES_DIR OUT_DIR   native vs reference
//   nativefx export-jobs LISTING OUTDIR                 accepted shader jobs for a reference compiler
//   nativefx compile-jobs JOBSDIR OUTDIR                our bytecode for those jobs
//   nativefx export-effects LIST OUTDIR                 preprocessed effects for Code/Tests/effect_check
//   nativefx temp-check FILE.bin...                     D3D9 validator rules (D3dbcCheck.h)
//
// LIST.tsv rows: id, key, d3dx_ok (0/1), d3dx_class.
#include "Cache.h"
#include "Wire.h"
#include "FxBuild.h"
#include "FxParser.h"
#include "NativeVkd3d.h"
#include "NativeMojo.h"
#include "Native.h"
#include "Dump.h"
#include "D3DAsm.h"
#include "D3dbcCheck.h"
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>

using namespace bf2fx;
namespace fs = std::filesystem;

namespace {
struct ListRow { std::string id, key, ok, cls; };
std::vector<ListRow> readList(const std::string& path) {
    std::ifstream in(path);
    std::vector<ListRow> rows;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        ListRow r;
        std::getline(s, r.id, '\t'); std::getline(s, r.key, '\t'); std::getline(s, r.ok, '\t'); std::getline(s, r.cls, '\t');
        if (!r.key.empty()) rows.push_back(r);
    }
    return rows;
}
std::string clean(std::string s) {
    for (auto& c : s) if (c == '\t' || c == '\n' || c == '\r') c = ' ';
    if (s.size() > 300) s.resize(300);
    return s;
}
// First vkd3d diagnostic: "file:line:col: E5017: message".
void firstError(const std::string& log, std::string& code, std::string& message) {
    std::istringstream s(log);
    std::string line;
    while (std::getline(s, line)) {
        auto at = line.find(": E");
        if (at == std::string::npos) continue;
        auto colon = line.find(':', at + 2);
        code = line.substr(at + 2, colon == std::string::npos ? std::string::npos : colon - at - 2);
        message = colon == std::string::npos ? "" : line.substr(colon + 2);
        return;
    }
    code = "unknown";
    message = log.substr(0, log.find('\n'));
}

// Profile ladder [CHOSEN]: the requested profile first, then promotions that the Metal
// target does not distinguish (MojoShader emits the same MSL interface). vkd3d has no working
// ps_1_x texture codegen and runs out of vs_1_1/vs_2_0 temporaries on skinned meshes.
std::vector<std::string> ladder(const std::string& profile) {
    static const bool enabled = getenv("NATIVEFX_LADDER") && std::string(getenv("NATIVEFX_LADDER")) == "1";
    std::vector<std::string> out{profile};
    if (!enabled) return out;
    if (profile == "vs_1_1") out.insert(out.end(), {"vs_2_0", "vs_3_0"});
    else if (profile == "vs_2_0" || profile == "vs_2_a") out.push_back("vs_3_0");
    else if (profile.rfind("ps_1_", 0) == 0) out.insert(out.end(), {"ps_2_0", "ps_2_b", "ps_3_0"});
    else if (profile == "ps_2_0" || profile == "ps_2_a" || profile == "ps_2_b") out.push_back("ps_3_0");
    return out;
}

struct Vkd3dBackend : ShaderBackend {
    std::string used;      // Profile that produced the last bytecode.
    std::string requested; // Diagnostic of the requested profile ("" when it succeeded).
    bool compile(const ShaderJob& job, const std::string& shaderText, Bytes& bytecode, std::string& error) override {
        used.clear(); requested.clear();
        if (!job.asmText.empty() || job.function.empty()) {
            std::string e;
            if (!assemble(job.asmText, bytecode, e)) { requested = "asm: " + e; error = requested; return false; }
            Shader probe; probe.bytecode = bytecode; probe.entry = "ShaderFunction";
            std::vector<SamplerSlot> none;
            if (!bf2fx::native::reflect(probe, none, e)) { requested = "mojo: " + e; error = requested; return false; }
            used = "asm";
            return true;
        }
        for (const auto& profile : ladder(job.profile)) {
            std::string log, e;
            int rc = bf2fx::native::compileShader(shaderText + job.wrapper, job.entry, profile, bytecode, log);
            if (rc < 0) {
                std::string code, message;
                firstError(log, code, message);
                e = code + ": " + message;
            } else {
                Shader probe; probe.bytecode = bytecode; probe.entry = "ShaderFunction";
                std::vector<SamplerSlot> none;
                if (bf2fx::native::reflect(probe, none, e)) { used = profile; return true; }
                e = "mojo: " + e;
            }
            if (profile == job.profile) requested = e;
            if (getenv("NATIVEFX_VERBOSE")) std::cerr << job.function << " " << profile << ": " << e << "\n";
        }
        error = requested;
        return false;
    }
    bool reflect(Shader& shader, const std::vector<SamplerSlot>& samplers, std::string& error) override {
        return bf2fx::native::reflect(shader, samplers, error);
    }
};

// Shader values of a pass state or a global shader initializer.
void collect(const src::Expr& e, uint32_t stage, std::vector<std::pair<uint32_t, const src::Expr*>>& out) {
    if (e.kind == src::Expr::Compile || e.kind == src::Expr::Asm) out.push_back({stage, &e});
    else if (e.kind == src::Expr::List) for (const auto& a : e.args) collect(*a, stage, out);
}

int measure(const std::string& listPath, const std::string& requests, const std::string& outPath) {
    auto rows = readList(listPath);
    std::ofstream out(outPath);
    out << "id\tkey\td3dx_ok\td3dx_class\tstage\tprofile\tfunction\tstatus\tcode\tmessage\tms\n";
    Vkd3dBackend backend;
    for (const auto& r : rows) {
        auto prefix = r.id + "\t" + r.key + "\t" + r.ok + "\t" + r.cls + "\t";
        Request req;
        try {
            req = unenvelope<Request>(readFile(requests + "/" + r.key + ".request"), "request");
        } catch (const std::exception& e) {
            out << prefix << "-\t-\t-\trequest\t-\t" << clean(e.what()) << "\t0\n";
            continue;
        }
        std::string text, log;
        if (!bf2fx::native::preprocessSource(req, text, log)) {
            std::string code, message;
            firstError(log, code, message);
            out << prefix << "-\t-\t-\tpreprocess\t" << code << "\t" << clean(message) << "\t0\n";
            continue;
        }
        src::Effect fx;
        src::Error perr;
        if (!src::parse(text, fx, perr)) {
            out << prefix << "-\t-\t-\tparse\t-\t" << clean("line " + std::to_string(perr.line) + ": " + perr.message) << "\t0\n";
            continue;
        }
        if (fx.techniques.empty()) {
            out << prefix << "-\t-\t-\tno_techniques\t-\t-\t0\n";
            continue;
        }
        std::vector<std::pair<uint32_t, const src::Expr*>> values;
        for (const auto& g : fx.globals) {
            if (!g.init) continue;
            if (g.type.base == src::Base::VertexShader) collect(*g.init, VertexShader, values);
            if (g.type.base == src::Base::PixelShader) collect(*g.init, PixelShader, values);
        }
        for (const auto& t : fx.techniques)
            for (const auto& p : t.passes)
                for (const auto& s : p.states) {
                    if (src::iequals(s.name, "VertexShader")) collect(*s.value, VertexShader, values);
                    if (src::iequals(s.name, "PixelShader")) collect(*s.value, PixelShader, values);
                }
        std::string shaderPS = compilerText(fx, text, PixelShader), shaderVS = compilerText(fx, text, VertexShader);
        std::set<std::string> seen;
        for (const auto& v : values) {
            ShaderJob job;
            std::string error;
            const char* stage = v.first == VertexShader ? "vs" : "ps";
            if (!shaderJob(fx, text, *v.second, v.first, job, error)) {
                out << prefix << stage << "\t-\t-\tjob\t-\t" << clean(error) << "\t0\n";
                continue;
            }
            if (!seen.insert(std::to_string(v.first) + job.key).second) continue;
            Bytes code;
            auto t0 = std::chrono::steady_clock::now();
            bool ok = backend.compile(job, v.first == VertexShader ? shaderVS : shaderPS, code, error);
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
            // status: compiled (requested profile) | promoted (another ladder profile) | the
            // requested profile's failure class (vkd3d_error / mojo_error).
            std::string status, detail = backend.requested;
            if (ok) status = backend.used == "asm" ? "assembled" : backend.used == job.profile ? "compiled" : "promoted";
            else status = detail.rfind("mojo: ", 0) == 0 ? "mojo_error" : detail.rfind("asm: ", 0) == 0 ? "asm_error" : "vkd3d_error";
            if (status == "asm_error") detail = detail.substr(5);
            if (status == "mojo_error") detail = detail.substr(6);
            auto colon = detail.find(": ");
            std::string ecode = status == "vkd3d_error" || status == "promoted" ? detail.substr(0, colon) : "-";
            std::string message = status == "vkd3d_error" || status == "promoted" ? (colon == std::string::npos ? detail : detail.substr(colon + 2)) : detail;
            if (status == "promoted") message = backend.used + " after " + (detail.rfind("mojo: ", 0) == 0 ? detail : ecode + " " + message);
            if (!job.asmText.empty()) { std::istringstream vs(job.asmText); vs >> job.profile; job.function = "asm"; }
            out << prefix << stage << "\t" << job.profile << "\t" << job.function << "\t" << status << "\t"
                << (status == "promoted" ? backend.used : ecode) << "\t" << clean(message) << "\t" << ms << "\n";
        }
        out.flush();
    }
    return 0;
}

int buildOne(const std::string& requestPath, const std::string& outPath, const char* dumpPath) {
    Request req = unenvelope<Request>(readFile(requestPath), "request");
    std::string text, log;
    if (!bf2fx::native::preprocessSource(req, text, log)) { std::cerr << log; return 1; }
    Vkd3dBackend backend;
    Entry e;
    e.key = cacheKey(req);
    std::string error;
    BuildReport report;
    if (!build(text, backend, e.description, error, &report)) { std::cerr << "build: " << error << "\n"; return 1; }
    writeFile(outPath, envelope(e, "entry"));
    if (dumpPath) {
        std::ofstream d(dumpPath);
        d << nativefx::dump(e.description);
    }
    return 0;
}
// Debug: compile one function of a request and print the vkd3d disassembly.
int shaderOne(const std::string& requestPath, const std::string& function, const std::string& profile, const char* binOut) {
    Request req = unenvelope<Request>(readFile(requestPath), "request");
    std::string text, log;
    if (!bf2fx::native::preprocessSource(req, text, log)) { std::cerr << log; return 1; }
    src::Effect fx; src::Error perr;
    if (!src::parse(text, fx, perr)) { std::cerr << perr.message << "\n"; return 1; }
    std::vector<std::pair<uint32_t, const src::Expr*>> values;
    for (const auto& g : fx.globals) if (g.init && (g.type.base == src::Base::VertexShader || g.type.base == src::Base::PixelShader))
        collect(*g.init, g.type.base == src::Base::VertexShader ? VertexShader : PixelShader, values);
    for (const auto& t : fx.techniques) for (const auto& p : t.passes) for (const auto& s : p.states) {
        if (src::iequals(s.name, "VertexShader")) collect(*s.value, VertexShader, values);
        if (src::iequals(s.name, "PixelShader")) collect(*s.value, PixelShader, values);
    }
    for (const auto& v : values) {
        ShaderJob job; std::string error;
        if (!shaderJob(fx, text, *v.second, v.first, job, error) || job.function != function) continue;
        if (!profile.empty() && job.profile != profile) continue;
        Bytes code;
        std::string shader = compilerText(fx, text, v.first);
        int rc = bf2fx::native::compileShader(shader + job.wrapper, job.entry, job.profile, code, log);
        std::cout << "// " << job.key << " rc=" << rc << "\n" << job.wrapper << "\n" << log;
        if (rc < 0) return 1;
        std::string dis; bf2fx::native::disassemble(code, dis); std::cout << dis;
        Shader rec; rec.bytecode = code; rec.entry = "ShaderFunction"; std::vector<SamplerSlot> none;
        if (!bf2fx::native::reflect(rec, none, error)) std::cout << "// mojo: " << error << "\n";
        if (binOut) writeFile(binOut, code);
        return 0;
    }
    std::cerr << "no such shader\n"; return 1;
}
// Native vs reference structure: canonical dumps of both descriptions per accepted request.
// memo-guard LISTING OUT.tsv: for every shader job of every request file in LISTING (one path
// per line), compile the full text and the trimmed text (bf2fx::native::trimSource) and compare
// outcome and bytecode. Only D3DX-accepted requests use the memo; others are skipped. Prints a
// summary; exit 1 on any difference.
// export-jobs LISTING OUTDIR: every distinct shader job of the D3DX-accepted requests in LISTING as
// OUTDIR/<key>.hlsl (the trimmed compile text, the input the shader memo compiles) plus OUTDIR/jobs.tsv
// rows key, profile, entry, loop (1 when the text has a for/while/do loop), flags. The key is memo-guard's
// MEMO_GUARD_BYTECODE_DIR name, so a reference compiler's output lines up with ours. Game-derived:
// stays under out/, never committed.
int exportJobs(const std::string& listing, const std::string& outDir) {
    fs::create_directories(outDir);
    std::ifstream in(listing);
    std::ofstream tsv(outDir + "/jobs.tsv", std::ios::app);
    std::ofstream sources(outDir + "/sources.tsv", std::ios::app);  // key, function, request path (every use)
    std::set<std::string> written;
    for (const auto& e : fs::directory_iterator(outDir)) if (e.path().extension() == ".hlsl") written.insert(e.path().stem().string());
    std::string path; unsigned added = 0, requests = 0;
    while (std::getline(in, path)) {
        if (path.empty()) continue;
        Request req;
        try { req = unenvelope<Request>(readFile(path), "request"); } catch (...) { continue; }
        if (bf2fx::native::known(req) != bf2fx::native::Known::Accepted) continue;
        std::string text, log;
        if (!bf2fx::native::preprocessSource(req, text, log)) continue;
        src::Effect fx; src::Error perr;
        if (!src::parse(text, fx, perr)) continue;
        ++requests;
        std::vector<std::pair<uint32_t, const src::Expr*>> values;
        for (const auto& g : fx.globals) if (g.init && (g.type.base == src::Base::VertexShader || g.type.base == src::Base::PixelShader))
            collect(*g.init, g.type.base == src::Base::VertexShader ? VertexShader : PixelShader, values);
        for (const auto& t : fx.techniques) for (const auto& ps : t.passes) for (const auto& st : ps.states) {
            if (src::iequals(st.name, "VertexShader")) collect(*st.value, VertexShader, values);
            if (src::iequals(st.name, "PixelShader")) collect(*st.value, PixelShader, values);
        }
        const std::string texts[2] = {compilerText(fx, text, PixelShader), compilerText(fx, text, VertexShader)};
        for (const auto& v : values) {
            ShaderJob job; std::string error;
            if (!shaderJob(fx, text, *v.second, v.first, job, error) || !job.asmText.empty()) continue;
            const std::string full = texts[v.first == VertexShader ? 1 : 0] + job.wrapper;
            std::string trimmed;
            if (!bf2fx::native::trimSource(full, job.entry, trimmed)) continue;
            std::ostringstream key;
            key << std::hex << std::hash<std::string>()(job.profile + "|" + job.entry + "|" + trimmed);
            sources << key.str() << "\t" << job.function << "\t" << path << "\n";
            if (!written.insert(key.str()).second) continue;
            std::ofstream(outDir + "/" + key.str() + ".hlsl", std::ios::binary) << trimmed;
            static const std::regex loop("\\b(for|while)\\s*\\(|\\bdo\\s*\\{");
            // Shader compile flags: the request's D3DXSHADER_* bits (effect-only D3DXFX_NOT_CLONEABLE and
            // D3DXFX_LARGEADDRESSAWARE removed).
            tsv << key.str() << "\t" << job.profile << "\t" << job.entry << "\t" << (std::regex_search(trimmed, loop) ? 1 : 0)
                << "\t" << (req.flags & ~0x20800u) << "\n";
            ++added;
        }
    }
    std::cout << "export-jobs: " << requests << " accepted requests, " << added << " new jobs, " << written.size() << " in " << outDir << "\n";
    return 0;
}

// export-effects LIST OUTDIR: each request in LIST (one path per line) as OUTDIR/<request stem>.fx (the
// preprocessed effect text) plus OUTDIR/effects.tsv rows stem, D3DXCreateEffect flags. Input of
// Code/Tests/effect_check (D3DXCreateEffect, ValidateTechnique, FindNextValidTechnique on Windows).
// Game-derived: stays under out/.
int exportEffects(const std::string& listing, const std::string& outDir) {
    fs::create_directories(outDir);
    std::ifstream in(listing);
    std::ofstream tsv(outDir + "/effects.tsv", std::ios::app);
    std::string path; unsigned n = 0, failed = 0;
    while (std::getline(in, path)) {
        if (path.empty()) continue;
        Request req = unenvelope<Request>(readFile(path), "request");
        std::string text, log;
        if (!bf2fx::native::preprocessSource(req, text, log)) { ++failed; continue; }
        const std::string stem = fs::path(path).stem().string();
        std::ofstream(outDir + "/" + stem + ".fx", std::ios::binary) << text;
        tsv << stem << "\t" << req.flags << "\n";
        ++n;
    }
    std::cout << "export-effects: " << n << " effects, " << failed << " not preprocessed\n";
    return failed ? 1 : 0;
}

// compile-jobs JOBSDIR OUTDIR: compiles every export-jobs job (as the shader memo does: the trimmed
// text, the requested profile's ladder) into OUTDIR/<key>.bin; failures are listed and counted.
int compileJobs(const std::string& dir, const std::string& outDir) {
    fs::create_directories(outDir);
    std::ifstream tsv(dir + "/jobs.tsv");
    std::string line; unsigned ok = 0, failed = 0;
    while (std::getline(tsv, line)) {
        std::istringstream f(line);
        std::string key, profile, entry;
        if (!std::getline(f, key, '\t') || !std::getline(f, profile, '\t') || !std::getline(f, entry, '\t')) continue;
        auto text = readFile(dir + "/" + key + ".hlsl");
        ShaderJob job; job.profile = profile; job.entry = entry; job.function = entry;
        Bytes code; std::string error;
        if (bf2fx::native::compileJob(job, std::string(text.begin(), text.end()), code, error)) {
            writeFile(outDir + "/" + key + ".bin", code); ++ok;
            // Promoted by the profile ladder (Native.cpp): why the requested profile failed.
            const unsigned major = code.size() >= 4 ? code[1] : 0;
            if (profile.size() >= 4 && major != unsigned(profile[3] - '0')) {
                Bytes direct; std::string log;
                bf2fx::native::compileShader(std::string(text.begin(), text.end()), entry, profile, direct, log);
                std::string first;
                std::istringstream ls(log);
                for (std::string l; std::getline(ls, l);) if (l.find(": E") != std::string::npos) { first = l.substr(l.find(": E") + 2); break; }
                std::cout << "promoted " << key << " " << profile << " -> " << major << ".x: " << first << "\n";
            }
        }
        else { ++failed; std::cout << "failed " << key << " " << profile << ": " << error.substr(0, error.find('\n')) << "\n"; }
    }
    std::cout << "compile-jobs: " << ok << " compiled, " << failed << " failed\n";
    return failed ? 1 : 0;
}

int memoGuard(const std::string& listing, const std::string& outPath) {
    std::ifstream in(listing); std::ofstream out(outPath);
    out << "request\tstage\tprofile\tfunction\tfull_ok\ttrim_ok\tsame\tfull_ms\ttrim_ms\thigher_vs\tuninit_reads\tencoding_findings\n";
    std::string path; unsigned jobs = 0, diffs = 0, skipped = 0, untrimmed = 0; double fullMs = 0, trimMs = 0;
    // higher: vs_1_x/vs_2_x jobs whose compile fails with E5017/E9015, i.e. the jobs the effect
    // layer compiles internally as vs_3_0 (BuildReport::higherVertexProfile; these requests are
    // all D3DX-accepted). uninit: compiled jobs reading a temp component before writing it.
    unsigned higher = 0, uninit = 0, malformed = 0, encoding = 0, constLimit = 0, swizzle = 0, outputWrite = 0, tempLimit = 0, slotLimit = 0, failing = 0;
    // MEMO_GUARD_BYTECODE_DIR: each distinct compiled job's bytecode as <hash of profile|entry|trimmed text>.bin, so
    // two compilers' outputs for the same job can be compared (tools/macos/nativefx/d3dbc_compare.py).
    const char* bytecodeDir = getenv("MEMO_GUARD_BYTECODE_DIR");
    std::set<std::string> distinctHigher;
    std::set<std::string> distinct;
    while (std::getline(in, path)) {
        if (path.empty()) continue;
        Request req;
        try { req = unenvelope<Request>(readFile(path), "request"); } catch (...) { ++skipped; continue; }
        if (bf2fx::native::known(req) != bf2fx::native::Known::Accepted) { ++skipped; continue; }
        std::string text, log;
        if (!bf2fx::native::preprocessSource(req, text, log)) { ++skipped; continue; }
        src::Effect fx; src::Error perr;
        if (!src::parse(text, fx, perr)) { ++skipped; continue; }
        std::vector<std::pair<uint32_t, const src::Expr*>> values;
        for (const auto& g : fx.globals) if (g.init && (g.type.base == src::Base::VertexShader || g.type.base == src::Base::PixelShader))
            collect(*g.init, g.type.base == src::Base::VertexShader ? VertexShader : PixelShader, values);
        for (const auto& t : fx.techniques) for (const auto& ps : t.passes) for (const auto& st : ps.states) {
            if (src::iequals(st.name, "VertexShader")) collect(*st.value, VertexShader, values);
            if (src::iequals(st.name, "PixelShader")) collect(*st.value, PixelShader, values);
        }
        const std::string texts[2] = {compilerText(fx, text, PixelShader), compilerText(fx, text, VertexShader)};
        std::set<std::string> seen;
        for (const auto& v : values) {
            ShaderJob job; std::string error;
            if (!shaderJob(fx, text, *v.second, v.first, job, error) || !job.asmText.empty()) continue;
            if (!seen.insert(std::to_string(v.first) + job.key).second) continue;
            const std::string full = texts[v.first == VertexShader ? 1 : 0] + job.wrapper;
            std::string trimmed;
            if (!bf2fx::native::trimSource(full, job.entry, trimmed)) { ++untrimmed; continue; }
            distinct.insert(job.profile + "|" + job.entry + "|" + trimmed);
            Bytes a, b; std::string ea, eb;
            auto t0 = std::chrono::steady_clock::now();
            bool oa = bf2fx::native::compileJob(job, full, a, ea);
            auto t1 = std::chrono::steady_clock::now();
            bool ob = bf2fx::native::compileJob(job, trimmed, b, eb);
            auto t2 = std::chrono::steady_clock::now();
            double ma = std::chrono::duration<double, std::milli>(t1 - t0).count(), mb = std::chrono::duration<double, std::milli>(t2 - t1).count();
            fullMs += ma; trimMs += mb; ++jobs;
            const bool same = oa == ob && (!oa || a == b);
            const bool isHigher = !oa && v.first == VertexShader && job.profile.rfind("vs_", 0) == 0 && job.profile != "vs_3_0" &&
                (ea.find("E5017") != std::string::npos || ea.find("E9015") != std::string::npos);
            if (isHigher) { ++higher; distinctHigher.insert(job.profile + "|" + job.entry + "|" + trimmed); }
            unsigned uninitReads = 0, encodingFindings = 0;
            if (oa) {
                const auto t = nativefx::checkD3dbc(a);
                uninitReads = t.uninitialized; encodingFindings = t.encodingFindings(); malformed += t.malformed;
                if (t.uninitialized) {
                    ++uninit;
                    if (uninit <= 5) std::cerr << "uninitialized temp read: " << path << " " << job.profile << " " << job.function << ": " << t.first << "\n";
                }
                if (encodingFindings) {
                    ++encoding; constLimit += !!t.constLimit; swizzle += !!t.swizzle; outputWrite += !!t.outputWrite;
                    tempLimit += t.tempLimit; slotLimit += t.slotLimit;
                    // [TEMP] Slot limits are allowed until vkd3d emits dp4/dp3 for mul() (piece 17b); every other
                    // validator rule fails the guard.
                    failing += encodingFindings > t.slotLimit;
                    if (encoding <= 5) std::cerr << "encoding: " << path << " " << job.profile << " " << job.function << ": " << t.firstRule << "\n";
                }
            }
            if (!same) {
                ++diffs;
                if (const char* dump = getenv("MEMO_GUARD_DUMP")) {
                    const std::string base = std::string(dump) + "/" + std::to_string(diffs);
                    std::ofstream(base + "-full.fx") << full; std::ofstream(base + "-trim.fx") << trimmed;
                    std::string da, db; bf2fx::native::disassemble(a, da); bf2fx::native::disassemble(b, db);
                    std::ofstream(base + "-full.asm") << ea << "\n" << da; std::ofstream(base + "-trim.asm") << eb << "\n" << db;
                    std::ofstream(base + "-job.txt") << job.profile << " " << job.entry << " " << job.function << "\n";
                }
            }
            if (oa && bytecodeDir) {
                std::ostringstream name;
                name << bytecodeDir << "/" << std::hex << std::hash<std::string>()(job.profile + "|" + job.entry + "|" + trimmed) << ".bin";
                writeFile(name.str(), a);
            }
            out << path << "\t" << (v.first == VertexShader ? "vs" : "ps") << "\t" << job.profile << "\t" << job.function << "\t"
                << oa << "\t" << ob << "\t" << same << "\t" << ma << "\t" << mb << "\t" << isHigher << "\t" << uninitReads << "\t" << encodingFindings << "\n";
        }
        out.flush();
    }
    std::cout << "memo-guard: " << jobs << " jobs, " << diffs << " differ, " << distinct.size() << " distinct trimmed inputs, "
              << untrimmed << " not trimmable, " << skipped << " requests skipped; full " << fullMs / 1000 << " s, trimmed "
              << trimMs / 1000 << " s\n";
    std::cout << "memo-guard: " << higher << " vs_1_x/vs_2_x jobs need the internal vs_3_0 (E5017/E9015), "
              << distinctHigher.size() << " distinct; " << uninit << " compiled jobs read an uninitialized temp, "
              << malformed << " not decodable\n";
    std::cout << "memo-guard: " << encoding << " compiled jobs break a validator encoding rule (" << constLimit
              << " vs c# limit, " << swizzle << " ps_2_0 swizzle, " << outputWrite << " ps_2_x oC/oDepth write, " << tempLimit << " r# limit, " << slotLimit << " slot limit)\n";
    std::cout << "memo-guard: " << failing << " compiled jobs fail a validator rule other than the slot limit; allowed until 17b: "
              << slotLimit << " over the slot limit\n";
    return diffs || uninit || malformed || failing ? 1 : 0;
}

int structure(const std::string& listPath, const std::string& requests, const std::string& entries, const std::string& outDir) {
    auto rows = readList(listPath);
    fs::create_directories(outDir);
    std::ofstream out(outDir + "/structure.tsv");
    out << "id\tkey\tstatus\tsame\tsame_objects\tshaders\tshader_failures\tfirst_difference\n";
    for (const auto& r : rows) {
        if (r.ok != "1") continue;
        auto prefix = r.id + "\t" + r.key + "\t";
        Entry ref;
        try { ref = unenvelope<Entry>(readFile(entries + "/" + r.key + "/entry.bin"), "entry"); }
        catch (const std::exception& e) { out << prefix << "no_reference\t0\t0\t0\t0\t" << clean(e.what()) << "\n"; continue; }
        Request req = unenvelope<Request>(readFile(requests + "/" + r.key + ".request"), "request");
        std::string text, log;
        if (!bf2fx::native::preprocessSource(req, text, log)) { out << prefix << "preprocess\t0\t0\t0\t0\t" << clean(log) << "\n"; continue; }
        Vkd3dBackend backend;
        Description d;
        std::string error;
        BuildReport report;
        report.tolerateShaderFailures = true;
        if (!build(text, backend, d, error, &report)) { out << prefix << "build\t0\t0\t0\t0\t" << clean(error) << "\n"; continue; }
        if (const char* save = getenv("NATIVEFX_SAVE_ENTRIES")) {
            Entry e; e.key = r.key; e.description = d;
            fs::create_directories(save);
            writeFile(std::string(save) + "/" + r.id + ".entry", envelope(e, "entry"));
        }
        if (const char* save = getenv("NATIVEFX_SAVE_REFS")) {
            // Reference shaders re-translated with this MojoShader build: GPU comparisons
            // then isolate the compiler (vkd3d vs D3DX) from translator differences.
            Entry e = ref;
            for (auto& sh : e.description.shaders) {
                if (sh.selectorKind || sh.bytecode.empty()) continue;
                std::string terr; std::vector<SamplerSlot> none;
                if (!bf2fx::native::reflect(sh, none, terr)) sh.msl.clear();
            }
            fs::create_directories(save);
            writeFile(std::string(save) + "/" + r.id + ".entry", envelope(e, "entry"));
        }
        std::string a = nativefx::dump(d), b = nativefx::dump(ref.description);
        std::string ao = nativefx::dump(d, true), bo = nativefx::dump(ref.description, true);
        std::string diff;
        if (a != b) {
            std::istringstream x(a), y(b); std::string lx, ly; int n = 0;
            for (;;) {
                bool gx = (bool)std::getline(x, lx), gy = (bool)std::getline(y, ly); ++n;
                if (!gx && !gy) break;
                if (lx != ly || gx != gy) { diff = "line " + std::to_string(n) + ": native '" + (gx ? lx : "<end>") + "' ref '" + (gy ? ly : "<end>") + "'"; break; }
            }
            std::ofstream(outDir + "/" + r.id + ".native.txt") << ao;
            std::ofstream(outDir + "/" + r.id + ".ref.txt") << bo;
        }
        out << prefix << "built\t" << (a == b) << "\t" << (ao == bo) << "\t" << report.shaders << "\t"
            << report.shaderFailures.size() << "\t" << clean(diff) << "\n";
        out.flush();
    }
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    setenv("VKD3D_SHADER_DEBUG", "none", 0);
    try {
        std::string cmd = argc > 1 ? argv[1] : "";
        if (cmd == "measure" && argc == 5) return measure(argv[2], argv[3], argv[4]);
        if (cmd == "native-compile" && argc == 3) {
            Request req = unenvelope<Request>(readFile(argv[2]), "request");
            Entry e = bf2fx::native::compile(req);
            std::cout << "kind " << e.kind << " parse " << std::hex << e.parseResult << " compile " << e.compileResult << std::dec
                      << " known " << int(bf2fx::native::known(req)) << " params " << e.description.parameters.size()
                      << " techniques " << e.description.techniques.size() << "\n" << e.log;
            return 0;
        }
        if (cmd == "refusal-keys" && argc == 3) {
            // LIST of .request paths -> "<cache key> <refusal key>" (toolchain independent).
            std::ifstream in(argv[2]); std::string path;
            while (std::getline(in, path)) {
                if (path.empty()) continue;
                Request req = unenvelope<Request>(readFile(path), "request");
                std::cout << fs::path(path).stem().string() << " " << bf2fx::native::refusalKey(req) << "\n";
            }
            return 0;
        }
        if (cmd == "disasm" && argc >= 4) {
            auto e = unenvelope<Entry>(readFile(argv[2]), "entry");
            for (const auto& sh : e.description.shaders) if (sh.object == uint32_t(std::stoul(argv[3]))) {
                std::string dis; bf2fx::native::disassemble(sh.bytecode, dis); std::cout << dis;
                if (argc > 4) std::cout << sh.msl;
            }
            return 0;
        }
        if (cmd == "buildtext" && argc == 3) {
            auto b = readFile(argv[2]); std::string text(b.begin(), b.end());
            Vkd3dBackend backend; Description d; std::string error; BuildReport report; report.tolerateShaderFailures = true;
            if (!build(text, backend, d, error, &report)) { std::cerr << error << "\n"; return 1; }
            std::cout << nativefx::dump(d, true);
            for (const auto& f : report.shaderFailures) std::cout << "// failed: " << f << "\n";
            return 0;
        }
        if (cmd == "memo-guard" && argc == 4) return memoGuard(argv[2], argv[3]);
        if (cmd == "export-jobs" && argc == 4) return exportJobs(argv[2], argv[3]);
        if (cmd == "compile-jobs" && argc == 4) return compileJobs(argv[2], argv[3]);
        if (cmd == "export-effects" && argc == 4) return exportEffects(argv[2], argv[3]);
        if (cmd == "temp-check" && argc >= 3) {
            // Each FILE: D3D9 bytecode. One line per file; exit 1 on any finding or undecodable file.
            unsigned bad = 0;
            for (int k = 2; k < argc; ++k) {
                const auto t = nativefx::checkD3dbc(readFile(argv[k]));
                std::cout << argv[k] << ": " << t.reads << " temp component reads, " << t.uninitialized << " uninitialized, "
                          << t.constLimit << " c# limit, " << t.swizzle << " swizzle, " << t.outputWrite << " output write, r" << t.temps
                          << ", " << t.slots << "+" << t.texSlots << " slots" << (t.tempLimit ? " OVER r# LIMIT" : "") << (t.slotLimit ? " OVER SLOT LIMIT" : "")
                          << (t.malformed ? ", not decodable" : "") << (t.first.empty() ? "" : " (first read: " + t.first + ")")
                          << (t.firstRule.empty() ? "" : " (first rule: " + t.firstRule + ")") << "\n";
                bad += !t.clean();
            }
            std::cout << "temp-check: " << argc - 2 << " files, " << bad << " failed\n";
            return bad ? 1 : 0;
        }
        if (cmd == "structure" && argc == 6) return structure(argv[2], argv[3], argv[4], argv[5]);
        if (cmd == "build" && (argc == 4 || argc == 5)) return buildOne(argv[2], argv[3], argc == 5 ? argv[4] : nullptr);
        if (cmd == "shader" && argc >= 4) return shaderOne(argv[2], argv[3], argc > 4 ? argv[4] : "", argc > 5 ? argv[5] : nullptr);
        if (cmd == "preprocess" && argc >= 4) {
            Request req = unenvelope<Request>(readFile(argv[2]), "request");
            // Optional NAME=VALUE overrides: another profile attempt of the same group.
            for (int i = 4; i < argc; ++i) {
                std::string a = argv[i]; auto eq = a.find('=');
                bool found = false;
                for (auto& d : req.defines) if (d.name == a.substr(0, eq)) { d.value = a.substr(eq + 1); found = true; }
                if (!found) req.defines.push_back({a.substr(0, eq), a.substr(eq + 1)});
            }
            std::string text, log;
            bool ok = bf2fx::native::preprocessSource(req, text, log);
            std::ofstream(argv[3]) << text;
            std::cerr << log;
            return ok ? 0 : 1;
        }
        if (cmd == "dump" && (argc == 3 || argc == 4)) {
            auto e = unenvelope<Entry>(readFile(argv[2]), "entry");
            std::string text = nativefx::dump(e.description);
            if (argc == 4) std::ofstream(argv[3]) << text; else std::cout << text;
            return 0;
        }
        std::cerr << "usage: nativefx measure|build|preprocess|dump ...\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "nativefx: " << e.what() << "\n";
        return 2;
    }
}
