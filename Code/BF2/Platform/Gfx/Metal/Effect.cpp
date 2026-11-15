// [CHOSEN, not recovered] Owner-requested macOS port: native ID3DXEffect, ID3DXEffectCompiler
// and D3DXCreateEffect* over native compilation, the FxNative cache and the s2 CPU runtime.
// Semantics follow the public D3DX9 API reference; nothing
// here is recovered from the retail image and no D3DX/Wine implementation was copied.
#if defined(__APPLE__) && defined(BF2_IMAGE_RENDDX9)
#include "../../macOS/HitchTrace.h" // [CHOSEN] BF2_HITCH_TRACE (port addition, measurement)
#include "../../macOS/Warmup.h"   // [CHOSEN] +warmupLevel progress (port addition)
#include "../D3DX9Subset.h"
#include "../Effect/Runtime.h"
#include "EffectShaders.h"
#include "RefTrace.h" // [TEMP] [RETIRE-WHEN: rend-deinit-reftrace]
#include "../Unsupported.h"
#if !defined(BF2_FX_NATIVE)
#error "The macOS effect facade requires the native compiler (BF2_FX_NATIVE)"
#endif
#include "../Effect/NativeMojo.h"
#include "../Effect/Native.h"
#include <future>
#include <mutex>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <tuple>

namespace {
constexpr DWORD D3DXFX_DONOTSAVESTATE = 1;
constexpr DWORD D3DXFX_DONOTSAVESHADERSTATE = 2;
constexpr DWORD D3DXFX_DONOTSAVESAMPLERSTATE = 4;
constexpr HRESULT kCompileFailure = E_FAIL; // D3DX's result for a failed compile.

// [CHOSEN, not recovered] Inputs retail BF2 cannot select:
// each takes the fallback its call site names, logged once (Unsupported.h), and the
// call answers hr.
HRESULT refuse(const char* method, const std::string& detail = std::string(), HRESULT hr = D3DERR_INVALIDCALL)
{
    bf2gfx::unsupported(std::string(method) + (detail.empty() ? "" : ": ") + detail);
    return hr;
}

bool sameGuid(REFIID a, const GUID& b) { return !std::memcmp(&a, &b, sizeof(GUID)); }
const GUID kIidUnknown = {0, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
const GUID kIidBuffer = {0x8ba5fb08, 0x5195, 0x40e2, {0xac, 0x58, 0x0d, 0x98, 0x9c, 0x3a, 0x01, 0x02}};

// Private ID3DXBuffer: compiled FX2 bytes and diagnostics. D3DXCreateBuffer belongs to g3c.
class Blob final : public ID3DXBuffer {
public:
    ULONG refs = 1;
    std::vector<std::uint8_t> bytes;
    explicit Blob(std::vector<std::uint8_t> b) : bytes(std::move(b)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return E_POINTER_VALUE();
        *out = nullptr;
        if (!sameGuid(id, kIidUnknown) && !sameGuid(id, kIidBuffer)) return E_NOINTERFACE;
        *out = static_cast<ID3DXBuffer*>(this); ++refs; return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = --refs; if (!n) delete this; return n; }
    void* STDMETHODCALLTYPE GetBufferPointer() override { return bytes.empty() ? nullptr : bytes.data(); }
    DWORD STDMETHODCALLTYPE GetBufferSize() override { return DWORD(bytes.size()); }
    static HRESULT E_POINTER_VALUE() { return static_cast<HRESULT>(0x80004003u); }
};
ID3DXBuffer* textBlob(const std::string& text)
{
    std::vector<std::uint8_t> bytes(text.begin(), text.end());
    bytes.push_back(0);
    return new (std::nothrow) Blob(std::move(bytes));
}

// [CHOSEN, not recovered] D3DX effect pools: parameters declared `shared` keep one value across
// every effect created with the same pool. The pool stores each shared parameter's type and
// current value. An effect writes a changed shared value through to the pool, and reads the pool's
// values back before it next uses its parameters (version counts the pool's changes).
struct PoolStore {
    struct Shared {
        bf2fx::Type type;                            // the type every sharing effect declares
        std::vector<std::uint8_t> bytes;             // numeric/struct value, GetValue layout
        std::vector<std::shared_ptr<void>> textures; // texture parameters, one per element
    };
    std::map<std::string, Shared> parameters;
    std::uint64_t version = 1;
};
const GUID kIidEffectPool = {0x9537ab04, 0x3250, 0x412e, {0x82, 0x13, 0xfc, 0xd2, 0xf8, 0x67, 0x79, 0x33}};
class Pool final : public ID3DXEffectPool {
public:
    ULONG refs = 1;
    std::shared_ptr<PoolStore> store = std::make_shared<PoolStore>();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return static_cast<HRESULT>(0x80004003u); // E_POINTER
        *out = nullptr;
        if (!sameGuid(id, kIidUnknown) && !sameGuid(id, kIidEffectPool)) return E_NOINTERFACE;
        *out = static_cast<ID3DXEffectPool*>(this); ++refs; return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = --refs; if (!n) delete this; return n; }
};
// Type agreement of two declarations of one shared parameter (class, type, shape, members).
bool sameType(const bf2fx::Type& a, const bf2fx::Type& b)
{
    if (a.type != b.type || a.cls != b.cls || a.rows != b.rows || a.columns != b.columns ||
        a.elements != b.elements || a.members.size() != b.members.size()) return false;
    for (std::size_t i = 0; i < a.members.size(); ++i)
        if (a.members[i].name != b.members[i].name || !sameType(a.members[i], b.members[i])) return false;
    return true;
}

struct IncludeAdapter final : bf2fx::IncludeResolver {
    ID3DXInclude* include;
    explicit IncludeAdapter(ID3DXInclude* i) : include(i) {}
    bool open(std::uint32_t kind, const std::string& name, const void* parent,
              const void*& bytes, std::size_t& size) override {
        // [CHOSEN, not recovered] A null include resolves nothing: D3DX's default file include is
        // not emulated, so an unresolved include is recorded as absent in the request.
        if (!include) return false;
        LPCVOID data = nullptr; UINT n = 0;
        if (FAILED(include->Open(D3DXINCLUDE_TYPE(kind), name.c_str(), parent, &data, &n))) return false;
        bytes = data; size = n; return true;
    }
    void close(const void* bytes) override { include->Close(bytes); }
};
// A native refusal or reflection-only record preserves the compiler failure contract.
HRESULT failAsD3dx(const bf2fx::Entry& entry, bool compilerObject, ID3DXBuffer** errors)
{
    const HRESULT hr = HRESULT(entry.parseResult ? entry.parseResult : entry.compileResult);
    if (compilerObject && !entry.parseResult) return D3D_OK; // CompileEffect reports compileResult
    if (errors && !entry.log.empty()) *errors = textBlob(entry.log);
    return hr;
}
// A natively compiled effect whose shaders are still compiling in the background.
struct Deferred {
    std::string key;
    std::shared_future<bf2fx::Entry> future;
    explicit operator bool() const { return future.valid(); }
};
// ---- Native compilation -----------------------------------------
// [CHOSEN, not recovered] The native compiler is the only effect route. Its toolchain
// identity keys the per-user cache; unsupported effects retain native refusal/frontier handling.
std::string nativeRoot()
{
    if (const char* e = std::getenv("BF2_FX_NATIVE_CACHE")) if (*e) return e;
    const char* home = std::getenv("HOME");
    return home && *home ? std::string(home) + "/Library/Application Support/OpenBF2/FxNative" : std::string();
}
struct NativeStats { unsigned hits = 0, refusedTable = 0, refusedRule = 0, deferred = 0, synchronous = 0, unknown = 0; };
NativeStats& nativeStats() { static NativeStats s; return s; }
void nativeReport(const char* what)
{
    if (!std::getenv("BF2_FX_NATIVE_TRACE")) return;
    const auto& s = nativeStats();
    unsigned memoHits = 0, memoMisses = 0;
    bf2fx::native::memoCounts(memoHits, memoMisses);
    std::fprintf(stderr, "BF2FX native: %s (hits %u, refused by table %u, by rule %u, deferred %u, compiled now %u, unknown %u, queue %zu, shader memo %u/%u)\n",
                 what, s.hits, s.refusedTable, s.refusedRule, s.deferred, s.synchronous, s.unknown, bf2fx::native::pending(),
                 memoHits, memoHits + memoMisses);
}
// BF2_HITCH_TRACE: one record per effect request.
struct RequestTrace {
    const char* outcome = "failed";
    std::string key;
    const bool armed = bf2hitch::enabled();
    std::chrono::steady_clock::time_point start;
    RequestTrace() { if (armed) start = std::chrono::steady_clock::now(); }
    ~RequestTrace() {
        if (!armed) return;
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        bf2hitch::add(bf2hitch::FxRequest, ms, outcome);
        bf2hitch::request(outcome, ms, key.c_str());
    }
};
// Keys whose skeleton was handed out while their full build is queued.
std::mutex gPendingMutex;
std::map<std::string, std::pair<std::shared_ptr<const bf2fx::Entry>, std::shared_future<bf2fx::Entry>>> gPending;

HRESULT nativeEntry(const char* api, LPCVOID source, UINT size, const std::vector<bf2fx::Define>& defines,
                    ID3DXInclude* include, DWORD flags, bf2fx::Entry& entry, ID3DXBuffer** errors, Deferred& deferred)
{
    namespace nt = bf2fx::native;
    RequestTrace trace;
    // [CHOSEN, not recovered] Host failures (E17) do not end the run: without a cache directory
    // (no BF2_FX_NATIVE_CACHE or HOME) every effect is compiled in memory and nothing is cached;
    // an unreadable cache entry is rebuilt; a failed publish keeps the built entry for this run.
    const std::string root = nativeRoot();
    if (root.empty()) refuse(api, "no native effect cache directory (set BF2_FX_NATIVE_CACHE or HOME); compiling uncached");
    bf2fx::Request request;
    if (!nt::untag(source, size, request)) { // A CompileEffect result (Ra, .cfx) carries its request.
        IncludeAdapter adapter(include);
        const auto result = bf2fx::capture(source, size, defines, flags, nt::identityHash(), nt::identity(), &adapter, request);
        if (!result) { // D3DX reports a source it cannot read as a failed compile with a message.
            refuse(api, "request capture failed: " + result.detail);
            if (errors) *errors = textBlob(api + std::string(": ") + result.detail + "\n");
            return kCompileFailure;
        }
    }
    auto publish = [&](const bf2fx::Entry& e) {
        if (root.empty()) return;
        const auto published = bf2fx::publish(root, request, e);
        if (!published) refuse(api, "native cache publish (entry kept for this run): " + published.detail);
    };
    const bool compiler = std::strcmp(api, "D3DXCreateEffectCompiler") == 0;
    const std::string key = bf2fx::cacheKey(request);
    trace.key = key;
    {
        std::lock_guard<std::mutex> lock(gPendingMutex);
        auto it = gPending.find(key);
        if (it != gPending.end()) {
            entry = *it->second.first; deferred = {key, it->second.second}; ++nativeStats().deferred;
            trace.outcome = "pending";
            return D3D_OK;
        }
    }
    auto finish = [&](const bf2fx::Entry& e) -> HRESULT {
        if (e.kind == bf2fx::EffectEntry) { entry = e; return D3D_OK; }
        entry = e;
        return failAsD3dx(entry, compiler, errors);
    };
    if (!root.empty()) {
        bf2fx::Result result = bf2fx::load(root, request, entry);
        if (result || result.error == bf2fx::Error::Refused) {
            ++nativeStats().hits; nativeReport("cache hit");
            trace.outcome = result ? "hit" : "hit-refused";
            return finish(entry);
        }
        if (result.error != bf2fx::Error::Miss) refuse(api, "native cache " + root + " (rebuilding the entry): " + result.detail);
    }

    const auto known = nt::known(request);
    if (known == nt::Known::Unknown) ++nativeStats().unknown;
    if (known == nt::Known::Refused || (known == nt::Known::Unknown && nt::ruleRefuses(request))) {
        bf2fx::Entry refused;
        refused.key = key; refused.kind = bf2fx::RefusedEntry; refused.compileResult = std::uint32_t(kCompileFailure);
        refused.log = known == nt::Known::Refused ? "BF2FX native: D3DX refuses this request (refusal table)\n"
                                                  : "BF2FX native: refused below ps_2_0 (Rule R, source unknown to the refusal table)\n";
        ++(known == nt::Known::Refused ? nativeStats().refusedTable : nativeStats().refusedRule);
        publish(refused);
        trace.outcome = "refused";
        return finish(refused);
    }
    std::string text, log;
    if (known == nt::Known::Accepted && nt::preprocess(request, text, log)) {
        // D3DX accepted this exact request: hand out the structure now, compile the shaders in
        // the background; the first pass that draws waits for them (Effect::ensureShaders).
        bf2fx::Entry skeleton;
        std::string error;
        if (nt::skeleton(text, skeleton, error)) {
            skeleton.key = key; skeleton.fx2 = nt::tag(request);
            auto shared = std::make_shared<const bf2fx::Entry>(skeleton);
            std::shared_future<bf2fx::Entry> future;
            {
                std::lock_guard<std::mutex> lock(gPendingMutex);
                bf2warmup::effectStarted();                   // [CHOSEN] +warmupLevel progress
                const auto queued = std::chrono::steady_clock::now();
                future = nt::submit(key, request, std::move(text), [root, request, key, queued](const bf2fx::Entry& e) {
                    // BF2_HITCH_TRACE: the build, from its submission (the queue wait included).
                    if (bf2hitch::enabled())
                        bf2hitch::build(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - queued).count(), key.c_str());
                    // [CHOSEN, not recovered] D3DX accepts this request, so a failed native build is a
                    // toolchain fault, not a D3DX refusal. Publishing it turned this run's working
                    // skeleton into a refused CreateEffect on the next run (an effect without
                    // techniques); now every run takes this path and ensureShaders stops loudly.
                    if (e.kind != bf2fx::EffectEntry) {
                        std::fprintf(stderr, "BF2FX native: build of accepted request %s failed, not cached: %s",
                                     key.c_str(), e.log.c_str());
                    } else if (!root.empty()) {
                        const auto r = bf2fx::publish(root, request, e);
                        if (!r) std::fprintf(stderr, "BF2FX native: publish %s: %s\n", key.c_str(), r.detail.c_str());
                    }
                    {
                        std::lock_guard<std::mutex> lock(gPendingMutex);
                        gPending.erase(key);
                    }
                    bf2warmup::effectFinished(e.kind == bf2fx::EffectEntry);   // after publish
                });
                gPending[key] = {shared, future};
            }
            entry = skeleton; deferred = {key, future}; ++nativeStats().deferred;
            nativeReport("deferred");
            trace.outcome = "deferred";
            return D3D_OK;
        }
    }
    // Unknown source (or a skeleton that does not build): compile now, D3DX-like errors.
    bf2warmup::effectStarted();
    bf2fx::Entry built = nt::compile(request, text.empty() ? nullptr : &text);
    ++nativeStats().synchronous;
    publish(built);
    bf2warmup::effectFinished(true);   // a synchronous build is the D3DX-like outcome either way
    if (built.kind == bf2fx::RefusedEntry) std::fprintf(stderr, "%s: %s", api, built.log.c_str());
    nativeReport("compiled");
    trace.outcome = "compiled";
    return finish(built);
}
HRESULT loadEntry(const char* api, LPCVOID source, UINT size, const D3DXMACRO* macros, ID3DXInclude* include,
                  DWORD flags, bf2fx::Entry& entry, ID3DXBuffer** errors, Deferred& deferred)
{
    if (errors) *errors = nullptr;
    std::vector<bf2fx::Define> defines;
    for (const D3DXMACRO* m = macros; m && m->Name; ++m) defines.push_back({m->Name, m->Definition ? m->Definition : ""});
    return nativeEntry(api, source, size, defines, include, flags, entry, errors, deferred);
}

// ---- Value helpers for annotations and state values ------------------------------------------
float bitsFloat(std::uint32_t w) { float f; std::memcpy(&f, &w, 4); return f; }
std::uint32_t floatBits(float f) { std::uint32_t w; std::memcpy(&w, &f, 4); return w; }
double number(std::uint32_t w, std::uint32_t type)
{
    return type == bf2fx::Float ? double(bitsFloat(w)) : type == bf2fx::Bool ? double(w != 0) : double(std::int32_t(w));
}
std::int32_t toInt(double v) { return !std::isfinite(v) || v < INT32_MIN || v >= 2147483648.0 ? INT32_MIN : std::int32_t(v); }
// D3D state DWORD from an FX2 value: float-typed states take float bits, others integers.
DWORD stateWord(const bf2fx::Value& v, bool floatState)
{
    // [CHOSEN, not recovered] A state value without data (no shipped pass has one, E16) sets 0.
    if (v.words.empty()) { refuse("ID3DXEffect::BeginPass", "state value without data (0 used): " + v.type.name); return 0; }
    const double n = number(v.words[0], v.type.type);
    return floatState ? floatBits(float(n)) : DWORD(toInt(n));
}
bool numericType(const bf2fx::Type& t) { return t.type >= bf2fx::Bool && t.type <= bf2fx::Float && t.cls <= 3; }
std::vector<std::size_t> valueComponents(const bf2fx::Value& v)
{
    std::vector<std::size_t> out;
    if (!numericType(v.type)) return out;
    const std::uint32_t elements = std::max(1u, v.type.elements);
    for (std::uint32_t e = 0; e < elements; ++e)
        for (std::uint32_t r = 0; r < v.type.rows; ++r)
            for (std::uint32_t c = 0; c < v.type.columns; ++c) out.push_back((e * v.type.rows + r) * 4 + c);
    return out;
}
// Reads n components of an immutable annotation value, converted to type.
bool readValue(const bf2fx::Value& v, std::uint32_t type, void* data, std::size_t n)
{
    const auto c = valueComponents(v);
    if (c.empty() || n > c.size() || (!data && n) || c.back() >= v.words.size()) return false;
    auto* p = static_cast<std::uint8_t*>(data);
    for (std::size_t i = 0; i < n; ++i) {
        const double x = number(v.words[c[i]], v.type.type);
        const std::uint32_t w = type == bf2fx::Float ? floatBits(float(x)) : type == bf2fx::Bool ? std::uint32_t(x != 0) : std::uint32_t(toInt(x));
        std::memcpy(p + i * 4, &w, 4);
    }
    return true;
}

// FX2 state table (D3DX9 effect binary), mapped to the public D3D9 enums.
const unsigned kRenderStates[103] = {
    7, 8, 9, 14, 15, 16, 19, 20, 22, 23, 24, 25, 26, 27, 28, 29,
    34, 35, 36, 37, 38, 48, 52, 53, 54, 55, 56, 57, 58, 59, 60,
    128, 129, 130, 131, 132, 133, 134, 135, 198, 199, 200, 201, 202, 203, 204, 205,
    136, 137, 139, 140, 141, 142, 143, 145, 146, 147, 148, 151, 152, 154, 155, 156, 157, 158, 159, 160,
    161, 162, 163, 165, 166, 167, 168, 170, 171, 172, 173, 174, 175, 176, 178, 179, 180, 181, 182, 183,
    184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 206, 207, 208, 209};
const unsigned kStageStates[18] = {1, 26, 2, 3, 4, 27, 5, 6, 28, 7, 8, 9, 10, 11, 22, 23, 24, 32};
bool floatRenderState(unsigned s)
{
    switch (s) {
    case 36: case 37: case 38: case 154: case 155: case 158: case 159: case 160: case 166: case 170:
    case 175: case 178: case 179: case 180: case 181: case 182: case 183: case 195: return true;
    default: return false;
    }
}
bool floatStageState(unsigned s) { return (s >= 7 && s <= 10) || s == 22 || s == 23; }

// ---- Handles --------------------------------------------------------------------------------
// [CHOSEN, not recovered] D3DXHANDLEs are addresses inside one private block per effect, so a
// pointer that is not in the block is read as a name string, as the D3DX API permits.
struct HandleInfo {
    enum Kind { Parameter, Annotation, Technique, Pass, AnnotationElement } kind;
    std::uint32_t a = 0, b = UINT32_MAX, c = UINT32_MAX, d = UINT32_MAX; // parameter, element, member, member element
    const bf2fx::Value* value = nullptr; // annotations
};
struct Annotated { std::uint32_t first = 0, count = 0; };

template<class I> class EffectBase : public I {
public:
    ULONG refs = 1;
    bf2fx::Runtime runtime;
    std::shared_ptr<const bf2fx::Description> shared; // shader records referenced by the device
    std::vector<std::uint8_t> fx2;
    std::uint32_t flags;
    std::vector<HandleInfo> infos;
    std::unique_ptr<char[]> block;
    std::vector<std::uint32_t> parameterBase, techniqueBase;
    std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>, std::uint32_t> parameterIndex;
    std::map<std::uint32_t, Annotated> annotations; // owner handle index -> annotation handles
    std::uint64_t generation = 1;
    std::vector<std::vector<std::uint32_t>> arrayRange;
    std::map<std::uint32_t, std::uint32_t> annotationElements; // annotation handle -> first element

    // Natively compiled effects fill their shader records later (Effect::ensureShaders).
    virtual void ensureShaders() {}
    EffectBase(bf2fx::Entry& entry, DWORD f)
        : runtime(entry.description),
          shared(std::make_shared<const bf2fx::Description>(std::move(entry.description))),
          fx2(std::move(entry.fx2)), flags(f) {
        const auto& d = runtime.description();
        auto annotate = [&](std::uint32_t owner, const std::vector<bf2fx::Value>& list) {
            annotations[owner] = {std::uint32_t(infos.size()), std::uint32_t(list.size())};
            for (const auto& v : list) { HandleInfo h{HandleInfo::Annotation}; h.value = &v; infos.push_back(h); }
        };
        std::vector<std::pair<std::uint32_t, const std::vector<bf2fx::Value>*>> pending;
        for (std::uint32_t i = 0; i < d.parameters.size(); ++i) {
            parameterBase.push_back(std::uint32_t(infos.size()));
            HandleInfo h{HandleInfo::Parameter}; h.a = i; infos.push_back(h);
            pending.push_back({parameterBase.back(), &d.parameters[i].annotations});
            parameterIndex[std::make_tuple(i, UINT32_MAX, UINT32_MAX, UINT32_MAX)] = parameterBase.back();
            const auto& type = d.parameters[i].value.type;
            auto add = [&](std::uint32_t e, std::uint32_t m) {
                parameterIndex[std::make_tuple(i, e, m, UINT32_MAX)] = std::uint32_t(infos.size());
                HandleInfo x{HandleInfo::Parameter}; x.a = i; x.b = e; x.c = m; infos.push_back(x);
                // Elements of an array member (UserData.uvMatrix[i]).
                const std::uint32_t n = m == UINT32_MAX ? 0 : type.members[m].elements;
                for (std::uint32_t k = 0; k < n; ++k) {
                    parameterIndex[std::make_tuple(i, e, m, k)] = std::uint32_t(infos.size());
                    HandleInfo y = x; y.d = k; infos.push_back(y);
                }
            };
            for (std::uint32_t e = 0; e < type.elements; ++e) add(e, UINT32_MAX);
            // Struct members: of the struct itself, or of each element of a struct array (D3DX9_25).
            if (type.cls == 5) {
                if (!type.elements) for (std::uint32_t m = 0; m < type.members.size(); ++m) add(UINT32_MAX, m);
                for (std::uint32_t e = 0; e < type.elements; ++e)
                    for (std::uint32_t m = 0; m < type.members.size(); ++m) add(e, m);
            }
        }
        for (std::uint32_t t = 0; t < d.techniques.size(); ++t) {
            techniqueBase.push_back(std::uint32_t(infos.size()));
            HandleInfo h{HandleInfo::Technique}; h.a = t; infos.push_back(h);
            pending.push_back({techniqueBase.back(), &d.techniques[t].annotations});
            for (std::uint32_t p = 0; p < d.techniques[t].passes.size(); ++p) {
                HandleInfo x{HandleInfo::Pass}; x.a = t; x.b = p; infos.push_back(x);
                pending.push_back({std::uint32_t(infos.size() - 1), &d.techniques[t].passes[p].annotations});
            }
        }
        for (const auto& p : pending) annotate(p.first, *p.second);
        // [CHOSEN, not recovered] String-array annotations (RaShader*.mfx literal requirement
        // lists) expose their elements through GetParameterElement, as D3DX's handles do.
        for (std::uint32_t i = 0, n = std::uint32_t(infos.size()); i < n; ++i) {
            const bf2fx::Value* v = infos[i].kind == HandleInfo::Annotation ? infos[i].value : nullptr;
            if (!v || v->type.type != bf2fx::String || !v->type.elements) continue;
            annotationElements[i] = std::uint32_t(infos.size());
            for (std::uint32_t e = 0; e < v->type.elements; ++e) {
                HandleInfo x{HandleInfo::AnnotationElement}; x.value = v; x.b = e; infos.push_back(x);
            }
        }
        block.reset(new char[std::max<std::size_t>(1, infos.size())]);
        arrayRange.resize(d.parameters.size());
    }
    virtual ~EffectBase() = default;
    D3DXHANDLE handle(std::uint32_t index) const { return block.get() + index; }
    const HandleInfo* info(D3DXHANDLE h) const {
        if (!h || h < block.get() || h >= block.get() + infos.size()) return nullptr;
        return &infos[std::size_t(h - block.get())];
    }
    D3DXHANDLE parameterHandle(bf2fx::Handle h) const {
        if (!h) return nullptr;
        const auto it = parameterIndex.find(std::make_tuple(h.parameter, h.element, h.member, h.memberElement));
        return it == parameterIndex.end() ? nullptr : handle(it->second);
    }
    // Effect pool (D3DXCreateEffect with a pool): the indices of this effect's shared parameters.
    std::shared_ptr<PoolStore> pool;
    std::vector<std::uint32_t> sharedParameters;
    std::uint64_t poolSeen = 0;
    static constexpr std::uint32_t kParameterShared = 1; // D3DX_PARAMETER_SHARED
    static bf2fx::Handle whole(std::uint32_t i) { return bf2fx::Handle{i, UINT32_MAX, UINT32_MAX, UINT32_MAX}; }
    static bool textureType(const bf2fx::Type& t) { return t.type >= bf2fx::Texture && t.type < bf2fx::Sampler; }
    bool readShared(std::uint32_t i, PoolStore::Shared& s) const {
        const auto& t = runtime.description().parameters[i].value.type;
        if (textureType(t)) {
            s.textures.assign(std::max(1u, t.elements), nullptr);
            for (std::uint32_t e = 0; e < s.textures.size(); ++e) {
                bf2fx::Handle h = whole(i); if (t.elements) h.element = e;
                if (!runtime.getTexture(h, s.textures[e])) return false;
            }
            return true;
        }
        bf2fx::ParameterDesc d;
        if (!runtime.getParameterDesc(whole(i), d) || !d.bytes) return false;
        s.bytes.assign(d.bytes, 0);
        return runtime.getValue(whole(i), s.bytes.data(), s.bytes.size());
    }
    bool writeShared(std::uint32_t i, const PoolStore::Shared& s) {
        const auto& t = runtime.description().parameters[i].value.type;
        if (!textureType(t)) return runtime.setValue(whole(i), s.bytes.data(), s.bytes.size());
        for (std::uint32_t e = 0; e < s.textures.size(); ++e) {
            bf2fx::Handle h = whole(i); if (t.elements) h.element = e;
            if (!runtime.setTexture(h, s.textures[e])) return false;
        }
        return true;
    }
    // Registers this effect's shared parameters with the pool. A parameter new to the pool brings
    // this effect's initial value; one the pool already holds keeps the pool's current value.
    // [CHOSEN, not recovered] No ID3DXEffectPool exists in 1.50 (E11), so the cases below have no
    // retail input: a shared string/sampler/shader parameter stays private to this effect, and a
    // declaration that conflicts with the pool's fails D3DXCreateEffect (D3DX refuses it too;
    // its exact HRESULT is not reproduced, E_FAIL is answered).
    HRESULT joinPool(std::shared_ptr<PoolStore> store) {
        pool = std::move(store);
        const auto& d = runtime.description();
        for (std::uint32_t i = 0; i < d.parameters.size(); ++i) {
            if (!(d.parameters[i].flags & kParameterShared)) continue;
            const auto& t = d.parameters[i].value.type;
            const bool numeric = (t.type >= bf2fx::Bool && t.type <= bf2fx::Float) || t.cls == 5;
            if (!numeric && !textureType(t)) {
                refuse("D3DXCreateEffect", "shared parameter of a non-numeric, non-texture type kept private: " + t.name);
                continue;
            }
            auto it = pool->parameters.find(t.name);
            if (it == pool->parameters.end()) {
                PoolStore::Shared s; s.type = t;
                if (!readShared(i, s)) {
                    refuse("D3DXCreateEffect", "shared parameter value unreadable, kept private: " + t.name);
                    continue;
                }
                pool->parameters[t.name] = std::move(s); ++pool->version;
            } else if (!sameType(it->second.type, t)) {
                return refuse("D3DXCreateEffect", "shared parameter declared with a different type in the pool: " + t.name, E_FAIL);
            }
            sharedParameters.push_back(i);
        }
        poolSeen = 0;
        pullShared();
        return D3D_OK;
    }
    // Brings this effect's shared parameters up to the pool's current values.
    void pullShared() {
        if (!pool || poolSeen == pool->version) return;
        for (auto i : sharedParameters) writeShared(i, pool->parameters.at(parameterName(i)));
        poolSeen = pool->version;
        touch();
    }
    // Writes the shared parameters this effect changed through to the pool.
    void pushShared() {
        if (!pool) return;
        for (auto i : sharedParameters) {
            PoolStore::Shared now;
            if (!readShared(i, now)) continue;
            auto& s = pool->parameters.at(parameterName(i));
            if (now.bytes != s.bytes || now.textures != s.textures) {
                s.bytes = std::move(now.bytes); s.textures = std::move(now.textures); ++pool->version;
            }
        }
        poolSeen = pool->version;
    }
    // A parameter handle, or a parameter name (with an optional [index]). Every parameter read
    // and write resolves its handle here, so a pooled effect first takes the pool's values.
    bf2fx::Handle parameter(D3DXHANDLE h) const {
        const_cast<EffectBase*>(this)->pullShared();
        if (const auto* i = info(h)) return i->kind == HandleInfo::Parameter ? bf2fx::Handle{i->a, i->b, i->c, i->d} : bf2fx::Handle{};
        return h ? runtime.getParameterByName(h) : bf2fx::Handle{};
    }
    int technique(D3DXHANDLE h) const {
        if (const auto* i = info(h)) return i->kind == HandleInfo::Technique ? int(i->a) : -1;
        return h ? runtime.getTechniqueByName(h) : -1;
    }
    const bf2fx::Value* annotation(D3DXHANDLE h) const {
        const auto* i = info(h); return i && i->kind == HandleInfo::Annotation ? i->value : nullptr;
    }
    std::uint32_t owner(D3DXHANDLE h) const {
        if (info(h)) return std::uint32_t(h - block.get());
        if (!h) return UINT32_MAX;
        const auto p = runtime.getParameterByName(h);
        if (p) return std::uint32_t(parameterHandle(p) - block.get());
        const int t = runtime.getTechniqueByName(h);
        return t >= 0 ? techniqueBase[std::size_t(t)] : UINT32_MAX;
    }
    void touch() { ++generation; }
    const std::string& parameterName(std::uint32_t i) const { return runtime.description().parameters[i].value.type.name; }

    // IUnknown
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (!out) return D3DERR_INVALIDCALL;
        *out = nullptr;
        // [CHOSEN, not recovered] No image holds an effect IID (E3): only IUnknown is answered,
        // every other interface gets the COM refusal.
        if (!sameGuid(id, kIidUnknown)) return refuse("ID3DXBaseEffect::QueryInterface", "interface other than IUnknown", E_NOINTERFACE);
        *out = static_cast<I*>(this); AddRef(); return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
    ULONG STDMETHODCALLTYPE Release() override { const ULONG n = --refs; if (!n) delete this; return n; }

    // Descriptions
    // D3DX semantics. The native build exposes no HLSL function objects (GetFunction*), so
    // Functions is 0; Creator is the compiler string D3DX would report, which FX2 does not keep.
    HRESULT STDMETHODCALLTYPE GetDesc(D3DXEFFECT_DESC* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        const auto& d = runtime.description();
        *out = {};
        out->Creator = nullptr;
        out->Parameters = UINT(d.parameters.size());
        out->Techniques = UINT(d.techniques.size());
        out->Functions = 0;
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetParameterDesc(D3DXHANDLE h, D3DXPARAMETER_DESC* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        if (const auto* v = annotation(h)) {
            *out = {};
            out->Name = v->type.name.c_str(); out->Semantic = v->semantic.empty() ? nullptr : v->semantic.c_str();
            out->Class = D3DXPARAMETER_CLASS(v->type.cls); out->Type = D3DXPARAMETER_TYPE(v->type.type);
            out->Rows = v->type.rows; out->Columns = v->type.columns; out->Elements = v->type.elements;
            out->StructMembers = UINT(v->type.members.size());
            out->Bytes = numericType(v->type) ? UINT(v->type.rows * v->type.columns * 4 * std::max(1u, v->type.elements)) : 0;
            return D3D_OK;
        }
        const auto p = parameter(h);
        bf2fx::ParameterDesc d;
        if (!p || !runtime.getParameterDesc(p, d)) return D3DERR_INVALIDCALL;
        const auto& source = runtime.description().parameters[p.parameter].value;
        const bool member = p.member != UINT32_MAX;
        *out = {};
        // A member reports its own name and no semantic (oracle: struct-oracle.c).
        out->Name = member ? source.type.members[p.member].name.c_str() : source.type.name.c_str();
        out->Semantic = member || source.semantic.empty() ? nullptr : source.semantic.c_str();
        out->Class = D3DXPARAMETER_CLASS(d.type.cls); out->Type = D3DXPARAMETER_TYPE(d.type.type);
        out->Rows = d.type.rows; out->Columns = d.type.columns; out->Elements = d.type.elements;
        out->Annotations = p.element == UINT32_MAX && !member ? d.annotations : 0;
        out->StructMembers = UINT(d.type.members.size()); out->Flags = d.flags; out->Bytes = d.bytes;
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTechniqueDesc(D3DXHANDLE h, D3DXTECHNIQUE_DESC* out) override {
        const int t = technique(h);
        if (!out || t < 0) return D3DERR_INVALIDCALL;
        const auto& tech = runtime.description().techniques[std::size_t(t)];
        out->Name = tech.name.c_str(); out->Passes = UINT(tech.passes.size()); out->Annotations = UINT(tech.annotations.size());
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetPassDesc(D3DXHANDLE h, D3DXPASS_DESC* out) override {
        const auto* i = info(h);
        if (!out || !i || i->kind != HandleInfo::Pass) return D3DERR_INVALIDCALL;
        ensureShaders();
        const auto& pass = runtime.description().techniques[i->a].passes[i->b];
        out->Name = pass.name.c_str(); out->Annotations = UINT(pass.annotations.size());
        out->pVertexShaderFunction = out->pPixelShaderFunction = nullptr;
        // The statically assigned shader objects; a selector is reported as no function.
        for (const auto& s : pass.states) if ((s.id == 146 || s.id == 147) && s.value.expression.empty() && !s.value.words.empty())
            for (const auto& shader : runtime.description().shaders)
                if (shader.object == s.value.words[0] && !shader.selectorKind && !shader.bytecode.empty())
                    (s.id == 146 ? out->pVertexShaderFunction : out->pPixelShaderFunction) =
                        reinterpret_cast<const DWORD*>(shader.bytecode.data());
        return D3D_OK;
    }
    // [CHOSEN, not recovered] No function objects (GetDesc reports 0; no BF2 caller, E5).
    HRESULT STDMETHODCALLTYPE GetFunctionDesc(D3DXHANDLE, D3DXFUNCTION_DESC*) override { return refuse("ID3DXBaseEffect::GetFunctionDesc"); }

    // Handle lookup
    D3DXHANDLE STDMETHODCALLTYPE GetParameter(D3DXHANDLE parent, UINT index) override {
        if (parent) return parameterHandle(runtime.getParameterMember(parameter(parent), std::uint32_t(index)));
        return index < parameterBase.size() ? handle(parameterBase[index]) : nullptr;
    }
    D3DXHANDLE STDMETHODCALLTYPE GetParameterByName(D3DXHANDLE parent, LPCSTR name) override {
        if (!name) return nullptr;
        if (parent) return parameterHandle(runtime.getParameterMember(parameter(parent), std::string(name)));
        return parameterHandle(runtime.getParameterByName(name));
    }
    D3DXHANDLE STDMETHODCALLTYPE GetParameterBySemantic(D3DXHANDLE parent, LPCSTR semantic) override {
        // [CHOSEN, not recovered] Struct members carry no semantic in FX2 here; the only retail
        // call passes no parent (E15).
        if (parent) { refuse("ID3DXBaseEffect::GetParameterBySemantic", "struct members (no handle)"); return nullptr; }
        if (!semantic) return nullptr;
        const auto& params = runtime.description().parameters;
        for (std::uint32_t i = 0; i < params.size(); ++i) {
            const auto& s = params[i].value.semantic;
            if (s.size() == std::strlen(semantic) && std::equal(s.begin(), s.end(), semantic,
                    [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); }))
                return handle(parameterBase[i]);
        }
        return nullptr;
    }
    D3DXHANDLE STDMETHODCALLTYPE GetParameterElement(D3DXHANDLE h, UINT index) override {
        if (const auto* i = info(h)) if (i->kind == HandleInfo::Annotation) {
            const auto it = annotationElements.find(std::uint32_t(h - block.get()));
            return it == annotationElements.end() || index >= i->value->type.elements ? nullptr : handle(it->second + index);
        }
        return parameterHandle(runtime.getParameterElement(parameter(h), index));
    }
    D3DXHANDLE STDMETHODCALLTYPE GetTechnique(UINT index) override {
        return index < techniqueBase.size() ? handle(techniqueBase[index]) : nullptr;
    }
    D3DXHANDLE STDMETHODCALLTYPE GetTechniqueByName(LPCSTR name) override {
        const int t = name ? runtime.getTechniqueByName(name) : -1;
        return t < 0 ? nullptr : handle(techniqueBase[std::size_t(t)]);
    }
    D3DXHANDLE STDMETHODCALLTYPE GetPass(D3DXHANDLE h, UINT index) override {
        const int t = technique(h);
        if (t < 0 || index >= runtime.description().techniques[std::size_t(t)].passes.size()) return nullptr;
        return handle(techniqueBase[std::size_t(t)] + 1 + index);
    }
    D3DXHANDLE STDMETHODCALLTYPE GetPassByName(D3DXHANDLE h, LPCSTR name) override {
        const int t = technique(h);
        if (t < 0 || !name) return nullptr;
        const auto& passes = runtime.description().techniques[std::size_t(t)].passes;
        for (std::size_t p = 0; p < passes.size(); ++p) if (passes[p].name == name) return handle(techniqueBase[std::size_t(t)] + 1 + std::uint32_t(p));
        return nullptr;
    }
    D3DXHANDLE STDMETHODCALLTYPE GetFunction(UINT) override { refuse("ID3DXBaseEffect::GetFunction"); return nullptr; }
    D3DXHANDLE STDMETHODCALLTYPE GetFunctionByName(LPCSTR) override { refuse("ID3DXBaseEffect::GetFunctionByName"); return nullptr; }
    D3DXHANDLE STDMETHODCALLTYPE GetAnnotation(D3DXHANDLE h, UINT index) override {
        auto it = annotations.find(owner(h));
        return it == annotations.end() || index >= it->second.count ? nullptr : handle(it->second.first + index);
    }
    D3DXHANDLE STDMETHODCALLTYPE GetAnnotationByName(D3DXHANDLE h, LPCSTR name) override {
        auto it = annotations.find(owner(h));
        if (it == annotations.end() || !name) return nullptr;
        for (std::uint32_t i = 0; i < it->second.count; ++i)
            if (infos[it->second.first + i].value->type.name == name) return handle(it->second.first + i);
        return nullptr;
    }

    // Values. Annotations are immutable; parameters live in the s2 runtime.
    template<class Get> HRESULT read(D3DXHANDLE h, std::uint32_t type, void* out, std::size_t n, Get get) const {
        if (const auto* v = annotation(h)) return readValue(*v, type, out, n) ? D3D_OK : D3DERR_INVALIDCALL;
        return get(parameter(h)) ? D3D_OK : D3DERR_INVALIDCALL;
    }
    // [CHOSEN, not recovered] Annotations are immutable here: a write is refused with
    // D3DERR_INVALIDCALL. Neither the tree nor retail writes one (E14).
    bool annotationWrite(D3DXHANDLE h, const char* method) const {
        if (!annotation(h)) return false;
        refuse(method, "annotation write"); return true;
    }
    // A vector/matrix read of a numeric annotation: rows x columns per element, D3DX's padded
    // 4-wide vector and 4x4 matrix layout.
    bool readAnnotationArray(const bf2fx::Value& v, float* out, std::size_t n, std::size_t stride, bool matrix, bool transpose) const {
        const auto& t = v.type;
        if (!out || !numericType(t) || t.rows > 4 || t.columns > 4 || (matrix ? t.cls < 2 : t.rows != 1) || n > std::max(1u, t.elements))
            return false;
        const std::size_t each = std::size_t(t.rows) * t.columns;
        std::vector<float> packed(n * each);
        if (!readValue(v, bf2fx::Float, packed.data(), packed.size())) return false;
        std::fill(out, out + n * stride, 0.0f);
        for (std::size_t e = 0, i = 0; e < n; ++e)
            for (std::uint32_t r = 0; r < t.rows; ++r)
                for (std::uint32_t c = 0; c < t.columns; ++c)
                    out[e * stride + (matrix ? (transpose ? c * 4 + r : r * 4 + c) : c)] = packed[i++];
        return true;
    }
    HRESULT result(bool ok, bool write) {
        if (ok && write) { touch(); pushShared(); }
        return ok ? D3D_OK : D3DERR_INVALIDCALL;
    }
    HRESULT STDMETHODCALLTYPE SetValue(D3DXHANDLE h, LPCVOID data, UINT bytes) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetValue")) return D3DERR_INVALIDCALL;
        const auto p = parameter(h); bf2fx::ParameterDesc d;
        if (p && runtime.getParameterDesc(p, d) && d.type.type >= bf2fx::Texture && d.type.type < bf2fx::Sampler) {
            // A texture value is the interface pointer itself.
            if (bytes != sizeof(void*) || !data) return D3DERR_INVALIDCALL;
            IDirect3DBaseTexture9* t = nullptr; std::memcpy(&t, data, sizeof(t));
            return SetTexture(h, t);
        }
        return result(runtime.setValue(p, data, bytes), true);
    }
    HRESULT STDMETHODCALLTYPE GetValue(D3DXHANDLE h, LPVOID data, UINT bytes) override {
        if (const auto* v = annotation(h)) {
            if (!data || bytes % 4 || !readValue(*v, v->type.type, data, bytes / 4)) return D3DERR_INVALIDCALL;
            return D3D_OK;
        }
        return result(runtime.getValue(parameter(h), data, bytes), false);
    }
    HRESULT STDMETHODCALLTYPE SetBool(D3DXHANDLE h, BOOL v) override { if (annotationWrite(h, "ID3DXBaseEffect::SetBool")) return D3DERR_INVALIDCALL; return result(runtime.setBool(parameter(h), v != 0), true); }
    HRESULT STDMETHODCALLTYPE GetBool(D3DXHANDLE h, BOOL* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        return read(h, bf2fx::Bool, out, 1, [&](bf2fx::Handle p) { bool b; if (!runtime.getBool(p, b)) return false; *out = b ? TRUE : FALSE; return true; });
    }
    HRESULT STDMETHODCALLTYPE SetBoolArray(D3DXHANDLE h, const BOOL* v, UINT n) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetBoolArray")) return D3DERR_INVALIDCALL; return result(runtime.setBoolArray(parameter(h), v, n), true);
    }
    HRESULT STDMETHODCALLTYPE GetBoolArray(D3DXHANDLE h, BOOL* out, UINT n) override {
        return read(h, bf2fx::Bool, out, n, [&](bf2fx::Handle p) { return runtime.getBoolArray(p, out, n); });
    }
    HRESULT STDMETHODCALLTYPE SetInt(D3DXHANDLE h, INT v) override { if (annotationWrite(h, "ID3DXBaseEffect::SetInt")) return D3DERR_INVALIDCALL; return result(runtime.setInt(parameter(h), v), true); }
    HRESULT STDMETHODCALLTYPE GetInt(D3DXHANDLE h, INT* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        return read(h, bf2fx::Int, out, 1, [&](bf2fx::Handle p) { std::int32_t i; if (!runtime.getInt(p, i)) return false; *out = i; return true; });
    }
    HRESULT STDMETHODCALLTYPE SetIntArray(D3DXHANDLE h, const INT* v, UINT n) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetIntArray")) return D3DERR_INVALIDCALL; return result(runtime.setIntArray(parameter(h), v, n), true);
    }
    HRESULT STDMETHODCALLTYPE GetIntArray(D3DXHANDLE h, INT* out, UINT n) override {
        return read(h, bf2fx::Int, out, n, [&](bf2fx::Handle p) { return runtime.getIntArray(p, out, n); });
    }
    HRESULT STDMETHODCALLTYPE SetFloat(D3DXHANDLE h, FLOAT v) override { if (annotationWrite(h, "ID3DXBaseEffect::SetFloat")) return D3DERR_INVALIDCALL; return result(runtime.setFloat(parameter(h), v), true); }
    HRESULT STDMETHODCALLTYPE GetFloat(D3DXHANDLE h, FLOAT* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        return read(h, bf2fx::Float, out, 1, [&](bf2fx::Handle p) { return runtime.getFloat(p, *out); });
    }
    HRESULT STDMETHODCALLTYPE SetFloatArray(D3DXHANDLE h, const FLOAT* v, UINT n) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetFloatArray")) return D3DERR_INVALIDCALL; return result(runtime.setFloatArray(parameter(h), v, n), true);
    }
    HRESULT STDMETHODCALLTYPE GetFloatArray(D3DXHANDLE h, FLOAT* out, UINT n) override {
        return read(h, bf2fx::Float, out, n, [&](bf2fx::Handle p) { return runtime.getFloatArray(p, out, n); });
    }
    HRESULT STDMETHODCALLTYPE SetVector(D3DXHANDLE h, const D3DXVECTOR4* v) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetVector") || !v) return D3DERR_INVALIDCALL;
        return result(runtime.setVector(parameter(h), {v->x, v->y, v->z, v->w}), true);
    }
    HRESULT STDMETHODCALLTYPE GetVector(D3DXHANDLE h, D3DXVECTOR4* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        if (const auto* v = annotation(h)) {
            float f[4] = {}; const auto n = valueComponents(*v).size();
            if (v->type.rows != 1 || n > 4 || !readValue(*v, bf2fx::Float, f, n)) return D3DERR_INVALIDCALL;
            *out = {f[0], f[1], f[2], f[3]}; return D3D_OK;
        }
        std::array<float, 4> f{};
        if (!runtime.getVector(parameter(h), f)) return D3DERR_INVALIDCALL;
        *out = {f[0], f[1], f[2], f[3]}; return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetVectorArray(D3DXHANDLE h, const D3DXVECTOR4* v, UINT n) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetVectorArray")) return D3DERR_INVALIDCALL;
        return result(runtime.setVectorArray(parameter(h), v ? &v->x : nullptr, n), true);
    }
    HRESULT STDMETHODCALLTYPE GetVectorArray(D3DXHANDLE h, D3DXVECTOR4* out, UINT n) override {
        if (const auto* v = annotation(h)) return readAnnotationArray(*v, out ? &out->x : nullptr, n, 4, false, false) ? D3D_OK : D3DERR_INVALIDCALL;
        return result(runtime.getVectorArray(parameter(h), out ? &out->x : nullptr, n), false);
    }
    HRESULT matrices(D3DXHANDLE h, const D3DXMATRIX* m, UINT n, bool transpose, const char* method) {
        if (annotationWrite(h, method)) return D3DERR_INVALIDCALL;
        return result(runtime.setMatrixArray(parameter(h), m ? &m->_11 : nullptr, n, transpose), true);
    }
    HRESULT getMatrices(D3DXHANDLE h, D3DXMATRIX* m, UINT n, bool transpose, const char*) {
        if (const auto* v = annotation(h)) return readAnnotationArray(*v, m ? &m->_11 : nullptr, n, 16, true, transpose) ? D3D_OK : D3DERR_INVALIDCALL;
        return result(runtime.getMatrixArray(parameter(h), m ? &m->_11 : nullptr, n, transpose), false);
    }
    HRESULT pointerMatrices(D3DXHANDLE h, const D3DXMATRIX** m, UINT n, bool transpose, const char* method) {
        if (!m && n) return D3DERR_INVALIDCALL;
        std::vector<D3DXMATRIX> packed(n);
        for (UINT i = 0; i < n; ++i) { if (!m[i]) return D3DERR_INVALIDCALL; packed[i] = *m[i]; }
        return matrices(h, n ? packed.data() : nullptr, n, transpose, method);
    }
    HRESULT getPointerMatrices(D3DXHANDLE h, D3DXMATRIX** m, UINT n, bool transpose, const char* method) {
        if (!m && n) return D3DERR_INVALIDCALL;
        std::vector<D3DXMATRIX> packed(n);
        const HRESULT hr = getMatrices(h, n ? packed.data() : nullptr, n, transpose, method);
        if (SUCCEEDED(hr)) for (UINT i = 0; i < n; ++i) { if (!m[i]) return D3DERR_INVALIDCALL; *m[i] = packed[i]; }
        return hr;
    }
    HRESULT STDMETHODCALLTYPE SetMatrix(D3DXHANDLE h, const D3DXMATRIX* m) override { return matrices(h, m, 1, false, "ID3DXBaseEffect::SetMatrix"); }
    HRESULT STDMETHODCALLTYPE GetMatrix(D3DXHANDLE h, D3DXMATRIX* m) override { return getMatrices(h, m, 1, false, "ID3DXBaseEffect::GetMatrix"); }
    HRESULT STDMETHODCALLTYPE SetMatrixArray(D3DXHANDLE h, const D3DXMATRIX* m, UINT n) override { return matrices(h, m, n, false, "ID3DXBaseEffect::SetMatrixArray"); }
    HRESULT STDMETHODCALLTYPE GetMatrixArray(D3DXHANDLE h, D3DXMATRIX* m, UINT n) override { return getMatrices(h, m, n, false, "ID3DXBaseEffect::GetMatrixArray"); }
    HRESULT STDMETHODCALLTYPE SetMatrixPointerArray(D3DXHANDLE h, const D3DXMATRIX** m, UINT n) override { return pointerMatrices(h, m, n, false, "ID3DXBaseEffect::SetMatrixPointerArray"); }
    HRESULT STDMETHODCALLTYPE GetMatrixPointerArray(D3DXHANDLE h, D3DXMATRIX** m, UINT n) override { return getPointerMatrices(h, m, n, false, "ID3DXBaseEffect::GetMatrixPointerArray"); }
    HRESULT STDMETHODCALLTYPE SetMatrixTranspose(D3DXHANDLE h, const D3DXMATRIX* m) override { return matrices(h, m, 1, true, "ID3DXBaseEffect::SetMatrixTranspose"); }
    HRESULT STDMETHODCALLTYPE GetMatrixTranspose(D3DXHANDLE h, D3DXMATRIX* m) override { return getMatrices(h, m, 1, true, "ID3DXBaseEffect::GetMatrixTranspose"); }
    HRESULT STDMETHODCALLTYPE SetMatrixTransposeArray(D3DXHANDLE h, const D3DXMATRIX* m, UINT n) override { return matrices(h, m, n, true, "ID3DXBaseEffect::SetMatrixTransposeArray"); }
    HRESULT STDMETHODCALLTYPE GetMatrixTransposeArray(D3DXHANDLE h, D3DXMATRIX* m, UINT n) override { return getMatrices(h, m, n, true, "ID3DXBaseEffect::GetMatrixTransposeArray"); }
    HRESULT STDMETHODCALLTYPE SetMatrixTransposePointerArray(D3DXHANDLE h, const D3DXMATRIX** m, UINT n) override { return pointerMatrices(h, m, n, true, "ID3DXBaseEffect::SetMatrixTransposePointerArray"); }
    HRESULT STDMETHODCALLTYPE GetMatrixTransposePointerArray(D3DXHANDLE h, D3DXMATRIX** m, UINT n) override { return getPointerMatrices(h, m, n, true, "ID3DXBaseEffect::GetMatrixTransposePointerArray"); }
    // [CHOSEN, not recovered] String parameters are immutable FX2 objects here (no BF2 caller, E5).
    HRESULT STDMETHODCALLTYPE SetString(D3DXHANDLE, LPCSTR) override { return refuse("ID3DXBaseEffect::SetString"); }
    HRESULT STDMETHODCALLTYPE GetString(D3DXHANDLE h, LPCSTR* out) override {
        if (!out) return D3DERR_INVALIDCALL;
        // A string-array element (parameter or annotation) names its FX2 string object.
        auto element = [&](const bf2fx::Value& array, std::uint32_t e) -> HRESULT {
            const auto& objects = runtime.description().objects;
            if (array.type.type != bf2fx::String || e >= array.words.size() || array.words[e] >= objects.size())
                return D3DERR_INVALIDCALL;
            *out = objects[array.words[e]].c_str(); return D3D_OK;
        };
        if (const auto* i = info(h)) if (i->kind == HandleInfo::AnnotationElement) return element(*i->value, i->b);
        const bf2fx::Value* v = annotation(h);
        if (!v) {
            const auto p = parameter(h);
            if (!p) return D3DERR_INVALIDCALL;
            v = &runtime.description().parameters[p.parameter].value;
            if (p.element != UINT32_MAX) return element(*v, p.element);
        }
        if (v->type.type != bf2fx::String || v->type.elements) return D3DERR_INVALIDCALL;
        *out = v->text.c_str(); return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTexture(D3DXHANDLE h, IDirect3DBaseTexture9* t) override {
        if (annotationWrite(h, "ID3DXBaseEffect::SetTexture")) return D3DERR_INVALIDCALL;
        std::shared_ptr<void> owner;
        if (t) { t->AddRef(); owner = std::shared_ptr<void>(t, [](void* p) { static_cast<IDirect3DBaseTexture9*>(p)->Release(); }); }
        return result(runtime.setTexture(parameter(h), std::move(owner)), true);
    }
    HRESULT STDMETHODCALLTYPE GetTexture(D3DXHANDLE h, IDirect3DBaseTexture9** out) override {
        if (!out) return D3DERR_INVALIDCALL;
        std::shared_ptr<void> owner;
        if (!runtime.getTexture(parameter(h), owner)) return D3DERR_INVALIDCALL;
        *out = static_cast<IDirect3DBaseTexture9*>(owner.get()); if (*out) (*out)->AddRef(); return D3D_OK;
    }
    // [CHOSEN, not recovered] Shader-typed parameters are not exposed as device shaders (no BF2
    // caller, E5): the call is refused and the out pointer cleared.
    HRESULT STDMETHODCALLTYPE GetPixelShader(D3DXHANDLE, IDirect3DPixelShader9** out) override {
        if (out) *out = nullptr; return refuse("ID3DXBaseEffect::GetPixelShader");
    }
    HRESULT STDMETHODCALLTYPE GetVertexShader(D3DXHANDLE, IDirect3DVertexShader9** out) override {
        if (out) *out = nullptr; return refuse("ID3DXBaseEffect::GetVertexShader");
    }
    HRESULT STDMETHODCALLTYPE SetArrayRange(D3DXHANDLE h, UINT start, UINT end) override {
        // [CHOSEN, not recovered] D3DX uses the range to limit constant uploads. Every element is
        // uploaded here; a shader cannot read an element it did not declare. Validation only.
        const auto p = parameter(h);
        bf2fx::ParameterDesc d;
        if (!p || p.element != UINT32_MAX || !runtime.getParameterDesc(p, d) || start > end || end > std::max(1u, d.type.elements))
            return D3DERR_INVALIDCALL;
        arrayRange[p.parameter] = {start, end};
        return D3D_OK;
    }
};

// ---- ID3DXEffect -------------------------------------------------------------------------------
class Effect;
// Applies one pass through the state manager (or the device) in D3DX's state order. In commit
// mode only states that depend on parameters are reissued, with all shader constants and every
// sampler's texture; fixed states the engine overrode inside the pass are left alone.
struct Applier final : bf2fx::Callbacks {
    Effect& effect;
    const bf2fx::Pass& pass;
    bool commit;
    std::size_t cursor = 0;
    Applier(Effect& e, const bf2fx::Pass& p, bool c) : effect(e), pass(p), commit(c) {}
    bool dynamicState() {
        if (cursor >= pass.states.size()) return true;
        return !pass.states[cursor++].value.expression.empty();
    }
    void state(std::uint32_t id, std::uint32_t index, const bf2fx::Value& value) override;
    void shader(std::uint32_t stage, const bf2fx::Shader* shader) override;
    void constants(const bf2fx::Shader& shader, const bf2fx::Registers& registers) override;
    void sampler(std::uint32_t stage, std::uint32_t slot, const bf2fx::Value& sampler,
                 const std::shared_ptr<void>& texture) override;
    bool validate(const bf2fx::Pass&, const bf2fx::Description&) override { return true; }
};
// D3DX technique validation: every statically assigned shader must have a translated program
// within the device's shader model. Selector shaders are checked through their candidates.
struct Validator final : bf2fx::Callbacks {
    void state(std::uint32_t, std::uint32_t, const bf2fx::Value&) override {}
    void shader(std::uint32_t, const bf2fx::Shader*) override {}
    void constants(const bf2fx::Shader&, const bf2fx::Registers&) override {}
    void sampler(std::uint32_t, std::uint32_t, const bf2fx::Value&, const std::shared_ptr<void>&) override {}
    bool validate(const bf2fx::Pass& pass, const bf2fx::Description& d) override {
        for (const auto& s : pass.states) {
            if (s.id != 146 && s.id != 147) continue;
            if (!s.value.expression.empty() || s.value.words.empty()) continue;
            for (const auto& shader : d.shaders) if (shader.object == s.value.words[0] && !shader.selectorKind) {
                if (!shader.bytecode.empty() && (shader.msl.empty() || shader.major > 3)) return false;
            }
        }
        return true;
    }
};

class Effect;
// Effects whose native build was still running when they were created and whose device shaders
// do not exist yet (render thread only).
std::set<Effect*>& unsettledEffects() { static std::set<Effect*> effects; return effects; }

class Effect final : public EffectBase<ID3DXEffect> {
public:
    IDirect3DDevice9* device;
    ID3DXEffectStateManager* manager = nullptr;
    std::map<std::uint32_t, IUnknown*> shaders; // FX2 object -> device shader
    int current = -1;
    bool begun = false;
    int active = -1;
    std::uint64_t applied = 0;
    Deferred deferred;           // Native build still compiling (empty: shaders are ready).
    std::string traceKey;        // BF2_HITCH_TRACE / draw trace: the start of the cache key
    std::shared_ptr<const bf2fx::Description> shaderOwner; // Owner of the shader records in use.
    // Device state a technique sets, saved by Begin without D3DXFX_DONOTSAVESTATE and reapplied
    // by End. The keys are recorded by applying every pass with the setters switched to
    // recording; the values are then read back from the device. Texture and shader entries hold
    // the reference the device getter returned until End reapplies them.
    struct SavedState {
        std::map<DWORD, DWORD> render;
        std::map<std::pair<DWORD, DWORD>, DWORD> stage, sampler;   // (stage or sampler, state)
        std::map<DWORD, IDirect3DBaseTexture9*> textures;          // sampler index
        bool vertexShader = false, pixelShader = false;
        IDirect3DVertexShader9* vs = nullptr;
        IDirect3DPixelShader9* ps = nullptr;
        std::map<std::pair<bool, UINT>, std::array<float, 4>> floats; // (vertex stage, register)
        std::map<std::pair<bool, UINT>, std::array<INT, 4>> ints;
        std::map<std::pair<bool, UINT>, BOOL> bools;
        ~SavedState() {
            for (auto& t : textures) if (t.second) t.second->Release();
            if (vs) vs->Release();
            if (ps) ps->Release();
        }
    };
    std::unique_ptr<SavedState> recording, saved;
    // [CHOSEN, not recovered] A native build that failed for a request D3DX accepts (a toolchain
    // fault, E17) leaves the effect without shaders: its techniques fail validation and BeginPass
    // answers D3DERR_INVALIDCALL, so the engine draws nothing with it instead of the run ending.
    bool broken = false;
    ID3DXEffectPool* poolObject = nullptr; // the pool passed to D3DXCreateEffect, referenced
    Effect(IDirect3DDevice9* d, bf2fx::Entry& entry, DWORD f, Deferred pending = {})
        : EffectBase(entry, f), device(d), deferred(std::move(pending)), traceKey(entry.key.substr(0, 12)) {
        { bf2metal::reftrace::Owned o; device->AddRef(); }
        bf2metal::reftrace::born(this, "ID3DXEffect", [](const void*) { return 1ul; });
        shaderOwner = shared;
        if (!runtime.description().techniques.empty()) current = 0;
        if (deferred) unsettledEffects().insert(this);
    }
    // The background build has finished: ensureShaders() will not wait.
    bool buildFinished() const {
        return deferred && deferred.future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }
    // [CHOSEN, not recovered] A deferred native effect gets its shader records from the
    // background build at first use; the structure (parameters, techniques, objects) is the same.
    void ensureShaders() override {
        if (!deferred) return;
        unsettledEffects().erase(this);
        bf2fx::Entry full;
        {
            bf2hitch::Scope timed(bf2hitch::FxWait, deferred.key.c_str());
            full = bf2fx::native::wait(deferred.key, deferred.future);
        }
        deferred = {};
        if (full.kind != bf2fx::EffectEntry) {
            broken = true; refuse("D3DXCreateEffect", "native build failed for a request D3DX accepts: " + full.log); return;
        }
        if (full.description.shaders.size() != runtime.description().shaders.size()) {
            broken = true; refuse("D3DXCreateEffect", "native build and its skeleton differ in shader objects"); return;
        }
        runtime.setShaders(full.description.shaders);
        shaderOwner = std::make_shared<const bf2fx::Description>(std::move(full.description));
        if (FAILED(createShaders())) { broken = true; refuse("D3DXCreateEffect", "device shader creation failed for a native build"); }
    }
    ~Effect() override {
        unsettledEffects().erase(this);
        saved.reset();
        if (poolObject) poolObject->Release();
        for (auto& s : shaders) if (s.second) s.second->Release();
        if (manager) manager->Release();
        bf2metal::reftrace::died(this);
        bf2metal::reftrace::Owned o; device->Release();
    }
    HRESULT createShaders() {
        bf2hitch::Scope timed(bf2hitch::ShaderCreate);
        for (const auto& s : shaderOwner->shaders) {
            if (s.selectorKind || s.bytecode.empty()) continue;
            std::shared_ptr<const bf2fx::Shader> record(shaderOwner, &s);
            bf2metal::ShaderOffer offer(record);
            const auto* code = reinterpret_cast<const DWORD*>(s.bytecode.data());
            HRESULT hr;
            if (s.stage == bf2fx::VertexShader) {
                IDirect3DVertexShader9* vs = nullptr; hr = device->CreateVertexShader(code, &vs); shaders[s.object] = vs;
            } else if (s.stage == bf2fx::PixelShader) {
                IDirect3DPixelShader9* ps = nullptr; hr = device->CreatePixelShader(code, &ps); shaders[s.object] = ps;
            } else {
                // [CHOSEN, not recovered] FX2 has only vertex and pixel shader objects; another
                // stage is malformed data and fails creation as D3DX does for a bad effect.
                hr = refuse("D3DXCreateEffect", "shader object of unknown stage", E_FAIL);
            }
            if (FAILED(hr)) return hr;
        }
        return D3D_OK;
    }
    // Device-facing setters; D3DX routes everything through the installed state manager. While
    // Begin records the technique's state (recording), they note the state they would set.
    void renderState(DWORD s, DWORD v) {
        if (recording) { recording->render[s] = 0; return; }
        manager ? manager->SetRenderState(D3DRENDERSTATETYPE(s), v) : device->SetRenderState(D3DRENDERSTATETYPE(s), v);
    }
    void stageState(DWORD stage, DWORD s, DWORD v) {
        if (recording) { recording->stage[{stage, s}] = 0; return; }
        manager ? manager->SetTextureStageState(stage, D3DTEXTURESTAGESTATETYPE(s), v) : device->SetTextureStageState(stage, D3DTEXTURESTAGESTATETYPE(s), v);
    }
    void samplerState(DWORD stage, DWORD s, DWORD v) {
        if (recording) { recording->sampler[{stage, s}] = 0; return; }
        manager ? manager->SetSamplerState(stage, D3DSAMPLERSTATETYPE(s), v) : device->SetSamplerState(stage, D3DSAMPLERSTATETYPE(s), v);
    }
    void texture(DWORD stage, IDirect3DBaseTexture9* t) {
        if (recording) { recording->textures[stage] = nullptr; return; }
        manager ? manager->SetTexture(stage, t) : device->SetTexture(stage, t);
    }
    void floats(bool vertex, UINT start, const float* v, UINT n) {
        if (recording) { for (UINT i = 0; i < n; ++i) recording->floats[{vertex, start + i}] = {}; return; }
        if (vertex) manager ? manager->SetVertexShaderConstantF(start, v, n) : device->SetVertexShaderConstantF(start, v, n);
        else manager ? manager->SetPixelShaderConstantF(start, v, n) : device->SetPixelShaderConstantF(start, v, n);
    }
    void ints(bool vertex, UINT start, const INT* v, UINT n) {
        if (recording) { for (UINT i = 0; i < n; ++i) recording->ints[{vertex, start + i}] = {}; return; }
        if (vertex) manager ? manager->SetVertexShaderConstantI(start, v, n) : device->SetVertexShaderConstantI(start, v, n);
        else manager ? manager->SetPixelShaderConstantI(start, v, n) : device->SetPixelShaderConstantI(start, v, n);
    }
    void bools(bool vertex, UINT start, const BOOL* v, UINT n) {
        if (recording) { for (UINT i = 0; i < n; ++i) recording->bools[{vertex, start + i}] = FALSE; return; }
        if (vertex) manager ? manager->SetVertexShaderConstantB(start, v, n) : device->SetVertexShaderConstantB(start, v, n);
        else manager ? manager->SetPixelShaderConstantB(start, v, n) : device->SetPixelShaderConstantB(start, v, n);
    }
    IDirect3DBaseTexture9* textureOf(const bf2fx::Value& v) {
        // A texture state names its parameter by reference or by FX2 object index.
        // [CHOSEN, not recovered] A reference that names no texture parameter (no shipped pass
        // has one, E16) binds no texture.
        std::string name = v.type.name;
        if (v.type.type < bf2fx::Texture || v.type.type >= bf2fx::Sampler) { refuse("ID3DXEffect::BeginPass", "Texture state without a texture value (none bound)"); return nullptr; }
        bf2fx::Handle h = runtime.getParameterByName(name);
        if (!h && !v.words.empty() && v.words[0] < runtime.description().objects.size())
            h = runtime.getParameterByName(runtime.description().objects[v.words[0]]);
        if (!h) { if (!v.words.empty() && v.words[0]) refuse("ID3DXEffect::BeginPass", "unknown texture object (none bound)"); return nullptr; }
        std::shared_ptr<void> owner;
        if (!runtime.getTexture(h, owner)) { refuse("ID3DXEffect::BeginPass", "texture reference of wrong type (none bound)"); return nullptr; }
        return static_cast<IDirect3DBaseTexture9*>(owner.get());
    }
    void applyState(std::uint32_t id, std::uint32_t index, const bf2fx::Value& v) {
        if (id < 103) { const unsigned s = kRenderStates[id]; renderState(s, stateWord(v, floatRenderState(s))); return; }
        if (id < 121) { const unsigned s = kStageStates[id - 103]; stageState(index, s, stateWord(v, floatStageState(s))); return; }
        if (id >= 148 && id <= 163) {
            // F, B, I, then the untyped float form sized by its value, then 1..4 registers.
            const bool vertex = id < 156; const unsigned kind = (id - 148) % 8;
            // [CHOSEN, not recovered] c-loopback: like D3DX, the typed and untyped forms take
            // as many registers as the value has bytes (16 per register), not rows x elements:
            // an expression value such as RaShaderWaterBase's PixelShaderConstantF[1] = (WaterColor)
            // arrives as a 4x1 float (4 words, one register). A value smaller than one register
            // (a scalar such as WaterCycleTime) fills one register with zero padding, as D3DX
            // rounds the parameter up to one element; any other partial register stays a stop.
            const std::size_t rows = kind >= 4 ? kind - 3
                : (kind == 1 ? std::size_t(v.type.rows) * std::max(1u, v.type.elements)
                             : std::max<std::size_t>(1, v.words.size() / 4));
            const bool padded = kind != 1 && kind < 4 && !v.words.empty() && v.words.size() < 4;
            const bool partial = kind != 1 && kind < 4 && v.words.size() > 4 && v.words.size() % 4;
            if ((v.words.size() < rows * 4 && !padded) || partial) {
                // [CHOSEN, not recovered] Any other partial register (no shipped pass has one,
                // E16) is zero-padded like the scalar case above.
                char shape[160];
                std::snprintf(shape, sizeof(shape), "shader-constant state shape, zero-padded (state %u[%u], %zu words, type %u rows %u cols %u elements %u)",
                              id, index, v.words.size(), unsigned(v.type.type), unsigned(v.type.rows), unsigned(v.type.columns),
                              unsigned(v.type.elements));
                refuse("ID3DXEffect::BeginPass", shape);
            }
            if (kind == 0 || kind >= 3) {
                std::vector<float> f(rows * 4, 0.0f);
                for (std::size_t i = 0; i < f.size() && i < v.words.size(); ++i) f[i] = float(number(v.words[i], v.type.type));
                floats(vertex, index, f.data(), UINT(rows));
            } else if (kind == 2) {
                std::vector<INT> n(rows * 4, 0);
                for (std::size_t i = 0; i < n.size() && i < v.words.size(); ++i) n[i] = toInt(number(v.words[i], v.type.type));
                ints(vertex, index, n.data(), UINT(rows));
            } else {
                const auto c = valueComponents(v); std::vector<BOOL> b;
                for (auto i : c) b.push_back(number(v.words[i], v.type.type) != 0 ? TRUE : FALSE);
                bools(vertex, index, b.data(), UINT(b.size()));
            }
            return;
        }
        if (id == 164) { texture(index, textureOf(v)); return; }
        if (id >= 165 && id <= 177) { const unsigned s = id - 164; samplerState(index, s, stateWord(v, s == 8)); return; }
        // [CHOSEN, not recovered] Transform, material, light, NPatch and FVF pass states drive
        // fixed-function vertex processing, which no shipped effect uses (E16: no pass sets
        // one). The state is ignored.
        char name[64]; std::snprintf(name, sizeof(name), "FX2 state %u[%u]", id, index);
        refuse("ID3DXEffect::BeginPass", std::string(name) + " (transform/material/light/NPatch/FVF) ignored");
    }
    void applyShader(std::uint32_t stage, const bf2fx::Shader* s) {
        if (recording) { (stage == bf2fx::VertexShader ? recording->vertexShader : recording->pixelShader) = true; return; }
        IUnknown* object = nullptr;
        if (s) {
            auto it = shaders.find(s->object);
            // [CHOSEN, not recovered] Unreachable: createShaders makes every object a pass can
            // name, else creation fails. Binds no shader.
            if (it == shaders.end()) refuse("ID3DXEffect::BeginPass", "shader object was not created (none bound)");
            else object = it->second;
        }
        if (stage == bf2fx::VertexShader) {
            auto* vs = static_cast<IDirect3DVertexShader9*>(object);
            manager ? manager->SetVertexShader(vs) : device->SetVertexShader(vs);
        } else {
            auto* ps = static_cast<IDirect3DPixelShader9*>(object);
            manager ? manager->SetPixelShader(ps) : device->SetPixelShader(ps);
        }
    }
    void applyConstants(const bf2fx::Shader& shader, const bf2fx::Registers& r) {
        const bool vertex = shader.stage == bf2fx::VertexShader;
        // Whole float4/int4 registers in contiguous runs; undeclared components are zero.
        auto runs = [&](std::size_t registers, const std::vector<std::uint8_t>& known, auto emit) {
            std::size_t start = 0, n = 0;
            for (std::size_t reg = 0; reg <= registers; ++reg) {
                bool any = false;
                for (std::size_t c = 0; reg < registers && c < 4; ++c) any = any || (reg * 4 + c < known.size() && known[reg * 4 + c]);
                if (any) { if (!n) start = reg; ++n; }
                else if (n) { emit(start, n); n = 0; }
            }
        };
        runs((r.f.size() + 3) / 4, r.knownF, [&](std::size_t start, std::size_t n) {
            std::vector<float> data(n * 4, 0.0f);
            for (std::size_t i = 0; i < n * 4; ++i) { const std::size_t k = start * 4 + i; if (k < r.f.size() && k < r.knownF.size() && r.knownF[k]) data[i] = r.f[k]; }
            floats(vertex, UINT(start), data.data(), UINT(n));
        });
        runs((r.i.size() + 3) / 4, r.knownI, [&](std::size_t start, std::size_t n) {
            std::vector<INT> data(n * 4, 0);
            for (std::size_t i = 0; i < n * 4; ++i) { const std::size_t k = start * 4 + i; if (k < r.i.size() && k < r.knownI.size() && r.knownI[k]) data[i] = r.i[k]; }
            ints(vertex, UINT(start), data.data(), UINT(n));
        });
        std::size_t start = 0, n = 0;
        for (std::size_t b = 0; b <= r.b.size(); ++b) {
            const bool known = b < r.b.size() && b < r.knownB.size() && r.knownB[b];
            if (known) { if (!n) start = b; ++n; }
            else if (n) {
                std::vector<BOOL> data(n);
                for (std::size_t i = 0; i < n; ++i) data[i] = r.b[start + i] ? TRUE : FALSE;
                bools(vertex, UINT(start), data.data(), UINT(n)); n = 0;
            }
        }
    }
    void applySampler(std::uint32_t stage, std::uint32_t slot, const bf2fx::Value& s,
                      const std::shared_ptr<void>& tex, bool commit) {
        const DWORD index = stage == bf2fx::VertexShader ? 257 + slot : slot; // D3DVERTEXTEXTURESAMPLER0
        const bf2fx::Value* original = nullptr;
        if (const auto h = runtime.getParameterByName(s.type.name)) original = &runtime.description().parameters[h.parameter].value;
        bool hasTexture = false;
        for (auto id : s.stateIds) hasTexture = hasTexture || id == 4;
        if (hasTexture) texture(index, static_cast<IDirect3DBaseTexture9*>(tex.get()));
        for (std::size_t i = 0; i < s.stateIds.size(); ++i) {
            const auto id = s.stateIds[i];
            if (id == 4) continue;
            // [CHOSEN, not recovered] FX2 sampler states are 4..17; another id is ignored.
            if (id < 5 || id > 17) { refuse("ID3DXEffect::BeginPass", "unknown sampler state ignored " + std::to_string(id)); continue; }
            if (commit && !(original && i < original->states.size() && !original->states[i].expression.empty())) continue;
            samplerState(index, id - 4, stateWord(s.states[i], id - 4 == 8));
        }
    }
    bool validTechnique(int t) {
        Validator v;
        return t >= 0 && !broken && runtime.findNextValidTechnique(t - 1, v) == t;
    }
    HRESULT apply(bool commit) {
        pullShared();
        if (current < 0 || active < 0 || broken) return D3DERR_INVALIDCALL;
        Applier a(*this, runtime.description().techniques[std::size_t(current)].passes[std::size_t(active)], commit);
        if (!runtime.applyPass(std::uint32_t(current), std::uint32_t(active), a)) return D3DERR_INVALIDCALL;
        applied = generation;
        return D3D_OK;
    }

    HRESULT STDMETHODCALLTYPE GetPool(ID3DXEffectPool** out) override {
        if (!out) return D3DERR_INVALIDCALL;
        // D3DX semantics: an effect created without a pool answers S_OK and a null pool.
        *out = poolObject;
        if (poolObject) poolObject->AddRef();
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE SetTechnique(D3DXHANDLE h) override {
        const int t = technique(h); if (t < 0) return D3DERR_INVALIDCALL; current = t; return D3D_OK;
    }
    D3DXHANDLE STDMETHODCALLTYPE GetCurrentTechnique() override { return current < 0 ? nullptr : handle(techniqueBase[std::size_t(current)]); }
    HRESULT STDMETHODCALLTYPE ValidateTechnique(D3DXHANDLE h) override {
        const int t = technique(h); if (t < 0) return D3DERR_INVALIDCALL;
        return validTechnique(t) ? D3D_OK : E_FAIL;
    }
    HRESULT STDMETHODCALLTYPE FindNextValidTechnique(D3DXHANDLE previous, D3DXHANDLE* next) override {
        if (!next) return D3DERR_INVALIDCALL;
        const int from = previous ? technique(previous) : -1;
        if (previous && from < 0) return D3DERR_INVALIDCALL;
        Validator v;
        const int t = broken ? -1 : runtime.findNextValidTechnique(from, v);
        *next = t < 0 ? nullptr : handle(techniqueBase[std::size_t(t)]);
        return t < 0 ? S_FALSE : D3D_OK;
    }
    BOOL STDMETHODCALLTYPE IsParameterUsed(D3DXHANDLE h, D3DXHANDLE tech) override {
        const auto p = parameter(h); const int t = technique(tech);
        if (!p || t < 0) return FALSE;
        return used(std::uint32_t(t)).count(p.parameter) ? TRUE : FALSE;
    }
    std::map<std::uint32_t, std::set<std::uint32_t>> usedCache;
    // Parameters referenced by a technique: state expressions/references, shader constant
    // tables and preshaders, samplers bound by shaders and their texture references.
    const std::set<std::uint32_t>& used(std::uint32_t t) {
        ensureShaders(); // Constant tables come from the shader records.
        auto it = usedCache.find(t); if (it != usedCache.end()) return it->second;
        auto& out = usedCache[t];
        const auto& d = runtime.description();
        std::vector<const bf2fx::Value*> values;
        std::set<std::uint32_t> shaderObjects;
        auto mark = [&](const std::string& name) {
            const auto h = runtime.getParameterByName(name);
            if (h && out.insert(h.parameter).second) values.push_back(&d.parameters[h.parameter].value);
        };
        auto program = [&](const bf2fx::Program& p) { for (const auto& b : p.bindings) mark(b.type.name); };
        for (const auto& pass : d.techniques[t].passes) for (const auto& s : pass.states) {
            if (!s.value.reference.empty()) mark(s.value.reference);
            program(s.value.program);
            if ((s.id == 146 || s.id == 147) && !s.value.words.empty()) shaderObjects.insert(s.value.words[0]);
            if (s.id == 178 || s.id == 164) values.push_back(&s.value);
        }
        std::set<std::uint32_t> seen;
        while (!shaderObjects.empty()) {
            const auto object = *shaderObjects.begin(); shaderObjects.erase(shaderObjects.begin());
            if (!seen.insert(object).second) continue;
            for (const auto& sh : d.shaders) if (sh.object == object) {
                for (const auto& b : sh.bindings) mark(b.type.name);
                program(sh.preshader);
                if (sh.selectorKind && sh.selectorParameter < d.parameters.size()) {
                    out.insert(sh.selectorParameter);
                    for (auto w : d.parameters[sh.selectorParameter].value.words) shaderObjects.insert(w);
                }
            }
        }
        for (std::size_t i = 0; i < values.size(); ++i) {
            const auto* v = values[i];
            if (!v->reference.empty()) mark(v->reference);
            for (const auto& st : v->states) {
                if (!st.reference.empty()) mark(st.reference);
                program(st.program);
                if (!st.words.empty() && st.words[0] < d.objects.size() && st.type.type >= bf2fx::Texture && st.type.type < bf2fx::Sampler)
                    mark(d.objects[st.words[0]]);
            }
            if (v->type.type >= bf2fx::Texture && v->type.type < bf2fx::Sampler && !v->words.empty() && v->words[0] < d.objects.size())
                mark(d.objects[v->words[0]]);
        }
        return out;
    }
    // Records the state every pass of the current technique sets, then reads its current value
    // from the device. D3DXFX_DONOTSAVESHADERSTATE leaves the shaders and shader constants out,
    // D3DXFX_DONOTSAVESAMPLERSTATE the sampler states (D3DX semantics).
    // [CHOSEN, not recovered] A state the device holds no value for is left out of the saved set
    // (not restored) rather than restoring an invented default; every state a shipped pass sets
    // is in the device's state cache (`Effect::save`).
    void save(DWORD f) {
        ensureShaders(); // Shader constant tables come from the shader records.
        recording.reset(new SavedState);
        const auto& technique = runtime.description().techniques[std::size_t(current)];
        for (std::uint32_t p = 0; p < technique.passes.size(); ++p) {
            Applier recorder(*this, technique.passes[p], false);
            runtime.applyPass(std::uint32_t(current), p, recorder); // A failing pass also fails its BeginPass.
        }
        saved = std::move(recording);
        SavedState& s = *saved;
        if (f & D3DXFX_DONOTSAVESHADERSTATE) {
            s.vertexShader = s.pixelShader = false;
            s.floats.clear(); s.ints.clear(); s.bools.clear();
        }
        if (f & D3DXFX_DONOTSAVESAMPLERSTATE) s.sampler.clear();
        auto missing = [](const std::string& what) {
            refuse("ID3DXEffect::Begin", "state saving: the device holds no value for " + what + " (not restored)");
        };
        auto keep = [&](auto& map, auto get, auto name) {
            for (auto it = map.begin(); it != map.end();) {
                if (FAILED(get(*it))) { missing(name(*it)); it = map.erase(it); } else ++it;
            }
        };
        keep(s.render, [&](auto& r) { return device->GetRenderState(D3DRENDERSTATETYPE(r.first), &r.second); },
             [](auto& r) { return "render state " + std::to_string(r.first); });
        keep(s.stage, [&](auto& r) { return device->GetTextureStageState(r.first.first, D3DTEXTURESTAGESTATETYPE(r.first.second), &r.second); },
             [](auto& r) { return "texture stage state " + std::to_string(r.first.second) + " of stage " + std::to_string(r.first.first); });
        keep(s.sampler, [&](auto& r) { return device->GetSamplerState(r.first.first, D3DSAMPLERSTATETYPE(r.first.second), &r.second); },
             [](auto& r) { return "sampler state " + std::to_string(r.first.second) + " of sampler " + std::to_string(r.first.first); });
        keep(s.textures, [&](auto& t) { t.second = nullptr; return device->GetTexture(t.first, &t.second); },
             [](auto& t) { return "the texture of sampler " + std::to_string(t.first); });
        if (s.vertexShader && FAILED(device->GetVertexShader(&s.vs))) { missing("the vertex shader"); s.vertexShader = false; s.vs = nullptr; }
        if (s.pixelShader && FAILED(device->GetPixelShader(&s.ps))) { missing("the pixel shader"); s.pixelShader = false; s.ps = nullptr; }
        keep(s.floats, [&](auto& c) { return c.first.first ? device->GetVertexShaderConstantF(c.first.second, c.second.data(), 1)
                                                          : device->GetPixelShaderConstantF(c.first.second, c.second.data(), 1); },
             [](auto& c) { return "float shader constant " + std::to_string(c.first.second); });
        keep(s.ints, [&](auto& c) { return c.first.first ? device->GetVertexShaderConstantI(c.first.second, c.second.data(), 1)
                                                        : device->GetPixelShaderConstantI(c.first.second, c.second.data(), 1); },
             [](auto& c) { return "integer shader constant " + std::to_string(c.first.second); });
        keep(s.bools, [&](auto& c) { return c.first.first ? device->GetVertexShaderConstantB(c.first.second, &c.second, 1)
                                                         : device->GetPixelShaderConstantB(c.first.second, &c.second, 1); },
             [](auto& c) { return "boolean shader constant " + std::to_string(c.first.second); });
    }
    // Reapplies the state Begin saved, directly on the device, as D3DX's state block does (also
    // when a state manager is installed: the saved values were read from the device).
    void restore() {
        std::unique_ptr<SavedState> s = std::move(saved);
        for (const auto& r : s->render) device->SetRenderState(D3DRENDERSTATETYPE(r.first), r.second);
        for (const auto& r : s->stage) device->SetTextureStageState(r.first.first, D3DTEXTURESTAGESTATETYPE(r.first.second), r.second);
        for (const auto& r : s->sampler) device->SetSamplerState(r.first.first, D3DSAMPLERSTATETYPE(r.first.second), r.second);
        for (const auto& t : s->textures) device->SetTexture(t.first, t.second);
        if (s->vertexShader) device->SetVertexShader(s->vs);
        if (s->pixelShader) device->SetPixelShader(s->ps);
        for (const auto& c : s->floats)
            c.first.first ? device->SetVertexShaderConstantF(c.first.second, c.second.data(), 1)
                          : device->SetPixelShaderConstantF(c.first.second, c.second.data(), 1);
        for (const auto& c : s->ints)
            c.first.first ? device->SetVertexShaderConstantI(c.first.second, c.second.data(), 1)
                          : device->SetPixelShaderConstantI(c.first.second, c.second.data(), 1);
        for (const auto& c : s->bools)
            c.first.first ? device->SetVertexShaderConstantB(c.first.second, &c.second, 1)
                          : device->SetPixelShaderConstantB(c.first.second, &c.second, 1);
    }
    HRESULT STDMETHODCALLTYPE Begin(UINT* passes, DWORD f) override {
        if (current < 0 || begun) return D3DERR_INVALIDCALL;
        if (!(f & D3DXFX_DONOTSAVESTATE)) {
            // [CHOSEN, not recovered] D3DX semantics: Begin saves the state the technique sets and
            // End restores it. BF2 passes 0 only from RendDX9 Effect::begin while
            // shadermanager.byPassDiceStates is set; those effects carry no state manager. Every
            // other Begin in RendDX9.dll passes 7. Partial saving and saving under a state manager
            // have no call site; they follow the same D3DX rules (save()).
            save(f);
        }
        begun = true; active = -1;
        if (passes) *passes = UINT(runtime.description().techniques[std::size_t(current)].passes.size());
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE BeginPass(UINT pass) override {
        ensureShaders();
        if (!begun || active >= 0 || pass >= runtime.description().techniques[std::size_t(current)].passes.size())
            return D3DERR_INVALIDCALL;
        active = int(pass);
        if (bf2metal::drawTraceEnabled() || bf2hitch::enabled())
            bf2metal::setDrawTraceLabel((traceKey + " " + runtime.description().techniques[std::size_t(current)].name + "#" +
                                         std::to_string(pass)).c_str());
        return apply(false);
    }
    HRESULT STDMETHODCALLTYPE CommitChanges() override {
        pullShared();
        if (active < 0) return D3D_OK; // No active pass: nothing to commit.
        return applied == generation ? D3D_OK : apply(true);
    }
    HRESULT STDMETHODCALLTYPE EndPass() override {
        if (active < 0) return D3DERR_INVALIDCALL; active = -1;
        if (bf2metal::drawTraceEnabled()) bf2metal::setDrawTraceLabel(nullptr);
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE End() override {
        begun = false; active = -1;
        if (saved) restore();
        return D3D_OK;
    }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice9** out) override {
        if (!out) return D3DERR_INVALIDCALL; *out = device; device->AddRef(); return D3D_OK;
    }
    // [CHOSEN, not recovered] This effect owns no D3DPOOL_DEFAULT resources or state blocks:
    // the native device keeps every resource across Reset, so there is nothing to release.
    HRESULT STDMETHODCALLTYPE OnLostDevice() override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE OnResetDevice() override { return D3D_OK; }
    HRESULT STDMETHODCALLTYPE SetStateManager(ID3DXEffectStateManager* m) override {
        if (m) m->AddRef();
        if (manager) manager->Release();
        manager = m; return D3D_OK;
    }
};

void Applier::state(std::uint32_t id, std::uint32_t index, const bf2fx::Value& value)
{
    if (!dynamicState() && commit) return;
    effect.applyState(id, index, value);
}
void Applier::shader(std::uint32_t stage, const bf2fx::Shader* s)
{
    if (!dynamicState() && commit) return;
    effect.applyShader(stage, s);
}
void Applier::constants(const bf2fx::Shader& s, const bf2fx::Registers& r) { effect.applyConstants(s, r); }
void Applier::sampler(std::uint32_t stage, std::uint32_t slot, const bf2fx::Value& s, const std::shared_ptr<void>& t)
{
    if (cursor < pass.states.size()) ++cursor; // Sampler[n] pass state; its texture is always reissued.
    effect.applySampler(stage, slot, s, t, commit);
}

// ---- ID3DXEffectCompiler ------------------------------------------------------------------------
class Compiler final : public EffectBase<ID3DXEffectCompiler> {
public:
    std::uint32_t kind, compileResult; std::string log;
    Compiler(bf2fx::Entry& entry, DWORD f)
        : EffectBase(entry, f), kind(entry.kind), compileResult(entry.compileResult), log(entry.log) {}
    // [CHOSEN, not recovered] Literal parameters are not modelled: the native request already
    // fixed the compiled values. No BF2 code calls either method (the retail compiler sites use
    // only CompileEffect).
    HRESULT STDMETHODCALLTYPE SetLiteral(D3DXHANDLE, BOOL) override { return refuse("ID3DXEffectCompiler::SetLiteral"); }
    HRESULT STDMETHODCALLTYPE GetLiteral(D3DXHANDLE, BOOL* out) override {
        if (out) *out = FALSE; return refuse("ID3DXEffectCompiler::GetLiteral");
    }
    HRESULT STDMETHODCALLTYPE CompileEffect(DWORD f, ID3DXBuffer** effect, ID3DXBuffer** errors) override {
        if (errors) *errors = nullptr;
        if (!effect) return D3DERR_INVALIDCALL;
        *effect = nullptr;
        // The native compiler uses the creation flags; another flag set is another key.
        // [CHOSEN, not recovered] Both cases have no BF2 call (CompileEffect follows creation with
        // the same flags and no parameter writes): the creation-time result is returned.
        if (f != flags) refuse("ID3DXEffectCompiler::CompileEffect", "flags differ from D3DXCreateEffectCompiler (creation flags used)");
        if (generation != 1) refuse("ID3DXEffectCompiler::CompileEffect", "parameter values changed before compilation (initial values used)");
        if (kind != bf2fx::EffectEntry) { // D3DX parsed this source but refused to compile it.
            if (errors && !log.empty()) *errors = textBlob(log);
            return HRESULT(compileResult);
        }
        auto* blob = new (std::nothrow) Blob(fx2);
        if (!blob) return E_OUTOFMEMORY;
        *effect = blob; return D3D_OK;
    }
};
} // namespace

extern "C" HRESULT WINAPI D3DXCreateEffect(IDirect3DDevice9* pDevice, LPCVOID pSrcData, UINT SrcDataLen,
    const D3DXMACRO* pDefines, ID3DXInclude* pInclude, DWORD Flags, ID3DXEffectPool* pPool,
    ID3DXEffect** ppEffect, ID3DXBuffer** ppCompilationErrors)
{
    if (ppCompilationErrors) *ppCompilationErrors = nullptr;
    if (!pDevice || !pSrcData || !SrcDataLen || !ppEffect) return D3DERR_INVALIDCALL;
    *ppEffect = nullptr;
    auto* pool = pPool ? dynamic_cast<Pool*>(pPool) : nullptr;
    if (pPool && !pool) return refuse("D3DXCreateEffect", "a pool D3DXCreateEffectPool did not create");
    bf2fx::Entry entry;
    Deferred deferred;
    const HRESULT hr = loadEntry("D3DXCreateEffect", pSrcData, SrcDataLen, pDefines, pInclude, Flags, entry, ppCompilationErrors, deferred);
    if (FAILED(hr)) return hr;
    auto* effect = new (std::nothrow) Effect(pDevice, entry, Flags, deferred);
    if (!effect) return E_OUTOFMEMORY;
    // Effect data the runtime refused at construction (Runtime.h) fails creation like a bad effect.
    if (!effect->runtime.valid()) { effect->Release(); return E_FAIL; }
    if (pool) {
        effect->poolObject = pool; pool->AddRef();
        const HRESULT joined = effect->joinPool(pool->store);
        if (FAILED(joined)) { effect->Release(); return joined; }
    }
    if (!effect->deferred) {
        const HRESULT created = effect->createShaders();
        if (FAILED(created)) { effect->Release(); return created; }
    }
    *ppEffect = effect;
    return D3D_OK;
}

// [CHOSEN, not recovered] D3DX semantics: a new, empty pool for D3DXCreateEffect's shared parameters.
extern "C" HRESULT WINAPI D3DXCreateEffectPool(ID3DXEffectPool** ppPool)
{
    if (!ppPool) return D3DERR_INVALIDCALL;
    *ppPool = new (std::nothrow) Pool;
    return *ppPool ? D3D_OK : E_OUTOFMEMORY;
}

extern "C" HRESULT WINAPI D3DXCreateEffectCompiler(LPCSTR pSrcData, UINT SrcDataLen, const D3DXMACRO* pDefines,
    ID3DXInclude* pInclude, DWORD Flags, ID3DXEffectCompiler** ppCompiler, ID3DXBuffer** ppParseErrors)
{
    if (ppParseErrors) *ppParseErrors = nullptr;
    if (!pSrcData || !SrcDataLen || !ppCompiler) return D3DERR_INVALIDCALL;
    *ppCompiler = nullptr;
    bf2fx::Entry entry;
    Deferred deferred; // A compiler object only reflects and returns the tagged request.
    const HRESULT hr = loadEntry("D3DXCreateEffectCompiler", pSrcData, SrcDataLen, pDefines, pInclude, Flags, entry, ppParseErrors, deferred);
    if (FAILED(hr)) return hr;
    auto* compiler = new (std::nothrow) Compiler(entry, Flags);
    if (!compiler) return E_OUTOFMEMORY;
    if (!compiler->runtime.valid()) { compiler->Release(); return E_FAIL; }
    *ppCompiler = compiler;
    return D3D_OK;
}

namespace bf2metal {
void createFinishedEffectShaders()
{
    auto& effects = unsettledEffects();
    if (effects.empty()) return;
    std::vector<Effect*> finished;
    for (Effect* e : effects) if (e->buildFinished()) { finished.push_back(e); if (finished.size() == 16) break; }
    for (Effect* e : finished) e->ensureShaders();
}
std::shared_ptr<const bf2fx::Shader> translateRuntimeShader(const std::uint32_t* tokens, std::size_t bytes, bool vertex)
{
#if defined(BF2_FX_NATIVE)
    if (!tokens || !bytes) return nullptr;
    auto shader = std::make_shared<bf2fx::Shader>();
    shader->stage = vertex ? bf2fx::VertexShader : bf2fx::PixelShader;
    shader->entry = vertex ? "RuntimeVertexShader" : "RuntimePixelShader";
    const auto* first = reinterpret_cast<const std::uint8_t*>(tokens);
    shader->bytecode.assign(first, first + bytes);
    std::string error;
    if (!bf2fx::native::reflect(*shader, {}, error)) {
        std::fprintf(stderr, "bf2metal::translateRuntimeShader: MojoShader refused %s bytecode (%s)\n",
                     vertex ? "vertex" : "pixel", error.c_str());
        std::fflush(stderr);
        return nullptr;
    }
    return shader;
#else
    (void)tokens; (void)bytes; (void)vertex;
    return nullptr;
#endif
}
} // namespace bf2metal
#endif
