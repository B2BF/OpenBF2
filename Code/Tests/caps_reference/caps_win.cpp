// [CHOSEN, not recovered] Dumps the real D3D9 HAL device's D3DCAPS9 (adapter 0) for comparison with
// the macOS device's caps (macos_metal_device --caps). Usage: caps_win OUT.txt
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include "caps_fields.h"

int main(int argc, char** argv) {
    if (argc != 2) { std::fprintf(stderr, "usage: caps_win OUT.txt\n"); return 2; }
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { std::fprintf(stderr, "Direct3DCreate9 failed\n"); return 3; }
    D3DADAPTER_IDENTIFIER9 id = {};
    d3d->GetAdapterIdentifier(0, 0, &id);
    D3DCAPS9 caps = {};
    const HRESULT hr = d3d->GetDeviceCaps(0, D3DDEVTYPE_HAL, &caps);
    if (FAILED(hr)) { std::fprintf(stderr, "GetDeviceCaps 0x%08lx\n", hr); return 3; }
    FILE* f = std::fopen(argv[1], "w");
    if (!f) return 2;
    std::fprintf(f, "device %s | %u.%u.%u.%u\n", id.Description,
                 HIWORD(id.DriverVersion.HighPart), LOWORD(id.DriverVersion.HighPart),
                 HIWORD(id.DriverVersion.LowPart), LOWORD(id.DriverVersion.LowPart));
    bf2PrintCaps(f, caps);
    std::fclose(f);
    d3d->Release();
    std::printf("caps_reference: written\n");
    return 0;
}
