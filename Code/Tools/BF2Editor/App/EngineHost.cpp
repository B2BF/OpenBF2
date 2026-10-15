// Code/Tools/BF2Editor/App/EngineHost.cpp - see EngineHost.h. Editor-only code (D3).

// Every engine call below is the tree's own shared code, called as BF2Engine::init and initEngine
// call it (Game/Main/BF2Engine.cpp); the comments identify the mirrored and omitted steps.
#include "Framework/EditorConfig.h"
#include "App/EngineHost.h"

#include "Framework/EditorLog.h"

#include "Game/Main/BF2Engine.h"
#include "Game/Main/BF2EngineSetup.h"
#include "Game/Main/BF2Log.h"
#include "Game/Main/BF2Window.h"
#include "Game/Main/BF2ClientSeams.h"
#include "RendMisc/IRenderer.h"
#include "Game/IModInfo.h"
#include "Game/SingletonServer.h"
#include "Game/Utility/Vars.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/Game/SupplyDropManager.h"
#include "Game/Objects/Fx/PersistentExplosionEffectManager.h"
#include "Menu/BF2HudManager.h"
#include "Core/System.h"
#include "Core/SmartItf.h"
#include "Core/ClassManager.h"
#include "Core/SettingsRepostitory.h"
#include "Core/DebugDraw.h"
#include "Core/StringUtil.h"
#include "IO/Console/Console.h"
#include "IO/File/FileManager.h"
#include "IO/Stream/IStream.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include <windows.h>
#include <d3d9.h>

#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace dice
{
namespace hfe
{
// The interface-scope static, declared exactly as Game/Main/BF2Engine.cpp and
// Core/SettingsRepostitory.cpp declare it (Core/SettingsRepostitory.h explains why the interface has
// no header yet). _ZN4dice3hfe20ISettingsRepostitory15createSingletonEv.
class ISettingsRepostitory { public: static void createSingleton(); };
} // namespace hfe
} // namespace dice

namespace dice { namespace hfe {
// Game/Main/BF2EngineSetup.cpp, editor flavour: the HUD's modules in the module list.
void setEditorHudModules(bool on);
extern hud::BF2HudManager* hudManager;          // Game/Main/BF2Engine.cpp
} }

namespace bf2editor
{

// Befriended by dice::hfe::BF2Engine (Game/Main/BF2Engine.h): seats the members BF2Engine::init
// would have seated, so shutdownEngine() and the shared code reading them work unchanged. Nothing
// else in the editor touches BF2Engine's private members.
class EngineAccess
{
public:
    static void setLog(dice::hfe::BF2Engine& engine, dice::hfe::BF2Log* log) { engine.bf2Log = log; }
    static dice::hfe::BF2Log* log(const dice::hfe::BF2Engine& engine) { return engine.bf2Log; }
    static void setSetup(dice::hfe::BF2Engine& engine, dice::hfe::BF2EngineSetup* setup)
    {
        engine.bf2EngineSetup = setup;
    }
    static dice::hfe::BF2EngineSetup* setup(const dice::hfe::BF2Engine& engine) { return engine.bf2EngineSetup; }
    static void setWindow(dice::hfe::BF2Engine& engine, dice::hfe::BF2Window* window) { engine.window = window; }
    static dice::hfe::BF2Window* window(const dice::hfe::BF2Engine& engine) { return engine.window; }
    static void setRender(dice::hfe::BF2Engine& engine, dice::hfe::BF2Render* render) { engine.bf2Render = render; }
    static dice::hfe::BF2Render* render(const dice::hfe::BF2Engine& engine) { return engine.bf2Render; }
};

namespace
{

const std::string kNoArg;

// The retail editor's renderer switches, mapped onto the settings the 1.50 RendDX9 reads
// (RendDX9Init.cpp, BF2Render.cpp, VideoOptions.cpp, TextureManager.cpp). 1.50 has no reader for two
// of them; those are reported, not silently dropped.
struct RendererSetting
{
    const char* option;
    const char* setting;    // 0: nothing in 1.50 reads it
    char        type;       // 'i' int, 'b' bool
};
const RendererSetting kRendererSettings[] = {
    { "deviceType",      "GSDeviceType",      'i' },
    { "deviceBehavior",  "GSDeviceBehavior",  'i' },
    { "rp",              "GSRenderPath",      'i' },
    { "skipMips",        "GSSkipMipLevels",   'i' },
    { "managedTextures", "GSManagedTextures", 'b' },
    { "mergeMaterials",  0,                   0   },
    { "videoOptions",    0,                   0   },
};

// The device RendDX9 created. There is no typed getter on IRenderer: the rebuilt Renderer
// (RendDX9/BF2Render.cpp, `class Renderer : public IRenderer`, single inheritance) is overlaid by its
// RendDX9Base layout from offset 0 (RendDX9Init.cpp constructD3DHalf: g_rendererBase = this), and
// RendDX9Base.h static_asserts its head {vptr, int refCount, IDirect3D9*, IDirect3DDevice9*}: `device`
// at +0x0C on x86, +0x18 on x64. The one read of the renderer's layout in the editor, mirrored by that
// head; EngineHost::startRenderer verifies it against the device's own creation parameters.
IDirect3DDevice9* rendererDevice(dice::hfe::dx9rend::IRenderer* renderer)
{
    struct RendererHead
    {
        void*             vptr;
        int               refCount;
        void*             d3d9;
        IDirect3DDevice9* device;
    };
    return reinterpret_cast<const RendererHead*>(renderer)->device;
}

// io::OldConsole::runFullAccess with no script arguments, as every BF2Engine call site passes them.
bool runFullAccess(const std::string& script)
{
    return dice::hfe::io::mainConsole->runFullAccess(script, kNoArg, kNoArg, kNoArg, kNoArg, kNoArg, kNoArg,
                                                     kNoArg, kNoArg, dice::hfe::io::Console::ignoredString_);
}

// An archive script run line by line with full access, the archives it mounts at Objects left out: the
// object editor's loose mode (+looseObjects 1), where templates are the mod's loose files. mountArchive
// finds a zip along the file manager's search paths, not the script's folder, so a line run alone mounts
// what the script would.
void runArchivesWithout(const std::string& script, bool objects, bool common)
{
    using namespace dice::hfe;
    std::string text;
    io::IStream* stream = io::fileManager->open(script, io::FILEMODE_READ, static_cast<io::FileAccess>(0), 0);
    if (!stream)
    {
        logLine(LogLevel::Warning, "engine: %s not found", script.c_str());
        return;
    }
    char buffer[4096];
    for (unsigned int got = 0; (got = stream->read(buffer, sizeof(buffer))) != 0;)
        text.append(buffer, got);
    stream->release();
    std::istringstream lines(text);
    std::string line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        std::istringstream words(line);
        std::string command, zip, point;
        if (!(words >> command))
            continue;
        if (_stricmp(command.c_str(), "fileManager.mountArchive") == 0 && (words >> zip >> point)
            && ((objects && _stricmp(point.c_str(), "Objects") == 0) || (common && _stricmp(point.c_str(), "Common") == 0)))
        {
            logLine(LogLevel::Info, "engine: not mounted (+loose%s 1): %s at %s", objects && _stricmp(point.c_str(), "Objects") == 0
                    ? "Objects" : "Common", zip.c_str(), point.c_str());
            continue;
        }
        std::string answer;
        io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
    }
}

} // namespace

EngineHost::EngineHost()
    : m_phase(PhaseDown)
    , m_engine(0)
    , m_window(0)
    , m_renderer(0)
    , m_device(0)
{
}

EngineHost::~EngineHost()
{
    shutdown();
}

bool EngineHost::fail(const std::string& step)
{
    logLine(LogLevel::Error, "engine: %s failed; shutting down what came up", step.c_str());
    shutdown();
    return false;
}

bool EngineHost::boot(const EngineConfig& config)
{
    using namespace dice::hfe;

    if (m_phase != PhaseDown)
    {
        logLine(LogLevel::Error, "engine: boot() called while the engine is already up");
        return false;
    }
    if (config.mod.empty() || config.mod.find_first_of("/\\:.") != std::string::npos)
    {
        logLine(LogLevel::Error, "engine: '%s' is not a mod folder name (expected e.g. bf2)", config.mod.c_str());
        return false;
    }
    const std::string modPath = "mods/" + config.mod;

    // The game layers a missing mod folder over mods/bf2 without a word; the editor would then edit
    // bf2 content while claiming another mod. Refuse instead: the working directory must be a BF2
    // root holding the mod.
    const std::string modDesc = modPath + "/mod.desc";
    const DWORD attributes = GetFileAttributesA(modDesc.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    {
        logLine(LogLevel::Error, "engine: %s not found; run the editor from a BF2 root (the folder that holds mods/)",
                modDesc.c_str());
        return false;
    }

    // WinMain: the System singleton first. BF2Engine's constructor then creates the ClassManager,
    // publishes the bf2Engine global and builds the tick calculator and demo objects.
    logLine(LogLevel::Info, "engine: System, BF2Engine");
    g_system = new System();
    m_engine = new BF2Engine(0);            // no BF2 owner: the editor is the host
    m_phase = PhaseEngine;

    // BF2Engine::init, in its order
    EngineAccess::setLog(*m_engine, new BF2Log());
    ISettingsRepostitory::createSingleton();
    m_engine->initDefaultSettings();

    // The engine's own parameter parser seeds the game's defaults. The editor hands it only the mod
    // path and no-sound (sound is off until the editor stages its own BF2Audio.dll: a working
    // directory holding the retail one would otherwise supply it). No +loadLevel: the level is the
    // editor's to open, not the game's to start.
    const std::string engineCommandLine = "+modPath " + modPath + " +noSound 1";
    logLine(LogLevel::Info, "engine: parameters '%s'", engineCommandLine.c_str());
    if (!m_engine->parseParameters(engineCommandLine))
        return fail("BF2Engine::parseParameters");

    // The renderer switches 1.50's parser does not have: write the settings RendDX9 reads. The
    // editor's own default device is non-pure (64) - see EngineConfig::rendererOptions.
    bool deviceBehaviorGiven = false;
    bool skipMipsGiven = false;
    for (size_t i = 0; i < config.rendererOptions.size(); ++i)
    {
        const std::string& name = config.rendererOptions[i].first;
        const std::string& value = config.rendererOptions[i].second;
        skipMipsGiven = skipMipsGiven || name == "skipMips";
        const RendererSetting* entry = 0;
        for (size_t j = 0; j < sizeof(kRendererSettings) / sizeof(kRendererSettings[0]); ++j)
        {
            if (name == kRendererSettings[j].option)
                entry = &kRendererSettings[j];
        }
        if (!entry || !entry->setting)
        {
            logLine(LogLevel::Warning, "engine: +%s %s has no effect: nothing in 1.50 reads it", name.c_str(),
                    value.c_str());
            continue;
        }
        if (entry->type == 'b')
            settings->boolSet(entry->setting, value == "1");
        else
            settings->intSet(entry->setting, std::atoi(value.c_str()));
        logLine(LogLevel::Info, "engine: %s = %s (+%s)", entry->setting, value.c_str(), name.c_str());
        if (name == "deviceBehavior")
        {
            deviceBehaviorGiven = true;
            if (std::atoi(value.c_str()) & D3DCREATE_PUREDEVICE)
                logLine(LogLevel::Warning, "engine: +deviceBehavior %s asks for a PURE device; the editor UI "
                        "reads device state back and will not restore it correctly", value.c_str());
        }
    }
    if (!deviceBehaviorGiven)
        settings->intSet("GSDeviceBehavior", D3DCREATE_HARDWARE_VERTEXPROCESSING);   // 64
    // [CHOSEN] Textures load at full resolution: the editor saves some back (the terrain's colormaps), and the game's
    // derived default (GSSkipMipLevels -1, medium quality) drops their top level.
    if (!skipMipsGiven)
        settings->intSet("GSSkipMipLevels", 0);

    // Not created: the instance guard check (the editor's BF2Engine.cpp takes no instance mutex),
    // and bf2Console - the game's drop-down console; the editor has its own console panel.
    EngineAccess::setSetup(*m_engine, new BF2EngineSetup(m_engine));
    BF2EngineSetup* const setup = EngineAccess::setup(*m_engine);

    // BF2.exe sets 0 when no debugger is attached, which makes every Assert fatal (BF2Log::
    // displayErrorWindow). Batch runs ignore asserts, as the retail editor's *AndClose switches did.
    Vars::setValue("hack-ignore-asserts", config.ignoreAsserts ? "1" : "0");

    logLine(LogLevel::Info, "engine: modules");
    // The HUD's modules join the list when the HUD is asked for (Game/Main/BF2EngineSetup.cpp, editor flavour).
    setEditorHudModules(config.hud && config.window);
    if (!setup->initModules())
        return fail("BF2EngineSetup::initModules");
    initSettingsRepostitory();
    m_phase = PhaseModules;

    // Not registered: the engine as input filter and event handler (categories 5, 10, 11) and the
    // demo's HUD handler - they drive the game's flow, which the editor does not run.

    // The window and the renderer bridge, where BF2Engine::init creates them (its client arm). The
    // BF2Window is handed the editor's hwnd (setHwnd, +0x40), so BF2Render::init finds a created
    // window and draws into it instead of creating the game's own.
    if (config.window)
    {
        m_window = config.window;
        BF2Window* const window = new BF2Window();
        window->setHwnd(m_window);
        EngineAccess::setWindow(*m_engine, window);
        EngineAccess::setRender(*m_engine, new BF2Render(m_engine));   // publishes g_bf2Render
    }

    if (!setup->initLateModules())
        return fail("BF2EngineSetup::initLateModules");

    // BF2Engine::initEngine, the editor's part
    BF2Log* const log = EngineAccess::log(*m_engine);
    if (!log->initDebugCallback())
        return fail("BF2Log::initDebugCallback");
    io::mainConsole->setUseRelativePaths(true);

    logLine(LogLevel::Info, "engine: file system, mod paths");
    if (!setup->initFileSystem())
        return fail("BF2EngineSetup::initFileSystem");
    const std::string modDirectory = settings->stringGet("GSModDirectory", "");
    // The retail editor's own content, searched after the mod (addPath puts the newest path first and writes go to
    // it, so these are added before the mod's): the editable terrain loads `Shaders/TerrainEditorShader` and
    // `BF2Editor/Content/Terrain/Grid` (Geom/TerrainEditable.cpp), which live in the editor's install folder
    // (bf2editor/), never in the mod. [CHOSEN] read where the retail editor keeps them, not copied into the mod; the
    // game root as "./" (an empty entry would read from the drive's root).
    io::fileManager->addPath("./");
    io::fileManager->addPath("bf2editor");
    if (compareIgnoreCase(modDirectory.c_str(), "mods/bf2"))      // a mod layers over bf2, as in the game
        io::fileManager->addPath("mods/bf2");
    io::fileManager->addPath(modDirectory);
    log->initLogFiles("");

    if (config.useArchives && (config.looseObjects || config.looseCommon))
    {
        logLine(LogLevel::Info, "engine: archives (ServerArchives.con, ClientArchives.con) but those at %s: read from the "
                                "mod's loose folders instead", config.looseObjects && config.looseCommon ? "Objects and Common"
                                : config.looseObjects ? "Objects (+looseObjects 1)" : "Common (+looseCommon 1)");
        io::fileManager->permitMountArchives(true);
        runArchivesWithout("ServerArchives.con", config.looseObjects, config.looseCommon);
        runArchivesWithout("ClientArchives.con", config.looseObjects, config.looseCommon);
        io::fileManager->permitMountArchives(false);
    }
    else if (config.useArchives)
    {
        logLine(LogLevel::Info, "engine: archives (ServerArchives.con, ClientArchives.con)");
        io::fileManager->permitMountArchives(true);
        runFullAccess("ServerArchives.con");
        runFullAccess("ClientArchives.con");
        // The lightmap samples, as the retail editor's setup mounted them: bf2editor/LightmapSamples*.zip at Objects -
        // the meshes' sample grids an object lightmap is traced from (Meshes/<mesh>.samples), which no game archive
        // holds. Not with loose objects: an archive at Objects/ hides the loose folder from the file manager's listings,
        // and the samples are then read from the mod's loose Objects folder like the rest.
        WIN32_FIND_DATAA found;
        const HANDLE search = FindFirstFileA("bf2editor/LightmapSamples*.zip", &found);
        int samples = 0;
        if (search != INVALID_HANDLE_VALUE)
        {
            do
            {
                if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    && io::fileManager->mountArchive(std::string("bf2editor/") + found.cFileName, "Objects", false))
                    ++samples;
            } while (FindNextFileA(search, &found));
            FindClose(search);
        }
        if (samples)
            logLine(LogLevel::Info, "engine: %d lightmap sample archive%s (bf2editor/LightmapSamples*.zip) at Objects",
                    samples, samples == 1 ? "" : "s");
        io::fileManager->permitMountArchives(false);
    }
    else
    {
        logLine(LogLevel::Info, "engine: archives not mounted (+useArchives 0): content is read from the mod's folders");
    }

    if (!m_engine->initLocalization())
        return fail("BF2Engine::initLocalization");
    // Not loaded: Settings/Usersettings.con - the game's user settings are not the editor's.

    IModInfo* const modInfo = static_cast<IModInfo*>(classManager->getSingleton("ModInfo"));
    if (!modInfo)
        return fail("the ModInfo singleton");
    modInfo->init();                                        // result discarded, as in initEngine

    // The HUD manager, as BF2Engine::init makes it for a client: published as the HudManager singleton before the
    // renderer starts, which takes its paint hook from that singleton.
    if (config.hud && config.window)
    {
        hudManager = new hud::BF2HudManager();
        SingletonRegInfo info = { std::string("HudManager"), reinterpret_cast<IBase**>(&hudManager), 0, 0, 4 };
        classManager->registerSingleton(info);
    }
    if (config.window && !startRenderer(config))
        return fail("the renderer");
    // Its widgets and Menu/HUD/HudSetup/HudSetupMain.con, where initEngine runs BF2HudManager::init (after the explosion
    // manager the HUD scripts' variables need, as the client constructs it first).
    if (hudManager)
    {
        if (!world::explosionEffectManager)
            world::explosionEffectManager = new world::CPersistentExplosionEffectManager();
        m_hudUp = hudManager->init();
        logLine(m_hudUp ? LogLevel::Info : LogLevel::Warning, "engine: the HUD %s",
                m_hudUp ? "is up (Menu/HUD/HudSetup/HudSetupMain.con)" : "did not start (BF2HudManager::init failed)");
    }
    // Not started: the game's input devices and Controls.con (the editor takes input from its own
    // window and ImGui), the HUD, the menu event names, SwiffHost, LocalProfileManager and the music
    // player (the game's front end).
    logLine(LogLevel::Info, "engine: network, singletons");
    if (!setup->initNetwork())
        return fail("BF2EngineSetup::initNetwork");
    SingletonServer::init();

    // BF2Engine::init's tail. Not run: the exit callback (the editor's frame loop owns quitting,
    // WP4), the file-change monitor, the command history and Init.con - the mod's Init.con only queues
    // the intro movies ("These should be noops in server & editor", its own comment).
    debugDraw = new DebugDraw;

    m_phase = PhaseUp;
    const std::vector<IModInfo::Mod>* mods = modInfo->getModList();
    logLine(LogLevel::Info, "engine: up - mod '%s', %u mods known, archives %s, renderer %s",
            modInfo->getCurrentModPath().c_str(), mods ? static_cast<unsigned>(mods->size()) : 0u,
            config.useArchives ? "mounted" : "not mounted", m_renderer ? "up" : "not started (no window)");
    return true;
}

// BF2EngineSetup::initRenderer -> BF2Render::init: the carrier from the GS settings, LoadLibraryBf
// ("RendDX9.dll" - resolved from the exe's directory first; the editor stages its own there), the
// device created inside the DLL's initDll, then Renderer::initialize.
bool EngineHost::startRenderer(const EngineConfig& config)
{
    using namespace dice::hfe;

    // What BF2Render::init reads for the carrier: windowed, and exactly the editor window's client
    // size (a windowed back buffer takes the client size; the view, aspect and render targets take the
    // carrier's).
    settings->boolSet("GSFullScreen", false);
    settings->intSet("GSWindowSizeX", config.width);
    settings->intSet("GSWindowSizeY", config.height);

    logLine(LogLevel::Info, "engine: renderer (RendDX9.dll) on the editor window, %dx%d", config.width, config.height);
    if (!EngineAccess::setup(*m_engine)->initRenderer())
        return false;

    BF2Render* const render = EngineAccess::render(*m_engine);
    m_renderer = render ? render->getRenderer() : 0;
    if (!m_renderer)
    {
        logLine(LogLevel::Error, "engine: RendDX9 loaded but published no Renderer singleton");
        return false;
    }

    // Verify the one raw read (rendererDevice) against what the device itself says it was made for.
    IDirect3DDevice9* const device = rendererDevice(m_renderer);
    D3DDEVICE_CREATION_PARAMETERS created;
    if (!device || FAILED(device->GetCreationParameters(&created)) || created.hFocusWindow != m_window)
    {
        logLine(LogLevel::Error, "engine: the renderer's device is not the one created for the editor window");
        m_renderer = 0;
        return false;
    }
    m_device = device;

    std::string adapter = "adapter ?";
    IDirect3D9* d3d = 0;
    if (SUCCEEDED(device->GetDirect3D(&d3d)) && d3d)
    {
        D3DADAPTER_IDENTIFIER9 id;
        if (SUCCEEDED(d3d->GetAdapterIdentifier(created.AdapterOrdinal, 0, &id)))
            adapter = id.Description;
        d3d->Release();
    }
    char summary[256];
    std::snprintf(summary, sizeof(summary), "Direct3D 9, %s, %dx%d windowed, behaviour 0x%lX", adapter.c_str(),
                  config.width, config.height, static_cast<unsigned long>(created.BehaviorFlags));
    m_rendererSummary = summary;
    logLine(LogLevel::Info, "engine: renderer up - %s", summary);
    return true;
}

void EngineHost::shutdown()
{
    using namespace dice::hfe;

    if (m_phase == PhaseDown)
        return;
    logLine(LogLevel::Info, "engine: shutting down");

    if (m_phase == PhaseUp)
    {
        // BF2Engine::shutdown's parts that apply: the debug-draw object, then the world teardown.
        operator delete(debugDraw);                         // as shutdown() frees it
        debugDraw = 0;
        if (world::objectManager)
        {
            if (world::IObject* terrain = world::objectManager->getObjectFromNameHash("mainTerrain"))
                terrain->release();
        }
        if (world::objectTemplateManager)
            world::objectTemplateManager->deleteAll(false);
        world::SupplyDropManager::Instance();
        world::SupplyDropManager::Release();
        // Not saved: the command history and Usersettings.con (never loaded).
    }
    if (m_phase >= PhaseModules)
        shutdownSettingsRepostitory();

    // The hwnd is the editor's: take it back from the BF2Window first, or ~Window would hide it,
    // post WM_QUIT and DestroyWindow it (Game/Main/BF2Window.cpp).
    if (BF2Window* window = EngineAccess::window(*m_engine))
        window->setHwnd(0);
    m_renderer = 0;
    m_device = 0;
    m_window = 0;

    // The engine's own teardown: its sub-objects (the window, then BF2Render, which frees
    // RendDX9.dll), then BF2EngineSetup::shutdownModules (network, log, AI, late modules, modules) and
    // the setup object.
    m_engine->shutdownEngine();
    delete m_engine;                                        // ClassManager, the guard objects
    m_engine = 0;
    delete g_system;
    g_system = 0;

    m_phase = PhaseDown;
    logLine(LogLevel::Info, "engine: down");
}

void EngineHost::hudLevelLoaded()
{
    if (m_hudUp && dice::hfe::hudManager)
        dice::hfe::hudManager->onLevelLoaded();
}

} // namespace bf2editor
