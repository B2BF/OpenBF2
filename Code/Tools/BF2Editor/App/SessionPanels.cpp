// Code/Tools/BF2Editor/App/SessionPanels.cpp - see SessionPanels.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "App/SessionPanels.h"

#include "App/EditSession.h"
#include "App/LevelDocument.h"
#include "App/LevelSettings.h"
#include "Framework/ConsoleCompletion.h"
#include "Framework/EditorConsole.h"
#include "Framework/EditorLog.h"
#include "Framework/EditorSettings.h"
#include "Framework/HelpDocuments.h"

#include "Core/ConsoleObjects.h"
#include "Core/Ids.h"
#include "Game/Objects/IObject.h"
#include "Game/Objects/IObjectTemplate.h"
#include "IO/File/File.h"
#include "Scene/Object/ObjectManager.h"
#include "Scene/Object/ObjectTypes/Base/ObjectTemplateManager.h"

#include "imgui.h"
#include "imgui_internal.h"     // FindWindowByName, for docking new panels beside existing ones
#include "App/ShortcutKeys.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using dice::hfe::Mat4;
using dice::hfe::Vec3;
using dice::hfe::world::IObject;

namespace bf2editor
{

namespace
{

const size_t kConsoleLines = 2000;

std::string lowered(const std::string& text)
{
    std::string out(text);
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
    return out;
}

// A panel that has never been placed (no .ini entry) opens as a tab beside `neighbour`, so panels
// added after a layout was saved do not appear floating over the 3D view.
void dockBeside(const char* neighbour)
{
    ImGuiWindow* window = ImGui::FindWindowByName(neighbour);
    if (window && window->DockId != 0)
        ImGui::SetNextWindowDockID(window->DockId, ImGuiCond_FirstUseEver);
}

int consoleHistoryCallback(ImGuiInputTextCallbackData* data);

// The gameplay objects the Edit menu makes: the kind EditorGameplay.createHere takes, the menu label,
// and the stem of the name the dialog suggests.
struct NewGameplayKind
{
    const char* kind;
    const char* label;
    const char* stem;
};
const NewGameplayKind kNewGameplayKinds[] = {
    { "ObjectSpawner", "Object spawner", "spawner" },
    { "SpawnPoint", "Spawn point", "spawnpoint" },
    { "ControlPoint", "Control point", "controlpoint" },
    { "Trigger", "Trigger", "trigger" },                        // AmbientObjects.con, like the next
    { "AmbientEffectArea", "Effect area", "effectarea" },
};
const size_t kFirstAmbientKind = 3;

// "<stem>_<n>" with the first n no template has.
std::string suggestedName(const char* stem)
{
    char name[128];
    for (int n = 1; n < 10000; ++n)
    {
        std::snprintf(name, sizeof(name), "%s_%d", stem, n);
        if (!dice::hfe::world::objectTemplateManager || !dice::hfe::world::objectTemplateManager->getTemplate(name))
            break;
    }
    return name;
}

} // namespace

SessionPanels::SessionPanels()
    : m_showSelection(true)
    , m_showObjects(true)
    , m_showHistory(true)
    , m_showConsole(true)
    , m_showLayers(true)
    , m_showProperties(true)
    , m_showCombatAreas(true)
    , m_showSounds(true)
    , m_showLevelSettings(true)
    , m_showGameplayLayers(true)
    , m_showTerrain(true)
    , m_showLibrary(true)
    , m_showObjectTemplates(true)
    , m_showWizard(false)
    , m_showMaterials(false)
    , m_showAnimations(false)
    , m_showMod(false)
    , m_confirmSnapAll(false)
    , m_newAreaSize(100.0f)
    , m_targetLayer(1)
    , m_newGameplayKind(-1)
    , m_openNewGameplay(false)
    , m_field(0)
    , m_fieldId(0)
    , m_rowsStale(true)
    , m_rowsRevision(0)
    , m_classFilter(0)
    , m_typedCursor(-1)
    , m_scrollConsole(false)
{
    m_filter[0] = 0;
    m_input[0] = 0;
    m_createTemplate[0] = 0;
    m_newProperty[0] = m_newValue[0] = 0;
    m_renameBuffer[0] = 0;
    m_newGameplayName[0] = 0;
    m_newGameplayEffect[0] = 0;
    m_newAreaName[0] = 0;
    m_newSoundName[0] = m_newSoundFile[0] = 0;
}

void SessionPanels::draw(EditSession& session)
{
    if (m_showSelection)
        drawSelection(session);
    if (m_showObjects)
        drawObjects(session);
    if (m_showHistory)
        drawHistory(session);
    if (m_showConsole)
        drawConsole(session);
    if (m_showLayers)
        drawLayers(session);
    if (m_showProperties)
        drawProperties(session);
    if (m_showCombatAreas)
        drawCombatAreas(session);
    if (m_showSounds)
        drawSounds(session);
    if (m_showLevelSettings)
        drawLevelSettings(session);
    if (m_showGameplayLayers)
        drawGameplayLayers(session);
    if (m_showTerrain)
        m_terrain.draw(session, &m_showTerrain);
    if (m_showRoads)
        m_roadPanel.draw(session, &m_showRoads);
    if (m_showLibrary)
        m_library.draw(session, &m_showLibrary);
    if (m_showObjectTemplates)
        m_objectTemplates.draw(session, &m_showObjectTemplates);
    if (m_showWizard)
        m_wizardPanel.draw(session, &m_showWizard);
    if (m_showMaterials)
        m_materialPanel.draw(session, &m_showMaterials);
    if (m_showAnimations)
        m_animationPanel.draw(session, &m_showAnimations);
    if (m_showMod)
        drawMod(session);
    // Edit > Snap all objects to the ground asks first, as retail did.
    if (m_confirmSnapAll)
    {
        ImGui::OpenPopup("Snap all objects");
        m_confirmSnapAll = false;
    }
    if (ImGui::BeginPopupModal("Snap all objects", 0, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted("Every object of the level's files goes down onto the terrain (one undoable edit).");
        if (ImGui::Button("Snap them"))
        {
            session.runCommand("EditorLevel.snapAllToGround", "snap");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    drawNewGameplay(session);
}

void SessionPanels::drawNewGameplay(EditSession& session)
{
    if (m_openNewGameplay)
    {
        ImGui::OpenPopup("New gameplay object");
        m_openNewGameplay = false;
    }
    if (m_newGameplayKind < 0 || !ImGui::BeginPopupModal("New gameplay object", 0, ImGuiWindowFlags_AlwaysAutoResize))
        return;
    const NewGameplayKind& kind = kNewGameplayKinds[m_newGameplayKind];
    const bool effectArea = std::strcmp(kind.kind, "AmbientEffectArea") == 0;
    ImGui::Text("New %s", kind.label);
    ImGui::TextDisabled(static_cast<size_t>(m_newGameplayKind) >= kFirstAmbientKind
                            ? "made under the middle of the view, in AmbientObjects.con, with a template of its own"
                            : "made under the middle of the view, in the gameplay layer, with a template of its own");
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
    bool create = ImGui::InputText("Name", m_newGameplayName, sizeof(m_newGameplayName),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled("the name of the template and of the object; it must be new");
    if (effectArea)
    {
        create = ImGui::InputText("Effect", m_newGameplayEffect, sizeof(m_newGameplayEffect),
                                  ImGuiInputTextFlags_EnterReturnsTrue) || create;
        ImGui::TextDisabled("the Bundle or EffectBundle template it plays (e_dAmb_dustCloud, say)");
    }
    if (!m_newGameplayError.empty())
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.4f, 1.0f), "%s", m_newGameplayError.c_str());
    create = ImGui::Button("Create") || create;
    ImGui::SameLine();
    const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);
    if (create && m_newGameplayName[0])
    {
        // Through the console like every edit, so it is logged and undoable.
        const std::string line = effectArea
            ? std::string("EditorGameplay.createEffectAreaHere \"") + m_newGameplayName + "\" \"" + m_newGameplayEffect + "\""
            : std::string("EditorGameplay.createHere ") + kind.kind + " \"" + m_newGameplayName + "\"";
        const ConsoleLineResult result = runConsoleLine(line);
        logLine(result.failed ? LogLevel::Warning : LogLevel::Info, "gameplay: %s", result.output.c_str());
        if (result.failed)
            m_newGameplayError = result.output;
        else
        {
            m_newGameplayKind = -1;
            ImGui::CloseCurrentPopup();
        }
    }
    else if (cancel)
    {
        m_newGameplayKind = -1;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void SessionPanels::loadSettings(const EditorSettings& settings)
{
    m_showSelection = settings.getBool("panels.selection", m_showSelection);
    m_showObjects = settings.getBool("panels.objects", m_showObjects);
    m_showHistory = settings.getBool("panels.history", m_showHistory);
    m_showConsole = settings.getBool("panels.console", m_showConsole);
    m_showLayers = settings.getBool("panels.layers", m_showLayers);
    m_showProperties = settings.getBool("panels.properties", m_showProperties);
    m_showCombatAreas = settings.getBool("panels.combatAreas", m_showCombatAreas);
    m_showSounds = settings.getBool("panels.sounds", m_showSounds);
    m_showLevelSettings = settings.getBool("panels.levelSettings", m_showLevelSettings);
    m_showGameplayLayers = settings.getBool("panels.gameplayLayers", m_showGameplayLayers);
    m_showTerrain = settings.getBool("panels.terrain", m_showTerrain);
    m_showRoads = settings.getBool("panels.roads", m_showRoads);
    m_showLibrary = settings.getBool("panels.library", m_showLibrary);
    m_showObjectTemplates = settings.getBool("panels.objectTemplates", m_showObjectTemplates);
    m_showWizard = settings.getBool("panels.wizard", m_showWizard);
    m_showMaterials = settings.getBool("panels.materials", m_showMaterials);
    m_showAnimations = settings.getBool("panels.animations", m_showAnimations);
    m_showMod = settings.getBool("panels.mod", m_showMod);
    // The Console panel's typed lines, kept between sessions as the retail ConsoleUI kept them (oldest first).
    m_typed.clear();
    for (int i = 0; i < kKeptConsoleLines; ++i)
    {
        char key[48];
        std::snprintf(key, sizeof(key), "console.history.%02d", i);
        const std::string line = settings.get(key, std::string());
        if (!line.empty())
            m_typed.push_back(line);
    }
}

void SessionPanels::saveSettings(EditorSettings& settings) const
{
    settings.setBool("panels.selection", m_showSelection);
    settings.setBool("panels.objects", m_showObjects);
    settings.setBool("panels.history", m_showHistory);
    settings.setBool("panels.console", m_showConsole);
    settings.setBool("panels.layers", m_showLayers);
    settings.setBool("panels.properties", m_showProperties);
    settings.setBool("panels.combatAreas", m_showCombatAreas);
    settings.setBool("panels.sounds", m_showSounds);
    settings.setBool("panels.levelSettings", m_showLevelSettings);
    settings.setBool("panels.gameplayLayers", m_showGameplayLayers);
    settings.setBool("panels.terrain", m_showTerrain);
    settings.setBool("panels.roads", m_showRoads);
    settings.setBool("panels.library", m_showLibrary);
    settings.setBool("panels.objectTemplates", m_showObjectTemplates);
    settings.setBool("panels.wizard", m_showWizard);
    settings.setBool("panels.materials", m_showMaterials);
    settings.setBool("panels.animations", m_showAnimations);
    settings.setBool("panels.mod", m_showMod);
    const size_t first = m_typed.size() > static_cast<size_t>(kKeptConsoleLines) ? m_typed.size() - kKeptConsoleLines : 0;
    for (int i = 0; i < kKeptConsoleLines; ++i)
    {
        char key[48];
        std::snprintf(key, sizeof(key), "console.history.%02d", i);
        const size_t at = first + static_cast<size_t>(i);
        settings.set(key, at < m_typed.size() ? m_typed[at] : std::string());
    }
}

std::vector<std::string> SessionPanels::openPanels() const
{
    std::vector<std::string> open;
    if (m_showSelection) open.push_back("Selection");
    if (m_showObjects)   open.push_back("Objects");
    if (m_showHistory)   open.push_back("History");
    if (m_showConsole)   open.push_back("Console");
    if (m_showLayers)    open.push_back("Layers");
    if (m_showProperties) open.push_back("Properties");
    if (m_showCombatAreas) open.push_back("Combat areas");
    if (m_showSounds) open.push_back("Sounds");
    if (m_showLevelSettings) open.push_back("Level settings");
    if (m_showGameplayLayers) open.push_back("Gameplay layers");
    if (m_showTerrain) open.push_back("Terrain");
    if (m_showRoads) open.push_back("Roads");
    if (m_showLibrary) open.push_back("Bundles and brushes");
    if (m_showObjectTemplates) open.push_back("Object templates");
    if (m_showWizard) open.push_back("Object wizard");
    if (m_showMaterials) open.push_back("Materials");
    if (m_showAnimations) open.push_back("Animations");
    if (m_showMod) open.push_back("Mod");
    return open;
}

bool SessionPanels::setPanelVisible(const std::string& name, bool visible)
{
    struct Panel { const char* name; bool* shown; };
    const Panel panels[] = {
        { "Selection", &m_showSelection }, { "Objects", &m_showObjects }, { "History", &m_showHistory },
        { "Console", &m_showConsole }, { "Layers", &m_showLayers }, { "Properties", &m_showProperties },
        { "CombatAreas", &m_showCombatAreas }, { "Combat areas", &m_showCombatAreas }, { "Sounds", &m_showSounds },
        { "LevelSettings", &m_showLevelSettings }, { "Level settings", &m_showLevelSettings },
        { "GameplayLayers", &m_showGameplayLayers }, { "Gameplay layers", &m_showGameplayLayers },
        { "Terrain", &m_showTerrain }, { "Roads", &m_showRoads },
        { "Library", &m_showLibrary }, { "Bundles and brushes", &m_showLibrary }, { "BundlesAndBrushes", &m_showLibrary },
        { "ObjectTemplates", &m_showObjectTemplates }, { "Object templates", &m_showObjectTemplates },
        { "Wizard", &m_showWizard }, { "ObjectWizard", &m_showWizard }, { "Object wizard", &m_showWizard },
        { "Materials", &m_showMaterials }, { "MaterialEditor", &m_showMaterials },
        { "Animations", &m_showAnimations }, { "AnimationEditor", &m_showAnimations },
        { "Animation editor", &m_showAnimations },
        { "Mod", &m_showMod }, { "ModManager", &m_showMod }, { "Mod manager", &m_showMod },
    };
    for (size_t i = 0; i < sizeof(panels) / sizeof(panels[0]); ++i)
    {
        if (_stricmp(panels[i].name, name.c_str()) == 0)
        {
            *panels[i].shown = visible;
            return true;
        }
    }
    return false;
}

void SessionPanels::viewMenuItems()
{
    ImGui::MenuItem("Selection", 0, &m_showSelection);
    ImGui::MenuItem("Properties", 0, &m_showProperties);
    ImGui::MenuItem("Objects", 0, &m_showObjects);
    ImGui::MenuItem("History", 0, &m_showHistory);
    ImGui::MenuItem("Console", 0, &m_showConsole);
    ImGui::MenuItem("Layers", 0, &m_showLayers);
    ImGui::MenuItem("Combat areas", 0, &m_showCombatAreas);
    ImGui::MenuItem("Sounds", 0, &m_showSounds);
    ImGui::MenuItem("Level settings", 0, &m_showLevelSettings);
    ImGui::MenuItem("Gameplay layers", 0, &m_showGameplayLayers);
    ImGui::MenuItem("Terrain", 0, &m_showTerrain);
    ImGui::MenuItem("Roads", 0, &m_showRoads);
    ImGui::MenuItem("Bundles and brushes", 0, &m_showLibrary);
    ImGui::MenuItem("Object templates", 0, &m_showObjectTemplates);
    ImGui::MenuItem("Object wizard", 0, &m_showWizard);
    ImGui::MenuItem("Materials", 0, &m_showMaterials);
    ImGui::MenuItem("Animations", 0, &m_showAnimations);
    ImGui::MenuItem("Mod", 0, &m_showMod);
}

// Mod (the mod manager's pages)

void SessionPanels::drawMod(EditSession& session)
{
    dockBeside("Level settings");
    if (!ImGui::Begin("Mod", &m_showMod))
    {
        ImGui::End();
        return;
    }
    // The mods under mods/: another one opens in a new editor (EditorMod.open), a new one is made with the
    // retail scaffold (EditorMod.create).
    std::string mods;
    session.listModsCommand(mods);
    ImGui::SeparatorText("Mods");
    ImGui::TextWrapped("%s", mods.c_str());
    ImGui::TextDisabled("EditorMod.open <name> starts the editor on another mod; EditorMod.create <name> makes one.");
    DescriptionDocuments& documents = session.descriptions();
    const struct Page { DescriptionDocuments::Which which; const char* name; const char* title; } pages[] = {
        { DescriptionDocuments::Mod, "mod", "This mod (mod.desc)" },
        { DescriptionDocuments::Level, "level", "This level (Info/<level>.desc)" },
    };
    for (int p = 0; p < 2; ++p)
    {
        ImGui::SeparatorText(pages[p].title);
        if (!documents.exists(pages[p].which))
        {
            ImGui::TextDisabled("%s is not there", documents.path(pages[p].which).c_str());
            continue;
        }
        ImGui::TextDisabled("%s; File > Save writes it", documents.path(pages[p].which).c_str());
        size_t count = 0;
        const char* const* fields = DescriptionDocuments::fields(pages[p].which, count);
        for (size_t i = 0; i < count; ++i)
        {
            const std::string key = std::string(pages[p].name) + "." + fields[i];
            std::string value;
            const bool present = documents.value(pages[p].which, fields[i], value);
            ImGui::PushID(key.c_str());
            char buffer[1024];
            if (m_descriptionEditing == key)
                std::snprintf(buffer, sizeof(buffer), "%s", m_descriptionBuffer);
            else
                std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
            ImGui::SetNextItemWidth(-90.0f);
            if (ImGui::InputTextWithHint(fields[i], present ? "" : "(not in the file)", buffer, sizeof(buffer),
                                         ImGuiInputTextFlags_EnterReturnsTrue))
            {
                session.runCommand(std::string(p == 0 ? "EditorMod" : "EditorLevel") + ".setDescription " + fields[i]
                                       + " \"" + buffer + "\"", "description");
                m_descriptionEditing.clear();
            }
            else if (ImGui::IsItemActive())
            {
                m_descriptionEditing = key;
                std::snprintf(m_descriptionBuffer, sizeof(m_descriptionBuffer), "%s", buffer);
            }
            else if (m_descriptionEditing == key)
                m_descriptionEditing.clear();
            ImGui::PopID();
        }
    }
    ImGui::TextDisabled("Enter sets a field (one undoable edit). A locid attribute (a localised text) stays as it is.");
    ImGui::End();
}

void SessionPanels::editMenu(EditSession& session, const std::string& resourceTemplate)
{
    if (!ImGui::BeginMenu("Edit"))
        return;
    const HistoryEntry* undoEntry = session.history().nextUndo();
    const HistoryEntry* redoEntry = session.history().nextRedo();
    const std::string undoLabel = undoEntry ? "Undo " + undoEntry->label : std::string("Undo");
    const std::string redoLabel = redoEntry ? "Redo " + redoEntry->label : std::string("Redo");
    if (ImGui::MenuItem(undoLabel.c_str(), shortcuts().display("edit.undo").c_str(), false, undoEntry != 0))
        session.runUndo();
    if (ImGui::MenuItem(redoLabel.c_str(), shortcuts().display("edit.redo").c_str(), false, redoEntry != 0))
        session.runRedo();
    ImGui::Separator();
    const bool selected = !session.selection().empty();
    if (ImGui::MenuItem("Cut", shortcuts().display("edit.cut").c_str(), false, selected))
        session.runCommand("EditorClipboard.cut", "cut");
    if (ImGui::MenuItem("Copy", shortcuts().display("edit.copy").c_str(), false, selected))
        session.runCommand("EditorClipboard.copy", "copy");
    if (ImGui::MenuItem("Paste at the view centre", shortcuts().display("edit.paste").c_str(), false, session.clipboardSize() != 0))
        session.runCommand("EditorClipboard.paste", "paste");
    if (ImGui::MenuItem("Paste in place", 0, false, session.clipboardSize() != 0 && !session.lastOffsetArmed()))
        session.runCommand("EditorClipboard.pasteInPlace", "paste");
    if (ImGui::MenuItem("Last-offset copy", 0, false, session.selection().size() == 2))
        session.runCommand("EditorClipboard.lastOffsetCopy", "copy");
    ImGui::Separator();
    if (ImGui::MenuItem("Reload the selected objects", 0, false, !session.selection().empty()))
        session.runCommand("EditorSelection.reload", "reload");
    ImGui::SetItemTooltip("Every object of the selected objects' templates made again from the templates in memory.");
    if (ImGui::MenuItem("Reload all objects"))
        session.runCommand("EditorLevel.reloadObjects", "reload");
    if (ImGui::MenuItem("Re-position objects for the terrain's scale"))
        session.runCommand("EditorLevel.repositionObjects", "reposition");
    ImGui::SetItemTooltip("After the terrain's vertical scale changed, each object rises or sinks with the ground under it.");
    ImGui::SetItemTooltip("Two objects of one template: each Paste then adds one more at the step from the first to "
                          "the second, from the last one made.");
    if (ImGui::MenuItem("Delete", shortcuts().display("edit.delete").c_str(), false, selected))
        session.runDelete();
    if (ImGui::MenuItem("Duplicate", shortcuts().display("edit.duplicate").c_str(), false, selected))
        session.runDuplicate();
    const std::string replaceLabel =
        resourceTemplate.empty() ? std::string("Replace with... (select a template in Resources)")
                                 : "Replace with " + resourceTemplate;
    if (ImGui::MenuItem(replaceLabel.c_str(), 0, false, selected && !resourceTemplate.empty()))
        session.runCommand("EditorSelection.replace " + resourceTemplate, "replace");
    // Gameplay objects (phase C): each with a template of its own, so the dialog asks for its name.
    if (ImGui::BeginMenu("New gameplay object"))
    {
        for (size_t i = 0; i < sizeof(kNewGameplayKinds) / sizeof(kNewGameplayKinds[0]); ++i)
        {
            if (i == kFirstAmbientKind)
                ImGui::SeparatorText("Ambient");
            const std::string label = std::string(kNewGameplayKinds[i].label) + "...";
            if (ImGui::MenuItem(label.c_str()))
            {
                m_newGameplayKind = static_cast<int>(i);
                m_openNewGameplay = true;
                m_newGameplayError.clear();
                std::snprintf(m_newGameplayName, sizeof(m_newGameplayName), "%s",
                              suggestedName(kNewGameplayKinds[i].stem).c_str());
                // An effect: the Resources panel's selected template, which the dialog lets be changed.
                std::snprintf(m_newGameplayEffect, sizeof(m_newGameplayEffect), "%s", resourceTemplate.c_str());
            }
        }
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Copy transformation", 0, false, selected))
        session.runCommand("EditorSelection.copyTransform", "transformation");
    if (ImGui::MenuItem("Paste transformation", 0, false, selected && session.hasCopiedTransform()))
        session.runCommand("EditorSelection.pasteTransform", "transformation");
    if (ImGui::MenuItem("Snap to ground", shortcuts().display("selection.snapToGround").c_str(), false, selected))
        session.runSnapToGround();
    if (ImGui::MenuItem("Snap all objects to the ground..."))
        m_confirmSnapAll = true;
    char gridLabel[64];
    std::snprintf(gridLabel, sizeof(gridLabel), "Snap to the %g m grid", session.gridStep());
    if (ImGui::MenuItem(gridLabel, 0, false, selected))
    {
        char line[64];
        std::snprintf(line, sizeof(line), "EditorSelection.snapToGrid %g", session.gridStep());
        session.runCommand(line, "snap");
    }
    if (ImGui::MenuItem("Deselect", "Esc", false, selected))
        session.selection().clear();
    if (ImGui::MenuItem("Frame selection", "F", false, selected))
    {
        std::string error;
        if (!session.frameSelection(error))
            logLine(LogLevel::Warning, "frame: %s", error.c_str());
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Select tool", "1", session.gizmoMode() == GizmoMode::Select))
        session.setMode(GizmoMode::Select);
    if (ImGui::MenuItem("Move tool", "2", session.gizmoMode() == GizmoMode::Move))
        session.setMode(GizmoMode::Move);
    if (ImGui::MenuItem("Rotate tool", "3", session.gizmoMode() == GizmoMode::Rotate))
        session.setMode(GizmoMode::Rotate);
    if (ImGui::MenuItem("Local axes", "X", session.gizmoSpace() == GizmoSpace::Local))
        session.setSpace(session.gizmoSpace() == GizmoSpace::Local ? GizmoSpace::World : GizmoSpace::Local);
    ImGui::EndMenu();
}

// Selection

void SessionPanels::finishFieldEdit(EditSession& session)
{
    const int field = m_field;
    m_field = 0;
    Mat4 after;
    if (!EditSession::transformOf(m_fieldId, after) || std::memcmp(after.m, m_fieldStart.m, sizeof(after.m)) == 0)
        return;
    std::vector<EditSession::TransformChange> changes(1);
    changes[0].id = m_fieldId;
    changes[0].before = m_fieldStart;
    changes[0].after = after;
    const std::string label = std::string(field == 2 ? "Rotate " : "Move ") + EditSession::describe(m_fieldId);
    const unsigned revision = session.history().revision();
    std::string error;
    if (!session.applyTransforms(changes, label, error))
    {
        if (session.history().revision() == revision)
        {
            IObject* object = EditSession::object(m_fieldId);
            if (object)
                object->setAbsoluteTransformation(m_fieldStart);
        }
        logLine(LogLevel::Error, "%s", error.c_str());
    }
}

void SessionPanels::drawSelection(EditSession& session)
{
    dockBeside("Engine");
    if (!ImGui::Begin("Selection", &m_showSelection))
    {
        ImGui::End();
        return;
    }
    const unsigned id = session.selection().primary();
    IObject* object = EditSession::object(id);
    if (m_field != 0 && m_fieldId != id)
        finishFieldEdit(session);       // the selection changed under a field edit
    if (!object)
    {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::TextDisabled("Click an object in the 3D view, or pick one in Objects.");
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(EditSession::describe(id).c_str());
    const dice::hfe::world::IObjectTemplate* objectTemplate = object->getTemplate();
    if (objectTemplate)
        ImGui::TextDisabled("class %s (0x%X)", EditSession::className(objectTemplate->getClassID()).c_str(),
                            objectTemplate->getClassID());
    if (session.selection().size() > 1)
        ImGui::TextDisabled("%u objects selected; these fields and the gizmo edit this one.",
                            static_cast<unsigned>(session.selection().size()));
    ImGui::Separator();

    // A field drag moves the object directly while it lasts (a preview), then becomes one edit
    // through the history when the field lets go, like a gizmo drag.
    const Mat4 before = object->getAbsoluteTransformation();
    Mat4 edited = before;
    float position[3] = { before.m[12], before.m[13], before.m[14] };
    const bool positionChanged = ImGui::DragFloat3("Position", position, 0.05f, 0.0f, 0.0f, "%.3f");
    const bool positionActive = ImGui::IsItemActive();
    const Vec3 angles = EditSession::rotationOf(before);
    float rotation[3] = { angles.x, angles.y, angles.z };
    const bool rotationChanged = ImGui::DragFloat3("Rotation", rotation, 0.25f, 0.0f, 0.0f, "%.1f");
    const bool rotationActive = ImGui::IsItemActive();
    ImGui::TextDisabled("rotation is yaw / pitch / roll in degrees, as .con's Object.rotation");

    const int activeField = positionActive ? 1 : (rotationActive ? 2 : 0);
    if (m_field != 0 && activeField != m_field)
        finishFieldEdit(session);
    if (activeField != 0 && m_field == 0)
    {
        m_field = activeField;
        m_fieldId = id;
        m_fieldStart = before;
    }
    if (positionChanged)
    {
        edited.m[12] = position[0];
        edited.m[13] = position[1];
        edited.m[14] = position[2];
    }
    if (rotationChanged)
    {
        Vec3 ypr;
        ypr.x = rotation[0];
        ypr.y = rotation[1];
        ypr.z = rotation[2];
        EditSession::setRotation(edited, ypr);
    }
    if (positionChanged || rotationChanged)
    {
        if (m_field != 0)
        {
            object->setAbsoluteTransformation(edited);      // preview
        }
        else
        {
            // Changed without staying active (a typed value): one edit at once.
            std::vector<EditSession::TransformChange> changes(1);
            changes[0].id = id;
            changes[0].before = before;
            changes[0].after = edited;
            std::string error;
            if (!session.applyTransforms(changes, std::string(rotationChanged ? "Rotate " : "Move ")
                                                      + EditSession::describe(id), error))
                logLine(LogLevel::Error, "%s", error.c_str());
        }
    }

    ImGui::Text("Layer %d, group %d", static_cast<int>(object->getLayer()), static_cast<int>(object->getGroup()));
    // The template's anchor (the point that stands on the ground) and where it is in the world.
    float anchor[3], worldAnchor[3];
    EditSession::anchorPosOf(object, anchor);
    EditSession::worldAnchorOf(id, worldAnchor);
    ImGui::TextDisabled("anchor %.3f/%.3f/%.3f, in the world %.2f/%.2f/%.2f", anchor[0], anchor[1], anchor[2],
                        worldAnchor[0], worldAnchor[1], worldAnchor[2]);
    if (ImGui::SmallButton("Stamp"))
        session.runCommand("EditorSelection.alignStamp", "anchor");
    ImGui::SetItemTooltip("Remember this object: Align brings the selected objects' anchors onto its anchor.");
    ImGui::SameLine();
    if (ImGui::SmallButton("Align to the stamp"))
        session.runCommand("EditorSelection.align", "anchor");
    ImGui::SameLine();
    if (ImGui::SmallButton("Raise by the anchor"))
        session.runCommand("EditorSelection.raiseByAnchor", "anchor");
    ImGui::SetItemTooltip("Each selected object's height less its anchor's (the retail Raise using anchor).");
    if (ImGui::SmallButton("Anchor to the base"))
        session.runCommand("EditorSelection.anchorToBox bottom", "anchor");
    ImGui::SetItemTooltip("The template's anchor to the middle of the bounding box's base (every object of the "
                          "template; saved in the template's file).");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset the anchor"))
        session.runCommand("EditorSelection.resetAnchor", "anchor");
    ImGui::SetItemTooltip("The template's anchor as its file had it when last saved.");
    ImGui::Separator();
    if (ImGui::Button("Frame (F)"))
    {
        std::string error;
        if (!session.frameSelection(error))
            logLine(LogLevel::Warning, "frame: %s", error.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button("Deselect (Esc)"))
        session.selection().clear();
    if (objectTemplate)
    {
        // Every object of this template in the level (the retail "select all of the same type").
        const std::string select = "Select all " + objectTemplate->getName();
        if (ImGui::Button(select.c_str()))
            session.runCommand("EditorSelection.selectTemplate " + objectTemplate->getName(), "select");
    }
    ImGui::End();
}

// Properties

void SessionPanels::loadTemplateLines(const std::string& templateName)
{
    m_templateName = templateName;
    m_templateFile.clear();
    m_templateLines.clear();
    if (!dice::hfe::world::objectTemplateManager)
        return;
    std::string path = dice::hfe::world::objectTemplateManager->getWorkingFolderAndFile(templateName);
    for (size_t i = 0; i < path.size(); ++i)
        if (path[i] == '\\')
            path[i] = '/';
    m_templateFile = path;
    if (path.empty())
        return;
    dice::hfe::io::File file;
    if (!file.open(path, dice::hfe::io::IFileSystem::FILEMODE_READ, static_cast<dice::hfe::io::IFileSystem::FileAccess>(0), 0))
        return;
    const int size = file.getSize();
    if (size <= 0)
        return;
    std::string text(static_cast<size_t>(size), '\0');
    if (file.read(&text[0], static_cast<unsigned>(size)) != static_cast<unsigned>(size))
        return;
    // The block from `ObjectTemplate.create <class> <templateName>` to the next ObjectTemplate.create.
    bool inside = false;
    size_t start = 0;
    const std::string wanted = lowered(templateName);
    while (start < text.size())
    {
        size_t end = text.find('\n', start);
        if (end == std::string::npos)
            end = text.size();
        std::string line = text.substr(start, end - start);
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        start = end + 1;
        const std::string low = lowered(line);
        const size_t first = low.find_first_not_of(" \t");
        if (first != std::string::npos && low.compare(first, 22, "objecttemplate.create ") == 0)
        {
            // "objecttemplate.create <class> <name>": the name is the third word.
            char command[64], className[128], name[256];
            const bool named = std::sscanf(low.c_str() + first, "%63s %127s %255s", command, className, name) == 3;
            if (inside)
                break;
            inside = named && wanted == name;
        }
        if (inside)
            m_templateLines.push_back(line);
    }
}

void SessionPanels::drawProperties(EditSession& session)
{
    dockBeside("Selection");
    if (!ImGui::Begin("Properties", &m_showProperties))
    {
        ImGui::End();
        return;
    }
    const unsigned id = session.selection().primary();
    IObject* object = EditSession::object(id);
    if (!object)
    {
        ImGui::TextDisabled("Nothing selected.");
        ImGui::End();
        return;
    }
    ImGui::TextUnformatted(EditSession::describe(id).c_str());
    if (session.selection().size() > 1)
        ImGui::TextDisabled("%u objects selected: a change applies to all of them.",
                            static_cast<unsigned>(session.selection().size()));

    // Per object: saved with the level, in the lines the stock files use.
    ImGui::SeparatorText("Object");
    const ObjectProperties p = EditSession::propertiesOf(object);
    char line[96];
    int group = p.group;
    if (ImGui::InputInt("Group", &group, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        std::snprintf(line, sizeof(line), "EditorSelection.setProperty group %d", group);
        session.runCommand(line, "properties");
    }
    const struct Flag { const char* label; const char* name; bool value; } flags[] = {
        { "Disable children", "disableChildren", p.disableChildren },
        { "Not in AI", "notInAI", p.notInAI },
        { "Overgrowth", "isOvergrowth", p.isOvergrowth },
    };
    for (size_t i = 0; i < sizeof(flags) / sizeof(flags[0]); ++i)
    {
        bool value = flags[i].value;
        if (ImGui::Checkbox(flags[i].label, &value))
        {
            std::snprintf(line, sizeof(line), "EditorSelection.setProperty %s %d", flags[i].name, value ? 1 : 0);
            session.runCommand(line, "properties");
        }
    }
    int mask = p.lightSourceMask;
    if (ImGui::InputInt("Light source mask", &mask, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        std::snprintf(line, sizeof(line), "EditorSelection.setProperty lightSourceMask %d", mask);
        session.runCommand(line, "properties");
    }
    if (p.isSpawner)
    {
        // The control point the spawner belongs to (its team follows the point's).
        int controlPoint = p.controlPointId;
        if (ImGui::InputInt("Control point id", &controlPoint, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::snprintf(line, sizeof(line), "EditorSelection.setProperty controlPointId %d", controlPoint);
            session.runCommand(line, "properties");
        }
        // Of the control points with this id, those whose layer meets the spawner's (each game mode numbers its own).
        std::string owner = "no control point of the level has this id";
        const std::vector<EditSession::LevelObjectRow> points = session.controlPoints();
        bool named = false;
        for (size_t i = 0; i < points.size(); ++i)
            if (points[i].id == p.controlPointId && session.layersMeet(points[i].layer, static_cast<int>(object->getLayer())))
            {
                owner = (named ? owner + ", " : std::string("control point ")) + points[i].name;
                named = true;
            }
        ImGui::TextDisabled("%s", owner.c_str());
    }
    if (object->getTemplate() && object->getTemplate()->getClassID() == dice::hfe::world::CID_ControlPointTemplate)
    {
        // Retail's "select the control point's spawners" (LevelEditor 0x10015930): the spawn points and object
        // spawners that name its id, selected with it, so a delete takes them along.
        if (ImGui::SmallButton("Select its spawn points and spawners"))
            session.runCommand("EditorGameplay.selectSpawners", "spawners");
        // Supply lines: the point's supply group, the groups its teams need, and retail's
        // Group / Ungroup / Connect / Disconnect (on the selected control points' templates; each undoable).
        const std::vector<EditSession::SupplyPoint> supply = session.supplyPoints();
        const EditSession::SupplyPoint* mine = 0;
        std::vector<int> groups;
        for (size_t i = 0; i < supply.size(); ++i)
        {
            if (supply[i].object == id)
                mine = &supply[i];
            if (supply[i].group >= 0 && std::find(groups.begin(), groups.end(), supply[i].group) == groups.end())
                groups.push_back(supply[i].group);
        }
        std::sort(groups.begin(), groups.end());
        if (mine)
        {
            std::string needs;
            for (int t = 0; t < 2; ++t)
            {
                if (mine->needs[t].empty())
                    continue;
                needs += std::string(needs.empty() ? "" : "; ") + (t == 0 ? "team 1 needs" : "team 2 needs");
                for (size_t k = 0; k < mine->needs[t].size(); ++k)
                    needs += " " + std::to_string(mine->needs[t][k]);
            }
            ImGui::TextDisabled("Supply group: %s%s%s", mine->group >= 0 ? std::to_string(mine->group).c_str() : "none",
                                needs.empty() ? "" : "; ", needs.c_str());
            if (ImGui::SmallButton("Group the selected control points"))
                session.runCommand("EditorGameplay.groupControlPoints", "supply");
            ImGui::SetItemTooltip("They take a new supply group (the highest + 1); this one takes the supply needs of the\n"
                                  "first other member that has any.");
            if (mine->group >= 0)
            {
                ImGui::SameLine();
                if (ImGui::SmallButton("Ungroup"))
                    session.runCommand("EditorGameplay.ungroupControlPoints", "supply");
                for (int t = 1; t <= 2; ++t)
                {
                    char label[48];
                    std::snprintf(label, sizeof(label), "Team %d of this group needs group##needs%d", t, t);
                    ImGui::SetNextItemWidth(160.0f);
                    if (ImGui::BeginCombo(label, "pick a group"))
                    {
                        for (size_t g = 0; g < groups.size(); ++g)
                            if (groups[g] != mine->group && ImGui::Selectable(std::to_string(groups[g]).c_str()))
                            {
                                std::snprintf(line, sizeof(line), "EditorGameplay.connectSupply %d %d %d", t, groups[g], mine->group);
                                session.runCommand(line, "supply");
                            }
                        ImGui::EndCombo();
                    }
                }
                if (ImGui::SmallButton("Disconnect this group's supply lines"))
                {
                    std::snprintf(line, sizeof(line), "EditorGameplay.disconnectSupply %d", mine->group);
                    session.runCommand(line, "supply");
                }
            }
        }
    }
    ImGui::TextDisabled(p.isSpawner ? "saved as Object.group, disableChildren, notInAI, isOvergrowth, setLightSourceMask, "
                                      "setControlPointId"
                                    : "saved as Object.group, disableChildren, notInAI, isOvergrowth, setLightSourceMask");

    // The template: its lines as the file that defines it has them.
    ImGui::SeparatorText("Template");
    const dice::hfe::world::IObjectTemplate* objectTemplate = object->getTemplate();
    if (!objectTemplate)
    {
        ImGui::TextDisabled("(no template)");
        ImGui::End();
        return;
    }
    // A template a level file defines (the gameplay layer's, AmbientObjects.con's): its lines are
    // edited here and saved with that file.
    size_t levelFile = 0;
    std::string levelClass;
    LevelDocument* document = session.document();
    if (document && document->templateFile(objectTemplate->getName(), levelFile, levelClass))
    {
        drawLevelTemplate(session, *document, levelFile, objectTemplate->getName());
        ImGui::End();
        return;
    }
    if (_stricmp(m_templateName.c_str(), objectTemplate->getName().c_str()) != 0)
        loadTemplateLines(objectTemplate->getName());
    ImGui::Text("%s  (%s)", objectTemplate->getName().c_str(), EditSession::className(objectTemplate->getClassID()).c_str());
    ImGui::TextDisabled("%s", m_templateFile.empty() ? "(the file that defines it is not known)" : m_templateFile.c_str());
    if (ImGui::SmallButton("Edit in Object templates"))
        session.runCommand("EditorObjectTemplate.show " + objectTemplate->getName(), "template");
    ImGui::SameLine();
    ImGui::TextDisabled("(shown here as its file has it; edited in place there)");
    ImGui::BeginChild("templateLines", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
    if (m_templateLines.empty())
        ImGui::TextDisabled("(its lines were not found in that file)");
    for (size_t i = 0; i < m_templateLines.size(); ++i)
        ImGui::TextUnformatted(m_templateLines[i].c_str());
    ImGui::EndChild();
    ImGui::End();
}

void SessionPanels::drawLevelTemplate(EditSession& session, LevelDocument& document, size_t file,
                                      const std::string& name)
{
    std::string className;
    document.templateFile(name, file, className);
    ImGui::Text("%s  (%s)", name.c_str(), className.c_str());
    ImGui::TextDisabled("defined in %s; edits are saved with it", document.fileName(file).c_str());
    // A new name for the template and its objects (one undoable edit, EditorGameplay.rename).
    if (m_renameFor != name)
    {
        std::snprintf(m_renameBuffer, sizeof(m_renameBuffer), "%s", name.c_str());
        m_renameFor = name;
    }
    ImGui::SetNextItemWidth(220.0f);
    const bool renameNow = ImGui::InputText("##rename", m_renameBuffer, sizeof(m_renameBuffer),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    if ((ImGui::Button("Rename") || renameNow) && m_renameBuffer[0] && name != m_renameBuffer)
    {
        session.runCommand("EditorGameplay.rename " + name + " " + m_renameBuffer, "rename");
        m_renameFor.clear();
    }
    ImGui::SetItemTooltip("The template's name in the level files. Players see a control point's setControlPointName "
                          "line, not this name.");
    // A spawner's two vehicles from the retail presets.
    if (_stricmp(className.c_str(), "ObjectSpawner") == 0)
    {
        const std::vector<SpawnerPreset>& presets = session.levelEditorLists().spawnerPresets();
        if (!presets.empty())
        {
            ImGui::SetNextItemWidth(320.0f);
            if (ImGui::BeginCombo("Vehicle preset", "team 1 / team 2 vehicles from a preset"))
            {
                for (size_t k = 0; k < presets.size(); ++k)
                {
                    const std::string item = presets[k].name + "  (" + presets[k].team1 + " / " + presets[k].team2 + ")";
                    if (ImGui::Selectable(item.c_str()))
                        session.runCommand("EditorGameplay.spawnerPreset " + name + " " + presets[k].name, "spawner");
                }
                ImGui::EndCombo();
            }
        }
    }
    const std::vector<LevelObjectFile::TemplateLine> lines = document.templateLines(file, name);
    ImGui::BeginChild("levelTemplate", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 1.2f), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const LevelObjectFile::TemplateLine& line = lines[i];
        ImGui::PushID(static_cast<int>(i));
        // An indexed line (setObjectTemplate <team> <template>) shows its index in the label and edits
        // the rest; the author stamp is shown, not edited.
        const bool indexed = line.key.find(' ') != std::string::npos;
        std::string label = line.property;
        std::string shown = line.value;
        std::string index;
        if (indexed)
        {
            const size_t space = line.value.find_first_of(" \t");
            index = space == std::string::npos ? line.value : line.value.substr(0, space);
            shown = space == std::string::npos ? std::string() : line.value.substr(line.value.find_first_not_of(" \t", space));
            label += " " + index;
        }
        if (shown.size() >= 2 && shown[0] == '"' && shown[shown.size() - 1] == '"')
            shown = shown.substr(1, shown.size() - 2);
        if (_stricmp(line.property.c_str(), "modifiedByUser") == 0)
        {
            ImGui::TextDisabled("%s  %s", label.c_str(), shown.c_str());
            ImGui::PopID();
            continue;
        }
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer), "%s", shown.c_str());
        ImGui::SetNextItemWidth(-ImGui::CalcTextSize(label.c_str()).x - 16.0f);
        if (ImGui::InputText(label.c_str(), buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue) && buffer[0])
        {
            const std::string value = indexed ? index + " " + buffer : std::string(buffer);
            session.runCommand("EditorTemplate.set " + name + " " + line.property + " \"" + value + "\"", "template");
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    // A line the template does not have yet.
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##newProperty", "property", m_newProperty, sizeof(m_newProperty));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##newValue", "value", m_newValue, sizeof(m_newValue));
    ImGui::SameLine();
    if (ImGui::Button("Add") && m_newProperty[0] && m_newValue[0])
    {
        session.runCommand(std::string("EditorTemplate.set ") + name + " " + m_newProperty + " \"" + m_newValue + "\"", "template");
        m_newProperty[0] = m_newValue[0] = 0;
    }
}

// Combat areas

void SessionPanels::drawCombatAreas(EditSession& session)
{
    dockBeside("Layers");
    if (!ImGui::Begin("Combat areas", &m_showCombatAreas))
    {
        ImGui::End();
        return;
    }
    char line[256];
    bool shown = session.combatAreasShown();
    if (ImGui::Checkbox("Show in the view", &shown))
        session.runCommand(std::string("EditorView.combatAreas ") + (shown ? "1" : "0"), "combat areas");

    // The manager: its master switch and the two defaults (saved as the CombatAreaManager lines).
    bool use = false;
    float timeOutside = 0.0f, damage = 0.0f;
    if (EditSession::combatAreaManagerValues(use, timeOutside, damage))
    {
        ImGui::SeparatorText("Settings");
        if (ImGui::Checkbox("Use combat areas", &use))
        {
            std::snprintf(line, sizeof(line), "EditorCombatArea.manager use %d", use ? 1 : 0);
            session.runCommand(line, "combat areas");
        }
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputFloat("Time allowed outside (s)", &timeOutside, 1.0f, 5.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::snprintf(line, sizeof(line), "EditorCombatArea.manager timeAllowedOutside %g", timeOutside);
            session.runCommand(line, "combat areas");
        }
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputFloat("Damage per second", &damage, 1.0f, 5.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::snprintf(line, sizeof(line), "EditorCombatArea.manager damage %g", damage);
            session.runCommand(line, "combat areas");
        }
    }

    ImGui::SeparatorText("Areas");
    const std::vector<LevelObjectFile::CombatAreaData> areas = EditSession::combatAreas();
    if (areas.empty())
        ImGui::TextDisabled("The level has no combat areas.");
    for (size_t i = 0; i < areas.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "%s  (%u points, team %d)", areas[i].name.c_str(),
                      static_cast<unsigned>(areas[i].points.size()), areas[i].team);
        const bool selected = _stricmp(areas[i].name.c_str(), session.activeCombatArea().c_str()) == 0;
        if (ImGui::Selectable(line, selected))
            session.runCommand("EditorCombatArea.select " + areas[i].name + " -1", "combat areas");
    }

    // The selected area.
    LevelObjectFile::CombatAreaData area;
    if (!session.activeCombatArea().empty() && EditSession::engineCombatArea(session.activeCombatArea(), area))
    {
        ImGui::SeparatorText(area.name.c_str());
        const struct Field { const char* label; const char* property; int value; } fields[] = {
            { "Team (0 any)", "team", area.team },
            { "Vehicles (4 all)", "vehicles", area.vehicles },
            { "Layer", "layer", area.layer },
        };
        for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i)
        {
            int value = fields[i].value;
            ImGui::SetNextItemWidth(120.0f);
            if (ImGui::InputInt(fields[i].label, &value, 1, 1, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::snprintf(line, sizeof(line), "EditorCombatArea.set %s %s %d", area.name.c_str(), fields[i].property, value);
                session.runCommand(line, "combat areas");
            }
        }
        bool pathFinding = area.usedByPathFinding;
        if (ImGui::Checkbox("Used by pathfinding", &pathFinding))
        {
            std::snprintf(line, sizeof(line), "EditorCombatArea.set %s usedByPathFinding %d", area.name.c_str(), pathFinding ? 1 : 0);
            session.runCommand(line, "combat areas");
        }
        bool inverted = area.inverted;
        if (ImGui::Checkbox("Inverted (an exclusion)", &inverted))
        {
            std::snprintf(line, sizeof(line), "EditorCombatArea.set %s inverted %d", area.name.c_str(), inverted ? 1 : 0);
            session.runCommand(line, "combat areas");
        }
        ImGui::TextDisabled("%u points: drag one in the view to move it, Alt+click an edge to add one, Delete",
                            static_cast<unsigned>(area.points.size()));
        ImGui::TextDisabled("removes the selected one.");
        ImGui::BeginChild("areaPoints", ImVec2(0, 150.0f), ImGuiChildFlags_Borders);
        for (size_t i = 0; i < area.points.size(); ++i)
        {
            std::snprintf(line, sizeof(line), "%u:  %.1f / %.1f", static_cast<unsigned>(i), area.points[i].first,
                          area.points[i].second);
            if (ImGui::Selectable(line, static_cast<int>(i) == session.activeCombatPoint()))
            {
                std::snprintf(line, sizeof(line), "EditorCombatArea.select %s %u", area.name.c_str(), static_cast<unsigned>(i));
                session.runCommand(line, "combat areas");
            }
        }
        ImGui::EndChild();
        if (ImGui::Button("Remove point") && session.activeCombatPoint() >= 0)
        {
            std::snprintf(line, sizeof(line), "EditorCombatArea.removePoint %s %d", area.name.c_str(), session.activeCombatPoint());
            session.runCommand(line, "combat areas");
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete area"))
            session.runCommand("EditorCombatArea.delete " + area.name, "combat areas");
    }

    ImGui::SeparatorText("New area");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Name##newArea", m_newAreaName, sizeof(m_newAreaName));
    ImGui::SetNextItemWidth(120.0f);
    ImGui::InputFloat("Size (m)", &m_newAreaSize, 10.0f, 50.0f, "%.0f");
    if (ImGui::Button("Create at the view centre") && m_newAreaName[0])
    {
        std::snprintf(line, sizeof(line), "EditorCombatArea.createHere %s %g", m_newAreaName, m_newAreaSize);
        session.runCommand(line, "combat areas");
    }
    ImGui::End();
}

// Gameplay layers

namespace
{

// "gpm_cq 16" -> "cq 16", for a narrow column.
std::string shortLayerName(const LayerSummary& layer)
{
    std::string mode = layer.mode;
    if (mode.size() > 4 && _strnicmp(mode.c_str(), "gpm_", 4) == 0)
        mode = mode.substr(4);
    return mode + " " + std::to_string(layer.size);
}

ImVec4 teamColour(int team)
{
    return team == 1 ? ImVec4(1.0f, 0.42f, 0.35f, 1.0f) : team == 2 ? ImVec4(0.4f, 0.62f, 1.0f, 1.0f)
                                                         : ImVec4(1.0f, 0.85f, 0.35f, 1.0f);
}

} // namespace

void SessionPanels::drawGameplayLayers(EditSession& session)
{
    dockBeside("Layers");
    if (!ImGui::Begin("Gameplay layers", &m_showGameplayLayers))
    {
        ImGui::End();
        return;
    }
    const std::vector<LayerSummary>& layers = session.layerSummaries();
    ImGui::TextWrapped("Each game mode and player count is a layer of its own, with its own control points, spawners, "
                       "spawn points and combat areas. The editor edits the open one (*); Open starts the editor again on "
                       "another. Show draws another layer's control points and combat areas beside the open one (white, in "
                       "the view and on the minimap).");
    const LayerSummary* compared = session.compareLayer();
    char line[256];
    const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("gameplayLayers", 7, flags))
    {
        ImGui::TableSetupColumn("Layer");
        ImGui::TableSetupColumn("Control points");
        ImGui::TableSetupColumn("Spawners");
        ImGui::TableSetupColumn("Spawn points");
        ImGui::TableSetupColumn("Combat areas");
        ImGui::TableSetupColumn("Show");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (size_t l = 0; l < layers.size(); ++l)
        {
            const LayerSummary& layer = layers[l];
            const bool open = session.isOpenLayer(layer.mode, layer.size);
            ImGui::PushID(static_cast<int>(l));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%s%s %d", open ? "* " : "", layer.mode.c_str(), layer.size);
            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(layer.controlPoints.size()));
            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(layer.spawners.size()));
            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(layer.spawnPoints.size()));
            ImGui::TableNextColumn();
            ImGui::Text("%u", static_cast<unsigned>(layer.combatAreas.size()));
            ImGui::TableNextColumn();
            if (!open)
            {
                const bool shown = compared && compared->mode == layer.mode && compared->size == layer.size;
                if (ImGui::RadioButton("##show", shown))
                {
                    std::snprintf(line, sizeof(line), shown ? "EditorGameplayLayers.compareNone"
                                                            : "EditorGameplayLayers.compare %s %d", layer.mode.c_str(), layer.size);
                    session.runCommand(line, "layers");
                }
            }
            ImGui::TableNextColumn();
            if (!open && ImGui::SmallButton("Open"))
            {
                std::snprintf(line, sizeof(line), "EditorLevel.switchLayer %s %d", layer.mode.c_str(), layer.size);
                session.runCommand(line, "layers");
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (compared)
    {
        std::snprintf(line, sizeof(line), "Stop showing %s %d", compared->mode.c_str(), compared->size);
        if (ImGui::Button(line))
            session.runCommand("EditorGameplayLayers.compareNone", "layers");
    }

    // Which layers have a control point at each place, and its team there.
    ImGui::SeparatorText("Control points in each layer");
    const std::vector<ControlPointRow> rows = matchControlPoints(layers, EditSession::kLayerMergeDistance);
    if (!layers.empty() && ImGui::BeginTable("controlPointMatrix", 1 + static_cast<int>(layers.size()), flags))
    {
        ImGui::TableSetupColumn("Place");
        for (size_t l = 0; l < layers.size(); ++l)
        {
            const std::string name = (session.isOpenLayer(layers[l].mode, layers[l].size) ? "*" : "") + shortLayerName(layers[l]);
            ImGui::TableSetupColumn(name.c_str());
        }
        ImGui::TableHeadersRow();
        for (size_t r = 0; r < rows.size(); ++r)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(rows[r].label.c_str());
            for (size_t l = 0; l < layers.size(); ++l)
            {
                ImGui::TableNextColumn();
                if (rows[r].point[l] < 0)
                {
                    ImGui::TextDisabled("-");
                    continue;
                }
                const LayerControlPoint& point = layers[l].controlPoints[rows[r].point[l]];
                ImGui::TextColored(teamColour(point.team), point.team ? "team %d" : "neutral", point.team);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s\nid %d, radius %g m\nat %.0f / %.0f / %.0f", point.name.c_str(), point.id,
                                      point.radius, point.position[0], point.position[1], point.position[2]);
            }
        }
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Combat areas");
    for (size_t l = 0; l < layers.size(); ++l)
    {
        for (size_t a = 0; a < layers[l].combatAreas.size(); ++a)
        {
            const LevelObjectFile::CombatAreaData& area = layers[l].combatAreas[a];
            ImGui::BulletText("%s %d: %s - %u points, team %d, vehicles %d%s%s", layers[l].mode.c_str(), layers[l].size,
                              area.name.c_str(), static_cast<unsigned>(area.points.size()), area.team, area.vehicles,
                              area.usedByPathFinding ? ", pathfinding" : "", area.inverted ? ", inverted" : "");
        }
    }
    ImGui::End();
}

// Level settings

namespace
{

// "a/b/c" -> up to four floats; the count read.
int readNumbers(const std::string& text, float out[4])
{
    int count = 0;
    size_t start = 0;
    while (count < 4)
    {
        const size_t slash = text.find('/', start);
        out[count++] = static_cast<float>(std::atof(text.substr(start, slash == std::string::npos ? std::string::npos
                                                                                                  : slash - start).c_str()));
        if (slash == std::string::npos)
            break;
        start = slash + 1;
    }
    return count;
}

std::string writeNumbers(const float* values, int count)
{
    std::string out;
    char text[32];
    for (int i = 0; i < count; ++i)
    {
        std::snprintf(text, sizeof(text), "%g", values[i]);
        out += (i ? "/" : "") + std::string(text);
    }
    return out;
}

} // namespace

void SessionPanels::drawLevelSettings(EditSession& session)
{
    dockBeside("Properties");
    if (!ImGui::Begin("Level settings", &m_showLevelSettings))
    {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("Init.con, Sky.con and Water.con, edited in place and saved with the level (Ctrl+S).");
    const std::vector<SettingSpec>& specs = levelSettingSpecs();
    std::string group;
    bool open = false;
    for (size_t i = 0; i < specs.size(); ++i)
    {
        const SettingSpec& spec = specs[i];
        if (group != spec.group)
        {
            group = spec.group;
            open = ImGui::CollapsingHeader(spec.group);
        }
        if (!open)
            continue;
        std::string value, error;
        const bool set = session.levelSetting(spec.key, value, error);
        ImGui::PushID(spec.key);
        std::string commit;
        bool commitNow = false;
        char buffer[256];
        char second[256];
        float numbers[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        ImGui::SetNextItemWidth(260.0f);
        switch (spec.kind)
        {
        case SettingKind::Quoted:
        case SettingKind::Word:
            std::snprintf(buffer, sizeof(buffer), "%s", value.c_str());
            if (ImGui::InputText(spec.label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                commit = buffer;
                commitNow = true;
            }
            break;
        case SettingKind::QuotedPair:
        {
            const size_t space = value.find(' ');
            std::snprintf(buffer, sizeof(buffer), "%s", value.substr(0, space).c_str());
            std::snprintf(second, sizeof(second), "%s", space == std::string::npos ? "" : value.substr(space + 1).c_str());
            ImGui::SetNextItemWidth(128.0f);
            bool changed = ImGui::InputText("##kit", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(128.0f);
            changed = ImGui::InputText(spec.label, second, sizeof(second), ImGuiInputTextFlags_EnterReturnsTrue) || changed;
            if (changed)
            {
                commit = std::string(buffer) + " " + second;
                commitNow = true;
            }
            break;
        }
        case SettingKind::Integer:
        {
            int number = std::atoi(value.c_str());
            if (ImGui::InputInt(spec.label, &number, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
            {
                commit = std::to_string(number);
                commitNow = true;
            }
            break;
        }
        case SettingKind::Number:
        {
            float number = static_cast<float>(std::atof(value.c_str()));
            if (ImGui::InputFloat(spec.label, &number, 0.0f, 0.0f, "%g", ImGuiInputTextFlags_EnterReturnsTrue))
            {
                commit = writeNumbers(&number, 1);
                commitNow = true;
            }
            break;
        }
        case SettingKind::Numbers:
        {
            readNumbers(value, numbers);
            const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
            const bool changed = spec.components == 2 ? ImGui::InputFloat2(spec.label, numbers, "%g", flags)
                               : spec.components == 3 ? ImGui::InputFloat3(spec.label, numbers, "%g", flags)
                                                      : ImGui::InputFloat4(spec.label, numbers, "%g", flags);
            if (changed)
            {
                commit = writeNumbers(numbers, spec.components);
                commitNow = true;
            }
            break;
        }
        case SettingKind::Color:
        case SettingKind::Color255:
        {
            // A colour is committed when the picker lets go, not on every frame of a drag.
            readNumbers(value, numbers);
            const float scale = spec.kind == SettingKind::Color255 ? 255.0f : 1.0f;
            float colour[3] = { numbers[0] / scale, numbers[1] / scale, numbers[2] / scale };
            ImGui::ColorEdit3(spec.label, colour, ImGuiColorEditFlags_Float);
            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                for (int k = 0; k < 3; ++k)
                    numbers[k] = colour[k] * scale;
                commit = writeNumbers(numbers, 3);
                commitNow = true;
            }
            break;
        }
        case SettingKind::Camera:
        {
            const size_t space = value.find(' ');
            float position[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, rotation[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            readNumbers(value.substr(0, space), position);
            if (space != std::string::npos)
                readNumbers(value.substr(space + 1), rotation);
            bool changed = ImGui::InputFloat3("Position", position, "%g", ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SetNextItemWidth(260.0f);
            changed = ImGui::InputFloat3("Yaw / pitch / roll", rotation, "%g", ImGuiInputTextFlags_EnterReturnsTrue) || changed;
            if (changed)
            {
                commit = writeNumbers(position, 3) + " " + writeNumbers(rotation, 3);
                commitNow = true;
            }
            if (ImGui::Button("From the view"))
                session.runCommand("EditorLevelSettings.spawnCameraFromView", "settings");
            ImGui::SameLine();
            ImGui::TextDisabled("%s", spec.label);
            break;
        }
        }
        if (!set)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(not in %s)", spec.file);
        }
        if (commitNow)
            session.runCommand("EditorLevelSettings.set " + std::string(spec.key) + " \"" + commit + "\"", "settings");
        ImGui::PopID();
        // After the teams' languages: a team picked from the retail team list sets its name and language
        // together.
        if (std::strcmp(spec.key, "team2.language") == 0)
        {
            const std::vector<TeamDefinition>& teams = session.levelEditorLists().teams();
            for (int t = 1; t <= 2 && !teams.empty(); ++t)
            {
                char label[48];
                std::snprintf(label, sizeof(label), "Team %d from the team list", t);
                ImGui::SetNextItemWidth(260.0f);
                if (ImGui::BeginCombo(label, "pick a team"))
                {
                    for (size_t k = 0; k < teams.size(); ++k)
                    {
                        const std::string item = teams[k].name + " (" + teams[k].language + ")";
                        if (ImGui::Selectable(item.c_str()))
                        {
                            char line[160];
                            std::snprintf(line, sizeof(line), "EditorLevelSettings.team %d %s", t, teams[k].name.c_str());
                            session.runCommand(line, "settings");
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        }
    }
    ImGui::End();
}

// Sounds

void SessionPanels::drawSounds(EditSession& session)
{
    dockBeside("Layers");
    if (!ImGui::Begin("Sounds", &m_showSounds))
    {
        ImGui::End();
        return;
    }
    char line[512];
    const std::vector<EditSession::SoundRow> rows = session.sounds();
    ImGui::TextDisabled("%u ambient sounds (AmbientObjects.con). Drag a marker in the view to move one.",
                        static_cast<unsigned>(rows.size()));
    ImGui::BeginChild("soundList", ImVec2(0, 150.0f), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < rows.size(); ++i)
    {
        std::snprintf(line, sizeof(line), "%s%s", rows[i].name.c_str(), rows[i].positioned ? "" : "  (everywhere)");
        if (ImGui::Selectable(line, _stricmp(rows[i].name.c_str(), session.activeSound().c_str()) == 0))
            session.runCommand("EditorSound.select " + rows[i].name, "sound");
    }
    ImGui::EndChild();

    ImGui::SeparatorText("New sound");
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputText("Name##newSound", m_newSoundName, sizeof(m_newSoundName));
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("File##newSound", "common/sound/levelambients/<level>/<sound>.ogg", m_newSoundFile,
                             sizeof(m_newSoundFile));
    if (ImGui::Button("Create at the view centre##sound") && m_newSoundName[0] && m_newSoundFile[0])
    {
        std::snprintf(line, sizeof(line), "EditorSound.createHere \"%s\" \"%s\"", m_newSoundName, m_newSoundFile);
        session.runCommand(line, "sound");
    }

    // The selected sound: where it plays, and its template's lines.
    EditSession::SoundRow row;
    LevelDocument* document = session.document();
    size_t file = 0;
    if (!session.activeSound().empty() && session.soundRow(session.activeSound(), row) && document
        && document->ambientFile(file))
    {
        ImGui::SeparatorText(row.name.c_str());
        if (row.positioned)
        {
            float position[3] = { row.position[0], row.position[1], row.position[2] };
            if (ImGui::InputFloat3("Position", position, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::snprintf(line, sizeof(line), "EditorSound.move %s %.3f/%.3f/%.3f", row.name.c_str(), position[0],
                              position[1], position[2]);
                session.runCommand(line, "sound");
            }
        }
        else
            ImGui::TextDisabled("It plays everywhere (no position).");
        if (ImGui::Button("Delete sound"))
            session.runCommand("EditorSound.delete " + row.name, "sound");
        drawLevelTemplate(session, *document, file, row.name);
    }
    ImGui::End();
}

// Objects

void SessionPanels::refreshObjects()
{
    m_rows.clear();
    m_rowsStale = false;
    if (!dice::hfe::world::objectManager)
        return;
    // Root objects in the grid (ObjectManager::getAllObjectsFromGrid: no parent, OFHasGrid), in id
    // order - what the level placed.
    const std::map<unsigned int, IObject*>& objects = dice::hfe::world::objectManager->getAllObjectsFromGrid();
    for (std::map<unsigned int, IObject*>::const_iterator it = objects.begin(); it != objects.end(); ++it)
    {
        ObjectRow row;
        row.id = it->first;
        row.label = EditSession::describe(it->first);
        row.lowered = lowered(row.label);
        row.classId = it->second && it->second->getTemplate() ? it->second->getTemplate()->getClassID() : 0;
        row.className = row.classId ? EditSession::className(row.classId) : std::string("-");
        row.layer = it->second ? static_cast<int>(it->second->getLayer()) : 0;
        m_rows.push_back(row);
    }
}

void SessionPanels::drawObjects(EditSession& session)
{
    dockBeside("Resources");
    if (!ImGui::Begin("Objects", &m_showObjects))
    {
        ImGui::End();
        return;
    }
    // An edit may have moved an object to another layer; the rows are read again after one.
    if (m_rowsStale || m_rowsRevision != session.history().revision())
    {
        refreshObjects();
        m_rowsRevision = session.history().revision();
    }
    // A new object of a template, where the middle of the 3D view looks (EditorObject.create).
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("##create", "template to create, e.g. stonefence_l", m_createTemplate,
                             sizeof(m_createTemplate));
    ImGui::SameLine();
    ImGui::BeginDisabled(m_createTemplate[0] == 0);
    if (ImGui::Button("Create"))
    {
        float at[3];
        session.pointAtViewCentre(at);
        char line[256];
        std::snprintf(line, sizeof(line), "EditorObject.create %s %.3f/%.3f/%.3f 0/0/0 1", m_createTemplate, at[0],
                      at[1], at[2]);
        submit(session, line, "objects");
    }
    ImGui::EndDisabled();

    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputTextWithHint("##filter", "filter by template name or #id", m_filter, sizeof(m_filter));
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        refreshObjects();

    // The class filter: every class the level uses.
    const std::vector<EditSession::ClassRow>& classes = session.classRows();
    std::string current = "every class";
    for (size_t i = 0; i < classes.size(); ++i)
        if (classes[i].classId == m_classFilter)
            current = classes[i].name;
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##class", current.c_str()))
    {
        if (ImGui::Selectable("every class", m_classFilter == 0))
            m_classFilter = 0;
        for (size_t i = 0; i < classes.size(); ++i)
        {
            char label[128];
            std::snprintf(label, sizeof(label), "%s (%d)", classes[i].name.c_str(), classes[i].objects);
            if (ImGui::Selectable(label, m_classFilter == classes[i].classId))
                m_classFilter = classes[i].classId;
        }
        ImGui::EndCombo();
    }

    const std::string filter = lowered(m_filter);
    std::vector<int> shown;
    shown.reserve(m_rows.size());
    for (size_t i = 0; i < m_rows.size(); ++i)
        if ((filter.empty() || m_rows[i].lowered.find(filter) != std::string::npos)
            && (m_classFilter == 0 || m_rows[i].classId == m_classFilter))
            shown.push_back(static_cast<int>(i));
    ImGui::TextDisabled("%u of %u objects", static_cast<unsigned>(shown.size()), static_cast<unsigned>(m_rows.size()));

    const ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV
                                | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("rows", 3, flags))
    {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("object", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("class", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn("layer", ImGuiTableColumnFlags_WidthFixed, 40.0f);
        ImGui::TableHeadersRow();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(shown.size()));
        while (clipper.Step())
        {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
            {
                const ObjectRow& row = m_rows[static_cast<size_t>(shown[static_cast<size_t>(i)])];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(row.id));
                if (ImGui::Selectable(row.label.c_str(), session.selection().contains(row.id),
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    // As in the 3D view: Ctrl toggles the object in the selection, Shift adds it.
                    const ImGuiIO& io = ImGui::GetIO();
                    std::string error;
                    if (io.KeyCtrl)
                        session.selection().toggle(row.id);
                    else if (!session.selectObject(row.id, io.KeyShift, error))
                        logLine(LogLevel::Warning, "select: %s", error.c_str());
                    if (error.empty() && !io.KeyCtrl && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)
                        && !session.frameSelection(error))
                        logLine(LogLevel::Warning, "frame: %s", error.c_str());
                }
                ImGui::PopID();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(row.className.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", row.layer);
            }
        }
        clipper.End();
        ImGui::EndTable();
    }
    ImGui::End();
}

// Layers

void SessionPanels::drawLayers(EditSession& session)
{
    dockBeside("History");
    if (!ImGui::Begin("Layers", &m_showLayers))
    {
        ImGui::End();
        return;
    }
    {
        // The markers of objects that have no geometry (EditSession::markerKind).
        bool markers = session.markersShown();
        if (ImGui::Checkbox("Markers for spawners, spawn points, control points, triggers", &markers))
            session.runCommand(markers ? "EditorView.markers 1" : "EditorView.markers 0", "markers");
    }
    if (ImGui::Button("Recount"))
        session.markCountsStale();
    ImGui::SameLine();
    ImGui::TextDisabled("hiding and locking are the editor's view; they are not saved");

    const std::vector<EditSession::LayerRow> layers = session.layerRows();     // a copy: toggles recount
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
    if (ImGui::BeginTable("layers", 5, flags))
    {
        ImGui::TableSetupColumn("layer");
        ImGui::TableSetupColumn("objects");
        ImGui::TableSetupColumn("shown");
        ImGui::TableSetupColumn("locked");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < layers.size(); ++i)
        {
            const EditSession::LayerRow& row = layers[i];
            ImGui::PushID(row.layer);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", row.layer);
            ImGui::TableNextColumn();
            ImGui::Text("%d", row.objects);
            ImGui::TableNextColumn();
            bool shown = !row.hidden;
            std::string error;
            if (ImGui::Checkbox("##shown", &shown) && !session.setLayerHidden(row.layer, !shown, error))
                logLine(LogLevel::Warning, "layers: %s", error.c_str());
            ImGui::TableNextColumn();
            bool locked = row.locked;
            if (ImGui::Checkbox("##locked", &locked) && !session.setLayerLocked(row.layer, locked, error))
                logLine(LogLevel::Warning, "layers: %s", error.c_str());
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("select all") && !session.selectLayer(row.layer, error))
                logLine(LogLevel::Warning, "layers: %s", error.c_str());
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    // The retail layer model: names, the disabled flag, filters, the active layer.
    GameplayLayerModel& model = session.layerModel();
    if (model.active())
    {
        ImGui::SeparatorText("Gameplay layers (Editor/Layer.con)");
        std::string status;
        session.layersCommand("status", std::vector<std::string>(), status);
        ImGui::TextDisabled("%s", status.c_str());
        const std::vector<GameplayLayer> list = model.layers();
        char line[256];
        const GameplayLayer* active = model.layer(model.activeLayer);
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::BeginCombo("New gameplay objects go to", active ? active->name.c_str() : "?"))
        {
            for (size_t i = 0; i < list.size(); ++i)
                if (ImGui::Selectable((list[i].name + "##active" + std::to_string(list[i].id)).c_str(), list[i].id == model.activeLayer))
                {
                    std::snprintf(line, sizeof(line), "EditorLayer.active %d", list[i].id);
                    session.runCommand(line, "layers");
                }
            ImGui::EndCombo();
        }
        if (ImGui::BeginTable("gameplayLayers", 4, flags))
        {
            ImGui::TableSetupColumn("id");
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("disabled");
            ImGui::TableSetupColumn("game modes; sizes");
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < list.size(); ++i)
            {
                const GameplayLayer& l = list[i];
                ImGui::PushID(l.id + 5000);
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%d", l.id);
                ImGui::TableNextColumn();
                if (ImGui::Selectable(l.name.c_str(), m_layerEditing == l.id, ImGuiSelectableFlags_SpanAllColumns))
                {
                    m_layerEditing = l.id;
                    std::snprintf(m_layerName, sizeof(m_layerName), "%s", l.name.c_str());
                    m_layerModes.assign(model.filters().modes.size(), 0);
                    m_layerPlayers.assign(model.filters().players.size(), 0);
                    for (size_t k = 0; k < model.filters().modes.size(); ++k)
                        m_layerModes[k] = std::find(l.modes.begin(), l.modes.end(), model.filters().modes[k]) != l.modes.end();
                    for (size_t k = 0; k < model.filters().players.size(); ++k)
                        m_layerPlayers[k] = std::find(l.players.begin(), l.players.end(), model.filters().players[k]) != l.players.end();
                }
                ImGui::TableNextColumn();
                bool disabled = l.disabled;
                if (ImGui::Checkbox("##disabled", &disabled))
                {
                    std::snprintf(line, sizeof(line), "EditorLayer.disable %d %d", l.id, disabled ? 1 : 0);
                    session.runCommand(line, "layers");
                }
                ImGui::SetItemTooltip("A disabled layer's objects are left out of every game-mode file (still loaded and edited).");
                ImGui::TableNextColumn();
                std::string filters;
                for (size_t k = 0; k < l.modes.size(); ++k)
                    filters += (k ? " " : "") + l.modes[k];
                filters += ";";
                for (size_t k = 0; k < l.players.size(); ++k)
                    filters += " " + l.players[k];
                ImGui::TextDisabled("%s", l.id == 1 ? "every game mode and size" : filters.c_str());
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        const GameplayLayer* editing = model.layer(m_layerEditing);
        if (editing && editing->id != 1 && m_layerModes.size() == model.filters().modes.size()
            && m_layerPlayers.size() == model.filters().players.size())
        {
            ImGui::Text("Layer %d", editing->id);
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::InputText("Name##layerName", m_layerName, sizeof(m_layerName), ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::snprintf(line, sizeof(line), "EditorLayer.rename %d \"%s\"", editing->id, m_layerName);
                session.runCommand(line, "layers");
            }
            std::string modes, players;
            for (size_t k = 0; k < model.filters().modes.size(); ++k)
            {
                bool on = m_layerModes[k] != 0;
                if (k)
                    ImGui::SameLine();
                if (ImGui::Checkbox((model.filters().modeLabels[k] + "##mode").c_str(), &on))
                    m_layerModes[k] = on ? 1 : 0;
                if (m_layerModes[k])
                    modes += (modes.empty() ? "" : " ") + model.filters().modes[k];
            }
            for (size_t k = 0; k < model.filters().players.size(); ++k)
            {
                bool on = m_layerPlayers[k] != 0;
                if (k)
                    ImGui::SameLine();
                if (ImGui::Checkbox((model.filters().players[k] + "##players").c_str(), &on))
                    m_layerPlayers[k] = on ? 1 : 0;
                if (m_layerPlayers[k])
                    players += (players.empty() ? "" : " ") + model.filters().players[k];
            }
            if (ImGui::Button("Apply the filters"))
            {
                const std::string command = "EditorLayer.filters " + std::to_string(editing->id) + " \"" + modes + "\" \"" + players + "\"";
                session.runCommand(command, "layers");
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove (objects to the Default)"))
            {
                std::snprintf(line, sizeof(line), "EditorLayer.remove %d move", editing->id);
                session.runCommand(line, "layers");
            }
            ImGui::SameLine();
            if (ImGui::Button("Remove with its objects"))
            {
                std::snprintf(line, sizeof(line), "EditorLayer.remove %d delete", editing->id);
                session.runCommand(line, "layers");
            }
            ImGui::SetItemTooltip("Its combat areas go either way, as in the retail editor.");
        }
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##newLayer", "a new layer's name", m_newLayerName, sizeof(m_newLayerName));
        ImGui::SameLine();
        ImGui::BeginDisabled(!m_newLayerName[0]);
        if (ImGui::Button("Add a layer"))
        {
            session.runCommand(std::string("EditorLayer.add \"") + m_newLayerName + "\"", "layers");
            m_newLayerName[0] = 0;
        }
        ImGui::EndDisabled();
        ImGui::SetItemTooltip("Every game mode and size to begin with, as the retail editor's new layer.");
    }

    ImGui::Separator();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::InputInt("##target", &m_targetLayer);
    if (m_targetLayer < 0)
        m_targetLayer = 0;
    if (m_targetLayer > 255)
        m_targetLayer = 255;
    ImGui::SameLine();
    ImGui::BeginDisabled(session.selection().empty());
    if (ImGui::Button("Move the selection to this layer"))
    {
        std::string error;
        if (!session.setSelectionLayer(m_targetLayer, error))
            logLine(LogLevel::Warning, "layers: %s", error.c_str());
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextUnformatted("Pick filter: the classes a click can select");
    const std::vector<EditSession::ClassRow> classes = session.classRows();
    if (ImGui::BeginTable("classes", 3, flags))
    {
        ImGui::TableSetupColumn("class");
        ImGui::TableSetupColumn("objects");
        ImGui::TableSetupColumn("pickable");
        ImGui::TableHeadersRow();
        for (size_t i = 0; i < classes.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(classes[i].classId));
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(classes[i].name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", classes[i].objects);
            ImGui::TableNextColumn();
            bool pickable = classes[i].pickable;
            if (ImGui::Checkbox("##pickable", &pickable))
                session.setClassPickable(classes[i].classId, pickable);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// History

void SessionPanels::drawHistory(EditSession& session)
{
    dockBeside("Engine");
    if (!ImGui::Begin("History", &m_showHistory))
    {
        ImGui::End();
        return;
    }
    CommandHistory& history = session.history();
    ImGui::BeginDisabled(!history.canUndo());
    if (ImGui::Button("Undo"))
        session.runUndo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!history.canRedo());
    if (ImGui::Button("Redo"))
        session.runRedo();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(history.entries().empty());
    if (ImGui::Button("Clear"))
        session.clearHistory("the History panel's Clear button");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", history.isDirty() ? "level modified" : "level unchanged");

    ImGui::BeginChild("entries");
    // Row 0 is the level as it was before the first entry; row k stands after entry k. Clicking a
    // row undoes or redoes to it.
    size_t target = history.position();
    const size_t count = history.entries().size();
    for (size_t row = 0; row <= count; ++row)
    {
        const bool current = row == history.position();
        const bool undone = row > history.position();
        if (undone)
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
        ImGui::PushID(static_cast<int>(row));
        const std::string label = row == 0 ? std::string("(the level as opened)") : history.entries()[row - 1].label;
        if (ImGui::Selectable(label.c_str(), current))
            target = row;
        ImGui::PopID();
        if (undone)
            ImGui::PopStyleColor();
    }
    ImGui::EndChild();
    ImGui::End();

    while (target < history.position() && history.canUndo())
    {
        const size_t before = history.position();
        session.runUndo();
        if (history.position() == before)
            break;                                  // it failed; the log says why
    }
    while (target > history.position() && history.canRedo())
    {
        const size_t before = history.position();
        session.runRedo();
        if (history.position() == before)
            break;
    }
}

// console

bool SessionPanels::submit(EditSession& session, const std::string& line, const char* origin)
{
    ConsoleLine typed;
    typed.text = "> " + line;
    typed.kind = 0;
    m_consoleLines.push_back(typed);

    const ConsoleLineResult result = runConsoleLine(line);
    logLine(result.failed ? LogLevel::Error : LogLevel::Info, "%s: %s%s%s", origin, line.c_str(),
            result.output.empty() ? "" : " -> ", result.output.c_str());
    if (!result.output.empty())
    {
        ConsoleLine answer;
        answer.text = result.output;
        answer.kind = result.failed ? 2 : 1;
        m_consoleLines.push_back(answer);
    }
    if (result.unknownEffect && !session.history().entries().empty())
    {
        session.clearHistory("'" + line + "' ran in the console, and the undo history cannot know what it "
                             "changed");
        ConsoleLine note;
        note.text = "(the undo history was cleared: this line's effect on the level is unknown to it)";
        note.kind = 3;
        m_consoleLines.push_back(note);
    }
    while (m_consoleLines.size() > kConsoleLines)
        m_consoleLines.pop_front();
    m_scrollConsole = true;
    return !result.failed;
}

namespace
{

struct ConsoleHistoryState
{
    std::vector<std::string>* typed;
    int*                      cursor;
    const ConsoleCompletion*  completion;
    std::vector<std::string>* listed;       // a Tab's candidates when there were several
};

// The engine's console names for the completion, read again whenever the registry's size changes, and the
// first binding of each name (for its argument types).
struct ConsoleNames
{
    ConsoleCompletion                                   completion;
    std::map<std::string, dice::hfe::ConsoleObject*>    byName;     // lower-case full name
    size_t                                              count;
    ConsoleNames() : count(static_cast<size_t>(-1)) {}
};

std::string lowerName(std::string text)
{
    for (size_t i = 0; i < text.size(); ++i)
        if (text[i] >= 'A' && text[i] <= 'Z')
            text[i] = static_cast<char>(text[i] - 'A' + 'a');
    return text;
}

ConsoleNames& consoleNames()
{
    static ConsoleNames names;
    if (!dice::hfe::mainConsoleObjects)
        return names;
    const dice::hfe::ConsoleObjects::ConsoleObjectList& all = dice::hfe::mainConsoleObjects->getConsoleObjects();
    if (all.size() == names.count)
        return names;
    names.count = all.size();
    names.byName.clear();
    std::vector<std::string> list;
    for (dice::hfe::ConsoleObjects::ConsoleObjectList::const_iterator it = all.begin(); it != all.end(); ++it)
    {
        dice::hfe::ConsoleObject* object = *it;
        if (!object || !object->getObjectName() || !object->getFunctionName() || !*object->getObjectName())
            continue;
        const std::string name = std::string(object->getObjectName()) + "." + object->getFunctionName();
        if (names.byName.insert(std::make_pair(lowerName(name), object)).second)
            list.push_back(name);
    }
    names.completion.setNames(list);
    return names;
}

// The retail command catalogue, for the hints' descriptions (read once from the retail editor's Help folder).
const CommandCatalogue& retailCatalogue()
{
    static CommandCatalogue catalogue;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        std::string error;
        catalogue.load("bf2editor/Help/CommandDescriptions.dat", error);
    }
    return catalogue;
}

// "name (type, type)" with the binding's argument types, then its retail description when there is one.
std::string hintLine(const std::string& name, const ConsoleNames& names)
{
    std::string text = name;
    const std::map<std::string, dice::hfe::ConsoleObject*>::const_iterator it = names.byName.find(lowerName(name));
    if (it != names.byName.end() && it->second)
    {
        dice::hfe::ConsoleObject* object = it->second;
        const int arguments = object->getMaxNoArgs();
        std::string types;
        for (int i = 0; i < arguments && i < 8; ++i)
        {
            const char* type = object->getArgType(i);
            types += (i ? ", " : "") + std::string(type && *type ? type : "?");
        }
        if (!types.empty())
            text += " (" + types + ")";
    }
    const std::string* description = retailCatalogue().describe(name);
    if (description && !description->empty())
        text += "  -  " + *description;
    return text;
}

int consoleHistoryCallback(ImGuiInputTextCallbackData* data)
{
    ConsoleHistoryState* state = static_cast<ConsoleHistoryState*>(data->UserData);
    if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion && state->completion)
    {
        const ConsoleCompletion::Result r = state->completion->complete(std::string(data->Buf, data->BufTextLen), 40);
        if (r.completed)
        {
            data->DeleteChars(0, data->BufTextLen);
            data->InsertChars(0, r.text.c_str());
        }
        if (r.candidates.size() > 1)
            *state->listed = r.candidates;
        return 0;
    }
    const std::vector<std::string>& typed = *state->typed;
    int& cursor = *state->cursor;
    if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory || typed.empty())
        return 0;
    if (data->EventKey == ImGuiKey_UpArrow)
        cursor = cursor < 0 ? static_cast<int>(typed.size()) - 1 : (cursor > 0 ? cursor - 1 : 0);
    else if (data->EventKey == ImGuiKey_DownArrow && cursor >= 0)
        cursor = cursor + 1 < static_cast<int>(typed.size()) ? cursor + 1 : -1;
    data->DeleteChars(0, data->BufTextLen);
    if (cursor >= 0)
        data->InsertChars(0, typed[static_cast<size_t>(cursor)].c_str());
    return 0;
}

} // namespace

void SessionPanels::drawConsole(EditSession& session)
{
    dockBeside("Log");
    if (!ImGui::Begin("Console", &m_showConsole))
    {
        ImGui::End();
        return;
    }
    const float inputHeight = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("lines", ImVec2(0.0f, -inputHeight), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
    // The retail ConsoleUI could clear its window; Copy puts every line on the clipboard.
    if (ImGui::BeginPopupContextWindow("consoleLines"))
    {
        if (ImGui::MenuItem("Clear"))
            m_consoleLines.clear();
        if (ImGui::MenuItem("Copy all"))
        {
            std::string text;
            for (size_t i = 0; i < m_consoleLines.size(); ++i)
                text += m_consoleLines[i].text + "\n";
            ImGui::SetClipboardText(text.c_str());
        }
        if (ImGui::MenuItem("Forget the typed lines"))
        {
            m_typed.clear();
            m_typedCursor = -1;
        }
        ImGui::EndPopup();
    }
    for (size_t i = 0; i < m_consoleLines.size(); ++i)
    {
        const ConsoleLine& line = m_consoleLines[i];
        if (line.kind == 0)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.65f, 0.75f, 0.9f, 1.0f));
        else if (line.kind == 2)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
        else if (line.kind == 3)
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.35f, 1.0f));
        ImGui::TextUnformatted(line.text.c_str());
        if (line.kind != 1)
            ImGui::PopStyleColor();
    }
    if (m_scrollConsole)
    {
        ImGui::SetScrollHereY(1.0f);
        m_scrollConsole = false;
    }
    ImGui::EndChild();

    ConsoleNames& names = consoleNames();
    std::vector<std::string> listed;
    ConsoleHistoryState state = { &m_typed, &m_typedCursor, &names.completion, &listed };
    ImGui::SetNextItemWidth(-1.0f);
    const bool entered = ImGui::InputTextWithHint("##input", "console command, e.g. EditorSelection.translate 0/1/0 (Tab completes)",
                                                  m_input, sizeof(m_input),
                                                  ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory
                                                      | ImGuiInputTextFlags_CallbackCompletion,
                                                  &consoleHistoryCallback, &state);
    // A Tab with several matches lists them, as a shell does.
    if (!listed.empty())
    {
        ConsoleLine note;
        for (size_t i = 0; i < listed.size(); ++i)
            note.text += (i ? "   " : "") + listed[i];
        note.kind = 3;
        m_consoleLines.push_back(note);
        m_scrollConsole = true;
    }
    // While a line is typed: the commands it can still become, with their arguments and the retail description.
    if (ImGui::IsItemActive() && m_input[0])
    {
        const std::vector<std::string> hints = names.completion.matches(m_input, 8);
        if (!hints.empty())
        {
            const ImVec2 at = ImGui::GetItemRectMin();
            ImGui::SetNextWindowPos(at, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
            ImGui::BeginTooltip();
            for (size_t i = 0; i < hints.size(); ++i)
            {
                const std::string line = hints[i][hints[i].size() - 1] == '.' ? hints[i] : hintLine(hints[i], names);
                ImGui::TextUnformatted(line.c_str());
            }
            ImGui::EndTooltip();
        }
    }
    if (entered)
    {
        const std::string line(m_input);
        m_input[0] = 0;
        m_typedCursor = -1;
        if (!line.empty())
        {
            if (m_typed.empty() || m_typed.back() != line)
                m_typed.push_back(line);
            submit(session, line, "console");
        }
        ImGui::SetKeyboardFocusHere(-1);    // keep typing
    }
    ImGui::End();
}

} // namespace bf2editor
