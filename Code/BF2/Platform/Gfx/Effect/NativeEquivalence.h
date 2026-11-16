// [CHOSEN, not recovered] Opt-in native compiler equivalence evidence; OpenBF2 issue #36.
#pragma once
#include "FxBuild.h"
#include <functional>
#include <map>
#include <mutex>

namespace bf2fx { namespace native {

// Identifies the exact final compiler input, including the generated entry wrapper.
std::string equivalenceInputKey(const ShaderJob& job, const std::string& source);

struct EquivalenceStats {
    size_t eligible = 0, hits = 0, compiled = 0, fallbacks = 0;
    size_t entries = 0, bytes = 0, peakBytes = 0;
};

// A manifest attests native full/trim equivalence, never D3DX acceptance. Only successful
// bytecode with its attested digest is retained. Failed attempts leave caller outputs untouched.
class EquivalenceCache {
public:
    using Compile = std::function<bool(const ShaderJob&, const std::string&, Bytes&, std::string&)>;
    EquivalenceCache(const std::string& manifest, const std::string& fingerprint,
                     size_t byteLimit = 32 * 1024 * 1024, size_t entryLimit = 4096);
    bool tryCompile(const ShaderJob& job, const std::string& source, const Compile& compile, Bytes& output);
    EquivalenceStats stats() const;
    size_t auditedInputs() const { return expected_.size(); }
private:
    struct Cached { Bytes code; std::string digest; uint64_t age = 0; };
    std::map<std::string, std::string> expected_;
    std::map<std::string, Cached> cache_;
    uint64_t age_ = 0;
    const size_t byteLimit_, entryLimit_;
    mutable std::mutex mutex_;
    EquivalenceStats stats_;
};

} }
