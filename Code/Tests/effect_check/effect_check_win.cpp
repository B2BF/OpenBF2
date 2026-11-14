// [CHOSEN, not recovered] Windows reference run: what retail D3DX does with whole effects at run time.
// For every row of EFFDIR/effects.tsv (stem, flags; written by `nativefx export-effects`), two ways:
//  - source: D3DXCreateEffect on EFFDIR/<stem>.fx with the request's flags;
//  - compiler (BF2's own path, RendDX9/Ra/EffectManager.cpp and DiceEffect.cpp):
//    D3DXCreateEffectCompiler + CompileEffect with the flags, then D3DXCreateEffect on the compiled
//    effect with flags & ~D3DXFX_NOT_CLONEABLE,
// on a HAL device, reporting each creation result, ValidateTechnique per technique and
// FindNextValidTechnique's answer.
// Usage: effect_check EFFDIR [d3dx9.dll|-] [--compile-only [--fresh-process]]
// Default output file, EFFDIR/effect_check_result.txt:
//   d3dx <dll path>
//   device <adapter> | <HAL|REF>
//   effect <stem> source flags=<n> hr=0x........ techniques=<n> [errors: <one line>]
//   effect <stem> compiler flags=<n> create_compiler=0x........ compile=0x........ hr=0x........
//          techniques=<n> [errors: <one line>]
//   technique <stem> <source|compiler> <index> <name> passes=<n> validate=0x........
//   valid <stem> <source|compiler> <first valid technique name or (none)> hr=0x........
//   summary effects=<n> source_created=<n> compiled=<n> compiler_created=<n> techniques=<n>
//           valid=<n> invalid=<n>
// With --compile-only, no window/device or D3DXCreateEffect calls are made. Rows may include a
// third column (refusal key); two-column rows use "-". EFFDIR/effect_compile_result.txt contains:
//   compile <stem> <key or -> flags=<n> create_compiler=0x........ compile=0x........ [errors: <one line>]
//   summary effects=<n> compiled=<n> refused=<n> unreadable=<n>
// Effects counts readable inputs; refused counts readable inputs that did not compile.
// Progress is printed every 500 rows and the result file is flushed every 100 rows. --fresh-process
// compiles every row in its own child process (for E_OUTOFMEMORY reruns).
// Inputs and result are game-derived and stay out of the repository. Exit status 1 when no effect
// was read or a setup step failed; effect and validation outcomes are data, not failures.
// D3DX is loaded at run time (argv[2] unless "-", else %D3DX9_DLL%, else d3dx9_25.dll). ID3DXEffect is called
// through its vtable: QueryInterface..FindNextValidTechnique have the same slots in d3dx9_25 as in
// later versions (they differ only after ApplyParameterBlock).
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
struct Buffer : IUnknown {  // ID3DXBuffer
    virtual LPVOID STDMETHODCALLTYPE GetBufferPointer() = 0;
    virtual DWORD STDMETHODCALLTYPE GetBufferSize() = 0;
};
struct EffectDesc { LPCSTR creator; UINT parameters, techniques, functions; };   // D3DXEFFECT_DESC
struct TechniqueDesc { LPCSTR name; UINT passes, annotations; };                 // D3DXTECHNIQUE_DESC
typedef const char* Handle;                                                      // D3DXHANDLE
typedef HRESULT(WINAPI* CreateEffectFn)(IDirect3DDevice9*, const void*, UINT, const void*, void*, DWORD, void*, IUnknown**, Buffer**);
typedef HRESULT(WINAPI* CreateCompilerFn)(const char*, UINT, const void*, void*, DWORD, IUnknown**, Buffer**);
// ID3DXEffectCompiler::CompileEffect slot (IUnknown, 54 ID3DXBaseEffect methods, SetLiteral, GetLiteral).
enum { kCompileEffect = 59 };

// ID3DXEffect vtable slots (0 = QueryInterface).
enum { kRelease = 2, kGetDesc = 3, kGetTechniqueDesc = 5, kGetTechnique = 12, kValidateTechnique = 60, kFindNextValidTechnique = 61 };
template<class F> F slot(IUnknown* o, int i) { return reinterpret_cast<F>((*reinterpret_cast<void***>(o))[i]); }

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

// [CHOSEN, not recovered] Device-free compiler fixture; compilation outcomes remain result data.
struct Row { std::string stem, key; DWORD flags; };

bool readRows(const std::string& dir, std::vector<Row>& rows) {
    std::string table;
    if (!readFile(dir + "\\effects.tsv", table)) { std::fprintf(stderr, "cannot read %s\\effects.tsv\n", dir.c_str()); return false; }
    size_t pos = 0;
    while (pos < table.size()) {
        size_t end = table.find('\n', pos);
        if (end == std::string::npos) end = table.size();
        std::string line = table.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t tab = line.find('\t');
        if (line.empty() || tab == std::string::npos) continue;
        const size_t keyTab = line.find('\t', tab + 1);
        const std::string key = keyTab == std::string::npos ? "" : line.substr(keyTab + 1);
        rows.push_back({line.substr(0, tab), key.empty() ? "-" : key,
                        DWORD(std::strtoul(line.c_str() + tab + 1, nullptr, 10))});
    }
    return true;
}

// One row through D3DXCreateEffectCompiler + CompileEffect; returns the result line (no newline).
// outcome: 0 compiled, 1 refused, 2 unreadable.
std::string compileRow(const std::string& dir, const Row& row, CreateCompilerFn createCompiler, int& outcome) {
    std::string src, diagnostic;
    HRESULT hc = E_FAIL, he = E_FAIL;
    if (!readFile(dir + "\\" + row.stem + ".fx", src)) {
        outcome = 2;
        diagnostic = "unreadable";
    } else {
        IUnknown* compiler = nullptr;
        Buffer *errors = nullptr, *blob = nullptr;
        hc = createCompiler(src.c_str(), UINT(src.size()), nullptr, nullptr, row.flags, &compiler, &errors);
        if (errors) { diagnostic = oneLine(errors); errors->Release(); errors = nullptr; }
        if (SUCCEEDED(hc) && compiler) {
            he = slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, DWORD, Buffer**, Buffer**)>(compiler, kCompileEffect)(compiler, row.flags, &blob, &errors);
        }
        if (compiler) slot<ULONG(STDMETHODCALLTYPE*)(IUnknown*)>(compiler, kRelease)(compiler);
        if (errors) {
            if (!diagnostic.empty()) diagnostic += " | ";
            diagnostic += oneLine(errors);
            errors->Release();
        }
        outcome = SUCCEEDED(he) && blob ? 0 : 1;
        if (blob) blob->Release();
    }
    char buf[160];
    std::snprintf(buf, sizeof buf, " flags=%lu create_compiler=0x%08lx compile=0x%08lx", row.flags, hc, he);
    std::string line = "compile " + row.stem + " " + row.key + buf;
    if (!diagnostic.empty()) line += " errors: " + diagnostic;
    return line;
}

// Child of --fresh-process: row `index` only, its line into EFFDIR\row.tmp.
int compileOneRow(const std::string& dir, CreateCompilerFn createCompiler, size_t index) {
    std::vector<Row> rows;
    if (!readRows(dir, rows) || index >= rows.size()) return 1;
    int outcome = 0;
    const std::string line = compileRow(dir, rows[index], createCompiler, outcome);
    FILE* f = std::fopen((dir + "\\row.tmp").c_str(), "w");
    if (!f) return 1;
    std::fprintf(f, "%s\n%d\n", line.c_str(), outcome);
    std::fclose(f);
    return 0;
}

// All rows into EFFDIR\effect_compile_result.txt; with freshProcess, each row in its own child
// process (exe EFFDIR DLL --compile-only --row N), as the native E_OUTOFMEMORY study did, so one
// effect's address-space use cannot fail the next.
int compileOnly(const std::string& dir, CreateCompilerFn createCompiler, bool freshProcess, const char* dllArg) {
    std::vector<Row> rows;
    if (!readRows(dir, rows)) return 1;
    FILE* out = std::fopen((dir + "\\effect_compile_result.txt").c_str(), "w");
    if (!out) { std::fprintf(stderr, "cannot write the result file\n"); return 1; }
    unsigned effects = 0, compiled = 0, refused = 0, unreadable = 0;
    char self[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, self, MAX_PATH);
    for (size_t i = 0; i < rows.size(); ++i) {
        std::string line;
        int outcome = 1;
        if (freshProcess) {
            DeleteFileA((dir + "\\row.tmp").c_str());
            std::string cmd = std::string("\"") + self + "\" \"" + dir + "\" \"" + dllArg + "\" --compile-only --row " + std::to_string(i);
            STARTUPINFOA si = {}; si.cb = sizeof si;
            PROCESS_INFORMATION pi = {};
            std::vector<char> cmdBuf(cmd.begin(), cmd.end()); cmdBuf.push_back('\0');
            if (CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
            }
            std::string got;
            if (readFile(dir + "\\row.tmp", got)) {
                const size_t nl = got.find('\n');
                line = got.substr(0, nl);
                outcome = std::atoi(got.c_str() + nl + 1);
            } else {
                line = "compile " + rows[i].stem + " " + rows[i].key + " child failed";
                outcome = 2;
            }
        } else {
            line = compileRow(dir, rows[i], createCompiler, outcome);
        }
        if (outcome == 2) ++unreadable; else { ++effects; if (outcome == 0) ++compiled; else ++refused; }
        std::fprintf(out, "%s\n", line.c_str());
        if ((i + 1) % 100 == 0 || freshProcess) std::fflush(out);
        if ((i + 1) % 500 == 0) {
            std::printf("effect_check: %lu of %lu\n", static_cast<unsigned long>(i + 1), static_cast<unsigned long>(rows.size()));
            std::fflush(stdout);
        }
    }
    DeleteFileA((dir + "\\row.tmp").c_str());
    std::fprintf(out, "summary effects=%u compiled=%u refused=%u unreadable=%u\n", effects, compiled, refused, unreadable);
    const bool writeFailed = std::ferror(out) != 0;
    const int closeResult = std::fclose(out);
    return effects && !writeFailed && closeResult == 0 ? 0 : 1;
}
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: effect_check EFFDIR [d3dx9.dll|-] [--compile-only [--fresh-process]]\n"); return 2; }
    const std::string dir = argv[1];
    const bool compileOnlyMode = argc > 3 && std::strcmp(argv[3], "--compile-only") == 0;
    HMODULE dx = nullptr;
    char envDll[MAX_PATH] = {};
    if (argc > 2 && std::strcmp(argv[2], "-") != 0) dx = LoadLibraryA(argv[2]);
    else if (GetEnvironmentVariableA("D3DX9_DLL", envDll, MAX_PATH)) dx = LoadLibraryA(envDll);
    else dx = LoadLibraryA("d3dx9_25.dll");
    if (!dx) { std::fprintf(stderr, "no d3dx9 dll\n"); return compileOnlyMode ? 1 : 3; }
    if (compileOnlyMode) {
        CreateCompilerFn createCompiler = reinterpret_cast<CreateCompilerFn>(reinterpret_cast<void*>(GetProcAddress(dx, "D3DXCreateEffectCompiler")));
        if (!createCompiler) { std::fprintf(stderr, "no D3DXCreateEffectCompiler\n"); FreeLibrary(dx); return 1; }
        int result;
        if (argc > 5 && std::strcmp(argv[4], "--row") == 0) result = compileOneRow(dir, createCompiler, size_t(std::strtoul(argv[5], nullptr, 10)));
        else result = compileOnly(dir, createCompiler, argc > 4 && std::strcmp(argv[4], "--fresh-process") == 0, argc > 2 ? argv[2] : "-");
        FreeLibrary(dx);
        return result;
    }
    CreateEffectFn create = reinterpret_cast<CreateEffectFn>(reinterpret_cast<void*>(GetProcAddress(dx, "D3DXCreateEffect")));
    CreateCompilerFn createCompiler = reinterpret_cast<CreateCompilerFn>(reinterpret_cast<void*>(GetProcAddress(dx, "D3DXCreateEffectCompiler")));
    if (!create || !createCompiler) { std::fprintf(stderr, "no D3DXCreateEffect/D3DXCreateEffectCompiler\n"); return 3; }
    char dxPath[MAX_PATH] = {};
    GetModuleFileNameA(dx, dxPath, MAX_PATH);

    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "effchk";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("effchk", "effchk", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, 0, 0, wc.hInstance, 0);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::fprintf(stderr, "Direct3DCreate9 failed\n"); return 3; }
    D3DADAPTER_IDENTIFIER9 id = {};
    d3d->GetAdapterIdentifier(0, 0, &id);
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.hDeviceWindow = hwnd;
    IDirect3DDevice9* device = nullptr;
    const char* type = "HAL";
    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &device);
    if (FAILED(hr)) hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &device);
    if (FAILED(hr)) { type = "REF"; hr = d3d->CreateDevice(0, D3DDEVTYPE_REF, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &device); }
    if (FAILED(hr)) { std::fprintf(stderr, "CreateDevice failed 0x%08lx\n", hr); return 3; }

    std::string table;
    if (!readFile(dir + "\\effects.tsv", table)) { std::fprintf(stderr, "cannot read %s\\effects.tsv\n", dir.c_str()); return 3; }
    FILE* out = std::fopen((dir + "\\effect_check_result.txt").c_str(), "w");
    if (!out) { std::fprintf(stderr, "cannot write the result file\n"); return 3; }
    std::fprintf(out, "d3dx %s\ndevice %s | %s\n", dxPath, id.Description, type);
    unsigned effects = 0, sourceCreated = 0, compiled = 0, compilerCreated = 0, failed = 0, techniques = 0, valid = 0, invalid = 0;
    size_t pos = 0;
    while (pos < table.size()) {
        size_t end = table.find('\n', pos);
        if (end == std::string::npos) end = table.size();
        std::string line = table.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t tab = line.find('\t');
        if (line.empty() || tab == std::string::npos) continue;
        const std::string stem = line.substr(0, tab);
        const DWORD flags = DWORD(std::strtoul(line.c_str() + tab + 1, nullptr, 10));
        std::string src;
        if (!readFile(dir + "\\" + stem + ".fx", src)) { std::fprintf(out, "effect %s unreadable\n", stem.c_str()); ++failed; continue; }
        ++effects;
        // One creation path: report the effect and its techniques; true when the effect was created.
        auto inspect = [&](const char* how, IUnknown* fx) {
            EffectDesc desc = {};
            slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, EffectDesc*)>(fx, kGetDesc)(fx, &desc);
            std::fprintf(out, " techniques=%u\n", desc.techniques);
            for (UINT t = 0; t < desc.techniques; ++t) {
                Handle h = slot<Handle(STDMETHODCALLTYPE*)(IUnknown*, UINT)>(fx, kGetTechnique)(fx, t);
                TechniqueDesc td = {};
                slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, Handle, TechniqueDesc*)>(fx, kGetTechniqueDesc)(fx, h, &td);
                const HRESULT hv = slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, Handle)>(fx, kValidateTechnique)(fx, h);
                ++techniques;
                if (SUCCEEDED(hv)) ++valid; else ++invalid;
                std::fprintf(out, "technique %s %s %u %s passes=%u validate=0x%08lx\n", stem.c_str(), how, t, td.name ? td.name : "(null)", td.passes, hv);
            }
            Handle next = nullptr;
            const HRESULT hn = slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, Handle, Handle*)>(fx, kFindNextValidTechnique)(fx, nullptr, &next);
            TechniqueDesc nd = {};
            if (SUCCEEDED(hn) && next) slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, Handle, TechniqueDesc*)>(fx, kGetTechniqueDesc)(fx, next, &nd);
            std::fprintf(out, "valid %s %s %s hr=0x%08lx\n", stem.c_str(), how, nd.name ? nd.name : "(none)", hn);
            slot<ULONG(STDMETHODCALLTYPE*)(IUnknown*)>(fx, kRelease)(fx);
        };
        // Source path.
        {
            IUnknown* fx = nullptr;
            Buffer* errors = nullptr;
            hr = create(device, src.data(), UINT(src.size()), nullptr, nullptr, flags, nullptr, &fx, &errors);
            std::fprintf(out, "effect %s source flags=%lu hr=0x%08lx", stem.c_str(), flags, hr);
            if (errors) { std::fprintf(out, " errors: %s", oneLine(errors).c_str()); errors->Release(); }
            if (SUCCEEDED(hr) && fx) { ++sourceCreated; inspect("source", fx); }
            else std::fprintf(out, "\n");
        }
        // Compiler path (BF2).
        {
            IUnknown* compiler = nullptr;
            Buffer *errors = nullptr, *blob = nullptr;
            const HRESULT hc = createCompiler(src.c_str(), UINT(src.size()), nullptr, nullptr, flags, &compiler, &errors);
            if (errors) { errors->Release(); errors = nullptr; }
            HRESULT he = E_FAIL, hf = E_FAIL;
            if (SUCCEEDED(hc) && compiler) {
                he = slot<HRESULT(STDMETHODCALLTYPE*)(IUnknown*, DWORD, Buffer**, Buffer**)>(compiler, kCompileEffect)(compiler, flags, &blob, &errors);
                slot<ULONG(STDMETHODCALLTYPE*)(IUnknown*)>(compiler, kRelease)(compiler);
            }
            IUnknown* fx = nullptr;
            if (SUCCEEDED(he) && blob) {
                ++compiled;
                hf = create(device, blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, nullptr, flags & ~DWORD(0x800), nullptr, &fx, nullptr);
            }
            std::fprintf(out, "effect %s compiler flags=%lu create_compiler=0x%08lx compile=0x%08lx hr=0x%08lx", stem.c_str(), flags, hc, he, hf);
            if (errors) { std::fprintf(out, " errors: %s", oneLine(errors).c_str()); errors->Release(); }
            if (blob) blob->Release();
            if (SUCCEEDED(hf) && fx) { ++compilerCreated; inspect("compiler", fx); }
            else std::fprintf(out, "\n");
        }
    }
    std::fprintf(out, "summary effects=%u source_created=%u compiled=%u compiler_created=%u unreadable=%u techniques=%u valid=%u invalid=%u\n",
                 effects, sourceCreated, compiled, compilerCreated, failed, techniques, valid, invalid);
    std::fclose(out);
    std::printf("effect_check: %u effects, source path %u created, compiler path %u compiled / %u created, %u techniques (%u valid, %u invalid)\n",
                effects, sourceCreated, compiled, compilerCreated, techniques, valid, invalid);
    device->Release(); d3d->Release();
    return effects ? 0 : 1;
}
