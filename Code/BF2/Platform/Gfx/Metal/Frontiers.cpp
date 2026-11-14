// [CHOSEN, not recovered] The CRT pure-virtual handler for the macOS renderer image. Every D3DX
// link boundary this file once stopped at now has a native implementation (D3DX*.cpp, Effect.cpp).
#if defined(__APPLE__) && defined(BF2_IMAGE_RENDDX9)
#include "../D3DX9Subset.h"
#include <cstdio>
#include <cstdlib>

// [CHOSEN, not recovered] Native spelling of the fatal MSVC CRT pure-virtual
// handler referenced by BundledMeshMaterial's existing destroyed-object table.
extern "C" int __cdecl _purecall(void)
{
    std::fprintf(stderr, "[FRONTIER] _purecall reached: destroyed renderer material pure-virtual slot\n");
    std::fflush(stderr);
    std::abort();
}


#endif
