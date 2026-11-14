// [CHOSEN, not recovered] Windows reference generator for fixed-function vertex processing (0.3.0
// pieces 1-3). Runs every case of ff_cases.cpp through the real d3d9 runtime's ProcessVertices with
// software vertex processing (the runtime's own CPU T&L, independent of the GPU driver) and writes
// what it produced, plus the exact inputs it used, as text:
//
//   ff_reference.exe OUT.txt
//
// Output (floats as C99 hex, %a, so nothing is lost):
//   device <adapter description> | <driver version>
//   case <index> <name>
//   matrix <world0|world1|world2|world3|view|projection|texture0|texture1> <16 floats>
//   vertex <i> <x> <y> <z> <rhw> <diffuse hex> <specular hex> <t0 x y z w> <t1 x y z w>
// The macOS checks read the matrices from this file instead of rebuilding them (libm differences).
// Build/run: ff_reference.cmd (MSVC x86, d3d9.lib). No game content is read or written.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <string>
#include <cstring>
#include "ff_cases.h"

using namespace ffref;

#include "ff_run.inc"


int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: ff_reference OUT.txt\n"); return 2; }
    FILE* f = std::fopen(argv[1], "w");
    if (!f) return 2;
    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(0); wc.lpszClassName = "ffref";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("ffref", "ffref", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, 0, 0, wc.hInstance, 0);
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::fprintf(stderr, "Direct3DCreate9 failed\n"); return 3; }
    D3DADAPTER_IDENTIFIER9 id = {};
    d3d->GetAdapterIdentifier(0, 0, &id);
    std::fprintf(f, "device %s | %u.%u.%u.%u\n", id.Description,
                 HIWORD(id.DriverVersion.HighPart), LOWORD(id.DriverVersion.HighPart),
                 HIWORD(id.DriverVersion.LowPart), LOWORD(id.DriverVersion.LowPart));
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = TRUE; pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.BackBufferFormat = D3DFMT_UNKNOWN; pp.hDeviceWindow = hwnd;
    IDirect3DDevice9* dev = 0;
    HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &dev);
    if (FAILED(hr)) hr = d3d->CreateDevice(0, D3DDEVTYPE_REF, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &dev);
    if (FAILED(hr)) { std::fprintf(stderr, "CreateDevice failed 0x%08lx\n", hr); return 3; }

    const FFRunResult res = ffRun(f, dev);
    int failures = res.failures; const int written = res.written, vertexLines = res.vertexLines;
    dev->Release(); d3d->Release();
    const bool closed = std::fclose(f) == 0;
    if (!closed) ++failures;
    // Forward slashes in the message: the path is echoed back through prlctl, which ate backslashes.
    std::string shown(argv[1]);
    for (char& ch : shown) if (ch == '\\') ch = '/';
    std::printf("ff_reference: %d of %d cases, %d vertex lines written to %s%s\n", written, kCaseCount, vertexLines,
                shown.c_str(), failures ? " (FAILED, see the error lines)" : "");
    return failures ? 4 : 0;
}
