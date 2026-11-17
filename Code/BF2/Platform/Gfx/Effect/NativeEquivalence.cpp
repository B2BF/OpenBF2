// [CHOSEN, not recovered] Exact-input native shader reuse; kr4uzi/OpenBF2#36.
#if defined(__APPLE__) || (defined(__linux__) && defined(BF2_GFX_DXVK))
#include "NativeEquivalence.h"
#include "Native.h"
#include "Wire.h"
#include <algorithm>
#include <sstream>

namespace bf2fx { namespace native {
namespace {
bool digest(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}
}

std::string equivalenceInputKey(const ShaderJob& job, const std::string& source) {
    Writer w;
    // Invocation options and compiler implementation are bound by the manifest fingerprint.
    w(std::string("BF2FX-native-equivalence-input"), uint32_t(1), job.stage, job.profile,
      job.entry, job.function, sha256(Bytes(source.begin(), source.end())));
    return sha256(w.bytes);
}

EquivalenceCache::EquivalenceCache(const std::string& manifest, const std::string& fingerprint,
                                 size_t byteLimit, size_t entryLimit)
    : byteLimit_(byteLimit), entryLimit_(entryLimit) {
    // Bounds also apply to malformed or hand-edited local manifests.
    if (manifest.size() > 16 * 1024 * 1024 || !digest(fingerprint)) return;
    std::istringstream input(manifest);
    std::string line;
    if (!std::getline(input, line) || line != "BF2FX-native-equivalence 1") return;
    if (!std::getline(input, line) || line != "fingerprint " + fingerprint) return;
    std::map<std::string, std::string> rows;
    while (std::getline(input, line)) {
        if (line.empty() || line.front() == '#') continue;
        std::istringstream row(line);
        std::string key, expected, extra;
        if (!(row >> key >> expected) || (row >> extra) || !digest(key) || !digest(expected)
                || rows.size() >= 100000 || !rows.emplace(key, expected).second) return;
    }
    expected_.swap(rows);
}

bool EquivalenceCache::tryCompile(const ShaderJob& job, const std::string& source,
                                 const Compile& compile, Bytes& output) {
    try {
    if (expected_.empty() || !job.asmText.empty() || job.function.empty()) return false;
    const auto attestation = expected_.find(equivalenceInputKey(job, source));
    if (attestation == expected_.end()) return false;
    std::string trimmed;
    if (!trimSource(source, job.entry, trimmed)) return false;
    const std::string key = equivalenceInputKey(job, trimmed);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.eligible;
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            if (it->second.digest != attestation->second) { ++stats_.fallbacks; return false; }
            it->second.age = ++age_;
            output = it->second.code;
            ++stats_.hits;
            return true;
        }
        ++stats_.compiled;
    }
    Bytes code;
    std::string error;
    const bool ok = compile(job, trimmed, code, error);
    const std::string hash = ok ? sha256(code) : std::string();
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ok || hash != attestation->second) { ++stats_.fallbacks; return false; }
    // Completed successes only: concurrent misses may duplicate work, but no failed future or
    // transient OOM can poison a later request. Retained code and metadata are both bounded.
    if (code.size() > byteLimit_ || !entryLimit_ || cache_.count(key)) { output = std::move(code); return true; }
    while (!cache_.empty() && (cache_.size() >= entryLimit_ || stats_.bytes > byteLimit_ - code.size())) {
        const auto oldest = std::min_element(cache_.begin(), cache_.end(), [](const auto& a, const auto& b) {
            return a.second.age < b.second.age;
        });
        stats_.bytes -= oldest->second.code.size();
        cache_.erase(oldest);
        stats_.entries = cache_.size();
    }
    cache_.emplace(key, Cached{code, hash, ++age_});
    stats_.bytes += code.size();
    stats_.entries = cache_.size();
    stats_.peakBytes = std::max(stats_.peakBytes, stats_.bytes);
    output = std::move(code);
    return true;
    } catch (...) {
        // The optional path must not turn allocation/compiler exceptions into a refusal.
        // The caller reruns its original full compile, including its original error handling.
        std::lock_guard<std::mutex> lock(mutex_);
        ++stats_.fallbacks;
        return false;
    }
}

EquivalenceStats EquivalenceCache::stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}
} }
#endif
