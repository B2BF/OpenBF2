// [CHOSEN, not recovered] DXVK Native loading, https://github.com/kr4uzi/OpenBF2/issues/36.
#if defined(__linux__) && defined(BF2_GFX_FORWARD_TU)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

extern "C" void* bf2fwd_GetProc(const char* module, const char* name)
{
    // Keep the module loaded until process exit: D3D resources may outlive their device wrapper.
    // RTLD_LOCAL keeps the SDK exports from interposing the engine's subset entry points.
    static void* library = [] {
        void* handle = dlopen("libdxvk_d3d9.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            std::fprintf(stderr, "[FRONTIER] bf2fwd_GetProc: DXVK Native load failed: %s\n", dlerror());
            std::fflush(stderr);
            std::abort();
        }
        return handle;
    }();
    if (std::strcmp(module, "d3d9.dll") != 0) {
        std::fprintf(stderr, "[FRONTIER] bf2fwd_GetProc: unexpected library %s\n", module);
        std::fflush(stderr);
        std::abort();
    }
    dlerror();
    void* proc = dlsym(library, name);
    const char* error = dlerror();
    if (error || !proc) {
        std::fprintf(stderr, "[FRONTIER] bf2fwd_GetProc: %s: %s\n", name, error ? error : "null symbol");
        std::fflush(stderr);
        std::abort();
    }
    return proc;
}
#endif
