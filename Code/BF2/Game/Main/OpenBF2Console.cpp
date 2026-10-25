// [CHOSEN, not recovered] The `openbf2.*` console object. This is an OpenBF2 extension, not a
// recovered Battlefield 2 1.50 command; no retail image defines it. `openbf2.addModDll <name>` loads
// a native module at run time - a mod, or an integration shim such as bf2hub. <name> is a module base
// name, normally WITHOUT an extension (e.g. `bf2hub`); this binding appends the platform's library
// extension - .dll on Windows, .so on ELF - so one config line works on every build.
// Issue: B2BF/OpenBF2#39 (approach discussed there before implementation).
//
// Where it is driven from: the dedicated server runs an optional `openbf2.con` from the active mod
// directory before Python initialization (GameServer::init), so an addModDll line there takes effect
// before the Python host comes up. It is equally callable from ServerSettings.con, Init.con, rcon,
// Python, or the server console.
//
// Safety model (discussed in the issue):
//   - Dedicated-server image only. The whole binding compiles out of the client and editor, so a
//     downloaded map cannot reach it on a player's machine.
//   - It loads native code, so it is meant for operator-controlled config only: the active mod's
//     loose openbf2.con, ServerSettings.con, Init.con, rcon, Python, or the server console. Do not
//     expose it to untrusted content.
//
// Loader contract (lenient, so both plugin styles work):
//   - A module exporting initDll(IClassManager*) is initialised through the same handshake the
//     engine uses for its own plugins (Core/PluginDll.cpp); a false return unloads it and fails.
//   - A module without initDll is kept loaded, because its DllMain has already run. That is what a
//     DllMain-based integration such as bf2hub needs: its DllMain (server path, gated on the host
//     image name containing "w32ded") patches the engine's dice_py import slots Py_InitModule4 and
//     PyImport_ImportModule, so bf2hublib and bf2.bf2hub are injected when the engine registers its
//     `host` module and imports `bf2`. This is why openbf2.con runs before Python init - load it
//     afterward and the hooks are too late.
//   - This loader does not itself reroute sockets. bf2hub's GameSpy/master redirection relied on an
//     install-time patch of the host's import table that a runtime load does not reproduce; OpenBF2
//     points the engine at the correct master-server address in source instead (see the issue).

#include "../../Core/ConsoleObjectMacro.h"

#if DEDICATED

// All STL and engine headers are included before <windows.h> so its min/max macros cannot break them.
#include "../../IO/Console/Console.h"

#include <string>
#include <vector>
#include <cstdio>
#include <cstring>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <dlfcn.h>
#endif

namespace dice {
namespace hfe {

class ClassManager;
extern ClassManager* classManager;   // Core/Globals.cpp - the host singleton handed to a plugin initDll.

namespace {

const char* const kOpenBf2ConsoleTag = "openbf2.console";

// Matches the initDll signature Core/PluginDll.cpp loads for engine plugins.
typedef bool (__cdecl* InitDllFn)(void*);

struct LoadedModule
{
    std::string requested;   // the name exactly as typed in the console
    std::string resolved;    // the platform path actually handed to the loader
    void*       handle;
};

// Function-local so it is constructed before any binding's static constructor runs and never
// becomes a load-order problem. It exists only to dedupe repeat loads and keep handles alive.
std::vector<LoadedModule>& loadedModules()
{
    static std::vector<LoadedModule> modules;
    return modules;
}

// Case-insensitive suffix test. Used only to avoid doubling an extension the operator already typed.
bool endsWithNoCase(const std::string& s, const char* suffix)
{
    const std::size_t n = std::strlen(suffix);
    if (s.size() < n)
        return false;
    for (std::size_t i = 0; i < n; ++i)
    {
        char a = s[s.size() - n + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}

// Resolves the console argument - a module base name, normally WITHOUT extension (e.g. "bf2hub") -
// to the platform's library file(s) to try, in order. The platform extension is appended here, so a
// single openbf2.con line (openbf2.addModDll bf2hub) works on every build: .dll on Windows, .so on
// ELF. A name that already carries a path separator is taken as given (an explicit operator path),
// and an extension the operator already typed is not doubled. On ELF both lib<name>.so and <name>.so
// are tried - the engine's lib-prefixed plugin convention and a bare .so. Case is preserved
// (filenames are case-sensitive on ELF, so the name must match the file there).
std::vector<std::string> resolveModuleCandidates(const std::string& name)
{
    std::vector<std::string> out;
#if defined(_WIN32)
    if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos)
    {
        out.push_back(name);                            // explicit path, used verbatim
        return out;
    }
    std::string file = name;
    if (!endsWithNoCase(file, ".dll"))
        file += ".dll";
    out.push_back(std::string(".\\") + file);           // from the current directory, not the system DLL search path
#else
    if (name.find('/') != std::string::npos)
    {
        out.push_back(name);
        return out;
    }
    std::string base = name;
    if (endsWithNoCase(base, ".so"))
        base.erase(base.size() - 3);
    else if (endsWithNoCase(base, ".dll"))
        base.erase(base.size() - 4);
    const std::string stem = (base.compare(0, 3, "lib") == 0) ? base.substr(3) : base;
    out.push_back(std::string("lib") + stem + ".so");
    if (base != std::string("lib") + stem)              // skip the duplicate when the name already had lib
        out.push_back(base + ".so");
#endif
    return out;
}

std::string lastOsError()
{
#if defined(_WIN32)
    char buf[32];
    std::sprintf(buf, "error %lu", static_cast<unsigned long>(::GetLastError()));
    return std::string(buf);
#else
    const char* msg = ::dlerror();
    return msg ? std::string(msg) : std::string("unknown error");
#endif
}

void* openModule(const std::string& path)
{
#if defined(_WIN32)
    return ::LoadLibraryA(path.c_str());
#else
    return ::dlopen(path.c_str(), RTLD_LAZY);
#endif
}

void closeModule(void* handle)
{
#if defined(_WIN32)
    ::FreeLibrary(static_cast<HMODULE>(handle));
#else
    ::dlclose(handle);
#endif
}

void* moduleSymbol(void* handle, const char* symbol)
{
#if defined(_WIN32)
    return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(handle), symbol));
#else
    return ::dlsym(handle, symbol);
#endif
}

// Loads the named module. Returns a human-readable status string (empty is never returned, so the
// outcome always reaches an operator over rcon or the server console). Also echoed to the server
// console so the result is visible when a script discards the return value.
std::string openBf2AddModDll(const std::string& name)
{
    std::string result;

    if (name.empty())
    {
        result = "openbf2.addModDll: no module name given";
    }
    else
    {
        std::vector<LoadedModule>& mods = loadedModules();
        bool already = false;
        for (std::vector<LoadedModule>::const_iterator it = mods.begin(); it != mods.end(); ++it)
        {
            if (it->requested == name)
            {
                already = true;
                break;
            }
        }

        if (already)
        {
            result = "openbf2.addModDll: '" + name + "' already loaded";
        }
        else
        {
            const std::vector<std::string> candidates = resolveModuleCandidates(name);
            void* handle = 0;
            std::string resolved;
            std::string tried;
            std::string lastErr;
            for (std::vector<std::string>::const_iterator c = candidates.begin();
                 c != candidates.end(); ++c)
            {
                if (!tried.empty())
                    tried += ", ";
                tried += *c;
                handle = openModule(*c);
                if (handle)
                {
                    resolved = *c;
                    break;
                }
                lastErr = lastOsError();
            }

            if (!handle)
            {
                result = "openbf2.addModDll: could not load '" + name + "' (tried " + tried
                         + "; last error: " + lastErr + ")";
            }
            else
            {
                InitDllFn initDll = reinterpret_cast<InitDllFn>(moduleSymbol(handle, "initDll"));
                if (initDll && !initDll(reinterpret_cast<void*>(classManager)))
                {
                    closeModule(handle);
                    result = "openbf2.addModDll: initDll refused '" + resolved + "'";
                }
                else
                {
                    LoadedModule entry;
                    entry.requested = name;
                    entry.resolved  = resolved;
                    entry.handle    = handle;
                    mods.push_back(entry);
                    result = std::string("openbf2.addModDll: loaded '") + resolved + "'"
                             + (initDll ? " (initDll ok)" : " (no initDll; DllMain only)");
                }
            }
        }
    }

    if (io::mainConsole)
        io::mainConsole->output(result);
    return result;
}

} // anonymous namespace

BF2_CONSOLE_OBJECT("openbf2", 1, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addModDll, openBf2AddModDll, std::string, true,
                                std::string, kOpenBf2ConsoleTag)

// Handed to the console registry by ModuleBF2::init (Core/ModuleRegistry.cpp).
ConsoleObjectList& openBf2ConsoleObjects()
{
    return consoleBindingList(kOpenBf2ConsoleTag);
}

} // namespace hfe
} // namespace dice

#endif // DEDICATED
