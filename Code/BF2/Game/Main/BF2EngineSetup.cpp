// Path: [INFERRED]; see BF2EngineSetup.h. Contents recovered from the symbolized ELF.
// Registers engine modules and manages their startup/shutdown order.

#include "BF2EngineSetup.h"
#include "Game.h"     // isDedicated()
#include "../../Core/PluginDll.h"
#include "BF2Engine.h"
#include "../../Core/IModule.h"             // [INFERRED] path
#include "../../IO/Network/SocketManager.h"   // the real setSocketManager - replaces the local shim
#include "../../IO/Network/NetworkManager.h"  // the real NetworkManager
#include "../../Core/Modules.h"             // the 25 module classes - see Core/ModuleRegistry.cpp
#include "../../Core/Module.h"              // dice::hfe::Module - the concrete one, module #1
#include "../../Core/ClassManager.h"
#include "../../IO/File/FileManager.h"
#include "../../Core/Ids.h"                 // CID_MemoryPool, IID_IMemoryPool, CID_SpawnManager
#if !DEDICATED
#include "BF2ClientSeams.h"                 // BF2Render: recovered client-side virtual interface
#include "../../Menu/ClientConsoleModules.h"
#endif

namespace dice {
namespace hfe {

// Manager creation helpers without a shared declaration header.
namespace world {
void setMaterialManager(unsigned int classId);
void setScoreManager(unsigned int classId);
}
void setSpawnManager(unsigned int classId);

namespace io {

// These filesystem/archive IDs belong to namespace io; namespace is part of
// their symbol identity.
extern const unsigned int IID_IFileSystem;          // 1343623678u
extern const unsigned int IID_IArchiveHandler;      // 2760103932u
extern const unsigned int CID_UnixFileSystem;       // 3230030076u
extern const unsigned int CID_ZipArchiveHandler;    // 4289832698u

class IFileSystem;
class IArchiveHandler;


} // namespace io


} // namespace hfe
} // namespace dice

#include <vector>

#include "../../Core/SmartItf.h"

namespace dice {
namespace hfe {

// The module list is a file-scope std::vector<IModule*> in the binary (ELF .bss 0x8AACBFC..0x8AACC04
// is the standard begin/end/capacity triple), not a member of BF2EngineSetup - which is why the body
// of initModules() never dereferences `this`. That does NOT make it static: its call site in
// BF2Engine::init pushes a receiver (0x8131451/0x8131457). See the note in BF2EngineSetup.h.
static std::vector<IModule*> s_modules;         // [INFERRED] name; ELF .bss 0x8AACBFC

// Null-terminated late-module array, distinct from the normal module vector.
// [INFERRED] Name and capacity. [UNRESOLVED] Which registration path populates it.
static IModule* s_lateModules[4];
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
// [CHOSEN, not recovered] The editor's HUD (its play test and HUD editor, App/EngineHost.cpp): set before initModules.
static bool s_editorHud = false;
void setEditorHudModules(bool on) { s_editorHud = on; }
#endif               // [INFERRED] name and bound; ELF .bss 0x8AACC08

extern ClassManager* classManager;              //  = _ZN4dice3hfe12classManagerE

// [INFERRED] aiInterfaceModule names the late-module array's second slot in the ELF.
// This build stores it separately; initialization and shutdown route through the AI path.
// The late-module loop stops at the null first slot and never processes the AI entry.
extern ai::AIInterfaceModule* aiInterfaceModule;//  == s_lateModules[1]

BF2EngineSetup::BF2EngineSetup(BF2Engine* engine)
    : m_engine(engine)
    , m_swiffPlayerDll(0)
    , m_unknown8(0)
    , m_aiInitialised(false)
{
}

BF2EngineSetup::~BF2EngineSetup()
{
}

// Registration order IS significant - reproduced exactly as emitted at 0x813b120.
// Every module is heap-allocated with a refcount of 1 in its second word.
bool BF2EngineSetup::initModules()
{
    s_modules.push_back(new Module());                                  //  1  8 bytes
    s_modules.push_back(new ModuleBF2());                               //  2  8
    s_modules.push_back(new ModuleGame());                              //  3 16  (extra ptr at +4)
    s_modules.push_back(new ModuleGameLogic());                         //  4  8
    s_modules.push_back(new MathModule());                              //  5  8
    s_modules.push_back(new io::ModuleIO());                            //  6  8
    s_modules.push_back(new io::FileModule());                          //  7  8
#if !DEDICATED
    if (!isDedicated())                                                 // BF2.exe 0x4472F0: the 8th module
    {
        s_modules.push_back(new io::InputDX8Module());                  // PE4475EA..44761A, table89AE6C
    }
#endif
    s_modules.push_back(new world::Module());                           //  8  8
    s_modules.push_back(new world::ObjectModule());                     //  9  8
    s_modules.push_back(new world::ObjTemplPortalModule());             // 10  8
    s_modules.push_back(new ModuleNetworkables());                      // 11  8
    s_modules.push_back(new world::ObjTemplBaseModule());               // 12  8
    s_modules.push_back(new tool::ModuleToolObject());                  // 13  8
    s_modules.push_back(new world::PhysicsModule());                    // 14  8
    s_modules.push_back(new animationTools::AnimModule());              // 15 12  (extra ptr at +4)
    s_modules.push_back(new io::NetworkModule());                       // 16  8
    s_modules.push_back(new BfSoundModule());                           // 17  8
    s_modules.push_back(new world::BFMaterialSystemModule());           // 18  8
    s_modules.push_back(new world::ScoreSystemModule());                // 19  8
    s_modules.push_back(new world::GameLogicObjTemplBaseModule());      // 20  8
    s_modules.push_back(new world::GameLogicObjTemplFxModule());        // 21  8
    s_modules.push_back(new world::GameLogicObjTemplVehicleModule());   // 22  8
    s_modules.push_back(new world::GameLogicObjTemplBFModule());        // 23  8
    s_modules.push_back(new world::GameLogicCompModule());              // 24  8
    s_modules.push_back(new SpawnSystemModule());                       // 25  8
#if !DEDICATED && !(defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR)
    if (!isDedicated())                                                 // PE447D2C..447DBB
    {
        s_modules.push_back(new world::HudCompModule());                // table89B0D0
        s_modules.push_back(new hud::HudConsoleModule());               // table89B0F0
        s_modules.push_back(new hud::ProfileConsoleModule());           // table89B110
    }
#endif
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] The editor's HUD, when it asked for it (setEditorHudModules): the HUD's components (the
    // templates' WeaponHud and VehicleHud) and its console module, as the client registers them; never the profiles.
    if (s_editorHud)
    {
        s_modules.push_back(new world::HudCompModule());
        s_modules.push_back(new hud::HudConsoleModule());
    }
#endif
    // [CHOSEN, not recovered] The editor omits client front-end modules, HUD (unless asked), and
    // profiles. Register the remaining modules' class servers for lazy construction.
    for (size_t i = 0; i < s_modules.size(); ++i)
        classManager->registerServer(s_modules[i]);

    classManager->setDefaultClass(IID_IMemoryPool, CID_MemoryPool);
#if defined(BF2_IMAGE_EDITOR) && BF2_IMAGE_EDITOR
    // [CHOSEN, not recovered] Editor images select HeightmapClusterEditable before
    // singleton initialization; its tiles use HeightmapEditable. Game images omit this override.
    classManager->setSingletonClass("HeightmapCluster", world::CID_HeightmapClusterEditable);
#endif
#if !DEDICATED
    if (!isDedicated())                                                 // BF2.exe 0x4472F0, before initSingletons
    {
        classManager->setSingletonClass("InputDeviceManager", 120508928u);   // vptr+0x3C; 0x072ED200 - [UNRESOLVED] the
        classManager->setSingletonClass("InputManager", 3139421003u);        //   two ids (0xBB1FC34B): not 121xxx CIDs
    }
#endif
    classManager->initSingletons();

    // Module init publishes console bindings and related conversions/constants.
    // A failed module is destroyed and unregistered, but this routine still returns true.
    for (size_t i = 0; i < s_modules.size(); ++i)
    {
        if (!s_modules[i]->init())
        {
            s_modules[i]->close();      //  WAS destroy() - see the note below
            classManager->unregisterServer(s_modules[i]);
        }
    }

    setSpawnManager(CID_SpawnManager);
    world::setMaterialManager(0xC4AF);          // [UNRESOLVED] CID constant, symbol not recovered
    world::setScoreManager(0xC4B9);             // [UNRESOLVED] CID constant, symbol not recovered
    return true;
}

bool BF2EngineSetup::initFileSystem()
{
    SmartItf<io::IFileSystem,     &io::IID_IFileSystem>     fileSystem;
    SmartItf<io::IArchiveHandler, &io::IID_IArchiveHandler> archiveHandler;

    // [PLATFORM] Select the filesystem factory supplied by the target image.
#if defined(_WIN32)
    if (fileSystem.create(io::CID_Win32FileSystem))
#else
    if (fileSystem.create(io::CID_UnixFileSystem))
#endif
        io::fileManager->setBaseSystem(fileSystem);

    if (archiveHandler.create(io::CID_ZipArchiveHandler))
        io::fileManager->addArchiveHandler(archiveHandler);

    return true;                                // SmartItf destructors release both
}

bool BF2EngineSetup::initNetwork()
{
    io::NetworkManager* nm = new io::NetworkManager();   // operator new(0x84) == 132 bytes
    io::networkManager = nm;                             // [INFERRED] name; ELF 0x8AC3D1C
    if (!nm)
        return false;
    return io::setSocketManager() != 0;
}

bool BF2EngineSetup::initAI()
{
    if (!aiInterfaceModule)
    {
        aiInterfaceModule = new ai::AIInterfaceModule();  // operator new(0x10) == 16 bytes
        classManager->registerServer(aiInterfaceModule);
    }
    if (!aiInterfaceModule)
        return false;

    if (aiInterfaceModule->init())
    {
        m_aiInitialised = true;
        return true;
    }

    aiInterfaceModule->close();
    classManager->unregisterServer(aiInterfaceModule);
    aiInterfaceModule->release();
    aiInterfaceModule = 0;
    m_aiInitialised = false;
    return false;
}

// Resets the AI module and clears the initialized flag. Leaves the module alive
// and registered for the next load rather than reversing initAI ownership.
void BF2EngineSetup::shutdownAI()
{
    if (aiInterfaceModule)
        aiInterfaceModule->reset();             // vcall +0x20

    m_aiInitialised = false;                    // offset 12
}

// [PLATFORM] Client-only. Stubs in the dedicated server build.
//  (10 bytes: `return true`) / BF2.exe 0x446CB0: `if (isDedicated()) return true;` then
// LoadLibraryBf("SwiffPlayer.dll") - LoadLibraryA + GetProcAddress("initDll") + the call - into +4.
bool BF2EngineSetup::initSwiff()
{
#if !DEDICATED
    if (isDedicated())
        return true;
    m_swiffPlayerDll = LoadLibraryBf("SwiffPlayer.dll");
    return m_swiffPlayerDll != 0;
#else
    return true;
#endif
}
void BF2EngineSetup::shutdownSwiff()
{
#if !DEDICATED
    // BF2.exe 0x446CE0.  The dedicated ELF's five-byte body remains the empty branch below.
    if (m_swiffPlayerDll)
    {
        FreeLibraryBf(m_swiffPlayerDll, "no name");
        m_swiffPlayerDll = 0;
    }
#endif
}

// Initializes a separate null-terminated module array. Failed modules are destroyed,
// unregistered, released, and nulled. [UNRESOLVED] The array's population mechanism.
bool BF2EngineSetup::initLateModules()
{
    for (int i = 0; s_lateModules[i]; ++i)
        classManager->registerServer(s_lateModules[i]);

    for (int i = 0; s_lateModules[i]; ++i)
    {
        if (!s_lateModules[i]->init())                  // IModule vtable +0x18
        {
            s_lateModules[i]->close();                  //               +0x1C
            classManager->unregisterServer(s_lateModules[i]);
            s_lateModules[i]->release();                //               +0x08
            s_lateModules[i] = 0;
        }
    }
    return true;
}

// . The console needs no setup here - io::mainConsole and the module
// console-object registration (see initModules) have already done the work.
bool BF2EngineSetup::initConsole()
{
    return true;
}

// Initializes the engine's renderer, returning false when absent. Dedicated images
// do not construct one and callers tolerate that result.
bool BF2EngineSetup::initRenderer()
{
    BF2Render* renderer = m_engine->bf2Render;          // BF2Engine +0x18
    if (!renderer)
        return false;
#if !DEDICATED
    return renderer->init();                            // PE vslot +0x04; ELF vslot +0x08
#else
    // The dedicated image never constructs BF2Render; the non-null arm is unreachable there.
    return false;
#endif
}

// Shuts down sockets, then deletes the network manager. Socket shutdown's result is discarded.
void BF2EngineSetup::shutdownNetwork()
{
    io::shutdownSocketManager();

    if (io::networkManager)
    {
        delete io::networkManager;              // +0x08
        io::networkManager = 0;
    }
}

// shutdownModules - ELF 0x813BC40. Teardown order is network, log, AI, late modules, then modules.
// Walk each module list backwards; close and release each module before unregistering its server.
// The AI teardown here is broader than shutdownAI and must not be replaced by that helper.
bool BF2EngineSetup::shutdownModules()
{
    shutdownNetwork();
    m_engine->closeLog();               // BF2EngineSetup+0 -> BF2Engine::closeLog, 0x813BC58

    // The AI module - s_lateModules[1] in the ELF, see the note on aiInterfaceModule above. Nulled
    // here, which is what stops the reverse walk below from tearing it down a second time.
    if (aiInterfaceModule)
    {
        aiInterfaceModule->close();                     // IModule vtable +0x1C
        aiInterfaceModule->release();                   //               +0x08
        classManager->unregisterServer(aiInterfaceModule);
        aiInterfaceModule = 0;
        m_aiInitialised = false;
    }

    int count = 0;
    while (s_lateModules[count])
        ++count;

    for (int i = count - 1; i >= 0; --i)
    {
        if (!s_lateModules[i])
            continue;

        s_lateModules[i]->close();
        s_lateModules[i]->release();
        classManager->unregisterServer(s_lateModules[i]);
        s_lateModules[i] = 0;
    }

    for (int i = (int)s_modules.size() - 1; i >= 0; --i)
    {
        if (!s_modules[i])
            continue;

        s_modules[i]->close();
        s_modules[i]->release();
        classManager->unregisterServer(s_modules[i]);
        s_modules[i] = 0;
    }

    // Emitted as erase(begin(), end()) - which is exactly what libstdc++'s vector::clear() is.
    // Note it clears the vector but does NOT free its buffer, and the slots were already nulled
    // one by one above; both facts are preserved.
    s_modules.clear();

    return true;
}

// shutdownAI resets the registered module; shutdownModules owns its final destruction.
} // namespace hfe
} // namespace dice
