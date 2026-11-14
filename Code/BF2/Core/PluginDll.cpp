// Loads engine plugins for client and dedicated images; source path [INFERRED].
// Windows unload calls optional deinitDll and reports exceptions. Linux also deinitializes before close.

#include "PluginDll.h"

#include <cstring>
#include <string>
#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>      // dlopen / dlsym / dlclose - what the ELF's LoadLibrary trio wraps
#endif

#include "ClassManager.h"
#if defined(BF2_STATIC_PLUGINS)
#include "../../StaticPlugins/StaticPlugins.h"
#endif

#if !defined(_WIN32)
// [PLATFORM] Global-namespace library wrappers for ELF; placement is [INFERRED].
// Loading strips the optional Windows-style prefix, requires an extension and substitutes lib<stem>.so.
// Symbol lookup uses dlsym; close returns false for null. The initial two-byte comparison is unguarded.
void* LoadLibrary(const char* name)
{
    if (std::memcmp(name, ".\\", 2) == 0)
        name += 2;
    const char* const dot = std::strrchr(name, '.');
    if (!dot)
        return 0;
#if defined(BF2_STATIC_PLUGINS)
    // [CHOSEN, not recovered] The macOS client links its plugin images statically
    // (Code/StaticPlugins/StaticPlugins.h); a linked image is answered before dlopen is tried.
    if (void* image = bf2staticplugins::find(std::string(name, dot).c_str()))
        return image;
#endif
    std::string file = std::string("lib") + std::string(name, dot);
#if defined(__APPLE__)
    // [CHOSEN, not recovered] The macOS port's shared-library suffix (docs/macos-port.md).
    file.append(".dylib");
#else
    file.append(".so");
#endif
    return ::dlopen(file.c_str(), RTLD_LAZY);
}

void* GetProcAddress(void* handle, const char* name)
{
#if defined(BF2_STATIC_PLUGINS)
    if (bf2staticplugins::isStaticHandle(handle))
        return bf2staticplugins::symbol(handle, name);
#endif
    return ::dlsym(handle, name);
}

bool FreeLibrary(void* handle)
{
#if defined(BF2_STATIC_PLUGINS)
    // A statically linked image stays mapped for the life of the process; there is nothing to close.
    // It is only marked released, so the next load can rebuild its module table
    // (StaticPlugins.h, menu re-entry).
    if (bf2staticplugins::isStaticHandle(handle))
    {
        bf2staticplugins::release(handle);
        return true;
    }
#endif
    if (handle)
        return ::dlclose(handle) == 0;
    return false;
}
#endif // !_WIN32

namespace dice {
namespace hfe {

class IClassManager; // the PE export's attested interface type; ClassManager is layout-compatible

extern ClassManager* classManager;
namespace memory { void memory_update_modules(const char*); }

typedef bool (__cdecl *InitDllFn)(IClassManager*);
typedef void (__cdecl *DeinitDllFn)();

#if defined(_MSC_VER)
void* __fastcall LoadLibraryBf(const char* name)
#else
void* LoadLibraryBf(const char* name)
#endif
{
    // The missing explicit terminator is a shipped defect: a name of 128 bytes or more lets the
    // following string construction read past this buffer.  Preserve it rather than silently
    // changing the loader's observable input handling.
    char copiedName[128];
    std::strncpy(copiedName, name, sizeof(copiedName));

    // The shared loader path uses a backslash; the Linux wrapper normalizes it before loading.
    const std::string path = std::string(".") + "\\" + copiedName;
#if defined(_WIN32)
    HMODULE module = ::LoadLibraryA(path.c_str());
#else
    void* module = ::LoadLibrary(path.c_str());               // the wrapper above
#endif
    if (!module)
        return 0;

    InitDllFn initDll = reinterpret_cast<InitDllFn>(::GetProcAddress(module, "initDll"));
    if (!initDll || !initDll(reinterpret_cast<IClassManager*>(classManager)))
    {
        ::FreeLibrary(module);
        return 0;
    }

    memory::memory_update_modules(name);
    return module;
}

#if defined(_MSC_VER)
bool __fastcall FreeLibraryBf(void* handle, const char* diagnosticName)
#else
bool FreeLibraryBf(void* handle, const char* diagnosticName)
#endif
{
#if defined(_WIN32)
    try
    {
        HMODULE module = static_cast<HMODULE>(handle);
        if (module)
        {
            DeinitDllFn deinitDll =
                reinterpret_cast<DeinitDllFn>(::GetProcAddress(module, "deinitDll"));
            if (deinitDll)
                deinitDll();
        }

        // The retail body calls FreeLibrary even when handle is null.
        return ::FreeLibrary(module) != FALSE;
    }
    catch (...)
    {
        ::MessageBoxA(0, diagnosticName, "DLL unload error:", 0x16);
        return false;
    }
#else
    (void)diagnosticName;
#if defined(BF2_STATIC_PLUGINS)
    // [CHOSEN, not recovered] A linked image that retail unloads per use (SwiffPlayer, on every
    // menu exit) gets the Win32 body's deinitDll before the release, so its modules are closed,
    // unregistered and released as BF2.exe 0x446CE0 does before FreeLibrary (StaticPlugins.h).
    if (bf2staticplugins::isStaticHandle(handle) && bf2staticplugins::unloadsWithDeinit(handle))
    {
        DeinitDllFn deinitDll = reinterpret_cast<DeinitDllFn>(::GetProcAddress(handle, "deinitDll"));
        if (deinitDll)
            deinitDll();
    }
#endif
#if defined(__APPLE__) || defined(__linux__)
    // [PLATFORM] [CHOSEN, not recovered] Native-port design, not retail: a dlopen'ed image
    // (libaidll.dylib, libaidll.so) gets the Win32 body's deinitDll before dlclose. The ELF body (bf2 0x83C3400 -> FreeLibrary 0x83D3170) is the bare
    // dlclose, so AIModule::close (libaidll.so 0x3EECB0) never runs and the "AI" simple singleton
    // that the IAI constructor (0x3EE430) registered stays in the host registry. The next load's
    // IAI constructor then finds "AI" taken and leaves the new BF2AI unregistered, and
    // AIConnection::init hands back the old object. [INFERRED] Retail Linux survives this only
    // while dlopen maps the reloaded image at the closed one's address, which makes the old vtable
    // pointer valid again (not observed on a retail run). dyld does not reuse the address: dlclose unmaps the dylib and the old vtable
    // pointer dangles (the second singleplayer game faulted with pc=0 at GameServer::loadPath's
    // aiSingleton->init()). The Win32 body (BF2.exe 0x446CE0) calls deinitDll first; AIModule::close
    // then unregisters "AI" and the module's console lists, and the reload registers fresh ones.
    // Request R4 of the AI track's client hand-off.
    bool dlopened = handle != 0;
#if defined(BF2_STATIC_PLUGINS)
    dlopened = dlopened && !bf2staticplugins::isStaticHandle(handle);
#endif
    if (dlopened)
    {
        DeinitDllFn deinitDll = reinterpret_cast<DeinitDllFn>(::GetProcAddress(handle, "deinitDll"));
        if (deinitDll)
            deinitDll();
    }
#endif
    return ::FreeLibrary(handle);
#endif
}

} // namespace hfe
} // namespace dice
