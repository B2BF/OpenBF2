// [CHOSEN, not recovered] Offline native compiler evidence generator; OpenBF2 issue #36.
#include "Native.h"
#include "NativeEquivalence.h"
#include "NativeVkd3d.h"
#include "Wire.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace bf2fx;
namespace {
struct Job { ShaderJob shader; std::string request, full, trimmed, key, trimKey; };
struct Compiled { bool ok = false; Bytes code; std::string error; };

std::vector<Job> collectJobs(const std::string& listing) {
    std::ifstream input(listing);
    if (!input) throw std::runtime_error("Cannot open request listing");
    std::vector<Job> jobs;
    std::string path;
    while (std::getline(input, path)) {
        if (!path.empty() && path.back() == '\r') path.pop_back();
        if (path.empty()) continue;
        const Request request = unenvelope<Request>(readFile(path), "request");
        std::string text, log;
        if (!native::preprocessSource(request, text, log)) throw std::runtime_error("Preprocessing failed: " + path);
        src::Effect effect; src::Error error;
        if (!src::parse(text, effect, error)) throw std::runtime_error("Effect parsing failed: " + path);
        std::vector<std::pair<uint32_t, const src::Expr*>> values;
        std::function<void(const src::Expr&, uint32_t)> collect = [&](const src::Expr& e, uint32_t stage) {
            if (e.kind == src::Expr::Compile || e.kind == src::Expr::Asm) values.push_back({stage, &e});
            else if (e.kind == src::Expr::List) for (const auto& a : e.args) collect(*a, stage);
        };
        for (const auto& g : effect.globals)
            if (g.init && (g.type.base == src::Base::VertexShader || g.type.base == src::Base::PixelShader))
                collect(*g.init, g.type.base == src::Base::VertexShader ? VertexShader : PixelShader);
        for (const auto& t : effect.techniques) for (const auto& p : t.passes) for (const auto& s : p.states) {
            if (src::iequals(s.name, "VertexShader")) collect(*s.value, VertexShader);
            if (src::iequals(s.name, "PixelShader")) collect(*s.value, PixelShader);
        }
        const std::string texts[2] = {compilerText(effect, text, PixelShader), compilerText(effect, text, VertexShader)};
        std::set<std::string> seen;
        for (const auto& v : values) {
            Job job; std::string message;
            if (!shaderJob(effect, text, *v.second, v.first, job.shader, message))
                throw std::runtime_error("Shader job failed: " + path + ": " + message);
            if (!seen.insert(std::to_string(job.shader.stage) + "|" + job.shader.key).second) continue;
            if (!job.shader.asmText.empty() || job.shader.function.empty()) continue;
            job.request = path;
            job.full = texts[job.shader.stage == VertexShader ? 1 : 0] + job.shader.wrapper;
            if (!native::trimSource(job.full, job.shader.entry, job.trimmed))
                throw std::runtime_error("Cannot trim shader job: " + path);
            job.key = native::equivalenceInputKey(job.shader, job.full);
            job.trimKey = native::equivalenceInputKey(job.shader, job.trimmed);
            jobs.push_back(std::move(job));
        }
    }
    return jobs;
}
Compiled compile(const Job& job, const std::string& source) {
    Compiled result;
    // Bypass every runtime memo/manifest: this is the independent full compiler path.
    result.ok = native::compileJob(job.shader, source, result.code, result.error);
    return result;
}
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: bf2_fx_equivalence REQUEST_LIST OUTPUT_MANIFEST WORKERS\n";
        return 2;
    }
    try {
        setenv("VKD3D_SHADER_DEBUG", "none", 1);
        const unsigned workers = unsigned(std::stoul(argv[3]));
        if (!workers || workers > 8) throw std::runtime_error("WORKERS must be between 1 and 8");
        const auto start = std::chrono::steady_clock::now();
        const auto jobs = collectJobs(argv[1]);
        if (jobs.empty()) throw std::runtime_error("No HLSL jobs to audit");
        std::map<std::string, std::shared_future<Compiled>> trimmed;
        std::map<std::string, std::string> rows;
        std::mutex memoMutex, resultMutex;
        std::atomic<size_t> next{0};
        size_t mismatches = 0, refused = 0;
        std::string workerError;
        std::ostringstream audit;
        audit << "request\tfull_input\ttrimmed_input\tfull_ok\ttrim_ok\tbytecode\n";
        auto work = [&] {
            try {
                for (size_t i; (i = next++) < jobs.size();) {
                    const auto& job = jobs[i];
                    const Compiled full = compile(job, job.full);
                    std::promise<Compiled> mine;
                    std::shared_future<Compiled> future;
                    bool owner = false;
                    {
                        std::lock_guard<std::mutex> lock(memoMutex);
                        auto found = trimmed.find(job.trimKey);
                        if (found != trimmed.end()) future = found->second;
                        else { future = mine.get_future().share(); trimmed[job.trimKey] = future; owner = true; }
                    }
                    if (owner) mine.set_value(compile(job, job.trimmed));
                    const auto& reduced = future.get();
                    const bool same = full.ok == reduced.ok && (!full.ok || full.code == reduced.code);
                    const std::string hash = full.ok ? sha256(full.code) : "refused";
                    std::lock_guard<std::mutex> lock(resultMutex);
                    audit << job.request << '\t' << job.key << '\t' << job.trimKey << '\t'
                          << full.ok << '\t' << reduced.ok << '\t' << hash << '\n';
                    if (!same) ++mismatches;
                    else if (!full.ok) ++refused;
                    else {
                        const auto added = rows.emplace(job.key, hash);
                        if (!added.second && added.first->second != hash) ++mismatches;
                    }
                }
            } catch (const std::exception& e) {
                std::lock_guard<std::mutex> lock(resultMutex);
                workerError = e.what();
            }
        };
        std::vector<std::thread> threads;
        for (unsigned i = 1; i < workers; ++i) threads.emplace_back(work);
        work();
        for (auto& thread : threads) thread.join();
        const std::string output = argv[2];
        const std::string provenance = audit.str();
        writeFile(output + ".audit.tsv", Bytes(provenance.begin(), provenance.end()));
        if (!workerError.empty()) throw std::runtime_error(workerError);
        if (mismatches) throw std::runtime_error(std::to_string(mismatches) + " full/trim mismatches; manifest not published");
        if (rows.size() > 100000) throw std::runtime_error("Manifest exceeds the runtime input-count bound");
        std::ostringstream manifest;
        manifest << "BF2FX-native-equivalence 1\nfingerprint " << native::compilerFingerprint()
                 << "\n# request-list-sha256 " << sha256(readFile(argv[1]))
                 << "\n# generator-sha256 " << sha256(readFile(argv[0]))
                 << "\n# audit-sha256 " << sha256(Bytes(provenance.begin(), provenance.end()))
                 << "\n# jobs " << jobs.size() << " refused " << refused << " workers " << workers << '\n';
        for (const auto& row : rows) manifest << row.first << ' ' << row.second << '\n';
        const std::string data = manifest.str();
        writeFile(output + ".tmp", Bytes(data.begin(), data.end()));
        std::filesystem::rename(output + ".tmp", output);
        std::cout << "Audited " << jobs.size() << " jobs, " << trimmed.size() << " distinct trimmed inputs; "
                  << rows.size() << " manifest rows, " << refused << " refused jobs omitted; "
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count() << " seconds\n";
    } catch (const std::exception& e) {
        std::cerr << "native equivalence: " << e.what() << '\n';
        return 1;
    }
}
