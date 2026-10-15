// [PLATFORM] Renderer implementation of the engine DLL lifecycle using the selected host ABI.

#include "PluginEntry.h"
#include "PluginModules.h"
#include "Frontier.h"

#include <cassert>

namespace dice {
namespace hfe {

// Renderer-local singleton pointers populated from the host at initDll.
// Compatibility pointers preserve the returned IBase face; concrete consumers must adjust at the boundary.
// Native mode uses typed interfaces where available.
IClassManager* classManager           = 0;

SettingsRepostitory* settings         = 0;   // 0x10244A90  "SettingsRepostitory"  (dice's typo)
ConsoleObjects* mainConsoleObjects    = 0;
IBase* mainConsole                    = 0;
::renddx9abi::HostFileManager* fileManager = 0;
IBase* fileChangeManager              = 0;
IBase* bfGlobal_callback              = 0;
IBase* bfGlobal_callbackData          = 0;
IBase* debugOn                        = 0;
IBase* debugModules                   = 0;
IBase* profilerClient                 = 0;
class System;
System* g_system                      = 0;   // 0x10244AC0, shared Core/System.h type
IBase* loadStat                       = 0;   // 0x10244AB8   not required
EventManager* eventManager            = 0;
IBase* frameEventManager              = 0;
IBase* objectManager                  = 0;
IBase* objectTemplateManager          = 0;
IBase* playerManager                  = 0;
IBase* game                           = 0;   // 0x10244AEC   not required
IBase* aiConnection                   = 0;   // 0x10244AF0   not required

// Dynamically constructs four heap-owned module objects and a null-terminated pointer table.
// Original translation-unit placement remains [UNRESOLVED].
IModule* dllmodules[] =
{
    new dx9rend::DX9RendModule(),
    new Rend::RendModule(),
    new geom::ModuleGeom(),
    new geom::ModuleGeomMisc(),
    0
};

// 0x48A6E0 in the ELF; MSVC inlines it into both exports here, twice into deinitDll. The
// function-local static is dice's - dice::hfe::getNoModules(void)::no is a real ELF symbol.
int getNoModules()
{
    static int no = 0;

    // The walk starts at 1, so the empty case needs its own guard - and a zero answer is never
    // cached, so an empty table re-walks every call. Reproduced, harmless.
    if (no == 0 && dllmodules[0] != 0)
    {
        int i = 1;
        int result;
        do
        {
            result = i;
        }
        while (dllmodules[i++] != 0);
        no = result;
    }

    return no;
}


extern "C" bool initDll(IClassManager* pClsMgr)
{
    // Ours, not dice's: open the instrument before anything can trip over. A replacement DLL that
    // dies without saying where is worth nothing, and this is the earliest point at which the log
    // can exist.
    renddx9abi::logOpen();
    renddx9abi::logf("initDll(classManager=0x%p) - replacement RendDX9",
                     static_cast<const void*>(pClsMgr));

    // Ours too, and for the same reason: BF2's own handler and the WER minidump both arrive after
    // the stack that explains the fault has been overwritten. See CrashReport.cpp. Report-only.
    renddx9abi::installCrashReporter();

    // Requires the host class-manager pointer before any registration or lookup.
    assert(pClsMgr);

    classManager = pClsMgr;

    // Fetches shared interfaces in their original order.
    settings              = reinterpret_cast<decltype(settings)>(
        classManager->getSingleton(renddx9abi::HostString("SettingsRepostitory")));
    mainConsoleObjects    = reinterpret_cast<ConsoleObjects*>(
        classManager->getSingleton(renddx9abi::HostString("MainConsoleObjects")));
    mainConsole           = classManager->getSingleton(renddx9abi::HostString("MainConsole"));
    fileManager           = static_cast<renddx9abi::HostFileManager*>(
        classManager->getSingleton(renddx9abi::HostString("FileManager")));
    fileChangeManager     = classManager->getSingleton(renddx9abi::HostString("FileChangeManager"));
    bfGlobal_callback     = classManager->getSingleton(renddx9abi::HostString("BfGlobal_callback"));
    bfGlobal_callbackData = classManager->getSingleton(renddx9abi::HostString("BfGlobal_callbackData"));
    debugOn               = classManager->getSingleton(renddx9abi::HostString("DebugOn"));
    debugModules          = classManager->getSingleton(renddx9abi::HostString("DebugModules"));
    profilerClient        = classManager->getSingleton(renddx9abi::HostString("Profiler"));
    g_system              = reinterpret_cast<System*>(classManager->getSingleton(renddx9abi::HostString("System")));
    loadStat              = classManager->getSingleton(renddx9abi::HostString("LoadStat"));
    eventManager          = reinterpret_cast<EventManager*>(
        classManager->getSingleton(renddx9abi::HostString("EventManager")));
    frameEventManager     = classManager->getSingleton(renddx9abi::HostString("FrameEventManager"));
    objectManager         = classManager->getSingleton(renddx9abi::HostString("ObjectManager"));
    objectTemplateManager = classManager->getSingleton(renddx9abi::HostString("ObjectTemplateManager"));
    playerManager         = classManager->getSingleton(renddx9abi::HostString("PlayerManager"));
    game                  = classManager->getSingleton(renddx9abi::HostString("Game"));
    aiConnection          = classManager->getSingleton(renddx9abi::HostString("AIConnection"));

    // The sixteen that are required
    // LoadStat, Game and AIConnection are fetched above and deliberately absent here: a module loads
    // before a game exists, so requiring "Game" would make every module fail. The chain's own order
    // is the binary's - eventManager and frameEventManager are tested last, after the three world
    // managers, even though they were fetched before them.
    const bool ok =
           settings              != 0
        && mainConsoleObjects    != 0
        && mainConsole           != 0
        && fileManager           != 0
        && fileChangeManager     != 0
        && bfGlobal_callback     != 0
        && bfGlobal_callbackData != 0
        && debugOn               != 0
        && debugModules          != 0
        && profilerClient        != 0
        && g_system              != 0
        && objectManager         != 0
        && objectTemplateManager != 0
        && playerManager         != 0
        && eventManager          != 0
        && frameEventManager     != 0;

    renddx9abi::logf("initDll: 19 singletons pulled, required-16 chain = %s", ok ? "true" : "false");
    if (!ok)
    {
        // Naming the missing ones costs nothing and turns a bare "false" into a diagnosis.
        static const char* const names[16] =
        {
            "SettingsRepostitory", "MainConsoleObjects", "MainConsole", "FileManager",
            "FileChangeManager", "BfGlobal_callback", "BfGlobal_callbackData", "DebugOn",
            "DebugModules", "Profiler", "System", "ObjectManager",
            "ObjectTemplateManager", "PlayerManager", "EventManager", "FrameEventManager"
        };
        IBase* const values[16] =
        {
            reinterpret_cast<IBase*>(settings), reinterpret_cast<IBase*>(mainConsoleObjects), mainConsole, fileManager,
            fileChangeManager, bfGlobal_callback, bfGlobal_callbackData, debugOn,
            debugModules, profilerClient, reinterpret_cast<IBase*>(g_system), objectManager,
            objectTemplateManager, playerManager, reinterpret_cast<IBase*>(eventManager),
            frameEventManager
        };
        for (int i = 0; i < 16; ++i)
            if (!values[i])
                renddx9abi::logf("initDll:   MISSING required singleton \"%s\"", names[i]);
    }

    // Pass one: every module joins the host's registry
    // two separate passes, not one loop. Every module is registered before any module is
    // initialised, so a module's init() can rely on finding its siblings already in the registry.
    // Fusing them would compile, run, and break exactly that guarantee.
    for (int i = 0; dllmodules[i] != 0; ++i)
        classManager->registerServer(dllmodules[i]);

    // Starts modules only after registration. Discards init results, leaving the previously
    // computed host result unchanged even if a module reports failure.
    for (int i = 0; dllmodules[i] != 0; ++i)
        dllmodules[i]->init();

    renddx9abi::logf("initDll: returning %s", ok ? "true" : "false");
    return ok;
}


extern "C" void deinitDll()
{
    renddx9abi::logf("deinitDll: %d modules", getNoModules());

    // Reverse order, two passes again - the mirror of initDll. Every module is closed before any is
    // unregistered, so close() can still reach its siblings through the registry.
    for (int i = getNoModules() - 1; i >= 0; --i)
        dllmodules[i]->close();

    for (int i = getNoModules() - 1; i >= 0; --i)
    {
        classManager->unregisterServer(dllmodules[i]);
        dllmodules[i]->release();
        // the slot is nulled, which silently poisons getNoModules(). The count was cached on the
        // first call above so this loop completes, but any call after this point walks a table
        // whose first entry is now 0. Nothing calls it after unload, so the defect never fires.
        // reproduced.
        dllmodules[i] = 0;
    }

    // neither classManager NOR the nineteen globals are cleared. Every one keeps pointing into
    // the host after teardown - harmless only because the image is about to be unmapped.
    renddx9abi::logf("deinitDll: done");
    renddx9abi::logClose();
}

} // namespace hfe
} // namespace dice
