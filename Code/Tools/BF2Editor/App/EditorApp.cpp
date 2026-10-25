// Code/Tools/BF2Editor/App/EditorApp.cpp - see EditorApp.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditorApp.h"

#include "App/EditSession.h"
#include "App/EditorLevel.h"
#include "App/EditorShell.h"
#include "App/FrameCapture.h"
#include "App/FrozenFrame.h"
#include "App/FreeCamera.h"
#include "App/HudEditor.h"
#include "App/ImGuiHost.h"
#include "App/LevelDocument.h"
#include "App/PlayTest.h"
#include "App/SceneOverlay.h"
#include "App/TestHarness.h"
#include "App/TexturePalette.h"
#include "App/ViewRecorder.h"
#include "App/ViewportTools.h"
#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"
#include "Framework/EditorSettings.h"
#include "Framework/SourceControl.h"

#include "Core/EventManager.h"
#include "RendMisc/IRenderer.h"
#include "RendMisc/IRenderView.h"
#include "Scene/Object/ObjectManager.h"

#include "imgui.h"

#include <windows.h>
#include <mmsystem.h>           // timeBeginPeriod (WIN32_LEAN_AND_MEAN leaves it out; winmm is linked)
#include <d3d9.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace bf2editor
{

namespace
{

const int kDefaultClientWidth = 1600;
const int kDefaultClientHeight = 900;

std::string savePartNames(unsigned parts)
{
    static const struct { unsigned bit; const char* name; } kParts[] = {
        { SaveAll, "all" }, { SaveObjects, "objects" }, { SaveTerrain, "terrain" },
        { SaveRoads, "roads" }, { SaveOvergrowth, "overgrowth" },
    };
    std::string names;
    for (size_t i = 0; i < sizeof(kParts) / sizeof(kParts[0]); ++i)
    {
        if (!(parts & kParts[i].bit))
            continue;
        if (!names.empty())
            names += ", ";
        names += kParts[i].name;
    }
    return names.empty() ? std::string("none") : names;
}

const char* orUnset(const std::string& text)
{
    return text.empty() ? "(not given)" : text.c_str();
}

// The editor's own settings live beside the executable, never in the game's or the retail editor's
// folders.
std::string besideExecutable(const char* fileName)
{
    char path[MAX_PATH];
    const DWORD length = GetModuleFileNameA(0, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH)
        return std::string();
    std::string dir(path, length);
    const size_t slash = dir.find_last_of("\\/");
    return (slash == std::string::npos ? std::string() : dir.substr(0, slash + 1)) + fileName;
}

// The camera's terrain (FollowTerrain): the open level's primary heightmap.
bool levelGround(void* context, float x, float z, float& height)
{
    return static_cast<const EditorLevel*>(context)->groundHeight(x, z, height);
}

// EditorView.panel: the shell owns the panels.
bool shellPanelHandler(void* context, const std::string& name, bool visible)
{
    return static_cast<EditorShell*>(context)->setPanelVisible(name, visible);
}

// EditorView.pauseRenderer / renderOneFrame: the shell's View switches. One frame is only for a paused
// renderer.
bool shellToolsHandler(void* context, const std::string& action, int index, const std::string& argument,
                       std::string& result)
{
    return static_cast<EditorShell*>(context)->toolsAction(action, index, argument, result);
}

bool shellViewHandler(void* context, const std::string& action)
{
    EditorShell* shell = static_cast<EditorShell*>(context);
    if (action == "pause" || action == "resume")
    {
        shell->setRenderPaused(action == "pause");
        return true;
    }
    if (action == "one" && shell->renderPaused())
    {
        shell->requestOneFrame();
        return true;
    }
    if (action == "pauseUpdate" || action == "resumeUpdate")
    {
        shell->setUpdatePaused(action == "pauseUpdate");
        return true;
    }
    if (action == "viewEditor" || action == "viewLevel")
    {
        shell->setLevelViewDistance(action == "viewLevel");
        return true;
    }
    if (action == "wireframeOn" || action == "wireframeOff")
    {
        shell->setWireframe(action == "wireframeOn");
        return true;
    }
    return false;
}

// +runConFile / +runConFileAndClose: every non-empty line of a plain file, in order, down the Console
// panel's path - each is logged with its answer, and a line whose effect the undo history cannot know
// clears it (§4). An `EditorTest.wait <frames>` line pauses the script: the interactive run steps it
// once a frame, so the lines after a wait run that many frames later (TestHarness). A batch run draws
// no frames and has no harness, so its script runs through at once.
class ScriptRun
{
public:
    ScriptRun() : m_next(0), m_wait(0), m_count(0), m_failed(0), m_started(false), m_done(true) {}

    bool load(const std::string& path)
    {
        m_started = true;
        std::ifstream in(path.c_str());
        if (!in)
        {
            logLine(LogLevel::Error, "+runConFile: cannot read %s", path.c_str());
            ++m_failed;
            return false;
        }
        std::string line;
        while (std::getline(in, line))
        {
            const size_t begin = line.find_first_not_of(" \t\r");
            if (begin == std::string::npos)
                continue;
            const size_t end = line.find_last_not_of(" \t\r");
            m_lines.push_back(line.substr(begin, end - begin + 1));
        }
        m_done = false;
        logLine(LogLevel::Info, "script: running %s", path.c_str());
        return true;
    }

    // Runs lines until a wait or the end of the script.
    void step(EditSession& session, SessionPanels& panels, TestHarness* harness)
    {
        if (m_done)
            return;
        if (m_wait > 0 && --m_wait > 0)
            return;
        while (m_next < m_lines.size())
        {
            if (!panels.submit(session, m_lines[m_next], "script"))
                ++m_failed;
            ++m_count;
            ++m_next;
            const int frames = harness ? harness->takeWait() : 0;
            if (frames > 0)
            {
                m_wait = frames;
                return;
            }
        }
        m_done = true;
        // An editor command that refused is a failed script: the run's exit code says so. (An engine
        // command's own complaint is only text, which the log shows.)
        logLine(m_failed ? LogLevel::Error : LogLevel::Info, "script: %u lines run, %u failed", m_count, m_failed);
    }

    bool started() const { return m_started; }
    bool done() const { return m_done; }
    bool ok() const { return m_failed == 0; }

private:
    std::vector<std::string> m_lines;
    size_t                   m_next;
    int                      m_wait;
    unsigned                 m_count;
    unsigned                 m_failed;
    bool                     m_started;
    bool                     m_done;
};

// Soldier mode's keys (PlayTest's control-map rows), read from ImGui's key state - so a scripted test's EditorTest.key
// drives the soldier as a hand would.
void readPlayTestKeys(bool keys[256])
{
    for (int c = 0; c < 26; ++c)
        keys['A' + c] = ImGui::IsKeyDown(static_cast<ImGuiKey>(ImGuiKey_A + c));
    for (int d = 0; d < 10; ++d)
        keys['0' + d] = ImGui::IsKeyDown(static_cast<ImGuiKey>(ImGuiKey_0 + d));
    for (int f = 0; f < 12; ++f)
        keys[VK_F1 + f] = ImGui::IsKeyDown(static_cast<ImGuiKey>(ImGuiKey_F1 + f));
    keys[VK_SPACE] = ImGui::IsKeyDown(ImGuiKey_Space);
    keys[VK_LSHIFT] = ImGui::IsKeyDown(ImGuiKey_LeftShift);
    keys[VK_LCONTROL] = ImGui::IsKeyDown(ImGuiKey_LeftCtrl);
}

} // namespace

EditorApp::EditorApp(const EditorOptions& options)
    : m_options(options)
{
}

EditorApp::~EditorApp()
{
    shutdownEngine();
    m_window.destroy();
}

int EditorApp::run()
{
    for (size_t i = 0; i < m_options.warnings.size(); ++i)
        logLine(LogLevel::Warning, "%s", m_options.warnings[i].c_str());

    if (!m_options.errors.empty())
    {
        for (size_t i = 0; i < m_options.errors.size(); ++i)
            logLine(LogLevel::Error, "%s", m_options.errors[i].c_str());
        logLine(LogLevel::Info, "run with +help for the switches");
        return ExitBadCommandLine;
    }

    if (m_options.help)
    {
        logLine(LogLevel::Info, "%s", usageText().c_str());
        return ExitOk;
    }

    logOptions();

    // The window comes first, because the renderer is created into it. A batch run opens a level too,
    // and the level load needs the renderer, so it gets a window that is never shown.
    {
        const int width = m_options.windowWidth > 0 ? m_options.windowWidth : kDefaultClientWidth;
        const int height = m_options.windowHeight > 0 ? m_options.windowHeight : kDefaultClientHeight;
        const std::string mod = m_options.mod.empty() ? std::string("bf2") : m_options.mod;
        if (!m_window.create("BF2Editor - mods/" + mod, width, height, !m_options.windowHidden && !m_options.isBatch()))
            return ExitEngineFailed;
    }

    if (!bootEngine())
        return ExitEngineFailed;

    const int result = m_options.isBatch() ? runBatch() : runInteractive();

    if (m_engine.renderer())
    {
        // Avoids renderer unloading while [FRONTIER] ParticleSystemManager teardown remains
        // incomplete. Windowed exit follows the game's process-exit route; headless runs use EngineHost shutdown.
        logLine(LogLevel::Info, "exit: leaving as BF2.exe does, without engine teardown (RendDX9's unload "
                                "path is still a frontier)");
        std::fflush(stdout);
        std::fflush(stderr);
        std::exit(result);
    }
    shutdownEngine();
    m_window.destroy();
    return result;
}

void EditorApp::logOptions() const
{
    logLine(LogLevel::Info, "mod: %s, level: %s", orUnset(m_options.mod), orUnset(m_options.level));
    if (m_options.isBatch())
        logLine(LogLevel::Info, "batch run: script %s, then save %s, then exit",
                orUnset(m_options.runConFileAndClose), savePartNames(m_options.batchSaveParts).c_str());
    for (size_t i = 0; i < m_options.forceLoadModules.size(); ++i)
        logLine(LogLevel::Info, "module forced on: %s", m_options.forceLoadModules[i].c_str());
    for (size_t i = 0; i < m_options.rendererOptions.size(); ++i)
        logLine(LogLevel::Info, "renderer option: %s %s", m_options.rendererOptions[i].first.c_str(),
                m_options.rendererOptions[i].second.c_str());
    const ISourceControl& scm = sourceControl();
    logLine(LogLevel::Info, "source control: %s (%s)", scm.name(),
            scm.isConnected() ? "connected" : "no workspace; files are edited in place");
}

bool EditorApp::bootEngine()
{
    EngineConfig config;
    config.mod = m_options.mod.empty() ? std::string("bf2") : m_options.mod;
    config.useArchives = m_options.useArchives != 0;               // on unless +useArchives 0
    config.looseObjects = m_options.looseObjects == 1;              // +looseObjects 1
    config.looseCommon = m_options.looseCommon == 1;                // +looseCommon 1
    config.hud = m_options.hud != 0;                                // on unless +hud 0
    // +enableAsserts decides; without it, batch runs ignore asserts (as retail's *AndClose did) and
    // interactive runs keep the game's default, where an Assert is fatal.
    config.ignoreAsserts = m_options.enableAsserts == 0 || (m_options.enableAsserts < 0 && m_options.isBatch());
    config.rendererOptions = m_options.rendererOptions;
    if (m_window.hwnd())
    {
        config.window = m_window.hwnd();
        config.width = m_window.clientWidth();
        config.height = m_window.clientHeight();
    }
    if (m_options.mod.empty())
        logLine(LogLevel::Info, "no +loadMod: using mod bf2");
    return m_engine.boot(config);
}

void EditorApp::shutdownEngine()
{
    m_engine.shutdown();
}

// Opens the level with a hidden renderer window, runs any script, then performs requested
// saves. Returns 0 for success, 3 for unsupported [FRONTIER] save parts, or 4 for load,
// script, or save failure. Supported saves still run when other parts are unsupported.
int EditorApp::runBatch()
{
    const std::string mod = m_options.mod.empty() ? std::string("bf2") : m_options.mod;
    EditorLevel level;
    FreeCamera camera;
    EditSession session(level, camera);
    session.setObjectArchivesMounted(m_options.useArchives != 0 && m_options.looseObjects != 1);
    session.setCommonArchivesMounted(m_options.useArchives != 0 && m_options.looseCommon != 1);
    SessionPanels panels;
    LevelDocument document;
    int result = ExitOk;

    if (!m_options.level.empty())
    {
        if (!level.open(m_options.level, m_options.gameMode, m_options.layerSize))
            return ExitEngineFailed;
        int made = 0;
        if (level.loadEditorContent(made))
            logLine(LogLevel::Info, "level: %d objects made for the editor's view", made);
        std::string error;
        const auto runLines = [&level](const std::vector<std::string>& lines) {
            std::string complaint;
            const int made = level.runObjectLines(lines, complaint);
            logLine(LogLevel::Info, "level: the other gameplay layers' lines made %d objects%s%s", made,
                    complaint.empty() ? "" : "; the first line that answered: ", complaint.c_str());
        };
        if (document.open(mod, level.name(), level.gameMode(), level.layerSize(), error, &session.layerModel(), runLines))
            session.setDocument(&document);
        else
            logLine(LogLevel::Error, "document: %s", error.c_str());
        if (!registerEditorConsole(&session))
            logLine(LogLevel::Error, "console: the editor's console commands could not be registered");
        // The terrain and overgrowth saves compile and generate through the editable terrain: opened for them, or
        // when +editableTerrain 1 asks for it (a script's height or paint edits).
        const bool terrainParts = (m_options.batchSaveParts & (SaveAll | SaveTerrain | SaveOvergrowth)) != 0;
        if (m_options.editableTerrain == 1 || (m_options.editableTerrain != 0 && terrainParts))
        {
            std::string report;
            if (session.editableTerrain().open(level.name(), report))
                logLine(LogLevel::Info, "terrain: %s", report.c_str());
            else
                logLine(LogLevel::Warning, "terrain: the compiled terrain stays: %s", report.c_str());
        }
        // The level's editable roads (App/RoadNetwork), and the mod's road templates.
        std::string roads;
        if (!session.openRoads(level.name(), roads))
            logLine(LogLevel::Warning, "%s", roads.c_str());
    }
    else if (m_options.batchSaveParts)
    {
        logLine(LogLevel::Error, "batch: a save switch names the level to save (+saveObjectsAndClose <level>)");
        return ExitBadCommandLine;
    }

    if (!m_options.runConFileAndClose.empty())
    {
        ScriptRun script;
        if (script.load(m_options.runConFileAndClose))
            script.step(session, panels, 0);
        if (!script.ok())
            result = ExitEngineFailed;
    }

    const unsigned parts = m_options.batchSaveParts;
    // The terrain and overgrowth parts first: the level save after them then finds them saved and does not compile
    // the terrain a second time.
    if (parts & (SaveAll | SaveTerrain))
    {
        std::string report;
        const bool saved = session.saveTerrainPart(report);
        logLine(saved ? LogLevel::Info : LogLevel::Error, "save: %s", report.c_str());
        if (!saved)
            result = ExitEngineFailed;
    }
    if (parts & (SaveAll | SaveOvergrowth))
    {
        std::string report;
        const bool saved = session.saveOvergrowthPart((parts & SaveOvergrowth) != 0, report);
        logLine(saved ? LogLevel::Info : LogLevel::Error, "save: %s",
                report.empty() ? "the level has no overgrowth to save" : report.c_str());
        if (!saved)
            result = ExitEngineFailed;
    }
    // The roads before the level: its save then finds them saved. The part compiles every editable road even
    // unchanged (the retail part save rewrote them); a level with none saves nothing.
    if (parts & (SaveAll | SaveRoads))
    {
        std::string report;
        const bool saved = session.saveRoads(true, report);
        logLine(saved ? LogLevel::Info : LogLevel::Error, "save: %s",
                report.empty() ? "the level has no editable roads to save" : report.c_str());
        if (!saved)
            result = ExitEngineFailed;
    }
    if (parts & (SaveAll | SaveObjects))
    {
        std::string report;
        const bool saved = session.saveLevel(report);
        logLine(saved ? LogLevel::Info : LogLevel::Error, "save: %s", report.c_str());
        if (!saved)
            result = ExitEngineFailed;
    }
    if (level.isOpen())
        unregisterEditorConsole();
    logLine(LogLevel::Info, "batch: done, exit code %d", result);
    return result;
}

// The interactive frame loop (WP4). Two ways to draw a frame:
//   - no level open: nothing in the world to draw, so the editor brackets the frame itself through
//     the renderer's own typed slots - beginScene(+0xC8) with a clear, ImGui, endScene(+0xCC),
//     present(+0xD0) - instead of Renderer::renderFrame, whose world passes have never run without a
//     level;
//   - a level open (E3): the world updates the client's frame runs before rendering (the events, the
//     object grid, the frame-update objects; BF2Engine::mainLoop / GameClient::stepWorld), the free
//     camera into the render view, then IRenderer::beginFrame(+0x20) and renderFrame(+0x28), which
//     draws, presents, and calls the SceneOverlay for ImGui inside its own scene.
int EditorApp::runInteractive()
{
    dice::hfe::dx9rend::IRenderer* const renderer = m_engine.renderer();
    IDirect3DDevice9* const device = m_engine.device();
    if (!renderer || !device)
    {
        logLine(LogLevel::Error, "interactive: the renderer is not up");
        return ExitEngineFailed;
    }

    // Hidden runs use the default dock layout and leave the user's saved layout untouched.
    ImGuiHost imgui;
    if (!imgui.init(m_window.hwnd(), device,
                    m_options.windowHidden ? std::string() : besideExecutable("BF2Editor.imgui.ini")))
        return ExitEngineFailed;
    m_window.setImGui(&imgui);
    TexturePalette::setDevice(device);      // the Object templates panel's texture thumbnails
    EditorShell shell;
    SceneOverlay overlay(imgui, device);
    if (!overlay.attach())
        return ExitEngineFailed;
    FrozenFrame frozen;                     // the paused renderer's last world frame (View > Pause the renderer)
    FreeCamera camera;

    // The editor's own settings (WP5 part 2). Automated runs (+windowHidden 1) neither read nor write
    // them, so a test can neither be changed by nor change the owner's settings - unless the run names
    // its own file with +settingsFile.
    const bool persistSettings = !m_options.windowHidden || !m_options.settingsFile.empty();
    const std::string settingsPath =
        m_options.settingsFile.empty() ? besideExecutable("BF2Editor.settings.ini") : m_options.settingsFile;
    EditorSettings settings;
    if (persistSettings)
    {
        std::vector<std::string> problems;
        settings.load(settingsPath, problems);
        for (size_t i = 0; i < problems.size(); ++i)
            logLine(LogLevel::Warning, "settings: %s", problems[i].c_str());
    }
    shell.loadSettings(settings);
    camera.setSpeed(settings.getFloat("camera.speed", camera.speed()));
    camera.setFarPlane(shell.options().viewDistance);

    ShellStatus status;
    status.mod = "mods/" + (m_options.mod.empty() ? std::string("bf2") : m_options.mod);
    status.archives = m_options.useArchives != 0;
    status.renderer = m_engine.rendererSummary();

    // +loadLevel opens the level before the first frame, on the gameplay layer +gameMode and +layerSize
    // name - conquest, the largest layer up to 64 players, by default (GameLogic::load picks the layer
    // from these).
    EditorLevel level;
    camera.setGroundQuery(&levelGround, &level);
    if (!m_options.level.empty())
    {
        if (level.open(m_options.level, m_options.gameMode, m_options.layerSize))
        {
            status.level = level.name() + " (" + level.gameMode() + ")";
            shell.resources().addRoot("Levels/" + level.name());
            // Default start: 30 m above the ground at the camera's own spot (south of the map centre),
            // facing north and a little down. Stock levels fog out close (Strike_at_Karkand's fog ends at
            // 135 m), so a high start sees only fog - and the ground must be sampled where the camera
            // stands, not at the centre (Karkand: 94.9 m at the centre, 145.8 m 60 m south of it).
            float startX = 0.0f, startZ = -60.0f, ground = 0.0f;
            if (m_options.cameraGiven)
            {
                startX = m_options.camera[0];
                startZ = m_options.camera[2];
            }
            const bool haveGround = level.groundHeight(startX, startZ, ground);
            if (haveGround)
                logLine(LogLevel::Info, "level: ground under the camera start is %.1f m", ground);
            // +cameraAt wins; otherwise where the camera was when this level was last closed.
            float saved[5];
            char tail = 0;
            const std::string savedCamera = settings.get("level." + level.name() + ".camera", std::string());
            if (m_options.cameraGiven)
                camera.place(m_options.camera[0], m_options.camera[1], m_options.camera[2], m_options.camera[3],
                             m_options.camera[4]);
            else if (shell.options().rememberCamera
                     && std::sscanf(savedCamera.c_str(), "%f,%f,%f,%f,%f%c", &saved[0], &saved[1], &saved[2],
                                    &saved[3], &saved[4], &tail) == 5)
            {
                camera.place(saved[0], saved[1], saved[2], saved[3], saved[4]);
                logLine(LogLevel::Info, "level: the camera starts where it was left (%s)", savedCamera.c_str());
            }
            else
                camera.place(startX, (haveGround ? ground : 100.0f) + 30.0f, startZ, 0.0f, -15.0f);
        }
        else
        {
            status.level = m_options.level + " - failed to open (see the log)";
        }
    }

    int result = ExitOk;

    // WP5: with a level open, an edit session (selection, undo history, picking) answers the editor's
    // console commands, and the shell shows its panels.
    EditSession session(level, camera);
    session.setViewSize(m_window.clientWidth(), m_window.clientHeight());
    session.setCanRelaunch(!m_options.windowHidden);
    session.setObjectArchivesMounted(m_options.useArchives != 0 && m_options.looseObjects != 1);
    session.setCommonArchivesMounted(m_options.useArchives != 0 && m_options.looseCommon != 1);
    ViewportTools tools;
    const int savedMode = settings.getInt("gizmo.mode", static_cast<int>(session.gizmoMode()));
    if (savedMode >= 0 && savedMode <= 2)
        session.setMode(static_cast<GizmoMode>(savedMode));
    session.setSpace(settings.getInt("gizmo.space", 0) == 1 ? GizmoSpace::Local : GizmoSpace::World);
    tools.setSnap(settings.getBool("gizmo.snap", tools.snap()), settings.getFloat("gizmo.moveSnap", tools.moveSnap()),
                  settings.getFloat("gizmo.rotateSnap", tools.rotateSnap()));
    // WP6: the level's object files, opened once the level has loaded. Without them the level can still
    // be edited, but not saved (the log says why).
    LevelDocument document;
    std::unique_ptr<PlayTest> playTest;     // soldier mode (App/PlayTest.h), once a level is open
    std::unique_ptr<HudEditor> hud;         // the game's HUD and the HUD editor, once the HUD is up
    if (level.isOpen())
    {
        int made = 0;
        if (level.loadEditorContent(made))
            logLine(LogLevel::Info, "level: %d objects made for the editor's view", made);
        std::string error;
        // The retail layer model: the other gameplay layers' objects made before the binding.
        const auto runLines = [&level](const std::vector<std::string>& lines) {
            std::string complaint;
            const int made = level.runObjectLines(lines, complaint);
            logLine(LogLevel::Info, "level: the other gameplay layers' lines made %d objects%s%s", made,
                    complaint.empty() ? "" : "; the first line that answered: ", complaint.c_str());
        };
        if (document.open(m_options.mod.empty() ? std::string("bf2") : m_options.mod, level.name(), level.gameMode(),
                          level.layerSize(), error, &session.layerModel(), runLines))
            session.setDocument(&document);
        else
            logLine(LogLevel::Warning, "document: %s - the level can be edited, but not saved", error.c_str());
        if (!registerEditorConsole(&session))
            logLine(LogLevel::Error, "console: the editor's console commands could not be registered");
        setPanelHandler(&shellPanelHandler, &shell);
        setViewHandler(&shellViewHandler, &shell);
        setToolsHandler(&shellToolsHandler, &shell);
        shell.setSession(&session);
        if (level.editorGame())
        {
            playTest.reset(new PlayTest(session, camera, *level.editorGame()));
            setPlayTest(playTest.get());
        }
        m_engine.hudLevelLoaded();          // the game's HUD: its map and the level's own settings
        if (m_engine.hudUp())
        {
            hud.reset(new HudEditor(session));
            setHudCommands(hud.get());
            shell.setHudEditor(hud.get());
            session.setHudDocument(hud.get());      // its scripts are saved with the level
        }
        // The level's terrain becomes the renderer's TerrainEditable (Terrain.con's editor branch), as the retail editor
        // showed it, unless +editableTerrain 0 keeps the compiled one.
        if (m_options.editableTerrain != 0)
        {
            std::string report;
            if (session.editableTerrain().open(level.name(), report))
                logLine(LogLevel::Info, "terrain: %s", report.c_str());
            else
                logLine(LogLevel::Warning, "terrain: the compiled terrain stays: %s", report.c_str());
        }
        // The level's editable roads (App/RoadNetwork), and the mod's road templates.
        {
            std::string roads;
            if (!session.openRoads(level.name(), roads))
                logLine(LogLevel::Warning, "%s", roads.c_str());
        }
        // Tools > Options: the start-up command, once the level is open (never in a hidden run, which
        // reads no settings).
        if (!shell.options().startupCommand.empty())
        {
            const ConsoleLineResult r = runConsoleLine(shell.options().startupCommand);
            logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "start-up command: %s -> %s",
                    shell.options().startupCommand.c_str(), r.output.empty() ? "done" : r.output.c_str());
        }
    }
    // The minimap navigator: the level's HUD map over the primary heightmap's extent.
    float worldBounds[4];
    if (level.isOpen() && level.worldBounds(worldBounds))
        shell.minimap().load(device, level.name(), worldBounds);
    tools.setResources(&shell.resources());
    const std::string baseTitle = "BF2Editor - mods/" + (m_options.mod.empty() ? std::string("bf2") : m_options.mod)
                                + (level.isOpen() ? " - " + level.name() : std::string());
    m_window.setTitle(baseTitle);
    bool titleDirty = false;
    ScriptRun script;
    bool scriptPending = !m_options.runConFile.empty();
    if (scriptPending && !level.isOpen())
    {
        logLine(LogLevel::Error, "+runConFile: no level is open, so the script does not run");
        scriptPending = false;
        result = ExitEngineFailed;
    }
    // Scripted UI tests (EditorTest.*) drive this run's input.
    TestHarness harness(session, camera);
    if (level.isOpen())
        setTestHarness(&harness);

    // The renderer presents without vsync (BF2Render::init's carrier), so the loop paces itself:
    // about 60 frames a second, 1 ms sleep granularity for the duration of the loop.
    const double kFrameSeconds = 1.0 / 60.0;
    timeBeginPeriod(1);
    LARGE_INTEGER frequency, frameStart, now;
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&frameStart);
    double frameSeconds = kFrameSeconds;

    static const unsigned int kClear[3] = { 38, 42, 50 };           // R, G, B (beginScene's order)
    bool deviceObjectsLive = true;
    bool reportedNoScene = false;
    unsigned int frame = 0;
    bool logFileOpen = false;           // Options > Write the log to a file, as last applied

    // Closing with unsaved changes asks first (WP6); an automated run (+windowHidden) just leaves.
    bool askToClose = false;
    bool leave = false;
    std::string unsavedText;
    bool relaunchAsked = false;         // a layer switch is waiting on the close prompt

    logLine(LogLevel::Info, "interactive: frame loop%s", m_options.maxFrames ? " (bounded by +frames)" : "");
    bool wasPlaying = false;            // soldier mode was on last frame (the mouse is taken when it starts)
    for (;;)
    {
        // Soldier mode's captured mouse is the game's, not the UI's (App/PlayTest.h).
        if (m_window.mouseCaptured())
            ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange;
        else
            ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange);
        const bool windowOpen = m_window.pumpMessages();
        // A layer switch leaves like a close does, through the same prompt.
        const bool relaunchNow = session.relaunchRequested() && !relaunchAsked;
        if (relaunchNow)
            relaunchAsked = true;
        if (!windowOpen || shell.quitRequested() || relaunchNow)
        {
            m_window.cancelClose();
            shell.clearQuit();
            // Only when a save would write something: edits undone or cancelled out leave nothing to ask.
            if (level.isOpen() && session.history().isDirty() && !m_options.windowHidden && session.hasUnsavedChanges())
            {
                askToClose = true;
                unsavedText = session.unsavedSummary();
            }
            else
            {
                break;
            }
        }

        // ImGui's DX9 objects are D3DPOOL_DEFAULT, and RendDX9 retries a failed Reset without bound
        // inside present() (RendDX9Init.cpp handleLostDevice). So the moment the device is not usable,
        // ImGui lets go of its objects - before the renderer gets another chance to Reset - and takes
        // them back only once the device is usable again.
        const HRESULT cooperative = device->TestCooperativeLevel();
        if (cooperative != D3D_OK && deviceObjectsLive)
        {
            frozen.release();
            imgui.onDeviceLost();
            deviceObjectsLive = false;
            logLine(LogLevel::Warning, "interactive: device lost (0x%08lX); UI objects released",
                    static_cast<unsigned long>(cooperative));
        }
        else if (cooperative == D3D_OK && !deviceObjectsLive)
        {
            imgui.onDeviceReset();
            deviceObjectsLive = true;
            logLine(LogLevel::Info, "interactive: device back; UI objects recreated");
        }

        // +runConFile starts after the first frame drew the level (the view is set by then), and steps
        // once a frame from then on.
        if (scriptPending && frame >= 1)
        {
            scriptPending = false;
            script.load(m_options.runConFile);
        }
        if (!script.done())
        {
            script.step(session, shell.panels(), &harness);
            if (script.done() && !script.ok())
                result = ExitEngineFailed;
        }
        else if (script.started() && !script.ok())
        {
            result = ExitEngineFailed;
        }

        // The world moves first - the updates the client's frame runs before drawing (the events, the
        // object grid, the frame-update objects) - so the UI below sees and edits this frame's state.
        // Options > Pause while another window has the focus: the editor idles as when minimised (a hidden run
        // never has the focus, and never loads the option).
        const bool minimized = m_window.isMinimized()
                            || (shell.options().pauseOnFocusLoss && GetForegroundWindow() != m_window.hwnd());
        camera.setLookSensitivity(shell.options().mouseSensitivity);
        if (shell.options().logToFile != logFileOpen)
        {
            std::string logError;
            logFileOpen = shell.options().logToFile;
            if (!setLogFile(logFileOpen ? besideExecutable("BF2Editor.log") : std::string(), logError))
                logLine(LogLevel::Warning, "log: %s", logError.c_str());
            else if (logFileOpen)
                logLine(LogLevel::Info, "log: writing to %s", besideExecutable("BF2Editor.log").c_str());
        }
        const float dt = static_cast<float>(frameSeconds);
        const bool playing = playTest && playTest->active();
        if (level.isOpen() && !minimized)
        {
            if (!playing)                       // soldier mode's steps run the events themselves, once a step
                dice::hfe::eventManager->processEvents(dt);
            dice::hfe::frameEventManager->processEvents(dt);
            dice::hfe::world::objectManager->updateObjectsInGrid();
            if (!shell.updatePaused() || playing)
                dice::hfe::world::objectManager->handleFrameUpdates(dt);
        }
        if (level.isOpen())
        {
            session.update();               // hidden layers follow edits, undos and redos
            if (hud)
                hud->update(dt, playing);   // the game's HUD: drawn in the play test and the preview
        }
        LARGE_INTEGER afterWorld, afterUi;
        QueryPerformanceCounter(&afterWorld);

        // View > Pause the renderer: a paused frame draws the kept world frame behind the UI and runs no world
        // frame; the first paused frame, and Render one frame, run the renderer and keep what it drew.
        if (!shell.renderPaused() && frozen.texture())
            frozen.release();
        const bool oneFrame = level.isOpen() && shell.renderPaused() && shell.takeOneFrame();
        const bool frozenFrame = level.isOpen() && shell.renderPaused() && !oneFrame && frozen.texture() && deviceObjectsLive;
        imgui.beginFrame();
        if (frozenFrame)
            ImGui::GetBackgroundDrawList()->AddImage(
                ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<intptr_t>(frozen.texture()))), ImVec2(0.0f, 0.0f),
                ImGui::GetIO().DisplaySize);
        if (level.isOpen())
            harness.beforeUi();
        dice::hfe::dx9rend::IRenderView* const view = level.isOpen() ? renderer->getRenderView() : 0;
        if (level.isOpen())
        {
            camera.setModifiedWheelReserved(session.terrainToolActive() || session.brushToolActive());
            float farPlane = shell.options().viewDistance;
            std::string levelDistance, ignored;
            if (shell.levelViewDistance() && session.levelSetting("viewDistance", levelDistance, ignored)
                && std::atof(levelDistance.c_str()) > 0.0)
                farPlane = static_cast<float>(std::atof(levelDistance.c_str()));
            camera.setFarPlane(farPlane);
            // Soldier mode: this frame's input to the play-test's steps, and the view from the player's camera; the
            // editor camera waits where it was. Esc gives the mouse back; a click in the view takes it again.
            bool playerView = false;
            if (playTest && playTest->active())
            {
                if (!m_options.windowHidden)
                {
                    if (m_window.mouseCaptured() && ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                        m_window.setMouseCaptured(false);
                    else if (!m_window.mouseCaptured()
                             && (!wasPlaying || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !pointerOverUi())))
                        m_window.setMouseCaptured(true);
                }
                PlayTestInput input;
                if (!ImGui::GetIO().WantTextInput)
                    readPlayTestKeys(input.keys);
                m_window.takeRawMouse(input.lookX, input.lookY, input.buttons);
                if (!minimized)
                    playTest->frame(dt, input);
                playerView = playTest->applyView(view, m_window.clientWidth(), m_window.clientHeight(), farPlane, dt);
            }
            wasPlaying = playTest && playTest->active();
            if (!wasPlaying && m_window.mouseCaptured())
                m_window.setMouseCaptured(false);
            if (!playerView)
            {
                camera.update(dt);
                // Before the UI is built, so the gizmo and the outlines use this frame's matrices.
                camera.apply(view, m_window.clientWidth(), m_window.clientHeight());
            }
            session.setViewSize(m_window.clientWidth(), m_window.clientHeight());
            session.setViewCentre(shell.viewX() + shell.viewWidth() * 0.5f, shell.viewY() + shell.viewHeight() * 0.5f);
        }
        char cameraText[128];
        std::snprintf(cameraText, sizeof(cameraText), "%.0f / %.0f / %.0f, yaw %.0f, pitch %.0f, %.0f m/s, %s",
                      camera.x(), camera.y(), camera.z(), camera.yawDegrees(), camera.pitchDegrees(), camera.speed(),
                      FreeCamera::modeName(camera.mode()));
        status.camera = level.isOpen() ? cameraText : "";
        shell.draw(status);
        if (level.isOpen() && playTest && playTest->active())
        {
            // Soldier mode: the editor's overlays, gizmo and picking follow the editor camera, not the player's, so the
            // view shows only the game's picture and how to leave.
            const char* const hint = m_window.mouseCaptured()
                ? "Soldier mode: W/A/S/D walk, the mouse looks, Space jumps, Ctrl crouches, Z lies down, E enters or "
                  "leaves a vehicle, 1-9 pick a weapon. Esc frees the mouse."
                : "Soldier mode: click in the view to take the mouse. Camera > Soldier (or any camera mode) leaves.";
            ImGui::GetForegroundDrawList()->AddText(ImVec2(shell.viewX() + 12.0f, shell.viewY() + 10.0f),
                                                    IM_COL32(255, 255, 255, 230), hint);
        }
        else if (level.isOpen())
            tools.draw(session, camera, view, ImVec2(shell.viewX(), shell.viewY()),
                       ImVec2(shell.viewWidth(), shell.viewHeight()));
        switch (shell.drawClosePrompt(askToClose, unsavedText))
        {
        case EditorShell::CloseChoice::Save:
        {
            askToClose = false;
            std::string report;
            const bool saved = session.saveLevel(report);
            logLine(saved ? LogLevel::Info : LogLevel::Error, "save: %s", report.c_str());
            leave = saved;              // a failed save keeps the editor open, with the reason in the log
            if (!saved)
            {
                session.cancelRelaunch();
                relaunchAsked = false;
            }
            break;
        }
        case EditorShell::CloseChoice::Discard:
            askToClose = false;
            leave = true;
            logLine(LogLevel::Warning, "exit: unsaved changes discarded (%s)", unsavedText.c_str());
            break;
        case EditorShell::CloseChoice::Cancel:
            askToClose = false;
            session.cancelRelaunch();
            relaunchAsked = false;
            break;
        case EditorShell::CloseChoice::None:
            break;
        }
        imgui.endFrame();
        QueryPerformanceCounter(&afterUi);

        // A modified level shows in the title ("*"), from the undo history (§4).
        if (level.isOpen() && session.history().isDirty() != titleDirty)
        {
            titleDirty = !titleDirty;
            m_window.setTitle(baseTitle + (titleDirty ? " *" : ""));
        }

        // +frames bounds the run, but a script still waiting on frames keeps it going.
        const bool lastFrame = m_options.maxFrames && frame + 1 >= m_options.maxFrames && !scriptPending && script.done();
        const std::string screenshot = lastFrame ? m_options.screenshot : std::string();
        if (minimized)
        {
            // nothing to draw into
        }
        else if (frozenFrame)
        {
            if (renderer->beginScene(kClear, 1))
            {
                imgui.renderDrawData();
                renderer->endScene();
                if (!screenshot.empty() && !saveBackBufferBmp(device, screenshot))
                    result = ExitEngineFailed;
                renderer->present(2);
            }
        }
        else if (level.isOpen())
        {
            if (shell.renderPaused() && deviceObjectsLive)
                overlay.setCapture(&frozen);
            overlay.arm(deviceObjectsLive, screenshot);
            // View > Wireframe: the device's fill mode for the world; the overlay puts it back to solid before the
            // UI draws (and ImGui's own state set does too).
            device->SetRenderState(D3DRS_FILLMODE, shell.wireframe() ? D3DFILL_WIREFRAME : D3DFILL_SOLID);
            renderer->beginFrame();
            renderer->renderFrame(frameSeconds, dt);
            if (!overlay.fired() && !reportedNoScene)
            {
                logLine(LogLevel::Warning, "interactive: renderFrame drew no scene this frame (no UI either)");
                reportedNoScene = true;
            }
            if (!screenshot.empty() && !overlay.screenshotSaved())
                result = ExitEngineFailed;
        }
        else if (renderer->beginScene(kClear, 1))
        {
            if (deviceObjectsLive)
                imgui.renderDrawData();
            renderer->endScene();
            if (!screenshot.empty() && !saveBackBufferBmp(device, screenshot))
                result = ExitEngineFailed;
            renderer->present(2);
        }

        // The frame's own cost, then the pacing sleep (a minimised window idles longer); the next
        // frame's time step is the whole interval, sleep included.
        QueryPerformanceCounter(&now);
        const double busy = static_cast<double>(now.QuadPart - frameStart.QuadPart) / frequency.QuadPart;
        status.frameMs = static_cast<float>(busy * 1000.0);
        const double budget = m_window.isMinimized() ? 0.1 : kFrameSeconds;
        if (!lastFrame && busy < budget)
            Sleep(static_cast<DWORD>((budget - busy) * 1000.0));
        const LARGE_INTEGER afterRender = now;
        QueryPerformanceCounter(&now);
        frameSeconds = static_cast<double>(now.QuadPart - frameStart.QuadPart) / frequency.QuadPart;
        // The Profiler panel: this frame's phases (the world phase also carries the message pump).
        const double toMs = 1000.0 / static_cast<double>(frequency.QuadPart);
        FramePhases phases;
        phases.world = static_cast<float>((afterWorld.QuadPart - frameStart.QuadPart) * toMs);
        phases.ui = static_cast<float>((afterUi.QuadPart - afterWorld.QuadPart) * toMs);
        phases.render = static_cast<float>((afterRender.QuadPart - afterUi.QuadPart) * toMs);
        phases.idle = static_cast<float>((now.QuadPart - afterRender.QuadPart) * toMs);
        phases.total = static_cast<float>(frameSeconds * 1000.0);
        shell.profile().push(phases);
        frameStart = now;
        status.frame = ++frame;
        if (lastFrame || leave)
            break;
    }
    timeEndPeriod(1);

    m_window.setMouseCaptured(false);
    if (hud)
    {
        setHudCommands(0);
        shell.setHudEditor(0);
        session.setHudDocument(0);
        hud.reset();
    }
    if (playTest)
    {
        setPlayTest(0);
        playTest->shutdown();               // soldier mode off and the player removed, before the level closes
        playTest.reset();
    }
    if (level.isOpen())
    {
        shell.setSession(0);
        setPanelHandler(0, 0);
        setViewHandler(0, 0);
        setTestHarness(0);
        unregisterEditorConsole();
    }
    if (persistSettings)
    {
        settings.setFloat("camera.speed", camera.speed());
        settings.setInt("gizmo.mode", static_cast<int>(session.gizmoMode()));
        settings.setInt("gizmo.space", static_cast<int>(session.gizmoSpace()));
        settings.setBool("gizmo.snap", tools.snap());
        settings.setFloat("gizmo.moveSnap", tools.moveSnap());
        settings.setFloat("gizmo.rotateSnap", tools.rotateSnap());
        shell.saveSettings(settings);
        if (level.isOpen() && shell.options().rememberCamera)
        {
            char where[96];
            std::snprintf(where, sizeof(where), "%.2f,%.2f,%.2f,%.1f,%.1f", camera.x(), camera.y(), camera.z(),
                          camera.yawDegrees(), camera.pitchDegrees());
            settings.set("level." + level.name() + ".camera", where);
        }
        std::string error;
        if (settings.save(settingsPath, error))
            logLine(LogLevel::Info, "settings: saved %s", settingsPath.c_str());
        else
            logLine(LogLevel::Warning, "settings: %s", error.c_str());
    }
    logLine(LogLevel::Info, "interactive: %u frames, leaving the frame loop", frame);
    // A layer switch: the editor again, with the same switches, on the other layer (the camera comes
    // back from the settings just saved).
    if (session.relaunchRequested())
    {
        const std::string commandLine = relaunchCommandLine(m_options.commandLine, session.relaunchMode(),
                                                            session.relaunchSize(), session.relaunchLevel(),
                                                            session.relaunchMod());
        char exe[MAX_PATH];
        const DWORD length = GetModuleFileNameA(0, exe, MAX_PATH);
        std::string line = "\"" + std::string(exe, length) + "\" " + commandLine;
        std::vector<char> buffer(line.begin(), line.end());
        buffer.push_back(0);
        STARTUPINFOA startup;
        std::memset(&startup, 0, sizeof(startup));
        startup.cb = sizeof(startup);
        PROCESS_INFORMATION process;
        if (length > 0 && CreateProcessA(0, &buffer[0], 0, 0, FALSE, 0, 0, 0, &startup, &process))
        {
            CloseHandle(process.hThread);
            CloseHandle(process.hProcess);
            logLine(LogLevel::Info, "layer: started the editor again: %s", commandLine.c_str());
        }
        else
            logLine(LogLevel::Error, "layer: could not start the editor again (error %lu): %s",
                    static_cast<unsigned long>(GetLastError()), commandLine.c_str());
    }
    // A recording still running is finished, so its file is whole.
    viewRecorder().shutdown();
    shell.minimap().release();
    TexturePalette::setDevice(0);
    overlay.detach();
    m_window.setImGui(0);
    imgui.shutdown();
    return result;
}

} // namespace bf2editor
