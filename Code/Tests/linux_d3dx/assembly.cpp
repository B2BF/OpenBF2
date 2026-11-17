// [TEMP] [CHOSEN, not recovered] Public D3DX9_25 assembler goldens for the native AA probes.
// D3DXAssembleShader, null macros/include, flags 0. These test programs contain no game assets.
#include "../../BF2/Platform/Gfx/Metal/D3DXHelpers.h"
#include <cstdio>
#include <cstring>

static bool check(const char* source, const DWORD* expected, std::size_t size) {
    ID3DXBuffer *code = nullptr, *errors = nullptr;
    const HRESULT result = D3DXAssembleShader(source, std::strlen(source), nullptr, nullptr, 0, &code, &errors);
    const bool matches = result == D3D_OK && code && !errors && code->GetBufferSize() == size
        && !std::memcmp(code->GetBufferPointer(), expected, size);
    if (!matches) std::fprintf(stderr, "D3DX assembly differs from original DLL: %s\n", source);
    if (code) code->Release();
    if (errors) errors->Release();
    return matches;
}

int main() {
    // The declaration remains in vs_1_1 bytecode. Without it DXVK has no POSITION0 input
    // mapping and this probe draws no pixels, even though shader creation succeeds.
    const DWORD vertex[] = {0xfffe0101, 0x1f, 0x80000000, 0x900f0000,
                            1, 0xc00f0000, 0x90e40000, 0xffff};
    const DWORD opaque[] = {0xffff0101, 81, 0xa00f0000, 0x3f800000, 0x3f800000, 0x3f800000,
                            0, 1, 0x800f0000, 0xa0e40000, 0xffff};
    const DWORD transparent[] = {0xffff0101, 81, 0xa00f0000, 0x3f800000, 0x3f800000, 0x3f800000,
                                 0x3f000000, 1, 0x800f0000, 0xa0e40000, 0xffff};
    bool ok = check("vs.1.1\ndcl_position0 v0\nmov oPos, v0", vertex, sizeof vertex);
    ok = check("\tVS.1.1\t\n\tdcl_position0 v0\t\n\tmov oPos, v0\t\n", vertex, sizeof vertex) && ok;
    ok = check("ps.1.1\ndef c0, 1,1,1,0\nmov r0,c0", opaque, sizeof opaque) && ok;
    ok = check("ps.1.1\ndef c0, 1,1,1,0.5\nmov r0,c0", transparent, sizeof transparent) && ok;
    if (!ok) return 1;
    std::puts("PASS: native AA assembly matches original D3DX9_25 tokens");
    return 0;
}
