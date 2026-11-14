// [CHOSEN, not recovered] Windows reference run: D3DXCompileShader (retail d3dx9_25) over the shader
// jobs `nativefx export-jobs` wrote (JOBSDIR/jobs.tsv rows key, profile, entry, loop, flags and
// JOBSDIR/<key>.hlsl). One output file, JOBSDIR/d3dx_jobs_result.txt:
//   d3dx <dll path>
//   job <key> <profile> loop=<0|1> hr=0x........ words=<n> <hex words...>   (compiled)
//   job <key> <profile> loop=<0|1> hr=0x........ error: <one line> skipvalidation_hr=0x........
//       [skipvalidation_words=<n> <hex words...>]                             (refused; retried with
//       D3DXSHADER_SKIPVALIDATION to tell validation refusals from compile refusals)
//   summary jobs=<n> compiled=<n> refused=<n> unreadable=<n> flag_retries=<n>
// (" flags=0" after hr: the job's flags gave D3DERR_INVALIDCALL and it was compiled without them.)
// The inputs are game-derived and stay out of the repository; so does the result.
// Exit status 1 when no job ran, or any job was refused or unreadable (all jobs come from requests
// D3DX accepted, so a refusal means the job extraction differs from the effect compiler's).
// D3DX is loaded at run time: argv[2], else %D3DX9_DLL%, else d3dx9_25.dll.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
struct Buffer : IUnknown {  // ID3DXBuffer
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};
struct Macro { LPCSTR name, definition; };
typedef HRESULT(WINAPI* CompileFn)(LPCSTR, UINT, const Macro*, void*, LPCSTR, LPCSTR, DWORD, Buffer**, Buffer**, void**);

bool readFile(const std::string& path, std::string& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[65536];
    size_t n;
    out.clear();
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    std::fclose(f);
    return true;
}

std::string oneLine(Buffer* b) {
    if (!b) return "";
    std::string s(static_cast<const char*>(b->GetBufferPointer()), b->GetBufferSize());
    while (!s.empty() && (s.back() == '\0' || s.back() == '\n' || s.back() == '\r')) s.pop_back();
    for (char& c : s) if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    if (s.size() > 2000) s = s.substr(0, 2000) + "...";
    return s;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: d3dx_jobs JOBSDIR [d3dx9.dll]\n"); return 2; }
    const std::string dir = argv[1];
    HMODULE dx = nullptr;
    char envDll[MAX_PATH] = {};
    if (argc > 2) dx = LoadLibraryA(argv[2]);
    else if (GetEnvironmentVariableA("D3DX9_DLL", envDll, MAX_PATH)) dx = LoadLibraryA(envDll);
    else dx = LoadLibraryA("d3dx9_25.dll");
    if (!dx) { std::fprintf(stderr, "no d3dx9 dll\n"); return 3; }
    CompileFn compile = reinterpret_cast<CompileFn>(reinterpret_cast<void*>(GetProcAddress(dx, "D3DXCompileShader")));
    if (!compile) { std::fprintf(stderr, "no D3DXCompileShader\n"); return 3; }
    char dxPath[MAX_PATH] = {};
    GetModuleFileNameA(dx, dxPath, MAX_PATH);

    std::string table;
    if (!readFile(dir + "\\jobs.tsv", table)) { std::fprintf(stderr, "cannot read %s\\jobs.tsv\n", dir.c_str()); return 3; }
    FILE* out = std::fopen((dir + "\\d3dx_jobs_result.txt").c_str(), "w");
    if (!out) { std::fprintf(stderr, "cannot write the result file\n"); return 3; }
    std::fprintf(out, "d3dx %s\n", dxPath);
    unsigned jobs = 0, compiled = 0, refused = 0, unreadable = 0, flagRetries = 0, skipValidated = 0;
    size_t pos = 0;
    while (pos < table.size()) {
        size_t end = table.find('\n', pos);
        if (end == std::string::npos) end = table.size();
        std::string line = table.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> f;
        size_t a = 0, b;
        while ((b = line.find('\t', a)) != std::string::npos) { f.push_back(line.substr(a, b - a)); a = b + 1; }
        f.push_back(line.substr(a));
        if (f.size() < 5) { ++unreadable; std::fprintf(out, "bad row %s\n", line.c_str()); continue; }
        ++jobs;
        std::string src;
        if (!readFile(dir + "\\" + f[0] + ".hlsl", src)) {
            ++unreadable;
            std::fprintf(out, "job %s %s loop=%s unreadable\n", f[0].c_str(), f[1].c_str(), f[3].c_str());
            continue;
        }
        Buffer *code = nullptr, *errors = nullptr;
        // D3DXSHADER_NO_PRESHADER (0x100) is an effect compiler flag; D3DXCompileShader answers
        // D3DERR_INVALIDCALL to it (a first run refused all 3591 jobs that way). The engine's effect
        // compile passes it, and it only concerns preshaders, which a single shader does not have.
        const DWORD flags = DWORD(std::strtoul(f[4].c_str(), nullptr, 10)) & ~DWORD(0x100);
        HRESULT hr = compile(src.c_str(), UINT(src.size()), nullptr, nullptr, f[2].c_str(), f[1].c_str(), flags, &code, &errors, nullptr);
        bool retried = false;
        if (hr == HRESULT(0x8876086cL) && flags) {  // D3DERR_INVALIDCALL: flag set not accepted; once more without
            if (errors) { errors->Release(); errors = nullptr; }
            hr = compile(src.c_str(), UINT(src.size()), nullptr, nullptr, f[2].c_str(), f[1].c_str(), 0, &code, &errors, nullptr);
            retried = true;
            ++flagRetries;
        }
        std::fprintf(out, "job %s %s loop=%s hr=0x%08lx%s", f[0].c_str(), f[1].c_str(), f[3].c_str(), hr, retried ? " flags=0" : "");
        if (SUCCEEDED(hr) && code) {
            ++compiled;
            const DWORD* w = static_cast<const DWORD*>(code->GetBufferPointer());
            const DWORD n = code->GetBufferSize() / 4;
            std::fprintf(out, " words=%lu", n);
            for (DWORD k = 0; k < n; ++k) std::fprintf(out, " %08lx", w[k]);
        } else {
            ++refused;
            std::fprintf(out, " error: %s", oneLine(errors).c_str());
            // Validation-only refusal? The same compile with D3DXSHADER_SKIPVALIDATION (0x2): an
            // effect can hold a shader the validator rejects (the technique then fails to validate).
            Buffer *code2 = nullptr, *errors2 = nullptr;
            HRESULT hs = compile(src.c_str(), UINT(src.size()), nullptr, nullptr, f[2].c_str(), f[1].c_str(), (retried ? 0 : flags) | 2,
                                 &code2, &errors2, nullptr);
            std::fprintf(out, " skipvalidation_hr=0x%08lx", hs);
            if (SUCCEEDED(hs) && code2) {
                ++skipValidated;
                const DWORD* w = static_cast<const DWORD*>(code2->GetBufferPointer());
                const DWORD n = code2->GetBufferSize() / 4;
                std::fprintf(out, " skipvalidation_words=%lu", n);
                for (DWORD k = 0; k < n; ++k) std::fprintf(out, " %08lx", w[k]);
            }
            if (code2) code2->Release();
            if (errors2) errors2->Release();
        }
        std::fprintf(out, "\n");
        if (code) code->Release();
        if (errors) errors->Release();
    }
    std::fprintf(out, "summary jobs=%u compiled=%u refused=%u unreadable=%u flag_retries=%u refused_but_skipvalidation_compiles=%u\n",
                 jobs, compiled, refused, unreadable, flagRetries, skipValidated);
    std::fclose(out);
    std::printf("d3dx_jobs: %u jobs, %u compiled, %u refused, %u unreadable\n", jobs, compiled, refused, unreadable);
    return (!jobs || refused || unreadable) ? 1 : 0;
}
