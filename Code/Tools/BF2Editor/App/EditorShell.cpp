// Code/Tools/BF2Editor/App/EditorShell.cpp - see EditorShell.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/EditorShell.h"

#include "App/LevelDocument.h"

#include "App/EditSession.h"
#include "App/FreeCamera.h"
#include "App/HudEditor.h"
#include "App/ImGuiHost.h"
#include "App/ShortcutKeys.h"
#include "App/ViewRecorder.h"

#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"
#include "Framework/EditorSettings.h"
#include "Framework/HelpDocuments.h"
#include "Framework/LevelCopier.h"
#include "Framework/ObjectLibrary.h"
#include "Framework/SourceControl.h"

#include "imgui.h"

#include <cctype>
#include "imgui_internal.h"     // the DockBuilder API (docking branch)

#include <cstdio>
#include <cstring>

namespace bf2editor
{

namespace
{

// The first-run layout: Selection and Engine on the right above History, Resources and Objects on the
// left, Log and Console along the bottom, the central node left empty and transparent for the 3D view.
// Built only when ImGui has no saved layout for the dockspace (a layout the user arranged is kept in
// BF2Editor.imgui.ini and restored instead; panels newer than that layout dock beside their
// neighbours on first use, SessionPanels.cpp).
void buildDefaultLayout(ImGuiID dockspace, const ImGuiViewport* viewport)
{
    ImGui::DockBuilderRemoveNode(dockspace);
    ImGui::DockBuilderAddNode(dockspace, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace, viewport->WorkSize);
    ImGuiID centre = dockspace;
    ImGuiID right = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Right, 0.22f, 0, &centre);
    const ImGuiID left = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Left, 0.22f, 0, &centre);
    const ImGuiID bottom = ImGui::DockBuilderSplitNode(centre, ImGuiDir_Down, 0.26f, 0, &centre);
    const ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.4f, 0, &right);
    ImGui::DockBuilderDockWindow("Selection", right);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderDockWindow("Engine", right);
    ImGui::DockBuilderDockWindow("HUD editor", right);
    ImGui::DockBuilderDockWindow("History", rightBottom);
    ImGui::DockBuilderDockWindow("Layers", rightBottom);
    ImGui::DockBuilderDockWindow("Profiler", rightBottom);
    ImGui::DockBuilderDockWindow("Minimap", rightBottom);
    ImGui::DockBuilderDockWindow("Resources", left);
    ImGui::DockBuilderDockWindow("Objects", left);
    ImGui::DockBuilderDockWindow("Log", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderFinish(dockspace);
}

} // namespace

EditorShell::EditorShell()
    : m_session(0)
    , m_showLog(true)
    , m_showEngine(true)
    , m_showResources(true)
    , m_showProfiler(false)
    , m_showMinimap(true)
    , m_renderPaused(false)
    , m_updatePaused(false)
    , m_levelViewDistance(false)
    , m_renderOneFrame(false)
    , m_showDemo(false)
    , m_showAbout(false)
    , m_showCatalogue(false)
    , m_openRecord(false)
    , m_showShortcuts(false)
    , m_captureCooldown(0)
    , m_showOptions(false)
    , m_openPack(false)
    , m_openSaveAs(false)
    , m_saveAsOpen(true)
    , m_openLightPreset(false)
    , m_quit(false)
    , m_logAutoScroll(true)
{
    m_view[0] = m_view[1] = m_view[2] = m_view[3] = 0.0f;
    m_cameraTarget[0] = m_cameraTarget[1] = m_cameraTarget[2] = 0.0f;
    m_startupBuffer[0] = m_rawDataBuffer[0] = m_packBuffer[0] = 0;
    m_saveAsName[0] = 0;
    m_lightPresetName[0] = 0;
    m_catalogueFilter[0] = 0;
    m_recordPath[0] = 0;
    for (int i = 0; i < 6; ++i)
        m_customBuffers[i][0] = 0;
}

void EditorShell::drawLayerMenu()
{
    LevelDocument* document = m_session ? m_session->document() : 0;
    if (!document || !document->isOpen() || !ImGui::BeginMenu("Gameplay layer"))
        return;
    const std::vector<std::pair<std::string, int> > layers = document->layersOnDisk();
    std::string open;
    m_session->listLayers(open);
    for (size_t i = 0; i < layers.size(); ++i)
    {
        char label[96];
        std::snprintf(label, sizeof(label), "%s %d", layers[i].first.c_str(), layers[i].second);
        const bool current = open.find("*" + std::string(label)) != std::string::npos;
        if (ImGui::MenuItem(label, 0, current, !current))
        {
            std::snprintf(label, sizeof(label), "EditorLevel.switchLayer %s %d", layers[i].first.c_str(), layers[i].second);
            m_session->runCommand(label, "layer");
        }
    }
    ImGui::Separator();
    // The open layer copied as another size of its mode (the sizes it lacks).
    if (ImGui::BeginMenu("Copy the open layer as"))
    {
        std::string mode;
        for (size_t i = 0; i < layers.size(); ++i)
        {
            char label[96];
            std::snprintf(label, sizeof(label), "*%s %d", layers[i].first.c_str(), layers[i].second);
            if (open.find(label) != std::string::npos)
                mode = layers[i].first;
        }
        static const int kSizes[] = { 16, 32, 64, 128 };
        for (size_t s = 0; s < sizeof(kSizes) / sizeof(kSizes[0]) && !mode.empty(); ++s)
        {
            bool present = false;
            for (size_t i = 0; i < layers.size(); ++i)
                present = present || (layers[i].first == mode && layers[i].second == kSizes[s]);
            char label[96];
            std::snprintf(label, sizeof(label), "%s %d", mode.c_str(), kSizes[s]);
            if (ImGui::MenuItem(label, 0, false, !present))
            {
                std::snprintf(label, sizeof(label), "EditorLevel.duplicateLayer %s %d", mode.c_str(), kSizes[s]);
                m_session->runCommand(label, "layer");
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
}

void EditorShell::draw(const ShellStatus& status)
{
    // The central node passes through: nothing is drawn there, so the scene behind it stays visible.
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImGuiID dockspace = ImGui::GetID("BF2EditorDockSpace");
    if (!ImGui::DockBuilderGetNode(dockspace))
        buildDefaultLayout(dockspace, viewport);
    ImGui::DockSpaceOverViewport(dockspace, viewport, ImGuiDockNodeFlags_PassthruCentralNode);
    const ImGuiDockNode* centre = ImGui::DockBuilderGetCentralNode(dockspace);
    m_view[0] = centre ? centre->Pos.x : viewport->WorkPos.x;
    m_view[1] = centre ? centre->Pos.y : viewport->WorkPos.y;
    m_view[2] = centre ? centre->Size.x : viewport->WorkSize.x;
    m_view[3] = centre ? centre->Size.y : viewport->WorkSize.y;

    // F11: every panel out of the way, and back (the retail editor's Ctrl+Tab, which ImGui's window
    // switcher owns here).
    // Nothing fires while the Keyboard shortcuts window takes a key, nor in the frame it took one, nor in soldier mode
    // (the keys are the soldier's: 1-9 pick weapons, E enters vehicles, Ctrl crouches).
    suspendShortcuts(!m_capturing.empty() || m_captureCooldown > 0 || playTestActive());
    if (m_captureCooldown > 0)
        --m_captureCooldown;
    if (shortcutPressed("view.fullView", true))
        setPanelVisible("all", !m_hiddenForFullView.empty());
    drawMenu();
    if (!m_focusRequest.empty())
    {
        ImGui::SetWindowFocus(m_focusRequest.c_str());
        m_focusRequest.clear();
    }
    if (m_showLog)
        drawLog();
    if (m_showEngine)
        drawEngine(status);
    if (m_showResources)
        m_resources.draw(&m_showResources);
    if (m_session)
    {
        m_panels.draw(*m_session);
        m_session->setPreviewBeforeSave(m_options.previewSave);
        m_session->setBackupsKept(m_options.backupsKept);
        if (m_session->takeSaveRequest())
            m_savePreview.requestPreview();
        m_savePreview.draw(*m_session, m_options.previewSave);
    }
    if (m_showProfiler)
    {
        // A layout saved before the panel existed: open it beside the Engine panel the first time.
        ImGuiWindow* engine = ImGui::FindWindowByName("Engine");
        if (engine && engine->DockId != 0)
            ImGui::SetNextWindowDockID(engine->DockId, ImGuiCond_FirstUseEver);
        m_profile.draw(&m_showProfiler);
    }
    if (m_showMinimap && m_session)
    {
        // A layout saved before the panel existed: open it beside the History panel the first time.
        ImGuiWindow* history = ImGui::FindWindowByName("History");
        if (history && history->DockId != 0)
            ImGui::SetNextWindowDockID(history->DockId, ImGuiCond_FirstUseEver);
        m_minimap.draw(&m_showMinimap, *m_session);
    }
    if (m_showHudEditor && m_hud)
    {
        // A layout saved before the panel existed: open it beside the Engine panel the first time.
        ImGuiWindow* engine = ImGui::FindWindowByName("Engine");
        if (engine && engine->DockId != 0)
            ImGui::SetNextWindowDockID(engine->DockId, ImGuiCond_FirstUseEver);
        m_hud->drawPanel(&m_showHudEditor, m_view, playTestActive());
    }
    else
        claimViewMouse(false);
    if (m_showAbout)
        drawAbout();
    if (m_showCatalogue)
        drawCatalogue();
    if (m_showShortcuts)
        drawShortcuts();
    if (m_showOptions)
        drawOptions();
    drawPackPopup();
    drawLightPresetPopup();
    if (m_showLightSetups && m_session)
        drawLightSetups();
    drawSaveAsPopup();
    drawNewLevelPopup();
    drawLightmapsPopup();
    drawRecordPopup();
    if (m_showDemo)
        ImGui::ShowDemoWindow(&m_showDemo);
}

void EditorShell::loadSettings(const EditorSettings& settings)
{
    installShortcutKeys();
    m_options.rememberCamera = settings.getBool("options.rememberCamera", m_options.rememberCamera);
    m_options.viewDistance = settings.getFloat("options.viewDistance", m_options.viewDistance);
    m_options.startupCommand = settings.get("options.startupCommand", m_options.startupCommand);
    m_options.rawDataPath = settings.get("options.rawDataPath", m_options.rawDataPath);
    m_options.packFolder = settings.get("options.packFolder", m_options.packFolder);
    m_options.style = settings.getInt("options.style", m_options.style);
    m_options.logToFile = settings.getBool("options.logToFile", m_options.logToFile);
    m_options.mouseSensitivity = settings.getFloat("options.mouseSensitivity", m_options.mouseSensitivity);
    m_options.pauseOnFocusLoss = settings.getBool("options.pauseOnFocusLoss", m_options.pauseOnFocusLoss);
    m_options.masterVolume = settings.getInt("options.masterVolume", m_options.masterVolume);
    m_options.soundOn = settings.getBool("options.soundOn", m_options.soundOn);
    m_options.previewSave = settings.getBool("options.previewSave", m_options.previewSave);
    m_options.backupsKept = settings.getInt("options.backupsKept", m_options.backupsKept);
    std::snprintf(m_startupBuffer, sizeof(m_startupBuffer), "%s", m_options.startupCommand.c_str());
    std::snprintf(m_rawDataBuffer, sizeof(m_rawDataBuffer), "%s", m_options.rawDataPath.c_str());
    std::snprintf(m_packBuffer, sizeof(m_packBuffer), "%s", m_options.packFolder.c_str());
    for (int i = 0; i < 3; ++i)
    {
        char key[48];
        std::snprintf(key, sizeof(key), "options.customCommand%d", i + 1);
        m_options.customCommands[i] = settings.get(key, m_options.customCommands[i]);
        std::snprintf(key, sizeof(key), "options.customFile%d", i + 1);
        m_options.customFiles[i] = settings.get(key, m_options.customFiles[i]);
        std::snprintf(m_customBuffers[i], sizeof(m_customBuffers[i]), "%s", m_options.customCommands[i].c_str());
        std::snprintf(m_customBuffers[i + 3], sizeof(m_customBuffers[i + 3]), "%s", m_options.customFiles[i].c_str());
    }
    applyStyle();
    m_showLog = settings.getBool("panels.log", m_showLog);
    m_showEngine = settings.getBool("panels.engine", m_showEngine);
    m_showResources = settings.getBool("panels.resources", m_showResources);
    m_showProfiler = settings.getBool("panels.profiler", m_showProfiler);
    m_showMinimap = settings.getBool("panels.minimap", m_showMinimap);
    m_showHudEditor = settings.getBool("panels.hudEditor", m_showHudEditor);
    m_panels.loadSettings(settings);
    m_resources.loadSettings(settings);
    shortcuts().load(settings);
}

void EditorShell::saveSettings(EditorSettings& settings) const
{
    settings.setBool("panels.log", m_showLog);
    settings.setBool("panels.engine", m_showEngine);
    settings.setBool("panels.resources", m_showResources);
    settings.setBool("panels.profiler", m_showProfiler);
    settings.setBool("panels.minimap", m_showMinimap);
    settings.setBool("panels.hudEditor", m_showHudEditor);
    m_panels.saveSettings(settings);
    m_resources.saveSettings(settings);
    shortcuts().save(settings);
    settings.setBool("options.rememberCamera", m_options.rememberCamera);
    settings.setFloat("options.viewDistance", m_options.viewDistance);
    settings.set("options.startupCommand", m_options.startupCommand);
    settings.set("options.rawDataPath", m_options.rawDataPath);
    settings.set("options.packFolder", m_options.packFolder);
    settings.setInt("options.style", m_options.style);
    settings.setBool("options.logToFile", m_options.logToFile);
    settings.setFloat("options.mouseSensitivity", m_options.mouseSensitivity);
    settings.setBool("options.pauseOnFocusLoss", m_options.pauseOnFocusLoss);
    settings.setInt("options.masterVolume", m_options.masterVolume);
    settings.setBool("options.soundOn", m_options.soundOn);
    settings.setBool("options.previewSave", m_options.previewSave);
    settings.setInt("options.backupsKept", m_options.backupsKept);
    for (int i = 0; i < 3; ++i)
    {
        char key[48];
        std::snprintf(key, sizeof(key), "options.customCommand%d", i + 1);
        settings.set(key, m_options.customCommands[i]);
        std::snprintf(key, sizeof(key), "options.customFile%d", i + 1);
        settings.set(key, m_options.customFiles[i]);
    }
}

bool EditorShell::toolsAction(const std::string& action, int index, const std::string& argument, std::string& result)
{
    if (action == "list")
    {
        result.clear();
        for (int i = 0; i < 3; ++i)
        {
            char text[48];
            std::snprintf(text, sizeof(text), "%scommand %d: ", i ? "; " : "", i + 1);
            result += text + (m_options.customCommands[i].empty() ? std::string("(none)") : m_options.customCommands[i]);
            std::snprintf(text, sizeof(text), "; script %d: ", i + 1);
            result += text + (m_options.customFiles[i].empty() ? std::string("(none)") : m_options.customFiles[i]);
        }
        return true;
    }
    if (index < 1 || index > 3)
    {
        result = "the custom commands are numbered 1 to 3";
        return false;
    }
    const size_t slot = static_cast<size_t>(index - 1);
    char name[48];
    if (action == "setCommand" || action == "setFile")
    {
        const bool file = action == "setFile";
        std::string& field = file ? m_options.customFiles[slot] : m_options.customCommands[slot];
        field = argument;
        char* buffer = m_customBuffers[slot + (file ? 3 : 0)];
        std::snprintf(buffer, sizeof(m_customBuffers[0]), "%s", argument.c_str());
        std::snprintf(name, sizeof(name), "custom %s %d", file ? "script" : "command", index);
        result = std::string(name) + (argument.empty() ? " cleared" : ": " + argument);
        return true;
    }
    if (action == "runCommand")
    {
        const std::string& line = m_options.customCommands[slot];
        std::snprintf(name, sizeof(name), "custom command %d", index);
        if (line.empty())
        {
            result = std::string(name) + " is not set (Tools > Options)";
            return false;
        }
        const ConsoleLineResult r = runConsoleLine(line);
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "%s: %s -> %s", name, line.c_str(),
                r.output.empty() ? "done" : r.output.c_str());
        result = std::string(name) + ": " + line + " -> " + (r.output.empty() ? std::string("done") : r.output);
        return !r.failed;
    }
    if (action == "runFile")
    {
        const std::string& path = m_options.customFiles[slot];
        std::snprintf(name, sizeof(name), "custom script %d", index);
        std::string text, error;
        if (path.empty())
        {
            result = std::string(name) + " is not set (Tools > Options)";
            return false;
        }
        if (!ObjectLibrary::readTextFile(path, text, error))
        {
            result = std::string(name) + ": " + error;
            return false;
        }
        // Every non-empty line in order, each down the Console panel's path; a failing line does not stop the
        // rest, as a console script runs on.
        unsigned lines = 0, failed = 0;
        size_t start = 0;
        while (start < text.size())
        {
            size_t end = text.find_first_of("\r\n", start);
            if (end == std::string::npos)
                end = text.size();
            const std::string line = text.substr(start, end - start);
            start = end + 1;
            if (line.find_first_not_of(" \t") == std::string::npos)
                continue;
            const ConsoleLineResult r = runConsoleLine(line);
            ++lines;
            if (r.failed)
                ++failed;
            logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "%s: %s -> %s", name, line.c_str(),
                    r.output.empty() ? "done" : r.output.c_str());
        }
        char counts[96];
        std::snprintf(counts, sizeof(counts), ": %u line%s run, %u failed", lines, lines == 1 ? "" : "s", failed);
        result = std::string(name) + " (" + path + ")" + counts;
        return failed == 0;
    }
    result = "no tools action '" + action + "'";
    return false;
}

EditorShell::CloseChoice EditorShell::drawClosePrompt(bool ask, const std::string& summary)
{
    const char* const title = "Unsaved changes";
    if (!ask)
        return CloseChoice::None;
    if (!ImGui::IsPopupOpen(title))
        ImGui::OpenPopup(title);
    CloseChoice choice = CloseChoice::None;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(title, 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("The level has changes that are not saved.");
        ImGui::TextDisabled("%s", summary.c_str());
        ImGui::Separator();
        if (ImGui::Button("Save and close"))
            choice = CloseChoice::Save;
        ImGui::SameLine();
        if (ImGui::Button("Close without saving"))
            choice = CloseChoice::Discard;
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            choice = CloseChoice::Cancel;
        if (choice != CloseChoice::None)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return choice;
}

namespace
{

// A panel's name as a command gives it ("GameplayLayers") against its window title ("Gameplay layers").
bool samePanel(const std::string& a, const std::string& b)
{
    std::string x, y;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != ' ')
            x += static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
    for (size_t i = 0; i < b.size(); ++i)
        if (b[i] != ' ')
            y += static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
    return x == y;
}

} // namespace

bool EditorShell::setPanelVisible(const std::string& name, bool visible)
{
    struct Panel { const char* name; bool* shown; };
    const Panel panels[] = {
        { "Log", &m_showLog }, { "Engine", &m_showEngine }, { "Resources", &m_showResources },
        { "Profiler", &m_showProfiler }, { "Minimap", &m_showMinimap }, { "Keyboard shortcuts", &m_showShortcuts },
        { "HUD editor", &m_showHudEditor },
        { "Options", &m_showOptions },
    };
    if (_stricmp(name.c_str(), "all") == 0)
    {
        if (!visible)
        {
            // Remember what was open, then close it all; a second hide changes nothing.
            if (!m_hiddenForFullView.empty())
                return true;
            for (size_t i = 0; i < sizeof(panels) / sizeof(panels[0]); ++i)
                if (*panels[i].shown)
                {
                    m_hiddenForFullView.push_back(panels[i].name);
                    *panels[i].shown = false;
                }
            const std::vector<std::string> session = m_panels.openPanels();
            for (size_t i = 0; i < session.size(); ++i)
            {
                m_hiddenForFullView.push_back(session[i]);
                m_panels.setPanelVisible(session[i], false);
            }
            if (m_hiddenForFullView.empty())
                m_hiddenForFullView.push_back("-");     // nothing was open: still a full view to leave
            logLine(LogLevel::Info, "view: full view (F11 brings the panels back)");
            return true;
        }
        for (size_t i = 0; i < m_hiddenForFullView.size(); ++i)
            if (m_hiddenForFullView[i] != "-")
                setPanelVisible(m_hiddenForFullView[i], true);
        m_hiddenForFullView.clear();
        return true;
    }
    for (size_t i = 0; i < sizeof(panels) / sizeof(panels[0]); ++i)
    {
        if (samePanel(panels[i].name, name))
        {
            *panels[i].shown = visible;
            if (visible)
                m_focusRequest = panels[i].name;
            return true;
        }
    }
    if (!m_panels.setPanelVisible(name, visible))
        return false;
    if (visible)
    {
        const std::vector<std::string> open = m_panels.openPanels();
        for (size_t i = 0; i < open.size(); ++i)
            if (samePanel(open[i], name))
                m_focusRequest = open[i];
    }
    return true;
}

void EditorShell::drawMenu()
{
    if (!ImGui::BeginMainMenuBar())
        return;
    if (ImGui::BeginMenu("File"))
    {
        if (m_session)
        {
            if (ImGui::MenuItem("Save", shortcuts().display("file.save").c_str()))
                m_session->runSave();
            ImGui::SetItemTooltip(m_options.previewSave ? "Shows every file the save writes, then writes them all or none."
                                                        : "Writes every change of the level, all of it or none.");
            if (ImGui::MenuItem("Preview the save..."))
                m_savePreview.requestPreview();
            ImGui::SetItemTooltip("Every file a save would write, with what changes in it; nothing is written until you "
                                  "say so.");
            if (ImGui::MenuItem("Backups..."))
                m_savePreview.openBackups();
            ImGui::SetItemTooltip("What each save replaced, kept per level; the level put back to before any save.");
            if (ImGui::MenuItem("Resave every level file"))
                m_session->runCommand("EditorLevel.resave", "save");
            ImGui::SetItemTooltip("Writes every object and settings file of the level, changed or not.");
            if (ImGui::MenuItem("New level..."))
            {
                m_openNewLevel = true;
                m_newLevelResult.clear();
            }
            if (ImGui::MenuItem("Save level as..."))
            {
                m_openSaveAs = true;
                m_saveAsResult.clear();
            }
            if (ImGui::MenuItem("Pack the level for the game..."))
            {
                m_openPack = true;
                m_packResult.clear();
            }
            drawLayerMenu();
            // Another level of the mod: the editor starts again on it (the retail File > Load).
            if (ImGui::BeginMenu("Open another level"))
            {
                std::string levels;
                m_session->hostTool("levels", "", levels);
                size_t start = 0;
                while (start < levels.size())
                {
                    size_t end = levels.find(", ", start);
                    if (end == std::string::npos)
                        end = levels.size();
                    const std::string name = levels.substr(start, end - start);
                    start = end + 2;
                    if (!name.empty() && ImGui::MenuItem(name.c_str()))
                        m_session->runCommand("EditorLevel.open " + name, "level");
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem("Write the object and material lists"))
                m_session->runCommand("EditorLevel.saveLists", "level");
            ImGui::SetItemTooltip("Editor/ObjectList.txt and Editor/MaterialList.txt in the level's folder.");
            ImGui::Separator();
        }
        if (ImGui::MenuItem("Exit", "Alt+F4"))
            m_quit = true;
        ImGui::EndMenu();
    }
    if (m_session)
    {
        m_panels.editMenu(*m_session, ResourceBrowser::templateOf(m_resources.selected()));
        drawCameraMenu();
    }
    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem("Resources", 0, &m_showResources);
        ImGui::MenuItem("Log", 0, &m_showLog);
        ImGui::MenuItem("Engine", 0, &m_showEngine);
        if (m_session)
            m_panels.viewMenuItems();
        ImGui::MenuItem("Profiler", 0, &m_showProfiler);
        if (m_session)
            ImGui::MenuItem("Minimap", 0, &m_showMinimap);
        if (m_hud)
            ImGui::MenuItem("HUD editor", 0, &m_showHudEditor);
        else if (m_session)
        {
            ImGui::MenuItem("HUD editor", 0, false, false);
            ImGui::SetItemTooltip("The game's HUD is not loaded (the editor was started with +hud 0, or the HUD did not start - see the log).");
        }
        ImGui::Separator();
        bool paused = m_renderPaused;
        if (ImGui::MenuItem("Pause the renderer", 0, &paused, m_session != 0))
            setRenderPaused(paused);
        ImGui::MenuItem("Pause the game update", 0, &m_updatePaused, m_session != 0);
        ImGui::SetItemTooltip("Effects and animations stop; the view keeps drawing and edits still show.");
        ImGui::MenuItem("The level's view distance", 0, &m_levelViewDistance, m_session != 0);
        ImGui::MenuItem("Wireframe", 0, &m_wireframe, m_session != 0);
        ImGui::SetItemTooltip("The world drawn as lines (the editor's panels stay solid).");
        ImGui::SetItemTooltip("The camera draws as far as the level's own view distance (Init.con) instead of Options'.");
        if (m_session && ImGui::BeginMenu("Snap"))
        {
            // What a free move (the gizmo's centre slid over the ground) does.
            const EditSession::SnapModes& snap = m_session->snapModes();
            if (ImGui::MenuItem("Onto the ground", 0, snap.ground))
                m_session->runCommand(snap.ground ? "EditorSnap.ground 0" : "EditorSnap.ground 1", "snap");
            if (ImGui::MenuItem("Onto other objects", 0, snap.objects))
                m_session->runCommand(snap.objects ? "EditorSnap.objects 0" : "EditorSnap.objects 1", "snap");
            ImGui::Separator();
            if (ImGui::MenuItem("No grid", 0, snap.grid == 0))
                m_session->runCommand("EditorSnap.grid 0", "snap");
            if (ImGui::MenuItem("The heightmap's grid", 0, snap.grid == 1))
                m_session->runCommand("EditorSnap.grid 1", "snap");
            if (ImGui::MenuItem("Half the heightmap's grid", 0, snap.grid == 2))
                m_session->runCommand("EditorSnap.grid 2", "snap");
            ImGui::EndMenu();
        }
        if (m_session && ImGui::BeginMenu("Overlays"))
        {
            // What the view draws besides the level.
            const EditSession::OverlayOptions& o = m_session->overlays();
            const struct { const char* label; const char* name; bool on; } flags[] = {
                { "Control points", "controlPoints", o.controlPoints },
                { "Spawn points", "spawnPoints", o.spawnPoints },
                { "Object spawners", "spawners", o.spawners },
                { "Triggers and effect areas", "ambient", o.ambient },
                { "Lines to each one's control point", "connections", o.connections },
                { "Control point radius", "controlPointRadius", o.controlPointRadius },
                { "Supply lines and groups", "supplyLines", o.supplyLines },
                { "Sea level", "seaLevel", o.seaLevel },
                { "Bounding spheres of the selection", "boundingSpheres", o.boundingSpheres },
                { "Light sources", "lightSources", o.lightSources },
                { "Selection boxes", "selectionBoxes", o.selectionBoxes },
                { "Box round the object under the cursor", "hoverBox", o.hoverBox },
            };
            for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
            {
                if (i == 4 || i == 7 || i == 10)
                    ImGui::Separator();
                if (ImGui::MenuItem(flags[i].label, 0, flags[i].on))
                    m_session->runCommand(std::string("EditorView.overlay ") + flags[i].name + (flags[i].on ? " 0" : " 1"), "overlay");
            }
            ImGui::Separator();
            static const char* const sizes[] = { "Sound markers: off", "Sound markers: small", "Sound markers: medium",
                                                 "Sound markers: large" };
            for (int i = 0; i < 4; ++i)
                if (ImGui::MenuItem(sizes[i], 0, o.soundMarkers == i))
                {
                    char line[64];
                    std::snprintf(line, sizeof(line), "EditorView.overlay soundMarkers %d", i);
                    m_session->runCommand(line, "overlay");
                }
            static const char* const reach[] = { "Sound reach: none", "Sound reach: the selected sound's",
                                                 "Sound reach: every sound's" };
            for (int i = 0; i < 3; ++i)
                if (ImGui::MenuItem(reach[i], 0, o.soundReach == i))
                {
                    char line[64];
                    std::snprintf(line, sizeof(line), "EditorView.overlay soundReach %d", i);
                    m_session->runCommand(line, "overlay");
                }
            if (ImGui::MenuItem("Sound reach on the ground", 0, o.soundReachOnGround))
                m_session->runCommand(o.soundReachOnGround ? "EditorView.overlay soundReachOnGround 0"
                                                           : "EditorView.overlay soundReachOnGround 1", "overlay");
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Render one frame", 0, false, m_session != 0 && m_renderPaused))
            m_renderOneFrame = true;
        ImGui::Separator();
        ImGui::MenuItem("ImGui demo", 0, &m_showDemo);
        ImGui::EndMenu();
    }
    // The retail editor's Compile menu: the bakes at its dialogs' defaults (App/EditSessionLighting.cpp).
    if (ImGui::BeginMenu("Compile"))
    {
        if (ImGui::MenuItem("Make ground hemi (1024, 0/200/0, 2048)", 0, false, m_session != 0))
            m_session->runCommand("EditorLighting.groundHemi 1024 0/200/0 2048", "lighting");
        if (ImGui::MenuItem("Capture the minimap picture", 0, false, m_session != 0))
            m_session->runCommand("EditorLighting.minimap 512", "lighting");
        if (ImGui::MenuItem("Generate environment maps", 0, false, m_session != 0))
            m_session->runCommand("EditorLighting.envMaps", "lighting");
        if (ImGui::MenuItem("Generate lightmaps...", 0, false, m_session != 0))
        {
            m_openLightmaps = true;
            m_lightmapResult.clear();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Find unused object lightmaps", 0, false, m_session != 0))
            m_session->runCommand("EditorLighting.cleanLightmaps 0", "lighting");
        if (ImGui::MenuItem("Move unused object lightmaps aside", 0, false, m_session != 0))
            m_session->runCommand("EditorLighting.cleanLightmaps 1", "lighting");
        ImGui::SetItemTooltip("Into Lightmaps/Objects/Unused; nothing is deleted.");
        ImGui::EndMenu();
    }
    drawLightMenu();
    if (ImGui::BeginMenu("Tools"))
    {
        ImGui::MenuItem("Options", 0, &m_showOptions);
        ImGui::MenuItem("Keyboard shortcuts", 0, &m_showShortcuts);
        // The measure tool.
        const bool measuring = m_session && m_session->measureToolActive();
        if (ImGui::MenuItem("Measure", 0, measuring, m_session != 0))
            m_session->runCommand(measuring ? "EditorMeasure.tool 0" : "EditorMeasure.tool 1", "measure");
        ImGui::SetItemTooltip("Click the ground to add points; Backspace takes the last off, Enter gives the length.");
        // Record the view.
        ImGui::Separator();
        if (!viewRecorder().recording())
        {
            if (ImGui::MenuItem("Record the view...", 0, false, m_session != 0))
            {
                SYSTEMTIME now;
                GetLocalTime(&now);
                std::snprintf(m_recordPath, sizeof(m_recordPath), "BF2Editor_view_%04u%02u%02u_%02u%02u%02u.mp4",
                              now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);
                m_openRecord = true;
            }
        }
        else
        {
            char label[96];
            std::snprintf(label, sizeof(label), "Stop recording (%u frames)", viewRecorder().frames());
            if (ImGui::MenuItem(label))
                m_session->runCommand("EditorView.stopRecording", "record");
        }
        // The retail host's custom commands: a console line and a script each, set in Options.
        ImGui::Separator();
        for (int i = 0; i < 3; ++i)
        {
            char label[600];
            const std::string& line = m_options.customCommands[i];
            std::snprintf(label, sizeof(label), "Custom command %d%s%s", i + 1, line.empty() ? "" : ": ", line.c_str());
            if (ImGui::MenuItem(label, 0, false, !line.empty()))
            {
                char command[48];
                std::snprintf(command, sizeof(command), "EditorTools.runCustomCommand %d", i + 1);
                const ConsoleLineResult r = runConsoleLine(command);
                logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "tools: %s", r.output.c_str());
            }
        }
        for (int i = 0; i < 3; ++i)
        {
            char label[600];
            const std::string& file = m_options.customFiles[i];
            std::snprintf(label, sizeof(label), "Custom script %d%s%s", i + 1, file.empty() ? "" : ": ", file.c_str());
            if (ImGui::MenuItem(label, 0, false, !file.empty()))
            {
                char command[48];
                std::snprintf(command, sizeof(command), "EditorTools.runCustomFile %d", i + 1);
                const ConsoleLineResult r = runConsoleLine(command);
                logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "tools: %s", r.output.c_str());
            }
        }
        ImGui::EndMenu();
    }
    drawHelpMenu();
    ImGui::EndMainMenuBar();
}

void EditorShell::drawRecordPopup()
{
    if (m_openRecord)
    {
        ImGui::OpenPopup("Record the view");
        m_openRecord = false;
    }
    if (!ImGui::BeginPopupModal("Record the view", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextUnformatted("The world as drawn (without the editor's panels), as H.264 video, until you stop it.");
    ImGui::SetNextItemWidth(420.0f);
    ImGui::InputText("File (.mp4)", m_recordPath, sizeof(m_recordPath));
    ImGui::TextDisabled("A relative path is under the game folder the editor runs in.");
    if (ImGui::Button("Record") && m_session)
    {
        m_session->runCommand(std::string("EditorView.record \"") + m_recordPath + "\"", "record");
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorShell::drawHelpMenu()
{
    if (!ImGui::BeginMenu("Help"))
        return;
    // The retail editor's own documents (its bf2editor/Help folder in the user's install), opened through the
    // shell as retail opened them.
    const std::vector<HelpDocument> documents = helpDocuments(kHelpFolder);
    if (documents.empty())
        ImGui::TextDisabled("no retail help documents in bf2editor/Help");
    const char* const groups[] = { "", "Workshop: ", "Tutorial: " };
    const char* const menus[] = { "", "Workshops", "Tutorials" };
    for (int g = 0; g < 3; ++g)
    {
        bool open = g == 0;
        bool any = false;
        for (size_t i = 0; i < documents.size(); ++i)
        {
            const std::string& label = documents[i].label;
            const bool inGroup = g == 0 ? label.find(": ") == std::string::npos : label.compare(0, std::strlen(groups[g]), groups[g]) == 0;
            if (!inGroup)
                continue;
            if (!any && g != 0)
                open = ImGui::BeginMenu(menus[g]);
            any = true;
            if (!open)
                break;
            const std::string shown = g == 0 ? label : label.substr(std::strlen(groups[g]));
            if (ImGui::MenuItem(shown.c_str()))
            {
                std::string result;
                const bool opened = openHelpDocument(documents[i], result);
                logLine(opened ? LogLevel::Info : LogLevel::Warning, "help: %s", result.c_str());
            }
        }
        if (any && g != 0 && open)
            ImGui::EndMenu();
    }
    ImGui::Separator();
    ImGui::MenuItem("Retail command catalogue", 0, &m_showCatalogue);
    ImGui::SetItemTooltip("The retail editor's commands and what each did (bf2editor/Help/CommandDescriptions.dat).");
    ImGui::Separator();
    ImGui::MenuItem("About BF2Editor", 0, &m_showAbout);
    ImGui::EndMenu();
}

void EditorShell::drawCatalogue()
{
    static CommandCatalogue catalogue;
    static std::string error;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        catalogue.load(std::string(kHelpFolder) + "/CommandDescriptions.dat", error);
    }
    ImGui::SetNextWindowSize(ImVec2(760.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Retail command catalogue", &m_showCatalogue))
    {
        ImGui::End();
        return;
    }
    if (!catalogue.size())
    {
        ImGui::TextWrapped("The catalogue could not be read: %s", error.c_str());
        ImGui::End();
        return;
    }
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##filter", "filter (name or description)", m_catalogueFilter, sizeof(m_catalogueFilter));
    std::string filter(m_catalogueFilter);
    for (size_t i = 0; i < filter.size(); ++i)
        filter[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(filter[i])));
    std::vector<size_t> rows;
    for (size_t i = 0; i < catalogue.size(); ++i)
    {
        if (filter.empty())
        {
            rows.push_back(i);
            continue;
        }
        std::string text = catalogue.name(i) + " " + catalogue.description(i);
        for (size_t k = 0; k < text.size(); ++k)
            text[k] = static_cast<char>(std::tolower(static_cast<unsigned char>(text[k])));
        if (text.find(filter) != std::string::npos)
            rows.push_back(i);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%u of %u commands; the rebuilt editor's own are listed by Tab in the Console panel",
                        static_cast<unsigned>(rows.size()), static_cast<unsigned>(catalogue.size()));
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("catalogue", 2, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Command", ImGuiTableColumnFlags_WidthFixed, 300.0f);
        ImGui::TableSetupColumn("What it did", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(rows.size()));
        while (clipper.Step())
        {
            for (int r = clipper.DisplayStart; r < clipper.DisplayEnd; ++r)
            {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(catalogue.name(rows[static_cast<size_t>(r)]).c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(catalogue.description(rows[static_cast<size_t>(r)]).c_str());
            }
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void EditorShell::setRenderPaused(bool paused)
{
    if (paused == m_renderPaused)
        return;
    m_renderPaused = paused;
    m_renderOneFrame = false;
    logLine(LogLevel::Info, "renderer: %s", paused ? "paused (the view keeps its last frame; View > Render one frame "
                                                     "draws one more)"
                                                   : "running");
}

void EditorShell::applyStyle() const
{
    if (!ImGui::GetCurrentContext())
        return;
    if (m_options.style == 1)
        ImGui::StyleColorsLight();
    else if (m_options.style == 2)
        ImGui::StyleColorsClassic();
    else
        ImGui::StyleColorsDark();
}

void EditorShell::drawOptions()
{
    ImGui::SetNextWindowSize(ImVec2(560.0f, 300.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Options", &m_showOptions))
    {
        ImGui::End();
        return;
    }
    ImGui::SeparatorText("Camera");
    ImGui::Checkbox("Open each level where its camera was left", &m_options.rememberCamera);
    ImGui::SetNextItemWidth(200.0f);
    ImGui::DragFloat("View distance", &m_options.viewDistance, 10.0f, 100.0f, 20000.0f, "%.0f m", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("How far the editor's camera draws (its far plane). The level's own view distance is a "
                          "level setting.");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderFloat("Mouse look sensitivity", &m_options.mouseSensitivity, 0.1f, 5.0f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
    ImGui::SeparatorText("Running");
    ImGui::Checkbox("Pause while another window has the focus", &m_options.pauseOnFocusLoss);
    // The retail host's volume popup (H9): kept, and used once the editor plays sound.
    ImGui::SetNextItemWidth(160.0f);
    ImGui::BeginDisabled(!m_options.soundOn);
    ImGui::SliderInt("Master volume", &m_options.masterVolume, 0, 100, "%d%%");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("Sound on", &m_options.soundOn);
    ImGui::TextDisabled("The editor plays no sound yet (it starts the engine with +noSound 1); the volume is kept for it.");
    ImGui::SetItemTooltip("The world and the view stand still until the editor's window is in front again.");
    ImGui::Checkbox("Write the log to BF2Editor.log beside the editor", &m_options.logToFile);
    ImGui::SeparatorText("Saving");
    ImGui::Checkbox("Show what a save writes before writing it (File > Save, Ctrl+S)", &m_options.previewSave);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::DragInt("Backups kept per level", &m_options.backupsKept, 0.2f, 0, 500, m_options.backupsKept ? "%d" : "every one",
                   ImGuiSliderFlags_AlwaysClamp);
    ImGui::SetItemTooltip("Each save keeps what it replaced in bf2editor/Backups/<mod>/<level>; the oldest go once there "
                          "are more. 0 keeps every one.");
    if (m_session && ImGui::Button("Backups..."))
        m_savePreview.openBackups();
    ImGui::SeparatorText("Start-up");
    ImGui::SetNextItemWidth(-160.0f);
    if (ImGui::InputTextWithHint("Start-up command", "a console line, e.g. EditorView.panel Terrain 1", m_startupBuffer,
                                 sizeof(m_startupBuffer)))
        m_options.startupCommand = m_startupBuffer;
    ImGui::SetItemTooltip("Run once a level has opened (the log shows its answer).");
    ImGui::SeparatorText("Files");
    ImGui::SetNextItemWidth(-160.0f);
    if (ImGui::InputTextWithHint("RawData folder", "where imports start looking", m_rawDataBuffer, sizeof(m_rawDataBuffer)))
        m_options.rawDataPath = m_rawDataBuffer;
    ImGui::SetNextItemWidth(-160.0f);
    if (ImGui::InputTextWithHint("Pack into", "<game>/mods/<mod>, for File > Pack the level", m_packBuffer, sizeof(m_packBuffer)))
        m_options.packFolder = m_packBuffer;
    ImGui::SeparatorText("Custom commands (Tools menu)");
    for (int i = 0; i < 3; ++i)
    {
        char label[48];
        std::snprintf(label, sizeof(label), "Command %d", i + 1);
        ImGui::SetNextItemWidth(-160.0f);
        if (ImGui::InputTextWithHint(label, "a console line", m_customBuffers[i], sizeof(m_customBuffers[i])))
            m_options.customCommands[i] = m_customBuffers[i];
    }
    for (int i = 0; i < 3; ++i)
    {
        char label[48];
        std::snprintf(label, sizeof(label), "Script %d", i + 1);
        ImGui::SetNextItemWidth(-160.0f);
        if (ImGui::InputTextWithHint(label, "a file of console lines (from the game folder, or a full path)",
                                     m_customBuffers[i + 3], sizeof(m_customBuffers[i + 3])))
            m_options.customFiles[i] = m_customBuffers[i + 3];
    }
    ImGui::SeparatorText("Look");
    const char* const styles[] = { "Dark", "Light", "Classic" };
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::Combo("Colours", &m_options.style, styles, 3))
        applyStyle();
    ImGui::TextDisabled("Kept in BF2Editor.settings.ini when the editor closes.");
    ImGui::End();
}

void EditorShell::drawLightMenu()
{
    if (!ImGui::BeginMenu("Light"))
        return;
    const bool level = m_session && m_session->document() && m_session->document()->isOpen();
    if (ImGui::BeginMenu("Light presets", level))
    {
        const std::vector<std::string> presets = EditSession::lightPresetNames();
        if (presets.empty())
            ImGui::TextDisabled("none in bf2editor/Content/LightPreset");
        for (size_t i = 0; i < presets.size(); ++i)
            if (ImGui::MenuItem(presets[i].c_str()))
                m_session->runCommand("EditorLight.apply " + presets[i], "light");
        ImGui::EndMenu();
    }
    ImGui::SetItemTooltip("A preset's light settings become the level's (one undoable edit; File > Save writes Sky.con).");
    if (ImGui::BeginMenu("Another level's light", level))
    {
        const std::vector<std::string> levels = m_session->lightLevelNames();
        if (levels.empty())
            ImGui::TextDisabled("no level of the mod has a Sky.con");
        for (size_t i = 0; i < levels.size(); ++i)
            if (ImGui::MenuItem(levels[i].c_str()))
                m_session->runCommand("EditorLight.applyLevel " + levels[i], "light");
        ImGui::EndMenu();
    }
    ImGui::SetItemTooltip("That level's Sky.con light, fog and sky settings become this level's (one undoable edit).");
    if (ImGui::MenuItem("Save the light as a preset...", 0, false, level))
        m_openLightPreset = true;
    if (ImGui::MenuItem("Back to the level's saved light", 0, false, level))
        m_session->runCommand("EditorLight.resetToLevel", "light");
    ImGui::MenuItem("Lightmap light setups", 0, &m_showLightSetups, level);
    ImGui::SetItemTooltip("The object and terrain setups the lightmap generator traces with (Editor/ObjectLight.con,\n"
                          "Editor/TerrainLight.con).");
    ImGui::Separator();
    if (ImGui::MenuItem("Sun along the view", 0, false, level))
        m_session->runCommand("EditorLight.sunFromCamera 1", "light");
    if (ImGui::MenuItem("Sun towards the camera", 0, false, level))
        m_session->runCommand("EditorLight.sunFromCamera -1", "light");
    if (ImGui::MenuItem("Sun flare along the view", 0, false, level))
        m_session->runCommand("EditorLight.flareFromCamera 1", "light");
    if (ImGui::MenuItem("Sun flare towards the camera", 0, false, level))
        m_session->runCommand("EditorLight.flareFromCamera -1", "light");
    ImGui::EndMenu();
}

void EditorShell::drawLightSetups()
{
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Lightmap light setups", &m_showLightSetups))
    {
        ImGui::End();
        return;
    }
    // The current setup as the session answers it: "<mode> light setup (<file>): <setting> <value>, ...".
    std::string status;
    m_session->lightSetupCommand("lightSetup", std::string(), status);
    int mode = m_session->lightSetupMode();
    if (ImGui::RadioButton("Object setup", mode == 0) && mode != 0)
        m_session->runCommand("EditorLighting.lightSetupMode object", "light setup");
    ImGui::SameLine();
    if (ImGui::RadioButton("Terrain setup", mode == 1) && mode != 1)
        m_session->runCommand("EditorLighting.lightSetupMode terrain", "light setup");
    ImGui::SetItemTooltip("A switch saves the setup left when it changed, as the retail editor's did.");
    const std::vector<std::string>& names = LightSetup::settingNames();
    if (ImGui::BeginTable("##lightSetup", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        for (size_t i = 0; i < names.size(); ++i)
        {
            // The value: the text after "<name> " up to the next ", " in the answer.
            std::string value;
            const size_t at = status.find(" " + names[i] + " ");
            if (at != std::string::npos)
            {
                const size_t from = at + names[i].size() + 2;
                const size_t end = status.find_first_of(",;", from);
                value = status.substr(from, end == std::string::npos ? std::string::npos : end - from);
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(names[i].c_str());
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(i));
            ImGui::SetNextItemWidth(-1.0f);
            if (m_lightSetupEditing != names[i])
            {
                char shown[64];
                std::snprintf(shown, sizeof(shown), "%s", value.c_str());
                ImGui::InputText("##v", shown, sizeof(shown));
                if (ImGui::IsItemActivated())
                {
                    m_lightSetupEditing = names[i];
                    std::snprintf(m_lightSetupValue, sizeof(m_lightSetupValue), "%s", value.c_str());
                }
            }
            else if (ImGui::InputText("##v", m_lightSetupValue, sizeof(m_lightSetupValue), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                m_session->runCommand("EditorLighting.lightSetupSet " + names[i] + " " + m_lightSetupValue, "light setup");
                m_lightSetupEditing.clear();
            }
            else if (ImGui::IsItemDeactivated())
                m_lightSetupEditing.clear();
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (ImGui::Button("Save the current setup"))
        m_session->runCommand("EditorLighting.lightSetupSave", "light setup");
    ImGui::TextDisabled("They take effect with lightmap generation, which 1.50 does not run yet.");
    ImGui::End();
}

void EditorShell::drawLightPresetPopup()
{
    if (m_openLightPreset)
    {
        ImGui::OpenPopup("Save the light as a preset");
        m_openLightPreset = false;
        m_lightPresetResult.clear();
    }
    if (!ImGui::BeginPopupModal("Save the light as a preset", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextWrapped("The level's light, fog and sky settings, as a preset in bf2editor/Content/LightPreset (the retail "
                       "editor reads them too).");
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    const bool enter = ImGui::InputTextWithHint("Name", "letters, digits, - and _", m_lightPresetName,
                                                sizeof(m_lightPresetName), ImGuiInputTextFlags_EnterReturnsTrue);
    if (!m_lightPresetResult.empty())
        ImGui::TextWrapped("%s", m_lightPresetResult.c_str());
    if ((ImGui::Button("Save") || enter) && m_lightPresetName[0])
    {
        const ConsoleLineResult r = runConsoleLine(std::string("EditorLight.save ") + m_lightPresetName);
        m_lightPresetResult = r.output;
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "light: %s", r.output.c_str());
        if (!r.failed)
            ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorShell::drawSaveAsPopup()
{
    if (m_openSaveAs)
    {
        ImGui::OpenPopup("Save level as");
        m_openSaveAs = false;
    }
    if (!ImGui::BeginPopupModal("Save level as", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextWrapped("Copies the saved level to Levels/<new name>/, with every path in its files pointing at the copy. "
                       "Save your changes first.");
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    const bool enter = ImGui::InputTextWithHint("New name", "letters, digits and underscores", m_saveAsName,
                                                sizeof(m_saveAsName), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::Checkbox("Open the copy (the editor starts again on it)", &m_saveAsOpen);
    if (!m_saveAsResult.empty())
        ImGui::TextWrapped("%s", m_saveAsResult.c_str());
    if ((ImGui::Button("Save as") || enter) && m_saveAsName[0])
    {
        const ConsoleLineResult r = runConsoleLine(std::string("EditorLevel.saveAs ") + m_saveAsName + (m_saveAsOpen ? " 1" : " 0"));
        m_saveAsResult = r.output;
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "save as: %s", r.output.c_str());
        if (!r.failed && m_saveAsOpen)
            ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorShell::drawNewLevelPopup()
{
    if (m_openNewLevel)
    {
        ImGui::OpenPopup("New level");
        m_openNewLevel = false;
    }
    if (!ImGui::BeginPopupModal("New level", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    static const int kSizes[] = { 256, 512, 1024 };
    static const int kScales[] = { 1, 2, 4 };
    static const int kLowRes[] = { 128, 256, 512, 1024 };
    static const char* const kSizeNames = "256\0" "512\0" "1024\0";
    static const char* const kScaleNames = "1\0" "2\0" "4\0";
    static const char* const kLowResNames = "128\0" "256\0" "512\0" "1024\0";
    ImGui::TextWrapped("Makes a level beside the open one, as the retail editor's Create new terrain did: flat "
                       "heightmaps at the default height, its default textures on every patch (from "
                       "bf2editor/Content/Terrain), the level's scripts and the compiled terrain.");
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    ImGui::InputTextWithHint("Name", "letters, digits and underscores", m_newLevelName, sizeof(m_newLevelName));
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("Size (cells per side)", &m_newLevelSize, kSizeNames);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("Scale (metres per cell)", &m_newLevelScale, kScaleNames);
    const int size = kSizes[m_newLevelSize], scale = kScales[m_newLevelScale];
    ImGui::Text("Level size: %d m", size * scale);
    ImGui::SetNextItemWidth(320.0f);
    ImGui::SliderInt("Default height (m)", &m_newLevelHeight, 0, 100);
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("Low-res size (the surrounding terrain)", &m_newLevelLowRes, kLowResNames);
    const int lowRes = kLowRes[m_newLevelLowRes];
    const int lowResScale = scale * (size / lowRes);
    if (lowResScale > 0)
        ImGui::Text("Low-res scale: %d m per cell", lowResScale);
    else
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "Low-res scale may not be 0: a smaller low-res size");
    ImGui::Checkbox("Open the new level (the editor starts again on it)", &m_newLevelOpen);
    if (!m_newLevelResult.empty())
        ImGui::TextWrapped("%s", m_newLevelResult.c_str());
    const bool named = validLevelName(m_newLevelName);
    if (m_newLevelName[0] && !named)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "The name takes letters, digits and underscores");
    ImGui::BeginDisabled(!named || lowResScale <= 0);
    if (ImGui::Button("Create"))
    {
        char line[200];
        std::snprintf(line, sizeof(line), "EditorLevel.create %s %d/%d/%d/%d %d", m_newLevelName, size, scale,
                      m_newLevelHeight, lowRes, m_newLevelOpen ? 1 : 0);
        const ConsoleLineResult r = runConsoleLine(line);
        m_newLevelResult = r.output;
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "new level: %s", r.output.c_str());
        if (!r.failed && m_newLevelOpen)
            ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorShell::drawLightmapsPopup()
{
    if (m_openLightmaps)
    {
        ImGui::OpenPopup("Generate lightmaps");
        m_openLightmaps = false;
    }
    if (!ImGui::BeginPopupModal("Generate lightmaps", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextWrapped("Traces lightmaps with the renderer's lightmap generator: the terrain's, lit by the level's "
                       "Editor/TerrainLight.con (the retail editor's terrain preset when the level has none), into "
                       "Lightmaps/ - the ring patches' into Editor/Lightmaps/ -, and the static objects', lit by "
                       "Editor/ObjectLight.con, into Lightmaps/Objects/ with their atlas. The editor is busy until the "
                       "trace ends.");
    ImGui::SeparatorText("Lightmap generation");
    ImGui::Checkbox("Trace terrain", &m_lightmapTerrain);
    ImGui::Checkbox("Trace objects", &m_lightmapObjects);
    ImGui::SetItemTooltip("Every static object, traced from its mesh's .samples file (bf2editor/LightmapSamples*.zip, "
                          "mounted at Objects), then the atlas the game reads.");
    ImGui::SameLine();
    ImGui::Checkbox("Selected only", &m_lightmapSelected);
    ImGui::SetItemTooltip("Only the selected objects. The atlas is packed again only when the level keeps its single "
                          "object lightmaps; a stock level has only its atlas.");
    ImGui::Checkbox("Only nearest", &m_lightmapNearest);
    ImGui::SetItemTooltip("Only the sample block nearest to the camera: a quick look at the settings.");
    ImGui::Checkbox("Primary", &m_lightmapPrimary);
    ImGui::SameLine();
    ImGui::Checkbox("Secondary (the ring patches)", &m_lightmapSecondary);
    ImGui::SeparatorText("Resolution");
    static const char* const kQualities[] = { "Low (0.25)", "Medium (0.5)", "Final (1)", "High (2)" };
    for (int i = 0; i < 4; ++i)
    {
        if (i)
            ImGui::SameLine();
        ImGui::RadioButton(kQualities[i], &m_lightmapQuality, i);
    }
    ImGui::SeparatorText("Resume options");
    const bool resumable = GetFileAttributesA("Logs/LightmapGeneration.log") != INVALID_FILE_ATTRIBUTES;
    ImGui::BeginDisabled(!resumable);
    ImGui::Checkbox(resumable ? "Resume previous (resume data available)" : "Resume previous (no resume data)",
                    &m_lightmapResume);
    ImGui::EndDisabled();
    const bool terrainParts = m_lightmapTerrain && (m_lightmapPrimary || m_lightmapSecondary);
    const bool parts = terrainParts || m_lightmapObjects;
    if (m_lightmapTerrain && !m_lightmapPrimary && !m_lightmapSecondary)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "At least one of Primary or Secondary must be selected");
    else if (!parts)
        ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "Trace the terrain, the objects or both");
    if (!m_lightmapResult.empty())
        ImGui::TextWrapped("%s", m_lightmapResult.c_str());
    ImGui::BeginDisabled(!parts);
    if (ImGui::Button("Generate"))
    {
        const bool resume = m_lightmapResume && resumable;
        std::string what = !terrainParts ? std::string()
                         : m_lightmapPrimary && m_lightmapSecondary ? "both" : m_lightmapPrimary ? "primary" : "secondary";
        if (m_lightmapObjects)
            what += std::string(what.empty() ? "" : "+") + (m_lightmapSelected ? "selected" : "objects");
        const char* const options = m_lightmapNearest && resume ? "nearest+resume" : m_lightmapNearest ? "nearest"
                                  : resume ? "resume" : "-";
        char line[160];
        std::snprintf(line, sizeof(line), "EditorLighting.lightmaps %d %s %s", m_lightmapQuality, what.c_str(), options);
        const ConsoleLineResult r = runConsoleLine(line);
        m_lightmapResult = r.output;
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "lightmaps: %s", r.output.c_str());
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorShell::drawPackPopup()
{
    if (m_openPack)
    {
        ImGui::OpenPopup("Pack the level");
        m_openPack = false;
    }
    if (!ImGui::BeginPopupModal("Pack the level", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    ImGui::TextWrapped("Writes the saved level as the game reads it - Levels/<name>/client.zip, server.zip and Info/ - "
                       "into a mod folder (<game>/mods/<mod>). Not into the editor's own mod folder: the game reads "
                       "archives before loose files, so the editor would load the pack instead of your edits.");
    ImGui::SetNextItemWidth(480.0f);
    if (ImGui::InputTextWithHint("Mod folder", "e.g. C:/Games/Battlefield 2/mods/mymod", m_packBuffer, sizeof(m_packBuffer)))
        m_options.packFolder = m_packBuffer;
    if (!m_packResult.empty())
        ImGui::TextWrapped("%s", m_packResult.c_str());
    if (ImGui::Button("Pack") && m_packBuffer[0])
    {
        const ConsoleLineResult r = runConsoleLine(std::string("EditorLevel.pack \"") + m_packBuffer + "\"");
        m_packResult = r.output;
        logLine(r.failed ? LogLevel::Warning : LogLevel::Info, "pack: %s", r.output.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void EditorShell::drawShortcuts()
{
    ImGui::SetNextWindowSize(ImVec2(600.0f, 560.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Keyboard shortcuts", &m_showShortcuts))
    {
        ImGui::End();
        return;
    }
    Shortcuts& table = shortcuts();
    if (!m_capturing.empty())
    {
        const ShortcutAction* action = Shortcuts::action(m_capturing);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Press the new key for \"%s\" (Escape cancels, Backspace clears it).",
                           action ? action->label : m_capturing.c_str());
        KeyChord chord;
        if (chordPressed(chord))
        {
            const bool plain = !chord.ctrl && !chord.shift && !chord.alt;
            std::string error;
            if (plain && chord.key == "Escape")
                logLine(LogLevel::Info, "shortcuts: %s unchanged", m_capturing.c_str());
            else if (!table.set(m_capturing, plain && chord.key == "Backspace" ? std::string() : formatChord(chord), error))
                logLine(LogLevel::Warning, "shortcuts: %s", error.c_str());
            else
                logLine(LogLevel::Info, "shortcuts: %s is %s", m_capturing.c_str(),
                        table.display(m_capturing).empty() ? "unbound" : table.display(m_capturing).c_str());
            m_capturing.clear();
            m_captureCooldown = 2;
        }
    }
    else
    {
        ImGui::TextDisabled("Click a key to change it. Kept in BF2Editor.settings.ini.");
    }
    if (ImGui::Button("Reset all"))
        table.resetAll();
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("shortcuts", 3, flags))
    {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();
        const std::vector<ShortcutAction>& actions = Shortcuts::actions();
        std::string group;
        for (size_t i = 0; i < actions.size(); ++i)
        {
            const ShortcutAction& a = actions[i];
            ImGui::PushID(a.id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (group != a.group)
            {
                group = a.group;
                ImGui::TextDisabled("%s", a.group);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
            }
            ImGui::Text("  %s%s", a.label, a.held ? " (held)" : "");
            const std::vector<std::string> clashes = table.conflicts(a.id);
            if (!clashes.empty())
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "(!)");
                std::string names;
                for (size_t k = 0; k < clashes.size(); ++k)
                {
                    const ShortcutAction* other = Shortcuts::action(clashes[k]);
                    names += (k ? ", " : "") + std::string(other ? other->label : clashes[k].c_str());
                }
                ImGui::SetItemTooltip("Also the key of: %s", names.c_str());
            }
            ImGui::TableNextColumn();
            const std::string keys = table.display(a.id);
            const bool capturing = m_capturing == a.id;
            if (ImGui::Button(capturing ? "..." : (keys.empty() ? "(none)" : keys.c_str()), ImVec2(-1.0f, 0.0f)))
                m_capturing = a.id;
            ImGui::TableNextColumn();
            if (!table.isDefault(a.id))
            {
                if (ImGui::SmallButton("Reset"))
                    table.reset(a.id);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

void EditorShell::drawCameraMenu()
{
    if (!ImGui::BeginMenu("Camera"))
        return;
    const CameraMode mode = m_session->camera().mode();
    struct Item { const char* label; const char* action; CameraMode mode; };
    const Item items[] = {
        { "Free", "camera.free", CameraMode::Free }, { "Top-down", "camera.topDown", CameraMode::TopDown },
        { "Orbit the selection", "camera.orbit", CameraMode::Orbit }, { "Follow the terrain", 0, CameraMode::FollowTerrain },
    };
    const bool soldier = playTestActive();
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); ++i)
    {
        const std::string keys = items[i].action ? shortcuts().display(items[i].action) : std::string();
        if (ImGui::MenuItem(items[i].label, keys.c_str(), !soldier && mode == items[i].mode))
        {
            // Any camera mode leaves soldier mode, as the retail host's Camera menu did.
            if (soldier)
                m_session->runCommand("EditorPlayTest.leave", "camera");
            char line[48];
            std::snprintf(line, sizeof(line), "EditorCamera.mode %d", static_cast<int>(items[i].mode));
            m_session->runCommand(line, "camera");
        }
    }
    // Soldier mode (App/PlayTest.h): walk the level as a soldier of the level's team kit, where the camera is. Esc
    // gives the mouse back, a click in the view takes it again.
    if (ImGui::MenuItem("Soldier", 0, soldier))
        m_session->runCommand(soldier ? "EditorPlayTest.leave" : "EditorPlayTest.soldier", "camera");
    // Vehicle mode: the selected object spawner's vehicle, made for the test and driven by the soldier (retail's
    // Camera.OnVehicleMode seated the editor player in the selected spawner's vehicle).
    if (ImGui::MenuItem("Vehicle (the selected spawner's)"))
        m_session->runCommand("EditorPlayTest.drive", "camera");
    ImGui::Separator();
    if (ImGui::MenuItem("Reset the speed", shortcuts().display("camera.resetSpeed").c_str()))
        m_session->runCommand("EditorCamera.resetSpeed", "camera");
    if (ImGui::MenuItem("Reset the position"))
        m_session->runCommand("EditorCamera.resetPosition", "camera");
    if (ImGui::BeginMenu("Set the position"))
    {
        if (ImGui::IsWindowAppearing())
        {
            m_cameraTarget[0] = m_session->camera().x();
            m_cameraTarget[1] = m_session->camera().y();
            m_cameraTarget[2] = m_session->camera().z();
        }
        ImGui::SetNextItemWidth(240.0f);
        ImGui::InputFloat3("x / y / z", m_cameraTarget, "%.1f");
        if (ImGui::Button("Go"))
        {
            char line[128];
            std::snprintf(line, sizeof(line), "EditorCamera.setPosition %.3f/%.3f/%.3f", m_cameraTarget[0],
                          m_cameraTarget[1], m_cameraTarget[2]);
            m_session->runCommand(line, "camera");
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Look from the selected object", 0, false, !m_session->selection().empty()))
        m_session->runCommand("EditorCamera.fromSelection", "camera");
    ImGui::SetItemTooltip("The camera takes the selected object's place and heading (a spawn point's view).");
    // The retail host's two camera position bookmarks (EditorCamera.addBookmark / toggleBookmark / bookmark).
    if (ImGui::BeginMenu("Position bookmarks"))
    {
        if (ImGui::MenuItem("Add a position bookmark"))
            m_session->runCommand("EditorCamera.addBookmark", "camera");
        ImGui::SetItemTooltip("Two are kept; a third drops the oldest.");
        const std::vector<EditSession::CameraBookmark>& bookmarks = m_session->cameraBookmarks();
        if (ImGui::MenuItem("Toggle between the two", 0, false, bookmarks.size() == 2))
            m_session->runCommand("EditorCamera.toggleBookmark", "camera");
        if (!bookmarks.empty())
            ImGui::Separator();
        for (size_t i = 0; i < bookmarks.size(); ++i)
        {
            char label[96];
            std::snprintf(label, sizeof(label), "%u. [%.1f %.1f %.1f]", static_cast<unsigned>(i + 1), bookmarks[i].x,
                          bookmarks[i].y, bookmarks[i].z);
            if (ImGui::MenuItem(label))
            {
                char line[48];
                std::snprintf(line, sizeof(line), "EditorCamera.bookmark %u", static_cast<unsigned>(i + 1));
                m_session->runCommand(line, "camera");
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Full view (hide the panels)", shortcuts().display("view.fullView").c_str(), !m_hiddenForFullView.empty()))
        setPanelVisible("all", !m_hiddenForFullView.empty());
    ImGui::EndMenu();
}

void EditorShell::drawLog()
{
    if (!ImGui::Begin("Log", &m_showLog))
    {
        ImGui::End();
        return;
    }
    ImGui::Checkbox("Auto-scroll", &m_logAutoScroll);
    ImGui::SameLine();
    if (ImGui::SmallButton("Clear"))
        clearRecentLog();
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy"))
    {
        std::string all;
        const std::deque<LogEntry>& entries = recentLog();
        for (size_t i = 0; i < entries.size(); ++i)
            all += entries[i].text + "\n";
        ImGui::SetClipboardText(all.c_str());
    }
    ImGui::Separator();
    ImGui::BeginChild("lines", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    const std::deque<LogEntry>& lines = recentLog();
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(lines.size()));
    while (clipper.Step())
    {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
        {
            const LogEntry& entry = lines[static_cast<size_t>(i)];
            if (entry.level == LogLevel::Error)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
            else if (entry.level == LogLevel::Warning)
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.35f, 1.0f));
            ImGui::TextUnformatted(entry.text.c_str());
            if (entry.level != LogLevel::Info)
                ImGui::PopStyleColor();
        }
    }
    clipper.End();
    if (m_logAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

void EditorShell::drawEngine(const ShellStatus& status)
{
    if (!ImGui::Begin("Engine", &m_showEngine))
    {
        ImGui::End();
        return;
    }
    ImGui::Text("Mod: %s", status.mod.c_str());
    ImGui::Text("Level: %s", status.level.empty() ? "(none open)" : status.level.c_str());
    ImGui::Text("Archives: %s", status.archives ? "mounted (read-only)" : "not mounted");
    ImGui::Text("Source control: %s", sourceControl().name());
    ImGui::Separator();
    ImGui::Text("Renderer: %s", status.renderer.empty() ? "(not up)" : status.renderer.c_str());
    ImGui::Text("Frame: %u, %.2f ms", status.frame, status.frameMs);
    if (!status.camera.empty())
    {
        ImGui::Separator();
        ImGui::Text("Camera: %s", status.camera.c_str());
        // The keys as the shortcut table has them now.
        const Shortcuts& keys = shortcuts();
        ImGui::TextDisabled("Hold the right mouse button to look; %s/%s/%s/%s move, %s/%s up/down,",
                            keys.display("camera.forward").c_str(), keys.display("camera.back").c_str(),
                            keys.display("camera.left").c_str(), keys.display("camera.right").c_str(),
                            keys.display("camera.up").c_str(), keys.display("camera.down").c_str());
        ImGui::TextDisabled("Shift is faster, the mouse wheel scales the speed.");
        ImGui::TextDisabled("Left click selects, a left drag draws a box; the move gizmo's centre slides");
        ImGui::TextDisabled("the selection over the ground, Ctrl when a drag starts copies. Undo %s, redo %s;",
                            keys.display("edit.undo").c_str(), keys.display("edit.redo").c_str());
        ImGui::TextDisabled("every key is listed (and changed) in Tools > Keyboard shortcuts.");
    }
    ImGui::End();
}

void EditorShell::drawAbout()
{
    if (!ImGui::Begin("About BF2Editor", &m_showAbout, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted("BF2Editor - the rebuilt Battlefield 2 editor");
    ImGui::TextUnformatted("Engine: the BF2Rebuilt 1.50 client object set");
    ImGui::Text("UI: Dear ImGui %s (docking)", IMGUI_VERSION);
    ImGui::End();
}

} // namespace bf2editor
