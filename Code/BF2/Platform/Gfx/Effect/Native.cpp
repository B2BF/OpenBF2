// [CHOSEN, not recovered] Native effect compilation for the macOS client. See Native.h.
#if defined(__APPLE__)
#include "Native.h"
#include "D3DAsm.h"
#include "FxBuild.h"
#include "NativeMojo.h"
#include "NativeVkd3d.h"
#include "Wire.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#ifndef BF2_FX_NATIVE_IDENTITY
#define BF2_FX_NATIVE_IDENTITY "native-unpinned"
#endif

namespace bf2fx {
namespace native {
namespace {

// The D3DX refusal table (data/d3dx-refusals.txt, generated into refusals.inc by CMake):
// 64-bit prefixes of refusalKey(), sorted, with 'A' (accepted) or 'R' (refused).
struct TableEntry { uint64_t key; char state; };
const TableEntry kTable[] = {
#include "refusals.inc"
};

// Profile ladder [CHOSEN]: the requested profile first, then promotions that the Metal target
// does not distinguish. vkd3d has no ps_1_x texture code generation and runs out of vs_1_1
// temporaries on skinned meshes (GPU equivalence).
std::vector<std::string> ladder(const std::string& profile) {
    std::vector<std::string> out{profile};
    if (profile == "vs_1_1") out.insert(out.end(), {"vs_2_0", "vs_3_0"});
    else if (profile == "vs_2_0" || profile == "vs_2_a") out.push_back("vs_3_0");
    else if (profile.rfind("ps_1_", 0) == 0) out.insert(out.end(), {"ps_2_0", "ps_2_b", "ps_3_0"});
    else if (profile == "ps_2_0" || profile == "ps_2_a" || profile == "ps_2_b") out.push_back("ps_3_0");
    return out;
}
// First vkd3d diagnostic: "file:line:col: E5017: message".
std::string firstError(const std::string& log) {
    std::istringstream s(log);
    std::string line;
    while (std::getline(s, line)) if (line.find(": E") != std::string::npos) return line;
    return log.substr(0, log.find('\n'));
}

// ---- Shader-level memo ----------------------------------------------------------------------
// [CHOSEN, not recovered] Every Ra permutation compiles its whole effect text, so equal shaders
// reach the compiler as distinct inputs (Devils_Perch: 7,314 compiled jobs, 941 distinct
// bytecodes). For a request D3DX accepts, a shader job compiles trimSource(): the text reduced to
// what the entry point reaches (its functions, the global declarations they name; structs,
// typedefs, prototypes and register-reserved globals always stay) with whitespace runs collapsed. The output is a function
// of that text, so equal trimmed texts share one compile (memo below). Unknown requests keep the
// full-text compile for D3DX-like diagnostics. tools/macos/nativefx `memo-guard` checks offline
// that full and trimmed texts give the same bytecode and outcome.
std::vector<std::pair<size_t, size_t>> identifiers(const std::string& t, size_t b, size_t e) {
    std::vector<std::pair<size_t, size_t>> out;
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    auto alnum = [&](char c) { return alpha(c) || (c >= '0' && c <= '9'); };
    for (size_t i = b; i < e;) {
        char c = t[i];
        if (alpha(c)) { size_t j = i; while (j < e && alnum(t[j])) ++j; out.push_back({i, j - i}); i = j; }
        else if (c >= '0' && c <= '9') { while (i < e && (alnum(t[i]) || t[i] == '.')) ++i; }
        else ++i;
    }
    return out;
}
} // namespace

bool trimSource(const std::string& text, const std::string& entry, std::string& out) {
    src::Effect fx; src::Error perr;
    if (!src::parse(text, fx, perr)) return false;
    std::map<std::string, std::vector<size_t>> functions, globals;
    for (size_t i = 0; i < fx.functions.size(); ++i) if (fx.functions[i].hasBody) functions[fx.functions[i].name].push_back(i);
    for (size_t i = 0; i < fx.globals.size(); ++i) globals[fx.globals[i].name].push_back(i);
    if (!functions.count(entry)) return false;
    std::vector<char> keepFunction(fx.functions.size(), 0), keepGlobal(fx.globals.size(), 0);
    std::vector<std::pair<size_t, size_t>> work;
    auto useName = [&](const std::string& n) {
        auto f = functions.find(n);
        if (f != functions.end()) for (size_t i : f->second) if (!keepFunction[i]) {
            keepFunction[i] = 1; work.push_back({fx.functions[i].begin, fx.functions[i].end});
        }
        auto g = globals.find(n);
        if (g != globals.end()) for (size_t i : g->second) if (!keepGlobal[i]) {
            // One statement can declare several globals: keep (and scan) it whole.
            for (size_t k = 0; k < fx.globals.size(); ++k)
                if (fx.globals[k].begin == fx.globals[i].begin && fx.globals[k].end == fx.globals[i].end) keepGlobal[k] = 1;
            work.push_back({fx.globals[i].begin, fx.globals[i].end});
        }
    };
    for (const auto& st : fx.structs) work.push_back({st.begin, st.end}); // kept: their names reach globals
    // A register reservation binds its register whether or not this shader reads the global; the
    // allocation of every other uniform depends on it, so such declarations always stay.
    for (const auto& g : fx.globals)
        for (const auto& id : identifiers(text, g.begin, std::min(g.end, text.size())))
            if (text.compare(id.first, id.second, "register") == 0) { useName(g.name); break; }
    useName(entry);
    while (!work.empty()) {
        auto r = work.back(); work.pop_back();
        for (const auto& id : identifiers(text, r.first, std::min(r.second, text.size()))) useName(text.substr(id.first, id.second));
    }
    std::string t = text;
    auto blank = [&](size_t b, size_t e) { for (size_t i = b; i < e && i < t.size(); ++i) if (t[i] != '\n') t[i] = ' '; };
    for (size_t i = 0; i < fx.functions.size(); ++i) if (fx.functions[i].hasBody && !keepFunction[i]) blank(fx.functions[i].begin, fx.functions[i].end);
    for (size_t i = 0; i < fx.globals.size(); ++i) if (!keepGlobal[i]) blank(fx.globals[i].begin, fx.globals[i].end);
    out.clear(); out.reserve(t.size() / 2);
    bool space = false;
    for (char c : t) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') { space = !out.empty(); continue; }
        if (space) { out.push_back(' '); space = false; }
        out.push_back(c);
    }
    return true;
}

namespace {
struct MemoResult { bool ok = false; Bytes code; std::string error; };
std::mutex gMemoMutex;
std::map<std::string, std::shared_future<MemoResult>> gMemo; // trimmed-text key -> outcome
std::atomic<unsigned> gMemoHits{0}, gMemoMisses{0};

bool compileFull(const ShaderJob& job, const std::string& source, Bytes& bytecode, std::string& error);

struct Backend final : ShaderBackend {
    bool memo = false; // the request is D3DX-accepted: compile the trimmed text, share outcomes
    bool compile(const ShaderJob& job, const std::string& text, Bytes& bytecode, std::string& error) override {
        if (!job.asmText.empty() || job.function.empty()) {
            if (!assemble(job.asmText, bytecode, error)) return false;
            Shader probe; probe.bytecode = bytecode; probe.entry = "ShaderFunction";
            return reflect(probe, {}, error);
        }
        std::string trimmed;
        if (!memo || !trimSource(text + job.wrapper, job.entry, trimmed)) return compileFull(job, text + job.wrapper, bytecode, error);
        Writer w;
        w(std::string("BF2FX-shader-memo"), uint32_t(1), job.profile, job.entry, job.function, sha256(Bytes(trimmed.begin(), trimmed.end())));
        const std::string key = sha256(w.bytes);
        std::promise<MemoResult> mine;
        std::shared_future<MemoResult> f;
        bool owner = false;
        {
            std::lock_guard<std::mutex> lock(gMemoMutex);
            auto it = gMemo.find(key);
            if (it != gMemo.end()) f = it->second;
            else { f = mine.get_future().share(); gMemo[key] = f; owner = true; }
        }
        if (owner) {
            ++gMemoMisses;
            MemoResult r; r.ok = compileFull(job, trimmed, r.code, r.error);
            mine.set_value(r);
        } else ++gMemoHits;
        const MemoResult& r = f.get();
        bytecode = r.code; error = r.error;
        return r.ok;
    }
    bool reflect(Shader& shader, const std::vector<SamplerSlot>& samplers, std::string& error) override {
        return native::reflect(shader, samplers, error);
    }
};
bool compileFull(const ShaderJob& job, const std::string& source, Bytes& bytecode, std::string& error) {
        std::string first;
        for (const auto& profile : ladder(job.profile)) {
            std::string log, e;
            if (compileShader(source, job.entry, profile, bytecode, log) < 0) e = firstError(log);
            else {
                Shader probe; probe.bytecode = bytecode; probe.entry = "ShaderFunction";
                if (native::reflect(probe, {}, e)) return true;
                e = "MojoShader: " + e;
            }
            if (first.empty()) first = job.profile + " " + job.function + ": " + e;
        }
        error = first;
        return false;
}
// Compiles every shader job of an effect in parallel first; the builder then reads the results.
struct Parallel final : ShaderBackend {
    bool memo = false; // passed to every Backend (see Backend::memo)
    struct Result { bool ok = false; Bytes code; std::string error; };
    std::map<std::string, Result> done;
    static std::string id(const ShaderJob& job) { return std::to_string(job.stage) + "|" + job.key; }
    void prepare(const std::string& text) {
        src::Effect fx; src::Error perr;
        if (!src::parse(text, fx, perr)) return;
        std::vector<std::pair<uint32_t, const src::Expr*>> values;
        std::function<void(const src::Expr&, uint32_t)> collect = [&](const src::Expr& e, uint32_t stage) {
            if (e.kind == src::Expr::Compile || e.kind == src::Expr::Asm) values.push_back({stage, &e});
            else if (e.kind == src::Expr::List) for (const auto& a : e.args) collect(*a, stage);
        };
        for (const auto& g : fx.globals) if (g.init && (g.type.base == src::Base::VertexShader || g.type.base == src::Base::PixelShader))
            collect(*g.init, g.type.base == src::Base::VertexShader ? VertexShader : PixelShader);
        for (const auto& t : fx.techniques) for (const auto& p : t.passes) for (const auto& st : p.states) {
            if (src::iequals(st.name, "VertexShader")) collect(*st.value, VertexShader);
            if (src::iequals(st.name, "PixelShader")) collect(*st.value, PixelShader);
        }
        std::vector<ShaderJob> jobs;
        for (const auto& v : values) {
            ShaderJob job; std::string error;
            if (shaderJob(fx, text, *v.second, v.first, job, error) && !done.count(id(job))) { done[id(job)]; jobs.push_back(job); }
        }
        const std::string texts[2] = {compilerText(fx, text, PixelShader), compilerText(fx, text, VertexShader)};
        std::atomic<size_t> next{0};
        auto work = [&] {
            Backend backend; backend.memo = memo;
            for (size_t i; (i = next++) < jobs.size();) {
                auto& r = done[id(jobs[i])]; // Pre-inserted: no rehash while workers run.
                r.ok = backend.compile(jobs[i], texts[jobs[i].stage == VertexShader ? 1 : 0], r.code, r.error);
            }
        };
        unsigned n = std::min<size_t>(std::max(1u, std::thread::hardware_concurrency()), jobs.size());
        std::vector<std::thread> threads;
        for (unsigned i = 1; i < n; ++i) threads.emplace_back(work);
        work();
        for (auto& t : threads) t.join();
    }
    bool compile(const ShaderJob& job, const std::string& text, Bytes& bytecode, std::string& error) override {
        auto it = done.find(id(job));
        if (it == done.end()) { Backend b; b.memo = memo; return b.compile(job, text, bytecode, error); }
        bytecode = it->second.code; error = it->second.error;
        return it->second.ok;
    }
    bool reflect(Shader& shader, const std::vector<SamplerSlot>& samplers, std::string& error) override {
        return native::reflect(shader, samplers, error);
    }
};
// Skeleton backend: no code, the records stay empty until the deferred compile.
struct Deferred final : ShaderBackend {
    bool compile(const ShaderJob&, const std::string&, Bytes& bytecode, std::string&) override { bytecode.clear(); return true; }
    bool reflect(Shader&, const std::vector<SamplerSlot>&, std::string&) override { return true; }
};

void quiet() {
    static std::once_flag once;
    std::call_once(once, [] { setenv("VKD3D_SHADER_DEBUG", "none", 0); });
}

const char kTag[] = "BF2FX-native-request\0";

// ---- Queue -----------------------------------------------------------------------------------
struct Job {
    std::string key, text;
    Request request;
    Done done;
    std::promise<Entry> promise;
    std::shared_future<Entry> future;
    bool started = false;
};
class Queue {
public:
    static Queue& get() { static Queue* q = new Queue(); return *q; } // Never destroyed.
    std::shared_future<Entry> submit(const std::string& key, const Request& r, std::string text, Done done) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = jobs_.find(key);
        if (it != jobs_.end()) return it->second->future;
        auto job = std::make_shared<Job>();
        job->key = key; job->request = r; job->text = std::move(text); job->done = std::move(done);
        job->future = job->promise.get_future().share();
        jobs_[key] = job; order_.push_back(job);
        start();
        cv_.notify_one();
        return job->future;
    }
    Entry wait(const std::string& key, const std::shared_future<Entry>& future) {
        std::shared_ptr<Job> job;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = jobs_.find(key);
            if (it != jobs_.end() && !it->second->started) { job = it->second; job->started = true; }
        }
        if (job) run(job);
        return future.get();
    }
    size_t pending() { std::lock_guard<std::mutex> lock(mutex_); return jobs_.size(); }
private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::map<std::string, std::shared_ptr<Job>> jobs_;
    std::deque<std::shared_ptr<Job>> order_;
    bool started_ = false;
    void start() {
        if (started_) return;
        started_ = true;
        unsigned n = std::max(1u, std::thread::hardware_concurrency());
        if (n > 1) --n; // Leave the engine thread a core.
        if (const char* e = getenv("BF2_FX_NATIVE_THREADS")) n = std::max(1, atoi(e));
        for (unsigned i = 0; i < n; ++i) std::thread([this] { worker(); }).detach();
    }
    void worker() {
        for (;;) {
            std::shared_ptr<Job> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return !order_.empty(); });
                job = order_.front(); order_.pop_front();
                if (job->started) continue;
                job->started = true;
            }
            run(job);
        }
    }
    void run(const std::shared_ptr<Job>& job) {
        Entry e = native::compile(job->request, job->text.empty() ? nullptr : &job->text);
        if (job->done) job->done(e);
        job->promise.set_value(e);
        std::lock_guard<std::mutex> lock(mutex_);
        jobs_.erase(job->key);
    }
};
} // namespace

const std::string& identity() {
    static const std::string id = std::string("bf2fx-native-") + BF2_FX_NATIVE_IDENTITY;
    return id;
}
const std::string& identityHash() {
    static const std::string h = sha256(Bytes(identity().begin(), identity().end()));
    return h;
}

std::string refusalKey(const Request& r) {
    // Toolchain-independent (no compiler identity, no cache version): the request as D3DX saw it.
    Writer w;
    w(std::string("BF2FX-d3dx-refusal-key"), uint32_t(1), sha256(r.source), r.defines, r.flags);
    w(uint32_t(r.includes.size()));
    for (const auto& i : r.includes) w(i.parent, i.kind, i.found, i.name, sha256(i.bytes));
    return sha256(w.bytes);
}
Known known(const Request& r) {
    const uint64_t k = std::strtoull(refusalKey(r).substr(0, 16).c_str(), nullptr, 16);
    const auto* end = kTable + sizeof(kTable) / sizeof(kTable[0]);
    const auto* it = std::lower_bound(kTable, end, k, [](const TableEntry& a, uint64_t b) { return a.key < b; });
    if (it == end || it->key != k) return Known::Unknown;
    return it->state == 'R' ? Known::Refused : it->state == 'A' ? Known::Accepted : Known::Unknown;
}
bool ruleRefuses(const Request& r) {
    for (const auto& d : r.defines) if (d.name == "PSMODEL" && d.value.rfind("ps_1_", 0) == 0) return true;
    return false;
}

bool preprocess(const Request& r, std::string& text, std::string& log) {
    quiet();
    return native::preprocessSource(r, text, log);
}

bool skeleton(const std::string& text, Entry& out, std::string& error) {
    Deferred backend;
    BuildReport report;
    out = Entry();
    out.kind = EffectEntry;
    return build(text, backend, out.description, error, &report);
}

Entry compile(const Request& r, const std::string* pre) {
    quiet();
    Entry e;
    e.key = cacheKey(r);
    e.fx2 = tag(r);
    std::string text, log, error;
    if (pre) text = *pre;
    else if (!native::preprocessSource(r, text, log)) {
        e.kind = RefusedEntry; e.parseResult = 0x80004005u; e.log = "BF2FX native: preprocessing failed\n" + log;
        return e;
    }
    Parallel backend;
    backend.memo = known(r) == Known::Accepted;
    backend.prepare(text);
    BuildReport report;
    // [TEMP] Test hook (not a user setting): BF2_FX_TEST_HIGHER_VERTEX_PROFILE=1 treats every
    // request as D3DX-accepted for this fallback, so the harness can cover it with its own sources;
    // =0 turns the fallback off (route A/B). Unset: the table decides.
    const char* testEnv = std::getenv("BF2_FX_TEST_HIGHER_VERTEX_PROFILE");
    const bool testHigher = testEnv && *testEnv == '1';
    report.higherVertexProfile = testHigher || known(r) == Known::Accepted;
    if (testEnv && *testEnv == '0') report.higherVertexProfile = false;
    if (build(text, backend, e.description, error, &report)) return e;
    e.fx2.clear(); // Only an effect hands CompileEffect a buffer.
    if (error == "There were no techniques") {
        // D3DXCreateEffectCompiler parses `.mfx` metadata for reflection; CompileEffect fails.
        BuildReport reflection; reflection.allowNoTechniques = true;
        e.description = Description();
        if (build(text, backend, e.description, error, &reflection)) {
            e.kind = ReflectionOnlyEntry; e.compileResult = 0x80004005u; e.log = "There were no techniques\n";
            return e;
        }
    }
    e.kind = RefusedEntry; e.compileResult = 0x80004005u; e.description = Description();
    e.log = "BF2FX native: " + error + "\n";
    return e;
}

Bytes tag(const Request& r) {
    Bytes out(kTag, kTag + sizeof kTag - 1);
    Bytes body = envelope(r, "request");
    out.insert(out.end(), body.begin(), body.end());
    return out;
}
bool untag(const void* data, size_t size, Request& r) {
    const size_t n = sizeof kTag - 1;
    if (!data || size < n || std::memcmp(data, kTag, n)) return false;
    auto p = static_cast<const uint8_t*>(data);
    try { r = unenvelope<Request>(Bytes(p + n, p + size), "request"); } catch (...) { return false; }
    return true;
}

std::shared_future<Entry> submit(const std::string& key, const Request& r, std::string text, Done done) {
    return Queue::get().submit(key, r, std::move(text), std::move(done));
}
Entry wait(const std::string& key, const std::shared_future<Entry>& f) { return Queue::get().wait(key, f); }
size_t pending() { return Queue::get().pending(); }
void memoCounts(unsigned& hits, unsigned& misses) { hits = gMemoHits; misses = gMemoMisses; }
bool compileJob(const ShaderJob& job, const std::string& source, Bytes& bytecode, std::string& error) {
    return compileFull(job, source, bytecode, error);
}

} // namespace native
} // namespace bf2fx
#endif
