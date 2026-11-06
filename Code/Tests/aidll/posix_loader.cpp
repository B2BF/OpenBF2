// [TEMP] [CHOSEN, not recovered] Engine-free POSIX loader fixture. Does not call initDll,
// create host services, bind sockets or establish AI initialization/gameplay coverage.
#include "../../BF2/Core/PluginDll.h"
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>

// Satisfy references from the unused LoadLibraryBf body. Accidental invocation stops visibly.
namespace dice { namespace hfe {
class ClassManager;
ClassManager* classManager = nullptr;
namespace memory {
void memory_update_modules(const char*)
{
    std::fputs("[TEMP] POSIX loader fixture unexpectedly called memory_update_modules\n", stderr);
    std::fflush(stderr);
    std::abort();
}
}
} }

int main()
{
    void* library = ::LoadLibrary(".\\aidll.so");
    if (!library)
    {
        std::fprintf(stderr, "AI library load failed: %s\n", ::dlerror());
        return 1;
    }
    const bool exports = ::GetProcAddress(library, "initDll") != nullptr
                      && ::GetProcAddress(library, "deinitDll") != nullptr;
    const bool privateGlobals = ::GetProcAddress(library, "_ZN4dice3hfe12classManagerE") == nullptr;
    const bool closed = ::FreeLibrary(library);
    if (!exports || !privateGlobals || !closed)
    {
        std::fprintf(stderr, "AI loader check failed: exports=%d privateGlobals=%d closed=%d\n",
                     exports, privateGlobals, closed);
        return 1;
    }
    std::puts("AI library loaded, handshake exports found, globals private, library closed; initDll not called");
    return 0;
}
