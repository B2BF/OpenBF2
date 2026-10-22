// Code/Tools/BF2Editor/Framework/EditorConsole.cpp - see EditorConsole.h. Editor-only code (D3).
#include "Framework/EditorConfig.h"
#include "Framework/EditorConsole.h"
#include "Framework/Shortcuts.h"

#include "Framework/EditorLog.h"

#include "Core/ConsoleObject.h"
#include "Core/ConsoleObjectMacro.h"
#include "Core/ConsoleObjects.h"
#include "IO/Console/Console.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

namespace bf2editor
{

namespace
{

// The editor's binding list: the generated classes below add themselves to it during static
// initialisation, and registerEditorConsole hands it to the engine's registry.
const char* const kEditorConsoleTag = "bf2editor.console";

IEditorCommands* g_commands = 0;
bool             g_registered = false;
PanelHandler     g_panelHandler = 0;
ViewHandler      g_viewHandler = 0;
void*            g_viewContext = 0;
ToolsHandler     g_toolsHandler = 0;
void*            g_toolsContext = 0;
void*            g_panelContext = 0;
ITestHarness*    g_testHarness = 0;
IPlayTestCommands* g_playTest = 0;
IHudCommands* g_hud = 0;
// Set by an editor command that failed while the current line ran; runConsoleScript and
// runConsoleLine clear it before each line.
std::string      g_lastError;

// The editor commands' common tail: "" on success, the reason (also kept in g_lastError) on failure.
std::string outcome(bool ok, const std::string& error)
{
    if (ok)
        return std::string();
    g_lastError = error.empty() ? std::string("failed") : error;
    return g_lastError;
}

std::string trimmed(const std::string& text)
{
    size_t begin = 0;
    size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])))
        ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    return text.substr(begin, end - begin);
}

std::string objectActivate(int id)
{
    std::string error;
    return outcome(g_commands->activateObject(static_cast<unsigned>(id), error), error);
}

std::string selectionClear()
{
    std::string error;
    return outcome(g_commands->selectObject(0, false, error), error);
}

std::string selectionSelect(const std::string& reference)
{
    std::string error;
    unsigned id = 0;
    if (!g_commands->resolveObject(reference, id, error))
        return outcome(false, error);
    return outcome(g_commands->selectObject(id, false, error), error);
}

std::string selectionAdd(const std::string& reference)
{
    std::string error;
    unsigned id = 0;
    if (!g_commands->resolveObject(reference, id, error))
        return outcome(false, error);
    return outcome(g_commands->selectObject(id, true, error), error);
}

// Answers what it hit (a successful query answers with text, unlike the edit commands).
std::string selectionPickAt(float x, float y)
{
    std::string result;
    if (!g_commands->pickAt(x, y, result))
        return outcome(false, result);
    return result;
}

std::string selectionPickAdd(float x, float y)
{
    std::string result;
    if (!g_commands->pickAdd(x, y, result))
        return outcome(false, result);
    return result;
}

std::string selectionSelectTemplate(const std::string& name)
{
    std::string result;
    if (!g_commands->selectTemplate(name, result))
        return outcome(false, result);
    return result;
}

std::string selectionInfo()
{
    std::string result;
    if (!g_commands->selectionInfo(result))
        return outcome(false, result);
    return result;
}

std::string selectionTranslate(const dice::hfe::Vec3& delta)
{
    std::string error;
    return outcome(g_commands->translateSelection(delta, error), error);
}

std::string selectionRotate(const dice::hfe::Vec3& delta)
{
    std::string error;
    return outcome(g_commands->rotateSelection(delta, error), error);
}

std::string selectionSetPosition(const dice::hfe::Vec3& position)
{
    std::string error;
    return outcome(g_commands->setSelectionPosition(position, error), error);
}

std::string selectionSetRotation(const dice::hfe::Vec3& yawPitchRoll)
{
    std::string error;
    return outcome(g_commands->setSelectionRotation(yawPitchRoll, error), error);
}

// The four-argument bindings return nothing (the tree's macro shape); a failure is reported through
// g_lastError, which the console runners check.
void objectCreate(const std::string& templateName, const dice::hfe::Vec3& position,
                  const dice::hfe::Vec3& yawPitchRoll, int layer)
{
    std::string result;
    if (!g_commands->createObject(templateName, position, yawPitchRoll, layer, result))
        outcome(false, result);
}

void objectRecreate(int originalId, const std::string& templateName, const dice::hfe::Mat4& transform, int layer)
{
    std::string error;
    if (!g_commands->recreateObject(static_cast<unsigned>(originalId), templateName, transform, layer, error))
        outcome(false, error);
}

std::string objectRemove(int id)
{
    std::string error;
    return outcome(g_commands->removeObject(static_cast<unsigned>(id), error), error);
}

// Answers with what it did (the edit's label), like pickAt.
std::string selectionDelete()
{
    std::string result;
    if (!g_commands->deleteSelection(result))
        return outcome(false, result);
    return result;
}

std::string objectPlaceAt(const std::string& templateName, float x, float y)
{
    std::string result;
    if (!g_commands->placeObjectAt(templateName, x, y, result))
        return outcome(false, result);
    return result;
}

std::string selectionDuplicate()
{
    std::string result;
    if (!g_commands->duplicateSelection(result))
        return outcome(false, result);
    return result;
}

std::string selectionSnapToGround()
{
    std::string result;
    if (!g_commands->snapSelectionToGround(result))
        return outcome(false, result);
    return result;
}

// The four-argument box bindings return nothing (the tree's macro shape): the count goes to the log,
// a refusal to g_lastError.
void selectionBoxIn(int mode, float x0, float y0, float x1, float y1)
{
    std::string result;
    if (!g_commands->selectInBox(x0, y0, x1, y1, mode, result))
        outcome(false, result);
    else
        logLine(LogLevel::Info, "select: %s", result.c_str());
}

void selectionBox(float x0, float y0, float x1, float y1)       { selectionBoxIn(0, x0, y0, x1, y1); }
void selectionBoxAdd(float x0, float y0, float x1, float y1)    { selectionBoxIn(1, x0, y0, x1, y1); }
void selectionBoxToggle(float x0, float y0, float x1, float y1) { selectionBoxIn(2, x0, y0, x1, y1); }

std::string selectionReplace(const std::string& templateName)
{
    std::string result;
    if (!g_commands->replaceSelection(templateName, result))
        return outcome(false, result);
    return result;
}

std::string selectionCopyTransform()
{
    std::string result;
    if (!g_commands->copyTransform(result))
        return outcome(false, result);
    return result;
}

std::string selectionPasteTransform()
{
    std::string result;
    if (!g_commands->pasteTransform(result))
        return outcome(false, result);
    return result;
}

std::string selectionSnapToGrid(float step)
{
    std::string result;
    if (!g_commands->snapSelectionToGrid(step, result))
        return outcome(false, result);
    return result;
}

std::string clipboardCopy()
{
    std::string result;
    if (!g_commands->copySelection(result))
        return outcome(false, result);
    return result;
}

std::string clipboardCut()
{
    std::string result;
    if (!g_commands->cutSelection(result))
        return outcome(false, result);
    return result;
}

std::string clipboardPaste()
{
    std::string result;
    if (!g_commands->pasteClipboard(dice::hfe::Vec3(), false, result))
        return outcome(false, result);
    return result;
}

std::string clipboardPasteAt(const dice::hfe::Vec3& at)
{
    std::string result;
    if (!g_commands->pasteClipboard(at, true, result))
        return outcome(false, result);
    return result;
}

std::string selectionSetProperty(const std::string& name, const std::string& value)
{
    std::string result;
    if (!g_commands->setSelectionProperty(name, value, result))
        return outcome(false, result);
    return result;
}

std::string selectionSetLayer(int layer)
{
    std::string error;
    return outcome(g_commands->setSelectionLayer(layer, error), error);
}

std::string layerHide(int layer)
{
    std::string error;
    return outcome(g_commands->setLayerHidden(layer, true, error), error);
}

std::string layerShow(int layer)
{
    std::string error;
    return outcome(g_commands->setLayerHidden(layer, false, error), error);
}

std::string layerLock(int layer)
{
    std::string error;
    return outcome(g_commands->setLayerLocked(layer, true, error), error);
}

std::string layerUnlock(int layer)
{
    std::string error;
    return outcome(g_commands->setLayerLocked(layer, false, error), error);
}

std::string layerSelect(int layer)
{
    std::string error;
    return outcome(g_commands->selectLayer(layer, error), error);
}

std::string historyUndo()
{
    std::string error;
    return outcome(g_commands->undo(error), error);
}

std::string historyRedo()
{
    std::string error;
    return outcome(g_commands->redo(error), error);
}

std::string historyClear()
{
    g_commands->clearHistory("EditorHistory.clear");
    return std::string();
}

std::string cameraFrameSelection()
{
    std::string error;
    return outcome(g_commands->frameSelection(error), error);
}

std::string cameraMode(int mode)
{
    std::string result;
    if (!g_commands->setCameraMode(mode, result))
        return outcome(false, result);
    return result;
}

std::string cameraSetPosition(const dice::hfe::Vec3& position)
{
    std::string result;
    if (!g_commands->setCameraPosition(position, result))
        return outcome(false, result);
    return result;
}

std::string cameraResetPosition()
{
    std::string result;
    if (!g_commands->resetCameraPosition(result))
        return outcome(false, result);
    return result;
}

std::string cameraSetSpeed(float speed)
{
    std::string result;
    if (!g_commands->setCameraSpeed(speed, result))
        return outcome(false, result);
    return result;
}

std::string cameraInfo()
{
    std::string result;
    if (!g_commands->cameraInfo(result))
        return outcome(false, result);
    return result;
}

std::string cameraResetSpeed()
{
    std::string result;
    if (!g_commands->setCameraSpeed(-1.0f, result))
        return outcome(false, result);
    return result;
}

std::string gizmoMode(int mode)
{
    std::string error;
    return outcome(g_commands->setGizmoMode(mode, error), error);
}

std::string gizmoSpace(int space)
{
    std::string error;
    return outcome(g_commands->setGizmoSpace(space, error), error);
}

// Both answer with text on success too: what the file holds or what the save wrote.
std::string levelStatus()
{
    std::string result;
    if (!g_commands->levelStatus(result))
        return outcome(false, result);
    return result;
}

std::string levelSave()
{
    std::string result;
    if (!g_commands->saveLevel(result))
        return outcome(false, result);
    return result;
}

std::string levelSaveAs(const std::string& name, int open)
{
    std::string result;
    if (open != 0 && open != 1)
        return outcome(false, "EditorLevel.saveAs takes a level name and 0 or 1 (open the copy)");
    if (!g_commands->saveLevelAs(name, open != 0, result))
        return outcome(false, result);
    return result;
}

std::string levelCreate(const std::string& name, const std::string& terrain, int open)
{
    std::string result;
    if (open != 0 && open != 1)
        return outcome(false, "EditorLevel.create takes a level name, size/scale/height/lowResSize (or default) and 0 or 1 "
                              "(open the new level)");
    if (!g_commands->createLevel(name, terrain, open != 0, result))
        return outcome(false, result);
    return result;
}

std::string levelPack(const std::string& modFolder)
{
    std::string result;
    if (!g_commands->packLevel(modFolder, result))
        return outcome(false, result);
    return result;
}

std::string testWait(int frames)
{
    std::string error;
    return outcome(g_testHarness->wait(frames, error), error);
}

std::string testMouseMove(float x, float y)
{
    std::string error;
    return outcome(g_testHarness->mouseMove(x, y, error), error);
}

std::string testMouseBy(float dx, float dy)
{
    std::string error;
    return outcome(g_testHarness->mouseBy(dx, dy, error), error);
}

// Answers where the mouse went.
std::string testMouseToObject(const std::string& reference)
{
    std::string result;
    unsigned id = 0;
    if (!g_commands || !g_commands->resolveObject(reference, id, result))
        return outcome(false, g_commands ? result : std::string("no level is open"));
    if (!g_testHarness->mouseToObject(id, result))
        return outcome(false, result);
    return result;
}

// Answers where the mouse went.
std::string testMouseToHud(const std::string& node, int which)
{
    float x = 0.0f, y = 0.0f;
    std::string error;
    if (!g_hud)
        return outcome(false, "the HUD is not up (+hud 1)");
    if (!g_hud->hudScreenPoint(node, which, x, y, error) || !g_testHarness->mouseMove(x, y, error))
        return outcome(false, error);
    char text[96];
    std::snprintf(text, sizeof(text), "the mouse at %.0f, %.0f", x, y);
    return text;
}

std::string testMouseToPart(int index)
{
    std::string result;
    if (!g_testHarness->mouseToPart(index, result))
        return outcome(false, result);
    return result;
}

// Answers where the mouse went.
std::string testMouseToAxis(int axis, float pixels)
{
    std::string result;
    if (!g_testHarness->mouseToAxis(axis, pixels, result))
        return outcome(false, result);
    return result;
}

std::string testMouseButton(int button, int down)
{
    std::string error;
    if (down != 0 && down != 1)
        return outcome(false, "EditorTest.mouseButton takes a button and 0 (up) or 1 (down)");
    return outcome(g_testHarness->mouseButton(button, down != 0, error), error);
}

std::string testMouseWheel(float steps)
{
    std::string error;
    return outcome(g_testHarness->mouseWheel(steps, error), error);
}

std::string testKey(const std::string& name, int down)
{
    std::string error;
    if (down != 0 && down != 1)
        return outcome(false, "EditorTest.key takes a key name and 0 (up) or 1 (down)");
    return outcome(g_testHarness->key(name, down != 0, error), error);
}

std::string testMouseToAreaPoint(const std::string& area, int index)
{
    std::string result;
    const bool ok = g_testHarness->mouseToAreaPoint(area, index, result);
    return ok ? result : outcome(false, result);
}

std::string testMouseToAreaEdge(const std::string& area, int index)
{
    std::string result;
    const bool ok = g_testHarness->mouseToAreaEdge(area, index, result);
    return ok ? result : outcome(false, result);
}

std::string testMouseToSound(const std::string& name)
{
    std::string result;
    const bool ok = g_testHarness->mouseToSound(name, result);
    return ok ? result : outcome(false, result);
}

std::string testMouseToGround(float x, float z)
{
    std::string result;
    const bool ok = g_testHarness->mouseToGround(x, z, result);
    return ok ? result : outcome(false, result);
}

std::string testMouseToMinimap(float x, float z)
{
    std::string result;
    const bool ok = g_testHarness->mouseToMinimap(x, z, result);
    return ok ? result : outcome(false, result);
}

std::string testMouseToItem(const std::string& item)
{
    std::string result;
    const bool ok = g_testHarness->mouseToItem(item, result);
    return ok ? result : outcome(false, result);
}

std::string testTypeText(const std::string& text)
{
    std::string error;
    return outcome(g_testHarness->typeText(text, error), error);
}

std::string playTestSoldier()
{
    std::string result;
    return g_playTest->enterSoldierMode(result) ? result : outcome(false, result);
}

std::string playTestLeave()
{
    std::string result;
    return g_playTest->leaveSoldierMode(result) ? result : outcome(false, result);
}

std::string playTestInfo()
{
    return g_playTest->describePlayTest();
}

std::string playTestKit(int team, int kit)
{
    std::string result;
    return g_playTest->setPlayTestKit(team, kit, result) ? result : outcome(false, result);
}

std::string playTestWeapon(const std::string& name)
{
    std::string result;
    return g_playTest->testPlayTestWeapon(name, result) ? result : outcome(false, result);
}

std::string hudInfo()
{
    return g_hud->describeHud();
}

std::string hudPreview(int on)
{
    std::string result;
    return g_hud->setHudPreview(on != 0, result) ? result : outcome(false, result);
}

std::string hudSet(const std::string& name, const std::string& value)
{
    std::string result;
    return g_hud->setHudVariable(name, value, result) ? result : outcome(false, result);
}

std::string hudSelect(const std::string& node)
{
    std::string result;
    return g_hud->selectHudNode(node, result) ? result : outcome(false, result);
}

std::string hudNode(const std::string& node)
{
    return g_hud->describeHudNode(node);
}

std::string hudRect(const std::string& node, const std::string& rect)
{
    int x = 0, y = 0, width = 0, height = 0;
    char tail = 0;
    if (std::sscanf(rect.c_str(), "%d/%d/%d/%d%c", &x, &y, &width, &height, &tail) != 4)
        return outcome(false, "EditorHud.rect takes a node and its rectangle as x/y/width/height");
    std::string result;
    return g_hud->setHudRect(node, x, y, width, height, result) ? result : outcome(false, result);
}

std::string hudSetRow(const std::string& node, int row, const std::string& words)
{
    std::string result;
    return g_hud->setHudRow(node, row, words, result) ? result : outcome(false, result);
}

std::string hudAddRow(const std::string& node, const std::string& method, const std::string& words)
{
    std::string result;
    return g_hud->addHudRow(node, method, words, result) ? result : outcome(false, result);
}

std::string hudRemoveRow(const std::string& node, int row)
{
    std::string result;
    return g_hud->removeHudRow(node, row, result) ? result : outcome(false, result);
}

std::string hudChanges()
{
    return g_hud->describeHudChanges();
}

// The history's rows: nothing answered on success (the history counts any answer as a failure); the note on what
// the live HUD did goes to the log.
std::string hudSetLine(int script, int line, const std::string& text)
{
    std::string result;
    return outcome(g_hud->editHudLine(0, script, line, text, result), result);
}

std::string hudInsertLine(int script, int line, const std::string& text)
{
    std::string result;
    return outcome(g_hud->editHudLine(1, script, line, text, result), result);
}

std::string hudRemoveLine(int script, int line)
{
    std::string result;
    return outcome(g_hud->editHudLine(2, script, line, std::string(), result), result);
}

std::string playTestHold(const std::string& name, float value, int steps)
{
    std::string result;
    return g_playTest->holdPlayTestInput(name, value, steps, result) ? result : outcome(false, result);
}

std::string playTestDrive()
{
    std::string result;
    return g_playTest->drivePlayTestVehicle(std::string(), result) ? result : outcome(false, result);
}

std::string playTestDriveTemplate(const std::string& name)
{
    std::string result;
    if (name.empty())
        return outcome(false, "EditorPlayTest.driveTemplate takes a vehicle template's name");
    return g_playTest->drivePlayTestVehicle(name, result) ? result : outcome(false, result);
}

std::string testDragTemplate(const std::string& path)
{
    std::string error;
    return outcome(g_testHarness->dragTemplate(path, error), error);
}

std::string templateSet(const std::string& name, const std::string& property, const std::string& value)
{
    std::string result;
    if (!g_commands->setTemplateProperty(name, property, value, result))
        return outcome(false, result);
    return result;
}

std::string templateApply(const std::string& name, const std::string& property, const std::string& value)
{
    std::string error;
    return outcome(g_commands->applyTemplateProperty(name, property, value, error), error);
}

std::string templateRemove(const std::string& name, const std::string& key)
{
    std::string error;
    return outcome(g_commands->removeTemplateLine(name, key, error), error);
}

std::string templateLines(const std::string& name)
{
    std::string result;
    if (!g_commands->templateInfo(name, result))
        return outcome(false, result);
    return result;
}

std::string gameplayCreate(const std::string& kind, const std::string& name, const dice::hfe::Vec3& position)
{
    std::string result;
    if (!g_commands->createGameplayObject(kind, name, position, result))
        return outcome(false, result);
    return result;
}

std::string gameplayRename(const std::string& from, const std::string& to)
{
    std::string result;
    if (!g_commands->renameLevelTemplate(from, to, result))
        return outcome(false, result);
    return result;
}

// The history's replay: no answer on success.
std::string gameplayApplyRename(const std::string& from, const std::string& to)
{
    std::string error;
    return outcome(g_commands->applyTemplateRename(from, to, error), error);
}

std::string gameplayCreateHere(const std::string& kind, const std::string& name)
{
    std::string result;
    if (!g_commands->createGameplayObjectHere(kind, name, result))
        return outcome(false, result);
    return result;
}

std::string gameplayCreateEffectArea(const std::string& name, const std::string& effect, const dice::hfe::Vec3& position)
{
    std::string result;
    if (!g_commands->createEffectArea(name, effect, position, result))
        return outcome(false, result);
    return result;
}

std::string gameplayCreateEffectAreaHere(const std::string& name, const std::string& effect)
{
    std::string result;
    if (!g_commands->createEffectAreaHere(name, effect, result))
        return outcome(false, result);
    return result;
}

// A command's result when it succeeded (a query's answer, an edit's label), its error when not.
std::string answered(bool ok, const std::string& text)
{
    return ok ? text : outcome(false, text);
}

std::string combatList()
{
    std::string r;
    return answered(g_commands->listCombatAreas(r), r);
}

std::string combatInfo(const std::string& name)
{
    std::string r;
    return answered(g_commands->combatAreaInfo(name, r), r);
}

std::string combatCreate(const std::string& name, const dice::hfe::Vec3& centre, float size)
{
    std::string r;
    return answered(g_commands->createCombatArea(name, centre, size, r), r);
}

std::string combatCreateHere(const std::string& name, float size)
{
    std::string r;
    return answered(g_commands->createCombatAreaHere(name, size, r), r);
}

std::string combatDelete(const std::string& name)
{
    std::string r;
    return answered(g_commands->deleteCombatArea(name, r), r);
}

void combatMovePoint(const std::string& name, int index, float x, float z)
{
    std::string r;
    if (!g_commands->moveCombatAreaPoint(name, index, x, z, r))
        outcome(false, r);
}

void combatInsertPoint(const std::string& name, int index, float x, float z)
{
    std::string r;
    if (!g_commands->insertCombatAreaPoint(name, index, x, z, r))
        outcome(false, r);
}

std::string combatRemovePoint(const std::string& name, int index)
{
    std::string r;
    return answered(g_commands->removeCombatAreaPoint(name, index, r), r);
}

std::string combatSet(const std::string& name, const std::string& property, int value)
{
    std::string r;
    return answered(g_commands->setCombatAreaProperty(name, property, value, r), r);
}

std::string combatManager(const std::string& property, float value)
{
    std::string r;
    return answered(g_commands->setCombatAreaManager(property, value, r), r);
}

std::string combatSync(const std::string& name)
{
    std::string error;
    return outcome(g_commands->syncCombatArea(name, error), error);
}

std::string combatSyncManager()
{
    std::string error;
    return outcome(g_commands->syncCombatAreaManager(error), error);
}

std::string combatSelect(const std::string& name, int point)
{
    std::string r;
    return answered(g_commands->selectCombatArea(name, point, r), r);
}

std::string combatDeselect()
{
    std::string r;
    return answered(g_commands->selectCombatArea(std::string(), -1, r), r);
}

std::string viewMarkers(int shown)
{
    std::string error;
    if (shown != 0 && shown != 1)
        return outcome(false, "EditorView.markers takes 0 or 1");
    return outcome(g_commands->setMarkersShown(shown != 0, error), error);
}

std::string settingsList(const std::string& group)
{
    std::string r;
    return answered(g_commands->listLevelSettings(group, r), r);
}

std::string settingsGet(const std::string& key)
{
    std::string value, error;
    return g_commands->levelSetting(key, value, error) ? value : outcome(false, error);
}

std::string settingsSet(const std::string& key, const std::string& value)
{
    std::string r;
    return answered(g_commands->setLevelSetting(key, value, r), r);
}

std::string settingsApply(const std::string& key, const std::string& value)
{
    std::string error;
    return outcome(g_commands->applyLevelSetting(key, value, error), error);
}

std::string settingsUnset(const std::string& key)
{
    std::string error;
    return outcome(g_commands->unsetLevelSetting(key, error), error);
}

std::string settingsSpawnCameraFromView()
{
    std::string r;
    return answered(g_commands->spawnCameraFromView(r), r);
}

std::string soundList()
{
    std::string r;
    return answered(g_commands->listSounds(r), r);
}

std::string soundInfoLine(const std::string& name)
{
    std::string r;
    return answered(g_commands->soundInfo(name, r), r);
}

std::string soundMove(const std::string& name, const dice::hfe::Vec3& position)
{
    std::string r;
    return answered(g_commands->moveSound(name, position, r), r);
}

std::string soundCreate(const std::string& name, const std::string& file, const dice::hfe::Vec3& position)
{
    std::string r;
    return answered(g_commands->createSound(name, file, position, r), r);
}

std::string soundCreateHere(const std::string& name, const std::string& file)
{
    std::string r;
    return answered(g_commands->createSoundHere(name, file, r), r);
}

std::string soundDelete(const std::string& name)
{
    std::string r;
    return answered(g_commands->deleteSound(name, r), r);
}

std::string soundApply(const std::string& name, const dice::hfe::Vec3& position)
{
    std::string error;
    return outcome(g_commands->applySoundPosition(name, position, error), error);
}

std::string soundRemove(const std::string& name)
{
    std::string error;
    return outcome(g_commands->removeSound(name, error), error);
}

std::string soundRestore(const std::string& name)
{
    std::string error;
    return outcome(g_commands->restoreSound(name, error), error);
}

std::string soundSelect(const std::string& name)
{
    std::string r;
    return answered(g_commands->selectSound(name, r), r);
}

std::string soundDeselect()
{
    std::string r;
    return answered(g_commands->selectSound(std::string(), r), r);
}

std::string gameplayLayersOverview()
{
    std::string r;
    return answered(g_commands->layersOverview(r), r);
}

std::string gameplayLayersCompare(const std::string& gameMode, int size)
{
    std::string r;
    return answered(g_commands->setCompareLayer(gameMode, size, r), r);
}

std::string gameplayLayersCompareNone()
{
    std::string r;
    return answered(g_commands->setCompareLayer(std::string(), 0, r), r);
}

std::string levelLayers()
{
    std::string r;
    return answered(g_commands->listLayers(r), r);
}

std::string levelSwitchLayer(const std::string& gameMode, int size)
{
    std::string r;
    return answered(g_commands->switchLayer(gameMode, size, r), r);
}

std::string levelDuplicateLayer(const std::string& gameMode, int size)
{
    std::string r;
    return answered(g_commands->duplicateLayer(gameMode, size, r), r);
}

std::string viewCombatAreas(int shown)
{
    std::string error;
    if (shown != 0 && shown != 1)
        return outcome(false, "EditorView.combatAreas takes 0 or 1");
    return outcome(g_commands->setCombatAreasShown(shown != 0, error), error);
}

std::string viewPauseRenderer(int paused)
{
    return outcome(g_viewHandler && g_viewHandler(g_viewContext, paused ? "pause" : "resume"),
                   paused ? "the renderer is paused" : "the renderer runs");
}

std::string viewPauseUpdate(int paused)
{
    return outcome(g_viewHandler && g_viewHandler(g_viewContext, paused ? "pauseUpdate" : "resumeUpdate"),
                   "the view is not up");
}

std::string viewLevelViewDistance(int level)
{
    return outcome(g_viewHandler && g_viewHandler(g_viewContext, level ? "viewLevel" : "viewEditor"),
                   level ? "the level sets no view distance" : "the view is not up");
}

std::string viewWireframe(int on)
{
    return outcome(g_viewHandler && g_viewHandler(g_viewContext, on ? "wireframeOn" : "wireframeOff"), "the view is not up");
}

std::string viewRenderOneFrame()
{
    return outcome(g_viewHandler && g_viewHandler(g_viewContext, "one"), "the renderer is not paused: nothing to render once");
}

std::string viewPanel(const std::string& name, int visible)
{
    if (visible != 0 && visible != 1)
        return outcome(false, "EditorView.panel takes a panel name and 0 or 1");
    return outcome(g_panelHandler && g_panelHandler(g_panelContext, name, visible != 0), "no panel named '" + name + "'");
}

// EditorTerrain (parity phase E)
std::string terrainInfoLine()
{
    std::string r;
    return answered(g_commands->terrainInfo(r), r);
}

std::string terrainHeightAt(float x, float z)
{
    std::string r;
    return answered(g_commands->terrainHeight(x, z, r), r);
}

std::string terrainBrushSet(const std::string& key, const std::string& value)
{
    std::string r;
    return answered(g_commands->terrainSetBrush(key, value, r), r);
}

std::string terrainStrokePrimary(float x, float z, int ticks)
{
    std::string r;
    return answered(g_commands->terrainStroke(x, z, ticks, false, r), r);
}

std::string terrainStrokeSecondary(float x, float z, int ticks)
{
    std::string r;
    return answered(g_commands->terrainStroke(x, z, ticks, true, r), r);
}

// The history's replay: no answer on success.
std::string terrainApply(int id, const std::string& state)
{
    std::string error;
    return outcome(g_commands->terrainApplyChange(id, state, error), error);
}

std::string terrainExport(const std::string& directory)
{
    std::string r;
    return answered(g_commands->terrainExportHeightmaps(directory, r), r);
}

std::string terrainSectorsLine()
{
    std::string r;
    return answered(g_commands->terrainSectors(r), r);
}

std::string terrainSector(int x, int z, int active)
{
    if (active != 0 && active != 1)
        return outcome(false, "EditorTerrain.sector takes a sector x, z and 0 or 1");
    std::string r;
    return answered(g_commands->terrainSetSector(x, z, active != 0, r), r);
}

// The history's replay: no answer on success.
std::string terrainApplySector(int x, int z, int active)
{
    std::string error;
    return outcome(g_commands->terrainApplySector(x, z, active != 0, error), error);
}

std::string terrainImportSurrounding(const std::string& path)
{
    std::string r;
    return answered(g_commands->terrainImportSurrounding(path, r), r);
}

std::string terrainSmoothEdges(int width, float amplitude, float falloff)
{
    std::string r;
    return answered(g_commands->terrainSmoothEdges(width, amplitude, falloff, r), r);
}

std::string terrainSeaLevel(float level)
{
    std::string r;
    return answered(g_commands->terrainSetSeaLevel(level, r), r);
}

// The history's replay: no answer on success.
std::string terrainApplySeaLevel(float level)
{
    std::string error;
    return outcome(g_commands->terrainApplySeaLevel(level, error), error);
}

// EditorShortcut (the keyboard shortcuts, Framework/Shortcuts)
std::string shortcutList()
{
    std::string text;
    const std::vector<ShortcutAction>& actions = Shortcuts::actions();
    for (size_t i = 0; i < actions.size(); ++i)
    {
        const std::string keys = editorShortcuts().display(actions[i].id);
        text += (i ? "; " : "") + std::string(actions[i].id) + " " + (keys.empty() ? "none" : keys);
    }
    return text;
}

std::string shortcutSet(const std::string& id, const std::string& chord)
{
    std::string error;
    if (!editorShortcuts().set(id, _stricmp(chord.c_str(), "none") == 0 ? std::string() : chord, error))
        return outcome(false, error);
    const std::vector<std::string> clashes = editorShortcuts().conflicts(id);
    std::string text = id + " is " + (editorShortcuts().display(id).empty() ? std::string("unbound") : editorShortcuts().display(id));
    for (size_t i = 0; i < clashes.size(); ++i)
        text += (i ? ", " : "; also the key of ") + clashes[i];
    return text;
}

std::string shortcutReset(const std::string& id)
{
    if (id == "all")
        editorShortcuts().resetAll();
    else if (!Shortcuts::action(id))
        return outcome(false, "no action '" + id + "'");
    else
        editorShortcuts().reset(id);
    return id == "all" ? std::string("every key is its default again") : id + " is " + editorShortcuts().display(id);
}

std::string undergrowthInfoLine()
{
    std::string r;
    return answered(g_commands->undergrowthInfo(r), r);
}

std::string undergrowthTool(int active)
{
    if (active != 0 && active != 1)
        return outcome(false, "EditorUndergrowth.tool takes 0 or 1");
    std::string r;
    return answered(g_commands->setUndergrowthTool(active != 0, r), r);
}

std::string undergrowthMaterial(int id)
{
    std::string r;
    return answered(g_commands->setUndergrowthMaterial(id, r), r);
}

std::string undergrowthRadius(float radius)
{
    std::string r;
    return answered(g_commands->setUndergrowthRadius(radius, r), r);
}

std::string undergrowthPaint(float x, float z)
{
    std::string r;
    return answered(g_commands->undergrowthDab(x, z, false, r), r);
}

std::string undergrowthErase(float x, float z)
{
    std::string r;
    return answered(g_commands->undergrowthDab(x, z, true, r), r);
}

std::string undergrowthSetting(const std::string& key, float value)
{
    std::string r;
    return answered(g_commands->setUndergrowthSetting(key, value, r), r);
}

std::string undergrowthMaterialHeight(int id, float value)
{
    std::string r;
    return answered(g_commands->setUndergrowthMaterialHeight(id, value, r), r);
}

// Four arguments: the tree's macro shape returns nothing; the answer goes to the log, a refusal to g_lastError.
void undergrowthType(int id, const std::string& type, const std::string& key, const std::string& value)
{
    std::string r;
    if (!g_commands->setUndergrowthTypeProperty(id, type, key, value, r))
        outcome(false, r);
    else
        logLine(LogLevel::Info, "undergrowth: %s", r.c_str());
}

std::string undergrowthApplyConfig(int id, const std::string& state)
{
    std::string error;
    return outcome(g_commands->applyUndergrowthConfig(id, state, error), error);
}

// The history's replay: no answer on success.
std::string undergrowthApply(int id, const std::string& state)
{
    std::string error;
    return outcome(g_commands->applyUndergrowthChange(id, state, error), error);
}

std::string terrainTool(int active)
{
    if (active != 0 && active != 1)
        return outcome(false, "EditorTerrain.tool takes 0 or 1");
    std::string r;
    return answered(g_commands->setTerrainTool(active != 0, r), r);
}

// EditorLibrary, EditorBundle, EditorBrush
std::string libraryInfoLine()
{
    std::string r;
    return answered(g_commands->libraryInfo(r), r);
}

std::string libraryFolder(const std::string& folder)
{
    std::string r;
    return answered(g_commands->setLibraryFolder(folder, r), r);
}

std::string bundleList()
{
    std::string r;
    return answered(g_commands->listBundles(r), r);
}

std::string bundleInfoLine(const std::string& name)
{
    std::string r;
    return answered(g_commands->bundleInfo(name, r), r);
}

std::string bundleSave(const std::string& name)
{
    std::string r;
    return answered(g_commands->saveBundle(name, false, r), r);
}

std::string bundleSaveOver(const std::string& name)
{
    std::string r;
    return answered(g_commands->saveBundle(name, true, r), r);
}

std::string bundlePlace(const std::string& name, const dice::hfe::Vec3& at, float yaw)
{
    std::string r;
    return answered(g_commands->placeBundle(name, at, yaw, r), r);
}

std::string bundlePlaceHere(const std::string& name, float yaw)
{
    std::string r;
    return answered(g_commands->placeBundleHere(name, yaw, r), r);
}

std::string bundleGroup(int group)
{
    if (group != 0 && group != 1)
        return outcome(false, "EditorBundle.group takes 0 or 1");
    std::string r;
    return answered(g_commands->setBundleGroup(group != 0, r), r);
}

std::string bundleDelete(const std::string& name)
{
    std::string r;
    return answered(g_commands->deleteBundle(name, r), r);
}

std::string selectionGroupNew()
{
    std::string r;
    return answered(g_commands->groupSelection(r), r);
}

std::string selectionUngroup()
{
    std::string r;
    return answered(g_commands->ungroupSelection(r), r);
}

std::string selectionSelectGroup()
{
    std::string r;
    return answered(g_commands->selectGroup(r), r);
}

std::string selectionAnchorInfo()
{
    std::string r;
    return answered(g_commands->anchorInfo(r), r);
}

std::string selectionAlignStamp()
{
    std::string r;
    return answered(g_commands->setAlignStamp(r), r);
}

std::string selectionAlign()
{
    std::string r;
    return answered(g_commands->alignToStamp(r), r);
}

std::string selectionRaiseByAnchor()
{
    std::string r;
    return answered(g_commands->raiseByAnchor(r), r);
}

std::string objectTemplateFilesLine(const std::string& name)
{
    std::string r;
    return answered(g_commands->objectTemplateFiles(name, r), r);
}

std::string objectTemplateLinesLine(const std::string& name)
{
    std::string r;
    return answered(g_commands->objectTemplateLines(name, r), r);
}

std::string objectTemplateEngineValueLine(const std::string& name, const std::string& property)
{
    std::string r;
    return answered(g_commands->objectTemplateEngineValue(name, property, r), r);
}

std::string objectTemplateSet(const std::string& name, const std::string& property, const std::string& value)
{
    std::string r;
    return answered(g_commands->setObjectTemplateProperty(name, property, value, r), r);
}

std::string objectTemplateRemove(const std::string& name, const std::string& property)
{
    std::string r;
    return answered(g_commands->removeObjectTemplateProperty(name, property, r), r);
}

// The history's replay: no answer on success.
std::string objectTemplateApplyEdit(int id, const std::string& state)
{
    std::string error;
    return outcome(g_commands->applyObjectTemplateEdit(id, state, error), error);
}

std::string modList()
{
    std::string r;
    return answered(g_commands->listModsCommand(r), r);
}

std::string modCreate(const std::string& name)
{
    std::string r;
    return answered(g_commands->createModCommand(name, r), r);
}

std::string modOpen(const std::string& name)
{
    std::string r;
    return answered(g_commands->openMod(name, r), r);
}

std::string objectsStatus()
{
    std::string r;
    return answered(g_commands->objectArchivesStatus(r), r);
}

std::string objectsExtract()
{
    std::string r;
    return answered(g_commands->extractObjectArchives(r), r);
}

std::string objectsPack(const std::string& modFolder)
{
    std::string r;
    return answered(g_commands->packObjectArchives(modFolder, r), r);
}

std::string objectTemplateChildren(const std::string& name)
{
    std::string r;
    return answered(g_commands->objectTemplateChildren(name, r), r);
}

std::string objectTemplateAddChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position)
{
    std::string r;
    return answered(g_commands->addTemplateChild(name, child, position, r), r);
}

// Four arguments: the tree's macro shape returns nothing; the answer goes to the log, a refusal to g_lastError.
void objectTemplateMoveChild(const std::string& name, const std::string& child, const dice::hfe::Vec3& position,
                             const dice::hfe::Vec3& rotation)
{
    std::string r;
    if (!g_commands->moveTemplateChild(name, child, position, rotation, r))
        outcome(false, r);
    else
        logLine(LogLevel::Info, "template: %s", r.c_str());
}

std::string cameraBookmarkAction(const std::string& action, int index)
{
    std::string r;
    return answered(g_commands->cameraBookmark(action, index, r), r);
}

std::string cameraAddBookmark() { return cameraBookmarkAction("add", 0); }
std::string cameraToggleBookmark() { return cameraBookmarkAction("toggle", 0); }
std::string cameraGoBookmark(int index) { return cameraBookmarkAction("go", index); }
std::string cameraBookmarks() { return cameraBookmarkAction("list", 0); }

std::string lightAction(const std::string& action, const std::string& argument)
{
    std::string r;
    return answered(g_commands->lightPreset(action, argument, r), r);
}

std::string lightPresets() { return lightAction("list", ""); }
std::string lightApply(const std::string& name) { return lightAction("apply", name); }
std::string lightSave(const std::string& name) { return lightAction("save", name); }
std::string lightReset() { return lightAction("reset", ""); }
std::string lightLevels() { return lightAction("levels", ""); }
std::string lightApplyLevel(const std::string& name) { return lightAction("level", name); }

std::string lightSignAction(const std::string& action, int sign)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", sign);
    return lightAction(action, text);
}

std::string lightSunFromCamera(int sign) { return lightSignAction("sun", sign); }
std::string lightFlareFromCamera(int sign) { return lightSignAction("flare", sign); }

std::string hostToolAction(const std::string& action, const std::string& argument)
{
    std::string r;
    return answered(g_commands->hostTool(action, argument, r), r);
}

std::string clipboardPasteInPlace() { return hostToolAction("pasteInPlace", ""); }
std::string terrainView() { return hostToolAction("terrainView", ""); }
std::string terrainTechnique(int technique) { return hostToolAction("terrainTechnique", std::to_string(technique)); }
std::string groundLayersLine() { return hostToolAction("groundLayers", ""); }
std::string groundSetLine(int layer, const std::string& name, const std::string& value)
{
    return hostToolAction("groundSet", std::to_string(layer) + "\n" + name + "\n" + value);
}
std::string groundApplyLine(int layer, const std::string& name, const std::string& value)
{
    return hostToolAction("groundApply", std::to_string(layer) + "\n" + name + "\n" + value);
}
std::string groundMiscLine(const std::string& name, const std::string& value)
{
    return hostToolAction("groundSet", "-1\n" + name + "\n" + value);
}
std::string groundRemoveLayerLine(int layer) { return hostToolAction("groundRemoveLayer", std::to_string(layer)); }
std::string terrainMerge(const std::string& what) { return hostToolAction("terrainMerge", what == "-" ? "" : what); }
std::string terrainClearEdits(const std::string& what) { return hostToolAction("terrainClearEdits", what == "-" ? "" : what); }
std::string terrainShowSector(int x, int z, int shown)
{
    return hostToolAction("terrainShowSector", std::to_string(x) + "\n" + std::to_string(z) + "\n" + std::to_string(shown));
}
std::string terrainShownSectors() { return hostToolAction("terrainShownSectors", ""); }
std::string terrainGenerateOvergrowth() { return hostToolAction("overgrowthGenerate", ""); }
std::string terrainCompile() { return hostToolAction("terrainCompile", ""); }
std::string cameraFromSelection() { return hostToolAction("fromSelection", ""); }
std::string levelSnapAll() { return hostToolAction("snapAll", ""); }
std::string levelSaveLists() { return hostToolAction("saveLists", ""); }
std::string levelLevels() { return hostToolAction("levels", ""); }
std::string levelOpen(const std::string& name) { return hostToolAction("open", name); }
std::string levelResave() { return hostToolAction("resave", ""); }
// The save preview, backups and restores ([CHOSEN, not recovered], issue draft 13; App/EditSessionSave.cpp).
std::string levelPreview() { return hostToolAction("save.preview", ""); }
std::string levelPreviewAll() { return hostToolAction("save.previewAll", ""); }
std::string levelPreviewDiff(int file) { return hostToolAction("save.diff", std::to_string(file)); }
std::string levelWritePreview() { return hostToolAction("save.write", ""); }
std::string levelDropPreview() { return hostToolAction("save.drop", ""); }
std::string levelBackups() { return hostToolAction("save.backups", ""); }
std::string levelRestore(const std::string& name) { return hostToolAction("save.restore", name); }
std::string viewOverlays() { return hostToolAction("overlays", ""); }
std::string previewShow(const std::string& name) { return hostToolAction("preview.show", name); }
std::string previewRefresh() { return hostToolAction("preview.refresh", ""); }
std::string previewReset() { return hostToolAction("preview.reset", ""); }
std::string previewFrame() { return hostToolAction("preview.frame", ""); }
std::string previewTurntable(float rate)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", rate);
    return hostToolAction("preview.turntable", text);
}
std::string previewInfo() { return hostToolAction("preview.info", ""); }
std::string previewCamera(int which)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", which);
    return hostToolAction("preview.camera", text);
}
std::string previewClear() { return hostToolAction("preview.clear", ""); }
std::string previewShowParts(int on) { return hostToolAction("preview.showParts", std::to_string(on)); }
std::string previewParts() { return hostToolAction("preview.parts", ""); }
std::string previewPick(int part) { return hostToolAction("preview.pick", std::to_string(part)); }
std::string previewMovePart(int part, const dice::hfe::Vec3& by)
{
    char text[96];
    std::snprintf(text, sizeof(text), "%d %g/%g/%g", part, by.x, by.y, by.z);
    return hostToolAction("preview.movePart", text);
}
std::string previewTurnPart(int part, float degrees)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%d %g", part, degrees);
    return hostToolAction("preview.turnPart", text);
}
std::string selectionReload() { return hostToolAction("reloadSelected", ""); }
std::string levelReloadObjects() { return hostToolAction("reloadAll", ""); }
std::string levelRepositionObjects() { return hostToolAction("reposition", ""); }
std::string lightingCleanLightmaps(int move) { return hostToolAction("cleanLightmaps", move ? "1" : "0"); }
std::string templateOp(const std::string& op, const std::string& a, const std::string& b, const std::string& c)
{
    return hostToolAction("template", op + "\n" + a + "\n" + b + "\n" + c);
}
std::string objectTemplateRevert(const std::string& name) { return templateOp("revert", name, "", ""); }
std::string objectTemplateAddLine(const std::string& name, const std::string& property, const std::string& value)
{
    return templateOp("addLine", name, property, value);
}
std::string objectTemplateArmorEffect(const std::string& name, const std::string& effect) { return templateOp("armorEffect", name, effect, ""); }
std::string objectTemplateBarrel(const std::string& parent, const std::string& child, int on)
{
    return templateOp("barrel", parent, child, on ? "1" : "0");
}
std::string objectTemplateFireRates(const std::string& name, const std::string& modes) { return templateOp("fireRates", name, modes, ""); }
std::string objectTemplateSeatParent(const std::string& vehicle, const std::string& seat) { return templateOp("seatParent", vehicle, seat, ""); }
std::string objectTemplateTurnChild(const std::string& parent, const std::string& child, float degrees)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", degrees);
    return templateOp("turnChild", parent, child, text);
}
std::string objectTemplateTurnChildren(const std::string& parent, const std::string& child, float degrees)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", degrees);
    return templateOp("turnChildren", parent, child, text);
}
std::string objectTemplateResetChild(const std::string& parent, const std::string& child) { return templateOp("resetChild", parent, child, ""); }
std::string objectTemplateGeometry(const std::string& name) { return templateOp("geometry", name, "", ""); }
std::string objectTemplateSetGeometry(const std::string& name, const std::string& property, const std::string& value)
{
    return templateOp("setGeometry", name, property, value);
}
std::string objectTemplateRemoveGeometry(const std::string& name, const std::string& property) { return templateOp("removeGeometry", name, property, ""); }
std::string snapInfo() { return hostToolAction("snap", ""); }
std::string snapGround(int on) { return hostToolAction("snap", on ? "ground 1" : "ground 0"); }
std::string snapObjects(int on) { return hostToolAction("snap", on ? "objects 1" : "objects 0"); }
std::string snapGridMode(int mode)
{
    char text[32];
    std::snprintf(text, sizeof(text), "grid %d", mode);
    return hostToolAction("snap", text);
}
std::string clipboardLastOffsetCopy() { return hostToolAction("lastOffsetCopy", ""); }
std::string measureToolSet(int on) { return hostToolAction("measureTool", on ? "1" : "0"); }
std::string measureAddPoint(float x, float z)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%g %g", x, z);
    return hostToolAction("measureAdd", text);
}
std::string measureUndoPoint() { return hostToolAction("measureUndo", ""); }
std::string measureFinishLine() { return hostToolAction("measureFinish", ""); }
std::string gameplaySpawnerPresets() { return hostToolAction("spawnerPresets", ""); }
std::string gameplaySpawnerPreset(const std::string& spawner, const std::string& preset) { return hostToolAction("spawnerPreset", spawner + "\n" + preset); }
std::string levelSettingsTeams() { return hostToolAction("teams", ""); }
std::string levelSettingsTeam(const std::string& number, const std::string& name) { return hostToolAction("team", number + "\n" + name); }
std::string descriptionAction(const std::string& action, const std::string& which, const std::string& field,
                              const std::string& value)
{
    return hostToolAction("description", action + "\n" + which + "\n" + field + "\n" + value);
}
std::string modDescription() { return descriptionAction("list", "mod", "", ""); }
std::string modSetDescription(const std::string& field, const std::string& value) { return descriptionAction("set", "mod", field, value); }
std::string levelDescription() { return descriptionAction("list", "level", "", ""); }
std::string levelSetDescription(const std::string& field, const std::string& value) { return descriptionAction("set", "level", field, value); }
std::string modApplyDescription(const std::string& which, const std::string& field, const std::string& value)
{
    return descriptionAction("apply", which, field, value);
}
std::string modRemoveDescription(const std::string& which, const std::string& field) { return descriptionAction("remove", which, field, ""); }
std::string viewRecord(const std::string& path) { return hostToolAction("record", path); }
std::string viewStopRecording() { return hostToolAction("stopRecord", ""); }
std::string viewRecording() { return hostToolAction("recordStatus", ""); }
std::string viewOverlayLinks() { return hostToolAction("overlayLinks", ""); }
std::string viewOverlay(const std::string& name, const std::string& value) { return hostToolAction("overlay", name + " " + value); }
std::string helpDocumentsList() { return hostToolAction("helpDocuments", ""); }
std::string helpOpen(const std::string& name) { return hostToolAction("helpOpen", name); }
std::string helpDescribe(const std::string& command) { return hostToolAction("helpDescribe", command); }
std::string combatCopy(const std::string& name) { return hostToolAction("copyArea", name); }
std::string gameplaySelectSpawners() { return hostToolAction("selectSpawners", ""); }
std::string gameplaySupplyGroups() { return hostToolAction("supplyGroups", ""); }
std::string lightingLightSetup() { return hostToolAction("lightSetup", ""); }
std::string lightingLightSetupMode(const std::string& mode) { return hostToolAction("lightSetupMode", mode); }
std::string lightingLightSetupSet(const std::string& name, const std::string& value) { return hostToolAction("lightSetupSet", name + " " + value); }
std::string lightingLightSetupSave() { return hostToolAction("lightSetupSave", ""); }
std::string gameplayGroupControlPoints() { return hostToolAction("groupControlPoints", ""); }
std::string gameplayUngroupControlPoints() { return hostToolAction("ungroupControlPoints", ""); }
std::string gameplayConnectSupply(int team, int supply, int group)
{
    char text[64];
    std::snprintf(text, sizeof(text), "%d %d %d", team, supply, group);
    return hostToolAction("connectSupply", text);
}
std::string gameplayDisconnectSupply(int group)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%d", group);
    return hostToolAction("disconnectSupply", text);
}
std::string gameplayApplyGroup(const std::string& name, int group)
{
    char text[32];
    std::snprintf(text, sizeof(text), " %d", group);
    return hostToolAction("applyGroup", name + text);
}
std::string gameplayApplySupply(const std::string& name, const std::string& entry) { return hostToolAction("applySupply", name + " " + entry); }
std::string combatPaste(const std::string& name) { return hostToolAction("pasteArea", name); }

std::string materialAction(const std::string& action, const std::vector<std::string>& args)
{
    std::string r;
    return answered(g_commands->materialCommand(action, args, r), r);
}

std::vector<std::string> materialArgs(int a = -1, int b = -1)
{
    std::vector<std::string> args;
    char text[16];
    if (a != -1)
    {
        std::snprintf(text, sizeof(text), "%d", a);
        args.push_back(text);
    }
    if (b != -1)
    {
        std::snprintf(text, sizeof(text), "%d", b);
        args.push_back(text);
    }
    return args;
}

std::string materialStatus() { return materialAction("status", materialArgs()); }
std::string materialList() { return materialAction("list", materialArgs()); }
std::string materialInfo(int id) { return materialAction("info", materialArgs(id)); }
std::string materialCellsOf(int id) { return materialAction("cells", materialArgs(id)); }
std::string materialCell(int a, int b) { return materialAction("cell", materialArgs(a, b)); }
std::string materialSave() { return materialAction("save", materialArgs()); }
std::string materialExtract() { return materialAction("extract", materialArgs()); }
std::string materialRemove(int id) { return materialAction("remove", materialArgs(id)); }
std::string materialAddCell(int a, int b) { return materialAction("addCell", materialArgs(a, b)); }
std::string materialRemoveCell(int a, int b) { return materialAction("removeCell", materialArgs(a, b)); }

std::string materialSet(int id, const std::string& property, const std::string& value)
{
    std::vector<std::string> args = materialArgs(id);
    args.push_back(property);
    args.push_back(value);
    return materialAction("set", args);
}

std::string materialAdd(const std::string& name)
{
    return materialAction("add", std::vector<std::string>(1, name));
}

std::string materialAddAs(const std::string& name, int id, int type)
{
    std::vector<std::string> args(1, name);
    const std::vector<std::string> numbers = materialArgs(id, type);
    args.insert(args.end(), numbers.begin(), numbers.end());
    return materialAction("addAs", args);
}

std::string materialSetDamageMod(int a, int b, const std::string& value)
{
    std::vector<std::string> args = materialArgs(a, b);
    args.push_back(value);
    return materialAction("setDamageMod", args);
}

// Five arguments: the tree's macro shape returns nothing; the answer goes to the log, a refusal to g_lastError.
void materialSetTemplate(int a, int b, const std::string& kind, int index, const std::string& name)
{
    std::vector<std::string> args = materialArgs(a, b);
    args.push_back(kind);
    char text[16];
    std::snprintf(text, sizeof(text), "%d", index);
    args.push_back(text);
    args.push_back(name);
    std::string r;
    if (!g_commands->materialCommand("setTemplate", args, r))
        outcome(false, r);
    else
        logLine(LogLevel::Info, "material: %s", r.c_str());
}

std::string materialApplyEdit(int id, const std::string& state)
{
    std::string error;
    return outcome(g_commands->applyMaterialEdit(id, state, error), error);
}

std::string layersAction(const std::string& action, const std::vector<std::string>& args)
{
    std::string r;
    return answered(g_commands->layersCommand(action, args, r), r);
}

std::vector<std::string> layerArgs(const std::string& a, const std::string& b = std::string(), const std::string& c = std::string(),
                                   int count = 1)
{
    std::vector<std::string> args;
    const std::string given[3] = { a, b, c };
    for (int i = 0; i < count; ++i)
        args.push_back(given[i]);
    return args;
}

std::string layerNumber(int id)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%d", id);
    return text;
}

std::string layerStatus() { return layersAction("status", std::vector<std::string>()); }
std::string layerList() { return layersAction("list", std::vector<std::string>()); }
std::string layerAdd(const std::string& name) { return layersAction("add", layerArgs(name)); }
std::string layerRename(int id, const std::string& name) { return layersAction("rename", layerArgs(layerNumber(id), name, "", 2)); }
std::string layerFilters(int id, const std::string& modes, const std::string& players)
{
    return layersAction("filters", layerArgs(layerNumber(id), modes, players, 3));
}
std::string layerDisable(int id, int on) { return layersAction("disable", layerArgs(layerNumber(id), layerNumber(on), "", 2)); }
std::string layerActive(int id) { return layersAction("active", layerArgs(layerNumber(id))); }
std::string layerRemove(int id) { return layersAction("remove", layerArgs(layerNumber(id))); }
std::string layerRemove(int id, const std::string& mode) { return layersAction("remove", layerArgs(layerNumber(id), mode, "", 2)); }
std::string layerApplyLayers(int id, const std::string& state) { return layersAction("applyLayers", layerArgs(layerNumber(id), state, "", 2)); }

std::string animationAction(const std::string& action, const std::string& a = std::string(), const std::string& b = std::string(),
                            const std::string& c = std::string(), int count = -1)
{
    std::vector<std::string> args;
    const std::string given[3] = { a, b, c };
    const int n = count >= 0 ? count : (!c.empty() ? 3 : !b.empty() ? 2 : !a.empty() ? 1 : 0);
    for (int i = 0; i < n; ++i)
        args.push_back(given[i]);
    std::string r;
    return answered(g_commands->animationCommand(action, args, r), r);
}

std::string animationSystems() { return animationAction("systems"); }
std::string animationOpen(const std::string& path) { return animationAction("open", path, "", "", 1); }
std::string animationStatus() { return animationAction("status"); }
std::string animationTree() { return animationAction("tree"); }
std::string animationList(const std::string& kind) { return animationAction("list", kind, "", "", 1); }
std::string animationSelect(const std::string& kind, const std::string& name) { return animationAction("select", kind, name, "", 2); }
std::string animationSelected() { return animationAction("selected"); }
std::string animationSet(const std::string& property, const std::string& value) { return animationAction("set", property, value, "", 2); }
std::string animationUnset(const std::string& property) { return animationAction("unset", property, "", "", 1); }
std::string animationSetSystem(const std::string& property, const std::string& value)
{
    return animationAction("setSystem", property, value, "", 2);
}
std::string animationUnsetSystem(const std::string& property) { return animationAction("unsetSystem", property, "", "", 1); }
std::string animationAddTrigger(const std::string& type, const std::string& name) { return animationAction("addTrigger", type, name, "", 2); }
std::string animationAddBundle(const std::string& name) { return animationAction("addBundle", name, "", "", 1); }
std::string animationAddValueHolder(const std::string& name, const std::string& values)
{
    return animationAction("addValueHolder", name, values, "", 2);
}
std::string animationAddAnimation(const std::string& path) { return animationAction("addAnimation", path, "", "", 1); }
std::string animationAddEvent(const std::string& name) { return animationAction("addEvent", name, "", "", 1); }
std::string animationRemoveAnimation(const std::string& path) { return animationAction("removeAnimation", path, "", "", 1); }
std::string animationRemoveEvent(const std::string& name) { return animationAction("removeEvent", name, "", "", 1); }
std::string animationSetStart(const std::string& which, const std::string& name, const std::string& time)
{
    return animationAction("setStart", which, name, time, 3);
}
std::string animationSetLength(const std::string& path, const std::string& time) { return animationAction("setLength", path, time, "", 2); }
std::string animationLink(const std::string& which, const std::string& name) { return animationAction("link", which, name, "", 2); }
std::string animationUnlink(const std::string& which, const std::string& name) { return animationAction("unlink", which, name, "", 2); }
std::string animationRemove() { return animationAction("remove"); }
std::string animationTimeline() { return animationAction("timeline"); }
std::string animationJoints(const std::string& folder) { return animationAction("joints", folder, "", "", 1); }
std::string animationJoints(const std::string& folder, const std::string& mode) { return animationAction("joints", folder, mode, "", 2); }
std::string animationCopySystems(const std::string& target, const std::string& source)
{
    return animationAction("copySystems", target, source, "", 2);
}
std::string animationPoint(const std::string& target, const std::string& which, const std::string& path)
{
    return animationAction("point", target, which, path, 3);
}
std::string animationSave() { return animationAction("save"); }

std::string animationApplyEdit(int id, const std::string& state)
{
    std::string error;
    return outcome(g_commands->applyAnimationEdit(id, state, error), error);
}

std::string toolsAction(const std::string& action, int index, const std::string& argument)
{
    std::string r;
    if (!g_toolsHandler)
        return outcome(false, "the editor's tools are not up");
    return answered(g_toolsHandler(g_toolsContext, action, index, argument, r), r);
}

std::string toolsCustomCommands() { return toolsAction("list", 0, ""); }
std::string toolsSetCustomCommand(int index, const std::string& line) { return toolsAction("setCommand", index, line); }
std::string toolsSetCustomFile(int index, const std::string& file) { return toolsAction("setFile", index, file); }
std::string toolsRunCustomCommand(int index) { return toolsAction("runCommand", index, ""); }
std::string toolsRunCustomFile(int index) { return toolsAction("runFile", index, ""); }

std::string wizardAction(const std::string& action, const std::string& argument)
{
    std::string r;
    return answered(g_commands->wizardCommand(action, argument, r), r);
}

std::string wizardLoad() { return wizardAction("load", ""); }
std::string wizardRules() { return wizardAction("rules", ""); }
std::string wizardRun(const std::string& category) { return wizardAction("run", category); }
std::string wizardItems() { return wizardAction("items", ""); }
std::string wizardExecute() { return wizardAction("execute", ""); }
std::string wizardPrompt() { return wizardAction("prompt", ""); }
std::string wizardAnswer(const std::string& text) { return wizardAction("answer", text); }
std::string wizardCancel() { return wizardAction("cancel", ""); }
std::string wizardChecklistOk() { return wizardAction("checklistOk", ""); }

std::string wizardTick(const std::string& which, int on)
{
    char text[16];
    std::snprintf(text, sizeof(text), " %d", on);
    return wizardAction("tick", which + text);
}

std::string wizardCheck(int row, int on)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%d %d", row, on);
    return wizardAction("check", text);
}

std::string lightingGroundHemi(int resolution, const dice::hfe::Vec3& position, float size)
{
    std::string r;
    return answered(g_commands->bakeGroundHemi(resolution, position, size, r), r);
}

std::string lightingMinimap(int resolution)
{
    std::string r;
    return answered(g_commands->captureMinimap(resolution, r), r);
}

std::string lightingEnvMaps()
{
    std::string r;
    return answered(g_commands->generateEnvMaps(r), r);
}

std::string lightingLightmaps(int quality, const std::string& parts, const std::string& options)
{
    std::string r;
    return answered(g_commands->generateLightmaps(quality, parts, options, r), r);
}

// Roads (App/EditSessionRoads.cpp): one command with its words, the empty optional ones left out.
std::string roadAction(const char* object, const char* method, const std::string& a = std::string(),
                       const std::string& b = std::string(), const std::string& c = std::string(),
                       const std::string& d = std::string())
{
    std::vector<std::string> args;
    const std::string* const words[] = { &a, &b, &c, &d };
    size_t used = 0;
    for (size_t i = 0; i < 4; ++i)
        if (!words[i]->empty())
            used = i + 1;
    for (size_t i = 0; i < used; ++i)
        args.push_back(*words[i]);
    std::string r;
    return answered(g_commands->roadCommand(object, method, args, r), r);
}

// The retail rows, each a binding of its own (the console finds rows by name).
#define BF2E_ROAD_ROW_0(fn, object, method) \
    std::string fn() { return roadAction(object, method); }
#define BF2E_ROAD_ROW_1(fn, object, method) \
    std::string fn(const std::string& a) { return roadAction(object, method, a); }
BF2E_ROAD_ROW_0(splineManagerCreateInstance, "SplineManager", "CreateInstance")
BF2E_ROAD_ROW_1(splineManagerSetId, "SplineManager", "SetId")
BF2E_ROAD_ROW_1(splineManagerSnapModes, "SplineManager", "SetControlPointSnapModes")
BF2E_ROAD_ROW_1(splineManagerLineWidth, "SplineManager", "setLineWidth")
BF2E_ROAD_ROW_1(splineManagerStepSize, "SplineManager", "setStepSize")
BF2E_ROAD_ROW_1(splineManagerAddControlPoint, "SplineManager", "AddControlPoint")
BF2E_ROAD_ROW_1(splineManagerSpeed, "SplineManager", "SetSpeed")
BF2E_ROAD_ROW_1(splineManagerDelay, "SplineManager", "SetDelay")
BF2E_ROAD_ROW_1(splineManagerDirection, "SplineManager", "SetDirection")
BF2E_ROAD_ROW_1(splineManagerSound, "SplineManager", "SetSound")
BF2E_ROAD_ROW_0(splineManagerEnd, "SplineManager", "End")
std::string roadCreateInstance(const std::string& name, const std::string& spline, const std::string& roadTemplate,
                               const std::string& seed, const std::string&)
{
    return roadAction("Road", "CreateInstance", name, spline, roadTemplate, seed);
}
BF2E_ROAD_ROW_1(roadTextureScaleOffset, "Road", "SetTextureScaleOffset")
BF2E_ROAD_ROW_1(roadPersistance, "Road", "SetPersistance")
BF2E_ROAD_ROW_1(roadRange, "Road", "SetRange")
BF2E_ROAD_ROW_1(roadOctaves, "Road", "SetOctaves")
BF2E_ROAD_ROW_1(roadPrio, "Road", "Prio")
BF2E_ROAD_ROW_1(roadFadeStartLeft, "Road", "FadeDistAtStartLeft")
BF2E_ROAD_ROW_1(roadFadeStartRight, "Road", "FadeDistAtStartRight")
BF2E_ROAD_ROW_1(roadFadeEndLeft, "Road", "FadeDistAtEndLeft")
BF2E_ROAD_ROW_1(roadFadeEndRight, "Road", "FadeDistAtEndRight")
BF2E_ROAD_ROW_0(roadTemplateCreateInstance, "RoadTemplate", "CreateInstance")
BF2E_ROAD_ROW_1(roadTemplateName, "RoadTemplate", "SetName")
BF2E_ROAD_ROW_1(roadTemplateWidth, "RoadTemplate", "SetWidth")
BF2E_ROAD_ROW_1(roadTemplateMaxError, "RoadTemplate", "SetMaxTesselatedError")
BF2E_ROAD_ROW_1(roadTemplateBlendFactor, "RoadTemplate", "SetBlendFactor")
BF2E_ROAD_ROW_1(roadTemplateFadeOff, "RoadTemplate", "SetFadeOffDistance")
BF2E_ROAD_ROW_1(roadTemplatePrio, "RoadTemplate", "SetPrio")
BF2E_ROAD_ROW_1(roadTemplateMaterial, "RoadTemplate", "SetMaterial")
BF2E_ROAD_ROW_1(roadTemplatePrimary, "RoadTemplate", "SetIsPrimaryTexture")
BF2E_ROAD_ROW_1(roadTextureFile, "RoadTemplateTexture", "SetTextureFile")
BF2E_ROAD_ROW_1(roadTextureMirror, "RoadTemplateTexture", "SetMirror")
BF2E_ROAD_ROW_1(roadTextureScale, "RoadTemplateTexture", "SetScale")
#undef BF2E_ROAD_ROW_0
#undef BF2E_ROAD_ROW_1

// EditorRoad.*
std::string editorRoadInfo() { return roadAction("EditorRoad", "info"); }
std::string editorRoadSplines() { return roadAction("EditorRoad", "splines"); }
std::string editorRoadRoads() { return roadAction("EditorRoad", "roads"); }
std::string editorRoadTemplates() { return roadAction("EditorRoad", "templates"); }
std::string editorRoadSave() { return roadAction("EditorRoad", "save"); }
std::string editorRoadSplineCreate() { return roadAction("EditorRoad", "splineCreate"); }
std::string editorRoadSpline(const std::string& id) { return roadAction("EditorRoad", "spline", id); }
std::string editorRoadRoad(const std::string& name) { return roadAction("EditorRoad", "road", name); }
std::string editorRoadTemplate(const std::string& name) { return roadAction("EditorRoad", "template", name); }
std::string editorRoadSplineCreateAs(const std::string& id) { return roadAction("EditorRoad", "splineCreate", id); }
std::string editorRoadSplineAdd(const std::string& id, const std::string& at)
{
    return roadAction("EditorRoad", "splineAdd", id, at);
}
std::string editorRoadSplineInsert(const std::string& id, const std::string& index, const std::string& at)
{
    return roadAction("EditorRoad", "splineInsert", id, index, at);
}
std::string editorRoadSplineMove(const std::string& id, const std::string& index, const std::string& at)
{
    return roadAction("EditorRoad", "splineMove", id, index, at);
}
std::string editorRoadSplineRemovePoint(const std::string& id, const std::string& index)
{
    return roadAction("EditorRoad", "splineRemovePoint", id, index);
}
std::string editorRoadSplineSet(const std::string& id, const std::string& key, const std::string& value)
{
    return roadAction("EditorRoad", "splineSet", id, key, value);
}
std::string editorRoadSplinePoint(const std::string& id, const std::string& index, const std::string& key,
                                  const std::string& value, const std::string&)
{
    return roadAction("EditorRoad", "splinePoint", id, index, key, value);
}
std::string editorRoadSplineDelete(const std::string& id) { return roadAction("EditorRoad", "splineDelete", id); }
std::string editorRoadTemplateCreate(const std::string& name) { return roadAction("EditorRoad", "templateCreate", name); }
std::string editorRoadTemplateCreateFrom(const std::string& name, const std::string& from)
{
    return roadAction("EditorRoad", "templateCreate", name, from);
}
std::string editorRoadTemplateSet(const std::string& name, const std::string& key, const std::string& value)
{
    return roadAction("EditorRoad", "templateSet", name, key, value);
}
std::string editorRoadTemplateDelete(const std::string& name) { return roadAction("EditorRoad", "templateDelete", name); }
std::string editorRoadCreate(const std::string& name, const std::string& spline, const std::string& roadTemplate,
                             const std::string& seed, const std::string&)
{
    return roadAction("EditorRoad", "create", name, spline, roadTemplate, seed);
}
std::string editorRoadSet(const std::string& name, const std::string& key, const std::string& value)
{
    return roadAction("EditorRoad", "set", name, key, value);
}
std::string editorRoadDelete(const std::string& name) { return roadAction("EditorRoad", "delete", name); }
std::string editorRoadRename(const std::string& name, const std::string& to)
{
    return roadAction("EditorRoad", "rename", name, to);
}
std::string editorRoadBuild(const std::string& name, const std::string& mode)
{
    return roadAction("EditorRoad", "build", name, mode);
}
std::string editorRoadApplyEdit(const std::string& id, const std::string& state)
{
    return roadAction("EditorRoad", "applyEdit", id, state);
}
std::string editorRoadTool(const std::string& on) { return roadAction("EditorRoad", "tool", on); }
std::string editorRoadActiveSpline(const std::string& id, const std::string& point = std::string())
{
    return roadAction("EditorRoad", "activeSpline", id, point);
}
std::string editorRoadShowSplines(const std::string& on) { return roadAction("EditorRoad", "showSplines", on); }
std::string editorRoadApply(const std::string& name) { return roadAction("EditorRoad", "apply", name); }
std::string editorRoadSplineCopy(const std::string& id = std::string()) { return roadAction("EditorRoad", "splineCopy", id); }
std::string editorRoadSplinePaste() { return roadAction("EditorRoad", "splinePaste"); }
std::string editorRoadSplineMark(const std::string& id = std::string(), const std::string& point = std::string())
{
    return roadAction("EditorRoad", "splineMark", id, point);
}
std::string editorRoadSplineMerge(const std::string& id = std::string(), const std::string& point = std::string())
{
    return roadAction("EditorRoad", "splineMerge", id, point);
}
std::string editorRoadActiveTemplate(const std::string& name) { return roadAction("EditorRoad", "activeTemplate", name); }
std::string editorRoadConnect(const std::string& name, const std::string& roadTemplate = std::string())
{
    return roadAction("EditorRoad", "connect", name, roadTemplate);
}
std::string editorRoadDisconnect() { return roadAction("EditorRoad", "disconnect"); }
std::string editorRoadRefresh() { return roadAction("EditorRoad", "refresh"); }
std::string editorRoadCleanFolder(const std::string& move = std::string()) { return roadAction("EditorRoad", "cleanFolder", move); }

std::string effectPreviewLine(const std::string& templateName)
{
    std::string r;
    return answered(g_commands->previewEffect(templateName, r), r);
}

std::string effectPlay()
{
    std::string r;
    return answered(g_commands->effectAction("play", r), r);
}

std::string effectStop()
{
    std::string r;
    return answered(g_commands->effectAction("stop", r), r);
}

std::string effectPause()
{
    std::string r;
    return answered(g_commands->effectAction("pause", r), r);
}

std::string effectClear()
{
    std::string r;
    return answered(g_commands->clearEffectPreview(r), r);
}

std::string effectInfoLine()
{
    std::string r;
    return answered(g_commands->effectInfo(r), r);
}

std::string objectTemplateNew(const std::string& className, const std::string& name, const std::string& folder)
{
    std::string r;
    return answered(g_commands->newTemplate(className, name, folder, r), r);
}

std::string objectTemplateChangeType(const std::string& name, const std::string& className)
{
    std::string r;
    return answered(g_commands->changeTemplateType(name, className, r), r);
}

std::string objectTemplateRename(const std::string& name, const std::string& newName)
{
    std::string r;
    return answered(g_commands->renameObjectTemplate(name, newName, r), r);
}

std::string objectTemplateInherit(const std::string& target, const std::string& source)
{
    std::string r;
    return answered(g_commands->inheritTemplateSettings(target, source, r), r);
}

std::string objectTemplateClone(const std::string& source, const std::string& name, const std::string& folder)
{
    std::string r;
    return answered(g_commands->cloneTemplate(source, name, folder, r), r);
}

// Four arguments: the tree's macro shape returns nothing; the answer goes to the log, a refusal to g_lastError.
void objectTemplateNewChild(const std::string& parent, const std::string& className, const std::string& name,
                            const dice::hfe::Vec3& position)
{
    std::string r;
    if (!g_commands->newTemplateChild(parent, className, name, position, r))
        outcome(false, r);
    else
        logLine(LogLevel::Info, "template: %s", r.c_str());
}

// The Object templates panel opened on the template.
std::string objectTemplateShow(const std::string& name)
{
    std::string r;
    if (!g_commands->showObjectTemplate(name, r))
        return outcome(false, r);
    if (g_panelHandler)
        g_panelHandler(g_panelContext, "ObjectTemplates", true);
    return r;
}

std::string objectTemplateComponents(const std::string& name)
{
    std::string r;
    return answered(g_commands->objectTemplateComponents(name, r), r);
}

std::string objectTemplateFixes(const std::string& name)
{
    std::string r;
    return answered(g_commands->objectTemplateFixes(name, r), r);
}

std::string objectTemplateComponentClasses()
{
    std::string r;
    return answered(g_commands->objectTemplateComponentClasses(r), r);
}

std::string objectTemplateAddComponent(const std::string& name, const std::string& component)
{
    std::string r;
    return answered(g_commands->addTemplateComponent(name, component, r), r);
}

std::string objectTemplateRemoveComponent(const std::string& name, const std::string& component)
{
    std::string r;
    return answered(g_commands->removeTemplateComponent(name, component, r), r);
}

std::string objectTemplateRemoveChild(const std::string& name, const std::string& child)
{
    std::string r;
    return answered(g_commands->removeTemplateChild(name, child, r), r);
}

std::string objectTemplateSave()
{
    std::string r;
    return answered(g_commands->saveObjectTemplates(r), r);
}

std::string selectionSetAnchor(const dice::hfe::Vec3& anchor)
{
    std::string r;
    return answered(g_commands->setAnchor(anchor, r), r);
}

std::string selectionAnchorToBox(const std::string& which)
{
    std::string r;
    return answered(g_commands->anchorToBox(which, r), r);
}

std::string selectionResetAnchor()
{
    std::string r;
    return answered(g_commands->resetAnchor(r), r);
}

std::string gizmoAnchorMode(int on)
{
    if (on != 0 && on != 1)
        return outcome(false, "EditorGizmo.anchorMode takes 0 or 1");
    std::string r;
    return answered(g_commands->setAnchorMoveMode(on != 0, r), r);
}

std::string gizmoPivot(int pivot)
{
    std::string r;
    return answered(g_commands->setGizmoPivot(pivot, r), r);
}

std::string brushList()
{
    std::string r;
    return answered(g_commands->listBrushes(r), r);
}

std::string brushInfoLine(const std::string& name)
{
    std::string r;
    return answered(g_commands->brushInfo(name, r), r);
}

std::string brushSelect(const std::string& name)
{
    std::string r;
    return answered(g_commands->selectBrush(name, r), r);
}

std::string brushDefine(const std::string& name)
{
    std::string r;
    return answered(g_commands->defineBrush(name, r), r);
}

std::string brushAddSelection()
{
    std::string r;
    return answered(g_commands->addSelectionToBrush(r), r);
}

std::string brushAddItem(const std::string& templateName)
{
    std::string r;
    return answered(g_commands->addBrushItem(templateName, r), r);
}

std::string brushSet(const std::string& key, const std::string& value)
{
    std::string r;
    return answered(g_commands->setBrushProperty(key, value, r), r);
}

std::string brushItem(int index, const std::string& key, const std::string& value)
{
    std::string r;
    return answered(g_commands->setBrushItem(index, key, value, r), r);
}

std::string brushRemoveItem(int index)
{
    std::string r;
    return answered(g_commands->removeBrushItem(index, r), r);
}

std::string brushDelete(const std::string& name)
{
    std::string r;
    return answered(g_commands->deleteBrush(name, r), r);
}

std::string brushSeed(int seed)
{
    std::string r;
    return answered(g_commands->seedBrush(static_cast<unsigned>(seed), r), r);
}

std::string brushTool(int active)
{
    if (active != 0 && active != 1)
        return outcome(false, "EditorBrush.tool takes 0 or 1");
    std::string r;
    return answered(g_commands->setBrushTool(active != 0, r), r);
}

std::string brushEraseRadius(float radius)
{
    std::string r;
    return answered(g_commands->setBrushEraseRadius(radius, r), r);
}

std::string brushStampAt(const dice::hfe::Vec3& at)
{
    std::string r;
    return answered(g_commands->brushStamp(at, r), r);
}

std::string brushEraseAt(const dice::hfe::Vec3& at)
{
    std::string r;
    return answered(g_commands->brushErase(at, r), r);
}

// Spelled as the generated classes store them ("Vec3", "Mat4"), like the engine's own bindings.
typedef dice::hfe::Vec3 Vec3;
typedef dice::hfe::Mat4 Mat4;

} // namespace

BF2_CONSOLE_OBJECT("EditorObjectTemplate", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(files, objectTemplateFilesLine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(lines, objectTemplateLinesLine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(engineValue, objectTemplateEngineValueLine, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(set, objectTemplateSet, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(remove, objectTemplateRemove, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyEdit, objectTemplateApplyEdit, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, objectTemplateSave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(children, objectTemplateChildren, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(addChild, objectTemplateAddChild, std::string, g_commands != 0, std::string, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(moveChild, objectTemplateMoveChild, g_commands != 0, std::string, std::string, Vec3, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(components, objectTemplateComponents, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(fixes, objectTemplateFixes, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(show, objectTemplateShow, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_3_RET_ACTIVE(new, objectTemplateNew, std::string, g_commands != 0, std::string, std::string, std::string, "", kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_3_RET_ACTIVE(clone, objectTemplateClone, std::string, g_commands != 0, std::string, std::string, std::string, "", kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(newChild, objectTemplateNewChild, g_commands != 0, std::string, std::string, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(inherit, objectTemplateInherit, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(rename, objectTemplateRename, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(changeType, objectTemplateChangeType, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(componentClasses, objectTemplateComponentClasses, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(addComponent, objectTemplateAddComponent, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeComponent, objectTemplateRemoveComponent, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeChild, objectTemplateRemoveChild, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(revert, objectTemplateRevert, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(addLine, objectTemplateAddLine, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(addArmorEffect, objectTemplateArmorEffect, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(barrel, objectTemplateBarrel, std::string, g_commands != 0, std::string, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(fireRates, objectTemplateFireRates, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(seatParent, objectTemplateSeatParent, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(turnChild, objectTemplateTurnChild, std::string, g_commands != 0, std::string, std::string, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(turnChildren, objectTemplateTurnChildren, std::string, g_commands != 0, std::string, std::string, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(resetChild, objectTemplateResetChild, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(geometry, objectTemplateGeometry, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(setGeometry, objectTemplateSetGeometry, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeGeometry, objectTemplateRemoveGeometry, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorWizard", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(load, wizardLoad, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(rules, wizardRules, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(run, wizardRun, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(items, wizardItems, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(tick, wizardTick, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(execute, wizardExecute, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(prompt, wizardPrompt, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(answer, wizardAnswer, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(cancel, wizardCancel, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(check, wizardCheck, std::string, g_commands != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(checklistOk, wizardChecklistOk, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorLighting", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(lightSetup, lightingLightSetup, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(lightSetupMode, lightingLightSetupMode, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(lightSetupSet, lightingLightSetupSet, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(lightSetupSave, lightingLightSetupSave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(cleanLightmaps, lightingCleanLightmaps, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(groundHemi, lightingGroundHemi, std::string, g_commands != 0, int, Vec3, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(minimap, lightingMinimap, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(envMaps, lightingEnvMaps, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(lightmaps, lightingLightmaps, std::string, g_commands != 0, int, std::string, std::string, kEditorConsoleTag)

// The retail editor's road rows (its LevelEditor plugin's and EditorEngine's): Editor/Splines.con,
// Editor/Roads.con and the road templates' editor branch run through these (App/RoadNetwork.h).
BF2_CONSOLE_OBJECT("SplineManager", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(CreateInstance, splineManagerCreateInstance, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetId, splineManagerSetId, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetControlPointSnapModes, splineManagerSnapModes, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setLineWidth, splineManagerLineWidth, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setStepSize, splineManagerStepSize, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(AddControlPoint, splineManagerAddControlPoint, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetSpeed, splineManagerSpeed, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetDelay, splineManagerDelay, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetDirection, splineManagerDirection, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetSound, splineManagerSound, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(End, splineManagerEnd, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("Road", 2, 0)
BF2_CONSOLE_METHOD_3_5_RET_ACTIVE(CreateInstance, roadCreateInstance, std::string, g_commands != 0, std::string, std::string, std::string, std::string, std::string, "", "", kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetTextureScaleOffset, roadTextureScaleOffset, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetPersistance, roadPersistance, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetRange, roadRange, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetOctaves, roadOctaves, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(Prio, roadPrio, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(FadeDistAtStartLeft, roadFadeStartLeft, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(FadeDistAtStartRight, roadFadeStartRight, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(FadeDistAtEndLeft, roadFadeEndLeft, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(FadeDistAtEndRight, roadFadeEndRight, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("RoadTemplate", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(CreateInstance, roadTemplateCreateInstance, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetName, roadTemplateName, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetWidth, roadTemplateWidth, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetMaxTesselatedError, roadTemplateMaxError, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetBlendFactor, roadTemplateBlendFactor, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetFadeOffDistance, roadTemplateFadeOff, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetPrio, roadTemplatePrio, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetMaterial, roadTemplateMaterial, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetIsPrimaryTexture, roadTemplatePrimary, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("RoadTemplateTexture", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetTextureFile, roadTextureFile, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetMirror, roadTextureMirror, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(SetScale, roadTextureScale, std::string, g_commands != 0, std::string, kEditorConsoleTag)

// Returning editor rows whose arguments are all optional: none or one, and none, one or two (the shared macros take at
// least one). The target is called with the arguments given; its own defaults fill the rest.
#define EDITOR_CONSOLE_METHOD_0_1_RET_ACTIVE(fnName, target, RET, activeExpr, A0, listTag)                     \
namespace {                                                                                                    \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                                  \
{                                                                                                              \
    BF2_CC_HEAD(fnName, 0, 0, 1, #RET, true)                                                                   \
            m_argTypes[0] = #A0;                                                                               \
    BF2_CC_TAIL(1, listTag)                                                                                    \
    virtual bool isObjectActive() const { return (activeExpr); }                                               \
    virtual void executeObjectMethod()                                                                         \
    {                                                                                                          \
        m_hasReturnValue = true;                                                                               \
        m_returnValue = m_argCount == 0 ? target() : target(BF2_CC_ARG(A0, 0));                                \
    }                                                                                                          \
    virtual void setArgFromString(int index, const std::string& value)                                         \
    {                                                                                                          \
        BF2_CC_CONVERT(A0, 1)                                                                                  \
    }                                                                                                          \
    BF2_CC_RETURN_AS_STRING                                                                                    \
    BF2_CC_COMMON_HOOKS                                                                                        \
private:                                                                                                       \
    RET m_returnValue;                                                                                         \
};                                                                                                             \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                            \
}
#define EDITOR_CONSOLE_METHOD_0_2_RET_ACTIVE(fnName, target, RET, activeExpr, A0, A1, listTag)                 \
namespace {                                                                                                    \
class BF2_CC_NAME : public ::dice::hfe::ConsoleObjectBaseImpl                                                  \
{                                                                                                              \
    BF2_CC_HEAD(fnName, 0, 0, 2, #RET, true)                                                                   \
            m_argTypes[0] = #A0;                                                                               \
            m_argTypes[1] = #A1;                                                                               \
    BF2_CC_TAIL(2, listTag)                                                                                    \
    virtual bool isObjectActive() const { return (activeExpr); }                                               \
    virtual void executeObjectMethod()                                                                         \
    {                                                                                                          \
        m_hasReturnValue = true;                                                                               \
        if (m_argCount == 0)                                                                                   \
            m_returnValue = target();                                                                          \
        else if (m_argCount == 1)                                                                              \
            m_returnValue = target(BF2_CC_ARG(A0, 0));                                                         \
        else                                                                                                   \
            m_returnValue = target(BF2_CC_ARG(A0, 0), BF2_CC_ARG(A1, 1));                                      \
    }                                                                                                          \
    virtual void setArgFromString(int index, const std::string& value)                                         \
    {                                                                                                          \
        BF2_CC_CONVERT(A0, 1)                                                                                  \
        BF2_CC_CONVERT(A1, 2)                                                                                  \
    }                                                                                                          \
    BF2_CC_RETURN_AS_STRING                                                                                    \
    BF2_CC_COMMON_HOOKS                                                                                        \
private:                                                                                                       \
    RET m_returnValue;                                                                                         \
};                                                                                                             \
static BF2_CC_NAME BF2_CC_INSTANCE;                                                                            \
}

BF2_CONSOLE_OBJECT("EditorRoad", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, editorRoadInfo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(splines, editorRoadSplines, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(roads, editorRoadRoads, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(templates, editorRoadTemplates, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, editorRoadSave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(splineCreate, editorRoadSplineCreate, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(spline, editorRoadSpline, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(road, editorRoadRoad, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(template, editorRoadTemplate, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(splineCreateAs, editorRoadSplineCreateAs, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(splineAdd, editorRoadSplineAdd, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(splineInsert, editorRoadSplineInsert, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(splineMove, editorRoadSplineMove, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(splineRemovePoint, editorRoadSplineRemovePoint, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(splineSet, editorRoadSplineSet, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_5_RET_ACTIVE(splinePoint, editorRoadSplinePoint, std::string, g_commands != 0, std::string, std::string, std::string, std::string, std::string, "", "", kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(splineDelete, editorRoadSplineDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(templateCreate, editorRoadTemplateCreate, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(templateCreateFrom, editorRoadTemplateCreateFrom, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(templateSet, editorRoadTemplateSet, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(templateDelete, editorRoadTemplateDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_5_RET_ACTIVE(create, editorRoadCreate, std::string, g_commands != 0, std::string, std::string, std::string, std::string, std::string, "", "", kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(set, editorRoadSet, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(delete, editorRoadDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(rename, editorRoadRename, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(build, editorRoadBuild, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyEdit, editorRoadApplyEdit, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(tool, editorRoadTool, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_2_RET_ACTIVE(activeSpline, editorRoadActiveSpline, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(showSplines, editorRoadShowSplines, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(apply, editorRoadApply, std::string, g_commands != 0, std::string, kEditorConsoleTag)
EDITOR_CONSOLE_METHOD_0_1_RET_ACTIVE(splineCopy, editorRoadSplineCopy, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(splinePaste, editorRoadSplinePaste, std::string, g_commands != 0, kEditorConsoleTag)
EDITOR_CONSOLE_METHOD_0_2_RET_ACTIVE(splineMark, editorRoadSplineMark, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
EDITOR_CONSOLE_METHOD_0_2_RET_ACTIVE(splineMerge, editorRoadSplineMerge, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(activeTemplate, editorRoadActiveTemplate, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_2_RET_ACTIVE(connect, editorRoadConnect, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(disconnect, editorRoadDisconnect, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(refresh, editorRoadRefresh, std::string, g_commands != 0, kEditorConsoleTag)
EDITOR_CONSOLE_METHOD_0_1_RET_ACTIVE(cleanFolder, editorRoadCleanFolder, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorEffect", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(preview, effectPreviewLine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(play, effectPlay, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(stop, effectStop, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(pause, effectPause, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(clear, effectClear, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, effectInfoLine, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorMod", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, modList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(create, modCreate, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(open, modOpen, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(description, modDescription, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setDescription, modSetDescription, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(applyDescription, modApplyDescription, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeDescription, modRemoveDescription, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorObjects", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(status, objectsStatus, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(extract, objectsExtract, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pack, objectsPack, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorLibrary", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, libraryInfoLine, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(folder, libraryFolder, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorBundle", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, bundleList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(info, bundleInfoLine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(save, bundleSave, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(saveOver, bundleSaveOver, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(place, bundlePlace, std::string, g_commands != 0, std::string, Vec3, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(placeHere, bundlePlaceHere, std::string, g_commands != 0, std::string, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(group, bundleGroup, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(delete, bundleDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorBrush", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, brushList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(info, brushInfoLine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(select, brushSelect, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(define, brushDefine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(addSelection, brushAddSelection, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addItem, brushAddItem, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(set, brushSet, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(item, brushItem, std::string, g_commands != 0, int, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(removeItem, brushRemoveItem, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(delete, brushDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(seed, brushSeed, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(tool, brushTool, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(eraseRadius, brushEraseRadius, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(stamp, brushStampAt, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(erase, brushEraseAt, std::string, g_commands != 0, Vec3, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorUndergrowth", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, undergrowthInfoLine, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(tool, undergrowthTool, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(material, undergrowthMaterial, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(radius, undergrowthRadius, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(paint, undergrowthPaint, std::string, g_commands != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(erase, undergrowthErase, std::string, g_commands != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyChange, undergrowthApply, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setting, undergrowthSetting, std::string, g_commands != 0, std::string, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(materialHeight, undergrowthMaterialHeight, std::string, g_commands != 0, int, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(type, undergrowthType, g_commands != 0, int, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyConfig, undergrowthApplyConfig, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorShortcut", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, shortcutList, std::string, true, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(set, shortcutSet, std::string, true, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(reset, shortcutReset, std::string, true, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorTerrain", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, terrainInfoLine, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(height, terrainHeightAt, std::string, g_commands != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(brush, terrainBrushSet, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(stroke, terrainStrokePrimary, std::string, g_commands != 0, float, float, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(strokeSecondary, terrainStrokeSecondary, std::string, g_commands != 0, float, float, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyChange, terrainApply, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(exportHeightmaps, terrainExport, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(tool, terrainTool, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(sectors, terrainSectorsLine, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(sector, terrainSector, std::string, g_commands != 0, int, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(applySector, terrainApplySector, std::string, g_commands != 0, int, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(importSurrounding, terrainImportSurrounding, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(smoothEdges, terrainSmoothEdges, std::string, g_commands != 0, int, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(seaLevel, terrainSeaLevel, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(applySeaLevel, terrainApplySeaLevel, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(view, terrainView, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(technique, terrainTechnique, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(merge, terrainMerge, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(clearEdits, terrainClearEdits, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(showSector, terrainShowSector, std::string, g_commands != 0, int, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(shownSectors, terrainShownSectors, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(generateOvergrowth, terrainGenerateOvergrowth, std::string, g_commands != 0,
                                kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(compile, terrainCompile, std::string, g_commands != 0, kEditorConsoleTag)

// The bindings. Each object block sets the console name the bindings after it register under
// (BF2_CONSOLE_OBJECT lowercases its first letter, and lookups ignore case). Access 2 and category 0,
// as the engine's `Object` block (Scene/Object/ObjectManagerConsole.cpp). Every binding is inactive
// while no edit session is attached, so the console refuses it rather than calling into nothing.
BF2_CONSOLE_OBJECT("EditorObject", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(activate, objectActivate, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(create, objectCreate, g_commands != 0, std::string, Vec3, Vec3, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(recreate, objectRecreate, g_commands != 0, int, std::string, Mat4, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(remove, objectRemove, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(placeAt, objectPlaceAt, std::string, g_commands != 0, std::string, float, float, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorSelection", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(clear, selectionClear, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(select, selectionSelect, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(add, selectionAdd, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(pickAt, selectionPickAt, std::string, g_commands != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(pickAdd, selectionPickAdd, std::string, g_commands != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(selectTemplate, selectionSelectTemplate, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, selectionInfo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(translate, selectionTranslate, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(rotate, selectionRotate, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setPosition, selectionSetPosition, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setRotation, selectionSetRotation, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setLayer, selectionSetLayer, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(delete, selectionDelete, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(duplicate, selectionDuplicate, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(snapToGround, selectionSnapToGround, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(reload, selectionReload, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(box, selectionBox, g_commands != 0, float, float, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(boxAdd, selectionBoxAdd, g_commands != 0, float, float, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(boxToggle, selectionBoxToggle, g_commands != 0, float, float, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(replace, selectionReplace, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(copyTransform, selectionCopyTransform, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(pasteTransform, selectionPasteTransform, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(snapToGrid, selectionSnapToGrid, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setProperty, selectionSetProperty, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(groupNew, selectionGroupNew, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(ungroup, selectionUngroup, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(selectGroup, selectionSelectGroup, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(anchorInfo, selectionAnchorInfo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(alignStamp, selectionAlignStamp, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(align, selectionAlign, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(raiseByAnchor, selectionRaiseByAnchor, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setAnchor, selectionSetAnchor, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(anchorToBox, selectionAnchorToBox, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(resetAnchor, selectionResetAnchor, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorClipboard", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(lastOffsetCopy, clipboardLastOffsetCopy, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(copy, clipboardCopy, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(cut, clipboardCut, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(paste, clipboardPaste, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pasteAt, clipboardPasteAt, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(pasteInPlace, clipboardPasteInPlace, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorLayer", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(hide, layerHide, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(show, layerShow, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(lock, layerLock, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(unlock, layerUnlock, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(select, layerSelect, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(status, layerStatus, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, layerList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(add, layerAdd, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(rename, layerRename, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(filters, layerFilters, std::string, g_commands != 0, int, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(disable, layerDisable, std::string, g_commands != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(active, layerActive, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_2_RET_ACTIVE(remove, layerRemove, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyLayers, layerApplyLayers, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorHistory", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(undo, historyUndo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(redo, historyRedo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(clear, historyClear, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorCamera", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(frameSelection, cameraFrameSelection, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mode, cameraMode, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setPosition, cameraSetPosition, std::string, g_commands != 0, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(resetPosition, cameraResetPosition, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(setSpeed, cameraSetSpeed, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(resetSpeed, cameraResetSpeed, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, cameraInfo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(addBookmark, cameraAddBookmark, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(fromSelection, cameraFromSelection, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(toggleBookmark, cameraToggleBookmark, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(bookmark, cameraGoBookmark, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(bookmarks, cameraBookmarks, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorLight", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(presets, lightPresets, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(apply, lightApply, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(save, lightSave, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(resetToLevel, lightReset, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(levels, lightLevels, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(applyLevel, lightApplyLevel, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(sunFromCamera, lightSunFromCamera, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(flareFromCamera, lightFlareFromCamera, std::string, g_commands != 0, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorMaterial", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(status, materialStatus, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, materialList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(info, materialInfo, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(cells, materialCellsOf, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(cell, materialCell, std::string, g_commands != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(set, materialSet, std::string, g_commands != 0, int, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(add, materialAdd, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(addAs, materialAddAs, std::string, g_commands != 0, std::string, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(remove, materialRemove, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(addCell, materialAddCell, std::string, g_commands != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeCell, materialRemoveCell, std::string, g_commands != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(setDamageMod, materialSetDamageMod, std::string, g_commands != 0, int, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_5(setTemplate, materialSetTemplate, g_commands != 0, int, int, std::string, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, materialSave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(extract, materialExtract, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyEdit, materialApplyEdit, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorAnimation", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(systems, animationSystems, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(open, animationOpen, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(status, animationStatus, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(tree, animationTree, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(list, animationList, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(select, animationSelect, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(selected, animationSelected, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(set, animationSet, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(unset, animationUnset, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setSystem, animationSetSystem, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(unsetSystem, animationUnsetSystem, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(addTrigger, animationAddTrigger, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addBundle, animationAddBundle, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(addValueHolder, animationAddValueHolder, std::string, g_commands != 0, std::string, std::string,
                                kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addAnimation, animationAddAnimation, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(addEvent, animationAddEvent, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(removeAnimation, animationRemoveAnimation, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(removeEvent, animationRemoveEvent, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(setStart, animationSetStart, std::string, g_commands != 0, std::string, std::string, std::string,
                                kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setLength, animationSetLength, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(link, animationLink, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(unlink, animationUnlink, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(remove, animationRemove, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(timeline, animationTimeline, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_2_RET_ACTIVE(joints, animationJoints, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(copySystems, animationCopySystems, std::string, g_commands != 0, std::string, std::string,
                                kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(point, animationPoint, std::string, g_commands != 0, std::string, std::string, std::string,
                                kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, animationSave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyEdit, animationApplyEdit, std::string, g_commands != 0, int, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorTools", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(customCommands, toolsCustomCommands, std::string, g_toolsHandler != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setCustomCommand, toolsSetCustomCommand, std::string, g_toolsHandler != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setCustomFile, toolsSetCustomFile, std::string, g_toolsHandler != 0, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(runCustomCommand, toolsRunCustomCommand, std::string, g_toolsHandler != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(runCustomFile, toolsRunCustomFile, std::string, g_toolsHandler != 0, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorGizmo", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mode, gizmoMode, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(space, gizmoSpace, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pivot, gizmoPivot, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(anchorMode, gizmoAnchorMode, std::string, g_commands != 0, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorView", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pauseRenderer, viewPauseRenderer, std::string, g_viewHandler != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(renderOneFrame, viewRenderOneFrame, std::string, g_viewHandler != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pauseUpdate, viewPauseUpdate, std::string, g_viewHandler != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(levelViewDistance, viewLevelViewDistance, std::string, g_viewHandler != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(wireframe, viewWireframe, std::string, g_viewHandler != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(overlays, viewOverlays, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(record, viewRecord, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(stopRecording, viewStopRecording, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(recording, viewRecording, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(overlayLinks, viewOverlayLinks, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(overlay, viewOverlay, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(panel, viewPanel, std::string, g_panelHandler != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(markers, viewMarkers, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(combatAreas, viewCombatAreas, std::string, g_commands != 0, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorTest", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(wait, testWait, std::string, g_testHarness != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseMove, testMouseMove, std::string, g_testHarness != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseBy, testMouseBy, std::string, g_testHarness != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mouseToObject, testMouseToObject, std::string, g_testHarness != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mouseToPart, testMouseToPart, std::string, g_testHarness != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseToHud, testMouseToHud, std::string, g_testHarness != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseToAxis, testMouseToAxis, std::string, g_testHarness != 0, int, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseButton, testMouseButton, std::string, g_testHarness != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mouseWheel, testMouseWheel, std::string, g_testHarness != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(key, testKey, std::string, g_testHarness != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(dragTemplate, testDragTemplate, std::string, g_testHarness != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseToAreaPoint, testMouseToAreaPoint, std::string, g_testHarness != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseToAreaEdge, testMouseToAreaEdge, std::string, g_testHarness != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mouseToSound, testMouseToSound, std::string, g_testHarness != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseToGround, testMouseToGround, std::string, g_testHarness != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(mouseToMinimap, testMouseToMinimap, std::string, g_testHarness != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(mouseToItem, testMouseToItem, std::string, g_testHarness != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(type, testTypeText, std::string, g_testHarness != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorPlayTest", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(soldier, playTestSoldier, std::string, g_playTest != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(leave, playTestLeave, std::string, g_playTest != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, playTestInfo, std::string, g_playTest != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(kit, playTestKit, std::string, g_playTest != 0, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(hold, playTestHold, std::string, g_playTest != 0, std::string, float, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(weapon, playTestWeapon, std::string, g_playTest != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(drive, playTestDrive, std::string, g_playTest != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(driveTemplate, playTestDriveTemplate, std::string, g_playTest != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorHud", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, hudInfo, std::string, g_hud != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(preview, hudPreview, std::string, g_hud != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(set, hudSet, std::string, g_hud != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(select, hudSelect, std::string, g_hud != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(node, hudNode, std::string, g_hud != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(rect, hudRect, std::string, g_hud != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(setRow, hudSetRow, std::string, g_hud != 0, std::string, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(addRow, hudAddRow, std::string, g_hud != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeRow, hudRemoveRow, std::string, g_hud != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(changes, hudChanges, std::string, g_hud != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(setLine, hudSetLine, std::string, g_hud != 0, int, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(insertLine, hudInsertLine, std::string, g_hud != 0, int, int, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removeLine, hudRemoveLine, std::string, g_hud != 0, int, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorGround", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(layers, groundLayersLine, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(set, groundSetLine, std::string, g_commands != 0, int, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(apply, groundApplyLine, std::string, g_commands != 0, int, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(misc, groundMiscLine, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(removeLayer, groundRemoveLayerLine, std::string, g_commands != 0, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorTemplate", 2, 0)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(set, templateSet, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(apply, templateApply, std::string, g_commands != 0, std::string, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(remove, templateRemove, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(lines, templateLines, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorPreview", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(show, previewShow, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(refresh, previewRefresh, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(reset, previewReset, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(frame, previewFrame, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(turntable, previewTurntable, std::string, g_commands != 0, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, previewInfo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(camera, previewCamera, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(clear, previewClear, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(showParts, previewShowParts, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(parts, previewParts, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pick, previewPick, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(movePart, previewMovePart, std::string, g_commands != 0, int, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(turnPart, previewTurnPart, std::string, g_commands != 0, int, float, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorSnap", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(info, snapInfo, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(ground, snapGround, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(objects, snapObjects, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(grid, snapGridMode, std::string, g_commands != 0, int, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorMeasure", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(tool, measureToolSet, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(add, measureAddPoint, std::string, g_commands != 0, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(undo, measureUndoPoint, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(finish, measureFinishLine, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorHelp", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(documents, helpDocumentsList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(open, helpOpen, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(describe, helpDescribe, std::string, g_commands != 0, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorGameplay", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(selectSpawners, gameplaySelectSpawners, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(supplyGroups, gameplaySupplyGroups, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(groupControlPoints, gameplayGroupControlPoints, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(ungroupControlPoints, gameplayUngroupControlPoints, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(connectSupply, gameplayConnectSupply, std::string, g_commands != 0, int, int, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(disconnectSupply, gameplayDisconnectSupply, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyGroup, gameplayApplyGroup, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applySupply, gameplayApplySupply, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(spawnerPresets, gameplaySpawnerPresets, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(spawnerPreset, gameplaySpawnerPreset, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(create, gameplayCreate, std::string, g_commands != 0, std::string, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(createHere, gameplayCreateHere, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(createEffectArea, gameplayCreateEffectArea, std::string, g_commands != 0, std::string, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(createEffectAreaHere, gameplayCreateEffectAreaHere, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(rename, gameplayRename, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(applyRename, gameplayApplyRename, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorCombatArea", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, combatList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(info, combatInfo, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(create, combatCreate, std::string, g_commands != 0, std::string, Vec3, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(createHere, combatCreateHere, std::string, g_commands != 0, std::string, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(copy, combatCopy, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(paste, combatPaste, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(delete, combatDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(movePoint, combatMovePoint, g_commands != 0, std::string, int, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_4(insertPoint, combatInsertPoint, g_commands != 0, std::string, int, float, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(removePoint, combatRemovePoint, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(set, combatSet, std::string, g_commands != 0, std::string, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(manager, combatManager, std::string, g_commands != 0, std::string, float, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(sync, combatSync, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(syncManager, combatSyncManager, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(select, combatSelect, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(deselect, combatDeselect, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorLevelSettings", 2, 0)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(list, settingsList, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(get, settingsGet, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(set, settingsSet, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(teams, levelSettingsTeams, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(team, levelSettingsTeam, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(apply, settingsApply, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(unset, settingsUnset, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(spawnCameraFromView, settingsSpawnCameraFromView, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorSound", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(list, soundList, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(info, soundInfoLine, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(move, soundMove, std::string, g_commands != 0, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(create, soundCreate, std::string, g_commands != 0, std::string, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(createHere, soundCreateHere, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(delete, soundDelete, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(apply, soundApply, std::string, g_commands != 0, std::string, Vec3, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(remove, soundRemove, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(restore, soundRestore, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(select, soundSelect, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(deselect, soundDeselect, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorGameplayLayers", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(overview, gameplayLayersOverview, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(compare, gameplayLayersCompare, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(compareNone, gameplayLayersCompareNone, std::string, g_commands != 0, kEditorConsoleTag)

BF2_CONSOLE_OBJECT("EditorLevel", 2, 0)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(status, levelStatus, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(save, levelSave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(resave, levelResave, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(preview, levelPreview, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(previewAll, levelPreviewAll, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(previewDiff, levelPreviewDiff, std::string, g_commands != 0, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(writePreview, levelWritePreview, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(dropPreview, levelDropPreview, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(backups, levelBackups, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(restore, levelRestore, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(reloadObjects, levelReloadObjects, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(repositionObjects, levelRepositionObjects, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(description, levelDescription, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(setDescription, levelSetDescription, std::string, g_commands != 0, std::string, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(snapAllToGround, levelSnapAll, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(saveLists, levelSaveLists, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(levels, levelLevels, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(open, levelOpen, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_1_RET_ACTIVE(pack, levelPack, std::string, g_commands != 0, std::string, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(saveAs, levelSaveAs, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_3_RET_ACTIVE(create, levelCreate, std::string, g_commands != 0, std::string, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_0_RET_ACTIVE(layers, levelLayers, std::string, g_commands != 0, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(switchLayer, levelSwitchLayer, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)
BF2_CONSOLE_METHOD_2_RET_ACTIVE(duplicateLayer, levelDuplicateLayer, std::string, g_commands != 0, std::string, int, kEditorConsoleTag)

namespace
{

bool isEditorBinding(const dice::hfe::ConsoleObject* object)
{
    const dice::hfe::ConsoleObjectList& ours = dice::hfe::consoleBindingList(kEditorConsoleTag);
    return std::find(ours.begin(), ours.end(), object) != ours.end();
}

// "Object.method args..." -> is it an editor command, or a set/get binding asked for its value?
// Either way the line cannot have changed the level behind the history's back.
bool effectIsKnown(const std::string& line)
{
    std::istringstream words(line);
    std::string command;
    words >> command;
    // A comment does nothing (a script's `rem` lines must not clear the history).
    if (_stricmp(command.c_str(), "rem") == 0)
        return true;
    const size_t dot = command.find('.');
    if (dot == std::string::npos || !dice::hfe::mainConsoleObjects)
        return false;
    dice::hfe::ConsoleObject* object =
        dice::hfe::mainConsoleObjects->getConsoleObject(command.substr(0, dot), command.substr(dot + 1));
    if (!object)
        return false;
    if (isEditorBinding(object))
        return true;
    std::string argument;
    return object->isSetGet() && !(words >> argument);
}

} // namespace

bool registerEditorConsole(IEditorCommands* commands)
{
    g_commands = commands;
    if (!g_registered && dice::hfe::mainConsoleObjects)
        g_registered = dice::hfe::mainConsoleObjects->registerConsoleObjects(
            dice::hfe::consoleBindingList(kEditorConsoleTag));
    return g_registered;
}

void setTestHarness(ITestHarness* harness)
{
    g_testHarness = harness;
}

void setPlayTest(IPlayTestCommands* playTest)
{
    g_playTest = playTest;
}

void setHudCommands(IHudCommands* hud)
{
    g_hud = hud;
}

bool playTestActive()
{
    return g_playTest && g_playTest->playTestActive();
}

void setViewHandler(ViewHandler handler, void* context)
{
    g_viewHandler = handler;
    g_viewContext = context;
}

void setToolsHandler(ToolsHandler handler, void* context)
{
    g_toolsHandler = handler;
    g_toolsContext = context;
}

void setPanelHandler(PanelHandler handler, void* context)
{
    g_panelHandler = handler;
    g_panelContext = context;
}

void unregisterEditorConsole()
{
    if (g_registered && dice::hfe::mainConsoleObjects)
        dice::hfe::mainConsoleObjects->unregisterConsoleObjects(dice::hfe::consoleBindingList(kEditorConsoleTag));
    g_registered = false;
    g_commands = 0;
}

bool runConsoleScript(const std::string& script, std::string& error)
{
    if (!dice::hfe::io::mainConsole)
    {
        error = "the engine console is not up";
        return false;
    }
    std::istringstream lines(script);
    std::string line;
    while (std::getline(lines, line))
    {
        line = trimmed(line);
        if (line.empty())
            continue;
        g_lastError.clear();
        std::string answer;
        // Full access, and nothing echoed into the engine console's own buffer: the history's lines
        // are the editor's business (OldConsole::executeLine, IO/Console/Console.cpp).
        dice::hfe::io::mainConsole->executeLine(line + "\n", answer, false, false, false, false, true);
        if (!g_lastError.empty())
        {
            error = "'" + line + "': " + g_lastError;
            return false;
        }
        const std::string text = trimmed(answer);
        if (!text.empty())
        {
            error = "'" + line + "': " + text;
            return false;
        }
    }
    return true;
}

bool consoleScriptRunner(void*, const std::string& script, std::string& error)
{
    return runConsoleScript(script, error);
}

namespace
{

// The engine console's own refusals (IO/Console/Console.cpp): an unknown command, a wrong number of
// arguments, an expression or name it cannot read. They come back as the answer's text with nothing in
// g_lastError, so a script line that hit one would otherwise count as run.
bool engineRefusal(const std::string& answer)
{
    static const char* const kPrefixes[] = { "Unknown object or method", "Too few arguments", "Too many arguments",
                                             "Error: ", "Unknown variable ", "Unknown constant " };
    for (size_t i = 0; i < sizeof(kPrefixes) / sizeof(kPrefixes[0]); ++i)
        if (answer.compare(0, std::strlen(kPrefixes[i]), kPrefixes[i]) == 0)
            return true;
    return false;
}

} // namespace

ConsoleLineResult runConsoleLine(const std::string& line)
{
    ConsoleLineResult result;
    const std::string text = trimmed(line);
    if (text.empty())
        return result;
    if (!dice::hfe::io::mainConsole)
    {
        result.output = "the engine console is not up";
        result.failed = true;
        return result;
    }
    result.unknownEffect = !effectIsKnown(text);
    g_lastError.clear();
    std::string answer;
    dice::hfe::io::mainConsole->executeLine(text + "\n", answer, false, false, false, false, true);
    result.output = trimmed(answer);
    result.failed = !g_lastError.empty() || engineRefusal(result.output);
    // A binding that returns nothing (the four-argument shape) reports only through g_lastError.
    if (result.failed && result.output.empty())
        result.output = g_lastError;
    return result;
}

} // namespace bf2editor
